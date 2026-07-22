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

#ifndef JKJ_HEADER_DRAGONBOX_TO_CHARS
#define JKJ_HEADER_DRAGONBOX_TO_CHARS

#include "dragonbox.h"

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

namespace jkj {
    namespace dragonbox {
        namespace detail {
            template <class FloatFormat, class CarrierUInt>
            extern char* to_chars(CarrierUInt significand, int exponent, char* buffer) noexcept;

            template <stdr::size_t max_digits, class UInt>
            JKJ_CONSTEXPR14 char* print_integer_naive(UInt n, char* buffer) noexcept {
                char temp[max_digits]{};
                auto ptr = temp + sizeof(temp) - 1;
                do {
                    *ptr = char('0' + n % 10);
                    n /= 10;
                    --ptr;
                } while (n != 0);
                while (++ptr != temp + sizeof(temp)) {
                    *buffer = *ptr;
                    ++buffer;
                }
                return buffer;
            }

            template <class FloatFormat, class CarrierUInt>
            JKJ_CONSTEXPR14 char* to_chars_naive(CarrierUInt significand, int exponent,
                                                 char* buffer) noexcept {
                // Print significand.
                {
                    auto ptr = print_integer_naive<FloatFormat::decimal_significand_digits>(significand,
                                                                                            buffer);

                    // Insert decimal dot.
                    if (ptr > buffer + 1) {
                        auto next = *++buffer;
                        ++exponent;
                        *buffer = '.';
                        while (++buffer != ptr) {
                            auto const temp = *buffer;
                            *buffer = next;
                            next = temp;
                            ++exponent;
                        }
                        *buffer = next;
                    }
                    ++buffer;
                }

                // Print exponent.
                *buffer = 'E';
                ++buffer;
                if (exponent < 0) {
                    *buffer = '-';
                    ++buffer;
                    exponent = -exponent;
                }
                return print_integer_naive<FloatFormat::decimal_exponent_digits>(unsigned(exponent),
                                                                                 buffer);
            }
        }

        namespace policy {
            namespace digit_generation {
                JKJ_INLINE_VARIABLE struct fast_t {
                    using digit_generation_policy = fast_t;

                    template <class DecimalToBinaryRoundingPolicy, class BinaryToDecimalRoundingPolicy,
                              class CachePolicy, class PreferredIntegerTypesPolicy, class FormatTraits>
                    static char* to_chars(signed_significand_bits<FormatTraits> s,
                                          typename FormatTraits::exponent_int exponent_bits,
                                          char* buffer) noexcept {
                        auto result = to_decimal_ex(
                            s, exponent_bits, policy::sign::ignore, policy::trailing_zero::ignore,
                            DecimalToBinaryRoundingPolicy{}, BinaryToDecimalRoundingPolicy{},
                            CachePolicy{}, PreferredIntegerTypesPolicy{});

                        return detail::to_chars<typename FormatTraits::format>(result.significand,
                                                                               result.exponent, buffer);
                    }
                } fast = {};

                JKJ_INLINE_VARIABLE struct compact_t {
                    using digit_generation_policy = compact_t;

                    template <class DecimalToBinaryRoundingPolicy, class BinaryToDecimalRoundingPolicy,
                              class CachePolicy, class PreferredIntegerTypesPolicy, class FormatTraits>
                    static JKJ_CONSTEXPR20 char*
                    to_chars(signed_significand_bits<FormatTraits> s,
                             typename FormatTraits::exponent_int exponent_bits, char* buffer) noexcept {
                        auto result = to_decimal_ex(s, exponent_bits, policy::sign::ignore,
                                                    policy::trailing_zero::remove_compact,
                                                    DecimalToBinaryRoundingPolicy{},
                                                    BinaryToDecimalRoundingPolicy{}, CachePolicy{},
                                                    PreferredIntegerTypesPolicy{});

                        return detail::to_chars_naive<typename FormatTraits::format>(
                            result.significand, result.exponent, buffer);
                    }
                } compact = {};
            }
        }

        namespace detail {
            struct is_digit_generation_policy {
                constexpr bool operator()(...) noexcept { return false; }
                template <class Policy, class = typename Policy::digit_generation_policy>
                constexpr bool operator()(dummy<Policy>) noexcept {
                    return true;
                }
            };

