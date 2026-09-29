//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// Proxy header for in_addr_t.
///
//===----------------------------------------------------------------------===//
#ifndef LLVM_LIBC_HDR_TYPES_IN_ADDR_T_H
#define LLVM_LIBC_HDR_TYPES_IN_ADDR_T_H

#ifdef LIBC_FULL_BUILD

#include "include/llvm-libc-types/in_addr_t.h"

#else

#include <netinet/in.h>

#endif // LIBC_FULL_BUILD

#endif // LLVM_LIBC_HDR_TYPES_IN_ADDR_T_H
