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

#include "perfetto/ext/trace_processor/export_json.h"
#include "src/trace_processor/export_json.h"

#include <inttypes.h>
#include <stdio.h>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <deque>
#include <limits>

#include "perfetto/base/build_config.h"
#include "perfetto/ext/base/string_splitter.h"
#include "perfetto/ext/base/string_utils.h"
#include "src/trace_processor/importers/json/json_utils.h"
#include "src/trace_processor/storage/metadata.h"
#include "src/trace_processor/storage/trace_storage.h"
#include "src/trace_processor/trace_processor_storage_impl.h"
#include "src/trace_processor/types/trace_processor_context.h"

#if PERFETTO_BUILDFLAG(PERFETTO_TP_JSON)
#include <json/reader.h>
#include <json/writer.h>
#endif

namespace perfetto {
namespace trace_processor {
namespace json {

namespace {

class FileWriter : public OutputWriter {
 public:
  FileWriter(FILE* file) : file_(file) {}
  ~FileWriter() override { fflush(file_); }

  util::Status AppendString(const std::string& s) override {
    size_t written =
        fwrite(s.data(), sizeof(std::string::value_type), s.size(), file_);
    if (written != s.size())
      return util::ErrStatus("Error writing to file: %d", ferror(file_));
    return util::OkStatus();
  }

 private:
  FILE* file_;
};

#if PERFETTO_BUILDFLAG(PERFETTO_TP_JSON)
using IndexMap = perfetto::trace_processor::TraceStorage::Stats::IndexMap;

const char kLegacyEventArgsKey[] = "legacy_event";
const char kLegacyEventPassthroughUtidKey[] = "passthrough_utid";
const char kLegacyEventCategoryKey[] = "category";
const char kLegacyEventNameKey[] = "name";
const char kLegacyEventPhaseKey[] = "phase";
const char kLegacyEventDurationNsKey[] = "duration_ns";
const char kLegacyEventThreadTimestampNsKey[] = "thread_timestamp_ns";
const char kLegacyEventThreadDurationNsKey[] = "thread_duration_ns";
const char kLegacyEventThreadInstructionCountKey[] = "thread_instruction_count";
const char kLegacyEventThreadInstructionDeltaKey[] = "thread_instruction_delta";
const char kLegacyEventUseAsyncTtsKey[] = "use_async_tts";
const char kLegacyEventUnscopedIdKey[] = "unscoped_id";
const char kLegacyEventGlobalIdKey[] = "global_id";
const char kLegacyEventLocalIdKey[] = "local_id";
const char kLegacyEventIdScopeKey[] = "id_scope";
const char kLegacyEventBindIdKey[] = "bind_id";
const char kLegacyEventBindToEnclosingKey[] = "bind_to_enclosing";
const char kLegacyEventFlowDirectionKey[] = "flow_direction";
const char kFlowDirectionValueIn[] = "in";
const char kFlowDirectionValueOut[] = "out";
const char kFlowDirectionValueInout[] = "inout";
const char kStrippedArgument[] = "__stripped__";

const char* GetNonNullString(const TraceStorage* storage, StringId id) {
  return id == kNullStringId ? "" : storage->GetString(id).c_str();
}

std::string PrintUint64(uint64_t x) {
  char hex_str[19];
  sprintf(hex_str, "0x%" PRIx64, x);
  return hex_str;
}

void ConvertLegacyFlowEventArgs(const Json::Value& legacy_args,
                                Json::Value* event) {
  if (legacy_args.isMember(kLegacyEventBindIdKey)) {
    (*event)["bind_id"] =
        PrintUint64(legacy_args[kLegacyEventBindIdKey].asUInt64());
  }

  if (legacy_args.isMember(kLegacyEventBindToEnclosingKey))
    (*event)["bp"] = "e";

  if (legacy_args.isMember(kLegacyEventFlowDirectionKey)) {
    const char* val = legacy_args[kLegacyEventFlowDirectionKey].asCString();
    if (strcmp(val, kFlowDirectionValueIn) == 0) {
      (*event)["flow_in"] = true;
    } else if (strcmp(val, kFlowDirectionValueOut) == 0) {
      (*event)["flow_out"] = true;
    } else {
      PERFETTO_DCHECK(strcmp(val, kFlowDirectionValueInout) == 0);
      (*event)["flow_in"] = true;
      (*event)["flow_out"] = true;
    }
  }
}

class JsonExporter {
 public:
  JsonExporter(const TraceStorage* storage,
               OutputWriter* output,
               ArgumentFilterPredicate argument_filter,
               MetadataFilterPredicate metadata_filter,
               LabelFilterPredicate label_filter)
      : storage_(storage),
        args_builder_(storage_),
        writer_(output, argument_filter, metadata_filter, label_filter) {}

  util::Status Export() {
    util::Status status = MapUniquePidsAndTids();
    if (!status.ok())
      return status;

    status = ExportThreadNames();
    if (!status.ok())
      return status;

    status = ExportProcessNames();
    if (!status.ok())
      return status;

    status = ExportSlices();
    if (!status.ok())
      return status;

    status = ExportRawEvents();
    if (!status.ok())
      return status;

    status = ExportCpuProfileSamples();
    if (!status.ok())
      return status;

    status = ExportMetadata();
    if (!status.ok())
      return status;

    status = ExportStats();
    if (!status.ok())
      return status;

    return util::OkStatus();
  }

 private:
  class TraceFormatWriter {
   public:
    TraceFormatWriter(OutputWriter* output,
                      ArgumentFilterPredicate argument_filter,
                      MetadataFilterPredicate metadata_filter,
                      LabelFilterPredicate label_filter)
        : output_(output),
          argument_filter_(argument_filter),
          metadata_filter_(metadata_filter),
          label_filter_(label_filter),
          first_event_(true) {
      WriteHeader();
    }

    ~TraceFormatWriter() { WriteFooter(); }

    void WriteCommonEvent(const Json::Value& event) {
      if (label_filter_ && !label_filter_("traceEvents"))
        return;

      DoWriteEvent(event);
    }

    void AddAsyncBeginEvent(const Json::Value& event) {
      if (label_filter_ && !label_filter_("traceEvents"))
        return;

      async_begin_events_.push_back(event);
    }

    void AddAsyncInstantEvent(const Json::Value& event) {
      if (label_filter_ && !label_filter_("traceEvents"))
        return;

      async_instant_events_.push_back(event);
    }

