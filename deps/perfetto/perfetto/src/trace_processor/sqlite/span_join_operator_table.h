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

#ifndef SRC_TRACE_PROCESSOR_SQLITE_SPAN_JOIN_OPERATOR_TABLE_H_
#define SRC_TRACE_PROCESSOR_SQLITE_SPAN_JOIN_OPERATOR_TABLE_H_

#include <sqlite3.h>

#include <array>
#include <deque>
#include <limits>
#include <map>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "perfetto/trace_processor/basic_types.h"
#include "perfetto/trace_processor/status.h"
#include "src/trace_processor/sqlite/scoped_db.h"
#include "src/trace_processor/sqlite/sqlite_table.h"

namespace perfetto {
namespace trace_processor {

// Implements the SPAN JOIN operation between two tables on a particular column.
//
// Span:
// A span is a row with a timestamp and a duration. It can is used to model
// operations which run for a particular *span* of time.
//
// We draw spans like so (time on the x-axis):
// start of span->[ time where opertion is running ]<- end of span
//
// Multiple spans can happen in parallel:
// [      ]
//    [        ]
//   [                    ]
//  [ ]
//
// The above for example, models scheduling activity on a 4-core computer for a
// short period of time.
//
// Span join:
// The span join operation can be thought of as the intersection of span tables.
// That is, the join table has a span for each pair of spans in the child tables
// where the spans overlap. Because many spans are possible in parallel, an
// extra metadata column (labelled the "join column") is used to distinguish
// between the spanned tables.
//
// For a given join key suppose these were the two span tables:
// Table 1:   [        ]              [      ]         [ ]
// Table 2:          [      ]            [  ]           [      ]
// Output :          [ ]                 [  ]           []
//
// All other columns apart from timestamp (ts), duration (dur) and the join key
// are passed through unchanged.
class SpanJoinOperatorTable : public SqliteTable {
 public:
  // Enum indicating whether the queries on the two inner tables should
  // emit shadows.
  enum class EmitShadowType {
    // Used when the table should emit all shadow slices (both present and
    // missing partition shadows).
    kAll,

    // Used when the table should only emit shadows for partitions which are
    // present.
    kPresentPartitionOnly,

    // Used when the table should emit no shadow slices.
    kNone,
  };

  // Contains the definition of the child tables.
  class TableDefinition {
   public:
    TableDefinition() = default;

    TableDefinition(std::string name,
                    std::string partition_col,
                    std::vector<SqliteTable::Column> cols,
                    EmitShadowType emit_shadow_type,
                    uint32_t ts_idx,
                    uint32_t dur_idx,
                    uint32_t partition_idx);

    // Returns whether this table should emit present partition shadow slices.
    bool ShouldEmitPresentPartitionShadow() const {
      return emit_shadow_type_ == EmitShadowType::kAll ||
             emit_shadow_type_ == EmitShadowType::kPresentPartitionOnly;
    }

    // Returns whether this table should emit missing partition shadow slices.
    bool ShouldEmitMissingPartitionShadow() const {
      return emit_shadow_type_ == EmitShadowType::kAll;
    }

    // Returns whether the table is partitioned.
    bool IsPartitioned() const { return !partition_col_.empty(); }

    const std::string& name() const { return name_; }
    const std::string& partition_col() const { return partition_col_; }
    const std::vector<SqliteTable::Column>& columns() const { return cols_; }

    uint32_t ts_idx() const { return ts_idx_; }
    uint32_t dur_idx() const { return dur_idx_; }
    uint32_t partition_idx() const { return partition_idx_; }

   private:
    EmitShadowType emit_shadow_type_ = EmitShadowType::kNone;

    std::string name_;
    std::string partition_col_;
    std::vector<SqliteTable::Column> cols_;

    uint32_t ts_idx_ = std::numeric_limits<uint32_t>::max();
    uint32_t dur_idx_ = std::numeric_limits<uint32_t>::max();
    uint32_t partition_idx_ = std::numeric_limits<uint32_t>::max();
  };

  // Stores information about a single subquery into one of the two child
  // tables.
  //
  // This class is implemented as a state machine which steps from one slice to
  // the next.
  class Query {
   public:
    // Enum encoding the current state of the query in the state machine.
    enum class State {
      // Encodes that the current slice is a real slice (i.e. comes directly
      // from the cursor).
      kReal,

      // Encodes that the current slice is on a partition for which there is a
      // real slice present.
      kPresentPartitionShadow,

      // Encodes that the current slice is on a paritition(s) for which there is
      // no real slice for those partition(s).
      kMissingPartitionShadow,

