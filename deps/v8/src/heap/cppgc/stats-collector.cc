// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/heap/cppgc/stats-collector.h"

#include <algorithm>
#include <cmath>

#include "src/base/logging.h"

namespace cppgc {
namespace internal {

// static
constexpr size_t StatsCollector::kAllocationThresholdBytes;

void StatsCollector::RegisterObserver(AllocationObserver* observer) {
  DCHECK_EQ(allocation_observers_.end(),
            std::find(allocation_observers_.begin(),
                      allocation_observers_.end(), observer));
  allocation_observers_.push_back(observer);
}

void StatsCollector::UnregisterObserver(AllocationObserver* observer) {
  auto it = std::find(allocation_observers_.begin(),
                      allocation_observers_.end(), observer);
  DCHECK_NE(allocation_observers_.end(), it);
  allocation_observers_.erase(it);
}

void StatsCollector::NotifyAllocation(size_t bytes) {
  // The current GC may not have been started. This is ok as recording considers
  // the whole time range between garbage collections.
  allocated_bytes_since_safepoint_ += bytes;
}

void StatsCollector::NotifyExplicitFree(size_t bytes) {
  // See IncreaseAllocatedObjectSize for lifetime of the counter.
  explicitly_freed_bytes_since_safepoint_ += bytes;
}

void StatsCollector::NotifySafePointForConservativeCollection() {
  if (std::abs(allocated_bytes_since_safepoint_ -
               explicitly_freed_bytes_since_safepoint_) >=
      static_cast<int64_t>(kAllocationThresholdBytes)) {
    AllocatedObjectSizeSafepointImpl();
  }
}

void StatsCollector::AllocatedObjectSizeSafepointImpl() {
  allocated_bytes_since_end_of_marking_ +=
      static_cast<int64_t>(allocated_bytes_since_safepoint_) -
      static_cast<int64_t>(explicitly_freed_bytes_since_safepoint_);

  // These observer methods may start or finalize GC. In case they trigger a
  // final GC pause, the delta counters are reset there and the following
  // observer calls are called with '0' updates.
  ForAllAllocationObservers([this](AllocationObserver* observer) {
    // Recompute delta here so that a GC finalization is able to clear the
    // delta for other observer calls.
    int64_t delta = allocated_bytes_since_safepoint_ -
                    explicitly_freed_bytes_since_safepoint_;
    if (delta < 0) {
      observer->AllocatedObjectSizeDecreased(static_cast<size_t>(-delta));
    } else {
      observer->AllocatedObjectSizeIncreased(static_cast<size_t>(delta));
    }
  });
  allocated_bytes_since_safepoint_ = 0;
  explicitly_freed_bytes_since_safepoint_ = 0;
}

void StatsCollector::NotifyMarkingStarted() {
  DCHECK_EQ(GarbageCollectionState::kNotRunning, gc_state_);
  gc_state_ = GarbageCollectionState::kMarking;
}

void StatsCollector::NotifyMarkingCompleted(size_t marked_bytes) {
  DCHECK_EQ(GarbageCollectionState::kMarking, gc_state_);
  gc_state_ = GarbageCollectionState::kSweeping;
  current_.marked_bytes = marked_bytes;
  allocated_bytes_since_safepoint_ = 0;
  explicitly_freed_bytes_since_safepoint_ = 0;

  ForAllAllocationObservers([marked_bytes](AllocationObserver* observer) {
    observer->ResetAllocatedObjectSize(marked_bytes);
  });

  // HeapGrowing would use the below fields to estimate allocation rate during
  // execution of ResetAllocatedObjectSize.
  allocated_bytes_since_end_of_marking_ = 0;
  time_of_last_end_of_marking_ = v8::base::TimeTicks::Now();
}

double StatsCollector::GetRecentAllocationSpeedInBytesPerMs() const {
  v8::base::TimeTicks current_time = v8::base::TimeTicks::Now();
  DCHECK_LE(time_of_last_end_of_marking_, current_time);
  if (time_of_last_end_of_marking_ == current_time) return 0;
  return allocated_bytes_since_end_of_marking_ /
         (current_time - time_of_last_end_of_marking_).InMillisecondsF();
}

const StatsCollector::Event& StatsCollector::NotifySweepingCompleted() {
  DCHECK_EQ(GarbageCollectionState::kSweeping, gc_state_);
  gc_state_ = GarbageCollectionState::kNotRunning;
  previous_ = std::move(current_);
  current_ = Event();
  return previous_;
}

size_t StatsCollector::allocated_object_size() const {
  // During sweeping we refer to the current Event as that already holds the
  // correct marking information. In all other phases, the previous event holds
  // the most up-to-date marking information.
  const Event& event =
      gc_state_ == GarbageCollectionState::kSweeping ? current_ : previous_;
  DCHECK_GE(static_cast<int64_t>(event.marked_bytes) +
                allocated_bytes_since_end_of_marking_,
            0);
  return static_cast<size_t>(static_cast<int64_t>(event.marked_bytes) +
                             allocated_bytes_since_end_of_marking_);
}

}  // namespace internal
}  // namespace cppgc
