// Copyright 2013 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_BACKEND_CODE_GENERATOR_IMPL_H_
#define V8_COMPILER_BACKEND_CODE_GENERATOR_IMPL_H_

#include "src/compiler/backend/code-generator.h"
#include "src/compiler/backend/instruction.h"
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

  FloatRegister InputFloatRegister(size_t index) {
    return ToFloatRegister(instr_->InputAt(index));
  }

  DoubleRegister InputDoubleRegister(size_t index) {
    return ToDoubleRegister(instr_->InputAt(index));
  }

  Simd128Register InputSimd128Register(size_t index) {
    return ToSimd128Register(instr_->InputAt(index));
  }

  double InputDouble(size_t index) { return ToDouble(instr_->InputAt(index)); }

  float InputFloat32(size_t index) { return ToFloat32(instr_->InputAt(index)); }

  int32_t InputInt32(size_t index) {
    return ToConstant(instr_->InputAt(index)).ToInt32();
  }

  uint32_t InputUint32(size_t index) {
    return bit_cast<uint32_t>(InputInt32(index));
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

  uint8_t InputInt3(size_t index) {
    return static_cast<uint8_t>(InputInt32(index) & 0x7);
  }

  uint8_t InputInt4(size_t index) {
    return static_cast<uint8_t>(InputInt32(index) & 0xF);
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

  Handle<Code> InputCode(size_t index) {
    return ToCode(instr_->InputAt(index));
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

  FloatRegister OutputFloatRegister() {
    return ToFloatRegister(instr_->Output());
  }

  DoubleRegister OutputDoubleRegister() {
    return ToDoubleRegister(instr_->Output());
  }

  Simd128Register OutputSimd128Register() {
    return ToSimd128Register(instr_->Output());
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

  FloatRegister ToFloatRegister(InstructionOperand* op) {
    return LocationOperand::cast(op)->GetFloatRegister();
  }

  DoubleRegister ToDoubleRegister(InstructionOperand* op) {
    return LocationOperand::cast(op)->GetDoubleRegister();
  }

  Simd128Register ToSimd128Register(InstructionOperand* op) {
    return LocationOperand::cast(op)->GetSimd128Register();
  }

  Constant ToConstant(InstructionOperand* op) {
    if (op->IsImmediate()) {
      return gen_->instructions()->GetImmediate(ImmediateOperand::cast(op));
    }
    return gen_->instructions()->GetConstant(
        ConstantOperand::cast(op)->virtual_register());
  }

  double ToDouble(InstructionOperand* op) {
    return ToConstant(op).ToFloat64().value();
  }

  float ToFloat32(InstructionOperand* op) { return ToConstant(op).ToFloat32(); }

  ExternalReference ToExternalReference(InstructionOperand* op) {
    return ToConstant(op).ToExternalReference();
  }

  Handle<Code> ToCode(InstructionOperand* op) {
    return ToConstant(op).ToCode();
  }

  const Frame* frame() const { return gen_->frame(); }
  FrameAccessState* frame_access_state() const {
    return gen_->frame_access_state();
  }
  Isolate* isolate() const { return gen_->isolate(); }
  Linkage* linkage() const { return gen_->linkage(); }

 protected:
  CodeGenerator* gen_;
  Instruction* instr_;
};

// Eager deoptimization exit.
class DeoptimizationExit : public ZoneObject {
 public:
  explicit DeoptimizationExit(int deoptimization_id, SourcePosition pos)
      : deoptimization_id_(deoptimization_id), pos_(pos) {}

  int deoptimization_id() const { return deoptimization_id_; }
  Label* label() { return &label_; }
  SourcePosition pos() const { return pos_; }

 private:
  int const deoptimization_id_;
  Label label_;
  SourcePosition const pos_;
};

// Generator for out-of-line code that is emitted after the main code is done.
class OutOfLineCode : public ZoneObject {
 public:
  explicit OutOfLineCode(CodeGenerator* gen);
  virtual ~OutOfLineCode();

  virtual void Generate() = 0;

  Label* entry() { return &entry_; }
  Label* exit() { return &exit_; }
  const Frame* frame() const { return frame_; }
  TurboAssembler* tasm() { return tasm_; }
  OutOfLineCode* next() const { return next_; }

 private:
  Label entry_;
  Label exit_;
  const Frame* const frame_;
  TurboAssembler* const tasm_;
  OutOfLineCode* const next_;
};

inline bool HasCallDescriptorFlag(Instruction* instr,
                                  CallDescriptor::Flag flag) {
  return MiscField::decode(instr->opcode()) & flag;
}

}  // namespace compiler
}  // namespace internal
}  // namespace v8

#endif  // V8_COMPILER_BACKEND_CODE_GENERATOR_IMPL_H_
