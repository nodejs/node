// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/ast/scopes.h"
#include "src/compiler/code-generator.h"
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
        return Operand(
            isolate()->factory()->NewNumber(constant.ToFloat32(), TENURED));
      case Constant::kFloat64:
        return Operand(
            isolate()->factory()->NewNumber(constant.ToFloat64(), TENURED));
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
    DCHECK(op->IsStackSlot() || op->IsDoubleStackSlot());
    FrameOffset offset = frame_access_state()->GetFrameOffset(
        AllocatedOperand::cast(op)->index());
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
        mode_(mode) {}

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
    if (!frame()->needs_frame()) {
      // We need to save and restore ra if the frame was elided.
      __ Push(ra);
    }
    RecordWriteStub stub(isolate(), object_, scratch0_, scratch1_,
                         remembered_set_action, save_fp_mode);
    __ Daddu(scratch1_, object_, index_);
    __ CallStub(&stub);
    if (!frame()->needs_frame()) {
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
};


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
      __ Daddu(kScratchReg, i.InputRegister(2), offset);                      \
      __ asm_instr(result, MemOperand(kScratchReg, 0));                       \
    } else {                                                                  \
      int offset = static_cast<int>(i.InputOperand(0).immediate());           \
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
      __ Daddu(kScratchReg, i.InputRegister(2), offset);                      \
      __ asm_instr(result, MemOperand(kScratchReg, 0));                       \
    } else {                                                                  \
      int offset = static_cast<int>(i.InputOperand(0).immediate());           \
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
      auto value = i.Input##width##Register(2);                        \
      __ Branch(USE_DELAY_SLOT, &done, hs, offset, i.InputOperand(1)); \
      __ Daddu(kScratchReg, i.InputRegister(3), offset);               \
      __ asm_instr(value, MemOperand(kScratchReg, 0));                 \
    } else {                                                           \
      int offset = static_cast<int>(i.InputOperand(0).immediate());    \
      auto value = i.Input##width##Register(2);                        \
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
      auto value = i.InputRegister(2);                                 \
      __ Branch(USE_DELAY_SLOT, &done, hs, offset, i.InputOperand(1)); \
      __ Daddu(kScratchReg, i.InputRegister(3), offset);               \
      __ asm_instr(value, MemOperand(kScratchReg, 0));                 \
    } else {                                                           \
      int offset = static_cast<int>(i.InputOperand(0).immediate());    \
      auto value = i.InputRegister(2);                                 \
      __ Branch(&done, ls, i.InputRegister(1), Operand(offset));       \
      __ asm_instr(value, MemOperand(i.InputRegister(3), offset));     \
    }                                                                  \
    __ bind(&done);                                                    \
  } while (0)


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

void CodeGenerator::AssembleDeconstructActivationRecord(int stack_param_delta) {
  int sp_slot_delta = TailCallFrameStackSlotDelta(stack_param_delta);
  if (sp_slot_delta > 0) {
    __ daddiu(sp, sp, sp_slot_delta * kPointerSize);
  }
  frame_access_state()->SetFrameAccessToDefault();
}


void CodeGenerator::AssemblePrepareTailCall(int stack_param_delta) {
  int sp_slot_delta = TailCallFrameStackSlotDelta(stack_param_delta);
  if (sp_slot_delta < 0) {
    __ Dsubu(sp, sp, Operand(-sp_slot_delta * kPointerSize));
    frame_access_state()->IncreaseSPDelta(-sp_slot_delta);
  }
  if (frame()->needs_frame()) {
    __ ld(ra, MemOperand(fp, StandardFrameConstants::kCallerPCOffset));
    __ ld(fp, MemOperand(fp, StandardFrameConstants::kCallerFPOffset));
  }
  frame_access_state()->SetFrameAccessToSP();
}


