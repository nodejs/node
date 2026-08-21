// Copyright 2020-2024 Junekey Jeon
//
// The contents of this file may be used under the terms of
// the Apache License v2.0 with LLVM Exceptions.
//
//    (See accompanying file LICENSE-Apache or copy at
//     https://llvm.org/foundation/relicensing/LICENSE.txt)
//
// Alternatively, the contents of this file may be used under the terms of
// the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE-Boost or copy at
//     https://www.boost.org/LICENSE_1_0.txt)
//
// Unless required by applicable law or agreed to in writing, this software
// is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
// KIND, either express or implied.


#ifndef JKJ_HEADER_DRAGONBOX
#define JKJ_HEADER_DRAGONBOX

// Attribute for storing static data into a dedicated place, e.g. flash memory. Every ODR-used
// static data declaration will be decorated with this macro. The users may define this macro,
// before including the library headers, into whatever they want.
#ifndef JKJ_STATIC_DATA_SECTION
    #define JKJ_STATIC_DATA_SECTION
#else
    #define JKJ_STATIC_DATA_SECTION_DEFINED 1
#endif

// To use the library with toolchains without standard C++ headers, the users may define this macro
// into their custom namespace which contains the definitions of all the standard C++ library
// features used in this header. (The list can be found below.)
#ifndef JKJ_STD_REPLACEMENT_NAMESPACE
    #define JKJ_STD_REPLACEMENT_NAMESPACE std
    #include <cassert>
    #include <cstdint>
    #include <cstring>
    #include <limits>
    #include <type_traits>

    #ifdef __has_include
        #if __has_include(<version>)
            #include <version>
        #endif
    #endif
#else
    #define JKJ_STD_REPLACEMENT_NAMESPACE_DEFINED 1
#endif

////////////////////////////////////////////////////////////////////////////////////////
// Language feature detections.
////////////////////////////////////////////////////////////////////////////////////////

// C++14 constexpr
#if defined(__cpp_constexpr) && __cpp_constexpr >= 201304L
    #define JKJ_HAS_CONSTEXPR14 1
#elif __cplusplus >= 201402L
    #define JKJ_HAS_CONSTEXPR14 1
#elif defined(_MSC_VER) && _MSC_VER >= 1910 && _MSVC_LANG >= 201402L
    #define JKJ_HAS_CONSTEXPR14 1
#else
    #define JKJ_HAS_CONSTEXPR14 0
#endif

#if JKJ_HAS_CONSTEXPR14
    #define JKJ_CONSTEXPR14 constexpr
#else
    #define JKJ_CONSTEXPR14
#endif

// C++17 constexpr lambdas
#if defined(__cpp_constexpr) && __cpp_constexpr >= 201603L
    #define JKJ_HAS_CONSTEXPR17 1
#elif __cplusplus >= 201703L
    #define JKJ_HAS_CONSTEXPR17 1
#elif defined(_MSC_VER) && _MSC_VER >= 1911 && _MSVC_LANG >= 201703L
    #define JKJ_HAS_CONSTEXPR17 1
#else
    #define JKJ_HAS_CONSTEXPR17 0
#endif

// C++17 inline variables
#if defined(__cpp_inline_variables) && __cpp_inline_variables >= 201606L
    #define JKJ_HAS_INLINE_VARIABLE 1
#elif __cplusplus >= 201703L
    #define JKJ_HAS_INLINE_VARIABLE 1
#elif defined(_MSC_VER) && _MSC_VER >= 1912 && _MSVC_LANG >= 201703L
    #define JKJ_HAS_INLINE_VARIABLE 1
#else
    #define JKJ_HAS_INLINE_VARIABLE 0
#endif

#if JKJ_HAS_INLINE_VARIABLE
    #define JKJ_INLINE_VARIABLE inline constexpr
#else
    #define JKJ_INLINE_VARIABLE static constexpr
#endif

// C++17 if constexpr
#if defined(__cpp_if_constexpr) && __cpp_if_constexpr >= 201606L
    #define JKJ_HAS_IF_CONSTEXPR 1
#elif __cplusplus >= 201703L
    #define JKJ_HAS_IF_CONSTEXPR 1
#elif defined(_MSC_VER) && _MSC_VER >= 1911 && _MSVC_LANG >= 201703L
    #define JKJ_HAS_IF_CONSTEXPR 1
#else
    #define JKJ_HAS_IF_CONSTEXPR 0
#endif

#if JKJ_HAS_IF_CONSTEXPR
    #define JKJ_IF_CONSTEXPR if constexpr
#else
    #define JKJ_IF_CONSTEXPR if
#endif

// C++20 std::bit_cast
#if JKJ_STD_REPLACEMENT_NAMESPACE_DEFINED
    #if JKJ_STD_REPLACEMENT_HAS_BIT_CAST
        #define JKJ_HAS_BIT_CAST 1
    #else
        #define JKJ_HAS_BIT_CAST 0
    #endif
#elif defined(__cpp_lib_bit_cast) && __cpp_lib_bit_cast >= 201806L
    #include <bit>
    #define JKJ_HAS_BIT_CAST 1
#else
    #define JKJ_HAS_BIT_CAST 0
#endif

// C++23 if consteval or C++20 std::is_constant_evaluated
#if defined(__cpp_if_consteval) && __cpp_is_consteval >= 202106L
    #define JKJ_IF_CONSTEVAL if consteval
    #define JKJ_IF_NOT_CONSTEVAL if !consteval
    #define JKJ_CAN_BRANCH_ON_CONSTEVAL 1
    #define JKJ_USE_IS_CONSTANT_EVALUATED 0
#elif JKJ_STD_REPLACEMENT_NAMESPACE_DEFINED
    #if JKJ_STD_REPLACEMENT_HAS_IS_CONSTANT_EVALUATED
        #define JKJ_IF_CONSTEVAL if (stdr::is_constant_evaluated())
        #define JKJ_IF_NOT_CONSTEVAL if (!stdr::is_constant_evaluated())
        #define JKJ_CAN_BRANCH_ON_CONSTEVAL 1
        #define JKJ_USE_IS_CONSTANT_EVALUATED 1
    #elif JKJ_HAS_IF_CONSTEXPR
        #define JKJ_IF_CONSTEVAL if constexpr (false)
        #define JKJ_IF_NOT_CONSTEVAL if constexpr (true)
        #define JKJ_CAN_BRANCH_ON_CONSTEVAL 0
        #define JKJ_USE_IS_CONSTANT_EVALUATED 0
    #else
        #define JKJ_IF_CONSTEVAL if (false)
        #define JKJ_IF_NOT_CONSTEVAL if (true)
        #define JKJ_CAN_BRANCH_ON_CONSTEVAL 0
        #define JKJ_USE_IS_CONSTANT_EVALUATED 0
    #endif
#else
    #if defined(__cpp_lib_is_constant_evaluated) && __cpp_lib_is_constant_evaluated >= 201811L
        #define JKJ_IF_CONSTEVAL if (stdr::is_constant_evaluated())
        #define JKJ_IF_NOT_CONSTEVAL if (!stdr::is_constant_evaluated())
        #define JKJ_CAN_BRANCH_ON_CONSTEVAL 1
        #define JKJ_USE_IS_CONSTANT_EVALUATED 1
    #elif JKJ_HAS_IF_CONSTEXPR
        #define JKJ_IF_CONSTEVAL if constexpr (false)
        #define JKJ_IF_NOT_CONSTEVAL if constexpr (true)
        #define JKJ_CAN_BRANCH_ON_CONSTEVAL 0
        #define JKJ_USE_IS_CONSTANT_EVALUATED 0
    #else
        #define JKJ_IF_CONSTEVAL if (false)
        #define JKJ_IF_NOT_CONSTEVAL if (true)
        #define JKJ_CAN_BRANCH_ON_CONSTEVAL 0
        #define JKJ_USE_IS_CONSTANT_EVALUATED 0
    #endif
#endif

#if JKJ_CAN_BRANCH_ON_CONSTEVAL && JKJ_HAS_BIT_CAST
    #define JKJ_CONSTEXPR20 constexpr
#else
    #define JKJ_CONSTEXPR20
#endif

// Suppress additional buffer overrun check.
// I have no idea why MSVC thinks some functions here are vulnerable to the buffer overrun
// attacks. No, they aren't.
#if defined(__GNUC__) || defined(__clang__)
    #define JKJ_SAFEBUFFERS
    #define JKJ_FORCEINLINE inline __attribute__((always_inline))
#elif defined(_MSC_VER)
    #define JKJ_SAFEBUFFERS __declspec(safebuffers)
    #define JKJ_FORCEINLINE __forceinline
#else
    #define JKJ_SAFEBUFFERS
    #define JKJ_FORCEINLINE inline
#endif

#if defined(__has_builtin)
    #define JKJ_HAS_BUILTIN(x) __has_builtin(x)
#else
    #define JKJ_HAS_BUILTIN(x) false
#endif

#if defined(_MSC_VER)
    #include <intrin.h>
#elif defined(__INTEL_COMPILER)
    #include <immintrin.h>
#endif

namespace jkj {
    namespace dragonbox {
        ////////////////////////////////////////////////////////////////////////////////////////
        // The Compatibility layer for toolchains without standard C++ headers.
        ////////////////////////////////////////////////////////////////////////////////////////
        namespace detail {
            namespace stdr {
                // <bit>
#if JKJ_HAS_BIT_CAST
                using JKJ_STD_REPLACEMENT_NAMESPACE::bit_cast;
#endif

                // <cassert>
                // We need assert() macro, but it is not namespaced anyway, so nothing to do here.

                // <cstdint>
                using JKJ_STD_REPLACEMENT_NAMESPACE::int_least8_t;
                using JKJ_STD_REPLACEMENT_NAMESPACE::int_least16_t;
                using JKJ_STD_REPLACEMENT_NAMESPACE::int_least32_t;
                using JKJ_STD_REPLACEMENT_NAMESPACE::int_fast8_t;
                using JKJ_STD_REPLACEMENT_NAMESPACE::int_fast16_t;
                using JKJ_STD_REPLACEMENT_NAMESPACE::int_fast32_t;
                using JKJ_STD_REPLACEMENT_NAMESPACE::uint_least8_t;
                using JKJ_STD_REPLACEMENT_NAMESPACE::uint_least16_t;
                using JKJ_STD_REPLACEMENT_NAMESPACE::uint_least32_t;
                using JKJ_STD_REPLACEMENT_NAMESPACE::uint_least64_t;
                using JKJ_STD_REPLACEMENT_NAMESPACE::uint_fast8_t;
                using JKJ_STD_REPLACEMENT_NAMESPACE::uint_fast16_t;
                using JKJ_STD_REPLACEMENT_NAMESPACE::uint_fast32_t;
                // We need INT32_C, UINT32_C and UINT64_C macros too, but again there is nothing to do
                // here.

                // <cstring>
                using JKJ_STD_REPLACEMENT_NAMESPACE::size_t;
                using JKJ_STD_REPLACEMENT_NAMESPACE::memcpy;

                // <limits>
                template <class T>
                using numeric_limits = JKJ_STD_REPLACEMENT_NAMESPACE::numeric_limits<T>;

                // <type_traits>
                template <bool cond, class T = void>
                using enable_if = JKJ_STD_REPLACEMENT_NAMESPACE::enable_if<cond, T>;
                template <class T>
                using add_rvalue_reference = JKJ_STD_REPLACEMENT_NAMESPACE::add_rvalue_reference<T>;
                template <bool cond, class T_true, class T_false>
                using conditional = JKJ_STD_REPLACEMENT_NAMESPACE::conditional<cond, T_true, T_false>;
#if JKJ_USE_IS_CONSTANT_EVALUATED
                using JKJ_STD_REPLACEMENT_NAMESPACE::is_constant_evaluated;
#endif
                template <class T1, class T2>
                using is_same = JKJ_STD_REPLACEMENT_NAMESPACE::is_same<T1, T2>;
#if !JKJ_HAS_BIT_CAST
                template <class T>
                using is_trivially_copyable = JKJ_STD_REPLACEMENT_NAMESPACE::is_trivially_copyable<T>;
#endif
                template <class T>
                using is_integral = JKJ_STD_REPLACEMENT_NAMESPACE::is_integral<T>;
                template <class T>
                using is_signed = JKJ_STD_REPLACEMENT_NAMESPACE::is_signed<T>;
                template <class T>
                using is_unsigned = JKJ_STD_REPLACEMENT_NAMESPACE::is_unsigned<T>;
            }
        }


        ////////////////////////////////////////////////////////////////////////////////////////
        // Some general utilities for C++11-compatibility.
        ////////////////////////////////////////////////////////////////////////////////////////
        namespace detail {
#if !JKJ_HAS_CONSTEXPR17
            template <stdr::size_t... indices>
            struct index_sequence {};

            template <stdr::size_t current, stdr::size_t total, class Dummy, stdr::size_t... indices>
            struct make_index_sequence_impl {
                using type = typename make_index_sequence_impl<current + 1, total, Dummy, indices...,
                                                               current>::type;
            };

            template <stdr::size_t total, class Dummy, stdr::size_t... indices>
            struct make_index_sequence_impl<total, total, Dummy, indices...> {
                using type = index_sequence<indices...>;
            };

            template <stdr::size_t N>
            using make_index_sequence = typename make_index_sequence_impl<0, N, void>::type;
#endif

            // Available since C++11, but including <utility> just for this is an overkill.
            template <class T>
            typename stdr::add_rvalue_reference<T>::type declval() noexcept;

            // Similarly, including <array> is an overkill.
            template <class T, stdr::size_t N>
            struct array {
                T data_[N];
                constexpr T operator[](stdr::size_t idx) const noexcept { return data_[idx]; }
                JKJ_CONSTEXPR14 T& operator[](stdr::size_t idx) noexcept { return data_[idx]; }
            };
        }


        ////////////////////////////////////////////////////////////////////////////////////////
        // Some basic features for encoding/decoding IEEE-754 formats.
        ////////////////////////////////////////////////////////////////////////////////////////
        namespace detail {
            template <class T>
            struct physical_bits {
                static constexpr stdr::size_t value =
                    sizeof(T) * stdr::numeric_limits<unsigned char>::digits;
            };
            template <class T>
            struct value_bits {
                static constexpr stdr::size_t value = stdr::numeric_limits<
                    typename stdr::enable_if<stdr::is_integral<T>::value, T>::type>::digits;
            };

            template <typename To, typename From>
            JKJ_CONSTEXPR20 To bit_cast(const From& from) {
#if JKJ_HAS_BIT_CAST
                return stdr::bit_cast<To>(from);
#else
                static_assert(sizeof(From) == sizeof(To), "");
                static_assert(stdr::is_trivially_copyable<To>::value, "");
                static_assert(stdr::is_trivially_copyable<From>::value, "");
                To to;
                stdr::memcpy(&to, &from, sizeof(To));
                return to;
#endif
            }
        }

        // These classes expose encoding specs of IEEE-754-like floating-point formats.
        // Currently available formats are IEEE-754 binary32 & IEEE-754 binary64.

        struct ieee754_binary32 {
            static constexpr int total_bits = 32;
            static constexpr int significand_bits = 23;
            static constexpr int exponent_bits = 8;
            static constexpr int min_exponent = -126;
            static constexpr int max_exponent = 127;
            static constexpr int exponent_bias = -127;
            static constexpr int decimal_significand_digits = 9;
            static constexpr int decimal_exponent_digits = 2;
        };
        struct ieee754_binary64 {
            static constexpr int total_bits = 64;
            static constexpr int significand_bits = 52;
            static constexpr int exponent_bits = 11;
            static constexpr int min_exponent = -1022;
            static constexpr int max_exponent = 1023;
            static constexpr int exponent_bias = -1023;
            static constexpr int decimal_significand_digits = 17;
            static constexpr int decimal_exponent_digits = 3;
        };

        // A floating-point format traits class defines ways to interpret a bit pattern of given size as
        // an encoding of floating-point number. This is an implementation of such a traits class,
        // supporting ways to interpret IEEE-754 binary floating-point numbers.
        template <class Format, class CarrierUInt, class ExponentInt = int>
        struct ieee754_binary_traits {
            // CarrierUInt needs to have enough size to hold the entire contents of floating-point
            // numbers. The actual bits are assumed to be aligned to the LSB, and every other bits are
            // assumed to be zeroed.
            static_assert(detail::value_bits<CarrierUInt>::value >= Format::total_bits,
                          "jkj::dragonbox: insufficient number of bits");
            static_assert(detail::stdr::is_unsigned<CarrierUInt>::value, "");

            // ExponentUInt needs to be large enough to hold (unsigned) exponent bits as well as the
            // (signed) actual exponent.
            // TODO: static overflow guard against intermediate computations.
            static_assert(detail::value_bits<ExponentInt>::value >= Format::exponent_bits + 1,
                          "jkj::dragonbox: insufficient number of bits");
            static_assert(detail::stdr::is_signed<ExponentInt>::value, "");

            using format = Format;
            using carrier_uint = CarrierUInt;
            static constexpr int carrier_bits = int(detail::value_bits<carrier_uint>::value);
            using exponent_int = ExponentInt;

            // Extract exponent bits from a bit pattern.
            // The result must be aligned to the LSB so that there is no additional zero paddings
            // on the right. This function does not do bias adjustment.
            static constexpr exponent_int extract_exponent_bits(carrier_uint u) noexcept {
                return exponent_int((u >> format::significand_bits) &
                                    ((exponent_int(1) << format::exponent_bits) - 1));
            }

            // Extract significand bits from a bit pattern.
            // The result must be aligned to the LSB so that there is no additional zero paddings
            // on the right. The result does not contain the implicit bit.
            static constexpr carrier_uint extract_significand_bits(carrier_uint u) noexcept {
                return carrier_uint(u & ((carrier_uint(1) << format::significand_bits) - 1u));
            }

            // Remove the exponent bits and extract significand bits together with the sign bit.
            static constexpr carrier_uint remove_exponent_bits(carrier_uint u) noexcept {
                return carrier_uint(u & ~(((carrier_uint(1) << format::exponent_bits) - 1u)
                                          << format::significand_bits));
            }

            // Shift the obtained signed significand bits to the left by 1 to remove the sign bit.
            static constexpr carrier_uint remove_sign_bit_and_shift(carrier_uint u) noexcept {
                return carrier_uint((carrier_uint(u) << 1) &
                                    ((((carrier_uint(1) << (Format::total_bits - 1)) - 1u) << 1) | 1u));
            }

            // Obtain the actual value of the binary exponent from the extracted exponent bits.
            static constexpr exponent_int binary_exponent(exponent_int exponent_bits) noexcept {
                return exponent_int(exponent_bits == 0 ? format::min_exponent
                                                       : exponent_bits + format::exponent_bias);
            }

            // Obtain the actual value of the binary significand from the extracted significand bits
            // and exponent bits.
            static constexpr carrier_uint binary_significand(carrier_uint significand_bits,
                                                             exponent_int exponent_bits) noexcept {
                return carrier_uint(
                    exponent_bits == 0
                        ? significand_bits
                        : (significand_bits | (carrier_uint(1) << format::significand_bits)));
            }

            /* Various boolean observer functions */

            static constexpr bool is_nonzero(carrier_uint u) noexcept {
                return (u & ((carrier_uint(1) << (format::significand_bits + format::exponent_bits)) -
                             1u)) != 0;
            }
            static constexpr bool is_positive(carrier_uint u) noexcept {
                return u < (carrier_uint(1) << (format::significand_bits + format::exponent_bits));
            }
            static constexpr bool is_negative(carrier_uint u) noexcept { return !is_positive(u); }
            static constexpr bool is_finite(exponent_int exponent_bits) noexcept {
                return exponent_bits != ((exponent_int(1) << format::exponent_bits) - 1);
            }
            static constexpr bool has_all_zero_significand_bits(carrier_uint u) noexcept {
                return ((u << 1) &
                        ((((carrier_uint(1) << (Format::total_bits - 1)) - 1u) << 1) | 1u)) == 0;
            }
            static constexpr bool has_even_significand_bits(carrier_uint u) noexcept {
                return u % 2 == 0;
            }
        };

        // Convert between bit patterns stored in carrier_uint and instances of an actual
        // floating-point type. Depending on format and carrier_uint, this operation might not
        // be possible for some specific bit patterns. However, the contract is that u always
        // denotes a valid bit pattern, so the functions here are assumed to be noexcept.
        // Users might specialize this class to change the behavior for certain types.
        // The default provided by the library is to treat the given floating-point type Float as either
        // IEEE-754 binary32 or IEEE-754 binary64, depending on the bitwise size of Float.
        template <class Float>
        struct default_float_bit_carrier_conversion_traits {
            // Guards against types that have different internal representations than IEEE-754
            // binary32/64. I don't know if there is a truly reliable way of detecting IEEE-754 binary
            // formats. I just did my best here. Note that in some cases
            // numeric_limits<Float>::is_iec559 may report false even if the internal representation is
            // IEEE-754 compatible. In such a case, the user can specialize this traits template and
            // remove this static sanity check in order to make Dragonbox work for Float.
            static_assert(detail::stdr::numeric_limits<Float>::is_iec559 &&
                              detail::stdr::numeric_limits<Float>::radix == 2 &&
                              (detail::physical_bits<Float>::value == 32 ||
                               detail::physical_bits<Float>::value == 64),
                          "jkj::dragonbox: Float may not be of IEEE-754 binary32/binary64");

            // Specifies the unsigned integer type to hold bitwise value of Float.
            using carrier_uint =
                typename detail::stdr::conditional<detail::physical_bits<Float>::value == 32,
                                                   detail::stdr::uint_least32_t,
                                                   detail::stdr::uint_least64_t>::type;

            // Specifies the floating-point format.
            using format = typename detail::stdr::conditional<detail::physical_bits<Float>::value == 32,
                                                              ieee754_binary32, ieee754_binary64>::type;

            // Converts the floating-point type into the bit-carrier unsigned integer type.
            static JKJ_CONSTEXPR20 carrier_uint float_to_carrier(Float x) noexcept {
                return detail::bit_cast<carrier_uint>(x);
            }

            // Converts the bit-carrier unsigned integer type into the floating-point type.
            static JKJ_CONSTEXPR20 Float carrier_to_float(carrier_uint x) noexcept {
                return detail::bit_cast<Float>(x);
            }
        };

        // Convenient wrappers for floating-point traits classes.
        // In order to reduce the argument passing overhead, these classes should be as simple as
        // possible (e.g., no inheritance, no private non-static data member, etc.; this is an
        // unfortunate fact about common ABI convention).

        template <class FormatTraits>
        struct signed_significand_bits {
            using format_traits = FormatTraits;
            using carrier_uint = typename format_traits::carrier_uint;

            carrier_uint u;

            signed_significand_bits() = default;
            constexpr explicit signed_significand_bits(carrier_uint bit_pattern) noexcept
                : u{bit_pattern} {}

            // Shift the obtained signed significand bits to the left by 1 to remove the sign bit.
            constexpr carrier_uint remove_sign_bit_and_shift() const noexcept {
                return format_traits::remove_sign_bit_and_shift(u);
            }

            constexpr bool is_positive() const noexcept { return format_traits::is_positive(u); }
            constexpr bool is_negative() const noexcept { return format_traits::is_negative(u); }
            constexpr bool has_all_zero_significand_bits() const noexcept {
                return format_traits::has_all_zero_significand_bits(u);
            }
            constexpr bool has_even_significand_bits() const noexcept {
                return format_traits::has_even_significand_bits(u);
            }
        };

        template <class FormatTraits>
        struct float_bits {
            using format_traits = FormatTraits;
            using carrier_uint = typename format_traits::carrier_uint;
            using exponent_int = typename format_traits::exponent_int;

            carrier_uint u;

            float_bits() = default;
            constexpr explicit float_bits(carrier_uint bit_pattern) noexcept : u{bit_pattern} {}

            // Extract exponent bits from a bit pattern.
            // The result must be aligned to the LSB so that there is no additional zero paddings
            // on the right. This function does not do bias adjustment.
            constexpr exponent_int extract_exponent_bits() const noexcept {
                return format_traits::extract_exponent_bits(u);
            }

            // Extract significand bits from a bit pattern.
            // The result must be aligned to the LSB so that there is no additional zero paddings
            // on the right. The result does not contain the implicit bit.
            constexpr carrier_uint extract_significand_bits() const noexcept {
                return format_traits::extract_significand_bits(u);
            }

            // Remove the exponent bits and extract significand bits together with the sign bit.
            constexpr signed_significand_bits<format_traits> remove_exponent_bits() const noexcept {
                return signed_significand_bits<format_traits>(format_traits::remove_exponent_bits(u));
            }

            // Obtain the actual value of the binary exponent from the extracted exponent bits.
            static constexpr exponent_int binary_exponent(exponent_int exponent_bits) noexcept {
                return format_traits::binary_exponent(exponent_bits);
            }
            constexpr exponent_int binary_exponent() const noexcept {
                return binary_exponent(extract_exponent_bits());
            }

            // Obtain the actual value of the binary exponent from the extracted significand bits
            // and exponent bits.
            static constexpr carrier_uint binary_significand(carrier_uint significand_bits,
                                                             exponent_int exponent_bits) noexcept {
                return format_traits::binary_significand(significand_bits, exponent_bits);
            }
            constexpr carrier_uint binary_significand() const noexcept {
                return binary_significand(extract_significand_bits(), extract_exponent_bits());
            }

            constexpr bool is_nonzero() const noexcept { return format_traits::is_nonzero(u); }
            constexpr bool is_positive() const noexcept { return format_traits::is_positive(u); }
            constexpr bool is_negative() const noexcept { return format_traits::is_negative(u); }
            constexpr bool is_finite(exponent_int exponent_bits) const noexcept {
                return format_traits::is_finite(exponent_bits);
            }
            constexpr bool is_finite() const noexcept {
                return format_traits::is_finite(extract_exponent_bits());
            }
            constexpr bool has_even_significand_bits() const noexcept {
                return format_traits::has_even_significand_bits(u);
            }
        };

        template <class Float,
                  class ConversionTraits = default_float_bit_carrier_conversion_traits<Float>,
                  class FormatTraits = ieee754_binary_traits<typename ConversionTraits::format,
                                                             typename ConversionTraits::carrier_uint>>
        JKJ_CONSTEXPR20 float_bits<FormatTraits> make_float_bits(Float x) noexcept {
            return float_bits<FormatTraits>(ConversionTraits::float_to_carrier(x));
        }

        namespace detail {
            ////////////////////////////////////////////////////////////////////////////////////////
            // Bit operation intrinsics.
            ////////////////////////////////////////////////////////////////////////////////////////

            namespace bits {
                // Most compilers should be able to optimize this into the ROR instruction.
                // n is assumed to be at most of bit_width bits.
                template <stdr::size_t bit_width, class UInt>
                JKJ_CONSTEXPR14 UInt rotr(UInt n, unsigned int r) noexcept {
                    static_assert(bit_width > 0, "jkj::dragonbox: rotation bit-width must be positive");
                    static_assert(bit_width <= value_bits<UInt>::value,
                                  "jkj::dragonbox: rotation bit-width is too large");
                    r &= (bit_width - 1);
                    return (n >> r) | (n << ((bit_width - r) & (bit_width - 1)));
                }
            }

            ////////////////////////////////////////////////////////////////////////////////////////
            // Utilities for wide unsigned integer arithmetic.
            ////////////////////////////////////////////////////////////////////////////////////////

            namespace wuint {
                // Compilers might support built-in 128-bit integer types. However, it seems that
                // emulating them with a pair of 64-bit integers actually produces a better code,
                // so we avoid using those built-ins. That said, they are still useful for
                // implementing 64-bit x 64-bit -> 128-bit multiplication.

                // clang-format off
#if defined(__SIZEOF_INT128__)
		// To silence "error: ISO C++ does not support '__int128' for 'type name'
		// [-Wpedantic]"
#if defined(__GNUC__)
			__extension__
#endif
			using builtin_uint128_t = unsigned __int128;
#endif
                // clang-format on

                struct uint128 {
                    uint128() = default;

                    stdr::uint_least64_t high_;
                    stdr::uint_least64_t low_;

                    constexpr uint128(stdr::uint_least64_t high, stdr::uint_least64_t low) noexcept
                        : high_{high}, low_{low} {}

                    constexpr stdr::uint_least64_t high() const noexcept { return high_; }
                    constexpr stdr::uint_least64_t low() const noexcept { return low_; }

