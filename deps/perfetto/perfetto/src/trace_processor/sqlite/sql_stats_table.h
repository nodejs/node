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

#ifndef SRC_TRACE_PROCESSOR_SQLITE_SQL_STATS_TABLE_H_
#define SRC_TRACE_PROCESSOR_SQLITE_SQL_STATS_TABLE_H_

#include <limits>
#include <memory>

#include "src/trace_processor/sqlite/sqlite_table.h"

namespace perfetto {
namespace trace_processor {

class QueryConstraints;
class TraceStorage;

// A virtual table that allows to introspect performances of the SQL engine
// for the kMaxLogEntries queries.
class SqlStatsTable : public SqliteTable {
 public:
  enum Column {
    kQuery = 0,
    kTimeQueued = 1,
    kTimeStarted = 2,
    kTimeFirstNext = 3,
    kTimeEnded = 4,
  };

  // Implementation of the SQLite cursor interface.
  class Cursor : public SqliteTable::Cursor {
   public:
    Cursor(SqlStatsTable* storage);
    ~Cursor() override;

    // Implementation of SqliteTable::Cursor.
    int Filter(const QueryConstraints&,
               sqlite3_value**,
               FilterHistory) override;
    int Next() override;
    int Eof() override;
    int Column(sqlite3_context*, int N) override;

   private:
    Cursor(Cursor&) = delete;
    Cursor& operator=(const Cursor&) = delete;

    Cursor(Cursor&&) noexcept = default;
    Cursor& operator=(Cursor&&) = default;

    size_t row_ = 0;
    size_t num_rows_ = 0;
    const TraceStorage* storage_ = nullptr;
    SqlStatsTable* table_ = nullptr;
  };

  SqlStatsTable(sqlite3*, const TraceStorage* storage);

  static void RegisterTable(sqlite3* db, const TraceStorage* storage);

  // Table implementation.
  util::Status Init(int, const char* const*, Schema*) override;
  std::unique_ptr<SqliteTable::Cursor> CreateCursor() override;
  int BestIndex(const QueryConstraints&, BestIndexInfo*) override;

 private:
  const TraceStorage* const storage_;
};

}  // namespace trace_processor
}  // namespace perfetto

#endif  // SRC_TRACE_PROCESSOR_SQLITE_SQL_STATS_TABLE_H_
