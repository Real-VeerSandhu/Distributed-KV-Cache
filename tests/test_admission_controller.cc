#include "controller/admission_controller.h"

#include <gtest/gtest.h>

#include "core/block_candidate.h"
#include "core/clock.h"
#include "core/content_hash.h"
#include "policy/policies/always_admit.h"
#include "policy/policies/lru_eviction.h"
#include "policy/policies/no_eviction.h"
#include "prefix/prefix_index.h"
#include "prefix/prefix_lookup_engine.h"
#include "sim/decision_logger.h"
#include "sim/event_sink.h"

using namespace kvcache;
using namespace kvcache::controller;
using namespace kvcache::policy;
using namespace kvcache::prefix;
using namespace kvcache::sim;

namespace {

constexpr uint32_t BS = 4;

CacheKeyContext makeCtx() {
    return CacheKeyContext{ModelId{1}, TokenizerId{1}, BS, DType::BF16, 32, 8, 128, 0, 0};
}

BlockCandidate makeCandidate(const CacheKeyContext& ctx, uint32_t block_idx,
                              std::initializer_list<int> all_toks) {
    const CacheContextHash ctx_hash = computeContextHash(ctx);
    std::vector<TokenId> toks;
    toks.reserve(all_toks.size());
    for (int t : all_toks) toks.push_back(static_cast<TokenId>(t));
    const ContentHash h = computeContentHash(
        ctx_hash, block_idx, static_cast<uint16_t>(BS),
        Span<const TokenId>{toks.data() + block_idx * BS, BS});
    return BlockCandidate{ctx, block_idx, std::move(toks), h, {}, BlockOrigin::LocallyComputed,
                          std::nullopt};
}

AdmissionContext makeAdmCtx(uint64_t ts = 1000) {
    return AdmissionContext{RequestId{0}, CacheContextHash{0}, 0, ts};
}

// Full fixture that owns both controllers.
struct Rig {
    FakeClock clock;
    BlockManager mgr;
    PrefixIndex index{BS};
    PrefixLookupEngine engine;
    SafeCandidateFinder finder;
    InMemoryEventSink events;
    InMemoryDecisionLogger decisions;

    // Controllers stored by value — order matters: eviction before admission.
    EvictionController eviction_ctrl;
    AdmissionController admission_ctrl;

    Rig(uint32_t capacity, AdmissionPolicy& ap, EvictionPolicy& ep)
        : mgr(capacity, clock),
          engine(index, mgr),
          finder(mgr.store()),
          eviction_ctrl(ep, mgr, engine, finder, events, decisions),
          admission_ctrl(ap, eviction_ctrl, mgr, engine, BS, events, decisions) {}

    AdmitResult admit(const BlockCandidate& cand, uint64_t ts = 1000) {
        return admission_ctrl.admit(cand, makeAdmCtx(ts));
    }
};

}  // namespace

TEST(AdmissionController, AdmitsValidBlock) {
    AlwaysAdmitPolicy ap;
    NoEvictionPolicy ep;
    Rig rig{8, ap, ep};

    const auto ctx = makeCtx();
    const auto result = rig.admit(makeCandidate(ctx, 0, {1, 2, 3, 4}));

    EXPECT_EQ(result.status, AdmitResult::Status::Admitted);
    ASSERT_TRUE(result.handle.has_value());
}

TEST(AdmissionController, AdmittedBlockAppearsInPrefixIndex) {
    AlwaysAdmitPolicy ap;
    NoEvictionPolicy ep;
    Rig rig{8, ap, ep};

    const auto ctx = makeCtx();
    const CacheContextHash ctx_hash = computeContextHash(ctx);
    rig.admit(makeCandidate(ctx, 0, {1, 2, 3, 4}));

    std::vector<TokenId> toks = {1, 2, 3, 4};
    const auto lookup =
        rig.index.lookup(ctx_hash, Span<const TokenId>{toks.data(), toks.size()});
    EXPECT_EQ(lookup.blocks.size(), 1u);
}

TEST(AdmissionController, InvalidBlockSizeFails) {
    AlwaysAdmitPolicy ap;
    NoEvictionPolicy ep;
    Rig rig{8, ap, ep};

    const auto ctx = makeCtx();
    BlockCandidate bad{ctx, 0, {1, 2}, {0, 0}, {}, BlockOrigin::LocallyComputed, std::nullopt};
    const auto result = rig.admission_ctrl.admit(bad, makeAdmCtx());

    EXPECT_EQ(result.status, AdmitResult::Status::InvalidBlock);
}

