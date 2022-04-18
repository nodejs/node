// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_HEAP_INVALIDATED_SLOTS_INL_H_
#define V8_HEAP_INVALIDATED_SLOTS_INL_H_

#include "src/heap/invalidated-slots.h"
#include "src/heap/spaces.h"
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
  if (slot < invalidated_start_) {
    return true;
  }

  while (slot >= next_invalidated_start_) {
    NextInvalidatedObject();
  }

  HeapObject invalidated_object = HeapObject::FromAddress(invalidated_start_);

  if (invalidated_size_ == 0) {
    DCHECK(MarkCompactCollector::IsMapOrForwardedMap(invalidated_object.map()));
    invalidated_size_ = invalidated_object.Size();
  }

  int offset = static_cast<int>(slot - invalidated_start_);

  // OLD_TO_OLD can have slots in map word unlike other remembered sets.
  DCHECK_GE(offset, 0);
  DCHECK_IMPLIES(remembered_set_type_ != OLD_TO_OLD, offset > 0);

  if (offset < invalidated_size_)
    return offset == 0 ||
           invalidated_object.IsValidSlot(invalidated_object.map(), offset);

  NextInvalidatedObject();
  return true;
}

void InvalidatedSlotsFilter::NextInvalidatedObject() {
  invalidated_start_ = next_invalidated_start_;
  invalidated_size_ = 0;

  if (iterator_ == iterator_end_) {
    next_invalidated_start_ = sentinel_;
  } else {
    next_invalidated_start_ = iterator_->address();
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
    invalidated_start_ = iterator_->address();
  } else {
    invalidated_start_ = sentinel_;
  }
}

}  // namespace internal
}  // namespace v8

#endif  // V8_HEAP_INVALIDATED_SLOTS_INL_H_
