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

#include "src/trace_processor/dynamic/experimental_counter_dur_generator.h"

namespace perfetto {
namespace trace_processor {

ExperimentalCounterDurGenerator::ExperimentalCounterDurGenerator(
    const tables::CounterTable& table)
    : counter_table_(&table) {}
ExperimentalCounterDurGenerator::~ExperimentalCounterDurGenerator() = default;

Table::Schema ExperimentalCounterDurGenerator::CreateSchema() {
  Table::Schema schema = tables::CounterTable::Schema();
  schema.columns.emplace_back(
      Table::Schema::Column{"dur", SqlValue::Type::kLong, false /* is_id */,
                            false /* is_sorted */, false /* is_hidden */});
  return schema;
}

std::string ExperimentalCounterDurGenerator::TableName() {
  return "experimental_counter_dur";
}

uint32_t ExperimentalCounterDurGenerator::EstimateRowCount() {
  return counter_table_->row_count();
}

util::Status ExperimentalCounterDurGenerator::ValidateConstraints(
    const QueryConstraints&) {
  return util::OkStatus();
}

std::unique_ptr<Table> ExperimentalCounterDurGenerator::ComputeTable(
    const std::vector<Constraint>& cs,
    const std::vector<Order>&) {
  std::vector<Constraint> constraints;
  for (const auto& c : cs) {
    if (c.col_idx ==
        static_cast<uint32_t>(tables::CounterTable::ColumnIndex::track_id)) {
      constraints.push_back(c);
    }
  }
  Table table = counter_table_->Filter(constraints);

  std::unique_ptr<SparseVector<int64_t>> dur_column(
      new SparseVector<int64_t>(ComputeDurColumn(table)));
  return std::unique_ptr<Table>(new Table(table.ExtendWithColumn(
      "dur", std::move(dur_column), TypedColumn<int64_t>::default_flags())));
}

// static
SparseVector<int64_t> ExperimentalCounterDurGenerator::ComputeDurColumn(
    const Table& table) {
  // Keep track of the last seen row for each track id.
  std::unordered_map<TrackId, uint32_t> last_row_for_track_id;
  SparseVector<int64_t> dur;

  const auto* ts_col =
      TypedColumn<int64_t>::FromColumn(table.GetColumnByName("ts"));
  const auto* track_id_col =
      TypedColumn<tables::CounterTrackTable::Id>::FromColumn(
          table.GetColumnByName("track_id"));

  for (uint32_t i = 0; i < table.row_count(); ++i) {
    // Check if we already have a previous row for the current track id.
    TrackId track_id = (*track_id_col)[i];
    auto it = last_row_for_track_id.find(track_id);
    if (it == last_row_for_track_id.end()) {
      // This means we don't have any row - start tracking this row for the
      // future.
      last_row_for_track_id.emplace(track_id, i);
    } else {
      // This means we have an previous row for the current track id. Update
      // the duration of the previous row to be up to the current ts.
      uint32_t old_row = it->second;
      it->second = i;
      dur.Set(old_row, (*ts_col)[i] - (*ts_col)[old_row]);
    }
    // Append -1 to mark this event as not having been finished. On a later
    // row, we may set this to have the correct value.
    dur.Append(-1);
  }
  return dur;
}

}  // namespace trace_processor
}  // namespace perfetto
