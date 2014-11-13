// Copyright 2013 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_CODE_GENERATOR_IMPL_H_
#define V8_COMPILER_CODE_GENERATOR_IMPL_H_

#include "src/code-stubs.h"
#include "src/compiler/code-generator.h"
#include "src/compiler/instruction.h"
#include "src/compiler/linkage.h"
#include "src/compiler/opcodes.h"
#include "src/macro-assembler.h"

namespace v8 {
namespace internal {
namespace compiler {

// Converts InstructionOperands from a given instruction to
// architecture-specific
// registers and operands after they have been assigned by the register
// allocator.
class InstructionOperandConverter {
 public:
  InstructionOperandConverter(CodeGenerator* gen, Instruction* instr)
      : gen_(gen), instr_(instr) {}

  Register InputRegister(int index) {
    return ToRegister(instr_->InputAt(index));
  }

  DoubleRegister InputDoubleRegister(int index) {
    return ToDoubleRegister(instr_->InputAt(index));
  }

  double InputDouble(int index) { return ToDouble(instr_->InputAt(index)); }

  int32_t InputInt32(int index) {
    return ToConstant(instr_->InputAt(index)).ToInt32();
  }

  int8_t InputInt8(int index) { return static_cast<int8_t>(InputInt32(index)); }

  int16_t InputInt16(int index) {
    return static_cast<int16_t>(InputInt32(index));
  }

  uint8_t InputInt5(int index) {
    return static_cast<uint8_t>(InputInt32(index) & 0x1F);
  }

  uint8_t InputInt6(int index) {
    return static_cast<uint8_t>(InputInt32(index) & 0x3F);
  }

  Handle<HeapObject> InputHeapObject(int index) {
    return ToHeapObject(instr_->InputAt(index));
  }

  Label* InputLabel(int index) { return gen_->GetLabel(InputRpo(index)); }

  BasicBlock::RpoNumber InputRpo(int index) {
    int rpo_number = InputInt32(index);
    return BasicBlock::RpoNumber::FromInt(rpo_number);
  }

  Register OutputRegister(int index = 0) {
    return ToRegister(instr_->OutputAt(index));
  }

  DoubleRegister OutputDoubleRegister() {
    return ToDoubleRegister(instr_->Output());
  }

  Register TempRegister(int index) { return ToRegister(instr_->TempAt(index)); }

  Register ToRegister(InstructionOperand* op) {
    DCHECK(op->IsRegister());
    return Register::FromAllocationIndex(op->index());
  }

  DoubleRegister ToDoubleRegister(InstructionOperand* op) {
    DCHECK(op->IsDoubleRegister());
    return DoubleRegister::FromAllocationIndex(op->index());
  }

  Constant ToConstant(InstructionOperand* operand) {
    if (operand->IsImmediate()) {
      return gen_->code()->GetImmediate(operand->index());
    }
    return gen_->code()->GetConstant(operand->index());
  }

  double ToDouble(InstructionOperand* operand) {
    return ToConstant(operand).ToFloat64();
  }

  Handle<HeapObject> ToHeapObject(InstructionOperand* operand) {
    return ToConstant(operand).ToHeapObject();
  }

  Frame* frame() const { return gen_->frame(); }
  Isolate* isolate() const { return gen_->isolate(); }
  Linkage* linkage() const { return gen_->linkage(); }

 protected:
  CodeGenerator* gen_;
  Instruction* instr_;
};


// TODO(dcarney): generify this on bleeding_edge and replace this call
// when merged.
static inline void FinishCode(MacroAssembler* masm) {
#if V8_TARGET_ARCH_ARM64 || V8_TARGET_ARCH_ARM
  masm->CheckConstPool(true, false);
#endif
}

}  // namespace compiler
}  // namespace internal
}  // namespace v8

#endif  // V8_COMPILER_CODE_GENERATOR_IMPL_H
