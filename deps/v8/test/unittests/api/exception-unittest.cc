// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <sstream>

#include "include/v8-context.h"
#include "include/v8-exception.h"
#include "include/v8-function.h"
#include "include/v8-isolate.h"
#include "include/v8-local-handle.h"
#include "include/v8-persistent-handle.h"
#include "include/v8-template.h"
#include "src/debug/debug-interface.h"
#include "src/flags/flags.h"
#include "test/unittests/test-utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace v8 {
namespace {

class APIExceptionTest : public TestWithIsolate {
 public:
  static void CEvaluate(const v8::FunctionCallbackInfo<v8::Value>& info) {
    CHECK(i::ValidateCallbackInfo(info));
    v8::HandleScope scope(info.GetIsolate());
    TryRunJS(info.GetIsolate(),
             info[0]
                 ->ToString(info.GetIsolate()->GetCurrentContext())
                 .ToLocalChecked());
  }

  static void CCatcher(const v8::FunctionCallbackInfo<v8::Value>& info) {
    CHECK(i::ValidateCallbackInfo(info));
    if (info.Length() < 1) {
      info.GetReturnValue().Set(false);
      return;
    }
    v8::HandleScope scope(info.GetIsolate());
    v8::TryCatch try_catch(info.GetIsolate());
    MaybeLocal<Value> result =
        TryRunJS(info.GetIsolate(),
                 info[0]
                     ->ToString(info.GetIsolate()->GetCurrentContext())
                     .ToLocalChecked());
    CHECK(!try_catch.HasCaught() || result.IsEmpty());
    info.GetReturnValue().Set(try_catch.HasCaught());
  }
};

class V8_NODISCARD ScopedExposeGc {
 public:
  ScopedExposeGc() : was_exposed_(i::v8_flags.expose_gc) {
    i::v8_flags.expose_gc.Set(true);
  }
  ~ScopedExposeGc() { i::v8_flags.expose_gc.Set(was_exposed_); }

 private:
  const bool was_exposed_;
};

TEST_F(APIExceptionTest, ExceptionMessageDoesNotKeepContextAlive) {
  i::DisableConservativeStackScanningScopeForTesting no_stack_scanning(
      i_isolate()->heap());
  ScopedExposeGc expose_gc;
  Persistent<Context> weak_context;
  {
    HandleScope handle_scope(isolate());
    Local<Context> context = Context::New(isolate());
    weak_context.Reset(isolate(), context);
    weak_context.SetWeak();

    Context::Scope context_scope(context);
    TryCatch try_catch(isolate());
    isolate()->ThrowException(Undefined(isolate()));
  }
  isolate()->RequestGarbageCollectionForTesting(
      Isolate::kFullGarbageCollection);
  EXPECT_TRUE(weak_context.IsEmpty());
}

TEST_F(APIExceptionTest, TryCatchCustomException) {
  v8::HandleScope scope(isolate());
  v8::Local<Context> context = Context::New(isolate());
  v8::Context::Scope context_scope(context);
  v8::TryCatch try_catch(isolate());
  TryRunJS(
      "function CustomError() { this.a = 'b'; }"
      "(function f() { throw new CustomError(); })();");
  EXPECT_TRUE(try_catch.HasCaught());
  EXPECT_TRUE(try_catch.Exception()
                  ->ToObject(context)
                  .ToLocalChecked()
                  ->Get(context, NewString("a"))
                  .ToLocalChecked()
                  ->Equals(context, NewString("b"))
                  .FromJust());
}

class TryCatchNestedTest : public TestWithIsolate {
 public:
  void TryCatchNested1Helper(int depth) {
    if (depth > 0) {
      v8::TryCatch try_catch(isolate());
      try_catch.SetVerbose(true);
      TryCatchNested1Helper(depth - 1);
      EXPECT_TRUE(try_catch.HasCaught());
      try_catch.ReThrow();
    } else {
      isolate()->ThrowException(NewString("E1"));
    }
  }

