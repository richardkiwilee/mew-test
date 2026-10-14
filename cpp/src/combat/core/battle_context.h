#pragma once
// ============================================================================
// BattleContext — 一次战斗的权威状态与服务入口（设计文档 §3.3）
// 拥有：地图、单位、目录、RNG、日志、调度器、属性计算与各结算器。
// 禁止任何模块绕过本入口直接改其他模块内部字段；
// 所有状态修改必须经这里或其委托的统一系统（Damage/Death/Action/Status）。
// ============================================================================

#include "combat/ai/enemy_ai.h"
#include "combat/definitions/definition_catalog.h"
#include "combat/domain/passive_runtime.h"
#include "combat/domain/unit_def.h"
#include "combat/domain/unit_runtime.h"
#include "combat/effects/event_queue.h"
#include "combat/infrastructure/combat_log.h"
#include "combat/infrastructure/rng_service.h"
#include "combat/map/grid_map.h"
#include "combat/resolution/action_resolver.h"
#include "combat/resolution/damage_resolver.h"
#include "combat/resolution/death_resolver.h"
#include "combat/scheduling/turn_scheduler.h"
#include "combat/stats/stat_calculator.h"
#include "combat/statuses/status_system.h"

namespace mew::combat {

class BattleContext {
public:
    explicit BattleContext(uint64_t seed);

    // ---------------- 装配 ----------------
    GridMap& grid() { return grid_; }

    // 设置棋盘（tiles 长度 = w*h，行优先）
    void set_grid(int32_t w, int32_t h, const std::vector<TerrainType>& tiles);

    // 生成单位（要求格子合法且为空）；失败返回 kInvalidEntity
    EntityId spawn_unit(const UnitDef& def, Team team, Vec2i pos);

    // 战斗开始：发布 BattleStarted、生成 tie-break、建立第 1 轮队列并开启首个回合
    EntityId start_battle();

    // ---------------- 状态访问 ----------------
    UnitRuntime* unit(EntityId id) {
        return id == kInvalidEntity || id > units_.size() ? nullptr : &units_[id - 1];
    }
    const UnitRuntime* unit(EntityId id) const {
        return id == kInvalidEntity || id > units_.size() ? nullptr : &units_[id - 1];
    }
    DefinitionCatalog& catalog() { return catalog_; }
    const DefinitionCatalog& catalog() const { return catalog_; }
    StatusSystem& status_system() { return status_system_; }
    const StatusSystem& status_system() const { return status_system_; }
    const std::vector<UnitRuntime>& units() const { return units_; }
    const UnitDef& def_of(EntityId id) const {
        return catalog_.require_unit_def(unit(id)->def_id);
    }
    RngService& rng() { return rng_; }
    CombatLog& log() { return log_; }
    StatCalculator& stats() { return stats_; }
    TurnScheduler& scheduler() { return scheduler_; }
    EventQueue& events() { return events_; }
    EffectRegistry& effects() { return effect_registry_; }

    EntityId current_unit() const { return current_unit_; }
    bool battle_started() const { return battle_started_; }
    bool battle_over() const { return battle_over_; }
    Team winner() const { return winner_; }

    bool is_ally(EntityId a, EntityId b) const {
        return unit(a)->team == unit(b)->team;
    }

    // ---------------- 回合流程 ----------------
    // 开启下一个单位回合（含 TurnStarted 与动作资源刷新）；无更多单位返回 kInvalidEntity
    EntityId begin_next_turn();

    // 结束当前单位回合（TurnEnding → 回蓝/回血 → TurnEnded）
    void end_current_turn();

    // ---------------- 行动（委托 ActionResolver） ----------------
    MoveResult try_move(EntityId unit_id, const std::vector<Vec2i>& path);
    AttackResult try_attack(EntityId attacker_id, EntityId target_id);
    CastResult try_cast_ability(EntityId caster_id, uint32_t ability_id, EntityId target_id,
                                Vec2i target_tile);

    // ---------------- 敌人 AI（意图经 ActionResolver 验证执行，§18 阶段 5） ----------------
    bool run_ai_turn();   // 当前单位是敌方时执行其 AI 行动并结束回合；否则无事发生

    // ---------------- 地形交互（设计文档 §11.2：进入格子 → 地形规则） ----------------
    void apply_terrain_on_enter(EntityId unit_id, Vec2i pos);
    int32_t terrain_regen_bonus(EntityId unit_id) const;  // 花丛等驻留回复加成

