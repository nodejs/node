// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_WASM_WASM_EXTERNAL_REFS_H_
#define V8_WASM_WASM_EXTERNAL_REFS_H_

#include <stdint.h>

#include "src/common/globals.h"

namespace v8 {
namespace internal {
namespace wasm {

V8_EXPORT_PRIVATE void f32_trunc_wrapper(Address data);

V8_EXPORT_PRIVATE void f32_floor_wrapper(Address data);

V8_EXPORT_PRIVATE void f32_ceil_wrapper(Address data);

V8_EXPORT_PRIVATE void f32_nearest_int_wrapper(Address data);

V8_EXPORT_PRIVATE void f64_trunc_wrapper(Address data);

V8_EXPORT_PRIVATE void f64_floor_wrapper(Address data);

V8_EXPORT_PRIVATE void f64_ceil_wrapper(Address data);

V8_EXPORT_PRIVATE void f64_nearest_int_wrapper(Address data);

V8_EXPORT_PRIVATE void int64_to_float32_wrapper(Address data);

V8_EXPORT_PRIVATE void uint64_to_float32_wrapper(Address data);

V8_EXPORT_PRIVATE void int64_to_float64_wrapper(Address data);

V8_EXPORT_PRIVATE void uint64_to_float64_wrapper(Address data);

V8_EXPORT_PRIVATE int32_t float32_to_int64_wrapper(Address data);

V8_EXPORT_PRIVATE int32_t float32_to_uint64_wrapper(Address data);

V8_EXPORT_PRIVATE int32_t float64_to_int64_wrapper(Address data);

V8_EXPORT_PRIVATE int32_t float64_to_uint64_wrapper(Address data);

V8_EXPORT_PRIVATE int32_t int64_div_wrapper(Address data);

V8_EXPORT_PRIVATE int32_t int64_mod_wrapper(Address data);

V8_EXPORT_PRIVATE int32_t uint64_div_wrapper(Address data);

V8_EXPORT_PRIVATE int32_t uint64_mod_wrapper(Address data);

V8_EXPORT_PRIVATE uint32_t word32_ctz_wrapper(Address data);

V8_EXPORT_PRIVATE uint32_t word64_ctz_wrapper(Address data);

V8_EXPORT_PRIVATE uint32_t word32_popcnt_wrapper(Address data);

V8_EXPORT_PRIVATE uint32_t word64_popcnt_wrapper(Address data);

V8_EXPORT_PRIVATE uint32_t word32_rol_wrapper(Address data);

V8_EXPORT_PRIVATE uint32_t word32_ror_wrapper(Address data);

V8_EXPORT_PRIVATE void word64_rol_wrapper(Address data);

V8_EXPORT_PRIVATE void word64_ror_wrapper(Address data);

V8_EXPORT_PRIVATE void float64_pow_wrapper(Address data);

// The return type is {int32_t} instead of {bool} to enforce the compiler to
// zero-extend the result in the return register.
int32_t memory_init_wrapper(Address data);

// The return type is {int32_t} instead of {bool} to enforce the compiler to
// zero-extend the result in the return register.
int32_t memory_copy_wrapper(Address data);

// The return type is {int32_t} instead of {bool} to enforce the compiler to
// zero-extend the result in the return register.
int32_t memory_fill_wrapper(Address data);

using WasmTrapCallbackForTesting = void (*)();

V8_EXPORT_PRIVATE void set_trap_callback_for_testing(
    WasmTrapCallbackForTesting callback);

V8_EXPORT_PRIVATE void call_trap_callback_for_testing();

}  // namespace wasm
}  // namespace internal
}  // namespace v8

#endif  // V8_WASM_WASM_EXTERNAL_REFS_H_
