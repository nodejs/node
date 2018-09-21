// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_CODE_REFERENCE_H_
#define V8_CODE_REFERENCE_H_

#include "src/handles.h"
#include "src/objects/code.h"

namespace v8 {
namespace internal {

class Code;

namespace wasm {
class WasmCode;
}

class CodeReference {
 public:
  CodeReference() : kind_(JS), js_code_() {}
  explicit CodeReference(const wasm::WasmCode* wasm_code)
      : kind_(WASM), wasm_code_(wasm_code) {}
  explicit CodeReference(Handle<Code> js_code) : kind_(JS), js_code_(js_code) {}

  Address constant_pool() const;
  Address instruction_start() const;
  Address instruction_end() const;
  int instruction_size() const;
  const byte* relocation_start() const;
  const byte* relocation_end() const;
  int relocation_size() const;
  bool is_null() const {
    return kind_ == JS ? js_code_.is_null() : wasm_code_ == nullptr;
  }

  Handle<Code> as_js_code() const {
    DCHECK_EQ(JS, kind_);
    return js_code_;
  }

  const wasm::WasmCode* as_wasm_code() const {
    DCHECK_EQ(WASM, kind_);
    return wasm_code_;
  }

 private:
  enum { JS, WASM } kind_;
  union {
    const wasm::WasmCode* wasm_code_;
    Handle<Code> js_code_;
  };

  DISALLOW_NEW_AND_DELETE();
};
ASSERT_TRIVIALLY_COPYABLE(CodeReference);

}  // namespace internal
}  // namespace v8

#endif  // V8_CODE_REFERENCE_H_
