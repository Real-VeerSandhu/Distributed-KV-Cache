#include "core/block_pool.h"

#include <stdexcept>

namespace kvcache {

namespace {
size_t slotOf(BlockId id) noexcept {
    return static_cast<size_t>(static_cast<uint64_t>(id));
}
}  // namespace

BlockPool::BlockPool(uint32_t capacity)
    : capacity_(capacity), generations_(capacity, 0), in_use_(capacity, false) {
    free_list_.reserve(capacity);
    for (uint32_t i = capacity; i > 0; --i) {
        free_list_.push_back(BlockId{i - 1});
    }
}

std::optional<std::pair<BlockId, uint64_t>> BlockPool::allocate() {
    // TODO(threading): lock free_list_
    if (free_list_.empty()) {
        return std::nullopt;
    }
    const BlockId id = free_list_.back();
    free_list_.pop_back();
    const size_t slot = slotOf(id);
    if (generations_[slot] == 0) {
        generations_[slot] = 1;
    }
    in_use_[slot] = true;
    return std::make_pair(id, generations_[slot]);
}

void BlockPool::free(BlockId id) {
    // TODO(threading): lock free_list_ and in_use_
    const size_t slot = slotOf(id);
    if (slot >= static_cast<size_t>(capacity_) || !in_use_[slot]) {
        throw std::logic_error("BlockPool::free: double-free or invalid BlockId");
    }
    in_use_[slot] = false;
    ++generations_[slot];
    free_list_.push_back(id);
}

uint32_t BlockPool::capacity() const noexcept {
    return capacity_;
}

uint32_t BlockPool::available() const noexcept {
    return static_cast<uint32_t>(free_list_.size());
}

}  // namespace kvcache
