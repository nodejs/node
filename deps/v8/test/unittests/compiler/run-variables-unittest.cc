// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <array>
#include <span>
#include <string_view>

#include "src/api/api-inl.h"
#include "src/objects/objects-inl.h"
#include "test/unittests/compiler/function-tester.h"
#include "test/unittests/test-utils.h"

namespace v8::internal::compiler {

namespace {
v8::Local<v8::Value> CompileRun(v8::Isolate* isolate, const char* source) {
  v8::Local<v8::Context> context = isolate->GetCurrentContext();
  v8::Local<v8::Script> script =
      v8::Script::Compile(
          context, v8::String::NewFromUtf8(isolate, source).ToLocalChecked())
          .ToLocalChecked();
  return script->Run(context).ToLocalChecked();
}

struct TestCase {
  std::string_view code;
  std::string_view expected_truthy;
  std::string_view expected_falsy;

  constexpr TestCase(std::string_view c, std::string_view t, std::string_view f)
      : code(c), expected_truthy(t), expected_falsy(f) {}
};

constexpr std::string_view kThrows = "throws";

constexpr auto kLoadTests =
    std::array{TestCase{"var x = a; r = x", "123", "0"},
               TestCase{"var x = (r = x)", "undefined", "undefined"},
               TestCase{"var x = (a?1:2); r = x", "1", "2"},
               TestCase{"const x = a; r = x", "123", "0"},
               TestCase{"const x = (a?3:4); r = x", "3", "4"},
               TestCase{"'use strict'; const x = a; r = x", "123", "0"},
               TestCase{"'use strict'; const x = (r = x)", kThrows, kThrows},
               TestCase{"'use strict'; const x = (a?5:6); r = x", "5", "6"},
               TestCase{"'use strict'; let x = a; r = x", "123", "0"},
               TestCase{"'use strict'; let x = (r = x)", kThrows, kThrows},
               TestCase{"'use strict'; let x = (a?7:8); r = x", "7", "8"}};

constexpr auto kStoreTests = std::array{
    TestCase{"var x = 1; x = a; r = x", "123", "0"},
    TestCase{"var x = (a?(x=4,2):3); r = x", "2", "3"},
    TestCase{"var x = (a?4:5); x = a; r = x", "123", "0"},
    // Assignments to 'const' are SyntaxErrors, handled by the parser,
    // hence we cannot test them here because they are early errors.
    TestCase{"'use strict'; let x = 1; x = a; r = x", "123", "0"},
    TestCase{"'use strict'; let x = (a?(x=4,2):3); r = x", kThrows, "3"},
    TestCase{"'use strict'; let x = (a?4:5); x = a; r = x", "123", "0"}};
}  // namespace

using RunVariablesTest = TestWithContext;

static void RunVariableTests(Isolate* isolate, std::string_view source_format,
                             std::span<const TestCase> test_cases) {
  for (size_t i = 0; i < test_cases.size(); ++i) {
    const auto& test_case = test_cases[i];
    base::EmbeddedVector<char, 512> buffer;
    SNPrintF(buffer, source_format.data(), test_case.code.data());
    PrintF("#%zu: %s\n", i, buffer.begin());

    FunctionTester tester(isolate, buffer.begin());

    // Check function with non-falsey parameter.
    if (test_case.expected_truthy != kThrows) {
      DirectHandle<Object> expected_result = v8::Utils::OpenDirectHandle(
          *CompileRun(reinterpret_cast<v8::Isolate*>(isolate),
                      test_case.expected_truthy.data()));
      tester.CheckCall(expected_result, tester.NewNumber(123),
                       tester.NewString("result"));
    } else {
      tester.CheckThrows(tester.NewNumber(123), tester.NewString("result"));
    }

    // Check function with falsey parameter.
    if (test_case.expected_falsy != kThrows) {
      DirectHandle<Object> expected_result = v8::Utils::OpenDirectHandle(
          *CompileRun(reinterpret_cast<v8::Isolate*>(isolate),
                      test_case.expected_falsy.data()));
      tester.CheckCall(expected_result, tester.NewNumber(0.0),
                       tester.NewString("result"));
    } else {
      tester.CheckThrows(tester.NewNumber(0.0), tester.NewString("result"));
    }
  }
}

TEST_F(RunVariablesTest, StackLoadVariables) {
  constexpr std::string_view source = "(function(a,r) { %s; return r; })";
  RunVariableTests(i_isolate(), source, kLoadTests);
}

TEST_F(RunVariablesTest, ContextLoadVariables) {
  constexpr std::string_view source =
      "(function(a,r) { %s; function f() {x} return r; })";
  RunVariableTests(i_isolate(), source, kLoadTests);
}

TEST_F(RunVariablesTest, StackStoreVariables) {
  constexpr std::string_view source = "(function(a,r) { %s; return r; })";
  RunVariableTests(i_isolate(), source, kStoreTests);
}

TEST_F(RunVariablesTest, ContextStoreVariables) {
  constexpr std::string_view source =
      "(function(a,r) { %s; function f() {x} return r; })";
  RunVariableTests(i_isolate(), source, kStoreTests);
}

TEST_F(RunVariablesTest, SelfReferenceVariable) {
  FunctionTester T(i_isolate(), "(function self() { return self; })");

  T.CheckCall(T.function);
  CompileRun(isolate(), "var self = 'not a function'");
  T.CheckCall(T.function);
}

}  // namespace v8::internal::compiler
