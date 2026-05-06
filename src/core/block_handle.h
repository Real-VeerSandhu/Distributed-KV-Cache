#pragma once

#include "core/block_store.h"

namespace kvcache {

class BlockHandle {
public:
    explicit BlockHandle(BlockRecord* record) noexcept;
    ~BlockHandle() noexcept;

    BlockHandle(BlockHandle&&) noexcept;
    BlockHandle& operator=(BlockHandle&&) noexcept;

    BlockHandle(const BlockHandle&) = delete;
    BlockHandle& operator=(const BlockHandle&) = delete;

    [[nodiscard]] BlockId id() const noexcept;
    [[nodiscard]] const BlockRecord& metadata() const noexcept;
    [[nodiscard]] bool valid() const noexcept;

private:
    BlockRecord* record_;
};

}  // namespace kvcache
