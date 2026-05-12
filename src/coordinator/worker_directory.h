#pragma once

#include <cstddef>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include "core/ids.h"

namespace kvcache::coordinator {

struct WorkerInfo {
    NodeId node_id;
    std::string address;  // "host:port" or empty for inproc
};

class WorkerDirectory {
public:
    WorkerDirectory() = default;

    void nodeJoin(WorkerInfo info);
    void nodeLeave(NodeId node);

    [[nodiscard]] std::optional<WorkerInfo> lookup(NodeId node) const;
    [[nodiscard]] std::vector<WorkerInfo> allWorkers() const;
    [[nodiscard]] std::size_t workerCount() const noexcept;

private:
    std::unordered_map<NodeId, WorkerInfo> workers_;
    // TODO(threading): mutex around workers_
};

}  // namespace kvcache::coordinator
