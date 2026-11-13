// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_UNITTESTS_COMPILER_FUNCTION_TESTER_H_
#define V8_UNITTESTS_COMPILER_FUNCTION_TESTER_H_

#include "src/compiler/js-heap-broker.h"
#include "src/compiler/turbofan-graph.h"
#include "src/execution/execution.h"
#include "src/handles/handles.h"
#include "test/unittests/test-utils.h"

namespace v8 {
namespace internal {

namespace compiler {

class FunctionTester {
 public:
  explicit FunctionTester(Isolate* i_isolate, const char* source,
                          uint32_t flags = 0);

  FunctionTester(Isolate* i_isolate, TFGraph* graph, int param_count);

  FunctionTester(Isolate* i_isolate, Handle<InstructionStream> code,
                 int param_count);

  // Assumes JSTrampolineDescriptor call interface.
  // TODO(ishell): deduce param count from Code object and make sure it works
  // for kDontAdaptArgumentsSentinel.
  explicit FunctionTester(Isolate* i_isolate, DirectHandle<Code> code,
                          int param_count = 0);

  Isolate* isolate;
  Handle<JSFunction> function;

  MaybeHandle<Object> Call() {
    return Execution::Call(isolate, function, undefined(), {});
  }

  template <typename Arg1, typename... Args>
  MaybeHandle<Object> Call(Arg1 arg1, Args... args) {
    const int nof_args = sizeof...(Args) + 1;
    DirectHandle<Object> call_args[] = {arg1, args...};
    return Execution::Call(isolate, function, undefined(),
                           {call_args, nof_args});
  }

  template <typename T, typename... Args>
  DirectHandle<T> CallChecked(Args... args) {
    DirectHandle<Object> result = Call(args...).ToHandleChecked();
    return Cast<T>(result);
  }

  void CheckThrows(DirectHandle<Object> a);
  void CheckThrows(DirectHandle<Object> a, DirectHandle<Object> b);
  v8::Local<v8::Message> CheckThrowsReturnMessage(DirectHandle<Object> a);
  v8::Local<v8::Message> CheckThrowsReturnMessage(DirectHandle<Object> a,
                                                  DirectHandle<Object> b);
  void CheckCall(DirectHandle<Object> expected, DirectHandle<Object> a,
                 DirectHandle<Object> b, DirectHandle<Object> c,
                 DirectHandle<Object> d);

  void CheckCall(DirectHandle<Object> expected, DirectHandle<Object> a,
                 DirectHandle<Object> b, DirectHandle<Object> c) {
    return CheckCall(expected, a, b, c, undefined());
  }

  void CheckCall(DirectHandle<Object> expected, DirectHandle<Object> a,
                 DirectHandle<Object> b) {
    return CheckCall(expected, a, b, undefined());
  }

  void CheckCall(DirectHandle<Object> expected, DirectHandle<Object> a) {
    CheckCall(expected, a, undefined());
  }

  void CheckCall(DirectHandle<Object> expected) {
    CheckCall(expected, undefined());
  }

  void CheckCall(double expected, double a, double b) {
    CheckCall(NewNumber(expected), NewNumber(a), NewNumber(b));
  }

  void CheckTrue(DirectHandle<Object> a) { CheckCall(true_value(), a); }

  void CheckTrue(DirectHandle<Object> a, DirectHandle<Object> b) {
    CheckCall(true_value(), a, b);
  }

  void CheckTrue(DirectHandle<Object> a, DirectHandle<Object> b,
                 DirectHandle<Object> c) {
    CheckCall(true_value(), a, b, c);
  }

  void CheckTrue(DirectHandle<Object> a, DirectHandle<Object> b,
                 DirectHandle<Object> c, DirectHandle<Object> d) {
    CheckCall(true_value(), a, b, c, d);
  }

  void CheckTrue(double a, double b) {
    CheckCall(true_value(), NewNumber(a), NewNumber(b));
  }

  void CheckFalse(DirectHandle<Object> a) { CheckCall(false_value(), a); }

  void CheckFalse(DirectHandle<Object> a, DirectHandle<Object> b) {
    CheckCall(false_value(), a, b);
  }

  void CheckFalse(double a, double b) {
    CheckCall(false_value(), NewNumber(a), NewNumber(b));
  }

  Handle<JSFunction> NewFunction(const char* source);
  Handle<JSObject> NewObject(const char* source);

  Handle<String> NewString(const char* string);
  Handle<Object> NewNumber(double value);
  DirectHandle<Object> infinity();
  DirectHandle<Object> minus_infinity();
  Handle<Object> nan();
  Handle<Object> undefined();
  Handle<Object> null();
  Handle<Object> true_value();
  Handle<Object> false_value();

 private:
  uint32_t flags_;

  Handle<JSFunction> Compile(Handle<JSFunction> function);
  std::string BuildFunction(int param_count) {
    std::string function_string = "(function(";
    if (param_count > 0) {
      function_string += 'a';
      for (int i = 1; i < param_count; i++) {
        function_string += ',';
        function_string += static_cast<char>('a' + i);
      }
    }
    function_string += "){})";
    return function_string;
  }

  // Compile the given machine graph instead of the source of the function
  // and replace the JSFunction's code with the result.
  Handle<JSFunction> CompileGraph(TFGraph* graph);

  // Takes a JSFunction and runs it through the test version of the optimizing
  // pipeline, allocating the temporary compilation artifacts in a given Zone.
  // For possible {flags} values, look at OptimizedCompilationInfo::Flag.
  Handle<JSFunction> Optimize(Handle<JSFunction> function, Zone* zone,
                              uint32_t flags);
};

}  // namespace compiler
}  // namespace internal
}  // namespace v8

#endif  // V8_UNITTESTS_COMPILER_FUNCTION_TESTER_H_
