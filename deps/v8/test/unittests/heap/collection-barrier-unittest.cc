// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/heap/collection-barrier.h"

#include "test/unittests/test-utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace v8::internal {

using CollectionRequest = CollectionBarrier::CollectionRequest;

TEST(CollectionBarrierTest, SetMajor) {
  CollectionRequest request;
  EXPECT_FALSE(request.Set(RequestedGCKind::kMajor));
  EXPECT_TRUE(request.Has(RequestedGCKind::kMajor));
  EXPECT_TRUE(request.Set(RequestedGCKind::kMajor));

  EXPECT_FALSE(request.Set(RequestedGCKind::kLastResort));
}

TEST(CollectionBarrierTest, SetLastResort) {
  CollectionRequest request;
  EXPECT_FALSE(request.Set(RequestedGCKind::kLastResort));
  EXPECT_TRUE(request.Has(RequestedGCKind::kLastResort));
  EXPECT_FALSE(request.Has(RequestedGCKind::kMajor));
  EXPECT_TRUE(request.Set(RequestedGCKind::kLastResort));

  EXPECT_TRUE(request.Set(RequestedGCKind::kMajor));
}

TEST(CollectionBarrierTest, Get) {
  CollectionRequest request;
  EXPECT_EQ(request.Get(), std::nullopt);

  EXPECT_FALSE(request.Set(RequestedGCKind::kMajor));
  EXPECT_EQ(request.Get(), RequestedGCKind::kMajor);

  EXPECT_FALSE(request.Set(RequestedGCKind::kLastResort));
  EXPECT_EQ(request.Get(), RequestedGCKind::kLastResort);
}

TEST(CollectionBarrierTest, Clear) {
  CollectionRequest request;
  EXPECT_FALSE(request.Set(RequestedGCKind::kMajor));
  EXPECT_TRUE(request.Clear(RequestedGCKind::kMajor));
  EXPECT_FALSE(request.HasAny());

  EXPECT_FALSE(request.Set(RequestedGCKind::kMajor));
  EXPECT_FALSE(request.Set(RequestedGCKind::kLastResort));
  EXPECT_TRUE(request.Clear(RequestedGCKind::kLastResort));
  EXPECT_FALSE(request.HasAny());

  EXPECT_FALSE(request.Set(RequestedGCKind::kMajor));
  EXPECT_FALSE(request.Clear(RequestedGCKind::kLastResort));
  EXPECT_FALSE(request.HasAny());
}

}  // namespace v8::internal
