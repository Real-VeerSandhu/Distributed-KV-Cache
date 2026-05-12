#pragma once

#include <unordered_map>

#include "core/ids.h"
#include "transport/transport.h"

namespace kvcache::worker {
class WorkerService;
}

namespace kvcache::transport {

// Connects workers running in the same process for integration tests and bench.
class InprocTransport : public Transport {
public:
    InprocTransport() = default;

    void registerWorker(NodeId node_id, worker::WorkerService* service);

    [[nodiscard]] FetchBlockResponse fetchBlock(NodeId node, BlockId block,
                                                uint64_t generation) override;

private:
    std::unordered_map<NodeId, worker::WorkerService*> workers_;
};

}  // namespace kvcache::transport
