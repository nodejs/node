// Copyright 2012 the V8 project authors. All rights reserved.
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
//       copyright notice, this list of conditions and the following
//       disclaimer in the documentation and/or other materials provided
//       with the distribution.
//     * Neither the name of Google Inc. nor the names of its
//       contributors may be used to endorse or promote products derived
//       from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include <stdlib.h>

#include "src/v8.h"

#include "src/api-inl.h"
#include "src/base/platform/platform.h"
#include "src/compilation-cache.h"
#include "src/debug/debug.h"
#include "src/deoptimizer.h"
#include "src/isolate.h"
#include "src/objects-inl.h"
#include "test/cctest/cctest.h"

using ::v8::base::OS;
using ::v8::internal::Deoptimizer;
using ::v8::internal::EmbeddedVector;
using ::v8::internal::Handle;
using ::v8::internal::JSFunction;

// Size of temp buffer for formatting small strings.
#define SMALL_STRING_BUFFER_SIZE 80

// Utility class to set the following runtime flags when constructed and return
// to their default state when destroyed:
//   --allow-natives-syntax --always-opt --noturbo-inlining
class AlwaysOptimizeAllowNativesSyntaxNoInlining {
 public:
  AlwaysOptimizeAllowNativesSyntaxNoInlining()
      : always_opt_(i::FLAG_always_opt),
        allow_natives_syntax_(i::FLAG_allow_natives_syntax),
        turbo_inlining_(i::FLAG_turbo_inlining) {
    i::FLAG_always_opt = true;
    i::FLAG_allow_natives_syntax = true;
    i::FLAG_turbo_inlining = false;
  }

  ~AlwaysOptimizeAllowNativesSyntaxNoInlining() {
    i::FLAG_always_opt = always_opt_;
    i::FLAG_allow_natives_syntax = allow_natives_syntax_;
    i::FLAG_turbo_inlining = turbo_inlining_;
  }

 private:
  bool always_opt_;
  bool allow_natives_syntax_;
  bool turbo_inlining_;
};

// Utility class to set the following runtime flags when constructed and return
// to their default state when destroyed:
//   --allow-natives-syntax --noturbo-inlining
class AllowNativesSyntaxNoInlining {
 public:
  AllowNativesSyntaxNoInlining()
      : allow_natives_syntax_(i::FLAG_allow_natives_syntax),
        turbo_inlining_(i::FLAG_turbo_inlining) {
    i::FLAG_allow_natives_syntax = true;
    i::FLAG_turbo_inlining = false;
  }

  ~AllowNativesSyntaxNoInlining() {
    i::FLAG_allow_natives_syntax = allow_natives_syntax_;
    i::FLAG_turbo_inlining = turbo_inlining_;
  }

 private:
  bool allow_natives_syntax_;
  bool turbo_inlining_;
};

static Handle<JSFunction> GetJSFunction(v8::Local<v8::Context> context,
                                        const char* property_name) {
  v8::Local<v8::Function> fun = v8::Local<v8::Function>::Cast(
      context->Global()->Get(context, v8_str(property_name)).ToLocalChecked());
  return i::Handle<i::JSFunction>::cast(v8::Utils::OpenHandle(*fun));
}


TEST(DeoptimizeSimple) {
  ManualGCScope manual_gc_scope;
  LocalContext env;
  v8::HandleScope scope(env->GetIsolate());

  // Test lazy deoptimization of a simple function.
  {
    AlwaysOptimizeAllowNativesSyntaxNoInlining options;
    CompileRun(
        "var count = 0;"
        "function h() { %DeoptimizeFunction(f); }"
        "function g() { count++; h(); }"
        "function f() { g(); };"
        "f();");
  }
  CcTest::CollectAllGarbage();

  CHECK_EQ(1, env->Global()
                  ->Get(env.local(), v8_str("count"))
                  .ToLocalChecked()
                  ->Int32Value(env.local())
                  .FromJust());
  CHECK(!GetJSFunction(env.local(), "f")->IsOptimized());
  CHECK_EQ(0, Deoptimizer::GetDeoptimizedCodeCount(CcTest::i_isolate()));

  // Test lazy deoptimization of a simple function. Call the function after the
  // deoptimization while it is still activated further down the stack.
  {
    AlwaysOptimizeAllowNativesSyntaxNoInlining options;
    CompileRun(
        "var count = 0;"
        "function g() { count++; %DeoptimizeFunction(f); f(false); }"
        "function f(x) { if (x) { g(); } else { return } };"
        "f(true);");
  }
  CcTest::CollectAllGarbage();

  CHECK_EQ(1, env->Global()
                  ->Get(env.local(), v8_str("count"))
                  .ToLocalChecked()
                  ->Int32Value(env.local())
                  .FromJust());
  CHECK(!GetJSFunction(env.local(), "f")->IsOptimized());
  CHECK_EQ(0, Deoptimizer::GetDeoptimizedCodeCount(CcTest::i_isolate()));
}


