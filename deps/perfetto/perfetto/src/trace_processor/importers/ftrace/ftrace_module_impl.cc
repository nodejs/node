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

#include "src/trace_processor/importers/ftrace/ftrace_module_impl.h"
#include "perfetto/base/build_config.h"
#include "src/trace_processor/importers/ftrace/ftrace_parser.h"
#include "src/trace_processor/importers/ftrace/ftrace_tokenizer.h"
#include "src/trace_processor/timestamped_trace_piece.h"
#include "src/trace_processor/trace_blob_view.h"

#include "protos/perfetto/trace/trace_packet.pbzero.h"

namespace perfetto {
namespace trace_processor {

using perfetto::protos::pbzero::TracePacket;

FtraceModuleImpl::FtraceModuleImpl(TraceProcessorContext* context)
    : tokenizer_(context), parser_(context) {
  RegisterForField(TracePacket::kFtraceEventsFieldNumber, context);
  RegisterForField(TracePacket::kFtraceStatsFieldNumber, context);
}

ModuleResult FtraceModuleImpl::TokenizePacket(
    const protos::pbzero::TracePacket::Decoder& decoder,
    TraceBlobView* packet,
    int64_t /*packet_timestamp*/,
    PacketSequenceState* /*state*/,
    uint32_t field_id) {
  if (field_id == TracePacket::kFtraceEventsFieldNumber) {
    auto ftrace_field = decoder.ftrace_events();
    const size_t fld_off = packet->offset_of(ftrace_field.data);
    tokenizer_.TokenizeFtraceBundle(packet->slice(fld_off, ftrace_field.size));
    return ModuleResult::Handled();
  }
  return ModuleResult::Ignored();
}

void FtraceModuleImpl::ParsePacket(
    const protos::pbzero::TracePacket::Decoder& decoder,
    const TimestampedTracePiece&,
    uint32_t field_id) {
  if (field_id == TracePacket::kFtraceStatsFieldNumber) {
    parser_.ParseFtraceStats(decoder.ftrace_stats());
  }
}

void FtraceModuleImpl::ParseFtracePacket(uint32_t cpu,
                                         const TimestampedTracePiece& ttp) {
  util::Status res = parser_.ParseFtraceEvent(cpu, ttp);
  if (!res.ok()) {
    PERFETTO_ELOG("%s", res.message().c_str());
  }
}

}  // namespace trace_processor
}  // namespace perfetto
