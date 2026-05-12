#pragma once

#include <cstddef>
#include <unordered_map>
#include <vector>

#include "core/ids.h"

namespace kvcache::coordinator {

// Maps content hashes to the set of workers that may hold a copy of that block.
// Does not enforce local cache correctness — only tracks possible locations.
class GlobalIndex {
public:
    GlobalIndex() = default;

    void announce(ContentHash hash, GlobalBlockRef ref);
    [[nodiscard]] std::vector<GlobalBlockRef> query(ContentHash hash) const;
    void invalidate(NodeId node, BlockId block_id, uint64_t generation);
    void removeNode(NodeId node);

    [[nodiscard]] std::size_t entryCount() const noexcept;

private:
    std::unordered_map<ContentHash, std::vector<GlobalBlockRef>> index_;
    // TODO(threading): shared_mutex for concurrent reads
};

}  // namespace kvcache::coordinator
