//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// Internal linux path helpers.
///
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC___SUPPORT_OSUTIL_LINUX_PATH_H
#define LLVM_LIBC_SRC___SUPPORT_OSUTIL_LINUX_PATH_H

#include "src/__support/CPP/string_view.h"
#include "src/__support/macros/config.h"

namespace LIBC_NAMESPACE_DECL {
namespace path {

// Separator character for linux paths.
constexpr char SEPARATOR = '/';

// Component pointing to the current directory.
constexpr cpp::string_view CURRENT_DIR_COMPONENT = ".";

// Component pointing to the parent directory.
constexpr cpp::string_view PARENT_DIR_COMPONENT = "..";

// Whether `path` is absolute.
LIBC_INLINE constexpr bool is_absolute(cpp::string_view path) {
  return path.starts_with(SEPARATOR);
}

// Whether `path` is absolute.
LIBC_INLINE constexpr bool is_root(cpp::string_view path) {
  return path.size() == 1 && path[0] == SEPARATOR;
}

} // namespace path
} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC___SUPPORT_OSUTIL_LINUX_PATH_H
