//===---------- UEFI implementation of IO utils ------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===-----------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC___SUPPORT_OSUTIL_UEFI_IO_H
#define LLVM_LIBC_SRC___SUPPORT_OSUTIL_UEFI_IO_H

#include "include/llvm-libc-types/size_t.h"
#include "include/llvm-libc-types/ssize_t.h"
#include "src/__support/CPP/string_view.h"
#include "src/__support/macros/config.h"

namespace LIBC_NAMESPACE_DECL {

ssize_t read_from_stdin(char *buf, size_t size);
void write_to_stderr(cpp::string_view msg);
void write_to_stdout(cpp::string_view msg);

} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC___SUPPORT_OSUTIL_UEFI_IO_H
