#include "pch.h"
#include "test_framework.h"

#include "battle_test_utils.h"

using namespace mew::combat;
using namespace mewtest;

namespace {

// 注册测试用被动：法力学者 + 猎人奖励（Phase 3 验收，设计文档 §15.3/§15.4）
void register_trigger_test_content(DefinitionCatalog& catalog) {
    register_builtin_content(catalog);
}

EntityId cat_with(BattleContext& ctx, uint32_t passive_id, const std::array<int32_t, 7>& stats,
                  Vec2i pos, uint32_t def_id) {
    const std::vector<uint32_t> passives = passive_id != 0 ? std::vector<uint32_t>{passive_id}
                                                           : std::vector<uint32_t>{};
    UnitDef def = make_cat_def(def_id, stats, std::format("cat{}", def_id), passives);
    ctx.catalog().register_unit_def(def);
    return ctx.spawn_unit(def, Team::PLAYER, pos);
}

}  // namespace

// ---------------------------------------------------------------------------
// P3 验收：法力学者 —— 友军消耗 ≥7 法力 → 事件来源获得 all_stats_up（设计文档 §15.3）
// ---------------------------------------------------------------------------
TEST_CASE(trigger_mana_scholar_fires) {
    BattleContext ctx = make_battle(42);
    register_trigger_test_content(ctx.catalog());
    const EntityId scholar = cat_with(ctx, passive_ids::kManaScholar, {5, 5, 5, 5, 5, 5, 5},
                                      {2, 2}, 100);
    const EntityId friend_cat = cat_with(ctx, 0, {5, 5, 5, 5, 8, 5, 5}, {4, 4}, 101);
    ctx.spawn_unit(ctx.catalog().require_unit_def(unit_ids::kRat), Team::ENEMY, {6, 6});

    // 消耗 7 法力 → 触发
    CHECK(ctx.spend_resource(friend_cat, ResourceType::MANA, 7, "test"));
    CHECK(ctx.status_system().has(friend_cat, status_ids::kAllStatsUp));
    CHECK_EQ(ctx.status_system().stacks_of(friend_cat, status_ids::kAllStatsUp), 1);
    CHECK_EQ(ctx.unit(friend_cat)->mana, 1);  // CHA 8 → 起始 8，-7
    CHECK(count_key(ctx.log(), "TriggerMatched") >= 1);
    CHECK(count_key(ctx.log(), "ConditionPassed") >= 1);
    CHECK(count_key(ctx.log(), "StatusApplied") >= 1);

    // 消耗 3 → 不满足 ≥7，条件失败（先补法力确保花得出去）
    ctx.gain_resource(friend_cat, ResourceType::MANA, 10);
    const size_t applied_before = count_key(ctx.log(), "StatusApplied");
    CHECK(ctx.spend_resource(friend_cat, ResourceType::MANA, 3, "test2"));
    CHECK_EQ(count_key(ctx.log(), "StatusApplied"), applied_before);
    CHECK(count_key(ctx.log(), "ConditionFailed") >= 1);
    (void)scholar;
}

// 条件：敌人消耗法力不触发（SOURCE_IS_ALLY_OF_OWNER 失败）
TEST_CASE(trigger_mana_scholar_enemy_not_ally) {
    BattleContext ctx = make_battle(42);
    register_trigger_test_content(ctx.catalog());
    cat_with(ctx, passive_ids::kManaScholar, {5, 5, 5, 5, 5, 5, 5}, {2, 2}, 100);
    // 高法力敌人（CHA 10 → 上限 30、起始 10）
    UnitDef enemy;
    enemy.id = 501;
    enemy.display_name = "施法敌人";
    enemy.base_stats = {6, 4, 3, 1, 10, 4, 1};
    enemy.default_team = Team::ENEMY;
    enemy.base_move = 4;
    enemy.is_required_enemy = true;
    ctx.catalog().register_unit_def(std::move(enemy));
    const EntityId mage = ctx.spawn_unit(ctx.catalog().require_unit_def(501), Team::ENEMY, {6, 6});
    CHECK(ctx.spend_resource(mage, ResourceType::MANA, 7, "enemy_cast"));
    CHECK(!ctx.status_system().has(mage, status_ids::kAllStatsUp));
    CHECK(count_key(ctx.log(), "ConditionFailed") >= 1);
}