            // Avoid needless ABI overhead incurred by tag dispatch.
            template <class DecimalToBinaryRoundingPolicy, class BinaryToDecimalRoundingPolicy,
                      class CachePolicy, class PreferredIntegerTypesPolicy, class DigitGenerationPolicy,
                      class FormatTraits>
            JKJ_CONSTEXPR20 char* to_chars_n_impl(float_bits<FormatTraits> br, char* buffer) noexcept {
                auto const exponent_bits = br.extract_exponent_bits();
                auto const s = br.remove_exponent_bits();

                if (br.is_finite(exponent_bits)) {
                    if (s.is_negative()) {
                        *buffer = '-';
                        ++buffer;
                    }
                    if (br.is_nonzero()) {
                        JKJ_IF_CONSTEVAL {
                            return policy::digit_generation::compact_t::to_chars<
                                DecimalToBinaryRoundingPolicy, BinaryToDecimalRoundingPolicy,
                                CachePolicy, PreferredIntegerTypesPolicy>(s, exponent_bits, buffer);
                        }

                        return DigitGenerationPolicy::template to_chars<
                            DecimalToBinaryRoundingPolicy, BinaryToDecimalRoundingPolicy, CachePolicy,
                            PreferredIntegerTypesPolicy>(s, exponent_bits, buffer);
                    }
                    else {
                        buffer[0] = '0';
                        buffer[1] = 'E';
                        buffer[2] = '0';
                        return buffer + 3;
                    }
                }
                else {
                    if (s.has_all_zero_significand_bits()) {
                        if (s.is_negative()) {
                            *buffer = '-';
                            ++buffer;
                        }
                        // MSVC generates two mov's for the below, so we guard it inside
                        // JKJ_IF_CONSTEVAL.
                        JKJ_IF_CONSTEVAL {
                            buffer[0] = 'I';
                            buffer[1] = 'n';
                            buffer[2] = 'f';
                            buffer[3] = 'i';
                            buffer[4] = 'n';
                            buffer[5] = 'i';
                            buffer[6] = 't';
                            buffer[7] = 'y';
                        }
                        else {
                            stdr::memcpy(buffer, "Infinity", 8);
                        }
                        return buffer + 8;
                    }
                    else {
                        buffer[0] = 'N';
                        buffer[1] = 'a';
                        buffer[2] = 'N';
                        return buffer + 3;
                    }
                }
            }
        }

        // Returns the next-to-end position
        template <class Float,
                  class ConversionTraits = default_float_bit_carrier_conversion_traits<Float>,
                  class FormatTraits = ieee754_binary_traits<typename ConversionTraits::format,
                                                             typename ConversionTraits::carrier_uint>,
                  class... Policies>
        JKJ_CONSTEXPR20 char* to_chars_n(Float x, char* buffer, Policies...) noexcept {
            using policy_holder = detail::make_policy_holder<
                detail::detector_default_pair_list<
                    detail::detector_default_pair<
                        detail::is_decimal_to_binary_rounding_policy,
                        policy::decimal_to_binary_rounding::nearest_to_even_t>,
                    detail::detector_default_pair<detail::is_binary_to_decimal_rounding_policy,
                                                  policy::binary_to_decimal_rounding::to_even_t>,
                    detail::detector_default_pair<detail::is_cache_policy, policy::cache::full_t>,
                    detail::detector_default_pair<detail::is_preferred_integer_types_policy,
                                                  policy::preferred_integer_types::match_t>,
                    detail::detector_default_pair<detail::is_digit_generation_policy,
                                                  policy::digit_generation::fast_t>>,
                Policies...>;

            return detail::to_chars_n_impl<typename policy_holder::decimal_to_binary_rounding_policy,
                                           typename policy_holder::binary_to_decimal_rounding_policy,
                                           typename policy_holder::cache_policy,
                                           typename policy_holder::preferred_integer_types_policy,
                                           typename policy_holder::digit_generation_policy>(
                make_float_bits<Float, ConversionTraits, FormatTraits>(x), buffer);
        }

        // Null-terminate and bypass the return value of to_chars_n
        template <class Float,
                  class ConversionTraits = default_float_bit_carrier_conversion_traits<Float>,
                  class FormatTraits = ieee754_binary_traits<typename ConversionTraits::format,
                                                             typename ConversionTraits::carrier_uint>,
                  class... Policies>
        JKJ_CONSTEXPR20 char* to_chars(Float x, char* buffer, Policies... policies) noexcept {
            auto ptr = to_chars_n<Float, ConversionTraits, FormatTraits>(x, buffer, policies...);
            *ptr = '\0';
            return ptr;
        }

        // Maximum required buffer size (excluding null-terminator)
        template <class FloatFormat>
        JKJ_INLINE_VARIABLE detail::stdr::size_t max_output_string_length =
            // sign(1) + significand + decimal_point(1) + exp_marker(1) + exp_sign(1) + exp
            1 + FloatFormat::decimal_significand_digits + 1 + 1 + 1 +
            FloatFormat::decimal_exponent_digits;
    }
}

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

#endif
