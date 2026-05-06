#pragma once

#include <random>

#include "policy/routing_policy.h"

namespace kvcache::policy {

class RandomRoutingPolicy : public RoutingPolicy {
public:
    explicit RandomRoutingPolicy(uint32_t seed = std::random_device{}());

    [[nodiscard]] NodeId chooseNode(const RoutingContext& context,
                                    Span<const NodePolicyFeatures> nodes) override;

    [[nodiscard]] const char* name() const noexcept override;

private:
    std::mt19937 rng_;
};

}  // namespace kvcache::policy
