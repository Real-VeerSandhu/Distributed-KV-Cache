#include "policy/policies/prefix_aware_eviction.h"

namespace kvcache::policy {

namespace {
constexpr double NS_PER_SEC = 1e9;
}

double PrefixAwareEvictionPolicy::score(const BlockPolicyFeatures& f) {
    const double age_pressure = static_cast<double>(f.age_ns) / NS_PER_SEC;
    return static_cast<double>(f.access_count) +
           2.0 * static_cast<double>(f.prefix_child_count) +
           4.0 * (f.is_shared_prefix ? 1.0 : 0.0) +
           2.0 * static_cast<double>(f.descendant_block_count) - age_pressure;
}

std::optional<BlockId> PrefixAwareEvictionPolicy::chooseVictim(
    const EvictionContext& /*context*/, Span<const BlockPolicyFeatures> candidates) {
    if (candidates.empty()) {
        return std::nullopt;
    }
    const BlockPolicyFeatures* best = &candidates[0];
    double best_score = score(*best);
    for (std::size_t i = 1; i < candidates.size(); ++i) {
        const double s = score(candidates[i]);
        if (s < best_score) {
            best_score = s;
            best = &candidates[i];
        }
    }
    return best->id;
}

const char* PrefixAwareEvictionPolicy::name() const noexcept { return "prefix_aware"; }

}  // namespace kvcache::policy
