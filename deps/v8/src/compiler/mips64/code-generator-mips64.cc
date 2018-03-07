// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/assembler-inl.h"
#include "src/callable.h"
#include "src/compilation-info.h"
#include "src/compiler/code-generator-impl.h"
#include "src/compiler/code-generator.h"
#include "src/compiler/gap-resolver.h"
#include "src/compiler/node-matchers.h"
#include "src/compiler/osr.h"
#include "src/heap/heap-inl.h"
#include "src/mips64/macro-assembler-mips64.h"

namespace v8 {
namespace internal {
namespace compiler {

#define __ tasm()->

// TODO(plind): Possibly avoid using these lithium names.
#define kScratchReg kLithiumScratchReg
#define kScratchReg2 kLithiumScratchReg2
#define kScratchDoubleReg kLithiumScratchDouble


// TODO(plind): consider renaming these macros.
#define TRACE_MSG(msg)                                                      \
  PrintF("code_gen: \'%s\' in function %s at line %d\n", msg, __FUNCTION__, \
         __LINE__)

#define TRACE_UNIMPL()                                                       \
  PrintF("UNIMPLEMENTED code_generator_mips: %s at line %d\n", __FUNCTION__, \
         __LINE__)


// Adds Mips-specific methods to convert InstructionOperands.
class MipsOperandConverter final : public InstructionOperandConverter {
 public:
  MipsOperandConverter(CodeGenerator* gen, Instruction* instr)
      : InstructionOperandConverter(gen, instr) {}

  FloatRegister OutputSingleRegister(size_t index = 0) {
    return ToSingleRegister(instr_->OutputAt(index));
  }

  FloatRegister InputSingleRegister(size_t index) {
    return ToSingleRegister(instr_->InputAt(index));
  }

  FloatRegister ToSingleRegister(InstructionOperand* op) {
    // Single (Float) and Double register namespace is same on MIPS,
    // both are typedefs of FPURegister.
    return ToDoubleRegister(op);
  }

  Register InputOrZeroRegister(size_t index) {
    if (instr_->InputAt(index)->IsImmediate()) {
      DCHECK_EQ(0, InputInt32(index));
      return zero_reg;
    }
    return InputRegister(index);
  }

  DoubleRegister InputOrZeroDoubleRegister(size_t index) {
    if (instr_->InputAt(index)->IsImmediate()) return kDoubleRegZero;

    return InputDoubleRegister(index);
  }

  DoubleRegister InputOrZeroSingleRegister(size_t index) {
    if (instr_->InputAt(index)->IsImmediate()) return kDoubleRegZero;

    return InputSingleRegister(index);
  }

  Operand InputImmediate(size_t index) {
    Constant constant = ToConstant(instr_->InputAt(index));
    switch (constant.type()) {
      case Constant::kInt32:
        return Operand(constant.ToInt32());
      case Constant::kInt64:
        return Operand(constant.ToInt64());
      case Constant::kFloat32:
        return Operand::EmbeddedNumber(constant.ToFloat32());
      case Constant::kFloat64:
        return Operand::EmbeddedNumber(constant.ToFloat64().value());
      case Constant::kExternalReference:
      case Constant::kHeapObject:
        // TODO(plind): Maybe we should handle ExtRef & HeapObj here?
        //    maybe not done on arm due to const pool ??
        break;
      case Constant::kRpoNumber:
        UNREACHABLE();  // TODO(titzer): RPO immediates on mips?
        break;
    }
    UNREACHABLE();
  }

  Operand InputOperand(size_t index) {
    InstructionOperand* op = instr_->InputAt(index);
    if (op->IsRegister()) {
      return Operand(ToRegister(op));
    }
    return InputImmediate(index);
  }

  MemOperand MemoryOperand(size_t* first_index) {
    const size_t index = *first_index;
    switch (AddressingModeField::decode(instr_->opcode())) {
      case kMode_None:
        break;
      case kMode_MRI:
        *first_index += 2;
        return MemOperand(InputRegister(index + 0), InputInt32(index + 1));
      case kMode_MRR:
        // TODO(plind): r6 address mode, to be implemented ...
        UNREACHABLE();
    }
    UNREACHABLE();
  }

  MemOperand MemoryOperand(size_t index = 0) { return MemoryOperand(&index); }

  MemOperand ToMemOperand(InstructionOperand* op) const {
    DCHECK_NOT_NULL(op);
    DCHECK(op->IsStackSlot() || op->IsFPStackSlot());
    return SlotToMemOperand(AllocatedOperand::cast(op)->index());
  }

  MemOperand SlotToMemOperand(int slot) const {
    FrameOffset offset = frame_access_state()->GetFrameOffset(slot);
    return MemOperand(offset.from_stack_pointer() ? sp : fp, offset.offset());
  }
};


static inline bool HasRegisterInput(Instruction* instr, size_t index) {
  return instr->InputAt(index)->IsRegister();
}


namespace {

class OutOfLineRound : public OutOfLineCode {
 public:
  OutOfLineRound(CodeGenerator* gen, DoubleRegister result)
      : OutOfLineCode(gen), result_(result) {}

  void Generate() final {
    // Handle rounding to zero case where sign has to be preserved.
    // High bits of double input already in kScratchReg.
    __ dsrl(at, kScratchReg, 31);
    __ dsll(at, at, 31);
    __ mthc1(at, result_);
  }

 private:
  DoubleRegister const result_;
};


class OutOfLineRound32 : public OutOfLineCode {
 public:
  OutOfLineRound32(CodeGenerator* gen, DoubleRegister result)
      : OutOfLineCode(gen), result_(result) {}

  void Generate() final {
    // Handle rounding to zero case where sign has to be preserved.
    // High bits of float input already in kScratchReg.
    __ srl(at, kScratchReg, 31);
    __ sll(at, at, 31);
    __ mtc1(at, result_);
  }

 private:
  DoubleRegister const result_;
};


class OutOfLineRecordWrite final : public OutOfLineCode {
 public:
  OutOfLineRecordWrite(CodeGenerator* gen, Register object, Register index,
                       Register value, Register scratch0, Register scratch1,
                       RecordWriteMode mode)
      : OutOfLineCode(gen),
        object_(object),
        index_(index),
        value_(value),
        scratch0_(scratch0),
        scratch1_(scratch1),
        mode_(mode),
        must_save_lr_(!gen->frame_access_state()->has_frame()),
        zone_(gen->zone()) {}

  void SaveRegisters(RegList registers) {
    DCHECK_LT(0, NumRegs(registers));
    RegList regs = 0;
    for (int i = 0; i < Register::kNumRegisters; ++i) {
      if ((registers >> i) & 1u) {
        regs |= Register::from_code(i).bit();
      }
    }
    __ MultiPush(regs | ra.bit());
  }

  void RestoreRegisters(RegList registers) {
    DCHECK_LT(0, NumRegs(registers));
    RegList regs = 0;
    for (int i = 0; i < Register::kNumRegisters; ++i) {
      if ((registers >> i) & 1u) {
        regs |= Register::from_code(i).bit();
      }
    }
    __ MultiPop(regs | ra.bit());
  }

  void Generate() final {
    if (mode_ > RecordWriteMode::kValueIsPointer) {
      __ JumpIfSmi(value_, exit());
    }
    __ CheckPageFlag(value_, scratch0_,
                     MemoryChunk::kPointersToHereAreInterestingMask, eq,
                     exit());
    __ Daddu(scratch1_, object_, index_);
    RememberedSetAction const remembered_set_action =
        mode_ > RecordWriteMode::kValueIsMap ? EMIT_REMEMBERED_SET
                                             : OMIT_REMEMBERED_SET;
    SaveFPRegsMode const save_fp_mode =
        frame()->DidAllocateDoubleRegisters() ? kSaveFPRegs : kDontSaveFPRegs;
    if (must_save_lr_) {
      // We need to save and restore ra if the frame was elided.
      __ Push(ra);
    }
    __ CallRecordWriteStub(object_, scratch1_, remembered_set_action,
                           save_fp_mode);
    if (must_save_lr_) {
      __ Pop(ra);
    }
  }

