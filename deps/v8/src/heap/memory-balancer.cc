// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/heap/memory-balancer.h"

#include "src/heap/heap-inl.h"
#include "src/heap/heap.h"

namespace v8 {
namespace internal {

void MemoryBalancer::RefreshLimit() {
  DCHECK(v8_flags.memory_balancer);
  if (major_allocation_rate_ && major_gc_speed_) {
    size_t new_limit =
        live_memory_after_gc_ +
        sqrt(live_memory_after_gc_ * (major_allocation_rate_.value().rate()) /
             (major_gc_speed_.value().rate()) /
             v8_flags.memory_balancer_c_value);
    UpdateHeapLimit(new_limit);
  }
}

// 2 MB of extra space.
// This allows the heap size to not decay to CurrentSizeOfObject()
// and prevents GC from triggering if, after a long period of idleness,
// a small allocation appears.
constexpr size_t kMinHeapExtraSpace = 2 * MB;

void MemoryBalancer::UpdateHeapLimit(size_t computed_limit) {
  size_t new_limit =
      std::max<size_t>(live_memory_after_gc_ + kMinHeapExtraSpace,
                       computed_limit) +
      heap_->NewSpaceCapacity();
  new_limit = std::min<size_t>(new_limit, heap_->max_old_generation_size());
  new_limit = std::max<size_t>(new_limit, heap_->min_old_generation_size());
  heap_->SetOldGenerationAndGlobalAllocationLimit(
      new_limit, new_limit + external_allocation_limit_);
}

void MemoryBalancer::PostHeartbeatTask() {
  heap_->GetForegroundTaskRunner()->PostDelayedTask(
      std::make_unique<HeartbeatTask>(heap_->isolate(), this), 1);
}

void MemoryBalancer::UpdateLiveMemory(size_t live_memory) {
  live_memory_after_gc_ = live_memory;
}

void MemoryBalancer::UpdateMajorGC(double major_gc_bytes,
                                   double major_gc_duration) {
  major_gc_duration *= kSecondsToNanoseconds;
  if (!major_gc_speed_) {
    major_gc_speed_ =
        SmoothedBytesAndDuration{major_gc_bytes, major_gc_duration};
  } else {
    major_gc_speed_->Update(major_gc_bytes, major_gc_duration,
                            kMajorGCDecayRate);
  }
}

void MemoryBalancer::UpdateMajorAllocation(double major_allocation_bytes,
                                           double major_allocation_duration) {
  major_allocation_duration *= kSecondsToNanoseconds;
  if (!major_allocation_rate_) {
    major_allocation_rate_ = SmoothedBytesAndDuration{
        major_allocation_bytes, major_allocation_duration};
  } else {
    major_allocation_rate_->Update(major_allocation_bytes,
                                   major_allocation_duration,
                                   kMajorAllocationDecayRate);
  }
}

void MemoryBalancer::NotifyGC() {
  last_measured_memory_ = heap_->OldGenerationSizeOfObjects();
  last_measured_at_ =
      heap_->MonotonicallyIncreasingTimeInMs() * kMillisecondsToNanoseconds;
  RefreshLimit();
  if (!heartbeat_task_started_) {
    heartbeat_task_started_ = true;
    PostHeartbeatTask();
  }
}

void MemoryBalancer::TracerUpdate(size_t live_memory,
                                  double major_allocation_bytes,
                                  double major_allocation_duration,
                                  double major_gc_bytes,
                                  double major_gc_duration) {
  UpdateLiveMemory(live_memory);
  UpdateMajorAllocation(major_allocation_bytes, major_allocation_duration);
  UpdateMajorGC(major_gc_bytes, major_gc_duration);
}

void MemoryBalancer::HeartbeatUpdate() {
  auto time = heap_->MonotonicallyIncreasingTimeInMs() *
              MemoryBalancer::kMillisecondsToNanoseconds;
  auto memory = heap_->OldGenerationSizeOfObjects() +
                heap_->AllocatedExternalMemorySinceMarkCompact();
  if (last_measured_memory_ && last_measured_at_) {
    UpdateMajorAllocation(
        std::max<double>(0, memory - last_measured_memory_.value()),
        time - last_measured_at_.value());
  }
  last_measured_memory_ = memory;
  last_measured_at_ = time;
  RefreshLimit();
  PostHeartbeatTask();
}

HeartbeatTask::HeartbeatTask(Isolate* isolate, MemoryBalancer* mb)
    : CancelableTask(isolate), mb_(mb) {}

void HeartbeatTask::RunInternal() { mb_->HeartbeatUpdate(); }

}  // namespace internal
}  // namespace v8
