// Copyright 2013 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/code-generator.h"

#include "src/compiler/code-generator-impl.h"
#include "src/compiler/gap-resolver.h"
#include "src/compiler/node-matchers.h"
#include "src/compiler/node-properties-inl.h"
#include "src/scopes.h"
#include "src/x64/assembler-x64.h"
#include "src/x64/macro-assembler-x64.h"

namespace v8 {
namespace internal {
namespace compiler {

#define __ masm()->


// TODO(turbofan): Cleanup these hacks.
enum Immediate64Type { kImm64Value, kImm64Handle, kImm64Reference };


struct Immediate64 {
  uint64_t value;
  Handle<Object> handle;
  ExternalReference reference;
  Immediate64Type type;
};


enum RegisterOrOperandType { kRegister, kDoubleRegister, kOperand };


struct RegisterOrOperand {
  RegisterOrOperand() : operand(no_reg, 0) {}
  Register reg;
  DoubleRegister double_reg;
  Operand operand;
  RegisterOrOperandType type;
};


// Adds X64 specific methods for decoding operands.
class X64OperandConverter : public InstructionOperandConverter {
 public:
  X64OperandConverter(CodeGenerator* gen, Instruction* instr)
      : InstructionOperandConverter(gen, instr) {}

  RegisterOrOperand InputRegisterOrOperand(int index) {
    return ToRegisterOrOperand(instr_->InputAt(index));
  }

  Immediate InputImmediate(int index) {
    return ToImmediate(instr_->InputAt(index));
  }

  RegisterOrOperand OutputRegisterOrOperand() {
    return ToRegisterOrOperand(instr_->Output());
  }

  Immediate64 InputImmediate64(int index) {
    return ToImmediate64(instr_->InputAt(index));
  }

  Immediate64 ToImmediate64(InstructionOperand* operand) {
    Constant constant = ToConstant(operand);
    Immediate64 immediate;
    immediate.value = 0xbeefdeaddeefbeed;
    immediate.type = kImm64Value;
    switch (constant.type()) {
      case Constant::kInt32:
      case Constant::kInt64:
        immediate.value = constant.ToInt64();
        return immediate;
      case Constant::kFloat32:
        immediate.type = kImm64Handle;
        immediate.handle =
            isolate()->factory()->NewNumber(constant.ToFloat32(), TENURED);
        return immediate;
      case Constant::kFloat64:
        immediate.type = kImm64Handle;
        immediate.handle =
            isolate()->factory()->NewNumber(constant.ToFloat64(), TENURED);
        return immediate;
      case Constant::kExternalReference:
        immediate.type = kImm64Reference;
        immediate.reference = constant.ToExternalReference();
        return immediate;
      case Constant::kHeapObject:
        immediate.type = kImm64Handle;
        immediate.handle = constant.ToHeapObject();
        return immediate;
    }
    UNREACHABLE();
    return immediate;
  }

  Immediate ToImmediate(InstructionOperand* operand) {
    Constant constant = ToConstant(operand);
    switch (constant.type()) {
      case Constant::kInt32:
        return Immediate(constant.ToInt32());
      case Constant::kInt64:
      case Constant::kFloat32:
      case Constant::kFloat64:
      case Constant::kExternalReference:
      case Constant::kHeapObject:
        break;
    }
    UNREACHABLE();
    return Immediate(-1);
  }

  Operand ToOperand(InstructionOperand* op, int extra = 0) {
    RegisterOrOperand result = ToRegisterOrOperand(op, extra);
    DCHECK_EQ(kOperand, result.type);
    return result.operand;
  }

  RegisterOrOperand ToRegisterOrOperand(InstructionOperand* op, int extra = 0) {
    RegisterOrOperand result;
    if (op->IsRegister()) {
      DCHECK(extra == 0);
      result.type = kRegister;
      result.reg = ToRegister(op);
      return result;
    } else if (op->IsDoubleRegister()) {
      DCHECK(extra == 0);
      DCHECK(extra == 0);
      result.type = kDoubleRegister;
      result.double_reg = ToDoubleRegister(op);
      return result;
    }

    DCHECK(op->IsStackSlot() || op->IsDoubleStackSlot());

    result.type = kOperand;
    // The linkage computes where all spill slots are located.
    FrameOffset offset = linkage()->GetFrameOffset(op->index(), frame(), extra);
    result.operand =
        Operand(offset.from_stack_pointer() ? rsp : rbp, offset.offset());
    return result;
  }

