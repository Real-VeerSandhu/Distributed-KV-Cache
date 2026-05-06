#pragma once

#include <cstdint>
#include <limits>
#include <optional>
#include <unordered_map>
#include <vector>

#include "core/ids.h"
#include "core/span.h"

namespace kvcache::prefix {

struct PrefixIndexLookup {
    std::vector<kvcache::BlockId> blocks;
    uint32_t matched_tokens{0};
};

class PrefixIndex {
public:
    explicit PrefixIndex(uint32_t block_size);

    [[nodiscard]] PrefixIndexLookup lookup(kvcache::CacheContextHash ctx_hash,
                                           Span<const kvcache::TokenId> tokens) const;

    void insert(kvcache::CacheContextHash ctx_hash, Span<const kvcache::TokenId> tokens,
                Span<const kvcache::BlockId> blocks);

    void remove(kvcache::BlockId id);

private:
    static constexpr uint32_t INVALID_NODE = std::numeric_limits<uint32_t>::max();

    struct Node {
        std::vector<kvcache::TokenId> edge_tokens;
        std::optional<kvcache::BlockId> terminal_block;
        std::unordered_map<kvcache::TokenId, uint32_t> children;
        uint32_t parent_idx{INVALID_NODE};
        kvcache::TokenId first_edge_token{0};
    };

    uint32_t block_size_;
    std::vector<Node> nodes_;
    std::unordered_map<kvcache::CacheContextHash, uint32_t> namespace_roots_;
    std::unordered_map<kvcache::BlockId, uint32_t> block_node_index_;

    uint32_t newNode();
    uint32_t getOrCreateRoot(kvcache::CacheContextHash ctx_hash);
    void pruneIfEmpty(uint32_t node_idx);
};

}  // namespace kvcache::prefix
