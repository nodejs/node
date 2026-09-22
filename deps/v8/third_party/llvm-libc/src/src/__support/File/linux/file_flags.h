//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// This file contains the defualt constants of Linux file open flags.
///
//===----------------------------------------------------------------------===//

#include "hdr/fcntl_macros.h" // For mode_t and other flags to the open syscall
#include "hdr/sys_stat_macros.h" // For S_IS*, S_IF*, and S_IR* flags.
#include "hdr/types/mode_t.h"
#include "src/__support/OSUtil/linux/syscall_wrappers/fcntl.h"
#include "src/__support/macros/attributes.h"
#include "src/__support/macros/config.h"

namespace LIBC_NAMESPACE_DECL {

class LinuxFileFlags {
public:
  static constexpr int CREATE_AND_APPEND = O_CREAT | O_APPEND;
  static constexpr int READ_AND_WRITE = O_RDWR;
  static constexpr int WRITE_ONLY = O_WRONLY;
  static constexpr int READ_ONLY = O_RDONLY;
  static constexpr int CREATE_OR_TRUNCATE = O_CREAT | O_TRUNC;

  // File created will have 0666 permissions.
  LIBC_INLINE static constexpr mode_t OPEN_MODE =
      S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH;

  LIBC_INLINE static constexpr int
  is_file_descriptor_opened_in_read_only(int flag) {
    return (flag & O_ACCMODE) == O_RDONLY;
  }

  LIBC_INLINE static constexpr int
  is_file_descriptor_opened_in_write_only(int flag) {
    return (flag & O_ACCMODE) == O_WRONLY;
  }

  LIBC_INLINE static constexpr int file_has_append_flag(int flag) {
    return flag & O_APPEND;
  }
};
} // namespace LIBC_NAMESPACE_DECL
