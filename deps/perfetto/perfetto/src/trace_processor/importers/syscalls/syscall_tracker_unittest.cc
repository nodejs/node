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

#include "src/trace_processor/importers/syscalls/syscall_tracker.h"

#include "src/trace_processor/importers/common/slice_tracker.h"
#include "test/gtest_and_gmock.h"

namespace perfetto {
namespace trace_processor {
namespace {

using ::testing::_;
using ::testing::Return;
using ::testing::SaveArg;

class MockSliceTracker : public SliceTracker {
 public:
  MockSliceTracker(TraceProcessorContext* context) : SliceTracker(context) {}
  virtual ~MockSliceTracker() = default;

  MOCK_METHOD5(Begin,
               base::Optional<uint32_t>(int64_t timestamp,
                                        TrackId track_id,
                                        StringId cat,
                                        StringId name,
                                        SetArgsCallback args_callback));
  MOCK_METHOD5(End,
               base::Optional<uint32_t>(int64_t timestamp,
                                        TrackId track_id,
                                        StringId cat,
                                        StringId name,
                                        SetArgsCallback args_callback));
};

class SyscallTrackerTest : public ::testing::Test {
 public:
  SyscallTrackerTest() {
    context.storage.reset(new TraceStorage());
    track_tracker = new TrackTracker(&context);
    context.track_tracker.reset(track_tracker);
    slice_tracker = new MockSliceTracker(&context);
    context.slice_tracker.reset(slice_tracker);
  }

 protected:
  TraceProcessorContext context;
  MockSliceTracker* slice_tracker;
  TrackTracker* track_tracker;
};

TEST_F(SyscallTrackerTest, ReportUnknownSyscalls) {
  constexpr TrackId track{0u};
  StringId begin_name = kNullStringId;
  StringId end_name = kNullStringId;
  EXPECT_CALL(*slice_tracker, Begin(100, track, kNullStringId, _, _))
      .WillOnce(DoAll(SaveArg<3>(&begin_name), Return(base::nullopt)));
  EXPECT_CALL(*slice_tracker, End(110, track, kNullStringId, _, _))
      .WillOnce(DoAll(SaveArg<3>(&end_name), Return(base::nullopt)));

  SyscallTracker* syscall_tracker = SyscallTracker::GetOrCreate(&context);
  syscall_tracker->Enter(100 /*ts*/, 42 /*utid*/, 57 /*sys_read*/);
  syscall_tracker->Exit(110 /*ts*/, 42 /*utid*/, 57 /*sys_read*/);
  EXPECT_EQ(context.storage->GetString(begin_name), "sys_57");
  EXPECT_EQ(context.storage->GetString(end_name), "sys_57");
}

TEST_F(SyscallTrackerTest, IgnoreWriteSyscalls) {
  SyscallTracker* syscall_tracker = SyscallTracker::GetOrCreate(&context);
  syscall_tracker->SetArchitecture(kAarch64);
  EXPECT_CALL(*slice_tracker, Begin(_, _, _, _, _)).Times(0);
  EXPECT_CALL(*slice_tracker, End(_, _, _, _, _)).Times(0);

  syscall_tracker->Enter(100 /*ts*/, 42 /*utid*/, 64 /*sys_write*/);
  syscall_tracker->Exit(110 /*ts*/, 42 /*utid*/, 64 /*sys_write*/);
}

TEST_F(SyscallTrackerTest, Aarch64) {
  constexpr TrackId track{0u};
  StringId begin_name = kNullStringId;
  StringId end_name = kNullStringId;
  EXPECT_CALL(*slice_tracker, Begin(100, track, kNullStringId, _, _))
      .WillOnce(DoAll(SaveArg<3>(&begin_name), Return(base::nullopt)));
  EXPECT_CALL(*slice_tracker, End(110, track, kNullStringId, _, _))
      .WillOnce(DoAll(SaveArg<3>(&end_name), Return(base::nullopt)));

  SyscallTracker* syscall_tracker = SyscallTracker::GetOrCreate(&context);
  syscall_tracker->SetArchitecture(kAarch64);
  syscall_tracker->Enter(100 /*ts*/, 42 /*utid*/, 63 /*sys_read*/);
  syscall_tracker->Exit(110 /*ts*/, 42 /*utid*/, 63 /*sys_read*/);
  EXPECT_EQ(context.storage->GetString(begin_name), "sys_read");
  EXPECT_EQ(context.storage->GetString(end_name), "sys_read");
}

TEST_F(SyscallTrackerTest, x8664) {
  constexpr TrackId track{0u};
  StringId begin_name = kNullStringId;
  StringId end_name = kNullStringId;
  EXPECT_CALL(*slice_tracker, Begin(100, track, kNullStringId, _, _))
      .WillOnce(DoAll(SaveArg<3>(&begin_name), Return(base::nullopt)));
  EXPECT_CALL(*slice_tracker, End(110, track, kNullStringId, _, _))
      .WillOnce(DoAll(SaveArg<3>(&end_name), Return(base::nullopt)));

  SyscallTracker* syscall_tracker = SyscallTracker::GetOrCreate(&context);
  syscall_tracker->SetArchitecture(kX86_64);
  syscall_tracker->Enter(100 /*ts*/, 42 /*utid*/, 0 /*sys_read*/);
  syscall_tracker->Exit(110 /*ts*/, 42 /*utid*/, 0 /*sys_read*/);
  EXPECT_EQ(context.storage->GetString(begin_name), "sys_read");
  EXPECT_EQ(context.storage->GetString(end_name), "sys_read");
}

TEST_F(SyscallTrackerTest, SyscallNumberTooLarge) {
  EXPECT_CALL(*slice_tracker, Begin(_, _, _, _, _)).Times(0);
  EXPECT_CALL(*slice_tracker, End(_, _, _, _, _)).Times(0);

  SyscallTracker* syscall_tracker = SyscallTracker::GetOrCreate(&context);
  syscall_tracker->SetArchitecture(kAarch64);
  syscall_tracker->Enter(100 /*ts*/, 42 /*utid*/, 9999);
  syscall_tracker->Exit(110 /*ts*/, 42 /*utid*/, 9999);
}

}  // namespace
}  // namespace trace_processor
}  // namespace perfetto
