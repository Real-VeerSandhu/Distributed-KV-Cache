#include "core/clock.h"

#include <chrono>

namespace kvcache {

uint64_t RealClock::nowNs() const {
    const auto now = std::chrono::steady_clock::now();
    return static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::nanoseconds>(now.time_since_epoch()).count());
}

void RealClock::advanceNs(uint64_t /*delta*/) {
    // RealClock tracks wall time; callers use FakeClock to drive simulated time.
}

FakeClock::FakeClock(uint64_t initial_ns) noexcept : current_ns_(initial_ns) {}

uint64_t FakeClock::nowNs() const { return current_ns_; }

void FakeClock::advanceNs(uint64_t delta) { current_ns_ += delta; }

void FakeClock::reset(uint64_t ns) noexcept { current_ns_ = ns; }

}  // namespace kvcache
