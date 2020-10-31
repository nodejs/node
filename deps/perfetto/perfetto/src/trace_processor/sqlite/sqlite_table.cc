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

#include "src/trace_processor/sqlite/sqlite_table.h"

#include <ctype.h>
#include <inttypes.h>
#include <string.h>
#include <algorithm>
#include <map>

#include "perfetto/base/logging.h"

namespace perfetto {
namespace trace_processor {

namespace {

std::string TypeToString(SqlValue::Type type) {
  switch (type) {
    case SqlValue::Type::kString:
      return "STRING";
    case SqlValue::Type::kLong:
      return "BIG INT";
    case SqlValue::Type::kDouble:
      return "DOUBLE";
    case SqlValue::Type::kBytes:
      return "BLOB";
    case SqlValue::Type::kNull:
      PERFETTO_FATAL("Cannot map unknown column type");
  }
  PERFETTO_FATAL("Not reached");  // For gcc
}

}  // namespace

// static
bool SqliteTable::debug = false;

SqliteTable::SqliteTable() = default;
SqliteTable::~SqliteTable() = default;

int SqliteTable::OpenInternal(sqlite3_vtab_cursor** ppCursor) {
  // Freed in xClose().
  *ppCursor = static_cast<sqlite3_vtab_cursor*>(CreateCursor().release());
  return SQLITE_OK;
}

int SqliteTable::BestIndexInternal(sqlite3_index_info* idx) {
  QueryConstraints qc;

  for (int i = 0; i < idx->nConstraint; i++) {
    const auto& cs = idx->aConstraint[i];
    if (!cs.usable)
      continue;
    qc.AddConstraint(cs.iColumn, cs.op, i);
  }

  for (int i = 0; i < idx->nOrderBy; i++) {
    int column = idx->aOrderBy[i].iColumn;
    bool desc = idx->aOrderBy[i].desc;
    qc.AddOrderBy(column, desc);
  }

  int ret = ModifyConstraints(&qc);
  if (ret != SQLITE_OK)
    return ret;

  BestIndexInfo info;
  info.estimated_cost = idx->estimatedCost;
  info.estimated_rows = idx->estimatedRows;
  info.sqlite_omit_constraint.resize(qc.constraints().size());

  ret = BestIndex(qc, &info);

  // Although the SQLite documentation promises that if we return
  // SQLITE_CONSTRAINT, it won't chose this query plan, in practice, this causes
  // the entire query to be abandonned even if there is another query plan which
  // would definitely work. For this reason, we reserve idxNum == INT_MAX for
  // invalid constraints and just keep the default estimated cost (which should
  // lead to the plan not being chosen). In xFilter, we can then return
  // SQLITE_CONSTRAINT if this query plan is still chosen.
  if (ret == SQLITE_CONSTRAINT) {
    idx->idxNum = kInvalidConstraintsInBestIndexNum;
    return SQLITE_OK;
  }

  if (ret != SQLITE_OK)
    return ret;

  idx->orderByConsumed = qc.order_by().empty() || info.sqlite_omit_order_by;
  idx->estimatedCost = info.estimated_cost;
  idx->estimatedRows = info.estimated_rows;

  // First pass: mark all constraints as omitted to ensure that any pruned
  // constraints are not checked for by SQLite.
  for (int i = 0; i < idx->nConstraint; ++i) {
    auto& u = idx->aConstraintUsage[i];
    u.omit = true;
  }

  // Second pass: actually set the correct omit and index values for all
  // retained constraints.
  for (uint32_t i = 0; i < qc.constraints().size(); ++i) {
    auto& u = idx->aConstraintUsage[qc.constraints()[i].a_constraint_idx];
    u.omit = info.sqlite_omit_constraint[i];
    u.argvIndex = static_cast<int>(i) + 1;
  }

  auto out_qc_str = qc.ToNewSqlite3String();
  if (SqliteTable::debug) {
    PERFETTO_LOG(
        "[%s::BestIndex] constraints=%s orderByConsumed=%d estimatedCost=%f "
        "estimatedRows=%" PRId64,
        name_.c_str(), out_qc_str.get(), idx->orderByConsumed,
        idx->estimatedCost, static_cast<int64_t>(idx->estimatedRows));
  }

  idx->idxStr = out_qc_str.release();
  idx->needToFreeIdxStr = true;
  idx->idxNum = ++best_index_num_;

  return SQLITE_OK;
}

int SqliteTable::ModifyConstraints(QueryConstraints*) {
  return SQLITE_OK;
}

int SqliteTable::FindFunction(const char*, FindFunctionFn, void**) {
  return 0;
}

int SqliteTable::Update(int, sqlite3_value**, sqlite3_int64*) {
  return SQLITE_READONLY;
}

bool SqliteTable::ReadConstraints(int idxNum, const char* idxStr, int argc) {
  bool cache_hit = true;
  if (idxNum != qc_hash_) {
    qc_cache_ = QueryConstraints::FromString(idxStr);
    qc_hash_ = idxNum;
    cache_hit = false;
  }

  // Logging this every ReadConstraints just leads to log spam on joins making
  // it unusable. Instead, only print this out when we miss the cache (which
  // happens precisely when the constraint set from SQLite changes.)
  if (SqliteTable::debug && !cache_hit) {
    PERFETTO_LOG("[%s::ParseConstraints] constraints=%s argc=%d", name_.c_str(),
                 idxStr, argc);
  }
  return cache_hit;
}

SqliteTable::Cursor::Cursor(SqliteTable* table) : table_(table) {
  // This is required to prevent us from leaving this field uninitialised if
  // we ever move construct the Cursor.
  pVtab = table;
}
SqliteTable::Cursor::~Cursor() = default;

int SqliteTable::Cursor::RowId(sqlite3_int64*) {
  return SQLITE_ERROR;
}

SqliteTable::Column::Column(size_t index,
                            std::string name,
                            SqlValue::Type type,
                            bool hidden)
    : index_(index), name_(name), type_(type), hidden_(hidden) {}

SqliteTable::Schema::Schema(std::vector<Column> columns,
                            std::vector<size_t> primary_keys)
    : columns_(std::move(columns)), primary_keys_(std::move(primary_keys)) {
  for (size_t i = 0; i < columns_.size(); i++) {
    PERFETTO_CHECK(columns_[i].index() == i);
  }
  for (auto key : primary_keys_) {
    PERFETTO_CHECK(key < columns_.size());
  }
}

SqliteTable::Schema::Schema() = default;
SqliteTable::Schema::Schema(const Schema&) = default;
SqliteTable::Schema& SqliteTable::Schema::operator=(const Schema&) = default;

std::string SqliteTable::Schema::ToCreateTableStmt() const {
  std::string stmt = "CREATE TABLE x(";
  for (size_t i = 0; i < columns_.size(); ++i) {
    const Column& col = columns_[i];
    stmt += " " + col.name();

    if (col.type() != SqlValue::Type::kNull) {
      stmt += " " + TypeToString(col.type());
    } else if (std::find(primary_keys_.begin(), primary_keys_.end(), i) !=
               primary_keys_.end()) {
      PERFETTO_FATAL("Unknown type for primary key column %s",
                     col.name().c_str());
    }
    if (col.hidden()) {
      stmt += " HIDDEN";
    }
    stmt += ",";
  }
  stmt += " PRIMARY KEY(";
  for (size_t i = 0; i < primary_keys_.size(); i++) {
    if (i != 0)
      stmt += ", ";
    stmt += columns_[primary_keys_[i]].name();
  }
  stmt += ")) WITHOUT ROWID;";
  return stmt;
}

}  // namespace trace_processor
}  // namespace perfetto
