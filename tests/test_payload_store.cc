#include "tier/payload_store.h"

#include <cstddef>
#include <vector>

#include <gtest/gtest.h>

using namespace kvcache;
using namespace kvcache::tier;

namespace {

Span<const std::byte> makeSpan(const std::vector<std::byte>& v) {
    return Span<const std::byte>{v.data(), v.size()};
}

std::vector<std::byte> makeBytes(std::size_t n, std::byte fill = std::byte{0xAB}) {
    return std::vector<std::byte>(n, fill);
}

}  // namespace

TEST(PayloadStore, MetadataOnlyPutReturnsSentinel) {
    PayloadStore store{8, 0};
    const auto ref = store.put(Span<const std::byte>{});
    EXPECT_FALSE(ref.valid());
}

TEST(PayloadStore, MetadataOnlyBytesUsedIsZero) {
    PayloadStore store{8, 0};
    store.put(Span<const std::byte>{});
    EXPECT_EQ(store.bytesUsed(), 0u);
}

TEST(PayloadStore, MetadataOnlyGetReturnsEmpty) {
    PayloadStore store{8, 0};
    const auto ref = store.put(Span<const std::byte>{});
    EXPECT_TRUE(store.get(ref).empty());
}

TEST(PayloadStore, MetadataOnlyRemoveNoOp) {
    PayloadStore store{8, 0};
    const auto ref = store.put(Span<const std::byte>{});
    EXPECT_NO_THROW(store.remove(ref));
}

TEST(PayloadStore, SmallModePutReturnsValidRef) {
    PayloadStore store{4, 64};
    const auto bytes = makeBytes(64);
    const auto ref = store.put(makeSpan(bytes));
    EXPECT_TRUE(ref.valid());
}

TEST(PayloadStore, SmallModeGetReturnsStoredBytes) {
    PayloadStore store{4, 64};
    auto bytes = makeBytes(64, std::byte{0x42});
    const auto ref = store.put(makeSpan(bytes));
    const auto got = store.get(ref);
    ASSERT_EQ(got.size(), 64u);
    EXPECT_EQ(got[0], std::byte{0x42});
}

TEST(PayloadStore, SmallModeBytesUsedIncrementsOnPut) {
    PayloadStore store{4, 64};
    EXPECT_EQ(store.bytesUsed(), 0u);
    const auto bytes = makeBytes(64);
    store.put(makeSpan(bytes));
    EXPECT_EQ(store.bytesUsed(), 64u);
}

TEST(PayloadStore, SmallModeBytesUsedDecrementsOnRemove) {
    PayloadStore store{4, 64};
    const auto bytes = makeBytes(64);
    const auto ref = store.put(makeSpan(bytes));
    store.remove(ref);
    EXPECT_EQ(store.bytesUsed(), 0u);
}

TEST(PayloadStore, MultipleSlotsPutAndGet) {
    PayloadStore store{4, 4};
    auto b0 = makeBytes(4, std::byte{0x01});
    auto b1 = makeBytes(4, std::byte{0x02});
    const auto ref0 = store.put(makeSpan(b0));
    const auto ref1 = store.put(makeSpan(b1));
    EXPECT_NE(ref0.index, ref1.index);
    EXPECT_EQ(store.get(ref0)[0], std::byte{0x01});
    EXPECT_EQ(store.get(ref1)[0], std::byte{0x02});
}

TEST(PayloadStore, SlotReuseAfterRemove) {
    PayloadStore store{2, 4};
    auto bytes = makeBytes(4);
    const auto ref0 = store.put(makeSpan(bytes));
    store.remove(ref0);
    // Slot should be reused
    const auto ref1 = store.put(makeSpan(bytes));
    EXPECT_EQ(ref0.index, ref1.index);
}

TEST(PayloadStore, FullStoreThrowsOnPut) {
    PayloadStore store{1, 4};
    auto bytes = makeBytes(4);
    store.put(makeSpan(bytes));
    EXPECT_THROW(store.put(makeSpan(bytes)), std::runtime_error);
}

TEST(PayloadStore, BytesPerBlockReflectsMode) {
    PayloadStore meta{8, 0};
    EXPECT_EQ(meta.bytesPerBlock(), 0u);

    PayloadStore small{8, 65536};
    EXPECT_EQ(small.bytesPerBlock(), 65536u);
}

TEST(PayloadStore, RemoveInvalidRefIsNoOp) {
    PayloadStore store{4, 64};
    PayloadRef invalid{};
    EXPECT_NO_THROW(store.remove(invalid));
}
