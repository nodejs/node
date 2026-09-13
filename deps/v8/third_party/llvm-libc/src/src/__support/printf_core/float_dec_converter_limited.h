//===-- Decimal Float Converter for printf (320-bit float) ------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file implements an alternative to the Ryū printf algorithm in
// float_dec_converter.h. Instead of generating output digits 9 at a time on
// demand, in this implementation, a float is converted to decimal by computing
// just one power of 10 and multiplying/dividing the entire input by it,
// generating the whole string of decimal output digits in one go.
//
// This avoids the large constant lookup table of Ryū, making it more suitable
// for low-memory embedded contexts; but it's also faster than the fallback
// version of Ryū which computes table entries on demand using DyadicFloat,
// because those must calculate a potentially large power of 10 per 9-digit
// output block, whereas this computes just one, which does the whole job.
//
// The calculation is done in 320-bit DyadicFloat, which provides enough
// precision to generate 39 correct digits of output from any floating-point
// size up to and including 128-bit long double, because the rounding errors in
// computing the largest necessary power of 10 are still smaller than the
// distance (in the 320-bit float format) between adjacent 39-decimal-digit
// outputs.
//
// No further digits beyond the 39th are generated: if the printf format string
// asks for more precision than that, the answer is padded with 0s. This is a
// permitted option in IEEE 754-2019 (section 5.12.2): you're allowed to define
// a limit H on the number of decimal digits you can generate, and pad with 0s
// if asked for more than that, subject to the constraint that H must be
// consistent across all float formats you support (you can't use a smaller H
// for single precision than double or long double), and must be large enough
// that even in the largest supported precision the only numbers misrounded are
// ones extremely close to a rounding boundary. 39 digits is the smallest
// permitted value for an implementation supporting binary128.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC___SUPPORT_PRINTF_CORE_FLOAT_DEC_CONVERTER_LIMITED_H
#define LLVM_LIBC_SRC___SUPPORT_PRINTF_CORE_FLOAT_DEC_CONVERTER_LIMITED_H

#include "src/__support/CPP/algorithm.h"
#include "src/__support/CPP/string.h"
#include "src/__support/CPP/string_view.h"
#include "src/__support/FPUtil/FPBits.h"
#include "src/__support/FPUtil/dyadic_float.h"
#include "src/__support/FPUtil/rounding_mode.h"
#include "src/__support/integer_to_string.h"
#include "src/__support/libc_assert.h"
#include "src/__support/macros/config.h"
#include "src/__support/printf_core/core_structs.h"
#include "src/__support/printf_core/float_inf_nan_converter.h"
#include "src/__support/printf_core/writer.h"
#include "src/string/memory_utils/inline_memcpy.h"

namespace LIBC_NAMESPACE_DECL {
namespace printf_core {

enum class ConversionType { E, F, G };

constexpr unsigned MAX_DIGITS = 39;
constexpr size_t DF_BITS = 320;

struct DigitsInput {
  // Input mantissa, stored with the explicit leading 1 bit (if any) at the
  // top. So either it has a value in the range [2^127,2^128) representing a
  // real number in [1,2), or it has the value 0, representing 0.
  UInt128 mantissa;

  // Input exponent, as a power of 2 to multiply into mantissa.
  int exponent;

  // Input sign.
  Sign sign;

