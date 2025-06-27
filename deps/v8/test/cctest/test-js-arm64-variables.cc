// Copyright 2010 the V8 project authors. All rights reserved.
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

// Adapted from test/mjsunit/compiler/variables.js

#include <limits.h>

#include "test/cctest/cctest.h"

namespace v8 {
namespace internal {
namespace test_js_arm64_variables {

static void ExpectInt32(Local<v8::Context> context, int32_t expected,
                        Local<Value> result) {
  CHECK(result->IsInt32());
  CHECK_EQ(expected, result->Int32Value(context).FromJust());
}


// Global variables.
TEST(global_variables) {
  LocalContext env;
  v8::HandleScope scope(env->GetIsolate());
  Local<Value> result = CompileRun(
"var x = 0;"
"function f0() { return x; }"
"f0();");
  ExpectInt32(env.local(), 0, result);
}


// Parameters.
TEST(parameters) {
  LocalContext env;
  v8::HandleScope scope(env->GetIsolate());
  Local<Value> result = CompileRun(
"function f1(x) { return x; }"
"f1(1);");
  ExpectInt32(env.local(), 1, result);
}


// Stack-allocated locals.
TEST(stack_allocated_locals) {
  LocalContext env;
  v8::HandleScope scope(env->GetIsolate());
  Local<Value> result = CompileRun(
"function f2() { var x = 2; return x; }"
"f2();");
  ExpectInt32(env.local(), 2, result);
}


// Context-allocated locals.  Local function forces x into f3's context.
TEST(context_allocated_locals) {
  LocalContext env;
  v8::HandleScope scope(env->GetIsolate());
  Local<Value> result = CompileRun(
"function f3(x) {"
"  function g() { return x; }"
"  return x;"
"}"
"f3(3);");
  ExpectInt32(env.local(), 3, result);
}


// Local function reads x from an outer context.
TEST(read_from_outer_context) {
  LocalContext env;
  v8::HandleScope scope(env->GetIsolate());
  Local<Value> result = CompileRun(
"function f4(x) {"
"  function g() { return x; }"
"  return g();"
"}"
"f4(4);");
  ExpectInt32(env.local(), 4, result);
}


// Local function reads x from an outer context.
TEST(lookup_slots) {
  LocalContext env;
  v8::HandleScope scope(env->GetIsolate());
  Local<Value> result = CompileRun(
"function f5(x) {"
"  with ({}) return x;"
"}"
"f5(5);");
  ExpectInt32(env.local(), 5, result);
}

}  // namespace test_js_arm64_variables
}  // namespace internal
}  // namespace v8
