#pragma once

#include <cstdint>

#include "core/dtype.h"
#include "core/ids.h"

namespace kvcache {

struct CacheKeyContext {
    ModelId model_id;
    TokenizerId tokenizer_id;
    uint32_t block_size;
    DType dtype;
    uint32_t num_layers;
    uint32_t num_kv_heads;
    uint32_t head_dim;
    uint64_t rope_config_hash;
    uint64_t kv_layout_hash;
};

}  // namespace kvcache
