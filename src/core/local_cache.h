#pragma once

#include <cstdint>
#include <optional>
#include <vector>

#include "core/block_candidate.h"
#include "core/block_handle.h"
#include "core/block_manager.h"
#include "core/clock.h"
#include "core/content_hash.h"
#include "core/ids.h"
#include "core/span.h"
#include "prefix/prefix_index.h"
#include "prefix/prefix_lookup_engine.h"

namespace kvcache {

struct LocalLookupOutcome {
    std::vector<BlockHandle> matched_blocks;
    uint32_t matched_tokens{0};
    uint32_t miss_token_offset{0};
    CacheContextHash context_hash{0};
    uint32_t total_full_blocks_in_request{0};
};

struct AdmitResult {
    enum class Status { Admitted, AlreadyPresent, CapacityExceeded, InvalidBlock };
    Status status;
    std::optional<BlockHandle> handle;
};

struct CacheSnapshot {
    uint32_t total_capacity_blocks{0};
    uint32_t used_blocks{0};
    uint32_t ready_blocks{0};
};

class LocalCache {
public:
    LocalCache(uint32_t capacity, uint32_t block_size, Clock& clock);

    [[nodiscard]] LocalLookupOutcome lookupPrefix(const CacheKeyContext& ctx,
                                                   Span<const TokenId> tokens);

    [[nodiscard]] AdmitResult admitBlock(const BlockCandidate& candidate);

    [[nodiscard]] FetchLocalResult getBlock(BlockId id, uint64_t generation);

    [[nodiscard]] CacheSnapshot snapshot() const;

private:
    uint32_t block_size_;
    BlockManager manager_;
    prefix::PrefixIndex prefix_index_;
    prefix::PrefixLookupEngine lookup_engine_;
};

}  // namespace kvcache
