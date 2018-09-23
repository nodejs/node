// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/code-generator.h"

#include "src/arm/macro-assembler-arm.h"
#include "src/assembler-inl.h"
#include "src/boxed-float.h"
#include "src/compiler/code-generator-impl.h"
#include "src/compiler/gap-resolver.h"
#include "src/compiler/node-matchers.h"
#include "src/compiler/osr.h"
#include "src/double.h"
#include "src/heap/heap-inl.h"
#include "src/optimized-compilation-info.h"
#include "src/wasm/wasm-objects.h"

namespace v8 {
namespace internal {
namespace compiler {

#define __ tasm()->

// Adds Arm-specific methods to convert InstructionOperands.
class ArmOperandConverter final : public InstructionOperandConverter {
 public:
  ArmOperandConverter(CodeGenerator* gen, Instruction* instr)
      : InstructionOperandConverter(gen, instr) {}

  SBit OutputSBit() const {
    switch (instr_->flags_mode()) {
      case kFlags_branch:
      case kFlags_branch_and_poison:
      case kFlags_deoptimize:
      case kFlags_deoptimize_and_poison:
      case kFlags_set:
      case kFlags_trap:
        return SetCC;
      case kFlags_none:
        return LeaveCC;
    }
    UNREACHABLE();
  }

  Operand InputImmediate(size_t index) {
    return ToImmediate(instr_->InputAt(index));
  }

  Operand InputOperand2(size_t first_index) {
    const size_t index = first_index;
    switch (AddressingModeField::decode(instr_->opcode())) {
      case kMode_None:
      case kMode_Offset_RI:
      case kMode_Offset_RR:
        break;
      case kMode_Operand2_I:
        return InputImmediate(index + 0);
      case kMode_Operand2_R:
        return Operand(InputRegister(index + 0));
      case kMode_Operand2_R_ASR_I:
        return Operand(InputRegister(index + 0), ASR, InputInt5(index + 1));
      case kMode_Operand2_R_ASR_R:
        return Operand(InputRegister(index + 0), ASR, InputRegister(index + 1));
      case kMode_Operand2_R_LSL_I:
        return Operand(InputRegister(index + 0), LSL, InputInt5(index + 1));
      case kMode_Operand2_R_LSL_R:
        return Operand(InputRegister(index + 0), LSL, InputRegister(index + 1));
      case kMode_Operand2_R_LSR_I:
        return Operand(InputRegister(index + 0), LSR, InputInt5(index + 1));
      case kMode_Operand2_R_LSR_R:
        return Operand(InputRegister(index + 0), LSR, InputRegister(index + 1));
      case kMode_Operand2_R_ROR_I:
        return Operand(InputRegister(index + 0), ROR, InputInt5(index + 1));
      case kMode_Operand2_R_ROR_R:
        return Operand(InputRegister(index + 0), ROR, InputRegister(index + 1));
    }
    UNREACHABLE();
  }

  MemOperand InputOffset(size_t* first_index) {
    const size_t index = *first_index;
    switch (AddressingModeField::decode(instr_->opcode())) {
      case kMode_None:
      case kMode_Operand2_I:
      case kMode_Operand2_R:
      case kMode_Operand2_R_ASR_I:
      case kMode_Operand2_R_ASR_R:
      case kMode_Operand2_R_LSL_R:
      case kMode_Operand2_R_LSR_I:
      case kMode_Operand2_R_LSR_R:
      case kMode_Operand2_R_ROR_I:
      case kMode_Operand2_R_ROR_R:
        break;
      case kMode_Operand2_R_LSL_I:
        *first_index += 3;
        return MemOperand(InputRegister(index + 0), InputRegister(index + 1),
                          LSL, InputInt32(index + 2));
      case kMode_Offset_RI:
        *first_index += 2;
        return MemOperand(InputRegister(index + 0), InputInt32(index + 1));
      case kMode_Offset_RR:
        *first_index += 2;
        return MemOperand(InputRegister(index + 0), InputRegister(index + 1));
    }
    UNREACHABLE();
  }

  MemOperand InputOffset(size_t first_index = 0) {
    return InputOffset(&first_index);
  }

  Operand ToImmediate(InstructionOperand* operand) {
    Constant constant = ToConstant(operand);
    switch (constant.type()) {
      case Constant::kInt32:
        if (RelocInfo::IsWasmReference(constant.rmode())) {
          return Operand(constant.ToInt32(), constant.rmode());
        } else {
          return Operand(constant.ToInt32());
        }
      case Constant::kFloat32:
        return Operand::EmbeddedNumber(constant.ToFloat32());
      case Constant::kFloat64:
        return Operand::EmbeddedNumber(constant.ToFloat64().value());
      case Constant::kExternalReference:
        return Operand(constant.ToExternalReference());
      case Constant::kInt64:
      case Constant::kHeapObject:
      // TODO(dcarney): loading RPO constants on arm.
      case Constant::kRpoNumber:
        break;
    }
    UNREACHABLE();
  }

  MemOperand ToMemOperand(InstructionOperand* op) const {
    DCHECK_NOT_NULL(op);
    DCHECK(op->IsStackSlot() || op->IsFPStackSlot());
    return SlotToMemOperand(AllocatedOperand::cast(op)->index());
  }

  MemOperand SlotToMemOperand(int slot) const {
    FrameOffset offset = frame_access_state()->GetFrameOffset(slot);
    return MemOperand(offset.from_stack_pointer() ? sp : fp, offset.offset());
  }

  NeonMemOperand NeonInputOperand(size_t first_index) {
    const size_t index = first_index;
    switch (AddressingModeField::decode(instr_->opcode())) {
      case kMode_Offset_RR:
        return NeonMemOperand(InputRegister(index + 0),
                              InputRegister(index + 1));
      case kMode_Operand2_R:
        return NeonMemOperand(InputRegister(index + 0));
      default:
        break;
    }
    UNREACHABLE();
  }
};

namespace {

class OutOfLineRecordWrite final : public OutOfLineCode {
 public:
  OutOfLineRecordWrite(CodeGenerator* gen, Register object, Register index,
                       Register value, Register scratch0, Register scratch1,
                       RecordWriteMode mode,
                       UnwindingInfoWriter* unwinding_info_writer)
      : OutOfLineCode(gen),
        object_(object),
        index_(index),
        index_immediate_(0),
        value_(value),
        scratch0_(scratch0),
        scratch1_(scratch1),
        mode_(mode),
        must_save_lr_(!gen->frame_access_state()->has_frame()),
        unwinding_info_writer_(unwinding_info_writer),
        zone_(gen->zone()) {}

  OutOfLineRecordWrite(CodeGenerator* gen, Register object, int32_t index,
                       Register value, Register scratch0, Register scratch1,
                       RecordWriteMode mode,
                       UnwindingInfoWriter* unwinding_info_writer)
      : OutOfLineCode(gen),
        object_(object),
        index_(no_reg),
        index_immediate_(index),
        value_(value),
        scratch0_(scratch0),
        scratch1_(scratch1),
        mode_(mode),
        must_save_lr_(!gen->frame_access_state()->has_frame()),
        unwinding_info_writer_(unwinding_info_writer),
        zone_(gen->zone()) {}

  void Generate() final {
    if (mode_ > RecordWriteMode::kValueIsPointer) {
      __ JumpIfSmi(value_, exit());
    }
    __ CheckPageFlag(value_, scratch0_,
                     MemoryChunk::kPointersToHereAreInterestingMask, eq,
                     exit());
    if (index_ == no_reg) {
      __ add(scratch1_, object_, Operand(index_immediate_));
    } else {
      DCHECK_EQ(0, index_immediate_);
      __ add(scratch1_, object_, Operand(index_));
    }
    RememberedSetAction const remembered_set_action =
        mode_ > RecordWriteMode::kValueIsMap ? EMIT_REMEMBERED_SET
                                             : OMIT_REMEMBERED_SET;
    SaveFPRegsMode const save_fp_mode =
        frame()->DidAllocateDoubleRegisters() ? kSaveFPRegs : kDontSaveFPRegs;
    if (must_save_lr_) {
      // We need to save and restore lr if the frame was elided.
      __ Push(lr);
      unwinding_info_writer_->MarkLinkRegisterOnTopOfStack(__ pc_offset());
    }
    __ CallRecordWriteStub(object_, scratch1_, remembered_set_action,
                           save_fp_mode);
    if (must_save_lr_) {
      __ Pop(lr);
      unwinding_info_writer_->MarkPopLinkRegisterFromTopOfStack(__ pc_offset());
    }
  }

 private:
  Register const object_;
  Register const index_;
  int32_t const index_immediate_;  // Valid if index_==no_reg.
  Register const value_;
  Register const scratch0_;
  Register const scratch1_;
  RecordWriteMode const mode_;
  bool must_save_lr_;
  UnwindingInfoWriter* const unwinding_info_writer_;
  Zone* zone_;
};

template <typename T>
class OutOfLineFloatMin final : public OutOfLineCode {
 public:
  OutOfLineFloatMin(CodeGenerator* gen, T result, T left, T right)
      : OutOfLineCode(gen), result_(result), left_(left), right_(right) {}

  void Generate() final { __ FloatMinOutOfLine(result_, left_, right_); }

 private:
  T const result_;
  T const left_;
  T const right_;
};
typedef OutOfLineFloatMin<SwVfpRegister> OutOfLineFloat32Min;
typedef OutOfLineFloatMin<DwVfpRegister> OutOfLineFloat64Min;

template <typename T>
class OutOfLineFloatMax final : public OutOfLineCode {
 public:
  OutOfLineFloatMax(CodeGenerator* gen, T result, T left, T right)
      : OutOfLineCode(gen), result_(result), left_(left), right_(right) {}

  void Generate() final { __ FloatMaxOutOfLine(result_, left_, right_); }

 private:
  T const result_;
  T const left_;
  T const right_;
};
typedef OutOfLineFloatMax<SwVfpRegister> OutOfLineFloat32Max;
typedef OutOfLineFloatMax<DwVfpRegister> OutOfLineFloat64Max;

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
    case kPositiveOrZero:
      return pl;
    case kNegative:
      return mi;
    default:
      break;
  }
  UNREACHABLE();
}

void EmitWordLoadPoisoningIfNeeded(CodeGenerator* codegen,
                                   InstructionCode opcode,
                                   ArmOperandConverter& i) {
  const MemoryAccessMode access_mode =
      static_cast<MemoryAccessMode>(MiscField::decode(opcode));
  if (access_mode == kMemoryAccessPoisoned) {
    Register value = i.OutputRegister();
    codegen->tasm()->and_(value, value, Operand(kSpeculationPoisonRegister));
  }
}

void ComputePoisonedAddressForLoad(CodeGenerator* codegen,
                                   InstructionCode opcode,
                                   ArmOperandConverter& i, Register address) {
  DCHECK_EQ(kMemoryAccessPoisoned,
            static_cast<MemoryAccessMode>(MiscField::decode(opcode)));
  switch (AddressingModeField::decode(opcode)) {
    case kMode_Offset_RI:
      codegen->tasm()->mov(address, i.InputImmediate(1));
      codegen->tasm()->add(address, address, i.InputRegister(0));
      break;
    case kMode_Offset_RR:
      codegen->tasm()->add(address, i.InputRegister(0), i.InputRegister(1));
      break;
    default:
      UNREACHABLE();
  }
  codegen->tasm()->and_(address, address, Operand(kSpeculationPoisonRegister));
}

}  // namespace

#define ASSEMBLE_ATOMIC_LOAD_INTEGER(asm_instr)                       \
  do {                                                                \
    __ asm_instr(i.OutputRegister(),                                  \
                 MemOperand(i.InputRegister(0), i.InputRegister(1))); \
    __ dmb(ISH);                                                      \
  } while (0)

#define ASSEMBLE_ATOMIC_STORE_INTEGER(asm_instr)                      \
  do {                                                                \
    __ dmb(ISH);                                                      \
    __ asm_instr(i.InputRegister(2),                                  \
                 MemOperand(i.InputRegister(0), i.InputRegister(1))); \
    __ dmb(ISH);                                                      \
  } while (0)

#define ASSEMBLE_ATOMIC_EXCHANGE_INTEGER(load_instr, store_instr)             \
  do {                                                                        \
    Label exchange;                                                           \
    __ add(i.TempRegister(1), i.InputRegister(0), i.InputRegister(1));        \
    __ dmb(ISH);                                                              \
    __ bind(&exchange);                                                       \
    __ load_instr(i.OutputRegister(0), i.TempRegister(1));                    \
    __ store_instr(i.TempRegister(0), i.InputRegister(2), i.TempRegister(1)); \
    __ teq(i.TempRegister(0), Operand(0));                                    \
    __ b(ne, &exchange);                                                      \
    __ dmb(ISH);                                                              \
  } while (0)

#define ASSEMBLE_ATOMIC_COMPARE_EXCHANGE_INTEGER(load_instr, store_instr,     \
                                                 cmp_reg)                     \
  do {                                                                        \
    Label compareExchange;                                                    \
    Label exit;                                                               \
    __ dmb(ISH);                                                              \
    __ bind(&compareExchange);                                                \
    __ load_instr(i.OutputRegister(0), i.TempRegister(1));                    \
    __ teq(cmp_reg, Operand(i.OutputRegister(0)));                            \
    __ b(ne, &exit);                                                          \
    __ store_instr(i.TempRegister(0), i.InputRegister(3), i.TempRegister(1)); \
    __ teq(i.TempRegister(0), Operand(0));                                    \
    __ b(ne, &compareExchange);                                               \
    __ bind(&exit);                                                           \
    __ dmb(ISH);                                                              \
  } while (0)

#define ASSEMBLE_ATOMIC_BINOP(load_instr, store_instr, bin_instr)            \
  do {                                                                       \
    Label binop;                                                             \
    __ add(i.TempRegister(1), i.InputRegister(0), i.InputRegister(1));       \
    __ dmb(ISH);                                                             \
    __ bind(&binop);                                                         \
    __ load_instr(i.OutputRegister(0), i.TempRegister(1));                   \
    __ bin_instr(i.TempRegister(0), i.OutputRegister(0),                     \
                 Operand(i.InputRegister(2)));                               \
    __ store_instr(i.TempRegister(2), i.TempRegister(0), i.TempRegister(1)); \
    __ teq(i.TempRegister(2), Operand(0));                                   \
    __ b(ne, &binop);                                                        \
    __ dmb(ISH);                                                             \
  } while (0)

#define ASSEMBLE_ATOMIC64_ARITH_BINOP(instr1, instr2)                       \
  do {                                                                      \
    Label binop;                                                            \
    __ add(i.TempRegister(0), i.InputRegister(2), i.InputRegister(3));      \
    __ dmb(ISH);                                                            \
    __ bind(&binop);                                                        \
    __ ldrexd(i.OutputRegister(0), i.OutputRegister(1), i.TempRegister(0)); \
    __ instr1(i.TempRegister(1), i.OutputRegister(0), i.InputRegister(0),   \
              SBit::SetCC);                                                 \
    __ instr2(i.TempRegister(2), i.OutputRegister(1),                       \
              Operand(i.InputRegister(1)));                                 \
    DCHECK_EQ(LeaveCC, i.OutputSBit());                                     \
    __ strexd(i.TempRegister(3), i.TempRegister(1), i.TempRegister(2),      \
              i.TempRegister(0));                                           \
    __ teq(i.TempRegister(3), Operand(0));                                  \
    __ b(ne, &binop);                                                       \
    __ dmb(ISH);                                                            \
  } while (0)

#define ASSEMBLE_ATOMIC64_LOGIC_BINOP(instr)                                \
  do {                                                                      \
    Label binop;                                                            \
    __ add(i.TempRegister(0), i.InputRegister(2), i.InputRegister(3));      \
    __ dmb(ISH);                                                            \
    __ bind(&binop);                                                        \
    __ ldrexd(i.OutputRegister(0), i.OutputRegister(1), i.TempRegister(0)); \
    __ instr(i.TempRegister(1), i.OutputRegister(0),                        \
             Operand(i.InputRegister(0)));                                  \
    __ instr(i.TempRegister(2), i.OutputRegister(1),                        \
             Operand(i.InputRegister(1)));                                  \
    __ strexd(i.TempRegister(3), i.TempRegister(1), i.TempRegister(2),      \
              i.TempRegister(0));                                           \
    __ teq(i.TempRegister(3), Operand(0));                                  \
    __ b(ne, &binop);                                                       \
    __ dmb(ISH);                                                            \
  } while (0)

#define ATOMIC_NARROW_OP_CLEAR_HIGH_WORD(op)       \
  if (arch_opcode == kArmWord64AtomicNarrow##op) { \
    __ mov(i.OutputRegister(1), Operand(0));       \
  }

