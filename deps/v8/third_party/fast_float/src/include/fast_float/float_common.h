#ifndef FASTFLOAT_FLOAT_COMMON_H
#define FASTFLOAT_FLOAT_COMMON_H

#include <cfloat>
#include <cstddef>
#include <cstdint>
#include <cassert>
#include <cstring>
#include <limits>
#include <type_traits>
#include <system_error>
#ifdef __has_include
#if __has_include(<stdfloat>) && (__cplusplus > 202002L || (defined(_MSVC_LANG) && (_MSVC_LANG > 202002L)))
#include <stdfloat>
#endif
#endif
#include "constexpr_feature_detect.h"

#define FASTFLOAT_VERSION_MAJOR 8
#define FASTFLOAT_VERSION_MINOR 2
#define FASTFLOAT_VERSION_PATCH 5

#define FASTFLOAT_STRINGIZE_IMPL(x) #x
#define FASTFLOAT_STRINGIZE(x) FASTFLOAT_STRINGIZE_IMPL(x)

#define FASTFLOAT_VERSION_STR                                                  \
  FASTFLOAT_STRINGIZE(FASTFLOAT_VERSION_MAJOR)                                 \
  "." FASTFLOAT_STRINGIZE(FASTFLOAT_VERSION_MINOR) "." FASTFLOAT_STRINGIZE(    \
      FASTFLOAT_VERSION_PATCH)

#define FASTFLOAT_VERSION                                                      \
  (FASTFLOAT_VERSION_MAJOR * 10000 + FASTFLOAT_VERSION_MINOR * 100 +           \
   FASTFLOAT_VERSION_PATCH)

namespace fast_float {

enum class chars_format : uint64_t;

namespace detail {
constexpr chars_format basic_json_fmt = chars_format(1 << 5);
constexpr chars_format basic_fortran_fmt = chars_format(1 << 6);
} // namespace detail

enum class chars_format : uint64_t {
  scientific = 1 << 0,
  fixed = 1 << 2,
  hex = 1 << 3,
  no_infnan = 1 << 4,
  // RFC 8259: https://datatracker.ietf.org/doc/html/rfc8259#section-6
  json = uint64_t(detail::basic_json_fmt) | fixed | scientific | no_infnan,
  // Extension of RFC 8259 where, e.g., "inf" and "nan" are allowed.
  json_or_infnan = uint64_t(detail::basic_json_fmt) | fixed | scientific,
  fortran = uint64_t(detail::basic_fortran_fmt) | fixed | scientific,
  general = fixed | scientific,
  allow_leading_plus = 1 << 7,
  skip_white_space = 1 << 8,
};

template <typename UC> struct from_chars_result_t {
  UC const *ptr;
  std::errc ec;

  // https://www.open-std.org/jtc1/sc22/wg21/docs/papers/2023/p2497r0.html
  constexpr explicit operator bool() const noexcept {
    return ec == std::errc();
  }
};

using from_chars_result = from_chars_result_t<char>;

template <typename UC> struct parse_options_t {
  constexpr explicit parse_options_t(chars_format fmt = chars_format::general,
                                     UC dot = UC('.'), int b = 10)
      : format(fmt), decimal_point(dot), base(b) {}

  /** Which number formats are accepted */
  chars_format format;
  /** The character used as decimal point */
  UC decimal_point;
  /** The base used for integers */
  int base;
};

using parse_options = parse_options_t<char>;

} // namespace fast_float

#if FASTFLOAT_HAS_BIT_CAST
#include <bit>
#endif

#if (defined(__x86_64) || defined(__x86_64__) || defined(_M_X64) ||            \
     defined(__amd64) || defined(__aarch64__) || defined(_M_ARM64) ||          \
     defined(__MINGW64__) || defined(__s390x__) ||                             \
     (defined(__ppc64__) || defined(__PPC64__) || defined(__ppc64le__) ||      \
      defined(__PPC64LE__)) ||                                                 \
     defined(__loongarch64) || (defined(__riscv) && __riscv_xlen == 64))
#define FASTFLOAT_64BIT 1
#elif (defined(__i386) || defined(__i386__) || defined(_M_IX86) ||             \
       defined(__arm__) || defined(_M_ARM) || defined(__ppc__) ||              \
       defined(__MINGW32__) || defined(__EMSCRIPTEN__) ||                      \
       (defined(__riscv) && __riscv_xlen == 32))
#define FASTFLOAT_32BIT 1
#else
  // Need to check incrementally, since SIZE_MAX is a size_t, avoid overflow.
// We can never tell the register width, but the SIZE_MAX is a good
// approximation. UINTPTR_MAX and INTPTR_MAX are optional, so avoid them for max
// portability.
#if SIZE_MAX == 0xffff
#error Unknown platform (16-bit, unsupported)
#elif SIZE_MAX == 0xffffffff
#define FASTFLOAT_32BIT 1
#elif SIZE_MAX == 0xffffffffffffffff
#define FASTFLOAT_64BIT 1
#else
#error Unknown platform (not 32-bit, not 64-bit?)
#endif
#endif

#if ((defined(_WIN32) || defined(_WIN64)) && !defined(__clang__)) ||           \
    (defined(_M_ARM64) && !defined(__MINGW32__))
#include <intrin.h>
#endif

#if defined(_MSC_VER) && !defined(__clang__)
#define FASTFLOAT_VISUAL_STUDIO 1
#endif

#if defined __BYTE_ORDER__ && defined __ORDER_BIG_ENDIAN__
#define FASTFLOAT_IS_BIG_ENDIAN (__BYTE_ORDER__ == __ORDER_BIG_ENDIAN__)
#elif defined _WIN32
#define FASTFLOAT_IS_BIG_ENDIAN 0
#else
#if defined(__APPLE__) || defined(__FreeBSD__)
#include <machine/endian.h>
#elif defined(sun) || defined(__sun)
#include <sys/byteorder.h>
#elif defined(__MVS__)
#include <sys/endian.h>
#else
#ifdef __has_include
#if __has_include(<endian.h>)
#include <endian.h>
#endif //__has_include(<endian.h>)
#endif //__has_include
#endif
#
#ifndef __BYTE_ORDER__
// safe choice
#define FASTFLOAT_IS_BIG_ENDIAN 0
#endif
#
#ifndef __ORDER_LITTLE_ENDIAN__
// safe choice
#define FASTFLOAT_IS_BIG_ENDIAN 0
#endif
#
#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
#define FASTFLOAT_IS_BIG_ENDIAN 0
#else
#define FASTFLOAT_IS_BIG_ENDIAN 1
#endif
#endif

