//===-- Definition of EFI_TPL type ----------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_TYPES_EFI_TPL_H
#define LLVM_LIBC_TYPES_EFI_TPL_H

#include "size_t.h"

typedef size_t EFI_TPL;

#define TPL_APPLICATION 4
#define TPL_CALLBACK 8
#define TPL_NOTIFY 16
#define TPL_HIGH_LEVEL 31

#endif // LLVM_LIBC_TYPES_EFI_TPL_H