                    JKJ_CONSTEXPR20 uint128& operator+=(stdr::uint_least64_t n) & noexcept {
                        auto const generic_impl = [&] {
                            auto const sum = (low_ + n) & UINT64_C(0xffffffffffffffff);
                            high_ += (sum < low_ ? 1 : 0);
                            low_ = sum;
                        };
                        // To suppress warning.
                        static_cast<void>(generic_impl);

                        JKJ_IF_CONSTEXPR(value_bits<stdr::uint_least64_t>::value > 64) {
                            generic_impl();
                            return *this;
                        }

                        JKJ_IF_CONSTEVAL {
                            generic_impl();
                            return *this;
                        }

                        // See https://github.com/fmtlib/fmt/pull/2985.
#if JKJ_HAS_BUILTIN(__builtin_addcll) && !defined(__ibmxl__)
                        JKJ_IF_CONSTEXPR(
                            stdr::is_same<stdr::uint_least64_t, unsigned long long>::value) {
                            unsigned long long carry{};
                            low_ = stdr::uint_least64_t(__builtin_addcll(low_, n, 0, &carry));
                            high_ = stdr::uint_least64_t(__builtin_addcll(high_, 0, carry, &carry));
                            return *this;
                        }
#endif
#if JKJ_HAS_BUILTIN(__builtin_addcl) && !defined(__ibmxl__)
                        JKJ_IF_CONSTEXPR(stdr::is_same<stdr::uint_least64_t, unsigned long>::value) {
                            unsigned long carry{};
                            low_ = stdr::uint_least64_t(
                                __builtin_addcl(static_cast<unsigned long>(low_),
                                                static_cast<unsigned long>(n), 0, &carry));
                            high_ = stdr::uint_least64_t(
                                __builtin_addcl(static_cast<unsigned long>(high_), 0, carry, &carry));
                            return *this;
                        }
#endif
#if JKJ_HAS_BUILTIN(__builtin_addc) && !defined(__ibmxl__)
                        JKJ_IF_CONSTEXPR(stdr::is_same<stdr::uint_least64_t, unsigned int>::value) {
                            unsigned int carry{};
                            low_ = stdr::uint_least64_t(__builtin_addc(static_cast<unsigned int>(low_),
                                                                       static_cast<unsigned int>(n), 0,
                                                                       &carry));
                            high_ = stdr::uint_least64_t(
                                __builtin_addc(static_cast<unsigned int>(high_), 0, carry, &carry));
                            return *this;
                        }
#endif

#if JKJ_HAS_BUILTIN(__builtin_ia32_addcarry_u64)
                        // __builtin_ia32_addcarry_u64 is not documented, but it seems it takes unsigned
                        // long long arguments.
                        unsigned long long result{};
                        auto const carry = __builtin_ia32_addcarry_u64(0, low_, n, &result);
                        low_ = stdr::uint_least64_t(result);
                        __builtin_ia32_addcarry_u64(carry, high_, 0, &result);
                        high_ = stdr::uint_least64_t(result);
#elif defined(_MSC_VER) && defined(_M_X64)
                        // On MSVC, uint_least64_t and __int64 must be unsigned long long; see
                        // https://learn.microsoft.com/en-us/cpp/c-runtime-library/standard-types
                        // and https://learn.microsoft.com/en-us/cpp/cpp/int8-int16-int32-int64.
                        static_assert(stdr::is_same<unsigned long long, stdr::uint_least64_t>::value,
                                      "");
                        auto const carry = _addcarry_u64(0, low_, n, &low_);
                        _addcarry_u64(carry, high_, 0, &high_);
#elif defined(__INTEL_COMPILER) && (defined(_M_X64) || defined(__x86_64))
                        // Cannot find any documentation on how things are defined, but hopefully this
                        // is always true...
                        static_assert(stdr::is_same<unsigned __int64, stdr::uint_least64_t>::value, "");
                        auto const carry = _addcarry_u64(0, low_, n, &low_);
                        _addcarry_u64(carry, high_, 0, &high_);
#else
                        generic_impl();
#endif
                        return *this;
                    }
                };

                inline JKJ_CONSTEXPR20 stdr::uint_least64_t umul64(stdr::uint_least32_t x,
                                                                   stdr::uint_least32_t y) noexcept {
#if defined(_MSC_VER) && defined(_M_IX86)
                    JKJ_IF_NOT_CONSTEVAL { return __emulu(x, y); }
#endif
                    return x * stdr::uint_least64_t(y);
                }

                // Get 128-bit result of multiplication of two 64-bit unsigned integers.
                JKJ_SAFEBUFFERS inline JKJ_CONSTEXPR20 uint128
                umul128(stdr::uint_least64_t x, stdr::uint_least64_t y) noexcept {
                    auto const generic_impl = [=]() -> uint128 {
                        auto const a = stdr::uint_least32_t(x >> 32);
                        auto const b = stdr::uint_least32_t(x);
                        auto const c = stdr::uint_least32_t(y >> 32);
                        auto const d = stdr::uint_least32_t(y);

                        auto const ac = umul64(a, c);
                        auto const bc = umul64(b, c);
                        auto const ad = umul64(a, d);
                        auto const bd = umul64(b, d);

                        auto const intermediate =
                            (bd >> 32) + stdr::uint_least32_t(ad) + stdr::uint_least32_t(bc);

                        return {ac + (intermediate >> 32) + (ad >> 32) + (bc >> 32),
                                (intermediate << 32) + stdr::uint_least32_t(bd)};
                    };
                    // To silence warning.
                    static_cast<void>(generic_impl);

#if defined(__SIZEOF_INT128__)
                    auto const result = builtin_uint128_t(x) * builtin_uint128_t(y);
                    return {stdr::uint_least64_t(result >> 64), stdr::uint_least64_t(result)};
#elif defined(_MSC_VER) && defined(_M_X64)
                    JKJ_IF_CONSTEVAL {
                        // This redundant variable is to workaround MSVC's codegen bug caused by the
                        // interaction of NRVO and intrinsics.
                        auto const result = generic_impl();
                        return result;
                    }
                    uint128 result;
    #if defined(__AVX2__)
                    result.low_ = _mulx_u64(x, y, &result.high_);
    #else
                    result.low_ = _umul128(x, y, &result.high_);
    #endif
                    return result;
#else
                    return generic_impl();
#endif
                }

                // Get high half of the 128-bit result of multiplication of two 64-bit unsigned
                // integers.
                JKJ_SAFEBUFFERS inline JKJ_CONSTEXPR20 stdr::uint_least64_t
                umul128_upper64(stdr::uint_least64_t x, stdr::uint_least64_t y) noexcept {
                    auto const generic_impl = [=]() -> stdr::uint_least64_t {
                        auto const a = stdr::uint_least32_t(x >> 32);
                        auto const b = stdr::uint_least32_t(x);
                        auto const c = stdr::uint_least32_t(y >> 32);
                        auto const d = stdr::uint_least32_t(y);

                        auto const ac = umul64(a, c);
                        auto const bc = umul64(b, c);
                        auto const ad = umul64(a, d);
                        auto const bd = umul64(b, d);

                        auto const intermediate =
                            (bd >> 32) + stdr::uint_least32_t(ad) + stdr::uint_least32_t(bc);

                        return ac + (intermediate >> 32) + (ad >> 32) + (bc >> 32);
                    };
                    // To silence warning.
                    static_cast<void>(generic_impl);

#if defined(__SIZEOF_INT128__)
                    auto const result = builtin_uint128_t(x) * builtin_uint128_t(y);
                    return stdr::uint_least64_t(result >> 64);
#elif defined(_MSC_VER) && defined(_M_X64)
                    JKJ_IF_CONSTEVAL {
                        // This redundant variable is to workaround MSVC's codegen bug caused by the
                        // interaction of NRVO and intrinsics.
                        auto const result = generic_impl();
                        return result;
                    }
                    stdr::uint_least64_t result;
    #if defined(__AVX2__)
                    _mulx_u64(x, y, &result);
    #else
                    result = __umulh(x, y);
    #endif
                    return result;
#else
                    return generic_impl();
#endif
                }

                // Get upper 128-bits of multiplication of a 64-bit unsigned integer and a 128-bit
                // unsigned integer.
                JKJ_SAFEBUFFERS inline JKJ_CONSTEXPR20 uint128 umul192_upper128(stdr::uint_least64_t x,
                                                                                uint128 y) noexcept {
                    auto r = umul128(x, y.high());
                    r += umul128_upper64(x, y.low());
                    return r;
                }

                // Get upper 64-bits of multiplication of a 32-bit unsigned integer and a 64-bit
                // unsigned integer.
                inline JKJ_CONSTEXPR20 stdr::uint_least64_t
                umul96_upper64(stdr::uint_least32_t x, stdr::uint_least64_t y) noexcept {
#if defined(__SIZEOF_INT128__) || (defined(_MSC_VER) && defined(_M_X64))
                    return umul128_upper64(stdr::uint_least64_t(x) << 32, y);
#else
                    auto const yh = stdr::uint_least32_t(y >> 32);
                    auto const yl = stdr::uint_least32_t(y);

                    auto const xyh = umul64(x, yh);
                    auto const xyl = umul64(x, yl);

                    return xyh + (xyl >> 32);
#endif
                }

                // Get lower 128-bits of multiplication of a 64-bit unsigned integer and a 128-bit
                // unsigned integer.
                JKJ_SAFEBUFFERS inline JKJ_CONSTEXPR20 uint128 umul192_lower128(stdr::uint_least64_t x,
                                                                                uint128 y) noexcept {
                    auto const high = x * y.high();
                    auto const high_low = umul128(x, y.low());
                    return {(high + high_low.high()) & UINT64_C(0xffffffffffffffff), high_low.low()};
                }

                // Get lower 64-bits of multiplication of a 32-bit unsigned integer and a 64-bit
                // unsigned integer.
                constexpr stdr::uint_least64_t umul96_lower64(stdr::uint_least32_t x,
                                                              stdr::uint_least64_t y) noexcept {
                    return (x * y) & UINT64_C(0xffffffffffffffff);
                }
            }

            ////////////////////////////////////////////////////////////////////////////////////////
            // Some simple utilities for constexpr computation.
            ////////////////////////////////////////////////////////////////////////////////////////

            template <int k, class Int>
            constexpr Int compute_power(Int a) noexcept {
                static_assert(k >= 0, "");
#if JKJ_HAS_CONSTEXPR14
                Int p = 1;
                for (int i = 0; i < k; ++i) {
                    p *= a;
                }
                return p;
#else
                return k == 0       ? 1
                       : k % 2 == 0 ? compute_power<k / 2, Int>(a * a)
                                    : a * compute_power<k / 2, Int>(a * a);
#endif
            }

            template <int a, class UInt>
            constexpr int count_factors(UInt n) noexcept {
                static_assert(a > 1, "");
#if JKJ_HAS_CONSTEXPR14
                int c = 0;
                while (n % a == 0) {
                    n /= a;
                    ++c;
                }
                return c;
#else
                return n % a == 0 ? count_factors<a, UInt>(n / a) + 1 : 0;
#endif
            }

            ////////////////////////////////////////////////////////////////////////////////////////
            // Utilities for fast/constexpr log computation.
            ////////////////////////////////////////////////////////////////////////////////////////

            namespace log {
                static_assert((stdr::int_fast32_t(-1) >> 1) == stdr::int_fast32_t(-1) &&
                                  (stdr::int_fast16_t(-1) >> 1) == stdr::int_fast16_t(-1),
                              "jkj::dragonbox: right-shift for signed integers must be arithmetic");

                // For constexpr computation.
                // Returns -1 when n = 0.
                template <class UInt>
                constexpr int floor_log2(UInt n) noexcept {
#if JKJ_HAS_CONSTEXPR14
                    int count = -1;
                    while (n != 0) {
                        ++count;
                        n >>= 1;
                    }
                    return count;
#else
                    return n == 0 ? -1 : floor_log2<UInt>(n / 2) + 1;
#endif
                }

                template <template <stdr::size_t> class Info, stdr::int_least32_t min_exponent,
                          stdr::int_least32_t max_exponent, stdr::size_t current_tier,
                          stdr::int_least32_t supported_min_exponent = Info<current_tier>::min_exponent,
                          stdr::int_least32_t supported_max_exponent = Info<current_tier>::max_exponent>
                constexpr bool is_in_range(int) noexcept {
                    return min_exponent >= supported_min_exponent &&
                           max_exponent <= supported_max_exponent;
                }
                template <template <stdr::size_t> class Info, stdr::int_least32_t min_exponent,
                          stdr::int_least32_t max_exponent, stdr::size_t current_tier>
                constexpr bool is_in_range(...) noexcept {
                    // Supposed to be always false, but formally dependent on the template parameters.
                    static_assert(min_exponent > max_exponent,
                                  "jkj::dragonbox: exponent range is too wide");
                    return false;
                }

                template <template <stdr::size_t> class Info, stdr::int_least32_t min_exponent,
                          stdr::int_least32_t max_exponent, stdr::size_t current_tier = 0,
                          bool = is_in_range<Info, min_exponent, max_exponent, current_tier>(0)>
                struct compute_impl;

                template <template <stdr::size_t> class Info, stdr::int_least32_t min_exponent,
                          stdr::int_least32_t max_exponent, stdr::size_t current_tier>
                struct compute_impl<Info, min_exponent, max_exponent, current_tier, true> {
                    using info = Info<current_tier>;
                    using default_return_type = typename info::default_return_type;
                    template <class ReturnType, class Int>
                    static constexpr ReturnType compute(Int e) noexcept {
#if JKJ_HAS_CONSTEXPR14
                        assert(min_exponent <= e && e <= max_exponent);
#endif
                        // The sign is irrelevant for the mathematical validity of the formula, but
                        // assuming positivity makes the overflow analysis simpler.
                        static_assert(info::multiply >= 0 && info::subtract >= 0, "");
                        return static_cast<ReturnType>((e * info::multiply - info::subtract) >>
                                                       info::shift);
                    }
                };

                template <template <stdr::size_t> class Info, stdr::int_least32_t min_exponent,
                          stdr::int_least32_t max_exponent, stdr::size_t current_tier>
                struct compute_impl<Info, min_exponent, max_exponent, current_tier, false> {
                    using next_tier = compute_impl<Info, min_exponent, max_exponent, current_tier + 1>;
                    using default_return_type = typename next_tier::default_return_type;
                    template <class ReturnType, class Int>
                    static constexpr ReturnType compute(Int e) noexcept {
                        return next_tier::template compute<ReturnType>(e);
                    }
                };

                template <stdr::size_t tier>
                struct floor_log10_pow2_info;
                template <>
                struct floor_log10_pow2_info<0> {
                    using default_return_type = stdr::int_fast8_t;
                    static constexpr stdr::int_fast16_t multiply = 77;
                    static constexpr stdr::int_fast16_t subtract = 0;
                    static constexpr stdr::size_t shift = 8;
                    static constexpr stdr::int_least32_t min_exponent = -102;
                    static constexpr stdr::int_least32_t max_exponent = 102;
                };
                template <>
                struct floor_log10_pow2_info<1> {
                    using default_return_type = stdr::int_fast8_t;
                    // 24-bits are enough in fact.
                    static constexpr stdr::int_fast32_t multiply = 1233;
                    static constexpr stdr::int_fast32_t subtract = 0;
                    static constexpr stdr::size_t shift = 12;
                    // Formula itself holds on [-680,680]; [-425,425] is to ensure that the output is
                    // within [-127,127].
                    static constexpr stdr::int_least32_t min_exponent = -425;
                    static constexpr stdr::int_least32_t max_exponent = 425;
                };
                template <>
                struct floor_log10_pow2_info<2> {
                    using default_return_type = stdr::int_fast16_t;
                    static constexpr stdr::int_fast32_t multiply = INT32_C(315653);
                    static constexpr stdr::int_fast32_t subtract = 0;
                    static constexpr stdr::size_t shift = 20;
                    static constexpr stdr::int_least32_t min_exponent = -2620;
                    static constexpr stdr::int_least32_t max_exponent = 2620;
                };
                template <stdr::int_least32_t min_exponent = -2620,
                          stdr::int_least32_t max_exponent = 2620,
                          class ReturnType = typename compute_impl<floor_log10_pow2_info, min_exponent,
                                                                   max_exponent>::default_return_type,
                          class Int>
                constexpr ReturnType floor_log10_pow2(Int e) noexcept {
                    return compute_impl<floor_log10_pow2_info, min_exponent,
                                        max_exponent>::template compute<ReturnType>(e);
                }

                template <stdr::size_t tier>
                struct floor_log2_pow10_info;
                template <>
                struct floor_log2_pow10_info<0> {
                    using default_return_type = stdr::int_fast8_t;
                    static constexpr stdr::int_fast16_t multiply = 53;
                    static constexpr stdr::int_fast16_t subtract = 0;
                    static constexpr stdr::size_t shift = 4;
                    static constexpr stdr::int_least32_t min_exponent = -15;
                    static constexpr stdr::int_least32_t max_exponent = 18;
                };
                template <>
                struct floor_log2_pow10_info<1> {
                    using default_return_type = stdr::int_fast16_t;
                    // 24-bits are enough in fact.
                    static constexpr stdr::int_fast32_t multiply = 1701;
                    static constexpr stdr::int_fast32_t subtract = 0;
                    static constexpr stdr::size_t shift = 9;
                    static constexpr stdr::int_least32_t min_exponent = -58;
                    static constexpr stdr::int_least32_t max_exponent = 58;
                };
                template <>
                struct floor_log2_pow10_info<2> {
                    using default_return_type = stdr::int_fast16_t;
                    static constexpr stdr::int_fast32_t multiply = INT32_C(1741647);
                    static constexpr stdr::int_fast32_t subtract = 0;
                    static constexpr stdr::size_t shift = 19;
                    // Formula itself holds on [-4003,4003]; [-1233,1233] is to ensure no overflow.
                    static constexpr stdr::int_least32_t min_exponent = -1233;
                    static constexpr stdr::int_least32_t max_exponent = 1233;
                };
                template <stdr::int_least32_t min_exponent = -1233,
                          stdr::int_least32_t max_exponent = 1233,
                          class ReturnType = typename compute_impl<floor_log2_pow10_info, min_exponent,
                                                                   max_exponent>::default_return_type,
                          class Int>
                constexpr ReturnType floor_log2_pow10(Int e) noexcept {
                    return compute_impl<floor_log2_pow10_info, min_exponent,
                                        max_exponent>::template compute<ReturnType>(e);
                }

                template <stdr::size_t tier>
                struct floor_log10_pow2_minus_log10_4_over_3_info;
                template <>
                struct floor_log10_pow2_minus_log10_4_over_3_info<0> {
                    using default_return_type = stdr::int_fast8_t;
                    static constexpr stdr::int_fast16_t multiply = 77;
                    static constexpr stdr::int_fast16_t subtract = 31;
                    static constexpr stdr::size_t shift = 8;
                    static constexpr stdr::int_least32_t min_exponent = -75;
                    static constexpr stdr::int_least32_t max_exponent = 129;
                };
                template <>
                struct floor_log10_pow2_minus_log10_4_over_3_info<1> {
                    using default_return_type = stdr::int_fast8_t;
                    // 24-bits are enough in fact.
                    static constexpr stdr::int_fast32_t multiply = 19728;
                    static constexpr stdr::int_fast32_t subtract = 8241;
                    static constexpr stdr::size_t shift = 16;
                    // Formula itself holds on [-849,315]; [-424,315] is to ensure that the output is
                    // within [-127,127].
                    static constexpr stdr::int_least32_t min_exponent = -424;
                    static constexpr stdr::int_least32_t max_exponent = 315;
                };
                template <>
                struct floor_log10_pow2_minus_log10_4_over_3_info<2> {
                    using default_return_type = stdr::int_fast16_t;
                    static constexpr stdr::int_fast32_t multiply = INT32_C(631305);
                    static constexpr stdr::int_fast32_t subtract = INT32_C(261663);
                    static constexpr stdr::size_t shift = 21;
                    static constexpr stdr::int_least32_t min_exponent = -2985;
                    static constexpr stdr::int_least32_t max_exponent = 2936;
                };
                template <stdr::int_least32_t min_exponent = -2985,
                          stdr::int_least32_t max_exponent = 2936,
                          class ReturnType =
                              typename compute_impl<floor_log10_pow2_minus_log10_4_over_3_info,
                                                    min_exponent, max_exponent>::default_return_type,
                          class Int>
                constexpr ReturnType floor_log10_pow2_minus_log10_4_over_3(Int e) noexcept {
                    return compute_impl<floor_log10_pow2_minus_log10_4_over_3_info, min_exponent,
                                        max_exponent>::template compute<ReturnType>(e);
                }

                template <stdr::size_t tier>
                struct floor_log5_pow2_info;
                template <>
                struct floor_log5_pow2_info<0> {
                    using default_return_type = stdr::int_fast32_t;
                    static constexpr stdr::int_fast32_t multiply = INT32_C(225799);
                    static constexpr stdr::int_fast32_t subtract = 0;
                    static constexpr stdr::size_t shift = 19;
                    static constexpr stdr::int_least32_t min_exponent = -1831;
                    static constexpr stdr::int_least32_t max_exponent = 1831;
                };
                template <stdr::int_least32_t min_exponent = -1831,
                          stdr::int_least32_t max_exponent = 1831,
                          class ReturnType = typename compute_impl<floor_log5_pow2_info, min_exponent,
                                                                   max_exponent>::default_return_type,
                          class Int>
                constexpr ReturnType floor_log5_pow2(Int e) noexcept {
                    return compute_impl<floor_log5_pow2_info, min_exponent,
                                        max_exponent>::template compute<ReturnType>(e);
                }

                template <stdr::size_t tier>
                struct floor_log5_pow2_minus_log5_3_info;
                template <>
                struct floor_log5_pow2_minus_log5_3_info<0> {
                    using default_return_type = stdr::int_fast32_t;
                    static constexpr stdr::int_fast32_t multiply = INT32_C(451597);
                    static constexpr stdr::int_fast32_t subtract = INT32_C(715764);
                    static constexpr stdr::size_t shift = 20;
                    static constexpr stdr::int_least32_t min_exponent = -3543;
                    static constexpr stdr::int_least32_t max_exponent = 2427;
                };
                template <stdr::int_least32_t min_exponent = -3543,
                          stdr::int_least32_t max_exponent = 2427,
                          class ReturnType =
                              typename compute_impl<floor_log5_pow2_minus_log5_3_info, min_exponent,
                                                    max_exponent>::default_return_type,
                          class Int>
                constexpr ReturnType floor_log5_pow2_minus_log5_3(Int e) noexcept {
                    return compute_impl<floor_log5_pow2_minus_log5_3_info, min_exponent,
                                        max_exponent>::template compute<ReturnType>(e);
                }
            }

            ////////////////////////////////////////////////////////////////////////////////////////
            // Utilities for fast divisibility tests.
            ////////////////////////////////////////////////////////////////////////////////////////

            namespace div {
                // Replace n by floor(n / 10^N).
                // Returns true if and only if n is divisible by 10^N.
                // Precondition: n <= 10^(N+1)
                // !!It takes an in-out parameter!!
                template <int N, class UInt>
                struct divide_by_pow10_info;

                template <class UInt>
                struct divide_by_pow10_info<1, UInt> {
                    static constexpr stdr::uint_fast32_t magic_number = 6554;
                    static constexpr int shift_amount = 16;
                };

                template <>
                struct divide_by_pow10_info<1, stdr::uint_least8_t> {
                    static constexpr stdr::uint_fast16_t magic_number = 103;
                    static constexpr int shift_amount = 10;
                };

                template <>
                struct divide_by_pow10_info<1, stdr::uint_least16_t> {
                    static constexpr stdr::uint_fast16_t magic_number = 103;
                    static constexpr int shift_amount = 10;
                };

                template <class UInt>
                struct divide_by_pow10_info<2, UInt> {
                    static constexpr stdr::uint_fast32_t magic_number = 656;
                    static constexpr int shift_amount = 16;
                };

                template <>
                struct divide_by_pow10_info<2, stdr::uint_least16_t> {
                    static constexpr stdr::uint_fast32_t magic_number = 41;
                    static constexpr int shift_amount = 12;
                };

                template <int N, class UInt>
                JKJ_CONSTEXPR14 bool check_divisibility_and_divide_by_pow10(UInt& n) noexcept {
                    // Make sure the computation for max_n does not overflow.
                    static_assert(N + 1 <= log::floor_log10_pow2(int(value_bits<UInt>::value)), "");
                    assert(n <= compute_power<N + 1>(UInt(10)));

                    using info = divide_by_pow10_info<N, UInt>;
                    using intermediate_type = decltype(info::magic_number);
                    auto const prod = intermediate_type(n * info::magic_number);

                    constexpr auto mask =
                        intermediate_type((intermediate_type(1) << info::shift_amount) - 1);
                    bool const result = ((prod & mask) < info::magic_number);

                    n = UInt(prod >> info::shift_amount);
                    return result;
                }

                // Compute floor(n / 10^N) for small n and N.
                // Precondition: n <= 10^(N+1)
                template <int N, class UInt>
                JKJ_CONSTEXPR14 UInt small_division_by_pow10(UInt n) noexcept {
                    // Make sure the computation for max_n does not overflow.
                    static_assert(N + 1 <= log::floor_log10_pow2(int(value_bits<UInt>::value)), "");
                    assert(n <= compute_power<N + 1>(UInt(10)));

                    return UInt((n * divide_by_pow10_info<N, UInt>::magic_number) >>
                                divide_by_pow10_info<N, UInt>::shift_amount);
                }

                // Compute floor(n / 10^N) for small N.
                // Precondition: n <= n_max
                template <int N, class UInt, UInt n_max>
                JKJ_CONSTEXPR20 UInt divide_by_pow10(UInt n) noexcept {
                    static_assert(N >= 0, "");

                    // Specialize for 32-bit division by 10.
                    // Without the bound on n_max (which compilers these days never leverage), the
                    // minimum needed amount of shift is larger than 32. Hence, this may generate better
                    // code for 32-bit or smaller architectures. Even for 64-bit architectures, it seems
                    // compilers tend to generate mov + mul instead of a single imul for an unknown
                    // reason if we just write n / 10.
                    JKJ_IF_CONSTEXPR(stdr::is_same<UInt, stdr::uint_least32_t>::value && N == 1 &&
                                     n_max <= UINT32_C(1073741828)) {
                        return UInt(wuint::umul64(n, UINT32_C(429496730)) >> 32);
                    }
                    // Specialize for 64-bit division by 10.
                    // Without the bound on n_max (which compilers these days never leverage), the
                    // minimum needed amount of shift is larger than 64.
                    else JKJ_IF_CONSTEXPR(stdr::is_same<UInt, stdr::uint_least64_t>::value && N == 1 &&
                                          n_max <= UINT64_C(4611686018427387908)) {
                        return UInt(wuint::umul128_upper64(n, UINT64_C(1844674407370955162)));
                    }
                    // Specialize for 32-bit division by 100.
                    // It seems compilers tend to generate mov + mul instead of a single imul for an
                    // unknown reason if we just write n / 100.
                    else JKJ_IF_CONSTEXPR(stdr::is_same<UInt, stdr::uint_least32_t>::value && N == 2) {
                        return UInt(wuint::umul64(n, UINT32_C(1374389535)) >> 37);
                    }
                    // Specialize for 64-bit division by 1000.
                    // Without the bound on n_max (which compilers these days never leverage), the
                    // smallest magic number for this computation does not fit into 64-bits.
                    else JKJ_IF_CONSTEXPR(stdr::is_same<UInt, stdr::uint_least64_t>::value && N == 3 &&
                                          n_max <= UINT64_C(15534100272597517998)) {
                        return UInt(wuint::umul128_upper64(n, UINT64_C(4722366482869645214)) >> 8);
                    }
                    else {
                        constexpr auto divisor = compute_power<N>(UInt(10));
                        return n / divisor;
                    }
                }
            }
        }

        ////////////////////////////////////////////////////////////////////////////////////////
        // Return types for the main interface function.
        ////////////////////////////////////////////////////////////////////////////////////////

        template <class SignificandType, class ExponentType, bool is_signed, bool trailing_zero_flag>
        struct decimal_fp;

        template <class SignificandType, class ExponentType>
        struct decimal_fp<SignificandType, ExponentType, false, false> {
            SignificandType significand;
            ExponentType exponent;
        };

        template <class SignificandType, class ExponentType>
        struct decimal_fp<SignificandType, ExponentType, true, false> {
            SignificandType significand;
            ExponentType exponent;
            bool is_negative;
        };

        template <class SignificandType, class ExponentType>
        struct decimal_fp<SignificandType, ExponentType, false, true> {
            SignificandType significand;
            ExponentType exponent;
            bool may_have_trailing_zeros;
        };

        template <class SignificandType, class ExponentType>
        struct decimal_fp<SignificandType, ExponentType, true, true> {
            SignificandType significand;
            ExponentType exponent;
            bool may_have_trailing_zeros;
            bool is_negative;
        };

        template <class SignificandType, class ExponentType, bool trailing_zero_flag = false>
        using unsigned_decimal_fp =
            decimal_fp<SignificandType, ExponentType, false, trailing_zero_flag>;

