// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/heap/cppgc/stats-collector.h"

#include <algorithm>
#include <cmath>

#include "src/base/logging.h"
#include "src/heap/cppgc/metric-recorder.h"

namespace cppgc {
namespace internal {

// static
constexpr size_t StatsCollector::kAllocationThresholdBytes;

StatsCollector::StatsCollector(
    std::unique_ptr<MetricRecorder> histogram_recorder, Platform* platform)
    : metric_recorder_(std::move(histogram_recorder)), platform_(platform) {
  USE(platform_);
}

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

StatsCollector::Event::Event() {
  static std::atomic<size_t> epoch_counter{0};
  epoch = epoch_counter.fetch_add(1);
}

void StatsCollector::NotifyMarkingStarted(CollectionType collection_type,
                                          IsForcedGC is_forced_gc) {
  DCHECK_EQ(GarbageCollectionState::kNotRunning, gc_state_);
  current_.collection_type = collection_type;
  current_.is_forced_gc = is_forced_gc;
  gc_state_ = GarbageCollectionState::kMarking;
}

void StatsCollector::NotifyMarkingCompleted(size_t marked_bytes) {
  DCHECK_EQ(GarbageCollectionState::kMarking, gc_state_);
  gc_state_ = GarbageCollectionState::kSweeping;
  current_.marked_bytes = marked_bytes;
  current_.object_size_before_sweep_bytes =
      previous_.marked_bytes + allocated_bytes_since_end_of_marking_ +
      allocated_bytes_since_safepoint_ -
      explicitly_freed_bytes_since_safepoint_;
  allocated_bytes_since_safepoint_ = 0;
  explicitly_freed_bytes_since_safepoint_ = 0;

  DCHECK_LE(memory_freed_bytes_since_end_of_marking_, memory_allocated_bytes_);
  memory_allocated_bytes_ -= memory_freed_bytes_since_end_of_marking_;
  current_.memory_size_before_sweep_bytes = memory_allocated_bytes_;
  memory_freed_bytes_since_end_of_marking_ = 0;

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

namespace {

int64_t SumPhases(const MetricRecorder::CppGCFullCycle::Phases& phases) {
  return phases.mark_duration_us + phases.weak_duration_us +
         phases.compact_duration_us + phases.sweep_duration_us;
}

MetricRecorder::CppGCFullCycle GetFullCycleEventForMetricRecorder(
    int64_t atomic_mark_us, int64_t atomic_weak_us, int64_t atomic_compact_us,
    int64_t atomic_sweep_us, int64_t incremental_mark_us,
    int64_t incremental_sweep_us, int64_t concurrent_mark_us,
    int64_t concurrent_sweep_us, int64_t objects_before_bytes,
    int64_t objects_after_bytes, int64_t objects_freed_bytes,
    int64_t memory_before_bytes, int64_t memory_after_bytes,
    int64_t memory_freed_bytes) {
  MetricRecorder::CppGCFullCycle event;
  // MainThread.Incremental:
  event.main_thread_incremental.mark_duration_us = incremental_mark_us;
  event.main_thread_incremental.sweep_duration_us = incremental_sweep_us;
  // MainThread.Atomic:
  event.main_thread_atomic.mark_duration_us = atomic_mark_us;
  event.main_thread_atomic.weak_duration_us = atomic_weak_us;
  event.main_thread_atomic.compact_duration_us = atomic_compact_us;
  event.main_thread_atomic.sweep_duration_us = atomic_sweep_us;
  // MainThread:
  event.main_thread.mark_duration_us =
      event.main_thread_atomic.mark_duration_us +
      event.main_thread_incremental.mark_duration_us;
  event.main_thread.weak_duration_us =
      event.main_thread_atomic.weak_duration_us;
  event.main_thread.compact_duration_us =
      event.main_thread_atomic.compact_duration_us;
  event.main_thread.sweep_duration_us =
      event.main_thread_atomic.sweep_duration_us +
      event.main_thread_incremental.sweep_duration_us;
  // Total:
  event.total.mark_duration_us =
      event.main_thread.mark_duration_us + concurrent_mark_us;
  event.total.weak_duration_us = event.main_thread.weak_duration_us;
  event.total.compact_duration_us = event.main_thread.compact_duration_us;
  event.total.sweep_duration_us =
      event.main_thread.sweep_duration_us + concurrent_sweep_us;
  // Objects:
  event.objects.before_bytes = objects_before_bytes;
  event.objects.after_bytes = objects_after_bytes;
  event.objects.freed_bytes = objects_freed_bytes;
  // Memory:
  event.memory.before_bytes = memory_before_bytes;
  event.memory.after_bytes = memory_after_bytes;
  event.memory.freed_bytes = memory_freed_bytes;
  // Collection Rate:
  event.collection_rate_in_percent =
      static_cast<double>(event.objects.after_bytes) /
      event.objects.before_bytes;
  // Efficiency:
  event.efficiency_in_bytes_per_us =
      static_cast<double>(event.objects.freed_bytes) / SumPhases(event.total);
  event.main_thread_efficiency_in_bytes_per_us =
      static_cast<double>(event.objects.freed_bytes) /
      SumPhases(event.main_thread);
  return event;
}

}  // namespace

void StatsCollector::NotifySweepingCompleted() {
  DCHECK_EQ(GarbageCollectionState::kSweeping, gc_state_);
  gc_state_ = GarbageCollectionState::kNotRunning;
  previous_ = std::move(current_);
  current_ = Event();
  if (metric_recorder_) {
    MetricRecorder::CppGCFullCycle event = GetFullCycleEventForMetricRecorder(
        previous_.scope_data[kAtomicMark].InMicroseconds(),
        previous_.scope_data[kAtomicWeak].InMicroseconds(),
        previous_.scope_data[kAtomicCompact].InMicroseconds(),
        previous_.scope_data[kAtomicSweep].InMicroseconds(),
        previous_.scope_data[kIncrementalMark].InMicroseconds(),
        previous_.scope_data[kIncrementalSweep].InMicroseconds(),
        previous_.concurrent_scope_data[kConcurrentMark],
        previous_.concurrent_scope_data[kConcurrentSweep],
        previous_.object_size_before_sweep_bytes /* objects_before */,
        previous_.marked_bytes /* objects_after */,
        previous_.object_size_before_sweep_bytes -
            previous_.marked_bytes /* objects_freed */,
        previous_.memory_size_before_sweep_bytes /* memory_before */,
        previous_.memory_size_before_sweep_bytes -
            memory_freed_bytes_since_end_of_marking_ /* memory_after */,
        memory_freed_bytes_since_end_of_marking_ /* memory_freed */);
    metric_recorder_->AddMainThreadEvent(event);
  }
}

size_t StatsCollector::allocated_memory_size() const {
  return memory_allocated_bytes_;
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

void StatsCollector::NotifyAllocatedMemory(int64_t size) {
  memory_allocated_bytes_ += size;
  ForAllAllocationObservers([size](AllocationObserver* observer) {
    observer->AllocatedSizeIncreased(static_cast<size_t>(size));
  });
}

void StatsCollector::NotifyFreedMemory(int64_t size) {
  memory_freed_bytes_since_end_of_marking_ += size;
  ForAllAllocationObservers([size](AllocationObserver* observer) {
    observer->AllocatedSizeDecreased(static_cast<size_t>(size));
  });
}

void StatsCollector::RecordHistogramSample(ScopeId scope_id_,
                                           v8::base::TimeDelta time) {
  switch (scope_id_) {
    case kIncrementalMark: {
      MetricRecorder::CppGCMainThreadIncrementalMark event{
          time.InMicroseconds()};
      metric_recorder_->AddMainThreadEvent(event);
      break;
    }
    case kIncrementalSweep: {
      MetricRecorder::CppGCMainThreadIncrementalSweep event{
          time.InMicroseconds()};
      metric_recorder_->AddMainThreadEvent(event);
      break;
    }
    default:
      break;
  }
}

}  // namespace internal
}  // namespace cppgc
