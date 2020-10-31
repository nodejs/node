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

#include <array>
#include <chrono>
#include <deque>
#include <thread>

#include "perfetto/ext/base/metatrace.h"
#include "src/base/test/test_task_runner.h"
#include "test/gtest_and_gmock.h"

namespace perfetto {
namespace {

namespace m = ::perfetto::metatrace;
using ::testing::Invoke;

class MetatraceTest : public ::testing::Test {
 public:
  void SetUp() override { m::Disable(); }

  void TearDown() override {
    task_runner_.RunUntilIdle();
    m::Disable();
  }

  void Enable(uint32_t tags) {
    m::Enable([this] { ReadCallback(); }, &task_runner_, tags);
  }

  MOCK_METHOD0(ReadCallback, void());
  base::TestTaskRunner task_runner_;
};

TEST_F(MetatraceTest, TagEnablingLogic) {
  EXPECT_CALL(*this, ReadCallback()).Times(0);
  for (int iteration = 0; iteration < 3; iteration++) {
    ASSERT_EQ(m::RingBuffer::GetSizeForTesting(), 0u);

    // No events should be traced before enabling.
    m::TraceCounter(m::TAG_ANY, /*id=*/1, /*value=*/42);
    { m::ScopedEvent evt(m::TAG_ANY, /*id=*/1); }
    ASSERT_EQ(m::RingBuffer::GetSizeForTesting(), 0u);

    // Enable tags bit 1 (=2) and 2 (=4) and verify that only those events are
    // added.
    auto t_start = metatrace::TraceTimeNowNs();
    Enable(/*tags=*/2 | 4);
    m::TraceCounter(/*tag=*/1, /*id=*/42, /*value=*/10);      // No.
    m::TraceCounter(/*tag=*/2, /*id=*/42, /*value=*/11);      // Yes.
    m::TraceCounter(/*tag=*/4, /*id=*/42, /*value=*/12);      // Yes.
    m::TraceCounter(/*tag=*/1 | 2, /*id=*/42, /*value=*/13);  // Yes.
    m::TraceCounter(/*tag=*/1 | 4, /*id=*/42, /*value=*/14);  // Yes.
    m::TraceCounter(/*tag=*/2 | 4, /*id=*/42, /*value=*/15);  // Yes.
    m::TraceCounter(/*tag=*/4 | 8, /*id=*/42, /*value=*/16);  // Yes.
    m::TraceCounter(/*tag=*/1 | 8, /*id=*/42, /*value=*/17);  // No.
    m::TraceCounter(m::TAG_ANY, /*id=*/42, /*value=*/18);     // Yes.
    { m::ScopedEvent evt(/*tag=*/1, /*id=*/20); }             // No.
    { m::ScopedEvent evt(/*tag=*/8, /*id=*/21); }             // No.
    { m::ScopedEvent evt(/*tag=*/2, /*id=*/22); }             // Yes.
    { m::ScopedEvent evt(/*tag=*/4 | 8, /*id=*/23); }         // Yes.
    { m::ScopedEvent evt(m::TAG_ANY, /*id=*/24); }            // Yes.

    {
      auto it = m::RingBuffer::GetReadIterator();
      ASSERT_TRUE(it);
      ASSERT_EQ(it->counter_value, 11);
      ASSERT_TRUE(++it);
      ASSERT_EQ(it->counter_value, 12);
      ASSERT_TRUE(++it);
      ASSERT_EQ(it->counter_value, 13);
      ASSERT_TRUE(++it);
      ASSERT_EQ(it->counter_value, 14);
    }

    // Test that destroying and re-creating the iterator resumes reading from
    // the right place.
    {
      auto it = m::RingBuffer::GetReadIterator();
      ASSERT_TRUE(++it);
      ASSERT_EQ(it->counter_value, 15);
      ASSERT_TRUE(++it);
      ASSERT_EQ(it->counter_value, 16);
      ASSERT_TRUE(++it);
      ASSERT_EQ(it->counter_value, 18);
      ASSERT_TRUE(++it);
      ASSERT_EQ(it->type_and_id, 22);
      ASSERT_TRUE(++it);
      ASSERT_EQ(it->type_and_id, 23);
      ASSERT_TRUE(++it);
      ASSERT_EQ(it->type_and_id, 24);
      ASSERT_FALSE(++it);
    }

    // Test that we can write pids up to 32 bit TIDs (I observed up to 262144
    // from /proc/sys/kernel/pid_max) and up to 2 days of timestamps.
    {
      auto* record = m::RingBuffer::AppendNewRecord();
      record->counter_value = 42;
      constexpr uint64_t kTwoDays = 48ULL * 3600 * 1000 * 1000 * 1000;
      record->set_timestamp(t_start + kTwoDays);
      record->thread_id = 0xbabaf00d;
      record->type_and_id = m::Record::kTypeCounter;

      auto it = m::RingBuffer::GetReadIterator();
      ASSERT_TRUE(it);
      ASSERT_EQ(it->timestamp_ns(), t_start + kTwoDays);
      ASSERT_EQ(it->thread_id, 0xbabaf00d);
      ASSERT_FALSE(++it);
    }

    m::Disable();
  }
}

// Test that overruns are handled properly and that the writer re-synchronizes
// after the reader catches up.
TEST_F(MetatraceTest, HandleOverruns) {
  int cnt = 0;
  int exp_cnt = 0;
  for (size_t iteration = 0; iteration < 3; iteration++) {
    Enable(m::TAG_ANY);
    std::string checkpoint_name = "ReadTask " + std::to_string(iteration);
    auto checkpoint = task_runner_.CreateCheckpoint(checkpoint_name);
    EXPECT_CALL(*this, ReadCallback()).WillOnce(Invoke(checkpoint));

    for (size_t i = 0; i < m::RingBuffer::kCapacity; i++)
      m::TraceCounter(/*tag=*/1, /*id=*/42, /*value=*/cnt++);
    ASSERT_EQ(m::RingBuffer::GetSizeForTesting(), m::RingBuffer::kCapacity);
    ASSERT_FALSE(m::RingBuffer::has_overruns());

    for (int n = 0; n < 3; n++)
      m::TraceCounter(/*tag=*/1, /*id=*/42, /*value=*/-1);  // Will overrun.

    ASSERT_TRUE(m::RingBuffer::has_overruns());
    ASSERT_EQ(m::RingBuffer::GetSizeForTesting(), m::RingBuffer::kCapacity);

    for (auto it = m::RingBuffer::GetReadIterator(); it; ++it)
      ASSERT_EQ(it->counter_value, exp_cnt++);

    ASSERT_EQ(m::RingBuffer::GetSizeForTesting(), 0u);

    task_runner_.RunUntilCheckpoint(checkpoint_name);
    m::Disable();
  }
}

// Sets up a scenario where the writer writes constantly (however, guaranteeing
// to not overrun) and the reader catches up. Tests that all events are seen
// consistently without gaps.
TEST_F(MetatraceTest, InterleavedReadWrites) {
  Enable(m::TAG_ANY);
  constexpr int kMaxValue = m::RingBuffer::kCapacity * 10;

  std::atomic<int> last_value_read{-1};
  auto read_task = [&last_value_read] {
    int last = last_value_read;
    for (auto it = m::RingBuffer::GetReadIterator(); it; ++it) {
      if (it->type_and_id.load(std::memory_order_acquire) == 0)
        break;
      EXPECT_EQ(it->counter_value, last + 1);
      last = it->counter_value;
    }
    // The read pointer is incremented only after destroying the iterator.
    // Publish the last read value after the loop.
    last_value_read = last;
  };

  EXPECT_CALL(*this, ReadCallback()).WillRepeatedly(Invoke(read_task));

  // The writer will write continuously counters from 0 to kMaxValue.
  auto writer_done = task_runner_.CreateCheckpoint("writer_done");
  std::thread writer_thread([this, &writer_done, &last_value_read] {
    for (int i = 0; i < kMaxValue; i++) {
      m::TraceCounter(/*tag=*/1, /*id=*/1, i);
      const int kCapacity = static_cast<int>(m::RingBuffer::kCapacity);

      // Wait for the reader to avoid overruns.
      while (i - last_value_read >= kCapacity - 1)
        std::this_thread::sleep_for(std::chrono::nanoseconds(1));
    }
    task_runner_.PostTask(writer_done);
  });

  task_runner_.RunUntilCheckpoint("writer_done");
  writer_thread.join();

  read_task();  // Do a final read pass.
  EXPECT_FALSE(m::RingBuffer::has_overruns());
  EXPECT_EQ(last_value_read, kMaxValue - 1);
}

// Try to hit potential thread races:
// - Test that the read callback is posted only once per cycle.
// - Test that the final size of the ring buffeer is sane.
// - Test that event records are consistent within each thread's event stream.
TEST_F(MetatraceTest, ThreadRaces) {
  for (size_t iteration = 0; iteration < 10; iteration++) {
    Enable(m::TAG_ANY);

    std::string checkpoint_name = "ReadTask " + std::to_string(iteration);
    auto checkpoint = task_runner_.CreateCheckpoint(checkpoint_name);
    EXPECT_CALL(*this, ReadCallback()).WillOnce(Invoke(checkpoint));

    auto thread_main = [](uint16_t thd_idx) {
      for (size_t i = 0; i < m::RingBuffer::kCapacity + 500; i++)
        m::TraceCounter(/*tag=*/1, thd_idx, static_cast<int>(i));
    };

    constexpr size_t kNumThreads = 8;
    std::array<std::thread, kNumThreads> threads;
    for (size_t thd_idx = 0; thd_idx < kNumThreads; thd_idx++)
      threads[thd_idx] = std::thread(thread_main, thd_idx);

    for (auto& t : threads)
      t.join();

    task_runner_.RunUntilCheckpoint(checkpoint_name);
    ASSERT_EQ(m::RingBuffer::GetSizeForTesting(), m::RingBuffer::kCapacity);

    std::array<int, kNumThreads> last_val{};  // Last value for each thread.
    for (auto it = m::RingBuffer::GetReadIterator(); it; ++it) {
      if (it->type_and_id.load(std::memory_order_acquire) == 0)
        break;
      using Record = m::Record;
      ASSERT_EQ(it->type_and_id & Record::kTypeMask, Record::kTypeCounter);
      auto thd_idx = static_cast<size_t>(it->type_and_id & ~Record::kTypeMask);
      ASSERT_EQ(it->counter_value, last_val[thd_idx]);
      last_val[thd_idx]++;
    }

    m::Disable();
  }
}

}  // namespace
}  // namespace perfetto
