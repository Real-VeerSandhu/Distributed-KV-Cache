#include "controller/admission_controller.h"
#include "controller/demotion_controller.h"
#include "controller/eviction_controller.h"
#include "controller/promotion_controller.h"
#include "controller/safe_candidate_finder.h"

#include <numeric>
#include <unordered_map>
#include <vector>

#include <gtest/gtest.h>

#include "core/block_manager.h"
#include "core/clock.h"
#include "core/content_hash.h"
#include "policy/demotion_policy.h"
#include "policy/policies/always_admit.h"
#include "policy/policies/bandwidth_latency.h"
#include "policy/policies/constant_latency.h"
#include "policy/policies/gpu_first_placement.h"
#include "policy/policies/lru_eviction.h"
#include "policy/policies/no_demotion.h"
#include "policy/policies/no_promotion.h"
#include "policy/promotion_policy.h"
#include "prefix/prefix_index.h"
#include "prefix/prefix_lookup_engine.h"
#include "sim/decision_logger.h"
#include "sim/event_sink.h"
#include "tier/payload_store.h"
#include "tier/tier_manager.h"
#include "tier/tier_store.h"

using namespace kvcache;
using namespace kvcache::tier;
using namespace kvcache::policy;
using namespace kvcache::sim;
using namespace kvcache::controller;

namespace {

constexpr uint32_t BLOCK_SIZE = 4;
constexpr uint32_t GPU_CAP = 4;
constexpr uint32_t HOST_CAP = 16;
constexpr uint32_t TOTAL_BLOCKS = GPU_CAP + HOST_CAP;

CacheKeyContext makeCtx() {
    return CacheKeyContext{ModelId{1}, TokenizerId{1}, BLOCK_SIZE, DType::BF16, 32, 8, 128, 0, 0};
}

TierManager buildTierManager(PayloadStore& ps, policy::LatencyModel& lat, BlockStore& store,
                               uint32_t gpu_cap = GPU_CAP, uint32_t host_cap = HOST_CAP) {
    std::unordered_map<Tier, TierStore> tiers;
    tiers.emplace(Tier::GpuSim, TierStore{gpu_cap});
    tiers.emplace(Tier::Host, TierStore{host_cap});
    return TierManager{std::move(tiers), ps, lat, store};
}

// Full Phase-3 rig: builds all controllers manually so TierManager lifetime is owned by the test.
struct Phase3Rig {
    FakeClock clock;
    AlwaysAdmitPolicy admit;
    LruEvictionPolicy evict;
    GpuFirstPlacementPolicy placement;
    InMemoryEventSink events;
    InMemoryDecisionLogger decisions;
    PayloadStore payload_store{TOTAL_BLOCKS + 4, 0};  // metadata_only
    ConstantLatencyModel latency;

    BlockManager block_mgr;
    prefix::PrefixIndex prefix_idx;
    prefix::PrefixLookupEngine engine;
    SafeCandidateFinder finder;
    TierManager tier_mgr;
    EvictionController eviction_ctrl;
    AdmissionController admission_ctrl;

    explicit Phase3Rig(uint32_t block_capacity = TOTAL_BLOCKS + 4, uint32_t gpu_cap = GPU_CAP,
                       uint32_t host_cap = HOST_CAP)
        : block_mgr(block_capacity, clock),
          prefix_idx(BLOCK_SIZE),
          engine(prefix_idx, block_mgr),
          finder(block_mgr.store()),
          tier_mgr(buildTierManager(payload_store, latency, block_mgr.store(), gpu_cap,
                                     host_cap)),
          eviction_ctrl(evict, block_mgr, engine, finder, tier_mgr, events, decisions),
          admission_ctrl(admit, placement, tier_mgr, eviction_ctrl, block_mgr, engine, BLOCK_SIZE,
                          events, decisions) {}

    // Admit a standalone block at index 0 with unique tokens derived from slot id.
    AdmitResult admitBlock(uint32_t slot_id, uint64_t ts = 1000) {
        const auto ctx = makeCtx();
        const CacheContextHash ctx_hash = computeContextHash(ctx);
        std::vector<TokenId> toks(BLOCK_SIZE);
        for (uint32_t t = 0; t < BLOCK_SIZE; ++t) {
            toks[t] = static_cast<TokenId>(slot_id * 100 + t + 1);
        }
        const ContentHash h = computeContentHash(ctx_hash, 0, BLOCK_SIZE,
                                                  Span<const TokenId>{toks.data(), toks.size()});
        BlockCandidate candidate{ctx, 0, toks, h, {}, BlockOrigin::LocallyComputed, std::nullopt};
        const AdmissionContext actx{RequestId{slot_id}, ctx_hash, 0, ts};
        return admission_ctrl.admit(candidate, actx);
    }
};

}  // namespace

// ──────────────────────── GPU_SIM fills first ────────────────────────

