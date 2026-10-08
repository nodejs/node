// Copyright 2024 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/sandbox/cppheap-pointer-table.h"

#include "src/execution/isolate.h"
#include "src/logging/counters.h"
#include "src/sandbox/cppheap-pointer-table-inl.h"

#ifdef V8_COMPRESS_POINTERS

namespace v8 {
namespace internal {

uint32_t CppHeapPointerTable::SweepAndCompact(Space* space,
                                              Counters* counters) {
  DCHECK(space->BelongsTo(this));

  // Lock the space. Technically this is not necessary since no other thread can
  // allocate entries at this point, but some of the methods we call on the
  // space assert that the lock is held.
  base::MutexGuard guard(&space->mutex_);
  // Same for the invalidated fields mutex.
  base::MutexGuard invalidated_fields_guard(&space->invalidated_fields_mutex_);

  // There must not be any entry allocations while the table is being swept as
  // that would not be safe. Set the freelist to this special marker value to
  // easily catch any violation of this requirement.
  space->freelist_head_.store(kEntryAllocationIsForbiddenMarker,
                              std::memory_order_relaxed);

  // When compacting, we can compute the number of unused segments at the end of
  // the table and skip those during sweeping.
  uint32_t start_of_evacuation_area =
      space->start_of_evacuation_area_.load(std::memory_order_relaxed);
  bool evacuation_was_successful = false;
  if (space->IsCompacting()) {
    if (space->CompactingWasAborted()) {
      // Extract the original start_of_evacuation_area value so that the
      // DCHECKs below and in TryResolveEvacuationEntryDuringSweeping work.
      start_of_evacuation_area &= ~Space::kCompactionAbortedMarker;
    } else {
      evacuation_was_successful = true;
    }
    DCHECK(IsAligned(start_of_evacuation_area, kEntriesPerSegment));

    space->StopCompacting();
  }

  // Sweep top to bottom and rebuild the freelist from newly dead and
  // previously freed entries while also clearing the marking bit on live
  // entries and resolving evacuation entries table when compacting the table.
  // This way, the freelist ends up sorted by index which already makes the
  // table somewhat self-compacting and is required for the compaction
  // algorithm so that evacuated entries are evacuated to the start of a space.
  // This method must run either on the mutator thread or while the mutator is
  // stopped.
  uint32_t current_freelist_head = 0;
  uint32_t current_freelist_length = 0;
  std::vector<Segment> segments_to_deallocate;
  for (auto segment : base::Reversed(space->segments_)) {
    bool segment_will_be_evacuated =
        evacuation_was_successful &&
        segment.first_entry() >= start_of_evacuation_area;
    if (SweepAndCompactSegment(
            space, segment, start_of_evacuation_area, segment_will_be_evacuated,
            &current_freelist_head, &current_freelist_length)) {
      segments_to_deallocate.push_back(segment);
    }
  }

  // We cannot deallocate the segments during the above loop, so do it now.
  for (auto segment : segments_to_deallocate) {
    FreeTableSegment(segment);
    space->segments_.erase(segment);
  }

  space->ClearInvalidatedFields();

  FreelistHead new_freelist(current_freelist_head, current_freelist_length);
  space->freelist_head_.store(new_freelist, std::memory_order_release);
  DCHECK_EQ(space->freelist_length(), current_freelist_length);

  uint32_t num_live_entries = space->capacity() - current_freelist_length;
  counters->cppheap_pointers_count()->AddSample(num_live_entries);
  return num_live_entries;
}


void CppHeapPointerTable::Verify(Isolate* isolate, Space* space) {
  IterateEntriesIn(space, [&](uint32_t index) {
    auto payload = at(index).GetRawPayload();
    CppHeapPointerTag tag = payload.ExtractTag();
    if (tag == CppHeapPointerTag::kFreeEntryTag ||
        tag == CppHeapPointerTag::kEvacuationEntryTag ||
        tag == CppHeapPointerTag::kZappedEntryTag) {
      return;
    }

    Address pointer = payload.Untag(tag);
    if (pointer == kNullAddress) return;

    // We don't know the C++ type of the referenced object, so we cannot do
    // much verification on it. What we can do is try to load the first byte of
    // the object (we assume we don't have zero-sized objects). This way, we
    // can at least detect issues like use-after-free on ASan builds.
    USE(*reinterpret_cast<const uint8_t*>(pointer));
  });
}

#ifdef OBJECT_PRINT

namespace {

constexpr std::string_view entry_spacer =
    "+-----------------------------------------+\n";

}  // namespace

// static
void CppHeapPointerTableEntryPrinter::PrintHeader(const char* space_name) {
  PrintF(stderr, "%s", entry_spacer.data());
  PrintF(stderr, "| %*s |\n", static_cast<int>(entry_spacer.size() - 5),
         space_name);
  PrintF(stderr, "%s", entry_spacer.data());
  PrintF(stderr, "|     handle |   tag |    CppHeap pointer |\n");
  PrintF(stderr, "%s", entry_spacer.data());
}

// static
void CppHeapPointerTableEntryPrinter::PrintIfInUse(
    CppHeapPointerHandle handle, const CppHeapPointerTableEntry& entry,
    std::function<bool(CppHeapPointerTag)> entry_callback) {
  const auto payload = entry.GetRawPayload();
  const CppHeapPointerTag tag = payload.ExtractTag();
  if (tag == CppHeapPointerTag::kFreeEntryTag ||
      tag == CppHeapPointerTag::kZappedEntryTag) {
    return;
  }
  if (!entry_callback(tag)) {
    return;
  }

  Address address = payload.Untag(tag);
  PrintF(stderr, "| %10" PRIu32 " | %5" PRIu16 " | 0x%016" PRIxPTR " |\n",
         handle, tag, address);
}

// static
void CppHeapPointerTableEntryPrinter::PrintFooter() {
  PrintF(stderr, "%s", entry_spacer.data());
}

#endif  // OBJECT_PRINT

}  // namespace internal
}  // namespace v8

#endif  // V8_COMPRESS_POINTERS
