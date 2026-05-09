#include "tier/payload_store.h"

#include <algorithm>
#include <stdexcept>

namespace kvcache::tier {

PayloadStore::PayloadStore(uint32_t max_blocks, uint64_t bytes_per_block)
    : max_blocks_(max_blocks), bytes_per_block_(bytes_per_block) {
    if (bytes_per_block_ > 0) {
        buffers_.resize(max_blocks_,
                        std::vector<std::byte>(static_cast<size_t>(bytes_per_block_), std::byte{0}));
        free_list_.resize(max_blocks_);
        for (uint32_t i = 0; i < max_blocks_; ++i) {
            free_list_[i] = max_blocks_ - 1 - i;  // push in reverse so pop_back gives 0 first
        }
    }
}

PayloadRef PayloadStore::put(Span<const std::byte> bytes) {
    if (bytes_per_block_ == 0) {
        return PayloadRef{PayloadRef::INVALID};
    }
    if (free_list_.empty()) {
        throw std::runtime_error("PayloadStore::put: no free slots");
    }
    const uint32_t slot = free_list_.back();
    free_list_.pop_back();
    const size_t copy_len = std::min(bytes.size(), static_cast<size_t>(bytes_per_block_));
    if (copy_len > 0) {
        std::copy_n(bytes.data(), copy_len, buffers_[slot].data());
    }
    return PayloadRef{static_cast<uint64_t>(slot)};
}

Span<const std::byte> PayloadStore::get(PayloadRef ref) const {
    if (!ref.valid() || bytes_per_block_ == 0) {
        return Span<const std::byte>{};
    }
    const size_t slot = static_cast<size_t>(ref.index);
    if (slot >= buffers_.size()) {
        throw std::logic_error("PayloadStore::get: invalid PayloadRef index");
    }
    return Span<const std::byte>{buffers_[slot].data(), buffers_[slot].size()};
}

void PayloadStore::remove(PayloadRef ref) {
    if (!ref.valid() || bytes_per_block_ == 0) {
        return;
    }
    const size_t slot = static_cast<size_t>(ref.index);
    if (slot >= buffers_.size()) {
        throw std::logic_error("PayloadStore::remove: invalid PayloadRef index");
    }
    free_list_.push_back(static_cast<uint32_t>(slot));
}

uint64_t PayloadStore::bytesUsed() const noexcept {
    if (bytes_per_block_ == 0) {
        return 0;
    }
    const uint64_t used = static_cast<uint64_t>(max_blocks_ - free_list_.size());
    return used * bytes_per_block_;
}

uint64_t PayloadStore::bytesPerBlock() const noexcept { return bytes_per_block_; }

}  // namespace kvcache::tier
