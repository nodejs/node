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

#include "src/trace_processor/sqlite/sqlite_raw_table.h"

#include <inttypes.h>

#include "perfetto/base/compiler.h"
#include "perfetto/ext/base/string_utils.h"
#include "src/trace_processor/importers/ftrace/ftrace_descriptors.h"
#include "src/trace_processor/sqlite/sqlite_utils.h"
#include "src/trace_processor/types/gfp_flags.h"
#include "src/trace_processor/types/task_state.h"
#include "src/trace_processor/types/variadic.h"

#include "protos/perfetto/trace/ftrace/binder.pbzero.h"
#include "protos/perfetto/trace/ftrace/clk.pbzero.h"
#include "protos/perfetto/trace/ftrace/filemap.pbzero.h"
#include "protos/perfetto/trace/ftrace/ftrace.pbzero.h"
#include "protos/perfetto/trace/ftrace/ftrace_event.pbzero.h"
#include "protos/perfetto/trace/ftrace/power.pbzero.h"
#include "protos/perfetto/trace/ftrace/sched.pbzero.h"
#include "protos/perfetto/trace/ftrace/workqueue.pbzero.h"

namespace perfetto {
namespace trace_processor {

namespace {
std::tuple<uint32_t, uint32_t> ParseKernelReleaseVersion(
    base::StringView system_release) {
  size_t first_dot_pos = system_release.find(".");
  size_t second_dot_pos = system_release.find(".", first_dot_pos + 1);
  auto major_version = base::StringToUInt32(
      system_release.substr(0, first_dot_pos).ToStdString());
  auto minor_version = base::StringToUInt32(
      system_release
          .substr(first_dot_pos + 1, second_dot_pos - (first_dot_pos + 1))
          .ToStdString());
  return std::make_tuple(major_version.value(), minor_version.value());
}

struct FtraceTime {
  FtraceTime(int64_t ns)
      : secs(ns / 1000000000LL), micros((ns - secs * 1000000000LL) / 1000) {}

  const int64_t secs;
  const int64_t micros;
};

class ArgsSerializer {
 public:
  ArgsSerializer(const TraceStorage*,
                 ArgSetId arg_set_id,
                 NullTermStringView event_name,
                 std::vector<uint32_t>* field_id_to_arg_index,
                 base::StringWriter*);

  void SerializeArgs();

 private:
  using ValueWriter = std::function<void(const Variadic&)>;

  void WriteArgForField(uint32_t field_id) {
    WriteArgForField(field_id,
                     [this](const Variadic& v) { return WriteValue(v); });
  }

  void WriteArgForField(uint32_t field_id, ValueWriter writer) {
    WriteArgAtRow(FieldIdToRow(field_id), writer);
  }

  void WriteValueForField(uint32_t field_id) {
    WriteValueForField(field_id,
                       [this](const Variadic& v) { return WriteValue(v); });
  }

  void WriteValueForField(uint32_t field_id, ValueWriter writer) {
    writer(storage_->GetArgValue(FieldIdToRow(field_id)));
  }

  void WriteArgAtRow(uint32_t arg_index) {
    WriteArgAtRow(arg_index,
                  [this](const Variadic& v) { return WriteValue(v); });
  }

  void WriteArgAtRow(uint32_t arg_index, ValueWriter writer);

  void WriteValue(const Variadic& variadic);

  bool ParseGfpFlags(Variadic value);

  uint32_t FieldIdToRow(uint32_t field_id) {
    PERFETTO_DCHECK(field_id > 0);
    PERFETTO_DCHECK(field_id < field_id_to_arg_index_->size());
    uint32_t index_in_arg_set = (*field_id_to_arg_index_)[field_id];
    return start_row_ + index_in_arg_set;
  }

  const TraceStorage* storage_ = nullptr;
  ArgSetId arg_set_id_ = kInvalidArgSetId;
  NullTermStringView event_name_;
  std::vector<uint32_t>* field_id_to_arg_index_;

  RowMap row_map_;
  uint32_t start_row_ = 0;

