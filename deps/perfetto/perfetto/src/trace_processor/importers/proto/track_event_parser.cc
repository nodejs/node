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

#include "src/trace_processor/importers/proto/track_event_parser.h"

#include <string>

#include "perfetto/base/logging.h"
#include "perfetto/ext/base/string_writer.h"
#include "perfetto/trace_processor/status.h"
#include "src/trace_processor/importers/common/args_tracker.h"
#include "src/trace_processor/importers/common/event_tracker.h"
#include "src/trace_processor/importers/common/process_tracker.h"
#include "src/trace_processor/importers/common/track_tracker.h"
#include "src/trace_processor/importers/json/json_utils.h"
#include "src/trace_processor/importers/proto/args_table_utils.h"
#include "src/trace_processor/importers/proto/packet_sequence_state.h"
#include "src/trace_processor/importers/proto/track_event.descriptor.h"
#include "src/trace_processor/util/status_macros.h"

#include "protos/perfetto/trace/interned_data/interned_data.pbzero.h"
#include "protos/perfetto/trace/track_event/chrome_compositor_scheduler_state.pbzero.h"
#include "protos/perfetto/trace/track_event/chrome_legacy_ipc.pbzero.h"
#include "protos/perfetto/trace/track_event/chrome_process_descriptor.pbzero.h"
#include "protos/perfetto/trace/track_event/chrome_thread_descriptor.pbzero.h"
#include "protos/perfetto/trace/track_event/counter_descriptor.pbzero.h"
#include "protos/perfetto/trace/track_event/debug_annotation.pbzero.h"
#include "protos/perfetto/trace/track_event/log_message.pbzero.h"
#include "protos/perfetto/trace/track_event/process_descriptor.pbzero.h"
#include "protos/perfetto/trace/track_event/source_location.pbzero.h"
#include "protos/perfetto/trace/track_event/task_execution.pbzero.h"
#include "protos/perfetto/trace/track_event/thread_descriptor.pbzero.h"
#include "protos/perfetto/trace/track_event/track_descriptor.pbzero.h"
#include "protos/perfetto/trace/track_event/track_event.pbzero.h"

namespace perfetto {
namespace trace_processor {

namespace {
using BoundInserter = ArgsTracker::BoundInserter;
using protos::pbzero::TrackEvent;
using LegacyEvent = TrackEvent::LegacyEvent;
using protozero::ConstBytes;

// Slices which have been opened but haven't been closed yet will be marked
// with these placeholder values.
constexpr int64_t kPendingThreadDuration = -1;
constexpr int64_t kPendingThreadInstructionDelta = -1;

void AddStringToArgsTable(const char* field,
                          const protozero::ConstChars& str,
                          const ProtoToArgsTable::ParsingOverrideState& state,
                          BoundInserter* inserter) {
  auto val = state.context->storage->InternString(base::StringView(str));
  auto key = state.context->storage->InternString(base::StringView(field));
  inserter->AddArg(key, Variadic::String(val));
}

bool MaybeParseSourceLocation(
    std::string prefix,
    const ProtoToArgsTable::ParsingOverrideState& state,
    const protozero::Field& field,
    BoundInserter* inserter) {
  auto* decoder = state.sequence_state->LookupInternedMessage<
      protos::pbzero::InternedData::kSourceLocationsFieldNumber,
      protos::pbzero::SourceLocation>(field.as_uint64());
  if (!decoder) {
    // Lookup failed fall back on default behaviour which will just put
    // the source_location_iid into the args table.
    return false;
  }
  {
    ProtoToArgsTable::ScopedStringAppender scoped("file_name", &prefix);
    AddStringToArgsTable(prefix.c_str(), decoder->file_name(), state, inserter);
  }
  {
    ProtoToArgsTable::ScopedStringAppender scoped("function_name", &prefix);
    AddStringToArgsTable(prefix.c_str(), decoder->function_name(), state,
                         inserter);
  }
  ProtoToArgsTable::ScopedStringAppender scoped("line_number", &prefix);
  auto key = state.context->storage->InternString(base::StringView(prefix));
  inserter->AddArg(key, Variadic::Integer(decoder->line_number()));
  // By returning false we expect this field to be handled like regular.
  return true;
}

std::string SafeDebugAnnotationName(const std::string& raw_name) {
  std::string result = raw_name;
  std::replace(result.begin(), result.end(), '.', '_');
  std::replace(result.begin(), result.end(), '[', '_');
  std::replace(result.begin(), result.end(), ']', '_');
  result = "debug." + result;
  return result;
}
}  // namespace

class TrackEventParser::EventImporter {
 public:
  EventImporter(TrackEventParser* parser,
                int64_t ts,
                TrackEventData* event_data,
                ConstBytes blob)
      : context_(parser->context_),
        storage_(context_->storage.get()),
        parser_(parser),
        ts_(ts),
        event_data_(event_data),
        sequence_state_(event_data_->sequence_state),
        blob_(std::move(blob)),
        event_(blob_),
        legacy_event_(event_.legacy_event()),
        defaults_(sequence_state_->GetTrackEventDefaults()) {}

  util::Status Import() {
    // TODO(eseckler): This legacy event field will eventually be replaced by
    // fields in TrackEvent itself.
    if (PERFETTO_UNLIKELY(!event_.type() && !legacy_event_.has_phase()))
      return util::ErrStatus("TrackEvent without type or phase");

    category_id_ = ParseTrackEventCategory();
    name_id_ = ParseTrackEventName();

    RETURN_IF_ERROR(ParseTrackAssociation());

    // Counter-type events don't support arguments (those are on the
    // CounterDescriptor instead). All they have is a |counter_value|.
    if (event_.type() == TrackEvent::TYPE_COUNTER) {
      ParseCounterEvent();
      return util::OkStatus();
    }

    // If we have legacy thread time / instruction count fields, also parse them
    // into the counters tables.
    ParseLegacyThreadTimeAndInstructionsAsCounters();

    // Parse extra counter values before parsing the actual event. This way, we
    // can update the slice's thread time / instruction count fields based on
    // these counter values and also parse them as slice attributes / arguments.
    ParseExtraCounterValues();

    // TODO(eseckler): Replace phase with type and remove handling of
    // legacy_event_.phase() once it is no longer used by producers.
    int32_t phase = ParsePhaseOrType();

    switch (static_cast<char>(phase)) {
      case 'B':  // TRACE_EVENT_PHASE_BEGIN.
        return ParseThreadBeginEvent();
      case 'E':  // TRACE_EVENT_PHASE_END.
        return ParseThreadEndEvent();
      case 'X':  // TRACE_EVENT_PHASE_COMPLETE.
        return ParseThreadCompleteEvent();
      case 'i':
      case 'I':  // TRACE_EVENT_PHASE_INSTANT.
        return ParseThreadInstantEvent();
      case 'b':  // TRACE_EVENT_PHASE_NESTABLE_ASYNC_BEGIN
        return ParseAsyncBeginEvent();
      case 'e':  // TRACE_EVENT_PHASE_NESTABLE_ASYNC_END
        return ParseAsyncEndEvent();
      case 'n':  // TRACE_EVENT_PHASE_NESTABLE_ASYNC_INSTANT
        return ParseAsyncInstantEvent();
      case 'M':  // TRACE_EVENT_PHASE_METADATA (process and thread names).
        return ParseMetadataEvent();
      default:
        // Other events are proxied via the raw table for JSON export.
        return ParseLegacyEventAsRawEvent();
    }
  }

