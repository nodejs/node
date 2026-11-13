//===-- Implementation header for strxfrm_l ---------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC_STRING_STRXFRM_L_H
#define LLVM_LIBC_SRC_STRING_STRXFRM_L_H

#include "include/llvm-libc-types/locale_t.h"
#include "src/__support/macros/config.h"
#include <stddef.h> // For size_t

namespace LIBC_NAMESPACE_DECL {

size_t strxfrm_l(char *__restrict dest, const char *__restrict src, size_t n,
                 locale_t locale);

} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC_STRING_STRXFRM_L_H
