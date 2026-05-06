#include <gtest/gtest.h>

#include "core/clock.h"

namespace kvcache {

TEST(FakeClock, StartsAtZero) {
    const FakeClock clk;
    EXPECT_EQ(clk.nowNs(), 0U);
}

TEST(FakeClock, StartsAtSuppliedValue) {
    const FakeClock clk{1000};
    EXPECT_EQ(clk.nowNs(), 1000U);
}

TEST(FakeClock, AdvanceIncreasesTime) {
    FakeClock clk;
    clk.advanceNs(500);
    EXPECT_EQ(clk.nowNs(), 500U);
}

TEST(FakeClock, MultipleAdvancesAccumulate) {
    FakeClock clk;
    clk.advanceNs(100);
    clk.advanceNs(200);
    clk.advanceNs(300);
    EXPECT_EQ(clk.nowNs(), 600U);
}

TEST(FakeClock, AdvanceByZeroIsNoop) {
    FakeClock clk{999};
    clk.advanceNs(0);
    EXPECT_EQ(clk.nowNs(), 999U);
}

TEST(FakeClock, ResetRestoresTime) {
    FakeClock clk;
    clk.advanceNs(9000);
    clk.reset(42);
    EXPECT_EQ(clk.nowNs(), 42U);
}

TEST(FakeClock, ResetWithNoArgResetsToZero) {
    FakeClock clk{5000};
    clk.reset();
    EXPECT_EQ(clk.nowNs(), 0U);
}

TEST(FakeClock, UsableViaClockInterface) {
    FakeClock clk;
    Clock& base = clk;
    base.advanceNs(77);
    EXPECT_EQ(base.nowNs(), 77U);
}

TEST(RealClock, ReturnsPositiveTimestamp) {
    const RealClock clk;
    EXPECT_GT(clk.nowNs(), 0U);
}

TEST(RealClock, IsMonotonic) {
    const RealClock clk;
    const uint64_t t1 = clk.nowNs();
    const uint64_t t2 = clk.nowNs();
    EXPECT_GE(t2, t1);
}

TEST(RealClock, AdvanceIsNoop) {
    const RealClock clk;
    const uint64_t before = clk.nowNs();
    // RealClock::advanceNs is a no-op; time does not jump
    const_cast<RealClock&>(clk).advanceNs(1'000'000'000ULL);
    const uint64_t after = clk.nowNs();
    // Wall time never jumps by a full second just from advanceNs
    EXPECT_LT(after - before, 1'000'000'000ULL);
}

}  // namespace kvcache
