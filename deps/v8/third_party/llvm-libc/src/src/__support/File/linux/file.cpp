//===--- Implementation of the Linux specialization of File ---------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "file.h"

#include "hdr/stdio_macros.h"
#include "hdr/types/off_t.h"
#include "src/__support/CPP/new.h"
#include "src/__support/File/file.h"
#include "src/__support/OSUtil/fcntl.h"
#include "src/__support/OSUtil/linux/syscall_wrappers/lseek.h"
#include "src/__support/OSUtil/syscall.h" // For internal syscall function.
#include "src/__support/alloc-checker.h"
#include "src/__support/libc_errno.h" // For error macros
#include "src/__support/macros/config.h"

#include "hdr/fcntl_macros.h" // For mode_t and other flags to the open syscall
#include <sys/stat.h>         // For S_IS*, S_IF*, and S_IR* flags.
#include <sys/syscall.h>      // For syscall numbers

namespace LIBC_NAMESPACE_DECL {

FileIOResult linux_file_write(File *f, const void *data, size_t size) {
  auto *lf = reinterpret_cast<LinuxFile *>(f);
  int ret =
      LIBC_NAMESPACE::syscall_impl<int>(SYS_write, lf->get_fd(), data, size);
  if (ret < 0) {
    return {0, -ret};
  }
  return ret;
}

FileIOResult linux_file_read(File *f, void *buf, size_t size) {
  auto *lf = reinterpret_cast<LinuxFile *>(f);
  int ret =
      LIBC_NAMESPACE::syscall_impl<int>(SYS_read, lf->get_fd(), buf, size);
  if (ret < 0) {
    return {0, -ret};
  }
  return ret;
}

ErrorOr<off_t> linux_file_seek(File *f, off_t offset, int whence) {
  auto *lf = reinterpret_cast<LinuxFile *>(f);
  return linux_syscalls::lseek(lf->get_fd(), offset, whence);
}

int linux_file_close(File *f) {
  File::remove_file(f);
  auto *lf = reinterpret_cast<LinuxFile *>(f);
  int ret = LIBC_NAMESPACE::syscall_impl<int>(SYS_close, lf->get_fd());
  if (ret < 0) {
    return -ret;
  }
  delete lf;
  return 0;
}

ErrorOr<File *> openfile(const char *path, const char *mode) {
  using ModeFlags = File::ModeFlags;
  auto modeflags = File::mode_flags(mode);
  if (modeflags == 0) {
    // return {nullptr, EINVAL};
    return Error(EINVAL);
  }
  long open_flags = 0;
  if (modeflags & ModeFlags(File::OpenMode::APPEND)) {
    open_flags = O_CREAT | O_APPEND;
    if (modeflags & ModeFlags(File::OpenMode::PLUS))
      open_flags |= O_RDWR;
    else
      open_flags |= O_WRONLY;
  } else if (modeflags & ModeFlags(File::OpenMode::WRITE)) {
    open_flags = O_CREAT | O_TRUNC;
    if (modeflags & ModeFlags(File::OpenMode::PLUS))
      open_flags |= O_RDWR;
    else
      open_flags |= O_WRONLY;
  } else {
    if (modeflags & ModeFlags(File::OpenMode::PLUS))
      open_flags |= O_RDWR;
    else
      open_flags |= O_RDONLY;
  }

  // File created will have 0666 permissions.
  constexpr long OPEN_MODE =
      S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH;

#ifdef SYS_open
  int fd =
      LIBC_NAMESPACE::syscall_impl<int>(SYS_open, path, open_flags, OPEN_MODE);
#elif defined(SYS_openat)
  int fd = LIBC_NAMESPACE::syscall_impl<int>(SYS_openat, AT_FDCWD, path,
                                             open_flags, OPEN_MODE);
#else
#error "open and openat syscalls not available."
#endif

  if (fd < 0)
    return Error(-fd);

  uint8_t *buffer;
  {
    AllocChecker ac;
    buffer = new (ac) uint8_t[File::DEFAULT_BUFFER_SIZE];
    if (!ac)
      return Error(ENOMEM);
  }
  AllocChecker ac;
  auto *file = new (ac)
      LinuxFile(fd, buffer, File::DEFAULT_BUFFER_SIZE, _IOFBF, true, modeflags);
  if (!ac)
    return Error(ENOMEM);
  File::add_file(file);
  return file;
}

ErrorOr<LinuxFile *> create_file_from_fd(int fd, const char *mode) {
  using ModeFlags = File::ModeFlags;
  ModeFlags modeflags = File::mode_flags(mode);
  if (modeflags == 0) {
    return Error(EINVAL);
  }

  auto result = internal::fcntl(fd, F_GETFL);
  if (!result.has_value()) {
    return Error(EBADF);
  }
  int fd_flags = result.value();

  using OpenMode = File::OpenMode;
  if (((fd_flags & O_ACCMODE) == O_RDONLY &&
       !(modeflags & static_cast<ModeFlags>(OpenMode::READ))) ||
      ((fd_flags & O_ACCMODE) == O_WRONLY &&
       !(modeflags & static_cast<ModeFlags>(OpenMode::WRITE)))) {
    return Error(EINVAL);
  }

  bool do_seek = false;
  if ((modeflags & static_cast<ModeFlags>(OpenMode::APPEND)) &&
      !(fd_flags & O_APPEND)) {
    do_seek = true;
    if (!internal::fcntl(fd, F_SETFL,
                         reinterpret_cast<void *>(fd_flags | O_APPEND))
             .has_value()) {
      return Error(EBADF);
    }
  }

  uint8_t *buffer;
  {
    AllocChecker ac;
    buffer = new (ac) uint8_t[File::DEFAULT_BUFFER_SIZE];
    if (!ac) {
      return Error(ENOMEM);
    }
  }
  AllocChecker ac;
  auto *file = new (ac)
      LinuxFile(fd, buffer, File::DEFAULT_BUFFER_SIZE, _IOFBF, true, modeflags);
  if (!ac) {
    return Error(ENOMEM);
  }
  File::add_file(file);
  if (do_seek) {
    result = file->seek(0, SEEK_END);
    if (!result.has_value()) {
      File::remove_file(file);
      delete file;
      return Error(result.error());
    }
  }
  return file;
}

int get_fileno(File *f) {
  auto *lf = reinterpret_cast<LinuxFile *>(f);
  return lf->get_fd();
}

} // namespace LIBC_NAMESPACE_DECL
