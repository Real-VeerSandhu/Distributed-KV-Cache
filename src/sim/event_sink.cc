#include "sim/event_sink.h"

#include <fstream>
#include <ostream>
#include <stdexcept>

#include <nlohmann/json.hpp>

#include "tier/tier.h"

namespace kvcache::sim {

void InMemoryEventSink::record(const Event& event) { events_.push_back(event); }

const std::vector<Event>& InMemoryEventSink::events() const noexcept { return events_; }

std::size_t InMemoryEventSink::count() const noexcept { return events_.size(); }

void InMemoryEventSink::clear() noexcept { events_.clear(); }

namespace {

nlohmann::json serializeEvent(const LookupHit& e) {
    return {{"type", "lookup_hit"},
            {"block_id", static_cast<uint64_t>(e.id)},
            {"timestamp_ns", e.timestamp_ns}};
}

nlohmann::json serializeEvent(const LookupMiss& e) {
    return {{"type", "lookup_miss"},
            {"context_hash", e.context_hash},
            {"miss_token_offset", e.miss_token_offset},
            {"timestamp_ns", e.timestamp_ns}};
}

nlohmann::json serializeEvent(const BlockAdmitted& e) {
    return {{"type", "block_admitted"},
            {"block_id", static_cast<uint64_t>(e.id)},
            {"context_hash", e.context_hash},
            {"tier", tier_name(e.tier)},
            {"origin", e.origin == BlockOrigin::LocallyComputed ? "local" : "remote"},
            {"timestamp_ns", e.timestamp_ns}};
}

nlohmann::json serializeEvent(const BlockEvicted& e) {
    return {{"type", "block_evicted"},
            {"block_id", static_cast<uint64_t>(e.id)},
            {"policy", e.policy_name},
            {"timestamp_ns", e.timestamp_ns}};
}

nlohmann::json serializeEvent(const BlockPromoted& e) {
    return {{"type", "block_promoted"},
            {"block_id", static_cast<uint64_t>(e.id)},
            {"from", tier_name(e.from_tier)},
            {"to", tier_name(e.to_tier)},
            {"timestamp_ns", e.timestamp_ns}};
}

nlohmann::json serializeEvent(const BlockDemoted& e) {
    return {{"type", "block_demoted"},
            {"block_id", static_cast<uint64_t>(e.id)},
            {"from", tier_name(e.from_tier)},
            {"to", tier_name(e.to_tier)},
            {"timestamp_ns", e.timestamp_ns}};
}

nlohmann::json serializeEvent(const RemoteFetchStarted& e) {
    return {{"type", "remote_fetch_started"},
            {"hash_lo", e.hash.lo},
            {"hash_hi", e.hash.hi},
            {"source_node", static_cast<uint64_t>(e.source_node)},
            {"timestamp_ns", e.timestamp_ns}};
}

nlohmann::json serializeEvent(const RemoteFetchCompleted& e) {
    return {{"type", "remote_fetch_completed"},
            {"hash_lo", e.hash.lo},
            {"hash_hi", e.hash.hi},
            {"source_node", static_cast<uint64_t>(e.source_node)},
            {"bytes_fetched", e.bytes_fetched},
            {"timestamp_ns", e.timestamp_ns}};
}

std::string_view fetchFailName(FetchFailReason reason) noexcept {
    switch (reason) {
        case FetchFailReason::NotFound:        return "not_found";
        case FetchFailReason::StaleGeneration: return "stale_generation";
        case FetchFailReason::HashMismatch:    return "hash_mismatch";
        case FetchFailReason::TransportError:  return "transport_error";
    }
    return "unknown";
}

nlohmann::json serializeEvent(const RemoteFetchFailed& e) {
    return {{"type", "remote_fetch_failed"},
            {"hash_lo", e.hash.lo},
            {"hash_hi", e.hash.hi},
            {"source_node", static_cast<uint64_t>(e.source_node)},
            {"reason", fetchFailName(e.reason)},
            {"timestamp_ns", e.timestamp_ns}};
}

nlohmann::json serializeEvent(const CoordinatorStaleRef& e) {
    return {{"type", "coordinator_stale_ref"},
            {"node_id", static_cast<uint64_t>(e.node)},
            {"block_id", static_cast<uint64_t>(e.block_id)},
            {"generation", e.generation},
            {"timestamp_ns", e.timestamp_ns}};
}

nlohmann::json serializeEvent(const NonContiguousLocalBlocks& e) {
    return {{"type", "non_contiguous_local_blocks"},
            {"gap_at_block_index", e.gap_at_block_index},
            {"timestamp_ns", e.timestamp_ns}};
}

}  // namespace

JsonlEventSink::JsonlEventSink(std::ostream& out) noexcept : owned_file_(nullptr), out_(out) {}

JsonlEventSink::JsonlEventSink(const std::string& path)
    : owned_file_(std::make_unique<std::ofstream>(path)), out_(*owned_file_) {
    if (!owned_file_->is_open()) {
        throw std::runtime_error("JsonlEventSink: cannot open file: " + path);
    }
}

JsonlEventSink::~JsonlEventSink() {
    if (owned_file_ && owned_file_->is_open()) {
        owned_file_->flush();
    }
}

void JsonlEventSink::record(const Event& event) {
    const auto json = std::visit([](const auto& e) { return serializeEvent(e); }, event);
    out_ << json.dump() << '\n';
}

void JsonlEventSink::flush() { out_.flush(); }

}  // namespace kvcache::sim
