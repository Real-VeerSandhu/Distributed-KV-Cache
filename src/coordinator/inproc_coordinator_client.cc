#include "coordinator/inproc_coordinator_client.h"

namespace kvcache::coordinator {

InprocCoordinatorClient::InprocCoordinatorClient(CoordinatorService& service) noexcept
    : service_(service) {}

CoordinatorQueryResult InprocCoordinatorClient::query(ContentHash hash) {
    return {service_.query(hash), true};
}

void InprocCoordinatorClient::announce(ContentHash hash, GlobalBlockRef ref) {
    service_.announce(hash, ref);
}

void InprocCoordinatorClient::invalidate(NodeId node, BlockId block_id, uint64_t generation) {
    service_.invalidate(node, block_id, generation);
}

}  // namespace kvcache::coordinator
