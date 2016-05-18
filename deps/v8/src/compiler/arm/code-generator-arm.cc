// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/code-generator.h"

#include "src/arm/macro-assembler-arm.h"
#include "src/ast/scopes.h"
#include "src/compiler/code-generator-impl.h"
#include "src/compiler/gap-resolver.h"
#include "src/compiler/node-matchers.h"
#include "src/compiler/osr.h"

namespace v8 {
namespace internal {
namespace compiler {

#define __ masm()->


#define kScratchReg r9


// Adds Arm-specific methods to convert InstructionOperands.
class ArmOperandConverter final : public InstructionOperandConverter {
 public:
  ArmOperandConverter(CodeGenerator* gen, Instruction* instr)
      : InstructionOperandConverter(gen, instr) {}

  SwVfpRegister OutputFloat32Register(size_t index = 0) {
    return ToFloat32Register(instr_->OutputAt(index));
  }

  SwVfpRegister InputFloat32Register(size_t index) {
    return ToFloat32Register(instr_->InputAt(index));
  }

  SwVfpRegister ToFloat32Register(InstructionOperand* op) {
    return ToFloat64Register(op).low();
  }

  LowDwVfpRegister OutputFloat64Register(size_t index = 0) {
    return ToFloat64Register(instr_->OutputAt(index));
  }

  LowDwVfpRegister InputFloat64Register(size_t index) {
    return ToFloat64Register(instr_->InputAt(index));
  }

  LowDwVfpRegister ToFloat64Register(InstructionOperand* op) {
    return LowDwVfpRegister::from_code(ToDoubleRegister(op).code());
  }

  SBit OutputSBit() const {
    switch (instr_->flags_mode()) {
      case kFlags_branch:
      case kFlags_set:
        return SetCC;
      case kFlags_none:
        return LeaveCC;
    }
    UNREACHABLE();
    return LeaveCC;
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
      case Constant::kRpoNumber:
        break;
    }
    UNREACHABLE();
    return Operand::Zero();
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
    return Operand::Zero();
  }

  MemOperand InputOffset(size_t* first_index) {
    const size_t index = *first_index;
    switch (AddressingModeField::decode(instr_->opcode())) {
      case kMode_None:
      case kMode_Operand2_I:
      case kMode_Operand2_R:
      case kMode_Operand2_R_ASR_I:
      case kMode_Operand2_R_ASR_R:
      case kMode_Operand2_R_LSL_I:
      case kMode_Operand2_R_LSL_R:
      case kMode_Operand2_R_LSR_I:
      case kMode_Operand2_R_LSR_R:
      case kMode_Operand2_R_ROR_I:
      case kMode_Operand2_R_ROR_R:
        break;
      case kMode_Offset_RI:
        *first_index += 2;
        return MemOperand(InputRegister(index + 0), InputInt32(index + 1));
      case kMode_Offset_RR:
        *first_index += 2;
        return MemOperand(InputRegister(index + 0), InputRegister(index + 1));
    }
    UNREACHABLE();
    return MemOperand(r0);
  }

  MemOperand InputOffset(size_t first_index = 0) {
    return InputOffset(&first_index);
  }

  MemOperand ToMemOperand(InstructionOperand* op) const {
    DCHECK_NOT_NULL(op);
    DCHECK(op->IsStackSlot() || op->IsDoubleStackSlot());
    FrameOffset offset = frame_access_state()->GetFrameOffset(
        AllocatedOperand::cast(op)->index());
    return MemOperand(offset.from_stack_pointer() ? sp : fp, offset.offset());
  }
};


namespace {

class OutOfLineLoadFloat32 final : public OutOfLineCode {
 public:
  OutOfLineLoadFloat32(CodeGenerator* gen, SwVfpRegister result)
      : OutOfLineCode(gen), result_(result) {}

  void Generate() final {
    __ vmov(result_, std::numeric_limits<float>::quiet_NaN());
  }

 private:
  SwVfpRegister const result_;
};


class OutOfLineLoadFloat64 final : public OutOfLineCode {
 public:
  OutOfLineLoadFloat64(CodeGenerator* gen, DwVfpRegister result)
      : OutOfLineCode(gen), result_(result) {}

  void Generate() final {
    __ vmov(result_, std::numeric_limits<double>::quiet_NaN(), kScratchReg);
  }

 private:
  DwVfpRegister const result_;
};


class OutOfLineLoadInteger final : public OutOfLineCode {
 public:
  OutOfLineLoadInteger(CodeGenerator* gen, Register result)
      : OutOfLineCode(gen), result_(result) {}

  void Generate() final { __ mov(result_, Operand::Zero()); }

 private:
  Register const result_;
};


class OutOfLineRecordWrite final : public OutOfLineCode {
 public:
  OutOfLineRecordWrite(CodeGenerator* gen, Register object, Register index,
                       Register value, Register scratch0, Register scratch1,
                       RecordWriteMode mode)
      : OutOfLineCode(gen),
        object_(object),
        index_(index),
        index_immediate_(0),
        value_(value),
        scratch0_(scratch0),
        scratch1_(scratch1),
        mode_(mode) {}