  void TryCatchNested2Helper(int depth) {
    if (depth > 0) {
      v8::TryCatch try_catch(isolate());
      try_catch.SetVerbose(true);
      TryCatchNested2Helper(depth - 1);
      EXPECT_TRUE(try_catch.HasCaught());
      try_catch.ReThrow();
    } else {
      TryRunJS("throw 'E2';");
    }
  }
};

TEST_F(TryCatchNestedTest, TryCatchNested) {
  v8::HandleScope scope(isolate());
  Local<Context> context = Context::New(isolate());
  v8::Context::Scope context_scope(context);

  {
    // Test nested try-catch with a native throw in the end.
    v8::TryCatch try_catch(isolate());
    TryCatchNested1Helper(5);
    EXPECT_TRUE(try_catch.HasCaught());
    EXPECT_EQ(
        0,
        strcmp(*v8::String::Utf8Value(isolate(), try_catch.Exception()), "E1"));
  }

  {
    // Test nested try-catch with a JavaScript throw in the end.
    v8::TryCatch try_catch(isolate());
    TryCatchNested2Helper(5);
    EXPECT_TRUE(try_catch.HasCaught());
    EXPECT_EQ(
        0,
        strcmp(*v8::String::Utf8Value(isolate(), try_catch.Exception()), "E2"));
  }
}

TEST_F(APIExceptionTest, TryCatchFinallyUsingTryCatchHandler) {
  v8::HandleScope scope(isolate());
  Local<Context> context = Context::New(isolate());
  v8::Context::Scope context_scope(context);
  v8::TryCatch try_catch(isolate());
  TryRunJS("try { throw ''; } catch (e) {}");
  EXPECT_TRUE(!try_catch.HasCaught());
  TryRunJS("try { throw ''; } finally {}");
  EXPECT_TRUE(try_catch.HasCaught());
  try_catch.Reset();
  TryRunJS(
      "(function() {"
      "try { throw ''; } finally { return; }"
      "})()");
  EXPECT_TRUE(!try_catch.HasCaught());
  TryRunJS(
      "(function()"
      "  { try { throw ''; } finally { throw 0; }"
      "})()");
  EXPECT_TRUE(try_catch.HasCaught());
}

TEST_F(APIExceptionTest, TryFinallyMessage) {
  v8::HandleScope scope(isolate());
  v8::Local<Context> context = Context::New(isolate());
  v8::Context::Scope context_scope(context);
  {
    // Test that the original error message is not lost if there is a
    // recursive call into Javascript is done in the finally block, e.g. to
    // initialize an IC. (crbug.com/129171)
    TryCatch try_catch(isolate());
    const char* trigger_ic =
        "try {                      \n"
        "  throw new Error('test'); \n"
        "} finally {                \n"
        "  var x = 0;               \n"
        "  x++;                     \n"  // Trigger an IC initialization here.
        "}                          \n";
    TryRunJS(trigger_ic);
    EXPECT_TRUE(try_catch.HasCaught());
    Local<Message> message = try_catch.Message();
    EXPECT_TRUE(!message.IsEmpty());
    EXPECT_EQ(2, message->GetLineNumber(context).FromJust());
  }

  {
    // Test that the original exception message is indeed overwritten if
    // a new error is thrown in the finally block.
    TryCatch try_catch(isolate());
    const char* throw_again =
        "try {                       \n"
        "  throw new Error('test');  \n"
        "} finally {                 \n"
        "  var x = 0;                \n"
        "  x++;                      \n"
        "  throw new Error('again'); \n"  // This is the new uncaught error.
        "}                           \n";
    TryRunJS(throw_again);
    EXPECT_TRUE(try_catch.HasCaught());
    Local<Message> message = try_catch.Message();
    EXPECT_TRUE(!message.IsEmpty());
    EXPECT_EQ(6, message->GetLineNumber(context).FromJust());
  }
}

// Test that a try-finally block doesn't shadow a try-catch block
// when setting up an external handler.
//
// BUG(271): Some of the exception propagation does not work on the
// ARM simulator because the simulator separates the C++ stack and the
// JS stack.  This test therefore fails on the simulator.  The test is
// not threaded to allow the threading tests to run on the simulator.
TEST_F(APIExceptionTest, TryCatchInTryFinally) {
  v8::HandleScope scope(isolate());
  Local<ObjectTemplate> templ = ObjectTemplate::New(isolate());
  templ->Set(isolate(), "CCatcher",
             v8::FunctionTemplate::New(isolate(), CCatcher));
  Local<Context> context = Context::New(isolate(), nullptr, templ);
  v8::Context::Scope context_scope(context);
  Local<Value> result = RunJS(
      "try {"
      "  try {"
      "    CCatcher('throw 7;');"
      "  } finally {"
      "  }"
      "} catch (e) {"
      "}");
  EXPECT_TRUE(result->IsTrue());
}

TEST_F(APIExceptionTest, TryCatchFinallyStoresMessageUsingTryCatchHandler) {
  v8::HandleScope scope(isolate());
  Local<ObjectTemplate> templ = ObjectTemplate::New(isolate());
  templ->Set(isolate(), "CEvaluate",
             v8::FunctionTemplate::New(isolate(), CEvaluate));
  Local<Context> context = Context::New(isolate(), nullptr, templ);
  v8::Context::Scope context_scope(context);
  v8::TryCatch try_catch(isolate());
  TryRunJS(
      "try {"
      "  CEvaluate('throw 1;');"
      "} finally {"
      "}");
  EXPECT_TRUE(try_catch.HasCaught());
  EXPECT_TRUE(!try_catch.Message().IsEmpty());
  String::Utf8Value exception_value(isolate(), try_catch.Exception());
  EXPECT_EQ(0, strcmp(*exception_value, "1"));
  try_catch.Reset();
  TryRunJS(
      "try {"
      "  CEvaluate('throw 1;');"
      "} finally {"
      "  throw 2;"
      "}");
  EXPECT_TRUE(try_catch.HasCaught());
  EXPECT_TRUE(!try_catch.Message().IsEmpty());
  String::Utf8Value finally_exception_value(isolate(), try_catch.Exception());
  EXPECT_EQ(0, strcmp(*finally_exception_value, "2"));
}

namespace {
void PrintStackCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  std::ostringstream out;
  v8::Message::PrintCurrentStackTrace(info.GetIsolate(), out);
  info.GetReturnValue().Set(
      v8::String::NewFromUtf8(info.GetIsolate(), out.str().c_str())
          .ToLocalChecked());
}
}  // namespace

