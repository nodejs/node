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

#include "src/traced/probes/ftrace/ftrace_procfs.h"

#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <fstream>
#include <sstream>
#include <string>

#include "perfetto/base/logging.h"
#include "perfetto/ext/base/file_utils.h"
#include "perfetto/ext/base/string_splitter.h"
#include "perfetto/ext/base/string_utils.h"
#include "perfetto/ext/base/utils.h"

namespace perfetto {

// Reading /trace produces human readable trace output.
// Writing to this file clears all trace buffers for all CPUS.

// Writing to /trace_marker file injects an event into the trace buffer.

// Reading /tracing_on returns 1/0 if tracing is enabled/disabled.
// Writing 1/0 to this file enables/disables tracing.
// Disabling tracing with this file prevents further writes but
// does not clear the buffer.

namespace {

void KernelLogWrite(const char* s) {
  PERFETTO_DCHECK(*s && s[strlen(s) - 1] == '\n');
  if (FtraceProcfs::g_kmesg_fd != -1)
    base::ignore_result(base::WriteAll(FtraceProcfs::g_kmesg_fd, s, strlen(s)));
}

bool WriteFileInternal(const std::string& path,
                       const std::string& str,
                       int flags) {
  base::ScopedFile fd = base::OpenFile(path, flags);
  if (!fd)
    return false;
  ssize_t written = base::WriteAll(fd.get(), str.c_str(), str.length());
  ssize_t length = static_cast<ssize_t>(str.length());
  // This should either fail or write fully.
  PERFETTO_CHECK(written == length || written == -1);
  return written == length;
}

}  // namespace

// static
int FtraceProcfs::g_kmesg_fd = -1;  // Set by ProbesMain() in probes.cc .

// static
std::unique_ptr<FtraceProcfs> FtraceProcfs::Create(const std::string& root) {
  if (!CheckRootPath(root)) {
    return nullptr;
  }
  return std::unique_ptr<FtraceProcfs>(new FtraceProcfs(root));
}

FtraceProcfs::FtraceProcfs(const std::string& root) : root_(root) {}
FtraceProcfs::~FtraceProcfs() = default;

bool FtraceProcfs::EnableEvent(const std::string& group,
                               const std::string& name) {
  std::string path = root_ + "events/" + group + "/" + name + "/enable";
  if (WriteToFile(path, "1"))
    return true;
  path = root_ + "set_event";
  return AppendToFile(path, group + ":" + name);
}

bool FtraceProcfs::DisableEvent(const std::string& group,
                                const std::string& name) {
  std::string path = root_ + "events/" + group + "/" + name + "/enable";
  if (WriteToFile(path, "0"))
    return true;
  path = root_ + "set_event";
  return AppendToFile(path, "!" + group + ":" + name);
}

bool FtraceProcfs::DisableAllEvents() {
  std::string path = root_ + "events/enable";
  return WriteToFile(path, "0");
}

std::string FtraceProcfs::ReadEventFormat(const std::string& group,
                                          const std::string& name) const {
  std::string path = root_ + "events/" + group + "/" + name + "/format";
  return ReadFileIntoString(path);
}

std::vector<std::string> FtraceProcfs::ReadEnabledEvents() {
  std::string path = root_ + "set_event";
  std::string s = ReadFileIntoString(path);
  base::StringSplitter ss(s, '\n');
  std::vector<std::string> events;
  while (ss.Next()) {
    std::string event = ss.cur_token();
    if (event.size() == 0)
      continue;
    events.push_back(base::StripChars(event, ":", '/'));
  }
  return events;
}

std::string FtraceProcfs::ReadPageHeaderFormat() const {
  std::string path = root_ + "events/header_page";
  return ReadFileIntoString(path);
}

std::string FtraceProcfs::ReadCpuStats(size_t cpu) const {
  std::string path = root_ + "per_cpu/cpu" + std::to_string(cpu) + "/stats";
  return ReadFileIntoString(path);
}

size_t FtraceProcfs::NumberOfCpus() const {
  static size_t num_cpus = static_cast<size_t>(sysconf(_SC_NPROCESSORS_CONF));
  return num_cpus;
}

void FtraceProcfs::ClearTrace() {
  std::string path = root_ + "trace";
  PERFETTO_CHECK(ClearFile(path));  // Could not clear.

  // Truncating the trace file leads to tracing_reset_online_cpus being called
  // in the kernel.
  //
  // In case some of the CPUs were not online, their buffer needs to be
  // cleared manually.
  //
  // We cannot use PERFETTO_CHECK as we might get a permission denied error
  // on Android. The permissions to these files are configured in
  // platform/framework/native/cmds/atrace/atrace.rc.
  for (size_t cpu = 0; cpu < NumberOfCpus(); cpu++) {
    if (!ClearFile(root_ + "per_cpu/cpu" + std::to_string(cpu) + "/trace"))
      PERFETTO_ELOG("Failed to clear buffer for CPU %zd", cpu);
  }
}

bool FtraceProcfs::WriteTraceMarker(const std::string& str) {
  std::string path = root_ + "trace_marker";
  return WriteToFile(path, str);
}

bool FtraceProcfs::SetCpuBufferSizeInPages(size_t pages) {
  if (pages * base::kPageSize > 1 * 1024 * 1024 * 1024) {
    PERFETTO_ELOG("Tried to set the per CPU buffer size to more than 1gb.");
    return false;
  }
  std::string path = root_ + "buffer_size_kb";
  return WriteNumberToFile(path, pages * (base::kPageSize / 1024ul));
}

bool FtraceProcfs::EnableTracing() {
  KernelLogWrite("perfetto: enabled ftrace\n");
  PERFETTO_LOG("enabled ftrace");
  std::string path = root_ + "tracing_on";
  return WriteToFile(path, "1");
}

bool FtraceProcfs::DisableTracing() {
  KernelLogWrite("perfetto: disabled ftrace\n");
  PERFETTO_LOG("disabled ftrace");
  std::string path = root_ + "tracing_on";
  return WriteToFile(path, "0");
}

bool FtraceProcfs::SetTracingOn(bool enable) {
  return enable ? EnableTracing() : DisableTracing();
}

bool FtraceProcfs::IsTracingEnabled() {
  std::string path = root_ + "tracing_on";
  char tracing_on = ReadOneCharFromFile(path);
  if (tracing_on == '\0')
    PERFETTO_PLOG("Failed to read %s", path.c_str());
  return tracing_on == '1';
}

bool FtraceProcfs::SetClock(const std::string& clock_name) {
  std::string path = root_ + "trace_clock";
  return WriteToFile(path, clock_name);
}

std::string FtraceProcfs::GetClock() {
  std::string path = root_ + "trace_clock";
  std::string s = ReadFileIntoString(path);

  size_t start = s.find('[');
  if (start == std::string::npos)
    return "";

  size_t end = s.find(']', start);
  if (end == std::string::npos)
    return "";

  return s.substr(start + 1, end - start - 1);
}

std::set<std::string> FtraceProcfs::AvailableClocks() {
  std::string path = root_ + "trace_clock";
  std::string s = ReadFileIntoString(path);
  std::set<std::string> names;

  size_t start = 0;
  size_t end = 0;

  for (;;) {
    end = s.find(' ', start);
    if (end == std::string::npos)
      end = s.size();
    while (end > start && s[end - 1] == '\n')
      end--;
    if (start == end)
      break;

    std::string name = s.substr(start, end - start);

    if (name[0] == '[')
      name = name.substr(1, name.size() - 2);

    names.insert(name);

    if (end == s.size())
      break;

    start = end + 1;
  }

  return names;
}

bool FtraceProcfs::WriteNumberToFile(const std::string& path, size_t value) {
  // 2^65 requires 20 digits to write.
  char buf[21];
  int res = snprintf(buf, 21, "%zu", value);
  if (res < 0 || res >= 21)
    return false;
  return WriteToFile(path, std::string(buf));
}

bool FtraceProcfs::WriteToFile(const std::string& path,
                               const std::string& str) {
  return WriteFileInternal(path, str, O_WRONLY);
}

bool FtraceProcfs::AppendToFile(const std::string& path,
                                const std::string& str) {
  return WriteFileInternal(path, str, O_WRONLY | O_APPEND);
}

base::ScopedFile FtraceProcfs::OpenPipeForCpu(size_t cpu) {
  std::string path =
      root_ + "per_cpu/cpu" + std::to_string(cpu) + "/trace_pipe_raw";
  return base::OpenFile(path, O_RDONLY | O_NONBLOCK);
}

char FtraceProcfs::ReadOneCharFromFile(const std::string& path) {
  base::ScopedFile fd = base::OpenFile(path, O_RDONLY);
  PERFETTO_CHECK(fd);
  char result = '\0';
  ssize_t bytes = PERFETTO_EINTR(read(fd.get(), &result, 1));
  PERFETTO_CHECK(bytes == 1 || bytes == -1);
  return result;
}

bool FtraceProcfs::ClearFile(const std::string& path) {
  base::ScopedFile fd = base::OpenFile(path, O_WRONLY | O_TRUNC);
  return !!fd;
}

std::string FtraceProcfs::ReadFileIntoString(const std::string& path) const {
  // You can't seek or stat the procfs files on Android.
  // The vast majority (884/886) of format files are under 4k.
  std::string str;
  str.reserve(4096);
  if (!base::ReadFile(path, &str))
    return "";
  return str;
}

const std::set<std::string> FtraceProcfs::GetEventNamesForGroup(
    const std::string& path) const {
  std::set<std::string> names;
  std::string full_path = root_ + path;
  base::ScopedDir dir(opendir(full_path.c_str()));
  if (!dir) {
    PERFETTO_DLOG("Unable to read events from %s", full_path.c_str());
    return names;
  }
  struct dirent* ent;
  while ((ent = readdir(*dir)) != nullptr) {
    if (strncmp(ent->d_name, ".", 1) == 0 ||
        strncmp(ent->d_name, "..", 2) == 0) {
      continue;
    }
    // Check ent is a directory.
    struct stat statbuf;
    std::string dir_path = full_path + "/" + ent->d_name;
    if (stat(dir_path.c_str(), &statbuf) == 0) {
      if (S_ISDIR(statbuf.st_mode)) {
        names.insert(ent->d_name);
      }
    }
  }
  return names;
}

// static
bool FtraceProcfs::CheckRootPath(const std::string& root) {
  base::ScopedFile fd = base::OpenFile(root + "trace", O_RDONLY);
  return static_cast<bool>(fd);
}

}  // namespace perfetto
