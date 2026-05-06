#include "policy/policies/first_replica.h"

namespace kvcache::policy {

std::optional<GlobalBlockRef> FirstReplicaPolicy::chooseReplica(
    const ReplicaSelectionContext& /*context*/, Span<const GlobalBlockRef> replicas) {
    if (replicas.empty()) {
        return std::nullopt;
    }
    return replicas[0];
}

const char* FirstReplicaPolicy::name() const noexcept { return "first_replica"; }

}  // namespace kvcache::policy
