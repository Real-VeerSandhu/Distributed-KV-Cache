#include "coordinator/global_index.h"

#include <algorithm>

namespace kvcache::coordinator {

void GlobalIndex::announce(ContentHash hash, GlobalBlockRef ref) {
    auto& refs = index_[hash];
    for (const auto& existing : refs) {
        if (existing.node_id == ref.node_id && existing.block_id == ref.block_id &&
            existing.generation == ref.generation) {
            return;
        }
    }
    refs.push_back(ref);
}

std::vector<GlobalBlockRef> GlobalIndex::query(ContentHash hash) const {
    const auto it = index_.find(hash);
    if (it == index_.end()) return {};
    return it->second;
}

void GlobalIndex::invalidate(NodeId node, BlockId block_id, uint64_t generation) {
    for (auto& [hash, refs] : index_) {
        refs.erase(std::remove_if(refs.begin(), refs.end(),
                                  [&](const GlobalBlockRef& r) {
                                      return r.node_id == node && r.block_id == block_id &&
                                             r.generation == generation;
                                  }),
                   refs.end());
    }
}

void GlobalIndex::removeNode(NodeId node) {
    for (auto& [hash, refs] : index_) {
        refs.erase(std::remove_if(refs.begin(), refs.end(),
                                  [&](const GlobalBlockRef& r) { return r.node_id == node; }),
                   refs.end());
    }
}

std::size_t GlobalIndex::entryCount() const noexcept {
    std::size_t total = 0;
    for (const auto& [hash, refs] : index_) {
        total += refs.size();
    }
    return total;
}

}  // namespace kvcache::coordinator
