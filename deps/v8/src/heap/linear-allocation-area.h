// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_HEAP_LINEAR_ALLOCATION_AREA_H_
#define V8_HEAP_LINEAR_ALLOCATION_AREA_H_

#include "include/v8-internal.h"
#include "src/common/checks.h"

namespace v8 {
namespace internal {

// A linear allocation area to allocate objects from.
//
// Invariant that must hold at all times:
//   start <= top <= limit
class LinearAllocationArea final {
 public:
  LinearAllocationArea() = default;
  LinearAllocationArea(Address top, Address limit)
      : start_(top), top_(top), limit_(limit) {
    Verify();
  }

  void Reset(Address top, Address limit) {
    start_ = top;
    top_ = top;
    limit_ = limit;
    Verify();
  }

  void ResetStart() { start_ = top_; }

  V8_INLINE bool CanIncrementTop(size_t bytes) {
    Verify();
    return (top_ + bytes) <= limit_;
  }

  V8_INLINE Address IncrementTop(size_t bytes) {
    Address old_top = top_;
    top_ += bytes;
    Verify();
    return old_top;
  }

  V8_INLINE bool DecrementTopIfAdjacent(Address new_top, size_t bytes) {
    Verify();
    if ((new_top + bytes) == top_) {
      top_ = new_top;
      if (start_ > top_) {
        ResetStart();
      }
      Verify();
      return true;
    }
    return false;
  }

  V8_INLINE bool MergeIfAdjacent(LinearAllocationArea& other) {
    Verify();
    other.Verify();
    if (top_ == other.limit_) {
      top_ = other.top_;
      start_ = other.start_;
      other.Reset(kNullAddress, kNullAddress);
      Verify();
      return true;
    }
    return false;
  }

  V8_INLINE void SetLimit(Address limit) {
    limit_ = limit;
    Verify();
  }

  V8_INLINE Address start() const {
    Verify();
    return start_;
  }
  V8_INLINE Address top() const {
    Verify();
    return top_;
  }
  V8_INLINE Address limit() const {
    Verify();
    return limit_;
  }
  const Address* top_address() const { return &top_; }
  Address* top_address() { return &top_; }
  const Address* limit_address() const { return &limit_; }
  Address* limit_address() { return &limit_; }

  void Verify() const {
#ifdef DEBUG
    SLOW_DCHECK(start_ <= top_);
    SLOW_DCHECK(top_ <= limit_);
    SLOW_DCHECK(top_ == kNullAddress || (top_ & kHeapObjectTagMask) == 0);
#endif  // DEBUG
  }

 private:
  // The start of the LAB. Initially coincides with `top_`. As top is moved
  // ahead, the area [start_, top_[ denotes a range of new objects. This range
  // is reset with `ResetStart()`.
  Address start_ = kNullAddress;
  // The top of the LAB that is used for allocation.
  Address top_ = kNullAddress;
  // Limit of the LAB the denotes the end of the valid range for allocation.
  Address limit_ = kNullAddress;
};

}  // namespace internal
}  // namespace v8

#endif  // V8_HEAP_LINEAR_ALLOCATION_AREA_H_
