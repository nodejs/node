// Copyright 2024 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_SANDBOX_COMPACTIBLE_EXTERNAL_ENTITY_TABLE_INL_H_
#define V8_SANDBOX_COMPACTIBLE_EXTERNAL_ENTITY_TABLE_INL_H_

#include "src/sandbox/compactible-external-entity-table.h"
// Include the non-inl header before the rest of the headers.

#include <algorithm>

#include "src/base/logging.h"
#include "src/logging/counters.h"
#include "src/sandbox/external-entity-table-inl.h"
#include "src/sandbox/external-pointer.h"

#ifdef V8_COMPRESS_POINTERS

namespace v8 {
namespace internal {

template <typename Entry, size_t size>
uint32_t CompactibleExternalEntityTable<Entry, size>::AllocateEntry(
    Space* space) {
  uint32_t index = Base::AllocateEntry(space);

  // When we're compacting a space, we're trying to move all entries above a
  // threshold index (the start of the evacuation area) into segments below
  // that threshold. However, if the freelist becomes too short and we start
  // allocating entries inside the area that is supposed to be evacuated, we
  // need to abort compaction. This is not just an optimization but is also
  // required for correctness: during sweeping we might otherwise assume that
  // all entries inside the evacuation area have been moved and that these
  // segments can therefore be deallocated. In particular, this check will also
  // make sure that we abort compaction if we extend the space with a new
  // segment and allocate at least one entry in it (if that segment is located
  // after the threshold, otherwise it is unproblematic).
  uint32_t start_of_evacuation_area =
      space->start_of_evacuation_area_.load(std::memory_order_relaxed);
  if (V8_UNLIKELY(index >= start_of_evacuation_area)) {
    space->AbortCompacting(start_of_evacuation_area);
  }

  return index;
}

template <typename Entry, size_t size>
typename CompactibleExternalEntityTable<Entry, size>::CompactionResult
CompactibleExternalEntityTable<Entry, size>::FinishCompaction(
    Space* space, Histogram* counter) {
  DCHECK(space->BelongsTo(this));
  DCHECK(!space->is_internal_read_only_space());

  // When compacting, we can compute the number of unused segments at the end of
  // the table and deallocate those after sweeping.
  uint32_t start_of_evacuation_area =
      space->start_of_evacuation_area_.load(std::memory_order_relaxed);
  bool evacuation_was_successful = false;
  if (space->IsCompacting()) {
    auto outcome = ExternalEntityTableCompactionOutcome::kAborted;
    if (space->CompactingWasAborted()) {
      // Compaction was aborted during marking because the freelist grew to
      // short. In this case, it is not guaranteed that any segments will now be
      // completely free.  Extract the original start_of_evacuation_area value.
      start_of_evacuation_area &= ~Space::kCompactionAbortedMarker;
    } else {
      // Entry evacuation was successful so all segments inside the evacuation
      // area are now guaranteed to be free and so can be deallocated.
      evacuation_was_successful = true;
      outcome = ExternalEntityTableCompactionOutcome::kSuccess;
    }
    DCHECK(IsAligned(start_of_evacuation_area,
                     ExternalEntityTable<Entry, size>::kEntriesPerSegment));

    space->StopCompacting();
    counter->AddSample(static_cast<int>(outcome));
  }

  return {start_of_evacuation_area, evacuation_was_successful};
}

template <typename Entry, size_t size>
void CompactibleExternalEntityTable<Entry, size>::MaybeCreateEvacuationEntry(
    Space* space, uint32_t index, Address handle_location) {
  // Check if the entry should be evacuated for table compaction.
  // The current value of the start of the evacuation area is cached in a local
  // variable here as it otherwise may be changed by another marking thread
  // while this method runs, causing non-optimal behaviour (for example, the
  // allocation of an evacuation entry _after_ the entry that is evacuated).
  uint32_t start_of_evacuation_area =
      space->start_of_evacuation_area_.load(std::memory_order_relaxed);
  if (index >= start_of_evacuation_area) {
    DCHECK(space->IsCompacting());
    uint32_t new_index =
        Base::AllocateEntryBelow(space, start_of_evacuation_area);
    if (new_index) {
      DCHECK_LT(new_index, start_of_evacuation_area);
      DCHECK(space->Contains(new_index));
      // Even though the new entry will only be accessed during sweeping, this
      // still needs to be an atomic write as another thread may attempt (and
      // fail) to allocate the same table entry, thereby causing a read from
      // this memory location. Without an atomic store here, TSan would then
      // complain about a data race.
      Base::at(new_index).MakeEvacuationEntry(handle_location);
    } else {
      // In this case, the application has allocated a sufficiently large
      // number of entries from the freelist so that new entries would now be
      // allocated inside the area that is being compacted. While it would be
      // possible to shrink that area and continue compacting, we probably do
      // not want to put more pressure on the freelist and so instead simply
      // abort compaction here. Entries that have already been visited will
      // still be compacted during Sweep, but there is no guarantee that any
      // blocks at the end of the table will now be completely free.
      space->AbortCompacting(start_of_evacuation_area);
    }
  }
}

template <typename Entry, size_t size>
void CompactibleExternalEntityTable<Entry, size>::
    ResolveEvacuationEntryDuringSweeping(
        uint32_t new_index,
        typename ExternalEntityTableCompactionTraits<Entry>::Handle*
            handle_location,
        uint32_t start_of_evacuation_area) {
  using Traits = ExternalEntityTableCompactionTraits<Entry>;
  using Handle = typename Traits::Handle;

  Handle old_handle = *handle_location;
  CHECK(Traits::IsValidHandle(old_handle));

  uint32_t old_index = Traits::HandleToIndex(old_handle);
  Handle new_handle = Traits::IndexToHandle(new_index);

  DCHECK_GE(old_index, start_of_evacuation_area);
  DCHECK_LT(new_index, start_of_evacuation_area);
  auto& new_entry = Base::at(new_index);
  Base::at(old_index).Evacuate(new_entry, EvacuateMarkMode::kLeaveUnmarked);
  *handle_location = new_handle;

  Traits::RelocateAuxiliaryEntryData(this, old_index, new_index);
}

template <typename Entry, size_t size>
bool CompactibleExternalEntityTable<Entry, size>::SweepAndCompactSegment(
    Space* space, Segment segment, uint32_t start_of_evacuation_area,
    bool segment_will_be_evacuated, uint32_t* current_freelist_head,
    uint32_t* current_freelist_length) {
  using Traits = ExternalEntityTableCompactionTraits<Entry>;
  using Handle = typename Traits::Handle;

  uint32_t previous_freelist_head = *current_freelist_head;
  uint32_t previous_freelist_length = *current_freelist_length;

  const auto AddToFreelist = [&](uint32_t entry_index) {
    Base::at(entry_index).MakeFreelistEntry(*current_freelist_head);
    *current_freelist_head = entry_index;
    (*current_freelist_length)++;
  };

  for (uint32_t i = segment.last_entry(); i >= segment.first_entry(); i--) {
    const auto payload = Base::at(i).GetRawPayload();
    if (payload.ContainsEvacuationEntry()) {
      DCHECK(!segment_will_be_evacuated);
      Handle* handle_location = reinterpret_cast<Handle*>(
          payload.ExtractEvacuationEntryHandleLocation());
      DCHECK_NOT_NULL(handle_location);

      // 1. Bail out if field was invalidated. This can happen when objects
      // change their underlying layout.
      if (space->FieldWasInvalidated(
              reinterpret_cast<Address>(handle_location))) {
        AddToFreelist(i);
        continue;
      }

      // 2. Bail out if handle was overwritten with null after compaction start.
      // This happens when resetting handles.
      Handle old_handle = *handle_location;
      if (old_handle == Traits::kNullHandle) {
        AddToFreelist(i);
        continue;
      }

      // Every non-null handle must be a valid handle; if not, memory corruption
      // occurred.
      CHECK(Traits::IsValidHandle(old_handle));

      // 3. Bail out if handle was overwritten with a valid handle below
      // compaction frontier. This can happen when setting a new handle after
      // compaction has started.
      if (Traits::HandleToIndex(old_handle) < start_of_evacuation_area) {
        AddToFreelist(i);
        continue;
      }

      ResolveEvacuationEntryDuringSweeping(i, handle_location,
                                           start_of_evacuation_area);
      DCHECK(Base::at(i).GetRawPayload().ContainsPointer());
      DCHECK(!Base::at(i).GetRawPayload().HasMarkBitSet());
    } else if (!payload.HasMarkBitSet()) {
      Traits::FreeEntry(this, i);
      AddToFreelist(i);
    } else {
      auto new_payload = payload;
      new_payload.ClearMarkBit();
      Base::at(i).SetRawPayload(new_payload);
    }
    DCHECK(!Base::at(i).HasEvacuationEntry());
  }

  uint32_t free_entries = *current_freelist_length - previous_freelist_length;
  bool segment_is_empty = free_entries == Base::kEntriesPerSegment;
  if (segment_is_empty || segment_will_be_evacuated) {
    *current_freelist_head = previous_freelist_head;
    *current_freelist_length = previous_freelist_length;
    return true;
  }
  return false;
}

template <typename Entry, size_t size>
void CompactibleExternalEntityTable<Entry, size>::Space::StartCompacting(
    uint32_t start_of_evacuation_area) {
  DCHECK_EQ(invalidated_fields_.size(), 0);
  start_of_evacuation_area_.store(start_of_evacuation_area,
                                  std::memory_order_relaxed);
}

template <typename Entry, size_t size>
void CompactibleExternalEntityTable<Entry, size>::Space::StopCompacting() {
  start_of_evacuation_area_.store(kNotCompactingMarker,
                                  std::memory_order_relaxed);
}

template <typename Entry, size_t size>
void CompactibleExternalEntityTable<Entry, size>::Space::AbortCompacting(
    uint32_t start_of_evacuation_area) {
  uint32_t compaction_aborted_marker =
      start_of_evacuation_area | kCompactionAbortedMarker;
  DCHECK_NE(compaction_aborted_marker, kNotCompactingMarker);
  start_of_evacuation_area_.store(compaction_aborted_marker,
                                  std::memory_order_relaxed);
}

template <typename Entry, size_t size>
bool CompactibleExternalEntityTable<Entry, size>::Space::IsCompacting() const {
  return start_of_evacuation_area_.load(std::memory_order_relaxed) !=
         kNotCompactingMarker;
}

template <typename Entry, size_t size>
bool CompactibleExternalEntityTable<Entry, size>::Space::CompactingWasAborted()
    const {
  auto value = start_of_evacuation_area_.load(std::memory_order_relaxed);
  return (value & kCompactionAbortedMarker) == kCompactionAbortedMarker;
}

template <typename Entry, size_t size>
bool CompactibleExternalEntityTable<Entry, size>::Space::FieldWasInvalidated(
    Address field_address) const {
  invalidated_fields_mutex_.AssertHeld();
  return std::find(invalidated_fields_.begin(), invalidated_fields_.end(),
                   field_address) != invalidated_fields_.end();
}

template <typename Entry, size_t size>
void CompactibleExternalEntityTable<Entry,
                                    size>::Space::ClearInvalidatedFields() {
  invalidated_fields_mutex_.AssertHeld();
  invalidated_fields_.clear();
}

template <typename Entry, size_t size>
void CompactibleExternalEntityTable<Entry, size>::Space::AddInvalidatedField(
    Address field_address) {
  if (IsCompacting()) {
    base::MutexGuard guard(&invalidated_fields_mutex_);
    invalidated_fields_.push_back(field_address);
  }
}

template <typename Entry, size_t size>
void CompactibleExternalEntityTable<Entry,
                                    size>::Space::StartCompactingIfNeeded() {
  // Take the lock so that we can be sure that no other thread modifies the
  // segments set concurrently.
  base::MutexGuard guard(&this->mutex_);

  // This method may be executed while other threads allocate entries from the
  // freelist. In that case, this method may use incorrect data to determine if
  // table compaction is necessary. That's fine however since in the worst
  // case, compaction will simply be aborted right away if the freelist became
  // too small.
  uint32_t num_free_entries = this->freelist_length();
  uint32_t num_total_entries = this->capacity();

  // Current (somewhat arbitrary) heuristic: need compacting if the space is
  // more than 1MB in size, is at least 10% empty, and if at least one segment
  // can be freed after successful compaction.
  double free_ratio = static_cast<double>(num_free_entries) /
                      static_cast<double>(num_total_entries);
  uint32_t num_segments_to_evacuate =
      (num_free_entries / 2) / Base::kEntriesPerSegment;
  uint32_t space_size = num_total_entries * Base::kEntrySize;
  bool should_compact = (space_size >= 1 * MB) && (free_ratio >= 0.10) &&
                        (num_segments_to_evacuate >= 1);

  // However, if --stress-compaction is enabled, we compact whenever possible:
  // whenever we have at least two segments, one to evacuate entries into and
  // the other to evacuate entries from.
  if (v8_flags.stress_compaction) {
    should_compact = this->num_segments() > 1;
    num_segments_to_evacuate = std::max(1u, num_segments_to_evacuate);
  }

  if (should_compact) {
    // If we're compacting, attempt to free up the last N segments so that they
    // can be decommitted afterwards.
    auto first_segment_to_evacuate =
        *std::prev(this->segments_.end(), num_segments_to_evacuate);
    uint32_t start_of_evacuation_area = first_segment_to_evacuate.first_entry();
    StartCompacting(start_of_evacuation_area);
  }
}

}  // namespace internal
}  // namespace v8

#endif  // V8_COMPRESS_POINTERS

#endif  // V8_SANDBOX_COMPACTIBLE_EXTERNAL_ENTITY_TABLE_INL_H_
