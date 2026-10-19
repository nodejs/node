//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// Internal Linux signal mask helpers.
///
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC___SUPPORT_OSUTIL_LINUX_SIGMASK_H
#define LLVM_LIBC_SRC___SUPPORT_OSUTIL_LINUX_SIGMASK_H

#include "hdr/errno_macros.h"
#include "hdr/signal_macros.h"
#include "hdr/types/sigset_t.h"
#include "hdr/types/size_t.h"
#include "src/__support/OSUtil/linux/syscall_wrappers/rt_sigprocmask.h"
#include "src/__support/common.h"
#include "src/__support/error_or.h"
#include "src/__support/macros/config.h"

namespace LIBC_NAMESPACE_DECL {
namespace signals {

static constexpr size_t BITS_PER_SIGWORD = sizeof(unsigned long) * 8;

/// Return a signal set with all signals set.
LIBC_INLINE constexpr sigset_t full_set() {
  sigset_t set{};
  for (unsigned long &sigword : set.__signals)
    sigword = -1UL;
  return set;
}

/// Return an empty signal set with no signals set.
LIBC_INLINE constexpr sigset_t empty_set() { return sigset_t{}; }

/// Set or clear the bit corresponding to |signal| in |set| according to
/// |value|. Return true on success and false if |signal| is out of range.
LIBC_INLINE constexpr bool set_bit(sigset_t &set, int signal, bool value) {
  if (signal >= NSIG || signal <= 0)
    return false;
  size_t n = static_cast<size_t>(signal) - 1;
  size_t word = n / BITS_PER_SIGWORD;
  size_t bit = n % BITS_PER_SIGWORD;
  if (value)
    set.__signals[word] |= (1UL << bit);
  else
    set.__signals[word] &= ~(1UL << bit);
  return true;
}

/// Set the bit corresponding to |signal| in |set|. Return true on success
/// and false on failure. |signal| must be less than NSIG and be positive.
LIBC_INLINE constexpr bool add(sigset_t &set, int signal) {
  return set_bit(set, signal, true);
}

/// Clear the bit corresponding to |signal| in |set|. Return true on success
/// and false on failure. |signal| must be less than NSIG and be positive.
LIBC_INLINE constexpr bool remove(sigset_t &set, int signal) {
  return set_bit(set, signal, false);
}

/// Block the signals in |set|. If |old_set| is non-null, store the previous
/// signal mask in |*old_set|.
LIBC_INLINE ErrorOr<int> block(const sigset_t &set,
                               sigset_t *old_set = nullptr) {
  return linux_syscalls::rt_sigprocmask(SIG_BLOCK, &set, old_set);
}

/// Block a single |signal|. If |old_set| is non-null, store the previous
/// signal mask in |*old_set|.
LIBC_INLINE ErrorOr<int> block(int signal, sigset_t *old_set = nullptr) {
  sigset_t set = empty_set();
  if (!add(set, signal))
    return Error(EINVAL);
  return block(set, old_set);
}

/// Block all signals. If |old_set| is non-null, store the previous signal mask
/// in |*old_set|.
LIBC_INLINE ErrorOr<int> block_all(sigset_t *old_set = nullptr) {
  return block(full_set(), old_set);
}

/// Set the signal mask to |set|. If |old_set| is non-null, store the previous
/// signal mask in |*old_set|.
LIBC_INLINE ErrorOr<int> set_mask(const sigset_t &set,
                                  sigset_t *old_set = nullptr) {
  return linux_syscalls::rt_sigprocmask(SIG_SETMASK, &set, old_set);
}

/// Store the current signal mask in |set|.
LIBC_INLINE ErrorOr<int> get_mask(sigset_t &set) {
  return linux_syscalls::rt_sigprocmask(SIG_SETMASK, nullptr, &set);
}

/// Unblock the signals in |set|. If |old_set| is non-null, store the previous
/// signal mask in |*old_set|.
LIBC_INLINE ErrorOr<int> unblock(const sigset_t &set,
                                 sigset_t *old_set = nullptr) {
  return linux_syscalls::rt_sigprocmask(SIG_UNBLOCK, &set, old_set);
}

/// Unblock a single |signal|. If |old_set| is non-null, store the previous
/// signal mask in |*old_set|.
LIBC_INLINE ErrorOr<int> unblock(int signal, sigset_t *old_set = nullptr) {
  sigset_t set = empty_set();
  if (!add(set, signal))
    return Error(EINVAL);
  return unblock(set, old_set);
}

} // namespace signals
} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC___SUPPORT_OSUTIL_LINUX_SIGMASK_H
