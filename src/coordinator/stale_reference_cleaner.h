#pragma once

#include "coordinator/global_index.h"
#include "core/ids.h"

namespace kvcache::coordinator {

// Removes stale GlobalIndex entries when fetches fail or nodes leave.
class StaleReferenceCleaner {
public:
    explicit StaleReferenceCleaner(GlobalIndex& index) noexcept;

    void reportStaleFetch(NodeId node, BlockId block_id, uint64_t generation);
    void cleanDeadNode(NodeId node);

private:
    GlobalIndex& index_;
};

}  // namespace kvcache::coordinator