    void AddAsyncEndEvent(const Json::Value& event) {
      if (label_filter_ && !label_filter_("traceEvents"))
        return;

      async_end_events_.push_back(event);
    }

    void SortAndEmitAsyncEvents() {
      // Catapult doesn't handle out-of-order begin/end events well, especially
      // when their timestamps are the same, but their order is incorrect. Since
      // we process events sorted by begin timestamp, |async_begin_events_| and
      // |async_instant_events_| are already sorted. We now only have to sort
      // |async_end_events_| and merge-sort all events into a single sequence.

      // Sort |async_end_events_|. Note that we should order by ascending
      // timestamp, but in reverse-stable order. This way, a child slices's end
      // is emitted before its parent's end event, even if both end events have
      // the same timestamp. To accomplish this, we perform a stable sort in
      // descending order and later iterate via reverse iterators.
      struct {
        bool operator()(const Json::Value& a, const Json::Value& b) const {
          return a["ts"].asInt64() > b["ts"].asInt64();
        }
      } CompareEvents;
      std::stable_sort(async_end_events_.begin(), async_end_events_.end(),
                       CompareEvents);

      // Merge sort by timestamp. If events share the same timestamp, prefer
      // instant events, then end events, so that old slices close before new
      // ones are opened, but instant events remain in their deepest nesting
      // level.
      auto instant_event_it = async_instant_events_.begin();
      auto end_event_it = async_end_events_.rbegin();
      auto begin_event_it = async_begin_events_.begin();

      auto has_instant_event = instant_event_it != async_instant_events_.end();
      auto has_end_event = end_event_it != async_end_events_.rend();
      auto has_begin_event = begin_event_it != async_begin_events_.end();

      auto emit_next_instant = [&instant_event_it, &has_instant_event, this]() {
        DoWriteEvent(*instant_event_it);
        instant_event_it++;
        has_instant_event = instant_event_it != async_instant_events_.end();
      };
      auto emit_next_end = [&end_event_it, &has_end_event, this]() {
        DoWriteEvent(*end_event_it);
        end_event_it++;
        has_end_event = end_event_it != async_end_events_.rend();
      };
      auto emit_next_begin = [&begin_event_it, &has_begin_event, this]() {
        DoWriteEvent(*begin_event_it);
        begin_event_it++;
        has_begin_event = begin_event_it != async_begin_events_.end();
      };

      auto emit_next_instant_or_end = [&instant_event_it, &end_event_it,
                                       &emit_next_instant, &emit_next_end]() {
        if ((*instant_event_it)["ts"].asInt64() <=
            (*end_event_it)["ts"].asInt64()) {
          emit_next_instant();
        } else {
          emit_next_end();
        }
      };
      auto emit_next_instant_or_begin = [&instant_event_it, &begin_event_it,
                                         &emit_next_instant,
                                         &emit_next_begin]() {
        if ((*instant_event_it)["ts"].asInt64() <=
            (*begin_event_it)["ts"].asInt64()) {
          emit_next_instant();
        } else {
          emit_next_begin();
        }
      };
      auto emit_next_end_or_begin = [&end_event_it, &begin_event_it,
                                     &emit_next_end, &emit_next_begin]() {
        if ((*end_event_it)["ts"].asInt64() <=
            (*begin_event_it)["ts"].asInt64()) {
          emit_next_end();
        } else {
          emit_next_begin();
        }
      };

      // While we still have events in all iterators, consider each.
      while (has_instant_event && has_end_event && has_begin_event) {
        if ((*instant_event_it)["ts"].asInt64() <=
            (*end_event_it)["ts"].asInt64()) {
          emit_next_instant_or_begin();
        } else {
          emit_next_end_or_begin();
        }
      }

      // Only instant and end events left.
      while (has_instant_event && has_end_event) {
        emit_next_instant_or_end();
      }

      // Only instant and begin events left.
      while (has_instant_event && has_begin_event) {
        emit_next_instant_or_begin();
      }

      // Only end and begin events left.
      while (has_end_event && has_begin_event) {
        emit_next_end_or_begin();
      }

      // Remaining instant events.
      while (has_instant_event) {
        emit_next_instant();
      }

      // Remaining end events.
      while (has_end_event) {
        emit_next_end();
      }

      // Remaining begin events.
      while (has_begin_event) {
        emit_next_begin();
      }
    }

    void WriteMetadataEvent(const char* metadata_type,
                            const char* metadata_value,
                            uint32_t pid,
                            uint32_t tid) {
      if (label_filter_ && !label_filter_("traceEvents"))
        return;

      if (!first_event_)
        output_->AppendString(",\n");

      Json::FastWriter writer;
      writer.omitEndingLineFeed();
      Json::Value value;
      value["ph"] = "M";
      value["cat"] = "__metadata";
      value["ts"] = 0;
      value["name"] = metadata_type;
      value["pid"] = Json::Int(pid);
      value["tid"] = Json::Int(tid);

      Json::Value args;
      args["name"] = metadata_value;
      value["args"] = args;

      output_->AppendString(writer.write(value));
      first_event_ = false;
    }

    void MergeMetadata(const Json::Value& value) {
      for (const auto& member : value.getMemberNames()) {
        metadata_[member] = value[member];
      }
    }

    void AppendTelemetryMetadataString(const char* key, const char* value) {
      metadata_["telemetry"][key].append(value);
    }

    void AppendTelemetryMetadataInt(const char* key, int64_t value) {
      metadata_["telemetry"][key].append(Json::Int64(value));
    }

    void AppendTelemetryMetadataBool(const char* key, bool value) {
      metadata_["telemetry"][key].append(value);
    }

    void SetTelemetryMetadataTimestamp(const char* key, int64_t value) {
      metadata_["telemetry"][key] = value / 1000.0;
    }

    void SetStats(const char* key, int64_t value) {
      metadata_["trace_processor_stats"][key] = Json::Int64(value);
    }

    void SetStats(const char* key, const IndexMap& indexed_values) {
      constexpr const char* kBufferStatsPrefix = "traced_buf_";

      // Stats for the same buffer should be grouped together in the JSON.
      if (strncmp(kBufferStatsPrefix, key, strlen(kBufferStatsPrefix)) == 0) {
        for (const auto& value : indexed_values) {
          metadata_["trace_processor_stats"]["traced_buf"][value.first]
                   [key + strlen(kBufferStatsPrefix)] =
                       Json::Int64(value.second);
        }
        return;
      }

      // Other indexed value stats are exported as array under their key.
      for (const auto& value : indexed_values) {
        metadata_["trace_processor_stats"][key][value.first] =
            Json::Int64(value.second);
      }
    }

