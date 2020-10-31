/*
 * Copyright (C) 2019 The Android Open Source Project
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

#include "src/profiling/perf/traced_perf.h"
#include "perfetto/ext/base/unix_task_runner.h"
#include "perfetto/ext/tracing/ipc/default_socket.h"
#include "src/profiling/perf/perf_producer.h"
#include "src/profiling/perf/proc_descriptors.h"

namespace perfetto {

namespace {
#if PERFETTO_BUILDFLAG(PERFETTO_ANDROID_BUILD)
static constexpr char kTracedPerfSocketEnvVar[] = "ANDROID_SOCKET_traced_perf";

int GetRawInheritedListeningSocket() {
  const char* sock_fd = getenv(kTracedPerfSocketEnvVar);
  if (sock_fd == nullptr)
    PERFETTO_FATAL("Did not inherit socket from init.");
  char* end;
  int raw_fd = static_cast<int>(strtol(sock_fd, &end, 10));
  if (*end != '\0')
    PERFETTO_FATAL("Invalid env variable format. Expected decimal integer.");
  return raw_fd;
}
#endif
}  // namespace

// TODO(rsavitski): watchdog.
int TracedPerfMain(int, char**) {
  base::UnixTaskRunner task_runner;

// TODO(rsavitski): support standalone --root or similar on android.
#if PERFETTO_BUILDFLAG(PERFETTO_ANDROID_BUILD)
  AndroidRemoteDescriptorGetter proc_fd_getter{GetRawInheritedListeningSocket(),
                                               &task_runner};
#else
  DirectDescriptorGetter proc_fd_getter;
#endif

  profiling::PerfProducer producer(&proc_fd_getter, &task_runner);
  producer.ConnectWithRetries(GetProducerSocket());
  task_runner.Run();
  return 0;
}

}  // namespace perfetto
