//===-- Proxy for struct dl_phdr_info  -----------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===---------------------------------------------------------------------===//
#ifndef LLVM_LIBC_HDR_TYPES_STRUCT_DL_PHDR_INFO_H
#define LLVM_LIBC_HDR_TYPES_STRUCT_DL_PHDR_INFO_H

#ifdef LIBC_FULL_BUILD

#include "include/llvm-libc-types/struct_dl_phdr_info.h"

#else

#include <link.h>

#endif // LIBC_FULL_BUILD

#endif // LLVM_LIBC_HDR_TYPES_STRUCT_DL_PHDR_INFO_H
