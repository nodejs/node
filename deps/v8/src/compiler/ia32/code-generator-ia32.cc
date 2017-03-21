// Copyright 2013 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/code-generator.h"

#include "src/compilation-info.h"
#include "src/compiler/code-generator-impl.h"
#include "src/compiler/gap-resolver.h"
#include "src/compiler/node-matchers.h"
#include "src/compiler/osr.h"
#include "src/frames.h"
#include "src/ia32/assembler-ia32.h"
#include "src/ia32/frames-ia32.h"
#include "src/ia32/macro-assembler-ia32.h"

namespace v8 {
namespace internal {
namespace compiler {

#define __ masm()->


#define kScratchDoubleReg xmm0


// Adds IA-32 specific methods for decoding operands.
class IA32OperandConverter : public InstructionOperandConverter {
 public:
  IA32OperandConverter(CodeGenerator* gen, Instruction* instr)
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
    } else if (op->IsFPRegister()) {
      DCHECK(extra == 0);
      return Operand(ToDoubleRegister(op));
    }
    DCHECK(op->IsStackSlot() || op->IsFPStackSlot());
    return SlotToOperand(AllocatedOperand::cast(op)->index(), extra);
  }

  Operand SlotToOperand(int slot, int extra = 0) {
    FrameOffset offset = frame_access_state()->GetFrameOffset(slot);
    return Operand(offset.from_stack_pointer() ? esp : ebp,
                   offset.offset() + extra);
  }

  Operand HighOperand(InstructionOperand* op) {
    DCHECK(op->IsFPStackSlot());
    return ToOperand(op, kPointerSize);
  }

