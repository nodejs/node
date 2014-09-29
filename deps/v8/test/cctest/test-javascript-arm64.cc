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
#include "src/parser.h"
#include "src/snapshot.h"
#include "src/unicode-inl.h"
#include "src/utils.h"
#include "test/cctest/cctest.h"

using ::v8::Context;
using ::v8::Extension;
using ::v8::Function;
using ::v8::FunctionTemplate;
using ::v8::Handle;
using ::v8::HandleScope;
using ::v8::Local;
using ::v8::Message;
using ::v8::MessageCallback;
using ::v8::Object;
using ::v8::ObjectTemplate;
using ::v8::Persistent;
using ::v8::Script;
using ::v8::StackTrace;
using ::v8::String;
using ::v8::TryCatch;
using ::v8::Undefined;
using ::v8::V8;
using ::v8::Value;

static void ExpectBoolean(bool expected, Local<Value> result) {
  CHECK(result->IsBoolean());
  CHECK_EQ(expected, result->BooleanValue());
}


static void ExpectInt32(int32_t expected, Local<Value> result) {
  CHECK(result->IsInt32());
  CHECK_EQ(expected, result->Int32Value());
}


static void ExpectNumber(double expected, Local<Value> result) {
  CHECK(result->IsNumber());
  CHECK_EQ(expected, result->NumberValue());
}


static void ExpectUndefined(Local<Value> result) {
  CHECK(result->IsUndefined());
}


// Tests are sorted by order of implementation.

TEST(simple_value) {
  LocalContext env;
  v8::HandleScope scope(env->GetIsolate());
  Local<Value> result = CompileRun("0x271828;");
  ExpectInt32(0x271828, result);
}


TEST(global_variable) {
  LocalContext env;
  v8::HandleScope scope(env->GetIsolate());
  Local<Value> result = CompileRun("var my_global_var = 0x123; my_global_var;");
  ExpectInt32(0x123, result);
}


TEST(simple_function_call) {
  LocalContext env;
  v8::HandleScope scope(env->GetIsolate());
  Local<Value> result = CompileRun(
      "function foo() { return 0x314; }"
      "foo();");
  ExpectInt32(0x314, result);
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
  ExpectInt32(0x2468, result);
}

static void if_comparison_testcontext_helper(
    char const * op,
    char const * lhs,
    char const * rhs,
    int          expect) {
  char buffer[256];
  snprintf(buffer, sizeof(buffer),
           "var lhs = %s;"
           "var rhs = %s;"
           "if ( lhs %s rhs ) { 1; }"
           "else { 0; }",
           lhs, rhs, op);
  Local<Value> result = CompileRun(buffer);
  ExpectInt32(expect, result);
}

static void if_comparison_effectcontext_helper(
    char const * op,
    char const * lhs,
    char const * rhs,
    int          expect) {
  char buffer[256];
  snprintf(buffer, sizeof(buffer),
           "var lhs = %s;"
           "var rhs = %s;"
           "var test = lhs %s rhs;"
           "if ( test ) { 1; }"
           "else { 0; }",
           lhs, rhs, op);
  Local<Value> result = CompileRun(buffer);
  ExpectInt32(expect, result);
}

static void if_comparison_helper(
    char const * op,
    int          expect_when_lt,
    int          expect_when_eq,
    int          expect_when_gt) {
  // TODO(all): Non-SMI tests.

  if_comparison_testcontext_helper(op, "1", "3", expect_when_lt);
  if_comparison_testcontext_helper(op, "5", "5", expect_when_eq);
  if_comparison_testcontext_helper(op, "9", "7", expect_when_gt);

  if_comparison_effectcontext_helper(op, "1", "3", expect_when_lt);
  if_comparison_effectcontext_helper(op, "5", "5", expect_when_eq);
  if_comparison_effectcontext_helper(op, "9", "7", expect_when_gt);
}


TEST(if_comparison) {
  LocalContext env;
  v8::HandleScope scope(env->GetIsolate());

  if_comparison_helper("<",   1, 0, 0);
  if_comparison_helper("<=",  1, 1, 0);
  if_comparison_helper("==",  0, 1, 0);
  if_comparison_helper("===", 0, 1, 0);
  if_comparison_helper(">=",  0, 1, 1);
  if_comparison_helper(">",   0, 0, 1);
  if_comparison_helper("!=",  1, 0, 1);
  if_comparison_helper("!==", 1, 0, 1);
}


TEST(unary_plus) {
  LocalContext env;
  v8::HandleScope scope(env->GetIsolate());
  Local<Value> result;
  // SMI
  result = CompileRun("var a = 1234; +a");
  ExpectInt32(1234, result);
  // Number
  result = CompileRun("var a = 1234.5; +a");
  ExpectNumber(1234.5, result);
  // String (SMI)
  result = CompileRun("var a = '1234'; +a");
  ExpectInt32(1234, result);
  // String (Number)
  result = CompileRun("var a = '1234.5'; +a");
  ExpectNumber(1234.5, result);
  // Check side effects.
  result = CompileRun("var a = 1234; +(a = 4321); a");
  ExpectInt32(4321, result);
}


TEST(unary_minus) {
  LocalContext env;
  v8::HandleScope scope(env->GetIsolate());
  Local<Value> result;
  result = CompileRun("var a = 1234; -a");
  ExpectInt32(-1234, result);
  result = CompileRun("var a = 1234.5; -a");
  ExpectNumber(-1234.5, result);
  result = CompileRun("var a = 1234; -(a = 4321); a");
  ExpectInt32(4321, result);
  result = CompileRun("var a = '1234'; -a");
  ExpectInt32(-1234, result);
  result = CompileRun("var a = '1234.5'; -a");
  ExpectNumber(-1234.5, result);
}


TEST(unary_void) {
  LocalContext env;
  v8::HandleScope scope(env->GetIsolate());
  Local<Value> result;
  result = CompileRun("var a = 1234; void (a);");
  ExpectUndefined(result);
  result = CompileRun("var a = 0; void (a = 42); a");
  ExpectInt32(42, result);
  result = CompileRun("var a = 0; void (a = 42);");
  ExpectUndefined(result);
}


TEST(unary_not) {
  LocalContext env;
  v8::HandleScope scope(env->GetIsolate());
  Local<Value> result;
  result = CompileRun("var a = 1234; !a");
  ExpectBoolean(false, result);
  result = CompileRun("var a = 0; !a");
  ExpectBoolean(true, result);
  result = CompileRun("var a = 0; !(a = 1234); a");
  ExpectInt32(1234, result);
  result = CompileRun("var a = '1234'; !a");
  ExpectBoolean(false, result);
  result = CompileRun("var a = ''; !a");
  ExpectBoolean(true, result);
  result = CompileRun("var a = 1234; !!a");
  ExpectBoolean(true, result);
  result = CompileRun("var a = 0; !!a");
  ExpectBoolean(false, result);
  result = CompileRun("var a = 0; if ( !a ) { 1; } else { 0; }");
  ExpectInt32(1, result);
  result = CompileRun("var a = 1; if ( !a ) { 1; } else { 0; }");
  ExpectInt32(0, result);
}
