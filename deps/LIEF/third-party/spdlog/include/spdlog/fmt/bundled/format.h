/*
  Formatting library for C++

  Copyright (c) 2012 - present, Victor Zverovich

  Permission is hereby granted, free of charge, to any person obtaining
  a copy of this software and associated documentation files (the
  "Software"), to deal in the Software without restriction, including
  without limitation the rights to use, copy, modify, merge, publish,
  distribute, sublicense, and/or sell copies of the Software, and to
  permit persons to whom the Software is furnished to do so, subject to
  the following conditions:

  The above copyright notice and this permission notice shall be
  included in all copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
  EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
  MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
  NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
  LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
  OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
  WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

  --- Optional exception to the license ---

  As an exception, if, as a result of your compiling your source code, portions
  of this Software are embedded into a machine-executable object form of such
  source code, you may redistribute such embedded portions in such object form
  without including the above copyright and permission notices.
 */

#ifndef FMT_FORMAT_H_
#define FMT_FORMAT_H_

#ifndef _LIBCPP_REMOVE_TRANSITIVE_INCLUDES
#  define _LIBCPP_REMOVE_TRANSITIVE_INCLUDES
#  define FMT_REMOVE_TRANSITIVE_INCLUDES
#endif

#include "base.h"

#ifndef FMT_MODULE
#  include <cmath>    // std::signbit
#  include <cstddef>  // std::byte
#  include <cstdint>  // uint32_t
#  include <cstring>  // std::memcpy
#  include <limits>   // std::numeric_limits
#  include <new>      // std::bad_alloc
#  if defined(__GLIBCXX__) && !defined(_GLIBCXX_USE_DUAL_ABI)
// Workaround for pre gcc 5 libstdc++.
#    include <memory>  // std::allocator_traits
#  endif
#  include <stdexcept>     // std::runtime_error
#  include <string>        // std::string
#  include <system_error>  // std::system_error

// Check FMT_CPLUSPLUS to avoid a warning in MSVC.
#  if FMT_HAS_INCLUDE(<bit>) && FMT_CPLUSPLUS > 201703L
#    include <bit>  // std::bit_cast
#  endif

// libc++ supports string_view in pre-c++17.
#  if FMT_HAS_INCLUDE(<string_view>) && \
      (FMT_CPLUSPLUS >= 201703L || defined(_LIBCPP_VERSION))
#    include <string_view>
#    define FMT_USE_STRING_VIEW
#  endif

#  if FMT_MSC_VERSION
#    include <intrin.h>  // _BitScanReverse[64], _umul128
#  endif
#endif  // FMT_MODULE

#if defined(FMT_USE_NONTYPE_TEMPLATE_ARGS)
// Use the provided definition.
#elif defined(__NVCOMPILER)
#  define FMT_USE_NONTYPE_TEMPLATE_ARGS 0
#elif FMT_GCC_VERSION >= 903 && FMT_CPLUSPLUS >= 201709L
#  define FMT_USE_NONTYPE_TEMPLATE_ARGS 1
#elif defined(__cpp_nontype_template_args) && \
    __cpp_nontype_template_args >= 201911L
#  define FMT_USE_NONTYPE_TEMPLATE_ARGS 1
#elif FMT_CLANG_VERSION >= 1200 && FMT_CPLUSPLUS >= 202002L
#  define FMT_USE_NONTYPE_TEMPLATE_ARGS 1
#else
#  define FMT_USE_NONTYPE_TEMPLATE_ARGS 0
#endif

#if defined __cpp_inline_variables && __cpp_inline_variables >= 201606L
#  define FMT_INLINE_VARIABLE inline
#else
#  define FMT_INLINE_VARIABLE
#endif

// Check if RTTI is disabled.
#ifdef FMT_USE_RTTI
// Use the provided definition.
#elif defined(__GXX_RTTI) || FMT_HAS_FEATURE(cxx_rtti) || defined(_CPPRTTI) || \
    defined(__INTEL_RTTI__) || defined(__RTTI)
// __RTTI is for EDG compilers. _CPPRTTI is for MSVC.
#  define FMT_USE_RTTI 1
#else
#  define FMT_USE_RTTI 0
#endif

// Visibility when compiled as a shared library/object.
#if defined(FMT_LIB_EXPORT) || defined(FMT_SHARED)
#  define FMT_SO_VISIBILITY(value) FMT_VISIBILITY(value)
#else
#  define FMT_SO_VISIBILITY(value)
#endif

#if FMT_GCC_VERSION || FMT_CLANG_VERSION
#  define FMT_NOINLINE __attribute__((noinline))
#else
#  define FMT_NOINLINE
#endif

// GCC 4.9 doesn't support qualified names in specializations.
namespace std {
template <typename T> struct iterator_traits<fmt::basic_appender<T>> {
  using iterator_category = output_iterator_tag;
  using value_type = T;
  using difference_type =
      decltype(static_cast<int*>(nullptr) - static_cast<int*>(nullptr));
  using pointer = void;
  using reference = void;
};
}  // namespace std

#ifndef FMT_THROW
#  if FMT_USE_EXCEPTIONS
#    if FMT_MSC_VERSION || defined(__NVCC__)
FMT_BEGIN_NAMESPACE
namespace detail {
template <typename Exception> inline void do_throw(const Exception& x) {
  // Silence unreachable code warnings in MSVC and NVCC because these
  // are nearly impossible to fix in a generic code.
  volatile bool b = true;
  if (b) throw x;
}
}  // namespace detail
FMT_END_NAMESPACE
#      define FMT_THROW(x) detail::do_throw(x)
#    else
#      define FMT_THROW(x) throw x
#    endif
#  else
#    define FMT_THROW(x) \
      ::fmt::detail::assert_fail(__FILE__, __LINE__, (x).what())
#  endif  // FMT_USE_EXCEPTIONS
#endif    // FMT_THROW

// Defining FMT_REDUCE_INT_INSTANTIATIONS to 1, will reduce the number of
// integer formatter template instantiations to just one by only using the
// largest integer type. This results in a reduction in binary size but will
// cause a decrease in integer formatting performance.
#if !defined(FMT_REDUCE_INT_INSTANTIATIONS)
#  define FMT_REDUCE_INT_INSTANTIATIONS 0
#endif

FMT_BEGIN_NAMESPACE

template <typename Char, typename Traits, typename Allocator>
struct is_contiguous<std::basic_string<Char, Traits, Allocator>>
    : std::true_type {};

namespace detail {

// __builtin_clz is broken in clang with Microsoft codegen:
// https://github.com/fmtlib/fmt/issues/519.
#if !FMT_MSC_VERSION
#  if FMT_HAS_BUILTIN(__builtin_clz) || FMT_GCC_VERSION || FMT_ICC_VERSION
#    define FMT_BUILTIN_CLZ(n) __builtin_clz(n)
#  endif
#  if FMT_HAS_BUILTIN(__builtin_clzll) || FMT_GCC_VERSION || FMT_ICC_VERSION
#    define FMT_BUILTIN_CLZLL(n) __builtin_clzll(n)
#  endif
#endif

// Some compilers masquerade as both MSVC and GCC but otherwise support
// __builtin_clz and __builtin_clzll, so only define FMT_BUILTIN_CLZ using the
// MSVC intrinsics if the clz and clzll builtins are not available.
#if FMT_MSC_VERSION && !defined(FMT_BUILTIN_CLZLL)
// Avoid Clang with Microsoft CodeGen's -Wunknown-pragmas warning.
#  ifndef __clang__
#    pragma intrinsic(_BitScanReverse)
#    ifdef _WIN64
#      pragma intrinsic(_BitScanReverse64)
#    endif
#  endif

inline auto clz(uint32_t x) -> int {
  FMT_ASSERT(x != 0, "");
  FMT_MSC_WARNING(suppress : 6102)  // Suppress a bogus static analysis warning.
  unsigned long r = 0;
  _BitScanReverse(&r, x);
  return 31 ^ static_cast<int>(r);
}
#  define FMT_BUILTIN_CLZ(n) detail::clz(n)

inline auto clzll(uint64_t x) -> int {
  FMT_ASSERT(x != 0, "");
  FMT_MSC_WARNING(suppress : 6102)  // Suppress a bogus static analysis warning.
  unsigned long r = 0;
#  ifdef _WIN64
  _BitScanReverse64(&r, x);
#  else
  // Scan the high 32 bits.
  if (_BitScanReverse(&r, static_cast<uint32_t>(x >> 32)))
    return 63 ^ static_cast<int>(r + 32);
  // Scan the low 32 bits.
  _BitScanReverse(&r, static_cast<uint32_t>(x));
#  endif
  return 63 ^ static_cast<int>(r);
}
#  define FMT_BUILTIN_CLZLL(n) detail::clzll(n)
#endif  // FMT_MSC_VERSION && !defined(FMT_BUILTIN_CLZLL)

FMT_CONSTEXPR inline void abort_fuzzing_if(bool condition) {
  ignore_unused(condition);
#ifdef FMT_FUZZ
  if (condition) throw std::runtime_error("fuzzing limit reached");
#endif
}

#if defined(FMT_USE_STRING_VIEW)
template <typename Char> using std_string_view = std::basic_string_view<Char>;
#else
template <typename Char> struct std_string_view {
  operator basic_string_view<Char>() const;
};
#endif

template <typename Char, Char... C> struct string_literal {
  static constexpr Char value[sizeof...(C)] = {C...};
  constexpr operator basic_string_view<Char>() const {
    return {value, sizeof...(C)};
  }
};
#if FMT_CPLUSPLUS < 201703L
template <typename Char, Char... C>
constexpr Char string_literal<Char, C...>::value[sizeof...(C)];
#endif

// Implementation of std::bit_cast for pre-C++20.
template <typename To, typename From, FMT_ENABLE_IF(sizeof(To) == sizeof(From))>
FMT_CONSTEXPR20 auto bit_cast(const From& from) -> To {
#ifdef __cpp_lib_bit_cast
  if (is_constant_evaluated()) return std::bit_cast<To>(from);
#endif
  auto to = To();
  // The cast suppresses a bogus -Wclass-memaccess on GCC.
  std::memcpy(static_cast<void*>(&to), &from, sizeof(to));
  return to;
}

inline auto is_big_endian() -> bool {
#ifdef _WIN32
  return false;
#elif defined(__BIG_ENDIAN__)
  return true;
#elif defined(__BYTE_ORDER__) && defined(__ORDER_BIG_ENDIAN__)
  return __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__;
#else
  struct bytes {
    char data[sizeof(int)];
  };
  return bit_cast<bytes>(1).data[0] == 0;
#endif
}

class uint128_fallback {
 private:
  uint64_t lo_, hi_;

 public:
  constexpr uint128_fallback(uint64_t hi, uint64_t lo) : lo_(lo), hi_(hi) {}
  constexpr uint128_fallback(uint64_t value = 0) : lo_(value), hi_(0) {}

  constexpr auto high() const noexcept -> uint64_t { return hi_; }
  constexpr auto low() const noexcept -> uint64_t { return lo_; }

  template <typename T, FMT_ENABLE_IF(std::is_integral<T>::value)>
  constexpr explicit operator T() const {
    return static_cast<T>(lo_);
  }

  friend constexpr auto operator==(const uint128_fallback& lhs,
                                   const uint128_fallback& rhs) -> bool {
    return lhs.hi_ == rhs.hi_ && lhs.lo_ == rhs.lo_;
  }
  friend constexpr auto operator!=(const uint128_fallback& lhs,
                                   const uint128_fallback& rhs) -> bool {
    return !(lhs == rhs);
  }
  friend constexpr auto operator>(const uint128_fallback& lhs,
                                  const uint128_fallback& rhs) -> bool {
    return lhs.hi_ != rhs.hi_ ? lhs.hi_ > rhs.hi_ : lhs.lo_ > rhs.lo_;
  }
  friend constexpr auto operator|(const uint128_fallback& lhs,
                                  const uint128_fallback& rhs)
      -> uint128_fallback {
    return {lhs.hi_ | rhs.hi_, lhs.lo_ | rhs.lo_};
  }
  friend constexpr auto operator&(const uint128_fallback& lhs,
                                  const uint128_fallback& rhs)
      -> uint128_fallback {
    return {lhs.hi_ & rhs.hi_, lhs.lo_ & rhs.lo_};
  }
  friend constexpr auto operator~(const uint128_fallback& n)
      -> uint128_fallback {
    return {~n.hi_, ~n.lo_};
  }
  friend FMT_CONSTEXPR auto operator+(const uint128_fallback& lhs,
                                      const uint128_fallback& rhs)
      -> uint128_fallback {
    auto result = uint128_fallback(lhs);
    result += rhs;
    return result;
  }
  friend FMT_CONSTEXPR auto operator*(const uint128_fallback& lhs, uint32_t rhs)
      -> uint128_fallback {
    FMT_ASSERT(lhs.hi_ == 0, "");
    uint64_t hi = (lhs.lo_ >> 32) * rhs;
    uint64_t lo = (lhs.lo_ & ~uint32_t()) * rhs;
    uint64_t new_lo = (hi << 32) + lo;
    return {(hi >> 32) + (new_lo < lo ? 1 : 0), new_lo};
  }
  friend constexpr auto operator-(const uint128_fallback& lhs, uint64_t rhs)
      -> uint128_fallback {
    return {lhs.hi_ - (lhs.lo_ < rhs ? 1 : 0), lhs.lo_ - rhs};
  }
  FMT_CONSTEXPR auto operator>>(int shift) const -> uint128_fallback {
    if (shift == 64) return {0, hi_};
    if (shift > 64) return uint128_fallback(0, hi_) >> (shift - 64);
    return {hi_ >> shift, (hi_ << (64 - shift)) | (lo_ >> shift)};
  }
  FMT_CONSTEXPR auto operator<<(int shift) const -> uint128_fallback {
    if (shift == 64) return {lo_, 0};
    if (shift > 64) return uint128_fallback(lo_, 0) << (shift - 64);
    return {hi_ << shift | (lo_ >> (64 - shift)), (lo_ << shift)};
  }
  FMT_CONSTEXPR auto operator>>=(int shift) -> uint128_fallback& {
    return *this = *this >> shift;
  }
  FMT_CONSTEXPR void operator+=(uint128_fallback n) {
    uint64_t new_lo = lo_ + n.lo_;
    uint64_t new_hi = hi_ + n.hi_ + (new_lo < lo_ ? 1 : 0);
    FMT_ASSERT(new_hi >= hi_, "");
    lo_ = new_lo;
    hi_ = new_hi;
  }
  FMT_CONSTEXPR void operator&=(uint128_fallback n) {
    lo_ &= n.lo_;
    hi_ &= n.hi_;
  }

