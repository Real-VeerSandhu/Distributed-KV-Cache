#include "core/local_cache.h"

#include <vector>

namespace kvcache {

LocalCache::LocalCache(uint32_t capacity, uint32_t block_size, Clock& clock)
    : block_size_(block_size),
      manager_(capacity, clock),
      prefix_index_(block_size),
      lookup_engine_(prefix_index_, manager_) {}

LocalLookupOutcome LocalCache::lookupPrefix(const CacheKeyContext& ctx,
                                             Span<const TokenId> tokens) {
    const CacheContextHash ctx_hash = computeContextHash(ctx);
    const uint32_t total_full =
        static_cast<uint32_t>(tokens.size() / static_cast<size_t>(block_size_));

    auto raw = lookup_engine_.lookup(ctx_hash, tokens, block_size_);

    LocalLookupOutcome outcome;
    outcome.matched_blocks = std::move(raw.handles);
    outcome.matched_tokens = raw.matched_tokens;
    outcome.miss_token_offset = raw.matched_tokens;
    outcome.context_hash = ctx_hash;
    outcome.total_full_blocks_in_request = total_full;
    return outcome;
}

AdmitResult LocalCache::admitBlock(const BlockCandidate& candidate) {
    const size_t expected =
        (static_cast<size_t>(candidate.block_index) + 1) * static_cast<size_t>(block_size_);
    if (candidate.tokens.size() != expected) {
        return {AdmitResult::Status::InvalidBlock, std::nullopt};
    }

    const CacheContextHash ctx_hash = computeContextHash(candidate.context);

    // Check whether this block position is already present in the index
    auto full_check = prefix_index_.lookup(
        ctx_hash,
        Span<const TokenId>{candidate.tokens.data(), candidate.tokens.size()});
    if (full_check.blocks.size() == static_cast<size_t>(candidate.block_index) + 1) {
        return {AdmitResult::Status::AlreadyPresent, std::nullopt};
    }

    // Look up existing blocks for the prefix leading to this block
    auto existing = prefix_index_.lookup(
        ctx_hash,
        Span<const TokenId>{candidate.tokens.data(),
                             static_cast<size_t>(candidate.block_index) * block_size_});
    if (existing.blocks.size() != static_cast<size_t>(candidate.block_index)) {
        return {AdmitResult::Status::InvalidBlock, std::nullopt};
    }

    auto slot =
        manager_.createBlock(candidate.hash, candidate.block_index,
                              static_cast<uint16_t>(block_size_), Tier::GpuSim);
    if (!slot) {
        return {AdmitResult::Status::CapacityExceeded, std::nullopt};
    }

    const auto [id, generation] = *slot;
    (void)generation;

    std::vector<BlockId> all_blocks = existing.blocks;
    all_blocks.push_back(id);

    prefix_index_.insert(
        ctx_hash,
        Span<const TokenId>{candidate.tokens.data(), candidate.tokens.size()},
        Span<const BlockId>{all_blocks.data(), all_blocks.size()});

    manager_.transitionState(id, BlockState::Admitting, BlockState::Ready);

    return {AdmitResult::Status::Admitted, manager_.acquire(id)};
}

FetchLocalResult LocalCache::getBlock(BlockId id, uint64_t generation) {
    return manager_.fetchLocal(id, generation);
}

CacheSnapshot LocalCache::snapshot() const {
    const auto recs = manager_.store().records();
    uint32_t used = 0;
    uint32_t ready = 0;
    for (size_t i = 0; i < recs.size(); ++i) {
        if (recs[i].generation > 0 && recs[i].state != BlockState::Free) {
            ++used;
        }
        if (recs[i].state == BlockState::Ready) {
            ++ready;
        }
    }
    return {static_cast<uint32_t>(recs.size()), used, ready};
}

}  // namespace kvcache
