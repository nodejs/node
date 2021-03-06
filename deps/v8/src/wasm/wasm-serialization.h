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
class V8_EXPORT_PRIVATE WasmSerializer {
 public:
  explicit WasmSerializer(NativeModule* native_module);

  // Measure the required buffer size needed for serialization.
  size_t GetSerializedNativeModuleSize() const;

  // Serialize the {NativeModule} into the provided {buffer}. Returns true on
  // success and false if the given buffer it too small for serialization.
  bool SerializeNativeModule(Vector<byte> buffer) const;

  // The data header consists of uint32_t-sized entries (see {WriteVersion}):
  // [0] magic number
  // [1] version hash
  // [2] supported CPU features
  // [3] flag hash
  // ...  number of functions
  // ... serialized functions
  static constexpr size_t kMagicNumberOffset = 0;
  static constexpr size_t kVersionHashOffset = kMagicNumberOffset + kUInt32Size;
  static constexpr size_t kSupportedCPUFeaturesOffset =
      kVersionHashOffset + kUInt32Size;
  static constexpr size_t kFlagHashOffset =
      kSupportedCPUFeaturesOffset + kUInt32Size;
  static constexpr size_t kHeaderSize = 4 * kUInt32Size;

 private:
  NativeModule* native_module_;
  std::vector<WasmCode*> code_table_;
};

// Support for deserializing WebAssembly {NativeModule} objects.
// Checks the version header of the data against the current version.
bool IsSupportedVersion(Vector<const byte> data);

// Deserializes the given data to create a Wasm module object.
V8_EXPORT_PRIVATE MaybeHandle<WasmModuleObject> DeserializeNativeModule(
    Isolate*, Vector<const byte> data, Vector<const byte> wire_bytes,
    Vector<const char> source_url);

}  // namespace wasm
}  // namespace internal
}  // namespace v8

#endif  // V8_WASM_WASM_SERIALIZATION_H_
