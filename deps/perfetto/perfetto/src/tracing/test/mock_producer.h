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

#ifndef SRC_TRACING_TEST_MOCK_PRODUCER_H_
#define SRC_TRACING_TEST_MOCK_PRODUCER_H_

#include <map>
#include <memory>
#include <string>

#include "perfetto/ext/tracing/core/producer.h"
#include "perfetto/ext/tracing/core/shared_memory.h"
#include "perfetto/ext/tracing/core/trace_writer.h"
#include "perfetto/ext/tracing/core/tracing_service.h"
#include "test/gtest_and_gmock.h"

namespace perfetto {

namespace base {
class TestTaskRunner;
}

class MockProducer : public Producer {
 public:
  struct EnabledDataSource {
    DataSourceInstanceID id;
    BufferID target_buffer;
    TracingSessionID session_id;
  };

  explicit MockProducer(base::TestTaskRunner*);
  ~MockProducer() override;

  void Connect(TracingService* svc,
               const std::string& producer_name,
               uid_t uid = 42,
               size_t shared_memory_size_hint_bytes = 0,
               size_t shared_memory_page_size_hint_bytes = 0,
               std::unique_ptr<SharedMemory> shm = nullptr);
  void RegisterDataSource(const std::string& name,
                          bool ack_stop = false,
                          bool ack_start = false,
                          bool handle_incremental_state_clear = false);
  void UnregisterDataSource(const std::string& name);
  void RegisterTraceWriter(uint32_t writer_id, uint32_t target_buffer);
  void UnregisterTraceWriter(uint32_t writer_id);
  void WaitForTracingSetup();
  void WaitForDataSourceSetup(const std::string& name);
  void WaitForDataSourceStart(const std::string& name);
  void WaitForDataSourceStop(const std::string& name);
  DataSourceInstanceID GetDataSourceInstanceId(const std::string& name);
  const EnabledDataSource* GetDataSourceInstance(const std::string& name);
  std::unique_ptr<TraceWriter> CreateTraceWriter(
      const std::string& data_source_name);

  // Expect a flush. Flushes |writer_to_flush| if non-null. If |reply| is true,
  // replies to the flush request, otherwise ignores it and doesn't reply.
  void WaitForFlush(TraceWriter* writer_to_flush, bool reply = true);
  // Same as above, but with a vector of writers.
  void WaitForFlush(std::vector<TraceWriter*> writers_to_flush,
                    bool reply = true);

  TracingService::ProducerEndpoint* endpoint() {
    return service_endpoint_.get();
  }

  // Producer implementation.
  MOCK_METHOD0(OnConnect, void());
  MOCK_METHOD0(OnDisconnect, void());
  MOCK_METHOD2(SetupDataSource,
               void(DataSourceInstanceID, const DataSourceConfig&));
  MOCK_METHOD2(StartDataSource,
               void(DataSourceInstanceID, const DataSourceConfig&));
  MOCK_METHOD1(StopDataSource, void(DataSourceInstanceID));
  MOCK_METHOD0(OnTracingSetup, void());
  MOCK_METHOD3(Flush,
               void(FlushRequestID, const DataSourceInstanceID*, size_t));
  MOCK_METHOD2(ClearIncrementalState,
               void(const DataSourceInstanceID*, size_t));

 private:
  base::TestTaskRunner* const task_runner_;
  std::string producer_name_;
  std::unique_ptr<TracingService::ProducerEndpoint> service_endpoint_;
  std::map<std::string, EnabledDataSource> data_source_instances_;
};

}  // namespace perfetto

#endif  // SRC_TRACING_TEST_MOCK_PRODUCER_H_
