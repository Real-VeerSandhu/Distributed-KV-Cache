#include "core/block_manager.h"

#include <stdexcept>

namespace kvcache {

namespace {

bool isLegalTransition(BlockState from, BlockState to) noexcept {
    switch (from) {
        case BlockState::Free:
            return to == BlockState::Admitting || to == BlockState::Fetching;
        case BlockState::Admitting:
            return to == BlockState::Ready || to == BlockState::Free;
        case BlockState::Fetching:
            return to == BlockState::Ready || to == BlockState::Free;
        case BlockState::Ready:
            return to == BlockState::Migrating || to == BlockState::Evicting;
        case BlockState::Migrating:
            return to == BlockState::Ready;
        case BlockState::Evicting:
            return to == BlockState::Free;
    }
    return false;
}

}  // namespace

BlockManager::BlockManager(uint32_t capacity, Clock& clock)
    : pool_(capacity), store_(capacity), clock_(clock) {}

std::optional<std::pair<BlockId, uint64_t>> BlockManager::createBlock(
    ContentHash hash, uint32_t block_index, uint16_t token_count, Tier tier) {
    // TODO(threading): lock pool_ + store_ atomically
    auto result = pool_.allocate();
    if (!result) {
        return std::nullopt;
    }
    const auto [id, generation] = *result;
    store_.initialize(id, generation, hash, block_index, token_count, tier,
                      BlockState::Admitting);
    return std::make_pair(id, generation);
}

std::optional<BlockHandle> BlockManager::acquire(BlockId id) {
    // TODO(threading): lock to check state + create handle atomically
    const auto slot = static_cast<size_t>(static_cast<uint64_t>(id));
    if (slot >= static_cast<size_t>(pool_.capacity())) {
        return std::nullopt;
    }
    const auto& rec = store_.get(id);
    if (rec.generation == 0 || rec.state != BlockState::Ready) {
        return std::nullopt;
    }
    store_.markAccessed(id, clock_.nowNs());
    return store_.createHandle(id);
}

FetchLocalResult BlockManager::fetchLocal(BlockId id, uint64_t generation) {
    const auto slot = static_cast<size_t>(static_cast<uint64_t>(id));
    if (slot >= static_cast<size_t>(pool_.capacity())) {
        return {FetchLocalResult::Status::NotFound, std::nullopt};
    }
    const auto& rec = store_.get(id);
    if (rec.generation == 0) {
        return {FetchLocalResult::Status::NotFound, std::nullopt};
    }
    if (rec.generation != generation) {
        return {FetchLocalResult::Status::StaleGeneration, std::nullopt};
    }
    if (rec.state != BlockState::Ready) {
        return {FetchLocalResult::Status::NotFound, std::nullopt};
    }
    store_.markAccessed(id, clock_.nowNs());
    return {FetchLocalResult::Status::Hit, store_.createHandle(id)};
}

void BlockManager::markAccessed(BlockId id) {
    store_.markAccessed(id, clock_.nowNs());
}

void BlockManager::transitionState(BlockId id, BlockState from, BlockState to) {
    const auto& rec = store_.get(id);
    if (rec.state != from) {
        throw std::logic_error("BlockManager::transitionState: unexpected current state");
    }
    if (!isLegalTransition(from, to)) {
        throw std::logic_error("BlockManager::transitionState: illegal state transition");
    }
    if (from == BlockState::Ready && to == BlockState::Evicting) {
        if (rec.refcount.load(std::memory_order_relaxed) > 0) {
            throw std::logic_error(
                "BlockManager::transitionState: cannot evict block with active handles");
        }
    }
    store_.setState(id, to);
    if (to == BlockState::Free) {
        pool_.free(id);
    }
}

void BlockManager::freeBlock(BlockId id) {
    transitionState(id, BlockState::Evicting, BlockState::Free);
}

BlockStore& BlockManager::store() noexcept { return store_; }
const BlockStore& BlockManager::store() const noexcept { return store_; }

uint32_t BlockManager::available() const noexcept { return pool_.available(); }

}  // namespace kvcache