TEST(AdmissionController, AlreadyPresentBlockReturnsStatus) {
    AlwaysAdmitPolicy ap;
    NoEvictionPolicy ep;
    Rig rig{8, ap, ep};

    const auto ctx = makeCtx();
    const auto cand = makeCandidate(ctx, 0, {1, 2, 3, 4});
    rig.admit(cand);
    const auto result = rig.admit(cand);

    EXPECT_EQ(result.status, AdmitResult::Status::AlreadyPresent);
}

TEST(AdmissionController, OutOfOrderBlockFails) {
    AlwaysAdmitPolicy ap;
    NoEvictionPolicy ep;
    Rig rig{8, ap, ep};

    const auto ctx = makeCtx();
    const auto cand = makeCandidate(ctx, 1, {1, 2, 3, 4, 5, 6, 7, 8});
    EXPECT_EQ(rig.admit(cand).status, AdmitResult::Status::InvalidBlock);
}

TEST(AdmissionController, CapacityExceededWhenFullAndNoEviction) {
    AlwaysAdmitPolicy ap;
    NoEvictionPolicy ep;
    Rig rig{1, ap, ep};

    const auto ctx = makeCtx();
    ASSERT_EQ(rig.admit(makeCandidate(ctx, 0, {1, 2, 3, 4})).status,
              AdmitResult::Status::Admitted);
    EXPECT_EQ(rig.admit(makeCandidate(ctx, 0, {5, 6, 7, 8})).status,
              AdmitResult::Status::CapacityExceeded);
}

TEST(AdmissionController, RejectedPolicyReturnsRejected) {
    struct AlwaysRejectPolicy : AdmissionPolicy {
        policy::AdmissionDecision decide(const AdmissionContext&,
                                         const BlockCandidateFeatures&) override {
            return policy::AdmissionDecision::Reject;
        }
        const char* name() const noexcept override { return "always_reject"; }
    };

    AlwaysRejectPolicy ap;
    NoEvictionPolicy ep;
    Rig rig{8, ap, ep};

    const auto ctx = makeCtx();
    EXPECT_EQ(rig.admit(makeCandidate(ctx, 0, {1, 2, 3, 4})).status,
              AdmitResult::Status::Rejected);
}

TEST(AdmissionController, AdmittedBlockEmitsEvent) {
    AlwaysAdmitPolicy ap;
    NoEvictionPolicy ep;
    Rig rig{8, ap, ep};

    const auto ctx = makeCtx();
    rig.admit(makeCandidate(ctx, 0, {1, 2, 3, 4}), 5000);

    const auto admitted = rig.events.eventsOfType<BlockAdmitted>();
    ASSERT_EQ(admitted.size(), 1u);
    EXPECT_EQ(admitted[0].timestamp_ns, 5000u);
}

TEST(AdmissionController, RejectedBlockDoesNotAppearInPrefixIndex) {
    struct AlwaysRejectPolicy : AdmissionPolicy {
        policy::AdmissionDecision decide(const AdmissionContext&,
                                         const BlockCandidateFeatures&) override {
            return policy::AdmissionDecision::Reject;
        }
        const char* name() const noexcept override { return "always_reject"; }
    };

    AlwaysRejectPolicy ap;
    NoEvictionPolicy ep;
    Rig rig{8, ap, ep};

    const auto ctx = makeCtx();
    const CacheContextHash ctx_hash = computeContextHash(ctx);
    rig.admit(makeCandidate(ctx, 0, {1, 2, 3, 4}));

    std::vector<TokenId> toks = {1, 2, 3, 4};
    const auto lookup =
        rig.index.lookup(ctx_hash, Span<const TokenId>{toks.data(), toks.size()});
    EXPECT_TRUE(lookup.blocks.empty());
}

TEST(AdmissionController, LruEvictionMakesSpaceForNewBlock) {
    LruEvictionPolicy lru;
    AlwaysAdmitPolicy ap;
    Rig rig{1, ap, lru};

    const auto ctx = makeCtx();
    ASSERT_EQ(rig.admit(makeCandidate(ctx, 0, {1, 2, 3, 4}), 1000).status,
              AdmitResult::Status::Admitted);

    // Handle from first block is gone; LRU should evict it to make room.
    const auto result = rig.admit(makeCandidate(ctx, 0, {5, 6, 7, 8}), 2000);
    EXPECT_EQ(result.status, AdmitResult::Status::Admitted);

    const auto evicted = rig.events.eventsOfType<BlockEvicted>();
    ASSERT_EQ(evicted.size(), 1u);
}
