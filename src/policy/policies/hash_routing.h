#pragma once

#include "policy/routing_policy.h"

namespace kvcache::policy {

// Routes by hashing the first full block. Same prefix always goes to the same node.
class HashFirstBlockRoutingPolicy : public RoutingPolicy {
public:
    HashFirstBlockRoutingPolicy() = default;

    [[nodiscard]] NodeId chooseNode(const RoutingContext& context,
                                    Span<const NodePolicyFeatures> nodes) override;

    [[nodiscard]] const char* name() const noexcept override;
};

}  // namespace kvcache::policy
