// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cmath>
#include <limits>

#include "src/heap/heap-inl.h"
#include "src/heap/heap.h"
#include "src/heap/marking-worklist.h"
#include "test/unittests/test-utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace v8 {
namespace internal {

using MarkingWorklistTest = TestWithContext;

TEST_F(MarkingWorklistTest, PushPop) {
  MarkingWorklistsHolder holder;
  MarkingWorklists worklists(kMainThreadTask, &holder);
  HeapObject pushed_object =
      ReadOnlyRoots(i_isolate()->heap()).undefined_value();
  worklists.Push(pushed_object);
  HeapObject popped_object;
  EXPECT_TRUE(worklists.Pop(&popped_object));
  EXPECT_EQ(popped_object, pushed_object);
}

TEST_F(MarkingWorklistTest, PushPopOnHold) {
  MarkingWorklistsHolder holder;
  MarkingWorklists worklists(kMainThreadTask, &holder);
  HeapObject pushed_object =
      ReadOnlyRoots(i_isolate()->heap()).undefined_value();
  worklists.PushOnHold(pushed_object);
  HeapObject popped_object;
  EXPECT_TRUE(worklists.PopOnHold(&popped_object));
  EXPECT_EQ(popped_object, pushed_object);
}

TEST_F(MarkingWorklistTest, PushPopEmbedder) {
  MarkingWorklistsHolder holder;
  MarkingWorklists worklists(kMainThreadTask, &holder);
  HeapObject pushed_object =
      ReadOnlyRoots(i_isolate()->heap()).undefined_value();
  worklists.PushEmbedder(pushed_object);
  HeapObject popped_object;
  EXPECT_TRUE(worklists.PopEmbedder(&popped_object));
  EXPECT_EQ(popped_object, pushed_object);
}

TEST_F(MarkingWorklistTest, MergeOnHold) {
  MarkingWorklistsHolder holder;
  MarkingWorklists main_worklists(kMainThreadTask, &holder);
  MarkingWorklists worker_worklists(kMainThreadTask + 1, &holder);
  HeapObject pushed_object =
      ReadOnlyRoots(i_isolate()->heap()).undefined_value();
  worker_worklists.PushOnHold(pushed_object);
  worker_worklists.FlushToGlobal();
  main_worklists.MergeOnHold();
  HeapObject popped_object;
  EXPECT_TRUE(main_worklists.Pop(&popped_object));
  EXPECT_EQ(popped_object, pushed_object);
}

TEST_F(MarkingWorklistTest, ShareWorkIfGlobalPoolIsEmpty) {
  MarkingWorklistsHolder holder;
  MarkingWorklists main_worklists(kMainThreadTask, &holder);
  MarkingWorklists worker_worklists(kMainThreadTask + 1, &holder);
  HeapObject pushed_object =
      ReadOnlyRoots(i_isolate()->heap()).undefined_value();
  main_worklists.Push(pushed_object);
  main_worklists.ShareWorkIfGlobalPoolIsEmpty();
  HeapObject popped_object;
  EXPECT_TRUE(worker_worklists.Pop(&popped_object));
  EXPECT_EQ(popped_object, pushed_object);
}

TEST_F(MarkingWorklistTest, ContextWorklistsPushPop) {
  const Address context = 0xabcdef;
  MarkingWorklistsHolder holder;
  holder.CreateContextWorklists({context});
  MarkingWorklists worklists(kMainThreadTask, &holder);
  HeapObject pushed_object =
      ReadOnlyRoots(i_isolate()->heap()).undefined_value();
  worklists.SwitchToContext(context);
  worklists.Push(pushed_object);
  worklists.SwitchToShared();
  HeapObject popped_object;
  EXPECT_TRUE(worklists.Pop(&popped_object));
  EXPECT_EQ(popped_object, pushed_object);
  holder.ReleaseContextWorklists();
}

TEST_F(MarkingWorklistTest, ContextWorklistsEmpty) {
  const Address context = 0xabcdef;
  MarkingWorklistsHolder holder;
  holder.CreateContextWorklists({context});
  MarkingWorklists worklists(kMainThreadTask, &holder);
  HeapObject pushed_object =
      ReadOnlyRoots(i_isolate()->heap()).undefined_value();
  worklists.SwitchToContext(context);
  worklists.Push(pushed_object);
  EXPECT_FALSE(worklists.IsEmpty());
  worklists.SwitchToShared();
  EXPECT_FALSE(worklists.IsEmpty());
  HeapObject popped_object;
  EXPECT_TRUE(worklists.Pop(&popped_object));
  EXPECT_EQ(popped_object, pushed_object);
  EXPECT_TRUE(worklists.IsEmpty());
  holder.ReleaseContextWorklists();
}

TEST_F(MarkingWorklistTest, ContextWorklistCrossTask) {
  const Address context1 = 0x1abcdef;
  const Address context2 = 0x2abcdef;
  MarkingWorklistsHolder holder;
  holder.CreateContextWorklists({context1, context2});
  MarkingWorklists main_worklists(kMainThreadTask, &holder);
  MarkingWorklists worker_worklists(kMainThreadTask + 1, &holder);
  HeapObject pushed_object =
      ReadOnlyRoots(i_isolate()->heap()).undefined_value();
  main_worklists.SwitchToContext(context1);
  main_worklists.Push(pushed_object);
  main_worklists.ShareWorkIfGlobalPoolIsEmpty();
  worker_worklists.SwitchToContext(context2);
  HeapObject popped_object;
  EXPECT_TRUE(worker_worklists.Pop(&popped_object));
  EXPECT_EQ(popped_object, pushed_object);
  EXPECT_EQ(context1, worker_worklists.Context());
  holder.ReleaseContextWorklists();
}

}  // namespace internal
}  // namespace v8
