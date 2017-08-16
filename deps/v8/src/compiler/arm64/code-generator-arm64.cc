// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/code-generator.h"

#include "src/arm64/assembler-arm64-inl.h"
#include "src/arm64/frames-arm64.h"
#include "src/arm64/macro-assembler-arm64-inl.h"
#include "src/compilation-info.h"
#include "src/compiler/code-generator-impl.h"
#include "src/compiler/gap-resolver.h"
#include "src/compiler/node-matchers.h"
#include "src/compiler/osr.h"
#include "src/heap/heap-inl.h"

namespace v8 {
namespace internal {
namespace compiler {

#define __ masm()->


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

  CPURegister InputFloat32OrZeroRegister(size_t index) {
    if (instr_->InputAt(index)->IsImmediate()) {
      DCHECK(bit_cast<int32_t>(InputFloat32(index)) == 0);
      return wzr;
    }
    DCHECK(instr_->InputAt(index)->IsFPRegister());
    return InputDoubleRegister(index).S();
  }

  CPURegister InputFloat64OrZeroRegister(size_t index) {
    if (instr_->InputAt(index)->IsImmediate()) {
      DCHECK(bit_cast<int64_t>(InputDouble(index)) == 0);
      return xzr;
    }
    DCHECK(instr_->InputAt(index)->IsDoubleRegister());
    return InputDoubleRegister(index);
  }

  size_t OutputCount() { return instr_->OutputCount(); }

  DoubleRegister OutputFloat32Register() { return OutputDoubleRegister().S(); }

  DoubleRegister OutputFloat64Register() { return OutputDoubleRegister(); }

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

  Operand InputImmediate(size_t index) {
    return ToImmediate(instr_->InputAt(index));
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
        break;
    }
    UNREACHABLE();
    return Operand(-1);
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
        break;
    }
    UNREACHABLE();
    return Operand(-1);
  }

  MemOperand MemoryOperand(size_t* first_index) {
    const size_t index = *first_index;
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
      case kMode_Operand2_R_LSL_I:
        *first_index += 3;
        return MemOperand(InputRegister(index + 0), InputRegister(index + 1),
                          LSL, InputInt32(index + 2));
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

  MemOperand MemoryOperand(size_t first_index = 0) {
    return MemoryOperand(&first_index);
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
        if (RelocInfo::IsWasmSizeReference(constant.rmode())) {
          return Operand(constant.ToInt32(), constant.rmode());
        } else {
          return Operand(constant.ToInt32());
        }
      case Constant::kInt64:
        if (RelocInfo::IsWasmPtrReference(constant.rmode())) {
          return Operand(constant.ToInt64(), constant.rmode());
        } else {
          DCHECK(!RelocInfo::IsWasmSizeReference(constant.rmode()));
          return Operand(constant.ToInt64());
        }
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
      case Constant::kRpoNumber:
        UNREACHABLE();  // TODO(dcarney): RPO immediates on arm64.
        break;
    }
    UNREACHABLE();
    return Operand(-1);
  }

  MemOperand ToMemOperand(InstructionOperand* op, MacroAssembler* masm) const {
    DCHECK_NOT_NULL(op);
    DCHECK(op->IsStackSlot() || op->IsFPStackSlot());
    return SlotToMemOperand(AllocatedOperand::cast(op)->index(), masm);
  }

  MemOperand SlotToMemOperand(int slot, MacroAssembler* masm) const {
    FrameOffset offset = frame_access_state()->GetFrameOffset(slot);
    if (offset.from_frame_pointer()) {
      int from_sp = offset.offset() + frame_access_state()->GetSPToFPOffset();
      // Convert FP-offsets to SP-offsets if it results in better code.
      if (Assembler::IsImmLSUnscaled(from_sp) ||
          Assembler::IsImmLSScaled(from_sp, LSDoubleWord)) {
        offset = FrameOffset::FromStackPointer(from_sp);
      }
    }
    return MemOperand(offset.from_stack_pointer() ? masm->StackPointer() : fp,
                      offset.offset());
  }
};


namespace {

class OutOfLineLoadNaN32 final : public OutOfLineCode {
 public:
  OutOfLineLoadNaN32(CodeGenerator* gen, DoubleRegister result)
      : OutOfLineCode(gen), result_(result) {}

  void Generate() final {
    __ Fmov(result_, std::numeric_limits<float>::quiet_NaN());
  }

 private:
  DoubleRegister const result_;
};


class OutOfLineLoadNaN64 final : public OutOfLineCode {
 public:
  OutOfLineLoadNaN64(CodeGenerator* gen, DoubleRegister result)
      : OutOfLineCode(gen), result_(result) {}

  void Generate() final {
    __ Fmov(result_, std::numeric_limits<double>::quiet_NaN());
  }

 private:
  DoubleRegister const result_;
};


class OutOfLineLoadZero final : public OutOfLineCode {
 public:
  OutOfLineLoadZero(CodeGenerator* gen, Register result)
      : OutOfLineCode(gen), result_(result) {}

  void Generate() final { __ Mov(result_, 0); }

 private:
  Register const result_;
};


class OutOfLineRecordWrite final : public OutOfLineCode {
 public:
  OutOfLineRecordWrite(CodeGenerator* gen, Register object, Operand index,
                       Register value, Register scratch0, Register scratch1,
                       RecordWriteMode mode,
                       UnwindingInfoWriter* unwinding_info_writer)
      : OutOfLineCode(gen),
        object_(object),
        index_(index),
        value_(value),
        scratch0_(scratch0),
        scratch1_(scratch1),
        mode_(mode),
        must_save_lr_(!gen->frame_access_state()->has_frame()),
        unwinding_info_writer_(unwinding_info_writer) {}

