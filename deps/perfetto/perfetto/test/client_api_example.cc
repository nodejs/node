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


#include <chrono>
#include <thread>

// This source file is built in two ways:
// 1. As part of the regular GN build, against standard includes.
// 2. To test that the amalgmated SDK works, against the perfetto.h source.

#ifdef PERFETTO_AMALGAMATED_SDK_TEST
#include "perfetto.h"
#else
#include "perfetto/tracing.h"
#include "protos/perfetto/config/gpu/gpu_counter_config.pbzero.h"
#include "protos/perfetto/trace/gpu/gpu_counter_event.pbzero.h"
#include "protos/perfetto/trace/test_event.pbzero.h"
#include "protos/perfetto/trace/trace_packet.pbzero.h"
#endif

// Deliberately not pulling any non-public perfetto header to spot accidental
// header public -> non-public dependency while building this file.
namespace {
class MyDataSource : public perfetto::DataSource<MyDataSource> {
 public:
  void OnSetup(const SetupArgs& args) override {
    // This can be used to access the domain-specific DataSourceConfig, via
    // args.config->xxx_config_raw().
    const std::string& config_raw = args.config->gpu_counter_config_raw();
    perfetto::protos::pbzero::GpuCounterConfig::Decoder config(config_raw);

    int sample_period = 0;
    if (config.has_counter_period_ns())
      sample_period = static_cast<int>(config.counter_period_ns());

    std::vector<uint32_t> counter_ids;
    for (auto it = config.counter_ids(); it; ++it) {
      uint32_t counter_id = it->as_uint32();
      if (counter_id > 0) {
        counter_ids.push_back(counter_id);
      }
    }

    PERFETTO_ILOG(
        "OnSetup called, name: %s,"
        "counter_period_ms: %d, tracing_session_id: %" PRIu64
        " num counters: %zu",
        args.config->name().c_str(), int(sample_period / 1000000),
        args.config->tracing_session_id(), counter_ids.size());
  }

  void OnStart(const StartArgs&) override { PERFETTO_ILOG("OnStart called"); }

  void OnStop(const StopArgs& stop_args) override {
    PERFETTO_ILOG("OnStop called");

    auto stop_closure = stop_args.HandleStopAsynchronously();

    // It is possible to trace on stop as well, but doing so requires manually
    // calling Flush() at the end.
    MyDataSource::Trace([](MyDataSource::TraceContext ctx) {
      PERFETTO_LOG("Tracing lambda called while stopping");
      // This block here is to auto-finalize the packet before calling Flush.
      {
        auto packet = ctx.NewTracePacket();
        packet->set_timestamp(999);
      }
      ctx.Flush();
    });

    stop_closure();
  }
};

}  // namespace

PERFETTO_DEFINE_DATA_SOURCE_STATIC_MEMBERS(MyDataSource);

int main() {
  perfetto::TracingInitArgs args;
  args.backends = perfetto::kSystemBackend;
  perfetto::Tracing::Initialize(args);

  // DataSourceDescriptor can be used to advertise domain-specific features.
  perfetto::DataSourceDescriptor dsd;
  dsd.set_name("com.example.mytrace");
  MyDataSource::Register(dsd);

  for (;;) {
    MyDataSource::Trace([](MyDataSource::TraceContext ctx) {
      PERFETTO_LOG("Tracing lambda called");
      auto packet = ctx.NewTracePacket();
      packet->set_timestamp(42);
      auto* gpu_packet = packet->set_gpu_counter_event();
      auto* cnt = gpu_packet->add_counters();
      cnt->set_counter_id(1);
    });
    std::this_thread::sleep_for(std::chrono::seconds(1));
  }
}
