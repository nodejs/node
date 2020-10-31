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

#include <signal.h>

#include "perfetto/ext/base/file_utils.h"
#include "perfetto/ext/base/string_splitter.h"
#include "perfetto/ext/base/string_utils.h"
#include "perfetto/ext/base/string_writer.h"
#include "perfetto/ext/base/unix_task_runner.h"
#include "perfetto/ext/base/utils.h"

namespace perfetto {
namespace {

// This dumps the ftrace stats into trace marker every 500ms. This is useful for
// debugging overruns over time.

base::UnixTaskRunner* g_task_runner = nullptr;

uint32_t ExtractInt(const char* s) {
  for (; *s != '\0'; s++) {
    if (*s == ':') {
      return static_cast<uint32_t>(atoi(s + 1));
    }
  }
  return 0;
}

size_t NumberOfCpus() {
  static size_t num_cpus = static_cast<size_t>(sysconf(_SC_NPROCESSORS_CONF));
  return num_cpus;
}

std::string ReadFileIntoString(const std::string& path) {
  // You can't seek or stat the procfs files on Android.
  // The vast majority (884/886) of format files are under 4k.
  std::string str;
  str.reserve(4096);
  if (!base::ReadFile(path, &str))
    return "";
  return str;
}

std::string ReadCpuStats(size_t cpu) {
  std::string path =
      "/sys/kernel/debug/tracing/per_cpu/cpu" + std::to_string(cpu) + "/stats";
  return ReadFileIntoString(path);
}

void DumpAllCpuStats() {
  base::ScopedFile file(
      base::OpenFile("/sys/kernel/debug/tracing/trace_marker", O_RDWR));
  if (!file) {
    PERFETTO_ELOG("Unable to open trace marker file");
    g_task_runner->PostDelayedTask(&DumpAllCpuStats, 500);
    return;
  }

  for (uint32_t cpu = 0; cpu < NumberOfCpus(); cpu++) {
    std::string text = ReadCpuStats(cpu);
    if (text.empty())
      continue;

    char buffer[1024];
    base::StringSplitter splitter(std::move(text), '\n');
    while (splitter.Next()) {
      base::StringWriter writer(buffer, base::ArraySize(buffer));
      writer.AppendLiteral("C|");
      writer.AppendInt(getpid());
      writer.AppendLiteral("|");

      if (base::StartsWith(splitter.cur_token(), "overrun")) {
        writer.AppendLiteral("overrun_");
      } else if (base::StartsWith(splitter.cur_token(), "commit overrun")) {
        writer.AppendLiteral("commit_overrun_");
      } else {
        continue;
      }
      writer.AppendInt(cpu);
      writer.AppendLiteral("|");
      writer.AppendInt(ExtractInt(splitter.cur_token()));
      writer.AppendLiteral("\n");

      auto output = writer.GetStringView();
      PERFETTO_CHECK(write(*file, output.data(), output.size()) ==
                     static_cast<ssize_t>(output.size()));
    }
  }
  g_task_runner->PostDelayedTask(&DumpAllCpuStats, 500);
}

void SetupCtrlCHandler() {
  // Setup signal handler.
  struct sigaction sa {};

// Glibc headers for sa_sigaction trigger this.
#pragma GCC diagnostic push
#if defined(__clang__)
#pragma GCC diagnostic ignored "-Wdisabled-macro-expansion"
#endif
  sa.sa_handler = [](int) { g_task_runner->Quit(); };
  sa.sa_flags = static_cast<decltype(sa.sa_flags)>(SA_RESETHAND | SA_RESTART);
#pragma GCC diagnostic pop
  sigaction(SIGINT, &sa, nullptr);
  sigaction(SIGTERM, &sa, nullptr);
}

int DumpFtraceStatsMain() {
  base::UnixTaskRunner task_runner;
  g_task_runner = &task_runner;
  SetupCtrlCHandler();
  task_runner.PostTask(&DumpAllCpuStats);
  task_runner.Run();
  return 0;
}

}  // namespace
}  // namespace perfetto

int main(int argc, char** argv) {
  perfetto::base::ignore_result(argc, argv);
  return perfetto::DumpFtraceStatsMain();
}