  FMT_CONSTEXPR20 auto operator+=(uint64_t n) noexcept -> uint128_fallback& {
    if (is_constant_evaluated()) {
      lo_ += n;
      hi_ += (lo_ < n ? 1 : 0);
      return *this;
    }
#if FMT_HAS_BUILTIN(__builtin_addcll) && !defined(__ibmxl__)
    unsigned long long carry;
    lo_ = __builtin_addcll(lo_, n, 0, &carry);
    hi_ += carry;
#elif FMT_HAS_BUILTIN(__builtin_ia32_addcarryx_u64) && !defined(__ibmxl__)
    unsigned long long result;
    auto carry = __builtin_ia32_addcarryx_u64(0, lo_, n, &result);
    lo_ = result;
    hi_ += carry;
#elif defined(_MSC_VER) && defined(_M_X64)
    auto carry = _addcarry_u64(0, lo_, n, &lo_);
    _addcarry_u64(carry, hi_, 0, &hi_);
#else
    lo_ += n;
    hi_ += (lo_ < n ? 1 : 0);
#endif
    return *this;
  }
};

using uint128_t = conditional_t<FMT_USE_INT128, uint128_opt, uint128_fallback>;

#ifdef UINTPTR_MAX
using uintptr_t = ::uintptr_t;
#else
using uintptr_t = uint128_t;
#endif

// Returns the largest possible value for type T. Same as
// std::numeric_limits<T>::max() but shorter and not affected by the max macro.
template <typename T> constexpr auto max_value() -> T {
  return (std::numeric_limits<T>::max)();
}
template <typename T> constexpr auto num_bits() -> int {
  return std::numeric_limits<T>::digits;
}
// std::numeric_limits<T>::digits may return 0 for 128-bit ints.
template <> constexpr auto num_bits<int128_opt>() -> int { return 128; }
template <> constexpr auto num_bits<uint128_opt>() -> int { return 128; }
template <> constexpr auto num_bits<uint128_fallback>() -> int { return 128; }

// A heterogeneous bit_cast used for converting 96-bit long double to uint128_t
// and 128-bit pointers to uint128_fallback.
template <typename To, typename From, FMT_ENABLE_IF(sizeof(To) > sizeof(From))>
inline auto bit_cast(const From& from) -> To {
  constexpr auto size = static_cast<int>(sizeof(From) / sizeof(unsigned short));
  struct data_t {
    unsigned short value[static_cast<unsigned>(size)];
  } data = bit_cast<data_t>(from);
  auto result = To();
  if (const_check(is_big_endian())) {
    for (int i = 0; i < size; ++i)
      result = (result << num_bits<unsigned short>()) | data.value[i];
  } else {
    for (int i = size - 1; i >= 0; --i)
      result = (result << num_bits<unsigned short>()) | data.value[i];
  }
  return result;
}

template <typename UInt>
FMT_CONSTEXPR20 inline auto countl_zero_fallback(UInt n) -> int {
  int lz = 0;
  constexpr UInt msb_mask = static_cast<UInt>(1) << (num_bits<UInt>() - 1);
  for (; (n & msb_mask) == 0; n <<= 1) lz++;
  return lz;
}

FMT_CONSTEXPR20 inline auto countl_zero(uint32_t n) -> int {
#ifdef FMT_BUILTIN_CLZ
  if (!is_constant_evaluated()) return FMT_BUILTIN_CLZ(n);
#endif
  return countl_zero_fallback(n);
}

FMT_CONSTEXPR20 inline auto countl_zero(uint64_t n) -> int {
#ifdef FMT_BUILTIN_CLZLL
  if (!is_constant_evaluated()) return FMT_BUILTIN_CLZLL(n);
#endif
  return countl_zero_fallback(n);
}

FMT_INLINE void assume(bool condition) {
  (void)condition;
#if FMT_HAS_BUILTIN(__builtin_assume) && !FMT_ICC_VERSION
  __builtin_assume(condition);
#elif FMT_GCC_VERSION
  if (!condition) __builtin_unreachable();
#endif
}

// Attempts to reserve space for n extra characters in the output range.
// Returns a pointer to the reserved range or a reference to it.
template <typename OutputIt,
          FMT_ENABLE_IF(is_back_insert_iterator<OutputIt>::value&&
                            is_contiguous<typename OutputIt::container>::value)>
#if FMT_CLANG_VERSION >= 307 && !FMT_ICC_VERSION
__attribute__((no_sanitize("undefined")))
#endif
FMT_CONSTEXPR20 inline auto
reserve(OutputIt it, size_t n) -> typename OutputIt::value_type* {
  auto& c = get_container(it);
  size_t size = c.size();
  c.resize(size + n);
  return &c[size];
}

template <typename T>
FMT_CONSTEXPR20 inline auto reserve(basic_appender<T> it, size_t n)
    -> basic_appender<T> {
  buffer<T>& buf = get_container(it);
  buf.try_reserve(buf.size() + n);
  return it;
}

template <typename Iterator>
constexpr auto reserve(Iterator& it, size_t) -> Iterator& {
  return it;
}

template <typename OutputIt>
using reserve_iterator =
    remove_reference_t<decltype(reserve(std::declval<OutputIt&>(), 0))>;

template <typename T, typename OutputIt>
constexpr auto to_pointer(OutputIt, size_t) -> T* {
  return nullptr;
}
template <typename T>
FMT_CONSTEXPR20 auto to_pointer(basic_appender<T> it, size_t n) -> T* {
  buffer<T>& buf = get_container(it);
  buf.try_reserve(buf.size() + n);
  auto size = buf.size();
  if (buf.capacity() < size + n) return nullptr;
  buf.try_resize(size + n);
  return buf.data() + size;
}

template <typename OutputIt,
          FMT_ENABLE_IF(is_back_insert_iterator<OutputIt>::value&&
                            is_contiguous<typename OutputIt::container>::value)>
inline auto base_iterator(OutputIt it,
                          typename OutputIt::container_type::value_type*)
    -> OutputIt {
  return it;
}

template <typename Iterator>
constexpr auto base_iterator(Iterator, Iterator it) -> Iterator {
  return it;
}

// <algorithm> is spectacularly slow to compile in C++20 so use a simple fill_n
// instead (#1998).
template <typename OutputIt, typename Size, typename T>
FMT_CONSTEXPR auto fill_n(OutputIt out, Size count, const T& value)
    -> OutputIt {
  for (Size i = 0; i < count; ++i) *out++ = value;
  return out;
}
template <typename T, typename Size>
FMT_CONSTEXPR20 auto fill_n(T* out, Size count, char value) -> T* {
  if (is_constant_evaluated()) return fill_n<T*, Size, T>(out, count, value);
  std::memset(out, value, to_unsigned(count));
  return out + count;
}

template <typename OutChar, typename InputIt, typename OutputIt>
FMT_CONSTEXPR FMT_NOINLINE auto copy_noinline(InputIt begin, InputIt end,
                                              OutputIt out) -> OutputIt {
  return copy<OutChar>(begin, end, out);
}

// A public domain branchless UTF-8 decoder by Christopher Wellons:
// https://github.com/skeeto/branchless-utf8
/* Decode the next character, c, from s, reporting errors in e.
 *
 * Since this is a branchless decoder, four bytes will be read from the
 * buffer regardless of the actual length of the next character. This
 * means the buffer _must_ have at least three bytes of zero padding
 * following the end of the data stream.
 *
 * Errors are reported in e, which will be non-zero if the parsed
 * character was somehow invalid: invalid byte sequence, non-canonical
 * encoding, or a surrogate half.
 *
 * The function returns a pointer to the next character. When an error
 * occurs, this pointer will be a guess that depends on the particular
 * error, but it will always advance at least one byte.
 */
FMT_CONSTEXPR inline auto utf8_decode(const char* s, uint32_t* c, int* e)
    -> const char* {
  constexpr const int masks[] = {0x00, 0x7f, 0x1f, 0x0f, 0x07};
  constexpr const uint32_t mins[] = {4194304, 0, 128, 2048, 65536};
  constexpr const int shiftc[] = {0, 18, 12, 6, 0};
  constexpr const int shifte[] = {0, 6, 4, 2, 0};

  int len = "\1\1\1\1\1\1\1\1\1\1\1\1\1\1\1\1\0\0\0\0\0\0\0\0\2\2\2\2\3\3\4"
      [static_cast<unsigned char>(*s) >> 3];
  // Compute the pointer to the next character early so that the next
  // iteration can start working on the next character. Neither Clang
  // nor GCC figure out this reordering on their own.
  const char* next = s + len + !len;

  using uchar = unsigned char;

  // Assume a four-byte character and load four bytes. Unused bits are
  // shifted out.
  *c = uint32_t(uchar(s[0]) & masks[len]) << 18;
  *c |= uint32_t(uchar(s[1]) & 0x3f) << 12;
  *c |= uint32_t(uchar(s[2]) & 0x3f) << 6;
  *c |= uint32_t(uchar(s[3]) & 0x3f) << 0;
  *c >>= shiftc[len];

  // Accumulate the various error conditions.
  *e = (*c < mins[len]) << 6;       // non-canonical encoding
  *e |= ((*c >> 11) == 0x1b) << 7;  // surrogate half?
  *e |= (*c > 0x10FFFF) << 8;       // out of range?
  *e |= (uchar(s[1]) & 0xc0) >> 2;
  *e |= (uchar(s[2]) & 0xc0) >> 4;
  *e |= uchar(s[3]) >> 6;
  *e ^= 0x2a;  // top two bits of each tail byte correct?
  *e >>= shifte[len];

  return next;
}

constexpr FMT_INLINE_VARIABLE uint32_t invalid_code_point = ~uint32_t();

// Invokes f(cp, sv) for every code point cp in s with sv being the string view
// corresponding to the code point. cp is invalid_code_point on error.
template <typename F>
FMT_CONSTEXPR void for_each_codepoint(string_view s, F f) {
  auto decode = [f](const char* buf_ptr, const char* ptr) {
    auto cp = uint32_t();
    auto error = 0;
    auto end = utf8_decode(buf_ptr, &cp, &error);
    bool result = f(error ? invalid_code_point : cp,
                    string_view(ptr, error ? 1 : to_unsigned(end - buf_ptr)));
    return result ? (error ? buf_ptr + 1 : end) : nullptr;
  };

  auto p = s.data();
  const size_t block_size = 4;  // utf8_decode always reads blocks of 4 chars.
  if (s.size() >= block_size) {
    for (auto end = p + s.size() - block_size + 1; p < end;) {
      p = decode(p, p);
      if (!p) return;
    }
  }
  auto num_chars_left = to_unsigned(s.data() + s.size() - p);
  if (num_chars_left == 0) return;

  // Suppress bogus -Wstringop-overflow.
  if (FMT_GCC_VERSION) num_chars_left &= 3;
  char buf[2 * block_size - 1] = {};
  copy<char>(p, p + num_chars_left, buf);
  const char* buf_ptr = buf;
  do {
    auto end = decode(buf_ptr, p);
    if (!end) return;
    p += end - buf_ptr;
    buf_ptr = end;
  } while (buf_ptr < buf + num_chars_left);
}

template <typename Char>
inline auto compute_width(basic_string_view<Char> s) -> size_t {
  return s.size();
}

// Computes approximate display width of a UTF-8 string.
FMT_CONSTEXPR inline auto compute_width(string_view s) -> size_t {
  size_t num_code_points = 0;
  // It is not a lambda for compatibility with C++14.
  struct count_code_points {
    size_t* count;
    FMT_CONSTEXPR auto operator()(uint32_t cp, string_view) const -> bool {
      *count += to_unsigned(
          1 +
          (cp >= 0x1100 &&
           (cp <= 0x115f ||  // Hangul Jamo init. consonants
            cp == 0x2329 ||  // LEFT-POINTING ANGLE BRACKET
            cp == 0x232a ||  // RIGHT-POINTING ANGLE BRACKET
            // CJK ... Yi except IDEOGRAPHIC HALF FILL SPACE:
            (cp >= 0x2e80 && cp <= 0xa4cf && cp != 0x303f) ||
            (cp >= 0xac00 && cp <= 0xd7a3) ||    // Hangul Syllables
            (cp >= 0xf900 && cp <= 0xfaff) ||    // CJK Compatibility Ideographs
            (cp >= 0xfe10 && cp <= 0xfe19) ||    // Vertical Forms
            (cp >= 0xfe30 && cp <= 0xfe6f) ||    // CJK Compatibility Forms
            (cp >= 0xff00 && cp <= 0xff60) ||    // Fullwidth Forms
            (cp >= 0xffe0 && cp <= 0xffe6) ||    // Fullwidth Forms
            (cp >= 0x20000 && cp <= 0x2fffd) ||  // CJK
            (cp >= 0x30000 && cp <= 0x3fffd) ||
            // Miscellaneous Symbols and Pictographs + Emoticons:
            (cp >= 0x1f300 && cp <= 0x1f64f) ||
            // Supplemental Symbols and Pictographs:
            (cp >= 0x1f900 && cp <= 0x1f9ff))));
      return true;
    }
  };
  // We could avoid branches by using utf8_decode directly.
  for_each_codepoint(s, count_code_points{&num_code_points});
  return num_code_points;
}

template <typename Char>
inline auto code_point_index(basic_string_view<Char> s, size_t n) -> size_t {
  return min_of(n, s.size());
}

// Calculates the index of the nth code point in a UTF-8 string.
inline auto code_point_index(string_view s, size_t n) -> size_t {
  size_t result = s.size();
  const char* begin = s.begin();
  for_each_codepoint(s, [begin, &n, &result](uint32_t, string_view sv) {
    if (n != 0) {
      --n;
      return true;
    }
    result = to_unsigned(sv.begin() - begin);
    return false;
  });
  return result;
}

template <typename T> struct is_integral : std::is_integral<T> {};
template <> struct is_integral<int128_opt> : std::true_type {};
template <> struct is_integral<uint128_t> : std::true_type {};

template <typename T>
using is_signed =
    std::integral_constant<bool, std::numeric_limits<T>::is_signed ||
                                     std::is_same<T, int128_opt>::value>;

template <typename T>
using is_integer =
    bool_constant<is_integral<T>::value && !std::is_same<T, bool>::value &&
                  !std::is_same<T, char>::value &&
                  !std::is_same<T, wchar_t>::value>;

#if defined(FMT_USE_FLOAT128)
// Use the provided definition.
#elif FMT_CLANG_VERSION >= 309 && FMT_HAS_INCLUDE(<quadmath.h>)
#  define FMT_USE_FLOAT128 1
#elif FMT_GCC_VERSION && defined(_GLIBCXX_USE_FLOAT128) && \
    !defined(__STRICT_ANSI__)
#  define FMT_USE_FLOAT128 1
#else
#  define FMT_USE_FLOAT128 0
#endif
#if FMT_USE_FLOAT128
using float128 = __float128;
#else
struct float128 {};
#endif

template <typename T> using is_float128 = std::is_same<T, float128>;

template <typename T> struct is_floating_point : std::is_floating_point<T> {};
template <> struct is_floating_point<float128> : std::true_type {};

template <typename T, bool = is_floating_point<T>::value>
struct is_fast_float : bool_constant<std::numeric_limits<T>::is_iec559 &&
                                     sizeof(T) <= sizeof(double)> {};
template <typename T> struct is_fast_float<T, false> : std::false_type {};

template <typename T>
using is_double_double = bool_constant<std::numeric_limits<T>::digits == 106>;

#ifndef FMT_USE_FULL_CACHE_DRAGONBOX
#  define FMT_USE_FULL_CACHE_DRAGONBOX 0
#endif

// An allocator that uses malloc/free to allow removing dependency on the C++
// standard libary runtime.
template <typename T> struct allocator {
  using value_type = T;

  T* allocate(size_t n) {
    FMT_ASSERT(n <= max_value<size_t>() / sizeof(T), "");
    T* p = static_cast<T*>(malloc(n * sizeof(T)));
    if (!p) FMT_THROW(std::bad_alloc());
    return p;
  }

  void deallocate(T* p, size_t) { free(p); }
};

}  // namespace detail

FMT_BEGIN_EXPORT

// The number of characters to store in the basic_memory_buffer object itself
// to avoid dynamic memory allocation.
enum { inline_buffer_size = 500 };

/**
 * A dynamically growing memory buffer for trivially copyable/constructible
 * types with the first `SIZE` elements stored in the object itself. Most
 * commonly used via the `memory_buffer` alias for `char`.
 *
 * **Example**:
 *
 *     auto out = fmt::memory_buffer();
 *     fmt::format_to(std::back_inserter(out), "The answer is {}.", 42);
 *
 * This will append "The answer is 42." to `out`. The buffer content can be
 * converted to `std::string` with `to_string(out)`.
 */
template <typename T, size_t SIZE = inline_buffer_size,
          typename Allocator = detail::allocator<T>>
class basic_memory_buffer : public detail::buffer<T> {
 private:
  T store_[SIZE];

  // Don't inherit from Allocator to avoid generating type_info for it.
  FMT_NO_UNIQUE_ADDRESS Allocator alloc_;

  // Deallocate memory allocated by the buffer.
  FMT_CONSTEXPR20 void deallocate() {
    T* data = this->data();
    if (data != store_) alloc_.deallocate(data, this->capacity());
  }

  static FMT_CONSTEXPR20 void grow(detail::buffer<T>& buf, size_t size) {
    detail::abort_fuzzing_if(size > 5000);
    auto& self = static_cast<basic_memory_buffer&>(buf);
    const size_t max_size =
        std::allocator_traits<Allocator>::max_size(self.alloc_);
    size_t old_capacity = buf.capacity();
    size_t new_capacity = old_capacity + old_capacity / 2;
    if (size > new_capacity)
      new_capacity = size;
    else if (new_capacity > max_size)
      new_capacity = max_of(size, max_size);
    T* old_data = buf.data();
    T* new_data = self.alloc_.allocate(new_capacity);
    // Suppress a bogus -Wstringop-overflow in gcc 13.1 (#3481).
    detail::assume(buf.size() <= new_capacity);
    // The following code doesn't throw, so the raw pointer above doesn't leak.
    memcpy(new_data, old_data, buf.size() * sizeof(T));
    self.set(new_data, new_capacity);
    // deallocate must not throw according to the standard, but even if it does,
    // the buffer already uses the new storage and will deallocate it in
    // destructor.
    if (old_data != self.store_) self.alloc_.deallocate(old_data, old_capacity);
  }

 public:
  using value_type = T;
  using const_reference = const T&;

  FMT_CONSTEXPR explicit basic_memory_buffer(
      const Allocator& alloc = Allocator())
      : detail::buffer<T>(grow), alloc_(alloc) {
    this->set(store_, SIZE);
    if (detail::is_constant_evaluated()) detail::fill_n(store_, SIZE, T());
  }
  FMT_CONSTEXPR20 ~basic_memory_buffer() { deallocate(); }

 private:
  // Move data from other to this buffer.
  FMT_CONSTEXPR20 void move(basic_memory_buffer& other) {
    alloc_ = std::move(other.alloc_);
    T* data = other.data();
    size_t size = other.size(), capacity = other.capacity();
    if (data == other.store_) {
      this->set(store_, capacity);
      detail::copy<T>(other.store_, other.store_ + size, store_);
    } else {
      this->set(data, capacity);
      // Set pointer to the inline array so that delete is not called
      // when deallocating.
      other.set(other.store_, 0);
      other.clear();
    }
    this->resize(size);
  }

 public:
  /// Constructs a `basic_memory_buffer` object moving the content of the other
  /// object to it.
  FMT_CONSTEXPR20 basic_memory_buffer(basic_memory_buffer&& other) noexcept
      : detail::buffer<T>(grow) {
    move(other);
  }

  /// Moves the content of the other `basic_memory_buffer` object to this one.
  auto operator=(basic_memory_buffer&& other) noexcept -> basic_memory_buffer& {
    FMT_ASSERT(this != &other, "");
    deallocate();
    move(other);
    return *this;
  }

  // Returns a copy of the allocator associated with this buffer.
  auto get_allocator() const -> Allocator { return alloc_; }

  /// Resizes the buffer to contain `count` elements. If T is a POD type new
  /// elements may not be initialized.
  FMT_CONSTEXPR void resize(size_t count) { this->try_resize(count); }

  /// Increases the buffer capacity to `new_capacity`.
  void reserve(size_t new_capacity) { this->try_reserve(new_capacity); }

  using detail::buffer<T>::append;
  template <typename ContiguousRange>
  FMT_CONSTEXPR20 void append(const ContiguousRange& range) {
    append(range.data(), range.data() + range.size());
  }
};

using memory_buffer = basic_memory_buffer<char>;

template <size_t SIZE>
FMT_NODISCARD auto to_string(const basic_memory_buffer<char, SIZE>& buf)
    -> std::string {
  auto size = buf.size();
  detail::assume(size < std::string().max_size());
  return {buf.data(), size};
}

// A writer to a buffered stream. It doesn't own the underlying stream.
class writer {
 private:
  detail::buffer<char>* buf_;

  // We cannot create a file buffer in advance because any write to a FILE may
  // invalidate it.
  FILE* file_;

 public:
  inline writer(FILE* f) : buf_(nullptr), file_(f) {}
  inline writer(detail::buffer<char>& buf) : buf_(&buf) {}

  /// Formats `args` according to specifications in `fmt` and writes the
  /// output to the file.
  template <typename... T> void print(format_string<T...> fmt, T&&... args) {
    if (buf_)
      fmt::format_to(appender(*buf_), fmt, std::forward<T>(args)...);
    else
      fmt::print(file_, fmt, std::forward<T>(args)...);
  }
};

class string_buffer {
 private:
  std::string str_;
  detail::container_buffer<std::string> buf_;

 public:
  inline string_buffer() : buf_(str_) {}

  inline operator writer() { return buf_; }
  inline std::string& str() { return str_; }
};

template <typename T, size_t SIZE, typename Allocator>
struct is_contiguous<basic_memory_buffer<T, SIZE, Allocator>> : std::true_type {
};

// Suppress a misleading warning in older versions of clang.
FMT_PRAGMA_CLANG(diagnostic ignored "-Wweak-vtables")

/// An error reported from a formatting function.
class FMT_SO_VISIBILITY("default") format_error : public std::runtime_error {
 public:
  using std::runtime_error::runtime_error;
};

class loc_value;

FMT_END_EXPORT
namespace detail {
FMT_API auto write_console(int fd, string_view text) -> bool;
FMT_API void print(FILE*, string_view);
}  // namespace detail

