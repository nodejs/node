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

#include "src/trace_processor/sqlite/sql_stats_table.h"

#include <sqlite3.h>

#include <algorithm>
#include <bitset>
#include <numeric>

#include "src/trace_processor/sqlite/sqlite_utils.h"
#include "src/trace_processor/storage/trace_storage.h"

namespace perfetto {
namespace trace_processor {

SqlStatsTable::SqlStatsTable(sqlite3*, const TraceStorage* storage)
    : storage_(storage) {}

void SqlStatsTable::RegisterTable(sqlite3* db, const TraceStorage* storage) {
  SqliteTable::Register<SqlStatsTable>(db, storage, "sqlstats");
}

util::Status SqlStatsTable::Init(int, const char* const*, Schema* schema) {
  *schema = Schema(
      {
          SqliteTable::Column(Column::kQuery, "query", SqlValue::Type::kString),
          SqliteTable::Column(Column::kTimeQueued, "queued",
                              SqlValue::Type::kLong),
          SqliteTable::Column(Column::kTimeStarted, "started",
                              SqlValue::Type::kLong),
          SqliteTable::Column(Column::kTimeFirstNext, "first_next",
                              SqlValue::Type::kLong),
          SqliteTable::Column(Column::kTimeEnded, "ended",
                              SqlValue::Type::kLong),
      },
      {Column::kTimeQueued});
  return util::OkStatus();
}

std::unique_ptr<SqliteTable::Cursor> SqlStatsTable::CreateCursor() {
  return std::unique_ptr<SqliteTable::Cursor>(new Cursor(this));
}

int SqlStatsTable::BestIndex(const QueryConstraints&, BestIndexInfo* info) {
  info->sqlite_omit_order_by = true;
  return SQLITE_OK;
}

SqlStatsTable::Cursor::Cursor(SqlStatsTable* table)
    : SqliteTable::Cursor(table), storage_(table->storage_), table_(table) {}

SqlStatsTable::Cursor::~Cursor() = default;

int SqlStatsTable::Cursor::Filter(const QueryConstraints&,
                                  sqlite3_value**,
                                  FilterHistory) {
  *this = Cursor(table_);
  num_rows_ = storage_->sql_stats().size();
  return SQLITE_OK;
}

int SqlStatsTable::Cursor::Next() {
  row_++;
  return SQLITE_OK;
}

int SqlStatsTable::Cursor::Eof() {
  return row_ >= num_rows_;
}

int SqlStatsTable::Cursor::Column(sqlite3_context* context, int col) {
  const TraceStorage::SqlStats& stats = storage_->sql_stats();
  switch (col) {
    case Column::kQuery:
      sqlite3_result_text(context, stats.queries()[row_].c_str(), -1,
                          sqlite_utils::kSqliteStatic);
      break;
    case Column::kTimeQueued:
      sqlite3_result_int64(context, stats.times_queued()[row_]);
      break;
    case Column::kTimeStarted:
      sqlite3_result_int64(context, stats.times_started()[row_]);
      break;
    case Column::kTimeFirstNext:
      sqlite3_result_int64(context, stats.times_first_next()[row_]);
      break;
    case Column::kTimeEnded:
      sqlite3_result_int64(context, stats.times_ended()[row_]);
      break;
  }
  return SQLITE_OK;
}

}  // namespace trace_processor
}  // namespace perfetto
