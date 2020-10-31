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

#include "test/test_helper.h"

#include "perfetto/ext/traced/traced.h"
#include "perfetto/ext/tracing/core/trace_packet.h"
#include "perfetto/ext/tracing/ipc/default_socket.h"
#include "perfetto/tracing/core/tracing_service_state.h"

#include "protos/perfetto/trace/trace_packet.pbzero.h"

namespace perfetto {

uint64_t TestHelper::next_instance_num_ = 0;

// If we're building on Android and starting the daemons ourselves,
// create the sockets in a world-writable location.
#if PERFETTO_BUILDFLAG(PERFETTO_OS_ANDROID) && \
    PERFETTO_BUILDFLAG(PERFETTO_START_DAEMONS)
#define TEST_PRODUCER_SOCK_NAME "/data/local/tmp/traced_producer"
#define TEST_CONSUMER_SOCK_NAME "/data/local/tmp/traced_consumer"
#else
#define TEST_PRODUCER_SOCK_NAME ::perfetto::GetProducerSocket()
#define TEST_CONSUMER_SOCK_NAME ::perfetto::GetConsumerSocket()
#endif

TestHelper::TestHelper(base::TestTaskRunner* task_runner)
    : instance_num_(next_instance_num_++),
      task_runner_(task_runner),
      service_thread_(TEST_PRODUCER_SOCK_NAME, TEST_CONSUMER_SOCK_NAME),
      fake_producer_thread_(TEST_PRODUCER_SOCK_NAME,
                            WrapTask(CreateCheckpoint("producer.connect")),
                            WrapTask(CreateCheckpoint("producer.setup")),
                            WrapTask(CreateCheckpoint("producer.enabled"))) {}

void TestHelper::OnConnect() {
  std::move(on_connect_callback_)();
}

void TestHelper::OnDisconnect() {
  PERFETTO_FATAL("Consumer unexpectedly disconnected from the service");
}

void TestHelper::OnTracingDisabled() {
  std::move(on_stop_tracing_callback_)();
}

void TestHelper::OnTraceData(std::vector<TracePacket> packets, bool has_more) {
  for (auto& encoded_packet : packets) {
    protos::gen::TracePacket packet;
    PERFETTO_CHECK(
        packet.ParseFromString(encoded_packet.GetRawBytesForTesting()));
    if (packet.has_clock_snapshot() || packet.has_trace_config() ||
        packet.has_trace_stats() || !packet.synchronization_marker().empty() ||
        packet.has_system_info() || packet.has_service_event()) {
      continue;
    }
    PERFETTO_CHECK(packet.has_trusted_uid());
    trace_.push_back(std::move(packet));
  }

  if (!has_more) {
    std::move(on_packets_finished_callback_)();
  }
}

void TestHelper::StartServiceIfRequired() {
#if PERFETTO_BUILDFLAG(PERFETTO_START_DAEMONS)
  service_thread_.Start();
#endif
}

FakeProducer* TestHelper::ConnectFakeProducer() {
  fake_producer_thread_.Connect();
  // This will wait until the service has seen the RegisterDataSource() call
  // (because of the Sync() in FakeProducer::OnConnect()).
  RunUntilCheckpoint("producer.connect");
  return fake_producer_thread_.producer();
}

void TestHelper::ConnectConsumer() {
  cur_consumer_num_++;
  on_connect_callback_ = CreateCheckpoint("consumer.connected." +
                                          std::to_string(cur_consumer_num_));
  endpoint_ =
      ConsumerIPCClient::Connect(TEST_CONSUMER_SOCK_NAME, this, task_runner_);
}

void TestHelper::DetachConsumer(const std::string& key) {
  on_detach_callback_ = CreateCheckpoint("detach." + key);
  endpoint_->Detach(key);
  RunUntilCheckpoint("detach." + key);
  endpoint_.reset();
}

bool TestHelper::AttachConsumer(const std::string& key) {
  bool success = false;
  auto checkpoint = CreateCheckpoint("attach." + key);
  on_attach_callback_ = [&success, checkpoint](bool s) {
    success = s;
    checkpoint();
  };
  endpoint_->Attach(key);
  RunUntilCheckpoint("attach." + key);
  return success;
}

void TestHelper::CreateProducerProvidedSmb() {
  fake_producer_thread_.CreateProducerProvidedSmb();
}

bool TestHelper::IsShmemProvidedByProducer() {
  return fake_producer_thread_.producer()->IsShmemProvidedByProducer();
}

void TestHelper::ProduceStartupEventBatch(
    const protos::gen::TestConfig& config) {
  auto on_data_written = CreateCheckpoint("startup_data_written");
  fake_producer_thread_.ProduceStartupEventBatch(config,
                                                 WrapTask(on_data_written));
  RunUntilCheckpoint("startup_data_written");
}

void TestHelper::StartTracing(const TraceConfig& config,
                              base::ScopedFile file) {
  trace_.clear();
  on_stop_tracing_callback_ = CreateCheckpoint("stop.tracing");
  endpoint_->EnableTracing(config, std::move(file));
}

void TestHelper::DisableTracing() {
  endpoint_->DisableTracing();
}

void TestHelper::FlushAndWait(uint32_t timeout_ms) {
  static int flush_num = 0;
  std::string checkpoint_name = "flush." + std::to_string(flush_num++);
  auto checkpoint = CreateCheckpoint(checkpoint_name);
  endpoint_->Flush(timeout_ms, [checkpoint](bool) { checkpoint(); });
  RunUntilCheckpoint(checkpoint_name, timeout_ms + 1000);
}

void TestHelper::ReadData(uint32_t read_count) {
  on_packets_finished_callback_ =
      CreateCheckpoint("readback.complete." + std::to_string(read_count));
  endpoint_->ReadBuffers();
}

void TestHelper::WaitForConsumerConnect() {
  RunUntilCheckpoint("consumer.connected." + std::to_string(cur_consumer_num_));
}

void TestHelper::WaitForProducerSetup() {
  RunUntilCheckpoint("producer.setup");
}

void TestHelper::WaitForProducerEnabled() {
  RunUntilCheckpoint("producer.enabled");
}

void TestHelper::WaitForTracingDisabled(uint32_t timeout_ms) {
  RunUntilCheckpoint("stop.tracing", timeout_ms);
}

void TestHelper::WaitForReadData(uint32_t read_count, uint32_t timeout_ms) {
  RunUntilCheckpoint("readback.complete." + std::to_string(read_count),
                     timeout_ms);
}

void TestHelper::SyncAndWaitProducer() {
  static int sync_id = 0;
  std::string checkpoint_name = "producer_sync_" + std::to_string(++sync_id);
  auto checkpoint = CreateCheckpoint(checkpoint_name);
  fake_producer_thread_.producer()->Sync(
      [this, &checkpoint] { task_runner_->PostTask(checkpoint); });
  RunUntilCheckpoint(checkpoint_name);
}

TracingServiceState TestHelper::QueryServiceStateAndWait() {
  TracingServiceState res;
  auto checkpoint = CreateCheckpoint("query_svc_state");
  auto callback = [&checkpoint, &res](bool, const TracingServiceState& tss) {
    res = tss;
    checkpoint();
  };
  endpoint_->QueryServiceState(callback);
  RunUntilCheckpoint("query_svc_state");
  return res;
}

std::function<void()> TestHelper::WrapTask(
    const std::function<void()>& function) {
  return [this, function] { task_runner_->PostTask(function); };
}

void TestHelper::OnDetach(bool) {
  if (on_detach_callback_)
    std::move(on_detach_callback_)();
}

void TestHelper::OnAttach(bool success, const TraceConfig&) {
  if (on_attach_callback_)
    std::move(on_attach_callback_)(success);
}

void TestHelper::OnTraceStats(bool, const TraceStats&) {}

void TestHelper::OnObservableEvents(const ObservableEvents&) {}

// static
const char* TestHelper::GetConsumerSocketName() {
  return TEST_CONSUMER_SOCK_NAME;
}

// static
const char* TestHelper::GetProducerSocketName() {
  return TEST_PRODUCER_SOCK_NAME;
}

}  // namespace perfetto
