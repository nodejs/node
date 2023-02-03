// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "include/v8-context.h"
#include "include/v8-isolate.h"
#include "include/v8-local-handle.h"
#include "include/v8-primitive.h"
#include "include/v8-template.h"
#include "src/objects/objects-inl.h"
#include "test/unittests/test-utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace v8 {
namespace {

namespace {
bool ValueEqualsString(v8::Isolate* isolate, Local<Value> lhs,
                       const char* rhs) {
  CHECK(!lhs.IsEmpty());
  CHECK(lhs->IsString());
  String::Utf8Value utf8_lhs(isolate, lhs);
  return strcmp(rhs, *utf8_lhs) == 0;
}

std::string from_v8_string(Isolate* isolate, Local<String> str) {
  String::Utf8Value utf8(isolate, str);
  return *utf8;
}

v8::MaybeLocal<Module> ResolveToTopLevelAwait(Local<Context> context,
                                              Local<String> specifier,
                                              Local<FixedArray> assertions,
                                              Local<Module> referrer) {
  v8::Isolate* isolate = v8::Isolate::GetCurrent();
  v8::ScriptOrigin origin(isolate, specifier, 0, 0, false, -1, Local<Value>(),
                          false, false, true);

  String::Utf8Value specifier_string(isolate, specifier);
  std::string source_string =
      "const promise = new Promise((resolve, reject) => {\n"
      "  if (should_resolve) {\n"
      "    resolve();\n"
      "  }\n"
      "});\n"
      "await promise;\n";
  if (strncmp(*specifier_string, "stall", strlen("stall")) == 0) {
    source_string = "const should_resolve = false;\n" + source_string;
  } else if (strncmp(*specifier_string, "resolve", strlen("resolve")) == 0) {
    source_string = "const should_resolve = true;\n" + source_string;
  } else {
    UNREACHABLE();
  }

  v8::ScriptCompiler::Source source(
      v8::String::NewFromUtf8(isolate, source_string.c_str()).ToLocalChecked(),
      origin);
  auto res = v8::ScriptCompiler::CompileModule(isolate, &source);
  return res;
}

class ScriptTest : public TestWithContext {
 protected:
  void TestGetStalledTopLevelAwaitMessage(
      const char* source_str, std::vector<std::string> expected_stalled) {
    v8::Isolate::Scope iscope(isolate());
    v8::HandleScope scope(isolate());
    v8::Local<v8::Context> context = v8::Context::New(isolate());
    v8::Context::Scope cscope(context);

    v8::ScriptOrigin origin(isolate(), NewString("root.mjs"), 0, 0, false, -1,
                            Local<Value>(), false, false, true);
    v8::ScriptCompiler::Source source(NewString(source_str), origin);
    Local<Module> root =
        v8::ScriptCompiler::CompileModule(isolate(), &source).ToLocalChecked();

    CHECK(root->InstantiateModule(context, ResolveToTopLevelAwait)
              .FromMaybe(false));

    Local<v8::Promise> promise =
        root->Evaluate(context).ToLocalChecked().As<v8::Promise>();
    isolate()->PerformMicrotaskCheckpoint();
    CHECK_EQ(expected_stalled.size() > 0
                 ? v8::Promise::PromiseState::kPending
                 : v8::Promise::PromiseState::kFulfilled,
             promise->State());

    std::vector<std::tuple<Local<Module>, Local<Message>>> stalled =
        root->GetStalledTopLevelAwaitMessage(isolate());
    CHECK_EQ(expected_stalled.size(), stalled.size());
    for (size_t i = 0; i < stalled.size(); ++i) {
      Local<Message> message = std::get<1>(stalled[i]);
      CHECK_EQ("Top-level await promise never resolved",
               from_v8_string(isolate(), message->Get()));
      CHECK_EQ(expected_stalled[i],
               from_v8_string(isolate(),
                              message->GetScriptResourceName().As<String>()));
      CHECK_EQ(
          "await promise;",
          from_v8_string(isolate(),
                         message->GetSourceLine(context).ToLocalChecked()));

      CHECK_EQ(7, message->GetLineNumber(context).ToChecked());
      CHECK_EQ(0, message->GetStartColumn(context).ToChecked());
      CHECK_EQ(1, message->GetEndColumn(context).ToChecked());
    }
  }
};

}  // namespace

TEST_F(ScriptTest, UnboundScriptPosition) {
  const char* url = "http://www.foo.com/foo.js";
  v8::ScriptOrigin origin(isolate(), NewString(url), 13, 0);
  v8::ScriptCompiler::Source script_source(NewString("var foo;"), origin);

  Local<Script> script =
      v8::ScriptCompiler::Compile(v8_context(), &script_source)
          .ToLocalChecked();
  EXPECT_TRUE(ValueEqualsString(
      isolate(), script->GetUnboundScript()->GetScriptName(), url));
  Local<UnboundScript> unbound_script = script->GetUnboundScript();

  int line_number = unbound_script->GetLineNumber();
  EXPECT_EQ(13, line_number);
  int column_number = unbound_script->GetColumnNumber();
  EXPECT_EQ(0, column_number);
}

TEST_F(ScriptTest, GetSingleStalledTopLevelAwaitMessage) {
  TestGetStalledTopLevelAwaitMessage("import 'stall.mjs';", {"stall.mjs"});
}

TEST_F(ScriptTest, GetMultipleStalledTopLevelAwaitMessage) {
  TestGetStalledTopLevelAwaitMessage(
      "import 'stall.mjs';\n"
      "import 'stall_2.mjs';\n"
      "import 'stall_3.mjs';\n"
      "import 'stall_4.mjs';\n",
      {"stall.mjs", "stall_2.mjs", "stall_3.mjs", "stall_4.mjs"});
}

TEST_F(ScriptTest, GetMixedStalledTopLevelAwaitMessage) {
  TestGetStalledTopLevelAwaitMessage(
      "import 'stall.mjs';\n"
      "import 'resolve.mjs';\n"
      "import 'stall_2.mjs';\n"
      "import 'resolve.mjs';\n",
      {"stall.mjs", "stall_2.mjs"});
}

TEST_F(ScriptTest, GetEmptyStalledTopLevelAwaitMessage) {
  TestGetStalledTopLevelAwaitMessage(
      "import 'resolve.mjs';\n"
      "import 'resolve_2.mjs';\n"
      "import 'resolve_3.mjs';\n",
      {});
}

}  // namespace
}  // namespace v8
