// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_WASM_WASM_SERIALIZATION_H_
#define V8_WASM_WASM_SERIALIZATION_H_

#include "src/wasm/wasm-objects.h"

namespace v8 {
namespace internal {
namespace wasm {

size_t GetSerializedNativeModuleSize(
    Isolate* isolate, Handle<WasmCompiledModule> compiled_module);

bool SerializeNativeModule(Isolate* isolate,
                           Handle<WasmCompiledModule> compiled_module,
                           Vector<byte> buffer);

MaybeHandle<WasmModuleObject> DeserializeNativeModule(
    Isolate* isolate, Vector<const byte> data, Vector<const byte> wire_bytes);

}  // namespace wasm
}  // namespace internal
}  // namespace v8

#endif  // V8_WASM_WASM_SERIALIZATION_H_
