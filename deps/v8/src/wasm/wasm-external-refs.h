// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_WASM_WASM_EXTERNAL_REFS_H_
#define V8_WASM_WASM_EXTERNAL_REFS_H_

#include <stdint.h>

#include "src/globals.h"

namespace v8 {
namespace internal {
namespace wasm {

void f32_trunc_wrapper(Address data);

void f32_floor_wrapper(Address data);

void f32_ceil_wrapper(Address data);

void f32_nearest_int_wrapper(Address data);

void f64_trunc_wrapper(Address data);

void f64_floor_wrapper(Address data);

void f64_ceil_wrapper(Address data);

void f64_nearest_int_wrapper(Address data);

void int64_to_float32_wrapper(Address data);

void uint64_to_float32_wrapper(Address data);

void int64_to_float64_wrapper(Address data);

void uint64_to_float64_wrapper(Address data);

int32_t float32_to_int64_wrapper(Address data);

int32_t float32_to_uint64_wrapper(Address data);

int32_t float64_to_int64_wrapper(Address data);

int32_t float64_to_uint64_wrapper(Address data);

int32_t int64_div_wrapper(Address data);

int32_t int64_mod_wrapper(Address data);

int32_t uint64_div_wrapper(Address data);

int32_t uint64_mod_wrapper(Address data);

uint32_t word32_ctz_wrapper(Address data);

uint32_t word64_ctz_wrapper(Address data);

uint32_t word32_popcnt_wrapper(Address data);

uint32_t word64_popcnt_wrapper(Address data);

uint32_t word32_rol_wrapper(Address data);

uint32_t word32_ror_wrapper(Address data);

void float64_pow_wrapper(Address data);

void memory_copy_wrapper(Address dst, Address src, uint32_t size);

void memory_fill_wrapper(Address dst, uint32_t value, uint32_t size);

typedef void (*WasmTrapCallbackForTesting)();

void set_trap_callback_for_testing(WasmTrapCallbackForTesting callback);

void call_trap_callback_for_testing();

}  // namespace wasm
}  // namespace internal
}  // namespace v8

#endif  // V8_WASM_WASM_EXTERNAL_REFS_H_