    // ---------------- 伤害与死亡（统一入口） ----------------
    DamageResult deal_damage(DamageContext dmg);
    void process_deaths(EntityId last_damage_source);
    void check_battle_end();

    // ---------------- 统一事件发布（日志 + 反应队列，设计文档 §5.1） ----------------
    void publish_event(CombatEvent event, std::string_view category,
                       std::vector<LogField> extra_fields = {}, std::string detail = {});

    // ---------------- 统一资源/治疗 API（所有增减走这里） ----------------
    int32_t gain_resource(EntityId unit_id, ResourceType res, int32_t amount);
    bool spend_resource(EntityId unit_id, ResourceType res, int32_t amount, std::string_view reason);
    int32_t heal(EntityId unit_id, int32_t amount, std::string_view reason);

    // ---------------- 复活（统一 API，设计文档 §12.2） ----------------
    // 保留原实体 ID；状态已在倒地时清除；本轮不再行动（下轮入队）；hp 由调用方给定
    bool revive_unit(EntityId unit_id, int32_t hp_value, std::string_view reason);

    // ---------------- 被动实例 ----------------
    void add_passive(EntityId owner, uint32_t passive_def_id);
    std::vector<PassiveRuntime>& passive_runtimes() { return passive_runtimes_; }
    const std::vector<PassiveRuntime>& passive_runtimes() const { return passive_runtimes_; }

    // 最后有效伤害来源（击杀归属/反伤判定用；由 DamageResolver 维护）
    EntityId last_effective_damage_source() const { return last_effective_damage_source_; }
    void set_last_effective_damage_source(EntityId src) { last_effective_damage_source_ = src; }

    // 事件序号（EventQueue 用）
    SequenceId next_event_seq() { return ++event_seq_; }
    EventId next_event_id() { return ++event_id_seq_; }

    // 属性初始化烘焙标记（DD-018）：标记之后创建的被动/状态实例动态叠加，
    // 之前创建的已在 initialize_stats 时烘焙进 effective_base
    InstanceId stat_init_passive_marker() const { return stat_init_passive_marker_; }
    InstanceId stat_init_status_marker() const { return stat_init_status_marker_; }
    void set_stat_init_markers(InstanceId passive_marker, InstanceId status_marker) {
        stat_init_passive_marker_ = passive_marker;
        stat_init_status_marker_ = status_marker;
    }

    // ---------------- 查询辅助 ----------------
    // 单位移动预算（半步单位，DD-002）
    int32_t move_budget_of(EntityId id) const {
        return stats_.move_budget(*this, id);
    }
    // 预算内可达格（Dijkstra，含占用排除；ignore_occupants 用于预览）
    std::vector<Vec2i> reachable_tiles(EntityId id, bool ignore_occupants) const;
    // 到目标的预算内最短路径
    std::vector<Vec2i> path_to(EntityId id, Vec2i target) const;

private:
    friend class TurnScheduler;
    friend class ActionResolver;
    friend class DamageResolver;
    friend class DeathResolver;
    friend class StatCalculator;
    friend class StatusSystem;
    friend class TriggerDispatcher;
    friend class EventQueue;

    void refresh_action_points(UnitRuntime& u);   // DD-012
    void regen_at_turn_end(UnitRuntime& u);       // DD-011
    void apply_victory_cleanup();                 // 复活倒地猫 + 战后回血（DD-007）

    GridMap grid_;
    DefinitionCatalog catalog_;
    std::vector<UnitRuntime> units_;   // entity_id = index+1，稳定不复用
    RngService rng_;
    CombatLog log_;
    StatCalculator stats_;
    TurnScheduler scheduler_;
    DamageResolver damage_resolver_;
    DeathResolver death_resolver_;
    ActionResolver action_resolver_;
    StatusSystem status_system_;
    EffectRegistry effect_registry_;
    EventQueue events_;
    EnemyAi enemy_ai_;
    std::vector<PassiveRuntime> passive_runtimes_;
    InstanceId passive_seq_ = 0;
    EntityId last_effective_damage_source_ = kInvalidEntity;
    SequenceId event_seq_ = 0;
    EventId event_id_seq_ = 0;
    InstanceId stat_init_passive_marker_ = 0;
    InstanceId stat_init_status_marker_ = 0;
    EntityId current_unit_ = kInvalidEntity;
    bool battle_started_ = false;
    bool battle_over_ = false;
    Team winner_ = Team::NEUTRAL;
};

}  // namespace mew::combat
