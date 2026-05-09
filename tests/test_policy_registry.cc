#include <gtest/gtest.h>

#include <stdexcept>
#include <vector>

#include "config/config.h"
#include "core/ids.h"
#include "core/span.h"
#include "policy/policy_features.h"
#include "policy/policy_registry.h"
#include "tier/tier.h"

namespace kvcache::policy {

namespace {

PolicyConfig makeDefaultPolicyConfig() {
    return PolicyConfig{
        .admission = "always_admit",
        .eviction = "lru",
        .tier_placement = "gpu_first",
        .promotion = "no_promotion",
        .demotion = "no_demotion",
        .routing = "random",
        .replica_selection = "first",
        .prefetch = "none",
        .latency = "constant",
    };
}

LatencyParams makeDefaultLatencyParams() {
    return LatencyParams{
        .local_lookup_ns = 1000,
        .tier_access_ns = 5000,
        .migration_ns = 50000,
        .network_fetch_ns = 500000,
    };
}

BlockCandidateFeatures makeCandidateFeatures() {
    return BlockCandidateFeatures{
        .hash = ContentHash{1, 2},
        .block_index = 0,
        .token_count = 16,
        .payload_bytes = 0,
        .origin = BlockOrigin::LocallyComputed,
        .prefix_depth_blocks = 0,
        .estimated_recompute_cost_ns = 10000,
    };
}

BlockPolicyFeatures makeBlockFeatures(uint64_t block_raw, uint64_t last_access_ns) {
    return BlockPolicyFeatures{
        .id = BlockId{block_raw},
        .hash = ContentHash{0, 0},
        .tier = Tier::GpuSim,
        .payload_bytes = 0,
        .age_ns = 0,
        .last_access_ns = last_access_ns,
        .access_count = 1,
        .prefix_depth_blocks = 0,
        .prefix_child_count = 0,
        .descendant_block_count = 0,
        .is_shared_prefix = false,
        .is_remote_origin = false,
        .estimated_recompute_cost_ns = 0,
        .estimated_refetch_cost_ns = 0,
    };
}

}  // namespace

TEST(PolicyRegistry, ConstructsFromDefaultConfig) {
    const auto reg = PolicyRegistry::fromConfig(makeDefaultPolicyConfig(), makeDefaultLatencyParams());
    EXPECT_NE(reg.admission, nullptr);
    EXPECT_NE(reg.eviction, nullptr);
    EXPECT_NE(reg.tier_placement, nullptr);
    EXPECT_NE(reg.promotion, nullptr);
    EXPECT_NE(reg.demotion, nullptr);
    EXPECT_NE(reg.routing, nullptr);
    EXPECT_NE(reg.replica_selection, nullptr);
    EXPECT_NE(reg.prefetch, nullptr);
    EXPECT_NE(reg.latency, nullptr);
}

TEST(PolicyRegistry, ThrowsOnUnknownAdmissionPolicy) {
    auto cfg = makeDefaultPolicyConfig();
    cfg.admission = "nonexistent_policy";
    EXPECT_THROW([&] { return PolicyRegistry::fromConfig(cfg, makeDefaultLatencyParams()); }(),
                 std::runtime_error);
}

TEST(PolicyRegistry, ThrowsOnUnknownEvictionPolicy) {
    auto cfg = makeDefaultPolicyConfig();
    cfg.eviction = "nonexistent_policy";
    EXPECT_THROW([&] { return PolicyRegistry::fromConfig(cfg, makeDefaultLatencyParams()); }(),
                 std::runtime_error);
}

TEST(PolicyRegistry, ThrowsOnUnknownLatencyModel) {
    auto cfg = makeDefaultPolicyConfig();
    cfg.latency = "nonexistent_model";
    EXPECT_THROW([&] { return PolicyRegistry::fromConfig(cfg, makeDefaultLatencyParams()); }(),
                 std::runtime_error);
}

TEST(AlwaysAdmitPolicy, AdmitsEveryCandidate) {
    const auto reg = PolicyRegistry::fromConfig(makeDefaultPolicyConfig(), makeDefaultLatencyParams());
    const AdmissionContext ctx{RequestId{1}, CacheContextHash{42}, 1, 0};
    EXPECT_EQ(reg.admission->decide(ctx, makeCandidateFeatures()), AdmissionDecision::Admit);
}

TEST(AlwaysAdmitPolicy, ReportsCorrectName) {
    const auto reg = PolicyRegistry::fromConfig(makeDefaultPolicyConfig(), makeDefaultLatencyParams());
    EXPECT_STREQ(reg.admission->name(), "always_admit");
}

TEST(LruEvictionPolicy, ChoosesOldestBlock) {
    const auto reg = PolicyRegistry::fromConfig(makeDefaultPolicyConfig(), makeDefaultLatencyParams());

    const std::vector<BlockPolicyFeatures> candidates = {
        makeBlockFeatures(10, 1000),
        makeBlockFeatures(20, 500),   // oldest — should be evicted
        makeBlockFeatures(30, 2000),
    };

    const EvictionContext ctx{Tier::GpuSim, 0, 0};
    const auto victim = reg.eviction->chooseVictim(ctx, make_span(candidates));

    ASSERT_TRUE(victim.has_value());
    EXPECT_EQ(*victim, BlockId{20});
}

TEST(LruEvictionPolicy, ReturnsNulloptForEmptyCandidates) {
    const auto reg = PolicyRegistry::fromConfig(makeDefaultPolicyConfig(), makeDefaultLatencyParams());
    const EvictionContext ctx{Tier::GpuSim, 0, 0};
    const auto victim = reg.eviction->chooseVictim(ctx, Span<const BlockPolicyFeatures>{});
    EXPECT_FALSE(victim.has_value());
}

TEST(LruEvictionPolicy, ChoosesSingleCandidate) {
    const auto reg = PolicyRegistry::fromConfig(makeDefaultPolicyConfig(), makeDefaultLatencyParams());
    const std::vector<BlockPolicyFeatures> candidates = {makeBlockFeatures(7, 999)};
    const EvictionContext ctx{Tier::Host, 0, 0};
    const auto victim = reg.eviction->chooseVictim(ctx, make_span(candidates));
    ASSERT_TRUE(victim.has_value());
    EXPECT_EQ(*victim, BlockId{7});
}

TEST(GpuFirstPlacementPolicy, ChoosesGpuSimWhenCapacityAvailable) {
    const auto reg = PolicyRegistry::fromConfig(makeDefaultPolicyConfig(), makeDefaultLatencyParams());
    const PlacementContext ctx{
        .gpu_sim_used_blocks = 1,
        .gpu_sim_capacity_blocks = 10,
        .host_used_blocks = 0,
        .host_capacity_blocks = 100,
        .timestamp_ns = 0,
    };
    EXPECT_EQ(reg.tier_placement->chooseTier(ctx, makeCandidateFeatures()), Tier::GpuSim);
}

TEST(GpuFirstPlacementPolicy, FallsBackToHostWhenGpuFull) {
    const auto reg = PolicyRegistry::fromConfig(makeDefaultPolicyConfig(), makeDefaultLatencyParams());
    const PlacementContext ctx{
        .gpu_sim_used_blocks = 10,
        .gpu_sim_capacity_blocks = 10,
        .host_used_blocks = 0,
        .host_capacity_blocks = 100,
        .timestamp_ns = 0,
    };
    EXPECT_EQ(reg.tier_placement->chooseTier(ctx, makeCandidateFeatures()), Tier::Host);
}

TEST(NoPromotionPolicy, AlwaysStays) {
    const auto reg = PolicyRegistry::fromConfig(makeDefaultPolicyConfig(), makeDefaultLatencyParams());
    const PromotionContext ctx{500, 1000, 0};
    const auto block = makeBlockFeatures(1, 100);
    EXPECT_EQ(reg.promotion->decide(ctx, block), PromotionDecision::Stay);
}

TEST(NoDemotionPolicy, AlwaysStays) {
    const auto reg = PolicyRegistry::fromConfig(makeDefaultPolicyConfig(), makeDefaultLatencyParams());
    const DemotionContext ctx{900, 1000, 0};
    const auto block = makeBlockFeatures(1, 100);
    EXPECT_EQ(reg.demotion->decide(ctx, block), DemotionDecision::Stay);
}

TEST(NoPrefetchPolicy, ReturnsEmptyPlan) {
    const auto reg = PolicyRegistry::fromConfig(makeDefaultPolicyConfig(), makeDefaultLatencyParams());
    const PrefetchContext ctx{CacheContextHash{1}, 3, NodeId{0}, 0};
    EXPECT_TRUE(reg.prefetch->planPrefetch(ctx).empty());
}

TEST(FirstReplicaPolicy, ReturnsFirstReplica) {
    const auto reg = PolicyRegistry::fromConfig(makeDefaultPolicyConfig(), makeDefaultLatencyParams());
    const std::vector<GlobalBlockRef> replicas = {
        GlobalBlockRef{NodeId{1}, BlockId{10}, 1},
        GlobalBlockRef{NodeId{2}, BlockId{20}, 1},
    };
    const ReplicaSelectionContext ctx{ContentHash{0, 0}, NodeId{0}, 0};
    const auto chosen = reg.replica_selection->chooseReplica(ctx, make_span(replicas));
    ASSERT_TRUE(chosen.has_value());
    EXPECT_EQ(chosen->node_id, NodeId{1});
    EXPECT_EQ(chosen->block_id, BlockId{10});
}

TEST(FirstReplicaPolicy, ReturnsNulloptForEmptyReplicas) {
    const auto reg = PolicyRegistry::fromConfig(makeDefaultPolicyConfig(), makeDefaultLatencyParams());
    const ReplicaSelectionContext ctx{ContentHash{0, 0}, NodeId{0}, 0};
    const auto chosen = reg.replica_selection->chooseReplica(ctx, Span<const GlobalBlockRef>{});
    EXPECT_FALSE(chosen.has_value());
}

TEST(ConstantLatencyModel, ReturnsConfiguredValues) {
    const auto reg = PolicyRegistry::fromConfig(makeDefaultPolicyConfig(), makeDefaultLatencyParams());
    EXPECT_EQ(reg.latency->localLookupNs(LookupCostInput{16, 0}), 1000U);
    EXPECT_EQ(reg.latency->tierAccessNs(TierAccessInput{Tier::GpuSim, 0}), 5000U);
    EXPECT_EQ(reg.latency->migrationNs(MigrationInput{Tier::GpuSim, Tier::Host, 0}), 50000U);
    EXPECT_EQ(reg.latency->networkFetchNs(NetworkFetchInput{0, 0}), 500000U);
}

TEST(RandomRoutingPolicy, ReturnsNodeFromSuppliedList) {
    const auto reg = PolicyRegistry::fromConfig(makeDefaultPolicyConfig(), makeDefaultLatencyParams());
    const std::vector<NodePolicyFeatures> nodes = {
        NodePolicyFeatures{.node_id = NodeId{0}},
        NodePolicyFeatures{.node_id = NodeId{1}},
    };
    const RoutingContext ctx{CacheContextHash{1}, 2, ContentHash{0, 0}, 0};
    const auto chosen = reg.routing->chooseNode(ctx, make_span(nodes));
    EXPECT_TRUE(chosen == NodeId{0} || chosen == NodeId{1});
}

TEST(RandomRoutingPolicy, ReturnsSingleNodeWhenOnlyOne) {
    const auto reg = PolicyRegistry::fromConfig(makeDefaultPolicyConfig(), makeDefaultLatencyParams());
    const std::vector<NodePolicyFeatures> nodes = {NodePolicyFeatures{.node_id = NodeId{5}}};
    const RoutingContext ctx{CacheContextHash{1}, 1, ContentHash{0, 0}, 0};
    EXPECT_EQ(reg.routing->chooseNode(ctx, make_span(nodes)), NodeId{5});
}

}  // namespace kvcache::policy