  void Generate() final {
    if (mode_ > RecordWriteMode::kValueIsPointer) {
      __ JumpIfSmi(value_, exit());
    }
    __ CheckPageFlagClear(value_, scratch0_,
                          MemoryChunk::kPointersToHereAreInterestingMask,
                          exit());
    RememberedSetAction const remembered_set_action =
        mode_ > RecordWriteMode::kValueIsMap ? EMIT_REMEMBERED_SET
                                             : OMIT_REMEMBERED_SET;
    SaveFPRegsMode const save_fp_mode =
        frame()->DidAllocateDoubleRegisters() ? kSaveFPRegs : kDontSaveFPRegs;
    if (must_save_lr_) {
      // We need to save and restore lr if the frame was elided.
      __ Push(lr);
      unwinding_info_writer_->MarkLinkRegisterOnTopOfStack(__ pc_offset(),
                                                           __ StackPointer());
    }
    RecordWriteStub stub(isolate(), object_, scratch0_, scratch1_,
                         remembered_set_action, save_fp_mode);
    __ Add(scratch1_, object_, index_);
    __ CallStub(&stub);
    if (must_save_lr_) {
      __ Pop(lr);
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
  bool must_save_lr_;
  UnwindingInfoWriter* const unwinding_info_writer_;
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
  return nv;
}

}  // namespace

#define ASSEMBLE_BOUNDS_CHECK(offset, length, out_of_bounds)   \
  do {                                                         \
    if (length.IsImmediate() &&                                \
        base::bits::IsPowerOfTwo64(length.ImmediateValue())) { \
      __ Tst(offset, ~(length.ImmediateValue() - 1));          \
      __ B(ne, out_of_bounds);                                 \
    } else {                                                   \
      __ Cmp(offset, length);                                  \
      __ B(hs, out_of_bounds);                                 \
    }                                                          \
  } while (0)

#define ASSEMBLE_CHECKED_LOAD_FLOAT(width)                         \
  do {                                                             \
    auto result = i.OutputFloat##width##Register();                \
    auto buffer = i.InputRegister(0);                              \
    auto offset = i.InputRegister32(1);                            \
    auto length = i.InputOperand32(2);                             \
    auto ool = new (zone()) OutOfLineLoadNaN##width(this, result); \
    ASSEMBLE_BOUNDS_CHECK(offset, length, ool->entry());           \
    __ Ldr(result, MemOperand(buffer, offset, UXTW));              \
    __ Bind(ool->exit());                                          \
  } while (0)

#define ASSEMBLE_CHECKED_LOAD_INTEGER(asm_instr)             \
  do {                                                       \
    auto result = i.OutputRegister32();                      \
    auto buffer = i.InputRegister(0);                        \
    auto offset = i.InputRegister32(1);                      \
    auto length = i.InputOperand32(2);                       \
    auto ool = new (zone()) OutOfLineLoadZero(this, result); \
    ASSEMBLE_BOUNDS_CHECK(offset, length, ool->entry());     \
    __ asm_instr(result, MemOperand(buffer, offset, UXTW));  \
    __ Bind(ool->exit());                                    \
  } while (0)

#define ASSEMBLE_CHECKED_LOAD_INTEGER_64(asm_instr)          \
  do {                                                       \
    auto result = i.OutputRegister();                        \
    auto buffer = i.InputRegister(0);                        \
    auto offset = i.InputRegister32(1);                      \
    auto length = i.InputOperand32(2);                       \
    auto ool = new (zone()) OutOfLineLoadZero(this, result); \
    ASSEMBLE_BOUNDS_CHECK(offset, length, ool->entry());     \
    __ asm_instr(result, MemOperand(buffer, offset, UXTW));  \
    __ Bind(ool->exit());                                    \
  } while (0)

#define ASSEMBLE_CHECKED_STORE_FLOAT(width)              \
  do {                                                   \
    auto buffer = i.InputRegister(0);                    \
    auto offset = i.InputRegister32(1);                  \
    auto length = i.InputOperand32(2);                   \
    auto value = i.InputFloat##width##OrZeroRegister(3); \
    Label done;                                          \
    ASSEMBLE_BOUNDS_CHECK(offset, length, &done);        \
    __ Str(value, MemOperand(buffer, offset, UXTW));     \
    __ Bind(&done);                                      \
  } while (0)

#define ASSEMBLE_CHECKED_STORE_INTEGER(asm_instr)          \
  do {                                                     \
    auto buffer = i.InputRegister(0);                      \
    auto offset = i.InputRegister32(1);                    \
    auto length = i.InputOperand32(2);                     \
    auto value = i.InputOrZeroRegister32(3);               \
    Label done;                                            \
    ASSEMBLE_BOUNDS_CHECK(offset, length, &done);          \
    __ asm_instr(value, MemOperand(buffer, offset, UXTW)); \
    __ Bind(&done);                                        \
  } while (0)

#define ASSEMBLE_CHECKED_STORE_INTEGER_64(asm_instr)       \
  do {                                                     \
    auto buffer = i.InputRegister(0);                      \
    auto offset = i.InputRegister32(1);                    \
    auto length = i.InputOperand32(2);                     \
    auto value = i.InputOrZeroRegister64(3);               \
    Label done;                                            \
    ASSEMBLE_BOUNDS_CHECK(offset, length, &done);          \
    __ asm_instr(value, MemOperand(buffer, offset, UXTW)); \
    __ Bind(&done);                                        \
  } while (0)

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

#define ASSEMBLE_ATOMIC_LOAD_INTEGER(asm_instr)                        \
  do {                                                                 \
    __ Add(i.TempRegister(0), i.InputRegister(0), i.InputRegister(1)); \
    __ asm_instr(i.OutputRegister32(), i.TempRegister(0));             \
  } while (0)

#define ASSEMBLE_ATOMIC_STORE_INTEGER(asm_instr)                       \
  do {                                                                 \
    __ Add(i.TempRegister(0), i.InputRegister(0), i.InputRegister(1)); \
    __ asm_instr(i.InputRegister32(2), i.TempRegister(0));             \
  } while (0)

#define ASSEMBLE_ATOMIC_EXCHANGE_INTEGER(load_instr, store_instr)      \
  do {                                                                 \
    Label exchange;                                                    \
    __ bind(&exchange);                                                \
    __ Add(i.TempRegister(0), i.InputRegister(0), i.InputRegister(1)); \
    __ load_instr(i.OutputRegister32(), i.TempRegister(0));            \
    __ store_instr(i.TempRegister32(0), i.InputRegister32(2),          \
                   i.TempRegister(0));                                 \
    __ cbnz(i.TempRegister32(0), &exchange);                           \
  } while (0)

#define ASSEMBLE_ATOMIC_COMPARE_EXCHANGE_INTEGER(load_instr, store_instr) \
  do {                                                                    \
    Label compareExchange;                                                \
    Label exit;                                                           \
    __ bind(&compareExchange);                                            \
    __ Add(i.TempRegister(0), i.InputRegister(0), i.InputRegister(1));    \
    __ load_instr(i.OutputRegister32(), i.TempRegister(0));               \
    __ cmp(i.TempRegister32(1), i.OutputRegister32());                    \
    __ B(ne, &exit);                                                      \
    __ store_instr(i.TempRegister32(0), i.InputRegister32(3),             \
                   i.TempRegister(0));                                    \
    __ cbnz(i.TempRegister32(0), &compareExchange);                       \
    __ bind(&exit);                                                       \
  } while (0)

#define ASSEMBLE_ATOMIC_BINOP(load_instr, store_instr, bin_instr)      \
  do {                                                                 \
    Label binop;                                                       \
    __ Add(i.TempRegister(0), i.InputRegister(0), i.InputRegister(1)); \
    __ bind(&binop);                                                   \
    __ load_instr(i.OutputRegister32(), i.TempRegister(0));            \
    __ bin_instr(i.TempRegister32(1), i.OutputRegister32(),            \
                 Operand(i.InputRegister32(2)));                       \
    __ store_instr(i.TempRegister32(1), i.TempRegister32(1),           \
                   i.TempRegister(0));                                 \
    __ cbnz(i.TempRegister32(1), &binop);                              \
  } while (0)

#define ASSEMBLE_IEEE754_BINOP(name)                                          \
  do {                                                                        \
    FrameScope scope(masm(), StackFrame::MANUAL);                             \
    __ CallCFunction(ExternalReference::ieee754_##name##_function(isolate()), \
                     0, 2);                                                   \
  } while (0)

#define ASSEMBLE_IEEE754_UNOP(name)                                           \
  do {                                                                        \
    FrameScope scope(masm(), StackFrame::MANUAL);                             \
    __ CallCFunction(ExternalReference::ieee754_##name##_function(isolate()), \
                     0, 1);                                                   \
  } while (0)

void CodeGenerator::AssembleDeconstructFrame() {
  const CallDescriptor* descriptor = linkage()->GetIncomingDescriptor();
  if (descriptor->IsCFunctionCall() || descriptor->UseNativeStack()) {
    __ Mov(csp, fp);
  } else {
    __ Mov(jssp, fp);
  }
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

void AdjustStackPointerForTailCall(MacroAssembler* masm,
                                   FrameAccessState* state,
                                   int new_slot_above_sp,
                                   bool allow_shrinkage = true) {
  int current_sp_offset = state->GetSPToFPSlotCount() +
                          StandardFrameConstants::kFixedSlotCountAboveFp;
  int stack_slot_delta = new_slot_above_sp - current_sp_offset;
  if (stack_slot_delta > 0) {
    masm->Claim(stack_slot_delta);
    state->IncreaseSPDelta(stack_slot_delta);
  } else if (allow_shrinkage && stack_slot_delta < 0) {
    masm->Drop(-stack_slot_delta);
    state->IncreaseSPDelta(stack_slot_delta);
  }
}

}  // namespace

void CodeGenerator::AssembleTailCallBeforeGap(Instruction* instr,
                                              int first_unused_stack_slot) {
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
  Arm64OperandConverter i(this, instr);
  InstructionCode opcode = instr->opcode();
  ArchOpcode arch_opcode = ArchOpcodeField::decode(opcode);
  switch (arch_opcode) {
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
      RecordCallPosition(instr);
      // TODO(titzer): this is ugly. JSSP should be a caller-save register
      // in this case, but it is not possible to express in the register
      // allocator.
      CallDescriptor::Flags flags(MiscField::decode(opcode));
      if (flags & CallDescriptor::kRestoreJSSP) {
        __ Ldr(jssp, MemOperand(csp));
        __ Mov(csp, jssp);
      }
      if (flags & CallDescriptor::kRestoreCSP) {
        __ Mov(csp, jssp);
        __ AssertCspAligned();
      }
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
        __ Jump(Handle<Code>::cast(i.InputHeapObject(0)),
                RelocInfo::CODE_TARGET);
      } else {
        Register target = i.InputRegister(0);
        __ Add(target, target, Code::kHeaderSize - kHeapObjectTag);
        __ Jump(target);
      }
      unwinding_info_writer_.MarkBlockWillExit();
      frame_access_state()->ClearSPDelta();
      frame_access_state()->SetFrameAccessToDefault();
      break;
    }
    case kArchTailCallAddress: {
      CHECK(!instr->InputAt(0)->IsImmediate());
      __ Jump(i.InputRegister(0));
      unwinding_info_writer_.MarkBlockWillExit();
      frame_access_state()->ClearSPDelta();
      frame_access_state()->SetFrameAccessToDefault();
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
      RecordCallPosition(instr);
      // TODO(titzer): this is ugly. JSSP should be a caller-save register
      // in this case, but it is not possible to express in the register
      // allocator.
      CallDescriptor::Flags flags(MiscField::decode(opcode));
      if (flags & CallDescriptor::kRestoreJSSP) {
        __ Ldr(jssp, MemOperand(csp));
        __ Mov(csp, jssp);
      }
      if (flags & CallDescriptor::kRestoreCSP) {
        __ Mov(csp, jssp);
        __ AssertCspAligned();
      }
      frame_access_state()->ClearSPDelta();
      break;
    }
    case kArchTailCallJSFunctionFromJSFunction: {
      Register func = i.InputRegister(0);
      if (FLAG_debug_code) {
        // Check the function's context matches the context argument.
        UseScratchRegisterScope scope(masm());
        Register temp = scope.AcquireX();
        __ Ldr(temp, FieldMemOperand(func, JSFunction::kContextOffset));
        __ cmp(cp, temp);
        __ Assert(eq, kWrongFunctionContext);
      }
      AssemblePopArgumentsAdaptorFrame(kJavaScriptCallArgCountRegister,
                                       i.TempRegister(0), i.TempRegister(1),
                                       i.TempRegister(2));
      __ Ldr(x10, FieldMemOperand(func, JSFunction::kCodeEntryOffset));
      __ Jump(x10);
      frame_access_state()->ClearSPDelta();
      frame_access_state()->SetFrameAccessToDefault();
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
      frame_access_state()->ClearSPDelta();
      break;
    }
    case kArchJmp:
      AssembleArchJump(i.InputRpo(0));
      break;
    case kArchTableSwitch:
      AssembleArchTableSwitch(instr);
      break;
    case kArchLookupSwitch:
      AssembleArchLookupSwitch(instr);
      break;
    case kArchDebugBreak:
      __ Debug("kArchDebugBreak", 0, BREAK);
      break;
    case kArchComment: {
      Address comment_string = i.InputExternalReference(0).address();
      __ RecordComment(reinterpret_cast<const char*>(comment_string));
      break;
    }
    case kArchNop:
    case kArchThrowTerminator:
      // don't emit code for nops.
      break;
    case kArchDeoptimize: {
      int deopt_state_id =
          BuildTranslation(instr, -1, 0, OutputFrameStateCombine::Ignore());
      CodeGenResult result =
          AssembleDeoptimizerCall(deopt_state_id, current_source_position_);
      if (result != kSuccess) return result;
      break;
    }
    case kArchRet:
      AssembleReturn(instr->InputAt(0));
      break;
    case kArchStackPointer:
      __ mov(i.OutputRegister(), masm()->StackPointer());
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
      __ TruncateDoubleToI(i.OutputRegister(), i.InputDoubleRegister(0));
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
      auto ool = new (zone())
          OutOfLineRecordWrite(this, object, index, value, scratch0, scratch1,
                               mode, &unwinding_info_writer_);
      __ Str(value, MemOperand(object, index));
      __ CheckPageFlagSet(object, scratch0,
                          MemoryChunk::kPointersFromHereAreInterestingMask,
                          ool->entry());
      __ Bind(ool->exit());
      break;
    }
    case kArchStackSlot: {
      FrameOffset offset =
          frame_access_state()->GetFrameOffset(i.InputInt32(0));
      Register base;
      if (offset.from_stack_pointer()) {
        base = __ StackPointer();
      } else {
        base = fp;
      }
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
    case kIeee754Float64Pow: {
      MathPowStub stub(isolate(), MathPowStub::DOUBLE);
      __ CallStub(&stub);
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
      // Pseudo instruction turned into cbz/cbnz in AssembleArchBranch.
      break;
    case kArm64ClaimCSP: {
      int count = RoundUp(i.InputInt32(0), 2);
      Register prev = __ StackPointer();
      if (prev.Is(jssp)) {
        // TODO(titzer): make this a macro-assembler method.
        // Align the CSP and store the previous JSSP on the stack. We do not
        // need to modify the SP delta here, as we will continue to access the
        // frame via JSSP.
        UseScratchRegisterScope scope(masm());
        Register tmp = scope.AcquireX();

        // TODO(arm64): Storing JSSP on the stack is redundant when calling a C
        // function, as JSSP is callee-saved (we still need to do this when
        // calling a code object that uses the CSP as the stack pointer). See
        // the code generation for kArchCallCodeObject vs. kArchCallCFunction
        // (the latter does not restore CSP/JSSP).
        // MacroAssembler::CallCFunction() (safely) drops this extra slot
        // anyway.
        int sp_alignment = __ ActivationFrameAlignment();
        __ Sub(tmp, jssp, kPointerSize);
        __ Bic(csp, tmp, sp_alignment - 1);
        __ Str(jssp, MemOperand(csp));
        if (count > 0) {
          __ SetStackPointer(csp);
          __ Claim(count);
          __ SetStackPointer(prev);
        }
      } else {
        __ AssertCspAligned();
        if (count > 0) {
          __ Claim(count);
          frame_access_state()->IncreaseSPDelta(count);
        }
      }
      break;
    }
    case kArm64ClaimJSSP: {
      int count = i.InputInt32(0);
      if (csp.Is(__ StackPointer())) {
        // No JSSP is set up. Compute it from the CSP.
        __ AssertCspAligned();
        if (count > 0) {
          int even = RoundUp(count, 2);
          __ Sub(jssp, csp, count * kPointerSize);
          // We must also update CSP to maintain stack consistency:
          __ Sub(csp, csp, even * kPointerSize);  // Must always be aligned.
          __ AssertStackConsistency();
          frame_access_state()->IncreaseSPDelta(even);
        } else {
          __ Mov(jssp, csp);
        }
      } else {
        // JSSP is the current stack pointer, just use regular Claim().
        __ Claim(count);
        frame_access_state()->IncreaseSPDelta(count);
      }
      break;
    }
    case kArm64PokeCSP:  // fall through
    case kArm64PokeJSSP: {
      Register prev = __ StackPointer();
      __ SetStackPointer(arch_opcode == kArm64PokeCSP ? csp : jssp);
      Operand operand(i.InputInt32(1) * kPointerSize);
      if (instr->InputAt(0)->IsFPRegister()) {
        __ Poke(i.InputFloat64Register(0), operand);
      } else {
        __ Poke(i.InputRegister(0), operand);
      }
      __ SetStackPointer(prev);
      break;
    }
    case kArm64PokePair: {
      int slot = i.InputInt32(2) - 1;
      if (instr->InputAt(0)->IsFPRegister()) {
        __ PokePair(i.InputFloat64Register(1), i.InputFloat64Register(0),
                    slot * kPointerSize);
      } else {
        __ PokePair(i.InputRegister(1), i.InputRegister(0),
                    slot * kPointerSize);
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
        DCHECK(i.InputFloat32(1) == 0.0f);
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
        DCHECK(i.InputDouble(1) == 0.0);
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
        __ Mov(i.OutputRegister(1), 1);
        Label done;
        __ Cmp(i.OutputRegister(0), 1);
        __ Ccmp(i.OutputRegister(0), -1, VFlag, vc);
        __ Fccmp(i.InputFloat32Register(0), i.InputFloat32Register(0), VFlag,
                 vc);
        __ B(vc, &done);
        __ Fcmp(i.InputFloat32Register(0), static_cast<float>(INT64_MIN));
        __ Cset(i.OutputRegister(1), eq);
        __ Bind(&done);
      }
      break;
    case kArm64Float64ToInt64:
      __ Fcvtzs(i.OutputRegister(0), i.InputDoubleRegister(0));
      if (i.OutputCount() > 1) {
        __ Mov(i.OutputRegister(1), 1);
        Label done;
        __ Cmp(i.OutputRegister(0), 1);
        __ Ccmp(i.OutputRegister(0), -1, VFlag, vc);
        __ Fccmp(i.InputDoubleRegister(0), i.InputDoubleRegister(0), VFlag, vc);
        __ B(vc, &done);
        __ Fcmp(i.InputDoubleRegister(0), static_cast<double>(INT64_MIN));
        __ Cset(i.OutputRegister(1), eq);
        __ Bind(&done);
      }
      break;
    case kArm64Float32ToUint64:
      __ Fcvtzu(i.OutputRegister64(), i.InputFloat32Register(0));
      if (i.OutputCount() > 1) {
        __ Fcmp(i.InputFloat32Register(0), -1.0);
        __ Ccmp(i.OutputRegister(0), -1, ZFlag, gt);
        __ Cset(i.OutputRegister(1), ne);
      }
      break;
    case kArm64Float64ToUint64:
      __ Fcvtzu(i.OutputRegister64(), i.InputDoubleRegister(0));
      if (i.OutputCount() > 1) {
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
      // TODO(arm64): This should use MOV (to general) when NEON is supported.
      __ Fmov(i.OutputRegister(), i.InputFloat64Register(0));
      __ Lsr(i.OutputRegister(), i.OutputRegister(), 32);
      break;
    case kArm64Float64InsertLowWord32: {
      // TODO(arm64): This should use MOV (from general) when NEON is supported.
      UseScratchRegisterScope scope(masm());
      Register tmp = scope.AcquireX();
      __ Fmov(tmp, i.InputFloat64Register(0));
      __ Bfi(tmp, i.InputRegister(1), 0, 32);
      __ Fmov(i.OutputFloat64Register(), tmp);
      break;
    }
    case kArm64Float64InsertHighWord32: {
      // TODO(arm64): This should use MOV (from general) when NEON is supported.
      UseScratchRegisterScope scope(masm());
      Register tmp = scope.AcquireX();
      __ Fmov(tmp.W(), i.InputFloat32Register(0));
      __ Bfi(tmp, i.InputRegister(1), 32, 32);
      __ Fmov(i.OutputFloat64Register(), tmp);
      break;
    }
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
      break;
    case kArm64Ldrsb:
      __ Ldrsb(i.OutputRegister(), i.MemoryOperand());
      break;
    case kArm64Strb:
      __ Strb(i.InputOrZeroRegister64(0), i.MemoryOperand(1));
      break;
    case kArm64Ldrh:
      __ Ldrh(i.OutputRegister(), i.MemoryOperand());
      break;
    case kArm64Ldrsh:
      __ Ldrsh(i.OutputRegister(), i.MemoryOperand());
      break;
    case kArm64Strh:
      __ Strh(i.InputOrZeroRegister64(0), i.MemoryOperand(1));
      break;
    case kArm64Ldrsw:
      __ Ldrsw(i.OutputRegister(), i.MemoryOperand());
      break;
    case kArm64LdrW:
      __ Ldr(i.OutputRegister32(), i.MemoryOperand());
      break;
    case kArm64StrW:
      __ Str(i.InputOrZeroRegister32(0), i.MemoryOperand(1));
      break;
    case kArm64Ldr:
      __ Ldr(i.OutputRegister(), i.MemoryOperand());
      break;
    case kArm64Str:
      __ Str(i.InputOrZeroRegister64(0), i.MemoryOperand(1));
      break;
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
    case kCheckedLoadInt8:
      ASSEMBLE_CHECKED_LOAD_INTEGER(Ldrsb);
      break;
    case kCheckedLoadUint8:
      ASSEMBLE_CHECKED_LOAD_INTEGER(Ldrb);
      break;
    case kCheckedLoadInt16:
      ASSEMBLE_CHECKED_LOAD_INTEGER(Ldrsh);
      break;
    case kCheckedLoadUint16:
      ASSEMBLE_CHECKED_LOAD_INTEGER(Ldrh);
      break;
    case kCheckedLoadWord32:
      ASSEMBLE_CHECKED_LOAD_INTEGER(Ldr);
      break;
    case kCheckedLoadWord64:
      ASSEMBLE_CHECKED_LOAD_INTEGER_64(Ldr);
      break;
    case kCheckedLoadFloat32:
      ASSEMBLE_CHECKED_LOAD_FLOAT(32);
      break;
    case kCheckedLoadFloat64:
      ASSEMBLE_CHECKED_LOAD_FLOAT(64);
      break;
    case kCheckedStoreWord8:
      ASSEMBLE_CHECKED_STORE_INTEGER(Strb);
      break;
    case kCheckedStoreWord16:
      ASSEMBLE_CHECKED_STORE_INTEGER(Strh);
      break;
    case kCheckedStoreWord32:
      ASSEMBLE_CHECKED_STORE_INTEGER(Str);
      break;
    case kCheckedStoreWord64:
      ASSEMBLE_CHECKED_STORE_INTEGER_64(Str);
      break;
    case kCheckedStoreFloat32:
      ASSEMBLE_CHECKED_STORE_FLOAT(32);
      break;
    case kCheckedStoreFloat64:
      ASSEMBLE_CHECKED_STORE_FLOAT(64);
      break;
    case kAtomicLoadInt8:
      ASSEMBLE_ATOMIC_LOAD_INTEGER(Ldarb);
      __ Sxtb(i.OutputRegister(0), i.OutputRegister(0));
      break;
    case kAtomicLoadUint8:
      ASSEMBLE_ATOMIC_LOAD_INTEGER(Ldarb);
      break;
    case kAtomicLoadInt16:
      ASSEMBLE_ATOMIC_LOAD_INTEGER(Ldarh);
      __ Sxth(i.OutputRegister(0), i.OutputRegister(0));
      break;
    case kAtomicLoadUint16:
      ASSEMBLE_ATOMIC_LOAD_INTEGER(Ldarh);
      break;
    case kAtomicLoadWord32:
      ASSEMBLE_ATOMIC_LOAD_INTEGER(Ldar);
      break;
    case kAtomicStoreWord8:
      ASSEMBLE_ATOMIC_STORE_INTEGER(Stlrb);
      break;
    case kAtomicStoreWord16:
      ASSEMBLE_ATOMIC_STORE_INTEGER(Stlrh);
      break;
    case kAtomicStoreWord32:
      ASSEMBLE_ATOMIC_STORE_INTEGER(Stlr);
      break;
    case kAtomicExchangeInt8:
      ASSEMBLE_ATOMIC_EXCHANGE_INTEGER(ldaxrb, stlxrb);
      __ Sxtb(i.OutputRegister(0), i.OutputRegister(0));
      break;
    case kAtomicExchangeUint8:
      ASSEMBLE_ATOMIC_EXCHANGE_INTEGER(ldaxrb, stlxrb);
      break;
    case kAtomicExchangeInt16:
      ASSEMBLE_ATOMIC_EXCHANGE_INTEGER(ldaxrh, stlxrh);
      __ Sxth(i.OutputRegister(0), i.OutputRegister(0));
      break;
    case kAtomicExchangeUint16:
      ASSEMBLE_ATOMIC_EXCHANGE_INTEGER(ldaxrh, stlxrh);
      break;
    case kAtomicExchangeWord32:
      ASSEMBLE_ATOMIC_EXCHANGE_INTEGER(ldaxr, stlxr);
      break;
    case kAtomicCompareExchangeInt8:
      __ Uxtb(i.TempRegister(1), i.InputRegister(2));
      ASSEMBLE_ATOMIC_COMPARE_EXCHANGE_INTEGER(ldaxrb, stlxrb);
      __ Sxtb(i.OutputRegister(0), i.OutputRegister(0));
      break;
    case kAtomicCompareExchangeUint8:
      __ Uxtb(i.TempRegister(1), i.InputRegister(2));
      ASSEMBLE_ATOMIC_COMPARE_EXCHANGE_INTEGER(ldaxrb, stlxrb);
      break;
    case kAtomicCompareExchangeInt16:
      __ Uxth(i.TempRegister(1), i.InputRegister(2));
      ASSEMBLE_ATOMIC_COMPARE_EXCHANGE_INTEGER(ldaxrh, stlxrh);
      __ Sxth(i.OutputRegister(0), i.OutputRegister(0));
      break;
    case kAtomicCompareExchangeUint16:
      __ Uxth(i.TempRegister(1), i.InputRegister(2));
      ASSEMBLE_ATOMIC_COMPARE_EXCHANGE_INTEGER(ldaxrh, stlxrh);
      break;
    case kAtomicCompareExchangeWord32:
      __ mov(i.TempRegister(1), i.InputRegister(2));
      ASSEMBLE_ATOMIC_COMPARE_EXCHANGE_INTEGER(ldaxr, stlxr);
      break;
#define ATOMIC_BINOP_CASE(op, inst)                    \
  case kAtomic##op##Int8:                              \
    ASSEMBLE_ATOMIC_BINOP(ldaxrb, stlxrb, inst);       \
    __ Sxtb(i.OutputRegister(0), i.OutputRegister(0)); \
    break;                                             \
  case kAtomic##op##Uint8:                             \
    ASSEMBLE_ATOMIC_BINOP(ldaxrb, stlxrb, inst);       \
    break;                                             \
  case kAtomic##op##Int16:                             \
    ASSEMBLE_ATOMIC_BINOP(ldaxrh, stlxrh, inst);       \
    __ Sxth(i.OutputRegister(0), i.OutputRegister(0)); \
    break;                                             \
  case kAtomic##op##Uint16:                            \
    ASSEMBLE_ATOMIC_BINOP(ldaxrh, stlxrh, inst);       \
    break;                                             \
  case kAtomic##op##Word32:                            \
    ASSEMBLE_ATOMIC_BINOP(ldaxr, stlxr, inst);         \
    break;
      ATOMIC_BINOP_CASE(Add, Add)
      ATOMIC_BINOP_CASE(Sub, Sub)
      ATOMIC_BINOP_CASE(And, And)
      ATOMIC_BINOP_CASE(Or, Orr)
      ATOMIC_BINOP_CASE(Xor, Eor)
#undef ATOMIC_BINOP_CASE
  }
  return kSuccess;
}  // NOLINT(readability/fn_size)


// Assemble branches after this instruction.
void CodeGenerator::AssembleArchBranch(Instruction* instr, BranchInfo* branch) {
  Arm64OperandConverter i(this, instr);
  Label* tlabel = branch->true_label;
  Label* flabel = branch->false_label;
  FlagsCondition condition = branch->condition;
  ArchOpcode opcode = instr->arch_opcode();

  if (opcode == kArm64CompareAndBranch32) {
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


void CodeGenerator::AssembleArchJump(RpoNumber target) {
  if (!IsNextInAssemblyOrder(target)) __ B(GetLabel(target));
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
      Arm64OperandConverter i(gen_, instr_);
      Builtins::Name trap_id =
          static_cast<Builtins::Name>(i.InputInt32(instr_->InputCount() - 1));
      bool old_has_frame = __ has_frame();
      if (frame_elided_) {
        __ set_has_frame(true);
        __ EnterFrame(StackFrame::WASM_COMPILED);
      }
      GenerateCallToTrap(trap_id);
      if (frame_elided_) {
        __ set_has_frame(old_has_frame);
      }
    }

   private:
    void GenerateCallToTrap(Builtins::Name trap_id) {
      if (trap_id == Builtins::builtin_count) {
        // We cannot test calls to the runtime in cctest/test-run-wasm.
        // Therefore we emit a call to C here instead of a call to the runtime.
        __ CallCFunction(
            ExternalReference::wasm_call_trap_callback_for_testing(isolate()),
            0);
        __ LeaveFrame(StackFrame::WASM_COMPILED);
        __ Ret();
      } else {
        DCHECK(csp.Is(__ StackPointer()));
        // Initialize the jssp because it is required for the runtime call.
        __ Mov(jssp, csp);
        gen_->AssembleSourcePosition(instr_);
        __ Call(handle(isolate()->builtins()->builtin(trap_id), isolate()),
                RelocInfo::CODE_TARGET);
        ReferenceMap* reference_map =
            new (gen_->zone()) ReferenceMap(gen_->zone());
        gen_->RecordSafepoint(reference_map, Safepoint::kSimple, 0,
                              Safepoint::kNoLazyDeopt);
        if (FLAG_debug_code) {
          // The trap code should never return.
          __ Brk(0);
        }
      }
    }
    bool frame_elided_;
    Instruction* instr_;
    CodeGenerator* gen_;
  };
  bool frame_elided = !frame_access_state()->has_frame();
  auto ool = new (zone()) OutOfLineTrap(this, frame_elided, instr);
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
  UseScratchRegisterScope scope(masm());
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

CodeGenerator::CodeGenResult CodeGenerator::AssembleDeoptimizerCall(
    int deoptimization_id, SourcePosition pos) {
  DeoptimizeKind deoptimization_kind = GetDeoptimizationKind(deoptimization_id);
  DeoptimizeReason deoptimization_reason =
      GetDeoptimizationReason(deoptimization_id);
  Deoptimizer::BailoutType bailout_type =
      deoptimization_kind == DeoptimizeKind::kSoft ? Deoptimizer::SOFT
                                                   : Deoptimizer::EAGER;
  Address deopt_entry = Deoptimizer::GetDeoptimizationEntry(
      isolate(), deoptimization_id, bailout_type);
  if (deopt_entry == nullptr) return kTooManyDeoptimizationBailouts;
  if (isolate()->NeedsSourcePositionsForProfiling()) {
    __ RecordDeoptReason(deoptimization_reason, pos, deoptimization_id);
  }
  __ Call(deopt_entry, RelocInfo::RUNTIME_ENTRY);
  return kSuccess;
}

void CodeGenerator::FinishFrame(Frame* frame) {
  frame->AlignFrame(16);
  CallDescriptor* descriptor = linkage()->GetIncomingDescriptor();

  if (descriptor->UseNativeStack() || descriptor->IsCFunctionCall()) {
    __ SetStackPointer(csp);
  } else {
    __ SetStackPointer(jssp);
  }

  // Save FP registers.
  CPURegList saves_fp = CPURegList(CPURegister::kFPRegister, kDRegSizeInBits,
                                   descriptor->CalleeSavedFPRegisters());
  int saved_count = saves_fp.Count();
  if (saved_count != 0) {
    DCHECK(saves_fp.list() == CPURegList::GetCalleeSavedFP().list());
    frame->AllocateSavedCalleeRegisterSlots(saved_count *
                                            (kDoubleSize / kPointerSize));
  }

  CPURegList saves = CPURegList(CPURegister::kRegister, kXRegSizeInBits,
                                descriptor->CalleeSavedRegisters());
  saved_count = saves.Count();
  if (saved_count != 0) {
    frame->AllocateSavedCalleeRegisterSlots(saved_count);
  }
}

void CodeGenerator::AssembleConstructFrame() {
  CallDescriptor* descriptor = linkage()->GetIncomingDescriptor();
  if (descriptor->UseNativeStack()) {
    __ AssertCspAligned();
  }

  int fixed_frame_size = descriptor->CalculateFixedFrameSize();
  int shrink_slots =
      frame()->GetTotalFrameSlotCount() - descriptor->CalculateFixedFrameSize();

  if (frame_access_state()->has_frame()) {
    // Link the frame
    if (descriptor->IsJSFunctionCall()) {
      DCHECK(!descriptor->UseNativeStack());
      __ Prologue(this->info()->GeneratePreagedPrologue());
    } else {
      __ Push(lr, fp);
      __ Mov(fp, masm_.StackPointer());
    }
    if (!info()->GeneratePreagedPrologue()) {
      unwinding_info_writer_.MarkFrameConstructed(__ pc_offset());
    }

    // Create OSR entry if applicable
    if (info()->is_osr()) {
      // TurboFan OSR-compiled functions cannot be entered directly.
      __ Abort(kShouldNotDirectlyEnterOsrFunction);

      // Unoptimized code jumps directly to this entrypoint while the
      // unoptimized
      // frame is still on the stack. Optimized code uses OSR values directly
      // from
      // the unoptimized frame. Thus, all that needs to be done is to allocate
      // the
      // remaining stack slots.
      if (FLAG_code_comments) __ RecordComment("-- OSR entrypoint --");
      osr_pc_offset_ = __ pc_offset();
      shrink_slots -= OsrHelper(info()).UnoptimizedFrameSlots();
    }

    if (info()->IsWasm() && shrink_slots > 128) {
      // For WebAssembly functions with big frames we have to do the stack
      // overflow check before we construct the frame. Otherwise we may not
      // have enough space on the stack to call the runtime for the stack
      // overflow.
      Label done;
      // If the frame is bigger than the stack, we throw the stack overflow
      // exception unconditionally. Thereby we can avoid the integer overflow
      // check in the condition code.
      if (shrink_slots * kPointerSize < FLAG_stack_size * 1024) {
        UseScratchRegisterScope scope(masm());
        Register scratch = scope.AcquireX();
        __ Mov(
            scratch,
            Operand(ExternalReference::address_of_real_stack_limit(isolate())));
        __ Ldr(scratch, MemOperand(scratch));
        __ Add(scratch, scratch, Operand(shrink_slots * kPointerSize));
        __ Cmp(__ StackPointer(), scratch);
        __ B(cs, &done);
      }

      if (!frame_access_state()->has_frame()) {
        __ set_has_frame(true);
        // There is no need to leave the frame, we will not return from the
        // runtime call.
        __ EnterFrame(StackFrame::WASM_COMPILED);
      }
      DCHECK(__ StackPointer().Is(csp));
      __ SetStackPointer(jssp);
      __ AssertStackConsistency();
      // Initialize the jssp because it is required for the runtime call.
      __ Mov(jssp, csp);
      __ Move(cp, Smi::kZero);
      __ CallRuntime(Runtime::kThrowWasmStackOverflow);
      // We come from WebAssembly, there are no references for the GC.
      ReferenceMap* reference_map = new (zone()) ReferenceMap(zone());
      RecordSafepoint(reference_map, Safepoint::kSimple, 0,
                      Safepoint::kNoLazyDeopt);
      if (FLAG_debug_code) {
        __ Brk(0);
      }
      __ SetStackPointer(csp);
      __ AssertStackConsistency();
      __ bind(&done);
    }

    // Build remainder of frame, including accounting for and filling-in
    // frame-specific header information, e.g. claiming the extra slot that
    // other platforms explicitly push for STUB frames and frames recording
    // their argument count.
    __ Claim(shrink_slots + (fixed_frame_size & 1));
    if (descriptor->PushArgumentCount()) {
      __ Str(kJavaScriptCallArgCountRegister,
             MemOperand(fp, OptimizedBuiltinFrameConstants::kArgCOffset));
    }
    bool is_stub_frame =
        !descriptor->IsJSFunctionCall() && !descriptor->IsCFunctionCall();
    if (is_stub_frame) {
      UseScratchRegisterScope temps(masm());
      Register temp = temps.AcquireX();
      __ Mov(temp, StackFrame::TypeToMarker(info()->GetOutputStackFrameType()));
      __ Str(temp, MemOperand(fp, TypedFrameConstants::kFrameTypeOffset));
    }
  }

  // Save FP registers.
  CPURegList saves_fp = CPURegList(CPURegister::kFPRegister, kDRegSizeInBits,
                                   descriptor->CalleeSavedFPRegisters());
  int saved_count = saves_fp.Count();
  if (saved_count != 0) {
    DCHECK(saves_fp.list() == CPURegList::GetCalleeSavedFP().list());
    __ PushCPURegList(saves_fp);
  }
  // Save registers.
  // TODO(palfia): TF save list is not in sync with
  // CPURegList::GetCalleeSaved(): x30 is missing.
  // DCHECK(saves.list() == CPURegList::GetCalleeSaved().list());
  CPURegList saves = CPURegList(CPURegister::kRegister, kXRegSizeInBits,
                                descriptor->CalleeSavedRegisters());
  saved_count = saves.Count();
  if (saved_count != 0) {
    __ PushCPURegList(saves);
  }
}

void CodeGenerator::AssembleReturn(InstructionOperand* pop) {
  CallDescriptor* descriptor = linkage()->GetIncomingDescriptor();

  // Restore registers.
  CPURegList saves = CPURegList(CPURegister::kRegister, kXRegSizeInBits,
                                descriptor->CalleeSavedRegisters());
  if (saves.Count() != 0) {
    __ PopCPURegList(saves);
  }

  // Restore fp registers.
  CPURegList saves_fp = CPURegList(CPURegister::kFPRegister, kDRegSizeInBits,
                                   descriptor->CalleeSavedFPRegisters());
  if (saves_fp.Count() != 0) {
    __ PopCPURegList(saves_fp);
  }

  unwinding_info_writer_.MarkBlockWillExit();

  Arm64OperandConverter g(this, nullptr);
  int pop_count = static_cast<int>(descriptor->StackParameterCount());
  if (descriptor->IsCFunctionCall()) {
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
        if (descriptor->UseNativeStack()) {
          pop_count += (pop_count & 1);  // align
        }
      }
    } else {
      AssembleDeconstructFrame();
      if (descriptor->UseNativeStack()) {
        pop_count += (pop_count & 1);  // align
      }
    }
  } else if (descriptor->UseNativeStack()) {
    pop_count += (pop_count & 1);  // align
  }

  if (pop->IsImmediate()) {
    DCHECK_EQ(Constant::kInt32, g.ToConstant(pop).type());
    pop_count += g.ToConstant(pop).ToInt32();
    __ Drop(pop_count);
  } else {
    Register pop_reg = g.ToRegister(pop);
    __ Add(pop_reg, pop_reg, pop_count);
    __ Drop(pop_reg);
  }

  if (descriptor->UseNativeStack()) {
    __ AssertCspAligned();
  }
  __ Ret();
}

void CodeGenerator::FinishCode() { masm()->CheckConstPool(true, false); }

void CodeGenerator::AssembleMove(InstructionOperand* source,
                                 InstructionOperand* destination) {
  Arm64OperandConverter g(this, nullptr);
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
        Handle<HeapObject> src_object = src.ToHeapObject();
        Heap::RootListIndex index;
        if (IsMaterializableFromRoot(src_object, &index)) {
          __ LoadRoot(dst, index);
        } else {
          __ LoadObject(dst, src_object);
        }
      } else {
        __ Mov(dst, g.ToImmediate(source));
      }
      if (destination->IsStackSlot()) {
        __ Str(dst, g.ToMemOperand(destination, masm()));
      }
    } else if (src.type() == Constant::kFloat32) {
      if (destination->IsFPRegister()) {
        FPRegister dst = g.ToDoubleRegister(destination).S();
        __ Fmov(dst, src.ToFloat32());
      } else {
        DCHECK(destination->IsFPStackSlot());
        if (bit_cast<int32_t>(src.ToFloat32()) == 0) {
          __ Str(wzr, g.ToMemOperand(destination, masm()));
        } else {
          UseScratchRegisterScope scope(masm());
          FPRegister temp = scope.AcquireS();
          __ Fmov(temp, src.ToFloat32());
          __ Str(temp, g.ToMemOperand(destination, masm()));
        }
      }
    } else {
      DCHECK_EQ(Constant::kFloat64, src.type());
      if (destination->IsFPRegister()) {
        FPRegister dst = g.ToDoubleRegister(destination);
        __ Fmov(dst, src.ToFloat64());
      } else {
        DCHECK(destination->IsFPStackSlot());
        if (bit_cast<int64_t>(src.ToFloat64()) == 0) {
          __ Str(xzr, g.ToMemOperand(destination, masm()));
        } else {
          UseScratchRegisterScope scope(masm());
          FPRegister temp = scope.AcquireD();
          __ Fmov(temp, src.ToFloat64());
          __ Str(temp, g.ToMemOperand(destination, masm()));
        }
      }
    }
  } else if (source->IsFPRegister()) {
    FPRegister src = g.ToDoubleRegister(source);
    if (destination->IsFPRegister()) {
      FPRegister dst = g.ToDoubleRegister(destination);
      __ Fmov(dst, src);
    } else {
      DCHECK(destination->IsFPStackSlot());
      __ Str(src, g.ToMemOperand(destination, masm()));
    }
  } else if (source->IsFPStackSlot()) {
    DCHECK(destination->IsFPRegister() || destination->IsFPStackSlot());
    MemOperand src = g.ToMemOperand(source, masm());
    if (destination->IsFPRegister()) {
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
  Arm64OperandConverter g(this, nullptr);
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
  } else if (source->IsStackSlot() || source->IsFPStackSlot()) {
    UseScratchRegisterScope scope(masm());
    DoubleRegister temp_0 = scope.AcquireD();
    DoubleRegister temp_1 = scope.AcquireD();
    MemOperand src = g.ToMemOperand(source, masm());
    MemOperand dst = g.ToMemOperand(destination, masm());
    __ Ldr(temp_0, src);
    __ Ldr(temp_1, dst);
    __ Str(temp_0, dst);
    __ Str(temp_1, src);
  } else if (source->IsFPRegister()) {
    UseScratchRegisterScope scope(masm());
    FPRegister temp = scope.AcquireD();
    FPRegister src = g.ToDoubleRegister(source);
    if (destination->IsFPRegister()) {
      FPRegister dst = g.ToDoubleRegister(destination);
      __ Fmov(temp, src);
      __ Fmov(src, dst);
      __ Fmov(dst, temp);
    } else {
      DCHECK(destination->IsFPStackSlot());
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


void CodeGenerator::AssembleJumpTable(Label** targets, size_t target_count) {
  // On 64-bit ARM we emit the jump tables inline.
  UNREACHABLE();
}


void CodeGenerator::EnsureSpaceForLazyDeopt() {
  if (!info()->ShouldEnsureSpaceForLazyDeopt()) {
    return;
  }

  int space_needed = Deoptimizer::patch_size();
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

#undef __

}  // namespace compiler
}  // namespace internal
}  // namespace v8
