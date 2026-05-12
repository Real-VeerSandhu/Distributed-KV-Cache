#include "coordinator/coordinator_service.h"

namespace kvcache::coordinator {

CoordinatorService::CoordinatorService(GlobalIndex& index, WorkerDirectory& dir,
                                        StaleReferenceCleaner& cleaner) noexcept
    : index_(index), dir_(dir), cleaner_(cleaner) {}

void CoordinatorService::announce(ContentHash hash, GlobalBlockRef ref) {
    index_.announce(hash, ref);
}

std::vector<GlobalBlockRef> CoordinatorService::query(ContentHash hash) const {
    return index_.query(hash);
}

void CoordinatorService::invalidate(NodeId node, BlockId block_id, uint64_t generation) {
    cleaner_.reportStaleFetch(node, block_id, generation);
}

void CoordinatorService::nodeJoin(WorkerInfo info) { dir_.nodeJoin(std::move(info)); }

void CoordinatorService::nodeLeave(NodeId node) {
    cleaner_.cleanDeadNode(node);
    dir_.nodeLeave(node);
}

const WorkerDirectory& CoordinatorService::directory() const noexcept { return dir_; }

const GlobalIndex& CoordinatorService::index() const noexcept { return index_; }

}  // namespace kvcache::coordinator
