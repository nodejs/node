// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/wasm/wasm-code-wrapper.h"

#include "src/objects-inl.h"
#include "src/objects/code.h"
#include "src/wasm/wasm-code-manager.h"
#include "src/wasm/wasm-objects.h"

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

#ifdef ENABLE_DISASSEMBLER
void WasmCodeWrapper::Disassemble(const char* name, Isolate* isolate,
                                  std::ostream& os) const {
  if (IsCodeObject()) {
    GetCode()->Disassemble(name, os);
  } else {
    GetWasmCode()->Disassemble(name, isolate, os);
  }
}
#endif

bool WasmCodeWrapper::is_liftoff() const {
  return IsCodeObject() ? !GetCode()->is_turbofanned()
                        : GetWasmCode()->is_liftoff();
}

Vector<uint8_t> WasmCodeWrapper::instructions() const {
  if (!IsCodeObject()) return GetWasmCode()->instructions();
  Handle<Code> code = GetCode();
  return {code->instruction_start(),
          static_cast<size_t>(code->instruction_size())};
}

Handle<WasmInstanceObject> WasmCodeWrapper::wasm_instance() const {
  return IsCodeObject()
             ? handle(WasmInstanceObject::GetOwningInstanceGC(*GetCode()))
             : handle(WasmInstanceObject::GetOwningInstance(GetWasmCode()));
}

}  // namespace internal
}  // namespace v8
