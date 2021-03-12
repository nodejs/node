// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_HEAP_CPPGC_PROCESS_HEAP_STATISTICS_H_
#define V8_HEAP_CPPGC_PROCESS_HEAP_STATISTICS_H_

#include "include/cppgc/process-heap-statistics.h"
#include "src/heap/cppgc/stats-collector.h"

namespace cppgc {
namespace internal {

class ProcessHeapStatisticsUpdater {
 public:
  // Allocation observer implementation for heaps should register to contribute
  // to ProcessHeapStatistics. The heap is responsible for allocating and
  // registering the obsrever impl with its stats collector.
  class AllocationObserverImpl final
      : public StatsCollector::AllocationObserver {
   public:
    void AllocatedObjectSizeIncreased(size_t bytes) final {
      ProcessHeapStatisticsUpdater::IncreaseTotalAllocatedObjectSize(bytes);
      object_size_changes_since_last_reset_ += bytes;
    }

    void AllocatedObjectSizeDecreased(size_t bytes) final {
      ProcessHeapStatisticsUpdater::DecreaseTotalAllocatedObjectSize(bytes);
      object_size_changes_since_last_reset_ -= bytes;
    }

    void ResetAllocatedObjectSize(size_t bytes) final {
      ProcessHeapStatisticsUpdater::DecreaseTotalAllocatedObjectSize(
          object_size_changes_since_last_reset_);
      ProcessHeapStatisticsUpdater::IncreaseTotalAllocatedObjectSize(bytes);
      object_size_changes_since_last_reset_ = bytes;
    }

    void AllocatedSizeIncreased(size_t bytes) final {
      ProcessHeapStatisticsUpdater::IncreaseTotalAllocatedSpace(bytes);
    }

    void AllocatedSizeDecreased(size_t bytes) final {
      ProcessHeapStatisticsUpdater::DecreaseTotalAllocatedSpace(bytes);
    }

   private:
    size_t object_size_changes_since_last_reset_ = 0;
  };

  // For cppgc::ProcessHeapStatistics
  static void IncreaseTotalAllocatedObjectSize(size_t delta) {
    ::cppgc::ProcessHeapStatistics::total_allocated_object_size_.fetch_add(
        delta, std::memory_order_relaxed);
  }
  static void DecreaseTotalAllocatedObjectSize(size_t delta) {
    ::cppgc::ProcessHeapStatistics::total_allocated_object_size_.fetch_sub(
        delta, std::memory_order_relaxed);
  }
  static void IncreaseTotalAllocatedSpace(size_t delta) {
    ::cppgc::ProcessHeapStatistics::total_allocated_space_.fetch_add(
        delta, std::memory_order_relaxed);
  }
  static void DecreaseTotalAllocatedSpace(size_t delta) {
    ::cppgc::ProcessHeapStatistics::total_allocated_space_.fetch_sub(
        delta, std::memory_order_relaxed);
  }
};

}  // namespace internal
}  // namespace cppgc

#endif  // V8_HEAP_CPPGC_PROCESS_HEAP_STATISTICS_H_
