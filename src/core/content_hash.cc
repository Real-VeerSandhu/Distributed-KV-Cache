#include "core/content_hash.h"

#include <array>
#include <cstddef>

#define XXH_INLINE_ALL
#include <xxhash.h>

namespace kvcache {

namespace {

void writeU32Le(uint8_t* buf, uint32_t val) noexcept {
    buf[0] = static_cast<uint8_t>(val);
    buf[1] = static_cast<uint8_t>(val >> 8U);
    buf[2] = static_cast<uint8_t>(val >> 16U);
    buf[3] = static_cast<uint8_t>(val >> 24U);
}

void writeU64Le(uint8_t* buf, uint64_t val) noexcept {
    buf[0] = static_cast<uint8_t>(val);
    buf[1] = static_cast<uint8_t>(val >> 8U);
    buf[2] = static_cast<uint8_t>(val >> 16U);
    buf[3] = static_cast<uint8_t>(val >> 24U);
    buf[4] = static_cast<uint8_t>(val >> 32U);
    buf[5] = static_cast<uint8_t>(val >> 40U);
    buf[6] = static_cast<uint8_t>(val >> 48U);
    buf[7] = static_cast<uint8_t>(val >> 56U);
}

}  // namespace

CacheContextHash computeContextHash(const CacheKeyContext& ctx) noexcept {
    // Canonical little-endian serialisation of all 9 fields, 56 bytes total:
    //  0– 7: model_id
    //  8–15: tokenizer_id
    // 16–19: block_size
    // 20–23: dtype
    // 24–27: num_layers
    // 28–31: num_kv_heads
    // 32–35: head_dim
    // 36–39: zero pad (alignment)
    // 40–47: rope_config_hash
    // 48–55: kv_layout_hash
    std::array<uint8_t, 56> buf{};

    writeU64Le(buf.data() + 0, static_cast<uint64_t>(ctx.model_id));
    writeU64Le(buf.data() + 8, static_cast<uint64_t>(ctx.tokenizer_id));
    writeU32Le(buf.data() + 16, ctx.block_size);
    writeU32Le(buf.data() + 20, static_cast<uint32_t>(ctx.dtype));
    writeU32Le(buf.data() + 24, ctx.num_layers);
    writeU32Le(buf.data() + 28, ctx.num_kv_heads);
    writeU32Le(buf.data() + 32, ctx.head_dim);
    writeU64Le(buf.data() + 40, ctx.rope_config_hash);
    writeU64Le(buf.data() + 48, ctx.kv_layout_hash);

    return static_cast<CacheContextHash>(XXH3_64bits(buf.data(), buf.size()));
}

ContentHash computeContentHash(CacheContextHash context_hash,
                               uint32_t block_index,
                               uint16_t token_count,
                               Span<const TokenId> tokens) noexcept {
    XXH3_state_t state;
    XXH3_128bits_reset(&state);

    XXH3_128bits_update(&state, &context_hash, sizeof(context_hash));
    XXH3_128bits_update(&state, &block_index, sizeof(block_index));
    XXH3_128bits_update(&state, &token_count, sizeof(token_count));

    if (!tokens.empty()) {
        XXH3_128bits_update(&state, tokens.data(), tokens.size() * sizeof(TokenId));
    }

    const XXH128_hash_t result = XXH3_128bits_digest(&state);
    return ContentHash{result.low64, result.high64};
}

}  // namespace kvcache
