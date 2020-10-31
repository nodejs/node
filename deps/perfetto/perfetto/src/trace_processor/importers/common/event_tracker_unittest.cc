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

#include "src/trace_processor/importers/common/event_tracker.h"

#include "perfetto/base/logging.h"
#include "src/trace_processor/importers/common/args_tracker.h"
#include "src/trace_processor/importers/common/process_tracker.h"
#include "src/trace_processor/importers/common/track_tracker.h"
#include "test/gtest_and_gmock.h"

namespace perfetto {
namespace trace_processor {
namespace {

using ::testing::_;
using ::testing::InSequence;
using ::testing::Invoke;

class EventTrackerTest : public ::testing::Test {
 public:
  EventTrackerTest() {
    context.storage.reset(new TraceStorage());
    context.global_args_tracker.reset(new GlobalArgsTracker(&context));
    context.args_tracker.reset(new ArgsTracker(&context));
    context.process_tracker.reset(new ProcessTracker(&context));
    context.event_tracker.reset(new EventTracker(&context));
    context.track_tracker.reset(new TrackTracker(&context));
  }

 protected:
  TraceProcessorContext context;
};

TEST_F(EventTrackerTest, CounterDuration) {
  uint32_t cpu = 3;
  int64_t timestamp = 100;
  StringId name_id = kNullStringId;

  TrackId track = context.track_tracker->InternCpuCounterTrack(name_id, cpu);
  context.event_tracker->PushCounter(timestamp, 1000, track);
  context.event_tracker->PushCounter(timestamp + 1, 4000, track);
  context.event_tracker->PushCounter(timestamp + 3, 5000, track);
  context.event_tracker->PushCounter(timestamp + 9, 1000, track);

  ASSERT_EQ(context.storage->counter_track_table().row_count(), 1ul);

  ASSERT_EQ(context.storage->counter_table().row_count(), 4ul);
  ASSERT_EQ(context.storage->counter_table().ts()[0], timestamp);
  ASSERT_DOUBLE_EQ(context.storage->counter_table().value()[0], 1000);

  ASSERT_EQ(context.storage->counter_table().ts()[1], timestamp + 1);
  ASSERT_DOUBLE_EQ(context.storage->counter_table().value()[1], 4000);

  ASSERT_EQ(context.storage->counter_table().ts()[2], timestamp + 3);
  ASSERT_DOUBLE_EQ(context.storage->counter_table().value()[2], 5000);
}

}  // namespace
}  // namespace trace_processor
}  // namespace perfetto
