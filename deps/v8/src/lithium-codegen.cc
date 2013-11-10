// Copyright 2013 the V8 project authors. All rights reserved.
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
//       copyright notice, this list of conditions and the following
//       disclaimer in the documentation and/or other materials provided
//       with the distribution.
//     * Neither the name of Google Inc. nor the names of its
//       contributors may be used to endorse or promote products derived
//       from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include "v8.h"

#include "lithium-codegen.h"

#if V8_TARGET_ARCH_IA32
#include "ia32/lithium-ia32.h"
#include "ia32/lithium-codegen-ia32.h"
#elif V8_TARGET_ARCH_X64
#include "x64/lithium-x64.h"
#include "x64/lithium-codegen-x64.h"
#elif V8_TARGET_ARCH_ARM
#include "arm/lithium-arm.h"
#include "arm/lithium-codegen-arm.h"
#elif V8_TARGET_ARCH_MIPS
#include "mips/lithium-mips.h"
#include "mips/lithium-codegen-mips.h"
#else
#error Unsupported target architecture.
#endif

namespace v8 {
namespace internal {


HGraph* LCodeGenBase::graph() const {
  return chunk()->graph();
}


LCodeGenBase::LCodeGenBase(LChunk* chunk,
                           MacroAssembler* assembler,
                           CompilationInfo* info)
    : chunk_(static_cast<LPlatformChunk*>(chunk)),
      masm_(assembler),
      info_(info),
      zone_(info->zone()),
      status_(UNUSED),
      current_block_(-1),
      current_instruction_(-1),
      instructions_(chunk->instructions()),
      last_lazy_deopt_pc_(0) {
}


bool LCodeGenBase::GenerateBody() {
  ASSERT(is_generating());
  bool emit_instructions = true;
  LCodeGen* codegen = static_cast<LCodeGen*>(this);
  for (current_instruction_ = 0;
       !is_aborted() && current_instruction_ < instructions_->length();
       current_instruction_++) {
    LInstruction* instr = instructions_->at(current_instruction_);

    // Don't emit code for basic blocks with a replacement.
    if (instr->IsLabel()) {
      emit_instructions = !LLabel::cast(instr)->HasReplacement() &&
          (!FLAG_unreachable_code_elimination ||
           instr->hydrogen_value()->block()->IsReachable());
      if (FLAG_code_comments && !emit_instructions) {
        Comment(
            ";;; <@%d,#%d> -------------------- B%d (unreachable/replaced) "
            "--------------------",
            current_instruction_,
            instr->hydrogen_value()->id(),
            instr->hydrogen_value()->block()->block_id());
      }
    }
    if (!emit_instructions) continue;

    if (FLAG_code_comments && instr->HasInterestingComment(codegen)) {
      Comment(";;; <@%d,#%d> %s",
              current_instruction_,
              instr->hydrogen_value()->id(),
              instr->Mnemonic());
    }

    GenerateBodyInstructionPre(instr);

    HValue* value = instr->hydrogen_value();
    if (value->position() != RelocInfo::kNoPosition) {
      ASSERT(!graph()->info()->IsOptimizing() ||
             !FLAG_emit_opt_code_positions ||
             value->position() != RelocInfo::kNoPosition);
      RecordAndWritePosition(value->position());
    }

    instr->CompileToNative(codegen);

    GenerateBodyInstructionPost(instr);
  }
  EnsureSpaceForLazyDeopt(Deoptimizer::patch_size());
  last_lazy_deopt_pc_ = masm()->pc_offset();
  return !is_aborted();
}


void LCodeGenBase::Comment(const char* format, ...) {
  if (!FLAG_code_comments) return;
  char buffer[4 * KB];
  StringBuilder builder(buffer, ARRAY_SIZE(buffer));
  va_list arguments;
  va_start(arguments, format);
  builder.AddFormattedList(format, arguments);
  va_end(arguments);

  // Copy the string before recording it in the assembler to avoid
  // issues when the stack allocated buffer goes out of scope.
  size_t length = builder.position();
  Vector<char> copy = Vector<char>::New(static_cast<int>(length) + 1);
  OS::MemCopy(copy.start(), builder.Finalize(), copy.length());
  masm()->RecordComment(copy.start());
}


int LCodeGenBase::GetNextEmittedBlock() const {
  for (int i = current_block_ + 1; i < graph()->blocks()->length(); ++i) {
    if (!chunk_->GetLabel(i)->HasReplacement()) return i;
  }
  return -1;
}


} }  // namespace v8::internal
