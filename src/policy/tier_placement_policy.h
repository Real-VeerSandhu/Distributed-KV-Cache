#pragma once

#include "policy/policy_features.h"
#include "tier/tier.h"

namespace kvcache::policy {

struct PlacementContext {
    uint32_t gpu_sim_used_blocks;
    uint32_t gpu_sim_capacity_blocks;
    uint32_t host_used_blocks;
    uint32_t host_capacity_blocks;
    uint64_t timestamp_ns;
};

class TierPlacementPolicy {
public:
    virtual ~TierPlacementPolicy() = default;

    TierPlacementPolicy(const TierPlacementPolicy&) = delete;
    TierPlacementPolicy& operator=(const TierPlacementPolicy&) = delete;

    [[nodiscard]] virtual Tier chooseTier(const PlacementContext& context,
                                          const BlockCandidateFeatures& candidate) = 0;

    [[nodiscard]] virtual const char* name() const noexcept = 0;

protected:
    TierPlacementPolicy() = default;
};

}  // namespace kvcache::policy