namespace detail {
template <typename Char, size_t N> struct fixed_string {
  FMT_CONSTEXPR20 fixed_string(const Char (&s)[N]) {
    detail::copy<Char, const Char*, Char*>(static_cast<const Char*>(s), s + N,
                                           data);
  }
  Char data[N] = {};
};

// Converts a compile-time string to basic_string_view.
FMT_EXPORT template <typename Char, size_t N>
constexpr auto compile_string_to_view(const Char (&s)[N])
    -> basic_string_view<Char> {
  // Remove trailing NUL character if needed. Won't be present if this is used
  // with a raw character array (i.e. not defined as a string).
  return {s, N - (std::char_traits<Char>::to_int_type(s[N - 1]) == 0 ? 1 : 0)};
}
FMT_EXPORT template <typename Char>
constexpr auto compile_string_to_view(basic_string_view<Char> s)
    -> basic_string_view<Char> {
  return s;
}

// Returns true if value is negative, false otherwise.
// Same as `value < 0` but doesn't produce warnings if T is an unsigned type.
template <typename T, FMT_ENABLE_IF(is_signed<T>::value)>
constexpr auto is_negative(T value) -> bool {
  return value < 0;
}
template <typename T, FMT_ENABLE_IF(!is_signed<T>::value)>
constexpr auto is_negative(T) -> bool {
  return false;
}

// Smallest of uint32_t, uint64_t, uint128_t that is large enough to
// represent all values of an integral type T.
template <typename T>
using uint32_or_64_or_128_t =
    conditional_t<num_bits<T>() <= 32 && !FMT_REDUCE_INT_INSTANTIATIONS,
                  uint32_t,
                  conditional_t<num_bits<T>() <= 64, uint64_t, uint128_t>>;
template <typename T>
using uint64_or_128_t = conditional_t<num_bits<T>() <= 64, uint64_t, uint128_t>;

#define FMT_POWERS_OF_10(factor)                                  \
  factor * 10, (factor) * 100, (factor) * 1000, (factor) * 10000, \
      (factor) * 100000, (factor) * 1000000, (factor) * 10000000, \
      (factor) * 100000000, (factor) * 1000000000

// Converts value in the range [0, 100) to a string.
// GCC generates slightly better code when value is pointer-size.
inline auto digits2(size_t value) -> const char* {
  // Align data since unaligned access may be slower when crossing a
  // hardware-specific boundary.
  alignas(2) static const char data[] =
      "0001020304050607080910111213141516171819"
      "2021222324252627282930313233343536373839"
      "4041424344454647484950515253545556575859"
      "6061626364656667686970717273747576777879"
      "8081828384858687888990919293949596979899";
  return &data[value * 2];
}

template <typename Char> constexpr auto getsign(sign s) -> Char {
  return static_cast<char>(((' ' << 24) | ('+' << 16) | ('-' << 8)) >>
                           (static_cast<int>(s) * 8));
}

template <typename T> FMT_CONSTEXPR auto count_digits_fallback(T n) -> int {
  int count = 1;
  for (;;) {
    // Integer division is slow so do it for a group of four digits instead
    // of for every digit. The idea comes from the talk by Alexandrescu
    // "Three Optimization Tips for C++". See speed-test for a comparison.
    if (n < 10) return count;
    if (n < 100) return count + 1;
    if (n < 1000) return count + 2;
    if (n < 10000) return count + 3;
    n /= 10000u;
    count += 4;
  }
}
#if FMT_USE_INT128
FMT_CONSTEXPR inline auto count_digits(uint128_opt n) -> int {
  return count_digits_fallback(n);
}
#endif

#ifdef FMT_BUILTIN_CLZLL
// It is a separate function rather than a part of count_digits to workaround
// the lack of static constexpr in constexpr functions.
inline auto do_count_digits(uint64_t n) -> int {
  // This has comparable performance to the version by Kendall Willets
  // (https://github.com/fmtlib/format-benchmark/blob/master/digits10)
  // but uses smaller tables.
  // Maps bsr(n) to ceil(log10(pow(2, bsr(n) + 1) - 1)).
  static constexpr uint8_t bsr2log10[] = {
      1,  1,  1,  2,  2,  2,  3,  3,  3,  4,  4,  4,  4,  5,  5,  5,
      6,  6,  6,  7,  7,  7,  7,  8,  8,  8,  9,  9,  9,  10, 10, 10,
      10, 11, 11, 11, 12, 12, 12, 13, 13, 13, 13, 14, 14, 14, 15, 15,
      15, 16, 16, 16, 16, 17, 17, 17, 18, 18, 18, 19, 19, 19, 19, 20};
  auto t = bsr2log10[FMT_BUILTIN_CLZLL(n | 1) ^ 63];
  static constexpr const uint64_t zero_or_powers_of_10[] = {
      0, 0, FMT_POWERS_OF_10(1U), FMT_POWERS_OF_10(1000000000ULL),
      10000000000000000000ULL};
  return t - (n < zero_or_powers_of_10[t]);
}
#endif

// Returns the number of decimal digits in n. Leading zeros are not counted
// except for n == 0 in which case count_digits returns 1.
FMT_CONSTEXPR20 inline auto count_digits(uint64_t n) -> int {
#ifdef FMT_BUILTIN_CLZLL
  if (!is_constant_evaluated() && !FMT_OPTIMIZE_SIZE) return do_count_digits(n);
#endif
  return count_digits_fallback(n);
}

// Counts the number of digits in n. BITS = log2(radix).
template <int BITS, typename UInt>
FMT_CONSTEXPR auto count_digits(UInt n) -> int {
#ifdef FMT_BUILTIN_CLZ
  if (!is_constant_evaluated() && num_bits<UInt>() == 32)
    return (FMT_BUILTIN_CLZ(static_cast<uint32_t>(n) | 1) ^ 31) / BITS + 1;
#endif
  // Lambda avoids unreachable code warnings from NVHPC.
  return [](UInt m) {
    int num_digits = 0;
    do {
      ++num_digits;
    } while ((m >>= BITS) != 0);
    return num_digits;
  }(n);
}

#ifdef FMT_BUILTIN_CLZ
// It is a separate function rather than a part of count_digits to workaround
// the lack of static constexpr in constexpr functions.
FMT_INLINE auto do_count_digits(uint32_t n) -> int {
// An optimization by Kendall Willets from https://bit.ly/3uOIQrB.
// This increments the upper 32 bits (log10(T) - 1) when >= T is added.
#  define FMT_INC(T) (((sizeof(#T) - 1ull) << 32) - T)
  static constexpr uint64_t table[] = {
      FMT_INC(0),          FMT_INC(0),          FMT_INC(0),           // 8
      FMT_INC(10),         FMT_INC(10),         FMT_INC(10),          // 64
      FMT_INC(100),        FMT_INC(100),        FMT_INC(100),         // 512
      FMT_INC(1000),       FMT_INC(1000),       FMT_INC(1000),        // 4096
      FMT_INC(10000),      FMT_INC(10000),      FMT_INC(10000),       // 32k
      FMT_INC(100000),     FMT_INC(100000),     FMT_INC(100000),      // 256k
      FMT_INC(1000000),    FMT_INC(1000000),    FMT_INC(1000000),     // 2048k
      FMT_INC(10000000),   FMT_INC(10000000),   FMT_INC(10000000),    // 16M
      FMT_INC(100000000),  FMT_INC(100000000),  FMT_INC(100000000),   // 128M
      FMT_INC(1000000000), FMT_INC(1000000000), FMT_INC(1000000000),  // 1024M
      FMT_INC(1000000000), FMT_INC(1000000000)                        // 4B
  };
  auto inc = table[FMT_BUILTIN_CLZ(n | 1) ^ 31];
  return static_cast<int>((n + inc) >> 32);
}
#endif

// Optional version of count_digits for better performance on 32-bit platforms.
FMT_CONSTEXPR20 inline auto count_digits(uint32_t n) -> int {
#ifdef FMT_BUILTIN_CLZ
  if (!is_constant_evaluated() && !FMT_OPTIMIZE_SIZE) return do_count_digits(n);
#endif
  return count_digits_fallback(n);
}

template <typename Int> constexpr auto digits10() noexcept -> int {
  return std::numeric_limits<Int>::digits10;
}
template <> constexpr auto digits10<int128_opt>() noexcept -> int { return 38; }
template <> constexpr auto digits10<uint128_t>() noexcept -> int { return 38; }

template <typename Char> struct thousands_sep_result {
  std::string grouping;
  Char thousands_sep;
};

template <typename Char>
FMT_API auto thousands_sep_impl(locale_ref loc) -> thousands_sep_result<Char>;
template <typename Char>
inline auto thousands_sep(locale_ref loc) -> thousands_sep_result<Char> {
  auto result = thousands_sep_impl<char>(loc);
  return {result.grouping, Char(result.thousands_sep)};
}
template <>
inline auto thousands_sep(locale_ref loc) -> thousands_sep_result<wchar_t> {
  return thousands_sep_impl<wchar_t>(loc);
}

template <typename Char>
FMT_API auto decimal_point_impl(locale_ref loc) -> Char;
template <typename Char> inline auto decimal_point(locale_ref loc) -> Char {
  return Char(decimal_point_impl<char>(loc));
}
template <> inline auto decimal_point(locale_ref loc) -> wchar_t {
  return decimal_point_impl<wchar_t>(loc);
}

#ifndef FMT_HEADER_ONLY
FMT_BEGIN_EXPORT
extern template FMT_API auto thousands_sep_impl<char>(locale_ref)
    -> thousands_sep_result<char>;
extern template FMT_API auto thousands_sep_impl<wchar_t>(locale_ref)
    -> thousands_sep_result<wchar_t>;
extern template FMT_API auto decimal_point_impl(locale_ref) -> char;
extern template FMT_API auto decimal_point_impl(locale_ref) -> wchar_t;
FMT_END_EXPORT
#endif  // FMT_HEADER_ONLY

// Compares two characters for equality.
template <typename Char> auto equal2(const Char* lhs, const char* rhs) -> bool {
  return lhs[0] == Char(rhs[0]) && lhs[1] == Char(rhs[1]);
}
inline auto equal2(const char* lhs, const char* rhs) -> bool {
  return memcmp(lhs, rhs, 2) == 0;
}

// Writes a two-digit value to out.
template <typename Char>
FMT_CONSTEXPR20 FMT_INLINE void write2digits(Char* out, size_t value) {
  if (!is_constant_evaluated() && std::is_same<Char, char>::value &&
      !FMT_OPTIMIZE_SIZE) {
    memcpy(out, digits2(value), 2);
    return;
  }
  *out++ = static_cast<Char>('0' + value / 10);
  *out = static_cast<Char>('0' + value % 10);
}

// Formats a decimal unsigned integer value writing to out pointing to a buffer
// of specified size. The caller must ensure that the buffer is large enough.
template <typename Char, typename UInt>
FMT_CONSTEXPR20 auto do_format_decimal(Char* out, UInt value, int size)
    -> Char* {
  FMT_ASSERT(size >= count_digits(value), "invalid digit count");
  unsigned n = to_unsigned(size);
  while (value >= 100) {
    // Integer division is slow so do it for a group of two digits instead
    // of for every digit. The idea comes from the talk by Alexandrescu
    // "Three Optimization Tips for C++". See speed-test for a comparison.
    n -= 2;
    write2digits(out + n, static_cast<unsigned>(value % 100));
    value /= 100;
  }
  if (value >= 10) {
    n -= 2;
    write2digits(out + n, static_cast<unsigned>(value));
  } else {
    out[--n] = static_cast<Char>('0' + value);
  }
  return out + n;
}

template <typename Char, typename UInt>
FMT_CONSTEXPR FMT_INLINE auto format_decimal(Char* out, UInt value,
                                             int num_digits) -> Char* {
  do_format_decimal(out, value, num_digits);
  return out + num_digits;
}

template <typename Char, typename UInt, typename OutputIt,
          FMT_ENABLE_IF(!std::is_pointer<remove_cvref_t<OutputIt>>::value)>
FMT_CONSTEXPR auto format_decimal(OutputIt out, UInt value, int num_digits)
    -> OutputIt {
  if (auto ptr = to_pointer<Char>(out, to_unsigned(num_digits))) {
    do_format_decimal(ptr, value, num_digits);
    return out;
  }
  // Buffer is large enough to hold all digits (digits10 + 1).
  char buffer[digits10<UInt>() + 1];
  if (is_constant_evaluated()) fill_n(buffer, sizeof(buffer), '\0');
  do_format_decimal(buffer, value, num_digits);
  return copy_noinline<Char>(buffer, buffer + num_digits, out);
}

template <typename Char, typename UInt>
FMT_CONSTEXPR auto do_format_base2e(int base_bits, Char* out, UInt value,
                                    int size, bool upper = false) -> Char* {
  out += size;
  do {
    const char* digits = upper ? "0123456789ABCDEF" : "0123456789abcdef";
    unsigned digit = static_cast<unsigned>(value & ((1 << base_bits) - 1));
    *--out = static_cast<Char>(base_bits < 4 ? static_cast<char>('0' + digit)
                                             : digits[digit]);
  } while ((value >>= base_bits) != 0);
  return out;
}

// Formats an unsigned integer in the power of two base (binary, octal, hex).
template <typename Char, typename UInt>
FMT_CONSTEXPR auto format_base2e(int base_bits, Char* out, UInt value,
                                 int num_digits, bool upper = false) -> Char* {
  do_format_base2e(base_bits, out, value, num_digits, upper);
  return out + num_digits;
}

template <typename Char, typename OutputIt, typename UInt,
          FMT_ENABLE_IF(is_back_insert_iterator<OutputIt>::value)>
FMT_CONSTEXPR inline auto format_base2e(int base_bits, OutputIt out, UInt value,
                                        int num_digits, bool upper = false)
    -> OutputIt {
  if (auto ptr = to_pointer<Char>(out, to_unsigned(num_digits))) {
    format_base2e(base_bits, ptr, value, num_digits, upper);
    return out;
  }
  // Make buffer large enough for any base.
  char buffer[num_bits<UInt>()];
  if (is_constant_evaluated()) fill_n(buffer, sizeof(buffer), '\0');
  format_base2e(base_bits, buffer, value, num_digits, upper);
  return detail::copy_noinline<Char>(buffer, buffer + num_digits, out);
}

// A converter from UTF-8 to UTF-16.
class utf8_to_utf16 {
 private:
  basic_memory_buffer<wchar_t> buffer_;

