// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/coalesced-live-ranges.h"
#include "src/compiler/greedy-allocator.h"
#include "src/compiler/register-allocator.h"

namespace v8 {
namespace internal {
namespace compiler {

#define TRACE(...)                             \
  do {                                         \
    if (FLAG_trace_alloc) PrintF(__VA_ARGS__); \
  } while (false)


const float CoalescedLiveRanges::kAllocatedRangeMultiplier = 10.0;

void CoalescedLiveRanges::AllocateRange(LiveRange* range) {
  UpdateWeightAtAllocation(range);
  for (auto interval = range->first_interval(); interval != nullptr;
       interval = interval->next()) {
    storage().insert({interval->start(), interval->end(), range});
  }
}


void CoalescedLiveRanges::Remove(LiveRange* range) {
  for (auto interval = range->first_interval(); interval != nullptr;
       interval = interval->next()) {
    storage().erase({interval->start(), interval->end(), nullptr});
  }
  range->UnsetAssignedRegister();
}


float CoalescedLiveRanges::GetMaximumConflictingWeight(
    const LiveRange* range) const {
  float ret = LiveRange::kInvalidWeight;
  auto end = storage().end();
  for (auto query = range->first_interval(); query != nullptr;
       query = query->next()) {
    auto conflict = GetFirstConflict(query);

    if (conflict == end) continue;
    for (; QueryIntersectsAllocatedInterval(query, conflict); ++conflict) {
      // It is possible we'll visit the same range multiple times, because
      // successive (not necessarily consecutive) intervals belong to the same
      // range, or because different intervals of the query range have the same
      // range as conflict.
      DCHECK_NE(conflict->range->weight(), LiveRange::kInvalidWeight);
      ret = Max(ret, conflict->range->weight());
      if (ret == LiveRange::kMaxWeight) break;
    }
  }
  return ret;
}


void CoalescedLiveRanges::EvictAndRescheduleConflicts(
    LiveRange* range, AllocationScheduler* scheduler) {
  auto end = storage().end();

  for (auto query = range->first_interval(); query != nullptr;
       query = query->next()) {
    auto conflict = GetFirstConflict(query);
    if (conflict == end) continue;
    while (QueryIntersectsAllocatedInterval(query, conflict)) {
      LiveRange* range_to_evict = conflict->range;
      // Bypass successive intervals belonging to the same range, because we're
      // about to remove this range, and we don't want the storage iterator to
      // become invalid.
      while (conflict != end && conflict->range == range_to_evict) {
        ++conflict;
      }

      DCHECK(range_to_evict->HasRegisterAssigned());
      CHECK(!range_to_evict->IsFixed());
      Remove(range_to_evict);
      UpdateWeightAtEviction(range_to_evict);
      TRACE("Evicted range %d.\n", range_to_evict->id());
      scheduler->Schedule(range_to_evict);
    }
  }
}


bool CoalescedLiveRanges::VerifyAllocationsAreValid() const {
  LifetimePosition last_end = LifetimePosition::GapFromInstructionIndex(0);
  for (auto i : storage_) {
    if (i.start < last_end) {
      return false;
    }
    last_end = i.end;
  }
  return true;
}


void CoalescedLiveRanges::UpdateWeightAtAllocation(LiveRange* range) {
  DCHECK_NE(range->weight(), LiveRange::kInvalidWeight);
  range->set_weight(range->weight() * kAllocatedRangeMultiplier);
}


void CoalescedLiveRanges::UpdateWeightAtEviction(LiveRange* range) {
  DCHECK_NE(range->weight(), LiveRange::kInvalidWeight);
  range->set_weight(range->weight() / kAllocatedRangeMultiplier);
}


CoalescedLiveRanges::interval_iterator CoalescedLiveRanges::GetFirstConflict(
    const UseInterval* query) const {
  DCHECK(query != nullptr);
  auto end = storage().end();
  LifetimePosition q_start = query->start();
  LifetimePosition q_end = query->end();

  if (storage().empty() || storage().rbegin()->end <= q_start ||
      storage().begin()->start >= q_end) {
    return end;
  }

  auto ret = storage().upper_bound(AsAllocatedInterval(q_start));
  // ret is either at the end (no start strictly greater than q_start) or
  // at some position with the aforementioned property. In either case, the
  // allocated interval before this one may intersect our query:
  // either because, although it starts before this query's start, it ends
  // after; or because it starts exactly at the query start. So unless we're
  // right at the beginning of the storage - meaning the first allocated
  // interval is also starting after this query's start - see what's behind.
  if (ret != storage().begin()) {
    --ret;
    if (!QueryIntersectsAllocatedInterval(query, ret)) {
      // The interval behind wasn't intersecting, so move back.
      ++ret;
    }
  }
  if (ret != end && QueryIntersectsAllocatedInterval(query, ret)) return ret;
  return end;
}


}  // namespace compiler
}  // namespace internal
}  // namespace v8
