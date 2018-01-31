// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/wasm/wasm-code-wrapper.h"

#include "src/objects.h"
#include "src/objects/code.h"

namespace v8 {
namespace internal {

// When constructing, we check the flag. After that, we just
// check using the member.
WasmCodeWrapper::WasmCodeWrapper(Handle<Code> code) {
  DCHECK(!FLAG_wasm_jit_to_native);
  code_ptr_.code_handle_ = code.location();
}

WasmCodeWrapper::WasmCodeWrapper(const wasm::WasmCode* code) {
  DCHECK(FLAG_wasm_jit_to_native);
  code_ptr_.wasm_code_ = code;
}

Handle<Code> WasmCodeWrapper::GetCode() const {
  DCHECK(IsCodeObject());
  return Handle<Code>(code_ptr_.code_handle_);
}

const wasm::WasmCode* WasmCodeWrapper::GetWasmCode() const {
  DCHECK(!IsCodeObject());
  return code_ptr_.wasm_code_;
}

bool WasmCodeWrapper::IsCodeObject() const { return !FLAG_wasm_jit_to_native; }

}  // namespace internal
}  // namespace v8
