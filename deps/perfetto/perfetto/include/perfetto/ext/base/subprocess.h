/*
 * Copyright (C) 2020 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef INCLUDE_PERFETTO_EXT_BASE_SUBPROCESS_H_
#define INCLUDE_PERFETTO_EXT_BASE_SUBPROCESS_H_

#include "perfetto/base/build_config.h"

// This is a #if as opposite to a GN condition, because GN conditions aren't propagated when
// translating to Bazel or other build systems, as they get resolved at translation time. Without
// this, the Bazel build breaks on Windows.
#if PERFETTO_BUILDFLAG(PERFETTO_OS_LINUX) ||   \
    PERFETTO_BUILDFLAG(PERFETTO_OS_ANDROID) || \
    PERFETTO_BUILDFLAG(PERFETTO_OS_MACOSX)
#define PERFETTO_HAS_SUBPROCESS() 1
#else
#define PERFETTO_HAS_SUBPROCESS() 0
#endif

#include <functional>
#include <initializer_list>
#include <string>
#include <thread>
#include <vector>

#include "perfetto/base/logging.h"
#include "perfetto/base/proc_utils.h"
#include "perfetto/ext/base/pipe.h"
#include "perfetto/ext/base/scoped_file.h"

namespace perfetto {
namespace base {

// Handles creation and lifecycle management of subprocesses, taking care of
// all subtleties involved in handling processes on UNIX.
// This class allows to deal with macro two use-cases:
// 1) fork() + exec() equivalent: for spawning a brand new process image.
//    This happens when |args.exec_cmd| is not empty.
//    This is safe to use even in a multi-threaded environment.
// 2) fork(): for spawning a process and running a function.
//    This happens when |args.entrypoint_for_testing| is not empty.
//    This is intended only for tests as it is extremely subtle.
//    This mode must be used with extreme care. Before the entrypoint is
//    invoked all file descriptors other than stdin/out/err and the ones
//    specified in |args.preserve_fds| will be closed, to avoid each process
//    retaining a dupe of other subprocesses pipes. This however means that
//    any non trivial calls (including logging) must be avoided as they might
//    refer to FDs that are now closed. The entrypoint should really be used
//    just to signal a pipe or similar for synchronizing sequencing in tests.

//
// This class allows to control stdin/out/err pipe redirection and takes care
// of keeping all the pipes pumped (stdin) / drained (stdout/err), in a similar
// fashion of python's subprocess.Communicate()
// stdin: is always piped and closed once the |args.input| buffer is written.
// stdout/err can be either:
//   - dup()ed onto the parent process stdout/err.
//   - redirected onto /dev/null.
//   - piped onto a buffer (see output() method). There is only one output
//     buffer in total. If both stdout and stderr are set to kBuffer mode, they
//     will be merged onto the same. There doesn't seem any use case where they
//     are needed distinctly.
//
// Some caveats worth mentioning:
// - It always waitpid()s, to avoid leaving zombies around. If the process is
//   not terminated by the time the destructor is reached, the dtor will
//   send a SIGKILL and wait for the termination.
// - After fork()-ing it will close all file descriptors, preserving only
//   stdin/out/err and the fds listed in |args.preserve_fds|.
// - On Linux/Android, the child process will be SIGKILL-ed if the calling
//   thread exists, even if the Subprocess is std::move()-d onto another thread.
//   This happens by virtue PR_SET_PDEATHSIG, which is used to avoid that
//   child processes are leaked in the case of a crash of the parent (frequent
//   in tests). However, the child process might still be leaked if execing
//   a setuid/setgid binary (see man 2 prctl).
//
// Usage:
// base::Subprocess p({"/bin/cat", "-"});
// (or equivalently:
//     base::Subprocess p;
//     p.args.exec_cmd.push_back("/bin/cat");
//     p.args.exec_cmd.push_back("-");
//  )
// p.args.stdout_mode = base::Subprocess::kBuffer;
// p.args.stderr_mode = base::Subprocess::kInherit;
// p.args.input = "stdin contents";
// p.Call();
// (or equivalently:
//     p.Start();
//     p.Wait();
// )
// EXPECT_EQ(p.status(), base::Subprocess::kExited);
// EXPECT_EQ(p.returncode(), 0);
class Subprocess {
 public:
  enum Status {
    kNotStarted = 0,  // Before calling Start() or Call().
    kRunning,         // After calling Start(), before Wait().
    kExited,          // The subprocess exited (either succesully or not).
    kKilledBySignal,  // The subprocess has been killed by a signal.
  };

  enum OutputMode {
    kInherit = 0,  // Inherit's the caller process stdout/stderr.
    kDevNull,      // dup() onto /dev/null
    kBuffer        // dup() onto a pipe and move it into the output() buffer.
  };

  // Input arguments for configuring the subprocess behavior.
  struct Args {
    Args(std::initializer_list<std::string> _cmd = {}) : exec_cmd(_cmd) {}
    Args(Args&&) noexcept;
    Args& operator=(Args&&);
    // If non-empty this will cause an exec() when Start()/Call() are called.
    std::vector<std::string> exec_cmd;

    // If non-empty, it changes the argv[0] argument passed to exec. If
    // unset, argv[0] == exec_cmd[0]. This is to handle cases like:
    // exec_cmd = {"/proc/self/exec"}, argv0: "my_custom_test_override".
    std::string argv0_override;

    // If non-empty this will be invoked on the fork()-ed child process, after
    // stdin/out/err has been redirected and all other file descriptor are
    // closed.
    // It is valid to specify both |exec_cmd| AND |entrypoint_for_testing|.
    // In this case |entrypoint_for_testing| will be invoked just before the
    // exec() call, but after having closed all fds % stdin/out/err.
    // This is for synchronization barriers in tests.
    std::function<void()> entrypoint_for_testing;

    // If non-empty, replaces the environment passed to exec().
    std::vector<std::string> env;

    // The file descriptors in this list will not be closed.
    std::vector<int> preserve_fds;

    // The data to push in the child process stdin.
    std::string input;

    OutputMode stdout_mode = kInherit;
    OutputMode stderr_mode = kInherit;

    // Returns " ".join(exec_cmd), quoting arguments.
    std::string GetCmdString() const;
  };

  explicit Subprocess(std::initializer_list<std::string> exec_cmd = {});
  Subprocess(Subprocess&&) noexcept;
  Subprocess& operator=(Subprocess&&);
  ~Subprocess();  // It will KillAndWaitForTermination() if still alive.

  // Starts the subprocess but doesn't wait for its termination. The caller
  // is expected to either call Wait() or Poll() after this call.
  void Start();

  // Wait for process termination. Can be called more than once.
  // Args:
  //   |timeout_ms| = 0: wait indefinitely.
  //   |timeout_ms| > 0: wait for at most |timeout_ms|.
  // Returns:
  //  True: The process terminated. See status() and returncode().
  //  False: Timeout reached, the process is still running. In this case the
  //         process will be left in the kRunning state.
  bool Wait(int timeout_ms = 0);

  // Equivalent of Start() + Wait();
  // Returns true if the process exited cleanly with return code 0. False in
  // any othe case.
  bool Call(int timeout_ms = 0);

  Status Poll();

  // Sends a SIGKILL and wait to see the process termination.
  void KillAndWaitForTermination();

  PlatformProcessId pid() const { return pid_; }
  Status status() const { return status_; }
  int returncode() const { return returncode_; }

  // This contains both stdout and stderr (if the corresponding _mode ==
  // kBuffer). It's non-const so the caller can std::move() it.
  std::string& output() { return output_; }

  Args args;

 private:
  Subprocess(const Subprocess&) = delete;
  Subprocess& operator=(const Subprocess&) = delete;
  void TryPushStdin();
  void TryReadStdoutAndErr();
  void TryReadExitStatus();
  void KillAtMostOnce();
  bool PollInternal(int poll_timeout_ms);

  base::Pipe stdin_pipe_;
  base::Pipe stdouterr_pipe_;
  base::Pipe exit_status_pipe_;
  PlatformProcessId pid_;
  size_t input_written_ = 0;
  Status status_ = kNotStarted;
  int returncode_ = -1;
  std::string output_;  // Stdin+stderr. Only when kBuffer.
  std::thread waitpid_thread_;
};

}  // namespace base
}  // namespace perfetto

#endif  // INCLUDE_PERFETTO_EXT_BASE_SUBPROCESS_H_
