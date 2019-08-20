// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_EMBEDDER_DATA_ARRAY_H_
#define V8_OBJECTS_EMBEDDER_DATA_ARRAY_H_

#include "src/common/globals.h"
#include "src/handles/maybe-handles.h"
#include "src/objects/heap-object.h"
#include "torque-generated/field-offsets-tq.h"

// Has to be the last include (doesn't have include guards):
#include "src/objects/object-macros.h"

namespace v8 {
namespace internal {

// This is a storage array for embedder data fields stored in native context.
// It's basically an "array of EmbedderDataSlots".
// Note, if the pointer compression is enabled the embedder data slot also
// contains a raw data part in addition to tagged part.
class EmbedderDataArray : public HeapObject {
 public:
  // [length]: length of the array in an embedder data slots.
  V8_INLINE int length() const;
  V8_INLINE void set_length(int value);

  DECL_CAST(EmbedderDataArray)

  DEFINE_FIELD_OFFSET_CONSTANTS(HeapObject::kHeaderSize,
                                TORQUE_GENERATED_EMBEDDER_DATA_ARRAY_FIELDS)
  // TODO(v8:8989): [torque] Support marker constants.
  static const int kHeaderSize = kSize;

  // Garbage collection support.
  static constexpr int SizeFor(int length) {
    return kHeaderSize + length * kEmbedderDataSlotSize;
  }

  // Returns a grown copy if the index is bigger than the array's length.
  static Handle<EmbedderDataArray> EnsureCapacity(
      Isolate* isolate, Handle<EmbedderDataArray> array, int index);

  // Code Generation support.
  static constexpr int OffsetOfElementAt(int index) { return SizeFor(index); }

  // Address of the first slot.
  V8_INLINE Address slots_start();

  // Address of the one past last slot.
  V8_INLINE Address slots_end();

  // Dispatched behavior.
  DECL_PRINTER(EmbedderDataArray)
  DECL_VERIFIER(EmbedderDataArray)

  class BodyDescriptor;

  static const int kMaxSize = kMaxRegularHeapObjectSize;
  static constexpr int kMaxLength =
      (kMaxSize - kHeaderSize) / kEmbedderDataSlotSize;

 private:
  STATIC_ASSERT(kHeaderSize == Internals::kFixedArrayHeaderSize);

  OBJECT_CONSTRUCTORS(EmbedderDataArray, HeapObject);
};

}  // namespace internal
}  // namespace v8

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_EMBEDDER_DATA_ARRAY_H_
