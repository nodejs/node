// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COALESCED_LIVE_RANGES_H_
#define V8_COALESCED_LIVE_RANGES_H_

#include "src/compiler/register-allocator.h"
#include "src/zone-containers.h"

namespace v8 {
namespace internal {
namespace compiler {


class AllocationScheduler;


// Collection of live ranges allocated to the same register.
// It supports efficiently finding all conflicts for a given, non-allocated
// range. See AllocatedInterval.
// Allocated live ranges do not intersect. At most, individual use intervals
// touch. We store, for a live range, an AllocatedInterval corresponding to each
// of that range's UseIntervals. We keep the list of AllocatedIntervals sorted
// by starts. Then, given the non-intersecting property, we know that
// consecutive AllocatedIntervals have the property that the "smaller"'s end is
// less or equal to the "larger"'s start.
// This allows for quick (logarithmic complexity) identification of the first
// AllocatedInterval to conflict with a given LiveRange, and then for efficient
// traversal of conflicts.
class CoalescedLiveRanges : public ZoneObject {
 public:
  explicit CoalescedLiveRanges(Zone* zone) : storage_(zone) {}
  void clear() { storage_.clear(); }

  bool empty() const { return storage_.empty(); }

  // Returns kInvalidWeight if there are no conflicts, or the largest weight of
  // a range conflicting with the given range.
  float GetMaximumConflictingWeight(const LiveRange* range) const;

  // Evicts all conflicts of the given range, and reschedules them with the
  // provided scheduler.
  void EvictAndRescheduleConflicts(LiveRange* range,
                                   AllocationScheduler* scheduler);

  // Allocates a range with a pre-calculated candidate weight.
  void AllocateRange(LiveRange* range);

  // TODO(mtrofin): remove this in favor of comprehensive unit tests.
  bool VerifyAllocationsAreValid() const;

 private:
  static const float kAllocatedRangeMultiplier;
  // Storage detail for CoalescedLiveRanges.
  struct AllocatedInterval {
    LifetimePosition start;
    LifetimePosition end;
    LiveRange* range;
    bool operator<(const AllocatedInterval& other) const {
      return start < other.start;
    }
    bool operator>(const AllocatedInterval& other) const {
      return start > other.start;
    }
  };
  typedef ZoneSet<AllocatedInterval> IntervalStore;
  typedef IntervalStore::const_iterator interval_iterator;

  IntervalStore& storage() { return storage_; }
  const IntervalStore& storage() const { return storage_; }

  // Augment the weight of a range that is about to be allocated.
  static void UpdateWeightAtAllocation(LiveRange* range);

  // Reduce the weight of a range that has lost allocation.
  static void UpdateWeightAtEviction(LiveRange* range);

  // Intersection utilities.
  static bool Intersects(LifetimePosition a_start, LifetimePosition a_end,
                         LifetimePosition b_start, LifetimePosition b_end) {
    return a_start < b_end && b_start < a_end;
  }
  static AllocatedInterval AsAllocatedInterval(LifetimePosition pos) {
    return {pos, LifetimePosition::Invalid(), nullptr};
  }

  bool QueryIntersectsAllocatedInterval(const UseInterval* query,
                                        interval_iterator& pos) const {
    DCHECK(query != nullptr);
    return pos != storage().end() &&
           Intersects(query->start(), query->end(), pos->start, pos->end);
  }

  void Remove(LiveRange* range);

  // Get the first interval intersecting query. Since the intervals are sorted,
  // subsequent intervals intersecting query follow.
  interval_iterator GetFirstConflict(const UseInterval* query) const;

  IntervalStore storage_;
  DISALLOW_COPY_AND_ASSIGN(CoalescedLiveRanges);
};


}  // namespace compiler
}  // namespace internal
}  // namespace v8
#endif  // V8_COALESCED_LIVE_RANGES_H_
