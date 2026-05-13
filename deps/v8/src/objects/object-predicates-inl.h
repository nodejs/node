// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_OBJECT_PREDICATES_INL_H_
#define V8_OBJECTS_OBJECT_PREDICATES_INL_H_

#include "src/objects/object-predicates.h"
// Include the non-inl header before the rest of the headers.

#include "src/common/globals.h"
#include "src/common/ptr-compr.h"
#include "src/objects/casting.h"
#include "src/objects/instance-type-checker.h"
#include "src/objects/name-inl.h"
#include "src/objects/oddball.h"
#include "src/objects/smi.h"
#include "src/objects/tagged-index.h"
#include "src/roots/roots.h"

namespace v8::internal {

bool IsTaggedIndex(Tagged<Object> obj) {
  return IsSmi(obj) &&
         TaggedIndex::IsValid(Tagged<TaggedIndex>(obj.ptr()).value());
}

#define IS_TYPE_FUNCTION_DEF(type_)                        \
  inline bool Is##type_(Tagged<Object> obj) {              \
    Tagged<HeapObject> ho;                                 \
    return TryCast<HeapObject>(obj, &ho) && Is##type_(ho); \
  }                                                        \
  inline bool Is##type_(const HeapObject* obj) {           \
    return Is##type_(Tagged<HeapObject>(obj));             \
  }
HEAP_OBJECT_TYPE_LIST(IS_TYPE_FUNCTION_DEF)
IS_TYPE_FUNCTION_DEF(HashTableBase)
IS_TYPE_FUNCTION_DEF(SmallOrderedHashTable)
IS_TYPE_FUNCTION_DEF(PropertyDictionary)
IS_TYPE_FUNCTION_DEF(AnyHole)
#undef IS_TYPE_FUNCTION_DEF

#define IS_TYPE_FUNCTION_DEF(Type, ...)                                        \
  inline bool Is##Type(Tagged<Object> obj, Isolate*) { return Is##Type(obj); } \
  inline bool Is##Type(Tagged<Object> obj, LocalIsolate*) {                    \
    return Is##Type(obj);                                                      \
  }                                                                            \
  inline bool Is##Type(Tagged<Object> obj, ReadOnlyRoots) {                    \
    return Is##Type(obj);                                                      \
  }                                                                            \
  inline bool Is##Type(Tagged<HeapObject> obj) {                               \
    return Is##Type(Tagged<Object>(obj));                                      \
  }                                                                            \
  inline bool Is##Type(const HeapObject* obj, Isolate*) {                      \
    return Is##Type(Tagged<Object>(obj));                                      \
  }                                                                            \
  inline bool Is##Type(const HeapObject* obj) {                                \
    return Is##Type(Tagged<Object>(obj));                                      \
  }
ODDBALL_LIST(IS_TYPE_FUNCTION_DEF)
HOLE_LIST(IS_TYPE_FUNCTION_DEF)
IS_TYPE_FUNCTION_DEF(UndefinedContextCell)
#undef IS_TYPE_FUNCTION_DEF

#if V8_STATIC_ROOTS_BOOL
#define IS_TYPE_FUNCTION_DEF(Type, Value, CamelName)                           \
  bool Is##Type(Tagged<Object> obj) {                                          \
    SLOW_DCHECK(CheckObjectComparisonAllowed(                                  \
        obj.ptr(), GetReadOnlyRoots().Value().ptr()));                         \
    return V8HeapCompressionScheme::CompressObject(obj.ptr()) ==               \
           StaticReadOnlyRoot::k##CamelName;                                   \
  }                                                                            \
  bool Is##Type(Tagged<Object> obj, EarlyReadOnlyRoots roots) {                \
    SLOW_DCHECK(CheckObjectComparisonAllowed(obj.ptr(), roots.Value().ptr())); \
    return V8HeapCompressionScheme::CompressObject(obj.ptr()) ==               \
           StaticReadOnlyRoot::k##CamelName;                                   \
  }
#else
#define IS_TYPE_FUNCTION_DEF(Type, Value, _)                    \
  bool Is##Type(Tagged<Object> obj) {                           \
    return obj == GetReadOnlyRoots().Value();                   \
  }                                                             \
  bool Is##Type(Tagged<Object> obj, EarlyReadOnlyRoots roots) { \
    return obj == roots.Value();                                \
  }
#endif
ODDBALL_LIST(IS_TYPE_FUNCTION_DEF)
HOLE_LIST(IS_TYPE_FUNCTION_DEF)
IS_TYPE_FUNCTION_DEF(UndefinedContextCell, undefined_context_cell,
                     UndefinedContextCell)
#undef IS_TYPE_FUNCTION_DEF

namespace detail {
#if V8_STATIC_ROOTS_BOOL
#define GET_HOLE_ROOT(Type, Value, CamelName) StaticReadOnlyRoot::k##CamelName,
constexpr Tagged_t kMinStaticHoleValue = std::min({HOLE_LIST(GET_HOLE_ROOT)});
constexpr Tagged_t kMaxStaticHoleValue = std::max({HOLE_LIST(GET_HOLE_ROOT)});
#undef GET_HOLE_ROOT
#endif

inline bool IsAnyHoleNoSpaceCheck(Tagged<HeapObject> obj) {
#if V8_STATIC_ROOTS_BOOL
  return base::IsInRange(static_cast<Tagged_t>(obj.ptr()), kMinStaticHoleValue,
                         kMaxStaticHoleValue);
#else
  return obj->map()->instance_type() == HOLE_TYPE;
#endif
}
}  // namespace detail

