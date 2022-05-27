// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/heap/invalidated-slots.h"

#include "src/heap/invalidated-slots-inl.h"
#include "src/heap/memory-chunk.h"
#include "src/heap/spaces.h"
#include "src/objects/objects-inl.h"

namespace v8 {
namespace internal {

InvalidatedSlotsFilter InvalidatedSlotsFilter::OldToOld(MemoryChunk* chunk) {
  return InvalidatedSlotsFilter(chunk, chunk->invalidated_slots<OLD_TO_OLD>(),
                                OLD_TO_OLD);
}

InvalidatedSlotsFilter InvalidatedSlotsFilter::OldToNew(MemoryChunk* chunk) {
  return InvalidatedSlotsFilter(chunk, chunk->invalidated_slots<OLD_TO_NEW>(),
                                OLD_TO_NEW);
}

InvalidatedSlotsFilter InvalidatedSlotsFilter::OldToShared(MemoryChunk* chunk) {
  return InvalidatedSlotsFilter(
      chunk, chunk->invalidated_slots<OLD_TO_SHARED>(), OLD_TO_SHARED);
}

InvalidatedSlotsFilter::InvalidatedSlotsFilter(
    MemoryChunk* chunk, InvalidatedSlots* invalidated_slots,
    RememberedSetType remembered_set_type) {
  USE(remembered_set_type);
  invalidated_slots = invalidated_slots ? invalidated_slots : &empty_;

  iterator_ = invalidated_slots->begin();
  iterator_end_ = invalidated_slots->end();
  sentinel_ = chunk->area_end();

  // Invoke NextInvalidatedObject twice, to initialize
  // invalidated_start_ to the first invalidated object and
  // next_invalidated_object_ to the second one.
  NextInvalidatedObject();
  NextInvalidatedObject();

#ifdef DEBUG
  last_slot_ = chunk->area_start();
  remembered_set_type_ = remembered_set_type;
#endif
}

InvalidatedSlotsCleanup InvalidatedSlotsCleanup::OldToNew(MemoryChunk* chunk) {
  return InvalidatedSlotsCleanup(chunk, chunk->invalidated_slots<OLD_TO_NEW>());
}

InvalidatedSlotsCleanup InvalidatedSlotsCleanup::OldToShared(
    MemoryChunk* chunk) {
  return InvalidatedSlotsCleanup(chunk,
                                 chunk->invalidated_slots<OLD_TO_SHARED>());
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

  NextInvalidatedObject();

#ifdef DEBUG
  last_free_ = chunk->area_start();
#endif
}

}  // namespace internal
}  // namespace v8
