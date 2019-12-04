// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/heap/invalidated-slots.h"
#include "src/heap/spaces.h"

namespace v8 {
namespace internal {

InvalidatedSlotsFilter InvalidatedSlotsFilter::OldToOld(MemoryChunk* chunk) {
  // The sweeper removes invalid slots and makes free space available for
  // allocation. Slots for new objects can be recorded in the free space.
  // Note that we cannot simply check for SweepingDone because pages in large
  // object space are not swept but have SweepingDone() == true.
  bool slots_in_free_space_are_valid =
      chunk->SweepingDone() && chunk->InOldSpace();
  return InvalidatedSlotsFilter(chunk, chunk->invalidated_slots<OLD_TO_OLD>(),
                                slots_in_free_space_are_valid);
}

InvalidatedSlotsFilter InvalidatedSlotsFilter::OldToNew(MemoryChunk* chunk) {
  // Always treat these slots as valid for old-to-new for now. Invalid
  // old-to-new slots are always cleared.
  bool slots_in_free_space_are_valid = true;
  return InvalidatedSlotsFilter(chunk, chunk->invalidated_slots<OLD_TO_NEW>(),
                                slots_in_free_space_are_valid);
}

InvalidatedSlotsFilter::InvalidatedSlotsFilter(
    MemoryChunk* chunk, InvalidatedSlots* invalidated_slots,
    bool slots_in_free_space_are_valid) {
  // Adjust slots_in_free_space_are_valid_ if more spaces are added.
  DCHECK_IMPLIES(invalidated_slots != nullptr,
                 chunk->InOldSpace() || chunk->InLargeObjectSpace());

  slots_in_free_space_are_valid_ = slots_in_free_space_are_valid;
  invalidated_slots = invalidated_slots ? invalidated_slots : &empty_;

  iterator_ = invalidated_slots->begin();
  iterator_end_ = invalidated_slots->end();
  sentinel_ = chunk->area_end();
  if (iterator_ != iterator_end_) {
    invalidated_start_ = iterator_->first.address();
    invalidated_end_ = invalidated_start_ + iterator_->second;
  } else {
    invalidated_start_ = sentinel_;
    invalidated_end_ = sentinel_;
  }
  // These values will be lazily set when needed.
  invalidated_object_size_ = 0;
#ifdef DEBUG
  last_slot_ = chunk->area_start();
#endif
}

InvalidatedSlotsCleanup InvalidatedSlotsCleanup::OldToNew(MemoryChunk* chunk) {
  return InvalidatedSlotsCleanup(chunk, chunk->invalidated_slots<OLD_TO_NEW>());
}

InvalidatedSlotsCleanup InvalidatedSlotsCleanup::NoCleanup(MemoryChunk* chunk) {
  return InvalidatedSlotsCleanup(chunk, nullptr);
}

InvalidatedSlotsCleanup::InvalidatedSlotsCleanup(
    MemoryChunk* chunk, InvalidatedSlots* invalidated_slots) {
  invalidated_slots_ = invalidated_slots ? invalidated_slots : &empty_;
  iterator_ = invalidated_slots_->begin();
  iterator_end_ = invalidated_slots_->end();
  sentinel_ = chunk->area_end();

  if (iterator_ != iterator_end_) {
    invalidated_start_ = iterator_->first.address();
    invalidated_end_ = invalidated_start_ + iterator_->second;
  } else {
    invalidated_start_ = sentinel_;
    invalidated_end_ = sentinel_;
  }

#ifdef DEBUG
  last_free_ = chunk->area_start();
#endif
}

}  // namespace internal
}  // namespace v8
