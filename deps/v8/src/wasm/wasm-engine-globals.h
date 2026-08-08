// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_WASM_WASM_ENGINE_GLOBALS_H_
#define V8_WASM_WASM_ENGINE_GLOBALS_H_

#if !V8_ENABLE_WEBASSEMBLY
#error This header should only be included if WebAssembly is enabled.
#endif  // !V8_ENABLE_WEBASSEMBLY

#include "src/base/macros.h"

namespace v8::internal::wasm {

class WasmEngine;
class WasmCodeManager;
class WasmImportWrapperCache;
class WasmStackEntryWrapperCache;
class CanonicalTypeNamesProvider;

// Returns a reference to the WasmEngine shared by the entire process.
V8_EXPORT_PRIVATE WasmEngine* GetWasmEngine();

// Returns a reference to the WasmCodeManager shared by the entire process.
V8_EXPORT_PRIVATE WasmCodeManager* GetWasmCodeManager();

// Returns a reference to the WasmImportWrapperCache shared by the entire
// process.
V8_EXPORT_PRIVATE WasmImportWrapperCache* GetWasmImportWrapperCache();
V8_EXPORT_PRIVATE WasmStackEntryWrapperCache* GetWasmStackEntryWrapperCache();

V8_EXPORT_PRIVATE CanonicalTypeNamesProvider* GetCanonicalTypeNamesProvider();

}  // namespace v8::internal::wasm

#endif  // V8_WASM_WASM_ENGINE_GLOBALS_H_
