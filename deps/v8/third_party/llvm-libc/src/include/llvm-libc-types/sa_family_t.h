//===-- Definition of sa_family_t type ------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_TYPES_SA_FAMILY_T_H
#define LLVM_LIBC_TYPES_SA_FAMILY_T_H

// The posix standard only says of sa_family_t that it must be unsigned. The
// linux man page for "address_families" lists approximately 32 different
// address families, meaning that a short 16 bit number will have plenty of
// space for all of them.

typedef unsigned short sa_family_t;

#endif // LLVM_LIBC_TYPES_SA_FAMILY_T_H