 private:
  Register const object_;
  Register const index_;
  Register const value_;
  Register const scratch0_;
  Register const scratch1_;
  RecordWriteMode const mode_;
  bool must_save_lr_;
  Zone* zone_;
};

#define CREATE_OOL_CLASS(ool_name, tasm_ool_name, T)                 \
  class ool_name final : public OutOfLineCode {                      \
   public:                                                           \
    ool_name(CodeGenerator* gen, T dst, T src1, T src2)              \
        : OutOfLineCode(gen), dst_(dst), src1_(src1), src2_(src2) {} \
                                                                     \
    void Generate() final { __ tasm_ool_name(dst_, src1_, src2_); }  \
                                                                     \
   private:                                                          \
    T const dst_;                                                    \
    T const src1_;                                                   \
    T const src2_;                                                   \
  }

CREATE_OOL_CLASS(OutOfLineFloat32Max, Float32MaxOutOfLine, FPURegister);
CREATE_OOL_CLASS(OutOfLineFloat32Min, Float32MinOutOfLine, FPURegister);
CREATE_OOL_CLASS(OutOfLineFloat64Max, Float64MaxOutOfLine, FPURegister);
CREATE_OOL_CLASS(OutOfLineFloat64Min, Float64MinOutOfLine, FPURegister);

#undef CREATE_OOL_CLASS

Condition FlagsConditionToConditionCmp(FlagsCondition condition) {
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
    case kUnorderedEqual:
    case kUnorderedNotEqual:
      break;
    default:
      break;
  }
  UNREACHABLE();
}


Condition FlagsConditionToConditionTst(FlagsCondition condition) {
  switch (condition) {
    case kNotEqual:
      return ne;
    case kEqual:
      return eq;
    default:
      break;
  }
  UNREACHABLE();
}


Condition FlagsConditionToConditionOvf(FlagsCondition condition) {
  switch (condition) {
    case kOverflow:
      return ne;
    case kNotOverflow:
      return eq;
    default:
      break;
  }
  UNREACHABLE();
}


FPUCondition FlagsConditionToConditionCmpFPU(bool& predicate,
                                             FlagsCondition condition) {
  switch (condition) {
    case kEqual:
      predicate = true;
      return EQ;
    case kNotEqual:
      predicate = false;
      return EQ;
    case kUnsignedLessThan:
      predicate = true;
      return OLT;
    case kUnsignedGreaterThanOrEqual:
      predicate = false;
      return ULT;
    case kUnsignedLessThanOrEqual:
      predicate = true;
      return OLE;
    case kUnsignedGreaterThan:
      predicate = false;
      return ULE;
    case kUnorderedEqual:
    case kUnorderedNotEqual:
      predicate = true;
      break;
    default:
      predicate = true;
      break;
  }
  UNREACHABLE();
}

}  // namespace

#define ASSEMBLE_ROUND_DOUBLE_TO_DOUBLE(mode)                                  \
  if (kArchVariant == kMips64r6) {                                             \
    __ cfc1(kScratchReg, FCSR);                                                \
    __ li(at, Operand(mode_##mode));                                           \
    __ ctc1(at, FCSR);                                                         \
    __ rint_d(i.OutputDoubleRegister(), i.InputDoubleRegister(0));             \
    __ ctc1(kScratchReg, FCSR);                                                \
  } else {                                                                     \
    auto ool = new (zone()) OutOfLineRound(this, i.OutputDoubleRegister());    \
    Label done;                                                                \
    __ mfhc1(kScratchReg, i.InputDoubleRegister(0));                           \
    __ Ext(at, kScratchReg, HeapNumber::kExponentShift,                        \
           HeapNumber::kExponentBits);                                         \
    __ Branch(USE_DELAY_SLOT, &done, hs, at,                                   \
              Operand(HeapNumber::kExponentBias + HeapNumber::kMantissaBits)); \
    __ mov_d(i.OutputDoubleRegister(), i.InputDoubleRegister(0));              \
    __ mode##_l_d(i.OutputDoubleRegister(), i.InputDoubleRegister(0));         \
    __ dmfc1(at, i.OutputDoubleRegister());                                    \
    __ Branch(USE_DELAY_SLOT, ool->entry(), eq, at, Operand(zero_reg));        \
    __ cvt_d_l(i.OutputDoubleRegister(), i.OutputDoubleRegister());            \
    __ bind(ool->exit());                                                      \
    __ bind(&done);                                                            \
  }

#define ASSEMBLE_ROUND_FLOAT_TO_FLOAT(mode)                                   \
  if (kArchVariant == kMips64r6) {                                            \
    __ cfc1(kScratchReg, FCSR);                                               \
    __ li(at, Operand(mode_##mode));                                          \
    __ ctc1(at, FCSR);                                                        \
    __ rint_s(i.OutputDoubleRegister(), i.InputDoubleRegister(0));            \
    __ ctc1(kScratchReg, FCSR);                                               \
  } else {                                                                    \
    int32_t kFloat32ExponentBias = 127;                                       \
    int32_t kFloat32MantissaBits = 23;                                        \
    int32_t kFloat32ExponentBits = 8;                                         \
    auto ool = new (zone()) OutOfLineRound32(this, i.OutputDoubleRegister()); \
    Label done;                                                               \
    __ mfc1(kScratchReg, i.InputDoubleRegister(0));                           \
    __ Ext(at, kScratchReg, kFloat32MantissaBits, kFloat32ExponentBits);      \
    __ Branch(USE_DELAY_SLOT, &done, hs, at,                                  \
              Operand(kFloat32ExponentBias + kFloat32MantissaBits));          \
    __ mov_s(i.OutputDoubleRegister(), i.InputDoubleRegister(0));             \
    __ mode##_w_s(i.OutputDoubleRegister(), i.InputDoubleRegister(0));        \
    __ mfc1(at, i.OutputDoubleRegister());                                    \
    __ Branch(USE_DELAY_SLOT, ool->entry(), eq, at, Operand(zero_reg));       \
    __ cvt_s_w(i.OutputDoubleRegister(), i.OutputDoubleRegister());           \
    __ bind(ool->exit());                                                     \
    __ bind(&done);                                                           \
  }

#define ASSEMBLE_ATOMIC_LOAD_INTEGER(asm_instr)          \
  do {                                                   \
    __ asm_instr(i.OutputRegister(), i.MemoryOperand()); \
    __ sync();                                           \
  } while (0)

#define ASSEMBLE_ATOMIC_STORE_INTEGER(asm_instr)               \
  do {                                                         \
    __ sync();                                                 \
    __ asm_instr(i.InputOrZeroRegister(2), i.MemoryOperand()); \
    __ sync();                                                 \
  } while (0)

#define ASSEMBLE_ATOMIC_BINOP(bin_instr)                                 \
  do {                                                                   \
    Label binop;                                                         \
    __ Daddu(i.TempRegister(0), i.InputRegister(0), i.InputRegister(1)); \
    __ sync();                                                           \
    __ bind(&binop);                                                     \
    __ Ll(i.OutputRegister(0), MemOperand(i.TempRegister(0), 0));        \
    __ bin_instr(i.TempRegister(1), i.OutputRegister(0),                 \
                 Operand(i.InputRegister(2)));                           \
    __ Sc(i.TempRegister(1), MemOperand(i.TempRegister(0), 0));          \
    __ BranchShort(&binop, eq, i.TempRegister(1), Operand(zero_reg));    \
    __ sync();                                                           \
  } while (0)

#define ASSEMBLE_ATOMIC_BINOP_EXT(sign_extend, size, bin_instr)               \
  do {                                                                        \
    Label binop;                                                              \
    __ daddu(i.TempRegister(0), i.InputRegister(0), i.InputRegister(1));      \
    __ andi(i.TempRegister(3), i.TempRegister(0), 0x3);                       \
    __ Dsubu(i.TempRegister(0), i.TempRegister(0),                            \
             Operand(i.TempRegister(3)));                                     \
    __ sll(i.TempRegister(3), i.TempRegister(3), 3);                          \
    __ sync();                                                                \
    __ bind(&binop);                                                          \
    __ Ll(i.TempRegister(1), MemOperand(i.TempRegister(0), 0));               \
    __ ExtractBits(i.OutputRegister(0), i.TempRegister(1), i.TempRegister(3), \
                   size, sign_extend);                                        \
    __ bin_instr(i.TempRegister(2), i.OutputRegister(0),                      \
                 Operand(i.InputRegister(2)));                                \
    __ InsertBits(i.TempRegister(1), i.TempRegister(2), i.TempRegister(3),    \
                  size);                                                      \
    __ Sc(i.TempRegister(1), MemOperand(i.TempRegister(0), 0));               \
    __ BranchShort(&binop, eq, i.TempRegister(1), Operand(zero_reg));         \
    __ sync();                                                                \
  } while (0)

#define ASSEMBLE_ATOMIC_EXCHANGE_INTEGER()                               \
  do {                                                                   \
    Label exchange;                                                      \
    __ sync();                                                           \
    __ bind(&exchange);                                                  \
    __ daddu(i.TempRegister(0), i.InputRegister(0), i.InputRegister(1)); \
    __ Ll(i.OutputRegister(0), MemOperand(i.TempRegister(0), 0));        \
    __ mov(i.TempRegister(1), i.InputRegister(2));                       \
    __ Sc(i.TempRegister(1), MemOperand(i.TempRegister(0), 0));          \
    __ BranchShort(&exchange, eq, i.TempRegister(1), Operand(zero_reg)); \
    __ sync();                                                           \
  } while (0)

#define ASSEMBLE_ATOMIC_EXCHANGE_INTEGER_EXT(sign_extend, size)               \
  do {                                                                        \
    Label exchange;                                                           \
    __ daddu(i.TempRegister(0), i.InputRegister(0), i.InputRegister(1));      \
    __ andi(i.TempRegister(1), i.TempRegister(0), 0x3);                       \
    __ Dsubu(i.TempRegister(0), i.TempRegister(0),                            \
             Operand(i.TempRegister(1)));                                     \
    __ sll(i.TempRegister(1), i.TempRegister(1), 3);                          \
    __ sync();                                                                \
    __ bind(&exchange);                                                       \
    __ Ll(i.TempRegister(2), MemOperand(i.TempRegister(0), 0));               \
    __ ExtractBits(i.OutputRegister(0), i.TempRegister(2), i.TempRegister(1), \
                   size, sign_extend);                                        \
    __ InsertBits(i.TempRegister(2), i.InputRegister(2), i.TempRegister(1),   \
                  size);                                                      \
    __ Sc(i.TempRegister(2), MemOperand(i.TempRegister(0), 0));               \
    __ BranchShort(&exchange, eq, i.TempRegister(2), Operand(zero_reg));      \
    __ sync();                                                                \
  } while (0)

#define ASSEMBLE_ATOMIC_COMPARE_EXCHANGE_INTEGER()                       \
  do {                                                                   \
    Label compareExchange;                                               \
    Label exit;                                                          \
    __ daddu(i.TempRegister(0), i.InputRegister(0), i.InputRegister(1)); \
    __ sync();                                                           \
    __ bind(&compareExchange);                                           \
    __ Ll(i.OutputRegister(0), MemOperand(i.TempRegister(0), 0));        \
    __ BranchShort(&exit, ne, i.InputRegister(2),                        \
                   Operand(i.OutputRegister(0)));                        \
    __ mov(i.TempRegister(2), i.InputRegister(3));                       \
    __ Sc(i.TempRegister(2), MemOperand(i.TempRegister(0), 0));          \
    __ BranchShort(&compareExchange, eq, i.TempRegister(2),              \
                   Operand(zero_reg));                                   \
    __ bind(&exit);                                                      \
    __ sync();                                                           \
  } while (0)

#define ASSEMBLE_ATOMIC_COMPARE_EXCHANGE_INTEGER_EXT(sign_extend, size)       \
  do {                                                                        \
    Label compareExchange;                                                    \
    Label exit;                                                               \
    __ daddu(i.TempRegister(0), i.InputRegister(0), i.InputRegister(1));      \
    __ andi(i.TempRegister(1), i.TempRegister(0), 0x3);                       \
    __ Dsubu(i.TempRegister(0), i.TempRegister(0),                            \
             Operand(i.TempRegister(1)));                                     \
    __ sll(i.TempRegister(1), i.TempRegister(1), 3);                          \
    __ sync();                                                                \
    __ bind(&compareExchange);                                                \
    __ Ll(i.TempRegister(2), MemOperand(i.TempRegister(0), 0));               \
    __ ExtractBits(i.OutputRegister(0), i.TempRegister(2), i.TempRegister(1), \
                   size, sign_extend);                                        \
    __ BranchShort(&exit, ne, i.InputRegister(2),                             \
                   Operand(i.OutputRegister(0)));                             \
    __ InsertBits(i.TempRegister(2), i.InputRegister(3), i.TempRegister(1),   \
                  size);                                                      \
    __ Sc(i.TempRegister(2), MemOperand(i.TempRegister(0), 0));               \
    __ BranchShort(&compareExchange, eq, i.TempRegister(2),                   \
                   Operand(zero_reg));                                        \
    __ bind(&exit);                                                           \
    __ sync();                                                                \
  } while (0)

#define ASSEMBLE_IEEE754_BINOP(name)                                        \
  do {                                                                      \
    FrameScope scope(tasm(), StackFrame::MANUAL);                           \
    __ PrepareCallCFunction(0, 2, kScratchReg);                             \
    __ MovToFloatParameters(i.InputDoubleRegister(0),                       \
                            i.InputDoubleRegister(1));                      \
    __ CallCFunction(                                                       \
        ExternalReference::ieee754_##name##_function(tasm()->isolate()), 0, \
        2);                                                                 \
    /* Move the result in the double result register. */                    \
    __ MovFromFloatResult(i.OutputDoubleRegister());                        \
  } while (0)

#define ASSEMBLE_IEEE754_UNOP(name)                                         \
  do {                                                                      \
    FrameScope scope(tasm(), StackFrame::MANUAL);                           \
    __ PrepareCallCFunction(0, 1, kScratchReg);                             \
    __ MovToFloatParameter(i.InputDoubleRegister(0));                       \
    __ CallCFunction(                                                       \
        ExternalReference::ieee754_##name##_function(tasm()->isolate()), 0, \
        1);                                                                 \
    /* Move the result in the double result register. */                    \
    __ MovFromFloatResult(i.OutputDoubleRegister());                        \
  } while (0)

void CodeGenerator::AssembleDeconstructFrame() {
  __ mov(sp, fp);
  __ Pop(ra, fp);
}

void CodeGenerator::AssemblePrepareTailCall() {
  if (frame_access_state()->has_frame()) {
    __ Ld(ra, MemOperand(fp, StandardFrameConstants::kCallerPCOffset));
    __ Ld(fp, MemOperand(fp, StandardFrameConstants::kCallerFPOffset));
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
  __ Ld(scratch3, MemOperand(fp, StandardFrameConstants::kContextOffset));
  __ Branch(&done, ne, scratch3,
            Operand(StackFrame::TypeToMarker(StackFrame::ARGUMENTS_ADAPTOR)));

  // Load arguments count from current arguments adaptor frame (note, it
  // does not include receiver).
  Register caller_args_count_reg = scratch1;
  __ Ld(caller_args_count_reg,
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
  if (stack_slot_delta > 0) {
    tasm->Dsubu(sp, sp, stack_slot_delta * kPointerSize);
    state->IncreaseSPDelta(stack_slot_delta);
  } else if (allow_shrinkage && stack_slot_delta < 0) {
    tasm->Daddu(sp, sp, -stack_slot_delta * kPointerSize);
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
  AdjustStackPointerForTailCall(tasm(), frame_access_state(),
                                first_unused_stack_slot);
}

// Check if the code object is marked for deoptimization. If it is, then it
// jumps to the CompileLazyDeoptimizedCode builtin. In order to do this we need
// to:
//    1. load the address of the current instruction;
//    2. read from memory the word that contains that bit, which can be found in
//       the flags in the referenced {CodeDataContainer} object;
//    3. test kMarkedForDeoptimizationBit in those flags; and
//    4. if it is not zero then it jumps to the builtin.
void CodeGenerator::BailoutIfDeoptimized() {
  Label current;
  // This push on ra and the pop below together ensure that we restore the
  // register ra, which is needed while computing frames for deoptimization.
  __ push(ra);
  // The bal instruction puts the address of the current instruction into
  // the return address (ra) register, which we can use later on.
  __ bal(&current);
  __ nop();
  int pc = __ pc_offset();
  __ bind(&current);
  int offset = Code::kCodeDataContainerOffset - (Code::kHeaderSize + pc);
  __ Ld(a2, MemOperand(ra, offset));
  __ pop(ra);
  __ Lw(a2, FieldMemOperand(a2, CodeDataContainer::kKindSpecificFlagsOffset));
  __ And(a2, a2, Operand(1 << Code::kMarkedForDeoptimizationBit));
  Handle<Code> code = isolate()->builtins()->builtin_handle(
      Builtins::kCompileLazyDeoptimizedCode);
  __ Jump(code, RelocInfo::CODE_TARGET, ne, a2, Operand(zero_reg));
}

// Assembles an instruction after register allocation, producing machine code.
CodeGenerator::CodeGenResult CodeGenerator::AssembleArchInstruction(
    Instruction* instr) {
  MipsOperandConverter i(this, instr);
  InstructionCode opcode = instr->opcode();
  ArchOpcode arch_opcode = ArchOpcodeField::decode(opcode);
  switch (arch_opcode) {
    case kArchCallCodeObject: {
      if (instr->InputAt(0)->IsImmediate()) {
        __ Call(i.InputCode(0), RelocInfo::CODE_TARGET);
      } else {
        __ daddiu(at, i.InputRegister(0), Code::kHeaderSize - kHeapObjectTag);
        __ Call(at);
      }
      RecordCallPosition(instr);
      frame_access_state()->ClearSPDelta();
      break;
    }
    case kArchCallWasmFunction: {
      if (arch_opcode == kArchTailCallCodeObjectFromJSFunction) {
        AssemblePopArgumentsAdaptorFrame(kJavaScriptCallArgCountRegister,
                                         i.TempRegister(0), i.TempRegister(1),
                                         i.TempRegister(2));
      }
      if (instr->InputAt(0)->IsImmediate()) {
        Address wasm_code = reinterpret_cast<Address>(
            i.ToConstant(instr->InputAt(0)).ToInt64());
        __ Call(wasm_code, info()->IsWasm() ? RelocInfo::WASM_CALL
                                            : RelocInfo::JS_TO_WASM_CALL);
      } else {
        __ daddiu(at, i.InputRegister(0), 0);
        __ Call(at);
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
        __ daddiu(at, i.InputRegister(0), Code::kHeaderSize - kHeapObjectTag);
        __ Jump(at);
      }
      frame_access_state()->ClearSPDelta();
      frame_access_state()->SetFrameAccessToDefault();
      break;
    }
    case kArchTailCallWasm: {
      if (instr->InputAt(0)->IsImmediate()) {
        Address wasm_code = reinterpret_cast<Address>(
            i.ToConstant(instr->InputAt(0)).ToInt64());
        __ Jump(wasm_code, info()->IsWasm() ? RelocInfo::WASM_CALL
                                            : RelocInfo::JS_TO_WASM_CALL);
      } else {
        __ daddiu(at, i.InputRegister(0), 0);
        __ Jump(at);
      }
      frame_access_state()->ClearSPDelta();
      frame_access_state()->SetFrameAccessToDefault();
      break;
    }
    case kArchTailCallAddress: {
      CHECK(!instr->InputAt(0)->IsImmediate());
      __ Jump(i.InputRegister(0));
      frame_access_state()->ClearSPDelta();
      frame_access_state()->SetFrameAccessToDefault();
      break;
    }
    case kArchCallJSFunction: {
      Register func = i.InputRegister(0);
      if (FLAG_debug_code) {
        // Check the function's context matches the context argument.
        __ Ld(kScratchReg, FieldMemOperand(func, JSFunction::kContextOffset));
        __ Assert(eq, AbortReason::kWrongFunctionContext, cp,
                  Operand(kScratchReg));
      }
      __ Ld(at, FieldMemOperand(func, JSFunction::kCodeOffset));
      __ Daddu(at, at, Operand(Code::kHeaderSize - kHeapObjectTag));
      __ Call(at);
      RecordCallPosition(instr);
      frame_access_state()->ClearSPDelta();
      break;
    }
    case kArchPrepareCallCFunction: {
      int const num_parameters = MiscField::decode(instr->opcode());
      __ PrepareCallCFunction(num_parameters, kScratchReg);
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
      break;
    case kArchLookupSwitch:
      AssembleArchLookupSwitch(instr);
      break;
    case kArchTableSwitch:
      AssembleArchTableSwitch(instr);
      break;
    case kArchDebugAbort:
      DCHECK(i.InputRegister(0) == a0);
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
      break;
    case kArchDebugBreak:
      __ stop("kArchDebugBreak");
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
      __ mov(i.OutputRegister(), sp);
      break;
    case kArchFramePointer:
      __ mov(i.OutputRegister(), fp);
      break;
    case kArchParentFramePointer:
      if (frame_access_state()->has_frame()) {
        __ Ld(i.OutputRegister(), MemOperand(fp, 0));
      } else {
        __ mov(i.OutputRegister(), fp);
      }
      break;
    case kArchTruncateDoubleToI:
      __ TruncateDoubleToIDelayed(zone(), i.OutputRegister(),
                                  i.InputDoubleRegister(0));
      break;
    case kArchStoreWithWriteBarrier: {
      RecordWriteMode mode =
          static_cast<RecordWriteMode>(MiscField::decode(instr->opcode()));
      Register object = i.InputRegister(0);
      Register index = i.InputRegister(1);
      Register value = i.InputRegister(2);
      Register scratch0 = i.TempRegister(0);
      Register scratch1 = i.TempRegister(1);
      auto ool = new (zone()) OutOfLineRecordWrite(this, object, index, value,
                                                   scratch0, scratch1, mode);
      __ Daddu(at, object, index);
      __ Sd(value, MemOperand(at));
      __ CheckPageFlag(object, scratch0,
                       MemoryChunk::kPointersFromHereAreInterestingMask, ne,
                       ool->entry());
      __ bind(ool->exit());
      break;
    }
    case kArchStackSlot: {
      FrameOffset offset =
          frame_access_state()->GetFrameOffset(i.InputInt32(0));
      Register base_reg = offset.from_stack_pointer() ? sp : fp;
      __ Daddu(i.OutputRegister(), base_reg, Operand(offset.offset()));
      int alignment = i.InputInt32(1);
      DCHECK(alignment == 0 || alignment == 4 || alignment == 8 ||
             alignment == 16);
      if (FLAG_debug_code && alignment > 0) {
        // Verify that the output_register is properly aligned
        __ And(kScratchReg, i.OutputRegister(), Operand(kPointerSize - 1));
        __ Assert(eq, AbortReason::kAllocationIsNotDoubleAligned, kScratchReg,
                  Operand(zero_reg));
      }
      if (alignment == 2 * kPointerSize) {
        Label done;
        __ Daddu(kScratchReg, base_reg, Operand(offset.offset()));
        __ And(kScratchReg, kScratchReg, Operand(alignment - 1));
        __ BranchShort(&done, eq, kScratchReg, Operand(zero_reg));
        __ Daddu(i.OutputRegister(), i.OutputRegister(), kPointerSize);
        __ bind(&done);
      } else if (alignment > 2 * kPointerSize) {
        Label done;
        __ Daddu(kScratchReg, base_reg, Operand(offset.offset()));
        __ And(kScratchReg, kScratchReg, Operand(alignment - 1));
        __ BranchShort(&done, eq, kScratchReg, Operand(zero_reg));
        __ li(kScratchReg2, alignment);
        __ Dsubu(kScratchReg2, kScratchReg2, Operand(kScratchReg));
        __ Daddu(i.OutputRegister(), i.OutputRegister(), kScratchReg2);
        __ bind(&done);
      }

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
      __ CallStubDelayed(new (zone())
                             MathPowStub(nullptr, MathPowStub::DOUBLE));
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
    case kMips64Add:
      __ Addu(i.OutputRegister(), i.InputRegister(0), i.InputOperand(1));
      break;
    case kMips64Dadd:
      __ Daddu(i.OutputRegister(), i.InputRegister(0), i.InputOperand(1));
      break;
    case kMips64DaddOvf:
      // Pseudo-instruction used for overflow/branch. No opcode emitted here.
      break;
    case kMips64Sub:
      __ Subu(i.OutputRegister(), i.InputRegister(0), i.InputOperand(1));
      break;
    case kMips64Dsub:
      __ Dsubu(i.OutputRegister(), i.InputRegister(0), i.InputOperand(1));
      break;
    case kMips64DsubOvf:
      // Pseudo-instruction used for overflow/branch. No opcode emitted here.
      break;
    case kMips64Mul:
      __ Mul(i.OutputRegister(), i.InputRegister(0), i.InputOperand(1));
      break;
    case kMips64MulOvf:
      // Pseudo-instruction used for overflow/branch. No opcode emitted here.
      break;
    case kMips64MulHigh:
      __ Mulh(i.OutputRegister(), i.InputRegister(0), i.InputOperand(1));
      break;
    case kMips64MulHighU:
      __ Mulhu(i.OutputRegister(), i.InputRegister(0), i.InputOperand(1));
      break;
    case kMips64DMulHigh:
      __ Dmulh(i.OutputRegister(), i.InputRegister(0), i.InputOperand(1));
      break;
    case kMips64Div:
      __ Div(i.OutputRegister(), i.InputRegister(0), i.InputOperand(1));
      if (kArchVariant == kMips64r6) {
        __ selnez(i.OutputRegister(), i.InputRegister(0), i.InputRegister(1));
      } else {
        __ Movz(i.OutputRegister(), i.InputRegister(1), i.InputRegister(1));
      }
      break;
    case kMips64DivU:
      __ Divu(i.OutputRegister(), i.InputRegister(0), i.InputOperand(1));
      if (kArchVariant == kMips64r6) {
        __ selnez(i.OutputRegister(), i.InputRegister(0), i.InputRegister(1));
      } else {
        __ Movz(i.OutputRegister(), i.InputRegister(1), i.InputRegister(1));
      }
      break;
    case kMips64Mod:
      __ Mod(i.OutputRegister(), i.InputRegister(0), i.InputOperand(1));
      break;
    case kMips64ModU:
      __ Modu(i.OutputRegister(), i.InputRegister(0), i.InputOperand(1));
      break;
    case kMips64Dmul:
      __ Dmul(i.OutputRegister(), i.InputRegister(0), i.InputOperand(1));
      break;
    case kMips64Ddiv:
      __ Ddiv(i.OutputRegister(), i.InputRegister(0), i.InputOperand(1));
      if (kArchVariant == kMips64r6) {
        __ selnez(i.OutputRegister(), i.InputRegister(0), i.InputRegister(1));
      } else {
        __ Movz(i.OutputRegister(), i.InputRegister(1), i.InputRegister(1));
      }
      break;
    case kMips64DdivU:
      __ Ddivu(i.OutputRegister(), i.InputRegister(0), i.InputOperand(1));
      if (kArchVariant == kMips64r6) {
        __ selnez(i.OutputRegister(), i.InputRegister(0), i.InputRegister(1));
      } else {
        __ Movz(i.OutputRegister(), i.InputRegister(1), i.InputRegister(1));
      }
      break;
    case kMips64Dmod:
      __ Dmod(i.OutputRegister(), i.InputRegister(0), i.InputOperand(1));
      break;
    case kMips64DmodU:
      __ Dmodu(i.OutputRegister(), i.InputRegister(0), i.InputOperand(1));
      break;
    case kMips64Dlsa:
      DCHECK(instr->InputAt(2)->IsImmediate());
      __ Dlsa(i.OutputRegister(), i.InputRegister(0), i.InputRegister(1),
              i.InputInt8(2));
      break;
    case kMips64Lsa:
      DCHECK(instr->InputAt(2)->IsImmediate());
      __ Lsa(i.OutputRegister(), i.InputRegister(0), i.InputRegister(1),
             i.InputInt8(2));
      break;
    case kMips64And:
      __ And(i.OutputRegister(), i.InputRegister(0), i.InputOperand(1));
      break;
    case kMips64And32:
      if (instr->InputAt(1)->IsRegister()) {
        __ sll(i.InputRegister(0), i.InputRegister(0), 0x0);
        __ sll(i.InputRegister(1), i.InputRegister(1), 0x0);
        __ And(i.OutputRegister(), i.InputRegister(0), i.InputOperand(1));
      } else {
        __ sll(i.InputRegister(0), i.InputRegister(0), 0x0);
        __ And(i.OutputRegister(), i.InputRegister(0), i.InputOperand(1));
      }
      break;
    case kMips64Or:
      __ Or(i.OutputRegister(), i.InputRegister(0), i.InputOperand(1));
      break;
    case kMips64Or32:
      if (instr->InputAt(1)->IsRegister()) {
        __ sll(i.InputRegister(0), i.InputRegister(0), 0x0);
        __ sll(i.InputRegister(1), i.InputRegister(1), 0x0);
        __ Or(i.OutputRegister(), i.InputRegister(0), i.InputOperand(1));
      } else {
        __ sll(i.InputRegister(0), i.InputRegister(0), 0x0);
        __ Or(i.OutputRegister(), i.InputRegister(0), i.InputOperand(1));
      }
      break;
    case kMips64Nor:
      if (instr->InputAt(1)->IsRegister()) {
        __ Nor(i.OutputRegister(), i.InputRegister(0), i.InputOperand(1));
      } else {
        DCHECK_EQ(0, i.InputOperand(1).immediate());
        __ Nor(i.OutputRegister(), i.InputRegister(0), zero_reg);
      }
      break;
    case kMips64Nor32:
      if (instr->InputAt(1)->IsRegister()) {
        __ sll(i.InputRegister(0), i.InputRegister(0), 0x0);
        __ sll(i.InputRegister(1), i.InputRegister(1), 0x0);
        __ Nor(i.OutputRegister(), i.InputRegister(0), i.InputOperand(1));
      } else {
        DCHECK_EQ(0, i.InputOperand(1).immediate());
        __ sll(i.InputRegister(0), i.InputRegister(0), 0x0);
        __ Nor(i.OutputRegister(), i.InputRegister(0), zero_reg);
      }
      break;
    case kMips64Xor:
      __ Xor(i.OutputRegister(), i.InputRegister(0), i.InputOperand(1));
      break;
    case kMips64Xor32:
      if (instr->InputAt(1)->IsRegister()) {
        __ sll(i.InputRegister(0), i.InputRegister(0), 0x0);
        __ sll(i.InputRegister(1), i.InputRegister(1), 0x0);
        __ Xor(i.OutputRegister(), i.InputRegister(0), i.InputOperand(1));
      } else {
        __ sll(i.InputRegister(0), i.InputRegister(0), 0x0);
        __ Xor(i.OutputRegister(), i.InputRegister(0), i.InputOperand(1));
      }
      break;
    case kMips64Clz:
      __ Clz(i.OutputRegister(), i.InputRegister(0));
      break;
    case kMips64Dclz:
      __ dclz(i.OutputRegister(), i.InputRegister(0));
      break;
    case kMips64Ctz: {
      Register src = i.InputRegister(0);
      Register dst = i.OutputRegister();
      if (kArchVariant == kMips64r6) {
        // We don't have an instruction to count the number of trailing zeroes.
        // Start by flipping the bits end-for-end so we can count the number of
        // leading zeroes instead.
        __ rotr(dst, src, 16);
        __ wsbh(dst, dst);
        __ bitswap(dst, dst);
        __ Clz(dst, dst);
      } else {
        // Convert trailing zeroes to trailing ones, and bits to their left
        // to zeroes.
        __ Daddu(kScratchReg, src, -1);
        __ Xor(dst, kScratchReg, src);
        __ And(dst, dst, kScratchReg);
        // Count number of leading zeroes.
        __ Clz(dst, dst);
        // Subtract number of leading zeroes from 32 to get number of trailing
        // ones. Remember that the trailing ones were formerly trailing zeroes.
        __ li(kScratchReg, 32);
        __ Subu(dst, kScratchReg, dst);
      }
    } break;
    case kMips64Dctz: {
      Register src = i.InputRegister(0);
      Register dst = i.OutputRegister();
      if (kArchVariant == kMips64r6) {
        // We don't have an instruction to count the number of trailing zeroes.
        // Start by flipping the bits end-for-end so we can count the number of
        // leading zeroes instead.
        __ dsbh(dst, src);
        __ dshd(dst, dst);
        __ dbitswap(dst, dst);
        __ dclz(dst, dst);
      } else {
        // Convert trailing zeroes to trailing ones, and bits to their left
        // to zeroes.
        __ Daddu(kScratchReg, src, -1);
        __ Xor(dst, kScratchReg, src);
        __ And(dst, dst, kScratchReg);
        // Count number of leading zeroes.
        __ dclz(dst, dst);
        // Subtract number of leading zeroes from 64 to get number of trailing
        // ones. Remember that the trailing ones were formerly trailing zeroes.
        __ li(kScratchReg, 64);
        __ Dsubu(dst, kScratchReg, dst);
      }
    } break;
    case kMips64Popcnt: {
      // https://graphics.stanford.edu/~seander/bithacks.html#CountBitsSetParallel
      //
      // A generalization of the best bit counting method to integers of
      // bit-widths up to 128 (parameterized by type T) is this:
      //
      // v = v - ((v >> 1) & (T)~(T)0/3);                           // temp
      // v = (v & (T)~(T)0/15*3) + ((v >> 2) & (T)~(T)0/15*3);      // temp
      // v = (v + (v >> 4)) & (T)~(T)0/255*15;                      // temp
      // c = (T)(v * ((T)~(T)0/255)) >> (sizeof(T) - 1) * BITS_PER_BYTE; //count
      //
      // For comparison, for 32-bit quantities, this algorithm can be executed
      // using 20 MIPS instructions (the calls to LoadConst32() generate two
      // machine instructions each for the values being used in this algorithm).
      // A(n unrolled) loop-based algorithm requires 25 instructions.
      //
      // For a 64-bit operand this can be performed in 24 instructions compared
      // to a(n unrolled) loop based algorithm which requires 38 instructions.
      //
      // There are algorithms which are faster in the cases where very few
      // bits are set but the algorithm here attempts to minimize the total
      // number of instructions executed even when a large number of bits
      // are set.
      Register src = i.InputRegister(0);
      Register dst = i.OutputRegister();
      uint32_t B0 = 0x55555555;     // (T)~(T)0/3
      uint32_t B1 = 0x33333333;     // (T)~(T)0/15*3
      uint32_t B2 = 0x0F0F0F0F;     // (T)~(T)0/255*15
      uint32_t value = 0x01010101;  // (T)~(T)0/255
      uint32_t shift = 24;          // (sizeof(T) - 1) * BITS_PER_BYTE
      __ srl(kScratchReg, src, 1);
      __ li(kScratchReg2, B0);
      __ And(kScratchReg, kScratchReg, kScratchReg2);
      __ Subu(kScratchReg, src, kScratchReg);
      __ li(kScratchReg2, B1);
      __ And(dst, kScratchReg, kScratchReg2);
      __ srl(kScratchReg, kScratchReg, 2);
      __ And(kScratchReg, kScratchReg, kScratchReg2);
      __ Addu(kScratchReg, dst, kScratchReg);
      __ srl(dst, kScratchReg, 4);
      __ Addu(dst, dst, kScratchReg);
      __ li(kScratchReg2, B2);
      __ And(dst, dst, kScratchReg2);
      __ li(kScratchReg, value);
      __ Mul(dst, dst, kScratchReg);
      __ srl(dst, dst, shift);
    } break;
    case kMips64Dpopcnt: {
      Register src = i.InputRegister(0);
      Register dst = i.OutputRegister();
      uint64_t B0 = 0x5555555555555555l;     // (T)~(T)0/3
      uint64_t B1 = 0x3333333333333333l;     // (T)~(T)0/15*3
      uint64_t B2 = 0x0F0F0F0F0F0F0F0Fl;     // (T)~(T)0/255*15
      uint64_t value = 0x0101010101010101l;  // (T)~(T)0/255
      uint64_t shift = 24;                   // (sizeof(T) - 1) * BITS_PER_BYTE
      __ dsrl(kScratchReg, src, 1);
      __ li(kScratchReg2, B0);
      __ And(kScratchReg, kScratchReg, kScratchReg2);
      __ Dsubu(kScratchReg, src, kScratchReg);
      __ li(kScratchReg2, B1);
      __ And(dst, kScratchReg, kScratchReg2);
      __ dsrl(kScratchReg, kScratchReg, 2);
      __ And(kScratchReg, kScratchReg, kScratchReg2);
      __ Daddu(kScratchReg, dst, kScratchReg);
      __ dsrl(dst, kScratchReg, 4);
      __ Daddu(dst, dst, kScratchReg);
      __ li(kScratchReg2, B2);
      __ And(dst, dst, kScratchReg2);
      __ li(kScratchReg, value);
      __ Dmul(dst, dst, kScratchReg);
      __ dsrl32(dst, dst, shift);
    } break;
    case kMips64Shl:
      if (instr->InputAt(1)->IsRegister()) {
        __ sllv(i.OutputRegister(), i.InputRegister(0), i.InputRegister(1));
      } else {
        int64_t imm = i.InputOperand(1).immediate();
        __ sll(i.OutputRegister(), i.InputRegister(0),
               static_cast<uint16_t>(imm));
      }
      break;
    case kMips64Shr:
      if (instr->InputAt(1)->IsRegister()) {
        __ sll(i.InputRegister(0), i.InputRegister(0), 0x0);
        __ srlv(i.OutputRegister(), i.InputRegister(0), i.InputRegister(1));
      } else {
        int64_t imm = i.InputOperand(1).immediate();
        __ sll(i.InputRegister(0), i.InputRegister(0), 0x0);
        __ srl(i.OutputRegister(), i.InputRegister(0),
               static_cast<uint16_t>(imm));
      }
      break;
    case kMips64Sar:
      if (instr->InputAt(1)->IsRegister()) {
        __ sll(i.InputRegister(0), i.InputRegister(0), 0x0);
        __ srav(i.OutputRegister(), i.InputRegister(0), i.InputRegister(1));
      } else {
        int64_t imm = i.InputOperand(1).immediate();
        __ sll(i.InputRegister(0), i.InputRegister(0), 0x0);
        __ sra(i.OutputRegister(), i.InputRegister(0),
               static_cast<uint16_t>(imm));
      }
      break;
    case kMips64Ext:
      __ Ext(i.OutputRegister(), i.InputRegister(0), i.InputInt8(1),
             i.InputInt8(2));
      break;
    case kMips64Ins:
      if (instr->InputAt(1)->IsImmediate() && i.InputInt8(1) == 0) {
        __ Ins(i.OutputRegister(), zero_reg, i.InputInt8(1), i.InputInt8(2));
      } else {
        __ Ins(i.OutputRegister(), i.InputRegister(0), i.InputInt8(1),
               i.InputInt8(2));
      }
      break;
    case kMips64Dext: {
      __ Dext(i.OutputRegister(), i.InputRegister(0), i.InputInt8(1),
              i.InputInt8(2));
      break;
    }
    case kMips64Dins:
      if (instr->InputAt(1)->IsImmediate() && i.InputInt8(1) == 0) {
        __ Dins(i.OutputRegister(), zero_reg, i.InputInt8(1), i.InputInt8(2));
      } else {
        __ Dins(i.OutputRegister(), i.InputRegister(0), i.InputInt8(1),
                i.InputInt8(2));
      }
      break;
    case kMips64Dshl:
      if (instr->InputAt(1)->IsRegister()) {
        __ dsllv(i.OutputRegister(), i.InputRegister(0), i.InputRegister(1));
      } else {
        int64_t imm = i.InputOperand(1).immediate();
        if (imm < 32) {
          __ dsll(i.OutputRegister(), i.InputRegister(0),
                  static_cast<uint16_t>(imm));
        } else {
          __ dsll32(i.OutputRegister(), i.InputRegister(0),
                    static_cast<uint16_t>(imm - 32));
        }
      }
      break;
    case kMips64Dshr:
      if (instr->InputAt(1)->IsRegister()) {
        __ dsrlv(i.OutputRegister(), i.InputRegister(0), i.InputRegister(1));
      } else {
        int64_t imm = i.InputOperand(1).immediate();
        if (imm < 32) {
          __ dsrl(i.OutputRegister(), i.InputRegister(0),
                  static_cast<uint16_t>(imm));
        } else {
          __ dsrl32(i.OutputRegister(), i.InputRegister(0),
                    static_cast<uint16_t>(imm - 32));
        }
      }
      break;
    case kMips64Dsar:
      if (instr->InputAt(1)->IsRegister()) {
        __ dsrav(i.OutputRegister(), i.InputRegister(0), i.InputRegister(1));
      } else {
        int64_t imm = i.InputOperand(1).immediate();
        if (imm < 32) {
          __ dsra(i.OutputRegister(), i.InputRegister(0), imm);
        } else {
          __ dsra32(i.OutputRegister(), i.InputRegister(0), imm - 32);
        }
      }
      break;
    case kMips64Ror:
      __ Ror(i.OutputRegister(), i.InputRegister(0), i.InputOperand(1));
      break;
    case kMips64Dror:
      __ Dror(i.OutputRegister(), i.InputRegister(0), i.InputOperand(1));
      break;
    case kMips64Tst:
      // Pseudo-instruction used for cmp/branch. No opcode emitted here.
      break;
    case kMips64Cmp:
      // Pseudo-instruction used for cmp/branch. No opcode emitted here.
      break;
    case kMips64Mov:
      // TODO(plind): Should we combine mov/li like this, or use separate instr?
      //    - Also see x64 ASSEMBLE_BINOP & RegisterOrOperandType
      if (HasRegisterInput(instr, 0)) {
        __ mov(i.OutputRegister(), i.InputRegister(0));
      } else {
        __ li(i.OutputRegister(), i.InputOperand(0));
      }
      break;

    case kMips64CmpS:
      // Pseudo-instruction used for FP cmp/branch. No opcode emitted here.
      break;
    case kMips64AddS:
      // TODO(plind): add special case: combine mult & add.
      __ add_s(i.OutputDoubleRegister(), i.InputDoubleRegister(0),
               i.InputDoubleRegister(1));
      break;
    case kMips64SubS:
      __ sub_s(i.OutputDoubleRegister(), i.InputDoubleRegister(0),
               i.InputDoubleRegister(1));
      break;
    case kMips64MulS:
      // TODO(plind): add special case: right op is -1.0, see arm port.
      __ mul_s(i.OutputDoubleRegister(), i.InputDoubleRegister(0),
               i.InputDoubleRegister(1));
      break;
    case kMips64DivS:
      __ div_s(i.OutputDoubleRegister(), i.InputDoubleRegister(0),
               i.InputDoubleRegister(1));
      break;
    case kMips64ModS: {
      // TODO(bmeurer): We should really get rid of this special instruction,
      // and generate a CallAddress instruction instead.
      FrameScope scope(tasm(), StackFrame::MANUAL);
      __ PrepareCallCFunction(0, 2, kScratchReg);
      __ MovToFloatParameters(i.InputDoubleRegister(0),
                              i.InputDoubleRegister(1));
      // TODO(balazs.kilvady): implement mod_two_floats_operation(isolate())
      __ CallCFunction(
          ExternalReference::mod_two_doubles_operation(tasm()->isolate()), 0,
          2);
      // Move the result in the double result register.
      __ MovFromFloatResult(i.OutputSingleRegister());
      break;
    }
    case kMips64AbsS:
      __ abs_s(i.OutputSingleRegister(), i.InputSingleRegister(0));
      break;
    case kMips64NegS:
      __ Neg_s(i.OutputSingleRegister(), i.InputSingleRegister(0));
      break;
    case kMips64SqrtS: {
      __ sqrt_s(i.OutputDoubleRegister(), i.InputDoubleRegister(0));
      break;
    }
    case kMips64MaxS:
      __ max_s(i.OutputDoubleRegister(), i.InputDoubleRegister(0),
               i.InputDoubleRegister(1));
      break;
    case kMips64MinS:
      __ min_s(i.OutputDoubleRegister(), i.InputDoubleRegister(0),
               i.InputDoubleRegister(1));
      break;
    case kMips64CmpD:
      // Pseudo-instruction used for FP cmp/branch. No opcode emitted here.
      break;
    case kMips64AddD:
      // TODO(plind): add special case: combine mult & add.
      __ add_d(i.OutputDoubleRegister(), i.InputDoubleRegister(0),
               i.InputDoubleRegister(1));
      break;
    case kMips64SubD:
      __ sub_d(i.OutputDoubleRegister(), i.InputDoubleRegister(0),
               i.InputDoubleRegister(1));
      break;
    case kMips64MulD:
      // TODO(plind): add special case: right op is -1.0, see arm port.
      __ mul_d(i.OutputDoubleRegister(), i.InputDoubleRegister(0),
               i.InputDoubleRegister(1));
      break;
    case kMips64DivD:
      __ div_d(i.OutputDoubleRegister(), i.InputDoubleRegister(0),
               i.InputDoubleRegister(1));
      break;
    case kMips64ModD: {
      // TODO(bmeurer): We should really get rid of this special instruction,
      // and generate a CallAddress instruction instead.
      FrameScope scope(tasm(), StackFrame::MANUAL);
      __ PrepareCallCFunction(0, 2, kScratchReg);
      __ MovToFloatParameters(i.InputDoubleRegister(0),
                              i.InputDoubleRegister(1));
      __ CallCFunction(
          ExternalReference::mod_two_doubles_operation(tasm()->isolate()), 0,
          2);
      // Move the result in the double result register.
      __ MovFromFloatResult(i.OutputDoubleRegister());
      break;
    }
    case kMips64AbsD:
      __ abs_d(i.OutputDoubleRegister(), i.InputDoubleRegister(0));
      break;
    case kMips64NegD:
      __ Neg_d(i.OutputDoubleRegister(), i.InputDoubleRegister(0));
      break;
    case kMips64SqrtD: {
      __ sqrt_d(i.OutputDoubleRegister(), i.InputDoubleRegister(0));
      break;
    }
    case kMips64MaxD:
      __ max_d(i.OutputDoubleRegister(), i.InputDoubleRegister(0),
               i.InputDoubleRegister(1));
      break;
    case kMips64MinD:
      __ min_d(i.OutputDoubleRegister(), i.InputDoubleRegister(0),
               i.InputDoubleRegister(1));
      break;
    case kMips64Float64RoundDown: {
      ASSEMBLE_ROUND_DOUBLE_TO_DOUBLE(floor);
      break;
    }
    case kMips64Float32RoundDown: {
      ASSEMBLE_ROUND_FLOAT_TO_FLOAT(floor);
      break;
    }
    case kMips64Float64RoundTruncate: {
      ASSEMBLE_ROUND_DOUBLE_TO_DOUBLE(trunc);
      break;
    }
    case kMips64Float32RoundTruncate: {
      ASSEMBLE_ROUND_FLOAT_TO_FLOAT(trunc);
      break;
    }
    case kMips64Float64RoundUp: {
      ASSEMBLE_ROUND_DOUBLE_TO_DOUBLE(ceil);
      break;
    }
    case kMips64Float32RoundUp: {
      ASSEMBLE_ROUND_FLOAT_TO_FLOAT(ceil);
      break;
    }
    case kMips64Float64RoundTiesEven: {
      ASSEMBLE_ROUND_DOUBLE_TO_DOUBLE(round);
      break;
    }
    case kMips64Float32RoundTiesEven: {
      ASSEMBLE_ROUND_FLOAT_TO_FLOAT(round);
      break;
    }
    case kMips64Float32Max: {
      FPURegister dst = i.OutputSingleRegister();
      FPURegister src1 = i.InputSingleRegister(0);
      FPURegister src2 = i.InputSingleRegister(1);
      auto ool = new (zone()) OutOfLineFloat32Max(this, dst, src1, src2);
      __ Float32Max(dst, src1, src2, ool->entry());
      __ bind(ool->exit());
      break;
    }
    case kMips64Float64Max: {
      FPURegister dst = i.OutputDoubleRegister();
      FPURegister src1 = i.InputDoubleRegister(0);
      FPURegister src2 = i.InputDoubleRegister(1);
      auto ool = new (zone()) OutOfLineFloat64Max(this, dst, src1, src2);
      __ Float64Max(dst, src1, src2, ool->entry());
      __ bind(ool->exit());
      break;
    }
    case kMips64Float32Min: {
      FPURegister dst = i.OutputSingleRegister();
      FPURegister src1 = i.InputSingleRegister(0);
      FPURegister src2 = i.InputSingleRegister(1);
      auto ool = new (zone()) OutOfLineFloat32Min(this, dst, src1, src2);
      __ Float32Min(dst, src1, src2, ool->entry());
      __ bind(ool->exit());
      break;
    }
    case kMips64Float64Min: {
      FPURegister dst = i.OutputDoubleRegister();
      FPURegister src1 = i.InputDoubleRegister(0);
      FPURegister src2 = i.InputDoubleRegister(1);
      auto ool = new (zone()) OutOfLineFloat64Min(this, dst, src1, src2);
      __ Float64Min(dst, src1, src2, ool->entry());
      __ bind(ool->exit());
      break;
    }
    case kMips64Float64SilenceNaN:
      __ FPUCanonicalizeNaN(i.OutputDoubleRegister(), i.InputDoubleRegister(0));
      break;
    case kMips64CvtSD:
      __ cvt_s_d(i.OutputSingleRegister(), i.InputDoubleRegister(0));
      break;
    case kMips64CvtDS:
      __ cvt_d_s(i.OutputDoubleRegister(), i.InputSingleRegister(0));
      break;
    case kMips64CvtDW: {
      FPURegister scratch = kScratchDoubleReg;
      __ mtc1(i.InputRegister(0), scratch);
      __ cvt_d_w(i.OutputDoubleRegister(), scratch);
      break;
    }
    case kMips64CvtSW: {
      FPURegister scratch = kScratchDoubleReg;
      __ mtc1(i.InputRegister(0), scratch);
      __ cvt_s_w(i.OutputDoubleRegister(), scratch);
      break;
    }
    case kMips64CvtSUw: {
      __ Cvt_s_uw(i.OutputDoubleRegister(), i.InputRegister(0));
      break;
    }
    case kMips64CvtSL: {
      FPURegister scratch = kScratchDoubleReg;
      __ dmtc1(i.InputRegister(0), scratch);
      __ cvt_s_l(i.OutputDoubleRegister(), scratch);
      break;
    }
    case kMips64CvtDL: {
      FPURegister scratch = kScratchDoubleReg;
      __ dmtc1(i.InputRegister(0), scratch);
      __ cvt_d_l(i.OutputDoubleRegister(), scratch);
      break;
    }
    case kMips64CvtDUw: {
      __ Cvt_d_uw(i.OutputDoubleRegister(), i.InputRegister(0));
      break;
    }
    case kMips64CvtDUl: {
      __ Cvt_d_ul(i.OutputDoubleRegister(), i.InputRegister(0));
      break;
    }
    case kMips64CvtSUl: {
      __ Cvt_s_ul(i.OutputDoubleRegister(), i.InputRegister(0));
      break;
    }
    case kMips64FloorWD: {
      FPURegister scratch = kScratchDoubleReg;
      __ floor_w_d(scratch, i.InputDoubleRegister(0));
      __ mfc1(i.OutputRegister(), scratch);
      break;
    }
    case kMips64CeilWD: {
      FPURegister scratch = kScratchDoubleReg;
      __ ceil_w_d(scratch, i.InputDoubleRegister(0));
      __ mfc1(i.OutputRegister(), scratch);
      break;
    }
    case kMips64RoundWD: {
      FPURegister scratch = kScratchDoubleReg;
      __ round_w_d(scratch, i.InputDoubleRegister(0));
      __ mfc1(i.OutputRegister(), scratch);
      break;
    }
    case kMips64TruncWD: {
      FPURegister scratch = kScratchDoubleReg;
      // Other arches use round to zero here, so we follow.
      __ trunc_w_d(scratch, i.InputDoubleRegister(0));
      __ mfc1(i.OutputRegister(), scratch);
      break;
    }
    case kMips64FloorWS: {
      FPURegister scratch = kScratchDoubleReg;
      __ floor_w_s(scratch, i.InputDoubleRegister(0));
      __ mfc1(i.OutputRegister(), scratch);
      break;
    }
    case kMips64CeilWS: {
      FPURegister scratch = kScratchDoubleReg;
      __ ceil_w_s(scratch, i.InputDoubleRegister(0));
      __ mfc1(i.OutputRegister(), scratch);
      break;
    }
    case kMips64RoundWS: {
      FPURegister scratch = kScratchDoubleReg;
      __ round_w_s(scratch, i.InputDoubleRegister(0));
      __ mfc1(i.OutputRegister(), scratch);
      break;
    }
    case kMips64TruncWS: {
      FPURegister scratch = kScratchDoubleReg;
      __ trunc_w_s(scratch, i.InputDoubleRegister(0));
      __ mfc1(i.OutputRegister(), scratch);
      // Avoid INT32_MAX as an overflow indicator and use INT32_MIN instead,
      // because INT32_MIN allows easier out-of-bounds detection.
      __ addiu(kScratchReg, i.OutputRegister(), 1);
      __ slt(kScratchReg2, kScratchReg, i.OutputRegister());
      __ Movn(i.OutputRegister(), kScratchReg, kScratchReg2);
      break;
    }
    case kMips64TruncLS: {
      FPURegister scratch = kScratchDoubleReg;
      Register tmp_fcsr = kScratchReg;
      Register result = kScratchReg2;

      bool load_status = instr->OutputCount() > 1;
      if (load_status) {
        // Save FCSR.
        __ cfc1(tmp_fcsr, FCSR);
        // Clear FPU flags.
        __ ctc1(zero_reg, FCSR);
      }
      // Other arches use round to zero here, so we follow.
      __ trunc_l_s(scratch, i.InputDoubleRegister(0));
      __ dmfc1(i.OutputRegister(), scratch);
      if (load_status) {
        __ cfc1(result, FCSR);
        // Check for overflow and NaNs.
        __ andi(result, result,
                (kFCSROverflowFlagMask | kFCSRInvalidOpFlagMask));
        __ Slt(result, zero_reg, result);
        __ xori(result, result, 1);
        __ mov(i.OutputRegister(1), result);
        // Restore FCSR
        __ ctc1(tmp_fcsr, FCSR);
      }
      break;
    }
    case kMips64TruncLD: {
      FPURegister scratch = kScratchDoubleReg;
      Register tmp_fcsr = kScratchReg;
      Register result = kScratchReg2;

      bool load_status = instr->OutputCount() > 1;
      if (load_status) {
        // Save FCSR.
        __ cfc1(tmp_fcsr, FCSR);
        // Clear FPU flags.
        __ ctc1(zero_reg, FCSR);
      }
      // Other arches use round to zero here, so we follow.
      __ trunc_l_d(scratch, i.InputDoubleRegister(0));
      __ dmfc1(i.OutputRegister(0), scratch);
      if (load_status) {
        __ cfc1(result, FCSR);
        // Check for overflow and NaNs.
        __ andi(result, result,
                (kFCSROverflowFlagMask | kFCSRInvalidOpFlagMask));
        __ Slt(result, zero_reg, result);
        __ xori(result, result, 1);
        __ mov(i.OutputRegister(1), result);
        // Restore FCSR
        __ ctc1(tmp_fcsr, FCSR);
      }
      break;
    }
    case kMips64TruncUwD: {
      FPURegister scratch = kScratchDoubleReg;
      // TODO(plind): Fix wrong param order of Trunc_uw_d() macro-asm function.
      __ Trunc_uw_d(i.InputDoubleRegister(0), i.OutputRegister(), scratch);
      break;
    }
    case kMips64TruncUwS: {
      FPURegister scratch = kScratchDoubleReg;
      // TODO(plind): Fix wrong param order of Trunc_uw_d() macro-asm function.
      __ Trunc_uw_s(i.InputDoubleRegister(0), i.OutputRegister(), scratch);
      // Avoid UINT32_MAX as an overflow indicator and use 0 instead,
      // because 0 allows easier out-of-bounds detection.
      __ addiu(kScratchReg, i.OutputRegister(), 1);
      __ Movz(i.OutputRegister(), zero_reg, kScratchReg);
      break;
    }
    case kMips64TruncUlS: {
      FPURegister scratch = kScratchDoubleReg;
      Register result = instr->OutputCount() > 1 ? i.OutputRegister(1) : no_reg;
      // TODO(plind): Fix wrong param order of Trunc_ul_s() macro-asm function.
      __ Trunc_ul_s(i.InputDoubleRegister(0), i.OutputRegister(), scratch,
                    result);
      break;
    }
    case kMips64TruncUlD: {
      FPURegister scratch = kScratchDoubleReg;
      Register result = instr->OutputCount() > 1 ? i.OutputRegister(1) : no_reg;
      // TODO(plind): Fix wrong param order of Trunc_ul_d() macro-asm function.
      __ Trunc_ul_d(i.InputDoubleRegister(0), i.OutputRegister(0), scratch,
                    result);
      break;
    }
    case kMips64BitcastDL:
      __ dmfc1(i.OutputRegister(), i.InputDoubleRegister(0));
      break;
    case kMips64BitcastLD:
      __ dmtc1(i.InputRegister(0), i.OutputDoubleRegister());
      break;
    case kMips64Float64ExtractLowWord32:
      __ FmoveLow(i.OutputRegister(), i.InputDoubleRegister(0));
      break;
    case kMips64Float64ExtractHighWord32:
      __ FmoveHigh(i.OutputRegister(), i.InputDoubleRegister(0));
      break;
    case kMips64Float64InsertLowWord32:
      __ FmoveLow(i.OutputDoubleRegister(), i.InputRegister(1));
      break;
    case kMips64Float64InsertHighWord32:
      __ FmoveHigh(i.OutputDoubleRegister(), i.InputRegister(1));
      break;
    // ... more basic instructions ...

    case kMips64Seb:
      __ seb(i.OutputRegister(), i.InputRegister(0));
      break;
    case kMips64Seh:
      __ seh(i.OutputRegister(), i.InputRegister(0));
      break;
    case kMips64Lbu:
      __ Lbu(i.OutputRegister(), i.MemoryOperand());
      break;
    case kMips64Lb:
      __ Lb(i.OutputRegister(), i.MemoryOperand());
      break;
    case kMips64Sb:
      __ Sb(i.InputOrZeroRegister(2), i.MemoryOperand());
      break;
    case kMips64Lhu:
      __ Lhu(i.OutputRegister(), i.MemoryOperand());
      break;
    case kMips64Ulhu:
      __ Ulhu(i.OutputRegister(), i.MemoryOperand());
      break;
    case kMips64Lh:
      __ Lh(i.OutputRegister(), i.MemoryOperand());
      break;
    case kMips64Ulh:
      __ Ulh(i.OutputRegister(), i.MemoryOperand());
      break;
    case kMips64Sh:
      __ Sh(i.InputOrZeroRegister(2), i.MemoryOperand());
      break;
    case kMips64Ush:
      __ Ush(i.InputOrZeroRegister(2), i.MemoryOperand(), kScratchReg);
      break;
    case kMips64Lw:
      __ Lw(i.OutputRegister(), i.MemoryOperand());
      break;
    case kMips64Ulw:
      __ Ulw(i.OutputRegister(), i.MemoryOperand());
      break;
    case kMips64Lwu:
      __ Lwu(i.OutputRegister(), i.MemoryOperand());
      break;
    case kMips64Ulwu:
      __ Ulwu(i.OutputRegister(), i.MemoryOperand());
      break;
    case kMips64Ld:
      __ Ld(i.OutputRegister(), i.MemoryOperand());
      break;
    case kMips64Uld:
      __ Uld(i.OutputRegister(), i.MemoryOperand());
      break;
    case kMips64Sw:
      __ Sw(i.InputOrZeroRegister(2), i.MemoryOperand());
      break;
    case kMips64Usw:
      __ Usw(i.InputOrZeroRegister(2), i.MemoryOperand());
      break;
    case kMips64Sd:
      __ Sd(i.InputOrZeroRegister(2), i.MemoryOperand());
      break;
    case kMips64Usd:
      __ Usd(i.InputOrZeroRegister(2), i.MemoryOperand());
      break;
    case kMips64Lwc1: {
      __ Lwc1(i.OutputSingleRegister(), i.MemoryOperand());
      break;
    }
    case kMips64Ulwc1: {
      __ Ulwc1(i.OutputSingleRegister(), i.MemoryOperand(), kScratchReg);
      break;
    }
    case kMips64Swc1: {
      size_t index = 0;
      MemOperand operand = i.MemoryOperand(&index);
      FPURegister ft = i.InputOrZeroSingleRegister(index);
      if (ft == kDoubleRegZero && !__ IsDoubleZeroRegSet()) {
        __ Move(kDoubleRegZero, 0.0);
      }
      __ Swc1(ft, operand);
      break;
    }
    case kMips64Uswc1: {
      size_t index = 0;
      MemOperand operand = i.MemoryOperand(&index);
      FPURegister ft = i.InputOrZeroSingleRegister(index);
      if (ft == kDoubleRegZero && !__ IsDoubleZeroRegSet()) {
        __ Move(kDoubleRegZero, 0.0);
      }
      __ Uswc1(ft, operand, kScratchReg);
      break;
    }
    case kMips64Ldc1:
      __ Ldc1(i.OutputDoubleRegister(), i.MemoryOperand());
      break;
    case kMips64Uldc1:
      __ Uldc1(i.OutputDoubleRegister(), i.MemoryOperand(), kScratchReg);
      break;
    case kMips64Sdc1: {
      FPURegister ft = i.InputOrZeroDoubleRegister(2);
      if (ft == kDoubleRegZero && !__ IsDoubleZeroRegSet()) {
        __ Move(kDoubleRegZero, 0.0);
      }
      __ Sdc1(ft, i.MemoryOperand());
      break;
    }
    case kMips64Usdc1: {
      FPURegister ft = i.InputOrZeroDoubleRegister(2);
      if (ft == kDoubleRegZero && !__ IsDoubleZeroRegSet()) {
        __ Move(kDoubleRegZero, 0.0);
      }
      __ Usdc1(ft, i.MemoryOperand(), kScratchReg);
      break;
    }
    case kMips64Push:
      if (instr->InputAt(0)->IsFPRegister()) {
        __ Sdc1(i.InputDoubleRegister(0), MemOperand(sp, -kDoubleSize));
        __ Subu(sp, sp, Operand(kDoubleSize));
        frame_access_state()->IncreaseSPDelta(kDoubleSize / kPointerSize);
      } else {
        __ Push(i.InputRegister(0));
        frame_access_state()->IncreaseSPDelta(1);
      }
      break;
    case kMips64Peek: {
      // The incoming value is 0-based, but we need a 1-based value.
      int reverse_slot = MiscField::decode(instr->opcode()) + 1;
      int offset =
          FrameSlotToFPOffset(frame()->GetTotalFrameSlotCount() - reverse_slot);
      if (instr->OutputAt(0)->IsFPRegister()) {
        LocationOperand* op = LocationOperand::cast(instr->OutputAt(0));
        if (op->representation() == MachineRepresentation::kFloat64) {
          __ Ldc1(i.OutputDoubleRegister(), MemOperand(fp, offset));
        } else {
          DCHECK_EQ(op->representation(), MachineRepresentation::kFloat32);
          __ lwc1(i.OutputSingleRegister(0), MemOperand(fp, offset));
        }
      } else {
        __ Ld(i.OutputRegister(0), MemOperand(fp, offset));
      }
      break;
    }
    case kMips64StackClaim: {
      __ Dsubu(sp, sp, Operand(i.InputInt32(0)));
      frame_access_state()->IncreaseSPDelta(i.InputInt32(0) / kPointerSize);
      break;
    }
    case kMips64StoreToStackSlot: {
      if (instr->InputAt(0)->IsFPRegister()) {
        __ Sdc1(i.InputDoubleRegister(0), MemOperand(sp, i.InputInt32(1)));
      } else {
        __ Sd(i.InputRegister(0), MemOperand(sp, i.InputInt32(1)));
      }
      break;
    }
    case kMips64ByteSwap64: {
      __ ByteSwapSigned(i.OutputRegister(0), i.InputRegister(0), 8);
      break;
    }
    case kMips64ByteSwap32: {
      __ ByteSwapUnsigned(i.OutputRegister(0), i.InputRegister(0), 4);
      __ dsrl32(i.OutputRegister(0), i.OutputRegister(0), 0);
      break;
    }
    case kAtomicLoadInt8:
      ASSEMBLE_ATOMIC_LOAD_INTEGER(Lb);
      break;
    case kAtomicLoadUint8:
      ASSEMBLE_ATOMIC_LOAD_INTEGER(Lbu);
      break;
    case kAtomicLoadInt16:
      ASSEMBLE_ATOMIC_LOAD_INTEGER(Lh);
      break;
    case kAtomicLoadUint16:
      ASSEMBLE_ATOMIC_LOAD_INTEGER(Lhu);
      break;
    case kAtomicLoadWord32:
      ASSEMBLE_ATOMIC_LOAD_INTEGER(Lw);
      break;
    case kAtomicStoreWord8:
      ASSEMBLE_ATOMIC_STORE_INTEGER(Sb);
      break;
    case kAtomicStoreWord16:
      ASSEMBLE_ATOMIC_STORE_INTEGER(Sh);
      break;
    case kAtomicStoreWord32:
      ASSEMBLE_ATOMIC_STORE_INTEGER(Sw);
      break;
    case kAtomicExchangeInt8:
      ASSEMBLE_ATOMIC_EXCHANGE_INTEGER_EXT(true, 8);
      break;
    case kAtomicExchangeUint8:
      ASSEMBLE_ATOMIC_EXCHANGE_INTEGER_EXT(false, 8);
      break;
    case kAtomicExchangeInt16:
      ASSEMBLE_ATOMIC_EXCHANGE_INTEGER_EXT(true, 16);
      break;
    case kAtomicExchangeUint16:
      ASSEMBLE_ATOMIC_EXCHANGE_INTEGER_EXT(false, 16);
      break;
    case kAtomicExchangeWord32:
      ASSEMBLE_ATOMIC_EXCHANGE_INTEGER();
      break;
    case kAtomicCompareExchangeInt8:
      ASSEMBLE_ATOMIC_COMPARE_EXCHANGE_INTEGER_EXT(true, 8);
      break;
    case kAtomicCompareExchangeUint8:
      ASSEMBLE_ATOMIC_COMPARE_EXCHANGE_INTEGER_EXT(false, 8);
      break;
    case kAtomicCompareExchangeInt16:
      ASSEMBLE_ATOMIC_COMPARE_EXCHANGE_INTEGER_EXT(true, 16);
      break;
    case kAtomicCompareExchangeUint16:
      ASSEMBLE_ATOMIC_COMPARE_EXCHANGE_INTEGER_EXT(false, 16);
      break;
    case kAtomicCompareExchangeWord32:
      __ sll(i.InputRegister(2), i.InputRegister(2), 0);
      ASSEMBLE_ATOMIC_COMPARE_EXCHANGE_INTEGER();
      break;
#define ATOMIC_BINOP_CASE(op, inst)             \
  case kAtomic##op##Int8:                       \
    ASSEMBLE_ATOMIC_BINOP_EXT(true, 8, inst);   \
    break;                                      \
  case kAtomic##op##Uint8:                      \
    ASSEMBLE_ATOMIC_BINOP_EXT(false, 8, inst);  \
    break;                                      \
  case kAtomic##op##Int16:                      \
    ASSEMBLE_ATOMIC_BINOP_EXT(true, 16, inst);  \
    break;                                      \
  case kAtomic##op##Uint16:                     \
    ASSEMBLE_ATOMIC_BINOP_EXT(false, 16, inst); \
    break;                                      \
  case kAtomic##op##Word32:                     \
    ASSEMBLE_ATOMIC_BINOP(inst);                \
    break;
      ATOMIC_BINOP_CASE(Add, Addu)
      ATOMIC_BINOP_CASE(Sub, Subu)
      ATOMIC_BINOP_CASE(And, And)
      ATOMIC_BINOP_CASE(Or, Or)
      ATOMIC_BINOP_CASE(Xor, Xor)
#undef ATOMIC_BINOP_CASE
    case kMips64AssertEqual:
      __ Assert(eq, static_cast<AbortReason>(i.InputOperand(2).immediate()),
                i.InputRegister(0), Operand(i.InputRegister(1)));
      break;
    case kMips64S128Zero: {
      CpuFeatureScope msa_scope(tasm(), MIPS_SIMD);
      __ xor_v(i.OutputSimd128Register(), i.OutputSimd128Register(),
               i.OutputSimd128Register());
      break;
    }
    case kMips64I32x4Splat: {
      CpuFeatureScope msa_scope(tasm(), MIPS_SIMD);
      __ fill_w(i.OutputSimd128Register(), i.InputRegister(0));
      break;
    }
    case kMips64I32x4ExtractLane: {
      CpuFeatureScope msa_scope(tasm(), MIPS_SIMD);
      __ copy_s_w(i.OutputRegister(), i.InputSimd128Register(0),
                  i.InputInt8(1));
      break;
    }
    case kMips64I32x4ReplaceLane: {
      CpuFeatureScope msa_scope(tasm(), MIPS_SIMD);
      Simd128Register src = i.InputSimd128Register(0);
      Simd128Register dst = i.OutputSimd128Register();
      if (src != dst) {
        __ move_v(dst, src);
      }
      __ insert_w(dst, i.InputInt8(1), i.InputRegister(2));
      break;
    }
    case kMips64I32x4Add: {
      CpuFeatureScope msa_scope(tasm(), MIPS_SIMD);
      __ addv_w(i.OutputSimd128Register(), i.InputSimd128Register(0),
                i.InputSimd128Register(1));
      break;
    }
    case kMips64I32x4Sub: {
      CpuFeatureScope msa_scope(tasm(), MIPS_SIMD);
      __ subv_w(i.OutputSimd128Register(), i.InputSimd128Register(0),
                i.InputSimd128Register(1));
      break;
    }
    case kMips64F32x4Splat: {
      CpuFeatureScope msa_scope(tasm(), MIPS_SIMD);
      __ FmoveLow(kScratchReg, i.InputSingleRegister(0));
      __ fill_w(i.OutputSimd128Register(), kScratchReg);
      break;
    }
    case kMips64F32x4ExtractLane: {
      CpuFeatureScope msa_scope(tasm(), MIPS_SIMD);
      __ copy_u_w(kScratchReg, i.InputSimd128Register(0), i.InputInt8(1));
      __ FmoveLow(i.OutputSingleRegister(), kScratchReg);
      break;
    }
    case kMips64F32x4ReplaceLane: {
      CpuFeatureScope msa_scope(tasm(), MIPS_SIMD);
      Simd128Register src = i.InputSimd128Register(0);
      Simd128Register dst = i.OutputSimd128Register();
      if (src != dst) {
        __ move_v(dst, src);
      }
      __ FmoveLow(kScratchReg, i.InputSingleRegister(2));
      __ insert_w(dst, i.InputInt8(1), kScratchReg);
      break;
    }
    case kMips64F32x4SConvertI32x4: {
      CpuFeatureScope msa_scope(tasm(), MIPS_SIMD);
      __ ffint_s_w(i.OutputSimd128Register(), i.InputSimd128Register(0));
      break;
    }
    case kMips64F32x4UConvertI32x4: {
      CpuFeatureScope msa_scope(tasm(), MIPS_SIMD);
      __ ffint_u_w(i.OutputSimd128Register(), i.InputSimd128Register(0));
      break;
    }
    case kMips64I32x4Mul: {
      CpuFeatureScope msa_scope(tasm(), MIPS_SIMD);
      __ mulv_w(i.OutputSimd128Register(), i.InputSimd128Register(0),
                i.InputSimd128Register(1));
      break;
    }
    case kMips64I32x4MaxS: {
      CpuFeatureScope msa_scope(tasm(), MIPS_SIMD);
      __ max_s_w(i.OutputSimd128Register(), i.InputSimd128Register(0),
                 i.InputSimd128Register(1));
      break;
    }
    case kMips64I32x4MinS: {
      CpuFeatureScope msa_scope(tasm(), MIPS_SIMD);
      __ min_s_w(i.OutputSimd128Register(), i.InputSimd128Register(0),
                 i.InputSimd128Register(1));
      break;
    }
    case kMips64I32x4Eq: {
      CpuFeatureScope msa_scope(tasm(), MIPS_SIMD);
      __ ceq_w(i.OutputSimd128Register(), i.InputSimd128Register(0),
               i.InputSimd128Register(1));
      break;
    }
    case kMips64I32x4Ne: {
      CpuFeatureScope msa_scope(tasm(), MIPS_SIMD);
      Simd128Register dst = i.OutputSimd128Register();
      __ ceq_w(dst, i.InputSimd128Register(0), i.InputSimd128Register(1));
      __ nor_v(dst, dst, dst);
      break;
    }
    case kMips64I32x4Shl: {
      CpuFeatureScope msa_scope(tasm(), MIPS_SIMD);
      __ slli_w(i.OutputSimd128Register(), i.InputSimd128Register(0),
                i.InputInt5(1));
      break;
    }
    case kMips64I32x4ShrS: {
      CpuFeatureScope msa_scope(tasm(), MIPS_SIMD);
      __ srai_w(i.OutputSimd128Register(), i.InputSimd128Register(0),
                i.InputInt5(1));
      break;
    }
    case kMips64I32x4ShrU: {
      CpuFeatureScope msa_scope(tasm(), MIPS_SIMD);
      __ srli_w(i.OutputSimd128Register(), i.InputSimd128Register(0),
                i.InputInt5(1));
      break;
    }
    case kMips64I32x4MaxU: {
      CpuFeatureScope msa_scope(tasm(), MIPS_SIMD);
      __ max_u_w(i.OutputSimd128Register(), i.InputSimd128Register(0),
                 i.InputSimd128Register(1));
      break;
    }
    case kMips64I32x4MinU: {
      CpuFeatureScope msa_scope(tasm(), MIPS_SIMD);
      __ min_u_w(i.OutputSimd128Register(), i.InputSimd128Register(0),
                 i.InputSimd128Register(1));
      break;
    }
    case kMips64S128Select: {
      CpuFeatureScope msa_scope(tasm(), MIPS_SIMD);
      DCHECK(i.OutputSimd128Register() == i.InputSimd128Register(0));
      __ bsel_v(i.OutputSimd128Register(), i.InputSimd128Register(2),
                i.InputSimd128Register(1));
      break;
    }
    case kMips64F32x4Abs: {
      CpuFeatureScope msa_scope(tasm(), MIPS_SIMD);
      __ bclri_w(i.OutputSimd128Register(), i.InputSimd128Register(0), 31);
      break;
    }
    case kMips64F32x4Neg: {
      CpuFeatureScope msa_scope(tasm(), MIPS_SIMD);
      __ bnegi_w(i.OutputSimd128Register(), i.InputSimd128Register(0), 31);
      break;
    }
    case kMips64F32x4RecipApprox: {
      CpuFeatureScope msa_scope(tasm(), MIPS_SIMD);
      __ frcp_w(i.OutputSimd128Register(), i.InputSimd128Register(0));
      break;
    }
    case kMips64F32x4RecipSqrtApprox: {
      CpuFeatureScope msa_scope(tasm(), MIPS_SIMD);
      __ frsqrt_w(i.OutputSimd128Register(), i.InputSimd128Register(0));
      break;
    }
    case kMips64F32x4Add: {
      CpuFeatureScope msa_scope(tasm(), MIPS_SIMD);
      __ fadd_w(i.OutputSimd128Register(), i.InputSimd128Register(0),
                i.InputSimd128Register(1));
      break;
    }
    case kMips64F32x4Sub: {
      CpuFeatureScope msa_scope(tasm(), MIPS_SIMD);
      __ fsub_w(i.OutputSimd128Register(), i.InputSimd128Register(0),
                i.InputSimd128Register(1));
      break;
    }
    case kMips64F32x4Mul: {
      CpuFeatureScope msa_scope(tasm(), MIPS_SIMD);
      __ fmul_w(i.OutputSimd128Register(), i.InputSimd128Register(0),
                i.InputSimd128Register(1));
      break;
    }
    case kMips64F32x4Max: {
      CpuFeatureScope msa_scope(tasm(), MIPS_SIMD);
      __ fmax_w(i.OutputSimd128Register(), i.InputSimd128Register(0),
                i.InputSimd128Register(1));
      break;
    }
    case kMips64F32x4Min: {
      CpuFeatureScope msa_scope(tasm(), MIPS_SIMD);
      __ fmin_w(i.OutputSimd128Register(), i.InputSimd128Register(0),
                i.InputSimd128Register(1));
      break;
    }
    case kMips64F32x4Eq: {
      CpuFeatureScope msa_scope(tasm(), MIPS_SIMD);
      __ fceq_w(i.OutputSimd128Register(), i.InputSimd128Register(0),
                i.InputSimd128Register(1));
      break;
    }
    case kMips64F32x4Ne: {
      CpuFeatureScope msa_scope(tasm(), MIPS_SIMD);
      __ fcne_w(i.OutputSimd128Register(), i.InputSimd128Register(0),
                i.InputSimd128Register(1));
      break;
    }
    case kMips64F32x4Lt: {
      CpuFeatureScope msa_scope(tasm(), MIPS_SIMD);
      __ fclt_w(i.OutputSimd128Register(), i.InputSimd128Register(0),
                i.InputSimd128Register(1));
      break;
    }
    case kMips64F32x4Le: {
      CpuFeatureScope msa_scope(tasm(), MIPS_SIMD);
      __ fcle_w(i.OutputSimd128Register(), i.InputSimd128Register(0),
                i.InputSimd128Register(1));
      break;
    }
    case kMips64I32x4SConvertF32x4: {
      CpuFeatureScope msa_scope(tasm(), MIPS_SIMD);
      __ ftrunc_s_w(i.OutputSimd128Register(), i.InputSimd128Register(0));
      break;
    }
    case kMips64I32x4UConvertF32x4: {
      CpuFeatureScope msa_scope(tasm(), MIPS_SIMD);
      __ ftrunc_u_w(i.OutputSimd128Register(), i.InputSimd128Register(0));
      break;
    }
    case kMips64I32x4Neg: {
      CpuFeatureScope msa_scope(tasm(), MIPS_SIMD);
      __ xor_v(kSimd128RegZero, kSimd128RegZero, kSimd128RegZero);
      __ subv_w(i.OutputSimd128Register(), kSimd128RegZero,
                i.InputSimd128Register(0));
      break;
    }
    case kMips64I32x4GtS: {
      CpuFeatureScope msa_scope(tasm(), MIPS_SIMD);
      __ clt_s_w(i.OutputSimd128Register(), i.InputSimd128Register(1),
                 i.InputSimd128Register(0));
      break;
    }
    case kMips64I32x4GeS: {
      CpuFeatureScope msa_scope(tasm(), MIPS_SIMD);
      __ cle_s_w(i.OutputSimd128Register(), i.InputSimd128Register(1),
                 i.InputSimd128Register(0));
      break;
    }
    case kMips64I32x4GtU: {
      CpuFeatureScope msa_scope(tasm(), MIPS_SIMD);
      __ clt_u_w(i.OutputSimd128Register(), i.InputSimd128Register(1),
                 i.InputSimd128Register(0));
      break;
    }
    case kMips64I32x4GeU: {
      CpuFeatureScope msa_scope(tasm(), MIPS_SIMD);
      __ cle_u_w(i.OutputSimd128Register(), i.InputSimd128Register(1),
                 i.InputSimd128Register(0));
      break;
    }
    case kMips64I16x8Splat: {
      CpuFeatureScope msa_scope(tasm(), MIPS_SIMD);
      __ fill_h(i.OutputSimd128Register(), i.InputRegister(0));
      break;
    }
    case kMips64I16x8ExtractLane: {
      CpuFeatureScope msa_scope(tasm(), MIPS_SIMD);
      __ copy_s_h(i.OutputRegister(), i.InputSimd128Register(0),
                  i.InputInt8(1));
      break;
    }
    case kMips64I16x8ReplaceLane: {
      CpuFeatureScope msa_scope(tasm(), MIPS_SIMD);
      Simd128Register src = i.InputSimd128Register(0);
      Simd128Register dst = i.OutputSimd128Register();
      if (src != dst) {
        __ move_v(dst, src);
      }
      __ insert_h(dst, i.InputInt8(1), i.InputRegister(2));
      break;
    }
    case kMips64I16x8Neg: {
      CpuFeatureScope msa_scope(tasm(), MIPS_SIMD);
      __ xor_v(kSimd128RegZero, kSimd128RegZero, kSimd128RegZero);
      __ subv_h(i.OutputSimd128Register(), kSimd128RegZero,
                i.InputSimd128Register(0));
      break;
    }
    case kMips64I16x8Shl: {
      CpuFeatureScope msa_scope(tasm(), MIPS_SIMD);
      __ slli_h(i.OutputSimd128Register(), i.InputSimd128Register(0),
                i.InputInt4(1));
      break;
    }
    case kMips64I16x8ShrS: {
      CpuFeatureScope msa_scope(tasm(), MIPS_SIMD);
      __ srai_h(i.OutputSimd128Register(), i.InputSimd128Register(0),
                i.InputInt4(1));
      break;
    }
    case kMips64I16x8ShrU: {
      CpuFeatureScope msa_scope(tasm(), MIPS_SIMD);
      __ srli_h(i.OutputSimd128Register(), i.InputSimd128Register(0),
                i.InputInt4(1));
      break;
    }
    case kMips64I16x8Add: {
      CpuFeatureScope msa_scope(tasm(), MIPS_SIMD);
      __ addv_h(i.OutputSimd128Register(), i.InputSimd128Register(0),
                i.InputSimd128Register(1));
      break;
    }
    case kMips64I16x8AddSaturateS: {
      CpuFeatureScope msa_scope(tasm(), MIPS_SIMD);
      __ adds_s_h(i.OutputSimd128Register(), i.InputSimd128Register(0),
                  i.InputSimd128Register(1));
      break;
    }
    case kMips64I16x8Sub: {
      CpuFeatureScope msa_scope(tasm(), MIPS_SIMD);
      __ subv_h(i.OutputSimd128Register(), i.InputSimd128Register(0),
                i.InputSimd128Register(1));
      break;
    }
    case kMips64I16x8SubSaturateS: {
      CpuFeatureScope msa_scope(tasm(), MIPS_SIMD);
      __ subs_s_h(i.OutputSimd128Register(), i.InputSimd128Register(0),
                  i.InputSimd128Register(1));
      break;
    }
    case kMips64I16x8Mul: {
      CpuFeatureScope msa_scope(tasm(), MIPS_SIMD);
      __ mulv_h(i.OutputSimd128Register(), i.InputSimd128Register(0),
                i.InputSimd128Register(1));
      break;
    }
    case kMips64I16x8MaxS: {
      CpuFeatureScope msa_scope(tasm(), MIPS_SIMD);
      __ max_s_h(i.OutputSimd128Register(), i.InputSimd128Register(0),
                 i.InputSimd128Register(1));
      break;
    }
    case kMips64I16x8MinS: {
      CpuFeatureScope msa_scope(tasm(), MIPS_SIMD);
      __ min_s_h(i.OutputSimd128Register(), i.InputSimd128Register(0),
                 i.InputSimd128Register(1));
      break;
    }
    case kMips64I16x8Eq: {
      CpuFeatureScope msa_scope(tasm(), MIPS_SIMD);
      __ ceq_h(i.OutputSimd128Register(), i.InputSimd128Register(0),
               i.InputSimd128Register(1));
      break;
    }
    case kMips64I16x8Ne: {
      CpuFeatureScope msa_scope(tasm(), MIPS_SIMD);
      Simd128Register dst = i.OutputSimd128Register();
      __ ceq_h(dst, i.InputSimd128Register(0), i.InputSimd128Register(1));
      __ nor_v(dst, dst, dst);
      break;
    }
    case kMips64I16x8GtS: {
      CpuFeatureScope msa_scope(tasm(), MIPS_SIMD);
      __ clt_s_h(i.OutputSimd128Register(), i.InputSimd128Register(1),
                 i.InputSimd128Register(0));
      break;
    }
    case kMips64I16x8GeS: {
      CpuFeatureScope msa_scope(tasm(), MIPS_SIMD);
      __ cle_s_h(i.OutputSimd128Register(), i.InputSimd128Register(1),
                 i.InputSimd128Register(0));
      break;
    }
    case kMips64I16x8AddSaturateU: {
      CpuFeatureScope msa_scope(tasm(), MIPS_SIMD);
      __ adds_u_h(i.OutputSimd128Register(), i.InputSimd128Register(0),
                  i.InputSimd128Register(1));
      break;
    }
    case kMips64I16x8SubSaturateU: {
      CpuFeatureScope msa_scope(tasm(), MIPS_SIMD);
      __ subs_u_h(i.OutputSimd128Register(), i.InputSimd128Register(0),
                  i.InputSimd128Register(1));
      break;
    }
    case kMips64I16x8MaxU: {
      CpuFeatureScope msa_scope(tasm(), MIPS_SIMD);
      __ max_u_h(i.OutputSimd128Register(), i.InputSimd128Register(0),
                 i.InputSimd128Register(1));
      break;
    }
    case kMips64I16x8MinU: {
      CpuFeatureScope msa_scope(tasm(), MIPS_SIMD);
      __ min_u_h(i.OutputSimd128Register(), i.InputSimd128Register(0),
                 i.InputSimd128Register(1));
      break;
    }
    case kMips64I16x8GtU: {
      CpuFeatureScope msa_scope(tasm(), MIPS_SIMD);
      __ clt_u_h(i.OutputSimd128Register(), i.InputSimd128Register(1),
                 i.InputSimd128Register(0));
      break;
    }
    case kMips64I16x8GeU: {
      CpuFeatureScope msa_scope(tasm(), MIPS_SIMD);
      __ cle_u_h(i.OutputSimd128Register(), i.InputSimd128Register(1),
                 i.InputSimd128Register(0));
      break;
    }
    case kMips64I8x16Splat: {
      CpuFeatureScope msa_scope(tasm(), MIPS_SIMD);
      __ fill_b(i.OutputSimd128Register(), i.InputRegister(0));
      break;
    }
    case kMips64I8x16ExtractLane: {
      CpuFeatureScope msa_scope(tasm(), MIPS_SIMD);
      __ copy_s_b(i.OutputRegister(), i.InputSimd128Register(0),
                  i.InputInt8(1));
      break;
    }
    case kMips64I8x16ReplaceLane: {
      CpuFeatureScope msa_scope(tasm(), MIPS_SIMD);
      Simd128Register src = i.InputSimd128Register(0);
      Simd128Register dst = i.OutputSimd128Register();
      if (src != dst) {
        __ move_v(dst, src);
      }
      __ insert_b(dst, i.InputInt8(1), i.InputRegister(2));
      break;
    }
    case kMips64I8x16Neg: {
      CpuFeatureScope msa_scope(tasm(), MIPS_SIMD);
      __ xor_v(kSimd128RegZero, kSimd128RegZero, kSimd128RegZero);
      __ subv_b(i.OutputSimd128Register(), kSimd128RegZero,
                i.InputSimd128Register(0));
      break;
    }
    case kMips64I8x16Shl: {
      CpuFeatureScope msa_scope(tasm(), MIPS_SIMD);
      __ slli_b(i.OutputSimd128Register(), i.InputSimd128Register(0),
                i.InputInt3(1));
      break;
    }
    case kMips64I8x16ShrS: {
      CpuFeatureScope msa_scope(tasm(), MIPS_SIMD);
      __ srai_b(i.OutputSimd128Register(), i.InputSimd128Register(0),
                i.InputInt3(1));
      break;
    }
    case kMips64I8x16Add: {
      CpuFeatureScope msa_scope(tasm(), MIPS_SIMD);
      __ addv_b(i.OutputSimd128Register(), i.InputSimd128Register(0),
                i.InputSimd128Register(1));
      break;
    }
    case kMips64I8x16AddSaturateS: {
      CpuFeatureScope msa_scope(tasm(), MIPS_SIMD);
      __ adds_s_b(i.OutputSimd128Register(), i.InputSimd128Register(0),
                  i.InputSimd128Register(1));
      break;
    }
    case kMips64I8x16Sub: {
      CpuFeatureScope msa_scope(tasm(), MIPS_SIMD);
      __ subv_b(i.OutputSimd128Register(), i.InputSimd128Register(0),
                i.InputSimd128Register(1));
      break;
    }
    case kMips64I8x16SubSaturateS: {
      CpuFeatureScope msa_scope(tasm(), MIPS_SIMD);
      __ subs_s_b(i.OutputSimd128Register(), i.InputSimd128Register(0),
                  i.InputSimd128Register(1));
      break;
    }
    case kMips64I8x16Mul: {
      CpuFeatureScope msa_scope(tasm(), MIPS_SIMD);
      __ mulv_b(i.OutputSimd128Register(), i.InputSimd128Register(0),
                i.InputSimd128Register(1));
      break;
    }
    case kMips64I8x16MaxS: {
      CpuFeatureScope msa_scope(tasm(), MIPS_SIMD);
      __ max_s_b(i.OutputSimd128Register(), i.InputSimd128Register(0),
                 i.InputSimd128Register(1));
      break;
    }
    case kMips64I8x16MinS: {
      CpuFeatureScope msa_scope(tasm(), MIPS_SIMD);
      __ min_s_b(i.OutputSimd128Register(), i.InputSimd128Register(0),
                 i.InputSimd128Register(1));
      break;
    }
    case kMips64I8x16Eq: {
      CpuFeatureScope msa_scope(tasm(), MIPS_SIMD);
      __ ceq_b(i.OutputSimd128Register(), i.InputSimd128Register(0),
               i.InputSimd128Register(1));
      break;
    }
    case kMips64I8x16Ne: {
      CpuFeatureScope msa_scope(tasm(), MIPS_SIMD);
      Simd128Register dst = i.OutputSimd128Register();
      __ ceq_b(dst, i.InputSimd128Register(0), i.InputSimd128Register(1));
      __ nor_v(dst, dst, dst);
      break;
    }
    case kMips64I8x16GtS: {
      CpuFeatureScope msa_scope(tasm(), MIPS_SIMD);
      __ clt_s_b(i.OutputSimd128Register(), i.InputSimd128Register(1),
                 i.InputSimd128Register(0));
      break;
    }
    case kMips64I8x16GeS: {
      CpuFeatureScope msa_scope(tasm(), MIPS_SIMD);
      __ cle_s_b(i.OutputSimd128Register(), i.InputSimd128Register(1),
                 i.InputSimd128Register(0));
      break;
    }
    case kMips64I8x16ShrU: {
      CpuFeatureScope msa_scope(tasm(), MIPS_SIMD);
      __ srli_b(i.OutputSimd128Register(), i.InputSimd128Register(0),
                i.InputInt3(1));
      break;
    }
    case kMips64I8x16AddSaturateU: {
      CpuFeatureScope msa_scope(tasm(), MIPS_SIMD);
      __ adds_u_b(i.OutputSimd128Register(), i.InputSimd128Register(0),
                  i.InputSimd128Register(1));
      break;
    }
    case kMips64I8x16SubSaturateU: {
      CpuFeatureScope msa_scope(tasm(), MIPS_SIMD);
      __ subs_u_b(i.OutputSimd128Register(), i.InputSimd128Register(0),
                  i.InputSimd128Register(1));
      break;
    }
    case kMips64I8x16MaxU: {
      CpuFeatureScope msa_scope(tasm(), MIPS_SIMD);
      __ max_u_b(i.OutputSimd128Register(), i.InputSimd128Register(0),
                 i.InputSimd128Register(1));
      break;
    }
    case kMips64I8x16MinU: {
      CpuFeatureScope msa_scope(tasm(), MIPS_SIMD);
      __ min_u_b(i.OutputSimd128Register(), i.InputSimd128Register(0),
                 i.InputSimd128Register(1));
      break;
    }
    case kMips64I8x16GtU: {
      CpuFeatureScope msa_scope(tasm(), MIPS_SIMD);
      __ clt_u_b(i.OutputSimd128Register(), i.InputSimd128Register(1),
                 i.InputSimd128Register(0));
      break;
    }
    case kMips64I8x16GeU: {
      CpuFeatureScope msa_scope(tasm(), MIPS_SIMD);
      __ cle_u_b(i.OutputSimd128Register(), i.InputSimd128Register(1),
                 i.InputSimd128Register(0));
      break;
    }
    case kMips64S128And: {
      CpuFeatureScope msa_scope(tasm(), MIPS_SIMD);
      __ and_v(i.OutputSimd128Register(), i.InputSimd128Register(0),
               i.InputSimd128Register(1));
      break;
    }
    case kMips64S128Or: {
      CpuFeatureScope msa_scope(tasm(), MIPS_SIMD);
      __ or_v(i.OutputSimd128Register(), i.InputSimd128Register(0),
              i.InputSimd128Register(1));
      break;
    }
    case kMips64S128Xor: {
      CpuFeatureScope msa_scope(tasm(), MIPS_SIMD);
      __ xor_v(i.OutputSimd128Register(), i.InputSimd128Register(0),
               i.InputSimd128Register(1));
      break;
    }
    case kMips64S128Not: {
      CpuFeatureScope msa_scope(tasm(), MIPS_SIMD);
      __ nor_v(i.OutputSimd128Register(), i.InputSimd128Register(0),
               i.InputSimd128Register(0));
      break;
    }
    case kMips64S1x4AnyTrue:
    case kMips64S1x8AnyTrue:
    case kMips64S1x16AnyTrue: {
      CpuFeatureScope msa_scope(tasm(), MIPS_SIMD);
      Register dst = i.OutputRegister();
      Label all_false;
      __ BranchMSA(&all_false, MSA_BRANCH_V, all_zero,
                   i.InputSimd128Register(0), USE_DELAY_SLOT);
      __ li(dst, 0);  // branch delay slot
      __ li(dst, -1);
      __ bind(&all_false);
      break;
    }
    case kMips64S1x4AllTrue: {
      CpuFeatureScope msa_scope(tasm(), MIPS_SIMD);
      Register dst = i.OutputRegister();
      Label all_true;
      __ BranchMSA(&all_true, MSA_BRANCH_W, all_not_zero,
                   i.InputSimd128Register(0), USE_DELAY_SLOT);
      __ li(dst, -1);  // branch delay slot
      __ li(dst, 0);
      __ bind(&all_true);
      break;
    }
    case kMips64S1x8AllTrue: {
      CpuFeatureScope msa_scope(tasm(), MIPS_SIMD);
      Register dst = i.OutputRegister();
      Label all_true;
      __ BranchMSA(&all_true, MSA_BRANCH_H, all_not_zero,
                   i.InputSimd128Register(0), USE_DELAY_SLOT);
      __ li(dst, -1);  // branch delay slot
      __ li(dst, 0);
      __ bind(&all_true);
      break;
    }
    case kMips64S1x16AllTrue: {
      CpuFeatureScope msa_scope(tasm(), MIPS_SIMD);
      Register dst = i.OutputRegister();
      Label all_true;
      __ BranchMSA(&all_true, MSA_BRANCH_B, all_not_zero,
                   i.InputSimd128Register(0), USE_DELAY_SLOT);
      __ li(dst, -1);  // branch delay slot
      __ li(dst, 0);
      __ bind(&all_true);
      break;
    }
    case kMips64MsaLd: {
      CpuFeatureScope msa_scope(tasm(), MIPS_SIMD);
      __ ld_b(i.OutputSimd128Register(), i.MemoryOperand());
      break;
    }
    case kMips64MsaSt: {
      CpuFeatureScope msa_scope(tasm(), MIPS_SIMD);
      __ st_b(i.InputSimd128Register(2), i.MemoryOperand());
      break;
    }
    case kMips64S32x4InterleaveRight: {
      CpuFeatureScope msa_scope(tasm(), MIPS_SIMD);
      Simd128Register dst = i.OutputSimd128Register(),
                      src0 = i.InputSimd128Register(0),
                      src1 = i.InputSimd128Register(1);
      // src1 = [7, 6, 5, 4], src0 = [3, 2, 1, 0]
      // dst = [5, 1, 4, 0]
      __ ilvr_w(dst, src1, src0);
      break;
    }
    case kMips64S32x4InterleaveLeft: {
      CpuFeatureScope msa_scope(tasm(), MIPS_SIMD);
      Simd128Register dst = i.OutputSimd128Register(),
                      src0 = i.InputSimd128Register(0),
                      src1 = i.InputSimd128Register(1);
      // src1 = [7, 6, 5, 4], src0 = [3, 2, 1, 0]
      // dst = [7, 3, 6, 2]
      __ ilvl_w(dst, src1, src0);
      break;
    }
    case kMips64S32x4PackEven: {
      CpuFeatureScope msa_scope(tasm(), MIPS_SIMD);
      Simd128Register dst = i.OutputSimd128Register(),
                      src0 = i.InputSimd128Register(0),
                      src1 = i.InputSimd128Register(1);
      // src1 = [7, 6, 5, 4], src0 = [3, 2, 1, 0]
      // dst = [6, 4, 2, 0]
      __ pckev_w(dst, src1, src0);
      break;
    }
    case kMips64S32x4PackOdd: {
      CpuFeatureScope msa_scope(tasm(), MIPS_SIMD);
      Simd128Register dst = i.OutputSimd128Register(),
                      src0 = i.InputSimd128Register(0),
                      src1 = i.InputSimd128Register(1);
      // src1 = [7, 6, 5, 4], src0 = [3, 2, 1, 0]
      // dst = [7, 5, 3, 1]
      __ pckod_w(dst, src1, src0);
      break;
    }
    case kMips64S32x4InterleaveEven: {
      CpuFeatureScope msa_scope(tasm(), MIPS_SIMD);
      Simd128Register dst = i.OutputSimd128Register(),
                      src0 = i.InputSimd128Register(0),
                      src1 = i.InputSimd128Register(1);
      // src1 = [7, 6, 5, 4], src0 = [3, 2, 1, 0]
      // dst = [6, 2, 4, 0]
      __ ilvev_w(dst, src1, src0);
      break;
    }
    case kMips64S32x4InterleaveOdd: {
      CpuFeatureScope msa_scope(tasm(), MIPS_SIMD);
      Simd128Register dst = i.OutputSimd128Register(),
                      src0 = i.InputSimd128Register(0),
                      src1 = i.InputSimd128Register(1);
      // src1 = [7, 6, 5, 4], src0 = [3, 2, 1, 0]
      // dst = [7, 3, 5, 1]
      __ ilvod_w(dst, src1, src0);
      break;
    }
    case kMips64S32x4Shuffle: {
      CpuFeatureScope msa_scope(tasm(), MIPS_SIMD);
      Simd128Register dst = i.OutputSimd128Register(),
                      src0 = i.InputSimd128Register(0),
                      src1 = i.InputSimd128Register(1);

      int32_t shuffle = i.InputInt32(2);

      if (src0 == src1) {
        // Unary S32x4 shuffles are handled with shf.w instruction
        unsigned lane = shuffle & 0xFF;
        if (FLAG_debug_code) {
          // range of all four lanes, for unary instruction,
          // should belong to the same range, which can be one of these:
          // [0, 3] or [4, 7]
          if (lane >= 4) {
            int32_t shuffle_helper = shuffle;
            for (int i = 0; i < 4; ++i) {
              lane = shuffle_helper & 0xFF;
              CHECK_GE(lane, 4);
              shuffle_helper >>= 8;
            }
          }
        }
        uint32_t i8 = 0;
        for (int i = 0; i < 4; i++) {
          lane = shuffle & 0xFF;
          if (lane >= 4) {
            lane -= 4;
          }
          DCHECK_GT(4, lane);
          i8 |= lane << (2 * i);
          shuffle >>= 8;
        }
        __ shf_w(dst, src0, i8);
      } else {
        // For binary shuffles use vshf.w instruction
        if (dst == src0) {
          __ move_v(kSimd128ScratchReg, src0);
          src0 = kSimd128ScratchReg;
        } else if (dst == src1) {
          __ move_v(kSimd128ScratchReg, src1);
          src1 = kSimd128ScratchReg;
        }

        __ li(kScratchReg, i.InputInt32(2));
        __ insert_w(dst, 0, kScratchReg);
        __ xor_v(kSimd128RegZero, kSimd128RegZero, kSimd128RegZero);
        __ ilvr_b(dst, kSimd128RegZero, dst);
        __ ilvr_h(dst, kSimd128RegZero, dst);
        __ vshf_w(dst, src1, src0);
      }
      break;
    }
    case kMips64S16x8InterleaveRight: {
      CpuFeatureScope msa_scope(tasm(), MIPS_SIMD);
      Simd128Register dst = i.OutputSimd128Register(),
                      src0 = i.InputSimd128Register(0),
                      src1 = i.InputSimd128Register(1);
      // src1 = [15, ... 11, 10, 9, 8], src0 = [7, ... 3, 2, 1, 0]
      // dst = [11, 3, 10, 2, 9, 1, 8, 0]
      __ ilvr_h(dst, src1, src0);
      break;
    }
    case kMips64S16x8InterleaveLeft: {
      CpuFeatureScope msa_scope(tasm(), MIPS_SIMD);
      Simd128Register dst = i.OutputSimd128Register(),
                      src0 = i.InputSimd128Register(0),
                      src1 = i.InputSimd128Register(1);
      // src1 = [15, ... 11, 10, 9, 8], src0 = [7, ... 3, 2, 1, 0]
      // dst = [15, 7, 14, 6, 13, 5, 12, 4]
      __ ilvl_h(dst, src1, src0);
      break;
    }
    case kMips64S16x8PackEven: {
      CpuFeatureScope msa_scope(tasm(), MIPS_SIMD);
      Simd128Register dst = i.OutputSimd128Register(),
                      src0 = i.InputSimd128Register(0),
                      src1 = i.InputSimd128Register(1);
      // src1 = [15, ... 11, 10, 9, 8], src0 = [7, ... 3, 2, 1, 0]
      // dst = [14, 12, 10, 8, 6, 4, 2, 0]
      __ pckev_h(dst, src1, src0);
      break;
    }
    case kMips64S16x8PackOdd: {
      CpuFeatureScope msa_scope(tasm(), MIPS_SIMD);
      Simd128Register dst = i.OutputSimd128Register(),
                      src0 = i.InputSimd128Register(0),
                      src1 = i.InputSimd128Register(1);
      // src1 = [15, ... 11, 10, 9, 8], src0 = [7, ... 3, 2, 1, 0]
      // dst = [15, 13, 11, 9, 7, 5, 3, 1]
      __ pckod_h(dst, src1, src0);
      break;
    }
    case kMips64S16x8InterleaveEven: {
      CpuFeatureScope msa_scope(tasm(), MIPS_SIMD);
      Simd128Register dst = i.OutputSimd128Register(),
                      src0 = i.InputSimd128Register(0),
                      src1 = i.InputSimd128Register(1);
      // src1 = [15, ... 11, 10, 9, 8], src0 = [7, ... 3, 2, 1, 0]
      // dst = [14, 6, 12, 4, 10, 2, 8, 0]
      __ ilvev_h(dst, src1, src0);
      break;
    }
    case kMips64S16x8InterleaveOdd: {
      CpuFeatureScope msa_scope(tasm(), MIPS_SIMD);
      Simd128Register dst = i.OutputSimd128Register(),
                      src0 = i.InputSimd128Register(0),
                      src1 = i.InputSimd128Register(1);
      // src1 = [15, ... 11, 10, 9, 8], src0 = [7, ... 3, 2, 1, 0]
      // dst = [15, 7, ... 11, 3, 9, 1]
      __ ilvod_h(dst, src1, src0);
      break;
    }
    case kMips64S16x4Reverse: {
      CpuFeatureScope msa_scope(tasm(), MIPS_SIMD);
      // src = [7, 6, 5, 4, 3, 2, 1, 0], dst = [4, 5, 6, 7, 0, 1, 2, 3]
      // shf.df imm field: 0 1 2 3 = 00011011 = 0x1B
      __ shf_h(i.OutputSimd128Register(), i.InputSimd128Register(0), 0x1B);
      break;
    }
    case kMips64S16x2Reverse: {
      CpuFeatureScope msa_scope(tasm(), MIPS_SIMD);
      // src = [7, 6, 5, 4, 3, 2, 1, 0], dst = [6, 7, 4, 5, 3, 2, 0, 1]
      // shf.df imm field: 2 3 0 1 = 10110001 = 0xB1
      __ shf_h(i.OutputSimd128Register(), i.InputSimd128Register(0), 0xB1);
      break;
    }
    case kMips64S8x16InterleaveRight: {
      CpuFeatureScope msa_scope(tasm(), MIPS_SIMD);
      Simd128Register dst = i.OutputSimd128Register(),
                      src0 = i.InputSimd128Register(0),
                      src1 = i.InputSimd128Register(1);
      // src1 = [31, ... 19, 18, 17, 16], src0 = [15, ... 3, 2, 1, 0]
      // dst = [23, 7, ... 17, 1, 16, 0]
      __ ilvr_b(dst, src1, src0);
      break;
    }
    case kMips64S8x16InterleaveLeft: {
      CpuFeatureScope msa_scope(tasm(), MIPS_SIMD);
      Simd128Register dst = i.OutputSimd128Register(),
                      src0 = i.InputSimd128Register(0),
                      src1 = i.InputSimd128Register(1);
      // src1 = [31, ... 19, 18, 17, 16], src0 = [15, ... 3, 2, 1, 0]
      // dst = [31, 15, ... 25, 9, 24, 8]
      __ ilvl_b(dst, src1, src0);
      break;
    }
    case kMips64S8x16PackEven: {
      CpuFeatureScope msa_scope(tasm(), MIPS_SIMD);
      Simd128Register dst = i.OutputSimd128Register(),
                      src0 = i.InputSimd128Register(0),
                      src1 = i.InputSimd128Register(1);
      // src1 = [31, ... 19, 18, 17, 16], src0 = [15, ... 3, 2, 1, 0]
      // dst = [30, 28, ... 6, 4, 2, 0]
      __ pckev_b(dst, src1, src0);
      break;
    }
    case kMips64S8x16PackOdd: {
      CpuFeatureScope msa_scope(tasm(), MIPS_SIMD);
      Simd128Register dst = i.OutputSimd128Register(),
                      src0 = i.InputSimd128Register(0),
                      src1 = i.InputSimd128Register(1);
      // src1 = [31, ... 19, 18, 17, 16], src0 = [15, ... 3, 2, 1, 0]
      // dst = [31, 29, ... 7, 5, 3, 1]
      __ pckod_b(dst, src1, src0);
      break;
    }
    case kMips64S8x16InterleaveEven: {
      CpuFeatureScope msa_scope(tasm(), MIPS_SIMD);
      Simd128Register dst = i.OutputSimd128Register(),
                      src0 = i.InputSimd128Register(0),
                      src1 = i.InputSimd128Register(1);
      // src1 = [31, ... 19, 18, 17, 16], src0 = [15, ... 3, 2, 1, 0]
      // dst = [30, 14, ... 18, 2, 16, 0]
      __ ilvev_b(dst, src1, src0);
      break;
    }
    case kMips64S8x16InterleaveOdd: {
      CpuFeatureScope msa_scope(tasm(), MIPS_SIMD);
      Simd128Register dst = i.OutputSimd128Register(),
                      src0 = i.InputSimd128Register(0),
                      src1 = i.InputSimd128Register(1);
      // src1 = [31, ... 19, 18, 17, 16], src0 = [15, ... 3, 2, 1, 0]
      // dst = [31, 15, ... 19, 3, 17, 1]
      __ ilvod_b(dst, src1, src0);
      break;
    }
    case kMips64S8x16Concat: {
      CpuFeatureScope msa_scope(tasm(), MIPS_SIMD);
      Simd128Register dst = i.OutputSimd128Register();
      DCHECK(dst == i.InputSimd128Register(0));
      __ sldi_b(dst, i.InputSimd128Register(1), i.InputInt4(2));
      break;
    }
    case kMips64S8x16Shuffle: {
      CpuFeatureScope msa_scope(tasm(), MIPS_SIMD);
      Simd128Register dst = i.OutputSimd128Register(),
                      src0 = i.InputSimd128Register(0),
                      src1 = i.InputSimd128Register(1);

      if (dst == src0) {
        __ move_v(kSimd128ScratchReg, src0);
        src0 = kSimd128ScratchReg;
      } else if (dst == src1) {
        __ move_v(kSimd128ScratchReg, src1);
        src1 = kSimd128ScratchReg;
      }

      int64_t control_low =
          static_cast<int64_t>(i.InputInt32(3)) << 32 | i.InputInt32(2);
      int64_t control_hi =
          static_cast<int64_t>(i.InputInt32(5)) << 32 | i.InputInt32(4);
      __ li(kScratchReg, control_low);
      __ insert_d(dst, 0, kScratchReg);
      __ li(kScratchReg, control_hi);
      __ insert_d(dst, 1, kScratchReg);
      __ vshf_b(dst, src1, src0);
      break;
    }
    case kMips64S8x8Reverse: {
      CpuFeatureScope msa_scope(tasm(), MIPS_SIMD);
      // src = [15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0]
      // dst = [8, 9, 10, 11, 12, 13, 14, 15, 0, 1, 2, 3, 4, 5, 6, 7]
      // [A B C D] => [B A D C]: shf.w imm: 2 3 0 1 = 10110001 = 0xB1
      // C: [7, 6, 5, 4] => A': [4, 5, 6, 7]: shf.b imm: 00011011 = 0x1B
      __ shf_w(kSimd128ScratchReg, i.InputSimd128Register(0), 0xB1);
      __ shf_b(i.OutputSimd128Register(), kSimd128ScratchReg, 0x1B);
      break;
    }
    case kMips64S8x4Reverse: {
      CpuFeatureScope msa_scope(tasm(), MIPS_SIMD);
      // src = [15, 14, ... 3, 2, 1, 0], dst = [12, 13, 14, 15, ... 0, 1, 2, 3]
      // shf.df imm field: 0 1 2 3 = 00011011 = 0x1B
      __ shf_b(i.OutputSimd128Register(), i.InputSimd128Register(0), 0x1B);
      break;
    }
    case kMips64S8x2Reverse: {
      CpuFeatureScope msa_scope(tasm(), MIPS_SIMD);
      // src = [15, 14, ... 3, 2, 1, 0], dst = [14, 15, 12, 13, ... 2, 3, 0, 1]
      // shf.df imm field: 2 3 0 1 = 10110001 = 0xB1
      __ shf_b(i.OutputSimd128Register(), i.InputSimd128Register(0), 0xB1);
      break;
    }
    case kMips64I32x4SConvertI16x8Low: {
      CpuFeatureScope msa_scope(tasm(), MIPS_SIMD);
      Simd128Register dst = i.OutputSimd128Register();
      Simd128Register src = i.InputSimd128Register(0);
      __ ilvr_h(kSimd128ScratchReg, src, src);
      __ slli_w(dst, kSimd128ScratchReg, 16);
      __ srai_w(dst, dst, 16);
      break;
    }
    case kMips64I32x4SConvertI16x8High: {
      CpuFeatureScope msa_scope(tasm(), MIPS_SIMD);
      Simd128Register dst = i.OutputSimd128Register();
      Simd128Register src = i.InputSimd128Register(0);
      __ ilvl_h(kSimd128ScratchReg, src, src);
      __ slli_w(dst, kSimd128ScratchReg, 16);
      __ srai_w(dst, dst, 16);
      break;
    }
    case kMips64I32x4UConvertI16x8Low: {
      CpuFeatureScope msa_scope(tasm(), MIPS_SIMD);
      __ xor_v(kSimd128RegZero, kSimd128RegZero, kSimd128RegZero);
      __ ilvr_h(i.OutputSimd128Register(), kSimd128RegZero,
                i.InputSimd128Register(0));
      break;
    }
    case kMips64I32x4UConvertI16x8High: {
      CpuFeatureScope msa_scope(tasm(), MIPS_SIMD);
      __ xor_v(kSimd128RegZero, kSimd128RegZero, kSimd128RegZero);
      __ ilvl_h(i.OutputSimd128Register(), kSimd128RegZero,
                i.InputSimd128Register(0));
      break;
    }
    case kMips64I16x8SConvertI8x16Low: {
      CpuFeatureScope msa_scope(tasm(), MIPS_SIMD);
      Simd128Register dst = i.OutputSimd128Register();
      Simd128Register src = i.InputSimd128Register(0);
      __ ilvr_b(kSimd128ScratchReg, src, src);
      __ slli_h(dst, kSimd128ScratchReg, 8);
      __ srai_h(dst, dst, 8);
      break;
    }
    case kMips64I16x8SConvertI8x16High: {
      CpuFeatureScope msa_scope(tasm(), MIPS_SIMD);
      Simd128Register dst = i.OutputSimd128Register();
      Simd128Register src = i.InputSimd128Register(0);
      __ ilvl_b(kSimd128ScratchReg, src, src);
      __ slli_h(dst, kSimd128ScratchReg, 8);
      __ srai_h(dst, dst, 8);
      break;
    }
    case kMips64I16x8SConvertI32x4: {
      CpuFeatureScope msa_scope(tasm(), MIPS_SIMD);
      Simd128Register dst = i.OutputSimd128Register();
      Simd128Register src0 = i.InputSimd128Register(0);
      Simd128Register src1 = i.InputSimd128Register(1);
      __ sat_s_w(kSimd128ScratchReg, src0, 15);
      __ sat_s_w(kSimd128RegZero, src1, 15);  // kSimd128RegZero as scratch
      __ pckev_h(dst, kSimd128RegZero, kSimd128ScratchReg);
      break;
    }
    case kMips64I16x8UConvertI32x4: {
      CpuFeatureScope msa_scope(tasm(), MIPS_SIMD);
      Simd128Register dst = i.OutputSimd128Register();
      Simd128Register src0 = i.InputSimd128Register(0);
      Simd128Register src1 = i.InputSimd128Register(1);
      __ sat_u_w(kSimd128ScratchReg, src0, 15);
      __ sat_u_w(kSimd128RegZero, src1, 15);  // kSimd128RegZero as scratch
      __ pckev_h(dst, kSimd128RegZero, kSimd128ScratchReg);
      break;
    }
    case kMips64I16x8UConvertI8x16Low: {
      CpuFeatureScope msa_scope(tasm(), MIPS_SIMD);
      __ xor_v(kSimd128RegZero, kSimd128RegZero, kSimd128RegZero);
      __ ilvr_b(i.OutputSimd128Register(), kSimd128RegZero,
                i.InputSimd128Register(0));
      break;
    }
    case kMips64I16x8UConvertI8x16High: {
      CpuFeatureScope msa_scope(tasm(), MIPS_SIMD);
      __ xor_v(kSimd128RegZero, kSimd128RegZero, kSimd128RegZero);
      __ ilvl_b(i.OutputSimd128Register(), kSimd128RegZero,
                i.InputSimd128Register(0));
      break;
    }
    case kMips64I8x16SConvertI16x8: {
      CpuFeatureScope msa_scope(tasm(), MIPS_SIMD);
      Simd128Register dst = i.OutputSimd128Register();
      Simd128Register src0 = i.InputSimd128Register(0);
      Simd128Register src1 = i.InputSimd128Register(1);
      __ sat_s_h(kSimd128ScratchReg, src0, 7);
      __ sat_s_h(kSimd128RegZero, src1, 7);  // kSimd128RegZero as scratch
      __ pckev_b(dst, kSimd128RegZero, kSimd128ScratchReg);
      break;
    }
    case kMips64I8x16UConvertI16x8: {
      CpuFeatureScope msa_scope(tasm(), MIPS_SIMD);
      Simd128Register dst = i.OutputSimd128Register();
      Simd128Register src0 = i.InputSimd128Register(0);
      Simd128Register src1 = i.InputSimd128Register(1);
      __ sat_u_h(kSimd128ScratchReg, src0, 7);
      __ sat_u_h(kSimd128RegZero, src1, 7);  // kSimd128RegZero as scratch
      __ pckev_b(dst, kSimd128RegZero, kSimd128ScratchReg);
      break;
    }
    case kMips64F32x4AddHoriz: {
      CpuFeatureScope msa_scope(tasm(), MIPS_SIMD);
      Simd128Register src0 = i.InputSimd128Register(0);
      Simd128Register src1 = i.InputSimd128Register(1);
      Simd128Register dst = i.OutputSimd128Register();
      __ shf_w(kSimd128ScratchReg, src0, 0xB1);  // 2 3 0 1 : 10110001 : 0xB1
      __ shf_w(kSimd128RegZero, src1, 0xB1);     // kSimd128RegZero as scratch
      __ fadd_w(kSimd128ScratchReg, kSimd128ScratchReg, src0);
      __ fadd_w(kSimd128RegZero, kSimd128RegZero, src1);
      __ pckev_w(dst, kSimd128RegZero, kSimd128ScratchReg);
      break;
    }
    case kMips64I32x4AddHoriz: {
      CpuFeatureScope msa_scope(tasm(), MIPS_SIMD);
      Simd128Register src0 = i.InputSimd128Register(0);
      Simd128Register src1 = i.InputSimd128Register(1);
      Simd128Register dst = i.OutputSimd128Register();
      __ hadd_s_d(kSimd128ScratchReg, src0, src0);
      __ hadd_s_d(kSimd128RegZero, src1, src1);  // kSimd128RegZero as scratch
      __ pckev_w(dst, kSimd128RegZero, kSimd128ScratchReg);
      break;
    }
    case kMips64I16x8AddHoriz: {
      CpuFeatureScope msa_scope(tasm(), MIPS_SIMD);
      Simd128Register src0 = i.InputSimd128Register(0);
      Simd128Register src1 = i.InputSimd128Register(1);
      Simd128Register dst = i.OutputSimd128Register();
      __ hadd_s_w(kSimd128ScratchReg, src0, src0);
      __ hadd_s_w(kSimd128RegZero, src1, src1);  // kSimd128RegZero as scratch
      __ pckev_h(dst, kSimd128RegZero, kSimd128ScratchReg);
      break;
    }
  }
  return kSuccess;
}  // NOLINT(readability/fn_size)


#define UNSUPPORTED_COND(opcode, condition)                                  \
  OFStream out(stdout);                                                      \
  out << "Unsupported " << #opcode << " condition: \"" << condition << "\""; \
  UNIMPLEMENTED();

static bool convertCondition(FlagsCondition condition, Condition& cc) {
  switch (condition) {
    case kEqual:
      cc = eq;
      return true;
    case kNotEqual:
      cc = ne;
      return true;
    case kUnsignedLessThan:
      cc = lt;
      return true;
    case kUnsignedGreaterThanOrEqual:
      cc = uge;
      return true;
    case kUnsignedLessThanOrEqual:
      cc = le;
      return true;
    case kUnsignedGreaterThan:
      cc = ugt;
      return true;
    default:
      break;
  }
  return false;
}

void AssembleBranchToLabels(CodeGenerator* gen, TurboAssembler* tasm,
                            Instruction* instr, FlagsCondition condition,
                            Label* tlabel, Label* flabel, bool fallthru) {
#undef __
#define __ tasm->
  MipsOperandConverter i(gen, instr);

  Condition cc = kNoCondition;
  // MIPS does not have condition code flags, so compare and branch are
  // implemented differently than on the other arch's. The compare operations
  // emit mips pseudo-instructions, which are handled here by branch
  // instructions that do the actual comparison. Essential that the input
  // registers to compare pseudo-op are not modified before this branch op, as
  // they are tested here.

  if (instr->arch_opcode() == kMips64Tst) {
    cc = FlagsConditionToConditionTst(condition);
    __ And(at, i.InputRegister(0), i.InputOperand(1));
    __ Branch(tlabel, cc, at, Operand(zero_reg));
  } else if (instr->arch_opcode() == kMips64Dadd ||
             instr->arch_opcode() == kMips64Dsub) {
    cc = FlagsConditionToConditionOvf(condition);
    __ dsra32(kScratchReg, i.OutputRegister(), 0);
    __ sra(at, i.OutputRegister(), 31);
    __ Branch(tlabel, cc, at, Operand(kScratchReg));
  } else if (instr->arch_opcode() == kMips64DaddOvf) {
    switch (condition) {
      case kOverflow:
        __ DaddBranchOvf(i.OutputRegister(), i.InputRegister(0),
                         i.InputOperand(1), tlabel, flabel);
        break;
      case kNotOverflow:
        __ DaddBranchOvf(i.OutputRegister(), i.InputRegister(0),
                         i.InputOperand(1), flabel, tlabel);
        break;
      default:
        UNSUPPORTED_COND(kMips64DaddOvf, condition);
        break;
    }
  } else if (instr->arch_opcode() == kMips64DsubOvf) {
    switch (condition) {
      case kOverflow:
        __ DsubBranchOvf(i.OutputRegister(), i.InputRegister(0),
                         i.InputOperand(1), tlabel, flabel);
        break;
      case kNotOverflow:
        __ DsubBranchOvf(i.OutputRegister(), i.InputRegister(0),
                         i.InputOperand(1), flabel, tlabel);
        break;
      default:
        UNSUPPORTED_COND(kMips64DsubOvf, condition);
        break;
    }
  } else if (instr->arch_opcode() == kMips64MulOvf) {
    switch (condition) {
      case kOverflow: {
        __ MulBranchOvf(i.OutputRegister(), i.InputRegister(0),
                        i.InputOperand(1), tlabel, flabel, kScratchReg);
      } break;
      case kNotOverflow: {
        __ MulBranchOvf(i.OutputRegister(), i.InputRegister(0),
                        i.InputOperand(1), flabel, tlabel, kScratchReg);
      } break;
      default:
        UNSUPPORTED_COND(kMips64MulOvf, condition);
        break;
    }
  } else if (instr->arch_opcode() == kMips64Cmp) {
    cc = FlagsConditionToConditionCmp(condition);
    __ Branch(tlabel, cc, i.InputRegister(0), i.InputOperand(1));
  } else if (instr->arch_opcode() == kMips64CmpS) {
    if (!convertCondition(condition, cc)) {
      UNSUPPORTED_COND(kMips64CmpS, condition);
    }
    FPURegister left = i.InputOrZeroSingleRegister(0);
    FPURegister right = i.InputOrZeroSingleRegister(1);
    if ((left == kDoubleRegZero || right == kDoubleRegZero) &&
        !__ IsDoubleZeroRegSet()) {
      __ Move(kDoubleRegZero, 0.0);
    }
    __ BranchF32(tlabel, nullptr, cc, left, right);
  } else if (instr->arch_opcode() == kMips64CmpD) {
    if (!convertCondition(condition, cc)) {
      UNSUPPORTED_COND(kMips64CmpD, condition);
    }
    FPURegister left = i.InputOrZeroDoubleRegister(0);
    FPURegister right = i.InputOrZeroDoubleRegister(1);
    if ((left == kDoubleRegZero || right == kDoubleRegZero) &&
        !__ IsDoubleZeroRegSet()) {
      __ Move(kDoubleRegZero, 0.0);
    }
    __ BranchF64(tlabel, nullptr, cc, left, right);
  } else {
    PrintF("AssembleArchBranch Unimplemented arch_opcode: %d\n",
           instr->arch_opcode());
    UNIMPLEMENTED();
  }
  if (!fallthru) __ Branch(flabel);  // no fallthru to flabel.
#undef __
#define __ tasm()->
}

// Assembles branches after an instruction.
void CodeGenerator::AssembleArchBranch(Instruction* instr, BranchInfo* branch) {
  Label* tlabel = branch->true_label;
  Label* flabel = branch->false_label;

  AssembleBranchToLabels(this, tasm(), instr, branch->condition, tlabel, flabel,
                         branch->fallthru);
}

void CodeGenerator::AssembleArchDeoptBranch(Instruction* instr,
                                            BranchInfo* branch) {
  AssembleArchBranch(instr, branch);
}

void CodeGenerator::AssembleArchJump(RpoNumber target) {
  if (!IsNextInAssemblyOrder(target)) __ Branch(GetLabel(target));
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
      MipsOperandConverter i(gen_, instr_);
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
        // We use the context register as the scratch register, because we do
        // not have a context here.
        __ PrepareCallCFunction(0, 0, cp);
        __ CallCFunction(ExternalReference::wasm_call_trap_callback_for_testing(
                             tasm()->isolate()),
                         0);
        __ LeaveFrame(StackFrame::WASM_COMPILED);
        CallDescriptor* descriptor = gen_->linkage()->GetIncomingDescriptor();
        int pop_count = static_cast<int>(descriptor->StackParameterCount());
        pop_count += (pop_count & 1);  // align
        __ Drop(pop_count);
        __ Ret();
      } else {
        gen_->AssembleSourcePosition(instr_);
        __ Call(tasm()->isolate()->builtins()->builtin_handle(trap_id),
                RelocInfo::CODE_TARGET);
        ReferenceMap* reference_map =
            new (gen_->zone()) ReferenceMap(gen_->zone());
        gen_->RecordSafepoint(reference_map, Safepoint::kSimple, 0,
                              Safepoint::kNoLazyDeopt);
        if (FLAG_debug_code) {
          __ stop(GetAbortReason(AbortReason::kUnexpectedReturnFromWasmTrap));
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
  AssembleBranchToLabels(this, tasm(), instr, condition, tlabel, nullptr, true);
}

// Assembles boolean materializations after an instruction.
void CodeGenerator::AssembleArchBoolean(Instruction* instr,
                                        FlagsCondition condition) {
  MipsOperandConverter i(this, instr);
  Label done;

  // Materialize a full 32-bit 1 or 0 value. The result register is always the
  // last output of the instruction.
  Label false_value;
  DCHECK_NE(0u, instr->OutputCount());
  Register result = i.OutputRegister(instr->OutputCount() - 1);
  Condition cc = kNoCondition;
  // MIPS does not have condition code flags, so compare and branch are
  // implemented differently than on the other arch's. The compare operations
  // emit mips pseudo-instructions, which are checked and handled here.

  if (instr->arch_opcode() == kMips64Tst) {
    cc = FlagsConditionToConditionTst(condition);
    if (instr->InputAt(1)->IsImmediate() &&
        base::bits::IsPowerOfTwo(i.InputOperand(1).immediate())) {
      uint16_t pos =
          base::bits::CountTrailingZeros64(i.InputOperand(1).immediate());
      __ Dext(result, i.InputRegister(0), pos, 1);
      if (cc == eq) {
        __ xori(result, result, 1);
      }
    } else {
      __ And(kScratchReg, i.InputRegister(0), i.InputOperand(1));
      if (cc == eq) {
        __ Sltu(result, kScratchReg, 1);
      } else {
        __ Sltu(result, zero_reg, kScratchReg);
      }
    }
    return;
  } else if (instr->arch_opcode() == kMips64Dadd ||
             instr->arch_opcode() == kMips64Dsub) {
    cc = FlagsConditionToConditionOvf(condition);
    // Check for overflow creates 1 or 0 for result.
    __ dsrl32(kScratchReg, i.OutputRegister(), 31);
    __ srl(at, i.OutputRegister(), 31);
    __ xor_(result, kScratchReg, at);
    if (cc == eq)  // Toggle result for not overflow.
      __ xori(result, result, 1);
    return;
  } else if (instr->arch_opcode() == kMips64DaddOvf ||
             instr->arch_opcode() == kMips64DsubOvf ||
             instr->arch_opcode() == kMips64MulOvf) {
    Label flabel, tlabel;
    switch (instr->arch_opcode()) {
      case kMips64DaddOvf:
        __ DaddBranchNoOvf(i.OutputRegister(), i.InputRegister(0),
                           i.InputOperand(1), &flabel);

        break;
      case kMips64DsubOvf:
        __ DsubBranchNoOvf(i.OutputRegister(), i.InputRegister(0),
                           i.InputOperand(1), &flabel);
        break;
      case kMips64MulOvf:
        __ MulBranchNoOvf(i.OutputRegister(), i.InputRegister(0),
                          i.InputOperand(1), &flabel, kScratchReg);
        break;
      default:
        UNREACHABLE();
        break;
    }
    __ li(result, 1);
    __ Branch(&tlabel);
    __ bind(&flabel);
    __ li(result, 0);
    __ bind(&tlabel);
  } else if (instr->arch_opcode() == kMips64Cmp) {
    cc = FlagsConditionToConditionCmp(condition);
    switch (cc) {
      case eq:
      case ne: {
        Register left = i.InputRegister(0);
        Operand right = i.InputOperand(1);
        if (instr->InputAt(1)->IsImmediate()) {
          if (is_int16(-right.immediate())) {
            if (right.immediate() == 0) {
              if (cc == eq) {
                __ Sltu(result, left, 1);
              } else {
                __ Sltu(result, zero_reg, left);
              }
            } else {
              __ Daddu(result, left, Operand(-right.immediate()));
              if (cc == eq) {
                __ Sltu(result, result, 1);
              } else {
                __ Sltu(result, zero_reg, result);
              }
            }
          } else {
            if (is_uint16(right.immediate())) {
              __ Xor(result, left, right);
            } else {
              __ li(kScratchReg, right);
              __ Xor(result, left, kScratchReg);
            }
            if (cc == eq) {
              __ Sltu(result, result, 1);
            } else {
              __ Sltu(result, zero_reg, result);
            }
          }
        } else {
          __ Xor(result, left, right);
          if (cc == eq) {
            __ Sltu(result, result, 1);
          } else {
            __ Sltu(result, zero_reg, result);
          }
        }
      } break;
      case lt:
      case ge: {
        Register left = i.InputRegister(0);
        Operand right = i.InputOperand(1);
        __ Slt(result, left, right);
        if (cc == ge) {
          __ xori(result, result, 1);
        }
      } break;
      case gt:
      case le: {
        Register left = i.InputRegister(1);
        Operand right = i.InputOperand(0);
        __ Slt(result, left, right);
        if (cc == le) {
          __ xori(result, result, 1);
        }
      } break;
      case lo:
      case hs: {
        Register left = i.InputRegister(0);
        Operand right = i.InputOperand(1);
        __ Sltu(result, left, right);
        if (cc == hs) {
          __ xori(result, result, 1);
        }
      } break;
      case hi:
      case ls: {
        Register left = i.InputRegister(1);
        Operand right = i.InputOperand(0);
        __ Sltu(result, left, right);
        if (cc == ls) {
          __ xori(result, result, 1);
        }
      } break;
      default:
        UNREACHABLE();
    }
    return;
  } else if (instr->arch_opcode() == kMips64CmpD ||
             instr->arch_opcode() == kMips64CmpS) {
    FPURegister left = i.InputOrZeroDoubleRegister(0);
    FPURegister right = i.InputOrZeroDoubleRegister(1);
    if ((left == kDoubleRegZero || right == kDoubleRegZero) &&
        !__ IsDoubleZeroRegSet()) {
      __ Move(kDoubleRegZero, 0.0);
    }
    bool predicate;
    FPUCondition cc = FlagsConditionToConditionCmpFPU(predicate, condition);
    if (kArchVariant != kMips64r6) {
      __ li(result, Operand(1));
      if (instr->arch_opcode() == kMips64CmpD) {
        __ c(cc, D, left, right);
      } else {
        DCHECK_EQ(kMips64CmpS, instr->arch_opcode());
        __ c(cc, S, left, right);
      }
      if (predicate) {
        __ Movf(result, zero_reg);
      } else {
        __ Movt(result, zero_reg);
      }
    } else {
      if (instr->arch_opcode() == kMips64CmpD) {
        __ cmp(cc, L, kDoubleCompareReg, left, right);
        __ dmfc1(result, kDoubleCompareReg);
      } else {
        DCHECK_EQ(kMips64CmpS, instr->arch_opcode());
        __ cmp(cc, W, kDoubleCompareReg, left, right);
        __ mfc1(result, kDoubleCompareReg);
      }
      if (predicate) {
        __ And(result, result, 1);  // cmp returns all 1's/0's, use only LSB.
      } else {
        __ Addu(result, result, 1);  // Toggle result for not equal.
      }
    }
    return;
  } else {
    PrintF("AssembleArchBranch Unimplemented arch_opcode is : %d\n",
           instr->arch_opcode());
    TRACE_UNIMPL();
    UNIMPLEMENTED();
  }
}


void CodeGenerator::AssembleArchLookupSwitch(Instruction* instr) {
  MipsOperandConverter i(this, instr);
  Register input = i.InputRegister(0);
  for (size_t index = 2; index < instr->InputCount(); index += 2) {
    __ li(at, Operand(i.InputInt32(index + 0)));
    __ beq(input, at, GetLabel(i.InputRpo(index + 1)));
  }
  __ nop();  // Branch delay slot of the last beq.
  AssembleArchJump(i.InputRpo(1));
}

void CodeGenerator::AssembleArchTableSwitch(Instruction* instr) {
  MipsOperandConverter i(this, instr);
  Register input = i.InputRegister(0);
  size_t const case_count = instr->InputCount() - 2;

  __ Branch(GetLabel(i.InputRpo(1)), hs, input, Operand(case_count));
  __ GenerateSwitchTable(input, case_count, [&i, this](size_t index) {
    return GetLabel(i.InputRpo(index + 2));
  });
}

void CodeGenerator::FinishFrame(Frame* frame) {
  CallDescriptor* descriptor = linkage()->GetIncomingDescriptor();

  const RegList saves_fpu = descriptor->CalleeSavedFPRegisters();
  if (saves_fpu != 0) {
    int count = base::bits::CountPopulation(saves_fpu);
    DCHECK_EQ(kNumCalleeSavedFPU, count);
    frame->AllocateSavedCalleeRegisterSlots(count *
                                            (kDoubleSize / kPointerSize));
  }

  const RegList saves = descriptor->CalleeSavedRegisters();
  if (saves != 0) {
    int count = base::bits::CountPopulation(saves);
    DCHECK_EQ(kNumCalleeSaved, count + 1);
    frame->AllocateSavedCalleeRegisterSlots(count);
  }
}

void CodeGenerator::AssembleConstructFrame() {
  CallDescriptor* descriptor = linkage()->GetIncomingDescriptor();

  if (frame_access_state()->has_frame()) {
    if (descriptor->IsCFunctionCall()) {
      __ Push(ra, fp);
      __ mov(fp, sp);
    } else if (descriptor->IsJSFunctionCall()) {
      __ Prologue();
      if (descriptor->PushArgumentCount()) {
        __ Push(kJavaScriptCallArgCountRegister);
      }
    } else {
      __ StubPrologue(info()->GetOutputStackFrameType());
    }
  }

  int shrink_slots =
      frame()->GetTotalFrameSlotCount() - descriptor->CalculateFixedFrameSize();

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
  }

  const RegList saves = descriptor->CalleeSavedRegisters();
  const RegList saves_fpu = descriptor->CalleeSavedFPRegisters();
  const int returns = frame()->GetReturnSlotCount();

  // Skip callee-saved and return slots, which are pushed below.
  shrink_slots -= base::bits::CountPopulation(saves);
  shrink_slots -= base::bits::CountPopulation(saves_fpu);
  shrink_slots -= returns;
  if (shrink_slots > 0) {
    __ Dsubu(sp, sp, Operand(shrink_slots * kPointerSize));
  }

  if (saves_fpu != 0) {
    // Save callee-saved FPU registers.
    __ MultiPushFPU(saves_fpu);
    DCHECK_EQ(kNumCalleeSavedFPU, base::bits::CountPopulation(saves_fpu));
  }

  if (saves != 0) {
    // Save callee-saved registers.
    __ MultiPush(saves);
    DCHECK_EQ(kNumCalleeSaved, base::bits::CountPopulation(saves) + 1);
  }

  if (returns != 0) {
    // Create space for returns.
    __ Dsubu(sp, sp, Operand(returns * kPointerSize));
  }
}

void CodeGenerator::AssembleReturn(InstructionOperand* pop) {
  CallDescriptor* descriptor = linkage()->GetIncomingDescriptor();

  const int returns = frame()->GetReturnSlotCount();
  if (returns != 0) {
    __ Daddu(sp, sp, Operand(returns * kPointerSize));
  }

  // Restore GP registers.
  const RegList saves = descriptor->CalleeSavedRegisters();
  if (saves != 0) {
    __ MultiPop(saves);
  }

  // Restore FPU registers.
  const RegList saves_fpu = descriptor->CalleeSavedFPRegisters();
  if (saves_fpu != 0) {
    __ MultiPopFPU(saves_fpu);
  }

  MipsOperandConverter g(this, nullptr);
  if (descriptor->IsCFunctionCall()) {
    AssembleDeconstructFrame();
  } else if (frame_access_state()->has_frame()) {
    // Canonicalize JSFunction return sites for now unless they have an variable
    // number of stack slot pops.
    if (pop->IsImmediate() && g.ToConstant(pop).ToInt32() == 0) {
      if (return_label_.is_bound()) {
        __ Branch(&return_label_);
        return;
      } else {
        __ bind(&return_label_);
        AssembleDeconstructFrame();
      }
    } else {
      AssembleDeconstructFrame();
    }
  }
  int pop_count = static_cast<int>(descriptor->StackParameterCount());
  if (pop->IsImmediate()) {
    pop_count += g.ToConstant(pop).ToInt32();
  } else {
    Register pop_reg = g.ToRegister(pop);
    __ dsll(pop_reg, pop_reg, kPointerSizeLog2);
    __ Daddu(sp, sp, pop_reg);
  }
  if (pop_count != 0) {
    __ DropAndRet(pop_count);
  } else {
    __ Ret();
  }
}

void CodeGenerator::FinishCode() {}

void CodeGenerator::AssembleMove(InstructionOperand* source,
                                 InstructionOperand* destination) {
  MipsOperandConverter g(this, nullptr);
  // Dispatch on the source and destination operand kinds.  Not all
  // combinations are possible.
  if (source->IsRegister()) {
    DCHECK(destination->IsRegister() || destination->IsStackSlot());
    Register src = g.ToRegister(source);
    if (destination->IsRegister()) {
      __ mov(g.ToRegister(destination), src);
    } else {
      __ Sd(src, g.ToMemOperand(destination));
    }
  } else if (source->IsStackSlot()) {
    DCHECK(destination->IsRegister() || destination->IsStackSlot());
    MemOperand src = g.ToMemOperand(source);
    if (destination->IsRegister()) {
      __ Ld(g.ToRegister(destination), src);
    } else {
      Register temp = kScratchReg;
      __ Ld(temp, src);
      __ Sd(temp, g.ToMemOperand(destination));
    }
  } else if (source->IsConstant()) {
    Constant src = g.ToConstant(source);
    if (destination->IsRegister() || destination->IsStackSlot()) {
      Register dst =
          destination->IsRegister() ? g.ToRegister(destination) : kScratchReg;
      switch (src.type()) {
        case Constant::kInt32:
          if (RelocInfo::IsWasmSizeReference(src.rmode())) {
            __ li(dst, Operand(src.ToInt32(), src.rmode()));
          } else {
            __ li(dst, Operand(src.ToInt32()));
          }
          break;
        case Constant::kFloat32:
          __ li(dst, Operand::EmbeddedNumber(src.ToFloat32()));
          break;
        case Constant::kInt64:
          if (RelocInfo::IsWasmPtrReference(src.rmode())) {
            __ li(dst, Operand(src.ToInt64(), src.rmode()));
          } else {
            DCHECK(!RelocInfo::IsWasmSizeReference(src.rmode()));
            __ li(dst, Operand(src.ToInt64()));
          }
          break;
        case Constant::kFloat64:
          __ li(dst, Operand::EmbeddedNumber(src.ToFloat64().value()));
          break;
        case Constant::kExternalReference:
          __ li(dst, Operand(src.ToExternalReference()));
          break;
        case Constant::kHeapObject: {
          Handle<HeapObject> src_object = src.ToHeapObject();
          Heap::RootListIndex index;
          if (IsMaterializableFromRoot(src_object, &index)) {
            __ LoadRoot(dst, index);
          } else {
            __ li(dst, src_object);
          }
          break;
        }
        case Constant::kRpoNumber:
          UNREACHABLE();  // TODO(titzer): loading RPO numbers on mips64.
          break;
      }
      if (destination->IsStackSlot()) __ Sd(dst, g.ToMemOperand(destination));
    } else if (src.type() == Constant::kFloat32) {
      if (destination->IsFPStackSlot()) {
        MemOperand dst = g.ToMemOperand(destination);
        if (bit_cast<int32_t>(src.ToFloat32()) == 0) {
          __ Sw(zero_reg, dst);
        } else {
          __ li(at, Operand(bit_cast<int32_t>(src.ToFloat32())));
          __ Sw(at, dst);
        }
      } else {
        DCHECK(destination->IsFPRegister());
        FloatRegister dst = g.ToSingleRegister(destination);
        __ Move(dst, src.ToFloat32());
      }
    } else {
      DCHECK_EQ(Constant::kFloat64, src.type());
      DoubleRegister dst = destination->IsFPRegister()
                               ? g.ToDoubleRegister(destination)
                               : kScratchDoubleReg;
      __ Move(dst, src.ToFloat64().value());
      if (destination->IsFPStackSlot()) {
        __ Sdc1(dst, g.ToMemOperand(destination));
      }
    }
  } else if (source->IsFPRegister()) {
    FPURegister src = g.ToDoubleRegister(source);
    if (destination->IsFPRegister()) {
      FPURegister dst = g.ToDoubleRegister(destination);
      __ Move(dst, src);
    } else {
      DCHECK(destination->IsFPStackSlot());
      __ Sdc1(src, g.ToMemOperand(destination));
    }
  } else if (source->IsFPStackSlot()) {
    DCHECK(destination->IsFPRegister() || destination->IsFPStackSlot());
    MemOperand src = g.ToMemOperand(source);
    if (destination->IsFPRegister()) {
      __ Ldc1(g.ToDoubleRegister(destination), src);
    } else {
      FPURegister temp = kScratchDoubleReg;
      __ Ldc1(temp, src);
      __ Sdc1(temp, g.ToMemOperand(destination));
    }
  } else {
    UNREACHABLE();
  }
}


void CodeGenerator::AssembleSwap(InstructionOperand* source,
                                 InstructionOperand* destination) {
  MipsOperandConverter g(this, nullptr);
  // Dispatch on the source and destination operand kinds.  Not all
  // combinations are possible.
  if (source->IsRegister()) {
    // Register-register.
    Register temp = kScratchReg;
    Register src = g.ToRegister(source);
    if (destination->IsRegister()) {
      Register dst = g.ToRegister(destination);
      __ Move(temp, src);
      __ Move(src, dst);
      __ Move(dst, temp);
    } else {
      DCHECK(destination->IsStackSlot());
      MemOperand dst = g.ToMemOperand(destination);
      __ mov(temp, src);
      __ Ld(src, dst);
      __ Sd(temp, dst);
    }
  } else if (source->IsStackSlot()) {
    DCHECK(destination->IsStackSlot());
    Register temp_0 = kScratchReg;
    Register temp_1 = kScratchReg2;
    MemOperand src = g.ToMemOperand(source);
    MemOperand dst = g.ToMemOperand(destination);
    __ Ld(temp_0, src);
    __ Ld(temp_1, dst);
    __ Sd(temp_0, dst);
    __ Sd(temp_1, src);
  } else if (source->IsFPRegister()) {
    FPURegister temp = kScratchDoubleReg;
    FPURegister src = g.ToDoubleRegister(source);
    if (destination->IsFPRegister()) {
      FPURegister dst = g.ToDoubleRegister(destination);
      __ Move(temp, src);
      __ Move(src, dst);
      __ Move(dst, temp);
    } else {
      DCHECK(destination->IsFPStackSlot());
      MemOperand dst = g.ToMemOperand(destination);
      __ Move(temp, src);
      __ Ldc1(src, dst);
      __ Sdc1(temp, dst);
    }
  } else if (source->IsFPStackSlot()) {
    DCHECK(destination->IsFPStackSlot());
    Register temp_0 = kScratchReg;
    FPURegister temp_1 = kScratchDoubleReg;
    MemOperand src0 = g.ToMemOperand(source);
    MemOperand src1(src0.rm(), src0.offset() + kIntSize);
    MemOperand dst0 = g.ToMemOperand(destination);
    MemOperand dst1(dst0.rm(), dst0.offset() + kIntSize);
    __ Ldc1(temp_1, dst0);  // Save destination in temp_1.
    __ Lw(temp_0, src0);    // Then use temp_0 to copy source to destination.
    __ Sw(temp_0, dst0);
    __ Lw(temp_0, src1);
    __ Sw(temp_0, dst1);
    __ Sdc1(temp_1, src0);
  } else {
    // No other combinations are possible.
    UNREACHABLE();
  }
}


void CodeGenerator::AssembleJumpTable(Label** targets, size_t target_count) {
  // On 64-bit MIPS we emit the jump tables inline.
  UNREACHABLE();
}


#undef __

}  // namespace compiler
}  // namespace internal
}  // namespace v8
