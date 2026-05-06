#pragma once

#include "policy/replica_selection_policy.h"

namespace kvcache::policy {

class FirstReplicaPolicy : public ReplicaSelectionPolicy {
public:
    FirstReplicaPolicy() = default;

    [[nodiscard]] std::optional<GlobalBlockRef> chooseReplica(
        const ReplicaSelectionContext& context, Span<const GlobalBlockRef> replicas) override;

    [[nodiscard]] const char* name() const noexcept override;
};

}  // namespace kvcache::policy
