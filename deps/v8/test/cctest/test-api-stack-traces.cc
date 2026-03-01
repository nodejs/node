// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "include/v8-function.h"
#include "src/api/api-inl.h"
#include "src/base/strings.h"
#include "test/cctest/test-api.h"

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
  return v8::Number::New(CcTest::isolate(), 42);
}

static v8::MaybeLocal<Value> PrepareStackTraceThrow(v8::Local<Context> context,
                                                    v8::Local<Value> error,
                                                    v8::Local<Array> trace) {
  v8::Isolate* isolate = CcTest::isolate();
  v8::Local<String> message = v8_str("42");
  isolate->ThrowException(v8::Exception::Error(message));
  return v8::MaybeLocal<Value>();
}

THREADED_TEST(IsolatePrepareStackTrace) {
  LocalContext context;
  v8::Isolate* isolate = context.isolate();
  v8::HandleScope scope(isolate);

  isolate->SetPrepareStackTraceCallback(PrepareStackTrace42);

  v8::Local<Value> v = CompileRun("new Error().stack");

  CHECK(v->IsNumber());
  CHECK_EQ(v.As<v8::Number>()->Int32Value(context.local()).FromJust(), 42);
}

THREADED_TEST(IsolatePrepareStackTraceThrow) {
  LocalContext context;
  v8::Isolate* isolate = context.isolate();
  v8::HandleScope scope(isolate);

  isolate->SetPrepareStackTraceCallback(PrepareStackTraceThrow);

  v8::Local<Value> v = CompileRun("try { new Error().stack } catch (e) { e }");

  CHECK(v->IsNativeError());

  v8::Local<String> message = v8::Exception::CreateMessage(isolate, v)->Get();

  CHECK(message->StrictEquals(v8_str("Uncaught Error: 42")));
}

static void ThrowV8Exception(const v8::FunctionCallbackInfo<v8::Value>& info) {
  CHECK(i::ValidateCallbackInfo(info));
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
  v8::HandleScope scope(context.isolate());
  v8::Local<String> foo_str = v8_str("foo");
  v8::Local<String> message_str = v8_str("message");

  context.isolate()->SetCaptureStackTraceForUncaughtExceptions(true);

  Local<v8::FunctionTemplate> fun =
      v8::FunctionTemplate::New(context.isolate(), ThrowV8Exception);
  v8::Local<v8::Object> global = context->Global();
  CHECK(global
            ->Set(context.local(), v8_str("throwV8Exception"),
                  fun->GetFunction(context.local()).ToLocalChecked())
            .FromJust());

  TryCatch try_catch(context.isolate());
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
      v8::Exception::CreateMessage(context.isolate(), error);
  CHECK(!message.IsEmpty());
  CHECK_EQ(2, message->GetLineNumber(context.local()).FromJust());
  CHECK_EQ(2, message->GetStartColumn(context.local()).FromJust());

  v8::Local<v8::StackTrace> stackTrace = message->GetStackTrace();
  CHECK(!stackTrace.IsEmpty());
  CHECK_EQ(2, stackTrace->GetFrameCount());

  stackTrace = v8::Exception::GetStackTrace(error);
  CHECK(!stackTrace.IsEmpty());
  CHECK_EQ(2, stackTrace->GetFrameCount());

  context.isolate()->SetCaptureStackTraceForUncaughtExceptions(false);

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

  message = v8::Exception::CreateMessage(context.isolate(), error);
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
  v8::Isolate* isolate = context.isolate();
  v8::HandleScope scope(isolate);
  v8::TryCatch try_catch(isolate);
  const char* source = "function foo() { FAIL.FAIL; }; foo();";
  v8::Local<v8::String> src = v8_str(source);
  v8::Local<v8::String> origin = v8_str("stack-trace-test");
  v8::ScriptCompiler::Source script_source(src, v8::ScriptOrigin(origin));
  CHECK(v8::ScriptCompiler::CompileUnboundScript(context.isolate(),
                                                 &script_source)
            .ToLocalChecked()
            ->BindToCurrentContext()
            ->Run(context.local())
            .IsEmpty());
  CHECK(try_catch.HasCaught());
  v8::String::Utf8Value stack(
      context.isolate(),
      try_catch.StackTrace(context.local()).ToLocalChecked());
  CHECK_NOT_NULL(strstr(*stack, "at foo (stack-trace-test"));
}

// Checks that a StackFrame has certain expected values.
static void checkStackFrame(const char* expected_script_name,
                            const char* expected_script_source,
                            const char* expected_script_source_mapping_url,
                            const char* expected_func_name,
                            int expected_line_number, int expected_column,
                            int expected_source_position, bool is_eval,
                            bool is_constructor,
                            v8::Local<v8::StackFrame> frame) {
  v8::HandleScope scope(CcTest::isolate());
  v8::String::Utf8Value func_name(CcTest::isolate(), frame->GetFunctionName());
  v8::String::Utf8Value script_name(CcTest::isolate(), frame->GetScriptName());
  v8::String::Utf8Value script_source(CcTest::isolate(),
                                      frame->GetScriptSource());
  v8::String::Utf8Value script_source_mapping_url(
      CcTest::isolate(), frame->GetScriptSourceMappingURL());
  if (*script_name == nullptr) {
    // The situation where there is no associated script, like for evals.
    CHECK_NULL(expected_script_name);
  } else {
    CHECK_NOT_NULL(strstr(*script_name, expected_script_name));
  }
  CHECK_NOT_NULL(strstr(*script_source, expected_script_source));
  if (*script_source_mapping_url == nullptr) {
    CHECK_NULL(expected_script_source_mapping_url);
  } else {
    CHECK_NOT_NULL(expected_script_source_mapping_url);
    CHECK_NOT_NULL(
        strstr(*script_source_mapping_url, expected_script_source_mapping_url));
  }
  if (!frame->GetFunctionName().IsEmpty()) {
    CHECK_NOT_NULL(strstr(*func_name, expected_func_name));
  }
  CHECK_EQ(expected_line_number, frame->GetLineNumber());
  CHECK_EQ(expected_column, frame->GetColumn());
  CHECK_EQ(is_eval, frame->IsEval());
  CHECK_EQ(is_constructor, frame->IsConstructor());
  CHECK_EQ(expected_source_position, frame->GetSourcePosition());
  CHECK(frame->IsUserJavaScript());
}

// Tests the C++ StackTrace API.

// Test getting OVERVIEW information. Should ignore information that is not
// script name, function name, line number, and column offset.
const char* overview_source_eval = "new foo();";
const char* overview_source =
    "function bar() {\n"
    "  var y; AnalyzeStackInNativeCode(1);\n"
    "}\n"
    "function foo() {\n"
    "\n"
    "  bar();\n"
    "}\n"
    "//# sourceMappingURL=http://foobar.com/overview.ts\n"
    "var x;eval('new foo();');";

// Test getting DETAILED information.
const char* detailed_source =
    "function bat() {AnalyzeStackInNativeCode(2);\n"
    "}\n"
    "\n"
    "function baz() {\n"
    "  bat();\n"
    "}\n"
    "eval('new baz();');";

// Test using function.name and function.displayName in stack trace
const char function_name_source[] =
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
    "bar('function.name', 'function.displayName', 4);\n"
    "bar(239, undefined, 5);\n";

// Maybe it's a bit pathological to depend on the exact format of the wrapper
// the Function constructor puts around it's input string. If this becomes a
// hassle, maybe come up with some regex matching approach?
const char function_name_source_anon3[] =
    "(function anonymous(\n"
    ") {\n"
    "AnalyzeStackInNativeCode(3);\n"
    "})";
const char function_name_source_anon4[] =
    "(function anonymous(\n"
    ") {\n"
    "AnalyzeStackInNativeCode(4);\n"
    "})";
const char function_name_source_anon5[] =
    "(function anonymous(\n"
    ") {\n"
    "AnalyzeStackInNativeCode(5);\n"
    "})";

