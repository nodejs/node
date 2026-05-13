// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_EMBEDDER_DATA_ARRAY_H_
#define V8_OBJECTS_EMBEDDER_DATA_ARRAY_H_

#include "src/common/globals.h"
#include "src/handles/maybe-handles.h"
#include "src/objects/heap-object.h"

// Has to be the last include (doesn't have include guards):
#include "src/objects/object-macros.h"

namespace v8 {
namespace internal {

#include "torque-generated/src/objects/embedder-data-array-tq.inc"

// This is a storage array for embedder data fields stored in native context.
// It's basically an "array of EmbedderDataSlots".
// Note, if the pointer compression is enabled the embedder data slot also
// contains a raw data part in addition to tagged part.
V8_OBJECT class EmbedderDataArray : public HeapObject {
 public:
  inline int length() const;
  inline void set_length(int value);

  // Garbage collection support. Defined out-of-line below so they can use
  // OFFSET_OF_DATA_START, which references the FLEXIBLE_ARRAY_MEMBER
  // declared below the class body.
  static constexpr int SizeFor(int length);

  // Returns a grown copy if the index is bigger than the array's length.
  static DirectHandle<EmbedderDataArray> EnsureCapacity(
      Isolate* isolate, DirectHandle<EmbedderDataArray> array, int index);

  // Code Generation support.
  static constexpr int OffsetOfElementAt(int index);

  // Address of the first slot.
  V8_INLINE Address slots_start();

  // Address of the one past last slot.
  V8_INLINE Address slots_end();

  // Dispatched behavior.
  DECL_PRINTER(EmbedderDataArray)
  DECL_VERIFIER(EmbedderDataArray)

  class BodyDescriptor;

  TaggedMember<Smi> length_;
  // EmbedderDataSlots stored inline; each slot occupies
  // kEmbedderDataSlotSize bytes (1 or 2 Address words depending on
  // whether pointer compression is enabled).
  FLEXIBLE_ARRAY_MEMBER(Address, slots);
} V8_OBJECT_END;

constexpr int EmbedderDataArray::SizeFor(int length) {
  static_assert(kEmbedderDataSlotSize == sizeof(Address));
  return OFFSET_OF_DATA_START(EmbedderDataArray) +
         length * kEmbedderDataSlotSize;
}

constexpr int EmbedderDataArray::OffsetOfElementAt(int index) {
  return SizeFor(index);
}

inline constexpr int kEmbedderDataArrayMaxSize = kMaxRegularHeapObjectSize;
inline constexpr int kEmbedderDataArrayMaxLength =
    (kEmbedderDataArrayMaxSize - OFFSET_OF_DATA_START(EmbedderDataArray)) /
    kEmbedderDataSlotSize;

static_assert(OFFSET_OF_DATA_START(EmbedderDataArray) ==
              Internals::kFixedArrayHeaderSize);

}  // namespace internal
}  // namespace v8

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_EMBEDDER_DATA_ARRAY_H_
