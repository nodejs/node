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

#include "src/trace_processor/importers/proto/track_event_tokenizer.h"

#include "perfetto/base/logging.h"
#include "src/trace_processor/importers/common/clock_tracker.h"
#include "src/trace_processor/importers/common/process_tracker.h"
#include "src/trace_processor/importers/common/track_tracker.h"
#include "src/trace_processor/importers/proto/packet_sequence_state.h"
#include "src/trace_processor/importers/proto/proto_trace_tokenizer.h"
#include "src/trace_processor/storage/stats.h"
#include "src/trace_processor/storage/trace_storage.h"
#include "src/trace_processor/trace_blob_view.h"
#include "src/trace_processor/trace_sorter.h"

#include "protos/perfetto/trace/clock_snapshot.pbzero.h"
#include "protos/perfetto/trace/trace_packet.pbzero.h"
#include "protos/perfetto/trace/track_event/chrome_process_descriptor.pbzero.h"
#include "protos/perfetto/trace/track_event/chrome_thread_descriptor.pbzero.h"
#include "protos/perfetto/trace/track_event/counter_descriptor.pbzero.h"
#include "protos/perfetto/trace/track_event/process_descriptor.pbzero.h"
#include "protos/perfetto/trace/track_event/thread_descriptor.pbzero.h"
#include "protos/perfetto/trace/track_event/track_descriptor.pbzero.h"
#include "protos/perfetto/trace/track_event/track_event.pbzero.h"