#define ASSEMBLE_IEEE754_BINOP(name)                                           \
  do {                                                                         \
    /* TODO(bmeurer): We should really get rid of this special instruction, */ \
    /* and generate a CallAddress instruction instead. */                      \
    FrameScope scope(tasm(), StackFrame::MANUAL);                              \
    __ PrepareCallCFunction(0, 2);                                             \
    __ MovToFloatParameters(i.InputDoubleRegister(0),                          \
                            i.InputDoubleRegister(1));                         \
    __ CallCFunction(ExternalReference::ieee754_##name##_function(), 0, 2);    \
    /* Move the result in the double result register. */                       \
    __ MovFromFloatResult(i.OutputDoubleRegister());                           \
    DCHECK_EQ(LeaveCC, i.OutputSBit());                                        \
  } while (0)

#define ASSEMBLE_IEEE754_UNOP(name)                                            \
  do {                                                                         \
    /* TODO(bmeurer): We should really get rid of this special instruction, */ \
    /* and generate a CallAddress instruction instead. */                      \
    FrameScope scope(tasm(), StackFrame::MANUAL);                              \
    __ PrepareCallCFunction(0, 1);                                             \
    __ MovToFloatParameter(i.InputDoubleRegister(0));                          \
    __ CallCFunction(ExternalReference::ieee754_##name##_function(), 0, 1);    \
    /* Move the result in the double result register. */                       \
    __ MovFromFloatResult(i.OutputDoubleRegister());                           \
    DCHECK_EQ(LeaveCC, i.OutputSBit());                                        \
  } while (0)

#define ASSEMBLE_NEON_NARROWING_OP(dt)                \
  do {                                                \
    Simd128Register dst = i.OutputSimd128Register(),  \
                    src0 = i.InputSimd128Register(0), \
                    src1 = i.InputSimd128Register(1); \
    if (dst == src0 && dst == src1) {                 \
      __ vqmovn(dt, dst.low(), src0);                 \
      __ vmov(dst.high(), dst.low());                 \
    } else if (dst == src0) {                         \
      __ vqmovn(dt, dst.low(), src0);                 \
      __ vqmovn(dt, dst.high(), src1);                \
    } else {                                          \
      __ vqmovn(dt, dst.high(), src1);                \
      __ vqmovn(dt, dst.low(), src0);                 \
    }                                                 \
  } while (0)

#define ASSEMBLE_NEON_PAIRWISE_OP(op, size)               \
  do {                                                    \
    Simd128Register dst = i.OutputSimd128Register(),      \
                    src0 = i.InputSimd128Register(0),     \
                    src1 = i.InputSimd128Register(1);     \
    if (dst == src0) {                                    \
      __ op(size, dst.low(), src0.low(), src0.high());    \
      if (dst == src1) {                                  \
        __ vmov(dst.high(), dst.low());                   \
      } else {                                            \
        __ op(size, dst.high(), src1.low(), src1.high()); \
      }                                                   \
    } else {                                              \
      __ op(size, dst.high(), src1.low(), src1.high());   \
      __ op(size, dst.low(), src0.low(), src0.high());    \
    }                                                     \
  } while (0)

void CodeGenerator::AssembleDeconstructFrame() {
  __ LeaveFrame(StackFrame::MANUAL);
  unwinding_info_writer_.MarkFrameDeconstructed(__ pc_offset());
}

void CodeGenerator::AssemblePrepareTailCall() {
  if (frame_access_state()->has_frame()) {
    __ ldr(lr, MemOperand(fp, StandardFrameConstants::kCallerPCOffset));
    __ ldr(fp, MemOperand(fp, StandardFrameConstants::kCallerFPOffset));
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
  __ ldr(scratch1, MemOperand(fp, StandardFrameConstants::kContextOffset));
  __ cmp(scratch1,
         Operand(StackFrame::TypeToMarker(StackFrame::ARGUMENTS_ADAPTOR)));
  __ b(ne, &done);

  // Load arguments count from current arguments adaptor frame (note, it
  // does not include receiver).
  Register caller_args_count_reg = scratch1;
  __ ldr(caller_args_count_reg,
         MemOperand(fp, ArgumentsAdaptorFrameConstants::kLengthOffset));
  __ SmiUntag(caller_args_count_reg);

  ParameterCount callee_args_count(args_reg);
  __ PrepareForTailCall(callee_args_count, caller_args_count_reg, scratch2,
                        scratch3);
  __ bind(&done);
}

namespace {

void FlushPendingPushRegisters(TurboAssembler* tasm,
                               FrameAccessState* frame_access_state,
                               ZoneVector<Register>* pending_pushes) {
  switch (pending_pushes->size()) {
    case 0:
      break;
    case 1:
      tasm->push((*pending_pushes)[0]);
      break;
    case 2:
      tasm->Push((*pending_pushes)[0], (*pending_pushes)[1]);
      break;
    case 3:
      tasm->Push((*pending_pushes)[0], (*pending_pushes)[1],
                 (*pending_pushes)[2]);
      break;
    default:
      UNREACHABLE();
      break;
  }
  frame_access_state->IncreaseSPDelta(pending_pushes->size());
  pending_pushes->clear();
}

void AdjustStackPointerForTailCall(
    TurboAssembler* tasm, FrameAccessState* state, int new_slot_above_sp,
    ZoneVector<Register>* pending_pushes = nullptr,
    bool allow_shrinkage = true) {
  int current_sp_offset = state->GetSPToFPSlotCount() +
                          StandardFrameConstants::kFixedSlotCountAboveFp;
  int stack_slot_delta = new_slot_above_sp - current_sp_offset;
  if (stack_slot_delta > 0) {
    if (pending_pushes != nullptr) {
      FlushPendingPushRegisters(tasm, state, pending_pushes);
    }
    tasm->sub(sp, sp, Operand(stack_slot_delta * kPointerSize));
    state->IncreaseSPDelta(stack_slot_delta);
  } else if (allow_shrinkage && stack_slot_delta < 0) {
    if (pending_pushes != nullptr) {
      FlushPendingPushRegisters(tasm, state, pending_pushes);
    }
    tasm->add(sp, sp, Operand(-stack_slot_delta * kPointerSize));
    state->IncreaseSPDelta(stack_slot_delta);
  }
}

}  // namespace

void CodeGenerator::AssembleTailCallBeforeGap(Instruction* instr,
                                              int first_unused_stack_slot) {
  ZoneVector<MoveOperands*> pushes(zone());
  GetPushCompatibleMoves(instr, kRegisterPush, &pushes);

  if (!pushes.empty() &&
      (LocationOperand::cast(pushes.back()->destination()).index() + 1 ==
       first_unused_stack_slot)) {
    ArmOperandConverter g(this, instr);
    ZoneVector<Register> pending_pushes(zone());
    for (auto move : pushes) {
      LocationOperand destination_location(
          LocationOperand::cast(move->destination()));
      InstructionOperand source(move->source());
      AdjustStackPointerForTailCall(
          tasm(), frame_access_state(),
          destination_location.index() - pending_pushes.size(),
          &pending_pushes);
      // Pushes of non-register data types are not supported.
      DCHECK(source.IsRegister());
      LocationOperand source_location(LocationOperand::cast(source));
      pending_pushes.push_back(source_location.GetRegister());
      // TODO(arm): We can push more than 3 registers at once. Add support in
      // the macro-assembler for pushing a list of registers.
      if (pending_pushes.size() == 3) {
        FlushPendingPushRegisters(tasm(), frame_access_state(),
                                  &pending_pushes);
      }
      move->Eliminate();
    }
    FlushPendingPushRegisters(tasm(), frame_access_state(), &pending_pushes);
  }
  AdjustStackPointerForTailCall(tasm(), frame_access_state(),
                                first_unused_stack_slot, nullptr, false);
}

void CodeGenerator::AssembleTailCallAfterGap(Instruction* instr,
                                             int first_unused_stack_slot) {
  AdjustStackPointerForTailCall(tasm(), frame_access_state(),
                                first_unused_stack_slot);
}

// Check that {kJavaScriptCallCodeStartRegister} is correct.
void CodeGenerator::AssembleCodeStartRegisterCheck() {
  UseScratchRegisterScope temps(tasm());
  Register scratch = temps.Acquire();
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
  Register scratch = temps.Acquire();
  int offset = Code::kCodeDataContainerOffset - Code::kHeaderSize;
  __ ldr(scratch, MemOperand(kJavaScriptCallCodeStartRegister, offset));
  __ ldr(scratch,
         FieldMemOperand(scratch, CodeDataContainer::kKindSpecificFlagsOffset));
  __ tst(scratch, Operand(1 << Code::kMarkedForDeoptimizationBit));
  // Ensure we're not serializing (otherwise we'd need to use an indirection to
  // access the builtin below).
  DCHECK(!isolate()->ShouldLoadConstantsFromRootList());
  Handle<Code> code = isolate()->builtins()->builtin_handle(
      Builtins::kCompileLazyDeoptimizedCode);
  __ Jump(code, RelocInfo::CODE_TARGET, ne);
}

void CodeGenerator::GenerateSpeculationPoisonFromCodeStartRegister() {
  UseScratchRegisterScope temps(tasm());
  Register scratch = temps.Acquire();

  // Set a mask which has all bits set in the normal case, but has all
  // bits cleared if we are speculatively executing the wrong PC.
  __ ComputeCodeStartAddress(scratch);
  __ cmp(kJavaScriptCallCodeStartRegister, scratch);
  __ mov(kSpeculationPoisonRegister, Operand(-1), SBit::LeaveCC, eq);
  __ mov(kSpeculationPoisonRegister, Operand(0), SBit::LeaveCC, ne);
  __ csdb();
}

void CodeGenerator::AssembleRegisterArgumentPoisoning() {
  __ and_(kJSFunctionRegister, kJSFunctionRegister, kSpeculationPoisonRegister);
  __ and_(kContextRegister, kContextRegister, kSpeculationPoisonRegister);
  __ and_(sp, sp, kSpeculationPoisonRegister);
}

// Assembles an instruction after register allocation, producing machine code.
CodeGenerator::CodeGenResult CodeGenerator::AssembleArchInstruction(
    Instruction* instr) {
  ArmOperandConverter i(this, instr);

  __ MaybeCheckConstPool();
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
        __ add(reg, reg, Operand(Code::kHeaderSize - kHeapObjectTag));
        __ Call(reg);
      }
      RecordCallPosition(instr);
      DCHECK_EQ(LeaveCC, i.OutputSBit());
      frame_access_state()->ClearSPDelta();
      break;
    }
    case kArchCallWasmFunction: {
      if (instr->InputAt(0)->IsImmediate()) {
        Constant constant = i.ToConstant(instr->InputAt(0));
        Address wasm_code = static_cast<Address>(constant.ToInt32());
        __ Call(wasm_code, constant.rmode());
      } else {
        __ Call(i.InputRegister(0));
      }
      RecordCallPosition(instr);
      DCHECK_EQ(LeaveCC, i.OutputSBit());
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
        __ add(reg, reg, Operand(Code::kHeaderSize - kHeapObjectTag));
        __ Jump(reg);
      }
      DCHECK_EQ(LeaveCC, i.OutputSBit());
      unwinding_info_writer_.MarkBlockWillExit();
      frame_access_state()->ClearSPDelta();
      frame_access_state()->SetFrameAccessToDefault();
      break;
    }
    case kArchTailCallWasm: {
      if (instr->InputAt(0)->IsImmediate()) {
        Constant constant = i.ToConstant(instr->InputAt(0));
        Address wasm_code = static_cast<Address>(constant.ToInt32());
        __ Jump(wasm_code, constant.rmode());
      } else {
        __ Jump(i.InputRegister(0));
      }
      DCHECK_EQ(LeaveCC, i.OutputSBit());
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
        UseScratchRegisterScope temps(tasm());
        Register scratch = temps.Acquire();
        // Check the function's context matches the context argument.
        __ ldr(scratch, FieldMemOperand(func, JSFunction::kContextOffset));
        __ cmp(cp, scratch);
        __ Assert(eq, AbortReason::kWrongFunctionContext);
      }
      static_assert(kJavaScriptCallCodeStartRegister == r2, "ABI mismatch");
      __ ldr(r2, FieldMemOperand(func, JSFunction::kCodeOffset));
      __ add(r2, r2, Operand(Code::kHeaderSize - kHeapObjectTag));
      __ Call(r2);
      RecordCallPosition(instr);
      DCHECK_EQ(LeaveCC, i.OutputSBit());
      frame_access_state()->ClearSPDelta();
      break;
    }
    case kArchPrepareCallCFunction: {
      int const num_parameters = MiscField::decode(instr->opcode());
      __ PrepareCallCFunction(num_parameters);
      // Frame alignment requires using FP-relative frame addressing.
      frame_access_state()->SetFrameAccessToFP();
      break;
    }
    case kArchSaveCallerRegisters: {
      fp_mode_ =
          static_cast<SaveFPRegsMode>(MiscField::decode(instr->opcode()));
      DCHECK(fp_mode_ == kDontSaveFPRegs || fp_mode_ == kSaveFPRegs);
      // kReturnRegister0 should have been saved before entering the stub.
      int bytes = __ PushCallerSaved(fp_mode_, kReturnRegister0);
      DCHECK_EQ(0, bytes % kPointerSize);
      DCHECK_EQ(0, frame_access_state()->sp_delta());
      frame_access_state()->IncreaseSPDelta(bytes / kPointerSize);
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
      frame_access_state()->IncreaseSPDelta(-(bytes / kPointerSize));
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
        __ CallCFunction(ref, num_parameters);
      } else {
        Register func = i.InputRegister(0);
        __ CallCFunction(func, num_parameters);
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
        frame_access_state()->IncreaseSPDelta(bytes / kPointerSize);
      }
      break;
    }
    case kArchJmp:
      AssembleArchJump(i.InputRpo(0));
      DCHECK_EQ(LeaveCC, i.OutputSBit());
      break;
    case kArchBinarySearchSwitch:
      AssembleArchBinarySearchSwitch(instr);
      break;
    case kArchLookupSwitch:
      AssembleArchLookupSwitch(instr);
      DCHECK_EQ(LeaveCC, i.OutputSBit());
      break;
    case kArchTableSwitch:
      AssembleArchTableSwitch(instr);
      DCHECK_EQ(LeaveCC, i.OutputSBit());
      break;
    case kArchDebugAbort:
      DCHECK(i.InputRegister(0) == r1);
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
      __ stop("kArchDebugAbort");
      unwinding_info_writer_.MarkBlockWillExit();
      break;
    case kArchDebugBreak:
      __ stop("kArchDebugBreak");
      break;
    case kArchComment:
      __ RecordComment(reinterpret_cast<const char*>(i.InputInt32(0)));
      break;
    case kArchThrowTerminator:
      DCHECK_EQ(LeaveCC, i.OutputSBit());
      unwinding_info_writer_.MarkBlockWillExit();
      break;
    case kArchNop:
      // don't emit code for nops.
      DCHECK_EQ(LeaveCC, i.OutputSBit());
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
      DCHECK_EQ(LeaveCC, i.OutputSBit());
      break;
    case kArchStackPointer:
      __ mov(i.OutputRegister(), sp);
      DCHECK_EQ(LeaveCC, i.OutputSBit());
      break;
    case kArchFramePointer:
      __ mov(i.OutputRegister(), fp);
      DCHECK_EQ(LeaveCC, i.OutputSBit());
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
      DCHECK_EQ(LeaveCC, i.OutputSBit());
      break;
    case kArchStoreWithWriteBarrier: {
      RecordWriteMode mode =
          static_cast<RecordWriteMode>(MiscField::decode(instr->opcode()));
      Register object = i.InputRegister(0);
      Register value = i.InputRegister(2);
      Register scratch0 = i.TempRegister(0);
      Register scratch1 = i.TempRegister(1);
      OutOfLineRecordWrite* ool;

      AddressingMode addressing_mode =
          AddressingModeField::decode(instr->opcode());
      if (addressing_mode == kMode_Offset_RI) {
        int32_t index = i.InputInt32(1);
        ool = new (zone())
            OutOfLineRecordWrite(this, object, index, value, scratch0, scratch1,
                                 mode, &unwinding_info_writer_);
        __ str(value, MemOperand(object, index));
      } else {
        DCHECK_EQ(kMode_Offset_RR, addressing_mode);
        Register index(i.InputRegister(1));
        ool = new (zone())
            OutOfLineRecordWrite(this, object, index, value, scratch0, scratch1,
                                 mode, &unwinding_info_writer_);
        __ str(value, MemOperand(object, index));
      }
      __ CheckPageFlag(object, scratch0,
                       MemoryChunk::kPointersFromHereAreInterestingMask, ne,
                       ool->entry());
      __ bind(ool->exit());
      break;
    }
    case kArchStackSlot: {
      FrameOffset offset =
          frame_access_state()->GetFrameOffset(i.InputInt32(0));
      Register base = offset.from_stack_pointer() ? sp : fp;
      __ add(i.OutputRegister(0), base, Operand(offset.offset()));
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
      __ Call(BUILTIN_CODE(isolate(), MathPowInternal), RelocInfo::CODE_TARGET);
      __ vmov(d0, d2);
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
    case kArmAdd:
      __ add(i.OutputRegister(), i.InputRegister(0), i.InputOperand2(1),
             i.OutputSBit());
      break;
    case kArmAnd:
      __ and_(i.OutputRegister(), i.InputRegister(0), i.InputOperand2(1),
              i.OutputSBit());
      break;
    case kArmBic:
      __ bic(i.OutputRegister(), i.InputRegister(0), i.InputOperand2(1),
             i.OutputSBit());
      break;
    case kArmMul:
      __ mul(i.OutputRegister(), i.InputRegister(0), i.InputRegister(1),
             i.OutputSBit());
      break;
    case kArmMla:
      __ mla(i.OutputRegister(), i.InputRegister(0), i.InputRegister(1),
             i.InputRegister(2), i.OutputSBit());
      break;
    case kArmMls: {
      CpuFeatureScope scope(tasm(), ARMv7);
      __ mls(i.OutputRegister(), i.InputRegister(0), i.InputRegister(1),
             i.InputRegister(2));
      DCHECK_EQ(LeaveCC, i.OutputSBit());
      break;
    }
    case kArmSmull:
      __ smull(i.OutputRegister(0), i.OutputRegister(1), i.InputRegister(0),
               i.InputRegister(1));
      break;
    case kArmSmmul:
      __ smmul(i.OutputRegister(), i.InputRegister(0), i.InputRegister(1));
      DCHECK_EQ(LeaveCC, i.OutputSBit());
      break;
    case kArmSmmla:
      __ smmla(i.OutputRegister(), i.InputRegister(0), i.InputRegister(1),
               i.InputRegister(2));
      DCHECK_EQ(LeaveCC, i.OutputSBit());
      break;
    case kArmUmull:
      __ umull(i.OutputRegister(0), i.OutputRegister(1), i.InputRegister(0),
               i.InputRegister(1), i.OutputSBit());
      break;
    case kArmSdiv: {
      CpuFeatureScope scope(tasm(), SUDIV);
      __ sdiv(i.OutputRegister(), i.InputRegister(0), i.InputRegister(1));
      DCHECK_EQ(LeaveCC, i.OutputSBit());
      break;
    }
    case kArmUdiv: {
      CpuFeatureScope scope(tasm(), SUDIV);
      __ udiv(i.OutputRegister(), i.InputRegister(0), i.InputRegister(1));
      DCHECK_EQ(LeaveCC, i.OutputSBit());
      break;
    }
    case kArmMov:
      __ Move(i.OutputRegister(), i.InputOperand2(0), i.OutputSBit());
      break;
    case kArmMvn:
      __ mvn(i.OutputRegister(), i.InputOperand2(0), i.OutputSBit());
      break;
    case kArmOrr:
      __ orr(i.OutputRegister(), i.InputRegister(0), i.InputOperand2(1),
             i.OutputSBit());
      break;
    case kArmEor:
      __ eor(i.OutputRegister(), i.InputRegister(0), i.InputOperand2(1),
             i.OutputSBit());
      break;
    case kArmSub:
      __ sub(i.OutputRegister(), i.InputRegister(0), i.InputOperand2(1),
             i.OutputSBit());
      break;
    case kArmRsb:
      __ rsb(i.OutputRegister(), i.InputRegister(0), i.InputOperand2(1),
             i.OutputSBit());
      break;
    case kArmBfc: {
      CpuFeatureScope scope(tasm(), ARMv7);
      __ bfc(i.OutputRegister(), i.InputInt8(1), i.InputInt8(2));
      DCHECK_EQ(LeaveCC, i.OutputSBit());
      break;
    }
    case kArmUbfx: {
      CpuFeatureScope scope(tasm(), ARMv7);
      __ ubfx(i.OutputRegister(), i.InputRegister(0), i.InputInt8(1),
              i.InputInt8(2));
      DCHECK_EQ(LeaveCC, i.OutputSBit());
      break;
    }
    case kArmSbfx: {
      CpuFeatureScope scope(tasm(), ARMv7);
      __ sbfx(i.OutputRegister(), i.InputRegister(0), i.InputInt8(1),
              i.InputInt8(2));
      DCHECK_EQ(LeaveCC, i.OutputSBit());
      break;
    }
    case kArmSxtb:
      __ sxtb(i.OutputRegister(), i.InputRegister(0), i.InputInt32(1));
      DCHECK_EQ(LeaveCC, i.OutputSBit());
      break;
    case kArmSxth:
      __ sxth(i.OutputRegister(), i.InputRegister(0), i.InputInt32(1));
      DCHECK_EQ(LeaveCC, i.OutputSBit());
      break;
    case kArmSxtab:
      __ sxtab(i.OutputRegister(), i.InputRegister(0), i.InputRegister(1),
               i.InputInt32(2));
      DCHECK_EQ(LeaveCC, i.OutputSBit());
      break;
    case kArmSxtah:
      __ sxtah(i.OutputRegister(), i.InputRegister(0), i.InputRegister(1),
               i.InputInt32(2));
      DCHECK_EQ(LeaveCC, i.OutputSBit());
      break;
    case kArmUxtb:
      __ uxtb(i.OutputRegister(), i.InputRegister(0), i.InputInt32(1));
      DCHECK_EQ(LeaveCC, i.OutputSBit());
      break;
    case kArmUxth:
      __ uxth(i.OutputRegister(), i.InputRegister(0), i.InputInt32(1));
      DCHECK_EQ(LeaveCC, i.OutputSBit());
      break;
    case kArmUxtab:
      __ uxtab(i.OutputRegister(), i.InputRegister(0), i.InputRegister(1),
               i.InputInt32(2));
      DCHECK_EQ(LeaveCC, i.OutputSBit());
      break;
    case kArmUxtah:
      __ uxtah(i.OutputRegister(), i.InputRegister(0), i.InputRegister(1),
               i.InputInt32(2));
      DCHECK_EQ(LeaveCC, i.OutputSBit());
      break;
    case kArmRbit: {
      CpuFeatureScope scope(tasm(), ARMv7);
      __ rbit(i.OutputRegister(), i.InputRegister(0));
      DCHECK_EQ(LeaveCC, i.OutputSBit());
      break;
    }
    case kArmRev:
      __ rev(i.OutputRegister(), i.InputRegister(0));
      DCHECK_EQ(LeaveCC, i.OutputSBit());
      break;
    case kArmClz:
      __ clz(i.OutputRegister(), i.InputRegister(0));
      DCHECK_EQ(LeaveCC, i.OutputSBit());
      break;
    case kArmCmp:
      __ cmp(i.InputRegister(0), i.InputOperand2(1));
      DCHECK_EQ(SetCC, i.OutputSBit());
      break;
    case kArmCmn:
      __ cmn(i.InputRegister(0), i.InputOperand2(1));
      DCHECK_EQ(SetCC, i.OutputSBit());
      break;
    case kArmTst:
      __ tst(i.InputRegister(0), i.InputOperand2(1));
      DCHECK_EQ(SetCC, i.OutputSBit());
      break;
    case kArmTeq:
      __ teq(i.InputRegister(0), i.InputOperand2(1));
      DCHECK_EQ(SetCC, i.OutputSBit());
      break;
    case kArmAddPair:
      // i.InputRegister(0) ... left low word.
      // i.InputRegister(1) ... left high word.
      // i.InputRegister(2) ... right low word.
      // i.InputRegister(3) ... right high word.
      __ add(i.OutputRegister(0), i.InputRegister(0), i.InputRegister(2),
             SBit::SetCC);
      __ adc(i.OutputRegister(1), i.InputRegister(1),
             Operand(i.InputRegister(3)));
      DCHECK_EQ(LeaveCC, i.OutputSBit());
      break;
    case kArmSubPair:
      // i.InputRegister(0) ... left low word.
      // i.InputRegister(1) ... left high word.
      // i.InputRegister(2) ... right low word.
      // i.InputRegister(3) ... right high word.
      __ sub(i.OutputRegister(0), i.InputRegister(0), i.InputRegister(2),
             SBit::SetCC);
      __ sbc(i.OutputRegister(1), i.InputRegister(1),
             Operand(i.InputRegister(3)));
      DCHECK_EQ(LeaveCC, i.OutputSBit());
      break;
    case kArmMulPair:
      // i.InputRegister(0) ... left low word.
      // i.InputRegister(1) ... left high word.
      // i.InputRegister(2) ... right low word.
      // i.InputRegister(3) ... right high word.
      __ umull(i.OutputRegister(0), i.OutputRegister(1), i.InputRegister(0),
               i.InputRegister(2));
      __ mla(i.OutputRegister(1), i.InputRegister(0), i.InputRegister(3),
             i.OutputRegister(1));
      __ mla(i.OutputRegister(1), i.InputRegister(2), i.InputRegister(1),
             i.OutputRegister(1));
      break;
    case kArmLslPair: {
      Register second_output =
          instr->OutputCount() >= 2 ? i.OutputRegister(1) : i.TempRegister(0);
      if (instr->InputAt(2)->IsImmediate()) {
        __ LslPair(i.OutputRegister(0), second_output, i.InputRegister(0),
                   i.InputRegister(1), i.InputInt32(2));
      } else {
        __ LslPair(i.OutputRegister(0), second_output, i.InputRegister(0),
                   i.InputRegister(1), i.InputRegister(2));
      }
      break;
    }
    case kArmLsrPair: {
      Register second_output =
          instr->OutputCount() >= 2 ? i.OutputRegister(1) : i.TempRegister(0);
      if (instr->InputAt(2)->IsImmediate()) {
        __ LsrPair(i.OutputRegister(0), second_output, i.InputRegister(0),
                   i.InputRegister(1), i.InputInt32(2));
      } else {
        __ LsrPair(i.OutputRegister(0), second_output, i.InputRegister(0),
                   i.InputRegister(1), i.InputRegister(2));
      }
      break;
    }
    case kArmAsrPair: {
      Register second_output =
          instr->OutputCount() >= 2 ? i.OutputRegister(1) : i.TempRegister(0);
      if (instr->InputAt(2)->IsImmediate()) {
        __ AsrPair(i.OutputRegister(0), second_output, i.InputRegister(0),
                   i.InputRegister(1), i.InputInt32(2));
      } else {
        __ AsrPair(i.OutputRegister(0), second_output, i.InputRegister(0),
                   i.InputRegister(1), i.InputRegister(2));
      }
      break;
    }
    case kArmVcmpF32:
      if (instr->InputAt(1)->IsFPRegister()) {
        __ VFPCompareAndSetFlags(i.InputFloatRegister(0),
                                 i.InputFloatRegister(1));
      } else {
        DCHECK(instr->InputAt(1)->IsImmediate());
        // 0.0 is the only immediate supported by vcmp instructions.
        DCHECK_EQ(0.0f, i.InputFloat32(1));
        __ VFPCompareAndSetFlags(i.InputFloatRegister(0), i.InputFloat32(1));
      }
      DCHECK_EQ(SetCC, i.OutputSBit());
      break;
    case kArmVaddF32:
      __ vadd(i.OutputFloatRegister(), i.InputFloatRegister(0),
              i.InputFloatRegister(1));
      DCHECK_EQ(LeaveCC, i.OutputSBit());
      break;
    case kArmVsubF32:
      __ vsub(i.OutputFloatRegister(), i.InputFloatRegister(0),
              i.InputFloatRegister(1));
      DCHECK_EQ(LeaveCC, i.OutputSBit());
      break;
    case kArmVmulF32:
      __ vmul(i.OutputFloatRegister(), i.InputFloatRegister(0),
              i.InputFloatRegister(1));
      DCHECK_EQ(LeaveCC, i.OutputSBit());
      break;
    case kArmVmlaF32:
      __ vmla(i.OutputFloatRegister(), i.InputFloatRegister(1),
              i.InputFloatRegister(2));
      DCHECK_EQ(LeaveCC, i.OutputSBit());
      break;
    case kArmVmlsF32:
      __ vmls(i.OutputFloatRegister(), i.InputFloatRegister(1),
              i.InputFloatRegister(2));
      DCHECK_EQ(LeaveCC, i.OutputSBit());
      break;
    case kArmVdivF32:
      __ vdiv(i.OutputFloatRegister(), i.InputFloatRegister(0),
              i.InputFloatRegister(1));
      DCHECK_EQ(LeaveCC, i.OutputSBit());
      break;
    case kArmVsqrtF32:
      __ vsqrt(i.OutputFloatRegister(), i.InputFloatRegister(0));
      break;
    case kArmVabsF32:
      __ vabs(i.OutputFloatRegister(), i.InputFloatRegister(0));
      break;
    case kArmVnegF32:
      __ vneg(i.OutputFloatRegister(), i.InputFloatRegister(0));
      break;
    case kArmVcmpF64:
      if (instr->InputAt(1)->IsFPRegister()) {
        __ VFPCompareAndSetFlags(i.InputDoubleRegister(0),
                                 i.InputDoubleRegister(1));
      } else {
        DCHECK(instr->InputAt(1)->IsImmediate());
        // 0.0 is the only immediate supported by vcmp instructions.
        DCHECK_EQ(0.0, i.InputDouble(1));
        __ VFPCompareAndSetFlags(i.InputDoubleRegister(0), i.InputDouble(1));
      }
      DCHECK_EQ(SetCC, i.OutputSBit());
      break;
    case kArmVaddF64:
      __ vadd(i.OutputDoubleRegister(), i.InputDoubleRegister(0),
              i.InputDoubleRegister(1));
      DCHECK_EQ(LeaveCC, i.OutputSBit());
      break;
    case kArmVsubF64:
      __ vsub(i.OutputDoubleRegister(), i.InputDoubleRegister(0),
              i.InputDoubleRegister(1));
      DCHECK_EQ(LeaveCC, i.OutputSBit());
      break;
    case kArmVmulF64:
      __ vmul(i.OutputDoubleRegister(), i.InputDoubleRegister(0),
              i.InputDoubleRegister(1));
      DCHECK_EQ(LeaveCC, i.OutputSBit());
      break;
    case kArmVmlaF64:
      __ vmla(i.OutputDoubleRegister(), i.InputDoubleRegister(1),
              i.InputDoubleRegister(2));
      DCHECK_EQ(LeaveCC, i.OutputSBit());
      break;
    case kArmVmlsF64:
      __ vmls(i.OutputDoubleRegister(), i.InputDoubleRegister(1),
              i.InputDoubleRegister(2));
      DCHECK_EQ(LeaveCC, i.OutputSBit());
      break;
    case kArmVdivF64:
      __ vdiv(i.OutputDoubleRegister(), i.InputDoubleRegister(0),
              i.InputDoubleRegister(1));
      DCHECK_EQ(LeaveCC, i.OutputSBit());
      break;
    case kArmVmodF64: {
      // TODO(bmeurer): We should really get rid of this special instruction,
      // and generate a CallAddress instruction instead.
      FrameScope scope(tasm(), StackFrame::MANUAL);
      __ PrepareCallCFunction(0, 2);
      __ MovToFloatParameters(i.InputDoubleRegister(0),
                              i.InputDoubleRegister(1));
      __ CallCFunction(ExternalReference::mod_two_doubles_operation(), 0, 2);
      // Move the result in the double result register.
      __ MovFromFloatResult(i.OutputDoubleRegister());
      DCHECK_EQ(LeaveCC, i.OutputSBit());
      break;
    }
    case kArmVsqrtF64:
      __ vsqrt(i.OutputDoubleRegister(), i.InputDoubleRegister(0));
      break;
    case kArmVabsF64:
      __ vabs(i.OutputDoubleRegister(), i.InputDoubleRegister(0));
      break;
    case kArmVnegF64:
      __ vneg(i.OutputDoubleRegister(), i.InputDoubleRegister(0));
      break;
    case kArmVrintmF32: {
      CpuFeatureScope scope(tasm(), ARMv8);
      __ vrintm(i.OutputFloatRegister(), i.InputFloatRegister(0));
      break;
    }
    case kArmVrintmF64: {
      CpuFeatureScope scope(tasm(), ARMv8);
      __ vrintm(i.OutputDoubleRegister(), i.InputDoubleRegister(0));
      break;
    }
    case kArmVrintpF32: {
      CpuFeatureScope scope(tasm(), ARMv8);
      __ vrintp(i.OutputFloatRegister(), i.InputFloatRegister(0));
      break;
    }
    case kArmVrintpF64: {
      CpuFeatureScope scope(tasm(), ARMv8);
      __ vrintp(i.OutputDoubleRegister(), i.InputDoubleRegister(0));
      break;
    }
    case kArmVrintzF32: {
      CpuFeatureScope scope(tasm(), ARMv8);
      __ vrintz(i.OutputFloatRegister(), i.InputFloatRegister(0));
      break;
    }
    case kArmVrintzF64: {
      CpuFeatureScope scope(tasm(), ARMv8);
      __ vrintz(i.OutputDoubleRegister(), i.InputDoubleRegister(0));
      break;
    }
    case kArmVrintaF64: {
      CpuFeatureScope scope(tasm(), ARMv8);
      __ vrinta(i.OutputDoubleRegister(), i.InputDoubleRegister(0));
      break;
    }
    case kArmVrintnF32: {
      CpuFeatureScope scope(tasm(), ARMv8);
      __ vrintn(i.OutputFloatRegister(), i.InputFloatRegister(0));
      break;
    }
    case kArmVrintnF64: {
      CpuFeatureScope scope(tasm(), ARMv8);
      __ vrintn(i.OutputDoubleRegister(), i.InputDoubleRegister(0));
      break;
    }
    case kArmVcvtF32F64: {
      __ vcvt_f32_f64(i.OutputFloatRegister(), i.InputDoubleRegister(0));
      DCHECK_EQ(LeaveCC, i.OutputSBit());
      break;
    }
    case kArmVcvtF64F32: {
      __ vcvt_f64_f32(i.OutputDoubleRegister(), i.InputFloatRegister(0));
      DCHECK_EQ(LeaveCC, i.OutputSBit());
      break;
    }
    case kArmVcvtF32S32: {
      UseScratchRegisterScope temps(tasm());
      SwVfpRegister scratch = temps.AcquireS();
      __ vmov(scratch, i.InputRegister(0));
      __ vcvt_f32_s32(i.OutputFloatRegister(), scratch);
      DCHECK_EQ(LeaveCC, i.OutputSBit());
      break;
    }
    case kArmVcvtF32U32: {
      UseScratchRegisterScope temps(tasm());
      SwVfpRegister scratch = temps.AcquireS();
      __ vmov(scratch, i.InputRegister(0));
      __ vcvt_f32_u32(i.OutputFloatRegister(), scratch);
      DCHECK_EQ(LeaveCC, i.OutputSBit());
      break;
    }
    case kArmVcvtF64S32: {
      UseScratchRegisterScope temps(tasm());
      SwVfpRegister scratch = temps.AcquireS();
      __ vmov(scratch, i.InputRegister(0));
      __ vcvt_f64_s32(i.OutputDoubleRegister(), scratch);
      DCHECK_EQ(LeaveCC, i.OutputSBit());
      break;
    }
    case kArmVcvtF64U32: {
      UseScratchRegisterScope temps(tasm());
      SwVfpRegister scratch = temps.AcquireS();
      __ vmov(scratch, i.InputRegister(0));
      __ vcvt_f64_u32(i.OutputDoubleRegister(), scratch);
      DCHECK_EQ(LeaveCC, i.OutputSBit());
      break;
    }
    case kArmVcvtS32F32: {
      UseScratchRegisterScope temps(tasm());
      SwVfpRegister scratch = temps.AcquireS();
      __ vcvt_s32_f32(scratch, i.InputFloatRegister(0));
      __ vmov(i.OutputRegister(), scratch);
      // Avoid INT32_MAX as an overflow indicator and use INT32_MIN instead,
      // because INT32_MIN allows easier out-of-bounds detection.
      __ cmn(i.OutputRegister(), Operand(1));
      __ mov(i.OutputRegister(), Operand(INT32_MIN), SBit::LeaveCC, vs);
      DCHECK_EQ(LeaveCC, i.OutputSBit());
      break;
    }
    case kArmVcvtU32F32: {
      UseScratchRegisterScope temps(tasm());
      SwVfpRegister scratch = temps.AcquireS();
      __ vcvt_u32_f32(scratch, i.InputFloatRegister(0));
      __ vmov(i.OutputRegister(), scratch);
      // Avoid UINT32_MAX as an overflow indicator and use 0 instead,
      // because 0 allows easier out-of-bounds detection.
      __ cmn(i.OutputRegister(), Operand(1));
      __ adc(i.OutputRegister(), i.OutputRegister(), Operand::Zero());
      DCHECK_EQ(LeaveCC, i.OutputSBit());
      break;
    }
    case kArmVcvtS32F64: {
      UseScratchRegisterScope temps(tasm());
      SwVfpRegister scratch = temps.AcquireS();
      __ vcvt_s32_f64(scratch, i.InputDoubleRegister(0));
      __ vmov(i.OutputRegister(), scratch);
      DCHECK_EQ(LeaveCC, i.OutputSBit());
      break;
    }
    case kArmVcvtU32F64: {
      UseScratchRegisterScope temps(tasm());
      SwVfpRegister scratch = temps.AcquireS();
      __ vcvt_u32_f64(scratch, i.InputDoubleRegister(0));
      __ vmov(i.OutputRegister(), scratch);
      DCHECK_EQ(LeaveCC, i.OutputSBit());
      break;
    }
    case kArmVmovU32F32:
      __ vmov(i.OutputRegister(), i.InputFloatRegister(0));
      DCHECK_EQ(LeaveCC, i.OutputSBit());
      break;
    case kArmVmovF32U32:
      __ vmov(i.OutputFloatRegister(), i.InputRegister(0));
      DCHECK_EQ(LeaveCC, i.OutputSBit());
      break;
    case kArmVmovLowU32F64:
      __ VmovLow(i.OutputRegister(), i.InputDoubleRegister(0));
      DCHECK_EQ(LeaveCC, i.OutputSBit());
      break;
    case kArmVmovLowF64U32:
      __ VmovLow(i.OutputDoubleRegister(), i.InputRegister(1));
      DCHECK_EQ(LeaveCC, i.OutputSBit());
      break;
    case kArmVmovHighU32F64:
      __ VmovHigh(i.OutputRegister(), i.InputDoubleRegister(0));
      DCHECK_EQ(LeaveCC, i.OutputSBit());
      break;
    case kArmVmovHighF64U32:
      __ VmovHigh(i.OutputDoubleRegister(), i.InputRegister(1));
      DCHECK_EQ(LeaveCC, i.OutputSBit());
      break;
    case kArmVmovF64U32U32:
      __ vmov(i.OutputDoubleRegister(), i.InputRegister(0), i.InputRegister(1));
      DCHECK_EQ(LeaveCC, i.OutputSBit());
      break;
    case kArmVmovU32U32F64:
      __ vmov(i.OutputRegister(0), i.OutputRegister(1),
              i.InputDoubleRegister(0));
      DCHECK_EQ(LeaveCC, i.OutputSBit());
      break;
    case kArmLdrb:
      __ ldrb(i.OutputRegister(), i.InputOffset());
      DCHECK_EQ(LeaveCC, i.OutputSBit());
      EmitWordLoadPoisoningIfNeeded(this, opcode, i);
      break;
    case kArmLdrsb:
      __ ldrsb(i.OutputRegister(), i.InputOffset());
      DCHECK_EQ(LeaveCC, i.OutputSBit());
      EmitWordLoadPoisoningIfNeeded(this, opcode, i);
      break;
    case kArmStrb:
      __ strb(i.InputRegister(0), i.InputOffset(1));
      DCHECK_EQ(LeaveCC, i.OutputSBit());
      break;
    case kArmLdrh:
      __ ldrh(i.OutputRegister(), i.InputOffset());
      EmitWordLoadPoisoningIfNeeded(this, opcode, i);
      break;
    case kArmLdrsh:
      __ ldrsh(i.OutputRegister(), i.InputOffset());
      EmitWordLoadPoisoningIfNeeded(this, opcode, i);
      break;
    case kArmStrh:
      __ strh(i.InputRegister(0), i.InputOffset(1));
      DCHECK_EQ(LeaveCC, i.OutputSBit());
      break;
    case kArmLdr:
      __ ldr(i.OutputRegister(), i.InputOffset());
      EmitWordLoadPoisoningIfNeeded(this, opcode, i);
      break;
    case kArmStr:
      __ str(i.InputRegister(0), i.InputOffset(1));
      DCHECK_EQ(LeaveCC, i.OutputSBit());
      break;
    case kArmVldrF32: {
      const MemoryAccessMode access_mode =
          static_cast<MemoryAccessMode>(MiscField::decode(opcode));
      if (access_mode == kMemoryAccessPoisoned) {
        UseScratchRegisterScope temps(tasm());
        Register address = temps.Acquire();
        ComputePoisonedAddressForLoad(this, opcode, i, address);
        __ vldr(i.OutputFloatRegister(), address, 0);
      } else {
        __ vldr(i.OutputFloatRegister(), i.InputOffset());
      }
      DCHECK_EQ(LeaveCC, i.OutputSBit());
      break;
    }
    case kArmVstrF32:
      __ vstr(i.InputFloatRegister(0), i.InputOffset(1));
      DCHECK_EQ(LeaveCC, i.OutputSBit());
      break;
    case kArmVld1F64: {
      __ vld1(Neon8, NeonListOperand(i.OutputDoubleRegister()),
              i.NeonInputOperand(0));
      break;
    }
    case kArmVst1F64: {
      __ vst1(Neon8, NeonListOperand(i.InputDoubleRegister(0)),
              i.NeonInputOperand(1));
      break;
    }
    case kArmVld1S128: {
      __ vld1(Neon8, NeonListOperand(i.OutputSimd128Register()),
              i.NeonInputOperand(0));
      break;
    }
    case kArmVst1S128: {
      __ vst1(Neon8, NeonListOperand(i.InputSimd128Register(0)),
              i.NeonInputOperand(1));
      break;
    }
    case kArmVldrF64: {
      const MemoryAccessMode access_mode =
          static_cast<MemoryAccessMode>(MiscField::decode(opcode));
      if (access_mode == kMemoryAccessPoisoned) {
        UseScratchRegisterScope temps(tasm());
        Register address = temps.Acquire();
        ComputePoisonedAddressForLoad(this, opcode, i, address);
        __ vldr(i.OutputDoubleRegister(), address, 0);
      } else {
        __ vldr(i.OutputDoubleRegister(), i.InputOffset());
      }
      DCHECK_EQ(LeaveCC, i.OutputSBit());
      break;
    }
    case kArmVstrF64:
      __ vstr(i.InputDoubleRegister(0), i.InputOffset(1));
      DCHECK_EQ(LeaveCC, i.OutputSBit());
      break;
    case kArmFloat32Max: {
      SwVfpRegister result = i.OutputFloatRegister();
      SwVfpRegister left = i.InputFloatRegister(0);
      SwVfpRegister right = i.InputFloatRegister(1);
      if (left == right) {
        __ Move(result, left);
      } else {
        auto ool = new (zone()) OutOfLineFloat32Max(this, result, left, right);
        __ FloatMax(result, left, right, ool->entry());
        __ bind(ool->exit());
      }
      DCHECK_EQ(LeaveCC, i.OutputSBit());
      break;
    }
    case kArmFloat64Max: {
      DwVfpRegister result = i.OutputDoubleRegister();
      DwVfpRegister left = i.InputDoubleRegister(0);
      DwVfpRegister right = i.InputDoubleRegister(1);
      if (left == right) {
        __ Move(result, left);
      } else {
        auto ool = new (zone()) OutOfLineFloat64Max(this, result, left, right);
        __ FloatMax(result, left, right, ool->entry());
        __ bind(ool->exit());
      }
      DCHECK_EQ(LeaveCC, i.OutputSBit());
      break;
    }
    case kArmFloat32Min: {
      SwVfpRegister result = i.OutputFloatRegister();
      SwVfpRegister left = i.InputFloatRegister(0);
      SwVfpRegister right = i.InputFloatRegister(1);
      if (left == right) {
        __ Move(result, left);
      } else {
        auto ool = new (zone()) OutOfLineFloat32Min(this, result, left, right);
        __ FloatMin(result, left, right, ool->entry());
        __ bind(ool->exit());
      }
      DCHECK_EQ(LeaveCC, i.OutputSBit());
      break;
    }
    case kArmFloat64Min: {
      DwVfpRegister result = i.OutputDoubleRegister();
      DwVfpRegister left = i.InputDoubleRegister(0);
      DwVfpRegister right = i.InputDoubleRegister(1);
      if (left == right) {
        __ Move(result, left);
      } else {
        auto ool = new (zone()) OutOfLineFloat64Min(this, result, left, right);
        __ FloatMin(result, left, right, ool->entry());
        __ bind(ool->exit());
      }
      DCHECK_EQ(LeaveCC, i.OutputSBit());
      break;
    }
    case kArmFloat64SilenceNaN: {
      DwVfpRegister value = i.InputDoubleRegister(0);
      DwVfpRegister result = i.OutputDoubleRegister();
      __ VFPCanonicalizeNaN(result, value);
      break;
    }
    case kArmPush:
      if (instr->InputAt(0)->IsFPRegister()) {
        LocationOperand* op = LocationOperand::cast(instr->InputAt(0));
        switch (op->representation()) {
          case MachineRepresentation::kFloat32:
            __ vpush(i.InputFloatRegister(0));
            frame_access_state()->IncreaseSPDelta(1);
            break;
          case MachineRepresentation::kFloat64:
            __ vpush(i.InputDoubleRegister(0));
            frame_access_state()->IncreaseSPDelta(kDoubleSize / kPointerSize);
            break;
          case MachineRepresentation::kSimd128: {
            __ vpush(i.InputSimd128Register(0));
            frame_access_state()->IncreaseSPDelta(kSimd128Size / kPointerSize);
            break;
          }
          default:
            UNREACHABLE();
            break;
        }
      } else {
        __ push(i.InputRegister(0));
        frame_access_state()->IncreaseSPDelta(1);
      }
      DCHECK_EQ(LeaveCC, i.OutputSBit());
      break;
    case kArmPoke: {
      int const slot = MiscField::decode(instr->opcode());
      __ str(i.InputRegister(0), MemOperand(sp, slot * kPointerSize));
      DCHECK_EQ(LeaveCC, i.OutputSBit());
      break;
    }
    case kArmPeek: {
      // The incoming value is 0-based, but we need a 1-based value.
      int reverse_slot = i.InputInt32(0) + 1;
      int offset =
          FrameSlotToFPOffset(frame()->GetTotalFrameSlotCount() - reverse_slot);
      if (instr->OutputAt(0)->IsFPRegister()) {
        LocationOperand* op = LocationOperand::cast(instr->OutputAt(0));
        if (op->representation() == MachineRepresentation::kFloat64) {
          __ vldr(i.OutputDoubleRegister(), MemOperand(fp, offset));
        } else {
          DCHECK_EQ(MachineRepresentation::kFloat32, op->representation());
          __ vldr(i.OutputFloatRegister(), MemOperand(fp, offset));
        }
      } else {
        __ ldr(i.OutputRegister(), MemOperand(fp, offset));
      }
      break;
    }
    case kArmDsbIsb: {
      __ dsb(SY);
      __ isb(SY);
      break;
    }
    case kArchWordPoisonOnSpeculation:
      __ and_(i.OutputRegister(0), i.InputRegister(0),
              Operand(kSpeculationPoisonRegister));
      break;
    case kArmF32x4Splat: {
      int src_code = i.InputFloatRegister(0).code();
      __ vdup(Neon32, i.OutputSimd128Register(),
              DwVfpRegister::from_code(src_code / 2), src_code % 2);
      break;
    }
    case kArmF32x4ExtractLane: {
      __ ExtractLane(i.OutputFloatRegister(), i.InputSimd128Register(0),
                     i.InputInt8(1));
      break;
    }
    case kArmF32x4ReplaceLane: {
      __ ReplaceLane(i.OutputSimd128Register(), i.InputSimd128Register(0),
                     i.InputFloatRegister(2), i.InputInt8(1));
      break;
    }
    case kArmF32x4SConvertI32x4: {
      __ vcvt_f32_s32(i.OutputSimd128Register(), i.InputSimd128Register(0));
      break;
    }
    case kArmF32x4UConvertI32x4: {
      __ vcvt_f32_u32(i.OutputSimd128Register(), i.InputSimd128Register(0));
      break;
    }
    case kArmF32x4Abs: {
      __ vabs(i.OutputSimd128Register(), i.InputSimd128Register(0));
      break;
    }
    case kArmF32x4Neg: {
      __ vneg(i.OutputSimd128Register(), i.InputSimd128Register(0));
      break;
    }
    case kArmF32x4RecipApprox: {
      __ vrecpe(i.OutputSimd128Register(), i.InputSimd128Register(0));
      break;
    }
    case kArmF32x4RecipSqrtApprox: {
      __ vrsqrte(i.OutputSimd128Register(), i.InputSimd128Register(0));
      break;
    }
    case kArmF32x4Add: {
      __ vadd(i.OutputSimd128Register(), i.InputSimd128Register(0),
              i.InputSimd128Register(1));
      break;
    }
    case kArmF32x4AddHoriz: {
      Simd128Register dst = i.OutputSimd128Register(),
                      src0 = i.InputSimd128Register(0),
                      src1 = i.InputSimd128Register(1);
      // Make sure we don't overwrite source data before it's used.
      if (dst == src0) {
        __ vpadd(dst.low(), src0.low(), src0.high());
        if (dst == src1) {
          __ vmov(dst.high(), dst.low());
        } else {
          __ vpadd(dst.high(), src1.low(), src1.high());
        }
      } else {
        __ vpadd(dst.high(), src1.low(), src1.high());
        __ vpadd(dst.low(), src0.low(), src0.high());
      }
      break;
    }
    case kArmF32x4Sub: {
      __ vsub(i.OutputSimd128Register(), i.InputSimd128Register(0),
              i.InputSimd128Register(1));
      break;
    }
    case kArmF32x4Mul: {
      __ vmul(i.OutputSimd128Register(), i.InputSimd128Register(0),
              i.InputSimd128Register(1));
      break;
    }
    case kArmF32x4Min: {
      __ vmin(i.OutputSimd128Register(), i.InputSimd128Register(0),
              i.InputSimd128Register(1));
      break;
    }
    case kArmF32x4Max: {
      __ vmax(i.OutputSimd128Register(), i.InputSimd128Register(0),
              i.InputSimd128Register(1));
      break;
    }
    case kArmF32x4Eq: {
      __ vceq(i.OutputSimd128Register(), i.InputSimd128Register(0),
              i.InputSimd128Register(1));
      break;
    }
    case kArmF32x4Ne: {
      Simd128Register dst = i.OutputSimd128Register();
      __ vceq(dst, i.InputSimd128Register(0), i.InputSimd128Register(1));
      __ vmvn(dst, dst);
      break;
    }
    case kArmF32x4Lt: {
      __ vcgt(i.OutputSimd128Register(), i.InputSimd128Register(1),
              i.InputSimd128Register(0));
      break;
    }
    case kArmF32x4Le: {
      __ vcge(i.OutputSimd128Register(), i.InputSimd128Register(1),
              i.InputSimd128Register(0));
      break;
    }
    case kArmI32x4Splat: {
      __ vdup(Neon32, i.OutputSimd128Register(), i.InputRegister(0));
      break;
    }
    case kArmI32x4ExtractLane: {
      __ ExtractLane(i.OutputRegister(), i.InputSimd128Register(0), NeonS32,
                     i.InputInt8(1));
      break;
    }
    case kArmI32x4ReplaceLane: {
      __ ReplaceLane(i.OutputSimd128Register(), i.InputSimd128Register(0),
                     i.InputRegister(2), NeonS32, i.InputInt8(1));
      break;
    }
    case kArmI32x4SConvertF32x4: {
      __ vcvt_s32_f32(i.OutputSimd128Register(), i.InputSimd128Register(0));
      break;
    }
    case kArmI32x4SConvertI16x8Low: {
      __ vmovl(NeonS16, i.OutputSimd128Register(),
               i.InputSimd128Register(0).low());
      break;
    }
    case kArmI32x4SConvertI16x8High: {
      __ vmovl(NeonS16, i.OutputSimd128Register(),
               i.InputSimd128Register(0).high());
      break;
    }
    case kArmI32x4Neg: {
      __ vneg(Neon32, i.OutputSimd128Register(), i.InputSimd128Register(0));
      break;
    }
    case kArmI32x4Shl: {
      __ vshl(NeonS32, i.OutputSimd128Register(), i.InputSimd128Register(0),
              i.InputInt5(1));
      break;
    }
    case kArmI32x4ShrS: {
      __ vshr(NeonS32, i.OutputSimd128Register(), i.InputSimd128Register(0),
              i.InputInt5(1));
      break;
    }
    case kArmI32x4Add: {
      __ vadd(Neon32, i.OutputSimd128Register(), i.InputSimd128Register(0),
              i.InputSimd128Register(1));
      break;
    }
    case kArmI32x4AddHoriz:
      ASSEMBLE_NEON_PAIRWISE_OP(vpadd, Neon32);
      break;
    case kArmI32x4Sub: {
      __ vsub(Neon32, i.OutputSimd128Register(), i.InputSimd128Register(0),
              i.InputSimd128Register(1));
      break;
    }
    case kArmI32x4Mul: {
      __ vmul(Neon32, i.OutputSimd128Register(), i.InputSimd128Register(0),
              i.InputSimd128Register(1));
      break;
    }
    case kArmI32x4MinS: {
      __ vmin(NeonS32, i.OutputSimd128Register(), i.InputSimd128Register(0),
              i.InputSimd128Register(1));
      break;
    }
    case kArmI32x4MaxS: {
      __ vmax(NeonS32, i.OutputSimd128Register(), i.InputSimd128Register(0),
              i.InputSimd128Register(1));
      break;
    }
    case kArmI32x4Eq: {
      __ vceq(Neon32, i.OutputSimd128Register(), i.InputSimd128Register(0),
              i.InputSimd128Register(1));
      break;
    }
    case kArmI32x4Ne: {
      Simd128Register dst = i.OutputSimd128Register();
      __ vceq(Neon32, dst, i.InputSimd128Register(0),
              i.InputSimd128Register(1));
      __ vmvn(dst, dst);
      break;
    }
    case kArmI32x4GtS: {
      __ vcgt(NeonS32, i.OutputSimd128Register(), i.InputSimd128Register(0),
              i.InputSimd128Register(1));
      break;
    }
    case kArmI32x4GeS: {
      __ vcge(NeonS32, i.OutputSimd128Register(), i.InputSimd128Register(0),
              i.InputSimd128Register(1));
      break;
    }
    case kArmI32x4UConvertF32x4: {
      __ vcvt_u32_f32(i.OutputSimd128Register(), i.InputSimd128Register(0));
      break;
    }
    case kArmI32x4UConvertI16x8Low: {
      __ vmovl(NeonU16, i.OutputSimd128Register(),
               i.InputSimd128Register(0).low());
      break;
    }
    case kArmI32x4UConvertI16x8High: {
      __ vmovl(NeonU16, i.OutputSimd128Register(),
               i.InputSimd128Register(0).high());
      break;
    }
    case kArmI32x4ShrU: {
      __ vshr(NeonU32, i.OutputSimd128Register(), i.InputSimd128Register(0),
              i.InputInt5(1));
      break;
    }
    case kArmI32x4MinU: {
      __ vmin(NeonU32, i.OutputSimd128Register(), i.InputSimd128Register(0),
              i.InputSimd128Register(1));
      break;
    }
    case kArmI32x4MaxU: {
      __ vmax(NeonU32, i.OutputSimd128Register(), i.InputSimd128Register(0),
              i.InputSimd128Register(1));
      break;
    }
    case kArmI32x4GtU: {
      __ vcgt(NeonU32, i.OutputSimd128Register(), i.InputSimd128Register(0),
              i.InputSimd128Register(1));
      break;
    }
    case kArmI32x4GeU: {
      __ vcge(NeonU32, i.OutputSimd128Register(), i.InputSimd128Register(0),
              i.InputSimd128Register(1));
      break;
    }
    case kArmI16x8Splat: {
      __ vdup(Neon16, i.OutputSimd128Register(), i.InputRegister(0));
      break;
    }
    case kArmI16x8ExtractLane: {
      __ ExtractLane(i.OutputRegister(), i.InputSimd128Register(0), NeonS16,
                     i.InputInt8(1));
      break;
    }
    case kArmI16x8ReplaceLane: {
      __ ReplaceLane(i.OutputSimd128Register(), i.InputSimd128Register(0),
                     i.InputRegister(2), NeonS16, i.InputInt8(1));
      break;
    }
    case kArmI16x8SConvertI8x16Low: {
      __ vmovl(NeonS8, i.OutputSimd128Register(),
               i.InputSimd128Register(0).low());
      break;
    }
    case kArmI16x8SConvertI8x16High: {
      __ vmovl(NeonS8, i.OutputSimd128Register(),
               i.InputSimd128Register(0).high());
      break;
    }
    case kArmI16x8Neg: {
      __ vneg(Neon16, i.OutputSimd128Register(), i.InputSimd128Register(0));
      break;
    }
    case kArmI16x8Shl: {
      __ vshl(NeonS16, i.OutputSimd128Register(), i.InputSimd128Register(0),
              i.InputInt4(1));
      break;
    }
    case kArmI16x8ShrS: {
      __ vshr(NeonS16, i.OutputSimd128Register(), i.InputSimd128Register(0),
              i.InputInt4(1));
      break;
    }
    case kArmI16x8SConvertI32x4:
      ASSEMBLE_NEON_NARROWING_OP(NeonS16);
      break;
    case kArmI16x8Add: {
      __ vadd(Neon16, i.OutputSimd128Register(), i.InputSimd128Register(0),
              i.InputSimd128Register(1));
      break;
    }
    case kArmI16x8AddSaturateS: {
      __ vqadd(NeonS16, i.OutputSimd128Register(), i.InputSimd128Register(0),
               i.InputSimd128Register(1));
      break;
    }
    case kArmI16x8AddHoriz:
      ASSEMBLE_NEON_PAIRWISE_OP(vpadd, Neon16);
      break;
    case kArmI16x8Sub: {
      __ vsub(Neon16, i.OutputSimd128Register(), i.InputSimd128Register(0),
              i.InputSimd128Register(1));
      break;
    }
    case kArmI16x8SubSaturateS: {
      __ vqsub(NeonS16, i.OutputSimd128Register(), i.InputSimd128Register(0),
               i.InputSimd128Register(1));
      break;
    }
    case kArmI16x8Mul: {
      __ vmul(Neon16, i.OutputSimd128Register(), i.InputSimd128Register(0),
              i.InputSimd128Register(1));
      break;
    }
    case kArmI16x8MinS: {
      __ vmin(NeonS16, i.OutputSimd128Register(), i.InputSimd128Register(0),
              i.InputSimd128Register(1));
      break;
    }
    case kArmI16x8MaxS: {
      __ vmax(NeonS16, i.OutputSimd128Register(), i.InputSimd128Register(0),
              i.InputSimd128Register(1));
      break;
    }
    case kArmI16x8Eq: {
      __ vceq(Neon16, i.OutputSimd128Register(), i.InputSimd128Register(0),
              i.InputSimd128Register(1));
      break;
    }
    case kArmI16x8Ne: {
      Simd128Register dst = i.OutputSimd128Register();
      __ vceq(Neon16, dst, i.InputSimd128Register(0),
              i.InputSimd128Register(1));
      __ vmvn(dst, dst);
      break;
    }
    case kArmI16x8GtS: {
      __ vcgt(NeonS16, i.OutputSimd128Register(), i.InputSimd128Register(0),
              i.InputSimd128Register(1));
      break;
    }
    case kArmI16x8GeS: {
      __ vcge(NeonS16, i.OutputSimd128Register(), i.InputSimd128Register(0),
              i.InputSimd128Register(1));
      break;
    }
    case kArmI16x8UConvertI8x16Low: {
      __ vmovl(NeonU8, i.OutputSimd128Register(),
               i.InputSimd128Register(0).low());
      break;
    }
    case kArmI16x8UConvertI8x16High: {
      __ vmovl(NeonU8, i.OutputSimd128Register(),
               i.InputSimd128Register(0).high());
      break;
    }
    case kArmI16x8ShrU: {
      __ vshr(NeonU16, i.OutputSimd128Register(), i.InputSimd128Register(0),
              i.InputInt4(1));
      break;
    }
    case kArmI16x8UConvertI32x4:
      ASSEMBLE_NEON_NARROWING_OP(NeonU16);
      break;
    case kArmI16x8AddSaturateU: {
      __ vqadd(NeonU16, i.OutputSimd128Register(), i.InputSimd128Register(0),
               i.InputSimd128Register(1));
      break;
    }
    case kArmI16x8SubSaturateU: {
      __ vqsub(NeonU16, i.OutputSimd128Register(), i.InputSimd128Register(0),
               i.InputSimd128Register(1));
      break;
    }
    case kArmI16x8MinU: {
      __ vmin(NeonU16, i.OutputSimd128Register(), i.InputSimd128Register(0),
              i.InputSimd128Register(1));
      break;
    }
    case kArmI16x8MaxU: {
      __ vmax(NeonU16, i.OutputSimd128Register(), i.InputSimd128Register(0),
              i.InputSimd128Register(1));
      break;
    }
    case kArmI16x8GtU: {
      __ vcgt(NeonU16, i.OutputSimd128Register(), i.InputSimd128Register(0),
              i.InputSimd128Register(1));
      break;
    }
    case kArmI16x8GeU: {
      __ vcge(NeonU16, i.OutputSimd128Register(), i.InputSimd128Register(0),
              i.InputSimd128Register(1));
      break;
    }
    case kArmI8x16Splat: {
      __ vdup(Neon8, i.OutputSimd128Register(), i.InputRegister(0));
      break;
    }
    case kArmI8x16ExtractLane: {
      __ ExtractLane(i.OutputRegister(), i.InputSimd128Register(0), NeonS8,
                     i.InputInt8(1));
      break;
    }
    case kArmI8x16ReplaceLane: {
      __ ReplaceLane(i.OutputSimd128Register(), i.InputSimd128Register(0),
                     i.InputRegister(2), NeonS8, i.InputInt8(1));
      break;
    }
    case kArmI8x16Neg: {
      __ vneg(Neon8, i.OutputSimd128Register(), i.InputSimd128Register(0));
      break;
    }
    case kArmI8x16Shl: {
      __ vshl(NeonS8, i.OutputSimd128Register(), i.InputSimd128Register(0),
              i.InputInt3(1));
      break;
    }
    case kArmI8x16ShrS: {
      __ vshr(NeonS8, i.OutputSimd128Register(), i.InputSimd128Register(0),
              i.InputInt3(1));
      break;
    }
    case kArmI8x16SConvertI16x8:
      ASSEMBLE_NEON_NARROWING_OP(NeonS8);
      break;
    case kArmI8x16Add: {
      __ vadd(Neon8, i.OutputSimd128Register(), i.InputSimd128Register(0),
              i.InputSimd128Register(1));
      break;
    }
    case kArmI8x16AddSaturateS: {
      __ vqadd(NeonS8, i.OutputSimd128Register(), i.InputSimd128Register(0),
               i.InputSimd128Register(1));
      break;
    }
    case kArmI8x16Sub: {
      __ vsub(Neon8, i.OutputSimd128Register(), i.InputSimd128Register(0),
              i.InputSimd128Register(1));
      break;
    }
    case kArmI8x16SubSaturateS: {
      __ vqsub(NeonS8, i.OutputSimd128Register(), i.InputSimd128Register(0),
               i.InputSimd128Register(1));
      break;
    }
    case kArmI8x16Mul: {
      __ vmul(Neon8, i.OutputSimd128Register(), i.InputSimd128Register(0),
              i.InputSimd128Register(1));
      break;
    }
    case kArmI8x16MinS: {
      __ vmin(NeonS8, i.OutputSimd128Register(), i.InputSimd128Register(0),
              i.InputSimd128Register(1));
      break;
    }
    case kArmI8x16MaxS: {
      __ vmax(NeonS8, i.OutputSimd128Register(), i.InputSimd128Register(0),
              i.InputSimd128Register(1));
      break;
    }
    case kArmI8x16Eq: {
      __ vceq(Neon8, i.OutputSimd128Register(), i.InputSimd128Register(0),
              i.InputSimd128Register(1));
      break;
    }
    case kArmI8x16Ne: {
      Simd128Register dst = i.OutputSimd128Register();
      __ vceq(Neon8, dst, i.InputSimd128Register(0), i.InputSimd128Register(1));
      __ vmvn(dst, dst);
      break;
    }
    case kArmI8x16GtS: {
      __ vcgt(NeonS8, i.OutputSimd128Register(), i.InputSimd128Register(0),
              i.InputSimd128Register(1));
      break;
    }
    case kArmI8x16GeS: {
      __ vcge(NeonS8, i.OutputSimd128Register(), i.InputSimd128Register(0),
              i.InputSimd128Register(1));
      break;
    }
    case kArmI8x16ShrU: {
      __ vshr(NeonU8, i.OutputSimd128Register(), i.InputSimd128Register(0),
              i.InputInt3(1));
      break;
    }
    case kArmI8x16UConvertI16x8:
      ASSEMBLE_NEON_NARROWING_OP(NeonU8);
      break;
    case kArmI8x16AddSaturateU: {
      __ vqadd(NeonU8, i.OutputSimd128Register(), i.InputSimd128Register(0),
               i.InputSimd128Register(1));
      break;
    }
    case kArmI8x16SubSaturateU: {
      __ vqsub(NeonU8, i.OutputSimd128Register(), i.InputSimd128Register(0),
               i.InputSimd128Register(1));
      break;
    }
    case kArmI8x16MinU: {
      __ vmin(NeonU8, i.OutputSimd128Register(), i.InputSimd128Register(0),
              i.InputSimd128Register(1));
      break;
    }
    case kArmI8x16MaxU: {
      __ vmax(NeonU8, i.OutputSimd128Register(), i.InputSimd128Register(0),
              i.InputSimd128Register(1));
      break;
    }
    case kArmI8x16GtU: {
      __ vcgt(NeonU8, i.OutputSimd128Register(), i.InputSimd128Register(0),
              i.InputSimd128Register(1));
      break;
    }
    case kArmI8x16GeU: {
      __ vcge(NeonU8, i.OutputSimd128Register(), i.InputSimd128Register(0),
              i.InputSimd128Register(1));
      break;
    }
    case kArmS128Zero: {
      __ veor(i.OutputSimd128Register(), i.OutputSimd128Register(),
              i.OutputSimd128Register());
      break;
    }
    case kArmS128Dup: {
      NeonSize size = static_cast<NeonSize>(i.InputInt32(1));
      int lanes = kSimd128Size >> size;
      int index = i.InputInt32(2);
      DCHECK(index < lanes);
      int d_lanes = lanes / 2;
      int src_d_index = index & (d_lanes - 1);
      int src_d_code = i.InputSimd128Register(0).low().code() + index / d_lanes;
      __ vdup(size, i.OutputSimd128Register(),
              DwVfpRegister::from_code(src_d_code), src_d_index);
      break;
    }
    case kArmS128And: {
      __ vand(i.OutputSimd128Register(), i.InputSimd128Register(0),
              i.InputSimd128Register(1));
      break;
    }
    case kArmS128Or: {
      __ vorr(i.OutputSimd128Register(), i.InputSimd128Register(0),
              i.InputSimd128Register(1));
      break;
    }
    case kArmS128Xor: {
      __ veor(i.OutputSimd128Register(), i.InputSimd128Register(0),
              i.InputSimd128Register(1));
      break;
    }
    case kArmS128Not: {
      __ vmvn(i.OutputSimd128Register(), i.InputSimd128Register(0));
      break;
    }
    case kArmS128Select: {
      Simd128Register dst = i.OutputSimd128Register();
      DCHECK(dst == i.InputSimd128Register(0));
      __ vbsl(dst, i.InputSimd128Register(1), i.InputSimd128Register(2));
      break;
    }
    case kArmS32x4ZipLeft: {
      Simd128Register dst = i.OutputSimd128Register(),
                      src1 = i.InputSimd128Register(1);
      DCHECK(dst == i.InputSimd128Register(0));
      // src0 = [0, 1, 2, 3], src1 = [4, 5, 6, 7]
      __ vmov(dst.high(), src1.low());         // dst = [0, 1, 4, 5]
      __ vtrn(Neon32, dst.low(), dst.high());  // dst = [0, 4, 1, 5]
      break;
    }
    case kArmS32x4ZipRight: {
      Simd128Register dst = i.OutputSimd128Register(),
                      src1 = i.InputSimd128Register(1);
      DCHECK(dst == i.InputSimd128Register(0));
      // src0 = [4, 5, 6, 7], src1 = [0, 1, 2, 3] (flipped from ZipLeft).
      __ vmov(dst.low(), src1.high());         // dst = [2, 3, 6, 7]
      __ vtrn(Neon32, dst.low(), dst.high());  // dst = [2, 6, 3, 7]
      break;
    }
    case kArmS32x4UnzipLeft: {
      Simd128Register dst = i.OutputSimd128Register(),
                      src1 = i.InputSimd128Register(1);
      DCHECK(dst == i.InputSimd128Register(0));
      UseScratchRegisterScope temps(tasm());
      Simd128Register scratch = temps.AcquireQ();
      // src0 = [0, 1, 2, 3], src1 = [4, 5, 6, 7]
      __ vmov(scratch, src1);
      __ vuzp(Neon32, dst, scratch);  // dst = [0, 2, 4, 6]
      break;
    }
    case kArmS32x4UnzipRight: {
      Simd128Register dst = i.OutputSimd128Register(),
                      src1 = i.InputSimd128Register(1);
      DCHECK(dst == i.InputSimd128Register(0));
      UseScratchRegisterScope temps(tasm());
      Simd128Register scratch = temps.AcquireQ();
      // src0 = [4, 5, 6, 7], src1 = [0, 1, 2, 3] (flipped from UnzipLeft).
      __ vmov(scratch, src1);
      __ vuzp(Neon32, scratch, dst);  // dst = [1, 3, 5, 7]
      break;
    }
    case kArmS32x4TransposeLeft: {
      Simd128Register dst = i.OutputSimd128Register(),
                      src1 = i.InputSimd128Register(1);
      DCHECK(dst == i.InputSimd128Register(0));
      UseScratchRegisterScope temps(tasm());
      Simd128Register scratch = temps.AcquireQ();
      // src0 = [0, 1, 2, 3], src1 = [4, 5, 6, 7]
      __ vmov(scratch, src1);
      __ vtrn(Neon32, dst, scratch);  // dst = [0, 4, 2, 6]
      break;
    }
    case kArmS32x4Shuffle: {
      Simd128Register dst = i.OutputSimd128Register(),
                      src0 = i.InputSimd128Register(0),
                      src1 = i.InputSimd128Register(1);
      DCHECK_NE(dst, src0);
      DCHECK_NE(dst, src1);
      // Perform shuffle as a vmov per lane.
      int dst_code = dst.code() * 4;
      int src0_code = src0.code() * 4;
      int src1_code = src1.code() * 4;
      int32_t shuffle = i.InputInt32(2);
      for (int i = 0; i < 4; i++) {
        int lane = shuffle & 0x7;
        int src_code = src0_code;
        if (lane >= 4) {
          src_code = src1_code;
          lane &= 0x3;
        }
        __ VmovExtended(dst_code + i, src_code + lane);
        shuffle >>= 8;
      }
      break;
    }
    case kArmS32x4TransposeRight: {
      Simd128Register dst = i.OutputSimd128Register(),
                      src1 = i.InputSimd128Register(1);
      UseScratchRegisterScope temps(tasm());
      Simd128Register scratch = temps.AcquireQ();
      DCHECK(dst == i.InputSimd128Register(0));
      // src0 = [4, 5, 6, 7], src1 = [0, 1, 2, 3] (flipped from TransposeLeft).
      __ vmov(scratch, src1);
      __ vtrn(Neon32, scratch, dst);  // dst = [1, 5, 3, 7]
      break;
    }
    case kArmS16x8ZipLeft: {
      Simd128Register dst = i.OutputSimd128Register(),
                      src1 = i.InputSimd128Register(1);
      // src0 = [0, 1, 2, 3, ... 7], src1 = [8, 9, 10, 11, ... 15]
      DCHECK(dst == i.InputSimd128Register(0));
      __ vmov(dst.high(), src1.low());         // dst = [0, 1, 2, 3, 8, ... 11]
      __ vzip(Neon16, dst.low(), dst.high());  // dst = [0, 8, 1, 9, ... 11]
      break;
    }
    case kArmS16x8ZipRight: {
      Simd128Register dst = i.OutputSimd128Register(),
                      src1 = i.InputSimd128Register(1);
      DCHECK(dst == i.InputSimd128Register(0));
      // src0 = [8, 9, 10, 11, ... 15], src1 = [0, 1, 2, 3, ... 7] (flipped).
      __ vmov(dst.low(), src1.high());
      __ vzip(Neon16, dst.low(), dst.high());  // dst = [4, 12, 5, 13, ... 15]
      break;
    }
    case kArmS16x8UnzipLeft: {
      Simd128Register dst = i.OutputSimd128Register(),
                      src1 = i.InputSimd128Register(1);
      UseScratchRegisterScope temps(tasm());
      Simd128Register scratch = temps.AcquireQ();
      DCHECK(dst == i.InputSimd128Register(0));
      // src0 = [0, 1, 2, 3, ... 7], src1 = [8, 9, 10, 11, ... 15]
      __ vmov(scratch, src1);
      __ vuzp(Neon16, dst, scratch);  // dst = [0, 2, 4, 6, ... 14]
      break;
    }
    case kArmS16x8UnzipRight: {
      Simd128Register dst = i.OutputSimd128Register(),
                      src1 = i.InputSimd128Register(1);
      UseScratchRegisterScope temps(tasm());
      Simd128Register scratch = temps.AcquireQ();
      DCHECK(dst == i.InputSimd128Register(0));
      // src0 = [8, 9, 10, 11, ... 15], src1 = [0, 1, 2, 3, ... 7] (flipped).
      __ vmov(scratch, src1);
      __ vuzp(Neon16, scratch, dst);  // dst = [1, 3, 5, 7, ... 15]
      break;
    }
    case kArmS16x8TransposeLeft: {
      Simd128Register dst = i.OutputSimd128Register(),
                      src1 = i.InputSimd128Register(1);
      UseScratchRegisterScope temps(tasm());
      Simd128Register scratch = temps.AcquireQ();
      DCHECK(dst == i.InputSimd128Register(0));
      // src0 = [0, 1, 2, 3, ... 7], src1 = [8, 9, 10, 11, ... 15]
      __ vmov(scratch, src1);
      __ vtrn(Neon16, dst, scratch);  // dst = [0, 8, 2, 10, ... 14]
      break;
    }
    case kArmS16x8TransposeRight: {
      Simd128Register dst = i.OutputSimd128Register(),
                      src1 = i.InputSimd128Register(1);
      UseScratchRegisterScope temps(tasm());
      Simd128Register scratch = temps.AcquireQ();
      DCHECK(dst == i.InputSimd128Register(0));
      // src0 = [8, 9, 10, 11, ... 15], src1 = [0, 1, 2, 3, ... 7] (flipped).
      __ vmov(scratch, src1);
      __ vtrn(Neon16, scratch, dst);  // dst = [1, 9, 3, 11, ... 15]
      break;
    }
    case kArmS8x16ZipLeft: {
      Simd128Register dst = i.OutputSimd128Register(),
                      src1 = i.InputSimd128Register(1);
      DCHECK(dst == i.InputSimd128Register(0));
      // src0 = [0, 1, 2, 3, ... 15], src1 = [16, 17, 18, 19, ... 31]
      __ vmov(dst.high(), src1.low());
      __ vzip(Neon8, dst.low(), dst.high());  // dst = [0, 16, 1, 17, ... 23]
      break;
    }
    case kArmS8x16ZipRight: {
      Simd128Register dst = i.OutputSimd128Register(),
                      src1 = i.InputSimd128Register(1);
      DCHECK(dst == i.InputSimd128Register(0));
      // src0 = [16, 17, 18, 19, ... 31], src1 = [0, 1, 2, 3, ... 15] (flipped).
      __ vmov(dst.low(), src1.high());
      __ vzip(Neon8, dst.low(), dst.high());  // dst = [8, 24, 9, 25, ... 31]
      break;
    }
    case kArmS8x16UnzipLeft: {
      Simd128Register dst = i.OutputSimd128Register(),
                      src1 = i.InputSimd128Register(1);
      UseScratchRegisterScope temps(tasm());
      Simd128Register scratch = temps.AcquireQ();
      DCHECK(dst == i.InputSimd128Register(0));
      // src0 = [0, 1, 2, 3, ... 15], src1 = [16, 17, 18, 19, ... 31]
      __ vmov(scratch, src1);
      __ vuzp(Neon8, dst, scratch);  // dst = [0, 2, 4, 6, ... 30]
      break;
    }
    case kArmS8x16UnzipRight: {
      Simd128Register dst = i.OutputSimd128Register(),
                      src1 = i.InputSimd128Register(1);
      UseScratchRegisterScope temps(tasm());
      Simd128Register scratch = temps.AcquireQ();
      DCHECK(dst == i.InputSimd128Register(0));
      // src0 = [16, 17, 18, 19, ... 31], src1 = [0, 1, 2, 3, ... 15] (flipped).
      __ vmov(scratch, src1);
      __ vuzp(Neon8, scratch, dst);  // dst = [1, 3, 5, 7, ... 31]
      break;
    }
    case kArmS8x16TransposeLeft: {
      Simd128Register dst = i.OutputSimd128Register(),
                      src1 = i.InputSimd128Register(1);
      UseScratchRegisterScope temps(tasm());
      Simd128Register scratch = temps.AcquireQ();
      DCHECK(dst == i.InputSimd128Register(0));
      // src0 = [0, 1, 2, 3, ... 15], src1 = [16, 17, 18, 19, ... 31]
      __ vmov(scratch, src1);
      __ vtrn(Neon8, dst, scratch);  // dst = [0, 16, 2, 18, ... 30]
      break;
    }
    case kArmS8x16TransposeRight: {
      Simd128Register dst = i.OutputSimd128Register(),
                      src1 = i.InputSimd128Register(1);
      UseScratchRegisterScope temps(tasm());
      Simd128Register scratch = temps.AcquireQ();
      DCHECK(dst == i.InputSimd128Register(0));
      // src0 = [16, 17, 18, 19, ... 31], src1 = [0, 1, 2, 3, ... 15] (flipped).
      __ vmov(scratch, src1);
      __ vtrn(Neon8, scratch, dst);  // dst = [1, 17, 3, 19, ... 31]
      break;
    }
    case kArmS8x16Concat: {
      __ vext(i.OutputSimd128Register(), i.InputSimd128Register(0),
              i.InputSimd128Register(1), i.InputInt4(2));
      break;
    }
    case kArmS8x16Shuffle: {
      Simd128Register dst = i.OutputSimd128Register(),
                      src0 = i.InputSimd128Register(0),
                      src1 = i.InputSimd128Register(1);
      DwVfpRegister table_base = src0.low();
      UseScratchRegisterScope temps(tasm());
      Simd128Register scratch = temps.AcquireQ();
      // If unary shuffle, table is src0 (2 d-registers), otherwise src0 and
      // src1. They must be consecutive.
      int table_size = src0 == src1 ? 2 : 4;
      DCHECK_IMPLIES(src0 != src1, src0.code() + 1 == src1.code());
      // The shuffle lane mask is a byte mask, materialize in scratch.
      int scratch_s_base = scratch.code() * 4;
      for (int j = 0; j < 4; j++) {
        uint32_t four_lanes = i.InputUint32(2 + j);
        // Ensure byte indices are in [0, 31] so masks are never NaNs.
        four_lanes &= 0x1F1F1F1F;
        __ vmov(SwVfpRegister::from_code(scratch_s_base + j),
                Float32::FromBits(four_lanes));
      }
      NeonListOperand table(table_base, table_size);
      if (dst != src0 && dst != src1) {
        __ vtbl(dst.low(), table, scratch.low());
        __ vtbl(dst.high(), table, scratch.high());
      } else {
        __ vtbl(scratch.low(), table, scratch.low());
        __ vtbl(scratch.high(), table, scratch.high());
        __ vmov(dst, scratch);
      }
      break;
    }
    case kArmS32x2Reverse: {
      __ vrev64(Neon32, i.OutputSimd128Register(), i.InputSimd128Register(0));
      break;
    }
    case kArmS16x4Reverse: {
      __ vrev64(Neon16, i.OutputSimd128Register(), i.InputSimd128Register(0));
      break;
    }
    case kArmS16x2Reverse: {
      __ vrev32(Neon16, i.OutputSimd128Register(), i.InputSimd128Register(0));
      break;
    }
    case kArmS8x8Reverse: {
      __ vrev64(Neon8, i.OutputSimd128Register(), i.InputSimd128Register(0));
      break;
    }
    case kArmS8x4Reverse: {
      __ vrev32(Neon8, i.OutputSimd128Register(), i.InputSimd128Register(0));
      break;
    }
    case kArmS8x2Reverse: {
      __ vrev16(Neon8, i.OutputSimd128Register(), i.InputSimd128Register(0));
      break;
    }
    case kArmS1x4AnyTrue: {
      const QwNeonRegister& src = i.InputSimd128Register(0);
      UseScratchRegisterScope temps(tasm());
      DwVfpRegister scratch = temps.AcquireD();
      __ vpmax(NeonU32, scratch, src.low(), src.high());
      __ vpmax(NeonU32, scratch, scratch, scratch);
      __ ExtractLane(i.OutputRegister(), scratch, NeonS32, 0);
      break;
    }
    case kArmS1x4AllTrue: {
      const QwNeonRegister& src = i.InputSimd128Register(0);
      UseScratchRegisterScope temps(tasm());
      DwVfpRegister scratch = temps.AcquireD();
      __ vpmin(NeonU32, scratch, src.low(), src.high());
      __ vpmin(NeonU32, scratch, scratch, scratch);
      __ ExtractLane(i.OutputRegister(), scratch, NeonS32, 0);
      break;
    }
    case kArmS1x8AnyTrue: {
      const QwNeonRegister& src = i.InputSimd128Register(0);
      UseScratchRegisterScope temps(tasm());
      DwVfpRegister scratch = temps.AcquireD();
      __ vpmax(NeonU16, scratch, src.low(), src.high());
      __ vpmax(NeonU16, scratch, scratch, scratch);
      __ vpmax(NeonU16, scratch, scratch, scratch);
      __ ExtractLane(i.OutputRegister(), scratch, NeonS16, 0);
      break;
    }
    case kArmS1x8AllTrue: {
      const QwNeonRegister& src = i.InputSimd128Register(0);
      UseScratchRegisterScope temps(tasm());
      DwVfpRegister scratch = temps.AcquireD();
      __ vpmin(NeonU16, scratch, src.low(), src.high());
      __ vpmin(NeonU16, scratch, scratch, scratch);
      __ vpmin(NeonU16, scratch, scratch, scratch);
      __ ExtractLane(i.OutputRegister(), scratch, NeonS16, 0);
      break;
    }
    case kArmS1x16AnyTrue: {
      const QwNeonRegister& src = i.InputSimd128Register(0);
      UseScratchRegisterScope temps(tasm());
      QwNeonRegister q_scratch = temps.AcquireQ();
      DwVfpRegister d_scratch = q_scratch.low();
      __ vpmax(NeonU8, d_scratch, src.low(), src.high());
      __ vpmax(NeonU8, d_scratch, d_scratch, d_scratch);
      // vtst to detect any bits in the bottom 32 bits of d_scratch.
      // This saves an instruction vs. the naive sequence of vpmax.
      // kDoubleRegZero is not changed, since it is 0.
      __ vtst(Neon32, q_scratch, q_scratch, q_scratch);
      __ ExtractLane(i.OutputRegister(), d_scratch, NeonS32, 0);
      break;
    }
    case kArmS1x16AllTrue: {
      const QwNeonRegister& src = i.InputSimd128Register(0);
      UseScratchRegisterScope temps(tasm());
      DwVfpRegister scratch = temps.AcquireD();
      __ vpmin(NeonU8, scratch, src.low(), src.high());
      __ vpmin(NeonU8, scratch, scratch, scratch);
      __ vpmin(NeonU8, scratch, scratch, scratch);
      __ vpmin(NeonU8, scratch, scratch, scratch);
      __ ExtractLane(i.OutputRegister(), scratch, NeonS8, 0);
      break;
    }
    case kWord32AtomicLoadInt8:
      ASSEMBLE_ATOMIC_LOAD_INTEGER(ldrsb);
      break;
    case kWord32AtomicLoadUint8:
      ASSEMBLE_ATOMIC_LOAD_INTEGER(ldrb);
      break;
    case kWord32AtomicLoadInt16:
      ASSEMBLE_ATOMIC_LOAD_INTEGER(ldrsh);
      break;
    case kWord32AtomicLoadUint16:
      ASSEMBLE_ATOMIC_LOAD_INTEGER(ldrh);
      break;
    case kWord32AtomicLoadWord32:
      ASSEMBLE_ATOMIC_LOAD_INTEGER(ldr);
      break;
    case kWord32AtomicStoreWord8:
      ASSEMBLE_ATOMIC_STORE_INTEGER(strb);
      break;
    case kWord32AtomicStoreWord16:
      ASSEMBLE_ATOMIC_STORE_INTEGER(strh);
      break;
    case kWord32AtomicStoreWord32:
      ASSEMBLE_ATOMIC_STORE_INTEGER(str);
      break;
    case kWord32AtomicExchangeInt8:
      ASSEMBLE_ATOMIC_EXCHANGE_INTEGER(ldrexb, strexb);
      __ sxtb(i.OutputRegister(0), i.OutputRegister(0));
      break;
    case kWord32AtomicExchangeUint8:
    case kArmWord64AtomicNarrowExchangeUint8:
      ASSEMBLE_ATOMIC_EXCHANGE_INTEGER(ldrexb, strexb);
      ATOMIC_NARROW_OP_CLEAR_HIGH_WORD(ExchangeUint8);
      break;
    case kWord32AtomicExchangeInt16:
      ASSEMBLE_ATOMIC_EXCHANGE_INTEGER(ldrexh, strexh);
      __ sxth(i.OutputRegister(0), i.OutputRegister(0));
      break;
    case kWord32AtomicExchangeUint16:
    case kArmWord64AtomicNarrowExchangeUint16:
      ASSEMBLE_ATOMIC_EXCHANGE_INTEGER(ldrexh, strexh);
      ATOMIC_NARROW_OP_CLEAR_HIGH_WORD(ExchangeUint16);
      break;
    case kWord32AtomicExchangeWord32:
    case kArmWord64AtomicNarrowExchangeUint32:
      ASSEMBLE_ATOMIC_EXCHANGE_INTEGER(ldrex, strex);
      ATOMIC_NARROW_OP_CLEAR_HIGH_WORD(ExchangeUint32);
      break;
    case kWord32AtomicCompareExchangeInt8:
      __ add(i.TempRegister(1), i.InputRegister(0), i.InputRegister(1));
      __ uxtb(i.TempRegister(2), i.InputRegister(2));
      ASSEMBLE_ATOMIC_COMPARE_EXCHANGE_INTEGER(ldrexb, strexb,
                                               i.TempRegister(2));
      __ sxtb(i.OutputRegister(0), i.OutputRegister(0));
      break;
    case kWord32AtomicCompareExchangeUint8:
    case kArmWord64AtomicNarrowCompareExchangeUint8:
      __ add(i.TempRegister(1), i.InputRegister(0), i.InputRegister(1));
      __ uxtb(i.TempRegister(2), i.InputRegister(2));
      ASSEMBLE_ATOMIC_COMPARE_EXCHANGE_INTEGER(ldrexb, strexb,
                                               i.TempRegister(2));
      ATOMIC_NARROW_OP_CLEAR_HIGH_WORD(CompareExchangeUint8);
      break;
    case kWord32AtomicCompareExchangeInt16:
      __ add(i.TempRegister(1), i.InputRegister(0), i.InputRegister(1));
      __ uxth(i.TempRegister(2), i.InputRegister(2));
      ASSEMBLE_ATOMIC_COMPARE_EXCHANGE_INTEGER(ldrexh, strexh,
                                               i.TempRegister(2));
      __ sxth(i.OutputRegister(0), i.OutputRegister(0));
      break;
    case kWord32AtomicCompareExchangeUint16:
    case kArmWord64AtomicNarrowCompareExchangeUint16:
      __ add(i.TempRegister(1), i.InputRegister(0), i.InputRegister(1));
      __ uxth(i.TempRegister(2), i.InputRegister(2));
      ASSEMBLE_ATOMIC_COMPARE_EXCHANGE_INTEGER(ldrexh, strexh,
                                               i.TempRegister(2));
      ATOMIC_NARROW_OP_CLEAR_HIGH_WORD(CompareExchangeUint16);
      break;
    case kWord32AtomicCompareExchangeWord32:
    case kArmWord64AtomicNarrowCompareExchangeUint32:
      __ add(i.TempRegister(1), i.InputRegister(0), i.InputRegister(1));
      ASSEMBLE_ATOMIC_COMPARE_EXCHANGE_INTEGER(ldrex, strex,
                                               i.InputRegister(2));
      ATOMIC_NARROW_OP_CLEAR_HIGH_WORD(CompareExchangeUint32);
      break;
#define ATOMIC_BINOP_CASE(op, inst)                    \
  case kWord32Atomic##op##Int8:                        \
    ASSEMBLE_ATOMIC_BINOP(ldrexb, strexb, inst);       \
    __ sxtb(i.OutputRegister(0), i.OutputRegister(0)); \
    break;                                             \
  case kWord32Atomic##op##Uint8:                       \
  case kArmWord64AtomicNarrow##op##Uint8:              \
    ASSEMBLE_ATOMIC_BINOP(ldrexb, strexb, inst);       \
    ATOMIC_NARROW_OP_CLEAR_HIGH_WORD(op##Uint8);       \
    break;                                             \
  case kWord32Atomic##op##Int16:                       \
    ASSEMBLE_ATOMIC_BINOP(ldrexh, strexh, inst);       \
    __ sxth(i.OutputRegister(0), i.OutputRegister(0)); \
    break;                                             \
  case kWord32Atomic##op##Uint16:                      \
  case kArmWord64AtomicNarrow##op##Uint16:             \
    ASSEMBLE_ATOMIC_BINOP(ldrexh, strexh, inst);       \
    ATOMIC_NARROW_OP_CLEAR_HIGH_WORD(op##Uint16);      \
    break;                                             \
  case kWord32Atomic##op##Word32:                      \
  case kArmWord64AtomicNarrow##op##Uint32:             \
    ASSEMBLE_ATOMIC_BINOP(ldrex, strex, inst);         \
    ATOMIC_NARROW_OP_CLEAR_HIGH_WORD(op##Uint32);      \
    break;
      ATOMIC_BINOP_CASE(Add, add)
      ATOMIC_BINOP_CASE(Sub, sub)
      ATOMIC_BINOP_CASE(And, and_)
      ATOMIC_BINOP_CASE(Or, orr)
      ATOMIC_BINOP_CASE(Xor, eor)
#undef ATOMIC_BINOP_CASE
    case kArmWord32AtomicPairLoad:
      __ add(i.TempRegister(0), i.InputRegister(0), i.InputRegister(1));
      __ ldrexd(i.OutputRegister(0), i.OutputRegister(1), i.TempRegister(0));
      __ dmb(ISH);
      break;
    case kArmWord32AtomicPairStore: {
      Label store;
      __ add(i.TempRegister(0), i.InputRegister(0), i.InputRegister(1));
      __ dmb(ISH);
      __ bind(&store);
      __ ldrexd(i.TempRegister(1), i.TempRegister(2), i.TempRegister(0));
      __ strexd(i.TempRegister(1), i.InputRegister(2), i.InputRegister(3),
                i.TempRegister(0));
      __ teq(i.TempRegister(1), Operand(0));
      __ b(ne, &store);
      __ dmb(ISH);
      break;
    }
#define ATOMIC_ARITH_BINOP_CASE(op, instr1, instr2) \
  case kArmWord32AtomicPair##op: {                  \
    ASSEMBLE_ATOMIC64_ARITH_BINOP(instr1, instr2);  \
    break;                                          \
  }
      ATOMIC_ARITH_BINOP_CASE(Add, add, adc)
      ATOMIC_ARITH_BINOP_CASE(Sub, sub, sbc)
#undef ATOMIC_ARITH_BINOP_CASE
#define ATOMIC_LOGIC_BINOP_CASE(op, instr) \
  case kArmWord32AtomicPair##op: {         \
    ASSEMBLE_ATOMIC64_LOGIC_BINOP(instr);  \
    break;                                 \
  }
      ATOMIC_LOGIC_BINOP_CASE(And, and_)
      ATOMIC_LOGIC_BINOP_CASE(Or, orr)
      ATOMIC_LOGIC_BINOP_CASE(Xor, eor)
    case kArmWord32AtomicPairExchange: {
      Label exchange;
      __ add(i.TempRegister(0), i.InputRegister(2), i.InputRegister(3));
      __ dmb(ISH);
      __ bind(&exchange);
      __ ldrexd(i.OutputRegister(0), i.OutputRegister(1), i.TempRegister(0));
      __ strexd(i.TempRegister(1), i.InputRegister(0), i.InputRegister(1),
                i.TempRegister(0));
      __ teq(i.TempRegister(1), Operand(0));
      __ b(ne, &exchange);
      __ dmb(ISH);
      break;
    }
    case kArmWord32AtomicPairCompareExchange: {
      __ add(i.TempRegister(0), i.InputRegister(4), i.InputRegister(5));
      Label compareExchange;
      Label exit;
      __ dmb(ISH);
      __ bind(&compareExchange);
      __ ldrexd(i.OutputRegister(0), i.OutputRegister(1), i.TempRegister(0));
      __ teq(i.InputRegister(0), Operand(i.OutputRegister(0)));
      __ b(ne, &exit);
      __ teq(i.InputRegister(1), Operand(i.OutputRegister(1)));
      __ b(ne, &exit);
      __ strexd(i.TempRegister(1), i.InputRegister(2), i.InputRegister(3),
                i.TempRegister(0));
      __ teq(i.TempRegister(1), Operand(0));
      __ b(ne, &compareExchange);
      __ bind(&exit);
      __ dmb(ISH);
      break;
    }
#undef ATOMIC_LOGIC_BINOP_CASE
#undef ATOMIC_NARROW_OP_CLEAR_HIGH_WORD
#undef ASSEMBLE_ATOMIC_LOAD_INTEGER
#undef ASSEMBLE_ATOMIC_STORE_INTEGER
#undef ASSEMBLE_ATOMIC_EXCHANGE_INTEGER
#undef ASSEMBLE_ATOMIC_COMPARE_EXCHANGE_INTEGER
#undef ASSEMBLE_ATOMIC_BINOP
#undef ASSEMBLE_ATOMIC64_ARITH_BINOP
#undef ASSEMBLE_ATOMIC64_LOGIC_BINOP
#undef ASSEMBLE_IEEE754_BINOP
#undef ASSEMBLE_IEEE754_UNOP
#undef ASSEMBLE_NEON_NARROWING_OP
#undef ASSEMBLE_NEON_PAIRWISE_OP
  }
  return kSuccess;
}  // NOLINT(readability/fn_size)


