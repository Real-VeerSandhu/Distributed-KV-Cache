#include "controller/demotion_controller.h"

#include <unordered_map>
#include <vector>

#include <gtest/gtest.h>

#include "core/block_manager.h"
#include "core/clock.h"
#include "core/content_hash.h"
#include "policy/demotion_policy.h"
#include "policy/policies/constant_latency.h"
#include "policy/policies/no_demotion.h"
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

struct AlwaysDemotePolicy : DemotionPolicy {
    DemotionDecision decide(const DemotionContext&, const BlockPolicyFeatures&) override {
        return DemotionDecision::Demote;
    }
    const char* name() const noexcept override { return "always_demote"; }
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

    BlockId admitToGpu(TierManager& tm) {
        auto slot = mgr.createBlock({1, 0}, 0, BLOCK_SIZE, Tier::GpuSim);
        EXPECT_TRUE(slot.has_value());
        const auto [id, gen] = *slot;
        mgr.transitionState(id, BlockState::Admitting, BlockState::Ready);
        std::vector<std::byte> payload(BYTES_PER_BLOCK, std::byte{0});
        tm.place(id, Tier::GpuSim, Span<const std::byte>{payload.data(), payload.size()});
        return id;
    }
};

}  // namespace

TEST(DemotionController, NoDemotionPolicyDemotesNothing) {
    Fixture f;
    auto tm = f.makeTierManager();
    NoDemotionPolicy policy;
    DemotionController ctrl{policy, tm, f.mgr, f.index, f.events};

    f.admitToGpu(tm);
    const auto result = ctrl.runScan(1000);
    EXPECT_EQ(result.demoted_count, 0u);
    EXPECT_EQ(tm.stats(Tier::GpuSim).used_blocks, 1u);
}

TEST(DemotionController, AlwaysDemoteMovesGpuBlockToHost) {
    Fixture f;
    auto tm = f.makeTierManager();
    AlwaysDemotePolicy policy;
    DemotionController ctrl{policy, tm, f.mgr, f.index, f.events};

    f.admitToGpu(tm);
    const auto result = ctrl.runScan(1000);
    EXPECT_EQ(result.demoted_count, 1u);
    EXPECT_EQ(tm.stats(Tier::GpuSim).used_blocks, 0u);
    EXPECT_EQ(tm.stats(Tier::Host).used_blocks, 1u);
}

TEST(DemotionController, EmitsBlockDemotedEvent) {
    Fixture f;
    auto tm = f.makeTierManager();
    AlwaysDemotePolicy policy;
    DemotionController ctrl{policy, tm, f.mgr, f.index, f.events};

    f.admitToGpu(tm);
    ctrl.runScan(7000);

    const auto demoted = f.events.eventsOfType<BlockDemoted>();
    ASSERT_EQ(demoted.size(), 1u);
    EXPECT_EQ(demoted[0].from_tier, Tier::GpuSim);
    EXPECT_EQ(demoted[0].to_tier, Tier::Host);
    EXPECT_EQ(demoted[0].timestamp_ns, 7000u);
}

TEST(DemotionController, StopsWhenHostFull) {
    Fixture f;
    auto tm = f.makeTierManager(4, 1);  // HOST cap = 1
    AlwaysDemotePolicy policy;
    DemotionController ctrl{policy, tm, f.mgr, f.index, f.events};

    f.admitToGpu(tm);
    f.admitToGpu(tm);
    const auto result = ctrl.runScan(1000);
    EXPECT_EQ(result.demoted_count, 1u);
    EXPECT_EQ(tm.stats(Tier::Host).used_blocks, 1u);
}
