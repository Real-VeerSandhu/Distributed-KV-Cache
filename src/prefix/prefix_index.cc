#include "prefix/prefix_index.h"

#include <algorithm>
#include <stdexcept>

namespace kvcache::prefix {

PrefixIndex::PrefixIndex(uint32_t block_size) : block_size_(block_size) {}

uint32_t PrefixIndex::newNode() {
    nodes_.emplace_back();
    return static_cast<uint32_t>(nodes_.size() - 1);
}

uint32_t PrefixIndex::getOrCreateRoot(kvcache::CacheContextHash ctx_hash) {
    auto it = namespace_roots_.find(ctx_hash);
    if (it != namespace_roots_.end()) {
        return it->second;
    }
    const uint32_t root = newNode();
    namespace_roots_[ctx_hash] = root;
    return root;
}

PrefixIndexLookup PrefixIndex::lookup(kvcache::CacheContextHash ctx_hash,
                                      Span<const kvcache::TokenId> tokens) const {
    PrefixIndexLookup result;

    if (tokens.empty()) {
        return result;
    }

    auto it = namespace_roots_.find(ctx_hash);
    if (it == namespace_roots_.end()) {
        return result;
    }

    uint32_t node_idx = it->second;
    size_t pos = 0;

    while (pos < tokens.size()) {
        const kvcache::TokenId first = tokens[pos];
        const auto child_it = nodes_[node_idx].children.find(first);
        if (child_it == nodes_[node_idx].children.end()) {
            break;
        }

        const uint32_t child_idx = child_it->second;
        const auto& edge = nodes_[child_idx].edge_tokens;

        const size_t avail = tokens.size() - pos;
        if (avail < edge.size()) {
            break;
        }

        bool mismatch = false;
        for (size_t i = 0; i < edge.size(); ++i) {
            if (tokens[pos + i] != edge[i]) {
                mismatch = true;
                break;
            }
        }
        if (mismatch) {
            break;
        }

        pos += edge.size();
        node_idx = child_idx;

        // At a block boundary: must have a terminal or we stop (contiguity gap)
        if (pos % static_cast<size_t>(block_size_) == 0) {
            const auto& terminal = nodes_[node_idx].terminal_block;
            if (terminal.has_value()) {
                result.blocks.push_back(*terminal);
            } else {
                break;
            }
        }
    }

    result.matched_tokens = static_cast<uint32_t>(result.blocks.size() * block_size_);
    return result;
}

void PrefixIndex::insert(kvcache::CacheContextHash ctx_hash,
                         Span<const kvcache::TokenId> tokens,
                         Span<const kvcache::BlockId> blocks) {
    if (blocks.empty()) return;

    const uint32_t root = getOrCreateRoot(ctx_hash);
    uint32_t node_idx = root;
    size_t pos = 0;

    for (size_t block_counter = 0; block_counter < blocks.size(); ++block_counter) {
        const size_t segment_end = (block_counter + 1) * static_cast<size_t>(block_size_);

        while (pos < segment_end) {
            const kvcache::TokenId first = tokens[pos];

            auto child_it = nodes_[node_idx].children.find(first);

            if (child_it == nodes_[node_idx].children.end()) {
                // No existing path: create leaf covering tokens[pos..segment_end]
                std::vector<kvcache::TokenId> edge(tokens.data() + pos,
                                                   tokens.data() + segment_end);
                const kvcache::BlockId block = blocks[block_counter];
                const uint32_t new_idx = newNode();
                nodes_[new_idx].edge_tokens = std::move(edge);
                nodes_[new_idx].terminal_block = block;
                nodes_[new_idx].parent_idx = node_idx;
                nodes_[new_idx].first_edge_token = first;
                block_node_index_[block] = new_idx;
                nodes_[node_idx].children[first] = new_idx;
                pos = segment_end;
                node_idx = new_idx;
                break;
            }

            const uint32_t child_idx = child_it->second;

            // Copy edge data before any potential vector reallocation via newNode()
            std::vector<kvcache::TokenId> edge = nodes_[child_idx].edge_tokens;
            const size_t remaining = segment_end - pos;
            size_t lcp = 0;
            {
                const size_t max_match = std::min(edge.size(), remaining);
                while (lcp < max_match && tokens[pos + lcp] == edge[lcp]) {
                    ++lcp;
                }
            }

            if (lcp == edge.size()) {
                // Full edge match — descend
                pos += lcp;
                node_idx = child_idx;

                if (pos == segment_end) {
                    if (!nodes_[child_idx].terminal_block.has_value()) {
                        nodes_[child_idx].terminal_block = blocks[block_counter];
                        block_node_index_[blocks[block_counter]] = child_idx;
                    }
                    break;
                }
                // Continue inner loop: same block segment, deeper node
            } else {
                // Partial match: split edge at lcp
                const std::optional<kvcache::BlockId> split_terminal =
                    nodes_[child_idx].terminal_block;
                std::vector<kvcache::TokenId> split_edge(
                    edge.begin() + static_cast<ptrdiff_t>(lcp), edge.end());
                const kvcache::TokenId split_first = split_edge[0];

                const uint32_t split_idx = newNode();
                nodes_[split_idx].edge_tokens = std::move(split_edge);
                nodes_[split_idx].terminal_block = split_terminal;
                nodes_[split_idx].children = std::move(nodes_[child_idx].children);
                nodes_[split_idx].parent_idx = child_idx;
                nodes_[split_idx].first_edge_token = split_first;

                for (auto& [tok, grandchild] : nodes_[split_idx].children) {
                    nodes_[grandchild].parent_idx = split_idx;
                }

                if (split_terminal.has_value()) {
                    block_node_index_[*split_terminal] = split_idx;
                }

                nodes_[child_idx].edge_tokens.resize(lcp);
                nodes_[child_idx].terminal_block = std::nullopt;
                nodes_[child_idx].children.clear();
                nodes_[child_idx].children[split_first] = split_idx;

                pos += lcp;
                node_idx = child_idx;
                // Continue inner loop: will create new branch from divergence point
            }
        }
    }
}

void PrefixIndex::remove(kvcache::BlockId id) {
    auto it = block_node_index_.find(id);
    if (it == block_node_index_.end()) {
        return;
    }
    const uint32_t node_idx = it->second;
    block_node_index_.erase(it);
    nodes_[node_idx].terminal_block = std::nullopt;
    pruneIfEmpty(node_idx);
}

void PrefixIndex::pruneIfEmpty(uint32_t node_idx) {
    while (node_idx != INVALID_NODE) {
        const auto& node = nodes_[node_idx];
        if (!node.children.empty() || node.terminal_block.has_value()) {
            break;
        }
        const uint32_t parent_idx = node.parent_idx;
        if (parent_idx == INVALID_NODE) {
            break;  // root: don't prune
        }
        const kvcache::TokenId first = node.first_edge_token;
        nodes_[parent_idx].children.erase(first);
        node_idx = parent_idx;
    }
}

}  // namespace kvcache::prefix