TEST(DeoptimizeSimpleWithArguments) {
  ManualGCScope manual_gc_scope;
  LocalContext env;
  v8::HandleScope scope(env->GetIsolate());

  // Test lazy deoptimization of a simple function with some arguments.
  {
    AlwaysOptimizeAllowNativesSyntaxNoInlining options;
    CompileRun(
        "var count = 0;"
        "function h(x) { %DeoptimizeFunction(f); }"
        "function g(x, y) { count++; h(x); }"
        "function f(x, y, z) { g(1,x); y+z; };"
        "f(1, \"2\", false);");
  }
  CcTest::CollectAllGarbage();

  CHECK_EQ(1, env->Global()
                  ->Get(env.local(), v8_str("count"))
                  .ToLocalChecked()
                  ->Int32Value(env.local())
                  .FromJust());
  CHECK(!GetJSFunction(env.local(), "f")->IsOptimized());
  CHECK_EQ(0, Deoptimizer::GetDeoptimizedCodeCount(CcTest::i_isolate()));

  // Test lazy deoptimization of a simple function with some arguments. Call the
  // function after the deoptimization while it is still activated further down
  // the stack.
  {
    AlwaysOptimizeAllowNativesSyntaxNoInlining options;
    CompileRun(
        "var count = 0;"
        "function g(x, y) { count++; %DeoptimizeFunction(f); f(false, 1, y); }"
        "function f(x, y, z) { if (x) { g(x, y); } else { return y + z; } };"
        "f(true, 1, \"2\");");
  }
  CcTest::CollectAllGarbage();

  CHECK_EQ(1, env->Global()
                  ->Get(env.local(), v8_str("count"))
                  .ToLocalChecked()
                  ->Int32Value(env.local())
                  .FromJust());
  CHECK(!GetJSFunction(env.local(), "f")->IsOptimized());
  CHECK_EQ(0, Deoptimizer::GetDeoptimizedCodeCount(CcTest::i_isolate()));
}


TEST(DeoptimizeSimpleNested) {
  ManualGCScope manual_gc_scope;
  LocalContext env;
  v8::HandleScope scope(env->GetIsolate());

  // Test lazy deoptimization of a simple function. Have a nested function call
  // do the deoptimization.
  {
    AlwaysOptimizeAllowNativesSyntaxNoInlining options;
    CompileRun(
        "var count = 0;"
        "var result = 0;"
        "function h(x, y, z) { return x + y + z; }"
        "function g(z) { count++; %DeoptimizeFunction(f); return z;}"
        "function f(x,y,z) { return h(x, y, g(z)); };"
        "result = f(1, 2, 3);");
    CcTest::CollectAllGarbage();

    CHECK_EQ(1, env->Global()
                    ->Get(env.local(), v8_str("count"))
                    .ToLocalChecked()
                    ->Int32Value(env.local())
                    .FromJust());
    CHECK_EQ(6, env->Global()
                    ->Get(env.local(), v8_str("result"))
                    .ToLocalChecked()
                    ->Int32Value(env.local())
                    .FromJust());
    CHECK(!GetJSFunction(env.local(), "f")->IsOptimized());
    CHECK_EQ(0, Deoptimizer::GetDeoptimizedCodeCount(CcTest::i_isolate()));
  }
}


