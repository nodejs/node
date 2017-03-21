// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_VISITING_H_
#define V8_OBJECTS_VISITING_H_

#include "src/allocation.h"
#include "src/heap/embedder-tracing.h"
#include "src/heap/heap.h"
#include "src/heap/spaces.h"
#include "src/layout-descriptor.h"
#include "src/objects-body-descriptors.h"

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
#define VISITOR_ID_LIST(V) \
  V(SeqOneByteString)      \
  V(SeqTwoByteString)      \
  V(ShortcutCandidate)     \
  V(ByteArray)             \
  V(BytecodeArray)         \
  V(FreeSpace)             \
  V(FixedArray)            \
  V(FixedDoubleArray)      \
  V(FixedTypedArray)       \
  V(FixedFloat64Array)     \
  V(NativeContext)         \
  V(AllocationSite)        \
  V(DataObject2)           \
  V(DataObject3)           \
  V(DataObject4)           \
  V(DataObject5)           \
  V(DataObject6)           \
  V(DataObject7)           \
  V(DataObject8)           \
  V(DataObject9)           \
  V(DataObjectGeneric)     \
  V(JSObject2)             \
  V(JSObject3)             \
  V(JSObject4)             \
  V(JSObject5)             \
  V(JSObject6)             \
  V(JSObject7)             \
  V(JSObject8)             \
  V(JSObject9)             \
  V(JSObjectGeneric)       \
  V(JSApiObject2)          \
  V(JSApiObject3)          \
  V(JSApiObject4)          \
  V(JSApiObject5)          \
  V(JSApiObject6)          \
  V(JSApiObject7)          \
  V(JSApiObject8)          \
  V(JSApiObject9)          \
  V(JSApiObjectGeneric)    \
  V(Struct2)               \
  V(Struct3)               \
  V(Struct4)               \
  V(Struct5)               \
  V(Struct6)               \
  V(Struct7)               \
  V(Struct8)               \
  V(Struct9)               \
  V(StructGeneric)         \
  V(ConsString)            \
  V(SlicedString)          \
  V(Symbol)                \
  V(Oddball)               \
  V(Code)                  \
  V(Map)                   \
  V(Cell)                  \
  V(PropertyCell)          \
  V(WeakCell)              \
  V(TransitionArray)       \
  V(SharedFunctionInfo)    \
  V(JSFunction)            \
  V(JSWeakCollection)      \
  V(JSArrayBuffer)         \
  V(JSRegExp)

  // For data objects, JS objects and structs along with generic visitor which
  // can visit object of any size we provide visitors specialized by
  // object size in words.
  // Ids of specialized visitors are declared in a linear order (without
  // holes) starting from the id of visitor specialized for 2 words objects
  // (base visitor id) and ending with the id of generic visitor.
  // Method GetVisitorIdForSize depends on this ordering to calculate visitor
  // id of specialized visitor from given instance size, base visitor id and
  // generic visitor's id.
  enum VisitorId {
#define VISITOR_ID_ENUM_DECL(id) kVisit##id,
    VISITOR_ID_LIST(VISITOR_ID_ENUM_DECL)
#undef VISITOR_ID_ENUM_DECL
        kVisitorIdCount,
    kVisitDataObject = kVisitDataObject2,
    kVisitJSObject = kVisitJSObject2,
    kVisitJSApiObject = kVisitJSApiObject2,
    kVisitStruct = kVisitStruct2,
  };

  // Visitor ID should fit in one byte.
  STATIC_ASSERT(kVisitorIdCount <= 256);

  // Determine which specialized visitor should be used for given instance type
  // and instance type.
  static VisitorId GetVisitorId(int instance_type, int instance_size,
                                bool has_unboxed_fields);

  // Determine which specialized visitor should be used for given map.
  static VisitorId GetVisitorId(Map* map);

  // For visitors that allow specialization by size calculate VisitorId based
  // on size, base visitor id and generic visitor id.
  static VisitorId GetVisitorIdForSize(VisitorId base, VisitorId generic,
                                       int object_size,
                                       bool has_unboxed_fields) {
    DCHECK((base == kVisitDataObject) || (base == kVisitStruct) ||
           (base == kVisitJSObject) || (base == kVisitJSApiObject));
    DCHECK(IsAligned(object_size, kPointerSize));
    DCHECK(Heap::kMinObjectSizeInWords * kPointerSize <= object_size);
    DCHECK(object_size <= kMaxRegularHeapObjectSize);
    DCHECK(!has_unboxed_fields || (base == kVisitJSObject) ||
           (base == kVisitJSApiObject));

    if (has_unboxed_fields) return generic;

    int visitor_id = Min(
        base + (object_size >> kPointerSizeLog2) - Heap::kMinObjectSizeInWords,
        static_cast<int>(generic));

    return static_cast<VisitorId>(visitor_id);
  }
};