 public:
  FMT_API explicit utf8_to_utf16(string_view s);
  inline operator basic_string_view<wchar_t>() const {
    return {&buffer_[0], size()};
  }
  inline auto size() const -> size_t { return buffer_.size() - 1; }
  inline auto c_str() const -> const wchar_t* { return &buffer_[0]; }
  inline auto str() const -> std::wstring { return {&buffer_[0], size()}; }
};

enum class to_utf8_error_policy { abort, replace };

// A converter from UTF-16/UTF-32 (host endian) to UTF-8.
template <typename WChar, typename Buffer = memory_buffer> class to_utf8 {
 private:
  Buffer buffer_;

 public:
  to_utf8() {}
  explicit to_utf8(basic_string_view<WChar> s,
                   to_utf8_error_policy policy = to_utf8_error_policy::abort) {
    static_assert(sizeof(WChar) == 2 || sizeof(WChar) == 4,
                  "Expect utf16 or utf32");
    if (!convert(s, policy))
      FMT_THROW(std::runtime_error(sizeof(WChar) == 2 ? "invalid utf16"
                                                      : "invalid utf32"));
  }
  operator string_view() const { return string_view(&buffer_[0], size()); }
  auto size() const -> size_t { return buffer_.size() - 1; }
  auto c_str() const -> const char* { return &buffer_[0]; }
  auto str() const -> std::string { return std::string(&buffer_[0], size()); }

  // Performs conversion returning a bool instead of throwing exception on
  // conversion error. This method may still throw in case of memory allocation
  // error.
  auto convert(basic_string_view<WChar> s,
               to_utf8_error_policy policy = to_utf8_error_policy::abort)
      -> bool {
    if (!convert(buffer_, s, policy)) return false;
    buffer_.push_back(0);
    return true;
  }
  static auto convert(Buffer& buf, basic_string_view<WChar> s,
                      to_utf8_error_policy policy = to_utf8_error_policy::abort)
      -> bool {
    for (auto p = s.begin(); p != s.end(); ++p) {
      uint32_t c = static_cast<uint32_t>(*p);
      if (sizeof(WChar) == 2 && c >= 0xd800 && c <= 0xdfff) {
        // Handle a surrogate pair.
        ++p;
        if (p == s.end() || (c & 0xfc00) != 0xd800 || (*p & 0xfc00) != 0xdc00) {
          if (policy == to_utf8_error_policy::abort) return false;
          buf.append(string_view("\xEF\xBF\xBD"));
          --p;
          continue;
        } else {
          c = (c << 10) + static_cast<uint32_t>(*p) - 0x35fdc00;
        }
      }
      if (c < 0x80) {
        buf.push_back(static_cast<char>(c));
      } else if (c < 0x800) {
        buf.push_back(static_cast<char>(0xc0 | (c >> 6)));
        buf.push_back(static_cast<char>(0x80 | (c & 0x3f)));
      } else if ((c >= 0x800 && c <= 0xd7ff) || (c >= 0xe000 && c <= 0xffff)) {
        buf.push_back(static_cast<char>(0xe0 | (c >> 12)));
        buf.push_back(static_cast<char>(0x80 | ((c & 0xfff) >> 6)));
        buf.push_back(static_cast<char>(0x80 | (c & 0x3f)));
      } else if (c >= 0x10000 && c <= 0x10ffff) {
        buf.push_back(static_cast<char>(0xf0 | (c >> 18)));
        buf.push_back(static_cast<char>(0x80 | ((c & 0x3ffff) >> 12)));
        buf.push_back(static_cast<char>(0x80 | ((c & 0xfff) >> 6)));
        buf.push_back(static_cast<char>(0x80 | (c & 0x3f)));
      } else {
        return false;
      }
    }
    return true;
  }
};

// Computes 128-bit result of multiplication of two 64-bit unsigned integers.
inline auto umul128(uint64_t x, uint64_t y) noexcept -> uint128_fallback {
#if FMT_USE_INT128
  auto p = static_cast<uint128_opt>(x) * static_cast<uint128_opt>(y);
  return {static_cast<uint64_t>(p >> 64), static_cast<uint64_t>(p)};
#elif defined(_MSC_VER) && defined(_M_X64)
  auto hi = uint64_t();
  auto lo = _umul128(x, y, &hi);
  return {hi, lo};
#else
  const uint64_t mask = static_cast<uint64_t>(max_value<uint32_t>());

  uint64_t a = x >> 32;
  uint64_t b = x & mask;
  uint64_t c = y >> 32;
  uint64_t d = y & mask;

  uint64_t ac = a * c;
  uint64_t bc = b * c;
  uint64_t ad = a * d;
  uint64_t bd = b * d;

  uint64_t intermediate = (bd >> 32) + (ad & mask) + (bc & mask);

  return {ac + (intermediate >> 32) + (ad >> 32) + (bc >> 32),
          (intermediate << 32) + (bd & mask)};
#endif
}

namespace dragonbox {
// Computes floor(log10(pow(2, e))) for e in [-2620, 2620] using the method from
// https://fmt.dev/papers/Dragonbox.pdf#page=28, section 6.1.
inline auto floor_log10_pow2(int e) noexcept -> int {
  FMT_ASSERT(e <= 2620 && e >= -2620, "too large exponent");
  static_assert((-1 >> 1) == -1, "right shift is not arithmetic");
  return (e * 315653) >> 20;
}

inline auto floor_log2_pow10(int e) noexcept -> int {
  FMT_ASSERT(e <= 1233 && e >= -1233, "too large exponent");
  return (e * 1741647) >> 19;
}

// Computes upper 64 bits of multiplication of two 64-bit unsigned integers.
inline auto umul128_upper64(uint64_t x, uint64_t y) noexcept -> uint64_t {
#if FMT_USE_INT128
  auto p = static_cast<uint128_opt>(x) * static_cast<uint128_opt>(y);
  return static_cast<uint64_t>(p >> 64);
#elif defined(_MSC_VER) && defined(_M_X64)
  return __umulh(x, y);
#else
  return umul128(x, y).high();
#endif
}

// Computes upper 128 bits of multiplication of a 64-bit unsigned integer and a
// 128-bit unsigned integer.
inline auto umul192_upper128(uint64_t x, uint128_fallback y) noexcept
    -> uint128_fallback {
  uint128_fallback r = umul128(x, y.high());
  r += umul128_upper64(x, y.low());
  return r;
}

FMT_API auto get_cached_power(int k) noexcept -> uint128_fallback;

// Type-specific information that Dragonbox uses.
template <typename T, typename Enable = void> struct float_info;

template <> struct float_info<float> {
  using carrier_uint = uint32_t;
  static const int exponent_bits = 8;
  static const int kappa = 1;
  static const int big_divisor = 100;
  static const int small_divisor = 10;
  static const int min_k = -31;
  static const int max_k = 46;
  static const int shorter_interval_tie_lower_threshold = -35;
  static const int shorter_interval_tie_upper_threshold = -35;
};

template <> struct float_info<double> {
  using carrier_uint = uint64_t;
  static const int exponent_bits = 11;
  static const int kappa = 2;
  static const int big_divisor = 1000;
  static const int small_divisor = 100;
  static const int min_k = -292;
  static const int max_k = 341;
  static const int shorter_interval_tie_lower_threshold = -77;
  static const int shorter_interval_tie_upper_threshold = -77;
};

// An 80- or 128-bit floating point number.
template <typename T>
struct float_info<T, enable_if_t<std::numeric_limits<T>::digits == 64 ||
                                 std::numeric_limits<T>::digits == 113 ||
                                 is_float128<T>::value>> {
  using carrier_uint = detail::uint128_t;
  static const int exponent_bits = 15;
};

// A double-double floating point number.
template <typename T>
struct float_info<T, enable_if_t<is_double_double<T>::value>> {
  using carrier_uint = detail::uint128_t;
};

template <typename T> struct decimal_fp {
  using significand_type = typename float_info<T>::carrier_uint;
  significand_type significand;
  int exponent;
};

template <typename T> FMT_API auto to_decimal(T x) noexcept -> decimal_fp<T>;
}  // namespace dragonbox

// Returns true iff Float has the implicit bit which is not stored.
template <typename Float> constexpr auto has_implicit_bit() -> bool {
  // An 80-bit FP number has a 64-bit significand an no implicit bit.
  return std::numeric_limits<Float>::digits != 64;
}

// Returns the number of significand bits stored in Float. The implicit bit is
// not counted since it is not stored.
template <typename Float> constexpr auto num_significand_bits() -> int {
  // std::numeric_limits may not support __float128.
  return is_float128<Float>() ? 112
                              : (std::numeric_limits<Float>::digits -
                                 (has_implicit_bit<Float>() ? 1 : 0));
}

template <typename Float>
constexpr auto exponent_mask() ->
    typename dragonbox::float_info<Float>::carrier_uint {
  using float_uint = typename dragonbox::float_info<Float>::carrier_uint;
  return ((float_uint(1) << dragonbox::float_info<Float>::exponent_bits) - 1)
         << num_significand_bits<Float>();
}
template <typename Float> constexpr auto exponent_bias() -> int {
  // std::numeric_limits may not support __float128.
  return is_float128<Float>() ? 16383
                              : std::numeric_limits<Float>::max_exponent - 1;
}

// Writes the exponent exp in the form "[+-]d{2,3}" to buffer.
template <typename Char, typename OutputIt>
FMT_CONSTEXPR auto write_exponent(int exp, OutputIt out) -> OutputIt {
  FMT_ASSERT(-10000 < exp && exp < 10000, "exponent out of range");
  if (exp < 0) {
    *out++ = static_cast<Char>('-');
    exp = -exp;
  } else {
    *out++ = static_cast<Char>('+');
  }
  auto uexp = static_cast<uint32_t>(exp);
  if (is_constant_evaluated()) {
    if (uexp < 10) *out++ = '0';
    return format_decimal<Char>(out, uexp, count_digits(uexp));
  }
  if (uexp >= 100u) {
    const char* top = digits2(uexp / 100);
    if (uexp >= 1000u) *out++ = static_cast<Char>(top[0]);
    *out++ = static_cast<Char>(top[1]);
    uexp %= 100;
  }
  const char* d = digits2(uexp);
  *out++ = static_cast<Char>(d[0]);
  *out++ = static_cast<Char>(d[1]);
  return out;
}

// A floating-point number f * pow(2, e) where F is an unsigned type.
template <typename F> struct basic_fp {
  F f;
  int e;

  static constexpr const int num_significand_bits =
      static_cast<int>(sizeof(F) * num_bits<unsigned char>());

  constexpr basic_fp() : f(0), e(0) {}
  constexpr basic_fp(uint64_t f_val, int e_val) : f(f_val), e(e_val) {}

  // Constructs fp from an IEEE754 floating-point number.
  template <typename Float> FMT_CONSTEXPR basic_fp(Float n) { assign(n); }

  // Assigns n to this and return true iff predecessor is closer than successor.
  template <typename Float, FMT_ENABLE_IF(!is_double_double<Float>::value)>
  FMT_CONSTEXPR auto assign(Float n) -> bool {
    static_assert(std::numeric_limits<Float>::digits <= 113, "unsupported FP");
    // Assume Float is in the format [sign][exponent][significand].
    using carrier_uint = typename dragonbox::float_info<Float>::carrier_uint;
    const auto num_float_significand_bits =
        detail::num_significand_bits<Float>();
    const auto implicit_bit = carrier_uint(1) << num_float_significand_bits;
    const auto significand_mask = implicit_bit - 1;
    auto u = bit_cast<carrier_uint>(n);
    f = static_cast<F>(u & significand_mask);
    auto biased_e = static_cast<int>((u & exponent_mask<Float>()) >>
                                     num_float_significand_bits);
    // The predecessor is closer if n is a normalized power of 2 (f == 0)
    // other than the smallest normalized number (biased_e > 1).
    auto is_predecessor_closer = f == 0 && biased_e > 1;
    if (biased_e == 0)
      biased_e = 1;  // Subnormals use biased exponent 1 (min exponent).
    else if (has_implicit_bit<Float>())
      f += static_cast<F>(implicit_bit);
    e = biased_e - exponent_bias<Float>() - num_float_significand_bits;
    if (!has_implicit_bit<Float>()) ++e;
    return is_predecessor_closer;
  }

  template <typename Float, FMT_ENABLE_IF(is_double_double<Float>::value)>
  FMT_CONSTEXPR auto assign(Float n) -> bool {
    static_assert(std::numeric_limits<double>::is_iec559, "unsupported FP");
    return assign(static_cast<double>(n));
  }
};

using fp = basic_fp<unsigned long long>;

// Normalizes the value converted from double and multiplied by (1 << SHIFT).
template <int SHIFT = 0, typename F>
FMT_CONSTEXPR auto normalize(basic_fp<F> value) -> basic_fp<F> {
  // Handle subnormals.
  const auto implicit_bit = F(1) << num_significand_bits<double>();
  const auto shifted_implicit_bit = implicit_bit << SHIFT;
  while ((value.f & shifted_implicit_bit) == 0) {
    value.f <<= 1;
    --value.e;
  }
  // Subtract 1 to account for hidden bit.
  const auto offset = basic_fp<F>::num_significand_bits -
                      num_significand_bits<double>() - SHIFT - 1;
  value.f <<= offset;
  value.e -= offset;
  return value;
}

// Computes lhs * rhs / pow(2, 64) rounded to nearest with half-up tie breaking.
FMT_CONSTEXPR inline auto multiply(uint64_t lhs, uint64_t rhs) -> uint64_t {
#if FMT_USE_INT128
  auto product = static_cast<__uint128_t>(lhs) * rhs;
  auto f = static_cast<uint64_t>(product >> 64);
  return (static_cast<uint64_t>(product) & (1ULL << 63)) != 0 ? f + 1 : f;
#else
  // Multiply 32-bit parts of significands.
  uint64_t mask = (1ULL << 32) - 1;
  uint64_t a = lhs >> 32, b = lhs & mask;
  uint64_t c = rhs >> 32, d = rhs & mask;
  uint64_t ac = a * c, bc = b * c, ad = a * d, bd = b * d;
  // Compute mid 64-bit of result and round.
  uint64_t mid = (bd >> 32) + (ad & mask) + (bc & mask) + (1U << 31);
  return ac + (ad >> 32) + (bc >> 32) + (mid >> 32);
#endif
}

FMT_CONSTEXPR inline auto operator*(fp x, fp y) -> fp {
  return {multiply(x.f, y.f), x.e + y.e + 64};
}

template <typename T, bool doublish = num_bits<T>() == num_bits<double>()>
using convert_float_result =
    conditional_t<std::is_same<T, float>::value || doublish, double, T>;

template <typename T>
constexpr auto convert_float(T value) -> convert_float_result<T> {
  return static_cast<convert_float_result<T>>(value);
}

template <typename Char, typename OutputIt>
FMT_CONSTEXPR FMT_NOINLINE auto fill(OutputIt it, size_t n,
                                     const basic_specs& specs) -> OutputIt {
  auto fill_size = specs.fill_size();
  if (fill_size == 1) return detail::fill_n(it, n, specs.fill_unit<Char>());
  if (const Char* data = specs.fill<Char>()) {
    for (size_t i = 0; i < n; ++i) it = copy<Char>(data, data + fill_size, it);
  }
  return it;
}

// Writes the output of f, padded according to format specifications in specs.
// size: output size in code units.
// width: output display width in (terminal) column positions.
template <typename Char, align default_align = align::left, typename OutputIt,
          typename F>
FMT_CONSTEXPR auto write_padded(OutputIt out, const format_specs& specs,
                                size_t size, size_t width, F&& f) -> OutputIt {
  static_assert(default_align == align::left || default_align == align::right,
                "");
  unsigned spec_width = to_unsigned(specs.width);
  size_t padding = spec_width > width ? spec_width - width : 0;
  // Shifts are encoded as string literals because static constexpr is not
  // supported in constexpr functions.
  auto* shifts =
      default_align == align::left ? "\x1f\x1f\x00\x01" : "\x00\x1f\x00\x01";
  size_t left_padding = padding >> shifts[static_cast<int>(specs.align())];
  size_t right_padding = padding - left_padding;
  auto it = reserve(out, size + padding * specs.fill_size());
  if (left_padding != 0) it = fill<Char>(it, left_padding, specs);
  it = f(it);
  if (right_padding != 0) it = fill<Char>(it, right_padding, specs);
  return base_iterator(out, it);
}

template <typename Char, align default_align = align::left, typename OutputIt,
          typename F>
constexpr auto write_padded(OutputIt out, const format_specs& specs,
                            size_t size, F&& f) -> OutputIt {
  return write_padded<Char, default_align>(out, specs, size, size, f);
}

template <typename Char, align default_align = align::left, typename OutputIt>
FMT_CONSTEXPR auto write_bytes(OutputIt out, string_view bytes,
                               const format_specs& specs = {}) -> OutputIt {
  return write_padded<Char, default_align>(
      out, specs, bytes.size(), [bytes](reserve_iterator<OutputIt> it) {
        const char* data = bytes.data();
        return copy<Char>(data, data + bytes.size(), it);
      });
}

template <typename Char, typename OutputIt, typename UIntPtr>
auto write_ptr(OutputIt out, UIntPtr value, const format_specs* specs)
    -> OutputIt {
  int num_digits = count_digits<4>(value);
  auto size = to_unsigned(num_digits) + size_t(2);
  auto write = [=](reserve_iterator<OutputIt> it) {
    *it++ = static_cast<Char>('0');
    *it++ = static_cast<Char>('x');
    return format_base2e<Char>(4, it, value, num_digits);
  };
  return specs ? write_padded<Char, align::right>(out, *specs, size, write)
               : base_iterator(out, write(reserve(out, size)));
}

// Returns true iff the code point cp is printable.
FMT_API auto is_printable(uint32_t cp) -> bool;

inline auto needs_escape(uint32_t cp) -> bool {
  if (cp < 0x20 || cp == 0x7f || cp == '"' || cp == '\\') return true;
  if (const_check(FMT_OPTIMIZE_SIZE > 1)) return false;
  return !is_printable(cp);
}

template <typename Char> struct find_escape_result {
  const Char* begin;
  const Char* end;
  uint32_t cp;
};

template <typename Char>
auto find_escape(const Char* begin, const Char* end)
    -> find_escape_result<Char> {
  for (; begin != end; ++begin) {
    uint32_t cp = static_cast<unsigned_char<Char>>(*begin);
    if (const_check(sizeof(Char) == 1) && cp >= 0x80) continue;
    if (needs_escape(cp)) return {begin, begin + 1, cp};
  }
  return {begin, nullptr, 0};
}

inline auto find_escape(const char* begin, const char* end)
    -> find_escape_result<char> {
  if (const_check(!use_utf8)) return find_escape<char>(begin, end);
  auto result = find_escape_result<char>{end, nullptr, 0};
  for_each_codepoint(string_view(begin, to_unsigned(end - begin)),
                     [&](uint32_t cp, string_view sv) {
                       if (needs_escape(cp)) {
                         result = {sv.begin(), sv.end(), cp};
                         return false;
                       }
                       return true;
                     });
  return result;
}

template <size_t width, typename Char, typename OutputIt>
auto write_codepoint(OutputIt out, char prefix, uint32_t cp) -> OutputIt {
  *out++ = static_cast<Char>('\\');
  *out++ = static_cast<Char>(prefix);
  Char buf[width];
  fill_n(buf, width, static_cast<Char>('0'));
  format_base2e(4, buf, cp, width);
  return copy<Char>(buf, buf + width, out);
}

template <typename OutputIt, typename Char>
auto write_escaped_cp(OutputIt out, const find_escape_result<Char>& escape)
    -> OutputIt {
  auto c = static_cast<Char>(escape.cp);
  switch (escape.cp) {
  case '\n':
    *out++ = static_cast<Char>('\\');
    c = static_cast<Char>('n');
    break;
  case '\r':
    *out++ = static_cast<Char>('\\');
    c = static_cast<Char>('r');
    break;
  case '\t':
    *out++ = static_cast<Char>('\\');
    c = static_cast<Char>('t');
    break;
  case '"':  FMT_FALLTHROUGH;
  case '\'': FMT_FALLTHROUGH;
  case '\\': *out++ = static_cast<Char>('\\'); break;
  default:
    if (escape.cp < 0x100) return write_codepoint<2, Char>(out, 'x', escape.cp);
    if (escape.cp < 0x10000)
      return write_codepoint<4, Char>(out, 'u', escape.cp);
    if (escape.cp < 0x110000)
      return write_codepoint<8, Char>(out, 'U', escape.cp);
    for (Char escape_char : basic_string_view<Char>(
             escape.begin, to_unsigned(escape.end - escape.begin))) {
      out = write_codepoint<2, Char>(out, 'x',
                                     static_cast<uint32_t>(escape_char) & 0xFF);
    }
    return out;
  }
  *out++ = c;
  return out;
}

template <typename Char, typename OutputIt>
auto write_escaped_string(OutputIt out, basic_string_view<Char> str)
    -> OutputIt {
  *out++ = static_cast<Char>('"');
  auto begin = str.begin(), end = str.end();
  do {
    auto escape = find_escape(begin, end);
    out = copy<Char>(begin, escape.begin, out);
    begin = escape.end;
    if (!begin) break;
    out = write_escaped_cp<OutputIt, Char>(out, escape);
  } while (begin != end);
  *out++ = static_cast<Char>('"');
  return out;
}

template <typename Char, typename OutputIt>
auto write_escaped_char(OutputIt out, Char v) -> OutputIt {
  Char v_array[1] = {v};
  *out++ = static_cast<Char>('\'');
  if ((needs_escape(static_cast<uint32_t>(v)) && v != static_cast<Char>('"')) ||
      v == static_cast<Char>('\'')) {
    out = write_escaped_cp(out,
                           find_escape_result<Char>{v_array, v_array + 1,
                                                    static_cast<uint32_t>(v)});
  } else {
    *out++ = v;
  }
  *out++ = static_cast<Char>('\'');
  return out;
}

template <typename Char, typename OutputIt>
FMT_CONSTEXPR auto write_char(OutputIt out, Char value,
                              const format_specs& specs) -> OutputIt {
  bool is_debug = specs.type() == presentation_type::debug;
  return write_padded<Char>(out, specs, 1, [=](reserve_iterator<OutputIt> it) {
    if (is_debug) return write_escaped_char(it, value);
    *it++ = value;
    return it;
  });
}
template <typename Char, typename OutputIt>
FMT_CONSTEXPR auto write(OutputIt out, Char value, const format_specs& specs,
                         locale_ref loc = {}) -> OutputIt {
  // char is formatted as unsigned char for consistency across platforms.
  using unsigned_type =
      conditional_t<std::is_same<Char, char>::value, unsigned char, unsigned>;
  return check_char_specs(specs)
             ? write_char<Char>(out, value, specs)
             : write<Char>(out, static_cast<unsigned_type>(value), specs, loc);
}

template <typename Char> class digit_grouping {
 private:
  std::string grouping_;
  std::basic_string<Char> thousands_sep_;

  struct next_state {
    std::string::const_iterator group;
    int pos;
  };
  auto initial_state() const -> next_state { return {grouping_.begin(), 0}; }

  // Returns the next digit group separator position.
  auto next(next_state& state) const -> int {
    if (thousands_sep_.empty()) return max_value<int>();
    if (state.group == grouping_.end()) return state.pos += grouping_.back();
    if (*state.group <= 0 || *state.group == max_value<char>())
      return max_value<int>();
    state.pos += *state.group++;
    return state.pos;
  }

 public:
  template <typename Locale,
            FMT_ENABLE_IF(std::is_same<Locale, locale_ref>::value)>
  explicit digit_grouping(Locale loc, bool localized = true) {
    if (!localized) return;
    auto sep = thousands_sep<Char>(loc);
    grouping_ = sep.grouping;
    if (sep.thousands_sep) thousands_sep_.assign(1, sep.thousands_sep);
  }
  digit_grouping(std::string grouping, std::basic_string<Char> sep)
      : grouping_(std::move(grouping)), thousands_sep_(std::move(sep)) {}

  auto has_separator() const -> bool { return !thousands_sep_.empty(); }

  auto count_separators(int num_digits) const -> int {
    int count = 0;
    auto state = initial_state();
    while (num_digits > next(state)) ++count;
    return count;
  }

  // Applies grouping to digits and write the output to out.
  template <typename Out, typename C>
  auto apply(Out out, basic_string_view<C> digits) const -> Out {
    auto num_digits = static_cast<int>(digits.size());
    auto separators = basic_memory_buffer<int>();
    separators.push_back(0);
    auto state = initial_state();
    while (int i = next(state)) {
      if (i >= num_digits) break;
      separators.push_back(i);
    }
    for (int i = 0, sep_index = static_cast<int>(separators.size() - 1);
         i < num_digits; ++i) {
      if (num_digits - i == separators[sep_index]) {
        out = copy<Char>(thousands_sep_.data(),
                         thousands_sep_.data() + thousands_sep_.size(), out);
        --sep_index;
      }
      *out++ = static_cast<Char>(digits[to_unsigned(i)]);
    }
    return out;
  }
};

FMT_CONSTEXPR inline void prefix_append(unsigned& prefix, unsigned value) {
  prefix |= prefix != 0 ? value << 8 : value;
  prefix += (1u + (value > 0xff ? 1 : 0)) << 24;
}

// Writes a decimal integer with digit grouping.
template <typename OutputIt, typename UInt, typename Char>
auto write_int(OutputIt out, UInt value, unsigned prefix,
               const format_specs& specs, const digit_grouping<Char>& grouping)
    -> OutputIt {
  static_assert(std::is_same<uint64_or_128_t<UInt>, UInt>::value, "");
  int num_digits = 0;
  auto buffer = memory_buffer();
  switch (specs.type()) {
  default: FMT_ASSERT(false, ""); FMT_FALLTHROUGH;
  case presentation_type::none:
  case presentation_type::dec:
    num_digits = count_digits(value);
    format_decimal<char>(appender(buffer), value, num_digits);
    break;
  case presentation_type::hex:
    if (specs.alt())
      prefix_append(prefix, unsigned(specs.upper() ? 'X' : 'x') << 8 | '0');
    num_digits = count_digits<4>(value);
    format_base2e<char>(4, appender(buffer), value, num_digits, specs.upper());
    break;
  case presentation_type::oct:
    num_digits = count_digits<3>(value);
    // Octal prefix '0' is counted as a digit, so only add it if precision
    // is not greater than the number of digits.
    if (specs.alt() && specs.precision <= num_digits && value != 0)
      prefix_append(prefix, '0');
    format_base2e<char>(3, appender(buffer), value, num_digits);
    break;
  case presentation_type::bin:
    if (specs.alt())
      prefix_append(prefix, unsigned(specs.upper() ? 'B' : 'b') << 8 | '0');
    num_digits = count_digits<1>(value);
    format_base2e<char>(1, appender(buffer), value, num_digits);
    break;
  case presentation_type::chr:
    return write_char<Char>(out, static_cast<Char>(value), specs);
  }

  unsigned size = (prefix != 0 ? prefix >> 24 : 0) + to_unsigned(num_digits) +
                  to_unsigned(grouping.count_separators(num_digits));
  return write_padded<Char, align::right>(
      out, specs, size, size, [&](reserve_iterator<OutputIt> it) {
        for (unsigned p = prefix & 0xffffff; p != 0; p >>= 8)
          *it++ = static_cast<Char>(p & 0xff);
        return grouping.apply(it, string_view(buffer.data(), buffer.size()));
      });
}

#if FMT_USE_LOCALE
// Writes a localized value.
FMT_API auto write_loc(appender out, loc_value value, const format_specs& specs,
                       locale_ref loc) -> bool;
#endif
template <typename OutputIt>
inline auto write_loc(OutputIt, const loc_value&, const format_specs&,
                      locale_ref) -> bool {
  return false;
}

template <typename UInt> struct write_int_arg {
  UInt abs_value;
  unsigned prefix;
};

template <typename T>
FMT_CONSTEXPR auto make_write_int_arg(T value, sign s)
    -> write_int_arg<uint32_or_64_or_128_t<T>> {
  auto prefix = 0u;
  auto abs_value = static_cast<uint32_or_64_or_128_t<T>>(value);
  if (is_negative(value)) {
    prefix = 0x01000000 | '-';
    abs_value = 0 - abs_value;
  } else {
    constexpr const unsigned prefixes[4] = {0, 0, 0x1000000u | '+',
                                            0x1000000u | ' '};
    prefix = prefixes[static_cast<int>(s)];
  }
  return {abs_value, prefix};
}

template <typename Char = char> struct loc_writer {
  basic_appender<Char> out;
  const format_specs& specs;
  std::basic_string<Char> sep;
  std::string grouping;
  std::basic_string<Char> decimal_point;

  template <typename T, FMT_ENABLE_IF(is_integer<T>::value)>
  auto operator()(T value) -> bool {
    auto arg = make_write_int_arg(value, specs.sign());
    write_int(out, static_cast<uint64_or_128_t<T>>(arg.abs_value), arg.prefix,
              specs, digit_grouping<Char>(grouping, sep));
    return true;
  }

  template <typename T, FMT_ENABLE_IF(!is_integer<T>::value)>
  auto operator()(T) -> bool {
    return false;
  }
};

// Size and padding computation separate from write_int to avoid template bloat.
struct size_padding {
  unsigned size;
  unsigned padding;

