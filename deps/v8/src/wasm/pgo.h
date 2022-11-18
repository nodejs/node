// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if !V8_ENABLE_WEBASSEMBLY
#error This header should only be included if WebAssembly is enabled.
#endif  // !V8_ENABLE_WEBASSEMBLY

#ifndef V8_WASM_PGO_H_
#define V8_WASM_PGO_H_

#include "src/base/vector.h"

namespace v8::internal::wasm {

struct WasmModule;

void DumpProfileToFile(const WasmModule* module,
                       base::Vector<const uint8_t> wire_bytes);

void LoadProfileFromFile(WasmModule* module,
                         base::Vector<const uint8_t> wire_bytes);

}  // namespace v8::internal::wasm

#endif  // V8_WASM_PGO_H_
