#pragma once

#include "core/cache_key.h"
#include "core/ids.h"
#include "core/span.h"

namespace kvcache {

[[nodiscard]] CacheContextHash computeContextHash(const CacheKeyContext& ctx) noexcept;

[[nodiscard]] ContentHash computeContentHash(CacheContextHash context_hash,
                                             uint32_t block_index,
                                             uint16_t token_count,
                                             Span<const TokenId> tokens) noexcept;

}  // namespace kvcache
