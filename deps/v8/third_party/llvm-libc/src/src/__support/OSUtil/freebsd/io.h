//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// FreeBSD implementation of IO utils.
///
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC___SUPPORT_OSUTIL_FREEBSD_IO_H
#define LLVM_LIBC_SRC___SUPPORT_OSUTIL_FREEBSD_IO_H

#include "src/__support/CPP/string_view.h"
#include "src/__support/OSUtil/freebsd/syscall_wrappers/write.h"
#include "src/__support/macros/config.h"

namespace LIBC_NAMESPACE_DECL {
LIBC_INLINE void write_to_stderr(cpp::string_view msg) {
  freebsd_syscalls::write(2 /* stderr */, msg.data(), msg.size());
}

} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC___SUPPORT_OSUTIL_FREEBSD_IO_H
