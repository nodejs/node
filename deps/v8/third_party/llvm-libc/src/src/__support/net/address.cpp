//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// This file implements helper functions for parsing and formatting network
/// addresses.
///
//===----------------------------------------------------------------------===//

#include "src/__support/net/address.h"
#include "hdr/inet-address-macros.h"
#include "hdr/types/in_addr_t.h"
#include "hdr/types/struct_in6_addr.h"
#include "hdr/types/struct_in_addr.h"
#include "src/__support/common.h"
#include "src/__support/ctype_utils.h"
#include "src/__support/endian_internal.h"
#include "src/__support/libc_assert.h"
#include "src/__support/str_to_integer.h"
#include "src/string/memory_utils/inline_memcpy.h"

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

namespace {

size_t ipv4_num_bytes(cpp::span<const uint8_t> src) {
  size_t result = 8; // four digits, three dots and '\0'
  for (uint8_t val : src)
    result += (val >= 10) + (val >= 100);
  return result;
}

size_t ipv4_to_str_unchecked(cpp::span<const uint8_t> src,
                             cpp::span<char> dst) {
  size_t pos = 0;
  for (unsigned i = 0; i < 4; ++i) {
    uint8_t val = src[i];
    if (val >= 100) {
      uint8_t cent = val / 100;
      uint8_t rem = val % 100;
      dst[pos++] = internal::int_to_b36_char(cent);
      dst[pos++] = internal::int_to_b36_char(rem / 10);
      dst[pos++] = internal::int_to_b36_char(rem % 10);
    } else if (val >= 10) {
      dst[pos++] = internal::int_to_b36_char(val / 10);
      dst[pos++] = internal::int_to_b36_char(val % 10);
    } else {
      dst[pos++] = internal::int_to_b36_char(val);
    }
    dst[pos++] = i < 3 ? '.' : '\0';
  }
  return pos;
}

size_t ipv6_to_str_unchecked(const struct in6_addr &src, cpp::span<char> dst) {
  // Find the longest run of zeroes to compress to "::"
  struct Run {
    unsigned start = 0;
    unsigned len = 0;
  };
  Run best, current;
  for (unsigned i = 0; i < 8; ++i) {
    uint16_t val = src.s6_addr16[i];
    if (val == 0) {
      ++current.len;
    } else {
      // In case of ties, the first sequence wins.
      if (current.len > best.len)
        best = current;
      current = {i + 1, 0};
    }
  }
  if (current.len > best.len)
    best = current;

  bool is_mapped =
      best.start == 0 &&
      (best.len == 6 || (best.len == 5 && src.s6_addr16[5] == 0xffff));
  unsigned num_words = is_mapped ? 6 : 8;

  size_t pos = 0;
  auto append_word = [&](unsigned i) {
    uint16_t word = Endian::from_big_endian(src.s6_addr16[i]);
    // This isn't using int_to_b36_char because it's large intermediate
    // representation prevents append_word from being inlined.
    static constexpr char DIGITS[] = "0123456789abcdef";
    if (word >= 0x1000) {
      dst[pos] = DIGITS[word >> 12];
      dst[pos + 1] = DIGITS[(word >> 8) & 0xf];
      dst[pos + 2] = DIGITS[(word >> 4) & 0xf];
      dst[pos + 3] = DIGITS[word & 0xf];
      pos += 4;
    } else if (word >= 0x100) {
      dst[pos] = DIGITS[word >> 8];
      dst[pos + 1] = DIGITS[(word >> 4) & 0xf];
      dst[pos + 2] = DIGITS[word & 0xf];
      pos += 3;
    } else if (word >= 0x10) {
      dst[pos] = DIGITS[(word >> 4) & 0xf];
      dst[pos + 1] = DIGITS[word & 0xf];
      pos += 2;
    } else {
      dst[pos] = DIGITS[word];
      pos += 1;
    }
  };

  if (best.len < 2) {
    // No compression
    for (unsigned i = 0; i < 7; ++i) {
      append_word(i);
      dst[pos++] = ':';
    }
    append_word(7);
    dst[pos++] = '\0';
    return pos;
  }

  // Left part
  for (unsigned i = 0; i < best.start; ++i) {
    append_word(i);
    dst[pos++] = ':';
  }
  // Compressed part
  if (best.start == 0)
    dst[pos++] = ':';
  dst[pos++] = ':';

  // Right part (if it exists)
  if (best.start + best.len < num_words) {
    unsigned end = num_words - 1;
    for (unsigned i = best.start + best.len; i < end; ++i) {
      append_word(i);
      dst[pos++] = ':';
    }
    append_word(end);
    if (num_words == 6)
      dst[pos++] = ':';
  }

  if (is_mapped) {
    cpp::span<const uint8_t> ipv4_part(src.s6_addr + 12, 4);
    pos += ipv4_to_str_unchecked(ipv4_part, dst.subspan(pos));
  } else {
    dst[pos++] = '\0';
  }

  return pos;
}

} // anonymous namespace

bool ipv4_to_str(const struct in_addr &src, cpp::span<char> dst) {
  cpp::span<const uint8_t> addr(reinterpret_cast<const uint8_t *>(&src), 4);

  if (dst.size() < INET_ADDRSTRLEN) {
    if (dst.size() < ipv4_num_bytes(addr))
      return false;
  }

  ipv4_to_str_unchecked(addr, dst);
  return true;
}

bool ipv6_to_str(const struct in6_addr &src, cpp::span<char> dst) {
  if (dst.size() >= INET6_ADDRSTRLEN) {
    ipv6_to_str_unchecked(src, dst);
    return true;
  }
  char buf[INET6_ADDRSTRLEN];
  size_t len = ipv6_to_str_unchecked(src, buf);
  LIBC_ASSERT(len < INET6_ADDRSTRLEN);
  if (len > dst.size())
    return false;
  inline_memcpy(dst.data(), buf, len);
  return true;
}

} // namespace net
} // namespace LIBC_NAMESPACE_DECL