// Assembles branches after an instruction.
void CodeGenerator::AssembleArchBranch(Instruction* instr, BranchInfo* branch) {
  ArmOperandConverter i(this, instr);
  Label* tlabel = branch->true_label;
  Label* flabel = branch->false_label;
  Condition cc = FlagsConditionToCondition(branch->condition);
  __ b(cc, tlabel);
  if (!branch->fallthru) __ b(flabel);  // no fallthru to flabel.
}

void CodeGenerator::AssembleBranchPoisoning(FlagsCondition condition,
                                            Instruction* instr) {
  // TODO(jarin) Handle float comparisons (kUnordered[Not]Equal).
  if (condition == kUnorderedEqual || condition == kUnorderedNotEqual) {
    return;
  }

  condition = NegateFlagsCondition(condition);
  __ eor(kSpeculationPoisonRegister, kSpeculationPoisonRegister,
         Operand(kSpeculationPoisonRegister), SBit::LeaveCC,
         FlagsConditionToCondition(condition));
  __ csdb();
}

void CodeGenerator::AssembleArchDeoptBranch(Instruction* instr,
                                            BranchInfo* branch) {
  AssembleArchBranch(instr, branch);
}

void CodeGenerator::AssembleArchJump(RpoNumber target) {
  if (!IsNextInAssemblyOrder(target)) __ b(GetLabel(target));
}

