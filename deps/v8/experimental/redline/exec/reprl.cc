// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "experimental/redline/exec/reprl.h"

#include <fcntl.h>
#include <poll.h>
#include <spawn.h>
#include <sys/mman.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

#include <algorithm>
#include <cerrno>
#include <csignal>
#include <cstring>
#include <mutex>

#include "experimental/redline/base/log.h"
#include "experimental/redline/base/time_util.h"

namespace redline {

namespace {

// Well-known file descriptor numbers on the child side.
constexpr int kChildCtrlIn = 100;
constexpr int kChildCtrlOut = 101;
constexpr int kChildDataIn = 102;
constexpr int kChildDataOut = 103;

// Maximum size of a data channel (and thus of a script / of captured
// output). Same value as Fuzzilli's REPRL_MAX_DATA_SIZE.
constexpr size_t kMaxDataSize = size_t{16} << 20;

// poll(2) takes an int number of milliseconds.
constexpr uint64_t kMaxTimeoutUs = uint64_t{INT32_MAX} * 1000;

constexpr const char* kLog = "reprl";

std::string Errno(const char* what) {
  return StringPrintf("%s: %s", what, strerror(errno));
}

bool ReadExact(int fd, void* dst, size_t len) {
  uint8_t* p = static_cast<uint8_t*>(dst);
  size_t remaining = len;
  while (remaining > 0) {
    ssize_t n = read(fd, p, remaining);
    if (n < 0) {
      if (errno == EINTR) {
        continue;
      }
      return false;
    }
    if (n == 0) {
      return false;
    }
    p += n;
    remaining -= static_cast<size_t>(n);
  }
  return true;
}

bool WriteExact(int fd, const void* src, size_t len) {
  const uint8_t* p = static_cast<const uint8_t*>(src);
  size_t remaining = len;
  while (remaining > 0) {
    ssize_t n = write(fd, p, remaining);
    if (n < 0) {
      if (errno == EINTR) {
        continue;
      }
      return false;
    }
    p += n;
    remaining -= static_cast<size_t>(n);
  }
  return true;
}

// Reserves the well-known fds once per process so that no other descriptor
// of ours can collide with them (which would break the dup2 dance during
// spawning), and ignores SIGPIPE since we may write to a pipe whose child
// has already died.
void InitializeProcessOnce() {
  static std::once_flag once;
  std::call_once(once, [] {
    int devnull = open("/dev/null", O_RDWR | O_CLOEXEC);
    if (devnull >= 0) {
      for (int fd :
           {kChildCtrlIn, kChildCtrlOut, kChildDataIn, kChildDataOut}) {
        dup3(devnull, fd, O_CLOEXEC);
      }
      close(devnull);
    }
    signal(SIGPIPE, SIG_IGN);
  });
}

}  // namespace

// A unidirectional channel for larger amounts of data, implemented as a
// RAM-backed file whose descriptor is shared with the child and which is
// mapped into our address space. Because the child's descriptor is a dup of
// ours, both share the file offset: the child simply read(2)s / write(2)s
// and we learn how much it wrote from lseek(SEEK_CUR).
class DataChannel {
 public:
  static std::unique_ptr<DataChannel> Create(const char* name,
                                             std::string* error) {
    int fd = memfd_create(name, MFD_CLOEXEC | MFD_ALLOW_SEALING);
    if (fd < 0) {
      *error = Errno("memfd_create");
      return nullptr;
    }
    if (ftruncate(fd, static_cast<off_t>(kMaxDataSize)) != 0) {
      *error = Errno("ftruncate");
      close(fd);
      return nullptr;
    }
    // Seal the size of the file to the size of our mapping. A child cannot
    // shrink the file, which would make the mapping fault with SIGBUS. A child
    // also cannot grow it: a write past the end fails with EPERM. Thus each
    // channel uses at most kMaxDataSize of memory.
    if (fcntl(fd, F_ADD_SEALS, F_SEAL_SHRINK | F_SEAL_GROW) != 0) {
      *error = Errno("fcntl(F_ADD_SEALS)");
      close(fd);
      return nullptr;
    }
    void* mapping =
        mmap(nullptr, kMaxDataSize, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (mapping == MAP_FAILED) {
      *error = Errno("mmap");
      close(fd);
      return nullptr;
    }
    return std::unique_ptr<DataChannel>(
        new DataChannel(fd, static_cast<char*>(mapping)));
  }

  ~DataChannel() {
    munmap(mapping_, kMaxDataSize);
    close(fd_);
  }

  int fd() const { return fd_; }
  char* mapping() { return mapping_; }

  void Rewind() { lseek(fd_, 0, SEEK_SET); }

  std::string_view Content() const {
    off_t pos = lseek(fd_, 0, SEEK_CUR);
    if (pos < 0) {
      pos = 0;
    }
    size_t size = std::min(static_cast<size_t>(pos), kMaxDataSize);
    return std::string_view(mapping_, size);
  }

 private:
  DataChannel(int fd, char* mapping) : fd_(fd), mapping_(mapping) {}
  int fd_;
  char* mapping_;
};

// static
std::unique_ptr<ReprlChild> ReprlChild::Create(Options options,
                                               std::string* error) {
  InitializeProcessOnce();
  std::unique_ptr<ReprlChild> child(new ReprlChild(std::move(options)));
  if (!child->Initialize(error)) {
    return nullptr;
  }
  return child;
}

ReprlChild::ReprlChild(Options options) : options_(std::move(options)) {}

ReprlChild::~ReprlChild() { Kill(); }

bool ReprlChild::Initialize(std::string* error) {
  if (options_.executable.empty()) {
    *error = "No target executable given";
    return false;
  }
  if (access(options_.executable.c_str(), X_OK) != 0) {
    *error = StringPrintf("Target executable %s is not executable: %s",
                          options_.executable.c_str(), strerror(errno));
    return false;
  }

  argv_storage_.push_back(options_.executable);
  for (const std::string& arg : options_.args) {
    argv_storage_.push_back(arg);
  }
  for (std::string& s : argv_storage_) {
    argv_.push_back(s.data());
  }
  argv_.push_back(nullptr);

  for (const auto& [key, value] : options_.env) {
    std::string entry = key;
    entry += '=';
    entry += value;
    env_storage_.push_back(std::move(entry));
  }
  for (std::string& s : env_storage_) {
    envp_.push_back(s.data());
  }
  envp_.push_back(nullptr);

  script_channel_ = DataChannel::Create("redline_reprl_script", error);
  if (!script_channel_) {
    return false;
  }
  fuzzout_channel_ = DataChannel::Create("redline_reprl_fuzzout", error);
  if (!fuzzout_channel_) {
    return false;
  }
  if (options_.capture_stdout) {
    stdout_channel_ = DataChannel::Create("redline_reprl_stdout", error);
    if (!stdout_channel_) {
      return false;
    }
  }
  if (options_.capture_stderr) {
    stderr_channel_ = DataChannel::Create("redline_reprl_stderr", error);
    if (!stderr_channel_) {
      return false;
    }
  }
  return true;
}

void ReprlChild::OnChildTerminated() {
  if (pid_ == 0) {
    return;
  }
  pid_ = 0;
  close(ctrl_in_);
  close(ctrl_out_);
  ctrl_in_ = ctrl_out_ = -1;
}

void ReprlChild::Kill() {
  if (pid_ == 0) {
    return;
  }
  kill(-pid_, SIGKILL);
  kill(pid_, SIGKILL);
  int status;
  while (waitpid(pid_, &status, 0) < 0 && errno == EINTR) {
  }
  OnChildTerminated();
}

bool ReprlChild::Spawn(std::string* error) {
  int child_to_parent[2] = {-1, -1};
  int parent_to_child[2] = {-1, -1};
  if (pipe2(child_to_parent, O_CLOEXEC) != 0) {
    *error = Errno("pipe2");
    return false;
  }
  if (pipe2(parent_to_child, O_CLOEXEC) != 0) {
    *error = Errno("pipe2");
    close(child_to_parent[0]);
    close(child_to_parent[1]);
    return false;
  }

  // posix_spawn uses vfork semantics on Linux, which is what makes
  // respawning cheap. All descriptors we own are O_CLOEXEC, so only the
  // explicitly dup2'ed ones survive the exec.
  posix_spawn_file_actions_t actions;
  posix_spawn_file_actions_init(&actions);
  posix_spawn_file_actions_adddup2(&actions, parent_to_child[0], kChildCtrlIn);
  posix_spawn_file_actions_adddup2(&actions, child_to_parent[1], kChildCtrlOut);
  posix_spawn_file_actions_adddup2(&actions, script_channel_->fd(),
                                   kChildDataIn);
  posix_spawn_file_actions_adddup2(&actions, fuzzout_channel_->fd(),
                                   kChildDataOut);
  posix_spawn_file_actions_addopen(&actions, STDIN_FILENO, "/dev/null",
                                   O_RDONLY, 0);
  if (stdout_channel_) {
    posix_spawn_file_actions_adddup2(&actions, stdout_channel_->fd(),
                                     STDOUT_FILENO);
  } else {
    posix_spawn_file_actions_addopen(&actions, STDOUT_FILENO, "/dev/null",
                                     O_WRONLY, 0);
  }
  if (stderr_channel_) {
    posix_spawn_file_actions_adddup2(&actions, stderr_channel_->fd(),
                                     STDERR_FILENO);
  } else {
    posix_spawn_file_actions_addopen(&actions, STDERR_FILENO, "/dev/null",
                                     O_WRONLY, 0);
  }

  posix_spawnattr_t attr;
  posix_spawnattr_init(&attr);
  // Start the child with a clean signal mask and default dispositions
  // (we ignore SIGPIPE ourselves, the child should not inherit that).
  sigset_t empty_set;
  sigemptyset(&empty_set);
  posix_spawnattr_setsigmask(&attr, &empty_set);
  sigset_t default_set;
  sigemptyset(&default_set);
  sigaddset(&default_set, SIGPIPE);
  posix_spawnattr_setsigdefault(&attr, &default_set);
  posix_spawnattr_setpgroup(&attr, 0);
  posix_spawnattr_setflags(&attr, POSIX_SPAWN_SETSIGMASK |
                                      POSIX_SPAWN_SETSIGDEF |
                                      POSIX_SPAWN_SETPGROUP);

  pid_t pid = 0;
  int rc =
      posix_spawn(&pid, argv_[0], &actions, &attr, argv_.data(), envp_.data());
  posix_spawn_file_actions_destroy(&actions);
  posix_spawnattr_destroy(&attr);
  close(child_to_parent[1]);
  close(parent_to_child[0]);

  if (rc != 0) {
    close(child_to_parent[0]);
    close(parent_to_child[1]);
    *error = StringPrintf("posix_spawn(%s) failed: %s", argv_[0], strerror(rc));
    return false;
  }

  pid_ = pid;
  ctrl_in_ = child_to_parent[0];
  ctrl_out_ = parent_to_child[1];
  execs_since_spawn_ = 0;
  total_spawns_++;

  // No core dumps from the target: with crashes being the point of the
  // exercise they would fill the disk. Doing this from the parent keeps
  // core dumps enabled for redline itself.
  struct rlimit no_core = {0, 0};
  if (prlimit(pid_, RLIMIT_CORE, &no_core, nullptr) != 0) {
    REDLINE_WARN(kLog, "prlimit(RLIMIT_CORE) failed: %s", strerror(errno));
  }

  // HELO handshake.
  uint64_t startup_deadline = MonotonicNowUs() + options_.startup_timeout_us;
  int res = 0;
  while (true) {
    uint64_t now = MonotonicNowUs();
    int remaining_ms =
        now >= startup_deadline
            ? 0
            : static_cast<int>(std::min<uint64_t>(
                  (startup_deadline - now + 999) / 1000, INT32_MAX));
    struct pollfd pfd = {ctrl_in_, POLLIN, 0};
    res = poll(&pfd, 1, remaining_ms);
    if (res < 0 && errno == EINTR) {
      continue;
    }
    break;
  }
  char helo[4] = {0, 0, 0, 0};
  if (res != 1 || !ReadExact(ctrl_in_, helo, 4)) {
    std::string output;
    if (stderr_channel_) {
      output = std::string(stderr_channel_->Content());
    }
    Kill();
    *error = StringPrintf(
        "Did not receive HELO from target %s (%s). Is it built with "
        "v8_fuzzilli=true? stderr: %s",
        argv_[0], res == 0 ? "timeout" : "child exited", output.c_str());
    return false;
  }
  if (memcmp(helo, "HELO", 4) != 0) {
    Kill();
    *error = "Received invalid HELO message from target";
    return false;
  }
  if (!WriteExact(ctrl_out_, "HELO", 4)) {
    Kill();
    *error = Errno("Failed to send HELO reply");
    return false;
  }

  // Detect targets that re-enable core dumps themselves.
  struct rlimit core_limit = {0, 0};
  if (prlimit(pid_, RLIMIT_CORE, nullptr, &core_limit) == 0 &&
      (core_limit.rlim_cur != 0 || core_limit.rlim_max != 0)) {
    Kill();
    *error = "Target raised RLIMIT_CORE; refusing to fuzz it";
    return false;
  }
  return true;
}

void ReprlChild::CollectOutput(Execution* result) {
  result->fuzzout = std::string(fuzzout_channel_->Content());
  if (stdout_channel_) {
    result->stdout_output = std::string(stdout_channel_->Content());
  }
  if (stderr_channel_) {
    result->stderr_output = std::string(stderr_channel_->Content());
  }
}

bool ReprlChild::Execute(std::string_view script, uint64_t timeout_us,
                         Execution* result, std::string* error) {
  *result = Execution();
  if (script.size() > kMaxDataSize) {
    *error = StringPrintf("Script too large (%zu bytes)", script.size());
    return false;
  }
  if (timeout_us > kMaxTimeoutUs) {
    *error = "Timeout too large";
    return false;
  }

  if (pid_ != 0 && options_.max_execs_before_respawn > 0 &&
      execs_since_spawn_ >= options_.max_execs_before_respawn) {
    Kill();
  }

  // Reset the file positions so the child can simply read(2) and write(2).
  script_channel_->Rewind();
  fuzzout_channel_->Rewind();
  if (stdout_channel_) {
    stdout_channel_->Rewind();
  }
  if (stderr_channel_) {
    stderr_channel_->Rewind();
  }

  if (pid_ == 0 && !Spawn(error)) {
    return false;
  }

  memcpy(script_channel_->mapping(), script.data(), script.size());
  uint64_t script_size = script.size();
  if (!WriteExact(ctrl_out_, "exec", 4) ||
      !WriteExact(ctrl_out_, &script_size, 8)) {
    // The child may have died between executions.
    int status = 0;
    if (waitpid(pid_, &status, WNOHANG) == pid_) {
      OnChildTerminated();
      if (WIFEXITED(status)) {
        *error = StringPrintf(
            "Child unexpectedly exited with status %d between executions",
            WEXITSTATUS(status));
      } else {
        *error = StringPrintf(
            "Child unexpectedly terminated with signal %d between executions",
            WTERMSIG(status));
      }
      return false;
    }
    std::string message = Errno("Failed to send command to child");
    Kill();
    *error = message;
    return false;
  }

  total_executions_++;
  execs_since_spawn_++;

  // Wait for the status word, EOF (child died) or the timeout.
  uint64_t start = MonotonicNowUs();
  uint64_t deadline = start + timeout_us;
  int res;
  while (true) {
    uint64_t now = MonotonicNowUs();
    int remaining_ms = now >= deadline
                           ? 0
                           : static_cast<int>(std::min<uint64_t>(
                                 (deadline - now + 999) / 1000, INT32_MAX));
    struct pollfd pfd = {ctrl_in_, POLLIN, 0};
    res = poll(&pfd, 1, remaining_ms);
    if (res < 0 && errno == EINTR) {
      continue;
    }
    break;
  }
  result->exec_time_us = MonotonicNowUs() - start;

  if (res == 0) {
    Kill();
    result->outcome = Outcome::kTimedOut;
    CollectOutput(result);
    return true;
  }
  if (res < 0) {
    std::string message = Errno("poll");
    Kill();
    *error = message;
    return false;
  }

  int status = 0;
  ssize_t rv = 0;
  do {
    rv = read(ctrl_in_, &status, 4);
  } while (rv < 0 && errno == EINTR);
  if (rv < 0) {
    std::string message = Errno("Failed to read from control pipe");
    Kill();
    *error = message;
    return false;
  }
  if (rv != 4) {
    // EOF: most likely the child crashed. Reap it; waitpid may not succeed
    // immediately after the pipe closed, so retry briefly.
    int wait_status = 0;
    bool reaped = false;
    uint64_t wait_deadline =
        MonotonicNowUs() + std::max<uint64_t>(timeout_us, 1000000);
    while (MonotonicNowUs() < wait_deadline) {
      pid_t r = waitpid(pid_, &wait_status, WNOHANG);
      if (r == pid_) {
        reaped = true;
        break;
      }
      if (r < 0 && errno != EINTR) {
        break;
      }
      usleep(10);
    }
    if (!reaped) {
      Kill();
      *error = "Child in unexpected state after execution";
      return false;
    }
    OnChildTerminated();
    if (WIFEXITED(wait_status)) {
      status = WEXITSTATUS(wait_status) << 8;
    } else if (WIFSIGNALED(wait_status)) {
      status = WTERMSIG(wait_status);
    } else {
      *error = StringPrintf("waitpid returned unexpected child state %d",
                            wait_status);
      return false;
    }
  }
  if (status < 0) {
    Kill();
    *error = StringPrintf("Child reported invalid status %d", status);
    return false;
  }

  status &= 0xffff;
  int signal_number = status & 0xff;
  int exit_code = (status >> 8) & 0xff;
  if (signal_number != 0) {
    result->outcome = Outcome::kCrashed;
    result->signal = signal_number;
    // A child that reported a signal in-band cannot be trusted anymore.
    if (pid_ != 0) {
      Kill();
    }
  } else {
    result->exit_code = exit_code;
    result->outcome = exit_code == 0 ? Outcome::kSucceeded : Outcome::kFailed;
  }
  CollectOutput(result);
  return true;
}

}  // namespace redline
