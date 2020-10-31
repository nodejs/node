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

#ifndef SRC_TRACING_TEST_MOCK_CONSUMER_H_
#define SRC_TRACING_TEST_MOCK_CONSUMER_H_

#include <memory>

#include "perfetto/ext/tracing/core/consumer.h"
#include "perfetto/ext/tracing/core/trace_packet.h"
#include "perfetto/ext/tracing/core/tracing_service.h"
#include "perfetto/tracing/core/tracing_service_state.h"
#include "test/gtest_and_gmock.h"

#include "protos/perfetto/trace/trace_packet.gen.h"

namespace perfetto {

namespace base {
class TestTaskRunner;
}

class MockConsumer : public Consumer {
 public:
  class FlushRequest {
   public:
    FlushRequest(std::function<bool(void)> wait_func) : wait_func_(wait_func) {}
    bool WaitForReply() { return wait_func_(); }

   private:
    std::function<bool(void)> wait_func_;
  };

  explicit MockConsumer(base::TestTaskRunner*);
  ~MockConsumer() override;

  void Connect(TracingService* svc, uid_t = 0);
  void EnableTracing(const TraceConfig&, base::ScopedFile = base::ScopedFile());
  void StartTracing();
  void ChangeTraceConfig(const TraceConfig&);
  void DisableTracing();
  void FreeBuffers();
  void WaitForTracingDisabled(uint32_t timeout_ms = 3000);
  FlushRequest Flush(uint32_t timeout_ms = 10000);
  std::vector<protos::gen::TracePacket> ReadBuffers();
  void GetTraceStats();
  void WaitForTraceStats(bool success);
  TracingServiceState QueryServiceState();
  void ObserveEvents(uint32_t enabled_event_types);
  ObservableEvents WaitForObservableEvents();

  TracingService::ConsumerEndpoint* endpoint() {
    return service_endpoint_.get();
  }

  // Consumer implementation.
  MOCK_METHOD0(OnConnect, void());
  MOCK_METHOD0(OnDisconnect, void());
  MOCK_METHOD0(OnTracingDisabled, void());
  MOCK_METHOD2(OnTraceData,
               void(std::vector<TracePacket>* /*packets*/, bool /*has_more*/));
  MOCK_METHOD1(OnDetach, void(bool));
  MOCK_METHOD2(OnAttach, void(bool, const TraceConfig&));
  MOCK_METHOD2(OnTraceStats, void(bool, const TraceStats&));
  MOCK_METHOD1(OnObservableEvents, void(const ObservableEvents&));

  // gtest doesn't support move-only types. This wrapper is here jut to pass
  // a pointer to the vector (rather than the vector itself) to the mock method.
  void OnTraceData(std::vector<TracePacket> packets, bool has_more) override {
    OnTraceData(&packets, has_more);
  }

 private:
  base::TestTaskRunner* const task_runner_;
  std::unique_ptr<TracingService::ConsumerEndpoint> service_endpoint_;
};

}  // namespace perfetto

#endif  // SRC_TRACING_TEST_MOCK_CONSUMER_H_