static void AnalyzeStackInNativeCode(
    const v8::FunctionCallbackInfo<v8::Value>& info) {
  CHECK(i::ValidateCallbackInfo(info));
  v8::HandleScope scope(info.GetIsolate());
  const char* origin = "capture-stack-trace-test";
  const int kOverviewTest = 1;
  const int kDetailedTest = 2;
  const int kFunctionName = 3;
  const int kFunctionNameAndDisplayName = 4;
  const int kFunctionNameIsNotString = 5;

  CHECK_EQ(info.Length(), 1);

  v8::Local<v8::Context> context = info.GetIsolate()->GetCurrentContext();
  v8::Isolate* isolate = info.GetIsolate();
  int testGroup = info[0]->Int32Value(context).FromJust();
  if (testGroup == kOverviewTest) {
    v8::Local<v8::StackTrace> stackTrace = v8::StackTrace::CurrentStackTrace(
        info.GetIsolate(), 10, v8::StackTrace::kOverview);
    CHECK_EQ(4, stackTrace->GetFrameCount());
    checkStackFrame(origin, overview_source, "//foobar.com/overview.ts", "bar",
                    2, 10, 26, false, false,
                    stackTrace->GetFrame(info.GetIsolate(), 0));
    checkStackFrame(origin, overview_source, "//foobar.com/overview.ts", "foo",
                    6, 3, 77, false, true, stackTrace->GetFrame(isolate, 1));
    // This is the source string inside the eval which has the call to foo.
    checkStackFrame(nullptr, "new foo();", nullptr, "", 1, 1, 0, true, false,
                    stackTrace->GetFrame(isolate, 2));
    // The last frame is an anonymous function which has the initial eval call.
    checkStackFrame(origin, overview_source, "//foobar.com/overview.ts", "", 9,
                    7, 143, false, false, stackTrace->GetFrame(isolate, 3));
  } else if (testGroup == kDetailedTest) {
    v8::Local<v8::StackTrace> stackTrace = v8::StackTrace::CurrentStackTrace(
        info.GetIsolate(), 10, v8::StackTrace::kDetailed);
    CHECK_EQ(4, stackTrace->GetFrameCount());
    checkStackFrame(origin, detailed_source, nullptr, "bat", 4, 22, 16, false,
                    false, stackTrace->GetFrame(isolate, 0));
    checkStackFrame(origin, detailed_source, nullptr, "baz", 8, 3, 67, false,
                    true, stackTrace->GetFrame(isolate, 1));
    bool is_eval = true;
    // This is the source string inside the eval which has the call to baz.
    checkStackFrame(nullptr, "new baz();", nullptr, "", 1, 1, 0, is_eval, false,
                    stackTrace->GetFrame(isolate, 2));
    // The last frame is an anonymous function which has the initial eval call.
    checkStackFrame(origin, detailed_source, nullptr, "", 10, 1, 76, false,
                    false, stackTrace->GetFrame(isolate, 3));
  } else if (testGroup == kFunctionName) {
    v8::Local<v8::StackTrace> stackTrace = v8::StackTrace::CurrentStackTrace(
        info.GetIsolate(), 5, v8::StackTrace::kOverview);
    CHECK_EQ(3, stackTrace->GetFrameCount());
    checkStackFrame(nullptr, function_name_source_anon3, nullptr,
                    "function.name", 3, 1, 25, true, false,
                    stackTrace->GetFrame(isolate, 0));
  } else if (testGroup == kFunctionNameAndDisplayName) {
    v8::Local<v8::StackTrace> stackTrace = v8::StackTrace::CurrentStackTrace(
        info.GetIsolate(), 5, v8::StackTrace::kOverview);
    CHECK_EQ(3, stackTrace->GetFrameCount());
    checkStackFrame(nullptr, function_name_source_anon4, nullptr,
                    "function.name", 3, 1, 25, true, false,
                    stackTrace->GetFrame(isolate, 0));
  } else if (testGroup == kFunctionNameIsNotString) {
    v8::Local<v8::StackTrace> stackTrace = v8::StackTrace::CurrentStackTrace(
        info.GetIsolate(), 5, v8::StackTrace::kOverview);
    CHECK_EQ(3, stackTrace->GetFrameCount());
    checkStackFrame(nullptr, function_name_source_anon5, nullptr, "", 3, 1, 25,
                    true, false, stackTrace->GetFrame(isolate, 0));
  }
}

THREADED_TEST(CaptureStackTrace) {
  v8::Isolate* isolate = CcTest::isolate();
  v8::HandleScope scope(isolate);
  v8::Local<v8::String> origin = v8_str("capture-stack-trace-test");
  Local<ObjectTemplate> templ = ObjectTemplate::New(isolate);
  templ->Set(isolate, "AnalyzeStackInNativeCode",
             v8::FunctionTemplate::New(isolate, AnalyzeStackInNativeCode));
  LocalContext context(nullptr, templ);

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

  v8::Local<v8::String> detailed_src = v8_str(detailed_source);
  // Make the script using a non-zero line and column offset.
  v8::ScriptOrigin detailed_origin(origin, 3, 5);
  v8::ScriptCompiler::Source script_source2(detailed_src, detailed_origin);
  v8::Local<v8::UnboundScript> detailed_script(
      v8::ScriptCompiler::CompileUnboundScript(isolate, &script_source2)
          .ToLocalChecked());
  v8::Local<Value> detailed_result(detailed_script->BindToCurrentContext()
                                       ->Run(context.local())
                                       .ToLocalChecked());
  CHECK(!detailed_result.IsEmpty());
  CHECK(detailed_result->IsObject());

  v8::Local<v8::String> function_name_src =
      v8::String::NewFromUtf8Literal(isolate, function_name_source);
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

// Test uncaught exception
const char uncaught_exception_source[] =
    "function foo() {\n"
    "  throw 1;\n"
    "};\n"
    "function bar() {\n"
    "  foo();\n"
    "};";

static void StackTraceForUncaughtExceptionListener(
    v8::Local<v8::Message> message, v8::Local<Value>) {
  report_count++;
  v8::Local<v8::StackTrace> stack_trace = message->GetStackTrace();
  CHECK_EQ(2, stack_trace->GetFrameCount());
  checkStackFrame("origin", uncaught_exception_source, nullptr, "foo", 2, 3, 19,
                  false, false, stack_trace->GetFrame(CcTest::isolate(), 0));
  checkStackFrame("origin", uncaught_exception_source, nullptr, "bar", 5, 3, 50,
                  false, false, stack_trace->GetFrame(CcTest::isolate(), 1));
}

TEST(CaptureStackTraceForUncaughtException) {
  LocalContext env;
  v8::Isolate* isolate = env.isolate();
  v8::HandleScope scope(isolate);
  isolate->AddMessageListener(StackTraceForUncaughtExceptionListener);
  isolate->SetCaptureStackTraceForUncaughtExceptions(true);

  CompileRunWithOrigin(uncaught_exception_source, "origin");
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

// Test uncaught exception in a setter
const char uncaught_setter_exception_source[] =
    "var setters = ['column', 'lineNumber', 'scriptName',\n"
    "    'scriptNameOrSourceURL', 'functionName', 'isEval',\n"
    "    'isConstructor'];\n"
    "for (let i = 0; i < setters.length; i++) {\n"
    "  let prop = setters[i];\n"
    "  Object.prototype.__defineSetter__(prop, function() { throw prop; });\n"
    "}\n";

static void StackTraceForUncaughtExceptionAndSettersListener(
    v8::Local<v8::Message> message, v8::Local<Value> value) {
  CHECK(value->IsObject());
  v8::Isolate* isolate = CcTest::isolate();
  v8::Local<v8::Context> context = isolate->GetCurrentContext();
  report_count++;
  v8::Local<v8::StackTrace> stack_trace = message->GetStackTrace();
  CHECK_EQ(1, stack_trace->GetFrameCount());
  checkStackFrame(nullptr, "throw 'exception';", nullptr, nullptr, 1, 1, 0,
                  false, false, stack_trace->GetFrame(isolate, 0));
  v8::Local<v8::StackFrame> stack_frame = stack_trace->GetFrame(isolate, 0);
  v8::Local<v8::Object> object = v8::Local<v8::Object>::Cast(value);
  CHECK(object
            ->Set(context,
                  v8::String::NewFromUtf8Literal(isolate, "lineNumber"),
                  v8::Integer::New(isolate, stack_frame->GetLineNumber()))
            .IsNothing());
}

TEST(CaptureStackTraceForUncaughtExceptionAndSetters) {
  report_count = 0;
  LocalContext env;
  v8::Isolate* isolate = env.isolate();
  v8::HandleScope scope(isolate);
  v8::Local<v8::Object> object = v8::Object::New(isolate);
  isolate->AddMessageListener(StackTraceForUncaughtExceptionAndSettersListener,
                              object);
  isolate->SetCaptureStackTraceForUncaughtExceptions(true, 1024,
                                                     v8::StackTrace::kDetailed);

  CompileRun(uncaught_setter_exception_source);
  CompileRun("throw 'exception';");
  isolate->SetCaptureStackTraceForUncaughtExceptions(false);
  isolate->RemoveMessageListeners(
      StackTraceForUncaughtExceptionAndSettersListener);
  CHECK(object
            ->Get(isolate->GetCurrentContext(),
                  v8::String::NewFromUtf8Literal(isolate, "lineNumber"))
            .ToLocalChecked()
            ->IsUndefined());
  CHECK_EQ(report_count, 1);
}

const char functions_with_function_name[] =
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
    "};"
    "//# sourceMappingURL=local/functional.sc";

const char functions_with_function_name_caller[] = "gen('foo', 3)();";

static void StackTraceFunctionNameListener(v8::Local<v8::Message> message,
                                           v8::Local<Value>) {
  v8::Local<v8::StackTrace> stack_trace = message->GetStackTrace();
  v8::Isolate* isolate = CcTest::isolate();
  CHECK_EQ(5, stack_trace->GetFrameCount());
  checkStackFrame("origin", functions_with_function_name, "local/functional.sc",
                  "foo:0", 4, 7, 86, false, false,
                  stack_trace->GetFrame(isolate, 0));
  checkStackFrame("origin", functions_with_function_name, "local/functional.sc",
                  "foo:1", 5, 27, 121, false, false,
                  stack_trace->GetFrame(isolate, 1));
  checkStackFrame("origin", functions_with_function_name, "local/functional.sc",
                  "foo", 5, 27, 121, false, false,
                  stack_trace->GetFrame(isolate, 2));
  checkStackFrame("origin", functions_with_function_name, "local/functional.sc",
                  "foo", 5, 27, 121, false, false,
                  stack_trace->GetFrame(isolate, 3));
  checkStackFrame("origin", functions_with_function_name_caller, nullptr, "", 1,
                  14, 13, false, false, stack_trace->GetFrame(isolate, 4));
}

TEST(GetStackTraceContainsFunctionsWithFunctionName) {
  LocalContext env;
  v8::Isolate* isolate = env.isolate();
  v8::HandleScope scope(isolate);

  CompileRunWithOrigin(functions_with_function_name, "origin");

  isolate->AddMessageListener(StackTraceFunctionNameListener);
  isolate->SetCaptureStackTraceForUncaughtExceptions(true);
  CompileRunWithOrigin(functions_with_function_name_caller, "origin");
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
             stack_trace->GetFrame(CcTest::isolate(), i)->GetLineNumber());
  }
}