TEST(DeoptimizeRecursive) {
  ManualGCScope manual_gc_scope;
  LocalContext env;
  v8::HandleScope scope(env->GetIsolate());

  {
    // Test lazy deoptimization of a simple function called recursively. Call
    // the function recursively a number of times before deoptimizing it.
    AlwaysOptimizeAllowNativesSyntaxNoInlining options;
    CompileRun(
        "var count = 0;"
        "var calls = 0;"
        "function g() { count++; %DeoptimizeFunction(f); }"
        "function f(x) { calls++; if (x > 0) { f(x - 1); } else { g(); } };"
        "f(10);");
  }
  CcTest::CollectAllGarbage();

  CHECK_EQ(1, env->Global()
                  ->Get(env.local(), v8_str("count"))
                  .ToLocalChecked()
                  ->Int32Value(env.local())
                  .FromJust());
  CHECK_EQ(11, env->Global()
                   ->Get(env.local(), v8_str("calls"))
                   .ToLocalChecked()
                   ->Int32Value(env.local())
                   .FromJust());
  CHECK_EQ(0, Deoptimizer::GetDeoptimizedCodeCount(CcTest::i_isolate()));

  v8::Local<v8::Function> fun = v8::Local<v8::Function>::Cast(
      env->Global()
          ->Get(env.local(), v8_str(CcTest::isolate(), "f"))
          .ToLocalChecked());
  CHECK(!fun.IsEmpty());
}


TEST(DeoptimizeMultiple) {
  ManualGCScope manual_gc_scope;
  LocalContext env;
  v8::HandleScope scope(env->GetIsolate());

  {
    AlwaysOptimizeAllowNativesSyntaxNoInlining options;
    CompileRun(
        "var count = 0;"
        "var result = 0;"
        "function g() { count++;"
        "               %DeoptimizeFunction(f1);"
        "               %DeoptimizeFunction(f2);"
        "               %DeoptimizeFunction(f3);"
        "               %DeoptimizeFunction(f4);}"
        "function f4(x) { g(); };"
        "function f3(x, y, z) { f4(); return x + y + z; };"
        "function f2(x, y) { return x + f3(y + 1, y + 1, y + 1) + y; };"
        "function f1(x) { return f2(x + 1, x + 1) + x; };"
        "result = f1(1);");
  }
  CcTest::CollectAllGarbage();

  CHECK_EQ(1, env->Global()
                  ->Get(env.local(), v8_str("count"))
                  .ToLocalChecked()
                  ->Int32Value(env.local())
                  .FromJust());
  CHECK_EQ(14, env->Global()
                   ->Get(env.local(), v8_str("result"))
                   .ToLocalChecked()
                   ->Int32Value(env.local())
                   .FromJust());
  CHECK_EQ(0, Deoptimizer::GetDeoptimizedCodeCount(CcTest::i_isolate()));
}


TEST(DeoptimizeConstructor) {
  ManualGCScope manual_gc_scope;
  LocalContext env;
  v8::HandleScope scope(env->GetIsolate());

  {
    AlwaysOptimizeAllowNativesSyntaxNoInlining options;
    CompileRun(
        "var count = 0;"
        "function g() { count++;"
        "               %DeoptimizeFunction(f); }"
        "function f() {  g(); };"
        "result = new f() instanceof f;");
  }
  CcTest::CollectAllGarbage();

  CHECK_EQ(1, env->Global()
                  ->Get(env.local(), v8_str("count"))
                  .ToLocalChecked()
                  ->Int32Value(env.local())
                  .FromJust());
  CHECK(env->Global()
            ->Get(env.local(), v8_str("result"))
            .ToLocalChecked()
            ->IsTrue());
  CHECK_EQ(0, Deoptimizer::GetDeoptimizedCodeCount(CcTest::i_isolate()));

  {
    AlwaysOptimizeAllowNativesSyntaxNoInlining options;
    CompileRun(
        "var count = 0;"
        "var result = 0;"
        "function g() { count++;"
        "               %DeoptimizeFunction(f); }"
        "function f(x, y) { this.x = x; g(); this.y = y; };"
        "result = new f(1, 2);"
        "result = result.x + result.y;");
  }
  CcTest::CollectAllGarbage();

  CHECK_EQ(1, env->Global()
                  ->Get(env.local(), v8_str("count"))
                  .ToLocalChecked()
                  ->Int32Value(env.local())
                  .FromJust());
  CHECK_EQ(3, env->Global()
                  ->Get(env.local(), v8_str("result"))
                  .ToLocalChecked()
                  ->Int32Value(env.local())
                  .FromJust());
  CHECK_EQ(0, Deoptimizer::GetDeoptimizedCodeCount(CcTest::i_isolate()));
}