// Regression test for https://crbug.com/575539921.
TEST_F(APIExceptionTest, HugeFunctionNameOverflow) {
  v8::HandleScope scope(isolate());
  Local<Context> context = Context::New(isolate());
  v8::Context::Scope context_scope(context);

  // Build a ConsString close to String::kMaxLength without flattening 512MB.
  std::string make_name =
      "'a'.repeat(" + std::to_string(v8::String::kMaxLength - 10) + ")";
  Local<String> huge_name = RunJS(make_name.c_str()).As<String>();

  // 1. Native/API JSFunction with an overflowing NativeCodeFunctionSourceString
  // ("function " + name + "() { [native code] }").
  Local<Function> api_fn =
      v8::Function::New(context, PrintStackCallback).ToLocalChecked();
  api_fn->SetName(huge_name);
  context->Global()->Set(context, NewString("apiFn"), api_fn).FromJust();

  // Function.prototype.toString throws RangeError.
  {
    v8::TryCatch try_catch(isolate());
    EXPECT_TRUE(TryRunJS("apiFn.toString()").IsEmpty());
    EXPECT_TRUE(try_catch.HasCaught());
  }

  // Object::NoSideEffectsToMaybeString falls back to "[object Function]".
  {
    v8::TryCatch try_catch(isolate());
    EXPECT_TRUE(TryRunJS("Promise.prototype.then.call(apiFn)").IsEmpty());
    EXPECT_TRUE(try_catch.HasCaught());
    String::Utf8Value msg(isolate(), try_catch.Exception());
    EXPECT_NE(nullptr, strstr(*msg, "[object Function]"));
  }

  // v8::debug::GetFunctionDescription falls back without throwing inside
  // DisallowExceptionsDebugOnly.
  Local<String> desc = v8::debug::GetFunctionDescription(api_fn);
  String::Utf8Value desc_utf8(isolate(), desc);
  EXPECT_STREQ("function () { [native code] }", *desc_utf8);

  // 2. User JSFunction in a Script whose ScriptOrigin name is huge_name, so
  // AppendEvalOrigin overflows String::kMaxLength without flattening huge_name.
  v8::ScriptOrigin origin(huge_name);
  v8::Script::Compile(
      context, NewString("function evalFn(s) { return eval(s); }"), &origin)
      .ToLocalChecked()
      ->Run(context)
      .ToLocalChecked();

  RunJS(
      "Error.prepareStackTrace = (e, frames) => frames;"
      "globalThis.frames = evalFn('new Error().stack');"
      "Error.prepareStackTrace = undefined;");

  {
    v8::TryCatch try_catch(isolate());
    EXPECT_TRUE(TryRunJS("frames[0].getEvalOrigin()").IsEmpty());
    EXPECT_TRUE(try_catch.HasCaught());
  }
  {
    v8::TryCatch try_catch(isolate());
    EXPECT_TRUE(TryRunJS("frames[0].toString()").IsEmpty());
    EXPECT_TRUE(try_catch.HasCaught());
  }
  {
    v8::TryCatch try_catch(isolate());
    EXPECT_TRUE(
        TryRunJS("Error.stackTraceLimit = 1; evalFn('new Error()').stack")
            .IsEmpty());
    EXPECT_TRUE(try_catch.HasCaught());
  }

  // v8::Message::PrintCurrentStackTrace prints "<error>" for overflowing frames
  // without crashing or tripping DisallowExceptionsDebugOnly.
  Local<String> printed = RunJS("evalFn('apiFn()')").As<String>();
  String::Utf8Value printed_utf8(isolate(), printed);
  EXPECT_NE(nullptr, strstr(*printed_utf8, "<error>"));
}

