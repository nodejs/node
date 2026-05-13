//===-- Definition of modular format macro for printf ---------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_MACROS_LIBC_MODULAR_FORMAT_PRINTF_H
#define LLVM_LIBC_MACROS_LIBC_MODULAR_FORMAT_PRINTF_H

#define _LIBC_MODULAR_FORMAT_PRINTF(MODULAR_IMPL_FN)                           \
  __attribute__((modular_format(MODULAR_IMPL_FN, "__printf", "float")))

#endif // LLVM_LIBC_MACROS_LIBC_MODULAR_FORMAT_PRINTF_H
