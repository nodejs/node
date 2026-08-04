// Copyright 2020-2025 Junekey Jeon
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

#define JKJ_DRAGONBOX_LEAK_MACROS
#include "dragonbox.h"

namespace JKJ_NAMESPACE {
    namespace dragonbox {
        namespace detail {
            template <class FloatFormat, class CarrierUInt>
            extern char* to_chars(CarrierUInt significand, int exponent, char* buffer) noexcept;

            // Implementation detail of to_chars_naive, which gets called in constexpr context or when
            // the digit_generation policy is set to be "compact". Unlike to_chars above, to_chars_naive
            // simply repeats division-by-10 until the input becomes zero. To improve codegen, we
            // manually convert this division-by-10 into multiply-shift. Similarly to the log
            // computations, again we try to minimize the size of types and the magic numbers depending
            // on the range of input, using the same "tier system" trick. But unlike the case of log
            // computations, we simply fallback to the plain division/remainder if none of the provided
            // tiers cover the requested range of input, rather than to generate a compile-error.
            namespace to_chars_impl {
                template <class UInt>
                struct div_result {
                    UInt quot;
                    UInt rem;
                };

                enum class tier_selector { current_tier, next_tier, fallback };

                template <template <stdr::size_t> class Info, stdr::uint_least64_t max_number,
                          stdr::size_t current_tier,
                          stdr::uint_least64_t supported_max_number = Info<current_tier>::max_number>
                constexpr tier_selector is_in_range(int) noexcept {
                    return max_number <= supported_max_number ? tier_selector::current_tier
                                                              : tier_selector::next_tier;
                }
                template <template <stdr::size_t> class Info, stdr::uint_least64_t max_number,
                          stdr::size_t current_tier>
                constexpr tier_selector is_in_range(...) noexcept {
                    return tier_selector::fallback;
                }

                template <template <stdr::size_t> class Info, stdr::uint_least64_t max_number,
                          stdr::size_t current_tier = 0,
                          tier_selector = is_in_range<Info, max_number, current_tier>(0)>
                struct div_by_10_impl;

                template <template <stdr::size_t> class Info, stdr::uint_least64_t max_number,
                          stdr::size_t current_tier>
                struct div_by_10_impl<Info, max_number, current_tier, tier_selector::current_tier> {
                    using info = Info<current_tier>;
                    template <class UInt>
                    static constexpr div_result<UInt> compute(UInt n) noexcept {
#if JKJ_HAS_CONSTEXPR14
                        assert(n <= max_number);
#endif
                        // This "wide_type" is supposed to be 2x larger than UInt.
                        using wide_type = typename info::wide_type;
                        static_assert(info::additional_shift_amount < value_bits<wide_type>::value, "");

                        // The upper-half and the lower-half of this multiplication result is used for
                        // computing the quotient and the remainder, respectively.
                        auto const mul_result = info::magic_number * n;
                        auto const high = info::high(mul_result) >> info::additional_shift_amount;
                        auto const low =
                            info::additional_shift_amount == 0
                                ? info::low(mul_result)
                                : ((info::high(mul_result)
                                    << (value_bits<wide_type>::value - info::additional_shift_amount)) |
                                   (info::low(mul_result) >> info::additional_shift_amount));

                        return div_result<UInt>{
                            static_cast<UInt>(high),                           // quotient
                            static_cast<UInt>(info::high(low * wide_type(10))) // remainder
                        };
                    }
                };

                template <template <stdr::size_t> class Info, stdr::uint_least64_t max_number,
                          stdr::size_t current_tier>
                struct div_by_10_impl<Info, max_number, current_tier, tier_selector::next_tier> {
                    using next_tier = div_by_10_impl<Info, max_number, current_tier + 1>;
                    template <class UInt>
                    static constexpr div_result<UInt> compute(UInt n) noexcept {
                        return next_tier::compute(n);
                    }
                };

                template <template <stdr::size_t> class Info, stdr::uint_least64_t max_number,
                          stdr::size_t current_tier>
                struct div_by_10_impl<Info, max_number, current_tier, tier_selector::fallback> {
                    using next_tier = div_by_10_impl<Info, max_number, current_tier + 1>;
                    template <class UInt>
                    static constexpr div_result<UInt> compute(UInt n) noexcept {
                        return div_result<UInt>{n / 10, n % 10};
                    }
                };

