#include "pch.h"
#include "combat/stats/stat_calculator.h"

#include "combat/core/battle_context.h"

namespace mew::combat {

namespace {

// 一个待应用的修正实例（含稳定排序键）
struct ModEntry {
    const StatModifierDef* mod = nullptr;
    uint32_t source_id = 0;
    uint32_t order = 0;   // 同源内的定义顺序
    uint32_t instance = 0; // 被动/状态实例 id（最终 tie-break）
};

// 按 phase 分组收集；返回 phase 升序排列（枚举序即设计文档 §9.2 的顺序）
constexpr int32_t phase_index(StatPhase p) { return static_cast<int32_t>(p) - 1; }

void apply_one_mod(int64_t& value, const StatModifierDef& mod) {
    switch (mod.operation) {
        case StatModifierOp::ADD:
            value += mod.value;
            break;
        case StatModifierOp::MUL:
            value = apply_bp(value, kPercentBase + mod.value);
            break;
        case StatModifierOp::SET:
            value = mod.value;
            break;
    }
}

}  // namespace

// ---------------------------------------------------------------------------
// 动态修正：初始化后才施加的被动实例与状态实例（声明式、可重复求值、幂等）
// ---------------------------------------------------------------------------
void StatCalculator::apply_dynamic_modifiers(const BattleContext& ctx, EntityId id,
                                             std::array<int32_t, kStatCount>& values) const {
    // 收集（确定性顺序；状态修正按层数缩放后以副本入桶）
    struct Collected {
        StatModifierDef mod;   // 副本（值已按层数缩放）
        uint32_t instance;
    };
    // 按 phase 建桶；桶内按 (source_id, instance) 排序
    std::array<std::vector<Collected>, 4> buckets;

    for (const auto& rt : ctx.passive_runtimes()) {
        if (rt.owner_entity != id || !rt.enabled) continue;
        if (rt.instance_id <= ctx.stat_init_passive_marker()) continue;  // 初始化前已烘焙
        const PassiveDef* def = ctx.catalog().passive_def(rt.passive_def_id);
        if (!def) continue;
        for (const auto& mod : def->stat_modifiers) {
            buckets[phase_index(mod.phase)].push_back({mod, rt.instance_id});
        }
    }
    for (const auto& inst : ctx.status_system().all()) {
        if (inst.target_entity != id) continue;
        if (inst.instance_id <= ctx.stat_init_status_marker()) continue;
        const StatusDef* def = ctx.catalog().status_def(inst.status_def_id);
        if (!def) continue;
        for (const auto& mod : def->stat_modifiers) {
            StatModifierDef scaled = mod;
            if (scaled.operation == StatModifierOp::ADD) scaled.value *= inst.stacks;  // 每层生效
            buckets[phase_index(mod.phase)].push_back({scaled, inst.instance_id});
        }
    }

    for (auto& bucket : buckets) {
        std::stable_sort(bucket.begin(), bucket.end(),
                         [](const Collected& a, const Collected& b) {
                             if (a.mod.source_id != b.mod.source_id)
                                 return a.mod.source_id < b.mod.source_id;
                             return a.instance < b.instance;
                         });
        // 按属性汇总并应用
        for (uint32_t s = 1; s <= kStatCount; ++s) {
            const StatType stat = static_cast<StatType>(s);
            int64_t value = values[s - 1];
            // OVERRIDE：priority 大者生效（平手取排序在前者）
            const StatModifierDef* override_mod = nullptr;
            // 普通修正（ADD/MUL/加减）按顺序应用
            bool has_override = false;
            std::vector<const StatModifierDef*> ordered;
            for (const auto& c : bucket) {
                if (c.mod.stat != stat) continue;
                if (c.mod.stacking_mode == StackingMode::OVERRIDE) {
                    has_override = true;
                    if (!override_mod || c.mod.priority > override_mod->priority) {
                        override_mod = &c.mod;
                    }
                } else {
                    ordered.push_back(&c.mod);
                }
            }
            if (has_override && override_mod) {
                value = override_mod->value;
            }
            // 非 OVERRIDE 修正：先应用 SET/ADD/MUL/UNIQUE 等
            const StatModifierDef* set_mod = nullptr;
            int64_t add_sum = 0;
            std::vector<int64_t> mul_factors;
            std::optional<int32_t> min_val, max_val;
            std::vector<int32_t> diminishing;
            std::set<uint32_t> seen_sources;
            for (const StatModifierDef* mod : ordered) {
                switch (mod->stacking_mode) {
                    case StackingMode::OVERRIDE:
                        break;  // 已处理
                    case StackingMode::ADDITIVE:
                        if (mod->operation == StatModifierOp::ADD) add_sum += mod->value;
                        else if (mod->operation == StatModifierOp::MUL) mul_factors.push_back(kPercentBase + mod->value);
                        else set_mod = mod;
                        break;
                    case StackingMode::MULTIPLICATIVE:
                        mul_factors.push_back(kPercentBase + mod->value);
                        break;
                    case StackingMode::MINIMUM:
                        min_val = min_val ? std::min(*min_val, mod->value) : mod->value;
                        break;
                    case StackingMode::MAXIMUM:
                        max_val = max_val ? std::max(*max_val, mod->value) : mod->value;
                        break;
                    case StackingMode::UNIQUE_BY_SOURCE:
                        if (seen_sources.insert(mod->source_id).second) {
                            apply_one_mod(value, *mod);
                        }
                        break;
                    case StackingMode::DIMINISHING_RETURNS:
                        diminishing.push_back(mod->value);
                        break;
                }
            }
            // 应用顺序：SET 覆盖 → ADD 求和 → MUL 连乘 → MIN/MAX → 递减
            if (set_mod) value = set_mod->value;
            value += add_sum;
            for (int64_t factor : mul_factors) value = apply_bp(value, factor);
            if (min_val) value = std::min<int64_t>(value, *min_val);
            if (max_val) value = std::max<int64_t>(value, *max_val);
            if (!diminishing.empty()) {
                std::sort(diminishing.begin(), diminishing.end(),
                          [](int32_t a, int32_t b) { return std::abs(a) > std::abs(b); });
                int64_t divisor = 1;
                for (int32_t v : diminishing) {
                    value += v / divisor;
                    divisor *= 2;
                }
            }
            values[s - 1] = clamp_to(value, std::numeric_limits<int32_t>::min() / 4,
                                     std::numeric_limits<int32_t>::max() / 4);
        }
    }
}

// ---------------------------------------------------------------------------
// 战斗初始化：base → 烘焙当前修正 →（可选）一次性变换 → effective_base
// 总是从 base 重算（幂等），绝不把修正重复叠加（设计文档 §9.2/§9.4）。
// 一次性变换只在战斗开始时执行（run_transforms=true），普通重算绝不执行（§0 规则 8）。
// ---------------------------------------------------------------------------
void StatCalculator::initialize_stats(BattleContext& ctx, EntityId id, bool run_transforms) {
    UnitRuntime* u = ctx.unit(id);
    if (!u) return;
    const UnitDef& def = ctx.def_of(id);

    // 快照 = 基础值
    std::array<int32_t, kStatCount> snapshot = def.base_stats;

    // 烘焙当前修正：被动（常驻）与此刻已存在的状态（参与初始化变换，DD-018）
    const auto bake_mods = [&](const std::vector<StatModifierDef>& mods, int32_t stacks = 1) {
        for (const auto& mod : mods) {
            const size_t idx = static_cast<uint32_t>(mod.stat) - 1;
            int64_t v = snapshot[idx];
            StatModifierDef scaled = mod;
            if (scaled.operation == StatModifierOp::ADD) scaled.value *= stacks;
            apply_one_mod(v, scaled);
            snapshot[idx] = clamp_to(v, std::numeric_limits<int32_t>::min() / 4,
                                     std::numeric_limits<int32_t>::max() / 4);
        }
    };
    for (const auto& rt : ctx.passive_runtimes()) {
        if (rt.owner_entity != id || !rt.enabled) continue;
        const PassiveDef* pdef = ctx.catalog().passive_def(rt.passive_def_id);
        if (pdef) bake_mods(pdef->stat_modifiers);
    }
    for (const auto& inst : ctx.status_system().all()) {
        if (inst.target_entity != id) continue;
        const StatusDef* sdef = ctx.catalog().status_def(inst.status_def_id);
        if (sdef) bake_mods(sdef->stat_modifiers, inst.stacks);
    }

    // 一次性变换（仅战斗初始化阶段）
    if (run_transforms) {
        for (const auto& rt : ctx.passive_runtimes()) {
            if (rt.owner_entity != id || !rt.enabled) continue;
            const PassiveDef* pdef = ctx.catalog().passive_def(rt.passive_def_id);
            if (!pdef) continue;
            for (const auto& eff : pdef->effects) {
                if (eff.type == EffectType::SWAP_EXTREME_STAT_VALUES
                    && eff.trigger == EventType::BattleStarted) {
                    swap_extreme_stats(snapshot);
                    ctx.log().append("Stats", "TransformApplied",
                                     {f_int("entity", id),
                                      f_str("transform", "SWAP_EXTREME_STAT_VALUES")},
                                     std::format("entity={} swap_extreme_stats applied (once)", id));
                }
            }
        }
    }

    u->effective_base = snapshot;
    u->stats_initialized = true;
    // 日志：初始化后的有效基底（验收"每个阶段的中间属性快照"，设计文档 §19）
    {
        std::string detail;
        for (uint32_t s = 0; s < kStatCount; ++s) {
            if (!detail.empty()) detail += " ";
            detail += std::format("{}={}", to_string(static_cast<StatType>(s + 1)), snapshot[s]);
        }
        ctx.log().append("Stats", "StatsInitialized", {f_int("entity", id)},
                         std::format("entity={} {}", id, detail));
    }
}

void StatCalculator::swap_extreme_stats(std::array<int32_t, kStatCount>& values) const {
    // 最大值/最小值槽位（tie-break：固定枚举顺序，取枚举序更小者 —— 设计文档 §9.1/§9.2）
    int32_t max_val = std::numeric_limits<int32_t>::min();
    int32_t min_val = std::numeric_limits<int32_t>::max();
    size_t max_idx = 0, min_idx = 0;
    for (size_t i = 0; i < kStatCount; ++i) {
        if (values[i] > max_val) {
            max_val = values[i];
            max_idx = i;
        }
        if (values[i] < min_val) {
            min_val = values[i];
            min_idx = i;
        }
    }
    std::swap(values[max_idx], values[min_idx]);
}

void StatCalculator::notify_stats_changed(BattleContext& ctx) const {
    ctx.scheduler().rebuild_pending_order();
}

// ---------------------------------------------------------------------------
// 有效值 = effective_base + 动态修正
// ---------------------------------------------------------------------------
int32_t StatCalculator::effective_stat(const BattleContext& ctx, EntityId id, StatType s) const {
    const UnitRuntime* u = ctx.unit(id);
    if (!u) return 0;
    std::array<int32_t, kStatCount> values = u->effective_base;
    apply_dynamic_modifiers(ctx, id, values);
    return values[static_cast<uint32_t>(s) - 1];
}

int32_t StatCalculator::derived(const BattleContext& ctx, EntityId id, DerivedStatType d) const {
    switch (d) {
        case DerivedStatType::MAX_HP:       return max_hp(ctx, id);
        case DerivedStatType::MAX_MANA:     return max_mana(ctx, id);
        case DerivedStatType::INITIATIVE:   return initiative(ctx, id);
        case DerivedStatType::MOVE_RANGE:   return move_range(ctx, id);
        case DerivedStatType::CRIT_CHANCE:  return crit_chance_bp(ctx, id);
        case DerivedStatType::MANA_REGEN:   return mana_regen(ctx, id);
        case DerivedStatType::HEALTH_REGEN: return health_regen(ctx, id);
        case DerivedStatType::DODGE_CHANCE: return 0;   // Phase 6（地形/状态闪避接入）
        case DerivedStatType::BACKSTAB_MULT: return 12500;
        default:                             return 0;
    }
}

int32_t StatCalculator::max_hp(const BattleContext& ctx, EntityId id) const {
    return std::max(1, effective_stat(ctx, id, StatType::CON) * 4);
}

int32_t StatCalculator::max_mana(const BattleContext& ctx, EntityId id) const {
    return std::max(0, effective_stat(ctx, id, StatType::CHA) * 3);
}

int32_t StatCalculator::initiative(const BattleContext& ctx, EntityId id) const {
    return 2 * effective_stat(ctx, id, StatType::SPD);
}

int32_t StatCalculator::move_range(const BattleContext& ctx, EntityId id) const {
    const int32_t spd = effective_stat(ctx, id, StatType::SPD);
    return std::max(1, ctx.def_of(id).base_move + static_cast<int32_t>(div_trunc(spd - 5, 2)));
}

int32_t StatCalculator::move_budget(const BattleContext& ctx, EntityId id) const {
    return move_range(ctx, id) * 2;
}

int32_t StatCalculator::crit_chance_bp(const BattleContext& ctx, EntityId id) const {
    const UnitDef& def = ctx.def_of(id);
    if (!def.can_crit) return 0;  // DD：敌人不能随机暴击
    return std::max(0, 2 * effective_stat(ctx, id, StatType::LCK) * 100);
}

int32_t StatCalculator::mana_regen(const BattleContext& ctx, EntityId id) const {
    return std::max(0, effective_stat(ctx, id, StatType::INT));
}

int32_t StatCalculator::health_regen(const BattleContext& ctx, EntityId id) const {
    // Phase 4：状态（Regen 类）与地形加成在 StatusSystem tick/地形规则中实现；
    // 此处返回有效 INT 无关的固定 0，Regen 状态用 HealthRegen 效果实现
    (void)ctx;
    (void)id;
    return 0;
}

int32_t StatCalculator::melee_attack_damage(const BattleContext& ctx, EntityId id) const {
    return std::max(1, effective_stat(ctx, id, StatType::STR) - 5);
}

int32_t StatCalculator::melee_skill_damage(const BattleContext& ctx, EntityId id) const {
    return std::max(1, static_cast<int32_t>(div_floor(effective_stat(ctx, id, StatType::STR) - 5, 2)));
}

int32_t StatCalculator::post_battle_regen(const BattleContext& ctx, EntityId id) const {
    const int32_t con = effective_stat(ctx, id, StatType::CON);
    if (con < 5) return con;
    return 5 + static_cast<int32_t>(div_trunc(con - 5, 2));
}

}  // namespace mew::combat
