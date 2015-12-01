// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>

#include "src/v8.h"

#include "src/compiler/pipeline.h"
#include "src/execution.h"
#include "src/handles.h"
#include "src/interpreter/bytecode-array-builder.h"
#include "src/interpreter/interpreter.h"
#include "src/parser.h"
#include "test/cctest/cctest.h"

namespace v8 {
namespace internal {
namespace compiler {


static const char kFunctionName[] = "f";


static MaybeHandle<Object> CallFunction(Isolate* isolate,
                                        Handle<JSFunction> function) {
  return Execution::Call(isolate, function,
                         isolate->factory()->undefined_value(), 0, nullptr);
}


template <class... A>
static MaybeHandle<Object> CallFunction(Isolate* isolate,
                                        Handle<JSFunction> function,
                                        A... args) {
  Handle<Object> argv[] = {args...};
  return Execution::Call(isolate, function,
                         isolate->factory()->undefined_value(), sizeof...(args),
                         argv);
}


template <class... A>
class BytecodeGraphCallable {
 public:
  BytecodeGraphCallable(Isolate* isolate, Handle<JSFunction> function)
      : isolate_(isolate), function_(function) {}
  virtual ~BytecodeGraphCallable() {}

  MaybeHandle<Object> operator()(A... args) {
    return CallFunction(isolate_, function_, args...);
  }

 private:
  Isolate* isolate_;
  Handle<JSFunction> function_;
};


class BytecodeGraphTester {
 public:
  BytecodeGraphTester(Isolate* isolate, Zone* zone, const char* script)
      : isolate_(isolate), zone_(zone), script_(script) {
    i::FLAG_ignition = true;
    i::FLAG_always_opt = false;
    i::FLAG_vector_stores = true;
    // Set ignition filter flag via SetFlagsFromString to avoid double-free
    // (or potential leak with StrDup() based on ownership confusion).
    ScopedVector<char> ignition_filter(64);
    SNPrintF(ignition_filter, "--ignition-filter=%s", kFunctionName);
    FlagList::SetFlagsFromString(ignition_filter.start(),
                                 ignition_filter.length());
    // Ensure handler table is generated.
    isolate->interpreter()->Initialize();
  }
  virtual ~BytecodeGraphTester() {}

  template <class... A>
  BytecodeGraphCallable<A...> GetCallable() {
    return BytecodeGraphCallable<A...>(isolate_, GetFunction());
  }

 private:
  Isolate* isolate_;
  Zone* zone_;
  const char* script_;

  Handle<JSFunction> GetFunction() {
    CompileRun(script_);
    Local<Function> api_function =
        Local<Function>::Cast(CcTest::global()->Get(v8_str(kFunctionName)));
    Handle<JSFunction> function = v8::Utils::OpenHandle(*api_function);
    CHECK(function->shared()->HasBytecodeArray());

    ParseInfo parse_info(zone_, function);

    CompilationInfo compilation_info(&parse_info);
    compilation_info.SetOptimizing(BailoutId::None(), Handle<Code>());
    Parser parser(&parse_info);
    CHECK(parser.Parse(&parse_info));
    compiler::Pipeline pipeline(&compilation_info);
    Handle<Code> code = pipeline.GenerateCode();
    function->ReplaceCode(*code);

    return function;
  }

  DISALLOW_COPY_AND_ASSIGN(BytecodeGraphTester);
};

}  // namespace compiler
}  // namespace internal
}  // namespace v8


using namespace v8::internal;
using namespace v8::internal::compiler;

template <int N>
struct ExpectedSnippet {
  const char* code_snippet;
  Handle<Object> return_value_and_parameters[N + 1];

  inline Handle<Object> return_value() const {
    return return_value_and_parameters[0];
  }

  inline Handle<Object> parameter(int i) const {
    return return_value_and_parameters[1 + i];
  }
};


TEST(BytecodeGraphBuilderReturnStatements) {
  HandleAndZoneScope scope;
  Isolate* isolate = scope.main_isolate();
  Zone* zone = scope.main_zone();
  Factory* factory = isolate->factory();

  ExpectedSnippet<0> snippets[] = {
      {"return;", {factory->undefined_value()}},
      {"return null;", {factory->null_value()}},
      {"return true;", {factory->true_value()}},
      {"return false;", {factory->false_value()}},
      {"return 0;", {factory->NewNumberFromInt(0)}},
      {"return +1;", {factory->NewNumberFromInt(1)}},
      {"return -1;", {factory->NewNumberFromInt(-1)}},
      {"return +127;", {factory->NewNumberFromInt(127)}},
      {"return -128;", {factory->NewNumberFromInt(-128)}},
      {"return 0.001;", {factory->NewNumber(0.001)}},
      {"return 3.7e-60;", {factory->NewNumber(3.7e-60)}},
      {"return -3.7e60;", {factory->NewNumber(-3.7e60)}},
      {"return '';", {factory->NewStringFromStaticChars("")}},
      {"return 'catfood';", {factory->NewStringFromStaticChars("catfood")}}
      // TODO(oth): {"return NaN;", {factory->NewNumber(NAN)}}
  };

  size_t num_snippets = sizeof(snippets) / sizeof(snippets[0]);
  for (size_t i = 0; i < num_snippets; i++) {
    ScopedVector<char> script(1024);
    SNPrintF(script, "function %s() { %s }\n%s();", kFunctionName,
             snippets[i].code_snippet, kFunctionName);

    BytecodeGraphTester tester(isolate, zone, script.start());
    auto callable = tester.GetCallable<>();
    Handle<Object> return_value = callable().ToHandleChecked();
    CHECK(return_value->SameValue(*snippets[i].return_value()));
  }
}


