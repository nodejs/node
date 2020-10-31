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

#ifndef SRC_TRACED_PROBES_PROBES_PRODUCER_H_
#define SRC_TRACED_PROBES_PROBES_PRODUCER_H_

#include <memory>
#include <unordered_map>
#include <utility>

#include "perfetto/base/task_runner.h"
#include "perfetto/ext/base/watchdog.h"
#include "perfetto/ext/base/weak_ptr.h"
#include "perfetto/ext/tracing/core/producer.h"
#include "perfetto/ext/tracing/core/trace_writer.h"
#include "perfetto/ext/tracing/core/tracing_service.h"
#include "src/traced/probes/filesystem/inode_file_data_source.h"
#include "src/traced/probes/ftrace/ftrace_controller.h"
#include "src/traced/probes/ftrace/ftrace_metadata.h"

#include "protos/perfetto/trace/filesystem/inode_file_map.pbzero.h"

namespace perfetto {

class ProbesDataSource;

const uint64_t kLRUInodeCacheSize = 1000;

class ProbesProducer : public Producer, public FtraceController::Observer {
 public:
  ProbesProducer();
  ~ProbesProducer() override;

  // Producer Impl:
  void OnConnect() override;
  void OnDisconnect() override;
  void SetupDataSource(DataSourceInstanceID, const DataSourceConfig&) override;
  void StartDataSource(DataSourceInstanceID, const DataSourceConfig&) override;
  void StopDataSource(DataSourceInstanceID) override;
  void OnTracingSetup() override;
  void Flush(FlushRequestID,
             const DataSourceInstanceID* data_source_ids,
             size_t num_data_sources) override;
  void ClearIncrementalState(const DataSourceInstanceID* data_source_ids,
                             size_t num_data_sources) override;

  // FtraceController::Observer implementation.
  void OnFtraceDataWrittenIntoDataSourceBuffers() override;

  // Our Impl
  void ConnectWithRetries(const char* socket_name,
                          base::TaskRunner* task_runner);
  std::unique_ptr<ProbesDataSource> CreateFtraceDataSource(
      TracingSessionID session_id,
      const DataSourceConfig& config);
  std::unique_ptr<ProbesDataSource> CreateProcessStatsDataSource(
      TracingSessionID session_id,
      const DataSourceConfig& config);
  std::unique_ptr<ProbesDataSource> CreateInodeFileDataSource(
      TracingSessionID session_id,
      DataSourceConfig config);
  std::unique_ptr<ProbesDataSource> CreateSysStatsDataSource(
      TracingSessionID session_id,
      const DataSourceConfig& config);
  std::unique_ptr<ProbesDataSource> CreateAndroidPowerDataSource(
      TracingSessionID session_id,
      const DataSourceConfig& config);
  std::unique_ptr<ProbesDataSource> CreateAndroidLogDataSource(
      TracingSessionID session_id,
      const DataSourceConfig& config);
  std::unique_ptr<ProbesDataSource> CreatePackagesListDataSource(
      TracingSessionID session_id,
      const DataSourceConfig& config);
  std::unique_ptr<ProbesDataSource> CreateMetatraceDataSource(
      TracingSessionID session_id,
      const DataSourceConfig& config);
  std::unique_ptr<ProbesDataSource> CreateSystemInfoDataSource(
      TracingSessionID session_id,
      const DataSourceConfig& config);
  std::unique_ptr<ProbesDataSource> CreateInitialDisplayStateDataSource(
      TracingSessionID session_id,
      const DataSourceConfig& config);

 private:
  enum State {
    kNotStarted = 0,
    kNotConnected,
    kConnecting,
    kConnected,
  };

  ProbesProducer(const ProbesProducer&) = delete;
  ProbesProducer& operator=(const ProbesProducer&) = delete;

  void Connect();
  void Restart();
  void ResetConnectionBackoff();
  void IncreaseConnectionBackoff();
  void OnDataSourceFlushComplete(FlushRequestID, DataSourceInstanceID);
  void OnFlushTimeout(FlushRequestID);

  State state_ = kNotStarted;
  base::TaskRunner* task_runner_ = nullptr;
  std::unique_ptr<TracingService::ProducerEndpoint> endpoint_;
  std::unique_ptr<FtraceController> ftrace_;
  bool ftrace_creation_failed_ = false;
  uint32_t connection_backoff_ms_ = 0;
  const char* socket_name_ = nullptr;

  // Owning map for all active data sources.
  std::unordered_map<DataSourceInstanceID, std::unique_ptr<ProbesDataSource>>
      data_sources_;

  // Keeps (pointers to) data sources ordered by session id.
  std::unordered_multimap<TracingSessionID, ProbesDataSource*>
      session_data_sources_;

  std::unordered_multimap<FlushRequestID, DataSourceInstanceID>
      pending_flushes_;

  std::unordered_map<DataSourceInstanceID, base::Watchdog::Timer> watchdogs_;
  LRUInodeCache cache_{kLRUInodeCacheSize};
  std::map<BlockDeviceID, std::unordered_map<Inode, InodeMapValue>>
      system_inodes_;

  base::WeakPtrFactory<ProbesProducer> weak_factory_;  // Keep last.
};

}  // namespace perfetto

#endif  // SRC_TRACED_PROBES_PROBES_PRODUCER_H_