// Test that we only return the stack trace at the site where the exception
// is first thrown (not where it is rethrown).
TEST(RethrowStackTrace) {
  LocalContext env;
  v8::Isolate* isolate = env.isolate();
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
             stack_trace->GetFrame(CcTest::isolate(), i)->GetLineNumber());
  }
}

// Test that we do not recognize identity for primitive exceptions.
TEST(RethrowPrimitiveStackTrace) {
  LocalContext env;
  v8::Isolate* isolate = env.isolate();
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
  CHECK_EQ(1, stack_trace->GetFrame(CcTest::isolate(), 0)->GetLineNumber());
}

// Test that the stack trace is captured when the error object is created and
// not where it is thrown.
TEST(RethrowExistingStackTrace) {
  LocalContext env;
  v8::Isolate* isolate = env.isolate();
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
  CHECK_EQ(1, stack_trace->GetFrame(CcTest::isolate(), 0)->GetLineNumber());
}

// Test that the stack trace is captured where the bogus Error object is created
// and not where it is thrown.
TEST(RethrowBogusErrorStackTrace) {
  LocalContext env;
  v8::Isolate* isolate = env.isolate();
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
    const v8::FunctionCallbackInfo<v8::Value>& info) {
  CHECK(i::ValidateCallbackInfo(info));
  v8::HandleScope scope(info.GetIsolate());
  v8::Local<v8::StackTrace> stackTrace = v8::StackTrace::CurrentStackTrace(
      info.GetIsolate(), 10, v8::StackTrace::kDetailed);
  CHECK_EQ(5, stackTrace->GetFrameCount());
  v8::Local<v8::String> url = v8_str("eval_url");
  for (int i = 0; i < 3; i++) {
    v8::Local<v8::String> name =
        stackTrace->GetFrame(info.GetIsolate(), i)->GetScriptNameOrSourceURL();
    CHECK(!name.IsEmpty());
    CHECK(url->Equals(info.GetIsolate()->GetCurrentContext(), name).FromJust());
  }
}

TEST(SourceURLInStackTrace) {
  v8::Isolate* isolate = CcTest::isolate();
  v8::HandleScope scope(isolate);
  Local<ObjectTemplate> templ = ObjectTemplate::New(isolate);
  templ->Set(
      isolate, "AnalyzeStackOfEvalWithSourceURL",
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

  auto code = v8::base::OwnedVector<char>::NewForOverwrite(1024);
  v8::base::SNPrintF(code.as_vector(), source, "//# sourceURL=eval_url");
  CHECK(CompileRun(code.begin())->IsUndefined());
  v8::base::SNPrintF(code.as_vector(), source, "//@ sourceURL=eval_url");
  CHECK(CompileRun(code.begin())->IsUndefined());
}

static int scriptIdInStack[2];

void AnalyzeScriptIdInStack(const v8::FunctionCallbackInfo<v8::Value>& info) {
  CHECK(i::ValidateCallbackInfo(info));
  v8::HandleScope scope(info.GetIsolate());
  v8::Local<v8::StackTrace> stackTrace = v8::StackTrace::CurrentStackTrace(
      info.GetIsolate(), 10, v8::StackTrace::kScriptId);
  CHECK_EQ(2, stackTrace->GetFrameCount());
  for (int i = 0; i < 2; i++) {
    scriptIdInStack[i] =
        stackTrace->GetFrame(info.GetIsolate(), i)->GetScriptId();
  }
}

TEST(ScriptIdInStackTrace) {
  v8::Isolate* isolate = CcTest::isolate();
  v8::HandleScope scope(isolate);
  Local<ObjectTemplate> templ = ObjectTemplate::New(isolate);
  templ->Set(isolate, "AnalyzeScriptIdInStack",
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
    CHECK_EQ(scriptIdInStack[i], script->ScriptId());
  }
}

static int currentScriptId = -1;

void AnalyzeCurrentScriptIdInStack(
    const v8::FunctionCallbackInfo<v8::Value>& info) {
  CHECK(i::ValidateCallbackInfo(info));
  v8::HandleScope scope(info.GetIsolate());
  currentScriptId = v8::StackTrace::CurrentScriptId(info.GetIsolate());
}

TEST(CurrentScriptId_Id) {
  v8::Isolate* isolate = CcTest::isolate();
  v8::HandleScope scope(isolate);
  Local<ObjectTemplate> templ = ObjectTemplate::New(isolate);
  templ->Set(isolate, "AnalyzeCurrentScriptIdInStack",
             v8::FunctionTemplate::New(CcTest::isolate(),
                                       AnalyzeCurrentScriptIdInStack));
  LocalContext context(nullptr, templ);

  const char* source = R"(
    function foo() {
      AnalyzeCurrentScriptIdInStack();
    }
    foo();
  )";

  v8::Local<v8::Script> script = CompileWithOrigin(source, "test", false);
  script->Run(context.local()).ToLocalChecked();

  CHECK_EQ(currentScriptId, script->ScriptId());

  // When nothing is on the stack, we should get the default response.
  CHECK_EQ(v8::Message::kNoScriptIdInfo,
           v8::StackTrace::CurrentScriptId(CcTest::isolate()));
}

