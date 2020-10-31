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

#ifndef SRC_TRACED_PROBES_FTRACE_FTRACE_PROCFS_H_
#define SRC_TRACED_PROBES_FTRACE_FTRACE_PROCFS_H_

#include <memory>
#include <set>
#include <string>
#include <vector>

#include "perfetto/ext/base/scoped_file.h"

namespace perfetto {

class FtraceProcfs {
 public:
  static std::unique_ptr<FtraceProcfs> Create(const std::string& root);
  static int g_kmesg_fd;

  explicit FtraceProcfs(const std::string& root);
  virtual ~FtraceProcfs();

  // Enable the event under with the given |group| and |name|.
  bool EnableEvent(const std::string& group, const std::string& name);

  // Disable the event under with the given |group| and |name|.
  bool DisableEvent(const std::string& group, const std::string& name);

  // Disable all events by writing to the global enable file.
  bool DisableAllEvents();

  // Read the format for event with the given |group| and |name|.
  // virtual for testing.
  virtual std::string ReadEventFormat(const std::string& group,
                                      const std::string& name) const;

  virtual std::string ReadPageHeaderFormat() const;

  // Read the "/per_cpu/cpuXX/stats" file for the given |cpu|.
  std::string ReadCpuStats(size_t cpu) const;

  // Set ftrace buffer size in pages.
  // This size is *per cpu* so for the total size you have to multiply
  // by the number of CPUs.
  bool SetCpuBufferSizeInPages(size_t pages);

  // Returns the number of CPUs.
  // This will match the number of tracing/per_cpu/cpuXX directories.
  size_t virtual NumberOfCpus() const;

  // Clears the trace buffers for all CPUs. Blocks until this is done.
  void ClearTrace();

  // Writes the string |str| as an event into the trace buffer.
  bool WriteTraceMarker(const std::string& str);

  // Enable tracing.
  bool EnableTracing();

  // Disables tracing, does not clear the buffer.
  bool DisableTracing();

  // Enables/disables tracing, does not clear the buffer.
  bool SetTracingOn(bool enable);

  // Returns true iff tracing is enabled.
  // Necessarily racy: another program could enable/disable tracing at any
  // point.
  bool IsTracingEnabled();

  // Set the clock. |clock_name| should be one of the names returned by
  // AvailableClocks. Setting the clock clears the buffer.
  bool SetClock(const std::string& clock_name);

  // Get the currently set clock.
  std::string GetClock();

  // Get all the available clocks.
  std::set<std::string> AvailableClocks();

  // Get all the enabled events.
  virtual std::vector<std::string> ReadEnabledEvents();

  // Open the raw pipe for |cpu|.
  virtual base::ScopedFile OpenPipeForCpu(size_t cpu);

  virtual const std::set<std::string> GetEventNamesForGroup(
      const std::string& path) const;

 protected:
  // virtual and protected for testing.
  virtual bool WriteToFile(const std::string& path, const std::string& str);
  virtual bool AppendToFile(const std::string& path, const std::string& str);
  virtual bool ClearFile(const std::string& path);
  virtual char ReadOneCharFromFile(const std::string& path);
  virtual std::string ReadFileIntoString(const std::string& path) const;

 private:
  // Checks the trace file is present at the given root path.
  static bool CheckRootPath(const std::string& root);

  bool WriteNumberToFile(const std::string& path, size_t value);

  const std::string root_;
};

}  // namespace perfetto

#endif  // SRC_TRACED_PROBES_FTRACE_FTRACE_PROCFS_H_
