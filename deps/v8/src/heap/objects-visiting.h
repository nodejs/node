// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_HEAP_OBJECTS_VISITING_H_
#define V8_HEAP_OBJECTS_VISITING_H_

#include "src/objects/fixed-array.h"
#include "src/objects/map.h"
#include "src/objects/object-list-macros.h"
#include "src/objects/objects.h"
#include "src/objects/visitors.h"

namespace v8 {
namespace internal {

#define TYPED_VISITOR_ID_LIST(V)        \
  V(AccessorInfo)                       \
  V(AllocationSite)                     \
  V(BigInt)                             \
  V(ByteArray)                          \
  V(BytecodeArray)                      \
  V(CallHandlerInfo)                    \
  V(Cell)                               \
  V(InstructionStream)                  \
  V(Code)                               \
  V(CoverageInfo)                       \
  V(DataHandler)                        \
  V(EmbedderDataArray)                  \
  V(EphemeronHashTable)                 \
  V(ExternalString)                     \
  V(FeedbackCell)                       \
  V(FeedbackMetadata)                   \
  V(FixedDoubleArray)                   \
  V(JSArrayBuffer)                      \
  V(JSDataViewOrRabGsabDataView)        \
  V(JSExternalObject)                   \
  V(JSFinalizationRegistry)             \
  V(JSFunction)                         \
  V(JSObject)                           \
  V(JSSynchronizationPrimitive)         \
  V(JSTypedArray)                       \
  V(WeakCell)                           \
  V(JSWeakCollection)                   \
  V(JSWeakRef)                          \
  V(Map)                                \
  V(NativeContext)                      \
  V(Oddball)                            \
  V(PreparseData)                       \
  V(PromiseOnStack)                     \
  V(PropertyArray)                      \
  V(PropertyCell)                       \
  V(PrototypeInfo)                      \
  V(SharedFunctionInfo)                 \
  V(SmallOrderedHashMap)                \
  V(SmallOrderedHashSet)                \
  V(SmallOrderedNameDictionary)         \
  V(SourceTextModule)                   \
  V(SwissNameDictionary)                \
  V(Symbol)                             \
  V(SyntheticModule)                    \
  V(TransitionArray)                    \
  IF_WASM(V, WasmApiFunctionRef)        \
  IF_WASM(V, WasmArray)                 \
  IF_WASM(V, WasmCapiFunctionData)      \
  IF_WASM(V, WasmExportedFunctionData)  \
  IF_WASM(V, WasmFunctionData)          \
  IF_WASM(V, WasmIndirectFunctionTable) \
  IF_WASM(V, WasmInstanceObject)        \
  IF_WASM(V, WasmInternalFunction)      \
  IF_WASM(V, WasmJSFunctionData)        \
  IF_WASM(V, WasmStruct)                \
  IF_WASM(V, WasmSuspenderObject)       \
  IF_WASM(V, WasmResumeData)            \
  IF_WASM(V, WasmTypeInfo)              \
  IF_WASM(V, WasmContinuationObject)    \
  IF_WASM(V, WasmNull)

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
class HeapVisitor : public ObjectVisitorWithCageBases {
 public:
  inline HeapVisitor(PtrComprCageBase cage_base,
                     PtrComprCageBase code_cage_base);
  inline explicit HeapVisitor(Isolate* isolate);
  inline explicit HeapVisitor(Heap* heap);

  V8_INLINE ResultType Visit(HeapObject object);
  V8_INLINE ResultType Visit(Map map, HeapObject object);

 protected:
  // A guard predicate for visiting the object.
  // If it returns false then the default implementations of the Visit*
  // functions bail out from iterating the object pointers.
  V8_INLINE bool ShouldVisit(HeapObject object) { return true; }

  // If this predicate return false the default implementations of Visit*
  // functions bail out from visiting the map pointer.
  V8_INLINE static constexpr bool ShouldVisitMapPointer() { return true; }

  // Only visits the Map pointer if `ShouldVisitMapPointer()` returns true.
  V8_INLINE void VisitMapPointerIfNeeded(HeapObject host);

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

  template <typename T, typename TBodyDescriptor = typename T::BodyDescriptor>
  V8_INLINE ResultType VisitJSObjectSubclass(Map map, T object);

  template <typename T>
  static V8_INLINE T Cast(HeapObject object);
};

template <typename ConcreteVisitor>
class NewSpaceVisitor : public HeapVisitor<int, ConcreteVisitor> {
 public:
  V8_INLINE explicit NewSpaceVisitor(Isolate* isolate);

 protected:
  V8_INLINE static constexpr bool ShouldVisitMapPointer() { return false; }

  // Special cases for young generation.
  V8_INLINE int VisitNativeContext(Map map, NativeContext object);
  V8_INLINE int VisitBytecodeArray(Map map, BytecodeArray object);
  V8_INLINE int VisitSharedFunctionInfo(Map map, SharedFunctionInfo object);
  V8_INLINE int VisitWeakCell(Map map, WeakCell weak_cell);

  friend class HeapVisitor<int, ConcreteVisitor>;
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