TEST(DeoptimizeConstructorMultiple) {
  ManualGCScope manual_gc_scope;
  LocalContext env;
  v8::HandleScope scope(env->GetIsolate());

  {
    AlwaysOptimizeAllowNativesSyntaxNoInlining options;
    CompileRun(
        "var count = 0;"
        "var result = 0;"
        "function g() { count++;"
        "               %DeoptimizeFunction(f1);"
        "               %DeoptimizeFunction(f2);"
        "               %DeoptimizeFunction(f3);"
        "               %DeoptimizeFunction(f4);}"
        "function f4(x) { this.result = x; g(); };"
        "function f3(x, y, z) { this.result = new f4(x + y + z).result; };"
        "function f2(x, y) {"
        "    this.result = x + new f3(y + 1, y + 1, y + 1).result + y; };"
        "function f1(x) { this.result = new f2(x + 1, x + 1).result + x; };"
        "result = new f1(1).result;");
  }
  CcTest::CollectAllGarbage();

  CHECK_EQ(1, env->Global()
                  ->Get(env.local(), v8_str("count"))
                  .ToLocalChecked()
                  ->Int32Value(env.local())
                  .FromJust());
  CHECK_EQ(14, env->Global()
                   ->Get(env.local(), v8_str("result"))
                   .ToLocalChecked()
                   ->Int32Value(env.local())
                   .FromJust());
  CHECK_EQ(0, Deoptimizer::GetDeoptimizedCodeCount(CcTest::i_isolate()));
}


UNINITIALIZED_TEST(DeoptimizeBinaryOperationADDString) {
  ManualGCScope manual_gc_scope;
  i::FLAG_concurrent_recompilation = false;
  AllowNativesSyntaxNoInlining options;
  v8::Isolate::CreateParams create_params;
  create_params.array_buffer_allocator = CcTest::array_buffer_allocator();
  v8::Isolate* isolate = v8::Isolate::New(create_params);
  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(isolate);
  isolate->Enter();
  {
    LocalContext env(isolate);
    v8::HandleScope scope(env->GetIsolate());

    const char* f_source = "function f(x, y) { return x + y; };";

    {
      // Compile function f and collect to type feedback to insert binary op
      // stub call in the optimized code.
      i::FLAG_prepare_always_opt = true;
      CompileRun(
          "var count = 0;"
          "var result = 0;"
          "var deopt = false;"
          "function X() { };"
          "X.prototype.toString = function () {"
          "  if (deopt) { count++; %DeoptimizeFunction(f); } return 'an X'"
          "};");
      CompileRun(f_source);
      CompileRun(
          "for (var i = 0; i < 5; i++) {"
          "  f('a+', new X());"
          "};");

      // Compile an optimized version of f.
      i::FLAG_always_opt = true;
      CompileRun(f_source);
      CompileRun("f('a+', new X());");
      CHECK(!i_isolate->use_optimizer() ||
            GetJSFunction(env.local(), "f")->IsOptimized());

      // Call f and force deoptimization while processing the binary operation.
      CompileRun(
          "deopt = true;"
          "var result = f('a+', new X());");
    }
    CcTest::CollectAllGarbage(i_isolate);

    CHECK(!GetJSFunction(env.local(), "f")->IsOptimized());
    CHECK_EQ(1, env->Global()
                    ->Get(env.local(), v8_str("count"))
                    .ToLocalChecked()
                    ->Int32Value(env.local())
                    .FromJust());
    v8::Local<v8::Value> result =
        env->Global()->Get(env.local(), v8_str("result")).ToLocalChecked();
    CHECK(result->IsString());
    v8::String::Utf8Value utf8(isolate, result);
    CHECK_EQ(0, strcmp("a+an X", *utf8));
    CHECK_EQ(0, Deoptimizer::GetDeoptimizedCodeCount(i_isolate));
  }
  isolate->Exit();
  isolate->Dispose();
}


static void CompileConstructorWithDeoptimizingValueOf() {
  CompileRun("var count = 0;"
             "var result = 0;"
             "var deopt = false;"
             "function X() { };"
             "X.prototype.valueOf = function () {"
             "  if (deopt) { count++; %DeoptimizeFunction(f); } return 8"
             "};");
}


