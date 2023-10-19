// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_CODEGEN_ALIGNED_SLOT_ALLOCATOR_H_
#define V8_CODEGEN_ALIGNED_SLOT_ALLOCATOR_H_

#include "src/base/macros.h"
#include "src/base/platform/platform.h"
#include "src/common/globals.h"

namespace v8 {
namespace internal {

// An aligned slot allocator. Allocates groups of 1, 2, or 4 slots such that the
// first slot of the group is aligned to the group size. The allocator can also
// allocate unaligned groups of arbitrary size, and an align the number of slots
// to 1, 2, or 4 slots. The allocator tries to be as thrifty as possible by
// reusing alignment padding slots in subsequent smaller slot allocations.
class V8_EXPORT_PRIVATE AlignedSlotAllocator {
 public:
  // Slots are always multiples of pointer-sized units.
  static constexpr int kSlotSize = kSystemPointerSize;

  static int NumSlotsForWidth(int bytes) {
    DCHECK_GT(bytes, 0);
    return (bytes + kSlotSize - 1) / kSlotSize;
  }

  AlignedSlotAllocator() = default;

  // Allocates |n| slots, where |n| must be 1, 2, or 4. Padding slots may be
  // inserted for alignment.
  // Returns the starting index of the slots, which is evenly divisible by |n|.
  int Allocate(int n);

  // Gets the starting index of the slots that would be returned by Allocate(n).
  int NextSlot(int n) const;

  // Allocates the given number of slots at the current end of the slot area,
  // and returns the starting index of the slots. This resets any fragment
  // slots, so subsequent allocations will be after the end of this one.
  // AllocateUnaligned(0) can be used to partition the slot area, for example
  // to make sure tagged values follow untagged values on a Frame.
  int AllocateUnaligned(int n);

  // Aligns the slot area so that future allocations begin at the alignment.
  // Returns the number of slots needed to align the slot area.
  int Align(int n);

  // Returns the size of the slot area, in slots. This will be greater than any
  // already allocated slot index.
  int Size() const { return size_; }

 private:
  static constexpr int kInvalidSlot = -1;

  static bool IsValid(int slot) { return slot > kInvalidSlot; }

  int next1_ = kInvalidSlot;
  int next2_ = kInvalidSlot;
  int next4_ = 0;
  int size_ = 0;

  DISALLOW_NEW_AND_DELETE()
};

}  // namespace internal
}  // namespace v8

#endif  // V8_CODEGEN_ALIGNED_SLOT_ALLOCATOR_H_