TEST(ScenarioPhase3, GpuFillsBeforeHost) {
    Phase3Rig rig;

    for (uint32_t i = 0; i < GPU_CAP; ++i) {
        const auto result = rig.admitBlock(i, static_cast<uint64_t>(i + 1) * 1000);
        EXPECT_EQ(result.status, AdmitResult::Status::Admitted);
    }

    EXPECT_EQ(rig.tier_mgr.stats(Tier::GpuSim).used_blocks, GPU_CAP);
    EXPECT_EQ(rig.tier_mgr.stats(Tier::Host).used_blocks, 0u);
}

// ──────────────────────── HOST spills on GPU overflow ────────────────────────

TEST(ScenarioPhase3, OverflowBlocksGoToHost) {
    Phase3Rig rig;
    const uint32_t overflow = 4;

    for (uint32_t i = 0; i < GPU_CAP + overflow; ++i) {
        rig.admitBlock(i, static_cast<uint64_t>(i + 1) * 1000);
    }

    EXPECT_EQ(rig.tier_mgr.stats(Tier::GpuSim).used_blocks, GPU_CAP);
    EXPECT_GT(rig.tier_mgr.stats(Tier::Host).used_blocks, 0u);
}

// ──────────────────────── AdmissionEvents emitted ────────────────────────

TEST(ScenarioPhase3, AdmissionEventsEmittedWithCorrectTier) {
    Phase3Rig rig;

    for (uint32_t i = 0; i < GPU_CAP + 2; ++i) {
        rig.admitBlock(i, static_cast<uint64_t>(i + 1) * 1000);
    }

    const auto admitted = rig.events.eventsOfType<BlockAdmitted>();
    ASSERT_GE(admitted.size(), GPU_CAP);

    uint32_t gpu_count = 0;
    uint32_t host_count = 0;
    for (const auto& ev : admitted) {
        if (ev.tier == Tier::GpuSim) ++gpu_count;
        if (ev.tier == Tier::Host) ++host_count;
    }
    EXPECT_EQ(gpu_count, GPU_CAP);
    EXPECT_GE(host_count, 1u);
}

// ──────────────────────── Eviction clears tier accounting ────────────────────────

TEST(ScenarioPhase3, EvictionRemovesBlockFromTierTracking) {
    Phase3Rig rig;

    // Fill both tiers completely so the next admission requires eviction.
    for (uint32_t i = 0; i < TOTAL_BLOCKS; ++i) {
        rig.admitBlock(i, static_cast<uint64_t>(i + 1) * 1000);
    }
    EXPECT_EQ(rig.tier_mgr.stats(Tier::GpuSim).used_blocks, GPU_CAP);
    EXPECT_EQ(rig.tier_mgr.stats(Tier::Host).used_blocks, HOST_CAP);

    // Both tiers full: LRU evicts the oldest Host block to make room.
    rig.admitBlock(200, 999999);

    const auto evicted = rig.events.eventsOfType<BlockEvicted>();
    EXPECT_GE(evicted.size(), 1u);
}

// ──────────────────────── BandwidthLatencyModel ────────────────────────

TEST(ScenarioPhase3, BandwidthModelGpuFasterThanHost) {
    BandwidthLatencyModel model{100, 200.0, 20.0};
    const TierAccessInput gpu_in{Tier::GpuSim, 65536};
    const TierAccessInput host_in{Tier::Host, 65536};
    EXPECT_LT(model.tierAccessNs(gpu_in), model.tierAccessNs(host_in));
}

TEST(ScenarioPhase3, BandwidthModelZeroPayloadReturnsBase) {
    BandwidthLatencyModel model{500};
    EXPECT_EQ(model.tierAccessNs({Tier::GpuSim, 0}), 500u);
    EXPECT_EQ(model.tierAccessNs({Tier::Host, 0}), 500u);
}

TEST(ScenarioPhase3, BandwidthModelMigrationIncludesTransferCost) {
    BandwidthLatencyModel model{0, 200.0, 20.0};
    const MigrationInput mig{Tier::GpuSim, Tier::Host, 20};  // 20 bytes / 20 bytes_per_ns = 1 ns
    EXPECT_GE(model.migrationNs(mig), 1u);
}

// ──────────────────────── Promotion/Demotion integration ────────────────────────

