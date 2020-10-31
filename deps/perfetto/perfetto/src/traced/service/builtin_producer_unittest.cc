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

#include "src/traced/service/builtin_producer.h"

#include "perfetto/tracing/core/data_source_config.h"
#include "src/base/test/test_task_runner.h"
#include "test/gtest_and_gmock.h"

namespace perfetto {
namespace {

constexpr char kHeapprofdDataSourceName[] = "android.heapprofd";
constexpr char kTracedPerfDataSourceName[] = "linux.perf";
constexpr char kLazyHeapprofdPropertyName[] = "traced.lazy.heapprofd";
constexpr char kLazyTracedPerfPropertyName[] = "traced.lazy.traced_perf";

using ::testing::_;
using ::testing::InvokeWithoutArgs;
using ::testing::Mock;
using ::testing::Return;
using ::testing::StrictMock;

class MockBuiltinProducer : public BuiltinProducer {
 public:
  MockBuiltinProducer(base::TaskRunner* task_runner)
      : BuiltinProducer(task_runner, /*lazy_stop_delay_ms=*/0) {}

  MOCK_METHOD2(SetAndroidProperty,
               bool(const std::string&, const std::string&));
};

TEST(BuiltinProducerTest, LazyHeapprofdSimple) {
  DataSourceConfig cfg;
  cfg.set_name(kHeapprofdDataSourceName);
  base::TestTaskRunner task_runner;
  auto done = task_runner.CreateCheckpoint("done");
  StrictMock<MockBuiltinProducer> p(&task_runner);
  testing::InSequence s;
  EXPECT_CALL(p, SetAndroidProperty(kLazyHeapprofdPropertyName, "1"))
      .WillOnce(Return(true));
  EXPECT_CALL(p, SetAndroidProperty(kLazyHeapprofdPropertyName, ""))
      .WillOnce(InvokeWithoutArgs([&done]() {
        done();
        return true;
      }));
  p.SetupDataSource(1, cfg);
  p.StopDataSource(1);
  task_runner.RunUntilCheckpoint("done");
}

TEST(BuiltinProducerTest, LazyTracedPerfSimple) {
  DataSourceConfig cfg;
  cfg.set_name(kTracedPerfDataSourceName);
  base::TestTaskRunner task_runner;
  auto done = task_runner.CreateCheckpoint("done");
  StrictMock<MockBuiltinProducer> p(&task_runner);
  testing::InSequence s;
  EXPECT_CALL(p, SetAndroidProperty(kLazyTracedPerfPropertyName, "1"))
      .WillOnce(Return(true));
  EXPECT_CALL(p, SetAndroidProperty(kLazyTracedPerfPropertyName, ""))
      .WillOnce(InvokeWithoutArgs([&done]() {
        done();
        return true;
      }));
  p.SetupDataSource(1, cfg);
  p.StopDataSource(1);
  task_runner.RunUntilCheckpoint("done");
}

TEST(BuiltinProducerTest, LazyHeapprofdRefCount) {
  DataSourceConfig cfg;
  cfg.set_name(kHeapprofdDataSourceName);
  base::TestTaskRunner task_runner;
  auto done = task_runner.CreateCheckpoint("done");
  StrictMock<MockBuiltinProducer> p(&task_runner);
  testing::InSequence s;
  EXPECT_CALL(p, SetAndroidProperty(kLazyHeapprofdPropertyName, "1"))
      .WillRepeatedly(Return(true));
  p.SetupDataSource(1, cfg);
  p.SetupDataSource(2, cfg);
  p.StopDataSource(2);
  task_runner.RunUntilIdle();
  EXPECT_CALL(p, SetAndroidProperty(kLazyHeapprofdPropertyName, ""))
      .WillOnce(InvokeWithoutArgs([&done]() {
        done();
        return true;
      }));
  p.StopDataSource(1);
  task_runner.RunUntilCheckpoint("done");
}

TEST(BuiltinProducerTest, LazyHeapprofdNoFlap) {
  DataSourceConfig cfg;
  cfg.set_name(kHeapprofdDataSourceName);
  base::TestTaskRunner task_runner;
  auto done = task_runner.CreateCheckpoint("done");
  StrictMock<MockBuiltinProducer> p(&task_runner);
  testing::InSequence s;
  EXPECT_CALL(p, SetAndroidProperty(kLazyHeapprofdPropertyName, "1"))
      .WillRepeatedly(Return(true));
  p.SetupDataSource(1, cfg);
  p.StopDataSource(1);
  p.SetupDataSource(2, cfg);
  task_runner.RunUntilIdle();
  p.StopDataSource(2);
  EXPECT_CALL(p, SetAndroidProperty(kLazyHeapprofdPropertyName, ""))
      .WillOnce(InvokeWithoutArgs([&done]() {
        done();
        return true;
      }));
  task_runner.RunUntilCheckpoint("done");
}

TEST(BuiltinProducerTest, LazyRefCountsIndependent) {
  DataSourceConfig cfg_perf;
  cfg_perf.set_name(kTracedPerfDataSourceName);
  DataSourceConfig cfg_heap;
  cfg_heap.set_name(kHeapprofdDataSourceName);

  base::TestTaskRunner task_runner;
  StrictMock<MockBuiltinProducer> p(&task_runner);
  testing::InSequence s;

  // start one instance of both types of sources
  EXPECT_CALL(p, SetAndroidProperty(kLazyHeapprofdPropertyName, "1"))
      .WillOnce(Return(true));
  EXPECT_CALL(p, SetAndroidProperty(kLazyTracedPerfPropertyName, "1"))
      .WillOnce(Return(true));
  p.SetupDataSource(1, cfg_heap);
  p.SetupDataSource(2, cfg_perf);
  task_runner.RunUntilIdle();
  Mock::VerifyAndClearExpectations(&p);

  // stop heapprofd source
  EXPECT_CALL(p, SetAndroidProperty(kLazyHeapprofdPropertyName, ""))
      .WillOnce(Return(true));
  p.StopDataSource(1);
  task_runner.RunUntilIdle();
  Mock::VerifyAndClearExpectations(&p);

  // stop traced_perf source
  EXPECT_CALL(p, SetAndroidProperty(kLazyTracedPerfPropertyName, ""))
      .WillOnce(Return(true));
  p.StopDataSource(2);
  task_runner.RunUntilIdle();
  Mock::VerifyAndClearExpectations(&p);
}

}  // namespace
}  // namespace perfetto
