// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/coalesced-live-ranges.h"
#include "src/compiler/greedy-allocator.h"
#include "src/compiler/register-allocator.h"

namespace v8 {
namespace internal {
namespace compiler {


LiveRangeConflictIterator::LiveRangeConflictIterator(const LiveRange* range,
                                                     IntervalStore* storage)
    : query_(range->first_interval()),
      pos_(storage->end()),
      intervals_(storage) {
  MovePosAndQueryToFirstConflict();
}


LiveRange* LiveRangeConflictIterator::Current() const {
  if (IsFinished()) return nullptr;
  return pos_->range_;
}


void LiveRangeConflictIterator::MovePosToFirstConflictForQuery() {
  DCHECK_NOT_NULL(query_);
  auto end = intervals_->end();
  LifetimePosition q_start = query_->start();
  LifetimePosition q_end = query_->end();

  if (intervals_->empty() || intervals_->rbegin()->end_ <= q_start ||
      intervals_->begin()->start_ >= q_end) {
    pos_ = end;
    return;
  }

  pos_ = intervals_->upper_bound(AsAllocatedInterval(q_start));
  // pos is either at the end (no start strictly greater than q_start) or
  // at some position with the aforementioned property. In either case, the
  // allocated interval before this one may intersect our query:
  // either because, although it starts before this query's start, it ends
  // after; or because it starts exactly at the query start. So unless we're
  // right at the beginning of the storage - meaning the first allocated
  // interval is also starting after this query's start - see what's behind.
  if (pos_ != intervals_->begin()) {
    --pos_;
    if (!QueryIntersectsAllocatedInterval()) {
      // The interval behind wasn't intersecting, so move back.
      ++pos_;
    }
  }
  if (pos_ == end || !QueryIntersectsAllocatedInterval()) {
    pos_ = end;
  }
}


void LiveRangeConflictIterator::MovePosAndQueryToFirstConflict() {
  auto end = intervals_->end();
  for (; query_ != nullptr; query_ = query_->next()) {
    MovePosToFirstConflictForQuery();
    if (pos_ != end) {
      DCHECK(QueryIntersectsAllocatedInterval());
      return;
    }
  }

  Invalidate();
}


void LiveRangeConflictIterator::IncrementPosAndSkipOverRepetitions() {
  auto end = intervals_->end();
  DCHECK(pos_ != end);
  LiveRange* current_conflict = Current();
  while (pos_ != end && pos_->range_ == current_conflict) {
    ++pos_;
  }
}


LiveRange* LiveRangeConflictIterator::InternalGetNext(bool clean_behind) {
  if (IsFinished()) return nullptr;

  LiveRange* to_clear = Current();
  IncrementPosAndSkipOverRepetitions();
  // At this point, pos_ is either at the end, or on an interval that doesn't
  // correspond to the same range as to_clear. This interval may not even be
  // a conflict.
  if (clean_behind) {
    // Since we parked pos_ on an iterator that won't be affected by removal,
    // we can safely delete to_clear's intervals.
    for (auto interval = to_clear->first_interval(); interval != nullptr;
         interval = interval->next()) {
      AllocatedInterval erase_key(interval->start(), interval->end(), nullptr);
      intervals_->erase(erase_key);
    }
  }
  // We may have parked pos_ at the end, or on a non-conflict. In that case,
  // move to the next query and reinitialize pos and query. This may invalidate
  // the iterator, if no more conflicts are available.
  if (!QueryIntersectsAllocatedInterval()) {
    query_ = query_->next();
    MovePosAndQueryToFirstConflict();
  }
  return Current();
}


LiveRangeConflictIterator CoalescedLiveRanges::GetConflicts(
    const LiveRange* range) {
  return LiveRangeConflictIterator(range, &intervals());
}


void CoalescedLiveRanges::AllocateRange(LiveRange* range) {
  for (auto interval = range->first_interval(); interval != nullptr;
       interval = interval->next()) {
    AllocatedInterval to_insert(interval->start(), interval->end(), range);
    intervals().insert(to_insert);
  }
}


bool CoalescedLiveRanges::VerifyAllocationsAreValidForTesting() const {
  LifetimePosition last_end = LifetimePosition::GapFromInstructionIndex(0);
  for (auto i : intervals_) {
    if (i.start_ < last_end) {
      return false;
    }
    last_end = i.end_;
  }
  return true;
}


}  // namespace compiler
}  // namespace internal
}  // namespace v8
