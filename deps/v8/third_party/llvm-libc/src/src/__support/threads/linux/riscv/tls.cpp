//===-- Implementation of tls for riscv -----------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "config/app.h"
#include "hdr/sys_mman_macros.h"
#include "src/__support/OSUtil/linux/syscall_wrappers/mmap.h"
#include "src/__support/OSUtil/linux/syscall_wrappers/munmap.h"
#include "src/__support/OSUtil/syscall.h"
#include "src/__support/macros/config.h"
#include "src/__support/threads/linux/tcb.h"
#include "src/string/memory_utils/inline_memcpy.h"
#include <sys/syscall.h>

namespace LIBC_NAMESPACE_DECL {

[[gnu::flatten]] void init_tls(TLSDescriptor &tls_descriptor) {
  // riscv follows the variant 1 TLS layout:
  const uintptr_t TCB_SIZE = sizeof(ThreadControlBlock);
  uintptr_t padding = 0;
  const uintptr_t ALIGNMENT_MASK = app.tls.align - 1;
  uintptr_t diff = TCB_SIZE & ALIGNMENT_MASK;
  if (diff != 0)
    padding += (ALIGNMENT_MASK - diff) + 1;

  uintptr_t alloc_size = TCB_SIZE + padding + app.tls.size;

  ErrorOr<void *> mmap_ret =
      linux_syscalls::mmap(nullptr, alloc_size, PROT_READ | PROT_WRITE,
                           MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
  if (!mmap_ret.has_value())
    syscall_impl<long>(SYS_exit, 1);
  uintptr_t thread_ptr = uintptr_t(mmap_ret.value());
  uintptr_t tls_addr = thread_ptr + TCB_SIZE + padding;
  inline_memcpy(reinterpret_cast<char *>(tls_addr),
                reinterpret_cast<const char *>(app.tls.address),
                app.tls.init_size);
  tls_descriptor.size = alloc_size;
  tls_descriptor.addr = thread_ptr;
  tls_descriptor.tp = tls_addr;
}

void cleanup_tls(uintptr_t addr, uintptr_t size) {
  if (size == 0)
    return;
  linux_syscalls::munmap(reinterpret_cast<void *>(addr), size);
}

bool set_thread_ptr(uintptr_t val) {
  LIBC_INLINE_ASM("mv tp, %0\n\t" : : "r"(val));
  return true;
}
} // namespace LIBC_NAMESPACE_DECL