 private:
  StringId ParseTrackEventCategory() {
    StringId category_id = kNullStringId;

    std::vector<uint64_t> category_iids;
    for (auto it = event_.category_iids(); it; ++it) {
      category_iids.push_back(*it);
    }
    std::vector<protozero::ConstChars> category_strings;
    for (auto it = event_.categories(); it; ++it) {
      category_strings.push_back(*it);
    }

    // If there's a single category, we can avoid building a concatenated
    // string.
    if (PERFETTO_LIKELY(category_iids.size() == 1 &&
                        category_strings.empty())) {
      auto* decoder = sequence_state_->LookupInternedMessage<
          protos::pbzero::InternedData::kEventCategoriesFieldNumber,
          protos::pbzero::EventCategory>(category_iids[0]);
      if (decoder) {
        category_id = storage_->InternString(decoder->name());
      } else {
        char buffer[32];
        base::StringWriter writer(buffer, sizeof(buffer));
        writer.AppendLiteral("unknown(");
        writer.AppendUnsignedInt(category_iids[0]);
        writer.AppendChar(')');
        category_id = storage_->InternString(writer.GetStringView());
      }
    } else if (category_iids.empty() && category_strings.size() == 1) {
      category_id = storage_->InternString(category_strings[0]);
    } else if (category_iids.size() + category_strings.size() > 1) {
      // We concatenate the category strings together since we currently only
      // support a single "cat" column.
      // TODO(eseckler): Support multi-category events in the table schema.
      std::string categories;
      for (uint64_t iid : category_iids) {
        auto* decoder = sequence_state_->LookupInternedMessage<
            protos::pbzero::InternedData::kEventCategoriesFieldNumber,
            protos::pbzero::EventCategory>(iid);
        if (!decoder)
          continue;
        base::StringView name = decoder->name();
        if (!categories.empty())
          categories.append(",");
        categories.append(name.data(), name.size());
      }
      for (const protozero::ConstChars& cat : category_strings) {
        if (!categories.empty())
          categories.append(",");
        categories.append(cat.data, cat.size);
      }
      if (!categories.empty())
        category_id = storage_->InternString(base::StringView(categories));
    }

    return category_id;
  }

  StringId ParseTrackEventName() {
    uint64_t name_iid = event_.name_iid();
    if (!name_iid)
      name_iid = legacy_event_.name_iid();

    if (PERFETTO_LIKELY(name_iid)) {
      auto* decoder = sequence_state_->LookupInternedMessage<
          protos::pbzero::InternedData::kEventNamesFieldNumber,
          protos::pbzero::EventName>(name_iid);
      if (decoder)
        return storage_->InternString(decoder->name());
    } else if (event_.has_name()) {
      return storage_->InternString(event_.name());
    }

    return kNullStringId;
  }

  util::Status ParseTrackAssociation() {
    TrackTracker* track_tracker = context_->track_tracker.get();
    ProcessTracker* procs = context_->process_tracker.get();

    // Consider track_uuid from the packet and TrackEventDefaults, fall back to
    // the default descriptor track (uuid 0).
    track_uuid_ = event_.has_track_uuid()
                      ? event_.track_uuid()
                      : (defaults_ && defaults_->has_track_uuid()
                             ? defaults_->track_uuid()
                             : 0u);

    // Determine track from track_uuid specified in either TrackEvent or
    // TrackEventDefaults. If a non-default track is not set, we either:
    //   a) fall back to the track specified by the sequence's (or event's) pid
    //   +
    //      tid (only in case of legacy tracks/events, i.e. events that don't
    //      specify an explicit track uuid or use legacy event phases instead of
    //      TrackEvent types), or
    //   b) a default track.
    if (track_uuid_) {
      base::Optional<TrackId> opt_track_id =
          track_tracker->GetDescriptorTrack(track_uuid_);
      if (!opt_track_id) {
        track_tracker->ReserveDescriptorChildTrack(track_uuid_,
                                                   /*parent_uuid=*/0, name_id_);
        opt_track_id = track_tracker->GetDescriptorTrack(track_uuid_);
      }
      track_id_ = *opt_track_id;

      auto thread_track_row =
          storage_->thread_track_table().id().IndexOf(track_id_);
      if (thread_track_row) {
        utid_ = storage_->thread_track_table().utid()[*thread_track_row];
        upid_ = storage_->thread_table().upid()[*utid_];
      } else {
        auto process_track_row =
            storage_->process_track_table().id().IndexOf(track_id_);
        if (process_track_row) {
          upid_ = storage_->process_track_table().upid()[*process_track_row];
          if (sequence_state_->state()->pid_and_tid_valid()) {
            uint32_t pid =
                static_cast<uint32_t>(sequence_state_->state()->pid());
            uint32_t tid =
                static_cast<uint32_t>(sequence_state_->state()->tid());
            UniqueTid utid_candidate = procs->UpdateThread(tid, pid);
            if (storage_->thread_table().upid()[utid_candidate] == upid_)
              legacy_passthrough_utid_ = utid_candidate;
          }
        } else {
          auto* tracks = context_->storage->mutable_track_table();
          auto track_index = tracks->id().IndexOf(track_id_);
          if (track_index) {
            const StringPool::Id& id = tracks->name()[*track_index];
            if (id.is_null())
              tracks->mutable_name()->Set(*track_index, name_id_);
          }

          if (sequence_state_->state()->pid_and_tid_valid()) {
            uint32_t pid =
                static_cast<uint32_t>(sequence_state_->state()->pid());
            uint32_t tid =
                static_cast<uint32_t>(sequence_state_->state()->tid());
            legacy_passthrough_utid_ = procs->UpdateThread(tid, pid);
          }
        }
      }
    } else {
      bool pid_tid_state_valid = sequence_state_->state()->pid_and_tid_valid();

      // We have a 0-value |track_uuid|. Nevertheless, we should only fall back
      // if we have either no |track_uuid| specified at all or |track_uuid| was
      // set explicitly to 0 (e.g. to override a default track_uuid) and we have
      // a legacy phase. Events with real phases should use |track_uuid| to
      // specify a different track (or use the pid/tid_override fields).
      bool fallback_to_legacy_pid_tid_tracks =
          (!event_.has_track_uuid() || !event_.has_type()) &&
          pid_tid_state_valid;

      // Always allow fallback if we have a process override.
      fallback_to_legacy_pid_tid_tracks |= legacy_event_.has_pid_override();

      // A thread override requires a valid pid.
      fallback_to_legacy_pid_tid_tracks |=
          legacy_event_.has_tid_override() && pid_tid_state_valid;

      if (fallback_to_legacy_pid_tid_tracks) {
        uint32_t pid = static_cast<uint32_t>(sequence_state_->state()->pid());
        uint32_t tid = static_cast<uint32_t>(sequence_state_->state()->tid());
        if (legacy_event_.has_pid_override()) {
          pid = static_cast<uint32_t>(legacy_event_.pid_override());
          tid = static_cast<uint32_t>(-1);
        }
        if (legacy_event_.has_tid_override())
          tid = static_cast<uint32_t>(legacy_event_.tid_override());

        utid_ = procs->UpdateThread(tid, pid);
        upid_ = storage_->thread_table().upid()[*utid_];
        track_id_ = track_tracker->InternThreadTrack(*utid_);
      } else {
        track_id_ = track_tracker->GetOrCreateDefaultDescriptorTrack();
      }
    }

    if (!legacy_event_.has_phase())
      return util::OkStatus();

    // Legacy phases may imply a different track than the one specified by
    // the fallback (or default track uuid) above.
    switch (legacy_event_.phase()) {
      case 'b':
      case 'e':
      case 'n': {
        // Intern tracks for legacy async events based on legacy event ids.
        int64_t source_id = 0;
        bool source_id_is_process_scoped = false;
        if (legacy_event_.has_unscoped_id()) {
          source_id = static_cast<int64_t>(legacy_event_.unscoped_id());
        } else if (legacy_event_.has_global_id()) {
          source_id = static_cast<int64_t>(legacy_event_.global_id());
        } else if (legacy_event_.has_local_id()) {
          if (!upid_) {
            return util::ErrStatus(
                "TrackEvent with local_id without process association");
          }

          source_id = static_cast<int64_t>(legacy_event_.local_id());
          source_id_is_process_scoped = true;
        } else {
          return util::ErrStatus("Async LegacyEvent without ID");
        }

        // Catapult treats nestable async events of different categories with
        // the same ID as separate tracks. We replicate the same behavior
        // here.
        StringId id_scope = category_id_;
        if (legacy_event_.has_id_scope()) {
          std::string concat = storage_->GetString(category_id_).ToStdString() +
                               ":" + legacy_event_.id_scope().ToStdString();
          id_scope = storage_->InternString(base::StringView(concat));
        }

        track_id_ = context_->track_tracker->InternLegacyChromeAsyncTrack(
            name_id_, upid_ ? *upid_ : 0, source_id,
            source_id_is_process_scoped, id_scope);
        legacy_passthrough_utid_ = utid_;
        break;
      }
      case 'i':
      case 'I': {
        // Intern tracks for global or process-scoped legacy instant events.
        switch (legacy_event_.instant_event_scope()) {
          case LegacyEvent::SCOPE_UNSPECIFIED:
          case LegacyEvent::SCOPE_THREAD:
            // Thread-scoped legacy instant events already have the right
            // track based on the tid/pid of the sequence.
            if (!utid_) {
              return util::ErrStatus(
                  "Thread-scoped instant event without thread association");
            }
            break;
          case LegacyEvent::SCOPE_GLOBAL:
            track_id_ = context_->track_tracker
                            ->GetOrCreateLegacyChromeGlobalInstantTrack();
            legacy_passthrough_utid_ = utid_;
            utid_ = base::nullopt;
            break;
          case LegacyEvent::SCOPE_PROCESS:
            if (!upid_) {
              return util::ErrStatus(
                  "Process-scoped instant event without process association");
            }

            track_id_ =
                context_->track_tracker->InternLegacyChromeProcessInstantTrack(
                    *upid_);
            legacy_passthrough_utid_ = utid_;
            utid_ = base::nullopt;
            break;
        }
        break;
      }
      default:
        break;
    }

    return util::OkStatus();
  }

