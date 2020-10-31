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
#include "src/trace_processor/importers/proto/track_event_module.h"

#include "perfetto/base/build_config.h"
#include "perfetto/ext/base/string_utils.h"
#include "src/trace_processor/importers/common/track_tracker.h"
#include "src/trace_processor/timestamped_trace_piece.h"
#include "src/trace_processor/types/trace_processor_context.h"

#include "protos/perfetto/config/data_source_config.pbzero.h"
#include "protos/perfetto/config/trace_config.pbzero.h"
#include "protos/perfetto/trace/trace_packet.pbzero.h"

namespace perfetto {
namespace trace_processor {

using perfetto::protos::pbzero::TracePacket;

TrackEventModule::TrackEventModule(TraceProcessorContext* context)
    : tokenizer_(context), parser_(context) {
  RegisterForField(TracePacket::kTrackEventFieldNumber, context);
  RegisterForField(TracePacket::kTrackDescriptorFieldNumber, context);
  RegisterForField(TracePacket::kThreadDescriptorFieldNumber, context);
  RegisterForField(TracePacket::kProcessDescriptorFieldNumber, context);
}

TrackEventModule::~TrackEventModule() = default;

ModuleResult TrackEventModule::TokenizePacket(
    const TracePacket::Decoder& decoder,
    TraceBlobView* packet,
    int64_t packet_timestamp,
    PacketSequenceState* state,
    uint32_t field_id) {
  switch (field_id) {
    case TracePacket::kTrackDescriptorFieldNumber:
      return tokenizer_.TokenizeTrackDescriptorPacket(state, decoder,
                                                      packet_timestamp);
    case TracePacket::kTrackEventFieldNumber:
      tokenizer_.TokenizeTrackEventPacket(state, decoder, packet,
                                          packet_timestamp);
      return ModuleResult::Handled();
    case TracePacket::kThreadDescriptorFieldNumber:
      // TODO(eseckler): Remove once Chrome has switched to TrackDescriptors.
      return tokenizer_.TokenizeThreadDescriptorPacket(state, decoder);
  }
  return ModuleResult::Ignored();
}

void TrackEventModule::ParsePacket(const TracePacket::Decoder& decoder,
                                   const TimestampedTracePiece& ttp,
                                   uint32_t field_id) {
  switch (field_id) {
    case TracePacket::kTrackDescriptorFieldNumber:
      PERFETTO_DCHECK(ttp.type == TimestampedTracePiece::Type::kTracePacket);
      parser_.ParseTrackDescriptor(decoder.track_descriptor());
      break;
    case TracePacket::kTrackEventFieldNumber:
      PERFETTO_DCHECK(ttp.type == TimestampedTracePiece::Type::kTrackEvent);
      parser_.ParseTrackEvent(ttp.timestamp, ttp.track_event_data.get(),
                              decoder.track_event());
      break;
    case TracePacket::kProcessDescriptorFieldNumber:
      // TODO(eseckler): Remove once Chrome has switched to TrackDescriptors.
      PERFETTO_DCHECK(ttp.type == TimestampedTracePiece::Type::kTracePacket);
      parser_.ParseProcessDescriptor(decoder.process_descriptor());
      break;
    case TracePacket::kThreadDescriptorFieldNumber:
      // TODO(eseckler): Remove once Chrome has switched to TrackDescriptors.
      PERFETTO_DCHECK(ttp.type == TimestampedTracePiece::Type::kTracePacket);
      parser_.ParseThreadDescriptor(decoder.thread_descriptor());
      break;
  }
}

}  // namespace trace_processor
}  // namespace perfetto