static void TestDeoptimizeBinaryOpHelper(LocalContext* env,
                                         const char* binary_op) {
  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>((*env)->GetIsolate());
  EmbeddedVector<char, SMALL_STRING_BUFFER_SIZE> f_source_buffer;
  SNPrintF(f_source_buffer,
           "function f(x, y) { return x %s y; };",
           binary_op);
  char* f_source = f_source_buffer.start();

  AllowNativesSyntaxNoInlining options;
  // Compile function f and collect to type feedback to insert binary op stub
  // call in the optimized code.
  i::FLAG_prepare_always_opt = true;
  CompileConstructorWithDeoptimizingValueOf();
  CompileRun(f_source);
  CompileRun("for (var i = 0; i < 5; i++) {"
             "  f(8, new X());"
             "};");

  // Compile an optimized version of f.
  i::FLAG_always_opt = true;
  CompileRun(f_source);
  CompileRun("f(7, new X());");
  CHECK(!i_isolate->use_optimizer() ||
        GetJSFunction((*env).local(), "f")->IsOptimized());

  // Call f and force deoptimization while processing the binary operation.
  CompileRun("deopt = true;"
             "var result = f(7, new X());");
  CcTest::CollectAllGarbage(i_isolate);
  CHECK(!GetJSFunction((*env).local(), "f")->IsOptimized());
}


UNINITIALIZED_TEST(DeoptimizeBinaryOperationADD) {
  ManualGCScope manual_gc_scope;
  i::FLAG_concurrent_recompilation = false;
  v8::Isolate::CreateParams create_params;
  create_params.array_buffer_allocator = CcTest::array_buffer_allocator();
  v8::Isolate* isolate = v8::Isolate::New(create_params);
  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(isolate);
  isolate->Enter();
  {
    LocalContext env(isolate);
    v8::HandleScope scope(env->GetIsolate());

    TestDeoptimizeBinaryOpHelper(&env, "+");

    CHECK_EQ(1, env->Global()
                    ->Get(env.local(), v8_str("count"))
                    .ToLocalChecked()
                    ->Int32Value(env.local())
                    .FromJust());
    CHECK_EQ(15, env->Global()
                     ->Get(env.local(), v8_str("result"))
                     .ToLocalChecked()
                     ->Int32Value(env.local())
                     .FromJust());
    CHECK_EQ(0, Deoptimizer::GetDeoptimizedCodeCount(i_isolate));
  }
  isolate->Exit();
  isolate->Dispose();
}


UNINITIALIZED_TEST(DeoptimizeBinaryOperationSUB) {
  ManualGCScope manual_gc_scope;
  i::FLAG_concurrent_recompilation = false;
  v8::Isolate::CreateParams create_params;
  create_params.array_buffer_allocator = CcTest::array_buffer_allocator();
  v8::Isolate* isolate = v8::Isolate::New(create_params);
  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(isolate);
  isolate->Enter();
  {
    LocalContext env(isolate);
    v8::HandleScope scope(env->GetIsolate());

    TestDeoptimizeBinaryOpHelper(&env, "-");

    CHECK_EQ(1, env->Global()
                    ->Get(env.local(), v8_str("count"))
                    .ToLocalChecked()
                    ->Int32Value(env.local())
                    .FromJust());
    CHECK_EQ(-1, env->Global()
                     ->Get(env.local(), v8_str("result"))
                     .ToLocalChecked()
                     ->Int32Value(env.local())
                     .FromJust());
    CHECK_EQ(0, Deoptimizer::GetDeoptimizedCodeCount(i_isolate));
  }
  isolate->Exit();
  isolate->Dispose();
}


UNINITIALIZED_TEST(DeoptimizeBinaryOperationMUL) {
  ManualGCScope manual_gc_scope;
  i::FLAG_concurrent_recompilation = false;
  v8::Isolate::CreateParams create_params;
  create_params.array_buffer_allocator = CcTest::array_buffer_allocator();
  v8::Isolate* isolate = v8::Isolate::New(create_params);
  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(isolate);
  isolate->Enter();
  {
    LocalContext env(isolate);
    v8::HandleScope scope(env->GetIsolate());

    TestDeoptimizeBinaryOpHelper(&env, "*");

    CHECK_EQ(1, env->Global()
                    ->Get(env.local(), v8_str("count"))
                    .ToLocalChecked()
                    ->Int32Value(env.local())
                    .FromJust());
    CHECK_EQ(56, env->Global()
                     ->Get(env.local(), v8_str("result"))
                     .ToLocalChecked()
                     ->Int32Value(env.local())
                     .FromJust());
    CHECK_EQ(0, Deoptimizer::GetDeoptimizedCodeCount(i_isolate));
  }
  isolate->Exit();
  isolate->Dispose();
}


