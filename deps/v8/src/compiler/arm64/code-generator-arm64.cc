// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/code-generator.h"

#include "src/arm64/macro-assembler-arm64.h"
#include "src/compiler/code-generator-impl.h"
#include "src/compiler/gap-resolver.h"
#include "src/compiler/node-matchers.h"
#include "src/compiler/node-properties-inl.h"
#include "src/scopes.h"

namespace v8 {
namespace internal {
namespace compiler {

#define __ masm()->


// Adds Arm64-specific methods to convert InstructionOperands.
class Arm64OperandConverter FINAL : public InstructionOperandConverter {
 public:
  Arm64OperandConverter(CodeGenerator* gen, Instruction* instr)
      : InstructionOperandConverter(gen, instr) {}

  Register InputRegister32(int index) {
    return ToRegister(instr_->InputAt(index)).W();
  }

  Register InputRegister64(int index) { return InputRegister(index); }

  Operand InputImmediate(int index) {
    return ToImmediate(instr_->InputAt(index));
  }

  Operand InputOperand(int index) { return ToOperand(instr_->InputAt(index)); }

  Operand InputOperand64(int index) { return InputOperand(index); }

  Operand InputOperand32(int index) {
    return ToOperand32(instr_->InputAt(index));
  }

  Register OutputRegister64() { return OutputRegister(); }

  Register OutputRegister32() { return ToRegister(instr_->Output()).W(); }

  Operand InputOperand2_32(int index) {
    switch (AddressingModeField::decode(instr_->opcode())) {
      case kMode_None:
        return InputOperand32(index);
      case kMode_Operand2_R_LSL_I:
        return Operand(InputRegister32(index), LSL, InputInt5(index + 1));
      case kMode_Operand2_R_LSR_I:
        return Operand(InputRegister32(index), LSR, InputInt5(index + 1));
      case kMode_Operand2_R_ASR_I:
        return Operand(InputRegister32(index), ASR, InputInt5(index + 1));
      case kMode_Operand2_R_ROR_I:
        return Operand(InputRegister32(index), ROR, InputInt5(index + 1));
      case kMode_MRI:
      case kMode_MRR:
        break;
    }
    UNREACHABLE();
    return Operand(-1);
  }

  Operand InputOperand2_64(int index) {
    switch (AddressingModeField::decode(instr_->opcode())) {
      case kMode_None:
        return InputOperand64(index);
      case kMode_Operand2_R_LSL_I:
        return Operand(InputRegister64(index), LSL, InputInt6(index + 1));
      case kMode_Operand2_R_LSR_I:
        return Operand(InputRegister64(index), LSR, InputInt6(index + 1));
      case kMode_Operand2_R_ASR_I:
        return Operand(InputRegister64(index), ASR, InputInt6(index + 1));
      case kMode_Operand2_R_ROR_I:
        return Operand(InputRegister64(index), ROR, InputInt6(index + 1));
      case kMode_MRI:
      case kMode_MRR:
        break;
    }
    UNREACHABLE();
    return Operand(-1);
  }

  MemOperand MemoryOperand(int* first_index) {
    const int index = *first_index;
    switch (AddressingModeField::decode(instr_->opcode())) {
      case kMode_None:
      case kMode_Operand2_R_LSL_I:
      case kMode_Operand2_R_LSR_I:
      case kMode_Operand2_R_ASR_I:
      case kMode_Operand2_R_ROR_I:
        break;
      case kMode_MRI:
        *first_index += 2;
        return MemOperand(InputRegister(index + 0), InputInt32(index + 1));
      case kMode_MRR:
        *first_index += 2;
        return MemOperand(InputRegister(index + 0), InputRegister(index + 1));
    }
    UNREACHABLE();
    return MemOperand(no_reg);
  }

  MemOperand MemoryOperand() {
    int index = 0;
    return MemoryOperand(&index);
  }

  Operand ToOperand(InstructionOperand* op) {
    if (op->IsRegister()) {
      return Operand(ToRegister(op));
    }
    return ToImmediate(op);
  }

  Operand ToOperand32(InstructionOperand* op) {
    if (op->IsRegister()) {
      return Operand(ToRegister(op).W());
    }
    return ToImmediate(op);
  }

  Operand ToImmediate(InstructionOperand* operand) {
    Constant constant = ToConstant(operand);
    switch (constant.type()) {
      case Constant::kInt32:
        return Operand(constant.ToInt32());
      case Constant::kInt64:
        return Operand(constant.ToInt64());
      case Constant::kFloat32:
        return Operand(
            isolate()->factory()->NewNumber(constant.ToFloat32(), TENURED));
      case Constant::kFloat64:
        return Operand(
            isolate()->factory()->NewNumber(constant.ToFloat64(), TENURED));
      case Constant::kExternalReference:
        return Operand(constant.ToExternalReference());
      case Constant::kHeapObject:
        return Operand(constant.ToHeapObject());
    }
    UNREACHABLE();
    return Operand(-1);
  }