    void AddSystemTraceData(const std::string& data) {
      system_trace_data_ += data;
    }

    void AddUserTraceData(const std::string& data) {
      if (user_trace_data_.empty())
        user_trace_data_ = "[";
      user_trace_data_ += data;
    }

   private:
    void WriteHeader() {
      if (!label_filter_)
        output_->AppendString("{\"traceEvents\":[\n");
    }

    void WriteFooter() {
      SortAndEmitAsyncEvents();

      // Filter metadata entries.
      if (metadata_filter_) {
        for (const auto& member : metadata_.getMemberNames()) {
          if (!metadata_filter_(member.c_str()))
            metadata_[member] = kStrippedArgument;
        }
      }

      Json::FastWriter writer;
      writer.omitEndingLineFeed();
      if ((!label_filter_ || label_filter_("traceEvents")) &&
          !user_trace_data_.empty()) {
        user_trace_data_ += "]";
        Json::Reader reader;
        Json::Value result;
        if (reader.parse(user_trace_data_, result)) {
          for (const auto& event : result) {
            WriteCommonEvent(event);
          }
        } else {
          PERFETTO_DLOG(
              "can't parse legacy user json trace export, skipping. data: %s",
              user_trace_data_.c_str());
        }
      }
      if (!label_filter_)
        output_->AppendString("]");
      if ((!label_filter_ || label_filter_("systemTraceEvents")) &&
          !system_trace_data_.empty()) {
        output_->AppendString(",\"systemTraceEvents\":\n");
        output_->AppendString(writer.write(Json::Value(system_trace_data_)));
      }
      if ((!label_filter_ || label_filter_("metadata")) && !metadata_.empty()) {
        output_->AppendString(",\"metadata\":\n");
        output_->AppendString(writer.write(metadata_));
      }
      if (!label_filter_)
        output_->AppendString("}");
    }

    void DoWriteEvent(const Json::Value& event) {
      if (!first_event_)
        output_->AppendString(",\n");

      Json::FastWriter writer;
      writer.omitEndingLineFeed();

      ArgumentNameFilterPredicate argument_name_filter;
      bool strip_args =
          argument_filter_ &&
          !argument_filter_(event["cat"].asCString(), event["name"].asCString(),
                            &argument_name_filter);
      if ((strip_args || argument_name_filter) && event.isMember("args")) {
        Json::Value event_copy = event;
        if (strip_args) {
          event_copy["args"] = kStrippedArgument;
        } else {
          auto& args = event_copy["args"];
          for (const auto& member : event["args"].getMemberNames()) {
            if (!argument_name_filter(member.c_str()))
              args[member] = kStrippedArgument;
          }
        }
        output_->AppendString(writer.write(event_copy));
      } else {
        output_->AppendString(writer.write(event));
      }
      first_event_ = false;
    }

    OutputWriter* output_;
    ArgumentFilterPredicate argument_filter_;
    MetadataFilterPredicate metadata_filter_;
    LabelFilterPredicate label_filter_;

    bool first_event_;
    Json::Value metadata_;
    std::string system_trace_data_;
    std::string user_trace_data_;
    std::vector<Json::Value> async_begin_events_;
    std::vector<Json::Value> async_instant_events_;
    std::vector<Json::Value> async_end_events_;
  };

  class ArgsBuilder {
   public:
    explicit ArgsBuilder(const TraceStorage* storage)
        : storage_(storage),
          empty_value_(Json::objectValue),
          nan_value_(Json::StaticString("NaN")),
          inf_value_(Json::StaticString("Infinity")),
          neg_inf_value_(Json::StaticString("-Infinity")) {
      const auto& arg_table = storage_->arg_table();
      uint32_t count = arg_table.row_count();
      if (count == 0) {
        args_sets_.resize(1, empty_value_);
        return;
      }
      args_sets_.resize(arg_table.arg_set_id()[count - 1] + 1, empty_value_);

      for (uint32_t i = 0; i < count; ++i) {
        ArgSetId set_id = arg_table.arg_set_id()[i];
        const char* key = arg_table.key().GetString(i).c_str();
        Variadic value = storage_->GetArgValue(i);
        AppendArg(set_id, key, VariadicToJson(value));
      }
      PostprocessArgs();
    }

    const Json::Value& GetArgs(ArgSetId set_id) const {
      // If |set_id| was empty and added to the storage last, it may not be in
      // args_sets_.
      if (set_id > args_sets_.size())
        return empty_value_;
      return args_sets_[set_id];
    }

   private:
    Json::Value VariadicToJson(Variadic variadic) {
      switch (variadic.type) {
        case Variadic::kInt:
          return Json::Int64(variadic.int_value);
        case Variadic::kUint:
          return Json::UInt64(variadic.uint_value);
        case Variadic::kString:
          return GetNonNullString(storage_, variadic.string_value);
        case Variadic::kReal:
          if (std::isnan(variadic.real_value)) {
            return nan_value_;
          } else if (std::isinf(variadic.real_value) &&
                     variadic.real_value > 0) {
            return inf_value_;
          } else if (std::isinf(variadic.real_value) &&
                     variadic.real_value < 0) {
            return neg_inf_value_;
          } else {
            return variadic.real_value;
          }
        case Variadic::kPointer:
          return PrintUint64(variadic.pointer_value);
        case Variadic::kBool:
          return variadic.bool_value;
        case Variadic::kJson:
          Json::Reader reader;
          Json::Value result;
          reader.parse(GetNonNullString(storage_, variadic.json_value), result);
          return result;
      }
      PERFETTO_FATAL("Not reached");  // For gcc.
    }

