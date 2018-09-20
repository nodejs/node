// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "testing/gtest/include/gtest/gtest.h"

#include "include/v8.h"
#include "src/api.h"
#include "src/handles.h"
#include "src/objects-inl.h"
#include "test/unittests/test-utils.h"

namespace v8 {
namespace remote_object_unittest {

typedef TestWithIsolate RemoteObjectTest;

namespace {

bool AccessCheck(Local<Context> accessing_context,
                 Local<Object> accessed_object, Local<Value> data) {
  return false;
}

void NamedGetter(Local<Name> property,
                 const PropertyCallbackInfo<Value>& info) {}

void Constructor(const FunctionCallbackInfo<Value>& info) {
  ASSERT_TRUE(info.IsConstructCall());
}

}  // namespace

TEST_F(RemoteObjectTest, CreationContextOfRemoteContext) {
  Local<ObjectTemplate> global_template = ObjectTemplate::New(isolate());
  global_template->SetAccessCheckCallbackAndHandler(
      AccessCheck, NamedPropertyHandlerConfiguration(NamedGetter),
      IndexedPropertyHandlerConfiguration());

  Local<Object> remote_context =
      Context::NewRemoteContext(isolate(), global_template).ToLocalChecked();
  EXPECT_TRUE(remote_context->CreationContext().IsEmpty());
}

TEST_F(RemoteObjectTest, CreationContextOfRemoteObject) {
  Local<FunctionTemplate> constructor_template =
      FunctionTemplate::New(isolate(), Constructor);
  constructor_template->InstanceTemplate()->SetAccessCheckCallbackAndHandler(
      AccessCheck, NamedPropertyHandlerConfiguration(NamedGetter),
      IndexedPropertyHandlerConfiguration());

  Local<Object> remote_object =
      constructor_template->NewRemoteInstance().ToLocalChecked();
  EXPECT_TRUE(remote_object->CreationContext().IsEmpty());
}

TEST_F(RemoteObjectTest, RemoteContextInstanceChecks) {
  Local<FunctionTemplate> parent_template =
      FunctionTemplate::New(isolate(), Constructor);

  Local<FunctionTemplate> constructor_template =
      FunctionTemplate::New(isolate(), Constructor);
  constructor_template->InstanceTemplate()->SetAccessCheckCallbackAndHandler(
      AccessCheck, NamedPropertyHandlerConfiguration(NamedGetter),
      IndexedPropertyHandlerConfiguration());
  constructor_template->Inherit(parent_template);

  Local<Object> remote_context =
      Context::NewRemoteContext(isolate(),
                                constructor_template->InstanceTemplate())
          .ToLocalChecked();
  EXPECT_TRUE(parent_template->HasInstance(remote_context));
  EXPECT_TRUE(constructor_template->HasInstance(remote_context));
}

TEST_F(RemoteObjectTest, TypeOfRemoteContext) {
  Local<ObjectTemplate> global_template = ObjectTemplate::New(isolate());
  global_template->SetAccessCheckCallbackAndHandler(
      AccessCheck, NamedPropertyHandlerConfiguration(NamedGetter),
      IndexedPropertyHandlerConfiguration());

  Local<Object> remote_context =
      Context::NewRemoteContext(isolate(), global_template).ToLocalChecked();
  String::Utf8Value result(isolate(), remote_context->TypeOf(isolate()));
  EXPECT_STREQ("object", *result);
}

TEST_F(RemoteObjectTest, TypeOfRemoteObject) {
  Local<FunctionTemplate> constructor_template =
      FunctionTemplate::New(isolate(), Constructor);
  constructor_template->InstanceTemplate()->SetAccessCheckCallbackAndHandler(
      AccessCheck, NamedPropertyHandlerConfiguration(NamedGetter),
      IndexedPropertyHandlerConfiguration());

  Local<Object> remote_object =
      constructor_template->NewRemoteInstance().ToLocalChecked();
  String::Utf8Value result(isolate(), remote_object->TypeOf(isolate()));
  EXPECT_STREQ("object", *result);
}

TEST_F(RemoteObjectTest, ClassOf) {
  Local<FunctionTemplate> constructor_template =
      FunctionTemplate::New(isolate(), Constructor);
  constructor_template->InstanceTemplate()->SetAccessCheckCallbackAndHandler(
      AccessCheck, NamedPropertyHandlerConfiguration(NamedGetter),
      IndexedPropertyHandlerConfiguration());
  constructor_template->SetClassName(
      String::NewFromUtf8(isolate(), "test_class", NewStringType::kNormal)
          .ToLocalChecked());

  Local<Object> remote_object =
      constructor_template->NewRemoteInstance().ToLocalChecked();
  Local<String> class_name = Utils::ToLocal(
      i::handle(Utils::OpenHandle(*remote_object)->class_name(), i_isolate()));
  String::Utf8Value result(isolate(), class_name);
  EXPECT_STREQ("test_class", *result);
}

}  // namespace remote_object_unittest
}  // namespace v8