static std::vector<v8::StackTrace::ScriptIdAndContext> expectedFrames;

START_ALLOW_USE_DEPRECATED()
void CaptureStackIdsAndContexts(
    const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  v8::Local<v8::Context> context = isolate->GetCurrentContext();

  int frame_limit = 10;
  // Was a frame limit specified in script?
  if (info.Length() == 1) {
    frame_limit = info[0]->Int32Value(context).FromJust();
  }
  CHECK_LE(frame_limit, 10);

  std::vector<v8::StackTrace::ScriptIdAndContext> results(10);
  auto result_span = v8::StackTrace::CurrentScriptIdsAndContexts(
      isolate, v8::MemorySpan<v8::StackTrace::ScriptIdAndContext>(
                   results.data(), frame_limit));

  CHECK_EQ(result_span.size(), expectedFrames.size());
  for (size_t i = 0; i < result_span.size(); ++i) {
    CHECK_EQ(result_span[i].id, expectedFrames[i].id);
    CHECK_EQ(result_span[i].context, expectedFrames[i].context);
  }
}

// Verify that `CurrentScriptIdsAndContexts` will follow an `eval`to the script
// id that initiated the `eval`.
TEST(CurrentScriptIdsAndContexts_FollowsEval) {
  v8::Isolate* isolate = CcTest::isolate();
  v8::HandleScope scope(isolate);

  Local<ObjectTemplate> templ = ObjectTemplate::New(isolate);
  templ->Set(isolate, "CaptureStack",
             v8::FunctionTemplate::New(isolate, CaptureStackIdsAndContexts));
  LocalContext context(nullptr, templ);

  const char* source = "function test() { eval('CaptureStack()'); } test();";
  v8::Local<v8::Script> script =
      CompileWithOrigin(source, "origin_script", false);

  // 0: The eval function which calls CaptureStack
  // 1: The test() function
  // 2: The top-level function
  int expected_id = script->GetUnboundScript()->ScriptId();
  expectedFrames = {3, {expected_id, context.local()}};
  script->Run(context.local()).ToLocalChecked();
}

// Verify that `CurrentScriptIdsAndContexts` will follow the `new Function`
// constructor back to the script id that initiated the creation.
TEST(CurrentScriptIdsAndContexts_FollowsNewFunction) {
  v8::Isolate* isolate = CcTest::isolate();
  v8::HandleScope scope(isolate);

  Local<ObjectTemplate> templ = ObjectTemplate::New(isolate);
  templ->Set(isolate, "CaptureStack",
             v8::FunctionTemplate::New(isolate, CaptureStackIdsAndContexts));
  LocalContext context(nullptr, templ);

  const char* source =
      "function test() {"
      "  var dynamicFn = new Function('CaptureStack()');"
      "  dynamicFn();"
      "} "
      "test();";

  v8::Local<v8::Script> script =
      CompileWithOrigin(source, "origin_script_new_fn", false);

  // 0: The dynamic function which calls CaptureStack
  // 1: The test() function
  // 2: The top-level function
  int expected_id = script->GetUnboundScript()->ScriptId();
  expectedFrames = {3, {expected_id, context.local()}};
  script->Run(context.local()).ToLocalChecked();
}

// Verify that `CurrentScriptIdsAndContexts` will follow an `eval`to the script
// id that initiated the `eval`, even through nested evals.
TEST(CurrentScriptIdsAndContexts_FollowsNestedEval) {
  v8::Isolate* isolate = CcTest::isolate();
  v8::HandleScope scope(isolate);

  Local<ObjectTemplate> templ = ObjectTemplate::New(isolate);
  templ->Set(isolate, "CaptureStack",
             v8::FunctionTemplate::New(isolate, CaptureStackIdsAndContexts));
  LocalContext context(nullptr, templ);

  const char* source = R"SCRIPT(
    function test() {
      eval("eval(\"CaptureStack();\")");
    }
    test();
  )SCRIPT";
  v8::Local<v8::Script> script =
      CompileWithOrigin(source, "origin_script", false);

  // The stack should have 2 eval frames and a test() frame.
  // All should point back to the original script ID.
  // 0: Outer eval
  // 1: Inner eval
  // 2: Test function
  // 3: Top-level function
  int expected_id = script->GetUnboundScript()->ScriptId();
  expectedFrames = {4, {expected_id, context.local()}};
  script->Run(context.local()).ToLocalChecked();
}

// Verify that stacks with multiple contexts show up appropriately.
TEST(CurrentScriptIdsAndContexts_CrossContextScripting) {
  v8::Isolate* isolate = CcTest::isolate();
  v8::HandleScope scope(isolate);

  Local<ObjectTemplate> templ = ObjectTemplate::New(isolate);
  templ->Set(isolate, "CaptureStack",
             v8::FunctionTemplate::New(isolate, CaptureStackIdsAndContexts));

  LocalContext context_a(nullptr, templ);
  LocalContext context_b(nullptr, templ);

  // Give the contexts a shared security token.
  v8::Local<v8::String> shared_token = v8_str("https://example.com");
  context_a->SetSecurityToken(shared_token);
  context_b->SetSecurityToken(shared_token);

  v8::Local<v8::Function> actor_function;
  int script_a_id = -1;

  // Define a method in the first context, that will later be called from the
  // second.
  {
    v8::Context::Scope context_scope(context_a.local());
    const char* source_a = "function publicMethod() { CaptureStack(); }";
    v8::Local<v8::Script> script_a =
        CompileWithOrigin(source_a, "script_a", false);
    script_a->Run(context_a.local()).ToLocalChecked();

    script_a_id = script_a->GetUnboundScript()->ScriptId();
    actor_function = context_a->Global()
                         ->Get(context_a.local(), v8_str("publicMethod"))
                         .ToLocalChecked()
                         .As<v8::Function>();
  }

  int script_b_id = -1;

  // The second context's script, which calls into the first.
  {
    v8::Context::Scope context_scope(context_b.local());

    // Inject actor into B
    context_b->Global()
        ->Set(context_b.local(), v8_str("publicMethod"), actor_function)
        .FromJust();

    // Wrap the call in a function to ensure a visible stack frame in Context B
    const char* source_b =
        "function foo() { publicMethod.call(globalThis); } foo();";
    v8::Local<v8::Script> script_b =
        CompileWithOrigin(source_b, "script_b", false);

    // frame 0: context a: publicMethod()
    // frame 1: context b: foo()
    // frame 2: context b: top-level
    script_b_id = script_b->GetUnboundScript()->ScriptId();
    expectedFrames = {{script_a_id, context_a.local()},
                      {script_b_id, context_b.local()},
                      {script_b_id, context_b.local()}};
    script_b->Run(context_b.local()).ToLocalChecked();
  }
}

// Ensure that the stack walk honors the provided frame limit.
TEST(CurrentScriptIdsAndContexts_FrameLimit) {
  v8::Isolate* isolate = CcTest::isolate();
  v8::HandleScope scope(isolate);

  Local<ObjectTemplate> templ = ObjectTemplate::New(isolate);
  templ->Set(isolate, "CaptureStack",
             v8::FunctionTemplate::New(isolate, CaptureStackIdsAndContexts));
  LocalContext context(nullptr, templ);

  const char* source = R"SCRIPT(
    function a() { b(); }
    function b() { c(); }
    function c() { d(); }
    function d() { CaptureStack(3); }  // frame limit 3 set here.
    a();
  )SCRIPT";

  v8::Local<v8::Script> script = CompileWithOrigin(source, "limit_test", false);

  // Even though the stack is ~5 frames deep (d, c, b, a, global),
  // we specifically requested a limit of 3.
  int expected_id = script->GetUnboundScript()->ScriptId();
  expectedFrames = {3, {expected_id, context.local()}};
  script->Run(context.local()).ToLocalChecked();
}

