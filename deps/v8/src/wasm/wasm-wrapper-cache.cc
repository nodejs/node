// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/wasm/wasm-wrapper-cache.h"

#include "src/wasm/wasm-engine.h"
#include "src/wasm/wasm-import-wrapper-cache.h"
#include "src/wasm/wasm-stack-wrapper-cache.h"

namespace v8::internal::wasm {

void WasmWrapperHandle::set_code(WasmCode* code) {
#ifdef DEBUG
  if (code->kind() == WasmCode::Kind::kWasmStackEntryWrapper) {
    GetWasmStackEntryWrapperCache()->mutex_.AssertHeld();
  } else {
    GetWasmImportWrapperCache()->mutex_.AssertHeld();
  }
#endif
  // We're taking ownership of a WasmCode object that has just been allocated
  // and should have a refcount of 1.
  code->DcheckRefCountIsOne();
  DCHECK(!has_code());
  code_.store(code, std::memory_order_release);
}

}  //  namespace v8::internal::wasm
