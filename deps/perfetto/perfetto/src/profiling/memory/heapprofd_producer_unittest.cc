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

#include "src/profiling/memory/heapprofd_producer.h"

#include "perfetto/ext/tracing/core/basic_types.h"
#include "perfetto/ext/tracing/core/commit_data_request.h"
#include "perfetto/tracing/core/data_source_descriptor.h"
#include "src/base/test/test_task_runner.h"
#include "test/gtest_and_gmock.h"

namespace perfetto {
namespace profiling {

using ::testing::Contains;
using ::testing::Pair;
using ::testing::Eq;
using ::testing::Property;

class MockProducerEndpoint : public TracingService::ProducerEndpoint {
 public:
  MOCK_METHOD1(UnregisterDataSource, void(const std::string&));
  MOCK_METHOD1(NotifyFlushComplete, void(FlushRequestID));
  MOCK_METHOD1(NotifyDataSourceStarted, void(DataSourceInstanceID));
  MOCK_METHOD1(NotifyDataSourceStopped, void(DataSourceInstanceID));

  MOCK_CONST_METHOD0(shared_memory, SharedMemory*());
  MOCK_CONST_METHOD0(shared_buffer_page_size_kb, size_t());
  MOCK_METHOD2(CreateTraceWriter,
               std::unique_ptr<TraceWriter>(BufferID, BufferExhaustedPolicy));
  MOCK_METHOD0(MaybeSharedMemoryArbiter, SharedMemoryArbiter*());
  MOCK_CONST_METHOD0(IsShmemProvidedByProducer, bool());
  MOCK_METHOD1(ActivateTriggers, void(const std::vector<std::string>&));

  MOCK_METHOD1(RegisterDataSource, void(const DataSourceDescriptor&));
  MOCK_METHOD2(CommitData, void(const CommitDataRequest&, CommitDataCallback));
  MOCK_METHOD2(RegisterTraceWriter, void(uint32_t, uint32_t));
  MOCK_METHOD1(UnregisterTraceWriter, void(uint32_t));
  MOCK_METHOD1(Sync, void(std::function<void()>));
};

TEST(LogHistogramTest, Simple) {
  LogHistogram h;
  h.Add(1);
  h.Add(0);
  EXPECT_THAT(h.GetData(), Contains(Pair(2, 1)));
  EXPECT_THAT(h.GetData(), Contains(Pair(1, 1)));
}

TEST(LogHistogramTest, Overflow) {
  LogHistogram h;
  h.Add(std::numeric_limits<uint64_t>::max());
  EXPECT_THAT(h.GetData(), Contains(Pair(LogHistogram::kMaxBucket, 1)));
}

TEST(HeapprofdProducerTest, ExposesDataSource) {
  base::TestTaskRunner task_runner;
  HeapprofdProducer producer(HeapprofdMode::kCentral, &task_runner);

  std::unique_ptr<MockProducerEndpoint> endpoint(new MockProducerEndpoint());
  EXPECT_CALL(*endpoint,
              RegisterDataSource(Property(&DataSourceDescriptor::name,
                                          Eq("android.heapprofd"))))
      .Times(1);
  producer.SetProducerEndpoint(std::move(endpoint));
  producer.OnConnect();
}

}  // namespace profiling
}  // namespace perfetto
