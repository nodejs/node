//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// This file implements helper functions for parsing network addresses.
///
//===----------------------------------------------------------------------===//

#include "src/__support/net/address.h"
#include "src/__support/common.h"
#include "src/__support/ctype_utils.h"
#include "src/__support/endian_internal.h"
#include "src/__support/str_to_integer.h"

namespace LIBC_NAMESPACE_DECL {
namespace net {

cpp::optional<in_addr_t> inet_addr(const char *cp) {
  constexpr int IPV4_MAX_DOT_NUM = 3;
  in_addr_t parts[IPV4_MAX_DOT_NUM + 1] = {0};
  int dot_num = 0;

  for (; dot_num <= IPV4_MAX_DOT_NUM; ++dot_num) {
    // strtointeger skips leading whitespace signs (1.+2.-3. 4), but we don't
    // want that, so we explicitly check that the first character is a digit.
    if (!internal::isdigit(*cp))
      return cpp::nullopt;

    auto result = internal::strtointeger<in_addr_t>(cp, 0);
    parts[dot_num] = result;

    if (result.has_error() || result.parsed_len == 0)
      return cpp::nullopt;
    cp += result.parsed_len;
    if (*cp == '\0' || internal::isspace(*cp))
      break;
    if (*cp != '.')
      return cpp::nullopt;
    ++cp;
  }

  if (dot_num > IPV4_MAX_DOT_NUM)
    return cpp::nullopt;

  // converts the Internet host address cp from the IPv4 numbers-and-dots
  // notation (a[.b[.c[.d]]]) into binary form (in network byte order)
  in_addr_t result = 0;
  for (int i = 0; i <= dot_num; ++i) {
    in_addr_t max_part = i == dot_num ? (0xffffffffu >> (8 * dot_num)) : 0xffu;
    if (parts[i] > max_part)
      return cpp::nullopt;
    int shift = i == dot_num ? 0 : 8 * (IPV4_MAX_DOT_NUM - i);
    result |= parts[i] << shift;
  }

  return Endian::to_big_endian(result);
}

} // namespace net
} // namespace LIBC_NAMESPACE_DECL
