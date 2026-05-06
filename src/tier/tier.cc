#include "tier/tier.h"

namespace kvcache {

std::string_view tier_name(Tier tier) noexcept {
    switch (tier) {
        case Tier::GpuSim: return "GPU_SIM";
        case Tier::Host:   return "HOST";
    }
    return "UNKNOWN";
}

}  // namespace kvcache
