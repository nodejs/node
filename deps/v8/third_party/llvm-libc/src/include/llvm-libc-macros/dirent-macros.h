//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// Definition of macros from dirent.h.
///
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_MACROS_DIRENT_MACROS_H
#define LLVM_LIBC_MACROS_DIRENT_MACROS_H

#ifdef __linux__
#include "linux/dirent-macros.h"
#endif

#endif // LLVM_LIBC_MACROS_DIRENT_MACROS_H
