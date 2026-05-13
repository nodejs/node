//===-- Definition of macros from locale_t.h ------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_HDR_TYPES_LOCALE_T_H
#define LLVM_LIBC_HDR_TYPES_LOCALE_T_H

#ifdef LIBC_FULL_BUILD

#include "include/llvm-libc-types/locale_t.h"

#else // overlay mode

#error "type not available in overlay mode"

#endif // LLVM_LIBC_FULL_BUILD

#endif // LLVM_LIBC_HDR_TYPES_LOCALE_T_H