    void AppendArg(ArgSetId set_id,
                   const std::string& key,
                   const Json::Value& value) {
      Json::Value* target = &args_sets_[set_id];
      for (base::StringSplitter parts(key, '.'); parts.Next();) {
        if (PERFETTO_UNLIKELY(!target->isNull() && !target->isObject())) {
          PERFETTO_DLOG("Malformed arguments. Can't append %s to %s.",
                        key.c_str(),
                        args_sets_[set_id].toStyledString().c_str());
          return;
        }
        std::string key_part = parts.cur_token();
        size_t bracketpos = key_part.find('[');
        if (bracketpos == key_part.npos) {  // A single item
          target = &(*target)[key_part];
        } else {  // A list item
          target = &(*target)[key_part.substr(0, bracketpos)];
          while (bracketpos != key_part.npos) {
            // We constructed this string from an int earlier in trace_processor
            // so it shouldn't be possible for this (or the StringToUInt32
            // below) to fail.
            std::string s =
                key_part.substr(bracketpos + 1, key_part.find(']', bracketpos) -
                                                    bracketpos - 1);
            if (PERFETTO_UNLIKELY(!target->isNull() && !target->isArray())) {
              PERFETTO_DLOG("Malformed arguments. Can't append %s to %s.",
                            key.c_str(),
                            args_sets_[set_id].toStyledString().c_str());
              return;
            }
            base::Optional<uint32_t> index = base::StringToUInt32(s);
            if (PERFETTO_UNLIKELY(!index)) {
              PERFETTO_ELOG("Expected to be able to extract index from %s",
                            key_part.c_str());
              return;
            }
            target = &(*target)[index.value()];
            bracketpos = key_part.find('[', bracketpos + 1);
          }
        }
      }
      *target = value;
    }

    void PostprocessArgs() {
      for (Json::Value& args : args_sets_) {
        // Move all fields from "debug" key to upper level.
        if (args.isMember("debug")) {
          Json::Value debug = args["debug"];
          args.removeMember("debug");
          for (const auto& member : debug.getMemberNames()) {
            args[member] = debug[member];
          }
        }

        // Rename source fields.
        if (args.isMember("task")) {
          if (args["task"].isMember("posted_from")) {
            Json::Value posted_from = args["task"]["posted_from"];
            args["task"].removeMember("posted_from");
            if (posted_from.isMember("function_name")) {
              args["src_func"] = posted_from["function_name"];
              args["src_file"] = posted_from["file_name"];
            } else if (posted_from.isMember("file_name")) {
              args["src"] = posted_from["file_name"];
            }
          }
          if (args["task"].empty())
            args.removeMember("task");
        }
      }
    }

    const TraceStorage* storage_;
    std::vector<Json::Value> args_sets_;
    const Json::Value empty_value_;
    const Json::Value nan_value_;
    const Json::Value inf_value_;
    const Json::Value neg_inf_value_;
  };

  util::Status MapUniquePidsAndTids() {
    const auto& process_table = storage_->process_table();
    for (UniquePid upid = 0; upid < process_table.row_count(); upid++) {
      uint32_t exported_pid = process_table.pid()[upid];
      auto it_and_inserted =
          exported_pids_to_upids_.emplace(exported_pid, upid);
      if (!it_and_inserted.second) {
        exported_pid = NextExportedPidOrTidForDuplicates();
        it_and_inserted = exported_pids_to_upids_.emplace(exported_pid, upid);
      }
      upids_to_exported_pids_.emplace(upid, exported_pid);
    }

    const auto& thread_table = storage_->thread_table();
    for (UniqueTid utid = 0; utid < thread_table.row_count(); utid++) {
      uint32_t exported_pid = 0;
      base::Optional<UniquePid> upid = thread_table.upid()[utid];
      if (upid) {
        auto exported_pid_it = upids_to_exported_pids_.find(*upid);
        PERFETTO_DCHECK(exported_pid_it != upids_to_exported_pids_.end());
        exported_pid = exported_pid_it->second;
      }

      uint32_t exported_tid = thread_table.tid()[utid];
      auto it_and_inserted = exported_pids_and_tids_to_utids_.emplace(
          std::make_pair(exported_pid, exported_tid), utid);
      if (!it_and_inserted.second) {
        exported_tid = NextExportedPidOrTidForDuplicates();
        it_and_inserted = exported_pids_and_tids_to_utids_.emplace(
            std::make_pair(exported_pid, exported_tid), utid);
      }
      utids_to_exported_pids_and_tids_.emplace(
          utid, std::make_pair(exported_pid, exported_tid));
    }

    return util::OkStatus();
  }

  util::Status ExportThreadNames() {
    const auto& thread_table = storage_->thread_table();
    for (UniqueTid utid = 0; utid < thread_table.row_count(); ++utid) {
      auto opt_name = thread_table.name()[utid];
      if (!opt_name.is_null()) {
        const char* thread_name = GetNonNullString(storage_, opt_name);
        auto pid_and_tid = UtidToPidAndTid(utid);
        writer_.WriteMetadataEvent("thread_name", thread_name,
                                   pid_and_tid.first, pid_and_tid.second);
      }
    }
    return util::OkStatus();
  }

  util::Status ExportProcessNames() {
    const auto& process_table = storage_->process_table();
    for (UniquePid upid = 0; upid < process_table.row_count(); ++upid) {
      auto opt_name = process_table.name()[upid];
      if (!opt_name.is_null()) {
        const char* process_name = GetNonNullString(storage_, opt_name);
        writer_.WriteMetadataEvent("process_name", process_name,
                                   UpidToPid(upid), /*tid=*/0);
      }
    }
    return util::OkStatus();
  }

