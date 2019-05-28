// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/backend/code-generator.h"

#include "src/arm64/assembler-arm64-inl.h"
#include "src/arm64/macro-assembler-arm64-inl.h"
#include "src/compiler/backend/code-generator-impl.h"
#include "src/compiler/backend/gap-resolver.h"
#include "src/compiler/node-matchers.h"
#include "src/compiler/osr.h"
#include "src/frame-constants.h"
#include "src/heap/heap-inl.h"  // crbug.com/v8/8499
#include "src/optimized-compilation-info.h"
#include "src/wasm/wasm-code-manager.h"
#include "src/wasm/wasm-objects.h"

namespace v8 {
namespace internal {
namespace compiler {

#define __ tasm()->

// Adds Arm64-specific methods to convert InstructionOperands.
class Arm64OperandConverter final : public InstructionOperandConverter {
 public:
  Arm64OperandConverter(CodeGenerator* gen, Instruction* instr)
      : InstructionOperandConverter(gen, instr) {}

  DoubleRegister InputFloat32Register(size_t index) {
    return InputDoubleRegister(index).S();
  }

  DoubleRegister InputFloat64Register(size_t index) {
    return InputDoubleRegister(index);
  }

  DoubleRegister InputSimd128Register(size_t index) {
    return InputDoubleRegister(index).Q();
  }

  CPURegister InputFloat32OrZeroRegister(size_t index) {
    if (instr_->InputAt(index)->IsImmediate()) {
      DCHECK_EQ(0, bit_cast<int32_t>(InputFloat32(index)));
      return wzr;
    }
    DCHECK(instr_->InputAt(index)->IsFPRegister());
    return InputDoubleRegister(index).S();
  }

  CPURegister InputFloat64OrZeroRegister(size_t index) {
    if (instr_->InputAt(index)->IsImmediate()) {
      DCHECK_EQ(0, bit_cast<int64_t>(InputDouble(index)));
      return xzr;
    }
    DCHECK(instr_->InputAt(index)->IsDoubleRegister());
    return InputDoubleRegister(index);
  }

  size_t OutputCount() { return instr_->OutputCount(); }

  DoubleRegister OutputFloat32Register() { return OutputDoubleRegister().S(); }

  DoubleRegister OutputFloat64Register() { return OutputDoubleRegister(); }

  DoubleRegister OutputSimd128Register() { return OutputDoubleRegister().Q(); }

  Register InputRegister32(size_t index) {
    return ToRegister(instr_->InputAt(index)).W();
  }

  Register InputOrZeroRegister32(size_t index) {
    DCHECK(instr_->InputAt(index)->IsRegister() ||
           (instr_->InputAt(index)->IsImmediate() && (InputInt32(index) == 0)));
    if (instr_->InputAt(index)->IsImmediate()) {
      return wzr;
    }
    return InputRegister32(index);
  }

  Register InputRegister64(size_t index) { return InputRegister(index); }

  Register InputOrZeroRegister64(size_t index) {
    DCHECK(instr_->InputAt(index)->IsRegister() ||
           (instr_->InputAt(index)->IsImmediate() && (InputInt64(index) == 0)));
    if (instr_->InputAt(index)->IsImmediate()) {
      return xzr;
    }
    return InputRegister64(index);
  }

  Operand InputOperand(size_t index) {
    return ToOperand(instr_->InputAt(index));
  }

  Operand InputOperand64(size_t index) { return InputOperand(index); }

  Operand InputOperand32(size_t index) {
    return ToOperand32(instr_->InputAt(index));
  }

  Register OutputRegister64() { return OutputRegister(); }

  Register OutputRegister32() { return ToRegister(instr_->Output()).W(); }

  Register TempRegister32(size_t index) {
    return ToRegister(instr_->TempAt(index)).W();
  }

  Operand InputOperand2_32(size_t index) {
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
      case kMode_Operand2_R_UXTB:
        return Operand(InputRegister32(index), UXTB);
      case kMode_Operand2_R_UXTH:
        return Operand(InputRegister32(index), UXTH);
      case kMode_Operand2_R_SXTB:
        return Operand(InputRegister32(index), SXTB);
      case kMode_Operand2_R_SXTH:
        return Operand(InputRegister32(index), SXTH);
      case kMode_Operand2_R_SXTW:
        return Operand(InputRegister32(index), SXTW);
      case kMode_MRI:
      case kMode_MRR:
      case kMode_Root:
        break;
    }
    UNREACHABLE();
  }

  Operand InputOperand2_64(size_t index) {
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
      case kMode_Operand2_R_UXTB:
        return Operand(InputRegister64(index), UXTB);
      case kMode_Operand2_R_UXTH:
        return Operand(InputRegister64(index), UXTH);
      case kMode_Operand2_R_SXTB:
        return Operand(InputRegister64(index), SXTB);
      case kMode_Operand2_R_SXTH:
        return Operand(InputRegister64(index), SXTH);
      case kMode_Operand2_R_SXTW:
        return Operand(InputRegister64(index), SXTW);
      case kMode_MRI:
      case kMode_MRR:
      case kMode_Root:
        break;
    }
    UNREACHABLE();
  }

  MemOperand MemoryOperand(size_t index = 0) {
    switch (AddressingModeField::decode(instr_->opcode())) {
      case kMode_None:
      case kMode_Operand2_R_LSR_I:
      case kMode_Operand2_R_ASR_I:
      case kMode_Operand2_R_ROR_I:
      case kMode_Operand2_R_UXTB:
      case kMode_Operand2_R_UXTH:
      case kMode_Operand2_R_SXTB:
      case kMode_Operand2_R_SXTH:
      case kMode_Operand2_R_SXTW:
        break;
      case kMode_Root:
        return MemOperand(kRootRegister, InputInt64(index));
      case kMode_Operand2_R_LSL_I:
        return MemOperand(InputRegister(index + 0), InputRegister(index + 1),
                          LSL, InputInt32(index + 2));
      case kMode_MRI:
        return MemOperand(InputRegister(index + 0), InputInt32(index + 1));
      case kMode_MRR:
        return MemOperand(InputRegister(index + 0), InputRegister(index + 1));
    }
    UNREACHABLE();
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
        if (RelocInfo::IsWasmReference(constant.rmode())) {
          return Operand(constant.ToInt64(), constant.rmode());
        } else {
          return Operand(constant.ToInt64());
        }
      case Constant::kFloat32:
        return Operand(Operand::EmbeddedNumber(constant.ToFloat32()));
      case Constant::kFloat64:
        return Operand(Operand::EmbeddedNumber(constant.ToFloat64().value()));
      case Constant::kExternalReference:
        return Operand(constant.ToExternalReference());
      case Constant::kHeapObject:
        return Operand(constant.ToHeapObject());
      case Constant::kDelayedStringConstant:
        return Operand::EmbeddedStringConstant(
            constant.ToDelayedStringConstant());
      case Constant::kRpoNumber:
        UNREACHABLE();  // TODO(dcarney): RPO immediates on arm64.
        break;
    }
    UNREACHABLE();
  }

  MemOperand ToMemOperand(InstructionOperand* op, TurboAssembler* tasm) const {
    DCHECK_NOT_NULL(op);
    DCHECK(op->IsStackSlot() || op->IsFPStackSlot());
    return SlotToMemOperand(AllocatedOperand::cast(op)->index(), tasm);
  }

  MemOperand SlotToMemOperand(int slot, TurboAssembler* tasm) const {
    FrameOffset offset = frame_access_state()->GetFrameOffset(slot);
    if (offset.from_frame_pointer()) {
      int from_sp = offset.offset() + frame_access_state()->GetSPToFPOffset();
      // Convert FP-offsets to SP-offsets if it results in better code.
      if (Assembler::IsImmLSUnscaled(from_sp) ||
          Assembler::IsImmLSScaled(from_sp, 3)) {
        offset = FrameOffset::FromStackPointer(from_sp);
      }
    }
    return MemOperand(offset.from_stack_pointer() ? sp : fp, offset.offset());
  }
};

namespace {

class OutOfLineRecordWrite final : public OutOfLineCode {
 public:
  OutOfLineRecordWrite(CodeGenerator* gen, Register object, Operand index,
                       Register value, Register scratch0, Register scratch1,
                       RecordWriteMode mode, StubCallMode stub_mode,
                       UnwindingInfoWriter* unwinding_info_writer)
      : OutOfLineCode(gen),
        object_(object),
        index_(index),
        value_(value),
        scratch0_(scratch0),
        scratch1_(scratch1),
        mode_(mode),
        stub_mode_(stub_mode),
        must_save_lr_(!gen->frame_access_state()->has_frame()),
        unwinding_info_writer_(unwinding_info_writer),
        zone_(gen->zone()) {}

  void Generate() final {
    if (mode_ > RecordWriteMode::kValueIsPointer) {
      __ JumpIfSmi(value_, exit());
    }
    __ CheckPageFlagClear(value_, scratch0_,
                          MemoryChunk::kPointersToHereAreInterestingMask,
                          exit());
    __ Add(scratch1_, object_, index_);
    RememberedSetAction const remembered_set_action =
        mode_ > RecordWriteMode::kValueIsMap ? EMIT_REMEMBERED_SET
                                             : OMIT_REMEMBERED_SET;
    SaveFPRegsMode const save_fp_mode =
        frame()->DidAllocateDoubleRegisters() ? kSaveFPRegs : kDontSaveFPRegs;
    if (must_save_lr_) {
      // We need to save and restore lr if the frame was elided.
      __ Push(lr, padreg);
      unwinding_info_writer_->MarkLinkRegisterOnTopOfStack(__ pc_offset(), sp);
    }
    if (mode_ == RecordWriteMode::kValueIsEphemeronKey) {
      __ CallEphemeronKeyBarrier(object_, scratch1_, save_fp_mode);
    } else if (stub_mode_ == StubCallMode::kCallWasmRuntimeStub) {
      // A direct call to a wasm runtime stub defined in this module.
      // Just encode the stub index. This will be patched when the code
      // is added to the native module and copied into wasm code space.
      __ CallRecordWriteStub(object_, scratch1_, remembered_set_action,
                             save_fp_mode, wasm::WasmCode::kWasmRecordWrite);
    } else {
      __ CallRecordWriteStub(object_, scratch1_, remembered_set_action,
                             save_fp_mode);
    }
    if (must_save_lr_) {
      __ Pop(padreg, lr);
      unwinding_info_writer_->MarkPopLinkRegisterFromTopOfStack(__ pc_offset());
    }
  }

 private:
  Register const object_;
  Operand const index_;
  Register const value_;
  Register const scratch0_;
  Register const scratch1_;
  RecordWriteMode const mode_;
  StubCallMode const stub_mode_;
  bool must_save_lr_;
  UnwindingInfoWriter* const unwinding_info_writer_;
  Zone* zone_;
};

Condition FlagsConditionToCondition(FlagsCondition condition) {
  switch (condition) {
    case kEqual:
      return eq;
    case kNotEqual:
      return ne;
    case kSignedLessThan:
      return lt;
    case kSignedGreaterThanOrEqual:
      return ge;
    case kSignedLessThanOrEqual:
      return le;
    case kSignedGreaterThan:
      return gt;
    case kUnsignedLessThan:
      return lo;
    case kUnsignedGreaterThanOrEqual:
      return hs;
    case kUnsignedLessThanOrEqual:
      return ls;
    case kUnsignedGreaterThan:
      return hi;
    case kFloatLessThanOrUnordered:
      return lt;
    case kFloatGreaterThanOrEqual:
      return ge;
    case kFloatLessThanOrEqual:
      return ls;
    case kFloatGreaterThanOrUnordered:
      return hi;
    case kFloatLessThan:
      return lo;
    case kFloatGreaterThanOrEqualOrUnordered:
      return hs;
    case kFloatLessThanOrEqualOrUnordered:
      return le;
    case kFloatGreaterThan:
      return gt;
    case kOverflow:
      return vs;
    case kNotOverflow:
      return vc;
    case kUnorderedEqual:
    case kUnorderedNotEqual:
      break;
    case kPositiveOrZero:
      return pl;
    case kNegative:
      return mi;
  }
  UNREACHABLE();
}

void EmitWordLoadPoisoningIfNeeded(CodeGenerator* codegen,
                                   InstructionCode opcode, Instruction* instr,
                                   Arm64OperandConverter& i) {
  const MemoryAccessMode access_mode =
      static_cast<MemoryAccessMode>(MiscField::decode(opcode));
  if (access_mode == kMemoryAccessPoisoned) {
    Register value = i.OutputRegister();
    Register poison = value.Is64Bits() ? kSpeculationPoisonRegister
                                       : kSpeculationPoisonRegister.W();
    codegen->tasm()->And(value, value, Operand(poison));
  }
}

}  // namespace

