//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// Proxy header for struct addrinfo.
///
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_HDR_TYPES_STRUCT_ADDRINFO_H
#define LLVM_LIBC_HDR_TYPES_STRUCT_ADDRINFO_H

#ifdef LIBC_FULL_BUILD

#include "include/llvm-libc-types/struct_addrinfo.h"

#else

#include <netdb.h>

#endif // LIBC_FULL_BUILD

#endif // LLVM_LIBC_HDR_TYPES_STRUCT_ADDRINFO_H
