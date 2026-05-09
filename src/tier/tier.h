#pragma once

#include <cstddef>
#include <functional>
#include <string_view>

namespace kvcache {

enum class Tier { GpuSim, Host };

[[nodiscard]] std::string_view tier_name(Tier tier) noexcept;

}  // namespace kvcache

namespace std {

template <>
struct hash<kvcache::Tier> {
    std::size_t operator()(kvcache::Tier t) const noexcept {
        return std::hash<int>{}(static_cast<int>(t));
    }
};

}  // namespace std
