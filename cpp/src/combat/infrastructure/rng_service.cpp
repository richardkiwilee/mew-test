#include "pch.h"
#include "combat/infrastructure/rng_service.h"

namespace mew::combat {

uint64_t RngService::next_raw() {
    // splitmix64
    uint64_t z = (state_ += 0x9E3779B97F4A7C15ULL);
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
    return z ^ (z >> 31);
}

void RngService::record(std::string_view reason, int64_t pa, int64_t pb, int64_t result) {
    RngCall call;
    call.call_id = ++call_seq_;
    call.reason = std::string(reason);
    call.param_a = pa;
    call.param_b = pb;
    call.result = result;
    calls_.push_back(std::move(call));
}

uint32_t RngService::next_u32(uint32_t upper, std::string_view reason) {
    assert(upper > 0);
    const uint32_t result = static_cast<uint32_t>(next_raw() % upper);
    record(reason, upper, 0, result);
    return result;
}

int32_t RngService::next_range(int32_t min, int32_t max, std::string_view reason) {
    assert(min <= max);
    const uint64_t span = static_cast<uint64_t>(max) - static_cast<uint64_t>(min) + 1;
    const uint32_t result = static_cast<uint32_t>(next_raw() % span);
    const int32_t value = min + static_cast<int32_t>(result);
    record(reason, min, max, value);
    return value;
}

bool RngService::roll_chance(int32_t chance_bp, std::string_view reason) {
    assert(chance_bp >= 0 && chance_bp <= kPercentBase);
    // 0% / 100% 不消耗随机数 —— 全项目统一，保证回放序列一致
    if (chance_bp <= 0) {
        record(reason, chance_bp, 0, 0);
        return false;
    }
    if (chance_bp >= kPercentBase) {
        record(reason, chance_bp, 0, 1);
        return true;
    }
    const uint32_t roll = static_cast<uint32_t>(next_raw() % kPercentBase);
    const bool ok = roll < static_cast<uint32_t>(chance_bp);
    record(reason, chance_bp, roll, ok ? 1 : 0);
    return ok;
}

uint32_t RngService::next_tie_break(std::string_view reason) {
    const uint32_t result = static_cast<uint32_t>(next_raw());
    record(reason, 0, 0, result);
    return result;
}

}  // namespace mew::combat
