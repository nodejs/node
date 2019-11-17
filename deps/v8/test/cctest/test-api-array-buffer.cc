// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "test/cctest/test-api.h"

#include "src/api/api-inl.h"

using ::v8::Array;
using ::v8::Context;
using ::v8::Local;
using ::v8::Value;

namespace {

void CheckDataViewIsDetached(v8::Local<v8::DataView> dv) {
  CHECK_EQ(0, static_cast<int>(dv->ByteLength()));
  CHECK_EQ(0, static_cast<int>(dv->ByteOffset()));
}

void CheckIsDetached(v8::Local<v8::TypedArray> ta) {
  CHECK_EQ(0, static_cast<int>(ta->ByteLength()));
  CHECK_EQ(0, static_cast<int>(ta->Length()));
  CHECK_EQ(0, static_cast<int>(ta->ByteOffset()));
}

void CheckIsTypedArrayVarDetached(const char* name) {
  i::ScopedVector<char> source(1024);
  i::SNPrintF(source,
              "%s.byteLength == 0 && %s.byteOffset == 0 && %s.length == 0",
              name, name, name);
  CHECK(CompileRun(source.begin())->IsTrue());
  v8::Local<v8::TypedArray> ta =
      v8::Local<v8::TypedArray>::Cast(CompileRun(name));
  CheckIsDetached(ta);
}

template <typename TypedArray, int kElementSize>
Local<TypedArray> CreateAndCheck(Local<v8::ArrayBuffer> ab, int byteOffset,
                                 int length) {
  v8::Local<TypedArray> ta = TypedArray::New(ab, byteOffset, length);
  CheckInternalFieldsAreZero<v8::ArrayBufferView>(ta);
  CHECK_EQ(byteOffset, static_cast<int>(ta->ByteOffset()));
  CHECK_EQ(length, static_cast<int>(ta->Length()));
  CHECK_EQ(length * kElementSize, static_cast<int>(ta->ByteLength()));
  return ta;
}

std::shared_ptr<v8::BackingStore> Externalize(Local<v8::ArrayBuffer> ab) {
  std::shared_ptr<v8::BackingStore> backing_store = ab->GetBackingStore();
  ab->Externalize(backing_store);
  CHECK(ab->IsExternal());
  return backing_store;
}

std::shared_ptr<v8::BackingStore> Externalize(Local<v8::SharedArrayBuffer> ab) {
  std::shared_ptr<v8::BackingStore> backing_store = ab->GetBackingStore();
  ab->Externalize(backing_store);
  CHECK(ab->IsExternal());
  return backing_store;
}

}  // namespace

THREADED_TEST(ArrayBuffer_ApiInternalToExternal) {
  LocalContext env;
  v8::Isolate* isolate = env->GetIsolate();
  v8::HandleScope handle_scope(isolate);

  Local<v8::ArrayBuffer> ab = v8::ArrayBuffer::New(isolate, 1024);
  CheckInternalFieldsAreZero(ab);
  CHECK_EQ(1024, ab->ByteLength());
  CHECK(!ab->IsExternal());
  CcTest::CollectAllGarbage();

  std::shared_ptr<v8::BackingStore> backing_store = Externalize(ab);
  CHECK_EQ(1024, backing_store->ByteLength());

  uint8_t* data = static_cast<uint8_t*>(backing_store->Data());
  CHECK_NOT_NULL(data);
  CHECK(env->Global()->Set(env.local(), v8_str("ab"), ab).FromJust());

  v8::Local<v8::Value> result = CompileRun("ab.byteLength");
  CHECK_EQ(1024, result->Int32Value(env.local()).FromJust());

  result = CompileRun(
      "var u8 = new Uint8Array(ab);"
      "u8[0] = 0xFF;"
      "u8[1] = 0xAA;"
      "u8.length");
  CHECK_EQ(1024, result->Int32Value(env.local()).FromJust());
  CHECK_EQ(0xFF, data[0]);
  CHECK_EQ(0xAA, data[1]);
  data[0] = 0xCC;
  data[1] = 0x11;
  result = CompileRun("u8[0] + u8[1]");
  CHECK_EQ(0xDD, result->Int32Value(env.local()).FromJust());
}

