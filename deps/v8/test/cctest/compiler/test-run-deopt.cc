// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/frames-inl.h"
#include "test/cctest/cctest.h"
#include "test/cctest/compiler/function-tester.h"

namespace v8 {
namespace internal {
namespace compiler {

static void IsOptimized(const v8::FunctionCallbackInfo<v8::Value>& args) {
  JavaScriptFrameIterator it(CcTest::i_isolate());
  JavaScriptFrame* frame = it.frame();
  return args.GetReturnValue().Set(frame->is_optimized());
}


static void InstallIsOptimizedHelper(v8::Isolate* isolate) {
  v8::Local<v8::Context> context = isolate->GetCurrentContext();
  v8::Local<v8::FunctionTemplate> t =
      v8::FunctionTemplate::New(isolate, IsOptimized);
  CHECK(context->Global()
            ->Set(context, v8_str("IsOptimized"),
                  t->GetFunction(context).ToLocalChecked())
            .FromJust());
}


TEST(DeoptSimple) {
  FLAG_allow_natives_syntax = true;

  FunctionTester T(
      "(function f(a) {"
      "  var b = 1;"
      "  if (!IsOptimized()) return 0;"
      "  %DeoptimizeFunction(f);"
      "  if (IsOptimized()) return 0;"
      "  return a + b;"
      "})");

  InstallIsOptimizedHelper(CcTest::isolate());
  T.CheckCall(T.Val(2), T.Val(1));
}


TEST(DeoptSimpleInExpr) {
  FLAG_allow_natives_syntax = true;

  FunctionTester T(
      "(function f(a) {"
      "  var b = 1;"
      "  var c = 2;"
      "  if (!IsOptimized()) return 0;"
      "  var d = b + (%DeoptimizeFunction(f), c);"
      "  if (IsOptimized()) return 0;"
      "  return d + a;"
      "})");

  InstallIsOptimizedHelper(CcTest::isolate());
  T.CheckCall(T.Val(6), T.Val(3));
}


TEST(DeoptExceptionHandlerCatch) {
  FLAG_allow_natives_syntax = true;

  FunctionTester T(
      "(function f() {"
      "  var is_opt = IsOptimized;"
      "  try {"
      "    DeoptAndThrow(f);"
      "  } catch (e) {"
      "    return is_opt();"
      "  }"
      "})");

  CompileRun("function DeoptAndThrow(f) { %DeoptimizeFunction(f); throw 0; }");
  InstallIsOptimizedHelper(CcTest::isolate());
  T.CheckCall(T.false_value());
}


TEST(DeoptExceptionHandlerFinally) {
  FLAG_allow_natives_syntax = true;

  FunctionTester T(
      "(function f() {"
      "  var is_opt = IsOptimized;"
      "  try {"
      "    DeoptAndThrow(f);"
      "  } finally {"
      "    return is_opt();"
      "  }"
      "})");

  CompileRun("function DeoptAndThrow(f) { %DeoptimizeFunction(f); throw 0; }");
  InstallIsOptimizedHelper(CcTest::isolate());
  T.CheckCall(T.false_value());
}


TEST(DeoptTrivial) {
  FLAG_allow_natives_syntax = true;

  FunctionTester T(
      "(function foo() {"
      "  %DeoptimizeFunction(foo);"
      "  return 1;"
      "})");

  T.CheckCall(T.Val(1));
}

}  // namespace compiler
}  // namespace internal
}  // namespace v8
