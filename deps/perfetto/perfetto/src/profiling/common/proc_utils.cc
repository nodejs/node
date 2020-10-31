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

#include "src/profiling/common/proc_utils.h"

#include <inttypes.h>
#include <sys/stat.h>
#include <unistd.h>

#include "perfetto/ext/base/file_utils.h"
#include "perfetto/ext/base/optional.h"
#include "perfetto/profiling/normalize.h"

namespace perfetto {
namespace profiling {
namespace {

bool GetProcFile(pid_t pid, const char* file, char* filename_buf, size_t size) {
  ssize_t written = snprintf(filename_buf, size, "/proc/%d/%s", pid, file);
  if (written < 0 || static_cast<size_t>(written) >= size) {
    if (written < 0)
      PERFETTO_ELOG("Failed to concatenate cmdline file.");
    else
      PERFETTO_ELOG("Overflow when concatenating cmdline file.");
    return false;
  }
  return true;
}

base::Optional<uint32_t> ParseProcStatusSize(const std::string& status,
                                             const std::string& key) {
  auto entry_idx = status.find(key);
  if (entry_idx == std::string::npos)
    return {};
  entry_idx = status.find_first_not_of(" \t", entry_idx + key.size());
  if (entry_idx == std::string::npos)
    return {};
  int32_t val = atoi(status.c_str() + entry_idx);
  if (val < 0) {
    PERFETTO_ELOG("Unexpected value reading %s", key.c_str());
    return {};
  }
  return static_cast<uint32_t>(val);
}
}  // namespace

base::Optional<std::vector<std::string>> NormalizeCmdlines(
    const std::vector<std::string>& cmdlines) {
  std::vector<std::string> normalized_cmdlines;
  normalized_cmdlines.reserve(cmdlines.size());

  for (size_t i = 0; i < cmdlines.size(); i++) {
    std::string cmdline = cmdlines[i];  // mutable copy
    // Add nullbyte to make sure it's a C string.
    cmdline.resize(cmdline.size() + 1, '\0');
    char* cmdline_cstr = &(cmdline[0]);
    ssize_t size = NormalizeCmdLine(&cmdline_cstr, cmdline.size());
    if (size == -1) {
      PERFETTO_PLOG("Failed to normalize cmdline %s. Stopping the parse.",
                    cmdlines[i].c_str());
      return base::nullopt;
    }
    normalized_cmdlines.emplace_back(cmdline_cstr, static_cast<size_t>(size));
  }
  return base::make_optional(normalized_cmdlines);
}

// This is mostly the same as GetHeapprofdProgramProperty in
// https://android.googlesource.com/platform/bionic/+/master/libc/bionic/malloc_common.cpp
// This should give the same result as GetHeapprofdProgramProperty.
bool GetCmdlineForPID(pid_t pid, std::string* name) {
  std::string filename = "/proc/" + std::to_string(pid) + "/cmdline";
  base::ScopedFile fd(base::OpenFile(filename, O_RDONLY | O_CLOEXEC));
  if (!fd) {
    PERFETTO_DPLOG("Failed to open %s", filename.c_str());
    return false;
  }
  char cmdline[512];
  const size_t max_read_size = sizeof(cmdline) - 1;
  ssize_t rd = read(*fd, cmdline, max_read_size);
  if (rd == -1) {
    PERFETTO_DPLOG("Failed to read %s", filename.c_str());
    return false;
  }

  if (rd == 0) {
    PERFETTO_DLOG("Empty cmdline for %" PRIdMAX ". Skipping.",
                  static_cast<intmax_t>(pid));
    return false;
  }

  // In some buggy kernels (before http://bit.ly/37R7qwL) /proc/pid/cmdline is
  // not NUL-terminated (see b/147438623). If we read < max_read_size bytes
  // assume we are hitting the aforementioned kernel bug and terminate anyways.
  const size_t rd_u = static_cast<size_t>(rd);
  if (rd_u >= max_read_size && memchr(cmdline, '\0', rd_u) == nullptr) {
    // We did not manage to read the first argument.
    PERFETTO_DLOG("Overflow reading cmdline for %" PRIdMAX,
                  static_cast<intmax_t>(pid));
    errno = EOVERFLOW;
    return false;
  }

  cmdline[rd] = '\0';
  char* cmdline_start = cmdline;
  ssize_t size = NormalizeCmdLine(&cmdline_start, rd_u);
  if (size == -1)
    return false;
  name->assign(cmdline_start, static_cast<size_t>(size));
  return true;
}

void FindAllProfilablePids(std::set<pid_t>* pids) {
  ForEachPid([pids](pid_t pid) {
    if (pid == getpid())
      return;

    char filename_buf[128];
    if (!GetProcFile(pid, "cmdline", filename_buf, sizeof(filename_buf)))
      return;
    struct stat statbuf;
    // Check if we have permission to the process.
    if (stat(filename_buf, &statbuf) == 0)
      pids->emplace(pid);
  });
}

void FindPidsForCmdlines(const std::vector<std::string>& cmdlines,
                         std::set<pid_t>* pids) {
  ForEachPid([&cmdlines, pids](pid_t pid) {
    if (pid == getpid())
      return;
    std::string process_cmdline;
    process_cmdline.reserve(512);
    GetCmdlineForPID(pid, &process_cmdline);
    for (const std::string& cmdline : cmdlines) {
      if (process_cmdline == cmdline)
        pids->emplace(static_cast<pid_t>(pid));
    }
  });
}

base::Optional<uint32_t> GetRssAnonAndSwap(pid_t pid) {
  std::string path = "/proc/" + std::to_string(pid) + "/status";
  std::string status;
  bool read_proc = base::ReadFile(path, &status);
  if (!read_proc) {
    PERFETTO_ELOG("Failed to read %s", path.c_str());
    return base::nullopt;
  }
  auto anon_rss = ParseProcStatusSize(status, "RssAnon:");
  auto swap = ParseProcStatusSize(status, "VmSwap:");
  if (anon_rss.has_value() && swap.has_value()) {
    return *anon_rss + *swap;
  }
  return base::nullopt;
}

void RemoveUnderAnonThreshold(uint32_t min_size_kb, std::set<pid_t>* pids) {
  for (auto it = pids->begin(); it != pids->end();) {
    base::Optional<uint32_t> rss_and_swap = GetRssAnonAndSwap(*it);
    if (rss_and_swap && rss_and_swap < min_size_kb) {
      it = pids->erase(it);
    } else {
      ++it;
    }
  }
}

bool IsUnderAnonRssAndSwapThreshold(pid_t pid,
                                    uint32_t min_size_kb,
                                    const std::string& status) {
  auto anon_rss = ParseProcStatusSize(status, "RssAnon:");
  auto swap = ParseProcStatusSize(status, "VmSwap:");
  if (anon_rss.has_value() && swap.has_value()) {
    uint32_t anon_kb = anon_rss.value() + swap.value();
    if (anon_kb < min_size_kb) {
      PERFETTO_LOG("Removing pid %d from profiled set (anon: %d kB)", pid,
                   anon_kb);
      return true;
    }
  }
  return false;
}

}  // namespace profiling
}  // namespace perfetto