UNINITIALIZED_TEST(DeoptimizeBinaryOperationDIV) {
  ManualGCScope manual_gc_scope;
  i::FLAG_concurrent_recompilation = false;
  v8::Isolate::CreateParams create_params;
  create_params.array_buffer_allocator = CcTest::array_buffer_allocator();
  v8::Isolate* isolate = v8::Isolate::New(create_params);
  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(isolate);
  isolate->Enter();
  {
    LocalContext env(isolate);
    v8::HandleScope scope(env->GetIsolate());

    TestDeoptimizeBinaryOpHelper(&env, "/");

    CHECK_EQ(1, env->Global()
                    ->Get(env.local(), v8_str("count"))
                    .ToLocalChecked()
                    ->Int32Value(env.local())
                    .FromJust());
    CHECK_EQ(0, env->Global()
                    ->Get(env.local(), v8_str("result"))
                    .ToLocalChecked()
                    ->Int32Value(env.local())
                    .FromJust());
    CHECK_EQ(0, Deoptimizer::GetDeoptimizedCodeCount(i_isolate));
  }
  isolate->Exit();
  isolate->Dispose();
}


UNINITIALIZED_TEST(DeoptimizeBinaryOperationMOD) {
  ManualGCScope manual_gc_scope;
  i::FLAG_concurrent_recompilation = false;
  v8::Isolate::CreateParams create_params;
  create_params.array_buffer_allocator = CcTest::array_buffer_allocator();
  v8::Isolate* isolate = v8::Isolate::New(create_params);
  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(isolate);
  isolate->Enter();
  {
    LocalContext env(isolate);
    v8::HandleScope scope(env->GetIsolate());

    TestDeoptimizeBinaryOpHelper(&env, "%");

    CHECK_EQ(1, env->Global()
                    ->Get(env.local(), v8_str("count"))
                    .ToLocalChecked()
                    ->Int32Value(env.local())
                    .FromJust());
    CHECK_EQ(7, env->Global()
                    ->Get(env.local(), v8_str("result"))
                    .ToLocalChecked()
                    ->Int32Value(env.local())
                    .FromJust());
    CHECK_EQ(0, Deoptimizer::GetDeoptimizedCodeCount(i_isolate));
  }
  isolate->Exit();
  isolate->Dispose();
}


UNINITIALIZED_TEST(DeoptimizeCompare) {
  ManualGCScope manual_gc_scope;
  i::FLAG_concurrent_recompilation = false;
  v8::Isolate::CreateParams create_params;
  create_params.array_buffer_allocator = CcTest::array_buffer_allocator();
  v8::Isolate* isolate = v8::Isolate::New(create_params);
  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(isolate);
  isolate->Enter();
  {
    LocalContext env(isolate);
    v8::HandleScope scope(env->GetIsolate());

    const char* f_source = "function f(x, y) { return x < y; };";

    {
      AllowNativesSyntaxNoInlining options;
      // Compile function f and collect to type feedback to insert compare ic
      // call in the optimized code.
      i::FLAG_prepare_always_opt = true;
      CompileRun(
          "var count = 0;"
          "var result = 0;"
          "var deopt = false;"
          "function X() { };"
          "X.prototype.toString = function () {"
          "  if (deopt) { count++; %DeoptimizeFunction(f); } return 'b'"
          "};");
      CompileRun(f_source);
      CompileRun(
          "for (var i = 0; i < 5; i++) {"
          "  f('a', new X());"
          "};");

      // Compile an optimized version of f.
      i::FLAG_always_opt = true;
      CompileRun(f_source);
      CompileRun("f('a', new X());");
      CHECK(!i_isolate->use_optimizer() ||
            GetJSFunction(env.local(), "f")->IsOptimized());

      // Call f and force deoptimization while processing the comparison.
      CompileRun(
          "deopt = true;"
          "var result = f('a', new X());");
    }
    CcTest::CollectAllGarbage(i_isolate);

    CHECK(!GetJSFunction(env.local(), "f")->IsOptimized());
    CHECK_EQ(1, env->Global()
                    ->Get(env.local(), v8_str("count"))
                    .ToLocalChecked()
                    ->Int32Value(env.local())
                    .FromJust());
    CHECK_EQ(true, env->Global()
                       ->Get(env.local(), v8_str("result"))
                       .ToLocalChecked()
                       ->BooleanValue(isolate));
    CHECK_EQ(0, Deoptimizer::GetDeoptimizedCodeCount(i_isolate));
  }
  isolate->Exit();
  isolate->Dispose();
}