void ThrowAndGetMessageCallback(
    const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  v8::HandleScope scope(isolate);
  Local<Context> context = isolate->GetCurrentContext();
  Local<Function> fn = info[0].As<Function>();
  v8::TryCatch try_catch(isolate);
  EXPECT_TRUE(fn->Call(context, v8::Undefined(isolate), 0, nullptr).IsEmpty());
  EXPECT_TRUE(try_catch.HasCaught());
  Local<String> msg = try_catch.Message()->Get();
  EXPECT_TRUE(reinterpret_cast<i::Isolate*>(isolate)->has_exception());
  info.GetReturnValue().Set(msg);
}

// Regression test for https://crbug.com/586579760: formatting error messages
// while another exception is pending must not throw InvalidStringLengthError
// (which would hit DCHECK(IsTerminationException(exception())) in
// Isolate::Throw) or clear the caller's pending exception.
TEST_F(APIExceptionTest, NestedTryCatchMessageOverflow) {
  v8::HandleScope scope(isolate());
  Local<Context> context = Context::New(isolate());
  v8::Context::Scope context_scope(context);

  Local<Function> helper =
      v8::Function::New(context, ThrowAndGetMessageCallback).ToLocalChecked();
  context->Global()
      ->Set(context, NewString("throwAndGetMessage"), helper)
      .FromJust();

  // Build ConsStrings near String::kMaxLength without flattening 512MB.
  std::string max_str_expr =
      "'a'.repeat(" + std::to_string(v8::String::kMaxLength) + ")";
  Local<String> max_str = RunJS(max_str_expr.c_str()).As<String>();
  context->Global()->Set(context, NewString("maxStr"), max_str).FromJust();

  // 1. MessageFormatter::TryFormat overflow ("Uncaught " + maxStr).
  {
    Local<String> msg =
        RunJS("throwAndGetMessage(() => { throw maxStr; })").As<String>();
    String::Utf8Value utf8(isolate(), msg);
    EXPECT_STREQ("<error>", *utf8);
  }

  // 2. Object::NoSideEffectsToMaybeString with max-length constructor name.
  {
    Local<Function> ctor =
        v8::Function::New(context, ThrowAndGetMessageCallback).ToLocalChecked();
    ctor->SetName(max_str);
    context->Global()->Set(context, NewString("hugeCtor"), ctor).FromJust();

    Local<String> msg =
        RunJS("throwAndGetMessage(() => { throw { constructor: hugeCtor }; })")
            .As<String>();
    String::Utf8Value utf8(isolate(), msg);
    EXPECT_NE(nullptr, strstr(*utf8, "#<aaaa"));
    EXPECT_NE(nullptr, strstr(*utf8, "<...>>"));
  }

  // 3. Object::NoSideEffectsToMaybeString with bound function whose
  // JSBoundFunction::GetName ("bound " + maxStr) overflows String::kMaxLength.
  {
    Local<String> msg = RunJS(
                            "const boundCtor = hugeCtor.bind();"
                            "throwAndGetMessage(() => {"
                            "  throw { constructor: boundCtor };"
                            "})")
                            .As<String>();
    String::Utf8Value utf8(isolate(), msg);
    EXPECT_STREQ("Uncaught [object Object]", *utf8);
  }
}

}  // namespace
}  // namespace v8
