#include "controller/safe_candidate_finder.h"

namespace kvcache::controller {

SafeCandidateFinder::SafeCandidateFinder(const BlockStore& store) : store_(store) {}

std::vector<BlockId> SafeCandidateFinder::find() const {
    std::vector<BlockId> candidates;
    for (const auto& rec : store_.records()) {
        if (rec.state == BlockState::Ready &&
            rec.refcount.load(std::memory_order_relaxed) == 0) {
            candidates.push_back(rec.id);
        }
    }
    return candidates;
}

}  // namespace kvcache::controller
