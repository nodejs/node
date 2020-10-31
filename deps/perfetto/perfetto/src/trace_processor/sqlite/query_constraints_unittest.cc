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

#include "perfetto/base/logging.h"
#include "test/gtest_and_gmock.h"

using testing::ElementsAreArray;
using testing::Field;
using testing::Matcher;
using testing::Matches;
using testing::Pointwise;

namespace perfetto {
namespace trace_processor {
namespace {

class QueryConstraintsTest : public ::testing::Test {
 public:
  QueryConstraintsTest() { PERFETTO_CHECK(sqlite3_initialize() == SQLITE_OK); }
};

TEST_F(QueryConstraintsTest, ConvertToAndFromSqlString) {
  QueryConstraints qc;
  qc.AddConstraint(12, 0, 0);

  QueryConstraints::SqliteString only_constraint = qc.ToNewSqlite3String();
  ASSERT_TRUE(strcmp(only_constraint.get(), "C1,12,0,O0") == 0);

  QueryConstraints qc_constraint =
      QueryConstraints::FromString(only_constraint.get());
  ASSERT_EQ(qc, qc_constraint);

  qc.AddOrderBy(1, false);
  qc.AddOrderBy(21, true);

  QueryConstraints::SqliteString result = qc.ToNewSqlite3String();
  ASSERT_TRUE(strcmp(result.get(), "C1,12,0,O2,1,0,21,1") == 0);

  QueryConstraints qc_result = QueryConstraints::FromString(result.get());
  ASSERT_EQ(qc, qc_result);
}

TEST_F(QueryConstraintsTest, CheckEmptyConstraints) {
  QueryConstraints qc;

  QueryConstraints::SqliteString string_result = qc.ToNewSqlite3String();
  ASSERT_TRUE(strcmp(string_result.get(), "C0,O0") == 0);

  QueryConstraints qc_result =
      QueryConstraints::FromString(string_result.get());
  ASSERT_EQ(qc_result.constraints().size(), 0u);
  ASSERT_EQ(qc_result.order_by().size(), 0u);
}

TEST_F(QueryConstraintsTest, OnlyOrderBy) {
  QueryConstraints qc;
  qc.AddOrderBy(3, true);

  QueryConstraints::SqliteString string_result = qc.ToNewSqlite3String();
  ASSERT_TRUE(strcmp(string_result.get(), "C0,O1,3,1") == 0);

  QueryConstraints qc_result =
      QueryConstraints::FromString(string_result.get());
  ASSERT_EQ(qc, qc_result);
}

}  // namespace
}  // namespace trace_processor
}  // namespace perfetto
