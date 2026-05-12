#include "transport/inproc_transport.h"

#include "worker/worker_service.h"

namespace kvcache::transport {

void InprocTransport::registerWorker(NodeId node_id, worker::WorkerService* service) {
    workers_[node_id] = service;
}

FetchBlockResponse InprocTransport::fetchBlock(NodeId node, BlockId block, uint64_t generation) {
    const auto it = workers_.find(node);
    if (it == workers_.end()) {
        return {FetchBlockResponse::Status::TransportError, {}, {}, 0};
    }

    auto result = it->second->serveBlockFetch(block, generation);

    switch (result.status) {
        case ServeBlockResult::Status::Ok:
            return {FetchBlockResponse::Status::Ok, std::move(result.payload), result.hash,
                    result.generation};
        case ServeBlockResult::Status::NotFound:
            return {FetchBlockResponse::Status::NotFound, {}, {}, 0};
        case ServeBlockResult::Status::StaleGeneration:
            return {FetchBlockResponse::Status::StaleGeneration, {}, {}, 0};
    }
    return {FetchBlockResponse::Status::TransportError, {}, {}, 0};
}

}  // namespace kvcache::transport
