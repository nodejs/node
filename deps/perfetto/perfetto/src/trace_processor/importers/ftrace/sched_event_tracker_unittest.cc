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

#include "src/trace_processor/importers/ftrace/sched_event_tracker.h"

#include "perfetto/base/logging.h"
#include "src/trace_processor/importers/common/args_tracker.h"
#include "src/trace_processor/importers/common/event_tracker.h"
#include "src/trace_processor/importers/common/process_tracker.h"
#include "test/gtest_and_gmock.h"

namespace perfetto {
namespace trace_processor {
namespace {

using ::testing::_;
using ::testing::InSequence;
using ::testing::Invoke;

class SchedEventTrackerTest : public ::testing::Test {
 public:
  SchedEventTrackerTest() {
    context.storage.reset(new TraceStorage());
    context.global_args_tracker.reset(new GlobalArgsTracker(&context));
    context.args_tracker.reset(new ArgsTracker(&context));
    context.event_tracker.reset(new EventTracker(&context));
    context.process_tracker.reset(new ProcessTracker(&context));
    sched_tracker = SchedEventTracker::GetOrCreate(&context);
  }

 protected:
  TraceProcessorContext context;
  SchedEventTracker* sched_tracker;
};

TEST_F(SchedEventTrackerTest, InsertSecondSched) {
  uint32_t cpu = 3;
  int64_t timestamp = 100;
  uint32_t pid_1 = 2;
  int64_t prev_state = 32;
  static const char kCommProc1[] = "process1";
  static const char kCommProc2[] = "process2";
  uint32_t pid_2 = 4;
  int32_t prio = 1024;

  sched_tracker->PushSchedSwitch(cpu, timestamp, pid_1, kCommProc2, prio,
                                 prev_state, pid_2, kCommProc1, prio);
  ASSERT_EQ(context.storage->sched_slice_table().row_count(), 1ul);

  sched_tracker->PushSchedSwitch(cpu, timestamp + 1, pid_2, kCommProc1, prio,
                                 prev_state, pid_1, kCommProc2, prio);

  ASSERT_EQ(context.storage->sched_slice_table().row_count(), 2ul);

  const auto& timestamps = context.storage->sched_slice_table().ts();
  ASSERT_EQ(timestamps[0], timestamp);
  ASSERT_EQ(context.storage->thread_table().start_ts()[1], base::nullopt);

  auto name =
      context.storage->GetString(context.storage->thread_table().name()[1]);
  ASSERT_STREQ(name.c_str(), kCommProc1);
  ASSERT_EQ(context.storage->sched_slice_table().utid()[0], 1u);
  ASSERT_EQ(context.storage->sched_slice_table().dur()[0], 1);
}

TEST_F(SchedEventTrackerTest, InsertThirdSched_SameThread) {
  uint32_t cpu = 3;
  int64_t timestamp = 100;
  int64_t prev_state = 32;
  static const char kCommProc1[] = "process1";
  static const char kCommProc2[] = "process2";
  int32_t prio = 1024;

  sched_tracker->PushSchedSwitch(cpu, timestamp, /*tid=*/4, kCommProc2, prio,
                                 prev_state,
                                 /*tid=*/2, kCommProc1, prio);
  ASSERT_EQ(context.storage->sched_slice_table().row_count(), 1u);

  sched_tracker->PushSchedSwitch(cpu, timestamp + 1, /*tid=*/2, kCommProc1,
                                 prio, prev_state,
                                 /*tid=*/4, kCommProc2, prio);
  sched_tracker->PushSchedSwitch(cpu, timestamp + 11, /*tid=*/4, kCommProc2,
                                 prio, prev_state,
                                 /*tid=*/2, kCommProc1, prio);
  sched_tracker->PushSchedSwitch(cpu, timestamp + 31, /*tid=*/2, kCommProc1,
                                 prio, prev_state,
                                 /*tid=*/4, kCommProc2, prio);
  ASSERT_EQ(context.storage->sched_slice_table().row_count(), 4ul);

  const auto& timestamps = context.storage->sched_slice_table().ts();
  ASSERT_EQ(timestamps[0], timestamp);
  ASSERT_EQ(context.storage->thread_table().start_ts()[1], base::nullopt);
  ASSERT_EQ(context.storage->sched_slice_table().dur()[0], 1u);
  ASSERT_EQ(context.storage->sched_slice_table().dur()[1], 11u - 1u);
  ASSERT_EQ(context.storage->sched_slice_table().dur()[2], 31u - 11u);
  ASSERT_EQ(context.storage->sched_slice_table().utid()[0],
            context.storage->sched_slice_table().utid()[2]);
}

TEST_F(SchedEventTrackerTest, UpdateThreadMatch) {
  uint32_t cpu = 3;
  int64_t timestamp = 100;
  int64_t prev_state = 32;
  static const char kCommProc1[] = "process1";
  static const char kCommProc2[] = "process2";
  int32_t prio = 1024;

  sched_tracker->PushSchedSwitch(cpu, timestamp, /*tid=*/1, kCommProc2, prio,
                                 prev_state,
                                 /*tid=*/4, kCommProc1, prio);
  sched_tracker->PushSchedSwitch(cpu, timestamp + 1, /*tid=*/4, kCommProc1,
                                 prio, prev_state,
                                 /*tid=*/1, kCommProc2, prio);

  context.process_tracker->SetProcessMetadata(2, base::nullopt, "test");
  context.process_tracker->UpdateThread(4, 2);

  ASSERT_EQ(context.storage->thread_table().tid()[1], 4u);
  ASSERT_EQ(context.storage->thread_table().upid()[1].value(), 1u);
  ASSERT_EQ(context.storage->process_table().pid()[1], 2u);
  ASSERT_EQ(context.storage->process_table().start_ts()[1], base::nullopt);
}

}  // namespace
}  // namespace trace_processor
}  // namespace perfetto
