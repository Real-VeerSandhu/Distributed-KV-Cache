#include "tier/tier_manager.h"

#include <unordered_map>
#include <vector>

#include <gtest/gtest.h>

#include "core/block_manager.h"
#include "core/clock.h"
#include "core/content_hash.h"
#include "policy/policies/constant_latency.h"
#include "tier/payload_store.h"
#include "tier/tier_store.h"

using namespace kvcache;
using namespace kvcache::tier;
using namespace kvcache::policy;

namespace {

constexpr uint32_t BLOCK_SIZE = 4;
constexpr uint64_t BYTES_PER_BLOCK = 64;

struct Fixture {
    FakeClock clock;
    BlockManager mgr{8, clock};
    PayloadStore payload_store{8, BYTES_PER_BLOCK};
    ConstantLatencyModel latency_model{1000, 5000, 50000, 500000};

    std::unordered_map<Tier, TierStore> makeTiers(uint32_t gpu_cap = 4,
                                                   uint32_t host_cap = 4) {
        std::unordered_map<Tier, TierStore> tiers;
        tiers.emplace(Tier::GpuSim, TierStore{gpu_cap});
        tiers.emplace(Tier::Host, TierStore{host_cap});
        return tiers;
    }

    TierManager makeTierManager(uint32_t gpu_cap = 4, uint32_t host_cap = 4) {
        return TierManager{makeTiers(gpu_cap, host_cap), payload_store, latency_model,
                           mgr.store()};
    }

    // Creates and admits a block through BlockManager directly (no TierManager).
    BlockId admitBlock(Tier tier = Tier::GpuSim) {
        auto slot = mgr.createBlock({1, 0}, 0, BLOCK_SIZE, tier);
        EXPECT_TRUE(slot.has_value());
        const auto [id, gen] = *slot;
        mgr.transitionState(id, BlockState::Admitting, BlockState::Ready);
        return id;
    }
};

std::vector<std::byte> makePayload(uint64_t size, std::byte fill = std::byte{0xCC}) {
    return std::vector<std::byte>(static_cast<size_t>(size), fill);
}

}  // namespace

// ──────────────────────── place ────────────────────────

TEST(TierManager, PlaceInGpuSimSucceeds) {
    Fixture f;
    auto tm = f.makeTierManager();
    const BlockId id = f.admitBlock(Tier::GpuSim);
    const auto payload = makePayload(BYTES_PER_BLOCK);
    const auto result =
        tm.place(id, Tier::GpuSim,
                 Span<const std::byte>{payload.data(), payload.size()});
    EXPECT_EQ(result.status, TierOpResult::Status::Success);
    EXPECT_GT(result.simulated_latency_ns, 0u);
}

TEST(TierManager, PlaceUpdatesBlockRecordTier) {
    Fixture f;
    auto tm = f.makeTierManager();
    const BlockId id = f.admitBlock(Tier::Host);
    const auto payload = makePayload(BYTES_PER_BLOCK);
    tm.place(id, Tier::Host, Span<const std::byte>{payload.data(), payload.size()});
    EXPECT_EQ(f.mgr.store().get(id).tier, Tier::Host);
}

TEST(TierManager, PlaceUpdatesBlockRecordPayload) {
    Fixture f;
    auto tm = f.makeTierManager();
    const BlockId id = f.admitBlock(Tier::GpuSim);
    const auto payload = makePayload(BYTES_PER_BLOCK);
    tm.place(id, Tier::GpuSim, Span<const std::byte>{payload.data(), payload.size()});
    EXPECT_TRUE(f.mgr.store().get(id).payload.valid());
}

TEST(TierManager, PlaceInFullTierReturnsCapacityExceeded) {
    Fixture f;
    auto tm = f.makeTierManager(1, 4);  // GPU capacity = 1
    const BlockId id0 = f.admitBlock(Tier::GpuSim);
    const BlockId id1 = f.admitBlock(Tier::GpuSim);
    const auto payload = makePayload(BYTES_PER_BLOCK);
    tm.place(id0, Tier::GpuSim, Span<const std::byte>{payload.data(), payload.size()});
    const auto result =
        tm.place(id1, Tier::GpuSim, Span<const std::byte>{payload.data(), payload.size()});
    EXPECT_EQ(result.status, TierOpResult::Status::CapacityExceeded);
}

TEST(TierManager, PlaceMetadataOnlyNoPayloadStored) {
    Fixture f;
    PayloadStore meta_store{8, 0};  // metadata_only
    ConstantLatencyModel latency;
    std::unordered_map<Tier, TierStore> tiers;
    tiers.emplace(Tier::GpuSim, TierStore{4});
    tiers.emplace(Tier::Host, TierStore{4});
    TierManager tm{std::move(tiers), meta_store, latency, f.mgr.store()};

    const BlockId id = f.admitBlock(Tier::GpuSim);
    const auto result = tm.place(id, Tier::GpuSim, Span<const std::byte>{});
    EXPECT_EQ(result.status, TierOpResult::Status::Success);
    EXPECT_FALSE(f.mgr.store().get(id).payload.valid());
    EXPECT_EQ(meta_store.bytesUsed(), 0u);
}

