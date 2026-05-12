#pragma once

#include "coordinator/coordinator_client.h"
#include "coordinator/coordinator_service.h"

namespace kvcache::coordinator {

// Connects a pipeline stage to a CoordinatorService running in the same process.
class InprocCoordinatorClient : public CoordinatorClient {
public:
    explicit InprocCoordinatorClient(CoordinatorService& service) noexcept;

    [[nodiscard]] CoordinatorQueryResult query(ContentHash hash) override;
    void announce(ContentHash hash, GlobalBlockRef ref) override;
    void invalidate(NodeId node, BlockId block_id, uint64_t generation) override;

private:
    CoordinatorService& service_;
};

}  // namespace kvcache::coordinator
