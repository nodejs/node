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


// Implementation detail for CoalescedLiveRanges.
struct AllocatedInterval {
  AllocatedInterval(LifetimePosition start, LifetimePosition end,
                    LiveRange* range)
      : start_(start), end_(end), range_(range) {}

  LifetimePosition start_;
  LifetimePosition end_;
  LiveRange* range_;
  bool operator<(const AllocatedInterval& other) const {
    return start_ < other.start_;
  }
  bool operator>(const AllocatedInterval& other) const {
    return start_ > other.start_;
  }
};
typedef ZoneSet<AllocatedInterval> IntervalStore;


// An iterator over conflicts of a live range, obtained from CoalescedLiveRanges
// The design supports two main scenarios (see GreedyAllocator):
// (1) observing each conflicting range, without mutating the allocations, and
// (2) observing each conflicting range, and then moving to the next, after
// removing the current conflict.
class LiveRangeConflictIterator {
 public:
  // Current conflict. nullptr if no conflicts, or if we reached the end of
  // conflicts.
  LiveRange* Current() const;

  // Get the next conflict. Caller should handle non-consecutive repetitions of
  // the same range.
  LiveRange* GetNext() { return InternalGetNext(false); }

  // Get the next conflict, after evicting the current one. Caller may expect
  // to never observe the same live range more than once.
  LiveRange* RemoveCurrentAndGetNext() { return InternalGetNext(true); }

 private:
  friend class CoalescedLiveRanges;

  typedef IntervalStore::const_iterator interval_iterator;
  LiveRangeConflictIterator(const LiveRange* range, IntervalStore* store);

  // Move the store iterator to  first interval intersecting query. Since the
  // intervals are sorted, subsequent intervals intersecting query follow. May
  // leave the store iterator at "end", meaning that the current query does not
  // have an intersection.
  void MovePosToFirstConflictForQuery();

  // Move both query and store iterator to the first intersection, if any. If
  // none, then it invalidates the iterator (IsFinished() == true)
  void MovePosAndQueryToFirstConflict();

  // Increment pos and skip over intervals belonging to the same range we
  // started with (i.e. Current() before the call). It is possible that range
  // will be seen again, but not consecutively.
  void IncrementPosAndSkipOverRepetitions();

  // Common implementation used by both GetNext as well as
  // ClearCurrentAndGetNext.
  LiveRange* InternalGetNext(bool clean_behind);

  bool IsFinished() const { return query_ == nullptr; }

  static AllocatedInterval AsAllocatedInterval(LifetimePosition pos) {
    return AllocatedInterval(pos, LifetimePosition::Invalid(), nullptr);
  }

  // Intersection utilities.
  static bool Intersects(LifetimePosition a_start, LifetimePosition a_end,
                         LifetimePosition b_start, LifetimePosition b_end) {
    return a_start < b_end && b_start < a_end;
  }

  bool QueryIntersectsAllocatedInterval() const {
    DCHECK_NOT_NULL(query_);
    return pos_ != intervals_->end() &&
           Intersects(query_->start(), query_->end(), pos_->start_, pos_->end_);
  }

  void Invalidate() {
    query_ = nullptr;
    pos_ = intervals_->end();
  }

  const UseInterval* query_;
  interval_iterator pos_;
  IntervalStore* intervals_;
};

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
  explicit CoalescedLiveRanges(Zone* zone) : intervals_(zone) {}
  void clear() { intervals_.clear(); }

  bool empty() const { return intervals_.empty(); }

  // Iterate over each live range conflicting with the provided one.
  // The same live range may be observed multiple, but non-consecutive times.
  LiveRangeConflictIterator GetConflicts(const LiveRange* range);


  // Allocates a range with a pre-calculated candidate weight.
  void AllocateRange(LiveRange* range);

  // Unit testing API, verifying that allocated intervals do not overlap.
  bool VerifyAllocationsAreValidForTesting() const;

 private:
  static const float kAllocatedRangeMultiplier;

  IntervalStore& intervals() { return intervals_; }
  const IntervalStore& intervals() const { return intervals_; }

  // Augment the weight of a range that is about to be allocated.
  static void UpdateWeightAtAllocation(LiveRange* range);

  // Reduce the weight of a range that has lost allocation.
  static void UpdateWeightAtEviction(LiveRange* range);


  IntervalStore intervals_;
  DISALLOW_COPY_AND_ASSIGN(CoalescedLiveRanges);
};


}  // namespace compiler
}  // namespace internal
}  // namespace v8
#endif  // V8_COALESCED_LIVE_RANGES_H_
