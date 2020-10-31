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

#include <stdlib.h>
#include <sys/system_properties.h>
#include <sys/types.h>
#include <sys/wait.h>

#include "perfetto/base/logging.h"
#include "perfetto/tracing/core/data_source_config.h"
#include "src/base/test/test_task_runner.h"
#include "test/cts/utils.h"
#include "test/gtest_and_gmock.h"
#include "test/test_helper.h"

#include "protos/perfetto/config/profiling/java_hprof_config.gen.h"
#include "protos/perfetto/trace/profiling/heap_graph.gen.h"
#include "protos/perfetto/trace/profiling/profile_common.gen.h"
#include "protos/perfetto/trace/trace_packet.gen.h"

namespace perfetto {
namespace {

std::string RandomSessionName() {
  std::random_device rd;
  std::default_random_engine generator(rd());
  std::uniform_int_distribution<char> distribution('a', 'z');

  constexpr size_t kSessionNameLen = 20;
  std::string result(kSessionNameLen, '\0');
  for (size_t i = 0; i < kSessionNameLen; ++i)
    result[i] = distribution(generator);
  return result;
}

std::vector<protos::gen::TracePacket> ProfileRuntime(std::string app_name) {
  base::TestTaskRunner task_runner;

  // (re)start the target app's main activity
  if (IsAppRunning(app_name)) {
    StopApp(app_name, "old.app.stopped", &task_runner);
    task_runner.RunUntilCheckpoint("old.app.stopped", 1000 /*ms*/);
  }
  StartAppActivity(app_name, "MainActivity", "target.app.running", &task_runner,
                   /*delay_ms=*/100);
  task_runner.RunUntilCheckpoint("target.app.running", 1000 /*ms*/);
  // If we try to dump too early in app initialization, we sometimes deadlock.
  sleep(1);

  // set up tracing
  TestHelper helper(&task_runner);
  helper.ConnectConsumer();
  helper.WaitForConsumerConnect();

  TraceConfig trace_config;
  trace_config.add_buffers()->set_size_kb(20 * 1024);
  trace_config.set_duration_ms(6000);
  trace_config.set_unique_session_name(RandomSessionName().c_str());

  auto* ds_config = trace_config.add_data_sources()->mutable_config();
  ds_config->set_name("android.java_hprof");
  ds_config->set_target_buffer(0);

  protos::gen::JavaHprofConfig java_hprof_config;
  java_hprof_config.add_process_cmdline(app_name.c_str());
  ds_config->set_java_hprof_config_raw(java_hprof_config.SerializeAsString());

  // start tracing
  helper.StartTracing(trace_config);
  helper.WaitForTracingDisabled(10000 /*ms*/);
  helper.ReadData();
  helper.WaitForReadData();
  PERFETTO_CHECK(IsAppRunning(app_name));
  StopApp(app_name, "new.app.stopped", &task_runner);
  task_runner.RunUntilCheckpoint("new.app.stopped", 1000 /*ms*/);
  return helper.trace();
}

void AssertGraphPresent(std::vector<protos::gen::TracePacket> packets) {
  ASSERT_GT(packets.size(), 0u);

  size_t objects = 0;
  size_t roots = 0;
  for (const auto& packet : packets) {
    objects += static_cast<size_t>(packet.heap_graph().objects_size());
    roots += static_cast<size_t>(packet.heap_graph().roots_size());
  }
  ASSERT_GT(objects, 0u);
  ASSERT_GT(roots, 0u);
}

void AssertNoProfileContents(std::vector<protos::gen::TracePacket> packets) {
  // If profile packets are present, they must be empty.
  for (const auto& packet : packets) {
    ASSERT_EQ(packet.heap_graph().roots_size(), 0);
    ASSERT_EQ(packet.heap_graph().objects_size(), 0);
    ASSERT_EQ(packet.heap_graph().type_names_size(), 0);
    ASSERT_EQ(packet.heap_graph().field_names_size(), 0);
  }
}

TEST(HeapprofdJavaCtsTest, DebuggableAppRuntime) {
  std::string app_name = "android.perfetto.cts.app.debuggable";
  const auto& packets = ProfileRuntime(app_name);
  AssertGraphPresent(packets);
}

TEST(HeapprofdJavaCtsTest, ProfileableAppRuntime) {
  std::string app_name = "android.perfetto.cts.app.profileable";
  const auto& packets = ProfileRuntime(app_name);
  AssertGraphPresent(packets);
}

TEST(HeapprofdJavaCtsTest, ReleaseAppRuntime) {
  std::string app_name = "android.perfetto.cts.app.release";
  const auto& packets = ProfileRuntime(app_name);

  if (IsDebuggableBuild())
    AssertGraphPresent(packets);
  else
    AssertNoProfileContents(packets);
}

}  // namespace
}  // namespace perfetto
