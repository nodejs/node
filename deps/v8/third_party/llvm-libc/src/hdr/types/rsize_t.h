//===-- Proxy for rsize_t -------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
#ifndef LLVM_LIBC_HDR_TYPES_RSIZE_T_H
#define LLVM_LIBC_HDR_TYPES_RSIZE_T_H

#ifndef __STDC_WANT_LIB_EXT1__
#define __STDC_WANT_LIB_EXT1__ 1
#endif

#ifdef LIBC_FULL_BUILD

#include "include/llvm-libc-types/rsize_t.h"

#else

#define __need_rsize_t
#include <stddef.h>
#undef __need_rsize_t

#endif // LIBC_FULL_BUILD

#ifdef __STDC_WANT_LIB_EXT1__
#undef __STDC_WANT_LIB_EXT1__
#endif

#endif // LLVM_LIBC_HDR_TYPES_RSIZE_T_H
