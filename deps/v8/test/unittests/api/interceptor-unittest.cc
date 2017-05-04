// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "include/v8.h"
#include "test/unittests/test-utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace v8 {
namespace {

using InterceptorTest = TestWithContext;

void NamedGetter(Local<Name> property,
                 const PropertyCallbackInfo<Value>& info) {}

TEST_F(InterceptorTest, FreezeApiObjectWithInterceptor) {
  TryCatch try_catch(isolate());

  Local<FunctionTemplate> tmpl = FunctionTemplate::New(isolate());
  tmpl->InstanceTemplate()->SetHandler(
      NamedPropertyHandlerConfiguration(NamedGetter));

  Local<Function> ctor = tmpl->GetFunction(context()).ToLocalChecked();
  Local<Object> obj = ctor->NewInstance(context()).ToLocalChecked();
  ASSERT_TRUE(
      obj->SetIntegrityLevel(context(), IntegrityLevel::kFrozen).IsNothing());
  ASSERT_TRUE(try_catch.HasCaught());
}

}  // namespace
}  // namespace v8
