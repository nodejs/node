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

#include "hdr/types/in_addr_t.h"
#include "src/__support/CPP/optional.h"
#include "src/__support/macros/config.h"

namespace LIBC_NAMESPACE_DECL {
namespace net {

cpp::optional<in_addr_t> inet_addr(const char *cp);

} // namespace net
} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC___SUPPORT_NET_ADDRESS_H
