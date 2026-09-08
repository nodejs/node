//===-- Integer Converter for printf ----------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC___SUPPORT_PRINTF_CORE_INT_CONVERTER_H
#define LLVM_LIBC_SRC___SUPPORT_PRINTF_CORE_INT_CONVERTER_H

#include "src/__support/CPP/span.h"
#include "src/__support/CPP/string_view.h"
#include "src/__support/ctype_utils.h"
#include "src/__support/integer_to_string.h"
#include "src/__support/macros/config.h"
#include "src/__support/printf_core/converter_utils.h"
#include "src/__support/printf_core/core_structs.h"
#include "src/__support/printf_core/writer.h"

#include <inttypes.h>
#include <stddef.h>

namespace LIBC_NAMESPACE_DECL {
namespace printf_core {

namespace details {

using HexFmt = IntegerToString<uintmax_t, radix::Hex>;
using HexFmtUppercase = IntegerToString<uintmax_t, radix::Hex::Uppercase>;
using OctFmt = IntegerToString<uintmax_t, radix::Oct>;
using DecFmt = IntegerToString<uintmax_t>;
using BinFmt = IntegerToString<uintmax_t, radix::Bin>;

LIBC_INLINE constexpr size_t num_buf_size() {
  cpp::array<size_t, 5> sizes{
      HexFmt::buffer_size(), HexFmtUppercase::buffer_size(),
      OctFmt::buffer_size(), DecFmt::buffer_size(), BinFmt::buffer_size()};

  auto result = sizes[0];
  for (size_t i = 1; i < sizes.size(); i++)
    result = cpp::max(result, sizes[i]);
  return result;
}

LIBC_INLINE cpp::optional<cpp::string_view>
num_to_strview(uintmax_t num, cpp::span<char> bufref, char conv_name) {
  if (internal::tolower(conv_name) == 'x') {
    if (internal::islower(conv_name))
      return HexFmt::format_to(bufref, num);
    else
      return HexFmtUppercase::format_to(bufref, num);
  } else if (conv_name == 'o') {
    return OctFmt::format_to(bufref, num);
  } else if (internal::tolower(conv_name) == 'b') {
    return BinFmt::format_to(bufref, num);
  } else {
    return DecFmt::format_to(bufref, num);
  }
}

} // namespace details

template <WriteMode write_mode>
LIBC_INLINE int convert_int(Writer<write_mode> *writer,
                            const FormatSection &to_conv) {
  static constexpr size_t BITS_IN_BYTE = 8;
  static constexpr size_t BITS_IN_NUM = sizeof(uintmax_t) * BITS_IN_BYTE;

  uintmax_t num = static_cast<uintmax_t>(to_conv.conv_val_raw);
  bool is_negative = false;
  FormatFlags flags = to_conv.flags;

  // If the conversion is signed, then handle negative values.
  if (to_conv.conv_name == 'd' || to_conv.conv_name == 'i') {
    // Check if the number is negative by checking the high bit. This works even
    // for smaller numbers because they're sign extended by default.
    if ((num & (uintmax_t(1) << (BITS_IN_NUM - 1))) > 0) {
      is_negative = true;
      num = -num;
    }
  } else {
    // These flags are only for signed conversions, so this removes them if the
    // conversion is unsigned.
    flags = FormatFlags(flags &
                        ~(FormatFlags::FORCE_SIGN | FormatFlags::SPACE_PREFIX));
  }

  num =
      apply_length_modifier(num, {to_conv.length_modifier, to_conv.bit_width});
  cpp::array<char, details::num_buf_size()> buf;
  auto str = details::num_to_strview(num, buf, to_conv.conv_name);
  if (!str)
    return INT_CONVERSION_ERROR;

  size_t digits_written = str->size();

  char sign_char = 0;

  if (is_negative)
    sign_char = '-';
  else if ((flags & FormatFlags::FORCE_SIGN) == FormatFlags::FORCE_SIGN)
    sign_char = '+'; // FORCE_SIGN has precedence over SPACE_PREFIX
  else if ((flags & FormatFlags::SPACE_PREFIX) == FormatFlags::SPACE_PREFIX)
    sign_char = ' ';

  // These are signed to prevent underflow due to negative values. The eventual
  // values will always be non-negative.
  int zeroes;
  int spaces;

  // prefix is "0x" for hexadecimal, or the sign character for signed
  // conversions. Since hexadecimal is unsigned these will never conflict.
  size_t prefix_len;
  char prefix[2];
  if ((internal::tolower(to_conv.conv_name) == 'x') &&
      ((flags & FormatFlags::ALTERNATE_FORM) != 0) && num != 0) {
    prefix_len = 2;
    prefix[0] = '0';
    prefix[1] = internal::islower(to_conv.conv_name) ? 'x' : 'X';
  } else if ((internal::tolower(to_conv.conv_name) == 'b') &&
             ((flags & FormatFlags::ALTERNATE_FORM) != 0) && num != 0) {
    prefix_len = 2;
    prefix[0] = '0';
    prefix[1] = internal::islower(to_conv.conv_name) ? 'b' : 'B';
  } else {
    prefix_len = (sign_char == 0 ? 0 : 1);
    prefix[0] = sign_char;
  }

  // Negative precision indicates that it was not specified.
  if (to_conv.precision < 0) {
    if ((flags & (FormatFlags::LEADING_ZEROES | FormatFlags::LEFT_JUSTIFIED)) ==
        FormatFlags::LEADING_ZEROES) {
      // If this conv has flag 0 but not - and no specified precision, it's
      // padded with 0's instead of spaces identically to if precision =
      // min_width - (1 if sign_char). For example: ("%+04d", 1) -> "+001"
      zeroes =
          static_cast<int>(to_conv.min_width - digits_written - prefix_len);
      spaces = 0;
    } else {
      // If there are enough digits to pass over the precision, just write the
      // number, padded by spaces.
      zeroes = 0;
      spaces =
          static_cast<int>(to_conv.min_width - digits_written - prefix_len);
    }
  } else {
    // If precision was specified, possibly write zeroes, and possibly write
    // spaces. Example: ("%5.4d", 10000) -> "10000"
    // If the check for if zeroes is negative was not there, spaces would be
    // incorrectly evaluated as 1.
    //
    // The standard treats the case when num and precision are both zeroes as
    // special - it requires that no characters are produced. So, we adjust for
    // that special case first.
    if (num == 0 && to_conv.precision == 0)
      digits_written = 0;
    zeroes = static_cast<int>(to_conv.precision -
                              digits_written); // a negative value means 0
    if (zeroes < 0)
      zeroes = 0;
    spaces = static_cast<int>(to_conv.min_width - zeroes - digits_written -
                              prefix_len);
  }

  // The standard says that alternate form for the o conversion "increases
  // the precision, if and only if necessary, to force the first digit of the
  // result to be a zero (if the value and precision are both 0, a single 0 is
  // printed)"
  // This if checks the following conditions:
  // 1) is this an o conversion in alternate form?
  // 2) does this number has a leading zero?
  //    2a) ... because there are additional leading zeroes?
  //    2b) ... because it is just "0", unless it will not write any digits.
  const bool has_leading_zero =
      (zeroes > 0) || ((num == 0) && (digits_written != 0));
  if ((to_conv.conv_name == 'o') &&
      ((to_conv.flags & FormatFlags::ALTERNATE_FORM) != 0) &&
      !has_leading_zero) {
    zeroes = 1;
    --spaces;
  }

  if ((flags & FormatFlags::LEFT_JUSTIFIED) == FormatFlags::LEFT_JUSTIFIED) {
    // If left justified it goes prefix zeroes digits spaces
    if (prefix_len != 0)
      RET_IF_RESULT_NEGATIVE(writer->write({prefix, prefix_len}));
    if (zeroes > 0)
      RET_IF_RESULT_NEGATIVE(writer->write('0', zeroes));
    if (digits_written > 0)
      RET_IF_RESULT_NEGATIVE(writer->write(*str));
    if (spaces > 0)
      RET_IF_RESULT_NEGATIVE(writer->write(' ', spaces));
  } else {
    // Else it goes spaces prefix zeroes digits
    if (spaces > 0)
      RET_IF_RESULT_NEGATIVE(writer->write(' ', spaces));
    if (prefix_len != 0)
      RET_IF_RESULT_NEGATIVE(writer->write({prefix, prefix_len}));
    if (zeroes > 0)
      RET_IF_RESULT_NEGATIVE(writer->write('0', zeroes));
    if (digits_written > 0)
      RET_IF_RESULT_NEGATIVE(writer->write(*str));
  }
  return WRITE_OK;
}

} // namespace printf_core
} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC___SUPPORT_PRINTF_CORE_INT_CONVERTER_H
