//===-- Implementation of tls for x86_64 ----------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "config/app.h"
#include "hdr/sys_mman_macros.h"
#include "src/__support/OSUtil/linux/syscall_wrappers/getrandom.h"
#include "src/__support/OSUtil/linux/syscall_wrappers/mmap.h"
#include "src/__support/OSUtil/linux/syscall_wrappers/munmap.h"
#include "src/__support/macros/config.h"
#include "src/__support/threads/linux/tcb.h"
#include "src/string/memory_utils/inline_memcpy.h"

#include <asm/prctl.h>
#include <sys/syscall.h>

namespace LIBC_NAMESPACE_DECL {

// TODO: Also generalize this routine and handle dynamic loading properly.
[[gnu::flatten]] void init_tls(TLSDescriptor &tls_descriptor) {
  // We will assume the alignment is always a power of two.
  uintptr_t tls_size = app.tls.size & -app.tls.align;
  if (tls_size != app.tls.size)
    tls_size += app.tls.align;

  uintptr_t tls_size_with_tcb = tls_size + sizeof(ThreadControlBlock);

  ErrorOr<void *> mmap_ret =
      linux_syscalls::mmap(nullptr, tls_size_with_tcb, PROT_READ | PROT_WRITE,
                           MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
  if (!mmap_ret.has_value())
    syscall_impl<long>(SYS_exit, 1);
  uintptr_t *tls_addr = static_cast<uintptr_t *>(mmap_ret.value());

  // x86_64 TLS faces down from the thread pointer with the first entry
  // pointing to the address of the first real TLS byte.
  uintptr_t end_ptr = reinterpret_cast<uintptr_t>(tls_addr) + tls_size;
  auto *tcb = reinterpret_cast<ThreadControlBlock *>(end_ptr);
  tcb->self = end_ptr;

  if (app.tls.init_size) {
    inline_memcpy(reinterpret_cast<char *>(tls_addr),
                  reinterpret_cast<const char *>(app.tls.address),
                  app.tls.init_size);
  }
  // Setting the stack guard to a random value.
  // We cannot call the get_random function here as the function sets errno on
  // failure. Since errno is implemented via a thread local variable, we cannot
  // use errno before TLS is setup. The linux_syscalls wrapper is safe as it
  // reports errors via ErrorOr instead of errno.
  ErrorOr<ssize_t> stack_guard_retval =
      linux_syscalls::getrandom(&tcb->stack_guard, sizeof(tcb->stack_guard), 0);
  if (!stack_guard_retval.has_value())
    syscall_impl(SYS_exit, 1);

  tls_descriptor = {tls_size_with_tcb, reinterpret_cast<uintptr_t>(tls_addr),
                    end_ptr};
  return;
}

void cleanup_tls(uintptr_t addr, uintptr_t size) {
  if (size == 0)
    return;
  linux_syscalls::munmap(reinterpret_cast<void *>(addr), size);
}

// Sets the thread pointer to |val|. Returns true on success, false on failure.
bool set_thread_ptr(uintptr_t val) {
  return syscall_impl(SYS_arch_prctl, ARCH_SET_FS, val) != -1;
}
} // namespace LIBC_NAMESPACE_DECL
