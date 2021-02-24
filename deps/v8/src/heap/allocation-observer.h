// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_HEAP_ALLOCATION_OBSERVER_H_
#define V8_HEAP_ALLOCATION_OBSERVER_H_

#include <cstdint>
#include <unordered_set>
#include <vector>

#include "src/common/globals.h"

namespace v8 {
namespace internal {

class AllocationObserver;

class AllocationCounter {
 public:
  AllocationCounter()
      : paused_(false),
        current_counter_(0),
        next_counter_(0),
        step_in_progress_(false) {}
  V8_EXPORT_PRIVATE void AddAllocationObserver(AllocationObserver* observer);
  V8_EXPORT_PRIVATE void RemoveAllocationObserver(AllocationObserver* observer);

  bool IsActive() { return !IsPaused() && observers_.size() > 0; }

  void Pause() {
    DCHECK(!paused_);
    DCHECK(!step_in_progress_);
    paused_ = true;
  }

  void Resume() {
    DCHECK(paused_);
    DCHECK(!step_in_progress_);
    paused_ = false;
  }

  V8_EXPORT_PRIVATE void AdvanceAllocationObservers(size_t allocated);
  V8_EXPORT_PRIVATE void InvokeAllocationObservers(Address soon_object,
                                                   size_t object_size,
                                                   size_t aligned_object_size);

  size_t NextBytes() {
    DCHECK(IsActive());
    return next_counter_ - current_counter_;
  }

  bool IsStepInProgress() { return step_in_progress_; }

 private:
  bool IsPaused() { return paused_; }

  struct AllocationObserverCounter {
    AllocationObserverCounter(AllocationObserver* observer, size_t prev_counter,
                              size_t next_counter)
        : observer_(observer),
          prev_counter_(prev_counter),
          next_counter_(next_counter) {}

    AllocationObserver* observer_;
    size_t prev_counter_;
    size_t next_counter_;
  };

  std::vector<AllocationObserverCounter> observers_;
  std::vector<AllocationObserverCounter> pending_added_;
  std::unordered_set<AllocationObserver*> pending_removed_;

  bool paused_;

  size_t current_counter_;
  size_t next_counter_;

  bool step_in_progress_;
};

// -----------------------------------------------------------------------------
// Allows observation of allocations.
class AllocationObserver {
 public:
  explicit AllocationObserver(intptr_t step_size) : step_size_(step_size) {
    DCHECK_LE(kTaggedSize, step_size);
  }
  virtual ~AllocationObserver() = default;
  AllocationObserver(const AllocationObserver&) = delete;
  AllocationObserver& operator=(const AllocationObserver&) = delete;

 protected:
  // Pure virtual method provided by the subclasses that gets called when at
  // least step_size bytes have been allocated. soon_object is the address just
  // allocated (but not yet initialized.) size is the size of the object as
  // requested (i.e. w/o the alignment fillers). Some complexities to be aware
  // of:
  // 1) soon_object will be nullptr in cases where we end up observing an
  //    allocation that happens to be a filler space (e.g. page boundaries.)
  // 2) size is the requested size at the time of allocation. Right-trimming
  //    may change the object size dynamically.
  // 3) soon_object may actually be the first object in an allocation-folding
  //    group. In such a case size is the size of the group rather than the
  //    first object.
  virtual void Step(int bytes_allocated, Address soon_object, size_t size) = 0;

  // Subclasses can override this method to make step size dynamic.
  virtual intptr_t GetNextStepSize() { return step_size_; }

 private:
  intptr_t step_size_;

  friend class AllocationCounter;
};

class V8_EXPORT_PRIVATE V8_NODISCARD PauseAllocationObserversScope {
 public:
  explicit PauseAllocationObserversScope(Heap* heap);
  ~PauseAllocationObserversScope();
  PauseAllocationObserversScope(const PauseAllocationObserversScope&) = delete;
  PauseAllocationObserversScope& operator=(
      const PauseAllocationObserversScope&) = delete;

 private:
  Heap* heap_;
};

}  // namespace internal
}  // namespace v8

#endif  // V8_HEAP_ALLOCATION_OBSERVER_H_