void CodeGenerator::AssembleArchTrap(Instruction* instr,
                                     FlagsCondition condition) {
  class OutOfLineTrap final : public OutOfLineCode {
   public:
    OutOfLineTrap(CodeGenerator* gen, Instruction* instr)
        : OutOfLineCode(gen), instr_(instr), gen_(gen) {}

    void Generate() final {
      ArmOperandConverter i(gen_, instr_);
      TrapId trap_id =
          static_cast<TrapId>(i.InputInt32(instr_->InputCount() - 1));
      GenerateCallToTrap(trap_id);
    }

   private:
    void GenerateCallToTrap(TrapId trap_id) {
      if (trap_id == TrapId::kInvalid) {
        // We cannot test calls to the runtime in cctest/test-run-wasm.
        // Therefore we emit a call to C here instead of a call to the runtime.
        // We use the context register as the scratch register, because we do
        // not have a context here.
        __ PrepareCallCFunction(0, 0);
        __ CallCFunction(
            ExternalReference::wasm_call_trap_callback_for_testing(), 0);
        __ LeaveFrame(StackFrame::WASM_COMPILED);
        auto call_descriptor = gen_->linkage()->GetIncomingDescriptor();
        int pop_count =
            static_cast<int>(call_descriptor->StackParameterCount());
        __ Drop(pop_count);
        __ Ret();
      } else {
        gen_->AssembleSourcePosition(instr_);
        // A direct call to a wasm runtime stub defined in this module.
        // Just encode the stub index. This will be patched at relocation.
        __ Call(static_cast<Address>(trap_id), RelocInfo::WASM_STUB_CALL);
        ReferenceMap* reference_map =
            new (gen_->zone()) ReferenceMap(gen_->zone());
        gen_->RecordSafepoint(reference_map, Safepoint::kSimple, 0,
                              Safepoint::kNoLazyDeopt);
        if (FLAG_debug_code) {
          __ stop(GetAbortReason(AbortReason::kUnexpectedReturnFromWasmTrap));
        }
      }
    }

    Instruction* instr_;
    CodeGenerator* gen_;
  };
  auto ool = new (zone()) OutOfLineTrap(this, instr);
  Label* tlabel = ool->entry();
  Condition cc = FlagsConditionToCondition(condition);
  __ b(cc, tlabel);
}