  FMT_CONSTEXPR size_padding(int num_digits, unsigned prefix,
                             const format_specs& specs)
      : size((prefix >> 24) + to_unsigned(num_digits)), padding(0) {
    if (specs.align() == align::numeric) {
      auto width = to_unsigned(specs.width);
      if (width > size) {
        padding = width - size;
        size = width;
      }
    } else if (specs.precision > num_digits) {
      size = (prefix >> 24) + to_unsigned(specs.precision);
      padding = to_unsigned(specs.precision - num_digits);
    }
  }
};

template <typename Char, typename OutputIt, typename T>
FMT_CONSTEXPR FMT_INLINE auto write_int(OutputIt out, write_int_arg<T> arg,
                                        const format_specs& specs) -> OutputIt {
  static_assert(std::is_same<T, uint32_or_64_or_128_t<T>>::value, "");

  constexpr int buffer_size = num_bits<T>();
  char buffer[buffer_size];
  if (is_constant_evaluated()) fill_n(buffer, buffer_size, '\0');
  const char* begin = nullptr;
  const char* end = buffer + buffer_size;

  auto abs_value = arg.abs_value;
  auto prefix = arg.prefix;
  switch (specs.type()) {
  default: FMT_ASSERT(false, ""); FMT_FALLTHROUGH;
  case presentation_type::none:
  case presentation_type::dec:
    begin = do_format_decimal(buffer, abs_value, buffer_size);
    break;
  case presentation_type::hex:
    begin = do_format_base2e(4, buffer, abs_value, buffer_size, specs.upper());
    if (specs.alt())
      prefix_append(prefix, unsigned(specs.upper() ? 'X' : 'x') << 8 | '0');
    break;
  case presentation_type::oct: {
    begin = do_format_base2e(3, buffer, abs_value, buffer_size);
    // Octal prefix '0' is counted as a digit, so only add it if precision
    // is not greater than the number of digits.
    auto num_digits = end - begin;
    if (specs.alt() && specs.precision <= num_digits && abs_value != 0)
      prefix_append(prefix, '0');
    break;
  }
  case presentation_type::bin:
    begin = do_format_base2e(1, buffer, abs_value, buffer_size);
    if (specs.alt())
      prefix_append(prefix, unsigned(specs.upper() ? 'B' : 'b') << 8 | '0');
    break;
  case presentation_type::chr:
    return write_char<Char>(out, static_cast<Char>(abs_value), specs);
  }

  // Write an integer in the format
  //   <left-padding><prefix><numeric-padding><digits><right-padding>
  // prefix contains chars in three lower bytes and the size in the fourth byte.
  int num_digits = static_cast<int>(end - begin);
  // Slightly faster check for specs.width == 0 && specs.precision == -1.
  if ((specs.width | (specs.precision + 1)) == 0) {
    auto it = reserve(out, to_unsigned(num_digits) + (prefix >> 24));
    for (unsigned p = prefix & 0xffffff; p != 0; p >>= 8)
      *it++ = static_cast<Char>(p & 0xff);
    return base_iterator(out, copy<Char>(begin, end, it));
  }
  auto sp = size_padding(num_digits, prefix, specs);
  unsigned padding = sp.padding;
  return write_padded<Char, align::right>(
      out, specs, sp.size, [=](reserve_iterator<OutputIt> it) {
        for (unsigned p = prefix & 0xffffff; p != 0; p >>= 8)
          *it++ = static_cast<Char>(p & 0xff);
        it = detail::fill_n(it, padding, static_cast<Char>('0'));
        return copy<Char>(begin, end, it);
      });
}

template <typename Char, typename OutputIt, typename T>
FMT_CONSTEXPR FMT_NOINLINE auto write_int_noinline(OutputIt out,
                                                   write_int_arg<T> arg,
                                                   const format_specs& specs)
    -> OutputIt {
  return write_int<Char>(out, arg, specs);
}

template <typename Char, typename T,
          FMT_ENABLE_IF(is_integral<T>::value &&
                        !std::is_same<T, bool>::value &&
                        !std::is_same<T, Char>::value)>
FMT_CONSTEXPR FMT_INLINE auto write(basic_appender<Char> out, T value,
                                    const format_specs& specs, locale_ref loc)
    -> basic_appender<Char> {
  if (specs.localized() && write_loc(out, value, specs, loc)) return out;
  return write_int_noinline<Char>(out, make_write_int_arg(value, specs.sign()),
                                  specs);
}

// An inlined version of write used in format string compilation.
template <typename Char, typename OutputIt, typename T,
          FMT_ENABLE_IF(is_integral<T>::value &&
                        !std::is_same<T, bool>::value &&
                        !std::is_same<T, Char>::value &&
                        !std::is_same<OutputIt, basic_appender<Char>>::value)>
FMT_CONSTEXPR FMT_INLINE auto write(OutputIt out, T value,
                                    const format_specs& specs, locale_ref loc)
    -> OutputIt {
  if (specs.localized() && write_loc(out, value, specs, loc)) return out;
  return write_int<Char>(out, make_write_int_arg(value, specs.sign()), specs);
}

template <typename Char, typename OutputIt>
FMT_CONSTEXPR auto write(OutputIt out, basic_string_view<Char> s,
                         const format_specs& specs) -> OutputIt {
  auto data = s.data();
  auto size = s.size();
  if (specs.precision >= 0 && to_unsigned(specs.precision) < size)
    size = code_point_index(s, to_unsigned(specs.precision));

  bool is_debug = specs.type() == presentation_type::debug;
  if (is_debug) {
    auto buf = counting_buffer<Char>();
    write_escaped_string(basic_appender<Char>(buf), s);
    size = buf.count();
  }

  size_t width = 0;
  if (specs.width != 0) {
    width =
        is_debug ? size : compute_width(basic_string_view<Char>(data, size));
  }
  return write_padded<Char>(
      out, specs, size, width, [=](reserve_iterator<OutputIt> it) {
        return is_debug ? write_escaped_string(it, s)
                        : copy<Char>(data, data + size, it);
      });
}
template <typename Char, typename OutputIt>
FMT_CONSTEXPR auto write(OutputIt out, basic_string_view<Char> s,
                         const format_specs& specs, locale_ref) -> OutputIt {
  return write<Char>(out, s, specs);
}
template <typename Char, typename OutputIt>
FMT_CONSTEXPR auto write(OutputIt out, const Char* s, const format_specs& specs,
                         locale_ref) -> OutputIt {
  if (specs.type() == presentation_type::pointer)
    return write_ptr<Char>(out, bit_cast<uintptr_t>(s), &specs);
  if (!s) report_error("string pointer is null");
  return write<Char>(out, basic_string_view<Char>(s), specs, {});
}

template <typename Char, typename OutputIt, typename T,
          FMT_ENABLE_IF(is_integral<T>::value &&
                        !std::is_same<T, bool>::value &&
                        !std::is_same<T, Char>::value)>
FMT_CONSTEXPR auto write(OutputIt out, T value) -> OutputIt {
  auto abs_value = static_cast<uint32_or_64_or_128_t<T>>(value);
  bool negative = is_negative(value);
  // Don't do -abs_value since it trips unsigned-integer-overflow sanitizer.
  if (negative) abs_value = ~abs_value + 1;
  int num_digits = count_digits(abs_value);
  auto size = (negative ? 1 : 0) + static_cast<size_t>(num_digits);
  if (auto ptr = to_pointer<Char>(out, size)) {
    if (negative) *ptr++ = static_cast<Char>('-');
    format_decimal<Char>(ptr, abs_value, num_digits);
    return out;
  }
  if (negative) *out++ = static_cast<Char>('-');
  return format_decimal<Char>(out, abs_value, num_digits);
}

template <typename Char>
FMT_CONSTEXPR auto parse_align(const Char* begin, const Char* end,
                               format_specs& specs) -> const Char* {
  FMT_ASSERT(begin != end, "");
  auto alignment = align::none;
  auto p = begin + code_point_length(begin);
  if (end - p <= 0) p = begin;
  for (;;) {
    switch (to_ascii(*p)) {
    case '<': alignment = align::left; break;
    case '>': alignment = align::right; break;
    case '^': alignment = align::center; break;
    }
    if (alignment != align::none) {
      if (p != begin) {
        auto c = *begin;
        if (c == '}') return begin;
        if (c == '{') {
          report_error("invalid fill character '{'");
          return begin;
        }
        specs.set_fill(basic_string_view<Char>(begin, to_unsigned(p - begin)));
        begin = p + 1;
      } else {
        ++begin;
      }
      break;
    } else if (p == begin) {
      break;
    }
    p = begin;
  }
  specs.set_align(alignment);
  return begin;
}

template <typename Char, typename OutputIt>
FMT_CONSTEXPR20 auto write_nonfinite(OutputIt out, bool isnan,
                                     format_specs specs, sign s) -> OutputIt {
  auto str =
      isnan ? (specs.upper() ? "NAN" : "nan") : (specs.upper() ? "INF" : "inf");
  constexpr size_t str_size = 3;
  auto size = str_size + (s != sign::none ? 1 : 0);
  // Replace '0'-padding with space for non-finite values.
  const bool is_zero_fill =
      specs.fill_size() == 1 && specs.fill_unit<Char>() == '0';
  if (is_zero_fill) specs.set_fill(' ');
  return write_padded<Char>(out, specs, size,
                            [=](reserve_iterator<OutputIt> it) {
                              if (s != sign::none)
                                *it++ = detail::getsign<Char>(s);
                              return copy<Char>(str, str + str_size, it);
                            });
}

// A decimal floating-point number significand * pow(10, exp).
struct big_decimal_fp {
  const char* significand;
  int significand_size;
  int exponent;
};

constexpr auto get_significand_size(const big_decimal_fp& f) -> int {
  return f.significand_size;
}
template <typename T>
inline auto get_significand_size(const dragonbox::decimal_fp<T>& f) -> int {
  return count_digits(f.significand);
}

template <typename Char, typename OutputIt>
constexpr auto write_significand(OutputIt out, const char* significand,
                                 int significand_size) -> OutputIt {
  return copy<Char>(significand, significand + significand_size, out);
}
template <typename Char, typename OutputIt, typename UInt>
inline auto write_significand(OutputIt out, UInt significand,
                              int significand_size) -> OutputIt {
  return format_decimal<Char>(out, significand, significand_size);
}
template <typename Char, typename OutputIt, typename T, typename Grouping>
FMT_CONSTEXPR20 auto write_significand(OutputIt out, T significand,
                                       int significand_size, int exponent,
                                       const Grouping& grouping) -> OutputIt {
  if (!grouping.has_separator()) {
    out = write_significand<Char>(out, significand, significand_size);
    return detail::fill_n(out, exponent, static_cast<Char>('0'));
  }
  auto buffer = memory_buffer();
  write_significand<char>(appender(buffer), significand, significand_size);
  detail::fill_n(appender(buffer), exponent, '0');
  return grouping.apply(out, string_view(buffer.data(), buffer.size()));
}

template <typename Char, typename UInt,
          FMT_ENABLE_IF(std::is_integral<UInt>::value)>
inline auto write_significand(Char* out, UInt significand, int significand_size,
                              int integral_size, Char decimal_point) -> Char* {
  if (!decimal_point) return format_decimal(out, significand, significand_size);
  out += significand_size + 1;
  Char* end = out;
  int floating_size = significand_size - integral_size;
  for (int i = floating_size / 2; i > 0; --i) {
    out -= 2;
    write2digits(out, static_cast<std::size_t>(significand % 100));
    significand /= 100;
  }
  if (floating_size % 2 != 0) {
    *--out = static_cast<Char>('0' + significand % 10);
    significand /= 10;
  }
  *--out = decimal_point;
  format_decimal(out - integral_size, significand, integral_size);
  return end;
}

template <typename OutputIt, typename UInt, typename Char,
          FMT_ENABLE_IF(!std::is_pointer<remove_cvref_t<OutputIt>>::value)>
inline auto write_significand(OutputIt out, UInt significand,
                              int significand_size, int integral_size,
                              Char decimal_point) -> OutputIt {
  // Buffer is large enough to hold digits (digits10 + 1) and a decimal point.
  Char buffer[digits10<UInt>() + 2];
  auto end = write_significand(buffer, significand, significand_size,
                               integral_size, decimal_point);
  return detail::copy_noinline<Char>(buffer, end, out);
}

template <typename OutputIt, typename Char>
FMT_CONSTEXPR auto write_significand(OutputIt out, const char* significand,
                                     int significand_size, int integral_size,
                                     Char decimal_point) -> OutputIt {
  out = detail::copy_noinline<Char>(significand, significand + integral_size,
                                    out);
  if (!decimal_point) return out;
  *out++ = decimal_point;
  return detail::copy_noinline<Char>(significand + integral_size,
                                     significand + significand_size, out);
}

template <typename OutputIt, typename Char, typename T, typename Grouping>
FMT_CONSTEXPR20 auto write_significand(OutputIt out, T significand,
                                       int significand_size, int integral_size,
                                       Char decimal_point,
                                       const Grouping& grouping) -> OutputIt {
  if (!grouping.has_separator()) {
    return write_significand(out, significand, significand_size, integral_size,
                             decimal_point);
  }
  auto buffer = basic_memory_buffer<Char>();
  write_significand(basic_appender<Char>(buffer), significand, significand_size,
                    integral_size, decimal_point);
  grouping.apply(
      out, basic_string_view<Char>(buffer.data(), to_unsigned(integral_size)));
  return detail::copy_noinline<Char>(buffer.data() + integral_size,
                                     buffer.end(), out);
}

template <typename Char, typename OutputIt, typename DecimalFP,
          typename Grouping = digit_grouping<Char>>
FMT_CONSTEXPR20 auto do_write_float(OutputIt out, const DecimalFP& f,
                                    const format_specs& specs, sign s,
                                    int exp_upper, locale_ref loc) -> OutputIt {
  auto significand = f.significand;
  int significand_size = get_significand_size(f);
  const Char zero = static_cast<Char>('0');
  size_t size = to_unsigned(significand_size) + (s != sign::none ? 1 : 0);
  using iterator = reserve_iterator<OutputIt>;

  Char decimal_point = specs.localized() ? detail::decimal_point<Char>(loc)
                                         : static_cast<Char>('.');

  int output_exp = f.exponent + significand_size - 1;
  auto use_exp_format = [=]() {
    if (specs.type() == presentation_type::exp) return true;
    if (specs.type() == presentation_type::fixed) return false;
    // Use the fixed notation if the exponent is in [exp_lower, exp_upper),
    // e.g. 0.0001 instead of 1e-04. Otherwise use the exponent notation.
    const int exp_lower = -4;
    return output_exp < exp_lower ||
           output_exp >= (specs.precision > 0 ? specs.precision : exp_upper);
  };
  if (use_exp_format()) {
    int num_zeros = 0;
    if (specs.alt()) {
      num_zeros = specs.precision - significand_size;
      if (num_zeros < 0) num_zeros = 0;
      size += to_unsigned(num_zeros);
    } else if (significand_size == 1) {
      decimal_point = Char();
    }
    auto abs_output_exp = output_exp >= 0 ? output_exp : -output_exp;
    int exp_digits = 2;
    if (abs_output_exp >= 100) exp_digits = abs_output_exp >= 1000 ? 4 : 3;

    size += to_unsigned((decimal_point ? 1 : 0) + 2 + exp_digits);
    char exp_char = specs.upper() ? 'E' : 'e';
    auto write = [=](iterator it) {
      if (s != sign::none) *it++ = detail::getsign<Char>(s);
      // Insert a decimal point after the first digit and add an exponent.
      it = write_significand(it, significand, significand_size, 1,
                             decimal_point);
      if (num_zeros > 0) it = detail::fill_n(it, num_zeros, zero);
      *it++ = static_cast<Char>(exp_char);
      return write_exponent<Char>(output_exp, it);
    };
    return specs.width > 0
               ? write_padded<Char, align::right>(out, specs, size, write)
               : base_iterator(out, write(reserve(out, size)));
  }

  int exp = f.exponent + significand_size;
  if (f.exponent >= 0) {
    // 1234e5 -> 123400000[.0+]
    size += to_unsigned(f.exponent);
    int num_zeros = specs.precision - exp;
    abort_fuzzing_if(num_zeros > 5000);
    if (specs.alt()) {
      ++size;
      if (num_zeros <= 0 && specs.type() != presentation_type::fixed)
        num_zeros = 0;
      if (num_zeros > 0) size += to_unsigned(num_zeros);
    }
    auto grouping = Grouping(loc, specs.localized());
    size += to_unsigned(grouping.count_separators(exp));
    return write_padded<Char, align::right>(out, specs, size, [&](iterator it) {
      if (s != sign::none) *it++ = detail::getsign<Char>(s);
      it = write_significand<Char>(it, significand, significand_size,
                                   f.exponent, grouping);
      if (!specs.alt()) return it;
      *it++ = decimal_point;
      return num_zeros > 0 ? detail::fill_n(it, num_zeros, zero) : it;
    });
  } else if (exp > 0) {
    // 1234e-2 -> 12.34[0+]
    int num_zeros = specs.alt() ? specs.precision - significand_size : 0;
    size += 1 + static_cast<unsigned>(max_of(num_zeros, 0));
    auto grouping = Grouping(loc, specs.localized());
    size += to_unsigned(grouping.count_separators(exp));
    return write_padded<Char, align::right>(out, specs, size, [&](iterator it) {
      if (s != sign::none) *it++ = detail::getsign<Char>(s);
      it = write_significand(it, significand, significand_size, exp,
                             decimal_point, grouping);
      return num_zeros > 0 ? detail::fill_n(it, num_zeros, zero) : it;
    });
  }
  // 1234e-6 -> 0.001234
  int num_zeros = -exp;
  if (significand_size == 0 && specs.precision >= 0 &&
      specs.precision < num_zeros) {
    num_zeros = specs.precision;
  }
  bool pointy = num_zeros != 0 || significand_size != 0 || specs.alt();
  size += 1 + (pointy ? 1 : 0) + to_unsigned(num_zeros);
  return write_padded<Char, align::right>(out, specs, size, [&](iterator it) {
    if (s != sign::none) *it++ = detail::getsign<Char>(s);
    *it++ = zero;
    if (!pointy) return it;
    *it++ = decimal_point;
    it = detail::fill_n(it, num_zeros, zero);
    return write_significand<Char>(it, significand, significand_size);
  });
}

template <typename Char> class fallback_digit_grouping {
 public:
  constexpr fallback_digit_grouping(locale_ref, bool) {}

  constexpr auto has_separator() const -> bool { return false; }

  constexpr auto count_separators(int) const -> int { return 0; }

  template <typename Out, typename C>
  constexpr auto apply(Out out, basic_string_view<C>) const -> Out {
    return out;
  }
};

template <typename Char, typename OutputIt, typename DecimalFP>
FMT_CONSTEXPR20 auto write_float(OutputIt out, const DecimalFP& f,
                                 const format_specs& specs, sign s,
                                 int exp_upper, locale_ref loc) -> OutputIt {
  if (is_constant_evaluated()) {
    return do_write_float<Char, OutputIt, DecimalFP,
                          fallback_digit_grouping<Char>>(out, f, specs, s,
                                                         exp_upper, loc);
  } else {
    return do_write_float<Char>(out, f, specs, s, exp_upper, loc);
  }
}

template <typename T> constexpr auto isnan(T value) -> bool {
  return value != value;  // std::isnan doesn't support __float128.
}

template <typename T, typename Enable = void>
struct has_isfinite : std::false_type {};

template <typename T>
struct has_isfinite<T, enable_if_t<sizeof(std::isfinite(T())) != 0>>
    : std::true_type {};

template <typename T,
          FMT_ENABLE_IF(is_floating_point<T>::value&& has_isfinite<T>::value)>
FMT_CONSTEXPR20 auto isfinite(T value) -> bool {
  constexpr T inf = T(std::numeric_limits<double>::infinity());
  if (is_constant_evaluated())
    return !detail::isnan(value) && value < inf && value > -inf;
  return std::isfinite(value);
}
template <typename T, FMT_ENABLE_IF(!has_isfinite<T>::value)>
FMT_CONSTEXPR auto isfinite(T value) -> bool {
  T inf = T(std::numeric_limits<double>::infinity());
  // std::isfinite doesn't support __float128.
  return !detail::isnan(value) && value < inf && value > -inf;
}

template <typename T, FMT_ENABLE_IF(is_floating_point<T>::value)>
FMT_INLINE FMT_CONSTEXPR bool signbit(T value) {
  if (is_constant_evaluated()) {
#ifdef __cpp_if_constexpr
    if constexpr (std::numeric_limits<double>::is_iec559) {
      auto bits = detail::bit_cast<uint64_t>(static_cast<double>(value));
      return (bits >> (num_bits<uint64_t>() - 1)) != 0;
    }
#endif
  }
  return std::signbit(static_cast<double>(value));
}

inline FMT_CONSTEXPR20 void adjust_precision(int& precision, int exp10) {
  // Adjust fixed precision by exponent because it is relative to decimal
  // point.
  if (exp10 > 0 && precision > max_value<int>() - exp10)
    FMT_THROW(format_error("number is too big"));
  precision += exp10;
}

class bigint {
 private:
  // A bigint is a number in the form bigit_[N - 1] ... bigit_[0] * 32^exp_.
  using bigit = uint32_t;  // A big digit.
  using double_bigit = uint64_t;
  enum { bigit_bits = num_bits<bigit>() };
  enum { bigits_capacity = 32 };
  basic_memory_buffer<bigit, bigits_capacity> bigits_;
  int exp_;

  friend struct formatter<bigint>;

  FMT_CONSTEXPR auto get_bigit(int i) const -> bigit {
    return i >= exp_ && i < num_bigits() ? bigits_[i - exp_] : 0;
  }

  FMT_CONSTEXPR void subtract_bigits(int index, bigit other, bigit& borrow) {
    auto result = double_bigit(bigits_[index]) - other - borrow;
    bigits_[index] = static_cast<bigit>(result);
    borrow = static_cast<bigit>(result >> (bigit_bits * 2 - 1));
  }

  FMT_CONSTEXPR void remove_leading_zeros() {
    int num_bigits = static_cast<int>(bigits_.size()) - 1;
    while (num_bigits > 0 && bigits_[num_bigits] == 0) --num_bigits;
    bigits_.resize(to_unsigned(num_bigits + 1));
  }

  // Computes *this -= other assuming aligned bigints and *this >= other.
  FMT_CONSTEXPR void subtract_aligned(const bigint& other) {
    FMT_ASSERT(other.exp_ >= exp_, "unaligned bigints");
    FMT_ASSERT(compare(*this, other) >= 0, "");
    bigit borrow = 0;
    int i = other.exp_ - exp_;
    for (size_t j = 0, n = other.bigits_.size(); j != n; ++i, ++j)
      subtract_bigits(i, other.bigits_[j], borrow);
    if (borrow != 0) subtract_bigits(i, 0, borrow);
    FMT_ASSERT(borrow == 0, "");
    remove_leading_zeros();
  }

  FMT_CONSTEXPR void multiply(uint32_t value) {
    bigit carry = 0;
    const double_bigit wide_value = value;
    for (size_t i = 0, n = bigits_.size(); i < n; ++i) {
      double_bigit result = bigits_[i] * wide_value + carry;
      bigits_[i] = static_cast<bigit>(result);
      carry = static_cast<bigit>(result >> bigit_bits);
    }
    if (carry != 0) bigits_.push_back(carry);
  }

  template <typename UInt, FMT_ENABLE_IF(std::is_same<UInt, uint64_t>::value ||
                                         std::is_same<UInt, uint128_t>::value)>
  FMT_CONSTEXPR void multiply(UInt value) {
    using half_uint =
        conditional_t<std::is_same<UInt, uint128_t>::value, uint64_t, uint32_t>;
    const int shift = num_bits<half_uint>() - bigit_bits;
    const UInt lower = static_cast<half_uint>(value);
    const UInt upper = value >> num_bits<half_uint>();
    UInt carry = 0;
    for (size_t i = 0, n = bigits_.size(); i < n; ++i) {
      UInt result = lower * bigits_[i] + static_cast<bigit>(carry);
      carry = (upper * bigits_[i] << shift) + (result >> bigit_bits) +
              (carry >> bigit_bits);
      bigits_[i] = static_cast<bigit>(result);
    }
    while (carry != 0) {
      bigits_.push_back(static_cast<bigit>(carry));
      carry >>= bigit_bits;
    }
  }

