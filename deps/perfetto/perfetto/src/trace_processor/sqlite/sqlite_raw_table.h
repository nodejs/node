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

#ifndef SRC_TRACE_PROCESSOR_SQLITE_SQLITE_RAW_TABLE_H_
#define SRC_TRACE_PROCESSOR_SQLITE_SQLITE_RAW_TABLE_H_

#include "perfetto/base/logging.h"
#include "perfetto/ext/base/string_writer.h"
#include "src/trace_processor/sqlite/db_sqlite_table.h"
#include "src/trace_processor/storage/trace_storage.h"
#include "src/trace_processor/types/variadic.h"

namespace perfetto {
namespace trace_processor {

class SystraceSerializer {
 public:
  using ScopedCString = std::unique_ptr<char, void (*)(void*)>;

  SystraceSerializer(const TraceStorage* storage);

  ScopedCString SerializeToString(uint32_t raw_row);

 private:
  using StringIdMap = std::unordered_map<StringId, std::vector<uint32_t>>;

  void SerializePrefix(uint32_t raw_row, base::StringWriter* writer);

  StringIdMap proto_id_to_arg_index_by_event_;
  const TraceStorage* storage_ = nullptr;
};

class SqliteRawTable : public DbSqliteTable {
 public:
  struct Context {
    QueryCache* cache;
    const TraceStorage* storage;
  };

  SqliteRawTable(sqlite3*, Context);
  virtual ~SqliteRawTable();

  static void RegisterTable(sqlite3* db, QueryCache*, const TraceStorage*);

 private:
  void ToSystrace(sqlite3_context* ctx, int argc, sqlite3_value** argv);

  SystraceSerializer serializer_;
};

}  // namespace trace_processor
}  // namespace perfetto

#endif  // SRC_TRACE_PROCESSOR_SQLITE_SQLITE_RAW_TABLE_H_
