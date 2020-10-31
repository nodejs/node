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

#include <sched.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "perfetto/base/logging.h"
#include "perfetto/base/time.h"

// Skippy is a program that produces a visually identifiable stepping pattern
// in the systrace UI that is useful for debugging dropped or corrupted data.

namespace perfetto {
namespace {

void SetAffinity(size_t cpu) {
  cpu_set_t set{};
  CPU_SET(cpu, &set);
  PERFETTO_CHECK(
      sched_setaffinity(0 /* calling process */, sizeof(cpu_set_t), &set) == 0);
}

int SkippyMain() {
  static size_t num_cpus = static_cast<size_t>(sysconf(_SC_NPROCESSORS_CONF));
  size_t cpu = 0;
  base::TimeMillis last = base::GetWallTimeMs();

  SetAffinity(cpu);

  for (;;) {
    base::TimeMillis now = base::GetWallTimeMs();
    if ((now - last) < base::TimeMillis(100))
      continue;
    last = now;
    cpu = (cpu + 1) % num_cpus;
    SetAffinity(cpu);
  }
}

}  // namespace
}  // namespace perfetto

int main() {
  return perfetto::SkippyMain();
}
