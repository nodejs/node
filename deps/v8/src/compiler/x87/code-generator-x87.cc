// Copyright 2013 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/code-generator.h"

#include "src/compiler/code-generator-impl.h"
#include "src/compiler/gap-resolver.h"
#include "src/compiler/node-matchers.h"
#include "src/scopes.h"
#include "src/x87/assembler-x87.h"
#include "src/x87/macro-assembler-x87.h"

namespace v8 {
namespace internal {
namespace compiler {

#define __ masm()->


// Adds X87 specific methods for decoding operands.
class X87OperandConverter : public InstructionOperandConverter {
 public:
  X87OperandConverter(CodeGenerator* gen, Instruction* instr)
      : InstructionOperandConverter(gen, instr) {}

  Operand InputOperand(size_t index, int extra = 0) {
    return ToOperand(instr_->InputAt(index), extra);
  }

  Immediate InputImmediate(size_t index) {
    return ToImmediate(instr_->InputAt(index));
  }

  Operand OutputOperand() { return ToOperand(instr_->Output()); }

  Operand ToOperand(InstructionOperand* op, int extra = 0) {
    if (op->IsRegister()) {
      DCHECK(extra == 0);
      return Operand(ToRegister(op));
    } else if (op->IsDoubleRegister()) {
      DCHECK(extra == 0);
      UNIMPLEMENTED();
    }
    DCHECK(op->IsStackSlot() || op->IsDoubleStackSlot());
    // The linkage computes where all spill slots are located.
    FrameOffset offset = linkage()->GetFrameOffset(
        AllocatedOperand::cast(op)->index(), frame(), extra);
    return Operand(offset.from_stack_pointer() ? esp : ebp, offset.offset());
  }

  Operand HighOperand(InstructionOperand* op) {
    DCHECK(op->IsDoubleStackSlot());
    return ToOperand(op, kPointerSize);
  }

  Immediate ToImmediate(InstructionOperand* operand) {
    Constant constant = ToConstant(operand);
    switch (constant.type()) {
      case Constant::kInt32:
        return Immediate(constant.ToInt32());
      case Constant::kFloat32:
        return Immediate(
            isolate()->factory()->NewNumber(constant.ToFloat32(), TENURED));
      case Constant::kFloat64:
        return Immediate(
            isolate()->factory()->NewNumber(constant.ToFloat64(), TENURED));
      case Constant::kExternalReference:
        return Immediate(constant.ToExternalReference());
      case Constant::kHeapObject:
        return Immediate(constant.ToHeapObject());
      case Constant::kInt64:
        break;
      case Constant::kRpoNumber:
        return Immediate::CodeRelativeOffset(ToLabel(operand));
    }
    UNREACHABLE();
    return Immediate(-1);
  }

  static size_t NextOffset(size_t* offset) {
    size_t i = *offset;
    (*offset)++;
    return i;
  }

  static ScaleFactor ScaleFor(AddressingMode one, AddressingMode mode) {
    STATIC_ASSERT(0 == static_cast<int>(times_1));
    STATIC_ASSERT(1 == static_cast<int>(times_2));
    STATIC_ASSERT(2 == static_cast<int>(times_4));
    STATIC_ASSERT(3 == static_cast<int>(times_8));
    int scale = static_cast<int>(mode - one);
    DCHECK(scale >= 0 && scale < 4);
    return static_cast<ScaleFactor>(scale);
  }

  Operand MemoryOperand(size_t* offset) {
    AddressingMode mode = AddressingModeField::decode(instr_->opcode());
    switch (mode) {
      case kMode_MR: {
        Register base = InputRegister(NextOffset(offset));
        int32_t disp = 0;
        return Operand(base, disp);
      }
      case kMode_MRI: {
        Register base = InputRegister(NextOffset(offset));
        int32_t disp = InputInt32(NextOffset(offset));
        return Operand(base, disp);
      }
      case kMode_MR1:
      case kMode_MR2:
      case kMode_MR4:
      case kMode_MR8: {
        Register base = InputRegister(NextOffset(offset));
        Register index = InputRegister(NextOffset(offset));
        ScaleFactor scale = ScaleFor(kMode_MR1, mode);
        int32_t disp = 0;
        return Operand(base, index, scale, disp);
      }
      case kMode_MR1I:
      case kMode_MR2I:
      case kMode_MR4I:
      case kMode_MR8I: {
        Register base = InputRegister(NextOffset(offset));
        Register index = InputRegister(NextOffset(offset));
        ScaleFactor scale = ScaleFor(kMode_MR1I, mode);
        int32_t disp = InputInt32(NextOffset(offset));
        return Operand(base, index, scale, disp);
      }
      case kMode_M1:
      case kMode_M2:
      case kMode_M4:
      case kMode_M8: {
        Register index = InputRegister(NextOffset(offset));
        ScaleFactor scale = ScaleFor(kMode_M1, mode);
        int32_t disp = 0;
        return Operand(index, scale, disp);
      }
      case kMode_M1I:
      case kMode_M2I:
      case kMode_M4I:
      case kMode_M8I: {
        Register index = InputRegister(NextOffset(offset));
        ScaleFactor scale = ScaleFor(kMode_M1I, mode);
        int32_t disp = InputInt32(NextOffset(offset));
        return Operand(index, scale, disp);
      }
      case kMode_MI: {
        int32_t disp = InputInt32(NextOffset(offset));
        return Operand(Immediate(disp));
      }
      case kMode_None:
        UNREACHABLE();
        return Operand(no_reg, 0);
    }
    UNREACHABLE();
    return Operand(no_reg, 0);
  }

  Operand MemoryOperand(size_t first_input = 0) {
    return MemoryOperand(&first_input);
  }
};


namespace {

bool HasImmediateInput(Instruction* instr, size_t index) {
  return instr->InputAt(index)->IsImmediate();
}


class OutOfLineLoadInteger final : public OutOfLineCode {
 public:
  OutOfLineLoadInteger(CodeGenerator* gen, Register result)
      : OutOfLineCode(gen), result_(result) {}

  void Generate() final { __ xor_(result_, result_); }

 private:
  Register const result_;
};


class OutOfLineLoadFloat final : public OutOfLineCode {
 public:
  OutOfLineLoadFloat(CodeGenerator* gen, X87Register result)
      : OutOfLineCode(gen), result_(result) {}

  void Generate() final {
    DCHECK(result_.code() == 0);
    USE(result_);
    __ fstp(0);
    __ push(Immediate(0xffffffff));
    __ push(Immediate(0x7fffffff));
    __ fld_d(MemOperand(esp, 0));
    __ lea(esp, Operand(esp, kDoubleSize));
  }

 private:
  X87Register const result_;
};


class OutOfLineTruncateDoubleToI final : public OutOfLineCode {
 public:
  OutOfLineTruncateDoubleToI(CodeGenerator* gen, Register result,
                             X87Register input)
      : OutOfLineCode(gen), result_(result), input_(input) {}

  void Generate() final {
    UNIMPLEMENTED();
    USE(result_);
    USE(input_);
  }

