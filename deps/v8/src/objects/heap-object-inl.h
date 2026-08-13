// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_HEAP_OBJECT_INL_H_
#define V8_OBJECTS_HEAP_OBJECT_INL_H_

#include "src/objects/heap-object.h"
// Include the non-inl header before the rest of the headers.

#include "src/common/ptr-compr-inl.h"
#include "src/objects/instance-type-inl.h"
#include "src/objects/map-word-inl.h"
#include "src/objects/tagged-field-inl.h"

// Has to be the last include (doesn't have include guards):
#include "src/objects/object-macros.h"

namespace v8 {
namespace internal {

MapWord HeapObject::map_word(RelaxedLoadTag tag) const {
  return MapField::Relaxed_Load_Map_Word(this);
}

MapWord HeapObject::map_word(AcquireLoadTag tag) const {
  return MapField::Acquire_Load_No_Unpack(this);
}

Tagged<Map> HeapObject::map() const { return map_word(kRelaxedLoad).ToMap(); }

DEF_ACQUIRE_GETTER(HeapObject, map, Tagged<Map>) {
  return map_word(kAcquireLoad).ToMap();
}

int HeapObject::Size() const { return SizeFromMap(map()); }

SafeHeapObjectSize HeapObject::SafeSize() const {
  return SafeSizeFromMap(map());
}

#define TYPE_CHECKER(type, ...)                                  \
  inline bool Is##type(Tagged<HeapObject> obj) {                 \
    Tagged<Map> map_object = obj->map();                         \
    return InstanceTypeChecker::Is##type(map_object);            \
  }                                                              \
  inline bool Is##type(Tagged<Object> obj) {                     \
    Tagged<HeapObject> ho;                                       \
    return TryCast<HeapObject>(obj, &ho) && Is##type(ho);        \
  }                                                              \
  inline bool Is##type(Tagged<HeapObject> obj, AcquireLoadTag) { \
    Tagged<Map> map_object = obj->map(kAcquireLoad);             \
    return InstanceTypeChecker::Is##type(map_object);            \
  }

INSTANCE_TYPE_CHECKERS(TYPE_CHECKER)
#undef TYPE_CHECKER

DEF_HEAP_OBJECT_PREDICATE(IsDependentCode) { return IsWeakArrayList(obj); }
DEF_HEAP_OBJECT_PREDICATE(IsSmiStringCache) { return IsFixedArray(obj); }

DEF_CAST_TRAITS(ArrayList)
DEF_CAST_TRAITS(ByteArray)
DEF_CAST_TRAITS(ConsString)
DEF_CAST_TRAITS(DependentCode)
DEF_CAST_TRAITS(DoubleStringCache)
DEF_CAST_TRAITS(ExternalOneByteString)
DEF_CAST_TRAITS(ExternalString)
DEF_CAST_TRAITS(ExternalTwoByteString)
DEF_CAST_TRAITS(FixedArray)
DEF_CAST_TRAITS(FixedDoubleArray)
DEF_CAST_TRAITS(FreeSpace)
DEF_CAST_TRAITS(FunctionTemplateInfo)
DEF_CAST_TRAITS(HeapNumber)
DEF_CAST_TRAITS(InternalizedString)
DEF_CAST_TRAITS(JSProxy)
DEF_CAST_TRAITS(Map)
DEF_CAST_TRAITS(Name)
DEF_CAST_TRAITS(NameToIndexHashTable)
DEF_CAST_TRAITS(ObjectTemplateInfo)
DEF_CAST_TRAITS(PropertyArray)
DEF_CAST_TRAITS(ScopeInfo)
DEF_CAST_TRAITS(SeqOneByteString)
DEF_CAST_TRAITS(SeqString)
DEF_CAST_TRAITS(SeqTwoByteString)
DEF_CAST_TRAITS(SlicedString)
DEF_CAST_TRAITS(SmiStringCache)
DEF_CAST_TRAITS(String)
DEF_CAST_TRAITS(Symbol)
DEF_CAST_TRAITS(ThinString)
DEF_CAST_TRAITS(WeakArrayList)
DEF_CAST_TRAITS(WeakFixedArray)

// All Struct subclasses' cast traits only need an instance-type check, so they
// live here (rather than objects-inl.h) for global visibility without forcing
// dependents to depend on objects-inl.h.
#define IS_HELPER_DEF_STRUCT(NAME, Name, name) DEF_CAST_TRAITS(Name)
STRUCT_LIST(IS_HELPER_DEF_STRUCT)
#undef IS_HELPER_DEF_STRUCT

// Support writing "IsFoo(this)" for types where "Is<Foo>(this)" doesn't work.
#define HEAPOBJECT_PTR_OVERLOAD(type, ...)      \
  inline bool Is##type(const HeapObject* obj) { \
    return Is##type(Tagged<HeapObject>(obj));   \
  }

HEAPOBJECT_PTR_OVERLOAD(FreeSpaceOrFiller)
HEAPOBJECT_PTR_OVERLOAD(JSError)
HEAPOBJECT_PTR_OVERLOAD(SmallOrderedHashTable)

#undef HEAPOBJECT_PTR_OVERLOAD

}  // namespace internal
}  // namespace v8

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_HEAP_OBJECT_INL_H_
