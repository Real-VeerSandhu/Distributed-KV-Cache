#include "tier/tier_store.h"

#include <stdexcept>

namespace kvcache::tier {

TierStore::TierStore(uint32_t capacity_blocks) : capacity_blocks_(capacity_blocks) {}

bool TierStore::hasCapacity() const noexcept {
    return static_cast<uint32_t>(block_ids_.size()) < capacity_blocks_;
}

bool TierStore::contains(BlockId id) const noexcept { return block_ids_.count(id) > 0; }

void TierStore::add(BlockId id) {
    if (!hasCapacity()) {
        throw std::logic_error("TierStore::add: tier at capacity");
    }
    block_ids_.insert(id);
}

void TierStore::remove(BlockId id) { block_ids_.erase(id); }

TierStats TierStore::stats() const noexcept {
    return {capacity_blocks_, static_cast<uint32_t>(block_ids_.size())};
}

const std::unordered_set<BlockId>& TierStore::blockIds() const noexcept { return block_ids_; }

}  // namespace kvcache::tier