        template <class SignificandType, class ExponentType, bool trailing_zero_flag = false>
        using signed_decimal_fp = decimal_fp<SignificandType, ExponentType, true, trailing_zero_flag>;

        template <class SignificandType, class ExponentType>
        constexpr signed_decimal_fp<SignificandType, ExponentType, false>
        add_sign_to_unsigned_decimal_fp(
            bool is_negative, unsigned_decimal_fp<SignificandType, ExponentType, false> r) noexcept {
            return {r.significand, r.exponent, is_negative};
        }

        template <class SignificandType, class ExponentType>
        constexpr signed_decimal_fp<SignificandType, ExponentType, true>
        add_sign_to_unsigned_decimal_fp(
            bool is_negative, unsigned_decimal_fp<SignificandType, ExponentType, true> r) noexcept {
            return {r.significand, r.exponent, r.may_have_trailing_zeros, is_negative};
        }

        namespace detail {
            template <class UnsignedDecimalFp>
            struct unsigned_decimal_fp_to_signed;

            template <class SignificandType, class ExponentType, bool trailing_zero_flag>
            struct unsigned_decimal_fp_to_signed<
                unsigned_decimal_fp<SignificandType, ExponentType, trailing_zero_flag>> {
                using type = signed_decimal_fp<SignificandType, ExponentType, trailing_zero_flag>;
            };

            template <class UnsignedDecimalFp>
            using unsigned_decimal_fp_to_signed_t =
                typename unsigned_decimal_fp_to_signed<UnsignedDecimalFp>::type;
        }


        ////////////////////////////////////////////////////////////////////////////////////////
        // Computed cache entries.
        ////////////////////////////////////////////////////////////////////////////////////////

        template <class FloatFormat, class Dummy = void>
        struct cache_holder;

        template <class Dummy>
        struct cache_holder<ieee754_binary32, Dummy> {
            using cache_entry_type = detail::stdr::uint_least64_t;
            static constexpr int cache_bits = 64;
            static constexpr int min_k = -31;
            static constexpr int max_k = 46;
            static constexpr detail::array<cache_entry_type, detail::stdr::size_t(max_k - min_k + 1)>
                cache JKJ_STATIC_DATA_SECTION = {
                    {UINT64_C(0x81ceb32c4b43fcf5), UINT64_C(0xa2425ff75e14fc32),
                     UINT64_C(0xcad2f7f5359a3b3f), UINT64_C(0xfd87b5f28300ca0e),
                     UINT64_C(0x9e74d1b791e07e49), UINT64_C(0xc612062576589ddb),
                     UINT64_C(0xf79687aed3eec552), UINT64_C(0x9abe14cd44753b53),
                     UINT64_C(0xc16d9a0095928a28), UINT64_C(0xf1c90080baf72cb2),
                     UINT64_C(0x971da05074da7bef), UINT64_C(0xbce5086492111aeb),
                     UINT64_C(0xec1e4a7db69561a6), UINT64_C(0x9392ee8e921d5d08),
                     UINT64_C(0xb877aa3236a4b44a), UINT64_C(0xe69594bec44de15c),
                     UINT64_C(0x901d7cf73ab0acda), UINT64_C(0xb424dc35095cd810),
                     UINT64_C(0xe12e13424bb40e14), UINT64_C(0x8cbccc096f5088cc),
                     UINT64_C(0xafebff0bcb24aaff), UINT64_C(0xdbe6fecebdedd5bf),
                     UINT64_C(0x89705f4136b4a598), UINT64_C(0xabcc77118461cefd),
                     UINT64_C(0xd6bf94d5e57a42bd), UINT64_C(0x8637bd05af6c69b6),
                     UINT64_C(0xa7c5ac471b478424), UINT64_C(0xd1b71758e219652c),
                     UINT64_C(0x83126e978d4fdf3c), UINT64_C(0xa3d70a3d70a3d70b),
                     UINT64_C(0xcccccccccccccccd), UINT64_C(0x8000000000000000),
                     UINT64_C(0xa000000000000000), UINT64_C(0xc800000000000000),
                     UINT64_C(0xfa00000000000000), UINT64_C(0x9c40000000000000),
                     UINT64_C(0xc350000000000000), UINT64_C(0xf424000000000000),
                     UINT64_C(0x9896800000000000), UINT64_C(0xbebc200000000000),
                     UINT64_C(0xee6b280000000000), UINT64_C(0x9502f90000000000),
                     UINT64_C(0xba43b74000000000), UINT64_C(0xe8d4a51000000000),
                     UINT64_C(0x9184e72a00000000), UINT64_C(0xb5e620f480000000),
                     UINT64_C(0xe35fa931a0000000), UINT64_C(0x8e1bc9bf04000000),
                     UINT64_C(0xb1a2bc2ec5000000), UINT64_C(0xde0b6b3a76400000),
                     UINT64_C(0x8ac7230489e80000), UINT64_C(0xad78ebc5ac620000),
                     UINT64_C(0xd8d726b7177a8000), UINT64_C(0x878678326eac9000),
                     UINT64_C(0xa968163f0a57b400), UINT64_C(0xd3c21bcecceda100),
                     UINT64_C(0x84595161401484a0), UINT64_C(0xa56fa5b99019a5c8),
                     UINT64_C(0xcecb8f27f4200f3a), UINT64_C(0x813f3978f8940985),
                     UINT64_C(0xa18f07d736b90be6), UINT64_C(0xc9f2c9cd04674edf),
                     UINT64_C(0xfc6f7c4045812297), UINT64_C(0x9dc5ada82b70b59e),
                     UINT64_C(0xc5371912364ce306), UINT64_C(0xf684df56c3e01bc7),
                     UINT64_C(0x9a130b963a6c115d), UINT64_C(0xc097ce7bc90715b4),
                     UINT64_C(0xf0bdc21abb48db21), UINT64_C(0x96769950b50d88f5),
                     UINT64_C(0xbc143fa4e250eb32), UINT64_C(0xeb194f8e1ae525fe),
                     UINT64_C(0x92efd1b8d0cf37bf), UINT64_C(0xb7abc627050305ae),
                     UINT64_C(0xe596b7b0c643c71a), UINT64_C(0x8f7e32ce7bea5c70),
                     UINT64_C(0xb35dbf821ae4f38c), UINT64_C(0xe0352f62a19e306f)}};
        };
#if !JKJ_HAS_INLINE_VARIABLE
        // decltype(...) should not depend on Dummy; see
        // https://stackoverflow.com/questions/76438400/decltype-on-static-variable-in-template-class.
        template <class Dummy>
        constexpr decltype(cache_holder<ieee754_binary32>::cache)
            cache_holder<ieee754_binary32, Dummy>::cache;
#endif

