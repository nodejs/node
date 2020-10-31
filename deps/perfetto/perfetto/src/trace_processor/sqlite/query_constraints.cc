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

#include "src/trace_processor/sqlite/query_constraints.h"

#include <sqlite3.h>

#include <string>

#include "perfetto/ext/base/string_splitter.h"

namespace perfetto {
namespace trace_processor {

QueryConstraints::QueryConstraints() = default;
QueryConstraints::~QueryConstraints() = default;
QueryConstraints::QueryConstraints(QueryConstraints&&) noexcept = default;
QueryConstraints& QueryConstraints::operator=(QueryConstraints&&) = default;

int QueryConstraints::FreeSqliteString(char* resource) {
  sqlite3_free(resource);
  return 0;
}

bool QueryConstraints::operator==(const QueryConstraints& other) const {
  if ((other.constraints().size() != constraints().size()) ||
      (other.order_by().size() != order_by().size())) {
    return false;
  }

  for (size_t i = 0; i < constraints().size(); ++i) {
    if ((constraints()[i].column != other.constraints()[i].column) ||
        (constraints()[i].op != other.constraints()[i].op)) {
      return false;
    }
  }

  for (size_t i = 0; i < order_by().size(); ++i) {
    if ((order_by()[i].iColumn != other.order_by()[i].iColumn) ||
        (order_by()[i].desc != other.order_by()[i].desc)) {
      return false;
    }
  }

  return true;
}

void QueryConstraints::AddConstraint(int column,
                                     unsigned char op,
                                     int aconstraint_idx) {
  Constraint c{};
  c.column = column;
  c.op = op;
  c.a_constraint_idx = aconstraint_idx;
  constraints_.emplace_back(c);
}

void QueryConstraints::AddOrderBy(int column, unsigned char desc) {
  OrderBy ob{};
  ob.iColumn = column;
  ob.desc = desc;
  order_by_.emplace_back(ob);
}

QueryConstraints::SqliteString QueryConstraints::ToNewSqlite3String() const {
  std::string str_result;
  str_result.reserve(512);
  str_result.append("C");
  str_result.append(std::to_string(constraints_.size()));
  str_result.append(",");
  for (const auto& cs : constraints_) {
    str_result.append(std::to_string(cs.column));
    str_result.append(",");
    str_result.append(std::to_string(cs.op));
    str_result.append(",");
  }
  str_result.append("O");
  str_result.append(std::to_string(order_by_.size()));
  str_result.append(",");
  for (const auto& ob : order_by_) {
    str_result.append(std::to_string(ob.iColumn));
    str_result.append(",");
    str_result.append(std::to_string(ob.desc));
    str_result.append(",");
  }

  // The last char is a "," so overwriting with the null terminator on purpose.
  SqliteString result(
      static_cast<char*>(sqlite3_malloc(static_cast<int>(str_result.size()))));
  strncpy(result.get(), str_result.c_str(), str_result.size());
  (*result)[str_result.size() - 1] = '\0';

  return result;
}

QueryConstraints QueryConstraints::FromString(const char* idxStr) {
  QueryConstraints qc;

  base::StringSplitter splitter(std::string(idxStr), ',');

  PERFETTO_CHECK(splitter.Next() && splitter.cur_token_size() > 1);
  // The '+ 1' skips the letter 'C' in the first token.
  long num_constraints = strtol(splitter.cur_token() + 1, nullptr, 10);
  for (int i = 0; i < num_constraints; ++i) {
    PERFETTO_CHECK(splitter.Next());
    int col = static_cast<int>(strtol(splitter.cur_token(), nullptr, 10));
    PERFETTO_CHECK(splitter.Next());
    unsigned char op =
        static_cast<unsigned char>(strtol(splitter.cur_token(), nullptr, 10));
    qc.AddConstraint(col, op, 0);
  }

  PERFETTO_CHECK(splitter.Next() && splitter.cur_token_size() > 1);
  // The '+ 1' skips the letter 'O' in the current token.
  long num_order_by = strtol(splitter.cur_token() + 1, nullptr, 10);
  for (int i = 0; i < num_order_by; ++i) {
    PERFETTO_CHECK(splitter.Next());
    int col = static_cast<int>(strtol(splitter.cur_token(), nullptr, 10));
    PERFETTO_CHECK(splitter.Next());
    unsigned char desc =
        static_cast<unsigned char>(strtol(splitter.cur_token(), nullptr, 10));
    qc.AddOrderBy(col, desc);
  }

  PERFETTO_DCHECK(!splitter.Next());
  return qc;
}

}  // namespace trace_processor
}  // namespace perfetto
