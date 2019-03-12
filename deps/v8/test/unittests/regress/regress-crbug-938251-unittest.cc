// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/heap/factory.h"
#include "src/isolate.h"
#include "test/unittests/test-utils.h"

namespace v8 {
namespace internal {

using NewFixedDoubleArrayTest = TestWithIsolateAndZone;

TEST_F(NewFixedDoubleArrayTest, ThrowOnNegativeLength) {
  ASSERT_DEATH_IF_SUPPORTED({ factory()->NewFixedDoubleArray(-1); },
                            "Fatal javascript OOM in invalid array length");
}

}  // namespace internal
}  // namespace v8
