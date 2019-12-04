// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_HEAP_INVALIDATED_SLOTS_INL_H_
#define V8_HEAP_INVALIDATED_SLOTS_INL_H_

#include <map>

#include "src/heap/invalidated-slots.h"
#include "src/heap/spaces.h"
#include "src/objects/objects-body-descriptors-inl.h"
#include "src/objects/objects-body-descriptors.h"
#include "src/objects/objects.h"
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
  while (slot >= invalidated_end_) {
    ++iterator_;
    if (iterator_ != iterator_end_) {
      // Invalidated ranges must not overlap.
      DCHECK_LE(invalidated_end_, iterator_->first.address());
      invalidated_start_ = iterator_->first.address();
      invalidated_end_ = invalidated_start_ + iterator_->second;
      invalidated_object_ = HeapObject();
      invalidated_object_size_ = 0;
    } else {
      invalidated_start_ = sentinel_;
      invalidated_end_ = sentinel_;
    }
  }
  // Now the invalidated region ends after the slot.
  if (slot < invalidated_start_) {
    // The invalidated region starts after the slot.
    return true;
  }
  // The invalidated region includes the slot.
  // Ask the object if the slot is valid.
  if (invalidated_object_.is_null()) {
    invalidated_object_ = HeapObject::FromAddress(invalidated_start_);
    DCHECK(!invalidated_object_.IsFiller());
    invalidated_object_size_ =
        invalidated_object_.SizeFromMap(invalidated_object_.map());
  }
  int offset = static_cast<int>(slot - invalidated_start_);
  DCHECK_GT(offset, 0);
  DCHECK_LE(invalidated_object_size_,
            static_cast<int>(invalidated_end_ - invalidated_start_));

  if (offset >= invalidated_object_size_) {
    return slots_in_free_space_are_valid_;
  }
  return invalidated_object_.IsValidSlot(invalidated_object_.map(), offset);
}

void InvalidatedSlotsCleanup::Free(Address free_start, Address free_end) {
#ifdef DEBUG
  DCHECK_LT(free_start, free_end);
  // Free regions should come in increasing order and do not overlap
  DCHECK_LE(last_free_, free_start);
  last_free_ = free_start;
#endif

  if (iterator_ == iterator_end_) return;

  // Ignore invalidated objects before free region
  while (free_start >= invalidated_end_) {
    ++iterator_;
    NextInvalidatedObject();
  }

  // Loop here: Free region might contain multiple invalidated objects
  while (free_end > invalidated_start_) {
    // Case: Free region starts before current invalidated object
    if (free_start <= invalidated_start_) {
      iterator_ = invalidated_slots_->erase(iterator_);

    } else {
      // Case: Free region starts within current invalidated object
      // (Can happen for right-trimmed objects)
      iterator_++;
    }

    NextInvalidatedObject();
  }
}

void InvalidatedSlotsCleanup::NextInvalidatedObject() {
  if (iterator_ != iterator_end_) {
    invalidated_start_ = iterator_->first.address();
    invalidated_end_ = invalidated_start_ + iterator_->second;
  } else {
    invalidated_start_ = sentinel_;
    invalidated_end_ = sentinel_;
  }
}

}  // namespace internal
}  // namespace v8

#endif  // V8_HEAP_INVALIDATED_SLOTS_INL_H_
