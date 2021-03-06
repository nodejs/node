// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/execution/isolate.h"
#include "src/heap/factory.h"
#include "test/unittests/test-utils.h"

namespace v8 {
namespace internal {

using NewUninitializedFixedArrayTest = TestWithIsolateAndZone;

TEST_F(NewUninitializedFixedArrayTest, ThrowOnNegativeLength) {
  ASSERT_DEATH_IF_SUPPORTED({ factory()->NewUninitializedFixedArray(-1); },
                            "Fatal javascript OOM in invalid array length");
}

}  // namespace internal
}  // namespace v8
