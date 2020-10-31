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

#include "src/traced/probes/ftrace/cpu_stats_parser.h"

#include "perfetto/ext/base/string_splitter.h"
#include "perfetto/ext/base/string_utils.h"
#include "src/traced/probes/ftrace/ftrace_controller.h"
#include "src/traced/probes/ftrace/ftrace_procfs.h"
#include "src/traced/probes/ftrace/ftrace_stats.h"

namespace perfetto {
namespace {

uint32_t ExtractInt(const char* s) {
  for (; *s != '\0'; s++) {
    if (*s == ':') {
      return static_cast<uint32_t>(atoi(s + 1));
    }
  }
  return 0;
}

double ExtractDouble(const char* s) {
  for (; *s != '\0'; s++) {
    if (*s == ':') {
      return strtod(s + 1, nullptr);
    }
  }
  return 0;
}

}  // namespace

bool DumpCpuStats(std::string text, FtraceCpuStats* stats) {
  if (text.empty())
    return false;

  base::StringSplitter splitter(std::move(text), '\n');
  while (splitter.Next()) {
    if (base::StartsWith(splitter.cur_token(), "entries")) {
      stats->entries = ExtractInt(splitter.cur_token());
    } else if (base::StartsWith(splitter.cur_token(), "overrun")) {
      stats->overrun = ExtractInt(splitter.cur_token());
    } else if (base::StartsWith(splitter.cur_token(), "commit overrun")) {
      stats->commit_overrun = ExtractInt(splitter.cur_token());
    } else if (base::StartsWith(splitter.cur_token(), "bytes")) {
      stats->bytes_read = ExtractInt(splitter.cur_token());
    } else if (base::StartsWith(splitter.cur_token(), "oldest event ts")) {
      stats->oldest_event_ts = ExtractDouble(splitter.cur_token());
    } else if (base::StartsWith(splitter.cur_token(), "now ts")) {
      stats->now_ts = ExtractDouble(splitter.cur_token());
    } else if (base::StartsWith(splitter.cur_token(), "dropped events")) {
      stats->dropped_events = ExtractInt(splitter.cur_token());
    } else if (base::StartsWith(splitter.cur_token(), "read events")) {
      stats->read_events = ExtractInt(splitter.cur_token());
    }
  }

  return true;
}

bool DumpAllCpuStats(FtraceProcfs* ftrace, FtraceStats* stats) {
  stats->cpu_stats.resize(ftrace->NumberOfCpus(), {});
  for (size_t cpu = 0; cpu < ftrace->NumberOfCpus(); cpu++) {
    stats->cpu_stats[cpu].cpu = cpu;
    if (!DumpCpuStats(ftrace->ReadCpuStats(cpu), &stats->cpu_stats[cpu]))
      return false;
  }
  return true;
}

}  // namespace perfetto
