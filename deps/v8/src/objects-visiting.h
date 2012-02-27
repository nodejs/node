// Copyright 2011 the V8 project authors. All rights reserved.
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
//       copyright notice, this list of conditions and the following
//       disclaimer in the documentation and/or other materials provided
//       with the distribution.
//     * Neither the name of Google Inc. nor the names of its
//       contributors may be used to endorse or promote products derived
//       from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#ifndef V8_OBJECTS_VISITING_H_
#define V8_OBJECTS_VISITING_H_

#include "allocation.h"

// This file provides base classes and auxiliary methods for defining
// static object visitors used during GC.
// Visiting HeapObject body with a normal ObjectVisitor requires performing
// two switches on object's instance type to determine object size and layout
// and one or more virtual method calls on visitor itself.
// Static visitor is different: it provides a dispatch table which contains
// pointers to specialized visit functions. Each map has the visitor_id
// field which contains an index of specialized visitor to use.

namespace v8 {
namespace internal {


// Base class for all static visitors.
class StaticVisitorBase : public AllStatic {
 public:
  enum VisitorId {
    kVisitSeqAsciiString = 0,
    kVisitSeqTwoByteString,
    kVisitShortcutCandidate,
    kVisitByteArray,
    kVisitFreeSpace,
    kVisitFixedArray,
    kVisitFixedDoubleArray,
    kVisitGlobalContext,

    // For data objects, JS objects and structs along with generic visitor which
    // can visit object of any size we provide visitors specialized by
    // object size in words.
    // Ids of specialized visitors are declared in a linear order (without
    // holes) starting from the id of visitor specialized for 2 words objects
    // (base visitor id) and ending with the id of generic visitor.
    // Method GetVisitorIdForSize depends on this ordering to calculate visitor
    // id of specialized visitor from given instance size, base visitor id and
    // generic visitor's id.

    kVisitDataObject,
    kVisitDataObject2 = kVisitDataObject,
    kVisitDataObject3,
    kVisitDataObject4,
    kVisitDataObject5,
    kVisitDataObject6,
    kVisitDataObject7,
    kVisitDataObject8,
    kVisitDataObject9,
    kVisitDataObjectGeneric,

    kVisitJSObject,
    kVisitJSObject2 = kVisitJSObject,
    kVisitJSObject3,
    kVisitJSObject4,
    kVisitJSObject5,
    kVisitJSObject6,
    kVisitJSObject7,
    kVisitJSObject8,
    kVisitJSObject9,
    kVisitJSObjectGeneric,

    kVisitStruct,
    kVisitStruct2 = kVisitStruct,
    kVisitStruct3,
    kVisitStruct4,
    kVisitStruct5,
    kVisitStruct6,
    kVisitStruct7,
    kVisitStruct8,
    kVisitStruct9,
    kVisitStructGeneric,

    kVisitConsString,
    kVisitSlicedString,
    kVisitOddball,
    kVisitCode,
    kVisitMap,
    kVisitPropertyCell,
    kVisitSharedFunctionInfo,
    kVisitJSFunction,
    kVisitJSWeakMap,
    kVisitJSRegExp,

    kVisitorIdCount,
    kMinObjectSizeInWords = 2
  };

  // Visitor ID should fit in one byte.
  STATIC_ASSERT(kVisitorIdCount <= 256);

  // Determine which specialized visitor should be used for given instance type
  // and instance type.
  static VisitorId GetVisitorId(int instance_type, int instance_size);

  static VisitorId GetVisitorId(Map* map) {
    return GetVisitorId(map->instance_type(), map->instance_size());
  }

