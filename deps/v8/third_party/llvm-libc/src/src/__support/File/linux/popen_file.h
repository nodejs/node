//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// Linux File specialization for streams created by popen.
///
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC___SUPPORT_FILE_LINUX_POPEN_FILE_H
#define LLVM_LIBC_SRC___SUPPORT_FILE_LINUX_POPEN_FILE_H

#include "hdr/types/pid_t.h"
#include "src/__support/File/file.h"
#include "src/__support/File/file_mode.h"
#include "src/__support/File/linux/file.h"
#include "src/__support/macros/config.h"
#include "src/__support/threads/mutex.h"

namespace LIBC_NAMESPACE_DECL {

// A LinuxFile connected to a pipe to a child process created by popen. Open
// popen streams are kept on their own list, so that each new popen child can
// close the streams of earlier popen calls.
class PopenFile : public LinuxFile {
  pid_t pid = -1;
  PopenFile *popen_prev = nullptr;
  PopenFile *popen_next = nullptr;

  static PopenFile *popen_list;
  static Mutex popen_list_lock;

  // Removes |f| from the popen list, then closes and frees it like a
  // LinuxFile. This does not wait for the child process.
  static int popen_file_close(File *f);

public:
  PopenFile(int file_descriptor, uint8_t *buffer, size_t buffer_size,
            int buffer_mode, bool owned, FileMode mode)
      : LinuxFile(file_descriptor, buffer, buffer_size, buffer_mode, owned,
                  mode, &popen_file_close) {}

  pid_t get_pid() const { return pid; }
  void set_pid(pid_t new_pid) { pid = new_pid; }

  // The popen list lock must be held while calling the *_unlocked functions
  // below. popen holds it from before forking until the new stream is on the
  // list, so that the child sees a consistent list.
  static void lock_popen_list() { popen_list_lock.lock(); }
  static void unlock_popen_list() { popen_list_lock.unlock(); }

  // Adds |f| to the popen list.
  static void add_to_popen_list_unlocked(PopenFile *f);

  // Closes the file descriptors of every stream on the popen list without
  // freeing anything. This is meant to be called in a newly forked child.
  static void close_popen_list_fds_unlocked();
};

} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC___SUPPORT_FILE_LINUX_POPEN_FILE_H
