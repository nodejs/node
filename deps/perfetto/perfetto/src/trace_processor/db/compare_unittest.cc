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

#include "src/trace_processor/db/compare.h"
#include "test/gtest_and_gmock.h"

namespace perfetto {
namespace trace_processor {
namespace {

TEST(CompareTest, SqlValueDifferentTypes) {
  ASSERT_LT(compare::SqlValue(SqlValue(), SqlValue::Long(10)), 0);
  ASSERT_LT(compare::SqlValue(SqlValue::Double(10.0), SqlValue::String("10")),
            0);

  // Numerics should still compare equal even if they have different types.
  ASSERT_EQ(compare::SqlValue(SqlValue::Long(10), SqlValue::Double(10.0)), 0);
}

TEST(CompareTest, SqlValueLong) {
  SqlValue int32_min = SqlValue::Long(std::numeric_limits<int32_t>::min());
  SqlValue minus_1 = SqlValue::Long(-1);
  SqlValue zero = SqlValue::Long(0);
  SqlValue uint32_max = SqlValue::Long(std::numeric_limits<uint32_t>::max());

  ASSERT_LT(compare::SqlValue(int32_min, minus_1), 0);
  ASSERT_LT(compare::SqlValue(int32_min, uint32_max), 0);
  ASSERT_LT(compare::SqlValue(minus_1, uint32_max), 0);

  ASSERT_GT(compare::SqlValue(uint32_max, zero), 0);

  ASSERT_EQ(compare::SqlValue(zero, zero), 0);
}

TEST(CompareTest, SqlValueDouble) {
  SqlValue int32_min = SqlValue::Double(std::numeric_limits<int32_t>::min());
  SqlValue minus_1 = SqlValue::Double(-1.0);
  SqlValue zero = SqlValue::Double(0);
  SqlValue uint32_max = SqlValue::Double(std::numeric_limits<uint32_t>::max());

  ASSERT_LT(compare::SqlValue(int32_min, minus_1), 0);
  ASSERT_LT(compare::SqlValue(int32_min, uint32_max), 0);
  ASSERT_LT(compare::SqlValue(minus_1, uint32_max), 0);

  ASSERT_GT(compare::SqlValue(uint32_max, zero), 0);

  ASSERT_EQ(compare::SqlValue(zero, zero), 0);
}

TEST(CompareTest, SqlValueString) {
  SqlValue a = SqlValue::String("a");
  SqlValue aa = SqlValue::String("aa");
  SqlValue b = SqlValue::String("b");

  ASSERT_LT(compare::SqlValue(a, aa), 0);
  ASSERT_LT(compare::SqlValue(aa, b), 0);
  ASSERT_LT(compare::SqlValue(a, b), 0);

  ASSERT_GT(compare::SqlValue(aa, a), 0);

  ASSERT_EQ(compare::SqlValue(a, a), 0);
  ASSERT_EQ(compare::SqlValue(aa, SqlValue::String("aa")), 0);
}

}  // namespace
}  // namespace trace_processor
}  // namespace perfetto