  OutOfLineRecordWrite(CodeGenerator* gen, Register object, int32_t index,
                       Register value, Register scratch0, Register scratch1,
                       RecordWriteMode mode)
      : OutOfLineCode(gen),
        object_(object),
        index_(no_reg),
        index_immediate_(index),
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
      // We need to save and restore lr if the frame was elided.
      __ Push(lr);
    }
    RecordWriteStub stub(isolate(), object_, scratch0_, scratch1_,
                         remembered_set_action, save_fp_mode);
    if (index_.is(no_reg)) {
      __ add(scratch1_, object_, Operand(index_immediate_));
    } else {
      DCHECK_EQ(0, index_immediate_);
      __ add(scratch1_, object_, Operand(index_));
    }
    __ CallStub(&stub);
    if (!frame()->needs_frame()) {
      __ Pop(lr);
    }
  }

 private:
  Register const object_;
  Register const index_;
  int32_t const index_immediate_;  // Valid if index_.is(no_reg).
  Register const value_;
  Register const scratch0_;
  Register const scratch1_;
  RecordWriteMode const mode_;
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
    default:
      break;
  }
  UNREACHABLE();
  return kNoCondition;
}

}  // namespace


#define ASSEMBLE_CHECKED_LOAD_FLOAT(width)                           \
  do {                                                               \
    auto result = i.OutputFloat##width##Register();                  \
    auto offset = i.InputRegister(0);                                \
    if (instr->InputAt(1)->IsRegister()) {                           \
      __ cmp(offset, i.InputRegister(1));                            \
    } else {                                                         \
      __ cmp(offset, i.InputImmediate(1));                           \
    }                                                                \
    auto ool = new (zone()) OutOfLineLoadFloat##width(this, result); \
    __ b(hs, ool->entry());                                          \
    __ vldr(result, i.InputOffset(2));                               \
    __ bind(ool->exit());                                            \
    DCHECK_EQ(LeaveCC, i.OutputSBit());                              \
  } while (0)


#define ASSEMBLE_CHECKED_LOAD_INTEGER(asm_instr)                \
  do {                                                          \
    auto result = i.OutputRegister();                           \
    auto offset = i.InputRegister(0);                           \
    if (instr->InputAt(1)->IsRegister()) {                      \
      __ cmp(offset, i.InputRegister(1));                       \
    } else {                                                    \
      __ cmp(offset, i.InputImmediate(1));                      \
    }                                                           \
    auto ool = new (zone()) OutOfLineLoadInteger(this, result); \
    __ b(hs, ool->entry());                                     \
    __ asm_instr(result, i.InputOffset(2));                     \
    __ bind(ool->exit());                                       \
    DCHECK_EQ(LeaveCC, i.OutputSBit());                         \
  } while (0)


#define ASSEMBLE_CHECKED_STORE_FLOAT(width)        \
  do {                                             \
    auto offset = i.InputRegister(0);              \
    if (instr->InputAt(1)->IsRegister()) {         \
      __ cmp(offset, i.InputRegister(1));          \
    } else {                                       \
      __ cmp(offset, i.InputImmediate(1));         \
    }                                              \
    auto value = i.InputFloat##width##Register(2); \
    __ vstr(value, i.InputOffset(3), lo);          \
    DCHECK_EQ(LeaveCC, i.OutputSBit());            \
  } while (0)


#define ASSEMBLE_CHECKED_STORE_INTEGER(asm_instr) \
  do {                                            \
    auto offset = i.InputRegister(0);             \
    if (instr->InputAt(1)->IsRegister()) {        \
      __ cmp(offset, i.InputRegister(1));         \
    } else {                                      \
      __ cmp(offset, i.InputImmediate(1));        \
    }                                             \
    auto value = i.InputRegister(2);              \
    __ asm_instr(value, i.InputOffset(3), lo);    \
    DCHECK_EQ(LeaveCC, i.OutputSBit());           \
  } while (0)


void CodeGenerator::AssembleDeconstructActivationRecord(int stack_param_delta) {
  int sp_slot_delta = TailCallFrameStackSlotDelta(stack_param_delta);
  if (sp_slot_delta > 0) {
    __ add(sp, sp, Operand(sp_slot_delta * kPointerSize));
  }
  frame_access_state()->SetFrameAccessToDefault();
}


void CodeGenerator::AssemblePrepareTailCall(int stack_param_delta) {
  int sp_slot_delta = TailCallFrameStackSlotDelta(stack_param_delta);
  if (sp_slot_delta < 0) {
    __ sub(sp, sp, Operand(-sp_slot_delta * kPointerSize));
    frame_access_state()->IncreaseSPDelta(-sp_slot_delta);
  }
  if (frame()->needs_frame()) {
    if (FLAG_enable_embedded_constant_pool) {
      __ ldr(cp, MemOperand(fp, StandardFrameConstants::kConstantPoolOffset));
    }
    __ ldr(lr, MemOperand(fp, StandardFrameConstants::kCallerPCOffset));
    __ ldr(fp, MemOperand(fp, StandardFrameConstants::kCallerFPOffset));
  }
  frame_access_state()->SetFrameAccessToSP();
}


