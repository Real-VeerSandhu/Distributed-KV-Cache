#pragma once

#include <cstdint>

#include "core/block_manager.h"
#include "policy/demotion_policy.h"
#include "policy/feature_builders.h"
#include "prefix/prefix_index.h"
#include "sim/event_sink.h"
#include "tier/tier_manager.h"

namespace kvcache::controller {

struct DemotionResult {
    uint32_t demoted_count;
};

// Scans the GPU_SIM tier and demotes cold blocks to HOST based on DemotionPolicy.
class DemotionController {
public:
    DemotionController(policy::DemotionPolicy& demotion_policy, tier::TierManager& tier_manager,
                       BlockManager& manager, const prefix::PrefixIndex& index,
                       sim::EventSink& events);

    [[nodiscard]] DemotionResult runScan(uint64_t timestamp_ns);

private:
    policy::DemotionPolicy& demotion_policy_;
    tier::TierManager& tier_manager_;
    BlockManager& manager_;
    sim::EventSink& events_;
    policy::BlockFeatureBuilder feature_builder_;
};

}  // namespace kvcache::controller
