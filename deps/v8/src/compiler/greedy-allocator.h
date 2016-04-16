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
  explicit AllocationCandidate(LiveRange* range)
      : is_group_(false), size_(range->GetSize()) {
    candidate_.range_ = range;
  }

  explicit AllocationCandidate(LiveRangeGroup* ranges)
      : is_group_(true), size_(CalculateGroupSize(ranges)) {
    candidate_.group_ = ranges;
  }

  // Strict ordering operators
  bool operator<(const AllocationCandidate& other) const {
    return size() < other.size();
  }

  bool operator>(const AllocationCandidate& other) const {
    return size() > other.size();
  }

  bool is_group() const { return is_group_; }
  LiveRange* live_range() const { return candidate_.range_; }
  LiveRangeGroup* group() const { return candidate_.group_; }

 private:
  unsigned CalculateGroupSize(LiveRangeGroup* group) {
    unsigned ret = 0;
    for (LiveRange* range : group->ranges()) {
      ret += range->GetSize();
    }
    return ret;
  }

  unsigned size() const { return size_; }
  bool is_group_;
  unsigned size_;
  union {
    LiveRange* range_;
    LiveRangeGroup* group_;
  } candidate_;
};


// Schedule processing (allocating) of AllocationCandidates.
class AllocationScheduler final : ZoneObject {
 public:
  explicit AllocationScheduler(Zone* zone) : queue_(zone) {}
  void Schedule(LiveRange* range);
  void Schedule(LiveRangeGroup* group);
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
  static const float kAllocatedRangeMultiplier;

  static void UpdateWeightAtAllocation(LiveRange* range) {
    DCHECK_NE(range->weight(), LiveRange::kInvalidWeight);
    range->set_weight(range->weight() * kAllocatedRangeMultiplier);
  }


  static void UpdateWeightAtEviction(LiveRange* range) {
    DCHECK_NE(range->weight(), LiveRange::kInvalidWeight);
    range->set_weight(range->weight() / kAllocatedRangeMultiplier);
  }

  AllocationScheduler& scheduler() { return scheduler_; }
  CoalescedLiveRanges* current_allocations(unsigned i) {
    return allocations_[i];
  }

  CoalescedLiveRanges* current_allocations(unsigned i) const {
    return allocations_[i];
  }

  Zone* local_zone() const { return local_zone_; }
  ZoneVector<LiveRangeGroup*>& groups() { return groups_; }
  const ZoneVector<LiveRangeGroup*>& groups() const { return groups_; }

  // Insert fixed ranges.
  void PreallocateFixedRanges();

  void GroupLiveRanges();

  // Schedule unassigned live ranges for allocation.
  void ScheduleAllocationCandidates();

  void AllocateRegisterToRange(unsigned reg_id, LiveRange* range) {
    UpdateWeightAtAllocation(range);
    current_allocations(reg_id)->AllocateRange(range);
  }
  // Evict and reschedule conflicts of a given range, at a given register.
  void EvictAndRescheduleConflicts(unsigned reg_id, const LiveRange* range);

  void TryAllocateCandidate(const AllocationCandidate& candidate);
  void TryAllocateLiveRange(LiveRange* range);
  void TryAllocateGroup(LiveRangeGroup* group);

  // Calculate the weight of a candidate for allocation.
  void EnsureValidRangeWeight(LiveRange* range);

  // Calculate the new weight of a range that is about to be allocated.
  float GetAllocatedRangeWeight(float candidate_weight);

  // Returns kInvalidWeight if there are no conflicts, or the largest weight of
  // a range conflicting with the given range, at the given register.
  float GetMaximumConflictingWeight(unsigned reg_id, const LiveRange* range,
                                    float competing_weight) const;

  // Returns kInvalidWeight if there are no conflicts, or the largest weight of
  // a range conflicting with the given range, at the given register.
  float GetMaximumConflictingWeight(unsigned reg_id,
                                    const LiveRangeGroup* group,
                                    float group_weight) const;

  // This is the extension point for splitting heuristics.
  void SplitOrSpillBlockedRange(LiveRange* range);

  // Find a good position where to fill, after a range was spilled after a call.
  LifetimePosition FindSplitPositionAfterCall(const LiveRange* range,
                                              int call_index);
  // Split a range around all calls it passes over. Returns true if any changes
  // were made, or false if no calls were found.
  bool TrySplitAroundCalls(LiveRange* range);

  // Find a split position at the outmost loop.
  LifetimePosition FindSplitPositionBeforeLoops(LiveRange* range);

  // Finds the first call instruction in the path of this range. Splits before
  // and requeues that segment (if any), spills the section over the call, and
  // returns the section after the call. The return is:
  // - same range, if no call was found
  // - nullptr, if the range finished at the call and there's no "after the
  //   call" portion.
  // - the portion after the call.
  LiveRange* GetRemainderAfterSplittingAroundFirstCall(LiveRange* range);

  // While we attempt to merge spill ranges later on in the allocation pipeline,
  // we want to ensure group elements get merged. Waiting until later may hinder
  // merge-ability, since the pipeline merger (being naive) may create conflicts
  // between spill ranges of group members.
  void TryReuseSpillRangesForGroups();

  LifetimePosition GetLastResortSplitPosition(const LiveRange* range);

  bool IsProgressPossible(const LiveRange* range);

  // Necessary heuristic: spill when all else failed.
  void SpillRangeAsLastResort(LiveRange* range);

  void AssignRangeToRegister(int reg_id, LiveRange* range);

  Zone* local_zone_;
  ZoneVector<CoalescedLiveRanges*> allocations_;
  AllocationScheduler scheduler_;
  ZoneVector<LiveRangeGroup*> groups_;

  DISALLOW_COPY_AND_ASSIGN(GreedyAllocator);
};
}  // namespace compiler
}  // namespace internal
}  // namespace v8
#endif  // V8_GREEDY_ALLOCATOR_H_
