//===-- Proxy for size_t --------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
#ifndef LLVM_LIBC_HDR_TYPES_SIZE_T_H
#define LLVM_LIBC_HDR_TYPES_SIZE_T_H

#ifdef LIBC_FULL_BUILD

#include "include/llvm-libc-types/size_t.h"

#else

#define __need_size_t
#include <stddef.h>
#undef __need_size_t

#endif // LIBC_FULL_BUILD

#endif // LLVM_LIBC_HDR_TYPES_SIZE_T_H
