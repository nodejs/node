// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if !V8_ENABLE_WEBASSEMBLY
#error This header should only be included if WebAssembly is enabled.
#endif  // !V8_ENABLE_WEBASSEMBLY

#ifndef V8_WASM_WASM_EXTERNAL_REFS_H_
#define V8_WASM_WASM_EXTERNAL_REFS_H_

#include <stdint.h>

#include "src/base/macros.h"

namespace v8 {
namespace internal {

class Isolate;

namespace wasm {

using Address = uintptr_t;

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

V8_EXPORT_PRIVATE void float32_to_int64_sat_wrapper(Address data);

V8_EXPORT_PRIVATE void float32_to_uint64_sat_wrapper(Address data);

V8_EXPORT_PRIVATE void float64_to_int64_sat_wrapper(Address data);

V8_EXPORT_PRIVATE void float64_to_uint64_sat_wrapper(Address data);

V8_EXPORT_PRIVATE void float32_to_float16_wrapper(Address data);

V8_EXPORT_PRIVATE void float16_to_float32_wrapper(Address data);

V8_EXPORT_PRIVATE int32_t int64_div_wrapper(Address data);

V8_EXPORT_PRIVATE int32_t int64_mod_wrapper(Address data);

V8_EXPORT_PRIVATE int32_t uint64_div_wrapper(Address data);

V8_EXPORT_PRIVATE int32_t uint64_mod_wrapper(Address data);

V8_EXPORT_PRIVATE uint32_t word32_rol_wrapper(uint32_t input, uint32_t shift);

V8_EXPORT_PRIVATE uint32_t word32_ror_wrapper(uint32_t input, uint32_t shift);

V8_EXPORT_PRIVATE uint64_t word64_rol_wrapper(uint64_t input, uint32_t shift);

V8_EXPORT_PRIVATE uint64_t word64_ror_wrapper(uint64_t input, uint32_t shift);

V8_EXPORT_PRIVATE void float64_pow_wrapper(Address data);

V8_EXPORT_PRIVATE void f64x2_ceil_wrapper(Address data);

V8_EXPORT_PRIVATE void f64x2_floor_wrapper(Address data);

V8_EXPORT_PRIVATE void f64x2_trunc_wrapper(Address data);

V8_EXPORT_PRIVATE void f64x2_nearest_int_wrapper(Address data);

V8_EXPORT_PRIVATE void f32x4_ceil_wrapper(Address data);

V8_EXPORT_PRIVATE void f32x4_floor_wrapper(Address data);

V8_EXPORT_PRIVATE void f32x4_trunc_wrapper(Address data);

V8_EXPORT_PRIVATE void f32x4_nearest_int_wrapper(Address data);

V8_EXPORT_PRIVATE void f16x8_abs_wrapper(Address data);

V8_EXPORT_PRIVATE void f16x8_neg_wrapper(Address data);

V8_EXPORT_PRIVATE void f16x8_sqrt_wrapper(Address data);

V8_EXPORT_PRIVATE void f16x8_ceil_wrapper(Address data);

V8_EXPORT_PRIVATE void f16x8_floor_wrapper(Address data);

V8_EXPORT_PRIVATE void f16x8_trunc_wrapper(Address data);

V8_EXPORT_PRIVATE void f16x8_nearest_int_wrapper(Address data);

V8_EXPORT_PRIVATE void f16x8_eq_wrapper(Address data);

V8_EXPORT_PRIVATE void f16x8_ne_wrapper(Address data);

V8_EXPORT_PRIVATE void f16x8_lt_wrapper(Address data);

V8_EXPORT_PRIVATE void f16x8_le_wrapper(Address data);

V8_EXPORT_PRIVATE void f16x8_add_wrapper(Address data);

V8_EXPORT_PRIVATE void f16x8_sub_wrapper(Address data);

V8_EXPORT_PRIVATE void f16x8_mul_wrapper(Address data);

V8_EXPORT_PRIVATE void f16x8_div_wrapper(Address data);

V8_EXPORT_PRIVATE void f16x8_min_wrapper(Address data);

V8_EXPORT_PRIVATE void f16x8_max_wrapper(Address data);

V8_EXPORT_PRIVATE void f16x8_pmin_wrapper(Address data);

V8_EXPORT_PRIVATE void f16x8_pmax_wrapper(Address data);

V8_EXPORT_PRIVATE void i16x8_sconvert_f16x8_wrapper(Address data);

V8_EXPORT_PRIVATE void i16x8_uconvert_f16x8_wrapper(Address data);

V8_EXPORT_PRIVATE void f16x8_sconvert_i16x8_wrapper(Address data);

V8_EXPORT_PRIVATE void f16x8_uconvert_i16x8_wrapper(Address data);

V8_EXPORT_PRIVATE void f32x4_promote_low_f16x8_wrapper(Address data);

V8_EXPORT_PRIVATE void f16x8_demote_f32x4_zero_wrapper(Address data);

V8_EXPORT_PRIVATE void f16x8_demote_f64x2_zero_wrapper(Address data);

V8_EXPORT_PRIVATE void f16x8_qfma_wrapper(Address data);

V8_EXPORT_PRIVATE void f16x8_qfms_wrapper(Address data);

// The return type is {int32_t} instead of {bool} to enforce the compiler to
// zero-extend the result in the return register.
int32_t memory_init_wrapper(Address instance_addr, uint32_t mem_index,
                            uintptr_t dst, uint32_t src, uint32_t seg_index,
                            uint32_t size);

// The return type is {int32_t} instead of {bool} to enforce the compiler to
// zero-extend the result in the return register.
int32_t memory_copy_wrapper(Address instance_addr, uint32_t dst_mem_index,
                            uint32_t src_mem_index, uintptr_t dst,
                            uintptr_t src, uintptr_t size);

// The return type is {int32_t} instead of {bool} to enforce the compiler to
// zero-extend the result in the return register.
int32_t memory_fill_wrapper(Address instance_addr, uint32_t mem_index,
                            uintptr_t dst, uint8_t value, uintptr_t size);

// Assumes copy ranges are in-bounds and length > 0.
void array_copy_wrapper(Address raw_dst_array, uint32_t dst_index,
                        Address raw_src_array, uint32_t src_index,
                        uint32_t length);

// The initial value is passed as an int64_t on the stack. Cannot handle s128
// other than 0.
void array_fill_wrapper(Address raw_array, uint32_t index, uint32_t length,
                        uint32_t emit_write_barrier, uint32_t raw_type,
                        Address initial_value_addr);

double flat_string_to_f64(Address string_address);

// Update the stack limit after a stack switch,
// and preserve pending interrupts.
void sync_stack_limit(Isolate* isolate);
// Return {continuation}'s stack memory to the stack pool after it has returned
// and switched back to its parent, and update the stack limit.
void return_switch(Isolate* isolate, Address continuation);

intptr_t switch_to_the_central_stack(Isolate* isolate, uintptr_t sp);
void switch_from_the_central_stack(Isolate* isolate);
intptr_t switch_to_the_central_stack_for_js(Isolate* isolate, Address fp);
void switch_from_the_central_stack_for_js(Isolate* isolate);
Address grow_stack(Isolate* isolate, void* current_sp, size_t frame_size,
                   size_t gap, Address current_fp);
Address shrink_stack(Isolate* isolate);
Address load_old_fp(Isolate* isolate);

}  // namespace wasm
}  // namespace internal
}  // namespace v8

#endif  // V8_WASM_WASM_EXTERNAL_REFS_H_
