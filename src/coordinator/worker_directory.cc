#include "coordinator/worker_directory.h"

namespace kvcache::coordinator {

void WorkerDirectory::nodeJoin(WorkerInfo info) {
    workers_.insert_or_assign(info.node_id, std::move(info));
}

void WorkerDirectory::nodeLeave(NodeId node) { workers_.erase(node); }

std::optional<WorkerInfo> WorkerDirectory::lookup(NodeId node) const {
    const auto it = workers_.find(node);
    if (it == workers_.end()) return std::nullopt;
    return it->second;
}

std::vector<WorkerInfo> WorkerDirectory::allWorkers() const {
    std::vector<WorkerInfo> result;
    result.reserve(workers_.size());
    for (const auto& [id, info] : workers_) {
        result.push_back(info);
    }
    return result;
}

std::size_t WorkerDirectory::workerCount() const noexcept { return workers_.size(); }

}  // namespace kvcache::coordinator
