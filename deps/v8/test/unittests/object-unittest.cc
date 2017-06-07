// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cmath>
#include <iostream>
#include <limits>

#include "src/objects-inl.h"
#include "src/objects.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace v8 {
namespace internal {

TEST(Object, InstanceTypeListOrder) {
  int current = 0;
  int last = -1;
  InstanceType current_type = static_cast<InstanceType>(current);
  EXPECT_EQ(current_type, InstanceType::FIRST_TYPE);
  EXPECT_EQ(current_type, InstanceType::INTERNALIZED_STRING_TYPE);
#define TEST_INSTANCE_TYPE(type)                                           \
  current_type = InstanceType::type;                                       \
  current = static_cast<int>(current_type);                                \
  if (current > static_cast<int>(LAST_NAME_TYPE)) {                        \
    EXPECT_EQ(last + 1, current);                                          \
  }                                                                        \
  EXPECT_LT(last, current) << " INSTANCE_TYPE_LIST is not ordered: "       \
                           << "last = " << static_cast<InstanceType>(last) \
                           << " vs. current = " << current_type;           \
  last = current;

  INSTANCE_TYPE_LIST(TEST_INSTANCE_TYPE)
#undef TEST_INSTANCE_TYPE
}

TEST(Object, StructListOrder) {
  int current = static_cast<int>(InstanceType::ACCESSOR_INFO_TYPE);
  int last = current - 1;
  ASSERT_LT(0, last);
  InstanceType current_type = static_cast<InstanceType>(current);
#define TEST_STRUCT(type, class, name)                 \
  current_type = InstanceType::type##_TYPE;            \
  current = static_cast<int>(current_type);            \
  EXPECT_EQ(last + 1, current)                         \
      << " STRUCT_LIST is not ordered: "               \
      << " last = " << static_cast<InstanceType>(last) \
      << " vs. current = " << current_type;            \
  last = current;

  STRUCT_LIST(TEST_STRUCT)
#undef TEST_STRUCT
}

}  // namespace internal
}  // namespace v8
