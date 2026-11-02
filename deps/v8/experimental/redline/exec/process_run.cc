// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "experimental/redline/exec/process_run.h"

#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <spawn.h>
#include <sys/resource.h>
#include <sys/wait.h>
#include <unistd.h>

#include <algorithm>
#include <cerrno>
#include <cstring>

#include "experimental/redline/base/time_util.h"

namespace redline {

ProcessResult RunProcess(
    const std::string& executable, const std::vector<std::string>& args,
    const std::vector<std::pair<std::string, std::string>>& env,
    uint64_t timeout_us, size_t max_stderr) {
  ProcessResult result;
  int err_pipe[2];
  if (pipe2(err_pipe, O_CLOEXEC) != 0) {
    result.error = std::string("pipe2: ") + strerror(errno);
    return result;
  }

  std::vector<std::string> argv_storage;
  argv_storage.reserve(args.size() + 1);
  argv_storage.push_back(executable);
  argv_storage.insert(argv_storage.end(), args.begin(), args.end());
  std::vector<char*> argv;
  argv.reserve(argv_storage.size() + 1);
  for (std::string& s : argv_storage) {
    argv.push_back(s.data());
  }
  argv.push_back(nullptr);
  std::vector<std::string> env_storage;
  env_storage.reserve(env.size());
  for (const auto& [key, value] : env) {
    std::string entry = key;
    entry += '=';
    entry += value;
    env_storage.push_back(std::move(entry));
  }
  std::vector<char*> envp;
  envp.reserve(env_storage.size() + 1);
  for (std::string& s : env_storage) {
    envp.push_back(s.data());
  }
  envp.push_back(nullptr);

  posix_spawn_file_actions_t actions;
  posix_spawn_file_actions_init(&actions);
  posix_spawn_file_actions_addopen(&actions, STDIN_FILENO, "/dev/null",
                                   O_RDONLY, 0);
  posix_spawn_file_actions_addopen(&actions, STDOUT_FILENO, "/dev/null",
                                   O_WRONLY, 0);
  posix_spawn_file_actions_adddup2(&actions, err_pipe[1], STDERR_FILENO);

  posix_spawnattr_t attr;
  posix_spawnattr_init(&attr);
  sigset_t empty_set;
  sigemptyset(&empty_set);
  posix_spawnattr_setsigmask(&attr, &empty_set);
  sigset_t default_set;
  sigfillset(&default_set);
  posix_spawnattr_setsigdefault(&attr, &default_set);
  posix_spawnattr_setpgroup(&attr, 0);
  posix_spawnattr_setflags(&attr, POSIX_SPAWN_SETSIGMASK |
                                      POSIX_SPAWN_SETSIGDEF |
                                      POSIX_SPAWN_SETPGROUP);

  pid_t pid = 0;
  const Stopwatch clock;
  const int rc = posix_spawn(&pid, executable.c_str(), &actions, &attr,
                             argv.data(), envp.data());
  posix_spawn_file_actions_destroy(&actions);
  posix_spawnattr_destroy(&attr);
  close(err_pipe[1]);
  if (rc != 0) {
    close(err_pipe[0]);
    result.error = std::string("posix_spawn: ") + strerror(rc);
    return result;
  }
  struct rlimit no_core = {0, 0};
  prlimit(pid, RLIMIT_CORE, &no_core, nullptr);
  result.started = true;

  // Read stderr until EOF or the timeout; keep at most max_stderr bytes.
  char buffer[8192];
  bool open = true;
  while (open) {
    const uint64_t elapsed = clock.ElapsedUs();
    if (elapsed >= timeout_us) {
      result.timed_out = true;
      kill(-pid, SIGKILL);
      kill(pid, SIGKILL);
      break;
    }
    const uint64_t left_ms = (timeout_us - elapsed + 999) / 1000;
    struct pollfd pfd = {err_pipe[0], POLLIN, 0};
    const int ready =
        poll(&pfd, 1, static_cast<int>(std::min<uint64_t>(left_ms, 1000)));
    if (ready < 0 && errno != EINTR) {
      break;
    }
    if (ready <= 0) {
      continue;
    }
    const ssize_t n = read(err_pipe[0], buffer, sizeof(buffer));
    if (n > 0) {
      const size_t room =
          max_stderr - std::min(max_stderr, result.stderr_text.size());
      result.stderr_text.append(buffer, std::min(room, static_cast<size_t>(n)));
    } else if (n == 0 || errno != EINTR) {
      open = false;
    }
  }
  close(err_pipe[0]);

  int status = 0;
  while (waitpid(pid, &status, 0) < 0 && errno == EINTR) {
  }
  result.wall_us = clock.ElapsedUs();
  if (WIFSIGNALED(status)) {
    result.signal = WTERMSIG(status);
  } else if (WIFEXITED(status)) {
    result.exit_code = WEXITSTATUS(status);
  }
  return result;
}

}  // namespace redline
