//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// Result type for directory scan operations.
///
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC___SUPPORT_FILE_DIR_SCAN_RESULT_H
#define LLVM_LIBC_SRC___SUPPORT_FILE_DIR_SCAN_RESULT_H

#include "hdr/types/struct_dirent.h"
#include "src/__support/macros/attributes.h"
#include "src/__support/macros/config.h"

#include <stddef.h>

namespace LIBC_NAMESPACE_DECL {

/// Holds the dynamically allocated array of directory entries and their count.
/// Caller assumes ownership of both the entry array and individual dirents.
struct DirScanResult {
  struct dirent **entries = nullptr;
  size_t count = 0;

  LIBC_INLINE constexpr DirScanResult() = default;
  LIBC_INLINE constexpr DirScanResult(struct dirent **e, size_t c)
      : entries(e), count(c) {}
};

} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC___SUPPORT_FILE_DIR_SCAN_RESULT_H