        template <class Dummy>
        struct cache_holder<ieee754_binary64, Dummy> {
            using cache_entry_type = detail::wuint::uint128;
            static constexpr int cache_bits = 128;
            static constexpr int min_k = -292;
            static constexpr int max_k = 326;
            static constexpr detail::array<cache_entry_type, detail::stdr::size_t(max_k - min_k + 1)>
                cache JKJ_STATIC_DATA_SECTION = {
                    {{UINT64_C(0xff77b1fcbebcdc4f), UINT64_C(0x25e8e89c13bb0f7b)},
                     {UINT64_C(0x9faacf3df73609b1), UINT64_C(0x77b191618c54e9ad)},
                     {UINT64_C(0xc795830d75038c1d), UINT64_C(0xd59df5b9ef6a2418)},
                     {UINT64_C(0xf97ae3d0d2446f25), UINT64_C(0x4b0573286b44ad1e)},
                     {UINT64_C(0x9becce62836ac577), UINT64_C(0x4ee367f9430aec33)},
                     {UINT64_C(0xc2e801fb244576d5), UINT64_C(0x229c41f793cda740)},
                     {UINT64_C(0xf3a20279ed56d48a), UINT64_C(0x6b43527578c11110)},
                     {UINT64_C(0x9845418c345644d6), UINT64_C(0x830a13896b78aaaa)},
                     {UINT64_C(0xbe5691ef416bd60c), UINT64_C(0x23cc986bc656d554)},
                     {UINT64_C(0xedec366b11c6cb8f), UINT64_C(0x2cbfbe86b7ec8aa9)},
                     {UINT64_C(0x94b3a202eb1c3f39), UINT64_C(0x7bf7d71432f3d6aa)},
                     {UINT64_C(0xb9e08a83a5e34f07), UINT64_C(0xdaf5ccd93fb0cc54)},
                     {UINT64_C(0xe858ad248f5c22c9), UINT64_C(0xd1b3400f8f9cff69)},
                     {UINT64_C(0x91376c36d99995be), UINT64_C(0x23100809b9c21fa2)},
                     {UINT64_C(0xb58547448ffffb2d), UINT64_C(0xabd40a0c2832a78b)},
                     {UINT64_C(0xe2e69915b3fff9f9), UINT64_C(0x16c90c8f323f516d)},
                     {UINT64_C(0x8dd01fad907ffc3b), UINT64_C(0xae3da7d97f6792e4)},
                     {UINT64_C(0xb1442798f49ffb4a), UINT64_C(0x99cd11cfdf41779d)},
                     {UINT64_C(0xdd95317f31c7fa1d), UINT64_C(0x40405643d711d584)},
                     {UINT64_C(0x8a7d3eef7f1cfc52), UINT64_C(0x482835ea666b2573)},
                     {UINT64_C(0xad1c8eab5ee43b66), UINT64_C(0xda3243650005eed0)},
                     {UINT64_C(0xd863b256369d4a40), UINT64_C(0x90bed43e40076a83)},
                     {UINT64_C(0x873e4f75e2224e68), UINT64_C(0x5a7744a6e804a292)},
                     {UINT64_C(0xa90de3535aaae202), UINT64_C(0x711515d0a205cb37)},
                     {UINT64_C(0xd3515c2831559a83), UINT64_C(0x0d5a5b44ca873e04)},
                     {UINT64_C(0x8412d9991ed58091), UINT64_C(0xe858790afe9486c3)},
                     {UINT64_C(0xa5178fff668ae0b6), UINT64_C(0x626e974dbe39a873)},
                     {UINT64_C(0xce5d73ff402d98e3), UINT64_C(0xfb0a3d212dc81290)},
                     {UINT64_C(0x80fa687f881c7f8e), UINT64_C(0x7ce66634bc9d0b9a)},
                     {UINT64_C(0xa139029f6a239f72), UINT64_C(0x1c1fffc1ebc44e81)},
                     {UINT64_C(0xc987434744ac874e), UINT64_C(0xa327ffb266b56221)},
                     {UINT64_C(0xfbe9141915d7a922), UINT64_C(0x4bf1ff9f0062baa9)},
                     {UINT64_C(0x9d71ac8fada6c9b5), UINT64_C(0x6f773fc3603db4aa)},
                     {UINT64_C(0xc4ce17b399107c22), UINT64_C(0xcb550fb4384d21d4)},
                     {UINT64_C(0xf6019da07f549b2b), UINT64_C(0x7e2a53a146606a49)},
                     {UINT64_C(0x99c102844f94e0fb), UINT64_C(0x2eda7444cbfc426e)},
                     {UINT64_C(0xc0314325637a1939), UINT64_C(0xfa911155fefb5309)},
                     {UINT64_C(0xf03d93eebc589f88), UINT64_C(0x793555ab7eba27cb)},
                     {UINT64_C(0x96267c7535b763b5), UINT64_C(0x4bc1558b2f3458df)},
                     {UINT64_C(0xbbb01b9283253ca2), UINT64_C(0x9eb1aaedfb016f17)},
                     {UINT64_C(0xea9c227723ee8bcb), UINT64_C(0x465e15a979c1cadd)},
                     {UINT64_C(0x92a1958a7675175f), UINT64_C(0x0bfacd89ec191eca)},
                     {UINT64_C(0xb749faed14125d36), UINT64_C(0xcef980ec671f667c)},
                     {UINT64_C(0xe51c79a85916f484), UINT64_C(0x82b7e12780e7401b)},
                     {UINT64_C(0x8f31cc0937ae58d2), UINT64_C(0xd1b2ecb8b0908811)},
                     {UINT64_C(0xb2fe3f0b8599ef07), UINT64_C(0x861fa7e6dcb4aa16)},
                     {UINT64_C(0xdfbdcece67006ac9), UINT64_C(0x67a791e093e1d49b)},
                     {UINT64_C(0x8bd6a141006042bd), UINT64_C(0xe0c8bb2c5c6d24e1)},
                     {UINT64_C(0xaecc49914078536d), UINT64_C(0x58fae9f773886e19)},
                     {UINT64_C(0xda7f5bf590966848), UINT64_C(0xaf39a475506a899f)},
                     {UINT64_C(0x888f99797a5e012d), UINT64_C(0x6d8406c952429604)},
                     {UINT64_C(0xaab37fd7d8f58178), UINT64_C(0xc8e5087ba6d33b84)},
                     {UINT64_C(0xd5605fcdcf32e1d6), UINT64_C(0xfb1e4a9a90880a65)},
                     {UINT64_C(0x855c3be0a17fcd26), UINT64_C(0x5cf2eea09a550680)},
                     {UINT64_C(0xa6b34ad8c9dfc06f), UINT64_C(0xf42faa48c0ea481f)},
                     {UINT64_C(0xd0601d8efc57b08b), UINT64_C(0xf13b94daf124da27)},
                     {UINT64_C(0x823c12795db6ce57), UINT64_C(0x76c53d08d6b70859)},
                     {UINT64_C(0xa2cb1717b52481ed), UINT64_C(0x54768c4b0c64ca6f)},
                     {UINT64_C(0xcb7ddcdda26da268), UINT64_C(0xa9942f5dcf7dfd0a)},
                     {UINT64_C(0xfe5d54150b090b02), UINT64_C(0xd3f93b35435d7c4d)},
                     {UINT64_C(0x9efa548d26e5a6e1), UINT64_C(0xc47bc5014a1a6db0)},
                     {UINT64_C(0xc6b8e9b0709f109a), UINT64_C(0x359ab6419ca1091c)},
                     {UINT64_C(0xf867241c8cc6d4c0), UINT64_C(0xc30163d203c94b63)},
                     {UINT64_C(0x9b407691d7fc44f8), UINT64_C(0x79e0de63425dcf1e)},
                     {UINT64_C(0xc21094364dfb5636), UINT64_C(0x985915fc12f542e5)},
                     {UINT64_C(0xf294b943e17a2bc4), UINT64_C(0x3e6f5b7b17b2939e)},
                     {UINT64_C(0x979cf3ca6cec5b5a), UINT64_C(0xa705992ceecf9c43)},
                     {UINT64_C(0xbd8430bd08277231), UINT64_C(0x50c6ff782a838354)},
                     {UINT64_C(0xece53cec4a314ebd), UINT64_C(0xa4f8bf5635246429)},
                     {UINT64_C(0x940f4613ae5ed136), UINT64_C(0x871b7795e136be9a)},
                     {UINT64_C(0xb913179899f68584), UINT64_C(0x28e2557b59846e40)},
                     {UINT64_C(0xe757dd7ec07426e5), UINT64_C(0x331aeada2fe589d0)},
                     {UINT64_C(0x9096ea6f3848984f), UINT64_C(0x3ff0d2c85def7622)},
                     {UINT64_C(0xb4bca50b065abe63), UINT64_C(0x0fed077a756b53aa)},
                     {UINT64_C(0xe1ebce4dc7f16dfb), UINT64_C(0xd3e8495912c62895)},
                     {UINT64_C(0x8d3360f09cf6e4bd), UINT64_C(0x64712dd7abbbd95d)},
                     {UINT64_C(0xb080392cc4349dec), UINT64_C(0xbd8d794d96aacfb4)},
                     {UINT64_C(0xdca04777f541c567), UINT64_C(0xecf0d7a0fc5583a1)},
                     {UINT64_C(0x89e42caaf9491b60), UINT64_C(0xf41686c49db57245)},
                     {UINT64_C(0xac5d37d5b79b6239), UINT64_C(0x311c2875c522ced6)},
                     {UINT64_C(0xd77485cb25823ac7), UINT64_C(0x7d633293366b828c)},
                     {UINT64_C(0x86a8d39ef77164bc), UINT64_C(0xae5dff9c02033198)},
                     {UINT64_C(0xa8530886b54dbdeb), UINT64_C(0xd9f57f830283fdfd)},
                     {UINT64_C(0xd267caa862a12d66), UINT64_C(0xd072df63c324fd7c)},
                     {UINT64_C(0x8380dea93da4bc60), UINT64_C(0x4247cb9e59f71e6e)},
                     {UINT64_C(0xa46116538d0deb78), UINT64_C(0x52d9be85f074e609)},
                     {UINT64_C(0xcd795be870516656), UINT64_C(0x67902e276c921f8c)},
                     {UINT64_C(0x806bd9714632dff6), UINT64_C(0x00ba1cd8a3db53b7)},
                     {UINT64_C(0xa086cfcd97bf97f3), UINT64_C(0x80e8a40eccd228a5)},
                     {UINT64_C(0xc8a883c0fdaf7df0), UINT64_C(0x6122cd128006b2ce)},
                     {UINT64_C(0xfad2a4b13d1b5d6c), UINT64_C(0x796b805720085f82)},
                     {UINT64_C(0x9cc3a6eec6311a63), UINT64_C(0xcbe3303674053bb1)},
                     {UINT64_C(0xc3f490aa77bd60fc), UINT64_C(0xbedbfc4411068a9d)},
                     {UINT64_C(0xf4f1b4d515acb93b), UINT64_C(0xee92fb5515482d45)},
                     {UINT64_C(0x991711052d8bf3c5), UINT64_C(0x751bdd152d4d1c4b)},
                     {UINT64_C(0xbf5cd54678eef0b6), UINT64_C(0xd262d45a78a0635e)},
                     {UINT64_C(0xef340a98172aace4), UINT64_C(0x86fb897116c87c35)},
                     {UINT64_C(0x9580869f0e7aac0e), UINT64_C(0xd45d35e6ae3d4da1)},
                     {UINT64_C(0xbae0a846d2195712), UINT64_C(0x8974836059cca10a)},
                     {UINT64_C(0xe998d258869facd7), UINT64_C(0x2bd1a438703fc94c)},
                     {UINT64_C(0x91ff83775423cc06), UINT64_C(0x7b6306a34627ddd0)},
                     {UINT64_C(0xb67f6455292cbf08), UINT64_C(0x1a3bc84c17b1d543)},
                     {UINT64_C(0xe41f3d6a7377eeca), UINT64_C(0x20caba5f1d9e4a94)},
                     {UINT64_C(0x8e938662882af53e), UINT64_C(0x547eb47b7282ee9d)},
                     {UINT64_C(0xb23867fb2a35b28d), UINT64_C(0xe99e619a4f23aa44)},
                     {UINT64_C(0xdec681f9f4c31f31), UINT64_C(0x6405fa00e2ec94d5)},
                     {UINT64_C(0x8b3c113c38f9f37e), UINT64_C(0xde83bc408dd3dd05)},
                     {UINT64_C(0xae0b158b4738705e), UINT64_C(0x9624ab50b148d446)},
                     {UINT64_C(0xd98ddaee19068c76), UINT64_C(0x3badd624dd9b0958)},
                     {UINT64_C(0x87f8a8d4cfa417c9), UINT64_C(0xe54ca5d70a80e5d7)},
                     {UINT64_C(0xa9f6d30a038d1dbc), UINT64_C(0x5e9fcf4ccd211f4d)},
                     {UINT64_C(0xd47487cc8470652b), UINT64_C(0x7647c32000696720)},
                     {UINT64_C(0x84c8d4dfd2c63f3b), UINT64_C(0x29ecd9f40041e074)},
                     {UINT64_C(0xa5fb0a17c777cf09), UINT64_C(0xf468107100525891)},
                     {UINT64_C(0xcf79cc9db955c2cc), UINT64_C(0x7182148d4066eeb5)},
                     {UINT64_C(0x81ac1fe293d599bf), UINT64_C(0xc6f14cd848405531)},
                     {UINT64_C(0xa21727db38cb002f), UINT64_C(0xb8ada00e5a506a7d)},
                     {UINT64_C(0xca9cf1d206fdc03b), UINT64_C(0xa6d90811f0e4851d)},
                     {UINT64_C(0xfd442e4688bd304a), UINT64_C(0x908f4a166d1da664)},
                     {UINT64_C(0x9e4a9cec15763e2e), UINT64_C(0x9a598e4e043287ff)},
                     {UINT64_C(0xc5dd44271ad3cdba), UINT64_C(0x40eff1e1853f29fe)},
                     {UINT64_C(0xf7549530e188c128), UINT64_C(0xd12bee59e68ef47d)},
                     {UINT64_C(0x9a94dd3e8cf578b9), UINT64_C(0x82bb74f8301958cf)},
                     {UINT64_C(0xc13a148e3032d6e7), UINT64_C(0xe36a52363c1faf02)},
                     {UINT64_C(0xf18899b1bc3f8ca1), UINT64_C(0xdc44e6c3cb279ac2)},
                     {UINT64_C(0x96f5600f15a7b7e5), UINT64_C(0x29ab103a5ef8c0ba)},
                     {UINT64_C(0xbcb2b812db11a5de), UINT64_C(0x7415d448f6b6f0e8)},
                     {UINT64_C(0xebdf661791d60f56), UINT64_C(0x111b495b3464ad22)},
                     {UINT64_C(0x936b9fcebb25c995), UINT64_C(0xcab10dd900beec35)},
                     {UINT64_C(0xb84687c269ef3bfb), UINT64_C(0x3d5d514f40eea743)},
                     {UINT64_C(0xe65829b3046b0afa), UINT64_C(0x0cb4a5a3112a5113)},
                     {UINT64_C(0x8ff71a0fe2c2e6dc), UINT64_C(0x47f0e785eaba72ac)},
                     {UINT64_C(0xb3f4e093db73a093), UINT64_C(0x59ed216765690f57)},
                     {UINT64_C(0xe0f218b8d25088b8), UINT64_C(0x306869c13ec3532d)},
                     {UINT64_C(0x8c974f7383725573), UINT64_C(0x1e414218c73a13fc)},
                     {UINT64_C(0xafbd2350644eeacf), UINT64_C(0xe5d1929ef90898fb)},
                     {UINT64_C(0xdbac6c247d62a583), UINT64_C(0xdf45f746b74abf3a)},
                     {UINT64_C(0x894bc396ce5da772), UINT64_C(0x6b8bba8c328eb784)},
                     {UINT64_C(0xab9eb47c81f5114f), UINT64_C(0x066ea92f3f326565)},
                     {UINT64_C(0xd686619ba27255a2), UINT64_C(0xc80a537b0efefebe)},
                     {UINT64_C(0x8613fd0145877585), UINT64_C(0xbd06742ce95f5f37)},
                     {UINT64_C(0xa798fc4196e952e7), UINT64_C(0x2c48113823b73705)},
                     {UINT64_C(0xd17f3b51fca3a7a0), UINT64_C(0xf75a15862ca504c6)},
                     {UINT64_C(0x82ef85133de648c4), UINT64_C(0x9a984d73dbe722fc)},
                     {UINT64_C(0xa3ab66580d5fdaf5), UINT64_C(0xc13e60d0d2e0ebbb)},
                     {UINT64_C(0xcc963fee10b7d1b3), UINT64_C(0x318df905079926a9)},
                     {UINT64_C(0xffbbcfe994e5c61f), UINT64_C(0xfdf17746497f7053)},
                     {UINT64_C(0x9fd561f1fd0f9bd3), UINT64_C(0xfeb6ea8bedefa634)},
                     {UINT64_C(0xc7caba6e7c5382c8), UINT64_C(0xfe64a52ee96b8fc1)},
                     {UINT64_C(0xf9bd690a1b68637b), UINT64_C(0x3dfdce7aa3c673b1)},
                     {UINT64_C(0x9c1661a651213e2d), UINT64_C(0x06bea10ca65c084f)},
                     {UINT64_C(0xc31bfa0fe5698db8), UINT64_C(0x486e494fcff30a63)},
                     {UINT64_C(0xf3e2f893dec3f126), UINT64_C(0x5a89dba3c3efccfb)},
                     {UINT64_C(0x986ddb5c6b3a76b7), UINT64_C(0xf89629465a75e01d)},
                     {UINT64_C(0xbe89523386091465), UINT64_C(0xf6bbb397f1135824)},
                     {UINT64_C(0xee2ba6c0678b597f), UINT64_C(0x746aa07ded582e2d)},
                     {UINT64_C(0x94db483840b717ef), UINT64_C(0xa8c2a44eb4571cdd)},
                     {UINT64_C(0xba121a4650e4ddeb), UINT64_C(0x92f34d62616ce414)},
                     {UINT64_C(0xe896a0d7e51e1566), UINT64_C(0x77b020baf9c81d18)},
                     {UINT64_C(0x915e2486ef32cd60), UINT64_C(0x0ace1474dc1d122f)},
                     {UINT64_C(0xb5b5ada8aaff80b8), UINT64_C(0x0d819992132456bb)},
                     {UINT64_C(0xe3231912d5bf60e6), UINT64_C(0x10e1fff697ed6c6a)},
                     {UINT64_C(0x8df5efabc5979c8f), UINT64_C(0xca8d3ffa1ef463c2)},
                     {UINT64_C(0xb1736b96b6fd83b3), UINT64_C(0xbd308ff8a6b17cb3)},
                     {UINT64_C(0xddd0467c64bce4a0), UINT64_C(0xac7cb3f6d05ddbdf)},
                     {UINT64_C(0x8aa22c0dbef60ee4), UINT64_C(0x6bcdf07a423aa96c)},
                     {UINT64_C(0xad4ab7112eb3929d), UINT64_C(0x86c16c98d2c953c7)},
                     {UINT64_C(0xd89d64d57a607744), UINT64_C(0xe871c7bf077ba8b8)},
                     {UINT64_C(0x87625f056c7c4a8b), UINT64_C(0x11471cd764ad4973)},
                     {UINT64_C(0xa93af6c6c79b5d2d), UINT64_C(0xd598e40d3dd89bd0)},
                     {UINT64_C(0xd389b47879823479), UINT64_C(0x4aff1d108d4ec2c4)},
                     {UINT64_C(0x843610cb4bf160cb), UINT64_C(0xcedf722a585139bb)},
                     {UINT64_C(0xa54394fe1eedb8fe), UINT64_C(0xc2974eb4ee658829)},
                     {UINT64_C(0xce947a3da6a9273e), UINT64_C(0x733d226229feea33)},
                     {UINT64_C(0x811ccc668829b887), UINT64_C(0x0806357d5a3f5260)},
                     {UINT64_C(0xa163ff802a3426a8), UINT64_C(0xca07c2dcb0cf26f8)},
                     {UINT64_C(0xc9bcff6034c13052), UINT64_C(0xfc89b393dd02f0b6)},
                     {UINT64_C(0xfc2c3f3841f17c67), UINT64_C(0xbbac2078d443ace3)},
                     {UINT64_C(0x9d9ba7832936edc0), UINT64_C(0xd54b944b84aa4c0e)},
                     {UINT64_C(0xc5029163f384a931), UINT64_C(0x0a9e795e65d4df12)},
                     {UINT64_C(0xf64335bcf065d37d), UINT64_C(0x4d4617b5ff4a16d6)},
                     {UINT64_C(0x99ea0196163fa42e), UINT64_C(0x504bced1bf8e4e46)},
                     {UINT64_C(0xc06481fb9bcf8d39), UINT64_C(0xe45ec2862f71e1d7)},
                     {UINT64_C(0xf07da27a82c37088), UINT64_C(0x5d767327bb4e5a4d)},
                     {UINT64_C(0x964e858c91ba2655), UINT64_C(0x3a6a07f8d510f870)},
                     {UINT64_C(0xbbe226efb628afea), UINT64_C(0x890489f70a55368c)},
                     {UINT64_C(0xeadab0aba3b2dbe5), UINT64_C(0x2b45ac74ccea842f)},
                     {UINT64_C(0x92c8ae6b464fc96f), UINT64_C(0x3b0b8bc90012929e)},
                     {UINT64_C(0xb77ada0617e3bbcb), UINT64_C(0x09ce6ebb40173745)},
                     {UINT64_C(0xe55990879ddcaabd), UINT64_C(0xcc420a6a101d0516)},
                     {UINT64_C(0x8f57fa54c2a9eab6), UINT64_C(0x9fa946824a12232e)},
                     {UINT64_C(0xb32df8e9f3546564), UINT64_C(0x47939822dc96abfa)},
                     {UINT64_C(0xdff9772470297ebd), UINT64_C(0x59787e2b93bc56f8)},
                     {UINT64_C(0x8bfbea76c619ef36), UINT64_C(0x57eb4edb3c55b65b)},
                     {UINT64_C(0xaefae51477a06b03), UINT64_C(0xede622920b6b23f2)},
                     {UINT64_C(0xdab99e59958885c4), UINT64_C(0xe95fab368e45ecee)},
                     {UINT64_C(0x88b402f7fd75539b), UINT64_C(0x11dbcb0218ebb415)},
                     {UINT64_C(0xaae103b5fcd2a881), UINT64_C(0xd652bdc29f26a11a)},
                     {UINT64_C(0xd59944a37c0752a2), UINT64_C(0x4be76d3346f04960)},
                     {UINT64_C(0x857fcae62d8493a5), UINT64_C(0x6f70a4400c562ddc)},
                     {UINT64_C(0xa6dfbd9fb8e5b88e), UINT64_C(0xcb4ccd500f6bb953)},
                     {UINT64_C(0xd097ad07a71f26b2), UINT64_C(0x7e2000a41346a7a8)},
                     {UINT64_C(0x825ecc24c873782f), UINT64_C(0x8ed400668c0c28c9)},
                     {UINT64_C(0xa2f67f2dfa90563b), UINT64_C(0x728900802f0f32fb)},
                     {UINT64_C(0xcbb41ef979346bca), UINT64_C(0x4f2b40a03ad2ffba)},
                     {UINT64_C(0xfea126b7d78186bc), UINT64_C(0xe2f610c84987bfa9)},
                     {UINT64_C(0x9f24b832e6b0f436), UINT64_C(0x0dd9ca7d2df4d7ca)},
                     {UINT64_C(0xc6ede63fa05d3143), UINT64_C(0x91503d1c79720dbc)},
                     {UINT64_C(0xf8a95fcf88747d94), UINT64_C(0x75a44c6397ce912b)},
                     {UINT64_C(0x9b69dbe1b548ce7c), UINT64_C(0xc986afbe3ee11abb)},
                     {UINT64_C(0xc24452da229b021b), UINT64_C(0xfbe85badce996169)},
                     {UINT64_C(0xf2d56790ab41c2a2), UINT64_C(0xfae27299423fb9c4)},
                     {UINT64_C(0x97c560ba6b0919a5), UINT64_C(0xdccd879fc967d41b)},
                     {UINT64_C(0xbdb6b8e905cb600f), UINT64_C(0x5400e987bbc1c921)},
                     {UINT64_C(0xed246723473e3813), UINT64_C(0x290123e9aab23b69)},
                     {UINT64_C(0x9436c0760c86e30b), UINT64_C(0xf9a0b6720aaf6522)},
                     {UINT64_C(0xb94470938fa89bce), UINT64_C(0xf808e40e8d5b3e6a)},
                     {UINT64_C(0xe7958cb87392c2c2), UINT64_C(0xb60b1d1230b20e05)},
                     {UINT64_C(0x90bd77f3483bb9b9), UINT64_C(0xb1c6f22b5e6f48c3)},
                     {UINT64_C(0xb4ecd5f01a4aa828), UINT64_C(0x1e38aeb6360b1af4)},
                     {UINT64_C(0xe2280b6c20dd5232), UINT64_C(0x25c6da63c38de1b1)},
                     {UINT64_C(0x8d590723948a535f), UINT64_C(0x579c487e5a38ad0f)},
                     {UINT64_C(0xb0af48ec79ace837), UINT64_C(0x2d835a9df0c6d852)},
                     {UINT64_C(0xdcdb1b2798182244), UINT64_C(0xf8e431456cf88e66)},
                     {UINT64_C(0x8a08f0f8bf0f156b), UINT64_C(0x1b8e9ecb641b5900)},
                     {UINT64_C(0xac8b2d36eed2dac5), UINT64_C(0xe272467e3d222f40)},
                     {UINT64_C(0xd7adf884aa879177), UINT64_C(0x5b0ed81dcc6abb10)},
                     {UINT64_C(0x86ccbb52ea94baea), UINT64_C(0x98e947129fc2b4ea)},
                     {UINT64_C(0xa87fea27a539e9a5), UINT64_C(0x3f2398d747b36225)},
                     {UINT64_C(0xd29fe4b18e88640e), UINT64_C(0x8eec7f0d19a03aae)},
                     {UINT64_C(0x83a3eeeef9153e89), UINT64_C(0x1953cf68300424ad)},
                     {UINT64_C(0xa48ceaaab75a8e2b), UINT64_C(0x5fa8c3423c052dd8)},
                     {UINT64_C(0xcdb02555653131b6), UINT64_C(0x3792f412cb06794e)},
                     {UINT64_C(0x808e17555f3ebf11), UINT64_C(0xe2bbd88bbee40bd1)},
                     {UINT64_C(0xa0b19d2ab70e6ed6), UINT64_C(0x5b6aceaeae9d0ec5)},
                     {UINT64_C(0xc8de047564d20a8b), UINT64_C(0xf245825a5a445276)},
                     {UINT64_C(0xfb158592be068d2e), UINT64_C(0xeed6e2f0f0d56713)},
                     {UINT64_C(0x9ced737bb6c4183d), UINT64_C(0x55464dd69685606c)},
                     {UINT64_C(0xc428d05aa4751e4c), UINT64_C(0xaa97e14c3c26b887)},
                     {UINT64_C(0xf53304714d9265df), UINT64_C(0xd53dd99f4b3066a9)},
                     {UINT64_C(0x993fe2c6d07b7fab), UINT64_C(0xe546a8038efe402a)},
                     {UINT64_C(0xbf8fdb78849a5f96), UINT64_C(0xde98520472bdd034)},
                     {UINT64_C(0xef73d256a5c0f77c), UINT64_C(0x963e66858f6d4441)},
                     {UINT64_C(0x95a8637627989aad), UINT64_C(0xdde7001379a44aa9)},
                     {UINT64_C(0xbb127c53b17ec159), UINT64_C(0x5560c018580d5d53)},
                     {UINT64_C(0xe9d71b689dde71af), UINT64_C(0xaab8f01e6e10b4a7)},
                     {UINT64_C(0x9226712162ab070d), UINT64_C(0xcab3961304ca70e9)},
                     {UINT64_C(0xb6b00d69bb55c8d1), UINT64_C(0x3d607b97c5fd0d23)},
                     {UINT64_C(0xe45c10c42a2b3b05), UINT64_C(0x8cb89a7db77c506b)},
                     {UINT64_C(0x8eb98a7a9a5b04e3), UINT64_C(0x77f3608e92adb243)},
                     {UINT64_C(0xb267ed1940f1c61c), UINT64_C(0x55f038b237591ed4)},
                     {UINT64_C(0xdf01e85f912e37a3), UINT64_C(0x6b6c46dec52f6689)},
                     {UINT64_C(0x8b61313bbabce2c6), UINT64_C(0x2323ac4b3b3da016)},
                     {UINT64_C(0xae397d8aa96c1b77), UINT64_C(0xabec975e0a0d081b)},
                     {UINT64_C(0xd9c7dced53c72255), UINT64_C(0x96e7bd358c904a22)},
                     {UINT64_C(0x881cea14545c7575), UINT64_C(0x7e50d64177da2e55)},
                     {UINT64_C(0xaa242499697392d2), UINT64_C(0xdde50bd1d5d0b9ea)},
                     {UINT64_C(0xd4ad2dbfc3d07787), UINT64_C(0x955e4ec64b44e865)},
                     {UINT64_C(0x84ec3c97da624ab4), UINT64_C(0xbd5af13bef0b113f)},
                     {UINT64_C(0xa6274bbdd0fadd61), UINT64_C(0xecb1ad8aeacdd58f)},
                     {UINT64_C(0xcfb11ead453994ba), UINT64_C(0x67de18eda5814af3)},
                     {UINT64_C(0x81ceb32c4b43fcf4), UINT64_C(0x80eacf948770ced8)},
                     {UINT64_C(0xa2425ff75e14fc31), UINT64_C(0xa1258379a94d028e)},
                     {UINT64_C(0xcad2f7f5359a3b3e), UINT64_C(0x096ee45813a04331)},
                     {UINT64_C(0xfd87b5f28300ca0d), UINT64_C(0x8bca9d6e188853fd)},
                     {UINT64_C(0x9e74d1b791e07e48), UINT64_C(0x775ea264cf55347e)},
                     {UINT64_C(0xc612062576589dda), UINT64_C(0x95364afe032a819e)},
                     {UINT64_C(0xf79687aed3eec551), UINT64_C(0x3a83ddbd83f52205)},
                     {UINT64_C(0x9abe14cd44753b52), UINT64_C(0xc4926a9672793543)},
                     {UINT64_C(0xc16d9a0095928a27), UINT64_C(0x75b7053c0f178294)},
                     {UINT64_C(0xf1c90080baf72cb1), UINT64_C(0x5324c68b12dd6339)},
                     {UINT64_C(0x971da05074da7bee), UINT64_C(0xd3f6fc16ebca5e04)},
                     {UINT64_C(0xbce5086492111aea), UINT64_C(0x88f4bb1ca6bcf585)},
                     {UINT64_C(0xec1e4a7db69561a5), UINT64_C(0x2b31e9e3d06c32e6)},
                     {UINT64_C(0x9392ee8e921d5d07), UINT64_C(0x3aff322e62439fd0)},
                     {UINT64_C(0xb877aa3236a4b449), UINT64_C(0x09befeb9fad487c3)},
                     {UINT64_C(0xe69594bec44de15b), UINT64_C(0x4c2ebe687989a9b4)},
                     {UINT64_C(0x901d7cf73ab0acd9), UINT64_C(0x0f9d37014bf60a11)},
                     {UINT64_C(0xb424dc35095cd80f), UINT64_C(0x538484c19ef38c95)},
                     {UINT64_C(0xe12e13424bb40e13), UINT64_C(0x2865a5f206b06fba)},
                     {UINT64_C(0x8cbccc096f5088cb), UINT64_C(0xf93f87b7442e45d4)},
                     {UINT64_C(0xafebff0bcb24aafe), UINT64_C(0xf78f69a51539d749)},
                     {UINT64_C(0xdbe6fecebdedd5be), UINT64_C(0xb573440e5a884d1c)},
                     {UINT64_C(0x89705f4136b4a597), UINT64_C(0x31680a88f8953031)},
                     {UINT64_C(0xabcc77118461cefc), UINT64_C(0xfdc20d2b36ba7c3e)},
                     {UINT64_C(0xd6bf94d5e57a42bc), UINT64_C(0x3d32907604691b4d)},
                     {UINT64_C(0x8637bd05af6c69b5), UINT64_C(0xa63f9a49c2c1b110)},
                     {UINT64_C(0xa7c5ac471b478423), UINT64_C(0x0fcf80dc33721d54)},
                     {UINT64_C(0xd1b71758e219652b), UINT64_C(0xd3c36113404ea4a9)},
                     {UINT64_C(0x83126e978d4fdf3b), UINT64_C(0x645a1cac083126ea)},
                     {UINT64_C(0xa3d70a3d70a3d70a), UINT64_C(0x3d70a3d70a3d70a4)},
                     {UINT64_C(0xcccccccccccccccc), UINT64_C(0xcccccccccccccccd)},
                     {UINT64_C(0x8000000000000000), UINT64_C(0x0000000000000000)},
                     {UINT64_C(0xa000000000000000), UINT64_C(0x0000000000000000)},
                     {UINT64_C(0xc800000000000000), UINT64_C(0x0000000000000000)},
                     {UINT64_C(0xfa00000000000000), UINT64_C(0x0000000000000000)},
                     {UINT64_C(0x9c40000000000000), UINT64_C(0x0000000000000000)},
                     {UINT64_C(0xc350000000000000), UINT64_C(0x0000000000000000)},
                     {UINT64_C(0xf424000000000000), UINT64_C(0x0000000000000000)},
                     {UINT64_C(0x9896800000000000), UINT64_C(0x0000000000000000)},
                     {UINT64_C(0xbebc200000000000), UINT64_C(0x0000000000000000)},
                     {UINT64_C(0xee6b280000000000), UINT64_C(0x0000000000000000)},
                     {UINT64_C(0x9502f90000000000), UINT64_C(0x0000000000000000)},
                     {UINT64_C(0xba43b74000000000), UINT64_C(0x0000000000000000)},
                     {UINT64_C(0xe8d4a51000000000), UINT64_C(0x0000000000000000)},
                     {UINT64_C(0x9184e72a00000000), UINT64_C(0x0000000000000000)},
                     {UINT64_C(0xb5e620f480000000), UINT64_C(0x0000000000000000)},
                     {UINT64_C(0xe35fa931a0000000), UINT64_C(0x0000000000000000)},
                     {UINT64_C(0x8e1bc9bf04000000), UINT64_C(0x0000000000000000)},
                     {UINT64_C(0xb1a2bc2ec5000000), UINT64_C(0x0000000000000000)},
                     {UINT64_C(0xde0b6b3a76400000), UINT64_C(0x0000000000000000)},
                     {UINT64_C(0x8ac7230489e80000), UINT64_C(0x0000000000000000)},
                     {UINT64_C(0xad78ebc5ac620000), UINT64_C(0x0000000000000000)},
                     {UINT64_C(0xd8d726b7177a8000), UINT64_C(0x0000000000000000)},
                     {UINT64_C(0x878678326eac9000), UINT64_C(0x0000000000000000)},
                     {UINT64_C(0xa968163f0a57b400), UINT64_C(0x0000000000000000)},
                     {UINT64_C(0xd3c21bcecceda100), UINT64_C(0x0000000000000000)},
                     {UINT64_C(0x84595161401484a0), UINT64_C(0x0000000000000000)},
                     {UINT64_C(0xa56fa5b99019a5c8), UINT64_C(0x0000000000000000)},
                     {UINT64_C(0xcecb8f27f4200f3a), UINT64_C(0x0000000000000000)},
                     {UINT64_C(0x813f3978f8940984), UINT64_C(0x4000000000000000)},
                     {UINT64_C(0xa18f07d736b90be5), UINT64_C(0x5000000000000000)},
                     {UINT64_C(0xc9f2c9cd04674ede), UINT64_C(0xa400000000000000)},
                     {UINT64_C(0xfc6f7c4045812296), UINT64_C(0x4d00000000000000)},
                     {UINT64_C(0x9dc5ada82b70b59d), UINT64_C(0xf020000000000000)},
                     {UINT64_C(0xc5371912364ce305), UINT64_C(0x6c28000000000000)},
                     {UINT64_C(0xf684df56c3e01bc6), UINT64_C(0xc732000000000000)},
                     {UINT64_C(0x9a130b963a6c115c), UINT64_C(0x3c7f400000000000)},
                     {UINT64_C(0xc097ce7bc90715b3), UINT64_C(0x4b9f100000000000)},
                     {UINT64_C(0xf0bdc21abb48db20), UINT64_C(0x1e86d40000000000)},
                     {UINT64_C(0x96769950b50d88f4), UINT64_C(0x1314448000000000)},
                     {UINT64_C(0xbc143fa4e250eb31), UINT64_C(0x17d955a000000000)},
                     {UINT64_C(0xeb194f8e1ae525fd), UINT64_C(0x5dcfab0800000000)},
                     {UINT64_C(0x92efd1b8d0cf37be), UINT64_C(0x5aa1cae500000000)},
                     {UINT64_C(0xb7abc627050305ad), UINT64_C(0xf14a3d9e40000000)},
                     {UINT64_C(0xe596b7b0c643c719), UINT64_C(0x6d9ccd05d0000000)},
                     {UINT64_C(0x8f7e32ce7bea5c6f), UINT64_C(0xe4820023a2000000)},
                     {UINT64_C(0xb35dbf821ae4f38b), UINT64_C(0xdda2802c8a800000)},
                     {UINT64_C(0xe0352f62a19e306e), UINT64_C(0xd50b2037ad200000)},
                     {UINT64_C(0x8c213d9da502de45), UINT64_C(0x4526f422cc340000)},
                     {UINT64_C(0xaf298d050e4395d6), UINT64_C(0x9670b12b7f410000)},
                     {UINT64_C(0xdaf3f04651d47b4c), UINT64_C(0x3c0cdd765f114000)},
                     {UINT64_C(0x88d8762bf324cd0f), UINT64_C(0xa5880a69fb6ac800)},
                     {UINT64_C(0xab0e93b6efee0053), UINT64_C(0x8eea0d047a457a00)},
                     {UINT64_C(0xd5d238a4abe98068), UINT64_C(0x72a4904598d6d880)},
                     {UINT64_C(0x85a36366eb71f041), UINT64_C(0x47a6da2b7f864750)},
                     {UINT64_C(0xa70c3c40a64e6c51), UINT64_C(0x999090b65f67d924)},
                     {UINT64_C(0xd0cf4b50cfe20765), UINT64_C(0xfff4b4e3f741cf6d)},
                     {UINT64_C(0x82818f1281ed449f), UINT64_C(0xbff8f10e7a8921a5)},
                     {UINT64_C(0xa321f2d7226895c7), UINT64_C(0xaff72d52192b6a0e)},
                     {UINT64_C(0xcbea6f8ceb02bb39), UINT64_C(0x9bf4f8a69f764491)},
                     {UINT64_C(0xfee50b7025c36a08), UINT64_C(0x02f236d04753d5b5)},
                     {UINT64_C(0x9f4f2726179a2245), UINT64_C(0x01d762422c946591)},
                     {UINT64_C(0xc722f0ef9d80aad6), UINT64_C(0x424d3ad2b7b97ef6)},
                     {UINT64_C(0xf8ebad2b84e0d58b), UINT64_C(0xd2e0898765a7deb3)},
                     {UINT64_C(0x9b934c3b330c8577), UINT64_C(0x63cc55f49f88eb30)},
                     {UINT64_C(0xc2781f49ffcfa6d5), UINT64_C(0x3cbf6b71c76b25fc)},
                     {UINT64_C(0xf316271c7fc3908a), UINT64_C(0x8bef464e3945ef7b)},
                     {UINT64_C(0x97edd871cfda3a56), UINT64_C(0x97758bf0e3cbb5ad)},
                     {UINT64_C(0xbde94e8e43d0c8ec), UINT64_C(0x3d52eeed1cbea318)},
                     {UINT64_C(0xed63a231d4c4fb27), UINT64_C(0x4ca7aaa863ee4bde)},
                     {UINT64_C(0x945e455f24fb1cf8), UINT64_C(0x8fe8caa93e74ef6b)},
                     {UINT64_C(0xb975d6b6ee39e436), UINT64_C(0xb3e2fd538e122b45)},
                     {UINT64_C(0xe7d34c64a9c85d44), UINT64_C(0x60dbbca87196b617)},
                     {UINT64_C(0x90e40fbeea1d3a4a), UINT64_C(0xbc8955e946fe31ce)},
                     {UINT64_C(0xb51d13aea4a488dd), UINT64_C(0x6babab6398bdbe42)},
                     {UINT64_C(0xe264589a4dcdab14), UINT64_C(0xc696963c7eed2dd2)},
                     {UINT64_C(0x8d7eb76070a08aec), UINT64_C(0xfc1e1de5cf543ca3)},
                     {UINT64_C(0xb0de65388cc8ada8), UINT64_C(0x3b25a55f43294bcc)},
                     {UINT64_C(0xdd15fe86affad912), UINT64_C(0x49ef0eb713f39ebf)},
                     {UINT64_C(0x8a2dbf142dfcc7ab), UINT64_C(0x6e3569326c784338)},
                     {UINT64_C(0xacb92ed9397bf996), UINT64_C(0x49c2c37f07965405)},
                     {UINT64_C(0xd7e77a8f87daf7fb), UINT64_C(0xdc33745ec97be907)},
                     {UINT64_C(0x86f0ac99b4e8dafd), UINT64_C(0x69a028bb3ded71a4)},
                     {UINT64_C(0xa8acd7c0222311bc), UINT64_C(0xc40832ea0d68ce0d)},
                     {UINT64_C(0xd2d80db02aabd62b), UINT64_C(0xf50a3fa490c30191)},
                     {UINT64_C(0x83c7088e1aab65db), UINT64_C(0x792667c6da79e0fb)},
                     {UINT64_C(0xa4b8cab1a1563f52), UINT64_C(0x577001b891185939)},
                     {UINT64_C(0xcde6fd5e09abcf26), UINT64_C(0xed4c0226b55e6f87)},
                     {UINT64_C(0x80b05e5ac60b6178), UINT64_C(0x544f8158315b05b5)},
                     {UINT64_C(0xa0dc75f1778e39d6), UINT64_C(0x696361ae3db1c722)},
                     {UINT64_C(0xc913936dd571c84c), UINT64_C(0x03bc3a19cd1e38ea)},
                     {UINT64_C(0xfb5878494ace3a5f), UINT64_C(0x04ab48a04065c724)},
                     {UINT64_C(0x9d174b2dcec0e47b), UINT64_C(0x62eb0d64283f9c77)},
                     {UINT64_C(0xc45d1df942711d9a), UINT64_C(0x3ba5d0bd324f8395)},
                     {UINT64_C(0xf5746577930d6500), UINT64_C(0xca8f44ec7ee3647a)},
                     {UINT64_C(0x9968bf6abbe85f20), UINT64_C(0x7e998b13cf4e1ecc)},
                     {UINT64_C(0xbfc2ef456ae276e8), UINT64_C(0x9e3fedd8c321a67f)},
                     {UINT64_C(0xefb3ab16c59b14a2), UINT64_C(0xc5cfe94ef3ea101f)},
                     {UINT64_C(0x95d04aee3b80ece5), UINT64_C(0xbba1f1d158724a13)},
                     {UINT64_C(0xbb445da9ca61281f), UINT64_C(0x2a8a6e45ae8edc98)},
                     {UINT64_C(0xea1575143cf97226), UINT64_C(0xf52d09d71a3293be)},
                     {UINT64_C(0x924d692ca61be758), UINT64_C(0x593c2626705f9c57)},
                     {UINT64_C(0xb6e0c377cfa2e12e), UINT64_C(0x6f8b2fb00c77836d)},
                     {UINT64_C(0xe498f455c38b997a), UINT64_C(0x0b6dfb9c0f956448)},
                     {UINT64_C(0x8edf98b59a373fec), UINT64_C(0x4724bd4189bd5ead)},
                     {UINT64_C(0xb2977ee300c50fe7), UINT64_C(0x58edec91ec2cb658)},
                     {UINT64_C(0xdf3d5e9bc0f653e1), UINT64_C(0x2f2967b66737e3ee)},
                     {UINT64_C(0x8b865b215899f46c), UINT64_C(0xbd79e0d20082ee75)},
                     {UINT64_C(0xae67f1e9aec07187), UINT64_C(0xecd8590680a3aa12)},
                     {UINT64_C(0xda01ee641a708de9), UINT64_C(0xe80e6f4820cc9496)},
                     {UINT64_C(0x884134fe908658b2), UINT64_C(0x3109058d147fdcde)},
                     {UINT64_C(0xaa51823e34a7eede), UINT64_C(0xbd4b46f0599fd416)},
                     {UINT64_C(0xd4e5e2cdc1d1ea96), UINT64_C(0x6c9e18ac7007c91b)},
                     {UINT64_C(0x850fadc09923329e), UINT64_C(0x03e2cf6bc604ddb1)},
                     {UINT64_C(0xa6539930bf6bff45), UINT64_C(0x84db8346b786151d)},
                     {UINT64_C(0xcfe87f7cef46ff16), UINT64_C(0xe612641865679a64)},
                     {UINT64_C(0x81f14fae158c5f6e), UINT64_C(0x4fcb7e8f3f60c07f)},
                     {UINT64_C(0xa26da3999aef7749), UINT64_C(0xe3be5e330f38f09e)},
                     {UINT64_C(0xcb090c8001ab551c), UINT64_C(0x5cadf5bfd3072cc6)},
                     {UINT64_C(0xfdcb4fa002162a63), UINT64_C(0x73d9732fc7c8f7f7)},
                     {UINT64_C(0x9e9f11c4014dda7e), UINT64_C(0x2867e7fddcdd9afb)},
                     {UINT64_C(0xc646d63501a1511d), UINT64_C(0xb281e1fd541501b9)},
                     {UINT64_C(0xf7d88bc24209a565), UINT64_C(0x1f225a7ca91a4227)},
                     {UINT64_C(0x9ae757596946075f), UINT64_C(0x3375788de9b06959)},
                     {UINT64_C(0xc1a12d2fc3978937), UINT64_C(0x0052d6b1641c83af)},
                     {UINT64_C(0xf209787bb47d6b84), UINT64_C(0xc0678c5dbd23a49b)},
                     {UINT64_C(0x9745eb4d50ce6332), UINT64_C(0xf840b7ba963646e1)},
                     {UINT64_C(0xbd176620a501fbff), UINT64_C(0xb650e5a93bc3d899)},
                     {UINT64_C(0xec5d3fa8ce427aff), UINT64_C(0xa3e51f138ab4cebf)},
                     {UINT64_C(0x93ba47c980e98cdf), UINT64_C(0xc66f336c36b10138)},
                     {UINT64_C(0xb8a8d9bbe123f017), UINT64_C(0xb80b0047445d4185)},
                     {UINT64_C(0xe6d3102ad96cec1d), UINT64_C(0xa60dc059157491e6)},
                     {UINT64_C(0x9043ea1ac7e41392), UINT64_C(0x87c89837ad68db30)},
                     {UINT64_C(0xb454e4a179dd1877), UINT64_C(0x29babe4598c311fc)},
                     {UINT64_C(0xe16a1dc9d8545e94), UINT64_C(0xf4296dd6fef3d67b)},
                     {UINT64_C(0x8ce2529e2734bb1d), UINT64_C(0x1899e4a65f58660d)},
                     {UINT64_C(0xb01ae745b101e9e4), UINT64_C(0x5ec05dcff72e7f90)},
                     {UINT64_C(0xdc21a1171d42645d), UINT64_C(0x76707543f4fa1f74)},
                     {UINT64_C(0x899504ae72497eba), UINT64_C(0x6a06494a791c53a9)},
                     {UINT64_C(0xabfa45da0edbde69), UINT64_C(0x0487db9d17636893)},
                     {UINT64_C(0xd6f8d7509292d603), UINT64_C(0x45a9d2845d3c42b7)},
                     {UINT64_C(0x865b86925b9bc5c2), UINT64_C(0x0b8a2392ba45a9b3)},
                     {UINT64_C(0xa7f26836f282b732), UINT64_C(0x8e6cac7768d7141f)},
                     {UINT64_C(0xd1ef0244af2364ff), UINT64_C(0x3207d795430cd927)},
                     {UINT64_C(0x8335616aed761f1f), UINT64_C(0x7f44e6bd49e807b9)},
                     {UINT64_C(0xa402b9c5a8d3a6e7), UINT64_C(0x5f16206c9c6209a7)},
                     {UINT64_C(0xcd036837130890a1), UINT64_C(0x36dba887c37a8c10)},
                     {UINT64_C(0x802221226be55a64), UINT64_C(0xc2494954da2c978a)},
                     {UINT64_C(0xa02aa96b06deb0fd), UINT64_C(0xf2db9baa10b7bd6d)},
                     {UINT64_C(0xc83553c5c8965d3d), UINT64_C(0x6f92829494e5acc8)},
                     {UINT64_C(0xfa42a8b73abbf48c), UINT64_C(0xcb772339ba1f17fa)},
                     {UINT64_C(0x9c69a97284b578d7), UINT64_C(0xff2a760414536efc)},
                     {UINT64_C(0xc38413cf25e2d70d), UINT64_C(0xfef5138519684abb)},
                     {UINT64_C(0xf46518c2ef5b8cd1), UINT64_C(0x7eb258665fc25d6a)},
                     {UINT64_C(0x98bf2f79d5993802), UINT64_C(0xef2f773ffbd97a62)},
                     {UINT64_C(0xbeeefb584aff8603), UINT64_C(0xaafb550ffacfd8fb)},
                     {UINT64_C(0xeeaaba2e5dbf6784), UINT64_C(0x95ba2a53f983cf39)},
                     {UINT64_C(0x952ab45cfa97a0b2), UINT64_C(0xdd945a747bf26184)},
                     {UINT64_C(0xba756174393d88df), UINT64_C(0x94f971119aeef9e5)},
                     {UINT64_C(0xe912b9d1478ceb17), UINT64_C(0x7a37cd5601aab85e)},
                     {UINT64_C(0x91abb422ccb812ee), UINT64_C(0xac62e055c10ab33b)},
                     {UINT64_C(0xb616a12b7fe617aa), UINT64_C(0x577b986b314d600a)},
                     {UINT64_C(0xe39c49765fdf9d94), UINT64_C(0xed5a7e85fda0b80c)},
                     {UINT64_C(0x8e41ade9fbebc27d), UINT64_C(0x14588f13be847308)},
                     {UINT64_C(0xb1d219647ae6b31c), UINT64_C(0x596eb2d8ae258fc9)},
                     {UINT64_C(0xde469fbd99a05fe3), UINT64_C(0x6fca5f8ed9aef3bc)},
                     {UINT64_C(0x8aec23d680043bee), UINT64_C(0x25de7bb9480d5855)},
                     {UINT64_C(0xada72ccc20054ae9), UINT64_C(0xaf561aa79a10ae6b)},
                     {UINT64_C(0xd910f7ff28069da4), UINT64_C(0x1b2ba1518094da05)},
                     {UINT64_C(0x87aa9aff79042286), UINT64_C(0x90fb44d2f05d0843)},
                     {UINT64_C(0xa99541bf57452b28), UINT64_C(0x353a1607ac744a54)},
                     {UINT64_C(0xd3fa922f2d1675f2), UINT64_C(0x42889b8997915ce9)},
                     {UINT64_C(0x847c9b5d7c2e09b7), UINT64_C(0x69956135febada12)},
                     {UINT64_C(0xa59bc234db398c25), UINT64_C(0x43fab9837e699096)},
                     {UINT64_C(0xcf02b2c21207ef2e), UINT64_C(0x94f967e45e03f4bc)},
                     {UINT64_C(0x8161afb94b44f57d), UINT64_C(0x1d1be0eebac278f6)},
                     {UINT64_C(0xa1ba1ba79e1632dc), UINT64_C(0x6462d92a69731733)},
                     {UINT64_C(0xca28a291859bbf93), UINT64_C(0x7d7b8f7503cfdcff)},
                     {UINT64_C(0xfcb2cb35e702af78), UINT64_C(0x5cda735244c3d43f)},
                     {UINT64_C(0x9defbf01b061adab), UINT64_C(0x3a0888136afa64a8)},
                     {UINT64_C(0xc56baec21c7a1916), UINT64_C(0x088aaa1845b8fdd1)},
                     {UINT64_C(0xf6c69a72a3989f5b), UINT64_C(0x8aad549e57273d46)},
                     {UINT64_C(0x9a3c2087a63f6399), UINT64_C(0x36ac54e2f678864c)},
                     {UINT64_C(0xc0cb28a98fcf3c7f), UINT64_C(0x84576a1bb416a7de)},
                     {UINT64_C(0xf0fdf2d3f3c30b9f), UINT64_C(0x656d44a2a11c51d6)},
                     {UINT64_C(0x969eb7c47859e743), UINT64_C(0x9f644ae5a4b1b326)},
                     {UINT64_C(0xbc4665b596706114), UINT64_C(0x873d5d9f0dde1fef)},
                     {UINT64_C(0xeb57ff22fc0c7959), UINT64_C(0xa90cb506d155a7eb)},
                     {UINT64_C(0x9316ff75dd87cbd8), UINT64_C(0x09a7f12442d588f3)},
                     {UINT64_C(0xb7dcbf5354e9bece), UINT64_C(0x0c11ed6d538aeb30)},
                     {UINT64_C(0xe5d3ef282a242e81), UINT64_C(0x8f1668c8a86da5fb)},
                     {UINT64_C(0x8fa475791a569d10), UINT64_C(0xf96e017d694487bd)},
                     {UINT64_C(0xb38d92d760ec4455), UINT64_C(0x37c981dcc395a9ad)},
                     {UINT64_C(0xe070f78d3927556a), UINT64_C(0x85bbe253f47b1418)},
                     {UINT64_C(0x8c469ab843b89562), UINT64_C(0x93956d7478ccec8f)},
                     {UINT64_C(0xaf58416654a6babb), UINT64_C(0x387ac8d1970027b3)},
                     {UINT64_C(0xdb2e51bfe9d0696a), UINT64_C(0x06997b05fcc0319f)},
                     {UINT64_C(0x88fcf317f22241e2), UINT64_C(0x441fece3bdf81f04)},
                     {UINT64_C(0xab3c2fddeeaad25a), UINT64_C(0xd527e81cad7626c4)},
                     {UINT64_C(0xd60b3bd56a5586f1), UINT64_C(0x8a71e223d8d3b075)},
                     {UINT64_C(0x85c7056562757456), UINT64_C(0xf6872d5667844e4a)},
                     {UINT64_C(0xa738c6bebb12d16c), UINT64_C(0xb428f8ac016561dc)},
                     {UINT64_C(0xd106f86e69d785c7), UINT64_C(0xe13336d701beba53)},
                     {UINT64_C(0x82a45b450226b39c), UINT64_C(0xecc0024661173474)},
                     {UINT64_C(0xa34d721642b06084), UINT64_C(0x27f002d7f95d0191)},
                     {UINT64_C(0xcc20ce9bd35c78a5), UINT64_C(0x31ec038df7b441f5)},
                     {UINT64_C(0xff290242c83396ce), UINT64_C(0x7e67047175a15272)},
                     {UINT64_C(0x9f79a169bd203e41), UINT64_C(0x0f0062c6e984d387)},
                     {UINT64_C(0xc75809c42c684dd1), UINT64_C(0x52c07b78a3e60869)},
                     {UINT64_C(0xf92e0c3537826145), UINT64_C(0xa7709a56ccdf8a83)},
                     {UINT64_C(0x9bbcc7a142b17ccb), UINT64_C(0x88a66076400bb692)},
                     {UINT64_C(0xc2abf989935ddbfe), UINT64_C(0x6acff893d00ea436)},
                     {UINT64_C(0xf356f7ebf83552fe), UINT64_C(0x0583f6b8c4124d44)},
                     {UINT64_C(0x98165af37b2153de), UINT64_C(0xc3727a337a8b704b)},
                     {UINT64_C(0xbe1bf1b059e9a8d6), UINT64_C(0x744f18c0592e4c5d)},
                     {UINT64_C(0xeda2ee1c7064130c), UINT64_C(0x1162def06f79df74)},
                     {UINT64_C(0x9485d4d1c63e8be7), UINT64_C(0x8addcb5645ac2ba9)},
                     {UINT64_C(0xb9a74a0637ce2ee1), UINT64_C(0x6d953e2bd7173693)},
                     {UINT64_C(0xe8111c87c5c1ba99), UINT64_C(0xc8fa8db6ccdd0438)},
                     {UINT64_C(0x910ab1d4db9914a0), UINT64_C(0x1d9c9892400a22a3)},
                     {UINT64_C(0xb54d5e4a127f59c8), UINT64_C(0x2503beb6d00cab4c)},
                     {UINT64_C(0xe2a0b5dc971f303a), UINT64_C(0x2e44ae64840fd61e)},
                     {UINT64_C(0x8da471a9de737e24), UINT64_C(0x5ceaecfed289e5d3)},
                     {UINT64_C(0xb10d8e1456105dad), UINT64_C(0x7425a83e872c5f48)},
                     {UINT64_C(0xdd50f1996b947518), UINT64_C(0xd12f124e28f7771a)},
                     {UINT64_C(0x8a5296ffe33cc92f), UINT64_C(0x82bd6b70d99aaa70)},
                     {UINT64_C(0xace73cbfdc0bfb7b), UINT64_C(0x636cc64d1001550c)},
                     {UINT64_C(0xd8210befd30efa5a), UINT64_C(0x3c47f7e05401aa4f)},
                     {UINT64_C(0x8714a775e3e95c78), UINT64_C(0x65acfaec34810a72)},
                     {UINT64_C(0xa8d9d1535ce3b396), UINT64_C(0x7f1839a741a14d0e)},
                     {UINT64_C(0xd31045a8341ca07c), UINT64_C(0x1ede48111209a051)},
                     {UINT64_C(0x83ea2b892091e44d), UINT64_C(0x934aed0aab460433)},
                     {UINT64_C(0xa4e4b66b68b65d60), UINT64_C(0xf81da84d56178540)},
                     {UINT64_C(0xce1de40642e3f4b9), UINT64_C(0x36251260ab9d668f)},
                     {UINT64_C(0x80d2ae83e9ce78f3), UINT64_C(0xc1d72b7c6b42601a)},
                     {UINT64_C(0xa1075a24e4421730), UINT64_C(0xb24cf65b8612f820)},
                     {UINT64_C(0xc94930ae1d529cfc), UINT64_C(0xdee033f26797b628)},
                     {UINT64_C(0xfb9b7cd9a4a7443c), UINT64_C(0x169840ef017da3b2)},
                     {UINT64_C(0x9d412e0806e88aa5), UINT64_C(0x8e1f289560ee864f)},
                     {UINT64_C(0xc491798a08a2ad4e), UINT64_C(0xf1a6f2bab92a27e3)},
                     {UINT64_C(0xf5b5d7ec8acb58a2), UINT64_C(0xae10af696774b1dc)},
                     {UINT64_C(0x9991a6f3d6bf1765), UINT64_C(0xacca6da1e0a8ef2a)},
                     {UINT64_C(0xbff610b0cc6edd3f), UINT64_C(0x17fd090a58d32af4)},
                     {UINT64_C(0xeff394dcff8a948e), UINT64_C(0xddfc4b4cef07f5b1)},
                     {UINT64_C(0x95f83d0a1fb69cd9), UINT64_C(0x4abdaf101564f98f)},
                     {UINT64_C(0xbb764c4ca7a4440f), UINT64_C(0x9d6d1ad41abe37f2)},
                     {UINT64_C(0xea53df5fd18d5513), UINT64_C(0x84c86189216dc5ee)},
                     {UINT64_C(0x92746b9be2f8552c), UINT64_C(0x32fd3cf5b4e49bb5)},
                     {UINT64_C(0xb7118682dbb66a77), UINT64_C(0x3fbc8c33221dc2a2)},
                     {UINT64_C(0xe4d5e82392a40515), UINT64_C(0x0fabaf3feaa5334b)},
                     {UINT64_C(0x8f05b1163ba6832d), UINT64_C(0x29cb4d87f2a7400f)},
                     {UINT64_C(0xb2c71d5bca9023f8), UINT64_C(0x743e20e9ef511013)},
                     {UINT64_C(0xdf78e4b2bd342cf6), UINT64_C(0x914da9246b255417)},
                     {UINT64_C(0x8bab8eefb6409c1a), UINT64_C(0x1ad089b6c2f7548f)},
                     {UINT64_C(0xae9672aba3d0c320), UINT64_C(0xa184ac2473b529b2)},
                     {UINT64_C(0xda3c0f568cc4f3e8), UINT64_C(0xc9e5d72d90a2741f)},
                     {UINT64_C(0x8865899617fb1871), UINT64_C(0x7e2fa67c7a658893)},
                     {UINT64_C(0xaa7eebfb9df9de8d), UINT64_C(0xddbb901b98feeab8)},
                     {UINT64_C(0xd51ea6fa85785631), UINT64_C(0x552a74227f3ea566)},
                     {UINT64_C(0x8533285c936b35de), UINT64_C(0xd53a88958f872760)},
                     {UINT64_C(0xa67ff273b8460356), UINT64_C(0x8a892abaf368f138)},
                     {UINT64_C(0xd01fef10a657842c), UINT64_C(0x2d2b7569b0432d86)},
                     {UINT64_C(0x8213f56a67f6b29b), UINT64_C(0x9c3b29620e29fc74)},
                     {UINT64_C(0xa298f2c501f45f42), UINT64_C(0x8349f3ba91b47b90)},
                     {UINT64_C(0xcb3f2f7642717713), UINT64_C(0x241c70a936219a74)},
                     {UINT64_C(0xfe0efb53d30dd4d7), UINT64_C(0xed238cd383aa0111)},
                     {UINT64_C(0x9ec95d1463e8a506), UINT64_C(0xf4363804324a40ab)},
                     {UINT64_C(0xc67bb4597ce2ce48), UINT64_C(0xb143c6053edcd0d6)},
                     {UINT64_C(0xf81aa16fdc1b81da), UINT64_C(0xdd94b7868e94050b)},
                     {UINT64_C(0x9b10a4e5e9913128), UINT64_C(0xca7cf2b4191c8327)},
                     {UINT64_C(0xc1d4ce1f63f57d72), UINT64_C(0xfd1c2f611f63a3f1)},
                     {UINT64_C(0xf24a01a73cf2dccf), UINT64_C(0xbc633b39673c8ced)},
                     {UINT64_C(0x976e41088617ca01), UINT64_C(0xd5be0503e085d814)},
                     {UINT64_C(0xbd49d14aa79dbc82), UINT64_C(0x4b2d8644d8a74e19)},
                     {UINT64_C(0xec9c459d51852ba2), UINT64_C(0xddf8e7d60ed1219f)},
                     {UINT64_C(0x93e1ab8252f33b45), UINT64_C(0xcabb90e5c942b504)},
                     {UINT64_C(0xb8da1662e7b00a17), UINT64_C(0x3d6a751f3b936244)},
                     {UINT64_C(0xe7109bfba19c0c9d), UINT64_C(0x0cc512670a783ad5)},
                     {UINT64_C(0x906a617d450187e2), UINT64_C(0x27fb2b80668b24c6)},
                     {UINT64_C(0xb484f9dc9641e9da), UINT64_C(0xb1f9f660802dedf7)},
                     {UINT64_C(0xe1a63853bbd26451), UINT64_C(0x5e7873f8a0396974)},
                     {UINT64_C(0x8d07e33455637eb2), UINT64_C(0xdb0b487b6423e1e9)},
                     {UINT64_C(0xb049dc016abc5e5f), UINT64_C(0x91ce1a9a3d2cda63)},
                     {UINT64_C(0xdc5c5301c56b75f7), UINT64_C(0x7641a140cc7810fc)},
                     {UINT64_C(0x89b9b3e11b6329ba), UINT64_C(0xa9e904c87fcb0a9e)},
                     {UINT64_C(0xac2820d9623bf429), UINT64_C(0x546345fa9fbdcd45)},
                     {UINT64_C(0xd732290fbacaf133), UINT64_C(0xa97c177947ad4096)},
                     {UINT64_C(0x867f59a9d4bed6c0), UINT64_C(0x49ed8eabcccc485e)},
                     {UINT64_C(0xa81f301449ee8c70), UINT64_C(0x5c68f256bfff5a75)},
                     {UINT64_C(0xd226fc195c6a2f8c), UINT64_C(0x73832eec6fff3112)},
                     {UINT64_C(0x83585d8fd9c25db7), UINT64_C(0xc831fd53c5ff7eac)},
                     {UINT64_C(0xa42e74f3d032f525), UINT64_C(0xba3e7ca8b77f5e56)},
                     {UINT64_C(0xcd3a1230c43fb26f), UINT64_C(0x28ce1bd2e55f35ec)},
                     {UINT64_C(0x80444b5e7aa7cf85), UINT64_C(0x7980d163cf5b81b4)},
                     {UINT64_C(0xa0555e361951c366), UINT64_C(0xd7e105bcc3326220)},
                     {UINT64_C(0xc86ab5c39fa63440), UINT64_C(0x8dd9472bf3fefaa8)},
                     {UINT64_C(0xfa856334878fc150), UINT64_C(0xb14f98f6f0feb952)},
                     {UINT64_C(0x9c935e00d4b9d8d2), UINT64_C(0x6ed1bf9a569f33d4)},
                     {UINT64_C(0xc3b8358109e84f07), UINT64_C(0x0a862f80ec4700c9)},
                     {UINT64_C(0xf4a642e14c6262c8), UINT64_C(0xcd27bb612758c0fb)},
                     {UINT64_C(0x98e7e9cccfbd7dbd), UINT64_C(0x8038d51cb897789d)},
                     {UINT64_C(0xbf21e44003acdd2c), UINT64_C(0xe0470a63e6bd56c4)},
                     {UINT64_C(0xeeea5d5004981478), UINT64_C(0x1858ccfce06cac75)},
                     {UINT64_C(0x95527a5202df0ccb), UINT64_C(0x0f37801e0c43ebc9)},
                     {UINT64_C(0xbaa718e68396cffd), UINT64_C(0xd30560258f54e6bb)},
                     {UINT64_C(0xe950df20247c83fd), UINT64_C(0x47c6b82ef32a206a)},
                     {UINT64_C(0x91d28b7416cdd27e), UINT64_C(0x4cdc331d57fa5442)},
                     {UINT64_C(0xb6472e511c81471d), UINT64_C(0xe0133fe4adf8e953)},
                     {UINT64_C(0xe3d8f9e563a198e5), UINT64_C(0x58180fddd97723a7)},
                     {UINT64_C(0x8e679c2f5e44ff8f), UINT64_C(0x570f09eaa7ea7649)},
                     {UINT64_C(0xb201833b35d63f73), UINT64_C(0x2cd2cc6551e513db)},
                     {UINT64_C(0xde81e40a034bcf4f), UINT64_C(0xf8077f7ea65e58d2)},
                     {UINT64_C(0x8b112e86420f6191), UINT64_C(0xfb04afaf27faf783)},
                     {UINT64_C(0xadd57a27d29339f6), UINT64_C(0x79c5db9af1f9b564)},
                     {UINT64_C(0xd94ad8b1c7380874), UINT64_C(0x18375281ae7822bd)},
                     {UINT64_C(0x87cec76f1c830548), UINT64_C(0x8f2293910d0b15b6)},
                     {UINT64_C(0xa9c2794ae3a3c69a), UINT64_C(0xb2eb3875504ddb23)},
                     {UINT64_C(0xd433179d9c8cb841), UINT64_C(0x5fa60692a46151ec)},
                     {UINT64_C(0x849feec281d7f328), UINT64_C(0xdbc7c41ba6bcd334)},
                     {UINT64_C(0xa5c7ea73224deff3), UINT64_C(0x12b9b522906c0801)},
                     {UINT64_C(0xcf39e50feae16bef), UINT64_C(0xd768226b34870a01)},
                     {UINT64_C(0x81842f29f2cce375), UINT64_C(0xe6a1158300d46641)},
                     {UINT64_C(0xa1e53af46f801c53), UINT64_C(0x60495ae3c1097fd1)},
                     {UINT64_C(0xca5e89b18b602368), UINT64_C(0x385bb19cb14bdfc5)},
                     {UINT64_C(0xfcf62c1dee382c42), UINT64_C(0x46729e03dd9ed7b6)},
                     {UINT64_C(0x9e19db92b4e31ba9), UINT64_C(0x6c07a2c26a8346d2)},
                     {UINT64_C(0xc5a05277621be293), UINT64_C(0xc7098b7305241886)},
                     {UINT64_C(0xf70867153aa2db38), UINT64_C(0xb8cbee4fc66d1ea8)}}};
        };
#if !JKJ_HAS_INLINE_VARIABLE
        // decltype(...) should not depend on Dummy; see
        // https://stackoverflow.com/questions/76438400/decltype-on-static-variable-in-template-class.
        template <class Dummy>
        constexpr decltype(cache_holder<ieee754_binary64>::cache)
            cache_holder<ieee754_binary64, Dummy>::cache;
#endif

