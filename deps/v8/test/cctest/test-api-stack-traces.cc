// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "test/cctest/test-api.h"

#include "src/api/api-inl.h"

using ::v8::Array;
using ::v8::Context;
using ::v8::Local;
using ::v8::ObjectTemplate;
using ::v8::String;
using ::v8::TryCatch;
using ::v8::Value;

static v8::MaybeLocal<Value> PrepareStackTrace42(v8::Local<Context> context,
                                                 v8::Local<Value> error,
                                                 v8::Local<Array> trace) {
  return v8::Number::New(context->GetIsolate(), 42);
}

static v8::MaybeLocal<Value> PrepareStackTraceThrow(v8::Local<Context> context,
                                                    v8::Local<Value> error,
                                                    v8::Local<Array> trace) {
  v8::Isolate* isolate = context->GetIsolate();
  v8::Local<String> message = v8_str("42");
  isolate->ThrowException(v8::Exception::Error(message));
  return v8::MaybeLocal<Value>();
}

THREADED_TEST(IsolatePrepareStackTrace) {
  LocalContext context;
  v8::Isolate* isolate = context->GetIsolate();
  v8::HandleScope scope(isolate);

  isolate->SetPrepareStackTraceCallback(PrepareStackTrace42);

  v8::Local<Value> v = CompileRun("new Error().stack");

  CHECK(v->IsNumber());
  CHECK_EQ(v.As<v8::Number>()->Int32Value(context.local()).FromJust(), 42);
}

THREADED_TEST(IsolatePrepareStackTraceThrow) {
  LocalContext context;
  v8::Isolate* isolate = context->GetIsolate();
  v8::HandleScope scope(isolate);

  isolate->SetPrepareStackTraceCallback(PrepareStackTraceThrow);

  v8::Local<Value> v = CompileRun("try { new Error().stack } catch (e) { e }");

  CHECK(v->IsNativeError());

  v8::Local<String> message = v8::Exception::CreateMessage(isolate, v)->Get();

  CHECK(message->StrictEquals(v8_str("Uncaught Error: 42")));
}

static void ThrowV8Exception(const v8::FunctionCallbackInfo<v8::Value>& info) {
  ApiTestFuzzer::Fuzz();
  v8::Local<String> foo = v8_str("foo");
  v8::Local<String> message = v8_str("message");
  v8::Local<Value> error = v8::Exception::Error(foo);
  CHECK(error->IsObject());
  v8::Local<v8::Context> context = info.GetIsolate()->GetCurrentContext();
  CHECK(error.As<v8::Object>()
            ->Get(context, message)
            .ToLocalChecked()
            ->Equals(context, foo)
            .FromJust());
  info.GetIsolate()->ThrowException(error);
  info.GetReturnValue().SetUndefined();
}

THREADED_TEST(ExceptionCreateMessage) {
  LocalContext context;
  v8::HandleScope scope(context->GetIsolate());
  v8::Local<String> foo_str = v8_str("foo");
  v8::Local<String> message_str = v8_str("message");

  context->GetIsolate()->SetCaptureStackTraceForUncaughtExceptions(true);

  Local<v8::FunctionTemplate> fun =
      v8::FunctionTemplate::New(context->GetIsolate(), ThrowV8Exception);
  v8::Local<v8::Object> global = context->Global();
  CHECK(global
            ->Set(context.local(), v8_str("throwV8Exception"),
                  fun->GetFunction(context.local()).ToLocalChecked())
            .FromJust());

  TryCatch try_catch(context->GetIsolate());
  CompileRun(
      "function f1() {\n"
      "  throwV8Exception();\n"
      "};\n"
      "f1();");
  CHECK(try_catch.HasCaught());

  v8::Local<v8::Value> error = try_catch.Exception();
  CHECK(error->IsObject());
  CHECK(error.As<v8::Object>()
            ->Get(context.local(), message_str)
            .ToLocalChecked()
            ->Equals(context.local(), foo_str)
            .FromJust());

  v8::Local<v8::Message> message =
      v8::Exception::CreateMessage(context->GetIsolate(), error);
  CHECK(!message.IsEmpty());
  CHECK_EQ(2, message->GetLineNumber(context.local()).FromJust());
  CHECK_EQ(2, message->GetStartColumn(context.local()).FromJust());

  v8::Local<v8::StackTrace> stackTrace = message->GetStackTrace();
  CHECK(!stackTrace.IsEmpty());
  CHECK_EQ(2, stackTrace->GetFrameCount());

  stackTrace = v8::Exception::GetStackTrace(error);
  CHECK(!stackTrace.IsEmpty());
  CHECK_EQ(2, stackTrace->GetFrameCount());

  context->GetIsolate()->SetCaptureStackTraceForUncaughtExceptions(false);

  // Now check message location when SetCaptureStackTraceForUncaughtExceptions
  // is false.
  try_catch.Reset();

  CompileRun(
      "function f2() {\n"
      "  return throwV8Exception();\n"
      "};\n"
      "f2();");
  CHECK(try_catch.HasCaught());

  error = try_catch.Exception();
  CHECK(error->IsObject());
  CHECK(error.As<v8::Object>()
            ->Get(context.local(), message_str)
            .ToLocalChecked()
            ->Equals(context.local(), foo_str)
            .FromJust());

  message = v8::Exception::CreateMessage(context->GetIsolate(), error);
  CHECK(!message.IsEmpty());
  CHECK_EQ(2, message->GetLineNumber(context.local()).FromJust());
  CHECK_EQ(9, message->GetStartColumn(context.local()).FromJust());

  // Should be empty stack trace.
  stackTrace = message->GetStackTrace();
  CHECK(stackTrace.IsEmpty());
  CHECK(v8::Exception::GetStackTrace(error).IsEmpty());
}

