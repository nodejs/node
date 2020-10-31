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

#include "src/traced/probes/common/cpu_freq_info.h"

#include <set>

#include "perfetto/ext/base/file_utils.h"
#include "perfetto/ext/base/string_splitter.h"
#include "perfetto/ext/base/string_utils.h"

namespace perfetto {

CpuFreqInfo::CpuFreqInfo(std::string cpu_dir_path) {
  base::ScopedDir cpu_dir(opendir(cpu_dir_path.c_str()));
  if (!cpu_dir) {
    PERFETTO_PLOG("Failed to opendir(%s)", cpu_dir_path.c_str());
    return;
  }
  // Accumulate cpu and freqs into a set to ensure stable order.
  std::set<std::pair</* cpu */ uint32_t, /* freq */ uint32_t>> freqs;
  // Number of CPUs.
  uint32_t cpus = 0;
  while (struct dirent* dir_ent = readdir(*cpu_dir)) {
    if (dir_ent->d_type != DT_DIR)
      continue;
    std::string dir_name(dir_ent->d_name);
    if (!base::StartsWith(dir_name, "cpu"))
      continue;
    auto maybe_cpu_index =
        base::StringToUInt32(base::StripPrefix(dir_name, "cpu"));
    // There are some directories (cpufreq, cpuidle) which should be skipped.
    if (!maybe_cpu_index.has_value())
      continue;
    cpus++;
    uint32_t cpu_index = maybe_cpu_index.value();
    std::string sys_cpu_freqs =
        ReadFile(cpu_dir_path + "/cpu" + std::to_string(cpu_index) +
                 "/cpufreq/scaling_available_frequencies");
    base::StringSplitter entries(sys_cpu_freqs, ' ');
    while (entries.Next()) {
      auto freq = base::StringToUInt32(entries.cur_token());
      if (freq.has_value())
        freqs.insert({cpu_index, freq.value()});
    }
  }

  // Build index with guards.
  uint32_t last_cpu = 0;
  uint32_t index = 0;
  frequencies_index_.push_back(0);
  for (const auto& cpu_freq : freqs) {
    frequencies_.push_back(cpu_freq.second);
    if (cpu_freq.first != last_cpu)
      frequencies_index_.push_back(index);
    last_cpu = cpu_freq.first;
    index++;
  }
  frequencies_.push_back(0);
  frequencies_index_.push_back(index);
}

CpuFreqInfo::~CpuFreqInfo() = default;

CpuFreqInfo::Range CpuFreqInfo::GetFreqs(uint32_t cpu) {
  if (cpu >= frequencies_index_.size() - 1) {
    PERFETTO_DLOG("No frequencies for cpu%" PRIu32, cpu);
    const uint32_t* end = frequencies_.data() + frequencies_.size();
    return {end, end};
  }
  auto* start = &frequencies_[frequencies_index_[cpu]];
  auto* end = &frequencies_[frequencies_index_[cpu + 1]];
  return {start, end};
}

uint32_t CpuFreqInfo::GetCpuFreqIndex(uint32_t cpu, uint32_t freq) {
  auto range = GetFreqs(cpu);
  uint32_t index = 0;
  for (const uint32_t* it = range.first; it != range.second; it++, index++) {
    if (*it == freq) {
      return static_cast<uint32_t>(frequencies_index_[cpu]) + index + 1;
    }
  }
  return 0;
}

std::string CpuFreqInfo::ReadFile(std::string path) {
  std::string contents;
  if (!base::ReadFile(path, &contents))
    return "";
  return contents;
}

}  // namespace perfetto
