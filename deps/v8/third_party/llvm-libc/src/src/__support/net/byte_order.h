//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// Helpers for converting between host and network byte order.
///
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC___SUPPORT_NET_BYTE_ORDER_H
#define LLVM_LIBC_SRC___SUPPORT_NET_BYTE_ORDER_H

#include "src/__support/common.h"
#include "src/__support/endian_internal.h"
#include "src/__support/macros/config.h"

namespace LIBC_NAMESPACE_DECL {
namespace net {

template <typename T> LIBC_INLINE constexpr T hton(T v) {
  return Endian::to_big_endian(v);
}

template <typename T> LIBC_INLINE constexpr T ntoh(T v) {
  return Endian::from_big_endian(v);
}

} // namespace net
} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC___SUPPORT_NET_BYTE_ORDER_H
