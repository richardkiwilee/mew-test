#include "pch.h"
#include "combat/effects/condition_evaluator.h"

#include "combat/core/battle_context.h"
#include "combat/domain/combat_event.h"

namespace mew::combat {

bool ConditionEvaluator::evaluate(const ConditionNode& node, const EvalContext& ec,
                                  std::string* why_not) const {
    const auto fail = [&](const char* reason) {
        if (why_not) *why_not = reason;
        return false;
    };

    switch (node.op) {
        case ConditionOp::ALWAYS:
            return true;

        case ConditionOp::ALL: {
            for (const auto& child : node.children) {
                if (!evaluate(child, ec, why_not)) return false;
            }
            return true;
        }

        case ConditionOp::ANY: {
            for (const auto& child : node.children) {
                std::string ignored;
                if (evaluate(child, ec, &ignored)) {
                    if (why_not) why_not->clear();
                    return true;
                }
            }
            return fail("ANY: no child condition passed");
        }

        case ConditionOp::NOT: {
            std::string ignored;
            if (!node.children.empty() && evaluate(node.children[0], ec, &ignored)) {
                return fail("NOT: child condition passed");
            }
            return true;
        }

        case ConditionOp::SOURCE_IS_ALLY_OF_OWNER: {
            const UnitRuntime* owner = ctx_.unit(ec.owner);
            const UnitRuntime* source = ctx_.unit(ec.event.source);
            if (owner && source && ec.event.source != kInvalidEntity && owner->team == source->team)
                return true;
            return fail("SOURCE_IS_ALLY_OF_OWNER: source is not an ally");
        }

        case ConditionOp::EVENT_TARGET_IS_ENEMY_OF_OWNER: {
            const UnitRuntime* owner = ctx_.unit(ec.owner);
            const UnitRuntime* target = ctx_.unit(ec.event.target);
            if (owner && target && ec.event.target != kInvalidEntity && owner->team != target->team)
                return true;
            return fail("EVENT_TARGET_IS_ENEMY_OF_OWNER: target is not an enemy");
        }

        case ConditionOp::OWNER_IS_LAST_EFFECTIVE_DAMAGE_SOURCE:
            if (ctx_.last_effective_damage_source() == ec.owner) return true;
            return fail("OWNER_IS_LAST_EFFECTIVE_DAMAGE_SOURCE: not the last damage source");

        case ConditionOp::EVENT_AMOUNT_GE:
            if (ec.event.amount >= node.value) return true;
            return fail("EVENT_AMOUNT_GE: amount below threshold");

        case ConditionOp::OWNER_HP_RATIO_LE: {
            const UnitRuntime* owner = ctx_.unit(ec.owner);
            if (!owner) return fail("OWNER_HP_RATIO_LE: owner missing");
            const int32_t max_hp = ctx_.stats().max_hp(ctx_, ec.owner);
            const int64_t ratio = max_hp > 0 ? static_cast<int64_t>(owner->hp) * kPercentBase / max_hp : 0;
            if (ratio <= node.value) return true;
            return fail("OWNER_HP_RATIO_LE: hp ratio above threshold");
        }

        case ConditionOp::EVENT_TARGET_HP_RATIO_LE: {
            const UnitRuntime* target = ctx_.unit(ec.event.target);
            if (!target) return fail("EVENT_TARGET_HP_RATIO_LE: target missing");
            const int32_t max_hp = ctx_.stats().max_hp(ctx_, ec.event.target);
            const int64_t ratio = max_hp > 0 ? static_cast<int64_t>(target->hp) * kPercentBase / max_hp : 0;
            if (ratio <= node.value) return true;
            return fail("EVENT_TARGET_HP_RATIO_LE: hp ratio above threshold");
        }

        case ConditionOp::OWNER_HAS_STATUS:
            if (ctx_.status_system().has(ec.owner, node.status_id)) return true;
            return fail("OWNER_HAS_STATUS: status not present");

        case ConditionOp::EVENT_TARGET_HAS_STATUS:
            if (ec.event.target != kInvalidEntity
                && ctx_.status_system().has(ec.event.target, node.status_id))
                return true;
            return fail("EVENT_TARGET_HAS_STATUS: status not present");

        case ConditionOp::EVENT_RESOURCE_IS:
            if (ec.event.resource == node.resource) return true;
            return fail("EVENT_RESOURCE_IS: resource mismatch");

        case ConditionOp::EVENT_HAS_TAG: {
            const auto has = std::find(ec.event.tags.begin(), ec.event.tags.end(),
                                       node.text_param) != ec.event.tags.end();
            if (has) return true;
            return fail("EVENT_HAS_TAG: tag absent");
        }

        case ConditionOp::EVENT_LACKS_TAG: {
            const auto has = std::find(ec.event.tags.begin(), ec.event.tags.end(),
                                       node.text_param) != ec.event.tags.end();
            if (!has) return true;
            return fail("EVENT_LACKS_TAG: tag present");
        }

        case ConditionOp::RANDOM_CHANCE:
            if (ctx_.rng().roll_chance(clamp_to(node.value, 0, kPercentBase), "effect_chance_condition"))
                return true;
            return fail("RANDOM_CHANCE: roll failed");
    }
    return fail("unknown condition op");
}

}  // namespace mew::combat
