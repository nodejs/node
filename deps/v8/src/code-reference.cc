// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/code-reference.h"

#include "src/handles-inl.h"
#include "src/objects-inl.h"
#include "src/wasm/wasm-code-manager.h"

namespace v8 {
namespace internal {

Address CodeReference::constant_pool() const {
  return kind_ == JS ? js_code_->constant_pool() : wasm_code_->constant_pool();
}

Address CodeReference::instruction_start() const {
  return kind_ == JS
             ? js_code_->InstructionStart()
             : reinterpret_cast<Address>(wasm_code_->instructions().start());
}

Address CodeReference::instruction_end() const {
  return kind_ == JS
             ? js_code_->InstructionEnd()
             : reinterpret_cast<Address>(wasm_code_->instructions().start() +
                                         wasm_code_->instructions().size());
}

int CodeReference::instruction_size() const {
  return kind_ == JS ? js_code_->InstructionSize()
                     : wasm_code_->instructions().length();
}

const byte* CodeReference::relocation_start() const {
  return kind_ == JS ? js_code_->relocation_start()
                     : wasm_code_->reloc_info().start();
}

const byte* CodeReference::relocation_end() const {
  return kind_ == JS ? js_code_->relocation_end()
                     : wasm_code_->reloc_info().start() +
                           wasm_code_->reloc_info().length();
}

int CodeReference::relocation_size() const {
  return kind_ == JS ? js_code_->relocation_size()
                     : wasm_code_->reloc_info().length();
}

}  // namespace internal
}  // namespace v8