      // Encodes that this query has reached the end.
      kEof,
    };

    Query(SpanJoinOperatorTable*, const TableDefinition*, sqlite3* db);
    virtual ~Query();

    Query(Query&&) noexcept = default;
    Query& operator=(Query&&) = default;

    // Initializes the query with the given constraints and query parameters.
    util::Status Initialize(const QueryConstraints& qc, sqlite3_value** argv);

    // Forwards the query to the next valid slice.
    util::Status Next();

    // Rewinds the query to the first valid slice
    // This is used in the mixed partitioning case where the query with no
    // partitions is rewound to the start on every new partition.
    util::Status Rewind();

    // Reports the column at the given index to given context.
    void ReportSqliteResult(sqlite3_context* context, size_t index);

    // Returns whether the cursor has reached eof.
    bool IsEof() const { return state_ == State::kEof; }

    // Returns whether the current slice pointed to is a real slice.
    bool IsReal() const { return state_ == State::kReal; }

    // Returns the first partition this slice covers (for real/single partition
    // shadows, this is the same as partition()).
    // This partition encodes a [start, end] (closed at start and at end) range
    // of partitions which works as the partitions are integers.
    int64_t FirstPartition() const {
      PERFETTO_DCHECK(!IsEof());
      return IsMissingPartitionShadow() ? missing_partition_start_
                                        : partition();
    }

    // Returns the last partition this slice covers (for real/single partition
    // shadows, this is the same as partition()).
    // This partition encodes a [start, end] (closed at start and at end) range
    // of partitions which works as the partitions are integers.
    int64_t LastPartition() const {
      PERFETTO_DCHECK(!IsEof());
      return IsMissingPartitionShadow() ? missing_partition_end_ - 1
                                        : partition();
    }

    // Returns the end timestamp of this slice adjusted to ensure that -1
    // duration slices always returns ts.
    int64_t AdjustedTsEnd() const {
      PERFETTO_DCHECK(!IsEof());
      return ts_end_ - ts() == -1 ? ts() : ts_end_;
    }

    int64_t ts() const {
      PERFETTO_DCHECK(!IsEof());
      return ts_;
    }
    int64_t partition() const {
      PERFETTO_DCHECK(!IsEof() && defn_->IsPartitioned());
      return partition_;
    }

    int64_t raw_ts_end() const {
      PERFETTO_DCHECK(!IsEof());
      return ts_end_;
    }

    const TableDefinition* definition() const { return defn_; }

   private:
    Query(Query&) = delete;
    Query& operator=(const Query&) = delete;

    // Returns whether the current slice pointed to is a valid slice.
    bool IsValidSlice();

    // Forwards the query to the next valid slice.
    util::Status FindNextValidSlice();

    // Advances the query state machine by one slice.
    util::Status NextSliceState();

    // Forwards the cursor to point to the next real slice.
    util::Status CursorNext();

    // Creates an SQL query from the given set of constraint strings.
    std::string CreateSqlQuery(const std::vector<std::string>& cs) const;

    // Returns whether the current slice pointed to is a present partition
    // shadow.
    bool IsPresentPartitionShadow() const {
      return state_ == State::kPresentPartitionShadow;
    }

    // Returns whether the current slice pointed to is a missing partition
    // shadow.
    bool IsMissingPartitionShadow() const {
      return state_ == State::kMissingPartitionShadow;
    }

    // Returns whether the current slice pointed to is an empty shadow.
    bool IsEmptyShadow() const {
      PERFETTO_DCHECK(!IsEof());
      return (!IsReal() && ts_ == ts_end_) ||
             (IsMissingPartitionShadow() &&
              missing_partition_start_ == missing_partition_end_);
    }

    int64_t CursorTs() const {
      PERFETTO_DCHECK(!cursor_eof_);
      auto ts_idx = static_cast<int>(defn_->ts_idx());
      return sqlite3_column_int64(stmt_.get(), ts_idx);
    }

    int64_t CursorDur() const {
      PERFETTO_DCHECK(!cursor_eof_);
      auto dur_idx = static_cast<int>(defn_->dur_idx());
      return sqlite3_column_int64(stmt_.get(), dur_idx);
    }

    int64_t CursorPartition() const {
      PERFETTO_DCHECK(!cursor_eof_);
      PERFETTO_DCHECK(defn_->IsPartitioned());
      auto partition_idx = static_cast<int>(defn_->partition_idx());
      return sqlite3_column_int64(stmt_.get(), partition_idx);
    }

