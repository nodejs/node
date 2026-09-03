//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// Implementation of the platform independent Dir class.
///
//===----------------------------------------------------------------------===//

#include "src/__support/File/dir.h"

#include "hdr/errno_macros.h"
#include "src/__support/CPP/mutex.h" // lock_guard
#include "src/__support/CPP/new.h"
#include "src/__support/alloc-checker.h"
#include "src/__support/error_or.h"
#include "src/__support/macros/config.h"

namespace LIBC_NAMESPACE_DECL {

ErrorOr<Dir *> Dir::fdopen(int fd) {
  LIBC_NAMESPACE::AllocChecker ac;
  Dir *dir = new (ac) Dir(fd);
  if (!ac)
    return LIBC_NAMESPACE::Error(ENOMEM);
  return dir;
}

ErrorOr<Dir *> Dir::open(const char *path) {
  auto fd = platform_opendir(path);
  if (!fd)
    return LIBC_NAMESPACE::Error(fd.error());

  return Dir::fdopen(fd.value());
}

ErrorOr<struct dirent *> Dir::read() {
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

  cpp::span<uint8_t> buf_span(buffer, BUFSIZE);

  if (fillsize - readptr < sizeof(struct dirent))
    return Error(EIO);

  struct dirent *d =
      reinterpret_cast<struct dirent *>(buf_span.subspan(readptr).data());

  size_t reclen = platform_dir_reclen(d);

  if (reclen == 0 || readptr + reclen > fillsize)
    return Error(EIO);

  readptr += reclen;
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