// Assembles boolean materializations after an instruction.
void CodeGenerator::AssembleArchBoolean(Instruction* instr,
                                        FlagsCondition condition) {
  ArmOperandConverter i(this, instr);

  // Materialize a full 32-bit 1 or 0 value. The result register is always the
  // last output of the instruction.
  DCHECK_NE(0u, instr->OutputCount());
  Register reg = i.OutputRegister(instr->OutputCount() - 1);
  Condition cc = FlagsConditionToCondition(condition);
  __ mov(reg, Operand(0));
  __ mov(reg, Operand(1), LeaveCC, cc);
}

void CodeGenerator::AssembleArchBinarySearchSwitch(Instruction* instr) {
  ArmOperandConverter i(this, instr);
  Register input = i.InputRegister(0);
  std::vector<std::pair<int32_t, Label*>> cases;
  for (size_t index = 2; index < instr->InputCount(); index += 2) {
    cases.push_back({i.InputInt32(index + 0), GetLabel(i.InputRpo(index + 1))});
  }
  AssembleArchBinarySearchSwitchRange(input, i.InputRpo(1), cases.data(),
                                      cases.data() + cases.size());
}

void CodeGenerator::AssembleArchLookupSwitch(Instruction* instr) {
  ArmOperandConverter i(this, instr);
  Register input = i.InputRegister(0);
  for (size_t index = 2; index < instr->InputCount(); index += 2) {
    __ cmp(input, Operand(i.InputInt32(index + 0)));
    __ b(eq, GetLabel(i.InputRpo(index + 1)));
  }
  AssembleArchJump(i.InputRpo(1));
}


