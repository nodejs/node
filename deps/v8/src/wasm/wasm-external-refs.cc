// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <limits>

#include "include/v8config.h"

#include "src/base/bits.h"
#include "src/base/ieee754.h"
#include "src/common/assert-scope.h"
#include "src/utils/memcopy.h"
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
#include "src/utils/utils.h"
#include "src/wasm/wasm-external-refs.h"

namespace v8 {
namespace internal {
namespace wasm {

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
  WriteUnalignedValue<float>(data, nearbyintf(ReadUnalignedValue<float>(data)));
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
  WriteUnalignedValue<double>(data,
                              nearbyint(ReadUnalignedValue<double>(data)));
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
    result *= bit_cast<float>(multiplier_bits);
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
  // We use "<" here to check the upper bound because of rounding problems: With
  // "<=" some inputs would be considered within int64 range which are actually
  // not within int64 range.
  float input = ReadUnalignedValue<float>(data);
  if (input >= static_cast<float>(std::numeric_limits<int64_t>::min()) &&
      input < static_cast<float>(std::numeric_limits<int64_t>::max())) {
    WriteUnalignedValue<int64_t>(data, static_cast<int64_t>(input));
    return 1;
  }
  return 0;
}

int32_t float32_to_uint64_wrapper(Address data) {
  float input = ReadUnalignedValue<float>(data);
  // We use "<" here to check the upper bound because of rounding problems: With
  // "<=" some inputs would be considered within uint64 range which are actually
  // not within uint64 range.
  if (input > -1.0 &&
      input < static_cast<float>(std::numeric_limits<uint64_t>::max())) {
    WriteUnalignedValue<uint64_t>(data, static_cast<uint64_t>(input));
    return 1;
  }
  return 0;
}

int32_t float64_to_int64_wrapper(Address data) {
  // We use "<" here to check the upper bound because of rounding problems: With
  // "<=" some inputs would be considered within int64 range which are actually
  // not within int64 range.
  double input = ReadUnalignedValue<double>(data);
  if (input >= static_cast<double>(std::numeric_limits<int64_t>::min()) &&
      input < static_cast<double>(std::numeric_limits<int64_t>::max())) {
    WriteUnalignedValue<int64_t>(data, static_cast<int64_t>(input));
    return 1;
  }
  return 0;
}

int32_t float64_to_uint64_wrapper(Address data) {
  // We use "<" here to check the upper bound because of rounding problems: With
  // "<=" some inputs would be considered within uint64 range which are actually
  // not within uint64 range.
  double input = ReadUnalignedValue<double>(data);
  if (input > -1.0 &&
      input < static_cast<double>(std::numeric_limits<uint64_t>::max())) {
    WriteUnalignedValue<uint64_t>(data, static_cast<uint64_t>(input));
    return 1;
  }
  return 0;
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

uint32_t word32_ctz_wrapper(Address data) {
  return base::bits::CountTrailingZeros(ReadUnalignedValue<uint32_t>(data));
}

uint32_t word64_ctz_wrapper(Address data) {
  return base::bits::CountTrailingZeros(ReadUnalignedValue<uint64_t>(data));
}

uint32_t word32_popcnt_wrapper(Address data) {
  return base::bits::CountPopulation(ReadUnalignedValue<uint32_t>(data));
}

uint32_t word64_popcnt_wrapper(Address data) {
  return base::bits::CountPopulation(ReadUnalignedValue<uint64_t>(data));
}

uint32_t word32_rol_wrapper(Address data) {
  uint32_t input = ReadUnalignedValue<uint32_t>(data);
  uint32_t shift = ReadUnalignedValue<uint32_t>(data + sizeof(input)) & 31;
  return (input << shift) | (input >> ((32 - shift) & 31));
}

uint32_t word32_ror_wrapper(Address data) {
  uint32_t input = ReadUnalignedValue<uint32_t>(data);
  uint32_t shift = ReadUnalignedValue<uint32_t>(data + sizeof(input)) & 31;
  return (input >> shift) | (input << ((32 - shift) & 31));
}

void word64_rol_wrapper(Address data) {
  uint64_t input = ReadUnalignedValue<uint64_t>(data);
  uint64_t shift = ReadUnalignedValue<uint64_t>(data + sizeof(input)) & 63;
  uint64_t result = (input << shift) | (input >> ((64 - shift) & 63));
  WriteUnalignedValue<uint64_t>(data, result);
}

void word64_ror_wrapper(Address data) {
  uint64_t input = ReadUnalignedValue<uint64_t>(data);
  uint64_t shift = ReadUnalignedValue<uint64_t>(data + sizeof(input)) & 63;
  uint64_t result = (input >> shift) | (input << ((64 - shift) & 63));
  WriteUnalignedValue<uint64_t>(data, result);
}

void float64_pow_wrapper(Address data) {
  double x = ReadUnalignedValue<double>(data);
  double y = ReadUnalignedValue<double>(data + sizeof(x));
  WriteUnalignedValue<double>(data, base::ieee754::pow(x, y));
}

namespace {
class ThreadNotInWasmScope {
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

#ifdef DISABLE_UNTRUSTED_CODE_MITIGATIONS
inline byte* EffectiveAddress(WasmInstanceObject instance, uint32_t index) {
  return instance.memory_start() + index;
}

inline byte* EffectiveAddress(byte* base, size_t size, uint32_t index) {
  return base + index;
}

#else
inline byte* EffectiveAddress(WasmInstanceObject instance, uint32_t index) {
  // Compute the effective address of the access, making sure to condition
  // the index even in the in-bounds case.
  return instance.memory_start() + (index & instance.memory_mask());
}

inline byte* EffectiveAddress(byte* base, size_t size, uint32_t index) {
  size_t mem_mask = base::bits::RoundUpToPowerOfTwo(size) - 1;
  return base + (index & mem_mask);
}
#endif

template <typename V>
V ReadAndIncrementOffset(Address data, size_t* offset) {
  V result = ReadUnalignedValue<V>(data + *offset);
  *offset += sizeof(V);
  return result;
}
}  // namespace

int32_t memory_init_wrapper(Address data) {
  constexpr int32_t kSuccess = 1;
  constexpr int32_t kOutOfBounds = 0;
  ThreadNotInWasmScope thread_not_in_wasm_scope;
  DisallowHeapAllocation disallow_heap_allocation;
  size_t offset = 0;
  Object raw_instance = ReadAndIncrementOffset<Object>(data, &offset);
  WasmInstanceObject instance = WasmInstanceObject::cast(raw_instance);
  uint32_t dst = ReadAndIncrementOffset<uint32_t>(data, &offset);
  uint32_t src = ReadAndIncrementOffset<uint32_t>(data, &offset);
  uint32_t seg_index = ReadAndIncrementOffset<uint32_t>(data, &offset);
  size_t size = ReadAndIncrementOffset<uint32_t>(data, &offset);

  size_t mem_size = instance.memory_size();
  if (!base::IsInBounds(dst, size, mem_size)) return kOutOfBounds;

  size_t seg_size = instance.data_segment_sizes()[seg_index];
  if (!base::IsInBounds(src, size, seg_size)) return kOutOfBounds;

  byte* seg_start =
      reinterpret_cast<byte*>(instance.data_segment_starts()[seg_index]);
  std::memcpy(EffectiveAddress(instance, dst),
              EffectiveAddress(seg_start, seg_size, src), size);
  return kSuccess;
}

int32_t memory_copy_wrapper(Address data) {
  constexpr int32_t kSuccess = 1;
  constexpr int32_t kOutOfBounds = 0;
  ThreadNotInWasmScope thread_not_in_wasm_scope;
  DisallowHeapAllocation disallow_heap_allocation;
  size_t offset = 0;
  Object raw_instance = ReadAndIncrementOffset<Object>(data, &offset);
  WasmInstanceObject instance = WasmInstanceObject::cast(raw_instance);
  uint32_t dst = ReadAndIncrementOffset<uint32_t>(data, &offset);
  uint32_t src = ReadAndIncrementOffset<uint32_t>(data, &offset);
  size_t size = ReadAndIncrementOffset<uint32_t>(data, &offset);

  size_t mem_size = instance.memory_size();
  if (!base::IsInBounds(dst, size, mem_size)) return kOutOfBounds;
  if (!base::IsInBounds(src, size, mem_size)) return kOutOfBounds;

  // Use std::memmove, because the ranges can overlap.
  std::memmove(EffectiveAddress(instance, dst), EffectiveAddress(instance, src),
               size);
  return kSuccess;
}

int32_t memory_fill_wrapper(Address data) {
  constexpr int32_t kSuccess = 1;
  constexpr int32_t kOutOfBounds = 0;

  ThreadNotInWasmScope thread_not_in_wasm_scope;
  DisallowHeapAllocation disallow_heap_allocation;

  size_t offset = 0;
  Object raw_instance = ReadAndIncrementOffset<Object>(data, &offset);
  WasmInstanceObject instance = WasmInstanceObject::cast(raw_instance);
  uint32_t dst = ReadAndIncrementOffset<uint32_t>(data, &offset);
  uint8_t value =
      static_cast<uint8_t>(ReadAndIncrementOffset<uint32_t>(data, &offset));
  size_t size = ReadAndIncrementOffset<uint32_t>(data, &offset);

  size_t mem_size = instance.memory_size();
  if (!base::IsInBounds(dst, size, mem_size)) return kOutOfBounds;

  std::memset(EffectiveAddress(instance, dst), value, size);
  return kSuccess;
}

static WasmTrapCallbackForTesting wasm_trap_callback_for_testing = nullptr;

void set_trap_callback_for_testing(WasmTrapCallbackForTesting callback) {
  wasm_trap_callback_for_testing = callback;
}

void call_trap_callback_for_testing() {
  if (wasm_trap_callback_for_testing) {
    wasm_trap_callback_for_testing();
  }
}

}  // namespace wasm
}  // namespace internal
}  // namespace v8

#undef V8_WITH_SANITIZER
#undef RESET_THREAD_IN_WASM_FLAG_FOR_ASAN_ON_WINDOWS
