// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_HEAP_CPPGC_OBJECT_START_BITMAP_H_
#define V8_HEAP_CPPGC_OBJECT_START_BITMAP_H_

#include <limits.h>
#include <stdint.h>

#include <array>

#include "include/cppgc/internal/write-barrier.h"
#include "src/base/atomic-utils.h"
#include "src/base/bits.h"
#include "src/base/macros.h"
#include "src/heap/cppgc/globals.h"
#include "src/heap/cppgc/heap-object-header.h"

namespace cppgc {
namespace internal {

// A bitmap for recording object starts. Objects have to be allocated at
// minimum granularity of kGranularity.
//
// Depends on internals such as:
// - kBlinkPageSize
// - kAllocationGranularity
//
// ObjectStartBitmap supports concurrent reads from multiple threads but
// only a single mutator thread can write to it.
class V8_EXPORT_PRIVATE ObjectStartBitmap {
 public:
  // Granularity of addresses added to the bitmap.
  static constexpr size_t Granularity() { return kAllocationGranularity; }

  // Maximum number of entries in the bitmap.
  static constexpr size_t MaxEntries() {
    return kReservedForBitmap * kBitsPerCell;
  }

  explicit inline ObjectStartBitmap(Address offset);

  // Finds an object header based on a
  // address_maybe_pointing_to_the_middle_of_object. Will search for an object
  // start in decreasing address order.
  template <AccessMode = AccessMode::kNonAtomic>
  inline HeapObjectHeader* FindHeader(
      ConstAddress address_maybe_pointing_to_the_middle_of_object) const;

  template <AccessMode = AccessMode::kNonAtomic>
  inline void SetBit(ConstAddress);
  template <AccessMode = AccessMode::kNonAtomic>
  inline void ClearBit(ConstAddress);
  template <AccessMode = AccessMode::kNonAtomic>
  inline bool CheckBit(ConstAddress) const;

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
  template <AccessMode = AccessMode::kNonAtomic>
  inline void store(size_t cell_index, uint8_t value);
  template <AccessMode = AccessMode::kNonAtomic>
  inline uint8_t load(size_t cell_index) const;

  static constexpr size_t kBitsPerCell = sizeof(uint8_t) * CHAR_BIT;
  static constexpr size_t kCellMask = kBitsPerCell - 1;
  static constexpr size_t kBitmapSize =
      (kPageSize + ((kBitsPerCell * kAllocationGranularity) - 1)) /
      (kBitsPerCell * kAllocationGranularity);
  static constexpr size_t kReservedForBitmap =
      ((kBitmapSize + kAllocationMask) & ~kAllocationMask);

  inline void ObjectStartIndexAndBit(ConstAddress, size_t*, size_t*) const;