  int32_t ParsePhaseOrType() {
    if (legacy_event_.has_phase())
      return legacy_event_.phase();

    switch (event_.type()) {
      case TrackEvent::TYPE_SLICE_BEGIN:
        return utid_ ? 'B' : 'b';
      case TrackEvent::TYPE_SLICE_END:
        return utid_ ? 'E' : 'e';
      case TrackEvent::TYPE_INSTANT:
        return utid_ ? 'i' : 'n';
      default:
        PERFETTO_ELOG("unexpected event type %d", event_.type());
        return 0;
    }
  }

  void ParseCounterEvent() {
    // Tokenizer ensures that TYPE_COUNTER events are associated with counter
    // tracks and have values.
    PERFETTO_DCHECK(storage_->counter_track_table().id().IndexOf(track_id_));
    PERFETTO_DCHECK(event_.has_counter_value());

    context_->event_tracker->PushCounter(ts_, event_data_->counter_value,
                                         track_id_);
  }

  void ParseLegacyThreadTimeAndInstructionsAsCounters() {
    if (!utid_)
      return;
    // When these fields are set, we don't expect TrackDescriptor-based counters
    // for thread time or instruction count for this thread in the trace, so we
    // intern separate counter tracks based on name + utid. Note that we cannot
    // import the counter values from the end of a complete event, because the
    // EventTracker expects counters to be pushed in order of their timestamps.
    // One more reason to switch to split begin/end events.
    if (event_data_->thread_timestamp) {
      TrackId track_id = context_->track_tracker->InternThreadCounterTrack(
          parser_->counter_name_thread_time_id_, *utid_);
      context_->event_tracker->PushCounter(ts_, event_data_->thread_timestamp,
                                           track_id);
    }
    if (event_data_->thread_instruction_count) {
      TrackId track_id = context_->track_tracker->InternThreadCounterTrack(
          parser_->counter_name_thread_instruction_count_id_, *utid_);
      context_->event_tracker->PushCounter(
          ts_, event_data_->thread_instruction_count, track_id);
    }
  }

  void ParseExtraCounterValues() {
    if (!event_.has_extra_counter_values())
      return;

    protozero::RepeatedFieldIterator<uint64_t> track_uuid_it;
    if (event_.has_extra_counter_track_uuids()) {
      track_uuid_it = event_.extra_counter_track_uuids();
    } else if (defaults_ && defaults_->has_extra_counter_track_uuids()) {
      track_uuid_it = defaults_->extra_counter_track_uuids();
    }

    size_t index = 0;
    for (auto value_it = event_.extra_counter_values(); value_it;
         ++value_it, ++track_uuid_it, ++index) {
      // Tokenizer ensures that there aren't more values than uuids, that we
      // don't have more values than kMaxNumExtraCounters and that the
      // track_uuids are for valid counter tracks.
      PERFETTO_DCHECK(track_uuid_it);
      PERFETTO_DCHECK(index < TrackEventData::kMaxNumExtraCounters);

      base::Optional<TrackId> track_id =
          context_->track_tracker->GetDescriptorTrack(*track_uuid_it);
      base::Optional<uint32_t> counter_row =
          storage_->counter_track_table().id().IndexOf(*track_id);

      int64_t value = event_data_->extra_counter_values[index];
      context_->event_tracker->PushCounter(ts_, value, *track_id);

      // Also import thread_time and thread_instruction_count counters into
      // slice columns to simplify JSON export.
      StringId counter_name =
          storage_->counter_track_table().name()[*counter_row];
      if (counter_name == parser_->counter_name_thread_time_id_) {
        event_data_->thread_timestamp = value;
      } else if (counter_name ==
                 parser_->counter_name_thread_instruction_count_id_) {
        event_data_->thread_instruction_count = value;
      }
    }
  }