void CodeGenerator::AssembleArchTableSwitch(Instruction* instr) {
  ArmOperandConverter i(this, instr);
  Register input = i.InputRegister(0);
  size_t const case_count = instr->InputCount() - 2;
  // Ensure to emit the constant pool first if necessary.
  __ CheckConstPool(true, true);
  __ cmp(input, Operand(case_count));
  __ BlockConstPoolFor(case_count + 2);
  __ add(pc, pc, Operand(input, LSL, 2), LeaveCC, lo);
  __ b(GetLabel(i.InputRpo(1)));
  for (size_t index = 0; index < case_count; ++index) {
    __ b(GetLabel(i.InputRpo(index + 2)));
  }
}

void CodeGenerator::FinishFrame(Frame* frame) {
  auto call_descriptor = linkage()->GetIncomingDescriptor();

  const RegList saves_fp = call_descriptor->CalleeSavedFPRegisters();
  if (saves_fp != 0) {
    frame->AlignSavedCalleeRegisterSlots();
  }

  if (saves_fp != 0) {
    // Save callee-saved FP registers.
    STATIC_ASSERT(DwVfpRegister::kNumRegisters == 32);
    uint32_t last = base::bits::CountLeadingZeros32(saves_fp) - 1;
    uint32_t first = base::bits::CountTrailingZeros32(saves_fp);
    DCHECK_EQ((last - first + 1), base::bits::CountPopulation(saves_fp));
    frame->AllocateSavedCalleeRegisterSlots((last - first + 1) *
                                            (kDoubleSize / kPointerSize));
  }
  const RegList saves = call_descriptor->CalleeSavedRegisters();
  if (saves != 0) {
    // Save callee-saved registers.
    frame->AllocateSavedCalleeRegisterSlots(base::bits::CountPopulation(saves));
  }
}

