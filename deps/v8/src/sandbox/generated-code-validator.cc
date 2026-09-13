// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/sandbox/generated-code-validator.h"

#ifdef V8_ENABLE_GENERATED_CODE_VALIDATOR

#include "src/objects/code-inl.h"
#include "src/sandbox/isolate-inl.h"

namespace v8::internal {

namespace {
bool ShouldValidateCode(Tagged<Code> code) {
  return v8_flags.validate_generated_code && code->has_instruction_stream();
}
}  // namespace

// static
void GeneratedCodeValidator::Validate(IsolateForSandbox isolate,
                                      Tagged<Code> code) {
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

}  // namespace v8::internal

#endif  // V8_ENABLE_GENERATED_CODE_VALIDATOR
