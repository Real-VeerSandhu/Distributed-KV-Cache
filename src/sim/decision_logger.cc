#include "sim/decision_logger.h"

#include <fstream>
#include <stdexcept>

#include <nlohmann/json.hpp>

namespace kvcache::sim {

void InMemoryDecisionLogger::log(const Decision& decision) { decisions_.push_back(decision); }

const std::vector<Decision>& InMemoryDecisionLogger::decisions() const noexcept {
    return decisions_;
}

std::size_t InMemoryDecisionLogger::count() const noexcept { return decisions_.size(); }

void InMemoryDecisionLogger::clear() noexcept { decisions_.clear(); }

namespace {

nlohmann::json toJson(const EvictionDecision& d) {
    std::vector<uint64_t> candidates;
    candidates.reserve(d.candidates.size());
    for (const auto id : d.candidates) {
        candidates.push_back(static_cast<uint64_t>(id));
    }
    return {{"event", "eviction_decision"},
            {"policy", d.policy_name},
            {"chosen_block", static_cast<uint64_t>(d.chosen_block)},
            {"candidates", candidates},
            {"timestamp_ns", d.timestamp_ns}};
}

nlohmann::json toJson(const AdmissionRecord& d) {
    return {{"event", "admission_decision"},
            {"policy", d.policy_name},
            {"admitted", d.admitted},
            {"hash_lo", d.candidate_hash.lo},
            {"hash_hi", d.candidate_hash.hi},
            {"timestamp_ns", d.timestamp_ns}};
}

}  // namespace

JsonlDecisionLogger::JsonlDecisionLogger(std::ostream& out) noexcept : out_(out) {}

JsonlDecisionLogger::JsonlDecisionLogger(const std::string& path)
    : owned_file_(std::make_unique<std::ofstream>(path, std::ios::app)), out_(*owned_file_) {
    if (!owned_file_->is_open()) {
        throw std::runtime_error("JsonlDecisionLogger: cannot open " + path);
    }
}

JsonlDecisionLogger::~JsonlDecisionLogger() {
    if (owned_file_) {
        owned_file_->flush();
    }
}

void JsonlDecisionLogger::log(const Decision& decision) {
    nlohmann::json j = std::visit(
        [](const auto& d) -> nlohmann::json { return toJson(d); }, decision);
    out_ << j.dump() << '\n';
}

void JsonlDecisionLogger::flush() {
    if (owned_file_) {
        owned_file_->flush();
    }
}

}  // namespace kvcache::sim
