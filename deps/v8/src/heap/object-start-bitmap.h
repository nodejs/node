// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_HEAP_OBJECT_START_BITMAP_H_
#define V8_HEAP_OBJECT_START_BITMAP_H_

#include <array>

#include "src/common/globals.h"
#include "testing/gtest/include/gtest/gtest_prod.h"  // nogncheck

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

  inline ObjectStartBitmap(PtrComprCageBase cage_base, size_t offset);

  // Finds an object header based on a maybe_inner_ptr. If the object start
  // bitmap is not fully populated, this iterates through the objects of the
  // page to recalculate the part of the bitmap that is required, for this
  // method to return the correct result.
  inline Address FindBasePtr(Address maybe_inner_ptr);

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

#ifdef VERIFY_HEAP
  // This method verifies that the object start bitmap is consistent with the
  // page's contents. That is, the bits that are set correspond to existing
  // objects in the page.
  inline void Verify() const;
#endif  // VERIFY_HEAP

 private:
  inline void store(size_t cell_index, uint32_t value);
  inline uint32_t load(size_t cell_index) const;

  PtrComprCageBase cage_base() const { return cage_base_; }
  Address offset() const { return offset_; }

  static constexpr size_t kBitsPerCell = sizeof(uint32_t) * CHAR_BIT;
  static constexpr size_t kCellMask = kBitsPerCell - 1;
  static constexpr size_t kBitmapSize =
      (kPageSize + ((kBitsPerCell * kAllocationGranularity) - 1)) /
      (kBitsPerCell * kAllocationGranularity);
  static constexpr size_t kReservedForBitmap =
      ((kBitmapSize + kAllocationMask) & ~kAllocationMask);

  inline void ObjectStartIndexAndBit(Address, size_t*, size_t*) const;

  inline Address StartIndexToAddress(size_t object_start_index) const;

  // Finds an object header based on a maybe_inner_ptr. Will search for an
  // object start in decreasing address order. If the object start bitmap is
  // not fully populated, this may incorrectly return |kNullPointer| or the base
  // pointer of a previous object on the page.
  inline Address FindBasePtrImpl(Address maybe_inner_ptr) const;

  PtrComprCageBase cage_base_;
  size_t offset_;

  std::array<uint32_t, kReservedForBitmap> object_start_bit_map_;

  FRIEND_TEST(V8ObjectStartBitmapTest, FindBasePtrExact);
  FRIEND_TEST(V8ObjectStartBitmapTest, FindBasePtrApproximate);
  FRIEND_TEST(V8ObjectStartBitmapTest, FindBasePtrIteratingWholeBitmap);
  FRIEND_TEST(V8ObjectStartBitmapTest, FindBasePtrNextCell);
  FRIEND_TEST(V8ObjectStartBitmapTest, FindBasePtrSameCell);
};

}  // namespace internal
}  // namespace v8

#endif  // V8_HEAP_OBJECT_START_BITMAP_H_
