// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef V8_WASM_CODE_WRAPPER_H_
#define V8_WASM_CODE_WRAPPER_H_

#include "src/handles.h"

namespace v8 {
namespace internal {
namespace wasm {
class WasmCode;
}  // namespace wasm

class Code;
class WasmInstanceObject;

// TODO(mtrofin): remove once we remove FLAG_wasm_jit_to_native
class WasmCodeWrapper {
 public:
  WasmCodeWrapper() {}

  explicit WasmCodeWrapper(Handle<Code> code);
  explicit WasmCodeWrapper(const wasm::WasmCode* code);
  Handle<Code> GetCode() const;
  const wasm::WasmCode* GetWasmCode() const;
  bool is_null() const { return code_ptr_.wasm_code_ == nullptr; }
  bool IsCodeObject() const;
  bool is_liftoff() const;

  Vector<uint8_t> instructions() const;

  Handle<WasmInstanceObject> wasm_instance() const;

#ifdef ENABLE_DISASSEMBLER
  void Disassemble(const char* name, Isolate* isolate, std::ostream& os) const;
#endif

 private:
  union {
    const wasm::WasmCode* wasm_code_;
    Code** code_handle_;
  } code_ptr_ = {};
};

}  // namespace internal
}  // namespace v8
#endif  // V8_WASM_CODE_WRAPPER_H_