// TODO(szuend): Re-enable as a threaded test once investigated and fixed.
// THREADED_TEST(StackTrace) {
TEST(StackTrace) {
  LocalContext context;
  v8::HandleScope scope(context->GetIsolate());
  v8::TryCatch try_catch(context->GetIsolate());
  const char* source = "function foo() { FAIL.FAIL; }; foo();";
  v8::Local<v8::String> src = v8_str(source);
  v8::Local<v8::String> origin = v8_str("stack-trace-test");
  v8::ScriptCompiler::Source script_source(src, v8::ScriptOrigin(origin));
  CHECK(v8::ScriptCompiler::CompileUnboundScript(context->GetIsolate(),
                                                 &script_source)
            .ToLocalChecked()
            ->BindToCurrentContext()
            ->Run(context.local())
            .IsEmpty());
  CHECK(try_catch.HasCaught());
  v8::String::Utf8Value stack(
      context->GetIsolate(),
      try_catch.StackTrace(context.local()).ToLocalChecked());
  CHECK_NOT_NULL(strstr(*stack, "at foo (stack-trace-test"));
}

// Checks that a StackFrame has certain expected values.
static void checkStackFrame(const char* expected_script_name,
                            const char* expected_func_name,
                            int expected_line_number, int expected_column,
                            bool is_eval, bool is_constructor,
                            v8::Local<v8::StackFrame> frame) {
  v8::HandleScope scope(CcTest::isolate());
  v8::String::Utf8Value func_name(CcTest::isolate(), frame->GetFunctionName());
  v8::String::Utf8Value script_name(CcTest::isolate(), frame->GetScriptName());
  if (*script_name == nullptr) {
    // The situation where there is no associated script, like for evals.
    CHECK_NULL(expected_script_name);
  } else {
    CHECK_NOT_NULL(strstr(*script_name, expected_script_name));
  }
  CHECK_NOT_NULL(strstr(*func_name, expected_func_name));
  CHECK_EQ(expected_line_number, frame->GetLineNumber());
  CHECK_EQ(expected_column, frame->GetColumn());
  CHECK_EQ(is_eval, frame->IsEval());
  CHECK_EQ(is_constructor, frame->IsConstructor());
  CHECK(frame->IsUserJavaScript());
}

