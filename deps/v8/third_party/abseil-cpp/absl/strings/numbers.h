// Copyright 2017 The Abseil Authors.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
// -----------------------------------------------------------------------------
// File: numbers.h
// -----------------------------------------------------------------------------
//
// This package contains functions for converting strings to numbers. For
// converting numbers to strings, use `StrCat()` or `StrAppend()` in str_cat.h,
// which automatically detect and convert most number values appropriately.

#ifndef ABSL_STRINGS_NUMBERS_H_
#define ABSL_STRINGS_NUMBERS_H_

#ifdef __SSSE3__
#include <tmmintrin.h>
#endif

#ifdef _MSC_VER
#include <intrin.h>
#endif

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <limits>
#include <string>
#include <type_traits>

#include "absl/base/attributes.h"
#include "absl/base/config.h"
#include "absl/base/internal/endian.h"
#include "absl/base/macros.h"
#include "absl/base/nullability.h"
#include "absl/base/port.h"
#include "absl/numeric/bits.h"
#include "absl/numeric/int128.h"
#include "absl/strings/string_view.h"

namespace absl {
ABSL_NAMESPACE_BEGIN

// SimpleAtoi()
//
// Converts the given string (optionally followed or preceded by ASCII
// whitespace) into an integer value, returning `true` if successful. The string
// must reflect a base-10 integer whose value falls within the range of the
// integer type (optionally preceded by a `+` or `-`). If any errors are
// encountered, this function returns `false`, leaving `out` in an unspecified
// state.
template <typename int_type>
[[nodiscard]] bool SimpleAtoi(absl::string_view str,
                              int_type* absl_nonnull out);

// SimpleAtof()
//
// Converts the given string (optionally followed or preceded by ASCII
// whitespace) into a float, which may be rounded on overflow or underflow,
// returning `true` if successful.
// See https://en.cppreference.com/w/c/string/byte/strtof for details about the
// allowed formats for `str`, except SimpleAtof() is locale-independent and will
// always use the "C" locale. If any errors are encountered, this function
// returns `false`, leaving `out` in an unspecified state.
[[nodiscard]] bool SimpleAtof(absl::string_view str, float* absl_nonnull out);

// SimpleAtod()
//
// Converts the given string (optionally followed or preceded by ASCII
// whitespace) into a double, which may be rounded on overflow or underflow,
// returning `true` if successful.
// See https://en.cppreference.com/w/c/string/byte/strtof for details about the
// allowed formats for `str`, except SimpleAtod is locale-independent and will
// always use the "C" locale. If any errors are encountered, this function
// returns `false`, leaving `out` in an unspecified state.
[[nodiscard]] bool SimpleAtod(absl::string_view str, double* absl_nonnull out);

// SimpleAtob()
//
// Converts the given string into a boolean, returning `true` if successful.
// The following case-insensitive strings are interpreted as boolean `true`:
// "true", "t", "yes", "y", "1". The following case-insensitive strings
// are interpreted as boolean `false`: "false", "f", "no", "n", "0". If any
// errors are encountered, this function returns `false`, leaving `out` in an
// unspecified state.
[[nodiscard]] bool SimpleAtob(absl::string_view str, bool* absl_nonnull out);

// SimpleHexAtoi()
//
// Converts a hexadecimal string (optionally followed or preceded by ASCII
// whitespace) to an integer, returning `true` if successful. Only valid base-16
// hexadecimal integers whose value falls within the range of the integer type
// (optionally preceded by a `+` or `-`) can be converted. A valid hexadecimal
// value may include both upper and lowercase character symbols, and may
// optionally include a leading "0x" (or "0X") number prefix, which is ignored
// by this function. If any errors are encountered, this function returns
// `false`, leaving `out` in an unspecified state.
template <typename int_type>
[[nodiscard]] bool SimpleHexAtoi(absl::string_view str,
                                 int_type* absl_nonnull out);

// Overloads of SimpleHexAtoi() for 128 bit integers.
[[nodiscard]] inline bool SimpleHexAtoi(absl::string_view str,
                                        absl::int128* absl_nonnull out);
[[nodiscard]] inline bool SimpleHexAtoi(absl::string_view str,
                                        absl::uint128* absl_nonnull out);

ABSL_NAMESPACE_END
}  // namespace absl

// End of public API.  Implementation details follow.

