/*
 * Copyright (C) 2019 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "perfetto/protozero/copyable_ptr.h"

#include "test/gtest_and_gmock.h"

namespace protozero {
namespace {

struct X {
  X() = default;
  X(const X&) = default;
  X& operator=(const X&) = default;
  ~X() { val = -1; }

  friend bool operator==(const X& lhs, const X& rhs) {
    return lhs.val == rhs.val;
  }

  // Deliberately unusual implementation.
  friend bool operator!=(const X& lhs, const X& rhs) {
    return lhs.val == rhs.val * -1;
  }

  int val = 0;
};

TEST(CopyablePtrTest, CopyOperators) {
  CopyablePtr<X> p1;
  p1->val = 1;
  ASSERT_NE(p1.get(), nullptr);
  ASSERT_EQ(&*p1, p1.get());

  CopyablePtr<X> p2(p1);
  EXPECT_NE(p1.get(), nullptr);
  EXPECT_NE(p2.get(), nullptr);
  EXPECT_NE(p1.get(), p2.get());

  p2->val = 2;
  EXPECT_EQ(p1->val, 1);
  EXPECT_EQ(p2->val, 2);

  {
    CopyablePtr<X> p3;
    p3 = p1;
    EXPECT_EQ(p3->val, 1);

    p3 = p2;
    EXPECT_EQ(p3->val, 2);

    p3->val = 3;
    EXPECT_EQ(p3->val, 3);
  }

  EXPECT_EQ(p1->val, 1);
  EXPECT_EQ(p2->val, 2);
}

TEST(CopyablePtrTest, MoveOperators) {
  CopyablePtr<X> p1;
  p1->val = 1;

  CopyablePtr<X> p2(std::move(p1));
  EXPECT_EQ(p2->val, 1);

  // The moved-from object needs to stay valid and non-null.
  EXPECT_EQ(p1->val, 0);
  {
    CopyablePtr<X> p3;
    p3->val = 3;
    p1 = std::move(p3);
    EXPECT_EQ(p1->val, 3);
    EXPECT_EQ(p3->val, 0);
  }
  EXPECT_EQ(p1->val, 3);
}

TEST(CopyablePtrTest, DeepCompare) {
  CopyablePtr<X> p1;
  p1->val = 1;

  CopyablePtr<X> p2;
  p2->val = 2;

  CopyablePtr<X> p3;
  p3->val = -2;

  EXPECT_NE(p1.get(), p2.get());
  EXPECT_NE(p1.get(), p3.get());

  EXPECT_FALSE(p1 == p2);
  EXPECT_FALSE(p1 == p3);
  EXPECT_FALSE(p2 == p3);

  EXPECT_FALSE(p1 != p2);  // The operator!= is special, see above.
  EXPECT_TRUE(p2 != p3);

  p1->val = -2;
  EXPECT_TRUE(p1 != p2);
  EXPECT_FALSE(p1 == p2);
}

}  // namespace
}  // namespace protozero