void CodeGenerator::AssembleConstructFrame() {
  auto call_descriptor = linkage()->GetIncomingDescriptor();
  if (frame_access_state()->has_frame()) {
    if (call_descriptor->IsCFunctionCall()) {
      __ Push(lr, fp);
      __ mov(fp, sp);
    } else if (call_descriptor->IsJSFunctionCall()) {
      __ Prologue();
      if (call_descriptor->PushArgumentCount()) {
        __ Push(kJavaScriptCallArgCountRegister);
      }
    } else {
      __ StubPrologue(info()->GetOutputStackFrameType());
      if (call_descriptor->IsWasmFunctionCall()) {
        __ Push(kWasmInstanceRegister);
      }
    }

    unwinding_info_writer_.MarkFrameConstructed(__ pc_offset());
  }

  int shrink_slots = frame()->GetTotalFrameSlotCount() -
                     call_descriptor->CalculateFixedFrameSize();

  if (info()->is_osr()) {
    // TurboFan OSR-compiled functions cannot be entered directly.
    __ Abort(AbortReason::kShouldNotDirectlyEnterOsrFunction);

    // Unoptimized code jumps directly to this entrypoint while the unoptimized
    // frame is still on the stack. Optimized code uses OSR values directly from
    // the unoptimized frame. Thus, all that needs to be done is to allocate the
    // remaining stack slots.
    if (FLAG_code_comments) __ RecordComment("-- OSR entrypoint --");
    osr_pc_offset_ = __ pc_offset();
    shrink_slots -= osr_helper()->UnoptimizedFrameSlots();
    ResetSpeculationPoison();
  }

  const RegList saves = call_descriptor->CalleeSavedRegisters();
  const RegList saves_fp = call_descriptor->CalleeSavedFPRegisters();

  if (shrink_slots > 0) {
    DCHECK(frame_access_state()->has_frame());
    if (info()->IsWasm() && shrink_slots > 128) {
      // For WebAssembly functions with big frames we have to do the stack
      // overflow check before we construct the frame. Otherwise we may not
      // have enough space on the stack to call the runtime for the stack
      // overflow.
      Label done;

      // If the frame is bigger than the stack, we throw the stack overflow
      // exception unconditionally. Thereby we can avoid the integer overflow
      // check in the condition code.
      if ((shrink_slots * kPointerSize) < (FLAG_stack_size * 1024)) {
        UseScratchRegisterScope temps(tasm());
        Register scratch = temps.Acquire();
        __ ldr(scratch, FieldMemOperand(
                            kWasmInstanceRegister,
                            WasmInstanceObject::kRealStackLimitAddressOffset));
        __ ldr(scratch, MemOperand(scratch));
        __ add(scratch, scratch, Operand(shrink_slots * kPointerSize));
        __ cmp(sp, scratch);
        __ b(cs, &done);
      }

      __ ldr(r2, FieldMemOperand(kWasmInstanceRegister,
                                 WasmInstanceObject::kCEntryStubOffset));
      __ Move(cp, Smi::kZero);
      __ CallRuntimeWithCEntry(Runtime::kThrowWasmStackOverflow, r2);
      // We come from WebAssembly, there are no references for the GC.
      ReferenceMap* reference_map = new (zone()) ReferenceMap(zone());
      RecordSafepoint(reference_map, Safepoint::kSimple, 0,
                      Safepoint::kNoLazyDeopt);
      if (FLAG_debug_code) {
        __ stop(GetAbortReason(AbortReason::kUnexpectedReturnFromThrow));
      }

      __ bind(&done);
    }

    // Skip callee-saved and return slots, which are pushed below.
    shrink_slots -= base::bits::CountPopulation(saves);
    shrink_slots -= frame()->GetReturnSlotCount();
    shrink_slots -= 2 * base::bits::CountPopulation(saves_fp);
    if (shrink_slots > 0) {
      __ sub(sp, sp, Operand(shrink_slots * kPointerSize));
    }
  }

  if (saves_fp != 0) {
    // Save callee-saved FP registers.
    STATIC_ASSERT(DwVfpRegister::kNumRegisters == 32);
    uint32_t last = base::bits::CountLeadingZeros32(saves_fp) - 1;
    uint32_t first = base::bits::CountTrailingZeros32(saves_fp);
    DCHECK_EQ((last - first + 1), base::bits::CountPopulation(saves_fp));
    __ vstm(db_w, sp, DwVfpRegister::from_code(first),
            DwVfpRegister::from_code(last));
  }

  if (saves != 0) {
    // Save callee-saved registers.
    __ stm(db_w, sp, saves);
  }

  const int returns = frame()->GetReturnSlotCount();
  if (returns != 0) {
    // Create space for returns.
    __ sub(sp, sp, Operand(returns * kPointerSize));
  }
}

