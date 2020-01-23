// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/api/api-inl.h"
#include "src/objects/backing-store.h"
#include "src/wasm/wasm-objects.h"

#include "test/cctest/cctest.h"
#include "test/cctest/manually-externalized-buffer.h"

namespace v8 {
namespace internal {

using testing::ManuallyExternalizedBuffer;

TEST(Run_WasmModule_Buffer_Externalized_Detach) {
  {
    // Regression test for
    // https://bugs.chromium.org/p/chromium/issues/detail?id=731046
    Isolate* isolate = CcTest::InitIsolateOnce();
    HandleScope scope(isolate);
    MaybeHandle<JSArrayBuffer> result =
        isolate->factory()->NewJSArrayBufferAndBackingStore(
            wasm::kWasmPageSize, InitializedFlag::kZeroInitialized);
    Handle<JSArrayBuffer> buffer = result.ToHandleChecked();

    // Embedder requests contents.
    ManuallyExternalizedBuffer external(buffer);

    buffer->Detach();
    CHECK(buffer->was_detached());

    // Make sure we can write to the buffer without crashing
    uint32_t* int_buffer =
        reinterpret_cast<uint32_t*>(external.backing_store());
    int_buffer[0] = 0;
    // Embedder frees contents.
  }
  CcTest::CollectAllAvailableGarbage();
}

TEST(Run_WasmModule_Buffer_Externalized_Regression_UseAfterFree) {
  {
    // Regression test for https://crbug.com/813876
    Isolate* isolate = CcTest::InitIsolateOnce();
    HandleScope scope(isolate);
    MaybeHandle<WasmMemoryObject> result =
        WasmMemoryObject::New(isolate, 1, 1, SharedFlag::kNotShared);
    Handle<WasmMemoryObject> memory_object = result.ToHandleChecked();
    Handle<JSArrayBuffer> buffer(memory_object->array_buffer(), isolate);

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
        memory_object->array_buffer().backing_store());
    int_buffer[0] = 0;
  }
  CcTest::CollectAllAvailableGarbage();
}

#if V8_TARGET_ARCH_64_BIT
TEST(BackingStore_Reclaim) {
  // Make sure we can allocate memories without running out of address space.
  Isolate* isolate = CcTest::InitIsolateOnce();
  for (int i = 0; i < 256; ++i) {
    auto backing_store =
        BackingStore::AllocateWasmMemory(isolate, 1, 1, SharedFlag::kNotShared);
    CHECK(backing_store);
  }
}
#endif

}  // namespace internal
}  // namespace v8
