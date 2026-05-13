// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_OBJECT_PREDICATES_H_
#define V8_OBJECTS_OBJECT_PREDICATES_H_

#include "src/common/globals.h"
#include "src/objects/object-list-macros.h"
#include "src/objects/objects-definitions.h"
#include "src/objects/tagged.h"

namespace v8::internal {

class EarlyReadOnlyRoots;
class HeapObject;
class Isolate;
class LocalIsolate;
class Object;
class ReadOnlyRoots;
class Smi;

template <HeapObjectReferenceType kRefType, typename StorageType>
V8_INLINE constexpr bool IsObject(TaggedImpl<kRefType, StorageType> obj) {
  return obj.IsObject();
}
template <HeapObjectReferenceType kRefType, typename StorageType>
V8_INLINE constexpr bool IsSmi(TaggedImpl<kRefType, StorageType> obj) {
  return obj.IsSmi();
}
template <HeapObjectReferenceType kRefType, typename StorageType>
V8_INLINE constexpr bool IsHeapObject(TaggedImpl<kRefType, StorageType> obj) {
  return obj.IsHeapObject();
}
template <typename StorageType>
V8_INLINE constexpr bool IsWeak(
    TaggedImpl<HeapObjectReferenceType::WEAK, StorageType> obj) {
  return obj.IsWeak();
}

// TODO(leszeks): These exist both as free functions and members of Tagged. They
// probably want to be cleaned up at some point.
V8_INLINE bool IsSmi(Tagged<Object> obj) { return obj.IsSmi(); }
V8_INLINE bool IsSmi(Tagged<HeapObject> obj) { return obj.IsSmi(); }
V8_INLINE bool IsSmi(Tagged<Smi> obj) { return true; }

V8_INLINE bool IsHeapObject(Tagged<Object> obj) { return obj.IsHeapObject(); }
V8_INLINE bool IsHeapObject(Tagged<HeapObject> obj) {
  return obj.IsHeapObject();
}
V8_INLINE bool IsHeapObject(Tagged<Smi> obj) { return false; }

V8_INLINE bool IsTaggedIndex(Tagged<Object> obj);

#define IS_TYPE_FUNCTION_DECL(Type) V8_INLINE bool Is##Type(Tagged<Object> obj);
OBJECT_TYPE_LIST(IS_TYPE_FUNCTION_DECL)
HEAP_OBJECT_TYPE_LIST(IS_TYPE_FUNCTION_DECL)
IS_TYPE_FUNCTION_DECL(HashTableBase)
IS_TYPE_FUNCTION_DECL(SloppyArgumentsElements)
IS_TYPE_FUNCTION_DECL(SmallOrderedHashTable)
IS_TYPE_FUNCTION_DECL(PropertyDictionary)
// A wrapper around IsHole to make it easier to distinguish from specific hole
// checks (e.g. IsTheHole).
IS_TYPE_FUNCTION_DECL(AnyHole)
#undef IS_TYPE_FUNCTION_DECL

V8_INLINE bool IsNumber(Tagged<Object> obj, ReadOnlyRoots roots);

// Oddball checks are faster when they are raw pointer comparisons, so the
// isolate/read-only roots overloads should be preferred where possible.
#define IS_TYPE_FUNCTION_DECL(Type, ...)                                 \
  V8_INLINE bool Is##Type(Tagged<Object> obj, Isolate* isolate);         \
  V8_INLINE bool Is##Type(Tagged<Object> obj, LocalIsolate* isolate);    \
  V8_INLINE bool Is##Type(Tagged<Object> obj, ReadOnlyRoots roots);      \
  V8_INLINE bool Is##Type(Tagged<Object> obj, EarlyReadOnlyRoots roots); \
  V8_INLINE bool Is##Type(Tagged<Object> obj);
ODDBALL_LIST(IS_TYPE_FUNCTION_DECL)
HOLE_LIST(IS_TYPE_FUNCTION_DECL)
IS_TYPE_FUNCTION_DECL(UndefinedContextCell)
IS_TYPE_FUNCTION_DECL(NullOrUndefined)
#undef IS_TYPE_FUNCTION_DECL

V8_INLINE bool IsZero(Tagged<Object> obj);
V8_INLINE bool IsNoSharedNameSentinel(Tagged<Object> obj);
V8_INLINE bool IsPrivateSymbol(Tagged<Object> obj);
V8_INLINE bool IsPublicSymbol(Tagged<Object> obj);
#if !V8_ENABLE_WEBASSEMBLY
// Dummy implementation on builds without WebAssembly.
template <typename T>
V8_INLINE bool IsWasmObject(T obj, Isolate* = nullptr) {
  return false;
}
#endif

// Returns true for JS and Wasm objects not on the shared heap.
V8_INLINE bool IsAnyObjectThatCanBeTrackedAsPrototype(Tagged<Object> obj);
V8_INLINE bool IsAnyObjectThatCanBeTrackedAsPrototype(Tagged<HeapObject> obj);
// Same, but only permits JS objects.
V8_INLINE bool IsJSObjectThatCanBeTrackedAsPrototype(Tagged<Object> obj);
V8_INLINE bool IsJSObjectThatCanBeTrackedAsPrototype(Tagged<HeapObject> obj);

#define DECL_STRUCT_PREDICATE(NAME, Name, name) \
  V8_INLINE bool Is##Name(Tagged<Object> obj);
STRUCT_LIST(DECL_STRUCT_PREDICATE)
#undef DECL_STRUCT_PREDICATE

V8_INLINE bool IsNaN(Tagged<Object> obj);
V8_INLINE bool IsMinusZero(Tagged<Object> obj);

// Returns whether the object is safe to share across Isolates.
inline bool IsShared(Tagged<Object> obj);

}  // namespace v8::internal

#endif  // V8_OBJECTS_OBJECT_PREDICATES_H_