static void AnalyzeStackInNativeCode(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  v8::HandleScope scope(args.GetIsolate());
  const char* origin = "capture-stack-trace-test";
  const int kOverviewTest = 1;
  const int kDetailedTest = 2;
  const int kFunctionName = 3;
  const int kDisplayName = 4;
  const int kFunctionNameAndDisplayName = 5;
  const int kDisplayNameIsNotString = 6;
  const int kFunctionNameIsNotString = 7;

  CHECK_EQ(args.Length(), 1);

  v8::Local<v8::Context> context = args.GetIsolate()->GetCurrentContext();
  v8::Isolate* isolate = args.GetIsolate();
  int testGroup = args[0]->Int32Value(context).FromJust();
  if (testGroup == kOverviewTest) {
    v8::Local<v8::StackTrace> stackTrace = v8::StackTrace::CurrentStackTrace(
        args.GetIsolate(), 10, v8::StackTrace::kOverview);
    CHECK_EQ(4, stackTrace->GetFrameCount());
    checkStackFrame(origin, "bar", 2, 10, false, false,
                    stackTrace->GetFrame(args.GetIsolate(), 0));
    checkStackFrame(origin, "foo", 6, 3, false, true,
                    stackTrace->GetFrame(isolate, 1));
    // This is the source string inside the eval which has the call to foo.
    checkStackFrame(nullptr, "", 1, 1, true, false,
                    stackTrace->GetFrame(isolate, 2));
    // The last frame is an anonymous function which has the initial eval call.
    checkStackFrame(origin, "", 8, 7, false, false,
                    stackTrace->GetFrame(isolate, 3));
  } else if (testGroup == kDetailedTest) {
    v8::Local<v8::StackTrace> stackTrace = v8::StackTrace::CurrentStackTrace(
        args.GetIsolate(), 10, v8::StackTrace::kDetailed);
    CHECK_EQ(4, stackTrace->GetFrameCount());
    checkStackFrame(origin, "bat", 4, 22, false, false,
                    stackTrace->GetFrame(isolate, 0));
    checkStackFrame(origin, "baz", 8, 3, false, true,
                    stackTrace->GetFrame(isolate, 1));
    bool is_eval = true;
    // This is the source string inside the eval which has the call to baz.
    checkStackFrame(nullptr, "", 1, 1, is_eval, false,
                    stackTrace->GetFrame(isolate, 2));
    // The last frame is an anonymous function which has the initial eval call.
    checkStackFrame(origin, "", 10, 1, false, false,
                    stackTrace->GetFrame(isolate, 3));
  } else if (testGroup == kFunctionName) {
    v8::Local<v8::StackTrace> stackTrace = v8::StackTrace::CurrentStackTrace(
        args.GetIsolate(), 5, v8::StackTrace::kOverview);
    CHECK_EQ(3, stackTrace->GetFrameCount());
    checkStackFrame(nullptr, "function.name", 3, 1, true, false,
                    stackTrace->GetFrame(isolate, 0));
  } else if (testGroup == kDisplayName) {
    v8::Local<v8::StackTrace> stackTrace = v8::StackTrace::CurrentStackTrace(
        args.GetIsolate(), 5, v8::StackTrace::kOverview);
    CHECK_EQ(3, stackTrace->GetFrameCount());
    checkStackFrame(nullptr, "function.displayName", 3, 1, true, false,
                    stackTrace->GetFrame(isolate, 0));
  } else if (testGroup == kFunctionNameAndDisplayName) {
    v8::Local<v8::StackTrace> stackTrace = v8::StackTrace::CurrentStackTrace(
        args.GetIsolate(), 5, v8::StackTrace::kOverview);
    CHECK_EQ(3, stackTrace->GetFrameCount());
    checkStackFrame(nullptr, "function.displayName", 3, 1, true, false,
                    stackTrace->GetFrame(isolate, 0));
  } else if (testGroup == kDisplayNameIsNotString) {
    v8::Local<v8::StackTrace> stackTrace = v8::StackTrace::CurrentStackTrace(
        args.GetIsolate(), 5, v8::StackTrace::kOverview);
    CHECK_EQ(3, stackTrace->GetFrameCount());
    checkStackFrame(nullptr, "function.name", 3, 1, true, false,
                    stackTrace->GetFrame(isolate, 0));
  } else if (testGroup == kFunctionNameIsNotString) {
    v8::Local<v8::StackTrace> stackTrace = v8::StackTrace::CurrentStackTrace(
        args.GetIsolate(), 5, v8::StackTrace::kOverview);
    CHECK_EQ(3, stackTrace->GetFrameCount());
    checkStackFrame(nullptr, "", 3, 1, true, false,
                    stackTrace->GetFrame(isolate, 0));
  }
}

