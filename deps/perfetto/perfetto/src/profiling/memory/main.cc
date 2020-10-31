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

#include <stdlib.h>
#include <unistd.h>
#include <array>
#include <memory>
#include <vector>

#include <getopt.h>
#include <signal.h>

#include "perfetto/ext/base/event_fd.h"
#include "perfetto/ext/base/scoped_file.h"
#include "perfetto/ext/base/unix_socket.h"
#include "perfetto/ext/base/watchdog.h"
#include "perfetto/ext/tracing/ipc/default_socket.h"
#include "src/profiling/memory/heapprofd_producer.h"
#include "src/profiling/memory/java_hprof_producer.h"
#include "src/profiling/memory/wire_protocol.h"

#include "perfetto/ext/base/unix_task_runner.h"

// TODO(rsavitski): the task runner watchdog spawns a thread (normally for
// tracking cpu/mem usage) that we don't strictly need.

namespace perfetto {
namespace profiling {
namespace {

int StartChildHeapprofd(pid_t target_pid,
                        std::string target_cmdline,
                        base::ScopedFile inherited_sock_fd);
int StartCentralHeapprofd();

int GetListeningSocket() {
  const char* sock_fd = getenv(kHeapprofdSocketEnvVar);
  if (sock_fd == nullptr)
    PERFETTO_FATAL("Did not inherit socket from init.");
  char* end;
  int raw_fd = static_cast<int>(strtol(sock_fd, &end, 10));
  if (*end != '\0')
    PERFETTO_FATAL("Invalid %s. Expected decimal integer.",
                   kHeapprofdSocketEnvVar);
  return raw_fd;
}

base::EventFd* g_dump_evt = nullptr;

int HeapprofdMain(int argc, char** argv) {
  bool cleanup_crash = false;
  pid_t target_pid = base::kInvalidPid;
  std::string target_cmdline;
  base::ScopedFile inherited_sock_fd;

  enum { kCleanupCrash = 256, kTargetPid, kTargetCmd, kInheritFd };
  static struct option long_options[] = {
      {"cleanup-after-crash", no_argument, nullptr, kCleanupCrash},
      {"exclusive-for-pid", required_argument, nullptr, kTargetPid},
      {"exclusive-for-cmdline", required_argument, nullptr, kTargetCmd},
      {"inherit-socket-fd", required_argument, nullptr, kInheritFd},
      {nullptr, 0, nullptr, 0}};
  int option_index;
  int c;
  while ((c = getopt_long(argc, argv, "", long_options, &option_index)) != -1) {
    switch (c) {
      case kCleanupCrash:
        cleanup_crash = true;
        break;
      case kTargetPid:
        if (target_pid != base::kInvalidPid)
          PERFETTO_FATAL("Duplicate exclusive-for-pid");
        target_pid = static_cast<pid_t>(atoi(optarg));
        break;
      case kTargetCmd:  // assumed to be already normalized
        if (!target_cmdline.empty())
          PERFETTO_FATAL("Duplicate exclusive-for-cmdline");
        target_cmdline = std::string(optarg);
        break;
      case kInheritFd:  // repetition not supported
        if (inherited_sock_fd)
          PERFETTO_FATAL("Duplicate inherit-socket-fd");
        inherited_sock_fd = base::ScopedFile(atoi(optarg));
        break;
      default:
        PERFETTO_ELOG("Usage: %s [--cleanup-after-crash]", argv[0]);
        return 1;
    }
  }

  if (cleanup_crash) {
    PERFETTO_LOG(
        "Recovering from crash: unsetting heapprofd system properties. "
        "Expect SELinux denials for unrelated properties.");
    SystemProperties::ResetHeapprofdProperties();
    PERFETTO_LOG(
        "Finished unsetting heapprofd system properties. "
        "SELinux denials about properties are unexpected after "
        "this point.");
    return 0;
  }

  // If |target_pid| is given, we're supposed to be operating as a private
  // heapprofd for that process. Note that we might not be a direct child due to
  // reparenting.
  bool tpid_set = target_pid != base::kInvalidPid;
  bool tcmd_set = !target_cmdline.empty();
  bool fds_set = !!inherited_sock_fd;
  if (tpid_set || tcmd_set || fds_set) {
    if (!tpid_set || !tcmd_set || !fds_set) {
      PERFETTO_ELOG(
          "If starting in child mode, requires all of: {--exclusive-for-pid, "
          "--exclusive-for-cmdline, --inherit_socket_fd}");
      return 1;
    }

    return StartChildHeapprofd(target_pid, target_cmdline,
                               std::move(inherited_sock_fd));
  }

  // Otherwise start as a central daemon.
  return StartCentralHeapprofd();
}

int StartChildHeapprofd(pid_t target_pid,
                        std::string target_cmdline,
                        base::ScopedFile inherited_sock_fd) {
  base::UnixTaskRunner task_runner;
  base::Watchdog::GetInstance()->Start();  // crash on exceedingly long tasks
  HeapprofdProducer producer(HeapprofdMode::kChild, &task_runner);
  producer.SetTargetProcess(target_pid, target_cmdline,
                            std::move(inherited_sock_fd));
  producer.ConnectWithRetries(GetProducerSocket());
  producer.ScheduleActiveDataSourceWatchdog();
  task_runner.Run();
  return 0;
}

int StartCentralHeapprofd() {
  // We set this up before launching any threads, so we do not have to use a
  // std::atomic for g_dump_evt.
  g_dump_evt = new base::EventFd();

  base::UnixTaskRunner task_runner;
  base::Watchdog::GetInstance()->Start();  // crash on exceedingly long tasks
  HeapprofdProducer producer(HeapprofdMode::kCentral, &task_runner);

  int listening_raw_socket = GetListeningSocket();
  auto listening_socket = base::UnixSocket::Listen(
      base::ScopedFile(listening_raw_socket), &producer.socket_delegate(),
      &task_runner, base::SockFamily::kUnix, base::SockType::kStream);

  struct sigaction action = {};
  action.sa_handler = [](int) { g_dump_evt->Notify(); };
  // Allow to trigger a full dump by sending SIGUSR1 to heapprofd.
  // This will allow manually deciding when to dump on userdebug.
  PERFETTO_CHECK(sigaction(SIGUSR1, &action, nullptr) == 0);
  task_runner.AddFileDescriptorWatch(g_dump_evt->fd(), [&producer] {
    g_dump_evt->Clear();
    producer.DumpAll();
  });
  producer.ConnectWithRetries(GetProducerSocket());
  // TODO(fmayer): Create one producer that manages both heapprofd and Java
  // producers, so we do not have two connections to traced.
  JavaHprofProducer java_producer(&task_runner);
  java_producer.ConnectWithRetries(GetProducerSocket());
  task_runner.Run();
  return 0;
}

}  // namespace
}  // namespace profiling
}  // namespace perfetto

int main(int argc, char** argv) {
  return perfetto::profiling::HeapprofdMain(argc, argv);
}
