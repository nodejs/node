// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_GREEDY_ALLOCATOR_H_
#define V8_GREEDY_ALLOCATOR_H_

#include "src/compiler/coalesced-live-ranges.h"
#include "src/compiler/register-allocator.h"
#include "src/zone-containers.h"

namespace v8 {
namespace internal {
namespace compiler {


// The object of allocation scheduling. At minimum, this is a LiveRange, but
// we may extend this to groups of LiveRanges. It has to be comparable.
class AllocationCandidate {
 public:
  explicit AllocationCandidate(LiveRange* range) : range_(range) {}

  // Strict ordering operators
  bool operator<(const AllocationCandidate& other) const {
    return range_->GetSize() < other.range_->GetSize();
  }

  bool operator>(const AllocationCandidate& other) const {
    return range_->GetSize() > other.range_->GetSize();
  }

  LiveRange* live_range() const { return range_; }

 private:
  LiveRange* range_;
};


// Schedule processing (allocating) of AllocationCandidates.
class AllocationScheduler final : ZoneObject {
 public:
  explicit AllocationScheduler(Zone* zone) : queue_(zone) {}
  void Schedule(LiveRange* range);
  AllocationCandidate GetNext();
  bool empty() const { return queue_.empty(); }

 private:
  typedef ZonePriorityQueue<AllocationCandidate> ScheduleQueue;
  ScheduleQueue queue_;

  DISALLOW_COPY_AND_ASSIGN(AllocationScheduler);
};


// A variant of the LLVM Greedy Register Allocator. See
// http://blog.llvm.org/2011/09/greedy-register-allocation-in-llvm-30.html
class GreedyAllocator final : public RegisterAllocator {
 public:
  explicit GreedyAllocator(RegisterAllocationData* data, RegisterKind kind,
                           Zone* local_zone);

  void AllocateRegisters();

 private:
  AllocationScheduler& scheduler() { return scheduler_; }
  CoalescedLiveRanges* current_allocations(unsigned i) {
    return allocations_[i];
  }
  Zone* local_zone() const { return local_zone_; }

  // Insert fixed ranges.
  void PreallocateFixedRanges();

  // Schedule unassigned live ranges for allocation.
  // TODO(mtrofin): groups.
  void ScheduleAllocationCandidates();

  // Find the optimal split for ranges defined by a memory operand, e.g.
  // constants or function parameters passed on the stack.
  void SplitAndSpillRangesDefinedByMemoryOperand();

  void TryAllocateCandidate(const AllocationCandidate& candidate);
  void TryAllocateLiveRange(LiveRange* range);

  bool CanProcessRange(LiveRange* range) const {
    return range != nullptr && !range->IsEmpty() && range->kind() == mode();
  }

  // Calculate the weight of a candidate for allocation.
  void EnsureValidRangeWeight(LiveRange* range);

  // Calculate the new weight of a range that is about to be allocated.
  float GetAllocatedRangeWeight(float candidate_weight);

  // This is the extension point for splitting heuristics.
  void SplitOrSpillBlockedRange(LiveRange* range);

  // Necessary heuristic: spill when all else failed.
  void SpillRangeAsLastResort(LiveRange* range);

  void AssignRangeToRegister(int reg_id, LiveRange* range);

  Zone* local_zone_;
  ZoneVector<CoalescedLiveRanges*> allocations_;
  AllocationScheduler scheduler_;
  DISALLOW_COPY_AND_ASSIGN(GreedyAllocator);
};
}  // namespace compiler
}  // namespace internal
}  // namespace v8
#endif  // V8_GREEDY_ALLOCATOR_H_
