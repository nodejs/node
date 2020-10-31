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

#ifndef SRC_TRACE_PROCESSOR_DYNAMIC_EXPERIMENTAL_SLICE_LAYOUT_GENERATOR_H_
#define SRC_TRACE_PROCESSOR_DYNAMIC_EXPERIMENTAL_SLICE_LAYOUT_GENERATOR_H_

#include <set>

#include "src/trace_processor/sqlite/db_sqlite_table.h"
#include "src/trace_processor/storage/trace_storage.h"

namespace perfetto {
namespace trace_processor {

class ExperimentalSliceLayoutGenerator
    : public DbSqliteTable::DynamicTableGenerator {
 public:
  static constexpr uint32_t kFilterTrackIdsColumnIndex =
      static_cast<uint32_t>(tables::SliceTable::ColumnIndex::arg_set_id) + 2;

  ExperimentalSliceLayoutGenerator(StringPool* string_pool,
                                   const tables::SliceTable* table);
  virtual ~ExperimentalSliceLayoutGenerator() override;

  Table::Schema CreateSchema() override;
  std::string TableName() override;
  uint32_t EstimateRowCount() override;
  util::Status ValidateConstraints(const QueryConstraints&) override;
  std::unique_ptr<Table> ComputeTable(const std::vector<Constraint>&,
                                      const std::vector<Order>&) override;

 private:
  std::unique_ptr<Table> AddLayoutColumn(const Table& table,
                                         const std::set<TrackId>& selected,
                                         StringPool::Id filter_id);

  StringPool* string_pool_;
  const tables::SliceTable* slice_table_;
  const StringPool::Id empty_string_id_;
};

}  // namespace trace_processor
}  // namespace perfetto

#endif  // SRC_TRACE_PROCESSOR_DYNAMIC_EXPERIMENTAL_SLICE_LAYOUT_GENERATOR_H_