  // Constructor which accepts a mantissa direct from a floating-point format,
  // and shifts it up to the top of the UInt128 so that a function consuming
  // this struct afterwards doesn't have to remember which format it came from.
  DigitsInput(int32_t fraction_len, AnyFloatStorageType mantissa_,
              int exponent_, Sign sign)
      : mantissa(UInt128(mantissa_) << (127 - fraction_len)),
        exponent(exponent_), sign(sign) {
    if (!(mantissa & (UInt128(1) << 127)) && mantissa != 0) {
      // Normalize a denormalized input.
      int shift = cpp::countl_zero(mantissa);
      mantissa <<= shift;
      exponent -= shift;
    }
  }
};

struct DigitsOutput {
  // Output from decimal_digits().
  //
  // `digits` is a buffer containing nothing but ASCII digits. Even if the
  // decimal point needs to appear somewhere in the final output string, it
  // isn't represented in _this_ string; the client of this object will insert
  // it in an appropriate place. `ndigits` gives the buffer size.
  //
  // `exponent` represents the exponent you would display if the decimal point
  // comes after the first digit of decimal_digits, e.g. if digits == "1234"
  // and exponent = 3 then this represents 1.234e3, or just the integer 1234.
  size_t ndigits;
  int exponent;
  char digits[MAX_DIGITS + 1];
};

// Estimate log10 of a power of 2, by multiplying its exponent by
// 1292913986/2^32. That is a rounded-down approximation to log10(2), accurate
// enough that for any binary exponent in the range of float128 it will give
// the correct value of floor(log10(2^n)).
LIBC_INLINE int estimate_log10(int exponent_of_2) {
  return static_cast<int>((exponent_of_2 * 1292913986LL) >> 32);
}

// Calculate the actual digits of a decimal representation of an FP number.
//
// If `e_mode` is true, then `precision` indicates the desired number of output
// decimal digits. On return, `decimal_digits` will be a string of length
// exactly `precision` starting with a nonzero digit; `decimal_exponent` will
// be filled in to indicate the exponent as shown above.
//
// If `e_mode` is false, then `precision` indicates the desired number of
// digits after the decimal point. On return, the last digit in the string
// `decimal_digits` has a place value of _at least_ 10^-precision. But also, at
// most `MAX_DIGITS` digits are returned, so the caller may need to pad it at
// the end with the appropriate number of extra 0s.
LIBC_INLINE
DigitsOutput decimal_digits(DigitsInput input, int precision, bool e_mode) {
  if (input.mantissa == 0) {
    // Special-case zero, by manually generating the right number of zero
    // digits and setting an appropriate exponent.
    DigitsOutput output;
    if (!e_mode) {
      // In F mode, it's enough to return an empty string of digits. That's the
      // same thing we do when given a nonzero number that rounds down to 0.
      output.ndigits = 0;
      output.exponent = -precision - 1;
    } else {
      // In E mode, generate a string containing the expected number of 0s.
      __builtin_memset(output.digits, '0', precision);
      output.ndigits = precision;
      output.exponent = 0;
    }
    return output;
  }

  // Calculate bounds on log10 of the input value. Its binary exponent bounds
  // the value between two powers of 2, and we use estimate_log10 to determine
  // log10 of each of those.
  //
  // If a power of 10 falls in the interval between those powers of 2, then
  // log10_input_min and log10_input_max will differ by 1, and the correct
  // decimal exponent of the output will be one of those two values. If no
  // power of 10 is in the interval, then these two values will be equal and
  // there is only one choice for the decimal exponent.
  int log10_input_min = estimate_log10(input.exponent - 1);
  int log10_input_max = estimate_log10(input.exponent);

  // Make a DyadicFloat containing the value 10, to use as the base for
  // exponentiation.
  fputil::DyadicFloat<DF_BITS> ten(Sign::POS, 1, 5);

  // Compute the exponent of the lowest-order digit we want as output. In F
  // mode this depends only on the desired precision. In E mode it's based on
  // log10_input, which is (an estimate of) the exponent corresponding to the
  // _high_-order decimal digit of the number.
  int log10_low_digit = e_mode ? log10_input_min + 1 - precision : -precision;

  // The general plan is to calculate an integer whose decimal representation
  // is precisely the string of output digits, by doing a DyadicFloat
  // computation of (input_mantissa / 10^(log10_low_digit)) and then rounding
  // that to an integer.
  //
  // The number of output decimal digits (if the mathematical result of this
  // operation were computed without overflow) will be one of these:
  //   (log10_input_min - log10_low_digit + 1)
  //   (log10_input_max - log10_low_digit + 1)
  //
  // In E mode, this means we'll either get the correct number of output digits
  // immediately, or else one too many (in which case we can correct for that
  // at the rounding stage). But in F mode, if the number is very large
  // compared to the number of decimal places the user asked for, we might be
  // about to generate far too many digits and overflow our float format. In
  // that case, reset to E mode immediately, to avoid having to detect the
  // overflow _after_ the multiplication and retry. So if even the smaller
  // number of possible output digits is too many, we might as well change our
  // mind right now and switch into E mode.
  if (log10_input_max - log10_low_digit + 1 > int(MAX_DIGITS)) {
    precision = MAX_DIGITS;
    e_mode = true;
    log10_low_digit = log10_input_min + 1 - precision;
  }

  // Now actually calculate (input_mantissa / 10^(log10_low_digit)).
  //
  // If log10_low_digit < 0, then we calculate 10^(-log10_low_digit) and
  // multiply by it instead, so that the exponent is non-negative in all cases.
  // This ensures that the power of 10 is always mathematically speaking an
  // integer, so that it can be represented exactly in binary (without a
  // recurring fraction), and when it's small enough to fit in DF_BITS,
  // fputil::pow_n should return the exact answer, and then
  // fputil::rounded_{div,mul} will introduce only the unavoidable rounding
  // error of up to 1/2 ULP.
  //
  // Beyond that point, pow_n will be imprecise. But DF_BITS is set high enough
  // that even for the most difficult cases in 128-bit long double, the extra
  // precision in the calculation is enough to ensure we still get the right
  // answer.
  //
  // If the output integer doesn't fit in DF_BITS, we set the `overflow` flag.

  // Calculate the power of 10 to divide or multiply by.
  fputil::DyadicFloat<DF_BITS> power_of_10 =
      fputil::pow_n(ten, cpp::abs(log10_low_digit));

  // Convert the mantissa into a DyadicFloat, making sure it has the right
  // sign, so that directed rounding will go in the right direction, if
  // enabled.
  fputil::DyadicFloat<DF_BITS> flt_mantissa(
      input.sign,
      input.exponent -
          (cpp::numeric_limits<decltype(input.mantissa)>::digits - 1),
      input.mantissa);

  // Divide or multiply, depending on whether log10_low_digit was positive
  // or negative.
  fputil::DyadicFloat<DF_BITS> flt_quotient =
      log10_low_digit > 0 ? fputil::rounded_div(flt_mantissa, power_of_10)
                          : fputil::rounded_mul(flt_mantissa, power_of_10);

  // Convert to an integer.
  int round_dir;
  UInt<DF_BITS> integer = flt_quotient.as_mantissa_type_rounded(&round_dir);

  // And take the absolute value.
  if (flt_quotient.sign.is_neg())
    integer = -integer;

  // Convert the mantissa integer into a string of decimal digits, and check
  // to see if it's the right size.
  const IntegerToString<decltype(integer), radix::Dec> buf{integer};
  cpp::string_view view = buf.view();

  // Start making the output struct, by copying in the digits from the above
  // object. At this stage we may also have one digit too many (but that's OK,
  // there's space for it in the DigitsOutput buffer).
  DigitsOutput output;
  output.ndigits = view.size();
  inline_memcpy(output.digits, view.data(), output.ndigits);

  // Set up the output exponent, which is done differently depending on mode.
  // Also, figure out whether we have one digit too many, and if so, set the
  // `need_reround` flag and adjust the exponent appropriately.
  bool need_reround = false;
  if (e_mode) {
    // In E mode, the output exponent is the exponent of the first decimal
    // digit, which we already calculated.
    output.exponent = log10_input_min;

    // In E mode, we're returning a fixed number of digits, given by
    // `precision`, so if we have more than that, then we must shorten the
    // buffer by one digit.
    //
    // If this happens, it's because the actual log10 of the input is
    // log10_input_min + 1. Equivalently, we guessed we'd see something like
    // X.YZe+NN and instead got WX.YZe+NN. So when we shorten the digit string
    // by one, we'll also need to increment the output exponent.
    if (output.ndigits > size_t(precision)) {
      LIBC_ASSERT(output.ndigits == size_t(precision) + 1);
      need_reround = true;
      output.exponent++;
    }
  } else {
    // In F mode, the output exponent is based on the place value of the _last_
    // digit, so we must recover the exponent of the first digit by adding
    // the number of digits.
    //
    // Because this takes the length of the buffer into account, it sets the
    // correct decimal exponent even if this digit string is one too long. So
    // we don't need to adjust the exponent if we reround.
    output.exponent = int(output.ndigits) - precision - 1;

    // In F mode, the number of returned digits isn't based on `precision`:
    // it's variable, and we don't mind how many digits we get as long as it
    // isn't beyond the limit MAX_DIGITS. If it is, we expect that it's only
    // one digit too long, or else we'd have spotted the problem in advance and
    // flipped into E mode already.
    if (output.ndigits > MAX_DIGITS) {
      LIBC_ASSERT(output.ndigits == MAX_DIGITS + 1);
      need_reround = true;
    }
  }

  if (need_reround) {
    // If either of the branches above decided that we had one digit too many,
    // we must now shorten the digit buffer by one. But we can't just truncate:
    // we need to make sure the remaining n-1 digits are correctly rounded, as
    // if we'd rounded just once from the original `flt_quotient`.
    //
    // In directed rounding modes this can't go wrong. If you had a real number
    // x, and the first rounding produced floor(x), then the second rounding
    // wants floor(x/10), and it doesn't matter if you actually compute
    // floor(floor(x)/10): the result is the same, because each rounding
    // boundary in the second rounding aligns with one in the first rounding,
    // which nothing could have crossed. Similarly for rounding away from zero,
    // with 'floor' replaced with 'ceil' throughout.
    //
    // In rounding to nearest, the danger is in the boundary case where the
    // final digit of the original output is 5. Then if we just rerounded the
    // digit string to remove the last digit, it would look like an exact
    // halfway case, and we'd break the tie by choosing the even one of the two
    // outputs. But if the original value before the first rounding was on one
    // side or the other of 5, then that supersedes the 'round to even' tie
    // break. So we need to consult `round_dir` from above, which tells us
    // which way (if either) the value was adjusted during the first rounding.
    // Effectively, we treat the last digit as 5+ε or 5-ε.
    //
    // To make this work in both directed modes and round-to-nearest mode
    // without having to look up the rounding direction, a simple rule is: take
    // account of round_dir if and only if the round digit (the one we're
    // removing when shortening the buffer) is 5. In directed rounding modes
    // this makes no difference.

    // Extract the two relevant digits. round_digit is the one we're removing;
    // new_low_digit is the last one we're keeping, so we need to know if it's
    // even or odd to handle exact tie cases (when round_dir == 0).
    --output.ndigits;
    int round_digit = internal::b36_char_to_int(output.digits[output.ndigits]);
    int new_low_digit =
        output.ndigits == 0
            ? 0
            : internal::b36_char_to_int(output.digits[output.ndigits - 1]);

    // Make a binary number that we can pass to `fputil::rounding_direction`.
    // We put new_low_digit at bit 8, and imagine that we're rounding away the
    // bottom 8 bits. Therefore round_digit must be "just below" bit 8, in the
    // sense that we set the bottom 8 bits to (256/10 * round_digit) so that
    // round_digit==5 corresponds to the binary half-way case of 0x80.
    //
    // Then we adjust by +1 or -1 based on round_dir if the round digit is 5,
    // as described above.
    //
    // The subexpression `(round_digit * 0x19a) >> 4` is computing the
    // expression (256/10 * round_digit) mentioned above, accurately enough to
    // map 5 to exactly 128 but avoiding an integer division (for platforms
    // where it's slow, e.g. not in hardware).
    LIBC_NAMESPACE::UInt<64> round_word = (new_low_digit * 256) +
                                          ((round_digit * 0x19a) >> 4) +
                                          (round_digit == 5 ? -round_dir : 0);

    // Now we can call the existing binary rounding helper function, which
    // takes account of the rounding mode.
    if (fputil::rounding_direction(round_word, 8, flt_quotient.sign) > 0) {
      // If that returned a positive answer, we must round the number up.
      //
      // The number is already in decimal, so we need to increment it one digit
      // at a time. (A bit painful, but better than going back to the integer
      // we made it from and doing the decimal conversion all over again.)
      for (size_t i = output.ndigits; i-- > 0;) {
        if (output.digits[i] != '9') {
          output.digits[i] = internal::int_to_b36_char(
              internal::b36_char_to_int(output.digits[i]) + 1);
          break;
        } else {
          output.digits[i] = '0';
        }
      }
    }
  }

  return output;
}

template <WriteMode write_mode>
LIBC_INLINE int convert_float_inner(Writer<write_mode> *writer,
                                    const FormatSection &to_conv,
                                    int32_t fraction_len, int exponent,
                                    AnyFloatStorageType mantissa, Sign sign,
                                    ConversionType ctype) {
  constexpr char DECIMAL_POINT = '.';
  // If to_conv doesn't specify a precision, the precision defaults to 6.
  unsigned precision = to_conv.precision < 0 ? 6 : to_conv.precision;

  // Decide if we're displaying a sign character, depending on the format flags
  // and whether the input is negative.
  char sign_char = 0;
  if (sign.is_neg())
    sign_char = '-';
  else if ((to_conv.flags & FormatFlags::FORCE_SIGN) == FormatFlags::FORCE_SIGN)
    sign_char = '+'; // FORCE_SIGN has precedence over SPACE_PREFIX
  else if ((to_conv.flags & FormatFlags::SPACE_PREFIX) ==
           FormatFlags::SPACE_PREFIX)
    sign_char = ' ';

  // Prepare the input to decimal_digits().
  DigitsInput input(fraction_len, mantissa, exponent, sign);

  // Call decimal_digits() in a different way, based on whether the format
  // character is 'e', 'f', or 'g'. After this loop we expect to have filled
  // in the following variables:

  // The decimal digits, and the exponent of the topmost one.
  DigitsOutput output;
  // The start and end of the digit string we're displaying, as indices into
  // `output.digits`. The indices may be out of bounds in either direction, in
  // which case digits beyond the bounds of the buffer should be displayed as
  // zeroes.
  //
  // As usual, the index 'start' is included, and 'limit' is not.
  int start, limit;
  // The index of the digit that we display a decimal point immediately after.
  // Again, represented as an index in `output.digits`, and may be out of
  // bounds.
  int pointpos;
  // Whether we need to display an "e+NNN" exponent suffix at all.
  bool show_exponent;

  switch (ctype) {
  case ConversionType::E:
    // In E mode, we display one digit more than the specified precision
    // (`%.6e` means six digits _after_ the decimal point, like 1.123456e+00).
    //
    // Also, bound the number of digits we request at MAX_DIGITS.
    output = decimal_digits(input, cpp::min(precision + 1, MAX_DIGITS), true);

    // We display digits from the start of the buffer, and always output
    // `precision+1` of them (which will append zeroes if the user requested
    // more than MAX_DIGITS).
    start = 0;
    limit = precision + 1;

    // The decimal point is always after the first digit of the buffer.
    pointpos = start;

    // The exponent is always displayed explicitly.
    show_exponent = true;
    break;
  case ConversionType::F:
    // In F mode, we provide decimal_digits() with the unmodified input
    // precision, and let it give us as many digits as we can.
    output = decimal_digits(input, precision, false);

    // Initialize (start, limit) to display everything from the first nonzero
    // digit (necessarily at the start of the output buffer) to the digit at
    // the correct distance after the decimal point.
    start = 0;
    limit = 1 + output.exponent + precision;

    // But we must display at least one digit _before_ the decimal point, i.e.
    // at least precision+1 digits in total. So if we're not already doing
    // that, we must correct those values.
    if (limit <= int(precision))
      start -= precision + 1 - limit;

    // The decimal point appears precisely 'precision' digits before the end of
    // the digits we output.
    pointpos = limit - 1 - precision;

    // The exponent is never displayed.
    show_exponent = false;
    break;
  case ConversionType::G:
    // In G mode, the precision says exactly how many significant digits you
    // want. (In that respect it's subtly unlike E mode: %.6g means six digits
    // _including_ the one before the point, whereas %.6e means six digits
    // _excluding_ that one.)
    //
    // Also, a precision of 0 is treated the same as 1.
    precision = cpp::max(precision, 1u);
    output = decimal_digits(input, cpp::min(precision, MAX_DIGITS), true);

    // As in E mode, we default to displaying precisely the digits in the
    // output buffer.
    start = 0;
    limit = precision;

    // If we're not in ALTERNATE_FORM mode, trailing zeroes on the mantissa are
    // removed (although not to the extent of leaving no digits at all - if the
    // entire output mantissa is all 0 then we keep a single zero digit).
    if (!(to_conv.flags & FormatFlags::ALTERNATE_FORM)) {
      // Start by removing trailing zeroes that were outside the buffer
      // entirely.
      limit = cpp::min(limit, int(output.ndigits));

      // Then check the digits in the buffer and remove as many as possible.
      while (limit > 1 && output.digits[limit - 1] == '0')
        limit--;
    }

    // Decide whether to display in %e style with an explicit exponent, or %f
    // style with the decimal point after the units place.
    //
    // %e mode is used to avoid an excessive number of leading zeroes after the
    // decimal point but before the first nonzero digit (specifically, 0.0001
    // is fine as it is, but 0.00001 prints as 1e-5), and also to avoid adding
    // trailing zeroes if the last digit in the buffer is still higher than the
    // units place.
    //
    // output.exponent is an int whereas precision is unsigned, so we must
    // check output.exponent >= 0 before comparing it against precision to
    // prevent a negative exponent from wrapping round to a large unsigned int.
    if ((output.exponent >= 0 && output.exponent >= int(precision)) ||
        output.exponent < -4) {
      // Display in %e style, so the point goes after the first digit and the
      // exponent is shown.
      pointpos = start;
      show_exponent = true;
    } else {
      // Display in %f style, so the point goes at its true mathematical
      // location and the exponent is not shown.
      pointpos = output.exponent;
      show_exponent = false;

      if (output.exponent < 0) {
        // If the first digit is below the decimal point, add leading zeroes.
        // (This _decreases_ start, because output.exponent is negative here.)
        start += output.exponent;
      } else if (limit <= output.exponent) {
        // If the last digit is above the decimal point, add trailing zeroes.
        // (This may involve putting back some zeroes that we trimmed in the
        // loop above!)
        limit = output.exponent + 1;
      }
    }
    break;
  }

  // Find out for sure whether we're displaying the decimal point, so that we
  // can include it in the calculation of the total string length for padding.
  //
  // We never expect pointpos to be _before_ the start of the displayed range
  // of digits. (If it had been, we'd have added leading zeroes.) But it might
  // be beyond the end.
  //
  // We don't display the point if it appears immediately after the _last_
  // digit we display, except in ALTERNATE_FORM mode.
  int last_point_digit =
      (to_conv.flags & FormatFlags::ALTERNATE_FORM) ? limit - 1 : limit - 2;
  bool show_point = pointpos <= last_point_digit;

  // Format the exponent suffix (e+NN, e-NN) into a buffer, or leave the buffer
  // empty if we're not displaying one.
  char expbuf[16]; // more than enough space for e+NNNN
  size_t explen = 0;
  if (show_exponent) {
    const IntegerToString<decltype(output.exponent),
                          radix::Dec::WithWidth<2>::WithSign>
        expcvt{output.exponent};
    cpp::string_view expview = expcvt.view();
    expbuf[0] = internal::islower(to_conv.conv_name) ? 'e' : 'E';
    explen = expview.size() + 1;
    inline_memcpy(expbuf + 1, expview.data(), expview.size());
  }

  // Now we know enough to work out the length of the unpadded output:
  //  * whether to write a sign
  //  * how many mantissa digits to write
  //  * whether to write a decimal point
  //  * the length of the trailing exponent string.
  size_t unpadded_len =
      (sign_char != 0) + (limit - start) + show_point + explen;

  // Work out how much padding is needed.
  size_t min_width = to_conv.min_width > 0 ? to_conv.min_width : 0;
  size_t padding_amount = cpp::max(min_width, unpadded_len) - unpadded_len;

  // Work out what the padding looks like and where it appears.
  enum class Padding {
    LeadingSpace,  // spaces at the start of the string
    Zero,          // zeroes between sign and mantissa
    TrailingSpace, // spaces at the end of the string
  } padding = Padding::LeadingSpace;
  // The '-' flag for left-justification takes priority over the '0' flag
  if (to_conv.flags & FormatFlags::LEFT_JUSTIFIED)
    padding = Padding::TrailingSpace;
  else if (to_conv.flags & FormatFlags::LEADING_ZEROES)
    padding = Padding::Zero;

  // Finally, write all the output!

  // Leading-space padding, if any
  if (padding == Padding::LeadingSpace)
    RET_IF_RESULT_NEGATIVE(writer->write(' ', padding_amount));

  // Sign, if any
  if (sign_char)
    RET_IF_RESULT_NEGATIVE(writer->write(sign_char));

  // Zero padding, if any
  if (padding == Padding::Zero)
    RET_IF_RESULT_NEGATIVE(writer->write('0', padding_amount));

  // Mantissa digits, maybe with a decimal point
  for (int pos = start; pos < limit; ++pos) {
    if (pos >= 0 && pos < int(output.ndigits)) {
      // Fetch a digit from the buffer
      RET_IF_RESULT_NEGATIVE(writer->write(output.digits[pos]));
    } else {
      // This digit is outside the buffer, so write a zero
      RET_IF_RESULT_NEGATIVE(writer->write('0'));
    }

    // Show the decimal point, if this is the digit it comes after
    if (show_point && pos == pointpos)
      RET_IF_RESULT_NEGATIVE(writer->write(DECIMAL_POINT));
  }

  // Exponent
  RET_IF_RESULT_NEGATIVE(writer->write(cpp::string_view(expbuf, explen)));

  // Trailing-space padding, if any
  if (padding == Padding::TrailingSpace)
    RET_IF_RESULT_NEGATIVE(writer->write(' ', padding_amount));

  return WRITE_OK;
}

template <typename T, WriteMode write_mode,
          cpp::enable_if_t<cpp::is_floating_point_v<T>, int> = 0>
LIBC_INLINE int
convert_float_typed(Writer<write_mode> *writer, const FormatSection &to_conv,
                    fputil::FPBits<T> float_bits, ConversionType ctype) {
  return convert_float_inner(writer, to_conv, float_bits.FRACTION_LEN,
                             float_bits.get_explicit_exponent(),
                             float_bits.get_explicit_mantissa(),
                             float_bits.sign(), ctype);
}

template <WriteMode write_mode>
LIBC_INLINE int convert_float_outer(Writer<write_mode> *writer,
                                    const FormatSection &to_conv,
                                    ConversionType ctype) {
#if defined(LIBC_INTERNAL_PRINTF_CONVERT_FLOAT128)
  if (to_conv.length_modifier == LengthModifier::Q) {
    fputil::FPBits<float128> float_bits(
        static_cast<fputil::FPBits<float128>::StorageType>(
            to_conv.conv_val_raw));
    if (!float_bits.is_inf_or_nan()) {
      return convert_float_typed<float128>(writer, to_conv, float_bits, ctype);
    }
  } else
#endif // LIBC_INTERNAL_PRINTF_CONVERT_FLOAT128
#ifndef LIBC_TYPES_LONG_DOUBLE_IS_DOUBLE_DOUBLE
      if (to_conv.length_modifier == LengthModifier::L) {
    fputil::FPBits<long double> float_bits(
        static_cast<fputil::FPBits<long double>::StorageType>(
            to_conv.conv_val_raw));
    if (!float_bits.is_inf_or_nan()) {
      return convert_float_typed<long double>(writer, to_conv, float_bits,
                                              ctype);
    }
  } else
#endif // !LIBC_TYPES_LONG_DOUBLE_IS_DOUBLE_DOUBLE
  {
    fputil::FPBits<double>::StorageType float_raw =
        static_cast<fputil::FPBits<double>::StorageType>(to_conv.conv_val_raw);
    fputil::FPBits<double> float_bits(float_raw);
    if (!float_bits.is_inf_or_nan()) {
      return convert_float_typed<double>(writer, to_conv, float_bits, ctype);
    }
  }

  return convert_inf_nan(writer, to_conv);
}

template <typename T, WriteMode write_mode,
          cpp::enable_if_t<cpp::is_floating_point_v<T>, int> = 0>
LIBC_INLINE int convert_float_decimal_typed(Writer<write_mode> *writer,
                                            const FormatSection &to_conv,
                                            fputil::FPBits<T> float_bits) {
  return convert_float_typed<T>(writer, to_conv, float_bits, ConversionType::F);
}

template <typename T, WriteMode write_mode,
          cpp::enable_if_t<cpp::is_floating_point_v<T>, int> = 0>
LIBC_INLINE int convert_float_dec_exp_typed(Writer<write_mode> *writer,
                                            const FormatSection &to_conv,
                                            fputil::FPBits<T> float_bits) {
  return convert_float_typed<T>(writer, to_conv, float_bits, ConversionType::E);
}

template <typename T, WriteMode write_mode,
          cpp::enable_if_t<cpp::is_floating_point_v<T>, int> = 0>
LIBC_INLINE int convert_float_dec_auto_typed(Writer<write_mode> *writer,
                                             const FormatSection &to_conv,
                                             fputil::FPBits<T> float_bits) {
  return convert_float_typed<T>(writer, to_conv, float_bits, ConversionType::G);
}

template <WriteMode write_mode>
LIBC_INLINE int convert_float_decimal(Writer<write_mode> *writer,
                                      const FormatSection &to_conv) {
  return convert_float_outer(writer, to_conv, ConversionType::F);
}

template <WriteMode write_mode>
LIBC_INLINE int convert_float_dec_exp(Writer<write_mode> *writer,
                                      const FormatSection &to_conv) {
  return convert_float_outer(writer, to_conv, ConversionType::E);
}

template <WriteMode write_mode>
LIBC_INLINE int convert_float_dec_auto(Writer<write_mode> *writer,
                                       const FormatSection &to_conv) {
  return convert_float_outer(writer, to_conv, ConversionType::G);
}

} // namespace printf_core
} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC___SUPPORT_PRINTF_CORE_FLOAT_DEC_CONVERTER_LIMITED_H
