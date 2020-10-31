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

#ifndef SRC_PROFILING_MEMORY_JAVA_HPROF_H_
#define SRC_PROFILING_MEMORY_JAVA_HPROF_H_

#include "perfetto/ext/tracing/core/producer.h"

namespace perfetto {
namespace profiling {

class JavaHprofProducer : public Producer {
 public:
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

  void ConnectService();
  void Restart();

  // State of connection to the tracing service.
  State state_ = kNotStarted;
  uint32_t connection_backoff_ms_ = 0;
  const char* producer_sock_name_ = nullptr;
};

}  // namespace profiling
}  // namespace perfetto

#endif  // SRC_PROFILING_MEMORY_JAVA_HPROF_H_