bool IsAnyHole(Tagged<HeapObject> obj) {
  if (detail::IsAnyHoleNoSpaceCheck(obj)) {
#if V8_STATIC_ROOTS_BOOL
    if (V8_UNLIKELY(!obj.IsInMainCageBase())) {
      return false;
    }
#endif
    return true;
  }
  return false;
}

bool IsNullOrUndefined(Tagged<Object> obj, Isolate*) {
  return IsNullOrUndefined(obj);
}

bool IsNullOrUndefined(Tagged<Object> obj, LocalIsolate*) {
  return IsNullOrUndefined(obj);
}

bool IsNullOrUndefined(Tagged<Object> obj, ReadOnlyRoots) {
  return IsNullOrUndefined(obj);
}

bool IsNullOrUndefined(Tagged<Object> obj, EarlyReadOnlyRoots roots) {
  return IsNull(obj, roots) || IsUndefined(obj, roots);
}

bool IsNullOrUndefined(Tagged<Object> obj) {
  return IsNull(obj) || IsUndefined(obj);
}

bool IsNullOrUndefined(Tagged<HeapObject> obj) {
#if V8_STATIC_ROOTS_BOOL
  static_assert(StaticReadOnlyRoot::kUndefinedValue ==
                StaticReadOnlyRoot::kFirstAllocatedRoot);
  static_assert(StaticReadOnlyRoot::kNullValue ==
                StaticReadOnlyRoot::kUndefinedValue + sizeof(Undefined));
  return V8HeapCompressionScheme::CompressObject(obj.ptr()) <=
         StaticReadOnlyRoot::kNullValue;
#else
  return IsNull(obj) || IsUndefined(obj);
#endif
}

bool IsZero(Tagged<Object> obj) { return obj == Smi::zero(); }

bool IsPublicSymbol(Tagged<Object> obj) {
  Tagged<Symbol> symbol;
  return TryCast<Symbol>(obj, &symbol) && !symbol->is_any_private();
}
bool IsPrivateSymbol(Tagged<Object> obj) {
  Tagged<Symbol> symbol;
  return TryCast<Symbol>(obj, &symbol) && symbol->is_any_private();
}

bool IsNoSharedNameSentinel(Tagged<Object> obj) {
  return obj == SharedFunctionInfo::kNoSharedNameSentinel;
}

bool IsJSObjectThatCanBeTrackedAsPrototype(Tagged<Object> obj) {
  return IsHeapObject(obj) &&
         IsJSObjectThatCanBeTrackedAsPrototype(Cast<HeapObject>(obj));
}

bool IsJSObjectThatCanBeTrackedAsPrototype(Tagged<HeapObject> obj) {
  return IsJSObject(obj) && !HeapLayout::InWritableSharedSpace(obj);
}

bool IsAnyObjectThatCanBeTrackedAsPrototype(Tagged<Object> obj) {
  return IsHeapObject(obj) &&
         IsAnyObjectThatCanBeTrackedAsPrototype(Cast<HeapObject>(obj));
}

bool IsAnyObjectThatCanBeTrackedAsPrototype(Tagged<HeapObject> obj) {
  return (IsJSObject(obj) || IsWasmObject(obj)) &&
         !HeapLayout::InWritableSharedSpace(obj);
}

bool IsNumber(Tagged<Object> obj) {
  if (IsSmi(obj)) return true;
  return IsHeapNumber(Cast<HeapObject>(obj));
}

bool IsNumeric(Tagged<Object> obj) {
  if (IsSmi(obj)) return true;
  Tagged<HeapObject> heap_object = Cast<HeapObject>(obj);
  return IsHeapNumber(heap_object) || IsBigInt(heap_object);
}

bool IsMetaMap(Tagged<Map> map) {
  return InstanceTypeChecker::IsMap(map->instance_type());
}

bool IsMetaMap(Tagged<HeapObject> obj) {
  if (!IsMap(obj)) return false;
  return IsMetaMap(UncheckedCast<Map>(obj));
}

bool IsExtendedMap(Tagged<Map> map) {
  DCHECK_IMPLIES(map->is_extended_map(), !IsMetaMap(map));
  return map->is_extended_map();
}

bool IsExtendedMap(Tagged<HeapObject> obj) {
  if (!IsMap(obj)) return false;
  return IsExtendedMap(UncheckedCast<Map>(obj));
}

bool IsJSInterceptorMap(Tagged<Map> map) {
  return IsExtendedMap(map) && (UncheckedCast<ExtendedMap>(map)->map_kind() ==
                                ExtendedMapKind::kJSInterceptorMap);
}

bool IsJSInterceptorMap(Tagged<HeapObject> obj) {
  if (!IsMap(obj)) return false;
  return IsJSInterceptorMap(UncheckedCast<Map>(obj));
}

bool IsPrimitive(Tagged<Object> obj) {
  if (obj.IsSmi()) return true;
  return IsPrimitiveMap(Cast<HeapObject>(obj)->map());
}

#define MAKE_STRUCT_PREDICATE(NAME, Name, name)                  \
  inline bool Is##Name(Tagged<Object> obj) {                     \
    return IsHeapObject(obj) && Is##Name(Cast<HeapObject>(obj)); \
  }                                                              \
  inline bool Is##Name(const HeapObject* obj) {                  \
    return Is##Name(Tagged<HeapObject>(obj));                    \
  }
STRUCT_LIST(MAKE_STRUCT_PREDICATE)
#undef MAKE_STRUCT_PREDICATE

}  // namespace v8::internal

#endif  // V8_OBJECTS_OBJECT_PREDICATES_INL_H_