#if defined(__SSE2__) || (defined(FASTFLOAT_VISUAL_STUDIO) &&                  \
                          (defined(_M_AMD64) || defined(_M_X64) ||             \
                           (defined(_M_IX86_FP) && _M_IX86_FP == 2)))
#define FASTFLOAT_SSE2 1
#endif

#if defined(__aarch64__) || defined(_M_ARM64)
#define FASTFLOAT_NEON 1
#endif

#if defined(FASTFLOAT_SSE2) || defined(FASTFLOAT_NEON)
#define FASTFLOAT_HAS_SIMD 1
#endif

#if defined(__GNUC__)
// disable -Wcast-align=strict (GCC only)
#define FASTFLOAT_SIMD_DISABLE_WARNINGS                                        \
  _Pragma("GCC diagnostic push")                                               \
      _Pragma("GCC diagnostic ignored \"-Wcast-align\"")
#else
#define FASTFLOAT_SIMD_DISABLE_WARNINGS
#endif

#if defined(__GNUC__)
#define FASTFLOAT_SIMD_RESTORE_WARNINGS _Pragma("GCC diagnostic pop")
#else
#define FASTFLOAT_SIMD_RESTORE_WARNINGS
#endif

#ifdef FASTFLOAT_VISUAL_STUDIO
#define fastfloat_really_inline __forceinline
#else
#define fastfloat_really_inline inline __attribute__((always_inline))
#endif

#ifndef FASTFLOAT_ASSERT
#define FASTFLOAT_ASSERT(x)                                                    \
  { ((void)(x)); }
#endif

#ifndef FASTFLOAT_DEBUG_ASSERT
#define FASTFLOAT_DEBUG_ASSERT(x)                                              \
  { ((void)(x)); }
#endif

// rust style `try!()` macro, or `?` operator
#define FASTFLOAT_TRY(x)                                                       \
  {                                                                            \
    if (!(x))                                                                  \
      return false;                                                            \
  }

#define FASTFLOAT_ENABLE_IF(...)                                               \
  typename std::enable_if<(__VA_ARGS__), int>::type

namespace fast_float {

fastfloat_really_inline constexpr bool cpp20_and_in_constexpr() {
#if FASTFLOAT_HAS_IS_CONSTANT_EVALUATED
  return std::is_constant_evaluated();
#else
  return false;
#endif
}

template <typename T>
struct is_supported_float_type
    : std::integral_constant<
          bool, std::is_same<T, double>::value || std::is_same<T, float>::value
#ifdef __STDCPP_FLOAT64_T__
                    || std::is_same<T, std::float64_t>::value
#endif
#ifdef __STDCPP_FLOAT32_T__
                    || std::is_same<T, std::float32_t>::value
#endif
#ifdef __STDCPP_FLOAT16_T__
                    || std::is_same<T, std::float16_t>::value
#endif
#ifdef __STDCPP_BFLOAT16_T__
                    || std::is_same<T, std::bfloat16_t>::value
#endif
          > {
};

template <typename T>
using equiv_uint_t = typename std::conditional<
    sizeof(T) == 1, uint8_t,
    typename std::conditional<
        sizeof(T) == 2, uint16_t,
        typename std::conditional<sizeof(T) == 4, uint32_t,
                                  uint64_t>::type>::type>::type;

template <typename T> struct is_supported_integer_type : std::is_integral<T> {};

template <typename UC>
struct is_supported_char_type
    : std::integral_constant<bool, std::is_same<UC, char>::value ||
                                       std::is_same<UC, wchar_t>::value ||
                                       std::is_same<UC, char16_t>::value ||
                                       std::is_same<UC, char32_t>::value
#ifdef __cpp_char8_t
                                       || std::is_same<UC, char8_t>::value
#endif
                             > {
};

template <typename UC>
inline FASTFLOAT_CONSTEXPR14 bool
fastfloat_strncasecmp3(UC const *actual_mixedcase,
                       UC const *expected_lowercase) {
  uint64_t mask{0};
  FASTFLOAT_IF_CONSTEXPR17(sizeof(UC) == 1) { mask = 0x2020202020202020; }
  else FASTFLOAT_IF_CONSTEXPR17(sizeof(UC) == 2) {
    mask = 0x0020002000200020;
  }
  else FASTFLOAT_IF_CONSTEXPR17(sizeof(UC) == 4) {
    mask = 0x0000002000000020;
  }
  else {
    return false;
  }

  uint64_t val1{0}, val2{0};
  if (cpp20_and_in_constexpr()) {
    for (size_t i = 0; i < 3; i++) {
      if ((actual_mixedcase[i] | 32) != expected_lowercase[i]) {
        return false;
      }
    }
    return true;
  } else {
    FASTFLOAT_IF_CONSTEXPR17(sizeof(UC) == 1 || sizeof(UC) == 2) {
      ::memcpy(&val1, actual_mixedcase, 3 * sizeof(UC));
      ::memcpy(&val2, expected_lowercase, 3 * sizeof(UC));
      val1 |= mask;
      val2 |= mask;
      return val1 == val2;
    }
    else FASTFLOAT_IF_CONSTEXPR17(sizeof(UC) == 4) {
      ::memcpy(&val1, actual_mixedcase, 2 * sizeof(UC));
      ::memcpy(&val2, expected_lowercase, 2 * sizeof(UC));
      val1 |= mask;
      if (val1 != val2) {
        return false;
      }
      return (actual_mixedcase[2] | 32) == (expected_lowercase[2]);
    }
    else {
      return false;
    }
  }
}

template <typename UC>
inline FASTFLOAT_CONSTEXPR14 bool
fastfloat_strncasecmp5(UC const *actual_mixedcase,
                       UC const *expected_lowercase) {
  uint64_t mask{0};
  uint64_t val1{0}, val2{0};
  if (cpp20_and_in_constexpr()) {
    for (size_t i = 0; i < 5; i++) {
      if ((actual_mixedcase[i] | 32) != expected_lowercase[i]) {
        return false;
      }
    }
    return true;
  } else {
    FASTFLOAT_IF_CONSTEXPR17(sizeof(UC) == 1) {
      mask = 0x2020202020202020;
      ::memcpy(&val1, actual_mixedcase, 5 * sizeof(UC));
      ::memcpy(&val2, expected_lowercase, 5 * sizeof(UC));
      val1 |= mask;
      val2 |= mask;
      return val1 == val2;
    }
    else FASTFLOAT_IF_CONSTEXPR17(sizeof(UC) == 2) {
      mask = 0x0020002000200020;
      ::memcpy(&val1, actual_mixedcase, 4 * sizeof(UC));
      ::memcpy(&val2, expected_lowercase, 4 * sizeof(UC));
      val1 |= mask;
      if (val1 != val2) {
        return false;
      }
      return (actual_mixedcase[4] | 32) == (expected_lowercase[4]);
    }
    else FASTFLOAT_IF_CONSTEXPR17(sizeof(UC) == 4) {
      mask = 0x0000002000000020;
      ::memcpy(&val1, actual_mixedcase, 2 * sizeof(UC));
      ::memcpy(&val2, expected_lowercase, 2 * sizeof(UC));
      val1 |= mask;
      if (val1 != val2) {
        return false;
      }
      ::memcpy(&val1, actual_mixedcase + 2, 2 * sizeof(UC));
      ::memcpy(&val2, expected_lowercase + 2, 2 * sizeof(UC));
      val1 |= mask;
      if (val1 != val2) {
        return false;
      }
      return (actual_mixedcase[4] | 32) == (expected_lowercase[4]);
    }
    else {
      return false;
    }
  }
}

// Compares two ASCII strings in a case insensitive manner.
template <typename UC>
inline FASTFLOAT_CONSTEXPR14 bool
fastfloat_strncasecmp(UC const *actual_mixedcase, UC const *expected_lowercase,
                      size_t length) {
  uint64_t mask{0};
  FASTFLOAT_IF_CONSTEXPR17(sizeof(UC) == 1) { mask = 0x2020202020202020; }
  else FASTFLOAT_IF_CONSTEXPR17(sizeof(UC) == 2) {
    mask = 0x0020002000200020;
  }
  else FASTFLOAT_IF_CONSTEXPR17(sizeof(UC) == 4) {
    mask = 0x0000002000000020;
  }
  else {
    return false;
  }

  if (cpp20_and_in_constexpr()) {
    for (size_t i = 0; i < length; i++) {
      if ((actual_mixedcase[i] | 32) != expected_lowercase[i]) {
        return false;
      }
    }
    return true;
  } else {
    uint64_t val1{0}, val2{0};
    size_t sz{8 / (sizeof(UC))};
    for (size_t i = 0; i < length; i += sz) {
      val1 = val2 = 0;
      sz = sz < (length - i) ? sz : length - i;
      ::memcpy(&val1, actual_mixedcase + i, sz * sizeof(UC));
      ::memcpy(&val2, expected_lowercase + i, sz * sizeof(UC));
      val1 |= mask;
      val2 |= mask;
      if (val1 != val2) {
        return false;
      }
    }
    return true;
  }
}

#ifndef FLT_EVAL_METHOD
#error "FLT_EVAL_METHOD should be defined, please include cfloat."
#endif

// a pointer and a length to a contiguous block of memory
template <typename T> struct span {
  T const *ptr;
  size_t length;

  constexpr span(T const *_ptr, size_t _length) : ptr(_ptr), length(_length) {}

  constexpr span() : ptr(nullptr), length(0) {}

  constexpr size_t len() const noexcept { return length; }

  FASTFLOAT_CONSTEXPR14 const T &operator[](size_t index) const noexcept {
    FASTFLOAT_DEBUG_ASSERT(index < length);
    return ptr[index];
  }
};

struct value128 {
  uint64_t low;
  uint64_t high;

