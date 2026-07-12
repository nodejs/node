// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "include/v8-function.h"
#include "src/execution/frames-inl.h"
#include "test/unittests/compiler/function-tester.h"
#include "test/unittests/test-utils.h"

namespace v8 {
namespace internal {
namespace compiler {

static void IsOptimized(const v8::FunctionCallbackInfo<v8::Value>& info) {
  CHECK(i::ValidateCallbackInfo(info));
  JavaScriptStackFrameIterator it(
      reinterpret_cast<Isolate*>(info.GetIsolate()));
  JavaScriptFrame* frame = it.frame();
  return info.GetReturnValue().Set(frame->is_turbofan());
}

class RunDeoptTest : public TestWithContext {
 protected:
  void InstallIsOptimizedHelper(v8::Isolate* isolate) {
    v8::Local<v8::Context> context = isolate->GetCurrentContext();
    v8::Local<v8::FunctionTemplate> t =
        v8::FunctionTemplate::New(isolate, IsOptimized);
    CHECK(context->Global()
              ->Set(context, NewString("IsOptimized"),
                    t->GetFunction(context).ToLocalChecked())
              .FromJust());
  }
};

TEST_F(RunDeoptTest, DeoptSimple) {
  v8_flags.allow_natives_syntax = true;

  FunctionTester T(i_isolate(),
                   "(function f(a) {"
                   "  var b = 1;"
                   "  if (!IsOptimized()) return 0;"
                   "  %DeoptimizeFunction(f);"
                   "  if (IsOptimized()) return 0;"
                   "  return a + b;"
                   "})");

  InstallIsOptimizedHelper(v8_isolate());
  T.CheckCall(T.NewNumber(2), T.NewNumber(1));
}

TEST_F(RunDeoptTest, DeoptSimpleInExpr) {
  v8_flags.allow_natives_syntax = true;

  FunctionTester T(i_isolate(),
                   "(function f(a) {"
                   "  var b = 1;"
                   "  var c = 2;"
                   "  if (!IsOptimized()) return 0;"
                   "  var d = b + (%DeoptimizeFunction(f), c);"
                   "  if (IsOptimized()) return 0;"
                   "  return d + a;"
                   "})");

  InstallIsOptimizedHelper(v8_isolate());
  T.CheckCall(T.NewNumber(6), T.NewNumber(3));
}

TEST_F(RunDeoptTest, DeoptExceptionHandlerCatch) {
  v8_flags.allow_natives_syntax = true;

  FunctionTester T(i_isolate(),
                   "(function f() {"
                   "  var is_opt = IsOptimized;"
                   "  try {"
                   "    DeoptAndThrow(f);"
                   "  } catch (e) {"
                   "    return is_opt();"
                   "  }"
                   "})");

  TryRunJS("function DeoptAndThrow(f) { %DeoptimizeFunction(f); throw 0; }");
  InstallIsOptimizedHelper(v8_isolate());
  T.CheckCall(T.false_value());
}

TEST_F(RunDeoptTest, DeoptExceptionHandlerFinally) {
  v8_flags.allow_natives_syntax = true;

  FunctionTester T(i_isolate(),
                   "(function f() {"
                   "  var is_opt = IsOptimized;"
                   "  try {"
                   "    DeoptAndThrow(f);"
                   "  } finally {"
                   "    return is_opt();"
                   "  }"
                   "})");

  TryRunJS("function DeoptAndThrow(f) { %DeoptimizeFunction(f); throw 0; }");
  InstallIsOptimizedHelper(v8_isolate());
  T.CheckCall(T.false_value());
}

TEST_F(RunDeoptTest, DeoptTrivial) {
  v8_flags.allow_natives_syntax = true;

  FunctionTester T(i_isolate(),
                   "(function foo() {"
                   "  %DeoptimizeFunction(foo);"
                   "  return 1;"
                   "})");

  T.CheckCall(T.NewNumber(1));
}

}  // namespace compiler
}  // namespace internal
}  // namespace v8
