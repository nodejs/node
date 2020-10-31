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

#include "src/trace_processor/importers/systrace/systrace_line_parser.h"

#include "perfetto/ext/base/string_splitter.h"
#include "perfetto/ext/base/string_utils.h"
#include "src/trace_processor/importers/common/args_tracker.h"
#include "src/trace_processor/importers/common/event_tracker.h"
#include "src/trace_processor/importers/common/process_tracker.h"
#include "src/trace_processor/importers/common/slice_tracker.h"
#include "src/trace_processor/importers/common/track_tracker.h"
#include "src/trace_processor/importers/ftrace/binder_tracker.h"
#include "src/trace_processor/importers/ftrace/sched_event_tracker.h"
#include "src/trace_processor/importers/systrace/systrace_parser.h"
#include "src/trace_processor/types/task_state.h"

#include <inttypes.h>
#include <cctype>
#include <string>
#include <unordered_map>

namespace perfetto {
namespace trace_processor {

SystraceLineParser::SystraceLineParser(TraceProcessorContext* ctx)
    : context_(ctx),
      sched_wakeup_name_id_(ctx->storage->InternString("sched_wakeup")),
      cpuidle_name_id_(ctx->storage->InternString("cpuidle")) {}

util::Status SystraceLineParser::ParseLine(const SystraceLine& line) {
  context_->process_tracker->GetOrCreateThread(line.pid);
  context_->process_tracker->UpdateThreadName(
      line.pid, context_->storage->InternString(base::StringView(line.task)));

  if (!line.tgid_str.empty() && line.tgid_str != "-----") {
    base::Optional<uint32_t> tgid = base::StringToUInt32(line.tgid_str);
    if (tgid) {
      context_->process_tracker->UpdateThread(line.pid, tgid.value());
    }
  }

  std::unordered_map<std::string, std::string> args;
  for (base::StringSplitter ss(line.args_str, ' '); ss.Next();) {
    std::string key;
    std::string value;
    for (base::StringSplitter inner(ss.cur_token(), '='); inner.Next();) {
      if (key.empty()) {
        key = inner.cur_token();
      } else {
        value = inner.cur_token();
      }
    }
    args.emplace(std::move(key), std::move(value));
  }
  if (line.event_name == "sched_switch") {
    auto prev_state_str = args["prev_state"];
    int64_t prev_state =
        ftrace_utils::TaskState(prev_state_str.c_str()).raw_state();

    auto prev_pid = base::StringToUInt32(args["prev_pid"]);
    auto prev_comm = base::StringView(args["prev_comm"]);
    auto prev_prio = base::StringToInt32(args["prev_prio"]);
    auto next_pid = base::StringToUInt32(args["next_pid"]);
    auto next_comm = base::StringView(args["next_comm"]);
    auto next_prio = base::StringToInt32(args["next_prio"]);

    if (!(prev_pid.has_value() && prev_prio.has_value() &&
          next_pid.has_value() && next_prio.has_value())) {
      return util::Status("Could not parse sched_switch");
    }

    SchedEventTracker::GetOrCreate(context_)->PushSchedSwitch(
        line.cpu, line.ts, prev_pid.value(), prev_comm, prev_prio.value(),
        prev_state, next_pid.value(), next_comm, next_prio.value());
  } else if (line.event_name == "tracing_mark_write" ||
             line.event_name == "0" || line.event_name == "print") {
    SystraceParser::GetOrCreate(context_)->ParsePrintEvent(
        line.ts, line.pid, line.args_str.c_str());
  } else if (line.event_name == "sched_wakeup") {
    auto comm = args["comm"];
    base::Optional<uint32_t> wakee_pid = base::StringToUInt32(args["pid"]);
    if (!wakee_pid.has_value()) {
      return util::Status("Could not convert wakee_pid");
    }

    StringId name_id = context_->storage->InternString(base::StringView(comm));
    auto wakee_utid =
        context_->process_tracker->UpdateThreadName(wakee_pid.value(), name_id);
    context_->event_tracker->PushInstant(line.ts, sched_wakeup_name_id_,
                                         wakee_utid, RefType::kRefUtid);
  } else if (line.event_name == "cpu_idle") {
    base::Optional<uint32_t> event_cpu = base::StringToUInt32(args["cpu_id"]);
    base::Optional<double> new_state = base::StringToDouble(args["state"]);
    if (!event_cpu.has_value()) {
      return util::Status("Could not convert event cpu");
    }
    if (!event_cpu.has_value()) {
      return util::Status("Could not convert state");
    }

    TrackId track = context_->track_tracker->InternCpuCounterTrack(
        cpuidle_name_id_, event_cpu.value());
    context_->event_tracker->PushCounter(line.ts, new_state.value(), track);
  } else if (line.event_name == "binder_transaction") {
    auto id = base::StringToInt32(args["transaction"]);
    auto dest_node = base::StringToInt32(args["dest_node"]);
    auto dest_tgid = base::StringToInt32(args["dest_proc"]);
    auto dest_tid = base::StringToInt32(args["dest_thread"]);
    auto is_reply = base::StringToInt32(args["reply"]).value() == 1;
    auto flags_str = args["flags"];
    char* end;
    uint32_t flags = static_cast<uint32_t>(strtol(flags_str.c_str(), &end, 16));
    std::string code_str = args["code"] + " Java Layer Dependent";
    StringId code = context_->storage->InternString(base::StringView(code_str));
    if (!dest_tgid.has_value()) {
      return util::Status("Could not convert dest_tgid");
    }
    if (!dest_tid.has_value()) {
      return util::Status("Could not convert dest_tid");
    }
    if (!id.has_value()) {
      return util::Status("Could not convert transaction id");
    }
    if (!dest_node.has_value()) {
      return util::Status("Could not covert dest node");
    }
    BinderTracker::GetOrCreate(context_)->Transaction(
        line.ts, line.pid, id.value(), dest_node.value(), dest_tgid.value(),
        dest_tid.value(), is_reply, flags, code);
  } else if (line.event_name == "binder_transaction_received") {
    auto id = base::StringToInt32(args["transaction"]);
    if (!id.has_value()) {
      return util::Status("Could not convert transaction id");
    }
    BinderTracker::GetOrCreate(context_)->TransactionReceived(line.ts, line.pid,
                                                              id.value());
  } else if (line.event_name == "binder_lock") {
    BinderTracker::GetOrCreate(context_)->Lock(line.ts, line.pid);
  } else if (line.event_name == "binder_locked") {
    BinderTracker::GetOrCreate(context_)->Locked(line.ts, line.pid);
  } else if (line.event_name == "binder_unlock") {
    BinderTracker::GetOrCreate(context_)->Unlock(line.ts, line.pid);
  } else if (line.event_name == "binder_transaction_alloc_buf") {
    auto data_size = base::StringToUInt64(args["data_size"]);
    auto offsets_size = base::StringToUInt64(args["offsets_size"]);
    if (!data_size.has_value()) {
      return util::Status("Could not convert data size");
    }
    if (!offsets_size.has_value()) {
      return util::Status("Could not convert offsets size");
    }
    BinderTracker::GetOrCreate(context_)->TransactionAllocBuf(
        line.ts, line.pid, data_size.value(), offsets_size.value());
  }

  return util::OkStatus();
}

}  // namespace trace_processor
}  // namespace perfetto