                template <stdr::size_t tier>
                struct div_by_10_info;
                template <>
                struct div_by_10_info<0> {
                    using wide_type = stdr::uint_fast16_t;
                    static constexpr wide_type magic_number = 26;
                    static constexpr stdr::size_t additional_shift_amount = 0;
                    static constexpr stdr::uint_least64_t max_number = 63;
                    static constexpr stdr::uint_fast8_t high(wide_type n) noexcept {
                        return stdr::uint_fast8_t(n >> 8);
                    }
                    static constexpr stdr::uint_fast8_t low(wide_type n) noexcept {
                        return stdr::uint_fast8_t(n);
                    }
                };
                template <>
                struct div_by_10_info<1> {
                    using wide_type = stdr::uint_fast32_t;
                    static constexpr wide_type magic_number = 6573;
                    static constexpr stdr::size_t additional_shift_amount = 0;
                    static constexpr stdr::uint_least64_t max_number = 16383;
                    static constexpr stdr::uint_fast16_t high(wide_type n) noexcept {
                        return stdr::uint_fast16_t(n >> 16);
                    }
                    static constexpr stdr::uint_fast16_t low(wide_type n) noexcept {
                        return stdr::uint_fast16_t(n);
                    }
                };
                template <>
                struct div_by_10_info<2> {
                    using wide_type = stdr::uint_fast64_t;
                    static constexpr wide_type magic_number = UINT64_C(429496730);
                    static constexpr stdr::size_t additional_shift_amount = 0;
                    static constexpr stdr::uint_least64_t max_number = UINT64_C(1073741823);
                    static constexpr stdr::uint_fast32_t high(wide_type n) noexcept {
                        return stdr::uint_fast32_t(n >> 32);
                    }
                    static constexpr stdr::uint_fast32_t low(wide_type n) noexcept {
                        return stdr::uint_fast32_t(n);
                    }
                };
                template <stdr::uint_least64_t max_number, class UInt>
                constexpr div_result<UInt> div_by_10(UInt n) noexcept {
                    return div_by_10_impl<div_by_10_info, max_number>::compute(n);
                }

                template <class UInt, UInt threshold, stdr::uint_least64_t max_number>
                JKJ_CONSTEXPR14 char* print_integer_backward(UInt& n, char* ptr) noexcept {
                    while (true) {
                        auto div_res = div_by_10<max_number>(n);
                        *ptr = char('0' + div_res.rem);
                        n = div_res.quot;
                        if (n < threshold) {
                            break;
                        }
                        --ptr;
                    }
                    return ptr;
                }
            }

            template <class FloatFormat, class CarrierUInt, class ExponentType>
            JKJ_CONSTEXPR20 char* to_chars_naive(CarrierUInt significand, ExponentType exponent,
                                                 char* buffer) noexcept {
                static_assert(value_bits<CarrierUInt>::value <= 64,
                              "jkj::dragonbox: integer type too large");
                // This is probably a too generous upper bound, but unless a tighter upper bound does
                // lead to better codegen, there is no point of optimizing this further.
                constexpr auto max_decimal_significand =
                    20 * ((stdr::uint_least64_t(1) << (FloatFormat::significand_bits + 1)) - 1) - 1;

                // Print significand.
                // For the single-digit case, just print that number directly into the buffer.
                if (significand < 10) {
                    *buffer = char('0' + significand);
                    ++buffer;
                }
                // For the multiple-digits case, print the digits into a temporary buffer and copy it
                // into the given buffer. GCC seems to be able to turn the last loop into memcpy.
                else {
                    char temp[FloatFormat::decimal_significand_digits];
                    auto ptr =
                        to_chars_impl::print_integer_backward<CarrierUInt, 10, max_decimal_significand>(
                            significand, temp + FloatFormat::decimal_significand_digits - 1);

                    buffer[0] = char('0' + significand);
                    buffer[1] = '.';
                    buffer += 2;
                    exponent +=
                        static_cast<ExponentType>(temp + FloatFormat::decimal_significand_digits - ptr);

                    do {
                        *buffer = *ptr;
                        ++buffer;
                        ++ptr;
                    } while (ptr != temp + FloatFormat::decimal_significand_digits);
                }

                // Print exponent.
                *buffer = 'E';
                ++buffer;
                if (exponent < 0) {
                    *buffer = '-';
                    ++buffer;
                    exponent = -exponent;
                }
                auto exponent_unsigned =
                    static_cast<typename stdr::make_unsigned<ExponentType>::type>(exponent);

                char temp[FloatFormat::decimal_exponent_digits];
                auto ptr = to_chars_impl::print_integer_backward<decltype(exponent_unsigned), 1,
                                                                 FloatFormat::max_abs_decimal_exponent>(
                    exponent_unsigned, temp + FloatFormat::decimal_exponent_digits - 1);

                do {
                    *buffer = *ptr;
                    ++buffer;
                    ++ptr;
                } while (ptr != temp + FloatFormat::decimal_exponent_digits);

                return buffer;
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

        // Maximum size of the output of to_chars_n (excluding null-terminator).
        template <class FloatFormat>
        struct max_output_string_length_holder {
            static constexpr detail::stdr::size_t value =
                // sign(1) + significand + decimal_point(1) + exp_marker(1) + exp_sign(1) + exp
                1 + FloatFormat::decimal_significand_digits + 1 + 1 + 1 +
                FloatFormat::decimal_exponent_digits;
        };
#if JKJ_HAS_VARIABLE_TEMPLATES
        template <class FloatFormat>
        JKJ_INLINE_VARIABLE detail::stdr::size_t max_output_string_length =
            max_output_string_length_holder<FloatFormat>::value;
#endif
    }
}

#ifndef JKJ_DRAGONBOX_TO_CHARS_LEAK_MACROS
    // This will clean up all leaked macros.
    #undef JKJ_DRAGONBOX_LEAK_MACROS
    #include "dragonbox.h"
#endif

#endif
