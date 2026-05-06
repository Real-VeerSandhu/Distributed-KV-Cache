#pragma once

#include <string_view>

namespace kvcache {

enum class Tier { GpuSim, Host };

[[nodiscard]] std::string_view tier_name(Tier tier) noexcept;

}  // namespace kvcache
