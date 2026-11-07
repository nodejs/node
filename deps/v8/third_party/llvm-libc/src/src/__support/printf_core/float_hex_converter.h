//===-- Hexadecimal Converter for printf ------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC___SUPPORT_PRINTF_CORE_FLOAT_HEX_CONVERTER_H
#define LLVM_LIBC_SRC___SUPPORT_PRINTF_CORE_FLOAT_HEX_CONVERTER_H

#include "src/__support/CPP/string_view.h"
#include "src/__support/FPUtil/FPBits.h"
#include "src/__support/FPUtil/rounding_mode.h"
#include "src/__support/ctype_utils.h"
#include "src/__support/macros/config.h"
#include "src/__support/printf_core/converter_utils.h"
#include "src/__support/printf_core/core_structs.h"
#include "src/__support/printf_core/float_inf_nan_converter.h"
#include "src/__support/printf_core/writer.h"

#include <inttypes.h>
#include <stddef.h>

namespace LIBC_NAMESPACE_DECL {
namespace printf_core {

struct FloatHexExpFPBitsProperties {
  AnyFloatStorageType mantissa;
  int exponent;
  uint32_t fraction_bits;
  bool is_negative;
  bool is_inf_or_nan;
  bool mantissa_is_zero;
};

template <typename T>
LIBC_INLINE FloatHexExpFPBitsProperties
get_float_hex_exp_fp_bits_properties_typed(fputil::FPBits<T> float_bits) {
  return {
      .mantissa = float_bits.get_explicit_mantissa(),
      .exponent = float_bits.get_explicit_exponent(),
      .fraction_bits = fputil::FPBits<T>::FRACTION_LEN,
      .is_negative = float_bits.is_neg(),
      .is_inf_or_nan = float_bits.is_inf_or_nan(),
      .mantissa_is_zero = float_bits.get_mantissa() == 0,
  };
}

// Returns relevant floating point properties for conversion using
// `to_conv.length_modifier` to infer the conversion value type.
LIBC_INLINE FloatHexExpFPBitsProperties
get_float_hex_exp_fp_bits_properties_lm(const FormatSection &to_conv) {
#if defined(LIBC_INTERNAL_PRINTF_CONVERT_FLOAT128)
  if (to_conv.length_modifier == LengthModifier::Q)
    return get_float_hex_exp_fp_bits_properties_typed<float128>(
        fputil::FPBits<float128>(to_conv.conv_val_raw));
#endif // LIBC_INTERNAL_PRINTF_CONVERT_FLOAT128
#ifndef LIBC_TYPES_LONG_DOUBLE_IS_DOUBLE_DOUBLE
  if (to_conv.length_modifier == LengthModifier::L)
    return get_float_hex_exp_fp_bits_properties_typed<long double>(
        fputil::FPBits<long double>(
            static_cast<fputil::FPBits<long double>::StorageType>(
                to_conv.conv_val_raw)));
#endif // !LIBC_TYPES_LONG_DOUBLE_IS_DOUBLE_DOUBLE
  return get_float_hex_exp_fp_bits_properties_typed<double>(
      fputil::FPBits<double>(static_cast<fputil::FPBits<double>::StorageType>(
          to_conv.conv_val_raw)));
}

template <OverflowMode mode>
LIBC_INLINE int
convert_finite_float_hex_exp(Writer<mode> *writer,
                             FloatHexExpFPBitsProperties fp_bits_properties,
                             const FormatSection &to_conv) {
#if defined(LIBC_INTERNAL_PRINTF_CONVERT_FLOAT128)
  static constexpr uint32_t MAX_POSSIBLE_FRACTION_LEN =
      fputil::FPBits<float128>::FRACTION_LEN;
  static constexpr uint32_t MAX_POSSIBLE_EXP_LEN =
      fputil::FPBits<float128>::EXP_LEN;
#elif !defined(LIBC_TYPES_LONG_DOUBLE_IS_DOUBLE_DOUBLE)
  static constexpr uint32_t MAX_POSSIBLE_FRACTION_LEN =
      fputil::FPBits<long double>::FRACTION_LEN;
  static constexpr uint32_t MAX_POSSIBLE_EXP_LEN =
      fputil::FPBits<long double>::EXP_LEN;
#else
  static constexpr uint32_t MAX_POSSIBLE_FRACTION_LEN =
      fputil::FPBits<double>::FRACTION_LEN;
  static constexpr uint32_t MAX_POSSIBLE_EXP_LEN =
      fputil::FPBits<double>::EXP_LEN;
#endif

  char sign_char = 0;

  if (fp_bits_properties.is_negative)
    sign_char = '-';
  else if ((to_conv.flags & FormatFlags::FORCE_SIGN) == FormatFlags::FORCE_SIGN)
    sign_char = '+'; // FORCE_SIGN has precedence over SPACE_PREFIX
  else if ((to_conv.flags & FormatFlags::SPACE_PREFIX) ==
           FormatFlags::SPACE_PREFIX)
    sign_char = ' ';

  constexpr size_t BITS_IN_HEX_DIGIT = 4;

  // This is to handle situations where the mantissa isn't an even number of hex
  // digits. This is primarily relevant for x86 80 bit long doubles, which have
  // 63 bit mantissas. In the case where the mantissa is 0, however, the
  // exponent should stay as 0.
  if (fp_bits_properties.fraction_bits % BITS_IN_HEX_DIGIT != 0 &&
      fp_bits_properties.mantissa > 0) {
    fp_bits_properties.exponent -=
        fp_bits_properties.fraction_bits % BITS_IN_HEX_DIGIT;
  }

  // This is the max number of digits it can take to represent the mantissa.
  // Since the number is in bits, we divide by 4, and then add one to account
  // for the extra implicit bit. We use the larger of the two possible values
  // since the size must be constant.
  constexpr size_t MANT_BUFF_LEN =
      (MAX_POSSIBLE_FRACTION_LEN / BITS_IN_HEX_DIGIT) + 1;
  char mant_buffer[MANT_BUFF_LEN];

  size_t mant_len = (fp_bits_properties.fraction_bits / BITS_IN_HEX_DIGIT) + 1;

  // Precision only tracks the number of digits after the hexadecimal point, so
  // we have to add one to account for the digit before the hexadecimal point.
  if (to_conv.precision + 1 < static_cast<int>(mant_len) &&
      to_conv.precision + 1 > 0) {
    const size_t intended_digits = to_conv.precision + 1;
    const size_t shift_amount =
        (mant_len - intended_digits) * BITS_IN_HEX_DIGIT;

    const AnyFloatStorageType truncated_bits =
        fp_bits_properties.mantissa &
        ((AnyFloatStorageType(1) << shift_amount) - 1);
    const AnyFloatStorageType halfway_const = AnyFloatStorageType(1)
                                              << (shift_amount - 1);

    fp_bits_properties.mantissa >>= shift_amount;

#ifdef LIBC_MATH_HAS_ASSUME_ROUND_NEAREST_ONLY
    // Round to nearest, if it's exactly halfway then round to even.
    if (truncated_bits > halfway_const)
      ++fp_bits_properties.mantissa;
    else if (truncated_bits == halfway_const)
      fp_bits_properties.mantissa =
          fp_bits_properties.mantissa + (fp_bits_properties.mantissa & 1);
#else
    switch (fputil::quick_get_round()) {
    case FE_TONEAREST:
      // Round to nearest, if it's exactly halfway then round to even.
      if (truncated_bits > halfway_const)
        ++fp_bits_properties.mantissa;
      else if (truncated_bits == halfway_const)
        fp_bits_properties.mantissa =
            fp_bits_properties.mantissa + (fp_bits_properties.mantissa & 1);
      break;
    case FE_DOWNWARD:
      if (truncated_bits > 0 && fp_bits_properties.is_negative)
        ++fp_bits_properties.mantissa;
      break;
    case FE_UPWARD:
      if (truncated_bits > 0 && !fp_bits_properties.is_negative)
        ++fp_bits_properties.mantissa;
      break;
    case FE_TOWARDZERO:
      break;
    }
#endif // LIBC_MATH_HAS_ASSUME_ROUND_NEAREST_ONLY

    // If the rounding caused an overflow, shift the mantissa and adjust the
    // exponent to match.
    if (fp_bits_properties.mantissa >=
        (AnyFloatStorageType(1) << (intended_digits * BITS_IN_HEX_DIGIT))) {
      fp_bits_properties.mantissa >>= BITS_IN_HEX_DIGIT;
      fp_bits_properties.exponent += BITS_IN_HEX_DIGIT;
    }

    mant_len = intended_digits;
  }

  size_t mant_cur = mant_len;
  size_t first_non_zero = 1;
  for (; mant_cur > 0; --mant_cur, fp_bits_properties.mantissa >>= 4) {
    char mant_mod_16 = static_cast<char>(fp_bits_properties.mantissa % 16);
    char new_digit = internal::int_to_b36_char(mant_mod_16);
    if (internal::isupper(to_conv.conv_name))
      new_digit = internal::toupper(new_digit);
    mant_buffer[mant_cur - 1] = new_digit;
    if (new_digit != '0' && first_non_zero < mant_cur)
      first_non_zero = mant_cur;
  }

  size_t mant_digits = first_non_zero;
  if (to_conv.precision >= 0)
    mant_digits = mant_len;

  // This approximates the number of digits it will take to represent the
  // exponent. The calculation is ceil((bits * 5) / 16). Floor also works, but
  // only on exact multiples of 16. We add 1 for the sign.
  // Relevant sizes:
  // 15 -> 5
  // 11 -> 4
  // 8  -> 3
  constexpr size_t EXP_LEN = (((MAX_POSSIBLE_EXP_LEN * 5) + 15) / 16) + 1;
  char exp_buffer[EXP_LEN];

  bool exp_is_negative = false;
  if (fp_bits_properties.exponent < 0) {
    exp_is_negative = true;
    fp_bits_properties.exponent = -fp_bits_properties.exponent;
  }

  size_t exp_cur = EXP_LEN;
  for (; fp_bits_properties.exponent > 0;
       --exp_cur, fp_bits_properties.exponent /= 10) {
    exp_buffer[exp_cur - 1] =
        internal::int_to_b36_char(fp_bits_properties.exponent % 10);
  }
  if (exp_cur == EXP_LEN) { // if nothing else was written, write a 0.
    exp_buffer[EXP_LEN - 1] = '0';
    exp_cur = EXP_LEN - 1;
  }

  exp_buffer[exp_cur - 1] = exp_is_negative ? '-' : '+';
  --exp_cur;

  // these are signed to prevent underflow due to negative values. The eventual
  // values will always be non-negative.
  size_t trailing_zeroes = 0;
  int padding;

  // prefix is "0x", and always appears.
  constexpr size_t PREFIX_LEN = 2;
  char prefix[PREFIX_LEN];
  prefix[0] = '0';
  prefix[1] = internal::islower(to_conv.conv_name) ? 'x' : 'X';
  const cpp::string_view prefix_str(prefix, PREFIX_LEN);

  // If the precision is greater than the actual result, pad with 0s
  if (to_conv.precision > static_cast<int>(mant_digits - 1))
    trailing_zeroes = to_conv.precision - (mant_digits - 1);

  bool has_hexadecimal_point =
      (mant_digits > 1) || ((to_conv.flags & FormatFlags::ALTERNATE_FORM) ==
                            FormatFlags::ALTERNATE_FORM);
  constexpr cpp::string_view HEXADECIMAL_POINT(".");

  // This is for the letter 'p' before the exponent.
  const char exp_separator = internal::islower(to_conv.conv_name) ? 'p' : 'P';
  constexpr int EXP_SEPARATOR_LEN = 1;

  padding = static_cast<int>(to_conv.min_width - (sign_char > 0 ? 1 : 0) -
                             PREFIX_LEN - mant_digits - trailing_zeroes -
                             static_cast<int>(has_hexadecimal_point) -
                             EXP_SEPARATOR_LEN - (EXP_LEN - exp_cur));
  if (padding < 0)
    padding = 0;

  if ((to_conv.flags & FormatFlags::LEFT_JUSTIFIED) ==
      FormatFlags::LEFT_JUSTIFIED) {
    // The pattern is (sign), 0x, digit, (.), (other digits), (zeroes), p,
    // exponent, (spaces)
    if (sign_char > 0)
      RET_IF_RESULT_NEGATIVE(writer->write(sign_char));
    RET_IF_RESULT_NEGATIVE(writer->write(prefix_str));
    RET_IF_RESULT_NEGATIVE(writer->write(mant_buffer[0]));
    if (has_hexadecimal_point)
      RET_IF_RESULT_NEGATIVE(writer->write(HEXADECIMAL_POINT));
    if (mant_digits > 1)
      RET_IF_RESULT_NEGATIVE(writer->write({mant_buffer + 1, mant_digits - 1}));
    if (trailing_zeroes > 0)
      RET_IF_RESULT_NEGATIVE(writer->write('0', trailing_zeroes));
    RET_IF_RESULT_NEGATIVE(writer->write(exp_separator));
    RET_IF_RESULT_NEGATIVE(
        writer->write({exp_buffer + exp_cur, EXP_LEN - exp_cur}));
    if (padding > 0)
      RET_IF_RESULT_NEGATIVE(writer->write(' ', padding));
  } else {
    // The pattern is (spaces), (sign), 0x, (zeroes), digit, (.), (other
    // digits), (zeroes), p, exponent
    if ((padding > 0) && ((to_conv.flags & FormatFlags::LEADING_ZEROES) !=
                          FormatFlags::LEADING_ZEROES))
      RET_IF_RESULT_NEGATIVE(writer->write(' ', padding));
    if (sign_char > 0)
      RET_IF_RESULT_NEGATIVE(writer->write(sign_char));
    RET_IF_RESULT_NEGATIVE(writer->write(prefix_str));
    if ((padding > 0) && ((to_conv.flags & FormatFlags::LEADING_ZEROES) ==
                          FormatFlags::LEADING_ZEROES))
      RET_IF_RESULT_NEGATIVE(writer->write('0', padding));
    RET_IF_RESULT_NEGATIVE(writer->write(mant_buffer[0]));
    if (has_hexadecimal_point)
      RET_IF_RESULT_NEGATIVE(writer->write(HEXADECIMAL_POINT));
    if (mant_digits > 1)
      RET_IF_RESULT_NEGATIVE(writer->write({mant_buffer + 1, mant_digits - 1}));
    if (trailing_zeroes > 0)
      RET_IF_RESULT_NEGATIVE(writer->write('0', trailing_zeroes));
    RET_IF_RESULT_NEGATIVE(writer->write(exp_separator));
    RET_IF_RESULT_NEGATIVE(
        writer->write({exp_buffer + exp_cur, EXP_LEN - exp_cur}));
  }
  return WRITE_OK;
}

template <OverflowMode mode>
LIBC_INLINE int convert_float_hex_exp(Writer<mode> *writer,
                                      const FormatSection &to_conv) {
  FloatHexExpFPBitsProperties fp_bits_properties =
      get_float_hex_exp_fp_bits_properties_lm(to_conv);
  if (fp_bits_properties.is_inf_or_nan)
    return convert_inf_nan(
        writer,
        InfNanFPBitsProperties{.is_negative = fp_bits_properties.is_negative,
                               .mantissa_is_zero =
                                   fp_bits_properties.mantissa_is_zero},
        to_conv);
  return convert_finite_float_hex_exp(writer, fp_bits_properties, to_conv);
}

} // namespace printf_core
} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC___SUPPORT_PRINTF_CORE_FLOAT_HEX_CONVERTER_H
