// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <math.h>
#include <stdint.h>
#include <stdlib.h>

#include <limits>

#include "src/base/bits.h"
#include "src/base/ieee754.h"
#include "src/base/safe_conversions.h"
#include "src/common/assert-scope.h"
#include "src/execution/pointer-authentication.h"
#include "src/numbers/conversions.h"
#include "src/roots/roots-inl.h"
#include "src/utils/memcopy.h"
#include "src/wasm/float16.h"
#include "src/wasm/wasm-engine.h"
#include "src/wasm/wasm-objects-inl.h"

#if defined(ADDRESS_SANITIZER) || defined(MEMORY_SANITIZER) || \
    defined(THREAD_SANITIZER) || defined(LEAK_SANITIZER) ||    \
    defined(UNDEFINED_SANITIZER)
#define V8_WITH_SANITIZER
#endif

#if defined(V8_OS_WIN) && defined(V8_WITH_SANITIZER)
// With ASAN on Windows we have to reset the thread-in-wasm flag. Exceptions
// caused by ASAN let the thread-in-wasm flag get out of sync. Even marking
// functions with DISABLE_ASAN is not sufficient when the compiler produces
// calls to memset. Therefore we add test-specific code for ASAN on
// Windows.
#define RESET_THREAD_IN_WASM_FLAG_FOR_ASAN_ON_WINDOWS
#include "src/trap-handler/trap-handler.h"
#endif

#include "src/base/memory.h"
#include "src/base/overflowing-math.h"
#include "src/utils/utils.h"
#include "src/wasm/wasm-external-refs.h"

