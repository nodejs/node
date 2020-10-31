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

#include "src/trace_processor/sqlite/stats_table.h"

#include "src/trace_processor/sqlite/sqlite_utils.h"

namespace perfetto {
namespace trace_processor {

StatsTable::StatsTable(sqlite3*, const TraceStorage* storage)
    : storage_(storage) {}

void StatsTable::RegisterTable(sqlite3* db, const TraceStorage* storage) {
  SqliteTable::Register<StatsTable>(db, storage, "stats");
}

util::Status StatsTable::Init(int, const char* const*, Schema* schema) {
  *schema = Schema(
      {
          SqliteTable::Column(Column::kName, "name", SqlValue::Type::kString),
          // Calling a column "index" causes sqlite to silently fail, hence idx.
          SqliteTable::Column(Column::kIndex, "idx", SqlValue::Type::kLong),
          SqliteTable::Column(Column::kSeverity, "severity",
                              SqlValue::Type::kString),
          SqliteTable::Column(Column::kSource, "source",
                              SqlValue::Type::kString),
          SqliteTable::Column(Column::kValue, "value", SqlValue::Type::kLong),
      },
      {Column::kName});
  return util::OkStatus();
}

std::unique_ptr<SqliteTable::Cursor> StatsTable::CreateCursor() {
  return std::unique_ptr<SqliteTable::Cursor>(new Cursor(this));
}

int StatsTable::BestIndex(const QueryConstraints&, BestIndexInfo*) {
  return SQLITE_OK;
}

StatsTable::Cursor::Cursor(StatsTable* table)
    : SqliteTable::Cursor(table), table_(table), storage_(table->storage_) {}

int StatsTable::Cursor::Filter(const QueryConstraints&,
                               sqlite3_value**,
                               FilterHistory) {
  *this = Cursor(table_);
  return SQLITE_OK;
}

int StatsTable::Cursor::Column(sqlite3_context* ctx, int N) {
  const auto kSqliteStatic = sqlite_utils::kSqliteStatic;
  switch (N) {
    case Column::kName:
      sqlite3_result_text(ctx, stats::kNames[key_], -1, kSqliteStatic);
      break;
    case Column::kIndex:
      if (stats::kTypes[key_] == stats::kIndexed) {
        sqlite3_result_int(ctx, index_->first);
      } else {
        sqlite3_result_null(ctx);
      }
      break;
    case Column::kSeverity:
      switch (stats::kSeverities[key_]) {
        case stats::kInfo:
          sqlite3_result_text(ctx, "info", -1, kSqliteStatic);
          break;
        case stats::kDataLoss:
          sqlite3_result_text(ctx, "data_loss", -1, kSqliteStatic);
          break;
        case stats::kError:
          sqlite3_result_text(ctx, "error", -1, kSqliteStatic);
          break;
      }
      break;
    case Column::kSource:
      switch (stats::kSources[key_]) {
        case stats::kTrace:
          sqlite3_result_text(ctx, "trace", -1, kSqliteStatic);
          break;
        case stats::kAnalysis:
          sqlite3_result_text(ctx, "analysis", -1, kSqliteStatic);
          break;
      }
      break;
    case Column::kValue:
      if (stats::kTypes[key_] == stats::kIndexed) {
        sqlite3_result_int64(ctx, index_->second);
      } else {
        sqlite3_result_int64(ctx, storage_->stats()[key_].value);
      }
      break;
    default:
      PERFETTO_FATAL("Unknown column %d", N);
      break;
  }
  return SQLITE_OK;
}

int StatsTable::Cursor::Next() {
  static_assert(stats::kTypes[0] == stats::kSingle,
                "the first stats entry cannot be indexed");
  const auto* cur_entry = &storage_->stats()[key_];
  if (stats::kTypes[key_] == stats::kIndexed) {
    if (++index_ != cur_entry->indexed_values.end()) {
      return SQLITE_OK;
    }
  }
  while (++key_ < stats::kNumKeys) {
    cur_entry = &storage_->stats()[key_];
    index_ = cur_entry->indexed_values.begin();
    if (stats::kTypes[key_] == stats::kSingle ||
        !cur_entry->indexed_values.empty()) {
      break;
    }
  }
  return SQLITE_OK;
}

int StatsTable::Cursor::Eof() {
  return key_ >= stats::kNumKeys;
}

}  // namespace trace_processor
}  // namespace perfetto
