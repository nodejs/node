// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_HEAP_OBJECTS_VISITING_H_
#define V8_HEAP_OBJECTS_VISITING_H_

#include "src/objects.h"
#include "src/objects/fixed-array.h"
#include "src/objects/map.h"
#include "src/visitors.h"

namespace v8 {
namespace internal {

#define TYPED_VISITOR_ID_LIST_CLASSES(V)                                  \
  V(AllocationSite, AllocationSite)                                       \
  V(BigInt, BigInt)                                                       \
  V(ByteArray, ByteArray)                                                 \
  V(BytecodeArray, BytecodeArray)                                         \
  V(Cell, Cell)                                                           \
  V(Code, Code)                                                           \
  V(CodeDataContainer, CodeDataContainer)                                 \
  V(ConsString, ConsString)                                               \
  V(Context, Context)                                                     \
  V(DataHandler, DataHandler)                                             \
  V(DescriptorArray, DescriptorArray)                                     \
  V(EmbedderDataArray, EmbedderDataArray)                                 \
  V(EphemeronHashTable, EphemeronHashTable)                               \
  V(FeedbackCell, FeedbackCell)                                           \
  V(FeedbackVector, FeedbackVector)                                       \
  V(FixedArray, FixedArray)                                               \
  V(FixedDoubleArray, FixedDoubleArray)                                   \
  V(FixedTypedArrayBase, FixedTypedArrayBase)                             \
  V(JSArrayBuffer, JSArrayBuffer)                                         \
  V(JSDataView, JSDataView)                                               \
  V(JSFunction, JSFunction)                                               \
  V(JSObject, JSObject)                                                   \
  V(JSTypedArray, JSTypedArray)                                           \
  V(WeakCell, WeakCell)                                                   \
  V(JSWeakCollection, JSWeakCollection)                                   \
  V(JSWeakRef, JSWeakRef)                                                 \
  V(Map, Map)                                                             \
  V(NativeContext, NativeContext)                                         \
  V(Oddball, Oddball)                                                     \
  V(PreparseData, PreparseData)                                           \
  V(PropertyArray, PropertyArray)                                         \
  V(PropertyCell, PropertyCell)                                           \
  V(PrototypeInfo, PrototypeInfo)                                         \
  V(SeqOneByteString, SeqOneByteString)                                   \
  V(SeqTwoByteString, SeqTwoByteString)                                   \
  V(SharedFunctionInfo, SharedFunctionInfo)                               \
  V(SlicedString, SlicedString)                                           \
  V(SmallOrderedHashMap, SmallOrderedHashMap)                             \
  V(SmallOrderedHashSet, SmallOrderedHashSet)                             \
  V(SmallOrderedNameDictionary, SmallOrderedNameDictionary)               \
  V(Symbol, Symbol)                                                       \
  V(ThinString, ThinString)                                               \
  V(TransitionArray, TransitionArray)                                     \
  V(UncompiledDataWithoutPreparseData, UncompiledDataWithoutPreparseData) \
  V(UncompiledDataWithPreparseData, UncompiledDataWithPreparseData)       \
  V(WasmInstanceObject, WasmInstanceObject)

#define FORWARD_DECLARE(TypeName, Type) class Type;
TYPED_VISITOR_ID_LIST_CLASSES(FORWARD_DECLARE)
#undef FORWARD_DECLARE

#define TYPED_VISITOR_ID_LIST_TYPEDEFS(V) \
  V(FixedFloat64Array, FixedFloat64Array)

#define TYPED_VISITOR_ID_LIST(V)   \
  TYPED_VISITOR_ID_LIST_CLASSES(V) \
  TYPED_VISITOR_ID_LIST_TYPEDEFS(V)

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
  V8_INLINE void VisitMapPointer(HeapObject host, MapWordSlot map_slot);
  // If this predicate returns false, then the heap visitor will fail
  // in default Visit implemention for subclasses of JSObject.
  V8_INLINE bool AllowDefaultJSObjectVisit() { return true; }

#define VISIT(TypeName, Type) \
  V8_INLINE ResultType Visit##TypeName(Map map, Type object);
  TYPED_VISITOR_ID_LIST(VISIT)
#undef VISIT
  V8_INLINE ResultType VisitShortcutCandidate(Map map, ConsString object);
  V8_INLINE ResultType VisitDataObject(Map map, HeapObject object);
  V8_INLINE ResultType VisitJSObjectFast(Map map, JSObject object);
  V8_INLINE ResultType VisitJSApiObject(Map map, JSObject object);
  V8_INLINE ResultType VisitStruct(Map map, HeapObject object);
  V8_INLINE ResultType VisitFreeSpace(Map map, FreeSpace object);
  V8_INLINE ResultType VisitWeakArray(Map map, HeapObject object);

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
template <class T>
Object VisitWeakList2(Heap* heap, Object list, WeakObjectRetainer* retainer);
}  // namespace internal
}  // namespace v8

#endif  // V8_HEAP_OBJECTS_VISITING_H_
