//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// Helpers for running a command with the shell in a child process, shared by
/// functions like system and popen.
///
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC___SUPPORT_OSUTIL_LINUX_SHELL_FORK_H
#define LLVM_LIBC_SRC___SUPPORT_OSUTIL_LINUX_SHELL_FORK_H

#include "hdr/errno_macros.h"
#include "hdr/signal_macros.h"
#include "hdr/types/pid_t.h"
#include "src/__support/CPP/functional.h"
#include "src/__support/OSUtil/exit.h"
#include "src/__support/OSUtil/linux/syscall_wrappers/clone3.h"
#include "src/__support/OSUtil/linux/syscall_wrappers/execle.h"
#include "src/__support/OSUtil/linux/syscall_wrappers/wait4.h"
#include "src/__support/common.h"
#include "src/__support/error_or.h"
#include "src/__support/macros/config.h"
#include "src/unistd/environ.h"

#include <linux/sched.h> // For struct clone_args

namespace LIBC_NAMESPACE_DECL {

// TODO: Move SHELL_PATH into its own header so it can be shared with functions
// like execvp.
LIBC_INLINE_VAR constexpr char SHELL_PATH[] = "/bin/sh";

// Work to do in the child process after forking and before executing the
// shell. Returns false if the setup failed and the shell should not be run.
using ShellChildSetup = cpp::function_ref<bool()>;

// Forks a child process which runs |setup| and then executes
// "sh -c -- |command|" with the current environment. If |setup| or the exec
// fails, the child exits with status 127. Returns the child's pid in the
// parent.
LIBC_INLINE ErrorOr<pid_t> shell_fork(const char *command,
                                      ShellChildSetup setup) {
  struct clone_args cl_args{};
  cl_args.exit_signal = SIGCHLD;
  ErrorOr<pid_t> fork_res = linux_syscalls::clone3(&cl_args, sizeof(cl_args));
  if (!fork_res.has_value())
    return Error(fork_res.error());

  if (fork_res.value() == 0) {
    // Error checking isn't helpful since this is the forked process, so we
    // can't set errno. All we can meaningfully do is exit with status 127.
    if (setup())
      linux_syscalls::execle(SHELL_PATH, "sh", "-c", "--", command, 0, environ);
    internal::exit(127);
  }

  return fork_res.value();
}

// Waits for the child process |pid| to terminate, retrying if interrupted by
// a signal. Returns the child's wait status.
LIBC_INLINE ErrorOr<int> shell_wait(pid_t pid) {
  int status = 0;
  while (true) {
    ErrorOr<pid_t> wait_res = linux_syscalls::wait4(pid, &status, 0, nullptr);
    if (wait_res.has_value())
      return status;
    if (wait_res.error() != EINTR)
      return Error(wait_res.error());
  }
}

} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC___SUPPORT_OSUTIL_LINUX_SHELL_FORK_H