        // Compressed cache.
        template <class FloatFormat, class Dummy = void>
        struct compressed_cache_holder {
            using cache_entry_type = typename cache_holder<FloatFormat>::cache_entry_type;
            static constexpr int cache_bits = cache_holder<FloatFormat>::cache_bits;
            static constexpr int min_k = cache_holder<FloatFormat>::min_k;
            static constexpr int max_k = cache_holder<FloatFormat>::max_k;

            template <class ShiftAmountType, class DecimalExponentType>
            static constexpr cache_entry_type get_cache(DecimalExponentType k) noexcept {
                return cache_holder<FloatFormat>::cache[k - min_k];
            }
        };

        template <class Dummy>
        struct compressed_cache_holder<ieee754_binary32, Dummy> {
            using cache_entry_type = cache_holder<ieee754_binary32>::cache_entry_type;
            static constexpr int cache_bits = cache_holder<ieee754_binary32>::cache_bits;
            static constexpr int min_k = cache_holder<ieee754_binary32>::min_k;
            static constexpr int max_k = cache_holder<ieee754_binary32>::max_k;
            static constexpr int compression_ratio = 13;
            static constexpr detail::stdr::size_t compressed_table_size =
                detail::stdr::size_t((max_k - min_k + compression_ratio) / compression_ratio);
            static constexpr detail::stdr::size_t pow5_table_size =
                detail::stdr::size_t((compression_ratio + 1) / 2);

            using cache_holder_t = detail::array<cache_entry_type, compressed_table_size>;
            using pow5_holder_t = detail::array<detail::stdr::uint_least16_t, pow5_table_size>;

#if JKJ_HAS_CONSTEXPR17
            static constexpr cache_holder_t cache JKJ_STATIC_DATA_SECTION = [] {
                cache_holder_t res{};
                for (detail::stdr::size_t i = 0; i < compressed_table_size; ++i) {
                    res[i] = cache_holder<ieee754_binary32>::cache[i * compression_ratio];
                }
                return res;
            }();
            static constexpr pow5_holder_t pow5_table JKJ_STATIC_DATA_SECTION = [] {
                pow5_holder_t res{};
                detail::stdr::uint_least16_t p = 1;
                for (detail::stdr::size_t i = 0; i < pow5_table_size; ++i) {
                    res[i] = p;
                    p *= 5;
                }
                return res;
            }();
#else
            template <detail::stdr::size_t... indices>
            static constexpr cache_holder_t make_cache(detail::index_sequence<indices...>) {
                return {cache_holder<ieee754_binary32>::cache[indices * compression_ratio]...};
            }
            static constexpr cache_holder_t cache JKJ_STATIC_DATA_SECTION =
                make_cache(detail::make_index_sequence<compressed_table_size>{});

            template <detail::stdr::size_t... indices>
            static constexpr pow5_holder_t make_pow5_table(detail::index_sequence<indices...>) {
                return {detail::compute_power<indices>(detail::stdr::uint_least16_t(5))...};
            }
            static constexpr pow5_holder_t pow5_table JKJ_STATIC_DATA_SECTION =
                make_pow5_table(detail::make_index_sequence<pow5_table_size>{});
#endif

            template <class ShiftAmountType, class DecimalExponentType>
            static JKJ_CONSTEXPR20 cache_entry_type get_cache(DecimalExponentType k) noexcept {
                // Compute the base index.
                // Supposed to compute (k - min_k) / compression_ratio.
                // Parentheses around min/max are to prevent macro expansions (e.g. in Windows.h).
                static_assert(max_k - min_k <= 89 && compression_ratio == 13, "");
                static_assert(
                    max_k - min_k <= (detail::stdr::numeric_limits<DecimalExponentType>::max)(), "");
                auto const cache_index =
                    DecimalExponentType(detail::stdr::uint_fast16_t(DecimalExponentType(k - min_k) *
                                                                    detail::stdr::int_fast16_t(79)) >>
                                        10);
                auto const kb = DecimalExponentType(cache_index * compression_ratio + min_k);
                auto const offset = DecimalExponentType(k - kb);

                // Get the base cache.
                auto const base_cache = cache[cache_index];

                if (offset == 0) {
                    return base_cache;
                }
                else {
                    // Compute the required amount of bit-shift.
                    auto const alpha =
                        ShiftAmountType(detail::log::floor_log2_pow10<min_k, max_k>(k) -
                                        detail::log::floor_log2_pow10<min_k, max_k>(kb) - offset);
                    assert(alpha > 0 && alpha < 64);

                    // Try to recover the real cache.
                    auto const pow5 =
                        offset >= 7
                            ? detail::stdr::uint_fast32_t(detail::stdr::uint_fast32_t(pow5_table[6]) *
                                                          pow5_table[offset - 6])
                            : detail::stdr::uint_fast32_t(pow5_table[offset]);
                    auto mul_result = detail::wuint::umul128(base_cache, pow5);
                    auto const recovered_cache =
                        cache_entry_type((((mul_result.high() << ShiftAmountType(64 - alpha)) |
                                           (mul_result.low() >> alpha)) +
                                          1) &
                                         UINT64_C(0xffffffffffffffff));
                    assert(recovered_cache != 0);

                    return recovered_cache;
                }
            }
        };
#if !JKJ_HAS_INLINE_VARIABLE
        template <class Dummy>
        constexpr typename compressed_cache_holder<ieee754_binary32, Dummy>::cache_holder_t
            compressed_cache_holder<ieee754_binary32, Dummy>::cache;
        template <class Dummy>
        constexpr typename compressed_cache_holder<ieee754_binary32, Dummy>::pow5_holder_t
            compressed_cache_holder<ieee754_binary32, Dummy>::pow5_table;
#endif

        template <class Dummy>
        struct compressed_cache_holder<ieee754_binary64, Dummy> {
            using cache_entry_type = cache_holder<ieee754_binary64>::cache_entry_type;
            static constexpr int cache_bits = cache_holder<ieee754_binary64>::cache_bits;
            static constexpr int min_k = cache_holder<ieee754_binary64>::min_k;
            static constexpr int max_k = cache_holder<ieee754_binary64>::max_k;
            static constexpr int compression_ratio = 27;
            static constexpr detail::stdr::size_t compressed_table_size =
                detail::stdr::size_t((max_k - min_k + compression_ratio) / compression_ratio);
            static constexpr detail::stdr::size_t pow5_table_size =
                detail::stdr::size_t(compression_ratio);

            using cache_holder_t = detail::array<cache_entry_type, compressed_table_size>;
            using pow5_holder_t = detail::array<detail::stdr::uint_least64_t, pow5_table_size>;

#if JKJ_HAS_CONSTEXPR17
            static constexpr cache_holder_t cache JKJ_STATIC_DATA_SECTION = [] {
                cache_holder_t res{};
                for (detail::stdr::size_t i = 0; i < compressed_table_size; ++i) {
                    res[i] = cache_holder<ieee754_binary64>::cache[i * compression_ratio];
                }
                return res;
            }();
            static constexpr pow5_holder_t pow5_table JKJ_STATIC_DATA_SECTION = [] {
                pow5_holder_t res{};
                detail::stdr::uint_least64_t p = 1;
                for (detail::stdr::size_t i = 0; i < pow5_table_size; ++i) {
                    res[i] = p;
                    p *= 5;
                }
                return res;
            }();
#else
            template <detail::stdr::size_t... indices>
            static constexpr cache_holder_t make_cache(detail::index_sequence<indices...>) {
                return {cache_holder<ieee754_binary64>::cache[indices * compression_ratio]...};
            }
            static constexpr cache_holder_t cache JKJ_STATIC_DATA_SECTION =
                make_cache(detail::make_index_sequence<compressed_table_size>{});

            template <detail::stdr::size_t... indices>
            static constexpr pow5_holder_t make_pow5_table(detail::index_sequence<indices...>) {
                return {detail::compute_power<indices>(detail::stdr::uint_least64_t(5))...};
            }
            static constexpr pow5_holder_t pow5_table JKJ_STATIC_DATA_SECTION =
                make_pow5_table(detail::make_index_sequence<pow5_table_size>{});
#endif

            template <class ShiftAmountType, class DecimalExponentType>
            static JKJ_CONSTEXPR20 cache_entry_type get_cache(DecimalExponentType k) noexcept {
                // Compute the base index.
                // Supposed to compute (k - min_k) / compression_ratio.
                // Parentheses around min/max are to prevent macro expansions (e.g. in Windows.h).
                static_assert(max_k - min_k <= 619 && compression_ratio == 27, "");
                static_assert(
                    max_k - min_k <= (detail::stdr::numeric_limits<DecimalExponentType>::max)(), "");
                auto const cache_index =
                    DecimalExponentType(detail::stdr::uint_fast32_t(DecimalExponentType(k - min_k) *
                                                                    detail::stdr::int_fast32_t(607)) >>
                                        14);
                auto const kb = DecimalExponentType(cache_index * compression_ratio + min_k);
                auto const offset = DecimalExponentType(k - kb);

                // Get the base cache.
                auto const base_cache = cache[cache_index];

                if (offset == 0) {
                    return base_cache;
                }
                else {
                    // Compute the required amount of bit-shift.
                    auto const alpha =
                        ShiftAmountType(detail::log::floor_log2_pow10<min_k, max_k>(k) -
                                        detail::log::floor_log2_pow10<min_k, max_k>(kb) - offset);
                    assert(alpha > 0 && alpha < 64);

                    // Try to recover the real cache.
                    auto const pow5 = pow5_table[offset];
                    auto recovered_cache = detail::wuint::umul128(base_cache.high(), pow5);
                    auto const middle_low = detail::wuint::umul128(base_cache.low(), pow5);

                    recovered_cache += middle_low.high();

                    auto const high_to_middle = detail::stdr::uint_least64_t(
                        (recovered_cache.high() << ShiftAmountType(64 - alpha)) &
                        UINT64_C(0xffffffffffffffff));
                    auto const middle_to_low = detail::stdr::uint_least64_t(
                        (recovered_cache.low() << ShiftAmountType(64 - alpha)) &
                        UINT64_C(0xffffffffffffffff));

                    recovered_cache = {(recovered_cache.low() >> alpha) | high_to_middle,
                                       ((middle_low.low() >> alpha) | middle_to_low)};

                    assert(recovered_cache.low() != UINT64_C(0xffffffffffffffff));
                    recovered_cache = {recovered_cache.high(),
                                       detail::stdr::uint_least64_t(recovered_cache.low() + 1)};

                    return recovered_cache;
                }
            }
        };
#if !JKJ_HAS_INLINE_VARIABLE
        template <class Dummy>
        constexpr typename compressed_cache_holder<ieee754_binary64, Dummy>::cache_holder_t
            compressed_cache_holder<ieee754_binary64, Dummy>::cache;
        template <class Dummy>
        constexpr typename compressed_cache_holder<ieee754_binary64, Dummy>::pow5_holder_t
            compressed_cache_holder<ieee754_binary64, Dummy>::pow5_table;
#endif

