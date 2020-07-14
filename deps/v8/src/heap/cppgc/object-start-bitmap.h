// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_HEAP_CPPGC_OBJECT_START_BITMAP_H_
#define V8_HEAP_CPPGC_OBJECT_START_BITMAP_H_

#include <limits.h>
#include <stdint.h>

#include <array>

#include "src/base/macros.h"
#include "src/heap/cppgc/globals.h"

namespace cppgc {
namespace internal {

class HeapObjectHeader;

// A bitmap for recording object starts. Objects have to be allocated at
// minimum granularity of kGranularity.
//
// Depends on internals such as:
// - kBlinkPageSize
// - kAllocationGranularity
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
  inline HeapObjectHeader* FindHeader(
      ConstAddress address_maybe_pointing_to_the_middle_of_object) const;

  inline void SetBit(ConstAddress);
  inline void ClearBit(ConstAddress);
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
  static constexpr size_t kBitsPerCell = sizeof(uint8_t) * CHAR_BIT;
  static constexpr size_t kCellMask = kBitsPerCell - 1;
  static constexpr size_t kBitmapSize =
      (kPageSize + ((kBitsPerCell * kAllocationGranularity) - 1)) /
      (kBitsPerCell * kAllocationGranularity);
  static constexpr size_t kReservedForBitmap =
      ((kBitmapSize + kAllocationMask) & ~kAllocationMask);

  inline void ObjectStartIndexAndBit(ConstAddress, size_t*, size_t*) const;

  Address offset_;
  // The bitmap contains a bit for every kGranularity aligned address on a
  // a NormalPage, i.e., for a page of size kBlinkPageSize.
  std::array<uint8_t, kReservedForBitmap> object_start_bit_map_;
};

}  // namespace internal
}  // namespace cppgc

#endif  // V8_HEAP_CPPGC_OBJECT_START_BITMAP_H_
