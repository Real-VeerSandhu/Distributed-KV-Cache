#include "core/block_store.h"

#include <stdexcept>

#include "core/block_handle.h"

namespace kvcache {

namespace {
size_t slotOf(BlockId id) noexcept {
    return static_cast<size_t>(static_cast<uint64_t>(id));
}
}  // namespace

BlockStore::BlockStore(uint32_t capacity)
    : capacity_(capacity), records_(std::make_unique<BlockRecord[]>(capacity)) {}

bool BlockStore::exists(BlockId id, uint64_t generation) const noexcept {
    const size_t slot = slotOf(id);
    if (slot >= static_cast<size_t>(capacity_)) {
        return false;
    }
    return records_[slot].generation == generation;
}

const BlockRecord& BlockStore::get(BlockId id) const {
    const size_t slot = slotOf(id);
    if (slot >= static_cast<size_t>(capacity_)) {
        throw std::logic_error("BlockStore::get: slot out of range");
    }
    return records_[slot];
}

Span<const BlockRecord> BlockStore::records() const noexcept {
    return Span<const BlockRecord>(records_.get(), static_cast<size_t>(capacity_));
}

void BlockStore::initialize(BlockId id, uint64_t generation, ContentHash hash,
                             uint32_t block_index, uint16_t token_count, Tier tier,
                             BlockState initial_state) {
    const size_t slot = slotOf(id);
    if (slot >= static_cast<size_t>(capacity_)) {
        throw std::logic_error("BlockStore::initialize: slot out of range");
    }
    auto& rec = records_[slot];
    rec.id = id;
    rec.generation = generation;
    rec.hash = hash;
    rec.block_index = block_index;
    rec.token_count = token_count;
    rec.tier = tier;
    rec.refcount.store(0, std::memory_order_relaxed);
    rec.last_access_ns = 0;
    rec.access_count = 0;
    rec.payload = tier::PayloadRef{};
    rec.state = initial_state;
}

void BlockStore::setState(BlockId id, BlockState state) {
    const size_t slot = slotOf(id);
    if (slot >= static_cast<size_t>(capacity_)) {
        throw std::logic_error("BlockStore::setState: slot out of range");
    }
    records_[slot].state = state;
}

void BlockStore::setTier(BlockId id, Tier tier) {
    const size_t slot = slotOf(id);
    if (slot >= static_cast<size_t>(capacity_)) {
        throw std::logic_error("BlockStore::setTier: slot out of range");
    }
    records_[slot].tier = tier;
}

void BlockStore::setPayload(BlockId id, tier::PayloadRef payload) {
    const size_t slot = slotOf(id);
    if (slot >= static_cast<size_t>(capacity_)) {
        throw std::logic_error("BlockStore::setPayload: slot out of range");
    }
    records_[slot].payload = payload;
}

void BlockStore::markAccessed(BlockId id, uint64_t now_ns) {
    const size_t slot = slotOf(id);
    if (slot >= static_cast<size_t>(capacity_)) {
        throw std::logic_error("BlockStore::markAccessed: slot out of range");
    }
    auto& rec = records_[slot];
    rec.last_access_ns = now_ns;
    ++rec.access_count;
}

BlockHandle BlockStore::createHandle(BlockId id) {
    const size_t slot = slotOf(id);
    if (slot >= static_cast<size_t>(capacity_)) {
        throw std::logic_error("BlockStore::createHandle: slot out of range");
    }
    return BlockHandle{&records_[slot]};
}

}  // namespace kvcache
