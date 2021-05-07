// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_HEAP_OBJECTS_VISITING_H_
#define V8_HEAP_OBJECTS_VISITING_H_

#include "src/objects/fixed-array.h"
#include "src/objects/map.h"
#include "src/objects/objects.h"
#include "src/objects/visitors.h"
#include "torque-generated/field-offsets.h"

namespace v8 {
namespace internal {

#define TYPED_VISITOR_ID_LIST(V) \
  V(AllocationSite)              \
  V(BigInt)                      \
  V(ByteArray)                   \
  V(BytecodeArray)               \
  V(Cell)                        \
  V(Code)                        \
  V(CodeDataContainer)           \
  V(CoverageInfo)                \
  V(DataHandler)                 \
  V(EmbedderDataArray)           \
  V(EphemeronHashTable)          \
  V(FeedbackCell)                \
  V(FeedbackMetadata)            \
  V(FixedDoubleArray)            \
  V(JSArrayBuffer)               \
  V(JSDataView)                  \
  V(JSFunction)                  \
  V(JSObject)                    \
  V(JSTypedArray)                \
  V(WeakCell)                    \
  V(JSWeakCollection)            \
  V(JSWeakRef)                   \
  V(Map)                         \
  V(NativeContext)               \
  V(PreparseData)                \
  V(PropertyArray)               \
  V(PropertyCell)                \
  V(PrototypeInfo)               \
  V(SmallOrderedHashMap)         \
  V(SmallOrderedHashSet)         \
  V(SmallOrderedNameDictionary)  \
  V(SourceTextModule)            \
  V(SwissNameDictionary)         \
  V(Symbol)                      \
  V(SyntheticModule)             \
  V(TransitionArray)             \
  V(WasmArray)                   \
  V(WasmIndirectFunctionTable)   \
  V(WasmInstanceObject)          \
  V(WasmStruct)                  \
  V(WasmTypeInfo)

#define FORWARD_DECLARE(TypeName) class TypeName;
TYPED_VISITOR_ID_LIST(FORWARD_DECLARE)
TORQUE_VISITOR_ID_LIST(FORWARD_DECLARE)
#undef FORWARD_DECLARE

// The base class for visitors that need to dispatch on object type. The default
// behavior of all visit functions is to iterate body of the given object using
// the BodyDescriptor of the object.
//
// The visit functions return the size of the object cast to ResultType.
//
// This class is intended to be used in the following way:
//
//   class SomeVisitor : public HeapVisitor<ResultType, SomeVisitor> {
//     ...
//   }
template <typename ResultType, typename ConcreteVisitor>
class HeapVisitor : public ObjectVisitor {
 public:
  V8_INLINE ResultType Visit(HeapObject object);
  V8_INLINE ResultType Visit(Map map, HeapObject object);

 protected:
  // A guard predicate for visiting the object.
  // If it returns false then the default implementations of the Visit*
  // functions bailout from iterating the object pointers.
  V8_INLINE bool ShouldVisit(HeapObject object) { return true; }
  // Guard predicate for visiting the objects map pointer separately.
  V8_INLINE bool ShouldVisitMapPointer() { return true; }
  // A callback for visiting the map pointer in the object header.
  V8_INLINE void VisitMapPointer(HeapObject host);
  // If this predicate returns false, then the heap visitor will fail
  // in default Visit implemention for subclasses of JSObject.
  V8_INLINE bool AllowDefaultJSObjectVisit() { return true; }

#define VISIT(TypeName) \
  V8_INLINE ResultType Visit##TypeName(Map map, TypeName object);
  TYPED_VISITOR_ID_LIST(VISIT)
  TORQUE_VISITOR_ID_LIST(VISIT)
#undef VISIT
  V8_INLINE ResultType VisitShortcutCandidate(Map map, ConsString object);
  V8_INLINE ResultType VisitDataObject(Map map, HeapObject object);
  V8_INLINE ResultType VisitJSObjectFast(Map map, JSObject object);
  V8_INLINE ResultType VisitJSApiObject(Map map, JSObject object);
  V8_INLINE ResultType VisitStruct(Map map, HeapObject object);
  V8_INLINE ResultType VisitFreeSpace(Map map, FreeSpace object);

  template <typename T>
  static V8_INLINE T Cast(HeapObject object);
};

template <typename ConcreteVisitor>
class NewSpaceVisitor : public HeapVisitor<int, ConcreteVisitor> {
 public:
  V8_INLINE bool ShouldVisitMapPointer() { return false; }

  // Special cases for young generation.

  V8_INLINE int VisitNativeContext(Map map, NativeContext object);
  V8_INLINE int VisitJSApiObject(Map map, JSObject object);

  int VisitBytecodeArray(Map map, BytecodeArray object) {
    UNREACHABLE();
    return 0;
  }

  int VisitSharedFunctionInfo(Map map, SharedFunctionInfo object);
  int VisitWeakCell(Map map, WeakCell weak_cell);
};

class WeakObjectRetainer;

// A weak list is single linked list where each element has a weak pointer to
// the next element. Given the head of the list, this function removes dead
// elements from the list and if requested records slots for next-element
// pointers. The template parameter T is a WeakListVisitor that defines how to
// access the next-element pointers.
template <class T>
Object VisitWeakList(Heap* heap, Object list, WeakObjectRetainer* retainer);
}  // namespace internal
}  // namespace v8

#endif  // V8_HEAP_OBJECTS_VISITING_H_
