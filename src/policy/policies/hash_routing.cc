#include "policy/policies/hash_routing.h"

#include <stdexcept>

namespace kvcache::policy {

NodeId HashFirstBlockRoutingPolicy::chooseNode(const RoutingContext& context,
                                                Span<const NodePolicyFeatures> nodes) {
    if (nodes.empty()) throw std::logic_error("HashFirstBlockRoutingPolicy: no nodes");
    const std::size_t idx = context.first_block_hash.lo % nodes.size();
    return nodes[idx].node_id;
}

const char* HashFirstBlockRoutingPolicy::name() const noexcept { return "hash_first_block"; }

}  // namespace kvcache::policy
