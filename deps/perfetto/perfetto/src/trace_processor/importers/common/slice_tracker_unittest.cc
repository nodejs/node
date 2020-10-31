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

#include <vector>

#include "src/trace_processor/importers/common/args_tracker.h"
#include "src/trace_processor/importers/common/slice_tracker.h"
#include "src/trace_processor/storage/trace_storage.h"
#include "src/trace_processor/types/trace_processor_context.h"
#include "test/gtest_and_gmock.h"

namespace perfetto {
namespace trace_processor {
namespace {

using ::testing::ElementsAre;
using ::testing::Eq;

struct SliceInfo {
  int64_t start;
  int64_t duration;

  bool operator==(const SliceInfo& other) const {
    return std::tie(start, duration) == std::tie(other.start, other.duration);
  }
};

inline void PrintTo(const SliceInfo& info, ::std::ostream* os) {
  *os << "SliceInfo{" << info.start << ", " << info.duration << "}";
}

std::vector<SliceInfo> ToSliceInfo(const tables::SliceTable& slices) {
  std::vector<SliceInfo> infos;
  for (uint32_t i = 0; i < slices.row_count(); i++) {
    infos.emplace_back(SliceInfo{slices.ts()[i], slices.dur()[i]});
  }
  return infos;
}

TEST(SliceTrackerTest, OneSliceDetailed) {
  TraceProcessorContext context;
  context.storage.reset(new TraceStorage());
  SliceTracker tracker(&context);

  constexpr TrackId track{22u};
  tracker.Begin(2 /*ts*/, track, kNullStringId /*cat*/,
                StringId::Raw(1) /*name*/);
  tracker.End(10 /*ts*/, track, kNullStringId /*cat*/,
              StringId::Raw(1) /*name*/);

  const auto& slices = context.storage->slice_table();
  EXPECT_EQ(slices.row_count(), 1u);
  EXPECT_EQ(slices.ts()[0], 2);
  EXPECT_EQ(slices.dur()[0], 8);
  EXPECT_EQ(slices.track_id()[0], track);
  EXPECT_EQ(slices.category()[0].raw_id(), 0u);
  EXPECT_EQ(slices.name()[0].raw_id(), 1u);
  EXPECT_EQ(slices.depth()[0], 0u);
  EXPECT_EQ(slices.arg_set_id()[0], kInvalidArgSetId);
}

TEST(SliceTrackerTest, OneSliceWithArgs) {
  TraceProcessorContext context;
  context.storage.reset(new TraceStorage());
  context.global_args_tracker.reset(new GlobalArgsTracker(&context));
  SliceTracker tracker(&context);

  constexpr TrackId track{22u};
  tracker.Begin(2 /*ts*/, track, kNullStringId /*cat*/,
                StringId::Raw(1) /*name*/,
                [](ArgsTracker::BoundInserter* inserter) {
                  inserter->AddArg(/*flat_key=*/StringId::Raw(1),
                                   /*key=*/StringId::Raw(2),
                                   /*value=*/Variadic::Integer(10));
                });
  tracker.End(10 /*ts*/, track, kNullStringId /*cat*/,
              StringId::Raw(1) /*name*/,
              [](ArgsTracker::BoundInserter* inserter) {
                inserter->AddArg(/*flat_key=*/StringId::Raw(3),
                                 /*key=*/StringId::Raw(4),
                                 /*value=*/Variadic::Integer(20));
              });

  const auto& slices = context.storage->slice_table();
  EXPECT_EQ(slices.row_count(), 1u);
  EXPECT_EQ(slices.ts()[0], 2);
  EXPECT_EQ(slices.dur()[0], 8);
  EXPECT_EQ(slices.track_id()[0], track);
  EXPECT_EQ(slices.category()[0].raw_id(), 0u);
  EXPECT_EQ(slices.name()[0].raw_id(), 1u);
  EXPECT_EQ(slices.depth()[0], 0u);
  auto set_id = slices.arg_set_id()[0];

  const auto& args = context.storage->arg_table();
  EXPECT_EQ(args.arg_set_id()[0], set_id);
  EXPECT_EQ(args.flat_key()[0].raw_id(), 1u);
  EXPECT_EQ(args.key()[0].raw_id(), 2u);
  EXPECT_EQ(args.int_value()[0], 10);
  EXPECT_EQ(args.arg_set_id()[1], set_id);
  EXPECT_EQ(args.flat_key()[1].raw_id(), 3u);
  EXPECT_EQ(args.key()[1].raw_id(), 4u);
  EXPECT_EQ(args.int_value()[1], 20);
}

TEST(SliceTrackerTest, TwoSliceDetailed) {
  TraceProcessorContext context;
  context.storage.reset(new TraceStorage());
  SliceTracker tracker(&context);

  constexpr TrackId track{22u};
  tracker.Begin(2 /*ts*/, track, kNullStringId /*cat*/,
                StringId::Raw(1) /*name*/);
  tracker.Begin(3 /*ts*/, track, kNullStringId /*cat*/,
                StringId::Raw(2) /*name*/);
  tracker.End(5 /*ts*/, track);
  tracker.End(10 /*ts*/, track);

  const auto& slices = context.storage->slice_table();

  EXPECT_EQ(slices.row_count(), 2u);

  uint32_t idx = 0;
  EXPECT_EQ(slices.ts()[idx], 2);
  EXPECT_EQ(slices.dur()[idx], 8);
  EXPECT_EQ(slices.track_id()[idx], track);
  EXPECT_EQ(slices.category()[idx].raw_id(), 0u);
  EXPECT_EQ(slices.name()[idx].raw_id(), 1u);
  EXPECT_EQ(slices.depth()[idx++], 0u);

  EXPECT_EQ(slices.ts()[idx], 3);
  EXPECT_EQ(slices.dur()[idx], 2);
  EXPECT_EQ(slices.track_id()[idx], track);
  EXPECT_EQ(slices.category()[idx].raw_id(), 0u);
  EXPECT_EQ(slices.name()[idx].raw_id(), 2u);
  EXPECT_EQ(slices.depth()[idx], 1u);

  EXPECT_EQ(slices.parent_stack_id()[0], 0);
  EXPECT_EQ(slices.stack_id()[0], slices.parent_stack_id()[1]);
  EXPECT_NE(slices.stack_id()[1], 0);
}

TEST(SliceTrackerTest, Scoped) {
  TraceProcessorContext context;
  context.storage.reset(new TraceStorage());
  SliceTracker tracker(&context);

  constexpr TrackId track{22u};
  tracker.Begin(0 /*ts*/, track, kNullStringId, kNullStringId);
  tracker.Begin(1 /*ts*/, track, kNullStringId, kNullStringId);
  tracker.Scoped(2 /*ts*/, track, kNullStringId, kNullStringId, 6);
  tracker.End(9 /*ts*/, track);
  tracker.End(10 /*ts*/, track);

  auto slices = ToSliceInfo(context.storage->slice_table());
  EXPECT_THAT(slices,
              ElementsAre(SliceInfo{0, 10}, SliceInfo{1, 8}, SliceInfo{2, 6}));
}

TEST(SliceTrackerTest, ParentId) {
  TraceProcessorContext context;
  context.storage.reset(new TraceStorage());
  SliceTracker tracker(&context);

  constexpr TrackId track{22u};
  tracker.Begin(100, track, kNullStringId, kNullStringId);
  tracker.Begin(101, track, kNullStringId, kNullStringId);
  tracker.Begin(102, track, kNullStringId, kNullStringId);
  tracker.End(103, track);
  tracker.End(150, track);
  tracker.End(200, track);

  SliceId parent = context.storage->slice_table().id()[0];
  SliceId child = context.storage->slice_table().id()[1];
  EXPECT_THAT(context.storage->slice_table().parent_id().ToVectorForTesting(),
              ElementsAre(base::nullopt, parent, child));
}

TEST(SliceTrackerTest, IgnoreMismatchedEnds) {
  TraceProcessorContext context;
  context.storage.reset(new TraceStorage());
  SliceTracker tracker(&context);

  constexpr TrackId track{22u};
  tracker.Begin(2 /*ts*/, track, StringId::Raw(5) /*cat*/,
                StringId::Raw(1) /*name*/);
  tracker.End(3 /*ts*/, track, StringId::Raw(1) /*cat*/,
              StringId::Raw(1) /*name*/);
  tracker.End(4 /*ts*/, track, kNullStringId /*cat*/,
              StringId::Raw(2) /*name*/);
  tracker.End(5 /*ts*/, track, StringId::Raw(5) /*cat*/,
              StringId::Raw(1) /*name*/);

  auto slices = ToSliceInfo(context.storage->slice_table());
  EXPECT_THAT(slices, ElementsAre(SliceInfo{2, 3}));
}

TEST(SliceTrackerTest, ZeroLengthScoped) {
  TraceProcessorContext context;
  context.storage.reset(new TraceStorage());
  SliceTracker tracker(&context);

  // Bug scenario: the second zero-length scoped slice prevents the first slice
  // from being closed, leading to an inconsistency when we try to insert the
  // final slice and it doesn't intersect with the still pending first slice.
  constexpr TrackId track{22u};
  tracker.Scoped(2 /*ts*/, track, kNullStringId /*cat*/,
                 StringId::Raw(1) /*name*/, 10 /* dur */);
  tracker.Scoped(2 /*ts*/, track, kNullStringId /*cat*/,
                 StringId::Raw(1) /*name*/, 0 /* dur */);
  tracker.Scoped(12 /*ts*/, track, kNullStringId /*cat*/,
                 StringId::Raw(1) /*name*/, 1 /* dur */);
  tracker.Scoped(13 /*ts*/, track, kNullStringId /*cat*/,
                 StringId::Raw(1) /*name*/, 1 /* dur */);

  auto slices = ToSliceInfo(context.storage->slice_table());
  EXPECT_THAT(slices, ElementsAre(SliceInfo{2, 10}, SliceInfo{2, 0},
                                  SliceInfo{12, 1}, SliceInfo{13, 1}));
}

TEST(SliceTrackerTest, DifferentTracks) {
  TraceProcessorContext context;
  context.storage.reset(new TraceStorage());
  SliceTracker tracker(&context);

  constexpr TrackId track_a{22u};
  constexpr TrackId track_b{23u};
  tracker.Begin(0 /*ts*/, track_a, kNullStringId, kNullStringId);
  tracker.Scoped(2 /*ts*/, track_b, kNullStringId, kNullStringId, 6);
  tracker.Scoped(3 /*ts*/, track_b, kNullStringId, kNullStringId, 4);
  tracker.End(10 /*ts*/, track_a);
  tracker.FlushPendingSlices();

  auto slices = ToSliceInfo(context.storage->slice_table());
  EXPECT_THAT(slices,
              ElementsAre(SliceInfo{0, 10}, SliceInfo{2, 6}, SliceInfo{3, 4}));

  EXPECT_EQ(context.storage->slice_table().track_id()[0], track_a);
  EXPECT_EQ(context.storage->slice_table().track_id()[1], track_b);
  EXPECT_EQ(context.storage->slice_table().track_id()[2], track_b);
  EXPECT_EQ(context.storage->slice_table().depth()[0], 0u);
  EXPECT_EQ(context.storage->slice_table().depth()[1], 0u);
  EXPECT_EQ(context.storage->slice_table().depth()[2], 1u);
}

TEST(SliceTrackerTest, EndEventOutOfOrder) {
  TraceProcessorContext context;
  context.storage.reset(new TraceStorage());
  SliceTracker tracker(&context);

  constexpr TrackId track{22u};
  tracker.Scoped(50 /*ts*/, track, StringId::Raw(11) /*cat*/,
                 StringId::Raw(21) /*name*/, 100 /*dur*/);
  tracker.Begin(100 /*ts*/, track, StringId::Raw(12) /*cat*/,
                StringId::Raw(22) /*name*/);

  // This slice should now have depth 0.
  tracker.Scoped(450 /*ts*/, track, StringId::Raw(12) /*cat*/,
                 StringId::Raw(22) /*name*/, 100 /*dur*/);

  // This slice should be ignored.
  tracker.End(500 /*ts*/, track, StringId::Raw(12) /*cat*/,
              StringId::Raw(22) /*name*/);

  tracker.Begin(800 /*ts*/, track, StringId::Raw(13) /*cat*/,
                StringId::Raw(23) /*name*/);
  // Null cat and name matches everything.
  tracker.End(1000 /*ts*/, track, kNullStringId /*cat*/,
              kNullStringId /*name*/);

  // Slice will not close if category is different.
  tracker.Begin(1100 /*ts*/, track, StringId::Raw(11) /*cat*/,
                StringId::Raw(21) /*name*/);
  tracker.End(1200 /*ts*/, track, StringId::Raw(12) /*cat*/,
              StringId::Raw(21) /*name*/);

  // Slice will not close if name is different.
  tracker.Begin(1300 /*ts*/, track, StringId::Raw(11) /*cat*/,
                StringId::Raw(21) /*name*/);
  tracker.End(1400 /*ts*/, track, StringId::Raw(11) /*cat*/,
              StringId::Raw(22) /*name*/);

  tracker.FlushPendingSlices();

  auto slices = ToSliceInfo(context.storage->slice_table());
  EXPECT_THAT(slices, ElementsAre(SliceInfo{50, 100}, SliceInfo{100, 50},
                                  SliceInfo{450, 100}, SliceInfo{800, 200},
                                  SliceInfo{1100, -1}, SliceInfo{1300, 0 - 1}));

  EXPECT_EQ(context.storage->slice_table().depth()[0], 0u);
  EXPECT_EQ(context.storage->slice_table().depth()[1], 1u);
  EXPECT_EQ(context.storage->slice_table().depth()[2], 0u);
  EXPECT_EQ(context.storage->slice_table().depth()[3], 0u);
}

}  // namespace
}  // namespace trace_processor
}  // namespace perfetto