// Tests the C++ StackTrace API.
// TODO(3074796): Reenable this as a THREADED_TEST once it passes.
// THREADED_TEST(CaptureStackTrace) {
TEST(CaptureStackTrace) {
  v8::Isolate* isolate = CcTest::isolate();
  v8::HandleScope scope(isolate);
  v8::Local<v8::String> origin = v8_str("capture-stack-trace-test");
  Local<ObjectTemplate> templ = ObjectTemplate::New(isolate);
  templ->Set(v8_str("AnalyzeStackInNativeCode"),
             v8::FunctionTemplate::New(isolate, AnalyzeStackInNativeCode));
  LocalContext context(nullptr, templ);

  // Test getting OVERVIEW information. Should ignore information that is not
  // script name, function name, line number, and column offset.
  const char* overview_source =
      "function bar() {\n"
      "  var y; AnalyzeStackInNativeCode(1);\n"
      "}\n"
      "function foo() {\n"
      "\n"
      "  bar();\n"
      "}\n"
      "var x;eval('new foo();');";
  v8::Local<v8::String> overview_src = v8_str(overview_source);
  v8::ScriptCompiler::Source script_source(overview_src,
                                           v8::ScriptOrigin(origin));
  v8::Local<Value> overview_result(
      v8::ScriptCompiler::CompileUnboundScript(isolate, &script_source)
          .ToLocalChecked()
          ->BindToCurrentContext()
          ->Run(context.local())
          .ToLocalChecked());
  CHECK(!overview_result.IsEmpty());
  CHECK(overview_result->IsObject());

  // Test getting DETAILED information.
  const char* detailed_source =
      "function bat() {AnalyzeStackInNativeCode(2);\n"
      "}\n"
      "\n"
      "function baz() {\n"
      "  bat();\n"
      "}\n"
      "eval('new baz();');";
  v8::Local<v8::String> detailed_src = v8_str(detailed_source);
  // Make the script using a non-zero line and column offset.
  v8::Local<v8::Integer> line_offset = v8::Integer::New(isolate, 3);
  v8::Local<v8::Integer> column_offset = v8::Integer::New(isolate, 5);
  v8::ScriptOrigin detailed_origin(origin, line_offset, column_offset);
  v8::ScriptCompiler::Source script_source2(detailed_src, detailed_origin);
  v8::Local<v8::UnboundScript> detailed_script(
      v8::ScriptCompiler::CompileUnboundScript(isolate, &script_source2)
          .ToLocalChecked());
  v8::Local<Value> detailed_result(detailed_script->BindToCurrentContext()
                                       ->Run(context.local())
                                       .ToLocalChecked());
  CHECK(!detailed_result.IsEmpty());
  CHECK(detailed_result->IsObject());

  // Test using function.name and function.displayName in stack trace
  const char* function_name_source =
      "function bar(function_name, display_name, testGroup) {\n"
      "  var f = new Function(`AnalyzeStackInNativeCode(${testGroup});`);\n"
      "  if (function_name) {\n"
      "    Object.defineProperty(f, 'name', { value: function_name });\n"
      "  }\n"
      "  if (display_name) {\n"
      "    f.displayName = display_name;"
      "  }\n"
      "  f()\n"
      "}\n"
      "bar('function.name', undefined, 3);\n"
      "bar(undefined, 'function.displayName', 4);\n"
      "bar('function.name', 'function.displayName', 5);\n"
      "bar('function.name', 239, 6);\n"
      "bar(239, undefined, 7);\n";
  v8::Local<v8::String> function_name_src =
      v8::String::NewFromUtf8(isolate, function_name_source,
                              v8::NewStringType::kNormal)
          .ToLocalChecked();
  v8::ScriptCompiler::Source script_source3(function_name_src,
                                            v8::ScriptOrigin(origin));
  v8::Local<Value> function_name_result(
      v8::ScriptCompiler::CompileUnboundScript(isolate, &script_source3)
          .ToLocalChecked()
          ->BindToCurrentContext()
          ->Run(context.local())
          .ToLocalChecked());
  CHECK(!function_name_result.IsEmpty());
}

static int report_count = 0;
static void StackTraceForUncaughtExceptionListener(
    v8::Local<v8::Message> message, v8::Local<Value>) {
  report_count++;
  v8::Local<v8::StackTrace> stack_trace = message->GetStackTrace();
  CHECK_EQ(2, stack_trace->GetFrameCount());
  checkStackFrame("origin", "foo", 2, 3, false, false,
                  stack_trace->GetFrame(message->GetIsolate(), 0));
  checkStackFrame("origin", "bar", 5, 3, false, false,
                  stack_trace->GetFrame(message->GetIsolate(), 1));
}

TEST(CaptureStackTraceForUncaughtException) {
  report_count = 0;
  LocalContext env;
  v8::Isolate* isolate = env->GetIsolate();
  v8::HandleScope scope(isolate);
  isolate->AddMessageListener(StackTraceForUncaughtExceptionListener);
  isolate->SetCaptureStackTraceForUncaughtExceptions(true);

  CompileRunWithOrigin(
      "function foo() {\n"
      "  throw 1;\n"
      "};\n"
      "function bar() {\n"
      "  foo();\n"
      "};",
      "origin");
  v8::Local<v8::Object> global = env->Global();
  Local<Value> trouble =
      global->Get(env.local(), v8_str("bar")).ToLocalChecked();
  CHECK(trouble->IsFunction());
  CHECK(v8::Function::Cast(*trouble)
            ->Call(env.local(), global, 0, nullptr)
            .IsEmpty());
  isolate->SetCaptureStackTraceForUncaughtExceptions(false);
  isolate->RemoveMessageListeners(StackTraceForUncaughtExceptionListener);
  CHECK_EQ(1, report_count);
}

