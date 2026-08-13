//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// Proxy for struct if_nameindex.
///
//===----------------------------------------------------------------------===//
#ifndef LLVM_LIBC_HDR_TYPES_STRUCT_IF_NAMEINDEX_H
#define LLVM_LIBC_HDR_TYPES_STRUCT_IF_NAMEINDEX_H

#ifdef LIBC_FULL_BUILD

#include "include/llvm-libc-types/struct_if_nameindex.h"

#else

#include <net/if.h>

#endif // LIBC_FULL_BUILD

#endif // LLVM_LIBC_HDR_TYPES_STRUCT_IF_NAMEINDEX_H