TEST(BytecodeGraphBuilderPrimitiveExpressions) {
  HandleAndZoneScope scope;
  Isolate* isolate = scope.main_isolate();
  Zone* zone = scope.main_zone();
  Factory* factory = isolate->factory();

  ExpectedSnippet<0> snippets[] = {
      {"return 1 + 1;", {factory->NewNumberFromInt(2)}},
      {"return 20 - 30;", {factory->NewNumberFromInt(-10)}},
      {"return 4 * 100;", {factory->NewNumberFromInt(400)}},
      {"return 100 / 5;", {factory->NewNumberFromInt(20)}},
      {"return 25 % 7;", {factory->NewNumberFromInt(4)}},
  };

  size_t num_snippets = sizeof(snippets) / sizeof(snippets[0]);
  for (size_t i = 0; i < num_snippets; i++) {
    ScopedVector<char> script(1024);
    SNPrintF(script, "function %s() { %s }\n%s();", kFunctionName,
             snippets[i].code_snippet, kFunctionName);

    BytecodeGraphTester tester(isolate, zone, script.start());
    auto callable = tester.GetCallable<>();
    Handle<Object> return_value = callable().ToHandleChecked();
    CHECK(return_value->SameValue(*snippets[i].return_value()));
  }
}


TEST(BytecodeGraphBuilderTwoParameterTests) {
  HandleAndZoneScope scope;
  Isolate* isolate = scope.main_isolate();
  Zone* zone = scope.main_zone();
  Factory* factory = isolate->factory();

  ExpectedSnippet<2> snippets[] = {
      // Integers
      {"return p1 + p2;",
       {factory->NewNumberFromInt(-70), factory->NewNumberFromInt(3),
        factory->NewNumberFromInt(-73)}},
      {"return p1 + p2 + 3;",
       {factory->NewNumberFromInt(1139044), factory->NewNumberFromInt(300),
        factory->NewNumberFromInt(1138741)}},
      {"return p1 - p2;",
       {factory->NewNumberFromInt(1100), factory->NewNumberFromInt(1000),
        factory->NewNumberFromInt(-100)}},
      {"return p1 * p2;",
       {factory->NewNumberFromInt(-100000), factory->NewNumberFromInt(1000),
        factory->NewNumberFromInt(-100)}},
      {"return p1 / p2;",
       {factory->NewNumberFromInt(-10), factory->NewNumberFromInt(1000),
        factory->NewNumberFromInt(-100)}},
      {"return p1 % p2;",
       {factory->NewNumberFromInt(5), factory->NewNumberFromInt(373),
        factory->NewNumberFromInt(16)}},
      // Doubles
      {"return p1 + p2;",
       {factory->NewHeapNumber(9.999), factory->NewHeapNumber(3.333),
        factory->NewHeapNumber(6.666)}},
      {"return p1 - p2;",
       {factory->NewHeapNumber(-3.333), factory->NewHeapNumber(3.333),
        factory->NewHeapNumber(6.666)}},
      {"return p1 * p2;",
       {factory->NewHeapNumber(3.333 * 6.666), factory->NewHeapNumber(3.333),
        factory->NewHeapNumber(6.666)}},
      {"return p1 / p2;",
       {factory->NewHeapNumber(2.25), factory->NewHeapNumber(9),
        factory->NewHeapNumber(4)}},
      // Strings
      {"return p1 + p2;",
       {factory->NewStringFromStaticChars("abcdef"),
        factory->NewStringFromStaticChars("abc"),
        factory->NewStringFromStaticChars("def")}}};

  size_t num_snippets = sizeof(snippets) / sizeof(snippets[0]);
  for (size_t i = 0; i < num_snippets; i++) {
    ScopedVector<char> script(1024);
    SNPrintF(script, "function %s(p1, p2) { %s }\n%s(0, 0);", kFunctionName,
             snippets[i].code_snippet, kFunctionName);

    BytecodeGraphTester tester(isolate, zone, script.start());
    auto callable = tester.GetCallable<Handle<Object>, Handle<Object>>();
    Handle<Object> return_value =
        callable(snippets[i].parameter(0), snippets[i].parameter(1))
            .ToHandleChecked();
    CHECK(return_value->SameValue(*snippets[i].return_value()));
  }
}