namespace absl {
ABSL_NAMESPACE_BEGIN
namespace numbers_internal {

template <typename int_type>
constexpr bool is_signed() {
  if constexpr (std::is_arithmetic<int_type>::value) {
    // Use std::numeric_limits<T>::is_signed where it's defined to work.
    return std::numeric_limits<int_type>::is_signed;
  }
  // TODO(jorg): This signed-ness check is used because it works correctly
  // with enums, and it also serves to check that int_type is not a pointer.
  // If one day something like std::is_signed<enum E> works, switch to it.
  return static_cast<int_type>(1) - 2 < 0;
}

// Digit conversion.
ABSL_DLL extern const char kHexChar[17];  // 0123456789abcdef
ABSL_DLL extern const char
    kHexTable[513];  // 000102030405060708090a0b0c0d0e0f1011...

// Writes a two-character representation of 'i' to 'buf'. 'i' must be in the
// range 0 <= i < 100, and buf must have space for two characters. Example:
//   char buf[2];
//   PutTwoDigits(42, buf);
//   // buf[0] == '4'
//   // buf[1] == '2'
void PutTwoDigits(uint32_t i, char* absl_nonnull buf);

// safe_strto?() functions for implementing SimpleAtoi()

bool safe_strto8_base(absl::string_view text, int8_t* absl_nonnull value,
                      int base);
bool safe_strto16_base(absl::string_view text, int16_t* absl_nonnull value,
                       int base);
bool safe_strto32_base(absl::string_view text, int32_t* absl_nonnull value,
                       int base);
bool safe_strto64_base(absl::string_view text, int64_t* absl_nonnull value,
                       int base);
bool safe_strto128_base(absl::string_view text,
                        absl::int128* absl_nonnull value, int base);
bool safe_strtou8_base(absl::string_view text, uint8_t* absl_nonnull value,
                       int base);
bool safe_strtou16_base(absl::string_view text, uint16_t* absl_nonnull value,
                        int base);
bool safe_strtou32_base(absl::string_view text, uint32_t* absl_nonnull value,
                        int base);
bool safe_strtou64_base(absl::string_view text, uint64_t* absl_nonnull value,
                        int base);
bool safe_strtou128_base(absl::string_view text,
                         absl::uint128* absl_nonnull value, int base);

static const int kFastToBufferSize = 32;
static const int kSixDigitsToBufferSize = 16;

// Helper function for fast formatting of floating-point values.
// The result is the same as printf's "%g", a.k.a. "%.6g"; that is, six
// significant digits are returned, trailing zeros are removed, and numbers
// outside the range 0.0001-999999 are output using scientific notation
// (1.23456e+06). This routine is heavily optimized.
// Required buffer size is `kSixDigitsToBufferSize`.
size_t SixDigitsToBuffer(double d, char* absl_nonnull buffer);

// WARNING: These functions may write more characters than necessary, because
// they are intended for speed. All functions take an output buffer
// as an argument and return a pointer to the last byte they wrote, which is the
// terminating '\0'. At most `kFastToBufferSize` bytes are written.
char* absl_nonnull FastIntToBuffer(int32_t i, char* absl_nonnull buffer)
    ABSL_INTERNAL_NEED_MIN_SIZE(buffer, kFastToBufferSize);
char* absl_nonnull FastIntToBuffer(uint32_t n, char* absl_nonnull out_str)
    ABSL_INTERNAL_NEED_MIN_SIZE(out_str, kFastToBufferSize);
char* absl_nonnull FastIntToBuffer(int64_t i, char* absl_nonnull buffer)
    ABSL_INTERNAL_NEED_MIN_SIZE(buffer, kFastToBufferSize);
char* absl_nonnull FastIntToBuffer(uint64_t i, char* absl_nonnull buffer)
    ABSL_INTERNAL_NEED_MIN_SIZE(buffer, kFastToBufferSize);

// For enums and integer types that are not an exact match for the types above,
// use templates to call the appropriate one of the four overloads above.
template <typename int_type>
char* absl_nonnull FastIntToBuffer(int_type i, char* absl_nonnull buffer)
    ABSL_INTERNAL_NEED_MIN_SIZE(buffer, kFastToBufferSize) {
  static_assert(sizeof(i) <= 64 / 8,
                "FastIntToBuffer works only with 64-bit-or-less integers.");
  // These conditions are constexpr bools to suppress MSVC warning C4127.
  constexpr bool kIsSigned = is_signed<int_type>();
  constexpr bool kUse64Bit = sizeof(i) > 32 / 8;
  if (kIsSigned) {
    if (kUse64Bit) {
      return FastIntToBuffer(static_cast<int64_t>(i), buffer);
    } else {
      return FastIntToBuffer(static_cast<int32_t>(i), buffer);
    }
  } else {
    if (kUse64Bit) {
      return FastIntToBuffer(static_cast<uint64_t>(i), buffer);
    } else {
      return FastIntToBuffer(static_cast<uint32_t>(i), buffer);
    }
  }
}

// Implementation of SimpleAtoi, generalized to support arbitrary base (used
// with base different from 10 elsewhere in Abseil implementation).
template <typename int_type>
[[nodiscard]] bool safe_strtoi_base(absl::string_view s,
                                    int_type* absl_nonnull out, int base) {
  static_assert(sizeof(*out) == 1 || sizeof(*out) == 2 || sizeof(*out) == 4 ||
                    sizeof(*out) == 8,
                "SimpleAtoi works only with 8, 16, 32, or 64-bit integers.");
  static_assert(!std::is_floating_point<int_type>::value,
                "Use SimpleAtof or SimpleAtod instead.");
  bool parsed;
  // These conditions are constexpr bools to suppress MSVC warning C4127.
  constexpr bool kIsSigned = is_signed<int_type>();
  constexpr int kIntTypeSize = sizeof(*out) * 8;
  if (kIsSigned) {
    if (kIntTypeSize == 64) {
      int64_t val;
      parsed = numbers_internal::safe_strto64_base(s, &val, base);
      *out = static_cast<int_type>(val);
    } else if (kIntTypeSize == 32) {
      int32_t val;
      parsed = numbers_internal::safe_strto32_base(s, &val, base);
      *out = static_cast<int_type>(val);
    } else if (kIntTypeSize == 16) {
      int16_t val;
      parsed = numbers_internal::safe_strto16_base(s, &val, base);
      *out = static_cast<int_type>(val);
    } else if (kIntTypeSize == 8) {
      int8_t val;
      parsed = numbers_internal::safe_strto8_base(s, &val, base);
      *out = static_cast<int_type>(val);
    }
  } else {
    if (kIntTypeSize == 64) {
      uint64_t val;
      parsed = numbers_internal::safe_strtou64_base(s, &val, base);
      *out = static_cast<int_type>(val);
    } else if (kIntTypeSize == 32) {
      uint32_t val;
      parsed = numbers_internal::safe_strtou32_base(s, &val, base);
      *out = static_cast<int_type>(val);
    } else if (kIntTypeSize == 16) {
      uint16_t val;
      parsed = numbers_internal::safe_strtou16_base(s, &val, base);
      *out = static_cast<int_type>(val);
    } else if (kIntTypeSize == 8) {
      uint8_t val;
      parsed = numbers_internal::safe_strtou8_base(s, &val, base);
      *out = static_cast<int_type>(val);
    }
  }
  return parsed;
}

// FastHexToBufferZeroPad16()
//
// Outputs `val` into `out` as if by `snprintf(out, 17, "%016x", val)` but
// without the terminating null character. Thus `out` must be of length >= 16.
// Returns the number of non-pad digits of the output (it can never be zero
// since 0 has one digit).
inline size_t FastHexToBufferZeroPad16(uint64_t val, char* absl_nonnull out) {
#ifdef ABSL_INTERNAL_HAVE_SSSE3
  uint64_t be = absl::big_endian::FromHost64(val);
  const auto kNibbleMask = _mm_set1_epi8(0xf);
  const auto kHexDigits = _mm_setr_epi8('0', '1', '2', '3', '4', '5', '6', '7',
                                        '8', '9', 'a', 'b', 'c', 'd', 'e', 'f');
  auto v = _mm_loadl_epi64(reinterpret_cast<__m128i*>(&be));  // load lo dword
  auto v4 = _mm_srli_epi64(v, 4);                            // shift 4 right
  auto il = _mm_unpacklo_epi8(v4, v);                        // interleave bytes
  auto m = _mm_and_si128(il, kNibbleMask);                   // mask out nibbles
  auto hexchars = _mm_shuffle_epi8(kHexDigits, m);           // hex chars
  _mm_storeu_si128(reinterpret_cast<__m128i*>(out), hexchars);
#else
  for (int i = 0; i < 8; ++i) {
    auto byte = (val >> (56 - 8 * i)) & 0xFF;
    auto* hex = &absl::numbers_internal::kHexTable[byte * 2];
    std::memcpy(out + 2 * i, hex, 2);
  }
#endif
  // | 0x1 so that even 0 has 1 digit.
  return 16 - static_cast<size_t>(countl_zero(val | 0x1) / 4);
}

}  // namespace numbers_internal

template <typename int_type>
[[nodiscard]] bool SimpleAtoi(absl::string_view str,
                              int_type* absl_nonnull out) {
  return numbers_internal::safe_strtoi_base(str, out, 10);
}

[[nodiscard]] inline bool SimpleAtoi(absl::string_view str,
                                     absl::int128* absl_nonnull out) {
  return numbers_internal::safe_strto128_base(str, out, 10);
}

[[nodiscard]] inline bool SimpleAtoi(absl::string_view str,
                                     absl::uint128* absl_nonnull out) {
  return numbers_internal::safe_strtou128_base(str, out, 10);
}

template <typename int_type>
[[nodiscard]] bool SimpleHexAtoi(absl::string_view str,
                                 int_type* absl_nonnull out) {
  return numbers_internal::safe_strtoi_base(str, out, 16);
}

[[nodiscard]] inline bool SimpleHexAtoi(absl::string_view str,
                                        absl::int128* absl_nonnull out) {
  return numbers_internal::safe_strto128_base(str, out, 16);
}

[[nodiscard]] inline bool SimpleHexAtoi(absl::string_view str,
                                        absl::uint128* absl_nonnull out) {
  return numbers_internal::safe_strtou128_base(str, out, 16);
}

ABSL_NAMESPACE_END
}  // namespace absl

#endif  // ABSL_STRINGS_NUMBERS_H_
