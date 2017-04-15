// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/code-generator.h"
#include "src/compilation-info.h"
#include "src/compiler/code-generator-impl.h"
#include "src/compiler/gap-resolver.h"
#include "src/compiler/node-matchers.h"
#include "src/compiler/osr.h"
#include "src/mips/macro-assembler-mips.h"

namespace v8 {
namespace internal {
namespace compiler {

#define __ masm()->


// TODO(plind): Possibly avoid using these lithium names.
#define kScratchReg kLithiumScratchReg
#define kCompareReg kLithiumScratchReg2
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
      DCHECK((InputInt32(index) == 0));
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
      case Constant::kFloat32:
        return Operand(
            isolate()->factory()->NewNumber(constant.ToFloat32(), TENURED));
      case Constant::kFloat64:
        return Operand(
            isolate()->factory()->NewNumber(constant.ToFloat64(), TENURED));
      case Constant::kInt64:
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
    return Operand(zero_reg);
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
    return MemOperand(no_reg);
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

class OutOfLineLoadSingle final : public OutOfLineCode {
 public:
  OutOfLineLoadSingle(CodeGenerator* gen, FloatRegister result)
      : OutOfLineCode(gen), result_(result) {}

  void Generate() final {
    __ Move(result_, std::numeric_limits<float>::quiet_NaN());
  }

 private:
  FloatRegister const result_;
};


class OutOfLineLoadDouble final : public OutOfLineCode {
 public:
  OutOfLineLoadDouble(CodeGenerator* gen, DoubleRegister result)
      : OutOfLineCode(gen), result_(result) {}

  void Generate() final {
    __ Move(result_, std::numeric_limits<double>::quiet_NaN());
  }

 private:
  DoubleRegister const result_;
};


class OutOfLineLoadInteger final : public OutOfLineCode {
 public:
  OutOfLineLoadInteger(CodeGenerator* gen, Register result)
      : OutOfLineCode(gen), result_(result) {}

  void Generate() final { __ mov(result_, zero_reg); }

 private:
  Register const result_;
};


class OutOfLineRound : public OutOfLineCode {
 public:
  OutOfLineRound(CodeGenerator* gen, DoubleRegister result)
      : OutOfLineCode(gen), result_(result) {}

