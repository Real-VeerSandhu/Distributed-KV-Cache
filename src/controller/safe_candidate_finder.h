#pragma once

#include <vector>

#include "core/block_store.h"
#include "core/ids.h"

namespace kvcache::controller {

class SafeCandidateFinder {
public:
    explicit SafeCandidateFinder(const BlockStore& store);

    [[nodiscard]] std::vector<BlockId> find() const;

private:
    const BlockStore& store_;
};

}  // namespace kvcache::controller
