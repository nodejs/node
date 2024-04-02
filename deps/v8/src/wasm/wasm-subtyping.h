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
// Type equivalence (~) is described by the following rules:
// - Two numeric types are equivalent iff they are equal.
// - T(ht1) ~ T(ht2) iff ht1 ~ ht2 for T in {ref, optref, rtt}.
// Equivalence of heap types ht1 ~ ht2 is defined as follows:
// - Two non-index heap types are equivalent iff they are equal.
// - Two indexed heap types are equivalent iff they are iso-recursive
//   equivalent.
V8_NOINLINE V8_EXPORT_PRIVATE bool EquivalentTypes(ValueType type1,
                                                   ValueType type2,
                                                   const WasmModule* module1,
                                                   const WasmModule* module2);

// Checks if {subtype}, defined in {module1}, is a subtype of {supertype},
// defined in {module2}.
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
//          |
//        array
// - All functions are subtypes of func.
// - All structs are subtypes of data.
// - All arrays are subtypes of array.
// - An indexed heap type h1 is a subtype of indexed heap type h2 if h2 is
//   transitively an explicit canonical supertype of h1.
V8_INLINE bool IsSubtypeOf(ValueType subtype, ValueType supertype,
                           const WasmModule* sub_module,
                           const WasmModule* super_module) {
  if (subtype == supertype && sub_module == super_module) return true;
  return IsSubtypeOfImpl(subtype, supertype, sub_module, super_module);
}

// Checks if {subtype} is a subtype of {supertype} (both defined in {module}).
V8_INLINE bool IsSubtypeOf(ValueType subtype, ValueType supertype,
                           const WasmModule* module) {
  // If the types are trivially identical, exit early.
  if (V8_LIKELY(subtype == supertype)) return true;
  return IsSubtypeOfImpl(subtype, supertype, module, module);
}

// We have this function call IsSubtypeOf instead of the opposite because type
// checks are much more common than heap type checks.
V8_INLINE bool IsHeapSubtypeOf(HeapType::Representation subtype,
                               HeapType::Representation supertype,
                               const WasmModule* module) {
  return IsSubtypeOf(ValueType::Ref(subtype, kNonNullable),
                     ValueType::Ref(supertype, kNonNullable), module);
}

// Checks whether {subtype_index} is valid as a declared subtype of
// {supertype_index}.
// - Both type must be of the same kind (function, struct, or array).
// - Structs: Subtype must have at least as many fields as supertype,
//   covariance for respective immutable fields, equivalence for respective
//   mutable fields.
// - Arrays: subtyping of respective element types for immutable arrays,
//   equivalence of element types for mutable arrays.
// - Functions: equal number of parameter and return types. Contravariance for
//   respective parameter types, covariance for respective return types.
V8_EXPORT_PRIVATE bool ValidSubtypeDefinition(uint32_t subtype_index,
                                              uint32_t supertype_index,
                                              const WasmModule* sub_module,
                                              const WasmModule* super_module);
}  // namespace wasm
}  // namespace internal
}  // namespace v8

#endif  // V8_WASM_WASM_SUBTYPING_H_
