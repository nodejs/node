//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// Syscall wrapper for ioctl.
///
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC___SUPPORT_OSUTIL_SYSCALL_WRAPPERS_IOCTL_H
#define LLVM_LIBC_SRC___SUPPORT_OSUTIL_SYSCALL_WRAPPERS_IOCTL_H

#include "src/__support/CPP/type_traits/enable_if.h"
#include "src/__support/CPP/type_traits/is_integral.h"
#include "src/__support/CPP/type_traits/is_null_pointer.h"
#include "src/__support/CPP/type_traits/is_pointer.h"
#include "src/__support/OSUtil/linux/syscall.h" // syscall_checked
#include "src/__support/error_or.h"
#include "src/__support/macros/attributes.h" // LIBC_INLINE
#include "src/__support/macros/config.h"
#include <sys/syscall.h> // For syscall numbers

namespace LIBC_NAMESPACE_DECL {
namespace linux_syscalls {

struct IoctlArg {
  unsigned long val;

  // Some ioctls read the argument, some write to it. The caller is responsible
  // for passing the correct pointer type.
  LIBC_INLINE IoctlArg(const void *ptr)
      : val(reinterpret_cast<unsigned long>(ptr)) {}
  LIBC_INLINE IoctlArg(void *ptr) : val(reinterpret_cast<unsigned long>(ptr)) {}

  template <typename T, cpp::enable_if_t<cpp::is_integral_v<T>, int> = 0>
  LIBC_INLINE constexpr IoctlArg(T num = 0)
      : val(static_cast<unsigned long>(num)) {}
};

LIBC_INLINE ErrorOr<int> ioctl(int fd, unsigned long request,
                               IoctlArg arg = 0) {
  return syscall_checked<int>(SYS_ioctl, fd, request, arg.val);
}

} // namespace linux_syscalls
} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC___SUPPORT_OSUTIL_SYSCALL_WRAPPERS_IOCTL_H
