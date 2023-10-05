// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/sandbox/code-pointer-table.h"

#include "src/execution/isolate.h"
#include "src/logging/counters.h"
#include "src/sandbox/code-pointer-table-inl.h"

#ifdef V8_COMPRESS_POINTERS

namespace v8 {
namespace internal {

uint32_t CodePointerTable::Sweep(Space* space, Counters* counters) {
  DCHECK(space->BelongsTo(this));

  // TODO(saelo): this code is fairly similar to
  // ExternalPointerTable::SweepAndCompact. Investigate whether common code can
  // be factored out.

  // Lock the space. Technically this is not necessary since no other thread can
  // allocate entries at this point, but some of the methods we call on the
  // space assert that the lock is held.
  base::MutexGuard guard(&space->mutex_);

  // There must not be any entry allocations while the table is being swept as
  // that would not be safe. Set the freelist to this special marker value to
  // easily catch any violation of this requirement.
  space->freelist_head_.store(kEntryAllocationIsForbiddenMarker,
                              std::memory_order_relaxed);

  // Here we can iterate over the segments collection without taking a lock
  // because no other thread can currently allocate entries in this space.
  uint32_t current_freelist_head = 0;
  uint32_t current_freelist_length = 0;
  std::vector<Segment> segments_to_deallocate;
  for (auto segment : base::Reversed(space->segments_)) {
    // Remember the state of the freelist before this segment in case this
    // segment turns out to be completely empty and we deallocate it.
    uint32_t previous_freelist_head = current_freelist_head;
    uint32_t previous_freelist_length = current_freelist_length;

    // Process every entry in this segment, again going top to bottom.
    for (uint32_t i = segment.last_entry(); i >= segment.first_entry(); i--) {
      if (!at(i).IsMarked()) {
        at(i).MakeFreelistEntry(current_freelist_head);
        current_freelist_head = i;
        current_freelist_length++;
      } else {
        at(i).Unmark();
      }
    }

    // If a segment is completely empty, free it.
    uint32_t free_entries = current_freelist_length - previous_freelist_length;
    bool segment_is_empty = free_entries == kEntriesPerSegment;
    if (segment_is_empty) {
      segments_to_deallocate.push_back(segment);
      // Restore the state of the freelist before this segment.
      current_freelist_head = previous_freelist_head;
      current_freelist_length = previous_freelist_length;
    }
  }

  // We cannot remove the segments while iterating over the segments set, so
  // defer that until now.
  for (auto segment : segments_to_deallocate) {
    FreeTableSegment(segment);
    space->segments_.erase(segment);
  }

  FreelistHead new_freelist(current_freelist_head, current_freelist_length);
  space->freelist_head_.store(new_freelist, std::memory_order_release);
  DCHECK_EQ(space->freelist_length(), current_freelist_length);

  uint32_t num_live_entries = space->capacity() - current_freelist_length;
  counters->code_pointers_count()->AddSample(num_live_entries);
  return num_live_entries;
}

DEFINE_LAZY_LEAKY_OBJECT_GETTER(CodePointerTable,
                                GetProcessWideCodePointerTable)

}  // namespace internal
}  // namespace v8

#endif  // V8_COMPRESS_POINTERS
