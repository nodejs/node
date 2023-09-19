// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/diagnostics/gdb-jit.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace v8 {
namespace internal {
namespace GDBJITInterface {

#ifdef ENABLE_GDB_JIT_INTERFACE
TEST(GDBJITTest, OverlapEntries) {
  ClearCodeMapForTesting();

  base::AddressRegion ar{10, 10};
  AddRegionForTesting(ar);

  // Full containment.
  ASSERT_EQ(1u, NumOverlapEntriesForTesting({11, 2}));
  // Overlap start.
  ASSERT_EQ(1u, NumOverlapEntriesForTesting({5, 10}));
  // Overlap end.
  ASSERT_EQ(1u, NumOverlapEntriesForTesting({15, 10}));

  // No overlap.
  // Completely smaller.
  ASSERT_EQ(0u, NumOverlapEntriesForTesting({5, 5}));
  // Completely bigger.
  ASSERT_EQ(0u, NumOverlapEntriesForTesting({20, 10}));

  AddRegionForTesting({20, 10});
  // Now we have 2 code entries that don't overlap:
  // [ entry 1 ][entry 2]
  // ^ 10       ^ 20

  // Overlap none.
  ASSERT_EQ(0u, NumOverlapEntriesForTesting({0, 5}));
  ASSERT_EQ(0u, NumOverlapEntriesForTesting({30, 5}));

  // Overlap one.
  ASSERT_EQ(1u, NumOverlapEntriesForTesting({15, 5}));
  ASSERT_EQ(1u, NumOverlapEntriesForTesting({20, 5}));

  // Overlap both.
  ASSERT_EQ(2u, NumOverlapEntriesForTesting({15, 10}));
  ASSERT_EQ(2u, NumOverlapEntriesForTesting({5, 20}));
  ASSERT_EQ(2u, NumOverlapEntriesForTesting({15, 20}));
  ASSERT_EQ(2u, NumOverlapEntriesForTesting({0, 40}));

  ClearCodeMapForTesting();
}
#endif

}  // namespace GDBJITInterface
}  // namespace internal
}  // namespace v8
