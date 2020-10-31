/*
 * Copyright (C) 2018 The Android Open Source Project
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

#include "src/traced/probes/ftrace/atrace_wrapper.h"

#include <fcntl.h>
#include <poll.h>
#include <stdint.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "perfetto/base/logging.h"
#include "perfetto/base/time.h"
#include "perfetto/ext/base/pipe.h"
#include "perfetto/ext/base/utils.h"

namespace perfetto {

namespace {

RunAtraceFunction g_run_atrace_for_testing = nullptr;

#if PERFETTO_BUILDFLAG(PERFETTO_OS_ANDROID)
// Args should include "atrace" for argv[0].
bool ExecvAtrace(const std::vector<std::string>& args) {
  int status = 1;

  std::vector<char*> argv;
  // args, and then a null.
  argv.reserve(1 + args.size());
  for (const auto& arg : args)
    argv.push_back(const_cast<char*>(arg.c_str()));
  argv.push_back(nullptr);

  // Create the pipe for the child process to return stderr.
  base::Pipe err_pipe = base::Pipe::Create(base::Pipe::kRdNonBlock);

  pid_t pid = fork();
  PERFETTO_CHECK(pid >= 0);
  if (pid == 0) {
    // Duplicate the write end of the pipe into stderr.
    if ((dup2(*err_pipe.wr, STDERR_FILENO) == -1)) {
      const char kError[] = "Unable to duplicate stderr fd";
      base::ignore_result(write(*err_pipe.wr, kError, sizeof(kError)));
      _exit(1);
    }

    int null_fd = open("/dev/null", O_RDWR);
    if (null_fd == -1) {
      const char kError[] = "Unable to open dev null";
      base::ignore_result(write(*err_pipe.wr, kError, sizeof(kError)));
      _exit(1);
    }

    if ((dup2(null_fd, STDOUT_FILENO) == -1)) {
      const char kError[] = "Unable to duplicate stdout fd";
      base::ignore_result(write(*err_pipe.wr, kError, sizeof(kError)));
      _exit(1);
    }

    if ((dup2(null_fd, STDIN_FILENO) == -1)) {
      const char kError[] = "Unable to duplicate stdin fd";
      base::ignore_result(write(*err_pipe.wr, kError, sizeof(kError)));
      _exit(1);
    }

    // Close stdin/out + any file descriptor that we might have mistakenly
    // not marked as FD_CLOEXEC. |err_pipe| is FD_CLOEXEC and will be
    // automatically closed on exec.
    for (int i = 0; i < 128; i++) {
      if (i != STDIN_FILENO && i != STDERR_FILENO && i != STDOUT_FILENO)
        close(i);
    }

    execv("/system/bin/atrace", &argv[0]);

    // Reached only if execv fails.
    _exit(1);
  }

  // Close the write end of the pipe.
  err_pipe.wr.reset();

  // Collect the output from child process.
  char buffer[4096];
  std::string error;

  // Get the read end of the pipe.
  constexpr uint8_t kFdCount = 1;
  struct pollfd fds[kFdCount]{};
  fds[0].fd = *err_pipe.rd;
  fds[0].events = POLLIN;

  // Store the start time of atrace and setup the timeout.
  constexpr auto timeout = base::TimeMillis(20000);
  auto start = base::GetWallTimeMs();
  for (;;) {
    // Check if we are below the timeout and update the select timeout to
    // the time remaining.
    auto now = base::GetWallTimeMs();
    auto remaining = timeout - (now - start);
    auto timeout_ms = static_cast<int>(remaining.count());
    if (timeout_ms <= 0) {
      // Kill atrace.
      kill(pid, SIGKILL);

      std::string cmdline = "/system/bin/atrace";
      for (const auto& arg : args) {
        cmdline += " " + arg;
      }
      error.append("Timed out waiting for atrace (cmdline: " + cmdline + ")");
      break;
    }

    // Wait for the value of the timeout.
    auto ret = poll(fds, kFdCount, timeout_ms);
    if (ret == 0 || (ret < 0 && errno == EINTR)) {
      // Either timeout occurred in poll (in which case continue so that this
      // will be picked up by our own timeout logic) or we received an EINTR and
      // we should try again.
      continue;
    } else if (ret < 0) {
      error.append("Error while polling atrace stderr");
      break;
    }

    // Data is available to be read from the fd.
    int64_t count = PERFETTO_EINTR(read(*err_pipe.rd, buffer, sizeof(buffer)));
    if (ret < 0 && errno == EAGAIN) {
      continue;
    } else if (count < 0) {
      error.append("Error while reading atrace stderr");
      break;
    } else if (count == 0) {
      // EOF so we can exit this loop.
      break;
    }
    error.append(buffer, static_cast<size_t>(count));
  }

  // Wait until the child process exits fully.
  PERFETTO_EINTR(waitpid(pid, &status, 0));

  bool ok = WIFEXITED(status) && WEXITSTATUS(status) == 0;
  if (!ok) {
    // TODO(lalitm): use the stderr result from atrace.
    PERFETTO_ELOG("%s", error.c_str());
  }
  return ok;
}
#endif

}  // namespace

bool RunAtrace(const std::vector<std::string>& args) {
  if (g_run_atrace_for_testing)
    return g_run_atrace_for_testing(args);
#if PERFETTO_BUILDFLAG(PERFETTO_OS_ANDROID)
  return ExecvAtrace(args);
#else
  PERFETTO_LOG("Atrace only supported on Android.");
  return false;
#endif
}

void SetRunAtraceForTesting(RunAtraceFunction f) {
  g_run_atrace_for_testing = f;
}

}  // namespace perfetto
