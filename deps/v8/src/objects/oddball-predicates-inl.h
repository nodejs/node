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

inline bool IsWasmNullNoSpaceCheck(Tagged<HeapObject> obj) {
#if V8_STATIC_ROOTS_BOOL && V8_ENABLE_WEBASSEMBLY
  return static_cast<Tagged_t>(obj.ptr()) == StaticReadOnlyRoot::kWasmNull;
#elif V8_ENABLE_WEBASSEMBLY
  return obj->map()->instance_type() == WASM_NULL_TYPE;
#else
  return false;
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

inline bool IsWasmNull(Tagged<Object> obj) {
  Tagged<HeapObject> ho;
  return TryCast<HeapObject>(obj, &ho) && IsWasmNull(ho);
}

inline bool IsWasmNull(Tagged<HeapObject> obj) {
#if V8_ENABLE_WEBASSEMBLY
  if (detail::IsWasmNullNoSpaceCheck(obj)) {
#if V8_STATIC_ROOTS_BOOL
    // Compressed object tests need to be done on a matching compression scheme.
    // WasmNull is always in the main cage's RO space. If the object is in a
    // different cage, IsWasmNullNoSpaceCheck may have returned a false positive
    // due to address aliasing.
    //
    // Only check this after the WasmNull check succeeds, to make it cheaper in
    // the common case that things aren't nulls.
    if (V8_UNLIKELY(!obj.IsInMainCageBase())) {
#if defined(DEBUG) && CONTIGUOUS_COMPRESSED_READ_ONLY_SPACE_BOOL
      // When contiguous compressed RO space is enabled, the trusted space guard
      // region is large enough (kContiguousReadOnlyReservationSize) to prevent
      // aliasing with WasmNull. This DCHECK verifies that assumption.
      DCHECK(obj.IsInTrustedCageBase());
      DCHECK_GT(TrustedSpaceCompressionScheme::CompressObject(obj.ptr()),
                StaticReadOnlyRoot::kWasmNull);
#endif
      // Object is not in main cage, so it can't be WasmNull (which is in RO
      // space which is in the main cage).
      return false;
    }
#endif  // V8_STATIC_ROOTS_BOOL
    return true;
  }
#endif  // V8_ENABLE_WEBASSEMBLY
  return false;
}

// Returns true if {obj} is entirely unmapped, so even its map cannot be read.
inline bool IsInaccessible(Tagged<HeapObject> obj) {
#if V8_ENABLE_WEBASSEMBLY && \
    (V8_STATIC_ROOTS_BOOL || V8_STATIC_ROOTS_GENERATION_BOOL)
  // For now, only WasmNull has an inaccessible map, so this function only
  // serves to document why we're checking for WasmNull in some places.
  // More objects may be added here in the future.
  return IsWasmNull(obj);
#else
  return false;
#endif  // V8_ENABLE_WEBASSEMBLY && ...
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
