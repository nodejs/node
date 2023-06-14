// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/sandbox/external-pointer-table.h"

#include <algorithm>

#include "src/execution/isolate.h"
#include "src/logging/counters.h"
#include "src/sandbox/external-pointer-table-inl.h"

#ifdef V8_COMPRESS_POINTERS

namespace v8 {
namespace internal {

void ExternalPointerTable::Init(Isolate* isolate) {
  InitializeTable(isolate);

  extra_.store(kNotCompactingMarker);

  // Set up the special null entry. This entry must contain nullptr so that
  // empty EmbedderDataSlots represent nullptr.
  static_assert(kNullExternalPointerHandle == 0);
  at(0).MakeExternalPointerEntry(kNullAddress, kExternalPointerNullTag);
}

void ExternalPointerTable::TearDown() { TearDownTable(); }

uint32_t ExternalPointerTable::SweepAndCompact(Isolate* isolate) {
  // There must not be any entry allocations while the table is being swept as
  // that would not be safe. Set the freelist to this special marker value to
  // easily catch any violation of this requirement.
  FreelistHead old_freelist = freelist_head_.load(std::memory_order_relaxed);
  freelist_head_.store(kEntryAllocationIsForbiddenMarker,
                       std::memory_order_relaxed);

  // Keep track of the last block (identified by the index of its first entry)
  // that has live entries. Used to decommit empty blocks at the end.
  DCHECK_GE(capacity(), kEntriesPerBlock);
  const uint32_t last_block = capacity() - kEntriesPerBlock;
  uint32_t last_in_use_block = last_block;

  // When compacting, we can compute the number of unused blocks at the end of
  // the table and skip those during sweeping.
  uint32_t start_of_evacuation_area = extra_.load(std::memory_order_relaxed);
  if (IsCompacting()) {
    TableCompactionOutcome outcome;
    if (CompactingWasAbortedDuringMarking()) {
      // Compaction was aborted during marking because the freelist grew to
      // short. This is not great because now there is no guarantee that any
      // blocks will be emtpy and so the entire table needs to be swept.
      outcome = TableCompactionOutcome::kAbortedDuringMarking;
      // Extract the original start_of_evacuation_area value so that the
      // DCHECKs below work correctly.
      start_of_evacuation_area &= ~kCompactionAbortedMarker;
    } else if (old_freelist.is_empty() ||
               old_freelist.next() > start_of_evacuation_area) {
      // In this case, marking finished successfully, but the application
      // afterwards allocated entries inside the area that is being compacted.
      // In this case, we can still compute how many blocks at the end of the
      // table are now empty.
      if (!old_freelist.is_empty()) {
        last_in_use_block = RoundDown(old_freelist.next(), kEntriesPerBlock);
      }
      outcome = TableCompactionOutcome::kPartialSuccess;
    } else {
      // Marking was successful so the entire evacuation area is now free.
      last_in_use_block = start_of_evacuation_area - kEntriesPerBlock;
      outcome = TableCompactionOutcome::kSuccess;
    }
    DCHECK(IsAligned(start_of_evacuation_area, kEntriesPerBlock));

    StopCompacting();

    isolate->counters()->external_pointer_table_compaction_outcome()->AddSample(
        static_cast<int>(outcome));
  }

  uint32_t table_end = last_in_use_block + kEntriesPerBlock;
  DCHECK(IsAligned(table_end, kEntriesPerBlock));

  // Sweep top to bottom and rebuild the freelist from newly dead and
  // previously freed entries while also clearing the marking bit on live
  // entries and resolving evacuation entries table when compacting the table.
  // This way, the freelist ends up sorted by index which already makes the
  // table somewhat self-compacting and is required for the compaction
  // algorithm so that evacuated entries are evacuated to the start of the
  // table. This method must run either on the mutator thread or while the
  // mutator is stopped.
  uint32_t current_freelist_head = 0;
  uint32_t current_freelist_length = 0;

  // Skip the special null entry. This also guarantees that the first block
  // will never be decommitted.
  // The null entry may have been marked as alive (if any live object was
  // referencing it), which is fine, the entry will just keep the bit set.
  for (uint32_t i = table_end - 1; i > 0; i--) {
    auto payload = at(i).GetRawPayload();
    if (payload.ContainsEvacuationEntry()) {
      // Resolve the evacuation entry: take the pointer to the handle from the
      // evacuation entry, copy the entry to its new location, and finally
      // update the handle to point to the new entry.
      // While we now know that the entry being evacuated is free, we don't add
      // it to (the start of) the freelist because that would immediately cause
      // new fragmentation when the next entry is allocated. Instead, we assume
      // that the blocks out of which entries are evacuated will all be
      // decommitted anyway after this loop, which is usually the case unless
      // compaction was already aborted during marking.
      ExternalPointerHandle* handle_location =
          reinterpret_cast<ExternalPointerHandle*>(
              payload.ExtractEvacuationEntryHandleLocation());
      ResolveEvacuationEntryDuringSweeping(i, handle_location,
                                           start_of_evacuation_area);
    } else if (!payload.HasMarkBitSet()) {
      at(i).MakeFreelistEntry(current_freelist_head);
      current_freelist_head = i;
      current_freelist_length++;
    } else {
      auto new_payload = payload;
      new_payload.ClearMarkBit();
      at(i).SetRawPayload(new_payload);
    }

    // If a block at the end of the table is completely empty, we can decommit
    // it afterwards, thereby shrinking the table.
    if (last_in_use_block == i && current_freelist_length == kEntriesPerBlock) {
      last_in_use_block -= kEntriesPerBlock;
      // This block will be decommitted, so the freelist is now empty again.
      current_freelist_head = 0;
      current_freelist_length = 0;
    }
  }

  // Decommit all blocks at the end of the table that are not used anymore.
  if (last_in_use_block != last_block) {
    uint32_t new_capacity = last_in_use_block + kEntriesPerBlock;
    Shrink(new_capacity);
    DCHECK_EQ(new_capacity, capacity());
  }

  FreelistHead new_freelist(current_freelist_head, current_freelist_length);
  freelist_head_.store(new_freelist, std::memory_order_release);
  DCHECK_EQ(freelist_length(), current_freelist_length);

  uint32_t num_live_entries = capacity() - current_freelist_length;
  isolate->counters()->external_pointers_count()->AddSample(num_live_entries);
  return num_live_entries;
}

void ExternalPointerTable::StartCompactingIfNeeded() {
  // This method may be executed while other threads allocate entries from the
  // freelist or even grow the table, thereby increasing the capacity. In that
  // case, this method may use incorrect data to determine if table compaction
  // is necessary. That's fine however since in the worst case, compaction will
  // simply be aborted right away if the freelist became too small.
  uint32_t current_freelist_length = freelist_length();
  uint32_t current_capacity = capacity();

  // Current (somewhat arbitrary) heuristic: need compacting if the table is
  // more than 1MB in size, is at least 10% empty, and if at least one block
  // can be decommitted after successful compaction.
  uint32_t table_size = current_capacity * kSystemPointerSize;
  double free_ratio = static_cast<double>(current_freelist_length) /
                      static_cast<double>(current_capacity);
  uint32_t num_blocks_to_evacuate =
      (current_freelist_length / 2) / kEntriesPerBlock;

  bool should_compact = (table_size >= 1 * MB) && (free_ratio >= 0.10) &&
                        (num_blocks_to_evacuate >= 1);

  if (should_compact) {
    uint32_t num_entries_to_evacuate =
        num_blocks_to_evacuate * kEntriesPerBlock;
    uint32_t start_of_evacuation_area =
        current_capacity - num_entries_to_evacuate;
    StartCompacting(start_of_evacuation_area);
  }
}

void ExternalPointerTable::ResolveEvacuationEntryDuringSweeping(
    uint32_t new_index, ExternalPointerHandle* handle_location,
    uint32_t start_of_evacuation_area) {
  ExternalPointerHandle old_handle = *handle_location;
  uint32_t old_index = HandleToIndex(old_handle);
  ExternalPointerHandle new_handle = IndexToHandle(new_index);

  // For the compaction algorithm to work optimally, double initialization
  // of entries is forbidden, see below. This DCHECK can detect double
  // initialization of external pointer fields in debug builds by checking
  // that the old_handle was visited during marking.
  // There's no need to clear the bit from the handle as the handle will be
  // replaced by a new, unmarked handle.
  DCHECK(HandleWasVisitedDuringMarking(old_handle));

  // The following DCHECKs assert that the compaction algorithm works
  // correctly: it always moves an entry from the evacuation area to the
  // front of the table. One reason this invariant can be broken is if an
  // external pointer slot is re-initialized, in which case the old_handle
  // may now also point before the evacuation area. For that reason,
  // re-initialization of external pointer slots is forbidden.
  DCHECK_GE(old_index, start_of_evacuation_area);
  DCHECK_LT(new_index, start_of_evacuation_area);

  auto& new_entry = at(new_index);
  at(old_index).UnmarkAndMigrateInto(new_entry);
  *handle_location = new_handle;
}

}  // namespace internal
}  // namespace v8

#endif  // V8_COMPRESS_POINTERS
