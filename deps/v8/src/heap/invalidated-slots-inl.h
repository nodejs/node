// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_HEAP_INVALIDATED_SLOTS_INL_H_
#define V8_HEAP_INVALIDATED_SLOTS_INL_H_

#include "src/base/logging.h"
#include "src/heap/invalidated-slots.h"
#include "src/heap/marking-state-inl.h"
#include "src/heap/spaces.h"
#include "src/objects/heap-object.h"
#include "src/objects/objects-inl.h"
#include "src/utils/allocation.h"

namespace v8 {
namespace internal {

bool InvalidatedSlotsFilter::IsValid(Address slot) {
#ifdef DEBUG
  DCHECK_LT(slot, sentinel_);
  // Slots must come in non-decreasing order.
  DCHECK_LE(last_slot_, slot);
  last_slot_ = slot;
#endif
  if (slot < current_.address) {
    return true;
  }

  while (slot >= next_.address) {
    NextInvalidatedObject();
  }

  int offset = static_cast<int>(slot - current_.address);

  // OLD_TO_OLD can have slots in map word unlike other remembered sets.
  DCHECK_GE(offset, 0);
  DCHECK_IMPLIES(remembered_set_type_ != OLD_TO_OLD, offset > 0);

#if DEBUG
  if (current_.is_live) {
    HeapObject object = HeapObject::FromAddress(current_.address);
    DCHECK_EQ(object.Size(), current_.size);
  }
#endif

  if (offset < current_.size) {
    // Slots in dead invalid objects are all treated as invalid.
    if (!current_.is_live) return false;

    // Map word is always a valid tagged reference.
    if (offset == 0) return true;

    // Check whether object has a tagged field at that particular offset.
    HeapObject invalidated_object = HeapObject::FromAddress(current_.address);
    DCHECK_IMPLIES(marking_state_,
                   marking_state_->IsMarked(invalidated_object));
    DCHECK(MarkCompactCollector::IsMapOrForwarded(invalidated_object.map()));
    return invalidated_object.IsValidSlot(invalidated_object.map(), offset);
  }

  NextInvalidatedObject();
  return true;
}

void InvalidatedSlotsFilter::NextInvalidatedObject() {
  current_ = next_;

  if (iterator_ == iterator_end_) {
    next_ = {sentinel_, 0, false};
  } else {
    HeapObject object = iterator_->first;
    bool is_live = marking_state_ ? marking_state_->IsMarked(object) : true;
    next_ = {object.address(), iterator_->second, is_live};
    iterator_++;
  }
}

void InvalidatedSlotsCleanup::Free(Address free_start, Address free_end) {
#ifdef DEBUG
  DCHECK_LT(free_start, free_end);
  // Free regions should come in increasing order and do not overlap
  DCHECK_LE(last_free_, free_start);
  last_free_ = free_start;
#endif

  if (iterator_ == iterator_end_) return;

  // Ignore invalidated objects that start before free region
  while (invalidated_start_ < free_start) {
    ++iterator_;
    NextInvalidatedObject();
  }

  // Remove all invalidated objects that start within
  // free region.
  while (invalidated_start_ < free_end) {
    iterator_ = invalidated_slots_->erase(iterator_);
    NextInvalidatedObject();
  }
}

void InvalidatedSlotsCleanup::NextInvalidatedObject() {
  if (iterator_ != iterator_end_) {
    invalidated_start_ = iterator_->first.address();
  } else {
    invalidated_start_ = sentinel_;
  }
}

}  // namespace internal
}  // namespace v8

#endif  // V8_HEAP_INVALIDATED_SLOTS_INL_H_
