// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "include/v8-function.h"
#include "src/flags/flags.h"
#include "test/cctest/test-api.h"
#include "test/common/node-observer-tester.h"

namespace v8 {
namespace internal {
namespace compiler {

void CompileRunWithNodeObserver(const std::string& js_code,
                                int32_t expected_result,
                                IrOpcode::Value initial_call_opcode,
                                IrOpcode::Value updated_call_opcode1,
                                IrOpcode::Value updated_call_opcode2) {
  LocalContext env;
  v8::Isolate* isolate = env->GetIsolate();
  v8::HandleScope handle_scope(isolate);

  v8_flags.allow_natives_syntax = true;
  v8_flags.turbo_optimize_apply = true;

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
    ObserveNodeScope scope(reinterpret_cast<i::Isolate*>(isolate),
                           &apply_call_observer);

    v8::Local<v8::Value> result_value = CompileRun(js_code.c_str());

    CHECK(result_value->IsNumber());
    int32_t result =
        ConvertJSValue<int32_t>::Get(result_value, env.local()).ToChecked();
    CHECK_EQ(result, expected_result);
  }
}

TEST(ReduceJSCallWithArrayLike) {
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

TEST(ReduceJSCallWithSpread) {
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

TEST(ReduceJSCreateClosure) {
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

TEST(ReduceJSCreateBoundFunction) {
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

static void SumF(const v8::FunctionCallbackInfo<v8::Value>& args) {
  ApiTestFuzzer::Fuzz();
  v8::Local<v8::Context> context = args.GetIsolate()->GetCurrentContext();
  int this_x = args.This()
                   ->Get(context, v8_str("x"))
                   .ToLocalChecked()
                   ->Int32Value(context)
                   .FromJust();
  args.GetReturnValue().Set(v8_num(
      args[0]->Int32Value(args.GetIsolate()->GetCurrentContext()).FromJust() +
      args[1]->Int32Value(args.GetIsolate()->GetCurrentContext()).FromJust() +
      this_x));
}

TEST(ReduceCAPICallWithArrayLike) {
  LocalContext env;
  v8::Isolate* isolate = env->GetIsolate();
  v8::HandleScope scope(isolate);

  v8_flags.allow_natives_syntax = true;
  v8_flags.turbo_optimize_apply = true;

  Local<v8::FunctionTemplate> sum = v8::FunctionTemplate::New(isolate, SumF);
  CHECK(env->Global()
            ->Set(env.local(), v8_str("sum"),
                  sum->GetFunction(env.local()).ToLocalChecked())
            .FromJust());

  Local<v8::FunctionTemplate> fun = v8::FunctionTemplate::New(isolate);
  v8::Local<v8::String> class_name = v8_str("the_class_name");
  fun->SetClassName(class_name);
  Local<ObjectTemplate> templ1 = ObjectTemplate::New(isolate, fun);
  templ1->Set(isolate, "x", v8_num(42));
  templ1->Set(isolate, "foo", sum);
  Local<v8::Object> instance1 =
      templ1->NewInstance(env.local()).ToLocalChecked();
  CHECK(env->Global()->Set(env.local(), v8_str("p"), instance1).FromJust());

  std::string js_code =
      "function bar(a, b) { return sum.apply(p, [a, b]); }"
      "%PrepareFunctionForOptimization(bar);"
      "bar(20, 22);"
      "%OptimizeFunctionOnNextCall(bar);"
      "bar(20, 22);";
  v8::Local<v8::Value> result_value = CompileRun(js_code.c_str());
  CHECK(result_value->IsNumber());
  int32_t result =
      ConvertJSValue<int32_t>::Get(result_value, env.local()).ToChecked();
  CHECK_EQ(result, 84);
}

}  // namespace compiler
}  // namespace internal
}  // namespace v8