THREADED_TEST(ArrayBuffer_JSInternalToExternal) {
  LocalContext env;
  v8::Isolate* isolate = env->GetIsolate();
  v8::HandleScope handle_scope(isolate);

  v8::Local<v8::Value> result = CompileRun(
      "var ab1 = new ArrayBuffer(2);"
      "var u8_a = new Uint8Array(ab1);"
      "u8_a[0] = 0xAA;"
      "u8_a[1] = 0xFF; u8_a.buffer");
  Local<v8::ArrayBuffer> ab1 = Local<v8::ArrayBuffer>::Cast(result);
  CheckInternalFieldsAreZero(ab1);
  CHECK_EQ(2, ab1->ByteLength());
  CHECK(!ab1->IsExternal());
  std::shared_ptr<v8::BackingStore> backing_store = Externalize(ab1);

  result = CompileRun("ab1.byteLength");
  CHECK_EQ(2, result->Int32Value(env.local()).FromJust());
  result = CompileRun("u8_a[0]");
  CHECK_EQ(0xAA, result->Int32Value(env.local()).FromJust());
  result = CompileRun("u8_a[1]");
  CHECK_EQ(0xFF, result->Int32Value(env.local()).FromJust());
  result = CompileRun(
      "var u8_b = new Uint8Array(ab1);"
      "u8_b[0] = 0xBB;"
      "u8_a[0]");
  CHECK_EQ(0xBB, result->Int32Value(env.local()).FromJust());
  result = CompileRun("u8_b[1]");
  CHECK_EQ(0xFF, result->Int32Value(env.local()).FromJust());

  CHECK_EQ(2, backing_store->ByteLength());
  uint8_t* ab1_data = static_cast<uint8_t*>(backing_store->Data());
  CHECK_EQ(0xBB, ab1_data[0]);
  CHECK_EQ(0xFF, ab1_data[1]);
  ab1_data[0] = 0xCC;
  ab1_data[1] = 0x11;
  result = CompileRun("u8_a[0] + u8_a[1]");
  CHECK_EQ(0xDD, result->Int32Value(env.local()).FromJust());
}

THREADED_TEST(ArrayBuffer_External) {
  LocalContext env;
  v8::Isolate* isolate = env->GetIsolate();
  v8::HandleScope handle_scope(isolate);

  i::ScopedVector<uint8_t> my_data(100);
  memset(my_data.begin(), 0, 100);
  Local<v8::ArrayBuffer> ab3 =
      v8::ArrayBuffer::New(isolate, my_data.begin(), 100);
  CheckInternalFieldsAreZero(ab3);
  CHECK_EQ(100, ab3->ByteLength());
  CHECK(ab3->IsExternal());

  CHECK(env->Global()->Set(env.local(), v8_str("ab3"), ab3).FromJust());

  v8::Local<v8::Value> result = CompileRun("ab3.byteLength");
  CHECK_EQ(100, result->Int32Value(env.local()).FromJust());

  result = CompileRun(
      "var u8_b = new Uint8Array(ab3);"
      "u8_b[0] = 0xBB;"
      "u8_b[1] = 0xCC;"
      "u8_b.length");
  CHECK_EQ(100, result->Int32Value(env.local()).FromJust());
  CHECK_EQ(0xBB, my_data[0]);
  CHECK_EQ(0xCC, my_data[1]);
  my_data[0] = 0xCC;
  my_data[1] = 0x11;
  result = CompileRun("u8_b[0] + u8_b[1]");
  CHECK_EQ(0xDD, result->Int32Value(env.local()).FromJust());
}

THREADED_TEST(ArrayBuffer_DisableDetach) {
  LocalContext env;
  v8::Isolate* isolate = env->GetIsolate();
  v8::HandleScope handle_scope(isolate);

  i::ScopedVector<uint8_t> my_data(100);
  memset(my_data.begin(), 0, 100);
  Local<v8::ArrayBuffer> ab =
      v8::ArrayBuffer::New(isolate, my_data.begin(), 100);
  CHECK(ab->IsDetachable());

  i::Handle<i::JSArrayBuffer> buf = v8::Utils::OpenHandle(*ab);
  buf->set_is_detachable(false);

  CHECK(!ab->IsDetachable());
}

