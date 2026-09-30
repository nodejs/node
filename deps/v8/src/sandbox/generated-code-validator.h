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

class InstructionChecker;

class GeneratedCodeValidator {
 public:
  static V8_EXPORT_PRIVATE void Validate(Isolate* isolate, Tagged<Code> code);
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

  class ViolationsReporter {
   public:
    explicit ViolationsReporter(Tagged<Code> code);
    ~ViolationsReporter();

    void ReportDisassemblyFailed(const uint8_t* pc,
                                 size_t max_instruction_size);
    void ReportViolationWithInstruction(const uint8_t* pc, std::string instr,
                                        std::string error);
    void RecordDisassembledInstruction(const uint8_t* pc,
                                       size_t instruction_size,
                                       std::string instr);

   private:
    void ReportViolation(const uint8_t* pc, std::string error);
    void PrintPrologueIfNeeded();
    void PrintEpilogueIfNeeded();

    const Tagged<Code> code_;
    const uint8_t* const code_start_;
    std::stringstream disassembled_instructions_;
    bool violations_found_ = false;
  };

  class Utils {
   public:
    static bool IsEntryCode(const Tagged<Code> code);
    static bool IsDeoptCode(const Tagged<Code> code);
  };

  // TODO(523128533): Propagate `State` between instructions based on data flow.
  struct State {
    explicit State(const Tagged<Code> code);

    bool is_root_reg_valid_;
    // TODO(523128533): Check this field when checking memory writes to sandbox.
    bool is_cage_base_reg_valid_;
  };

  static void ValidateImpl(Isolate* isolate, Tagged<Code> code);

  friend class InstructionChecker;
};

}  // namespace v8::internal

#endif  // V8_ENABLE_GENERATED_CODE_VALIDATOR

#endif  // V8_SANDBOX_GENERATED_CODE_VALIDATOR_H_
