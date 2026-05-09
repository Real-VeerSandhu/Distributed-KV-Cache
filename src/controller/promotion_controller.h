#pragma once

#include <cstdint>

#include "core/block_manager.h"
#include "policy/feature_builders.h"
#include "policy/promotion_policy.h"
#include "prefix/prefix_index.h"
#include "sim/event_sink.h"
#include "tier/tier_manager.h"

namespace kvcache::controller {

struct PromotionResult {
    uint32_t promoted_count;
};

// Scans the HOST tier and promotes hot blocks to GPU_SIM based on PromotionPolicy.
class PromotionController {
public:
    PromotionController(policy::PromotionPolicy& promotion_policy,
                        tier::TierManager& tier_manager, BlockManager& manager,
                        const prefix::PrefixIndex& index, sim::EventSink& events);

    [[nodiscard]] PromotionResult runScan(uint64_t timestamp_ns);

private:
    policy::PromotionPolicy& promotion_policy_;
    tier::TierManager& tier_manager_;
    BlockManager& manager_;
    sim::EventSink& events_;
    policy::BlockFeatureBuilder feature_builder_;
};

}  // namespace kvcache::controller