  template <typename UInt, FMT_ENABLE_IF(std::is_same<UInt, uint64_t>::value ||
                                         std::is_same<UInt, uint128_t>::value)>
  FMT_CONSTEXPR void assign(UInt n) {
    size_t num_bigits = 0;
    do {
      bigits_[num_bigits++] = static_cast<bigit>(n);
      n >>= bigit_bits;
    } while (n != 0);
    bigits_.resize(num_bigits);
    exp_ = 0;
  }

 public:
  FMT_CONSTEXPR bigint() : exp_(0) {}
  explicit bigint(uint64_t n) { assign(n); }

  bigint(const bigint&) = delete;
  void operator=(const bigint&) = delete;

  FMT_CONSTEXPR void assign(const bigint& other) {
    auto size = other.bigits_.size();
    bigits_.resize(size);
    auto data = other.bigits_.data();
    copy<bigit>(data, data + size, bigits_.data());
    exp_ = other.exp_;
  }

  template <typename Int> FMT_CONSTEXPR void operator=(Int n) {
    FMT_ASSERT(n > 0, "");
    assign(uint64_or_128_t<Int>(n));
  }

  FMT_CONSTEXPR auto num_bigits() const -> int {
    return static_cast<int>(bigits_.size()) + exp_;
  }

  FMT_CONSTEXPR auto operator<<=(int shift) -> bigint& {
    FMT_ASSERT(shift >= 0, "");
    exp_ += shift / bigit_bits;
    shift %= bigit_bits;
    if (shift == 0) return *this;
    bigit carry = 0;
    for (size_t i = 0, n = bigits_.size(); i < n; ++i) {
      bigit c = bigits_[i] >> (bigit_bits - shift);
      bigits_[i] = (bigits_[i] << shift) + carry;
      carry = c;
    }
    if (carry != 0) bigits_.push_back(carry);
    return *this;
  }

  template <typename Int> FMT_CONSTEXPR auto operator*=(Int value) -> bigint& {
    FMT_ASSERT(value > 0, "");
    multiply(uint32_or_64_or_128_t<Int>(value));
    return *this;
  }

  friend FMT_CONSTEXPR auto compare(const bigint& b1, const bigint& b2) -> int {
    int num_bigits1 = b1.num_bigits(), num_bigits2 = b2.num_bigits();
    if (num_bigits1 != num_bigits2) return num_bigits1 > num_bigits2 ? 1 : -1;
    int i = static_cast<int>(b1.bigits_.size()) - 1;
    int j = static_cast<int>(b2.bigits_.size()) - 1;
    int end = i - j;
    if (end < 0) end = 0;
    for (; i >= end; --i, --j) {
      bigit b1_bigit = b1.bigits_[i], b2_bigit = b2.bigits_[j];
      if (b1_bigit != b2_bigit) return b1_bigit > b2_bigit ? 1 : -1;
    }
    if (i != j) return i > j ? 1 : -1;
    return 0;
  }

  // Returns compare(lhs1 + lhs2, rhs).
  friend FMT_CONSTEXPR auto add_compare(const bigint& lhs1, const bigint& lhs2,
                                        const bigint& rhs) -> int {
    int max_lhs_bigits = max_of(lhs1.num_bigits(), lhs2.num_bigits());
    int num_rhs_bigits = rhs.num_bigits();
    if (max_lhs_bigits + 1 < num_rhs_bigits) return -1;
    if (max_lhs_bigits > num_rhs_bigits) return 1;
    double_bigit borrow = 0;
    int min_exp = min_of(min_of(lhs1.exp_, lhs2.exp_), rhs.exp_);
    for (int i = num_rhs_bigits - 1; i >= min_exp; --i) {
      double_bigit sum = double_bigit(lhs1.get_bigit(i)) + lhs2.get_bigit(i);
      bigit rhs_bigit = rhs.get_bigit(i);
      if (sum > rhs_bigit + borrow) return 1;
      borrow = rhs_bigit + borrow - sum;
      if (borrow > 1) return -1;
      borrow <<= bigit_bits;
    }
    return borrow != 0 ? -1 : 0;
  }

  // Assigns pow(10, exp) to this bigint.
  FMT_CONSTEXPR20 void assign_pow10(int exp) {
    FMT_ASSERT(exp >= 0, "");
    if (exp == 0) return *this = 1;
    int bitmask = 1 << (num_bits<unsigned>() -
                        countl_zero(static_cast<uint32_t>(exp)) - 1);
    // pow(10, exp) = pow(5, exp) * pow(2, exp). First compute pow(5, exp) by
    // repeated squaring and multiplication.
    *this = 5;
    bitmask >>= 1;
    while (bitmask != 0) {
      square();
      if ((exp & bitmask) != 0) *this *= 5;
      bitmask >>= 1;
    }
    *this <<= exp;  // Multiply by pow(2, exp) by shifting.
  }

  FMT_CONSTEXPR20 void square() {
    int num_bigits = static_cast<int>(bigits_.size());
    int num_result_bigits = 2 * num_bigits;
    basic_memory_buffer<bigit, bigits_capacity> n(std::move(bigits_));
    bigits_.resize(to_unsigned(num_result_bigits));
    auto sum = uint128_t();
    for (int bigit_index = 0; bigit_index < num_bigits; ++bigit_index) {
      // Compute bigit at position bigit_index of the result by adding
      // cross-product terms n[i] * n[j] such that i + j == bigit_index.
      for (int i = 0, j = bigit_index; j >= 0; ++i, --j) {
        // Most terms are multiplied twice which can be optimized in the future.
        sum += double_bigit(n[i]) * n[j];
      }
      bigits_[bigit_index] = static_cast<bigit>(sum);
      sum >>= num_bits<bigit>();  // Compute the carry.
    }
    // Do the same for the top half.
    for (int bigit_index = num_bigits; bigit_index < num_result_bigits;
         ++bigit_index) {
      for (int j = num_bigits - 1, i = bigit_index - j; i < num_bigits;)
        sum += double_bigit(n[i++]) * n[j--];
      bigits_[bigit_index] = static_cast<bigit>(sum);
      sum >>= num_bits<bigit>();
    }
    remove_leading_zeros();
    exp_ *= 2;
  }

  // If this bigint has a bigger exponent than other, adds trailing zero to make
  // exponents equal. This simplifies some operations such as subtraction.
  FMT_CONSTEXPR void align(const bigint& other) {
    int exp_difference = exp_ - other.exp_;
    if (exp_difference <= 0) return;
    int num_bigits = static_cast<int>(bigits_.size());
    bigits_.resize(to_unsigned(num_bigits + exp_difference));
    for (int i = num_bigits - 1, j = i + exp_difference; i >= 0; --i, --j)
      bigits_[j] = bigits_[i];
    memset(bigits_.data(), 0, to_unsigned(exp_difference) * sizeof(bigit));
    exp_ -= exp_difference;
  }