THREADED_TEST(ArrayBuffer_DetachingApi) {
  LocalContext env;
  v8::Isolate* isolate = env->GetIsolate();
  v8::HandleScope handle_scope(isolate);

  v8::Local<v8::ArrayBuffer> buffer = v8::ArrayBuffer::New(isolate, 1024);

  v8::Local<v8::Uint8Array> u8a =
      CreateAndCheck<v8::Uint8Array, 1>(buffer, 1, 1023);
  v8::Local<v8::Uint8ClampedArray> u8c =
      CreateAndCheck<v8::Uint8ClampedArray, 1>(buffer, 1, 1023);
  v8::Local<v8::Int8Array> i8a =
      CreateAndCheck<v8::Int8Array, 1>(buffer, 1, 1023);

  v8::Local<v8::Uint16Array> u16a =
      CreateAndCheck<v8::Uint16Array, 2>(buffer, 2, 511);
  v8::Local<v8::Int16Array> i16a =
      CreateAndCheck<v8::Int16Array, 2>(buffer, 2, 511);

  v8::Local<v8::Uint32Array> u32a =
      CreateAndCheck<v8::Uint32Array, 4>(buffer, 4, 255);
  v8::Local<v8::Int32Array> i32a =
      CreateAndCheck<v8::Int32Array, 4>(buffer, 4, 255);

  v8::Local<v8::Float32Array> f32a =
      CreateAndCheck<v8::Float32Array, 4>(buffer, 4, 255);
  v8::Local<v8::Float64Array> f64a =
      CreateAndCheck<v8::Float64Array, 8>(buffer, 8, 127);

  v8::Local<v8::DataView> dv = v8::DataView::New(buffer, 1, 1023);
  CheckInternalFieldsAreZero<v8::ArrayBufferView>(dv);
  CHECK_EQ(1, dv->ByteOffset());
  CHECK_EQ(1023, dv->ByteLength());

  Externalize(buffer);
  buffer->Detach();
  CHECK_EQ(0, buffer->ByteLength());
  CheckIsDetached(u8a);
  CheckIsDetached(u8c);
  CheckIsDetached(i8a);
  CheckIsDetached(u16a);
  CheckIsDetached(i16a);
  CheckIsDetached(u32a);
  CheckIsDetached(i32a);
  CheckIsDetached(f32a);
  CheckIsDetached(f64a);
  CheckDataViewIsDetached(dv);
}

THREADED_TEST(ArrayBuffer_DetachingScript) {
  LocalContext env;
  v8::Isolate* isolate = env->GetIsolate();
  v8::HandleScope handle_scope(isolate);

  CompileRun(
      "var ab = new ArrayBuffer(1024);"
      "var u8a = new Uint8Array(ab, 1, 1023);"
      "var u8c = new Uint8ClampedArray(ab, 1, 1023);"
      "var i8a = new Int8Array(ab, 1, 1023);"
      "var u16a = new Uint16Array(ab, 2, 511);"
      "var i16a = new Int16Array(ab, 2, 511);"
      "var u32a = new Uint32Array(ab, 4, 255);"
      "var i32a = new Int32Array(ab, 4, 255);"
      "var f32a = new Float32Array(ab, 4, 255);"
      "var f64a = new Float64Array(ab, 8, 127);"
      "var dv = new DataView(ab, 1, 1023);");

  v8::Local<v8::ArrayBuffer> ab =
      Local<v8::ArrayBuffer>::Cast(CompileRun("ab"));

  v8::Local<v8::DataView> dv = v8::Local<v8::DataView>::Cast(CompileRun("dv"));

  Externalize(ab);
  ab->Detach();
  CHECK_EQ(0, ab->ByteLength());
  CHECK_EQ(0, v8_run_int32value(v8_compile("ab.byteLength")));

  CheckIsTypedArrayVarDetached("u8a");
  CheckIsTypedArrayVarDetached("u8c");
  CheckIsTypedArrayVarDetached("i8a");
  CheckIsTypedArrayVarDetached("u16a");
  CheckIsTypedArrayVarDetached("i16a");
  CheckIsTypedArrayVarDetached("u32a");
  CheckIsTypedArrayVarDetached("i32a");
  CheckIsTypedArrayVarDetached("f32a");
  CheckIsTypedArrayVarDetached("f64a");

  CHECK(CompileRun("dv.byteLength == 0 && dv.byteOffset == 0")->IsTrue());
  CheckDataViewIsDetached(dv);
}