TEST(CurrentScriptIdsAndContexts_ZeroLimit) {
  v8::Isolate* isolate = CcTest::isolate();
  v8::HandleScope scope(isolate);
  Local<ObjectTemplate> templ = ObjectTemplate::New(isolate);
  templ->Set(isolate, "CaptureStack",
             v8::FunctionTemplate::New(isolate, CaptureStackIdsAndContexts));

  LocalContext context(nullptr, templ);

  expectedFrames = {};

  // This should not crash and should return a span of size 0
  CompileRun("CaptureStack(0);");
}

TEST(CurrentScriptIdsAndContexts_SkipsBuiltins) {
  v8::Isolate* isolate = CcTest::isolate();
  v8::HandleScope scope(isolate);

  Local<ObjectTemplate> templ = ObjectTemplate::New(isolate);
  templ->Set(isolate, "CaptureStack",
             v8::FunctionTemplate::New(isolate, CaptureStackIdsAndContexts));
  LocalContext context(nullptr, templ);

  const char* source =
      "function userFunc() { CaptureStack(); } [1].forEach(userFunc);";
  v8::Local<v8::Script> script =
      CompileWithOrigin(source, "builtin_test", false);

  // The full stack should look like:
  // [userFunc (JS), forEach (Builtin), top-level (JS)]
  // However, we should skip over the forEach builtin.
  int expected_id = script->GetUnboundScript()->ScriptId();
  expectedFrames = {2, {expected_id, context.local()}};
  script->Run(context.local()).ToLocalChecked();
}

TEST(CurrentScriptIdsAndContexts_BoundFunction) {
  v8::Isolate* isolate = CcTest::isolate();
  v8::HandleScope scope(isolate);

  Local<ObjectTemplate> templ = ObjectTemplate::New(isolate);
  templ->Set(isolate, "CaptureStack",
             v8::FunctionTemplate::New(isolate, CaptureStackIdsAndContexts));
  LocalContext context(nullptr, templ);

  const char* source = R"SCRIPT(
    function target() { CaptureStack(); }
    const bound = target.bind(null);
    function caller() { bound(); }
    caller();
  )SCRIPT";

  v8::Local<v8::Script> script = CompileWithOrigin(source, "bound_test", false);

  // Should see: target, caller, global script.
  int expected_id = script->GetUnboundScript()->ScriptId();
  expectedFrames = {3, {expected_id, context.local()}};
  script->Run(context.local()).ToLocalChecked();
}
END_ALLOW_USE_DEPRECATED()

struct PersistentScriptData {
  int id;
  v8::Global<v8::Function> function;
  v8::Global<v8::Context> context;
};

static std::vector<PersistentScriptData> capturedFramesScriptData;

struct CapturedFramesScope {
  ~CapturedFramesScope() { capturedFramesScriptData.clear(); }
};

void CaptureScriptData(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  v8::Local<v8::Context> context = isolate->GetCurrentContext();

  int frame_limit = 10;
  // Was a frame limit specified in script?
  if (info.Length() == 1) {
    frame_limit = info[0]->Int32Value(context).FromJust();
  }
  CHECK_LE(frame_limit, 10);

  std::vector<v8::StackTrace::ScriptData> results(10);
  auto result_span = v8::StackTrace::CurrentScriptData(
      isolate,
      v8::MemorySpan<v8::StackTrace::ScriptData>(results.data(), frame_limit));

  capturedFramesScriptData.clear();
  for (size_t i = 0; i < result_span.size(); ++i) {
    capturedFramesScriptData.push_back(
        {result_span[i].id,
         v8::Global<v8::Function>(isolate, result_span[i].function),
         v8::Global<v8::Context>(isolate, result_span[i].context)});
  }
}

// Verify that `CurrentScriptData` will follow an `eval`to the script
// id that initiated the `eval`.
TEST(CurrentScriptData_FollowsEval) {
  v8::Isolate* isolate = CcTest::isolate();
  v8::HandleScope handle_scope(isolate);
  CapturedFramesScope scope;

  Local<ObjectTemplate> templ = ObjectTemplate::New(isolate);
  templ->Set(isolate, "CaptureStack",
             v8::FunctionTemplate::New(isolate, CaptureScriptData));
  LocalContext context(nullptr, templ);

  const char* source =
      "function test() { eval('CaptureStack()'); } "
      "function main() { test(); } "
      "main();";
  v8::Local<v8::Script> script =
      CompileWithOrigin(source, "origin_script", false);

  // 0: The eval function which calls CaptureStack
  // 1: The test() function
  // 2: The main() function
  // 3: The top-level function
  int expected_id = script->GetUnboundScript()->ScriptId();
  script->Run(context.local()).ToLocalChecked();

  v8::Local<v8::Function> test_func = context->Global()
                                          ->Get(context.local(), v8_str("test"))
                                          .ToLocalChecked()
                                          .As<v8::Function>();
  v8::Local<v8::Function> main_func = context->Global()
                                          ->Get(context.local(), v8_str("main"))
                                          .ToLocalChecked()
                                          .As<v8::Function>();

  CHECK_EQ(capturedFramesScriptData.size(), 4);
  for (size_t i = 0; i < capturedFramesScriptData.size(); ++i) {
    CHECK_EQ(capturedFramesScriptData[i].id, expected_id);
    CHECK_EQ(capturedFramesScriptData[i].context, context.local());
    CHECK(!capturedFramesScriptData[i].function.IsEmpty());
  }
  CHECK_EQ(capturedFramesScriptData[1].function, test_func);
  CHECK_EQ(capturedFramesScriptData[2].function, main_func);
}

// Verify that `CurrentScriptData` will follow the `new Function`
// constructor back to the script id that initiated the creation.
TEST(CurrentScriptData_FollowsNewFunction) {
  v8::Isolate* isolate = CcTest::isolate();
  v8::HandleScope handle_scope(isolate);
  CapturedFramesScope scope;

  Local<ObjectTemplate> templ = ObjectTemplate::New(isolate);
  templ->Set(isolate, "CaptureStack",
             v8::FunctionTemplate::New(isolate, CaptureScriptData));
  LocalContext context(nullptr, templ);

  const char* source =
      "function test() {"
      "  var dynamicFn = new Function('CaptureStack()');"
      "  dynamicFn();"
      "} "
      "function main() { test(); } "
      "main();";

  v8::Local<v8::Script> script =
      CompileWithOrigin(source, "origin_script_new_fn", false);

  // 0: The dynamic function which calls CaptureStack
  // 1: The test() function
  // 2: The main() function
  // 3: The top-level function
  int expected_id = script->GetUnboundScript()->ScriptId();
  script->Run(context.local()).ToLocalChecked();

  v8::Local<v8::Function> test_func = context->Global()
                                          ->Get(context.local(), v8_str("test"))
                                          .ToLocalChecked()
                                          .As<v8::Function>();
  v8::Local<v8::Function> main_func = context->Global()
                                          ->Get(context.local(), v8_str("main"))
                                          .ToLocalChecked()
                                          .As<v8::Function>();

  CHECK_EQ(capturedFramesScriptData.size(), 4);
  for (size_t i = 0; i < capturedFramesScriptData.size(); ++i) {
    CHECK_EQ(capturedFramesScriptData[i].id, expected_id);
    CHECK_EQ(capturedFramesScriptData[i].context, context.local());
    CHECK(!capturedFramesScriptData[i].function.IsEmpty());
  }
  CHECK_EQ(capturedFramesScriptData[1].function, test_func);
  CHECK_EQ(capturedFramesScriptData[2].function, main_func);
}

