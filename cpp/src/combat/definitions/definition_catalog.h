#pragma once
// ============================================================================
// DefinitionCatalog — 只读定义目录（设计文档 §3.2）
// 负责加载/索引/验证定义（ID 重复、引用检查）；不执行战斗效果。
// Phase 2：仅 UnitDef；Phase 6 起扩展 PassiveDef/StatusDef/AbilityDef 等。
// ============================================================================

#include <string>
#include <unordered_map>

#include "combat/domain/ability_def.h"
#include "combat/domain/passive_def.h"
#include "combat/domain/status_def.h"
#include "combat/domain/unit_def.h"

namespace mew::combat {

class DefinitionCatalog {
public:
    void register_unit_def(UnitDef def);
    void register_passive_def(PassiveDef def);
    void register_status_def(StatusDef def);
    void register_ability_def(AbilityDef def);

    const UnitDef* unit_def(uint32_t id) const;
    const UnitDef& require_unit_def(uint32_t id) const;  // 未找到则 assert
    const PassiveDef* passive_def(uint32_t id) const;
    const StatusDef* status_def(uint32_t id) const;
    const AbilityDef* ability_def(uint32_t id) const;

    const std::unordered_map<uint32_t, UnitDef>& unit_defs() const { return unit_defs_; }

    // 校验：重复 ID / 未知引用（效果引用的状态、被动引用的效果类型等）报错返回 false
    bool validate(std::string* errors_out) const;

private:
    std::unordered_map<uint32_t, UnitDef> unit_defs_;
    std::unordered_map<uint32_t, PassiveDef> passive_defs_;
    std::unordered_map<uint32_t, StatusDef> status_defs_;
    std::unordered_map<uint32_t, AbilityDef> ability_defs_;
};

}  // namespace mew::combat