    State state_ = State::kMissingPartitionShadow;
    bool cursor_eof_ = false;

    // Only valid when |state_| != kEof.
    int64_t ts_ = 0;
    int64_t ts_end_ = std::numeric_limits<int64_t>::max();

    // Only valid when |state_| == kReal or |state_| == kPresentPartitionShadow.
    int64_t partition_ = std::numeric_limits<int64_t>::min();

    // Only valid when |state_| == kMissingPartitionShadow.
    int64_t missing_partition_start_ = 0;
    int64_t missing_partition_end_ = 0;

    std::string sql_query_;
    ScopedStmt stmt_;

    const TableDefinition* defn_ = nullptr;
    sqlite3* db_ = nullptr;
    SpanJoinOperatorTable* table_ = nullptr;
  };

  // Base class for a cursor on the span table.
  class Cursor : public SqliteTable::Cursor {
   public:
    Cursor(SpanJoinOperatorTable*, sqlite3* db);
    ~Cursor() override = default;

    int Filter(const QueryConstraints& qc,
               sqlite3_value** argv,
               FilterHistory) override;
    int Column(sqlite3_context* context, int N) override;
    int Next() override;
    int Eof() override;

   private:
    Cursor(Cursor&) = delete;
    Cursor& operator=(const Cursor&) = delete;

    Cursor(Cursor&&) noexcept = default;
    Cursor& operator=(Cursor&&) = default;

    bool IsOverlappingSpan();

    util::Status FindOverlappingSpan();

    Query* FindEarliestFinishQuery();

    Query t1_;
    Query t2_;

    Query* next_query_ = nullptr;

    // Only valid for kMixedPartition.
    int64_t last_mixed_partition_ = std::numeric_limits<int64_t>::min();

    SpanJoinOperatorTable* table_;
  };

  SpanJoinOperatorTable(sqlite3*, const TraceStorage*);

  static void RegisterTable(sqlite3* db, const TraceStorage* storage);

  // Table implementation.
  util::Status Init(int, const char* const*, SqliteTable::Schema*) override;
  std::unique_ptr<SqliteTable::Cursor> CreateCursor() override;
  int BestIndex(const QueryConstraints& qc, BestIndexInfo* info) override;

 private:
  // Columns of the span operator table.
  enum Column {
    kTimestamp = 0,
    kDuration = 1,
    kPartition = 2,
    // All other columns are dynamic depending on the joined tables.
  };

  // Enum indicating the possible partitionings of the two tables in span join.
  enum class PartitioningType {
    // Used when both tables don't have a partition specified.
    kNoPartitioning = 0,

    // Used when both tables have the same partition specified.
    kSamePartitioning = 1,

    // Used when one table has a partition and the other table doesn't.
    kMixedPartitioning = 2
  };

  // Parsed version of a table descriptor.
  struct TableDescriptor {
    static util::Status Parse(const std::string& raw_descriptor,
                              TableDescriptor* descriptor);

    bool IsPartitioned() const { return !partition_col.empty(); }

    std::string name;
    std::string partition_col;
  };

  // Identifier for a column by index in a given table.
  struct ColumnLocator {
    const TableDefinition* defn;
    size_t col_index;
  };

  bool IsLeftJoin() const { return name() == "span_left_join"; }
  bool IsOuterJoin() const { return name() == "span_outer_join"; }

  const std::string& partition_col() const {
    return t1_defn_.IsPartitioned() ? t1_defn_.partition_col()
                                    : t2_defn_.partition_col();
  }

  util::Status CreateTableDefinition(
      const TableDescriptor& desc,
      EmitShadowType emit_shadow_type,
      SpanJoinOperatorTable::TableDefinition* defn);

  std::vector<std::string> ComputeSqlConstraintsForDefinition(
      const TableDefinition& defn,
      const QueryConstraints& qc,
      sqlite3_value** argv);

  std::string GetNameForGlobalColumnIndex(const TableDefinition& defn,
                                          int global_column);

  void CreateSchemaColsForDefn(const TableDefinition& defn,
                               std::vector<SqliteTable::Column>* cols);

  TableDefinition t1_defn_;
  TableDefinition t2_defn_;
  PartitioningType partitioning_;
  std::unordered_map<size_t, ColumnLocator> global_index_to_column_locator_;

  sqlite3* const db_;
};

}  // namespace trace_processor
}  // namespace perfetto

#endif  // SRC_TRACE_PROCESSOR_SQLITE_SPAN_JOIN_OPERATOR_TABLE_H_