// Verify that `CurrentScriptData` will follow an `eval`to the script
// id that initiated the `eval`, even through nested evals.
TEST(CurrentScriptData_FollowsNestedEval) {
  v8::Isolate* isolate = CcTest::isolate();
  v8::HandleScope handle_scope(isolate);
  CapturedFramesScope scope;

  Local<ObjectTemplate> templ = ObjectTemplate::New(isolate);
  templ->Set(isolate, "CaptureStack",
             v8::FunctionTemplate::New(isolate, CaptureScriptData));
  LocalContext context(nullptr, templ);

  const char* source = R"SCRIPT(
    function test() {
      eval("eval(\"CaptureStack();\")");
    }
    function main() { test(); }
    main();
  )SCRIPT";
  v8::Local<v8::Script> script =
      CompileWithOrigin(source, "origin_script", false);

  // The stack should have 2 eval frames and a test() frame.
  // All should point back to the original script ID.
  // 0: Outer eval
  // 1: Inner eval
  // 2: Test function
  // 3: Main function
  // 4: Top-level function
  int expected_id = script->GetUnboundScript()->ScriptId();
  script->Run(context.local()).ToLocalChecked();

  v8::Local<v8::Function> test_func = context->Global()
                                          ->Get(context.local(), v8_str("test"))
                                          .ToLocalChecked()
                                          .As<v8::Function>();
  v8::Local<v8::Function> main_func = context->Global()
                                          ->Get(context.local(), v8_str("main"))
                                          .ToLocalChecked()
                                          .As<v8::Function>();

  CHECK_EQ(capturedFramesScriptData.size(), 5);
  for (size_t i = 0; i < capturedFramesScriptData.size(); ++i) {
    CHECK_EQ(capturedFramesScriptData[i].id, expected_id);
    CHECK_EQ(capturedFramesScriptData[i].context, context.local());
    CHECK(!capturedFramesScriptData[i].function.IsEmpty());
  }
  CHECK_EQ(capturedFramesScriptData[2].function, test_func);
  CHECK_EQ(capturedFramesScriptData[3].function, main_func);
}

// Verify that stacks with multiple contexts show up appropriately.
TEST(CurrentScriptData_CrossContextScripting) {
  v8::Isolate* isolate = CcTest::isolate();
  v8::HandleScope handle_scope(isolate);
  CapturedFramesScope scope;

  Local<ObjectTemplate> templ = ObjectTemplate::New(isolate);
  templ->Set(isolate, "CaptureStack",
             v8::FunctionTemplate::New(isolate, CaptureScriptData));

  LocalContext context_a(nullptr, templ);
  LocalContext context_b(nullptr, templ);

  // Give the contexts a shared security token.
  v8::Local<v8::String> shared_token = v8_str("https://example.com");
  context_a->SetSecurityToken(shared_token);
  context_b->SetSecurityToken(shared_token);

  v8::Local<v8::Function> actor_function;
  int script_a_id = -1;

  // Define a method in the first context, that will later be called from the
  // second.
  {
    v8::Context::Scope context_scope(context_a.local());
    const char* source_a = "function publicMethod() { CaptureStack(); }";
    v8::Local<v8::Script> script_a =
        CompileWithOrigin(source_a, "script_a", false);
    script_a->Run(context_a.local()).ToLocalChecked();

    script_a_id = script_a->GetUnboundScript()->ScriptId();
    actor_function = context_a->Global()
                         ->Get(context_a.local(), v8_str("publicMethod"))
                         .ToLocalChecked()
                         .As<v8::Function>();
  }

  int script_b_id = -1;

  // The second context's script, which calls into the first.
  {
    v8::Context::Scope context_scope(context_b.local());

    // Inject actor into B
    context_b->Global()
        ->Set(context_b.local(), v8_str("publicMethod"), actor_function)
        .FromJust();

    // Wrap the call in a function to ensure a visible stack frame in Context B
    const char* source_b =
        "function foo() { publicMethod.call(globalThis); } foo();";
    v8::Local<v8::Script> script_b =
        CompileWithOrigin(source_b, "script_b", false);

    // frame 0: context a: publicMethod()
    // frame 1: context b: foo()
    // frame 2: context b: top-level
    script_b_id = script_b->GetUnboundScript()->ScriptId();
    script_b->Run(context_b.local()).ToLocalChecked();

    v8::Local<v8::Function> foo_func =
        context_b->Global()
            ->Get(context_b.local(), v8_str("foo"))
            .ToLocalChecked()
            .As<v8::Function>();

    CHECK_EQ(capturedFramesScriptData.size(), 3);
    CHECK_EQ(capturedFramesScriptData[0].id, script_a_id);
    CHECK_EQ(capturedFramesScriptData[0].function, actor_function);
    CHECK_EQ(capturedFramesScriptData[0].context, context_a.local());

    CHECK_EQ(capturedFramesScriptData[1].id, script_b_id);
    CHECK_EQ(capturedFramesScriptData[1].function, foo_func);
    CHECK_EQ(capturedFramesScriptData[1].context, context_b.local());

    CHECK_EQ(capturedFramesScriptData[2].id, script_b_id);
    CHECK(!capturedFramesScriptData[2].function.IsEmpty());
    CHECK_EQ(capturedFramesScriptData[2].context, context_b.local());
  }
}

// Ensure that the stack walk honors the provided frame limit.
TEST(CurrentScriptData_FrameLimit) {
  v8::Isolate* isolate = CcTest::isolate();
  v8::HandleScope handle_scope(isolate);
  CapturedFramesScope scope;

  Local<ObjectTemplate> templ = ObjectTemplate::New(isolate);
  templ->Set(isolate, "CaptureStack",
             v8::FunctionTemplate::New(isolate, CaptureScriptData));
  LocalContext context(nullptr, templ);

  const char* source = R"SCRIPT(
    function a() { b(); }
    function b() { c(); }
    function c() { d(); }
    function d() { CaptureStack(3); }  // frame limit 3 set here.
    a();
  )SCRIPT";

  v8::Local<v8::Script> script = CompileWithOrigin(source, "limit_test", false);

  // Even though the stack is ~5 frames deep (d, c, b, a, global),
  // we specifically requested a limit of 3.
  int expected_id = script->GetUnboundScript()->ScriptId();
  script->Run(context.local()).ToLocalChecked();

  v8::Local<v8::Function> b_func = context->Global()
                                       ->Get(context.local(), v8_str("b"))
                                       .ToLocalChecked()
                                       .As<v8::Function>();
  v8::Local<v8::Function> c_func = context->Global()
                                       ->Get(context.local(), v8_str("c"))
                                       .ToLocalChecked()
                                       .As<v8::Function>();
  v8::Local<v8::Function> d_func = context->Global()
                                       ->Get(context.local(), v8_str("d"))
                                       .ToLocalChecked()
                                       .As<v8::Function>();

  CHECK_EQ(capturedFramesScriptData.size(), 3);
  for (size_t i = 0; i < capturedFramesScriptData.size(); ++i) {
    CHECK_EQ(capturedFramesScriptData[i].id, expected_id);
    CHECK_EQ(capturedFramesScriptData[i].context, context.local());
    CHECK(!capturedFramesScriptData[i].function.IsEmpty());
  }
  CHECK_EQ(capturedFramesScriptData[0].function, d_func);
  CHECK_EQ(capturedFramesScriptData[1].function, c_func);
  CHECK_EQ(capturedFramesScriptData[2].function, b_func);
}

TEST(CurrentScriptData_ZeroLimit) {
  v8::Isolate* isolate = CcTest::isolate();
  v8::HandleScope handle_scope(isolate);
  CapturedFramesScope scope;

  Local<ObjectTemplate> templ = ObjectTemplate::New(isolate);
  templ->Set(isolate, "CaptureStack",
             v8::FunctionTemplate::New(isolate, CaptureScriptData));

  LocalContext context(nullptr, templ);

  // This should not crash and should return a span of size 0
  CompileRun("CaptureStack(0);");

  CHECK_EQ(capturedFramesScriptData.size(), 0);
}

