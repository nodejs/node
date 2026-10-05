// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/sandbox/generated-code-validator.h"

#ifdef V8_ENABLE_GENERATED_CODE_VALIDATOR

#include <algorithm>

#include "src/objects/code-inl.h"
#include "src/objects/code-kind.h"
#include "src/sandbox/isolate-inl.h"

namespace v8::internal {

namespace {
bool ShouldValidateCode(Tagged<Code> code) {
  return v8_flags.validate_generated_code && code->has_instruction_stream();
}
}  // namespace

// static
void GeneratedCodeValidator::Validate(Isolate* isolate, Tagged<Code> code) {
  if (!ShouldValidateCode(code)) {
    return;
  }

  DCHECK(!IsValidated(code));

  ValidateImpl(isolate, code);

  code->instruction_stream()->SetValidated();
}

// static
bool GeneratedCodeValidator::IsValidated(Tagged<Code> code) {
  if (!ShouldValidateCode(code)) {
    return true;
  }
  return code->instruction_stream()->IsValidated();
}

GeneratedCodeValidator::InstructionIteratorSkippingData::
    InstructionIteratorSkippingData(Tagged<Code> code)
    : start_(reinterpret_cast<const uint8_t*>(code->instruction_start())),
      end_(start_ + code->instruction_size()),
      current_(start_),
      disasm_(converter_) {
  static_assert(V8_JUMP_TABLE_INFO_BOOL);
  if (code->has_jump_table_info()) {
    jump_table_info_it_.emplace(code->jump_table_info(),
                                code->jump_table_info_size());
  }

  if (code->has_instruction_stream()) {
    reloc_it_.emplace(code, 1 << RelocInfo::INTERNAL_REFERENCE);
  }

  SkipCheck();
}

void GeneratedCodeValidator::InstructionIteratorSkippingData::Advance(
    int instruction_size) {
  DCHECK(!IsDone());
  DCHECK_GT(instruction_size, 0);
  current_ += instruction_size;
  SkipCheck();
}

void GeneratedCodeValidator::InstructionIteratorSkippingData::SkipCheck() {
  while (!IsDone()) {
    if (jump_table_info_it_) {
      // TODO(523128533): Validate that the jump table only refers to
      // successfully disassembled instructions.
      const uint32_t offset = static_cast<uint32_t>(current_ - start_);
      while (jump_table_info_it_->HasCurrent() &&
             (jump_table_info_it_->GetPCOffset() < offset)) {
        jump_table_info_it_->Next();
      }
      if (jump_table_info_it_->HasCurrent() &&
          (jump_table_info_it_->GetPCOffset() == offset)) {
        current_ += JumpTableInfoEntry::kTargetSize;
        jump_table_info_it_->Next();
        continue;
      }
    }

    if (reloc_it_) {
      // TODO(523128533): Consider validating that embedded references actually
      // point to valid objects.
      Address current_addr = reinterpret_cast<Address>(current_);
      while (!reloc_it_->done() && (reloc_it_->rinfo()->pc() < current_addr)) {
        reloc_it_->next();
      }
      if (!reloc_it_->done() && (reloc_it_->rinfo()->pc() == current_addr)) {
        current_ += kSystemPointerSize;
        reloc_it_->next();
        continue;
      }
    }

    int pool_size = disasm_.ConstantPoolSizeAt(const_cast<uint8_t*>(current_));
    if (pool_size > 0) {
      static constexpr int kConstantSize = 4;
      current_ +=
          kConstantSize *
          (pool_size + 1);  // +1 to account for the marker or fence instruction
      continue;
    }

    // Found an instruction that should not be skipped.
    break;
  }
}

GeneratedCodeValidator::ViolationsReporter::ViolationsReporter(
    Tagged<Code> code)
    : code_(code),
      code_start_(reinterpret_cast<const uint8_t*>(code->instruction_start())) {
}

GeneratedCodeValidator::ViolationsReporter::~ViolationsReporter() {
  PrintEpilogueIfNeeded();
}

void GeneratedCodeValidator::ViolationsReporter::ReportDisassemblyFailed(
    const uint8_t* pc, size_t max_instruction_size) {
  if (v8_flags.validate_generated_code_include_code) {
    RecordDisassembledInstruction(pc, max_instruction_size, "???");
  }
  ReportViolation(pc, "Failed to disassemble invalid instruction.");
}

void GeneratedCodeValidator::ViolationsReporter::ReportViolationWithInstruction(
    const uint8_t* pc, std::string instr, std::string error) {
  ReportViolation(pc, error + ": " + instr);
}

void GeneratedCodeValidator::ViolationsReporter::RecordDisassembledInstruction(
    const uint8_t* pc, size_t instruction_size, std::string instr) {
  DCHECK(v8_flags.validate_generated_code_include_code);
  DCHECK_GT(instruction_size, 0);
  // Print offset in the instruction stream.
  disassembled_instructions_ << "\t" << AsHex(pc - code_start_, 8, true)
                             << "\t";
  // Print raw bytes (with padding if needed).
  for (size_t i = 0; i < instruction_size; ++i) {
    disassembled_instructions_ << AsHex(pc[i], 2, false);
  }
  static constexpr size_t kMaxInstructionSizeForPadding = 8;
  static constexpr char kInstructionPadding[] =
      "              ";  // 14 whitespaces
  if (kMaxInstructionSizeForPadding > instruction_size) {
    disassembled_instructions_.write(
        kInstructionPadding,
        (kMaxInstructionSizeForPadding - instruction_size) * 2);
  }
  // Print disassembled instruction.
  disassembled_instructions_ << "\t" << instr << "\n";
}

namespace {

static constexpr char kGeneratedCodeValidatorTag[] =
    "[Generated code validator]";

std::string GetCodeName(Tagged<Code> code) {
  if (code->is_builtin()) {
    return Builtins::name(code->builtin_id());
  }
  if (code->uses_deoptimization_data()) {
    Tagged<DeoptimizationData> data = code->deoptimization_data();
    if (data->length().value() > 0) {
      std::unique_ptr<char[]> debug_name =
          data->GetSharedFunctionInfo()->DebugNameCStr();
      std::string name(debug_name.get());
      for (char& c : name) {
        if (!std::isalnum(static_cast<unsigned char>(c)) && c != '_') {
          c = '_';
        }
      }
      return name;
    }
  }
  return "unknown method";
}

}  // namespace

void GeneratedCodeValidator::ViolationsReporter::ReportViolation(
    const uint8_t* pc, std::string error) {
  PrintPrologueIfNeeded();
  v8::base::OS::PrintError("%s Offset %.8zx: %s\n", kGeneratedCodeValidatorTag,
                           pc - code_start_, error.c_str());
}

void GeneratedCodeValidator::ViolationsReporter::PrintPrologueIfNeeded() {
  if (violations_found_) {
    // Prologue have already been printed.
    return;
  }
  v8::base::OS::PrintError("%s Violations found for %s compiled at tier %s:\n",
                           kGeneratedCodeValidatorTag,
                           GetCodeName(code_).c_str(),
                           CodeKindToString(code_->kind()));
  violations_found_ = true;
}

void GeneratedCodeValidator::ViolationsReporter::PrintEpilogueIfNeeded() {
  if (!violations_found_) {
    return;
  }

  DCHECK_IMPLIES(!v8_flags.validate_generated_code_include_code,
                 disassembled_instructions_.str().empty());
  if (v8_flags.validate_generated_code_include_code) {
    v8::base::OS::PrintError("%s Disassembled code is:\n%s",
                             kGeneratedCodeValidatorTag,
                             disassembled_instructions_.str().c_str());
  }

  if (v8_flags.validate_generated_code_non_fatal) {
    return;
  }
  FATAL("Generated code validation failed. See stderr output for violations.");
}

// static
bool GeneratedCodeValidator::Utils::IsEntryCode(const Tagged<Code> code) {
  switch (code->kind()) {
#if V8_ENABLE_WEBASSEMBLY
    case CodeKind::C_WASM_ENTRY:
#endif  // V8_ENABLE_WEBASSEMBLY
    case CodeKind::FOR_TESTING:
      return true;
    case CodeKind::BUILTIN:
      DCHECK(code->is_builtin());
      static constexpr Builtin entry_builtins[] = {
          Builtin::kJSEntry, Builtin::kJSConstructEntry,
          Builtin::kJSRunMicrotasksEntry};
      return std::find(std::begin(entry_builtins), std::end(entry_builtins),
                       code->builtin_id()) != std::end(entry_builtins);
    default:
      return false;
  }
}

// static
bool GeneratedCodeValidator::Utils::IsDeoptCode(const Tagged<Code> code) {
  static constexpr Builtin entry_builtins[] = {
      Builtin::kDeoptimizationEntry_Eager, Builtin::kDeoptimizationEntry_Lazy,
      Builtin::kDeoptimizationEntry_LazyAfterFastCall};
  return code->is_builtin() &&
         std::find(std::begin(entry_builtins), std::end(entry_builtins),
                   code->builtin_id()) != std::end(entry_builtins);
}

GeneratedCodeValidator::State::State(const Tagged<Code> code)
    : is_root_reg_valid_(!Utils::IsEntryCode(code)),
      is_cage_base_reg_valid_(!Utils::IsEntryCode(code)) {}

}  // namespace v8::internal

#endif  // V8_ENABLE_GENERATED_CODE_VALIDATOR
