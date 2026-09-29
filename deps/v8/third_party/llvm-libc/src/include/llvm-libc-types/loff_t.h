//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// Definition of loff_t type.
///
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_TYPES_LOFF_T_H
#define LLVM_LIBC_TYPES_LOFF_T_H

#if defined(__linux__)
#include "linux/loff_t.h"
#endif

#endif // LLVM_LIBC_TYPES_LOFF_T_H
