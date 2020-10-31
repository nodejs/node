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

#ifndef SRC_TRACED_PROBES_SYS_STATS_SYS_STATS_DATA_SOURCE_H_
#define SRC_TRACED_PROBES_SYS_STATS_SYS_STATS_DATA_SOURCE_H_

#include <string.h>

#include <map>
#include <memory>
#include <string>

#include "perfetto/ext/base/paged_memory.h"
#include "perfetto/ext/base/scoped_file.h"
#include "perfetto/ext/base/weak_ptr.h"
#include "perfetto/ext/tracing/core/basic_types.h"
#include "perfetto/ext/tracing/core/trace_writer.h"
#include "perfetto/tracing/core/data_source_config.h"
#include "src/traced/probes/probes_data_source.h"

namespace perfetto {

namespace base {
class TaskRunner;
}

namespace protos {
namespace pbzero {
class SysStats;
}
}  // namespace protos

class SysStatsDataSource : public ProbesDataSource {
 public:
  static const ProbesDataSource::Descriptor descriptor;

  using OpenFunction = base::ScopedFile (*)(const char*);
  SysStatsDataSource(base::TaskRunner*,
                     TracingSessionID,
                     std::unique_ptr<TraceWriter> writer,
                     const DataSourceConfig&,
                     OpenFunction = nullptr);
  ~SysStatsDataSource() override;

  // ProbesDataSource implementation.
  void Start() override;
  void Flush(FlushRequestID, std::function<void()> callback) override;

  base::WeakPtr<SysStatsDataSource> GetWeakPtr() const;

  void set_ns_per_user_hz_for_testing(uint64_t ns) { ns_per_user_hz_ = ns; }
  uint32_t tick_for_testing() const { return tick_; }

 private:
  struct CStrCmp {
    bool operator()(const char* a, const char* b) const {
      return strcmp(a, b) < 0;
    }
  };

  static void Tick(base::WeakPtr<SysStatsDataSource>);

  SysStatsDataSource(const SysStatsDataSource&) = delete;
  SysStatsDataSource& operator=(const SysStatsDataSource&) = delete;
  void ReadSysStats();  // Virtual for testing.
  void ReadMeminfo(protos::pbzero::SysStats* sys_stats);
  void ReadVmstat(protos::pbzero::SysStats* sys_stats);
  void ReadStat(protos::pbzero::SysStats* sys_stats);
  size_t ReadFile(base::ScopedFile*, const char* path);

  base::TaskRunner* const task_runner_;
  std::unique_ptr<TraceWriter> writer_;
  base::ScopedFile meminfo_fd_;
  base::ScopedFile vmstat_fd_;
  base::ScopedFile stat_fd_;
  base::PagedMemory read_buf_;
  TraceWriter::TracePacketHandle cur_packet_;
  std::map<const char*, int, CStrCmp> meminfo_counters_;
  std::map<const char*, int, CStrCmp> vmstat_counters_;
  uint64_t ns_per_user_hz_ = 0;
  uint32_t tick_ = 0;
  uint32_t tick_period_ms_ = 0;
  uint32_t meminfo_ticks_ = 0;
  uint32_t vmstat_ticks_ = 0;
  uint32_t stat_ticks_ = 0;
  uint32_t stat_enabled_fields_ = 0;

  base::WeakPtrFactory<SysStatsDataSource> weak_factory_;  // Keep last.
};

}  // namespace perfetto

#endif  // SRC_TRACED_PROBES_SYS_STATS_SYS_STATS_DATA_SOURCE_H_
