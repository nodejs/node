// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "include/cppgc/garbage-collected.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace cppgc {
namespace internal {

namespace {

class GCed : public GarbageCollected<GCed> {};
class NotGCed {};

}  // namespace

TEST(GarbageCollectedTest, GarbageCollectedTrait) {
  EXPECT_FALSE(IsGarbageCollectedType<int>::value);
  EXPECT_FALSE(IsGarbageCollectedType<NotGCed>::value);
  EXPECT_TRUE(IsGarbageCollectedType<GCed>::value);
}

}  // namespace internal
}  // namespace cppgc
