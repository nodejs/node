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

#include <stddef.h>
#include <stdint.h>
#include <unistd.h>

#include "perfetto/base/logging.h"
#include "perfetto/base/task_runner.h"
#include "perfetto/ext/base/utils.h"
#include "perfetto/ext/tracing/core/producer.h"
#include "perfetto/ext/tracing/core/trace_writer.h"
#include "perfetto/ext/tracing/ipc/default_socket.h"
#include "perfetto/ext/tracing/ipc/producer_ipc_client.h"
#include "perfetto/ext/tracing/ipc/service_ipc_host.h"
#include "perfetto/tracing/core/data_source_config.h"
#include "perfetto/tracing/core/data_source_descriptor.h"
#include "protos/perfetto/trace/test_event.pbzero.h"
#include "src/base/test/test_task_runner.h"
#include "test/test_helper.h"

#include "protos/perfetto/trace/trace_packet.pbzero.h"

// If we're building on Android and starting the daemons ourselves,
// create the sockets in a world-writable location.
#if PERFETTO_BUILDFLAG(PERFETTO_OS_ANDROID) && \
    PERFETTO_BUILDFLAG(PERFETTO_START_DAEMONS)
#define TEST_PRODUCER_SOCK_NAME "/data/local/tmp/traced_producer"
#else
#define TEST_PRODUCER_SOCK_NAME ::perfetto::GetProducerSocket()
#endif

namespace perfetto {
namespace shm_fuzz {
namespace {

// Fake producer writing a protozero message of data into shared memory
// buffer, followed by a sentinel message to signal completion to the
// consumer.
class FakeProducer : public Producer {
 public:
  FakeProducer(std::string name,
               const uint8_t* data,
               size_t size,
               std::function<void()> on_produced_and_committed)
      : name_(std::move(name)),
        data_(data),
        size_(size),
        on_produced_and_committed_(on_produced_and_committed) {}

  void Connect(const char* socket_name, base::TaskRunner* task_runner) {
    endpoint_ = ProducerIPCClient::Connect(
        socket_name, this, "android.perfetto.FakeProducer", task_runner);
  }

  void OnConnect() override {
    DataSourceDescriptor descriptor;
    descriptor.set_name(name_);
    endpoint_->RegisterDataSource(descriptor);
  }

  void OnDisconnect() override {}

  void SetupDataSource(DataSourceInstanceID, const DataSourceConfig&) override {
  }

  void StartDataSource(DataSourceInstanceID,
                       const DataSourceConfig& source_config) override {
    auto trace_writer = endpoint_->CreateTraceWriter(
        static_cast<BufferID>(source_config.target_buffer()));
    {
      auto packet = trace_writer->NewTracePacket();
      packet->stream_writer_for_testing()->WriteBytes(data_, size_);
    }
    trace_writer->Flush();

    {
      auto end_packet = trace_writer->NewTracePacket();
      end_packet->set_for_testing()->set_str("end");
    }
    trace_writer->Flush(on_produced_and_committed_);
  }

  void StopDataSource(DataSourceInstanceID) override {}
  void OnTracingSetup() override {}
  void Flush(FlushRequestID, const DataSourceInstanceID*, size_t) override {}
  void ClearIncrementalState(const DataSourceInstanceID*, size_t) override {}

 private:
  const std::string name_;
  const uint8_t* data_;
  const size_t size_;
  std::unique_ptr<TracingService::ProducerEndpoint> endpoint_;
  std::function<void()> on_produced_and_committed_;
};

class FuzzerFakeProducerThread {
 public:
  FuzzerFakeProducerThread(const uint8_t* data,
                           size_t size,
                           std::function<void()> on_produced_and_committed)
      : data_(data),
        size_(size),
        on_produced_and_committed_(on_produced_and_committed) {}

  ~FuzzerFakeProducerThread() {
    if (!runner_)
      return;
    runner_->PostTaskAndWaitForTesting([this]() { producer_.reset(); });
  }

  void Connect() {
    runner_ = base::ThreadTaskRunner::CreateAndStart("perfetto.prd.fake");
    runner_->PostTaskAndWaitForTesting([this]() {
      producer_.reset(new FakeProducer("android.perfetto.FakeProducer", data_,
                                       size_, on_produced_and_committed_));
      producer_->Connect(TEST_PRODUCER_SOCK_NAME, runner_->get());
    });
  }

 private:
  base::Optional<base::ThreadTaskRunner> runner_;  // Keep first.

  std::unique_ptr<FakeProducer> producer_;
  const uint8_t* data_;
  const size_t size_;
  std::function<void()> on_produced_and_committed_;
};

int FuzzSharedMemory(const uint8_t* data, size_t size);

int FuzzSharedMemory(const uint8_t* data, size_t size) {
  base::TestTaskRunner task_runner;

  TestHelper helper(&task_runner);
  helper.StartServiceIfRequired();

  auto cp =
      helper.WrapTask(task_runner.CreateCheckpoint("produced.and.committed"));
  FuzzerFakeProducerThread producer_thread(data, size, cp);
  producer_thread.Connect();

  helper.ConnectConsumer();
  helper.WaitForConsumerConnect();

  TraceConfig trace_config;
  trace_config.add_buffers()->set_size_kb(8);

  auto* ds_config = trace_config.add_data_sources()->mutable_config();
  ds_config->set_name("android.perfetto.FakeProducer");
  ds_config->set_target_buffer(0);

  helper.StartTracing(trace_config);
  task_runner.RunUntilCheckpoint("produced.and.committed");

  helper.ReadData();
  helper.WaitForReadData();

  return 0;
}

}  // namespace
}  // namespace shm_fuzz
}  // namespace perfetto

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size);

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  return perfetto::shm_fuzz::FuzzSharedMemory(data, size);
}
