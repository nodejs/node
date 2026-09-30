//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// Defines OS-level Thread Control Block dispatcher.
///
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC___SUPPORT_THREADS_TCB_H
#define LLVM_LIBC_SRC___SUPPORT_THREADS_TCB_H

#include "src/__support/macros/properties/os.h"

#if defined(LIBC_TARGET_OS_IS_LINUX)
#include "src/__support/threads/linux/tcb.h"
#else
#error "Unsupported OS for Thread Control Block"
#endif

#endif // LLVM_LIBC_SRC___SUPPORT_THREADS_TCB_H
