#pragma once

#include <cstdint>

#include "controller/safe_candidate_finder.h"
#include "core/block_manager.h"
#include "policy/eviction_policy.h"
#include "policy/feature_builders.h"
#include "prefix/prefix_lookup_engine.h"
#include "sim/decision_logger.h"
#include "sim/event_sink.h"
#include "tier/tier.h"

namespace kvcache::tier {
class TierManager;
}

namespace kvcache::controller {

struct MakeSpaceRequest {
    uint32_t blocks_needed;
    uint64_t timestamp_ns;
    Tier tier{Tier::GpuSim};  // which tier to evict from
};

struct EvictionResult {
    enum class Status { Success, InsufficientCandidates };
    Status status;
    uint32_t evicted_count;
};

class EvictionController {
public:
    // Phase 1/2 constructor — no TierManager.
    EvictionController(policy::EvictionPolicy& eviction_policy, BlockManager& manager,
                       prefix::PrefixLookupEngine& lookup_engine,
                       const SafeCandidateFinder& candidate_finder, sim::EventSink& events,
                       sim::DecisionLogger& decisions);

    // Phase 3 constructor — with TierManager for payload removal.
    EvictionController(policy::EvictionPolicy& eviction_policy, BlockManager& manager,
                       prefix::PrefixLookupEngine& lookup_engine,
                       const SafeCandidateFinder& candidate_finder,
                       tier::TierManager& tier_manager, sim::EventSink& events,
                       sim::DecisionLogger& decisions);

    [[nodiscard]] EvictionResult makeSpace(const MakeSpaceRequest& request);

private:
    policy::EvictionPolicy& eviction_policy_;
    BlockManager& manager_;
    prefix::PrefixLookupEngine& lookup_engine_;
    const SafeCandidateFinder& candidate_finder_;
    tier::TierManager* tier_manager_{nullptr};  // nullable; Phase 3 only
    sim::EventSink& events_;
    sim::DecisionLogger& decisions_;
    policy::BlockFeatureBuilder feature_builder_;
};

}  // namespace kvcache::controller