// TODO(v8:9380) the Contents data structure should be deprecated.
THREADED_TEST(ArrayBuffer_AllocationInformation) {
  LocalContext env;
  v8::Isolate* isolate = env->GetIsolate();
  v8::HandleScope handle_scope(isolate);

  const size_t ab_size = 1024;
  Local<v8::ArrayBuffer> ab = v8::ArrayBuffer::New(isolate, ab_size);
  v8::ArrayBuffer::Contents contents(ab->GetContents());

  // Array buffers should have normal allocation mode.
  CHECK_EQ(contents.AllocationMode(),
           v8::ArrayBuffer::Allocator::AllocationMode::kNormal);
  // The allocation must contain the buffer (normally they will be equal, but
  // this is not required by the contract).
  CHECK_NOT_NULL(contents.AllocationBase());
  const uintptr_t alloc =
      reinterpret_cast<uintptr_t>(contents.AllocationBase());
  const uintptr_t data = reinterpret_cast<uintptr_t>(contents.Data());
  CHECK_LE(alloc, data);
  CHECK_LE(data + contents.ByteLength(), alloc + contents.AllocationLength());
}

THREADED_TEST(ArrayBuffer_ExternalizeEmpty) {
  LocalContext env;
  v8::Isolate* isolate = env->GetIsolate();
  v8::HandleScope handle_scope(isolate);

  Local<v8::ArrayBuffer> ab = v8::ArrayBuffer::New(isolate, 2);
  CheckInternalFieldsAreZero(ab);
  CHECK_EQ(2, ab->ByteLength());
  CHECK(!ab->IsExternal());

  // Externalize the buffer (taking ownership of the backing store memory).
  std::shared_ptr<v8::BackingStore> backing_store = Externalize(ab);

  Local<v8::Uint8Array> u8a = v8::Uint8Array::New(ab, 0, 0);
  // Calling Buffer() will materialize the ArrayBuffer (transitioning it from
  // on-heap to off-heap if need be). This should not affect whether it is
  // marked as is_external or not.
  USE(u8a->Buffer());

  CHECK(ab->IsExternal());
  CHECK_EQ(2, backing_store->ByteLength());
}

THREADED_TEST(SharedArrayBuffer_ApiInternalToExternal) {
  i::FLAG_harmony_sharedarraybuffer = true;
  LocalContext env;
  v8::Isolate* isolate = env->GetIsolate();
  v8::HandleScope handle_scope(isolate);

  Local<v8::SharedArrayBuffer> ab = v8::SharedArrayBuffer::New(isolate, 1024);
  CheckInternalFieldsAreZero(ab);
  CHECK_EQ(1024, ab->ByteLength());
  CHECK(!ab->IsExternal());
  CcTest::CollectAllGarbage();

  std::shared_ptr<v8::BackingStore> backing_store = Externalize(ab);

  CHECK_EQ(1024, backing_store->ByteLength());
  uint8_t* data = static_cast<uint8_t*>(backing_store->Data());
  CHECK_NOT_NULL(data);
  CHECK(env->Global()->Set(env.local(), v8_str("ab"), ab).FromJust());

  v8::Local<v8::Value> result = CompileRun("ab.byteLength");
  CHECK_EQ(1024, result->Int32Value(env.local()).FromJust());

  result = CompileRun(
      "var u8 = new Uint8Array(ab);"
      "u8[0] = 0xFF;"
      "u8[1] = 0xAA;"
      "u8.length");
  CHECK_EQ(1024, result->Int32Value(env.local()).FromJust());
  CHECK_EQ(0xFF, data[0]);
  CHECK_EQ(0xAA, data[1]);
  data[0] = 0xCC;
  data[1] = 0x11;
  result = CompileRun("u8[0] + u8[1]");
  CHECK_EQ(0xDD, result->Int32Value(env.local()).FromJust());
}