TEST(CurrentScriptData_SkipsBuiltins) {
  v8::Isolate* isolate = CcTest::isolate();
  v8::HandleScope handle_scope(isolate);
  CapturedFramesScope scope;

  Local<ObjectTemplate> templ = ObjectTemplate::New(isolate);
  templ->Set(isolate, "CaptureStack",
             v8::FunctionTemplate::New(isolate, CaptureScriptData));
  LocalContext context(nullptr, templ);

  const char* source =
      "function userFunc() { CaptureStack(); } "
      "function main() { [1].forEach(userFunc); } "
      "main();";
  v8::Local<v8::Script> script =
      CompileWithOrigin(source, "builtin_test", false);

  // The full stack should look like:
  // [userFunc (JS), forEach (Builtin), main (JS), top-level (JS)]
  // However, we should skip over the forEach builtin.
  int expected_id = script->GetUnboundScript()->ScriptId();
  script->Run(context.local()).ToLocalChecked();

  v8::Local<v8::Function> user_func =
      context->Global()
          ->Get(context.local(), v8_str("userFunc"))
          .ToLocalChecked()
          .As<v8::Function>();
  v8::Local<v8::Function> main_func = context->Global()
                                          ->Get(context.local(), v8_str("main"))
                                          .ToLocalChecked()
                                          .As<v8::Function>();

  CHECK_EQ(capturedFramesScriptData.size(), 3);
  for (size_t i = 0; i < capturedFramesScriptData.size(); ++i) {
    CHECK_EQ(capturedFramesScriptData[i].id, expected_id);
    CHECK_EQ(capturedFramesScriptData[i].context, context.local());
    CHECK(!capturedFramesScriptData[i].function.IsEmpty());
  }
  CHECK_EQ(capturedFramesScriptData[0].function, user_func);
  CHECK_EQ(capturedFramesScriptData[1].function, main_func);
}

TEST(CurrentScriptData_BoundFunction) {
  v8::Isolate* isolate = CcTest::isolate();
  v8::HandleScope handle_scope(isolate);
  CapturedFramesScope scope;

  Local<ObjectTemplate> templ = ObjectTemplate::New(isolate);
  templ->Set(isolate, "CaptureStack",
             v8::FunctionTemplate::New(isolate, CaptureScriptData));
  LocalContext context(nullptr, templ);

  const char* source = R"SCRIPT(
    function target() { CaptureStack(); }
    const bound = target.bind(null);
    function caller() { bound(); }
    function main() { caller(); }
    main();
  )SCRIPT";

  v8::Local<v8::Script> script = CompileWithOrigin(source, "bound_test", false);

  // Should see: target, caller, main, global script.
  int expected_id = script->GetUnboundScript()->ScriptId();
  script->Run(context.local()).ToLocalChecked();

  v8::Local<v8::Function> target_func =
      context->Global()
          ->Get(context.local(), v8_str("target"))
          .ToLocalChecked()
          .As<v8::Function>();
  v8::Local<v8::Function> caller_func =
      context->Global()
          ->Get(context.local(), v8_str("caller"))
          .ToLocalChecked()
          .As<v8::Function>();
  v8::Local<v8::Function> main_func = context->Global()
                                          ->Get(context.local(), v8_str("main"))
                                          .ToLocalChecked()
                                          .As<v8::Function>();

  CHECK_EQ(capturedFramesScriptData.size(), 4);
  for (size_t i = 0; i < capturedFramesScriptData.size(); ++i) {
    CHECK_EQ(capturedFramesScriptData[i].id, expected_id);
    CHECK_EQ(capturedFramesScriptData[i].context, context.local());
    CHECK(!capturedFramesScriptData[i].function.IsEmpty());
  }
  CHECK_EQ(capturedFramesScriptData[0].function, target_func);
  CHECK_EQ(capturedFramesScriptData[1].function, caller_func);
  CHECK_EQ(capturedFramesScriptData[2].function, main_func);
}

TEST(CurrentScriptData_RegExp) {
  v8::Isolate* isolate = CcTest::isolate();
  v8::HandleScope handle_scope(isolate);
  CapturedFramesScope scope;

  Local<ObjectTemplate> templ = ObjectTemplate::New(isolate);
  templ->Set(isolate, "CaptureStack",
             v8::FunctionTemplate::New(isolate, CaptureScriptData));
  LocalContext context(nullptr, templ);

  const char* source =
      "function replacer() { CaptureStack(); return 'A'; } "
      "function main() { 'abc'.replace(/a/, replacer); } "
      "main();";
  v8::Local<v8::Script> script =
      CompileWithOrigin(source, "regexp_test", false);

  int expected_id = script->GetUnboundScript()->ScriptId();
  script->Run(context.local()).ToLocalChecked();

  v8::Local<v8::Function> replacer_func =
      context->Global()
          ->Get(context.local(), v8_str("replacer"))
          .ToLocalChecked()
          .As<v8::Function>();
  v8::Local<v8::Function> main_func = context->Global()
                                          ->Get(context.local(), v8_str("main"))
                                          .ToLocalChecked()
                                          .As<v8::Function>();

  // Expected stack: replacer, [RegExp Builtin], main, top-level
  // RegExp builtin should be skipped.
  CHECK_EQ(capturedFramesScriptData.size(), 3);
  for (size_t i = 0; i < capturedFramesScriptData.size(); ++i) {
    CHECK_EQ(capturedFramesScriptData[i].id, expected_id);
    CHECK_EQ(capturedFramesScriptData[i].context, context.local());
    CHECK(!capturedFramesScriptData[i].function.IsEmpty());
  }
  CHECK_EQ(capturedFramesScriptData[0].function, replacer_func);
  CHECK_EQ(capturedFramesScriptData[1].function, main_func);
}

TEST(CurrentScriptData_Wasm) {
  // Exit early if we don't have executable memory.
  if (i::v8_flags.jitless) return;

  v8::Isolate* isolate = CcTest::isolate();
  v8::HandleScope handle_scope(isolate);
  CapturedFramesScope scope;

  Local<ObjectTemplate> templ = ObjectTemplate::New(isolate);
  templ->Set(isolate, "CaptureStack",
             v8::FunctionTemplate::New(isolate, CaptureScriptData));
  LocalContext context(nullptr, templ);

  // Wasm module that calls back to JS.
  // (module (import "m" "f" (func $f)) (func (export "g") (call $f)))
  const char* source = R"SCRIPT(
    function userFunc() { CaptureStack(); }
    const bytes = new Uint8Array([
      0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00,
      0x01, 0x04, 0x01, 0x60, 0x00, 0x00,
      0x02, 0x07, 0x01, 0x01, 0x6d, 0x01, 0x66, 0x00, 0x00,
      0x03, 0x02, 0x01, 0x00,
      0x07, 0x05, 0x01, 0x01, 0x67, 0x00, 0x01,
      0x0a, 0x06, 0x01, 0x04, 0x00, 0x10, 0x00, 0x0b
    ]);
    const module = new WebAssembly.Module(bytes);
    const instance = new WebAssembly.Instance(module, {m: {f: userFunc}});
    function main() { instance.exports.g(); }
    main();
  )SCRIPT";

  v8::Local<v8::Script> script = CompileWithOrigin(source, "wasm_test", false);

  int expected_id = script->GetUnboundScript()->ScriptId();
  script->Run(context.local()).ToLocalChecked();

  v8::Local<v8::Function> user_func =
      context->Global()
          ->Get(context.local(), v8_str("userFunc"))
          .ToLocalChecked()
          .As<v8::Function>();
  v8::Local<v8::Function> main_func = context->Global()
                                          ->Get(context.local(), v8_str("main"))
                                          .ToLocalChecked()
                                          .As<v8::Function>();

  // Expected stack: userFunc, [Wasm Frame], main, top-level
  // Wasm frame should be skipped because it's not JavaScript.
  CHECK_EQ(capturedFramesScriptData.size(), 3);
  for (size_t i = 0; i < capturedFramesScriptData.size(); ++i) {
    CHECK_EQ(capturedFramesScriptData[i].id, expected_id);
    CHECK_EQ(capturedFramesScriptData[i].context, context.local());
    CHECK(!capturedFramesScriptData[i].function.IsEmpty());
  }
  CHECK_EQ(capturedFramesScriptData[0].function, user_func);
  CHECK_EQ(capturedFramesScriptData[1].function, main_func);
}

