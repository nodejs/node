// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_WASM_CODE_SPECIALIZATION_H_
#define V8_WASM_CODE_SPECIALIZATION_H_

#include "src/assembler.h"
#include "src/identity-map.h"
#include "src/wasm/decoder.h"
#include "src/wasm/wasm-objects.h"

namespace v8 {
namespace internal {
namespace wasm {

uint32_t ExtractDirectCallIndex(wasm::Decoder& decoder, const byte* pc);

// Helper class to specialize wasm code for a specific instance, or to update
// code when memory / globals / tables change.
// This class in unhandlified, and contains a DisallowHeapAllocation field to
// ensure that no allocations happen while it is alive.
//
// Set up all relocations / patching that should be performed by the Relocate* /
// Patch* methods, then apply all changes in one step using the Apply* methods.
class CodeSpecialization {
 public:
  CodeSpecialization(Isolate*, Zone*);
  ~CodeSpecialization();

  // Update WasmContext references.
  void RelocateWasmContextReferences(Address new_context);
  // Update function table size.
  // TODO(wasm): Prepare this for more than one indirect function table.
  void PatchTableSize(uint32_t old_size, uint32_t new_size);
  // Update all direct call sites based on the code table in the given instance.
  void RelocateDirectCalls(Handle<WasmInstanceObject> instance);
  // Relocate an arbitrary object (e.g. function table).
  void RelocatePointer(Address old_obj, Address new_obj);

  // Apply all relocations and patching to all code in the instance (wasm code
  // and exported functions).
  bool ApplyToWholeInstance(WasmInstanceObject*,
                            ICacheFlushMode = FLUSH_ICACHE_IF_NEEDED);
  // Apply all relocations and patching to one wasm code object.
  bool ApplyToWasmCode(WasmCodeWrapper,
                       ICacheFlushMode = FLUSH_ICACHE_IF_NEEDED);

 private:
  Isolate* isolate_;
  Address new_wasm_context_address = 0;

  uint32_t old_function_table_size = 0;
  uint32_t new_function_table_size = 0;

  Handle<WasmInstanceObject> relocate_direct_calls_instance;

  std::map<Address, Address> pointers_to_relocate;
};

}  // namespace wasm
}  // namespace internal
}  // namespace v8

#endif  // V8_WASM_CODE_SPECIALIZATION_H_
