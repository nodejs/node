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

template <typename CharT>
using HexFmt = IntegerToString<uintmax_t, radix::Hex, CharT>;

template <typename CharT>
using HexFmtUppercase =
    IntegerToString<uintmax_t, radix::Hex::Uppercase, CharT>;

template <typename CharT>
using OctFmt = IntegerToString<uintmax_t, radix::Oct, CharT>;

template <typename CharT>
using DecFmt = IntegerToString<uintmax_t, radix::Dec, CharT>;

template <typename CharT>
using BinFmt = IntegerToString<uintmax_t, radix::Bin, CharT>;

template <typename CharT> LIBC_INLINE constexpr size_t num_buf_size() {
  cpp::array<size_t, 5> sizes{
      HexFmt<CharT>::buffer_size(), HexFmtUppercase<CharT>::buffer_size(),
      OctFmt<CharT>::buffer_size(), DecFmt<CharT>::buffer_size(),
      BinFmt<CharT>::buffer_size()};

  auto result = sizes[0];
  for (size_t i = 1; i < sizes.size(); i++)
    result = cpp::max(result, sizes[i]);
  return result;
}

template <typename CharT>
LIBC_INLINE cpp::optional<cpp::basic_string_view<CharT>>
num_to_strview(uintmax_t num, cpp::span<CharT> bufref, CharT conv_name) {
  switch (conv_name) {
  case CharT{'x'}:
    return HexFmt<CharT>::format_to(bufref, num);
  case CharT{'X'}:
    return HexFmtUppercase<CharT>::format_to(bufref, num);
  case CharT{'o'}:
    return OctFmt<CharT>::format_to(bufref, num);
  case CharT('b'):
  case CharT('B'):
    return BinFmt<CharT>::format_to(bufref, num);
  default:
    return DecFmt<CharT>::format_to(bufref, num);
  }
}

} // namespace details

template <OverflowMode mode, typename CharT>
LIBC_INLINE int convert_int(Writer<mode, CharT> *writer,
                            const FormatSection<CharT> &to_conv) {
  static constexpr size_t BITS_IN_BYTE = 8;
  static constexpr size_t BITS_IN_NUM = sizeof(uintmax_t) * BITS_IN_BYTE;

  uintmax_t num = static_cast<uintmax_t>(to_conv.conv_val_raw);
  bool is_negative = false;
  FormatFlags flags = to_conv.flags;

  // If the conversion is signed, then handle negative values.
  if (to_conv.conv_name == CharT{'d'} || to_conv.conv_name == CharT{'i'}) {
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
  cpp::array<CharT, details::num_buf_size<CharT>()> buf;
  auto str = details::num_to_strview<CharT>(num, buf, to_conv.conv_name);
  if (!str)
    return INT_CONVERSION_ERROR;

  size_t digits_written = str->size();

  CharT sign_char = 0;

  if (is_negative)
    sign_char = CharT{'-'};
  else if ((flags & FormatFlags::FORCE_SIGN) == FormatFlags::FORCE_SIGN)
    sign_char = CharT{'+'}; // FORCE_SIGN has precedence over SPACE_PREFIX
  else if ((flags & FormatFlags::SPACE_PREFIX) == FormatFlags::SPACE_PREFIX)
    sign_char = CharT{' '};

  // These are signed to prevent underflow due to negative values. The eventual
  // values will always be non-negative.
  int zeroes;
  int spaces;

  // Prefix is "0x" or "OX" for hexadecimal, "0b" or "0B" for binary, or the
  // sign character for signed conversions. Since hexadecimal and binary are
  // unsigned these will never conflict.
  size_t prefix_len;
  CharT prefix[2];
  if ((to_conv.conv_name == CharT{'x'} || to_conv.conv_name == CharT{'X'} ||
       to_conv.conv_name == CharT{'b'} || to_conv.conv_name == CharT{'B'}) &&
      (flags & FormatFlags::ALTERNATE_FORM) != 0 && num != 0) {
    prefix_len = 2;
    prefix[0] = CharT{'0'};
    prefix[1] = to_conv.conv_name;
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
  if ((to_conv.conv_name == CharT{'o'}) &&
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
      RET_IF_RESULT_NEGATIVE(writer->write(CharT{'0'}, zeroes));
    if (digits_written > 0)
      RET_IF_RESULT_NEGATIVE(writer->write(*str));
    if (spaces > 0)
      RET_IF_RESULT_NEGATIVE(writer->write(CharT{' '}, spaces));
  } else {
    // Else it goes spaces prefix zeroes digits
    if (spaces > 0)
      RET_IF_RESULT_NEGATIVE(writer->write(CharT{' '}, spaces));
    if (prefix_len != 0)
      RET_IF_RESULT_NEGATIVE(writer->write({prefix, prefix_len}));
    if (zeroes > 0)
      RET_IF_RESULT_NEGATIVE(writer->write(CharT{'0'}, zeroes));
    if (digits_written > 0)
      RET_IF_RESULT_NEGATIVE(writer->write(*str));
  }
  return WRITE_OK;
}

} // namespace printf_core
} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC___SUPPORT_PRINTF_CORE_INT_CONVERTER_H
