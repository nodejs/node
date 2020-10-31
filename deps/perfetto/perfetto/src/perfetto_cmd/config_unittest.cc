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

#include "src/perfetto_cmd/config.h"

#include "test/gtest_and_gmock.h"

#include "perfetto/tracing/core/data_source_config.h"
#include "perfetto/tracing/core/trace_config.h"

#include "protos/perfetto/config/ftrace/ftrace_config.gen.h"

namespace perfetto {
namespace {

using testing::Contains;

class CreateConfigFromOptionsTest : public ::testing::Test {
 public:
  TraceConfig config;
  ConfigOptions options;
};

TEST_F(CreateConfigFromOptionsTest, Default) {
  ASSERT_TRUE(CreateConfigFromOptions(options, &config));
  EXPECT_FALSE(config.write_into_file());
}

TEST_F(CreateConfigFromOptionsTest, MilliSeconds) {
  options.time = "2ms";
  ASSERT_TRUE(CreateConfigFromOptions(options, &config));
  EXPECT_EQ(config.duration_ms(), 2u);
}

TEST_F(CreateConfigFromOptionsTest, Seconds) {
  options.time = "100s";
  ASSERT_TRUE(CreateConfigFromOptions(options, &config));
  EXPECT_EQ(config.duration_ms(), 100 * 1000u);
}

TEST_F(CreateConfigFromOptionsTest, Minutes) {
  options.time = "2m";
  ASSERT_TRUE(CreateConfigFromOptions(options, &config));
  EXPECT_EQ(config.duration_ms(), 2 * 60 * 1000u);
}

TEST_F(CreateConfigFromOptionsTest, Hours) {
  options.time = "2h";
  ASSERT_TRUE(CreateConfigFromOptions(options, &config));
  EXPECT_EQ(config.duration_ms(), 2 * 60 * 60 * 1000u);
}

TEST_F(CreateConfigFromOptionsTest, Kilobyte) {
  options.buffer_size = "2kb";
  ASSERT_TRUE(CreateConfigFromOptions(options, &config));
  EXPECT_EQ(config.buffers()[0].size_kb(), 2u);
}

TEST_F(CreateConfigFromOptionsTest, Megabyte) {
  options.buffer_size = "2mb";
  ASSERT_TRUE(CreateConfigFromOptions(options, &config));
  EXPECT_EQ(config.buffers()[0].size_kb(), 2 * 1024u);
}

TEST_F(CreateConfigFromOptionsTest, Gigabyte) {
  options.buffer_size = "2gb";
  ASSERT_TRUE(CreateConfigFromOptions(options, &config));
  EXPECT_EQ(config.buffers()[0].size_kb(), 2 * 1024 * 1024u);
}

TEST_F(CreateConfigFromOptionsTest, BadTrailingSpace) {
  options.buffer_size = "2gb ";
  ASSERT_FALSE(CreateConfigFromOptions(options, &config));
}

TEST_F(CreateConfigFromOptionsTest, UnmatchedUnit) {
  options.buffer_size = "2ly";
  ASSERT_FALSE(CreateConfigFromOptions(options, &config));
}

TEST_F(CreateConfigFromOptionsTest, OnlyNumber) {
  options.buffer_size = "2";
  ASSERT_FALSE(CreateConfigFromOptions(options, &config));
}

TEST_F(CreateConfigFromOptionsTest, OnlyUnit) {
  options.buffer_size = "kb";
  ASSERT_FALSE(CreateConfigFromOptions(options, &config));
}

TEST_F(CreateConfigFromOptionsTest, Empty) {
  options.buffer_size = "";
  ASSERT_TRUE(CreateConfigFromOptions(options, &config));
}

TEST_F(CreateConfigFromOptionsTest, InvalidTime) {
  options.time = "2mb";
  ASSERT_FALSE(CreateConfigFromOptions(options, &config));
}

TEST_F(CreateConfigFromOptionsTest, InvalidSize) {
  options.max_file_size = "2s";
  ASSERT_FALSE(CreateConfigFromOptions(options, &config));
}

TEST_F(CreateConfigFromOptionsTest, FullConfig) {
  options.buffer_size = "100mb";
  options.max_file_size = "1gb";
  options.time = "1h";
  options.categories.push_back("sw");
  options.categories.push_back("sched/sched_switch");
  options.atrace_apps.push_back("com.android.chrome");
  ASSERT_TRUE(CreateConfigFromOptions(options, &config));
  EXPECT_EQ(config.duration_ms(), 60 * 60 * 1000u);
  EXPECT_EQ(config.flush_period_ms(), 30 * 1000u);
  EXPECT_EQ(config.max_file_size_bytes(), 1 * 1024 * 1024 * 1024u);
  EXPECT_EQ(config.buffers()[0].size_kb(), 100 * 1024u);
  EXPECT_EQ(config.data_sources()[0].config().name(), "linux.ftrace");
  EXPECT_EQ(config.data_sources()[0].config().target_buffer(), 0u);

  protos::gen::FtraceConfig ftrace;
  ASSERT_TRUE(ftrace.ParseFromString(
      config.data_sources()[0].config().ftrace_config_raw()));
  EXPECT_THAT(ftrace.ftrace_events(), Contains("sched/sched_switch"));
  EXPECT_THAT(ftrace.atrace_categories(), Contains("sw"));
  EXPECT_THAT(ftrace.atrace_apps(), Contains("com.android.chrome"));
}

}  // namespace
}  // namespace perfetto
