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

#ifndef SRC_TRACE_PROCESSOR_SQLITE_QUERY_CONSTRAINTS_H_
#define SRC_TRACE_PROCESSOR_SQLITE_QUERY_CONSTRAINTS_H_

#include <sqlite3.h>

#include <vector>

#include "perfetto/ext/base/scoped_file.h"

namespace perfetto {
namespace trace_processor {

// This class stores the constraints (including the order-by information) for
// a query on a sqlite3 virtual table and handles their de/serialization into
// strings.
// This is because the constraint columns and the order-by clauses are passed
// to the xBestIndex method but the constraint values are available only in the
// xFilter method. Unfortunately sqlite vtable API don't give any hint about
// the validity of the constraints (i.e. constraints passed to xBestIndex can
// be used by future xFilter calls in the far future). The only mechanism
// offered by sqlite is the idxStr string which is returned by the vtable
// in the xBestIndex call and passed to each corresponding xFilter call.
class QueryConstraints {
 public:
  struct Constraint {
    // Column this constraint refers to.
    int column;

    // SQLite op for the constraint.
    int op;

    // The original index of this constraint in the aConstraint array.
    // Used internally by SqliteTable for xBestIndex - this should not be
    // read or modified by subclasses of SqliteTable.
    int a_constraint_idx;
  };

  using OrderBy = sqlite3_index_info::sqlite3_index_orderby;

  static int FreeSqliteString(char* resource);

  using SqliteString = base::ScopedResource<char*, FreeSqliteString, nullptr>;

  QueryConstraints();
  ~QueryConstraints();
  QueryConstraints(QueryConstraints&&) noexcept;
  QueryConstraints& operator=(QueryConstraints&&);

  // Two QueryConstraints with the same constraint and orderby vectors
  // are equal.
  bool operator==(const QueryConstraints& other) const;

  void AddConstraint(int column, unsigned char op, int aconstraint_idx);

  void AddOrderBy(int column, unsigned char desc);

  void ClearOrderBy() { order_by_.clear(); }

  // Converts the constraints and order by information to a string for
  // use by sqlite.
  SqliteString ToNewSqlite3String() const;

  // Deserializes the string into QueryConstraints. String given is in the form
  // C{# of constraints},col1,op1,col2,op2...,O{# of order by},col1,desc1...
  // For example C1,0,3,O2,1,0,4,1
  static QueryConstraints FromString(const char* idxStr);

  const std::vector<OrderBy>& order_by() const { return order_by_; }

  const std::vector<Constraint>& constraints() const { return constraints_; }

  std::vector<OrderBy>* mutable_order_by() { return &order_by_; }

  std::vector<Constraint>* mutable_constraints() { return &constraints_; }

 private:
  QueryConstraints(const QueryConstraints&) = delete;
  QueryConstraints& operator=(const QueryConstraints&) = delete;

  std::vector<OrderBy> order_by_;
  std::vector<Constraint> constraints_;
};

}  // namespace trace_processor
}  // namespace perfetto

#endif  // SRC_TRACE_PROCESSOR_SQLITE_QUERY_CONSTRAINTS_H_
