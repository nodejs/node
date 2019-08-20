// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_OBJECTS_BODY_DESCRIPTORS_H_
#define V8_OBJECTS_OBJECTS_BODY_DESCRIPTORS_H_

#include "src/objects/map.h"
#include "src/objects/objects.h"

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
//   static bool IsValidSlot(Map map, HeapObject obj, int offset);
//
//
// 2) Iterate object's body using stateful object visitor.
//
//   template <typename ObjectVisitor>
//   static inline void IterateBody(Map map, HeapObject obj, int object_size,
//                                  ObjectVisitor* v);
class BodyDescriptorBase {
 public:
  template <typename ObjectVisitor>
  static inline void IteratePointers(HeapObject obj, int start_offset,
                                     int end_offset, ObjectVisitor* v);

  template <typename ObjectVisitor>
  static inline void IteratePointer(HeapObject obj, int offset,
                                    ObjectVisitor* v);

  template <typename ObjectVisitor>
  static inline void IterateCustomWeakPointers(HeapObject obj, int start_offset,
                                               int end_offset,
                                               ObjectVisitor* v);

  template <typename ObjectVisitor>
  static inline void IterateCustomWeakPointer(HeapObject obj, int offset,
                                              ObjectVisitor* v);

  template <typename ObjectVisitor>
  static inline void IterateEphemeron(HeapObject obj, int index, int key_offset,
                                      int value_offset, ObjectVisitor* v);

  template <typename ObjectVisitor>
  static inline void IterateMaybeWeakPointers(HeapObject obj, int start_offset,
                                              int end_offset, ObjectVisitor* v);

  template <typename ObjectVisitor>
  static inline void IterateMaybeWeakPointer(HeapObject obj, int offset,
                                             ObjectVisitor* v);

 protected:
  // Returns true for all header and embedder fields.
  static inline bool IsValidJSObjectSlotImpl(Map map, HeapObject obj,
                                             int offset);

  // Returns true for all header and embedder fields.
  static inline bool IsValidEmbedderJSObjectSlotImpl(Map map, HeapObject obj,
                                                     int offset);

  // Treats all header and embedder fields in the range as tagged.
  template <typename ObjectVisitor>
  static inline void IterateJSObjectBodyImpl(Map map, HeapObject obj,
                                             int start_offset, int end_offset,
                                             ObjectVisitor* v);
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

  static bool IsValidSlot(Map map, HeapObject obj, int offset) {
    return offset >= kStartOffset && offset < kEndOffset;
  }

  template <typename ObjectVisitor>
  static inline void IterateBody(Map map, HeapObject obj, ObjectVisitor* v) {
    IteratePointers(obj, start_offset, end_offset, v);
  }

  template <typename ObjectVisitor>
  static inline void IterateBody(Map map, HeapObject obj, int object_size,
                                 ObjectVisitor* v) {
    IterateBody(map, obj, v);
  }

  static inline int SizeOf(Map map, HeapObject object) { return kSize; }
};

// This class describes a body of an object of a variable size
// in which all pointer fields are located in the [start_offset, object_size)
// interval.
template <int start_offset>
class FlexibleBodyDescriptor final : public BodyDescriptorBase {
 public:
  static const int kStartOffset = start_offset;

  static bool IsValidSlot(Map map, HeapObject obj, int offset) {
    return (offset >= kStartOffset);
  }

  template <typename ObjectVisitor>
  static inline void IterateBody(Map map, HeapObject obj, int object_size,
                                 ObjectVisitor* v) {
    IteratePointers(obj, start_offset, object_size, v);
  }

  static inline int SizeOf(Map map, HeapObject object);
};

using StructBodyDescriptor = FlexibleBodyDescriptor<HeapObject::kHeaderSize>;

template <int start_offset>
class FlexibleWeakBodyDescriptor final : public BodyDescriptorBase {
 public:
  static const int kStartOffset = start_offset;

  static bool IsValidSlot(Map map, HeapObject obj, int offset) {
    return (offset >= kStartOffset);
  }

  template <typename ObjectVisitor>
  static inline void IterateBody(Map map, HeapObject obj, int object_size,
                                 ObjectVisitor* v) {
    IterateMaybeWeakPointers(obj, start_offset, object_size, v);
  }

  static inline int SizeOf(Map map, HeapObject object);
};

// This class describes a body of an object which has a parent class that also
// has a body descriptor. This represents a union of the parent's body
// descriptor, and a new descriptor for the child -- so, both parent and child's
// slots are iterated. The parent must be fixed size, and its slots be disjoint
// with the child's.
template <class ParentBodyDescriptor, class ChildBodyDescriptor>
class SubclassBodyDescriptor final : public BodyDescriptorBase {
 public:
  // The parent must end be before the child's start offset, to make sure that
  // their slots are disjoint.
  STATIC_ASSERT(ParentBodyDescriptor::kSize <=
                ChildBodyDescriptor::kStartOffset);

  static bool IsValidSlot(Map map, HeapObject obj, int offset) {
    return ParentBodyDescriptor::IsValidSlot(map, obj, offset) ||
           ChildBodyDescriptor::IsValidSlot(map, obj, offset);
  }

  template <typename ObjectVisitor>
  static inline void IterateBody(Map map, HeapObject obj, ObjectVisitor* v) {
    ParentBodyDescriptor::IterateBody(map, obj, v);
    ChildBodyDescriptor::IterateBody(map, obj, v);
  }

  template <typename ObjectVisitor>
  static inline void IterateBody(Map map, HeapObject obj, int object_size,
                                 ObjectVisitor* v) {
    ParentBodyDescriptor::IterateBody(map, obj, object_size, v);
    ChildBodyDescriptor::IterateBody(map, obj, object_size, v);
  }

  static inline int SizeOf(Map map, HeapObject object) {
    // The child should know its full size.
    return ChildBodyDescriptor::SizeOf(map, object);
  }
};

}  // namespace internal
}  // namespace v8

#endif  // V8_OBJECTS_OBJECTS_BODY_DESCRIPTORS_H_