// ---------------------------------------------------------------------------
// P3 验收：猎人奖励 —— 击杀归属正确，环境击杀不归功（设计文档 §15.4）
// ---------------------------------------------------------------------------
TEST_CASE(trigger_hunter_reward_on_kill) {
    BattleContext ctx = make_battle(42);
    register_trigger_test_content(ctx.catalog());
    const EntityId hunter = cat_with(ctx, passive_ids::kHunterReward, {5, 5, 5, 5, 8, 5, 5},
                                     {2, 2}, 100);
    const EntityId rat = ctx.spawn_unit(ctx.catalog().require_unit_def(unit_ids::kRat),
                                        Team::ENEMY, {3, 2});
    ctx.start_battle();

    const int32_t mana_before = ctx.unit(hunter)->mana;
    DamageContext dmg;
    dmg.source = hunter;
    dmg.target = rat;
    dmg.type = DamageType::TRUE;
    dmg.base_amount = 999;
    ctx.deal_damage(dmg);

    // 击杀 → +3 法力 + 1 层力量提升；且法力获得（priority 10）先于状态施加（priority 20）
    CHECK(ctx.unit(rat)->is_gone());
    CHECK_EQ(ctx.unit(hunter)->mana, mana_before + 3);
    CHECK_EQ(ctx.status_system().stacks_of(hunter, status_ids::kStrengthUp), 1);

    // 日志顺序：ResourceGained 在 StatusApplied 之前
    const auto& entries = ctx.log().entries();
    size_t pos_mana = 0, pos_status = 0;
    for (size_t i = 0; i < entries.size(); ++i) {
        if (entries[i].message_key == "ResourceGained") pos_mana = i;
        if (entries[i].message_key == "StatusApplied") pos_status = i;
    }
    CHECK(pos_mana != 0 && pos_status != 0 && pos_mana < pos_status);
}

// 环境击杀（无来源）不触发猎人奖励
TEST_CASE(trigger_hunter_reward_no_env_kill) {
    BattleContext ctx = make_battle(42);
    register_trigger_test_content(ctx.catalog());
    const EntityId hunter = cat_with(ctx, passive_ids::kHunterReward, {5, 5, 5, 5, 8, 5, 5},
                                     {2, 2}, 100);
    const EntityId rat = ctx.spawn_unit(ctx.catalog().require_unit_def(unit_ids::kRat),
                                        Team::ENEMY, {3, 2});
    ctx.start_battle();

    const int32_t mana_before = ctx.unit(hunter)->mana;
    DamageContext dmg;
    dmg.source = kInvalidEntity;  // 环境伤害（如地形）
    dmg.target = rat;
    dmg.type = DamageType::TRUE;
    dmg.base_amount = 999;
    ctx.deal_damage(dmg);

    CHECK(ctx.unit(rat)->is_gone());
    CHECK_EQ(ctx.unit(hunter)->mana, mana_before);  // 无奖励
    CHECK_EQ(ctx.status_system().stacks_of(hunter, status_ids::kStrengthUp), 0);
}

