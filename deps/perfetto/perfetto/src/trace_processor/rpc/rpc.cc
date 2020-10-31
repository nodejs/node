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

#include "src/trace_processor/rpc/rpc.h"

#include <vector>

#include "perfetto/base/time.h"
#include "perfetto/protozero/scattered_heap_buffer.h"
#include "perfetto/trace_processor/trace_processor.h"
#include "protos/perfetto/metrics/metrics.pbzero.h"
#include "protos/perfetto/trace_processor/trace_processor.pbzero.h"

namespace perfetto {
namespace trace_processor {

using ColumnValues = protos::pbzero::RawQueryResult::ColumnValues;
using ColumnDesc = protos::pbzero::RawQueryResult::ColumnDesc;

// Writes a "Loading trace ..." update every N bytes.
constexpr size_t kProgressUpdateBytes = 50 * 1000 * 1000;

Rpc::Rpc(std::unique_ptr<TraceProcessor> preloaded_instance)
    : trace_processor_(std::move(preloaded_instance)) {}

Rpc::Rpc() : Rpc(nullptr) {}

Rpc::~Rpc() = default;

util::Status Rpc::Parse(const uint8_t* data, size_t len) {
  if (eof_) {
    // Reset the trace processor state if this is either the first call ever or
    // if another trace has been previously fully loaded.
    trace_processor_ = TraceProcessor::CreateInstance(Config());
    bytes_parsed_ = bytes_last_progress_ = 0;
    t_parse_started_ = base::GetWallTimeNs().count();
  }

  eof_ = false;
  bytes_parsed_ += len;
  MaybePrintProgress();

  if (len == 0)
    return util::OkStatus();

  // TraceProcessor needs take ownership of the memory chunk.
  std::unique_ptr<uint8_t[]> data_copy(new uint8_t[len]);
  memcpy(data_copy.get(), data, len);
  return trace_processor_->Parse(std::move(data_copy), len);
}

void Rpc::NotifyEndOfFile() {
  if (!trace_processor_)
    return;
  trace_processor_->NotifyEndOfFile();
  eof_ = true;
  MaybePrintProgress();
}

void Rpc::MaybePrintProgress() {
  if (eof_ || bytes_parsed_ - bytes_last_progress_ > kProgressUpdateBytes) {
    bytes_last_progress_ = bytes_parsed_;
    auto t_load_s = (base::GetWallTimeNs().count() - t_parse_started_) / 1e9;
    fprintf(stderr, "\rLoading trace %.2f MB (%.1f MB/s)%s",
            bytes_parsed_ / 1e6, bytes_parsed_ / 1e6 / t_load_s,
            (eof_ ? "\n" : ""));
    fflush(stderr);
  }
}

std::vector<uint8_t> Rpc::RawQuery(const uint8_t* args, size_t len) {
  protozero::HeapBuffered<protos::pbzero::RawQueryResult> result;
  protos::pbzero::RawQueryArgs::Decoder query(args, len);
  std::string sql_query = query.sql_query().ToStdString();
  PERFETTO_DLOG("[RPC] RawQuery < %s", sql_query.c_str());

  if (!trace_processor_) {
    static const char kErr[] = "RawQuery() called before Parse()";
    PERFETTO_ELOG("[RPC] %s", kErr);
    result->set_error(kErr);
    return result.SerializeAsArray();
  }

  auto it = trace_processor_->ExecuteQuery(sql_query.c_str());

  // This vector contains a standalone protozero message per column. The problem
  // it's solving is the following: (i) sqlite iterators are row-based; (ii) the
  // RawQueryResult proto is column-based (that was a poor design choice we
  // should revisit at some point); (iii) the protozero API doesn't allow to
  // begin a new nested message before the previous one is completed.
  // In order to avoid the interleaved-writing, we write each column in a
  // dedicated heap buffer and then we merge all the column data at the end,
  // after having iterated all rows.
  std::vector<protozero::HeapBuffered<ColumnValues>> cols(it.ColumnCount());

  // This constexpr is to avoid ODR-use of protozero constants which are only
  // declared but not defined. Putting directly UNKONWN in the vector ctor
  // causes a linker error in the WASM toolchain.
  static constexpr auto kUnknown = ColumnDesc::UNKNOWN;
  std::vector<ColumnDesc::Type> col_types(it.ColumnCount(), kUnknown);
  uint32_t rows = 0;

  for (; it.Next(); ++rows) {
    for (uint32_t col_idx = 0; col_idx < it.ColumnCount(); ++col_idx) {
      auto& col = cols[col_idx];
      auto& col_type = col_types[col_idx];

      using SqlValue = trace_processor::SqlValue;
      auto cell = it.Get(col_idx);
      if (col_type == ColumnDesc::UNKNOWN) {
        switch (cell.type) {
          case SqlValue::Type::kLong:
            col_type = ColumnDesc::LONG;
            break;
          case SqlValue::Type::kString:
            col_type = ColumnDesc::STRING;
            break;
          case SqlValue::Type::kDouble:
            col_type = ColumnDesc::DOUBLE;
            break;
          case SqlValue::Type::kBytes:
            col_type = ColumnDesc::STRING;
            break;
          case SqlValue::Type::kNull:
            break;
        }
      }

      // If either the column type is null or we still don't know the type,
      // just add null values to all the columns.
      if (cell.type == SqlValue::Type::kNull ||
          col_type == ColumnDesc::UNKNOWN) {
        col->add_long_values(0);
        col->add_string_values("[NULL]");
        col->add_double_values(0);
        col->add_is_nulls(true);
        continue;
      }

      // Cast the sqlite value to the type of the column.
      switch (col_type) {
        case ColumnDesc::LONG:
          PERFETTO_CHECK(cell.type == SqlValue::Type::kLong ||
                         cell.type == SqlValue::Type::kDouble);
          if (cell.type == SqlValue::Type::kLong) {
            col->add_long_values(cell.long_value);
          } else /* if (cell.type == SqlValue::Type::kDouble) */ {
            col->add_long_values(static_cast<int64_t>(cell.double_value));
          }
          col->add_is_nulls(false);
          break;
        case ColumnDesc::STRING: {
          if (cell.type == SqlValue::Type::kBytes) {
            col->add_string_values("<bytes>");
          } else {
            PERFETTO_CHECK(cell.type == SqlValue::Type::kString);
            col->add_string_values(cell.string_value);
          }
          col->add_is_nulls(false);
          break;
        }
        case ColumnDesc::DOUBLE:
          PERFETTO_CHECK(cell.type == SqlValue::Type::kLong ||
                         cell.type == SqlValue::Type::kDouble);
          if (cell.type == SqlValue::Type::kLong) {
            col->add_double_values(static_cast<double>(cell.long_value));
          } else /* if (cell.type == SqlValue::Type::kDouble) */ {
            col->add_double_values(cell.double_value);
          }
          col->add_is_nulls(false);
          break;
        case ColumnDesc::UNKNOWN:
          PERFETTO_FATAL("Handled in if statement above.");
      }
    }  // for(col)
  }    // for(row)

  // Write the column descriptors.
  for (uint32_t col_idx = 0; col_idx < it.ColumnCount(); ++col_idx) {
    auto* descriptor = result->add_column_descriptors();
    std::string col_name = it.GetColumnName(col_idx);
    descriptor->set_name(col_name.data(), col_name.size());
    descriptor->set_type(col_types[col_idx]);
  }

  // Merge the column values.
  for (uint32_t col_idx = 0; col_idx < it.ColumnCount(); ++col_idx) {
    std::vector<uint8_t> col_data = cols[col_idx].SerializeAsArray();
    result->AppendBytes(protos::pbzero::RawQueryResult::kColumnsFieldNumber,
                        col_data.data(), col_data.size());
  }

  util::Status status = it.Status();
  result->set_num_records(rows);
  if (!status.ok())
    result->set_error(status.c_message());
  PERFETTO_DLOG("[RPC] RawQuery > %d rows (err: %d)", rows, !status.ok());

  return result.SerializeAsArray();
}

std::string Rpc::GetCurrentTraceName() {
  if (!trace_processor_)
    return "";
  return trace_processor_->GetCurrentTraceName();
}

void Rpc::RestoreInitialTables() {
  if (trace_processor_)
    trace_processor_->RestoreInitialTables();
}

std::vector<uint8_t> Rpc::ComputeMetric(const uint8_t* data, size_t len) {
  protozero::HeapBuffered<protos::pbzero::ComputeMetricResult> result;
  if (!trace_processor_) {
    result->set_error("Null trace processor instance");
    return result.SerializeAsArray();
  }

  protos::pbzero::ComputeMetricArgs::Decoder args(data, len);
  std::vector<std::string> metric_names;
  for (auto it = args.metric_names(); it; ++it) {
    metric_names.emplace_back(it->as_std_string());
  }

  std::vector<uint8_t> metrics_proto;
  util::Status status =
      trace_processor_->ComputeMetric(metric_names, &metrics_proto);
  if (status.ok()) {
    auto* metrics = result->set_metrics();
    metrics->AppendRawProtoBytes(metrics_proto.data(), metrics_proto.size());
  } else {
    result->set_error(status.message());
  }
  return result.SerializeAsArray();
}

}  // namespace trace_processor
}  // namespace perfetto