TEST(ScenarioPhase3, PromotionMovesBlockToGpu) {
    FakeClock clock;
    BlockManager block_mgr{8, clock};
    prefix::PrefixIndex prefix_idx{BLOCK_SIZE};
    PayloadStore ps{8, 0};
    ConstantLatencyModel latency;
    InMemoryEventSink events;

    std::unordered_map<Tier, TierStore> tiers;
    tiers.emplace(Tier::GpuSim, TierStore{4});
    tiers.emplace(Tier::Host, TierStore{4});
    TierManager tier_mgr{std::move(tiers), ps, latency, block_mgr.store()};

    // Place a block in HOST
    auto slot = block_mgr.createBlock({1, 0}, 0, BLOCK_SIZE, Tier::Host);
    ASSERT_TRUE(slot.has_value());
    const auto [id, gen] = *slot;
    block_mgr.transitionState(id, BlockState::Admitting, BlockState::Ready);
    tier_mgr.place(id, Tier::Host, Span<const std::byte>{});
    EXPECT_EQ(tier_mgr.stats(Tier::Host).used_blocks, 1u);

    struct AlwaysPromote : PromotionPolicy {
        PromotionDecision decide(const PromotionContext&, const BlockPolicyFeatures&) override {
            return PromotionDecision::Promote;
        }
        const char* name() const noexcept override { return "test"; }
    } promo_policy;

    PromotionController promo{promo_policy, tier_mgr, block_mgr, prefix_idx, events};
    const auto result = promo.runScan(1000);

    EXPECT_EQ(result.promoted_count, 1u);
    EXPECT_EQ(tier_mgr.stats(Tier::GpuSim).used_blocks, 1u);
    EXPECT_EQ(tier_mgr.stats(Tier::Host).used_blocks, 0u);
    EXPECT_EQ(events.eventsOfType<BlockPromoted>().size(), 1u);
}

TEST(ScenarioPhase3, DemotionMovesBlockToHost) {
    FakeClock clock;
    BlockManager block_mgr{8, clock};
    prefix::PrefixIndex prefix_idx{BLOCK_SIZE};
    PayloadStore ps{8, 0};
    ConstantLatencyModel latency;
    InMemoryEventSink events;

    std::unordered_map<Tier, TierStore> tiers;
    tiers.emplace(Tier::GpuSim, TierStore{4});
    tiers.emplace(Tier::Host, TierStore{4});
    TierManager tier_mgr{std::move(tiers), ps, latency, block_mgr.store()};

    // Place a block in GPU_SIM
    auto slot = block_mgr.createBlock({1, 0}, 0, BLOCK_SIZE, Tier::GpuSim);
    ASSERT_TRUE(slot.has_value());
    const auto [id, gen] = *slot;
    block_mgr.transitionState(id, BlockState::Admitting, BlockState::Ready);
    tier_mgr.place(id, Tier::GpuSim, Span<const std::byte>{});
    EXPECT_EQ(tier_mgr.stats(Tier::GpuSim).used_blocks, 1u);

    struct AlwaysDemote : DemotionPolicy {
        DemotionDecision decide(const DemotionContext&, const BlockPolicyFeatures&) override {
            return DemotionDecision::Demote;
        }
        const char* name() const noexcept override { return "test"; }
    } demo_policy;

    DemotionController demo{demo_policy, tier_mgr, block_mgr, prefix_idx, events};
    const auto result = demo.runScan(1000);

    EXPECT_EQ(result.demoted_count, 1u);
    EXPECT_EQ(tier_mgr.stats(Tier::GpuSim).used_blocks, 0u);
    EXPECT_EQ(tier_mgr.stats(Tier::Host).used_blocks, 1u);
    EXPECT_EQ(events.eventsOfType<BlockDemoted>().size(), 1u);
}

TEST(ScenarioPhase3, PromotionThenDemotionResultsInHostTier) {
    FakeClock clock;
    BlockManager block_mgr{8, clock};
    prefix::PrefixIndex prefix_idx{BLOCK_SIZE};
    PayloadStore ps{8, 0};
    ConstantLatencyModel latency;
    InMemoryEventSink events;

    std::unordered_map<Tier, TierStore> tiers;
    tiers.emplace(Tier::GpuSim, TierStore{4});
    tiers.emplace(Tier::Host, TierStore{4});
    TierManager tier_mgr{std::move(tiers), ps, latency, block_mgr.store()};

    auto slot = block_mgr.createBlock({1, 0}, 0, BLOCK_SIZE, Tier::Host);
    ASSERT_TRUE(slot.has_value());
    const auto [id, gen] = *slot;
    block_mgr.transitionState(id, BlockState::Admitting, BlockState::Ready);
    tier_mgr.place(id, Tier::Host, Span<const std::byte>{});

    struct AlwaysPromote : PromotionPolicy {
        PromotionDecision decide(const PromotionContext&, const BlockPolicyFeatures&) override {
            return PromotionDecision::Promote;
        }
        const char* name() const noexcept override { return "test"; }
    } promo_policy;

    struct AlwaysDemote : DemotionPolicy {
        DemotionDecision decide(const DemotionContext&, const BlockPolicyFeatures&) override {
            return DemotionDecision::Demote;
        }
        const char* name() const noexcept override { return "test"; }
    } demo_policy;

    PromotionController promo{promo_policy, tier_mgr, block_mgr, prefix_idx, events};
    DemotionController demo{demo_policy, tier_mgr, block_mgr, prefix_idx, events};

    promo.runScan(1000);
    demo.runScan(2000);

    EXPECT_EQ(block_mgr.store().get(id).tier, Tier::Host);
    EXPECT_EQ(events.eventsOfType<BlockPromoted>().size(), 1u);
    EXPECT_EQ(events.eventsOfType<BlockDemoted>().size(), 1u);
}
