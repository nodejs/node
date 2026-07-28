//===-- Character Converter for printf --------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC___SUPPORT_PRINTF_CORE_CHAR_CONVERTER_H
#define LLVM_LIBC_SRC___SUPPORT_PRINTF_CORE_CHAR_CONVERTER_H

#ifndef LIBC_COPT_PRINTF_DISABLE_WIDE
#include "hdr/types/wchar_t.h"
#include "hdr/types/wint_t.h"
#include "hdr/wchar_macros.h"
#include "src/__support/wchar/mbstate.h"
#include "src/__support/wchar/wcrtomb.h"
#endif // LIBC_COPT_PRINTF_DISABLE_WIDE

#include "hdr/limits_macros.h"
#include "src/__support/macros/config.h"
#include "src/__support/printf_core/converter_utils.h"
#include "src/__support/printf_core/core_structs.h"
#include "src/__support/printf_core/writer.h"

namespace LIBC_NAMESPACE_DECL {
namespace printf_core {

template <WriteMode write_mode>
LIBC_INLINE int convert_char(Writer<write_mode> *writer,
                             const FormatSection &to_conv) {

  char buffer[MB_LEN_MAX];
  size_t write_size = 0;

  if (to_conv.length_modifier == LengthModifier::l) {
#ifndef LIBC_COPT_PRINTF_DISABLE_WIDE
    wint_t wi = static_cast<wint_t>(to_conv.conv_val_raw);

    if (wi == WEOF) {
      return ILLEGAL_WIDE_CHAR;
    }

    internal::mbstate mbstate;
    wchar_t wc = static_cast<wchar_t>(wi);
    auto ret = internal::wcrtomb(buffer, wc, &mbstate);

    if (!ret.has_value()) {
      return MB_CONVERSION_ERROR;
    }

    write_size = ret.value();
#else
    // If wide characters are disabled, treat the 'l' modifier as a no-op.
    buffer[0] = static_cast<char>(to_conv.conv_val_raw);
    write_size = 1;

#endif // LIBC_COPT_PRINTF_DISABLE_WIDE
  } else {
    buffer[0] = static_cast<char>(to_conv.conv_val_raw);
    write_size = 1;
  }

  size_t padding_spaces = to_conv.min_width > static_cast<int>(write_size)
                              ? to_conv.min_width - static_cast<int>(write_size)
                              : 0;

  // If the padding is on the left side, write the spaces first.
  if (padding_spaces > 0 &&
      (to_conv.flags & FormatFlags::LEFT_JUSTIFIED) == 0) {
    RET_IF_RESULT_NEGATIVE(writer->write(' ', padding_spaces));
  }

  RET_IF_RESULT_NEGATIVE(writer->write({buffer, write_size}));

  // If the padding is on the right side, write the spaces last.
  if (padding_spaces > 0 &&
      (to_conv.flags & FormatFlags::LEFT_JUSTIFIED) != 0) {
    RET_IF_RESULT_NEGATIVE(writer->write(' ', padding_spaces));
  }

  return WRITE_OK;
}

} // namespace printf_core
} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC___SUPPORT_PRINTF_CORE_CHAR_CONVERTER_H
