// Copyright 2013 the V8 project authors. All rights reserved.
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

#include <limits.h>

#include "src/v8.h"

#include "src/api.h"
#include "src/base/platform/platform.h"
#include "src/compilation-cache.h"
#include "src/execution.h"
#include "src/isolate.h"
#include "src/objects-inl.h"
#include "src/unicode-inl.h"
#include "src/utils.h"
#include "test/cctest/cctest.h"

namespace v8 {
namespace internal {
namespace test_javascript_arm64 {

static void ExpectBoolean(Local<v8::Context> context, bool expected,
                          Local<Value> result) {
  CHECK(result->IsBoolean());
  CHECK_EQ(expected, result->BooleanValue(context->GetIsolate()));
}

static void ExpectInt32(Local<v8::Context> context, int32_t expected,
                        Local<Value> result) {
  CHECK(result->IsInt32());
  CHECK_EQ(expected, result->Int32Value(context).FromJust());
}

static void ExpectNumber(Local<v8::Context> context, double expected,
                         Local<Value> result) {
  CHECK(result->IsNumber());
  CHECK_EQ(expected, result->NumberValue(context).FromJust());
}


static void ExpectUndefined(Local<Value> result) {
  CHECK(result->IsUndefined());
}


// Tests are sorted by order of implementation.

TEST(simple_value) {
  LocalContext env;
  v8::HandleScope scope(env->GetIsolate());
  Local<Value> result = CompileRun("0x271828;");
  ExpectInt32(env.local(), 0x271828, result);
}


TEST(global_variable) {
  LocalContext env;
  v8::HandleScope scope(env->GetIsolate());
  Local<Value> result = CompileRun("var my_global_var = 0x123; my_global_var;");
  ExpectInt32(env.local(), 0x123, result);
}


TEST(simple_function_call) {
  LocalContext env;
  v8::HandleScope scope(env->GetIsolate());
  Local<Value> result = CompileRun(
      "function foo() { return 0x314; }"
      "foo();");
  ExpectInt32(env.local(), 0x314, result);
}


TEST(binary_op) {
  LocalContext env;
  v8::HandleScope scope(env->GetIsolate());
  Local<Value> result = CompileRun(
      "function foo() {"
      "  var a = 0x1200;"
      "  var b = 0x0035;"
      "  return 2 * (a + b - 1);"
      "}"
      "foo();");
  ExpectInt32(env.local(), 0x2468, result);
}

static void if_comparison_testcontext_helper(Local<v8::Context> context,
                                             char const* op, char const* lhs,
                                             char const* rhs, int expect) {
  char buffer[256];
  snprintf(buffer, sizeof(buffer),
           "var lhs = %s;"
           "var rhs = %s;"
           "if ( lhs %s rhs ) { 1; }"
           "else { 0; }",
           lhs, rhs, op);
  Local<Value> result = CompileRun(buffer);
  ExpectInt32(context, expect, result);
}

static void if_comparison_effectcontext_helper(Local<v8::Context> context,
                                               char const* op, char const* lhs,
                                               char const* rhs, int expect) {
  char buffer[256];
  snprintf(buffer, sizeof(buffer),
           "var lhs = %s;"
           "var rhs = %s;"
           "var test = lhs %s rhs;"
           "if ( test ) { 1; }"
           "else { 0; }",
           lhs, rhs, op);
  Local<Value> result = CompileRun(buffer);
  ExpectInt32(context, expect, result);
}

static void if_comparison_helper(Local<v8::Context> context, char const* op,
                                 int expect_when_lt, int expect_when_eq,
                                 int expect_when_gt) {
  // TODO(all): Non-SMI tests.

  if_comparison_testcontext_helper(context, op, "1", "3", expect_when_lt);
  if_comparison_testcontext_helper(context, op, "5", "5", expect_when_eq);
  if_comparison_testcontext_helper(context, op, "9", "7", expect_when_gt);

  if_comparison_effectcontext_helper(context, op, "1", "3", expect_when_lt);
  if_comparison_effectcontext_helper(context, op, "5", "5", expect_when_eq);
  if_comparison_effectcontext_helper(context, op, "9", "7", expect_when_gt);
}


TEST(if_comparison) {
  LocalContext env;
  v8::HandleScope scope(env->GetIsolate());

  if_comparison_helper(env.local(), "<", 1, 0, 0);
  if_comparison_helper(env.local(), "<=", 1, 1, 0);
  if_comparison_helper(env.local(), "==", 0, 1, 0);
  if_comparison_helper(env.local(), "===", 0, 1, 0);
  if_comparison_helper(env.local(), ">=", 0, 1, 1);
  if_comparison_helper(env.local(), ">", 0, 0, 1);
  if_comparison_helper(env.local(), "!=", 1, 0, 1);
  if_comparison_helper(env.local(), "!==", 1, 0, 1);
}


TEST(unary_plus) {
  LocalContext env;
  v8::HandleScope scope(env->GetIsolate());
  Local<Value> result;
  // SMI
  result = CompileRun("var a = 1234; +a");
  ExpectInt32(env.local(), 1234, result);
  // Number
  result = CompileRun("var a = 1234.5; +a");
  ExpectNumber(env.local(), 1234.5, result);
  // String (SMI)
  result = CompileRun("var a = '1234'; +a");
  ExpectInt32(env.local(), 1234, result);
  // String (Number)
  result = CompileRun("var a = '1234.5'; +a");
  ExpectNumber(env.local(), 1234.5, result);
  // Check side effects.
  result = CompileRun("var a = 1234; +(a = 4321); a");
  ExpectInt32(env.local(), 4321, result);
}


TEST(unary_minus) {
  LocalContext env;
  v8::HandleScope scope(env->GetIsolate());
  Local<Value> result;
  result = CompileRun("var a = 1234; -a");
  ExpectInt32(env.local(), -1234, result);
  result = CompileRun("var a = 1234.5; -a");
  ExpectNumber(env.local(), -1234.5, result);
  result = CompileRun("var a = 1234; -(a = 4321); a");
  ExpectInt32(env.local(), 4321, result);
  result = CompileRun("var a = '1234'; -a");
  ExpectInt32(env.local(), -1234, result);
  result = CompileRun("var a = '1234.5'; -a");
  ExpectNumber(env.local(), -1234.5, result);
}


TEST(unary_void) {
  LocalContext env;
  v8::HandleScope scope(env->GetIsolate());
  Local<Value> result;
  result = CompileRun("var a = 1234; void (a);");
  ExpectUndefined(result);
  result = CompileRun("var a = 0; void (a = 42); a");
  ExpectInt32(env.local(), 42, result);
  result = CompileRun("var a = 0; void (a = 42);");
  ExpectUndefined(result);
}


TEST(unary_not) {
  LocalContext env;
  v8::HandleScope scope(env->GetIsolate());
  Local<Value> result;
  result = CompileRun("var a = 1234; !a");
  ExpectBoolean(env.local(), false, result);
  result = CompileRun("var a = 0; !a");
  ExpectBoolean(env.local(), true, result);
  result = CompileRun("var a = 0; !(a = 1234); a");
  ExpectInt32(env.local(), 1234, result);
  result = CompileRun("var a = '1234'; !a");
  ExpectBoolean(env.local(), false, result);
  result = CompileRun("var a = ''; !a");
  ExpectBoolean(env.local(), true, result);
  result = CompileRun("var a = 1234; !!a");
  ExpectBoolean(env.local(), true, result);
  result = CompileRun("var a = 0; !!a");
  ExpectBoolean(env.local(), false, result);
  result = CompileRun("var a = 0; if ( !a ) { 1; } else { 0; }");
  ExpectInt32(env.local(), 1, result);
  result = CompileRun("var a = 1; if ( !a ) { 1; } else { 0; }");
  ExpectInt32(env.local(), 0, result);
}

}  // namespace test_javascript_arm64
}  // namespace internal
}  // namespace v8
