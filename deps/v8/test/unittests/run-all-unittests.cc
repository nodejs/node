// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cstring>
#include <memory>
#include <string>

#if defined(_WIN32)
#include <windows.h>
#endif

#include "include/cppgc/platform.h"
#include "include/libplatform/libplatform.h"
#include "include/v8-initialization.h"
#include "include/v8config.h"
#include "src/base/compiler-specific.h"
#include "src/base/logging.h"
#include "src/base/page-allocator.h"
#include "testing/gmock/include/gmock/gmock.h"

#if defined(V8_OS_POSIX) && !defined(V8_OS_FUCHSIA) && !defined(V8_OS_IOS) && \
    !defined(THREAD_SANITIZER)
#define V8_SUPPORTS_FORK_SERVER 1
#include <fcntl.h>
#include <poll.h>
#include <sys/wait.h>
#include <unistd.h>

#include <cerrno>
#include <csignal>
#endif

#ifdef V8_ENABLE_FUZZTEST
#include "test/unittests/fuzztest-init-adapter.h"
#endif  // V8_ENABLE_FUZZTEST

#ifdef V8_USE_PERFETTO
#include "src/tracing/trace-event.h"
#endif  // V8_USE_PERFETTO

namespace {

class CppGCEnvironment final : public ::testing::Environment {
 public:
  void SetUp() override {
    // Initialize the process for cppgc with an arbitrary page allocator. This
    // has to survive as long as the process, so it's ok to leak the allocator
    // here.
    cppgc::InitializeProcess(new v8::base::PageAllocator());

#ifdef V8_USE_PERFETTO
    // Set up the in-process perfetto backend.
    perfetto::TracingInitArgs init_args;
    init_args.backends = perfetto::BackendType::kInProcessBackend;
    perfetto::Tracing::Initialize(init_args);
#endif  // V8_USE_PERFETTO
  }

