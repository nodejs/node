// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_TEST_CCTEST_INTERPRETER_INTERPRETER_TESTER_H_
#define V8_TEST_CCTEST_INTERPRETER_INTERPRETER_TESTER_H_

#include "src/v8.h"

#include "src/api.h"
#include "src/execution.h"
#include "src/handles.h"
#include "src/interpreter/bytecode-array-builder.h"
#include "src/interpreter/interpreter.h"
#include "test/cctest/cctest.h"
#include "test/cctest/test-feedback-vector.h"

namespace v8 {
namespace internal {
namespace interpreter {

MaybeHandle<Object> CallInterpreter(Isolate* isolate,
                                    Handle<JSFunction> function);
template <class... A>
static MaybeHandle<Object> CallInterpreter(Isolate* isolate,
                                           Handle<JSFunction> function,
                                           A... args) {
  Handle<Object> argv[] = {args...};
  return Execution::Call(isolate, function,
                         isolate->factory()->undefined_value(), sizeof...(args),
                         argv);
}

template <class... A>
class InterpreterCallable {
 public:
  InterpreterCallable(Isolate* isolate, Handle<JSFunction> function)
      : isolate_(isolate), function_(function) {}
  virtual ~InterpreterCallable() {}

  MaybeHandle<Object> operator()(A... args) {
    return CallInterpreter(isolate_, function_, args...);
  }

  FeedbackVector* vector() const { return function_->feedback_vector(); }

 private:
  Isolate* isolate_;
  Handle<JSFunction> function_;
};

class InterpreterTester {
 public:
  InterpreterTester(Isolate* isolate, const char* source,
                    MaybeHandle<BytecodeArray> bytecode,
                    MaybeHandle<FeedbackMetadata> feedback_metadata,
                    const char* filter);

  InterpreterTester(Isolate* isolate, Handle<BytecodeArray> bytecode,
                    MaybeHandle<FeedbackMetadata> feedback_metadata =
                        MaybeHandle<FeedbackMetadata>(),
                    const char* filter = kFunctionName);

  InterpreterTester(Isolate* isolate, const char* source,
                    const char* filter = kFunctionName);

  virtual ~InterpreterTester();

  template <class... A>
  InterpreterCallable<A...> GetCallable() {
    return InterpreterCallable<A...>(isolate_, GetBytecodeFunction<A...>());
  }

  Local<Message> CheckThrowsReturnMessage();

  static Handle<Object> NewObject(const char* script);

  static Handle<String> GetName(Isolate* isolate, const char* name);

  static std::string SourceForBody(const char* body);

  static std::string function_name();

  static const char kFunctionName[];

 private:
  Isolate* isolate_;
  const char* source_;
  MaybeHandle<BytecodeArray> bytecode_;
  MaybeHandle<FeedbackMetadata> feedback_metadata_;

  template <class... A>
  Handle<JSFunction> GetBytecodeFunction() {
    Handle<JSFunction> function;
    if (source_) {
      CompileRun(source_);
      v8::Local<v8::Context> context =
          v8::Isolate::GetCurrent()->GetCurrentContext();
      Local<Function> api_function =
          Local<Function>::Cast(CcTest::global()
                                    ->Get(context, v8_str(kFunctionName))
                                    .ToLocalChecked());
      function = Handle<JSFunction>::cast(v8::Utils::OpenHandle(*api_function));
    } else {
      int arg_count = sizeof...(A);
      std::string source("(function " + function_name() + "(");
      for (int i = 0; i < arg_count; i++) {
        source += i == 0 ? "a" : ", a";
      }
      source += "){})";
      function = Handle<JSFunction>::cast(v8::Utils::OpenHandle(
          *v8::Local<v8::Function>::Cast(CompileRun(source.c_str()))));
      function->set_code(*BUILTIN_CODE(isolate_, InterpreterEntryTrampoline));
    }

    if (!bytecode_.is_null()) {
      function->shared()->set_function_data(*bytecode_.ToHandleChecked());
    }
    if (!feedback_metadata_.is_null()) {
      function->set_feedback_vector_cell(isolate_->heap()->undefined_cell());
      function->shared()->set_feedback_metadata(
          *feedback_metadata_.ToHandleChecked());
      JSFunction::EnsureLiterals(function);
    }
    return function;
  }

  DISALLOW_COPY_AND_ASSIGN(InterpreterTester);
};

}  // namespace interpreter
}  // namespace internal
}  // namespace v8

#endif  // V8_TEST_CCTEST_INTERPRETER_INTERPRETER_TESTER_H_
