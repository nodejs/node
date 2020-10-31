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

#include <random>

#include "perfetto/base/logging.h"
#include "perfetto/tracing/core/data_source_config.h"
#include "src/base/test/test_task_runner.h"
#include "test/cts/utils.h"
#include "test/gtest_and_gmock.h"
#include "test/test_helper.h"

#include "protos/perfetto/config/profiling/heapprofd_config.gen.h"
#include "protos/perfetto/trace/profiling/profile_common.gen.h"
#include "protos/perfetto/trace/profiling/profile_packet.gen.h"
#include "protos/perfetto/trace/trace_packet.gen.h"

namespace perfetto {
namespace {

// Size of individual (repeated) allocations done by the test apps (must be kept
// in sync with their sources).
constexpr uint64_t kTestSamplingInterval = 4096;
constexpr uint64_t kExpectedIndividualAllocSz = 4153;
// Tests rely on the sampling behaviour where allocations larger than the
// sampling interval are recorded at their actual size.
static_assert(kExpectedIndividualAllocSz > kTestSamplingInterval,
              "kTestSamplingInterval invalid");

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

std::vector<protos::gen::TracePacket> ProfileRuntime(
    const std::string& app_name,
    const bool enable_extra_guardrails = false) {
  base::TestTaskRunner task_runner;

  // (re)start the target app's main activity
  if (IsAppRunning(app_name)) {
    StopApp(app_name, "old.app.stopped", &task_runner);
    task_runner.RunUntilCheckpoint("old.app.stopped", 1000 /*ms*/);
  }
  StartAppActivity(app_name, "MainActivity", "target.app.running", &task_runner,
                   /*delay_ms=*/100);
  task_runner.RunUntilCheckpoint("target.app.running", 1000 /*ms*/);

  // set up tracing
  TestHelper helper(&task_runner);
  helper.ConnectConsumer();
  helper.WaitForConsumerConnect();

  TraceConfig trace_config;
  trace_config.add_buffers()->set_size_kb(10 * 1024);
  trace_config.set_duration_ms(4000);
  trace_config.set_enable_extra_guardrails(enable_extra_guardrails);
  trace_config.set_unique_session_name(RandomSessionName().c_str());

  auto* ds_config = trace_config.add_data_sources()->mutable_config();
  ds_config->set_name("android.heapprofd");
  ds_config->set_target_buffer(0);

  protos::gen::HeapprofdConfig heapprofd_config;
  heapprofd_config.set_sampling_interval_bytes(kTestSamplingInterval);
  heapprofd_config.add_process_cmdline(app_name.c_str());
  heapprofd_config.set_block_client(true);
  heapprofd_config.set_all(false);
  ds_config->set_heapprofd_config_raw(heapprofd_config.SerializeAsString());

  // start tracing
  helper.StartTracing(trace_config);
  helper.WaitForTracingDisabled(10000 /*ms*/);
  helper.ReadData();
  helper.WaitForReadData();

  return helper.trace();
}

std::vector<protos::gen::TracePacket> ProfileStartup(
    const std::string& app_name,
    const bool enable_extra_guardrails = false) {
  base::TestTaskRunner task_runner;

  if (IsAppRunning(app_name)) {
    StopApp(app_name, "old.app.stopped", &task_runner);
    task_runner.RunUntilCheckpoint("old.app.stopped", 1000 /*ms*/);
  }

  // set up tracing
  TestHelper helper(&task_runner);
  helper.ConnectConsumer();
  helper.WaitForConsumerConnect();

  TraceConfig trace_config;
  trace_config.add_buffers()->set_size_kb(10 * 1024);
  trace_config.set_duration_ms(4000);
  trace_config.set_enable_extra_guardrails(enable_extra_guardrails);
  trace_config.set_unique_session_name(RandomSessionName().c_str());

  auto* ds_config = trace_config.add_data_sources()->mutable_config();
  ds_config->set_name("android.heapprofd");
  ds_config->set_target_buffer(0);

  protos::gen::HeapprofdConfig heapprofd_config;
  heapprofd_config.set_sampling_interval_bytes(kTestSamplingInterval);
  heapprofd_config.add_process_cmdline(app_name.c_str());
  heapprofd_config.set_block_client(true);
  heapprofd_config.set_all(false);
  ds_config->set_heapprofd_config_raw(heapprofd_config.SerializeAsString());

  // start tracing
  helper.StartTracing(trace_config);

  // start app
  StartAppActivity(app_name, "MainActivity", "target.app.running", &task_runner,
                   /*delay_ms=*/100);
  task_runner.RunUntilCheckpoint("target.app.running", 2000 /*ms*/);

  helper.WaitForTracingDisabled(8000 /*ms*/);
  helper.ReadData();
  helper.WaitForReadData();

  return helper.trace();
}

void AssertExpectedAllocationsPresent(
    const std::vector<protos::gen::TracePacket>& packets) {
  ASSERT_GT(packets.size(), 0u);

  // TODO(rsavitski): assert particular stack frames once we clarify the
  // expected behaviour of unwinding native libs within an apk.
  // Until then, look for an allocation that is a multiple of the expected
  // allocation size.
  bool found_alloc = false;
  bool found_proc_dump = false;
  for (const auto& packet : packets) {
    for (const auto& proc_dump : packet.profile_packet().process_dumps()) {
      found_proc_dump = true;
      for (const auto& sample : proc_dump.samples()) {
        if (sample.self_allocated() > 0 &&
            sample.self_allocated() % kExpectedIndividualAllocSz == 0) {
          found_alloc = true;

          EXPECT_TRUE(sample.self_freed() > 0 &&
                      sample.self_freed() % kExpectedIndividualAllocSz == 0)
              << "self_freed: " << sample.self_freed();
        }
      }
    }
  }
  ASSERT_TRUE(found_proc_dump);
  ASSERT_TRUE(found_alloc);
}

void AssertNoProfileContents(
    const std::vector<protos::gen::TracePacket>& packets) {
  // If profile packets are present, they must be empty.
  for (const auto& packet : packets) {
    ASSERT_EQ(packet.profile_packet().process_dumps_size(), 0);
  }
}

TEST(HeapprofdCtsTest, DebuggableAppRuntime) {
  std::string app_name = "android.perfetto.cts.app.debuggable";
  const auto& packets = ProfileRuntime(app_name);
  AssertExpectedAllocationsPresent(packets);
  StopApp(app_name);
}

TEST(HeapprofdCtsTest, DebuggableAppStartup) {
  std::string app_name = "android.perfetto.cts.app.debuggable";
  const auto& packets = ProfileStartup(app_name);
  AssertExpectedAllocationsPresent(packets);
  StopApp(app_name);
}

TEST(HeapprofdCtsTest, ProfileableAppRuntime) {
  std::string app_name = "android.perfetto.cts.app.profileable";
  const auto& packets = ProfileRuntime(app_name);
  AssertExpectedAllocationsPresent(packets);
  StopApp(app_name);
}

TEST(HeapprofdCtsTest, ProfileableAppStartup) {
  std::string app_name = "android.perfetto.cts.app.profileable";
  const auto& packets = ProfileStartup(app_name);
  AssertExpectedAllocationsPresent(packets);
  StopApp(app_name);
}

TEST(HeapprofdCtsTest, ProfileableAppRuntimeExtraGuardrails) {
  std::string app_name = "android.perfetto.cts.app.profileable";
  const auto& packets = ProfileRuntime(app_name,
                                       /*enable_extra_guardrails=*/true);

  if (IsUserBuild())
    AssertNoProfileContents(packets);
  else
    AssertExpectedAllocationsPresent(packets);
  StopApp(app_name);
}

TEST(HeapprofdCtsTest, ProfileableAppStartupExtraGuardrails) {
  std::string app_name = "android.perfetto.cts.app.profileable";
  const auto& packets = ProfileStartup(app_name,
                                       /*enable_extra_guardrails=*/
                                       true);
  if (IsUserBuild())
    AssertNoProfileContents(packets);
  else
    AssertExpectedAllocationsPresent(packets);
  StopApp(app_name);
}

TEST(HeapprofdCtsTest, ReleaseAppRuntime) {
  std::string app_name = "android.perfetto.cts.app.release";
  const auto& packets = ProfileRuntime(app_name);

  if (IsDebuggableBuild())
    AssertExpectedAllocationsPresent(packets);
  else
    AssertNoProfileContents(packets);

  StopApp(app_name);
}

TEST(HeapprofdCtsTest, ReleaseAppStartup) {
  std::string app_name = "android.perfetto.cts.app.release";
  const auto& packets = ProfileStartup(app_name);

  if (IsDebuggableBuild())
    AssertExpectedAllocationsPresent(packets);
  else
    AssertNoProfileContents(packets);

  StopApp(app_name);
}

}  // namespace
}  // namespace perfetto
