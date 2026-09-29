//===-- Strerror Converter for printf ---------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC___SUPPORT_PRINTF_CORE_STRERROR_CONVERTER_H
#define LLVM_LIBC_SRC___SUPPORT_PRINTF_CORE_STRERROR_CONVERTER_H

#include "src/__support/StringUtil/error_to_string.h"
#include "src/__support/macros/config.h"
#include "src/__support/printf_core/core_structs.h"
#include "src/__support/printf_core/int_converter.h"
#include "src/__support/printf_core/string_converter.h"
#include "src/__support/printf_core/writer.h"

namespace LIBC_NAMESPACE_DECL {
namespace printf_core {

template <WriteMode write_mode>
LIBC_INLINE int convert_strerror(Writer<write_mode> *writer,
                                 const FormatSection &to_conv) {
  FormatSection new_conv = to_conv;
  const int error_num = static_cast<int>(to_conv.conv_val_raw);

  // The %m conversion takes no arguments passes the result of strerror(errno)
  // to a string conversion (including all options). If the alternate form flag
  // is set, then if errno is a valid error number the string of the errno macro
  // is passed to a string conversion, else the integer value of errno is passed
  // to an integer conversion.

  // It's assumed that errno is passed in to_conv.conv_val_raw.

  // normal form
  if ((to_conv.flags & FormatFlags::ALTERNATE_FORM) == 0) {
    char strerror_buff[64];
    auto strerror_result = get_error_string(error_num, strerror_buff);
    new_conv.conv_val_ptr =
        reinterpret_cast<void *>(const_cast<char *>(strerror_result.data()));
    new_conv.conv_name = 's';
    return convert_string(writer, new_conv);
  } else {
    // alt form

    // The handling of errno = 0 is in alt form weird. The rule for %m in alt
    // form is "named macros print their name, else print errno as int." There
    // isn't a specific name for errno = 0, but it does have an explicit meaning
    // (success). Due to the way the string mappings work, it's easiest to just
    // say that 0 is a valid macro with a string of "0". This works fine for
    // most cases, but for precision and the int flags it changes the behavior.
    // Given that this behavior is so incredibly deep in the weeds I doubt
    // anyone would notice, I'm going to leave it as the simplest to implement
    // (0 maps to "0"), which also happens to match what other libc
    // implementations have done.

    auto errno_name = try_get_errno_name(error_num);
    // if there's a name available, use it.
    if (errno_name) {
      new_conv.conv_val_ptr =
          reinterpret_cast<void *>(const_cast<char *>(errno_name->data()));
      new_conv.conv_name = 's';
      return convert_string(writer, new_conv);
    } else {
      // else do an int conversion
      new_conv.conv_name = 'd';
      return convert_int(writer, new_conv);
    }
  }
}

} // namespace printf_core
} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC___SUPPORT_PRINTF_CORE_STRERROR_CONVERTER_H