template <typename Callback>
class VisitorDispatchTable {
 public:
  void CopyFrom(VisitorDispatchTable* other) {
    // We are not using memcpy to guarantee that during update
    // every element of callbacks_ array will remain correct
    // pointer (memcpy might be implemented as a byte copying loop).
    for (int i = 0; i < StaticVisitorBase::kVisitorIdCount; i++) {
      base::NoBarrier_Store(&callbacks_[i], other->callbacks_[i]);
    }
  }

  inline Callback GetVisitor(Map* map);

  inline Callback GetVisitorById(StaticVisitorBase::VisitorId id) {
    return reinterpret_cast<Callback>(callbacks_[id]);
  }

  void Register(StaticVisitorBase::VisitorId id, Callback callback) {
    DCHECK(id < StaticVisitorBase::kVisitorIdCount);  // id is unsigned.
    callbacks_[id] = reinterpret_cast<base::AtomicWord>(callback);
  }

  template <typename Visitor, StaticVisitorBase::VisitorId base,
            StaticVisitorBase::VisitorId generic, int object_size_in_words>
  void RegisterSpecialization() {
    static const int size = object_size_in_words * kPointerSize;
    Register(StaticVisitorBase::GetVisitorIdForSize(base, generic, size, false),
             &Visitor::template VisitSpecialized<size>);
  }


  template <typename Visitor, StaticVisitorBase::VisitorId base,
            StaticVisitorBase::VisitorId generic>
  void RegisterSpecializations() {
    STATIC_ASSERT((generic - base + Heap::kMinObjectSizeInWords) == 10);
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
  base::AtomicWord callbacks_[StaticVisitorBase::kVisitorIdCount];
};


template <typename StaticVisitor, typename BodyDescriptor, typename ReturnType>
class FlexibleBodyVisitor : public AllStatic {
 public:
  INLINE(static ReturnType Visit(Map* map, HeapObject* object)) {
    int object_size = BodyDescriptor::SizeOf(map, object);
    BodyDescriptor::template IterateBody<StaticVisitor>(object, object_size);
    return static_cast<ReturnType>(object_size);
  }

