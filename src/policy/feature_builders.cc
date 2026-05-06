#include "policy/feature_builders.h"

namespace kvcache::policy {

BlockFeatureBuilder::BlockFeatureBuilder(const BlockStore& store,
                                          const prefix::PrefixIndex& index)
    : store_(store), index_(index) {}

BlockPolicyFeatures BlockFeatureBuilder::build(BlockId id, uint64_t now_ns) const {
    const auto& rec = store_.get(id);
    const uint32_t child_count = index_.blockChildCount(id);
    const uint64_t last_ns = rec.last_access_ns;
    const uint64_t age_ns = (now_ns >= last_ns) ? (now_ns - last_ns) : 0;

    return BlockPolicyFeatures{
        .id = id,
        .hash = rec.hash,
        .tier = rec.tier,
        .payload_bytes = 0,
        .age_ns = age_ns,
        .last_access_ns = last_ns,
        .access_count = rec.access_count,
        .prefix_depth_blocks = rec.block_index,
        .prefix_child_count = child_count,
        .descendant_block_count = index_.blockDescendantCount(id),
        .is_shared_prefix = child_count > 0,
        .is_remote_origin = false,
        .estimated_recompute_cost_ns = 0,
        .estimated_refetch_cost_ns = 0,
    };
}

}  // namespace kvcache::policy
