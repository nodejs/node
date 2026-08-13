// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_FIXED_ARRAY_BASE_H_
#define V8_OBJECTS_FIXED_ARRAY_BASE_H_

#include "src/common/globals.h"
#include "src/objects/heap-object.h"
#include "src/objects/trusted-object.h"

// Has to be the last include (doesn't have include guards):
#include "src/objects/object-macros.h"

namespace v8::internal {

// Limit all fixed arrays to the same max capacity, so that non-resizing
// transitions between different elements kinds (like Smi to Double) will not
// error.
// This could be larger, but the next power of two up would push the maximum
// byte size of FixedDoubleArray out of int32 range.
static constexpr uint32_t kMaxFixedArrayCapacity =
    V8_LOWER_LIMITS_MODE_BOOL ? (16 * 1024 * 1024) : (128 * 1024 * 1024);

// Common superclass for the FixedArray family of heap classes. Owns the
// length_ field (and optional_padding_ for TAGGED_SIZE_8_BYTES); concrete
// subclasses get them via inheritance and append their own fields after
// kHeaderSize.
V8_OBJECT class FixedArrayBase : public HeapObject {
 public:
  static constexpr int kLengthOffset = sizeof(HeapObject);
#if TAGGED_SIZE_8_BYTES
  static constexpr uint32_t kPaddingOffset = kLengthOffset + kUInt32Size;
  static constexpr uint32_t kHeaderSize = kPaddingOffset + kUInt32Size;
#else
  static constexpr uint32_t kHeaderSize = kLengthOffset + kUInt32Size;
#endif  // TAGGED_SIZE_8_BYTES
  static constexpr uint32_t kMaxLength = kMaxFixedArrayCapacity;
  static constexpr int kMaxRegularLength =
      (kMaxRegularHeapObjectSize - kHeaderSize) / kTaggedSize;

  inline SafeHeapObjectSize length() const;
  inline SafeHeapObjectSize ulength() const;
  inline SafeHeapObjectSize length(AcquireLoadTag tag) const;
  inline SafeHeapObjectSize length(RelaxedLoadTag tag) const;
  inline void set_length(uint32_t value);
  inline void set_length(uint32_t value, ReleaseStoreTag tag);

  static inline int GetMaxLengthForNewSpaceAllocation(ElementsKind kind);

  V8_EXPORT_PRIVATE inline bool IsCowArray() const;

  DECL_VERIFIER(FixedArrayBase)

 public:
  uint32_t length_;
#if TAGGED_SIZE_8_BYTES
  uint32_t optional_padding_;
#endif
} V8_OBJECT_END;

// Trusted-space mirror of FixedArrayBase. Owns the same length_ /
// optional_padding_ fields but extends TrustedObject so its concrete
// subclasses live in trusted space.
V8_OBJECT class TrustedFixedArrayBase : public TrustedObject {
 public:
  static constexpr int kLengthOffset = sizeof(TrustedObject);
#if TAGGED_SIZE_8_BYTES
  static constexpr uint32_t kPaddingOffset = kLengthOffset + kUInt32Size;
  static constexpr uint32_t kHeaderSize = kPaddingOffset + kUInt32Size;
#else
  static constexpr uint32_t kHeaderSize = kLengthOffset + kUInt32Size;
#endif  // TAGGED_SIZE_8_BYTES

  inline SafeHeapObjectSize length() const;
  inline SafeHeapObjectSize ulength() const;
  inline SafeHeapObjectSize length(AcquireLoadTag tag) const;
  inline SafeHeapObjectSize length(RelaxedLoadTag tag) const;
  inline void set_length(uint32_t value);
  inline void set_length(uint32_t value, ReleaseStoreTag tag);

 public:
  uint32_t length_;
#if TAGGED_SIZE_8_BYTES
  uint32_t optional_padding_;
#endif
} V8_OBJECT_END;

}  // namespace v8::internal

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_FIXED_ARRAY_BASE_H_
