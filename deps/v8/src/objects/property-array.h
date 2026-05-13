// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_PROPERTY_ARRAY_H_
#define V8_OBJECTS_PROPERTY_ARRAY_H_

#include "src/objects/heap-object.h"

// Has to be the last include (doesn't have include guards):
#include "src/objects/object-macros.h"

namespace v8 {
namespace internal {

#include "torque-generated/src/objects/property-array-tq.inc"

V8_OBJECT class PropertyArray : public HeapObject {
 public:
  // [length]: length of the array.
  // The function returns an alias instead of uint32_t to force conversion at
  // the callsites without missing any implicit casts.
  inline SafeHeapObjectSize length() const;
  inline SafeHeapObjectSize length(AcquireLoadTag) const;

  // This is only used on a newly allocated PropertyArray which
  // doesn't have an existing hash.
  inline void initialize_length(uint32_t length);

  inline void SetHash(int hash);
  inline int Hash() const;

  inline Tagged<Object> get(int index) const;
  inline Tagged<Object> get(int index, SeqCstAccessTag tag) const;

  inline void set(int index, Tagged<Object> value);
  inline void set(int index, Tagged<Object> value, SeqCstAccessTag tag);
  // Setter with explicit barrier mode.
  inline void set(int index, Tagged<Object> value, WriteBarrierMode mode);

  inline Tagged<Object> Swap(int index, Tagged<Object> value,
                             SeqCstAccessTag tag);
  inline Tagged<Object> CompareAndSwap(int index, Tagged<Object> expected,
                                       Tagged<Object> value,
                                       SeqCstAccessTag tag);

  // Signature must be in sync with FixedArray::CopyElements().
  inline static void CopyElements(Isolate* isolate, Tagged<PropertyArray> dst,
                                  int dst_index, Tagged<PropertyArray> src,
                                  int src_index, int len,
                                  WriteBarrierMode mode = UPDATE_WRITE_BARRIER);

  // Gives access to raw memory which stores the array's data.
  inline ObjectSlot data_start();
  inline ObjectSlot RawFieldOfElementAt(int index);

  // Garbage collection support. Defined out of line so they can use
  // OFFSET_OF_DATA_START, which references the FLEXIBLE_ARRAY_MEMBER
  // declared below the class body.
  static constexpr int SizeFor(int length);
  static constexpr int OffsetOfElementAt(int index);
  static constexpr int OffsetInWordsToIndex(int offset_in_words);

  DECL_PRINTER(PropertyArray)
  DECL_VERIFIER(PropertyArray)

  static const int kLengthFieldSize = 10;
  using LengthField = base::BitField<int, 0, kLengthFieldSize>;
  static const int kMaxLength = LengthField::kMax;
  using HashField = base::BitField<int, kLengthFieldSize,
                                   kSmiValueSize - kLengthFieldSize - 1>;

  static const int kNoHashSentinel = 0;

 private:
  inline int length_and_hash() const;
  inline int length_and_hash(AcquireLoadTag) const;
  inline void set_length_and_hash(int value);
  inline void set_length_and_hash(int value, ReleaseStoreTag);

 public:
  TaggedMember<Smi> length_and_hash_;
  // Slots hold property-table spillover values: JSAny for data properties,
  // AccessorPair / AccessorInfo for accessors, ClassPositions / other Structs
  // for class metadata, and the uninitialized_value Hole for not-yet-assigned
  // slots (see MigrateFastToFast in js-objects.cc). The actual element kind
  // is dispatched by the descriptor; callers know what to expect.
  FLEXIBLE_ARRAY_MEMBER(TaggedMember<Object>, objects);
} V8_OBJECT_END;

constexpr int PropertyArray::SizeFor(int length) {
  return OFFSET_OF_DATA_START(PropertyArray) + length * kTaggedSize;
}
constexpr int PropertyArray::OffsetOfElementAt(int index) {
  return SizeFor(index);
}
constexpr int PropertyArray::OffsetInWordsToIndex(int offset_in_words) {
  return offset_in_words - OFFSET_OF_DATA_START(PropertyArray) / kTaggedSize;
}

template <>
struct ObjectTraits<PropertyArray> {
  using BodyDescriptor =
      FlexibleBodyDescriptor<OFFSET_OF_DATA_START(PropertyArray)>;
};

static_assert(FieldStorageLocation::kFirstOutOfObjectOffsetInWords ==
              PropertyArray::OffsetOfElementAt(0) / kTaggedSize);

}  // namespace internal
}  // namespace v8

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_PROPERTY_ARRAY_H_
