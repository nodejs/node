// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "include/v8-context.h"
#include "include/v8-function.h"
#include "include/v8-isolate.h"
#include "include/v8-local-handle.h"
#include "include/v8-primitive.h"
#include "include/v8-template.h"
#include "src/api/api.h"
#include "src/objects/objects-inl.h"
#include "test/unittests/test-utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace v8 {
namespace {

using ScriptTest = TestWithContext;

namespace {
bool ValueEqualsString(v8::Isolate* isolate, Local<Value> lhs,
                       const char* rhs) {
  CHECK(!lhs.IsEmpty());
  CHECK(lhs->IsString());
  String::Utf8Value utf8_lhs(isolate, lhs);
  return strcmp(rhs, *utf8_lhs) == 0;
}
}  // namespace

TEST_F(ScriptTest, UnboundScriptPosition) {
  const char* url = "http://www.foo.com/foo.js";
  v8::ScriptOrigin origin(isolate(), NewString(url), 13, 0);
  v8::ScriptCompiler::Source script_source(NewString("var foo;"), origin);

  Local<Script> script =
    v8::ScriptCompiler::Compile(v8_context(), &script_source).ToLocalChecked();
  EXPECT_TRUE(
      ValueEqualsString(isolate(), script->GetUnboundScript()->GetScriptName(),
      url));
  Local<UnboundScript> unbound_script =  script->GetUnboundScript();

  int line_number = unbound_script->GetLineNumber();
  EXPECT_EQ(13, line_number);
  int column_number = unbound_script->GetColumnNumber();
  EXPECT_EQ(0, column_number);
}


}  // namespace
}  // namespace v8