TEST(CaptureStackTraceForUncaughtExceptionAndSetters) {
  LocalContext env;
  v8::Isolate* isolate = env->GetIsolate();
  v8::HandleScope scope(isolate);
  isolate->SetCaptureStackTraceForUncaughtExceptions(true, 1024,
                                                     v8::StackTrace::kDetailed);

  CompileRun(
      "var setters = ['column', 'lineNumber', 'scriptName',\n"
      "    'scriptNameOrSourceURL', 'functionName', 'isEval',\n"
      "    'isConstructor'];\n"
      "for (var i = 0; i < setters.length; i++) {\n"
      "  var prop = setters[i];\n"
      "  Object.prototype.__defineSetter__(prop, function() { throw prop; });\n"
      "}\n");
  CompileRun("throw 'exception';");
  isolate->SetCaptureStackTraceForUncaughtExceptions(false);
}

static void StackTraceFunctionNameListener(v8::Local<v8::Message> message,
                                           v8::Local<Value>) {
  v8::Local<v8::StackTrace> stack_trace = message->GetStackTrace();
  v8::Isolate* isolate = message->GetIsolate();
  CHECK_EQ(5, stack_trace->GetFrameCount());
  checkStackFrame("origin", "foo:0", 4, 7, false, false,
                  stack_trace->GetFrame(isolate, 0));
  checkStackFrame("origin", "foo:1", 5, 27, false, false,
                  stack_trace->GetFrame(isolate, 1));
  checkStackFrame("origin", "foo", 5, 27, false, false,
                  stack_trace->GetFrame(isolate, 2));
  checkStackFrame("origin", "foo", 5, 27, false, false,
                  stack_trace->GetFrame(isolate, 3));
  checkStackFrame("origin", "", 1, 14, false, false,
                  stack_trace->GetFrame(isolate, 4));
}

TEST(GetStackTraceContainsFunctionsWithFunctionName) {
  LocalContext env;
  v8::Isolate* isolate = env->GetIsolate();
  v8::HandleScope scope(isolate);

  CompileRunWithOrigin(
      "function gen(name, counter) {\n"
      "  var f = function foo() {\n"
      "    if (counter === 0)\n"
      "      throw 1;\n"
      "    gen(name, counter - 1)();\n"
      "  };\n"
      "  if (counter == 3) {\n"
      "    Object.defineProperty(f, 'name', {get: function(){ throw 239; }});\n"
      "  } else {\n"
      "    Object.defineProperty(f, 'name', {writable:true});\n"
      "    if (counter == 2)\n"
      "      f.name = 42;\n"
      "    else\n"
      "      f.name = name + ':' + counter;\n"
      "  }\n"
      "  return f;\n"
      "};",
      "origin");

  isolate->AddMessageListener(StackTraceFunctionNameListener);
  isolate->SetCaptureStackTraceForUncaughtExceptions(true);
  CompileRunWithOrigin("gen('foo', 3)();", "origin");
  isolate->SetCaptureStackTraceForUncaughtExceptions(false);
  isolate->RemoveMessageListeners(StackTraceFunctionNameListener);
}

static void RethrowStackTraceHandler(v8::Local<v8::Message> message,
                                     v8::Local<v8::Value> data) {
  // Use the frame where JavaScript is called from.
  v8::Local<v8::StackTrace> stack_trace = message->GetStackTrace();
  CHECK(!stack_trace.IsEmpty());
  int frame_count = stack_trace->GetFrameCount();
  CHECK_EQ(3, frame_count);
  int line_number[] = {1, 2, 5};
  for (int i = 0; i < frame_count; i++) {
    CHECK_EQ(line_number[i],
             stack_trace->GetFrame(message->GetIsolate(), i)->GetLineNumber());
  }
}

