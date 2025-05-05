// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/objects/simd.h"

#include "src/base/cpu.h"
#include "src/codegen/cpu-features.h"
#include "src/objects/compressed-slots.h"
#include "src/objects/fixed-array-inl.h"
#include "src/objects/heap-number-inl.h"
#include "src/objects/js-array-buffer-inl.h"
#include "src/objects/smi-inl.h"

#ifdef _MSC_VER
// MSVC doesn't define SSE3. However, it does define AVX, and AVX implies SSE3.
#ifdef __AVX__
#ifndef __SSE3__
#define __SSE3__
#endif
#endif
#endif

#ifdef __SSE3__
#include <immintrin.h>
#endif

#ifdef V8_HOST_ARCH_ARM64
// We use Neon only on 64-bit ARM (because on 32-bit, some instructions and some
// types are not available). Note that ARM64 is guaranteed to have Neon.
#define NEON64
#include <arm_neon.h>
#endif

namespace v8 {
namespace internal {

namespace {

enum class SimdKinds { kSSE, kNeon, kAVX2, kNone };

inline SimdKinds get_vectorization_kind() {
#ifdef __SSE3__
#if defined(V8_TARGET_ARCH_IA32) || defined(V8_TARGET_ARCH_X64)
  bool has_avx2 = CpuFeatures::IsSupported(AVX2);
#else
  bool has_avx2 = false;
#endif
  if (has_avx2) {
    return SimdKinds::kAVX2;
  } else {
    // No need for a runtime check since we do not support x86/x64 CPUs without
    // SSE3.
    return SimdKinds::kSSE;
  }
#elif defined(NEON64)
  // No need for a runtime check since all Arm64 CPUs have Neon.
  return SimdKinds::kNeon;
#else
  return SimdKinds::kNone;
#endif
}

// Searches for |search_element| in |array| using a simple non-vectorized linear
// search. This is used as a fall-back when SIMD are not available, and to
// process the end of arrays than SIMD cannot process.
template <typename T>
inline uintptr_t slow_search(T* array, uintptr_t array_len, uintptr_t index,
                             T search_element) {
  for (; index < array_len; index++) {
    if (array[index] == search_element) {
      return index;
    }
  }
  return -1;
}

#ifdef NEON64
// extract_first_nonzero_index returns the first non-zero index in |v|. |v| is a
// Neon vector that can be either 32x4 (the return is then 0, 1, 2 or 3) or 64x2
// (the return is then 0 or 1). This is more or less equivalent to doing a
// movemask followed by a tzcnt on Intel.
//
// The input |v| should be a vector of -1 or 0 (for instance {0, 0},
// {0, -1, 0, -1}, {0, -1, 0, 0}), where -1 represents a match (and 0 a
// non-match), that was obtained by doing a vceqq. This function extract the
// index of the first non-zero item of the vector. To do so, we "and" the vector
// with {4, 3, 2, 1} (each number is "4 - the index of the item it's in"), which
// produces a vector of "indices or 0". Then, we extract the maximum of this
// vector, which is the index of the 1st match. An example:
//
//   v = {-1, 0, 0, -1}
//   mask = {4, 3, 2, 1}
//   v & mask = {4, 0, 0, 1}
//   max(v & mask) = 4
//   index of the first match = 4-max = 4-4 = 0
//

// With MSVC, uint32x4_t and uint64x2_t typedef to a union, where first member
// is uint64_t[2], and not uint32_t[4].
// C++ standard dictates that a union can only be initialized through its first
// member, which forces us to have uint64_t[2] for definition.
#if defined(_MSC_VER) && !defined(__clang__)
#define PACK32x4(w, x, y, z) \
  { ((w) + (uint64_t(x) << 32)), ((y) + (uint64_t(z) << 32)) }
#else
#define PACK32x4(w, x, y, z) \
  { (w), (x), (y), (z) }
#endif  // MSVC workaround

V8_ALLOW_UNUSED inline int extract_first_nonzero_index_uint32x4_t(
    uint32x4_t v) {
  uint32x4_t mask = PACK32x4(4, 3, 2, 1);
  mask = vandq_u32(mask, v);
  return 4 - vmaxvq_u32(mask);
}

inline int extract_first_nonzero_index_uint64x2_t(uint64x2_t v) {
  uint32x4_t mask =
      PACK32x4(2, 0, 1, 0);  // Could also be {2,2,1,1} or {0,2,0,1}
  mask = vandq_u32(mask, vreinterpretq_u32_u64(v));
  return 2 - vmaxvq_u32(mask);
}

inline int32_t reinterpret_vmaxvq_u64(uint64x2_t v) {
  return vmaxvq_u32(vreinterpretq_u32_u64(v));
}
#endif

#define VECTORIZED_LOOP_Neon(type_load, type_eq, set1, cmp, movemask)        \
  {                                                                          \
    constexpr int elems_in_vector = sizeof(type_load) / sizeof(T);           \
    type_load search_element_vec = set1(search_element);                     \
                                                                             \
    for (; index + elems_in_vector <= array_len; index += elems_in_vector) { \
      type_load vector = *reinterpret_cast<type_load*>(&array[index]);       \
      type_eq eq = cmp(vector, search_element_vec);                          \
      if (movemask(eq)) {                                                    \
        return index + extract_first_nonzero_index_##type_eq(eq);            \
      }                                                                      \
    }                                                                        \
  }

#define VECTORIZED_LOOP_x86(type_load, type_eq, set1, cmp, movemask, extract) \
  {                                                                           \
    constexpr int elems_in_vector = sizeof(type_load) / sizeof(T);            \
    type_load search_element_vec = set1(search_element);                      \
                                                                              \
    for (; index + elems_in_vector <= array_len; index += elems_in_vector) {  \
      type_load vector = *reinterpret_cast<type_load*>(&array[index]);        \
      type_eq eq = cmp(vector, search_element_vec);                           \
      int eq_mask = movemask(eq);                                             \
      if (eq_mask) {                                                          \
        return index + extract(eq_mask);                                      \
      }                                                                       \
    }                                                                         \
  }

#ifdef __SSE3__
__m128i _mm_cmpeq_epi64_nosse4_2(__m128i a, __m128i b) {
  __m128i res = _mm_cmpeq_epi32(a, b);
  // For each 64-bit value swap results of lower 32 bits comparison with
  // the results of upper 32 bits comparison.
  __m128i res_swapped = _mm_shuffle_epi32(res, _MM_SHUFFLE(2, 3, 0, 1));
  // Report match only when both upper and lower parts of 64-bit values match.
  return _mm_and_si128(res, res_swapped);
}
#endif  // __SSE3__

// Uses SIMD to vectorize the search loop. This function should only be called
// for large-ish arrays. Note that nothing will break if |array_len| is less
// than vectorization_threshold: things will just be slower than necessary.
template <typename T>
inline uintptr_t fast_search_noavx(T* array, uintptr_t array_len,
                                   uintptr_t index, T search_element) {
  static constexpr bool is_uint32 =
      sizeof(T) == sizeof(uint32_t) && std::is_integral_v<T>;
  static constexpr bool is_uint64 =
      sizeof(T) == sizeof(uint64_t) && std::is_integral_v<T>;
  static constexpr bool is_double =
      sizeof(T) == sizeof(double) && std::is_floating_point_v<T>;

  static_assert(is_uint32 || is_uint64 || is_double);

#if !(defined(__SSE3__) || defined(NEON64))
  // No SIMD available.
  return slow_search(array, array_len, index, search_element);
#endif

#ifdef __SSE3__
  const int target_align = 16;
#elif defined(NEON64)
  const int target_align = 16;
#else
  const int target_align = 4;
  UNREACHABLE();
#endif

  // Scalar loop to reach desired alignment
  for (;
       index < array_len &&
       (reinterpret_cast<std::uintptr_t>(&(array[index])) % target_align) != 0;
       index++) {
    if (array[index] == search_element) {
      return index;
    }
  }

  // Inserting one of the vectorized loop
#ifdef __SSE3__
  if constexpr (is_uint32) {
#define MOVEMASK(x) _mm_movemask_ps(_mm_castsi128_ps(x))
#define EXTRACT(x) base::bits::CountTrailingZeros32(x)
    VECTORIZED_LOOP_x86(__m128i, __m128i, _mm_set1_epi32, _mm_cmpeq_epi32,
                        MOVEMASK, EXTRACT)
#undef MOVEMASK
#undef EXTRACT
  } else if constexpr (is_uint64) {
#define MOVEMASK(x) _mm_movemask_ps(_mm_castsi128_ps(x))
// _mm_cmpeq_epi64_nosse4_2() might produce only the following non-zero
// patterns:
//   0b0011 -> 0 (the first value matches),
//   0b1100 -> 1 (the second value matches),
//   0b1111 -> 0 (both first and second value match).
// Thus it's enough to check only the least significant bit.
#define EXTRACT(x) (((x) & 1) ? 0 : 1)
    VECTORIZED_LOOP_x86(__m128i, __m128i, _mm_set1_epi64x,
                        _mm_cmpeq_epi64_nosse4_2, MOVEMASK, EXTRACT)
#undef MOVEMASK
#undef EXTRACT
  } else if constexpr (is_double) {
#define EXTRACT(x) base::bits::CountTrailingZeros32(x)
    VECTORIZED_LOOP_x86(__m128d, __m128d, _mm_set1_pd, _mm_cmpeq_pd,
                        _mm_movemask_pd, EXTRACT)
#undef EXTRACT
  }
#elif defined(NEON64)
  if constexpr (is_uint32) {
    VECTORIZED_LOOP_Neon(uint32x4_t, uint32x4_t, vdupq_n_u32, vceqq_u32,
                         vmaxvq_u32)
  } else if constexpr (is_uint64) {
    VECTORIZED_LOOP_Neon(uint64x2_t, uint64x2_t, vdupq_n_u64, vceqq_u64,
                         reinterpret_vmaxvq_u64)
  } else if constexpr (is_double) {
    VECTORIZED_LOOP_Neon(float64x2_t, uint64x2_t, vdupq_n_f64, vceqq_f64,
                         reinterpret_vmaxvq_u64)
  }
#else
  UNREACHABLE();
#endif

  // The vectorized loop stops when there are not enough items left in the array
  // to fill a vector register. The slow_search function will take care of
  // iterating through the few remaining items.
  return slow_search(array, array_len, index, search_element);
}

#if defined(_MSC_VER) && defined(__clang__)
// Generating AVX2 code with Clang on Windows without the /arch:AVX2 flag does
// not seem possible at the moment.
#define IS_CLANG_WIN 1
#endif

// Since we don't compile with -mavx or -mavx2 (or /arch:AVX2 on MSVC), Clang
// and MSVC do not define __AVX__ nor __AVX2__. Thus, if __SSE3__ is defined, we
// generate the AVX2 code, and, at runtime, we'll decide to call it or not,
// depending on whether the CPU supports AVX2.
#if defined(__SSE3__) && !defined(_M_IX86) && !defined(IS_CLANG_WIN)
#ifdef _MSC_VER
#define TARGET_AVX2
#else
#define TARGET_AVX2 __attribute__((target("avx2")))
#endif
template <typename T>
TARGET_AVX2 inline uintptr_t fast_search_avx(T* array, uintptr_t array_len,
                                             uintptr_t index,
                                             T search_element) {
  static constexpr bool is_uint32 =
      sizeof(T) == sizeof(uint32_t) && std::is_integral_v<T>;
  static constexpr bool is_uint64 =
      sizeof(T) == sizeof(uint64_t) && std::is_integral_v<T>;
  static constexpr bool is_double =
      sizeof(T) == sizeof(double) && std::is_floating_point_v<T>;

  static_assert(is_uint32 || is_uint64 || is_double);

  const int target_align = 32;
  // Scalar loop to reach desired alignment
  for (;
       index < array_len &&
       (reinterpret_cast<std::uintptr_t>(&(array[index])) % target_align) != 0;
       index++) {
    if (array[index] == search_element) {
      return index;
    }
  }

  // Generating vectorized loop
  if constexpr (is_uint32) {
#define MOVEMASK(x) _mm256_movemask_ps(_mm256_castsi256_ps(x))
#define EXTRACT(x) base::bits::CountTrailingZeros32(x)
    VECTORIZED_LOOP_x86(__m256i, __m256i, _mm256_set1_epi32, _mm256_cmpeq_epi32,
                        MOVEMASK, EXTRACT)
#undef MOVEMASK
#undef EXTRACT
  } else if constexpr (is_uint64) {
#define MOVEMASK(x) _mm256_movemask_pd(_mm256_castsi256_pd(x))
#define EXTRACT(x) base::bits::CountTrailingZeros32(x)
    VECTORIZED_LOOP_x86(__m256i, __m256i, _mm256_set1_epi64x,
                        _mm256_cmpeq_epi64, MOVEMASK, EXTRACT)
#undef MOVEMASK
#undef EXTRACT
  } else if constexpr (is_double) {
#define CMP(a, b) _mm256_cmp_pd(a, b, _CMP_EQ_OQ)
#define EXTRACT(x) base::bits::CountTrailingZeros32(x)
    VECTORIZED_LOOP_x86(__m256d, __m256d, _mm256_set1_pd, CMP,
                        _mm256_movemask_pd, EXTRACT)
#undef CMP
#undef EXTRACT
  }

  // The vectorized loop stops when there are not enough items left in the array
  // to fill a vector register. The slow_search function will take care of
  // iterating through the few remaining items.
  return slow_search(array, array_len, index, search_element);
}

#undef TARGET_AVX2
#elif defined(IS_CLANG_WIN)
template <typename T>
inline uintptr_t fast_search_avx(T* array, uintptr_t array_len, uintptr_t index,
                                 T search_element) {
  // Falling back to SSE version
  return fast_search_noavx(array, array_len, index, search_element);
}
#else
template <typename T>
uintptr_t fast_search_avx(T* array, uintptr_t array_len, uintptr_t index,
                          T search_element) {
  UNREACHABLE();
}
#endif  // ifdef __SSE3__

#undef IS_CLANG_WIN
#undef VECTORIZED_LOOP_Neon
#undef VECTORIZED_LOOP_x86

template <typename T>
inline uintptr_t search(T* array, uintptr_t array_len, uintptr_t index,
                        T search_element) {
  if (get_vectorization_kind() == SimdKinds::kAVX2) {
    return fast_search_avx(array, array_len, index, search_element);
  } else {
    return fast_search_noavx(array, array_len, index, search_element);
  }
}

enum class ArrayIndexOfIncludesKind { DOUBLE, OBJECTORSMI };

// ArrayIndexOfIncludes only handles cases that can be efficiently
// vectorized:
//
//   * Searching for a Smi in a Smi array
//
//   * Searching for a Smi or Double in a Double array
//
//   * Searching for an object in an object array.
//
// Other cases should be dealt with either with the CSA builtin or with the
// inlined optimized code.
template <ArrayIndexOfIncludesKind kind>
Address ArrayIndexOfIncludes(Address array_start, uintptr_t array_len,
                             uintptr_t from_index, Address search_element) {
  if (array_len == 0) {
    return Smi::FromInt(-1).ptr();
  }

  if constexpr (kind == ArrayIndexOfIncludesKind::DOUBLE) {
    Tagged<FixedDoubleArray> fixed_array =
        Cast<FixedDoubleArray>(Tagged<Object>(array_start));
    UnalignedDoubleMember* unaligned_array = fixed_array->begin();
    // TODO(leszeks): This reinterpret cast is a bit sketchy because the values
    // are unaligned doubles. Ideally we'd fix the search method to support
    // UnalignedDoubleMember.
    static_assert(sizeof(UnalignedDoubleMember) == sizeof(double));
    double* array = reinterpret_cast<double*>(unaligned_array);

    double search_num;
    if (IsSmi(Tagged<Object>(search_element))) {
      search_num = Tagged<Object>(search_element).ToSmi().value();
    } else {
      DCHECK(IsHeapNumber(Tagged<Object>(search_element)));
      search_num = Cast<HeapNumber>(Tagged<Object>(search_element))->value();
    }

    DCHECK(!std::isnan(search_num));

    if (reinterpret_cast<uintptr_t>(array) % sizeof(double) != 0) {
      // Slow scalar search for unaligned double array.
      for (; from_index < array_len; from_index++) {
        if (fixed_array->is_the_hole(static_cast<int>(from_index))) {
          // |search_num| cannot be NaN, so there is no need to check against
          // holes.
          continue;
        }
        if (fixed_array->get_scalar(static_cast<int>(from_index)) ==
            search_num) {
          return from_index;
        }
      }
      return Smi::FromInt(-1).ptr();
    }

    return search<double>(array, array_len, from_index, search_num);
  }

  if constexpr (kind == ArrayIndexOfIncludesKind::OBJECTORSMI) {
    Tagged<FixedArray> fixed_array =
        Cast<FixedArray>(Tagged<Object>(array_start));
    Tagged_t* array = static_cast<Tagged_t*>(
        fixed_array->RawFieldOfFirstElement().ToVoidPtr());

    DCHECK(!IsHeapNumber(Tagged<Object>(search_element)));
    DCHECK(!IsBigInt(Tagged<Object>(search_element)));
    DCHECK(!IsString(Tagged<Object>(search_element)));

    return search<Tagged_t>(array, array_len, from_index,
                            static_cast<Tagged_t>(search_element));
  }
}

}  // namespace

uintptr_t ArrayIndexOfIncludesSmiOrObject(Address array_start,
                                          uintptr_t array_len,
                                          uintptr_t from_index,
                                          Address search_element) {
  return ArrayIndexOfIncludes<ArrayIndexOfIncludesKind::OBJECTORSMI>(
      array_start, array_len, from_index, search_element);
}

uintptr_t ArrayIndexOfIncludesDouble(Address array_start, uintptr_t array_len,
                                     uintptr_t from_index,
                                     Address search_element) {
  return ArrayIndexOfIncludes<ArrayIndexOfIncludesKind::DOUBLE>(
      array_start, array_len, from_index, search_element);
}

// http://0x80.pl/notesen/2014-09-21-convert-to-hex.html
namespace {

char NibbleToHex(uint8_t nibble) {
  const char correction = 'a' - '0' - 10;
  const char c = nibble + '0';
  uint8_t temp = 128 - 10 + nibble;
  uint8_t msb = temp & 0x80;
  uint8_t mask = msb - (msb >> 7);
  return c + (mask & correction);
}

void Uint8ArrayToHexSlow(const char* bytes, size_t length,
                         DirectHandle<SeqOneByteString> string_output) {
  int index = 0;
  for (size_t i = 0; i < length; i++) {
    uint8_t byte = bytes[i];
    uint8_t high = byte >> 4;
    uint8_t low = byte & 0x0F;

    string_output->SeqOneByteStringSet(index++, NibbleToHex(high));
    string_output->SeqOneByteStringSet(index++, NibbleToHex(low));
  }
}

inline uint16_t ByteToHex(uint8_t byte) {
  const uint16_t correction = (('a' - '0' - 10) << 8) + ('a' - '0' - 10);
#if V8_TARGET_BIG_ENDIAN
  const uint16_t nibbles = (byte << 4) + (byte & 0xF);
#else
  const uint16_t nibbles = ((byte & 0xF) << 8) + (byte >> 4);
#endif
  const uint16_t chars = nibbles + 0x3030;
  const uint16_t temp = 0x8080 - 0x0A0A + nibbles;
  const uint16_t msb = temp & 0x8080;
  const uint16_t mask = msb - (msb >> 7);
  return chars + (mask & correction);
}

V8_ALLOW_UNUSED void HandleRemainingNibbles(const char* bytes, uint8_t* output,
                                            size_t length, size_t i) {
  uint16_t* output_pairs = reinterpret_cast<uint16_t*>(output) + i;
  bytes += i;
  size_t rest = length & 0x7;
  for (i = 0; i < rest; i++) {
    *(output_pairs++) = ByteToHex(*bytes++);
  }
}

/**
The following procedure converts 16 nibbles at a time:

uint8_t nine[9]         = packed_byte(9);
uint8_t ascii0[9]       = packed_byte('0');
uint8_t correction[9]   = packed_byte('a' - 10 - '0');

// assembler
movdqu    nibbles_x_16, %xmm0
movdqa    %xmm0, %xmm1

// convert to ASCII
paddb     ascii0, %xmm1

// make mask
pcmpgtb   nine, %xmm0

// correct result
pand      correction, %xmm0
paddb     %xmm1, %xmm0

// save result...
*/

#ifdef __SSE3__
void Uint8ArrayToHexFastWithSSE(const char* bytes, uint8_t* output,
                                size_t length) {
  size_t i;
  size_t index = 0;
  alignas(16) uint8_t nibbles_buffer[16];
  for (i = 0; i + 8 <= length; i += 8) {
    index = 0;
    for (size_t j = i; j < i + 8; j++) {
      nibbles_buffer[index++] = bytes[j] >> 4;    // High nibble
      nibbles_buffer[index++] = bytes[j] & 0x0F;  // Low nibble
    }

    // Load data into SSE registers
    __m128i nibbles =
        _mm_load_si128(reinterpret_cast<__m128i*>(nibbles_buffer));
    __m128i nine = _mm_set1_epi8(9);
    __m128i ascii_0 = _mm_set1_epi8('0');
    __m128i correction = _mm_set1_epi8('a' - 10 - '0');

    // Make a copy for ASCII conversion
    __m128i ascii_result = _mm_add_epi8(nibbles, ascii_0);

    // Create a mask for values greater than 9
    __m128i mask = _mm_cmpgt_epi8(nibbles, nine);

    // Apply correction
    __m128i corrected_result = _mm_and_si128(mask, correction);
    corrected_result = _mm_add_epi8(ascii_result, corrected_result);

    // Store the result
    _mm_storeu_si128(reinterpret_cast<__m128i*>(&output[i * 2]),
                     corrected_result);
  }

  HandleRemainingNibbles(bytes, output, length, i);
}
#endif

#ifdef NEON64
void Uint8ArrayToHexFastWithNeon(const char* bytes, uint8_t* output,
                                 size_t length) {
  size_t i;
  size_t index = 0;
  alignas(16) uint8_t nibbles_buffer[16];
  for (i = 0; i + 8 <= length; i += 8) {
    index = 0;
    for (size_t j = i; j < i + 8; j++) {
      nibbles_buffer[index++] = bytes[j] >> 4;    // High nibble
      nibbles_buffer[index++] = bytes[j] & 0x0F;  // Low nibble
    }

    // Load data into NEON registers
    uint8x16_t nibbles = vld1q_u8(nibbles_buffer);
    uint8x16_t nine = vdupq_n_u8(9);
    uint8x16_t ascii0 = vdupq_n_u8('0');
    uint8x16_t correction = vdupq_n_u8('a' - 10 - '0');

    // Make a copy for ASCII conversion
    uint8x16_t ascii_result = vaddq_u8(nibbles, ascii0);

    // Create a mask for values greater than 9
    uint8x16_t mask = vcgtq_u8(nibbles, nine);

    // Apply correction
    uint8x16_t corrected_result = vandq_u8(mask, correction);
    corrected_result = vaddq_u8(ascii_result, corrected_result);

    // Store the result
    vst1q_u8(&output[i * 2], corrected_result);
  }

  HandleRemainingNibbles(bytes, output, length, i);
}
#endif
}  // namespace

Tagged<Object> Uint8ArrayToHex(const char* bytes, size_t length,
                               DirectHandle<SeqOneByteString> string_output) {
#ifdef __SSE3__
  if (get_vectorization_kind() == SimdKinds::kAVX2 ||
      get_vectorization_kind() == SimdKinds::kSSE) {
    {
      DisallowGarbageCollection no_gc;
      Uint8ArrayToHexFastWithSSE(bytes, string_output->GetChars(no_gc), length);
    }
    return *string_output;
  }
#endif

#ifdef NEON64
  if (get_vectorization_kind() == SimdKinds::kNeon) {
    {
      DisallowGarbageCollection no_gc;
      Uint8ArrayToHexFastWithNeon(bytes, string_output->GetChars(no_gc),
                                  length);
    }
    return *string_output;
  }
#endif

  Uint8ArrayToHexSlow(bytes, length, string_output);
  return *string_output;
}

namespace {

Maybe<uint8_t> HexToUint8(base::uc16 hex) {
  if (hex >= '0' && hex <= '9') {
    return Just<uint8_t>(hex - '0');
  } else if (hex >= 'a' && hex <= 'f') {
    return Just<uint8_t>(hex - 'a' + 10);
  } else if (hex >= 'A' && hex <= 'F') {
    return Just<uint8_t>(hex - 'A' + 10);
  }

  return Nothing<uint8_t>();
}

#ifdef __SSE3__
const __m128i all_11 = _mm_set1_epi8(11);
__m128i char_0 = _mm_set1_epi8('0');

inline __m128i HexToUint8FastWithSSE(__m128i nibbles) {
  // Example:
  // nibbles: {0x36, 0x66, 0x66, 0x32, 0x31, 0x32, 0x31, 0x32, 0x36, 0x66, 0x66,
  // 0x32, 0x31, 0x32, 0x31, 0x66}

  static const __m128i char_a = _mm_set1_epi8('a');
  static const __m128i char_A = _mm_set1_epi8('A');
  static const __m128i all_10 = _mm_set1_epi8(10);
  static const __m128i all_6 = _mm_set1_epi8(6);

  // Create masks and nibbles for different character ranges
  // Valid hexadecimal values are 0-9, a-f and A-F.
  // mask_09 is 0xff when the corresponding value in nibbles is in range
  // of 0 to 9. nibbles_09 is value-'0' and 0x0 for the rest of the values.
  // Similar description apply to mask_af, mask_AF, nibbles_af and nibbles_af.

  // mask_09: {0xff, 0x0, 0x0, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x0, 0x0,
  // 0xff, 0xff, 0xff, 0xff, 0x0}
  // nibbles_09: {0x6, 0x0, 0x0, 0x2, 0x1, 0x2, 0x1, 0x2, 0x6, 0x0,
  // 0x0, 0x2, 0x1, 0x2, 0x1, 0x0}
  __m128i nibbles_09 = _mm_sub_epi8(nibbles, char_0);
  // If the value is in the expected range (for 09 set is between 0-9), then it
  // will be less than specified max (in this case 10) and the result for this
  // corresponding value is 0xff. For the rest of the values, it will never be
  // less than itself (max in that case) and the result is 0x0.
  __m128i mask_09 =
      _mm_cmplt_epi8(nibbles_09, _mm_max_epu8(nibbles_09, all_10));
  nibbles_09 = _mm_and_si128(nibbles_09, mask_09);

  // mask_af: {0x0, 0xff, 0xff, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0xff, 0xff, 0x0,
  // 0x0, 0x0, 0x0, 0xff}
  // nibbles_af: {0x0, 0xf, 0xf, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0xf, 0xf, 0x0,
  // 0x0, 0x0, 0x0, 0xf}
  __m128i nibbles_af = _mm_sub_epi8(nibbles, char_a);
  __m128i mask_af = _mm_cmplt_epi8(nibbles_af, _mm_max_epu8(nibbles_af, all_6));
  nibbles_af = _mm_and_si128(_mm_add_epi8(nibbles_af, all_10), mask_af);

  // mask_AF: {0x0 <repeats 16 times>}
  __m128i nibbles_AF = _mm_sub_epi8(nibbles, char_A);
  __m128i mask_AF = _mm_cmplt_epi8(nibbles_AF, _mm_max_epu8(nibbles_AF, all_6));
  nibbles_AF = _mm_and_si128(_mm_add_epi8(nibbles_AF, all_10), mask_AF);

  // Combine masks to check if all nibbles are valid hex values
  // combined_mask: {0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  //                 0xff, 0xff, 0xff, 0xff, 0xff, 0xff}
  __m128i combined_mask = _mm_or_si128(_mm_or_si128(mask_af, mask_AF), mask_09);

  if (_mm_movemask_epi8(_mm_cmpeq_epi8(
          combined_mask, _mm_set1_epi64x(0xffffffffffffffff))) != 0xFFFF) {
    return all_11;
  }

  // Combine the results using bitwise OR
  // returns {0x0, 0x6, 0x0, 0xf, 0x0, 0xf, 0x0, 0x2, 0x0, 0x1,
  //                0x0, 0x2, 0x0, 0x1, 0x0, 0x2}
  return _mm_or_si128(_mm_or_si128(nibbles_af, nibbles_AF), nibbles_09);
}

template <typename T>
bool Uint8ArrayFromHexWithSSE(base::Vector<T> input_vector,
                              DirectHandle<JSArrayBuffer> buffer,
                              size_t output_length) {
  CHECK_EQ(buffer->GetByteLength(), output_length);
  // Example:
  // input_vector: 666f6f6261726172666f6f62617261ff
  size_t i;

  for (i = 0; i + 32 <= output_length * 2; i += 32) {
    // Load first batch of 16 hex characters into an SSE register
    // {0x36, 0x36, 0x36, 0x66, 0x36, 0x66, 0x36, 0x32, 0x36, 0x31,
    //         0x37, 0x32, 0x36, 0x31, 0x37, 0x32}
    __m128i first_patch =
        _mm_loadu_si128(reinterpret_cast<const __m128i*>(&input_vector[i]));
    // Handle TwoByteStrings
    if constexpr (std::is_same_v<T, const base::uc16>) {
      __m128i second_part_first_patch = _mm_loadu_si128(
          reinterpret_cast<const __m128i*>(&input_vector[i + 8]));

      first_patch = _mm_packus_epi16(first_patch, second_part_first_patch);
    }

    // Load second batch of 16 hex characters into an SSE register
    // {0x36, 0x36, 0x36, 0x66, 0x36, 0x66, 0x36, 0x32, 0x36, 0x31, 0x37, 0x32,
    // 0x36, 0x31, 0x66, 0x66}
    __m128i second_patch = _mm_loadu_si128(
        reinterpret_cast<const __m128i*>(&input_vector[i + 16]));
    if constexpr (std::is_same_v<T, const base::uc16>) {
      __m128i second_part_second_patch = _mm_loadu_si128(
          reinterpret_cast<const __m128i*>(&input_vector[i + 24]));

      second_patch = _mm_packus_epi16(second_patch, second_part_second_patch);
    }

    __m128i mask = _mm_set1_epi64((__m64)0x00ff00ff00ff00ff);

    // {0x36, 0x0, 0x66, 0x0, 0x66, 0x0, 0x32, 0x0, 0x31, 0x0,
    //             0x32, 0x0, 0x31, 0x0, 0x32, 0x0}
    __m128i first_batch_lo_nibbles = _mm_srli_epi16(first_patch, 8);

    // {0x36, 0x0, 0x36, 0x0, 0x36, 0x0, 0x36, 0x0, 0x36, 0x0,
    //              0x37, 0x0, 0x36, 0x0, 0x37, 0x0}
    __m128i first_batch_hi_nibbles = _mm_and_si128(first_patch, mask);

    // {0x36, 0x0, 0x66, 0x0, 0x66, 0x0, 0x32, 0x0, 0x31, 0x0,
    //             0x32, 0x0, 0x31, 0x0, 0x66, 0x0}
    __m128i second_batch_lo_nibbles = _mm_srli_epi16(second_patch, 8);

    // {0x36, 0x0, 0x36, 0x0, 0x36, 0x0, 0x36, 0x0, 0x36, 0x0,
    //              0x37, 0x0, 0x36, 0x0, 0x66, 0x0}
    __m128i second_batch_hi_nibbles = _mm_and_si128(second_patch, mask);

    // Append first_batch_lo_nibbles and second_batch_lo_nibbles and
    // remove 0x0 values
    // {0x36, 0x66, 0x66, 0x32, 0x31, 0x32, 0x31, 0x32, 0x36, 0x66, 0x66, 0x32,
    // 0x31, 0x32, 0x31, 0x66}
    __m128i lo_nibbles =
        _mm_packus_epi16(first_batch_lo_nibbles, second_batch_lo_nibbles);

    // Append first_batch_hi_nibbles and second_batch_hi_nibbles and
    // remove 0x0 values
    // {0x36, 0x36, 0x36, 0x36, 0x36, 0x37, 0x36, 0x37, 0x36, 0x36, 0x36, 0x36,
    // 0x36, 0x37, 0x36, 0x66}
    __m128i hi_nibbles =
        _mm_packus_epi16(first_batch_hi_nibbles, second_batch_hi_nibbles);

    // {0x6, 0xf, 0xf, 0x2, 0x1, 0x2, 0x1, 0x2, 0x6, 0xf, 0xf, 0x2, 0x1, 0x2,
    // 0x1, 0xf}
    __m128i uint8_low_nibbles = HexToUint8FastWithSSE(lo_nibbles);
    // If any of the values was invalid hex value, we got all_11 vector as a
    // result.
    if (_mm_movemask_epi8(_mm_cmpeq_epi8(uint8_low_nibbles, all_11)) ==
        0xFFFF) {
      return false;
    }

    // {0x6, 0x6, 0x6, 0x6, 0x6, 0x7, 0x6, 0x7, 0x6, 0x6, 0x6, 0x6, 0x6, 0x7,
    // 0x6, 0xf}
    __m128i uint8_high_nibbles = HexToUint8FastWithSSE(hi_nibbles);
    // If any of the values was invalid hex value, we got all_11 vector as a
    // result.
    if (_mm_movemask_epi8(_mm_cmpeq_epi8(uint8_high_nibbles, all_11)) ==
        0xFFFF) {
      return false;
    }

    // {0x60, 0x60, 0x60, 0x60, 0x60, 0x70, 0x60, 0x70, 0x60, 0x60, 0x60, 0x60,
    // 0x60, 0x70, 0x60, 0xf0}
    __m128i uint8_shifted_high_nibbles = _mm_slli_epi64(uint8_high_nibbles, 4);

    // {0x66, 0x6f, 0x6f, 0x62, 0x61, 0x72, 0x61, 0x72, 0x66, 0x6f, 0x6f, 0x62,
    // 0x61, 0x72, 0x61, 0xff}
    __m128i final_result =
        _mm_or_si128(uint8_shifted_high_nibbles, uint8_low_nibbles);

    // store result in a buffer and it is equivalent to
    // [102,111,111,98,97,114,97,114,102,111,111,98,97,114,97,255]
    _mm_storeu_si128(reinterpret_cast<__m128i*>(&(static_cast<uint8_t*>(
                         buffer->backing_store())[i / 2])),
                     final_result);
  }

  // Handle remaining values
  for (size_t j = i; j < output_length * 2; j += 2) {
    T higher = input_vector[j];
    T lower = input_vector[j + 1];

    uint8_t result = 0;
    Maybe<uint8_t> maybe_result = HexToUint8(higher);
    if (!maybe_result.To(&result)) {
      return false;
    }

    uint8_t result_low = 0;
    Maybe<uint8_t> maybe_result_low = HexToUint8(lower);
    if (!maybe_result_low.To(&result_low)) {
      return false;
    }

    result <<= 4;
    result += result_low;
    static_cast<uint8_t*>(buffer->backing_store())[j / 2] = result;
  }

  return true;
}
#endif

}  // namespace

template <typename T>
bool ArrayBufferFromHex(base::Vector<T> input_vector,
                        DirectHandle<JSArrayBuffer> buffer,
                        size_t output_length) {
  size_t input_length = input_vector.size();
  DCHECK_EQ(output_length, input_length / 2);
#ifdef __SSE3__
  if (get_vectorization_kind() == SimdKinds::kAVX2 ||
      get_vectorization_kind() == SimdKinds::kSSE) {
    return Uint8ArrayFromHexWithSSE(input_vector, buffer, output_length);
  }
#endif

  size_t index = 0;
  for (uint32_t i = 0; i < input_length; i += 2) {
    T higher = input_vector[i];
    T lower = input_vector[i + 1];

    uint8_t result = 0;
    Maybe<uint8_t> maybe_result = HexToUint8(higher);
    if (!maybe_result.To(&result)) {
      return false;
    }

    uint8_t result_low = 0;
    Maybe<uint8_t> maybe_result_low = HexToUint8(lower);
    if (!maybe_result_low.To(&result_low)) {
      return false;
    }

    result <<= 4;
    result += result_low;
    reinterpret_cast<uint8_t*>(buffer->backing_store())[index++] = result;
  }
  return true;
}

template bool ArrayBufferFromHex(base::Vector<const uint8_t> input_vector,
                                 DirectHandle<JSArrayBuffer> buffer,
                                 size_t output_length);
template bool ArrayBufferFromHex(base::Vector<const base::uc16> input_vector,
                                 DirectHandle<JSArrayBuffer> buffer,
                                 size_t output_length);

#ifdef NEON64
#undef NEON64
#endif

}  // namespace internal
}  // namespace v8
