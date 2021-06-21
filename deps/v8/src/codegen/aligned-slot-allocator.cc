// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/codegen/aligned-slot-allocator.h"

#include "src/base/bits.h"
#include "src/base/logging.h"

namespace v8 {
namespace internal {

int AlignedSlotAllocator::NextSlot(int n) const {
  DCHECK(n == 1 || n == 2 || n == 4);
  if (n <= 1 && IsValid(next1_)) return next1_;
  if (n <= 2 && IsValid(next2_)) return next2_;
  DCHECK(IsValid(next4_));
  return next4_;
}

int AlignedSlotAllocator::Allocate(int n) {
  DCHECK(n == 1 || n == 2 || n == 4);
  // Check invariants.
  DCHECK_EQ(0, next4_ & 3);
  DCHECK_IMPLIES(IsValid(next2_), (next2_ & 1) == 0);

  // The sentinel value kInvalidSlot is used to indicate no slot.
  // next1_ is the index of the 1 slot fragment, or kInvalidSlot.
  // next2_ is the 2-aligned index of the 2 slot fragment, or kInvalidSlot.
  // next4_ is the 4-aligned index of the next 4 slot group. It is always valid.
  // In order to ensure we only have a single 1- or 2-slot fragment, we greedily
  // use any fragment that satisfies the request.
  int result = kInvalidSlot;
  switch (n) {
    case 1: {
      if (IsValid(next1_)) {
        result = next1_;
        next1_ = kInvalidSlot;
      } else if (IsValid(next2_)) {
        result = next2_;
        next1_ = result + 1;
        next2_ = kInvalidSlot;
      } else {
        result = next4_;
        next1_ = result + 1;
        next2_ = result + 2;
        next4_ += 4;
      }
      break;
    }
    case 2: {
      if (IsValid(next2_)) {
        result = next2_;
        next2_ = kInvalidSlot;
      } else {
        result = next4_;
        next2_ = result + 2;
        next4_ += 4;
      }
      break;
    }
    case 4: {
      result = next4_;
      next4_ += 4;
      break;
    }
    default:
      UNREACHABLE();
      break;
  }
  DCHECK(IsValid(result));
  size_ = std::max(size_, result + n);
  return result;
}

int AlignedSlotAllocator::AllocateUnaligned(int n) {
  DCHECK_GE(n, 0);
  // Check invariants.
  DCHECK_EQ(0, next4_ & 3);
  DCHECK_IMPLIES(IsValid(next2_), (next2_ & 1) == 0);

  // Reserve |n| slots at |size_|, invalidate fragments below the new |size_|,
  // and add any new fragments beyond the new |size_|.
  int result = size_;
  size_ += n;
  switch (size_ & 3) {
    case 0: {
      next1_ = next2_ = kInvalidSlot;
      next4_ = size_;
      break;
    }
    case 1: {
      next1_ = size_;
      next2_ = size_ + 1;
      next4_ = size_ + 3;
      break;
    }
    case 2: {
      next1_ = kInvalidSlot;
      next2_ = size_;
      next4_ = size_ + 2;
      break;
    }
    case 3: {
      next1_ = size_;
      next2_ = kInvalidSlot;
      next4_ = size_ + 1;
      break;
    }
  }
  return result;
}

int AlignedSlotAllocator::Align(int n) {
  DCHECK(base::bits::IsPowerOfTwo(n));
  DCHECK_LE(n, 4);
  int mask = n - 1;
  int misalignment = size_ & mask;
  int padding = (n - misalignment) & mask;
  AllocateUnaligned(padding);
  return padding;
}

}  // namespace internal
}  // namespace v8