  // Divides this bignum by divisor, assigning the remainder to this and
  // returning the quotient.
  FMT_CONSTEXPR auto divmod_assign(const bigint& divisor) -> int {
    FMT_ASSERT(this != &divisor, "");
    if (compare(*this, divisor) < 0) return 0;
    FMT_ASSERT(divisor.bigits_[divisor.bigits_.size() - 1u] != 0, "");
    align(divisor);
    int quotient = 0;
    do {
      subtract_aligned(divisor);
      ++quotient;
    } while (compare(*this, divisor) >= 0);
    return quotient;
  }
};

// format_dragon flags.
enum dragon {
  predecessor_closer = 1,
  fixup = 2,  // Run fixup to correct exp10 which can be off by one.
  fixed = 4,
};

// Formats a floating-point number using a variation of the Fixed-Precision
// Positive Floating-Point Printout ((FPP)^2) algorithm by Steele & White:
// https://fmt.dev/papers/p372-steele.pdf.
FMT_CONSTEXPR20 inline void format_dragon(basic_fp<uint128_t> value,
                                          unsigned flags, int num_digits,
                                          buffer<char>& buf, int& exp10) {
  bigint numerator;    // 2 * R in (FPP)^2.
  bigint denominator;  // 2 * S in (FPP)^2.
  // lower and upper are differences between value and corresponding boundaries.
  bigint lower;             // (M^- in (FPP)^2).
  bigint upper_store;       // upper's value if different from lower.
  bigint* upper = nullptr;  // (M^+ in (FPP)^2).
  // Shift numerator and denominator by an extra bit or two (if lower boundary
  // is closer) to make lower and upper integers. This eliminates multiplication
  // by 2 during later computations.
  bool is_predecessor_closer = (flags & dragon::predecessor_closer) != 0;
  int shift = is_predecessor_closer ? 2 : 1;
  if (value.e >= 0) {
    numerator = value.f;
    numerator <<= value.e + shift;
    lower = 1;
    lower <<= value.e;
    if (is_predecessor_closer) {
      upper_store = 1;
      upper_store <<= value.e + 1;
      upper = &upper_store;
    }
    denominator.assign_pow10(exp10);
    denominator <<= shift;
  } else if (exp10 < 0) {
    numerator.assign_pow10(-exp10);
    lower.assign(numerator);
    if (is_predecessor_closer) {
      upper_store.assign(numerator);
      upper_store <<= 1;
      upper = &upper_store;
    }
    numerator *= value.f;
    numerator <<= shift;
    denominator = 1;
    denominator <<= shift - value.e;
  } else {
    numerator = value.f;
    numerator <<= shift;
    denominator.assign_pow10(exp10);
    denominator <<= shift - value.e;
    lower = 1;
    if (is_predecessor_closer) {
      upper_store = 1ULL << 1;
      upper = &upper_store;
    }
  }
  int even = static_cast<int>((value.f & 1) == 0);
  if (!upper) upper = &lower;
  bool shortest = num_digits < 0;
  if ((flags & dragon::fixup) != 0) {
    if (add_compare(numerator, *upper, denominator) + even <= 0) {
      --exp10;
      numerator *= 10;
      if (num_digits < 0) {
        lower *= 10;
        if (upper != &lower) *upper *= 10;
      }
    }
    if ((flags & dragon::fixed) != 0) adjust_precision(num_digits, exp10 + 1);
  }
  // Invariant: value == (numerator / denominator) * pow(10, exp10).
  if (shortest) {
    // Generate the shortest representation.
    num_digits = 0;
    char* data = buf.data();
    for (;;) {
      int digit = numerator.divmod_assign(denominator);
      bool low = compare(numerator, lower) - even < 0;  // numerator <[=] lower.
      // numerator + upper >[=] pow10:
      bool high = add_compare(numerator, *upper, denominator) + even > 0;
      data[num_digits++] = static_cast<char>('0' + digit);
      if (low || high) {
        if (!low) {
          ++data[num_digits - 1];
        } else if (high) {
          int result = add_compare(numerator, numerator, denominator);
          // Round half to even.
          if (result > 0 || (result == 0 && (digit % 2) != 0))
            ++data[num_digits - 1];
        }
        buf.try_resize(to_unsigned(num_digits));
        exp10 -= num_digits - 1;
        return;
      }
      numerator *= 10;
      lower *= 10;
      if (upper != &lower) *upper *= 10;
    }
  }
  // Generate the given number of digits.
  exp10 -= num_digits - 1;
  if (num_digits <= 0) {
    auto digit = '0';
    if (num_digits == 0) {
      denominator *= 10;
      digit = add_compare(numerator, numerator, denominator) > 0 ? '1' : '0';
    }
    buf.push_back(digit);
    return;
  }
  buf.try_resize(to_unsigned(num_digits));
  for (int i = 0; i < num_digits - 1; ++i) {
    int digit = numerator.divmod_assign(denominator);
    buf[i] = static_cast<char>('0' + digit);
    numerator *= 10;
  }
  int digit = numerator.divmod_assign(denominator);
  auto result = add_compare(numerator, numerator, denominator);
  if (result > 0 || (result == 0 && (digit % 2) != 0)) {
    if (digit == 9) {
      const auto overflow = '0' + 10;
      buf[num_digits - 1] = overflow;
      // Propagate the carry.
      for (int i = num_digits - 1; i > 0 && buf[i] == overflow; --i) {
        buf[i] = '0';
        ++buf[i - 1];
      }
      if (buf[0] == overflow) {
        buf[0] = '1';
        if ((flags & dragon::fixed) != 0)
          buf.push_back('0');
        else
          ++exp10;
      }
      return;
    }
    ++digit;
  }
  buf[num_digits - 1] = static_cast<char>('0' + digit);
}

// Formats a floating-point number using the hexfloat format.
template <typename Float, FMT_ENABLE_IF(!is_double_double<Float>::value)>
FMT_CONSTEXPR20 void format_hexfloat(Float value, format_specs specs,
                                     buffer<char>& buf) {
  // float is passed as double to reduce the number of instantiations and to
  // simplify implementation.
  static_assert(!std::is_same<Float, float>::value, "");

  using info = dragonbox::float_info<Float>;

  // Assume Float is in the format [sign][exponent][significand].
  using carrier_uint = typename info::carrier_uint;

  const auto num_float_significand_bits = detail::num_significand_bits<Float>();

  basic_fp<carrier_uint> f(value);
  f.e += num_float_significand_bits;
  if (!has_implicit_bit<Float>()) --f.e;

  const auto num_fraction_bits =
      num_float_significand_bits + (has_implicit_bit<Float>() ? 1 : 0);
  const auto num_xdigits = (num_fraction_bits + 3) / 4;

  const auto leading_shift = ((num_xdigits - 1) * 4);
  const auto leading_mask = carrier_uint(0xF) << leading_shift;
  const auto leading_xdigit =
      static_cast<uint32_t>((f.f & leading_mask) >> leading_shift);
  if (leading_xdigit > 1) f.e -= (32 - countl_zero(leading_xdigit) - 1);

  int print_xdigits = num_xdigits - 1;
  if (specs.precision >= 0 && print_xdigits > specs.precision) {
    const int shift = ((print_xdigits - specs.precision - 1) * 4);
    const auto mask = carrier_uint(0xF) << shift;
    const auto v = static_cast<uint32_t>((f.f & mask) >> shift);

    if (v >= 8) {
      const auto inc = carrier_uint(1) << (shift + 4);
      f.f += inc;
      f.f &= ~(inc - 1);
    }

    // Check long double overflow
    if (!has_implicit_bit<Float>()) {
      const auto implicit_bit = carrier_uint(1) << num_float_significand_bits;
      if ((f.f & implicit_bit) == implicit_bit) {
        f.f >>= 4;
        f.e += 4;
      }
    }

    print_xdigits = specs.precision;
  }

  char xdigits[num_bits<carrier_uint>() / 4];
  detail::fill_n(xdigits, sizeof(xdigits), '0');
  format_base2e(4, xdigits, f.f, num_xdigits, specs.upper());

  // Remove zero tail
  while (print_xdigits > 0 && xdigits[print_xdigits] == '0') --print_xdigits;

  buf.push_back('0');
  buf.push_back(specs.upper() ? 'X' : 'x');
  buf.push_back(xdigits[0]);
  if (specs.alt() || print_xdigits > 0 || print_xdigits < specs.precision)
    buf.push_back('.');
  buf.append(xdigits + 1, xdigits + 1 + print_xdigits);
  for (; print_xdigits < specs.precision; ++print_xdigits) buf.push_back('0');

  buf.push_back(specs.upper() ? 'P' : 'p');

  uint32_t abs_e;
  if (f.e < 0) {
    buf.push_back('-');
    abs_e = static_cast<uint32_t>(-f.e);
  } else {
    buf.push_back('+');
    abs_e = static_cast<uint32_t>(f.e);
  }
  format_decimal<char>(appender(buf), abs_e, detail::count_digits(abs_e));
}

template <typename Float, FMT_ENABLE_IF(is_double_double<Float>::value)>
FMT_CONSTEXPR20 void format_hexfloat(Float value, format_specs specs,
                                     buffer<char>& buf) {
  format_hexfloat(static_cast<double>(value), specs, buf);
}

constexpr auto fractional_part_rounding_thresholds(int index) -> uint32_t {
  // For checking rounding thresholds.
  // The kth entry is chosen to be the smallest integer such that the
  // upper 32-bits of 10^(k+1) times it is strictly bigger than 5 * 10^k.
  // It is equal to ceil(2^31 + 2^32/10^(k + 1)).
  // These are stored in a string literal because we cannot have static arrays
  // in constexpr functions and non-static ones are poorly optimized.
  return U"\x9999999a\x828f5c29\x80418938\x80068db9\x8000a7c6\x800010c7"
         U"\x800001ae\x8000002b"[index];
}

template <typename Float>
FMT_CONSTEXPR20 auto format_float(Float value, int precision,
                                  const format_specs& specs, bool binary32,
                                  buffer<char>& buf) -> int {
  // float is passed as double to reduce the number of instantiations.
  static_assert(!std::is_same<Float, float>::value, "");
  auto converted_value = convert_float(value);

  const bool fixed = specs.type() == presentation_type::fixed;
  if (value == 0) {
    if (precision <= 0 || !fixed) {
      buf.push_back('0');
      return 0;
    }
    buf.try_resize(to_unsigned(precision));
    fill_n(buf.data(), precision, '0');
    return -precision;
  }

  int exp = 0;
  bool use_dragon = true;
  unsigned dragon_flags = 0;
  if (!is_fast_float<Float>() || is_constant_evaluated()) {
    const auto inv_log2_10 = 0.3010299956639812;  // 1 / log2(10)
    using info = dragonbox::float_info<decltype(converted_value)>;
    const auto f = basic_fp<typename info::carrier_uint>(converted_value);
    // Compute exp, an approximate power of 10, such that
    //   10^(exp - 1) <= value < 10^exp or 10^exp <= value < 10^(exp + 1).
    // This is based on log10(value) == log2(value) / log2(10) and approximation
    // of log2(value) by e + num_fraction_bits idea from double-conversion.
    auto e = (f.e + count_digits<1>(f.f) - 1) * inv_log2_10 - 1e-10;
    exp = static_cast<int>(e);
    if (e > exp) ++exp;  // Compute ceil.
    dragon_flags = dragon::fixup;
  } else {
    // Extract significand bits and exponent bits.
    using info = dragonbox::float_info<double>;
    auto br = bit_cast<uint64_t>(static_cast<double>(value));

    const uint64_t significand_mask =
        (static_cast<uint64_t>(1) << num_significand_bits<double>()) - 1;
    uint64_t significand = (br & significand_mask);
    int exponent = static_cast<int>((br & exponent_mask<double>()) >>
                                    num_significand_bits<double>());

    if (exponent != 0) {  // Check if normal.
      exponent -= exponent_bias<double>() + num_significand_bits<double>();
      significand |=
          (static_cast<uint64_t>(1) << num_significand_bits<double>());
      significand <<= 1;
    } else {
      // Normalize subnormal inputs.
      FMT_ASSERT(significand != 0, "zeros should not appear here");
      int shift = countl_zero(significand);
      FMT_ASSERT(shift >= num_bits<uint64_t>() - num_significand_bits<double>(),
                 "");
      shift -= (num_bits<uint64_t>() - num_significand_bits<double>() - 2);
      exponent = (std::numeric_limits<double>::min_exponent -
                  num_significand_bits<double>()) -
                 shift;
      significand <<= shift;
    }

    // Compute the first several nonzero decimal significand digits.
    // We call the number we get the first segment.
    const int k = info::kappa - dragonbox::floor_log10_pow2(exponent);
    exp = -k;
    const int beta = exponent + dragonbox::floor_log2_pow10(k);
    uint64_t first_segment;
    bool has_more_segments;
    int digits_in_the_first_segment;
    {
      const auto r = dragonbox::umul192_upper128(
          significand << beta, dragonbox::get_cached_power(k));
      first_segment = r.high();
      has_more_segments = r.low() != 0;

      // The first segment can have 18 ~ 19 digits.
      if (first_segment >= 1000000000000000000ULL) {
        digits_in_the_first_segment = 19;
      } else {
        // When it is of 18-digits, we align it to 19-digits by adding a bogus
        // zero at the end.
        digits_in_the_first_segment = 18;
        first_segment *= 10;
      }
    }

    // Compute the actual number of decimal digits to print.
    if (fixed) adjust_precision(precision, exp + digits_in_the_first_segment);

    // Use Dragon4 only when there might be not enough digits in the first
    // segment.
    if (digits_in_the_first_segment > precision) {
      use_dragon = false;

      if (precision <= 0) {
        exp += digits_in_the_first_segment;

        if (precision < 0) {
          // Nothing to do, since all we have are just leading zeros.
          buf.try_resize(0);
        } else {
          // We may need to round-up.
          buf.try_resize(1);
          if ((first_segment | static_cast<uint64_t>(has_more_segments)) >
              5000000000000000000ULL) {
            buf[0] = '1';
          } else {
            buf[0] = '0';
          }
        }
      }  // precision <= 0
      else {
        exp += digits_in_the_first_segment - precision;

        // When precision > 0, we divide the first segment into three
        // subsegments, each with 9, 9, and 0 ~ 1 digits so that each fits
        // in 32-bits which usually allows faster calculation than in
        // 64-bits. Since some compiler (e.g. MSVC) doesn't know how to optimize
        // division-by-constant for large 64-bit divisors, we do it here
        // manually. The magic number 7922816251426433760 below is equal to
        // ceil(2^(64+32) / 10^10).
        const uint32_t first_subsegment = static_cast<uint32_t>(
            dragonbox::umul128_upper64(first_segment, 7922816251426433760ULL) >>
            32);
        const uint64_t second_third_subsegments =
            first_segment - first_subsegment * 10000000000ULL;

        uint64_t prod;
        uint32_t digits;
        bool should_round_up;
        int number_of_digits_to_print = min_of(precision, 9);

        // Print a 9-digits subsegment, either the first or the second.
        auto print_subsegment = [&](uint32_t subsegment, char* buffer) {
          int number_of_digits_printed = 0;

          // If we want to print an odd number of digits from the subsegment,
          if ((number_of_digits_to_print & 1) != 0) {
            // Convert to 64-bit fixed-point fractional form with 1-digit
            // integer part. The magic number 720575941 is a good enough
            // approximation of 2^(32 + 24) / 10^8; see
            // https://jk-jeon.github.io/posts/2022/12/fixed-precision-formatting/#fixed-length-case
            // for details.
            prod = ((subsegment * static_cast<uint64_t>(720575941)) >> 24) + 1;
            digits = static_cast<uint32_t>(prod >> 32);
            *buffer = static_cast<char>('0' + digits);
            number_of_digits_printed++;
          }
          // If we want to print an even number of digits from the
          // first_subsegment,
          else {
            // Convert to 64-bit fixed-point fractional form with 2-digits
            // integer part. The magic number 450359963 is a good enough
            // approximation of 2^(32 + 20) / 10^7; see
            // https://jk-jeon.github.io/posts/2022/12/fixed-precision-formatting/#fixed-length-case
            // for details.
            prod = ((subsegment * static_cast<uint64_t>(450359963)) >> 20) + 1;
            digits = static_cast<uint32_t>(prod >> 32);
            write2digits(buffer, digits);
            number_of_digits_printed += 2;
          }

          // Print all digit pairs.
          while (number_of_digits_printed < number_of_digits_to_print) {
            prod = static_cast<uint32_t>(prod) * static_cast<uint64_t>(100);
            digits = static_cast<uint32_t>(prod >> 32);
            write2digits(buffer + number_of_digits_printed, digits);
            number_of_digits_printed += 2;
          }
        };

        // Print first subsegment.
        print_subsegment(first_subsegment, buf.data());

        // Perform rounding if the first subsegment is the last subsegment to
        // print.
        if (precision <= 9) {
          // Rounding inside the subsegment.
          // We round-up if:
          //  - either the fractional part is strictly larger than 1/2, or
          //  - the fractional part is exactly 1/2 and the last digit is odd.
          // We rely on the following observations:
          //  - If fractional_part >= threshold, then the fractional part is
          //    strictly larger than 1/2.
          //  - If the MSB of fractional_part is set, then the fractional part
          //    must be at least 1/2.
          //  - When the MSB of fractional_part is set, either
          //    second_third_subsegments being nonzero or has_more_segments
          //    being true means there are further digits not printed, so the
          //    fractional part is strictly larger than 1/2.
          if (precision < 9) {
            uint32_t fractional_part = static_cast<uint32_t>(prod);
            should_round_up =
                fractional_part >= fractional_part_rounding_thresholds(
                                       8 - number_of_digits_to_print) ||
                ((fractional_part >> 31) &
                 ((digits & 1) | (second_third_subsegments != 0) |
                  has_more_segments)) != 0;
          }
          // Rounding at the subsegment boundary.
          // In this case, the fractional part is at least 1/2 if and only if
          // second_third_subsegments >= 5000000000ULL, and is strictly larger
          // than 1/2 if we further have either second_third_subsegments >
          // 5000000000ULL or has_more_segments == true.
          else {
            should_round_up = second_third_subsegments > 5000000000ULL ||
                              (second_third_subsegments == 5000000000ULL &&
                               ((digits & 1) != 0 || has_more_segments));
          }
        }
        // Otherwise, print the second subsegment.
        else {
          // Compilers are not aware of how to leverage the maximum value of
          // second_third_subsegments to find out a better magic number which
          // allows us to eliminate an additional shift. 1844674407370955162 =
          // ceil(2^64/10) < ceil(2^64*(10^9/(10^10 - 1))).
          const uint32_t second_subsegment =
              static_cast<uint32_t>(dragonbox::umul128_upper64(
                  second_third_subsegments, 1844674407370955162ULL));
          const uint32_t third_subsegment =
              static_cast<uint32_t>(second_third_subsegments) -
              second_subsegment * 10;

          number_of_digits_to_print = precision - 9;
          print_subsegment(second_subsegment, buf.data() + 9);

          // Rounding inside the subsegment.
          if (precision < 18) {
            // The condition third_subsegment != 0 implies that the segment was
            // of 19 digits, so in this case the third segment should be
            // consisting of a genuine digit from the input.
            uint32_t fractional_part = static_cast<uint32_t>(prod);
            should_round_up =
                fractional_part >= fractional_part_rounding_thresholds(
                                       8 - number_of_digits_to_print) ||
                ((fractional_part >> 31) &
                 ((digits & 1) | (third_subsegment != 0) |
                  has_more_segments)) != 0;
          }
          // Rounding at the subsegment boundary.
          else {
            // In this case, the segment must be of 19 digits, thus
            // the third subsegment should be consisting of a genuine digit from
            // the input.
            should_round_up = third_subsegment > 5 ||
                              (third_subsegment == 5 &&
                               ((digits & 1) != 0 || has_more_segments));
          }
        }

        // Round-up if necessary.
        if (should_round_up) {
          ++buf[precision - 1];
          for (int i = precision - 1; i > 0 && buf[i] > '9'; --i) {
            buf[i] = '0';
            ++buf[i - 1];
          }
          if (buf[0] > '9') {
            buf[0] = '1';
            if (fixed)
              buf[precision++] = '0';
            else
              ++exp;
          }
        }
        buf.try_resize(to_unsigned(precision));
      }
    }  // if (digits_in_the_first_segment > precision)
    else {
      // Adjust the exponent for its use in Dragon4.
      exp += digits_in_the_first_segment - 1;
    }
  }
  if (use_dragon) {
    auto f = basic_fp<uint128_t>();
    bool is_predecessor_closer = binary32 ? f.assign(static_cast<float>(value))
                                          : f.assign(converted_value);
    if (is_predecessor_closer) dragon_flags |= dragon::predecessor_closer;
    if (fixed) dragon_flags |= dragon::fixed;
    // Limit precision to the maximum possible number of significant digits in
    // an IEEE754 double because we don't need to generate zeros.
    const int max_double_digits = 767;
    if (precision > max_double_digits) precision = max_double_digits;
    format_dragon(f, dragon_flags, precision, buf, exp);
  }
  if (!fixed && !specs.alt()) {
    // Remove trailing zeros.
    auto num_digits = buf.size();
    while (num_digits > 0 && buf[num_digits - 1] == '0') {
      --num_digits;
      ++exp;
    }
    buf.try_resize(num_digits);
  }
  return exp;
}

// Numbers with exponents greater or equal to the returned value will use
// the exponential notation.
template <typename T> constexpr auto exp_upper() -> int {
  return std::numeric_limits<T>::digits10 != 0
             ? min_of(16, std::numeric_limits<T>::digits10 + 1)
             : 16;
}

template <typename Char, typename OutputIt, typename T>
FMT_CONSTEXPR20 auto write_float(OutputIt out, T value, format_specs specs,
                                 locale_ref loc) -> OutputIt {
  // Use signbit because value < 0 is false for NaN.
  sign s = detail::signbit(value) ? sign::minus : specs.sign();

  if (!detail::isfinite(value))
    return write_nonfinite<Char>(out, detail::isnan(value), specs, s);

  if (specs.align() == align::numeric && s != sign::none) {
    *out++ = detail::getsign<Char>(s);
    s = sign::none;
    if (specs.width != 0) --specs.width;
  }

  constexpr int exp_upper = detail::exp_upper<T>();
  int precision = specs.precision;
  if (precision < 0) {
    if (specs.type() != presentation_type::none) {
      precision = 6;
    } else if (is_fast_float<T>::value && !is_constant_evaluated()) {
      // Use Dragonbox for the shortest format.
      using floaty = conditional_t<sizeof(T) >= sizeof(double), double, float>;
      auto dec = dragonbox::to_decimal(static_cast<floaty>(value));
      return write_float<Char>(out, dec, specs, s, exp_upper, loc);
    }
  }

  memory_buffer buffer;
  if (specs.type() == presentation_type::hexfloat) {
    if (s != sign::none) buffer.push_back(detail::getsign<char>(s));
    format_hexfloat(convert_float(value), specs, buffer);
    return write_bytes<Char, align::right>(out, {buffer.data(), buffer.size()},
                                           specs);
  }

  if (specs.type() == presentation_type::exp) {
    if (precision == max_value<int>())
      report_error("number is too big");
    else
      ++precision;
    if (specs.precision != 0) specs.set_alt();
  } else if (specs.type() == presentation_type::fixed) {
    if (specs.precision != 0) specs.set_alt();
  } else if (precision == 0) {
    precision = 1;
  }
  int exp = format_float(convert_float(value), precision, specs,
                         std::is_same<T, float>(), buffer);

  specs.precision = precision;
  auto f = big_decimal_fp{buffer.data(), static_cast<int>(buffer.size()), exp};
  return write_float<Char>(out, f, specs, s, exp_upper, loc);
}

template <typename Char, typename OutputIt, typename T,
          FMT_ENABLE_IF(is_floating_point<T>::value)>
FMT_CONSTEXPR20 auto write(OutputIt out, T value, format_specs specs,
                           locale_ref loc = {}) -> OutputIt {
  return specs.localized() && write_loc(out, value, specs, loc)
             ? out
             : write_float<Char>(out, value, specs, loc);
}

template <typename Char, typename OutputIt, typename T,
          FMT_ENABLE_IF(is_fast_float<T>::value)>
FMT_CONSTEXPR20 auto write(OutputIt out, T value) -> OutputIt {
  if (is_constant_evaluated()) return write<Char>(out, value, format_specs());

  auto s = detail::signbit(value) ? sign::minus : sign::none;

  constexpr auto specs = format_specs();
  using floaty = conditional_t<sizeof(T) >= sizeof(double), double, float>;
  using floaty_uint = typename dragonbox::float_info<floaty>::carrier_uint;
  floaty_uint mask = exponent_mask<floaty>();
  if ((bit_cast<floaty_uint>(value) & mask) == mask)
    return write_nonfinite<Char>(out, std::isnan(value), specs, s);

  auto dec = dragonbox::to_decimal(static_cast<floaty>(value));
  return write_float<Char>(out, dec, specs, s, exp_upper<T>(), {});
}

template <typename Char, typename OutputIt, typename T,
          FMT_ENABLE_IF(is_floating_point<T>::value &&
                        !is_fast_float<T>::value)>
inline auto write(OutputIt out, T value) -> OutputIt {
  return write<Char>(out, value, format_specs());
}

template <typename Char, typename OutputIt>
auto write(OutputIt out, monostate, format_specs = {}, locale_ref = {})
    -> OutputIt {
  FMT_ASSERT(false, "");
  return out;
}

template <typename Char, typename OutputIt>
FMT_CONSTEXPR auto write(OutputIt out, basic_string_view<Char> value)
    -> OutputIt {
  return copy_noinline<Char>(value.begin(), value.end(), out);
}

template <typename Char, typename OutputIt, typename T,
          FMT_ENABLE_IF(has_to_string_view<T>::value)>
constexpr auto write(OutputIt out, const T& value) -> OutputIt {
  return write<Char>(out, to_string_view(value));
}

// FMT_ENABLE_IF() condition separated to workaround an MSVC bug.
template <
    typename Char, typename OutputIt, typename T,
    bool check = std::is_enum<T>::value && !std::is_same<T, Char>::value &&
                 mapped_type_constant<T, Char>::value != type::custom_type,
    FMT_ENABLE_IF(check)>
FMT_CONSTEXPR auto write(OutputIt out, T value) -> OutputIt {
  return write<Char>(out, static_cast<underlying_t<T>>(value));
}

template <typename Char, typename OutputIt, typename T,
          FMT_ENABLE_IF(std::is_same<T, bool>::value)>
FMT_CONSTEXPR auto write(OutputIt out, T value, const format_specs& specs = {},
                         locale_ref = {}) -> OutputIt {
  return specs.type() != presentation_type::none &&
                 specs.type() != presentation_type::string
             ? write<Char>(out, value ? 1 : 0, specs, {})
             : write_bytes<Char>(out, value ? "true" : "false", specs);
}

template <typename Char, typename OutputIt>
FMT_CONSTEXPR auto write(OutputIt out, Char value) -> OutputIt {
  auto it = reserve(out, 1);
  *it++ = value;
  return base_iterator(out, it);
}

template <typename Char, typename OutputIt>
FMT_CONSTEXPR20 auto write(OutputIt out, const Char* value) -> OutputIt {
  if (value) return write(out, basic_string_view<Char>(value));
  report_error("string pointer is null");
  return out;
}

template <typename Char, typename OutputIt, typename T,
          FMT_ENABLE_IF(std::is_same<T, void>::value)>
auto write(OutputIt out, const T* value, const format_specs& specs = {},
           locale_ref = {}) -> OutputIt {
  return write_ptr<Char>(out, bit_cast<uintptr_t>(value), &specs);
}

template <typename Char, typename OutputIt, typename T,
          FMT_ENABLE_IF(mapped_type_constant<T, Char>::value ==
                            type::custom_type &&
                        !std::is_fundamental<T>::value)>
FMT_CONSTEXPR auto write(OutputIt out, const T& value) -> OutputIt {
  auto f = formatter<T, Char>();
  auto parse_ctx = parse_context<Char>({});
  f.parse(parse_ctx);
  auto ctx = basic_format_context<OutputIt, Char>(out, {}, {});
  return f.format(value, ctx);
}

template <typename T>
using is_builtin =
    bool_constant<std::is_same<T, int>::value || FMT_BUILTIN_TYPES>;

// An argument visitor that formats the argument and writes it via the output
// iterator. It's a class and not a generic lambda for compatibility with C++11.
template <typename Char> struct default_arg_formatter {
  using context = buffered_context<Char>;

  basic_appender<Char> out;

  void operator()(monostate) { report_error("argument not found"); }

  template <typename T, FMT_ENABLE_IF(is_builtin<T>::value)>
  void operator()(T value) {
    write<Char>(out, value);
  }

  template <typename T, FMT_ENABLE_IF(!is_builtin<T>::value)>
  void operator()(T) {
    FMT_ASSERT(false, "");
  }

  void operator()(typename basic_format_arg<context>::handle h) {
    // Use a null locale since the default format must be unlocalized.
    auto parse_ctx = parse_context<Char>({});
    auto format_ctx = context(out, {}, {});
    h.format(parse_ctx, format_ctx);
  }
};

template <typename Char> struct arg_formatter {
  basic_appender<Char> out;
  const format_specs& specs;
  FMT_NO_UNIQUE_ADDRESS locale_ref locale;

  template <typename T, FMT_ENABLE_IF(is_builtin<T>::value)>
  FMT_CONSTEXPR FMT_INLINE void operator()(T value) {
    detail::write<Char>(out, value, specs, locale);
  }

  template <typename T, FMT_ENABLE_IF(!is_builtin<T>::value)>
  void operator()(T) {
    FMT_ASSERT(false, "");
  }

  void operator()(typename basic_format_arg<buffered_context<Char>>::handle) {
    // User-defined types are handled separately because they require access
    // to the parse context.
  }
};

struct dynamic_spec_getter {
  template <typename T, FMT_ENABLE_IF(is_integer<T>::value)>
  FMT_CONSTEXPR auto operator()(T value) -> unsigned long long {
    return is_negative(value) ? ~0ull : static_cast<unsigned long long>(value);
  }

  template <typename T, FMT_ENABLE_IF(!is_integer<T>::value)>
  FMT_CONSTEXPR auto operator()(T) -> unsigned long long {
    report_error("width/precision is not integer");
    return 0;
  }
};

template <typename Context, typename ID>
FMT_CONSTEXPR auto get_arg(Context& ctx, ID id) -> basic_format_arg<Context> {
  auto arg = ctx.arg(id);
  if (!arg) report_error("argument not found");
  return arg;
}

template <typename Context>
FMT_CONSTEXPR int get_dynamic_spec(
    arg_id_kind kind, const arg_ref<typename Context::char_type>& ref,
    Context& ctx) {
  FMT_ASSERT(kind != arg_id_kind::none, "");
  auto arg =
      kind == arg_id_kind::index ? ctx.arg(ref.index) : ctx.arg(ref.name);
  if (!arg) report_error("argument not found");
  unsigned long long value = arg.visit(dynamic_spec_getter());
  if (value > to_unsigned(max_value<int>()))
    report_error("width/precision is out of range");
  return static_cast<int>(value);
}

template <typename Context>
FMT_CONSTEXPR void handle_dynamic_spec(
    arg_id_kind kind, int& value,
    const arg_ref<typename Context::char_type>& ref, Context& ctx) {
  if (kind != arg_id_kind::none) value = get_dynamic_spec(kind, ref, ctx);
}

#if FMT_USE_NONTYPE_TEMPLATE_ARGS
template <typename T, typename Char, size_t N,
          fmt::detail::fixed_string<Char, N> Str>
struct static_named_arg : view {
  static constexpr auto name = Str.data;

  const T& value;
  static_named_arg(const T& v) : value(v) {}
};

template <typename T, typename Char, size_t N,
          fmt::detail::fixed_string<Char, N> Str>
struct is_named_arg<static_named_arg<T, Char, N, Str>> : std::true_type {};

template <typename T, typename Char, size_t N,
          fmt::detail::fixed_string<Char, N> Str>
struct is_static_named_arg<static_named_arg<T, Char, N, Str>> : std::true_type {
};

template <typename Char, size_t N, fmt::detail::fixed_string<Char, N> Str>
struct udl_arg {
  template <typename T> auto operator=(T&& value) const {
    return static_named_arg<T, Char, N, Str>(std::forward<T>(value));
  }
};
#else
template <typename Char> struct udl_arg {
  const Char* str;

  template <typename T> auto operator=(T&& value) const -> named_arg<Char, T> {
    return {str, std::forward<T>(value)};
  }
};
#endif  // FMT_USE_NONTYPE_TEMPLATE_ARGS

template <typename Char> struct format_handler {
  parse_context<Char> parse_ctx;
  buffered_context<Char> ctx;

  void on_text(const Char* begin, const Char* end) {
    copy_noinline<Char>(begin, end, ctx.out());
  }

  FMT_CONSTEXPR auto on_arg_id() -> int { return parse_ctx.next_arg_id(); }
  FMT_CONSTEXPR auto on_arg_id(int id) -> int {
    parse_ctx.check_arg_id(id);
    return id;
  }
  FMT_CONSTEXPR auto on_arg_id(basic_string_view<Char> id) -> int {
    parse_ctx.check_arg_id(id);
    int arg_id = ctx.arg_id(id);
    if (arg_id < 0) report_error("argument not found");
    return arg_id;
  }

  FMT_INLINE void on_replacement_field(int id, const Char*) {
    ctx.arg(id).visit(default_arg_formatter<Char>{ctx.out()});
  }

  auto on_format_specs(int id, const Char* begin, const Char* end)
      -> const Char* {
    auto arg = get_arg(ctx, id);
    // Not using a visitor for custom types gives better codegen.
    if (arg.format_custom(begin, parse_ctx, ctx)) return parse_ctx.begin();

    auto specs = dynamic_format_specs<Char>();
    begin = parse_format_specs(begin, end, specs, parse_ctx, arg.type());
    if (specs.dynamic()) {
      handle_dynamic_spec(specs.dynamic_width(), specs.width, specs.width_ref,
                          ctx);
      handle_dynamic_spec(specs.dynamic_precision(), specs.precision,
                          specs.precision_ref, ctx);
    }

    arg.visit(arg_formatter<Char>{ctx.out(), specs, ctx.locale()});
    return begin;
  }

  FMT_NORETURN void on_error(const char* message) { report_error(message); }
};

using format_func = void (*)(detail::buffer<char>&, int, const char*);
FMT_API void do_report_error(format_func func, int error_code,
                             const char* message) noexcept;

FMT_API void format_error_code(buffer<char>& out, int error_code,
                               string_view message) noexcept;

template <typename T, typename Char, type TYPE>
template <typename FormatContext>
FMT_CONSTEXPR auto native_formatter<T, Char, TYPE>::format(
    const T& val, FormatContext& ctx) const -> decltype(ctx.out()) {
  if (!specs_.dynamic())
    return write<Char>(ctx.out(), val, specs_, ctx.locale());
  auto specs = format_specs(specs_);
  handle_dynamic_spec(specs.dynamic_width(), specs.width, specs_.width_ref,
                      ctx);
  handle_dynamic_spec(specs.dynamic_precision(), specs.precision,
                      specs_.precision_ref, ctx);
  return write<Char>(ctx.out(), val, specs, ctx.locale());
}

// DEPRECATED! https://github.com/fmtlib/fmt/issues/4292.
template <typename T, typename Enable = void>
struct is_locale : std::false_type {};
template <typename T>
struct is_locale<T, void_t<decltype(T::classic())>> : std::true_type {};

// DEPRECATED!
template <typename Char = char> struct vformat_args {
  using type = basic_format_args<buffered_context<Char>>;
};
template <> struct vformat_args<char> {
  using type = format_args;
};

template <typename Char>
void vformat_to(buffer<Char>& buf, basic_string_view<Char> fmt,
                typename vformat_args<Char>::type args, locale_ref loc = {}) {
  auto out = basic_appender<Char>(buf);
  parse_format_string(
      fmt, format_handler<Char>{parse_context<Char>(fmt), {out, args, loc}});
}
}  // namespace detail

