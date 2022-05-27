// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/utils/bit-vector.h"

#include <stdlib.h>

#include "src/init/v8.h"
#include "test/unittests/test-utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace v8 {
namespace internal {

using BitVectorTest = TestWithIsolate;

TEST_F(BitVectorTest, BitVector) {
  v8::internal::AccountingAllocator allocator;
  Zone zone(&allocator, ZONE_NAME);
  {
    BitVector v(15, &zone);
    v.Add(1);
    EXPECT_TRUE(v.Contains(1));
    v.Remove(0);
    EXPECT_TRUE(!v.Contains(0));
    v.Add(0);
    v.Add(1);
    BitVector w(15, &zone);
    w.Add(1);
    v.Intersect(w);
    EXPECT_TRUE(!v.Contains(0));
    EXPECT_TRUE(v.Contains(1));
  }

  {
    BitVector v(64, &zone);
    v.Add(27);
    v.Add(30);
    v.Add(31);
    v.Add(33);
    BitVector::Iterator iter = v.begin();
    BitVector::Iterator end = v.end();
    EXPECT_NE(iter, end);
    EXPECT_EQ(27, *iter);
    ++iter;
    EXPECT_NE(iter, end);
    EXPECT_EQ(30, *iter);
    ++iter;
    EXPECT_NE(iter, end);
    EXPECT_EQ(31, *iter);
    ++iter;
    EXPECT_NE(iter, end);
    EXPECT_EQ(33, *iter);
    ++iter;
    EXPECT_TRUE(!(iter != end));
  }

  {
    BitVector v(15, &zone);
    v.Add(0);
    BitVector w(15, &zone);
    w.Add(1);
    v.Union(w);
    EXPECT_TRUE(v.Contains(0));
    EXPECT_TRUE(v.Contains(1));
  }

  {
    BitVector v(15, &zone);
    v.Add(0);
    BitVector w(15, &zone);
    w.CopyFrom(v);
    EXPECT_TRUE(w.Contains(0));
    w.Add(1);
    BitVector u(w, &zone);
    EXPECT_TRUE(u.Contains(0));
    EXPECT_TRUE(u.Contains(1));
    v.Union(w);
    EXPECT_TRUE(v.Contains(0));
    EXPECT_TRUE(v.Contains(1));
  }

  {
    BitVector v(35, &zone);
    v.Add(0);
    BitVector w(35, &zone);
    w.Add(33);
    v.Union(w);
    EXPECT_TRUE(v.Contains(0));
    EXPECT_TRUE(v.Contains(33));
  }

  {
    BitVector v(35, &zone);
    v.Add(32);
    v.Add(33);
    BitVector w(35, &zone);
    w.Add(33);
    v.Intersect(w);
    EXPECT_TRUE(!v.Contains(32));
    EXPECT_TRUE(v.Contains(33));
    BitVector r(35, &zone);
    r.CopyFrom(v);
    EXPECT_TRUE(!r.Contains(32));
    EXPECT_TRUE(r.Contains(33));
  }

  {
    BitVector v(35, &zone);
    v.Add(32);
    v.Add(33);
    EXPECT_TRUE(v.Contains(32));
    EXPECT_TRUE(v.Contains(33));
    EXPECT_TRUE(!v.Contains(22));
    EXPECT_TRUE(!v.Contains(34));
    v.Resize(50, &zone);
    EXPECT_TRUE(v.Contains(32));
    EXPECT_TRUE(v.Contains(33));
    EXPECT_TRUE(!v.Contains(22));
    EXPECT_TRUE(!v.Contains(34));
    EXPECT_TRUE(!v.Contains(43));
    v.Resize(300, &zone);
    EXPECT_TRUE(v.Contains(32));
    EXPECT_TRUE(v.Contains(33));
    EXPECT_TRUE(!v.Contains(22));
    EXPECT_TRUE(!v.Contains(34));
    EXPECT_TRUE(!v.Contains(43));
    EXPECT_TRUE(!v.Contains(243));
  }
}

}  // namespace internal
}  // namespace v8