// Assembles an instruction after register allocation, producing machine code.
void CodeGenerator::AssembleArchInstruction(Instruction* instr) {
  ArmOperandConverter i(this, instr);

  masm()->MaybeCheckConstPool();

  switch (ArchOpcodeField::decode(instr->opcode())) {
    case kArchCallCodeObject: {
      EnsureSpaceForLazyDeopt();
      if (instr->InputAt(0)->IsImmediate()) {
        __ Call(Handle<Code>::cast(i.InputHeapObject(0)),
                RelocInfo::CODE_TARGET);
      } else {
        __ add(ip, i.InputRegister(0),
               Operand(Code::kHeaderSize - kHeapObjectTag));
        __ Call(ip);
      }
      RecordCallPosition(instr);
      DCHECK_EQ(LeaveCC, i.OutputSBit());
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
        __ add(ip, i.InputRegister(0),
               Operand(Code::kHeaderSize - kHeapObjectTag));
        __ Jump(ip);
      }
      DCHECK_EQ(LeaveCC, i.OutputSBit());
      frame_access_state()->ClearSPDelta();
      break;
    }
    case kArchCallJSFunction: {
      EnsureSpaceForLazyDeopt();
      Register func = i.InputRegister(0);
      if (FLAG_debug_code) {
        // Check the function's context matches the context argument.
        __ ldr(kScratchReg, FieldMemOperand(func, JSFunction::kContextOffset));
        __ cmp(cp, kScratchReg);
        __ Assert(eq, kWrongFunctionContext);
      }
      __ ldr(ip, FieldMemOperand(func, JSFunction::kCodeEntryOffset));
      __ Call(ip);
      RecordCallPosition(instr);
      DCHECK_EQ(LeaveCC, i.OutputSBit());
      frame_access_state()->ClearSPDelta();
      break;
    }
    case kArchTailCallJSFunction: {
      Register func = i.InputRegister(0);
      if (FLAG_debug_code) {
        // Check the function's context matches the context argument.
        __ ldr(kScratchReg, FieldMemOperand(func, JSFunction::kContextOffset));
        __ cmp(cp, kScratchReg);
        __ Assert(eq, kWrongFunctionContext);
      }
      int stack_param_delta = i.InputInt32(instr->InputCount() - 1);
      AssembleDeconstructActivationRecord(stack_param_delta);
      __ ldr(ip, FieldMemOperand(func, JSFunction::kCodeEntryOffset));
      __ Jump(ip);
      DCHECK_EQ(LeaveCC, i.OutputSBit());
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
      DCHECK_EQ(LeaveCC, i.OutputSBit());
      break;
    case kArchLookupSwitch:
      AssembleArchLookupSwitch(instr);
      DCHECK_EQ(LeaveCC, i.OutputSBit());
      break;
    case kArchTableSwitch:
      AssembleArchTableSwitch(instr);
      DCHECK_EQ(LeaveCC, i.OutputSBit());
      break;
    case kArchNop:
    case kArchThrowTerminator:
      // don't emit code for nops.
      DCHECK_EQ(LeaveCC, i.OutputSBit());
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
      if (frame_access_state()->frame()->needs_frame()) {
        __ ldr(i.OutputRegister(), MemOperand(fp, 0));
      } else {
        __ mov(i.OutputRegister(), fp);
      }
      break;
    case kArchTruncateDoubleToI:
      __ TruncateDoubleToI(i.OutputRegister(), i.InputFloat64Register(0));
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
        ool = new (zone()) OutOfLineRecordWrite(this, object, index, value,
                                                scratch0, scratch1, mode);
        __ str(value, MemOperand(object, index));
      } else {
        DCHECK_EQ(kMode_Offset_RR, addressing_mode);
        Register index(i.InputRegister(1));
        ool = new (zone()) OutOfLineRecordWrite(this, object, index, value,
                                                scratch0, scratch1, mode);
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
      Register base;
      if (offset.from_stack_pointer()) {
        base = sp;
      } else {
        base = fp;
      }
      __ add(i.OutputRegister(0), base, Operand(offset.offset()));
      break;
    }
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
      CpuFeatureScope scope(masm(), MLS);
      __ mls(i.OutputRegister(), i.InputRegister(0), i.InputRegister(1),
             i.InputRegister(2));
      DCHECK_EQ(LeaveCC, i.OutputSBit());
      break;
    }
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
      CpuFeatureScope scope(masm(), SUDIV);
      __ sdiv(i.OutputRegister(), i.InputRegister(0), i.InputRegister(1));
      DCHECK_EQ(LeaveCC, i.OutputSBit());
      break;
    }
    case kArmUdiv: {
      CpuFeatureScope scope(masm(), SUDIV);
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
      CpuFeatureScope scope(masm(), ARMv7);
      __ bfc(i.OutputRegister(), i.InputInt8(1), i.InputInt8(2));
      DCHECK_EQ(LeaveCC, i.OutputSBit());
      break;
    }
    case kArmUbfx: {
      CpuFeatureScope scope(masm(), ARMv7);
      __ ubfx(i.OutputRegister(), i.InputRegister(0), i.InputInt8(1),
              i.InputInt8(2));
      DCHECK_EQ(LeaveCC, i.OutputSBit());
      break;
    }
    case kArmSbfx: {
      CpuFeatureScope scope(masm(), ARMv7);
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
      CpuFeatureScope scope(masm(), ARMv7);
      __ rbit(i.OutputRegister(), i.InputRegister(0));
      DCHECK_EQ(LeaveCC, i.OutputSBit());
      break;
    }
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
    case kArmVcmpF32:
      if (instr->InputAt(1)->IsDoubleRegister()) {
        __ VFPCompareAndSetFlags(i.InputFloat32Register(0),
                                 i.InputFloat32Register(1));
      } else {
        DCHECK(instr->InputAt(1)->IsImmediate());
        // 0.0 is the only immediate supported by vcmp instructions.
        DCHECK(i.InputFloat32(1) == 0.0f);
        __ VFPCompareAndSetFlags(i.InputFloat32Register(0), i.InputFloat32(1));
      }
      DCHECK_EQ(SetCC, i.OutputSBit());
      break;
    case kArmVaddF32:
      __ vadd(i.OutputFloat32Register(), i.InputFloat32Register(0),
              i.InputFloat32Register(1));
      DCHECK_EQ(LeaveCC, i.OutputSBit());
      break;
    case kArmVsubF32:
      __ vsub(i.OutputFloat32Register(), i.InputFloat32Register(0),
              i.InputFloat32Register(1));
      DCHECK_EQ(LeaveCC, i.OutputSBit());
      break;
    case kArmVmulF32:
      __ vmul(i.OutputFloat32Register(), i.InputFloat32Register(0),
              i.InputFloat32Register(1));
      DCHECK_EQ(LeaveCC, i.OutputSBit());
      break;
    case kArmVmlaF32:
      __ vmla(i.OutputFloat32Register(), i.InputFloat32Register(1),
              i.InputFloat32Register(2));
      DCHECK_EQ(LeaveCC, i.OutputSBit());
      break;
    case kArmVmlsF32:
      __ vmls(i.OutputFloat32Register(), i.InputFloat32Register(1),
              i.InputFloat32Register(2));
      DCHECK_EQ(LeaveCC, i.OutputSBit());
      break;
    case kArmVdivF32:
      __ vdiv(i.OutputFloat32Register(), i.InputFloat32Register(0),
              i.InputFloat32Register(1));
      DCHECK_EQ(LeaveCC, i.OutputSBit());
      break;
    case kArmVsqrtF32:
      __ vsqrt(i.OutputFloat32Register(), i.InputFloat32Register(0));
      break;
    case kArmVabsF32:
      __ vabs(i.OutputFloat32Register(), i.InputFloat32Register(0));
      break;
    case kArmVnegF32:
      __ vneg(i.OutputFloat32Register(), i.InputFloat32Register(0));
      break;
    case kArmVcmpF64:
      if (instr->InputAt(1)->IsDoubleRegister()) {
        __ VFPCompareAndSetFlags(i.InputFloat64Register(0),
                                 i.InputFloat64Register(1));
      } else {
        DCHECK(instr->InputAt(1)->IsImmediate());
        // 0.0 is the only immediate supported by vcmp instructions.
        DCHECK(i.InputDouble(1) == 0.0);
        __ VFPCompareAndSetFlags(i.InputFloat64Register(0), i.InputDouble(1));
      }
      DCHECK_EQ(SetCC, i.OutputSBit());
      break;
    case kArmVaddF64:
      __ vadd(i.OutputFloat64Register(), i.InputFloat64Register(0),
              i.InputFloat64Register(1));
      DCHECK_EQ(LeaveCC, i.OutputSBit());
      break;
    case kArmVsubF64:
      __ vsub(i.OutputFloat64Register(), i.InputFloat64Register(0),
              i.InputFloat64Register(1));
      DCHECK_EQ(LeaveCC, i.OutputSBit());
      break;
    case kArmVmulF64:
      __ vmul(i.OutputFloat64Register(), i.InputFloat64Register(0),
              i.InputFloat64Register(1));
      DCHECK_EQ(LeaveCC, i.OutputSBit());
      break;
    case kArmVmlaF64:
      __ vmla(i.OutputFloat64Register(), i.InputFloat64Register(1),
              i.InputFloat64Register(2));
      DCHECK_EQ(LeaveCC, i.OutputSBit());
      break;
    case kArmVmlsF64:
      __ vmls(i.OutputFloat64Register(), i.InputFloat64Register(1),
              i.InputFloat64Register(2));
      DCHECK_EQ(LeaveCC, i.OutputSBit());
      break;
    case kArmVdivF64:
      __ vdiv(i.OutputFloat64Register(), i.InputFloat64Register(0),
              i.InputFloat64Register(1));
      DCHECK_EQ(LeaveCC, i.OutputSBit());
      break;
    case kArmVmodF64: {
      // TODO(bmeurer): We should really get rid of this special instruction,
      // and generate a CallAddress instruction instead.
      FrameScope scope(masm(), StackFrame::MANUAL);
      __ PrepareCallCFunction(0, 2, kScratchReg);
      __ MovToFloatParameters(i.InputFloat64Register(0),
                              i.InputFloat64Register(1));
      __ CallCFunction(ExternalReference::mod_two_doubles_operation(isolate()),
                       0, 2);
      // Move the result in the double result register.
      __ MovFromFloatResult(i.OutputFloat64Register());
      DCHECK_EQ(LeaveCC, i.OutputSBit());
      break;
    }
    case kArmVsqrtF64:
      __ vsqrt(i.OutputFloat64Register(), i.InputFloat64Register(0));
      break;
    case kArmVabsF64:
      __ vabs(i.OutputFloat64Register(), i.InputFloat64Register(0));
      break;
    case kArmVnegF64:
      __ vneg(i.OutputFloat64Register(), i.InputFloat64Register(0));
      break;
    case kArmVrintmF32:
      __ vrintm(i.OutputFloat32Register(), i.InputFloat32Register(0));
      break;
    case kArmVrintmF64:
      __ vrintm(i.OutputFloat64Register(), i.InputFloat64Register(0));
      break;
    case kArmVrintpF32:
      __ vrintp(i.OutputFloat32Register(), i.InputFloat32Register(0));
      break;
    case kArmVrintpF64:
      __ vrintp(i.OutputFloat64Register(), i.InputFloat64Register(0));
      break;
    case kArmVrintzF32:
      __ vrintz(i.OutputFloat32Register(), i.InputFloat32Register(0));
      break;
    case kArmVrintzF64:
      __ vrintz(i.OutputFloat64Register(), i.InputFloat64Register(0));
      break;
    case kArmVrintaF64:
      __ vrinta(i.OutputFloat64Register(), i.InputFloat64Register(0));
      break;
    case kArmVrintnF32:
      __ vrintn(i.OutputFloat32Register(), i.InputFloat32Register(0));
      break;
    case kArmVrintnF64:
      __ vrintn(i.OutputFloat64Register(), i.InputFloat64Register(0));
      break;
    case kArmVcvtF32F64: {
      __ vcvt_f32_f64(i.OutputFloat32Register(), i.InputFloat64Register(0));
      DCHECK_EQ(LeaveCC, i.OutputSBit());
      break;
    }
    case kArmVcvtF64F32: {
      __ vcvt_f64_f32(i.OutputFloat64Register(), i.InputFloat32Register(0));
      DCHECK_EQ(LeaveCC, i.OutputSBit());
      break;
    }
    case kArmVcvtF32S32: {
      SwVfpRegister scratch = kScratchDoubleReg.low();
      __ vmov(scratch, i.InputRegister(0));
      __ vcvt_f32_s32(i.OutputFloat32Register(), scratch);
      DCHECK_EQ(LeaveCC, i.OutputSBit());
      break;
    }
    case kArmVcvtF32U32: {
      SwVfpRegister scratch = kScratchDoubleReg.low();
      __ vmov(scratch, i.InputRegister(0));
      __ vcvt_f32_u32(i.OutputFloat32Register(), scratch);
      DCHECK_EQ(LeaveCC, i.OutputSBit());
      break;
    }
    case kArmVcvtF64S32: {
      SwVfpRegister scratch = kScratchDoubleReg.low();
      __ vmov(scratch, i.InputRegister(0));
      __ vcvt_f64_s32(i.OutputFloat64Register(), scratch);
      DCHECK_EQ(LeaveCC, i.OutputSBit());
      break;
    }
    case kArmVcvtF64U32: {
      SwVfpRegister scratch = kScratchDoubleReg.low();
      __ vmov(scratch, i.InputRegister(0));
      __ vcvt_f64_u32(i.OutputFloat64Register(), scratch);
      DCHECK_EQ(LeaveCC, i.OutputSBit());
      break;
    }
    case kArmVcvtS32F32: {
      SwVfpRegister scratch = kScratchDoubleReg.low();
      __ vcvt_s32_f32(scratch, i.InputFloat32Register(0));
      __ vmov(i.OutputRegister(), scratch);
      DCHECK_EQ(LeaveCC, i.OutputSBit());
      break;
    }
    case kArmVcvtU32F32: {
      SwVfpRegister scratch = kScratchDoubleReg.low();
      __ vcvt_u32_f32(scratch, i.InputFloat32Register(0));
      __ vmov(i.OutputRegister(), scratch);
      DCHECK_EQ(LeaveCC, i.OutputSBit());
      break;
    }
    case kArmVcvtS32F64: {
      SwVfpRegister scratch = kScratchDoubleReg.low();
      __ vcvt_s32_f64(scratch, i.InputFloat64Register(0));
      __ vmov(i.OutputRegister(), scratch);
      DCHECK_EQ(LeaveCC, i.OutputSBit());
      break;
    }
    case kArmVcvtU32F64: {
      SwVfpRegister scratch = kScratchDoubleReg.low();
      __ vcvt_u32_f64(scratch, i.InputFloat64Register(0));
      __ vmov(i.OutputRegister(), scratch);
      DCHECK_EQ(LeaveCC, i.OutputSBit());
      break;
    }
    case kArmVmovLowU32F64:
      __ VmovLow(i.OutputRegister(), i.InputFloat64Register(0));
      DCHECK_EQ(LeaveCC, i.OutputSBit());
      break;
    case kArmVmovLowF64U32:
      __ VmovLow(i.OutputFloat64Register(), i.InputRegister(1));
      DCHECK_EQ(LeaveCC, i.OutputSBit());
      break;
    case kArmVmovHighU32F64:
      __ VmovHigh(i.OutputRegister(), i.InputFloat64Register(0));
      DCHECK_EQ(LeaveCC, i.OutputSBit());
      break;
    case kArmVmovHighF64U32:
      __ VmovHigh(i.OutputFloat64Register(), i.InputRegister(1));
      DCHECK_EQ(LeaveCC, i.OutputSBit());
      break;
    case kArmVmovF64U32U32:
      __ vmov(i.OutputFloat64Register(), i.InputRegister(0),
              i.InputRegister(1));
      DCHECK_EQ(LeaveCC, i.OutputSBit());
      break;
    case kArmLdrb:
      __ ldrb(i.OutputRegister(), i.InputOffset());
      DCHECK_EQ(LeaveCC, i.OutputSBit());
      break;
    case kArmLdrsb:
      __ ldrsb(i.OutputRegister(), i.InputOffset());
      DCHECK_EQ(LeaveCC, i.OutputSBit());
      break;
    case kArmStrb: {
      size_t index = 0;
      MemOperand operand = i.InputOffset(&index);
      __ strb(i.InputRegister(index), operand);
      DCHECK_EQ(LeaveCC, i.OutputSBit());
      break;
    }
    case kArmLdrh:
      __ ldrh(i.OutputRegister(), i.InputOffset());
      break;
    case kArmLdrsh:
      __ ldrsh(i.OutputRegister(), i.InputOffset());
      break;
    case kArmStrh: {
      size_t index = 0;
      MemOperand operand = i.InputOffset(&index);
      __ strh(i.InputRegister(index), operand);
      DCHECK_EQ(LeaveCC, i.OutputSBit());
      break;
    }
    case kArmLdr:
      __ ldr(i.OutputRegister(), i.InputOffset());
      break;
    case kArmStr: {
      size_t index = 0;
      MemOperand operand = i.InputOffset(&index);
      __ str(i.InputRegister(index), operand);
      DCHECK_EQ(LeaveCC, i.OutputSBit());
      break;
    }
    case kArmVldrF32: {
      __ vldr(i.OutputFloat32Register(), i.InputOffset());
      DCHECK_EQ(LeaveCC, i.OutputSBit());
      break;
    }
    case kArmVstrF32: {
      size_t index = 0;
      MemOperand operand = i.InputOffset(&index);
      __ vstr(i.InputFloat32Register(index), operand);
      DCHECK_EQ(LeaveCC, i.OutputSBit());
      break;
    }
    case kArmVldrF64:
      __ vldr(i.OutputFloat64Register(), i.InputOffset());
      DCHECK_EQ(LeaveCC, i.OutputSBit());
      break;
    case kArmVstrF64: {
      size_t index = 0;
      MemOperand operand = i.InputOffset(&index);
      __ vstr(i.InputFloat64Register(index), operand);
      DCHECK_EQ(LeaveCC, i.OutputSBit());
      break;
    }
    case kArmPush:
      if (instr->InputAt(0)->IsDoubleRegister()) {
        __ vpush(i.InputDoubleRegister(0));
        frame_access_state()->IncreaseSPDelta(kDoubleSize / kPointerSize);
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
    case kCheckedLoadInt8:
      ASSEMBLE_CHECKED_LOAD_INTEGER(ldrsb);
      break;
    case kCheckedLoadUint8:
      ASSEMBLE_CHECKED_LOAD_INTEGER(ldrb);
      break;
    case kCheckedLoadInt16:
      ASSEMBLE_CHECKED_LOAD_INTEGER(ldrsh);
      break;
    case kCheckedLoadUint16:
      ASSEMBLE_CHECKED_LOAD_INTEGER(ldrh);
      break;
    case kCheckedLoadWord32:
      ASSEMBLE_CHECKED_LOAD_INTEGER(ldr);
      break;
    case kCheckedLoadFloat32:
      ASSEMBLE_CHECKED_LOAD_FLOAT(32);
      break;
    case kCheckedLoadFloat64:
      ASSEMBLE_CHECKED_LOAD_FLOAT(64);
      break;
    case kCheckedStoreWord8:
      ASSEMBLE_CHECKED_STORE_INTEGER(strb);
      break;
    case kCheckedStoreWord16:
      ASSEMBLE_CHECKED_STORE_INTEGER(strh);
      break;
    case kCheckedStoreWord32:
      ASSEMBLE_CHECKED_STORE_INTEGER(str);
      break;
    case kCheckedStoreFloat32:
      ASSEMBLE_CHECKED_STORE_FLOAT(32);
      break;
    case kCheckedStoreFloat64:
      ASSEMBLE_CHECKED_STORE_FLOAT(64);
      break;
    case kCheckedLoadWord64:
    case kCheckedStoreWord64:
      UNREACHABLE();  // currently unsupported checked int64 load/store.
      break;
  }
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


