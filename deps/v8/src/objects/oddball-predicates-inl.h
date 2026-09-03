// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_ODDBALL_PREDICATES_INL_H_
#define V8_OBJECTS_ODDBALL_PREDICATES_INL_H_

#include "src/objects/oddball-predicates.h"
// Include the non-inl header before the rest of the headers.

#include "src/common/globals.h"
#include "src/common/ptr-compr.h"
#include "src/objects/heap-object.h"
#include "src/objects/tagged-impl-inl.h"
#include "src/roots/roots-inl.h"

namespace v8::internal {

#define IS_TYPE_FUNCTION_DEF(Type, ...)          \
  inline bool Is##Type(Tagged<HeapObject> obj) { \
    return Is##Type(Tagged<Object>(obj));        \
  }                                              \
  inline bool Is##Type(const HeapObject* obj) {  \
    return Is##Type(Tagged<Object>(obj));        \
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

bool IsAnyHole(Tagged<Object> obj) {
  Tagged<HeapObject> ho;
  return TryCast<HeapObject>(obj, &ho) && IsAnyHole(ho);
}

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

}  // namespace v8::internal

#endif  // V8_OBJECTS_ODDBALL_PREDICATES_INL_H_
