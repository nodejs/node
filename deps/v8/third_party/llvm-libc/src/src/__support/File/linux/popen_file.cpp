//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// Implementation of the Linux File specialization for popen streams.
///
//===----------------------------------------------------------------------===//

#include "src/__support/File/linux/popen_file.h"

#include "src/__support/CPP/new.h"
#include "src/__support/File/file.h"
#include "src/__support/macros/config.h"
#include "src/__support/threads/mutex.h"

namespace LIBC_NAMESPACE_DECL {

PopenFile *PopenFile::popen_list = nullptr;
Mutex PopenFile::popen_list_lock(/*timed=*/false, /*recursive=*/false,
                                 /*robust=*/false, /*pshared=*/false);

void PopenFile::add_to_popen_list_unlocked(PopenFile *f) {
  f->popen_prev = nullptr;
  f->popen_next = popen_list;
  if (popen_list != nullptr)
    popen_list->popen_prev = f;
  popen_list = f;
}

void PopenFile::close_popen_list_fds_unlocked() {
  for (PopenFile *f = popen_list; f != nullptr; f = f->popen_next)
    f->close_fd();
}

int PopenFile::popen_file_close(File *f) {
  File::remove_file(f);

  auto *pf = reinterpret_cast<PopenFile *>(f);
  lock_popen_list();
  if (pf->popen_prev != nullptr)
    pf->popen_prev->popen_next = pf->popen_next;
  if (pf->popen_next != nullptr)
    pf->popen_next->popen_prev = pf->popen_prev;
  if (popen_list == pf)
    popen_list = pf->popen_next;
  // Close the fd before unlocking so a concurrent popen cannot fork and
  // inherit an fd that has already been removed from popen_list.
  int retval = pf->close_fd();
  unlock_popen_list();

  delete pf;
  return retval;
}

} // namespace LIBC_NAMESPACE_DECL
