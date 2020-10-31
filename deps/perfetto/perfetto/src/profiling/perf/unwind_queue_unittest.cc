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

#include "src/profiling/perf/unwind_queue.h"

#include "test/gtest_and_gmock.h"

namespace perfetto {
namespace profiling {
namespace {

TEST(UnwindQueueTest, SinglePass) {
  static constexpr uint32_t kCapacity = 4;
  UnwindQueue<int, kCapacity> queue;

  // write kCapacity entries
  for (int i = 0; i < static_cast<int>(kCapacity); i++) {
    WriteView v = queue.BeginWrite();
    ASSERT_TRUE(v.valid);
    queue.at(v.write_pos) = i;
    queue.CommitWrite();
  }
  {
    // no more available capacity
    WriteView v = queue.BeginWrite();
    ASSERT_FALSE(v.valid);
  }

  // reader sees all four writes
  ReadView v = queue.BeginRead();
  ASSERT_EQ(v.read_pos, 0u);
  ASSERT_EQ(v.write_pos, 4u);

  std::vector<int> read_back;
  for (auto pos = v.read_pos; pos < v.write_pos; pos++) {
    read_back.push_back(queue.at(pos));
  }
  queue.CommitNewReadPosition(v.write_pos);

  ASSERT_THAT(read_back, ::testing::ElementsAre(0, 1, 2, 3));

  // writer sees an available slot
  ASSERT_TRUE(queue.BeginWrite().valid);
  // reader caught up
  ASSERT_TRUE(queue.BeginRead().read_pos == queue.BeginRead().write_pos);
}

TEST(UnwindQueueTest, Wrapped) {
  static constexpr uint32_t kCapacity = 4;
  UnwindQueue<int, kCapacity> queue;

  // write kCapacity entries
  for (int i = 0; i < static_cast<int>(kCapacity); i++) {
    WriteView v = queue.BeginWrite();
    ASSERT_TRUE(v.valid);
    queue.at(v.write_pos) = i;
    queue.CommitWrite();
  }

  // no more available capacity
  ASSERT_FALSE(queue.BeginWrite().valid);

  {
    // consume 2 entries (partial read)
    ReadView v = queue.BeginRead();
    ASSERT_EQ(v.read_pos, 0u);
    ASSERT_EQ(v.write_pos, 4u);
    queue.CommitNewReadPosition(v.read_pos + 2);
  }

  // write 2 more entries
  for (int i = 0; i < 2; i++) {
    WriteView v = queue.BeginWrite();
    ASSERT_TRUE(v.valid);
    queue.at(v.write_pos) = 4 + i;
    queue.CommitWrite();
  }

  // no more available capacity
  ASSERT_FALSE(queue.BeginWrite().valid);

  // read the remainder of the buffer
  ReadView v = queue.BeginRead();
  ASSERT_EQ(v.read_pos, 2u);
  ASSERT_EQ(v.write_pos, 6u);

  std::vector<int> read_back;
  for (auto pos = v.read_pos; pos < v.write_pos; pos++) {
    read_back.push_back(queue.at(pos));
  }
  queue.CommitNewReadPosition(v.write_pos);

  ASSERT_THAT(read_back, ::testing::ElementsAre(2, 3, 4, 5));

  // writer sees an available slot
  ASSERT_TRUE(queue.BeginWrite().valid);
  // reader caught up
  ASSERT_TRUE(queue.BeginRead().read_pos == queue.BeginRead().write_pos);
}

}  // namespace
}  // namespace profiling
}  // namespace perfetto
