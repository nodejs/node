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

#include "src/tracing/test/mock_consumer.h"

#include "perfetto/ext/tracing/core/trace_stats.h"
#include "perfetto/tracing/core/trace_config.h"
#include "src/base/test/test_task_runner.h"

using ::testing::_;
using ::testing::Invoke;

namespace perfetto {

MockConsumer::MockConsumer(base::TestTaskRunner* task_runner)
    : task_runner_(task_runner) {}

MockConsumer::~MockConsumer() {
  if (!service_endpoint_)
    return;
  static int i = 0;
  auto checkpoint_name = "on_consumer_disconnect_" + std::to_string(i++);
  auto on_disconnect = task_runner_->CreateCheckpoint(checkpoint_name);
  EXPECT_CALL(*this, OnDisconnect()).WillOnce(Invoke(on_disconnect));
  service_endpoint_.reset();
  task_runner_->RunUntilCheckpoint(checkpoint_name);
}

void MockConsumer::Connect(TracingService* svc, uid_t uid) {
  service_endpoint_ = svc->ConnectConsumer(this, uid);
  static int i = 0;
  auto checkpoint_name = "on_consumer_connect_" + std::to_string(i++);
  auto on_connect = task_runner_->CreateCheckpoint(checkpoint_name);
  EXPECT_CALL(*this, OnConnect()).WillOnce(Invoke(on_connect));
  task_runner_->RunUntilCheckpoint(checkpoint_name);
}

void MockConsumer::EnableTracing(const TraceConfig& trace_config,
                                 base::ScopedFile write_into_file) {
  service_endpoint_->EnableTracing(trace_config, std::move(write_into_file));
}

void MockConsumer::StartTracing() {
  service_endpoint_->StartTracing();
}

void MockConsumer::ChangeTraceConfig(const TraceConfig& trace_config) {
  service_endpoint_->ChangeTraceConfig(trace_config);
}

void MockConsumer::DisableTracing() {
  service_endpoint_->DisableTracing();
}

void MockConsumer::FreeBuffers() {
  service_endpoint_->FreeBuffers();
}

void MockConsumer::WaitForTracingDisabled(uint32_t timeout_ms) {
  static int i = 0;
  auto checkpoint_name = "on_tracing_disabled_consumer_" + std::to_string(i++);
  auto on_tracing_disabled = task_runner_->CreateCheckpoint(checkpoint_name);
  EXPECT_CALL(*this, OnTracingDisabled()).WillOnce(Invoke(on_tracing_disabled));
  task_runner_->RunUntilCheckpoint(checkpoint_name, timeout_ms);
}

MockConsumer::FlushRequest MockConsumer::Flush(uint32_t timeout_ms) {
  static int i = 0;
  auto checkpoint_name = "on_consumer_flush_" + std::to_string(i++);
  auto on_flush = task_runner_->CreateCheckpoint(checkpoint_name);
  std::shared_ptr<bool> result(new bool());
  service_endpoint_->Flush(timeout_ms, [result, on_flush](bool success) {
    *result = success;
    on_flush();
  });

  base::TestTaskRunner* task_runner = task_runner_;
  auto wait_for_flush_completion = [result, task_runner,
                                    checkpoint_name]() -> bool {
    task_runner->RunUntilCheckpoint(checkpoint_name);
    return *result;
  };

  return FlushRequest(wait_for_flush_completion);
}

std::vector<protos::gen::TracePacket> MockConsumer::ReadBuffers() {
  std::vector<protos::gen::TracePacket> decoded_packets;
  static int i = 0;
  std::string checkpoint_name = "on_read_buffers_" + std::to_string(i++);
  auto on_read_buffers = task_runner_->CreateCheckpoint(checkpoint_name);
  EXPECT_CALL(*this, OnTraceData(_, _))
      .WillRepeatedly(
          Invoke([&decoded_packets, on_read_buffers](
                     std::vector<TracePacket>* packets, bool has_more) {
            for (TracePacket& packet : *packets) {
              decoded_packets.emplace_back();
              protos::gen::TracePacket* decoded_packet =
                  &decoded_packets.back();
              decoded_packet->ParseFromString(packet.GetRawBytesForTesting());
            }
            if (!has_more)
              on_read_buffers();
          }));
  service_endpoint_->ReadBuffers();
  task_runner_->RunUntilCheckpoint(checkpoint_name);
  return decoded_packets;
}

void MockConsumer::GetTraceStats() {
  service_endpoint_->GetTraceStats();
}

void MockConsumer::WaitForTraceStats(bool success) {
  static int i = 0;
  auto checkpoint_name = "on_trace_stats_" + std::to_string(i++);
  auto on_trace_stats = task_runner_->CreateCheckpoint(checkpoint_name);
  auto result_callback = [on_trace_stats](bool, const TraceStats&) {
    on_trace_stats();
  };
  if (success) {
    EXPECT_CALL(*this,
                OnTraceStats(true, testing::Property(&TraceStats::total_buffers,
                                                     testing::Gt(0u))))
        .WillOnce(Invoke(result_callback));
  } else {
    EXPECT_CALL(*this, OnTraceStats(false, _))
        .WillOnce(Invoke(result_callback));
  }
  task_runner_->RunUntilCheckpoint(checkpoint_name);
}

void MockConsumer::ObserveEvents(uint32_t enabled_event_types) {
  service_endpoint_->ObserveEvents(enabled_event_types);
}

ObservableEvents MockConsumer::WaitForObservableEvents() {
  ObservableEvents events;
  static int i = 0;
  std::string checkpoint_name = "on_observable_events_" + std::to_string(i++);
  auto on_observable_events = task_runner_->CreateCheckpoint(checkpoint_name);
  EXPECT_CALL(*this, OnObservableEvents(_))
      .WillOnce(Invoke([&events, on_observable_events](
                           const ObservableEvents& observable_events) {
        events = observable_events;
        on_observable_events();
      }));
  task_runner_->RunUntilCheckpoint(checkpoint_name);
  return events;
}

TracingServiceState MockConsumer::QueryServiceState() {
  static int i = 0;
  TracingServiceState res;
  std::string checkpoint_name = "query_service_state_" + std::to_string(i++);
  auto checkpoint = task_runner_->CreateCheckpoint(checkpoint_name);
  auto callback = [checkpoint, &res](bool success,
                                     const TracingServiceState& svc_state) {
    EXPECT_TRUE(success);
    res = svc_state;
    checkpoint();
  };
  service_endpoint_->QueryServiceState(callback);
  task_runner_->RunUntilCheckpoint(checkpoint_name);
  return res;
}

}  // namespace perfetto
