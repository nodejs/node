// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cmath>
#include <iostream>
#include <limits>

#include "src/api/api-inl.h"
#include "src/codegen/compiler.h"
#include "src/objects/hash-table-inl.h"
#include "src/objects/objects-inl.h"
#include "src/objects/objects.h"
#include "src/objects/string-set.h"
#include "test/unittests/test-utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace v8 {
namespace internal {

namespace {

bool IsInStringInstanceTypeList(InstanceType instance_type) {
  switch (instance_type) {
#define ASSERT_INSTANCE_TYPE(type, ...) \
  STATIC_ASSERT(InstanceType::type < InstanceType::FIRST_NONSTRING_TYPE);
    STRING_TYPE_LIST(ASSERT_INSTANCE_TYPE)
#undef ASSERT_INSTANCE_TYPE
#define TEST_INSTANCE_TYPE(type, ...) case InstanceType::type:
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
  int prev = -1;
  InstanceType current_type = static_cast<InstanceType>(current);
  EXPECT_EQ(current_type, InstanceType::FIRST_TYPE);
  EXPECT_EQ(current_type, InstanceType::INTERNALIZED_STRING_TYPE);
#define TEST_INSTANCE_TYPE(type)                                           \
  current_type = InstanceType::type;                                       \
  current = static_cast<int>(current_type);                                \
  if (current > static_cast<int>(LAST_NAME_TYPE)) {                        \
    EXPECT_LE(prev + 1, current);                                          \
  }                                                                        \
  EXPECT_LT(prev, current) << " INSTANCE_TYPE_LIST is not ordered: "       \
                           << "last = " << static_cast<InstanceType>(prev) \
                           << " vs. current = " << current_type;           \
  prev = current;

  // Only test hand-written portion of instance type list. The generated portion
  // doesn't run the same risk of getting out of order, and it does emit type
  // names out of numerical order in one case: JS_OBJECT_TYPE is emitted before
  // its subclass types, because types are emitted in depth-first pre-order
  // traversal order, and some of its subclass types are numerically earlier.
  INSTANCE_TYPE_LIST_BASE(TEST_INSTANCE_TYPE)
#undef TEST_INSTANCE_TYPE
}

TEST(Object, StructListOrder) {
  int current = static_cast<int>(InstanceType::FIRST_STRUCT_TYPE);
  int prev = current - 1;
  ASSERT_LT(0, prev);
  InstanceType current_type = static_cast<InstanceType>(current);
#define TEST_STRUCT(TYPE, class, name)                 \
  current_type = InstanceType::TYPE;                   \
  current = static_cast<int>(current_type);            \
  EXPECT_LE(prev + 1, current)                         \
      << " STRUCT_LIST is not ordered: "               \
      << " last = " << static_cast<InstanceType>(prev) \
      << " vs. current = " << current_type;            \
  prev = current;

  // Only test the _BASE portion (the hand-coded part). Note that the values are
  // not necessarily consecutive because some Structs that need special
  // handling, such as those that have multiple Map instances associated, are
  // omitted from this list.
  STRUCT_LIST_GENERATOR_BASE(STRUCT_LIST_ADAPTER, TEST_STRUCT)
#undef TEST_STRUCT
}

using ObjectWithIsolate = TestWithIsolate;

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
    dict = NumberDictionary::Add(isolate(), dict, i, value, details);
    CHECK_EQ(4, dict->Capacity());
  }
  // 4th element triggers growth.
  DCHECK_EQ(4, i);
  for (; i <= 5; i++) {
    dict = NumberDictionary::Add(isolate(), dict, i, value, details);
    CHECK_EQ(8, dict->Capacity());
  }
  // 6th element triggers growth.
  DCHECK_EQ(6, i);
  for (; i <= 11; i++) {
    dict = NumberDictionary::Add(isolate(), dict, i, value, details);
    CHECK_EQ(16, dict->Capacity());
  }
  // 12th element triggers growth.
  DCHECK_EQ(12, i);
  for (; i <= 21; i++) {
    dict = NumberDictionary::Add(isolate(), dict, i, value, details);
    CHECK_EQ(32, dict->Capacity());
  }
  // 22nd element triggers growth.
  DCHECK_EQ(22, i);
  for (; i <= 43; i++) {
    dict = NumberDictionary::Add(isolate(), dict, i, value, details);
    CHECK_EQ(64, dict->Capacity());
  }
  // 44th element triggers growth.
  DCHECK_EQ(44, i);
  for (; i <= 50; i++) {
    dict = NumberDictionary::Add(isolate(), dict, i, value, details);
    CHECK_EQ(128, dict->Capacity());
  }