  Immediate ToImmediate(InstructionOperand* operand) {
    Constant constant = ToConstant(operand);
    if (constant.type() == Constant::kInt32 &&
        RelocInfo::IsWasmReference(constant.rmode())) {
      return Immediate(reinterpret_cast<Address>(constant.ToInt32()),
                       constant.rmode());
    }
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
        Constant ctant = ToConstant(instr_->InputAt(NextOffset(offset)));
        return Operand(base, ctant.ToInt32(), ctant.rmode());
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
        Constant ctant = ToConstant(instr_->InputAt(NextOffset(offset)));
        return Operand(base, index, scale, ctant.ToInt32(), ctant.rmode());
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
        Constant ctant = ToConstant(instr_->InputAt(NextOffset(offset)));
        return Operand(index, scale, ctant.ToInt32(), ctant.rmode());
      }
      case kMode_MI: {
        Constant ctant = ToConstant(instr_->InputAt(NextOffset(offset)));
        return Operand(ctant.ToInt32(), ctant.rmode());
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

class OutOfLineLoadFloat32NaN final : public OutOfLineCode {
 public:
  OutOfLineLoadFloat32NaN(CodeGenerator* gen, XMMRegister result)
      : OutOfLineCode(gen), result_(result) {}

  void Generate() final {
    __ xorps(result_, result_);
    __ divss(result_, result_);
  }

 private:
  XMMRegister const result_;
};

class OutOfLineLoadFloat64NaN final : public OutOfLineCode {
 public:
  OutOfLineLoadFloat64NaN(CodeGenerator* gen, XMMRegister result)
      : OutOfLineCode(gen), result_(result) {}

  void Generate() final {
    __ xorpd(result_, result_);
    __ divsd(result_, result_);
  }

 private:
  XMMRegister const result_;
};

class OutOfLineTruncateDoubleToI final : public OutOfLineCode {
 public:
  OutOfLineTruncateDoubleToI(CodeGenerator* gen, Register result,
                             XMMRegister input)
      : OutOfLineCode(gen), result_(result), input_(input) {}

  void Generate() final {
    __ sub(esp, Immediate(kDoubleSize));
    __ movsd(MemOperand(esp, 0), input_);
    __ SlowTruncateToI(result_, esp, 0);
    __ add(esp, Immediate(kDoubleSize));
  }

 private:
  Register const result_;
  XMMRegister const input_;
};


class OutOfLineRecordWrite final : public OutOfLineCode {
 public:
  OutOfLineRecordWrite(CodeGenerator* gen, Register object, Operand operand,
                       Register value, Register scratch0, Register scratch1,
                       RecordWriteMode mode)
      : OutOfLineCode(gen),
        object_(object),
        operand_(operand),
        value_(value),
        scratch0_(scratch0),
        scratch1_(scratch1),
        mode_(mode) {}

  void Generate() final {
    if (mode_ > RecordWriteMode::kValueIsPointer) {
      __ JumpIfSmi(value_, exit());
    }
    __ CheckPageFlag(value_, scratch0_,
                     MemoryChunk::kPointersToHereAreInterestingMask, zero,
                     exit());
    RememberedSetAction const remembered_set_action =
        mode_ > RecordWriteMode::kValueIsMap ? EMIT_REMEMBERED_SET
                                             : OMIT_REMEMBERED_SET;
    SaveFPRegsMode const save_fp_mode =
        frame()->DidAllocateDoubleRegisters() ? kSaveFPRegs : kDontSaveFPRegs;
    RecordWriteStub stub(isolate(), object_, scratch0_, scratch1_,
                         remembered_set_action, save_fp_mode);
    __ lea(scratch1_, operand_);
    __ CallStub(&stub);
  }

 private:
  Register const object_;
  Operand const operand_;
  Register const value_;
  Register const scratch0_;
  Register const scratch1_;
  RecordWriteMode const mode_;
};

}  // namespace

#define ASSEMBLE_CHECKED_LOAD_FLOAT(asm_instr, OutOfLineLoadNaN)      \
  do {                                                                \
    auto result = i.OutputDoubleRegister();                           \
    auto offset = i.InputRegister(0);                                 \
    if (instr->InputAt(1)->IsRegister()) {                            \
      __ cmp(offset, i.InputRegister(1));                             \
    } else {                                                          \
      __ cmp(offset, i.InputImmediate(1));                            \
    }                                                                 \
    OutOfLineCode* ool = new (zone()) OutOfLineLoadNaN(this, result); \
    __ j(above_equal, ool->entry());                                  \
    __ asm_instr(result, i.MemoryOperand(2));                         \
    __ bind(ool->exit());                                             \
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

#define ASSEMBLE_CHECKED_STORE_FLOAT(asm_instr)                 \
  do {                                                          \
    auto offset = i.InputRegister(0);                           \
    if (instr->InputAt(1)->IsRegister()) {                      \
      __ cmp(offset, i.InputRegister(1));                       \
    } else {                                                    \
      __ cmp(offset, i.InputImmediate(1));                      \
    }                                                           \
    Label done;                                                 \
    __ j(above_equal, &done, Label::kNear);                     \
    __ asm_instr(i.MemoryOperand(3), i.InputDoubleRegister(2)); \
    __ bind(&done);                                             \
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

#define ASSEMBLE_COMPARE(asm_instr)                                   \
  do {                                                                \
    if (AddressingModeField::decode(instr->opcode()) != kMode_None) { \
      size_t index = 0;                                               \
      Operand left = i.MemoryOperand(&index);                         \
      if (HasImmediateInput(instr, index)) {                          \
        __ asm_instr(left, i.InputImmediate(index));                  \
      } else {                                                        \
        __ asm_instr(left, i.InputRegister(index));                   \
      }                                                               \
    } else {                                                          \
      if (HasImmediateInput(instr, 1)) {                              \
        if (instr->InputAt(0)->IsRegister()) {                        \
          __ asm_instr(i.InputRegister(0), i.InputImmediate(1));      \
        } else {                                                      \
          __ asm_instr(i.InputOperand(0), i.InputImmediate(1));       \
        }                                                             \
      } else {                                                        \
        if (instr->InputAt(1)->IsRegister()) {                        \
          __ asm_instr(i.InputRegister(0), i.InputRegister(1));       \
        } else {                                                      \
          __ asm_instr(i.InputRegister(0), i.InputOperand(1));        \
        }                                                             \
      }                                                               \
    }                                                                 \
  } while (0)

#define ASSEMBLE_IEEE754_BINOP(name)                                          \
  do {                                                                        \
    /* Pass two doubles as arguments on the stack. */                         \
    __ PrepareCallCFunction(4, eax);                                          \
    __ movsd(Operand(esp, 0 * kDoubleSize), i.InputDoubleRegister(0));        \
    __ movsd(Operand(esp, 1 * kDoubleSize), i.InputDoubleRegister(1));        \
    __ CallCFunction(ExternalReference::ieee754_##name##_function(isolate()), \
                     4);                                                      \
    /* Return value is in st(0) on ia32. */                                   \
    /* Store it into the result register. */                                  \
    __ sub(esp, Immediate(kDoubleSize));                                      \
    __ fstp_d(Operand(esp, 0));                                               \
    __ movsd(i.OutputDoubleRegister(), Operand(esp, 0));                      \
    __ add(esp, Immediate(kDoubleSize));                                      \
  } while (false)

#define ASSEMBLE_IEEE754_UNOP(name)                                           \
  do {                                                                        \
    /* Pass one double as argument on the stack. */                           \
    __ PrepareCallCFunction(2, eax);                                          \
    __ movsd(Operand(esp, 0 * kDoubleSize), i.InputDoubleRegister(0));        \
    __ CallCFunction(ExternalReference::ieee754_##name##_function(isolate()), \
                     2);                                                      \
    /* Return value is in st(0) on ia32. */                                   \
    /* Store it into the result register. */                                  \
    __ sub(esp, Immediate(kDoubleSize));                                      \
    __ fstp_d(Operand(esp, 0));                                               \
    __ movsd(i.OutputDoubleRegister(), Operand(esp, 0));                      \
    __ add(esp, Immediate(kDoubleSize));                                      \
  } while (false)

void CodeGenerator::AssembleDeconstructFrame() {
  __ mov(esp, ebp);
  __ pop(ebp);
}

void CodeGenerator::AssemblePrepareTailCall() {
  if (frame_access_state()->has_frame()) {
    __ mov(ebp, MemOperand(ebp, 0));
  }
  frame_access_state()->SetFrameAccessToSP();
}

void CodeGenerator::AssemblePopArgumentsAdaptorFrame(Register args_reg,
                                                     Register, Register,
                                                     Register) {
  // There are not enough temp registers left on ia32 for a call instruction
  // so we pick some scratch registers and save/restore them manually here.
  int scratch_count = 3;
  Register scratch1 = ebx;
  Register scratch2 = ecx;
  Register scratch3 = edx;
  DCHECK(!AreAliased(args_reg, scratch1, scratch2, scratch3));
  Label done;

  // Check if current frame is an arguments adaptor frame.
  __ cmp(Operand(ebp, StandardFrameConstants::kContextOffset),
         Immediate(Smi::FromInt(StackFrame::ARGUMENTS_ADAPTOR)));
  __ j(not_equal, &done, Label::kNear);

  __ push(scratch1);
  __ push(scratch2);
  __ push(scratch3);

  // Load arguments count from current arguments adaptor frame (note, it
  // does not include receiver).
  Register caller_args_count_reg = scratch1;
  __ mov(caller_args_count_reg,
         Operand(ebp, ArgumentsAdaptorFrameConstants::kLengthOffset));
  __ SmiUntag(caller_args_count_reg);

  ParameterCount callee_args_count(args_reg);
  __ PrepareForTailCall(callee_args_count, caller_args_count_reg, scratch2,
                        scratch3, ReturnAddressState::kOnStack, scratch_count);
  __ pop(scratch3);
  __ pop(scratch2);
  __ pop(scratch1);

  __ bind(&done);
}

namespace {

void AdjustStackPointerForTailCall(MacroAssembler* masm,
                                   FrameAccessState* state,
                                   int new_slot_above_sp,
                                   bool allow_shrinkage = true) {
  int current_sp_offset = state->GetSPToFPSlotCount() +
                          StandardFrameConstants::kFixedSlotCountAboveFp;
  int stack_slot_delta = new_slot_above_sp - current_sp_offset;
  if (stack_slot_delta > 0) {
    masm->sub(esp, Immediate(stack_slot_delta * kPointerSize));
    state->IncreaseSPDelta(stack_slot_delta);
  } else if (allow_shrinkage && stack_slot_delta < 0) {
    masm->add(esp, Immediate(-stack_slot_delta * kPointerSize));
    state->IncreaseSPDelta(stack_slot_delta);
  }
}

}  // namespace

void CodeGenerator::AssembleTailCallBeforeGap(Instruction* instr,
                                              int first_unused_stack_slot) {
  CodeGenerator::PushTypeFlags flags(kImmediatePush | kScalarPush);
  ZoneVector<MoveOperands*> pushes(zone());
  GetPushCompatibleMoves(instr, flags, &pushes);

  if (!pushes.empty() &&
      (LocationOperand::cast(pushes.back()->destination()).index() + 1 ==
       first_unused_stack_slot)) {
    IA32OperandConverter g(this, instr);
    for (auto move : pushes) {
      LocationOperand destination_location(
          LocationOperand::cast(move->destination()));
      InstructionOperand source(move->source());
      AdjustStackPointerForTailCall(masm(), frame_access_state(),
                                    destination_location.index());
      if (source.IsStackSlot()) {
        LocationOperand source_location(LocationOperand::cast(source));
        __ push(g.SlotToOperand(source_location.index()));
      } else if (source.IsRegister()) {
        LocationOperand source_location(LocationOperand::cast(source));
        __ push(source_location.GetRegister());
      } else if (source.IsImmediate()) {
        __ push(Immediate(ImmediateOperand::cast(source).inline_value()));
      } else {
        // Pushes of non-scalar data types is not supported.
        UNIMPLEMENTED();
      }
      frame_access_state()->IncreaseSPDelta(1);
      move->Eliminate();
    }
  }
  AdjustStackPointerForTailCall(masm(), frame_access_state(),
                                first_unused_stack_slot, false);
}

void CodeGenerator::AssembleTailCallAfterGap(Instruction* instr,
                                             int first_unused_stack_slot) {
  AdjustStackPointerForTailCall(masm(), frame_access_state(),
                                first_unused_stack_slot);
}

// Assembles an instruction after register allocation, producing machine code.
CodeGenerator::CodeGenResult CodeGenerator::AssembleArchInstruction(
    Instruction* instr) {
  IA32OperandConverter i(this, instr);
  InstructionCode opcode = instr->opcode();
  ArchOpcode arch_opcode = ArchOpcodeField::decode(opcode);
  switch (arch_opcode) {
    case kArchCallCodeObject: {
      EnsureSpaceForLazyDeopt();
      if (HasImmediateInput(instr, 0)) {
        Handle<Code> code = Handle<Code>::cast(i.InputHeapObject(0));
        __ call(code, RelocInfo::CODE_TARGET);
      } else {
        Register reg = i.InputRegister(0);
        __ add(reg, Immediate(Code::kHeaderSize - kHeapObjectTag));
        __ call(reg);
      }
      RecordCallPosition(instr);
      frame_access_state()->ClearSPDelta();
      break;
    }
    case kArchTailCallCodeObjectFromJSFunction:
    case kArchTailCallCodeObject: {
      if (arch_opcode == kArchTailCallCodeObjectFromJSFunction) {
        AssemblePopArgumentsAdaptorFrame(kJavaScriptCallArgCountRegister,
                                         no_reg, no_reg, no_reg);
      }
      if (HasImmediateInput(instr, 0)) {
        Handle<Code> code = Handle<Code>::cast(i.InputHeapObject(0));
        __ jmp(code, RelocInfo::CODE_TARGET);
      } else {
        Register reg = i.InputRegister(0);
        __ add(reg, Immediate(Code::kHeaderSize - kHeapObjectTag));
        __ jmp(reg);
      }
      frame_access_state()->ClearSPDelta();
      frame_access_state()->SetFrameAccessToDefault();
      break;
    }
    case kArchTailCallAddress: {
      CHECK(!HasImmediateInput(instr, 0));
      Register reg = i.InputRegister(0);
      __ jmp(reg);
      frame_access_state()->ClearSPDelta();
      frame_access_state()->SetFrameAccessToDefault();
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
      frame_access_state()->ClearSPDelta();
      break;
    }
    case kArchTailCallJSFunctionFromJSFunction: {
      Register func = i.InputRegister(0);
      if (FLAG_debug_code) {
        // Check the function's context matches the context argument.
        __ cmp(esi, FieldOperand(func, JSFunction::kContextOffset));
        __ Assert(equal, kWrongFunctionContext);
      }
      AssemblePopArgumentsAdaptorFrame(kJavaScriptCallArgCountRegister, no_reg,
                                       no_reg, no_reg);
      __ jmp(FieldOperand(func, JSFunction::kCodeEntryOffset));
      frame_access_state()->ClearSPDelta();
      frame_access_state()->SetFrameAccessToDefault();
      break;
    }
    case kArchPrepareCallCFunction: {
      // Frame alignment requires using FP-relative frame addressing.
      frame_access_state()->SetFrameAccessToFP();
      int const num_parameters = MiscField::decode(instr->opcode());
      __ PrepareCallCFunction(num_parameters, i.TempRegister(0));
      break;
    }
    case kArchPrepareTailCall:
      AssemblePrepareTailCall();
      break;
    case kArchCallCFunction: {
      int const num_parameters = MiscField::decode(instr->opcode());
      if (HasImmediateInput(instr, 0)) {
        ExternalReference ref = i.InputExternalReference(0);
        __ CallCFunction(ref, num_parameters);
      } else {
        Register func = i.InputRegister(0);
        __ CallCFunction(func, num_parameters);
      }
      frame_access_state()->SetFrameAccessToDefault();
      frame_access_state()->ClearSPDelta();
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
    case kArchComment: {
      Address comment_string = i.InputExternalReference(0).address();
      __ RecordComment(reinterpret_cast<const char*>(comment_string));
      break;
    }
    case kArchDebugBreak:
      __ int3();
      break;
    case kArchNop:
    case kArchThrowTerminator:
      // don't emit code for nops.
      break;
    case kArchDeoptimize: {
      int deopt_state_id =
          BuildTranslation(instr, -1, 0, OutputFrameStateCombine::Ignore());
      Deoptimizer::BailoutType bailout_type =
          Deoptimizer::BailoutType(MiscField::decode(instr->opcode()));
      CodeGenResult result = AssembleDeoptimizerCall(
          deopt_state_id, bailout_type, current_source_position_);
      if (result != kSuccess) return result;
      break;
    }
    case kArchRet:
      AssembleReturn(instr->InputAt(0));
      break;
    case kArchStackPointer:
      __ mov(i.OutputRegister(), esp);
      break;
    case kArchFramePointer:
      __ mov(i.OutputRegister(), ebp);
      break;
    case kArchParentFramePointer:
      if (frame_access_state()->has_frame()) {
        __ mov(i.OutputRegister(), Operand(ebp, 0));
      } else {
        __ mov(i.OutputRegister(), ebp);
      }
      break;
    case kArchTruncateDoubleToI: {
      auto result = i.OutputRegister();
      auto input = i.InputDoubleRegister(0);
      auto ool = new (zone()) OutOfLineTruncateDoubleToI(this, result, input);
      __ cvttsd2si(result, Operand(input));
      __ cmp(result, 1);
      __ j(overflow, ool->entry());
      __ bind(ool->exit());
      break;
    }
    case kArchStoreWithWriteBarrier: {
      RecordWriteMode mode =
          static_cast<RecordWriteMode>(MiscField::decode(instr->opcode()));
      Register object = i.InputRegister(0);
      size_t index = 0;
      Operand operand = i.MemoryOperand(&index);
      Register value = i.InputRegister(index);
      Register scratch0 = i.TempRegister(0);
      Register scratch1 = i.TempRegister(1);
      auto ool = new (zone()) OutOfLineRecordWrite(this, object, operand, value,
                                                   scratch0, scratch1, mode);
      __ mov(operand, value);
      __ CheckPageFlag(object, scratch0,
                       MemoryChunk::kPointersFromHereAreInterestingMask,
                       not_zero, ool->entry());
      __ bind(ool->exit());
      break;
    }
    case kArchStackSlot: {
      FrameOffset offset =
          frame_access_state()->GetFrameOffset(i.InputInt32(0));
      Register base;
      if (offset.from_stack_pointer()) {
        base = esp;
      } else {
        base = ebp;
      }
      __ lea(i.OutputRegister(), Operand(base, offset.offset()));
      break;
    }
    case kIeee754Float64Acos:
      ASSEMBLE_IEEE754_UNOP(acos);
      break;
    case kIeee754Float64Acosh:
      ASSEMBLE_IEEE754_UNOP(acosh);
      break;
    case kIeee754Float64Asin:
      ASSEMBLE_IEEE754_UNOP(asin);
      break;
    case kIeee754Float64Asinh:
      ASSEMBLE_IEEE754_UNOP(asinh);
      break;
    case kIeee754Float64Atan:
      ASSEMBLE_IEEE754_UNOP(atan);
      break;
    case kIeee754Float64Atanh:
      ASSEMBLE_IEEE754_UNOP(atanh);
      break;
    case kIeee754Float64Atan2:
      ASSEMBLE_IEEE754_BINOP(atan2);
      break;
    case kIeee754Float64Cbrt:
      ASSEMBLE_IEEE754_UNOP(cbrt);
      break;
    case kIeee754Float64Cos:
      ASSEMBLE_IEEE754_UNOP(cos);
      break;
    case kIeee754Float64Cosh:
      ASSEMBLE_IEEE754_UNOP(cosh);
      break;
    case kIeee754Float64Expm1:
      ASSEMBLE_IEEE754_UNOP(expm1);
      break;
    case kIeee754Float64Exp:
      ASSEMBLE_IEEE754_UNOP(exp);
      break;
    case kIeee754Float64Log:
      ASSEMBLE_IEEE754_UNOP(log);
      break;
    case kIeee754Float64Log1p:
      ASSEMBLE_IEEE754_UNOP(log1p);
      break;
    case kIeee754Float64Log2:
      ASSEMBLE_IEEE754_UNOP(log2);
      break;
    case kIeee754Float64Log10:
      ASSEMBLE_IEEE754_UNOP(log10);
      break;
    case kIeee754Float64Pow: {
      // TODO(bmeurer): Improve integration of the stub.
      if (!i.InputDoubleRegister(1).is(xmm2)) {
        __ movaps(xmm2, i.InputDoubleRegister(0));
        __ movaps(xmm1, i.InputDoubleRegister(1));
      } else {
        __ movaps(xmm0, i.InputDoubleRegister(0));
        __ movaps(xmm1, xmm2);
        __ movaps(xmm2, xmm0);
      }
      MathPowStub stub(isolate(), MathPowStub::DOUBLE);
      __ CallStub(&stub);
      __ movaps(i.OutputDoubleRegister(), xmm3);
      break;
    }
    case kIeee754Float64Sin:
      ASSEMBLE_IEEE754_UNOP(sin);
      break;
    case kIeee754Float64Sinh:
      ASSEMBLE_IEEE754_UNOP(sinh);
      break;
    case kIeee754Float64Tan:
      ASSEMBLE_IEEE754_UNOP(tan);
      break;
    case kIeee754Float64Tanh:
      ASSEMBLE_IEEE754_UNOP(tanh);
      break;
    case kIA32Add:
      if (HasImmediateInput(instr, 1)) {
        __ add(i.InputOperand(0), i.InputImmediate(1));
      } else {
        __ add(i.InputRegister(0), i.InputOperand(1));
      }
      break;
    case kIA32And:
      if (HasImmediateInput(instr, 1)) {
        __ and_(i.InputOperand(0), i.InputImmediate(1));
      } else {
        __ and_(i.InputRegister(0), i.InputOperand(1));
      }
      break;
    case kIA32Cmp:
      ASSEMBLE_COMPARE(cmp);
      break;
    case kIA32Cmp16:
      ASSEMBLE_COMPARE(cmpw);
      break;
    case kIA32Cmp8:
      ASSEMBLE_COMPARE(cmpb);
      break;
    case kIA32Test:
      ASSEMBLE_COMPARE(test);
      break;
    case kIA32Test16:
      ASSEMBLE_COMPARE(test_w);
      break;
    case kIA32Test8:
      ASSEMBLE_COMPARE(test_b);
      break;
    case kIA32Imul:
      if (HasImmediateInput(instr, 1)) {
        __ imul(i.OutputRegister(), i.InputOperand(0), i.InputInt32(1));
      } else {
        __ imul(i.OutputRegister(), i.InputOperand(1));
      }
      break;
    case kIA32ImulHigh:
      __ imul(i.InputRegister(1));
      break;
    case kIA32UmulHigh:
      __ mul(i.InputRegister(1));
      break;
    case kIA32Idiv:
      __ cdq();
      __ idiv(i.InputOperand(1));
      break;
    case kIA32Udiv:
      __ Move(edx, Immediate(0));
      __ div(i.InputOperand(1));
      break;
    case kIA32Not:
      __ not_(i.OutputOperand());
      break;
    case kIA32Neg:
      __ neg(i.OutputOperand());
      break;
    case kIA32Or:
      if (HasImmediateInput(instr, 1)) {
        __ or_(i.InputOperand(0), i.InputImmediate(1));
      } else {
        __ or_(i.InputRegister(0), i.InputOperand(1));
      }
      break;
    case kIA32Xor:
      if (HasImmediateInput(instr, 1)) {
        __ xor_(i.InputOperand(0), i.InputImmediate(1));
      } else {
        __ xor_(i.InputRegister(0), i.InputOperand(1));
      }
      break;
    case kIA32Sub:
      if (HasImmediateInput(instr, 1)) {
        __ sub(i.InputOperand(0), i.InputImmediate(1));
      } else {
        __ sub(i.InputRegister(0), i.InputOperand(1));
      }
      break;
    case kIA32Shl:
      if (HasImmediateInput(instr, 1)) {
        __ shl(i.OutputOperand(), i.InputInt5(1));
      } else {
        __ shl_cl(i.OutputOperand());
      }
      break;
    case kIA32Shr:
      if (HasImmediateInput(instr, 1)) {
        __ shr(i.OutputOperand(), i.InputInt5(1));
      } else {
        __ shr_cl(i.OutputOperand());
      }
      break;
    case kIA32Sar:
      if (HasImmediateInput(instr, 1)) {
        __ sar(i.OutputOperand(), i.InputInt5(1));
      } else {
        __ sar_cl(i.OutputOperand());
      }
      break;
    case kIA32AddPair: {
      // i.OutputRegister(0) == i.InputRegister(0) ... left low word.
      // i.InputRegister(1) ... left high word.
      // i.InputRegister(2) ... right low word.
      // i.InputRegister(3) ... right high word.
      bool use_temp = false;
      if (i.OutputRegister(0).code() == i.InputRegister(1).code() ||
          i.OutputRegister(0).code() == i.InputRegister(3).code()) {
        // We cannot write to the output register directly, because it would
        // overwrite an input for adc. We have to use the temp register.
        use_temp = true;
        __ Move(i.TempRegister(0), i.InputRegister(0));
        __ add(i.TempRegister(0), i.InputRegister(2));
      } else {
        __ add(i.OutputRegister(0), i.InputRegister(2));
      }
      if (i.OutputRegister(1).code() != i.InputRegister(1).code()) {
        __ Move(i.OutputRegister(1), i.InputRegister(1));
      }
      __ adc(i.OutputRegister(1), Operand(i.InputRegister(3)));
      if (use_temp) {
        __ Move(i.OutputRegister(0), i.TempRegister(0));
      }
      break;
    }
    case kIA32SubPair: {
      // i.OutputRegister(0) == i.InputRegister(0) ... left low word.
      // i.InputRegister(1) ... left high word.
      // i.InputRegister(2) ... right low word.
      // i.InputRegister(3) ... right high word.
      bool use_temp = false;
      if (i.OutputRegister(0).code() == i.InputRegister(1).code() ||
          i.OutputRegister(0).code() == i.InputRegister(3).code()) {
        // We cannot write to the output register directly, because it would
        // overwrite an input for adc. We have to use the temp register.
        use_temp = true;
        __ Move(i.TempRegister(0), i.InputRegister(0));
        __ sub(i.TempRegister(0), i.InputRegister(2));
      } else {
        __ sub(i.OutputRegister(0), i.InputRegister(2));
      }
      if (i.OutputRegister(1).code() != i.InputRegister(1).code()) {
        __ Move(i.OutputRegister(1), i.InputRegister(1));
      }
      __ sbb(i.OutputRegister(1), Operand(i.InputRegister(3)));
      if (use_temp) {
        __ Move(i.OutputRegister(0), i.TempRegister(0));
      }
      break;
    }
    case kIA32MulPair: {
      __ imul(i.OutputRegister(1), i.InputOperand(0));
      __ mov(i.TempRegister(0), i.InputOperand(1));
      __ imul(i.TempRegister(0), i.InputOperand(2));
      __ add(i.OutputRegister(1), i.TempRegister(0));
      __ mov(i.OutputRegister(0), i.InputOperand(0));
      // Multiplies the low words and stores them in eax and edx.
      __ mul(i.InputRegister(2));
      __ add(i.OutputRegister(1), i.TempRegister(0));

      break;
    }
    case kIA32ShlPair:
      if (HasImmediateInput(instr, 2)) {
        __ ShlPair(i.InputRegister(1), i.InputRegister(0), i.InputInt6(2));
      } else {
        // Shift has been loaded into CL by the register allocator.
        __ ShlPair_cl(i.InputRegister(1), i.InputRegister(0));
      }
      break;
    case kIA32ShrPair:
      if (HasImmediateInput(instr, 2)) {
        __ ShrPair(i.InputRegister(1), i.InputRegister(0), i.InputInt6(2));
      } else {
        // Shift has been loaded into CL by the register allocator.
        __ ShrPair_cl(i.InputRegister(1), i.InputRegister(0));
      }
      break;
    case kIA32SarPair:
      if (HasImmediateInput(instr, 2)) {
        __ SarPair(i.InputRegister(1), i.InputRegister(0), i.InputInt6(2));
      } else {
        // Shift has been loaded into CL by the register allocator.
        __ SarPair_cl(i.InputRegister(1), i.InputRegister(0));
      }
      break;
    case kIA32Ror:
      if (HasImmediateInput(instr, 1)) {
        __ ror(i.OutputOperand(), i.InputInt5(1));
      } else {
        __ ror_cl(i.OutputOperand());
      }
      break;
    case kIA32Lzcnt:
      __ Lzcnt(i.OutputRegister(), i.InputOperand(0));
      break;
    case kIA32Tzcnt:
      __ Tzcnt(i.OutputRegister(), i.InputOperand(0));
      break;
    case kIA32Popcnt:
      __ Popcnt(i.OutputRegister(), i.InputOperand(0));
      break;
    case kSSEFloat32Cmp:
      __ ucomiss(i.InputDoubleRegister(0), i.InputOperand(1));
      break;
    case kSSEFloat32Add:
      __ addss(i.InputDoubleRegister(0), i.InputOperand(1));
      break;
    case kSSEFloat32Sub:
      __ subss(i.InputDoubleRegister(0), i.InputOperand(1));
      break;
    case kSSEFloat32Mul:
      __ mulss(i.InputDoubleRegister(0), i.InputOperand(1));
      break;
    case kSSEFloat32Div:
      __ divss(i.InputDoubleRegister(0), i.InputOperand(1));
      // Don't delete this mov. It may improve performance on some CPUs,
      // when there is a (v)mulss depending on the result.
      __ movaps(i.OutputDoubleRegister(), i.OutputDoubleRegister());
      break;
    case kSSEFloat32Sqrt:
      __ sqrtss(i.OutputDoubleRegister(), i.InputOperand(0));
      break;
    case kSSEFloat32Abs: {
      // TODO(bmeurer): Use 128-bit constants.
      __ pcmpeqd(kScratchDoubleReg, kScratchDoubleReg);
      __ psrlq(kScratchDoubleReg, 33);
      __ andps(i.OutputDoubleRegister(), kScratchDoubleReg);
      break;
    }
    case kSSEFloat32Neg: {
      // TODO(bmeurer): Use 128-bit constants.
      __ pcmpeqd(kScratchDoubleReg, kScratchDoubleReg);
      __ psllq(kScratchDoubleReg, 31);
      __ xorps(i.OutputDoubleRegister(), kScratchDoubleReg);
      break;
    }
    case kSSEFloat32Round: {
      CpuFeatureScope sse_scope(masm(), SSE4_1);
      RoundingMode const mode =
          static_cast<RoundingMode>(MiscField::decode(instr->opcode()));
      __ roundss(i.OutputDoubleRegister(), i.InputDoubleRegister(0), mode);
      break;
    }
    case kSSEFloat64Cmp:
      __ ucomisd(i.InputDoubleRegister(0), i.InputOperand(1));
      break;
    case kSSEFloat64Add:
      __ addsd(i.InputDoubleRegister(0), i.InputOperand(1));
      break;
    case kSSEFloat64Sub:
      __ subsd(i.InputDoubleRegister(0), i.InputOperand(1));
      break;
    case kSSEFloat64Mul:
      __ mulsd(i.InputDoubleRegister(0), i.InputOperand(1));
      break;
    case kSSEFloat64Div:
      __ divsd(i.InputDoubleRegister(0), i.InputOperand(1));
      // Don't delete this mov. It may improve performance on some CPUs,
      // when there is a (v)mulsd depending on the result.
      __ movaps(i.OutputDoubleRegister(), i.OutputDoubleRegister());
      break;
    case kSSEFloat32Max: {
      Label compare_nan, compare_swap, done_compare;
      if (instr->InputAt(1)->IsFPRegister()) {
        __ ucomiss(i.InputDoubleRegister(0), i.InputDoubleRegister(1));
      } else {
        __ ucomiss(i.InputDoubleRegister(0), i.InputOperand(1));
      }
      auto ool =
          new (zone()) OutOfLineLoadFloat32NaN(this, i.OutputDoubleRegister());
      __ j(parity_even, ool->entry());
      __ j(above, &done_compare, Label::kNear);
      __ j(below, &compare_swap, Label::kNear);
      __ movmskps(i.TempRegister(0), i.InputDoubleRegister(0));
      __ test(i.TempRegister(0), Immediate(1));
      __ j(zero, &done_compare, Label::kNear);
      __ bind(&compare_swap);
      if (instr->InputAt(1)->IsFPRegister()) {
        __ movss(i.InputDoubleRegister(0), i.InputDoubleRegister(1));
      } else {
        __ movss(i.InputDoubleRegister(0), i.InputOperand(1));
      }
      __ bind(&done_compare);
      __ bind(ool->exit());
      break;
    }

    case kSSEFloat64Max: {
      Label compare_nan, compare_swap, done_compare;
      if (instr->InputAt(1)->IsFPRegister()) {
        __ ucomisd(i.InputDoubleRegister(0), i.InputDoubleRegister(1));
      } else {
        __ ucomisd(i.InputDoubleRegister(0), i.InputOperand(1));
      }
      auto ool =
          new (zone()) OutOfLineLoadFloat64NaN(this, i.OutputDoubleRegister());
      __ j(parity_even, ool->entry());
      __ j(above, &done_compare, Label::kNear);
      __ j(below, &compare_swap, Label::kNear);
      __ movmskpd(i.TempRegister(0), i.InputDoubleRegister(0));
      __ test(i.TempRegister(0), Immediate(1));
      __ j(zero, &done_compare, Label::kNear);
      __ bind(&compare_swap);
      if (instr->InputAt(1)->IsFPRegister()) {
        __ movsd(i.InputDoubleRegister(0), i.InputDoubleRegister(1));
      } else {
        __ movsd(i.InputDoubleRegister(0), i.InputOperand(1));
      }
      __ bind(&done_compare);
      __ bind(ool->exit());
      break;
    }
    case kSSEFloat32Min: {
      Label compare_swap, done_compare;
      if (instr->InputAt(1)->IsFPRegister()) {
        __ ucomiss(i.InputDoubleRegister(0), i.InputDoubleRegister(1));
      } else {
        __ ucomiss(i.InputDoubleRegister(0), i.InputOperand(1));
      }
      auto ool =
          new (zone()) OutOfLineLoadFloat32NaN(this, i.OutputDoubleRegister());
      __ j(parity_even, ool->entry());
      __ j(below, &done_compare, Label::kNear);
      __ j(above, &compare_swap, Label::kNear);
      if (instr->InputAt(1)->IsFPRegister()) {
        __ movmskps(i.TempRegister(0), i.InputDoubleRegister(1));
      } else {
        __ movss(kScratchDoubleReg, i.InputOperand(1));
        __ movmskps(i.TempRegister(0), kScratchDoubleReg);
      }
      __ test(i.TempRegister(0), Immediate(1));
      __ j(zero, &done_compare, Label::kNear);
      __ bind(&compare_swap);
      if (instr->InputAt(1)->IsFPRegister()) {
        __ movss(i.InputDoubleRegister(0), i.InputDoubleRegister(1));
      } else {
        __ movss(i.InputDoubleRegister(0), i.InputOperand(1));
      }
      __ bind(&done_compare);
      __ bind(ool->exit());
      break;
    }
    case kSSEFloat64Min: {
      Label compare_swap, done_compare;
      if (instr->InputAt(1)->IsFPRegister()) {
        __ ucomisd(i.InputDoubleRegister(0), i.InputDoubleRegister(1));
      } else {
        __ ucomisd(i.InputDoubleRegister(0), i.InputOperand(1));
      }
      auto ool =
          new (zone()) OutOfLineLoadFloat64NaN(this, i.OutputDoubleRegister());
      __ j(parity_even, ool->entry());
      __ j(below, &done_compare, Label::kNear);
      __ j(above, &compare_swap, Label::kNear);
      if (instr->InputAt(1)->IsFPRegister()) {
        __ movmskpd(i.TempRegister(0), i.InputDoubleRegister(1));
      } else {
        __ movsd(kScratchDoubleReg, i.InputOperand(1));
        __ movmskpd(i.TempRegister(0), kScratchDoubleReg);
      }
      __ test(i.TempRegister(0), Immediate(1));
      __ j(zero, &done_compare, Label::kNear);
      __ bind(&compare_swap);
      if (instr->InputAt(1)->IsFPRegister()) {
        __ movsd(i.InputDoubleRegister(0), i.InputDoubleRegister(1));
      } else {
        __ movsd(i.InputDoubleRegister(0), i.InputOperand(1));
      }
      __ bind(&done_compare);
      __ bind(ool->exit());
      break;
    }
    case kSSEFloat64Mod: {
      // TODO(dcarney): alignment is wrong.
      __ sub(esp, Immediate(kDoubleSize));
      // Move values to st(0) and st(1).
      __ movsd(Operand(esp, 0), i.InputDoubleRegister(1));
      __ fld_d(Operand(esp, 0));
      __ movsd(Operand(esp, 0), i.InputDoubleRegister(0));
      __ fld_d(Operand(esp, 0));
      // Loop while fprem isn't done.
      Label mod_loop;
      __ bind(&mod_loop);
      // This instructions traps on all kinds inputs, but we are assuming the
      // floating point control word is set to ignore them all.
      __ fprem();
      // The following 2 instruction implicitly use eax.
      __ fnstsw_ax();
      __ sahf();
      __ j(parity_even, &mod_loop);
      // Move output to stack and clean up.
      __ fstp(1);
      __ fstp_d(Operand(esp, 0));
      __ movsd(i.OutputDoubleRegister(), Operand(esp, 0));
      __ add(esp, Immediate(kDoubleSize));
      break;
    }
    case kSSEFloat64Abs: {
      // TODO(bmeurer): Use 128-bit constants.
      __ pcmpeqd(kScratchDoubleReg, kScratchDoubleReg);
      __ psrlq(kScratchDoubleReg, 1);
      __ andpd(i.OutputDoubleRegister(), kScratchDoubleReg);
      break;
    }
    case kSSEFloat64Neg: {
      // TODO(bmeurer): Use 128-bit constants.
      __ pcmpeqd(kScratchDoubleReg, kScratchDoubleReg);
      __ psllq(kScratchDoubleReg, 63);
      __ xorpd(i.OutputDoubleRegister(), kScratchDoubleReg);
      break;
    }
    case kSSEFloat64Sqrt:
      __ sqrtsd(i.OutputDoubleRegister(), i.InputOperand(0));
      break;
    case kSSEFloat64Round: {
      CpuFeatureScope sse_scope(masm(), SSE4_1);
      RoundingMode const mode =
          static_cast<RoundingMode>(MiscField::decode(instr->opcode()));
      __ roundsd(i.OutputDoubleRegister(), i.InputDoubleRegister(0), mode);
      break;
    }
    case kSSEFloat32ToFloat64:
      __ cvtss2sd(i.OutputDoubleRegister(), i.InputOperand(0));
      break;
    case kSSEFloat64ToFloat32:
      __ cvtsd2ss(i.OutputDoubleRegister(), i.InputOperand(0));
      break;
    case kSSEFloat32ToInt32:
      __ cvttss2si(i.OutputRegister(), i.InputOperand(0));
      break;
    case kSSEFloat32ToUint32: {
      Label success;
      __ cvttss2si(i.OutputRegister(), i.InputOperand(0));
      __ test(i.OutputRegister(), i.OutputRegister());
      __ j(positive, &success);
      __ Move(kScratchDoubleReg, static_cast<float>(INT32_MIN));
      __ addss(kScratchDoubleReg, i.InputOperand(0));
      __ cvttss2si(i.OutputRegister(), kScratchDoubleReg);
      __ or_(i.OutputRegister(), Immediate(0x80000000));
      __ bind(&success);
      break;
    }
    case kSSEFloat64ToInt32:
      __ cvttsd2si(i.OutputRegister(), i.InputOperand(0));
      break;
    case kSSEFloat64ToUint32: {
      __ Move(kScratchDoubleReg, -2147483648.0);
      __ addsd(kScratchDoubleReg, i.InputOperand(0));
      __ cvttsd2si(i.OutputRegister(), kScratchDoubleReg);
      __ add(i.OutputRegister(), Immediate(0x80000000));
      break;
    }
    case kSSEInt32ToFloat32:
      __ cvtsi2ss(i.OutputDoubleRegister(), i.InputOperand(0));
      break;
    case kSSEUint32ToFloat32: {
      Register scratch0 = i.TempRegister(0);
      Register scratch1 = i.TempRegister(1);
      __ mov(scratch0, i.InputOperand(0));
      __ Cvtui2ss(i.OutputDoubleRegister(), scratch0, scratch1);
      break;
    }
    case kSSEInt32ToFloat64:
      __ cvtsi2sd(i.OutputDoubleRegister(), i.InputOperand(0));
      break;
    case kSSEUint32ToFloat64:
      __ LoadUint32(i.OutputDoubleRegister(), i.InputOperand(0));
      break;
    case kSSEFloat64ExtractLowWord32:
      if (instr->InputAt(0)->IsFPStackSlot()) {
        __ mov(i.OutputRegister(), i.InputOperand(0));
      } else {
        __ movd(i.OutputRegister(), i.InputDoubleRegister(0));
      }
      break;
    case kSSEFloat64ExtractHighWord32:
      if (instr->InputAt(0)->IsFPStackSlot()) {
        __ mov(i.OutputRegister(), i.InputOperand(0, kDoubleSize / 2));
      } else {
        __ Pextrd(i.OutputRegister(), i.InputDoubleRegister(0), 1);
      }
      break;
    case kSSEFloat64InsertLowWord32:
      __ Pinsrd(i.OutputDoubleRegister(), i.InputOperand(1), 0);
      break;
    case kSSEFloat64InsertHighWord32:
      __ Pinsrd(i.OutputDoubleRegister(), i.InputOperand(1), 1);
      break;
    case kSSEFloat64LoadLowWord32:
      __ movd(i.OutputDoubleRegister(), i.InputOperand(0));
      break;
    case kAVXFloat32Add: {
      CpuFeatureScope avx_scope(masm(), AVX);
      __ vaddss(i.OutputDoubleRegister(), i.InputDoubleRegister(0),
                i.InputOperand(1));
      break;
    }
    case kAVXFloat32Sub: {
      CpuFeatureScope avx_scope(masm(), AVX);
      __ vsubss(i.OutputDoubleRegister(), i.InputDoubleRegister(0),
                i.InputOperand(1));
      break;
    }
    case kAVXFloat32Mul: {
      CpuFeatureScope avx_scope(masm(), AVX);
      __ vmulss(i.OutputDoubleRegister(), i.InputDoubleRegister(0),
                i.InputOperand(1));
      break;
    }
    case kAVXFloat32Div: {
      CpuFeatureScope avx_scope(masm(), AVX);
      __ vdivss(i.OutputDoubleRegister(), i.InputDoubleRegister(0),
                i.InputOperand(1));
      // Don't delete this mov. It may improve performance on some CPUs,
      // when there is a (v)mulss depending on the result.
      __ movaps(i.OutputDoubleRegister(), i.OutputDoubleRegister());
      break;
    }
    case kAVXFloat64Add: {
      CpuFeatureScope avx_scope(masm(), AVX);
      __ vaddsd(i.OutputDoubleRegister(), i.InputDoubleRegister(0),
                i.InputOperand(1));
      break;
    }
    case kAVXFloat64Sub: {
      CpuFeatureScope avx_scope(masm(), AVX);
      __ vsubsd(i.OutputDoubleRegister(), i.InputDoubleRegister(0),
                i.InputOperand(1));
      break;
    }
    case kAVXFloat64Mul: {
      CpuFeatureScope avx_scope(masm(), AVX);
      __ vmulsd(i.OutputDoubleRegister(), i.InputDoubleRegister(0),
                i.InputOperand(1));
      break;
    }
    case kAVXFloat64Div: {
      CpuFeatureScope avx_scope(masm(), AVX);
      __ vdivsd(i.OutputDoubleRegister(), i.InputDoubleRegister(0),
                i.InputOperand(1));
      // Don't delete this mov. It may improve performance on some CPUs,
      // when there is a (v)mulsd depending on the result.
      __ movaps(i.OutputDoubleRegister(), i.OutputDoubleRegister());
      break;
    }
    case kAVXFloat32Abs: {
      // TODO(bmeurer): Use RIP relative 128-bit constants.
      __ pcmpeqd(kScratchDoubleReg, kScratchDoubleReg);
      __ psrlq(kScratchDoubleReg, 33);
      CpuFeatureScope avx_scope(masm(), AVX);
      __ vandps(i.OutputDoubleRegister(), kScratchDoubleReg, i.InputOperand(0));
      break;
    }
    case kAVXFloat32Neg: {
      // TODO(bmeurer): Use RIP relative 128-bit constants.
      __ pcmpeqd(kScratchDoubleReg, kScratchDoubleReg);
      __ psllq(kScratchDoubleReg, 31);
      CpuFeatureScope avx_scope(masm(), AVX);
      __ vxorps(i.OutputDoubleRegister(), kScratchDoubleReg, i.InputOperand(0));
      break;
    }
    case kAVXFloat64Abs: {
      // TODO(bmeurer): Use RIP relative 128-bit constants.
      __ pcmpeqd(kScratchDoubleReg, kScratchDoubleReg);
      __ psrlq(kScratchDoubleReg, 1);
      CpuFeatureScope avx_scope(masm(), AVX);
      __ vandpd(i.OutputDoubleRegister(), kScratchDoubleReg, i.InputOperand(0));
      break;
    }
    case kAVXFloat64Neg: {
      // TODO(bmeurer): Use RIP relative 128-bit constants.
      __ pcmpeqd(kScratchDoubleReg, kScratchDoubleReg);
      __ psllq(kScratchDoubleReg, 63);
      CpuFeatureScope avx_scope(masm(), AVX);
      __ vxorpd(i.OutputDoubleRegister(), kScratchDoubleReg, i.InputOperand(0));
      break;
    }
    case kSSEFloat64SilenceNaN:
      __ xorpd(kScratchDoubleReg, kScratchDoubleReg);
      __ subsd(i.InputDoubleRegister(0), kScratchDoubleReg);
      break;
    case kIA32Movsxbl:
      __ movsx_b(i.OutputRegister(), i.MemoryOperand());
      break;
    case kIA32Movzxbl:
      __ movzx_b(i.OutputRegister(), i.MemoryOperand());
      break;
    case kIA32Movb: {
      size_t index = 0;
      Operand operand = i.MemoryOperand(&index);
      if (HasImmediateInput(instr, index)) {
        __ mov_b(operand, i.InputInt8(index));
      } else {
        __ mov_b(operand, i.InputRegister(index));
      }
      break;
    }
    case kIA32Movsxwl:
      __ movsx_w(i.OutputRegister(), i.MemoryOperand());
      break;
    case kIA32Movzxwl:
      __ movzx_w(i.OutputRegister(), i.MemoryOperand());
      break;
    case kIA32Movw: {
      size_t index = 0;
      Operand operand = i.MemoryOperand(&index);
      if (HasImmediateInput(instr, index)) {
        __ mov_w(operand, i.InputInt16(index));
      } else {
        __ mov_w(operand, i.InputRegister(index));
      }
      break;
    }
    case kIA32Movl:
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
    case kIA32Movsd:
      if (instr->HasOutput()) {
        __ movsd(i.OutputDoubleRegister(), i.MemoryOperand());
      } else {
        size_t index = 0;
        Operand operand = i.MemoryOperand(&index);
        __ movsd(operand, i.InputDoubleRegister(index));
      }
      break;
    case kIA32Movss:
      if (instr->HasOutput()) {
        __ movss(i.OutputDoubleRegister(), i.MemoryOperand());
      } else {
        size_t index = 0;
        Operand operand = i.MemoryOperand(&index);
        __ movss(operand, i.InputDoubleRegister(index));
      }
      break;
    case kIA32BitcastFI:
      if (instr->InputAt(0)->IsFPStackSlot()) {
        __ mov(i.OutputRegister(), i.InputOperand(0));
      } else {
        __ movd(i.OutputRegister(), i.InputDoubleRegister(0));
      }
      break;
    case kIA32BitcastIF:
      if (instr->InputAt(0)->IsRegister()) {
        __ movd(i.OutputDoubleRegister(), i.InputRegister(0));
      } else {
        __ movss(i.OutputDoubleRegister(), i.InputOperand(0));
      }
      break;
    case kIA32Lea: {
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
            __ add(i.OutputRegister(), i.InputRegister(1));
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
      } else if (mode == kMode_MR1 &&
                 i.InputRegister(1).is(i.OutputRegister())) {
        __ add(i.OutputRegister(), i.InputRegister(0));
      } else {
        __ lea(i.OutputRegister(), i.MemoryOperand());
      }
      break;
    }
    case kIA32PushFloat32:
      if (instr->InputAt(0)->IsFPRegister()) {
        __ sub(esp, Immediate(kFloatSize));
        __ movss(Operand(esp, 0), i.InputDoubleRegister(0));
        frame_access_state()->IncreaseSPDelta(kFloatSize / kPointerSize);
      } else if (HasImmediateInput(instr, 0)) {
        __ Move(kScratchDoubleReg, i.InputDouble(0));
        __ sub(esp, Immediate(kDoubleSize));
        __ movss(Operand(esp, 0), kScratchDoubleReg);
        frame_access_state()->IncreaseSPDelta(kDoubleSize / kPointerSize);
      } else {
        __ movsd(kScratchDoubleReg, i.InputOperand(0));
        __ sub(esp, Immediate(kDoubleSize));
        __ movss(Operand(esp, 0), kScratchDoubleReg);
        frame_access_state()->IncreaseSPDelta(kDoubleSize / kPointerSize);
      }
      break;
    case kIA32PushFloat64:
      if (instr->InputAt(0)->IsFPRegister()) {
        __ sub(esp, Immediate(kDoubleSize));
        __ movsd(Operand(esp, 0), i.InputDoubleRegister(0));
        frame_access_state()->IncreaseSPDelta(kDoubleSize / kPointerSize);
      } else if (HasImmediateInput(instr, 0)) {
        __ Move(kScratchDoubleReg, i.InputDouble(0));
        __ sub(esp, Immediate(kDoubleSize));
        __ movsd(Operand(esp, 0), kScratchDoubleReg);
        frame_access_state()->IncreaseSPDelta(kDoubleSize / kPointerSize);
      } else {
        __ movsd(kScratchDoubleReg, i.InputOperand(0));
        __ sub(esp, Immediate(kDoubleSize));
        __ movsd(Operand(esp, 0), kScratchDoubleReg);
        frame_access_state()->IncreaseSPDelta(kDoubleSize / kPointerSize);
      }
      break;
    case kIA32Push:
      if (instr->InputAt(0)->IsFPRegister()) {
        __ sub(esp, Immediate(kFloatSize));
        __ movsd(Operand(esp, 0), i.InputDoubleRegister(0));
        frame_access_state()->IncreaseSPDelta(kFloatSize / kPointerSize);
      } else if (HasImmediateInput(instr, 0)) {
        __ push(i.InputImmediate(0));
        frame_access_state()->IncreaseSPDelta(1);
      } else {
        __ push(i.InputOperand(0));
        frame_access_state()->IncreaseSPDelta(1);
      }
      break;
    case kIA32Poke: {
      int const slot = MiscField::decode(instr->opcode());
      if (HasImmediateInput(instr, 0)) {
        __ mov(Operand(esp, slot * kPointerSize), i.InputImmediate(0));
      } else {
        __ mov(Operand(esp, slot * kPointerSize), i.InputRegister(0));
      }
      break;
    }
    case kIA32Xchgb: {
      size_t index = 0;
      Operand operand = i.MemoryOperand(&index);
      __ xchg_b(i.InputRegister(index), operand);
      break;
    }
    case kIA32Xchgw: {
      size_t index = 0;
      Operand operand = i.MemoryOperand(&index);
      __ xchg_w(i.InputRegister(index), operand);
      break;
    }
    case kIA32Xchgl: {
      size_t index = 0;
      Operand operand = i.MemoryOperand(&index);
      __ xchg(i.InputRegister(index), operand);
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
      ASSEMBLE_CHECKED_LOAD_FLOAT(movss, OutOfLineLoadFloat32NaN);
      break;
    case kCheckedLoadFloat64:
      ASSEMBLE_CHECKED_LOAD_FLOAT(movsd, OutOfLineLoadFloat64NaN);
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
      ASSEMBLE_CHECKED_STORE_FLOAT(movss);
      break;
    case kCheckedStoreFloat64:
      ASSEMBLE_CHECKED_STORE_FLOAT(movsd);
      break;
    case kIA32StackCheck: {
      ExternalReference const stack_limit =
          ExternalReference::address_of_stack_limit(isolate());
      __ cmp(esp, Operand::StaticVariable(stack_limit));
      break;
    }
    case kCheckedLoadWord64:
    case kCheckedStoreWord64:
      UNREACHABLE();  // currently unsupported checked int64 load/store.
      break;
    case kAtomicLoadInt8:
    case kAtomicLoadUint8:
    case kAtomicLoadInt16:
    case kAtomicLoadUint16:
    case kAtomicLoadWord32:
    case kAtomicStoreWord8:
    case kAtomicStoreWord16:
    case kAtomicStoreWord32:
      UNREACHABLE();  // Won't be generated by instruction selector.
      break;
  }
  return kSuccess;
}  // NOLINT(readability/fn_size)

static Condition FlagsConditionToCondition(FlagsCondition condition) {
  switch (condition) {
    case kUnorderedEqual:
    case kEqual:
      return equal;
      break;
    case kUnorderedNotEqual:
    case kNotEqual:
      return not_equal;
      break;
    case kSignedLessThan:
      return less;
      break;
    case kSignedGreaterThanOrEqual:
      return greater_equal;
      break;
    case kSignedLessThanOrEqual:
      return less_equal;
      break;
    case kSignedGreaterThan:
      return greater;
      break;
    case kUnsignedLessThan:
      return below;
      break;
    case kUnsignedGreaterThanOrEqual:
      return above_equal;
      break;
    case kUnsignedLessThanOrEqual:
      return below_equal;
      break;
    case kUnsignedGreaterThan:
      return above;
      break;
    case kOverflow:
      return overflow;
      break;
    case kNotOverflow:
      return no_overflow;
      break;
    default:
      UNREACHABLE();
      return no_condition;
      break;
  }
}

// Assembles a branch after an instruction.
void CodeGenerator::AssembleArchBranch(Instruction* instr, BranchInfo* branch) {
  Label::Distance flabel_distance =
      branch->fallthru ? Label::kNear : Label::kFar;
  Label* tlabel = branch->true_label;
  Label* flabel = branch->false_label;
  if (branch->condition == kUnorderedEqual) {
    __ j(parity_even, flabel, flabel_distance);
  } else if (branch->condition == kUnorderedNotEqual) {
    __ j(parity_even, tlabel);
  }
  __ j(FlagsConditionToCondition(branch->condition), tlabel);

  // Add a jump if not falling through to the next block.
  if (!branch->fallthru) __ jmp(flabel);
}


void CodeGenerator::AssembleArchJump(RpoNumber target) {
  if (!IsNextInAssemblyOrder(target)) __ jmp(GetLabel(target));
}

void CodeGenerator::AssembleArchTrap(Instruction* instr,
                                     FlagsCondition condition) {
  class OutOfLineTrap final : public OutOfLineCode {
   public:
    OutOfLineTrap(CodeGenerator* gen, bool frame_elided, Instruction* instr)
        : OutOfLineCode(gen),
          frame_elided_(frame_elided),
          instr_(instr),
          gen_(gen) {}

    void Generate() final {
      IA32OperandConverter i(gen_, instr_);

      Runtime::FunctionId trap_id = static_cast<Runtime::FunctionId>(
          i.InputInt32(instr_->InputCount() - 1));
      bool old_has_frame = __ has_frame();
      if (frame_elided_) {
        __ set_has_frame(true);
        __ EnterFrame(StackFrame::WASM_COMPILED);
      }
      GenerateCallToTrap(trap_id);
      if (frame_elided_) {
        ReferenceMap* reference_map =
            new (gen_->zone()) ReferenceMap(gen_->zone());
        gen_->RecordSafepoint(reference_map, Safepoint::kSimple, 0,
                              Safepoint::kNoLazyDeopt);
        __ set_has_frame(old_has_frame);
      }
      if (FLAG_debug_code) {
        __ ud2();
      }
    }

   private:
    void GenerateCallToTrap(Runtime::FunctionId trap_id) {
      if (trap_id == Runtime::kNumFunctions) {
        // We cannot test calls to the runtime in cctest/test-run-wasm.
        // Therefore we emit a call to C here instead of a call to the runtime.
        __ PrepareCallCFunction(0, esi);
        __ CallCFunction(
            ExternalReference::wasm_call_trap_callback_for_testing(isolate()),
            0);
      } else {
        __ Move(esi, isolate()->native_context());
        gen_->AssembleSourcePosition(instr_);
        __ CallRuntime(trap_id);
      }
    }

    bool frame_elided_;
    Instruction* instr_;
    CodeGenerator* gen_;
  };
  bool frame_elided = !frame_access_state()->has_frame();
  auto ool = new (zone()) OutOfLineTrap(this, frame_elided, instr);
  Label* tlabel = ool->entry();
  Label end;
  if (condition == kUnorderedEqual) {
    __ j(parity_even, &end);
  } else if (condition == kUnorderedNotEqual) {
    __ j(parity_even, tlabel);
  }
  __ j(FlagsConditionToCondition(condition), tlabel);
  __ bind(&end);
}

// Assembles boolean materializations after an instruction.
void CodeGenerator::AssembleArchBoolean(Instruction* instr,
                                        FlagsCondition condition) {
  IA32OperandConverter i(this, instr);
  Label done;

  // Materialize a full 32-bit 1 or 0 value. The result register is always the
  // last output of the instruction.
  Label check;
  DCHECK_NE(0u, instr->OutputCount());
  Register reg = i.OutputRegister(instr->OutputCount() - 1);
  if (condition == kUnorderedEqual) {
    __ j(parity_odd, &check, Label::kNear);
    __ Move(reg, Immediate(0));
    __ jmp(&done, Label::kNear);
  } else if (condition == kUnorderedNotEqual) {
    __ j(parity_odd, &check, Label::kNear);
    __ mov(reg, Immediate(1));
    __ jmp(&done, Label::kNear);
  }
  Condition cc = FlagsConditionToCondition(condition);

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
  IA32OperandConverter i(this, instr);
  Register input = i.InputRegister(0);
  for (size_t index = 2; index < instr->InputCount(); index += 2) {
    __ cmp(input, Immediate(i.InputInt32(index + 0)));
    __ j(equal, GetLabel(i.InputRpo(index + 1)));
  }
  AssembleArchJump(i.InputRpo(1));
}


void CodeGenerator::AssembleArchTableSwitch(Instruction* instr) {
  IA32OperandConverter i(this, instr);
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

CodeGenerator::CodeGenResult CodeGenerator::AssembleDeoptimizerCall(
    int deoptimization_id, Deoptimizer::BailoutType bailout_type,
    SourcePosition pos) {
  Address deopt_entry = Deoptimizer::GetDeoptimizationEntry(
      isolate(), deoptimization_id, bailout_type);
  if (deopt_entry == nullptr) return kTooManyDeoptimizationBailouts;
  DeoptimizeReason deoptimization_reason =
      GetDeoptimizationReason(deoptimization_id);
  __ RecordDeoptReason(deoptimization_reason, pos, deoptimization_id);
  __ call(deopt_entry, RelocInfo::RUNTIME_ENTRY);
  return kSuccess;
}


// The calling convention for JSFunctions on IA32 passes arguments on the
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
// CEntryStub (a real code object). On IA32 passes arguments on the
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

void CodeGenerator::FinishFrame(Frame* frame) {
  CallDescriptor* descriptor = linkage()->GetIncomingDescriptor();
  const RegList saves = descriptor->CalleeSavedRegisters();
  if (saves != 0) {  // Save callee-saved registers.
    DCHECK(!info()->is_osr());
    int pushed = 0;
    for (int i = Register::kNumRegisters - 1; i >= 0; i--) {
      if (!((1 << i) & saves)) continue;
      ++pushed;
    }
    frame->AllocateSavedCalleeRegisterSlots(pushed);
  }
}

void CodeGenerator::AssembleConstructFrame() {
  CallDescriptor* descriptor = linkage()->GetIncomingDescriptor();
  if (frame_access_state()->has_frame()) {
    if (descriptor->IsCFunctionCall()) {
      __ push(ebp);
      __ mov(ebp, esp);
    } else if (descriptor->IsJSFunctionCall()) {
      __ Prologue(this->info()->GeneratePreagedPrologue());
      if (descriptor->PushArgumentCount()) {
        __ push(kJavaScriptCallArgCountRegister);
      }
    } else {
      __ StubPrologue(info()->GetOutputStackFrameType());
    }
  }

  int shrink_slots =
      frame()->GetTotalFrameSlotCount() - descriptor->CalculateFixedFrameSize();

  if (info()->is_osr()) {
    // TurboFan OSR-compiled functions cannot be entered directly.
    __ Abort(kShouldNotDirectlyEnterOsrFunction);

    // Unoptimized code jumps directly to this entrypoint while the unoptimized
    // frame is still on the stack. Optimized code uses OSR values directly from
    // the unoptimized frame. Thus, all that needs to be done is to allocate the
    // remaining stack slots.
    if (FLAG_code_comments) __ RecordComment("-- OSR entrypoint --");
    osr_pc_offset_ = __ pc_offset();
    shrink_slots -= OsrHelper(info()).UnoptimizedFrameSlots();
  }

  const RegList saves = descriptor->CalleeSavedRegisters();
  if (shrink_slots > 0) {
    __ sub(esp, Immediate(shrink_slots * kPointerSize));
  }

  if (saves != 0) {  // Save callee-saved registers.
    DCHECK(!info()->is_osr());
    int pushed = 0;
    for (int i = Register::kNumRegisters - 1; i >= 0; i--) {
      if (!((1 << i) & saves)) continue;
      __ push(Register::from_code(i));
      ++pushed;
    }
  }
}

void CodeGenerator::AssembleReturn(InstructionOperand* pop) {
  CallDescriptor* descriptor = linkage()->GetIncomingDescriptor();

  const RegList saves = descriptor->CalleeSavedRegisters();
  // Restore registers.
  if (saves != 0) {
    for (int i = 0; i < Register::kNumRegisters; i++) {
      if (!((1 << i) & saves)) continue;
      __ pop(Register::from_code(i));
    }
  }

  // Might need ecx for scratch if pop_size is too big or if there is a variable
  // pop count.
  DCHECK_EQ(0u, descriptor->CalleeSavedRegisters() & ecx.bit());
  size_t pop_size = descriptor->StackParameterCount() * kPointerSize;
  IA32OperandConverter g(this, nullptr);
  if (descriptor->IsCFunctionCall()) {
    AssembleDeconstructFrame();
  } else if (frame_access_state()->has_frame()) {
    // Canonicalize JSFunction return sites for now if they always have the same
    // number of return args.
    if (pop->IsImmediate() && g.ToConstant(pop).ToInt32() == 0) {
      if (return_label_.is_bound()) {
        __ jmp(&return_label_);
        return;
      } else {
        __ bind(&return_label_);
        AssembleDeconstructFrame();
      }
    } else {
      AssembleDeconstructFrame();
    }
  }
  DCHECK_EQ(0u, descriptor->CalleeSavedRegisters() & edx.bit());
  DCHECK_EQ(0u, descriptor->CalleeSavedRegisters() & ecx.bit());
  if (pop->IsImmediate()) {
    DCHECK_EQ(Constant::kInt32, g.ToConstant(pop).type());
    pop_size += g.ToConstant(pop).ToInt32() * kPointerSize;
    __ Ret(static_cast<int>(pop_size), ecx);
  } else {
    Register pop_reg = g.ToRegister(pop);
    Register scratch_reg = pop_reg.is(ecx) ? edx : ecx;
    __ pop(scratch_reg);
    __ lea(esp, Operand(esp, pop_reg, times_4, static_cast<int>(pop_size)));
    __ jmp(scratch_reg);
  }
}


void CodeGenerator::AssembleMove(InstructionOperand* source,
                                 InstructionOperand* destination) {
  IA32OperandConverter g(this, nullptr);
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
      if (destination->IsRegister()) {
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
      uint32_t src = src_constant.ToFloat32AsInt();
      if (destination->IsFPRegister()) {
        XMMRegister dst = g.ToDoubleRegister(destination);
        __ Move(dst, src);
      } else {
        DCHECK(destination->IsFPStackSlot());
        Operand dst = g.ToOperand(destination);
        __ Move(dst, Immediate(src));
      }
    } else {
      DCHECK_EQ(Constant::kFloat64, src_constant.type());
      uint64_t src = src_constant.ToFloat64AsInt();
      uint32_t lower = static_cast<uint32_t>(src);
      uint32_t upper = static_cast<uint32_t>(src >> 32);
      if (destination->IsFPRegister()) {
        XMMRegister dst = g.ToDoubleRegister(destination);
        __ Move(dst, src);
      } else {
        DCHECK(destination->IsFPStackSlot());
        Operand dst0 = g.ToOperand(destination);
        Operand dst1 = g.HighOperand(destination);
        __ Move(dst0, Immediate(lower));
        __ Move(dst1, Immediate(upper));
      }
    }
  } else if (source->IsFPRegister()) {
    XMMRegister src = g.ToDoubleRegister(source);
    if (destination->IsFPRegister()) {
      XMMRegister dst = g.ToDoubleRegister(destination);
      __ movaps(dst, src);
    } else {
      DCHECK(destination->IsFPStackSlot());
      Operand dst = g.ToOperand(destination);
      MachineRepresentation rep =
          LocationOperand::cast(source)->representation();
      if (rep == MachineRepresentation::kFloat64) {
        __ movsd(dst, src);
      } else if (rep == MachineRepresentation::kFloat32) {
        __ movss(dst, src);
      } else {
        DCHECK_EQ(MachineRepresentation::kSimd128, rep);
        __ movups(dst, src);
      }
    }
  } else if (source->IsFPStackSlot()) {
    DCHECK(destination->IsFPRegister() || destination->IsFPStackSlot());
    Operand src = g.ToOperand(source);
    MachineRepresentation rep = LocationOperand::cast(source)->representation();
    if (destination->IsFPRegister()) {
      XMMRegister dst = g.ToDoubleRegister(destination);
      if (rep == MachineRepresentation::kFloat64) {
        __ movsd(dst, src);
      } else if (rep == MachineRepresentation::kFloat32) {
        __ movss(dst, src);
      } else {
        DCHECK_EQ(MachineRepresentation::kSimd128, rep);
        __ movups(dst, src);
      }
    } else {
      Operand dst = g.ToOperand(destination);
      if (rep == MachineRepresentation::kFloat64) {
        __ movsd(kScratchDoubleReg, src);
        __ movsd(dst, kScratchDoubleReg);
      } else if (rep == MachineRepresentation::kFloat32) {
        __ movss(kScratchDoubleReg, src);
        __ movss(dst, kScratchDoubleReg);
      } else {
        DCHECK_EQ(MachineRepresentation::kSimd128, rep);
        __ movups(kScratchDoubleReg, src);
        __ movups(dst, kScratchDoubleReg);
      }
    }
  } else {
    UNREACHABLE();
  }
}


void CodeGenerator::AssembleSwap(InstructionOperand* source,
                                 InstructionOperand* destination) {
  IA32OperandConverter g(this, nullptr);
  // Dispatch on the source and destination operand kinds.  Not all
  // combinations are possible.
  if (source->IsRegister() && destination->IsRegister()) {
    // Register-register.
    Register src = g.ToRegister(source);
    Register dst = g.ToRegister(destination);
    __ push(src);
    __ mov(src, dst);
    __ pop(dst);
  } else if (source->IsRegister() && destination->IsStackSlot()) {
    // Register-memory.
    Register src = g.ToRegister(source);
    __ push(src);
    frame_access_state()->IncreaseSPDelta(1);
    Operand dst = g.ToOperand(destination);
    __ mov(src, dst);
    frame_access_state()->IncreaseSPDelta(-1);
    dst = g.ToOperand(destination);
    __ pop(dst);
  } else if (source->IsStackSlot() && destination->IsStackSlot()) {
    // Memory-memory.
    Operand dst1 = g.ToOperand(destination);
    __ push(dst1);
    frame_access_state()->IncreaseSPDelta(1);
    Operand src1 = g.ToOperand(source);
    __ push(src1);
    Operand dst2 = g.ToOperand(destination);
    __ pop(dst2);
    frame_access_state()->IncreaseSPDelta(-1);
    Operand src2 = g.ToOperand(source);
    __ pop(src2);
  } else if (source->IsFPRegister() && destination->IsFPRegister()) {
    // XMM register-register swap.
    XMMRegister src = g.ToDoubleRegister(source);
    XMMRegister dst = g.ToDoubleRegister(destination);
    __ movaps(kScratchDoubleReg, src);
    __ movaps(src, dst);
    __ movaps(dst, kScratchDoubleReg);
  } else if (source->IsFPRegister() && destination->IsFPStackSlot()) {
    // XMM register-memory swap.
    XMMRegister reg = g.ToDoubleRegister(source);
    Operand other = g.ToOperand(destination);
    MachineRepresentation rep = LocationOperand::cast(source)->representation();
    if (rep == MachineRepresentation::kFloat64) {
      __ movsd(kScratchDoubleReg, other);
      __ movsd(other, reg);
      __ movaps(reg, kScratchDoubleReg);
    } else if (rep == MachineRepresentation::kFloat32) {
      __ movss(kScratchDoubleReg, other);
      __ movss(other, reg);
      __ movaps(reg, kScratchDoubleReg);
    } else {
      DCHECK_EQ(MachineRepresentation::kSimd128, rep);
      __ movups(kScratchDoubleReg, other);
      __ movups(other, reg);
      __ movups(reg, kScratchDoubleReg);
    }
  } else if (source->IsFPStackSlot() && destination->IsFPStackSlot()) {
    // Double-width memory-to-memory.
    Operand src0 = g.ToOperand(source);
    Operand dst0 = g.ToOperand(destination);
    MachineRepresentation rep = LocationOperand::cast(source)->representation();
    if (rep == MachineRepresentation::kFloat64) {
      Operand src1 = g.HighOperand(source);
      Operand dst1 = g.HighOperand(destination);
      __ movsd(kScratchDoubleReg, dst0);  // Save dst in scratch register.
      __ push(src0);  // Then use stack to copy src to destination.
      __ pop(dst0);
      __ push(src1);
      __ pop(dst1);
      __ movsd(src0, kScratchDoubleReg);
    } else if (rep == MachineRepresentation::kFloat32) {
      __ movss(kScratchDoubleReg, dst0);  // Save dst in scratch register.
      __ push(src0);  // Then use stack to copy src to destination.
      __ pop(dst0);
      __ movss(src0, kScratchDoubleReg);
    } else {
      DCHECK_EQ(MachineRepresentation::kSimd128, rep);
      // Use the XOR trick to swap without a temporary.
      __ movups(kScratchDoubleReg, src0);
      __ xorps(kScratchDoubleReg, dst0);  // scratch contains src ^ dst.
      __ movups(src0, kScratchDoubleReg);
      __ xorps(kScratchDoubleReg, dst0);  // scratch contains src.
      __ movups(dst0, kScratchDoubleReg);
      __ xorps(kScratchDoubleReg, src0);  // scratch contains dst.
      __ movups(src0, kScratchDoubleReg);
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


void CodeGenerator::EnsureSpaceForLazyDeopt() {
  if (!info()->ShouldEnsureSpaceForLazyDeopt()) {
    return;
  }

  int space_needed = Deoptimizer::patch_size();
  // Ensure that we have enough space after the previous lazy-bailout
  // instruction for patching the code here.
  int current_pc = masm()->pc_offset();
  if (current_pc < last_lazy_deopt_pc_ + space_needed) {
    int padding_size = last_lazy_deopt_pc_ + space_needed - current_pc;
    __ Nop(padding_size);
  }
}

#undef __

}  // namespace compiler
}  // namespace internal
}  // namespace v8
