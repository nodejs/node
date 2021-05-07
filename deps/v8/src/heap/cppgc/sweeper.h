// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_HEAP_CPPGC_SWEEPER_H_
#define V8_HEAP_CPPGC_SWEEPER_H_

#include <memory>

#include "include/cppgc/heap.h"
#include "src/base/macros.h"

namespace cppgc {

class Platform;

namespace internal {

class StatsCollector;
class RawHeap;
class ConcurrentSweeperTest;
class NormalPageSpace;

class V8_EXPORT_PRIVATE Sweeper final {
 public:
  struct SweepingConfig {
    using SweepingType = cppgc::Heap::SweepingType;
    enum class CompactableSpaceHandling { kSweep, kIgnore };

    SweepingType sweeping_type = SweepingType::kIncrementalAndConcurrent;
    CompactableSpaceHandling compactable_space_handling =
        CompactableSpaceHandling::kSweep;
  };

  Sweeper(RawHeap*, cppgc::Platform*, StatsCollector*);
  ~Sweeper();

  Sweeper(const Sweeper&) = delete;
  Sweeper& operator=(const Sweeper&) = delete;

  // Sweeper::Start assumes the heap holds no linear allocation buffers.
  void Start(SweepingConfig);
  void FinishIfRunning();
  void NotifyDoneIfNeeded();
  // SweepForAllocationIfRunning sweeps the given |space| until a slot that can
  // fit an allocation of size |size| is found. Returns true if a slot was
  // found.
  bool SweepForAllocationIfRunning(NormalPageSpace* space, size_t size);

  bool IsSweepingOnMutatorThread() const;
  bool IsSweepingInProgress() const;

 private:
  void WaitForConcurrentSweepingForTesting();

  class SweeperImpl;
  std::unique_ptr<SweeperImpl> impl_;

  friend class ConcurrentSweeperTest;
};

}  // namespace internal
}  // namespace cppgc

#endif  // V8_HEAP_CPPGC_SWEEPER_H_
