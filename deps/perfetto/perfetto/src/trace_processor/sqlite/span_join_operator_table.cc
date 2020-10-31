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

#include "src/trace_processor/sqlite/span_join_operator_table.h"

#include <sqlite3.h>
#include <string.h>

#include <algorithm>
#include <set>
#include <utility>

#include "perfetto/base/logging.h"
#include "perfetto/ext/base/string_splitter.h"
#include "perfetto/ext/base/string_utils.h"
#include "perfetto/ext/base/string_view.h"
#include "src/trace_processor/sqlite/sqlite_utils.h"

namespace perfetto {
namespace trace_processor {

namespace {

constexpr char kTsColumnName[] = "ts";
constexpr char kDurColumnName[] = "dur";

bool IsRequiredColumn(const std::string& name) {
  return name == kTsColumnName || name == kDurColumnName;
}

base::Optional<std::string> HasDuplicateColumns(
    const std::vector<SqliteTable::Column>& cols) {
  std::set<std::string> names;
  for (const auto& col : cols) {
    if (names.count(col.name()) > 0)
      return col.name();
    names.insert(col.name());
  }
  return base::nullopt;
}

}  // namespace

SpanJoinOperatorTable::SpanJoinOperatorTable(sqlite3* db, const TraceStorage*)
    : db_(db) {}

void SpanJoinOperatorTable::RegisterTable(sqlite3* db,
                                          const TraceStorage* storage) {
  SqliteTable::Register<SpanJoinOperatorTable>(db, storage, "span_join",
                                               /* read_write */ false,
                                               /* requires_args */ true);

  SqliteTable::Register<SpanJoinOperatorTable>(db, storage, "span_left_join",
                                               /* read_write */ false,
                                               /* requires_args */ true);

  SqliteTable::Register<SpanJoinOperatorTable>(db, storage, "span_outer_join",
                                               /* read_write */ false,
                                               /* requires_args */ true);
}

util::Status SpanJoinOperatorTable::Init(int argc,
                                         const char* const* argv,
                                         Schema* schema) {
  // argv[0] - argv[2] are SQLite populated fields which are always present.
  if (argc < 5)
    return util::Status("SPAN_JOIN: expected at least 2 args");

  TableDescriptor t1_desc;
  auto status = TableDescriptor::Parse(
      std::string(reinterpret_cast<const char*>(argv[3])), &t1_desc);
  if (!status.ok())
    return status;

  TableDescriptor t2_desc;
  status = TableDescriptor::Parse(
      std::string(reinterpret_cast<const char*>(argv[4])), &t2_desc);
  if (!status.ok())
    return status;

  // Check that the partition columns match between the two tables.
  if (t1_desc.partition_col == t2_desc.partition_col) {
    partitioning_ = t1_desc.IsPartitioned()
                        ? PartitioningType::kSamePartitioning
                        : PartitioningType::kNoPartitioning;
    if (partitioning_ == PartitioningType::kNoPartitioning && IsOuterJoin()) {
      return util::ErrStatus(
          "SPAN_JOIN: Outer join not supported for no partition tables");
    }
  } else if (t1_desc.IsPartitioned() && t2_desc.IsPartitioned()) {
    return util::ErrStatus(
        "SPAN_JOIN: mismatching partitions between the two tables; "
        "(partition %s in table %s, partition %s in table %s)",
        t1_desc.partition_col.c_str(), t1_desc.name.c_str(),
        t2_desc.partition_col.c_str(), t2_desc.name.c_str());
  } else {
    if (IsOuterJoin()) {
      return util::ErrStatus(
          "SPAN_JOIN: Outer join not supported for mixed partitioned tables");
    }
    partitioning_ = PartitioningType::kMixedPartitioning;
  }

  bool t1_part_mixed = t1_desc.IsPartitioned() &&
                       partitioning_ == PartitioningType::kMixedPartitioning;
  bool t2_part_mixed = t2_desc.IsPartitioned() &&
                       partitioning_ == PartitioningType::kMixedPartitioning;

  EmitShadowType t1_shadow_type;
  if (IsOuterJoin()) {
    if (t1_part_mixed || partitioning_ == PartitioningType::kNoPartitioning) {
      t1_shadow_type = EmitShadowType::kPresentPartitionOnly;
    } else {
      t1_shadow_type = EmitShadowType::kAll;
    }
  } else {
    t1_shadow_type = EmitShadowType::kNone;
  }
  status = CreateTableDefinition(t1_desc, t1_shadow_type, &t1_defn_);
  if (!status.ok())
    return status;

  EmitShadowType t2_shadow_type;
  if (IsOuterJoin() || IsLeftJoin()) {
    if (t2_part_mixed || partitioning_ == PartitioningType::kNoPartitioning) {
      t2_shadow_type = EmitShadowType::kPresentPartitionOnly;
    } else {
      t2_shadow_type = EmitShadowType::kAll;
    }
  } else {
    t2_shadow_type = EmitShadowType::kNone;
  }
  status = CreateTableDefinition(t2_desc, t2_shadow_type, &t2_defn_);
  if (!status.ok())
    return status;

  std::vector<SqliteTable::Column> cols;
  // Ensure the shared columns are consistently ordered and are not
  // present twice in the final schema
  cols.emplace_back(Column::kTimestamp, kTsColumnName, SqlValue::Type::kLong);
  cols.emplace_back(Column::kDuration, kDurColumnName, SqlValue::Type::kLong);
  if (partitioning_ != PartitioningType::kNoPartitioning)
    cols.emplace_back(Column::kPartition, partition_col(),
                      SqlValue::Type::kLong);

  CreateSchemaColsForDefn(t1_defn_, &cols);
  CreateSchemaColsForDefn(t2_defn_, &cols);

  if (auto opt_dupe_col = HasDuplicateColumns(cols)) {
    return util::ErrStatus(
        "SPAN_JOIN: column %s present in both tables %s and %s",
        opt_dupe_col->c_str(), t1_defn_.name().c_str(),
        t2_defn_.name().c_str());
  }
  std::vector<size_t> primary_keys = {Column::kTimestamp};
  if (partitioning_ != PartitioningType::kNoPartitioning)
    primary_keys.push_back(Column::kPartition);
  *schema = Schema(cols, primary_keys);

  return util::OkStatus();
}

void SpanJoinOperatorTable::CreateSchemaColsForDefn(
    const TableDefinition& defn,
    std::vector<SqliteTable::Column>* cols) {
  for (size_t i = 0; i < defn.columns().size(); i++) {
    const auto& n = defn.columns()[i].name();
    if (IsRequiredColumn(n) || n == defn.partition_col())
      continue;

    ColumnLocator* locator = &global_index_to_column_locator_[cols->size()];
    locator->defn = &defn;
    locator->col_index = i;

    cols->emplace_back(cols->size(), n, defn.columns()[i].type());
  }
}

std::unique_ptr<SqliteTable::Cursor> SpanJoinOperatorTable::CreateCursor() {
  return std::unique_ptr<SpanJoinOperatorTable::Cursor>(new Cursor(this, db_));
}

int SpanJoinOperatorTable::BestIndex(const QueryConstraints& qc,
                                     BestIndexInfo* info) {
  // TODO(lalitm): figure out cost estimation.
  const auto& ob = qc.order_by();

  if (partitioning_ == PartitioningType::kNoPartitioning) {
    // If both tables are not partitioned and we have a single order by on ts,
    // we return data in the correct order.
    info->sqlite_omit_order_by =
        ob.size() == 1 && ob[0].iColumn == Column::kTimestamp && !ob[0].desc;
  } else {
    // If one of the tables is partitioned, and we have an order by on the
    // partition column followed (optionally) by an order by on timestamp, we
    // return data in the correct order.
    bool is_first_ob_partition =
        ob.size() >= 1 && ob[0].iColumn == Column::kPartition && !ob[0].desc;
    bool is_second_ob_ts =
        ob.size() >= 2 && ob[1].iColumn == Column::kTimestamp && !ob[1].desc;
    info->sqlite_omit_order_by =
        (ob.size() == 1 && is_first_ob_partition) ||
        (ob.size() == 2 && is_first_ob_partition && is_second_ob_ts);
  }
  return SQLITE_OK;
}

std::vector<std::string>
SpanJoinOperatorTable::ComputeSqlConstraintsForDefinition(
    const TableDefinition& defn,
    const QueryConstraints& qc,
    sqlite3_value** argv) {
  std::vector<std::string> constraints;
  for (size_t i = 0; i < qc.constraints().size(); i++) {
    const auto& cs = qc.constraints()[i];
    auto col_name = GetNameForGlobalColumnIndex(defn, cs.column);
    if (col_name == "")
      continue;

    if (col_name == kTsColumnName || col_name == kDurColumnName) {
      // Allow SQLite handle any constraints on ts or duration.
      continue;
    }
    auto op = sqlite_utils::OpToString(cs.op);
    auto value = sqlite_utils::SqliteValueAsString(argv[i]);

    constraints.emplace_back("`" + col_name + "`" + op + value);
  }
  return constraints;
}

util::Status SpanJoinOperatorTable::CreateTableDefinition(
    const TableDescriptor& desc,
    EmitShadowType emit_shadow_type,
    SpanJoinOperatorTable::TableDefinition* defn) {
  if (desc.partition_col == kTsColumnName ||
      desc.partition_col == kDurColumnName) {
    return util::ErrStatus(
        "SPAN_JOIN: partition column cannot be any of {ts, dur} for table %s",
        desc.name.c_str());
  }

  auto cols = sqlite_utils::GetColumnsForTable(db_, desc.name);

  uint32_t required_columns_found = 0;
  uint32_t ts_idx = std::numeric_limits<uint32_t>::max();
  uint32_t dur_idx = std::numeric_limits<uint32_t>::max();
  uint32_t partition_idx = std::numeric_limits<uint32_t>::max();
  for (uint32_t i = 0; i < cols.size(); i++) {
    auto col = cols[i];
    if (IsRequiredColumn(col.name())) {
      ++required_columns_found;
      if (col.type() != SqlValue::Type::kLong &&
          col.type() != SqlValue::Type::kNull) {
        return util::ErrStatus(
            "SPAN_JOIN: Invalid type for column %s in table %s",
            col.name().c_str(), desc.name.c_str());
      }
    }

    if (col.name() == kTsColumnName) {
      ts_idx = i;
    } else if (col.name() == kDurColumnName) {
      dur_idx = i;
    } else if (col.name() == desc.partition_col) {
      partition_idx = i;
    }
  }
  if (required_columns_found != 2) {
    return util::ErrStatus(
        "SPAN_JOIN: Missing one of columns {ts, dur} in table %s",
        desc.name.c_str());
  } else if (desc.IsPartitioned() && partition_idx >= cols.size()) {
    return util::ErrStatus("SPAN_JOIN: Missing partition column %s in table %s",
                           desc.partition_col.c_str(), desc.name.c_str());
  }

  PERFETTO_DCHECK(ts_idx < cols.size());
  PERFETTO_DCHECK(dur_idx < cols.size());

  *defn = TableDefinition(desc.name, desc.partition_col, std::move(cols),
                          emit_shadow_type, ts_idx, dur_idx, partition_idx);
  return util::OkStatus();
}

std::string SpanJoinOperatorTable::GetNameForGlobalColumnIndex(
    const TableDefinition& defn,
    int global_column) {
  size_t col_idx = static_cast<size_t>(global_column);
  if (col_idx == Column::kTimestamp)
    return kTsColumnName;
  else if (col_idx == Column::kDuration)
    return kDurColumnName;
  else if (col_idx == Column::kPartition &&
           partitioning_ != PartitioningType::kNoPartitioning)
    return defn.partition_col().c_str();

  const auto& locator = global_index_to_column_locator_[col_idx];
  if (locator.defn != &defn)
    return "";
  return defn.columns()[locator.col_index].name().c_str();
}

SpanJoinOperatorTable::Cursor::Cursor(SpanJoinOperatorTable* table, sqlite3* db)
    : SqliteTable::Cursor(table),
      t1_(table, &table->t1_defn_, db),
      t2_(table, &table->t2_defn_, db),
      table_(table) {}

int SpanJoinOperatorTable::Cursor::Filter(const QueryConstraints& qc,
                                          sqlite3_value** argv,
                                          FilterHistory) {
  util::Status status = t1_.Initialize(qc, argv);
  if (!status.ok())
    return SQLITE_ERROR;

  status = t2_.Initialize(qc, argv);
  if (!status.ok())
    return SQLITE_ERROR;

  status = FindOverlappingSpan();
  return status.ok() ? SQLITE_OK : SQLITE_ERROR;
}

int SpanJoinOperatorTable::Cursor::Next() {
  util::Status status = next_query_->Next();
  if (!status.ok())
    return SQLITE_ERROR;

  status = FindOverlappingSpan();
  return status.ok() ? SQLITE_OK : SQLITE_ERROR;
}

bool SpanJoinOperatorTable::Cursor::IsOverlappingSpan() {
  // If either of the tables are eof, then we cannot possibly have an
  // overlapping span.
  if (t1_.IsEof() || t2_.IsEof())
    return false;

  // One of the tables always needs to have a real span to have a valid
  // overlapping span.
  if (!t1_.IsReal() && !t2_.IsReal())
    return false;

  if (table_->partitioning_ == PartitioningType::kSamePartitioning) {
    // If both tables are partitioned, then ensure that the partitions overlap.
    bool partition_in_bounds = (t1_.FirstPartition() >= t2_.FirstPartition() &&
                                t1_.FirstPartition() <= t2_.LastPartition()) ||
                               (t2_.FirstPartition() >= t1_.FirstPartition() &&
                                t2_.FirstPartition() <= t1_.LastPartition());
    if (!partition_in_bounds)
      return false;
  }

  // We consider all slices to be [start, end) - that is the range of
  // timestamps has an open interval at the start but a closed interval
  // at the end. (with the exception of dur == -1 which we treat as if
  // end == start for the purpose of this function).
  return (t1_.ts() == t2_.ts() && t1_.IsReal() && t2_.IsReal()) ||
         (t1_.ts() >= t2_.ts() && t1_.ts() < t2_.AdjustedTsEnd()) ||
         (t2_.ts() >= t1_.ts() && t2_.ts() < t1_.AdjustedTsEnd());
}

util::Status SpanJoinOperatorTable::Cursor::FindOverlappingSpan() {
  // We loop until we find a slice which overlaps from the two tables.
  while (true) {
    if (table_->partitioning_ == PartitioningType::kMixedPartitioning) {
      // If we have a mixed partition setup, we need to have special checks
      // for eof and to reset the unpartitioned cursor every time the partition
      // changes in the partitioned table.
      auto* partitioned = t1_.definition()->IsPartitioned() ? &t1_ : &t2_;
      auto* unpartitioned = t1_.definition()->IsPartitioned() ? &t2_ : &t1_;

      // If the partitioned table reaches eof, then we are really done.
      if (partitioned->IsEof())
        break;

      // If the partition has changed from the previous one, reset the cursor
      // and keep a lot of the new partition.
      if (last_mixed_partition_ != partitioned->partition()) {
        util::Status status = unpartitioned->Rewind();
        if (!status.ok())
          return status;
        last_mixed_partition_ = partitioned->partition();
      }
    } else if (t1_.IsEof() || t2_.IsEof()) {
      // For both no partition and same partition cases, either cursor ending
      // ends the whole span join.
      break;
    }

    // Find which slice finishes first.
    next_query_ = FindEarliestFinishQuery();

    // If the current span is overlapping, just finsh there to emit the current
    // slice.
    if (IsOverlappingSpan())
      break;

    // Otherwise, step to the next row.
    util::Status status = next_query_->Next();
    if (!status.ok())
      return status;
  }
  return util::OkStatus();
}

SpanJoinOperatorTable::Query*
SpanJoinOperatorTable::Cursor::FindEarliestFinishQuery() {
  int64_t t1_part;
  int64_t t2_part;

  switch (table_->partitioning_) {
    case PartitioningType::kMixedPartitioning: {
      // If either table is EOF, forward the other table to try and make
      // the partitions not match anymore.
      if (t1_.IsEof())
        return &t2_;
      if (t2_.IsEof())
        return &t1_;

      // Otherwise, just make the partition equal from both tables.
      t1_part = last_mixed_partition_;
      t2_part = last_mixed_partition_;
      break;
    }
    case PartitioningType::kSamePartitioning: {
      // Get the partition values from the cursor.
      t1_part = t1_.LastPartition();
      t2_part = t2_.LastPartition();
      break;
    }
    case PartitioningType::kNoPartitioning: {
      t1_part = 0;
      t2_part = 0;
      break;
    }
  }

  // Prefer to forward the earliest cursors based on the following
  // lexiographical ordering:
  // 1. partition
  // 2. end timestamp
  // 3. whether the slice is real or shadow (shadow < real)
  bool t1_less = std::make_tuple(t1_part, t1_.AdjustedTsEnd(), t1_.IsReal()) <
                 std::make_tuple(t2_part, t2_.AdjustedTsEnd(), t2_.IsReal());
  return t1_less ? &t1_ : &t2_;
}

int SpanJoinOperatorTable::Cursor::Eof() {
  return t1_.IsEof() || t2_.IsEof();
}

int SpanJoinOperatorTable::Cursor::Column(sqlite3_context* context, int N) {
  PERFETTO_DCHECK(t1_.IsReal() || t2_.IsReal());

  switch (N) {
    case Column::kTimestamp: {
      auto max_ts = std::max(t1_.ts(), t2_.ts());
      sqlite3_result_int64(context, static_cast<sqlite3_int64>(max_ts));
      break;
    }
    case Column::kDuration: {
      auto max_start = std::max(t1_.ts(), t2_.ts());
      auto min_end = std::min(t1_.raw_ts_end(), t2_.raw_ts_end());
      auto dur = min_end - max_start;
      sqlite3_result_int64(context, static_cast<sqlite3_int64>(dur));
      break;
    }
    case Column::kPartition: {
      if (table_->partitioning_ != PartitioningType::kNoPartitioning) {
        int64_t partition;
        if (table_->partitioning_ == PartitioningType::kMixedPartitioning) {
          partition = last_mixed_partition_;
        } else {
          partition = t1_.IsReal() ? t1_.partition() : t2_.partition();
        }
        sqlite3_result_int64(context, static_cast<sqlite3_int64>(partition));
        break;
      }
      [[clang::fallthrough]];
    }
    default: {
      size_t index = static_cast<size_t>(N);
      const auto& locator = table_->global_index_to_column_locator_[index];
      if (locator.defn == t1_.definition())
        t1_.ReportSqliteResult(context, locator.col_index);
      else
        t2_.ReportSqliteResult(context, locator.col_index);
    }
  }
  return SQLITE_OK;
}

SpanJoinOperatorTable::Query::Query(SpanJoinOperatorTable* table,
                                    const TableDefinition* definition,
                                    sqlite3* db)
    : defn_(definition), db_(db), table_(table) {
  PERFETTO_DCHECK(!defn_->IsPartitioned() ||
                  defn_->partition_idx() < defn_->columns().size());
}

SpanJoinOperatorTable::Query::~Query() = default;

util::Status SpanJoinOperatorTable::Query::Initialize(
    const QueryConstraints& qc,
    sqlite3_value** argv) {
  *this = Query(table_, definition(), db_);
  sql_query_ = CreateSqlQuery(
      table_->ComputeSqlConstraintsForDefinition(*defn_, qc, argv));
  return Rewind();
}

util::Status SpanJoinOperatorTable::Query::Next() {
  util::Status status = NextSliceState();
  if (!status.ok())
    return status;
  return FindNextValidSlice();
}

bool SpanJoinOperatorTable::Query::IsValidSlice() {
  // Disallow any single partition shadow slices if the definition doesn't allow
  // them.
  if (IsPresentPartitionShadow() && !defn_->ShouldEmitPresentPartitionShadow())
    return false;

  // Disallow any missing partition shadow slices if the definition doesn't
  // allow them.
  if (IsMissingPartitionShadow() && !defn_->ShouldEmitMissingPartitionShadow())
    return false;

  // Disallow any "empty" shadows; these are shadows which either have the same
  // start and end time or missing-partition shadows which have the same start
  // and end partition.
  if (IsEmptyShadow())
    return false;

  return true;
}

util::Status SpanJoinOperatorTable::Query::FindNextValidSlice() {
  // The basic idea of this function is that |NextSliceState()| always emits
  // all possible slices (including shadows for any gaps inbetween the real
  // slices) and we filter out the invalid slices (as defined by the table
  // definition) using |IsValidSlice()|.
  //
  // This has proved to be a lot cleaner to implement than trying to choose
  // when to emit and not emit shadows directly.
  while (!IsEof() && !IsValidSlice()) {
    util::Status status = NextSliceState();
    if (!status.ok())
      return status;
  }
  return util::OkStatus();
}

util::Status SpanJoinOperatorTable::Query::NextSliceState() {
  switch (state_) {
    case State::kReal: {
      // Forward the cursor to figure out where the next slice should be.
      util::Status status = CursorNext();
      if (!status.ok())
        return status;

      // Depending on the next slice, we can do two things here:
      // 1. If the next slice is on the same partition, we can just emit a
      //    single shadow until the start of the next slice.
      // 2. If the next slice is on another partition or we hit eof, just emit
      //    a shadow to the end of the whole partition.
      bool shadow_to_end = cursor_eof_ || (defn_->IsPartitioned() &&
                                           partition_ != CursorPartition());
      state_ = State::kPresentPartitionShadow;
      ts_ = AdjustedTsEnd();
      ts_end_ =
          shadow_to_end ? std::numeric_limits<int64_t>::max() : CursorTs();
      return util::OkStatus();
    }
    case State::kPresentPartitionShadow: {
      if (ts_end_ == std::numeric_limits<int64_t>::max()) {
        // If the shadow is to the end of the slice, create a missing partition
        // shadow to the start of the partition of the next slice or to the max
        // partition if we hit eof.
        state_ = State::kMissingPartitionShadow;
        ts_ = 0;
        ts_end_ = std::numeric_limits<int64_t>::max();

        missing_partition_start_ = partition_ + 1;
        missing_partition_end_ = cursor_eof_
                                     ? std::numeric_limits<int64_t>::max()
                                     : CursorPartition();
      } else {
        // If the shadow is not to the end, we must have another slice on the
        // current partition.
        state_ = State::kReal;
        ts_ = CursorTs();
        ts_end_ = ts_ + CursorDur();

        PERFETTO_DCHECK(!defn_->IsPartitioned() ||
                        partition_ == CursorPartition());
      }
      return util::OkStatus();
    }
    case State::kMissingPartitionShadow: {
      if (missing_partition_end_ == std::numeric_limits<int64_t>::max()) {
        PERFETTO_DCHECK(cursor_eof_);

        // If we have a missing partition to the max partition, we must have hit
        // eof.
        state_ = State::kEof;
      } else {
        PERFETTO_DCHECK(!defn_->IsPartitioned() ||
                        CursorPartition() == missing_partition_end_);

        // Otherwise, setup a single partition slice on the end partition to the
        // start of the next slice.
        state_ = State::kPresentPartitionShadow;
        ts_ = 0;
        ts_end_ = CursorTs();
        partition_ = missing_partition_end_;
      }
      return util::OkStatus();
    }
    case State::kEof: {
      PERFETTO_DFATAL("Called Next when EOF");
      return util::ErrStatus("Called Next when EOF");
    }
  }
  PERFETTO_FATAL("For GCC");
}

util::Status SpanJoinOperatorTable::Query::Rewind() {
  sqlite3_stmt* stmt = nullptr;
  int res =
      sqlite3_prepare_v2(db_, sql_query_.c_str(),
                         static_cast<int>(sql_query_.size()), &stmt, nullptr);
  stmt_.reset(stmt);

  cursor_eof_ = res != SQLITE_OK;
  if (res != SQLITE_OK)
    return util::ErrStatus("%s", sqlite3_errmsg(db_));

  util::Status status = CursorNext();
  if (!status.ok())
    return status;

  // Setup the first slice as a missing partition shadow from the lowest
  // partition until the first slice partition. We will handle finding the real
  // slice in |FindNextValidSlice()|.
  state_ = State::kMissingPartitionShadow;
  ts_ = 0;
  ts_end_ = std::numeric_limits<int64_t>::max();
  missing_partition_start_ = std::numeric_limits<int64_t>::min();

  if (cursor_eof_) {
    missing_partition_end_ = std::numeric_limits<int64_t>::max();
  } else if (defn_->IsPartitioned()) {
    missing_partition_end_ = CursorPartition();
  } else {
    missing_partition_end_ = std::numeric_limits<int64_t>::min();
  }

  // Actually compute the first valid slice.
  return FindNextValidSlice();
}

util::Status SpanJoinOperatorTable::Query::CursorNext() {
  auto* stmt = stmt_.get();
  int res;
  if (defn_->IsPartitioned()) {
    auto partition_idx = static_cast<int>(defn_->partition_idx());
    // Fastforward through any rows with null partition keys.
    int row_type;
    do {
      res = sqlite3_step(stmt);
      row_type = sqlite3_column_type(stmt, partition_idx);
    } while (res == SQLITE_ROW && row_type == SQLITE_NULL);
  } else {
    res = sqlite3_step(stmt);
  }
  cursor_eof_ = res != SQLITE_ROW;
  return res == SQLITE_ROW || res == SQLITE_DONE
             ? util::OkStatus()
             : util::ErrStatus("%s", sqlite3_errmsg(db_));
}

std::string SpanJoinOperatorTable::Query::CreateSqlQuery(
    const std::vector<std::string>& cs) const {
  std::vector<std::string> col_names;
  for (const SqliteTable::Column& c : defn_->columns()) {
    col_names.push_back("`" + c.name() + "`");
  }

  std::string sql = "SELECT " + base::Join(col_names, ", ");
  sql += " FROM " + defn_->name();
  if (!cs.empty()) {
    sql += " WHERE " + base::Join(cs, " AND ");
  }
  sql += " ORDER BY ";
  sql += defn_->IsPartitioned()
             ? base::Join({"`" + defn_->partition_col() + "`", "ts"}, ", ")
             : "ts";
  sql += ";";
  PERFETTO_DLOG("%s", sql.c_str());
  return sql;
}

void SpanJoinOperatorTable::Query::ReportSqliteResult(sqlite3_context* context,
                                                      size_t index) {
  if (state_ != State::kReal) {
    sqlite3_result_null(context);
    return;
  }

  sqlite3_stmt* stmt = stmt_.get();
  int idx = static_cast<int>(index);
  switch (sqlite3_column_type(stmt, idx)) {
    case SQLITE_INTEGER:
      sqlite3_result_int64(context, sqlite3_column_int64(stmt, idx));
      break;
    case SQLITE_FLOAT:
      sqlite3_result_double(context, sqlite3_column_double(stmt, idx));
      break;
    case SQLITE_TEXT: {
      // TODO(lalitm): note for future optimizations: if we knew the addresses
      // of the string intern pool, we could check if the string returned here
      // comes from the pool, and pass it as non-transient.
      const auto kSqliteTransient =
          reinterpret_cast<sqlite3_destructor_type>(-1);
      auto ptr = reinterpret_cast<const char*>(sqlite3_column_text(stmt, idx));
      sqlite3_result_text(context, ptr, -1, kSqliteTransient);
      break;
    }
  }
}

SpanJoinOperatorTable::TableDefinition::TableDefinition(
    std::string name,
    std::string partition_col,
    std::vector<SqliteTable::Column> cols,
    EmitShadowType emit_shadow_type,
    uint32_t ts_idx,
    uint32_t dur_idx,
    uint32_t partition_idx)
    : emit_shadow_type_(emit_shadow_type),
      name_(std::move(name)),
      partition_col_(std::move(partition_col)),
      cols_(std::move(cols)),
      ts_idx_(ts_idx),
      dur_idx_(dur_idx),
      partition_idx_(partition_idx) {}

util::Status SpanJoinOperatorTable::TableDescriptor::Parse(
    const std::string& raw_descriptor,
    SpanJoinOperatorTable::TableDescriptor* descriptor) {
  // Descriptors have one of the following forms:
  // table_name [PARTITIONED column_name]

  // Find the table name.
  base::StringSplitter splitter(raw_descriptor, ' ');
  if (!splitter.Next())
    return util::ErrStatus("SPAN_JOIN: Missing table name");

  descriptor->name = splitter.cur_token();
  if (!splitter.Next())
    return util::OkStatus();

  if (!base::CaseInsensitiveEqual(splitter.cur_token(), "PARTITIONED"))
    return util::ErrStatus("SPAN_JOIN: Invalid token");

  if (!splitter.Next())
    return util::ErrStatus("SPAN_JOIN: Missing partitioning column");

  descriptor->partition_col = splitter.cur_token();
  return util::OkStatus();
}

}  // namespace trace_processor
}  // namespace perfetto
