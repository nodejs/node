/*
 * Copyright (C) 2020 The Android Open Source Project
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

#include "src/traced/probes/initial_display_state/initial_display_state_data_source.h"
#include "perfetto/tracing/core/data_source_config.h"
#include "src/base/test/test_task_runner.h"
#include "src/tracing/core/trace_writer_for_testing.h"
#include "test/gtest_and_gmock.h"

#include "protos/perfetto/trace/android/initial_display_state.gen.h"
#include "protos/perfetto/trace/trace_packet.gen.h"

using ::testing::AnyOf;
using ::testing::ElementsAre;
using ::testing::Return;

namespace perfetto {
namespace {

class TestInitialDisplayStateDataSource : public InitialDisplayStateDataSource {
 public:
  TestInitialDisplayStateDataSource(base::TaskRunner* task_runner,
                                    const DataSourceConfig& config,
                                    std::unique_ptr<TraceWriter> writer)
      : InitialDisplayStateDataSource(task_runner,
                                      config,
                                      /* session_id */ 0,
                                      std::move(writer)) {}

  MOCK_METHOD1(ReadProperty,
               const base::Optional<std::string>(const std::string));
};

class InitialDisplayStateDataSourceTest : public ::testing::Test {
 protected:
  std::unique_ptr<TestInitialDisplayStateDataSource>
  GetInitialDisplayStateDataSource(const DataSourceConfig& config) {
    auto writer =
        std::unique_ptr<TraceWriterForTesting>(new TraceWriterForTesting());
    writer_raw_ = writer.get();
    auto instance = std::unique_ptr<TestInitialDisplayStateDataSource>(
        new TestInitialDisplayStateDataSource(&task_runner_, config,
                                              std::move(writer)));
    return instance;
  }

  base::TestTaskRunner task_runner_;
  TraceWriterForTesting* writer_raw_ = nullptr;
};

TEST_F(InitialDisplayStateDataSourceTest, Success) {
  ASSERT_TRUE(true);
  auto data_source = GetInitialDisplayStateDataSource(DataSourceConfig());
  EXPECT_CALL(*data_source, ReadProperty("debug.tracing.screen_state"))
      .WillOnce(Return(base::make_optional("2")));
  EXPECT_CALL(*data_source, ReadProperty("debug.tracing.screen_brightness"))
      .WillOnce(Return(base::make_optional("0.123456")));
  data_source->Start();

  protos::gen::TracePacket packet = writer_raw_->GetOnlyTracePacket();
  ASSERT_TRUE(packet.has_initial_display_state());
  auto state = packet.initial_display_state();
  ASSERT_EQ(state.display_state(), 2);
  ASSERT_EQ(state.brightness(), 0.123456);
}

TEST_F(InitialDisplayStateDataSourceTest, Invalid) {
  ASSERT_TRUE(true);
  auto data_source = GetInitialDisplayStateDataSource(DataSourceConfig());
  EXPECT_CALL(*data_source, ReadProperty("debug.tracing.screen_state"))
      .WillOnce(Return(base::make_optional("2")));
  EXPECT_CALL(*data_source, ReadProperty("debug.tracing.screen_brightness"))
      .WillOnce(Return(base::make_optional("gotta wear shades")));
  data_source->Start();

  protos::gen::TracePacket packet = writer_raw_->GetOnlyTracePacket();
  ASSERT_TRUE(packet.has_initial_display_state());
  auto state = packet.initial_display_state();
  ASSERT_EQ(state.display_state(), 2);
  ASSERT_FALSE(state.has_brightness());
}

TEST_F(InitialDisplayStateDataSourceTest, Failure) {
  ASSERT_TRUE(true);
  auto data_source = GetInitialDisplayStateDataSource(DataSourceConfig());
  EXPECT_CALL(*data_source, ReadProperty("debug.tracing.screen_state"))
      .WillOnce(Return(base::nullopt));
  EXPECT_CALL(*data_source, ReadProperty("debug.tracing.screen_brightness"))
      .WillOnce(Return(base::nullopt));
  data_source->Start();

  protos::gen::TracePacket packet = writer_raw_->GetOnlyTracePacket();
  ASSERT_FALSE(packet.has_initial_display_state());
}

}  // namespace
}  // namespace perfetto
