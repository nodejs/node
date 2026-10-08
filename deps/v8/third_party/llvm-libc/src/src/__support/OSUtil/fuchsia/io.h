//===------------- Fuchsia implementation of IO utils -----------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC___SUPPORT_OSUTIL_FUCHSIA_IO_H
#define LLVM_LIBC_SRC___SUPPORT_OSUTIL_FUCHSIA_IO_H

#include "src/__support/CPP/string_view.h"
#include "src/__support/macros/config.h"

#include <iostream>
#include <zircon/sanitizer.h>

namespace LIBC_NAMESPACE_DECL {

LIBC_INLINE void write_to_stderr(cpp::string_view msg) {
#if defined(LIBC_COPT_TEST_USE_ZXTEST)
  // This is used in standalone context where there is nothing like POSIX I/O.
  __sanitizer_log_write(msg.data(), msg.size());
#elif defined(LIBC_COPT_TEST_USE_GTEST)
  // The gtest framework already relies on full standard C++ I/O via fdio.
  std::cerr << std::string_view{msg.data(), msg.size()};
#else
#error this file should only be used by tests
#endif
}

} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC___SUPPORT_OSUTIL_FUCHSIA_IO_H
