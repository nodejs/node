// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/objects/backing-store.h"

#include "src/api/api-inl.h"
#include "src/objects/js-array-buffer-inl.h"
#include "src/wasm/wasm-objects-inl.h"
#include "test/unittests/heap/heap-utils.h"
#include "test/unittests/wasm/manually-externalized-buffer.h"
#include "test/unittests/wasm/wasm-runner.h"

namespace v8::internal::wasm {

class WasmBackingStoreTest
    : public WithWasmTrapHandlerMixin<TestWithHeapInternalsAndContext> {};

TEST_F(WasmBackingStoreTest, Run_WasmModule_Buffer_Externalized_Detach) {
  {
    // Regression test for
    // https://bugs.chromium.org/p/chromium/issues/detail?id=731046
    Isolate* isolate = i_isolate();
    HandleScope scope(isolate);
    MaybeHandle<JSArrayBuffer> result =
        isolate->factory()->NewJSArrayBufferAndBackingStore(
            kWasmPageSize, InitializedFlag{true});
    Handle<JSArrayBuffer> buffer = result.ToHandleChecked();

    // Embedder requests contents.
    ManuallyExternalizedBuffer external(buffer);

    JSArrayBuffer::Detach(buffer).Check();
    CHECK(buffer->was_detached());

    // Make sure we can write to the buffer without crashing
    uint32_t* int_buffer =
        reinterpret_cast<uint32_t*>(external.backing_store());
    int_buffer[0] = 0;
    // Embedder frees contents.
  }
  InvokeMemoryReducingMajorGCs();
}

TEST_F(WasmBackingStoreTest,
       Run_WasmModule_Buffer_Externalized_Regression_UseAfterFree) {
  {
    // Regression test for https://crbug.com/813876
    Isolate* isolate = i_isolate();
    HandleScope scope(isolate);
    MaybeDirectHandle<WasmMemoryObject> result = WasmMemoryObject::New(
        isolate, 1, 1, SharedFlag{false}, wasm::AddressType::kI32);
    DirectHandle<WasmMemoryObject> memory_object = result.ToHandleChecked();
    DirectHandle<JSArrayBuffer> buffer =
        WasmMemoryObject::GetArrayBuffer(isolate, memory_object);

    {
      // Embedder requests contents.
      ManuallyExternalizedBuffer external(buffer);

      // Growing (even by 0) detaches the old buffer.
      WasmMemoryObject::Grow(isolate, memory_object, 0);
      CHECK(buffer->was_detached());

      // Embedder frees contents.
    }

    // Make sure the memory object has a new buffer that can be written to.
    uint32_t* int_buffer = reinterpret_cast<uint32_t*>(
        memory_object->backing_store()->buffer_start());
    int_buffer[0] = 0;
  }
  InvokeMemoryReducingMajorGCs();
}

#if V8_TARGET_ARCH_64_BIT
TEST_F(WasmBackingStoreTest, BackingStore_Reclaim) {
  // Make sure we can allocate memories without running out of address space.
  Isolate* isolate = i_isolate();
  for (int i = 0; i < 256; ++i) {
    auto backing_store = BackingStore::AllocateWasmMemory(
        isolate, 1, 1, WasmMemoryFlag::kWasmMemory32, SharedFlag{false});
    CHECK(backing_store);
  }
}
#endif

}  // namespace v8::internal::wasm
