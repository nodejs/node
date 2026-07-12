// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_WASM_COMPILATION_HINTS_GENERATION_H_
#define V8_WASM_COMPILATION_HINTS_GENERATION_H_

#if !V8_ENABLE_WEBASSEMBLY
#error This header should only be included if WebAssembly is enabled.
#endif  // !V8_ENABLE_WEBASSEMBLY

#include "src/base/macros.h"

namespace v8::internal::wasm {
class NativeModule;
class ZoneBuffer;

V8_EXPORT_PRIVATE void EmitCompilationHintsToBuffer(
    ZoneBuffer& buffer, NativeModule* native_module);
void WriteCompilationHintsToFile(ZoneBuffer& buffer,
                                 NativeModule* native_module);

}  // namespace v8::internal::wasm

#endif  // V8_WASM_COMPILATION_HINTS_GENERATION_H_