  const Address offset_;
  // The bitmap contains a bit for every kGranularity aligned address on a
  // a NormalPage, i.e., for a page of size kBlinkPageSize.
  std::array<uint8_t, kReservedForBitmap> object_start_bit_map_;
};

ObjectStartBitmap::ObjectStartBitmap(Address offset) : offset_(offset) {
  Clear();
}

template <AccessMode mode>
HeapObjectHeader* ObjectStartBitmap::FindHeader(
    ConstAddress address_maybe_pointing_to_the_middle_of_object) const {
  DCHECK_LE(offset_, address_maybe_pointing_to_the_middle_of_object);
  size_t object_offset =
      address_maybe_pointing_to_the_middle_of_object - offset_;
  size_t object_start_number = object_offset / kAllocationGranularity;
  size_t cell_index = object_start_number / kBitsPerCell;
  DCHECK_GT(object_start_bit_map_.size(), cell_index);
  const size_t bit = object_start_number & kCellMask;
  uint8_t byte = load<mode>(cell_index) & ((1 << (bit + 1)) - 1);
  while (!byte && cell_index) {
    DCHECK_LT(0u, cell_index);
    byte = load<mode>(--cell_index);
  }
  const int leading_zeroes = v8::base::bits::CountLeadingZeros(byte);
  object_start_number =
      (cell_index * kBitsPerCell) + (kBitsPerCell - 1) - leading_zeroes;
  object_offset = object_start_number * kAllocationGranularity;
  return reinterpret_cast<HeapObjectHeader*>(object_offset + offset_);
}

template <AccessMode mode>
void ObjectStartBitmap::SetBit(ConstAddress header_address) {
  size_t cell_index, object_bit;
  ObjectStartIndexAndBit(header_address, &cell_index, &object_bit);
  // Only a single mutator thread can write to the bitmap, so no need for CAS.
  store<mode>(cell_index,
              static_cast<uint8_t>(load(cell_index) | (1 << object_bit)));
}

template <AccessMode mode>
void ObjectStartBitmap::ClearBit(ConstAddress header_address) {
  size_t cell_index, object_bit;
  ObjectStartIndexAndBit(header_address, &cell_index, &object_bit);
  store<mode>(cell_index,
              static_cast<uint8_t>(load(cell_index) & ~(1 << object_bit)));
}

template <AccessMode mode>
bool ObjectStartBitmap::CheckBit(ConstAddress header_address) const {
  size_t cell_index, object_bit;
  ObjectStartIndexAndBit(header_address, &cell_index, &object_bit);
  return load<mode>(cell_index) & (1 << object_bit);
}

template <AccessMode mode>
void ObjectStartBitmap::store(size_t cell_index, uint8_t value) {
  if (mode == AccessMode::kNonAtomic) {
    object_start_bit_map_[cell_index] = value;
    return;
  }
  v8::base::AsAtomicPtr(&object_start_bit_map_[cell_index])
      ->store(value, std::memory_order_release);
}

template <AccessMode mode>
uint8_t ObjectStartBitmap::load(size_t cell_index) const {
  if (mode == AccessMode::kNonAtomic) {
    return object_start_bit_map_[cell_index];
  }
  return v8::base::AsAtomicPtr(&object_start_bit_map_[cell_index])
      ->load(std::memory_order_acquire);
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

// A platform aware version of ObjectStartBitmap to provide platform specific
// optimizations (e.g. Use non-atomic stores on ARMv7 when not marking).
class V8_EXPORT_PRIVATE PlatformAwareObjectStartBitmap
    : public ObjectStartBitmap {
 public:
  explicit inline PlatformAwareObjectStartBitmap(Address offset);

  template <AccessMode = AccessMode::kNonAtomic>
  inline void SetBit(ConstAddress);
  template <AccessMode = AccessMode::kNonAtomic>
  inline void ClearBit(ConstAddress);

 private:
  template <AccessMode>
  static bool ShouldForceNonAtomic();
};

PlatformAwareObjectStartBitmap::PlatformAwareObjectStartBitmap(Address offset)
    : ObjectStartBitmap(offset) {}

// static
template <AccessMode mode>
bool PlatformAwareObjectStartBitmap::ShouldForceNonAtomic() {
#if defined(V8_TARGET_ARCH_ARM)
  // Use non-atomic accesses on ARMv7 when marking is not active.
  if (mode == AccessMode::kAtomic) {
    if (V8_LIKELY(!WriteBarrier::IsAnyIncrementalOrConcurrentMarking()))
      return true;
  }
#endif  // defined(V8_TARGET_ARCH_ARM)
  return false;
}

template <AccessMode mode>
void PlatformAwareObjectStartBitmap::SetBit(ConstAddress header_address) {
  if (ShouldForceNonAtomic<mode>()) {
    ObjectStartBitmap::SetBit<AccessMode::kNonAtomic>(header_address);
    return;
  }
  ObjectStartBitmap::SetBit<mode>(header_address);
}

template <AccessMode mode>
void PlatformAwareObjectStartBitmap::ClearBit(ConstAddress header_address) {
  if (ShouldForceNonAtomic<mode>()) {
    ObjectStartBitmap::ClearBit<AccessMode::kNonAtomic>(header_address);
    return;
  }
  ObjectStartBitmap::ClearBit<mode>(header_address);
}

}  // namespace internal
}  // namespace cppgc

#endif  // V8_HEAP_CPPGC_OBJECT_START_BITMAP_H_
