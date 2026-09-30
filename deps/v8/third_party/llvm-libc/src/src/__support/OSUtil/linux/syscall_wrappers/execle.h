//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// Implementation header for execle.
///
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC___SUPPORT_OSUTIL_SYSCALL_WRAPPERS_EXECLE_H
#define LLVM_LIBC_SRC___SUPPORT_OSUTIL_SYSCALL_WRAPPERS_EXECLE_H

#include "src/__support/OSUtil/linux/syscall.h" // For syscall_checked
#include "src/__support/common.h"
#include "src/__support/error_or.h"
#include "src/__support/macros/config.h"
#include <sys/syscall.h> // For syscall numbers

namespace LIBC_NAMESPACE_DECL {
namespace linux_syscalls {

template <typename... Args>
LIBC_INLINE ErrorOr<int> execle(const char *path, Args... args) {
  // All this logic because the standard says the environment pointer goes at
  // the end. It's annoying but it's all compile time so it's not actually a
  // problem.
  const void *all[] = {reinterpret_cast<const void *>(args)...};
  constexpr size_t total = sizeof...(Args);
  static_assert(total >= 2,
                "execle requires at least (arg0, ..., nullptr, envp)");
  char *const *envp =
      static_cast<char *const *>(const_cast<void *>(all[total - 1]));

  const char *argv[total];
  for (size_t i = 0; i < total - 1; ++i)
    argv[i] = reinterpret_cast<const char *>(all[i]);
  argv[total - 1] = nullptr;

  return syscall_checked<int>(SYS_execve, path, argv, envp);
}

} // namespace linux_syscalls
} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC___SUPPORT_OSUTIL_SYSCALL_WRAPPERS_EXECLE_H
