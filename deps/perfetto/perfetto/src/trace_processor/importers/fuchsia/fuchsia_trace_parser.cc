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

#include "src/trace_processor/importers/fuchsia/fuchsia_trace_parser.h"

#include "src/trace_processor/importers/common/args_tracker.h"
#include "src/trace_processor/importers/common/event_tracker.h"
#include "src/trace_processor/importers/common/process_tracker.h"
#include "src/trace_processor/importers/common/slice_tracker.h"
#include "src/trace_processor/importers/common/track_tracker.h"

namespace perfetto {
namespace trace_processor {

namespace {
// Record Types
constexpr uint32_t kEvent = 4;

// Event Types
constexpr uint32_t kInstant = 0;
constexpr uint32_t kCounter = 1;
constexpr uint32_t kDurationBegin = 2;
constexpr uint32_t kDurationEnd = 3;
constexpr uint32_t kDurationComplete = 4;
constexpr uint32_t kAsyncBegin = 5;
constexpr uint32_t kAsyncInstant = 6;
constexpr uint32_t kAsyncEnd = 7;

// Argument Types
constexpr uint32_t kNull = 0;
constexpr uint32_t kInt32 = 1;
constexpr uint32_t kUint32 = 2;
constexpr uint32_t kInt64 = 3;
constexpr uint32_t kUint64 = 4;
constexpr uint32_t kDouble = 5;
constexpr uint32_t kString = 6;
constexpr uint32_t kPointer = 7;
constexpr uint32_t kKoid = 8;

struct Arg {
  StringId name;
  fuchsia_trace_utils::ArgValue value;
};
}  // namespace

FuchsiaTraceParser::FuchsiaTraceParser(TraceProcessorContext* context)
    : context_(context) {}

FuchsiaTraceParser::~FuchsiaTraceParser() = default;

void FuchsiaTraceParser::ParseFtracePacket(uint32_t,
                                           int64_t,
                                           TimestampedTracePiece) {
  PERFETTO_FATAL("Fuchsia Trace Parser cannot handle ftrace packets.");
}

void FuchsiaTraceParser::ParseTracePacket(int64_t, TimestampedTracePiece ttp) {
  PERFETTO_DCHECK(ttp.type == TimestampedTracePiece::Type::kFuchsiaRecord);
  PERFETTO_DCHECK(ttp.fuchsia_record != nullptr);

  // The timestamp is also present in the record, so we'll ignore the one passed
  // as an argument.
  fuchsia_trace_utils::RecordCursor cursor(
      ttp.fuchsia_record->record_view()->data(),
      ttp.fuchsia_record->record_view()->length());
  FuchsiaRecord* record = ttp.fuchsia_record.get();
  ProcessTracker* procs = context_->process_tracker.get();
  SliceTracker* slices = context_->slice_tracker.get();

  uint64_t header;
  if (!cursor.ReadUint64(&header)) {
    context_->storage->IncrementStats(stats::fuchsia_invalid_event);
    return;
  }
  uint32_t record_type = fuchsia_trace_utils::ReadField<uint32_t>(header, 0, 3);
  switch (record_type) {
    case kEvent: {
      uint32_t event_type =
          fuchsia_trace_utils::ReadField<uint32_t>(header, 16, 19);
      uint32_t n_args =
          fuchsia_trace_utils::ReadField<uint32_t>(header, 20, 23);
      uint32_t thread_ref =
          fuchsia_trace_utils::ReadField<uint32_t>(header, 24, 31);
      uint32_t cat_ref =
          fuchsia_trace_utils::ReadField<uint32_t>(header, 32, 47);
      uint32_t name_ref =
          fuchsia_trace_utils::ReadField<uint32_t>(header, 48, 63);

      int64_t ts;
      if (!cursor.ReadTimestamp(record->get_ticks_per_second(), &ts)) {
        context_->storage->IncrementStats(stats::fuchsia_invalid_event);
        return;
      }
      fuchsia_trace_utils::ThreadInfo tinfo;
      if (fuchsia_trace_utils::IsInlineThread(thread_ref)) {
        if (!cursor.ReadInlineThread(&tinfo)) {
          context_->storage->IncrementStats(stats::fuchsia_invalid_event);
          return;
        }
      } else {
        tinfo = record->GetThread(thread_ref);
      }
      StringId cat;
      if (fuchsia_trace_utils::IsInlineString(cat_ref)) {
        base::StringView cat_string_view;
        if (!cursor.ReadInlineString(cat_ref, &cat_string_view)) {
          context_->storage->IncrementStats(stats::fuchsia_invalid_event);
          return;
        }
        cat = context_->storage->InternString(cat_string_view);
      } else {
        cat = record->GetString(cat_ref);
      }
      StringId name;
      if (fuchsia_trace_utils::IsInlineString(name_ref)) {
        base::StringView name_string_view;
        if (!cursor.ReadInlineString(name_ref, &name_string_view)) {
          context_->storage->IncrementStats(stats::fuchsia_invalid_event);
          return;
        }
        name = context_->storage->InternString(name_string_view);
      } else {
        name = record->GetString(name_ref);
      }

      // Read arguments
      std::vector<Arg> args;
      for (uint32_t i = 0; i < n_args; i++) {
        size_t arg_base = cursor.WordIndex();
        uint64_t arg_header;
        if (!cursor.ReadUint64(&arg_header)) {
          context_->storage->IncrementStats(stats::fuchsia_invalid_event);
          return;
        }
        uint32_t arg_type =
            fuchsia_trace_utils::ReadField<uint32_t>(arg_header, 0, 3);
        uint32_t arg_size_words =
            fuchsia_trace_utils::ReadField<uint32_t>(arg_header, 4, 15);
        uint32_t arg_name_ref =
            fuchsia_trace_utils::ReadField<uint32_t>(arg_header, 16, 31);
        Arg arg;
        if (fuchsia_trace_utils::IsInlineString(arg_name_ref)) {
          base::StringView arg_name_view;
          if (!cursor.ReadInlineString(arg_name_ref, &arg_name_view)) {
            context_->storage->IncrementStats(stats::fuchsia_invalid_event);
            return;
          }
          arg.name = context_->storage->InternString(arg_name_view);
        } else {
          arg.name = record->GetString(arg_name_ref);
        }

        switch (arg_type) {
          case kNull:
            arg.value = fuchsia_trace_utils::ArgValue::Null();
            break;
          case kInt32:
            arg.value = fuchsia_trace_utils::ArgValue::Int32(
                fuchsia_trace_utils::ReadField<int32_t>(arg_header, 32, 63));
            break;
          case kUint32:
            arg.value = fuchsia_trace_utils::ArgValue::Uint32(
                fuchsia_trace_utils::ReadField<uint32_t>(arg_header, 32, 63));
            break;
          case kInt64: {
            int64_t value;
            if (!cursor.ReadInt64(&value)) {
              context_->storage->IncrementStats(stats::fuchsia_invalid_event);
              return;
            }
            arg.value = fuchsia_trace_utils::ArgValue::Int64(value);
            break;
          }
          case kUint64: {
            uint64_t value;
            if (!cursor.ReadUint64(&value)) {
              context_->storage->IncrementStats(stats::fuchsia_invalid_event);
              return;
            }
            arg.value = fuchsia_trace_utils::ArgValue::Uint64(value);
            break;
          }
          case kDouble: {
            double value;
            if (!cursor.ReadDouble(&value)) {
              context_->storage->IncrementStats(stats::fuchsia_invalid_event);
              return;
            }
            arg.value = fuchsia_trace_utils::ArgValue::Double(value);
            break;
          }
          case kString: {
            uint32_t arg_value_ref =
                fuchsia_trace_utils::ReadField<uint32_t>(arg_header, 32, 47);
            StringId value;
            if (fuchsia_trace_utils::IsInlineString(arg_value_ref)) {
              base::StringView arg_value_view;
              if (!cursor.ReadInlineString(arg_value_ref, &arg_value_view)) {
                context_->storage->IncrementStats(stats::fuchsia_invalid_event);
                return;
              }
              value = context_->storage->InternString(arg_value_view);
            } else {
              value = record->GetString(arg_value_ref);
            }
            arg.value = fuchsia_trace_utils::ArgValue::String(value);
            break;
          }
          case kPointer: {
            uint64_t value;
            if (!cursor.ReadUint64(&value)) {
              context_->storage->IncrementStats(stats::fuchsia_invalid_event);
              return;
            }
            arg.value = fuchsia_trace_utils::ArgValue::Pointer(value);
            break;
          }
          case kKoid: {
            uint64_t value;
            if (!cursor.ReadUint64(&value)) {
              context_->storage->IncrementStats(stats::fuchsia_invalid_event);
              return;
            }
            arg.value = fuchsia_trace_utils::ArgValue::Koid(value);
            break;
          }
          default:
            arg.value = fuchsia_trace_utils::ArgValue::Unknown();
            break;
        }

        args.push_back(arg);
        cursor.SetWordIndex(arg_base + arg_size_words);
      }

      switch (event_type) {
        case kInstant: {
          UniqueTid utid =
              procs->UpdateThread(static_cast<uint32_t>(tinfo.tid),
                                  static_cast<uint32_t>(tinfo.pid));
          InstantId id = context_->event_tracker->PushInstant(
              ts, name, utid, RefType::kRefUtid);
          auto inserter = context_->args_tracker->AddArgsTo(id);
          for (const Arg& arg : args) {
            inserter.AddArg(
                arg.name, arg.value.ToStorageVariadic(context_->storage.get()));
          }
          context_->args_tracker->Flush();
          break;
        }
        case kCounter: {
          UniqueTid utid =
              procs->UpdateThread(static_cast<uint32_t>(tinfo.tid),
                                  static_cast<uint32_t>(tinfo.pid));
          std::string name_str =
              context_->storage->GetString(name).ToStdString();
          // Note: In the Fuchsia trace format, counter values are stored in the
          // arguments for the record, with the data series defined by both the
          // record name and the argument name. In Perfetto, counters only have
          // one name, so we combine both names into one here.
          for (const Arg& arg : args) {
            std::string counter_name_str = name_str + ":";
            counter_name_str +=
                context_->storage->GetString(arg.name).ToStdString();
            bool is_valid_value = false;
            double counter_value = -1;
            switch (arg.value.Type()) {
              case fuchsia_trace_utils::ArgValue::kInt32:
                is_valid_value = true;
                counter_value = static_cast<double>(arg.value.Int32());
                break;
              case fuchsia_trace_utils::ArgValue::kUint32:
                is_valid_value = true;
                counter_value = static_cast<double>(arg.value.Uint32());
                break;
              case fuchsia_trace_utils::ArgValue::kInt64:
                is_valid_value = true;
                counter_value = static_cast<double>(arg.value.Int64());
                break;
              case fuchsia_trace_utils::ArgValue::kUint64:
                is_valid_value = true;
                counter_value = static_cast<double>(arg.value.Uint64());
                break;
              case fuchsia_trace_utils::ArgValue::kDouble:
                is_valid_value = true;
                counter_value = arg.value.Double();
                break;
              case fuchsia_trace_utils::ArgValue::kNull:
              case fuchsia_trace_utils::ArgValue::kString:
              case fuchsia_trace_utils::ArgValue::kPointer:
              case fuchsia_trace_utils::ArgValue::kKoid:
              case fuchsia_trace_utils::ArgValue::kUnknown:
                context_->storage->IncrementStats(
                    stats::fuchsia_non_numeric_counters);
                break;
            }
            if (is_valid_value) {
              StringId counter_name_id = context_->storage->InternString(
                  base::StringView(counter_name_str));
              TrackId track = context_->track_tracker->InternThreadCounterTrack(
                  counter_name_id, utid);
              context_->event_tracker->PushCounter(ts, counter_value, track);
            }
          }
          break;
        }
        case kDurationBegin: {
          UniqueTid utid =
              procs->UpdateThread(static_cast<uint32_t>(tinfo.tid),
                                  static_cast<uint32_t>(tinfo.pid));
          TrackId track_id = context_->track_tracker->InternThreadTrack(utid);
          slices->Begin(ts, track_id, cat, name);
          break;
        }
        case kDurationEnd: {
          UniqueTid utid =
              procs->UpdateThread(static_cast<uint32_t>(tinfo.tid),
                                  static_cast<uint32_t>(tinfo.pid));
          TrackId track_id = context_->track_tracker->InternThreadTrack(utid);
          // TODO(b/131181693): |cat| and |name| are not passed here so that
          // if two slices end at the same timestep, the slices get closed in
          // the correct order regardless of which end event is processed first.
          slices->End(ts, track_id);
          break;
        }
        case kDurationComplete: {
          int64_t end_ts;
          if (!cursor.ReadTimestamp(record->get_ticks_per_second(), &end_ts)) {
            context_->storage->IncrementStats(stats::fuchsia_invalid_event);
            return;
          }
          int64_t duration = end_ts - ts;
          if (duration < 0) {
            context_->storage->IncrementStats(stats::fuchsia_invalid_event);
            return;
          }
          UniqueTid utid =
              procs->UpdateThread(static_cast<uint32_t>(tinfo.tid),
                                  static_cast<uint32_t>(tinfo.pid));
          TrackId track_id = context_->track_tracker->InternThreadTrack(utid);
          slices->Scoped(ts, track_id, cat, name, duration);
          break;
        }
        case kAsyncBegin: {
          int64_t correlation_id;
          if (!cursor.ReadInt64(&correlation_id)) {
            context_->storage->IncrementStats(stats::fuchsia_invalid_event);
            return;
          }
          TrackId track_id = context_->track_tracker->InternFuchsiaAsyncTrack(
              name, correlation_id);
          slices->Begin(ts, track_id, cat, name);
          break;
        }
        case kAsyncInstant: {
          // TODO(eseckler): Consider storing these instants as 0-duration
          // slices instead, so that they get nested underneath begin/end
          // slices.
          int64_t correlation_id;
          if (!cursor.ReadInt64(&correlation_id)) {
            context_->storage->IncrementStats(stats::fuchsia_invalid_event);
            return;
          }
          TrackId track_id = context_->track_tracker->InternFuchsiaAsyncTrack(
              name, correlation_id);
          InstantId id = context_->event_tracker->PushInstant(
              ts, name, track_id.value, RefType::kRefTrack);
          auto inserter = context_->args_tracker->AddArgsTo(id);
          for (const Arg& arg : args) {
            inserter.AddArg(
                arg.name, arg.name,
                arg.value.ToStorageVariadic(context_->storage.get()));
          }
          context_->args_tracker->Flush();
          break;
        }
        case kAsyncEnd: {
          int64_t correlation_id;
          if (!cursor.ReadInt64(&correlation_id)) {
            context_->storage->IncrementStats(stats::fuchsia_invalid_event);
            return;
          }
          TrackId track_id = context_->track_tracker->InternFuchsiaAsyncTrack(
              name, correlation_id);
          slices->End(ts, track_id, cat, name);
          break;
        }
      }
      break;
    }
    default: {
      PERFETTO_DFATAL("Unknown record type %d in FuchsiaTraceParser",
                      record_type);
      break;
    }
  }
}

}  // namespace trace_processor
}  // namespace perfetto
