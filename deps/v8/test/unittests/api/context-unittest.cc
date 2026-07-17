// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "include/libplatform/libplatform.h"
#include "include/v8-context.h"
#include "include/v8-data.h"
#include "include/v8-isolate.h"
#include "include/v8-local-handle.h"
#include "include/v8-value.h"
#include "test/unittests/test-utils.h"
#include "testing/gtest/include/gtest/gtest.h"

using ContextTest = v8::TestWithIsolate;

TEST_F(ContextTest, HasTemplateLiteralObjectBasic) {
  v8::Local<v8::Context> context = v8::Context::New(isolate());
  v8::Context::Scope scope(context);
  ASSERT_FALSE(
      context->HasTemplateLiteralObject(v8::Number::New(isolate(), 1)));
  ASSERT_FALSE(context->HasTemplateLiteralObject(v8::String::Empty(isolate())));
  ASSERT_FALSE(
      context->HasTemplateLiteralObject(v8::Array::New(isolate(), 10)));
}

TEST_F(ContextTest, HasTemplateLiteralObject) {
  const char* source = R"(
    function ret(literal) {
      return literal;
    };
    ret`one_${'two'}_three`;
  )";
  const char* otherObject1Source = R"(
    Object.freeze(
      Object.defineProperty(['one_', '_three'], 'raw', {
        value: ['asdf'],
        writable: false,
        enumerable: false,
        configurable: false,
      })
    );
  )";
  const char* otherObject2Source = R"(
    Object.freeze(
      Object.defineProperty(['one_', '_three'], 'raw', {
        get() { return ['asdf']; },
        enumerable: false,
        configurable: false,
      })
    );
  )";

  v8::Local<v8::Context> context1 = v8::Context::New(isolate());
  v8::Local<v8::Value> templateLiteral1;
  v8::Local<v8::Value> templateLiteral1_2;
  v8::Local<v8::Value> otherObject1_ctx1;
  v8::Local<v8::Value> otherObject2_ctx1;
  {
    v8::Context::Scope scope(context1);
    auto script =
        v8::Script::Compile(context1, NewString(source)).ToLocalChecked();
    templateLiteral1 = script->Run(context1).ToLocalChecked();
    templateLiteral1_2 = script->Run(context1).ToLocalChecked();
    otherObject1_ctx1 = RunJS(context1, otherObject1Source);
    otherObject2_ctx1 = RunJS(context1, otherObject2Source);
  }

  v8::Local<v8::Value> templateLiteral2;
  v8::Local<v8::Context> context2 = v8::Context::New(isolate());
  v8::Local<v8::Value> otherObject1_ctx2;
  v8::Local<v8::Value> otherObject2_ctx2;
  {
    v8::Context::Scope scope(context2);
    templateLiteral2 = RunJS(context2, source);
    otherObject1_ctx2 = RunJS(context2, otherObject1Source);
    otherObject2_ctx2 = RunJS(context1, otherObject2Source);
  }

  ASSERT_TRUE(context1->HasTemplateLiteralObject(templateLiteral1));
  ASSERT_TRUE(context1->HasTemplateLiteralObject(templateLiteral1_2));
  ASSERT_FALSE(context1->HasTemplateLiteralObject(templateLiteral2));

  ASSERT_FALSE(context2->HasTemplateLiteralObject(templateLiteral1));
  ASSERT_FALSE(context2->HasTemplateLiteralObject(templateLiteral1_2));
  ASSERT_TRUE(context2->HasTemplateLiteralObject(templateLiteral2));

  // Neither otherObject is a template object
  ASSERT_FALSE(context1->HasTemplateLiteralObject(otherObject1_ctx1));
  ASSERT_FALSE(context1->HasTemplateLiteralObject(otherObject2_ctx1));
  ASSERT_FALSE(context1->HasTemplateLiteralObject(otherObject1_ctx2));
  ASSERT_FALSE(context1->HasTemplateLiteralObject(otherObject1_ctx1));
  ASSERT_FALSE(context2->HasTemplateLiteralObject(otherObject2_ctx1));
  ASSERT_FALSE(context2->HasTemplateLiteralObject(otherObject1_ctx2));
  ASSERT_FALSE(context2->HasTemplateLiteralObject(otherObject2_ctx2));
  ASSERT_FALSE(context2->HasTemplateLiteralObject(otherObject2_ctx2));
}