  constexpr value128(uint64_t _low, uint64_t _high) : low(_low), high(_high) {}

  constexpr value128() : low(0), high(0) {}
};

/* Helper C++14 constexpr generic implementation of leading_zeroes */
fastfloat_really_inline FASTFLOAT_CONSTEXPR14 int
leading_zeroes_generic(uint64_t input_num, int last_bit = 0) {
  if (input_num & uint64_t(0xffffffff00000000)) {
    input_num >>= 32;
    last_bit |= 32;
  }
  if (input_num & uint64_t(0xffff0000)) {
    input_num >>= 16;
    last_bit |= 16;
  }
  if (input_num & uint64_t(0xff00)) {
    input_num >>= 8;
    last_bit |= 8;
  }
  if (input_num & uint64_t(0xf0)) {
    input_num >>= 4;
    last_bit |= 4;
  }
  if (input_num & uint64_t(0xc)) {
    input_num >>= 2;
    last_bit |= 2;
  }
  if (input_num & uint64_t(0x2)) { /* input_num >>=  1; */
    last_bit |= 1;
  }
  return 63 - last_bit;
}

/* result might be undefined when input_num is zero */
fastfloat_really_inline FASTFLOAT_CONSTEXPR20 int
leading_zeroes(uint64_t input_num) {
  assert(input_num > 0);
  if (cpp20_and_in_constexpr()) {
    return leading_zeroes_generic(input_num);
  }
#ifdef FASTFLOAT_VISUAL_STUDIO
#if defined(_M_X64) || defined(_M_ARM64)
  unsigned long leading_zero = 0;
  // Search the mask data from most significant bit (MSB)
  // to least significant bit (LSB) for a set bit (1).
  _BitScanReverse64(&leading_zero, input_num);
  return (int)(63 - leading_zero);
#else
  return leading_zeroes_generic(input_num);
#endif
#else
  return __builtin_clzll(input_num);
#endif
}

/* Helper C++14 constexpr generic implementation of countr_zero for 32-bit */
fastfloat_really_inline FASTFLOAT_CONSTEXPR14 int
countr_zero_generic_32(uint32_t input_num) {
  if (input_num == 0) {
    return 32;
  }
  int last_bit = 0;
  if (!(input_num & 0x0000FFFF)) {
    input_num >>= 16;
    last_bit |= 16;
  }
  if (!(input_num & 0x00FF)) {
    input_num >>= 8;
    last_bit |= 8;
  }
  if (!(input_num & 0x0F)) {
    input_num >>= 4;
    last_bit |= 4;
  }
  if (!(input_num & 0x3)) {
    input_num >>= 2;
    last_bit |= 2;
  }
  if (!(input_num & 0x1)) {
    last_bit |= 1;
  }
  return last_bit;
}

/* count trailing zeroes for 32-bit integers */
fastfloat_really_inline FASTFLOAT_CONSTEXPR20 int
countr_zero_32(uint32_t input_num) {
  if (cpp20_and_in_constexpr()) {
    return countr_zero_generic_32(input_num);
  }
#ifdef FASTFLOAT_VISUAL_STUDIO
  unsigned long trailing_zero = 0;
  if (_BitScanForward(&trailing_zero, input_num)) {
    return (int)trailing_zero;
  }
  return 32;
#else
  return input_num == 0 ? 32 : __builtin_ctz(input_num);
#endif
}

// slow emulation routine for 32-bit
fastfloat_really_inline constexpr uint64_t emulu(uint32_t x, uint32_t y) {
  return x * (uint64_t)y;
}

fastfloat_really_inline FASTFLOAT_CONSTEXPR14 uint64_t
umul128_generic(uint64_t ab, uint64_t cd, uint64_t *hi) {
  uint64_t ad = emulu((uint32_t)(ab >> 32), (uint32_t)cd);
  uint64_t bd = emulu((uint32_t)ab, (uint32_t)cd);
  uint64_t adbc = ad + emulu((uint32_t)ab, (uint32_t)(cd >> 32));
  uint64_t adbc_carry = (uint64_t)(adbc < ad);
  uint64_t lo = bd + (adbc << 32);
  *hi = emulu((uint32_t)(ab >> 32), (uint32_t)(cd >> 32)) + (adbc >> 32) +
        (adbc_carry << 32) + (uint64_t)(lo < bd);
  return lo;
}

#ifdef FASTFLOAT_32BIT

// slow emulation routine for 32-bit
#if !defined(__MINGW64__)
fastfloat_really_inline FASTFLOAT_CONSTEXPR14 uint64_t _umul128(uint64_t ab,
                                                                uint64_t cd,
                                                                uint64_t *hi) {
  return umul128_generic(ab, cd, hi);
}
#endif // !__MINGW64__

#endif // FASTFLOAT_32BIT

// compute 64-bit a*b
fastfloat_really_inline FASTFLOAT_CONSTEXPR20 value128
full_multiplication(uint64_t a, uint64_t b) {
  if (cpp20_and_in_constexpr()) {
    value128 answer;
    answer.low = umul128_generic(a, b, &answer.high);
    return answer;
  }
  value128 answer;
#if defined(_M_ARM64) && !defined(__MINGW32__)
  // ARM64 has native support for 64-bit multiplications, no need to emulate
  // But MinGW on ARM64 doesn't have native support for 64-bit multiplications
  answer.high = __umulh(a, b);
  answer.low = a * b;
#elif defined(FASTFLOAT_32BIT) || (defined(_WIN64) && !defined(__clang__) &&   \
                                   !defined(_M_ARM64) && !defined(__GNUC__))
  answer.low = _umul128(a, b, &answer.high); // _umul128 not available on ARM64
#elif defined(FASTFLOAT_64BIT) && defined(__SIZEOF_INT128__)
  __uint128_t r = ((__uint128_t)a) * b;
  answer.low = uint64_t(r);
  answer.high = uint64_t(r >> 64);
#else
  answer.low = umul128_generic(a, b, &answer.high);
#endif
  return answer;
}

