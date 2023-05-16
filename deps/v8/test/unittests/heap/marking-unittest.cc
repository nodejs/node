// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdlib.h>

#include "src/common/globals.h"
#include "src/heap/marking.h"
#include "test/unittests/heap/bitmap-test-utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace v8 {
namespace internal {

using MarkingTest = TestWithBitmap;

TEST_F(MarkingTest, TransitionMarkBit) {
  auto bitmap = this->bitmap();
  const int kLocationsSize = 3;
  int position[kLocationsSize] = {MarkingBitmap::kBitsPerCell - 2,
                                  MarkingBitmap::kBitsPerCell - 1,
                                  MarkingBitmap::kBitsPerCell};
  for (int i = 0; i < kLocationsSize; i++) {
    MarkBit mark_bit = bitmap->MarkBitFromIndexForTesting(position[i]);
    CHECK(!mark_bit.template Get<AccessMode::NON_ATOMIC>());
    CHECK(mark_bit.template Set<AccessMode::NON_ATOMIC>());
    CHECK(mark_bit.template Get<AccessMode::NON_ATOMIC>());
    CHECK(mark_bit.Clear());
    CHECK(!mark_bit.template Get<AccessMode::NON_ATOMIC>());
  }
}

}  // namespace internal
}  // namespace v8
