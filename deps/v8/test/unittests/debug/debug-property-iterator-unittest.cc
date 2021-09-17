// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "include/v8.h"
#include "src/api/api.h"
#include "src/objects/objects-inl.h"
#include "test/unittests/test-utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace v8 {
namespace debug {
namespace {

using DebugPropertyIteratorTest = TestWithContext;

TEST_F(DebugPropertyIteratorTest, WalksPrototypeChain) {
  TryCatch try_catch(isolate());

  Local<Object> object = Object::New(isolate());

  ASSERT_TRUE(object
                  ->CreateDataProperty(
                      context(),
                      String::NewFromUtf8Literal(isolate(), "own_property"),
                      Number::New(isolate(), 42))
                  .FromMaybe(false));

  Local<Object> prototype = Object::New(isolate());
  ASSERT_TRUE(object->SetPrototype(context(), prototype).FromMaybe(false));
  ASSERT_TRUE(prototype
                  ->CreateDataProperty(context(),
                                       String::NewFromUtf8Literal(
                                           isolate(), "prototype_property"),
                                       Number::New(isolate(), 21))
                  .FromMaybe(false));

  auto iterator = PropertyIterator::Create(context(), object);
  ASSERT_NE(iterator, nullptr);
  ASSERT_FALSE(iterator->Done());
  EXPECT_TRUE(iterator->is_own());
  char name_buffer[100];
  iterator->name().As<v8::String>()->WriteUtf8(isolate(), name_buffer);
  EXPECT_EQ("own_property", std::string(name_buffer));
  ASSERT_TRUE(iterator->Advance().FromMaybe(false));

  ASSERT_FALSE(iterator->Done());
  EXPECT_TRUE(iterator->is_own());
  iterator->name().As<v8::String>()->WriteUtf8(isolate(), name_buffer);
  EXPECT_EQ("own_property", std::string(name_buffer));
  ASSERT_TRUE(iterator->Advance().FromMaybe(false));

  ASSERT_FALSE(iterator->Done());
  EXPECT_FALSE(iterator->is_own());
  iterator->name().As<v8::String>()->WriteUtf8(isolate(), name_buffer);
  EXPECT_EQ("prototype_property", std::string(name_buffer));
  ASSERT_TRUE(iterator->Advance().FromMaybe(false));

  ASSERT_FALSE(iterator->Done());
}

bool may_access = true;

bool AccessCheck(Local<Context> accessing_context,
                 Local<Object> accessed_object, Local<Value> data) {
  return may_access;
}

TEST_F(DebugPropertyIteratorTest, DoestWalksPrototypeChainIfInaccesible) {
  TryCatch try_catch(isolate());

  Local<ObjectTemplate> object_template = ObjectTemplate::New(isolate());
  object_template->SetAccessCheckCallback(AccessCheck);

  Local<Object> object =
      object_template->NewInstance(context()).ToLocalChecked();
  ASSERT_TRUE(object
                  ->CreateDataProperty(
                      context(),
                      String::NewFromUtf8Literal(isolate(), "own_property"),
                      Number::New(isolate(), 42))
                  .FromMaybe(false));

  auto iterator = PropertyIterator::Create(context(), object);
  may_access = false;
  ASSERT_NE(iterator, nullptr);
  ASSERT_FALSE(iterator->Done());
  EXPECT_TRUE(iterator->is_own());
  char name_buffer[100];
  iterator->name().As<v8::String>()->WriteUtf8(isolate(), name_buffer);
  EXPECT_EQ("own_property", std::string(name_buffer));
  ASSERT_TRUE(iterator->Advance().FromMaybe(false));

  ASSERT_TRUE(iterator->Done());
}

}  // namespace
}  // namespace debug
}  // namespace v8