  util::Status ExportSlices() {
    const auto& slices = storage_->slice_table();
    for (uint32_t i = 0; i < slices.row_count(); ++i) {
      // Skip slices with empty category - these are ftrace/system slices that
      // were also imported into the raw table and will be exported from there
      // by trace_to_text.
      // TODO(b/153609716): Add a src column or do_not_export flag instead.
      auto cat = slices.category().GetString(i);
      if (cat.c_str() == nullptr || cat == "binder")
        continue;

      Json::Value event;
      event["ts"] = Json::Int64(slices.ts()[i] / 1000);
      event["cat"] = GetNonNullString(storage_, slices.category()[i]);
      event["name"] = GetNonNullString(storage_, slices.name()[i]);
      event["pid"] = 0;
      event["tid"] = 0;

      base::Optional<UniqueTid> legacy_utid;

      event["args"] =
          args_builder_.GetArgs(slices.arg_set_id()[i]);  // Makes a copy.
      if (event["args"].isMember(kLegacyEventArgsKey)) {
        ConvertLegacyFlowEventArgs(event["args"][kLegacyEventArgsKey], &event);

        if (event["args"][kLegacyEventArgsKey].isMember(
                kLegacyEventPassthroughUtidKey)) {
          legacy_utid =
              event["args"][kLegacyEventArgsKey][kLegacyEventPassthroughUtidKey]
                  .asUInt();
        }

        event["args"].removeMember(kLegacyEventArgsKey);
      }

      // To prevent duplicate export of slices, only export slices on descriptor
      // or chrome tracks (i.e. TrackEvent slices). Slices on other tracks may
      // also be present as raw events and handled by trace_to_text. Only add
      // more track types here if they are not already covered by trace_to_text.
      TrackId track_id = slices.track_id()[i];

      const auto& track_table = storage_->track_table();

      uint32_t track_row = *track_table.id().IndexOf(track_id);
      auto track_args_id = track_table.source_arg_set_id()[track_row];
      const Json::Value* track_args = nullptr;
      bool legacy_chrome_track = false;
      bool is_child_track = false;
      if (track_args_id) {
        track_args = &args_builder_.GetArgs(*track_args_id);
        legacy_chrome_track = (*track_args)["source"].asString() == "chrome";
        is_child_track = track_args->isMember("parent_track_id");
      }

      const auto& thread_track = storage_->thread_track_table();
      const auto& process_track = storage_->process_track_table();
      const auto& thread_slices = storage_->thread_slices();
      const auto& virtual_track_slices = storage_->virtual_track_slices();

      int64_t duration_ns = slices.dur()[i];
      int64_t thread_ts_ns = 0;
      int64_t thread_duration_ns = 0;
      int64_t thread_instruction_count = 0;
      int64_t thread_instruction_delta = 0;

      base::Optional<uint32_t> thread_slice_row =
          thread_slices.FindRowForSliceId(i);
      if (thread_slice_row) {
        thread_ts_ns = thread_slices.thread_timestamp_ns()[*thread_slice_row];
        thread_duration_ns =
            thread_slices.thread_duration_ns()[*thread_slice_row];
        thread_instruction_count =
            thread_slices.thread_instruction_counts()[*thread_slice_row];
        thread_instruction_delta =
            thread_slices.thread_instruction_deltas()[*thread_slice_row];
      } else {
        base::Optional<uint32_t> vtrack_slice_row =
            virtual_track_slices.FindRowForSliceId(i);
        if (vtrack_slice_row) {
          thread_ts_ns =
              virtual_track_slices.thread_timestamp_ns()[*vtrack_slice_row];
          thread_duration_ns =
              virtual_track_slices.thread_duration_ns()[*vtrack_slice_row];
          thread_instruction_count =
              virtual_track_slices
                  .thread_instruction_counts()[*vtrack_slice_row];
          thread_instruction_delta =
              virtual_track_slices
                  .thread_instruction_deltas()[*vtrack_slice_row];
        }
      }

      auto opt_thread_track_row = thread_track.id().IndexOf(TrackId{track_id});

      if (opt_thread_track_row && !is_child_track) {
        // Synchronous (thread) slice or instant event.
        UniqueTid utid = thread_track.utid()[*opt_thread_track_row];
        auto pid_and_tid = UtidToPidAndTid(utid);
        event["pid"] = Json::Int(pid_and_tid.first);
        event["tid"] = Json::Int(pid_and_tid.second);

        if (duration_ns == 0) {
          // Use "I" instead of "i" phase for backwards-compat with old
          // consumers.
          event["ph"] = "I";
          if (thread_ts_ns > 0) {
            event["tts"] = Json::Int64(thread_ts_ns / 1000);
          }
          if (thread_instruction_count > 0) {
            event["ticount"] = Json::Int64(thread_instruction_count);
          }
          event["s"] = "t";
        } else {
          if (duration_ns > 0) {
            event["ph"] = "X";
            event["dur"] = Json::Int64(duration_ns / 1000);
          } else {
            // If the slice didn't finish, the duration may be negative. Only
            // write a begin event without end event in this case.
            event["ph"] = "B";
          }
          if (thread_ts_ns > 0) {
            event["tts"] = Json::Int64(thread_ts_ns / 1000);
            // Only write thread duration for completed events.
            if (duration_ns > 0)
              event["tdur"] = Json::Int64(thread_duration_ns / 1000);
          }
          if (thread_instruction_count > 0) {
            event["ticount"] = Json::Int64(thread_instruction_count);
            // Only write thread instruction delta for completed events.
            if (duration_ns > 0)
              event["tidelta"] = Json::Int64(thread_instruction_delta);
          }
        }
        writer_.WriteCommonEvent(event);
      } else if (is_child_track ||
                 (legacy_chrome_track && track_args->isMember("source_id"))) {
        // Async event slice.
        auto opt_process_row = process_track.id().IndexOf(TrackId{track_id});
        if (legacy_chrome_track) {
          // Legacy async tracks are always process-associated and have args.
          PERFETTO_DCHECK(opt_process_row);
          PERFETTO_DCHECK(track_args);
          uint32_t upid = process_track.upid()[*opt_process_row];
          uint32_t exported_pid = UpidToPid(upid);
          event["pid"] = Json::Int(exported_pid);
          event["tid"] =
              Json::Int(legacy_utid ? UtidToPidAndTid(*legacy_utid).second
                                    : exported_pid);

          // Preserve original event IDs for legacy tracks. This is so that e.g.
          // memory dump IDs show up correctly in the JSON trace.
          PERFETTO_DCHECK(track_args->isMember("source_id"));
          PERFETTO_DCHECK(track_args->isMember("source_id_is_process_scoped"));
          PERFETTO_DCHECK(track_args->isMember("source_scope"));
          uint64_t source_id =
              static_cast<uint64_t>((*track_args)["source_id"].asInt64());
          std::string source_scope = (*track_args)["source_scope"].asString();
          if (!source_scope.empty())
            event["scope"] = source_scope;
          bool source_id_is_process_scoped =
              (*track_args)["source_id_is_process_scoped"].asBool();
          if (source_id_is_process_scoped) {
            event["id2"]["local"] = PrintUint64(source_id);
          } else {
            // Some legacy importers don't understand "id2" fields, so we use
            // the "usually" global "id" field instead. This works as long as
            // the event phase is not in {'N', 'D', 'O', '(', ')'}, see
            // "LOCAL_ID_PHASES" in catapult.
            event["id"] = PrintUint64(source_id);
          }
        } else {
          if (opt_thread_track_row) {
            UniqueTid utid = thread_track.utid()[*opt_thread_track_row];
            auto pid_and_tid = UtidToPidAndTid(utid);
            event["pid"] = Json::Int(pid_and_tid.first);
            event["tid"] = Json::Int(pid_and_tid.second);
            event["id2"]["local"] = PrintUint64(track_id.value);
          } else if (opt_process_row) {
            uint32_t upid = process_track.upid()[*opt_process_row];
            uint32_t exported_pid = UpidToPid(upid);
            event["pid"] = Json::Int(exported_pid);
            event["tid"] =
                Json::Int(legacy_utid ? UtidToPidAndTid(*legacy_utid).second
                                      : exported_pid);
            event["id2"]["local"] = PrintUint64(track_id.value);
          } else {
            if (legacy_utid) {
              auto pid_and_tid = UtidToPidAndTid(*legacy_utid);
              event["pid"] = Json::Int(pid_and_tid.first);
              event["tid"] = Json::Int(pid_and_tid.second);
            }

            // Some legacy importers don't understand "id2" fields, so we use
            // the "usually" global "id" field instead. This works as long as
            // the event phase is not in {'N', 'D', 'O', '(', ')'}, see
            // "LOCAL_ID_PHASES" in catapult.
            event["id"] = PrintUint64(track_id.value);
          }
        }

        if (thread_ts_ns > 0) {
          event["tts"] = Json::Int64(thread_ts_ns / 1000);
          event["use_async_tts"] = Json::Int(1);
        }
        if (thread_instruction_count > 0) {
          event["ticount"] = Json::Int64(thread_instruction_count);
          event["use_async_tts"] = Json::Int(1);
        }

        if (duration_ns == 0) {  // Instant async event.
          event["ph"] = "n";
          writer_.AddAsyncInstantEvent(event);
        } else {  // Async start and end.
          event["ph"] = "b";
          writer_.AddAsyncBeginEvent(event);
          // If the slice didn't finish, the duration may be negative. Don't
          // write the end event in this case.
          if (duration_ns > 0) {
            event["ph"] = "e";
            event["ts"] = Json::Int64((slices.ts()[i] + duration_ns) / 1000);
            if (thread_ts_ns > 0) {
              event["tts"] =
                  Json::Int64((thread_ts_ns + thread_duration_ns) / 1000);
            }
            if (thread_instruction_count > 0) {
              event["ticount"] = Json::Int64(
                  (thread_instruction_count + thread_instruction_delta));
            }
            event["args"].clear();
            writer_.AddAsyncEndEvent(event);
          }
        }
      } else {
        // Global or process-scoped instant event.
        PERFETTO_DCHECK(legacy_chrome_track || !is_child_track);
        if (duration_ns != 0) {
          // We don't support exporting slices on the default global or process
          // track to JSON (JSON only supports instant events on these tracks).
          PERFETTO_DLOG(
              "skipping non-instant slice on global or process track");
        } else {
          // Use "I" instead of "i" phase for backwards-compat with old
          // consumers.
          event["ph"] = "I";

          auto opt_process_row = process_track.id().IndexOf(TrackId{track_id});
          if (opt_process_row.has_value()) {
            uint32_t upid = process_track.upid()[*opt_process_row];
            uint32_t exported_pid = UpidToPid(upid);
            event["pid"] = Json::Int(exported_pid);
            event["tid"] =
                Json::Int(legacy_utid ? UtidToPidAndTid(*legacy_utid).second
                                      : exported_pid);
            event["s"] = "p";
          } else {
            event["s"] = "g";
          }
          writer_.WriteCommonEvent(event);
        }
      }
    }
    return util::OkStatus();
  }

