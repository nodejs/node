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
V8_NOINLINE V8_EXPORT_PRIVATE bool IsSubtypeOfImpl(ValueType subtype,
                                                   ValueType supertype,
                                                   const WasmModule* module);

// The subtyping between value types is described by the following rules:
// - All types are a supertype of bottom.
// - All reference types, except funcref, are subtypes of eqref.
// - optref(ht1) <: optref(ht2) iff ht1 <: ht2.
// - ref(ht1) <: ref/optref(ht2) iff ht1 <: ht2.
V8_INLINE bool IsSubtypeOf(ValueType subtype, ValueType supertype,
                           const WasmModule* module) {
  if (V8_LIKELY(subtype == supertype)) return true;
  return IsSubtypeOfImpl(subtype, supertype, module);
}

ValueType CommonSubtype(ValueType a, ValueType b, const WasmModule* module);

}  // namespace wasm
}  // namespace internal
}  // namespace v8

#endif  // V8_WASM_WASM_SUBTYPING_H_
