//===------------- Linux AUXV Header --------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC___SUPPORT_OSUTIL_LINUX_AUXV_H
#define LLVM_LIBC_SRC___SUPPORT_OSUTIL_LINUX_AUXV_H

#include "hdr/fcntl_macros.h" // For open flags
#include "hdr/sys_auxv_macros.h" // For AT_ macros
#include "src/__support/OSUtil/syscall.h"
#include "src/__support/common.h"
#include "src/__support/threads/callonce.h"

#include <linux/mman.h>   // For mmap flags
#include <linux/param.h>  // For EXEC_PAGESIZE
#include <linux/prctl.h>  // For prctl
#include <sys/syscall.h>  // For syscall numbers

namespace LIBC_NAMESPACE_DECL {

namespace auxv {
struct Entry {
  unsigned long type; // Entry type
  unsigned long val;  // Integer value
};

class Vector {
  LIBC_INLINE_VAR static constexpr Entry END = {AT_NULL, AT_NULL};
  LIBC_INLINE_VAR static const Entry *entries = &END;
  LIBC_INLINE_VAR static CallOnceFlag init_flag = callonce_impl::NOT_CALLED;
  LIBC_INLINE_VAR constexpr static size_t FALLBACK_AUXV_ENTRIES = 64;

  LIBC_INLINE static void fallback_initialize_unsync();
  LIBC_INLINE static const Entry *get_entries() {
    if (LIBC_LIKELY(entries != &END))
      return entries;
    callonce(&init_flag, fallback_initialize_unsync);
    return entries;
  }

public:
  class Iterator {
    const Entry *current;

  public:
    LIBC_INLINE explicit Iterator(const Entry *entry) : current(entry) {}
    LIBC_INLINE Iterator &operator++() {
      ++current;
      return *this;
    }
    LIBC_INLINE const Entry &operator*() const { return *current; }
    LIBC_INLINE bool operator!=(const Iterator &other) const {
      return current->type != other.current->type;
    }
    LIBC_INLINE bool operator==(const Iterator &other) const {
      return current->type == other.current->type;
    }
  };
  using iterator = Iterator;
  LIBC_INLINE static Iterator begin() { return Iterator(get_entries()); }
  LIBC_INLINE static Iterator end() { return Iterator(&END); }
  LIBC_INLINE static void initialize_unsafe(const Entry *auxv);
};

// Initializes the auxv entries.
// This function is intended to be called once inside crt0.
LIBC_INLINE void Vector::initialize_unsafe(const Entry *auxv) {
  init_flag = callonce_impl::FINISH;
  entries = auxv;
}

// When CRT0 does not setup the global array, this function is called.
// As its name suggests, this function is not thread-safe and should be
// backed by a callonce guard.
// This initialize routine will do a mmap to allocate a memory region.
// Since auxv tends to live throughout the program lifetime, we do not
// munmap it.
[[gnu::cold]]
LIBC_INLINE void Vector::fallback_initialize_unsync() {
  constexpr size_t AUXV_MMAP_SIZE = FALLBACK_AUXV_ENTRIES * sizeof(Entry);
#ifdef SYS_mmap2
  constexpr int MMAP_SYSNO = SYS_mmap2;
#else
  constexpr int MMAP_SYSNO = SYS_mmap;
#endif
  long mmap_ret = syscall_impl<long>(MMAP_SYSNO, nullptr, AUXV_MMAP_SIZE,
                                     PROT_READ | PROT_WRITE,
                                     MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  // We do not proceed if mmap fails.
  if (!linux_utils::is_valid_mmap(mmap_ret))
    return;

  // Initialize the auxv array with AT_NULL entries.
  Entry *vector = reinterpret_cast<Entry *>(mmap_ret);
  for (size_t i = 0; i < FALLBACK_AUXV_ENTRIES; ++i) {
    vector[i].type = AT_NULL;
    vector[i].val = AT_NULL;
  }
  size_t avaiable_size = AUXV_MMAP_SIZE - sizeof(Entry);

// Attempt 1: use PRCTL to get the auxv.
// We guarantee that the vector is always padded with AT_NULL entries.
#ifdef PR_GET_AUXV
  long prctl_ret = syscall_impl<long>(SYS_prctl, PR_GET_AUXV,
                                      reinterpret_cast<unsigned long>(vector),
                                      avaiable_size, 0, 0);
  if (prctl_ret >= 0) {
    entries = vector;
    return;
  }
#endif

  // Attempt 2: read /proc/self/auxv.
#ifdef SYS_openat
  int fd = syscall_impl<int>(SYS_openat, AT_FDCWD, "/proc/self/auxv",
                             O_RDONLY | O_CLOEXEC);
#else
  int fd = syscall_impl<int>(SYS_open, "/proc/self/auxv", O_RDONLY | O_CLOEXEC);
#endif
  if (fd < 0) {
    syscall_impl<long>(SYS_munmap, vector, AUXV_MMAP_SIZE);
    return;
  }
  uint8_t *cursor = reinterpret_cast<uint8_t *>(vector);
  bool has_error = false;
  while (avaiable_size != 0) {
    long bytes_read = syscall_impl<long>(SYS_read, fd, cursor, avaiable_size);
    if (bytes_read <= 0) {
      if (bytes_read == -EINTR)
        continue;
      has_error = bytes_read < 0;
      break;
    }
    avaiable_size -= bytes_read;
    cursor += bytes_read;
  }
  syscall_impl<long>(SYS_close, fd);
  if (has_error) {
    syscall_impl<long>(SYS_munmap, vector, AUXV_MMAP_SIZE);
    return;
  }
  entries = vector;
}

LIBC_INLINE cpp::optional<unsigned long> get(unsigned long type) {
  Vector auxvec;
  for (const auto &entry : auxvec)
    if (entry.type == type)
      return entry.val;
  return cpp::nullopt;
}
} // namespace auxv
} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC___SUPPORT_OSUTIL_LINUX_AUXV_H
