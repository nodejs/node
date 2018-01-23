// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_WASM_MEMORY_H_
#define V8_WASM_MEMORY_H_

#include "src/flags.h"
#include "src/handles.h"
#include "src/objects/js-array.h"

namespace v8 {
namespace internal {
namespace wasm {

class WasmAllocationTracker {
 public:
  WasmAllocationTracker() {}
  ~WasmAllocationTracker();

  // ReserveAddressSpace attempts to increase the reserved address space counter
  // to determine whether there is enough headroom to allocate another guarded
  // Wasm memory. Returns true if successful (meaning it is okay to go ahead and
  // allocate the buffer), false otherwise.
  bool ReserveAddressSpace(size_t num_bytes);

  // Reduces the address space counter so that the space can be reused.
  void ReleaseAddressSpace(size_t num_bytes);

 private:
  std::atomic_size_t allocated_address_space_{0};

  DISALLOW_COPY_AND_ASSIGN(WasmAllocationTracker);
};

Handle<JSArrayBuffer> NewArrayBuffer(
    Isolate*, size_t size, bool require_guard_regions,
    SharedFlag shared = SharedFlag::kNotShared);

Handle<JSArrayBuffer> SetupArrayBuffer(
    Isolate*, void* allocation_base, size_t allocation_length,
    void* backing_store, size_t size, bool is_external,
    bool enable_guard_regions, SharedFlag shared = SharedFlag::kNotShared);

void DetachMemoryBuffer(Isolate* isolate, Handle<JSArrayBuffer> buffer,
                        bool free_memory);

}  // namespace wasm
}  // namespace internal
}  // namespace v8

#endif  // V8_WASM_MODULE_H_
