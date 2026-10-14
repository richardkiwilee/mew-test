#pragma once
// ============================================================================
// EffectRegistry — EffectType → 执行器注册表（设计文档 §3.2 / §5.4）
// 每个原子效果一个 handler，通过 BattleContext 统一 API 修改状态；
// 新机制才新增 handler，组合效果 = 同一 trigger 下多个 EffectDef。
// ============================================================================

#include <functional>
#include <unordered_map>

#include "combat/domain/passive_def.h"

namespace mew::combat {

class BattleContext;
struct CombatEvent;

// handler 签名：效果定义 / 触发事件 / 所有者 / 已解析目标
using EffectHandler = std::function<void(BattleContext& ctx, const EffectDef& def,
                                         const CombatEvent& event, EntityId owner,
                                         const std::vector<EntityId>& targets)>;

class EffectRegistry {
public:
    // 注册（重复注册覆盖）
    void register_handler(EffectType type, EffectHandler handler);

    // 执行；未注册的 type 记错误日志并返回 false
    bool execute(EffectType type, BattleContext& ctx, const EffectDef& def,
                 const CombatEvent& event, EntityId owner,
                 const std::vector<EntityId>& targets);

    // 内置原子效果（Phase 3 子集；Phase 4+ 扩充属性修正/位移等）
    static void install_builtin(EffectRegistry& registry);

private:
    std::unordered_map<EffectType, EffectHandler> handlers_;
};

}  // namespace mew::combat
