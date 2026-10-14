#pragma once
// ============================================================================
// ActionResolver — 行动校验与结算（设计文档 §7）
// 移动/基础攻击；校验（行动资源/距离/目标合法性）→ 支付成本 → 结算 → 日志。
// AI 与玩家 UI 都只提交"意图"，由本模块验证后执行。
// ============================================================================

#include <string>
#include <vector>

#include "combat/core/basic_types.h"
#include "combat/core/vec2i.h"

namespace mew::combat {

class BattleContext;

struct MoveResult {
    bool ok = false;
    std::string reject;                 // 拒绝原因（日志/UI 提示）
};

struct AttackResult {
    bool ok = false;
    std::string reject;
};

struct CastResult {
    bool ok = false;
    std::string reject;
};

class ActionResolver {
public:
    explicit ActionResolver(BattleContext& ctx) : ctx_(ctx) {}

    // 路径：从当前格开始的四方向步进序列（含起点）
    MoveResult try_move(EntityId unit_id, const std::vector<Vec2i>& path);

    AttackResult try_attack(EntityId attacker_id, EntityId target_id);

    // 施放技能（设计文档 §7 事务化流水线：校验 → 支付 → payload 按定义序执行 → 完成）
    CastResult try_cast_ability(EntityId caster_id, uint32_t ability_id, EntityId target_id,
                                Vec2i target_tile);

private:
    BattleContext& ctx_;
};

}  // namespace mew::combat
