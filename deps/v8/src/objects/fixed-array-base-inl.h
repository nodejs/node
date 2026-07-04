// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_FIXED_ARRAY_BASE_INL_H_
#define V8_OBJECTS_FIXED_ARRAY_BASE_INL_H_

#include "src/objects/fixed-array-base.h"
// Include the non-inl header before the rest of the headers.

#include "src/heap/read-only-heap-inl.h"
#include "src/objects/heap-object-inl.h"

// Has to be the last include (doesn't have include guards):
#include "src/objects/object-macros.h"

namespace v8::internal {

inline SafeHeapObjectSize FixedArrayBase::length() const {
  return SafeHeapObjectSize(length_);
}

inline SafeHeapObjectSize FixedArrayBase::ulength() const { return length(); }

inline SafeHeapObjectSize FixedArrayBase::length(AcquireLoadTag tag) const {
  return SafeHeapObjectSize(base::AsAtomic32::Acquire_Load(&length_));
}

inline SafeHeapObjectSize FixedArrayBase::length(RelaxedLoadTag tag) const {
  return SafeHeapObjectSize(base::AsAtomic32::Relaxed_Load(&length_));
}

inline void FixedArrayBase::set_length(uint32_t value) { length_ = value; }

inline void FixedArrayBase::set_length(uint32_t value, ReleaseStoreTag tag) {
  base::AsAtomic32::Release_Store(&length_, value);
}

inline SafeHeapObjectSize TrustedFixedArrayBase::length() const {
  return SafeHeapObjectSize(length_);
}

inline SafeHeapObjectSize TrustedFixedArrayBase::ulength() const {
  return length();
}

inline SafeHeapObjectSize TrustedFixedArrayBase::length(
    AcquireLoadTag tag) const {
  return SafeHeapObjectSize(base::AsAtomic32::Acquire_Load(&length_));
}

inline SafeHeapObjectSize TrustedFixedArrayBase::length(
    RelaxedLoadTag tag) const {
  return SafeHeapObjectSize(base::AsAtomic32::Relaxed_Load(&length_));
}

inline void TrustedFixedArrayBase::set_length(uint32_t value) {
  length_ = value;
}

inline void TrustedFixedArrayBase::set_length(uint32_t value,
                                              ReleaseStoreTag tag) {
  base::AsAtomic32::Release_Store(&length_, value);
}

// static
inline int FixedArrayBase::GetMaxLengthForNewSpaceAllocation(
    ElementsKind kind) {
  return ((kMaxRegularHeapObjectSize - FixedArrayBase::kHeaderSize) >>
          ElementsKindToShiftSize(kind));
}

inline bool FixedArrayBase::IsCowArray() const {
  return map() == ReadOnlyHeap::EarlyGetReadOnlyRoots(this)
                      .unchecked_fixed_cow_array_map();
}

}  // namespace v8::internal

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_FIXED_ARRAY_BASE_INL_H_
