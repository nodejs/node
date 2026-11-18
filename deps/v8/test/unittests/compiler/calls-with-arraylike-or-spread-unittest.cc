// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "include/v8-function.h"
#include "include/v8-template.h"
#include "src/flags/flags.h"
#include "test/common/flag-utils.h"
#include "test/common/node-observer-tester.h"
#include "test/unittests/test-utils.h"

namespace v8 {
namespace internal {
namespace compiler {
namespace calls_with_arraylike_or_spread_unittest {

class CallsWithArrayLikeOrSpreadTest : public TestWithContext {
 protected:
  void CompileRunWithNodeObserver(const std::string& js_code,
                                  int32_t expected_result,
                                  IrOpcode::Value initial_call_opcode,
                                  IrOpcode::Value updated_call_opcode1,
                                  IrOpcode::Value updated_call_opcode2) {
    FlagScope<bool> allow_natives_syntax(&v8_flags.allow_natives_syntax, true);
    FlagScope<bool> turbo_optimize_apply(&v8_flags.turbo_optimize_apply, true);

    // Note: Make sure to not capture stack locations (e.g. `this`) here since
    // these lambdas are executed on another thread.
    ModificationObserver apply_call_observer(
        [initial_call_opcode](const Node* node) {
          CHECK_EQ(initial_call_opcode, node->opcode());
        },
        [updated_call_opcode1, updated_call_opcode2](
            const Node* node,
            const ObservableNodeState& old_state) -> NodeObserver::Observation {
          if (updated_call_opcode1 == node->opcode()) {
            return NodeObserver::Observation::kContinue;
          } else {
            CHECK(updated_call_opcode2 == node->opcode());
            return NodeObserver::Observation::kStop;
          }
        });

    {
      ObserveNodeScope scope(i_isolate(), &apply_call_observer);

      v8::Local<v8::Value> result_value = RunJS(js_code.c_str());

      CHECK(result_value->IsNumber());
      int32_t result = result_value->Int32Value(context()).FromJust();
      CHECK_EQ(result, expected_result);
    }
  }
};

TEST_F(CallsWithArrayLikeOrSpreadTest, ReduceJSCallWithArrayLike) {
  CompileRunWithNodeObserver(
      "function sum_js3(a, b, c) { return a + b + c; }"
      "function foo(x, y, z) {"
      "  return %ObserveNode(sum_js3.apply(null, [x, y, z]));"
      "}"
      "%PrepareFunctionForOptimization(sum_js3);"
      "%PrepareFunctionForOptimization(foo);"
      "foo(41, 42, 43);"
      "%OptimizeFunctionOnNextCall(foo);"
      "foo(41, 42, 43);",
      126, IrOpcode::kJSCall,
      IrOpcode::kJSCall,  // not JSCallWithArrayLike
      IrOpcode::kPhi);    // JSCall => Phi when the call is inlined.
}

TEST_F(CallsWithArrayLikeOrSpreadTest, ReduceJSCallWithSpread) {
  CompileRunWithNodeObserver(
      "function sum_js3(a, b, c) { return a + b + c; }"
      "function foo(x, y, z) {"
      "  const numbers = [x, y, z];"
      "  return %ObserveNode(sum_js3(...numbers));"
      "}"
      "%PrepareFunctionForOptimization(sum_js3);"
      "%PrepareFunctionForOptimization(foo);"
      "foo(41, 42, 43);"
      "%OptimizeFunctionOnNextCall(foo);"
      "foo(41, 42, 43)",
      126, IrOpcode::kJSCallWithSpread,
      IrOpcode::kJSCall,  // not JSCallWithSpread
      IrOpcode::kPhi);
}

TEST_F(CallsWithArrayLikeOrSpreadTest, ReduceJSCreateClosure) {
  CompileRunWithNodeObserver(
      "function foo_closure() {"
      "  return function(a, b, c) {"
      "    return a + b + c;"
      "  }"
      "}"
      "const _foo_closure = foo_closure();"
      "%PrepareFunctionForOptimization(_foo_closure);"
      "function foo(x, y, z) {"
      "  return %ObserveNode(foo_closure().apply(null, [x, y, z]));"
      "}"
      "%PrepareFunctionForOptimization(foo_closure);"
      "%PrepareFunctionForOptimization(foo);"
      "foo(41, 42, 43);"
      "%OptimizeFunctionOnNextCall(foo_closure);"
      "%OptimizeFunctionOnNextCall(foo);"
      "foo(41, 42, 43)",
      126, IrOpcode::kJSCall,
      IrOpcode::kJSCall,  // not JSCallWithArrayLike
      IrOpcode::kPhi);
}

TEST_F(CallsWithArrayLikeOrSpreadTest, ReduceJSCreateBoundFunction) {
  CompileRunWithNodeObserver(
      "function sum_js3(a, b, c) {"
      "  return this.x + a + b + c;"
      "}"
      "function foo(x, y ,z) {"
      "  return %ObserveNode(sum_js3.bind({x : 42}).apply(null, [ x, y, z ]));"
      "}"
      "%PrepareFunctionForOptimization(sum_js3);"
      "%PrepareFunctionForOptimization(foo);"
      "foo(41, 42, 43);"
      "%OptimizeFunctionOnNextCall(foo);"
      "foo(41, 42, 43)",
      168, IrOpcode::kJSCall,
      IrOpcode::kJSCall,  // not JSCallWithArrayLike
      IrOpcode::kPhi);
}

static void SumF(const v8::FunctionCallbackInfo<v8::Value>& info) {
  CHECK(i::ValidateCallbackInfo(info));
  v8::Isolate* isolate = info.GetIsolate();
  v8::Local<v8::Context> context = isolate->GetCurrentContext();
  int this_x = info.This()
                   ->Get(context, v8::String::NewFromUtf8Literal(isolate, "x"))
                   .ToLocalChecked()
                   ->Int32Value(context)
                   .FromJust();
  info.GetReturnValue().Set(v8::Number::New(
      isolate, info[0]->Int32Value(context).FromJust() +
                   info[1]->Int32Value(context).FromJust() + this_x));
}

TEST_F(CallsWithArrayLikeOrSpreadTest, ReduceCAPICallWithArrayLike) {
  FlagScope<bool> allow_natives_syntax(&v8_flags.allow_natives_syntax, true);
  FlagScope<bool> turbo_optimize_apply(&v8_flags.turbo_optimize_apply, true);

  Local<v8::FunctionTemplate> sum = v8::FunctionTemplate::New(isolate(), SumF);
  CHECK(context()
            ->Global()
            ->Set(context(), NewString("sum"),
                  sum->GetFunction(context()).ToLocalChecked())
            .FromJust());

  Local<v8::FunctionTemplate> fun = v8::FunctionTemplate::New(isolate());
  v8::Local<v8::String> class_name = NewString("the_class_name");
  fun->SetClassName(class_name);
  Local<ObjectTemplate> templ1 = ObjectTemplate::New(isolate(), fun);
  templ1->Set(isolate(), "x", NewNumber(42));
  templ1->Set(isolate(), "foo", sum);
  Local<v8::Object> instance1 = templ1->NewInstance(context()).ToLocalChecked();
  CHECK(context()
            ->Global()
            ->Set(context(), NewString("p"), instance1)
            .FromJust());

  std::string js_code =
      "function bar(a, b) { return sum.apply(p, [a, b]); }"
      "%PrepareFunctionForOptimization(bar);"
      "bar(20, 22);"
      "%OptimizeFunctionOnNextCall(bar);"
      "bar(20, 22);";
  v8::Local<v8::Value> result_value = RunJS(js_code.c_str());
  CHECK(result_value->IsNumber());
  int32_t result = result_value->Int32Value(context()).FromJust();
  CHECK_EQ(result, 84);
}

}  // namespace calls_with_arraylike_or_spread_unittest
}  // namespace compiler
}  // namespace internal
}  // namespace v8
