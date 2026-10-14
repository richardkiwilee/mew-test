#include "pch.h"
#include "combat/definitions/definition_catalog.h"

namespace mew::combat {

void DefinitionCatalog::register_unit_def(UnitDef def) {
    assert(def.id != 0 && "unit def id must be non-zero");
    const auto [it, inserted] = unit_defs_.emplace(def.id, std::move(def));
    assert(inserted && "duplicate unit def id");
    (void)it;
}

const UnitDef* DefinitionCatalog::unit_def(uint32_t id) const {
    const auto it = unit_defs_.find(id);
    return it == unit_defs_.end() ? nullptr : &it->second;
}

const UnitDef& DefinitionCatalog::require_unit_def(uint32_t id) const {
    const UnitDef* def = unit_def(id);
    assert(def && "unknown unit def id");
    return *def;
}

void DefinitionCatalog::register_passive_def(PassiveDef def) {
    assert(def.id != 0 && "passive def id must be non-zero");
    const auto [it, inserted] = passive_defs_.emplace(def.id, std::move(def));
    assert(inserted && "duplicate passive def id");
    (void)it;
}

void DefinitionCatalog::register_status_def(StatusDef def) {
    assert(def.id != 0 && "status def id must be non-zero");
    const auto [it, inserted] = status_defs_.emplace(def.id, std::move(def));
    assert(inserted && "duplicate status def id");
    (void)it;
}

const PassiveDef* DefinitionCatalog::passive_def(uint32_t id) const {
    const auto it = passive_defs_.find(id);
    return it == passive_defs_.end() ? nullptr : &it->second;
}

const StatusDef* DefinitionCatalog::status_def(uint32_t id) const {
    const auto it = status_defs_.find(id);
    return it == status_defs_.end() ? nullptr : &it->second;
}

void DefinitionCatalog::register_ability_def(AbilityDef def) {
    assert(def.id != 0 && "ability def id must be non-zero");
    const auto [it, inserted] = ability_defs_.emplace(def.id, std::move(def));
    assert(inserted && "duplicate ability def id");
    (void)it;
}

const AbilityDef* DefinitionCatalog::ability_def(uint32_t id) const {
    const auto it = ability_defs_.find(id);
    return it == ability_defs_.end() ? nullptr : &it->second;
}

bool DefinitionCatalog::validate(std::string* errors_out) const {
    std::string errors;
    const auto add_err = [&](const std::string& msg) {
        if (!errors.empty()) errors += "\n";
        errors += msg;
    };
    // 效果引用的状态必须存在（APPLY_STATUS 等）
    for (const auto& [pid, def] : passive_defs_) {
        for (const auto& eff : def.effects) {
            if ((eff.type == EffectType::APPLY_STATUS || eff.type == EffectType::REMOVE_STATUS
                 || eff.type == EffectType::MODIFY_STATUS_STACKS)
                && eff.status_id != 0 && status_def(eff.status_id) == nullptr) {
                add_err(std::format("passive {} effect {} references unknown status {}", pid,
                                    eff.id, eff.status_id));
            }
        }
    }
    // 单位的初始被动必须存在
    for (const auto& [uid, def] : unit_defs_) {
        for (uint32_t pid : def.passive_ids) {
            if (passive_def(pid) == nullptr) {
                add_err(std::format("unit {} references unknown passive {}", uid, pid));
            }
        }
        for (uint32_t aid : def.ability_ids) {
            if (ability_def(aid) == nullptr) {
                add_err(std::format("unit {} references unknown ability {}", uid, aid));
            }
        }
    }
    // 技能 payload 引用的状态必须存在
    for (const auto& [aid, def] : ability_defs_) {
        for (const auto& eff : def.payload) {
            if ((eff.type == EffectType::APPLY_STATUS || eff.type == EffectType::REMOVE_STATUS)
                && eff.status_id != 0 && status_def(eff.status_id) == nullptr) {
                add_err(std::format("ability {} effect {} references unknown status {}", aid,
                                    eff.id, eff.status_id));
            }
            if (eff.type == EffectType::SPAWN_UNIT && unit_def(eff.unit_def_id) == nullptr) {
                add_err(std::format("ability {} effect {} references unknown unit def {}", aid,
                                    eff.id, eff.unit_def_id));
            }
        }
    }
    if (errors_out) *errors_out = errors;
    return errors.empty();
}

}  // namespace mew::combat
