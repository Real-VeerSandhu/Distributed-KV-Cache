#pragma once

#include <cstdint>
#include <optional>

#include "controller/eviction_controller.h"
#include "core/block_candidate.h"
#include "core/block_handle.h"
#include "core/block_manager.h"
#include "core/ids.h"
#include "policy/admission_policy.h"
#include "prefix/prefix_lookup_engine.h"
#include "sim/decision_logger.h"
#include "sim/event_sink.h"

namespace kvcache::policy {
class TierPlacementPolicy;
}

namespace kvcache::tier {
class TierManager;
}

namespace kvcache::controller {

struct AdmitResult {
    enum class Status { Admitted, AlreadyPresent, CapacityExceeded, InvalidBlock, Rejected };
    Status status;
    std::optional<BlockHandle> handle;
};

class AdmissionController {
public:
    // Phase 1/2 constructor — no tier management, always places in GpuSim.
    AdmissionController(policy::AdmissionPolicy& admission_policy,
                        EvictionController& eviction_controller, BlockManager& manager,
                        prefix::PrefixLookupEngine& lookup_engine, uint32_t block_size,
                        sim::EventSink& events, sim::DecisionLogger& decisions);

    // Phase 3 constructor — uses TierPlacementPolicy and TierManager.
    AdmissionController(policy::AdmissionPolicy& admission_policy,
                        policy::TierPlacementPolicy& placement_policy,
                        tier::TierManager& tier_manager,
                        EvictionController& eviction_controller, BlockManager& manager,
                        prefix::PrefixLookupEngine& lookup_engine, uint32_t block_size,
                        sim::EventSink& events, sim::DecisionLogger& decisions);

    [[nodiscard]] AdmitResult admit(const BlockCandidate& candidate,
                                    const policy::AdmissionContext& ctx);

private:
    policy::AdmissionPolicy& admission_policy_;
    policy::TierPlacementPolicy* placement_policy_{nullptr};  // nullable; Phase 3 only
    tier::TierManager* tier_manager_{nullptr};                // nullable; Phase 3 only
    EvictionController& eviction_controller_;
    BlockManager& manager_;
    prefix::PrefixLookupEngine& lookup_engine_;
    uint32_t block_size_;
    sim::EventSink& events_;
    sim::DecisionLogger& decisions_;
};

}  // namespace kvcache::controller