        ////////////////////////////////////////////////////////////////////////////////////////
        // Forward declarations of user-specializable templates used in the main algorithm.
        ////////////////////////////////////////////////////////////////////////////////////////

        // Remove trailing zeros from significand and add the number of removed zeros into
        // exponent.
        template <class TrailingZeroPolicy, class Format, class DecimalSignificand,
                  class DecimalExponentType>
        struct remove_trailing_zeros_traits;

        // Users can specialize this traits class to make Dragonbox work with their own formats.
        // However, this requires detailed knowledge on how the algorithm works, so it is recommended to
        // read through the paper.
        template <class FormatTraits, class CacheEntryType, detail::stdr::size_t cache_bits_>
        struct multiplication_traits;

        // A collection of some common definitions to reduce boilerplate.
        template <class FormatTraits, class CacheEntryType, detail::stdr::size_t cache_bits_>
        struct multiplication_traits_base {
            using format = typename FormatTraits::format;
            static constexpr int significand_bits = format::significand_bits;
            static constexpr int total_bits = format::total_bits;
            using carrier_uint = typename FormatTraits::carrier_uint;
            using cache_entry_type = CacheEntryType;
            static constexpr int cache_bits = int(cache_bits_);

            struct compute_mul_result {
                carrier_uint integer_part;
                bool is_integer;
            };
            struct compute_mul_parity_result {
                bool parity;
                bool is_integer;
            };
        };


        ////////////////////////////////////////////////////////////////////////////////////////
        // Policies.
        ////////////////////////////////////////////////////////////////////////////////////////

        namespace detail {
            template <class T>
            struct dummy {};
        }

        namespace policy {
            namespace sign {
                JKJ_INLINE_VARIABLE struct ignore_t {
                    using sign_policy = ignore_t;
                    static constexpr bool return_has_sign = false;

#if defined(_MSC_VER) && !defined(__clang__)
                    // See
                    // https://developercommunity.visualstudio.com/t/Failure-to-optimize-intrinsics/10628226
                    template <class SignedSignificandBits, class DecimalSignificand,
                              class DecimalExponentType>
                    static constexpr decimal_fp<DecimalSignificand, DecimalExponentType, false, false>
                    handle_sign(
                        SignedSignificandBits,
                        decimal_fp<DecimalSignificand, DecimalExponentType, false, false> r) noexcept {
                        return {r.significand, r.exponent};
                    }
                    template <class SignedSignificandBits, class DecimalSignificand,
                              class DecimalExponentType>
                    static constexpr decimal_fp<DecimalSignificand, DecimalExponentType, false, true>
                    handle_sign(
                        SignedSignificandBits,
                        decimal_fp<DecimalSignificand, DecimalExponentType, false, true> r) noexcept {
                        return {r.significand, r.exponent, r.may_have_trailing_zeros};
                    }
#else
                    template <class SignedSignificandBits, class UnsignedDecimalFp>
                    static constexpr UnsignedDecimalFp handle_sign(SignedSignificandBits,
                                                                   UnsignedDecimalFp r) noexcept {
                        return r;
                    }
#endif
                } ignore = {};

                JKJ_INLINE_VARIABLE struct return_sign_t {
                    using sign_policy = return_sign_t;
                    static constexpr bool return_has_sign = true;

                    template <class SignedSignificandBits, class UnsignedDecimalFp>
                    static constexpr detail::unsigned_decimal_fp_to_signed_t<UnsignedDecimalFp>
                    handle_sign(SignedSignificandBits s, UnsignedDecimalFp r) noexcept {
                        return add_sign_to_unsigned_decimal_fp(s.is_negative(), r);
                    }
                } return_sign = {};
            }

            namespace trailing_zero {
                JKJ_INLINE_VARIABLE struct ignore_t {
                    using trailing_zero_policy = ignore_t;
                    static constexpr bool report_trailing_zeros = false;

                    template <class Format, class DecimalSignificand, class DecimalExponentType>
                    static constexpr unsigned_decimal_fp<DecimalSignificand, DecimalExponentType, false>
                    on_trailing_zeros(DecimalSignificand significand,
                                      DecimalExponentType exponent) noexcept {
                        return {significand, exponent};
                    }

                    template <class Format, class DecimalSignificand, class DecimalExponentType>
                    static constexpr unsigned_decimal_fp<DecimalSignificand, DecimalExponentType, false>
                    no_trailing_zeros(DecimalSignificand significand,
                                      DecimalExponentType exponent) noexcept {
                        return {significand, exponent};
                    }
                } ignore = {};

                JKJ_INLINE_VARIABLE struct remove_t {
                    using trailing_zero_policy = remove_t;
                    static constexpr bool report_trailing_zeros = false;

                    template <class Format, class DecimalSignificand, class DecimalExponentType>
                    JKJ_FORCEINLINE static JKJ_CONSTEXPR14
                        unsigned_decimal_fp<DecimalSignificand, DecimalExponentType, false>
                        on_trailing_zeros(DecimalSignificand significand,
                                          DecimalExponentType exponent) noexcept {
                        remove_trailing_zeros_traits<
                            remove_t, Format, DecimalSignificand,
                            DecimalExponentType>::remove_trailing_zeros(significand, exponent);
                        return {significand, exponent};
                    }

                    template <class Format, class DecimalSignificand, class DecimalExponentType>
                    static constexpr unsigned_decimal_fp<DecimalSignificand, DecimalExponentType, false>
                    no_trailing_zeros(DecimalSignificand significand,
                                      DecimalExponentType exponent) noexcept {
                        return {significand, exponent};
                    }
                } remove = {};

                JKJ_INLINE_VARIABLE struct remove_compact_t {
                    using trailing_zero_policy = remove_compact_t;
                    static constexpr bool report_trailing_zeros = false;

                    template <class Format, class DecimalSignificand, class DecimalExponentType>
                    JKJ_FORCEINLINE static JKJ_CONSTEXPR14
                        unsigned_decimal_fp<DecimalSignificand, DecimalExponentType, false>
                        on_trailing_zeros(DecimalSignificand significand,
                                          DecimalExponentType exponent) noexcept {
                        remove_trailing_zeros_traits<
                            remove_compact_t, Format, DecimalSignificand,
                            DecimalExponentType>::remove_trailing_zeros(significand, exponent);
                        return {significand, exponent};
                    }

                    template <class Format, class DecimalSignificand, class DecimalExponentType>
                    static constexpr unsigned_decimal_fp<DecimalSignificand, DecimalExponentType, false>
                    no_trailing_zeros(DecimalSignificand significand,
                                      DecimalExponentType exponent) noexcept {
                        return {significand, exponent};
                    }
                } remove_compact = {};

                JKJ_INLINE_VARIABLE struct report_t {
                    using trailing_zero_policy = report_t;
                    static constexpr bool report_trailing_zeros = true;

                    template <class Format, class DecimalSignificand, class DecimalExponentType>
                    static constexpr unsigned_decimal_fp<DecimalSignificand, DecimalExponentType, true>
                    on_trailing_zeros(DecimalSignificand significand,
                                      DecimalExponentType exponent) noexcept {
                        return {significand, exponent, true};
                    }

                    template <class Format, class DecimalSignificand, class DecimalExponentType>
                    static constexpr unsigned_decimal_fp<DecimalSignificand, DecimalExponentType, true>
                    no_trailing_zeros(DecimalSignificand significand,
                                      DecimalExponentType exponent) noexcept {
                        return {significand, exponent, false};
                    }
                } report = {};
            }

            namespace decimal_to_binary_rounding {
                enum class tag_t { to_nearest, left_closed_directed, right_closed_directed };

                namespace interval_type {
                    struct symmetric_boundary {
                        static constexpr bool is_symmetric = true;
                        bool is_closed;
                        constexpr bool include_left_endpoint() const noexcept { return is_closed; }
                        constexpr bool include_right_endpoint() const noexcept { return is_closed; }
                    };
                    struct asymmetric_boundary {
                        static constexpr bool is_symmetric = false;
                        bool is_left_closed;
                        constexpr bool include_left_endpoint() const noexcept { return is_left_closed; }
                        constexpr bool include_right_endpoint() const noexcept {
                            return !is_left_closed;
                        }
                    };
                    struct closed {
                        static constexpr bool is_symmetric = true;
                        static constexpr bool include_left_endpoint() noexcept { return true; }
                        static constexpr bool include_right_endpoint() noexcept { return true; }
                    };
                    struct open {
                        static constexpr bool is_symmetric = true;
                        static constexpr bool include_left_endpoint() noexcept { return false; }
                        static constexpr bool include_right_endpoint() noexcept { return false; }
                    };
                    struct left_closed_right_open {
                        static constexpr bool is_symmetric = false;
                        static constexpr bool include_left_endpoint() noexcept { return true; }
                        static constexpr bool include_right_endpoint() noexcept { return false; }
                    };
                    struct right_closed_left_open {
                        static constexpr bool is_symmetric = false;
                        static constexpr bool include_left_endpoint() noexcept { return false; }
                        static constexpr bool include_right_endpoint() noexcept { return true; }
                    };
                }

                JKJ_INLINE_VARIABLE struct nearest_to_even_t {
                    using decimal_to_binary_rounding_policy = nearest_to_even_t;
                    using interval_type_provider = nearest_to_even_t;
                    static constexpr auto tag = tag_t::to_nearest;

                    template <class SignedSignificandBits, class Func, class... Args>
                    JKJ_FORCEINLINE JKJ_SAFEBUFFERS static constexpr decltype(Func{}(
                        dragonbox::detail::declval<nearest_to_even_t>(), Args{}...))
                    delegate(SignedSignificandBits, Func f, Args... args) noexcept {
                        return f(nearest_to_even_t{}, args...);
                    }

                    template <class SignedSignificandBits>
                    static constexpr interval_type::symmetric_boundary
                    normal_interval(SignedSignificandBits s) noexcept {
                        return {s.has_even_significand_bits()};
                    }

                    template <class SignedSignificandBits>
                    static constexpr interval_type::closed
                    shorter_interval(SignedSignificandBits) noexcept {
                        return {};
                    }
                } nearest_to_even = {};

                JKJ_INLINE_VARIABLE struct nearest_to_odd_t {
                    using decimal_to_binary_rounding_policy = nearest_to_odd_t;
                    using interval_type_provider = nearest_to_odd_t;
                    static constexpr auto tag = tag_t::to_nearest;

                    template <class SignedSignificandBits, class Func, class... Args>
                    JKJ_FORCEINLINE JKJ_SAFEBUFFERS static constexpr decltype(Func{}(
                        dragonbox::detail::declval<nearest_to_odd_t>(), Args{}...))
                    delegate(SignedSignificandBits, Func f, Args... args) noexcept {
                        return f(nearest_to_odd_t{}, args...);
                    }

                    template <class SignedSignificandBits>
                    static constexpr interval_type::symmetric_boundary
                    normal_interval(SignedSignificandBits s) noexcept {
                        return {!s.has_even_significand_bits()};
                    }
                    template <class SignedSignificandBits>
                    static constexpr interval_type::open
                    shorter_interval(SignedSignificandBits) noexcept {
                        return {};
                    }
                } nearest_to_odd = {};

                JKJ_INLINE_VARIABLE struct nearest_toward_plus_infinity_t {
                    using decimal_to_binary_rounding_policy = nearest_toward_plus_infinity_t;
                    using interval_type_provider = nearest_toward_plus_infinity_t;
                    static constexpr auto tag = tag_t::to_nearest;

                    template <class SignedSignificandBits, class Func, class... Args>
                    JKJ_FORCEINLINE JKJ_SAFEBUFFERS static constexpr decltype(Func{}(
                        dragonbox::detail::declval<nearest_toward_plus_infinity_t>(), Args{}...))
                    delegate(SignedSignificandBits, Func f, Args... args) noexcept {
                        return f(nearest_toward_plus_infinity_t{}, args...);
                    }

                    template <class SignedSignificandBits>
                    static constexpr interval_type::asymmetric_boundary
                    normal_interval(SignedSignificandBits s) noexcept {
                        return {!s.is_negative()};
                    }
                    template <class SignedSignificandBits>
                    static constexpr interval_type::asymmetric_boundary
                    shorter_interval(SignedSignificandBits s) noexcept {
                        return {!s.is_negative()};
                    }
                } nearest_toward_plus_infinity = {};

                JKJ_INLINE_VARIABLE struct nearest_toward_minus_infinity_t {
                    using decimal_to_binary_rounding_policy = nearest_toward_minus_infinity_t;
                    using interval_type_provider = nearest_toward_minus_infinity_t;
                    static constexpr auto tag = tag_t::to_nearest;

                    template <class SignedSignificandBits, class Func, class... Args>
                    JKJ_FORCEINLINE JKJ_SAFEBUFFERS static constexpr decltype(Func{}(
                        dragonbox::detail::declval<nearest_toward_minus_infinity_t>(), Args{}...))
                    delegate(SignedSignificandBits, Func f, Args... args) noexcept {
                        return f(nearest_toward_minus_infinity_t{}, args...);
                    }

                    template <class SignedSignificandBits>
                    static constexpr interval_type::asymmetric_boundary
                    normal_interval(SignedSignificandBits s) noexcept {
                        return {s.is_negative()};
                    }
                    template <class SignedSignificandBits>
                    static constexpr interval_type::asymmetric_boundary
                    shorter_interval(SignedSignificandBits s) noexcept {
                        return {s.is_negative()};
                    }
                } nearest_toward_minus_infinity = {};

                JKJ_INLINE_VARIABLE struct nearest_toward_zero_t {
                    using decimal_to_binary_rounding_policy = nearest_toward_zero_t;
                    using interval_type_provider = nearest_toward_zero_t;
                    static constexpr auto tag = tag_t::to_nearest;

                    template <class SignedSignificandBits, class Func, class... Args>
                    JKJ_FORCEINLINE JKJ_SAFEBUFFERS static constexpr decltype(Func{}(
                        dragonbox::detail::declval<nearest_toward_zero_t>(), Args{}...))
                    delegate(SignedSignificandBits, Func f, Args... args) noexcept {
                        return f(nearest_toward_zero_t{}, args...);
                    }

                    template <class SignedSignificandBits>
                    static constexpr interval_type::right_closed_left_open
                    normal_interval(SignedSignificandBits) noexcept {
                        return {};
                    }
                    template <class SignedSignificandBits>
                    static constexpr interval_type::right_closed_left_open
                    shorter_interval(SignedSignificandBits) noexcept {
                        return {};
                    }
                } nearest_toward_zero = {};

                JKJ_INLINE_VARIABLE struct nearest_away_from_zero_t {
                    using decimal_to_binary_rounding_policy = nearest_away_from_zero_t;
                    using interval_type_provider = nearest_away_from_zero_t;
                    static constexpr auto tag = tag_t::to_nearest;

                    template <class SignedSignificandBits, class Func, class... Args>
                    JKJ_FORCEINLINE JKJ_SAFEBUFFERS static constexpr decltype(Func{}(
                        dragonbox::detail::declval<nearest_away_from_zero_t>(), Args{}...))
                    delegate(SignedSignificandBits, Func f, Args... args) noexcept {
                        return f(nearest_away_from_zero_t{}, args...);
                    }

                    template <class SignedSignificandBits>
                    static constexpr interval_type::left_closed_right_open
                    normal_interval(SignedSignificandBits) noexcept {
                        return {};
                    }
                    template <class SignedSignificandBits>
                    static constexpr interval_type::left_closed_right_open
                    shorter_interval(SignedSignificandBits) noexcept {
                        return {};
                    }
                } nearest_away_from_zero = {};

                namespace detail {
                    struct nearest_always_closed_t {
                        using interval_type_provider = nearest_always_closed_t;
                        static constexpr auto tag = tag_t::to_nearest;

                        template <class SignedSignificandBits>
                        static constexpr interval_type::closed
                        normal_interval(SignedSignificandBits) noexcept {
                            return {};
                        }
                        template <class SignedSignificandBits>
                        static constexpr interval_type::closed
                        shorter_interval(SignedSignificandBits) noexcept {
                            return {};
                        }
                    };
                    struct nearest_always_open_t {
                        using interval_type_provider = nearest_always_open_t;
                        static constexpr auto tag = tag_t::to_nearest;

                        template <class SignedSignificandBits>
                        static constexpr interval_type::open
                        normal_interval(SignedSignificandBits) noexcept {
                            return {};
                        }
                        template <class SignedSignificandBits>
                        static constexpr interval_type::open
                        shorter_interval(SignedSignificandBits) noexcept {
                            return {};
                        }
                    };
                }

                JKJ_INLINE_VARIABLE struct nearest_to_even_static_boundary_t {
                    using decimal_to_binary_rounding_policy = nearest_to_even_static_boundary_t;

                    template <class SignedSignificandBits, class Func, class... Args>
                    JKJ_FORCEINLINE JKJ_SAFEBUFFERS static constexpr decltype(Func{}(
                        detail::nearest_always_closed_t{}, Args{}...))
                    delegate(SignedSignificandBits s, Func f, Args... args) noexcept {
                        return s.has_even_significand_bits()
                                   ? f(detail::nearest_always_closed_t{}, args...)
                                   : f(detail::nearest_always_open_t{}, args...);
                    }
                } nearest_to_even_static_boundary = {};

                JKJ_INLINE_VARIABLE struct nearest_to_odd_static_boundary_t {
                    using decimal_to_binary_rounding_policy = nearest_to_odd_static_boundary_t;

                    template <class SignedSignificandBits, class Func, class... Args>
                    JKJ_FORCEINLINE JKJ_SAFEBUFFERS static constexpr decltype(Func{}(
                        detail::nearest_always_closed_t{}, Args{}...))
                    delegate(SignedSignificandBits s, Func f, Args... args) noexcept {
                        return s.has_even_significand_bits()
                                   ? f(detail::nearest_always_open_t{}, args...)
                                   : f(detail::nearest_always_closed_t{}, args...);
                    }
                } nearest_to_odd_static_boundary = {};

                JKJ_INLINE_VARIABLE struct nearest_toward_plus_infinity_static_boundary_t {
                    using decimal_to_binary_rounding_policy =
                        nearest_toward_plus_infinity_static_boundary_t;

                    template <class SignedSignificandBits, class Func, class... Args>
                    JKJ_FORCEINLINE
                        JKJ_SAFEBUFFERS static constexpr decltype(Func{}(nearest_toward_zero,
                                                                         Args{}...))
                        delegate(SignedSignificandBits s, Func f, Args... args) noexcept {
                        return s.is_negative() ? f(nearest_toward_zero, args...)
                                               : f(nearest_away_from_zero, args...);
                    }
                } nearest_toward_plus_infinity_static_boundary = {};

                JKJ_INLINE_VARIABLE struct nearest_toward_minus_infinity_static_boundary_t {
                    using decimal_to_binary_rounding_policy =
                        nearest_toward_minus_infinity_static_boundary_t;

                    template <class SignedSignificandBits, class Func, class... Args>
                    JKJ_FORCEINLINE
                        JKJ_SAFEBUFFERS static constexpr decltype(Func{}(nearest_toward_zero,
                                                                         Args{}...))
                        delegate(SignedSignificandBits s, Func f, Args... args) noexcept {
                        return s.is_negative() ? f(nearest_away_from_zero, args...)
                                               : f(nearest_toward_zero, args...);
                    }
                } nearest_toward_minus_infinity_static_boundary = {};

                namespace detail {
                    struct left_closed_directed_t {
                        static constexpr auto tag = tag_t::left_closed_directed;
                    };
                    struct right_closed_directed_t {
                        static constexpr auto tag = tag_t::right_closed_directed;
                    };
                }

                JKJ_INLINE_VARIABLE struct toward_plus_infinity_t {
                    using decimal_to_binary_rounding_policy = toward_plus_infinity_t;

                    template <class SignedSignificandBits, class Func, class... Args>
                    JKJ_FORCEINLINE JKJ_SAFEBUFFERS static constexpr decltype(Func{}(
                        detail::left_closed_directed_t{}, Args{}...))
                    delegate(SignedSignificandBits s, Func f, Args... args) noexcept {
                        return s.is_negative() ? f(detail::left_closed_directed_t{}, args...)
                                               : f(detail::right_closed_directed_t{}, args...);
                    }
                } toward_plus_infinity = {};

                JKJ_INLINE_VARIABLE struct toward_minus_infinity_t {
                    using decimal_to_binary_rounding_policy = toward_minus_infinity_t;

                    template <class SignedSignificandBits, class Func, class... Args>
                    JKJ_FORCEINLINE JKJ_SAFEBUFFERS static constexpr decltype(Func{}(
                        detail::left_closed_directed_t{}, Args{}...))
                    delegate(SignedSignificandBits s, Func f, Args... args) noexcept {
                        return s.is_negative() ? f(detail::right_closed_directed_t{}, args...)
                                               : f(detail::left_closed_directed_t{}, args...);
                    }
                } toward_minus_infinity = {};

                JKJ_INLINE_VARIABLE struct toward_zero_t {
                    using decimal_to_binary_rounding_policy = toward_zero_t;

                    template <class SignedSignificandBits, class Func, class... Args>
                    JKJ_FORCEINLINE JKJ_SAFEBUFFERS static constexpr decltype(Func{}(
                        detail::left_closed_directed_t{}, Args{}...))
                    delegate(SignedSignificandBits, Func f, Args... args) noexcept {
                        return f(detail::left_closed_directed_t{}, args...);
                    }
                } toward_zero = {};

                JKJ_INLINE_VARIABLE struct away_from_zero_t {
                    using decimal_to_binary_rounding_policy = away_from_zero_t;

                    template <class SignedSignificandBits, class Func, class... Args>
                    JKJ_FORCEINLINE JKJ_SAFEBUFFERS static constexpr decltype(Func{}(
                        detail::right_closed_directed_t{}, Args{}...))
                    delegate(SignedSignificandBits, Func f, Args... args) noexcept {
                        return f(detail::right_closed_directed_t{}, args...);
                    }
                } away_from_zero = {};
            }

            namespace binary_to_decimal_rounding {
                // (Always assumes nearest rounding modes, as there can be no tie for other rounding
                // modes.)
                enum class tag_t { do_not_care, to_even, to_odd, away_from_zero, toward_zero };

                // The parameter significand corresponds to 10\tilde{s}+t in the paper.

                JKJ_INLINE_VARIABLE struct do_not_care_t {
                    using binary_to_decimal_rounding_policy = do_not_care_t;
                    static constexpr auto tag = tag_t::do_not_care;

                    template <class CarrierUInt>
                    static constexpr bool prefer_round_down(CarrierUInt) noexcept {
                        return false;
                    }
                } do_not_care = {};

                JKJ_INLINE_VARIABLE struct to_even_t {
                    using binary_to_decimal_rounding_policy = to_even_t;
                    static constexpr auto tag = tag_t::to_even;

                    template <class CarrierUInt>
                    static constexpr bool prefer_round_down(CarrierUInt significand) noexcept {
                        return significand % 2 != 0;
                    }
                } to_even = {};

                JKJ_INLINE_VARIABLE struct to_odd_t {
                    using binary_to_decimal_rounding_policy = to_odd_t;
                    static constexpr auto tag = tag_t::to_odd;

                    template <class CarrierUInt>
                    static constexpr bool prefer_round_down(CarrierUInt significand) noexcept {
                        return significand % 2 == 0;
                    }
                } to_odd = {};

                JKJ_INLINE_VARIABLE struct away_from_zero_t {
                    using binary_to_decimal_rounding_policy = away_from_zero_t;
                    static constexpr auto tag = tag_t::away_from_zero;

                    template <class CarrierUInt>
                    static constexpr bool prefer_round_down(CarrierUInt) noexcept {
                        return false;
                    }
                } away_from_zero = {};

                JKJ_INLINE_VARIABLE struct toward_zero_t {
                    using binary_to_decimal_rounding_policy = toward_zero_t;
                    static constexpr auto tag = tag_t::toward_zero;

                    template <class CarrierUInt>
                    static constexpr bool prefer_round_down(CarrierUInt) noexcept {
                        return true;
                    }
                } toward_zero = {};
            }

            namespace cache {
                JKJ_INLINE_VARIABLE struct full_t {
                    using cache_policy = full_t;
                    template <class FloatFormat>
                    using cache_holder_type = cache_holder<FloatFormat>;

                    template <class FloatFormat, class ShiftAmountType, class DecimalExponentType>
                    static constexpr typename cache_holder_type<FloatFormat>::cache_entry_type
                    get_cache(DecimalExponentType k) noexcept {
#if JKJ_HAS_CONSTEXPR14
                        assert(k >= cache_holder_type<FloatFormat>::min_k &&
                               k <= cache_holder_type<FloatFormat>::max_k);
#endif
                        return cache_holder_type<FloatFormat>::cache[detail::stdr::size_t(
                            k - cache_holder_type<FloatFormat>::min_k)];
                    }
                } full = {};

                JKJ_INLINE_VARIABLE struct compact_t {
                    using cache_policy = compact_t;
                    template <class FloatFormat>
                    using cache_holder_type = compressed_cache_holder<FloatFormat>;

                    template <class FloatFormat, class ShiftAmountType, class DecimalExponentType>
                    static JKJ_CONSTEXPR20 typename cache_holder<FloatFormat>::cache_entry_type
                    get_cache(DecimalExponentType k) noexcept {
                        assert(k >= cache_holder<FloatFormat>::min_k &&
                               k <= cache_holder<FloatFormat>::max_k);

                        return cache_holder_type<FloatFormat>::template get_cache<ShiftAmountType>(k);
                    }
                } compact = {};
            }

            namespace preferred_integer_types {
                JKJ_INLINE_VARIABLE struct match_t {
                    using preferred_integer_types_policy = match_t;

                    template <class FormatTraits, detail::stdr::uint_least64_t upper_bound>
                    using remainder_type = typename FormatTraits::carrier_uint;

                    template <class FormatTraits, detail::stdr::int_least32_t lower_bound,
                              detail::stdr::uint_least32_t upper_bound>
                    using decimal_exponent_type = typename FormatTraits::exponent_int;

                    template <class FormatTraits>
                    using shift_amount_type = typename FormatTraits::exponent_int;
                } match;

                JKJ_INLINE_VARIABLE struct prefer_32_t {
                    using preferred_integer_types_policy = prefer_32_t;

                    // Parentheses around min/max are to prevent macro expansions (e.g. in Windows.h).
                    template <class FormatTraits, detail::stdr::uint_least64_t upper_bound>
                    using remainder_type = typename detail::stdr::conditional<
                        upper_bound <=
                            (detail::stdr::numeric_limits<detail::stdr::uint_least32_t>::max)(),
                        detail::stdr::uint_least32_t, typename FormatTraits::carrier_uint>::type;

                    template <class FormatTraits, detail::stdr::int_least32_t lower_bound,
                              detail::stdr::uint_least32_t upper_bound>
                    using decimal_exponent_type = typename detail::stdr::conditional<
                        FormatTraits::format::exponent_bits <=
                            detail::value_bits<detail::stdr::int_least32_t>::value,
                        detail::stdr::int_least32_t, typename FormatTraits::exponent_int>::type;

                    template <class FormatTraits>
                    using shift_amount_type = detail::stdr::int_least32_t;
                } prefer_32;

