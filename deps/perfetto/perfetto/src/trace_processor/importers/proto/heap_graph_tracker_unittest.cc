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

#include "src/trace_processor/importers/proto/heap_graph_tracker.h"

#include "perfetto/base/logging.h"
#include "test/gtest_and_gmock.h"

namespace perfetto {
namespace trace_processor {
namespace {

using ::testing::UnorderedElementsAre;

TEST(HeapGraphTrackerTest, PackageFromLocationApp) {
  TraceProcessorContext context;
  HeapGraphTracker tracker(&context);
  EXPECT_EQ(tracker.PackageFromLocation(
                "/data/app/~~ASDFGH1234QWerT==/"
                "com.twitter.android-MNBVCX7890SDTst6==/test.apk"),
            "com.twitter.android");
}

TEST(HeapGraphTrackerTest, BuildFlamegraph) {
  //           4@A 5@B
  //             \ /
  //         2@Y 3@Y
  //           \ /
  //           1@X

  constexpr uint64_t kSeqId = 1;
  constexpr UniquePid kPid = 1;
  constexpr int64_t kTimestamp = 1;

  TraceProcessorContext context;
  context.storage.reset(new TraceStorage());

  HeapGraphTracker tracker(&context);

  constexpr uint64_t kField = 1;

  constexpr uint64_t kX = 1;
  constexpr uint64_t kY = 2;
  constexpr uint64_t kA = 3;
  constexpr uint64_t kB = 4;

  base::StringView field = base::StringView("foo");
  StringPool::Id x = context.storage->InternString("X");
  StringPool::Id y = context.storage->InternString("Y");
  StringPool::Id a = context.storage->InternString("A");
  StringPool::Id b = context.storage->InternString("B");

  tracker.AddInternedFieldName(kSeqId, kField, field);

  tracker.AddInternedTypeName(kSeqId, kX, x);
  tracker.AddInternedTypeName(kSeqId, kY, y);
  tracker.AddInternedTypeName(kSeqId, kA, a);
  tracker.AddInternedTypeName(kSeqId, kB, b);

  {
    HeapGraphTracker::SourceObject obj;
    obj.object_id = 1;
    obj.self_size = 1;
    obj.type_id = kX;
    HeapGraphTracker::SourceObject::Reference ref;
    ref.field_name_id = kField;
    ref.owned_object_id = 2;
    obj.references.emplace_back(std::move(ref));

    ref.field_name_id = kField;
    ref.owned_object_id = 3;
    obj.references.emplace_back(std::move(ref));

    tracker.AddObject(kSeqId, kPid, kTimestamp, std::move(obj));
  }

  {
    HeapGraphTracker::SourceObject obj;
    obj.object_id = 2;
    obj.self_size = 2;
    obj.type_id = kY;
    tracker.AddObject(kSeqId, kPid, kTimestamp, std::move(obj));
  }

  {
    HeapGraphTracker::SourceObject obj;
    obj.object_id = 3;
    obj.self_size = 3;
    obj.type_id = kY;
    HeapGraphTracker::SourceObject::Reference ref;
    ref.field_name_id = kField;
    ref.owned_object_id = 4;
    obj.references.emplace_back(std::move(ref));

    ref.field_name_id = kField;
    ref.owned_object_id = 5;
    obj.references.emplace_back(std::move(ref));

    tracker.AddObject(kSeqId, kPid, kTimestamp, std::move(obj));
  }

  {
    HeapGraphTracker::SourceObject obj;
    obj.object_id = 4;
    obj.self_size = 4;
    obj.type_id = kA;
    tracker.AddObject(kSeqId, kPid, kTimestamp, std::move(obj));
  }

  {
    HeapGraphTracker::SourceObject obj;
    obj.object_id = 5;
    obj.self_size = 5;
    obj.type_id = kB;
    tracker.AddObject(kSeqId, kPid, kTimestamp, std::move(obj));
  }

  HeapGraphTracker::SourceRoot root;
  root.root_type = context.storage->InternString("ROOT");
  root.object_ids.emplace_back(1);
  tracker.AddRoot(kSeqId, kPid, kTimestamp, root);

  tracker.FinalizeProfile(kSeqId);
  std::unique_ptr<tables::ExperimentalFlamegraphNodesTable> flame =
      tracker.BuildFlamegraph(kPid, kTimestamp);
  ASSERT_NE(flame, nullptr);

  auto cumulative_sizes = flame->cumulative_size().ToVectorForTesting();
  EXPECT_THAT(cumulative_sizes, UnorderedElementsAre(15, 4, 14, 5));

  auto cumulative_counts = flame->cumulative_count().ToVectorForTesting();
  EXPECT_THAT(cumulative_counts, UnorderedElementsAre(5, 4, 1, 1));

  auto sizes = flame->size().ToVectorForTesting();
  EXPECT_THAT(sizes, UnorderedElementsAre(1, 5, 4, 5));

  auto counts = flame->count().ToVectorForTesting();
  EXPECT_THAT(counts, UnorderedElementsAre(1, 2, 1, 1));
}

static const char kArray[] = "X[]";
static const char kDoubleArray[] = "X[][]";
static const char kNoArray[] = "X";
static const char kLongNoArray[] = "ABCDE";
static const char kStaticClassNoArray[] = "java.lang.Class<abc>";
static const char kStaticClassArray[] = "java.lang.Class<abc[]>";

TEST(HeapGraphTrackerTest, NormalizeTypeName) {
  // sizeof(...) - 1 below to get rid of the null-byte.
  EXPECT_EQ(NormalizeTypeName(base::StringView(kArray, sizeof(kArray) - 1))
                .ToStdString(),
            "X");
  EXPECT_EQ(NormalizeTypeName(
                base::StringView(kDoubleArray, sizeof(kDoubleArray) - 1))
                .ToStdString(),
            "X");
  EXPECT_EQ(NormalizeTypeName(base::StringView(kNoArray, sizeof(kNoArray) - 1))
                .ToStdString(),
            "X");
  EXPECT_EQ(NormalizeTypeName(
                base::StringView(kLongNoArray, sizeof(kLongNoArray) - 1))
                .ToStdString(),
            "ABCDE");
  EXPECT_EQ(NormalizeTypeName(base::StringView(kStaticClassNoArray,
                                               sizeof(kStaticClassNoArray) - 1))
                .ToStdString(),
            "abc");
  EXPECT_EQ(NormalizeTypeName(base::StringView(kStaticClassArray,
                                               sizeof(kStaticClassArray) - 1))
                .ToStdString(),
            "abc");
}

TEST(HeapGraphTrackerTest, NumberOfArray) {
  // sizeof(...) - 1 below to get rid of the null-byte.
  EXPECT_EQ(NumberOfArrays(base::StringView(kArray, sizeof(kArray) - 1)), 1u);
  EXPECT_EQ(
      NumberOfArrays(base::StringView(kDoubleArray, sizeof(kDoubleArray) - 1)),
      2u);
  EXPECT_EQ(NumberOfArrays(base::StringView(kNoArray, sizeof(kNoArray) - 1)),
            0u);
  EXPECT_EQ(
      NumberOfArrays(base::StringView(kLongNoArray, sizeof(kLongNoArray) - 1)),
      0u);
}

}  // namespace
}  // namespace trace_processor
}  // namespace perfetto
