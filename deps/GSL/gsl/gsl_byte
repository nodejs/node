///////////////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2015 Microsoft Corporation. All rights reserved.
//
// This code is licensed under the MIT License (MIT).
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.
//
///////////////////////////////////////////////////////////////////////////////

#ifndef GSL_BYTE_H
#define GSL_BYTE_H

//
// make suppress attributes work for some compilers
// Hopefully temporary until suppresion standardization occurs
//
#if defined(_MSC_VER)
#define GSL_SUPPRESS(x) [[gsl::suppress(x)]]
#else
#if defined(__clang__)
#define GSL_SUPPRESS(x) [[gsl::suppress("x")]]
#else
#define GSL_SUPPRESS(x)
#endif // __clang__
#endif // _MSC_VER

#include <type_traits>

#ifdef _MSC_VER

#pragma warning(push)

// Turn MSVC /analyze rules that generate too much noise. TODO: fix in the tool.
#pragma warning(disable : 26493) // don't use c-style casts // TODO: MSVC suppression in templates does not always work

#ifndef GSL_USE_STD_BYTE
// this tests if we are under MSVC and the standard lib has std::byte and it is enabled
#if defined(_HAS_STD_BYTE) && _HAS_STD_BYTE

#define GSL_USE_STD_BYTE 1

#else // defined(_HAS_STD_BYTE) && _HAS_STD_BYTE

#define GSL_USE_STD_BYTE 0

#endif // defined(_HAS_STD_BYTE) && _HAS_STD_BYTE
#endif // GSL_USE_STD_BYTE

#else // _MSC_VER

#ifndef GSL_USE_STD_BYTE
// this tests if we are under GCC or Clang with enough -std:c++1z power to get us std::byte
// also check if libc++ version is sufficient (> 5.0) or libstc++ actually contains std::byte
#if defined(__cplusplus) && (__cplusplus >= 201703L) && \
  (defined(__cpp_lib_byte) && (__cpp_lib_byte >= 201603)  || \
   defined(_LIBCPP_VERSION) && (_LIBCPP_VERSION >= 5000))

#define GSL_USE_STD_BYTE 1
#include <cstddef>

#else // defined(__cplusplus) && (__cplusplus >= 201703L) &&
      //   (defined(__cpp_lib_byte) && (__cpp_lib_byte >= 201603)  ||
      //    defined(_LIBCPP_VERSION) && (_LIBCPP_VERSION >= 5000))

#define GSL_USE_STD_BYTE 0

#endif //defined(__cplusplus) && (__cplusplus >= 201703L) &&
       //   (defined(__cpp_lib_byte) && (__cpp_lib_byte >= 201603)  ||
       //    defined(_LIBCPP_VERSION) && (_LIBCPP_VERSION >= 5000))
#endif // GSL_USE_STD_BYTE

#endif // _MSC_VER

// Use __may_alias__ attribute on gcc and clang
#if defined __clang__ || (__GNUC__ > 5)
#define byte_may_alias __attribute__((__may_alias__))
#else // defined __clang__ || defined __GNUC__
#define byte_may_alias
#endif // defined __clang__ || defined __GNUC__

namespace gsl
{
#if GSL_USE_STD_BYTE

using std::byte;
using std::to_integer;

#else // GSL_USE_STD_BYTE

// This is a simple definition for now that allows
// use of byte within span<> to be standards-compliant
enum class byte_may_alias byte : unsigned char
{
};

template <class IntegerType, class = std::enable_if_t<std::is_integral<IntegerType>::value>>
constexpr byte& operator<<=(byte& b, IntegerType shift) noexcept
{
    return b = byte(static_cast<unsigned char>(b) << shift);
}

template <class IntegerType, class = std::enable_if_t<std::is_integral<IntegerType>::value>>
constexpr byte operator<<(byte b, IntegerType shift) noexcept
{
    return byte(static_cast<unsigned char>(b) << shift);
}

template <class IntegerType, class = std::enable_if_t<std::is_integral<IntegerType>::value>>
constexpr byte& operator>>=(byte& b, IntegerType shift) noexcept
{
    return b = byte(static_cast<unsigned char>(b) >> shift);
}

template <class IntegerType, class = std::enable_if_t<std::is_integral<IntegerType>::value>>
constexpr byte operator>>(byte b, IntegerType shift) noexcept
{
    return byte(static_cast<unsigned char>(b) >> shift);
}

constexpr byte& operator|=(byte& l, byte r) noexcept
{
    return l = byte(static_cast<unsigned char>(l) | static_cast<unsigned char>(r));
}

constexpr byte operator|(byte l, byte r) noexcept
{
    return byte(static_cast<unsigned char>(l) | static_cast<unsigned char>(r));
}

constexpr byte& operator&=(byte& l, byte r) noexcept
{
    return l = byte(static_cast<unsigned char>(l) & static_cast<unsigned char>(r));
}

constexpr byte operator&(byte l, byte r) noexcept
{
    return byte(static_cast<unsigned char>(l) & static_cast<unsigned char>(r));
}

constexpr byte& operator^=(byte& l, byte r) noexcept
{
    return l = byte(static_cast<unsigned char>(l) ^ static_cast<unsigned char>(r));
}

constexpr byte operator^(byte l, byte r) noexcept
{
    return byte(static_cast<unsigned char>(l) ^ static_cast<unsigned char>(r));
}

constexpr byte operator~(byte b) noexcept { return byte(~static_cast<unsigned char>(b)); }

template <class IntegerType, class = std::enable_if_t<std::is_integral<IntegerType>::value>>
constexpr IntegerType to_integer(byte b) noexcept
{
    return static_cast<IntegerType>(b);
}

#endif // GSL_USE_STD_BYTE

template <bool E, typename T>
constexpr byte to_byte_impl(T t) noexcept
{
    static_assert(
        E, "gsl::to_byte(t) must be provided an unsigned char, otherwise data loss may occur. "
           "If you are calling to_byte with an integer contant use: gsl::to_byte<t>() version.");
    return static_cast<byte>(t);
}
template <>
// NOTE: need suppression since c++14 does not allow "return {t}"
// GSL_SUPPRESS(type.4) // NO-FORMAT: attribute // TODO: suppression does not work
constexpr byte to_byte_impl<true, unsigned char>(unsigned char t) noexcept
{
    return byte(t);
}

template <typename T>
constexpr byte to_byte(T t) noexcept
{
    return to_byte_impl<std::is_same<T, unsigned char>::value, T>(t);
}

template <int I>
constexpr byte to_byte() noexcept
{
    static_assert(I >= 0 && I <= 255,
                  "gsl::byte only has 8 bits of storage, values must be in range 0-255");
    return static_cast<byte>(I);
}

} // namespace gsl

#ifdef _MSC_VER
#pragma warning(pop)
#endif // _MSC_VER

#endif // GSL_BYTE_H
