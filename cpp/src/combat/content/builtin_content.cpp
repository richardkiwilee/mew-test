#include "pch.h"
#include "combat/content/builtin_content.h"

#include "combat/definitions/definition_catalog.h"

namespace mew::combat {

namespace {

UnitDef make_def(uint32_t id, const char* name, const std::array<int32_t, kStatCount>& stats,
                 Team team, int32_t base_move, bool required_enemy, int32_t corpse_health,
                 bool can_crit) {
    UnitDef def;
    def.id = id;
    def.display_name = name;
    def.base_stats = stats;
    def.default_team = team;
    def.base_move = base_move;
    def.is_required_enemy = required_enemy;
    def.corpse_health = corpse_health;
    def.can_crit = can_crit;
    return def;
}

}  // namespace

UnitDef make_cat_def(uint32_t id, const std::array<int32_t, kStatCount>& stats,
                     const std::string& name, const std::vector<uint32_t>& passive_ids_list) {
    UnitDef def = make_def(id, name.c_str(), stats, Team::PLAYER,
                           /*base_move=*/4, /*required=*/false,
                           /*corpse_health=*/3, /*can_crit=*/true);
    def.passive_ids = passive_ids_list;
    return def;
}

void register_builtin_content(DefinitionCatalog& catalog) {
    // ------------------------------------------------------------------ 状态
    {
        StatusDef def;
        def.id = status_ids::kAllStatsUp;
        def.display_name = "全属性提升";
        def.description = "全属性 +1（每层）";
        def.stack_policy = StackPolicy::STACK_AND_REFRESH;
        def.max_stacks = 5;
        def.duration_unit = DurationUnit::ACTOR_TURNS;
        def.duration_value = 1;
        def.tick_timing = TickTiming::EVENT_DRIVEN;
        for (uint32_t i = 1; i <= kStatCount; ++i) {
            StatModifierDef mod;
            mod.stat = static_cast<StatType>(i);
            mod.operation = StatModifierOp::ADD;
            mod.value = 1;
            mod.stacking_mode = StackingMode::ADDITIVE;
            mod.phase = StatPhase::PRE_TRANSFORM_TEMPORARY;
            mod.source_id = def.id;
            def.stat_modifiers.push_back(mod);
        }
        catalog.register_status_def(std::move(def));
    }
    {
        StatusDef def;
        def.id = status_ids::kStrengthUp;
        def.display_name = "力量提升";
        def.description = "STR +2（每层）";
        def.stack_policy = StackPolicy::STACK_AND_REFRESH;
        def.max_stacks = 3;
        def.duration_unit = DurationUnit::ACTOR_TURNS;
        def.duration_value = 1;
        def.tick_timing = TickTiming::EVENT_DRIVEN;
        StatModifierDef mod;
        mod.stat = StatType::STR;
        mod.operation = StatModifierOp::ADD;
        mod.value = 2;
        mod.stacking_mode = StackingMode::ADDITIVE;
        mod.phase = StatPhase::PRE_TRANSFORM_TEMPORARY;
        mod.source_id = def.id;
        def.stat_modifiers.push_back(mod);
        catalog.register_status_def(std::move(def));
    }

    // ------------------------------------------------------------------ DoT / 控制状态（wiki 时机对齐）
    {
        // Burn：回合开始每层 1 伤害（无视盾），然后 −1 层
        StatusDef def;
        def.id = status_ids::kBurn;
        def.display_name = "燃烧";
        def.description = "回合开始：每层 1 点伤害，然后层数 −1";
        def.tags.push_back("fire");   // 元素标签：施加时移除湿润
        def.stack_policy = StackPolicy::STACK_AND_REFRESH;
        def.max_stacks = 9;
        def.duration_unit = DurationUnit::PERMANENT;
        def.tick_timing = TickTiming::START_OF_TURN;
        def.tick_before_decay = true;
        def.decay_stacks_on_tick = true;
        def.expire_when_stacks_zero = true;
        EffectDef tick;
        tick.id = 1;
        tick.type = EffectType::DEAL_DAMAGE;
        tick.trigger = EventType::StatusTicked;
        tick.phase = EventPhase::AFTER;
        tick.target = TargetSelectorType::SELF;
        tick.amount = 1;
        tick.amount_scales_with_stacks = true;
        tick.damage_type = DamageType::TRUE;
        def.effects.push_back(std::move(tick));
        catalog.register_status_def(std::move(def));
    }
    {
        // Poison：回合开始每层 1 伤害，静态不衰减
        StatusDef def;
        def.id = status_ids::kPoison;
        def.display_name = "中毒";
        def.description = "回合开始：每层 1 点伤害（不衰减）";
        def.stack_policy = StackPolicy::STACK_AND_REFRESH;
        def.max_stacks = 9;
        def.duration_unit = DurationUnit::PERMANENT;
        def.tick_timing = TickTiming::START_OF_TURN;
        EffectDef tick;
        tick.id = 1;
        tick.type = EffectType::DEAL_DAMAGE;
        tick.trigger = EventType::StatusTicked;
        tick.phase = EventPhase::AFTER;
        tick.target = TargetSelectorType::SELF;
        tick.amount = 1;
        tick.amount_scales_with_stacks = true;
        tick.damage_type = DamageType::TRUE;
        def.effects.push_back(std::move(tick));
        catalog.register_status_def(std::move(def));
    }
    {
        // Bleed：轮末受 =层数 伤害，然后 +1 层（逐轮成长）
        StatusDef def;
        def.id = status_ids::kBleed;
        def.display_name = "流血";
        def.description = "轮末：受等于层数的伤害，然后层数 +1";
        def.stack_policy = StackPolicy::STACK_AND_REFRESH;
        def.max_stacks = 9;
        def.duration_unit = DurationUnit::PERMANENT;
        def.tick_timing = TickTiming::ROUND_END;
        def.stack_delta_on_tick = 1;
        EffectDef tick;
        tick.id = 1;
        tick.type = EffectType::DEAL_DAMAGE;
        tick.trigger = EventType::StatusTicked;
        tick.phase = EventPhase::AFTER;
        tick.target = TargetSelectorType::SELF;
        tick.amount = 1;
        tick.amount_scales_with_stacks = true;
        tick.damage_type = DamageType::TRUE;
        def.effects.push_back(std::move(tick));
        catalog.register_status_def(std::move(def));
    }
    {
        // Stun：跳过回合（DD-019：ACTOR_TURNS 时长模型，1 层持续 1 个后续回合）
        StatusDef def;
        def.id = status_ids::kStun;
        def.display_name = "眩晕";
        def.description = "跳过回合";
        def.stack_policy = StackPolicy::REPLACE_STRONGER;
        def.max_stacks = 1;
        def.duration_unit = DurationUnit::ACTOR_TURNS;
        def.duration_value = 1;
        def.tick_timing = TickTiming::EVENT_DRIVEN;
        def.blocks_turn = true;
        catalog.register_status_def(std::move(def));
    }
    {
        // Weakness：STR/DEX/SPD −2
        StatusDef def;
        def.id = status_ids::kWeakness;
        def.display_name = "虚弱";
        def.description = "STR/DEX/SPD −2";
        def.stack_policy = StackPolicy::REFRESH_ONLY;
        def.duration_unit = DurationUnit::ACTOR_TURNS;
        def.duration_value = 2;
        for (StatType s : {StatType::STR, StatType::DEX, StatType::SPD}) {
            StatModifierDef mod;
            mod.stat = s;
            mod.operation = StatModifierOp::ADD;
            mod.value = -2;
            mod.stacking_mode = StackingMode::ADDITIVE;
            mod.phase = StatPhase::PRE_TRANSFORM_TEMPORARY;
            mod.source_id = def.id;
            def.stat_modifiers.push_back(mod);
        }
        catalog.register_status_def(std::move(def));
    }

    {
        // Wet：湿润（水域/苔沼进入施加；火焰类状态施加时移除，元素交互）
        StatusDef def;
        def.id = status_ids::kWet;
        def.display_name = "湿润";
        def.description = "被火焰类状态移除";
        def.tags.push_back("wet");
        def.stack_policy = StackPolicy::UNIQUE;
        def.duration_unit = DurationUnit::PERMANENT;
        def.tick_timing = TickTiming::EVENT_DRIVEN;
        catalog.register_status_def(std::move(def));
    }

    // ------------------------------------------------------------------ 被动
    {
        // 法力学者（设计文档 §15.3）：友军消耗 ≥7 法力 → 事件来源获得全属性提升 1 层
        PassiveDef def;
        def.id = passive_ids::kManaScholar;
        def.display_name = "法力学者";
        def.description = "友军消耗至少 7 点法力时，其获得 1 层全属性提升";
        EffectDef eff;
        eff.id = 1;
        eff.type = EffectType::APPLY_STATUS;
        eff.trigger = EventType::ResourceSpent;
        eff.phase = EventPhase::AFTER;
        eff.priority = 0;
        eff.condition = ConditionNode::all({
            ConditionNode::make(ConditionOp::SOURCE_IS_ALLY_OF_OWNER),
            ConditionNode::make(ConditionOp::EVENT_AMOUNT_GE, 7),
        });
        eff.target = TargetSelectorType::EVENT_SOURCE;
        eff.status_id = status_ids::kAllStatsUp;
        eff.stacks = 1;
        eff.duration = 1;
        eff.limit_scope = TriggerLimitScope::PER_EVENT;
        eff.limit_count = 1;
        def.effects.push_back(std::move(eff));
        catalog.register_passive_def(std::move(def));
    }
    {
        // 猎人奖励（设计文档 §15.4）：击杀敌人 → 自己 +3 法力、+1 力量提升
        PassiveDef def;
        def.id = passive_ids::kHunterReward;
        def.display_name = "猎人奖励";
        def.description = "击杀敌人后获得 3 点法力和 1 层力量提升";
        const ConditionNode kill_condition = ConditionNode::all({
            ConditionNode::make(ConditionOp::EVENT_TARGET_IS_ENEMY_OF_OWNER),
            ConditionNode::make(ConditionOp::OWNER_IS_LAST_EFFECTIVE_DAMAGE_SOURCE),
        });
        EffectDef mana_eff;
        mana_eff.id = 1;
        mana_eff.type = EffectType::GAIN_RESOURCE;
        mana_eff.trigger = EventType::UnitDied;
        mana_eff.phase = EventPhase::AFTER;
        mana_eff.priority = 10;
        mana_eff.condition = kill_condition;
        mana_eff.target = TargetSelectorType::SELF;
        mana_eff.resource = ResourceType::MANA;
        mana_eff.amount = 3;
        mana_eff.limit_scope = TriggerLimitScope::PER_EVENT;
        mana_eff.limit_count = 1;
        def.effects.push_back(std::move(mana_eff));

        EffectDef str_eff;
        str_eff.id = 2;
        str_eff.type = EffectType::APPLY_STATUS;
        str_eff.trigger = EventType::UnitDied;
        str_eff.phase = EventPhase::AFTER;
        str_eff.priority = 20;
        str_eff.condition = kill_condition;
        str_eff.target = TargetSelectorType::SELF;
        str_eff.status_id = status_ids::kStrengthUp;
        str_eff.stacks = 1;
        str_eff.duration = 1;
        str_eff.limit_scope = TriggerLimitScope::PER_EVENT;
        str_eff.limit_count = 1;
        def.effects.push_back(std::move(str_eff));
        catalog.register_passive_def(std::move(def));
    }

    {
        // 极值交换（设计文档 §15.2）：战斗初始化阶段一次性交换最高/最低属性
        PassiveDef def;
        def.id = passive_ids::kSwapExtremes;
        def.display_name = "极端对调";
        def.description = "战斗开始时交换最高与最低属性值（仅一次）";
        EffectDef eff;
        eff.id = 1;
        eff.type = EffectType::SWAP_EXTREME_STAT_VALUES;
        eff.trigger = EventType::BattleStarted;
        eff.phase = EventPhase::AFTER;
        eff.priority = 100;
        eff.target = TargetSelectorType::SELF;
        def.effects.push_back(std::move(eff));
        catalog.register_passive_def(std::move(def));
    }
    {
        // 幸运 +21（设计文档 §15.1）：装备/被动阶段常驻修正
        PassiveDef def;
        def.id = passive_ids::kSuperLuck;
        def.display_name = "超级幸运";
        def.description = "幸运 +21";
        StatModifierDef mod;
        mod.stat = StatType::LCK;
        mod.operation = StatModifierOp::ADD;
        mod.value = 21;
        mod.stacking_mode = StackingMode::ADDITIVE;
        mod.phase = StatPhase::EQUIPMENT_AND_PASSIVE;
        mod.source_id = def.id;
        def.stat_modifiers.push_back(mod);
        catalog.register_passive_def(std::move(def));
    }
    {
        // 勇敢：STR +2
        PassiveDef def;
        def.id = passive_ids::kBrave;
        def.display_name = "勇敢";
        def.description = "力量 +2";
        StatModifierDef mod;
        mod.stat = StatType::STR;
        mod.operation = StatModifierOp::ADD;
        mod.value = 2;
        mod.stacking_mode = StackingMode::ADDITIVE;
        mod.phase = StatPhase::EQUIPMENT_AND_PASSIVE;
        mod.source_id = def.id;
        def.stat_modifiers.push_back(mod);
        catalog.register_passive_def(std::move(def));
    }
    {
        // 敏捷：+2 移动 = SPD +4（wiki: Agile 页明示等效）
        PassiveDef def;
        def.id = passive_ids::kAgile;
        def.display_name = "敏捷";
        def.description = "移动 +2（等效速度 +4）";
        StatModifierDef mod;
        mod.stat = StatType::SPD;
        mod.operation = StatModifierOp::ADD;
        mod.value = 4;
        mod.stacking_mode = StackingMode::ADDITIVE;
        mod.phase = StatPhase::EQUIPMENT_AND_PASSIVE;
        mod.source_id = def.id;
        def.stat_modifiers.push_back(mod);
        catalog.register_passive_def(std::move(def));
    }
    {
        // 荆棘（反伤）：受到正伤害时对来源反伤 2（TRUE，无视减免）
        PassiveDef def;
        def.id = passive_ids::kThorns;
        def.display_name = "荆棘";
        def.description = "受到伤害时，对来源反射 2 点真实伤害";
        EffectDef eff;
        eff.id = 1;
        eff.type = EffectType::REFLECT_DAMAGE;
        eff.trigger = EventType::HealthDamaged;
        eff.phase = EventPhase::AFTER;
        eff.priority = 0;
        // 反伤伤害本身不触发反伤（防无限弹射）
        eff.condition = ConditionNode::make(ConditionOp::EVENT_LACKS_TAG, 0, 0, ResourceType::HP,
                                            "reflect");
        eff.target = TargetSelectorType::SELF;
        eff.amount = 2;
        eff.limit_scope = TriggerLimitScope::PER_EVENT;
        eff.limit_count = 1;
        def.effects.push_back(std::move(eff));
        catalog.register_passive_def(std::move(def));
    }
    {
        // 再生：回合结束回血 +1（演示用；Health Regen 状态类效果）
        PassiveDef def;
        def.id = passive_ids::kRegen;
        def.display_name = "再生";
        def.description = "回合结束时回复 1 点生命";
        EffectDef eff;
        eff.id = 1;
        eff.type = EffectType::HEAL;
        eff.trigger = EventType::TurnEnding;
        eff.phase = EventPhase::AFTER;
        eff.priority = 0;
        eff.target = TargetSelectorType::SELF;
        eff.amount = 1;
        eff.limit_scope = TriggerLimitScope::PER_EVENT;
        eff.limit_count = 1;
        def.effects.push_back(std::move(eff));
        catalog.register_passive_def(std::move(def));
    }

    // ------------------------------------------------------------------ 技能
    {
        // 火球：3 蓝，射程 2-4，需视线，5 魔法伤害 + 燃烧 1
        AbilityDef def;
        def.id = ability_ids::kFireball;
        def.display_name = "火球";
        def.description = "远程火焰：5 点魔法伤害并施加 1 层燃烧";
        def.mana_cost = 3;
        def.min_range = 2;
        def.max_range = 4;
        def.needs_los = true;
        def.target_policy = AbilityTargetPolicy::EnemyUnit;
        EffectDef dmg;
        dmg.id = 1;
        dmg.type = EffectType::DEAL_DAMAGE;
        dmg.trigger = EventType::AbilityCastStarted;
        dmg.phase = EventPhase::AFTER;
        dmg.target = TargetSelectorType::CHOSEN_TARGET;
        dmg.amount = 5;
        dmg.damage_type = DamageType::MAGICAL;
        def.payload.push_back(std::move(dmg));
        EffectDef burn;
        burn.id = 2;
        burn.type = EffectType::APPLY_STATUS;
        burn.trigger = EventType::AbilityCastStarted;
        burn.phase = EventPhase::AFTER;
        burn.target = TargetSelectorType::CHOSEN_TARGET;
        burn.status_id = status_ids::kBurn;
        burn.stacks = 1;
        def.payload.push_back(std::move(burn));
        catalog.register_ability_def(std::move(def));
    }
    {
        // 治疗：2 蓝，射程 1-3，友方恢复 4 生命
        AbilityDef def;
        def.id = ability_ids::kHeal;
        def.display_name = "治疗";
        def.description = "恢复友方 4 点生命";
        def.mana_cost = 2;
        def.min_range = 1;
        def.max_range = 3;
        def.target_policy = AbilityTargetPolicy::AllyUnit;
        EffectDef heal;
        heal.id = 1;
        heal.type = EffectType::HEAL;
        heal.trigger = EventType::AbilityCastStarted;
        heal.phase = EventPhase::AFTER;
        heal.target = TargetSelectorType::CHOSEN_TARGET;
        heal.amount = 4;
        def.payload.push_back(std::move(heal));
        catalog.register_ability_def(std::move(def));
    }
    {
        // 电击：2 蓝，射程 2-5，需视线，2 魔法伤害
        AbilityDef def;
        def.id = ability_ids::kZap;
        def.display_name = "电击";
        def.description = "远程 2 点魔法伤害";
        def.mana_cost = 2;
        def.min_range = 2;
        def.max_range = 5;
        def.needs_los = true;
        def.target_policy = AbilityTargetPolicy::EnemyUnit;
        EffectDef dmg;
        dmg.id = 1;
        dmg.type = EffectType::DEAL_DAMAGE;
        dmg.trigger = EventType::AbilityCastStarted;
        dmg.phase = EventPhase::AFTER;
        dmg.target = TargetSelectorType::CHOSEN_TARGET;
        dmg.amount = 2;
        dmg.damage_type = DamageType::MAGICAL;
        def.payload.push_back(std::move(dmg));
        catalog.register_ability_def(std::move(def));
    }
    {
        // 毒液喷吐：1 蓝，射程 2-4，1 伤害 + 中毒 2（敌人用）
        AbilityDef def;
        def.id = ability_ids::kPoisonSpit;
        def.display_name = "毒液喷吐";
        def.description = "1 点伤害并施加 2 层中毒";
        def.mana_cost = 1;
        def.min_range = 2;
        def.max_range = 4;
        def.target_policy = AbilityTargetPolicy::EnemyUnit;
        EffectDef dmg;
        dmg.id = 1;
        dmg.type = EffectType::DEAL_DAMAGE;
        dmg.trigger = EventType::AbilityCastStarted;
        dmg.phase = EventPhase::AFTER;
        dmg.target = TargetSelectorType::CHOSEN_TARGET;
        dmg.amount = 1;
        dmg.damage_type = DamageType::TRUE;
        def.payload.push_back(std::move(dmg));
        EffectDef poison;
        poison.id = 2;
        poison.type = EffectType::APPLY_STATUS;
        poison.trigger = EventType::AbilityCastStarted;
        poison.phase = EventPhase::AFTER;
        poison.target = TargetSelectorType::CHOSEN_TARGET;
        poison.status_id = status_ids::kPoison;
        poison.stacks = 2;
        def.payload.push_back(std::move(poison));
        catalog.register_ability_def(std::move(def));
    }
    {
        // 烈焰扫荡：4 蓝，射程 2-3，半径 1 溅射 3 魔法伤害（演示 AoE）
        AbilityDef def;
        def.id = ability_ids::kFlameSweep;
        def.display_name = "烈焰扫荡";
        def.description = "对目标格半径 1 内所有敌人造成 3 点魔法伤害";
        def.mana_cost = 4;
        def.min_range = 2;
        def.max_range = 3;
        def.target_policy = AbilityTargetPolicy::EnemyUnit;
        EffectDef dmg;
        dmg.id = 1;
        dmg.type = EffectType::DEAL_DAMAGE;
        dmg.trigger = EventType::AbilityCastStarted;
        dmg.phase = EventPhase::AFTER;
        dmg.target = TargetSelectorType::UNITS_IN_RADIUS;
        dmg.radius = 1;
        dmg.amount = 3;
        dmg.damage_type = DamageType::MAGICAL;
        def.payload.push_back(std::move(dmg));
        catalog.register_ability_def(std::move(def));
    }
    // ------------------------------------------------------------------ 装备（被动形态）
    {
        const struct { uint32_t id; const char* name; const char* desc; StatType stat; int32_t value; } defs[] = {
            {passive_ids::kEqLuckyCharm, "幸运护符", "幸运 +3", StatType::LCK, 3},
            {passive_ids::kEqStrengthBelt, "力量腰带", "力量 +2", StatType::STR, 2},
            {passive_ids::kEqSwiftBoots, "疾风之靴", "速度 +2", StatType::SPD, 2},
            {passive_ids::kEqScholarCap, "学者之帽", "智力 +2", StatType::INT, 2},
            {passive_ids::kEqHeartAmulet, "生命护符", "体质 +2", StatType::CON, 2},
        };
        for (const auto& e : defs) {
            PassiveDef def;
            def.id = e.id;
            def.display_name = e.name;
            def.description = e.desc;
            StatModifierDef mod;
            mod.stat = e.stat;
            mod.operation = StatModifierOp::ADD;
            mod.value = e.value;
            mod.stacking_mode = StackingMode::ADDITIVE;
            mod.phase = StatPhase::EQUIPMENT_AND_PASSIVE;
            mod.source_id = def.id;
            def.stat_modifiers.push_back(mod);
            catalog.register_passive_def(std::move(def));
        }
    }

    // ------------------------------------------------------------------ 单位
    // 玩家猫模板（默认全 5 中性）
    catalog.register_unit_def(make_cat_def(unit_ids::kCat, {5, 5, 5, 5, 5, 5, 5}, "猫模板"));

    // 老鼠：近战小怪。HP = 3×4 = 12，移动 4，先攻 2×4 = 8
    catalog.register_unit_def(make_def(unit_ids::kRat, "老鼠",
        {6, 4, 3, 1, 1, 4, 1}, Team::ENEMY, 4, true, 0, false));

    // 蝙蝠：更快更脆。SPD 6 → 移动 4、先攻 12
    catalog.register_unit_def(make_def(unit_ids::kBat, "蝙蝠",
        {4, 6, 2, 1, 1, 6, 2}, Team::ENEMY, 4, true, 0, false));

    // 暴徒：高血近战。CON 8 → HP 32；SPD 2 → 先攻 4；STR 8 → 攻 3
    catalog.register_unit_def(make_def(unit_ids::kThug, "暴徒",
        {8, 3, 8, 1, 1, 2, 1}, Team::ENEMY, 4, true, 0, false));

    // 火妖：远程火球。SPD 6 → 先攻 12；CHA 3 → 法力 3；INT 5 → 回蓝 5
    {
        UnitDef def = make_def(unit_ids::kImp, "火妖",
            {3, 3, 2, 5, 3, 6, 2}, Team::ENEMY, 4, true, 0, false);
        def.ability_ids.push_back(ability_ids::kFireball);
        catalog.register_unit_def(std::move(def));
    }

    // 喷毒者：远程毒液。CHA 4 → 法力 4；SPD 3 → 先攻 6
    {
        UnitDef def = make_def(unit_ids::kSpitter, "喷毒者",
            {4, 5, 3, 3, 4, 3, 1}, Team::ENEMY, 4, true, 0, false);
        def.ability_ids.push_back(ability_ids::kPoisonSpit);
        catalog.register_unit_def(std::move(def));
    }
}

int32_t terrain_enter_damage(TerrainType t) {
    switch (t) {
        case TerrainType::BRAMBLE:       return 2;  // wiki: Bramble 每进入一次 2 伤害
        case TerrainType::THICK_BRAMBLE: return 4;
        case TerrainType::CREEP:         return 1;  // wiki: 首次进入 1 伤害
        case TerrainType::LAVA:          return 5;  // demo 地形
        default:                         return 0;
    }
}

bool terrain_applies_wet(TerrainType t) {
    return t == TerrainType::WATER || t == TerrainType::WATER_CURRENT
           || t == TerrainType::CREEP;  // wiki: Water/Creep 施加 Wet
}

}  // namespace mew::combat