 private:
  Register const result_;
  X87Register const input_;
};

}  // namespace


#define ASSEMBLE_CHECKED_LOAD_FLOAT(asm_instr)                          \
  do {                                                                  \
    auto result = i.OutputDoubleRegister();                             \
    auto offset = i.InputRegister(0);                                   \
    DCHECK(result.code() == 0);                                         \
    if (instr->InputAt(1)->IsRegister()) {                              \
      __ cmp(offset, i.InputRegister(1));                               \
    } else {                                                            \
      __ cmp(offset, i.InputImmediate(1));                              \
    }                                                                   \
    OutOfLineCode* ool = new (zone()) OutOfLineLoadFloat(this, result); \
    __ j(above_equal, ool->entry());                                    \
    __ fstp(0);                                                         \
    __ asm_instr(i.MemoryOperand(2));                                   \
    __ bind(ool->exit());                                               \
  } while (false)


#define ASSEMBLE_CHECKED_LOAD_INTEGER(asm_instr)                          \
  do {                                                                    \
    auto result = i.OutputRegister();                                     \
    auto offset = i.InputRegister(0);                                     \
    if (instr->InputAt(1)->IsRegister()) {                                \
      __ cmp(offset, i.InputRegister(1));                                 \
    } else {                                                              \
      __ cmp(offset, i.InputImmediate(1));                                \
    }                                                                     \
    OutOfLineCode* ool = new (zone()) OutOfLineLoadInteger(this, result); \
    __ j(above_equal, ool->entry());                                      \
    __ asm_instr(result, i.MemoryOperand(2));                             \
    __ bind(ool->exit());                                                 \
  } while (false)


#define ASSEMBLE_CHECKED_STORE_FLOAT(asm_instr)   \
  do {                                            \
    auto offset = i.InputRegister(0);             \
    if (instr->InputAt(1)->IsRegister()) {        \
      __ cmp(offset, i.InputRegister(1));         \
    } else {                                      \
      __ cmp(offset, i.InputImmediate(1));        \
    }                                             \
    Label done;                                   \
    DCHECK(i.InputDoubleRegister(2).code() == 0); \
    __ j(above_equal, &done, Label::kNear);       \
    __ asm_instr(i.MemoryOperand(3));             \
    __ bind(&done);                               \
  } while (false)


#define ASSEMBLE_CHECKED_STORE_INTEGER(asm_instr)            \
  do {                                                       \
    auto offset = i.InputRegister(0);                        \
    if (instr->InputAt(1)->IsRegister()) {                   \
      __ cmp(offset, i.InputRegister(1));                    \
    } else {                                                 \
      __ cmp(offset, i.InputImmediate(1));                   \
    }                                                        \
    Label done;                                              \
    __ j(above_equal, &done, Label::kNear);                  \
    if (instr->InputAt(2)->IsRegister()) {                   \
      __ asm_instr(i.MemoryOperand(3), i.InputRegister(2));  \
    } else {                                                 \
      __ asm_instr(i.MemoryOperand(3), i.InputImmediate(2)); \
    }                                                        \
    __ bind(&done);                                          \
  } while (false)


void CodeGenerator::AssembleDeconstructActivationRecord() {
  CallDescriptor* descriptor = linkage()->GetIncomingDescriptor();
  int stack_slots = frame()->GetSpillSlotCount();
  if (descriptor->IsJSFunctionCall() || stack_slots > 0) {
    __ mov(esp, ebp);
    __ pop(ebp);
  }
}


// Assembles an instruction after register allocation, producing machine code.
void CodeGenerator::AssembleArchInstruction(Instruction* instr) {
  X87OperandConverter i(this, instr);

  switch (ArchOpcodeField::decode(instr->opcode())) {
    case kArchCallCodeObject: {
      EnsureSpaceForLazyDeopt();
      if (HasImmediateInput(instr, 0)) {
        Handle<Code> code = Handle<Code>::cast(i.InputHeapObject(0));
        __ call(code, RelocInfo::CODE_TARGET);
      } else {
        Register reg = i.InputRegister(0);
        __ call(Operand(reg, Code::kHeaderSize - kHeapObjectTag));
      }
      RecordCallPosition(instr);
      bool double_result =
          instr->HasOutput() && instr->Output()->IsDoubleRegister();
      if (double_result) {
        __ lea(esp, Operand(esp, -kDoubleSize));
        __ fstp_d(Operand(esp, 0));
      }
      __ fninit();
      if (double_result) {
        __ fld_d(Operand(esp, 0));
        __ lea(esp, Operand(esp, kDoubleSize));
      } else {
        __ fld1();
      }
      break;
    }
    case kArchTailCallCodeObject: {
      AssembleDeconstructActivationRecord();
      if (HasImmediateInput(instr, 0)) {
        Handle<Code> code = Handle<Code>::cast(i.InputHeapObject(0));
        __ jmp(code, RelocInfo::CODE_TARGET);
      } else {
        Register reg = i.InputRegister(0);
        __ jmp(Operand(reg, Code::kHeaderSize - kHeapObjectTag));
      }
      break;
    }
    case kArchCallJSFunction: {
      EnsureSpaceForLazyDeopt();
      Register func = i.InputRegister(0);
      if (FLAG_debug_code) {
        // Check the function's context matches the context argument.
        __ cmp(esi, FieldOperand(func, JSFunction::kContextOffset));
        __ Assert(equal, kWrongFunctionContext);
      }
      __ call(FieldOperand(func, JSFunction::kCodeEntryOffset));
      RecordCallPosition(instr);
      bool double_result =
          instr->HasOutput() && instr->Output()->IsDoubleRegister();
      if (double_result) {
        __ lea(esp, Operand(esp, -kDoubleSize));
        __ fstp_d(Operand(esp, 0));
      }
      __ fninit();
      if (double_result) {
        __ fld_d(Operand(esp, 0));
        __ lea(esp, Operand(esp, kDoubleSize));
      } else {
        __ fld1();
      }
      break;
    }
    case kArchTailCallJSFunction: {
      Register func = i.InputRegister(0);
      if (FLAG_debug_code) {
        // Check the function's context matches the context argument.
        __ cmp(esi, FieldOperand(func, JSFunction::kContextOffset));
        __ Assert(equal, kWrongFunctionContext);
      }
      AssembleDeconstructActivationRecord();
      __ jmp(FieldOperand(func, JSFunction::kCodeEntryOffset));
      break;
    }
    case kArchPrepareCallCFunction: {
      int const num_parameters = MiscField::decode(instr->opcode());
      __ PrepareCallCFunction(num_parameters, i.TempRegister(0));
      break;
    }
    case kArchCallCFunction: {
      int const num_parameters = MiscField::decode(instr->opcode());
      if (HasImmediateInput(instr, 0)) {
        ExternalReference ref = i.InputExternalReference(0);
        __ CallCFunction(ref, num_parameters);
      } else {
        Register func = i.InputRegister(0);
        __ CallCFunction(func, num_parameters);
      }
      break;
    }
    case kArchJmp:
      AssembleArchJump(i.InputRpo(0));
      break;
    case kArchLookupSwitch:
      AssembleArchLookupSwitch(instr);
      break;
    case kArchTableSwitch:
      AssembleArchTableSwitch(instr);
      break;
    case kArchNop:
      // don't emit code for nops.
      break;
    case kArchDeoptimize: {
      int deopt_state_id =
          BuildTranslation(instr, -1, 0, OutputFrameStateCombine::Ignore());
      AssembleDeoptimizerCall(deopt_state_id, Deoptimizer::EAGER);
      break;
    }
    case kArchRet:
      AssembleReturn();
      break;
    case kArchFramePointer:
      __ mov(i.OutputRegister(), ebp);
      break;
    case kArchStackPointer:
      __ mov(i.OutputRegister(), esp);
      break;
    case kArchTruncateDoubleToI: {
      if (!instr->InputAt(0)->IsDoubleRegister()) {
        __ fld_d(i.InputOperand(0));
      }
      __ TruncateX87TOSToI(i.OutputRegister());
      if (!instr->InputAt(0)->IsDoubleRegister()) {
        __ fstp(0);
      }
      break;
    }
    case kX87Add:
      if (HasImmediateInput(instr, 1)) {
        __ add(i.InputOperand(0), i.InputImmediate(1));
      } else {
        __ add(i.InputRegister(0), i.InputOperand(1));
      }
      break;
    case kX87And:
      if (HasImmediateInput(instr, 1)) {
        __ and_(i.InputOperand(0), i.InputImmediate(1));
      } else {
        __ and_(i.InputRegister(0), i.InputOperand(1));
      }
      break;
    case kX87Cmp:
      if (HasImmediateInput(instr, 1)) {
        __ cmp(i.InputOperand(0), i.InputImmediate(1));
      } else {
        __ cmp(i.InputRegister(0), i.InputOperand(1));
      }
      break;
    case kX87Test:
      if (HasImmediateInput(instr, 1)) {
        __ test(i.InputOperand(0), i.InputImmediate(1));
      } else {
        __ test(i.InputRegister(0), i.InputOperand(1));
      }
      break;
    case kX87Imul:
      if (HasImmediateInput(instr, 1)) {
        __ imul(i.OutputRegister(), i.InputOperand(0), i.InputInt32(1));
      } else {
        __ imul(i.OutputRegister(), i.InputOperand(1));
      }
      break;
    case kX87ImulHigh:
      __ imul(i.InputRegister(1));
      break;
    case kX87UmulHigh:
      __ mul(i.InputRegister(1));
      break;
    case kX87Idiv:
      __ cdq();
      __ idiv(i.InputOperand(1));
      break;
    case kX87Udiv:
      __ Move(edx, Immediate(0));
      __ div(i.InputOperand(1));
      break;
    case kX87Not:
      __ not_(i.OutputOperand());
      break;
    case kX87Neg:
      __ neg(i.OutputOperand());
      break;
    case kX87Or:
      if (HasImmediateInput(instr, 1)) {
        __ or_(i.InputOperand(0), i.InputImmediate(1));
      } else {
        __ or_(i.InputRegister(0), i.InputOperand(1));
      }
      break;
    case kX87Xor:
      if (HasImmediateInput(instr, 1)) {
        __ xor_(i.InputOperand(0), i.InputImmediate(1));
      } else {
        __ xor_(i.InputRegister(0), i.InputOperand(1));
      }
      break;
    case kX87Sub:
      if (HasImmediateInput(instr, 1)) {
        __ sub(i.InputOperand(0), i.InputImmediate(1));
      } else {
        __ sub(i.InputRegister(0), i.InputOperand(1));
      }
      break;
    case kX87Shl:
      if (HasImmediateInput(instr, 1)) {
        __ shl(i.OutputOperand(), i.InputInt5(1));
      } else {
        __ shl_cl(i.OutputOperand());
      }
      break;
    case kX87Shr:
      if (HasImmediateInput(instr, 1)) {
        __ shr(i.OutputOperand(), i.InputInt5(1));
      } else {
        __ shr_cl(i.OutputOperand());
      }
      break;
    case kX87Sar:
      if (HasImmediateInput(instr, 1)) {
        __ sar(i.OutputOperand(), i.InputInt5(1));
      } else {
        __ sar_cl(i.OutputOperand());
      }
      break;
    case kX87Ror:
      if (HasImmediateInput(instr, 1)) {
        __ ror(i.OutputOperand(), i.InputInt5(1));
      } else {
        __ ror_cl(i.OutputOperand());
      }
      break;
    case kX87Lzcnt:
      __ Lzcnt(i.OutputRegister(), i.InputOperand(0));
      break;
    case kX87LoadFloat64Constant: {
      InstructionOperand* source = instr->InputAt(0);
      InstructionOperand* destination = instr->Output();
      DCHECK(source->IsConstant());
      X87OperandConverter g(this, NULL);
      Constant src_constant = g.ToConstant(source);

      DCHECK_EQ(Constant::kFloat64, src_constant.type());
      uint64_t src = bit_cast<uint64_t>(src_constant.ToFloat64());
      uint32_t lower = static_cast<uint32_t>(src);
      uint32_t upper = static_cast<uint32_t>(src >> 32);
      if (destination->IsDoubleRegister()) {
        __ sub(esp, Immediate(kDoubleSize));
        __ mov(MemOperand(esp, 0), Immediate(lower));
        __ mov(MemOperand(esp, kInt32Size), Immediate(upper));
        __ fstp(0);
        __ fld_d(MemOperand(esp, 0));
        __ add(esp, Immediate(kDoubleSize));
      } else {
        UNREACHABLE();
      }
      break;
    }
    case kX87Float32Cmp: {
      __ fld_s(MemOperand(esp, kFloatSize));
      __ fld_s(MemOperand(esp, 0));
      __ FCmp();
      __ lea(esp, Operand(esp, 2 * kFloatSize));
      break;
    }
    case kX87Float32Add: {
      __ X87SetFPUCW(0x027F);
      __ fstp(0);
      __ fld_s(MemOperand(esp, 0));
      __ fld_s(MemOperand(esp, kFloatSize));
      __ faddp();
      // Clear stack.
      __ lea(esp, Operand(esp, 2 * kFloatSize));
      // Restore the default value of control word.
      __ X87SetFPUCW(0x037F);
      break;
    }
    case kX87Float32Sub: {
      __ X87SetFPUCW(0x027F);
      __ fstp(0);
      __ fld_s(MemOperand(esp, kFloatSize));
      __ fld_s(MemOperand(esp, 0));
      __ fsubp();
      // Clear stack.
      __ lea(esp, Operand(esp, 2 * kFloatSize));
      // Restore the default value of control word.
      __ X87SetFPUCW(0x037F);
      break;
    }
    case kX87Float32Mul: {
      __ X87SetFPUCW(0x027F);
      __ fstp(0);
      __ fld_s(MemOperand(esp, kFloatSize));
      __ fld_s(MemOperand(esp, 0));
      __ fmulp();
      // Clear stack.
      __ lea(esp, Operand(esp, 2 * kFloatSize));
      // Restore the default value of control word.
      __ X87SetFPUCW(0x037F);
      break;
    }
    case kX87Float32Div: {
      __ X87SetFPUCW(0x027F);
      __ fstp(0);
      __ fld_s(MemOperand(esp, kFloatSize));
      __ fld_s(MemOperand(esp, 0));
      __ fdivp();
      // Clear stack.
      __ lea(esp, Operand(esp, 2 * kFloatSize));
      // Restore the default value of control word.
      __ X87SetFPUCW(0x037F);
      break;
    }
    case kX87Float32Max: {
      Label check_nan_left, check_zero, return_left, return_right;
      Condition condition = below;
      __ fstp(0);
      __ fld_s(MemOperand(esp, kFloatSize));
      __ fld_s(MemOperand(esp, 0));
      __ fld(1);
      __ fld(1);
      __ FCmp();
      __ j(parity_even, &check_nan_left, Label::kNear);  // At least one NaN.
      __ j(equal, &check_zero, Label::kNear);            // left == right.
      __ j(condition, &return_left, Label::kNear);
      __ jmp(&return_right, Label::kNear);

      __ bind(&check_zero);
      __ fld(0);
      __ fldz();
      __ FCmp();
      __ j(not_equal, &return_left, Label::kNear);  // left == right != 0.

      __ fadd(1);
      __ jmp(&return_left, Label::kNear);

      __ bind(&check_nan_left);
      __ fld(0);
      __ fld(0);
      __ FCmp();                                      // NaN check.
      __ j(parity_even, &return_left, Label::kNear);  // left == NaN.

      __ bind(&return_right);
      __ fxch();

      __ bind(&return_left);
      __ fstp(0);
      __ lea(esp, Operand(esp, 2 * kFloatSize));
      break;
    }
    case kX87Float32Min: {
      Label check_nan_left, check_zero, return_left, return_right;
      Condition condition = above;
      __ fstp(0);
      __ fld_s(MemOperand(esp, kFloatSize));
      __ fld_s(MemOperand(esp, 0));
      __ fld(1);
      __ fld(1);
      __ FCmp();
      __ j(parity_even, &check_nan_left, Label::kNear);  // At least one NaN.
      __ j(equal, &check_zero, Label::kNear);            // left == right.
      __ j(condition, &return_left, Label::kNear);
      __ jmp(&return_right, Label::kNear);

      __ bind(&check_zero);
      __ fld(0);
      __ fldz();
      __ FCmp();
      __ j(not_equal, &return_left, Label::kNear);  // left == right != 0.
      // At this point, both left and right are either 0 or -0.
      // Push st0 and st1 to stack, then pop them to temp registers and OR them,
      // load it to left.
      __ push(eax);
      __ fld(1);
      __ fld(1);
      __ sub(esp, Immediate(2 * kPointerSize));
      __ fstp_s(MemOperand(esp, 0));
      __ fstp_s(MemOperand(esp, kPointerSize));
      __ pop(eax);
      __ xor_(MemOperand(esp, 0), eax);
      __ fstp(0);
      __ fld_s(MemOperand(esp, 0));
      __ pop(eax);  // restore esp
      __ pop(eax);  // restore esp
      __ jmp(&return_left, Label::kNear);

      __ bind(&check_nan_left);
      __ fld(0);
      __ fld(0);
      __ FCmp();                                      // NaN check.
      __ j(parity_even, &return_left, Label::kNear);  // left == NaN.

      __ bind(&return_right);
      __ fxch();

      __ bind(&return_left);
      __ fstp(0);
      __ lea(esp, Operand(esp, 2 * kFloatSize));
      break;
    }
    case kX87Float32Sqrt: {
      __ fstp(0);
      __ fld_s(MemOperand(esp, 0));
      __ fsqrt();
      __ lea(esp, Operand(esp, kFloatSize));
      break;
    }
    case kX87Float32Abs: {
      __ fstp(0);
      __ fld_s(MemOperand(esp, 0));
      __ fabs();
      __ lea(esp, Operand(esp, kFloatSize));
      break;
    }
    case kX87Float64Add: {
      __ X87SetFPUCW(0x027F);
      __ fstp(0);
      __ fld_d(MemOperand(esp, 0));
      __ fld_d(MemOperand(esp, kDoubleSize));
      __ faddp();
      // Clear stack.
      __ lea(esp, Operand(esp, 2 * kDoubleSize));
      // Restore the default value of control word.
      __ X87SetFPUCW(0x037F);
      break;
    }
    case kX87Float64Sub: {
      __ X87SetFPUCW(0x027F);
      __ fstp(0);
      __ fld_d(MemOperand(esp, kDoubleSize));
      __ fsub_d(MemOperand(esp, 0));
      // Clear stack.
      __ lea(esp, Operand(esp, 2 * kDoubleSize));
      // Restore the default value of control word.
      __ X87SetFPUCW(0x037F);
      break;
    }
    case kX87Float64Mul: {
      __ X87SetFPUCW(0x027F);
      __ fstp(0);
      __ fld_d(MemOperand(esp, kDoubleSize));
      __ fmul_d(MemOperand(esp, 0));
      // Clear stack.
      __ lea(esp, Operand(esp, 2 * kDoubleSize));
      // Restore the default value of control word.
      __ X87SetFPUCW(0x037F);
      break;
    }
    case kX87Float64Div: {
      __ X87SetFPUCW(0x027F);
      __ fstp(0);
      __ fld_d(MemOperand(esp, kDoubleSize));
      __ fdiv_d(MemOperand(esp, 0));
      // Clear stack.
      __ lea(esp, Operand(esp, 2 * kDoubleSize));
      // Restore the default value of control word.
      __ X87SetFPUCW(0x037F);
      break;
    }
    case kX87Float64Mod: {
      FrameScope frame_scope(&masm_, StackFrame::MANUAL);
      __ mov(eax, esp);
      __ PrepareCallCFunction(4, eax);
      __ fstp(0);
      __ fld_d(MemOperand(eax, 0));
      __ fstp_d(Operand(esp, 1 * kDoubleSize));
      __ fld_d(MemOperand(eax, kDoubleSize));
      __ fstp_d(Operand(esp, 0));
      __ CallCFunction(ExternalReference::mod_two_doubles_operation(isolate()),
                       4);
      __ lea(esp, Operand(esp, 2 * kDoubleSize));
      break;
    }
    case kX87Float64Max: {
      Label check_zero, return_left, return_right;
      Condition condition = below;
      __ fstp(0);
      __ fld_d(MemOperand(esp, kDoubleSize));
      __ fld_d(MemOperand(esp, 0));
      __ fld(1);
      __ fld(1);
      __ FCmp();
      __ j(parity_even, &return_right,
           Label::kNear);  // At least one NaN, Return right.
      __ j(equal, &check_zero, Label::kNear);  // left == right.
      __ j(condition, &return_left, Label::kNear);
      __ jmp(&return_right, Label::kNear);

      __ bind(&check_zero);
      __ fld(0);
      __ fldz();
      __ FCmp();
      __ j(not_equal, &return_left, Label::kNear);  // left == right != 0.

      __ bind(&return_right);
      __ fxch();

      __ bind(&return_left);
      __ fstp(0);
      __ lea(esp, Operand(esp, 2 * kDoubleSize));
      break;
    }
    case kX87Float64Min: {
      Label check_zero, return_left, return_right;
      Condition condition = above;
      __ fstp(0);
      __ fld_d(MemOperand(esp, kDoubleSize));
      __ fld_d(MemOperand(esp, 0));
      __ fld(1);
      __ fld(1);
      __ FCmp();
      __ j(parity_even, &return_right,
           Label::kNear);  // At least one NaN, return right value.
      __ j(equal, &check_zero, Label::kNear);  // left == right.
      __ j(condition, &return_left, Label::kNear);
      __ jmp(&return_right, Label::kNear);

      __ bind(&check_zero);
      __ fld(0);
      __ fldz();
      __ FCmp();
      __ j(not_equal, &return_left, Label::kNear);  // left == right != 0.

      __ bind(&return_right);
      __ fxch();

      __ bind(&return_left);
      __ fstp(0);
      __ lea(esp, Operand(esp, 2 * kDoubleSize));
      break;
    }
    case kX87Float64Abs: {
      __ fstp(0);
      __ fld_d(MemOperand(esp, 0));
      __ fabs();
      __ lea(esp, Operand(esp, kDoubleSize));
      break;
    }
    case kX87Int32ToFloat64: {
      InstructionOperand* input = instr->InputAt(0);
      DCHECK(input->IsRegister() || input->IsStackSlot());
      __ fstp(0);
      if (input->IsRegister()) {
        Register input_reg = i.InputRegister(0);
        __ push(input_reg);
        __ fild_s(Operand(esp, 0));
        __ pop(input_reg);
      } else {
        __ fild_s(i.InputOperand(0));
      }
      break;
    }
    case kX87Float32ToFloat64: {
      InstructionOperand* input = instr->InputAt(0);
      if (input->IsDoubleRegister()) {
        __ sub(esp, Immediate(kDoubleSize));
        __ fstp_d(MemOperand(esp, 0));
        __ fld_d(MemOperand(esp, 0));
        __ add(esp, Immediate(kDoubleSize));
      } else {
        DCHECK(input->IsDoubleStackSlot());
        __ fstp(0);
        __ fld_s(i.InputOperand(0));
      }
      break;
    }
    case kX87Uint32ToFloat64: {
      __ fstp(0);
      __ LoadUint32NoSSE2(i.InputRegister(0));
      break;
    }
    case kX87Float64ToInt32: {
      if (!instr->InputAt(0)->IsDoubleRegister()) {
        __ fld_d(i.InputOperand(0));
      }
      __ TruncateX87TOSToI(i.OutputRegister(0));
      if (!instr->InputAt(0)->IsDoubleRegister()) {
        __ fstp(0);
      }
      break;
    }
    case kX87Float64ToFloat32: {
      InstructionOperand* input = instr->InputAt(0);
      if (input->IsDoubleRegister()) {
        __ sub(esp, Immediate(kDoubleSize));
        __ fstp_s(MemOperand(esp, 0));
        __ fld_s(MemOperand(esp, 0));
        __ add(esp, Immediate(kDoubleSize));
      } else {
        DCHECK(input->IsDoubleStackSlot());
        __ fstp(0);
        __ fld_d(i.InputOperand(0));
        __ sub(esp, Immediate(kDoubleSize));
        __ fstp_s(MemOperand(esp, 0));
        __ fld_s(MemOperand(esp, 0));
        __ add(esp, Immediate(kDoubleSize));
      }
      break;
    }
    case kX87Float64ToUint32: {
      __ push_imm32(-2147483648);
      if (!instr->InputAt(0)->IsDoubleRegister()) {
        __ fld_d(i.InputOperand(0));
      }
      __ fild_s(Operand(esp, 0));
      __ fadd(1);
      __ fstp(0);
      __ TruncateX87TOSToI(i.OutputRegister(0));
      __ add(esp, Immediate(kInt32Size));
      __ add(i.OutputRegister(), Immediate(0x80000000));
      if (!instr->InputAt(0)->IsDoubleRegister()) {
        __ fstp(0);
      }
      break;
    }
    case kX87Float64ExtractHighWord32: {
      if (instr->InputAt(0)->IsDoubleRegister()) {
        __ sub(esp, Immediate(kDoubleSize));
        __ fst_d(MemOperand(esp, 0));
        __ mov(i.OutputRegister(), MemOperand(esp, kDoubleSize / 2));
        __ add(esp, Immediate(kDoubleSize));
      } else {
        InstructionOperand* input = instr->InputAt(0);
        USE(input);
        DCHECK(input->IsDoubleStackSlot());
        __ mov(i.OutputRegister(), i.InputOperand(0, kDoubleSize / 2));
      }
      break;
    }
    case kX87Float64ExtractLowWord32: {
      if (instr->InputAt(0)->IsDoubleRegister()) {
        __ sub(esp, Immediate(kDoubleSize));
        __ fst_d(MemOperand(esp, 0));
        __ mov(i.OutputRegister(), MemOperand(esp, 0));
        __ add(esp, Immediate(kDoubleSize));
      } else {
        InstructionOperand* input = instr->InputAt(0);
        USE(input);
        DCHECK(input->IsDoubleStackSlot());
        __ mov(i.OutputRegister(), i.InputOperand(0));
      }
      break;
    }
    case kX87Float64InsertHighWord32: {
      __ sub(esp, Immediate(kDoubleSize));
      __ fstp_d(MemOperand(esp, 0));
      __ mov(MemOperand(esp, kDoubleSize / 2), i.InputRegister(1));
      __ fld_d(MemOperand(esp, 0));
      __ add(esp, Immediate(kDoubleSize));
      break;
    }
    case kX87Float64InsertLowWord32: {
      __ sub(esp, Immediate(kDoubleSize));
      __ fstp_d(MemOperand(esp, 0));
      __ mov(MemOperand(esp, 0), i.InputRegister(1));
      __ fld_d(MemOperand(esp, 0));
      __ add(esp, Immediate(kDoubleSize));
      break;
    }
    case kX87Float64Sqrt: {
      __ fstp(0);
      __ fld_d(MemOperand(esp, 0));
      __ fsqrt();
      __ lea(esp, Operand(esp, kDoubleSize));
      break;
    }
    case kX87Float64Round: {
      RoundingMode mode =
          static_cast<RoundingMode>(MiscField::decode(instr->opcode()));
      if (mode == MiscField::encode(kRoundDown)) {
        __ X87SetRC(0x0400);
      } else {
        __ X87SetRC(0x0c00);
      }

      if (!instr->InputAt(0)->IsDoubleRegister()) {
        InstructionOperand* input = instr->InputAt(0);
        USE(input);
        DCHECK(input->IsDoubleStackSlot());
        __ fstp(0);
        __ fld_d(i.InputOperand(0));
      }
      __ frndint();
      __ X87SetRC(0x0000);
      break;
    }
    case kX87Float64Cmp: {
      __ fld_d(MemOperand(esp, kDoubleSize));
      __ fld_d(MemOperand(esp, 0));
      __ FCmp();
      __ lea(esp, Operand(esp, 2 * kDoubleSize));
      break;
    }
    case kX87Movsxbl:
      __ movsx_b(i.OutputRegister(), i.MemoryOperand());
      break;
    case kX87Movzxbl:
      __ movzx_b(i.OutputRegister(), i.MemoryOperand());
      break;
    case kX87Movb: {
      size_t index = 0;
      Operand operand = i.MemoryOperand(&index);
      if (HasImmediateInput(instr, index)) {
        __ mov_b(operand, i.InputInt8(index));
      } else {
        __ mov_b(operand, i.InputRegister(index));
      }
      break;
    }
    case kX87Movsxwl:
      __ movsx_w(i.OutputRegister(), i.MemoryOperand());
      break;
    case kX87Movzxwl:
      __ movzx_w(i.OutputRegister(), i.MemoryOperand());
      break;
    case kX87Movw: {
      size_t index = 0;
      Operand operand = i.MemoryOperand(&index);
      if (HasImmediateInput(instr, index)) {
        __ mov_w(operand, i.InputInt16(index));
      } else {
        __ mov_w(operand, i.InputRegister(index));
      }
      break;
    }
    case kX87Movl:
      if (instr->HasOutput()) {
        __ mov(i.OutputRegister(), i.MemoryOperand());
      } else {
        size_t index = 0;
        Operand operand = i.MemoryOperand(&index);
        if (HasImmediateInput(instr, index)) {
          __ mov(operand, i.InputImmediate(index));
        } else {
          __ mov(operand, i.InputRegister(index));
        }
      }
      break;
    case kX87Movsd: {
      if (instr->HasOutput()) {
        X87Register output = i.OutputDoubleRegister();
        USE(output);
        DCHECK(output.code() == 0);
        __ fstp(0);
        __ fld_d(i.MemoryOperand());
      } else {
        size_t index = 0;
        Operand operand = i.MemoryOperand(&index);
        __ fst_d(operand);
      }
      break;
    }
    case kX87Movss: {
      if (instr->HasOutput()) {
        X87Register output = i.OutputDoubleRegister();
        USE(output);
        DCHECK(output.code() == 0);
        __ fstp(0);
        __ fld_s(i.MemoryOperand());
      } else {
        size_t index = 0;
        Operand operand = i.MemoryOperand(&index);
        __ fst_s(operand);
      }
      break;
    }
    case kX87Lea: {
      AddressingMode mode = AddressingModeField::decode(instr->opcode());
      // Shorten "leal" to "addl", "subl" or "shll" if the register allocation
      // and addressing mode just happens to work out. The "addl"/"subl" forms
      // in these cases are faster based on measurements.
      if (mode == kMode_MI) {
        __ Move(i.OutputRegister(), Immediate(i.InputInt32(0)));
      } else if (i.InputRegister(0).is(i.OutputRegister())) {
        if (mode == kMode_MRI) {
          int32_t constant_summand = i.InputInt32(1);
          if (constant_summand > 0) {
            __ add(i.OutputRegister(), Immediate(constant_summand));
          } else if (constant_summand < 0) {
            __ sub(i.OutputRegister(), Immediate(-constant_summand));
          }
        } else if (mode == kMode_MR1) {
          if (i.InputRegister(1).is(i.OutputRegister())) {
            __ shl(i.OutputRegister(), 1);
          } else {
            __ lea(i.OutputRegister(), i.MemoryOperand());
          }
        } else if (mode == kMode_M2) {
          __ shl(i.OutputRegister(), 1);
        } else if (mode == kMode_M4) {
          __ shl(i.OutputRegister(), 2);
        } else if (mode == kMode_M8) {
          __ shl(i.OutputRegister(), 3);
        } else {
          __ lea(i.OutputRegister(), i.MemoryOperand());
        }
      } else {
        __ lea(i.OutputRegister(), i.MemoryOperand());
      }
      break;
    }
    case kX87Push:
      if (HasImmediateInput(instr, 0)) {
        __ push(i.InputImmediate(0));
      } else {
        __ push(i.InputOperand(0));
      }
      break;
    case kX87Poke: {
      int const slot = MiscField::decode(instr->opcode());
      if (HasImmediateInput(instr, 0)) {
        __ mov(Operand(esp, slot * kPointerSize), i.InputImmediate(0));
      } else {
        __ mov(Operand(esp, slot * kPointerSize), i.InputRegister(0));
      }
      break;
    }
    case kX87PushFloat32:
      __ lea(esp, Operand(esp, -kFloatSize));
      if (instr->InputAt(0)->IsDoubleStackSlot()) {
        __ fld_s(i.InputOperand(0));
        __ fstp_s(MemOperand(esp, 0));
      } else if (instr->InputAt(0)->IsDoubleRegister()) {
        __ fst_s(MemOperand(esp, 0));
      } else {
        UNREACHABLE();
      }
      break;
    case kX87PushFloat64:
      __ lea(esp, Operand(esp, -kDoubleSize));
      if (instr->InputAt(0)->IsDoubleStackSlot()) {
        __ fld_d(i.InputOperand(0));
        __ fstp_d(MemOperand(esp, 0));
      } else if (instr->InputAt(0)->IsDoubleRegister()) {
        __ fst_d(MemOperand(esp, 0));
      } else {
        UNREACHABLE();
      }
      break;
    case kX87StoreWriteBarrier: {
      Register object = i.InputRegister(0);
      Register value = i.InputRegister(2);
      SaveFPRegsMode mode =
          frame()->DidAllocateDoubleRegisters() ? kSaveFPRegs : kDontSaveFPRegs;
      if (HasImmediateInput(instr, 1)) {
        int index = i.InputInt32(1);
        Register scratch = i.TempRegister(1);
        __ mov(Operand(object, index), value);
        __ RecordWriteContextSlot(object, index, value, scratch, mode);
      } else {
        Register index = i.InputRegister(1);
        __ mov(Operand(object, index, times_1, 0), value);
        __ lea(index, Operand(object, index, times_1, 0));
        __ RecordWrite(object, index, value, mode);
      }
      break;
    }
    case kCheckedLoadInt8:
      ASSEMBLE_CHECKED_LOAD_INTEGER(movsx_b);
      break;
    case kCheckedLoadUint8:
      ASSEMBLE_CHECKED_LOAD_INTEGER(movzx_b);
      break;
    case kCheckedLoadInt16:
      ASSEMBLE_CHECKED_LOAD_INTEGER(movsx_w);
      break;
    case kCheckedLoadUint16:
      ASSEMBLE_CHECKED_LOAD_INTEGER(movzx_w);
      break;
    case kCheckedLoadWord32:
      ASSEMBLE_CHECKED_LOAD_INTEGER(mov);
      break;
    case kCheckedLoadFloat32:
      ASSEMBLE_CHECKED_LOAD_FLOAT(fld_s);
      break;
    case kCheckedLoadFloat64:
      ASSEMBLE_CHECKED_LOAD_FLOAT(fld_d);
      break;
    case kCheckedStoreWord8:
      ASSEMBLE_CHECKED_STORE_INTEGER(mov_b);
      break;
    case kCheckedStoreWord16:
      ASSEMBLE_CHECKED_STORE_INTEGER(mov_w);
      break;
    case kCheckedStoreWord32:
      ASSEMBLE_CHECKED_STORE_INTEGER(mov);
      break;
    case kCheckedStoreFloat32:
      ASSEMBLE_CHECKED_STORE_FLOAT(fst_s);
      break;
    case kCheckedStoreFloat64:
      ASSEMBLE_CHECKED_STORE_FLOAT(fst_d);
      break;
    case kX87StackCheck: {
      ExternalReference const stack_limit =
          ExternalReference::address_of_stack_limit(isolate());
      __ cmp(esp, Operand::StaticVariable(stack_limit));
      break;
    }
  }
}  // NOLINT(readability/fn_size)


// Assembles a branch after an instruction.
void CodeGenerator::AssembleArchBranch(Instruction* instr, BranchInfo* branch) {
  X87OperandConverter i(this, instr);
  Label::Distance flabel_distance =
      branch->fallthru ? Label::kNear : Label::kFar;
  Label* tlabel = branch->true_label;
  Label* flabel = branch->false_label;
  switch (branch->condition) {
    case kUnorderedEqual:
      __ j(parity_even, flabel, flabel_distance);
    // Fall through.
    case kEqual:
      __ j(equal, tlabel);
      break;
    case kUnorderedNotEqual:
      __ j(parity_even, tlabel);
    // Fall through.
    case kNotEqual:
      __ j(not_equal, tlabel);
      break;
    case kSignedLessThan:
      __ j(less, tlabel);
      break;
    case kSignedGreaterThanOrEqual:
      __ j(greater_equal, tlabel);
      break;
    case kSignedLessThanOrEqual:
      __ j(less_equal, tlabel);
      break;
    case kSignedGreaterThan:
      __ j(greater, tlabel);
      break;
    case kUnsignedLessThan:
      __ j(below, tlabel);
      break;
    case kUnsignedGreaterThanOrEqual:
      __ j(above_equal, tlabel);
      break;
    case kUnsignedLessThanOrEqual:
      __ j(below_equal, tlabel);
      break;
    case kUnsignedGreaterThan:
      __ j(above, tlabel);
      break;
    case kOverflow:
      __ j(overflow, tlabel);
      break;
    case kNotOverflow:
      __ j(no_overflow, tlabel);
      break;
  }
  // Add a jump if not falling through to the next block.
  if (!branch->fallthru) __ jmp(flabel);
}


void CodeGenerator::AssembleArchJump(RpoNumber target) {
  if (!IsNextInAssemblyOrder(target)) __ jmp(GetLabel(target));
}


// Assembles boolean materializations after an instruction.
void CodeGenerator::AssembleArchBoolean(Instruction* instr,
                                        FlagsCondition condition) {
  X87OperandConverter i(this, instr);
  Label done;

  // Materialize a full 32-bit 1 or 0 value. The result register is always the
  // last output of the instruction.
  Label check;
  DCHECK_NE(0u, instr->OutputCount());
  Register reg = i.OutputRegister(instr->OutputCount() - 1);
  Condition cc = no_condition;
  switch (condition) {
    case kUnorderedEqual:
      __ j(parity_odd, &check, Label::kNear);
      __ Move(reg, Immediate(0));
      __ jmp(&done, Label::kNear);
    // Fall through.
    case kEqual:
      cc = equal;
      break;
    case kUnorderedNotEqual:
      __ j(parity_odd, &check, Label::kNear);
      __ mov(reg, Immediate(1));
      __ jmp(&done, Label::kNear);
    // Fall through.
    case kNotEqual:
      cc = not_equal;
      break;
    case kSignedLessThan:
      cc = less;
      break;
    case kSignedGreaterThanOrEqual:
      cc = greater_equal;
      break;
    case kSignedLessThanOrEqual:
      cc = less_equal;
      break;
    case kSignedGreaterThan:
      cc = greater;
      break;
    case kUnsignedLessThan:
      cc = below;
      break;
    case kUnsignedGreaterThanOrEqual:
      cc = above_equal;
      break;
    case kUnsignedLessThanOrEqual:
      cc = below_equal;
      break;
    case kUnsignedGreaterThan:
      cc = above;
      break;
    case kOverflow:
      cc = overflow;
      break;
    case kNotOverflow:
      cc = no_overflow;
      break;
  }
  __ bind(&check);
  if (reg.is_byte_register()) {
    // setcc for byte registers (al, bl, cl, dl).
    __ setcc(cc, reg);
    __ movzx_b(reg, reg);
  } else {
    // Emit a branch to set a register to either 1 or 0.
    Label set;
    __ j(cc, &set, Label::kNear);
    __ Move(reg, Immediate(0));
    __ jmp(&done, Label::kNear);
    __ bind(&set);
    __ mov(reg, Immediate(1));
  }
  __ bind(&done);
}


void CodeGenerator::AssembleArchLookupSwitch(Instruction* instr) {
  X87OperandConverter i(this, instr);
  Register input = i.InputRegister(0);
  for (size_t index = 2; index < instr->InputCount(); index += 2) {
    __ cmp(input, Immediate(i.InputInt32(index + 0)));
    __ j(equal, GetLabel(i.InputRpo(index + 1)));
  }
  AssembleArchJump(i.InputRpo(1));
}


void CodeGenerator::AssembleArchTableSwitch(Instruction* instr) {
  X87OperandConverter i(this, instr);
  Register input = i.InputRegister(0);
  size_t const case_count = instr->InputCount() - 2;
  Label** cases = zone()->NewArray<Label*>(case_count);
  for (size_t index = 0; index < case_count; ++index) {
    cases[index] = GetLabel(i.InputRpo(index + 2));
  }
  Label* const table = AddJumpTable(cases, case_count);
  __ cmp(input, Immediate(case_count));
  __ j(above_equal, GetLabel(i.InputRpo(1)));
  __ jmp(Operand::JumpTable(input, times_4, table));
}


void CodeGenerator::AssembleDeoptimizerCall(
    int deoptimization_id, Deoptimizer::BailoutType bailout_type) {
  Address deopt_entry = Deoptimizer::GetDeoptimizationEntry(
      isolate(), deoptimization_id, bailout_type);
  __ call(deopt_entry, RelocInfo::RUNTIME_ENTRY);
}


// The calling convention for JSFunctions on X87 passes arguments on the
// stack and the JSFunction and context in EDI and ESI, respectively, thus
// the steps of the call look as follows:

// --{ before the call instruction }--------------------------------------------
//                                                         |  caller frame |
//                                                         ^ esp           ^ ebp

// --{ push arguments and setup ESI, EDI }--------------------------------------
//                                       | args + receiver |  caller frame |
//                                       ^ esp                             ^ ebp
//                 [edi = JSFunction, esi = context]

// --{ call [edi + kCodeEntryOffset] }------------------------------------------
//                                 | RET | args + receiver |  caller frame |
//                                 ^ esp                                   ^ ebp

// =={ prologue of called function }============================================
// --{ push ebp }---------------------------------------------------------------
//                            | FP | RET | args + receiver |  caller frame |
//                            ^ esp                                        ^ ebp

// --{ mov ebp, esp }-----------------------------------------------------------
//                            | FP | RET | args + receiver |  caller frame |
//                            ^ ebp,esp

// --{ push esi }---------------------------------------------------------------
//                      | CTX | FP | RET | args + receiver |  caller frame |
//                      ^esp  ^ ebp

// --{ push edi }---------------------------------------------------------------
//                | FNC | CTX | FP | RET | args + receiver |  caller frame |
//                ^esp        ^ ebp

// --{ subi esp, #N }-----------------------------------------------------------
// | callee frame | FNC | CTX | FP | RET | args + receiver |  caller frame |
// ^esp                       ^ ebp

// =={ body of called function }================================================

// =={ epilogue of called function }============================================
// --{ mov esp, ebp }-----------------------------------------------------------
//                            | FP | RET | args + receiver |  caller frame |
//                            ^ esp,ebp

// --{ pop ebp }-----------------------------------------------------------
// |                               | RET | args + receiver |  caller frame |
//                                 ^ esp                                   ^ ebp

// --{ ret #A+1 }-----------------------------------------------------------
// |                                                       |  caller frame |
//                                                         ^ esp           ^ ebp


// Runtime function calls are accomplished by doing a stub call to the
// CEntryStub (a real code object). On X87 passes arguments on the
// stack, the number of arguments in EAX, the address of the runtime function
// in EBX, and the context in ESI.

// --{ before the call instruction }--------------------------------------------
//                                                         |  caller frame |
//                                                         ^ esp           ^ ebp

// --{ push arguments and setup EAX, EBX, and ESI }-----------------------------
//                                       | args + receiver |  caller frame |
//                                       ^ esp                             ^ ebp
//              [eax = #args, ebx = runtime function, esi = context]

// --{ call #CEntryStub }-------------------------------------------------------
//                                 | RET | args + receiver |  caller frame |
//                                 ^ esp                                   ^ ebp

// =={ body of runtime function }===============================================

// --{ runtime returns }--------------------------------------------------------
//                                                         |  caller frame |
//                                                         ^ esp           ^ ebp

// Other custom linkages (e.g. for calling directly into and out of C++) may
// need to save callee-saved registers on the stack, which is done in the
// function prologue of generated code.

// --{ before the call instruction }--------------------------------------------
//                                                         |  caller frame |
//                                                         ^ esp           ^ ebp

// --{ set up arguments in registers on stack }---------------------------------
//                                                  | args |  caller frame |
//                                                  ^ esp                  ^ ebp
//                  [r0 = arg0, r1 = arg1, ...]

// --{ call code }--------------------------------------------------------------
//                                            | RET | args |  caller frame |
//                                            ^ esp                        ^ ebp

// =={ prologue of called function }============================================
// --{ push ebp }---------------------------------------------------------------
//                                       | FP | RET | args |  caller frame |
//                                       ^ esp                             ^ ebp

// --{ mov ebp, esp }-----------------------------------------------------------
//                                       | FP | RET | args |  caller frame |
//                                       ^ ebp,esp

// --{ save registers }---------------------------------------------------------
//                                | regs | FP | RET | args |  caller frame |
//                                ^ esp  ^ ebp

// --{ subi esp, #N }-----------------------------------------------------------
//                 | callee frame | regs | FP | RET | args |  caller frame |
//                 ^esp                  ^ ebp

// =={ body of called function }================================================

// =={ epilogue of called function }============================================
// --{ restore registers }------------------------------------------------------
//                                | regs | FP | RET | args |  caller frame |
//                                ^ esp  ^ ebp

// --{ mov esp, ebp }-----------------------------------------------------------
//                                       | FP | RET | args |  caller frame |
//                                       ^ esp,ebp

// --{ pop ebp }----------------------------------------------------------------
//                                            | RET | args |  caller frame |
//                                            ^ esp                        ^ ebp


void CodeGenerator::AssemblePrologue() {
  CallDescriptor* descriptor = linkage()->GetIncomingDescriptor();
  int stack_slots = frame()->GetSpillSlotCount();
  if (descriptor->kind() == CallDescriptor::kCallAddress) {
    // Assemble a prologue similar the to cdecl calling convention.
    __ push(ebp);
    __ mov(ebp, esp);
    const RegList saves = descriptor->CalleeSavedRegisters();
    if (saves != 0) {  // Save callee-saved registers.
      int register_save_area_size = 0;
      for (int i = Register::kNumRegisters - 1; i >= 0; i--) {
        if (!((1 << i) & saves)) continue;
        __ push(Register::from_code(i));
        register_save_area_size += kPointerSize;
      }
      frame()->SetRegisterSaveAreaSize(register_save_area_size);
    }
  } else if (descriptor->IsJSFunctionCall()) {
    // TODO(turbofan): this prologue is redundant with OSR, but needed for
    // code aging.
    CompilationInfo* info = this->info();
    __ Prologue(info->IsCodePreAgingActive());
    frame()->SetRegisterSaveAreaSize(
        StandardFrameConstants::kFixedFrameSizeFromFp);
  } else if (needs_frame_) {
    __ StubPrologue();
    frame()->SetRegisterSaveAreaSize(
        StandardFrameConstants::kFixedFrameSizeFromFp);
  }

  if (info()->is_osr()) {
    // TurboFan OSR-compiled functions cannot be entered directly.
    __ Abort(kShouldNotDirectlyEnterOsrFunction);

    // Unoptimized code jumps directly to this entrypoint while the unoptimized
    // frame is still on the stack. Optimized code uses OSR values directly from
    // the unoptimized frame. Thus, all that needs to be done is to allocate the
    // remaining stack slots.
    if (FLAG_code_comments) __ RecordComment("-- OSR entrypoint --");
    osr_pc_offset_ = __ pc_offset();
    // TODO(titzer): cannot address target function == local #-1
    __ mov(edi, Operand(ebp, JavaScriptFrameConstants::kFunctionOffset));
    DCHECK(stack_slots >= frame()->GetOsrStackSlotCount());
    stack_slots -= frame()->GetOsrStackSlotCount();
  }

  if (stack_slots > 0) {
    // Allocate the stack slots used by this frame.
    __ sub(esp, Immediate(stack_slots * kPointerSize));
  }

  // Initailize FPU state.
  __ fninit();
  __ fld1();
}


void CodeGenerator::AssembleReturn() {
  CallDescriptor* descriptor = linkage()->GetIncomingDescriptor();
  int stack_slots = frame()->GetSpillSlotCount();
  if (descriptor->kind() == CallDescriptor::kCallAddress) {
    const RegList saves = descriptor->CalleeSavedRegisters();
    if (frame()->GetRegisterSaveAreaSize() > 0) {
      // Remove this frame's spill slots first.
      if (stack_slots > 0) {
        __ add(esp, Immediate(stack_slots * kPointerSize));
      }
      // Restore registers.
      if (saves != 0) {
        for (int i = 0; i < Register::kNumRegisters; i++) {
          if (!((1 << i) & saves)) continue;
          __ pop(Register::from_code(i));
        }
      }
      __ pop(ebp);  // Pop caller's frame pointer.
      __ ret(0);
    } else {
      // No saved registers.
      __ mov(esp, ebp);  // Move stack pointer back to frame pointer.
      __ pop(ebp);       // Pop caller's frame pointer.
      __ ret(0);
    }
  } else if (descriptor->IsJSFunctionCall() || needs_frame_) {
    // Canonicalize JSFunction return sites for now.
    if (return_label_.is_bound()) {
      __ jmp(&return_label_);
    } else {
      __ bind(&return_label_);
      __ mov(esp, ebp);  // Move stack pointer back to frame pointer.
      __ pop(ebp);       // Pop caller's frame pointer.
      int pop_count = descriptor->IsJSFunctionCall()
                          ? static_cast<int>(descriptor->JSParameterCount())
                          : (info()->IsStub()
                                 ? info()->code_stub()->GetStackParameterCount()
                                 : 0);
      if (pop_count == 0) {
        __ ret(0);
      } else {
        __ Ret(pop_count * kPointerSize, ebx);
      }
    }
  } else {
    __ ret(0);
  }
}


void CodeGenerator::AssembleMove(InstructionOperand* source,
                                 InstructionOperand* destination) {
  X87OperandConverter g(this, NULL);
  // Dispatch on the source and destination operand kinds.  Not all
  // combinations are possible.
  if (source->IsRegister()) {
    DCHECK(destination->IsRegister() || destination->IsStackSlot());
    Register src = g.ToRegister(source);
    Operand dst = g.ToOperand(destination);
    __ mov(dst, src);
  } else if (source->IsStackSlot()) {
    DCHECK(destination->IsRegister() || destination->IsStackSlot());
    Operand src = g.ToOperand(source);
    if (destination->IsRegister()) {
      Register dst = g.ToRegister(destination);
      __ mov(dst, src);
    } else {
      Operand dst = g.ToOperand(destination);
      __ push(src);
      __ pop(dst);
    }
  } else if (source->IsConstant()) {
    Constant src_constant = g.ToConstant(source);
    if (src_constant.type() == Constant::kHeapObject) {
      Handle<HeapObject> src = src_constant.ToHeapObject();
      int offset;
      if (IsMaterializableFromFrame(src, &offset)) {
        if (destination->IsRegister()) {
          Register dst = g.ToRegister(destination);
          __ mov(dst, Operand(ebp, offset));
        } else {
          DCHECK(destination->IsStackSlot());
          Operand dst = g.ToOperand(destination);
          __ push(Operand(ebp, offset));
          __ pop(dst);
        }
      } else if (destination->IsRegister()) {
        Register dst = g.ToRegister(destination);
        __ LoadHeapObject(dst, src);
      } else {
        DCHECK(destination->IsStackSlot());
        Operand dst = g.ToOperand(destination);
        AllowDeferredHandleDereference embedding_raw_address;
        if (isolate()->heap()->InNewSpace(*src)) {
          __ PushHeapObject(src);
          __ pop(dst);
        } else {
          __ mov(dst, src);
        }
      }
    } else if (destination->IsRegister()) {
      Register dst = g.ToRegister(destination);
      __ Move(dst, g.ToImmediate(source));
    } else if (destination->IsStackSlot()) {
      Operand dst = g.ToOperand(destination);
      __ Move(dst, g.ToImmediate(source));
    } else if (src_constant.type() == Constant::kFloat32) {
      // TODO(turbofan): Can we do better here?
      uint32_t src = bit_cast<uint32_t>(src_constant.ToFloat32());
      if (destination->IsDoubleRegister()) {
        __ sub(esp, Immediate(kInt32Size));
        __ mov(MemOperand(esp, 0), Immediate(src));
        // always only push one value into the x87 stack.
        __ fstp(0);
        __ fld_s(MemOperand(esp, 0));
        __ add(esp, Immediate(kInt32Size));
      } else {
        DCHECK(destination->IsDoubleStackSlot());
        Operand dst = g.ToOperand(destination);
        __ Move(dst, Immediate(src));
      }
    } else {
      DCHECK_EQ(Constant::kFloat64, src_constant.type());
      uint64_t src = bit_cast<uint64_t>(src_constant.ToFloat64());
      uint32_t lower = static_cast<uint32_t>(src);
      uint32_t upper = static_cast<uint32_t>(src >> 32);
      if (destination->IsDoubleRegister()) {
        __ sub(esp, Immediate(kDoubleSize));
        __ mov(MemOperand(esp, 0), Immediate(lower));
        __ mov(MemOperand(esp, kInt32Size), Immediate(upper));
        // always only push one value into the x87 stack.
        __ fstp(0);
        __ fld_d(MemOperand(esp, 0));
        __ add(esp, Immediate(kDoubleSize));
      } else {
        DCHECK(destination->IsDoubleStackSlot());
        Operand dst0 = g.ToOperand(destination);
        Operand dst1 = g.HighOperand(destination);
        __ Move(dst0, Immediate(lower));
        __ Move(dst1, Immediate(upper));
      }
    }
  } else if (source->IsDoubleRegister()) {
    DCHECK(destination->IsDoubleStackSlot());
    Operand dst = g.ToOperand(destination);
    auto allocated = AllocatedOperand::cast(*source);
    switch (allocated.machine_type()) {
      case kRepFloat32:
        __ fst_s(dst);
        break;
      case kRepFloat64:
        __ fst_d(dst);
        break;
      default:
        UNREACHABLE();
    }
  } else if (source->IsDoubleStackSlot()) {
    DCHECK(destination->IsDoubleRegister() || destination->IsDoubleStackSlot());
    Operand src = g.ToOperand(source);
    auto allocated = AllocatedOperand::cast(*source);
    if (destination->IsDoubleRegister()) {
      // always only push one value into the x87 stack.
      __ fstp(0);
      switch (allocated.machine_type()) {
        case kRepFloat32:
          __ fld_s(src);
          break;
        case kRepFloat64:
          __ fld_d(src);
          break;
        default:
          UNREACHABLE();
      }
    } else {
      Operand dst = g.ToOperand(destination);
      switch (allocated.machine_type()) {
        case kRepFloat32:
          __ fld_s(src);
          __ fstp_s(dst);
          break;
        case kRepFloat64:
          __ fld_d(src);
          __ fstp_d(dst);
          break;
        default:
          UNREACHABLE();
      }
    }
  } else {
    UNREACHABLE();
  }
}


void CodeGenerator::AssembleSwap(InstructionOperand* source,
                                 InstructionOperand* destination) {
  X87OperandConverter g(this, NULL);
  // Dispatch on the source and destination operand kinds.  Not all
  // combinations are possible.
  if (source->IsRegister() && destination->IsRegister()) {
    // Register-register.
    Register src = g.ToRegister(source);
    Register dst = g.ToRegister(destination);
    __ xchg(dst, src);
  } else if (source->IsRegister() && destination->IsStackSlot()) {
    // Register-memory.
    __ xchg(g.ToRegister(source), g.ToOperand(destination));
  } else if (source->IsStackSlot() && destination->IsStackSlot()) {
    // Memory-memory.
    Operand src = g.ToOperand(source);
    Operand dst = g.ToOperand(destination);
    __ push(dst);
    __ push(src);
    __ pop(dst);
    __ pop(src);
  } else if (source->IsDoubleRegister() && destination->IsDoubleRegister()) {
    UNREACHABLE();
  } else if (source->IsDoubleRegister() && destination->IsDoubleStackSlot()) {
    auto allocated = AllocatedOperand::cast(*source);
    switch (allocated.machine_type()) {
      case kRepFloat32:
        __ fld_s(g.ToOperand(destination));
        __ fxch();
        __ fstp_s(g.ToOperand(destination));
        break;
      case kRepFloat64:
        __ fld_d(g.ToOperand(destination));
        __ fxch();
        __ fstp_d(g.ToOperand(destination));
        break;
      default:
        UNREACHABLE();
    }
  } else if (source->IsDoubleStackSlot() && destination->IsDoubleStackSlot()) {
    auto allocated = AllocatedOperand::cast(*source);
    switch (allocated.machine_type()) {
      case kRepFloat32:
        __ fld_s(g.ToOperand(source));
        __ fld_s(g.ToOperand(destination));
        __ fstp_s(g.ToOperand(source));
        __ fstp_s(g.ToOperand(destination));
        break;
      case kRepFloat64:
        __ fld_d(g.ToOperand(source));
        __ fld_d(g.ToOperand(destination));
        __ fstp_d(g.ToOperand(source));
        __ fstp_d(g.ToOperand(destination));
        break;
      default:
        UNREACHABLE();
    }
  } else {
    // No other combinations are possible.
    UNREACHABLE();
  }
}


void CodeGenerator::AssembleJumpTable(Label** targets, size_t target_count) {
  for (size_t index = 0; index < target_count; ++index) {
    __ dd(targets[index]);
  }
}


void CodeGenerator::AddNopForSmiCodeInlining() { __ nop(); }


void CodeGenerator::EnsureSpaceForLazyDeopt() {
  int space_needed = Deoptimizer::patch_size();
  if (!info()->IsStub()) {
    // Ensure that we have enough space after the previous lazy-bailout
    // instruction for patching the code here.
    int current_pc = masm()->pc_offset();
    if (current_pc < last_lazy_deopt_pc_ + space_needed) {
      int padding_size = last_lazy_deopt_pc_ + space_needed - current_pc;
      __ Nop(padding_size);
    }
  }
}

#undef __

}  // namespace compiler
}  // namespace internal
}  // namespace v8
