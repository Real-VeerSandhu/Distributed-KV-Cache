#include "pipeline/admission_stage.h"

namespace kvcache::pipeline {

AdmissionStage::AdmissionStage(LocalCache& cache) noexcept : cache_(cache) {}

AdmissionOutcome AdmissionStage::admitRemoteBlocks(const RouteRequest&,
                                                    const RemoteResolutionOutcome& remote) {
    AdmissionOutcome outcome;
    for (const auto& candidate : remote.fetched_blocks) {
        auto result = cache_.admitBlock(candidate);
        if (result.status == AdmitResult::Status::Admitted ||
            result.status == AdmitResult::Status::AlreadyPresent) {
            if (result.handle.has_value()) {
                outcome.admitted_handles.push_back(std::move(*result.handle));
            }
            ++outcome.admitted_count;
        } else {
            ++outcome.rejected_count;
        }
    }
    return outcome;
}

AdmissionOutcome AdmissionStage::admitComputedBlocks(const RouteRequest&,
                                                      Span<const BlockCandidate> blocks) {
    AdmissionOutcome outcome;
    for (const auto& candidate : blocks) {
        auto result = cache_.admitBlock(candidate);
        if (result.status == AdmitResult::Status::Admitted ||
            result.status == AdmitResult::Status::AlreadyPresent) {
            if (result.handle.has_value()) {
                outcome.admitted_handles.push_back(std::move(*result.handle));
            }
            ++outcome.admitted_count;
        } else {
            ++outcome.rejected_count;
        }
    }
    return outcome;
}

}  // namespace kvcache::pipeline
