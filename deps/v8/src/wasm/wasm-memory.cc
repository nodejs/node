// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/wasm/wasm-memory.h"
#include "src/objects-inl.h"
#include "src/wasm/wasm-limits.h"
#include "src/wasm/wasm-module.h"

namespace v8 {
namespace internal {
namespace wasm {

void* TryAllocateBackingStore(Isolate* isolate, size_t size,
                              bool enable_guard_regions, void*& allocation_base,
                              size_t& allocation_length) {
  // TODO(eholk): Right now enable_guard_regions has no effect on 32-bit
  // systems. It may be safer to fail instead, given that other code might do
  // things that would be unsafe if they expected guard pages where there
  // weren't any.
  if (enable_guard_regions) {
    // TODO(eholk): On Windows we want to make sure we don't commit the guard
    // pages yet.

    // We always allocate the largest possible offset into the heap, so the
    // addressable memory after the guard page can be made inaccessible.
    allocation_length = RoundUp(kWasmMaxHeapOffset, base::OS::CommitPageSize());
    DCHECK_EQ(0, size % base::OS::CommitPageSize());

    // The Reserve makes the whole region inaccessible by default.
    allocation_base =
        isolate->array_buffer_allocator()->Reserve(allocation_length);
    if (allocation_base == nullptr) {
      return nullptr;
    }

    void* memory = allocation_base;

    // Make the part we care about accessible.
    isolate->array_buffer_allocator()->SetProtection(
        memory, size, v8::ArrayBuffer::Allocator::Protection::kReadWrite);

    reinterpret_cast<v8::Isolate*>(isolate)
        ->AdjustAmountOfExternalAllocatedMemory(size);

    return memory;
  } else {
    // TODO(titzer): use guard regions for minicage and merge with above code.
    CHECK_LE(size, kV8MaxWasmMemoryBytes);
    allocation_length =
        base::bits::RoundUpToPowerOfTwo32(static_cast<uint32_t>(size));
    void* memory =
        size == 0
            ? nullptr
            : isolate->array_buffer_allocator()->Allocate(allocation_length);
    allocation_base = memory;
    return memory;
  }
}

Handle<JSArrayBuffer> SetupArrayBuffer(Isolate* isolate, void* allocation_base,
                                       size_t allocation_length,
                                       void* backing_store, size_t size,
                                       bool is_external,
                                       bool enable_guard_regions,
                                       SharedFlag shared) {
  Handle<JSArrayBuffer> buffer =
      isolate->factory()->NewJSArrayBuffer(shared, TENURED);
  DCHECK_GE(kMaxInt, size);
  if (shared == SharedFlag::kShared) DCHECK(FLAG_experimental_wasm_threads);
  JSArrayBuffer::Setup(buffer, isolate, is_external, allocation_base,
                       allocation_length, backing_store, static_cast<int>(size),
                       shared);
  buffer->set_is_neuterable(false);
  buffer->set_is_growable(true);
  buffer->set_has_guard_region(enable_guard_regions);
  return buffer;
}

Handle<JSArrayBuffer> NewArrayBuffer(Isolate* isolate, size_t size,
                                     bool enable_guard_regions,
                                     SharedFlag shared) {
  // Check against kMaxInt, since the byte length is stored as int in the
  // JSArrayBuffer. Note that wasm_max_mem_pages can be raised from the command
  // line, and we don't want to fail a CHECK then.
  if (size > FLAG_wasm_max_mem_pages * WasmModule::kPageSize ||
      size > kMaxInt) {
    // TODO(titzer): lift restriction on maximum memory allocated here.
    return Handle<JSArrayBuffer>::null();
  }

  void* allocation_base = nullptr;  // Set by TryAllocateBackingStore
  size_t allocation_length = 0;     // Set by TryAllocateBackingStore
  // Do not reserve memory till non zero memory is encountered.
  void* memory =
      (size == 0) ? nullptr
                  : TryAllocateBackingStore(isolate, size, enable_guard_regions,
                                            allocation_base, allocation_length);

  if (size > 0 && memory == nullptr) {
    return Handle<JSArrayBuffer>::null();
  }

#if DEBUG
  // Double check the API allocator actually zero-initialized the memory.
  const byte* bytes = reinterpret_cast<const byte*>(memory);
  for (size_t i = 0; i < size; ++i) {
    DCHECK_EQ(0, bytes[i]);
  }
#endif

  constexpr bool is_external = false;
  return SetupArrayBuffer(isolate, allocation_base, allocation_length, memory,
                          size, is_external, enable_guard_regions, shared);
}

void DetachMemoryBuffer(Isolate* isolate, Handle<JSArrayBuffer> buffer,
                        bool free_memory) {
  const bool is_external = buffer->is_external();
  DCHECK(!buffer->is_neuterable());
  if (!is_external) {
    buffer->set_is_external(true);
    isolate->heap()->UnregisterArrayBuffer(*buffer);
    if (free_memory) {
      // We need to free the memory before neutering the buffer because
      // FreeBackingStore reads buffer->allocation_base(), which is nulled out
      // by Neuter. This means there is a dangling pointer until we neuter the
      // buffer. Since there is no way for the user to directly call
      // FreeBackingStore, we can ensure this is safe.
      buffer->FreeBackingStore();
    }
  }
  buffer->set_is_neuterable(true);
  buffer->Neuter();
}

}  // namespace wasm
}  // namespace internal
}  // namespace v8
