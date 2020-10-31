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

#include <sys/system_properties.h>
#include <random>
#include "test/gtest_and_gmock.h"

#include "perfetto/tracing/core/data_source_config.h"
#include "src/base/test/test_task_runner.h"
#include "test/cts/utils.h"
#include "test/test_helper.h"

#include "protos/perfetto/config/test_config.gen.h"
#include "protos/perfetto/trace/test_event.gen.h"
#include "protos/perfetto/trace/trace_packet.gen.h"

namespace perfetto {

class PerfettoCtsTest : public ::testing::Test {
 protected:
  void TestMockProducer(const std::string& producer_name) {
    // Filter out watches; they do not have the required infrastructure to run
    // these tests.
    char chars[PROP_VALUE_MAX + 1];
    int ret = __system_property_get("ro.build.characteristics", chars);
    ASSERT_GE(ret, 0);
    std::string characteristics(chars);
    if (characteristics.find("watch") != std::string::npos) {
      return;
    }

    base::TestTaskRunner task_runner;

    const std::string app_name = "android.perfetto.producer";
    const std::string activity = "ProducerActivity";
    if (IsAppRunning(app_name)) {
      StopApp(app_name, "old.app.stopped", &task_runner);
      task_runner.RunUntilCheckpoint("old.app.stopped");
    }
    StartAppActivity(app_name, activity, "target.app.running", &task_runner,
                     /*delay_ms=*/100);
    task_runner.RunUntilCheckpoint("target.app.running");

    const std::string isolated_process_name =
        "android.perfetto.producer:android.perfetto.producer."
        "ProducerIsolatedService";
    WaitForProcess(isolated_process_name, "isolated.service.running",
                   &task_runner, 1000 /*ms*/);
    task_runner.RunUntilCheckpoint("isolated.service.running");

    TestHelper helper(&task_runner);
    helper.ConnectConsumer();
    helper.WaitForConsumerConnect();

    TraceConfig trace_config;
    trace_config.add_buffers()->set_size_kb(1024);
    trace_config.set_duration_ms(200);

    auto* ds_config = trace_config.add_data_sources()->mutable_config();
    ds_config->set_name(producer_name);
    ds_config->set_target_buffer(0);

    static constexpr uint32_t kRandomSeed = 42;
    static constexpr uint32_t kEventCount = 10;
    static constexpr uint32_t kMessageSizeBytes = 1024;
    ds_config->mutable_for_testing()->set_seed(kRandomSeed);
    ds_config->mutable_for_testing()->set_message_count(kEventCount);
    ds_config->mutable_for_testing()->set_message_size(kMessageSizeBytes);
    ds_config->mutable_for_testing()->set_send_batch_on_register(true);

    helper.StartTracing(trace_config);
    helper.WaitForTracingDisabled();

    helper.ReadData();
    helper.WaitForReadData();

    const auto& packets = helper.trace();
    ASSERT_EQ(packets.size(), kEventCount);

    std::minstd_rand0 rnd_engine(kRandomSeed);
    for (const auto& packet : packets) {
      ASSERT_TRUE(packet.has_for_testing());
      ASSERT_EQ(packet.for_testing().seq_value(), rnd_engine());
    }
  }
};

TEST_F(PerfettoCtsTest, TestProducerActivity) {
  TestMockProducer("android.perfetto.cts.ProducerActivity");
}

TEST_F(PerfettoCtsTest, TestProducerService) {
  TestMockProducer("android.perfetto.cts.ProducerService");
}

TEST_F(PerfettoCtsTest, TestProducerIsolatedService) {
  TestMockProducer("android.perfetto.cts.ProducerIsolatedService");
}

}  // namespace perfetto
