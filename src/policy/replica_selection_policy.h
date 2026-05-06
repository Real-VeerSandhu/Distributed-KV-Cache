#pragma once

#include <optional>

#include "core/ids.h"
#include "core/span.h"

namespace kvcache::policy {

struct ReplicaSelectionContext {
    ContentHash target_hash;
    NodeId requesting_node;
    uint64_t timestamp_ns;
};

class ReplicaSelectionPolicy {
public:
    virtual ~ReplicaSelectionPolicy() = default;

    ReplicaSelectionPolicy(const ReplicaSelectionPolicy&) = delete;
    ReplicaSelectionPolicy& operator=(const ReplicaSelectionPolicy&) = delete;

    [[nodiscard]] virtual std::optional<GlobalBlockRef> chooseReplica(
        const ReplicaSelectionContext& context, Span<const GlobalBlockRef> replicas) = 0;

    [[nodiscard]] virtual const char* name() const noexcept = 0;

protected:
    ReplicaSelectionPolicy() = default;
};

}  // namespace kvcache::policy
