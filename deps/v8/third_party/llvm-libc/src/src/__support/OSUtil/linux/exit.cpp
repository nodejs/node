//===------------ Linux implementation of an exit function ------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "src/__support/OSUtil/linux/syscall_wrappers/exit_group.h"
#include "src/__support/common.h"
#include "src/__support/macros/config.h"

namespace LIBC_NAMESPACE_DECL {
namespace internal {

// mark as no_stack_protector for x86 since TLS can be torn down before calling
// exit so that the stack protector canary cannot be loaded.
#ifdef LIBC_TARGET_ARCH_IS_X86
__attribute__((no_stack_protector))
#endif
__attribute__((noreturn)) void
exit(int status) {
  linux_syscalls::exit_group(status);
}

} // namespace internal
} // namespace LIBC_NAMESPACE_DECL