  void TearDown() override { cppgc::ShutdownProcess(); }
};

bool ExtractForkServerFlag(int* argc, char** argv) {
  if (*argc <= 1) return false;
  bool found = false;
  int write_idx = 1;
  for (int read_idx = 1; read_idx < *argc; ++read_idx) {
    if (strcmp(argv[read_idx], "--fork-server") == 0) {
      found = true;
    } else {
      argv[write_idx++] = argv[read_idx];
    }
  }
  argv[write_idx] = nullptr;
  *argc = write_idx;
  return found;
}

#ifdef V8_SUPPORTS_FORK_SERVER

bool WriteAll(int fd, const void* data, size_t len) {
  const char* ptr = static_cast<const char*>(data);
  size_t remaining = len;
  while (remaining > 0) {
    ssize_t written = write(fd, ptr, remaining);
    if (written < 0) {
      if (errno == EINTR) continue;
      return false;
    }
    ptr += written;
    remaining -= static_cast<size_t>(written);
  }
  return true;
}

bool ReadLine(int fd, std::string* line) {
  line->clear();
  char ch;
  while (true) {
    ssize_t n = read(fd, &ch, 1);
    if (n < 0) {
      if (errno == EINTR) continue;
      return false;
    }
    if (n == 0) {
      return false;
    }
    if (ch == '\n') {
      return true;
    }
    line->push_back(ch);
  }
}

bool DrainPipes(int ctrl_fd, int out_fd, int err_fd, std::string* out_str,
                std::string* err_str) {
  struct pollfd fds[3] = {
      {out_fd, POLLIN, 0}, {err_fd, POLLIN, 0}, {ctrl_fd, POLLIN, 0}};
  int open_fds = 2;
  char buf[4096];
  while (open_fds > 0) {
    if (poll(fds, 3, -1) < 0) {
      if (errno == EINTR) continue;
      break;
    }
    if (fds[2].revents) return false;
    for (int i = 0; i < 2; ++i) {
      if (fds[i].fd < 0 || !fds[i].revents) continue;
      ssize_t n = read(fds[i].fd, buf, sizeof(buf));
      if (n > 0) {
        (i == 0 ? out_str : err_str)->append(buf, static_cast<size_t>(n));
      } else if (n == 0 || errno != EINTR) {
        fds[i].fd = -1;
        --open_fds;
      }
    }
  }
  return true;
}

// Runs a persistent fork-server loop over the original stdin/stdout pipes:
//   Request (on stdin):   "<gtest_filter>\n"
//   Response (on stdout): "RES <exit_code> <stdout_len> <stderr_len>\n"
//                         followed by <stdout_len> raw stdout bytes and
//                         <stderr_len> raw stderr bytes.
//   <exit_code> is the child's exit status, or -<signal> if terminated by a
//   signal (matching Python's subprocess.Popen.returncode convention).
//   The protocol is synchronous (one request in flight at a time). Any
//   activity or EOF on the control pipe while a child is running indicates
//   that the parent testrunner aborted or died and makes the server kill
//   the child and exit without a response (hence e.g.
//   "echo Foo.Bar | v8_unittests --fork-server" produces no output).
int RunForkServer() {
  fflush(stdout);
  fflush(stderr);

  int ctrl_in_fd = fcntl(STDIN_FILENO, F_DUPFD_CLOEXEC, 0);
  int ctrl_out_fd = fcntl(STDOUT_FILENO, F_DUPFD_CLOEXEC, 0);
  CHECK_GE(ctrl_in_fd, 0);
  CHECK_GE(ctrl_out_fd, 0);

  int devnull = open("/dev/null", O_RDWR);
  CHECK_GE(devnull, 0);
  CHECK_NE(dup2(devnull, STDIN_FILENO), -1);
  CHECK_NE(dup2(devnull, STDOUT_FILENO), -1);
  close(devnull);

  std::string test_filter;
  while (ReadLine(ctrl_in_fd, &test_filter)) {
    if (test_filter.empty()) continue;

    int out_pipe[2];
    int err_pipe[2];
    CHECK_EQ(pipe(out_pipe), 0);
    CHECK_EQ(pipe(err_pipe), 0);

    pid_t pid = fork();
    CHECK_GE(pid, 0);
    if (pid == 0) {
      close(ctrl_in_fd);
      close(ctrl_out_fd);
      close(out_pipe[0]);
      close(err_pipe[0]);
      CHECK_NE(dup2(out_pipe[1], STDOUT_FILENO), -1);
      CHECK_NE(dup2(err_pipe[1], STDERR_FILENO), -1);
      close(out_pipe[1]);
      close(err_pipe[1]);

      GTEST_FLAG_SET(filter, test_filter);
      return RUN_ALL_TESTS();
    }

    close(out_pipe[1]);
    close(err_pipe[1]);

    std::string stdout_data;
    std::string stderr_data;
    bool parent_alive = DrainPipes(ctrl_in_fd, out_pipe[0], err_pipe[0],
                                   &stdout_data, &stderr_data);
    close(out_pipe[0]);
    close(err_pipe[0]);

    if (!parent_alive) {
      if (getpgrp() == getpid()) {
        // Under the testrunner, the server is always its own process group
        // leader (start_new_session). Kill the whole group, including the
        // child, any grandchildren and ourselves.
        kill(0, SIGKILL);
      } else {
        kill(pid, SIGKILL);
        while (waitpid(pid, nullptr, 0) < 0 && errno == EINTR) {
        }
      }
      break;
    }

    int status = 0;
    pid_t waited;
    while ((waited = waitpid(pid, &status, 0)) < 0 && errno == EINTR) {
    }

    int exit_code = -1;
    if (waited == pid) {
      if (WIFEXITED(status)) {
        exit_code = WEXITSTATUS(status);
      } else if (WIFSIGNALED(status)) {
        exit_code = -WTERMSIG(status);
      }
    }

    std::string header = "RES " + std::to_string(exit_code) + " " +
                         std::to_string(stdout_data.size()) + " " +
                         std::to_string(stderr_data.size()) + "\n";
    if (!WriteAll(ctrl_out_fd, header.data(), header.size()) ||
        !WriteAll(ctrl_out_fd, stdout_data.data(), stdout_data.size()) ||
        !WriteAll(ctrl_out_fd, stderr_data.data(), stderr_data.size())) {
      break;
    }
  }

  close(ctrl_in_fd);
  close(ctrl_out_fd);
  // The server itself never ran a test, so skip static destructors and
  // sanitizer exit hooks (e.g. LSan). Children return from RUN_ALL_TESTS().
  _exit(0);
}

#endif  // V8_SUPPORTS_FORK_SERVER

}  // namespace


int main(int argc, char** argv) {
#if defined(_WIN32)
  // Preload these DLLs before symbolization can reenter ASAN's allocator.
  ::LoadLibraryW(L"dbghelp.dll");
  ::LoadLibraryW(L"msdia140.dll");
#endif
  // Strip --fork-server before InitGoogleMock records argv for death tests.
  bool use_fork_server = ExtractForkServerFlag(&argc, argv);

  // Don't catch SEH exceptions and continue as the following tests might hang
  // in an broken environment on windows.
  GTEST_FLAG_SET(catch_exceptions, false);

  // Most V8 unit-tests are multi-threaded, so enable thread-safe death-tests.
  GTEST_FLAG_SET(death_test_style, "threadsafe");

  testing::InitGoogleMock(&argc, argv);
  testing::AddGlobalTestEnvironment(new CppGCEnvironment);

#ifdef V8_ENABLE_SANDBOX_HARDWARE_SUPPORT
  v8::SandboxHardwareSupport::InitializeBeforeThreadCreation();
#endif  // V8_ENABLE_SANDBOX_HARDWARE_SUPPORT

  v8::V8::SetFlagsFromCommandLine(&argc, argv, true);
  v8::V8::InitializeExternalStartupData(argv[0]);
  CHECK(v8::V8::InitializeICUDefaultLocation(argv[0]));

#ifdef V8_ENABLE_FUZZTEST
  absl::ParseCommandLine(argc, argv);
  fuzztest::InitFuzzTest(&argc, &argv);
#endif  // V8_ENABLE_FUZZTEST

  if (use_fork_server) {
#ifdef V8_SUPPORTS_FORK_SERVER
    return RunForkServer();
#else
    FATAL("--fork-server is not supported on this build");
#endif  // V8_SUPPORTS_FORK_SERVER
  }

  return RUN_ALL_TESTS();
}
