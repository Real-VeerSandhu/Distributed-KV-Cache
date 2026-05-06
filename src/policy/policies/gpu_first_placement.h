#pragma once

#include "policy/tier_placement_policy.h"

namespace kvcache::policy {

class GpuFirstPlacementPolicy : public TierPlacementPolicy {
public:
    GpuFirstPlacementPolicy() = default;

    [[nodiscard]] Tier chooseTier(const PlacementContext& context,
                                  const BlockCandidateFeatures& candidate) override;

    [[nodiscard]] const char* name() const noexcept override;
};

}  // namespace kvcache::policy