UNINITIALIZED_TEST(DeoptimizeLoadICStoreIC) {
  ManualGCScope manual_gc_scope;
  i::FLAG_concurrent_recompilation = false;
  v8::Isolate::CreateParams create_params;
  create_params.array_buffer_allocator = CcTest::array_buffer_allocator();
  v8::Isolate* isolate = v8::Isolate::New(create_params);
  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(isolate);
  isolate->Enter();
  {
    LocalContext env(isolate);
    v8::HandleScope scope(env->GetIsolate());

    // Functions to generate load/store/keyed load/keyed store IC calls.
    const char* f1_source = "function f1(x) { return x.y; };";
    const char* g1_source = "function g1(x) { x.y = 1; };";
    const char* f2_source = "function f2(x, y) { return x[y]; };";
    const char* g2_source = "function g2(x, y) { x[y] = 1; };";

    {
      AllowNativesSyntaxNoInlining options;
      // Compile functions and collect to type feedback to insert ic
      // calls in the optimized code.
      i::FLAG_prepare_always_opt = true;
      CompileRun(
          "var count = 0;"
          "var result = 0;"
          "var deopt = false;"
          "function X() { };"
          "X.prototype.__defineGetter__('y', function () {"
          "  if (deopt) { count++; %DeoptimizeFunction(f1); };"
          "  return 13;"
          "});"
          "X.prototype.__defineSetter__('y', function () {"
          "  if (deopt) { count++; %DeoptimizeFunction(g1); };"
          "});"
          "X.prototype.__defineGetter__('z', function () {"
          "  if (deopt) { count++; %DeoptimizeFunction(f2); };"
          "  return 13;"
          "});"
          "X.prototype.__defineSetter__('z', function () {"
          "  if (deopt) { count++; %DeoptimizeFunction(g2); };"
          "});");
      CompileRun(f1_source);
      CompileRun(g1_source);
      CompileRun(f2_source);
      CompileRun(g2_source);
      CompileRun(
          "for (var i = 0; i < 5; i++) {"
          "  f1(new X());"
          "  g1(new X());"
          "  f2(new X(), 'z');"
          "  g2(new X(), 'z');"
          "};");

      // Compile an optimized version of the functions.
      i::FLAG_always_opt = true;
      CompileRun(f1_source);
      CompileRun(g1_source);
      CompileRun(f2_source);
      CompileRun(g2_source);
      CompileRun("f1(new X());");
      CompileRun("g1(new X());");
      CompileRun("f2(new X(), 'z');");
      CompileRun("g2(new X(), 'z');");
      if (i_isolate->use_optimizer()) {
        CHECK(GetJSFunction(env.local(), "f1")->IsOptimized());
        CHECK(GetJSFunction(env.local(), "g1")->IsOptimized());
        CHECK(GetJSFunction(env.local(), "f2")->IsOptimized());
        CHECK(GetJSFunction(env.local(), "g2")->IsOptimized());
      }

      // Call functions and force deoptimization while processing the ics.
      CompileRun(
          "deopt = true;"
          "var result = f1(new X());"
          "g1(new X());"
          "f2(new X(), 'z');"
          "g2(new X(), 'z');");
    }
    CcTest::CollectAllGarbage(i_isolate);

    CHECK(!GetJSFunction(env.local(), "f1")->IsOptimized());
    CHECK(!GetJSFunction(env.local(), "g1")->IsOptimized());
    CHECK(!GetJSFunction(env.local(), "f2")->IsOptimized());
    CHECK(!GetJSFunction(env.local(), "g2")->IsOptimized());
    CHECK_EQ(4, env->Global()
                    ->Get(env.local(), v8_str("count"))
                    .ToLocalChecked()
                    ->Int32Value(env.local())
                    .FromJust());
    CHECK_EQ(13, env->Global()
                     ->Get(env.local(), v8_str("result"))
                     .ToLocalChecked()
                     ->Int32Value(env.local())
                     .FromJust());
  }
  isolate->Exit();
  isolate->Dispose();
}


