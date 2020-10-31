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

#include "src/trace_processor/importers/proto/system_probes_module.h"
#include "perfetto/base/build_config.h"
#include "src/trace_processor/importers/proto/system_probes_parser.h"
#include "src/trace_processor/timestamped_trace_piece.h"

#include "protos/perfetto/trace/trace_packet.pbzero.h"

namespace perfetto {
namespace trace_processor {

using perfetto::protos::pbzero::TracePacket;

SystemProbesModule::SystemProbesModule(TraceProcessorContext* context)
    : parser_(context) {
  RegisterForField(TracePacket::kProcessTreeFieldNumber, context);
  RegisterForField(TracePacket::kProcessStatsFieldNumber, context);
  RegisterForField(TracePacket::kSysStatsFieldNumber, context);
  RegisterForField(TracePacket::kSystemInfoFieldNumber, context);
  RegisterForField(TracePacket::kCpuInfoFieldNumber, context);
}

ModuleResult SystemProbesModule::TokenizePacket(
    const protos::pbzero::TracePacket::Decoder& decoder,
    TraceBlobView*,
    int64_t,
    PacketSequenceState*,
    uint32_t field_id) {
  switch (field_id) {
    case TracePacket::kSystemInfoFieldNumber:
      parser_.ParseSystemInfo(decoder.system_info());
      return ModuleResult::Handled();
  }
  return ModuleResult::Ignored();
}

void SystemProbesModule::ParsePacket(const TracePacket::Decoder& decoder,
                                     const TimestampedTracePiece& ttp,
                                     uint32_t field_id) {
  switch (field_id) {
    case TracePacket::kProcessTreeFieldNumber:
      parser_.ParseProcessTree(decoder.process_tree());
      return;
    case TracePacket::kProcessStatsFieldNumber:
      parser_.ParseProcessStats(ttp.timestamp, decoder.process_stats());
      return;
    case TracePacket::kSysStatsFieldNumber:
      parser_.ParseSysStats(ttp.timestamp, decoder.sys_stats());
      return;
    case TracePacket::kCpuInfoFieldNumber:
      parser_.ParseCpuInfo(decoder.cpu_info());
      return;
  }
}

}  // namespace trace_processor
}  // namespace perfetto