// Test that we only return the stack trace at the site where the exception
// is first thrown (not where it is rethrown).
TEST(RethrowStackTrace) {
  LocalContext env;
  v8::Isolate* isolate = env->GetIsolate();
  v8::HandleScope scope(isolate);
  // We make sure that
  // - the stack trace of the ReferenceError in g() is reported.
  // - the stack trace is not overwritten when e1 is rethrown by t().
  // - the stack trace of e2 does not overwrite that of e1.
  const char* source =
      "function g() { error; }          \n"
      "function f() { g(); }            \n"
      "function t(e) { throw e; }       \n"
      "try {                            \n"
      "  f();                           \n"
      "} catch (e1) {                   \n"
      "  try {                          \n"
      "    error;                       \n"
      "  } catch (e2) {                 \n"
      "    t(e1);                       \n"
      "  }                              \n"
      "}                                \n";
  isolate->AddMessageListener(RethrowStackTraceHandler);
  isolate->SetCaptureStackTraceForUncaughtExceptions(true);
  CompileRun(source);
  isolate->SetCaptureStackTraceForUncaughtExceptions(false);
  isolate->RemoveMessageListeners(RethrowStackTraceHandler);
}

static void RethrowPrimitiveStackTraceHandler(v8::Local<v8::Message> message,
                                              v8::Local<v8::Value> data) {
  v8::Local<v8::StackTrace> stack_trace = message->GetStackTrace();
  CHECK(!stack_trace.IsEmpty());
  int frame_count = stack_trace->GetFrameCount();
  CHECK_EQ(2, frame_count);
  int line_number[] = {3, 7};
  for (int i = 0; i < frame_count; i++) {
    CHECK_EQ(line_number[i],
             stack_trace->GetFrame(message->GetIsolate(), i)->GetLineNumber());
  }
}

// Test that we do not recognize identity for primitive exceptions.
TEST(RethrowPrimitiveStackTrace) {
  LocalContext env;
  v8::Isolate* isolate = env->GetIsolate();
  v8::HandleScope scope(isolate);
  // We do not capture stack trace for non Error objects on creation time.
  // Instead, we capture the stack trace on last throw.
  const char* source =
      "function g() { throw 404; }      \n"
      "function f() { g(); }            \n"
      "function t(e) { throw e; }       \n"
      "try {                            \n"
      "  f();                           \n"
      "} catch (e1) {                   \n"
      "  t(e1)                          \n"
      "}                                \n";
  isolate->AddMessageListener(RethrowPrimitiveStackTraceHandler);
  isolate->SetCaptureStackTraceForUncaughtExceptions(true);
  CompileRun(source);
  isolate->SetCaptureStackTraceForUncaughtExceptions(false);
  isolate->RemoveMessageListeners(RethrowPrimitiveStackTraceHandler);
}

static void RethrowExistingStackTraceHandler(v8::Local<v8::Message> message,
                                             v8::Local<v8::Value> data) {
  // Use the frame where JavaScript is called from.
  v8::Local<v8::StackTrace> stack_trace = message->GetStackTrace();
  CHECK(!stack_trace.IsEmpty());
  CHECK_EQ(1, stack_trace->GetFrameCount());
  CHECK_EQ(1, stack_trace->GetFrame(message->GetIsolate(), 0)->GetLineNumber());
}

// Test that the stack trace is captured when the error object is created and
// not where it is thrown.
TEST(RethrowExistingStackTrace) {
  LocalContext env;
  v8::Isolate* isolate = env->GetIsolate();
  v8::HandleScope scope(isolate);
  const char* source =
      "var e = new Error();           \n"
      "throw e;                       \n";
  isolate->AddMessageListener(RethrowExistingStackTraceHandler);
  isolate->SetCaptureStackTraceForUncaughtExceptions(true);
  CompileRun(source);
  isolate->SetCaptureStackTraceForUncaughtExceptions(false);
  isolate->RemoveMessageListeners(RethrowExistingStackTraceHandler);
}

static void RethrowBogusErrorStackTraceHandler(v8::Local<v8::Message> message,
                                               v8::Local<v8::Value> data) {
  // Use the frame where JavaScript is called from.
  v8::Local<v8::StackTrace> stack_trace = message->GetStackTrace();
  CHECK(!stack_trace.IsEmpty());
  CHECK_EQ(1, stack_trace->GetFrameCount());
  CHECK_EQ(2, stack_trace->GetFrame(message->GetIsolate(), 0)->GetLineNumber());
}

// Test that the stack trace is captured where the bogus Error object is thrown.
TEST(RethrowBogusErrorStackTrace) {
  LocalContext env;
  v8::Isolate* isolate = env->GetIsolate();
  v8::HandleScope scope(isolate);
  const char* source =
      "var e = {__proto__: new Error()} \n"
      "throw e;                         \n";
  isolate->AddMessageListener(RethrowBogusErrorStackTraceHandler);
  isolate->SetCaptureStackTraceForUncaughtExceptions(true);
  CompileRun(source);
  isolate->SetCaptureStackTraceForUncaughtExceptions(false);
  isolate->RemoveMessageListeners(RethrowBogusErrorStackTraceHandler);
}

