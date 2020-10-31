/*
 * Copyright (C) 2020 The Android Open Source Project
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

#include "src/traced/probes/system_info/system_info_data_source.h"

#include "perfetto/base/time.h"
#include "perfetto/ext/base/file_utils.h"
#include "perfetto/ext/base/string_splitter.h"
#include "perfetto/ext/base/string_utils.h"

#include "protos/perfetto/trace/system_info/cpu_info.pbzero.h"
#include "protos/perfetto/trace/trace_packet.pbzero.h"

namespace perfetto {

namespace {

// Key for default processor string in /proc/cpuinfo as seen on arm. Note the
// uppercase P.
const char kDefaultProcessor[] = "Processor";

// Key for processor entry in /proc/cpuinfo. Used to determine whether a group
// of lines describes a CPU.
const char kProcessor[] = "processor";

}  // namespace

// static
const ProbesDataSource::Descriptor SystemInfoDataSource::descriptor = {
    /* name */ "linux.system_info",
    /* flags */ Descriptor::kFlagsNone,
};

SystemInfoDataSource::SystemInfoDataSource(
    TracingSessionID session_id,
    std::unique_ptr<TraceWriter> writer,
    std::unique_ptr<CpuFreqInfo> cpu_freq_info)
    : ProbesDataSource(session_id, &descriptor),
      writer_(std::move(writer)),
      cpu_freq_info_(std::move(cpu_freq_info)) {}

void SystemInfoDataSource::Start() {
  auto packet = writer_->NewTracePacket();
  packet->set_timestamp(static_cast<uint64_t>(base::GetBootTimeNs().count()));
  auto* cpu_info = packet->set_cpu_info();

  // Parse /proc/cpuinfo which contains groups of "key\t: value" lines separated
  // by an empty line. Each group represents a CPU. See the full example in the
  // unittest.
  std::string proc_cpu_info = ReadFile("/proc/cpuinfo");
  std::string::iterator line_start = proc_cpu_info.begin();
  std::string::iterator line_end = proc_cpu_info.end();
  std::string default_processor = "unknown";
  std::string cpu_index = "";
  uint32_t next_cpu_index = 0;
  while (line_start != proc_cpu_info.end()) {
    line_end = find(line_start, proc_cpu_info.end(), '\n');
    if (line_end == proc_cpu_info.end())
      break;
    std::string line = std::string(line_start, line_end);
    line_start = line_end + 1;
    if (line.empty() && !cpu_index.empty()) {
      PERFETTO_DCHECK(cpu_index == std::to_string(next_cpu_index));
      auto* cpu = cpu_info->add_cpus();
      cpu->set_processor(default_processor);
      auto freqs_range = cpu_freq_info_->GetFreqs(next_cpu_index);
      for (auto it = freqs_range.first; it != freqs_range.second; it++) {
        cpu->add_frequencies(*it);
      }
      cpu_index = "";
      next_cpu_index++;
      continue;
    }
    auto splits = base::SplitString(line, ":");
    if (splits.size() != 2)
      continue;
    std::string key =
        base::StripSuffix(base::StripChars(splits[0], "\t", ' '), " ");
    std::string value = base::StripPrefix(splits[1], " ");
    if (key == kDefaultProcessor)
      default_processor = value;
    else if (key == kProcessor)
      cpu_index = value;
  }

  packet->Finalize();
  writer_->Flush();
}

void SystemInfoDataSource::Flush(FlushRequestID,
                                 std::function<void()> callback) {
  writer_->Flush(callback);
}

std::string SystemInfoDataSource::ReadFile(std::string path) {
  std::string contents;
  if (!base::ReadFile(path, &contents))
    return "";
  return contents;
}

}  // namespace perfetto