  Json::Value ConvertLegacyRawEventToJson(uint32_t index) {
    const auto& events = storage_->raw_table();

    Json::Value event;
    event["ts"] = Json::Int64(events.ts()[index] / 1000);

    UniqueTid utid = static_cast<UniqueTid>(events.utid()[index]);
    auto pid_and_tid = UtidToPidAndTid(utid);
    event["pid"] = Json::Int(pid_and_tid.first);
    event["tid"] = Json::Int(pid_and_tid.second);

    // Raw legacy events store all other params in the arg set. Make a copy of
    // the converted args here, parse, and then remove the legacy params.
    event["args"] = args_builder_.GetArgs(events.arg_set_id()[index]);
    const Json::Value& legacy_args = event["args"][kLegacyEventArgsKey];

    PERFETTO_DCHECK(legacy_args.isMember(kLegacyEventCategoryKey));
    event["cat"] = legacy_args[kLegacyEventCategoryKey];

    PERFETTO_DCHECK(legacy_args.isMember(kLegacyEventNameKey));
    event["name"] = legacy_args[kLegacyEventNameKey];

    PERFETTO_DCHECK(legacy_args.isMember(kLegacyEventPhaseKey));
    event["ph"] = legacy_args[kLegacyEventPhaseKey];

    // Object snapshot events are supposed to have a mandatory "snapshot" arg,
    // which may be removed in trace processor if it is empty.
    if (legacy_args[kLegacyEventPhaseKey] == "O" &&
        !event["args"].isMember("snapshot")) {
      event["args"]["snapshot"] = Json::Value(Json::objectValue);
    }

    if (legacy_args.isMember(kLegacyEventDurationNsKey))
      event["dur"] = legacy_args[kLegacyEventDurationNsKey].asInt64() / 1000;

    if (legacy_args.isMember(kLegacyEventThreadTimestampNsKey)) {
      event["tts"] =
          legacy_args[kLegacyEventThreadTimestampNsKey].asInt64() / 1000;
    }

    if (legacy_args.isMember(kLegacyEventThreadDurationNsKey)) {
      event["tdur"] =
          legacy_args[kLegacyEventThreadDurationNsKey].asInt64() / 1000;
    }

    if (legacy_args.isMember(kLegacyEventThreadInstructionCountKey))
      event["ticount"] = legacy_args[kLegacyEventThreadInstructionCountKey];

    if (legacy_args.isMember(kLegacyEventThreadInstructionDeltaKey))
      event["tidelta"] = legacy_args[kLegacyEventThreadInstructionDeltaKey];

    if (legacy_args.isMember(kLegacyEventUseAsyncTtsKey))
      event["use_async_tts"] = legacy_args[kLegacyEventUseAsyncTtsKey];

    if (legacy_args.isMember(kLegacyEventUnscopedIdKey)) {
      event["id"] =
          PrintUint64(legacy_args[kLegacyEventUnscopedIdKey].asUInt64());
    }

    if (legacy_args.isMember(kLegacyEventGlobalIdKey)) {
      event["id2"]["global"] =
          PrintUint64(legacy_args[kLegacyEventGlobalIdKey].asUInt64());
    }

    if (legacy_args.isMember(kLegacyEventLocalIdKey)) {
      event["id2"]["local"] =
          PrintUint64(legacy_args[kLegacyEventLocalIdKey].asUInt64());
    }

    if (legacy_args.isMember(kLegacyEventIdScopeKey))
      event["scope"] = legacy_args[kLegacyEventIdScopeKey];

    ConvertLegacyFlowEventArgs(legacy_args, &event);

    event["args"].removeMember(kLegacyEventArgsKey);

    return event;
  }

