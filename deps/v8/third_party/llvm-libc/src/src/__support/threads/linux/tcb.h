//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// Defines Linux architecture-level Thread Control Block dispatcher.
///
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC___SUPPORT_THREADS_LINUX_TCB_H
#define LLVM_LIBC_SRC___SUPPORT_THREADS_LINUX_TCB_H

#include "src/__support/macros/properties/architectures.h"

#if defined(LIBC_TARGET_ARCH_IS_X86_64)
#include "src/__support/threads/linux/x86_64/tcb.h"
#elif defined(LIBC_TARGET_ARCH_IS_AARCH64)
#include "src/__support/threads/linux/aarch64/tcb.h"
#elif defined(LIBC_TARGET_ARCH_IS_ANY_RISCV)
#include "src/__support/threads/linux/riscv/tcb.h"
#else
#error "Unsupported architecture for Linux Thread Control Block"
#endif

#endif // LLVM_LIBC_SRC___SUPPORT_THREADS_LINUX_TCB_H