UNINITIALIZED_TEST(DeoptimizeLoadICStoreICNested) {
  ManualGCScope manual_gc_scope;
  i::FLAG_concurrent_recompilation = false;
  v8::Isolate::CreateParams create_params;
  create_params.array_buffer_allocator = CcTest::array_buffer_allocator();
  v8::Isolate* isolate = v8::Isolate::New(create_params);
  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(isolate);
  isolate->Enter();
  {
    LocalContext env(isolate);
    v8::HandleScope scope(env->GetIsolate());

    // Functions to generate load/store/keyed load/keyed store IC calls.
    const char* f1_source = "function f1(x) { return x.y; };";
    const char* g1_source = "function g1(x) { x.y = 1; };";
    const char* f2_source = "function f2(x, y) { return x[y]; };";
    const char* g2_source = "function g2(x, y) { x[y] = 1; };";

    {
      AllowNativesSyntaxNoInlining options;
      // Compile functions and collect to type feedback to insert ic
      // calls in the optimized code.
      i::FLAG_prepare_always_opt = true;
      CompileRun(
          "var count = 0;"
          "var result = 0;"
          "var deopt = false;"
          "function X() { };"
          "X.prototype.__defineGetter__('y', function () {"
          "  g1(this);"
          "  return 13;"
          "});"
          "X.prototype.__defineSetter__('y', function () {"
          "  f2(this, 'z');"
          "});"
          "X.prototype.__defineGetter__('z', function () {"
          "  g2(this, 'z');"
          "});"
          "X.prototype.__defineSetter__('z', function () {"
          "  if (deopt) {"
          "    count++;"
          "    %DeoptimizeFunction(f1);"
          "    %DeoptimizeFunction(g1);"
          "    %DeoptimizeFunction(f2);"
          "    %DeoptimizeFunction(g2); };"
          "});");
      CompileRun(f1_source);
      CompileRun(g1_source);
      CompileRun(f2_source);
      CompileRun(g2_source);
      CompileRun(
          "for (var i = 0; i < 5; i++) {"
          "  f1(new X());"
          "  g1(new X());"
          "  f2(new X(), 'z');"
          "  g2(new X(), 'z');"
          "};");

      // Compile an optimized version of the functions.
      i::FLAG_always_opt = true;
      CompileRun(f1_source);
      CompileRun(g1_source);
      CompileRun(f2_source);
      CompileRun(g2_source);
      CompileRun("f1(new X());");
      CompileRun("g1(new X());");
      CompileRun("f2(new X(), 'z');");
      CompileRun("g2(new X(), 'z');");
      if (i_isolate->use_optimizer()) {
        CHECK(GetJSFunction(env.local(), "f1")->IsOptimized());
        CHECK(GetJSFunction(env.local(), "g1")->IsOptimized());
        CHECK(GetJSFunction(env.local(), "f2")->IsOptimized());
        CHECK(GetJSFunction(env.local(), "g2")->IsOptimized());
      }

      // Call functions and force deoptimization while processing the ics.
      CompileRun(
          "deopt = true;"
          "var result = f1(new X());");
    }
    CcTest::CollectAllGarbage(i_isolate);

    CHECK(!GetJSFunction(env.local(), "f1")->IsOptimized());
    CHECK(!GetJSFunction(env.local(), "g1")->IsOptimized());
    CHECK(!GetJSFunction(env.local(), "f2")->IsOptimized());
    CHECK(!GetJSFunction(env.local(), "g2")->IsOptimized());
    CHECK_EQ(1, env->Global()
                    ->Get(env.local(), v8_str("count"))
                    .ToLocalChecked()
                    ->Int32Value(env.local())
                    .FromJust());
    CHECK_EQ(13, env->Global()
                     ->Get(env.local(), v8_str("result"))
                     .ToLocalChecked()
                     ->Int32Value(env.local())
                     .FromJust());
  }
  isolate->Exit();
  isolate->Dispose();
}