void AnalyzeStackOfEvalWithSourceURL(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  v8::HandleScope scope(args.GetIsolate());
  v8::Local<v8::StackTrace> stackTrace = v8::StackTrace::CurrentStackTrace(
      args.GetIsolate(), 10, v8::StackTrace::kDetailed);
  CHECK_EQ(5, stackTrace->GetFrameCount());
  v8::Local<v8::String> url = v8_str("eval_url");
  for (int i = 0; i < 3; i++) {
    v8::Local<v8::String> name =
        stackTrace->GetFrame(args.GetIsolate(), i)->GetScriptNameOrSourceURL();
    CHECK(!name.IsEmpty());
    CHECK(url->Equals(args.GetIsolate()->GetCurrentContext(), name).FromJust());
  }
}

TEST(SourceURLInStackTrace) {
  v8::Isolate* isolate = CcTest::isolate();
  v8::HandleScope scope(isolate);
  Local<ObjectTemplate> templ = ObjectTemplate::New(isolate);
  templ->Set(
      v8_str("AnalyzeStackOfEvalWithSourceURL"),
      v8::FunctionTemplate::New(isolate, AnalyzeStackOfEvalWithSourceURL));
  LocalContext context(nullptr, templ);

  const char* source =
      "function outer() {\n"
      "function bar() {\n"
      "  AnalyzeStackOfEvalWithSourceURL();\n"
      "}\n"
      "function foo() {\n"
      "\n"
      "  bar();\n"
      "}\n"
      "foo();\n"
      "}\n"
      "eval('(' + outer +')()%s');";

  i::ScopedVector<char> code(1024);
  i::SNPrintF(code, source, "//# sourceURL=eval_url");
  CHECK(CompileRun(code.begin())->IsUndefined());
  i::SNPrintF(code, source, "//@ sourceURL=eval_url");
  CHECK(CompileRun(code.begin())->IsUndefined());
}

static int scriptIdInStack[2];

void AnalyzeScriptIdInStack(const v8::FunctionCallbackInfo<v8::Value>& args) {
  v8::HandleScope scope(args.GetIsolate());
  v8::Local<v8::StackTrace> stackTrace = v8::StackTrace::CurrentStackTrace(
      args.GetIsolate(), 10, v8::StackTrace::kScriptId);
  CHECK_EQ(2, stackTrace->GetFrameCount());
  for (int i = 0; i < 2; i++) {
    scriptIdInStack[i] =
        stackTrace->GetFrame(args.GetIsolate(), i)->GetScriptId();
  }
}

TEST(ScriptIdInStackTrace) {
  v8::Isolate* isolate = CcTest::isolate();
  v8::HandleScope scope(isolate);
  Local<ObjectTemplate> templ = ObjectTemplate::New(isolate);
  templ->Set(v8_str("AnalyzeScriptIdInStack"),
             v8::FunctionTemplate::New(isolate, AnalyzeScriptIdInStack));
  LocalContext context(nullptr, templ);

  v8::Local<v8::String> scriptSource = v8_str(
      "function foo() {\n"
      "  AnalyzeScriptIdInStack();"
      "}\n"
      "foo();\n");
  v8::Local<v8::Script> script = CompileWithOrigin(scriptSource, "test", false);
  script->Run(context.local()).ToLocalChecked();
  for (int i = 0; i < 2; i++) {
    CHECK_NE(scriptIdInStack[i], v8::Message::kNoScriptIdInfo);
    CHECK_EQ(scriptIdInStack[i], script->GetUnboundScript()->GetId());
  }
}

void AnalyzeStackOfInlineScriptWithSourceURL(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  v8::HandleScope scope(args.GetIsolate());
  v8::Local<v8::StackTrace> stackTrace = v8::StackTrace::CurrentStackTrace(
      args.GetIsolate(), 10, v8::StackTrace::kDetailed);
  CHECK_EQ(4, stackTrace->GetFrameCount());
  v8::Local<v8::String> url = v8_str("source_url");
  for (int i = 0; i < 3; i++) {
    v8::Local<v8::String> name =
        stackTrace->GetFrame(args.GetIsolate(), i)->GetScriptNameOrSourceURL();
    CHECK(!name.IsEmpty());
    CHECK(url->Equals(args.GetIsolate()->GetCurrentContext(), name).FromJust());
  }
}

