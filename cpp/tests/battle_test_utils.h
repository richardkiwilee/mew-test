#pragma once
// 战斗测试公共工具：构造标准战场 + 脚本化 AI（确定性、无直接 RNG 调用）

#include "pch.h"
#include "test_framework.h"

#include "combat/content/builtin_content.h"
#include "combat/core/battle_context.h"

namespace mewtest {

using namespace mew::combat;

inline BattleContext make_battle(uint64_t seed, int32_t w = 10, int32_t h = 10) {
    BattleContext ctx(seed);
    ctx.set_grid(w, h, std::vector<TerrainType>(static_cast<size_t>(w) * h, TerrainType::EMPTY));
    register_builtin_content(ctx.catalog());
    return ctx;
}

// 以自定义属性生成玩家猫
inline EntityId spawn_cat(BattleContext& ctx, const std::array<int32_t, 7>& stats, Vec2i pos,
                          uint32_t def_id) {
    UnitDef def = make_cat_def(def_id, stats, std::format("cat{}", def_id));
    ctx.catalog().register_unit_def(def);
    return ctx.spawn_unit(def, Team::PLAYER, pos);
}

// 脚本化 AI：攻击最近敌人（相邻含斜角），否则向敌人走最近路径
inline void run_scripted_ai(BattleContext& ctx) {
    const EntityId cur = ctx.current_unit();
    if (cur == kInvalidEntity) return;
    const UnitRuntime* u = ctx.unit(cur);

    EntityId target = kInvalidEntity;
    int32_t best = std::numeric_limits<int32_t>::max();
    for (const auto& other : ctx.units()) {
        if (other.team == u->team || other.is_gone()) continue;
        const int32_t d = GridMap::manhattan(u->pos, other.pos);
        if (d < best) {
            best = d;
            target = other.entity_id;
        }
    }
    if (target != kInvalidEntity && GridMap::adjacent8(u->pos, ctx.unit(target)->pos)) {
        ctx.try_attack(cur, target);
    } else if (target != kInvalidEntity) {
        const auto path = ctx.path_to(cur, ctx.unit(target)->pos);
        if (!path.empty()) ctx.try_move(cur, path);
    }
}

// 完整打一场（两边都跑脚本 AI），max_actions 兜底防死循环
inline void play_battle(BattleContext& ctx, int max_actions = 2000) {
    ctx.start_battle();
    for (int i = 0; i < max_actions; ++i) {
        if (ctx.battle_over()) break;
        EntityId cur = ctx.current_unit();
        if (cur == kInvalidEntity) {
            cur = ctx.begin_next_turn();
            if (cur == kInvalidEntity) break;
        }
        run_scripted_ai(ctx);
        ctx.end_current_turn();
        ctx.begin_next_turn();
    }
}

// 统计日志中某 message_key 出现次数
inline size_t count_key(const CombatLog& log, std::string_view key) {
    size_t n = 0;
    for (const auto& e : log.entries()) {
        if (e.message_key == key) ++n;
    }
    return n;
}

}  // namespace mewtest
