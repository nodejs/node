// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_WASM_WASM_SERIALIZATION_H_
#define V8_WASM_WASM_SERIALIZATION_H_

#include "src/wasm/wasm-objects.h"

namespace v8 {
namespace internal {
namespace wasm {

// Support to serialize WebAssembly {NativeModule} objects. This class intends
// to be thread-safe in that it takes a consistent snapshot of the module state
// at instantiation, allowing other threads to mutate the module concurrently.
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

// Support to deserialize WebAssembly {NativeModule} objects.
MaybeHandle<WasmModuleObject> DeserializeNativeModule(
    Isolate* isolate, Vector<const byte> data, Vector<const byte> wire_bytes);

}  // namespace wasm
}  // namespace internal
}  // namespace v8

#endif  // V8_WASM_WASM_SERIALIZATION_H_
