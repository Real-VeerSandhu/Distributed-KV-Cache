#pragma once

#include "core/ids.h"
#include "core/span.h"
#include "policy/policy_features.h"

namespace kvcache::policy {

struct RoutingContext {
    CacheContextHash context_hash;
    uint32_t full_block_count;
    ContentHash first_block_hash;
    uint64_t timestamp_ns;
};

class RoutingPolicy {
public:
    virtual ~RoutingPolicy() = default;

    RoutingPolicy(const RoutingPolicy&) = delete;
    RoutingPolicy& operator=(const RoutingPolicy&) = delete;

    [[nodiscard]] virtual NodeId chooseNode(const RoutingContext& context,
                                            Span<const NodePolicyFeatures> nodes) = 0;

    [[nodiscard]] virtual const char* name() const noexcept = 0;

protected:
    RoutingPolicy() = default;
};

}  // namespace kvcache::policy
