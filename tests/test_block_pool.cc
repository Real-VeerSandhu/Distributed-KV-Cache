#include "core/block_pool.h"

#include <gtest/gtest.h>

using namespace kvcache;

TEST(BlockPool, InitialStateIsFullyAvailable) {
    BlockPool pool{4};
    EXPECT_EQ(pool.capacity(), 4u);
    EXPECT_EQ(pool.available(), 4u);
}

TEST(BlockPool, AllocateDecreasesAvailable) {
    BlockPool pool{4};
    (void)pool.allocate();
    EXPECT_EQ(pool.available(), 3u);
    (void)pool.allocate();
    EXPECT_EQ(pool.available(), 2u);
}

TEST(BlockPool, AllocateReturnsDistinctIds) {
    BlockPool pool{4};
    auto a = pool.allocate();
    auto b = pool.allocate();
    ASSERT_TRUE(a.has_value());
    ASSERT_TRUE(b.has_value());
    EXPECT_NE(a->first, b->first);
}

TEST(BlockPool, FirstGenerationIsOne) {
    BlockPool pool{4};
    auto slot = pool.allocate();
    ASSERT_TRUE(slot.has_value());
    EXPECT_EQ(slot->second, 1u);
}

TEST(BlockPool, AllocateReturnsNulloptWhenExhausted) {
    BlockPool pool{2};
    ASSERT_TRUE(pool.allocate().has_value());
    ASSERT_TRUE(pool.allocate().has_value());
    EXPECT_FALSE(pool.allocate().has_value());
}

TEST(BlockPool, FreeIncreasesAvailable) {
    BlockPool pool{2};
    auto slot = pool.allocate();
    ASSERT_TRUE(slot.has_value());
    pool.free(slot->first);
    EXPECT_EQ(pool.available(), 2u);
}

TEST(BlockPool, GenerationIncrementsOnFree) {
    BlockPool pool{1};
    auto first = pool.allocate();
    ASSERT_TRUE(first.has_value());
    EXPECT_EQ(first->second, 1u);

    pool.free(first->first);

    auto second = pool.allocate();
    ASSERT_TRUE(second.has_value());
    EXPECT_EQ(second->first, first->first);   // same slot reused
    EXPECT_EQ(second->second, 2u);            // generation bumped
}

TEST(BlockPool, GenerationIncrementRepeats) {
    BlockPool pool{1};
    auto first = pool.allocate();
    ASSERT_TRUE(first.has_value());
    EXPECT_EQ(first->second, 1u);
    pool.free(first->first);

    for (uint64_t expected_gen = 2; expected_gen <= 5; ++expected_gen) {
        auto s = pool.allocate();
        ASSERT_TRUE(s.has_value());
        EXPECT_EQ(s->second, expected_gen);
        pool.free(s->first);
    }
}

TEST(BlockPool, DoubleFreeThrows) {
    BlockPool pool{2};
    auto slot = pool.allocate();
    ASSERT_TRUE(slot.has_value());
    pool.free(slot->first);
    EXPECT_THROW([&] { pool.free(slot->first); }(), std::logic_error);
}

TEST(BlockPool, FreeInvalidIdThrows) {
    BlockPool pool{2};
    EXPECT_THROW([&] { pool.free(BlockId{99}); }(), std::logic_error);
}

TEST(BlockPool, AllocateFreeAllocateCycle) {
    BlockPool pool{2};
    auto a = pool.allocate();
    auto b = pool.allocate();
    ASSERT_TRUE(a.has_value());
    ASSERT_TRUE(b.has_value());
    EXPECT_FALSE(pool.allocate().has_value());

    pool.free(a->first);
    EXPECT_EQ(pool.available(), 1u);

    auto c = pool.allocate();
    ASSERT_TRUE(c.has_value());
    EXPECT_EQ(c->first, a->first);
    EXPECT_EQ(c->second, 2u);
}
