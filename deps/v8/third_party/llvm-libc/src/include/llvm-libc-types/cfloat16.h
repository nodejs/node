//===-- Definition of cfloat16 type ---------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_TYPES_CFLOAT16_H
#define LLVM_LIBC_TYPES_CFLOAT16_H

#include "../llvm-libc-macros/cfloat16-macros.h"

#ifdef LIBC_TYPES_HAS_CFLOAT16
typedef _Complex _Float16 cfloat16;
#endif // LIBC_TYPES_HAS_CFLOAT16

#endif // LLVM_LIBC_TYPES_CFLOAT16_H
