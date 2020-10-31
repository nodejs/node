/*
 * Copyright (C) 2019 The Android Open Source Project
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

#ifndef SRC_PROFILING_MEMORY_JAVA_HPROF_PRODUCER_H_
#define SRC_PROFILING_MEMORY_JAVA_HPROF_PRODUCER_H_

#include <memory>
#include <set>
#include <vector>

#include "perfetto/ext/base/unix_task_runner.h"
#include "perfetto/ext/base/weak_ptr.h"
#include "perfetto/ext/tracing/core/producer.h"
#include "perfetto/ext/tracing/core/tracing_service.h"
#include "perfetto/ext/tracing/ipc/producer_ipc_client.h"
#include "perfetto/tracing/core/data_source_config.h"
#include "perfetto/tracing/core/data_source_descriptor.h"

#include "protos/perfetto/config/profiling/java_hprof_config.gen.h"

namespace perfetto {
namespace profiling {

using JavaHprofConfig = protos::gen::JavaHprofConfig;

class JavaHprofProducer : public Producer {
 public:
  JavaHprofProducer(base::TaskRunner* task_runner)
      : task_runner_(task_runner), weak_factory_(this) {}

  // Producer Impl:
  void OnConnect() override;
  void OnDisconnect() override;
  void SetupDataSource(DataSourceInstanceID, const DataSourceConfig&) override;
  void StartDataSource(DataSourceInstanceID, const DataSourceConfig&) override;
  void StopDataSource(DataSourceInstanceID) override;
  void OnTracingSetup() override {}
  void Flush(FlushRequestID,
             const DataSourceInstanceID* data_source_ids,
             size_t num_data_sources) override;
  void ClearIncrementalState(const DataSourceInstanceID* /*data_source_ids*/,
                             size_t /*num_data_sources*/) override {}
  // TODO(fmayer): Refactor once/if we have generic reconnect logic.
  void ConnectWithRetries(const char* socket_name);
  void SetProducerEndpoint(
      std::unique_ptr<TracingService::ProducerEndpoint> endpoint);

 private:
  // State of the connection to tracing service (traced).
  enum State {
    kNotStarted = 0,
    kNotConnected,
    kConnecting,
    kConnected,
  };

  struct DataSource {
    DataSourceInstanceID id;
    std::set<pid_t> pids;
    JavaHprofConfig config;
  };

  void ConnectService();
  void Restart();
  void ResetConnectionBackoff();
  void IncreaseConnectionBackoff();

  void DoContinuousDump(DataSourceInstanceID id, uint32_t dump_interval);
  static void SignalDataSource(const DataSource& ds);

  // State of connection to the tracing service.
  State state_ = kNotStarted;
  uint32_t connection_backoff_ms_ = 0;
  const char* producer_sock_name_ = nullptr;

  base::TaskRunner* const task_runner_;
  std::unique_ptr<TracingService::ProducerEndpoint> endpoint_;

  std::map<DataSourceInstanceID, DataSource> data_sources_;

  base::WeakPtrFactory<JavaHprofProducer> weak_factory_;  // Keep last.
};

}  // namespace profiling
}  // namespace perfetto

#endif  // SRC_PROFILING_MEMORY_JAVA_HPROF_PRODUCER_H_
