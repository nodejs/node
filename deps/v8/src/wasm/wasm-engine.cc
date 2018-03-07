// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/wasm/wasm-engine.h"
#include "src/objects-inl.h"
#include "src/wasm/module-compiler.h"

namespace v8 {
namespace internal {
namespace wasm {

bool WasmEngine::SyncValidate(Isolate* isolate, const ModuleWireBytes& bytes) {
  // TODO(titzer): remove dependency on the isolate.
  if (bytes.start() == nullptr || bytes.length() == 0) return false;
  ModuleResult result = SyncDecodeWasmModule(isolate, bytes.start(),
                                             bytes.end(), true, kWasmOrigin);
  return result.ok();
}

}  // namespace wasm
}  // namespace internal
}  // namespace v8
