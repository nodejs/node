//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// This file contains helper functions for parsing and manipulating network
/// addresses.
///
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC___SUPPORT_NET_ADDRESS_H
#define LLVM_LIBC_SRC___SUPPORT_NET_ADDRESS_H

#include "hdr/types/struct_in6_addr.h"
#include "hdr/types/struct_in_addr.h"
#include "src/__support/CPP/optional.h"
#include "src/__support/CPP/span.h"
#include "src/__support/macros/config.h"

namespace LIBC_NAMESPACE_DECL {
namespace net {

cpp::optional<in_addr_t> inet_addr(const char *cp);

/// Writes a string representation (including the terminating \0) of the
/// provided address into the destination buffer. In case of error, returns
/// false and does not modify the buffer.
[[nodiscard]] bool ipv4_to_str(const struct in_addr &src, cpp::span<char> dst);

/// Writes a string representation (including the terminating \0) of the
/// provided address into the destination buffer. In case of error, returns
/// false and does not modify the buffer.
[[nodiscard]] bool ipv6_to_str(const struct in6_addr &src, cpp::span<char> dst);

} // namespace net
} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC___SUPPORT_NET_ADDRESS_H
