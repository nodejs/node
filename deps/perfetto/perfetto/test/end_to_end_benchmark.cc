// Copyright (C) 2018 The Android Open Source Project
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <random>

#include <benchmark/benchmark.h>

#include "perfetto/base/time.h"
#include "perfetto/ext/traced/traced.h"
#include "perfetto/ext/tracing/core/trace_packet.h"
#include "perfetto/tracing/core/trace_config.h"
#include "src/base/test/test_task_runner.h"
#include "test/gtest_and_gmock.h"
#include "test/test_helper.h"

#include "protos/perfetto/config/test_config.gen.h"
#include "protos/perfetto/trace/test_event.gen.h"
#include "protos/perfetto/trace/trace_packet.pbzero.h"

namespace perfetto {

namespace {

bool IsBenchmarkFunctionalOnly() {
  return getenv("BENCHMARK_FUNCTIONAL_TEST_ONLY") != nullptr;
}

void BenchmarkProducer(benchmark::State& state) {
  base::TestTaskRunner task_runner;

  TestHelper helper(&task_runner);
  helper.StartServiceIfRequired();

  FakeProducer* producer = helper.ConnectFakeProducer();
  helper.ConnectConsumer();
  helper.WaitForConsumerConnect();

  TraceConfig trace_config;
  trace_config.add_buffers()->set_size_kb(512);

  auto* ds_config = trace_config.add_data_sources()->mutable_config();
  ds_config->set_name("android.perfetto.FakeProducer");
  ds_config->set_target_buffer(0);

  static constexpr uint32_t kRandomSeed = 42;
  uint32_t message_count = static_cast<uint32_t>(state.range(0));
  uint32_t message_bytes = static_cast<uint32_t>(state.range(1));
  uint32_t mb_per_s = static_cast<uint32_t>(state.range(2));

  uint32_t messages_per_s = mb_per_s * 1024 * 1024 / message_bytes;
  uint32_t time_for_messages_ms =
      10000 + (messages_per_s == 0 ? 0 : message_count * 1000 / messages_per_s);

  ds_config->mutable_for_testing()->set_seed(kRandomSeed);
  ds_config->mutable_for_testing()->set_message_count(message_count);
  ds_config->mutable_for_testing()->set_message_size(message_bytes);
  ds_config->mutable_for_testing()->set_max_messages_per_second(messages_per_s);

  helper.StartTracing(trace_config);
  helper.WaitForProducerEnabled();

  uint64_t wall_start_ns = static_cast<uint64_t>(base::GetWallTimeNs().count());
  uint64_t service_start_ns =
      helper.service_thread()->GetThreadCPUTimeNsForTesting();
  uint64_t producer_start_ns =
      helper.producer_thread()->GetThreadCPUTimeNsForTesting();
  uint32_t iterations = 0;
  for (auto _ : state) {
    auto cname = "produced.and.committed." + std::to_string(iterations++);
    auto on_produced_and_committed = task_runner.CreateCheckpoint(cname);
    producer->ProduceEventBatch(helper.WrapTask(on_produced_and_committed));
    task_runner.RunUntilCheckpoint(cname, time_for_messages_ms);
  }
  uint64_t service_ns =
      helper.service_thread()->GetThreadCPUTimeNsForTesting() -
      service_start_ns;
  uint64_t producer_ns =
      helper.producer_thread()->GetThreadCPUTimeNsForTesting() -
      producer_start_ns;
  uint64_t wall_ns =
      static_cast<uint64_t>(base::GetWallTimeNs().count()) - wall_start_ns;

  state.counters["Ser CPU"] = benchmark::Counter(100.0 * service_ns / wall_ns);
  state.counters["Ser ns/m"] =
      benchmark::Counter(1.0 * service_ns / message_count);
  state.counters["Pro CPU"] = benchmark::Counter(100.0 * producer_ns / wall_ns);
  state.SetBytesProcessed(iterations * message_bytes * message_count);

  // Read back the buffer just to check correctness.
  helper.ReadData();
  helper.WaitForReadData();

  bool is_first_packet = true;
  std::minstd_rand0 rnd_engine(kRandomSeed);
  for (const auto& packet : helper.trace()) {
    ASSERT_TRUE(packet.has_for_testing());
    if (is_first_packet) {
      rnd_engine = std::minstd_rand0(packet.for_testing().seq_value());
      is_first_packet = false;
    } else {
      ASSERT_EQ(packet.for_testing().seq_value(), rnd_engine());
    }
  }
}

static void BenchmarkConsumer(benchmark::State& state) {
  base::TestTaskRunner task_runner;

  TestHelper helper(&task_runner);
  helper.StartServiceIfRequired();

  FakeProducer* producer = helper.ConnectFakeProducer();
  helper.ConnectConsumer();
  helper.WaitForConsumerConnect();

  TraceConfig trace_config;

  static const uint32_t kBufferSizeBytes =
      IsBenchmarkFunctionalOnly() ? 16 * 1024 : 2 * 1024 * 1024;
  trace_config.add_buffers()->set_size_kb(kBufferSizeBytes / 1024);

  static constexpr uint32_t kRandomSeed = 42;
  uint32_t message_bytes = static_cast<uint32_t>(state.range(0));
  uint32_t mb_per_s = static_cast<uint32_t>(state.range(1));
  bool is_saturated_producer = mb_per_s == 0;

  uint32_t message_count = kBufferSizeBytes / message_bytes;
  uint32_t messages_per_s = mb_per_s * 1024 * 1024 / message_bytes;
  uint32_t number_of_batches =
      is_saturated_producer ? 0 : std::max(1u, message_count / messages_per_s);

  auto* ds_config = trace_config.add_data_sources()->mutable_config();
  ds_config->set_name("android.perfetto.FakeProducer");
  ds_config->set_target_buffer(0);
  ds_config->mutable_for_testing()->set_seed(kRandomSeed);
  ds_config->mutable_for_testing()->set_message_count(message_count);
  ds_config->mutable_for_testing()->set_message_size(message_bytes);
  ds_config->mutable_for_testing()->set_max_messages_per_second(messages_per_s);

  helper.StartTracing(trace_config);
  helper.WaitForProducerEnabled();

  uint64_t wall_start_ns = static_cast<uint64_t>(base::GetWallTimeNs().count());
  uint64_t service_start_ns = static_cast<uint64_t>(
      helper.service_thread()->GetThreadCPUTimeNsForTesting());
  uint64_t consumer_start_ns =
      static_cast<uint64_t>(base::GetThreadCPUTimeNs().count());
  uint64_t read_time_taken_ns = 0;

  uint64_t iterations = 0;
  uint32_t counter = 0;
  for (auto _ : state) {
    auto cname = "produced.and.committed." + std::to_string(iterations++);
    auto on_produced_and_committed = task_runner.CreateCheckpoint(cname);
    producer->ProduceEventBatch(helper.WrapTask(on_produced_and_committed));

    if (is_saturated_producer) {
      // If the producer is running in saturated mode, wait until it flushes
      // data.
      task_runner.RunUntilCheckpoint(cname);

      // Then time how long it takes to read back the data.
      int64_t start = base::GetWallTimeNs().count();
      helper.ReadData(counter);
      helper.WaitForReadData(counter++);
      read_time_taken_ns +=
          static_cast<uint64_t>(base::GetWallTimeNs().count() - start);
    } else {
      // If the producer is not running in saturated mode, every second the
      // producer will send a batch of data over. Wait for a second before
      // performing readback; do this for each batch the producer sends.
      for (uint32_t i = 0; i < number_of_batches; i++) {
        auto batch_cname = "batch.checkpoint." + std::to_string(counter);
        auto batch_checkpoint = task_runner.CreateCheckpoint(batch_cname);
        task_runner.PostDelayedTask(batch_checkpoint, 1000);
        task_runner.RunUntilCheckpoint(batch_cname);

        int64_t start = base::GetWallTimeNs().count();
        helper.ReadData(counter);
        helper.WaitForReadData(counter++);
        read_time_taken_ns +=
            static_cast<uint64_t>(base::GetWallTimeNs().count() - start);
      }
    }
  }
  uint64_t service_ns =
      helper.service_thread()->GetThreadCPUTimeNsForTesting() -
      service_start_ns;
  uint64_t consumer_ns =
      static_cast<uint64_t>(base::GetThreadCPUTimeNs().count()) -
      consumer_start_ns;
  uint64_t wall_ns =
      static_cast<uint64_t>(base::GetWallTimeNs().count()) - wall_start_ns;

  state.counters["Ser CPU"] = benchmark::Counter(100.0 * service_ns / wall_ns);
  state.counters["Ser ns/m"] =
      benchmark::Counter(1.0 * service_ns / message_count);
  state.counters["Con CPU"] = benchmark::Counter(100.0 * consumer_ns / wall_ns);
  state.counters["Con Speed"] =
      benchmark::Counter(iterations * 1000.0 * 1000 * 1000 * kBufferSizeBytes /
                         read_time_taken_ns);
}

void SaturateCpuProducerArgs(benchmark::internal::Benchmark* b) {
  int min_message_count = 16;
  int max_message_count = IsBenchmarkFunctionalOnly() ? 16 : 1024 * 1024;
  int min_payload = 8;
  int max_payload = IsBenchmarkFunctionalOnly() ? 8 : 2048;
  for (int count = min_message_count; count <= max_message_count; count *= 2) {
    for (int bytes = min_payload; bytes <= max_payload; bytes *= 2) {
      b->Args({count, bytes, 0 /* speed */});
    }
  }
}

void ConstantRateProducerArgs(benchmark::internal::Benchmark* b) {
  int message_count = IsBenchmarkFunctionalOnly() ? 2 * 1024 : 128 * 1024;
  int min_speed = IsBenchmarkFunctionalOnly() ? 64 : 8;
  int max_speed = 128;
  for (int speed = min_speed; speed <= max_speed; speed *= 2) {
    b->Args({message_count, 128, speed});
    b->Args({message_count, 256, speed});
  }
}

void SaturateCpuConsumerArgs(benchmark::internal::Benchmark* b) {
  int min_payload = 8;
  int max_payload = IsBenchmarkFunctionalOnly() ? 16 : 64 * 1024;
  for (int bytes = min_payload; bytes <= max_payload; bytes *= 2) {
    b->Args({bytes, 0 /* speed */});
  }
}

void ConstantRateConsumerArgs(benchmark::internal::Benchmark* b) {
  int min_speed = IsBenchmarkFunctionalOnly() ? 128 : 1;
  int max_speed = IsBenchmarkFunctionalOnly() ? 128 : 2;
  for (int speed = min_speed; speed <= max_speed; speed *= 2) {
    b->Args({2, speed});
    b->Args({4, speed});
  }
}

}  // namespace

static void BM_EndToEnd_Producer_SaturateCpu(benchmark::State& state) {
  BenchmarkProducer(state);
}

BENCHMARK(BM_EndToEnd_Producer_SaturateCpu)
    ->Unit(benchmark::kMicrosecond)
    ->UseRealTime()
    ->Apply(SaturateCpuProducerArgs);

static void BM_EndToEnd_Producer_ConstantRate(benchmark::State& state) {
  BenchmarkProducer(state);
}

BENCHMARK(BM_EndToEnd_Producer_ConstantRate)
    ->Unit(benchmark::kMicrosecond)
    ->UseRealTime()
    ->Apply(ConstantRateProducerArgs);

static void BM_EndToEnd_Consumer_SaturateCpu(benchmark::State& state) {
  BenchmarkConsumer(state);
}

BENCHMARK(BM_EndToEnd_Consumer_SaturateCpu)
    ->Unit(benchmark::kMicrosecond)
    ->UseRealTime()
    ->Apply(SaturateCpuConsumerArgs);

static void BM_EndToEnd_Consumer_ConstantRate(benchmark::State& state) {
  BenchmarkConsumer(state);
}

BENCHMARK(BM_EndToEnd_Consumer_ConstantRate)
    ->Unit(benchmark::kMillisecond)
    ->UseRealTime()
    ->Apply(ConstantRateConsumerArgs);

}  // namespace perfetto
