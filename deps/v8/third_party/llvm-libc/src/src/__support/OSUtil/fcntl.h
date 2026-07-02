//===-- Implementation header of internal fcntl function ------------------===//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC___SUPPORT_OSUTIL_FCNTL_H
#define LLVM_LIBC_SRC___SUPPORT_OSUTIL_FCNTL_H

#include "hdr/types/mode_t.h"
#include "src/__support/error_or.h"
#include "src/__support/macros/config.h"

namespace LIBC_NAMESPACE_DECL {
namespace internal {

ErrorOr<int> fcntl(int fd, int cmd, void *arg = nullptr);

ErrorOr<int> open(const char *path, int flags, mode_t mode_flags = 0);

ErrorOr<int> close(int fd);

} // namespace internal
} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC___SUPPORT_OSUTIL_FCNTL_H
