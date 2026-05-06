#pragma once

#include <string_view>

namespace kvcache {

enum class DType { BF16, FP16, FP32, INT8 };

[[nodiscard]] std::string_view dtype_name(DType dtype) noexcept;

}  // namespace kvcache
