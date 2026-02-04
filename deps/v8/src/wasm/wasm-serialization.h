// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_WASM_WASM_SERIALIZATION_H_
#define V8_WASM_WASM_SERIALIZATION_H_

#if !V8_ENABLE_WEBASSEMBLY
#error This header should only be included if WebAssembly is enabled.
#endif  // !V8_ENABLE_WEBASSEMBLY

#include "src/wasm/wasm-code-manager.h"

namespace v8::internal::wasm {

// Support for serializing WebAssembly {NativeModule} objects. This class takes
// a snapshot of the module state at instantiation, and other code that modifies
// the module after that won't affect the serialized result.
class V8_EXPORT_PRIVATE WasmSerializer {
 public:
  explicit WasmSerializer(NativeModule* native_module);

  // Measure the required buffer size needed for serialization.
  size_t GetSerializedNativeModuleSize() const;

  // Serialize the {NativeModule} into the provided {buffer}. Returns true on
  // success and false if the given buffer it too small for serialization.
  bool SerializeNativeModule(base::Vector<uint8_t> buffer) const;

  // The data header consists of (see {WriteHeader}):
  // [0] magic number (uint32_t)
  // [1] version hash (uint32_t)
  // [2] supported CPU features (uint32_t)
  // [3] flag hash (uint32_t)
  // [4] enabled features (via flags and OT) (uint32_t)
  // [5] compile time imports (variable size)
  static constexpr size_t kVersionHashOffset = kUInt32Size;

 private:
  NativeModule* native_module_;
  // The {WasmCodeRefScope} keeps the pointers in {code_table_} alive.
  WasmCodeRefScope code_ref_scope_;
  std::vector<WasmCode*> code_table_;
  std::vector<WellKnownImport> import_statuses_;
};

// Deserializes the given data to create a Wasm module object.
// On successful deserialization, ownership of the `wire_bytes` vector is taken
// over by the deserialized module (the parameter will be reset to an empty
// vector); otherwise ownership stays with the caller.
V8_EXPORT_PRIVATE MaybeDirectHandle<WasmModuleObject> DeserializeNativeModule(
    Isolate*, WasmEnabledFeatures, base::Vector<const uint8_t> data,
    base::OwnedVector<const uint8_t>& wire_bytes,
    const CompileTimeImports& compile_imports,
    base::Vector<const char> source_url);

}  // namespace v8::internal::wasm

#endif  // V8_WASM_WASM_SERIALIZATION_H_