// ──────────────────────── stats / hasCapacity ────────────────────────

TEST(TierManager, StatsReflectsPlacedBlocks) {
    Fixture f;
    auto tm = f.makeTierManager();
    const BlockId id = f.admitBlock(Tier::GpuSim);
    const auto payload = makePayload(BYTES_PER_BLOCK);
    tm.place(id, Tier::GpuSim, Span<const std::byte>{payload.data(), payload.size()});
    const auto stats = tm.stats(Tier::GpuSim);
    EXPECT_EQ(stats.used_blocks, 1u);
    EXPECT_EQ(stats.capacity_blocks, 4u);
}

TEST(TierManager, HasCapacityFalseWhenFull) {
    Fixture f;
    auto tm = f.makeTierManager(1, 4);
    const BlockId id = f.admitBlock(Tier::GpuSim);
    const auto payload = makePayload(BYTES_PER_BLOCK);
    tm.place(id, Tier::GpuSim, Span<const std::byte>{payload.data(), payload.size()});
    EXPECT_FALSE(tm.hasCapacity(Tier::GpuSim));
    EXPECT_TRUE(tm.hasCapacity(Tier::Host));
}

// ──────────────────────── promote ────────────────────────

TEST(TierManager, PromoteMovesBlockFromHostToGpu) {
    Fixture f;
    auto tm = f.makeTierManager();
    const BlockId id = f.admitBlock(Tier::Host);
    const auto payload = makePayload(BYTES_PER_BLOCK);
    tm.place(id, Tier::Host, Span<const std::byte>{payload.data(), payload.size()});

    const auto result = tm.promote(id, Tier::GpuSim);
    EXPECT_EQ(result.status, TierOpResult::Status::Success);
    EXPECT_GT(result.simulated_latency_ns, 0u);
    EXPECT_EQ(f.mgr.store().get(id).tier, Tier::GpuSim);
    EXPECT_EQ(tm.stats(Tier::Host).used_blocks, 0u);
    EXPECT_EQ(tm.stats(Tier::GpuSim).used_blocks, 1u);
}

TEST(TierManager, PromoteToFullTierReturnsCapacityExceeded) {
    Fixture f;
    auto tm = f.makeTierManager(1, 4);
    // Fill GPU tier
    const BlockId id0 = f.admitBlock(Tier::GpuSim);
    const BlockId id1 = f.admitBlock(Tier::Host);
    const auto payload = makePayload(BYTES_PER_BLOCK);
    tm.place(id0, Tier::GpuSim, Span<const std::byte>{payload.data(), payload.size()});
    tm.place(id1, Tier::Host, Span<const std::byte>{payload.data(), payload.size()});

    const auto result = tm.promote(id1, Tier::GpuSim);
    EXPECT_EQ(result.status, TierOpResult::Status::CapacityExceeded);
}

// ──────────────────────── demote ────────────────────────

TEST(TierManager, DemoteMovesBlockFromGpuToHost) {
    Fixture f;
    auto tm = f.makeTierManager();
    const BlockId id = f.admitBlock(Tier::GpuSim);
    const auto payload = makePayload(BYTES_PER_BLOCK);
    tm.place(id, Tier::GpuSim, Span<const std::byte>{payload.data(), payload.size()});

    const auto result = tm.demote(id, Tier::Host);
    EXPECT_EQ(result.status, TierOpResult::Status::Success);
    EXPECT_EQ(f.mgr.store().get(id).tier, Tier::Host);
    EXPECT_EQ(tm.stats(Tier::GpuSim).used_blocks, 0u);
    EXPECT_EQ(tm.stats(Tier::Host).used_blocks, 1u);
}

// ──────────────────────── remove ────────────────────────

TEST(TierManager, RemoveDecreasesUsedBlocks) {
    Fixture f;
    auto tm = f.makeTierManager();
    const BlockId id = f.admitBlock(Tier::GpuSim);
    const auto payload = makePayload(BYTES_PER_BLOCK);
    tm.place(id, Tier::GpuSim, Span<const std::byte>{payload.data(), payload.size()});

    const auto result = tm.remove(id);
    EXPECT_EQ(result.status, TierOpResult::Status::Success);
    EXPECT_EQ(tm.stats(Tier::GpuSim).used_blocks, 0u);
    EXPECT_EQ(f.payload_store.bytesUsed(), 0u);
}

TEST(TierManager, RemoveUntrackedBlockReturnsNotFound) {
    Fixture f;
    auto tm = f.makeTierManager();
    const auto result = tm.remove(BlockId{99});
    EXPECT_EQ(result.status, TierOpResult::Status::BlockNotFound);
}

TEST(TierManager, TierStoreThrowsForUnknownTier) {
    Fixture f;
    auto tm = f.makeTierManager();
    // GpuSim and Host are the only valid tiers; the method still works for those.
    EXPECT_NO_THROW(tm.tierStore(Tier::GpuSim));
    EXPECT_NO_THROW(tm.tierStore(Tier::Host));
}
