//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// Implementation of the Linux specialization of File.
///
//===----------------------------------------------------------------------===//

#include "file.h"
#include "file_flags.h"
#include "hdr/errno_macros.h"
#include "hdr/stdio_macros.h"
#include "hdr/sys_stat_macros.h" // For S_IS*, S_IF*, and S_IR* flags.
#include "hdr/types/off_t.h"
#include "src/__support/CPP/new.h"
#include "src/__support/File/file.h"
#include "src/__support/File/file_mode.h"
#include "src/__support/OSUtil/linux/syscall_wrappers/close.h"
#include "src/__support/OSUtil/linux/syscall_wrappers/dup3.h"
#include "src/__support/OSUtil/linux/syscall_wrappers/fcntl.h"
#include "src/__support/OSUtil/linux/syscall_wrappers/lseek.h"
#include "src/__support/OSUtil/linux/syscall_wrappers/open.h"
#include "src/__support/OSUtil/linux/syscall_wrappers/read.h"
#include "src/__support/OSUtil/linux/syscall_wrappers/write.h"
#include "src/__support/macros/config.h"

namespace LIBC_NAMESPACE_DECL {

FileIOResult linux_file_write(File *f, const void *data, size_t size) {
  auto *lf = reinterpret_cast<LinuxFile *>(f);
  auto ret = linux_syscalls::write(lf->get_fd(), data, size);
  if (!ret) {
    return {0, ret.error()};
  }
  return static_cast<size_t>(ret.value());
}

FileIOResult linux_file_read(File *f, void *buf, size_t size) {
  auto *lf = reinterpret_cast<LinuxFile *>(f);
  auto ret = linux_syscalls::read(lf->get_fd(), buf, size);
  if (!ret) {
    return {0, ret.error()};
  }
  return static_cast<size_t>(ret.value());
}

ErrorOr<off_t> linux_file_seek(File *f, off_t offset, int whence) {
  auto *lf = reinterpret_cast<LinuxFile *>(f);
  return linux_syscalls::lseek(lf->get_fd(), offset, whence);
}

int LinuxFile::close_fd() {
  if (fd < 0)
    return EBADF;
  // Linux closes the file descriptor early in the syscall, so we assume it's
  // always closed after the call. That means callers should also delete the
  // file object on error.
  auto result = linux_syscalls::close(fd);
  if (!result)
    return result.error();
  return 0;
}

int linux_file_close(File *f) {
  File::remove_file(f);
  auto *lf = reinterpret_cast<LinuxFile *>(f);
  int retval = lf->close_fd();
  delete lf;
  return retval;
}

static int map_c_mode_flags_to_linux_open_flags(const FileMode &file_mode) {
  int open_flags = 0;

  // handle access patterns i.e whether the file should be in
  // only read, write modes or both.
  if (file_mode.is_update())
    open_flags = LinuxFileFlags::READ_AND_WRITE;
  else if (file_mode.is_append() || file_mode.is_write())
    open_flags = LinuxFileFlags::WRITE_ONLY;
  else
    open_flags = LinuxFileFlags::READ_ONLY;

  // handle the behaviour of the file when accessed i.e should the file
  // be appended to or truncate when created.
  if (file_mode.is_append())
    open_flags |= LinuxFileFlags::CREATE_AND_APPEND;
  else if (file_mode.is_write())
    open_flags |= LinuxFileFlags::CREATE_OR_TRUNCATE;

  // Ignore 'x' for read modes: O_EXCL is only meaningful with O_CREAT here.
  if (file_mode.is_exclusive_create() &&
      (file_mode.is_write() || file_mode.is_append()))
    open_flags |= LinuxFileFlags::EXCLUSIVE_CREATE;

  if (file_mode.is_close_on_exec())
    open_flags |= LinuxFileFlags::CLOSE_ON_EXEC;

  return open_flags;
}

ErrorOr<File *> openfile(const char *path, const char *mode) {
  const FileMode file_mode(mode);

  if (!file_mode.is_valid()) {
    return Error(EINVAL);
  }
  int open_flags = map_c_mode_flags_to_linux_open_flags(file_mode);

  ErrorOr<int> fd =
      linux_syscalls::open(path, open_flags, LinuxFileFlags::OPEN_MODE);
  if (!fd)
    return Error(fd.error());

  ErrorOr<LinuxFile *> file = LinuxFile::create_buffered(fd.value(), file_mode);
  if (!file)
    return Error(file.error());
  File::add_file(file.value());
  return file.value();
}

static ErrorOr<int> set_close_on_exec(int fd) {
  auto flags = linux_syscalls::fcntl(fd, F_GETFD);
  if (!flags)
    return Error(flags.error());
  return linux_syscalls::fcntl(
      fd, F_SETFD, reinterpret_cast<void *>(flags.value() | FD_CLOEXEC));
}

ErrorOr<LinuxFile *> create_file_from_fd(int fd, const char *mode) {
  const FileMode file_mode(mode);

  if (!file_mode.is_valid()) {
    return Error(EINVAL);
  }

  auto result = linux_syscalls::fcntl(fd, F_GETFL);
  if (!result.has_value()) {
    return Error(EBADF);
  }
  int fd_flags = result.value();

  if ((LinuxFileFlags::is_file_descriptor_opened_in_read_only(fd_flags) &&
       file_mode.write_allowed()) ||
      (LinuxFileFlags::is_file_descriptor_opened_in_write_only(fd_flags) &&
       file_mode.read_allowed())) {
    return Error(EINVAL);
  }

  if (file_mode.is_close_on_exec()) {
    auto cloexec_result = set_close_on_exec(fd);
    if (!cloexec_result)
      return Error(cloexec_result.error());
  }

  bool do_seek = false;
  if (file_mode.is_append() &&
      !LinuxFileFlags::file_has_append_flag(fd_flags)) {
    do_seek = true;
    if (!linux_syscalls::fcntl(fd, F_SETFL,
                               reinterpret_cast<void *>(fd_flags | O_APPEND))
             .has_value()) {
      return Error(EBADF);
    }
  }

  ErrorOr<LinuxFile *> new_file = LinuxFile::create_buffered(fd, file_mode);
  if (!new_file)
    return Error(new_file.error());
  LinuxFile *file = new_file.value();
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

int LinuxFile::reopen_unlocked(const char *path, const char *mode) {
  flush_unlocked();

  const FileMode file_mode(mode);

  if (path != nullptr) {
    int old_fd = get_fd();

    if (!file_mode.is_valid()) {
      if (old_fd >= 0) {
        linux_syscalls::close(old_fd);
        set_fd(-1);
      }
      reset_stream_state_unlocked(file_mode);
      return EINVAL;
    }

    int open_flags = map_c_mode_flags_to_linux_open_flags(file_mode);

    // Prevent the temporary descriptor from being inherited across exec.
    if (old_fd >= 0)
      open_flags |= LinuxFileFlags::CLOSE_ON_EXEC;

    ErrorOr<int> new_fd =
        linux_syscalls::open(path, open_flags, LinuxFileFlags::OPEN_MODE);

    // If the new file fails to open, POSIX says we still have to close the old
    // file.
    if (!new_fd) {
      if (old_fd >= 0) {
        // POSIX: "Failure to close the file descriptor successfully shall be
        // ignored"
        linux_syscalls::close(old_fd);

        set_fd(-1);
      }
      reset_stream_state_unlocked(file_mode);
      return new_fd.error();
    }

    // Else the new file successfully opened, so we move it into the fd the old
    // file was using if the old fd exists.
    if (old_fd >= 0) {
      // Preserve close-on-exec atomically when replacing the old descriptor.
      int dup_flags =
          file_mode.is_close_on_exec() ? LinuxFileFlags::CLOSE_ON_EXEC : 0;
      auto dup_result = linux_syscalls::dup3(new_fd.value(), old_fd, dup_flags);
      if (!dup_result) {
        linux_syscalls::close(new_fd.value());
        reset_stream_state_unlocked(file_mode);
        return dup_result.error();
      }
      auto close_result = linux_syscalls::close(new_fd.value());
      if (!close_result) {
        reset_stream_state_unlocked(file_mode);
        return close_result.error();
      }
    } else {
      set_fd(new_fd.value());
    }

    reset_stream_state_unlocked(file_mode);
    return 0;
  }

  if (!file_mode.is_valid())
    return EINVAL;

  if (fd < 0)
    return EBADF;

  auto result = linux_syscalls::fcntl(fd, F_GETFL);
  if (!result.has_value())
    return EBADF;
  int fd_flags = result.value();

  if ((LinuxFileFlags::is_file_descriptor_opened_in_read_only(fd_flags) &&
       file_mode.write_allowed()) ||
      (LinuxFileFlags::is_file_descriptor_opened_in_write_only(fd_flags) &&
       file_mode.read_allowed())) {
    return EBADF;
  }

  bool do_seek = false;
  bool has_append_flag = LinuxFileFlags::file_has_append_flag(fd_flags);

  if (file_mode.is_append() && !has_append_flag) {
    if (!linux_syscalls::fcntl(fd, F_SETFL,
                               reinterpret_cast<void *>(fd_flags | O_APPEND))
             .has_value()) {
      return EBADF;
    }
    do_seek = true;
  } else if (!file_mode.is_append() && has_append_flag) {
    if (!linux_syscalls::fcntl(fd, F_SETFL,
                               reinterpret_cast<void *>(fd_flags & ~O_APPEND))
             .has_value()) {
      return EBADF;
    }
  }

  reset_stream_state_unlocked(file_mode);

  if (do_seek) {
    auto seek_result = linux_file_seek(this, 0, SEEK_END);
    if (!seek_result.has_value())
      return seek_result.error();
  }
  return 0;
}

int get_fileno(File *f) {
  auto *lf = reinterpret_cast<LinuxFile *>(f);
  return lf->get_fd();
}

int reopenfile(File *f, const char *path, const char *mode) {
  File::FileLock lock(f);
  return reopenfile_unlocked(f, path, mode);
}

int reopenfile_unlocked(File *f, const char *path, const char *mode) {
  auto *lf = reinterpret_cast<LinuxFile *>(f);
  return lf->reopen_unlocked(path, mode);
}

} // namespace LIBC_NAMESPACE_DECL
