//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// Proxy header for struct in6_addr.
///
//===----------------------------------------------------------------------===//
#ifndef LLVM_LIBC_HDR_TYPES_STRUCT_IN6_ADDR_H
#define LLVM_LIBC_HDR_TYPES_STRUCT_IN6_ADDR_H

#ifdef LIBC_FULL_BUILD

#include "include/llvm-libc-types/struct_in6_addr.h"

#else

#include <netinet/in.h>

#endif // LIBC_FULL_BUILD

#endif // LLVM_LIBC_HDR_TYPES_STRUCT_IN6_ADDR_H
