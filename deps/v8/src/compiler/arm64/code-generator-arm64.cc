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
class Arm64OperandConverter V8_FINAL : public InstructionOperandConverter {
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

  MemOperand MemoryOperand(int* first_index) {
    const int index = *first_index;
    switch (AddressingModeField::decode(instr_->opcode())) {
      case kMode_None:
        break;
      case kMode_MRI:
        *first_index += 2;
        return MemOperand(InputRegister(index + 0), InputInt32(index + 1));
      case kMode_MRR:
        *first_index += 2;
        return MemOperand(InputRegister(index + 0), InputRegister(index + 1),
                          SXTW);
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
  } while (0);


// Assembles an instruction after register allocation, producing machine code.
void CodeGenerator::AssembleArchInstruction(Instruction* instr) {
  Arm64OperandConverter i(this, instr);
  InstructionCode opcode = instr->opcode();
  switch (ArchOpcodeField::decode(opcode)) {
    case kArchJmp:
      __ B(code_->GetLabel(i.InputBlock(0)));
      break;
    case kArchNop:
      // don't emit code for nops.
      break;
    case kArchRet:
      AssembleReturn();
      break;
    case kArchDeoptimize: {
      int deoptimization_id = MiscField::decode(instr->opcode());
      BuildTranslation(instr, deoptimization_id);

      Address deopt_entry = Deoptimizer::GetDeoptimizationEntry(
          isolate(), deoptimization_id, Deoptimizer::LAZY);
      __ Call(deopt_entry, RelocInfo::RUNTIME_ENTRY);
      break;
    }
    case kArm64Add:
      __ Add(i.OutputRegister(), i.InputRegister(0), i.InputOperand(1));
      break;
    case kArm64Add32:
      if (FlagsModeField::decode(opcode) != kFlags_none) {
        __ Adds(i.OutputRegister32(), i.InputRegister32(0),
                i.InputOperand32(1));
      } else {
        __ Add(i.OutputRegister32(), i.InputRegister32(0), i.InputOperand32(1));
      }
      break;
    case kArm64And:
      __ And(i.OutputRegister(), i.InputRegister(0), i.InputOperand(1));
      break;
    case kArm64And32:
      __ And(i.OutputRegister32(), i.InputRegister32(0), i.InputOperand32(1));
      break;
    case kArm64Mul:
      __ Mul(i.OutputRegister(), i.InputRegister(0), i.InputRegister(1));
      break;
    case kArm64Mul32:
      __ Mul(i.OutputRegister32(), i.InputRegister32(0), i.InputRegister32(1));
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
      __ Orr(i.OutputRegister(), i.InputRegister(0), i.InputOperand(1));
      break;
    case kArm64Or32:
      __ Orr(i.OutputRegister32(), i.InputRegister32(0), i.InputOperand32(1));
      break;
    case kArm64Xor:
      __ Eor(i.OutputRegister(), i.InputRegister(0), i.InputOperand(1));
      break;
    case kArm64Xor32:
      __ Eor(i.OutputRegister32(), i.InputRegister32(0), i.InputOperand32(1));
      break;
    case kArm64Sub:
      __ Sub(i.OutputRegister(), i.InputRegister(0), i.InputOperand(1));
      break;
    case kArm64Sub32:
      if (FlagsModeField::decode(opcode) != kFlags_none) {
        __ Subs(i.OutputRegister32(), i.InputRegister32(0),
                i.InputOperand32(1));
      } else {
        __ Sub(i.OutputRegister32(), i.InputRegister32(0), i.InputOperand32(1));
      }
      break;
    case kArm64Shl:
      ASSEMBLE_SHIFT(Lsl, 64);
      break;
    case kArm64Shl32:
      ASSEMBLE_SHIFT(Lsl, 32);
      break;
    case kArm64Shr:
      ASSEMBLE_SHIFT(Lsr, 64);
      break;
    case kArm64Shr32:
      ASSEMBLE_SHIFT(Lsr, 32);
      break;
    case kArm64Sar:
      ASSEMBLE_SHIFT(Asr, 64);
      break;
    case kArm64Sar32:
      ASSEMBLE_SHIFT(Asr, 32);
      break;
    case kArm64CallCodeObject: {
      if (instr->InputAt(0)->IsImmediate()) {
        Handle<Code> code = Handle<Code>::cast(i.InputHeapObject(0));
        __ Call(code, RelocInfo::CODE_TARGET);
        RecordSafepoint(instr->pointer_map(), Safepoint::kSimple, 0,
                        Safepoint::kNoLazyDeopt);
      } else {
        Register reg = i.InputRegister(0);
        int entry = Code::kHeaderSize - kHeapObjectTag;
        __ Ldr(reg, MemOperand(reg, entry));
        __ Call(reg);
        RecordSafepoint(instr->pointer_map(), Safepoint::kSimple, 0,
                        Safepoint::kNoLazyDeopt);
      }
      bool lazy_deopt = (MiscField::decode(instr->opcode()) == 1);
      if (lazy_deopt) {
        RecordLazyDeoptimizationEntry(instr);
      }
      // Meaningless instruction for ICs to overwrite.
      AddNopForSmiCodeInlining();
      break;
    }
    case kArm64CallJSFunction: {
      Register func = i.InputRegister(0);

      // TODO(jarin) The load of the context should be separated from the call.
      __ Ldr(cp, FieldMemOperand(func, JSFunction::kContextOffset));
      __ Ldr(x10, FieldMemOperand(func, JSFunction::kCodeEntryOffset));
      __ Call(x10);

      RecordSafepoint(instr->pointer_map(), Safepoint::kSimple, 0,
                      Safepoint::kNoLazyDeopt);
      RecordLazyDeoptimizationEntry(instr);
      break;
    }
    case kArm64CallAddress: {
      DirectCEntryStub stub(isolate());
      stub.GenerateCall(masm(), i.InputRegister(0));
      break;
    }
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
    case kArm64Drop: {
      int words = MiscField::decode(instr->opcode());
      __ Drop(words);
      break;
    }
    case kArm64Cmp:
      __ Cmp(i.InputRegister(0), i.InputOperand(1));
      break;
    case kArm64Cmp32:
      __ Cmp(i.InputRegister32(0), i.InputOperand32(1));
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
    case kArm64Int32ToInt64:
      __ Sxtw(i.OutputRegister(), i.InputRegister(0));
      break;
    case kArm64Int64ToInt32:
      if (!i.OutputRegister().is(i.InputRegister(0))) {
        __ Mov(i.OutputRegister(), i.InputRegister(0));
      }
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
    case kArm64LoadWord8:
      __ Ldrb(i.OutputRegister(), i.MemoryOperand());
      break;
    case kArm64StoreWord8:
      __ Strb(i.InputRegister(2), i.MemoryOperand());
      break;
    case kArm64LoadWord16:
      __ Ldrh(i.OutputRegister(), i.MemoryOperand());
      break;
    case kArm64StoreWord16:
      __ Strh(i.InputRegister(2), i.MemoryOperand());
      break;
    case kArm64LoadWord32:
      __ Ldr(i.OutputRegister32(), i.MemoryOperand());
      break;
    case kArm64StoreWord32:
      __ Str(i.InputRegister32(2), i.MemoryOperand());
      break;
    case kArm64LoadWord64:
      __ Ldr(i.OutputRegister(), i.MemoryOperand());
      break;
    case kArm64StoreWord64:
      __ Str(i.InputRegister(2), i.MemoryOperand());
      break;
    case kArm64Float64Load:
      __ Ldr(i.OutputDoubleRegister(), i.MemoryOperand());
      break;
    case kArm64Float64Store:
      __ Str(i.InputDoubleRegister(2), i.MemoryOperand());
      break;
    case kArm64StoreWriteBarrier: {
      Register object = i.InputRegister(0);
      Register index = i.InputRegister(1);
      Register value = i.InputRegister(2);
      __ Add(index, object, Operand(index, SXTW));
      __ Str(value, MemOperand(index));
      SaveFPRegsMode mode = code_->frame()->DidAllocateDoubleRegisters()
                                ? kSaveFPRegs
                                : kDontSaveFPRegs;
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
  BasicBlock* tblock = i.InputBlock(instr->InputCount() - 2);
  BasicBlock* fblock = i.InputBlock(instr->InputCount() - 1);
  bool fallthru = IsNextInAssemblyOrder(fblock);
  Label* tlabel = code()->GetLabel(tblock);
  Label* flabel = fallthru ? &done : code()->GetLabel(fblock);
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
    CompilationInfo* info = linkage()->info();
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
    int pop_count =
        descriptor->IsJSFunctionCall() ? descriptor->ParameterCount() : 0;
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
    ConstantOperand* constant_source = ConstantOperand::cast(source);
    if (destination->IsRegister() || destination->IsStackSlot()) {
      UseScratchRegisterScope scope(masm());
      Register dst = destination->IsRegister() ? g.ToRegister(destination)
                                               : scope.AcquireX();
      Constant src = g.ToConstant(source);
      if (src.type() == Constant::kHeapObject) {
        __ LoadObject(dst, src.ToHeapObject());
      } else {
        __ Mov(dst, g.ToImmediate(source));
      }
      if (destination->IsStackSlot()) {
        __ Str(dst, g.ToMemOperand(destination, masm()));
      }
    } else if (destination->IsDoubleRegister()) {
      FPRegister result = g.ToDoubleRegister(destination);
      __ Fmov(result, g.ToDouble(constant_source));
    } else {
      DCHECK(destination->IsDoubleStackSlot());
      UseScratchRegisterScope scope(masm());
      FPRegister temp = scope.AcquireD();
      __ Fmov(temp, g.ToDouble(constant_source));
      __ Str(temp, g.ToMemOperand(destination, masm()));
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
      __ Fmov(src, temp);
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

#undef __

#if DEBUG

// Checks whether the code between start_pc and end_pc is a no-op.
bool CodeGenerator::IsNopForSmiCodeInlining(Handle<Code> code, int start_pc,
                                            int end_pc) {
  if (start_pc + 4 != end_pc) {
    return false;
  }
  Address instr_address = code->instruction_start() + start_pc;

  v8::internal::Instruction* instr =
      reinterpret_cast<v8::internal::Instruction*>(instr_address);
  return instr->IsMovz() && instr->Rd() == xzr.code() && instr->SixtyFourBits();
}

#endif  // DEBUG

}  // namespace compiler
}  // namespace internal
}  // namespace v8
