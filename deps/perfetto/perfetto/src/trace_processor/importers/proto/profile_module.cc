/*
 * Copyright (C) 2020 The Android Open Source Project
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

#include "src/trace_processor/importers/proto/profile_module.h"

#include "perfetto/base/logging.h"
#include "src/trace_processor/importers/common/clock_tracker.h"
#include "src/trace_processor/importers/common/process_tracker.h"
#include "src/trace_processor/importers/proto/packet_sequence_state.h"
#include "src/trace_processor/importers/proto/profile_packet_utils.h"
#include "src/trace_processor/importers/proto/stack_profile_tracker.h"
#include "src/trace_processor/storage/trace_storage.h"
#include "src/trace_processor/tables/profiler_tables.h"
#include "src/trace_processor/timestamped_trace_piece.h"
#include "src/trace_processor/trace_sorter.h"
#include "src/trace_processor/types/trace_processor_context.h"

#include "protos/perfetto/trace/clock_snapshot.pbzero.h"
#include "protos/perfetto/trace/profiling/profile_packet.pbzero.h"

namespace perfetto {
namespace trace_processor {

using perfetto::protos::pbzero::TracePacket;
using protozero::ConstBytes;

ProfileModule::ProfileModule(TraceProcessorContext* context)
    : context_(context) {
  RegisterForField(TracePacket::kStreamingProfilePacketFieldNumber, context);
}

ProfileModule::~ProfileModule() = default;

ModuleResult ProfileModule::TokenizePacket(const TracePacket::Decoder& decoder,
                                           TraceBlobView* packet,
                                           int64_t /*packet_timestamp*/,
                                           PacketSequenceState* state,
                                           uint32_t field_id) {
  switch (field_id) {
    case TracePacket::kStreamingProfilePacketFieldNumber:
      return TokenizeStreamingProfilePacket(state, packet,
                                            decoder.streaming_profile_packet());
  }
  return ModuleResult::Ignored();
}

void ProfileModule::ParsePacket(const TracePacket::Decoder& decoder,
                                const TimestampedTracePiece& ttp,
                                uint32_t field_id) {
  switch (field_id) {
    case TracePacket::kStreamingProfilePacketFieldNumber:
      PERFETTO_DCHECK(ttp.type == TimestampedTracePiece::Type::kTracePacket);
      ParseStreamingProfilePacket(ttp.timestamp, ttp.packet_data.sequence_state,
                                  decoder.streaming_profile_packet());
      return;
  }
}

ModuleResult ProfileModule::TokenizeStreamingProfilePacket(
    PacketSequenceState* sequence_state,
    TraceBlobView* packet,
    ConstBytes streaming_profile_packet) {
  protos::pbzero::StreamingProfilePacket::Decoder decoder(
      streaming_profile_packet.data, streaming_profile_packet.size);

  // We have to resolve the reference timestamp of a StreamingProfilePacket
  // during tokenization. If we did this during parsing instead, the
  // tokenization of a subsequent ThreadDescriptor with a new reference
  // timestamp would cause us to later calculate timestamps based on the wrong
  // reference value during parsing. Since StreamingProfilePackets only need to
  // be sorted correctly with respect to process/thread metadata events (so that
  // pid/tid are resolved correctly during parsing), we forward the packet as a
  // whole through the sorter, using the "root" timestamp of the packet, i.e.
  // the current timestamp of the packet sequence.
  auto packet_ts =
      sequence_state->IncrementAndGetTrackEventTimeNs(/*delta_ns=*/0);
  auto trace_ts = context_->clock_tracker->ToTraceTime(
      protos::pbzero::ClockSnapshot::Clock::MONOTONIC, packet_ts);
  if (trace_ts)
    packet_ts = *trace_ts;

  // Increment the sequence's timestamp by all deltas.
  for (auto timestamp_it = decoder.timestamp_delta_us(); timestamp_it;
       ++timestamp_it) {
    sequence_state->IncrementAndGetTrackEventTimeNs(*timestamp_it * 1000);
  }

  context_->sorter->PushTracePacket(packet_ts, sequence_state,
                                    std::move(*packet));
  return ModuleResult::Handled();
}

void ProfileModule::ParseStreamingProfilePacket(
    int64_t timestamp,
    PacketSequenceStateGeneration* sequence_state,
    ConstBytes streaming_profile_packet) {
  protos::pbzero::StreamingProfilePacket::Decoder packet(
      streaming_profile_packet.data, streaming_profile_packet.size);

  ProcessTracker* procs = context_->process_tracker.get();
  TraceStorage* storage = context_->storage.get();
  StackProfileTracker& stack_profile_tracker =
      sequence_state->state()->stack_profile_tracker();
  ProfilePacketInternLookup intern_lookup(sequence_state);

  uint32_t pid = static_cast<uint32_t>(sequence_state->state()->pid());
  uint32_t tid = static_cast<uint32_t>(sequence_state->state()->tid());
  UniqueTid utid = procs->UpdateThread(tid, pid);

  // Iterate through timestamps and callstacks simultaneously.
  auto timestamp_it = packet.timestamp_delta_us();
  for (auto callstack_it = packet.callstack_iid(); callstack_it;
       ++callstack_it, ++timestamp_it) {
    if (!timestamp_it) {
      context_->storage->IncrementStats(stats::stackprofile_parser_error);
      PERFETTO_ELOG(
          "StreamingProfilePacket has less callstack IDs than timestamps!");
      break;
    }

    auto opt_cs_id = stack_profile_tracker.FindOrInsertCallstack(
        *callstack_it, &intern_lookup);
    if (!opt_cs_id) {
      context_->storage->IncrementStats(stats::stackprofile_parser_error);
      PERFETTO_ELOG("StreamingProfilePacket referencing invalid callstack!");
      continue;
    }

    // Resolve the delta timestamps based on the packet's root timestamp.
    timestamp += *timestamp_it * 1000;

    tables::CpuProfileStackSampleTable::Row sample_row{
        timestamp, *opt_cs_id, utid, packet.process_priority()};
    storage->mutable_cpu_profile_stack_sample_table()->Insert(sample_row);
  }
}

}  // namespace trace_processor
}  // namespace perfetto