struct adjusted_mantissa {
  uint64_t mantissa{0};
  int32_t power2{0}; // a negative value indicates an invalid result
  adjusted_mantissa() = default;

  constexpr bool operator==(adjusted_mantissa const &o) const {
    return mantissa == o.mantissa && power2 == o.power2;
  }

  constexpr bool operator!=(adjusted_mantissa const &o) const {
    return mantissa != o.mantissa || power2 != o.power2;
  }
};

// Bias so we can get the real exponent with an invalid adjusted_mantissa.
constexpr static int32_t invalid_am_bias = -0x8000;

// used for binary_format_lookup_tables<T>::max_mantissa
constexpr uint64_t constant_55555 = 5 * 5 * 5 * 5 * 5;

template <typename T, typename U = void> struct binary_format_lookup_tables;

template <typename T> struct binary_format : binary_format_lookup_tables<T> {
  using equiv_uint = equiv_uint_t<T>;

  static constexpr int mantissa_explicit_bits();
  static constexpr int minimum_exponent();
  static constexpr int infinite_power();
  static constexpr int sign_index();
  static constexpr int
  min_exponent_fast_path(); // used when fegetround() == FE_TONEAREST
  static constexpr int max_exponent_fast_path();
  static constexpr int max_exponent_round_to_even();
  static constexpr int min_exponent_round_to_even();
  static constexpr uint64_t max_mantissa_fast_path(int64_t power);
  static constexpr uint64_t
  max_mantissa_fast_path(); // used when fegetround() == FE_TONEAREST
  static constexpr int largest_power_of_ten();
  static constexpr int smallest_power_of_ten();
  static constexpr T exact_power_of_ten(int64_t power);
  static constexpr size_t max_digits();
  static constexpr equiv_uint exponent_mask();
  static constexpr equiv_uint mantissa_mask();
  static constexpr equiv_uint hidden_bit_mask();
};

template <typename U> struct binary_format_lookup_tables<double, U> {
  static constexpr double powers_of_ten[] = {
      1e0,  1e1,  1e2,  1e3,  1e4,  1e5,  1e6,  1e7,  1e8,  1e9,  1e10, 1e11,
      1e12, 1e13, 1e14, 1e15, 1e16, 1e17, 1e18, 1e19, 1e20, 1e21, 1e22};

  // Largest integer value v so that (5**index * v) <= 1<<53.
  // 0x20000000000000 == 1 << 53
  static constexpr uint64_t max_mantissa[] = {
      0x20000000000000,
      0x20000000000000 / 5,
      0x20000000000000 / (5 * 5),
      0x20000000000000 / (5 * 5 * 5),
      0x20000000000000 / (5 * 5 * 5 * 5),
      0x20000000000000 / (constant_55555),
      0x20000000000000 / (constant_55555 * 5),
      0x20000000000000 / (constant_55555 * 5 * 5),
      0x20000000000000 / (constant_55555 * 5 * 5 * 5),
      0x20000000000000 / (constant_55555 * 5 * 5 * 5 * 5),
      0x20000000000000 / (constant_55555 * constant_55555),
      0x20000000000000 / (constant_55555 * constant_55555 * 5),
      0x20000000000000 / (constant_55555 * constant_55555 * 5 * 5),
      0x20000000000000 / (constant_55555 * constant_55555 * 5 * 5 * 5),
      0x20000000000000 / (constant_55555 * constant_55555 * constant_55555),
      0x20000000000000 / (constant_55555 * constant_55555 * constant_55555 * 5),
      0x20000000000000 /
          (constant_55555 * constant_55555 * constant_55555 * 5 * 5),
      0x20000000000000 /
          (constant_55555 * constant_55555 * constant_55555 * 5 * 5 * 5),
      0x20000000000000 /
          (constant_55555 * constant_55555 * constant_55555 * 5 * 5 * 5 * 5),
      0x20000000000000 /
          (constant_55555 * constant_55555 * constant_55555 * constant_55555),
      0x20000000000000 / (constant_55555 * constant_55555 * constant_55555 *
                          constant_55555 * 5),
      0x20000000000000 / (constant_55555 * constant_55555 * constant_55555 *
                          constant_55555 * 5 * 5),
      0x20000000000000 / (constant_55555 * constant_55555 * constant_55555 *
                          constant_55555 * 5 * 5 * 5),
      0x20000000000000 / (constant_55555 * constant_55555 * constant_55555 *
                          constant_55555 * 5 * 5 * 5 * 5)};
};

