#pragma once

#include <atomic>
#include <cstdint>
#include <memory>

#include "core/ids.h"
#include "core/span.h"
#include "tier/payload_store.h"
#include "tier/tier.h"

namespace kvcache {

enum class BlockState { Free, Admitting, Ready, Fetching, Migrating, Evicting };

struct BlockRecord {
    BlockId id{BlockId{0}};
    uint64_t generation{0};
    ContentHash hash{0, 0};
    uint32_t block_index{0};
    uint16_t token_count{0};
    Tier tier{Tier::GpuSim};
    mutable std::atomic<uint32_t> refcount{0};
    uint64_t last_access_ns{0};
    uint64_t access_count{0};
    tier::PayloadRef payload{};
    BlockState state{BlockState::Free};

    BlockRecord() noexcept = default;
    BlockRecord(const BlockRecord&) = delete;
    BlockRecord& operator=(const BlockRecord&) = delete;
    BlockRecord(BlockRecord&&) = delete;
    BlockRecord& operator=(BlockRecord&&) = delete;
};

class BlockHandle;

class BlockStore {
public:
    explicit BlockStore(uint32_t capacity);

    [[nodiscard]] bool exists(BlockId id, uint64_t generation) const noexcept;
    [[nodiscard]] const BlockRecord& get(BlockId id) const;
    [[nodiscard]] Span<const BlockRecord> records() const noexcept;

    void initialize(BlockId id, uint64_t generation, ContentHash hash, uint32_t block_index,
                    uint16_t token_count, Tier tier, BlockState initial_state);
    void setState(BlockId id, BlockState state);
    void setTier(BlockId id, Tier tier);
    void setPayload(BlockId id, tier::PayloadRef payload);
    void markAccessed(BlockId id, uint64_t now_ns);

    [[nodiscard]] BlockHandle createHandle(BlockId id);

private:
    uint32_t capacity_;
    std::unique_ptr<BlockRecord[]> records_;
    // TODO(threading): per-tier or per-record mutex for state and refcount operations
};

}  // namespace kvcache