                JKJ_INLINE_VARIABLE struct minimal_t {
                    using preferred_integer_types_policy = minimal_t;

                    // Parentheses around min/max are to prevent macro expansions (e.g. in Windows.h).
                    template <class FormatTraits, detail::stdr::uint_least64_t upper_bound>
                    using remainder_type = typename detail::stdr::conditional<
                        upper_bound <=
                            (detail::stdr::numeric_limits<detail::stdr::uint_least8_t>::max)(),
                        detail::stdr::uint_least8_t,
                        typename detail::stdr::conditional<
                            upper_bound <=
                                (detail::stdr::numeric_limits<detail::stdr::uint_least16_t>::max)(),
                            detail::stdr::uint_least16_t,
                            typename detail::stdr::conditional<
                                upper_bound <=
                                    (detail::stdr::numeric_limits<detail::stdr::uint_least32_t>::max)(),
                                detail::stdr::uint_least32_t,
                                typename detail::stdr::conditional<
                                    upper_bound <= (detail::stdr::numeric_limits<
                                                       detail::stdr::uint_least64_t>::max)(),
                                    detail::stdr::uint_least64_t,
                                    typename FormatTraits::carrier_uint>::type>::type>::type>::type;

                    // Parentheses around min/max are to prevent macro expansions (e.g. in Windows.h).
                    template <class FormatTraits, detail::stdr::int_least32_t lower_bound,
                              detail::stdr::uint_least32_t upper_bound>
                    using decimal_exponent_type = typename detail::stdr::conditional<
                        lower_bound >=
                                (detail::stdr::numeric_limits<detail::stdr::int_least8_t>::min)() &&
                            upper_bound <=
                                (detail::stdr::numeric_limits<detail::stdr::int_least8_t>::max)(),
                        detail::stdr::int_least8_t,
                        typename detail::stdr::conditional<
                            lower_bound >= (detail::stdr::numeric_limits<
                                               detail::stdr::int_least16_t>::min)() &&
                                upper_bound <=
                                    (detail::stdr::numeric_limits<detail::stdr::int_least16_t>::max)(),
                            detail::stdr::int_least16_t,
                            typename detail::stdr::conditional<
                                lower_bound >= (detail::stdr::numeric_limits<
                                                   detail::stdr::int_least32_t>::min)() &&
                                    upper_bound <= (detail::stdr::numeric_limits<
                                                       detail::stdr::int_least32_t>::max)(),
                                detail::stdr::int_least32_t,
                                typename FormatTraits::exponent_int>::type>::type>::type;

                    template <class FormatTraits>
                    using shift_amount_type = detail::stdr::int_least8_t;
                } minimal;
            }
        }

        ////////////////////////////////////////////////////////////////////////////////////////
        // Specializations of user-specializable templates used in the main algorithm.
        ////////////////////////////////////////////////////////////////////////////////////////

        template <class DecimalExponentType>
        struct remove_trailing_zeros_traits<policy::trailing_zero::remove_t, ieee754_binary32,
                                            detail::stdr::uint_least32_t, DecimalExponentType> {
            JKJ_FORCEINLINE static JKJ_CONSTEXPR14 void
            remove_trailing_zeros(detail::stdr::uint_least32_t& significand,
                                  DecimalExponentType& exponent) noexcept {
                // See https://github.com/jk-jeon/rtz_benchmark.
                // The idea of branchless search below is by reddit users r/pigeon768 and
                // r/TheoreticalDumbass.

                auto r = detail::bits::rotr<32>(
                    detail::stdr::uint_least32_t(significand * UINT32_C(184254097)), 4);
                auto b = r < UINT32_C(429497);
                auto s = detail::stdr::size_t(b);
                significand = b ? r : significand;

                r = detail::bits::rotr<32>(
                    detail::stdr::uint_least32_t(significand * UINT32_C(42949673)), 2);
                b = r < UINT32_C(42949673);
                s = s * 2 + b;
                significand = b ? r : significand;

                r = detail::bits::rotr<32>(
                    detail::stdr::uint_least32_t(significand * UINT32_C(1288490189)), 1);
                b = r < UINT32_C(429496730);
                s = s * 2 + b;
                significand = b ? r : significand;

                exponent += s;
            }
        };

        template <class DecimalExponentType>
        struct remove_trailing_zeros_traits<policy::trailing_zero::remove_t, ieee754_binary64,
                                            detail::stdr::uint_least64_t, DecimalExponentType> {
            JKJ_FORCEINLINE static JKJ_CONSTEXPR14 void
            remove_trailing_zeros(detail::stdr::uint_least64_t& significand,
                                  DecimalExponentType& exponent) noexcept {
                // See https://github.com/jk-jeon/rtz_benchmark.
                // The idea of branchless search below is by reddit users r/pigeon768 and
                // r/TheoreticalDumbass.

                auto r = detail::bits::rotr<64>(
                    detail::stdr::uint_least64_t(significand * UINT64_C(28999941890838049)), 8);
                auto b = r < UINT64_C(184467440738);
                auto s = detail::stdr::size_t(b);
                significand = b ? r : significand;

                r = detail::bits::rotr<64>(
                    detail::stdr::uint_least64_t(significand * UINT64_C(182622766329724561)), 4);
                b = r < UINT64_C(1844674407370956);
                s = s * 2 + b;
                significand = b ? r : significand;

                r = detail::bits::rotr<64>(
                    detail::stdr::uint_least64_t(significand * UINT64_C(10330176681277348905)), 2);
                b = r < UINT64_C(184467440737095517);
                s = s * 2 + b;
                significand = b ? r : significand;

                r = detail::bits::rotr<64>(
                    detail::stdr::uint_least64_t(significand * UINT64_C(14757395258967641293)), 1);
                b = r < UINT64_C(1844674407370955162);
                s = s * 2 + b;
                significand = b ? r : significand;

                exponent += s;
            }
        };

        template <class DecimalExponentType>
        struct remove_trailing_zeros_traits<policy::trailing_zero::remove_compact_t, ieee754_binary32,
                                            detail::stdr::uint_least32_t, DecimalExponentType> {
            JKJ_FORCEINLINE static JKJ_CONSTEXPR14 void
            remove_trailing_zeros(detail::stdr::uint_least32_t& significand,
                                  DecimalExponentType& exponent) noexcept {
                // See https://github.com/jk-jeon/rtz_benchmark.
                while (true) {
                    auto const r = detail::stdr::uint_least32_t(significand * UINT32_C(1288490189));
                    if (r < UINT32_C(429496731)) {
                        significand = detail::stdr::uint_least32_t(r >> 1);
                        exponent += 1;
                    }
                    else {
                        break;
                    }
                }
            }
        };

        template <class DecimalExponentType>
        struct remove_trailing_zeros_traits<policy::trailing_zero::remove_compact_t, ieee754_binary64,
                                            detail::stdr::uint_least64_t, DecimalExponentType> {
            JKJ_FORCEINLINE static JKJ_CONSTEXPR14 void
            remove_trailing_zeros(detail::stdr::uint_least64_t& significand,
                                  DecimalExponentType& exponent) noexcept {
                // See https://github.com/jk-jeon/rtz_benchmark.
                while (true) {
                    auto const r =
                        detail::stdr::uint_least64_t(significand * UINT64_C(5534023222112865485));
                    if (r < UINT64_C(1844674407370955163)) {
                        significand = detail::stdr::uint_least64_t(r >> 1);
                        exponent += 1;
                    }
                    else {
                        break;
                    }
                }
            }
        };

        template <class ExponentInt>
        struct multiplication_traits<
            ieee754_binary_traits<ieee754_binary32, detail::stdr::uint_least32_t, ExponentInt>,
            detail::stdr::uint_least64_t, 64>
            : public multiplication_traits_base<
                  ieee754_binary_traits<ieee754_binary32, detail::stdr::uint_least32_t>,
                  detail::stdr::uint_least64_t, 64> {
            static JKJ_CONSTEXPR20 compute_mul_result
            compute_mul(carrier_uint u, cache_entry_type const& cache) noexcept {
                auto const r = detail::wuint::umul96_upper64(u, cache);
                return {carrier_uint(r >> 32), carrier_uint(r) == 0};
            }

            template <class ShiftAmountType>
            static constexpr detail::stdr::uint_least64_t compute_delta(cache_entry_type const& cache,
                                                                        ShiftAmountType beta) noexcept {
                return detail::stdr::uint_least64_t(cache >> ShiftAmountType(cache_bits - 1 - beta));
            }

            template <class ShiftAmountType>
            static JKJ_CONSTEXPR20 compute_mul_parity_result compute_mul_parity(
                carrier_uint two_f, cache_entry_type const& cache, ShiftAmountType beta) noexcept {
                assert(beta >= 1);
                assert(beta <= 32);

                auto const r = detail::wuint::umul96_lower64(two_f, cache);
                return {((r >> ShiftAmountType(64 - beta)) & 1) != 0,
                        (UINT32_C(0xffffffff) & (r >> ShiftAmountType(32 - beta))) == 0};
            }

            template <class ShiftAmountType>
            static constexpr carrier_uint
            compute_left_endpoint_for_shorter_interval_case(cache_entry_type const& cache,
                                                            ShiftAmountType beta) noexcept {
                return carrier_uint((cache - (cache >> (significand_bits + 2))) >>
                                    ShiftAmountType(cache_bits - significand_bits - 1 - beta));
            }

            template <class ShiftAmountType>
            static constexpr carrier_uint
            compute_right_endpoint_for_shorter_interval_case(cache_entry_type const& cache,
                                                             ShiftAmountType beta) noexcept {
                return carrier_uint((cache + (cache >> (significand_bits + 1))) >>
                                    ShiftAmountType(cache_bits - significand_bits - 1 - beta));
            }

            template <class ShiftAmountType>
            static constexpr carrier_uint
            compute_round_up_for_shorter_interval_case(cache_entry_type const& cache,
                                                       ShiftAmountType beta) noexcept {
                return (carrier_uint(cache >>
                                     ShiftAmountType(cache_bits - significand_bits - 2 - beta)) +
                        1) /
                       2;
            }
        };

        template <class ExponentInt>
        struct multiplication_traits<
            ieee754_binary_traits<ieee754_binary64, detail::stdr::uint_least64_t, ExponentInt>,
            detail::wuint::uint128, 128>
            : public multiplication_traits_base<
                  ieee754_binary_traits<ieee754_binary64, detail::stdr::uint_least64_t>,
                  detail::wuint::uint128, 128> {
            static JKJ_CONSTEXPR20 compute_mul_result
            compute_mul(carrier_uint u, cache_entry_type const& cache) noexcept {
                auto const r = detail::wuint::umul192_upper128(u, cache);
                return {r.high(), r.low() == 0};
            }

            template <class ShiftAmountType>
            static constexpr detail::stdr::uint_least64_t compute_delta(cache_entry_type const& cache,
                                                                        ShiftAmountType beta) noexcept {
                return detail::stdr::uint_least64_t(cache.high() >>
                                                    ShiftAmountType(total_bits - 1 - beta));
            }

            template <class ShiftAmountType>
            static JKJ_CONSTEXPR20 compute_mul_parity_result compute_mul_parity(
                carrier_uint two_f, cache_entry_type const& cache, ShiftAmountType beta) noexcept {
                assert(beta >= 1);
                assert(beta < 64);

                auto const r = detail::wuint::umul192_lower128(two_f, cache);
                return {((r.high() >> ShiftAmountType(64 - beta)) & 1) != 0,
                        (((r.high() << beta) & UINT64_C(0xffffffffffffffff)) |
                         (r.low() >> ShiftAmountType(64 - beta))) == 0};
            }

            template <class ShiftAmountType>
            static constexpr carrier_uint
            compute_left_endpoint_for_shorter_interval_case(cache_entry_type const& cache,
                                                            ShiftAmountType beta) noexcept {
                return (cache.high() - (cache.high() >> (significand_bits + 2))) >>
                       ShiftAmountType(total_bits - significand_bits - 1 - beta);
            }

            template <class ShiftAmountType>
            static constexpr carrier_uint
            compute_right_endpoint_for_shorter_interval_case(cache_entry_type const& cache,
                                                             ShiftAmountType beta) noexcept {
                return (cache.high() + (cache.high() >> (significand_bits + 1))) >>
                       ShiftAmountType(total_bits - significand_bits - 1 - beta);
            }

            template <class ShiftAmountType>
            static constexpr carrier_uint
            compute_round_up_for_shorter_interval_case(cache_entry_type const& cache,
                                                       ShiftAmountType beta) noexcept {
                return ((cache.high() >> ShiftAmountType(total_bits - significand_bits - 2 - beta)) +
                        1) /
                       2;
            }
        };

        namespace detail {
            ////////////////////////////////////////////////////////////////////////////////////////
            // The main algorithm.
            ////////////////////////////////////////////////////////////////////////////////////////

            template <class FormatTraits>
            struct impl : private FormatTraits::format {
                using format = typename FormatTraits::format;
                using carrier_uint = typename FormatTraits::carrier_uint;
                static constexpr int carrier_bits = FormatTraits::carrier_bits;
                using exponent_int = typename FormatTraits::exponent_int;

                using format::significand_bits;
                using format::min_exponent;
                using format::max_exponent;
                using format::exponent_bias;

                static constexpr int kappa =
                    log::floor_log10_pow2(carrier_bits - significand_bits - 2) - 1;
                static_assert(kappa >= 1, "");
                static_assert(carrier_bits >= significand_bits + 2 + log::floor_log2_pow10(kappa + 1),
                              "");

                // Trailing underscores are to avoid name clash with macros (e.g. Windows.h).
                static constexpr int min_(int x, int y) noexcept { return x < y ? x : y; }
                static constexpr int max_(int x, int y) noexcept { return x > y ? x : y; }

                static constexpr int min_k =
                    min_(-log::floor_log10_pow2_minus_log10_4_over_3(max_exponent - significand_bits),
                         -log::floor_log10_pow2(max_exponent - significand_bits) + kappa);

                // We do invoke shorter_interval_case for exponent == min_exponent case,
                // so we should not add 1 here.
                static constexpr int max_k =
                    max_(-log::floor_log10_pow2_minus_log10_4_over_3(min_exponent -
                                                                     significand_bits /*+ 1*/),
                         -log::floor_log10_pow2(min_exponent - significand_bits) + kappa);

                static constexpr int case_shorter_interval_left_endpoint_lower_threshold = 2;
                static constexpr int case_shorter_interval_left_endpoint_upper_threshold =
                    2 +
                    log::floor_log2(
                        compute_power<
                            count_factors<5>((carrier_uint(1) << (significand_bits + 2)) - 1) + 1>(10) /
                        3);

                static constexpr int case_shorter_interval_right_endpoint_lower_threshold = 0;
                static constexpr int case_shorter_interval_right_endpoint_upper_threshold =
                    2 +
                    log::floor_log2(
                        compute_power<
                            count_factors<5>((carrier_uint(1) << (significand_bits + 1)) + 1) + 1>(10) /
                        3);

                static constexpr int shorter_interval_tie_lower_threshold =
                    -log::floor_log5_pow2_minus_log5_3(significand_bits + 4) - 2 - significand_bits;
                static constexpr int shorter_interval_tie_upper_threshold =
                    -log::floor_log5_pow2(significand_bits + 2) - 2 - significand_bits;

                template <class PreferredIntegerTypesPolicy>
                using remainder_type = typename PreferredIntegerTypesPolicy::template remainder_type<
                    FormatTraits, compute_power<kappa + 1>(detail::stdr::uint_least64_t(10))>;

                template <class PreferredIntegerTypesPolicy>
                using decimal_exponent_type =
                    typename PreferredIntegerTypesPolicy::template decimal_exponent_type<
                        FormatTraits, detail::stdr::int_least32_t(min_(-max_k, min_k)),
                        detail::stdr::int_least32_t(max_(max_k, -min_k + kappa + 1))>;

                template <class SignPolicy, class TrailingZeroPolicy, class PreferredIntegerTypesPolicy>
                using return_type =
                    decimal_fp<carrier_uint, decimal_exponent_type<PreferredIntegerTypesPolicy>,
                               SignPolicy::return_has_sign, TrailingZeroPolicy::report_trailing_zeros>;

                //// The main algorithm assumes the input is a normal/subnormal finite number.

                template <class SignPolicy, class TrailingZeroPolicy, class IntervalTypeProvider,
                          class BinaryToDecimalRoundingPolicy, class CachePolicy,
                          class PreferredIntegerTypesPolicy>
                JKJ_SAFEBUFFERS static JKJ_CONSTEXPR20
                    return_type<SignPolicy, TrailingZeroPolicy, PreferredIntegerTypesPolicy>
                    compute_nearest(signed_significand_bits<FormatTraits> s,
                                    exponent_int exponent_bits) noexcept {
                    using cache_holder_type = typename CachePolicy::template cache_holder_type<format>;
                    static_assert(
                        min_k >= cache_holder_type::min_k && max_k <= cache_holder_type::max_k, "");

                    using remainder_type_ = remainder_type<PreferredIntegerTypesPolicy>;
                    using decimal_exponent_type_ = decimal_exponent_type<PreferredIntegerTypesPolicy>;
                    using shift_amount_type =
                        typename PreferredIntegerTypesPolicy::template shift_amount_type<FormatTraits>;

                    using multiplication_traits_ =
                        multiplication_traits<FormatTraits,
                                              typename cache_holder_type::cache_entry_type,
                                              cache_holder_type::cache_bits>;

                    auto two_fc = s.remove_sign_bit_and_shift();
                    auto binary_exponent = exponent_bits;

                    // Is the input a normal number?
                    if (binary_exponent != 0) {
                        binary_exponent += format::exponent_bias - format::significand_bits;

                        // Shorter interval case; proceed like Schubfach.
                        // One might think this condition is wrong, since when exponent_bits ==
                        // 1 and two_fc == 0, the interval is actually regular. However, it
                        // turns out that this seemingly wrong condition is actually fine,
                        // because the end result is anyway the same.
                        //
                        // [binary32]
                        // (fc-1/2) * 2^e = 1.175'494'28... * 10^-38
                        // (fc-1/4) * 2^e = 1.175'494'31... * 10^-38
                        //    fc    * 2^e = 1.175'494'35... * 10^-38
                        // (fc+1/2) * 2^e = 1.175'494'42... * 10^-38
                        //
                        // Hence, shorter_interval_case will return 1.175'494'4 * 10^-38.
                        // 1.175'494'3 * 10^-38 is also a correct shortest representation that
                        // will be rejected if we assume shorter interval, but 1.175'494'4 *
                        // 10^-38 is closer to the true value so it doesn't matter.
                        //
                        // [binary64]
                        // (fc-1/2) * 2^e = 2.225'073'858'507'201'13... * 10^-308
                        // (fc-1/4) * 2^e = 2.225'073'858'507'201'25... * 10^-308
                        //    fc    * 2^e = 2.225'073'858'507'201'38... * 10^-308
                        // (fc+1/2) * 2^e = 2.225'073'858'507'201'63... * 10^-308
                        //
                        // Hence, shorter_interval_case will return 2.225'073'858'507'201'4 *
                        // 10^-308. This is indeed of the shortest length, and it is the unique
                        // one closest to the true value among valid representations of the same
                        // length.
                        static_assert(stdr::is_same<format, ieee754_binary32>::value ||
                                          stdr::is_same<format, ieee754_binary64>::value,
                                      "");

                        // Shorter interval case.
                        if (two_fc == 0) {
                            auto interval_type = IntervalTypeProvider::shorter_interval(s);

                            // Compute k and beta.
                            auto const minus_k = log::floor_log10_pow2_minus_log10_4_over_3<
                                min_exponent - format::significand_bits,
                                max_exponent - format::significand_bits, decimal_exponent_type_>(
                                binary_exponent);
                            auto const beta = shift_amount_type(
                                binary_exponent +
                                log::floor_log2_pow10<min_k, max_k>(decimal_exponent_type_(-minus_k)));

                            // Compute xi and zi.
                            auto const cache =
                                CachePolicy::template get_cache<format, shift_amount_type>(
                                    decimal_exponent_type_(-minus_k));

                            auto xi =
                                multiplication_traits_::compute_left_endpoint_for_shorter_interval_case(
                                    cache, beta);
                            auto zi = multiplication_traits_::
                                compute_right_endpoint_for_shorter_interval_case(cache, beta);

                            // If we don't accept the right endpoint and
                            // if the right endpoint is an integer, decrease it.
                            if (!interval_type.include_right_endpoint() &&
                                is_right_endpoint_integer_shorter_interval(binary_exponent)) {
                                --zi;
                            }
                            // If we don't accept the left endpoint or
                            // if the left endpoint is not an integer, increase it.
                            if (!interval_type.include_left_endpoint() ||
                                !is_left_endpoint_integer_shorter_interval(binary_exponent)) {
                                ++xi;
                            }

                            // Try bigger divisor.
                            // zi is at most floor((f_c + 1/2) * 2^e * 10^k0).
                            // Substituting f_c = 2^p and k0 = -floor(log10(3 * 2^(e-2))), we get
                            // zi <= floor((2^(p+1) + 1) * 20/3) <= ceil((2^(p+1) + 1)/3) * 20.
                            // This computation does not overflow for any of the formats I care about.
                            carrier_uint decimal_significand = div::divide_by_pow10<
                                1, carrier_uint,
                                carrier_uint(
                                    ((((carrier_uint(1) << (significand_bits + 1)) + 1) / 3) + 1) *
                                    20)>(zi);

                            // If succeed, remove trailing zeros if necessary and return.
                            if (decimal_significand * 10 >= xi) {
                                return SignPolicy::handle_sign(
                                    s, TrailingZeroPolicy::template on_trailing_zeros<format>(
                                           decimal_significand, decimal_exponent_type_(minus_k + 1)));
                            }

                            // Otherwise, compute the round-up of y.
                            decimal_significand =
                                multiplication_traits_::compute_round_up_for_shorter_interval_case(
                                    cache, beta);

                            // When tie occurs, choose one of them according to the rule.
                            if (BinaryToDecimalRoundingPolicy::prefer_round_down(decimal_significand) &&
                                binary_exponent >= shorter_interval_tie_lower_threshold &&
                                binary_exponent <= shorter_interval_tie_upper_threshold) {
                                --decimal_significand;
                            }
                            else if (decimal_significand < xi) {
                                ++decimal_significand;
                            }
                            return SignPolicy::handle_sign(
                                s, TrailingZeroPolicy::template no_trailing_zeros<format>(
                                       decimal_significand, decimal_exponent_type_(minus_k)));
                        }

                        // Normal interval case.
                        two_fc |= (carrier_uint(1) << (format::significand_bits + 1));
                    }
                    // Is the input a subnormal number?
                    else {
                        // Normal interval case.
                        binary_exponent = format::min_exponent - format::significand_bits;
                    }

                    //////////////////////////////////////////////////////////////////////
                    // Step 1: Schubfach multiplier calculation.
                    //////////////////////////////////////////////////////////////////////

                    auto interval_type = IntervalTypeProvider::normal_interval(s);

                    // Compute k and beta.
                    auto const minus_k = decimal_exponent_type_(
                        log::floor_log10_pow2<min_exponent - format::significand_bits,
                                              max_exponent - format::significand_bits,
                                              decimal_exponent_type_>(binary_exponent) -
                        kappa);
                    auto const cache = CachePolicy::template get_cache<format, shift_amount_type>(
                        decimal_exponent_type_(-minus_k));
                    auto const beta =
                        shift_amount_type(binary_exponent + log::floor_log2_pow10<min_k, max_k>(
                                                                decimal_exponent_type_(-minus_k)));

                    // Compute zi and deltai.
                    // 10^kappa <= deltai < 10^(kappa + 1)
                    auto const deltai = static_cast<remainder_type_>(
                        multiplication_traits_::compute_delta(cache, beta));
                    // For the case of binary32, the result of integer check is not correct for
                    // 29711844 * 2^-82
                    // = 6.1442653300000000008655037797566933477355632930994033813476... * 10^-18
                    // and 29711844 * 2^-81
                    // = 1.2288530660000000001731007559513386695471126586198806762695... * 10^-17,
                    // and they are the unique counterexamples. However, since 29711844 is even,
                    // this does not cause any problem for the endpoints calculations; it can only
                    // cause a problem when we need to perform integer check for the center.
                    // Fortunately, with these inputs, that branch is never executed, so we are
                    // fine.
                    auto const z_result =
                        multiplication_traits_::compute_mul(carrier_uint((two_fc | 1) << beta), cache);


                    //////////////////////////////////////////////////////////////////////
                    // Step 2: Try larger divisor; remove trailing zeros if necessary.
                    //////////////////////////////////////////////////////////////////////

                    constexpr auto big_divisor = compute_power<kappa + 1>(remainder_type_(10));
                    constexpr auto small_divisor = compute_power<kappa>(remainder_type_(10));

                    // Using an upper bound on zi, we might be able to optimize the division
                    // better than the compiler; we are computing zi / big_divisor here.
                    carrier_uint decimal_significand = div::divide_by_pow10<
                        kappa + 1, carrier_uint,
                        carrier_uint((carrier_uint(1) << (significand_bits + 1)) * big_divisor - 1)>(
                        z_result.integer_part);
                    auto r = remainder_type_(z_result.integer_part - big_divisor * decimal_significand);

                    do {
                        if (r < deltai) {
                            // Exclude the right endpoint if necessary.
                            if ((r | remainder_type_(!z_result.is_integer) |
                                 remainder_type_(interval_type.include_right_endpoint())) == 0) {
                                JKJ_IF_CONSTEXPR(
                                    BinaryToDecimalRoundingPolicy::tag ==
                                    policy::binary_to_decimal_rounding::tag_t::do_not_care) {
                                    decimal_significand *= 10;
                                    --decimal_significand;
                                    return SignPolicy::handle_sign(
                                        s, TrailingZeroPolicy::template no_trailing_zeros<format>(
                                               decimal_significand,
                                               decimal_exponent_type_(minus_k + kappa)));
                                }
                                else {
                                    --decimal_significand;
                                    r = big_divisor;
                                    break;
                                }
                            }
                        }
                        else if (r > deltai) {
                            break;
                        }
                        else {
                            // r == deltai; compare fractional parts.
                            auto const x_result = multiplication_traits_::compute_mul_parity(
                                carrier_uint(two_fc - 1), cache, beta);

                            if (!(x_result.parity |
                                  (x_result.is_integer & interval_type.include_left_endpoint()))) {
                                break;
                            }
                        }

                        // We may need to remove trailing zeros.
                        return SignPolicy::handle_sign(
                            s, TrailingZeroPolicy::template on_trailing_zeros<format>(
                                   decimal_significand, decimal_exponent_type_(minus_k + kappa + 1)));
                    } while (false);


                    //////////////////////////////////////////////////////////////////////
                    // Step 3: Find the significand with the smaller divisor.
                    //////////////////////////////////////////////////////////////////////

                    decimal_significand *= 10;

                    JKJ_IF_CONSTEXPR(BinaryToDecimalRoundingPolicy::tag ==
                                     policy::binary_to_decimal_rounding::tag_t::do_not_care) {
                        // Normally, we want to compute
                        // significand += r / small_divisor
                        // and return, but we need to take care of the case that the resulting
                        // value is exactly the right endpoint, while that is not included in the
                        // interval.
                        if (!interval_type.include_right_endpoint()) {
                            // Is r divisible by 10^kappa?
                            if (div::check_divisibility_and_divide_by_pow10<kappa>(r) &&
                                z_result.is_integer) {
                                // This should be in the interval.
                                decimal_significand += r - 1;
                            }
                            else {
                                decimal_significand += r;
                            }
                        }
                        else {
                            decimal_significand += div::small_division_by_pow10<kappa>(r);
                        }
                    }
                    else {
                        // delta is equal to 10^(kappa + elog10(2) - floor(elog10(2))), so dist cannot
                        // be larger than r.
                        auto dist = remainder_type_(r - (deltai / 2) + (small_divisor / 2));
                        bool const approx_y_parity = ((dist ^ (small_divisor / 2)) & 1) != 0;

                        // Is dist divisible by 10^kappa?
                        bool const divisible_by_small_divisor =
                            div::check_divisibility_and_divide_by_pow10<kappa>(dist);

                        // Add dist / 10^kappa to the significand.
                        decimal_significand += dist;

                        if (divisible_by_small_divisor) {
                            // Check z^(f) >= epsilon^(f).
                            // We have either yi == zi - epsiloni or yi == (zi - epsiloni) - 1,
                            // where yi == zi - epsiloni if and only if z^(f) >= epsilon^(f).
                            // Since there are only 2 possibilities, we only need to care about the
                            // parity. Also, zi and r should have the same parity since the divisor
                            // is an even number.
                            auto const y_result =
                                multiplication_traits_::compute_mul_parity(two_fc, cache, beta);
                            if (y_result.parity != approx_y_parity) {
                                --decimal_significand;
                            }
                            else {
                                // If z^(f) >= epsilon^(f), we might have a tie
                                // when z^(f) == epsilon^(f), or equivalently, when y is an integer.
                                // For tie-to-up case, we can just choose the upper one.
                                if (BinaryToDecimalRoundingPolicy::prefer_round_down(
                                        decimal_significand) &
                                    y_result.is_integer) {
                                    --decimal_significand;
                                }
                            }
                        }
                    }
                    return SignPolicy::handle_sign(
                        s, TrailingZeroPolicy::template no_trailing_zeros<format>(
                               decimal_significand, decimal_exponent_type_(minus_k + kappa)));
                }