  // For visitors that allow specialization by size calculate VisitorId based
  // on size, base visitor id and generic visitor id.
  static VisitorId GetVisitorIdForSize(VisitorId base,
                                       VisitorId generic,
                                       int object_size) {
    ASSERT((base == kVisitDataObject) ||
           (base == kVisitStruct) ||
           (base == kVisitJSObject));
    ASSERT(IsAligned(object_size, kPointerSize));
    ASSERT(kMinObjectSizeInWords * kPointerSize <= object_size);
    ASSERT(object_size < Page::kMaxNonCodeHeapObjectSize);

    const VisitorId specialization = static_cast<VisitorId>(
        base + (object_size >> kPointerSizeLog2) - kMinObjectSizeInWords);

    return Min(specialization, generic);
  }
};


template<typename Callback>
class VisitorDispatchTable {
 public:
  void CopyFrom(VisitorDispatchTable* other) {
    // We are not using memcpy to guarantee that during update
    // every element of callbacks_ array will remain correct
    // pointer (memcpy might be implemented as a byte copying loop).
    for (int i = 0; i < StaticVisitorBase::kVisitorIdCount; i++) {
      NoBarrier_Store(&callbacks_[i], other->callbacks_[i]);
    }
  }

  inline Callback GetVisitorById(StaticVisitorBase::VisitorId id) {
    return reinterpret_cast<Callback>(callbacks_[id]);
  }

  inline Callback GetVisitor(Map* map) {
    return reinterpret_cast<Callback>(callbacks_[map->visitor_id()]);
  }

  void Register(StaticVisitorBase::VisitorId id, Callback callback) {
    ASSERT(id < StaticVisitorBase::kVisitorIdCount);  // id is unsigned.
    callbacks_[id] = reinterpret_cast<AtomicWord>(callback);
  }

  template<typename Visitor,
           StaticVisitorBase::VisitorId base,
           StaticVisitorBase::VisitorId generic,
           int object_size_in_words>
  void RegisterSpecialization() {
    static const int size = object_size_in_words * kPointerSize;
    Register(StaticVisitorBase::GetVisitorIdForSize(base, generic, size),
             &Visitor::template VisitSpecialized<size>);
  }


  template<typename Visitor,
           StaticVisitorBase::VisitorId base,
           StaticVisitorBase::VisitorId generic>
  void RegisterSpecializations() {
    STATIC_ASSERT(
        (generic - base + StaticVisitorBase::kMinObjectSizeInWords) == 10);
    RegisterSpecialization<Visitor, base, generic, 2>();
    RegisterSpecialization<Visitor, base, generic, 3>();
    RegisterSpecialization<Visitor, base, generic, 4>();
    RegisterSpecialization<Visitor, base, generic, 5>();
    RegisterSpecialization<Visitor, base, generic, 6>();
    RegisterSpecialization<Visitor, base, generic, 7>();
    RegisterSpecialization<Visitor, base, generic, 8>();
    RegisterSpecialization<Visitor, base, generic, 9>();
    Register(generic, &Visitor::Visit);
  }

 private:
  AtomicWord callbacks_[StaticVisitorBase::kVisitorIdCount];
};


template<typename StaticVisitor>
class BodyVisitorBase : public AllStatic {
 public:
  INLINE(static void IteratePointers(Heap* heap,
                                     HeapObject* object,
                                     int start_offset,
                                     int end_offset)) {
    Object** start_slot = reinterpret_cast<Object**>(object->address() +
                                                     start_offset);
    Object** end_slot = reinterpret_cast<Object**>(object->address() +
                                                   end_offset);
    StaticVisitor::VisitPointers(heap, start_slot, end_slot);
  }
};


template<typename StaticVisitor, typename BodyDescriptor, typename ReturnType>
class FlexibleBodyVisitor : public BodyVisitorBase<StaticVisitor> {
 public:
  static inline ReturnType Visit(Map* map, HeapObject* object) {
    int object_size = BodyDescriptor::SizeOf(map, object);
    BodyVisitorBase<StaticVisitor>::IteratePointers(
        map->GetHeap(),
        object,
        BodyDescriptor::kStartOffset,
        object_size);
    return static_cast<ReturnType>(object_size);
  }

