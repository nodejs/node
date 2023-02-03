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

  base::Optional<Snapshot> s1;
  {
    SnapshotTable<int>::Scope scope(table);
    EXPECT_EQ(scope.Get(k1), 1);
    EXPECT_EQ(scope.Get(k2), 2);
    EXPECT_EQ(scope.Get(k3), 3);
    EXPECT_EQ(scope.Get(k4), 4);
    scope.Set(k1, 10);
    scope.Set(k2, 20);
    scope.Set(k4, 4);
    EXPECT_EQ(scope.Get(k1), 10);
    EXPECT_EQ(scope.Get(k2), 20);
    EXPECT_EQ(scope.Get(k3), 3);
    EXPECT_EQ(scope.Get(k4), 4);
    s1 = scope.Seal();
  }

  base::Optional<Snapshot> s2;
  {
    SnapshotTable<int>::Scope scope(table);
    EXPECT_EQ(scope.Get(k1), 1);
    EXPECT_EQ(scope.Get(k2), 2);
    EXPECT_EQ(scope.Get(k3), 3);
    EXPECT_EQ(scope.Get(k4), 4);
    scope.Set(k1, 11);
    scope.Set(k3, 33);
    EXPECT_EQ(scope.Get(k1), 11);
    EXPECT_EQ(scope.Get(k2), 2);
    EXPECT_EQ(scope.Get(k3), 33);
    EXPECT_EQ(scope.Get(k4), 4);
    s2 = scope.Seal();
  }

  {
    SnapshotTable<int>::Scope scope(table, *s2);
    // Assignments of the same value are ignored.
    EXPECT_EQ(scope.Get(k1), 11);
    scope.Set(k1, 11);
    // An empty scope does not produce a new snapshot.
    EXPECT_EQ(scope.Seal(), *s2);
  }

  base::Optional<Snapshot> s3;
  {
    SnapshotTable<int>::Scope scope(
        table, {*s1, *s2}, [&](Key key, base::Vector<const int> values) {
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
    EXPECT_EQ(scope.Get(k1), 21);
    EXPECT_EQ(scope.Get(k2), 22);
    EXPECT_EQ(scope.Get(k3), 36);
    EXPECT_EQ(scope.Get(k4), 4);
    scope.Set(k1, 40);
    EXPECT_EQ(scope.Get(k1), 40);
    EXPECT_EQ(scope.Get(k2), 22);
    EXPECT_EQ(scope.Get(k3), 36);
    EXPECT_EQ(scope.Get(k4), 4);
    s3 = scope.Seal();
  }

  base::Optional<Snapshot> s4;
  {
    SnapshotTable<int>::Scope scope(table, *s2);
    EXPECT_EQ(scope.Get(k1), 11);
    EXPECT_EQ(scope.Get(k2), 2);
    EXPECT_EQ(scope.Get(k3), 33);
    EXPECT_EQ(scope.Get(k4), 4);
    scope.Set(k3, 30);
    EXPECT_EQ(scope.Get(k3), 30);
    s4 = scope.Seal();
  }

  base::Optional<Snapshot> s5;
  {
    SnapshotTable<int>::Scope scope(
        table, {*s4, *s2}, [&](Key key, base::Vector<const int> values) {
          if (key == k3) {
            EXPECT_EQ(values[0], 30);
            EXPECT_EQ(values[1], 33);
          } else {
            EXPECT_TRUE(false);
          }
          return values[0] + values[1];
        });
    EXPECT_EQ(scope.Get(k1), 11);
    EXPECT_EQ(scope.Get(k2), 2);
    EXPECT_EQ(scope.Get(k3), 63);
    EXPECT_EQ(scope.Get(k4), 4);
    s5 = scope.Seal();
  }

  base::Optional<Key> k5;
  base::Optional<Snapshot> s6;
  {
    SnapshotTable<int>::Scope scope(table, *s2);
    scope.Set(k1, 5);
    // Creating a new key while the SnapshotTable is already in use, in the
    // middle of a scope. This is the same as creating the key in the beginning.
    k5 = table.NewKey(-1);
    EXPECT_EQ(scope.Get(*k5), -1);
    scope.Set(*k5, 42);
    EXPECT_EQ(scope.Get(*k5), 42);
    EXPECT_EQ(scope.Get(k1), 5);
    s6 = scope.Seal();
  }

  base::Optional<Snapshot> s7;
  {
    // We're merging {s6} and {s1}, to make sure that {s1}'s behavior is correct
    // with regard to {k5}, which wasn't created yet when {s1} was sealed.
    SnapshotTable<int>::Scope scope(
        table, {*s6, *s1}, [&](Key key, base::Vector<const int> values) {
          if (key == k1) {
            EXPECT_EQ(values[1], 10);
            EXPECT_EQ(values[0], 5);
          } else if (key == k2) {
            EXPECT_EQ(values[1], 20);
            EXPECT_EQ(values[0], 2);
          } else if (key == k3) {
            EXPECT_EQ(values[1], 3);
            EXPECT_EQ(values[0], 33);
          } else if (key == *k5) {
            EXPECT_EQ(values[0], 42);
            EXPECT_EQ(values[1], -1);
            return 127;
          } else {
            EXPECT_TRUE(false);
          }
          return values[0] + values[1];
        });
    EXPECT_EQ(scope.Get(k1), 15);
    EXPECT_EQ(scope.Get(k2), 22);
    EXPECT_EQ(scope.Get(k3), 36);
    EXPECT_EQ(scope.Get(k4), 4);
    EXPECT_EQ(scope.Get(*k5), 127);
    // We're not setting anything else, but the merges should produce entries in
    // the log.
    s7 = scope.Seal();
  }

  base::Optional<Snapshot> s8;
  {
    SnapshotTable<int>::Scope scope(table, *s7);
    // We're checking that {s7} did indeed capture the merge entries, despite
    // that we didn't do any explicit Set.
    EXPECT_EQ(scope.Get(k1), 15);
    EXPECT_EQ(scope.Get(k2), 22);
    EXPECT_EQ(scope.Get(k3), 36);
    EXPECT_EQ(scope.Get(k4), 4);
    EXPECT_EQ(scope.Get(*k5), 127);
    s8 = scope.Seal();
  }
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