  util::Status ExportRawEvents() {
    base::Optional<StringId> raw_legacy_event_key_id =
        storage_->string_pool().GetId("track_event.legacy_event");
    base::Optional<StringId> raw_legacy_system_trace_event_id =
        storage_->string_pool().GetId("chrome_event.legacy_system_trace");
    base::Optional<StringId> raw_legacy_user_trace_event_id =
        storage_->string_pool().GetId("chrome_event.legacy_user_trace");
    base::Optional<StringId> raw_chrome_metadata_event_id =
        storage_->string_pool().GetId("chrome_event.metadata");

    const auto& events = storage_->raw_table();
    for (uint32_t i = 0; i < events.row_count(); ++i) {
      if (raw_legacy_event_key_id &&
          events.name()[i] == *raw_legacy_event_key_id) {
        Json::Value event = ConvertLegacyRawEventToJson(i);
        writer_.WriteCommonEvent(event);
      } else if (raw_legacy_system_trace_event_id &&
                 events.name()[i] == *raw_legacy_system_trace_event_id) {
        Json::Value args = args_builder_.GetArgs(events.arg_set_id()[i]);
        PERFETTO_DCHECK(args.isMember("data"));
        writer_.AddSystemTraceData(args["data"].asString());
      } else if (raw_legacy_user_trace_event_id &&
                 events.name()[i] == *raw_legacy_user_trace_event_id) {
        Json::Value args = args_builder_.GetArgs(events.arg_set_id()[i]);
        PERFETTO_DCHECK(args.isMember("data"));
        writer_.AddUserTraceData(args["data"].asString());
      } else if (raw_chrome_metadata_event_id &&
                 events.name()[i] == *raw_chrome_metadata_event_id) {
        Json::Value args = args_builder_.GetArgs(events.arg_set_id()[i]);
        writer_.MergeMetadata(args);
      }
    }
    return util::OkStatus();
  }

  util::Status ExportCpuProfileSamples() {
    const tables::CpuProfileStackSampleTable& samples =
        storage_->cpu_profile_stack_sample_table();
    for (uint32_t i = 0; i < samples.row_count(); ++i) {
      Json::Value event;
      event["ts"] = Json::Int64(samples.ts()[i] / 1000);

      UniqueTid utid = static_cast<UniqueTid>(samples.utid()[i]);
      auto pid_and_tid = UtidToPidAndTid(utid);
      event["pid"] = Json::Int(pid_and_tid.first);
      event["tid"] = Json::Int(pid_and_tid.second);

      event["ph"] = "n";
      event["cat"] = "disabled-by-default-cpu_profiler";
      event["name"] = "StackCpuSampling";
      event["s"] = "t";

      // Add a dummy thread timestamp to this event to match the format of
      // instant events. Useful in the UI to view args of a selected group of
      // samples.
      event["tts"] = Json::Int64(1);

      // "n"-phase events are nestable async events which get tied together with
      // their id, so we need to give each one a unique ID as we only
      // want the samples to show up on their own track in the trace-viewer but
      // not nested together.
      static size_t g_id_counter = 0;
      event["id"] = PrintUint64(++g_id_counter);

      const auto& callsites = storage_->stack_profile_callsite_table();
      const auto& frames = storage_->stack_profile_frame_table();
      const auto& mappings = storage_->stack_profile_mapping_table();

      std::vector<std::string> callstack;
      base::Optional<CallsiteId> opt_callsite_id = samples.callsite_id()[i];

      while (opt_callsite_id) {
        CallsiteId callsite_id = *opt_callsite_id;
        uint32_t callsite_row = *callsites.id().IndexOf(callsite_id);

        FrameId frame_id = callsites.frame_id()[callsite_row];
        uint32_t frame_row = *frames.id().IndexOf(frame_id);

        MappingId mapping_id = frames.mapping()[frame_row];
        uint32_t mapping_row = *mappings.id().IndexOf(mapping_id);

        NullTermStringView symbol_name;
        auto opt_symbol_set_id = frames.symbol_set_id()[frame_row];
        if (opt_symbol_set_id) {
          symbol_name = storage_->GetString(
              storage_->symbol_table().name()[*opt_symbol_set_id]);
        }

        char frame_entry[1024];
        snprintf(frame_entry, sizeof(frame_entry), "%s - %s [%s]\n",
                 (symbol_name.empty()
                      ? PrintUint64(
                            static_cast<uint64_t>(frames.rel_pc()[frame_row]))
                            .c_str()
                      : symbol_name.c_str()),
                 GetNonNullString(storage_, mappings.name()[mapping_row]),
                 GetNonNullString(storage_, mappings.build_id()[mapping_row]));

        callstack.emplace_back(frame_entry);

        opt_callsite_id = callsites.parent_id()[callsite_row];
      }

      std::string merged_callstack;
      for (auto entry = callstack.rbegin(); entry != callstack.rend();
           ++entry) {
        merged_callstack += *entry;
      }

      event["args"]["frames"] = merged_callstack;
      event["args"]["process_priority"] = samples.process_priority()[i];

      // TODO(oysteine): Used for backwards compatibility with the memlog
      // pipeline, should remove once we've switched to looking directly at the
      // tid.
      event["args"]["thread_id"] = Json::Int(pid_and_tid.second);

      writer_.WriteCommonEvent(event);
    }

    return util::OkStatus();
  }