  // This specialization is only suitable for objects containing pointer fields.
  template <int object_size>
  static inline ReturnType VisitSpecialized(Map* map, HeapObject* object) {
    DCHECK(BodyDescriptor::SizeOf(map, object) == object_size);
    DCHECK(!FLAG_unbox_double_fields || map->HasFastPointerLayout());
    StaticVisitor::VisitPointers(
        object->GetHeap(), object,
        HeapObject::RawField(object, BodyDescriptor::kStartOffset),
        HeapObject::RawField(object, object_size));
    return static_cast<ReturnType>(object_size);
  }
};


template <typename StaticVisitor, typename BodyDescriptor, typename ReturnType>
class FixedBodyVisitor : public AllStatic {
 public:
  INLINE(static ReturnType Visit(Map* map, HeapObject* object)) {
    BodyDescriptor::template IterateBody<StaticVisitor>(object);
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
template <typename StaticVisitor>
class StaticNewSpaceVisitor : public StaticVisitorBase {
 public:
  static void Initialize();

  INLINE(static int IterateBody(Map* map, HeapObject* obj)) {
    return table_.GetVisitor(map)(map, obj);
  }

  INLINE(static void VisitPointers(Heap* heap, HeapObject* object,
                                   Object** start, Object** end)) {
    for (Object** p = start; p < end; p++) {
      StaticVisitor::VisitPointer(heap, object, p);
    }
  }

  // Although we are using the JSFunction body descriptor which does not
  // visit the code entry, compiler wants it to be accessible.
  // See JSFunction::BodyDescriptorImpl.
  inline static void VisitCodeEntry(Heap* heap, HeapObject* object,
                                    Address entry_address) {
    UNREACHABLE();
  }

 private:
  inline static int UnreachableVisitor(Map* map, HeapObject* object) {
    UNREACHABLE();
    return 0;
  }

  INLINE(static int VisitByteArray(Map* map, HeapObject* object)) {
    return reinterpret_cast<ByteArray*>(object)->ByteArraySize();
  }

  INLINE(static int VisitFixedDoubleArray(Map* map, HeapObject* object)) {
    int length = reinterpret_cast<FixedDoubleArray*>(object)->length();
    return FixedDoubleArray::SizeFor(length);
  }

  INLINE(static int VisitJSObject(Map* map, HeapObject* object)) {
    return JSObjectVisitor::Visit(map, object);
  }

  INLINE(static int VisitSeqOneByteString(Map* map, HeapObject* object)) {
    return SeqOneByteString::cast(object)
        ->SeqOneByteStringSize(map->instance_type());
  }

  INLINE(static int VisitSeqTwoByteString(Map* map, HeapObject* object)) {
    return SeqTwoByteString::cast(object)
        ->SeqTwoByteStringSize(map->instance_type());
  }

  INLINE(static int VisitFreeSpace(Map* map, HeapObject* object)) {
    return FreeSpace::cast(object)->size();
  }

  class DataObjectVisitor {
   public:
    template <int object_size>
    static inline int VisitSpecialized(Map* map, HeapObject* object) {
      return object_size;
    }

    INLINE(static int Visit(Map* map, HeapObject* object)) {
      return map->instance_size();
    }
  };

  typedef FlexibleBodyVisitor<StaticVisitor, StructBodyDescriptor, int>
      StructVisitor;

  typedef FlexibleBodyVisitor<StaticVisitor, JSObject::BodyDescriptor, int>
      JSObjectVisitor;

  typedef int (*Callback)(Map* map, HeapObject* object);

  static VisitorDispatchTable<Callback> table_;
};


template <typename StaticVisitor>
VisitorDispatchTable<typename StaticNewSpaceVisitor<StaticVisitor>::Callback>
    StaticNewSpaceVisitor<StaticVisitor>::table_;


// Base class for visitors used to transitively mark the entire heap.
// IterateBody returns nothing.
// Certain types of objects might not be handled by this base class and
// no visitor function is registered by the generic initialization. A
// specialized visitor function needs to be provided by the inheriting
// class itself for those cases.
//
// This class is intended to be used in the following way:
//
//   class SomeVisitor : public StaticMarkingVisitor<SomeVisitor> {
//     ...
//   }
//
// This is an example of Curiously recurring template pattern.
template <typename StaticVisitor>
class StaticMarkingVisitor : public StaticVisitorBase {
 public:
  static void Initialize();

  INLINE(static void IterateBody(Map* map, HeapObject* obj)) {
    table_.GetVisitor(map)(map, obj);
  }

  INLINE(static void VisitWeakCell(Map* map, HeapObject* object));
  INLINE(static void VisitTransitionArray(Map* map, HeapObject* object));
  INLINE(static void VisitCodeEntry(Heap* heap, HeapObject* object,
                                    Address entry_address));
  INLINE(static void VisitEmbeddedPointer(Heap* heap, RelocInfo* rinfo));
  INLINE(static void VisitCell(Heap* heap, RelocInfo* rinfo));
  INLINE(static void VisitDebugTarget(Heap* heap, RelocInfo* rinfo));
  INLINE(static void VisitCodeTarget(Heap* heap, RelocInfo* rinfo));
  INLINE(static void VisitCodeAgeSequence(Heap* heap, RelocInfo* rinfo));
  INLINE(static void VisitExternalReference(RelocInfo* rinfo)) {}
  INLINE(static void VisitInternalReference(RelocInfo* rinfo)) {}
  INLINE(static void VisitRuntimeEntry(RelocInfo* rinfo)) {}
  // Skip the weak next code link in a code object.
  INLINE(static void VisitNextCodeLink(Heap* heap, Object** slot)) {}

 protected:
  INLINE(static void VisitMap(Map* map, HeapObject* object));
  INLINE(static void VisitCode(Map* map, HeapObject* object));
  INLINE(static void VisitBytecodeArray(Map* map, HeapObject* object));
  INLINE(static void VisitSharedFunctionInfo(Map* map, HeapObject* object));
  INLINE(static void VisitWeakCollection(Map* map, HeapObject* object));
  INLINE(static void VisitJSFunction(Map* map, HeapObject* object));
  INLINE(static void VisitNativeContext(Map* map, HeapObject* object));

  // Mark pointers in a Map treating some elements of the descriptor array weak.
  static void MarkMapContents(Heap* heap, Map* map);

  // Code flushing support.
  INLINE(static bool IsFlushable(Heap* heap, JSFunction* function));
  INLINE(static bool IsFlushable(Heap* heap, SharedFunctionInfo* shared_info));

  // Helpers used by code flushing support that visit pointer fields and treat
  // references to code objects either strongly or weakly.
  static void VisitSharedFunctionInfoStrongCode(Map* map, HeapObject* object);
  static void VisitSharedFunctionInfoWeakCode(Map* map, HeapObject* object);
  static void VisitJSFunctionStrongCode(Map* map, HeapObject* object);
  static void VisitJSFunctionWeakCode(Map* map, HeapObject* object);

  class DataObjectVisitor {
   public:
    template <int size>
    static inline void VisitSpecialized(Map* map, HeapObject* object) {}

    INLINE(static void Visit(Map* map, HeapObject* object)) {}
  };

  typedef FlexibleBodyVisitor<StaticVisitor, FixedArray::BodyDescriptor, void>
      FixedArrayVisitor;

  typedef FlexibleBodyVisitor<StaticVisitor, JSObject::BodyDescriptor, void>
      JSObjectVisitor;

  class JSApiObjectVisitor : AllStatic {
   public:
    template <int size>
    static inline void VisitSpecialized(Map* map, HeapObject* object) {
      TracePossibleWrapper(object);
      JSObjectVisitor::template VisitSpecialized<size>(map, object);
    }

    INLINE(static void Visit(Map* map, HeapObject* object)) {
      TracePossibleWrapper(object);
      JSObjectVisitor::Visit(map, object);
    }

   private:
    INLINE(static void TracePossibleWrapper(HeapObject* object)) {
      if (object->GetHeap()->local_embedder_heap_tracer()->InUse()) {
        DCHECK(object->IsJSObject());
        object->GetHeap()->TracePossibleWrapper(JSObject::cast(object));
      }
    }
  };

  typedef FlexibleBodyVisitor<StaticVisitor, StructBodyDescriptor, void>
      StructObjectVisitor;

  typedef void (*Callback)(Map* map, HeapObject* object);

  static VisitorDispatchTable<Callback> table_;
};


template <typename StaticVisitor>
VisitorDispatchTable<typename StaticMarkingVisitor<StaticVisitor>::Callback>
    StaticMarkingVisitor<StaticVisitor>::table_;


class WeakObjectRetainer;


// A weak list is single linked list where each element has a weak pointer to
// the next element. Given the head of the list, this function removes dead
// elements from the list and if requested records slots for next-element
// pointers. The template parameter T is a WeakListVisitor that defines how to
// access the next-element pointers.
template <class T>
Object* VisitWeakList(Heap* heap, Object* list, WeakObjectRetainer* retainer);
}  // namespace internal
}  // namespace v8

#endif  // V8_OBJECTS_VISITING_H_
