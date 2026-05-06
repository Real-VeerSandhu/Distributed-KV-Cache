#include "policy/policies/no_prefetch.h"

namespace kvcache::policy {

std::vector<ContentHash> NoPrefetchPolicy::planPrefetch(const PrefetchContext& /*context*/) {
    return {};
}

const char* NoPrefetchPolicy::name() const noexcept { return "none"; }

}  // namespace kvcache::policy