THREADED_TEST(ArrayBuffer_ExternalReused) {
  LocalContext env;
  v8::Isolate* isolate = env->GetIsolate();
  v8::HandleScope handle_scope(isolate);

  i::ScopedVector<uint8_t> data(100);
  Local<v8::ArrayBuffer> ab1 = v8::ArrayBuffer::New(isolate, data.begin(), 100);
  std::shared_ptr<v8::BackingStore> bs1 = ab1->GetBackingStore();
  ab1->Detach();
  Local<v8::ArrayBuffer> ab2 = v8::ArrayBuffer::New(isolate, data.begin(), 100);
  std::shared_ptr<v8::BackingStore> bs2 = ab2->GetBackingStore();
  CHECK_EQ(bs1->Data(), bs2->Data());
}

THREADED_TEST(SharedArrayBuffer_ExternalReused) {
  LocalContext env;
  v8::Isolate* isolate = env->GetIsolate();
  v8::HandleScope handle_scope(isolate);

  i::ScopedVector<uint8_t> data(100);
  Local<v8::SharedArrayBuffer> ab1 =
      v8::SharedArrayBuffer::New(isolate, data.begin(), 100);
  std::shared_ptr<v8::BackingStore> bs1 = ab1->GetBackingStore();
  Local<v8::SharedArrayBuffer> ab2 =
      v8::SharedArrayBuffer::New(isolate, data.begin(), 100);
  std::shared_ptr<v8::BackingStore> bs2 = ab2->GetBackingStore();
  CHECK_EQ(bs1->Data(), bs2->Data());
}

THREADED_TEST(SharedArrayBuffer_JSInternalToExternal) {
  i::FLAG_harmony_sharedarraybuffer = true;
  LocalContext env;
  v8::Isolate* isolate = env->GetIsolate();
  v8::HandleScope handle_scope(isolate);

  v8::Local<v8::Value> result = CompileRun(
      "var ab1 = new SharedArrayBuffer(2);"
      "var u8_a = new Uint8Array(ab1);"
      "u8_a[0] = 0xAA;"
      "u8_a[1] = 0xFF; u8_a.buffer");
  Local<v8::SharedArrayBuffer> ab1 = Local<v8::SharedArrayBuffer>::Cast(result);
  CheckInternalFieldsAreZero(ab1);
  CHECK_EQ(2, ab1->ByteLength());
  CHECK(!ab1->IsExternal());
  std::shared_ptr<v8::BackingStore> backing_store = Externalize(ab1);

  result = CompileRun("ab1.byteLength");
  CHECK_EQ(2, result->Int32Value(env.local()).FromJust());
  result = CompileRun("u8_a[0]");
  CHECK_EQ(0xAA, result->Int32Value(env.local()).FromJust());
  result = CompileRun("u8_a[1]");
  CHECK_EQ(0xFF, result->Int32Value(env.local()).FromJust());
  result = CompileRun(
      "var u8_b = new Uint8Array(ab1);"
      "u8_b[0] = 0xBB;"
      "u8_a[0]");
  CHECK_EQ(0xBB, result->Int32Value(env.local()).FromJust());
  result = CompileRun("u8_b[1]");
  CHECK_EQ(0xFF, result->Int32Value(env.local()).FromJust());

  CHECK_EQ(2, backing_store->ByteLength());
  uint8_t* ab1_data = static_cast<uint8_t*>(backing_store->Data());
  CHECK_EQ(0xBB, ab1_data[0]);
  CHECK_EQ(0xFF, ab1_data[1]);
  ab1_data[0] = 0xCC;
  ab1_data[1] = 0x11;
  result = CompileRun("u8_a[0] + u8_a[1]");
  CHECK_EQ(0xDD, result->Int32Value(env.local()).FromJust());
}

