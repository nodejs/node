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

#include <stddef.h>
#include <stdint.h>

#include <algorithm>

#include "perfetto/base/logging.h"
#include "perfetto/ext/base/utils.h"
#include "perfetto/protozero/scattered_stream_null_delegate.h"
#include "perfetto/protozero/scattered_stream_writer.h"
#include "protos/perfetto/trace/ftrace/ftrace_event_bundle.pbzero.h"
#include "src/traced/probes/ftrace/cpu_reader.h"
#include "src/traced/probes/ftrace/ftrace_config_muxer.h"
#include "src/traced/probes/ftrace/test/cpu_reader_support.h"
#include "src/tracing/core/null_trace_writer.h"

namespace perfetto {
namespace {

uint8_t g_page[base::kPageSize];

}  // namespace

using perfetto::protos::pbzero::FtraceEventBundle;

void FuzzCpuReaderParsePage(const uint8_t* data, size_t size);
void FuzzCpuReaderProcessPagesForDataSource(const uint8_t* data, size_t size);

// TODO(rsavitski): make the fuzzer generate multi-page payloads.
void FuzzCpuReaderProcessPagesForDataSource(const uint8_t* data, size_t size) {
  ProtoTranslationTable* table = GetTable("synthetic");
  if (!table) {
    PERFETTO_FATAL(
        "Could not read table. "
        "This fuzzer must be run in the root directory.");
  }
  memset(g_page, 0, base::kPageSize);
  memcpy(g_page, data, std::min(base::kPageSize, size));

  FtraceMetadata metadata{};
  FtraceDataSourceConfig ds_config{
      EventFilter{}, DisabledCompactSchedConfigForTesting(), {}, {}};
  ds_config.event_filter.AddEnabledEvent(
      table->EventToFtraceId(GroupAndName("sched", "sched_switch")));
  ds_config.event_filter.AddEnabledEvent(
      table->EventToFtraceId(GroupAndName("ftrace", "print")));

  NullTraceWriter null_writer;
  CpuReader::ProcessPagesForDataSource(&null_writer, &metadata, /*cpu=*/0,
                                       &ds_config, g_page, /*pages_read=*/1,
                                       table);
}

}  // namespace perfetto

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size);

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  perfetto::FuzzCpuReaderProcessPagesForDataSource(data, size);
  return 0;
}