namespace v8::internal::wasm {

using base::ReadUnalignedValue;
using base::WriteUnalignedValue;

void f32_trunc_wrapper(Address data) {
  WriteUnalignedValue<float>(data, truncf(ReadUnalignedValue<float>(data)));
}

void f32_floor_wrapper(Address data) {
  WriteUnalignedValue<float>(data, floorf(ReadUnalignedValue<float>(data)));
}

void f32_ceil_wrapper(Address data) {
  WriteUnalignedValue<float>(data, ceilf(ReadUnalignedValue<float>(data)));
}

void f32_nearest_int_wrapper(Address data) {
  float input = ReadUnalignedValue<float>(data);
  float value = nearbyintf(input);
#if V8_OS_AIX
  value = FpOpWorkaround<float>(input, value);
#endif
  WriteUnalignedValue<float>(data, value);
}

void f64_trunc_wrapper(Address data) {
  WriteUnalignedValue<double>(data, trunc(ReadUnalignedValue<double>(data)));
}

void f64_floor_wrapper(Address data) {
  WriteUnalignedValue<double>(data, floor(ReadUnalignedValue<double>(data)));
}

void f64_ceil_wrapper(Address data) {
  WriteUnalignedValue<double>(data, ceil(ReadUnalignedValue<double>(data)));
}

void f64_nearest_int_wrapper(Address data) {
  double input = ReadUnalignedValue<double>(data);
  double value = nearbyint(input);
#if V8_OS_AIX
  value = FpOpWorkaround<double>(input, value);
#endif
  WriteUnalignedValue<double>(data, value);
}

void int64_to_float32_wrapper(Address data) {
  int64_t input = ReadUnalignedValue<int64_t>(data);
  WriteUnalignedValue<float>(data, static_cast<float>(input));
}

void uint64_to_float32_wrapper(Address data) {
  uint64_t input = ReadUnalignedValue<uint64_t>(data);
#if defined(V8_OS_WIN)
  // On Windows, the FP stack registers calculate with less precision, which
  // leads to a uint64_t to float32 conversion which does not satisfy the
  // WebAssembly specification. Therefore we do a different approach here:
  //
  // / leading 0 \/  24 float data bits  \/  for rounding \/ trailing 0 \
  // 00000000000001XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX100000000000000
  //
  // Float32 can only represent 24 data bit (1 implicit 1 bit + 23 mantissa
  // bits). Starting from the most significant 1 bit, we can therefore extract
  // 24 bits and do the conversion only on them. The other bits can affect the
  // result only through rounding. Rounding works as follows:
  // * If the most significant rounding bit is not set, then round down.
  // * If the most significant rounding bit is set, and at least one of the
  //   other rounding bits is set, then round up.
  // * If the most significant rounding bit is set, but all other rounding bits
  //   are not set, then round to even.
  // We can aggregate 'all other rounding bits' in the second-most significant
  // rounding bit.
  // The resulting algorithm is therefore as follows:
  // * Check if the distance between the most significant bit (MSB) and the
  //   least significant bit (LSB) is greater than 25 bits. If the distance is
  //   less or equal to 25 bits, the uint64 to float32 conversion is anyways
  //   exact, and we just use the C++ conversion.
  // * Find the most significant bit (MSB).
  // * Starting from the MSB, extract 25 bits (24 data bits + the first rounding
  //   bit).
  // * The remaining rounding bits are guaranteed to contain at least one 1 bit,
  //   due to the check we did above.
  // * Store the 25 bits + 1 aggregated bit in an uint32_t.
  // * Convert this uint32_t to float. The conversion does the correct rounding
  //   now.
  // * Shift the result back to the original magnitude.
  uint32_t leading_zeros = base::bits::CountLeadingZeros(input);
  uint32_t trailing_zeros = base::bits::CountTrailingZeros(input);
  constexpr uint32_t num_extracted_bits = 25;
  // Check if there are any rounding bits we have to aggregate.
  if (leading_zeros + trailing_zeros + num_extracted_bits < 64) {
    // Shift to extract the data bits.
    uint32_t num_aggregation_bits = 64 - num_extracted_bits - leading_zeros;
    // We extract the bits we want to convert. Note that we convert one bit more
    // than necessary. This bit is a placeholder where we will store the
    // aggregation bit.
    int32_t extracted_bits =
        static_cast<int32_t>(input >> (num_aggregation_bits - 1));
    // Set the aggregation bit. We don't have to clear the slot first, because
    // the bit there is also part of the aggregation.
    extracted_bits |= 1;
    float result = static_cast<float>(extracted_bits);
    // We have to shift the result back. The shift amount is
    // (num_aggregation_bits - 1), which is the shift amount we did originally,
    // and (-2), which is for the two additional bits we kept originally for
    // rounding.
    int32_t shift_back = static_cast<int32_t>(num_aggregation_bits) - 1 - 2;
    // Calculate the multiplier to shift the extracted bits back to the original
    // magnitude. This multiplier is a power of two, so in the float32 bit
    // representation we just have to construct the correct exponent and put it
    // at the correct bit offset. The exponent consists of 8 bits, starting at
    // the second MSB (a.k.a '<< 23'). The encoded exponent itself is
    // ('actual exponent' - 127).
    int32_t multiplier_bits = ((shift_back - 127) & 0xff) << 23;
    result *= base::bit_cast<float>(multiplier_bits);
    WriteUnalignedValue<float>(data, result);
    return;
  }
#endif  // defined(V8_OS_WIN)
  WriteUnalignedValue<float>(data, static_cast<float>(input));
}

void int64_to_float64_wrapper(Address data) {
  int64_t input = ReadUnalignedValue<int64_t>(data);
  WriteUnalignedValue<double>(data, static_cast<double>(input));
}

void uint64_to_float64_wrapper(Address data) {
  uint64_t input = ReadUnalignedValue<uint64_t>(data);
  double result = static_cast<double>(input);

#if V8_CC_MSVC
  // With MSVC we use static_cast<double>(uint32_t) instead of
  // static_cast<double>(uint64_t) to achieve round-to-nearest-ties-even
  // semantics. The idea is to calculate
  // static_cast<double>(high_word) * 2^32 + static_cast<double>(low_word).
  uint32_t low_word = static_cast<uint32_t>(input & 0xFFFFFFFF);
  uint32_t high_word = static_cast<uint32_t>(input >> 32);

  double shift = static_cast<double>(1ull << 32);

  result = static_cast<double>(high_word);
  result *= shift;
  result += static_cast<double>(low_word);
#endif

  WriteUnalignedValue<double>(data, result);
}

int32_t float32_to_int64_wrapper(Address data) {
  float input = ReadUnalignedValue<float>(data);
  if (base::IsValueInRangeForNumericType<int64_t>(input)) {
    WriteUnalignedValue<int64_t>(data, static_cast<int64_t>(input));
    return 1;
  }
  return 0;
}

int32_t float32_to_uint64_wrapper(Address data) {
  float input = ReadUnalignedValue<float>(data);
  if (base::IsValueInRangeForNumericType<uint64_t>(input)) {
    WriteUnalignedValue<uint64_t>(data, static_cast<uint64_t>(input));
    return 1;
  }
  return 0;
}

int32_t float64_to_int64_wrapper(Address data) {
  double input = ReadUnalignedValue<double>(data);
  if (base::IsValueInRangeForNumericType<int64_t>(input)) {
    WriteUnalignedValue<int64_t>(data, static_cast<int64_t>(input));
    return 1;
  }
  return 0;
}

int32_t float64_to_uint64_wrapper(Address data) {
  double input = ReadUnalignedValue<double>(data);
  if (base::IsValueInRangeForNumericType<uint64_t>(input)) {
    WriteUnalignedValue<uint64_t>(data, static_cast<uint64_t>(input));
    return 1;
  }
  return 0;
}

void float32_to_int64_sat_wrapper(Address data) {
  float input = ReadUnalignedValue<float>(data);
  if (base::IsValueInRangeForNumericType<int64_t>(input)) {
    WriteUnalignedValue<int64_t>(data, static_cast<int64_t>(input));
    return;
  }
  if (std::isnan(input)) {
    WriteUnalignedValue<int64_t>(data, 0);
    return;
  }
  if (input < 0.0) {
    WriteUnalignedValue<int64_t>(data, std::numeric_limits<int64_t>::min());
    return;
  }
  WriteUnalignedValue<int64_t>(data, std::numeric_limits<int64_t>::max());
}

void float32_to_uint64_sat_wrapper(Address data) {
  float input = ReadUnalignedValue<float>(data);
  if (base::IsValueInRangeForNumericType<uint64_t>(input)) {
    WriteUnalignedValue<uint64_t>(data, static_cast<uint64_t>(input));
    return;
  }
  if (input >= static_cast<float>(std::numeric_limits<uint64_t>::max())) {
    WriteUnalignedValue<uint64_t>(data, std::numeric_limits<uint64_t>::max());
    return;
  }
  WriteUnalignedValue<uint64_t>(data, 0);
}

void float64_to_int64_sat_wrapper(Address data) {
  double input = ReadUnalignedValue<double>(data);
  if (base::IsValueInRangeForNumericType<int64_t>(input)) {
    WriteUnalignedValue<int64_t>(data, static_cast<int64_t>(input));
    return;
  }
  if (std::isnan(input)) {
    WriteUnalignedValue<int64_t>(data, 0);
    return;
  }
  if (input < 0.0) {
    WriteUnalignedValue<int64_t>(data, std::numeric_limits<int64_t>::min());
    return;
  }
  WriteUnalignedValue<int64_t>(data, std::numeric_limits<int64_t>::max());
}

void float64_to_uint64_sat_wrapper(Address data) {
  double input = ReadUnalignedValue<double>(data);
  if (base::IsValueInRangeForNumericType<uint64_t>(input)) {
    WriteUnalignedValue<uint64_t>(data, static_cast<uint64_t>(input));
    return;
  }
  if (input >= static_cast<double>(std::numeric_limits<uint64_t>::max())) {
    WriteUnalignedValue<uint64_t>(data, std::numeric_limits<uint64_t>::max());
    return;
  }
  WriteUnalignedValue<uint64_t>(data, 0);
}

void float16_to_float32_wrapper(Address data) {
  WriteUnalignedValue<float>(data, Float16::Read(data).ToFloat32());
}

void float32_to_float16_wrapper(Address data) {
  Float16::FromFloat32(ReadUnalignedValue<float>(data)).Write(data);
}

int32_t int64_div_wrapper(Address data) {
  int64_t dividend = ReadUnalignedValue<int64_t>(data);
  int64_t divisor = ReadUnalignedValue<int64_t>(data + sizeof(dividend));
  if (divisor == 0) {
    return 0;
  }
  if (divisor == -1 && dividend == std::numeric_limits<int64_t>::min()) {
    return -1;
  }
  WriteUnalignedValue<int64_t>(data, dividend / divisor);
  return 1;
}

int32_t int64_mod_wrapper(Address data) {
  int64_t dividend = ReadUnalignedValue<int64_t>(data);
  int64_t divisor = ReadUnalignedValue<int64_t>(data + sizeof(dividend));
  if (divisor == 0) {
    return 0;
  }
  if (divisor == -1 && dividend == std::numeric_limits<int64_t>::min()) {
    WriteUnalignedValue<int64_t>(data, 0);
    return 1;
  }
  WriteUnalignedValue<int64_t>(data, dividend % divisor);
  return 1;
}

int32_t uint64_div_wrapper(Address data) {
  uint64_t dividend = ReadUnalignedValue<uint64_t>(data);
  uint64_t divisor = ReadUnalignedValue<uint64_t>(data + sizeof(dividend));
  if (divisor == 0) {
    return 0;
  }
  WriteUnalignedValue<uint64_t>(data, dividend / divisor);
  return 1;
}

int32_t uint64_mod_wrapper(Address data) {
  uint64_t dividend = ReadUnalignedValue<uint64_t>(data);
  uint64_t divisor = ReadUnalignedValue<uint64_t>(data + sizeof(dividend));
  if (divisor == 0) {
    return 0;
  }
  WriteUnalignedValue<uint64_t>(data, dividend % divisor);
  return 1;
}

uint32_t word32_rol_wrapper(uint32_t input, uint32_t shift) {
  return (input << (shift & 31)) | (input >> ((32 - shift) & 31));
}

uint32_t word32_ror_wrapper(uint32_t input, uint32_t shift) {
  return (input >> (shift & 31)) | (input << ((32 - shift) & 31));
}

uint64_t word64_rol_wrapper(uint64_t input, uint32_t shift) {
  return (input << (shift & 63)) | (input >> ((64 - shift) & 63));
}

uint64_t word64_ror_wrapper(uint64_t input, uint32_t shift) {
  return (input >> (shift & 63)) | (input << ((64 - shift) & 63));
}

void float64_pow_wrapper(Address data) {
  double x = ReadUnalignedValue<double>(data);
  double y = ReadUnalignedValue<double>(data + sizeof(x));
  WriteUnalignedValue<double>(data, base::ieee754::pow(x, y));
}

template <typename T, T (*float_round_op)(T)>
void simd_float_round_wrapper(Address data) {
  constexpr int n = kSimd128Size / sizeof(T);
  for (int i = 0; i < n; i++) {
    T input = ReadUnalignedValue<T>(data + (i * sizeof(T)));
    T value = float_round_op(input);
#if V8_OS_AIX
    value = FpOpWorkaround<T>(input, value);
#endif
    WriteUnalignedValue<T>(data + (i * sizeof(T)), value);
  }
}

void f64x2_ceil_wrapper(Address data) {
  simd_float_round_wrapper<double, &ceil>(data);
}

void f64x2_floor_wrapper(Address data) {
  simd_float_round_wrapper<double, &floor>(data);
}

void f64x2_trunc_wrapper(Address data) {
  simd_float_round_wrapper<double, &trunc>(data);
}

void f64x2_nearest_int_wrapper(Address data) {
  simd_float_round_wrapper<double, &nearbyint>(data);
}

void f32x4_ceil_wrapper(Address data) {
  simd_float_round_wrapper<float, &ceilf>(data);
}

void f32x4_floor_wrapper(Address data) {
  simd_float_round_wrapper<float, &floorf>(data);
}

void f32x4_trunc_wrapper(Address data) {
  simd_float_round_wrapper<float, &truncf>(data);
}

void f32x4_nearest_int_wrapper(Address data) {
  simd_float_round_wrapper<float, &nearbyintf>(data);
}

Float16 f16_abs(Float16 a) {
  return Float16::FromFloat32(std::abs(a.ToFloat32()));
}

void f16x8_abs_wrapper(Address data) {
  simd_float_round_wrapper<Float16, &f16_abs>(data);
}

Float16 f16_neg(Float16 a) { return Float16::FromFloat32(-(a.ToFloat32())); }

void f16x8_neg_wrapper(Address data) {
  simd_float_round_wrapper<Float16, &f16_neg>(data);
}

Float16 f16_sqrt(Float16 a) {
  return Float16::FromFloat32(std::sqrt(a.ToFloat32()));
}

void f16x8_sqrt_wrapper(Address data) {
  simd_float_round_wrapper<Float16, &f16_sqrt>(data);
}

Float16 f16_ceil(Float16 a) {
  return Float16::FromFloat32(ceilf(a.ToFloat32()));
}

void f16x8_ceil_wrapper(Address data) {
  simd_float_round_wrapper<Float16, &f16_ceil>(data);
}

Float16 f16_floor(Float16 a) {
  return Float16::FromFloat32(floorf(a.ToFloat32()));
}

void f16x8_floor_wrapper(Address data) {
  simd_float_round_wrapper<Float16, &f16_floor>(data);
}

Float16 f16_trunc(Float16 a) {
  return Float16::FromFloat32(truncf(a.ToFloat32()));
}

void f16x8_trunc_wrapper(Address data) {
  simd_float_round_wrapper<Float16, &f16_trunc>(data);
}

Float16 f16_nearest_int(Float16 a) {
  return Float16::FromFloat32(nearbyintf(a.ToFloat32()));
}

void f16x8_nearest_int_wrapper(Address data) {
  simd_float_round_wrapper<Float16, &f16_nearest_int>(data);
}

template <typename R, R (*float_bin_op)(Float16, Float16)>
void simd_float16_bin_wrapper(Address data) {
  constexpr int n = kSimd128Size / sizeof(Float16);
  for (int i = 0; i < n; i++) {
    Float16 lhs = Float16::Read(data + (i * sizeof(Float16)));
    Float16 rhs = Float16::Read(data + kSimd128Size + (i * sizeof(Float16)));
    R value = float_bin_op(lhs, rhs);
    WriteUnalignedValue<R>(data + (i * sizeof(R)), value);
  }
}

int16_t f16_eq(Float16 a, Float16 b) {
  return a.ToFloat32() == b.ToFloat32() ? -1 : 0;
}

void f16x8_eq_wrapper(Address data) {
  simd_float16_bin_wrapper<int16_t, &f16_eq>(data);
}

int16_t f16_ne(Float16 a, Float16 b) {
  return a.ToFloat32() != b.ToFloat32() ? -1 : 0;
}

void f16x8_ne_wrapper(Address data) {
  simd_float16_bin_wrapper<int16_t, &f16_ne>(data);
}

int16_t f16_lt(Float16 a, Float16 b) {
  return a.ToFloat32() < b.ToFloat32() ? -1 : 0;
}

void f16x8_lt_wrapper(Address data) {
  simd_float16_bin_wrapper<int16_t, &f16_lt>(data);
}

int16_t f16_le(Float16 a, Float16 b) {
  return a.ToFloat32() <= b.ToFloat32() ? -1 : 0;
}

void f16x8_le_wrapper(Address data) {
  simd_float16_bin_wrapper<int16_t, &f16_le>(data);
}

Float16 f16_add(Float16 a, Float16 b) {
  return Float16::FromFloat32(a.ToFloat32() + b.ToFloat32());
}

void f16x8_add_wrapper(Address data) {
  simd_float16_bin_wrapper<Float16, &f16_add>(data);
}

Float16 f16_sub(Float16 a, Float16 b) {
  return Float16::FromFloat32(a.ToFloat32() - b.ToFloat32());
}

void f16x8_sub_wrapper(Address data) {
  simd_float16_bin_wrapper<Float16, &f16_sub>(data);
}

Float16 f16_mul(Float16 a, Float16 b) {
  return Float16::FromFloat32(a.ToFloat32() * b.ToFloat32());
}

void f16x8_mul_wrapper(Address data) {
  simd_float16_bin_wrapper<Float16, &f16_mul>(data);
}

Float16 f16_div(Float16 a, Float16 b) {
  return Float16::FromFloat32(base::Divide(a.ToFloat32(), b.ToFloat32()));
}

void f16x8_div_wrapper(Address data) {
  simd_float16_bin_wrapper<Float16, &f16_div>(data);
}

Float16 f16_min(Float16 a, Float16 b) {
  return Float16::FromFloat32(JSMin(a.ToFloat32(), b.ToFloat32()));
}

void f16x8_min_wrapper(Address data) {
  simd_float16_bin_wrapper<Float16, &f16_min>(data);
}

Float16 f16_max(Float16 a, Float16 b) {
  return Float16::FromFloat32(JSMax(a.ToFloat32(), b.ToFloat32()));
}

void f16x8_max_wrapper(Address data) {
  simd_float16_bin_wrapper<Float16, &f16_max>(data);
}

Float16 f16_pmin(Float16 a, Float16 b) {
  return Float16::FromFloat32(std::min(a.ToFloat32(), b.ToFloat32()));
}

void f16x8_pmin_wrapper(Address data) {
  simd_float16_bin_wrapper<Float16, &f16_pmin>(data);
}

Float16 f16_pmax(Float16 a, Float16 b) {
  return Float16::FromFloat32(std::max(a.ToFloat32(), b.ToFloat32()));
}

void f16x8_pmax_wrapper(Address data) {
  simd_float16_bin_wrapper<Float16, &f16_pmax>(data);
}

template <typename T, typename R, R (*float_un_op)(T)>
void simd_float_un_wrapper(Address data) {
  constexpr int n = kSimd128Size / sizeof(T);
  for (int i = 0; i < n; i++) {
    T input = ReadUnalignedValue<T>(data + (i * sizeof(T)));
    R value = float_un_op(input);
    WriteUnalignedValue<R>(data + (i * sizeof(T)), value);
  }
}

int16_t ConvertToIntS(Float16 val) {
  float f32 = val.ToFloat32();
  if (std::isnan(f32)) return 0;
  if (f32 > float{kMaxInt16}) return kMaxInt16;
  if (f32 < float{kMinInt16}) return kMinInt16;
  return static_cast<int16_t>(f32);
}

uint16_t ConvertToIntU(Float16 val) {
  float f32 = val.ToFloat32();
  if (std::isnan(f32)) return 0;
  if (f32 > float{kMaxUInt16}) return kMaxUInt16;
  if (f32 < float{0}) return 0;
  return static_cast<uint16_t>(f32);
}

void i16x8_sconvert_f16x8_wrapper(Address data) {
  simd_float_un_wrapper<Float16, int16_t, &ConvertToIntS>(data);
}

void i16x8_uconvert_f16x8_wrapper(Address data) {
  simd_float_un_wrapper<Float16, uint16_t, &ConvertToIntU>(data);
}

Float16 ConvertToF16S(int16_t val) { return Float16::FromFloat32(val); }

void f16x8_sconvert_i16x8_wrapper(Address data) {
  simd_float_un_wrapper<int16_t, Float16, &ConvertToF16S>(data);
}

Float16 ConvertToF16U(uint16_t val) { return Float16::FromFloat32(val); }

void f16x8_uconvert_i16x8_wrapper(Address data) {
  simd_float_un_wrapper<uint16_t, Float16, &ConvertToF16U>(data);
}

void f32x4_promote_low_f16x8_wrapper(Address data) {
  // Result is stored in the same buffer, so read all values to local
  // stack variables first.
  Float16 a = Float16::Read(data);
  Float16 b = Float16::Read(data + sizeof(Float16));
  Float16 c = Float16::Read(data + 2 * sizeof(Float16));
  Float16 d = Float16::Read(data + 3 * sizeof(Float16));

  WriteUnalignedValue<float>(data, a.ToFloat32());
  WriteUnalignedValue<float>(data + sizeof(float), b.ToFloat32());
  WriteUnalignedValue<float>(data + (2 * sizeof(float)), c.ToFloat32());
  WriteUnalignedValue<float>(data + (3 * sizeof(float)), d.ToFloat32());
}

void f16x8_demote_f32x4_zero_wrapper(Address data) {
#if V8_TARGET_BIG_ENDIAN
  for (int i = 3, j = 7; i >= 0; i--, j--) {
    float input = ReadUnalignedValue<float>(data + (i * sizeof(float)));
    Float16::FromFloat32(input).Write(data + (j * sizeof(Float16)));
  }
  for (int i = 0; i < 4; i++) {
    WriteUnalignedValue<Float16>(data + (i * sizeof(Float16)),
                                 Float16::FromFloat32(0));
  }
#else
  for (int i = 0; i < 4; i++) {
    float input = ReadUnalignedValue<float>(data + (i * sizeof(float)));
    Float16::FromFloat32(input).Write(data + (i * sizeof(Float16)));
  }
  for (int i = 4; i < 8; i++) {
    WriteUnalignedValue<Float16>(data + (i * sizeof(Float16)),
                                 Float16::FromFloat32(0));
  }
#endif
}

void f16x8_demote_f64x2_zero_wrapper(Address data) {
#if V8_TARGET_BIG_ENDIAN
  for (int i = 1, j = 7; i >= 0; i--, j--) {
    double input = ReadUnalignedValue<double>(data + (i * sizeof(double)));
    WriteUnalignedValue<uint16_t>(data + (j * sizeof(uint16_t)),
                                  DoubleToFloat16(input));
  }
  for (int i = 0; i < 6; i++) {
    WriteUnalignedValue<Float16>(data + (i * sizeof(Float16)),
                                 Float16::FromFloat32(0));
  }
#else
  for (int i = 0; i < 2; i++) {
    double input = ReadUnalignedValue<double>(data + (i * sizeof(double)));
    WriteUnalignedValue<uint16_t>(data + (i * sizeof(uint16_t)),
                                  DoubleToFloat16(input));
  }
  for (int i = 2; i < 8; i++) {
    WriteUnalignedValue<Float16>(data + (i * sizeof(Float16)),
                                 Float16::FromFloat32(0));
  }
#endif
}

template <float (*float_fma_op)(float, float, float)>
void simd_float16_fma_wrapper(Address data) {
  constexpr int n = kSimd128Size / sizeof(Float16);
  for (int i = 0; i < n; i++) {
    Address offset = data + i * sizeof(Float16);
    Float16 a = Float16::Read(offset);
    Float16 b = Float16::Read(offset + kSimd128Size);
    Float16 c = Float16::Read(offset + 2 * kSimd128Size);
    float value = float_fma_op(a.ToFloat32(), b.ToFloat32(), c.ToFloat32());
    Float16::FromFloat32(value).Write(offset);
  }
}

float Qfma(float a, float b, float c) { return a * b + c; }

void f16x8_qfma_wrapper(Address data) {
  return simd_float16_fma_wrapper<&Qfma>(data);
}

float Qfms(float a, float b, float c) { return -(a * b) + c; }

void f16x8_qfms_wrapper(Address data) {
  return simd_float16_fma_wrapper<&Qfms>(data);
}

namespace {
class V8_NODISCARD ThreadNotInWasmScope {
// Asan on Windows triggers exceptions to allocate shadow memory lazily. When
// this function is called from WebAssembly, these exceptions would be handled
// by the trap handler before they get handled by Asan, and thereby confuse the
// thread-in-wasm flag. Therefore we disable ASAN for this function.
// Alternatively we could reset the thread-in-wasm flag before calling this
// function. However, as this is only a problem with Asan on Windows, we did not
// consider it worth the overhead.
#if defined(RESET_THREAD_IN_WASM_FLAG_FOR_ASAN_ON_WINDOWS)

