#pragma once

#include <memory>

#include "policy/admission_policy.h"
#include "policy/demotion_policy.h"
#include "policy/eviction_policy.h"
#include "policy/latency_model.h"
#include "policy/prefetch_policy.h"
#include "policy/promotion_policy.h"
#include "policy/replica_selection_policy.h"
#include "policy/routing_policy.h"
#include "policy/tier_placement_policy.h"

namespace kvcache {

struct PolicyConfig;
struct LatencyParams;

namespace policy {

struct PolicyRegistry {
    std::unique_ptr<AdmissionPolicy> admission;
    std::unique_ptr<EvictionPolicy> eviction;
    std::unique_ptr<TierPlacementPolicy> tier_placement;
    std::unique_ptr<PromotionPolicy> promotion;
    std::unique_ptr<DemotionPolicy> demotion;
    std::unique_ptr<RoutingPolicy> routing;
    std::unique_ptr<ReplicaSelectionPolicy> replica_selection;
    std::unique_ptr<PrefetchPolicy> prefetch;
    std::unique_ptr<LatencyModel> latency;

    [[nodiscard]] static PolicyRegistry fromConfig(const PolicyConfig& cfg,
                                                   const LatencyParams& latency_params);
};

}  // namespace policy
}  // namespace kvcache