  util::Status ParseThreadBeginEvent() {
    if (!utid_) {
      return util::ErrStatus(
          "TrackEvent with phase B without thread association");
    }

    auto opt_slice_id = context_->slice_tracker->Begin(
        ts_, track_id_, category_id_, name_id_,
        [this](BoundInserter* inserter) { ParseTrackEventArgs(inserter); });
    if (opt_slice_id.has_value()) {
      auto* thread_slices = storage_->mutable_thread_slices();
      PERFETTO_DCHECK(!thread_slices->slice_count() ||
                      thread_slices->slice_ids().back() < opt_slice_id.value());
      thread_slices->AddThreadSlice(
          opt_slice_id.value(), event_data_->thread_timestamp,
          kPendingThreadDuration, event_data_->thread_instruction_count,
          kPendingThreadInstructionDelta);
    }

    return util::OkStatus();
  }

  util::Status ParseThreadEndEvent() {
    if (!utid_) {
      return util::ErrStatus(
          "TrackEvent with phase E without thread association");
    }

    auto opt_slice_id = context_->slice_tracker->End(
        ts_, track_id_, category_id_, name_id_,
        [this](BoundInserter* inserter) { ParseTrackEventArgs(inserter); });
    if (opt_slice_id.has_value()) {
      auto* thread_slices = storage_->mutable_thread_slices();
      thread_slices->UpdateThreadDeltasForSliceId(
          opt_slice_id.value(), event_data_->thread_timestamp,
          event_data_->thread_instruction_count);
    }

    return util::OkStatus();
  }

  util::Status ParseThreadCompleteEvent() {
    if (!utid_) {
      return util::ErrStatus(
          "TrackEvent with phase X without thread association");
    }

    auto duration_ns = legacy_event_.duration_us() * 1000;
    if (duration_ns < 0)
      return util::ErrStatus("TrackEvent with phase X with negative duration");
    auto opt_slice_id = context_->slice_tracker->Scoped(
        ts_, track_id_, category_id_, name_id_, duration_ns,
        [this](BoundInserter* inserter) { ParseTrackEventArgs(inserter); });
    if (opt_slice_id.has_value()) {
      auto* thread_slices = storage_->mutable_thread_slices();
      PERFETTO_DCHECK(!thread_slices->slice_count() ||
                      thread_slices->slice_ids().back() < opt_slice_id.value());
      auto thread_duration_ns = legacy_event_.thread_duration_us() * 1000;
      thread_slices->AddThreadSlice(
          opt_slice_id.value(), event_data_->thread_timestamp,
          thread_duration_ns, event_data_->thread_instruction_count,
          legacy_event_.thread_instruction_delta());
    }

    return util::OkStatus();
  }

  util::Status ParseThreadInstantEvent() {
    // Handle instant events as slices with zero duration, so that they end
    // up nested underneath their parent slices.
    int64_t duration_ns = 0;
    int64_t tidelta = 0;
    auto opt_slice_id = context_->slice_tracker->Scoped(
        ts_, track_id_, category_id_, name_id_, duration_ns,
        [this](BoundInserter* inserter) { ParseTrackEventArgs(inserter); });
    if (utid_ && opt_slice_id.has_value()) {
      auto* thread_slices = storage_->mutable_thread_slices();
      PERFETTO_DCHECK(!thread_slices->slice_count() ||
                      thread_slices->slice_ids().back() < opt_slice_id.value());
      thread_slices->AddThreadSlice(
          opt_slice_id.value(), event_data_->thread_timestamp, duration_ns,
          event_data_->thread_instruction_count, tidelta);
    }
    return util::OkStatus();
  }

  util::Status ParseAsyncBeginEvent() {
    auto opt_slice_id = context_->slice_tracker->Begin(
        ts_, track_id_, category_id_, name_id_,
        [this](BoundInserter* inserter) { ParseTrackEventArgs(inserter); });
    // For the time beeing, we only create vtrack slice rows if we need to
    // store thread timestamps/counters.
    if (legacy_event_.use_async_tts() && opt_slice_id.has_value()) {
      auto* vtrack_slices = storage_->mutable_virtual_track_slices();
      PERFETTO_DCHECK(!vtrack_slices->slice_count() ||
                      vtrack_slices->slice_ids().back() < opt_slice_id.value());
      vtrack_slices->AddVirtualTrackSlice(
          opt_slice_id.value(), event_data_->thread_timestamp,
          kPendingThreadDuration, event_data_->thread_instruction_count,
          kPendingThreadInstructionDelta);
    }
    return util::OkStatus();
  }

  util::Status ParseAsyncEndEvent() {
    auto opt_slice_id = context_->slice_tracker->End(
        ts_, track_id_, category_id_, name_id_,
        [this](BoundInserter* inserter) { ParseTrackEventArgs(inserter); });
    if (legacy_event_.use_async_tts() && opt_slice_id.has_value()) {
      auto* vtrack_slices = storage_->mutable_virtual_track_slices();
      vtrack_slices->UpdateThreadDeltasForSliceId(
          opt_slice_id.value(), event_data_->thread_timestamp,
          event_data_->thread_instruction_count);
    }
    return util::OkStatus();
  }

  util::Status ParseAsyncInstantEvent() {
    // Handle instant events as slices with zero duration, so that they end
    // up nested underneath their parent slices.
    int64_t duration_ns = 0;
    int64_t tidelta = 0;
    auto opt_slice_id = context_->slice_tracker->Scoped(
        ts_, track_id_, category_id_, name_id_, duration_ns,
        [this](BoundInserter* inserter) { ParseTrackEventArgs(inserter); });
    if (legacy_event_.use_async_tts() && opt_slice_id.has_value()) {
      auto* vtrack_slices = storage_->mutable_virtual_track_slices();
      PERFETTO_DCHECK(!vtrack_slices->slice_count() ||
                      vtrack_slices->slice_ids().back() < opt_slice_id.value());
      vtrack_slices->AddVirtualTrackSlice(
          opt_slice_id.value(), event_data_->thread_timestamp, duration_ns,
          event_data_->thread_instruction_count, tidelta);
    }
    return util::OkStatus();
  }

  util::Status ParseMetadataEvent() {
    ProcessTracker* procs = context_->process_tracker.get();

    // Parse process and thread names from correspondingly named events.
    NullTermStringView event_name = storage_->GetString(name_id_);
    PERFETTO_DCHECK(event_name.data());
    if (strcmp(event_name.c_str(), "thread_name") == 0) {
      if (!utid_) {
        return util::ErrStatus(
            "thread_name metadata event without thread association");
      }

      auto it = event_.debug_annotations();
      if (!it) {
        return util::ErrStatus(
            "thread_name metadata event without debug annotations");
      }
      protos::pbzero::DebugAnnotation::Decoder annotation(*it);
      auto thread_name = annotation.string_value();
      if (!thread_name.size)
        return util::OkStatus();
      auto thread_name_id = storage_->InternString(thread_name);
      // Don't override system-provided names.
      procs->SetThreadNameIfUnset(*utid_, thread_name_id);
      return util::OkStatus();
    }
    if (strcmp(event_name.c_str(), "process_name") == 0) {
      if (!upid_) {
        return util::ErrStatus(
            "process_name metadata event without process association");
      }

      auto it = event_.debug_annotations();
      if (!it) {
        return util::ErrStatus(
            "process_name metadata event without debug annotations");
      }
      protos::pbzero::DebugAnnotation::Decoder annotation(*it);
      auto process_name = annotation.string_value();
      if (!process_name.size)
        return util::OkStatus();
      auto process_name_id = storage_->InternString(process_name);
      // Don't override system-provided names.
      procs->SetProcessNameIfUnset(*upid_, process_name_id);
      return util::OkStatus();
    }
    // Other metadata events are proxied via the raw table for JSON export.
    ParseLegacyEventAsRawEvent();
    return util::OkStatus();
  }