#if FASTFLOAT_DETAIL_MUST_DEFINE_CONSTEXPR_VARIABLE

template <typename U>
constexpr double binary_format_lookup_tables<double, U>::powers_of_ten[];

template <typename U>
constexpr uint64_t binary_format_lookup_tables<double, U>::max_mantissa[];

#endif

template <typename U> struct binary_format_lookup_tables<float, U> {
  static constexpr float powers_of_ten[] = {1e0f, 1e1f, 1e2f, 1e3f, 1e4f, 1e5f,
                                            1e6f, 1e7f, 1e8f, 1e9f, 1e10f};

  // Largest integer value v so that (5**index * v) <= 1<<24.
  // 0x1000000 == 1<<24
  static constexpr uint64_t max_mantissa[] = {
      0x1000000,
      0x1000000 / 5,
      0x1000000 / (5 * 5),
      0x1000000 / (5 * 5 * 5),
      0x1000000 / (5 * 5 * 5 * 5),
      0x1000000 / (constant_55555),
      0x1000000 / (constant_55555 * 5),
      0x1000000 / (constant_55555 * 5 * 5),
      0x1000000 / (constant_55555 * 5 * 5 * 5),
      0x1000000 / (constant_55555 * 5 * 5 * 5 * 5),
      0x1000000 / (constant_55555 * constant_55555),
      0x1000000 / (constant_55555 * constant_55555 * 5)};
};

#if FASTFLOAT_DETAIL_MUST_DEFINE_CONSTEXPR_VARIABLE

template <typename U>
constexpr float binary_format_lookup_tables<float, U>::powers_of_ten[];

template <typename U>
constexpr uint64_t binary_format_lookup_tables<float, U>::max_mantissa[];

#endif

template <>
inline constexpr int binary_format<double>::min_exponent_fast_path() {
#if (FLT_EVAL_METHOD != 1) && (FLT_EVAL_METHOD != 0)
  return 0;
#else
  return -22;
#endif
}

template <>
inline constexpr int binary_format<float>::min_exponent_fast_path() {
#if (FLT_EVAL_METHOD != 1) && (FLT_EVAL_METHOD != 0)
  return 0;
#else
  return -10;
#endif
}

template <>
inline constexpr int binary_format<double>::mantissa_explicit_bits() {
  return 52;
}

template <>
inline constexpr int binary_format<float>::mantissa_explicit_bits() {
  return 23;
}

template <>
inline constexpr int binary_format<double>::max_exponent_round_to_even() {
  return 23;
}

template <>
inline constexpr int binary_format<float>::max_exponent_round_to_even() {
  return 10;
}

template <>
inline constexpr int binary_format<double>::min_exponent_round_to_even() {
  return -4;
}

template <>
inline constexpr int binary_format<float>::min_exponent_round_to_even() {
  return -17;
}

template <> inline constexpr int binary_format<double>::minimum_exponent() {
  return -1023;
}

template <> inline constexpr int binary_format<float>::minimum_exponent() {
  return -127;
}

template <> inline constexpr int binary_format<double>::infinite_power() {
  return 0x7FF;
}

template <> inline constexpr int binary_format<float>::infinite_power() {
  return 0xFF;
}

template <> inline constexpr int binary_format<double>::sign_index() {
  return 63;
}

template <> inline constexpr int binary_format<float>::sign_index() {
  return 31;
}

template <>
inline constexpr int binary_format<double>::max_exponent_fast_path() {
  return 22;
}

template <>
inline constexpr int binary_format<float>::max_exponent_fast_path() {
  return 10;
}

template <>
inline constexpr uint64_t binary_format<double>::max_mantissa_fast_path() {
  return uint64_t(2) << mantissa_explicit_bits();
}

template <>
inline constexpr uint64_t binary_format<float>::max_mantissa_fast_path() {
  return uint64_t(2) << mantissa_explicit_bits();
}

// credit: Jakub Jelínek
#ifdef __STDCPP_FLOAT16_T__
template <typename U> struct binary_format_lookup_tables<std::float16_t, U> {
  static constexpr std::float16_t powers_of_ten[] = {1e0f16, 1e1f16, 1e2f16,
                                                     1e3f16, 1e4f16};

  // Largest integer value v so that (5**index * v) <= 1<<11.
  // 0x800 == 1<<11
  static constexpr uint64_t max_mantissa[] = {0x800,
                                              0x800 / 5,
                                              0x800 / (5 * 5),
                                              0x800 / (5 * 5 * 5),
                                              0x800 / (5 * 5 * 5 * 5),
                                              0x800 / (constant_55555)};
};

#if FASTFLOAT_DETAIL_MUST_DEFINE_CONSTEXPR_VARIABLE

template <typename U>
constexpr std::float16_t
    binary_format_lookup_tables<std::float16_t, U>::powers_of_ten[];

template <typename U>
constexpr uint64_t
    binary_format_lookup_tables<std::float16_t, U>::max_mantissa[];

#endif

template <>
inline constexpr std::float16_t
binary_format<std::float16_t>::exact_power_of_ten(int64_t power) {
  // Work around clang bug https://godbolt.org/z/zedh7rrhc
  return (void)powers_of_ten[0], powers_of_ten[power];
}