  MemOperand ToMemOperand(InstructionOperand* op, MacroAssembler* masm) const {
    DCHECK(op != NULL);
    DCHECK(!op->IsRegister());
    DCHECK(!op->IsDoubleRegister());
    DCHECK(op->IsStackSlot() || op->IsDoubleStackSlot());
    // The linkage computes where all spill slots are located.
    FrameOffset offset = linkage()->GetFrameOffset(op->index(), frame(), 0);
    return MemOperand(offset.from_stack_pointer() ? masm->StackPointer() : fp,
                      offset.offset());
  }
};


#define ASSEMBLE_SHIFT(asm_instr, width)                                       \
  do {                                                                         \
    if (instr->InputAt(1)->IsRegister()) {                                     \
      __ asm_instr(i.OutputRegister##width(), i.InputRegister##width(0),       \
                   i.InputRegister##width(1));                                 \
    } else {                                                                   \
      int64_t imm = i.InputOperand##width(1).immediate().value();              \
      __ asm_instr(i.OutputRegister##width(), i.InputRegister##width(0), imm); \
    }                                                                          \
  } while (0)


#define ASSEMBLE_TEST_AND_BRANCH(asm_instr, width)           \
  do {                                                       \
    bool fallthrough = IsNextInAssemblyOrder(i.InputRpo(3)); \
    __ asm_instr(i.InputRegister##width(0), i.InputInt6(1),  \
                 GetLabel(i.InputRpo(2)));                   \
    if (!fallthrough) __ B(GetLabel(i.InputRpo(3)));         \
  } while (0)


// Assembles an instruction after register allocation, producing machine code.
void CodeGenerator::AssembleArchInstruction(Instruction* instr) {
  Arm64OperandConverter i(this, instr);
  InstructionCode opcode = instr->opcode();
  switch (ArchOpcodeField::decode(opcode)) {
    case kArchCallCodeObject: {
      EnsureSpaceForLazyDeopt();
      if (instr->InputAt(0)->IsImmediate()) {
        __ Call(Handle<Code>::cast(i.InputHeapObject(0)),
                RelocInfo::CODE_TARGET);
      } else {
        Register target = i.InputRegister(0);
        __ Add(target, target, Code::kHeaderSize - kHeapObjectTag);
        __ Call(target);
      }
      AddSafepointAndDeopt(instr);
      break;
    }
    case kArchCallJSFunction: {
      EnsureSpaceForLazyDeopt();
      Register func = i.InputRegister(0);
      if (FLAG_debug_code) {
        // Check the function's context matches the context argument.
        UseScratchRegisterScope scope(masm());
        Register temp = scope.AcquireX();
        __ Ldr(temp, FieldMemOperand(func, JSFunction::kContextOffset));
        __ cmp(cp, temp);
        __ Assert(eq, kWrongFunctionContext);
      }
      __ Ldr(x10, FieldMemOperand(func, JSFunction::kCodeEntryOffset));
      __ Call(x10);
      AddSafepointAndDeopt(instr);
      break;
    }
    case kArchJmp:
      __ B(GetLabel(i.InputRpo(0)));
      break;
    case kArchNop:
      // don't emit code for nops.
      break;
    case kArchRet:
      AssembleReturn();
      break;
    case kArchStackPointer:
      __ mov(i.OutputRegister(), masm()->StackPointer());
      break;
    case kArchTruncateDoubleToI:
      __ TruncateDoubleToI(i.OutputRegister(), i.InputDoubleRegister(0));
      break;
    case kArm64Float64Ceil:
      __ Frintp(i.OutputDoubleRegister(), i.InputDoubleRegister(0));
      break;
    case kArm64Float64Floor:
      __ Frintm(i.OutputDoubleRegister(), i.InputDoubleRegister(0));
      break;
    case kArm64Float64RoundTruncate:
      __ Frintz(i.OutputDoubleRegister(), i.InputDoubleRegister(0));
      break;
    case kArm64Float64RoundTiesAway:
      __ Frinta(i.OutputDoubleRegister(), i.InputDoubleRegister(0));
      break;
    case kArm64Add:
      __ Add(i.OutputRegister(), i.InputRegister(0), i.InputOperand2_64(1));
      break;
    case kArm64Add32:
      if (FlagsModeField::decode(opcode) != kFlags_none) {
        __ Adds(i.OutputRegister32(), i.InputRegister32(0),
                i.InputOperand2_32(1));
      } else {
        __ Add(i.OutputRegister32(), i.InputRegister32(0),
               i.InputOperand2_32(1));
      }
      break;
    case kArm64And:
      __ And(i.OutputRegister(), i.InputRegister(0), i.InputOperand2_64(1));
      break;
    case kArm64And32:
      __ And(i.OutputRegister32(), i.InputRegister32(0), i.InputOperand2_32(1));
      break;
    case kArm64Bic:
      __ Bic(i.OutputRegister(), i.InputRegister(0), i.InputOperand2_64(1));
      break;
    case kArm64Bic32:
      __ Bic(i.OutputRegister32(), i.InputRegister32(0), i.InputOperand2_32(1));
      break;
    case kArm64Mul:
      __ Mul(i.OutputRegister(), i.InputRegister(0), i.InputRegister(1));
      break;
    case kArm64Mul32:
      __ Mul(i.OutputRegister32(), i.InputRegister32(0), i.InputRegister32(1));
      break;
    case kArm64Smull:
      __ Smull(i.OutputRegister(), i.InputRegister32(0), i.InputRegister32(1));
      break;
    case kArm64Umull:
      __ Umull(i.OutputRegister(), i.InputRegister32(0), i.InputRegister32(1));
      break;
    case kArm64Madd:
      __ Madd(i.OutputRegister(), i.InputRegister(0), i.InputRegister(1),
              i.InputRegister(2));
      break;
    case kArm64Madd32:
      __ Madd(i.OutputRegister32(), i.InputRegister32(0), i.InputRegister32(1),
              i.InputRegister32(2));
      break;
    case kArm64Msub:
      __ Msub(i.OutputRegister(), i.InputRegister(0), i.InputRegister(1),
              i.InputRegister(2));
      break;
    case kArm64Msub32:
      __ Msub(i.OutputRegister32(), i.InputRegister32(0), i.InputRegister32(1),
              i.InputRegister32(2));
      break;
    case kArm64Mneg:
      __ Mneg(i.OutputRegister(), i.InputRegister(0), i.InputRegister(1));
      break;
    case kArm64Mneg32:
      __ Mneg(i.OutputRegister32(), i.InputRegister32(0), i.InputRegister32(1));
      break;
    case kArm64Idiv:
      __ Sdiv(i.OutputRegister(), i.InputRegister(0), i.InputRegister(1));
      break;
    case kArm64Idiv32:
      __ Sdiv(i.OutputRegister32(), i.InputRegister32(0), i.InputRegister32(1));
      break;
    case kArm64Udiv:
      __ Udiv(i.OutputRegister(), i.InputRegister(0), i.InputRegister(1));
      break;
    case kArm64Udiv32:
      __ Udiv(i.OutputRegister32(), i.InputRegister32(0), i.InputRegister32(1));
      break;
    case kArm64Imod: {
      UseScratchRegisterScope scope(masm());
      Register temp = scope.AcquireX();
      __ Sdiv(temp, i.InputRegister(0), i.InputRegister(1));
      __ Msub(i.OutputRegister(), temp, i.InputRegister(1), i.InputRegister(0));
      break;
    }
    case kArm64Imod32: {
      UseScratchRegisterScope scope(masm());
      Register temp = scope.AcquireW();
      __ Sdiv(temp, i.InputRegister32(0), i.InputRegister32(1));
      __ Msub(i.OutputRegister32(), temp, i.InputRegister32(1),
              i.InputRegister32(0));
      break;
    }
    case kArm64Umod: {
      UseScratchRegisterScope scope(masm());
      Register temp = scope.AcquireX();
      __ Udiv(temp, i.InputRegister(0), i.InputRegister(1));
      __ Msub(i.OutputRegister(), temp, i.InputRegister(1), i.InputRegister(0));
      break;
    }
    case kArm64Umod32: {
      UseScratchRegisterScope scope(masm());
      Register temp = scope.AcquireW();
      __ Udiv(temp, i.InputRegister32(0), i.InputRegister32(1));
      __ Msub(i.OutputRegister32(), temp, i.InputRegister32(1),
              i.InputRegister32(0));
      break;
    }
    // TODO(dcarney): use mvn instr??
    case kArm64Not:
      __ Orn(i.OutputRegister(), xzr, i.InputOperand(0));
      break;
    case kArm64Not32:
      __ Orn(i.OutputRegister32(), wzr, i.InputOperand32(0));
      break;
    case kArm64Neg:
      __ Neg(i.OutputRegister(), i.InputOperand(0));
      break;
    case kArm64Neg32:
      __ Neg(i.OutputRegister32(), i.InputOperand32(0));
      break;
    case kArm64Or:
      __ Orr(i.OutputRegister(), i.InputRegister(0), i.InputOperand2_64(1));
      break;
    case kArm64Or32:
      __ Orr(i.OutputRegister32(), i.InputRegister32(0), i.InputOperand2_32(1));
      break;
    case kArm64Orn:
      __ Orn(i.OutputRegister(), i.InputRegister(0), i.InputOperand2_64(1));
      break;
    case kArm64Orn32:
      __ Orn(i.OutputRegister32(), i.InputRegister32(0), i.InputOperand2_32(1));
      break;
    case kArm64Eor:
      __ Eor(i.OutputRegister(), i.InputRegister(0), i.InputOperand2_64(1));
      break;
    case kArm64Eor32:
      __ Eor(i.OutputRegister32(), i.InputRegister32(0), i.InputOperand2_32(1));
      break;
    case kArm64Eon:
      __ Eon(i.OutputRegister(), i.InputRegister(0), i.InputOperand2_64(1));
      break;
    case kArm64Eon32:
      __ Eon(i.OutputRegister32(), i.InputRegister32(0), i.InputOperand2_32(1));
      break;
    case kArm64Sub:
      __ Sub(i.OutputRegister(), i.InputRegister(0), i.InputOperand2_64(1));
      break;
    case kArm64Sub32:
      if (FlagsModeField::decode(opcode) != kFlags_none) {
        __ Subs(i.OutputRegister32(), i.InputRegister32(0),
                i.InputOperand2_32(1));
      } else {
        __ Sub(i.OutputRegister32(), i.InputRegister32(0),
               i.InputOperand2_32(1));
      }
      break;
    case kArm64Lsl:
      ASSEMBLE_SHIFT(Lsl, 64);
      break;
    case kArm64Lsl32:
      ASSEMBLE_SHIFT(Lsl, 32);
      break;
    case kArm64Lsr:
      ASSEMBLE_SHIFT(Lsr, 64);
      break;
    case kArm64Lsr32:
      ASSEMBLE_SHIFT(Lsr, 32);
      break;
    case kArm64Asr:
      ASSEMBLE_SHIFT(Asr, 64);
      break;
    case kArm64Asr32:
      ASSEMBLE_SHIFT(Asr, 32);
      break;
    case kArm64Ror:
      ASSEMBLE_SHIFT(Ror, 64);
      break;
    case kArm64Ror32:
      ASSEMBLE_SHIFT(Ror, 32);
      break;
    case kArm64Mov32:
      __ Mov(i.OutputRegister32(), i.InputRegister32(0));
      break;
    case kArm64Sxtw:
      __ Sxtw(i.OutputRegister(), i.InputRegister32(0));
      break;
    case kArm64Ubfx:
      __ Ubfx(i.OutputRegister(), i.InputRegister(0), i.InputInt8(1),
              i.InputInt8(2));
      break;
    case kArm64Ubfx32:
      __ Ubfx(i.OutputRegister32(), i.InputRegister32(0), i.InputInt8(1),
              i.InputInt8(2));
      break;
    case kArm64Tbz:
      ASSEMBLE_TEST_AND_BRANCH(Tbz, 64);
      break;
    case kArm64Tbz32:
      ASSEMBLE_TEST_AND_BRANCH(Tbz, 32);
      break;
    case kArm64Tbnz:
      ASSEMBLE_TEST_AND_BRANCH(Tbnz, 64);
      break;
    case kArm64Tbnz32:
      ASSEMBLE_TEST_AND_BRANCH(Tbnz, 32);
      break;
    case kArm64Claim: {
      int words = MiscField::decode(instr->opcode());
      __ Claim(words);
      break;
    }
    case kArm64Poke: {
      int slot = MiscField::decode(instr->opcode());
      Operand operand(slot * kPointerSize);
      __ Poke(i.InputRegister(0), operand);
      break;
    }
    case kArm64PokePairZero: {
      // TODO(dcarney): test slot offset and register order.
      int slot = MiscField::decode(instr->opcode()) - 1;
      __ PokePair(i.InputRegister(0), xzr, slot * kPointerSize);
      break;
    }
    case kArm64PokePair: {
      int slot = MiscField::decode(instr->opcode()) - 1;
      __ PokePair(i.InputRegister(1), i.InputRegister(0), slot * kPointerSize);
      break;
    }
    case kArm64Cmp:
      __ Cmp(i.InputRegister(0), i.InputOperand(1));
      break;
    case kArm64Cmp32:
      __ Cmp(i.InputRegister32(0), i.InputOperand32(1));
      break;
    case kArm64Cmn:
      __ Cmn(i.InputRegister(0), i.InputOperand(1));
      break;
    case kArm64Cmn32:
      __ Cmn(i.InputRegister32(0), i.InputOperand32(1));
      break;
    case kArm64Tst:
      __ Tst(i.InputRegister(0), i.InputOperand(1));
      break;
    case kArm64Tst32:
      __ Tst(i.InputRegister32(0), i.InputOperand32(1));
      break;
    case kArm64Float64Cmp:
      __ Fcmp(i.InputDoubleRegister(0), i.InputDoubleRegister(1));
      break;
    case kArm64Float64Add:
      __ Fadd(i.OutputDoubleRegister(), i.InputDoubleRegister(0),
              i.InputDoubleRegister(1));
      break;
    case kArm64Float64Sub:
      __ Fsub(i.OutputDoubleRegister(), i.InputDoubleRegister(0),
              i.InputDoubleRegister(1));
      break;
    case kArm64Float64Mul:
      __ Fmul(i.OutputDoubleRegister(), i.InputDoubleRegister(0),
              i.InputDoubleRegister(1));
      break;
    case kArm64Float64Div:
      __ Fdiv(i.OutputDoubleRegister(), i.InputDoubleRegister(0),
              i.InputDoubleRegister(1));
      break;
    case kArm64Float64Mod: {
      // TODO(dcarney): implement directly. See note in lithium-codegen-arm64.cc
      FrameScope scope(masm(), StackFrame::MANUAL);
      DCHECK(d0.is(i.InputDoubleRegister(0)));
      DCHECK(d1.is(i.InputDoubleRegister(1)));
      DCHECK(d0.is(i.OutputDoubleRegister()));
      // TODO(dcarney): make sure this saves all relevant registers.
      __ CallCFunction(ExternalReference::mod_two_doubles_operation(isolate()),
                       0, 2);
      break;
    }
    case kArm64Float64Sqrt:
      __ Fsqrt(i.OutputDoubleRegister(), i.InputDoubleRegister(0));
      break;
    case kArm64Float32ToFloat64:
      __ Fcvt(i.OutputDoubleRegister(), i.InputDoubleRegister(0).S());
      break;
    case kArm64Float64ToFloat32:
      __ Fcvt(i.OutputDoubleRegister().S(), i.InputDoubleRegister(0));
      break;
    case kArm64Float64ToInt32:
      __ Fcvtzs(i.OutputRegister32(), i.InputDoubleRegister(0));
      break;
    case kArm64Float64ToUint32:
      __ Fcvtzu(i.OutputRegister32(), i.InputDoubleRegister(0));
      break;
    case kArm64Int32ToFloat64:
      __ Scvtf(i.OutputDoubleRegister(), i.InputRegister32(0));
      break;
    case kArm64Uint32ToFloat64:
      __ Ucvtf(i.OutputDoubleRegister(), i.InputRegister32(0));
      break;
    case kArm64Ldrb:
      __ Ldrb(i.OutputRegister(), i.MemoryOperand());
      break;
    case kArm64Ldrsb:
      __ Ldrsb(i.OutputRegister(), i.MemoryOperand());
      break;
    case kArm64Strb:
      __ Strb(i.InputRegister(2), i.MemoryOperand());
      break;
    case kArm64Ldrh:
      __ Ldrh(i.OutputRegister(), i.MemoryOperand());
      break;
    case kArm64Ldrsh:
      __ Ldrsh(i.OutputRegister(), i.MemoryOperand());
      break;
    case kArm64Strh:
      __ Strh(i.InputRegister(2), i.MemoryOperand());
      break;
    case kArm64LdrW:
      __ Ldr(i.OutputRegister32(), i.MemoryOperand());
      break;
    case kArm64StrW:
      __ Str(i.InputRegister32(2), i.MemoryOperand());
      break;
    case kArm64Ldr:
      __ Ldr(i.OutputRegister(), i.MemoryOperand());
      break;
    case kArm64Str:
      __ Str(i.InputRegister(2), i.MemoryOperand());
      break;
    case kArm64LdrS:
      __ Ldr(i.OutputDoubleRegister().S(), i.MemoryOperand());
      break;
    case kArm64StrS:
      __ Str(i.InputDoubleRegister(2).S(), i.MemoryOperand());
      break;
    case kArm64LdrD:
      __ Ldr(i.OutputDoubleRegister(), i.MemoryOperand());
      break;
    case kArm64StrD:
      __ Str(i.InputDoubleRegister(2), i.MemoryOperand());
      break;
    case kArm64StoreWriteBarrier: {
      Register object = i.InputRegister(0);
      Register index = i.InputRegister(1);
      Register value = i.InputRegister(2);
      __ Add(index, object, Operand(index, SXTW));
      __ Str(value, MemOperand(index));
      SaveFPRegsMode mode =
          frame()->DidAllocateDoubleRegisters() ? kSaveFPRegs : kDontSaveFPRegs;
      // TODO(dcarney): we shouldn't test write barriers from c calls.
      LinkRegisterStatus lr_status = kLRHasNotBeenSaved;
      UseScratchRegisterScope scope(masm());
      Register temp = no_reg;
      if (csp.is(masm()->StackPointer())) {
        temp = scope.AcquireX();
        lr_status = kLRHasBeenSaved;
        __ Push(lr, temp);  // Need to push a pair
      }
      __ RecordWrite(object, index, value, lr_status, mode);
      if (csp.is(masm()->StackPointer())) {
        __ Pop(temp, lr);
      }
      break;
    }
  }
}


// Assemble branches after this instruction.
void CodeGenerator::AssembleArchBranch(Instruction* instr,
                                       FlagsCondition condition) {
  Arm64OperandConverter i(this, instr);
  Label done;

  // Emit a branch. The true and false targets are always the last two inputs
  // to the instruction.
  BasicBlock::RpoNumber tblock =
      i.InputRpo(static_cast<int>(instr->InputCount()) - 2);
  BasicBlock::RpoNumber fblock =
      i.InputRpo(static_cast<int>(instr->InputCount()) - 1);
  bool fallthru = IsNextInAssemblyOrder(fblock);
  Label* tlabel = GetLabel(tblock);
  Label* flabel = fallthru ? &done : GetLabel(fblock);
  switch (condition) {
    case kUnorderedEqual:
      __ B(vs, flabel);
    // Fall through.
    case kEqual:
      __ B(eq, tlabel);
      break;
    case kUnorderedNotEqual:
      __ B(vs, tlabel);
    // Fall through.
    case kNotEqual:
      __ B(ne, tlabel);
      break;
    case kSignedLessThan:
      __ B(lt, tlabel);
      break;
    case kSignedGreaterThanOrEqual:
      __ B(ge, tlabel);
      break;
    case kSignedLessThanOrEqual:
      __ B(le, tlabel);
      break;
    case kSignedGreaterThan:
      __ B(gt, tlabel);
      break;
    case kUnorderedLessThan:
      __ B(vs, flabel);
    // Fall through.
    case kUnsignedLessThan:
      __ B(lo, tlabel);
      break;
    case kUnorderedGreaterThanOrEqual:
      __ B(vs, tlabel);
    // Fall through.
    case kUnsignedGreaterThanOrEqual:
      __ B(hs, tlabel);
      break;
    case kUnorderedLessThanOrEqual:
      __ B(vs, flabel);
    // Fall through.
    case kUnsignedLessThanOrEqual:
      __ B(ls, tlabel);
      break;
    case kUnorderedGreaterThan:
      __ B(vs, tlabel);
    // Fall through.
    case kUnsignedGreaterThan:
      __ B(hi, tlabel);
      break;
    case kOverflow:
      __ B(vs, tlabel);
      break;
    case kNotOverflow:
      __ B(vc, tlabel);
      break;
  }
  if (!fallthru) __ B(flabel);  // no fallthru to flabel.
  __ Bind(&done);
}


// Assemble boolean materializations after this instruction.
void CodeGenerator::AssembleArchBoolean(Instruction* instr,
                                        FlagsCondition condition) {
  Arm64OperandConverter i(this, instr);
  Label done;

  // Materialize a full 64-bit 1 or 0 value. The result register is always the
  // last output of the instruction.
  Label check;
  DCHECK_NE(0, instr->OutputCount());
  Register reg = i.OutputRegister(instr->OutputCount() - 1);
  Condition cc = nv;
  switch (condition) {
    case kUnorderedEqual:
      __ B(vc, &check);
      __ Mov(reg, 0);
      __ B(&done);
    // Fall through.
    case kEqual:
      cc = eq;
      break;
    case kUnorderedNotEqual:
      __ B(vc, &check);
      __ Mov(reg, 1);
      __ B(&done);
    // Fall through.
    case kNotEqual:
      cc = ne;
      break;
    case kSignedLessThan:
      cc = lt;
      break;
    case kSignedGreaterThanOrEqual:
      cc = ge;
      break;
    case kSignedLessThanOrEqual:
      cc = le;
      break;
    case kSignedGreaterThan:
      cc = gt;
      break;
    case kUnorderedLessThan:
      __ B(vc, &check);
      __ Mov(reg, 0);
      __ B(&done);
    // Fall through.
    case kUnsignedLessThan:
      cc = lo;
      break;
    case kUnorderedGreaterThanOrEqual:
      __ B(vc, &check);
      __ Mov(reg, 1);
      __ B(&done);
    // Fall through.
    case kUnsignedGreaterThanOrEqual:
      cc = hs;
      break;
    case kUnorderedLessThanOrEqual:
      __ B(vc, &check);
      __ Mov(reg, 0);
      __ B(&done);
    // Fall through.
    case kUnsignedLessThanOrEqual:
      cc = ls;
      break;
    case kUnorderedGreaterThan:
      __ B(vc, &check);
      __ Mov(reg, 1);
      __ B(&done);
    // Fall through.
    case kUnsignedGreaterThan:
      cc = hi;
      break;
    case kOverflow:
      cc = vs;
      break;
    case kNotOverflow:
      cc = vc;
      break;
  }
  __ bind(&check);
  __ Cset(reg, cc);
  __ Bind(&done);
}


void CodeGenerator::AssembleDeoptimizerCall(int deoptimization_id) {
  Address deopt_entry = Deoptimizer::GetDeoptimizationEntry(
      isolate(), deoptimization_id, Deoptimizer::LAZY);
  __ Call(deopt_entry, RelocInfo::RUNTIME_ENTRY);
}


// TODO(dcarney): increase stack slots in frame once before first use.
static int AlignedStackSlots(int stack_slots) {
  if (stack_slots & 1) stack_slots++;
  return stack_slots;
}


void CodeGenerator::AssemblePrologue() {
  CallDescriptor* descriptor = linkage()->GetIncomingDescriptor();
  if (descriptor->kind() == CallDescriptor::kCallAddress) {
    __ SetStackPointer(csp);
    __ Push(lr, fp);
    __ Mov(fp, csp);
    // TODO(dcarney): correct callee saved registers.
    __ PushCalleeSavedRegisters();
    frame()->SetRegisterSaveAreaSize(20 * kPointerSize);
  } else if (descriptor->IsJSFunctionCall()) {
    CompilationInfo* info = this->info();
    __ SetStackPointer(jssp);
    __ Prologue(info->IsCodePreAgingActive());
    frame()->SetRegisterSaveAreaSize(
        StandardFrameConstants::kFixedFrameSizeFromFp);

    // Sloppy mode functions and builtins need to replace the receiver with the
    // global proxy when called as functions (without an explicit receiver
    // object).
    // TODO(mstarzinger/verwaest): Should this be moved back into the CallIC?
    if (info->strict_mode() == SLOPPY && !info->is_native()) {
      Label ok;
      // +2 for return address and saved frame pointer.
      int receiver_slot = info->scope()->num_parameters() + 2;
      __ Ldr(x10, MemOperand(fp, receiver_slot * kXRegSize));
      __ JumpIfNotRoot(x10, Heap::kUndefinedValueRootIndex, &ok);
      __ Ldr(x10, GlobalObjectMemOperand());
      __ Ldr(x10, FieldMemOperand(x10, GlobalObject::kGlobalProxyOffset));
      __ Str(x10, MemOperand(fp, receiver_slot * kXRegSize));
      __ Bind(&ok);
    }

  } else {
    __ SetStackPointer(jssp);
    __ StubPrologue();
    frame()->SetRegisterSaveAreaSize(
        StandardFrameConstants::kFixedFrameSizeFromFp);
  }
  int stack_slots = frame()->GetSpillSlotCount();
  if (stack_slots > 0) {
    Register sp = __ StackPointer();
    if (!sp.Is(csp)) {
      __ Sub(sp, sp, stack_slots * kPointerSize);
    }
    __ Sub(csp, csp, AlignedStackSlots(stack_slots) * kPointerSize);
  }
}


void CodeGenerator::AssembleReturn() {
  CallDescriptor* descriptor = linkage()->GetIncomingDescriptor();
  if (descriptor->kind() == CallDescriptor::kCallAddress) {
    if (frame()->GetRegisterSaveAreaSize() > 0) {
      // Remove this frame's spill slots first.
      int stack_slots = frame()->GetSpillSlotCount();
      if (stack_slots > 0) {
        __ Add(csp, csp, AlignedStackSlots(stack_slots) * kPointerSize);
      }
      // Restore registers.
      // TODO(dcarney): correct callee saved registers.
      __ PopCalleeSavedRegisters();
    }
    __ Mov(csp, fp);
    __ Pop(fp, lr);
    __ Ret();
  } else {
    __ Mov(jssp, fp);
    __ Pop(fp, lr);
    int pop_count = descriptor->IsJSFunctionCall()
                        ? static_cast<int>(descriptor->JSParameterCount())
                        : 0;
    __ Drop(pop_count);
    __ Ret();
  }
}


void CodeGenerator::AssembleMove(InstructionOperand* source,
                                 InstructionOperand* destination) {
  Arm64OperandConverter g(this, NULL);
  // Dispatch on the source and destination operand kinds.  Not all
  // combinations are possible.
  if (source->IsRegister()) {
    DCHECK(destination->IsRegister() || destination->IsStackSlot());
    Register src = g.ToRegister(source);
    if (destination->IsRegister()) {
      __ Mov(g.ToRegister(destination), src);
    } else {
      __ Str(src, g.ToMemOperand(destination, masm()));
    }
  } else if (source->IsStackSlot()) {
    MemOperand src = g.ToMemOperand(source, masm());
    DCHECK(destination->IsRegister() || destination->IsStackSlot());
    if (destination->IsRegister()) {
      __ Ldr(g.ToRegister(destination), src);
    } else {
      UseScratchRegisterScope scope(masm());
      Register temp = scope.AcquireX();
      __ Ldr(temp, src);
      __ Str(temp, g.ToMemOperand(destination, masm()));
    }
  } else if (source->IsConstant()) {
    Constant src = g.ToConstant(ConstantOperand::cast(source));
    if (destination->IsRegister() || destination->IsStackSlot()) {
      UseScratchRegisterScope scope(masm());
      Register dst = destination->IsRegister() ? g.ToRegister(destination)
                                               : scope.AcquireX();
      if (src.type() == Constant::kHeapObject) {
        __ LoadObject(dst, src.ToHeapObject());
      } else {
        __ Mov(dst, g.ToImmediate(source));
      }
      if (destination->IsStackSlot()) {
        __ Str(dst, g.ToMemOperand(destination, masm()));
      }
    } else if (src.type() == Constant::kFloat32) {
      if (destination->IsDoubleRegister()) {
        FPRegister dst = g.ToDoubleRegister(destination).S();
        __ Fmov(dst, src.ToFloat32());
      } else {
        DCHECK(destination->IsDoubleStackSlot());
        UseScratchRegisterScope scope(masm());
        FPRegister temp = scope.AcquireS();
        __ Fmov(temp, src.ToFloat32());
        __ Str(temp, g.ToMemOperand(destination, masm()));
      }
    } else {
      DCHECK_EQ(Constant::kFloat64, src.type());
      if (destination->IsDoubleRegister()) {
        FPRegister dst = g.ToDoubleRegister(destination);
        __ Fmov(dst, src.ToFloat64());
      } else {
        DCHECK(destination->IsDoubleStackSlot());
        UseScratchRegisterScope scope(masm());
        FPRegister temp = scope.AcquireD();
        __ Fmov(temp, src.ToFloat64());
        __ Str(temp, g.ToMemOperand(destination, masm()));
      }
    }
  } else if (source->IsDoubleRegister()) {
    FPRegister src = g.ToDoubleRegister(source);
    if (destination->IsDoubleRegister()) {
      FPRegister dst = g.ToDoubleRegister(destination);
      __ Fmov(dst, src);
    } else {
      DCHECK(destination->IsDoubleStackSlot());
      __ Str(src, g.ToMemOperand(destination, masm()));
    }
  } else if (source->IsDoubleStackSlot()) {
    DCHECK(destination->IsDoubleRegister() || destination->IsDoubleStackSlot());
    MemOperand src = g.ToMemOperand(source, masm());
    if (destination->IsDoubleRegister()) {
      __ Ldr(g.ToDoubleRegister(destination), src);
    } else {
      UseScratchRegisterScope scope(masm());
      FPRegister temp = scope.AcquireD();
      __ Ldr(temp, src);
      __ Str(temp, g.ToMemOperand(destination, masm()));
    }
  } else {
    UNREACHABLE();
  }
}


void CodeGenerator::AssembleSwap(InstructionOperand* source,
                                 InstructionOperand* destination) {
  Arm64OperandConverter g(this, NULL);
  // Dispatch on the source and destination operand kinds.  Not all
  // combinations are possible.
  if (source->IsRegister()) {
    // Register-register.
    UseScratchRegisterScope scope(masm());
    Register temp = scope.AcquireX();
    Register src = g.ToRegister(source);
    if (destination->IsRegister()) {
      Register dst = g.ToRegister(destination);
      __ Mov(temp, src);
      __ Mov(src, dst);
      __ Mov(dst, temp);
    } else {
      DCHECK(destination->IsStackSlot());
      MemOperand dst = g.ToMemOperand(destination, masm());
      __ Mov(temp, src);
      __ Ldr(src, dst);
      __ Str(temp, dst);
    }
  } else if (source->IsStackSlot() || source->IsDoubleStackSlot()) {
    UseScratchRegisterScope scope(masm());
    CPURegister temp_0 = scope.AcquireX();
    CPURegister temp_1 = scope.AcquireX();
    MemOperand src = g.ToMemOperand(source, masm());
    MemOperand dst = g.ToMemOperand(destination, masm());
    __ Ldr(temp_0, src);
    __ Ldr(temp_1, dst);
    __ Str(temp_0, dst);
    __ Str(temp_1, src);
  } else if (source->IsDoubleRegister()) {
    UseScratchRegisterScope scope(masm());
    FPRegister temp = scope.AcquireD();
    FPRegister src = g.ToDoubleRegister(source);
    if (destination->IsDoubleRegister()) {
      FPRegister dst = g.ToDoubleRegister(destination);
      __ Fmov(temp, src);
      __ Fmov(src, dst);
      __ Fmov(dst, temp);
    } else {
      DCHECK(destination->IsDoubleStackSlot());
      MemOperand dst = g.ToMemOperand(destination, masm());
      __ Fmov(temp, src);
      __ Ldr(src, dst);
      __ Str(temp, dst);
    }
  } else {
    // No other combinations are possible.
    UNREACHABLE();
  }
}


void CodeGenerator::AddNopForSmiCodeInlining() { __ movz(xzr, 0); }


void CodeGenerator::EnsureSpaceForLazyDeopt() {
  int space_needed = Deoptimizer::patch_size();
  if (!info()->IsStub()) {
    // Ensure that we have enough space after the previous lazy-bailout
    // instruction for patching the code here.
    intptr_t current_pc = masm()->pc_offset();

    if (current_pc < (last_lazy_deopt_pc_ + space_needed)) {
      intptr_t padding_size = last_lazy_deopt_pc_ + space_needed - current_pc;
      DCHECK((padding_size % kInstructionSize) == 0);
      InstructionAccurateScope instruction_accurate(
          masm(), padding_size / kInstructionSize);

      while (padding_size > 0) {
        __ nop();
        padding_size -= kInstructionSize;
      }
    }
  }
  MarkLazyDeoptSite();
}

#undef __

}  // namespace compiler
}  // namespace internal
}  // namespace v8
