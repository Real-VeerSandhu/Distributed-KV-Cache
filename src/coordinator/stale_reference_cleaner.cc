#include "coordinator/stale_reference_cleaner.h"

namespace kvcache::coordinator {

StaleReferenceCleaner::StaleReferenceCleaner(GlobalIndex& index) noexcept : index_(index) {}

void StaleReferenceCleaner::reportStaleFetch(NodeId node, BlockId block_id,
                                              uint64_t generation) {
    index_.invalidate(node, block_id, generation);
}

void StaleReferenceCleaner::cleanDeadNode(NodeId node) { index_.removeNode(node); }

}  // namespace kvcache::coordinator