                template <class SignPolicy, class TrailingZeroPolicy, class CachePolicy,
                          class PreferredIntegerTypesPolicy>
                JKJ_FORCEINLINE JKJ_SAFEBUFFERS static JKJ_CONSTEXPR20
                    return_type<SignPolicy, TrailingZeroPolicy, PreferredIntegerTypesPolicy>
                    compute_left_closed_directed(signed_significand_bits<FormatTraits> s,
                                                 exponent_int exponent_bits) noexcept {
                    using cache_holder_type = typename CachePolicy::template cache_holder_type<format>;
                    static_assert(
                        min_k >= cache_holder_type::min_k && max_k <= cache_holder_type::max_k, "");

                    using remainder_type_ = remainder_type<PreferredIntegerTypesPolicy>;
                    using decimal_exponent_type_ = decimal_exponent_type<PreferredIntegerTypesPolicy>;
                    using shift_amount_type =
                        typename PreferredIntegerTypesPolicy::template shift_amount_type<FormatTraits>;

                    using multiplication_traits_ =
                        multiplication_traits<FormatTraits,
                                              typename cache_holder_type::cache_entry_type,
                                              cache_holder_type::cache_bits>;

                    auto two_fc = s.remove_sign_bit_and_shift();
                    auto binary_exponent = exponent_bits;

                    // Is the input a normal number?
                    if (binary_exponent != 0) {
                        binary_exponent += format::exponent_bias - format::significand_bits;
                        two_fc |= (carrier_uint(1) << (format::significand_bits + 1));
                    }
                    // Is the input a subnormal number?
                    else {
                        binary_exponent = format::min_exponent - format::significand_bits;
                    }

                    //////////////////////////////////////////////////////////////////////
                    // Step 1: Schubfach multiplier calculation.
                    //////////////////////////////////////////////////////////////////////

                    // Compute k and beta.
                    auto const minus_k = decimal_exponent_type_(
                        log::floor_log10_pow2<format::min_exponent - format::significand_bits,
                                              format::max_exponent - format::significand_bits,
                                              decimal_exponent_type_>(binary_exponent) -
                        kappa);
                    auto const cache = CachePolicy::template get_cache<format, shift_amount_type>(
                        decimal_exponent_type_(-minus_k));
                    auto const beta =
                        shift_amount_type(binary_exponent + log::floor_log2_pow10<min_k, max_k>(
                                                                decimal_exponent_type_(-minus_k)));

                    // Compute xi and deltai.
                    // 10^kappa <= deltai < 10^(kappa + 1)
                    auto const deltai = static_cast<remainder_type_>(
                        multiplication_traits_::compute_delta(cache, beta));
                    auto x_result =
                        multiplication_traits_::compute_mul(carrier_uint(two_fc << beta), cache);

                    // Deal with the unique exceptional cases
                    // 29711844 * 2^-82
                    // = 6.1442653300000000008655037797566933477355632930994033813476... * 10^-18
                    // and 29711844 * 2^-81
                    // = 1.2288530660000000001731007559513386695471126586198806762695... * 10^-17
                    // for binary32.
                    JKJ_IF_CONSTEXPR(stdr::is_same<format, ieee754_binary32>::value) {
                        if (binary_exponent <= -80) {
                            x_result.is_integer = false;
                        }
                    }

                    if (!x_result.is_integer) {
                        ++x_result.integer_part;
                    }

                    //////////////////////////////////////////////////////////////////////
                    // Step 2: Try larger divisor; remove trailing zeros if necessary.
                    //////////////////////////////////////////////////////////////////////

                    constexpr auto big_divisor = compute_power<kappa + 1>(remainder_type_(10));

                    // Using an upper bound on xi, we might be able to optimize the division
                    // better than the compiler; we are computing xi / big_divisor here.
                    carrier_uint decimal_significand = div::divide_by_pow10<
                        kappa + 1, carrier_uint,
                        carrier_uint((carrier_uint(1) << (significand_bits + 1)) * big_divisor - 1)>(
                        x_result.integer_part);
                    auto r = remainder_type_(x_result.integer_part - big_divisor * decimal_significand);

                    if (r != 0) {
                        ++decimal_significand;
                        r = remainder_type_(big_divisor - r);
                    }

                    do {
                        if (r > deltai) {
                            break;
                        }
                        else if (r == deltai) {
                            // Compare the fractional parts.
                            // This branch is never taken for the exceptional cases
                            // 2f_c = 29711482, e = -81
                            // (6.1442649164096937243516663440523473127541365101933479309082... *
                            // 10^-18) and 2f_c = 29711482, e = -80
                            // (1.2288529832819387448703332688104694625508273020386695861816... *
                            // 10^-17).
                            // For the case of compressed cache for binary32, there is another
                            // exceptional case 2f_c = 33554430, e = -10 (16383.9990234375). In this
                            // case, the recovered cache is two large to make compute_mul_parity
                            // mistakenly conclude that z is not an integer, but actually z = 16384 is
                            // an integer.
                            JKJ_IF_CONSTEXPR(
                                stdr::is_same<cache_holder_type,
                                              compressed_cache_holder<ieee754_binary32>>::value) {
                                if (two_fc == 33554430 && binary_exponent == -10) {
                                    break;
                                }
                            }
                            auto const z_result = multiplication_traits_::compute_mul_parity(
                                carrier_uint(two_fc + 2), cache, beta);
                            if (z_result.parity || z_result.is_integer) {
                                break;
                            }
                        }

                        // The ceiling is inside, so we are done.
                        return SignPolicy::handle_sign(
                            s, TrailingZeroPolicy::template on_trailing_zeros<format>(
                                   decimal_significand, decimal_exponent_type_(minus_k + kappa + 1)));
                    } while (false);


                    //////////////////////////////////////////////////////////////////////
                    // Step 3: Find the significand with the smaller divisor.
                    //////////////////////////////////////////////////////////////////////

                    decimal_significand *= 10;
                    decimal_significand -= div::small_division_by_pow10<kappa>(r);
                    return SignPolicy::handle_sign(
                        s, TrailingZeroPolicy::template no_trailing_zeros<format>(
                               decimal_significand, decimal_exponent_type_(minus_k + kappa)));
                }

                template <class SignPolicy, class TrailingZeroPolicy, class CachePolicy,
                          class PreferredIntegerTypesPolicy>
                JKJ_FORCEINLINE JKJ_SAFEBUFFERS static JKJ_CONSTEXPR20
                    return_type<SignPolicy, TrailingZeroPolicy, PreferredIntegerTypesPolicy>
                    compute_right_closed_directed(signed_significand_bits<FormatTraits> s,
                                                  exponent_int exponent_bits) noexcept {
                    using cache_holder_type = typename CachePolicy::template cache_holder_type<format>;
                    static_assert(
                        min_k >= cache_holder_type::min_k && max_k <= cache_holder_type::max_k, "");

                    using remainder_type_ = remainder_type<PreferredIntegerTypesPolicy>;
                    using decimal_exponent_type_ = decimal_exponent_type<PreferredIntegerTypesPolicy>;
                    using shift_amount_type =
                        typename PreferredIntegerTypesPolicy::template shift_amount_type<FormatTraits>;

                    using multiplication_traits_ =
                        multiplication_traits<FormatTraits,
                                              typename cache_holder_type::cache_entry_type,
                                              cache_holder_type::cache_bits>;

                    auto two_fc = s.remove_sign_bit_and_shift();
                    auto binary_exponent = exponent_bits;
                    bool shorter_interval = false;

                    // Is the input a normal number?
                    if (binary_exponent != 0) {
                        if (two_fc == 0 && binary_exponent != 1) {
                            shorter_interval = true;
                        }
                        binary_exponent += format::exponent_bias - format::significand_bits;
                        two_fc |= (carrier_uint(1) << (format::significand_bits + 1));
                    }
                    // Is the input a subnormal number?
                    else {
                        binary_exponent = format::min_exponent - format::significand_bits;
                    }

                    //////////////////////////////////////////////////////////////////////
                    // Step 1: Schubfach multiplier calculation.
                    //////////////////////////////////////////////////////////////////////

                    // Compute k and beta.
                    auto const minus_k = decimal_exponent_type_(
                        log::floor_log10_pow2<format::min_exponent - format::significand_bits,
                                              format::max_exponent - format::significand_bits,
                                              decimal_exponent_type_>(
                            exponent_int(binary_exponent - (shorter_interval ? 1 : 0))) -
                        kappa);
                    auto const cache = CachePolicy::template get_cache<format, shift_amount_type>(
                        decimal_exponent_type_(-minus_k));
                    auto const beta = shift_amount_type(
                        binary_exponent + log::floor_log2_pow10(decimal_exponent_type_(-minus_k)));

                    // Compute zi and deltai.
                    // 10^kappa <= deltai < 10^(kappa + 1)
                    auto const deltai =
                        static_cast<remainder_type_>(multiplication_traits_::compute_delta(
                            cache, shift_amount_type(beta - (shorter_interval ? 1 : 0))));
                    carrier_uint const zi =
                        multiplication_traits_::compute_mul(carrier_uint(two_fc << beta), cache)
                            .integer_part;


                    //////////////////////////////////////////////////////////////////////
                    // Step 2: Try larger divisor; remove trailing zeros if necessary.
                    //////////////////////////////////////////////////////////////////////

                    constexpr auto big_divisor = compute_power<kappa + 1>(remainder_type_(10));

                    // Using an upper bound on zi, we might be able to optimize the division better
                    // than the compiler; we are computing zi / big_divisor here.
                    carrier_uint decimal_significand = div::divide_by_pow10<
                        kappa + 1, carrier_uint,
                        carrier_uint((carrier_uint(1) << (significand_bits + 1)) * big_divisor - 1)>(
                        zi);
                    auto const r = remainder_type_(zi - big_divisor * decimal_significand);

                    do {
                        if (r > deltai) {
                            break;
                        }
                        else if (r == deltai) {
                            // Compare the fractional parts.
                            if (!multiplication_traits_::compute_mul_parity(
                                     carrier_uint(two_fc - (shorter_interval ? 1 : 2)), cache, beta)
                                     .parity) {
                                break;
                            }
                        }

                        // The floor is inside, so we are done.
                        return SignPolicy::handle_sign(
                            s, TrailingZeroPolicy::template on_trailing_zeros<format>(
                                   decimal_significand, decimal_exponent_type_(minus_k + kappa + 1)));
                    } while (false);


                    //////////////////////////////////////////////////////////////////////
                    // Step 3: Find the significand with the small divisor.
                    //////////////////////////////////////////////////////////////////////

                    decimal_significand *= 10;
                    decimal_significand += div::small_division_by_pow10<kappa>(r);
                    return SignPolicy::handle_sign(
                        s, TrailingZeroPolicy::template no_trailing_zeros<format>(
                               decimal_significand, decimal_exponent_type_(minus_k + kappa)));
                }

                static constexpr bool
                is_right_endpoint_integer_shorter_interval(exponent_int binary_exponent) noexcept {
                    return binary_exponent >= case_shorter_interval_right_endpoint_lower_threshold &&
                           binary_exponent <= case_shorter_interval_right_endpoint_upper_threshold;
                }

                static constexpr bool
                is_left_endpoint_integer_shorter_interval(exponent_int binary_exponent) noexcept {
                    return binary_exponent >= case_shorter_interval_left_endpoint_lower_threshold &&
                           binary_exponent <= case_shorter_interval_left_endpoint_upper_threshold;
                }
            };


            ////////////////////////////////////////////////////////////////////////////////////////
            // Policy holder.
            ////////////////////////////////////////////////////////////////////////////////////////

            // The library will specify a list of accepted kinds of policies and their defaults,
            // and the user will pass a list of policies parameters. The policy parameters are
            // supposed to be stateless and only convey information through their types.
            // The aim of the helper classes/functions given below is to do the following:
            //   1. Check if the policy parameters given by the user are all valid; that means,
            //      each of them should be at least of one of the kinds specified by the library.
            //      If that's not the case, then the compilation fails.
            //   2. Check if multiple policy parameters for the same kind is specified by the
            //      user. If that's the case, then the compilation fails.
            //   3. Build a class deriving from all policies the user have given, and also from
            //      the default policies if the user did not specify one for some kinds.
            // The library considers a certain policy parameter to belong to a specific kind if and only
            // if the parameter's type has a member type with a specific name; for example, it belongs
            // to "sign policy" kind if there is a member type sign_policy.

            // For a given kind, find a policy belonging to that kind.
            // Check if there are more than one such policies.
            enum class policy_found_info { not_found, unique, repeated };
            template <class Policy, policy_found_info info>
            struct found_policy_pair {
                // Either the policy parameter type given by the user, or the default policy.
                using policy = Policy;
                static constexpr auto found_info = info;
            };

            template <class KindDetector, class DefaultPolicy>
            struct detector_default_pair {
                using kind_detector = KindDetector;

                // Iterate through all given policy parameter types and see if there is a policy
                // parameter type belonging to the policy kind specified by KindDetector.
                // 1. If there is none, get_found_policy_pair returns
                //    found_policy_pair<DefaultPolicy, policy_found_info::not_found>.
                // 2. If there is only one parameter type belonging to the specified kind, then
                //    get_found_policy_pair returns
                //    found_policy_pair<Policy, policy_found_info::unique>
                //    where Policy is the unique parameter type belonging to the specified kind.
                // 3. If there are multiple parameter types belonging to the specified kind, then
                //    get_found_policy_pair returns
                //    found_policy_pair<FirstPolicy, policy_found_info::repeated>
                //    where FirstPolicy is the first parameter type belonging to the specified kind.
                //    The compilation must fail if this happens.
                // This is done by first setting FoundPolicyInfo below to
                // found_policy_pair<DefaultPolicy, policy_found_info::not_found>, and then iterate
                // over Policies, replacing FoundPolicyInfo by the appropriate one if a parameter
                // type belonging to the specified kind is found.

                template <class FoundPolicyInfo, class... Policies>
                struct get_found_policy_pair_impl;

                template <class FoundPolicyInfo>
                struct get_found_policy_pair_impl<FoundPolicyInfo> {
                    using type = FoundPolicyInfo;
                };

                template <class FoundPolicyInfo, class FirstPolicy, class... RemainingPolicies>
                struct get_found_policy_pair_impl<FoundPolicyInfo, FirstPolicy, RemainingPolicies...> {
                    using type = typename stdr::conditional<
                        KindDetector{}(dummy<FirstPolicy>{}),
                        typename stdr::conditional<
                            FoundPolicyInfo::found_info == policy_found_info::not_found,
                            typename get_found_policy_pair_impl<
                                found_policy_pair<FirstPolicy, policy_found_info::unique>,
                                RemainingPolicies...>::type,
                            typename get_found_policy_pair_impl<
                                found_policy_pair<FirstPolicy, policy_found_info::repeated>,
                                RemainingPolicies...>::type>::type,
                        typename get_found_policy_pair_impl<FoundPolicyInfo,
                                                            RemainingPolicies...>::type>::type;
                };

                template <class... Policies>
                using get_found_policy_pair = typename get_found_policy_pair_impl<
                    found_policy_pair<DefaultPolicy, policy_found_info::not_found>, Policies...>::type;
            };

            // Simple typelist of detector_default_pair's.
            template <class... DetectorDefaultPairs>
            struct detector_default_pair_list {};

            // Check if a given policy belongs to one of the kinds specified by the library.
            template <class Policy>
            constexpr bool check_policy_validity(dummy<Policy>, detector_default_pair_list<>) noexcept {
                return false;
            }
            template <class Policy, class FirstDetectorDefaultPair,
                      class... RemainingDetectorDefaultPairs>
            constexpr bool check_policy_validity(
                dummy<Policy>, detector_default_pair_list<FirstDetectorDefaultPair,
                                                          RemainingDetectorDefaultPairs...>) noexcept {
                return typename FirstDetectorDefaultPair::kind_detector{}(dummy<Policy>{}) ||
                       check_policy_validity(
                           dummy<Policy>{},
                           detector_default_pair_list<RemainingDetectorDefaultPairs...>{});
            }

            // Check if all of policies belong to some of the kinds specified by the library.
            template <class DetectorDefaultPairList>
            constexpr bool check_policy_list_validity(DetectorDefaultPairList) noexcept {
                return true;
            }
            template <class DetectorDefaultPairList, class FirstPolicy, class... RemainingPolicies>
            constexpr bool check_policy_list_validity(DetectorDefaultPairList,
                                                      dummy<FirstPolicy> first_policy,
                                                      dummy<RemainingPolicies>... remaining_policies) {
                return check_policy_validity(first_policy, DetectorDefaultPairList{}) &&
                       check_policy_list_validity(DetectorDefaultPairList{}, remaining_policies...);
            }

            // Actual policy holder class deriving from all specified policy types.
            template <class... Policies>
            struct policy_holder : Policies... {};

            // Iterate through the library-specified list of base-default pairs, i.e., the list of
            // policy kinds and their defaults. For each base-default pair, call
            // base_default_pair::get_found_policy_pair on the list of user-specified list of
            // policies to get found_policy_pair, and build the list of them.

            template <bool repeated_, class... FoundPolicyPairs>
            struct found_policy_pair_list {
                // This will be set to be true if and only if there exists at least one
                // found_policy_pair inside FoundPolicyPairs with
                // found_info == policy_found_info::repeated, in which case the compilation must
                // fail.
                static constexpr bool repeated = repeated_;
            };

            // Iterate through DetectorDefaultPairList and augment FoundPolicyPairList by one at each
            // iteration.
            template <class DetectorDefaultPairList, class FoundPolicyPairList, class... Policies>
            struct make_policy_pair_list_impl;

            // When there is no more detector-default pair to iterate, then the current
            // found_policy_pair_list is the final result.
            template <bool repeated, class... FoundPolicyPairs, class... Policies>
            struct make_policy_pair_list_impl<detector_default_pair_list<>,
                                              found_policy_pair_list<repeated, FoundPolicyPairs...>,
                                              Policies...> {
                using type = found_policy_pair_list<repeated, FoundPolicyPairs...>;
            };

            // For the first detector-default pair in the remaining list, call
            // detector_default_pair::get_found_policy_pair on Policies and add the returned
            // found_policy_pair into the current list of found_policy_pair's, and move to the next
            // detector-default pair.
            template <class FirstDetectorDefaultPair, class... RemainingDetectorDefaultPairs,
                      bool repeated, class... FoundPolicyPairs, class... Policies>
            struct make_policy_pair_list_impl<
                detector_default_pair_list<FirstDetectorDefaultPair, RemainingDetectorDefaultPairs...>,
                found_policy_pair_list<repeated, FoundPolicyPairs...>, Policies...> {
                using new_found_policy_pair =
                    typename FirstDetectorDefaultPair::template get_found_policy_pair<Policies...>;

                using type = typename make_policy_pair_list_impl<
                    detector_default_pair_list<RemainingDetectorDefaultPairs...>,
                    found_policy_pair_list<(repeated || new_found_policy_pair::found_info ==
                                                            policy_found_info::repeated),
                                           new_found_policy_pair, FoundPolicyPairs...>,
                    Policies...>::type;
            };

            template <class DetectorDefaultPairList, class... Policies>
            using policy_pair_list =
                typename make_policy_pair_list_impl<DetectorDefaultPairList,
                                                    found_policy_pair_list<false>, Policies...>::type;

            // Unpack FoundPolicyPairList into found_policy_pair's and build the policy_holder type
            // from the corresponding typelist of found_policy_pair::policy's.
            template <class FoundPolicyPairList, class... RawPolicies>
            struct convert_to_policy_holder_impl;

            template <bool repeated, class... RawPolicies>
            struct convert_to_policy_holder_impl<found_policy_pair_list<repeated>, RawPolicies...> {
                using type = policy_holder<RawPolicies...>;
            };

            template <bool repeated, class FirstFoundPolicyPair, class... RemainingFoundPolicyPairs,
                      class... RawPolicies>
            struct convert_to_policy_holder_impl<
                found_policy_pair_list<repeated, FirstFoundPolicyPair, RemainingFoundPolicyPairs...>,
                RawPolicies...> {
                using type = typename convert_to_policy_holder_impl<
                    found_policy_pair_list<repeated, RemainingFoundPolicyPairs...>,
                    typename FirstFoundPolicyPair::policy, RawPolicies...>::type;
            };

            template <class FoundPolicyPairList>
            using convert_to_policy_holder =
                typename convert_to_policy_holder_impl<FoundPolicyPairList>::type;

            template <class DetectorDefaultPairList, class... Policies>
            struct make_policy_holder_impl {
                static_assert(check_policy_list_validity(DetectorDefaultPairList{},
                                                         dummy<Policies>{}...),
                              "jkj::dragonbox: an invalid policy is specified");

                static_assert(
                    !policy_pair_list<DetectorDefaultPairList, Policies...>::repeated,
                    "jkj::dragonbox: at most one policy should be specified for each policy kind");

                using type =
                    convert_to_policy_holder<policy_pair_list<DetectorDefaultPairList, Policies...>>;
            };

            template <class DetectorDefaultPairList, class... Policies>
            using make_policy_holder =
                typename make_policy_holder_impl<DetectorDefaultPairList, Policies...>::type;

                // Policy kind detectors.
                struct is_sign_policy {
                constexpr bool operator()(...) noexcept { return false; }
                template <class Policy, class = typename Policy::sign_policy>
                constexpr bool operator()(dummy<Policy>) noexcept {
                    return true;
                }
            };
            struct is_trailing_zero_policy {
                constexpr bool operator()(...) noexcept { return false; }
                template <class Policy, class = typename Policy::trailing_zero_policy>
                constexpr bool operator()(dummy<Policy>) noexcept {
                    return true;
                }
            };
            struct is_decimal_to_binary_rounding_policy {
                constexpr bool operator()(...) noexcept { return false; }
                template <class Policy, class = typename Policy::decimal_to_binary_rounding_policy>
                constexpr bool operator()(dummy<Policy>) noexcept {
                    return true;
                }
            };
            struct is_binary_to_decimal_rounding_policy {
                constexpr bool operator()(...) noexcept { return false; }
                template <class Policy, class = typename Policy::binary_to_decimal_rounding_policy>
                constexpr bool operator()(dummy<Policy>) noexcept {
                    return true;
                }
            };
            struct is_cache_policy {
                constexpr bool operator()(...) noexcept { return false; }
                template <class Policy, class = typename Policy::cache_policy>
                constexpr bool operator()(dummy<Policy>) noexcept {
                    return true;
                }
            };
            struct is_preferred_integer_types_policy {
                constexpr bool operator()(...) noexcept { return false; }
                template <class Policy, class = typename Policy::preferred_integer_types_policy>
                constexpr bool operator()(dummy<Policy>) noexcept {
                    return true;
                }
            };

            template <class... Policies>
            using to_decimal_policy_holder = make_policy_holder<
                detector_default_pair_list<
                    detector_default_pair<is_sign_policy, policy::sign::return_sign_t>,
                    detector_default_pair<is_trailing_zero_policy, policy::trailing_zero::remove_t>,
                    detector_default_pair<is_decimal_to_binary_rounding_policy,
                                          policy::decimal_to_binary_rounding::nearest_to_even_t>,
                    detector_default_pair<is_binary_to_decimal_rounding_policy,
                                          policy::binary_to_decimal_rounding::to_even_t>,
                    detector_default_pair<is_cache_policy, policy::cache::full_t>,
                    detector_default_pair<is_preferred_integer_types_policy,
                                          policy::preferred_integer_types::match_t>>,
                Policies...>;

            template <class FormatTraits, class... Policies>
            using to_decimal_return_type = typename impl<FormatTraits>::template return_type<
                typename to_decimal_policy_holder<Policies...>::sign_policy,
                typename to_decimal_policy_holder<Policies...>::trailing_zero_policy,
                typename to_decimal_policy_holder<Policies...>::preferred_integer_types_policy>;

            template <class FormatTraits, class PolicyHolder>
            struct to_decimal_dispatcher {
                using sign_policy = typename PolicyHolder::sign_policy;
                using trailing_zero_policy = typename PolicyHolder::trailing_zero_policy;
                using binary_to_decimal_rounding_policy =
                    typename PolicyHolder::binary_to_decimal_rounding_policy;
                using cache_policy = typename PolicyHolder::cache_policy;
                using preferred_integer_types_policy =
                    typename PolicyHolder::preferred_integer_types_policy;
                using return_type =
                    typename impl<FormatTraits>::template return_type<sign_policy, trailing_zero_policy,
                                                                      preferred_integer_types_policy>;

                template <class IntervalTypeProvider>
                JKJ_FORCEINLINE JKJ_SAFEBUFFERS JKJ_CONSTEXPR20 return_type
                operator()(IntervalTypeProvider, signed_significand_bits<FormatTraits> s,
                           typename FormatTraits::exponent_int exponent_bits) noexcept {
                    constexpr auto tag = IntervalTypeProvider::tag;

                    JKJ_IF_CONSTEXPR(tag == policy::decimal_to_binary_rounding::tag_t::to_nearest) {
                        return impl<FormatTraits>::template compute_nearest<
                            sign_policy, trailing_zero_policy, IntervalTypeProvider,
                            binary_to_decimal_rounding_policy, cache_policy,
                            preferred_integer_types_policy>(s, exponent_bits);
                    }
                    else JKJ_IF_CONSTEXPR(
                        tag == policy::decimal_to_binary_rounding::tag_t::left_closed_directed) {
                        return impl<FormatTraits>::template compute_left_closed_directed<
                            sign_policy, trailing_zero_policy, cache_policy,
                            preferred_integer_types_policy>(s, exponent_bits);
                    }
                    else {
#if JKJ_HAS_IF_CONSTEXPR
                        static_assert(
                            tag == policy::decimal_to_binary_rounding::tag_t::right_closed_directed,
                            "");
#endif
                        return impl<FormatTraits>::template compute_right_closed_directed<
                            sign_policy, trailing_zero_policy, cache_policy,
                            preferred_integer_types_policy>(s, exponent_bits);
                    }
                }
            };
        }


        ////////////////////////////////////////////////////////////////////////////////////////
        // The interface function.
        ////////////////////////////////////////////////////////////////////////////////////////

        template <class FormatTraits, class ExponentBits, class... Policies>
        JKJ_FORCEINLINE
            JKJ_SAFEBUFFERS JKJ_CONSTEXPR20 detail::to_decimal_return_type<FormatTraits, Policies...>
            to_decimal_ex(signed_significand_bits<FormatTraits> s, ExponentBits exponent_bits,
                          Policies...) noexcept {
            // Build policy holder type.
            using policy_holder = detail::to_decimal_policy_holder<Policies...>;

            return policy_holder::delegate(
                s, detail::to_decimal_dispatcher<FormatTraits, policy_holder>{}, s, exponent_bits);
        }

        template <class Float,
                  class ConversionTraits = default_float_bit_carrier_conversion_traits<Float>,
                  class FormatTraits = ieee754_binary_traits<typename ConversionTraits::format,
                                                             typename ConversionTraits::carrier_uint>,
                  class... Policies>
        JKJ_FORCEINLINE
            JKJ_SAFEBUFFERS JKJ_CONSTEXPR20 detail::to_decimal_return_type<FormatTraits, Policies...>
            to_decimal(Float x, Policies... policies) noexcept {
            auto const br = make_float_bits<Float, ConversionTraits, FormatTraits>(x);
            auto const exponent_bits = br.extract_exponent_bits();
            auto const s = br.remove_exponent_bits();
            assert(br.is_finite() && br.is_nonzero());

            return to_decimal_ex(s, exponent_bits, policies...);
        }
    }
}

#undef JKJ_HAS_BUILTIN
#undef JKJ_FORCEINLINE
#undef JKJ_SAFEBUFFERS
#undef JKJ_CONSTEXPR20
#undef JKJ_USE_IS_CONSTANT_EVALUATED
#undef JKJ_CAN_BRANCH_ON_CONSTEVAL
#undef JKJ_IF_NOT_CONSTEVAL
#undef JKJ_IF_CONSTEVAL
#undef JKJ_HAS_BIT_CAST
#undef JKJ_IF_CONSTEXPR
#undef JKJ_HAS_IF_CONSTEXPR
#undef JKJ_INLINE_VARIABLE
#undef JKJ_HAS_INLINE_VARIABLE
#undef JKJ_HAS_CONSTEXPR17
#undef JKJ_CONSTEXPR14
#undef JKJ_HAS_CONSTEXPR14
#if JKJ_STD_REPLACEMENT_NAMESPACE_DEFINED
    #undef JKJ_STD_REPLACEMENT_NAMESPACE_DEFINED
#else
    #undef JKJ_STD_REPLACEMENT_NAMESPACE
#endif
#if JKJ_STATIC_DATA_SECTION_DEFINED
    #undef JKJ_STATIC_DATA_SECTION_DEFINED
#else
    #undef JKJ_STATIC_DATA_SECTION
#endif

#endif
