#pragma once

#include <cstdint>
#include <unordered_set>

#include "core/ids.h"

namespace kvcache::tier {

struct TierStats {
    uint32_t capacity_blocks;
    uint32_t used_blocks;
};

// Tracks which blocks are physically present in one memory tier.
// Does not own payload bytes; does not choose placement policy.
class TierStore {
public:
    explicit TierStore(uint32_t capacity_blocks);
    TierStore(TierStore&&) = default;
    TierStore& operator=(TierStore&&) = default;

    [[nodiscard]] bool hasCapacity() const noexcept;
    [[nodiscard]] bool contains(BlockId id) const noexcept;

    void add(BlockId id);
    void remove(BlockId id);

    [[nodiscard]] TierStats stats() const noexcept;
    [[nodiscard]] const std::unordered_set<BlockId>& blockIds() const noexcept;

private:
    uint32_t capacity_blocks_;
    std::unordered_set<BlockId> block_ids_;
    // TODO(threading): per-tier mutex
};

}  // namespace kvcache::tier
