// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cmath>
#include <iostream>
#include <limits>

#include "src/objects-inl.h"
#include "src/objects.h"
#include "test/unittests/test-utils.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace v8 {
namespace internal {

namespace {

bool IsInStringInstanceTypeList(InstanceType instance_type) {
  switch (instance_type) {
#define TEST_INSTANCE_TYPE(type, ...) \
  case InstanceType::type:            \
    STATIC_ASSERT(InstanceType::type < InstanceType::FIRST_NONSTRING_TYPE);

    STRING_TYPE_LIST(TEST_INSTANCE_TYPE)
#undef TEST_INSTANCE_TYPE
    return true;
    default:
      EXPECT_LE(InstanceType::FIRST_NONSTRING_TYPE, instance_type);
      return false;
  }
}

void CheckOneInstanceType(InstanceType instance_type) {
  if (IsInStringInstanceTypeList(instance_type)) {
    EXPECT_TRUE((instance_type & kIsNotStringMask) == kStringTag)
        << "Failing IsString mask check for " << instance_type;
  } else {
    EXPECT_FALSE((instance_type & kIsNotStringMask) == kStringTag)
        << "Failing !IsString mask check for " << instance_type;
  }
}

}  // namespace

TEST(Object, InstanceTypeList) {
#define TEST_INSTANCE_TYPE(type) CheckOneInstanceType(InstanceType::type);

  INSTANCE_TYPE_LIST(TEST_INSTANCE_TYPE)
#undef TEST_INSTANCE_TYPE
}

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

typedef TestWithIsolate ObjectWithIsolate;

TEST_F(ObjectWithIsolate, DictionaryGrowth) {
  Handle<NumberDictionary> dict = NumberDictionary::New(isolate(), 1);
  Handle<Object> value = isolate()->factory()->null_value();
  PropertyDetails details = PropertyDetails::Empty();

  // This test documents the expected growth behavior of a dictionary getting
  // elements added to it one by one.
  STATIC_ASSERT(HashTableBase::kMinCapacity == 4);
  uint32_t i = 1;
  // 3 elements fit into the initial capacity.
  for (; i <= 3; i++) {
    dict = NumberDictionary::Add(dict, i, value, details);
    CHECK_EQ(4, dict->Capacity());
  }
  // 4th element triggers growth.
  DCHECK_EQ(4, i);
  for (; i <= 5; i++) {
    dict = NumberDictionary::Add(dict, i, value, details);
    CHECK_EQ(8, dict->Capacity());
  }
  // 6th element triggers growth.
  DCHECK_EQ(6, i);
  for (; i <= 11; i++) {
    dict = NumberDictionary::Add(dict, i, value, details);
    CHECK_EQ(16, dict->Capacity());
  }
  // 12th element triggers growth.
  DCHECK_EQ(12, i);
  for (; i <= 21; i++) {
    dict = NumberDictionary::Add(dict, i, value, details);
    CHECK_EQ(32, dict->Capacity());
  }
  // 22nd element triggers growth.
  DCHECK_EQ(22, i);
  for (; i <= 43; i++) {
    dict = NumberDictionary::Add(dict, i, value, details);
    CHECK_EQ(64, dict->Capacity());
  }
  // 44th element triggers growth.
  DCHECK_EQ(44, i);
  for (; i <= 50; i++) {
    dict = NumberDictionary::Add(dict, i, value, details);
    CHECK_EQ(128, dict->Capacity());
  }

  // If we grow by larger chunks, the next (sufficiently big) power of 2 is
  // chosen as the capacity.
  dict = NumberDictionary::New(isolate(), 1);
  dict = NumberDictionary::EnsureCapacity(dict, 65);
  CHECK_EQ(128, dict->Capacity());

  dict = NumberDictionary::New(isolate(), 1);
  dict = NumberDictionary::EnsureCapacity(dict, 30);
  CHECK_EQ(64, dict->Capacity());
}

}  // namespace internal
}  // namespace v8
