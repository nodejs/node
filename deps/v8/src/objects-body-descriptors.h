// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_BODY_DESCRIPTORS_H_
#define V8_OBJECTS_BODY_DESCRIPTORS_H_

#include "src/objects.h"

namespace v8 {
namespace internal {

// This is the base class for object's body descriptors.
//
// Each BodyDescriptor subclass must provide the following methods:
//
// 1) Returns true if the object contains a tagged value at given offset.
//    It is used for invalid slots filtering. If the offset points outside
//    of the object or to the map word, the result is UNDEFINED (!!!).
//
//   static bool IsValidSlot(HeapObject* obj, int offset);
//
//
// 2) Iterate object's body using stateful object visitor.
//
//   template <typename ObjectVisitor>
//   static inline void IterateBody(HeapObject* obj, int object_size,
//                                  ObjectVisitor* v);
//
//
// 3) Iterate object's body using stateless object visitor.
//
//   template <typename StaticVisitor>
//   static inline void IterateBody(HeapObject* obj, int object_size);
//
class BodyDescriptorBase BASE_EMBEDDED {
 public:
  template <typename ObjectVisitor>
  static inline void IteratePointers(HeapObject* obj, int start_offset,
                                     int end_offset, ObjectVisitor* v);

  template <typename StaticVisitor>
  static inline void IteratePointers(Heap* heap, HeapObject* obj,
                                     int start_offset, int end_offset);

  template <typename ObjectVisitor>
  static inline void IteratePointer(HeapObject* obj, int offset,
                                    ObjectVisitor* v);

  template <typename StaticVisitor>
  static inline void IteratePointer(Heap* heap, HeapObject* obj, int offset);

 protected:
  // Returns true for all header and internal fields.
  static inline bool IsValidSlotImpl(HeapObject* obj, int offset);

  // Treats all header and internal fields in the range as tagged.
  template <typename ObjectVisitor>
  static inline void IterateBodyImpl(HeapObject* obj, int start_offset,
                                     int end_offset, ObjectVisitor* v);

  // Treats all header and internal fields in the range as tagged.
  template <typename StaticVisitor>
  static inline void IterateBodyImpl(Heap* heap, HeapObject* obj,
                                     int start_offset, int end_offset);
};


// This class describes a body of an object of a fixed size
// in which all pointer fields are located in the [start_offset, end_offset)
// interval.
template <int start_offset, int end_offset, int size>
class FixedBodyDescriptor final : public BodyDescriptorBase {
 public:
  static const int kStartOffset = start_offset;
  static const int kEndOffset = end_offset;
  static const int kSize = size;

  static bool IsValidSlot(HeapObject* obj, int offset) {
    return offset >= kStartOffset && offset < kEndOffset;
  }

  template <typename ObjectVisitor>
  static inline void IterateBody(HeapObject* obj, ObjectVisitor* v) {
    IterateBodyImpl(obj, start_offset, end_offset, v);
  }

  template <typename ObjectVisitor>
  static inline void IterateBody(HeapObject* obj, int object_size,
                                 ObjectVisitor* v) {
    IterateBody(obj, v);
  }

  template <typename StaticVisitor>
  static inline void IterateBody(HeapObject* obj) {
    Heap* heap = obj->GetHeap();
    IterateBodyImpl<StaticVisitor>(heap, obj, start_offset, end_offset);
  }

  template <typename StaticVisitor>
  static inline void IterateBody(HeapObject* obj, int object_size) {
    IterateBody(obj);
  }
};


// This class describes a body of an object of a variable size
// in which all pointer fields are located in the [start_offset, object_size)
// interval.
template <int start_offset>
class FlexibleBodyDescriptor final : public BodyDescriptorBase {
 public:
  static const int kStartOffset = start_offset;

  static bool IsValidSlot(HeapObject* obj, int offset) {
    if (offset < kStartOffset) return false;
    return IsValidSlotImpl(obj, offset);
  }

  template <typename ObjectVisitor>
  static inline void IterateBody(HeapObject* obj, int object_size,
                                 ObjectVisitor* v) {
    IterateBodyImpl(obj, start_offset, object_size, v);
  }

  template <typename StaticVisitor>
  static inline void IterateBody(HeapObject* obj, int object_size) {
    Heap* heap = obj->GetHeap();
    IterateBodyImpl<StaticVisitor>(heap, obj, start_offset, object_size);
  }

  static inline int SizeOf(Map* map, HeapObject* object);
};


typedef FlexibleBodyDescriptor<HeapObject::kHeaderSize> StructBodyDescriptor;

}  // namespace internal
}  // namespace v8

#endif  // V8_OBJECTS_BODY_DESCRIPTORS_H_
