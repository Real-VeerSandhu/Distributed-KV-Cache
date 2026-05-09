#include "policy/policies/gpu_first_placement.h"

namespace kvcache::policy {

Tier GpuFirstPlacementPolicy::chooseTier(const PlacementContext& context,
                                          const BlockCandidateFeatures& /*candidate*/) {
    const bool gpu_has_space = context.gpu_sim_used_blocks < context.gpu_sim_capacity_blocks;
    return gpu_has_space ? Tier::GpuSim : Tier::Host;
}

const char* GpuFirstPlacementPolicy::name() const noexcept { return "gpu_first"; }

}  // namespace kvcache::policy