  base::StringWriter* writer_ = nullptr;
};

ArgsSerializer::ArgsSerializer(const TraceStorage* storage,
                               ArgSetId arg_set_id,
                               NullTermStringView event_name,
                               std::vector<uint32_t>* field_id_to_arg_index,
                               base::StringWriter* writer)
    : storage_(storage),
      arg_set_id_(arg_set_id),
      event_name_(event_name),
      field_id_to_arg_index_(field_id_to_arg_index),
      writer_(writer) {
  const auto& args = storage_->arg_table();
  const auto& set_ids = args.arg_set_id();

  // We assume that the row map is a contiguous range (which is always the case
  // because arg_set_ids are contiguous by definition).
  row_map_ = args.FilterToRowMap({set_ids.eq(arg_set_id_)});
  start_row_ = row_map_.empty() ? 0 : row_map_.Get(0);

  // If the vector already has entries, we've previously cached the mapping
  // from field id to arg index.
  if (!field_id_to_arg_index->empty())
    return;

  auto* descriptor = GetMessageDescriptorForName(event_name);
  if (!descriptor) {
    // If we don't have a descriptor, this event must be a generic ftrace event.
    // As we can't possibly have any special handling for generic events, just
    // add a row to the vector (for the invalid field id 0) to remove future
    // lookups for this event name.
    field_id_to_arg_index->resize(1);
    return;
  }

  // If we have a descriptor, try and create the mapping from proto field id
  // to the index in the arg set.
  size_t max = descriptor->max_field_id;

  // We need to reserve an index for the invalid field id 0.
  field_id_to_arg_index_->resize(max + 1);

  // Go through each field id and find the entry in the args table for that
  for (uint32_t i = 1; i <= max; ++i) {
    for (auto it = row_map_.IterateRows(); it; it.Next()) {
      base::StringView key = args.key().GetString(it.row());
      if (key == descriptor->fields[i].name) {
        (*field_id_to_arg_index)[i] = it.index();
        break;
      }
    }
  }
}

void ArgsSerializer::SerializeArgs() {
  if (row_map_.empty())
    return;

  if (event_name_ == "sched_switch") {
    using SS = protos::pbzero::SchedSwitchFtraceEvent;

    WriteArgForField(SS::kPrevCommFieldNumber);
    WriteArgForField(SS::kPrevPidFieldNumber);
    WriteArgForField(SS::kPrevPrioFieldNumber);
    WriteArgForField(SS::kPrevStateFieldNumber, [this](const Variadic& value) {
      PERFETTO_DCHECK(value.type == Variadic::Type::kInt);
      auto state = static_cast<uint16_t>(value.int_value);
      writer_->AppendString(
          ftrace_utils::TaskState(state).ToString('|').data());
    });
    writer_->AppendLiteral(" ==>");
    WriteArgForField(SS::kNextCommFieldNumber);
    WriteArgForField(SS::kNextPidFieldNumber);
    WriteArgForField(SS::kNextPrioFieldNumber);
    return;
  } else if (event_name_ == "sched_wakeup") {
    using SW = protos::pbzero::SchedWakeupFtraceEvent;
    WriteArgForField(SW::kCommFieldNumber);
    WriteArgForField(SW::kPidFieldNumber);
    WriteArgForField(SW::kPrioFieldNumber);
    WriteArgForField(SW::kTargetCpuFieldNumber, [this](const Variadic& value) {
      PERFETTO_DCHECK(value.type == Variadic::Type::kInt);
      writer_->AppendPaddedInt<'0', 3>(value.int_value);
    });
    return;
  } else if (event_name_ == "clock_set_rate") {
    using CSR = protos::pbzero::ClockSetRateFtraceEvent;

    // We use the string "todo" as the name to stay consistent with old
    // trace_to_text print code.
    writer_->AppendString(" todo");
    WriteArgForField(CSR::kStateFieldNumber);
    WriteArgForField(CSR::kCpuIdFieldNumber);
    return;
  } else if (event_name_ == "clk_set_rate") {
    using CSR = protos::pbzero::ClkSetRateFtraceEvent;
    writer_->AppendLiteral(" ");
    WriteValueForField(CSR::kNameFieldNumber);
    writer_->AppendLiteral(" ");
    WriteValueForField(CSR::kRateFieldNumber);
    return;
  } else if (event_name_ == "clock_enable") {
    using CE = protos::pbzero::ClockEnableFtraceEvent;
    WriteValueForField(CE::kNameFieldNumber);
    WriteArgForField(CE::kStateFieldNumber);
    WriteArgForField(CE::kCpuIdFieldNumber);
    return;
  } else if (event_name_ == "clock_disable") {
    using CD = protos::pbzero::ClockDisableFtraceEvent;
    WriteValueForField(CD::kNameFieldNumber);
    WriteArgForField(CD::kStateFieldNumber);
    WriteArgForField(CD::kCpuIdFieldNumber);
    return;
  } else if (event_name_ == "binder_transaction") {
    using BT = protos::pbzero::BinderTransactionFtraceEvent;
    writer_->AppendString(" transaction=");
    WriteValueForField(BT::kDebugIdFieldNumber, [this](const Variadic& value) {
      PERFETTO_DCHECK(value.type == Variadic::Type::kInt);
      writer_->AppendUnsignedInt(static_cast<uint32_t>(value.int_value));
    });

    writer_->AppendString(" dest_node=");
    WriteValueForField(
        BT::kTargetNodeFieldNumber, [this](const Variadic& value) {
          PERFETTO_DCHECK(value.type == Variadic::Type::kInt);
          writer_->AppendUnsignedInt(static_cast<uint32_t>(value.int_value));
        });

    writer_->AppendString(" dest_proc=");
    WriteValueForField(BT::kToProcFieldNumber);

    writer_->AppendString(" dest_thread=");
    WriteValueForField(BT::kToThreadFieldNumber);

    writer_->AppendString(" reply=");
    WriteValueForField(BT::kReplyFieldNumber);

    writer_->AppendString(" flags=0x");
    WriteValueForField(BT::kFlagsFieldNumber, [this](const Variadic& value) {
      PERFETTO_DCHECK(value.type == Variadic::Type::kUint);
      writer_->AppendHexInt(value.uint_value);
    });

    writer_->AppendString(" code=0x");
    WriteValueForField(BT::kCodeFieldNumber, [this](const Variadic& value) {
      PERFETTO_DCHECK(value.type == Variadic::Type::kUint);
      writer_->AppendHexInt(value.uint_value);
    });
    return;
  } else if (event_name_ == "binder_transaction_alloc_buf") {
    using BTAB = protos::pbzero::BinderTransactionAllocBufFtraceEvent;
    writer_->AppendString(" transaction=");
    WriteValueForField(
        BTAB::kDebugIdFieldNumber, [this](const Variadic& value) {
          PERFETTO_DCHECK(value.type == Variadic::Type::kInt);
          writer_->AppendUnsignedInt(static_cast<uint32_t>(value.int_value));
        });
    WriteArgForField(BTAB::kDataSizeFieldNumber);
    WriteArgForField(BTAB::kOffsetsSizeFieldNumber);
    return;
  } else if (event_name_ == "binder_transaction_received") {
    using BTR = protos::pbzero::BinderTransactionReceivedFtraceEvent;
    writer_->AppendString(" transaction=");
    WriteValueForField(BTR::kDebugIdFieldNumber, [this](const Variadic& value) {
      PERFETTO_DCHECK(value.type == Variadic::Type::kInt);
      writer_->AppendUnsignedInt(static_cast<uint32_t>(value.int_value));
    });
    return;
  } else if (event_name_ == "mm_filemap_add_to_page_cache") {
    using MFA = protos::pbzero::MmFilemapAddToPageCacheFtraceEvent;
    writer_->AppendString(" dev ");
    WriteValueForField(MFA::kSDevFieldNumber, [this](const Variadic& value) {
      PERFETTO_DCHECK(value.type == Variadic::Type::kUint);
      writer_->AppendUnsignedInt(value.uint_value >> 20);
    });
    writer_->AppendString(":");
    WriteValueForField(MFA::kSDevFieldNumber, [this](const Variadic& value) {
      PERFETTO_DCHECK(value.type == Variadic::Type::kUint);
      writer_->AppendUnsignedInt(value.uint_value & ((1 << 20) - 1));
    });
    writer_->AppendString(" ino ");
    WriteValueForField(MFA::kIInoFieldNumber, [this](const Variadic& value) {
      PERFETTO_DCHECK(value.type == Variadic::Type::kUint);
      writer_->AppendHexInt(value.uint_value);
    });
    writer_->AppendString(" page=0000000000000000");
    writer_->AppendString(" pfn=");
    WriteValueForField(MFA::kPfnFieldNumber);
    writer_->AppendString(" ofs=");
    WriteValueForField(MFA::kIndexFieldNumber, [this](const Variadic& value) {
      PERFETTO_DCHECK(value.type == Variadic::Type::kUint);
      writer_->AppendUnsignedInt(value.uint_value << 12);
    });
    return;
  } else if (event_name_ == "print") {
    using P = protos::pbzero::PrintFtraceEvent;

    writer_->AppendChar(' ');
    WriteValueForField(P::kBufFieldNumber, [this](const Variadic& value) {
      PERFETTO_DCHECK(value.type == Variadic::Type::kString);

      NullTermStringView str = storage_->GetString(value.string_value);
      // If the last character is a newline in a print, just drop it.
      auto chars_to_print = !str.empty() && str.c_str()[str.size() - 1] == '\n'
                                ? str.size() - 1
                                : str.size();
      writer_->AppendString(str.c_str(), chars_to_print);
    });
    return;
  } else if (event_name_ == "sched_blocked_reason") {
    using SBR = protos::pbzero::SchedBlockedReasonFtraceEvent;
    WriteArgForField(SBR::kPidFieldNumber);
    WriteArgForField(SBR::kIoWaitFieldNumber);
    WriteArgForField(SBR::kCallerFieldNumber, [this](const Variadic& value) {
      PERFETTO_DCHECK(value.type == Variadic::Type::kUint);
      writer_->AppendHexInt(value.uint_value);
    });
    return;
  } else if (event_name_ == "workqueue_activate_work") {
    using WAW = protos::pbzero::WorkqueueActivateWorkFtraceEvent;
    writer_->AppendString(" work struct ");
    WriteValueForField(WAW::kWorkFieldNumber, [this](const Variadic& value) {
      PERFETTO_DCHECK(value.type == Variadic::Type::kUint);
      writer_->AppendHexInt(value.uint_value);
    });
    return;
  } else if (event_name_ == "workqueue_execute_start") {
    using WES = protos::pbzero::WorkqueueExecuteStartFtraceEvent;
    writer_->AppendString(" work struct ");
    WriteValueForField(WES::kWorkFieldNumber, [this](const Variadic& value) {
      PERFETTO_DCHECK(value.type == Variadic::Type::kUint);
      writer_->AppendHexInt(value.uint_value);
    });
    writer_->AppendString(": function ");
    WriteValueForField(WES::kFunctionFieldNumber,
                       [this](const Variadic& value) {
                         PERFETTO_DCHECK(value.type == Variadic::Type::kUint);
                         writer_->AppendHexInt(value.uint_value);
                       });
    return;
  } else if (event_name_ == "workqueue_execute_end") {
    using WE = protos::pbzero::WorkqueueExecuteEndFtraceEvent;
    writer_->AppendString(" work struct ");
    WriteValueForField(WE::kWorkFieldNumber, [this](const Variadic& value) {
      PERFETTO_DCHECK(value.type == Variadic::Type::kUint);
      writer_->AppendHexInt(value.uint_value);
    });
    return;
  } else if (event_name_ == "workqueue_queue_work") {
    using WQW = protos::pbzero::WorkqueueQueueWorkFtraceEvent;
    writer_->AppendString(" work struct=");
    WriteValueForField(WQW::kWorkFieldNumber, [this](const Variadic& value) {
      PERFETTO_DCHECK(value.type == Variadic::Type::kUint);
      writer_->AppendHexInt(value.uint_value);
    });
    WriteArgForField(WQW::kFunctionFieldNumber, [this](const Variadic& value) {
      PERFETTO_DCHECK(value.type == Variadic::Type::kUint);
      writer_->AppendHexInt(value.uint_value);
    });
    WriteArgForField(WQW::kWorkqueueFieldNumber, [this](const Variadic& value) {
      PERFETTO_DCHECK(value.type == Variadic::Type::kUint);
      writer_->AppendHexInt(value.uint_value);
    });
    WriteValueForField(WQW::kReqCpuFieldNumber);
    WriteValueForField(WQW::kCpuFieldNumber);
    return;
  }

  for (auto it = row_map_.IterateRows(); it; it.Next()) {
    WriteArgAtRow(it.row());
  }
}

void ArgsSerializer::WriteArgAtRow(uint32_t arg_row, ValueWriter writer) {
  const auto& args = storage_->arg_table();
  const auto& key = storage_->GetString(args.key()[arg_row]);
  auto value = storage_->GetArgValue(arg_row);

  writer_->AppendChar(' ');
  writer_->AppendString(key.c_str(), key.size());
  writer_->AppendChar('=');

  if (key == "gfp_flags" && ParseGfpFlags(value))
    return;
  writer(value);
}

void ArgsSerializer::WriteValue(const Variadic& value) {
  switch (value.type) {
    case Variadic::kInt:
      writer_->AppendInt(value.int_value);
      break;
    case Variadic::kUint:
      writer_->AppendUnsignedInt(value.uint_value);
      break;
    case Variadic::kString: {
      const auto& str = storage_->GetString(value.string_value);
      writer_->AppendString(str.c_str(), str.size());
      break;
    }
    case Variadic::kReal:
      writer_->AppendDouble(value.real_value);
      break;
    case Variadic::kPointer:
      writer_->AppendUnsignedInt(value.pointer_value);
      break;
    case Variadic::kBool:
      writer_->AppendBool(value.bool_value);
      break;
    case Variadic::kJson: {
      const auto& str = storage_->GetString(value.json_value);
      writer_->AppendString(str.c_str(), str.size());
      break;
    }
  }
}

bool ArgsSerializer::ParseGfpFlags(Variadic value) {
  const auto& metadata_table = storage_->metadata_table();

  auto opt_name_idx = metadata_table.name().IndexOf(
      metadata::kNames[metadata::KeyIDs::system_name]);
  auto opt_release_idx = metadata_table.name().IndexOf(
      metadata::kNames[metadata::KeyIDs::system_release]);
  if (!opt_name_idx || !opt_release_idx)
    return false;

  const auto& str_value = metadata_table.str_value();
  base::StringView system_name = str_value.GetString(*opt_name_idx);
  if (system_name != "Linux")
    return false;

  base::StringView system_release = str_value.GetString(*opt_release_idx);
  auto version = ParseKernelReleaseVersion(system_release);

  WriteGfpFlag(value.uint_value, version, writer_);
  return true;
}

}  // namespace

SqliteRawTable::SqliteRawTable(sqlite3* db, Context context)
    : DbSqliteTable(
          db,
          {context.cache, tables::RawTable::Schema(), TableComputation::kStatic,
           &context.storage->raw_table(), nullptr}),
      serializer_(context.storage) {
  auto fn = [](sqlite3_context* ctx, int argc, sqlite3_value** argv) {
    auto* thiz = static_cast<SqliteRawTable*>(sqlite3_user_data(ctx));
    thiz->ToSystrace(ctx, argc, argv);
  };
  sqlite3_create_function(db, "to_ftrace", 1,
                          SQLITE_UTF8 | SQLITE_DETERMINISTIC, this, fn, nullptr,
                          nullptr);
}

SqliteRawTable::~SqliteRawTable() = default;

void SqliteRawTable::RegisterTable(sqlite3* db,
                                   QueryCache* cache,
                                   const TraceStorage* storage) {
  SqliteTable::Register<SqliteRawTable, Context>(db, Context{cache, storage},
                                                 "raw");
}

void SqliteRawTable::ToSystrace(sqlite3_context* ctx,
                                int argc,
                                sqlite3_value** argv) {
  if (argc != 1 || sqlite3_value_type(argv[0]) != SQLITE_INTEGER) {
    sqlite3_result_error(ctx, "Usage: to_ftrace(id)", -1);
    return;
  }
  uint32_t row = static_cast<uint32_t>(sqlite3_value_int64(argv[0]));

  auto str = serializer_.SerializeToString(row);
  sqlite3_result_text(ctx, str.release(), -1, free);
}

SystraceSerializer::SystraceSerializer(const TraceStorage* storage)
    : storage_(storage) {}

SystraceSerializer::ScopedCString SystraceSerializer::SerializeToString(
    uint32_t raw_row) {
  const auto& raw = storage_->raw_table();

  char line[4096];
  base::StringWriter writer(line, sizeof(line));

  SerializePrefix(raw_row, &writer);

  StringId event_name_id = raw.name()[raw_row];
  NullTermStringView event_name = storage_->GetString(event_name_id);
  writer.AppendChar(' ');
  if (event_name == "print") {
    writer.AppendString("tracing_mark_write");
  } else {
    writer.AppendString(event_name.c_str(), event_name.size());
  }
  writer.AppendChar(':');

  ArgsSerializer serializer(storage_, raw.arg_set_id()[raw_row], event_name,
                            &proto_id_to_arg_index_by_event_[event_name_id],
                            &writer);
  serializer.SerializeArgs();

  return ScopedCString(writer.CreateStringCopy(), free);
}

void SystraceSerializer::SerializePrefix(uint32_t raw_row,
                                         base::StringWriter* writer) {
  const auto& raw = storage_->raw_table();

  int64_t ts = raw.ts()[raw_row];
  uint32_t cpu = raw.cpu()[raw_row];

  UniqueTid utid = raw.utid()[raw_row];
  uint32_t tid = storage_->thread_table().tid()[utid];

  uint32_t tgid = 0;
  auto opt_upid = storage_->thread_table().upid()[utid];
  if (opt_upid.has_value()) {
    tgid = storage_->process_table().pid()[*opt_upid];
  }
  auto name = storage_->GetString(storage_->thread_table().name()[utid]);

  FtraceTime ftrace_time(ts);
  if (tid == 0) {
    name = "<idle>";
  } else if (name == "") {
    name = "<unknown>";
  } else if (name == "CrRendererMain") {
    // TODO(taylori): Remove this when crbug.com/978093 is fixed or
    // when a better solution is found.
    name = "CrRendererMainThread";
  }

  int64_t padding = 16 - static_cast<int64_t>(name.size());
  if (padding > 0) {
    writer->AppendChar(' ', static_cast<size_t>(padding));
  }
  for (size_t i = 0; i < name.size(); ++i) {
    char c = name.data()[i];
    writer->AppendChar(c == '-' ? '_' : c);
  }
  writer->AppendChar('-');

  size_t pre_pid_pos = writer->pos();
  writer->AppendInt(tid);
  size_t pid_chars = writer->pos() - pre_pid_pos;
  if (PERFETTO_LIKELY(pid_chars < 5)) {
    writer->AppendChar(' ', 5 - pid_chars);
  }

  writer->AppendLiteral(" (");
  if (tgid == 0) {
    writer->AppendLiteral("-----");
  } else {
    writer->AppendPaddedInt<' ', 5>(tgid);
  }
  writer->AppendLiteral(") [");
  writer->AppendPaddedInt<'0', 3>(cpu);
  writer->AppendLiteral("] .... ");

  writer->AppendInt(ftrace_time.secs);
  writer->AppendChar('.');
  writer->AppendPaddedInt<'0', 6>(ftrace_time.micros);
  writer->AppendChar(':');
}

}  // namespace trace_processor
}  // namespace perfetto