// ---------------------------------------------------------------------------
// 触发次数限制：PER_BATTLE 1（注册一次性被动）
// ---------------------------------------------------------------------------
TEST_CASE(trigger_limit_per_battle) {
    BattleContext ctx = make_battle(42);
    register_trigger_test_content(ctx.catalog());
    // 一次性被动：消耗法力时回 1 法力（仅战斗内首次）
    PassiveDef once;
    once.id = 900;
    once.display_name = "once";
    EffectDef eff;
    eff.id = 1;
    eff.type = EffectType::GAIN_RESOURCE;
    eff.trigger = EventType::ResourceSpent;
    eff.phase = EventPhase::AFTER;
    eff.target = TargetSelectorType::EVENT_SOURCE;
    eff.resource = ResourceType::MANA;
    eff.amount = 1;
    eff.limit_scope = TriggerLimitScope::PER_BATTLE;
    eff.limit_count = 1;
    once.effects.push_back(std::move(eff));
    ctx.catalog().register_passive_def(std::move(once));

    const EntityId cat = cat_with(ctx, 900, {5, 5, 5, 5, 8, 5, 5}, {2, 2}, 100);
    ctx.spawn_unit(ctx.catalog().require_unit_def(unit_ids::kRat), Team::ENEMY, {6, 6});

    CHECK(ctx.spend_resource(cat, ResourceType::MANA, 2, "a"));
    CHECK_EQ(ctx.unit(cat)->mana, 8 - 2 + 1);  // 首次触发 +1
    CHECK(ctx.spend_resource(cat, ResourceType::MANA, 2, "b"));
    CHECK_EQ(ctx.unit(cat)->mana, 8 - 4 + 1);  // 第二次不触发
    CHECK(count_key(ctx.log(), "TriggerLimitReached") >= 1);
}

// ---------------------------------------------------------------------------
// 无限连锁保护（README 要求 / DD-009）：自身消耗→自身消耗 的死循环必须被截断
// ---------------------------------------------------------------------------
TEST_CASE(trigger_infinite_loop_protection) {
    BattleContext ctx = make_battle(42);
    register_trigger_test_content(ctx.catalog());
    // 病态被动：每次消耗法力 → 再消耗 1 法力（自我循环）
    PassiveDef loop;
    loop.id = 901;
    loop.display_name = "loop";
    EffectDef eff;
    eff.id = 1;
    eff.type = EffectType::SPEND_RESOURCE;
    eff.trigger = EventType::ResourceSpent;
    eff.phase = EventPhase::AFTER;
    eff.target = TargetSelectorType::EVENT_SOURCE;
    eff.resource = ResourceType::MANA;
    eff.amount = 1;
    eff.limit_scope = TriggerLimitScope::PER_EVENT;
    eff.limit_count = 1;
    loop.effects.push_back(std::move(eff));
    ctx.catalog().register_passive_def(std::move(loop));

    const EntityId cat = cat_with(ctx, 901, {5, 5, 5, 5, 20, 5, 5}, {2, 2}, 100);
    ctx.spawn_unit(ctx.catalog().require_unit_def(unit_ids::kRat), Team::ENEMY, {6, 6});
    // 法力足够多（上限 CHA×3 = 60）：截断由深度上限触发，而非法力耗尽自然终止
    ctx.gain_resource(cat, ResourceType::MANA, 1000);
    CHECK_EQ(ctx.unit(cat)->mana, 60);  // gain 被上限钳制

    // 一次消耗触发连锁；深度上限 16 截断（事件深度 0..16 被处理、深度 17 被丢弃）：
    // 初始消耗 1 次 + 深度 0..16 各引发 1 次 = 18 次消耗
    CHECK(ctx.spend_resource(cat, ResourceType::MANA, 1, "start_loop"));
    CHECK(count_key(ctx.log(), "ChainLimitReached") >= 1);
    CHECK_EQ(ctx.unit(cat)->mana, 60 - 18);  // 精确可复现：截断后余量确定
}

