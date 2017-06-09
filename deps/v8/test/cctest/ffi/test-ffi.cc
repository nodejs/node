// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/api.h"
#include "src/codegen.h"
#include "src/ffi/ffi-compiler.h"
#include "src/objects-inl.h"
#include "test/cctest/cctest.h"

namespace v8 {
namespace internal {
namespace ffi {

static void hello_world() { printf("hello world from native code\n"); }

TEST(Run_FFI_Hello) {
  Isolate* isolate = CcTest::InitIsolateOnce();
  HandleScope scope(isolate);

  Handle<String> name =
      isolate->factory()->InternalizeUtf8String("hello_world");
  Handle<Object> undefined = isolate->factory()->undefined_value();

  AccountingAllocator allocator;
  Zone zone(&allocator, ZONE_NAME);
  FFISignature::Builder sig_builder(&zone, 0, 0);
  NativeFunction func = {sig_builder.Build(),
                         reinterpret_cast<uint8_t*>(hello_world)};

  Handle<JSFunction> jsfunc = CompileJSToNativeWrapper(isolate, name, func);

  Handle<Object> result =
      Execution::Call(isolate, jsfunc, undefined, 0, nullptr).ToHandleChecked();

  CHECK(result->IsUndefined(isolate));
}

static int add2(int x, int y) { return x + y; }

TEST(Run_FFI_add2) {
  Isolate* isolate = CcTest::InitIsolateOnce();
  HandleScope scope(isolate);

  Handle<String> name = isolate->factory()->InternalizeUtf8String("add2");
  Handle<Object> undefined = isolate->factory()->undefined_value();

  AccountingAllocator allocator;
  Zone zone(&allocator, ZONE_NAME);
  FFISignature::Builder sig_builder(&zone, 1, 2);
  sig_builder.AddReturn(FFIType::kInt32);
  sig_builder.AddParam(FFIType::kInt32);
  sig_builder.AddParam(FFIType::kInt32);
  NativeFunction func = {sig_builder.Build(), reinterpret_cast<uint8_t*>(add2)};

  Handle<JSFunction> jsfunc = CompileJSToNativeWrapper(isolate, name, func);

  // Simple math should work.
  {
    Handle<Object> args[] = {isolate->factory()->NewNumber(1.0),
                             isolate->factory()->NewNumber(41.0)};
    Handle<Object> result =
        Execution::Call(isolate, jsfunc, undefined, arraysize(args), args)
            .ToHandleChecked();
    CHECK_EQ(42.0, result->Number());
  }

  // Truncate floating point to integer.
  {
    Handle<Object> args[] = {isolate->factory()->NewNumber(1.9),
                             isolate->factory()->NewNumber(41.0)};
    Handle<Object> result =
        Execution::Call(isolate, jsfunc, undefined, arraysize(args), args)
            .ToHandleChecked();
    CHECK_EQ(42.0, result->Number());
  }

  // INT_MAX + 1 should wrap.
  {
    Handle<Object> args[] = {isolate->factory()->NewNumber(kMaxInt),
                             isolate->factory()->NewNumber(1)};
    Handle<Object> result =
        Execution::Call(isolate, jsfunc, undefined, arraysize(args), args)
            .ToHandleChecked();
    CHECK_EQ(kMinInt, result->Number());
  }

  // INT_MIN + -1 should wrap.
  {
    Handle<Object> args[] = {isolate->factory()->NewNumber(kMinInt),
                             isolate->factory()->NewNumber(-1)};
    Handle<Object> result =
        Execution::Call(isolate, jsfunc, undefined, arraysize(args), args)
            .ToHandleChecked();
    CHECK_EQ(kMaxInt, result->Number());
  }

  // Numbers get truncated to the 32 least significant bits.
  {
    Handle<Object> args[] = {isolate->factory()->NewNumber(1ull << 40),
                             isolate->factory()->NewNumber(-1)};
    Handle<Object> result =
        Execution::Call(isolate, jsfunc, undefined, arraysize(args), args)
            .ToHandleChecked();
    CHECK_EQ(-1, result->Number());
  }

  // String '57' converts to 57.
  {
    Handle<Object> args[] = {
        isolate->factory()->NewStringFromAsciiChecked("57"),
        isolate->factory()->NewNumber(41.0)};
    Handle<Object> result =
        Execution::Call(isolate, jsfunc, undefined, arraysize(args), args)
            .ToHandleChecked();
    CHECK_EQ(98.0, result->Number());
  }

  // String 'foo' converts to 0.
  {
    Handle<Object> args[] = {
        isolate->factory()->NewStringFromAsciiChecked("foo"),
        isolate->factory()->NewNumber(41.0)};
    Handle<Object> result =
        Execution::Call(isolate, jsfunc, undefined, arraysize(args), args)
            .ToHandleChecked();
    CHECK_EQ(41.0, result->Number());
  }

  // String '58o' converts to 0.
  {
    Handle<Object> args[] = {
        isolate->factory()->NewStringFromAsciiChecked("58o"),
        isolate->factory()->NewNumber(41.0)};
    Handle<Object> result =
        Execution::Call(isolate, jsfunc, undefined, arraysize(args), args)
            .ToHandleChecked();
    CHECK_EQ(41.0, result->Number());
  }

  // NaN converts to 0.
  {
    Handle<Object> args[] = {isolate->factory()->nan_value(),
                             isolate->factory()->NewNumber(41.0)};
    Handle<Object> result =
        Execution::Call(isolate, jsfunc, undefined, arraysize(args), args)
            .ToHandleChecked();
    CHECK_EQ(41.0, result->Number());
  }

  // null converts to 0.
  {
    Handle<Object> args[] = {isolate->factory()->null_value(),
                             isolate->factory()->NewNumber(41.0)};
    Handle<Object> result =
        Execution::Call(isolate, jsfunc, undefined, arraysize(args), args)
            .ToHandleChecked();
    CHECK_EQ(41.0, result->Number());
  }
}

static int add6(int a, int b, int c, int d, int e, int f) {
  return a + b + c + d + e + f;
}

TEST(Run_FFI_add6) {
  Isolate* isolate = CcTest::InitIsolateOnce();
  HandleScope scope(isolate);

  Handle<String> name = isolate->factory()->InternalizeUtf8String("add6");
  Handle<Object> undefined = isolate->factory()->undefined_value();

  AccountingAllocator allocator;
  Zone zone(&allocator, ZONE_NAME);
  FFISignature::Builder sig_builder(&zone, 1, 7);
  sig_builder.AddReturn(FFIType::kInt32);
  for (int i = 0; i < 7; i++) {
    sig_builder.AddParam(FFIType::kInt32);
  }
  NativeFunction func = {sig_builder.Build(), reinterpret_cast<uint8_t*>(add6)};

  Handle<JSFunction> jsfunc = CompileJSToNativeWrapper(isolate, name, func);
  Handle<Object> args[] = {
      isolate->factory()->NewNumber(1), isolate->factory()->NewNumber(2),
      isolate->factory()->NewNumber(3), isolate->factory()->NewNumber(4),
      isolate->factory()->NewNumber(5), isolate->factory()->NewNumber(6)};

  Handle<Object> result =
      Execution::Call(isolate, jsfunc, undefined, arraysize(args), args)
          .ToHandleChecked();

  CHECK_EQ(21.0, result->Number());

  {
    // Ensure builtin frames are generated
    FLAG_allow_natives_syntax = true;
    v8::Local<v8::Value> res = CompileRun(
        "var o = { valueOf: function() { %DebugTrace(); return 1; } }; o;");
    Handle<JSReceiver> param(v8::Utils::OpenHandle(v8::Object::Cast(*res)));
    Handle<Object> args[] = {param,
                             isolate->factory()->NewNumber(2),
                             isolate->factory()->NewNumber(3),
                             isolate->factory()->NewNumber(4),
                             isolate->factory()->NewNumber(5),
                             isolate->factory()->NewNumber(6),
                             isolate->factory()->NewNumber(21)};

    Handle<Object> result =
        Execution::Call(isolate, jsfunc, undefined, arraysize(args), args)
            .ToHandleChecked();
    CHECK_EQ(21.0, result->Number());
    CHECK_EQ(
        1.0,
        res->NumberValue(
               reinterpret_cast<v8::Isolate*>(isolate)->GetCurrentContext())
            .ToChecked());
  }
}

}  // namespace ffi
}  // namespace internal
}  // namespace v8
