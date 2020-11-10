// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_CODEGEN_CODE_REFERENCE_H_
#define V8_CODEGEN_CODE_REFERENCE_H_

#include "src/handles/handles.h"
#include "src/objects/code.h"

namespace v8 {
namespace internal {

class Code;
class CodeDesc;

namespace wasm {
class WasmCode;
}

class CodeReference {
 public:
  CodeReference() : kind_(NONE), null_(nullptr) {}
  explicit CodeReference(const wasm::WasmCode* wasm_code)
      : kind_(WASM), wasm_code_(wasm_code) {}
  explicit CodeReference(const CodeDesc* code_desc)
      : kind_(CODE_DESC), code_desc_(code_desc) {}
  explicit CodeReference(Handle<Code> js_code) : kind_(JS), js_code_(js_code) {}

  Address constant_pool() const;
  Address instruction_start() const;
  Address instruction_end() const;
  int instruction_size() const;
  const byte* relocation_start() const;
  const byte* relocation_end() const;
  int relocation_size() const;
  Address code_comments() const;
  int code_comments_size() const;

  bool is_null() const { return kind_ == NONE; }
  bool is_js() const { return kind_ == JS; }
  bool is_wasm_code() const { return kind_ == WASM; }

  Handle<Code> as_js_code() const {
    DCHECK_EQ(JS, kind_);
    return js_code_;
  }

  const wasm::WasmCode* as_wasm_code() const {
    DCHECK_EQ(WASM, kind_);
    return wasm_code_;
  }

 private:
  enum { NONE, JS, WASM, CODE_DESC } kind_;
  union {
    std::nullptr_t null_;
    const wasm::WasmCode* wasm_code_;
    const CodeDesc* code_desc_;
    Handle<Code> js_code_;
  };

  DISALLOW_NEW_AND_DELETE()
};
ASSERT_TRIVIALLY_COPYABLE(CodeReference);

}  // namespace internal
}  // namespace v8

#endif  // V8_CODEGEN_CODE_REFERENCE_H_
