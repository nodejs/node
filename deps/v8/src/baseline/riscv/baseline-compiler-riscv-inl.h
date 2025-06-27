// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_BASELINE_RISCV_BASELINE_COMPILER_RISCV_INL_H_
#define V8_BASELINE_RISCV_BASELINE_COMPILER_RISCV_INL_H_

#include "src/baseline/baseline-compiler.h"

namespace v8 {
namespace internal {
namespace baseline {

// A builtin call/jump mode that is used then short builtin calls feature is
// not enabled.
constexpr BuiltinCallJumpMode kFallbackBuiltinCallJumpModeForBaseline =
    BuiltinCallJumpMode::kIndirect;

#define __ basm_.

void BaselineCompiler::Prologue() {
  ASM_CODE_COMMENT(&masm_);
  // Enter the frame here, since CallBuiltin will override lr.
  __ masm()->EnterFrame(StackFrame::BASELINE);
  DCHECK_EQ(kJSFunctionRegister, kJavaScriptCallTargetRegister);
  int max_frame_size = bytecode_->max_frame_size();
  CallBuiltin<Builtin::kBaselineOutOfLinePrologue>(
      kContextRegister, kJSFunctionRegister, kJavaScriptCallArgCountRegister,
      max_frame_size, kJavaScriptCallNewTargetRegister, bytecode_);
  PrologueFillFrame();
}

void BaselineCompiler::PrologueFillFrame() {
  ASM_CODE_COMMENT(&masm_);
  // Inlined register frame fill
  interpreter::Register new_target_or_generator_register =
      bytecode_->incoming_new_target_or_generator_register();
  __ LoadRoot(kInterpreterAccumulatorRegister, RootIndex::kUndefinedValue);
  int register_count = bytecode_->register_count();
  // Magic value
  const int kLoopUnrollSize = 8;
  const int new_target_index = new_target_or_generator_register.index();
  const bool has_new_target = new_target_index != kMaxInt;
  if (has_new_target) {
    DCHECK_LE(new_target_index, register_count);
    __ masm()->AddWord(sp, sp,
                       Operand(-(kSystemPointerSize * new_target_index)));
    for (int i = 0; i < new_target_index; i++) {
      __ masm()->StoreWord(kInterpreterAccumulatorRegister,
                           MemOperand(sp, i * kSystemPointerSize));
    }
    // Push new_target_or_generator.
    __ Push(kJavaScriptCallNewTargetRegister);
    register_count -= new_target_index + 1;
  }
  if (register_count < 2 * kLoopUnrollSize) {
    // If the frame is small enough, just unroll the frame fill completely.
    __ masm()->AddWord(sp, sp, Operand(-(kSystemPointerSize * register_count)));
    for (int i = 0; i < register_count; ++i) {
      __ masm()->StoreWord(kInterpreterAccumulatorRegister,
                           MemOperand(sp, i * kSystemPointerSize));
    }
  } else {
    __ masm()->AddWord(sp, sp, Operand(-(kSystemPointerSize * register_count)));
    for (int i = 0; i < register_count; ++i) {
      __ masm()->StoreWord(kInterpreterAccumulatorRegister,
                           MemOperand(sp, i * kSystemPointerSize));
    }
  }
}

void BaselineCompiler::VerifyFrameSize() {
  ASM_CODE_COMMENT(&masm_);
  __ masm()->AddWord(t0, sp,
                     Operand(InterpreterFrameConstants::kFixedFrameSizeFromFp +
                             bytecode_->frame_size()));
  __ masm()->Assert(eq, AbortReason::kUnexpectedStackPointer, t0, Operand(fp));
}

#undef __

}  // namespace baseline
}  // namespace internal
}  // namespace v8

#endif  // V8_BASELINE_RISCV_BASELINE_COMPILER_RISCV_INL_H_
