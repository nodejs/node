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

#ifndef TEST_FAKE_PRODUCER_H_
#define TEST_FAKE_PRODUCER_H_

#include <memory>
#include <random>
#include <string>

#include "perfetto/ext/base/thread_checker.h"
#include "perfetto/ext/tracing/core/producer.h"
#include "perfetto/ext/tracing/ipc/producer_ipc_client.h"
#include "perfetto/tracing/core/data_source_descriptor.h"
#include "perfetto/tracing/core/trace_config.h"
#include "src/base/test/test_task_runner.h"

namespace perfetto {

namespace protos {
namespace gen {
class TestConfig;
}  // namespace gen
}  // namespace protos

class FakeProducer : public Producer {
 public:
  explicit FakeProducer(const std::string& name, base::TaskRunner* task_runner);
  ~FakeProducer() override;

  void Connect(const char* socket_name,
               std::function<void()> on_connect,
               std::function<void()> on_setup_data_source_instance,
               std::function<void()> on_create_data_source_instance,
               std::unique_ptr<SharedMemory> shm = nullptr,
               std::unique_ptr<SharedMemoryArbiter> shm_arbiter = nullptr);

  // Produces a batch of events (as configured by the passed config) before the
  // producer is connected to the service using the provided unbound arbiter.
  // Posts |callback| once the data was written. May only be called once.
  void ProduceStartupEventBatch(
      const protos::gen::TestConfig& config,
      SharedMemoryArbiter* arbiter,
      std::function<void()> callback = [] {});

  // Produces a batch of events (as configured in the DataSourceConfig) and
  // posts a callback when the service acknowledges the commit.
  void ProduceEventBatch(std::function<void()> callback = [] {});

  void RegisterDataSource(const DataSourceDescriptor&);
  void CommitData(const CommitDataRequest&, std::function<void()> callback);
  void Sync(std::function<void()> callback);

  bool IsShmemProvidedByProducer() const {
    return endpoint_->IsShmemProvidedByProducer();
  }

  // Producer implementation.
  void OnConnect() override;
  void OnDisconnect() override;
  void SetupDataSource(DataSourceInstanceID,
                       const DataSourceConfig& source_config) override;
  void StartDataSource(DataSourceInstanceID,
                       const DataSourceConfig& source_config) override;
  void StopDataSource(DataSourceInstanceID) override;
  void OnTracingSetup() override;
  void Flush(FlushRequestID, const DataSourceInstanceID*, size_t) override;
  void ClearIncrementalState(const DataSourceInstanceID* /*data_source_ids*/,
                             size_t /*num_data_sources*/) override {}

 private:
  void SetupFromConfig(const protos::gen::TestConfig& config);
  void EmitEventBatchOnTaskRunner(std::function<void()> callback);

  base::ThreadChecker thread_checker_;
  std::string name_;
  base::TaskRunner* task_runner_ = nullptr;
  std::minstd_rand0 rnd_engine_;
  uint32_t message_size_ = 0;
  uint32_t message_count_ = 0;
  uint32_t max_messages_per_second_ = 0;
  std::function<void()> on_connect_;
  std::function<void()> on_setup_data_source_instance_;
  std::function<void()> on_create_data_source_instance_;
  std::unique_ptr<TracingService::ProducerEndpoint> endpoint_;
  std::unique_ptr<TraceWriter> trace_writer_;
};

}  // namespace perfetto

#endif  // TEST_FAKE_PRODUCER_H_
