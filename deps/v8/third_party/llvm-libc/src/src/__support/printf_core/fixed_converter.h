//===-- Fixed Point Converter for printf ------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC___SUPPORT_PRINTF_CORE_FIXED_CONVERTER_H
#define LLVM_LIBC_SRC___SUPPORT_PRINTF_CORE_FIXED_CONVERTER_H

#include "include/llvm-libc-macros/stdfix-macros.h"
#include "src/__support/CPP/string_view.h"
#include "src/__support/ctype_utils.h"
#include "src/__support/fixed_point/fx_bits.h"
#include "src/__support/fixed_point/fx_rep.h"
#include "src/__support/integer_to_string.h"
#include "src/__support/libc_assert.h"
#include "src/__support/macros/config.h"
#include "src/__support/printf_core/converter_utils.h"
#include "src/__support/printf_core/core_structs.h"
#include "src/__support/printf_core/writer.h"

#include <inttypes.h>
#include <stddef.h>

namespace LIBC_NAMESPACE_DECL {
namespace printf_core {

// This is just for assertions. It will be compiled out for release builds.
LIBC_INLINE constexpr uint32_t const_ten_exp(uint32_t exponent) {
  uint32_t result = 1;
  LIBC_ASSERT(exponent < 11);
  for (uint32_t i = 0; i < exponent; ++i)
    result *= 10;

  return result;
}

#define READ_FX_BITS(TYPE)                                                     \
  do {                                                                         \
    auto fixed_bits = fixed_point::FXBits<TYPE>(                               \
        fixed_point::FXRep<TYPE>::StorageType(to_conv.conv_val_raw));          \
    integral = fixed_bits.get_integral();                                      \
    fractional = fixed_bits.get_fraction();                                    \
    exponent = fixed_bits.get_exponent();                                      \
    is_negative = fixed_bits.get_sign();                                       \
  } while (false)

#define APPLY_FX_LENGTH_MODIFIER(LENGTH_MODIFIER)                              \
  do {                                                                         \
    if (to_conv.conv_name == 'r') {                                            \
      READ_FX_BITS(LENGTH_MODIFIER fract);                                     \
    } else if (to_conv.conv_name == 'R') {                                     \
      READ_FX_BITS(unsigned LENGTH_MODIFIER fract);                            \
    } else if (to_conv.conv_name == 'k') {                                     \
      READ_FX_BITS(LENGTH_MODIFIER accum);                                     \
    } else if (to_conv.conv_name == 'K') {                                     \
      READ_FX_BITS(unsigned LENGTH_MODIFIER accum);                            \
    } else {                                                                   \
      LIBC_ASSERT(false && "Invalid conversion name passed to convert_fixed"); \
      return FIXED_POINT_CONVERSION_ERROR;                                     \
    }                                                                          \
  } while (false)

template <WriteMode write_mode>
LIBC_INLINE int convert_fixed(Writer<write_mode> *writer,
                              const FormatSection &to_conv) {
  // Long accum should be the largest type, so we can store all the smaller
  // numbers in things sized for it.
  using LARep = fixed_point::FXRep<unsigned long accum>;
  using StorageType = LARep::StorageType;

  FormatFlags flags = to_conv.flags;

  bool is_negative;
  int exponent;
  StorageType integral;
  StorageType fractional;

  // r = fract
  // k = accum
  // lowercase = signed
  // uppercase = unsigned
  // h = short
  // l = long
  // any other length modifier has no effect

  if (to_conv.length_modifier == LengthModifier::h) {
    APPLY_FX_LENGTH_MODIFIER(short);
  } else if (to_conv.length_modifier == LengthModifier::l) {
    APPLY_FX_LENGTH_MODIFIER(long);
  } else {
    APPLY_FX_LENGTH_MODIFIER();
  }

  LIBC_ASSERT(static_cast<size_t>(exponent) <=
                  (sizeof(StorageType) - sizeof(uint32_t)) * CHAR_BIT &&
              "StorageType must be large enough to hold the fractional "
              "component multiplied by a 32 bit number.");

  // If to_conv doesn't specify a precision, the precision defaults to 6.
  const size_t precision = to_conv.precision < 0 ? 6 : to_conv.precision;
  bool has_decimal_point =
      (precision > 0) || ((flags & FormatFlags::ALTERNATE_FORM) != 0);

  // The number of non-zero digits below the decimal point for a negative power
  // of 2 in base 10 is equal to the magnitude of the power of 2.

  // A quick proof:
  // Let p be any positive integer.
  // Let e = 2^(-p)
  // Let t be a positive integer such that e * 10^t is an integer.
  // By definition: The smallest allowed value of t must be equal to the number
  // of non-zero digits below the decimal point in e.
  // If we evaluate e * 10^t we get the following:
  // e * 10^t = 2^(-p) * 10*t = 2^(-p) * 2^t * 5^t = 5^t * 2^(t-p)
  // For 5^t * 2^(t-p) to be an integer, both exponents must be non-negative,
  // since 5 and 2 are coprime.
  // The smallest value of t such that t-p is non-negative is p.
  // Therefor, the number of non-zero digits below the decimal point for a given
  // negative power of 2 "p" is equal to the value of p.

  constexpr size_t MAX_FRACTION_DIGITS = LARep::FRACTION_LEN;

  char fraction_digits[MAX_FRACTION_DIGITS];

  size_t valid_fraction_digits = 0;

  // TODO: Factor this part out
  while (fractional > 0) {
    uint32_t cur_digits = 0;
    // 10^9 is used since it's the largest power of 10 that fits in a uint32_t
    constexpr uint32_t TEN_EXP_NINE = 1000000000;
    constexpr size_t DIGITS_PER_BLOCK = 9;

    // Multiply by 10^9, then grab the digits above the decimal point, then
    // clear those digits in fractional.
    fractional = fractional * TEN_EXP_NINE;
    cur_digits = static_cast<uint32_t>(fractional >> exponent);
    fractional = fractional % (StorageType(1) << exponent);

    // we add TEN_EXP_NINE to force leading zeroes to show up, then we skip the
    // first digit in the loop.
    const IntegerToString<uint32_t> cur_fractional_digits(cur_digits +
                                                          TEN_EXP_NINE);
    for (size_t i = 0;
         i < DIGITS_PER_BLOCK && valid_fraction_digits < MAX_FRACTION_DIGITS;
         ++i, ++valid_fraction_digits)
      fraction_digits[valid_fraction_digits] =
          cur_fractional_digits.view()[i + 1];

    if (valid_fraction_digits >= MAX_FRACTION_DIGITS) {
      LIBC_ASSERT(fractional == 0 && "If the fraction digit buffer is full, "
                                     "there should be no remaining digits.");
      /*
        A visual explanation of what this assert is checking:

         32 digits (max for 32 bit fract)
         +------------------------------++--+--- must be zero
         |                              ||  |
         123456789012345678901234567890120000
         |       ||       ||       ||       |
         +-------++-------++-------++-------+
         9 digit blocks
      */
      LIBC_ASSERT(cur_digits % const_ten_exp(
                                   DIGITS_PER_BLOCK -
                                   (MAX_FRACTION_DIGITS % DIGITS_PER_BLOCK)) ==
                      0 &&
                  "Digits after the MAX_FRACTION_DIGITS should all be zero.");
      valid_fraction_digits = MAX_FRACTION_DIGITS;
    }
  }

  if (precision < valid_fraction_digits) {
    // Handle rounding. Just do round to nearest, tie to even since it's
    // unspecified.
    RoundDirection round;
    char first_digit_after = fraction_digits[precision];
    if (internal::b36_char_to_int(first_digit_after) > 5) {
      round = RoundDirection::Up;
    } else if (internal::b36_char_to_int(first_digit_after) < 5) {
      round = RoundDirection::Down;
    } else {
      // first_digit_after == '5'
      // need to check the remaining digits, but default to even.
      round = RoundDirection::Even;
      for (size_t cur_digit_index = precision + 1;
           cur_digit_index + 1 < valid_fraction_digits; ++cur_digit_index) {
        if (fraction_digits[cur_digit_index] != '0') {
          round = RoundDirection::Up;
          break;
        }
      }
    }

    // If we need to actually perform rounding, do so.
    if (round == RoundDirection::Up || round == RoundDirection::Even) {
      bool keep_rounding = true;
      int digit_to_round = static_cast<int>(precision) - 1;
      for (; digit_to_round >= 0 && keep_rounding; --digit_to_round) {
        keep_rounding = false;
        char cur_digit = fraction_digits[digit_to_round];
        // if the digit should not be rounded up
        if (round == RoundDirection::Even &&
            (internal::b36_char_to_int(cur_digit) % 2) == 0) {
          // break out of the loop
          break;
        }
        fraction_digits[digit_to_round] += 1;

        // if the digit was a 9, instead replace with a 0.
        if (cur_digit == '9') {
          fraction_digits[digit_to_round] = '0';
          keep_rounding = true;
        }
      }

      // if every digit below the decimal point was rounded up but we need to
      // keep rounding
      if (keep_rounding &&
          (round == RoundDirection::Up ||
           (round == RoundDirection::Even && ((integral % 2) == 1)))) {
        // add one to the integral portion to round it up.
        ++integral;
      }
    }

    valid_fraction_digits = precision;
  }

  const IntegerToString<StorageType> integral_str(integral);

  // these are signed to prevent underflow due to negative values. The
  // eventual values will always be non-negative.
  size_t trailing_zeroes = 0;
  int padding;

  // If the precision is greater than the actual result, pad with 0s
  if (precision > valid_fraction_digits)
    trailing_zeroes = precision - (valid_fraction_digits);

  constexpr cpp::string_view DECIMAL_POINT(".");

  char sign_char = 0;

  // Check if the conv name is uppercase
  if (internal::isupper(to_conv.conv_name)) {
    // These flags are only for signed conversions, so this removes them if the
    // conversion is unsigned.
    flags = FormatFlags(flags &
                        ~(FormatFlags::FORCE_SIGN | FormatFlags::SPACE_PREFIX));
  }

  if (is_negative)
    sign_char = '-';
  else if ((flags & FormatFlags::FORCE_SIGN) == FormatFlags::FORCE_SIGN)
    sign_char = '+'; // FORCE_SIGN has precedence over SPACE_PREFIX
  else if ((flags & FormatFlags::SPACE_PREFIX) == FormatFlags::SPACE_PREFIX)
    sign_char = ' ';

  padding = static_cast<int>(to_conv.min_width - (sign_char > 0 ? 1 : 0) -
                             integral_str.size() -
                             static_cast<int>(has_decimal_point) -
                             valid_fraction_digits - trailing_zeroes);
  if (padding < 0)
    padding = 0;

  if ((flags & FormatFlags::LEFT_JUSTIFIED) == FormatFlags::LEFT_JUSTIFIED) {
    // The pattern is (sign), integral, (.), (fraction), (zeroes), (spaces)
    if (sign_char > 0)
      RET_IF_RESULT_NEGATIVE(writer->write(sign_char));
    RET_IF_RESULT_NEGATIVE(writer->write(integral_str.view()));
    if (has_decimal_point)
      RET_IF_RESULT_NEGATIVE(writer->write(DECIMAL_POINT));
    if (valid_fraction_digits > 0)
      RET_IF_RESULT_NEGATIVE(
          writer->write({fraction_digits, valid_fraction_digits}));
    if (trailing_zeroes > 0)
      RET_IF_RESULT_NEGATIVE(writer->write('0', trailing_zeroes));
    if (padding > 0)
      RET_IF_RESULT_NEGATIVE(writer->write(' ', padding));
  } else {
    // The pattern is (spaces), (sign), (zeroes), integral, (.), (fraction),
    // (zeroes)
    if ((padding > 0) &&
        ((flags & FormatFlags::LEADING_ZEROES) != FormatFlags::LEADING_ZEROES))
      RET_IF_RESULT_NEGATIVE(writer->write(' ', padding));
    if (sign_char > 0)
      RET_IF_RESULT_NEGATIVE(writer->write(sign_char));
    if ((padding > 0) &&
        ((flags & FormatFlags::LEADING_ZEROES) == FormatFlags::LEADING_ZEROES))
      RET_IF_RESULT_NEGATIVE(writer->write('0', padding));
    RET_IF_RESULT_NEGATIVE(writer->write(integral_str.view()));
    if (has_decimal_point)
      RET_IF_RESULT_NEGATIVE(writer->write(DECIMAL_POINT));
    if (valid_fraction_digits > 0)
      RET_IF_RESULT_NEGATIVE(
          writer->write({fraction_digits, valid_fraction_digits}));
    if (trailing_zeroes > 0)
      RET_IF_RESULT_NEGATIVE(writer->write('0', trailing_zeroes));
  }
  return WRITE_OK;
}

} // namespace printf_core
} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC___SUPPORT_PRINTF_CORE_FIXED_CONVERTER_H
