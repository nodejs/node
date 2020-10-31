/*
 * Copyright (C) 2017 The Android Open Source Project
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

#include <getopt.h>
#include <stdio.h>

#include "perfetto/ext/base/unix_task_runner.h"
#include "perfetto/ext/base/watchdog.h"
#include "perfetto/ext/traced/traced.h"
#include "perfetto/ext/tracing/ipc/default_socket.h"
#include "perfetto/ext/tracing/ipc/service_ipc_host.h"
#include "src/traced/service/builtin_producer.h"

#if PERFETTO_BUILDFLAG(PERFETTO_VERSION_GEN)
#include "perfetto_version.gen.h"
#else
#define PERFETTO_GET_GIT_REVISION() "unknown"
#endif

namespace perfetto {

int __attribute__((visibility("default"))) ServiceMain(int argc, char** argv) {
  enum LongOption {
    OPT_VERSION = 1000,
  };

  static const struct option long_options[] = {
      {"version", no_argument, nullptr, OPT_VERSION},
      {nullptr, 0, nullptr, 0}};

  int option_index;
  for (;;) {
    int option = getopt_long(argc, argv, "", long_options, &option_index);
    if (option == -1)
      break;
    switch (option) {
      case OPT_VERSION:
        printf("%s\n", PERFETTO_GET_GIT_REVISION());
        return 0;
      default:
        PERFETTO_ELOG("Usage: %s [--version]", argv[0]);
        return 1;
    }
  }

  base::UnixTaskRunner task_runner;
  std::unique_ptr<ServiceIPCHost> svc;
  svc = ServiceIPCHost::CreateInstance(&task_runner);

  // When built as part of the Android tree, the two socket are created and
  // bound by init and their fd number is passed in two env variables.
  // See libcutils' android_get_control_socket().
  const char* env_prod = getenv("ANDROID_SOCKET_traced_producer");
  const char* env_cons = getenv("ANDROID_SOCKET_traced_consumer");
  PERFETTO_CHECK((!env_prod && !env_cons) || (env_prod && env_cons));
  bool started;
  if (env_prod) {
    base::ScopedFile producer_fd(atoi(env_prod));
    base::ScopedFile consumer_fd(atoi(env_cons));
    started = svc->Start(std::move(producer_fd), std::move(consumer_fd));
  } else {
    unlink(GetProducerSocket());
    unlink(GetConsumerSocket());
    started = svc->Start(GetProducerSocket(), GetConsumerSocket());
  }

  if (!started) {
    PERFETTO_ELOG("Failed to start the traced service");
    return 1;
  }

  BuiltinProducer builtin_producer(&task_runner, /*lazy_stop_delay_ms=*/30000);
  builtin_producer.ConnectInProcess(svc->service());

  // Set the CPU limit and start the watchdog running. The memory limit will
  // be set inside the service code as it relies on the size of buffers.
  // The CPU limit is the generic one defined in watchdog.h.
  base::Watchdog* watchdog = base::Watchdog::GetInstance();
  watchdog->SetCpuLimit(base::kWatchdogDefaultCpuLimit,
                        base::kWatchdogDefaultCpuWindow);
  watchdog->Start();

  PERFETTO_ILOG("Started traced, listening on %s %s", GetProducerSocket(),
                GetConsumerSocket());
  task_runner.Run();
  return 0;
}

}  // namespace perfetto
