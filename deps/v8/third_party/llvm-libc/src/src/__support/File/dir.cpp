//===--- Implementation of a platform independent Dir data structure ------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "dir.h"

#include "src/__support/CPP/mutex.h" // lock_guard
#include "src/__support/CPP/new.h"
#include "src/__support/alloc-checker.h"
#include "src/__support/error_or.h"
#include "src/__support/libc_errno.h" // For error macros
#include "src/__support/macros/config.h"

namespace LIBC_NAMESPACE_DECL {

ErrorOr<Dir *> Dir::open(const char *path) {
  auto fd = platform_opendir(path);
  if (!fd)
    return LIBC_NAMESPACE::Error(fd.error());

  LIBC_NAMESPACE::AllocChecker ac;
  Dir *dir = new (ac) Dir(fd.value());
  if (!ac)
    return LIBC_NAMESPACE::Error(ENOMEM);
  return dir;
}

ErrorOr<struct ::dirent *> Dir::read() {
  cpp::lock_guard lock(mutex);
  if (readptr >= fillsize) {
    auto readsize = platform_fetch_dirents(fd, buffer);
    if (!readsize)
      return LIBC_NAMESPACE::Error(readsize.error());
    fillsize = readsize.value();
    readptr = 0;
  }
  if (fillsize == 0)
    return nullptr;

  struct ::dirent *d = reinterpret_cast<struct ::dirent *>(buffer + readptr);
#ifdef __linux__
  // The d_reclen field is available on Linux but not required by POSIX.
  readptr += d->d_reclen;
#else
  // Other platforms have to implement how the read pointer is to be updated.
#error "DIR read pointer update is missing."
#endif
  return d;
}

int Dir::close() {
  {
    cpp::lock_guard lock(mutex);
    int retval = platform_closedir(fd);
    if (retval != 0)
      return retval;
  }
  delete this;
  return 0;
}

} // namespace LIBC_NAMESPACE_DECL