THREADED_TEST(SharedArrayBuffer_External) {
  i::FLAG_harmony_sharedarraybuffer = true;
  LocalContext env;
  v8::Isolate* isolate = env->GetIsolate();
  v8::HandleScope handle_scope(isolate);

  i::ScopedVector<uint8_t> my_data(100);
  memset(my_data.begin(), 0, 100);
  Local<v8::SharedArrayBuffer> ab3 =
      v8::SharedArrayBuffer::New(isolate, my_data.begin(), 100);
  CheckInternalFieldsAreZero(ab3);
  CHECK_EQ(100, static_cast<int>(ab3->ByteLength()));
  CHECK(ab3->IsExternal());

  CHECK(env->Global()->Set(env.local(), v8_str("ab3"), ab3).FromJust());

  v8::Local<v8::Value> result = CompileRun("ab3.byteLength");
  CHECK_EQ(100, result->Int32Value(env.local()).FromJust());

  result = CompileRun(
      "var u8_b = new Uint8Array(ab3);"
      "u8_b[0] = 0xBB;"
      "u8_b[1] = 0xCC;"
      "u8_b.length");
  CHECK_EQ(100, result->Int32Value(env.local()).FromJust());
  CHECK_EQ(0xBB, my_data[0]);
  CHECK_EQ(0xCC, my_data[1]);
  my_data[0] = 0xCC;
  my_data[1] = 0x11;
  result = CompileRun("u8_b[0] + u8_b[1]");
  CHECK_EQ(0xDD, result->Int32Value(env.local()).FromJust());
}

// TODO(v8:9380) the Contents data structure should be deprecated.
THREADED_TEST(SharedArrayBuffer_AllocationInformation) {
  i::FLAG_harmony_sharedarraybuffer = true;
  LocalContext env;
  v8::Isolate* isolate = env->GetIsolate();
  v8::HandleScope handle_scope(isolate);

  const size_t ab_size = 1024;
  Local<v8::SharedArrayBuffer> ab =
      v8::SharedArrayBuffer::New(isolate, ab_size);
  v8::SharedArrayBuffer::Contents contents(ab->GetContents());

  // Array buffers should have normal allocation mode.
  CHECK_EQ(contents.AllocationMode(),
           v8::ArrayBuffer::Allocator::AllocationMode::kNormal);
  // The allocation must contain the buffer (normally they will be equal, but
  // this is not required by the contract).
  CHECK_NOT_NULL(contents.AllocationBase());
  const uintptr_t alloc =
      reinterpret_cast<uintptr_t>(contents.AllocationBase());
  const uintptr_t data = reinterpret_cast<uintptr_t>(contents.Data());
  CHECK_LE(alloc, data);
  CHECK_LE(data + contents.ByteLength(), alloc + contents.AllocationLength());
}

THREADED_TEST(SkipArrayBufferBackingStoreDuringGC) {
  LocalContext env;
  v8::Isolate* isolate = env->GetIsolate();
  v8::HandleScope handle_scope(isolate);

  // Make sure the pointer looks like a heap object
  uint8_t* store_ptr = reinterpret_cast<uint8_t*>(i::kHeapObjectTag);

  // Create ArrayBuffer with pointer-that-cannot-be-visited in the backing store
  Local<v8::ArrayBuffer> ab = v8::ArrayBuffer::New(isolate, store_ptr, 8);

  // Should not crash
  CcTest::CollectGarbage(i::NEW_SPACE);  // in survivor space now
  CcTest::CollectGarbage(i::NEW_SPACE);  // in old gen now
  CcTest::CollectAllGarbage();
  CcTest::CollectAllGarbage();

  // Should not move the pointer
  CHECK_EQ(ab->GetBackingStore()->Data(), store_ptr);
}

THREADED_TEST(SkipArrayBufferDuringScavenge) {
  LocalContext env;
  v8::Isolate* isolate = env->GetIsolate();
  v8::HandleScope handle_scope(isolate);

  // Make sure the pointer looks like a heap object
  Local<v8::Object> tmp = v8::Object::New(isolate);
  uint8_t* store_ptr =
      reinterpret_cast<uint8_t*>(*reinterpret_cast<uintptr_t*>(*tmp));

  // Make `store_ptr` point to from space
  CcTest::CollectGarbage(i::NEW_SPACE);

  // Create ArrayBuffer with pointer-that-cannot-be-visited in the backing store
  Local<v8::ArrayBuffer> ab = v8::ArrayBuffer::New(isolate, store_ptr, 8);

  // Should not crash,
  // i.e. backing store pointer should not be treated as a heap object pointer
  CcTest::CollectGarbage(i::NEW_SPACE);  // in survivor space now
  CcTest::CollectGarbage(i::NEW_SPACE);  // in old gen now

  // Use `ab` to silence compiler warning
  CHECK_EQ(ab->GetBackingStore()->Data(), store_ptr);
}