 public:
  ThreadNotInWasmScope() : thread_was_in_wasm_(trap_handler::IsThreadInWasm()) {
    if (thread_was_in_wasm_) {
      trap_handler::ClearThreadInWasm();
    }
  }

  ~ThreadNotInWasmScope() {
    if (thread_was_in_wasm_) {
      trap_handler::SetThreadInWasm();
    }
  }

 private:
  bool thread_was_in_wasm_;
#else

 public:
  ThreadNotInWasmScope() {
    // This is needed to avoid compilation errors (unused variable).
    USE(this);
  }
#endif
};

inline uint8_t* EffectiveAddress(Tagged<WasmTrustedInstanceData> trusted_data,
                                 uint32_t mem_index, uintptr_t index) {
  return trusted_data->memory_base(mem_index) + index;
}

template <typename V>
V ReadAndIncrementOffset(Address data, size_t* offset) {
  V result = ReadUnalignedValue<V>(data + *offset);
  *offset += sizeof(V);
  return result;
}

constexpr int32_t kSuccess = 1;
constexpr int32_t kOutOfBounds = 0;
}  // namespace

int32_t memory_init_wrapper(Address trusted_data_addr, uint32_t mem_index,
                            uintptr_t dst, uint32_t src, uint32_t seg_index,
                            uint32_t size) {
  ThreadNotInWasmScope thread_not_in_wasm_scope;
  DisallowGarbageCollection no_gc;
  Tagged<WasmTrustedInstanceData> trusted_data =
      Cast<WasmTrustedInstanceData>(Tagged<Object>{trusted_data_addr});

  uint64_t mem_size = trusted_data->memory_size(mem_index);
  if (!base::IsInBounds<uint64_t>(dst, size, mem_size)) return kOutOfBounds;

  uint32_t seg_size = trusted_data->data_segment_sizes()->get(seg_index);
  if (!base::IsInBounds<uint32_t>(src, size, seg_size)) return kOutOfBounds;

  uint8_t* seg_start = reinterpret_cast<uint8_t*>(
      trusted_data->data_segment_starts()->get(seg_index));
  std::memcpy(EffectiveAddress(trusted_data, mem_index, dst), seg_start + src,
              size);
  return kSuccess;
}

int32_t memory_copy_wrapper(Address trusted_data_addr, uint32_t dst_mem_index,
                            uint32_t src_mem_index, uintptr_t dst,
                            uintptr_t src, uintptr_t size) {
  ThreadNotInWasmScope thread_not_in_wasm_scope;
  DisallowGarbageCollection no_gc;
  Tagged<WasmTrustedInstanceData> trusted_data =
      Cast<WasmTrustedInstanceData>(Tagged<Object>{trusted_data_addr});

  size_t dst_mem_size = trusted_data->memory_size(dst_mem_index);
  size_t src_mem_size = trusted_data->memory_size(src_mem_index);
  static_assert(std::is_same_v<size_t, uintptr_t>);
  if (!base::IsInBounds<size_t>(dst, size, dst_mem_size)) return kOutOfBounds;
  if (!base::IsInBounds<size_t>(src, size, src_mem_size)) return kOutOfBounds;

  // Use std::memmove, because the ranges can overlap.
  std::memmove(EffectiveAddress(trusted_data, dst_mem_index, dst),
               EffectiveAddress(trusted_data, src_mem_index, src), size);
  return kSuccess;
}

int32_t memory_fill_wrapper(Address trusted_data_addr, uint32_t mem_index,
                            uintptr_t dst, uint8_t value, uintptr_t size) {
  ThreadNotInWasmScope thread_not_in_wasm_scope;
  DisallowGarbageCollection no_gc;

  Tagged<WasmTrustedInstanceData> trusted_data =
      Cast<WasmTrustedInstanceData>(Tagged<Object>{trusted_data_addr});

  uint64_t mem_size = trusted_data->memory_size(mem_index);
  if (!base::IsInBounds<uint64_t>(dst, size, mem_size)) return kOutOfBounds;

  std::memset(EffectiveAddress(trusted_data, mem_index, dst), value, size);
  return kSuccess;
}

namespace {
inline void* ArrayElementAddress(Address array, uint32_t index,
                                 int element_size_bytes) {
  return reinterpret_cast<void*>(array + WasmArray::kHeaderSize -
                                 kHeapObjectTag + index * element_size_bytes);
}
inline void* ArrayElementAddress(Tagged<WasmArray> array, uint32_t index,
                                 int element_size_bytes) {
  return ArrayElementAddress(array.ptr(), index, element_size_bytes);
}
}  // namespace

void array_copy_wrapper(Address raw_dst_array, uint32_t dst_index,
                        Address raw_src_array, uint32_t src_index,
                        uint32_t length) {
  DCHECK_GT(length, 0);
  ThreadNotInWasmScope thread_not_in_wasm_scope;
  DisallowGarbageCollection no_gc;
  Tagged<WasmArray> dst_array = Cast<WasmArray>(Tagged<Object>(raw_dst_array));
  Tagged<WasmArray> src_array = Cast<WasmArray>(Tagged<Object>(raw_src_array));

  bool overlapping_ranges =
      dst_array.ptr() == src_array.ptr() &&
      (dst_index < src_index ? dst_index + length > src_index
                             : src_index + length > dst_index);
  wasm::ValueType element_type = src_array->type()->element_type();
  if (element_type.is_reference()) {
    ObjectSlot dst_slot = dst_array->ElementSlot(dst_index);
    ObjectSlot src_slot = src_array->ElementSlot(src_index);
    Heap* heap = dst_array->GetIsolate()->heap();
    if (overlapping_ranges) {
      heap->MoveRange(dst_array, dst_slot, src_slot, length,
                      UPDATE_WRITE_BARRIER);
    } else {
      heap->CopyRange(dst_array, dst_slot, src_slot, length,
                      UPDATE_WRITE_BARRIER);
    }
  } else {
    int element_size_bytes = element_type.value_kind_size();
    void* dst = ArrayElementAddress(dst_array, dst_index, element_size_bytes);
    void* src = ArrayElementAddress(src_array, src_index, element_size_bytes);
    size_t copy_size = length * element_size_bytes;
    if (overlapping_ranges) {
      MemMove(dst, src, copy_size);
    } else {
      MemCopy(dst, src, copy_size);
    }
  }
}

void array_fill_wrapper(Address raw_array, uint32_t index, uint32_t length,
                        uint32_t emit_write_barrier, uint32_t raw_type,
                        Address initial_value_addr) {
  ThreadNotInWasmScope thread_not_in_wasm_scope;
  DisallowGarbageCollection no_gc;
  ValueType type = ValueType::FromRawBitField(raw_type);
  int8_t* initial_element_address = reinterpret_cast<int8_t*>(
      ArrayElementAddress(raw_array, index, type.value_kind_size()));
  // Stack pointers are only aligned to 4 bytes.
  int64_t initial_value = base::ReadUnalignedValue<int64_t>(initial_value_addr);
  const int bytes_to_set = length * type.value_kind_size();

  // If the initial value is zero, we memset the array.
  if (type.is_numeric() && initial_value == 0) {
    std::memset(initial_element_address, 0, bytes_to_set);
    return;
  }

  // We implement the general case by setting the first 8 bytes manually, then
  // filling the rest by exponentially growing {memcpy}s.

  DCHECK_GE(static_cast<size_t>(bytes_to_set), sizeof(int64_t));

  switch (type.kind()) {
    case kI64:
    case kF64: {
      // Array elements are only aligned to 4 bytes, therefore
      // `initial_element_address` may be misaligned as a 64-bit pointer.
      base::WriteUnalignedValue<int64_t>(
          reinterpret_cast<Address>(initial_element_address), initial_value);
      break;
    }
    case kI32:
    case kF32: {
      int32_t* base = reinterpret_cast<int32_t*>(initial_element_address);
      base[0] = base[1] = static_cast<int32_t>(initial_value);
      break;
    }
    case kF16:
    case kI16: {
      int16_t* base = reinterpret_cast<int16_t*>(initial_element_address);
      base[0] = base[1] = base[2] = base[3] =
          static_cast<int16_t>(initial_value);
      break;
    }
    case kI8: {
      int8_t* base = reinterpret_cast<int8_t*>(initial_element_address);
      for (size_t i = 0; i < sizeof(int64_t); i++) {
        base[i] = static_cast<int8_t>(initial_value);
      }
      break;
    }
    case kRefNull:
    case kRef:
      if constexpr (kTaggedSize == 4) {
        int32_t* base = reinterpret_cast<int32_t*>(initial_element_address);
        base[0] = base[1] = static_cast<int32_t>(initial_value);
      } else {
        // We use WriteUnalignedValue; see above.
        base::WriteUnalignedValue(
            reinterpret_cast<Address>(initial_element_address), initial_value);
      }
      break;
    case kS128:
    case kRtt:
    case kVoid:
    case kBottom:
      UNREACHABLE();
  }

  int bytes_already_set = sizeof(int64_t);

  while (bytes_already_set * 2 <= bytes_to_set) {
    std::memcpy(initial_element_address + bytes_already_set,
                initial_element_address, bytes_already_set);
    bytes_already_set *= 2;
  }

  if (bytes_already_set < bytes_to_set) {
    std::memcpy(initial_element_address + bytes_already_set,
                initial_element_address, bytes_to_set - bytes_already_set);
  }

  if (emit_write_barrier) {
    DCHECK(type.is_reference());
    Tagged<WasmArray> array = Cast<WasmArray>(Tagged<Object>(raw_array));
    Isolate* isolate = array->GetIsolate();
    ObjectSlot start(reinterpret_cast<Address>(initial_element_address));
    ObjectSlot end(
        reinterpret_cast<Address>(initial_element_address + bytes_to_set));
    WriteBarrier::ForRange(isolate->heap(), array, start, end);
  }
}

double flat_string_to_f64(Address string_address) {
  Tagged<String> s = Cast<String>(Tagged<Object>(string_address));
  return FlatStringToDouble(s, ALLOW_TRAILING_JUNK,
                            std::numeric_limits<double>::quiet_NaN());
}

void sync_stack_limit(Isolate* isolate) {
  DisallowGarbageCollection no_gc;

  isolate->SyncStackLimit();
}

void return_switch(Isolate* isolate, Address raw_continuation) {
  DisallowGarbageCollection no_gc;

  Tagged<WasmContinuationObject> continuation =
      Cast<WasmContinuationObject>(Tagged<Object>{raw_continuation});
  wasm::StackMemory* stack =
      reinterpret_cast<StackMemory*>(continuation->stack());
  isolate->RetireWasmStack(stack);
  isolate->SyncStackLimit();
}

intptr_t switch_to_the_central_stack(Isolate* isolate, uintptr_t current_sp) {
  ThreadLocalTop* thread_local_top = isolate->thread_local_top();
  StackGuard* stack_guard = isolate->stack_guard();

  auto secondary_stack_limit = stack_guard->real_jslimit();

  stack_guard->SetStackLimitForStackSwitching(
      thread_local_top->central_stack_limit_);

  thread_local_top->secondary_stack_limit_ = secondary_stack_limit;
  thread_local_top->secondary_stack_sp_ = current_sp;
  thread_local_top->is_on_central_stack_flag_ = true;

  auto counter = isolate->wasm_switch_to_the_central_stack_counter();
  isolate->set_wasm_switch_to_the_central_stack_counter(counter + 1);

  return thread_local_top->central_stack_sp_;
}

void switch_from_the_central_stack(Isolate* isolate) {
  ThreadLocalTop* thread_local_top = isolate->thread_local_top();
  CHECK_NE(thread_local_top->secondary_stack_sp_, 0);
  CHECK_NE(thread_local_top->secondary_stack_limit_, 0);

  auto secondary_stack_limit = thread_local_top->secondary_stack_limit_;
  thread_local_top->secondary_stack_limit_ = 0;
  thread_local_top->secondary_stack_sp_ = 0;
  thread_local_top->is_on_central_stack_flag_ = false;

  StackGuard* stack_guard = isolate->stack_guard();
  stack_guard->SetStackLimitForStackSwitching(secondary_stack_limit);
}

intptr_t switch_to_the_central_stack_for_js(Isolate* isolate, Address fp) {
  auto active_suspender =
      Cast<WasmSuspenderObject>(isolate->root(RootIndex::kActiveSuspender));
  ThreadLocalTop* thread_local_top = isolate->thread_local_top();
  StackGuard* stack_guard = isolate->stack_guard();
  auto* stack = reinterpret_cast<StackMemory*>(
      Cast<WasmContinuationObject>(active_suspender->continuation())->stack());
  stack->set_stack_switch_info(fp, thread_local_top->central_stack_sp_);
  stack_guard->SetStackLimitForStackSwitching(
      thread_local_top->central_stack_limit_);
  thread_local_top->is_on_central_stack_flag_ = true;
  return thread_local_top->central_stack_sp_;
}

void switch_from_the_central_stack_for_js(Isolate* isolate) {
  // The stack only contains wasm frames after this JS call.
  auto active_suspender =
      Cast<WasmSuspenderObject>(isolate->root(RootIndex::kActiveSuspender));
  auto* stack = reinterpret_cast<StackMemory*>(
      Cast<WasmContinuationObject>(active_suspender->continuation())->stack());
  stack->clear_stack_switch_info();
  ThreadLocalTop* thread_local_top = isolate->thread_local_top();
  thread_local_top->is_on_central_stack_flag_ = false;
  StackGuard* stack_guard = isolate->stack_guard();
  stack_guard->SetStackLimitForStackSwitching(
      reinterpret_cast<uintptr_t>(stack->jslimit()));
}

// frame_size includes param slots area and extra frame slots above FP.
Address grow_stack(Isolate* isolate, void* current_sp, size_t frame_size,
                   size_t gap, Address current_fp) {
  // Check if this is a real stack overflow.
  StackLimitCheck check(isolate);
  if (check.WasmHasOverflowed(gap)) {
    Tagged<WasmContinuationObject> current_continuation =
        Cast<WasmContinuationObject>(
            isolate->root(RootIndex::kActiveContinuation));
    // If there is no parent, then the current stack is the main isolate stack.
    if (IsUndefined(current_continuation->parent())) {
      return 0;
    }
    auto stack =
        reinterpret_cast<wasm::StackMemory*>(current_continuation->stack());
    DCHECK(stack->IsActive());
    if (!stack->Grow(current_fp)) {
      return 0;
    }

    Address new_sp = stack->base() - frame_size;
    // Here we assume stack values don't refer other moved stack slots.
    // A stack grow event happens right in the beginning of the function
    // call so moved slots contain only incoming params and frame header.
    // So, it is reasonable to assume no self references.
    std::memcpy(reinterpret_cast<void*>(new_sp), current_sp, frame_size);

#if V8_TARGET_ARCH_ARM64
    Address new_fp =
        new_sp + (current_fp - reinterpret_cast<Address>(current_sp));
    Address old_pc_address = current_fp + CommonFrameConstants::kCallerPCOffset;
    Address new_pc_address = new_fp + CommonFrameConstants::kCallerPCOffset;
    Address old_signed_pc = base::Memory<Address>(old_pc_address);
    Address new_signed_pc = PointerAuthentication::MoveSignedPC(
        isolate, old_signed_pc, new_pc_address + kSystemPointerSize,
        old_pc_address + kSystemPointerSize);
    WriteUnalignedValue<Address>(new_pc_address, new_signed_pc);
#endif

    isolate->stack_guard()->SetStackLimitForStackSwitching(
        reinterpret_cast<uintptr_t>(stack->jslimit()));
    return new_sp;
  }

  return 0;
}

Address shrink_stack(Isolate* isolate) {
  Tagged<WasmContinuationObject> current_continuation =
      Cast<WasmContinuationObject>(
          isolate->root(RootIndex::kActiveContinuation));
  // If there is no parent, then the current stack is the main isolate stack.
  if (IsUndefined(current_continuation->parent())) {
    return 0;
  }
  auto stack =
      reinterpret_cast<wasm::StackMemory*>(current_continuation->stack());
  DCHECK(stack->IsActive());
  Address old_fp = stack->Shrink();

  isolate->stack_guard()->SetStackLimitForStackSwitching(
      reinterpret_cast<uintptr_t>(stack->jslimit()));
  return old_fp;
}

Address load_old_fp(Isolate* isolate) {
  Tagged<WasmContinuationObject> current_continuation =
      Cast<WasmContinuationObject>(
          isolate->root(RootIndex::kActiveContinuation));
  // If there is no parent, then the current stack is the main isolate stack.
  if (IsUndefined(current_continuation->parent())) {
    return 0;
  }
  auto stack =
      reinterpret_cast<wasm::StackMemory*>(current_continuation->stack());
  DCHECK_EQ(stack->jmpbuf()->state, wasm::JumpBuffer::Active);
  return stack->old_fp();
}

}  // namespace v8::internal::wasm

#undef V8_WITH_SANITIZER
#undef RESET_THREAD_IN_WASM_FLAG_FOR_ASAN_ON_WINDOWS