  util::Status ParseLegacyEventAsRawEvent() {
    if (!utid_)
      return util::ErrStatus("raw legacy event without thread association");

    RawId id = storage_->mutable_raw_table()
                   ->Insert({ts_, parser_->raw_legacy_event_id_, 0, *utid_})
                   .id;

    ArgsTracker args(context_);
    auto inserter = args.AddArgsTo(id);

    inserter
        .AddArg(parser_->legacy_event_category_key_id_,
                Variadic::String(category_id_))
        .AddArg(parser_->legacy_event_name_key_id_, Variadic::String(name_id_));

    std::string phase_string(1, static_cast<char>(legacy_event_.phase()));
    StringId phase_id = storage_->InternString(phase_string.c_str());
    inserter.AddArg(parser_->legacy_event_phase_key_id_,
                    Variadic::String(phase_id));

    if (legacy_event_.has_duration_us()) {
      inserter.AddArg(parser_->legacy_event_duration_ns_key_id_,
                      Variadic::Integer(legacy_event_.duration_us() * 1000));
    }

    if (event_data_->thread_timestamp) {
      inserter.AddArg(parser_->legacy_event_thread_timestamp_ns_key_id_,
                      Variadic::Integer(event_data_->thread_timestamp));
      if (legacy_event_.has_thread_duration_us()) {
        inserter.AddArg(
            parser_->legacy_event_thread_duration_ns_key_id_,
            Variadic::Integer(legacy_event_.thread_duration_us() * 1000));
      }
    }

    if (event_data_->thread_instruction_count) {
      inserter.AddArg(parser_->legacy_event_thread_instruction_count_key_id_,
                      Variadic::Integer(event_data_->thread_instruction_count));
      if (legacy_event_.has_thread_instruction_delta()) {
        inserter.AddArg(
            parser_->legacy_event_thread_instruction_delta_key_id_,
            Variadic::Integer(legacy_event_.thread_instruction_delta()));
      }
    }

    if (legacy_event_.use_async_tts()) {
      inserter.AddArg(parser_->legacy_event_use_async_tts_key_id_,
                      Variadic::Boolean(true));
    }

    bool has_id = false;
    if (legacy_event_.has_unscoped_id()) {
      // Unscoped ids are either global or local depending on the phase. Pass
      // them through as unscoped IDs to JSON export to preserve this behavior.
      inserter.AddArg(parser_->legacy_event_unscoped_id_key_id_,
                      Variadic::UnsignedInteger(legacy_event_.unscoped_id()));
      has_id = true;
    } else if (legacy_event_.has_global_id()) {
      inserter.AddArg(parser_->legacy_event_global_id_key_id_,
                      Variadic::UnsignedInteger(legacy_event_.global_id()));
      has_id = true;
    } else if (legacy_event_.has_local_id()) {
      inserter.AddArg(parser_->legacy_event_local_id_key_id_,
                      Variadic::UnsignedInteger(legacy_event_.local_id()));
      has_id = true;
    }

    if (has_id && legacy_event_.has_id_scope() &&
        legacy_event_.id_scope().size) {
      inserter.AddArg(
          parser_->legacy_event_id_scope_key_id_,
          Variadic::String(storage_->InternString(legacy_event_.id_scope())));
    }

    // No need to parse legacy_event.instant_event_scope() because we import
    // instant events into the slice table.

    ParseTrackEventArgs(&inserter);
    return util::OkStatus();
  }

  void ParseTrackEventArgs(BoundInserter* inserter) {
    auto log_errors = [this](util::Status status) {
      if (status.ok())
        return;
      // Log error but continue parsing the other args.
      storage_->IncrementStats(stats::track_event_parser_errors);
      PERFETTO_DLOG("ParseTrackEventArgs error: %s", status.c_message());
    };

    for (auto it = event_.debug_annotations(); it; ++it) {
      log_errors(ParseDebugAnnotationArgs(*it, inserter));
    }

    if (event_.has_task_execution()) {
      log_errors(ParseTaskExecutionArgs(event_.task_execution(), inserter));
    }
    if (event_.has_log_message()) {
      log_errors(ParseLogMessage(event_.log_message(), inserter));
    }

    log_errors(parser_->proto_to_args_.InternProtoFieldsIntoArgsTable(
        blob_, ".perfetto.protos.TrackEvent", parser_->reflect_fields_,
        inserter, sequence_state_));

    if (legacy_passthrough_utid_) {
      inserter->AddArg(parser_->legacy_event_passthrough_utid_id_,
                       Variadic::UnsignedInteger(*legacy_passthrough_utid_),
                       ArgsTracker::UpdatePolicy::kSkipIfExists);
    }

    // TODO(eseckler): Parse legacy flow events into flow events table once we
    // have a design for it.
    if (legacy_event_.has_bind_id()) {
      inserter->AddArg(parser_->legacy_event_bind_id_key_id_,
                       Variadic::UnsignedInteger(legacy_event_.bind_id()));
    }

    if (legacy_event_.bind_to_enclosing()) {
      inserter->AddArg(parser_->legacy_event_bind_to_enclosing_key_id_,
                       Variadic::Boolean(true));
    }

    if (legacy_event_.flow_direction()) {
      StringId value = kNullStringId;
      switch (legacy_event_.flow_direction()) {
        case LegacyEvent::FLOW_IN:
          value = parser_->flow_direction_value_in_id_;
          break;
        case LegacyEvent::FLOW_OUT:
          value = parser_->flow_direction_value_out_id_;
          break;
        case LegacyEvent::FLOW_INOUT:
          value = parser_->flow_direction_value_inout_id_;
          break;
        default:
          PERFETTO_ELOG("Unknown flow direction: %d",
                        legacy_event_.flow_direction());
          break;
      }
      inserter->AddArg(parser_->legacy_event_flow_direction_key_id_,
                       Variadic::String(value));
    }
  }

  util::Status ParseDebugAnnotationArgs(ConstBytes debug_annotation,
                                        BoundInserter* inserter) {
    protos::pbzero::DebugAnnotation::Decoder annotation(debug_annotation);

    StringId name_id = kNullStringId;

    uint64_t name_iid = annotation.name_iid();
    if (PERFETTO_LIKELY(name_iid)) {
      auto* decoder = sequence_state_->LookupInternedMessage<
          protos::pbzero::InternedData::kDebugAnnotationNamesFieldNumber,
          protos::pbzero::DebugAnnotationName>(name_iid);
      if (!decoder)
        return util::ErrStatus("Debug annotation with invalid name_iid");

      std::string name_prefixed =
          SafeDebugAnnotationName(decoder->name().ToStdString());
      name_id = storage_->InternString(base::StringView(name_prefixed));
    } else if (annotation.has_name()) {
      name_id = storage_->InternString(annotation.name());
    } else {
      return util::ErrStatus("Debug annotation without name");
    }

    if (annotation.has_bool_value()) {
      inserter->AddArg(name_id, Variadic::Boolean(annotation.bool_value()));
    } else if (annotation.has_uint_value()) {
      inserter->AddArg(name_id,
                       Variadic::UnsignedInteger(annotation.uint_value()));
    } else if (annotation.has_int_value()) {
      inserter->AddArg(name_id, Variadic::Integer(annotation.int_value()));
    } else if (annotation.has_double_value()) {
      inserter->AddArg(name_id, Variadic::Real(annotation.double_value()));
    } else if (annotation.has_string_value()) {
      inserter->AddArg(
          name_id,
          Variadic::String(storage_->InternString(annotation.string_value())));
    } else if (annotation.has_pointer_value()) {
      inserter->AddArg(name_id, Variadic::Pointer(annotation.pointer_value()));
    } else if (annotation.has_legacy_json_value()) {
      if (!json::IsJsonSupported())
        return util::ErrStatus("Ignoring legacy_json_value (no json support)");

      auto value = json::ParseJsonString(annotation.legacy_json_value());
      auto name = storage_->GetString(name_id);
      json::AddJsonValueToArgs(*value, name, name, storage_, inserter);
    } else if (annotation.has_nested_value()) {
      auto name = storage_->GetString(name_id);
      ParseNestedValueArgs(annotation.nested_value(), name, name, inserter);
    }

    return util::OkStatus();
  }

