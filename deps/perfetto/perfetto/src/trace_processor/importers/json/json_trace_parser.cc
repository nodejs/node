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

#include "src/trace_processor/importers/json/json_trace_parser.h"

#include <inttypes.h>

#include <limits>
#include <string>

#include "perfetto/base/logging.h"
#include "perfetto/ext/base/string_view.h"
#include "perfetto/ext/base/utils.h"
#include "src/trace_processor/importers/common/process_tracker.h"
#include "src/trace_processor/importers/common/slice_tracker.h"
#include "src/trace_processor/importers/common/track_tracker.h"
#include "src/trace_processor/importers/json/json_tracker.h"
#include "src/trace_processor/importers/json/json_utils.h"
#include "src/trace_processor/types/trace_processor_context.h"

namespace perfetto {
namespace trace_processor {

JsonTraceParser::JsonTraceParser(TraceProcessorContext* context)
    : context_(context), systrace_line_parser_(context) {}

JsonTraceParser::~JsonTraceParser() = default;

void JsonTraceParser::ParseFtracePacket(uint32_t,
                                        int64_t,
                                        TimestampedTracePiece) {
  PERFETTO_FATAL("Json Trace Parser cannot handle ftrace packets.");
}

void JsonTraceParser::ParseTracePacket(int64_t timestamp,
                                       TimestampedTracePiece ttp) {
  PERFETTO_DCHECK(json::IsJsonSupported());

#if PERFETTO_BUILDFLAG(PERFETTO_TP_JSON)
  PERFETTO_DCHECK(ttp.type == TimestampedTracePiece::Type::kJsonValue ||
                  ttp.type == TimestampedTracePiece::Type::kSystraceLine);
  if (ttp.type == TimestampedTracePiece::Type::kSystraceLine) {
    systrace_line_parser_.ParseLine(*ttp.systrace_line);
    return;
  }

  const Json::Value& value = *(ttp.json_value);

  ProcessTracker* procs = context_->process_tracker.get();
  TraceStorage* storage = context_->storage.get();
  SliceTracker* slice_tracker = context_->slice_tracker.get();

  auto& ph = value["ph"];
  if (!ph.isString())
    return;
  char phase = *ph.asCString();

  base::Optional<uint32_t> opt_pid;
  base::Optional<uint32_t> opt_tid;

  if (value.isMember("pid"))
    opt_pid = json::CoerceToUint32(value["pid"]);
  if (value.isMember("tid"))
    opt_tid = json::CoerceToUint32(value["tid"]);

  uint32_t pid = opt_pid.value_or(0);
  uint32_t tid = opt_tid.value_or(pid);

  base::StringView cat = value.isMember("cat")
                             ? base::StringView(value["cat"].asCString())
                             : base::StringView();
  base::StringView name = value.isMember("name")
                              ? base::StringView(value["name"].asCString())
                              : base::StringView();

  StringId cat_id = storage->InternString(cat);
  StringId name_id = storage->InternString(name);
  UniqueTid utid = procs->UpdateThread(tid, pid);

  auto args_inserter = [this, &value](ArgsTracker::BoundInserter* inserter) {
    if (value.isMember("args")) {
      json::AddJsonValueToArgs(value["args"], /* flat_key = */ "args",
                               /* key = */ "args", context_->storage.get(),
                               inserter);
    }
  };
  switch (phase) {
    case 'B': {  // TRACE_EVENT_BEGIN.
      TrackId track_id = context_->track_tracker->InternThreadTrack(utid);
      slice_tracker->Begin(timestamp, track_id, cat_id, name_id, args_inserter);
      break;
    }
    case 'E': {  // TRACE_EVENT_END.
      TrackId track_id = context_->track_tracker->InternThreadTrack(utid);
      slice_tracker->End(timestamp, track_id, cat_id, name_id, args_inserter);
      break;
    }
    case 'X': {  // TRACE_EVENT (scoped event).
      base::Optional<int64_t> opt_dur =
          JsonTracker::GetOrCreate(context_)->CoerceToTs(value["dur"]);
      if (!opt_dur.has_value())
        return;
      TrackId track_id = context_->track_tracker->InternThreadTrack(utid);
      slice_tracker->Scoped(timestamp, track_id, cat_id, name_id,
                            opt_dur.value(), args_inserter);
      break;
    }
    case 'M': {  // Metadata events (process and thread names).
      if (strcmp(value["name"].asCString(), "thread_name") == 0 &&
          !value["args"]["name"].empty()) {
        const char* thread_name = value["args"]["name"].asCString();
        auto thread_name_id = context_->storage->InternString(thread_name);
        procs->UpdateThreadName(tid, thread_name_id);
        break;
      }
      if (strcmp(value["name"].asCString(), "process_name") == 0 &&
          !value["args"]["name"].empty()) {
        const char* proc_name = value["args"]["name"].asCString();
        procs->SetProcessMetadata(pid, base::nullopt, proc_name);
        break;
      }
    }
  }
#else
  perfetto::base::ignore_result(timestamp);
  perfetto::base::ignore_result(ttp);
  perfetto::base::ignore_result(context_);
  PERFETTO_ELOG("Cannot parse JSON trace due to missing JSON support");
#endif  // PERFETTO_BUILDFLAG(PERFETTO_TP_JSON)
}

}  // namespace trace_processor
}  // namespace perfetto

