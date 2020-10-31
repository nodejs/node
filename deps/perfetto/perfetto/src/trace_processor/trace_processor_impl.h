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

#ifndef SRC_TRACE_PROCESSOR_TRACE_PROCESSOR_IMPL_H_
#define SRC_TRACE_PROCESSOR_TRACE_PROCESSOR_IMPL_H_

#include <sqlite3.h>

#include <atomic>
#include <functional>
#include <string>
#include <vector>

#include "perfetto/ext/base/string_view.h"
#include "perfetto/trace_processor/basic_types.h"
#include "perfetto/trace_processor/status.h"
#include "perfetto/trace_processor/trace_processor.h"
#include "src/trace_processor/sqlite/db_sqlite_table.h"
#include "src/trace_processor/sqlite/query_cache.h"
#include "src/trace_processor/sqlite/scoped_db.h"
#include "src/trace_processor/trace_processor_storage_impl.h"

#include "src/trace_processor/metrics/metrics.h"
#include "src/trace_processor/util/descriptors.h"

namespace perfetto {
namespace trace_processor {

// Coordinates the loading of traces from an arbitrary source and allows
// execution of SQL queries on the events in these traces.
class TraceProcessorImpl : public TraceProcessor,
                           public TraceProcessorStorageImpl {
 public:
  explicit TraceProcessorImpl(const Config&);

  ~TraceProcessorImpl() override;

  // TraceProcessorStorage implementation:
  util::Status Parse(std::unique_ptr<uint8_t[]>, size_t) override;
  void NotifyEndOfFile() override;

  // TraceProcessor implementation:
  Iterator ExecuteQuery(const std::string& sql,
                        int64_t time_queued = 0) override;

  util::Status RegisterMetric(const std::string& path,
                              const std::string& sql) override;

  util::Status ExtendMetricsProto(const uint8_t* data, size_t size) override;

  util::Status ComputeMetric(const std::vector<std::string>& metric_names,
                             std::vector<uint8_t>* metrics) override;

  void InterruptQuery() override;

  size_t RestoreInitialTables() override;

  std::string GetCurrentTraceName() override;
  void SetCurrentTraceName(const std::string&) override;

 private:
  // Needed for iterators to be able to delete themselves from the vector.
  friend class IteratorImpl;

  template <typename Table>
  void RegisterDbTable(const Table& table) {
    DbSqliteTable::RegisterTable(*db_, query_cache_.get(), Table::Schema(),
                                 &table, table.table_name());
  }

  void RegisterDynamicTable(
      std::unique_ptr<DbSqliteTable::DynamicTableGenerator> generator) {
    DbSqliteTable::RegisterTable(*db_, query_cache_.get(),
                                 std::move(generator));
  }

  ScopedDb db_;
  std::unique_ptr<QueryCache> query_cache_;

  DescriptorPool pool_;
  std::vector<metrics::SqlMetricFile> sql_metrics_;

  std::vector<IteratorImpl*> iterators_;

  // This is atomic because it is set by the CTRL-C signal handler and we need
  // to prevent single-flow compiler optimizations in ExecuteQuery().
  std::atomic<bool> query_interrupted_{false};

  // Keeps track of the tables created by the ingestion process. This is used
  // by RestoreInitialTables() to delete all the tables/view that have been
  // created after that point.
  std::vector<std::string> initial_tables_;

  std::string current_trace_name_;
  uint64_t bytes_parsed_ = 0;
};

// The pointer implementation of TraceProcessor::Iterator.
class TraceProcessor::IteratorImpl {
 public:
  IteratorImpl(TraceProcessorImpl* impl,
               sqlite3* db,
               ScopedStmt,
               uint32_t column_count,
               util::Status,
               uint32_t sql_stats_row);
  ~IteratorImpl();

  IteratorImpl(IteratorImpl&) noexcept = delete;
  IteratorImpl& operator=(IteratorImpl&) = delete;

  IteratorImpl(IteratorImpl&&) noexcept = default;
  IteratorImpl& operator=(IteratorImpl&&) = default;

  // Methods called by TraceProcessor::Iterator.
  bool Next() {
    // Delegate to the cc file to prevent trace_storage.h include in this
    // file.
    if (!called_next_) {
      RecordFirstNextInSqlStats();
      called_next_ = true;
    }

    if (!status_.ok())
      return false;

    int ret = sqlite3_step(*stmt_);
    if (PERFETTO_UNLIKELY(ret != SQLITE_ROW && ret != SQLITE_DONE)) {
      status_ = util::ErrStatus("%s", sqlite3_errmsg(db_));
      return false;
    }
    return ret == SQLITE_ROW;
  }

  SqlValue Get(uint32_t col) {
    auto column = static_cast<int>(col);
    auto col_type = sqlite3_column_type(*stmt_, column);
    SqlValue value;
    switch (col_type) {
      case SQLITE_INTEGER:
        value.type = SqlValue::kLong;
        value.long_value = sqlite3_column_int64(*stmt_, column);
        break;
      case SQLITE_TEXT:
        value.type = SqlValue::kString;
        value.string_value =
            reinterpret_cast<const char*>(sqlite3_column_text(*stmt_, column));
        break;
      case SQLITE_FLOAT:
        value.type = SqlValue::kDouble;
        value.double_value = sqlite3_column_double(*stmt_, column);
        break;
      case SQLITE_BLOB:
        value.type = SqlValue::kBytes;
        value.bytes_value = sqlite3_column_blob(*stmt_, column);
        value.bytes_count =
            static_cast<size_t>(sqlite3_column_bytes(*stmt_, column));
        break;
      case SQLITE_NULL:
        value.type = SqlValue::kNull;
        break;
    }
    return value;
  }

  std::string GetColumnName(uint32_t col) {
    return sqlite3_column_name(stmt_.get(), static_cast<int>(col));
  }

  uint32_t ColumnCount() { return column_count_; }

  util::Status Status() { return status_; }

  // Methods called by TraceProcessorImpl.
  void Reset();

 private:
  void RecordFirstNextInSqlStats();

  TraceProcessorImpl* trace_processor_;
  sqlite3* db_ = nullptr;
  ScopedStmt stmt_;
  uint32_t column_count_ = 0;
  util::Status status_;

  uint32_t sql_stats_row_ = 0;
  bool called_next_ = false;
};

}  // namespace trace_processor
}  // namespace perfetto

#endif  // SRC_TRACE_PROCESSOR_TRACE_PROCESSOR_IMPL_H_