namespace perfetto {
namespace trace_processor {

namespace {
using protos::pbzero::CounterDescriptor;
}

TrackEventTokenizer::TrackEventTokenizer(TraceProcessorContext* context)
    : context_(context),
      counter_name_thread_time_id_(
          context_->storage->InternString("thread_time")),
      counter_name_thread_instruction_count_id_(
          context_->storage->InternString("thread_instruction_count")) {}

ModuleResult TrackEventTokenizer::TokenizeTrackDescriptorPacket(
    PacketSequenceState* state,
    const protos::pbzero::TracePacket::Decoder& packet,
    int64_t packet_timestamp) {
  auto track_descriptor_field = packet.track_descriptor();
  protos::pbzero::TrackDescriptor::Decoder track(track_descriptor_field.data,
                                                 track_descriptor_field.size);

  if (!track.has_uuid()) {
    PERFETTO_ELOG("TrackDescriptor packet without uuid");
    context_->storage->IncrementStats(stats::track_event_tokenizer_errors);
    return ModuleResult::Handled();
  }

  StringId name_id = kNullStringId;
  if (track.has_name())
    name_id = context_->storage->InternString(track.name());

  if (track.has_thread()) {
    protos::pbzero::ThreadDescriptor::Decoder thread(track.thread());

    if (!thread.has_pid() || !thread.has_tid()) {
      PERFETTO_ELOG(
          "No pid or tid in ThreadDescriptor for track with uuid %" PRIu64,
          track.uuid());
      context_->storage->IncrementStats(stats::track_event_tokenizer_errors);
      return ModuleResult::Handled();
    }

    if (state->IsIncrementalStateValid()) {
      TokenizeThreadDescriptor(state, thread);
    }

    context_->track_tracker->ReserveDescriptorThreadTrack(
        track.uuid(), track.parent_uuid(), name_id,
        static_cast<uint32_t>(thread.pid()),
        static_cast<uint32_t>(thread.tid()), packet_timestamp);
  } else if (track.has_process()) {
    protos::pbzero::ProcessDescriptor::Decoder process(track.process());

    if (!process.has_pid()) {
      PERFETTO_ELOG("No pid in ProcessDescriptor for track with uuid %" PRIu64,
                    track.uuid());
      context_->storage->IncrementStats(stats::track_event_tokenizer_errors);
      return ModuleResult::Handled();
    }

    context_->track_tracker->ReserveDescriptorProcessTrack(
        track.uuid(), name_id, static_cast<uint32_t>(process.pid()),
        packet_timestamp);
  } else if (track.has_counter()) {
    protos::pbzero::CounterDescriptor::Decoder counter(track.counter());

    StringId category_id = kNullStringId;
    if (counter.has_categories()) {
      // TODO(eseckler): Support multi-category events in the table schema.
      std::string categories;
      for (auto it = counter.categories(); it; ++it) {
        if (!categories.empty())
          categories += ",";
        categories.append((*it).data, (*it).size);
      }
      if (!categories.empty()) {
        category_id =
            context_->storage->InternString(base::StringView(categories));
      }
    }

    // TODO(eseckler): Intern counter tracks for specific counter types like
    // thread time, so that the same counter can be referred to from tracks with
    // different uuids. (Chrome may emit thread time values on behalf of other
    // threads, in which case it has to use absolute values on a different
    // track_uuid. Right now these absolute values are imported onto a separate
    // counter track than the other thread's regular thread time values.)
    if (name_id == kNullStringId) {
      switch (counter.type()) {
        case CounterDescriptor::COUNTER_UNSPECIFIED:
          break;
        case CounterDescriptor::COUNTER_THREAD_TIME_NS:
          name_id = counter_name_thread_time_id_;
          break;
        case CounterDescriptor::COUNTER_THREAD_INSTRUCTION_COUNT:
          name_id = counter_name_thread_instruction_count_id_;
          break;
      }
    }

    context_->track_tracker->ReserveDescriptorCounterTrack(
        track.uuid(), track.parent_uuid(), name_id, category_id,
        counter.unit_multiplier(), counter.is_incremental(),
        packet.trusted_packet_sequence_id());
  } else {
    context_->track_tracker->ReserveDescriptorChildTrack(
        track.uuid(), track.parent_uuid(), name_id);
  }

  // Let ProtoTraceTokenizer forward the packet to the parser.
  return ModuleResult::Ignored();
}

ModuleResult TrackEventTokenizer::TokenizeThreadDescriptorPacket(
    PacketSequenceState* state,
    const protos::pbzero::TracePacket::Decoder& packet) {
  if (PERFETTO_UNLIKELY(!packet.has_trusted_packet_sequence_id())) {
    PERFETTO_ELOG("ThreadDescriptor packet without trusted_packet_sequence_id");
    context_->storage->IncrementStats(stats::track_event_tokenizer_errors);
    return ModuleResult::Handled();
  }

  // TrackEvents will be ignored while incremental state is invalid. As a
  // consequence, we should also ignore any ThreadDescriptors received in this
  // state. Otherwise, any delta-encoded timestamps would be calculated
  // incorrectly once we move out of the packet loss state. Instead, wait until
  // the first subsequent descriptor after incremental state is cleared.
  if (!state->IsIncrementalStateValid()) {
    context_->storage->IncrementStats(stats::tokenizer_skipped_packets);
    return ModuleResult::Handled();
  }

  protos::pbzero::ThreadDescriptor::Decoder thread(packet.thread_descriptor());
  TokenizeThreadDescriptor(state, thread);

  // Let ProtoTraceTokenizer forward the packet to the parser.
  return ModuleResult::Ignored();
}

void TrackEventTokenizer::TokenizeThreadDescriptor(
    PacketSequenceState* state,
    const protos::pbzero::ThreadDescriptor::Decoder& thread) {
  // TODO(eseckler): Remove support for legacy thread descriptor-based default
  // tracks and delta timestamps.
  state->SetThreadDescriptor(thread.pid(), thread.tid(),
                             thread.reference_timestamp_us() * 1000,
                             thread.reference_thread_time_us() * 1000,
                             thread.reference_thread_instruction_count());
}

void TrackEventTokenizer::TokenizeTrackEventPacket(
    PacketSequenceState* state,
    const protos::pbzero::TracePacket::Decoder& packet,
    TraceBlobView* packet_blob,
    int64_t packet_timestamp) {
  if (PERFETTO_UNLIKELY(!packet.has_trusted_packet_sequence_id())) {
    PERFETTO_ELOG("TrackEvent packet without trusted_packet_sequence_id");
    context_->storage->IncrementStats(stats::track_event_tokenizer_errors);
    return;
  }

  auto field = packet.track_event();
  protos::pbzero::TrackEvent::Decoder event(field.data, field.size);

  protos::pbzero::TrackEventDefaults::Decoder* defaults =
      state->current_generation()->GetTrackEventDefaults();

  int64_t timestamp;
  std::unique_ptr<TrackEventData> data(
      new TrackEventData(std::move(*packet_blob), state->current_generation()));

  // TODO(eseckler): Remove handling of timestamps relative to ThreadDescriptors
  // once all producers have switched to clock-domain timestamps (e.g.
  // TracePacket's timestamp).

  if (event.has_timestamp_delta_us()) {
    // Delta timestamps require a valid ThreadDescriptor packet since the last
    // packet loss.
    if (!state->track_event_timestamps_valid()) {
      context_->storage->IncrementStats(stats::tokenizer_skipped_packets);
      return;
    }
    timestamp = state->IncrementAndGetTrackEventTimeNs(
        event.timestamp_delta_us() * 1000);

    // Legacy TrackEvent timestamp fields are in MONOTONIC domain. Adjust to
    // trace time if we have a clock snapshot.
    auto trace_ts = context_->clock_tracker->ToTraceTime(
        protos::pbzero::ClockSnapshot::Clock::MONOTONIC, timestamp);
    if (trace_ts.has_value())
      timestamp = trace_ts.value();
  } else if (int64_t ts_absolute_us = event.timestamp_absolute_us()) {
    // One-off absolute timestamps don't affect delta computation.
    timestamp = ts_absolute_us * 1000;

    // Legacy TrackEvent timestamp fields are in MONOTONIC domain. Adjust to
    // trace time if we have a clock snapshot.
    auto trace_ts = context_->clock_tracker->ToTraceTime(
        protos::pbzero::ClockSnapshot::Clock::MONOTONIC, timestamp);
    if (trace_ts.has_value())
      timestamp = trace_ts.value();
  } else if (packet.has_timestamp()) {
    timestamp = packet_timestamp;
  } else {
    PERFETTO_ELOG("TrackEvent without valid timestamp");
    context_->storage->IncrementStats(stats::track_event_tokenizer_errors);
    return;
  }

  if (event.has_thread_time_delta_us()) {
    // Delta timestamps require a valid ThreadDescriptor packet since the last
    // packet loss.
    if (!state->track_event_timestamps_valid()) {
      context_->storage->IncrementStats(stats::tokenizer_skipped_packets);
      return;
    }
    data->thread_timestamp = state->IncrementAndGetTrackEventThreadTimeNs(
        event.thread_time_delta_us() * 1000);
  } else if (event.has_thread_time_absolute_us()) {
    // One-off absolute timestamps don't affect delta computation.
    data->thread_timestamp = event.thread_time_absolute_us() * 1000;
  }

  if (event.has_thread_instruction_count_delta()) {
    // Delta timestamps require a valid ThreadDescriptor packet since the last
    // packet loss.
    if (!state->track_event_timestamps_valid()) {
      context_->storage->IncrementStats(stats::tokenizer_skipped_packets);
      return;
    }
    data->thread_instruction_count =
        state->IncrementAndGetTrackEventThreadInstructionCount(
            event.thread_instruction_count_delta());
  } else if (event.has_thread_instruction_count_absolute()) {
    // One-off absolute timestamps don't affect delta computation.
    data->thread_instruction_count = event.thread_instruction_count_absolute();
  }

  // TODO(eseckler): Also convert & attach counter values from TYPE_COUNTER
  // events and extra_counter_* fields.
  if (event.type() == protos::pbzero::TrackEvent::TYPE_COUNTER) {
    // Consider track_uuid from the packet and TrackEventDefaults.
    uint64_t track_uuid;
    if (event.has_track_uuid()) {
      track_uuid = event.track_uuid();
    } else if (defaults && defaults->has_track_uuid()) {
      track_uuid = defaults->track_uuid();
    } else {
      PERFETTO_DLOG(
          "Ignoring TrackEvent with counter_value but without track_uuid");
      context_->storage->IncrementStats(stats::track_event_tokenizer_errors);
      return;
    }

    if (!event.has_counter_value()) {
      PERFETTO_DLOG(
          "Ignoring TrackEvent with TYPE_COUNTER but without counter_value for "
          "track_uuid %" PRIu64,
          track_uuid);
      context_->storage->IncrementStats(stats::track_event_tokenizer_errors);
      return;
    }

    base::Optional<int64_t> value =
        context_->track_tracker->ConvertToAbsoluteCounterValue(
            track_uuid, packet.trusted_packet_sequence_id(),
            event.counter_value());

    if (!value) {
      PERFETTO_DLOG("Ignoring TrackEvent with invalid track_uuid %" PRIu64,
                    track_uuid);
      context_->storage->IncrementStats(stats::track_event_tokenizer_errors);
      return;
    }

    data->counter_value = *value;
  }

  if (event.has_extra_counter_values()) {
    // Consider extra_counter_track_uuids from the packet and
    // TrackEventDefaults.
    protozero::RepeatedFieldIterator<uint64_t> track_uuid_it;
    if (event.has_extra_counter_track_uuids()) {
      track_uuid_it = event.extra_counter_track_uuids();
    } else if (defaults && defaults->has_extra_counter_track_uuids()) {
      track_uuid_it = defaults->extra_counter_track_uuids();
    } else {
      PERFETTO_DLOG(
          "Ignoring TrackEvent with extra_counter_values but without "
          "extra_counter_track_uuids");
      context_->storage->IncrementStats(stats::track_event_tokenizer_errors);
      return;
    }

    size_t index = 0;
    for (auto value_it = event.extra_counter_values(); value_it;
         ++value_it, ++track_uuid_it, ++index) {
      if (!track_uuid_it) {
        PERFETTO_DLOG(
            "Ignoring TrackEvent with more extra_counter_values than "
            "extra_counter_track_uuids");
        context_->storage->IncrementStats(stats::track_event_tokenizer_errors);
        return;
      }
      if (index >= TrackEventData::kMaxNumExtraCounters) {
        PERFETTO_ELOG(
            "Ignoring TrackEvent with more extra_counter_values than "
            "TrackEventData::kMaxNumExtraCounters");
        context_->storage->IncrementStats(stats::track_event_tokenizer_errors);
        return;
      }
      base::Optional<int64_t> value =
          context_->track_tracker->ConvertToAbsoluteCounterValue(
              *track_uuid_it, packet.trusted_packet_sequence_id(), *value_it);
      if (!value) {
        PERFETTO_DLOG("Ignoring TrackEvent with invalid extra counter track");
        context_->storage->IncrementStats(stats::track_event_tokenizer_errors);
        return;
      }
      data->extra_counter_values[index] = *value;
    }
  }

  context_->sorter->PushTrackEventPacket(timestamp, std::move(data));
}

}  // namespace trace_processor
}  // namespace perfetto
