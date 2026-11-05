//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// Helpers for parsing hexadecimal numbers in network address strings.
///
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC___SUPPORT_NET_HEX_H
#define LLVM_LIBC_SRC___SUPPORT_NET_HEX_H

#include "hdr/types/size_t.h"
#include "src/__support/CPP/optional.h"
#include "src/__support/CPP/string_view.h"
#include "src/__support/CPP/type_traits.h"
#include "src/__support/common.h"
#include "src/__support/ctype_utils.h"
#include "src/__support/macros/config.h"

namespace LIBC_NAMESPACE_DECL {
namespace net {

/// Returns the value of the hexadecimal digit \p ch, or -1 if \p ch is not a
/// hexadecimal digit.
LIBC_INLINE constexpr int hex_digit_value(char ch) {
  if (!internal::isalnum(ch))
    return -1;
  int val = internal::b36_char_to_int(ch);
  return val < 16 ? val : -1;
}

/// Parses a hexadecimal number at the start of \p src and removes the parsed
/// characters from it. At most `2 * sizeof(T)` digits (i.e., as many as can
/// always fit into a T) are consumed. Any following characters (including
/// further hexadecimal digits) are left in \p src for the caller to deal with.
/// Unlike strtointeger, this does not accept leading whitespace, signs or
/// base prefixes. Returns nullopt (leaving \p src unchanged) if \p src does not
/// start with a hexadecimal digit.
template <typename T>
LIBC_INLINE constexpr cpp::optional<T> consume_hex(cpp::string_view &src) {
  static_assert(cpp::is_unsigned_v<T>, "T must be an unsigned integer type");
  constexpr size_t MAX_DIGITS = 2 * sizeof(T);

  T result = 0;
  size_t len = 0;
  for (; len < MAX_DIGITS && len < src.size(); ++len) {
    int digit = hex_digit_value(src[len]);
    if (digit < 0)
      break;
    result = static_cast<T>(result << 4 | digit);
  }
  if (len == 0)
    return cpp::nullopt;
  src.remove_prefix(len);
  return result;
}

} // namespace net
} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC___SUPPORT_NET_HEX_H
