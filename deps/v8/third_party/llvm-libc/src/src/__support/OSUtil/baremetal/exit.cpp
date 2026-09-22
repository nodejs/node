//===-------- Baremetal implementation of an exit function ------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "src/__support/OSUtil/exit.h"
#include "src/__support/macros/config.h"

namespace LIBC_NAMESPACE_DECL {
namespace internal {

// This is intended to be provided by the vendor.
extern "C" [[noreturn]] void __llvm_libc_exit(int status);

[[noreturn]] void exit(int status) { __llvm_libc_exit(status); }

} // namespace internal
} // namespace LIBC_NAMESPACE_DECL
