//===-- Definition of EFIAPI macro ------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===------------------------------------------------------------------===//

#ifndef LLVM_LIBC_MACROS_EFIAPI_MACROS_H
#define LLVM_LIBC_MACROS_EFIAPI_MACROS_H

#if defined(__x86_64__) && !defined(__ILP32__)
#define EFIAPI __attribute__((ms_abi))
#else
#define EFIAPI
#endif

#endif // LLVM_LIBC_MACROS_EFIAPI_MACROS_H