  void Generate() final {
    // Handle rounding to zero case where sign has to be preserved.
    // High bits of double input already in kScratchReg.
    __ srl(at, kScratchReg, 31);
    __ sll(at, at, 31);
    __ Mthc1(at, result_);
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
        must_save_lr_(!gen->frame_access_state()->has_frame()) {}

  void Generate() final {
    if (mode_ > RecordWriteMode::kValueIsPointer) {
      __ JumpIfSmi(value_, exit());
    }
    __ CheckPageFlag(value_, scratch0_,
                     MemoryChunk::kPointersToHereAreInterestingMask, eq,
                     exit());
    RememberedSetAction const remembered_set_action =
        mode_ > RecordWriteMode::kValueIsMap ? EMIT_REMEMBERED_SET
                                             : OMIT_REMEMBERED_SET;
    SaveFPRegsMode const save_fp_mode =
        frame()->DidAllocateDoubleRegisters() ? kSaveFPRegs : kDontSaveFPRegs;
    if (must_save_lr_) {
      // We need to save and restore ra if the frame was elided.
      __ Push(ra);
    }
    RecordWriteStub stub(isolate(), object_, scratch0_, scratch1_,
                         remembered_set_action, save_fp_mode);
    __ Addu(scratch1_, object_, index_);
    __ CallStub(&stub);
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
};

#define CREATE_OOL_CLASS(ool_name, masm_ool_name, T)                 \
  class ool_name final : public OutOfLineCode {                      \
   public:                                                           \
    ool_name(CodeGenerator* gen, T dst, T src1, T src2)              \
        : OutOfLineCode(gen), dst_(dst), src1_(src1), src2_(src2) {} \
                                                                     \
    void Generate() final { __ masm_ool_name(dst_, src1_, src2_); }  \
                                                                     \
   private:                                                          \
    T const dst_;                                                    \
    T const src1_;                                                   \
    T const src2_;                                                   \
  }

CREATE_OOL_CLASS(OutOfLineFloat32Max, Float32MaxOutOfLine, FPURegister);
CREATE_OOL_CLASS(OutOfLineFloat32Min, Float32MinOutOfLine, FPURegister);
CREATE_OOL_CLASS(OutOfLineFloat64Max, Float64MaxOutOfLine, DoubleRegister);
CREATE_OOL_CLASS(OutOfLineFloat64Min, Float64MinOutOfLine, DoubleRegister);

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
  return kNoCondition;
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
  return kNoCondition;
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
  return kNoFPUCondition;
}

}  // namespace


#define ASSEMBLE_CHECKED_LOAD_FLOAT(width, asm_instr)                         \
  do {                                                                        \
    auto result = i.Output##width##Register();                                \
    auto ool = new (zone()) OutOfLineLoad##width(this, result);               \
    if (instr->InputAt(0)->IsRegister()) {                                    \
      auto offset = i.InputRegister(0);                                       \
      __ Branch(USE_DELAY_SLOT, ool->entry(), hs, offset, i.InputOperand(1)); \
      __ addu(kScratchReg, i.InputRegister(2), offset);                       \
      __ asm_instr(result, MemOperand(kScratchReg, 0));                       \
    } else {                                                                  \
      auto offset = i.InputOperand(0).immediate();                            \
      __ Branch(ool->entry(), ls, i.InputRegister(1), Operand(offset));       \
      __ asm_instr(result, MemOperand(i.InputRegister(2), offset));           \
    }                                                                         \
    __ bind(ool->exit());                                                     \
  } while (0)


#define ASSEMBLE_CHECKED_LOAD_INTEGER(asm_instr)                              \
  do {                                                                        \
    auto result = i.OutputRegister();                                         \
    auto ool = new (zone()) OutOfLineLoadInteger(this, result);               \
    if (instr->InputAt(0)->IsRegister()) {                                    \
      auto offset = i.InputRegister(0);                                       \
      __ Branch(USE_DELAY_SLOT, ool->entry(), hs, offset, i.InputOperand(1)); \
      __ addu(kScratchReg, i.InputRegister(2), offset);                       \
      __ asm_instr(result, MemOperand(kScratchReg, 0));                       \
    } else {                                                                  \
      auto offset = i.InputOperand(0).immediate();                            \
      __ Branch(ool->entry(), ls, i.InputRegister(1), Operand(offset));       \
      __ asm_instr(result, MemOperand(i.InputRegister(2), offset));           \
    }                                                                         \
    __ bind(ool->exit());                                                     \
  } while (0)

#define ASSEMBLE_CHECKED_STORE_FLOAT(width, asm_instr)                 \
  do {                                                                 \
    Label done;                                                        \
    if (instr->InputAt(0)->IsRegister()) {                             \
      auto offset = i.InputRegister(0);                                \
      auto value = i.InputOrZero##width##Register(2);                  \
      if (value.is(kDoubleRegZero) && !__ IsDoubleZeroRegSet()) {      \
        __ Move(kDoubleRegZero, 0.0);                                  \
      }                                                                \
      __ Branch(USE_DELAY_SLOT, &done, hs, offset, i.InputOperand(1)); \
      __ addu(kScratchReg, i.InputRegister(3), offset);                \
      __ asm_instr(value, MemOperand(kScratchReg, 0));                 \
    } else {                                                           \
      auto offset = i.InputOperand(0).immediate();                     \
      auto value = i.InputOrZero##width##Register(2);                  \
      if (value.is(kDoubleRegZero) && !__ IsDoubleZeroRegSet()) {      \
        __ Move(kDoubleRegZero, 0.0);                                  \
      }                                                                \
      __ Branch(&done, ls, i.InputRegister(1), Operand(offset));       \
      __ asm_instr(value, MemOperand(i.InputRegister(3), offset));     \
    }                                                                  \
    __ bind(&done);                                                    \
  } while (0)

#define ASSEMBLE_CHECKED_STORE_INTEGER(asm_instr)                      \
  do {                                                                 \
    Label done;                                                        \
    if (instr->InputAt(0)->IsRegister()) {                             \
      auto offset = i.InputRegister(0);                                \
      auto value = i.InputOrZeroRegister(2);                           \
      __ Branch(USE_DELAY_SLOT, &done, hs, offset, i.InputOperand(1)); \
      __ addu(kScratchReg, i.InputRegister(3), offset);                \
      __ asm_instr(value, MemOperand(kScratchReg, 0));                 \
    } else {                                                           \
      auto offset = i.InputOperand(0).immediate();                     \
      auto value = i.InputOrZeroRegister(2);                           \
      __ Branch(&done, ls, i.InputRegister(1), Operand(offset));       \
      __ asm_instr(value, MemOperand(i.InputRegister(3), offset));     \
    }                                                                  \
    __ bind(&done);                                                    \
  } while (0)

#define ASSEMBLE_ROUND_DOUBLE_TO_DOUBLE(mode)                                  \
  if (IsMipsArchVariant(kMips32r6)) {                                          \
    __ cfc1(kScratchReg, FCSR);                                                \
    __ li(at, Operand(mode_##mode));                                           \
    __ ctc1(at, FCSR);                                                         \
    __ rint_d(i.OutputDoubleRegister(), i.InputDoubleRegister(0));             \
    __ ctc1(kScratchReg, FCSR);                                                \
  } else {                                                                     \
    auto ool = new (zone()) OutOfLineRound(this, i.OutputDoubleRegister());    \
    Label done;                                                                \
    __ Mfhc1(kScratchReg, i.InputDoubleRegister(0));                           \
    __ Ext(at, kScratchReg, HeapNumber::kExponentShift,                        \
           HeapNumber::kExponentBits);                                         \
    __ Branch(USE_DELAY_SLOT, &done, hs, at,                                   \
              Operand(HeapNumber::kExponentBias + HeapNumber::kMantissaBits)); \
    __ mov_d(i.OutputDoubleRegister(), i.InputDoubleRegister(0));              \
    __ mode##_l_d(i.OutputDoubleRegister(), i.InputDoubleRegister(0));         \
    __ Move(at, kScratchReg2, i.OutputDoubleRegister());                       \
    __ or_(at, at, kScratchReg2);                                              \
    __ Branch(USE_DELAY_SLOT, ool->entry(), eq, at, Operand(zero_reg));        \
    __ cvt_d_l(i.OutputDoubleRegister(), i.OutputDoubleRegister());            \
    __ bind(ool->exit());                                                      \
    __ bind(&done);                                                            \
  }


#define ASSEMBLE_ROUND_FLOAT_TO_FLOAT(mode)                                   \
  if (IsMipsArchVariant(kMips32r6)) {                                         \
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

#define ASSEMBLE_IEEE754_BINOP(name)                                          \
  do {                                                                        \
    FrameScope scope(masm(), StackFrame::MANUAL);                             \
    __ PrepareCallCFunction(0, 2, kScratchReg);                               \
    __ MovToFloatParameters(i.InputDoubleRegister(0),                         \
                            i.InputDoubleRegister(1));                        \
    __ CallCFunction(ExternalReference::ieee754_##name##_function(isolate()), \
                     0, 2);                                                   \
    /* Move the result in the double result register. */                      \
    __ MovFromFloatResult(i.OutputDoubleRegister());                          \
  } while (0)

#define ASSEMBLE_IEEE754_UNOP(name)                                           \
  do {                                                                        \
    FrameScope scope(masm(), StackFrame::MANUAL);                             \
    __ PrepareCallCFunction(0, 1, kScratchReg);                               \
    __ MovToFloatParameter(i.InputDoubleRegister(0));                         \
    __ CallCFunction(ExternalReference::ieee754_##name##_function(isolate()), \
                     0, 1);                                                   \
    /* Move the result in the double result register. */                      \
    __ MovFromFloatResult(i.OutputDoubleRegister());                          \
  } while (0)

void CodeGenerator::AssembleDeconstructFrame() {
  __ mov(sp, fp);
  __ Pop(ra, fp);
}

void CodeGenerator::AssemblePrepareTailCall() {
  if (frame_access_state()->has_frame()) {
    __ lw(ra, MemOperand(fp, StandardFrameConstants::kCallerPCOffset));
    __ lw(fp, MemOperand(fp, StandardFrameConstants::kCallerFPOffset));
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
  __ lw(scratch1, MemOperand(fp, StandardFrameConstants::kContextOffset));
  __ Branch(&done, ne, scratch1,
            Operand(Smi::FromInt(StackFrame::ARGUMENTS_ADAPTOR)));

  // Load arguments count from current arguments adaptor frame (note, it
  // does not include receiver).
  Register caller_args_count_reg = scratch1;
  __ lw(caller_args_count_reg,
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
    masm->Subu(sp, sp, stack_slot_delta * kPointerSize);
    state->IncreaseSPDelta(stack_slot_delta);
  } else if (allow_shrinkage && stack_slot_delta < 0) {
    masm->Addu(sp, sp, -stack_slot_delta * kPointerSize);
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
  MipsOperandConverter i(this, instr);
  InstructionCode opcode = instr->opcode();
  ArchOpcode arch_opcode = ArchOpcodeField::decode(opcode);
  switch (arch_opcode) {
    case kArchCallCodeObject: {
      EnsureSpaceForLazyDeopt();
      if (instr->InputAt(0)->IsImmediate()) {
        __ Call(Handle<Code>::cast(i.InputHeapObject(0)),
                RelocInfo::CODE_TARGET);
      } else {
        __ addiu(at, i.InputRegister(0), Code::kHeaderSize - kHeapObjectTag);
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
        __ Jump(Handle<Code>::cast(i.InputHeapObject(0)),
                RelocInfo::CODE_TARGET);
      } else {
        __ addiu(at, i.InputRegister(0), Code::kHeaderSize - kHeapObjectTag);
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
      EnsureSpaceForLazyDeopt();
      Register func = i.InputRegister(0);
      if (FLAG_debug_code) {
        // Check the function's context matches the context argument.
        __ lw(kScratchReg, FieldMemOperand(func, JSFunction::kContextOffset));
        __ Assert(eq, kWrongFunctionContext, cp, Operand(kScratchReg));
      }

      __ lw(at, FieldMemOperand(func, JSFunction::kCodeEntryOffset));
      __ Call(at);
      RecordCallPosition(instr);
      frame_access_state()->ClearSPDelta();
      frame_access_state()->SetFrameAccessToDefault();
      break;
    }
    case kArchTailCallJSFunctionFromJSFunction: {
      Register func = i.InputRegister(0);
      if (FLAG_debug_code) {
        // Check the function's context matches the context argument.
        __ lw(kScratchReg, FieldMemOperand(func, JSFunction::kContextOffset));
        __ Assert(eq, kWrongFunctionContext, cp, Operand(kScratchReg));
      }
      AssemblePopArgumentsAdaptorFrame(kJavaScriptCallArgCountRegister,
                                       i.TempRegister(0), i.TempRegister(1),
                                       i.TempRegister(2));
      __ lw(at, FieldMemOperand(func, JSFunction::kCodeEntryOffset));
      __ Jump(at);
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
      __ mov(i.OutputRegister(), sp);
      break;
    case kArchFramePointer:
      __ mov(i.OutputRegister(), fp);
      break;
    case kArchParentFramePointer:
      if (frame_access_state()->has_frame()) {
        __ lw(i.OutputRegister(), MemOperand(fp, 0));
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
      Register object = i.InputRegister(0);
      Register index = i.InputRegister(1);
      Register value = i.InputRegister(2);
      Register scratch0 = i.TempRegister(0);
      Register scratch1 = i.TempRegister(1);
      auto ool = new (zone()) OutOfLineRecordWrite(this, object, index, value,
                                                   scratch0, scratch1, mode);
      __ Addu(at, object, index);
      __ sw(value, MemOperand(at));
      __ CheckPageFlag(object, scratch0,
                       MemoryChunk::kPointersFromHereAreInterestingMask, ne,
                       ool->entry());
      __ bind(ool->exit());
      break;
    }
    case kArchStackSlot: {
      FrameOffset offset =
          frame_access_state()->GetFrameOffset(i.InputInt32(0));
      __ Addu(i.OutputRegister(), offset.from_stack_pointer() ? sp : fp,
              Operand(offset.offset()));
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
    case kIeee754Float64Log10:
      ASSEMBLE_IEEE754_UNOP(log10);
      break;
    case kIeee754Float64Log2:
      ASSEMBLE_IEEE754_UNOP(log2);
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
    case kMipsAdd:
      __ Addu(i.OutputRegister(), i.InputRegister(0), i.InputOperand(1));
      break;
    case kMipsAddOvf:
      // Pseudo-instruction used for overflow/branch. No opcode emitted here.
      break;
    case kMipsSub:
      __ Subu(i.OutputRegister(), i.InputRegister(0), i.InputOperand(1));
      break;
    case kMipsSubOvf:
      // Pseudo-instruction used for overflow/branch. No opcode emitted here.
      break;
    case kMipsMul:
      __ Mul(i.OutputRegister(), i.InputRegister(0), i.InputOperand(1));
      break;
    case kMipsMulOvf:
      // Pseudo-instruction used for overflow/branch. No opcode emitted here.
      break;
    case kMipsMulHigh:
      __ Mulh(i.OutputRegister(), i.InputRegister(0), i.InputOperand(1));
      break;
    case kMipsMulHighU:
      __ Mulhu(i.OutputRegister(), i.InputRegister(0), i.InputOperand(1));
      break;
    case kMipsDiv:
      __ Div(i.OutputRegister(), i.InputRegister(0), i.InputOperand(1));
      if (IsMipsArchVariant(kMips32r6)) {
        __ selnez(i.OutputRegister(), i.InputRegister(0), i.InputRegister(1));
      } else {
        __ Movz(i.OutputRegister(), i.InputRegister(1), i.InputRegister(1));
      }
      break;
    case kMipsDivU:
      __ Divu(i.OutputRegister(), i.InputRegister(0), i.InputOperand(1));
      if (IsMipsArchVariant(kMips32r6)) {
        __ selnez(i.OutputRegister(), i.InputRegister(0), i.InputRegister(1));
      } else {
        __ Movz(i.OutputRegister(), i.InputRegister(1), i.InputRegister(1));
      }
      break;
    case kMipsMod:
      __ Mod(i.OutputRegister(), i.InputRegister(0), i.InputOperand(1));
      break;
    case kMipsModU:
      __ Modu(i.OutputRegister(), i.InputRegister(0), i.InputOperand(1));
      break;
    case kMipsAnd:
      __ And(i.OutputRegister(), i.InputRegister(0), i.InputOperand(1));
      break;
    case kMipsOr:
      __ Or(i.OutputRegister(), i.InputRegister(0), i.InputOperand(1));
      break;
    case kMipsNor:
      if (instr->InputAt(1)->IsRegister()) {
        __ Nor(i.OutputRegister(), i.InputRegister(0), i.InputOperand(1));
      } else {
        DCHECK(i.InputOperand(1).immediate() == 0);
        __ Nor(i.OutputRegister(), i.InputRegister(0), zero_reg);
      }
      break;
    case kMipsXor:
      __ Xor(i.OutputRegister(), i.InputRegister(0), i.InputOperand(1));
      break;
    case kMipsClz:
      __ Clz(i.OutputRegister(), i.InputRegister(0));
      break;
    case kMipsCtz: {
      Register reg1 = kScratchReg;
      Register reg2 = kScratchReg2;
      Label skip_for_zero;
      Label end;
      // Branch if the operand is zero
      __ Branch(&skip_for_zero, eq, i.InputRegister(0), Operand(zero_reg));
      // Find the number of bits before the last bit set to 1.
      __ Subu(reg2, zero_reg, i.InputRegister(0));
      __ And(reg2, reg2, i.InputRegister(0));
      __ clz(reg2, reg2);
      // Get the number of bits after the last bit set to 1.
      __ li(reg1, 0x1F);
      __ Subu(i.OutputRegister(), reg1, reg2);
      __ Branch(&end);
      __ bind(&skip_for_zero);
      // If the operand is zero, return word length as the result.
      __ li(i.OutputRegister(), 0x20);
      __ bind(&end);
    } break;
    case kMipsPopcnt: {
      Register reg1 = kScratchReg;
      Register reg2 = kScratchReg2;
      uint32_t m1 = 0x55555555;
      uint32_t m2 = 0x33333333;
      uint32_t m4 = 0x0f0f0f0f;
      uint32_t m8 = 0x00ff00ff;
      uint32_t m16 = 0x0000ffff;

      // Put count of ones in every 2 bits into those 2 bits.
      __ li(at, m1);
      __ srl(reg1, i.InputRegister(0), 1);
      __ And(reg2, i.InputRegister(0), at);
      __ And(reg1, reg1, at);
      __ addu(reg1, reg1, reg2);

      // Put count of ones in every 4 bits into those 4 bits.
      __ li(at, m2);
      __ srl(reg2, reg1, 2);
      __ And(reg2, reg2, at);
      __ And(reg1, reg1, at);
      __ addu(reg1, reg1, reg2);

      // Put count of ones in every 8 bits into those 8 bits.
      __ li(at, m4);
      __ srl(reg2, reg1, 4);
      __ And(reg2, reg2, at);
      __ And(reg1, reg1, at);
      __ addu(reg1, reg1, reg2);

      // Put count of ones in every 16 bits into those 16 bits.
      __ li(at, m8);
      __ srl(reg2, reg1, 8);
      __ And(reg2, reg2, at);
      __ And(reg1, reg1, at);
      __ addu(reg1, reg1, reg2);

      // Calculate total number of ones.
      __ li(at, m16);
      __ srl(reg2, reg1, 16);
      __ And(reg2, reg2, at);
      __ And(reg1, reg1, at);
      __ addu(i.OutputRegister(), reg1, reg2);
    } break;
    case kMipsShl:
      if (instr->InputAt(1)->IsRegister()) {
        __ sllv(i.OutputRegister(), i.InputRegister(0), i.InputRegister(1));
      } else {
        int32_t imm = i.InputOperand(1).immediate();
        __ sll(i.OutputRegister(), i.InputRegister(0), imm);
      }
      break;
    case kMipsShr:
      if (instr->InputAt(1)->IsRegister()) {
        __ srlv(i.OutputRegister(), i.InputRegister(0), i.InputRegister(1));
      } else {
        int32_t imm = i.InputOperand(1).immediate();
        __ srl(i.OutputRegister(), i.InputRegister(0), imm);
      }
      break;
    case kMipsSar:
      if (instr->InputAt(1)->IsRegister()) {
        __ srav(i.OutputRegister(), i.InputRegister(0), i.InputRegister(1));
      } else {
        int32_t imm = i.InputOperand(1).immediate();
        __ sra(i.OutputRegister(), i.InputRegister(0), imm);
      }
      break;
    case kMipsShlPair: {
      Register second_output =
          instr->OutputCount() >= 2 ? i.OutputRegister(1) : i.TempRegister(0);
      if (instr->InputAt(2)->IsRegister()) {
        __ ShlPair(i.OutputRegister(0), second_output, i.InputRegister(0),
                   i.InputRegister(1), i.InputRegister(2));
      } else {
        uint32_t imm = i.InputOperand(2).immediate();
        __ ShlPair(i.OutputRegister(0), second_output, i.InputRegister(0),
                   i.InputRegister(1), imm);
      }
    } break;
    case kMipsShrPair: {
      Register second_output =
          instr->OutputCount() >= 2 ? i.OutputRegister(1) : i.TempRegister(0);
      if (instr->InputAt(2)->IsRegister()) {
        __ ShrPair(i.OutputRegister(0), second_output, i.InputRegister(0),
                   i.InputRegister(1), i.InputRegister(2));
      } else {
        uint32_t imm = i.InputOperand(2).immediate();
        __ ShrPair(i.OutputRegister(0), second_output, i.InputRegister(0),
                   i.InputRegister(1), imm);
      }
    } break;
    case kMipsSarPair: {
      Register second_output =
          instr->OutputCount() >= 2 ? i.OutputRegister(1) : i.TempRegister(0);
      if (instr->InputAt(2)->IsRegister()) {
        __ SarPair(i.OutputRegister(0), second_output, i.InputRegister(0),
                   i.InputRegister(1), i.InputRegister(2));
      } else {
        uint32_t imm = i.InputOperand(2).immediate();
        __ SarPair(i.OutputRegister(0), second_output, i.InputRegister(0),
                   i.InputRegister(1), imm);
      }
    } break;
    case kMipsExt:
      __ Ext(i.OutputRegister(), i.InputRegister(0), i.InputInt8(1),
             i.InputInt8(2));
      break;
    case kMipsIns:
      if (instr->InputAt(1)->IsImmediate() && i.InputInt8(1) == 0) {
        __ Ins(i.OutputRegister(), zero_reg, i.InputInt8(1), i.InputInt8(2));
      } else {
        __ Ins(i.OutputRegister(), i.InputRegister(0), i.InputInt8(1),
               i.InputInt8(2));
      }
      break;
    case kMipsRor:
      __ Ror(i.OutputRegister(), i.InputRegister(0), i.InputOperand(1));
      break;
    case kMipsTst:
      // Pseudo-instruction used for tst/branch. No opcode emitted here.
      break;
    case kMipsCmp:
      // Pseudo-instruction used for cmp/branch. No opcode emitted here.
      break;
    case kMipsMov:
      // TODO(plind): Should we combine mov/li like this, or use separate instr?
      //    - Also see x64 ASSEMBLE_BINOP & RegisterOrOperandType
      if (HasRegisterInput(instr, 0)) {
        __ mov(i.OutputRegister(), i.InputRegister(0));
      } else {
        __ li(i.OutputRegister(), i.InputOperand(0));
      }
      break;
    case kMipsLsa:
      DCHECK(instr->InputAt(2)->IsImmediate());
      __ Lsa(i.OutputRegister(), i.InputRegister(0), i.InputRegister(1),
             i.InputInt8(2));
      break;
    case kMipsCmpS:
      // Psuedo-instruction used for FP cmp/branch. No opcode emitted here.
      break;
    case kMipsAddS:
      // TODO(plind): add special case: combine mult & add.
      __ add_s(i.OutputDoubleRegister(), i.InputDoubleRegister(0),
               i.InputDoubleRegister(1));
      break;
    case kMipsSubS:
      __ sub_s(i.OutputDoubleRegister(), i.InputDoubleRegister(0),
               i.InputDoubleRegister(1));
      break;
    case kMipsMulS:
      // TODO(plind): add special case: right op is -1.0, see arm port.
      __ mul_s(i.OutputDoubleRegister(), i.InputDoubleRegister(0),
               i.InputDoubleRegister(1));
      break;
    case kMipsDivS:
      __ div_s(i.OutputDoubleRegister(), i.InputDoubleRegister(0),
               i.InputDoubleRegister(1));
      break;
    case kMipsModS: {
      // TODO(bmeurer): We should really get rid of this special instruction,
      // and generate a CallAddress instruction instead.
      FrameScope scope(masm(), StackFrame::MANUAL);
      __ PrepareCallCFunction(0, 2, kScratchReg);
      __ MovToFloatParameters(i.InputDoubleRegister(0),
                              i.InputDoubleRegister(1));
      // TODO(balazs.kilvady): implement mod_two_floats_operation(isolate())
      __ CallCFunction(ExternalReference::mod_two_doubles_operation(isolate()),
                       0, 2);
      // Move the result in the double result register.
      __ MovFromFloatResult(i.OutputSingleRegister());
      break;
    }
    case kMipsAbsS:
      __ abs_s(i.OutputSingleRegister(), i.InputSingleRegister(0));
      break;
    case kMipsSqrtS: {
      __ sqrt_s(i.OutputDoubleRegister(), i.InputDoubleRegister(0));
      break;
    }
    case kMipsMaxS:
      __ max_s(i.OutputDoubleRegister(), i.InputDoubleRegister(0),
               i.InputDoubleRegister(1));
      break;
    case kMipsMinS:
      __ min_s(i.OutputDoubleRegister(), i.InputDoubleRegister(0),
               i.InputDoubleRegister(1));
      break;
    case kMipsCmpD:
      // Psuedo-instruction used for FP cmp/branch. No opcode emitted here.
      break;
    case kMipsAddPair:
      __ AddPair(i.OutputRegister(0), i.OutputRegister(1), i.InputRegister(0),
                 i.InputRegister(1), i.InputRegister(2), i.InputRegister(3));
      break;
    case kMipsSubPair:
      __ SubPair(i.OutputRegister(0), i.OutputRegister(1), i.InputRegister(0),
                 i.InputRegister(1), i.InputRegister(2), i.InputRegister(3));
      break;
    case kMipsMulPair: {
      __ Mulu(i.OutputRegister(1), i.OutputRegister(0), i.InputRegister(0),
              i.InputRegister(2));
      __ mul(kScratchReg, i.InputRegister(0), i.InputRegister(3));
      __ mul(kScratchReg2, i.InputRegister(1), i.InputRegister(2));
      __ Addu(i.OutputRegister(1), i.OutputRegister(1), kScratchReg);
      __ Addu(i.OutputRegister(1), i.OutputRegister(1), kScratchReg2);
    } break;
    case kMipsAddD:
      // TODO(plind): add special case: combine mult & add.
      __ add_d(i.OutputDoubleRegister(), i.InputDoubleRegister(0),
               i.InputDoubleRegister(1));
      break;
    case kMipsSubD:
      __ sub_d(i.OutputDoubleRegister(), i.InputDoubleRegister(0),
               i.InputDoubleRegister(1));
      break;
    case kMipsMaddS:
      __ Madd_s(i.OutputFloatRegister(), i.InputFloatRegister(0),
                i.InputFloatRegister(1), i.InputFloatRegister(2),
                kScratchDoubleReg);
      break;
    case kMipsMaddD:
      __ Madd_d(i.OutputDoubleRegister(), i.InputDoubleRegister(0),
                i.InputDoubleRegister(1), i.InputDoubleRegister(2),
                kScratchDoubleReg);
      break;
    case kMipsMsubS:
      __ Msub_s(i.OutputFloatRegister(), i.InputFloatRegister(0),
                i.InputFloatRegister(1), i.InputFloatRegister(2),
                kScratchDoubleReg);
      break;
    case kMipsMsubD:
      __ Msub_d(i.OutputDoubleRegister(), i.InputDoubleRegister(0),
                i.InputDoubleRegister(1), i.InputDoubleRegister(2),
                kScratchDoubleReg);
      break;
    case kMipsMulD:
      // TODO(plind): add special case: right op is -1.0, see arm port.
      __ mul_d(i.OutputDoubleRegister(), i.InputDoubleRegister(0),
               i.InputDoubleRegister(1));
      break;
    case kMipsDivD:
      __ div_d(i.OutputDoubleRegister(), i.InputDoubleRegister(0),
               i.InputDoubleRegister(1));
      break;
    case kMipsModD: {
      // TODO(bmeurer): We should really get rid of this special instruction,
      // and generate a CallAddress instruction instead.
      FrameScope scope(masm(), StackFrame::MANUAL);
      __ PrepareCallCFunction(0, 2, kScratchReg);
      __ MovToFloatParameters(i.InputDoubleRegister(0),
                              i.InputDoubleRegister(1));
      __ CallCFunction(ExternalReference::mod_two_doubles_operation(isolate()),
                       0, 2);
      // Move the result in the double result register.
      __ MovFromFloatResult(i.OutputDoubleRegister());
      break;
    }
    case kMipsAbsD:
      __ abs_d(i.OutputDoubleRegister(), i.InputDoubleRegister(0));
      break;
    case kMipsNegS:
      __ Neg_s(i.OutputSingleRegister(), i.InputSingleRegister(0));
      break;
    case kMipsNegD:
      __ Neg_d(i.OutputDoubleRegister(), i.InputDoubleRegister(0));
      break;
    case kMipsSqrtD: {
      __ sqrt_d(i.OutputDoubleRegister(), i.InputDoubleRegister(0));
      break;
    }
    case kMipsMaxD:
      __ max_d(i.OutputDoubleRegister(), i.InputDoubleRegister(0),
               i.InputDoubleRegister(1));
      break;
    case kMipsMinD:
      __ min_d(i.OutputDoubleRegister(), i.InputDoubleRegister(0),
               i.InputDoubleRegister(1));
      break;
    case kMipsFloat64RoundDown: {
      ASSEMBLE_ROUND_DOUBLE_TO_DOUBLE(floor);
      break;
    }
    case kMipsFloat32RoundDown: {
      ASSEMBLE_ROUND_FLOAT_TO_FLOAT(floor);
      break;
    }
    case kMipsFloat64RoundTruncate: {
      ASSEMBLE_ROUND_DOUBLE_TO_DOUBLE(trunc);
      break;
    }
    case kMipsFloat32RoundTruncate: {
      ASSEMBLE_ROUND_FLOAT_TO_FLOAT(trunc);
      break;
    }
    case kMipsFloat64RoundUp: {
      ASSEMBLE_ROUND_DOUBLE_TO_DOUBLE(ceil);
      break;
    }
    case kMipsFloat32RoundUp: {
      ASSEMBLE_ROUND_FLOAT_TO_FLOAT(ceil);
      break;
    }
    case kMipsFloat64RoundTiesEven: {
      ASSEMBLE_ROUND_DOUBLE_TO_DOUBLE(round);
      break;
    }
    case kMipsFloat32RoundTiesEven: {
      ASSEMBLE_ROUND_FLOAT_TO_FLOAT(round);
      break;
    }
    case kMipsFloat32Max: {
      FPURegister dst = i.OutputSingleRegister();
      FPURegister src1 = i.InputSingleRegister(0);
      FPURegister src2 = i.InputSingleRegister(1);
      auto ool = new (zone()) OutOfLineFloat32Max(this, dst, src1, src2);
      __ Float32Max(dst, src1, src2, ool->entry());
      __ bind(ool->exit());
      break;
    }
    case kMipsFloat64Max: {
      DoubleRegister dst = i.OutputDoubleRegister();
      DoubleRegister src1 = i.InputDoubleRegister(0);
      DoubleRegister src2 = i.InputDoubleRegister(1);
      auto ool = new (zone()) OutOfLineFloat64Max(this, dst, src1, src2);
      __ Float64Max(dst, src1, src2, ool->entry());
      __ bind(ool->exit());
      break;
    }
    case kMipsFloat32Min: {
      FPURegister dst = i.OutputSingleRegister();
      FPURegister src1 = i.InputSingleRegister(0);
      FPURegister src2 = i.InputSingleRegister(1);
      auto ool = new (zone()) OutOfLineFloat32Min(this, dst, src1, src2);
      __ Float32Min(dst, src1, src2, ool->entry());
      __ bind(ool->exit());
      break;
    }
    case kMipsFloat64Min: {
      DoubleRegister dst = i.OutputDoubleRegister();
      DoubleRegister src1 = i.InputDoubleRegister(0);
      DoubleRegister src2 = i.InputDoubleRegister(1);
      auto ool = new (zone()) OutOfLineFloat64Min(this, dst, src1, src2);
      __ Float64Min(dst, src1, src2, ool->entry());
      __ bind(ool->exit());
      break;
    }
    case kMipsCvtSD: {
      __ cvt_s_d(i.OutputSingleRegister(), i.InputDoubleRegister(0));
      break;
    }
    case kMipsCvtDS: {
      __ cvt_d_s(i.OutputDoubleRegister(), i.InputSingleRegister(0));
      break;
    }
    case kMipsCvtDW: {
      FPURegister scratch = kScratchDoubleReg;
      __ mtc1(i.InputRegister(0), scratch);
      __ cvt_d_w(i.OutputDoubleRegister(), scratch);
      break;
    }
    case kMipsCvtSW: {
      FPURegister scratch = kScratchDoubleReg;
      __ mtc1(i.InputRegister(0), scratch);
      __ cvt_s_w(i.OutputDoubleRegister(), scratch);
      break;
    }
    case kMipsCvtSUw: {
      FPURegister scratch = kScratchDoubleReg;
      __ Cvt_d_uw(i.OutputDoubleRegister(), i.InputRegister(0), scratch);
      __ cvt_s_d(i.OutputDoubleRegister(), i.OutputDoubleRegister());
      break;
    }
    case kMipsCvtDUw: {
      FPURegister scratch = kScratchDoubleReg;
      __ Cvt_d_uw(i.OutputDoubleRegister(), i.InputRegister(0), scratch);
      break;
    }
    case kMipsFloorWD: {
      FPURegister scratch = kScratchDoubleReg;
      __ floor_w_d(scratch, i.InputDoubleRegister(0));
      __ mfc1(i.OutputRegister(), scratch);
      break;
    }
    case kMipsCeilWD: {
      FPURegister scratch = kScratchDoubleReg;
      __ ceil_w_d(scratch, i.InputDoubleRegister(0));
      __ mfc1(i.OutputRegister(), scratch);
      break;
    }
    case kMipsRoundWD: {
      FPURegister scratch = kScratchDoubleReg;
      __ round_w_d(scratch, i.InputDoubleRegister(0));
      __ mfc1(i.OutputRegister(), scratch);
      break;
    }
    case kMipsTruncWD: {
      FPURegister scratch = kScratchDoubleReg;
      // Other arches use round to zero here, so we follow.
      __ trunc_w_d(scratch, i.InputDoubleRegister(0));
      __ mfc1(i.OutputRegister(), scratch);
      break;
    }
    case kMipsFloorWS: {
      FPURegister scratch = kScratchDoubleReg;
      __ floor_w_s(scratch, i.InputDoubleRegister(0));
      __ mfc1(i.OutputRegister(), scratch);
      break;
    }
    case kMipsCeilWS: {
      FPURegister scratch = kScratchDoubleReg;
      __ ceil_w_s(scratch, i.InputDoubleRegister(0));
      __ mfc1(i.OutputRegister(), scratch);
      break;
    }
    case kMipsRoundWS: {
      FPURegister scratch = kScratchDoubleReg;
      __ round_w_s(scratch, i.InputDoubleRegister(0));
      __ mfc1(i.OutputRegister(), scratch);
      break;
    }
    case kMipsTruncWS: {
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
    case kMipsTruncUwD: {
      FPURegister scratch = kScratchDoubleReg;
      // TODO(plind): Fix wrong param order of Trunc_uw_d() macro-asm function.
      __ Trunc_uw_d(i.InputDoubleRegister(0), i.OutputRegister(), scratch);
      break;
    }
    case kMipsTruncUwS: {
      FPURegister scratch = kScratchDoubleReg;
      // TODO(plind): Fix wrong param order of Trunc_uw_s() macro-asm function.
      __ Trunc_uw_s(i.InputDoubleRegister(0), i.OutputRegister(), scratch);
      // Avoid UINT32_MAX as an overflow indicator and use 0 instead,
      // because 0 allows easier out-of-bounds detection.
      __ addiu(kScratchReg, i.OutputRegister(), 1);
      __ Movz(i.OutputRegister(), zero_reg, kScratchReg);
      break;
    }
    case kMipsFloat64ExtractLowWord32:
      __ FmoveLow(i.OutputRegister(), i.InputDoubleRegister(0));
      break;
    case kMipsFloat64ExtractHighWord32:
      __ FmoveHigh(i.OutputRegister(), i.InputDoubleRegister(0));
      break;
    case kMipsFloat64InsertLowWord32:
      __ FmoveLow(i.OutputDoubleRegister(), i.InputRegister(1));
      break;
    case kMipsFloat64InsertHighWord32:
      __ FmoveHigh(i.OutputDoubleRegister(), i.InputRegister(1));
      break;
    case kMipsFloat64SilenceNaN:
      __ FPUCanonicalizeNaN(i.OutputDoubleRegister(), i.InputDoubleRegister(0));
      break;

    // ... more basic instructions ...
    case kMipsSeb:
      __ Seb(i.OutputRegister(), i.InputRegister(0));
      break;
    case kMipsSeh:
      __ Seh(i.OutputRegister(), i.InputRegister(0));
      break;
    case kMipsLbu:
      __ lbu(i.OutputRegister(), i.MemoryOperand());
      break;
    case kMipsLb:
      __ lb(i.OutputRegister(), i.MemoryOperand());
      break;
    case kMipsSb:
      __ sb(i.InputOrZeroRegister(2), i.MemoryOperand());
      break;
    case kMipsLhu:
      __ lhu(i.OutputRegister(), i.MemoryOperand());
      break;
    case kMipsUlhu:
      __ Ulhu(i.OutputRegister(), i.MemoryOperand());
      break;
    case kMipsLh:
      __ lh(i.OutputRegister(), i.MemoryOperand());
      break;
    case kMipsUlh:
      __ Ulh(i.OutputRegister(), i.MemoryOperand());
      break;
    case kMipsSh:
      __ sh(i.InputOrZeroRegister(2), i.MemoryOperand());
      break;
    case kMipsUsh:
      __ Ush(i.InputOrZeroRegister(2), i.MemoryOperand(), kScratchReg);
      break;
    case kMipsLw:
      __ lw(i.OutputRegister(), i.MemoryOperand());
      break;
    case kMipsUlw:
      __ Ulw(i.OutputRegister(), i.MemoryOperand());
      break;
    case kMipsSw:
      __ sw(i.InputOrZeroRegister(2), i.MemoryOperand());
      break;
    case kMipsUsw:
      __ Usw(i.InputOrZeroRegister(2), i.MemoryOperand());
      break;
    case kMipsLwc1: {
      __ lwc1(i.OutputSingleRegister(), i.MemoryOperand());
      break;
    }
    case kMipsUlwc1: {
      __ Ulwc1(i.OutputSingleRegister(), i.MemoryOperand(), kScratchReg);
      break;
    }
    case kMipsSwc1: {
      size_t index = 0;
      MemOperand operand = i.MemoryOperand(&index);
      FPURegister ft = i.InputOrZeroSingleRegister(index);
      if (ft.is(kDoubleRegZero) && !__ IsDoubleZeroRegSet()) {
        __ Move(kDoubleRegZero, 0.0);
      }
      __ swc1(ft, operand);
      break;
    }
    case kMipsUswc1: {
      size_t index = 0;
      MemOperand operand = i.MemoryOperand(&index);
      FPURegister ft = i.InputOrZeroSingleRegister(index);
      if (ft.is(kDoubleRegZero) && !__ IsDoubleZeroRegSet()) {
        __ Move(kDoubleRegZero, 0.0);
      }
      __ Uswc1(ft, operand, kScratchReg);
      break;
    }
    case kMipsLdc1:
      __ ldc1(i.OutputDoubleRegister(), i.MemoryOperand());
      break;
    case kMipsUldc1:
      __ Uldc1(i.OutputDoubleRegister(), i.MemoryOperand(), kScratchReg);
      break;
    case kMipsSdc1: {
      FPURegister ft = i.InputOrZeroDoubleRegister(2);
      if (ft.is(kDoubleRegZero) && !__ IsDoubleZeroRegSet()) {
        __ Move(kDoubleRegZero, 0.0);
      }
      __ sdc1(ft, i.MemoryOperand());
      break;
    }
    case kMipsUsdc1: {
      FPURegister ft = i.InputOrZeroDoubleRegister(2);
      if (ft.is(kDoubleRegZero) && !__ IsDoubleZeroRegSet()) {
        __ Move(kDoubleRegZero, 0.0);
      }
      __ Usdc1(ft, i.MemoryOperand(), kScratchReg);
      break;
    }
    case kMipsPush:
      if (instr->InputAt(0)->IsFPRegister()) {
        __ sdc1(i.InputDoubleRegister(0), MemOperand(sp, -kDoubleSize));
        __ Subu(sp, sp, Operand(kDoubleSize));
        frame_access_state()->IncreaseSPDelta(kDoubleSize / kPointerSize);
      } else {
        __ Push(i.InputRegister(0));
        frame_access_state()->IncreaseSPDelta(1);
      }
      break;
    case kMipsStackClaim: {
      __ Subu(sp, sp, Operand(i.InputInt32(0)));
      frame_access_state()->IncreaseSPDelta(i.InputInt32(0) / kPointerSize);
      break;
    }
    case kMipsStoreToStackSlot: {
      if (instr->InputAt(0)->IsFPRegister()) {
        LocationOperand* op = LocationOperand::cast(instr->InputAt(0));
        if (op->representation() == MachineRepresentation::kFloat64) {
          __ sdc1(i.InputDoubleRegister(0), MemOperand(sp, i.InputInt32(1)));
        } else {
          DCHECK_EQ(MachineRepresentation::kFloat32, op->representation());
          __ swc1(i.InputSingleRegister(0), MemOperand(sp, i.InputInt32(1)));
        }
      } else {
        __ sw(i.InputRegister(0), MemOperand(sp, i.InputInt32(1)));
      }
      break;
    }
    case kMipsByteSwap32: {
      __ ByteSwapSigned(i.OutputRegister(0), i.InputRegister(0), 4);
      break;
    }
    case kCheckedLoadInt8:
      ASSEMBLE_CHECKED_LOAD_INTEGER(lb);
      break;
    case kCheckedLoadUint8:
      ASSEMBLE_CHECKED_LOAD_INTEGER(lbu);
      break;
    case kCheckedLoadInt16:
      ASSEMBLE_CHECKED_LOAD_INTEGER(lh);
      break;
    case kCheckedLoadUint16:
      ASSEMBLE_CHECKED_LOAD_INTEGER(lhu);
      break;
    case kCheckedLoadWord32:
      ASSEMBLE_CHECKED_LOAD_INTEGER(lw);
      break;
    case kCheckedLoadFloat32:
      ASSEMBLE_CHECKED_LOAD_FLOAT(Single, lwc1);
      break;
    case kCheckedLoadFloat64:
      ASSEMBLE_CHECKED_LOAD_FLOAT(Double, ldc1);
      break;
    case kCheckedStoreWord8:
      ASSEMBLE_CHECKED_STORE_INTEGER(sb);
      break;
    case kCheckedStoreWord16:
      ASSEMBLE_CHECKED_STORE_INTEGER(sh);
      break;
    case kCheckedStoreWord32:
      ASSEMBLE_CHECKED_STORE_INTEGER(sw);
      break;
    case kCheckedStoreFloat32:
      ASSEMBLE_CHECKED_STORE_FLOAT(Single, swc1);
      break;
    case kCheckedStoreFloat64:
      ASSEMBLE_CHECKED_STORE_FLOAT(Double, sdc1);
      break;
    case kCheckedLoadWord64:
    case kCheckedStoreWord64:
      UNREACHABLE();  // currently unsupported checked int64 load/store.
      break;
    case kAtomicLoadInt8:
      ASSEMBLE_ATOMIC_LOAD_INTEGER(lb);
      break;
    case kAtomicLoadUint8:
      ASSEMBLE_ATOMIC_LOAD_INTEGER(lbu);
      break;
    case kAtomicLoadInt16:
      ASSEMBLE_ATOMIC_LOAD_INTEGER(lh);
      break;
    case kAtomicLoadUint16:
      ASSEMBLE_ATOMIC_LOAD_INTEGER(lhu);
      break;
    case kAtomicLoadWord32:
      ASSEMBLE_ATOMIC_LOAD_INTEGER(lw);
      break;
    case kAtomicStoreWord8:
      ASSEMBLE_ATOMIC_STORE_INTEGER(sb);
      break;
    case kAtomicStoreWord16:
      ASSEMBLE_ATOMIC_STORE_INTEGER(sh);
      break;
    case kAtomicStoreWord32:
      ASSEMBLE_ATOMIC_STORE_INTEGER(sw);
      break;
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

void AssembleBranchToLabels(CodeGenerator* gen, MacroAssembler* masm,
                            Instruction* instr, FlagsCondition condition,
                            Label* tlabel, Label* flabel, bool fallthru) {
#undef __
#define __ masm->

  Condition cc = kNoCondition;
  // MIPS does not have condition code flags, so compare and branch are
  // implemented differently than on the other arch's. The compare operations
  // emit mips pseudo-instructions, which are handled here by branch
  // instructions that do the actual comparison. Essential that the input
  // registers to compare pseudo-op are not modified before this branch op, as
  // they are tested here.

  MipsOperandConverter i(gen, instr);
  if (instr->arch_opcode() == kMipsTst) {
    cc = FlagsConditionToConditionTst(condition);
    __ And(at, i.InputRegister(0), i.InputOperand(1));
    __ Branch(tlabel, cc, at, Operand(zero_reg));
  } else if (instr->arch_opcode() == kMipsAddOvf) {
    switch (condition) {
      case kOverflow:
        __ AddBranchOvf(i.OutputRegister(), i.InputRegister(0),
                        i.InputOperand(1), tlabel, flabel);
        break;
      case kNotOverflow:
        __ AddBranchOvf(i.OutputRegister(), i.InputRegister(0),
                        i.InputOperand(1), flabel, tlabel);
        break;
      default:
        UNSUPPORTED_COND(kMipsAddOvf, condition);
        break;
    }
  } else if (instr->arch_opcode() == kMipsSubOvf) {
    switch (condition) {
      case kOverflow:
        __ SubBranchOvf(i.OutputRegister(), i.InputRegister(0),
                        i.InputOperand(1), tlabel, flabel);
        break;
      case kNotOverflow:
        __ SubBranchOvf(i.OutputRegister(), i.InputRegister(0),
                        i.InputOperand(1), flabel, tlabel);
        break;
      default:
        UNSUPPORTED_COND(kMipsAddOvf, condition);
        break;
    }
  } else if (instr->arch_opcode() == kMipsMulOvf) {
    switch (condition) {
      case kOverflow:
        __ MulBranchOvf(i.OutputRegister(), i.InputRegister(0),
                        i.InputOperand(1), tlabel, flabel);
        break;
      case kNotOverflow:
        __ MulBranchOvf(i.OutputRegister(), i.InputRegister(0),
                        i.InputOperand(1), flabel, tlabel);
        break;
      default:
        UNSUPPORTED_COND(kMipsMulOvf, condition);
        break;
    }
  } else if (instr->arch_opcode() == kMipsCmp) {
    cc = FlagsConditionToConditionCmp(condition);
    __ Branch(tlabel, cc, i.InputRegister(0), i.InputOperand(1));
  } else if (instr->arch_opcode() == kMipsCmpS) {
    if (!convertCondition(condition, cc)) {
      UNSUPPORTED_COND(kMips64CmpS, condition);
    }
    FPURegister left = i.InputOrZeroSingleRegister(0);
    FPURegister right = i.InputOrZeroSingleRegister(1);
    if ((left.is(kDoubleRegZero) || right.is(kDoubleRegZero)) &&
        !__ IsDoubleZeroRegSet()) {
      __ Move(kDoubleRegZero, 0.0);
    }
    __ BranchF32(tlabel, nullptr, cc, left, right);
  } else if (instr->arch_opcode() == kMipsCmpD) {
    if (!convertCondition(condition, cc)) {
      UNSUPPORTED_COND(kMips64CmpD, condition);
    }
    FPURegister left = i.InputOrZeroDoubleRegister(0);
    FPURegister right = i.InputOrZeroDoubleRegister(1);
    if ((left.is(kDoubleRegZero) || right.is(kDoubleRegZero)) &&
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
#define __ masm()->
}

// Assembles branches after an instruction.
void CodeGenerator::AssembleArchBranch(Instruction* instr, BranchInfo* branch) {
  Label* tlabel = branch->true_label;
  Label* flabel = branch->false_label;
  AssembleBranchToLabels(this, masm(), instr, branch->condition, tlabel, flabel,
                         branch->fallthru);
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

      Runtime::FunctionId trap_id = static_cast<Runtime::FunctionId>(
          i.InputInt32(instr_->InputCount() - 1));
      bool old_has_frame = __ has_frame();
      if (frame_elided_) {
        __ set_has_frame(true);
        __ EnterFrame(StackFrame::WASM_COMPILED);
      }
      GenerateCallToTrap(trap_id);
      if (frame_elided_) {
        __ set_has_frame(old_has_frame);
      }
      if (FLAG_debug_code) {
        __ stop(GetBailoutReason(kUnexpectedReturnFromWasmTrap));
      }
    }

   private:
    void GenerateCallToTrap(Runtime::FunctionId trap_id) {
      if (trap_id == Runtime::kNumFunctions) {
        // We cannot test calls to the runtime in cctest/test-run-wasm.
        // Therefore we emit a call to C here instead of a call to the runtime.
        // We use the context register as the scratch register, because we do
        // not have a context here.
        __ PrepareCallCFunction(0, 0, cp);
        __ CallCFunction(
            ExternalReference::wasm_call_trap_callback_for_testing(isolate()),
            0);
      } else {
        __ Move(cp, isolate()->native_context());
        gen_->AssembleSourcePosition(instr_);
        __ CallRuntime(trap_id);
      }
      ReferenceMap* reference_map =
          new (gen_->zone()) ReferenceMap(gen_->zone());
      gen_->RecordSafepoint(reference_map, Safepoint::kSimple, 0,
                            Safepoint::kNoLazyDeopt);
    }

    bool frame_elided_;
    Instruction* instr_;
    CodeGenerator* gen_;
  };
  bool frame_elided = !frame_access_state()->has_frame();
  auto ool = new (zone()) OutOfLineTrap(this, frame_elided, instr);
  Label* tlabel = ool->entry();
  AssembleBranchToLabels(this, masm(), instr, condition, tlabel, nullptr, true);
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
  // emit mips psuedo-instructions, which are checked and handled here.

  if (instr->arch_opcode() == kMipsTst) {
    cc = FlagsConditionToConditionTst(condition);
    if (instr->InputAt(1)->IsImmediate() &&
        base::bits::IsPowerOfTwo32(i.InputOperand(1).immediate())) {
      uint16_t pos =
          base::bits::CountTrailingZeros32(i.InputOperand(1).immediate());
      __ Ext(result, i.InputRegister(0), pos, 1);
    } else {
      __ And(kScratchReg, i.InputRegister(0), i.InputOperand(1));
      __ Sltu(result, zero_reg, kScratchReg);
    }
    if (cc == eq) {
      // Sltu produces 0 for equality, invert the result.
      __ xori(result, result, 1);
    }
    return;
  } else if (instr->arch_opcode() == kMipsAddOvf ||
             instr->arch_opcode() == kMipsSubOvf ||
             instr->arch_opcode() == kMipsMulOvf) {
    Label flabel, tlabel;
    switch (instr->arch_opcode()) {
      case kMipsAddOvf:
        __ AddBranchNoOvf(i.OutputRegister(), i.InputRegister(0),
                          i.InputOperand(1), &flabel);

        break;
      case kMipsSubOvf:
        __ SubBranchNoOvf(i.OutputRegister(), i.InputRegister(0),
                          i.InputOperand(1), &flabel);
        break;
      case kMipsMulOvf:
        __ MulBranchNoOvf(i.OutputRegister(), i.InputRegister(0),
                          i.InputOperand(1), &flabel);
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
  } else if (instr->arch_opcode() == kMipsCmp) {
    cc = FlagsConditionToConditionCmp(condition);
    switch (cc) {
      case eq:
      case ne: {
        Register left = i.InputRegister(0);
        Operand right = i.InputOperand(1);
        Register select;
        if (instr->InputAt(1)->IsImmediate() && right.immediate() == 0) {
          // Pass left operand if right is zero.
          select = left;
        } else {
          __ Subu(kScratchReg, left, right);
          select = kScratchReg;
        }
        __ Sltu(result, zero_reg, select);
        if (cc == eq) {
          // Sltu produces 0 for equality, invert the result.
          __ xori(result, result, 1);
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
  } else if (instr->arch_opcode() == kMipsCmpD ||
             instr->arch_opcode() == kMipsCmpS) {
    FPURegister left = i.InputOrZeroDoubleRegister(0);
    FPURegister right = i.InputOrZeroDoubleRegister(1);
    if ((left.is(kDoubleRegZero) || right.is(kDoubleRegZero)) &&
        !__ IsDoubleZeroRegSet()) {
      __ Move(kDoubleRegZero, 0.0);
    }
    bool predicate;
    FPUCondition cc = FlagsConditionToConditionCmpFPU(predicate, condition);
    if (!IsMipsArchVariant(kMips32r6)) {
      __ li(result, Operand(1));
      if (instr->arch_opcode() == kMipsCmpD) {
        __ c(cc, D, left, right);
      } else {
        DCHECK(instr->arch_opcode() == kMipsCmpS);
        __ c(cc, S, left, right);
      }
      if (predicate) {
        __ Movf(result, zero_reg);
      } else {
        __ Movt(result, zero_reg);
      }
    } else {
      if (instr->arch_opcode() == kMipsCmpD) {
        __ cmp(cc, L, kDoubleCompareReg, left, right);
      } else {
        DCHECK(instr->arch_opcode() == kMipsCmpS);
        __ cmp(cc, W, kDoubleCompareReg, left, right);
      }
      __ mfc1(result, kDoubleCompareReg);
      __ andi(result, result, 1);  // Cmp returns all 1's/0's, use only LSB.
      if (!predicate)          // Toggle result for not equal.
        __ xori(result, result, 1);
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

CodeGenerator::CodeGenResult CodeGenerator::AssembleDeoptimizerCall(
    int deoptimization_id, Deoptimizer::BailoutType bailout_type,
    SourcePosition pos) {
  Address deopt_entry = Deoptimizer::GetDeoptimizationEntry(
      isolate(), deoptimization_id, bailout_type);
  if (deopt_entry == nullptr) return kTooManyDeoptimizationBailouts;
  DeoptimizeReason deoptimization_reason =
      GetDeoptimizationReason(deoptimization_id);
  __ RecordDeoptReason(deoptimization_reason, pos, deoptimization_id);
  __ Call(deopt_entry, RelocInfo::RUNTIME_ENTRY);
  return kSuccess;
}

void CodeGenerator::FinishFrame(Frame* frame) {
  CallDescriptor* descriptor = linkage()->GetIncomingDescriptor();

  const RegList saves_fpu = descriptor->CalleeSavedFPRegisters();
  if (saves_fpu != 0) {
    frame->AlignSavedCalleeRegisterSlots();
  }

  if (saves_fpu != 0) {
    int count = base::bits::CountPopulation32(saves_fpu);
    DCHECK(kNumCalleeSavedFPU == count);
    frame->AllocateSavedCalleeRegisterSlots(count *
                                            (kDoubleSize / kPointerSize));
  }

  const RegList saves = descriptor->CalleeSavedRegisters();
  if (saves != 0) {
    int count = base::bits::CountPopulation32(saves);
    DCHECK(kNumCalleeSaved == count + 1);
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
      __ Prologue(this->info()->GeneratePreagedPrologue());
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
    __ Abort(kShouldNotDirectlyEnterOsrFunction);

    // Unoptimized code jumps directly to this entrypoint while the unoptimized
    // frame is still on the stack. Optimized code uses OSR values directly from
    // the unoptimized frame. Thus, all that needs to be done is to allocate the
    // remaining stack slots.
    if (FLAG_code_comments) __ RecordComment("-- OSR entrypoint --");
    osr_pc_offset_ = __ pc_offset();
    shrink_slots -= OsrHelper(info()).UnoptimizedFrameSlots();
  }

  const RegList saves_fpu = descriptor->CalleeSavedFPRegisters();
  if (shrink_slots > 0) {
    __ Subu(sp, sp, Operand(shrink_slots * kPointerSize));
  }

  // Save callee-saved FPU registers.
  if (saves_fpu != 0) {
    __ MultiPushFPU(saves_fpu);
  }

  const RegList saves = descriptor->CalleeSavedRegisters();
  if (saves != 0) {
    // Save callee-saved registers.
    __ MultiPush(saves);
    DCHECK(kNumCalleeSaved == base::bits::CountPopulation32(saves) + 1);
  }
}

void CodeGenerator::AssembleReturn(InstructionOperand* pop) {
  CallDescriptor* descriptor = linkage()->GetIncomingDescriptor();
  int pop_count = static_cast<int>(descriptor->StackParameterCount());

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
  if (pop->IsImmediate()) {
    DCHECK_EQ(Constant::kInt32, g.ToConstant(pop).type());
    pop_count += g.ToConstant(pop).ToInt32();
  } else {
    Register pop_reg = g.ToRegister(pop);
    __ sll(pop_reg, pop_reg, kPointerSizeLog2);
    __ Addu(sp, sp, Operand(pop_reg));
  }
  if (pop_count != 0) {
    __ DropAndRet(pop_count);
  } else {
    __ Ret();
  }
}


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
      __ sw(src, g.ToMemOperand(destination));
    }
  } else if (source->IsStackSlot()) {
    DCHECK(destination->IsRegister() || destination->IsStackSlot());
    MemOperand src = g.ToMemOperand(source);
    if (destination->IsRegister()) {
      __ lw(g.ToRegister(destination), src);
    } else {
      Register temp = kScratchReg;
      __ lw(temp, src);
      __ sw(temp, g.ToMemOperand(destination));
    }
  } else if (source->IsConstant()) {
    Constant src = g.ToConstant(source);
    if (destination->IsRegister() || destination->IsStackSlot()) {
      Register dst =
          destination->IsRegister() ? g.ToRegister(destination) : kScratchReg;
      switch (src.type()) {
        case Constant::kInt32:
          if (RelocInfo::IsWasmReference(src.rmode())) {
            __ li(dst, Operand(src.ToInt32(), src.rmode()));
          } else {
            __ li(dst, Operand(src.ToInt32()));
          }
          break;
        case Constant::kFloat32:
          __ li(dst, isolate()->factory()->NewNumber(src.ToFloat32(), TENURED));
          break;
        case Constant::kInt64:
          UNREACHABLE();
          break;
        case Constant::kFloat64:
          __ li(dst, isolate()->factory()->NewNumber(src.ToFloat64(), TENURED));
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
          UNREACHABLE();  // TODO(titzer): loading RPO numbers on mips.
          break;
      }
      if (destination->IsStackSlot()) __ sw(dst, g.ToMemOperand(destination));
    } else if (src.type() == Constant::kFloat32) {
      if (destination->IsFPStackSlot()) {
        MemOperand dst = g.ToMemOperand(destination);
        if (bit_cast<int32_t>(src.ToFloat32()) == 0) {
          __ sw(zero_reg, dst);
        } else {
          __ li(at, Operand(bit_cast<int32_t>(src.ToFloat32())));
          __ sw(at, dst);
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
      __ Move(dst, src.ToFloat64());
      if (destination->IsFPStackSlot()) {
        __ sdc1(dst, g.ToMemOperand(destination));
      }
    }
  } else if (source->IsFPRegister()) {
    FPURegister src = g.ToDoubleRegister(source);
    if (destination->IsFPRegister()) {
      FPURegister dst = g.ToDoubleRegister(destination);
      __ Move(dst, src);
    } else {
      DCHECK(destination->IsFPStackSlot());
      MachineRepresentation rep =
          LocationOperand::cast(source)->representation();
      if (rep == MachineRepresentation::kFloat64) {
        __ sdc1(src, g.ToMemOperand(destination));
      } else if (rep == MachineRepresentation::kFloat32) {
        __ swc1(src, g.ToMemOperand(destination));
      } else {
        DCHECK_EQ(MachineRepresentation::kSimd128, rep);
        UNREACHABLE();
      }
    }
  } else if (source->IsFPStackSlot()) {
    DCHECK(destination->IsFPRegister() || destination->IsFPStackSlot());
    MemOperand src = g.ToMemOperand(source);
    MachineRepresentation rep = LocationOperand::cast(source)->representation();
    if (destination->IsFPRegister()) {
      if (rep == MachineRepresentation::kFloat64) {
        __ ldc1(g.ToDoubleRegister(destination), src);
      } else if (rep == MachineRepresentation::kFloat32) {
        __ lwc1(g.ToDoubleRegister(destination), src);
      } else {
        DCHECK_EQ(MachineRepresentation::kSimd128, rep);
        UNREACHABLE();
      }
    } else {
      FPURegister temp = kScratchDoubleReg;
      if (rep == MachineRepresentation::kFloat64) {
        __ ldc1(temp, src);
        __ sdc1(temp, g.ToMemOperand(destination));
      } else if (rep == MachineRepresentation::kFloat32) {
        __ lwc1(temp, src);
        __ swc1(temp, g.ToMemOperand(destination));
      } else {
        DCHECK_EQ(MachineRepresentation::kSimd128, rep);
        UNREACHABLE();
      }
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
      __ lw(src, dst);
      __ sw(temp, dst);
    }
  } else if (source->IsStackSlot()) {
    DCHECK(destination->IsStackSlot());
    Register temp_0 = kScratchReg;
    Register temp_1 = kCompareReg;
    MemOperand src = g.ToMemOperand(source);
    MemOperand dst = g.ToMemOperand(destination);
    __ lw(temp_0, src);
    __ lw(temp_1, dst);
    __ sw(temp_0, dst);
    __ sw(temp_1, src);
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
      MachineRepresentation rep =
          LocationOperand::cast(source)->representation();
      if (rep == MachineRepresentation::kFloat64) {
        __ Move(temp, src);
        __ ldc1(src, dst);
        __ sdc1(temp, dst);
      } else if (rep == MachineRepresentation::kFloat32) {
        __ Move(temp, src);
        __ lwc1(src, dst);
        __ swc1(temp, dst);
      } else {
        DCHECK_EQ(MachineRepresentation::kSimd128, rep);
        UNREACHABLE();
      }
    }
  } else if (source->IsFPStackSlot()) {
    DCHECK(destination->IsFPStackSlot());
    Register temp_0 = kScratchReg;
    FPURegister temp_1 = kScratchDoubleReg;
    MemOperand src0 = g.ToMemOperand(source);
    MemOperand dst0 = g.ToMemOperand(destination);
    MachineRepresentation rep = LocationOperand::cast(source)->representation();
    if (rep == MachineRepresentation::kFloat64) {
      MemOperand src1(src0.rm(), src0.offset() + kIntSize);
      MemOperand dst1(dst0.rm(), dst0.offset() + kIntSize);
      __ ldc1(temp_1, dst0);  // Save destination in temp_1.
      __ lw(temp_0, src0);    // Then use temp_0 to copy source to destination.
      __ sw(temp_0, dst0);
      __ lw(temp_0, src1);
      __ sw(temp_0, dst1);
      __ sdc1(temp_1, src0);
    } else if (rep == MachineRepresentation::kFloat32) {
      __ lwc1(temp_1, dst0);  // Save destination in temp_1.
      __ lw(temp_0, src0);    // Then use temp_0 to copy source to destination.
      __ sw(temp_0, dst0);
      __ swc1(temp_1, src0);
    } else {
      DCHECK_EQ(MachineRepresentation::kSimd128, rep);
      UNREACHABLE();
    }
  } else {
    // No other combinations are possible.
    UNREACHABLE();
  }
}


void CodeGenerator::AssembleJumpTable(Label** targets, size_t target_count) {
  // On 32-bit MIPS we emit the jump tables inline.
  UNREACHABLE();
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
    // Block tramoline pool emission for duration of padding.
    v8::internal::Assembler::BlockTrampolinePoolScope block_trampoline_pool(
        masm());
    int padding_size = last_lazy_deopt_pc_ + space_needed - current_pc;
    DCHECK_EQ(0, padding_size % v8::internal::Assembler::kInstrSize);
    while (padding_size > 0) {
      __ nop();
      padding_size -= v8::internal::Assembler::kInstrSize;
    }
  }
}

#undef __

}  // namespace compiler
}  // namespace internal
}  // namespace v8
