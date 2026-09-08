// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/heap/pending-allocations.h"

#include "src/flags/flags.h"
#include "src/heap/heap.h"
#include "test/unittests/heap/heap-utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace v8 {
namespace internal {

using YoungPendingAllocationsTest = TestWithHeapInternals;

TEST_F(YoungPendingAllocationsTest, BasicTrackingAndSnapshot) {
  if (!v8_flags.incremental_marking) return;

  SimulateIncrementalMarking(false);

  YoungPendingAllocations young(heap());
  YoungPendingAllocations::Snapshot snap;

  young.UpdateSnapshotIfOutdated(&snap);
  EXPECT_FALSE(snap.Contains(100));
  EXPECT_FALSE(young.ContainsSynchronized(100));
  EXPECT_EQ(snap.version, 1u);

  young.UpdateLab(100, 200);
  young.UpdateSnapshotIfOutdated(&snap);
  EXPECT_TRUE(snap.Contains(100));
  EXPECT_TRUE(snap.Contains(150));
  EXPECT_FALSE(snap.Contains(200));
  EXPECT_TRUE(young.ContainsSynchronized(150));
  EXPECT_EQ(snap.version, 2u);

  young.UpdateLargeObject(300);
  young.UpdateSnapshotIfOutdated(&snap);
  EXPECT_TRUE(snap.Contains(150));
  EXPECT_TRUE(snap.Contains(300));
  EXPECT_FALSE(snap.Contains(301));
  EXPECT_TRUE(young.ContainsSynchronized(300));
  EXPECT_EQ(snap.version, 3u);

  young.RemoveLab();
  young.UpdateSnapshotIfOutdated(&snap);
  EXPECT_FALSE(snap.Contains(150));
  EXPECT_TRUE(snap.Contains(300));
  EXPECT_EQ(snap.version, 4u);

  young.RemoveLargeObject();
  young.UpdateSnapshotIfOutdated(&snap);
  EXPECT_FALSE(snap.Contains(300));
  EXPECT_FALSE(young.ContainsSynchronized(300));
  EXPECT_EQ(snap.version, 5u);
}

}  // namespace internal
}  // namespace v8
