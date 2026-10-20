//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// Syscall wrapper for mknodat.
///
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC___SUPPORT_OSUTIL_SYSCALL_WRAPPERS_MKNODAT_H
#define LLVM_LIBC_SRC___SUPPORT_OSUTIL_SYSCALL_WRAPPERS_MKNODAT_H

#include "hdr/types/dev_t.h"
#include "hdr/types/mode_t.h"
#include "src/__support/OSUtil/linux/syscall.h" // For syscall_checked
#include "src/__support/common.h"
#include "src/__support/error_or.h"
#include "src/__support/macros/config.h"
#include <sys/syscall.h> // For syscall numbers

namespace LIBC_NAMESPACE_DECL {
namespace linux_syscalls {

// Note that the kernel expects |dev| in its own old-style encoding
// ((major << 8) | minor, with the extended bits), which is not necessarily the
// encoding of dev_t. Callers creating device nodes must encode it themselves;
// callers creating FIFOs and sockets pass 0.
LIBC_INLINE ErrorOr<int> mknodat(int dfd, const char *path, mode_t mode,
                                 dev_t dev) {
  return syscall_checked<int>(SYS_mknodat, dfd, path, mode, dev);
}

} // namespace linux_syscalls
} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC___SUPPORT_OSUTIL_SYSCALL_WRAPPERS_MKNODAT_H
