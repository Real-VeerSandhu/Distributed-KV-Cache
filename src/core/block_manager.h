#pragma once

#include <cstdint>
#include <optional>
#include <utility>

#include "core/block_handle.h"
#include "core/block_pool.h"
#include "core/block_store.h"
#include "core/clock.h"

namespace kvcache {

struct FetchLocalResult {
    enum class Status { Hit, NotFound, StaleGeneration };
    Status status;
    std::optional<BlockHandle> handle;
};

class BlockManager {
public:
    BlockManager(uint32_t capacity, Clock& clock);

    [[nodiscard]] std::optional<std::pair<BlockId, uint64_t>> createBlock(
        ContentHash hash, uint32_t block_index, uint16_t token_count, Tier tier);

    [[nodiscard]] std::optional<BlockHandle> acquire(BlockId id);

    [[nodiscard]] FetchLocalResult fetchLocal(BlockId id, uint64_t generation);

    void markAccessed(BlockId id);

    void transitionState(BlockId id, BlockState from, BlockState to);

    void freeBlock(BlockId id);

    [[nodiscard]] const BlockStore& store() const noexcept;
    [[nodiscard]] uint32_t available() const noexcept;

private:
    BlockPool pool_;
    BlockStore store_;
    Clock& clock_;
    // TODO(threading): mutex for block lifecycle operations
};

}  // namespace kvcache
