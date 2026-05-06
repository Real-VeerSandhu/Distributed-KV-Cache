#include <gtest/gtest.h>

#include <unordered_map>

#include "core/ids.h"

namespace kvcache {

TEST(BlockId, DistinctValuesAreUnequal) {
    const BlockId a{1};
    const BlockId b{2};
    EXPECT_NE(a, b);
}

TEST(BlockId, SameValueIsEqual) {
    const BlockId a{42};
    const BlockId b{42};
    EXPECT_EQ(a, b);
}

TEST(BlockId, UsableAsUnorderedMapKey) {
    std::unordered_map<BlockId, int> m;
    m[BlockId{1}] = 10;
    m[BlockId{2}] = 20;
    EXPECT_EQ(m.at(BlockId{1}), 10);
    EXPECT_EQ(m.at(BlockId{2}), 20);
    EXPECT_EQ(m.count(BlockId{3}), 0U);
}

TEST(NodeId, UsableAsUnorderedMapKey) {
    std::unordered_map<NodeId, std::string> m;
    m[NodeId{0}] = "worker-0";
    m[NodeId{1}] = "worker-1";
    EXPECT_EQ(m.at(NodeId{0}), "worker-0");
}

TEST(ContentHash, EqualHashesCompareEqual) {
    const ContentHash a{123, 456};
    const ContentHash b{123, 456};
    EXPECT_EQ(a, b);
}

TEST(ContentHash, DifferentLoMakesUnequal) {
    const ContentHash a{1, 456};
    const ContentHash b{2, 456};
    EXPECT_NE(a, b);
}

TEST(ContentHash, DifferentHiMakesUnequal) {
    const ContentHash a{123, 1};
    const ContentHash b{123, 2};
    EXPECT_NE(a, b);
}

TEST(ContentHash, UsableAsUnorderedMapKey) {
    std::unordered_map<ContentHash, int> m;
    const ContentHash h1{111, 222};
    const ContentHash h2{333, 444};
    m[h1] = 1;
    m[h2] = 2;
    EXPECT_EQ(m.at(h1), 1);
    EXPECT_EQ(m.at(h2), 2);
    EXPECT_EQ(m.count(ContentHash{0, 0}), 0U);
}

TEST(GlobalBlockRef, FieldsAreAccessible) {
    const GlobalBlockRef ref{NodeId{3}, BlockId{7}, 99};
    EXPECT_EQ(ref.node_id, NodeId{3});
    EXPECT_EQ(ref.block_id, BlockId{7});
    EXPECT_EQ(ref.generation, 99U);
}

TEST(BlockOrigin, EnumValuesAreDistinct) {
    EXPECT_NE(BlockOrigin::LocallyComputed, BlockOrigin::RemoteFetch);
}

TEST(FetchFailReason, AllValuesAreDistinct) {
    EXPECT_NE(FetchFailReason::NotFound, FetchFailReason::StaleGeneration);
    EXPECT_NE(FetchFailReason::HashMismatch, FetchFailReason::TransportError);
    EXPECT_NE(FetchFailReason::NotFound, FetchFailReason::HashMismatch);
}

}  // namespace kvcache
