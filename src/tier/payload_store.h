#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#include "core/span.h"

namespace kvcache::tier {

struct PayloadRef {
    static constexpr uint64_t INVALID = UINT64_MAX;
    uint64_t index{INVALID};

    [[nodiscard]] bool valid() const noexcept { return index != INVALID; }
    [[nodiscard]] bool operator==(const PayloadRef& o) const noexcept {
        return index == o.index;
    }
    [[nodiscard]] bool operator!=(const PayloadRef& o) const noexcept { return !(*this == o); }
};

// Owns opaque payload bytes for admitted blocks.
//   bytes_per_block == 0 → metadata_only mode: put() returns sentinel, nothing allocated
//   bytes_per_block > 0  → manages a pool of fixed-size buffers
class PayloadStore {
public:
    PayloadStore(uint32_t max_blocks, uint64_t bytes_per_block);

    [[nodiscard]] PayloadRef put(Span<const std::byte> bytes);
    [[nodiscard]] Span<const std::byte> get(PayloadRef ref) const;
    void remove(PayloadRef ref);

    [[nodiscard]] uint64_t bytesUsed() const noexcept;
    [[nodiscard]] uint64_t bytesPerBlock() const noexcept;

private:
    uint32_t max_blocks_;
    uint64_t bytes_per_block_;
    std::vector<std::vector<std::byte>> buffers_;
    std::vector<uint32_t> free_list_;
};

}  // namespace kvcache::tier
