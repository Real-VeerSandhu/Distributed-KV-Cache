#pragma once

#include <vector>

#include "core/block_handle.h"
#include "core/ids.h"
#include "core/ids.h"
#include "core/span.h"
#include "prefix/prefix_index.h"

namespace kvcache {
class BlockManager;
}

namespace kvcache::prefix {

struct PrefixLookupResult {
    std::vector<kvcache::BlockHandle> handles;
    uint32_t matched_tokens{0};
};

class PrefixLookupEngine {
public:
    PrefixLookupEngine(PrefixIndex& index, kvcache::BlockManager& manager);

    [[nodiscard]] PrefixLookupResult lookup(kvcache::CacheContextHash ctx_hash,
                                            Span<const kvcache::TokenId> tokens,
                                            uint32_t block_size) const;

    [[nodiscard]] std::vector<kvcache::BlockId> rawLookupBlocks(
        kvcache::CacheContextHash ctx_hash, Span<const kvcache::TokenId> tokens) const;

    void insertFullBlocks(kvcache::CacheContextHash ctx_hash, Span<const kvcache::TokenId> tokens,
                          Span<const kvcache::BlockId> block_ids);

    void removeBlock(kvcache::BlockId id);

    [[nodiscard]] const PrefixIndex& index() const noexcept;

private:
    PrefixIndex& index_;
    kvcache::BlockManager& manager_;
};

}  // namespace kvcache::prefix
