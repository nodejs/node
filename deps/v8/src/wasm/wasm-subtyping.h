// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if !V8_ENABLE_WEBASSEMBLY
#error This header should only be included if WebAssembly is enabled.
#endif  // !V8_ENABLE_WEBASSEMBLY

#ifndef V8_WASM_WASM_SUBTYPING_H_
#define V8_WASM_WASM_SUBTYPING_H_

#include "src/wasm/value-type.h"

namespace v8 {
namespace internal {
namespace wasm {

struct WasmModule;

V8_NOINLINE V8_EXPORT_PRIVATE bool IsSubtypeOfImpl(
    ValueType subtype, ValueType supertype, const WasmModule* sub_module,
    const WasmModule* super_module);

// Checks if type1, defined in module1, is equivalent with type2, defined in
// module2.
// Type equivalence (~) is described by the following rules (structural
// equivalence):
// - Two numeric types are equivalent iff they are equal.
// - optref(ht1) ~ optref(ht2) iff ht1 ~ ht2.
// - ref(ht1) ~ ref(ht2) iff ht1 ~ ht2.
// - rtt(d1, ht1) ~ rtt(d2, ht2) iff (d1 = d2 and ht1 ~ ht2).
// For heap types, the following rules hold:
// - Two generic heap types are equivalent iff they are equal.
// - Two structs are equivalent iff they contain the same number of fields and
//   these are pairwise equivalent.
// - Two functions are equivalent iff they contain the same number of parameters
//   and returns and these are pairwise equivalent.
// - Two arrays are equivalent iff their underlying types are equivalent.
V8_NOINLINE bool EquivalentTypes(ValueType type1, ValueType type2,
                                 const WasmModule* module1,
                                 const WasmModule* module2);

// Checks if subtype, defined in module1, is a subtype of supertype, defined in
// module2.
// Subtyping between value types is described by the following rules
// (structural subtyping):
// - numeric types are subtype-related iff they are equal.
// - optref(ht1) <: optref(ht2) iff ht1 <: ht2.
// - ref(ht1) <: ref/optref(ht2) iff ht1 <: ht2.
// - rtt1 <: rtt2 iff rtt1 ~ rtt2.
// For heap types, the following subtyping rules hold:
// - The abstract heap types form the following type hierarchy:
//           any
//         /  |  \
//       eq func  extern
//      / \
//   i31   data
// - All structs and arrays are subtypes of data.
// - All functions are subtypes of func.
// - Struct subtyping: Subtype must have at least as many fields as supertype,
//   covariance for immutable fields, equivalence for mutable fields.
// - Array subtyping (mutable only) is the equivalence relation.
// - Function subtyping depends on the enabled wasm features: if
//   --experimental-wasm-gc is enabled, then subtyping is computed
//   contravariantly for parameter types and covariantly for return types.
//   Otherwise, the subtyping relation is the equivalence relation.
V8_INLINE bool IsSubtypeOf(ValueType subtype, ValueType supertype,
                           const WasmModule* sub_module,
                           const WasmModule* super_module) {
  if (subtype == supertype && sub_module == super_module) return true;
  return IsSubtypeOfImpl(subtype, supertype, sub_module, super_module);
}

// Checks if 'subtype' is a subtype of 'supertype' (both defined in module).
V8_INLINE bool IsSubtypeOf(ValueType subtype, ValueType supertype,
                           const WasmModule* module) {
  // If the types are trivially identical, exit early.
  if (V8_LIKELY(subtype == supertype)) return true;
  return IsSubtypeOfImpl(subtype, supertype, module, module);
}

// We have this function call IsSubtypeOf instead of the opposite because type
// checks are much more common than heap type checks.}
V8_INLINE bool IsHeapSubtypeOf(uint32_t subtype_index,
                               HeapType::Representation supertype,
                               const WasmModule* module) {
  return IsSubtypeOf(ValueType::Ref(subtype_index, kNonNullable),
                     ValueType::Ref(supertype, kNonNullable), module);
}
V8_INLINE bool IsHeapSubtypeOf(uint32_t subtype_index, uint32_t supertype_index,
                               const WasmModule* module) {
  return IsSubtypeOf(ValueType::Ref(subtype_index, kNonNullable),
                     ValueType::Ref(supertype_index, kNonNullable), module);
}

// Call this function in {module}'s destructor to avoid spurious cache hits in
// case another WasmModule gets allocated in the same address later.
void DeleteCachedTypeJudgementsForModule(const WasmModule* module);

}  // namespace wasm
}  // namespace internal
}  // namespace v8

#endif  // V8_WASM_WASM_SUBTYPING_H_