TEST(InlineScriptWithSourceURLInStackTrace) {
  v8::Isolate* isolate = CcTest::isolate();
  v8::HandleScope scope(isolate);
  Local<ObjectTemplate> templ = ObjectTemplate::New(isolate);
  templ->Set(v8_str("AnalyzeStackOfInlineScriptWithSourceURL"),
             v8::FunctionTemplate::New(
                 CcTest::isolate(), AnalyzeStackOfInlineScriptWithSourceURL));
  LocalContext context(nullptr, templ);

  const char* source =
      "function outer() {\n"
      "function bar() {\n"
      "  AnalyzeStackOfInlineScriptWithSourceURL();\n"
      "}\n"
      "function foo() {\n"
      "\n"
      "  bar();\n"
      "}\n"
      "foo();\n"
      "}\n"
      "outer()\n%s";

  i::ScopedVector<char> code(1024);
  i::SNPrintF(code, source, "//# sourceURL=source_url");
  CHECK(CompileRunWithOrigin(code.begin(), "url", 0, 1)->IsUndefined());
  i::SNPrintF(code, source, "//@ sourceURL=source_url");
  CHECK(CompileRunWithOrigin(code.begin(), "url", 0, 1)->IsUndefined());
}

void AnalyzeStackOfDynamicScriptWithSourceURL(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  v8::HandleScope scope(args.GetIsolate());
  v8::Local<v8::StackTrace> stackTrace = v8::StackTrace::CurrentStackTrace(
      args.GetIsolate(), 10, v8::StackTrace::kDetailed);
  CHECK_EQ(4, stackTrace->GetFrameCount());
  v8::Local<v8::String> url = v8_str("source_url");
  for (int i = 0; i < 3; i++) {
    v8::Local<v8::String> name =
        stackTrace->GetFrame(args.GetIsolate(), i)->GetScriptNameOrSourceURL();
    CHECK(!name.IsEmpty());
    CHECK(url->Equals(args.GetIsolate()->GetCurrentContext(), name).FromJust());
  }
}

TEST(DynamicWithSourceURLInStackTrace) {
  v8::Isolate* isolate = CcTest::isolate();
  v8::HandleScope scope(isolate);
  Local<ObjectTemplate> templ = ObjectTemplate::New(isolate);
  templ->Set(v8_str("AnalyzeStackOfDynamicScriptWithSourceURL"),
             v8::FunctionTemplate::New(
                 CcTest::isolate(), AnalyzeStackOfDynamicScriptWithSourceURL));
  LocalContext context(nullptr, templ);

  const char* source =
      "function outer() {\n"
      "function bar() {\n"
      "  AnalyzeStackOfDynamicScriptWithSourceURL();\n"
      "}\n"
      "function foo() {\n"
      "\n"
      "  bar();\n"
      "}\n"
      "foo();\n"
      "}\n"
      "outer()\n%s";

  i::ScopedVector<char> code(1024);
  i::SNPrintF(code, source, "//# sourceURL=source_url");
  CHECK(CompileRunWithOrigin(code.begin(), "url", 0, 0)->IsUndefined());
  i::SNPrintF(code, source, "//@ sourceURL=source_url");
  CHECK(CompileRunWithOrigin(code.begin(), "url", 0, 0)->IsUndefined());
}

TEST(DynamicWithSourceURLInStackTraceString) {
  LocalContext context;
  v8::HandleScope scope(context->GetIsolate());

  const char* source =
      "function outer() {\n"
      "  function foo() {\n"
      "    FAIL.FAIL;\n"
      "  }\n"
      "  foo();\n"
      "}\n"
      "outer()\n%s";

  i::ScopedVector<char> code(1024);
  i::SNPrintF(code, source, "//# sourceURL=source_url");
  v8::TryCatch try_catch(context->GetIsolate());
  CompileRunWithOrigin(code.begin(), "", 0, 0);
  CHECK(try_catch.HasCaught());
  v8::String::Utf8Value stack(
      context->GetIsolate(),
      try_catch.StackTrace(context.local()).ToLocalChecked());
  CHECK_NOT_NULL(strstr(*stack, "at foo (source_url:3:5)"));
}

TEST(CaptureStackTraceForStackOverflow) {
  v8::internal::FLAG_stack_size = 150;
  LocalContext current;
  v8::Isolate* isolate = current->GetIsolate();
  v8::HandleScope scope(isolate);
  isolate->SetCaptureStackTraceForUncaughtExceptions(true, 10,
                                                     v8::StackTrace::kDetailed);
  v8::TryCatch try_catch(isolate);
  CompileRun("(function f(x) { f(x+1); })(0)");
  CHECK(try_catch.HasCaught());
}
