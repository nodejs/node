// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "include/v8-context.h"
#include "include/v8-exception.h"
#include "include/v8-isolate.h"
#include "include/v8-local-handle.h"
#include "include/v8-persistent-handle.h"
#include "include/v8-template.h"
#include "src/flags/flags.h"
#include "test/unittests/test-utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace v8 {
namespace {

class APIExceptionTest : public TestWithIsolate {
 public:
  static void CEvaluate(const v8::FunctionCallbackInfo<v8::Value>& args) {
    v8::HandleScope scope(args.GetIsolate());
    TryRunJS(args.GetIsolate(),
             args[0]
                 ->ToString(args.GetIsolate()->GetCurrentContext())
                 .ToLocalChecked());
  }

  static void CCatcher(const v8::FunctionCallbackInfo<v8::Value>& args) {
    if (args.Length() < 1) {
      args.GetReturnValue().Set(false);
      return;
    }
    v8::HandleScope scope(args.GetIsolate());
    v8::TryCatch try_catch(args.GetIsolate());
    MaybeLocal<Value> result =
        TryRunJS(args.GetIsolate(),
                 args[0]
                     ->ToString(args.GetIsolate()->GetCurrentContext())
                     .ToLocalChecked());
    CHECK(!try_catch.HasCaught() || result.IsEmpty());
    args.GetReturnValue().Set(try_catch.HasCaught());
  }
};

class V8_NODISCARD ScopedExposeGc {
 public:
  ScopedExposeGc() : was_exposed_(i::FLAG_expose_gc) {
    i::FLAG_expose_gc = true;
  }
  ~ScopedExposeGc() { i::FLAG_expose_gc = was_exposed_; }

 private:
  const bool was_exposed_;
};

TEST_F(APIExceptionTest, ExceptionMessageDoesNotKeepContextAlive) {
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

}  // namespace
}  // namespace v8
