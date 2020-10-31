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

#include <getopt.h>
#include <inttypes.h>
#include <stdint.h>
#include <unistd.h>

#include <thread>

#include "perfetto/base/logging.h"
#include "perfetto/base/time.h"

#define PERFETTO_HAVE_PTHREADS                \
  (PERFETTO_BUILDFLAG(PERFETTO_OS_LINUX) ||   \
   PERFETTO_BUILDFLAG(PERFETTO_OS_ANDROID) || \
   PERFETTO_BUILDFLAG(PERFETTO_OS_MACOSX))

#if PERFETTO_HAVE_PTHREADS
#include <pthread.h>
#endif

// Spawns the requested number threads that alternate between busy-waiting and
// sleeping.

namespace perfetto {
namespace {

void SetRandomThreadName(uint32_t thread_name_count) {
#if PERFETTO_HAVE_PTHREADS
  char name[16] = {};
  snprintf(name, sizeof(name), "busy-%" PRIu32,
           static_cast<uint32_t>(rand()) % thread_name_count);
  pthread_setname_np(pthread_self(), name);
#endif
}

void PrintUsage(const char* bin_name) {
#if PERFETTO_HAVE_PTHREADS
  PERFETTO_ELOG(
      "Usage: %s --threads=N --period_us=N --duty_cycle=[1-100] "
      "[--thread_names=N]",
      bin_name);
#else
  PERFETTO_ELOG("Usage: %s --threads=N --period_us=N --duty_cycle=[1-100]",
                bin_name);
#endif
}

__attribute__((noreturn)) void BusyWait(int64_t tstart,
                                        int64_t period_us,
                                        int64_t busy_us,
                                        uint32_t thread_name_count) {
  int64_t tbusy = tstart;
  int64_t tnext = tstart;
  for (;;) {
    if (thread_name_count)
      SetRandomThreadName(thread_name_count);

    tbusy = tnext + busy_us * 1000;
    tnext += period_us * 1000;
    while (base::GetWallTimeNs().count() < tbusy) {
      for (int i = 0; i < 10000; i++) {
        asm volatile("" ::: "memory");
      }
    }
    auto tnow = base::GetWallTimeNs().count();
    if (tnow >= tnext) {
      std::this_thread::yield();
      continue;
    }

    while (tnow < tnext) {
      // +1 to prevent sleeping twice when there is truncation.
      base::SleepMicroseconds(static_cast<uint32_t>((tnext - tnow) / 1000) + 1);
      tnow = base::GetWallTimeNs().count();
    }
  }
}

int BusyThreadsMain(int argc, char** argv) {
  int64_t num_threads = -1;
  int64_t period_us = -1;
  int64_t duty_cycle = -1;
  uint32_t thread_name_count = 0;

  static struct option long_options[] = {
    {"threads", required_argument, nullptr, 't'},
    {"period_us", required_argument, nullptr, 'p'},
    {"duty_cycle", required_argument, nullptr, 'd'},
#if PERFETTO_HAVE_PTHREADS
    {"thread_names", required_argument, nullptr, 'r'},
#endif
    {nullptr, 0, nullptr, 0}
  };
  int option_index;
  int c;
  while ((c = getopt_long(argc, argv, "", long_options, &option_index)) != -1) {
    switch (c) {
      case 't':
        num_threads = atol(optarg);
        break;
      case 'p':
        period_us = atol(optarg);
        break;
      case 'd':
        duty_cycle = atol(optarg);
        break;
#if PERFETTO_HAVE_PTHREADS
      case 'r':
        thread_name_count = static_cast<uint32_t>(atoi(optarg));
        break;
#endif
      default:
        break;
    }
  }
  if (num_threads < 1 || period_us < 0 || duty_cycle < 1 || duty_cycle > 100 ||
      thread_name_count > (1 << 20)) {
    PrintUsage(argv[0]);
    return 1;
  }

  int64_t busy_us = static_cast<int64_t>(period_us * (duty_cycle / 100.0));

  PERFETTO_LOG("Spawning %" PRId64 " threads; period duration: %" PRId64
               "us; busy duration: %" PRId64 "us.",
               num_threads, period_us, busy_us);

  int64_t tstart = base::GetWallTimeNs().count();
  for (int i = 0; i < num_threads; i++) {
    std::thread th(BusyWait, tstart, period_us, busy_us, thread_name_count);
    th.detach();
  }
  PERFETTO_LOG("Threads spawned, Ctrl-C to stop.");
  while (sleep(600))
    ;

  return 0;
}

}  // namespace
}  // namespace perfetto

int main(int argc, char** argv) {
  return perfetto::BusyThreadsMain(argc, argv);
}
