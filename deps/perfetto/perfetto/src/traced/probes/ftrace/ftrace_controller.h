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

#ifndef SRC_TRACED_PROBES_FTRACE_FTRACE_CONTROLLER_H_
#define SRC_TRACED_PROBES_FTRACE_FTRACE_CONTROLLER_H_

#include <stdint.h>
#include <unistd.h>

#include <bitset>
#include <functional>
#include <map>
#include <memory>
#include <set>
#include <string>

#include "perfetto/base/task_runner.h"
#include "perfetto/ext/base/paged_memory.h"
#include "perfetto/ext/base/utils.h"
#include "perfetto/ext/base/weak_ptr.h"
#include "perfetto/ext/tracing/core/basic_types.h"
#include "src/traced/probes/ftrace/cpu_reader.h"
#include "src/traced/probes/ftrace/ftrace_config_utils.h"

namespace perfetto {

class FtraceConfigMuxer;
class FtraceDataSource;
class FtraceProcfs;
class ProtoTranslationTable;
struct FtraceStats;

// Method of last resort to reset ftrace state.
void HardResetFtraceState();

// Utility class for controlling ftrace.
class FtraceController {
 public:
  static const char* const kTracingPaths[];

  class Observer {
   public:
    virtual ~Observer();
    virtual void OnFtraceDataWrittenIntoDataSourceBuffers() = 0;
  };

  // The passed Observer must outlive the returned FtraceController instance.
  static std::unique_ptr<FtraceController> Create(base::TaskRunner*, Observer*);
  virtual ~FtraceController();

  void DisableAllEvents();
  void WriteTraceMarker(const std::string& s);
  void ClearTrace();

  bool AddDataSource(FtraceDataSource*) PERFETTO_WARN_UNUSED_RESULT;
  bool StartDataSource(FtraceDataSource*);
  void RemoveDataSource(FtraceDataSource*);

  // Force a read of the ftrace buffers. Will call OnFtraceFlushComplete() on
  // all |started_data_sources_|.
  void Flush(FlushRequestID);

  void DumpFtraceStats(FtraceStats*);

  base::WeakPtr<FtraceController> GetWeakPtr() {
    return weak_factory_.GetWeakPtr();
  }

 protected:
  // Protected for testing.
  FtraceController(std::unique_ptr<FtraceProcfs>,
                   std::unique_ptr<ProtoTranslationTable>,
                   std::unique_ptr<FtraceConfigMuxer>,
                   base::TaskRunner*,
                   Observer*);

  // Protected and virtual for testing.
  virtual uint64_t NowMs() const;

 private:
  friend class TestFtraceController;

  struct PerCpuState {
    PerCpuState(std::unique_ptr<CpuReader> _reader, size_t _period_page_quota)
        : reader(std::move(_reader)), period_page_quota(_period_page_quota) {}
    std::unique_ptr<CpuReader> reader;
    size_t period_page_quota = 0;
  };

  FtraceController(const FtraceController&) = delete;
  FtraceController& operator=(const FtraceController&) = delete;

  // Periodic task that reads all per-cpu ftrace buffers.
  void ReadTick(int generation);

  uint32_t GetDrainPeriodMs();

  void StartIfNeeded();
  void StopIfNeeded();

  base::TaskRunner* const task_runner_;
  Observer* const observer_;
  base::PagedMemory parsing_mem_;
  std::unique_ptr<FtraceProcfs> ftrace_procfs_;
  std::unique_ptr<ProtoTranslationTable> table_;
  std::unique_ptr<FtraceConfigMuxer> ftrace_config_muxer_;
  int generation_ = 0;
  bool atrace_running_ = false;
  std::vector<PerCpuState> per_cpu_;  // empty if tracing isn't active
  std::set<FtraceDataSource*> data_sources_;
  std::set<FtraceDataSource*> started_data_sources_;
  base::WeakPtrFactory<FtraceController> weak_factory_;  // Keep last.
};

}  // namespace perfetto

#endif  // SRC_TRACED_PROBES_FTRACE_FTRACE_CONTROLLER_H_
