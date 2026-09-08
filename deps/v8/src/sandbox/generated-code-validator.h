// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_SANDBOX_GENERATED_CODE_VALIDATOR_H_
#define V8_SANDBOX_GENERATED_CODE_VALIDATOR_H_

#ifdef V8_ENABLE_GENERATED_CODE_VALIDATOR

#include <optional>

#include "src/codegen/jump-table-info.h"
#include "src/codegen/reloc-info.h"
#include "src/diagnostics/disasm.h"
#include "src/objects/code.h"

namespace v8::internal {

class GeneratedCodeValidator {
 public:
  static V8_EXPORT_PRIVATE void Validate(IsolateForSandbox isolate,
                                         Tagged<Code> code);
  static V8_EXPORT_PRIVATE bool IsValidated(Tagged<Code> code);

 private:
  // Helper class to iterate over the instructions of a Code object while
  // skipping inline data payload such as constant pools, jump tables, and
  // internal references.
  class InstructionIteratorSkippingData {
   public:
    explicit InstructionIteratorSkippingData(Tagged<Code> code);

    bool IsDone() const { return current_ >= end_; }
    const uint8_t* GetCurrent() const {
      DCHECK(!IsDone());
      return current_;
    }
    // Advances the iterator by `instruction_size` bytes, and then checks if
    // the new position points to data that needs to be skipped.
    void Advance(int instruction_size);

   private:
    // Checks if the current position points to data (jump table, reloc info,
    // constant pool) and if so, advances `current_` past it. Loops until
    // `current_` points to an instruction or `end_`.
    void SkipCheck();

    const uint8_t* const start_;
    const uint8_t* const end_;
    const uint8_t* current_;

    std::optional<JumpTableInfoIterator> jump_table_info_it_;
    std::optional<RelocIterator> reloc_it_;
    disasm::NameConverter converter_;
    disasm::Disassembler disasm_;
  };

  static void ValidateImpl(IsolateForSandbox isolate, Tagged<Code> code);
};

}  // namespace v8::internal

#endif  // V8_ENABLE_GENERATED_CODE_VALIDATOR

#endif  // V8_SANDBOX_GENERATED_CODE_VALIDATOR_H_