void AnalyzeStackOfInlineScriptWithSourceURL(
    const v8::FunctionCallbackInfo<v8::Value>& info) {
  CHECK(i::ValidateCallbackInfo(info));
  v8::HandleScope scope(info.GetIsolate());
  v8::Local<v8::StackTrace> stackTrace = v8::StackTrace::CurrentStackTrace(
      info.GetIsolate(), 10, v8::StackTrace::kDetailed);
  CHECK_EQ(4, stackTrace->GetFrameCount());
  v8::Local<v8::String> url = v8_str("source_url");
  for (int i = 0; i < 3; i++) {
    v8::Local<v8::String> name =
        stackTrace->GetFrame(info.GetIsolate(), i)->GetScriptNameOrSourceURL();
    CHECK(!name.IsEmpty());
    CHECK(url->Equals(info.GetIsolate()->GetCurrentContext(), name).FromJust());
  }
}

TEST(InlineScriptWithSourceURLInStackTrace) {
  v8::Isolate* isolate = CcTest::isolate();
  v8::HandleScope scope(isolate);
  Local<ObjectTemplate> templ = ObjectTemplate::New(isolate);
  templ->Set(isolate, "AnalyzeStackOfInlineScriptWithSourceURL",
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

  auto code = v8::base::OwnedVector<char>::NewForOverwrite(1024);
  v8::base::SNPrintF(code.as_vector(), source, "//# sourceURL=source_url");
  CHECK(CompileRunWithOrigin(code.begin(), "url", 0, 1)->IsUndefined());
  v8::base::SNPrintF(code.as_vector(), source, "//@ sourceURL=source_url");
  CHECK(CompileRunWithOrigin(code.begin(), "url", 0, 1)->IsUndefined());
}

void AnalyzeStackOfDynamicScriptWithSourceURL(
    const v8::FunctionCallbackInfo<v8::Value>& info) {
  CHECK(i::ValidateCallbackInfo(info));
  v8::HandleScope scope(info.GetIsolate());
  v8::Local<v8::StackTrace> stackTrace = v8::StackTrace::CurrentStackTrace(
      info.GetIsolate(), 10, v8::StackTrace::kDetailed);
  CHECK_EQ(4, stackTrace->GetFrameCount());
  v8::Local<v8::String> url = v8_str("source_url");
  for (int i = 0; i < 3; i++) {
    v8::Local<v8::String> name =
        stackTrace->GetFrame(info.GetIsolate(), i)->GetScriptNameOrSourceURL();
    CHECK(!name.IsEmpty());
    CHECK(url->Equals(info.GetIsolate()->GetCurrentContext(), name).FromJust());
  }
}

TEST(DynamicWithSourceURLInStackTrace) {
  v8::Isolate* isolate = CcTest::isolate();
  v8::HandleScope scope(isolate);
  Local<ObjectTemplate> templ = ObjectTemplate::New(isolate);
  templ->Set(isolate, "AnalyzeStackOfDynamicScriptWithSourceURL",
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

  auto code = v8::base::OwnedVector<char>::NewForOverwrite(1024);
  v8::base::SNPrintF(code.as_vector(), source, "//# sourceURL=source_url");
  CHECK(CompileRunWithOrigin(code.begin(), "url", 0, 0)->IsUndefined());
  v8::base::SNPrintF(code.as_vector(), source, "//@ sourceURL=source_url");
  CHECK(CompileRunWithOrigin(code.begin(), "url", 0, 0)->IsUndefined());
}

TEST(DynamicWithSourceURLInStackTraceString) {
  LocalContext context;
  v8::HandleScope scope(context.isolate());

  const char* source =
      "function outer() {\n"
      "  function foo() {\n"
      "    FAIL.FAIL;\n"
      "  }\n"
      "  foo();\n"
      "}\n"
      "outer()\n%s";

  auto code = v8::base::OwnedVector<char>::NewForOverwrite(1024);
  v8::base::SNPrintF(code.as_vector(), source, "//# sourceURL=source_url");
  v8::TryCatch try_catch(context.isolate());
  CompileRunWithOrigin(code.begin(), "", 0, 0);
  CHECK(try_catch.HasCaught());
  v8::String::Utf8Value stack(
      context.isolate(),
      try_catch.StackTrace(context.local()).ToLocalChecked());
  CHECK_NOT_NULL(strstr(*stack, "at foo (source_url:3:5)"));
}

UNINITIALIZED_TEST(CaptureStackTraceForStackOverflow) {
  // We must set v8_flags.stack_size before initializing the isolate.
  v8::internal::v8_flags.stack_size = 150;
  v8::Isolate::CreateParams create_params;
  create_params.array_buffer_allocator = CcTest::array_buffer_allocator();
  v8::Isolate* isolate = v8::Isolate::New(create_params);
  isolate->Enter();
  {
    LocalContext current(isolate);
    v8::HandleScope scope(isolate);
    isolate->SetCaptureStackTraceForUncaughtExceptions(
        true, 10, v8::StackTrace::kDetailed);
    v8::TryCatch try_catch(isolate);
    CompileRun("(function f(x) { f(x+1); })(0)");
    CHECK(try_catch.HasCaught());
  }
  isolate->Exit();
  isolate->Dispose();
}

void AnalyzeScriptNameInStack(const v8::FunctionCallbackInfo<v8::Value>& info) {
  CHECK(i::ValidateCallbackInfo(info));
  v8::HandleScope scope(info.GetIsolate());
  v8::Local<v8::String> name =
      v8::StackTrace::CurrentScriptNameOrSourceURL(info.GetIsolate());
  CHECK(!name.IsEmpty());
  CHECK(name->StringEquals(v8_str("test.js")));
}

TEST(CurrentScriptNameOrSourceURL_Name) {
  v8::Isolate* isolate = CcTest::isolate();
  v8::HandleScope scope(isolate);
  Local<ObjectTemplate> templ = ObjectTemplate::New(isolate);
  templ->Set(
      isolate, "AnalyzeScriptNameInStack",
      v8::FunctionTemplate::New(CcTest::isolate(), AnalyzeScriptNameInStack));
  LocalContext context(nullptr, templ);

  const char* source = R"(
    function foo() {
      AnalyzeScriptNameInStack();
    }
    foo();
  )";

  CHECK(CompileRunWithOrigin(source, "test.js")->IsUndefined());
}

void AnalyzeScriptURLInStack(const v8::FunctionCallbackInfo<v8::Value>& info) {
  CHECK(i::ValidateCallbackInfo(info));
  v8::HandleScope scope(info.GetIsolate());
  v8::Local<v8::String> name =
      v8::StackTrace::CurrentScriptNameOrSourceURL(info.GetIsolate());
  CHECK(!name.IsEmpty());
  CHECK(name->StringEquals(v8_str("foo.js")));
}

TEST(CurrentScriptNameOrSourceURL_SourceURL) {
  v8::Isolate* isolate = CcTest::isolate();
  v8::HandleScope scope(isolate);
  Local<ObjectTemplate> templ = ObjectTemplate::New(isolate);
  templ->Set(
      isolate, "AnalyzeScriptURLInStack",
      v8::FunctionTemplate::New(CcTest::isolate(), AnalyzeScriptURLInStack));
  LocalContext context(nullptr, templ);

  const char* source = R"(
    function foo() {
      AnalyzeScriptURLInStack();
    }
    foo();
    //# sourceURL=foo.js
  )";

  CHECK(CompileRunWithOrigin(source, "")->IsUndefined());
}