template <>
inline constexpr binary_format<std::float16_t>::equiv_uint
binary_format<std::float16_t>::exponent_mask() {
  return 0x7C00;
}

template <>
inline constexpr binary_format<std::float16_t>::equiv_uint
binary_format<std::float16_t>::mantissa_mask() {
  return 0x03FF;
}

template <>
inline constexpr binary_format<std::float16_t>::equiv_uint
binary_format<std::float16_t>::hidden_bit_mask() {
  return 0x0400;
}

template <>
inline constexpr int binary_format<std::float16_t>::max_exponent_fast_path() {
  return 4;
}

template <>
inline constexpr int binary_format<std::float16_t>::mantissa_explicit_bits() {
  return 10;
}

template <>
inline constexpr uint64_t
binary_format<std::float16_t>::max_mantissa_fast_path() {
  return uint64_t(2) << mantissa_explicit_bits();
}

template <>
inline constexpr uint64_t
binary_format<std::float16_t>::max_mantissa_fast_path(int64_t power) {
  // caller is responsible to ensure that
  // power >= 0 && power <= 4
  //
  // Work around clang bug https://godbolt.org/z/zedh7rrhc
  return (void)max_mantissa[0], max_mantissa[power];
}

template <>
inline constexpr int binary_format<std::float16_t>::min_exponent_fast_path() {
  return 0;
}

template <>
inline constexpr int
binary_format<std::float16_t>::max_exponent_round_to_even() {
  return 5;
}

template <>
inline constexpr int
binary_format<std::float16_t>::min_exponent_round_to_even() {
  return -22;
}

template <>
inline constexpr int binary_format<std::float16_t>::minimum_exponent() {
  return -15;
}

template <>
inline constexpr int binary_format<std::float16_t>::infinite_power() {
  return 0x1F;
}

template <> inline constexpr int binary_format<std::float16_t>::sign_index() {
  return 15;
}

template <>
inline constexpr int binary_format<std::float16_t>::largest_power_of_ten() {
  return 4;
}

template <>
inline constexpr int binary_format<std::float16_t>::smallest_power_of_ten() {
  return -27;
}

template <>
inline constexpr size_t binary_format<std::float16_t>::max_digits() {
  return 22;
}
#endif // __STDCPP_FLOAT16_T__

// credit: Jakub Jelínek
#ifdef __STDCPP_BFLOAT16_T__
template <typename U> struct binary_format_lookup_tables<std::bfloat16_t, U> {
  static constexpr std::bfloat16_t powers_of_ten[] = {1e0bf16, 1e1bf16, 1e2bf16,
                                                      1e3bf16};

  // Largest integer value v so that (5**index * v) <= 1<<8.
  // 0x100 == 1<<8
  static constexpr uint64_t max_mantissa[] = {0x100, 0x100 / 5, 0x100 / (5 * 5),
                                              0x100 / (5 * 5 * 5),
                                              0x100 / (5 * 5 * 5 * 5)};
};

#if FASTFLOAT_DETAIL_MUST_DEFINE_CONSTEXPR_VARIABLE

template <typename U>
constexpr std::bfloat16_t
    binary_format_lookup_tables<std::bfloat16_t, U>::powers_of_ten[];

template <typename U>
constexpr uint64_t
    binary_format_lookup_tables<std::bfloat16_t, U>::max_mantissa[];

#endif

template <>
inline constexpr std::bfloat16_t
binary_format<std::bfloat16_t>::exact_power_of_ten(int64_t power) {
  // Work around clang bug https://godbolt.org/z/zedh7rrhc
  return (void)powers_of_ten[0], powers_of_ten[power];
}

template <>
inline constexpr int binary_format<std::bfloat16_t>::max_exponent_fast_path() {
  return 3;
}

template <>
inline constexpr binary_format<std::bfloat16_t>::equiv_uint
binary_format<std::bfloat16_t>::exponent_mask() {
  return 0x7F80;
}

template <>
inline constexpr binary_format<std::bfloat16_t>::equiv_uint
binary_format<std::bfloat16_t>::mantissa_mask() {
  return 0x007F;
}

template <>
inline constexpr binary_format<std::bfloat16_t>::equiv_uint
binary_format<std::bfloat16_t>::hidden_bit_mask() {
  return 0x0080;
}

template <>
inline constexpr int binary_format<std::bfloat16_t>::mantissa_explicit_bits() {
  return 7;
}

template <>
inline constexpr uint64_t
binary_format<std::bfloat16_t>::max_mantissa_fast_path() {
  return uint64_t(2) << mantissa_explicit_bits();
}

template <>
inline constexpr uint64_t
binary_format<std::bfloat16_t>::max_mantissa_fast_path(int64_t power) {
  // caller is responsible to ensure that
  // power >= 0 && power <= 3
  //
  // Work around clang bug https://godbolt.org/z/zedh7rrhc
  return (void)max_mantissa[0], max_mantissa[power];
}

template <>
inline constexpr int binary_format<std::bfloat16_t>::min_exponent_fast_path() {
  return 0;
}

template <>
inline constexpr int
binary_format<std::bfloat16_t>::max_exponent_round_to_even() {
  return 3;
}

template <>
inline constexpr int
binary_format<std::bfloat16_t>::min_exponent_round_to_even() {
  return -24;
}

template <>
inline constexpr int binary_format<std::bfloat16_t>::minimum_exponent() {
  return -127;
}

template <>
inline constexpr int binary_format<std::bfloat16_t>::infinite_power() {
  return 0xFF;
}

template <> inline constexpr int binary_format<std::bfloat16_t>::sign_index() {
  return 15;
}

template <>
inline constexpr int binary_format<std::bfloat16_t>::largest_power_of_ten() {
  return 38;
}

template <>
inline constexpr int binary_format<std::bfloat16_t>::smallest_power_of_ten() {
  return -60;
}

template <>
inline constexpr size_t binary_format<std::bfloat16_t>::max_digits() {
  return 98;
}
#endif // __STDCPP_BFLOAT16_T__

template <>
inline constexpr uint64_t
binary_format<double>::max_mantissa_fast_path(int64_t power) {
  // caller is responsible to ensure that
  // power >= 0 && power <= 22
  //
  // Work around clang bug https://godbolt.org/z/zedh7rrhc
  return (void)max_mantissa[0], max_mantissa[power];
}

