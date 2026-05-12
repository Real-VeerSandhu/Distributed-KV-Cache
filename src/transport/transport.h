#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#include "core/ids.h"

namespace kvcache::transport {

struct FetchBlockResponse {
    enum class Status { Ok, NotFound, StaleGeneration, TransportError };
    Status status{Status::TransportError};
    std::vector<std::byte> payload;
    ContentHash hash{};
    uint64_t generation{0};
};

class Transport {
public:
    virtual ~Transport() = default;

    Transport(const Transport&) = delete;
    Transport& operator=(const Transport&) = delete;

    [[nodiscard]] virtual FetchBlockResponse fetchBlock(NodeId node, BlockId block,
                                                        uint64_t generation) = 0;

protected:
    Transport() = default;
};

}  // namespace kvcache::transport
