#include "prefix/prefix_lookup_engine.h"

#include "core/block_manager.h"

namespace kvcache::prefix {

PrefixLookupEngine::PrefixLookupEngine(PrefixIndex& index, kvcache::BlockManager& manager)
    : index_(index), manager_(manager) {}

PrefixLookupResult PrefixLookupEngine::lookup(kvcache::CacheContextHash ctx_hash,
                                               Span<const kvcache::TokenId> tokens,
                                               uint32_t block_size) const {
    PrefixLookupResult result;

    const PrefixIndexLookup raw = index_.lookup(ctx_hash, tokens);

    for (const kvcache::BlockId id : raw.blocks) {
        auto handle = manager_.acquire(id);
        if (!handle.has_value()) {
            break;
        }
        if (handle->metadata().token_count != static_cast<uint16_t>(block_size)) {
            break;
        }
        result.matched_tokens += block_size;
        result.handles.push_back(std::move(*handle));
    }

    return result;
}

std::vector<kvcache::BlockId> PrefixLookupEngine::rawLookupBlocks(
    kvcache::CacheContextHash ctx_hash, Span<const kvcache::TokenId> tokens) const {
    return index_.lookup(ctx_hash, tokens).blocks;
}

void PrefixLookupEngine::insertFullBlocks(kvcache::CacheContextHash ctx_hash,
                                           Span<const kvcache::TokenId> tokens,
                                           Span<const kvcache::BlockId> block_ids) {
    index_.insert(ctx_hash, tokens, block_ids);
}

void PrefixLookupEngine::removeBlock(kvcache::BlockId id) { index_.remove(id); }

const PrefixIndex& PrefixLookupEngine::index() const noexcept { return index_; }

}  // namespace kvcache::prefix