  bool ParseNestedValueArgs(ConstBytes nested_value,
                            base::StringView flat_key,
                            base::StringView key,
                            BoundInserter* inserter) {
    protos::pbzero::DebugAnnotation::NestedValue::Decoder value(nested_value);
    switch (value.nested_type()) {
      case protos::pbzero::DebugAnnotation::NestedValue::UNSPECIFIED: {
        auto flat_key_id = storage_->InternString(flat_key);
        auto key_id = storage_->InternString(key);
        // Leaf value.
        if (value.has_bool_value()) {
          inserter->AddArg(flat_key_id, key_id,
                           Variadic::Boolean(value.bool_value()));
          return true;
        }
        if (value.has_int_value()) {
          inserter->AddArg(flat_key_id, key_id,
                           Variadic::Integer(value.int_value()));
          return true;
        }
        if (value.has_double_value()) {
          inserter->AddArg(flat_key_id, key_id,
                           Variadic::Real(value.double_value()));
          return true;
        }
        if (value.has_string_value()) {
          inserter->AddArg(
              flat_key_id, key_id,
              Variadic::String(storage_->InternString(value.string_value())));
          return true;
        }
        return false;
      }
      case protos::pbzero::DebugAnnotation::NestedValue::DICT: {
        auto key_it = value.dict_keys();
        auto value_it = value.dict_values();
        bool inserted = false;
        for (; key_it && value_it; ++key_it, ++value_it) {
          std::string child_name = (*key_it).ToStdString();
          std::string child_flat_key =
              flat_key.ToStdString() + "." + child_name;
          std::string child_key = key.ToStdString() + "." + child_name;
          inserted |=
              ParseNestedValueArgs(*value_it, base::StringView(child_flat_key),
                                   base::StringView(child_key), inserter);
        }
        return inserted;
      }
      case protos::pbzero::DebugAnnotation::NestedValue::ARRAY: {
        bool inserted_any = false;
        std::string array_key = key.ToStdString();
        StringId array_key_id = storage_->InternString(key);
        for (auto value_it = value.array_values(); value_it; ++value_it) {
          size_t array_index = inserter->GetNextArrayEntryIndex(array_key_id);
          std::string child_key =
              array_key + "[" + std::to_string(array_index) + "]";
          bool inserted = ParseNestedValueArgs(
              *value_it, flat_key, base::StringView(child_key), inserter);
          if (inserted)
            inserter->IncrementArrayEntryIndex(array_key_id);
          inserted_any |= inserted;
        }
        return inserted_any;
      }
    }
    return false;
  }

  util::Status ParseTaskExecutionArgs(ConstBytes task_execution,
                                      BoundInserter* inserter) {
    protos::pbzero::TaskExecution::Decoder task(task_execution);
    uint64_t iid = task.posted_from_iid();
    if (!iid)
      return util::ErrStatus("TaskExecution with invalid posted_from_iid");

    auto* decoder = sequence_state_->LookupInternedMessage<
        protos::pbzero::InternedData::kSourceLocationsFieldNumber,
        protos::pbzero::SourceLocation>(iid);
    if (!decoder)
      return util::ErrStatus("TaskExecution with invalid posted_from_iid");

    StringId file_name_id = kNullStringId;
    StringId function_name_id = kNullStringId;
    uint32_t line_number = 0;

    file_name_id = storage_->InternString(decoder->file_name());
    function_name_id = storage_->InternString(decoder->function_name());
    line_number = decoder->line_number();

    inserter->AddArg(parser_->task_file_name_args_key_id_,
                     Variadic::String(file_name_id));
    inserter->AddArg(parser_->task_function_name_args_key_id_,
                     Variadic::String(function_name_id));
    inserter->AddArg(parser_->task_line_number_args_key_id_,
                     Variadic::UnsignedInteger(line_number));
    return util::OkStatus();
  }

  util::Status ParseLogMessage(ConstBytes blob, BoundInserter* inserter) {
    if (!utid_)
      return util::ErrStatus("LogMessage without thread association");

    protos::pbzero::LogMessage::Decoder message(blob);

    StringId log_message_id = kNullStringId;

    auto* decoder = sequence_state_->LookupInternedMessage<
        protos::pbzero::InternedData::kLogMessageBodyFieldNumber,
        protos::pbzero::LogMessageBody>(message.body_iid());
    if (!decoder)
      return util::ErrStatus("LogMessage with invalid body_iid");

    log_message_id = storage_->InternString(decoder->body());

    // TODO(nicomazz): LogMessage also contains the source of the message (file
    // and line number). Android logs doesn't support this so far.
    storage_->mutable_android_log_table()->Insert(
        {ts_, *utid_,
         /*priority*/ 0,
         /*tag_id*/ kNullStringId,  // TODO(nicomazz): Abuse tag_id to display
                                    // "file_name:line_number".
         log_message_id});

    inserter->AddArg(parser_->log_message_body_key_id_,
                     Variadic::String(log_message_id));
    // TODO(nicomazz): Add the source location as an argument.
    return util::OkStatus();
  }

  TraceProcessorContext* context_;
  TraceStorage* storage_;
  TrackEventParser* parser_;
  int64_t ts_;
  TrackEventData* event_data_;
  PacketSequenceStateGeneration* sequence_state_;
  ConstBytes blob_;
  TrackEvent::Decoder event_;
  LegacyEvent::Decoder legacy_event_;
  protos::pbzero::TrackEventDefaults::Decoder* defaults_;

