// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "experimental/redline/base/subprocess.h"

#include <fcntl.h>
#include <poll.h>
#include <spawn.h>
#include <sys/resource.h>
#include <sys/wait.h>
#include <unistd.h>

#include <algorithm>
#include <cerrno>
#include <csignal>
#include <cstring>

#include "experimental/redline/base/log.h"
#include "experimental/redline/base/time_util.h"

extern "C" char** environ;

namespace redline {

namespace {

// Reads from `fd` into `out` until EOF, keeping at most `limit` bytes.
// Returns false on EOF or error (i.e. when the fd should be closed).
bool DrainFd(int fd, std::string* out, size_t limit) {
  char buffer[65536];
  while (true) {
    ssize_t n = read(fd, buffer, sizeof(buffer));
    if (n < 0) {
      if (errno == EINTR) {
        continue;
      }
      return errno == EAGAIN;  // keep open only if it would block
    }
    if (n == 0) {
      return false;
    }
    if (out->size() < limit) {
      out->append(buffer, static_cast<size_t>(std::min<size_t>(
                              static_cast<size_t>(n), limit - out->size())));
    }
  }
}

void SetNonBlocking(int fd) {
  int flags = fcntl(fd, F_GETFL, 0);
  if (flags >= 0) {
    fcntl(fd, F_SETFL, flags | O_NONBLOCK);
  }
}

}  // namespace

SubprocessResult RunSubprocess(const std::vector<std::string>& argv,
                               int64_t timeout_ms,
                               const std::vector<std::string>& extra_env,
                               size_t max_output_bytes) {
  SubprocessResult result;
  if (argv.empty()) {
    result.error = "empty argv";
    return result;
  }

  int out_pipe[2], err_pipe[2];
  if (pipe2(out_pipe, O_CLOEXEC) != 0) {
    result.error = StringPrintf("pipe failed: %s", strerror(errno));
    return result;
  }
  if (pipe2(err_pipe, O_CLOEXEC) != 0) {
    int saved_errno = errno;
    close(out_pipe[0]);
    close(out_pipe[1]);
    result.error = StringPrintf("pipe failed: %s", strerror(saved_errno));
    return result;
  }

  std::vector<char*> c_argv;
  c_argv.reserve(argv.size() + 1);
  for (const std::string& arg : argv) {
    c_argv.push_back(const_cast<char*>(arg.c_str()));
  }
  c_argv.push_back(nullptr);

  std::vector<char*> c_env;
  for (char** e = environ; e != nullptr && *e != nullptr; e++) {
    c_env.push_back(*e);
  }
  for (const std::string& kv : extra_env) {
    c_env.push_back(const_cast<char*>(kv.c_str()));
  }
  c_env.push_back(nullptr);

  posix_spawn_file_actions_t actions;
  posix_spawn_file_actions_init(&actions);
  posix_spawn_file_actions_addopen(&actions, STDIN_FILENO, "/dev/null",
                                   O_RDONLY, 0);
  posix_spawn_file_actions_adddup2(&actions, out_pipe[1], STDOUT_FILENO);
  posix_spawn_file_actions_adddup2(&actions, err_pipe[1], STDERR_FILENO);

  posix_spawnattr_t attr;
  posix_spawnattr_init(&attr);
  sigset_t empty_set;
  sigemptyset(&empty_set);
  posix_spawnattr_setsigmask(&attr, &empty_set);
  sigset_t default_set;
  sigfillset(&default_set);
  posix_spawnattr_setsigdefault(&attr, &default_set);
  // A new process group so that a timeout kill takes helper processes too.
  posix_spawnattr_setpgroup(&attr, 0);
  posix_spawnattr_setflags(&attr, POSIX_SPAWN_SETSIGMASK |
                                      POSIX_SPAWN_SETSIGDEF |
                                      POSIX_SPAWN_SETPGROUP);

  pid_t pid = -1;
  Stopwatch stopwatch;
  int rc = posix_spawn(&pid, c_argv[0], &actions, &attr, c_argv.data(),
                       c_env.data());
  posix_spawn_file_actions_destroy(&actions);
  posix_spawnattr_destroy(&attr);
  close(out_pipe[1]);
  close(err_pipe[1]);

  if (rc != 0) {
    close(out_pipe[0]);
    close(err_pipe[0]);
    result.error = StringPrintf("posix_spawn(%s) failed: %s", argv[0].c_str(),
                                strerror(rc));
    return result;
  }
  // No core dumps from children (they may crash on purpose). Setting this on
  // the child pid avoids racing with other threads or suppressing core dumps
  // for redline itself.
  struct rlimit no_core = {0, 0};
  prlimit(pid, RLIMIT_CORE, &no_core, nullptr);
  result.started = true;
  SetNonBlocking(out_pipe[0]);
  SetNonBlocking(err_pipe[0]);

  int fds[2] = {out_pipe[0], err_pipe[0]};
  std::string* sinks[2] = {&result.stdout_data, &result.stderr_data};
  int open_count = 2;
  bool killed = false;
  while (open_count > 0) {
    struct pollfd pfds[2];
    int n = 0;
    for (int i = 0; i < 2; i++) {
      if (fds[i] >= 0) {
        pfds[n++] = {fds[i], POLLIN, 0};
      }
    }
    int64_t elapsed_ms = static_cast<int64_t>(stopwatch.ElapsedUs() / 1000);
    int wait_ms =
        killed
            ? 1000
            : static_cast<int>(std::max<int64_t>(0, timeout_ms - elapsed_ms));
    int ready = poll(pfds, static_cast<nfds_t>(n), wait_ms);
    if (ready < 0) {
      if (errno == EINTR) {
        continue;
      }
      break;
    }
    if (ready == 0) {
      if (killed) {
        break;  // give up waiting for output after a kill
      }
      // Timeout: kill the whole process group.
      kill(-pid, SIGKILL);
      kill(pid, SIGKILL);
      result.timed_out = true;
      killed = true;
      continue;
    }
    for (int i = 0, j = 0; i < 2; i++) {
      if (fds[i] < 0) {
        continue;
      }
      const struct pollfd& p = pfds[j++];
      if (p.revents == 0) {
        continue;
      }
      if (!DrainFd(fds[i], sinks[i], max_output_bytes)) {
        close(fds[i]);
        fds[i] = -1;
        open_count--;
      }
    }
  }
  for (int i = 0; i < 2; i++) {
    if (fds[i] >= 0) {
      close(fds[i]);
    }
  }

  int status = 0;
  while (waitpid(pid, &status, 0) < 0) {
    if (errno != EINTR) {
      break;
    }
  }
  result.elapsed_us = stopwatch.ElapsedUs();
  if (WIFEXITED(status)) {
    result.exit_code = WEXITSTATUS(status);
  } else if (WIFSIGNALED(status)) {
    result.signal = WTERMSIG(status);
  }
  return result;
}

}  // namespace redline
