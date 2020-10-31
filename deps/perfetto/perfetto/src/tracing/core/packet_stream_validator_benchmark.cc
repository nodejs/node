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

#include <benchmark/benchmark.h>

#include "src/tracing/core/packet_stream_validator.h"

#include "perfetto/ext/tracing/core/slice.h"
#include "perfetto/protozero/scattered_heap_buffer.h"

#include "protos/perfetto/trace/ftrace/ftrace_event.pbzero.h"
#include "protos/perfetto/trace/ftrace/ftrace_event_bundle.pbzero.h"
#include "protos/perfetto/trace/ftrace/sched.pbzero.h"
#include "protos/perfetto/trace/test_event.pbzero.h"
#include "protos/perfetto/trace/trace_packet.pbzero.h"

namespace {

static void BM_PacketStreamValidator(benchmark::State& state) {
  using namespace perfetto;

  // Create a packet that resembles a ftrace sched bundle. A ftrace page is
  // 4KB and typically contains ~64 sched events of 64 bytes each.
  protozero::HeapBuffered<protos::pbzero::TracePacket> packet;
  auto* bundle = packet->set_ftrace_events();
  bundle->set_cpu(1);
  for (size_t events = 0; events < 64; events++) {
    auto* ftrace_evt = bundle->add_event();
    ftrace_evt->set_pid(12345);
    ftrace_evt->set_timestamp(1000ull * 1000 * 1000 * 3600 * 24 * 365);
    auto* sched_switch = ftrace_evt->set_sched_switch();
    sched_switch->set_prev_comm("thread_name_1");
    sched_switch->set_prev_pid(12345);
    sched_switch->set_prev_state(42);
    sched_switch->set_next_comm("thread_name_2");
    sched_switch->set_next_pid(67890);
  }
  std::vector<uint8_t> buf = packet.SerializeAsArray();

  // Append 10 packets like the one above, splitting each packet into slices
  // of 512B each.
  Slices slices;
  static constexpr size_t kSliceSize = 512;
  for (size_t num_packets = 0; num_packets < 10; num_packets++) {
    for (size_t pos = 0; pos < buf.size(); pos += kSliceSize) {
      size_t slice_size = std::min(kSliceSize, buf.size() - pos);
      Slice slice = Slice::Allocate(slice_size);
      memcpy(slice.own_data(), &buf[pos], slice_size);
      slices.emplace_back(std::move(slice));
    }
  }

  bool res = true;
  while (state.KeepRunning()) {
    res &= PacketStreamValidator::Validate(slices);
  }
  PERFETTO_CHECK(res);
}

}  // namespace

BENCHMARK(BM_PacketStreamValidator);
