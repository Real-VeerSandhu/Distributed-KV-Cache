#pragma once

#include <cstdint>
#include <functional>
#include <optional>

namespace kvcache {

enum class BlockId : uint64_t {};
enum class NodeId : uint64_t {};
enum class RequestId : uint64_t {};
enum class ModelId : uint64_t {};
enum class TokenizerId : uint64_t {};

using TokenId = int32_t;
using CacheContextHash = uint64_t;

struct ContentHash {
    uint64_t lo;
    uint64_t hi;

    [[nodiscard]] bool operator==(const ContentHash& other) const noexcept {
        return lo == other.lo && hi == other.hi;
    }
    [[nodiscard]] bool operator!=(const ContentHash& other) const noexcept {
        return !(*this == other);
    }
};

struct GlobalBlockRef {
    NodeId node_id;
    BlockId block_id;
    uint64_t generation;
};

enum class BlockOrigin { LocallyComputed, RemoteFetch };

enum class FetchFailReason { NotFound, StaleGeneration, HashMismatch, TransportError };

}  // namespace kvcache

namespace std {

template <>
struct hash<kvcache::BlockId> {
    std::size_t operator()(kvcache::BlockId id) const noexcept {
        return std::hash<uint64_t>{}(static_cast<uint64_t>(id));
    }
};

template <>
struct hash<kvcache::NodeId> {
    std::size_t operator()(kvcache::NodeId id) const noexcept {
        return std::hash<uint64_t>{}(static_cast<uint64_t>(id));
    }
};

template <>
struct hash<kvcache::RequestId> {
    std::size_t operator()(kvcache::RequestId id) const noexcept {
        return std::hash<uint64_t>{}(static_cast<uint64_t>(id));
    }
};

template <>
struct hash<kvcache::ContentHash> {
    std::size_t operator()(const kvcache::ContentHash& h) const noexcept {
        const std::size_t h1 = std::hash<uint64_t>{}(h.lo);
        const std::size_t h2 = std::hash<uint64_t>{}(h.hi);
        return h1 ^ (h2 * 0x9e3779b97f4a7c15ULL + 0x6c62272e07bb0142ULL + (h1 << 6U) + (h1 >> 2U));
    }
};

}  // namespace std
