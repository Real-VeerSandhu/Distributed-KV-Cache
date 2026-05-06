#pragma once

#include "core/block_store.h"
#include "policy/policy_features.h"
#include "prefix/prefix_index.h"

namespace kvcache::policy {

class BlockFeatureBuilder {
public:
    BlockFeatureBuilder(const BlockStore& store, const prefix::PrefixIndex& index);

    [[nodiscard]] BlockPolicyFeatures build(BlockId id, uint64_t now_ns) const;

private:
    const BlockStore& store_;
    const prefix::PrefixIndex& index_;
};

}  // namespace kvcache::policy
