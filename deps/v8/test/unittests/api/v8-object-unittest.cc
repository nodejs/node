// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "include/v8.h"
#include "test/unittests/test-utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace v8 {
namespace {

using ObjectTest = TestWithContext;

void accessor_name_getter_callback(Local<Name>,
                                   const PropertyCallbackInfo<Value>&) {}

TEST_F(ObjectTest, SetAccessorWhenUnconfigurablePropAlreadyDefined) {
  TryCatch try_catch(isolate());

  Local<Object> global = context()->Global();
  Local<String> property_name =
      String::NewFromUtf8(isolate(), "foo", NewStringType::kNormal)
          .ToLocalChecked();

  PropertyDescriptor prop_desc;
  prop_desc.set_configurable(false);
  global->DefineProperty(context(), property_name, prop_desc).ToChecked();

  Maybe<bool> result = global->SetAccessor(context(), property_name,
                                           accessor_name_getter_callback);
  ASSERT_TRUE(result.IsJust());
  ASSERT_FALSE(result.FromJust());
  ASSERT_FALSE(try_catch.HasCaught());
}

}  // namespace
}  // namespace v8