void CodeGenerator::AssembleReturn(InstructionOperand* pop) {
  auto call_descriptor = linkage()->GetIncomingDescriptor();
  int pop_count = static_cast<int>(call_descriptor->StackParameterCount());

  const int returns = frame()->GetReturnSlotCount();
  if (returns != 0) {
    // Free space of returns.
    __ add(sp, sp, Operand(returns * kPointerSize));
  }

  // Restore registers.
  const RegList saves = call_descriptor->CalleeSavedRegisters();
  if (saves != 0) {
    __ ldm(ia_w, sp, saves);
  }

  // Restore FP registers.
  const RegList saves_fp = call_descriptor->CalleeSavedFPRegisters();
  if (saves_fp != 0) {
    STATIC_ASSERT(DwVfpRegister::kNumRegisters == 32);
    uint32_t last = base::bits::CountLeadingZeros32(saves_fp) - 1;
    uint32_t first = base::bits::CountTrailingZeros32(saves_fp);
    __ vldm(ia_w, sp, DwVfpRegister::from_code(first),
            DwVfpRegister::from_code(last));
  }

  unwinding_info_writer_.MarkBlockWillExit();

  ArmOperandConverter g(this, nullptr);
  if (call_descriptor->IsCFunctionCall()) {
    AssembleDeconstructFrame();
  } else if (frame_access_state()->has_frame()) {
    // Canonicalize JSFunction return sites for now unless they have an variable
    // number of stack slot pops.
    if (pop->IsImmediate() && g.ToConstant(pop).ToInt32() == 0) {
      if (return_label_.is_bound()) {
        __ b(&return_label_);
        return;
      } else {
        __ bind(&return_label_);
        AssembleDeconstructFrame();
      }
    } else {
      AssembleDeconstructFrame();
    }
  }

  if (pop->IsImmediate()) {
    DCHECK_EQ(Constant::kInt32, g.ToConstant(pop).type());
    pop_count += g.ToConstant(pop).ToInt32();
  } else {
    __ Drop(g.ToRegister(pop));
  }
  __ Drop(pop_count);
  __ Ret();
}

void CodeGenerator::FinishCode() { __ CheckConstPool(true, false); }

void CodeGenerator::AssembleMove(InstructionOperand* source,
                                 InstructionOperand* destination) {
  ArmOperandConverter g(this, nullptr);
  // Helper function to write the given constant to the dst register.
  auto MoveConstantToRegister = [&](Register dst, Constant src) {
    if (src.type() == Constant::kHeapObject) {
      Handle<HeapObject> src_object = src.ToHeapObject();
      Heap::RootListIndex index;
      if (IsMaterializableFromRoot(src_object, &index)) {
        __ LoadRoot(dst, index);
      } else {
        __ Move(dst, src_object);
      }
    } else if (src.type() == Constant::kExternalReference) {
      __ Move(dst, src.ToExternalReference());
    } else {
      __ mov(dst, g.ToImmediate(source));
    }
  };
  switch (MoveType::InferMove(source, destination)) {
    case MoveType::kRegisterToRegister:
      if (source->IsRegister()) {
        __ mov(g.ToRegister(destination), g.ToRegister(source));
      } else if (source->IsFloatRegister()) {
        DCHECK(destination->IsFloatRegister());
        // GapResolver may give us reg codes that don't map to actual
        // s-registers. Generate code to work around those cases.
        int src_code = LocationOperand::cast(source)->register_code();
        int dst_code = LocationOperand::cast(destination)->register_code();
        __ VmovExtended(dst_code, src_code);
      } else if (source->IsDoubleRegister()) {
        __ Move(g.ToDoubleRegister(destination), g.ToDoubleRegister(source));
      } else {
        __ Move(g.ToSimd128Register(destination), g.ToSimd128Register(source));
      }
      return;
    case MoveType::kRegisterToStack: {
      MemOperand dst = g.ToMemOperand(destination);
      if (source->IsRegister()) {
        __ str(g.ToRegister(source), dst);
      } else if (source->IsFloatRegister()) {
        // GapResolver may give us reg codes that don't map to actual
        // s-registers. Generate code to work around those cases.
        int src_code = LocationOperand::cast(source)->register_code();
        __ VmovExtended(dst, src_code);
      } else if (source->IsDoubleRegister()) {
        __ vstr(g.ToDoubleRegister(source), dst);
      } else {
        UseScratchRegisterScope temps(tasm());
        Register temp = temps.Acquire();
        QwNeonRegister src = g.ToSimd128Register(source);
        __ add(temp, dst.rn(), Operand(dst.offset()));
        __ vst1(Neon8, NeonListOperand(src.low(), 2), NeonMemOperand(temp));
      }
      return;
    }
    case MoveType::kStackToRegister: {
      MemOperand src = g.ToMemOperand(source);
      if (source->IsStackSlot()) {
        __ ldr(g.ToRegister(destination), src);
      } else if (source->IsFloatStackSlot()) {
        DCHECK(destination->IsFloatRegister());
        // GapResolver may give us reg codes that don't map to actual
        // s-registers. Generate code to work around those cases.
        int dst_code = LocationOperand::cast(destination)->register_code();
        __ VmovExtended(dst_code, src);
      } else if (source->IsDoubleStackSlot()) {
        __ vldr(g.ToDoubleRegister(destination), src);
      } else {
        UseScratchRegisterScope temps(tasm());
        Register temp = temps.Acquire();
        QwNeonRegister dst = g.ToSimd128Register(destination);
        __ add(temp, src.rn(), Operand(src.offset()));
        __ vld1(Neon8, NeonListOperand(dst.low(), 2), NeonMemOperand(temp));
      }
      return;
    }
    case MoveType::kStackToStack: {
      MemOperand src = g.ToMemOperand(source);
      MemOperand dst = g.ToMemOperand(destination);
      UseScratchRegisterScope temps(tasm());
      if (source->IsStackSlot() || source->IsFloatStackSlot()) {
        SwVfpRegister temp = temps.AcquireS();
        __ vldr(temp, src);
        __ vstr(temp, dst);
      } else if (source->IsDoubleStackSlot()) {
        DwVfpRegister temp = temps.AcquireD();
        __ vldr(temp, src);
        __ vstr(temp, dst);
      } else {
        DCHECK(source->IsSimd128StackSlot());
        Register temp = temps.Acquire();
        QwNeonRegister temp_q = temps.AcquireQ();
        __ add(temp, src.rn(), Operand(src.offset()));
        __ vld1(Neon8, NeonListOperand(temp_q.low(), 2), NeonMemOperand(temp));
        __ add(temp, dst.rn(), Operand(dst.offset()));
        __ vst1(Neon8, NeonListOperand(temp_q.low(), 2), NeonMemOperand(temp));
      }
      return;
    }
    case MoveType::kConstantToRegister: {
      Constant src = g.ToConstant(source);
      if (destination->IsRegister()) {
        MoveConstantToRegister(g.ToRegister(destination), src);
      } else if (destination->IsFloatRegister()) {
        __ vmov(g.ToFloatRegister(destination),
                Float32::FromBits(src.ToFloat32AsInt()));
      } else {
        // TODO(arm): Look into optimizing this further if possible. Supporting
        // the NEON version of VMOV may help.
        __ vmov(g.ToDoubleRegister(destination), src.ToFloat64());
      }
      return;
    }
    case MoveType::kConstantToStack: {
      Constant src = g.ToConstant(source);
      MemOperand dst = g.ToMemOperand(destination);
      if (destination->IsStackSlot()) {
        UseScratchRegisterScope temps(tasm());
        // Acquire a S register instead of a general purpose register in case
        // `vstr` needs one to compute the address of `dst`.
        SwVfpRegister s_temp = temps.AcquireS();
        {
          // TODO(arm): This sequence could be optimized further if necessary by
          // writing the constant directly into `s_temp`.
          UseScratchRegisterScope temps(tasm());
          Register temp = temps.Acquire();
          MoveConstantToRegister(temp, src);
          __ vmov(s_temp, temp);
        }
        __ vstr(s_temp, dst);
      } else if (destination->IsFloatStackSlot()) {
        UseScratchRegisterScope temps(tasm());
        SwVfpRegister temp = temps.AcquireS();
        __ vmov(temp, Float32::FromBits(src.ToFloat32AsInt()));
        __ vstr(temp, dst);
      } else {
        DCHECK(destination->IsDoubleStackSlot());
        UseScratchRegisterScope temps(tasm());
        DwVfpRegister temp = temps.AcquireD();
        // TODO(arm): Look into optimizing this further if possible. Supporting
        // the NEON version of VMOV may help.
        __ vmov(temp, src.ToFloat64());
        __ vstr(temp, g.ToMemOperand(destination));
      }
      return;
    }
  }
  UNREACHABLE();
}

void CodeGenerator::AssembleSwap(InstructionOperand* source,
                                 InstructionOperand* destination) {
  ArmOperandConverter g(this, nullptr);
  switch (MoveType::InferSwap(source, destination)) {
    case MoveType::kRegisterToRegister:
      if (source->IsRegister()) {
        __ Swap(g.ToRegister(source), g.ToRegister(destination));
      } else if (source->IsFloatRegister()) {
        DCHECK(destination->IsFloatRegister());
        // GapResolver may give us reg codes that don't map to actual
        // s-registers. Generate code to work around those cases.
        UseScratchRegisterScope temps(tasm());
        LowDwVfpRegister temp = temps.AcquireLowD();
        int src_code = LocationOperand::cast(source)->register_code();
        int dst_code = LocationOperand::cast(destination)->register_code();
        __ VmovExtended(temp.low().code(), src_code);
        __ VmovExtended(src_code, dst_code);
        __ VmovExtended(dst_code, temp.low().code());
      } else if (source->IsDoubleRegister()) {
        __ Swap(g.ToDoubleRegister(source), g.ToDoubleRegister(destination));
      } else {
        __ Swap(g.ToSimd128Register(source), g.ToSimd128Register(destination));
      }
      return;
    case MoveType::kRegisterToStack: {
      MemOperand dst = g.ToMemOperand(destination);
      if (source->IsRegister()) {
        Register src = g.ToRegister(source);
        UseScratchRegisterScope temps(tasm());
        SwVfpRegister temp = temps.AcquireS();
        __ vmov(temp, src);
        __ ldr(src, dst);
        __ vstr(temp, dst);
      } else if (source->IsFloatRegister()) {
        int src_code = LocationOperand::cast(source)->register_code();
        UseScratchRegisterScope temps(tasm());
        LowDwVfpRegister temp = temps.AcquireLowD();
        __ VmovExtended(temp.low().code(), src_code);
        __ VmovExtended(src_code, dst);
        __ vstr(temp.low(), dst);
      } else if (source->IsDoubleRegister()) {
        UseScratchRegisterScope temps(tasm());
        DwVfpRegister temp = temps.AcquireD();
        DwVfpRegister src = g.ToDoubleRegister(source);
        __ Move(temp, src);
        __ vldr(src, dst);
        __ vstr(temp, dst);
      } else {
        QwNeonRegister src = g.ToSimd128Register(source);
        UseScratchRegisterScope temps(tasm());
        Register temp = temps.Acquire();
        QwNeonRegister temp_q = temps.AcquireQ();
        __ Move(temp_q, src);
        __ add(temp, dst.rn(), Operand(dst.offset()));
        __ vld1(Neon8, NeonListOperand(src.low(), 2), NeonMemOperand(temp));
        __ vst1(Neon8, NeonListOperand(temp_q.low(), 2), NeonMemOperand(temp));
      }
      return;
    }
    case MoveType::kStackToStack: {
      MemOperand src = g.ToMemOperand(source);
      MemOperand dst = g.ToMemOperand(destination);
      if (source->IsStackSlot() || source->IsFloatStackSlot()) {
        UseScratchRegisterScope temps(tasm());
        SwVfpRegister temp_0 = temps.AcquireS();
        SwVfpRegister temp_1 = temps.AcquireS();
        __ vldr(temp_0, dst);
        __ vldr(temp_1, src);
        __ vstr(temp_0, src);
        __ vstr(temp_1, dst);
      } else if (source->IsDoubleStackSlot()) {
        UseScratchRegisterScope temps(tasm());
        LowDwVfpRegister temp = temps.AcquireLowD();
        if (temps.CanAcquireD()) {
          DwVfpRegister temp_0 = temp;
          DwVfpRegister temp_1 = temps.AcquireD();
          __ vldr(temp_0, dst);
          __ vldr(temp_1, src);
          __ vstr(temp_0, src);
          __ vstr(temp_1, dst);
        } else {
          // We only have a single D register available. However, we can split
          // it into 2 S registers and swap the slots 32 bits at a time.
          MemOperand src0 = src;
          MemOperand dst0 = dst;
          MemOperand src1(src.rn(), src.offset() + kFloatSize);
          MemOperand dst1(dst.rn(), dst.offset() + kFloatSize);
          SwVfpRegister temp_0 = temp.low();
          SwVfpRegister temp_1 = temp.high();
          __ vldr(temp_0, dst0);
          __ vldr(temp_1, src0);
          __ vstr(temp_0, src0);
          __ vstr(temp_1, dst0);
          __ vldr(temp_0, dst1);
          __ vldr(temp_1, src1);
          __ vstr(temp_0, src1);
          __ vstr(temp_1, dst1);
        }
      } else {
        DCHECK(source->IsSimd128StackSlot());
        MemOperand src0 = src;
        MemOperand dst0 = dst;
        MemOperand src1(src.rn(), src.offset() + kDoubleSize);
        MemOperand dst1(dst.rn(), dst.offset() + kDoubleSize);
        UseScratchRegisterScope temps(tasm());
        DwVfpRegister temp_0 = temps.AcquireD();
        DwVfpRegister temp_1 = temps.AcquireD();
        __ vldr(temp_0, dst0);
        __ vldr(temp_1, src0);
        __ vstr(temp_0, src0);
        __ vstr(temp_1, dst0);
        __ vldr(temp_0, dst1);
        __ vldr(temp_1, src1);
        __ vstr(temp_0, src1);
        __ vstr(temp_1, dst1);
      }
      return;
    }
    default:
      UNREACHABLE();
      break;
  }
}

void CodeGenerator::AssembleJumpTable(Label** targets, size_t target_count) {
  // On 32-bit ARM we emit the jump tables inline.
  UNREACHABLE();
}

#undef __

}  // namespace compiler
}  // namespace internal
}  // namespace v8
