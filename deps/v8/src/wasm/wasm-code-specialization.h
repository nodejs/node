// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_WASM_WASM_CODE_SPECIALIZATION_H_
#define V8_WASM_WASM_CODE_SPECIALIZATION_H_

#include "src/assembler.h"
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
  CodeSpecialization();
  ~CodeSpecialization();

  // Update instance references in code. Instance references should only
  // appear in export wrappers.
  void UpdateInstanceReferences(Handle<WeakCell> old_weak_instance,
                                Handle<WeakCell> new_weak_instance);
  // Update all direct call sites based on the code table in the given instance.
  void RelocateDirectCalls(NativeModule* module);
  // Apply all relocations and patching to all code in the instance (wasm code
  // and exported functions).
  bool ApplyToWholeModule(NativeModule*,
                          ICacheFlushMode = FLUSH_ICACHE_IF_NEEDED);
  // Apply all relocations and patching to one wasm code object.
  bool ApplyToWasmCode(wasm::WasmCode*,
                       ICacheFlushMode = FLUSH_ICACHE_IF_NEEDED);

 private:
  Handle<WeakCell> old_weak_instance_;
  Handle<WeakCell> new_weak_instance_;
  NativeModule* relocate_direct_calls_module_ = nullptr;
};

}  // namespace wasm
}  // namespace internal
}  // namespace v8

#endif  // V8_WASM_WASM_CODE_SPECIALIZATION_H_
