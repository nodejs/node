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

  // -- Instruction operand accesses with conversions --------------------------

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

  Label* InputLabel(int index) { return ToLabel(instr_->InputAt(index)); }

  BasicBlock::RpoNumber InputRpo(int index) {
    return ToRpoNumber(instr_->InputAt(index));
  }

  Register OutputRegister(int index = 0) {
    return ToRegister(instr_->OutputAt(index));
  }

  Register TempRegister(int index) { return ToRegister(instr_->TempAt(index)); }

  DoubleRegister OutputDoubleRegister() {
    return ToDoubleRegister(instr_->Output());
  }

  // -- Conversions for operands -----------------------------------------------

  Label* ToLabel(InstructionOperand* op) {
    return gen_->GetLabel(ToRpoNumber(op));
  }

  BasicBlock::RpoNumber ToRpoNumber(InstructionOperand* op) {
    return ToConstant(op).ToRpoNumber();
  }

  Register ToRegister(InstructionOperand* op) {
    DCHECK(op->IsRegister());
    return Register::FromAllocationIndex(op->index());
  }

  DoubleRegister ToDoubleRegister(InstructionOperand* op) {
    DCHECK(op->IsDoubleRegister());
    return DoubleRegister::FromAllocationIndex(op->index());
  }

  Constant ToConstant(InstructionOperand* op) {
    if (op->IsImmediate()) {
      return gen_->code()->GetImmediate(op->index());
    }
    return gen_->code()->GetConstant(op->index());
  }

  double ToDouble(InstructionOperand* op) { return ToConstant(op).ToFloat64(); }

  Handle<HeapObject> ToHeapObject(InstructionOperand* op) {
    return ToConstant(op).ToHeapObject();
  }

  Frame* frame() const { return gen_->frame(); }
  Isolate* isolate() const { return gen_->isolate(); }
  Linkage* linkage() const { return gen_->linkage(); }

 protected:
  CodeGenerator* gen_;
  Instruction* instr_;
};


// Generator for out-of-line code that is emitted after the main code is done.
class OutOfLineCode : public ZoneObject {
 public:
  explicit OutOfLineCode(CodeGenerator* gen);
  virtual ~OutOfLineCode();

  virtual void Generate() = 0;

  Label* entry() { return &entry_; }
  Label* exit() { return &exit_; }
  MacroAssembler* masm() const { return masm_; }
  OutOfLineCode* next() const { return next_; }

 private:
  Label entry_;
  Label exit_;
  MacroAssembler* const masm_;
  OutOfLineCode* const next_;
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
