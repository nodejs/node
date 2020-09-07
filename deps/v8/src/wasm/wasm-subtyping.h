// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_WASM_WASM_SUBTYPING_H_
#define V8_WASM_WASM_SUBTYPING_H_

#include "src/wasm/value-type.h"

namespace v8 {
namespace internal {
namespace wasm {

struct WasmModule;
V8_EXPORT_PRIVATE bool IsSubtypeOfHeap(HeapType subtype, HeapType supertype,
                                       const WasmModule* module);

// The subtyping between value types is described by the following rules:
// - All types are a supertype of bottom.
// - All reference types, except funcref, are subtypes of eqref.
// - optref(ht1) <: optref(ht2) iff ht1 <: ht2.
// - ref(ht1) <: ref/optref(ht2) iff ht1 <: ht2.
V8_INLINE bool IsSubtypeOf(ValueType subtype, ValueType supertype,
                           const WasmModule* module) {
  if (subtype == supertype) return true;
  bool compatible_references = (subtype.kind() == ValueType::kRef &&
                                supertype.kind() == ValueType::kRef) ||
                               (subtype.kind() == ValueType::kRef &&
                                supertype.kind() == ValueType::kOptRef) ||
                               (subtype.kind() == ValueType::kOptRef &&
                                supertype.kind() == ValueType::kOptRef);
  if (!compatible_references) return false;
  return IsSubtypeOfHeap(subtype.heap_type(), supertype.heap_type(), module);
}

ValueType CommonSubtype(ValueType a, ValueType b, const WasmModule* module);

}  // namespace wasm
}  // namespace internal
}  // namespace v8

#endif  // V8_WASM_WASM_SUBTYPING_H_
