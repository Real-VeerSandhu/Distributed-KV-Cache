#include "core/dtype.h"

namespace kvcache {

std::string_view dtype_name(DType dtype) noexcept {
    switch (dtype) {
        case DType::BF16: return "BF16";
        case DType::FP16: return "FP16";
        case DType::FP32: return "FP32";
        case DType::INT8: return "INT8";
    }
    return "UNKNOWN";
}

}  // namespace kvcache