template <>
inline constexpr uint64_t
binary_format<float>::max_mantissa_fast_path(int64_t power) {
  // caller is responsible to ensure that
  // power >= 0 && power <= 10
  //
  // Work around clang bug https://godbolt.org/z/zedh7rrhc
  return (void)max_mantissa[0], max_mantissa[power];
}

template <>
inline constexpr double
binary_format<double>::exact_power_of_ten(int64_t power) {
  // Work around clang bug https://godbolt.org/z/zedh7rrhc
  return (void)powers_of_ten[0], powers_of_ten[power];
}

template <>
inline constexpr float binary_format<float>::exact_power_of_ten(int64_t power) {
  // Work around clang bug https://godbolt.org/z/zedh7rrhc
  return (void)powers_of_ten[0], powers_of_ten[power];
}

template <> inline constexpr int binary_format<double>::largest_power_of_ten() {
  return 308;
}

template <> inline constexpr int binary_format<float>::largest_power_of_ten() {
  return 38;
}

template <>
inline constexpr int binary_format<double>::smallest_power_of_ten() {
  return -342;
}

template <> inline constexpr int binary_format<float>::smallest_power_of_ten() {
  return -64;
}

template <> inline constexpr size_t binary_format<double>::max_digits() {
  return 769;
}

template <> inline constexpr size_t binary_format<float>::max_digits() {
  return 114;
}

template <>
inline constexpr binary_format<float>::equiv_uint
binary_format<float>::exponent_mask() {
  return 0x7F800000;
}

template <>
inline constexpr binary_format<double>::equiv_uint
binary_format<double>::exponent_mask() {
  return 0x7FF0000000000000;
}

template <>
inline constexpr binary_format<float>::equiv_uint
binary_format<float>::mantissa_mask() {
  return 0x007FFFFF;
}

template <>
inline constexpr binary_format<double>::equiv_uint
binary_format<double>::mantissa_mask() {
  return 0x000FFFFFFFFFFFFF;
}

template <>
inline constexpr binary_format<float>::equiv_uint
binary_format<float>::hidden_bit_mask() {
  return 0x00800000;
}

template <>
inline constexpr binary_format<double>::equiv_uint
binary_format<double>::hidden_bit_mask() {
  return 0x0010000000000000;
}

template <typename T>
fastfloat_really_inline FASTFLOAT_CONSTEXPR20 void
to_float(bool negative, adjusted_mantissa am, T &value) {
  using equiv_uint = equiv_uint_t<T>;
  equiv_uint word = equiv_uint(am.mantissa);
  word = equiv_uint(word | equiv_uint(am.power2)
                               << binary_format<T>::mantissa_explicit_bits());
  word =
      equiv_uint(word | equiv_uint(negative) << binary_format<T>::sign_index());
#if FASTFLOAT_HAS_BIT_CAST
  value = std::bit_cast<T>(word);
#else
  ::memcpy(&value, &word, sizeof(T));
#endif
}

