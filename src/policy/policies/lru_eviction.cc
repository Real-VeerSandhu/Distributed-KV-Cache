#include "policy/policies/lru_eviction.h"

#include <algorithm>

namespace kvcache::policy {

std::optional<BlockId> LruEvictionPolicy::chooseVictim(
    const EvictionContext& /*context*/, Span<const BlockPolicyFeatures> candidates) {
    if (candidates.empty()) {
        return std::nullopt;
    }
    const auto it = std::min_element(
        candidates.begin(), candidates.end(),
        [](const BlockPolicyFeatures& a, const BlockPolicyFeatures& b) {
            return a.last_access_ns < b.last_access_ns;
        });
    return it->id;
}

const char* LruEvictionPolicy::name() const noexcept { return "lru"; }

}  // namespace kvcache::policy
