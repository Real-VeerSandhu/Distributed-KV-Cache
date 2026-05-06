#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

#include "core/cache_key.h"
#include "core/ids.h"

namespace kvcache {

// tokens holds the full token sequence from position 0 through the end of this block:
// tokens.size() == (block_index + 1) * context.block_size
struct BlockCandidate {
    CacheKeyContext context;
    uint32_t block_index;
    std::vector<TokenId> tokens;
    ContentHash hash;
    std::vector<std::byte> payload;
    BlockOrigin origin;
    std::optional<GlobalBlockRef> source_ref;
};

}  // namespace kvcache
