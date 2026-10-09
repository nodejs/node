//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// Definition of macros from threads.h.
///
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_MACROS_THREADS_MACROS_H
#define LLVM_LIBC_MACROS_THREADS_MACROS_H

#include "common-threads-macros.h"

/// Maximum number of destructor iterations for thread-specific storage.
#define TSS_DTOR_ITERATIONS __LLVM_LIBC_TSS_DTOR_ITERATIONS

#endif // LLVM_LIBC_MACROS_THREADS_MACROS_H