  template<int object_size>
  static inline ReturnType VisitSpecialized(Map* map, HeapObject* object) {
    ASSERT(BodyDescriptor::SizeOf(map, object) == object_size);
    BodyVisitorBase<StaticVisitor>::IteratePointers(
        map->GetHeap(),
        object,
        BodyDescriptor::kStartOffset,
        object_size);
    return static_cast<ReturnType>(object_size);
  }
};


template<typename StaticVisitor, typename BodyDescriptor, typename ReturnType>
class FixedBodyVisitor : public BodyVisitorBase<StaticVisitor> {
 public:
  static inline ReturnType Visit(Map* map, HeapObject* object) {
    BodyVisitorBase<StaticVisitor>::IteratePointers(
        map->GetHeap(),
        object,
        BodyDescriptor::kStartOffset,
        BodyDescriptor::kEndOffset);
    return static_cast<ReturnType>(BodyDescriptor::kSize);
  }
};


// Base class for visitors used for a linear new space iteration.
// IterateBody returns size of visited object.
// Certain types of objects (i.e. Code objects) are not handled
// by dispatch table of this visitor because they cannot appear
// in the new space.
//
// This class is intended to be used in the following way:
//
//   class SomeVisitor : public StaticNewSpaceVisitor<SomeVisitor> {
//     ...
//   }
//
// This is an example of Curiously recurring template pattern
// (see http://en.wikipedia.org/wiki/Curiously_recurring_template_pattern).
// We use CRTP to guarantee aggressive compile time optimizations (i.e.
// inlining and specialization of StaticVisitor::VisitPointers methods).
template<typename StaticVisitor>
class StaticNewSpaceVisitor : public StaticVisitorBase {
 public:
  static void Initialize();

  static inline int IterateBody(Map* map, HeapObject* obj) {
    return table_.GetVisitor(map)(map, obj);
  }

  static inline void VisitPointers(Heap* heap, Object** start, Object** end) {
    for (Object** p = start; p < end; p++) StaticVisitor::VisitPointer(heap, p);
  }

 private:
  static inline int VisitByteArray(Map* map, HeapObject* object) {
    return reinterpret_cast<ByteArray*>(object)->ByteArraySize();
  }

  static inline int VisitFixedDoubleArray(Map* map, HeapObject* object) {
    int length = reinterpret_cast<FixedDoubleArray*>(object)->length();
    return FixedDoubleArray::SizeFor(length);
  }

  static inline int VisitJSObject(Map* map, HeapObject* object) {
    return JSObjectVisitor::Visit(map, object);
  }

  static inline int VisitSeqAsciiString(Map* map, HeapObject* object) {
    return SeqAsciiString::cast(object)->
        SeqAsciiStringSize(map->instance_type());
  }

  static inline int VisitSeqTwoByteString(Map* map, HeapObject* object) {
    return SeqTwoByteString::cast(object)->
        SeqTwoByteStringSize(map->instance_type());
  }

  static inline int VisitFreeSpace(Map* map, HeapObject* object) {
    return FreeSpace::cast(object)->Size();
  }

  class DataObjectVisitor {
   public:
    template<int object_size>
    static inline int VisitSpecialized(Map* map, HeapObject* object) {
      return object_size;
    }

    static inline int Visit(Map* map, HeapObject* object) {
      return map->instance_size();
    }
  };

  typedef FlexibleBodyVisitor<StaticVisitor,
                              StructBodyDescriptor,
                              int> StructVisitor;

  typedef FlexibleBodyVisitor<StaticVisitor,
                              JSObject::BodyDescriptor,
                              int> JSObjectVisitor;

  typedef int (*Callback)(Map* map, HeapObject* object);

  static VisitorDispatchTable<Callback> table_;
};


template<typename StaticVisitor>
VisitorDispatchTable<typename StaticNewSpaceVisitor<StaticVisitor>::Callback>
  StaticNewSpaceVisitor<StaticVisitor>::table_;


} }  // namespace v8::internal

#endif  // V8_OBJECTS_VISITING_H_
