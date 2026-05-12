#include "tier/tier_manager.h"

#include <stdexcept>

namespace kvcache::tier {

TierManager::TierManager(std::unordered_map<Tier, TierStore> tiers,
                          PayloadStore& payload_store, policy::LatencyModel& latency_model,
                          BlockStore& block_store)
    : tiers_(std::move(tiers)),
      payload_store_(payload_store),
      latency_model_(latency_model),
      block_store_(block_store) {}

TierOpResult TierManager::place(BlockId block_id, Tier target,
                                  Span<const std::byte> payload_bytes) {
    auto it = tiers_.find(target);
    if (it == tiers_.end()) {
        throw std::logic_error("TierManager::place: unknown tier");
    }
    if (!it->second.hasCapacity()) {
        return {TierOpResult::Status::CapacityExceeded, 0};
    }

    const PayloadRef ref = payload_store_.put(payload_bytes);
    it->second.add(block_id);
    block_store_.setTier(block_id, target);
    block_store_.setPayload(block_id, ref);

    const policy::TierAccessInput access_input{target, payload_store_.bytesPerBlock()};
    return {TierOpResult::Status::Success, latency_model_.tierAccessNs(access_input)};
}

TierOpResult TierManager::promote(BlockId block_id, Tier target) {
    const Tier source = block_store_.get(block_id).tier;

    auto source_it = tiers_.find(source);
    if (source_it == tiers_.end() || !source_it->second.contains(block_id)) {
        return {TierOpResult::Status::BlockNotFound, 0};
    }

    auto target_it = tiers_.find(target);
    if (target_it == tiers_.end()) {
        throw std::logic_error("TierManager::promote: unknown target tier");
    }
    if (!target_it->second.hasCapacity()) {
        return {TierOpResult::Status::CapacityExceeded, 0};
    }

    source_it->second.remove(block_id);
    target_it->second.add(block_id);
    block_store_.setTier(block_id, target);

    const policy::MigrationInput mig{source, target, payload_store_.bytesPerBlock()};
    return {TierOpResult::Status::Success, latency_model_.migrationNs(mig)};
}

TierOpResult TierManager::demote(BlockId block_id, Tier target) {
    const Tier source = block_store_.get(block_id).tier;

    auto source_it = tiers_.find(source);
    if (source_it == tiers_.end() || !source_it->second.contains(block_id)) {
        return {TierOpResult::Status::BlockNotFound, 0};
    }

    auto target_it = tiers_.find(target);
    if (target_it == tiers_.end()) {
        throw std::logic_error("TierManager::demote: unknown target tier");
    }
    if (!target_it->second.hasCapacity()) {
        return {TierOpResult::Status::CapacityExceeded, 0};
    }

    source_it->second.remove(block_id);
    target_it->second.add(block_id);
    block_store_.setTier(block_id, target);

    const policy::MigrationInput mig{source, target, payload_store_.bytesPerBlock()};
    return {TierOpResult::Status::Success, latency_model_.migrationNs(mig)};
}

TierOpResult TierManager::remove(BlockId block_id) {
    for (auto& [tier, store] : tiers_) {
        if (store.contains(block_id)) {
            const auto& rec = block_store_.get(block_id);
            payload_store_.remove(rec.payload);
            store.remove(block_id);

            const policy::TierAccessInput access_input{tier, payload_store_.bytesPerBlock()};
            return {TierOpResult::Status::Success, latency_model_.tierAccessNs(access_input)};
        }
    }
    return {TierOpResult::Status::BlockNotFound, 0};
}

TierStats TierManager::stats(Tier tier) const {
    const auto it = tiers_.find(tier);
    if (it == tiers_.end()) {
        return {0, 0};
    }
    return it->second.stats();
}

bool TierManager::hasCapacity(Tier tier) const noexcept {
    const auto it = tiers_.find(tier);
    if (it == tiers_.end()) {
        return false;
    }
    return it->second.hasCapacity();
}

const TierStore& TierManager::tierStore(Tier tier) const {
    const auto it = tiers_.find(tier);
    if (it == tiers_.end()) {
        throw std::logic_error("TierManager::tierStore: unknown tier");
    }
    return it->second;
}

Span<const std::byte> TierManager::getPayload(tier::PayloadRef ref) const {
    return payload_store_.get(ref);
}

}  // namespace kvcache::tier
