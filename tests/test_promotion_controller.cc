#include "controller/promotion_controller.h"

#include <unordered_map>
#include <vector>

#include <gtest/gtest.h>

#include "core/block_manager.h"
#include "core/clock.h"
#include "core/content_hash.h"
#include "policy/policies/constant_latency.h"
#include "policy/policies/no_promotion.h"
#include "policy/promotion_policy.h"
#include "prefix/prefix_index.h"
#include "sim/event_sink.h"
#include "tier/tier_manager.h"

using namespace kvcache;
using namespace kvcache::controller;
using namespace kvcache::tier;
using namespace kvcache::policy;
using namespace kvcache::sim;

namespace {

constexpr uint32_t BLOCK_SIZE = 4;
constexpr uint64_t BYTES_PER_BLOCK = 64;

struct AlwaysPromotePolicy : PromotionPolicy {
    PromotionDecision decide(const PromotionContext&, const BlockPolicyFeatures&) override {
        return PromotionDecision::Promote;
    }
    const char* name() const noexcept override { return "always_promote"; }
};

struct Fixture {
    FakeClock clock;
    BlockManager mgr{8, clock};
    prefix::PrefixIndex index{BLOCK_SIZE};
    PayloadStore payload_store{8, BYTES_PER_BLOCK};
    ConstantLatencyModel latency;
    InMemoryEventSink events;

    TierManager makeTierManager(uint32_t gpu_cap = 4, uint32_t host_cap = 4) {
        std::unordered_map<Tier, TierStore> tiers;
        tiers.emplace(Tier::GpuSim, TierStore{gpu_cap});
        tiers.emplace(Tier::Host, TierStore{host_cap});
        return TierManager{std::move(tiers), payload_store, latency, mgr.store()};
    }

    // Creates block in HOST tier and places it via TierManager.
    BlockId admitToHost(TierManager& tm) {
        auto slot = mgr.createBlock({1, 0}, 0, BLOCK_SIZE, Tier::Host);
        EXPECT_TRUE(slot.has_value());
        const auto [id, gen] = *slot;
        mgr.transitionState(id, BlockState::Admitting, BlockState::Ready);
        std::vector<std::byte> payload(BYTES_PER_BLOCK, std::byte{0});
        tm.place(id, Tier::Host, Span<const std::byte>{payload.data(), payload.size()});
        return id;
    }
};

}  // namespace

TEST(PromotionController, NoPromotionPolicyPromotesNothing) {
    Fixture f;
    auto tm = f.makeTierManager();
    NoPromotionPolicy policy;
    PromotionController ctrl{policy, tm, f.mgr, f.index, f.events};

    f.admitToHost(tm);
    const auto result = ctrl.runScan(1000);
    EXPECT_EQ(result.promoted_count, 0u);
    EXPECT_EQ(tm.stats(Tier::GpuSim).used_blocks, 0u);
}

TEST(PromotionController, AlwaysPromoteMovesHostBlockToGpu) {
    Fixture f;
    auto tm = f.makeTierManager();
    AlwaysPromotePolicy policy;
    PromotionController ctrl{policy, tm, f.mgr, f.index, f.events};

    f.admitToHost(tm);
    const auto result = ctrl.runScan(1000);
    EXPECT_EQ(result.promoted_count, 1u);
    EXPECT_EQ(tm.stats(Tier::Host).used_blocks, 0u);
    EXPECT_EQ(tm.stats(Tier::GpuSim).used_blocks, 1u);
}

TEST(PromotionController, EmitsBlockPromotedEvent) {
    Fixture f;
    auto tm = f.makeTierManager();
    AlwaysPromotePolicy policy;
    PromotionController ctrl{policy, tm, f.mgr, f.index, f.events};

    f.admitToHost(tm);
    ctrl.runScan(5000);

    const auto promoted = f.events.eventsOfType<BlockPromoted>();
    ASSERT_EQ(promoted.size(), 1u);
    EXPECT_EQ(promoted[0].from_tier, Tier::Host);
    EXPECT_EQ(promoted[0].to_tier, Tier::GpuSim);
    EXPECT_EQ(promoted[0].timestamp_ns, 5000u);
}

TEST(PromotionController, StopsWhenGpuFull) {
    Fixture f;
    auto tm = f.makeTierManager(1, 4);  // GPU cap = 1
    AlwaysPromotePolicy policy;
    PromotionController ctrl{policy, tm, f.mgr, f.index, f.events};

    f.admitToHost(tm);
    f.admitToHost(tm);
    const auto result = ctrl.runScan(1000);
    // Only one block can fit in GPU_SIM
    EXPECT_EQ(result.promoted_count, 1u);
    EXPECT_EQ(tm.stats(Tier::GpuSim).used_blocks, 1u);
    EXPECT_EQ(tm.stats(Tier::Host).used_blocks, 1u);
}
