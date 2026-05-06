#include <gtest/gtest.h>

#include <sstream>

#include <nlohmann/json.hpp>

#include "sim/event.h"
#include "sim/event_sink.h"

namespace kvcache::sim {

namespace {

Event makeLookupHit(uint64_t block_raw = 1, uint64_t ts = 100) {
    return LookupHit{BlockId{block_raw}, ts};
}

Event makeLookupMiss(uint64_t ctx_hash = 42, uint32_t offset = 0, uint64_t ts = 200) {
    return LookupMiss{ctx_hash, offset, ts};
}

Event makeBlockAdmitted(uint64_t block_raw = 5, uint64_t ts = 300) {
    return BlockAdmitted{BlockId{block_raw}, CacheContextHash{1}, Tier::GpuSim,
                         BlockOrigin::LocallyComputed, ts};
}

}  // namespace

// ---------------------------------------------------------------------------
// NullEventSink
// ---------------------------------------------------------------------------

TEST(NullEventSink, AcceptsHitWithoutCrash) {
    NullEventSink sink;
    sink.record(makeLookupHit());
}

TEST(NullEventSink, AcceptsAllEventTypes) {
    NullEventSink sink;
    sink.record(makeLookupHit());
    sink.record(makeLookupMiss());
    sink.record(makeBlockAdmitted());
    sink.record(BlockEvicted{BlockId{3}, "lru", 400});
    sink.record(BlockPromoted{BlockId{4}, Tier::Host, Tier::GpuSim, 500});
    sink.record(BlockDemoted{BlockId{5}, Tier::GpuSim, Tier::Host, 600});
    sink.record(RemoteFetchStarted{ContentHash{1, 2}, NodeId{0}, 700});
    sink.record(RemoteFetchCompleted{ContentHash{1, 2}, NodeId{0}, 1024, 800});
    sink.record(RemoteFetchFailed{ContentHash{3, 4}, NodeId{1}, FetchFailReason::NotFound, 900});
    sink.record(CoordinatorStaleRef{NodeId{2}, BlockId{9}, 3, 1000});
    sink.record(NonContiguousLocalBlocks{2, 1100});
}

// ---------------------------------------------------------------------------
// InMemoryEventSink
// ---------------------------------------------------------------------------

TEST(InMemoryEventSink, StartsEmpty) {
    InMemoryEventSink sink;
    EXPECT_EQ(sink.count(), 0U);
    EXPECT_TRUE(sink.events().empty());
}

TEST(InMemoryEventSink, RecordIncreasesCount) {
    InMemoryEventSink sink;
    sink.record(makeLookupHit());
    EXPECT_EQ(sink.count(), 1U);
    sink.record(makeLookupMiss());
    EXPECT_EQ(sink.count(), 2U);
}

TEST(InMemoryEventSink, EventsAreStoredInOrder) {
    InMemoryEventSink sink;
    sink.record(makeLookupHit(1, 100));
    sink.record(makeLookupMiss(42, 0, 200));

    ASSERT_EQ(sink.count(), 2U);
    EXPECT_TRUE(std::holds_alternative<LookupHit>(sink.events()[0]));
    EXPECT_TRUE(std::holds_alternative<LookupMiss>(sink.events()[1]));
}

TEST(InMemoryEventSink, ClearRemovesAllEvents) {
    InMemoryEventSink sink;
    sink.record(makeLookupHit());
    sink.record(makeLookupHit());
    sink.clear();
    EXPECT_EQ(sink.count(), 0U);
}

TEST(InMemoryEventSink, EventsOfTypeFiltersCorrectly) {
    InMemoryEventSink sink;
    sink.record(makeLookupHit(1, 100));
    sink.record(makeLookupMiss(42, 0, 200));
    sink.record(makeLookupHit(2, 300));

    const auto hits = sink.eventsOfType<LookupHit>();
    EXPECT_EQ(hits.size(), 2U);
    EXPECT_EQ(hits[0].id, BlockId{1});
    EXPECT_EQ(hits[1].id, BlockId{2});

    const auto misses = sink.eventsOfType<LookupMiss>();
    EXPECT_EQ(misses.size(), 1U);
    EXPECT_EQ(misses[0].context_hash, 42U);
}

TEST(InMemoryEventSink, EventsOfTypeReturnsEmptyWhenNoneMatch) {
    InMemoryEventSink sink;
    sink.record(makeLookupHit());
    EXPECT_TRUE(sink.eventsOfType<LookupMiss>().empty());
}

TEST(InMemoryEventSink, PreservesLookupHitFields) {
    InMemoryEventSink sink;
    sink.record(LookupHit{BlockId{77}, 999});

    const auto hits = sink.eventsOfType<LookupHit>();
    ASSERT_EQ(hits.size(), 1U);
    EXPECT_EQ(hits[0].id, BlockId{77});
    EXPECT_EQ(hits[0].timestamp_ns, 999U);
}

TEST(InMemoryEventSink, PreservesBlockAdmittedFields) {
    InMemoryEventSink sink;
    sink.record(BlockAdmitted{BlockId{3}, CacheContextHash{7}, Tier::Host,
                              BlockOrigin::RemoteFetch, 12345});

    const auto admitted = sink.eventsOfType<BlockAdmitted>();
    ASSERT_EQ(admitted.size(), 1U);
    EXPECT_EQ(admitted[0].id, BlockId{3});
    EXPECT_EQ(admitted[0].context_hash, 7U);
    EXPECT_EQ(admitted[0].tier, Tier::Host);
    EXPECT_EQ(admitted[0].origin, BlockOrigin::RemoteFetch);
    EXPECT_EQ(admitted[0].timestamp_ns, 12345U);
}

// ---------------------------------------------------------------------------
// JsonlEventSink
// ---------------------------------------------------------------------------

TEST(JsonlEventSink, WritesOneLookupHitAsValidJson) {
    std::ostringstream oss;
    JsonlEventSink sink(oss);
    sink.record(LookupHit{BlockId{42}, 1000});
    sink.flush();

    const auto line = oss.str();
    ASSERT_FALSE(line.empty());

    const auto j = nlohmann::json::parse(line);
    EXPECT_EQ(j.at("type").get<std::string>(), "lookup_hit");
    EXPECT_EQ(j.at("block_id").get<uint64_t>(), 42U);
    EXPECT_EQ(j.at("timestamp_ns").get<uint64_t>(), 1000U);
}

TEST(JsonlEventSink, EachEventIsOneSeparateLine) {
    std::ostringstream oss;
    JsonlEventSink sink(oss);
    sink.record(LookupHit{BlockId{1}, 100});
    sink.record(LookupMiss{42, 0, 200});
    sink.flush();

    const auto output = oss.str();
    // Two newlines expected (one per event)
    const auto newline_count = std::count(output.begin(), output.end(), '\n');
    EXPECT_EQ(newline_count, 2);
}

TEST(JsonlEventSink, BlockEvictedContainsPolicyName) {
    std::ostringstream oss;
    JsonlEventSink sink(oss);
    sink.record(BlockEvicted{BlockId{5}, "lru", 500});
    sink.flush();

    const auto j = nlohmann::json::parse(oss.str());
    EXPECT_EQ(j.at("type").get<std::string>(), "block_evicted");
    EXPECT_EQ(j.at("policy").get<std::string>(), "lru");
}

TEST(JsonlEventSink, RemoteFetchFailedContainsReason) {
    std::ostringstream oss;
    JsonlEventSink sink(oss);
    sink.record(RemoteFetchFailed{ContentHash{1, 2}, NodeId{0},
                                  FetchFailReason::HashMismatch, 700});
    sink.flush();

    const auto j = nlohmann::json::parse(oss.str());
    EXPECT_EQ(j.at("reason").get<std::string>(), "hash_mismatch");
}

}  // namespace kvcache::sim
