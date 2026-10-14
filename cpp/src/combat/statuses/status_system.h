#pragma once
// ============================================================================
// StatusSystem — 状态施加/叠层/查询（设计文档 §10）
// Phase 3：apply/remove/has/stacks + 全部叠层策略；倒地清除。
// Phase 4：tick 推进、过期、免疫/驱散标签、状态交互规则。
// ============================================================================

#include <vector>

#include "combat/core/basic_types.h"
#include "combat/domain/status_def.h"
#include "combat/effects/effect_registry.h"

namespace mew::combat {

class BattleContext;

class StatusSystem {
public:
    explicit StatusSystem(BattleContext& ctx);

    // 统一施加 API（§10.5）：校验 → 叠层策略 → StatusApplied / StatusStackChanged 日志
    // 返回 false = 目标非法或施加被忽略（原因已记日志）
    bool apply(EntityId target, uint32_t status_def_id, int32_t stacks, EntityId source,
               std::string_view reason);

    // 按状态定义 ID 移除（驱散）；removed_count = 移除的实例数
    int32_t remove(EntityId target, uint32_t status_def_id, std::string_view reason);
    bool remove_instance(EntityId target, InstanceId instance_id, std::string_view reason);

    bool has(EntityId target, uint32_t status_def_id) const;
    int32_t stacks_of(EntityId target, uint32_t status_def_id) const;
    const std::vector<StatusInstance>& instances_of(EntityId target) const;

    // 倒地清除可移除状态（DD-007：倒地清除绝大多数状态）
    void clear_on_down(EntityId target);

    // 全部实例（遍历用，属性修正汇总）
    const std::vector<StatusInstance>& all() const { return instances_; }

    // 实例序号（BattleContext 烘焙标记用，DD-018）
    InstanceId instance_seq() const { return instance_seq_; }

    // ---- Phase 4：tick 推进（时机由回合流水线调用，DD-011/DD-012） ----
    void tick_start_of_turn(EntityId unit_id);   // Burn 等：先结算 tick 再衰减（§6.4）
    void tick_end_of_turn(EntityId unit_id);     // 时长推进 + 过期（§6.5）
    void tick_round_end();                       // Bleed 等（ROUND_END 时机）

    // 持有者是否被阻断行动（Stun 类）
    bool blocked_by_status(EntityId unit_id) const;

private:
    StatusInstance* find_instance(EntityId target, uint32_t status_def_id);
    const StatusInstance* find_instance(EntityId target, uint32_t status_def_id) const;

    // 按实例 ID 执行 tick：效果 → 层数变化 → 时长推进/过期。
    // tick 效果可能杀死持有者/清除状态，因此按 ID 查找并重新定位（不持引用跨效果执行）。
    void run_tick(InstanceId instance_id, TickTiming timing);

    BattleContext& ctx_;
    std::vector<StatusInstance> instances_;  // 下标稳定；目标死亡后保留至战斗结束（日志引用）
    InstanceId instance_seq_ = 0;
};

}  // namespace mew::combat
