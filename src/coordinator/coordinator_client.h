#pragma once

#include <vector>

#include "core/ids.h"
#include "core/span.h"

namespace kvcache::coordinator {

struct CoordinatorQueryResult {
    std::vector<GlobalBlockRef> refs;
    bool available{true};
};

class CoordinatorClient {
public:
    virtual ~CoordinatorClient() = default;

    CoordinatorClient(const CoordinatorClient&) = delete;
    CoordinatorClient& operator=(const CoordinatorClient&) = delete;

    [[nodiscard]] virtual CoordinatorQueryResult query(ContentHash hash) = 0;
    virtual void announce(ContentHash hash, GlobalBlockRef ref) = 0;
    virtual void invalidate(NodeId node, BlockId block_id, uint64_t generation) = 0;

protected:
    CoordinatorClient() = default;
};

class NullCoordinatorClient : public CoordinatorClient {
public:
    NullCoordinatorClient() = default;

    [[nodiscard]] CoordinatorQueryResult query(ContentHash) override {
        return {{}, false};
    }
    void announce(ContentHash, GlobalBlockRef) override {}
    void invalidate(NodeId, BlockId, uint64_t) override {}
};

}  // namespace kvcache::coordinator
