#include "policy/policies/no_eviction.h"

namespace kvcache::policy {

std::optional<BlockId> NoEvictionPolicy::chooseVictim(const EvictionContext& /*context*/,
                                                        Span<const BlockPolicyFeatures> /*candidates*/) {
    return std::nullopt;
}

const char* NoEvictionPolicy::name() const noexcept { return "no_eviction"; }

}  // namespace kvcache::policy