template <typename = void> struct space_lut {
  static constexpr bool value[] = {
      0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
      0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
      0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
      0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
      0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
      0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
      0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
      0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
      0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
      0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
      0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
};

#if FASTFLOAT_DETAIL_MUST_DEFINE_CONSTEXPR_VARIABLE

template <typename T> constexpr bool space_lut<T>::value[];

#endif

template <typename UC> constexpr bool is_space(UC c) {
  return c < 256 && space_lut<>::value[uint8_t(c)];
}

template <typename UC> static constexpr uint64_t int_cmp_zeros() {
  static_assert((sizeof(UC) == 1) || (sizeof(UC) == 2) || (sizeof(UC) == 4),
                "Unsupported character size");
  return (sizeof(UC) == 1) ? 0x3030303030303030
         : (sizeof(UC) == 2)
             ? (uint64_t(UC('0')) << 48 | uint64_t(UC('0')) << 32 |
                uint64_t(UC('0')) << 16 | UC('0'))
             : (uint64_t(UC('0')) << 32 | UC('0'));
}

template <typename UC> static constexpr int int_cmp_len() {
  return sizeof(uint64_t) / sizeof(UC);
}

template <typename UC> constexpr UC const *str_const_nan();

template <> constexpr char const *str_const_nan<char>() { return "nan"; }

template <> constexpr wchar_t const *str_const_nan<wchar_t>() { return L"nan"; }

template <> constexpr char16_t const *str_const_nan<char16_t>() {
  return u"nan";
}

template <> constexpr char32_t const *str_const_nan<char32_t>() {
  return U"nan";
}

#ifdef __cpp_char8_t
template <> constexpr char8_t const *str_const_nan<char8_t>() {
  return u8"nan";
}
#endif

template <typename UC> constexpr UC const *str_const_inf();

template <> constexpr char const *str_const_inf<char>() { return "infinity"; }

template <> constexpr wchar_t const *str_const_inf<wchar_t>() {
  return L"infinity";
}

template <> constexpr char16_t const *str_const_inf<char16_t>() {
  return u"infinity";
}

template <> constexpr char32_t const *str_const_inf<char32_t>() {
  return U"infinity";
}

#ifdef __cpp_char8_t
template <> constexpr char8_t const *str_const_inf<char8_t>() {
  return u8"infinity";
}
#endif

template <typename = void> struct int_luts {
  static constexpr uint8_t chdigit[] = {
      255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
      255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
      255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
      255, 255, 255, 0,   1,   2,   3,   4,   5,   6,   7,   8,   9,   255, 255,
      255, 255, 255, 255, 255, 10,  11,  12,  13,  14,  15,  16,  17,  18,  19,
      20,  21,  22,  23,  24,  25,  26,  27,  28,  29,  30,  31,  32,  33,  34,
      35,  255, 255, 255, 255, 255, 255, 10,  11,  12,  13,  14,  15,  16,  17,
      18,  19,  20,  21,  22,  23,  24,  25,  26,  27,  28,  29,  30,  31,  32,
      33,  34,  35,  255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
      255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
      255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
      255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
      255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
      255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
      255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
      255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
      255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
      255};

  static constexpr size_t maxdigits_u64[] = {
      64, 41, 32, 28, 25, 23, 22, 21, 20, 19, 18, 18, 17, 17, 16, 16, 16, 16,
      15, 15, 15, 15, 14, 14, 14, 14, 14, 14, 14, 13, 13, 13, 13, 13, 13};

  static constexpr uint64_t min_safe_u64[] = {
      9223372036854775808ull,  12157665459056928801ull, 4611686018427387904,
      7450580596923828125,     4738381338321616896,     3909821048582988049,
      9223372036854775808ull,  12157665459056928801ull, 10000000000000000000ull,
      5559917313492231481,     2218611106740436992,     8650415919381337933,
      2177953337809371136,     6568408355712890625,     1152921504606846976,
      2862423051509815793,     6746640616477458432,     15181127029874798299ull,
      1638400000000000000,     3243919932521508681,     6221821273427820544,
      11592836324538749809ull, 876488338465357824,      1490116119384765625,
      2481152873203736576,     4052555153018976267,     6502111422497947648,
      10260628712958602189ull, 15943230000000000000ull, 787662783788549761,
      1152921504606846976,     1667889514952984961,     2386420683693101056,
      3379220508056640625,     4738381338321616896};
};

#if FASTFLOAT_DETAIL_MUST_DEFINE_CONSTEXPR_VARIABLE

template <typename T> constexpr uint8_t int_luts<T>::chdigit[];

template <typename T> constexpr size_t int_luts<T>::maxdigits_u64[];

template <typename T> constexpr uint64_t int_luts<T>::min_safe_u64[];

#endif

template <typename UC>
fastfloat_really_inline constexpr uint8_t ch_to_digit(UC c) {
  // wchar_t and char can be signed, so we need to be careful.
  using UnsignedUC = typename std::make_unsigned<UC>::type;
  return int_luts<>::chdigit[static_cast<unsigned char>(
      static_cast<UnsignedUC>(c) &
      static_cast<UnsignedUC>(
          -((static_cast<UnsignedUC>(c) & ~0xFFull) == 0)))];
}

fastfloat_really_inline constexpr size_t max_digits_u64(int base) {
  return int_luts<>::maxdigits_u64[base - 2];
}

// If a u64 is exactly max_digits_u64() in length, this is
// the value below which it has definitely overflowed.
fastfloat_really_inline constexpr uint64_t min_safe_u64(int base) {
  return int_luts<>::min_safe_u64[base - 2];
}

static_assert(std::is_same<equiv_uint_t<double>, uint64_t>::value,
              "equiv_uint should be uint64_t for double");
static_assert(std::numeric_limits<double>::is_iec559,
              "double must fulfill the requirements of IEC 559 (IEEE 754)");

static_assert(std::is_same<equiv_uint_t<float>, uint32_t>::value,
              "equiv_uint should be uint32_t for float");
static_assert(std::numeric_limits<float>::is_iec559,
              "float must fulfill the requirements of IEC 559 (IEEE 754)");

#ifdef __STDCPP_FLOAT64_T__
static_assert(std::is_same<equiv_uint_t<std::float64_t>, uint64_t>::value,
              "equiv_uint should be uint64_t for std::float64_t");
static_assert(
    std::numeric_limits<std::float64_t>::is_iec559,
    "std::float64_t must fulfill the requirements of IEC 559 (IEEE 754)");

template <>
struct binary_format<std::float64_t> : public binary_format<double> {};
#endif // __STDCPP_FLOAT64_T__

#ifdef __STDCPP_FLOAT32_T__
static_assert(std::is_same<equiv_uint_t<std::float32_t>, uint32_t>::value,
              "equiv_uint should be uint32_t for std::float32_t");
static_assert(
    std::numeric_limits<std::float32_t>::is_iec559,
    "std::float32_t must fulfill the requirements of IEC 559 (IEEE 754)");

template <>
struct binary_format<std::float32_t> : public binary_format<float> {};
#endif // __STDCPP_FLOAT32_T__

#ifdef __STDCPP_FLOAT16_T__
static_assert(
    std::is_same<binary_format<std::float16_t>::equiv_uint, uint16_t>::value,
    "equiv_uint should be uint16_t for std::float16_t");
static_assert(
    std::numeric_limits<std::float16_t>::is_iec559,
    "std::float16_t must fulfill the requirements of IEC 559 (IEEE 754)");
#endif // __STDCPP_FLOAT16_T__

#ifdef __STDCPP_BFLOAT16_T__
static_assert(
    std::is_same<binary_format<std::bfloat16_t>::equiv_uint, uint16_t>::value,
    "equiv_uint should be uint16_t for std::bfloat16_t");
static_assert(
    std::numeric_limits<std::bfloat16_t>::is_iec559,
    "std::bfloat16_t must fulfill the requirements of IEC 559 (IEEE 754)");
#endif // __STDCPP_BFLOAT16_T__

constexpr chars_format operator~(chars_format rhs) noexcept {
  using int_type = std::underlying_type<chars_format>::type;
  return static_cast<chars_format>(~static_cast<int_type>(rhs));
}

constexpr chars_format operator&(chars_format lhs, chars_format rhs) noexcept {
  using int_type = std::underlying_type<chars_format>::type;
  return static_cast<chars_format>(static_cast<int_type>(lhs) &
                                   static_cast<int_type>(rhs));
}

constexpr chars_format operator|(chars_format lhs, chars_format rhs) noexcept {
  using int_type = std::underlying_type<chars_format>::type;
  return static_cast<chars_format>(static_cast<int_type>(lhs) |
                                   static_cast<int_type>(rhs));
}

constexpr chars_format operator^(chars_format lhs, chars_format rhs) noexcept {
  using int_type = std::underlying_type<chars_format>::type;
  return static_cast<chars_format>(static_cast<int_type>(lhs) ^
                                   static_cast<int_type>(rhs));
}

fastfloat_really_inline FASTFLOAT_CONSTEXPR14 chars_format &
operator&=(chars_format &lhs, chars_format rhs) noexcept {
  return lhs = (lhs & rhs);
}

fastfloat_really_inline FASTFLOAT_CONSTEXPR14 chars_format &
operator|=(chars_format &lhs, chars_format rhs) noexcept {
  return lhs = (lhs | rhs);
}

fastfloat_really_inline FASTFLOAT_CONSTEXPR14 chars_format &
operator^=(chars_format &lhs, chars_format rhs) noexcept {
  return lhs = (lhs ^ rhs);
}

namespace detail {
// adjust for deprecated feature macros
constexpr chars_format adjust_for_feature_macros(chars_format fmt) {
  return fmt
#ifdef FASTFLOAT_ALLOWS_LEADING_PLUS
         | chars_format::allow_leading_plus
#endif
#ifdef FASTFLOAT_SKIP_WHITE_SPACE
         | chars_format::skip_white_space
#endif
      ;
}
} // namespace detail
} // namespace fast_float

#endif
