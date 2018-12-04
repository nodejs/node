// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_WASM_WASM_SERIALIZATION_H_
#define V8_WASM_WASM_SERIALIZATION_H_

#include "src/wasm/wasm-objects.h"

namespace v8 {
namespace internal {
namespace wasm {

// Support for serializing WebAssembly {NativeModule} objects. This class takes
// a snapshot of the module state at instantiation, and other code that modifies
// the module after that won't affect the serialized result.
class WasmSerializer {
 public:
  WasmSerializer(Isolate* isolate, NativeModule* native_module);

  // Measure the required buffer size needed for serialization.
  size_t GetSerializedNativeModuleSize() const;

  // Serialize the {NativeModule} into the provided {buffer}. Returns true on
  // success and false if the given buffer it too small for serialization.
  bool SerializeNativeModule(Vector<byte> buffer) const;

 private:
  Isolate* isolate_;
  NativeModule* native_module_;
  std::vector<WasmCode*> code_table_;
};

// Support for deserializing WebAssembly {NativeModule} objects.
// Checks the version header of the data against the current version.
bool IsSupportedVersion(Isolate* isolate, Vector<const byte> data);

// Deserializes the given data to create a compiled Wasm module.
MaybeHandle<WasmModuleObject> DeserializeNativeModule(
    Isolate* isolate, Vector<const byte> data, Vector<const byte> wire_bytes);

}  // namespace wasm
}  // namespace internal
}  // namespace v8

#endif  // V8_WASM_WASM_SERIALIZATION_H_