#define ASSEMBLE_SHIFT(asm_instr, width)                                    \
  do {                                                                      \
    if (instr->InputAt(1)->IsRegister()) {                                  \
      __ asm_instr(i.OutputRegister##width(), i.InputRegister##width(0),    \
                   i.InputRegister##width(1));                              \
    } else {                                                                \
      uint32_t imm =                                                        \
          static_cast<uint32_t>(i.InputOperand##width(1).ImmediateValue()); \
      __ asm_instr(i.OutputRegister##width(), i.InputRegister##width(0),    \
                   imm % (width));                                          \
    }                                                                       \
  } while (0)

#define ASSEMBLE_ATOMIC_LOAD_INTEGER(asm_instr, reg)                   \
  do {                                                                 \
    __ Add(i.TempRegister(0), i.InputRegister(0), i.InputRegister(1)); \
    __ asm_instr(i.Output##reg(), i.TempRegister(0));                  \
  } while (0)

#define ASSEMBLE_ATOMIC_STORE_INTEGER(asm_instr, reg)                  \
  do {                                                                 \
    __ Add(i.TempRegister(0), i.InputRegister(0), i.InputRegister(1)); \
    __ asm_instr(i.Input##reg(2), i.TempRegister(0));                  \
  } while (0)

#define ASSEMBLE_ATOMIC_EXCHANGE_INTEGER(load_instr, store_instr, reg)       \
  do {                                                                       \
    Label exchange;                                                          \
    __ Add(i.TempRegister(0), i.InputRegister(0), i.InputRegister(1));       \
    __ Bind(&exchange);                                                      \
    __ load_instr(i.Output##reg(), i.TempRegister(0));                       \
    __ store_instr(i.TempRegister32(1), i.Input##reg(2), i.TempRegister(0)); \
    __ Cbnz(i.TempRegister32(1), &exchange);                                 \
  } while (0)

#define ASSEMBLE_ATOMIC_COMPARE_EXCHANGE_INTEGER(load_instr, store_instr, ext, \
                                                 reg)                          \
  do {                                                                         \
    Label compareExchange;                                                     \
    Label exit;                                                                \
    __ Add(i.TempRegister(0), i.InputRegister(0), i.InputRegister(1));         \
    __ Bind(&compareExchange);                                                 \
    __ load_instr(i.Output##reg(), i.TempRegister(0));                         \
    __ Cmp(i.Output##reg(), Operand(i.Input##reg(2), ext));                    \
    __ B(ne, &exit);                                                           \
    __ store_instr(i.TempRegister32(1), i.Input##reg(3), i.TempRegister(0));   \
    __ Cbnz(i.TempRegister32(1), &compareExchange);                            \
    __ Bind(&exit);                                                            \
  } while (0)

#define ASSEMBLE_ATOMIC_BINOP(load_instr, store_instr, bin_instr, reg)       \
  do {                                                                       \
    Label binop;                                                             \
    __ Add(i.TempRegister(0), i.InputRegister(0), i.InputRegister(1));       \
    __ Bind(&binop);                                                         \
    __ load_instr(i.Output##reg(), i.TempRegister(0));                       \
    __ bin_instr(i.Temp##reg(1), i.Output##reg(), Operand(i.Input##reg(2))); \
    __ store_instr(i.TempRegister32(2), i.Temp##reg(1), i.TempRegister(0));  \
    __ Cbnz(i.TempRegister32(2), &binop);                                    \
  } while (0)

#define ASSEMBLE_IEEE754_BINOP(name)                                        \
  do {                                                                      \
    FrameScope scope(tasm(), StackFrame::MANUAL);                           \
    __ CallCFunction(ExternalReference::ieee754_##name##_function(), 0, 2); \
  } while (0)

#define ASSEMBLE_IEEE754_UNOP(name)                                         \
  do {                                                                      \
    FrameScope scope(tasm(), StackFrame::MANUAL);                           \
    __ CallCFunction(ExternalReference::ieee754_##name##_function(), 0, 1); \
  } while (0)

void CodeGenerator::AssembleDeconstructFrame() {
  __ Mov(sp, fp);
  __ Pop(fp, lr);

  unwinding_info_writer_.MarkFrameDeconstructed(__ pc_offset());
}

void CodeGenerator::AssemblePrepareTailCall() {
  if (frame_access_state()->has_frame()) {
    __ Ldr(lr, MemOperand(fp, StandardFrameConstants::kCallerPCOffset));
    __ Ldr(fp, MemOperand(fp, StandardFrameConstants::kCallerFPOffset));
  }
  frame_access_state()->SetFrameAccessToSP();
}

void CodeGenerator::AssemblePopArgumentsAdaptorFrame(Register args_reg,
                                                     Register scratch1,
                                                     Register scratch2,
                                                     Register scratch3) {
  DCHECK(!AreAliased(args_reg, scratch1, scratch2, scratch3));
  Label done;

  // Check if current frame is an arguments adaptor frame.
  __ Ldr(scratch1, MemOperand(fp, StandardFrameConstants::kContextOffset));
  __ Cmp(scratch1,
         Operand(StackFrame::TypeToMarker(StackFrame::ARGUMENTS_ADAPTOR)));
  __ B(ne, &done);

  // Load arguments count from current arguments adaptor frame (note, it
  // does not include receiver).
  Register caller_args_count_reg = scratch1;
  __ Ldr(caller_args_count_reg,
         MemOperand(fp, ArgumentsAdaptorFrameConstants::kLengthOffset));
  __ SmiUntag(caller_args_count_reg);

  ParameterCount callee_args_count(args_reg);
  __ PrepareForTailCall(callee_args_count, caller_args_count_reg, scratch2,
                        scratch3);
  __ bind(&done);
}

namespace {

void AdjustStackPointerForTailCall(TurboAssembler* tasm,
                                   FrameAccessState* state,
                                   int new_slot_above_sp,
                                   bool allow_shrinkage = true) {
  int current_sp_offset = state->GetSPToFPSlotCount() +
                          StandardFrameConstants::kFixedSlotCountAboveFp;
  int stack_slot_delta = new_slot_above_sp - current_sp_offset;
  DCHECK_EQ(stack_slot_delta % 2, 0);
  if (stack_slot_delta > 0) {
    tasm->Claim(stack_slot_delta);
    state->IncreaseSPDelta(stack_slot_delta);
  } else if (allow_shrinkage && stack_slot_delta < 0) {
    tasm->Drop(-stack_slot_delta);
    state->IncreaseSPDelta(stack_slot_delta);
  }
}

}  // namespace

void CodeGenerator::AssembleTailCallBeforeGap(Instruction* instr,
                                              int first_unused_stack_slot) {
  AdjustStackPointerForTailCall(tasm(), frame_access_state(),
                                first_unused_stack_slot, false);
}

void CodeGenerator::AssembleTailCallAfterGap(Instruction* instr,
                                             int first_unused_stack_slot) {
  DCHECK_EQ(first_unused_stack_slot % 2, 0);
  AdjustStackPointerForTailCall(tasm(), frame_access_state(),
                                first_unused_stack_slot);
  DCHECK(instr->IsTailCall());
  InstructionOperandConverter g(this, instr);
  int optional_padding_slot = g.InputInt32(instr->InputCount() - 2);
  if (optional_padding_slot % 2) {
    __ Poke(padreg, optional_padding_slot * kSystemPointerSize);
  }
}

// Check that {kJavaScriptCallCodeStartRegister} is correct.
void CodeGenerator::AssembleCodeStartRegisterCheck() {
  UseScratchRegisterScope temps(tasm());
  Register scratch = temps.AcquireX();
  __ ComputeCodeStartAddress(scratch);
  __ cmp(scratch, kJavaScriptCallCodeStartRegister);
  __ Assert(eq, AbortReason::kWrongFunctionCodeStart);
}

// Check if the code object is marked for deoptimization. If it is, then it
// jumps to the CompileLazyDeoptimizedCode builtin. In order to do this we need
// to:
//    1. read from memory the word that contains that bit, which can be found in
//       the flags in the referenced {CodeDataContainer} object;
//    2. test kMarkedForDeoptimizationBit in those flags; and
//    3. if it is not zero then it jumps to the builtin.
void CodeGenerator::BailoutIfDeoptimized() {
  UseScratchRegisterScope temps(tasm());
  Register scratch = temps.AcquireX();
  int offset = Code::kCodeDataContainerOffset - Code::kHeaderSize;
  __ LoadTaggedPointerField(
      scratch, MemOperand(kJavaScriptCallCodeStartRegister, offset));
  __ Ldr(scratch.W(),
         FieldMemOperand(scratch, CodeDataContainer::kKindSpecificFlagsOffset));
  Label not_deoptimized;
  __ Tbz(scratch.W(), Code::kMarkedForDeoptimizationBit, &not_deoptimized);
  __ Jump(BUILTIN_CODE(isolate(), CompileLazyDeoptimizedCode),
          RelocInfo::CODE_TARGET);
  __ Bind(&not_deoptimized);
}

void CodeGenerator::GenerateSpeculationPoisonFromCodeStartRegister() {
  UseScratchRegisterScope temps(tasm());
  Register scratch = temps.AcquireX();

  // Set a mask which has all bits set in the normal case, but has all
  // bits cleared if we are speculatively executing the wrong PC.
  __ ComputeCodeStartAddress(scratch);
  __ Cmp(kJavaScriptCallCodeStartRegister, scratch);
  __ Csetm(kSpeculationPoisonRegister, eq);
  __ Csdb();
}

void CodeGenerator::AssembleRegisterArgumentPoisoning() {
  UseScratchRegisterScope temps(tasm());
  Register scratch = temps.AcquireX();

  __ Mov(scratch, sp);
  __ And(kJSFunctionRegister, kJSFunctionRegister, kSpeculationPoisonRegister);
  __ And(kContextRegister, kContextRegister, kSpeculationPoisonRegister);
  __ And(scratch, scratch, kSpeculationPoisonRegister);
  __ Mov(sp, scratch);
}

// Assembles an instruction after register allocation, producing machine code.
CodeGenerator::CodeGenResult CodeGenerator::AssembleArchInstruction(
    Instruction* instr) {
  Arm64OperandConverter i(this, instr);
  InstructionCode opcode = instr->opcode();
  ArchOpcode arch_opcode = ArchOpcodeField::decode(opcode);
  switch (arch_opcode) {
    case kArchCallCodeObject: {
      if (instr->InputAt(0)->IsImmediate()) {
        __ Call(i.InputCode(0), RelocInfo::CODE_TARGET);
      } else {
        Register reg = i.InputRegister(0);
        DCHECK_IMPLIES(
            HasCallDescriptorFlag(instr, CallDescriptor::kFixedTargetRegister),
            reg == kJavaScriptCallCodeStartRegister);
        __ CallCodeObject(reg);
      }
      RecordCallPosition(instr);
      frame_access_state()->ClearSPDelta();
      break;
    }
    case kArchCallBuiltinPointer: {
      DCHECK(!instr->InputAt(0)->IsImmediate());
      Register builtin_pointer = i.InputRegister(0);
      __ CallBuiltinPointer(builtin_pointer);
      RecordCallPosition(instr);
      frame_access_state()->ClearSPDelta();
      break;
    }
    case kArchCallWasmFunction: {
      if (instr->InputAt(0)->IsImmediate()) {
        Constant constant = i.ToConstant(instr->InputAt(0));
        Address wasm_code = static_cast<Address>(constant.ToInt64());
        __ Call(wasm_code, constant.rmode());
      } else {
        Register target = i.InputRegister(0);
        __ Call(target);
      }
      RecordCallPosition(instr);
      frame_access_state()->ClearSPDelta();
      break;
    }
    case kArchTailCallCodeObjectFromJSFunction:
    case kArchTailCallCodeObject: {
      if (arch_opcode == kArchTailCallCodeObjectFromJSFunction) {
        AssemblePopArgumentsAdaptorFrame(kJavaScriptCallArgCountRegister,
                                         i.TempRegister(0), i.TempRegister(1),
                                         i.TempRegister(2));
      }
      if (instr->InputAt(0)->IsImmediate()) {
        __ Jump(i.InputCode(0), RelocInfo::CODE_TARGET);
      } else {
        Register reg = i.InputRegister(0);
        DCHECK_IMPLIES(
            HasCallDescriptorFlag(instr, CallDescriptor::kFixedTargetRegister),
            reg == kJavaScriptCallCodeStartRegister);
        __ JumpCodeObject(reg);
      }
      unwinding_info_writer_.MarkBlockWillExit();
      frame_access_state()->ClearSPDelta();
      frame_access_state()->SetFrameAccessToDefault();
      break;
    }
    case kArchTailCallWasm: {
      if (instr->InputAt(0)->IsImmediate()) {
        Constant constant = i.ToConstant(instr->InputAt(0));
        Address wasm_code = static_cast<Address>(constant.ToInt64());
        __ Jump(wasm_code, constant.rmode());
      } else {
        Register target = i.InputRegister(0);
        __ Jump(target);
      }
      unwinding_info_writer_.MarkBlockWillExit();
      frame_access_state()->ClearSPDelta();
      frame_access_state()->SetFrameAccessToDefault();
      break;
    }
    case kArchTailCallAddress: {
      CHECK(!instr->InputAt(0)->IsImmediate());
      Register reg = i.InputRegister(0);
      DCHECK_IMPLIES(
          HasCallDescriptorFlag(instr, CallDescriptor::kFixedTargetRegister),
          reg == kJavaScriptCallCodeStartRegister);
      __ Jump(reg);
      unwinding_info_writer_.MarkBlockWillExit();
      frame_access_state()->ClearSPDelta();
      frame_access_state()->SetFrameAccessToDefault();
      break;
    }
    case kArchCallJSFunction: {
      Register func = i.InputRegister(0);
      if (FLAG_debug_code) {
        // Check the function's context matches the context argument.
        UseScratchRegisterScope scope(tasm());
        Register temp = scope.AcquireX();
        __ LoadTaggedPointerField(
            temp, FieldMemOperand(func, JSFunction::kContextOffset));
        __ cmp(cp, temp);
        __ Assert(eq, AbortReason::kWrongFunctionContext);
      }
      static_assert(kJavaScriptCallCodeStartRegister == x2, "ABI mismatch");
      __ LoadTaggedPointerField(x2,
                                FieldMemOperand(func, JSFunction::kCodeOffset));
      __ CallCodeObject(x2);
      RecordCallPosition(instr);
      frame_access_state()->ClearSPDelta();
      break;
    }
    case kArchPrepareCallCFunction:
      // We don't need kArchPrepareCallCFunction on arm64 as the instruction
      // selector has already performed a Claim to reserve space on the stack.
      // Frame alignment is always 16 bytes, and the stack pointer is already
      // 16-byte aligned, therefore we do not need to align the stack pointer
      // by an unknown value, and it is safe to continue accessing the frame
      // via the stack pointer.
      UNREACHABLE();
      break;
    case kArchSaveCallerRegisters: {
      fp_mode_ =
          static_cast<SaveFPRegsMode>(MiscField::decode(instr->opcode()));
      DCHECK(fp_mode_ == kDontSaveFPRegs || fp_mode_ == kSaveFPRegs);
      // kReturnRegister0 should have been saved before entering the stub.
      int bytes = __ PushCallerSaved(fp_mode_, kReturnRegister0);
      DCHECK(IsAligned(bytes, kSystemPointerSize));
      DCHECK_EQ(0, frame_access_state()->sp_delta());
      frame_access_state()->IncreaseSPDelta(bytes / kSystemPointerSize);
      DCHECK(!caller_registers_saved_);
      caller_registers_saved_ = true;
      break;
    }
    case kArchRestoreCallerRegisters: {
      DCHECK(fp_mode_ ==
             static_cast<SaveFPRegsMode>(MiscField::decode(instr->opcode())));
      DCHECK(fp_mode_ == kDontSaveFPRegs || fp_mode_ == kSaveFPRegs);
      // Don't overwrite the returned value.
      int bytes = __ PopCallerSaved(fp_mode_, kReturnRegister0);
      frame_access_state()->IncreaseSPDelta(-(bytes / kSystemPointerSize));
      DCHECK_EQ(0, frame_access_state()->sp_delta());
      DCHECK(caller_registers_saved_);
      caller_registers_saved_ = false;
      break;
    }
    case kArchPrepareTailCall:
      AssemblePrepareTailCall();
      break;
    case kArchCallCFunction: {
      int const num_parameters = MiscField::decode(instr->opcode());
      if (instr->InputAt(0)->IsImmediate()) {
        ExternalReference ref = i.InputExternalReference(0);
        __ CallCFunction(ref, num_parameters, 0);
      } else {
        Register func = i.InputRegister(0);
        __ CallCFunction(func, num_parameters, 0);
      }
      frame_access_state()->SetFrameAccessToDefault();
      // Ideally, we should decrement SP delta to match the change of stack
      // pointer in CallCFunction. However, for certain architectures (e.g.
      // ARM), there may be more strict alignment requirement, causing old SP
      // to be saved on the stack. In those cases, we can not calculate the SP
      // delta statically.
      frame_access_state()->ClearSPDelta();
      if (caller_registers_saved_) {
        // Need to re-sync SP delta introduced in kArchSaveCallerRegisters.
        // Here, we assume the sequence to be:
        //   kArchSaveCallerRegisters;
        //   kArchCallCFunction;
        //   kArchRestoreCallerRegisters;
        int bytes =
            __ RequiredStackSizeForCallerSaved(fp_mode_, kReturnRegister0);
        frame_access_state()->IncreaseSPDelta(bytes / kSystemPointerSize);
      }
      break;
    }
    case kArchJmp:
      AssembleArchJump(i.InputRpo(0));
      break;
    case kArchTableSwitch:
      AssembleArchTableSwitch(instr);
      break;
    case kArchBinarySearchSwitch:
      AssembleArchBinarySearchSwitch(instr);
      break;
    case kArchLookupSwitch:
      AssembleArchLookupSwitch(instr);
      break;
    case kArchDebugAbort:
      DCHECK(i.InputRegister(0).is(x1));
      if (!frame_access_state()->has_frame()) {
        // We don't actually want to generate a pile of code for this, so just
        // claim there is a stack frame, without generating one.
        FrameScope scope(tasm(), StackFrame::NONE);
        __ Call(isolate()->builtins()->builtin_handle(Builtins::kAbortJS),
                RelocInfo::CODE_TARGET);
      } else {
        __ Call(isolate()->builtins()->builtin_handle(Builtins::kAbortJS),
                RelocInfo::CODE_TARGET);
      }
      __ Debug("kArchDebugAbort", 0, BREAK);
      unwinding_info_writer_.MarkBlockWillExit();
      break;
    case kArchDebugBreak:
      __ Debug("kArchDebugBreak", 0, BREAK);
      break;
    case kArchComment:
      __ RecordComment(reinterpret_cast<const char*>(i.InputInt64(0)));
      break;
    case kArchThrowTerminator:
      unwinding_info_writer_.MarkBlockWillExit();
      break;
    case kArchNop:
      // don't emit code for nops.
      break;
    case kArchDeoptimize: {
      int deopt_state_id =
          BuildTranslation(instr, -1, 0, OutputFrameStateCombine::Ignore());
      CodeGenResult result =
          AssembleDeoptimizerCall(deopt_state_id, current_source_position_);
      if (result != kSuccess) return result;
      unwinding_info_writer_.MarkBlockWillExit();
      break;
    }
    case kArchRet:
      AssembleReturn(instr->InputAt(0));
      break;
    case kArchStackPointer:
      __ mov(i.OutputRegister(), sp);
      break;
    case kArchFramePointer:
      __ mov(i.OutputRegister(), fp);
      break;
    case kArchParentFramePointer:
      if (frame_access_state()->has_frame()) {
        __ ldr(i.OutputRegister(), MemOperand(fp, 0));
      } else {
        __ mov(i.OutputRegister(), fp);
      }
      break;
    case kArchTruncateDoubleToI:
      __ TruncateDoubleToI(isolate(), zone(), i.OutputRegister(),
                           i.InputDoubleRegister(0), DetermineStubCallMode());
      break;
    case kArchStoreWithWriteBarrier: {
      RecordWriteMode mode =
          static_cast<RecordWriteMode>(MiscField::decode(instr->opcode()));
      AddressingMode addressing_mode =
          AddressingModeField::decode(instr->opcode());
      Register object = i.InputRegister(0);
      Operand index(0);
      if (addressing_mode == kMode_MRI) {
        index = Operand(i.InputInt64(1));
      } else {
        DCHECK_EQ(addressing_mode, kMode_MRR);
        index = Operand(i.InputRegister(1));
      }
      Register value = i.InputRegister(2);
      Register scratch0 = i.TempRegister(0);
      Register scratch1 = i.TempRegister(1);
      auto ool = new (zone()) OutOfLineRecordWrite(
          this, object, index, value, scratch0, scratch1, mode,
          DetermineStubCallMode(), &unwinding_info_writer_);
      __ StoreTaggedField(value, MemOperand(object, index));
      __ CheckPageFlagSet(object, scratch0,
                          MemoryChunk::kPointersFromHereAreInterestingMask,
                          ool->entry());
      __ Bind(ool->exit());
      break;
    }
    case kArchStackSlot: {
      FrameOffset offset =
          frame_access_state()->GetFrameOffset(i.InputInt32(0));
      Register base = offset.from_stack_pointer() ? sp : fp;
      __ Add(i.OutputRegister(0), base, Operand(offset.offset()));
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
    case kIeee754Float64Cos:
      ASSEMBLE_IEEE754_UNOP(cos);
      break;
    case kIeee754Float64Cosh:
      ASSEMBLE_IEEE754_UNOP(cosh);
      break;
    case kIeee754Float64Cbrt:
      ASSEMBLE_IEEE754_UNOP(cbrt);
      break;
    case kIeee754Float64Exp:
      ASSEMBLE_IEEE754_UNOP(exp);
      break;
    case kIeee754Float64Expm1:
      ASSEMBLE_IEEE754_UNOP(expm1);
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
    case kIeee754Float64Pow:
      ASSEMBLE_IEEE754_BINOP(pow);
      break;
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
    case kArm64Float32RoundDown:
      __ Frintm(i.OutputFloat32Register(), i.InputFloat32Register(0));
      break;
    case kArm64Float64RoundDown:
      __ Frintm(i.OutputDoubleRegister(), i.InputDoubleRegister(0));
      break;
    case kArm64Float32RoundUp:
      __ Frintp(i.OutputFloat32Register(), i.InputFloat32Register(0));
      break;
    case kArm64Float64RoundUp:
      __ Frintp(i.OutputDoubleRegister(), i.InputDoubleRegister(0));
      break;
    case kArm64Float64RoundTiesAway:
      __ Frinta(i.OutputDoubleRegister(), i.InputDoubleRegister(0));
      break;
    case kArm64Float32RoundTruncate:
      __ Frintz(i.OutputFloat32Register(), i.InputFloat32Register(0));
      break;
    case kArm64Float64RoundTruncate:
      __ Frintz(i.OutputDoubleRegister(), i.InputDoubleRegister(0));
      break;
    case kArm64Float32RoundTiesEven:
      __ Frintn(i.OutputFloat32Register(), i.InputFloat32Register(0));
      break;
    case kArm64Float64RoundTiesEven:
      __ Frintn(i.OutputDoubleRegister(), i.InputDoubleRegister(0));
      break;
    case kArm64Add:
      if (FlagsModeField::decode(opcode) != kFlags_none) {
        __ Adds(i.OutputRegister(), i.InputOrZeroRegister64(0),
                i.InputOperand2_64(1));
      } else {
        __ Add(i.OutputRegister(), i.InputOrZeroRegister64(0),
               i.InputOperand2_64(1));
      }
      break;
    case kArm64Add32:
      if (FlagsModeField::decode(opcode) != kFlags_none) {
        __ Adds(i.OutputRegister32(), i.InputOrZeroRegister32(0),
                i.InputOperand2_32(1));
      } else {
        __ Add(i.OutputRegister32(), i.InputOrZeroRegister32(0),
               i.InputOperand2_32(1));
      }
      break;
    case kArm64And:
      if (FlagsModeField::decode(opcode) != kFlags_none) {
        // The ands instruction only sets N and Z, so only the following
        // conditions make sense.
        DCHECK(FlagsConditionField::decode(opcode) == kEqual ||
               FlagsConditionField::decode(opcode) == kNotEqual ||
               FlagsConditionField::decode(opcode) == kPositiveOrZero ||
               FlagsConditionField::decode(opcode) == kNegative);
        __ Ands(i.OutputRegister(), i.InputOrZeroRegister64(0),
                i.InputOperand2_64(1));
      } else {
        __ And(i.OutputRegister(), i.InputOrZeroRegister64(0),
               i.InputOperand2_64(1));
      }
      break;
    case kArm64And32:
      if (FlagsModeField::decode(opcode) != kFlags_none) {
        // The ands instruction only sets N and Z, so only the following
        // conditions make sense.
        DCHECK(FlagsConditionField::decode(opcode) == kEqual ||
               FlagsConditionField::decode(opcode) == kNotEqual ||
               FlagsConditionField::decode(opcode) == kPositiveOrZero ||
               FlagsConditionField::decode(opcode) == kNegative);
        __ Ands(i.OutputRegister32(), i.InputOrZeroRegister32(0),
                i.InputOperand2_32(1));
      } else {
        __ And(i.OutputRegister32(), i.InputOrZeroRegister32(0),
               i.InputOperand2_32(1));
      }
      break;
    case kArm64Bic:
      __ Bic(i.OutputRegister(), i.InputOrZeroRegister64(0),
             i.InputOperand2_64(1));
      break;
    case kArm64Bic32:
      __ Bic(i.OutputRegister32(), i.InputOrZeroRegister32(0),
             i.InputOperand2_32(1));
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
      UseScratchRegisterScope scope(tasm());
      Register temp = scope.AcquireX();
      __ Sdiv(temp, i.InputRegister(0), i.InputRegister(1));
      __ Msub(i.OutputRegister(), temp, i.InputRegister(1), i.InputRegister(0));
      break;
    }
    case kArm64Imod32: {
      UseScratchRegisterScope scope(tasm());
      Register temp = scope.AcquireW();
      __ Sdiv(temp, i.InputRegister32(0), i.InputRegister32(1));
      __ Msub(i.OutputRegister32(), temp, i.InputRegister32(1),
              i.InputRegister32(0));
      break;
    }
    case kArm64Umod: {
      UseScratchRegisterScope scope(tasm());
      Register temp = scope.AcquireX();
      __ Udiv(temp, i.InputRegister(0), i.InputRegister(1));
      __ Msub(i.OutputRegister(), temp, i.InputRegister(1), i.InputRegister(0));
      break;
    }
    case kArm64Umod32: {
      UseScratchRegisterScope scope(tasm());
      Register temp = scope.AcquireW();
      __ Udiv(temp, i.InputRegister32(0), i.InputRegister32(1));
      __ Msub(i.OutputRegister32(), temp, i.InputRegister32(1),
              i.InputRegister32(0));
      break;
    }
    case kArm64Not:
      __ Mvn(i.OutputRegister(), i.InputOperand(0));
      break;
    case kArm64Not32:
      __ Mvn(i.OutputRegister32(), i.InputOperand32(0));
      break;
    case kArm64Or:
      __ Orr(i.OutputRegister(), i.InputOrZeroRegister64(0),
             i.InputOperand2_64(1));
      break;
    case kArm64Or32:
      __ Orr(i.OutputRegister32(), i.InputOrZeroRegister32(0),
             i.InputOperand2_32(1));
      break;
    case kArm64Orn:
      __ Orn(i.OutputRegister(), i.InputOrZeroRegister64(0),
             i.InputOperand2_64(1));
      break;
    case kArm64Orn32:
      __ Orn(i.OutputRegister32(), i.InputOrZeroRegister32(0),
             i.InputOperand2_32(1));
      break;
    case kArm64Eor:
      __ Eor(i.OutputRegister(), i.InputOrZeroRegister64(0),
             i.InputOperand2_64(1));
      break;
    case kArm64Eor32:
      __ Eor(i.OutputRegister32(), i.InputOrZeroRegister32(0),
             i.InputOperand2_32(1));
      break;
    case kArm64Eon:
      __ Eon(i.OutputRegister(), i.InputOrZeroRegister64(0),
             i.InputOperand2_64(1));
      break;
    case kArm64Eon32:
      __ Eon(i.OutputRegister32(), i.InputOrZeroRegister32(0),
             i.InputOperand2_32(1));
      break;
    case kArm64Sub:
      if (FlagsModeField::decode(opcode) != kFlags_none) {
        __ Subs(i.OutputRegister(), i.InputOrZeroRegister64(0),
                i.InputOperand2_64(1));
      } else {
        __ Sub(i.OutputRegister(), i.InputOrZeroRegister64(0),
               i.InputOperand2_64(1));
      }
      break;
    case kArm64Sub32:
      if (FlagsModeField::decode(opcode) != kFlags_none) {
        __ Subs(i.OutputRegister32(), i.InputOrZeroRegister32(0),
                i.InputOperand2_32(1));
      } else {
        __ Sub(i.OutputRegister32(), i.InputOrZeroRegister32(0),
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
    case kArm64Sxtb32:
      __ Sxtb(i.OutputRegister32(), i.InputRegister32(0));
      break;
    case kArm64Sxth32:
      __ Sxth(i.OutputRegister32(), i.InputRegister32(0));
      break;
    case kArm64Sxtb:
      __ Sxtb(i.OutputRegister(), i.InputRegister32(0));
      break;
    case kArm64Sxth:
      __ Sxth(i.OutputRegister(), i.InputRegister32(0));
      break;
    case kArm64Sxtw:
      __ Sxtw(i.OutputRegister(), i.InputRegister32(0));
      break;
    case kArm64Sbfx32:
      __ Sbfx(i.OutputRegister32(), i.InputRegister32(0), i.InputInt5(1),
              i.InputInt5(2));
      break;
    case kArm64Ubfx:
      __ Ubfx(i.OutputRegister(), i.InputRegister(0), i.InputInt6(1),
              i.InputInt32(2));
      break;
    case kArm64Ubfx32:
      __ Ubfx(i.OutputRegister32(), i.InputRegister32(0), i.InputInt5(1),
              i.InputInt32(2));
      break;
    case kArm64Ubfiz32:
      __ Ubfiz(i.OutputRegister32(), i.InputRegister32(0), i.InputInt5(1),
               i.InputInt5(2));
      break;
    case kArm64Bfi:
      __ Bfi(i.OutputRegister(), i.InputRegister(1), i.InputInt6(2),
             i.InputInt6(3));
      break;
    case kArm64TestAndBranch32:
    case kArm64TestAndBranch:
      // Pseudo instructions turned into tbz/tbnz in AssembleArchBranch.
      break;
    case kArm64CompareAndBranch32:
    case kArm64CompareAndBranch:
      // Pseudo instruction handled in AssembleArchBranch.
      break;
    case kArm64Claim: {
      int count = i.InputInt32(0);
      DCHECK_EQ(count % 2, 0);
      __ AssertSpAligned();
      if (count > 0) {
        __ Claim(count);
        frame_access_state()->IncreaseSPDelta(count);
      }
      break;
    }
    case kArm64Poke: {
      Operand operand(i.InputInt32(1) * kSystemPointerSize);
      if (instr->InputAt(0)->IsSimd128Register()) {
        __ Poke(i.InputSimd128Register(0), operand);
      } else if (instr->InputAt(0)->IsFPRegister()) {
        __ Poke(i.InputFloat64Register(0), operand);
      } else {
        __ Poke(i.InputOrZeroRegister64(0), operand);
      }
      break;
    }
    case kArm64PokePair: {
      int slot = i.InputInt32(2) - 1;
      if (instr->InputAt(0)->IsFPRegister()) {
        __ PokePair(i.InputFloat64Register(1), i.InputFloat64Register(0),
                    slot * kSystemPointerSize);
      } else {
        __ PokePair(i.InputRegister(1), i.InputRegister(0),
                    slot * kSystemPointerSize);
      }
      break;
    }
    case kArm64Peek: {
      int reverse_slot = i.InputInt32(0);
      int offset =
          FrameSlotToFPOffset(frame()->GetTotalFrameSlotCount() - reverse_slot);
      if (instr->OutputAt(0)->IsFPRegister()) {
        LocationOperand* op = LocationOperand::cast(instr->OutputAt(0));
        if (op->representation() == MachineRepresentation::kFloat64) {
          __ Ldr(i.OutputDoubleRegister(), MemOperand(fp, offset));
        } else {
          DCHECK_EQ(MachineRepresentation::kFloat32, op->representation());
          __ Ldr(i.OutputFloatRegister(), MemOperand(fp, offset));
        }
      } else {
        __ Ldr(i.OutputRegister(), MemOperand(fp, offset));
      }
      break;
    }
    case kArm64Clz:
      __ Clz(i.OutputRegister64(), i.InputRegister64(0));
      break;
    case kArm64Clz32:
      __ Clz(i.OutputRegister32(), i.InputRegister32(0));
      break;
    case kArm64Rbit:
      __ Rbit(i.OutputRegister64(), i.InputRegister64(0));
      break;
    case kArm64Rbit32:
      __ Rbit(i.OutputRegister32(), i.InputRegister32(0));
      break;
    case kArm64Rev:
      __ Rev(i.OutputRegister64(), i.InputRegister64(0));
      break;
    case kArm64Rev32:
      __ Rev(i.OutputRegister32(), i.InputRegister32(0));
      break;
    case kArm64Cmp:
      __ Cmp(i.InputOrZeroRegister64(0), i.InputOperand2_64(1));
      break;
    case kArm64Cmp32:
      __ Cmp(i.InputOrZeroRegister32(0), i.InputOperand2_32(1));
      break;
    case kArm64Cmn:
      __ Cmn(i.InputOrZeroRegister64(0), i.InputOperand2_64(1));
      break;
    case kArm64Cmn32:
      __ Cmn(i.InputOrZeroRegister32(0), i.InputOperand2_32(1));
      break;
    case kArm64Tst:
      __ Tst(i.InputOrZeroRegister64(0), i.InputOperand2_64(1));
      break;
    case kArm64Tst32:
      __ Tst(i.InputOrZeroRegister32(0), i.InputOperand2_32(1));
      break;
    case kArm64Float32Cmp:
      if (instr->InputAt(1)->IsFPRegister()) {
        __ Fcmp(i.InputFloat32Register(0), i.InputFloat32Register(1));
      } else {
        DCHECK(instr->InputAt(1)->IsImmediate());
        // 0.0 is the only immediate supported by fcmp instructions.
        DCHECK_EQ(0.0f, i.InputFloat32(1));
        __ Fcmp(i.InputFloat32Register(0), i.InputFloat32(1));
      }
      break;
    case kArm64Float32Add:
      __ Fadd(i.OutputFloat32Register(), i.InputFloat32Register(0),
              i.InputFloat32Register(1));
      break;
    case kArm64Float32Sub:
      __ Fsub(i.OutputFloat32Register(), i.InputFloat32Register(0),
              i.InputFloat32Register(1));
      break;
    case kArm64Float32Mul:
      __ Fmul(i.OutputFloat32Register(), i.InputFloat32Register(0),
              i.InputFloat32Register(1));
      break;
    case kArm64Float32Div:
      __ Fdiv(i.OutputFloat32Register(), i.InputFloat32Register(0),
              i.InputFloat32Register(1));
      break;
    case kArm64Float32Abs:
      __ Fabs(i.OutputFloat32Register(), i.InputFloat32Register(0));
      break;
    case kArm64Float32Neg:
      __ Fneg(i.OutputFloat32Register(), i.InputFloat32Register(0));
      break;
    case kArm64Float32Sqrt:
      __ Fsqrt(i.OutputFloat32Register(), i.InputFloat32Register(0));
      break;
    case kArm64Float64Cmp:
      if (instr->InputAt(1)->IsFPRegister()) {
        __ Fcmp(i.InputDoubleRegister(0), i.InputDoubleRegister(1));
      } else {
        DCHECK(instr->InputAt(1)->IsImmediate());
        // 0.0 is the only immediate supported by fcmp instructions.
        DCHECK_EQ(0.0, i.InputDouble(1));
        __ Fcmp(i.InputDoubleRegister(0), i.InputDouble(1));
      }
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
      // TODO(turbofan): implement directly.
      FrameScope scope(tasm(), StackFrame::MANUAL);
      DCHECK(d0.is(i.InputDoubleRegister(0)));
      DCHECK(d1.is(i.InputDoubleRegister(1)));
      DCHECK(d0.is(i.OutputDoubleRegister()));
      // TODO(turbofan): make sure this saves all relevant registers.
      __ CallCFunction(ExternalReference::mod_two_doubles_operation(), 0, 2);
      break;
    }
    case kArm64Float32Max: {
      __ Fmax(i.OutputFloat32Register(), i.InputFloat32Register(0),
              i.InputFloat32Register(1));
      break;
    }
    case kArm64Float64Max: {
      __ Fmax(i.OutputDoubleRegister(), i.InputDoubleRegister(0),
              i.InputDoubleRegister(1));
      break;
    }
    case kArm64Float32Min: {
      __ Fmin(i.OutputFloat32Register(), i.InputFloat32Register(0),
              i.InputFloat32Register(1));
      break;
    }
    case kArm64Float64Min: {
      __ Fmin(i.OutputDoubleRegister(), i.InputDoubleRegister(0),
              i.InputDoubleRegister(1));
      break;
    }
    case kArm64Float64Abs:
      __ Fabs(i.OutputDoubleRegister(), i.InputDoubleRegister(0));
      break;
    case kArm64Float64Neg:
      __ Fneg(i.OutputDoubleRegister(), i.InputDoubleRegister(0));
      break;
    case kArm64Float64Sqrt:
      __ Fsqrt(i.OutputDoubleRegister(), i.InputDoubleRegister(0));
      break;
    case kArm64Float32ToFloat64:
      __ Fcvt(i.OutputDoubleRegister(), i.InputDoubleRegister(0).S());
      break;
    case kArm64Float64ToFloat32:
      __ Fcvt(i.OutputDoubleRegister().S(), i.InputDoubleRegister(0));
      break;
    case kArm64Float32ToInt32:
      __ Fcvtzs(i.OutputRegister32(), i.InputFloat32Register(0));
      // Avoid INT32_MAX as an overflow indicator and use INT32_MIN instead,
      // because INT32_MIN allows easier out-of-bounds detection.
      __ Cmn(i.OutputRegister32(), 1);
      __ Csinc(i.OutputRegister32(), i.OutputRegister32(), i.OutputRegister32(),
               vc);
      break;
    case kArm64Float64ToInt32:
      __ Fcvtzs(i.OutputRegister32(), i.InputDoubleRegister(0));
      break;
    case kArm64Float32ToUint32:
      __ Fcvtzu(i.OutputRegister32(), i.InputFloat32Register(0));
      // Avoid UINT32_MAX as an overflow indicator and use 0 instead,
      // because 0 allows easier out-of-bounds detection.
      __ Cmn(i.OutputRegister32(), 1);
      __ Adc(i.OutputRegister32(), i.OutputRegister32(), Operand(0));
      break;
    case kArm64Float64ToUint32:
      __ Fcvtzu(i.OutputRegister32(), i.InputDoubleRegister(0));
      break;
    case kArm64Float32ToInt64:
      __ Fcvtzs(i.OutputRegister64(), i.InputFloat32Register(0));
      if (i.OutputCount() > 1) {
        // Check for inputs below INT64_MIN and NaN.
        __ Fcmp(i.InputFloat32Register(0), static_cast<float>(INT64_MIN));
        // Check overflow.
        // -1 value is used to indicate a possible overflow which will occur
        // when subtracting (-1) from the provided INT64_MAX operand.
        // OutputRegister(1) is set to 0 if the input was out of range or NaN.
        __ Ccmp(i.OutputRegister(0), -1, VFlag, ge);
        __ Cset(i.OutputRegister(1), vc);
      }
      break;
    case kArm64Float64ToInt64:
      __ Fcvtzs(i.OutputRegister(0), i.InputDoubleRegister(0));
      if (i.OutputCount() > 1) {
        // See kArm64Float32ToInt64 for a detailed description.
        __ Fcmp(i.InputDoubleRegister(0), static_cast<double>(INT64_MIN));
        __ Ccmp(i.OutputRegister(0), -1, VFlag, ge);
        __ Cset(i.OutputRegister(1), vc);
      }
      break;
    case kArm64Float32ToUint64:
      __ Fcvtzu(i.OutputRegister64(), i.InputFloat32Register(0));
      if (i.OutputCount() > 1) {
        // See kArm64Float32ToInt64 for a detailed description.
        __ Fcmp(i.InputFloat32Register(0), -1.0);
        __ Ccmp(i.OutputRegister(0), -1, ZFlag, gt);
        __ Cset(i.OutputRegister(1), ne);
      }
      break;
    case kArm64Float64ToUint64:
      __ Fcvtzu(i.OutputRegister64(), i.InputDoubleRegister(0));
      if (i.OutputCount() > 1) {
        // See kArm64Float32ToInt64 for a detailed description.
        __ Fcmp(i.InputDoubleRegister(0), -1.0);
        __ Ccmp(i.OutputRegister(0), -1, ZFlag, gt);
        __ Cset(i.OutputRegister(1), ne);
      }
      break;
    case kArm64Int32ToFloat32:
      __ Scvtf(i.OutputFloat32Register(), i.InputRegister32(0));
      break;
    case kArm64Int32ToFloat64:
      __ Scvtf(i.OutputDoubleRegister(), i.InputRegister32(0));
      break;
    case kArm64Int64ToFloat32:
      __ Scvtf(i.OutputDoubleRegister().S(), i.InputRegister64(0));
      break;
    case kArm64Int64ToFloat64:
      __ Scvtf(i.OutputDoubleRegister(), i.InputRegister64(0));
      break;
    case kArm64Uint32ToFloat32:
      __ Ucvtf(i.OutputFloat32Register(), i.InputRegister32(0));
      break;
    case kArm64Uint32ToFloat64:
      __ Ucvtf(i.OutputDoubleRegister(), i.InputRegister32(0));
      break;
    case kArm64Uint64ToFloat32:
      __ Ucvtf(i.OutputDoubleRegister().S(), i.InputRegister64(0));
      break;
    case kArm64Uint64ToFloat64:
      __ Ucvtf(i.OutputDoubleRegister(), i.InputRegister64(0));
      break;
    case kArm64Float64ExtractLowWord32:
      __ Fmov(i.OutputRegister32(), i.InputFloat32Register(0));
      break;
    case kArm64Float64ExtractHighWord32:
      __ Umov(i.OutputRegister32(), i.InputFloat64Register(0).V2S(), 1);
      break;
    case kArm64Float64InsertLowWord32:
      DCHECK(i.OutputFloat64Register().Is(i.InputFloat64Register(0)));
      __ Ins(i.OutputFloat64Register().V2S(), 0, i.InputRegister32(1));
      break;
    case kArm64Float64InsertHighWord32:
      DCHECK(i.OutputFloat64Register().Is(i.InputFloat64Register(0)));
      __ Ins(i.OutputFloat64Register().V2S(), 1, i.InputRegister32(1));
      break;
    case kArm64Float64MoveU64:
      __ Fmov(i.OutputFloat64Register(), i.InputRegister(0));
      break;
    case kArm64Float64SilenceNaN:
      __ CanonicalizeNaN(i.OutputDoubleRegister(), i.InputDoubleRegister(0));
      break;
    case kArm64U64MoveFloat64:
      __ Fmov(i.OutputRegister(), i.InputDoubleRegister(0));
      break;
    case kArm64Ldrb:
      __ Ldrb(i.OutputRegister(), i.MemoryOperand());
      EmitWordLoadPoisoningIfNeeded(this, opcode, instr, i);
      break;
    case kArm64Ldrsb:
      __ Ldrsb(i.OutputRegister(), i.MemoryOperand());
      EmitWordLoadPoisoningIfNeeded(this, opcode, instr, i);
      break;
    case kArm64Strb:
      __ Strb(i.InputOrZeroRegister64(0), i.MemoryOperand(1));
      break;
    case kArm64Ldrh:
      __ Ldrh(i.OutputRegister(), i.MemoryOperand());
      EmitWordLoadPoisoningIfNeeded(this, opcode, instr, i);
      break;
    case kArm64Ldrsh:
      __ Ldrsh(i.OutputRegister(), i.MemoryOperand());
      EmitWordLoadPoisoningIfNeeded(this, opcode, instr, i);
      break;
    case kArm64Strh:
      __ Strh(i.InputOrZeroRegister64(0), i.MemoryOperand(1));
      break;
    case kArm64Ldrsw:
      __ Ldrsw(i.OutputRegister(), i.MemoryOperand());
      EmitWordLoadPoisoningIfNeeded(this, opcode, instr, i);
      break;
    case kArm64LdrW:
      __ Ldr(i.OutputRegister32(), i.MemoryOperand());
      EmitWordLoadPoisoningIfNeeded(this, opcode, instr, i);
      break;
    case kArm64StrW:
      __ Str(i.InputOrZeroRegister32(0), i.MemoryOperand(1));
      break;
    case kArm64Ldr:
      __ Ldr(i.OutputRegister(), i.MemoryOperand());
      EmitWordLoadPoisoningIfNeeded(this, opcode, instr, i);
      break;
    case kArm64LdrDecompressTaggedSigned:
      __ DecompressTaggedSigned(i.OutputRegister(), i.MemoryOperand());
      EmitWordLoadPoisoningIfNeeded(this, opcode, instr, i);
      break;
    case kArm64LdrDecompressTaggedPointer:
      __ DecompressTaggedPointer(i.OutputRegister(), i.MemoryOperand());
      EmitWordLoadPoisoningIfNeeded(this, opcode, instr, i);
      break;
    case kArm64LdrDecompressAnyTagged:
      __ DecompressAnyTagged(i.OutputRegister(), i.MemoryOperand());
      EmitWordLoadPoisoningIfNeeded(this, opcode, instr, i);
      break;
    case kArm64Str:
      __ Str(i.InputOrZeroRegister64(0), i.MemoryOperand(1));
      break;
    case kArm64DecompressSigned: {
      __ Sxtw(i.OutputRegister(), i.InputRegister(0));
      break;
    }
    case kArm64DecompressPointer: {
      __ Add(i.OutputRegister(), kRootRegister,
             Operand(i.InputRegister(0), SXTW));
      break;
    }
    case kArm64DecompressAny: {
      // TODO(solanes): Do branchful compute?
      // Branchlessly compute |masked_root|:
      STATIC_ASSERT((kSmiTagSize == 1) && (kSmiTag == 0));
      UseScratchRegisterScope temps(tasm());
      Register masked_root = temps.AcquireX();
      // Sign extend tag bit to entire register.
      __ Sbfx(masked_root, i.InputRegister(0), 0, kSmiTagSize);
      __ And(masked_root, masked_root, kRootRegister);
      // Now this add operation will either leave the value unchanged if it is a
      // smi or add the isolate root if it is a heap object.
      __ Add(i.OutputRegister(), masked_root,
             Operand(i.InputRegister(0), SXTW));
      break;
    }
    // TODO(solanes): Combine into one Compress? They seem to be identical.
    // TODO(solanes): We might get away with doing a no-op in these three cases.
    // The Uxtw instruction is the conservative way for the moment.
    case kArm64CompressSigned: {
      __ Uxtw(i.OutputRegister(), i.InputRegister(0));
      break;
    }
    case kArm64CompressPointer: {
      __ Uxtw(i.OutputRegister(), i.InputRegister(0));
      break;
    }
    case kArm64CompressAny: {
      __ Uxtw(i.OutputRegister(), i.InputRegister(0));
      break;
    }
    case kArm64LdrS:
      __ Ldr(i.OutputDoubleRegister().S(), i.MemoryOperand());
      break;
    case kArm64StrS:
      __ Str(i.InputFloat32OrZeroRegister(0), i.MemoryOperand(1));
      break;
    case kArm64LdrD:
      __ Ldr(i.OutputDoubleRegister(), i.MemoryOperand());
      break;
    case kArm64StrD:
      __ Str(i.InputFloat64OrZeroRegister(0), i.MemoryOperand(1));
      break;
    case kArm64LdrQ:
      __ Ldr(i.OutputSimd128Register(), i.MemoryOperand());
      break;
    case kArm64StrQ:
      __ Str(i.InputSimd128Register(0), i.MemoryOperand(1));
      break;
    case kArm64StrCompressTagged:
      __ StoreTaggedField(i.InputOrZeroRegister64(0), i.MemoryOperand(1));
      break;
    case kArm64DsbIsb:
      __ Dsb(FullSystem, BarrierAll);
      __ Isb();
      break;
    case kArchWordPoisonOnSpeculation:
      __ And(i.OutputRegister(0), i.InputRegister(0),
             Operand(kSpeculationPoisonRegister));
      break;
    case kWord32AtomicLoadInt8:
      ASSEMBLE_ATOMIC_LOAD_INTEGER(Ldarb, Register32);
      __ Sxtb(i.OutputRegister(0), i.OutputRegister(0));
      break;
    case kWord32AtomicLoadUint8:
    case kArm64Word64AtomicLoadUint8:
      ASSEMBLE_ATOMIC_LOAD_INTEGER(Ldarb, Register32);
      break;
    case kWord32AtomicLoadInt16:
      ASSEMBLE_ATOMIC_LOAD_INTEGER(Ldarh, Register32);
      __ Sxth(i.OutputRegister(0), i.OutputRegister(0));
      break;
    case kWord32AtomicLoadUint16:
    case kArm64Word64AtomicLoadUint16:
      ASSEMBLE_ATOMIC_LOAD_INTEGER(Ldarh, Register32);
      break;
    case kWord32AtomicLoadWord32:
    case kArm64Word64AtomicLoadUint32:
      ASSEMBLE_ATOMIC_LOAD_INTEGER(Ldar, Register32);
      break;
    case kArm64Word64AtomicLoadUint64:
      ASSEMBLE_ATOMIC_LOAD_INTEGER(Ldar, Register);
      break;
    case kWord32AtomicStoreWord8:
    case kArm64Word64AtomicStoreWord8:
      ASSEMBLE_ATOMIC_STORE_INTEGER(Stlrb, Register32);
      break;
    case kWord32AtomicStoreWord16:
    case kArm64Word64AtomicStoreWord16:
      ASSEMBLE_ATOMIC_STORE_INTEGER(Stlrh, Register32);
      break;
    case kWord32AtomicStoreWord32:
    case kArm64Word64AtomicStoreWord32:
      ASSEMBLE_ATOMIC_STORE_INTEGER(Stlr, Register32);
      break;
    case kArm64Word64AtomicStoreWord64:
      ASSEMBLE_ATOMIC_STORE_INTEGER(Stlr, Register);
      break;
    case kWord32AtomicExchangeInt8:
      ASSEMBLE_ATOMIC_EXCHANGE_INTEGER(ldaxrb, stlxrb, Register32);
      __ Sxtb(i.OutputRegister(0), i.OutputRegister(0));
      break;
    case kWord32AtomicExchangeUint8:
    case kArm64Word64AtomicExchangeUint8:
      ASSEMBLE_ATOMIC_EXCHANGE_INTEGER(ldaxrb, stlxrb, Register32);
      break;
    case kWord32AtomicExchangeInt16:
      ASSEMBLE_ATOMIC_EXCHANGE_INTEGER(ldaxrh, stlxrh, Register32);
      __ Sxth(i.OutputRegister(0), i.OutputRegister(0));
      break;
    case kWord32AtomicExchangeUint16:
    case kArm64Word64AtomicExchangeUint16:
      ASSEMBLE_ATOMIC_EXCHANGE_INTEGER(ldaxrh, stlxrh, Register32);
      break;
    case kWord32AtomicExchangeWord32:
    case kArm64Word64AtomicExchangeUint32:
      ASSEMBLE_ATOMIC_EXCHANGE_INTEGER(ldaxr, stlxr, Register32);
      break;
    case kArm64Word64AtomicExchangeUint64:
      ASSEMBLE_ATOMIC_EXCHANGE_INTEGER(ldaxr, stlxr, Register);
      break;
    case kWord32AtomicCompareExchangeInt8:
      ASSEMBLE_ATOMIC_COMPARE_EXCHANGE_INTEGER(ldaxrb, stlxrb, UXTB,
                                               Register32);
      __ Sxtb(i.OutputRegister(0), i.OutputRegister(0));
      break;
    case kWord32AtomicCompareExchangeUint8:
    case kArm64Word64AtomicCompareExchangeUint8:
      ASSEMBLE_ATOMIC_COMPARE_EXCHANGE_INTEGER(ldaxrb, stlxrb, UXTB,
                                               Register32);
      break;
    case kWord32AtomicCompareExchangeInt16:
      ASSEMBLE_ATOMIC_COMPARE_EXCHANGE_INTEGER(ldaxrh, stlxrh, UXTH,
                                               Register32);
      __ Sxth(i.OutputRegister(0), i.OutputRegister(0));
      break;
    case kWord32AtomicCompareExchangeUint16:
    case kArm64Word64AtomicCompareExchangeUint16:
      ASSEMBLE_ATOMIC_COMPARE_EXCHANGE_INTEGER(ldaxrh, stlxrh, UXTH,
                                               Register32);
      break;
    case kWord32AtomicCompareExchangeWord32:
    case kArm64Word64AtomicCompareExchangeUint32:
      ASSEMBLE_ATOMIC_COMPARE_EXCHANGE_INTEGER(ldaxr, stlxr, UXTW, Register32);
      break;
    case kArm64Word64AtomicCompareExchangeUint64:
      ASSEMBLE_ATOMIC_COMPARE_EXCHANGE_INTEGER(ldaxr, stlxr, UXTX, Register);
      break;
#define ATOMIC_BINOP_CASE(op, inst)                          \
  case kWord32Atomic##op##Int8:                              \
    ASSEMBLE_ATOMIC_BINOP(ldaxrb, stlxrb, inst, Register32); \
    __ Sxtb(i.OutputRegister(0), i.OutputRegister(0));       \
    break;                                                   \
  case kWord32Atomic##op##Uint8:                             \
  case kArm64Word64Atomic##op##Uint8:                        \
    ASSEMBLE_ATOMIC_BINOP(ldaxrb, stlxrb, inst, Register32); \
    break;                                                   \
  case kWord32Atomic##op##Int16:                             \
    ASSEMBLE_ATOMIC_BINOP(ldaxrh, stlxrh, inst, Register32); \
    __ Sxth(i.OutputRegister(0), i.OutputRegister(0));       \
    break;                                                   \
  case kWord32Atomic##op##Uint16:                            \
  case kArm64Word64Atomic##op##Uint16:                       \
    ASSEMBLE_ATOMIC_BINOP(ldaxrh, stlxrh, inst, Register32); \
    break;                                                   \
  case kWord32Atomic##op##Word32:                            \
  case kArm64Word64Atomic##op##Uint32:                       \
    ASSEMBLE_ATOMIC_BINOP(ldaxr, stlxr, inst, Register32);   \
    break;                                                   \
  case kArm64Word64Atomic##op##Uint64:                       \
    ASSEMBLE_ATOMIC_BINOP(ldaxr, stlxr, inst, Register);     \
    break;
      ATOMIC_BINOP_CASE(Add, Add)
      ATOMIC_BINOP_CASE(Sub, Sub)
      ATOMIC_BINOP_CASE(And, And)
      ATOMIC_BINOP_CASE(Or, Orr)
      ATOMIC_BINOP_CASE(Xor, Eor)
#undef ATOMIC_BINOP_CASE
#undef ASSEMBLE_SHIFT
#undef ASSEMBLE_ATOMIC_LOAD_INTEGER
#undef ASSEMBLE_ATOMIC_STORE_INTEGER
#undef ASSEMBLE_ATOMIC_EXCHANGE_INTEGER
#undef ASSEMBLE_ATOMIC_COMPARE_EXCHANGE_INTEGER
#undef ASSEMBLE_ATOMIC_BINOP
#undef ASSEMBLE_IEEE754_BINOP
#undef ASSEMBLE_IEEE754_UNOP

#define SIMD_UNOP_CASE(Op, Instr, FORMAT)            \
  case Op:                                           \
    __ Instr(i.OutputSimd128Register().V##FORMAT(),  \
             i.InputSimd128Register(0).V##FORMAT()); \
    break;
#define SIMD_WIDENING_UNOP_CASE(Op, Instr, WIDE, NARROW) \
  case Op:                                               \
    __ Instr(i.OutputSimd128Register().V##WIDE(),        \
             i.InputSimd128Register(0).V##NARROW());     \
    break;
#define SIMD_BINOP_CASE(Op, Instr, FORMAT)           \
  case Op:                                           \
    __ Instr(i.OutputSimd128Register().V##FORMAT(),  \
             i.InputSimd128Register(0).V##FORMAT(),  \
             i.InputSimd128Register(1).V##FORMAT()); \
    break;

    case kArm64F32x4Splat: {
      __ Dup(i.OutputSimd128Register().V4S(), i.InputSimd128Register(0).S(), 0);
      break;
    }
    case kArm64F32x4ExtractLane: {
      __ Mov(i.OutputSimd128Register().S(), i.InputSimd128Register(0).V4S(),
             i.InputInt8(1));
      break;
    }
    case kArm64F32x4ReplaceLane: {
      VRegister dst = i.OutputSimd128Register().V4S(),
                src1 = i.InputSimd128Register(0).V4S();
      if (!dst.is(src1)) {
        __ Mov(dst, src1);
      }
      __ Mov(dst, i.InputInt8(1), i.InputSimd128Register(2).V4S(), 0);
      break;
    }
      SIMD_UNOP_CASE(kArm64F32x4SConvertI32x4, Scvtf, 4S);
      SIMD_UNOP_CASE(kArm64F32x4UConvertI32x4, Ucvtf, 4S);
      SIMD_UNOP_CASE(kArm64F32x4Abs, Fabs, 4S);
      SIMD_UNOP_CASE(kArm64F32x4Neg, Fneg, 4S);
      SIMD_UNOP_CASE(kArm64F32x4RecipApprox, Frecpe, 4S);
      SIMD_UNOP_CASE(kArm64F32x4RecipSqrtApprox, Frsqrte, 4S);
      SIMD_BINOP_CASE(kArm64F32x4Add, Fadd, 4S);
      SIMD_BINOP_CASE(kArm64F32x4AddHoriz, Faddp, 4S);
      SIMD_BINOP_CASE(kArm64F32x4Sub, Fsub, 4S);
      SIMD_BINOP_CASE(kArm64F32x4Mul, Fmul, 4S);
      SIMD_BINOP_CASE(kArm64F32x4Min, Fmin, 4S);
      SIMD_BINOP_CASE(kArm64F32x4Max, Fmax, 4S);
      SIMD_BINOP_CASE(kArm64F32x4Eq, Fcmeq, 4S);
    case kArm64F32x4Ne: {
      VRegister dst = i.OutputSimd128Register().V4S();
      __ Fcmeq(dst, i.InputSimd128Register(0).V4S(),
               i.InputSimd128Register(1).V4S());
      __ Mvn(dst, dst);
      break;
    }
    case kArm64F32x4Lt: {
      __ Fcmgt(i.OutputSimd128Register().V4S(), i.InputSimd128Register(1).V4S(),
               i.InputSimd128Register(0).V4S());
      break;
    }
    case kArm64F32x4Le: {
      __ Fcmge(i.OutputSimd128Register().V4S(), i.InputSimd128Register(1).V4S(),
               i.InputSimd128Register(0).V4S());
      break;
    }
    case kArm64I32x4Splat: {
      __ Dup(i.OutputSimd128Register().V4S(), i.InputRegister32(0));
      break;
    }
    case kArm64I32x4ExtractLane: {
      __ Mov(i.OutputRegister32(), i.InputSimd128Register(0).V4S(),
             i.InputInt8(1));
      break;
    }
    case kArm64I32x4ReplaceLane: {
      VRegister dst = i.OutputSimd128Register().V4S(),
                src1 = i.InputSimd128Register(0).V4S();
      if (!dst.is(src1)) {
        __ Mov(dst, src1);
      }
      __ Mov(dst, i.InputInt8(1), i.InputRegister32(2));
      break;
    }
      SIMD_UNOP_CASE(kArm64I32x4SConvertF32x4, Fcvtzs, 4S);
      SIMD_WIDENING_UNOP_CASE(kArm64I32x4SConvertI16x8Low, Sxtl, 4S, 4H);
      SIMD_WIDENING_UNOP_CASE(kArm64I32x4SConvertI16x8High, Sxtl2, 4S, 8H);
      SIMD_UNOP_CASE(kArm64I32x4Neg, Neg, 4S);
    case kArm64I32x4Shl: {
      __ Shl(i.OutputSimd128Register().V4S(), i.InputSimd128Register(0).V4S(),
             i.InputInt5(1));
      break;
    }
    case kArm64I32x4ShrS: {
      __ Sshr(i.OutputSimd128Register().V4S(), i.InputSimd128Register(0).V4S(),
              i.InputInt5(1));
      break;
    }
      SIMD_BINOP_CASE(kArm64I32x4Add, Add, 4S);
      SIMD_BINOP_CASE(kArm64I32x4AddHoriz, Addp, 4S);
      SIMD_BINOP_CASE(kArm64I32x4Sub, Sub, 4S);
      SIMD_BINOP_CASE(kArm64I32x4Mul, Mul, 4S);
      SIMD_BINOP_CASE(kArm64I32x4MinS, Smin, 4S);
      SIMD_BINOP_CASE(kArm64I32x4MaxS, Smax, 4S);
      SIMD_BINOP_CASE(kArm64I32x4Eq, Cmeq, 4S);
    case kArm64I32x4Ne: {
      VRegister dst = i.OutputSimd128Register().V4S();
      __ Cmeq(dst, i.InputSimd128Register(0).V4S(),
              i.InputSimd128Register(1).V4S());
      __ Mvn(dst, dst);
      break;
    }
      SIMD_BINOP_CASE(kArm64I32x4GtS, Cmgt, 4S);
      SIMD_BINOP_CASE(kArm64I32x4GeS, Cmge, 4S);
      SIMD_UNOP_CASE(kArm64I32x4UConvertF32x4, Fcvtzu, 4S);
      SIMD_WIDENING_UNOP_CASE(kArm64I32x4UConvertI16x8Low, Uxtl, 4S, 4H);
      SIMD_WIDENING_UNOP_CASE(kArm64I32x4UConvertI16x8High, Uxtl2, 4S, 8H);
    case kArm64I32x4ShrU: {
      __ Ushr(i.OutputSimd128Register().V4S(), i.InputSimd128Register(0).V4S(),
              i.InputInt5(1));
      break;
    }
      SIMD_BINOP_CASE(kArm64I32x4MinU, Umin, 4S);
      SIMD_BINOP_CASE(kArm64I32x4MaxU, Umax, 4S);
      SIMD_BINOP_CASE(kArm64I32x4GtU, Cmhi, 4S);
      SIMD_BINOP_CASE(kArm64I32x4GeU, Cmhs, 4S);
    case kArm64I16x8Splat: {
      __ Dup(i.OutputSimd128Register().V8H(), i.InputRegister32(0));
      break;
    }
    case kArm64I16x8ExtractLane: {
      __ Smov(i.OutputRegister32(), i.InputSimd128Register(0).V8H(),
              i.InputInt8(1));
      break;
    }
    case kArm64I16x8ReplaceLane: {
      VRegister dst = i.OutputSimd128Register().V8H(),
                src1 = i.InputSimd128Register(0).V8H();
      if (!dst.is(src1)) {
        __ Mov(dst, src1);
      }
      __ Mov(dst, i.InputInt8(1), i.InputRegister32(2));
      break;
    }
      SIMD_WIDENING_UNOP_CASE(kArm64I16x8SConvertI8x16Low, Sxtl, 8H, 8B);
      SIMD_WIDENING_UNOP_CASE(kArm64I16x8SConvertI8x16High, Sxtl2, 8H, 16B);
      SIMD_UNOP_CASE(kArm64I16x8Neg, Neg, 8H);
    case kArm64I16x8Shl: {
      __ Shl(i.OutputSimd128Register().V8H(), i.InputSimd128Register(0).V8H(),
             i.InputInt5(1));
      break;
    }
    case kArm64I16x8ShrS: {
      __ Sshr(i.OutputSimd128Register().V8H(), i.InputSimd128Register(0).V8H(),
              i.InputInt5(1));
      break;
    }
    case kArm64I16x8SConvertI32x4: {
      VRegister dst = i.OutputSimd128Register(),
                src0 = i.InputSimd128Register(0),
                src1 = i.InputSimd128Register(1);
      UseScratchRegisterScope scope(tasm());
      VRegister temp = scope.AcquireV(kFormat4S);
      if (dst.is(src1)) {
        __ Mov(temp, src1.V4S());
        src1 = temp;
      }
      __ Sqxtn(dst.V4H(), src0.V4S());
      __ Sqxtn2(dst.V8H(), src1.V4S());
      break;
    }
      SIMD_BINOP_CASE(kArm64I16x8Add, Add, 8H);
      SIMD_BINOP_CASE(kArm64I16x8AddSaturateS, Sqadd, 8H);
      SIMD_BINOP_CASE(kArm64I16x8AddHoriz, Addp, 8H);
      SIMD_BINOP_CASE(kArm64I16x8Sub, Sub, 8H);
      SIMD_BINOP_CASE(kArm64I16x8SubSaturateS, Sqsub, 8H);
      SIMD_BINOP_CASE(kArm64I16x8Mul, Mul, 8H);
      SIMD_BINOP_CASE(kArm64I16x8MinS, Smin, 8H);
      SIMD_BINOP_CASE(kArm64I16x8MaxS, Smax, 8H);
      SIMD_BINOP_CASE(kArm64I16x8Eq, Cmeq, 8H);
    case kArm64I16x8Ne: {
      VRegister dst = i.OutputSimd128Register().V8H();
      __ Cmeq(dst, i.InputSimd128Register(0).V8H(),
              i.InputSimd128Register(1).V8H());
      __ Mvn(dst, dst);
      break;
    }
      SIMD_BINOP_CASE(kArm64I16x8GtS, Cmgt, 8H);
      SIMD_BINOP_CASE(kArm64I16x8GeS, Cmge, 8H);
    case kArm64I16x8UConvertI8x16Low: {
      __ Uxtl(i.OutputSimd128Register().V8H(), i.InputSimd128Register(0).V8B());
      break;
    }
    case kArm64I16x8UConvertI8x16High: {
      __ Uxtl2(i.OutputSimd128Register().V8H(),
               i.InputSimd128Register(0).V16B());
      break;
    }
    case kArm64I16x8ShrU: {
      __ Ushr(i.OutputSimd128Register().V8H(), i.InputSimd128Register(0).V8H(),
              i.InputInt5(1));
      break;
    }
    case kArm64I16x8UConvertI32x4: {
      VRegister dst = i.OutputSimd128Register(),
                src0 = i.InputSimd128Register(0),
                src1 = i.InputSimd128Register(1);
      UseScratchRegisterScope scope(tasm());
      VRegister temp = scope.AcquireV(kFormat4S);
      if (dst.is(src1)) {
        __ Mov(temp, src1.V4S());
        src1 = temp;
      }
      __ Uqxtn(dst.V4H(), src0.V4S());
      __ Uqxtn2(dst.V8H(), src1.V4S());
      break;
    }
      SIMD_BINOP_CASE(kArm64I16x8AddSaturateU, Uqadd, 8H);
      SIMD_BINOP_CASE(kArm64I16x8SubSaturateU, Uqsub, 8H);
      SIMD_BINOP_CASE(kArm64I16x8MinU, Umin, 8H);
      SIMD_BINOP_CASE(kArm64I16x8MaxU, Umax, 8H);
      SIMD_BINOP_CASE(kArm64I16x8GtU, Cmhi, 8H);
      SIMD_BINOP_CASE(kArm64I16x8GeU, Cmhs, 8H);
    case kArm64I8x16Splat: {
      __ Dup(i.OutputSimd128Register().V16B(), i.InputRegister32(0));
      break;
    }
    case kArm64I8x16ExtractLane: {
      __ Smov(i.OutputRegister32(), i.InputSimd128Register(0).V16B(),
              i.InputInt8(1));
      break;
    }
    case kArm64I8x16ReplaceLane: {
      VRegister dst = i.OutputSimd128Register().V16B(),
                src1 = i.InputSimd128Register(0).V16B();
      if (!dst.is(src1)) {
        __ Mov(dst, src1);
      }
      __ Mov(dst, i.InputInt8(1), i.InputRegister32(2));
      break;
    }
      SIMD_UNOP_CASE(kArm64I8x16Neg, Neg, 16B);
    case kArm64I8x16Shl: {
      __ Shl(i.OutputSimd128Register().V16B(), i.InputSimd128Register(0).V16B(),
             i.InputInt5(1));
      break;
    }
    case kArm64I8x16ShrS: {
      __ Sshr(i.OutputSimd128Register().V16B(),
              i.InputSimd128Register(0).V16B(), i.InputInt5(1));
      break;
    }
    case kArm64I8x16SConvertI16x8: {
      VRegister dst = i.OutputSimd128Register(),
                src0 = i.InputSimd128Register(0),
                src1 = i.InputSimd128Register(1);
      UseScratchRegisterScope scope(tasm());
      VRegister temp = scope.AcquireV(kFormat8H);
      if (dst.is(src1)) {
        __ Mov(temp, src1.V8H());
        src1 = temp;
      }
      __ Sqxtn(dst.V8B(), src0.V8H());
      __ Sqxtn2(dst.V16B(), src1.V8H());
      break;
    }
      SIMD_BINOP_CASE(kArm64I8x16Add, Add, 16B);
      SIMD_BINOP_CASE(kArm64I8x16AddSaturateS, Sqadd, 16B);
      SIMD_BINOP_CASE(kArm64I8x16Sub, Sub, 16B);
      SIMD_BINOP_CASE(kArm64I8x16SubSaturateS, Sqsub, 16B);
      SIMD_BINOP_CASE(kArm64I8x16Mul, Mul, 16B);
      SIMD_BINOP_CASE(kArm64I8x16MinS, Smin, 16B);
      SIMD_BINOP_CASE(kArm64I8x16MaxS, Smax, 16B);
      SIMD_BINOP_CASE(kArm64I8x16Eq, Cmeq, 16B);
    case kArm64I8x16Ne: {
      VRegister dst = i.OutputSimd128Register().V16B();
      __ Cmeq(dst, i.InputSimd128Register(0).V16B(),
              i.InputSimd128Register(1).V16B());
      __ Mvn(dst, dst);
      break;
    }
      SIMD_BINOP_CASE(kArm64I8x16GtS, Cmgt, 16B);
      SIMD_BINOP_CASE(kArm64I8x16GeS, Cmge, 16B);
    case kArm64I8x16ShrU: {
      __ Ushr(i.OutputSimd128Register().V16B(),
              i.InputSimd128Register(0).V16B(), i.InputInt5(1));
      break;
    }
    case kArm64I8x16UConvertI16x8: {
      VRegister dst = i.OutputSimd128Register(),
                src0 = i.InputSimd128Register(0),
                src1 = i.InputSimd128Register(1);
      UseScratchRegisterScope scope(tasm());
      VRegister temp = scope.AcquireV(kFormat8H);
      if (dst.is(src1)) {
        __ Mov(temp, src1.V8H());
        src1 = temp;
      }
      __ Uqxtn(dst.V8B(), src0.V8H());
      __ Uqxtn2(dst.V16B(), src1.V8H());
      break;
    }
      SIMD_BINOP_CASE(kArm64I8x16AddSaturateU, Uqadd, 16B);
      SIMD_BINOP_CASE(kArm64I8x16SubSaturateU, Uqsub, 16B);
      SIMD_BINOP_CASE(kArm64I8x16MinU, Umin, 16B);
      SIMD_BINOP_CASE(kArm64I8x16MaxU, Umax, 16B);
      SIMD_BINOP_CASE(kArm64I8x16GtU, Cmhi, 16B);
      SIMD_BINOP_CASE(kArm64I8x16GeU, Cmhs, 16B);
    case kArm64S128Zero: {
      __ Movi(i.OutputSimd128Register().V16B(), 0);
      break;
    }
      SIMD_BINOP_CASE(kArm64S128And, And, 16B);
      SIMD_BINOP_CASE(kArm64S128Or, Orr, 16B);
      SIMD_BINOP_CASE(kArm64S128Xor, Eor, 16B);
      SIMD_UNOP_CASE(kArm64S128Not, Mvn, 16B);
    case kArm64S128Dup: {
      VRegister dst = i.OutputSimd128Register(),
                src = i.InputSimd128Register(0);
      int lanes = i.InputInt32(1);
      int index = i.InputInt32(2);
      switch (lanes) {
        case 4:
          __ Dup(dst.V4S(), src.V4S(), index);
          break;
        case 8:
          __ Dup(dst.V8H(), src.V8H(), index);
          break;
        case 16:
          __ Dup(dst.V16B(), src.V16B(), index);
          break;
        default:
          UNREACHABLE();
          break;
      }
      break;
    }
    case kArm64S128Select: {
      VRegister dst = i.OutputSimd128Register().V16B();
      DCHECK(dst.is(i.InputSimd128Register(0).V16B()));
      __ Bsl(dst, i.InputSimd128Register(1).V16B(),
             i.InputSimd128Register(2).V16B());
      break;
    }
    case kArm64S32x4Shuffle: {
      Simd128Register dst = i.OutputSimd128Register().V4S(),
                      src0 = i.InputSimd128Register(0).V4S(),
                      src1 = i.InputSimd128Register(1).V4S();
      // Check for in-place shuffles.
      // If dst == src0 == src1, then the shuffle is unary and we only use src0.
      UseScratchRegisterScope scope(tasm());
      VRegister temp = scope.AcquireV(kFormat4S);
      if (dst.is(src0)) {
        __ Mov(temp, src0);
        src0 = temp;
      } else if (dst.is(src1)) {
        __ Mov(temp, src1);
        src1 = temp;
      }
      // Perform shuffle as a vmov per lane.
      int32_t shuffle = i.InputInt32(2);
      for (int i = 0; i < 4; i++) {
        VRegister src = src0;
        int lane = shuffle & 0x7;
        if (lane >= 4) {
          src = src1;
          lane &= 0x3;
        }
        __ Mov(dst, i, src, lane);
        shuffle >>= 8;
      }
      break;
    }
      SIMD_BINOP_CASE(kArm64S32x4ZipLeft, Zip1, 4S);
      SIMD_BINOP_CASE(kArm64S32x4ZipRight, Zip2, 4S);
      SIMD_BINOP_CASE(kArm64S32x4UnzipLeft, Uzp1, 4S);
      SIMD_BINOP_CASE(kArm64S32x4UnzipRight, Uzp2, 4S);
      SIMD_BINOP_CASE(kArm64S32x4TransposeLeft, Trn1, 4S);
      SIMD_BINOP_CASE(kArm64S32x4TransposeRight, Trn2, 4S);
      SIMD_BINOP_CASE(kArm64S16x8ZipLeft, Zip1, 8H);
      SIMD_BINOP_CASE(kArm64S16x8ZipRight, Zip2, 8H);
      SIMD_BINOP_CASE(kArm64S16x8UnzipLeft, Uzp1, 8H);
      SIMD_BINOP_CASE(kArm64S16x8UnzipRight, Uzp2, 8H);
      SIMD_BINOP_CASE(kArm64S16x8TransposeLeft, Trn1, 8H);
      SIMD_BINOP_CASE(kArm64S16x8TransposeRight, Trn2, 8H);
      SIMD_BINOP_CASE(kArm64S8x16ZipLeft, Zip1, 16B);
      SIMD_BINOP_CASE(kArm64S8x16ZipRight, Zip2, 16B);
      SIMD_BINOP_CASE(kArm64S8x16UnzipLeft, Uzp1, 16B);
      SIMD_BINOP_CASE(kArm64S8x16UnzipRight, Uzp2, 16B);
      SIMD_BINOP_CASE(kArm64S8x16TransposeLeft, Trn1, 16B);
      SIMD_BINOP_CASE(kArm64S8x16TransposeRight, Trn2, 16B);
    case kArm64S8x16Concat: {
      __ Ext(i.OutputSimd128Register().V16B(), i.InputSimd128Register(0).V16B(),
             i.InputSimd128Register(1).V16B(), i.InputInt4(2));
      break;
    }
    case kArm64S8x16Shuffle: {
      Simd128Register dst = i.OutputSimd128Register().V16B(),
                      src0 = i.InputSimd128Register(0).V16B(),
                      src1 = i.InputSimd128Register(1).V16B();
      // Unary shuffle table is in src0, binary shuffle table is in src0, src1,
      // which must be consecutive.
      int64_t mask = 0;
      if (src0.is(src1)) {
        mask = 0x0F0F0F0F;
      } else {
        mask = 0x1F1F1F1F;
        DCHECK(AreConsecutive(src0, src1));
      }
      int64_t imm1 =
          (i.InputInt32(2) & mask) | ((i.InputInt32(3) & mask) << 32);
      int64_t imm2 =
          (i.InputInt32(4) & mask) | ((i.InputInt32(5) & mask) << 32);
      UseScratchRegisterScope scope(tasm());
      VRegister temp = scope.AcquireV(kFormat16B);
      __ Movi(temp, imm2, imm1);

      if (src0.is(src1)) {
        __ Tbl(dst, src0, temp.V16B());
      } else {
        __ Tbl(dst, src0, src1, temp.V16B());
      }
      break;
    }
      SIMD_UNOP_CASE(kArm64S32x2Reverse, Rev64, 4S);
      SIMD_UNOP_CASE(kArm64S16x4Reverse, Rev64, 8H);
      SIMD_UNOP_CASE(kArm64S16x2Reverse, Rev32, 8H);
      SIMD_UNOP_CASE(kArm64S8x8Reverse, Rev64, 16B);
      SIMD_UNOP_CASE(kArm64S8x4Reverse, Rev32, 16B);
      SIMD_UNOP_CASE(kArm64S8x2Reverse, Rev16, 16B);

#define SIMD_REDUCE_OP_CASE(Op, Instr, format, FORMAT)     \
  case Op: {                                               \
    UseScratchRegisterScope scope(tasm());                 \
    VRegister temp = scope.AcquireV(format);               \
    __ Instr(temp, i.InputSimd128Register(0).V##FORMAT()); \
    __ Umov(i.OutputRegister32(), temp, 0);                \
    break;                                                 \
  }
      SIMD_REDUCE_OP_CASE(kArm64S1x4AnyTrue, Umaxv, kFormatS, 4S);
      SIMD_REDUCE_OP_CASE(kArm64S1x4AllTrue, Uminv, kFormatS, 4S);
      SIMD_REDUCE_OP_CASE(kArm64S1x8AnyTrue, Umaxv, kFormatH, 8H);
      SIMD_REDUCE_OP_CASE(kArm64S1x8AllTrue, Uminv, kFormatH, 8H);
      SIMD_REDUCE_OP_CASE(kArm64S1x16AnyTrue, Umaxv, kFormatB, 16B);
      SIMD_REDUCE_OP_CASE(kArm64S1x16AllTrue, Uminv, kFormatB, 16B);
  }
  return kSuccess;
}  // NOLINT(readability/fn_size)

#undef SIMD_UNOP_CASE
#undef SIMD_WIDENING_UNOP_CASE
#undef SIMD_BINOP_CASE
#undef SIMD_REDUCE_OP_CASE

// Assemble branches after this instruction.
void CodeGenerator::AssembleArchBranch(Instruction* instr, BranchInfo* branch) {
  Arm64OperandConverter i(this, instr);
  Label* tlabel = branch->true_label;
  Label* flabel = branch->false_label;
  FlagsCondition condition = branch->condition;
  ArchOpcode opcode = instr->arch_opcode();

  if (opcode == kArm64CompareAndBranch32) {
    DCHECK(FlagsModeField::decode(instr->opcode()) != kFlags_branch_and_poison);
    switch (condition) {
      case kEqual:
        __ Cbz(i.InputRegister32(0), tlabel);
        break;
      case kNotEqual:
        __ Cbnz(i.InputRegister32(0), tlabel);
        break;
      default:
        UNREACHABLE();
    }
  } else if (opcode == kArm64CompareAndBranch) {
    DCHECK(FlagsModeField::decode(instr->opcode()) != kFlags_branch_and_poison);
    switch (condition) {
      case kEqual:
        __ Cbz(i.InputRegister64(0), tlabel);
        break;
      case kNotEqual:
        __ Cbnz(i.InputRegister64(0), tlabel);
        break;
      default:
        UNREACHABLE();
    }
  } else if (opcode == kArm64TestAndBranch32) {
    DCHECK(FlagsModeField::decode(instr->opcode()) != kFlags_branch_and_poison);
    switch (condition) {
      case kEqual:
        __ Tbz(i.InputRegister32(0), i.InputInt5(1), tlabel);
        break;
      case kNotEqual:
        __ Tbnz(i.InputRegister32(0), i.InputInt5(1), tlabel);
        break;
      default:
        UNREACHABLE();
    }
  } else if (opcode == kArm64TestAndBranch) {
    DCHECK(FlagsModeField::decode(instr->opcode()) != kFlags_branch_and_poison);
    switch (condition) {
      case kEqual:
        __ Tbz(i.InputRegister64(0), i.InputInt6(1), tlabel);
        break;
      case kNotEqual:
        __ Tbnz(i.InputRegister64(0), i.InputInt6(1), tlabel);
        break;
      default:
        UNREACHABLE();
    }
  } else {
    Condition cc = FlagsConditionToCondition(condition);
    __ B(cc, tlabel);
  }
  if (!branch->fallthru) __ B(flabel);  // no fallthru to flabel.
}

void CodeGenerator::AssembleBranchPoisoning(FlagsCondition condition,
                                            Instruction* instr) {
  // TODO(jarin) Handle float comparisons (kUnordered[Not]Equal).
  if (condition == kUnorderedEqual || condition == kUnorderedNotEqual) {
    return;
  }

  condition = NegateFlagsCondition(condition);
  __ CmovX(kSpeculationPoisonRegister, xzr,
           FlagsConditionToCondition(condition));
  __ Csdb();
}

void CodeGenerator::AssembleArchDeoptBranch(Instruction* instr,
                                            BranchInfo* branch) {
  AssembleArchBranch(instr, branch);
}

void CodeGenerator::AssembleArchJump(RpoNumber target) {
  if (!IsNextInAssemblyOrder(target)) __ B(GetLabel(target));
}

void CodeGenerator::AssembleArchTrap(Instruction* instr,
                                     FlagsCondition condition) {
  class OutOfLineTrap final : public OutOfLineCode {
   public:
    OutOfLineTrap(CodeGenerator* gen, Instruction* instr)
        : OutOfLineCode(gen), instr_(instr), gen_(gen) {}
    void Generate() final {
      Arm64OperandConverter i(gen_, instr_);
      TrapId trap_id =
          static_cast<TrapId>(i.InputInt32(instr_->InputCount() - 1));
      GenerateCallToTrap(trap_id);
    }

   private:
    void GenerateCallToTrap(TrapId trap_id) {
      if (trap_id == TrapId::kInvalid) {
        // We cannot test calls to the runtime in cctest/test-run-wasm.
        // Therefore we emit a call to C here instead of a call to the runtime.
        __ CallCFunction(
            ExternalReference::wasm_call_trap_callback_for_testing(), 0);
        __ LeaveFrame(StackFrame::WASM_COMPILED);
        auto call_descriptor = gen_->linkage()->GetIncomingDescriptor();
        int pop_count =
            static_cast<int>(call_descriptor->StackParameterCount());
        pop_count += (pop_count & 1);  // align
        __ Drop(pop_count);
        __ Ret();
      } else {
        gen_->AssembleSourcePosition(instr_);
        // A direct call to a wasm runtime stub defined in this module.
        // Just encode the stub index. This will be patched when the code
        // is added to the native module and copied into wasm code space.
        __ Call(static_cast<Address>(trap_id), RelocInfo::WASM_STUB_CALL);
        ReferenceMap* reference_map =
            new (gen_->zone()) ReferenceMap(gen_->zone());
        gen_->RecordSafepoint(reference_map, Safepoint::kSimple,
                              Safepoint::kNoLazyDeopt);
        if (FLAG_debug_code) {
          // The trap code should never return.
          __ Brk(0);
        }
      }
    }
    Instruction* instr_;
    CodeGenerator* gen_;
  };
  auto ool = new (zone()) OutOfLineTrap(this, instr);
  Label* tlabel = ool->entry();
  Condition cc = FlagsConditionToCondition(condition);
  __ B(cc, tlabel);
}

// Assemble boolean materializations after this instruction.
void CodeGenerator::AssembleArchBoolean(Instruction* instr,
                                        FlagsCondition condition) {
  Arm64OperandConverter i(this, instr);

  // Materialize a full 64-bit 1 or 0 value. The result register is always the
  // last output of the instruction.
  DCHECK_NE(0u, instr->OutputCount());
  Register reg = i.OutputRegister(instr->OutputCount() - 1);
  Condition cc = FlagsConditionToCondition(condition);
  __ Cset(reg, cc);
}

void CodeGenerator::AssembleArchBinarySearchSwitch(Instruction* instr) {
  Arm64OperandConverter i(this, instr);
  Register input = i.InputRegister32(0);
  std::vector<std::pair<int32_t, Label*>> cases;
  for (size_t index = 2; index < instr->InputCount(); index += 2) {
    cases.push_back({i.InputInt32(index + 0), GetLabel(i.InputRpo(index + 1))});
  }
  AssembleArchBinarySearchSwitchRange(input, i.InputRpo(1), cases.data(),
                                      cases.data() + cases.size());
}

void CodeGenerator::AssembleArchLookupSwitch(Instruction* instr) {
  Arm64OperandConverter i(this, instr);
  Register input = i.InputRegister32(0);
  for (size_t index = 2; index < instr->InputCount(); index += 2) {
    __ Cmp(input, i.InputInt32(index + 0));
    __ B(eq, GetLabel(i.InputRpo(index + 1)));
  }
  AssembleArchJump(i.InputRpo(1));
}

void CodeGenerator::AssembleArchTableSwitch(Instruction* instr) {
  Arm64OperandConverter i(this, instr);
  UseScratchRegisterScope scope(tasm());
  Register input = i.InputRegister32(0);
  Register temp = scope.AcquireX();
  size_t const case_count = instr->InputCount() - 2;
  Label table;
  __ Cmp(input, case_count);
  __ B(hs, GetLabel(i.InputRpo(1)));
  __ Adr(temp, &table);
  __ Add(temp, temp, Operand(input, UXTW, 2));
  __ Br(temp);
  __ StartBlockPools();
  __ Bind(&table);
  for (size_t index = 0; index < case_count; ++index) {
    __ B(GetLabel(i.InputRpo(index + 2)));
  }
  __ EndBlockPools();
}

void CodeGenerator::FinishFrame(Frame* frame) {
  frame->AlignFrame(16);
  auto call_descriptor = linkage()->GetIncomingDescriptor();

  // Save FP registers.
  CPURegList saves_fp = CPURegList(CPURegister::kVRegister, kDRegSizeInBits,
                                   call_descriptor->CalleeSavedFPRegisters());
  int saved_count = saves_fp.Count();
  if (saved_count != 0) {
    DCHECK(saves_fp.list() == CPURegList::GetCalleeSavedV().list());
    DCHECK_EQ(saved_count % 2, 0);
    frame->AllocateSavedCalleeRegisterSlots(saved_count *
                                            (kDoubleSize / kSystemPointerSize));
  }

  CPURegList saves = CPURegList(CPURegister::kRegister, kXRegSizeInBits,
                                call_descriptor->CalleeSavedRegisters());
  saved_count = saves.Count();
  if (saved_count != 0) {
    DCHECK_EQ(saved_count % 2, 0);
    frame->AllocateSavedCalleeRegisterSlots(saved_count);
  }
}

void CodeGenerator::AssembleConstructFrame() {
  auto call_descriptor = linkage()->GetIncomingDescriptor();
  __ AssertSpAligned();

  // The frame has been previously padded in CodeGenerator::FinishFrame().
  DCHECK_EQ(frame()->GetTotalFrameSlotCount() % 2, 0);
  int required_slots = frame()->GetTotalFrameSlotCount() -
                       call_descriptor->CalculateFixedFrameSize();

  CPURegList saves = CPURegList(CPURegister::kRegister, kXRegSizeInBits,
                                call_descriptor->CalleeSavedRegisters());
  CPURegList saves_fp = CPURegList(CPURegister::kVRegister, kDRegSizeInBits,
                                   call_descriptor->CalleeSavedFPRegisters());
  // The number of slots for returns has to be even to ensure the correct stack
  // alignment.
  const int returns = RoundUp(frame()->GetReturnSlotCount(), 2);

  if (frame_access_state()->has_frame()) {
    // Link the frame
    if (call_descriptor->IsJSFunctionCall()) {
      __ Prologue();
    } else {
      __ Push(lr, fp);
      __ Mov(fp, sp);
    }
    unwinding_info_writer_.MarkFrameConstructed(__ pc_offset());

    // Create OSR entry if applicable
    if (info()->is_osr()) {
      // TurboFan OSR-compiled functions cannot be entered directly.
      __ Abort(AbortReason::kShouldNotDirectlyEnterOsrFunction);

      // Unoptimized code jumps directly to this entrypoint while the
      // unoptimized frame is still on the stack. Optimized code uses OSR values
      // directly from the unoptimized frame. Thus, all that needs to be done is
      // to allocate the remaining stack slots.
      if (FLAG_code_comments) __ RecordComment("-- OSR entrypoint --");
      osr_pc_offset_ = __ pc_offset();
      required_slots -= osr_helper()->UnoptimizedFrameSlots();
      ResetSpeculationPoison();
    }

    if (info()->IsWasm() && required_slots > 128) {
      // For WebAssembly functions with big frames we have to do the stack
      // overflow check before we construct the frame. Otherwise we may not
      // have enough space on the stack to call the runtime for the stack
      // overflow.
      Label done;
      // If the frame is bigger than the stack, we throw the stack overflow
      // exception unconditionally. Thereby we can avoid the integer overflow
      // check in the condition code.
      if (required_slots * kSystemPointerSize < FLAG_stack_size * 1024) {
        UseScratchRegisterScope scope(tasm());
        Register scratch = scope.AcquireX();
        __ Ldr(scratch, FieldMemOperand(
                            kWasmInstanceRegister,
                            WasmInstanceObject::kRealStackLimitAddressOffset));
        __ Ldr(scratch, MemOperand(scratch));
        __ Add(scratch, scratch, required_slots * kSystemPointerSize);
        __ Cmp(sp, scratch);
        __ B(hs, &done);
      }

      {
        // Finish the frame that hasn't been fully built yet.
        UseScratchRegisterScope temps(tasm());
        __ Claim(2);  // Claim extra slots for marker + instance.
        Register scratch = temps.AcquireX();
        __ Mov(scratch,
               StackFrame::TypeToMarker(info()->GetOutputStackFrameType()));
        __ Str(scratch, MemOperand(fp, TypedFrameConstants::kFrameTypeOffset));
        __ Str(kWasmInstanceRegister,
               MemOperand(fp, WasmCompiledFrameConstants::kWasmInstanceOffset));
      }

      __ Call(wasm::WasmCode::kWasmStackOverflow, RelocInfo::WASM_STUB_CALL);
      // We come from WebAssembly, there are no references for the GC.
      ReferenceMap* reference_map = new (zone()) ReferenceMap(zone());
      RecordSafepoint(reference_map, Safepoint::kSimple,
                      Safepoint::kNoLazyDeopt);
      if (FLAG_debug_code) {
        __ Brk(0);
      }
      __ Bind(&done);
    }

    // Skip callee-saved slots, which are pushed below.
    required_slots -= saves.Count();
    required_slots -= saves_fp.Count();
    required_slots -= returns;

    // Build remainder of frame, including accounting for and filling-in
    // frame-specific header information, i.e. claiming the extra slot that
    // other platforms explicitly push for STUB (code object) frames and frames
    // recording their argument count.
    switch (call_descriptor->kind()) {
      case CallDescriptor::kCallJSFunction:
        if (call_descriptor->PushArgumentCount()) {
          __ Claim(required_slots + 1);  // Claim extra slot for argc.
          __ Str(kJavaScriptCallArgCountRegister,
                 MemOperand(fp, OptimizedBuiltinFrameConstants::kArgCOffset));
        } else {
          __ Claim(required_slots);
        }
        break;
      case CallDescriptor::kCallCodeObject: {
        UseScratchRegisterScope temps(tasm());
        __ Claim(required_slots +
                 1);  // Claim extra slot for frame type marker.
        Register scratch = temps.AcquireX();
        __ Mov(scratch,
               StackFrame::TypeToMarker(info()->GetOutputStackFrameType()));
        __ Str(scratch, MemOperand(fp, TypedFrameConstants::kFrameTypeOffset));
      } break;
      case CallDescriptor::kCallWasmFunction: {
        UseScratchRegisterScope temps(tasm());
        __ Claim(required_slots +
                 2);  // Claim extra slots for marker + instance.
        Register scratch = temps.AcquireX();
        __ Mov(scratch,
               StackFrame::TypeToMarker(info()->GetOutputStackFrameType()));
        __ Str(scratch, MemOperand(fp, TypedFrameConstants::kFrameTypeOffset));
        __ Str(kWasmInstanceRegister,
               MemOperand(fp, WasmCompiledFrameConstants::kWasmInstanceOffset));
      } break;
      case CallDescriptor::kCallWasmImportWrapper: {
        UseScratchRegisterScope temps(tasm());
        __ LoadTaggedPointerField(
            kJSFunctionRegister,
            FieldMemOperand(kWasmInstanceRegister, Tuple2::kValue2Offset));
        __ LoadTaggedPointerField(
            kWasmInstanceRegister,
            FieldMemOperand(kWasmInstanceRegister, Tuple2::kValue1Offset));
        __ Claim(required_slots +
                 2);  // Claim extra slots for marker + instance.
        Register scratch = temps.AcquireX();
        __ Mov(scratch,
               StackFrame::TypeToMarker(info()->GetOutputStackFrameType()));
        __ Str(scratch, MemOperand(fp, TypedFrameConstants::kFrameTypeOffset));
        __ Str(kWasmInstanceRegister,
               MemOperand(fp, WasmCompiledFrameConstants::kWasmInstanceOffset));
      } break;
      case CallDescriptor::kCallAddress:
        __ Claim(required_slots);
        break;
      default:
        UNREACHABLE();
    }
  }

  // Save FP registers.
  DCHECK_IMPLIES(saves_fp.Count() != 0,
                 saves_fp.list() == CPURegList::GetCalleeSavedV().list());
  __ PushCPURegList(saves_fp);

  // Save registers.
  // TODO(palfia): TF save list is not in sync with
  // CPURegList::GetCalleeSaved(): x30 is missing.
  // DCHECK(saves.list() == CPURegList::GetCalleeSaved().list());
  __ PushCPURegList(saves);

  if (returns != 0) {
    __ Claim(returns);
  }
}

void CodeGenerator::AssembleReturn(InstructionOperand* pop) {
  auto call_descriptor = linkage()->GetIncomingDescriptor();

  const int returns = RoundUp(frame()->GetReturnSlotCount(), 2);

  if (returns != 0) {
    __ Drop(returns);
  }

  // Restore registers.
  CPURegList saves = CPURegList(CPURegister::kRegister, kXRegSizeInBits,
                                call_descriptor->CalleeSavedRegisters());
  __ PopCPURegList(saves);

  // Restore fp registers.
  CPURegList saves_fp = CPURegList(CPURegister::kVRegister, kDRegSizeInBits,
                                   call_descriptor->CalleeSavedFPRegisters());
  __ PopCPURegList(saves_fp);

  unwinding_info_writer_.MarkBlockWillExit();

  Arm64OperandConverter g(this, nullptr);
  int pop_count = static_cast<int>(call_descriptor->StackParameterCount());
  if (call_descriptor->IsCFunctionCall()) {
    AssembleDeconstructFrame();
  } else if (frame_access_state()->has_frame()) {
    // Canonicalize JSFunction return sites for now unless they have an variable
    // number of stack slot pops.
    if (pop->IsImmediate() && g.ToConstant(pop).ToInt32() == 0) {
      if (return_label_.is_bound()) {
        __ B(&return_label_);
        return;
      } else {
        __ Bind(&return_label_);
        AssembleDeconstructFrame();
      }
    } else {
      AssembleDeconstructFrame();
    }
  }

  if (pop->IsImmediate()) {
    pop_count += g.ToConstant(pop).ToInt32();
    __ DropArguments(pop_count);
  } else {
    Register pop_reg = g.ToRegister(pop);
    __ Add(pop_reg, pop_reg, pop_count);
    __ DropArguments(pop_reg);
  }

  __ AssertSpAligned();
  __ Ret();
}

void CodeGenerator::FinishCode() { __ CheckConstPool(true, false); }

void CodeGenerator::AssembleMove(InstructionOperand* source,
                                 InstructionOperand* destination) {
  Arm64OperandConverter g(this, nullptr);
  // Helper function to write the given constant to the dst register.
  auto MoveConstantToRegister = [&](Register dst, Constant src) {
    if (src.type() == Constant::kHeapObject) {
      Handle<HeapObject> src_object = src.ToHeapObject();
      RootIndex index;
      if (IsMaterializableFromRoot(src_object, &index)) {
        __ LoadRoot(dst, index);
      } else {
        __ Mov(dst, src_object);
      }
    } else {
      __ Mov(dst, g.ToImmediate(source));
    }
  };
  switch (MoveType::InferMove(source, destination)) {
    case MoveType::kRegisterToRegister:
      if (source->IsRegister()) {
        __ Mov(g.ToRegister(destination), g.ToRegister(source));
      } else if (source->IsFloatRegister() || source->IsDoubleRegister()) {
        __ Mov(g.ToDoubleRegister(destination), g.ToDoubleRegister(source));
      } else {
        DCHECK(source->IsSimd128Register());
        __ Mov(g.ToDoubleRegister(destination).Q(),
               g.ToDoubleRegister(source).Q());
      }
      return;
    case MoveType::kRegisterToStack: {
      MemOperand dst = g.ToMemOperand(destination, tasm());
      if (source->IsRegister()) {
        __ Str(g.ToRegister(source), dst);
      } else {
        VRegister src = g.ToDoubleRegister(source);
        if (source->IsFloatRegister() || source->IsDoubleRegister()) {
          __ Str(src, dst);
        } else {
          DCHECK(source->IsSimd128Register());
          __ Str(src.Q(), dst);
        }
      }
      return;
    }
    case MoveType::kStackToRegister: {
      MemOperand src = g.ToMemOperand(source, tasm());
      if (destination->IsRegister()) {
        __ Ldr(g.ToRegister(destination), src);
      } else {
        VRegister dst = g.ToDoubleRegister(destination);
        if (destination->IsFloatRegister() || destination->IsDoubleRegister()) {
          __ Ldr(dst, src);
        } else {
          DCHECK(destination->IsSimd128Register());
          __ Ldr(dst.Q(), src);
        }
      }
      return;
    }
    case MoveType::kStackToStack: {
      MemOperand src = g.ToMemOperand(source, tasm());
      MemOperand dst = g.ToMemOperand(destination, tasm());
      if (source->IsSimd128StackSlot()) {
        UseScratchRegisterScope scope(tasm());
        VRegister temp = scope.AcquireQ();
        __ Ldr(temp, src);
        __ Str(temp, dst);
      } else {
        UseScratchRegisterScope scope(tasm());
        Register temp = scope.AcquireX();
        __ Ldr(temp, src);
        __ Str(temp, dst);
      }
      return;
    }
    case MoveType::kConstantToRegister: {
      Constant src = g.ToConstant(source);
      if (destination->IsRegister()) {
        MoveConstantToRegister(g.ToRegister(destination), src);
      } else {
        VRegister dst = g.ToDoubleRegister(destination);
        if (destination->IsFloatRegister()) {
          __ Fmov(dst.S(), src.ToFloat32());
        } else {
          DCHECK(destination->IsDoubleRegister());
          __ Fmov(dst, src.ToFloat64().value());
        }
      }
      return;
    }
    case MoveType::kConstantToStack: {
      Constant src = g.ToConstant(source);
      MemOperand dst = g.ToMemOperand(destination, tasm());
      if (destination->IsStackSlot()) {
        UseScratchRegisterScope scope(tasm());
        Register temp = scope.AcquireX();
        MoveConstantToRegister(temp, src);
        __ Str(temp, dst);
      } else if (destination->IsFloatStackSlot()) {
        if (bit_cast<int32_t>(src.ToFloat32()) == 0) {
          __ Str(wzr, dst);
        } else {
          UseScratchRegisterScope scope(tasm());
          VRegister temp = scope.AcquireS();
          __ Fmov(temp, src.ToFloat32());
          __ Str(temp, dst);
        }
      } else {
        DCHECK(destination->IsDoubleStackSlot());
        if (src.ToFloat64().AsUint64() == 0) {
          __ Str(xzr, dst);
        } else {
          UseScratchRegisterScope scope(tasm());
          VRegister temp = scope.AcquireD();
          __ Fmov(temp, src.ToFloat64().value());
          __ Str(temp, dst);
        }
      }
      return;
    }
  }
  UNREACHABLE();
}

void CodeGenerator::AssembleSwap(InstructionOperand* source,
                                 InstructionOperand* destination) {
  Arm64OperandConverter g(this, nullptr);
  switch (MoveType::InferSwap(source, destination)) {
    case MoveType::kRegisterToRegister:
      if (source->IsRegister()) {
        __ Swap(g.ToRegister(source), g.ToRegister(destination));
      } else {
        VRegister src = g.ToDoubleRegister(source);
        VRegister dst = g.ToDoubleRegister(destination);
        if (source->IsFloatRegister() || source->IsDoubleRegister()) {
          __ Swap(src, dst);
        } else {
          DCHECK(source->IsSimd128Register());
          __ Swap(src.Q(), dst.Q());
        }
      }
      return;
    case MoveType::kRegisterToStack: {
      UseScratchRegisterScope scope(tasm());
      MemOperand dst = g.ToMemOperand(destination, tasm());
      if (source->IsRegister()) {
        Register temp = scope.AcquireX();
        Register src = g.ToRegister(source);
        __ Mov(temp, src);
        __ Ldr(src, dst);
        __ Str(temp, dst);
      } else {
        UseScratchRegisterScope scope(tasm());
        VRegister src = g.ToDoubleRegister(source);
        if (source->IsFloatRegister() || source->IsDoubleRegister()) {
          VRegister temp = scope.AcquireD();
          __ Mov(temp, src);
          __ Ldr(src, dst);
          __ Str(temp, dst);
        } else {
          DCHECK(source->IsSimd128Register());
          VRegister temp = scope.AcquireQ();
          __ Mov(temp, src.Q());
          __ Ldr(src.Q(), dst);
          __ Str(temp, dst);
        }
      }
      return;
    }
    case MoveType::kStackToStack: {
      UseScratchRegisterScope scope(tasm());
      MemOperand src = g.ToMemOperand(source, tasm());
      MemOperand dst = g.ToMemOperand(destination, tasm());
      VRegister temp_0 = scope.AcquireD();
      VRegister temp_1 = scope.AcquireD();
      if (source->IsSimd128StackSlot()) {
        __ Ldr(temp_0.Q(), src);
        __ Ldr(temp_1.Q(), dst);
        __ Str(temp_0.Q(), dst);
        __ Str(temp_1.Q(), src);
      } else {
        __ Ldr(temp_0, src);
        __ Ldr(temp_1, dst);
        __ Str(temp_0, dst);
        __ Str(temp_1, src);
      }
      return;
    }
    default:
      UNREACHABLE();
      break;
  }
}

void CodeGenerator::AssembleJumpTable(Label** targets, size_t target_count) {
  // On 64-bit ARM we emit the jump tables inline.
  UNREACHABLE();
}

#undef __

}  // namespace compiler
}  // namespace internal
}  // namespace v8