  static int NextOffset(int* offset) {
    int i = *offset;
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

  Operand MemoryOperand(int* offset) {
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
      case kMode_None:
        UNREACHABLE();
        return Operand(no_reg, 0);
    }
    UNREACHABLE();
    return Operand(no_reg, 0);
  }

  Operand MemoryOperand() {
    int first_input = 0;
    return MemoryOperand(&first_input);
  }
};


static bool HasImmediateInput(Instruction* instr, int index) {
  return instr->InputAt(index)->IsImmediate();
}


#define ASSEMBLE_BINOP(asm_instr)                            \
  do {                                                       \
    if (HasImmediateInput(instr, 1)) {                       \
      RegisterOrOperand input = i.InputRegisterOrOperand(0); \
      if (input.type == kRegister) {                         \
        __ asm_instr(input.reg, i.InputImmediate(1));        \
      } else {                                               \
        __ asm_instr(input.operand, i.InputImmediate(1));    \
      }                                                      \
    } else {                                                 \
      RegisterOrOperand input = i.InputRegisterOrOperand(1); \
      if (input.type == kRegister) {                         \
        __ asm_instr(i.InputRegister(0), input.reg);         \
      } else {                                               \
        __ asm_instr(i.InputRegister(0), input.operand);     \
      }                                                      \
    }                                                        \
  } while (0)


#define ASSEMBLE_SHIFT(asm_instr, width)                                 \
  do {                                                                   \
    if (HasImmediateInput(instr, 1)) {                                   \
      __ asm_instr(i.OutputRegister(), Immediate(i.InputInt##width(1))); \
    } else {                                                             \
      __ asm_instr##_cl(i.OutputRegister());                             \
    }                                                                    \
  } while (0)


// Assembles an instruction after register allocation, producing machine code.
void CodeGenerator::AssembleArchInstruction(Instruction* instr) {
  X64OperandConverter i(this, instr);

  switch (ArchOpcodeField::decode(instr->opcode())) {
    case kArchCallCodeObject: {
      EnsureSpaceForLazyDeopt();
      if (HasImmediateInput(instr, 0)) {
        Handle<Code> code = Handle<Code>::cast(i.InputHeapObject(0));
        __ Call(code, RelocInfo::CODE_TARGET);
      } else {
        Register reg = i.InputRegister(0);
        int entry = Code::kHeaderSize - kHeapObjectTag;
        __ Call(Operand(reg, entry));
      }
      AddSafepointAndDeopt(instr);
      break;
    }
    case kArchCallJSFunction: {
      EnsureSpaceForLazyDeopt();
      Register func = i.InputRegister(0);
      if (FLAG_debug_code) {
        // Check the function's context matches the context argument.
        __ cmpp(rsi, FieldOperand(func, JSFunction::kContextOffset));
        __ Assert(equal, kWrongFunctionContext);
      }
      __ Call(FieldOperand(func, JSFunction::kCodeEntryOffset));
      AddSafepointAndDeopt(instr);
      break;
    }
    case kArchJmp:
      __ jmp(code_->GetLabel(i.InputBlock(0)));
      break;
    case kArchNop:
      // don't emit code for nops.
      break;
    case kArchRet:
      AssembleReturn();
      break;
    case kArchTruncateDoubleToI:
      __ TruncateDoubleToI(i.OutputRegister(), i.InputDoubleRegister(0));
      break;
    case kX64Add32:
      ASSEMBLE_BINOP(addl);
      break;
    case kX64Add:
      ASSEMBLE_BINOP(addq);
      break;
    case kX64Sub32:
      ASSEMBLE_BINOP(subl);
      break;
    case kX64Sub:
      ASSEMBLE_BINOP(subq);
      break;
    case kX64And32:
      ASSEMBLE_BINOP(andl);
      break;
    case kX64And:
      ASSEMBLE_BINOP(andq);
      break;
    case kX64Cmp32:
      ASSEMBLE_BINOP(cmpl);
      break;
    case kX64Cmp:
      ASSEMBLE_BINOP(cmpq);
      break;
    case kX64Test32:
      ASSEMBLE_BINOP(testl);
      break;
    case kX64Test:
      ASSEMBLE_BINOP(testq);
      break;
    case kX64Imul32:
      if (HasImmediateInput(instr, 1)) {
        RegisterOrOperand input = i.InputRegisterOrOperand(0);
        if (input.type == kRegister) {
          __ imull(i.OutputRegister(), input.reg, i.InputImmediate(1));
        } else {
          __ imull(i.OutputRegister(), input.operand, i.InputImmediate(1));
        }
      } else {
        RegisterOrOperand input = i.InputRegisterOrOperand(1);
        if (input.type == kRegister) {
          __ imull(i.OutputRegister(), input.reg);
        } else {
          __ imull(i.OutputRegister(), input.operand);
        }
      }
      break;
    case kX64Imul:
      if (HasImmediateInput(instr, 1)) {
        RegisterOrOperand input = i.InputRegisterOrOperand(0);
        if (input.type == kRegister) {
          __ imulq(i.OutputRegister(), input.reg, i.InputImmediate(1));
        } else {
          __ imulq(i.OutputRegister(), input.operand, i.InputImmediate(1));
        }
      } else {
        RegisterOrOperand input = i.InputRegisterOrOperand(1);
        if (input.type == kRegister) {
          __ imulq(i.OutputRegister(), input.reg);
        } else {
          __ imulq(i.OutputRegister(), input.operand);
        }
      }
      break;
    case kX64Idiv32:
      __ cdq();
      __ idivl(i.InputRegister(1));
      break;
    case kX64Idiv:
      __ cqo();
      __ idivq(i.InputRegister(1));
      break;
    case kX64Udiv32:
      __ xorl(rdx, rdx);
      __ divl(i.InputRegister(1));
      break;
    case kX64Udiv:
      __ xorq(rdx, rdx);
      __ divq(i.InputRegister(1));
      break;
    case kX64Not: {
      RegisterOrOperand output = i.OutputRegisterOrOperand();
      if (output.type == kRegister) {
        __ notq(output.reg);
      } else {
        __ notq(output.operand);
      }
      break;
    }
    case kX64Not32: {
      RegisterOrOperand output = i.OutputRegisterOrOperand();
      if (output.type == kRegister) {
        __ notl(output.reg);
      } else {
        __ notl(output.operand);
      }
      break;
    }
    case kX64Neg: {
      RegisterOrOperand output = i.OutputRegisterOrOperand();
      if (output.type == kRegister) {
        __ negq(output.reg);
      } else {
        __ negq(output.operand);
      }
      break;
    }
    case kX64Neg32: {
      RegisterOrOperand output = i.OutputRegisterOrOperand();
      if (output.type == kRegister) {
        __ negl(output.reg);
      } else {
        __ negl(output.operand);
      }
      break;
    }
    case kX64Or32:
      ASSEMBLE_BINOP(orl);
      break;
    case kX64Or:
      ASSEMBLE_BINOP(orq);
      break;
    case kX64Xor32:
      ASSEMBLE_BINOP(xorl);
      break;
    case kX64Xor:
      ASSEMBLE_BINOP(xorq);
      break;
    case kX64Shl32:
      ASSEMBLE_SHIFT(shll, 5);
      break;
    case kX64Shl:
      ASSEMBLE_SHIFT(shlq, 6);
      break;
    case kX64Shr32:
      ASSEMBLE_SHIFT(shrl, 5);
      break;
    case kX64Shr:
      ASSEMBLE_SHIFT(shrq, 6);
      break;
    case kX64Sar32:
      ASSEMBLE_SHIFT(sarl, 5);
      break;
    case kX64Sar:
      ASSEMBLE_SHIFT(sarq, 6);
      break;
    case kX64Ror32:
      ASSEMBLE_SHIFT(rorl, 5);
      break;
    case kX64Ror:
      ASSEMBLE_SHIFT(rorq, 6);
      break;
    case kSSEFloat64Cmp: {
      RegisterOrOperand input = i.InputRegisterOrOperand(1);
      if (input.type == kDoubleRegister) {
        __ ucomisd(i.InputDoubleRegister(0), input.double_reg);
      } else {
        __ ucomisd(i.InputDoubleRegister(0), input.operand);
      }
      break;
    }
    case kSSEFloat64Add:
      __ addsd(i.InputDoubleRegister(0), i.InputDoubleRegister(1));
      break;
    case kSSEFloat64Sub:
      __ subsd(i.InputDoubleRegister(0), i.InputDoubleRegister(1));
      break;
    case kSSEFloat64Mul:
      __ mulsd(i.InputDoubleRegister(0), i.InputDoubleRegister(1));
      break;
    case kSSEFloat64Div:
      __ divsd(i.InputDoubleRegister(0), i.InputDoubleRegister(1));
      break;
    case kSSEFloat64Mod: {
      __ subq(rsp, Immediate(kDoubleSize));
      // Move values to st(0) and st(1).
      __ movsd(Operand(rsp, 0), i.InputDoubleRegister(1));
      __ fld_d(Operand(rsp, 0));
      __ movsd(Operand(rsp, 0), i.InputDoubleRegister(0));
      __ fld_d(Operand(rsp, 0));
      // Loop while fprem isn't done.
      Label mod_loop;
      __ bind(&mod_loop);
      // This instructions traps on all kinds inputs, but we are assuming the
      // floating point control word is set to ignore them all.
      __ fprem();
      // The following 2 instruction implicitly use rax.
      __ fnstsw_ax();
      if (CpuFeatures::IsSupported(SAHF) && masm()->IsEnabled(SAHF)) {
        __ sahf();
      } else {
        __ shrl(rax, Immediate(8));
        __ andl(rax, Immediate(0xFF));
        __ pushq(rax);
        __ popfq();
      }
      __ j(parity_even, &mod_loop);
      // Move output to stack and clean up.
      __ fstp(1);
      __ fstp_d(Operand(rsp, 0));
      __ movsd(i.OutputDoubleRegister(), Operand(rsp, 0));
      __ addq(rsp, Immediate(kDoubleSize));
      break;
    }
    case kSSEFloat64Sqrt: {
      RegisterOrOperand input = i.InputRegisterOrOperand(0);
      if (input.type == kDoubleRegister) {
        __ sqrtsd(i.OutputDoubleRegister(), input.double_reg);
      } else {
        __ sqrtsd(i.OutputDoubleRegister(), input.operand);
      }
      break;
    }
    case kSSECvtss2sd:
      __ cvtss2sd(i.OutputDoubleRegister(), i.InputDoubleRegister(0));
      break;
    case kSSECvtsd2ss:
      __ cvtsd2ss(i.OutputDoubleRegister(), i.InputDoubleRegister(0));
      break;
    case kSSEFloat64ToInt32: {
      RegisterOrOperand input = i.InputRegisterOrOperand(0);
      if (input.type == kDoubleRegister) {
        __ cvttsd2si(i.OutputRegister(), input.double_reg);
      } else {
        __ cvttsd2si(i.OutputRegister(), input.operand);
      }
      break;
    }
    case kSSEFloat64ToUint32: {
      RegisterOrOperand input = i.InputRegisterOrOperand(0);
      if (input.type == kDoubleRegister) {
        __ cvttsd2siq(i.OutputRegister(), input.double_reg);
      } else {
        __ cvttsd2siq(i.OutputRegister(), input.operand);
      }
      __ andl(i.OutputRegister(), i.OutputRegister());  // clear upper bits.
      // TODO(turbofan): generated code should not look at the upper 32 bits
      // of the result, but those bits could escape to the outside world.
      break;
    }
    case kSSEInt32ToFloat64: {
      RegisterOrOperand input = i.InputRegisterOrOperand(0);
      if (input.type == kRegister) {
        __ cvtlsi2sd(i.OutputDoubleRegister(), input.reg);
      } else {
        __ cvtlsi2sd(i.OutputDoubleRegister(), input.operand);
      }
      break;
    }
    case kSSEUint32ToFloat64: {
      // TODO(turbofan): X64 SSE cvtqsi2sd should support operands.
      __ cvtqsi2sd(i.OutputDoubleRegister(), i.InputRegister(0));
      break;
    }
    case kX64Movsxbl:
      __ movsxbl(i.OutputRegister(), i.MemoryOperand());
      break;
    case kX64Movzxbl:
      __ movzxbl(i.OutputRegister(), i.MemoryOperand());
      break;
    case kX64Movb: {
      int index = 0;
      Operand operand = i.MemoryOperand(&index);
      if (HasImmediateInput(instr, index)) {
        __ movb(operand, Immediate(i.InputInt8(index)));
      } else {
        __ movb(operand, i.InputRegister(index));
      }
      break;
    }
    case kX64Movsxwl:
      __ movsxwl(i.OutputRegister(), i.MemoryOperand());
      break;
    case kX64Movzxwl:
      __ movzxwl(i.OutputRegister(), i.MemoryOperand());
      break;
    case kX64Movw: {
      int index = 0;
      Operand operand = i.MemoryOperand(&index);
      if (HasImmediateInput(instr, index)) {
        __ movw(operand, Immediate(i.InputInt16(index)));
      } else {
        __ movw(operand, i.InputRegister(index));
      }
      break;
    }
    case kX64Movl:
      if (instr->HasOutput()) {
        if (instr->addressing_mode() == kMode_None) {
          RegisterOrOperand input = i.InputRegisterOrOperand(0);
          if (input.type == kRegister) {
            __ movl(i.OutputRegister(), input.reg);
          } else {
            __ movl(i.OutputRegister(), input.operand);
          }
        } else {
          __ movl(i.OutputRegister(), i.MemoryOperand());
        }
      } else {
        int index = 0;
        Operand operand = i.MemoryOperand(&index);
        if (HasImmediateInput(instr, index)) {
          __ movl(operand, i.InputImmediate(index));
        } else {
          __ movl(operand, i.InputRegister(index));
        }
      }
      break;
    case kX64Movsxlq: {
      RegisterOrOperand input = i.InputRegisterOrOperand(0);
      if (input.type == kRegister) {
        __ movsxlq(i.OutputRegister(), input.reg);
      } else {
        __ movsxlq(i.OutputRegister(), input.operand);
      }
      break;
    }
    case kX64Movq:
      if (instr->HasOutput()) {
        __ movq(i.OutputRegister(), i.MemoryOperand());
      } else {
        int index = 0;
        Operand operand = i.MemoryOperand(&index);
        if (HasImmediateInput(instr, index)) {
          __ movq(operand, i.InputImmediate(index));
        } else {
          __ movq(operand, i.InputRegister(index));
        }
      }
      break;
    case kX64Movss:
      if (instr->HasOutput()) {
        __ movss(i.OutputDoubleRegister(), i.MemoryOperand());
      } else {
        int index = 0;
        Operand operand = i.MemoryOperand(&index);
        __ movss(operand, i.InputDoubleRegister(index));
      }
      break;
    case kX64Movsd:
      if (instr->HasOutput()) {
        __ movsd(i.OutputDoubleRegister(), i.MemoryOperand());
      } else {
        int index = 0;
        Operand operand = i.MemoryOperand(&index);
        __ movsd(operand, i.InputDoubleRegister(index));
      }
      break;
    case kX64Push:
      if (HasImmediateInput(instr, 0)) {
        __ pushq(i.InputImmediate(0));
      } else {
        RegisterOrOperand input = i.InputRegisterOrOperand(0);
        if (input.type == kRegister) {
          __ pushq(input.reg);
        } else {
          __ pushq(input.operand);
        }
      }
      break;
    case kX64StoreWriteBarrier: {
      Register object = i.InputRegister(0);
      Register index = i.InputRegister(1);
      Register value = i.InputRegister(2);
      __ movsxlq(index, index);
      __ movq(Operand(object, index, times_1, 0), value);
      __ leaq(index, Operand(object, index, times_1, 0));
      SaveFPRegsMode mode = code_->frame()->DidAllocateDoubleRegisters()
                                ? kSaveFPRegs
                                : kDontSaveFPRegs;
      __ RecordWrite(object, index, value, mode);
      break;
    }
  }
}


// Assembles branches after this instruction.
void CodeGenerator::AssembleArchBranch(Instruction* instr,
                                       FlagsCondition condition) {
  X64OperandConverter i(this, instr);
  Label done;

  // Emit a branch. The true and false targets are always the last two inputs
  // to the instruction.
  BasicBlock* tblock = i.InputBlock(static_cast<int>(instr->InputCount()) - 2);
  BasicBlock* fblock = i.InputBlock(static_cast<int>(instr->InputCount()) - 1);
  bool fallthru = IsNextInAssemblyOrder(fblock);
  Label* tlabel = code()->GetLabel(tblock);
  Label* flabel = fallthru ? &done : code()->GetLabel(fblock);
  Label::Distance flabel_distance = fallthru ? Label::kNear : Label::kFar;
  switch (condition) {
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
    case kUnorderedLessThan:
      __ j(parity_even, flabel, flabel_distance);
    // Fall through.
    case kUnsignedLessThan:
      __ j(below, tlabel);
      break;
    case kUnorderedGreaterThanOrEqual:
      __ j(parity_even, tlabel);
    // Fall through.
    case kUnsignedGreaterThanOrEqual:
      __ j(above_equal, tlabel);
      break;
    case kUnorderedLessThanOrEqual:
      __ j(parity_even, flabel, flabel_distance);
    // Fall through.
    case kUnsignedLessThanOrEqual:
      __ j(below_equal, tlabel);
      break;
    case kUnorderedGreaterThan:
      __ j(parity_even, tlabel);
    // Fall through.
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
  if (!fallthru) __ jmp(flabel, flabel_distance);  // no fallthru to flabel.
  __ bind(&done);
}


// Assembles boolean materializations after this instruction.
void CodeGenerator::AssembleArchBoolean(Instruction* instr,
                                        FlagsCondition condition) {
  X64OperandConverter i(this, instr);
  Label done;

  // Materialize a full 64-bit 1 or 0 value. The result register is always the
  // last output of the instruction.
  Label check;
  DCHECK_NE(0, instr->OutputCount());
  Register reg = i.OutputRegister(static_cast<int>(instr->OutputCount() - 1));
  Condition cc = no_condition;
  switch (condition) {
    case kUnorderedEqual:
      __ j(parity_odd, &check, Label::kNear);
      __ movl(reg, Immediate(0));
      __ jmp(&done, Label::kNear);
    // Fall through.
    case kEqual:
      cc = equal;
      break;
    case kUnorderedNotEqual:
      __ j(parity_odd, &check, Label::kNear);
      __ movl(reg, Immediate(1));
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
    case kUnorderedLessThan:
      __ j(parity_odd, &check, Label::kNear);
      __ movl(reg, Immediate(0));
      __ jmp(&done, Label::kNear);
    // Fall through.
    case kUnsignedLessThan:
      cc = below;
      break;
    case kUnorderedGreaterThanOrEqual:
      __ j(parity_odd, &check, Label::kNear);
      __ movl(reg, Immediate(1));
      __ jmp(&done, Label::kNear);
    // Fall through.
    case kUnsignedGreaterThanOrEqual:
      cc = above_equal;
      break;
    case kUnorderedLessThanOrEqual:
      __ j(parity_odd, &check, Label::kNear);
      __ movl(reg, Immediate(0));
      __ jmp(&done, Label::kNear);
    // Fall through.
    case kUnsignedLessThanOrEqual:
      cc = below_equal;
      break;
    case kUnorderedGreaterThan:
      __ j(parity_odd, &check, Label::kNear);
      __ movl(reg, Immediate(1));
      __ jmp(&done, Label::kNear);
    // Fall through.
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
  __ setcc(cc, reg);
  __ movzxbl(reg, reg);
  __ bind(&done);
}


void CodeGenerator::AssembleDeoptimizerCall(int deoptimization_id) {
  Address deopt_entry = Deoptimizer::GetDeoptimizationEntry(
      isolate(), deoptimization_id, Deoptimizer::LAZY);
  __ call(deopt_entry, RelocInfo::RUNTIME_ENTRY);
}


void CodeGenerator::AssemblePrologue() {
  CallDescriptor* descriptor = linkage()->GetIncomingDescriptor();
  int stack_slots = frame()->GetSpillSlotCount();
  if (descriptor->kind() == CallDescriptor::kCallAddress) {
    __ pushq(rbp);
    __ movq(rbp, rsp);
    const RegList saves = descriptor->CalleeSavedRegisters();
    if (saves != 0) {  // Save callee-saved registers.
      int register_save_area_size = 0;
      for (int i = Register::kNumRegisters - 1; i >= 0; i--) {
        if (!((1 << i) & saves)) continue;
        __ pushq(Register::from_code(i));
        register_save_area_size += kPointerSize;
      }
      frame()->SetRegisterSaveAreaSize(register_save_area_size);
    }
  } else if (descriptor->IsJSFunctionCall()) {
    CompilationInfo* info = linkage()->info();
    __ Prologue(info->IsCodePreAgingActive());
    frame()->SetRegisterSaveAreaSize(
        StandardFrameConstants::kFixedFrameSizeFromFp);

    // Sloppy mode functions and builtins need to replace the receiver with the
    // global proxy when called as functions (without an explicit receiver
    // object).
    // TODO(mstarzinger/verwaest): Should this be moved back into the CallIC?
    if (info->strict_mode() == SLOPPY && !info->is_native()) {
      Label ok;
      StackArgumentsAccessor args(rbp, info->scope()->num_parameters());
      __ movp(rcx, args.GetReceiverOperand());
      __ CompareRoot(rcx, Heap::kUndefinedValueRootIndex);
      __ j(not_equal, &ok, Label::kNear);
      __ movp(rcx, GlobalObjectOperand());
      __ movp(rcx, FieldOperand(rcx, GlobalObject::kGlobalProxyOffset));
      __ movp(args.GetReceiverOperand(), rcx);
      __ bind(&ok);
    }

  } else {
    __ StubPrologue();
    frame()->SetRegisterSaveAreaSize(
        StandardFrameConstants::kFixedFrameSizeFromFp);
  }
  if (stack_slots > 0) {
    __ subq(rsp, Immediate(stack_slots * kPointerSize));
  }
}


void CodeGenerator::AssembleReturn() {
  CallDescriptor* descriptor = linkage()->GetIncomingDescriptor();
  if (descriptor->kind() == CallDescriptor::kCallAddress) {
    if (frame()->GetRegisterSaveAreaSize() > 0) {
      // Remove this frame's spill slots first.
      int stack_slots = frame()->GetSpillSlotCount();
      if (stack_slots > 0) {
        __ addq(rsp, Immediate(stack_slots * kPointerSize));
      }
      const RegList saves = descriptor->CalleeSavedRegisters();
      // Restore registers.
      if (saves != 0) {
        for (int i = 0; i < Register::kNumRegisters; i++) {
          if (!((1 << i) & saves)) continue;
          __ popq(Register::from_code(i));
        }
      }
      __ popq(rbp);  // Pop caller's frame pointer.
      __ ret(0);
    } else {
      // No saved registers.
      __ movq(rsp, rbp);  // Move stack pointer back to frame pointer.
      __ popq(rbp);       // Pop caller's frame pointer.
      __ ret(0);
    }
  } else {
    __ movq(rsp, rbp);  // Move stack pointer back to frame pointer.
    __ popq(rbp);       // Pop caller's frame pointer.
    int pop_count = descriptor->IsJSFunctionCall()
                        ? static_cast<int>(descriptor->JSParameterCount())
                        : 0;
    __ ret(pop_count * kPointerSize);
  }
}


void CodeGenerator::AssembleMove(InstructionOperand* source,
                                 InstructionOperand* destination) {
  X64OperandConverter g(this, NULL);
  // Dispatch on the source and destination operand kinds.  Not all
  // combinations are possible.
  if (source->IsRegister()) {
    DCHECK(destination->IsRegister() || destination->IsStackSlot());
    Register src = g.ToRegister(source);
    if (destination->IsRegister()) {
      __ movq(g.ToRegister(destination), src);
    } else {
      __ movq(g.ToOperand(destination), src);
    }
  } else if (source->IsStackSlot()) {
    DCHECK(destination->IsRegister() || destination->IsStackSlot());
    Operand src = g.ToOperand(source);
    if (destination->IsRegister()) {
      Register dst = g.ToRegister(destination);
      __ movq(dst, src);
    } else {
      // Spill on demand to use a temporary register for memory-to-memory
      // moves.
      Register tmp = kScratchRegister;
      Operand dst = g.ToOperand(destination);
      __ movq(tmp, src);
      __ movq(dst, tmp);
    }
  } else if (source->IsConstant()) {
    ConstantOperand* constant_source = ConstantOperand::cast(source);
    Constant src = g.ToConstant(constant_source);
    if (destination->IsRegister() || destination->IsStackSlot()) {
      Register dst = destination->IsRegister() ? g.ToRegister(destination)
                                               : kScratchRegister;
      Immediate64 imm = g.ToImmediate64(constant_source);
      switch (imm.type) {
        case kImm64Value:
          __ Set(dst, imm.value);
          break;
        case kImm64Reference:
          __ Move(dst, imm.reference);
          break;
        case kImm64Handle:
          __ Move(dst, imm.handle);
          break;
      }
      if (destination->IsStackSlot()) {
        __ movq(g.ToOperand(destination), kScratchRegister);
      }
    } else if (src.type() == Constant::kFloat32) {
      // TODO(turbofan): Can we do better here?
      __ movl(kScratchRegister, Immediate(bit_cast<int32_t>(src.ToFloat32())));
      if (destination->IsDoubleRegister()) {
        XMMRegister dst = g.ToDoubleRegister(destination);
        __ movq(dst, kScratchRegister);
      } else {
        DCHECK(destination->IsDoubleStackSlot());
        Operand dst = g.ToOperand(destination);
        __ movl(dst, kScratchRegister);
      }
    } else {
      DCHECK_EQ(Constant::kFloat64, src.type());
      __ movq(kScratchRegister, bit_cast<int64_t>(src.ToFloat64()));
      if (destination->IsDoubleRegister()) {
        __ movq(g.ToDoubleRegister(destination), kScratchRegister);
      } else {
        DCHECK(destination->IsDoubleStackSlot());
        __ movq(g.ToOperand(destination), kScratchRegister);
      }
    }
  } else if (source->IsDoubleRegister()) {
    XMMRegister src = g.ToDoubleRegister(source);
    if (destination->IsDoubleRegister()) {
      XMMRegister dst = g.ToDoubleRegister(destination);
      __ movsd(dst, src);
    } else {
      DCHECK(destination->IsDoubleStackSlot());
      Operand dst = g.ToOperand(destination);
      __ movsd(dst, src);
    }
  } else if (source->IsDoubleStackSlot()) {
    DCHECK(destination->IsDoubleRegister() || destination->IsDoubleStackSlot());
    Operand src = g.ToOperand(source);
    if (destination->IsDoubleRegister()) {
      XMMRegister dst = g.ToDoubleRegister(destination);
      __ movsd(dst, src);
    } else {
      // We rely on having xmm0 available as a fixed scratch register.
      Operand dst = g.ToOperand(destination);
      __ movsd(xmm0, src);
      __ movsd(dst, xmm0);
    }
  } else {
    UNREACHABLE();
  }
}


void CodeGenerator::AssembleSwap(InstructionOperand* source,
                                 InstructionOperand* destination) {
  X64OperandConverter g(this, NULL);
  // Dispatch on the source and destination operand kinds.  Not all
  // combinations are possible.
  if (source->IsRegister() && destination->IsRegister()) {
    // Register-register.
    __ xchgq(g.ToRegister(source), g.ToRegister(destination));
  } else if (source->IsRegister() && destination->IsStackSlot()) {
    Register src = g.ToRegister(source);
    Operand dst = g.ToOperand(destination);
    __ xchgq(src, dst);
  } else if ((source->IsStackSlot() && destination->IsStackSlot()) ||
             (source->IsDoubleStackSlot() &&
              destination->IsDoubleStackSlot())) {
    // Memory-memory.
    Register tmp = kScratchRegister;
    Operand src = g.ToOperand(source);
    Operand dst = g.ToOperand(destination);
    __ movq(tmp, dst);
    __ xchgq(tmp, src);
    __ movq(dst, tmp);
  } else if (source->IsDoubleRegister() && destination->IsDoubleRegister()) {
    // XMM register-register swap. We rely on having xmm0
    // available as a fixed scratch register.
    XMMRegister src = g.ToDoubleRegister(source);
    XMMRegister dst = g.ToDoubleRegister(destination);
    __ movsd(xmm0, src);
    __ movsd(src, dst);
    __ movsd(dst, xmm0);
  } else if (source->IsDoubleRegister() && destination->IsDoubleRegister()) {
    // XMM register-memory swap.  We rely on having xmm0
    // available as a fixed scratch register.
    XMMRegister src = g.ToDoubleRegister(source);
    Operand dst = g.ToOperand(destination);
    __ movsd(xmm0, src);
    __ movsd(src, dst);
    __ movsd(dst, xmm0);
  } else {
    // No other combinations are possible.
    UNREACHABLE();
  }
}


void CodeGenerator::AddNopForSmiCodeInlining() { __ nop(); }


void CodeGenerator::EnsureSpaceForLazyDeopt() {
  int space_needed = Deoptimizer::patch_size();
  if (!linkage()->info()->IsStub()) {
    // Ensure that we have enough space after the previous lazy-bailout
    // instruction for patching the code here.
    int current_pc = masm()->pc_offset();
    if (current_pc < last_lazy_deopt_pc_ + space_needed) {
      int padding_size = last_lazy_deopt_pc_ + space_needed - current_pc;
      __ Nop(padding_size);
    }
  }
  MarkLazyDeoptSite();
}

#undef __

}  // namespace internal
}  // namespace compiler
}  // namespace v8