  // Importing state.
  StringId category_id_;
  StringId name_id_;
  uint64_t track_uuid_;
  TrackId track_id_;
  base::Optional<UniqueTid> utid_;
  base::Optional<UniqueTid> upid_;
  // All events in legacy JSON require a thread ID, but for some types of
  // events (e.g. async events or process/global-scoped instants), we don't
  // store it in the slice/track model. To pass the utid through to the json
  // export, we store it in an arg.
  base::Optional<UniqueTid> legacy_passthrough_utid_;
};

TrackEventParser::TrackEventParser(TraceProcessorContext* context)
    : context_(context),
      proto_to_args_(context_),
      counter_name_thread_time_id_(
          context->storage->InternString("thread_time")),
      counter_name_thread_instruction_count_id_(
          context->storage->InternString("thread_instruction_count")),
      task_file_name_args_key_id_(
          context->storage->InternString("task.posted_from.file_name")),
      task_function_name_args_key_id_(
          context->storage->InternString("task.posted_from.function_name")),
      task_line_number_args_key_id_(
          context->storage->InternString("task.posted_from.line_number")),
      log_message_body_key_id_(
          context->storage->InternString("track_event.log_message")),
      raw_legacy_event_id_(
          context->storage->InternString("track_event.legacy_event")),
      legacy_event_passthrough_utid_id_(
          context->storage->InternString("legacy_event.passthrough_utid")),
      legacy_event_category_key_id_(
          context->storage->InternString("legacy_event.category")),
      legacy_event_name_key_id_(
          context->storage->InternString("legacy_event.name")),
      legacy_event_phase_key_id_(
          context->storage->InternString("legacy_event.phase")),
      legacy_event_duration_ns_key_id_(
          context->storage->InternString("legacy_event.duration_ns")),
      legacy_event_thread_timestamp_ns_key_id_(
          context->storage->InternString("legacy_event.thread_timestamp_ns")),
      legacy_event_thread_duration_ns_key_id_(
          context->storage->InternString("legacy_event.thread_duration_ns")),
      legacy_event_thread_instruction_count_key_id_(
          context->storage->InternString(
              "legacy_event.thread_instruction_count")),
      legacy_event_thread_instruction_delta_key_id_(
          context->storage->InternString(
              "legacy_event.thread_instruction_delta")),
      legacy_event_use_async_tts_key_id_(
          context->storage->InternString("legacy_event.use_async_tts")),
      legacy_event_unscoped_id_key_id_(
          context->storage->InternString("legacy_event.unscoped_id")),
      legacy_event_global_id_key_id_(
          context->storage->InternString("legacy_event.global_id")),
      legacy_event_local_id_key_id_(
          context->storage->InternString("legacy_event.local_id")),
      legacy_event_id_scope_key_id_(
          context->storage->InternString("legacy_event.id_scope")),
      legacy_event_bind_id_key_id_(
          context->storage->InternString("legacy_event.bind_id")),
      legacy_event_bind_to_enclosing_key_id_(
          context->storage->InternString("legacy_event.bind_to_enclosing")),
      legacy_event_flow_direction_key_id_(
          context->storage->InternString("legacy_event.flow_direction")),
      flow_direction_value_in_id_(context->storage->InternString("in")),
      flow_direction_value_out_id_(context->storage->InternString("out")),
      flow_direction_value_inout_id_(context->storage->InternString("inout")),
      chrome_legacy_ipc_class_args_key_id_(
          context->storage->InternString("legacy_ipc.class")),
      chrome_legacy_ipc_line_args_key_id_(
          context->storage->InternString("legacy_ipc.line")),
      chrome_legacy_ipc_class_ids_{
          {context->storage->InternString("UNSPECIFIED"),
           context->storage->InternString("AUTOMATION"),
           context->storage->InternString("FRAME"),
           context->storage->InternString("PAGE"),
           context->storage->InternString("VIEW"),
           context->storage->InternString("WIDGET"),
           context->storage->InternString("INPUT"),
           context->storage->InternString("TEST"),
           context->storage->InternString("WORKER"),
           context->storage->InternString("NACL"),
           context->storage->InternString("GPU_CHANNEL"),
           context->storage->InternString("MEDIA"),
           context->storage->InternString("PPAPI"),
           context->storage->InternString("CHROME"),
           context->storage->InternString("DRAG"),
           context->storage->InternString("PRINT"),
           context->storage->InternString("EXTENSION"),
           context->storage->InternString("TEXT_INPUT_CLIENT"),
           context->storage->InternString("BLINK_TEST"),
           context->storage->InternString("ACCESSIBILITY"),
           context->storage->InternString("PRERENDER"),
           context->storage->InternString("CHROMOTING"),
           context->storage->InternString("BROWSER_PLUGIN"),
           context->storage->InternString("ANDROID_WEB_VIEW"),
           context->storage->InternString("NACL_HOST"),
           context->storage->InternString("ENCRYPTED_MEDIA"),
           context->storage->InternString("CAST"),
           context->storage->InternString("GIN_JAVA_BRIDGE"),
           context->storage->InternString("CHROME_UTILITY_PRINTING"),
           context->storage->InternString("OZONE_GPU"),
           context->storage->InternString("WEB_TEST"),
           context->storage->InternString("NETWORK_HINTS"),
           context->storage->InternString("EXTENSIONS_GUEST_VIEW"),
           context->storage->InternString("GUEST_VIEW"),
           context->storage->InternString("MEDIA_PLAYER_DELEGATE"),
           context->storage->InternString("EXTENSION_WORKER"),
           context->storage->InternString("SUBRESOURCE_FILTER"),
           context->storage->InternString("UNFREEZABLE_FRAME")}},
      chrome_process_name_ids_{
          {kNullStringId, context_->storage->InternString("Browser"),
           context_->storage->InternString("Renderer"),
           context_->storage->InternString("Utility"),
           context_->storage->InternString("Zygote"),
           context_->storage->InternString("SandboxHelper"),
           context_->storage->InternString("Gpu"),
           context_->storage->InternString("PpapiPlugin"),
           context_->storage->InternString("PpapiBroker")}},
      chrome_thread_name_ids_{
          {kNullStringId, context_->storage->InternString("CrProcessMain"),
           context_->storage->InternString("ChromeIOThread"),
           context_->storage->InternString("ThreadPoolBackgroundWorker&"),
           context_->storage->InternString("ThreadPoolForegroundWorker&"),
           context_->storage->InternString(
               "ThreadPoolSingleThreadForegroundBlocking&"),
           context_->storage->InternString(
               "ThreadPoolSingleThreadBackgroundBlocking&"),
           context_->storage->InternString("ThreadPoolService"),
           context_->storage->InternString("Compositor"),
           context_->storage->InternString("VizCompositorThread"),
           context_->storage->InternString("CompositorTileWorker&"),
           context_->storage->InternString("ServiceWorkerThread&"),
           context_->storage->InternString("MemoryInfra"),
           context_->storage->InternString("StackSamplingProfiler")}},
      counter_unit_ids_{{kNullStringId, context_->storage->InternString("ns"),
                         context_->storage->InternString("count"),
                         context_->storage->InternString("bytes")}} {
  auto status = proto_to_args_.AddProtoFileDescriptor(
      kTrackEventDescriptor.data(), kTrackEventDescriptor.size());
  PERFETTO_DCHECK(status.ok());

  // Switch |source_location_iid| into its interned data variant.
  proto_to_args_.AddParsingOverride(
      "begin_impl_frame_args.current_args.source_location_iid",
      [](const ProtoToArgsTable::ParsingOverrideState& state,
         const protozero::Field& field, BoundInserter* inserter) {
        return MaybeParseSourceLocation("begin_impl_frame_args.current_args",
                                        state, field, inserter);
      });
  proto_to_args_.AddParsingOverride(
      "begin_impl_frame_args.last_args.source_location_iid",
      [](const ProtoToArgsTable::ParsingOverrideState& state,
         const protozero::Field& field, BoundInserter* inserter) {
        return MaybeParseSourceLocation("begin_impl_frame_args.last_args",
                                        state, field, inserter);
      });
  proto_to_args_.AddParsingOverride(
      "begin_frame_observer_state.last_begin_frame_args.source_location_iid",
      [](const ProtoToArgsTable::ParsingOverrideState& state,
         const protozero::Field& field, BoundInserter* inserter) {
        return MaybeParseSourceLocation(
            "begin_frame_observer_state.last_begin_frame_args", state, field,
            inserter);
      });

  for (uint16_t index : kReflectFields) {
    reflect_fields_.push_back(index);
  }
}

void TrackEventParser::ParseTrackDescriptor(
    protozero::ConstBytes track_descriptor) {
  protos::pbzero::TrackDescriptor::Decoder decoder(track_descriptor);

  // Ensure that the track and its parents are resolved. This may start a new
  // process and/or thread (i.e. new upid/utid).
  TrackId track_id =
      *context_->track_tracker->GetDescriptorTrack(decoder.uuid());

  if (decoder.has_thread()) {
    UniqueTid utid = ParseThreadDescriptor(decoder.thread());
    if (decoder.has_chrome_thread())
      ParseChromeThreadDescriptor(utid, decoder.chrome_thread());
  } else if (decoder.has_process()) {
    UniquePid upid = ParseProcessDescriptor(decoder.process());
    if (decoder.has_chrome_process())
      ParseChromeProcessDescriptor(upid, decoder.chrome_process());
  } else if (decoder.has_counter()) {
    ParseCounterDescriptor(track_id, decoder.counter());
  }

  // Override the name with the most recent name seen (after sorting by ts).
  if (decoder.has_name()) {
    auto* tracks = context_->storage->mutable_track_table();
    StringId name_id = context_->storage->InternString(decoder.name());
    tracks->mutable_name()->Set(*tracks->id().IndexOf(track_id), name_id);
  }
}

UniquePid TrackEventParser::ParseProcessDescriptor(
    protozero::ConstBytes process_descriptor) {
  protos::pbzero::ProcessDescriptor::Decoder decoder(process_descriptor);
  UniquePid upid = context_->process_tracker->GetOrCreateProcess(
      static_cast<uint32_t>(decoder.pid()));
  if (decoder.has_process_name() && decoder.process_name().size) {
    // Don't override system-provided names.
    context_->process_tracker->SetProcessNameIfUnset(
        upid, context_->storage->InternString(decoder.process_name()));
  }
  // TODO(skyostil): Remove parsing for legacy chrome_process_type field.
  if (decoder.has_chrome_process_type()) {
    auto process_type = decoder.chrome_process_type();
    size_t name_index =
        static_cast<size_t>(process_type) < chrome_process_name_ids_.size()
            ? static_cast<size_t>(process_type)
            : 0u;
    StringId name_id = chrome_process_name_ids_[name_index];
    // Don't override system-provided names.
    context_->process_tracker->SetProcessNameIfUnset(upid, name_id);
  }
  return upid;
}

void TrackEventParser::ParseChromeProcessDescriptor(
    UniquePid upid,
    protozero::ConstBytes chrome_process_descriptor) {
  protos::pbzero::ChromeProcessDescriptor::Decoder decoder(
      chrome_process_descriptor);
  auto process_type = decoder.process_type();
  size_t name_index =
      static_cast<size_t>(process_type) < chrome_process_name_ids_.size()
          ? static_cast<size_t>(process_type)
          : 0u;
  StringId name_id = chrome_process_name_ids_[name_index];
  // Don't override system-provided names.
  context_->process_tracker->SetProcessNameIfUnset(upid, name_id);
}

UniqueTid TrackEventParser::ParseThreadDescriptor(
    protozero::ConstBytes thread_descriptor) {
  protos::pbzero::ThreadDescriptor::Decoder decoder(thread_descriptor);
  UniqueTid utid = context_->process_tracker->UpdateThread(
      static_cast<uint32_t>(decoder.tid()),
      static_cast<uint32_t>(decoder.pid()));
  StringId name_id = kNullStringId;
  if (decoder.has_thread_name() && decoder.thread_name().size) {
    name_id = context_->storage->InternString(decoder.thread_name());
  } else if (decoder.has_chrome_thread_type()) {
    // TODO(skyostil): Remove parsing for legacy chrome_thread_type field.
    auto thread_type = decoder.chrome_thread_type();
    size_t name_index =
        static_cast<size_t>(thread_type) < chrome_thread_name_ids_.size()
            ? static_cast<size_t>(thread_type)
            : 0u;
    name_id = chrome_thread_name_ids_[name_index];
  }
  if (name_id != kNullStringId) {
    // Don't override system-provided names.
    context_->process_tracker->SetThreadNameIfUnset(utid, name_id);
  }
  return utid;
}

void TrackEventParser::ParseChromeThreadDescriptor(
    UniqueTid utid,
    protozero::ConstBytes chrome_thread_descriptor) {
  protos::pbzero::ChromeThreadDescriptor::Decoder decoder(
      chrome_thread_descriptor);
  if (!decoder.has_thread_type())
    return;

  auto thread_type = decoder.thread_type();
  size_t name_index =
      static_cast<size_t>(thread_type) < chrome_thread_name_ids_.size()
          ? static_cast<size_t>(thread_type)
          : 0u;
  StringId name_id = chrome_thread_name_ids_[name_index];
  context_->process_tracker->SetThreadNameIfUnset(utid, name_id);
}

void TrackEventParser::ParseCounterDescriptor(
    TrackId track_id,
    protozero::ConstBytes counter_descriptor) {
  using protos::pbzero::CounterDescriptor;

  CounterDescriptor::Decoder decoder(counter_descriptor);
  auto* counter_tracks = context_->storage->mutable_counter_track_table();

  size_t unit_index = static_cast<size_t>(decoder.unit());
  if (unit_index >= counter_unit_ids_.size())
    unit_index = CounterDescriptor::UNIT_UNSPECIFIED;

  switch (decoder.type()) {
    case CounterDescriptor::COUNTER_UNSPECIFIED:
      break;
    case CounterDescriptor::COUNTER_THREAD_TIME_NS:
      unit_index = CounterDescriptor::UNIT_TIME_NS;
      counter_tracks->mutable_name()->Set(
          *counter_tracks->id().IndexOf(track_id),
          counter_name_thread_time_id_);
      break;
    case CounterDescriptor::COUNTER_THREAD_INSTRUCTION_COUNT:
      unit_index = CounterDescriptor::UNIT_COUNT;
      counter_tracks->mutable_name()->Set(
          *counter_tracks->id().IndexOf(track_id),
          counter_name_thread_instruction_count_id_);
      break;
  }

  counter_tracks->mutable_unit()->Set(*counter_tracks->id().IndexOf(track_id),
                                      counter_unit_ids_[unit_index]);
}

void TrackEventParser::ParseTrackEvent(int64_t ts,
                                       TrackEventData* event_data,
                                       ConstBytes blob) {
  util::Status status =
      EventImporter(this, ts, event_data, std::move(blob)).Import();
  if (!status.ok()) {
    context_->storage->IncrementStats(stats::track_event_parser_errors);
    PERFETTO_DLOG("ParseTrackEvent error: %s", status.c_message());
  }
}

}  // namespace trace_processor
}  // namespace perfetto
