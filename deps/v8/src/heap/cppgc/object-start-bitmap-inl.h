// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_HEAP_CPPGC_OBJECT_START_BITMAP_INL_H_
#define V8_HEAP_CPPGC_OBJECT_START_BITMAP_INL_H_

#include <algorithm>

#include "src/base/bits.h"
#include "src/heap/cppgc/object-start-bitmap.h"

namespace cppgc {
namespace internal {

ObjectStartBitmap::ObjectStartBitmap(Address offset) : offset_(offset) {
  Clear();
}

HeapObjectHeader* ObjectStartBitmap::FindHeader(
    ConstAddress address_maybe_pointing_to_the_middle_of_object) const {
  DCHECK_LE(offset_, address_maybe_pointing_to_the_middle_of_object);
  size_t object_offset =
      address_maybe_pointing_to_the_middle_of_object - offset_;
  size_t object_start_number = object_offset / kAllocationGranularity;
  size_t cell_index = object_start_number / kBitsPerCell;
  DCHECK_GT(object_start_bit_map_.size(), cell_index);
  const size_t bit = object_start_number & kCellMask;
  uint8_t byte = object_start_bit_map_[cell_index] & ((1 << (bit + 1)) - 1);
  while (!byte && cell_index) {
    DCHECK_LT(0u, cell_index);
    byte = object_start_bit_map_[--cell_index];
  }
  const int leading_zeroes = v8::base::bits::CountLeadingZeros(byte);
  object_start_number =
      (cell_index * kBitsPerCell) + (kBitsPerCell - 1) - leading_zeroes;
  object_offset = object_start_number * kAllocationGranularity;
  return reinterpret_cast<HeapObjectHeader*>(object_offset + offset_);
}

void ObjectStartBitmap::SetBit(ConstAddress header_address) {
  size_t cell_index, object_bit;
  ObjectStartIndexAndBit(header_address, &cell_index, &object_bit);
  object_start_bit_map_[cell_index] |= (1 << object_bit);
}

void ObjectStartBitmap::ClearBit(ConstAddress header_address) {
  size_t cell_index, object_bit;
  ObjectStartIndexAndBit(header_address, &cell_index, &object_bit);
  object_start_bit_map_[cell_index] &= ~(1 << object_bit);
}

bool ObjectStartBitmap::CheckBit(ConstAddress header_address) const {
  size_t cell_index, object_bit;
  ObjectStartIndexAndBit(header_address, &cell_index, &object_bit);
  return object_start_bit_map_[cell_index] & (1 << object_bit);
}

void ObjectStartBitmap::ObjectStartIndexAndBit(ConstAddress header_address,
                                               size_t* cell_index,
                                               size_t* bit) const {
  const size_t object_offset = header_address - offset_;
  DCHECK(!(object_offset & kAllocationMask));
  const size_t object_start_number = object_offset / kAllocationGranularity;
  *cell_index = object_start_number / kBitsPerCell;
  DCHECK_GT(kBitmapSize, *cell_index);
  *bit = object_start_number & kCellMask;
}

template <typename Callback>
inline void ObjectStartBitmap::Iterate(Callback callback) const {
  for (size_t cell_index = 0; cell_index < kReservedForBitmap; cell_index++) {
    if (!object_start_bit_map_[cell_index]) continue;

    uint8_t value = object_start_bit_map_[cell_index];
    while (value) {
      const int trailing_zeroes = v8::base::bits::CountTrailingZeros(value);
      const size_t object_start_number =
          (cell_index * kBitsPerCell) + trailing_zeroes;
      const Address object_address =
          offset_ + (kAllocationGranularity * object_start_number);
      callback(object_address);
      // Clear current object bit in temporary value to advance iteration.
      value &= ~(1 << (object_start_number & kCellMask));
    }
  }
}

void ObjectStartBitmap::Clear() {
  std::fill(object_start_bit_map_.begin(), object_start_bit_map_.end(), 0);
}

}  // namespace internal
}  // namespace cppgc

#endif  // V8_HEAP_CPPGC_OBJECT_START_BITMAP_INL_H_
