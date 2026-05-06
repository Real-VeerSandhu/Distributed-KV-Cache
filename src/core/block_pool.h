#pragma once

#include <cstdint>
#include <optional>
#include <utility>
#include <vector>

#include "core/ids.h"

namespace kvcache {

class BlockPool {
public:
    explicit BlockPool(uint32_t capacity);

    [[nodiscard]] std::optional<std::pair<BlockId, uint64_t>> allocate();
    void free(BlockId id);

    [[nodiscard]] uint32_t capacity() const noexcept;
    [[nodiscard]] uint32_t available() const noexcept;

private:
    uint32_t capacity_;
    std::vector<uint64_t> generations_;
    std::vector<bool> in_use_;
    std::vector<BlockId> free_list_;
    // TODO(threading): std::mutex around free_list_ and in_use_
};

}  // namespace kvcache
