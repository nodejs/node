// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_TRUSTED_POINTER_H_
#define V8_OBJECTS_TRUSTED_POINTER_H_

#include "src/common/globals.h"
#include "src/objects/heap-object.h"
#include "src/objects/objects.h"
#include "src/objects/tagged-field.h"
#include "src/objects/tagged.h"
#include "src/sandbox/indirect-pointer-tag.h"

// Has to be the last include (doesn't have include guards):
#include "src/objects/object-macros.h"

namespace v8 {
namespace internal {

class IsolateForSandbox;
class Code;
class BytecodeArray;
class InterpreterData;
class UncompiledData;
class RegExpData;
class WasmDispatchTable;
class WasmTrustedInstanceData;
class WasmInternalFunction;
class WasmSuspenderObject;
class WasmFunctionData;

template <typename T, IndirectPointerTag kTag>
class TrustedPointerMember;

namespace detail {

#define TRUSTED_POINTER_TAG_TO_TYPE_MAP(V)                                  \
  V(kCodeIndirectPointerTag, Code)                                          \
  V(kBytecodeArrayIndirectPointerTag, BytecodeArray)                        \
  V(kInterpreterDataIndirectPointerTag, InterpreterData)                    \
  V(kUncompiledDataIndirectPointerTag, UncompiledData)                      \
  V(kRegExpDataIndirectPointerTag, RegExpData)                              \
  IF_WASM(V, kWasmDispatchTableIndirectPointerTag, WasmDispatchTable)       \
  IF_WASM(V, kSharedWasmDispatchTableIndirectPointerTag, WasmDispatchTable) \
  IF_WASM(V, kWasmTrustedInstanceDataIndirectPointerTag,                    \
          WasmTrustedInstanceData)                                          \
  IF_WASM(V, kSharedWasmTrustedInstanceDataIndirectPointerTag,              \
          WasmTrustedInstanceData)                                          \
  IF_WASM(V, kWasmInternalFunctionIndirectPointerTag, WasmInternalFunction) \
  IF_WASM(V, kWasmSuspenderIndirectPointerTag, WasmSuspenderObject)         \
  IF_WASM(V, kWasmFunctionDataIndirectPointerTag, WasmFunctionData)

template <IndirectPointerTag tag>
struct TrustedPointerType;

#define DEFINE_TRUSTED_POINTER_TYPE(Tag, Type) \
  template <>                                  \
  struct TrustedPointerType<Tag> {             \
    using type = Type;                         \
  };

TRUSTED_POINTER_TAG_TO_TYPE_MAP(DEFINE_TRUSTED_POINTER_TYPE)
#undef DEFINE_TRUSTED_POINTER_TYPE

}  // namespace detail

template <IndirectPointerTag tag>
using TrustedTypeFor = typename detail::TrustedPointerType<tag>::type;

// TrustedPointerField provides static methods for reading and writing trusted
// pointers from HeapObjects.
//
// A pointer to a trusted object. When the sandbox is enabled, these are
// indirect pointers using the the TrustedPointerTable (TPT). When the sandbox
// is disabled, they are regular tagged pointers. They must always point to an
// ExposedTrustedObject as (only) these objects can be referenced through the
// trusted pointer table.
class TrustedPointerField {
 public:
  template <IndirectPointerTag tag>
  static inline Tagged<TrustedTypeFor<tag>> ReadTrustedPointerField(
      Tagged<HeapObject> host, size_t offset, IsolateForSandbox isolate);

  template <IndirectPointerTag tag>
  static inline Tagged<TrustedTypeFor<tag>> ReadTrustedPointerField(
      Tagged<HeapObject> host, size_t offset, IsolateForSandbox isolate,
      AcquireLoadTag acquire_load);

  // Like ReadTrustedPointerField, but if the field is cleared, this will
  // return Smi::zero().
  template <IndirectPointerTag tag>
  static inline Tagged<Object> ReadMaybeEmptyTrustedPointerField(
      Tagged<HeapObject> host, size_t offset, IsolateForSandbox isolate,
      AcquireLoadTag);

  template <IndirectPointerTag tag>
  static inline void WriteTrustedPointerField(
      Tagged<HeapObject> host, size_t offset,
      Tagged<ExposedTrustedObject> value);

  // Trusted pointer fields can be cleared/empty, in which case they no longer
  // point to any object. When the sandbox is enabled, this will set the fields
  // indirect pointer handle to the null handle (referencing the zeroth entry
  // in the TrustedPointerTable which just contains nullptr). When the sandbox
  // is disabled, this will set the field to Smi::zero().
  static inline bool IsTrustedPointerFieldEmpty(Tagged<HeapObject> host,
                                                size_t offset);

  template <typename IsolateT>
  static inline bool IsTrustedPointerFieldUnpublished(Tagged<HeapObject> host,
                                                      size_t offset,
                                                      IndirectPointerTag tag,
                                                      IsolateT isolate);

  static inline void ClearTrustedPointerField(Tagged<HeapObject> host,
                                              size_t offset);

  static inline void ClearTrustedPointerField(Tagged<HeapObject> host,
                                              size_t offset, ReleaseStoreTag);
};

// TrustedPointerMember is a type wrapping a trusted pointer for
// HeapObjectLayout objects.
//
// This is a HeapObjectLayout version of TrustedPointerField, similar to
// TaggedMember vs. TaggedField.
//
// TODO(leszeks): Remove TrustedPointerField (and update these comments) when
// all objects are ported.
template <typename T, IndirectPointerTag kTag>
class TrustedPointerMember {
 public:
  constexpr TrustedPointerMember() = default;

  inline Tagged<T> load(IsolateForSandbox isolate) const;
  inline Tagged<Object> load_maybe_empty(IsolateForSandbox isolate,
                                         AcquireLoadTag) const;
  inline void store(HeapObjectLayout* host, Tagged<T> value,
                    WriteBarrierMode mode = UPDATE_WRITE_BARRIER);
  inline void store_no_write_barrier(HeapObjectLayout* host, Tagged<T> value);
  inline void clear(HeapObjectLayout* host);

 private:
#ifdef V8_ENABLE_SANDBOX
  std::atomic<IndirectPointerHandle> handle_;
#else
  TaggedMember<T> member_;
#endif
};

}  // namespace internal
}  // namespace v8

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_TRUSTED_POINTER_H_
