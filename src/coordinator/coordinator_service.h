#pragma once

#include <vector>

#include "coordinator/global_index.h"
#include "coordinator/stale_reference_cleaner.h"
#include "coordinator/worker_directory.h"
#include "core/ids.h"

namespace kvcache::coordinator {

class CoordinatorService {
public:
    CoordinatorService(GlobalIndex& index, WorkerDirectory& dir,
                       StaleReferenceCleaner& cleaner) noexcept;

    void announce(ContentHash hash, GlobalBlockRef ref);
    [[nodiscard]] std::vector<GlobalBlockRef> query(ContentHash hash) const;
    void invalidate(NodeId node, BlockId block_id, uint64_t generation);
    void nodeJoin(WorkerInfo info);
    void nodeLeave(NodeId node);

    [[nodiscard]] const WorkerDirectory& directory() const noexcept;
    [[nodiscard]] const GlobalIndex& index() const noexcept;

private:
    GlobalIndex& index_;
    WorkerDirectory& dir_;
    StaleReferenceCleaner& cleaner_;
};

}  // namespace kvcache::coordinator
