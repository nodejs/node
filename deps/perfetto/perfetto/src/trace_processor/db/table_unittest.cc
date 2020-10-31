/*
 * Copyright (C) 2020 The Android Open Source Project
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

#include "src/trace_processor/db/table.h"
#include "perfetto/ext/base/optional.h"
#include "src/trace_processor/db/typed_column.h"
#include "src/trace_processor/tables/macros.h"

#include "test/gtest_and_gmock.h"

namespace perfetto {
namespace trace_processor {
namespace {

constexpr uint32_t kColumnCount = 1024;

std::unique_ptr<SparseVector<int64_t>> Column() {
  auto c = std::unique_ptr<SparseVector<int64_t>>(new SparseVector<int64_t>());
  for (int64_t i = 0; i < kColumnCount; ++i)
    c->Append(i);
  return c;
}

uint32_t Flags() {
  return TypedColumn<int64_t>::default_flags();
}

#define PERFETTO_TP_TEST_EVENT_TABLE_DEF(NAME, PARENT, C) \
  NAME(TestEventTable, "event")                           \
  PARENT(PERFETTO_TP_ROOT_TABLE_PARENT_DEF, C)            \
  C(int64_t, ts, Column::Flag::kSorted)                   \
  C(int64_t, arg_set_id)
PERFETTO_TP_TABLE(PERFETTO_TP_TEST_EVENT_TABLE_DEF);

TestEventTable::~TestEventTable() = default;

TEST(TableTest, ExtendingTableTwice) {
  StringPool pool;
  TestEventTable table{&pool, nullptr};

  for (uint32_t i = 0; i < kColumnCount; ++i)
    table.Insert(TestEventTable::Row(i));

  Table filtered_table;
  {
    filtered_table = table.ExtendWithColumn("a", Column(), Flags())
                         .ExtendWithColumn("b", Column(), Flags())
                         .Filter({});
  }
  ASSERT_TRUE(filtered_table.GetColumnByName("a")->Max().has_value());
  ASSERT_TRUE(filtered_table.GetColumnByName("b")->Max().has_value());
}

}  // namespace
}  // namespace trace_processor
}  // namespace perfetto
