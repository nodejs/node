// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/turboshaft/snapshot-table.h"

#include "src/base/optional.h"
#include "src/base/vector.h"
#include "test/unittests/test-utils.h"

namespace v8::internal::compiler::turboshaft {

class SnapshotTableTest : public TestWithPlatform {};

TEST_F(SnapshotTableTest, BasicTest) {
  AccountingAllocator allocator;
  Zone zone(&allocator, ZONE_NAME);

  using Key = SnapshotTable<int>::Key;
  using Snapshot = SnapshotTable<int>::Snapshot;

  SnapshotTable<int> table(&zone);

  Key k1 = table.NewKey(1);
  Key k2 = table.NewKey(2);
  Key k3 = table.NewKey(3);
  Key k4 = table.NewKey(4);

  table.StartNewSnapshot();
  EXPECT_EQ(table.Get(k1), 1);
  EXPECT_EQ(table.Get(k2), 2);
  EXPECT_EQ(table.Get(k3), 3);
  EXPECT_EQ(table.Get(k4), 4);
  table.Set(k1, 10);
  table.Set(k2, 20);
  table.Set(k4, 4);
  EXPECT_EQ(table.Get(k1), 10);
  EXPECT_EQ(table.Get(k2), 20);
  EXPECT_EQ(table.Get(k3), 3);
  EXPECT_EQ(table.Get(k4), 4);
  Snapshot s1 = table.Seal();

  table.StartNewSnapshot();
  EXPECT_EQ(table.Get(k1), 1);
  EXPECT_EQ(table.Get(k2), 2);
  EXPECT_EQ(table.Get(k3), 3);
  EXPECT_EQ(table.Get(k4), 4);
  table.Set(k1, 11);
  table.Set(k3, 33);
  EXPECT_EQ(table.Get(k1), 11);
  EXPECT_EQ(table.Get(k2), 2);
  EXPECT_EQ(table.Get(k3), 33);
  EXPECT_EQ(table.Get(k4), 4);
  Snapshot s2 = table.Seal();

  table.StartNewSnapshot(s2);
  // Assignments of the same value are ignored.
  EXPECT_EQ(table.Get(k1), 11);
  table.Set(k1, 11);
  // Sealing an empty snapshot does not produce a new snapshot.
  EXPECT_EQ(table.Seal(), s2);

  table.StartNewSnapshot({s1, s2},
                         [&](Key key, base::Vector<const int> values) {
                           if (key == k1) {
                             EXPECT_EQ(values[0], 10);
                             EXPECT_EQ(values[1], 11);
                           } else if (key == k2) {
                             EXPECT_EQ(values[0], 20);
                             EXPECT_EQ(values[1], 2);
                           } else if (key == k3) {
                             EXPECT_EQ(values[0], 3);
                             EXPECT_EQ(values[1], 33);
                           } else {
                             EXPECT_TRUE(false);
                           }
                           return values[0] + values[1];
                         });
  EXPECT_EQ(table.Get(k1), 21);
  EXPECT_EQ(table.Get(k2), 22);
  EXPECT_EQ(table.Get(k3), 36);
  EXPECT_EQ(table.Get(k4), 4);
  table.Set(k1, 40);
  EXPECT_EQ(table.Get(k1), 40);
  EXPECT_EQ(table.Get(k2), 22);
  EXPECT_EQ(table.Get(k3), 36);
  EXPECT_EQ(table.Get(k4), 4);
  EXPECT_EQ(table.GetPredecessorValue(k1, 0), 10);
  EXPECT_EQ(table.GetPredecessorValue(k1, 1), 11);
  EXPECT_EQ(table.GetPredecessorValue(k2, 0), 20);
  EXPECT_EQ(table.GetPredecessorValue(k2, 1), 2);
  EXPECT_EQ(table.GetPredecessorValue(k3, 0), 3);
  EXPECT_EQ(table.GetPredecessorValue(k3, 1), 33);
  table.Seal();

  table.StartNewSnapshot({s1, s2});
  EXPECT_EQ(table.Get(k1), 1);
  EXPECT_EQ(table.Get(k2), 2);
  EXPECT_EQ(table.Get(k3), 3);
  EXPECT_EQ(table.Get(k4), 4);
  table.Seal();

  table.StartNewSnapshot(s2);
  EXPECT_EQ(table.Get(k1), 11);
  EXPECT_EQ(table.Get(k2), 2);
  EXPECT_EQ(table.Get(k3), 33);
  EXPECT_EQ(table.Get(k4), 4);
  table.Set(k3, 30);
  EXPECT_EQ(table.Get(k3), 30);
  Snapshot s4 = table.Seal();

  table.StartNewSnapshot({s4, s2},
                         [&](Key key, base::Vector<const int> values) {
                           if (key == k3) {
                             EXPECT_EQ(values[0], 30);
                             EXPECT_EQ(values[1], 33);
                           } else {
                             EXPECT_TRUE(false);
                           }
                           return values[0] + values[1];
                         });
  EXPECT_EQ(table.Get(k1), 11);
  EXPECT_EQ(table.Get(k2), 2);
  EXPECT_EQ(table.Get(k3), 63);
  EXPECT_EQ(table.Get(k4), 4);
  EXPECT_EQ(table.GetPredecessorValue(k3, 0), 30);
  EXPECT_EQ(table.GetPredecessorValue(k3, 1), 33);
  table.Seal();

  table.StartNewSnapshot(s2);
  table.Set(k1, 5);
  // Creating a new key while the SnapshotTable is already in use. This is the
  // same as creating the key at the beginning.
  Key k5 = table.NewKey(-1);
  EXPECT_EQ(table.Get(k5), -1);
  table.Set(k5, 42);
  EXPECT_EQ(table.Get(k5), 42);
  EXPECT_EQ(table.Get(k1), 5);
  Snapshot s6 = table.Seal();

  // We're merging {s6} and {s1}, to make sure that {s1}'s behavior is correct
  // with regard to {k5}, which wasn't created yet when {s1} was sealed.
  table.StartNewSnapshot({s6, s1},
                         [&](Key key, base::Vector<const int> values) {
                           if (key == k1) {
                             EXPECT_EQ(values[1], 10);
                             EXPECT_EQ(values[0], 5);
                           } else if (key == k2) {
                             EXPECT_EQ(values[1], 20);
                             EXPECT_EQ(values[0], 2);
                           } else if (key == k3) {
                             EXPECT_EQ(values[1], 3);
                             EXPECT_EQ(values[0], 33);
                           } else if (key == k5) {
                             EXPECT_EQ(values[0], 42);
                             EXPECT_EQ(values[1], -1);
                             return 127;
                           } else {
                             EXPECT_TRUE(false);
                           }
                           return values[0] + values[1];
                         });
  EXPECT_EQ(table.Get(k1), 15);
  EXPECT_EQ(table.Get(k2), 22);
  EXPECT_EQ(table.Get(k3), 36);
  EXPECT_EQ(table.Get(k4), 4);
  EXPECT_EQ(table.Get(k5), 127);
  EXPECT_EQ(table.GetPredecessorValue(k1, 0), 5);
  EXPECT_EQ(table.GetPredecessorValue(k1, 1), 10);
  EXPECT_EQ(table.GetPredecessorValue(k2, 0), 2);
  EXPECT_EQ(table.GetPredecessorValue(k2, 1), 20);
  EXPECT_EQ(table.GetPredecessorValue(k3, 0), 33);
  EXPECT_EQ(table.GetPredecessorValue(k3, 1), 3);
  EXPECT_EQ(table.GetPredecessorValue(k5, 0), 42);
  EXPECT_EQ(table.GetPredecessorValue(k5, 1), -1);
  // We're not setting anything else, but the merges should produce entries in
  // the log.
  Snapshot s7 = table.Seal();

  table.StartNewSnapshot(s7);
  // We're checking that {s7} did indeed capture the merge entries, despite
  // that we didn't do any explicit Set.
  EXPECT_EQ(table.Get(k1), 15);
  EXPECT_EQ(table.Get(k2), 22);
  EXPECT_EQ(table.Get(k3), 36);
  EXPECT_EQ(table.Get(k4), 4);
  EXPECT_EQ(table.Get(k5), 127);
  table.Seal();
}

TEST_F(SnapshotTableTest, KeyData) {
  AccountingAllocator allocator;
  Zone zone(&allocator, ZONE_NAME);

  struct Data {
    int x;
  };

  using STable = SnapshotTable<int, Data>;
  using Key = STable::Key;

  STable table(&zone);

  Key k1 = table.NewKey(Data{5}, 1);

  EXPECT_EQ(k1.data().x, 5);
}

}  // namespace v8::internal::compiler::turboshaft
