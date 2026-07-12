// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_HEAP_CPPGC_HEAP_GROWING_H_
#define V8_HEAP_CPPGC_HEAP_GROWING_H_

#include "include/cppgc/heap.h"
#include "src/base/macros.h"
#include "src/heap/cppgc/globals.h"
#include "src/heap/cppgc/raw-heap.h"

namespace cppgc {

class Platform;

namespace internal {

class GarbageCollector;
class StatsCollector;

// Growing strategy that invokes garbage collection using GarbageCollector based
// on allocation statistics provided by StatsCollector and ResourceConstraints.
//
// Implements a fixed-ratio growing strategy with an initial heap size that the
// GC can ignore to avoid excessive GCs for smaller heaps.
class V8_EXPORT_PRIVATE HeapGrowing final {
 public:
  // Constant growing factor for growing the heap limit.
  static constexpr double kGrowingFactor = 1.5;
  // For smaller heaps, allow allocating at least LAB in each regular space
  // before triggering GC again.
  static constexpr size_t kMinLimitIncrease =
      kPageSize * RawHeap::kNumberOfRegularSpaces;

  HeapGrowing(GarbageCollector*, StatsCollector*,
              cppgc::Heap::ResourceConstraints, cppgc::Heap::MarkingType,
              cppgc::Heap::SweepingType);
  ~HeapGrowing();

  HeapGrowing(const HeapGrowing&) = delete;
  HeapGrowing& operator=(const HeapGrowing&) = delete;

  size_t limit_for_atomic_gc() const;
  size_t limit_for_incremental_gc() const;

  void DisableForTesting();

 private:
  class HeapGrowingImpl;
  std::unique_ptr<HeapGrowingImpl> impl_;
};

}  // namespace internal
}  // namespace cppgc

#endif  // V8_HEAP_CPPGC_HEAP_GROWING_H_
