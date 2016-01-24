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

  Register InputRegister(size_t index) {
    return ToRegister(instr_->InputAt(index));
  }

  DoubleRegister InputDoubleRegister(size_t index) {
    return ToDoubleRegister(instr_->InputAt(index));
  }

  double InputDouble(size_t index) { return ToDouble(instr_->InputAt(index)); }

  float InputFloat32(size_t index) { return ToFloat32(instr_->InputAt(index)); }

  int32_t InputInt32(size_t index) {
    return ToConstant(instr_->InputAt(index)).ToInt32();
  }

  int64_t InputInt64(size_t index) {
    return ToConstant(instr_->InputAt(index)).ToInt64();
  }

  int8_t InputInt8(size_t index) {
    return static_cast<int8_t>(InputInt32(index));
  }

  int16_t InputInt16(size_t index) {
    return static_cast<int16_t>(InputInt32(index));
  }

  uint8_t InputInt5(size_t index) {
    return static_cast<uint8_t>(InputInt32(index) & 0x1F);
  }

  uint8_t InputInt6(size_t index) {
    return static_cast<uint8_t>(InputInt32(index) & 0x3F);
  }

  ExternalReference InputExternalReference(size_t index) {
    return ToExternalReference(instr_->InputAt(index));
  }

  Handle<HeapObject> InputHeapObject(size_t index) {
    return ToHeapObject(instr_->InputAt(index));
  }

  Label* InputLabel(size_t index) { return ToLabel(instr_->InputAt(index)); }

  RpoNumber InputRpo(size_t index) {
    return ToRpoNumber(instr_->InputAt(index));
  }

  Register OutputRegister(size_t index = 0) {
    return ToRegister(instr_->OutputAt(index));
  }

  Register TempRegister(size_t index) {
    return ToRegister(instr_->TempAt(index));
  }

  DoubleRegister OutputDoubleRegister() {
    return ToDoubleRegister(instr_->Output());
  }

  // -- Conversions for operands -----------------------------------------------

  Label* ToLabel(InstructionOperand* op) {
    return gen_->GetLabel(ToRpoNumber(op));
  }

  RpoNumber ToRpoNumber(InstructionOperand* op) {
    return ToConstant(op).ToRpoNumber();
  }

  Register ToRegister(InstructionOperand* op) {
    return LocationOperand::cast(op)->GetRegister();
  }

  DoubleRegister ToDoubleRegister(InstructionOperand* op) {
    return LocationOperand::cast(op)->GetDoubleRegister();
  }

  Constant ToConstant(InstructionOperand* op) {
    if (op->IsImmediate()) {
      return gen_->code()->GetImmediate(ImmediateOperand::cast(op));
    }
    return gen_->code()->GetConstant(
        ConstantOperand::cast(op)->virtual_register());
  }

  double ToDouble(InstructionOperand* op) { return ToConstant(op).ToFloat64(); }

  float ToFloat32(InstructionOperand* op) { return ToConstant(op).ToFloat32(); }

  ExternalReference ToExternalReference(InstructionOperand* op) {
    return ToConstant(op).ToExternalReference();
  }

  Handle<HeapObject> ToHeapObject(InstructionOperand* op) {
    return ToConstant(op).ToHeapObject();
  }

  Frame* frame() const { return gen_->frame(); }
  FrameAccessState* frame_access_state() const {
    return gen_->frame_access_state();
  }
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
  Frame* frame() const { return frame_; }
  Isolate* isolate() const { return masm()->isolate(); }
  MacroAssembler* masm() const { return masm_; }
  OutOfLineCode* next() const { return next_; }

 private:
  Label entry_;
  Label exit_;
  Frame* const frame_;
  MacroAssembler* const masm_;
  OutOfLineCode* const next_;
};


// TODO(dcarney): generify this on bleeding_edge and replace this call
// when merged.
static inline void FinishCode(MacroAssembler* masm) {
#if V8_TARGET_ARCH_ARM64 || V8_TARGET_ARCH_ARM
  masm->CheckConstPool(true, false);
#elif V8_TARGET_ARCH_IA32 || V8_TARGET_ARCH_X64
  masm->ud2();
#endif
}

}  // namespace compiler
}  // namespace internal
}  // namespace v8

#endif  // V8_COMPILER_CODE_GENERATOR_IMPL_H