THREADED_TEST(Regress1006600) {
  LocalContext env;
  v8::Isolate* isolate = env->GetIsolate();
  v8::HandleScope handle_scope(isolate);

  Local<v8::Value> ab = CompileRunChecked(isolate, "new ArrayBuffer()");
  for (int i = 0; i < v8::ArrayBuffer::kEmbedderFieldCount; i++) {
    CHECK_NULL(ab.As<v8::Object>()->GetAlignedPointerFromInternalField(i));
  }
}

THREADED_TEST(ArrayBuffer_NewBackingStore) {
  LocalContext env;
  v8::Isolate* isolate = env->GetIsolate();
  v8::HandleScope handle_scope(isolate);
  std::shared_ptr<v8::BackingStore> backing_store =
      v8::ArrayBuffer::NewBackingStore(isolate, 100);
  Local<v8::ArrayBuffer> ab = v8::ArrayBuffer::New(isolate, backing_store);
  CHECK_EQ(backing_store.get(), ab->GetBackingStore().get());
}

THREADED_TEST(SharedArrayBuffer_NewBackingStore) {
  LocalContext env;
  v8::Isolate* isolate = env->GetIsolate();
  v8::HandleScope handle_scope(isolate);
  std::shared_ptr<v8::BackingStore> backing_store =
      v8::SharedArrayBuffer::NewBackingStore(isolate, 100);
  Local<v8::SharedArrayBuffer> ab =
      v8::SharedArrayBuffer::New(isolate, backing_store);
  CHECK_EQ(backing_store.get(), ab->GetBackingStore().get());
}

static void* backing_store_custom_data = nullptr;
static size_t backing_store_custom_length = 0;
static bool backing_store_custom_called = false;
const intptr_t backing_store_custom_deleter_data = 1234567;

static void BackingStoreCustomDeleter(void* data, size_t length,
                                      void* deleter_data) {
  CHECK(!backing_store_custom_called);
  CHECK_EQ(backing_store_custom_data, data);
  CHECK_EQ(backing_store_custom_length, length);
  CHECK_EQ(backing_store_custom_deleter_data,
           reinterpret_cast<intptr_t>(deleter_data));
  free(data);
  backing_store_custom_called = true;
}

TEST(ArrayBuffer_NewBackingStore_CustomDeleter) {
  {
    // Create and destroy a backing store.
    backing_store_custom_called = false;
    backing_store_custom_data = malloc(100);
    backing_store_custom_length = 100;
    v8::ArrayBuffer::NewBackingStore(
        backing_store_custom_data, backing_store_custom_length,
        BackingStoreCustomDeleter,
        reinterpret_cast<void*>(backing_store_custom_deleter_data));
  }
  CHECK(backing_store_custom_called);
}

TEST(SharedArrayBuffer_NewBackingStore_CustomDeleter) {
  {
    // Create and destroy a backing store.
    backing_store_custom_called = false;
    backing_store_custom_data = malloc(100);
    backing_store_custom_length = 100;
    v8::SharedArrayBuffer::NewBackingStore(
        backing_store_custom_data, backing_store_custom_length,
        BackingStoreCustomDeleter,
        reinterpret_cast<void*>(backing_store_custom_deleter_data));
  }
  CHECK(backing_store_custom_called);
}

class DummyAllocator final : public v8::ArrayBuffer::Allocator {
 public:
  DummyAllocator() : allocator_(NewDefaultAllocator()) {}

  ~DummyAllocator() override { CHECK_EQ(allocation_count(), 0); }

  void* Allocate(size_t length) override {
    allocation_count_++;
    return allocator_->Allocate(length);
  }
  void* AllocateUninitialized(size_t length) override {
    allocation_count_++;
    return allocator_->AllocateUninitialized(length);
  }
  void Free(void* data, size_t length) override {
    allocation_count_--;
    allocator_->Free(data, length);
  }

  uint64_t allocation_count() const { return allocation_count_; }

 private:
  std::unique_ptr<v8::ArrayBuffer::Allocator> allocator_;
  uint64_t allocation_count_ = 0;
};
