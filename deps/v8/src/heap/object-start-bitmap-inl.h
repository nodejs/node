// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_HEAP_OBJECT_START_BITMAP_INL_H_
#define V8_HEAP_OBJECT_START_BITMAP_INL_H_

#include <limits.h>
#include <stdint.h>

#include "include/v8-internal.h"
#include "src/base/bits.h"
#include "src/base/macros.h"
#include "src/heap/object-start-bitmap.h"
#include "src/heap/paged-spaces-inl.h"
#include "src/heap/paged-spaces.h"

namespace v8 {
namespace internal {

ObjectStartBitmap::ObjectStartBitmap(PtrComprCageBase cage_base, size_t offset)
    : cage_base_(cage_base), offset_(offset) {
  Clear();
}

Address ObjectStartBitmap::FindBasePtrImpl(Address maybe_inner_ptr) const {
  DCHECK_LE(offset(), maybe_inner_ptr);
  size_t object_offset = maybe_inner_ptr - offset();
  size_t object_start_number = object_offset / kAllocationGranularity;
  size_t cell_index = object_start_number / kBitsPerCell;
  DCHECK_GT(object_start_bit_map_.size(), cell_index);
  const size_t bit = object_start_number & kCellMask;
  // check if maybe_inner_ptr is the base pointer
  uint32_t byte =
      load(cell_index) & static_cast<uint32_t>((1 << (bit + 1)) - 1);
  while (byte == 0 && cell_index > 0) {
    byte = load(--cell_index);
  }
  if (byte == 0) {
    DCHECK_EQ(0, cell_index);
    return kNullAddress;
  }
  const int leading_zeroes = v8::base::bits::CountLeadingZeros(byte);
  DCHECK_GT(kBitsPerCell, leading_zeroes);
  object_start_number =
      (cell_index * kBitsPerCell) + (kBitsPerCell - 1) - leading_zeroes;
  return StartIndexToAddress(object_start_number);
}

Address ObjectStartBitmap::FindBasePtr(Address maybe_inner_ptr) {
  Address base_ptr = FindBasePtrImpl(maybe_inner_ptr);
  if (base_ptr == maybe_inner_ptr) {
    DCHECK(CheckBit(base_ptr));
    return base_ptr;
  }
  // TODO(v8:12851): If the ObjectStartBitmap implementation stays, this part of
  // code involving the Page and the iteration through its objects is the only
  // connection with V8 internals. It should be moved to some different
  // abstraction.
  const Page* page = Page::FromAddress(offset_);
  DCHECK_EQ(page->area_start(), offset_);

#ifdef VERIFY_HEAP
  if (v8_flags.verify_heap) {
    Verify();
  }
#endif  // VERIFY_HEAP

  if (base_ptr == kNullAddress) base_ptr = offset_;
  DCHECK_LE(base_ptr, maybe_inner_ptr);

  const Address limit = page->area_end();
  while (base_ptr < limit) {
    SetBit(base_ptr);
    if (maybe_inner_ptr < base_ptr) break;
    const int size = HeapObject::FromAddress(base_ptr).Size(cage_base());
    if (maybe_inner_ptr < base_ptr + size) return base_ptr;
    base_ptr += size;
    DCHECK_LE(base_ptr, limit);
  }
  return kNullAddress;
}

void ObjectStartBitmap::SetBit(Address base_ptr) {
  size_t cell_index, object_bit;
  ObjectStartIndexAndBit(base_ptr, &cell_index, &object_bit);
  store(cell_index, load(cell_index) | static_cast<uint32_t>(1 << object_bit));
}

void ObjectStartBitmap::ClearBit(Address base_ptr) {
  size_t cell_index, object_bit;
  ObjectStartIndexAndBit(base_ptr, &cell_index, &object_bit);
  store(cell_index,
        load(cell_index) & static_cast<uint32_t>(~(1 << object_bit)));
}

bool ObjectStartBitmap::CheckBit(Address base_ptr) const {
  size_t cell_index, object_bit;
  ObjectStartIndexAndBit(base_ptr, &cell_index, &object_bit);
  return (load(cell_index) & static_cast<uint32_t>(1 << object_bit)) != 0;
}

void ObjectStartBitmap::store(size_t cell_index, uint32_t value) {
  object_start_bit_map_[cell_index] = value;
}

uint32_t ObjectStartBitmap::load(size_t cell_index) const {
  return object_start_bit_map_[cell_index];
}

void ObjectStartBitmap::ObjectStartIndexAndBit(Address base_ptr,
                                               size_t* cell_index,
                                               size_t* bit) const {
  const size_t object_offset = base_ptr - offset();
  DCHECK(!(object_offset & kAllocationMask));
  const size_t object_start_number = object_offset / kAllocationGranularity;
  *cell_index = object_start_number / kBitsPerCell;
  DCHECK_GT(kBitmapSize, *cell_index);
  *bit = object_start_number & kCellMask;
}

Address ObjectStartBitmap::StartIndexToAddress(
    size_t object_start_index) const {
  return offset() + (kAllocationGranularity * object_start_index);
}

template <typename Callback>
inline void ObjectStartBitmap::Iterate(Callback callback) const {
  for (size_t cell_index = 0; cell_index < kReservedForBitmap; cell_index++) {
    uint32_t value = load(cell_index);
    while (value != 0) {
      const int trailing_zeroes =
          v8::base::bits::CountTrailingZerosNonZero(value);
      DCHECK_GT(kBitsPerCell, trailing_zeroes);
      const size_t object_start_number =
          (cell_index * kBitsPerCell) + trailing_zeroes;
      const Address object_address = StartIndexToAddress(object_start_number);
      callback(object_address);
      // Clear current object bit in temporary value to advance iteration.
      value &= value - 1;
    }
  }
}

void ObjectStartBitmap::Clear() {
  std::fill(object_start_bit_map_.begin(), object_start_bit_map_.end(), 0);
}

#ifdef VERIFY_HEAP
void ObjectStartBitmap::Verify() const {
  Page* page = Page::FromAddress(offset_);
  DCHECK_EQ(page->area_start(), offset_);
  Address next_object_in_page = page->area_start();
  const PtrComprCageBase cage = cage_base();
  Iterate([&next_object_in_page, cage](Address next_object_in_bitmap) {
    while (next_object_in_page != next_object_in_bitmap) {
      DCHECK_LT(next_object_in_page, next_object_in_bitmap);
      next_object_in_page +=
          HeapObject::FromAddress(next_object_in_page).Size(cage);
    }
  });
}
#endif  // VERIFY_HEAP

}  // namespace internal
}  // namespace v8

#endif  // V8_HEAP_OBJECT_START_BITMAP_INL_H_