  // If we grow by larger chunks, the next (sufficiently big) power of 2 is
  // chosen as the capacity.
  dict = NumberDictionary::New(isolate(), 1);
  dict = NumberDictionary::EnsureCapacity(isolate(), dict, 65);
  CHECK_EQ(128, dict->Capacity());

  dict = NumberDictionary::New(isolate(), 1);
  dict = NumberDictionary::EnsureCapacity(isolate(), dict, 30);
  CHECK_EQ(64, dict->Capacity());
}

TEST_F(TestWithNativeContext, EmptyFunctionScopeInfo) {
  // Check that the empty_function has a properly set up ScopeInfo.
  Handle<JSFunction> function = RunJS<JSFunction>("(function(){})");

  Handle<ScopeInfo> scope_info(function->shared().scope_info(),
                               function->GetIsolate());
  Handle<ScopeInfo> empty_function_scope_info(
      isolate()->empty_function()->shared().scope_info(),
      function->GetIsolate());

  EXPECT_EQ(scope_info->Flags(), empty_function_scope_info->Flags());
  EXPECT_EQ(scope_info->ParameterCount(),
            empty_function_scope_info->ParameterCount());
  EXPECT_EQ(scope_info->ContextLocalCount(),
            empty_function_scope_info->ContextLocalCount());
}

TEST_F(TestWithNativeContext, RecreateScopeInfoWithLocalsBlocklistWorks) {
  // Create a JSFunction to get a {ScopeInfo} we can use for the test.
  Handle<JSFunction> function = RunJS<JSFunction>("(function foo() {})");
  Handle<ScopeInfo> original_scope_info(function->shared().scope_info(),
                                        isolate());
  ASSERT_FALSE(original_scope_info->HasLocalsBlockList());

  Handle<String> foo_string =
      isolate()->factory()->NewStringFromStaticChars("foo");
  Handle<String> bar_string =
      isolate()->factory()->NewStringFromStaticChars("bar");

  Handle<StringSet> blocklist = StringSet::New(isolate());
  StringSet::Add(isolate(), blocklist, foo_string);

  Handle<ScopeInfo> scope_info = ScopeInfo::RecreateWithBlockList(
      isolate(), original_scope_info, blocklist);

  DisallowGarbageCollection no_gc;
  EXPECT_TRUE(scope_info->HasLocalsBlockList());
  EXPECT_TRUE(scope_info->LocalsBlockList().Has(isolate(), foo_string));
  EXPECT_FALSE(scope_info->LocalsBlockList().Has(isolate(), bar_string));

  EXPECT_EQ(original_scope_info->length() + 1, scope_info->length());

  // Check that all variable fields *before* the blocklist stayed the same.
  for (int i = ScopeInfo::kVariablePartIndex;
       i < scope_info->LocalsBlockListIndex(); ++i) {
    EXPECT_EQ(original_scope_info->get(i), scope_info->get(i));
  }

  // Check that all variable fields *after* the blocklist stayed the same.
  for (int i = scope_info->LocalsBlockListIndex() + 1; i < scope_info->length();
       ++i) {
    EXPECT_EQ(original_scope_info->get(i - 1), scope_info->get(i));
  }
}

}  // namespace internal
}  // namespace v8