void CodeGenerator::AssembleArchJump(RpoNumber target) {
  if (!IsNextInAssemblyOrder(target)) __ b(GetLabel(target));
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


void CodeGenerator::AssembleDeoptimizerCall(
    int deoptimization_id, Deoptimizer::BailoutType bailout_type) {
  Address deopt_entry = Deoptimizer::GetDeoptimizationEntry(
      isolate(), deoptimization_id, bailout_type);
  __ Call(deopt_entry, RelocInfo::RUNTIME_ENTRY);
}


void CodeGenerator::AssemblePrologue() {
  CallDescriptor* descriptor = linkage()->GetIncomingDescriptor();
  if (descriptor->IsCFunctionCall()) {
    if (FLAG_enable_embedded_constant_pool) {
      __ Push(lr, fp, pp);
      // Adjust FP to point to saved FP.
      __ sub(fp, sp, Operand(StandardFrameConstants::kConstantPoolOffset));
    } else {
      __ Push(lr, fp);
      __ mov(fp, sp);
    }
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

  const RegList saves_fp = descriptor->CalleeSavedFPRegisters();
  if (saves_fp != 0) {
    stack_shrink_slots += frame()->AlignSavedCalleeRegisterSlots();
  }
  if (stack_shrink_slots > 0) {
    __ sub(sp, sp, Operand(stack_shrink_slots * kPointerSize));
  }

  if (saves_fp != 0) {
    // Save callee-saved FP registers.
    STATIC_ASSERT(DwVfpRegister::kMaxNumRegisters == 32);
    uint32_t last = base::bits::CountLeadingZeros32(saves_fp) - 1;
    uint32_t first = base::bits::CountTrailingZeros32(saves_fp);
    DCHECK_EQ((last - first + 1), base::bits::CountPopulation32(saves_fp));
    __ vstm(db_w, sp, DwVfpRegister::from_code(first),
            DwVfpRegister::from_code(last));
    frame()->AllocateSavedCalleeRegisterSlots((last - first + 1) *
                                              (kDoubleSize / kPointerSize));
  }
  const RegList saves = FLAG_enable_embedded_constant_pool
                            ? (descriptor->CalleeSavedRegisters() & ~pp.bit())
                            : descriptor->CalleeSavedRegisters();
  if (saves != 0) {
    // Save callee-saved registers.
    __ stm(db_w, sp, saves);
    frame()->AllocateSavedCalleeRegisterSlots(
        base::bits::CountPopulation32(saves));
  }
}


void CodeGenerator::AssembleReturn() {
  CallDescriptor* descriptor = linkage()->GetIncomingDescriptor();
  int pop_count = static_cast<int>(descriptor->StackParameterCount());

  // Restore registers.
  const RegList saves = FLAG_enable_embedded_constant_pool
                            ? (descriptor->CalleeSavedRegisters() & ~pp.bit())
                            : descriptor->CalleeSavedRegisters();
  if (saves != 0) {
    __ ldm(ia_w, sp, saves);
  }

  // Restore FP registers.
  const RegList saves_fp = descriptor->CalleeSavedFPRegisters();
  if (saves_fp != 0) {
    STATIC_ASSERT(DwVfpRegister::kMaxNumRegisters == 32);
    uint32_t last = base::bits::CountLeadingZeros32(saves_fp) - 1;
    uint32_t first = base::bits::CountTrailingZeros32(saves_fp);
    __ vldm(ia_w, sp, DwVfpRegister::from_code(first),
            DwVfpRegister::from_code(last));
  }

  if (descriptor->IsCFunctionCall()) {
    __ LeaveFrame(StackFrame::MANUAL);
  } else if (frame()->needs_frame()) {
    // Canonicalize JSFunction return sites for now.
    if (return_label_.is_bound()) {
      __ b(&return_label_);
      return;
    } else {
      __ bind(&return_label_);
      __ LeaveFrame(StackFrame::MANUAL);
    }
  }
  __ Ret(pop_count);
}


void CodeGenerator::AssembleMove(InstructionOperand* source,
                                 InstructionOperand* destination) {
  ArmOperandConverter g(this, nullptr);
  // Dispatch on the source and destination operand kinds.  Not all
  // combinations are possible.
  if (source->IsRegister()) {
    DCHECK(destination->IsRegister() || destination->IsStackSlot());
    Register src = g.ToRegister(source);
    if (destination->IsRegister()) {
      __ mov(g.ToRegister(destination), src);
    } else {
      __ str(src, g.ToMemOperand(destination));
    }
  } else if (source->IsStackSlot()) {
    DCHECK(destination->IsRegister() || destination->IsStackSlot());
    MemOperand src = g.ToMemOperand(source);
    if (destination->IsRegister()) {
      __ ldr(g.ToRegister(destination), src);
    } else {
      Register temp = kScratchReg;
      __ ldr(temp, src);
      __ str(temp, g.ToMemOperand(destination));
    }
  } else if (source->IsConstant()) {
    Constant src = g.ToConstant(source);
    if (destination->IsRegister() || destination->IsStackSlot()) {
      Register dst =
          destination->IsRegister() ? g.ToRegister(destination) : kScratchReg;
      switch (src.type()) {
        case Constant::kInt32:
          __ mov(dst, Operand(src.ToInt32()));
          break;
        case Constant::kInt64:
          UNREACHABLE();
          break;
        case Constant::kFloat32:
          __ Move(dst,
                  isolate()->factory()->NewNumber(src.ToFloat32(), TENURED));
          break;
        case Constant::kFloat64:
          __ Move(dst,
                  isolate()->factory()->NewNumber(src.ToFloat64(), TENURED));
          break;
        case Constant::kExternalReference:
          __ mov(dst, Operand(src.ToExternalReference()));
          break;
        case Constant::kHeapObject: {
          Handle<HeapObject> src_object = src.ToHeapObject();
          Heap::RootListIndex index;
          int offset;
          if (IsMaterializableFromFrame(src_object, &offset)) {
            __ ldr(dst, MemOperand(fp, offset));
          } else if (IsMaterializableFromRoot(src_object, &index)) {
            __ LoadRoot(dst, index);
          } else {
            __ Move(dst, src_object);
          }
          break;
        }
        case Constant::kRpoNumber:
          UNREACHABLE();  // TODO(dcarney): loading RPO constants on arm.
          break;
      }
      if (destination->IsStackSlot()) __ str(dst, g.ToMemOperand(destination));
    } else if (src.type() == Constant::kFloat32) {
      if (destination->IsDoubleStackSlot()) {
        MemOperand dst = g.ToMemOperand(destination);
        __ mov(ip, Operand(bit_cast<int32_t>(src.ToFloat32())));
        __ str(ip, dst);
      } else {
        SwVfpRegister dst = g.ToFloat32Register(destination);
        __ vmov(dst, src.ToFloat32());
      }
    } else {
      DCHECK_EQ(Constant::kFloat64, src.type());
      DwVfpRegister dst = destination->IsDoubleRegister()
                              ? g.ToFloat64Register(destination)
                              : kScratchDoubleReg;
      __ vmov(dst, src.ToFloat64(), kScratchReg);
      if (destination->IsDoubleStackSlot()) {
        __ vstr(dst, g.ToMemOperand(destination));
      }
    }
  } else if (source->IsDoubleRegister()) {
    DwVfpRegister src = g.ToDoubleRegister(source);
    if (destination->IsDoubleRegister()) {
      DwVfpRegister dst = g.ToDoubleRegister(destination);
      __ Move(dst, src);
    } else {
      DCHECK(destination->IsDoubleStackSlot());
      __ vstr(src, g.ToMemOperand(destination));
    }
  } else if (source->IsDoubleStackSlot()) {
    DCHECK(destination->IsDoubleRegister() || destination->IsDoubleStackSlot());
    MemOperand src = g.ToMemOperand(source);
    if (destination->IsDoubleRegister()) {
      __ vldr(g.ToDoubleRegister(destination), src);
    } else {
      DwVfpRegister temp = kScratchDoubleReg;
      __ vldr(temp, src);
      __ vstr(temp, g.ToMemOperand(destination));
    }
  } else {
    UNREACHABLE();
  }
}


void CodeGenerator::AssembleSwap(InstructionOperand* source,
                                 InstructionOperand* destination) {
  ArmOperandConverter g(this, nullptr);
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
      __ ldr(src, dst);
      __ str(temp, dst);
    }
  } else if (source->IsStackSlot()) {
    DCHECK(destination->IsStackSlot());
    Register temp_0 = kScratchReg;
    SwVfpRegister temp_1 = kScratchDoubleReg.low();
    MemOperand src = g.ToMemOperand(source);
    MemOperand dst = g.ToMemOperand(destination);
    __ ldr(temp_0, src);
    __ vldr(temp_1, dst);
    __ str(temp_0, dst);
    __ vstr(temp_1, src);
  } else if (source->IsDoubleRegister()) {
    DwVfpRegister temp = kScratchDoubleReg;
    DwVfpRegister src = g.ToDoubleRegister(source);
    if (destination->IsDoubleRegister()) {
      DwVfpRegister dst = g.ToDoubleRegister(destination);
      __ Move(temp, src);
      __ Move(src, dst);
      __ Move(dst, temp);
    } else {
      DCHECK(destination->IsDoubleStackSlot());
      MemOperand dst = g.ToMemOperand(destination);
      __ Move(temp, src);
      __ vldr(src, dst);
      __ vstr(temp, dst);
    }
  } else if (source->IsDoubleStackSlot()) {
    DCHECK(destination->IsDoubleStackSlot());
    Register temp_0 = kScratchReg;
    DwVfpRegister temp_1 = kScratchDoubleReg;
    MemOperand src0 = g.ToMemOperand(source);
    MemOperand src1(src0.rn(), src0.offset() + kPointerSize);
    MemOperand dst0 = g.ToMemOperand(destination);
    MemOperand dst1(dst0.rn(), dst0.offset() + kPointerSize);
    __ vldr(temp_1, dst0);  // Save destination in temp_1.
    __ ldr(temp_0, src0);   // Then use temp_0 to copy source to destination.
    __ str(temp_0, dst0);
    __ ldr(temp_0, src1);
    __ str(temp_0, dst1);
    __ vstr(temp_1, src0);
  } else {
    // No other combinations are possible.
    UNREACHABLE();
  }
}


void CodeGenerator::AssembleJumpTable(Label** targets, size_t target_count) {
  // On 32-bit ARM we emit the jump tables inline.
  UNREACHABLE();
}


void CodeGenerator::AddNopForSmiCodeInlining() {
  // On 32-bit ARM we do not insert nops for inlined Smi code.
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
    // Block literal pool emission for duration of padding.
    v8::internal::Assembler::BlockConstPoolScope block_const_pool(masm());
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