  util::Status ExportMetadata() {
    const auto& trace_metadata = storage_->metadata_table();
    const auto& keys = trace_metadata.name();
    const auto& int_values = trace_metadata.int_value();
    const auto& str_values = trace_metadata.str_value();

    // Create a mapping from key string ids to keys.
    std::unordered_map<StringId, metadata::KeyIDs> key_map;
    for (uint32_t i = 0; i < metadata::kNumKeys; ++i) {
      auto id = *storage_->string_pool().GetId(metadata::kNames[i]);
      key_map[id] = static_cast<metadata::KeyIDs>(i);
    }

    for (uint32_t pos = 0; pos < trace_metadata.row_count(); pos++) {
      // Cast away from enum type, as otherwise -Wswitch-enum will demand an
      // exhaustive list of cases, even if there's a default case.
      metadata::KeyIDs key = key_map[keys[pos]];
      switch (static_cast<size_t>(key)) {
        case metadata::benchmark_description:
          writer_.AppendTelemetryMetadataString(
              "benchmarkDescriptions", str_values.GetString(pos).c_str());
          break;

        case metadata::benchmark_name:
          writer_.AppendTelemetryMetadataString(
              "benchmarks", str_values.GetString(pos).c_str());
          break;

        case metadata::benchmark_start_time_us:
          writer_.SetTelemetryMetadataTimestamp("benchmarkStart",
                                                *int_values[pos]);
          break;

        case metadata::benchmark_had_failures:
          writer_.AppendTelemetryMetadataBool("hadFailures", *int_values[pos]);
          break;

        case metadata::benchmark_label:
          writer_.AppendTelemetryMetadataString(
              "labels", str_values.GetString(pos).c_str());
          break;

        case metadata::benchmark_story_name:
          writer_.AppendTelemetryMetadataString(
              "stories", str_values.GetString(pos).c_str());
          break;

        case metadata::benchmark_story_run_index:
          writer_.AppendTelemetryMetadataInt("storysetRepeats",
                                             *int_values[pos]);
          break;

        case metadata::benchmark_story_run_time_us:
          writer_.SetTelemetryMetadataTimestamp("traceStart", *int_values[pos]);
          break;

        case metadata::benchmark_story_tags:  // repeated
          writer_.AppendTelemetryMetadataString(
              "storyTags", str_values.GetString(pos).c_str());
          break;

        default:
          PERFETTO_DLOG("Ignoring metadata key %zu", static_cast<size_t>(key));
          break;
      }
    }
    return util::OkStatus();
  }

  util::Status ExportStats() {
    const auto& stats = storage_->stats();

    for (size_t idx = 0; idx < stats::kNumKeys; idx++) {
      if (stats::kTypes[idx] == stats::kSingle) {
        writer_.SetStats(stats::kNames[idx], stats[idx].value);
      } else {
        PERFETTO_DCHECK(stats::kTypes[idx] == stats::kIndexed);
        writer_.SetStats(stats::kNames[idx], stats[idx].indexed_values);
      }
    }

    return util::OkStatus();
  }

  uint32_t UpidToPid(UniquePid upid) {
    auto pid_it = upids_to_exported_pids_.find(upid);
    PERFETTO_DCHECK(pid_it != upids_to_exported_pids_.end());
    return pid_it->second;
  }

  std::pair<uint32_t, uint32_t> UtidToPidAndTid(UniqueTid utid) {
    auto pid_and_tid_it = utids_to_exported_pids_and_tids_.find(utid);
    PERFETTO_DCHECK(pid_and_tid_it != utids_to_exported_pids_and_tids_.end());
    return pid_and_tid_it->second;
  }

  uint32_t NextExportedPidOrTidForDuplicates() {
    // Ensure that the exported substitute value does not represent a valid
    // pid/tid. This would be very unlikely in practice.
    while (IsValidPidOrTid(next_exported_pid_or_tid_for_duplicates_))
      next_exported_pid_or_tid_for_duplicates_--;
    return next_exported_pid_or_tid_for_duplicates_--;
  }

  bool IsValidPidOrTid(uint32_t pid_or_tid) {
    const auto& process_table = storage_->process_table();
    for (UniquePid upid = 0; upid < process_table.row_count(); upid++) {
      if (process_table.pid()[upid] == pid_or_tid)
        return true;
    }

    const auto& thread_table = storage_->thread_table();
    for (UniqueTid utid = 0; utid < thread_table.row_count(); utid++) {
      if (thread_table.tid()[utid] == pid_or_tid)
        return true;
    }

    return false;
  }

  const TraceStorage* storage_;
  ArgsBuilder args_builder_;
  TraceFormatWriter writer_;

  // If a pid/tid is duplicated between two or more  different processes/threads
  // (pid/tid reuse), we export the subsequent occurrences with different
  // pids/tids that is visibly different from regular pids/tids - counting down
  // from uint32_t max.
  uint32_t next_exported_pid_or_tid_for_duplicates_ =
      std::numeric_limits<uint32_t>::max();

  std::map<UniquePid, uint32_t> upids_to_exported_pids_;
  std::map<uint32_t, UniquePid> exported_pids_to_upids_;
  std::map<UniqueTid, std::pair<uint32_t, uint32_t>>
      utids_to_exported_pids_and_tids_;
  std::map<std::pair<uint32_t, uint32_t>, UniqueTid>
      exported_pids_and_tids_to_utids_;
};

#endif  // PERFETTO_BUILDFLAG(PERFETTO_TP_JSON)

}  // namespace

OutputWriter::OutputWriter() = default;
OutputWriter::~OutputWriter() = default;

util::Status ExportJson(const TraceStorage* storage,
                        OutputWriter* output,
                        ArgumentFilterPredicate argument_filter,
                        MetadataFilterPredicate metadata_filter,
                        LabelFilterPredicate label_filter) {
#if PERFETTO_BUILDFLAG(PERFETTO_TP_JSON)
  JsonExporter exporter(storage, output, std::move(argument_filter),
                        std::move(metadata_filter), std::move(label_filter));
  return exporter.Export();
#else
  perfetto::base::ignore_result(storage);
  perfetto::base::ignore_result(output);
  perfetto::base::ignore_result(argument_filter);
  perfetto::base::ignore_result(metadata_filter);
  perfetto::base::ignore_result(label_filter);
  return util::ErrStatus("JSON support is not compiled in this build");
#endif  // PERFETTO_BUILDFLAG(PERFETTO_TP_JSON)
}

util::Status ExportJson(TraceProcessorStorage* tp,
                        OutputWriter* output,
                        ArgumentFilterPredicate argument_filter,
                        MetadataFilterPredicate metadata_filter,
                        LabelFilterPredicate label_filter) {
  const TraceStorage* storage = reinterpret_cast<TraceProcessorStorageImpl*>(tp)
                                    ->context()
                                    ->storage.get();
  return ExportJson(storage, output, argument_filter, metadata_filter,
                    label_filter);
}

util::Status ExportJson(const TraceStorage* storage, FILE* output) {
  FileWriter writer(output);
  return ExportJson(storage, &writer, nullptr, nullptr, nullptr);
}

}  // namespace json
}  // namespace trace_processor
}  // namespace perfetto

