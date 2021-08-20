// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_HEAP_OBJECT_START_BITMAP_H_
#define V8_HEAP_OBJECT_START_BITMAP_H_

#include <limits.h>
#include <stdint.h>

#include <array>

#include "include/v8-internal.h"
#include "src/base/bits.h"
#include "src/base/macros.h"
#include "src/common/globals.h"

namespace v8 {
namespace internal {

static constexpr size_t kAllocationGranularity = kTaggedSize;
static constexpr size_t kAllocationMask = kAllocationGranularity - 1;
static const int kPageSize = 1 << kPageSizeBits;

// A bitmap for recording object starts. Objects have to be allocated at
// minimum granularity of kGranularity.
//
// Depends on internals such as:
// - kPageSize
// - kAllocationGranularity
//
// ObjectStartBitmap does not support concurrent access and is used only by the
// main thread.
class V8_EXPORT_PRIVATE ObjectStartBitmap {
 public:
  // Granularity of addresses added to the bitmap.
  static constexpr size_t Granularity() { return kAllocationGranularity; }

  // Maximum number of entries in the bitmap.
  static constexpr size_t MaxEntries() {
    return kReservedForBitmap * kBitsPerCell;
  }

  explicit inline ObjectStartBitmap(size_t offset = 0);

  // Finds an object header based on a maybe_inner_ptr. Will search for an
  // object start in decreasing address order.
  //
  // This must only be used when there exists at least one entry in the bitmap.
  inline Address FindBasePtr(Address maybe_inner_ptr) const;

  inline void SetBit(Address);
  inline void ClearBit(Address);
  inline bool CheckBit(Address) const;

  // Iterates all object starts recorded in the bitmap.
  //
  // The callback is of type
  //   void(Address)
  // and is passed the object start address as parameter.
  template <typename Callback>
  inline void Iterate(Callback) const;

  // Clear the object start bitmap.
  inline void Clear();

 private:
  inline void store(size_t cell_index, uint32_t value);
  inline uint32_t load(size_t cell_index) const;

  inline Address offset() const;

  static constexpr size_t kBitsPerCell = sizeof(uint32_t) * CHAR_BIT;
  static constexpr size_t kCellMask = kBitsPerCell - 1;
  static constexpr size_t kBitmapSize =
      (kPageSize + ((kBitsPerCell * kAllocationGranularity) - 1)) /
      (kBitsPerCell * kAllocationGranularity);
  static constexpr size_t kReservedForBitmap =
      ((kBitmapSize + kAllocationMask) & ~kAllocationMask);

  inline void ObjectStartIndexAndBit(Address, size_t*, size_t*) const;

  inline Address StartIndexToAddress(size_t object_start_index) const;

  size_t offset_;

  std::array<uint32_t, kReservedForBitmap> object_start_bit_map_;
};

ObjectStartBitmap::ObjectStartBitmap(size_t offset) : offset_(offset) {
  Clear();
}

Address ObjectStartBitmap::FindBasePtr(Address maybe_inner_ptr) const {
  DCHECK_LE(offset(), maybe_inner_ptr);
  size_t object_offset = maybe_inner_ptr - offset();
  size_t object_start_number = object_offset / kAllocationGranularity;
  size_t cell_index = object_start_number / kBitsPerCell;
  DCHECK_GT(object_start_bit_map_.size(), cell_index);
  const size_t bit = object_start_number & kCellMask;
  // check if maybe_inner_ptr is the base pointer
  uint32_t byte = load(cell_index) & ((1 << (bit + 1)) - 1);
  while (!byte && cell_index) {
    DCHECK_LT(0u, cell_index);
    byte = load(--cell_index);
  }
  const int leading_zeroes = v8::base::bits::CountLeadingZeros(byte);
  if (leading_zeroes == kBitsPerCell) {
    return kNullAddress;
  }

  object_start_number =
      (cell_index * kBitsPerCell) + (kBitsPerCell - 1) - leading_zeroes;
  Address base_ptr = StartIndexToAddress(object_start_number);
  return base_ptr;
}

void ObjectStartBitmap::SetBit(Address base_ptr) {
  size_t cell_index, object_bit;
  ObjectStartIndexAndBit(base_ptr, &cell_index, &object_bit);
  store(cell_index,
        static_cast<uint32_t>(load(cell_index) | (1 << object_bit)));
}

void ObjectStartBitmap::ClearBit(Address base_ptr) {
  size_t cell_index, object_bit;
  ObjectStartIndexAndBit(base_ptr, &cell_index, &object_bit);
  store(cell_index,
        static_cast<uint32_t>(load(cell_index) & ~(1 << object_bit)));
}

bool ObjectStartBitmap::CheckBit(Address base_ptr) const {
  size_t cell_index, object_bit;
  ObjectStartIndexAndBit(base_ptr, &cell_index, &object_bit);
  return load(cell_index) & (1 << object_bit);
}

void ObjectStartBitmap::store(size_t cell_index, uint32_t value) {
  object_start_bit_map_[cell_index] = value;
  return;
}

uint32_t ObjectStartBitmap::load(size_t cell_index) const {
  return object_start_bit_map_[cell_index];
}

Address ObjectStartBitmap::offset() const { return offset_; }

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
    uint32_t value = object_start_bit_map_[cell_index];
    while (value) {
      const int trailing_zeroes = v8::base::bits::CountTrailingZeros(value);
      const size_t object_start_number =
          (cell_index * kBitsPerCell) + trailing_zeroes;
      const Address object_address = StartIndexToAddress(object_start_number);
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
}  // namespace v8

#endif  // V8_HEAP_OBJECT_START_BITMAP_H_
