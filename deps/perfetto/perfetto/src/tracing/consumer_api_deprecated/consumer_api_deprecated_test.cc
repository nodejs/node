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

#include <inttypes.h>

#include <array>
#include <atomic>
#include <chrono>
#include <thread>

#include "perfetto/base/logging.h"
#include "perfetto/public/consumer_api.h"

#include "protos/perfetto/config/data_source_config.gen.h"
#include "protos/perfetto/config/ftrace/ftrace_config.gen.h"
#include "protos/perfetto/config/trace_config.gen.h"
#include "protos/perfetto/trace/ftrace/ftrace_event.gen.h"
#include "protos/perfetto/trace/ftrace/ftrace_event_bundle.gen.h"
#include "protos/perfetto/trace/trace.gen.h"
#include "protos/perfetto/trace/trace_packet.gen.h"

using namespace perfetto::consumer;

namespace {

int g_pointer = 0;

std::string GetConfig(uint32_t duration_ms) {
  perfetto::protos::gen::TraceConfig trace_config;
  trace_config.set_duration_ms(duration_ms);
  trace_config.add_buffers()->set_size_kb(4096);
  trace_config.set_deferred_start(true);
  auto* ds_config = trace_config.add_data_sources()->mutable_config();
  ds_config->set_name("linux.ftrace");

  perfetto::protos::gen::FtraceConfig ftrace_config;
  ftrace_config.add_ftrace_events("sched_switch");
  ftrace_config.add_ftrace_events("mm_filemap_add_to_page_cache");
  ftrace_config.add_ftrace_events("mm_filemap_delete_from_page_cache");
  ds_config->set_ftrace_config_raw(ftrace_config.SerializeAsString());
  ds_config->set_target_buffer(0);
  return trace_config.SerializeAsString();
}

void DumpTrace(TraceBuffer buf) {
  perfetto::protos::gen::Trace trace;
  bool parsed = trace.ParseFromArray(buf.begin, buf.size);
  if (!parsed) {
    PERFETTO_ELOG("Failed to parse the trace");
    return;
  }

  PERFETTO_LOG("Parsing %d trace packets", trace.packet_size());
  int num_filemap_events = 0;
  for (const auto& packet : trace.packet()) {
    if (packet.has_ftrace_events()) {
      const auto& bundle = packet.ftrace_events();
      for (const auto& ftrace : bundle.event()) {
        if (ftrace.has_mm_filemap_add_to_page_cache()) {
          num_filemap_events++;
          // const auto& evt = ftrace.mm_filemap_add_to_page_cache();
          // PERFETTO_LOG(
          //     "mm_filemap_add_to_page_cache pfn=%llu, dev=%llu, ino=%llu",
          //     evt.pfn(), evt.s_dev(), evt.i_ino());
        }
        if (ftrace.has_mm_filemap_delete_from_page_cache()) {
          num_filemap_events++;
          // const auto& evt = ftrace.mm_filemap_delete_from_page_cache();
          // PERFETTO_LOG(
          //     "mm_filemap_delete_from_page_cache pfn=%llu, dev=%llu,
          //     ino=%llu", evt.pfn(), evt.s_dev(), evt.i_ino());
        }
      }
    }
  }
  PERFETTO_LOG("Got %d mm_filemap events", num_filemap_events);
}

void OnStateChanged(Handle handle, State state, void* ptr) {
  PERFETTO_LOG("Callback: handle=%" PRId64 " state=%d", handle,
               static_cast<int>(state));
  PERFETTO_CHECK(ptr == &g_pointer);
}

void TestSingle() {
  std::string cfg = GetConfig(1000);
  auto handle = Create(cfg.data(), cfg.size(), &OnStateChanged, &g_pointer);
  PERFETTO_ILOG("Starting, handle=%" PRId64 " state=%d", handle,
                static_cast<int>(PollState(handle)));
  std::this_thread::sleep_for(std::chrono::milliseconds(100));
  StartTracing(handle);
  // Wait for either completion or error.
  while (static_cast<int>(PollState(handle)) > 0 &&
         PollState(handle) != State::kTraceEnded) {
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }

  if (PollState(handle) == State::kTraceEnded) {
    auto buf = ReadTrace(handle);
    DumpTrace(buf);
  } else {
    PERFETTO_ELOG("Trace failed");
  }

  PERFETTO_ILOG("Destroying");
  Destroy(handle);
}

void TestMany() {
  std::string cfg = GetConfig(8000);

  std::array<Handle, 5> handles{};
  for (size_t i = 0; i < handles.size(); i++) {
    auto handle = Create(cfg.data(), cfg.size(), &OnStateChanged, &g_pointer);
    handles[i] = handle;
    PERFETTO_ILOG("Creating handle=%" PRId64 " state=%d", handle,
                  static_cast<int>(PollState(handle)));
  }

  // Wait that all sessions are connected.
  for (bool all_connected = false; !all_connected;) {
    all_connected = true;
    for (size_t i = 0; i < handles.size(); i++) {
      if (PollState(handles[i]) != State::kConfigured) {
        all_connected = false;
      }
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }

  // Start only 3 out of 5 sessions, scattering them with 1 second delay.
  for (size_t i = 0; i < handles.size(); i++) {
    if (i % 2 == 0) {
      StartTracing(handles[i]);
      std::this_thread::sleep_for(std::chrono::seconds(1));
    }
  }

  // Wait until all sessions are complete.
  for (int num_complete = 0; num_complete != 3;) {
    num_complete = 0;
    for (size_t i = 0; i < handles.size(); i++) {
      if (PollState(handles[i]) == State::kTraceEnded) {
        num_complete++;
      }
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }

  // Read the trace buffers.
  for (size_t i = 0; i < handles.size(); i++) {
    auto buf = ReadTrace(handles[i]);
    PERFETTO_ILOG("ReadTrace[%zu] buf=%p %zu", i, static_cast<void*>(buf.begin),
                  buf.size);
    if (i % 2 == 0) {
      if (!buf.begin) {
        PERFETTO_ELOG("FAIL: the buffer was supposed to be not empty");
      } else {
        DumpTrace(buf);
      }
    }
  }

  PERFETTO_ILOG("Destroying");
  for (size_t i = 0; i < handles.size(); i++)
    Destroy(handles[i]);
}
}  // namespace

int main() {
  PERFETTO_LOG("Testing single trace");
  PERFETTO_LOG("=============================================================");
  TestSingle();
  PERFETTO_LOG("=============================================================");

  PERFETTO_LOG("\n");

  PERFETTO_LOG("\n");
  PERFETTO_LOG("Testing concurrent traces");
  PERFETTO_LOG("=============================================================");
  TestMany();
  PERFETTO_LOG("=============================================================");

  return 0;
}