// ---------------------------------------------------------------------------
// 状态叠层策略（§10.2）
// ---------------------------------------------------------------------------
TEST_CASE(status_stack_policies) {
    BattleContext ctx = make_battle(42);
    register_trigger_test_content(ctx.catalog());

    // STACK_AND_REFRESH + max_stacks=5（all_stats_up）
    {
        const EntityId cat = cat_with(ctx, 0, {5, 5, 5, 5, 5, 5, 5}, {2, 2}, 100);
        ctx.status_system().apply(cat, status_ids::kAllStatsUp, 1, kInvalidEntity, "t");
        ctx.status_system().apply(cat, status_ids::kAllStatsUp, 1, kInvalidEntity, "t");
        CHECK_EQ(ctx.status_system().stacks_of(cat, status_ids::kAllStatsUp), 2);
        ctx.status_system().apply(cat, status_ids::kAllStatsUp, 9, kInvalidEntity, "t");
        CHECK_EQ(ctx.status_system().stacks_of(cat, status_ids::kAllStatsUp), 5);  // 上限钳制
    }

    // REPLACE_STRONGER（Stun 类）：更强覆盖，更弱保留
    {
        StatusDef stun;
        stun.id = 800;
        stun.display_name = "stun_test";
        stun.stack_policy = StackPolicy::REPLACE_STRONGER;
        stun.max_stacks = 3;
        stun.duration_unit = DurationUnit::ACTOR_TURNS;
        stun.duration_value = 2;
        ctx.catalog().register_status_def(std::move(stun));
        const EntityId cat = cat_with(ctx, 0, {5, 5, 5, 5, 5, 5, 5}, {3, 3}, 101);
        ctx.status_system().apply(cat, 800, 2, kInvalidEntity, "t");
        ctx.status_system().apply(cat, 800, 1, kInvalidEntity, "t");   // 更弱 → 不变
        CHECK_EQ(ctx.status_system().stacks_of(cat, 800), 2);
        ctx.status_system().apply(cat, 800, 3, kInvalidEntity, "t");   // 更强 → 覆盖
        CHECK_EQ(ctx.status_system().stacks_of(cat, 800), 3);
    }

    // INDEPENDENT_INSTANCES：独立实例各自计时
    {
        StatusDef dot;
        dot.id = 801;
        dot.display_name = "dot_test";
        dot.stack_policy = StackPolicy::INDEPENDENT_INSTANCES;
        dot.duration_unit = DurationUnit::ACTOR_TURNS;
        dot.duration_value = 3;
        ctx.catalog().register_status_def(std::move(dot));
        const EntityId cat = cat_with(ctx, 0, {5, 5, 5, 5, 5, 5, 5}, {4, 4}, 102);
        ctx.status_system().apply(cat, 801, 1, kInvalidEntity, "t");
        ctx.status_system().apply(cat, 801, 1, kInvalidEntity, "t");
        const auto& insts = ctx.status_system().instances_of(cat);
        size_t count_801 = 0;
        for (const auto& inst : insts) {
            if (inst.status_def_id == 801) ++count_801;
        }
        CHECK_EQ(count_801, size_t(2));  // 两个独立实例
    }
}

// ---------------------------------------------------------------------------
// 带被动的完整战斗保持确定性（事件队列不破坏重放一致性）
// ---------------------------------------------------------------------------
TEST_CASE(trigger_determinism_with_passives) {
    auto run = [] {
        BattleContext ctx = make_battle(777);
        register_trigger_test_content(ctx.catalog());
        cat_with(ctx, passive_ids::kHunterReward, {8, 5, 5, 5, 5, 6, 5}, {2, 2}, 100);
        cat_with(ctx, passive_ids::kManaScholar, {5, 5, 5, 5, 8, 5, 5}, {2, 4}, 101);
        ctx.spawn_unit(ctx.catalog().require_unit_def(unit_ids::kRat), Team::ENEMY, {6, 2});
        ctx.spawn_unit(ctx.catalog().require_unit_def(unit_ids::kBat), Team::ENEMY, {7, 5});
        play_battle(ctx);
        return ctx.log().to_text();
    };
    CHECK_EQ(run(), run());
}

// 定义校验器：未知状态引用报错
TEST_CASE(catalog_validate_unknown_status_ref) {
    DefinitionCatalog catalog;
    register_builtin_content(catalog);
    PassiveDef bad;
    bad.id = 777;
    bad.display_name = "bad";
    EffectDef eff;
    eff.id = 1;
    eff.type = EffectType::APPLY_STATUS;
    eff.trigger = EventType::TurnStarted;
    eff.phase = EventPhase::AFTER;
    eff.target = TargetSelectorType::SELF;
    eff.status_id = 99999;  // 不存在
    bad.effects.push_back(std::move(eff));
    catalog.register_passive_def(std::move(bad));
    std::string errors;
    CHECK(!catalog.validate(&errors));
    CHECK(errors.find("unknown status") != std::string::npos);
}
