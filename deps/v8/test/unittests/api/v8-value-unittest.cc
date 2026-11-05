// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "include/v8-value.h"

#include "include/v8-primitive.h"
#include "test/unittests/test-utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace v8 {
namespace {

using ValueTest = TestWithContext;

TEST_F(ValueTest, TypecheckWitness) {
  Local<Object> global = context()->Global();
  Local<String> foo = String::NewFromUtf8Literal(isolate(), "foo");
  Local<String> bar = String::NewFromUtf8Literal(isolate(), "bar");
  v8::TypecheckWitness witness(isolate());
  EXPECT_FALSE(witness.Matches(global));
  EXPECT_FALSE(witness.Matches(foo));
  witness.Update(global);
  EXPECT_TRUE(witness.Matches(global));
  EXPECT_FALSE(witness.Matches(foo));
  witness.Update(foo);
  EXPECT_FALSE(witness.Matches(global));
  EXPECT_TRUE(witness.Matches(foo));
  EXPECT_TRUE(witness.Matches(bar));
}

TEST_F(ValueTest, ToUint32) {
  HandleScope scope(isolate());

  Local<Value> pos_smi = Integer::New(isolate(), 42);
  Local<Uint32> pos_uint32 = pos_smi->ToUint32(context()).ToLocalChecked();
  EXPECT_TRUE(pos_uint32->IsUint32());
  EXPECT_EQ(42u, pos_uint32->Value());
  EXPECT_EQ(42.0, pos_uint32.As<Number>()->Value());
  EXPECT_EQ(42u, pos_smi->Uint32Value(context()).FromJust());

  Local<Value> neg_smi = Integer::New(isolate(), -37);
  Local<Uint32> neg_uint32 = neg_smi->ToUint32(context()).ToLocalChecked();
  EXPECT_TRUE(neg_uint32->IsUint32());
  EXPECT_EQ(4294967259u, neg_uint32->Value());
  EXPECT_EQ(4294967259.0, neg_uint32.As<Number>()->Value());
  EXPECT_EQ(4294967259u, neg_smi->Uint32Value(context()).FromJust());

  Local<Value> neg_str = String::NewFromUtf8Literal(isolate(), "-37");
  Local<Uint32> str_uint32 = neg_str->ToUint32(context()).ToLocalChecked();
  EXPECT_TRUE(str_uint32->IsUint32());
  EXPECT_EQ(4294967259u, str_uint32->Value());
  EXPECT_EQ(4294967259.0, str_uint32.As<Number>()->Value());
  EXPECT_EQ(4294967259u, neg_str->Uint32Value(context()).FromJust());
}

}  // namespace
}  // namespace v8
