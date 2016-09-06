// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#ifndef WASM_EXTERNAL_REFS_H
#define WASM_EXTERNAL_REFS_H

namespace v8 {
namespace internal {
namespace wasm {

void f32_trunc_wrapper(float* param);

void f32_floor_wrapper(float* param);

void f32_ceil_wrapper(float* param);

void f32_nearest_int_wrapper(float* param);

void f64_trunc_wrapper(double* param);

void f64_floor_wrapper(double* param);

void f64_ceil_wrapper(double* param);

void f64_nearest_int_wrapper(double* param);

void int64_to_float32_wrapper(int64_t* input, float* output);

void uint64_to_float32_wrapper(uint64_t* input, float* output);

void int64_to_float64_wrapper(int64_t* input, double* output);

void uint64_to_float64_wrapper(uint64_t* input, double* output);

int32_t float32_to_int64_wrapper(float* input, int64_t* output);

int32_t float32_to_uint64_wrapper(float* input, uint64_t* output);

int32_t float64_to_int64_wrapper(double* input, int64_t* output);

int32_t float64_to_uint64_wrapper(double* input, uint64_t* output);

int32_t int64_div_wrapper(int64_t* dst, int64_t* src);

int32_t int64_mod_wrapper(int64_t* dst, int64_t* src);

int32_t uint64_div_wrapper(uint64_t* dst, uint64_t* src);

int32_t uint64_mod_wrapper(uint64_t* dst, uint64_t* src);

uint32_t word32_ctz_wrapper(uint32_t* input);

uint32_t word64_ctz_wrapper(uint64_t* input);

uint32_t word32_popcnt_wrapper(uint32_t* input);

uint32_t word64_popcnt_wrapper(uint64_t* input);

void float64_pow_wrapper(double* param0, double* param1);

}  // namespace wasm
}  // namespace internal
}  // namespace v8
#endif
