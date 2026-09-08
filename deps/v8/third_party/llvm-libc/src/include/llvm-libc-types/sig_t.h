//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// Definition of sig_t type.
///
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_TYPES_SIG_T_H
#define LLVM_LIBC_TYPES_SIG_T_H

// BSD type for signal handlers.
typedef void (*sig_t)(int);

#endif // LLVM_LIBC_TYPES_SIG_T_H
