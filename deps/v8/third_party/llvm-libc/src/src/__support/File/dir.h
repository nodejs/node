//===--- A platform independent Dir class ---------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC___SUPPORT_FILE_DIR_H
#define LLVM_LIBC_SRC___SUPPORT_FILE_DIR_H

#include "src/__support/CPP/span.h"
#include "src/__support/error_or.h"
#include "src/__support/macros/config.h"
#include "src/__support/threads/mutex.h"

#include <dirent.h>

namespace LIBC_NAMESPACE_DECL {

// Platform specific function which will open the directory |name|
// and return its file descriptor. Upon failure, the error value is returned.
ErrorOr<int> platform_opendir(const char *name);

// Platform specific function which will close the directory with
// file descriptor |fd|. Returns 0 on success, or the error number on failure.
int platform_closedir(int fd);

// Platform specific function which will fetch dirents in to buffer.
// Returns the number of bytes written into buffer or the error number on
// failure.
ErrorOr<size_t> platform_fetch_dirents(int fd, cpp::span<uint8_t> buffer);

// This class is designed to allow implementation of the POSIX dirent.h API.
// By itself, it is platform independent but calls platform specific
// functions to perform OS operations.
class Dir {
  static constexpr size_t BUFSIZE = 1024;
  int fd;
  size_t readptr = 0;  // The current read pointer.
  size_t fillsize = 0; // The number of valid bytes availabe in the buffer.

  // This is a buffer of struct dirent values which will be fetched
  // from the OS. Since the d_name of struct dirent can be of a variable
  // size, we store the data in a byte array.
  uint8_t buffer[BUFSIZE];

  Mutex mutex;

  // A directory is to be opened by the static method open and closed
  // by the close method. So, all constructors and destructor are declared
  // as private. Inappropriate constructors are declared as deleted.
  LIBC_INLINE Dir() = delete;
  LIBC_INLINE Dir(const Dir &) = delete;

  LIBC_INLINE explicit Dir(int fdesc)
      : fd(fdesc), readptr(0), fillsize(0),
        mutex(/*timed=*/false, /*recursive=*/false, /*robust=*/false,
              /*pshared=*/false) {}
  LIBC_INLINE ~Dir() = default;

  LIBC_INLINE Dir &operator=(const Dir &) = delete;

public:
  static ErrorOr<Dir *> open(const char *path);

  ErrorOr<struct ::dirent *> read();

  // Returns 0 on success or the error number on failure. If an error number
  // was returned, then the resources associated with the directory are not
  // cleaned up.
  int close();

  LIBC_INLINE int getfd() { return fd; }
};

} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC___SUPPORT_FILE_DIR_H
