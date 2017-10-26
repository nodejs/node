// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdlib.h>

#include "src/v8.h"
#include "test/cctest/cctest.h"

#include "src/api.h"
#include "src/heap/heap.h"
#include "src/objects-inl.h"
#include "src/objects.h"

namespace v8 {
namespace internal {

void TestArrayBufferViewContents(LocalContext& env, bool should_use_buffer) {
  v8::Local<v8::Object> obj_a = v8::Local<v8::Object>::Cast(
      env->Global()
          ->Get(v8::Isolate::GetCurrent()->GetCurrentContext(), v8_str("a"))
          .ToLocalChecked());
  CHECK(obj_a->IsArrayBufferView());
  v8::Local<v8::ArrayBufferView> array_buffer_view =
      v8::Local<v8::ArrayBufferView>::Cast(obj_a);
  CHECK_EQ(array_buffer_view->HasBuffer(), should_use_buffer);
  unsigned char contents[4] = {23, 23, 23, 23};
  CHECK_EQ(sizeof(contents),
           array_buffer_view->CopyContents(contents, sizeof(contents)));
  CHECK_EQ(array_buffer_view->HasBuffer(), should_use_buffer);
  for (size_t i = 0; i < sizeof(contents); ++i) {
    CHECK_EQ(i, contents[i]);
  }
}


TEST(CopyContentsTypedArray) {
  LocalContext env;
  v8::HandleScope scope(env->GetIsolate());
  CompileRun(
      "var a = new Uint8Array(4);"
      "a[0] = 0;"
      "a[1] = 1;"
      "a[2] = 2;"
      "a[3] = 3;");
  TestArrayBufferViewContents(env, false);
}


TEST(CopyContentsArray) {
  LocalContext env;
  v8::HandleScope scope(env->GetIsolate());
  CompileRun("var a = new Uint8Array([0, 1, 2, 3]);");
  TestArrayBufferViewContents(env, false);
}


TEST(CopyContentsView) {
  LocalContext env;
  v8::HandleScope scope(env->GetIsolate());
  CompileRun(
      "var b = new ArrayBuffer(6);"
      "var c = new Uint8Array(b);"
      "c[0] = -1;"
      "c[1] = -1;"
      "c[2] = 0;"
      "c[3] = 1;"
      "c[4] = 2;"
      "c[5] = 3;"
      "var a = new DataView(b, 2);");
  TestArrayBufferViewContents(env, true);
}


TEST(AllocateNotExternal) {
  LocalContext env;
  v8::HandleScope scope(env->GetIsolate());
  void* memory = reinterpret_cast<Isolate*>(env->GetIsolate())
                     ->array_buffer_allocator()
                     ->Allocate(1024);
  v8::Local<v8::ArrayBuffer> buffer =
      v8::ArrayBuffer::New(env->GetIsolate(), memory, 1024,
                           v8::ArrayBufferCreationMode::kInternalized);
  CHECK(!buffer->IsExternal());
  CHECK_EQ(memory, buffer->GetContents().Data());
}

}  // namespace internal
}  // namespace v8
