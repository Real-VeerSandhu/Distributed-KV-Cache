#include "policy/policies/random_routing.h"

namespace kvcache::policy {

RandomRoutingPolicy::RandomRoutingPolicy(uint32_t seed) : rng_(seed) {}

NodeId RandomRoutingPolicy::chooseNode(const RoutingContext& /*context*/,
                                        Span<const NodePolicyFeatures> nodes) {
    if (nodes.empty()) {
        return NodeId{0};
    }
    if (nodes.size() == 1) {
        return nodes[0].node_id;
    }
    std::uniform_int_distribution<std::size_t> dist(0, nodes.size() - 1);
    return nodes[dist(rng_)].node_id;
}

const char* RandomRoutingPolicy::name() const noexcept { return "random"; }

}  // namespace kvcache::policy