FMT_BEGIN_EXPORT

// A generic formatting context with custom output iterator and character
// (code unit) support. Char is the format string code unit type which can be
// different from OutputIt::value_type.
template <typename OutputIt, typename Char> class generic_context {
 private:
  OutputIt out_;
  basic_format_args<generic_context> args_;
  detail::locale_ref loc_;

 public:
  using char_type = Char;
  using iterator = OutputIt;
  using parse_context_type FMT_DEPRECATED = parse_context<Char>;
  template <typename T>
  using formatter_type FMT_DEPRECATED = formatter<T, Char>;
  enum { builtin_types = FMT_BUILTIN_TYPES };

  constexpr generic_context(OutputIt out,
                            basic_format_args<generic_context> args,
                            detail::locale_ref loc = {})
      : out_(out), args_(args), loc_(loc) {}
  generic_context(generic_context&&) = default;
  generic_context(const generic_context&) = delete;
  void operator=(const generic_context&) = delete;

  constexpr auto arg(int id) const -> basic_format_arg<generic_context> {
    return args_.get(id);
  }
  auto arg(basic_string_view<Char> name) const
      -> basic_format_arg<generic_context> {
    return args_.get(name);
  }
  constexpr auto arg_id(basic_string_view<Char> name) const -> int {
    return args_.get_id(name);
  }

  constexpr auto out() const -> iterator { return out_; }

  void advance_to(iterator it) {
    if (!detail::is_back_insert_iterator<iterator>()) out_ = it;
  }

  constexpr auto locale() const -> detail::locale_ref { return loc_; }
};

class loc_value {
 private:
  basic_format_arg<context> value_;

 public:
  template <typename T, FMT_ENABLE_IF(!detail::is_float128<T>::value)>
  loc_value(T value) : value_(value) {}

  template <typename T, FMT_ENABLE_IF(detail::is_float128<T>::value)>
  loc_value(T) {}

  template <typename Visitor> auto visit(Visitor&& vis) -> decltype(vis(0)) {
    return value_.visit(vis);
  }
};

// A locale facet that formats values in UTF-8.
// It is parameterized on the locale to avoid the heavy <locale> include.
template <typename Locale> class format_facet : public Locale::facet {
 private:
  std::string separator_;
  std::string grouping_;
  std::string decimal_point_;

 protected:
  virtual auto do_put(appender out, loc_value val,
                      const format_specs& specs) const -> bool;

 public:
  static FMT_API typename Locale::id id;

  explicit format_facet(Locale& loc);
  explicit format_facet(string_view sep = "", std::string grouping = "\3",
                        std::string decimal_point = ".")
      : separator_(sep.data(), sep.size()),
        grouping_(grouping),
        decimal_point_(decimal_point) {}

  auto put(appender out, loc_value val, const format_specs& specs) const
      -> bool {
    return do_put(out, val, specs);
  }
};

#define FMT_FORMAT_AS(Type, Base)                                   \
  template <typename Char>                                          \
  struct formatter<Type, Char> : formatter<Base, Char> {            \
    template <typename FormatContext>                               \
    FMT_CONSTEXPR auto format(Type value, FormatContext& ctx) const \
        -> decltype(ctx.out()) {                                    \
      return formatter<Base, Char>::format(value, ctx);             \
    }                                                               \
  }

FMT_FORMAT_AS(signed char, int);
FMT_FORMAT_AS(unsigned char, unsigned);
FMT_FORMAT_AS(short, int);
FMT_FORMAT_AS(unsigned short, unsigned);
FMT_FORMAT_AS(long, detail::long_type);
FMT_FORMAT_AS(unsigned long, detail::ulong_type);
FMT_FORMAT_AS(Char*, const Char*);
FMT_FORMAT_AS(detail::std_string_view<Char>, basic_string_view<Char>);
FMT_FORMAT_AS(std::nullptr_t, const void*);
FMT_FORMAT_AS(void*, const void*);

template <typename Char, size_t N>
struct formatter<Char[N], Char> : formatter<basic_string_view<Char>, Char> {};

template <typename Char, typename Traits, typename Allocator>
class formatter<std::basic_string<Char, Traits, Allocator>, Char>
    : public formatter<basic_string_view<Char>, Char> {};

template <int N, typename Char>
struct formatter<detail::bitint<N>, Char> : formatter<long long, Char> {};
template <int N, typename Char>
struct formatter<detail::ubitint<N>, Char>
    : formatter<unsigned long long, Char> {};

template <typename Char>
struct formatter<detail::float128, Char>
    : detail::native_formatter<detail::float128, Char,
                               detail::type::float_type> {};

template <typename T, typename Char>
struct formatter<T, Char, void_t<detail::format_as_result<T>>>
    : formatter<detail::format_as_result<T>, Char> {
  template <typename FormatContext>
  FMT_CONSTEXPR auto format(const T& value, FormatContext& ctx) const
      -> decltype(ctx.out()) {
    auto&& val = format_as(value);  // Make an lvalue reference for format.
    return formatter<detail::format_as_result<T>, Char>::format(val, ctx);
  }
};

/**
 * Converts `p` to `const void*` for pointer formatting.
 *
 * **Example**:
 *
 *     auto s = fmt::format("{}", fmt::ptr(p));
 */
template <typename T> auto ptr(T p) -> const void* {
  static_assert(std::is_pointer<T>::value, "");
  return detail::bit_cast<const void*>(p);
}

/**
 * Converts `e` to the underlying type.
 *
 * **Example**:
 *
 *     enum class color { red, green, blue };
 *     auto s = fmt::format("{}", fmt::underlying(color::red));  // s == "0"
 */
template <typename Enum>
constexpr auto underlying(Enum e) noexcept -> underlying_t<Enum> {
  return static_cast<underlying_t<Enum>>(e);
}

namespace enums {
template <typename Enum, FMT_ENABLE_IF(std::is_enum<Enum>::value)>
constexpr auto format_as(Enum e) noexcept -> underlying_t<Enum> {
  return static_cast<underlying_t<Enum>>(e);
}
}  // namespace enums

#ifdef __cpp_lib_byte
template <> struct formatter<std::byte> : formatter<unsigned> {
  static auto format_as(std::byte b) -> unsigned char {
    return static_cast<unsigned char>(b);
  }
  template <typename Context>
  auto format(std::byte b, Context& ctx) const -> decltype(ctx.out()) {
    return formatter<unsigned>::format(format_as(b), ctx);
  }
};
#endif

struct bytes {
  string_view data;

  inline explicit bytes(string_view s) : data(s) {}
};

template <> struct formatter<bytes> {
 private:
  detail::dynamic_format_specs<> specs_;

 public:
  FMT_CONSTEXPR auto parse(parse_context<>& ctx) -> const char* {
    return parse_format_specs(ctx.begin(), ctx.end(), specs_, ctx,
                              detail::type::string_type);
  }

  template <typename FormatContext>
  auto format(bytes b, FormatContext& ctx) const -> decltype(ctx.out()) {
    auto specs = specs_;
    detail::handle_dynamic_spec(specs.dynamic_width(), specs.width,
                                specs.width_ref, ctx);
    detail::handle_dynamic_spec(specs.dynamic_precision(), specs.precision,
                                specs.precision_ref, ctx);
    return detail::write_bytes<char>(ctx.out(), b.data, specs);
  }
};

// group_digits_view is not derived from view because it copies the argument.
template <typename T> struct group_digits_view {
  T value;
};

/**
 * Returns a view that formats an integer value using ',' as a
 * locale-independent thousands separator.
 *
 * **Example**:
 *
 *     fmt::print("{}", fmt::group_digits(12345));
 *     // Output: "12,345"
 */
template <typename T> auto group_digits(T value) -> group_digits_view<T> {
  return {value};
}

template <typename T> struct formatter<group_digits_view<T>> : formatter<T> {
 private:
  detail::dynamic_format_specs<> specs_;

 public:
  FMT_CONSTEXPR auto parse(parse_context<>& ctx) -> const char* {
    return parse_format_specs(ctx.begin(), ctx.end(), specs_, ctx,
                              detail::type::int_type);
  }

  template <typename FormatContext>
  auto format(group_digits_view<T> view, FormatContext& ctx) const
      -> decltype(ctx.out()) {
    auto specs = specs_;
    detail::handle_dynamic_spec(specs.dynamic_width(), specs.width,
                                specs.width_ref, ctx);
    detail::handle_dynamic_spec(specs.dynamic_precision(), specs.precision,
                                specs.precision_ref, ctx);
    auto arg = detail::make_write_int_arg(view.value, specs.sign());
    return detail::write_int(
        ctx.out(), static_cast<detail::uint64_or_128_t<T>>(arg.abs_value),
        arg.prefix, specs, detail::digit_grouping<char>("\3", ","));
  }
};

template <typename T, typename Char> struct nested_view {
  const formatter<T, Char>* fmt;
  const T* value;
};

template <typename T, typename Char>
struct formatter<nested_view<T, Char>, Char> {
  FMT_CONSTEXPR auto parse(parse_context<Char>& ctx) -> const Char* {
    return ctx.begin();
  }
  template <typename FormatContext>
  auto format(nested_view<T, Char> view, FormatContext& ctx) const
      -> decltype(ctx.out()) {
    return view.fmt->format(*view.value, ctx);
  }
};

template <typename T, typename Char = char> struct nested_formatter {
 private:
  basic_specs specs_;
  int width_;
  formatter<T, Char> formatter_;

 public:
  constexpr nested_formatter() : width_(0) {}

  FMT_CONSTEXPR auto parse(parse_context<Char>& ctx) -> const Char* {
    auto it = ctx.begin(), end = ctx.end();
    if (it == end) return it;
    auto specs = format_specs();
    it = detail::parse_align(it, end, specs);
    specs_ = specs;
    Char c = *it;
    auto width_ref = detail::arg_ref<Char>();
    if ((c >= '0' && c <= '9') || c == '{') {
      it = detail::parse_width(it, end, specs, width_ref, ctx);
      width_ = specs.width;
    }
    ctx.advance_to(it);
    return formatter_.parse(ctx);
  }

  template <typename FormatContext, typename F>
  auto write_padded(FormatContext& ctx, F write) const -> decltype(ctx.out()) {
    if (width_ == 0) return write(ctx.out());
    auto buf = basic_memory_buffer<Char>();
    write(basic_appender<Char>(buf));
    auto specs = format_specs();
    specs.width = width_;
    specs.copy_fill_from(specs_);
    specs.set_align(specs_.align());
    return detail::write<Char>(
        ctx.out(), basic_string_view<Char>(buf.data(), buf.size()), specs);
  }

  auto nested(const T& value) const -> nested_view<T, Char> {
    return nested_view<T, Char>{&formatter_, &value};
  }
};

inline namespace literals {
#if FMT_USE_NONTYPE_TEMPLATE_ARGS
template <detail::fixed_string S> constexpr auto operator""_a() {
  using char_t = remove_cvref_t<decltype(*S.data)>;
  return detail::udl_arg<char_t, sizeof(S.data) / sizeof(char_t), S>();
}
#else
/**
 * User-defined literal equivalent of `fmt::arg`.
 *
 * **Example**:
 *
 *     using namespace fmt::literals;
 *     fmt::print("The answer is {answer}.", "answer"_a=42);
 */
constexpr auto operator""_a(const char* s, size_t) -> detail::udl_arg<char> {
  return {s};
}
#endif  // FMT_USE_NONTYPE_TEMPLATE_ARGS
}  // namespace literals

/// A fast integer formatter.
class format_int {
 private:
  // Buffer should be large enough to hold all digits (digits10 + 1),
  // a sign and a null character.
  enum { buffer_size = std::numeric_limits<unsigned long long>::digits10 + 3 };
  mutable char buffer_[buffer_size];
  char* str_;

  template <typename UInt>
  FMT_CONSTEXPR20 auto format_unsigned(UInt value) -> char* {
    auto n = static_cast<detail::uint32_or_64_or_128_t<UInt>>(value);
    return detail::do_format_decimal(buffer_, n, buffer_size - 1);
  }

  template <typename Int>
  FMT_CONSTEXPR20 auto format_signed(Int value) -> char* {
    auto abs_value = static_cast<detail::uint32_or_64_or_128_t<Int>>(value);
    bool negative = value < 0;
    if (negative) abs_value = 0 - abs_value;
    auto begin = format_unsigned(abs_value);
    if (negative) *--begin = '-';
    return begin;
  }

 public:
  FMT_CONSTEXPR20 explicit format_int(int value) : str_(format_signed(value)) {}
  FMT_CONSTEXPR20 explicit format_int(long value)
      : str_(format_signed(value)) {}
  FMT_CONSTEXPR20 explicit format_int(long long value)
      : str_(format_signed(value)) {}
  FMT_CONSTEXPR20 explicit format_int(unsigned value)
      : str_(format_unsigned(value)) {}
  FMT_CONSTEXPR20 explicit format_int(unsigned long value)
      : str_(format_unsigned(value)) {}
  FMT_CONSTEXPR20 explicit format_int(unsigned long long value)
      : str_(format_unsigned(value)) {}

  /// Returns the number of characters written to the output buffer.
  FMT_CONSTEXPR20 auto size() const -> size_t {
    return detail::to_unsigned(buffer_ - str_ + buffer_size - 1);
  }

  /// Returns a pointer to the output buffer content. No terminating null
  /// character is appended.
  FMT_CONSTEXPR20 auto data() const -> const char* { return str_; }

  /// Returns a pointer to the output buffer content with terminating null
  /// character appended.
  FMT_CONSTEXPR20 auto c_str() const -> const char* {
    buffer_[buffer_size - 1] = '\0';
    return str_;
  }

  /// Returns the content of the output buffer as an `std::string`.
  inline auto str() const -> std::string { return {str_, size()}; }
};

#define FMT_STRING_IMPL(s, base)                                              \
  [] {                                                                        \
    /* Use the hidden visibility as a workaround for a GCC bug (#1973). */    \
    /* Use a macro-like name to avoid shadowing warnings. */                  \
    struct FMT_VISIBILITY("hidden") FMT_COMPILE_STRING : base {               \
      using char_type = fmt::remove_cvref_t<decltype(s[0])>;                  \
      constexpr explicit operator fmt::basic_string_view<char_type>() const { \
        return fmt::detail::compile_string_to_view<char_type>(s);             \
      }                                                                       \
    };                                                                        \
    using FMT_STRING_VIEW =                                                   \
        fmt::basic_string_view<typename FMT_COMPILE_STRING::char_type>;       \
    fmt::detail::ignore_unused(FMT_STRING_VIEW(FMT_COMPILE_STRING()));        \
    return FMT_COMPILE_STRING();                                              \
  }()

/**
 * Constructs a legacy compile-time format string from a string literal `s`.
 *
 * **Example**:
 *
 *     // A compile-time error because 'd' is an invalid specifier for strings.
 *     std::string s = fmt::format(FMT_STRING("{:d}"), "foo");
 */
#define FMT_STRING(s) FMT_STRING_IMPL(s, fmt::detail::compile_string)

FMT_API auto vsystem_error(int error_code, string_view fmt, format_args args)
    -> std::system_error;

/**
 * Constructs `std::system_error` with a message formatted with
 * `fmt::format(fmt, args...)`.
 * `error_code` is a system error code as given by `errno`.
 *
 * **Example**:
 *
 *     // This throws std::system_error with the description
 *     //   cannot open file 'madeup': No such file or directory
 *     // or similar (system message may vary).
 *     const char* filename = "madeup";
 *     FILE* file = fopen(filename, "r");
 *     if (!file)
 *       throw fmt::system_error(errno, "cannot open file '{}'", filename);
 */
template <typename... T>
auto system_error(int error_code, format_string<T...> fmt, T&&... args)
    -> std::system_error {
  return vsystem_error(error_code, fmt.str, vargs<T...>{{args...}});
}

/**
 * Formats an error message for an error returned by an operating system or a
 * language runtime, for example a file opening error, and writes it to `out`.
 * The format is the same as the one used by `std::system_error(ec, message)`
 * where `ec` is `std::error_code(error_code, std::generic_category())`.
 * It is implementation-defined but normally looks like:
 *
 *     <message>: <system-message>
 *
 * where `<message>` is the passed message and `<system-message>` is the system
 * message corresponding to the error code.
 * `error_code` is a system error code as given by `errno`.
 */
FMT_API void format_system_error(detail::buffer<char>& out, int error_code,
                                 const char* message) noexcept;

// Reports a system error without throwing an exception.
// Can be used to report errors from destructors.
FMT_API void report_system_error(int error_code, const char* message) noexcept;

template <typename Locale, FMT_ENABLE_IF(detail::is_locale<Locale>::value)>
inline auto vformat(const Locale& loc, string_view fmt, format_args args)
    -> std::string {
  auto buf = memory_buffer();
  detail::vformat_to(buf, fmt, args, detail::locale_ref(loc));
  return {buf.data(), buf.size()};
}

template <typename Locale, typename... T,
          FMT_ENABLE_IF(detail::is_locale<Locale>::value)>
FMT_INLINE auto format(const Locale& loc, format_string<T...> fmt, T&&... args)
    -> std::string {
  return vformat(loc, fmt.str, vargs<T...>{{args...}});
}

template <typename OutputIt, typename Locale,
          FMT_ENABLE_IF(detail::is_output_iterator<OutputIt, char>::value)>
auto vformat_to(OutputIt out, const Locale& loc, string_view fmt,
                format_args args) -> OutputIt {
  auto&& buf = detail::get_buffer<char>(out);
  detail::vformat_to(buf, fmt, args, detail::locale_ref(loc));
  return detail::get_iterator(buf, out);
}

template <typename OutputIt, typename Locale, typename... T,
          FMT_ENABLE_IF(detail::is_output_iterator<OutputIt, char>::value&&
                            detail::is_locale<Locale>::value)>
FMT_INLINE auto format_to(OutputIt out, const Locale& loc,
                          format_string<T...> fmt, T&&... args) -> OutputIt {
  return fmt::vformat_to(out, loc, fmt.str, vargs<T...>{{args...}});
}

template <typename Locale, typename... T,
          FMT_ENABLE_IF(detail::is_locale<Locale>::value)>
FMT_NODISCARD FMT_INLINE auto formatted_size(const Locale& loc,
                                             format_string<T...> fmt,
                                             T&&... args) -> size_t {
  auto buf = detail::counting_buffer<>();
  detail::vformat_to(buf, fmt.str, vargs<T...>{{args...}},
                     detail::locale_ref(loc));
  return buf.count();
}

FMT_API auto vformat(string_view fmt, format_args args) -> std::string;

/**
 * Formats `args` according to specifications in `fmt` and returns the result
 * as a string.
 *
 * **Example**:
 *
 *     #include <fmt/format.h>
 *     std::string message = fmt::format("The answer is {}.", 42);
 */
template <typename... T>
FMT_NODISCARD FMT_INLINE auto format(format_string<T...> fmt, T&&... args)
    -> std::string {
  return vformat(fmt.str, vargs<T...>{{args...}});
}

/**
 * Converts `value` to `std::string` using the default format for type `T`.
 *
 * **Example**:
 *
 *     std::string answer = fmt::to_string(42);
 */
template <typename T, FMT_ENABLE_IF(std::is_integral<T>::value)>
FMT_NODISCARD auto to_string(T value) -> std::string {
  // The buffer should be large enough to store the number including the sign
  // or "false" for bool.
  char buffer[max_of(detail::digits10<T>() + 2, 5)];
  return {buffer, detail::write<char>(buffer, value)};
}

template <typename T, FMT_ENABLE_IF(detail::use_format_as<T>::value)>
FMT_NODISCARD auto to_string(const T& value) -> std::string {
  return to_string(format_as(value));
}

template <typename T, FMT_ENABLE_IF(!std::is_integral<T>::value &&
                                    !detail::use_format_as<T>::value)>
FMT_NODISCARD auto to_string(const T& value) -> std::string {
  auto buffer = memory_buffer();
  detail::write<char>(appender(buffer), value);
  return {buffer.data(), buffer.size()};
}

FMT_END_EXPORT
FMT_END_NAMESPACE

#ifdef FMT_HEADER_ONLY
#  define FMT_FUNC inline
#  include "format-inl.h"
#endif

// Restore _LIBCPP_REMOVE_TRANSITIVE_INCLUDES.
#ifdef FMT_REMOVE_TRANSITIVE_INCLUDES
#  undef _LIBCPP_REMOVE_TRANSITIVE_INCLUDES
#endif

#endif  // FMT_FORMAT_H_