// Assembles an instruction after register allocation, producing machine code.
void CodeGenerator::AssembleArchInstruction(Instruction* instr) {
  MipsOperandConverter i(this, instr);
  InstructionCode opcode = instr->opcode();

  switch (ArchOpcodeField::decode(opcode)) {
    case kArchCallCodeObject: {
      EnsureSpaceForLazyDeopt();
      if (instr->InputAt(0)->IsImmediate()) {
        __ Call(Handle<Code>::cast(i.InputHeapObject(0)),
                RelocInfo::CODE_TARGET);
      } else {
        __ daddiu(at, i.InputRegister(0), Code::kHeaderSize - kHeapObjectTag);
        __ Call(at);
      }
      RecordCallPosition(instr);
      frame_access_state()->ClearSPDelta();
      break;
    }
    case kArchTailCallCodeObject: {
      int stack_param_delta = i.InputInt32(instr->InputCount() - 1);
      AssembleDeconstructActivationRecord(stack_param_delta);
      if (instr->InputAt(0)->IsImmediate()) {
        __ Jump(Handle<Code>::cast(i.InputHeapObject(0)),
                RelocInfo::CODE_TARGET);
      } else {
        __ daddiu(at, i.InputRegister(0), Code::kHeaderSize - kHeapObjectTag);
        __ Jump(at);
      }
      frame_access_state()->ClearSPDelta();
      break;
    }
    case kArchCallJSFunction: {
      EnsureSpaceForLazyDeopt();
      Register func = i.InputRegister(0);
      if (FLAG_debug_code) {
        // Check the function's context matches the context argument.
        __ ld(kScratchReg, FieldMemOperand(func, JSFunction::kContextOffset));
        __ Assert(eq, kWrongFunctionContext, cp, Operand(kScratchReg));
      }
      __ ld(at, FieldMemOperand(func, JSFunction::kCodeEntryOffset));
      __ Call(at);
      RecordCallPosition(instr);
      frame_access_state()->ClearSPDelta();
      break;
    }
    case kArchTailCallJSFunction: {
      Register func = i.InputRegister(0);
      if (FLAG_debug_code) {
        // Check the function's context matches the context argument.
        __ ld(kScratchReg, FieldMemOperand(func, JSFunction::kContextOffset));
        __ Assert(eq, kWrongFunctionContext, cp, Operand(kScratchReg));
      }
      int stack_param_delta = i.InputInt32(instr->InputCount() - 1);
      AssembleDeconstructActivationRecord(stack_param_delta);
      __ ld(at, FieldMemOperand(func, JSFunction::kCodeEntryOffset));
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
      AssemblePrepareTailCall(i.InputInt32(instr->InputCount() - 1));
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
    case kArchNop:
    case kArchThrowTerminator:
      // don't emit code for nops.
      break;
    case kArchDeoptimize: {
      int deopt_state_id =
          BuildTranslation(instr, -1, 0, OutputFrameStateCombine::Ignore());
      Deoptimizer::BailoutType bailout_type =
          Deoptimizer::BailoutType(MiscField::decode(instr->opcode()));
      AssembleDeoptimizerCall(deopt_state_id, bailout_type);
      break;
    }
    case kArchRet:
      AssembleReturn();
      break;
    case kArchStackPointer:
      __ mov(i.OutputRegister(), sp);
      break;
    case kArchFramePointer:
      __ mov(i.OutputRegister(), fp);
      break;
    case kArchParentFramePointer:
      if (frame_access_state()->frame()->needs_frame()) {
        __ ld(i.OutputRegister(), MemOperand(fp, 0));
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
      __ Daddu(at, object, index);
      __ sd(value, MemOperand(at));
      __ CheckPageFlag(object, scratch0,
                       MemoryChunk::kPointersFromHereAreInterestingMask, ne,
                       ool->entry());
      __ bind(ool->exit());
      break;
    }
    case kArchStackSlot: {
      FrameOffset offset =
          frame_access_state()->GetFrameOffset(i.InputInt32(0));
      __ Daddu(i.OutputRegister(), offset.from_stack_pointer() ? sp : fp,
               Operand(offset.offset()));
      break;
    }
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
    case kMips64And:
      __ And(i.OutputRegister(), i.InputRegister(0), i.InputOperand(1));
      break;
    case kMips64Or:
      __ Or(i.OutputRegister(), i.InputRegister(0), i.InputOperand(1));
      break;
    case kMips64Nor:
      if (instr->InputAt(1)->IsRegister()) {
        __ Nor(i.OutputRegister(), i.InputRegister(0), i.InputOperand(1));
      } else {
        DCHECK(i.InputOperand(1).immediate() == 0);
        __ Nor(i.OutputRegister(), i.InputRegister(0), zero_reg);
      }
      break;
    case kMips64Xor:
      __ Xor(i.OutputRegister(), i.InputRegister(0), i.InputOperand(1));
      break;
    case kMips64Clz:
      __ Clz(i.OutputRegister(), i.InputRegister(0));
      break;
    case kMips64Dclz:
      __ dclz(i.OutputRegister(), i.InputRegister(0));
      break;
    case kMips64Ctz: {
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
    case kMips64Dctz: {
      Register reg1 = kScratchReg;
      Register reg2 = kScratchReg2;
      Label skip_for_zero;
      Label end;
      // Branch if the operand is zero
      __ Branch(&skip_for_zero, eq, i.InputRegister(0), Operand(zero_reg));
      // Find the number of bits before the last bit set to 1.
      __ Dsubu(reg2, zero_reg, i.InputRegister(0));
      __ And(reg2, reg2, i.InputRegister(0));
      __ dclz(reg2, reg2);
      // Get the number of bits after the last bit set to 1.
      __ li(reg1, 0x3F);
      __ Subu(i.OutputRegister(), reg1, reg2);
      __ Branch(&end);
      __ bind(&skip_for_zero);
      // If the operand is zero, return word length as the result.
      __ li(i.OutputRegister(), 0x40);
      __ bind(&end);
    } break;
    case kMips64Popcnt: {
      Register reg1 = kScratchReg;
      Register reg2 = kScratchReg2;
      uint32_t m1 = 0x55555555;
      uint32_t m2 = 0x33333333;
      uint32_t m4 = 0x0f0f0f0f;
      uint32_t m8 = 0x00ff00ff;
      uint32_t m16 = 0x0000ffff;

      // Put count of ones in every 2 bits into those 2 bits.
      __ li(at, m1);
      __ dsrl(reg1, i.InputRegister(0), 1);
      __ And(reg2, i.InputRegister(0), at);
      __ And(reg1, reg1, at);
      __ Daddu(reg1, reg1, reg2);

      // Put count of ones in every 4 bits into those 4 bits.
      __ li(at, m2);
      __ dsrl(reg2, reg1, 2);
      __ And(reg2, reg2, at);
      __ And(reg1, reg1, at);
      __ Daddu(reg1, reg1, reg2);

      // Put count of ones in every 8 bits into those 8 bits.
      __ li(at, m4);
      __ dsrl(reg2, reg1, 4);
      __ And(reg2, reg2, at);
      __ And(reg1, reg1, at);
      __ Daddu(reg1, reg1, reg2);

      // Put count of ones in every 16 bits into those 16 bits.
      __ li(at, m8);
      __ dsrl(reg2, reg1, 8);
      __ And(reg2, reg2, at);
      __ And(reg1, reg1, at);
      __ Daddu(reg1, reg1, reg2);

      // Calculate total number of ones.
      __ li(at, m16);
      __ dsrl(reg2, reg1, 16);
      __ And(reg2, reg2, at);
      __ And(reg1, reg1, at);
      __ Daddu(i.OutputRegister(), reg1, reg2);
    } break;
    case kMips64Dpopcnt: {
      Register reg1 = kScratchReg;
      Register reg2 = kScratchReg2;
      uint64_t m1 = 0x5555555555555555;
      uint64_t m2 = 0x3333333333333333;
      uint64_t m4 = 0x0f0f0f0f0f0f0f0f;
      uint64_t m8 = 0x00ff00ff00ff00ff;
      uint64_t m16 = 0x0000ffff0000ffff;
      uint64_t m32 = 0x00000000ffffffff;

      // Put count of ones in every 2 bits into those 2 bits.
      __ li(at, m1);
      __ dsrl(reg1, i.InputRegister(0), 1);
      __ and_(reg2, i.InputRegister(0), at);
      __ and_(reg1, reg1, at);
      __ Daddu(reg1, reg1, reg2);

      // Put count of ones in every 4 bits into those 4 bits.
      __ li(at, m2);
      __ dsrl(reg2, reg1, 2);
      __ and_(reg2, reg2, at);
      __ and_(reg1, reg1, at);
      __ Daddu(reg1, reg1, reg2);

      // Put count of ones in every 8 bits into those 8 bits.
      __ li(at, m4);
      __ dsrl(reg2, reg1, 4);
      __ and_(reg2, reg2, at);
      __ and_(reg1, reg1, at);
      __ Daddu(reg1, reg1, reg2);

      // Put count of ones in every 16 bits into those 16 bits.
      __ li(at, m8);
      __ dsrl(reg2, reg1, 8);
      __ and_(reg2, reg2, at);
      __ and_(reg1, reg1, at);
      __ Daddu(reg1, reg1, reg2);

      // Put count of ones in every 32 bits into those 32 bits.
      __ li(at, m16);
      __ dsrl(reg2, reg1, 16);
      __ and_(reg2, reg2, at);
      __ and_(reg1, reg1, at);
      __ Daddu(reg1, reg1, reg2);

      // Calculate total number of ones.
      __ li(at, m32);
      __ dsrl32(reg2, reg1, 0);
      __ and_(reg2, reg2, at);
      __ and_(reg1, reg1, at);
      __ Daddu(i.OutputRegister(), reg1, reg2);
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
        __ srlv(i.OutputRegister(), i.InputRegister(0), i.InputRegister(1));
      } else {
        int64_t imm = i.InputOperand(1).immediate();
        __ srl(i.OutputRegister(), i.InputRegister(0),
               static_cast<uint16_t>(imm));
      }
      break;
    case kMips64Sar:
      if (instr->InputAt(1)->IsRegister()) {
        __ srav(i.OutputRegister(), i.InputRegister(0), i.InputRegister(1));
      } else {
        int64_t imm = i.InputOperand(1).immediate();
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
      int16_t pos = i.InputInt8(1);
      int16_t size = i.InputInt8(2);
      if (size > 0 && size <= 32 && pos >= 0 && pos < 32) {
        __ Dext(i.OutputRegister(), i.InputRegister(0), i.InputInt8(1),
                i.InputInt8(2));
      } else if (size > 32 && size <= 64 && pos > 0 && pos < 32) {
        __ Dextm(i.OutputRegister(), i.InputRegister(0), i.InputInt8(1),
                 i.InputInt8(2));
      } else {
        DCHECK(size > 0 && size <= 32 && pos >= 32 && pos < 64);
        __ Dextu(i.OutputRegister(), i.InputRegister(0), i.InputInt8(1),
                 i.InputInt8(2));
      }
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
      // Psuedo-instruction used for FP cmp/branch. No opcode emitted here.
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
    case kMips64AbsS:
      __ abs_s(i.OutputSingleRegister(), i.InputSingleRegister(0));
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
      // Psuedo-instruction used for FP cmp/branch. No opcode emitted here.
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
    case kMips64AbsD:
      __ abs_d(i.OutputDoubleRegister(), i.InputDoubleRegister(0));
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
    case kMips64Float64Max: {
      // (b < a) ? a : b
      if (kArchVariant == kMips64r6) {
        __ cmp_d(OLT, i.OutputDoubleRegister(), i.InputDoubleRegister(1),
                 i.InputDoubleRegister(0));
        __ sel_d(i.OutputDoubleRegister(), i.InputDoubleRegister(1),
                 i.InputDoubleRegister(0));
      } else {
        __ c_d(OLT, i.InputDoubleRegister(0), i.InputDoubleRegister(1));
        // Left operand is result, passthrough if false.
        __ movt_d(i.OutputDoubleRegister(), i.InputDoubleRegister(1));
      }
      break;
    }
    case kMips64Float64Min: {
      // (a < b) ? a : b
      if (kArchVariant == kMips64r6) {
        __ cmp_d(OLT, i.OutputDoubleRegister(), i.InputDoubleRegister(0),
                 i.InputDoubleRegister(1));
        __ sel_d(i.OutputDoubleRegister(), i.InputDoubleRegister(1),
                 i.InputDoubleRegister(0));
      } else {
        __ c_d(OLT, i.InputDoubleRegister(1), i.InputDoubleRegister(0));
        // Right operand is result, passthrough if false.
        __ movt_d(i.OutputDoubleRegister(), i.InputDoubleRegister(1));
      }
      break;
    }
    case kMips64Float32Max: {
      // (b < a) ? a : b
      if (kArchVariant == kMips64r6) {
        __ cmp_s(OLT, i.OutputDoubleRegister(), i.InputDoubleRegister(1),
                 i.InputDoubleRegister(0));
        __ sel_s(i.OutputDoubleRegister(), i.InputDoubleRegister(1),
                 i.InputDoubleRegister(0));
      } else {
        __ c_s(OLT, i.InputDoubleRegister(0), i.InputDoubleRegister(1));
        // Left operand is result, passthrough if false.
        __ movt_s(i.OutputDoubleRegister(), i.InputDoubleRegister(1));
      }
      break;
    }
    case kMips64Float32Min: {
      // (a < b) ? a : b
      if (kArchVariant == kMips64r6) {
        __ cmp_s(OLT, i.OutputDoubleRegister(), i.InputDoubleRegister(0),
                 i.InputDoubleRegister(1));
        __ sel_s(i.OutputDoubleRegister(), i.InputDoubleRegister(1),
                 i.InputDoubleRegister(0));
      } else {
        __ c_s(OLT, i.InputDoubleRegister(1), i.InputDoubleRegister(0));
        // Right operand is result, passthrough if false.
        __ movt_s(i.OutputDoubleRegister(), i.InputDoubleRegister(1));
      }
      break;
    }
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

    case kMips64Lbu:
      __ lbu(i.OutputRegister(), i.MemoryOperand());
      break;
    case kMips64Lb:
      __ lb(i.OutputRegister(), i.MemoryOperand());
      break;
    case kMips64Sb:
      __ sb(i.InputRegister(2), i.MemoryOperand());
      break;
    case kMips64Lhu:
      __ lhu(i.OutputRegister(), i.MemoryOperand());
      break;
    case kMips64Lh:
      __ lh(i.OutputRegister(), i.MemoryOperand());
      break;
    case kMips64Sh:
      __ sh(i.InputRegister(2), i.MemoryOperand());
      break;
    case kMips64Lw:
      __ lw(i.OutputRegister(), i.MemoryOperand());
      break;
    case kMips64Ld:
      __ ld(i.OutputRegister(), i.MemoryOperand());
      break;
    case kMips64Sw:
      __ sw(i.InputRegister(2), i.MemoryOperand());
      break;
    case kMips64Sd:
      __ sd(i.InputRegister(2), i.MemoryOperand());
      break;
    case kMips64Lwc1: {
      __ lwc1(i.OutputSingleRegister(), i.MemoryOperand());
      break;
    }
    case kMips64Swc1: {
      size_t index = 0;
      MemOperand operand = i.MemoryOperand(&index);
      __ swc1(i.InputSingleRegister(index), operand);
      break;
    }
    case kMips64Ldc1:
      __ ldc1(i.OutputDoubleRegister(), i.MemoryOperand());
      break;
    case kMips64Sdc1:
      __ sdc1(i.InputDoubleRegister(2), i.MemoryOperand());
      break;
    case kMips64Push:
      if (instr->InputAt(0)->IsDoubleRegister()) {
        __ sdc1(i.InputDoubleRegister(0), MemOperand(sp, -kDoubleSize));
        __ Subu(sp, sp, Operand(kDoubleSize));
        frame_access_state()->IncreaseSPDelta(kDoubleSize / kPointerSize);
      } else {
        __ Push(i.InputRegister(0));
        frame_access_state()->IncreaseSPDelta(1);
      }
      break;
    case kMips64StackClaim: {
      __ Dsubu(sp, sp, Operand(i.InputInt32(0)));
      frame_access_state()->IncreaseSPDelta(i.InputInt32(0) / kPointerSize);
      break;
    }
    case kMips64StoreToStackSlot: {
      if (instr->InputAt(0)->IsDoubleRegister()) {
        __ sdc1(i.InputDoubleRegister(0), MemOperand(sp, i.InputInt32(1)));
      } else {
        __ sd(i.InputRegister(0), MemOperand(sp, i.InputInt32(1)));
      }
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
    case kCheckedLoadWord64:
      ASSEMBLE_CHECKED_LOAD_INTEGER(ld);
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
    case kCheckedStoreWord64:
      ASSEMBLE_CHECKED_STORE_INTEGER(sd);
      break;
    case kCheckedStoreFloat32:
      ASSEMBLE_CHECKED_STORE_FLOAT(Single, swc1);
      break;
    case kCheckedStoreFloat64:
      ASSEMBLE_CHECKED_STORE_FLOAT(Double, sdc1);
      break;
  }
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


// Assembles branches after an instruction.
void CodeGenerator::AssembleArchBranch(Instruction* instr, BranchInfo* branch) {
  MipsOperandConverter i(this, instr);
  Label* tlabel = branch->true_label;
  Label* flabel = branch->false_label;
  Condition cc = kNoCondition;
  // MIPS does not have condition code flags, so compare and branch are
  // implemented differently than on the other arch's. The compare operations
  // emit mips psuedo-instructions, which are handled here by branch
  // instructions that do the actual comparison. Essential that the input
  // registers to compare pseudo-op are not modified before this branch op, as
  // they are tested here.

  if (instr->arch_opcode() == kMips64Tst) {
    cc = FlagsConditionToConditionTst(branch->condition);
    __ And(at, i.InputRegister(0), i.InputOperand(1));
    __ Branch(tlabel, cc, at, Operand(zero_reg));
  } else if (instr->arch_opcode() == kMips64Dadd ||
             instr->arch_opcode() == kMips64Dsub) {
    cc = FlagsConditionToConditionOvf(branch->condition);
    __ dsra32(kScratchReg, i.OutputRegister(), 0);
    __ sra(at, i.OutputRegister(), 31);
    __ Branch(tlabel, cc, at, Operand(kScratchReg));
  } else if (instr->arch_opcode() == kMips64DaddOvf) {
    switch (branch->condition) {
      case kOverflow:
        __ DaddBranchOvf(i.OutputRegister(), i.InputRegister(0),
                         i.InputOperand(1), tlabel, flabel);
        break;
      case kNotOverflow:
        __ DaddBranchOvf(i.OutputRegister(), i.InputRegister(0),
                         i.InputOperand(1), flabel, tlabel);
        break;
      default:
        UNSUPPORTED_COND(kMips64DaddOvf, branch->condition);
        break;
    }
  } else if (instr->arch_opcode() == kMips64DsubOvf) {
    switch (branch->condition) {
      case kOverflow:
        __ DsubBranchOvf(i.OutputRegister(), i.InputRegister(0),
                         i.InputOperand(1), tlabel, flabel);
        break;
      case kNotOverflow:
        __ DsubBranchOvf(i.OutputRegister(), i.InputRegister(0),
                         i.InputOperand(1), flabel, tlabel);
        break;
      default:
        UNSUPPORTED_COND(kMips64DsubOvf, branch->condition);
        break;
    }
  } else if (instr->arch_opcode() == kMips64Cmp) {
    cc = FlagsConditionToConditionCmp(branch->condition);
    __ Branch(tlabel, cc, i.InputRegister(0), i.InputOperand(1));
  } else if (instr->arch_opcode() == kMips64CmpS) {
    if (!convertCondition(branch->condition, cc)) {
      UNSUPPORTED_COND(kMips64CmpS, branch->condition);
    }
    FPURegister left = i.InputOrZeroSingleRegister(0);
    FPURegister right = i.InputOrZeroSingleRegister(1);
    if ((left.is(kDoubleRegZero) || right.is(kDoubleRegZero)) &&
        !__ IsDoubleZeroRegSet()) {
      __ Move(kDoubleRegZero, 0.0);
    }
    __ BranchF32(tlabel, nullptr, cc, left, right);
  } else if (instr->arch_opcode() == kMips64CmpD) {
    if (!convertCondition(branch->condition, cc)) {
      UNSUPPORTED_COND(kMips64CmpD, branch->condition);
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
  if (!branch->fallthru) __ Branch(flabel);  // no fallthru to flabel.
}


void CodeGenerator::AssembleArchJump(RpoNumber target) {
  if (!IsNextInAssemblyOrder(target)) __ Branch(GetLabel(target));
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
    __ And(kScratchReg, i.InputRegister(0), i.InputOperand(1));
    __ Sltu(result, zero_reg, kScratchReg);
    if (cc == eq) {
      // Sltu produces 0 for equality, invert the result.
      __ xori(result, result, 1);
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
             instr->arch_opcode() == kMips64DsubOvf) {
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
        Register select;
        if (instr->InputAt(1)->IsImmediate() && right.immediate() == 0) {
          // Pass left operand if right is zero.
          select = left;
        } else {
          __ Dsubu(kScratchReg, left, right);
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
  } else if (instr->arch_opcode() == kMips64CmpD ||
             instr->arch_opcode() == kMips64CmpS) {
    FPURegister left = i.InputOrZeroDoubleRegister(0);
    FPURegister right = i.InputOrZeroDoubleRegister(1);
    if ((left.is(kDoubleRegZero) || right.is(kDoubleRegZero)) &&
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
        DCHECK(instr->arch_opcode() == kMips64CmpS);
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
      } else {
        DCHECK(instr->arch_opcode() == kMips64CmpS);
        __ cmp(cc, W, kDoubleCompareReg, left, right);
      }
      __ dmfc1(result, kDoubleCompareReg);
      __ andi(result, result, 1);  // Cmp returns all 1's/0's, use only LSB.

      if (!predicate)  // Toggle result for not equal.
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


void CodeGenerator::AssembleDeoptimizerCall(
    int deoptimization_id, Deoptimizer::BailoutType bailout_type) {
  Address deopt_entry = Deoptimizer::GetDeoptimizationEntry(
      isolate(), deoptimization_id, bailout_type);
  __ Call(deopt_entry, RelocInfo::RUNTIME_ENTRY);
}


void CodeGenerator::AssemblePrologue() {
  CallDescriptor* descriptor = linkage()->GetIncomingDescriptor();
  if (descriptor->IsCFunctionCall()) {
    __ Push(ra, fp);
    __ mov(fp, sp);
  } else if (descriptor->IsJSFunctionCall()) {
    __ Prologue(this->info()->GeneratePreagedPrologue());
  } else if (frame()->needs_frame()) {
    __ StubPrologue();
  } else {
    frame()->SetElidedFrameSizeInSlots(0);
  }
  frame_access_state()->SetFrameAccessToDefault();

  int stack_shrink_slots = frame()->GetSpillSlotCount();
  if (info()->is_osr()) {
    // TurboFan OSR-compiled functions cannot be entered directly.
    __ Abort(kShouldNotDirectlyEnterOsrFunction);

    // Unoptimized code jumps directly to this entrypoint while the unoptimized
    // frame is still on the stack. Optimized code uses OSR values directly from
    // the unoptimized frame. Thus, all that needs to be done is to allocate the
    // remaining stack slots.
    if (FLAG_code_comments) __ RecordComment("-- OSR entrypoint --");
    osr_pc_offset_ = __ pc_offset();
    stack_shrink_slots -= OsrHelper(info()).UnoptimizedFrameSlots();
  }

  if (stack_shrink_slots > 0) {
    __ Dsubu(sp, sp, Operand(stack_shrink_slots * kPointerSize));
  }

  const RegList saves_fpu = descriptor->CalleeSavedFPRegisters();
  if (saves_fpu != 0) {
    // Save callee-saved FPU registers.
    __ MultiPushFPU(saves_fpu);
    int count = base::bits::CountPopulation32(saves_fpu);
    DCHECK(kNumCalleeSavedFPU == count);
    frame()->AllocateSavedCalleeRegisterSlots(count *
                                              (kDoubleSize / kPointerSize));
  }

  const RegList saves = descriptor->CalleeSavedRegisters();
  if (saves != 0) {
    // Save callee-saved registers.
    __ MultiPush(saves);
    // kNumCalleeSaved includes the fp register, but the fp register
    // is saved separately in TF.
    int count = base::bits::CountPopulation32(saves);
    DCHECK(kNumCalleeSaved == count + 1);
    frame()->AllocateSavedCalleeRegisterSlots(count);
  }
}


void CodeGenerator::AssembleReturn() {
  CallDescriptor* descriptor = linkage()->GetIncomingDescriptor();

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

  if (descriptor->IsCFunctionCall()) {
    __ mov(sp, fp);
    __ Pop(ra, fp);
  } else if (frame()->needs_frame()) {
    // Canonicalize JSFunction return sites for now.
    if (return_label_.is_bound()) {
      __ Branch(&return_label_);
      return;
    } else {
      __ bind(&return_label_);
      __ mov(sp, fp);
      __ Pop(ra, fp);
    }
  }
  int pop_count = static_cast<int>(descriptor->StackParameterCount());
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
      __ sd(src, g.ToMemOperand(destination));
    }
  } else if (source->IsStackSlot()) {
    DCHECK(destination->IsRegister() || destination->IsStackSlot());
    MemOperand src = g.ToMemOperand(source);
    if (destination->IsRegister()) {
      __ ld(g.ToRegister(destination), src);
    } else {
      Register temp = kScratchReg;
      __ ld(temp, src);
      __ sd(temp, g.ToMemOperand(destination));
    }
  } else if (source->IsConstant()) {
    Constant src = g.ToConstant(source);
    if (destination->IsRegister() || destination->IsStackSlot()) {
      Register dst =
          destination->IsRegister() ? g.ToRegister(destination) : kScratchReg;
      switch (src.type()) {
        case Constant::kInt32:
          __ li(dst, Operand(src.ToInt32()));
          break;
        case Constant::kFloat32:
          __ li(dst, isolate()->factory()->NewNumber(src.ToFloat32(), TENURED));
          break;
        case Constant::kInt64:
          __ li(dst, Operand(src.ToInt64()));
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
          int offset;
          if (IsMaterializableFromFrame(src_object, &offset)) {
            __ ld(dst, MemOperand(fp, offset));
          } else if (IsMaterializableFromRoot(src_object, &index)) {
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
      if (destination->IsStackSlot()) __ sd(dst, g.ToMemOperand(destination));
    } else if (src.type() == Constant::kFloat32) {
      if (destination->IsDoubleStackSlot()) {
        MemOperand dst = g.ToMemOperand(destination);
        __ li(at, Operand(bit_cast<int32_t>(src.ToFloat32())));
        __ sw(at, dst);
      } else {
        FloatRegister dst = g.ToSingleRegister(destination);
        __ Move(dst, src.ToFloat32());
      }
    } else {
      DCHECK_EQ(Constant::kFloat64, src.type());
      DoubleRegister dst = destination->IsDoubleRegister()
                               ? g.ToDoubleRegister(destination)
                               : kScratchDoubleReg;
      __ Move(dst, src.ToFloat64());
      if (destination->IsDoubleStackSlot()) {
        __ sdc1(dst, g.ToMemOperand(destination));
      }
    }
  } else if (source->IsDoubleRegister()) {
    FPURegister src = g.ToDoubleRegister(source);
    if (destination->IsDoubleRegister()) {
      FPURegister dst = g.ToDoubleRegister(destination);
      __ Move(dst, src);
    } else {
      DCHECK(destination->IsDoubleStackSlot());
      __ sdc1(src, g.ToMemOperand(destination));
    }
  } else if (source->IsDoubleStackSlot()) {
    DCHECK(destination->IsDoubleRegister() || destination->IsDoubleStackSlot());
    MemOperand src = g.ToMemOperand(source);
    if (destination->IsDoubleRegister()) {
      __ ldc1(g.ToDoubleRegister(destination), src);
    } else {
      FPURegister temp = kScratchDoubleReg;
      __ ldc1(temp, src);
      __ sdc1(temp, g.ToMemOperand(destination));
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
      __ ld(src, dst);
      __ sd(temp, dst);
    }
  } else if (source->IsStackSlot()) {
    DCHECK(destination->IsStackSlot());
    Register temp_0 = kScratchReg;
    Register temp_1 = kScratchReg2;
    MemOperand src = g.ToMemOperand(source);
    MemOperand dst = g.ToMemOperand(destination);
    __ ld(temp_0, src);
    __ ld(temp_1, dst);
    __ sd(temp_0, dst);
    __ sd(temp_1, src);
  } else if (source->IsDoubleRegister()) {
    FPURegister temp = kScratchDoubleReg;
    FPURegister src = g.ToDoubleRegister(source);
    if (destination->IsDoubleRegister()) {
      FPURegister dst = g.ToDoubleRegister(destination);
      __ Move(temp, src);
      __ Move(src, dst);
      __ Move(dst, temp);
    } else {
      DCHECK(destination->IsDoubleStackSlot());
      MemOperand dst = g.ToMemOperand(destination);
      __ Move(temp, src);
      __ ldc1(src, dst);
      __ sdc1(temp, dst);
    }
  } else if (source->IsDoubleStackSlot()) {
    DCHECK(destination->IsDoubleStackSlot());
    Register temp_0 = kScratchReg;
    FPURegister temp_1 = kScratchDoubleReg;
    MemOperand src0 = g.ToMemOperand(source);
    MemOperand src1(src0.rm(), src0.offset() + kIntSize);
    MemOperand dst0 = g.ToMemOperand(destination);
    MemOperand dst1(dst0.rm(), dst0.offset() + kIntSize);
    __ ldc1(temp_1, dst0);  // Save destination in temp_1.
    __ lw(temp_0, src0);    // Then use temp_0 to copy source to destination.
    __ sw(temp_0, dst0);
    __ lw(temp_0, src1);
    __ sw(temp_0, dst1);
    __ sdc1(temp_1, src0);
  } else {
    // No other combinations are possible.
    UNREACHABLE();
  }
}


void CodeGenerator::AssembleJumpTable(Label** targets, size_t target_count) {
  // On 64-bit MIPS we emit the jump tables inline.
  UNREACHABLE();
}


void CodeGenerator::AddNopForSmiCodeInlining() {
  // Unused on 32-bit ARM. Still exists on 64-bit arm.
  // TODO(plind): Unclear when this is called now. Understand, fix if needed.
  __ nop();  // Maybe PROPERTY_ACCESS_INLINED?
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
