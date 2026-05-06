#include "core/block_handle.h"

namespace kvcache {

BlockHandle::BlockHandle(BlockRecord* record) noexcept : record_(record) {
    if (record_) {
        record_->refcount.fetch_add(1, std::memory_order_relaxed);
    }
}

BlockHandle::~BlockHandle() noexcept {
    if (record_) {
        record_->refcount.fetch_sub(1, std::memory_order_acq_rel);
    }
}

BlockHandle::BlockHandle(BlockHandle&& other) noexcept : record_(other.record_) {
    other.record_ = nullptr;
}

BlockHandle& BlockHandle::operator=(BlockHandle&& other) noexcept {
    if (this != &other) {
        if (record_) {
            record_->refcount.fetch_sub(1, std::memory_order_acq_rel);
        }
        record_ = other.record_;
        other.record_ = nullptr;
    }
    return *this;
}

BlockId BlockHandle::id() const noexcept {
    return record_->id;
}

const BlockRecord& BlockHandle::metadata() const noexcept {
    return *record_;
}

bool BlockHandle::valid() const noexcept {
    return record_ != nullptr;
}

}  // namespace kvcache
