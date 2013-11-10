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

#include "v8.h"

#include "cctest.h"
#include "compiler.h"
#include "execution.h"
#include "isolate.h"


using namespace v8::internal;


void SetSeeds(Handle<ByteArray> seeds, uint32_t state0, uint32_t state1) {
  for (int i = 0; i < 4; i++) {
    seeds->set(i, static_cast<byte>(state0 >> (i * kBitsPerByte)));
    seeds->set(i + 4, static_cast<byte>(state1 >> (i * kBitsPerByte)));
  }
}


void TestSeeds(Handle<JSFunction> fun,
               Handle<Context> context,
               uint32_t state0,
               uint32_t state1) {
  bool has_pending_exception;
  Handle<JSObject> global(context->global_object());
  Handle<ByteArray> seeds(context->random_seed());

  SetSeeds(seeds, state0, state1);
  Handle<Object> value = Execution::Call(
      context->GetIsolate(), fun, global, 0, NULL, &has_pending_exception);
  CHECK(value->IsHeapNumber());
  CHECK(fun->IsOptimized());
  double crankshaft_value = HeapNumber::cast(*value)->value();

  SetSeeds(seeds, state0, state1);
  V8::FillHeapNumberWithRandom(*value, *context);
  double runtime_value = HeapNumber::cast(*value)->value();
  CHECK_EQ(runtime_value, crankshaft_value);
}


TEST(CrankshaftRandom) {
  v8::V8::Initialize();
  // Skip test if crankshaft is disabled.
  if (!CcTest::i_isolate()->use_crankshaft()) return;
  v8::Isolate* v8_isolate = CcTest::isolate();
  v8::HandleScope scope(v8_isolate);
  v8::Context::Scope context_scope(v8::Context::New(v8_isolate));

  i::Isolate* isolate = reinterpret_cast<i::Isolate*>(v8_isolate);
  Handle<Context> context(isolate->context());
  Handle<JSObject> global(context->global_object());
  Handle<ByteArray> seeds(context->random_seed());
  bool has_pending_exception;

  CompileRun("function f() { return Math.random(); }");

  Object* string = CcTest::i_isolate()->factory()->InternalizeOneByteString(
      STATIC_ASCII_VECTOR("f"))->ToObjectChecked();
  MaybeObject* fun_object =
      context->global_object()->GetProperty(String::cast(string));
  Handle<JSFunction> fun(JSFunction::cast(fun_object->ToObjectChecked()));

  // Optimize function.
  Execution::Call(isolate, fun, global, 0, NULL, &has_pending_exception);
  Execution::Call(isolate, fun, global, 0, NULL, &has_pending_exception);
  if (!fun->IsOptimized()) fun->MarkForLazyRecompilation();

  // Test with some random values.
  TestSeeds(fun, context, 0xC0C0AFFE, 0x31415926);
  TestSeeds(fun, context, 0x01020304, 0xFFFFFFFF);
  TestSeeds(fun, context, 0x00000001, 0x00000001);
}
