//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// Definitions internet address macros.
///
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_HDR_INET_ADDRESS_MACROS_H
#define LLVM_LIBC_HDR_INET_ADDRESS_MACROS_H

#ifdef LIBC_FULL_BUILD

#include "include/llvm-libc-macros/inet-address-macros.h"

#else // Overlay mode

#include <netinet/in.h>

#endif // LIBC_FULL_BUILD

#endif // LLVM_LIBC_HDR_INET_ADDRESS_MACROS_H
