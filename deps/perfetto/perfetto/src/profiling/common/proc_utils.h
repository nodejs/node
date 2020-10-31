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

#ifndef SRC_PROFILING_COMMON_PROC_UTILS_H_
#define SRC_PROFILING_COMMON_PROC_UTILS_H_

#include <sys/types.h>
#include <set>
#include <vector>

#include "perfetto/ext/base/optional.h"
#include "perfetto/ext/base/scoped_file.h"

namespace perfetto {
namespace profiling {

template <typename Fn>
void ForEachPid(Fn callback) {
  base::ScopedDir proc_dir(opendir("/proc"));
  if (!proc_dir) {
    PERFETTO_DFATAL_OR_ELOG("Failed to open /proc");
    return;
  }
  struct dirent* entry;
  while ((entry = readdir(*proc_dir))) {
    char* end;
    long int pid = strtol(entry->d_name, &end, 10);
    if (*end != '\0')
      continue;
    callback(static_cast<pid_t>(pid));
  }
}

base::Optional<std::vector<std::string>> NormalizeCmdlines(
    const std::vector<std::string>& cmdlines);

void FindAllProfilablePids(std::set<pid_t>* pids);
void FindPidsForCmdlines(const std::vector<std::string>& cmdlines,
                         std::set<pid_t>* pids);
bool GetCmdlineForPID(pid_t pid, std::string* name);

base::Optional<uint32_t> GetRssAnonAndSwap(pid_t pid);
// Filters the list of pids (in-place), keeping only the
// entries satisfying the minimum size criteria for anonymous memory.
void RemoveUnderAnonThreshold(uint32_t min_size_kb, std::set<pid_t>* pids);
bool IsUnderAnonRssAndSwapThreshold(pid_t pid,
                                    uint32_t min_size_kb,
                                    const std::string& status);

}  // namespace profiling
}  // namespace perfetto

#endif  // SRC_PROFILING_COMMON_PROC_UTILS_H_
