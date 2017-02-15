// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/code-generator.h"

#include "src/compilation-info.h"
#include "src/compiler/code-generator-impl.h"
#include "src/compiler/gap-resolver.h"
#include "src/compiler/node-matchers.h"
#include "src/compiler/osr.h"
#include "src/ppc/macro-assembler-ppc.h"

namespace v8 {
namespace internal {
namespace compiler {

#define __ masm()->


#define kScratchReg r11


// Adds PPC-specific methods to convert InstructionOperands.
class PPCOperandConverter final : public InstructionOperandConverter {
 public:
  PPCOperandConverter(CodeGenerator* gen, Instruction* instr)
      : InstructionOperandConverter(gen, instr) {}

  size_t OutputCount() { return instr_->OutputCount(); }

  RCBit OutputRCBit() const {
    switch (instr_->flags_mode()) {
      case kFlags_branch:
      case kFlags_deoptimize:
      case kFlags_set:
        return SetRC;
      case kFlags_none:
        return LeaveRC;
    }
    UNREACHABLE();
    return LeaveRC;
  }

  bool CompareLogical() const {
    switch (instr_->flags_condition()) {
      case kUnsignedLessThan:
      case kUnsignedGreaterThanOrEqual:
      case kUnsignedLessThanOrEqual:
      case kUnsignedGreaterThan:
        return true;
      default:
        return false;
    }
    UNREACHABLE();
    return false;
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
#if V8_TARGET_ARCH_PPC64
        return Operand(constant.ToInt64());
#endif
      case Constant::kExternalReference:
      case Constant::kHeapObject:
      case Constant::kRpoNumber:
        break;
    }
    UNREACHABLE();
    return Operand::Zero();
  }

  MemOperand MemoryOperand(AddressingMode* mode, size_t* first_index) {
    const size_t index = *first_index;
    *mode = AddressingModeField::decode(instr_->opcode());
    switch (*mode) {
      case kMode_None:
        break;
      case kMode_MRI:
        *first_index += 2;
        return MemOperand(InputRegister(index + 0), InputInt32(index + 1));
      case kMode_MRR:
        *first_index += 2;
        return MemOperand(InputRegister(index + 0), InputRegister(index + 1));
    }
    UNREACHABLE();
    return MemOperand(r0);
  }

  MemOperand MemoryOperand(AddressingMode* mode, size_t first_index = 0) {
    return MemoryOperand(mode, &first_index);
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
};


static inline bool HasRegisterInput(Instruction* instr, size_t index) {
  return instr->InputAt(index)->IsRegister();
}


namespace {

class OutOfLineLoadNAN32 final : public OutOfLineCode {
 public:
  OutOfLineLoadNAN32(CodeGenerator* gen, DoubleRegister result)
      : OutOfLineCode(gen), result_(result) {}

  void Generate() final {
    __ LoadDoubleLiteral(result_, std::numeric_limits<float>::quiet_NaN(),
                         kScratchReg);
  }

 private:
  DoubleRegister const result_;
};


class OutOfLineLoadNAN64 final : public OutOfLineCode {
 public:
  OutOfLineLoadNAN64(CodeGenerator* gen, DoubleRegister result)
      : OutOfLineCode(gen), result_(result) {}

  void Generate() final {
    __ LoadDoubleLiteral(result_, std::numeric_limits<double>::quiet_NaN(),
                         kScratchReg);
  }

 private:
  DoubleRegister const result_;
};


class OutOfLineLoadZero final : public OutOfLineCode {
 public:
  OutOfLineLoadZero(CodeGenerator* gen, Register result)
      : OutOfLineCode(gen), result_(result) {}

  void Generate() final { __ li(result_, Operand::Zero()); }

 private:
  Register const result_;
};


class OutOfLineRecordWrite final : public OutOfLineCode {
 public:
  OutOfLineRecordWrite(CodeGenerator* gen, Register object, Register offset,
                       Register value, Register scratch0, Register scratch1,
                       RecordWriteMode mode)
      : OutOfLineCode(gen),
        object_(object),
        offset_(offset),
        offset_immediate_(0),
        value_(value),
        scratch0_(scratch0),
        scratch1_(scratch1),
        mode_(mode),
        must_save_lr_(!gen->frame_access_state()->has_frame()) {}

  OutOfLineRecordWrite(CodeGenerator* gen, Register object, int32_t offset,
                       Register value, Register scratch0, Register scratch1,
                       RecordWriteMode mode)
      : OutOfLineCode(gen),
        object_(object),
        offset_(no_reg),
        offset_immediate_(offset),
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
      // We need to save and restore lr if the frame was elided.
      __ mflr(scratch1_);
      __ Push(scratch1_);
    }
    RecordWriteStub stub(isolate(), object_, scratch0_, scratch1_,
                         remembered_set_action, save_fp_mode);
    if (offset_.is(no_reg)) {
      __ addi(scratch1_, object_, Operand(offset_immediate_));
    } else {
      DCHECK_EQ(0, offset_immediate_);
      __ add(scratch1_, object_, offset_);
    }
    if (must_save_lr_ && FLAG_enable_embedded_constant_pool) {
      ConstantPoolUnavailableScope constant_pool_unavailable(masm());
      __ CallStub(&stub);
    } else {
      __ CallStub(&stub);
    }
    if (must_save_lr_) {
      // We need to save and restore lr if the frame was elided.
      __ Pop(scratch1_);
      __ mtlr(scratch1_);
    }
  }

 private:
  Register const object_;
  Register const offset_;
  int32_t const offset_immediate_;  // Valid if offset_.is(no_reg).
  Register const value_;
  Register const scratch0_;
  Register const scratch1_;
  RecordWriteMode const mode_;
  bool must_save_lr_;
};


Condition FlagsConditionToCondition(FlagsCondition condition, ArchOpcode op) {
  switch (condition) {
    case kEqual:
      return eq;
    case kNotEqual:
      return ne;
    case kSignedLessThan:
    case kUnsignedLessThan:
      return lt;
    case kSignedGreaterThanOrEqual:
    case kUnsignedGreaterThanOrEqual:
      return ge;
    case kSignedLessThanOrEqual:
    case kUnsignedLessThanOrEqual:
      return le;
    case kSignedGreaterThan:
    case kUnsignedGreaterThan:
      return gt;
    case kOverflow:
      // Overflow checked for add/sub only.
      switch (op) {
#if V8_TARGET_ARCH_PPC64
        case kPPC_Add:
        case kPPC_Sub:
#endif
        case kPPC_AddWithOverflow32:
        case kPPC_SubWithOverflow32:
          return lt;
        default:
          break;
      }
      break;
    case kNotOverflow:
      switch (op) {
#if V8_TARGET_ARCH_PPC64
        case kPPC_Add:
        case kPPC_Sub:
#endif
        case kPPC_AddWithOverflow32:
        case kPPC_SubWithOverflow32:
          return ge;
        default:
          break;
      }
      break;
    default:
      break;
  }
  UNREACHABLE();
  return kNoCondition;
}

}  // namespace

#define ASSEMBLE_FLOAT_UNOP_RC(asm_instr, round)                     \
  do {                                                               \
    __ asm_instr(i.OutputDoubleRegister(), i.InputDoubleRegister(0), \
                 i.OutputRCBit());                                   \
    if (round) {                                                     \
      __ frsp(i.OutputDoubleRegister(), i.OutputDoubleRegister());   \
    }                                                                \
  } while (0)

#define ASSEMBLE_FLOAT_BINOP_RC(asm_instr, round)                    \
  do {                                                               \
    __ asm_instr(i.OutputDoubleRegister(), i.InputDoubleRegister(0), \
                 i.InputDoubleRegister(1), i.OutputRCBit());         \
    if (round) {                                                     \
      __ frsp(i.OutputDoubleRegister(), i.OutputDoubleRegister());   \
    }                                                                \
  } while (0)

#define ASSEMBLE_BINOP(asm_instr_reg, asm_instr_imm)           \
  do {                                                         \
    if (HasRegisterInput(instr, 1)) {                          \
      __ asm_instr_reg(i.OutputRegister(), i.InputRegister(0), \
                       i.InputRegister(1));                    \
    } else {                                                   \
      __ asm_instr_imm(i.OutputRegister(), i.InputRegister(0), \
                       i.InputImmediate(1));                   \
    }                                                          \
  } while (0)


#define ASSEMBLE_BINOP_RC(asm_instr_reg, asm_instr_imm)        \
  do {                                                         \
    if (HasRegisterInput(instr, 1)) {                          \
      __ asm_instr_reg(i.OutputRegister(), i.InputRegister(0), \
                       i.InputRegister(1), i.OutputRCBit());   \
    } else {                                                   \
      __ asm_instr_imm(i.OutputRegister(), i.InputRegister(0), \
                       i.InputImmediate(1), i.OutputRCBit());  \
    }                                                          \
  } while (0)


#define ASSEMBLE_BINOP_INT_RC(asm_instr_reg, asm_instr_imm)    \
  do {                                                         \
    if (HasRegisterInput(instr, 1)) {                          \
      __ asm_instr_reg(i.OutputRegister(), i.InputRegister(0), \
                       i.InputRegister(1), i.OutputRCBit());   \
    } else {                                                   \
      __ asm_instr_imm(i.OutputRegister(), i.InputRegister(0), \
                       i.InputInt32(1), i.OutputRCBit());      \
    }                                                          \
  } while (0)


#define ASSEMBLE_ADD_WITH_OVERFLOW()                                    \
  do {                                                                  \
    if (HasRegisterInput(instr, 1)) {                                   \
      __ AddAndCheckForOverflow(i.OutputRegister(), i.InputRegister(0), \
                                i.InputRegister(1), kScratchReg, r0);   \
    } else {                                                            \
      __ AddAndCheckForOverflow(i.OutputRegister(), i.InputRegister(0), \
                                i.InputInt32(1), kScratchReg, r0);      \
    }                                                                   \
  } while (0)


#define ASSEMBLE_SUB_WITH_OVERFLOW()                                    \
  do {                                                                  \
    if (HasRegisterInput(instr, 1)) {                                   \
      __ SubAndCheckForOverflow(i.OutputRegister(), i.InputRegister(0), \
                                i.InputRegister(1), kScratchReg, r0);   \
    } else {                                                            \
      __ AddAndCheckForOverflow(i.OutputRegister(), i.InputRegister(0), \
                                -i.InputInt32(1), kScratchReg, r0);     \
    }                                                                   \
  } while (0)


#if V8_TARGET_ARCH_PPC64
#define ASSEMBLE_ADD_WITH_OVERFLOW32()         \
  do {                                         \
    ASSEMBLE_ADD_WITH_OVERFLOW();              \
    __ extsw(kScratchReg, kScratchReg, SetRC); \
  } while (0)

#define ASSEMBLE_SUB_WITH_OVERFLOW32()         \
  do {                                         \
    ASSEMBLE_SUB_WITH_OVERFLOW();              \
    __ extsw(kScratchReg, kScratchReg, SetRC); \
  } while (0)
#else
#define ASSEMBLE_ADD_WITH_OVERFLOW32 ASSEMBLE_ADD_WITH_OVERFLOW
#define ASSEMBLE_SUB_WITH_OVERFLOW32 ASSEMBLE_SUB_WITH_OVERFLOW
#endif


#define ASSEMBLE_COMPARE(cmp_instr, cmpl_instr)                        \
  do {                                                                 \
    const CRegister cr = cr0;                                          \
    if (HasRegisterInput(instr, 1)) {                                  \
      if (i.CompareLogical()) {                                        \
        __ cmpl_instr(i.InputRegister(0), i.InputRegister(1), cr);     \
      } else {                                                         \
        __ cmp_instr(i.InputRegister(0), i.InputRegister(1), cr);      \
      }                                                                \
    } else {                                                           \
      if (i.CompareLogical()) {                                        \
        __ cmpl_instr##i(i.InputRegister(0), i.InputImmediate(1), cr); \
      } else {                                                         \
        __ cmp_instr##i(i.InputRegister(0), i.InputImmediate(1), cr);  \
      }                                                                \
    }                                                                  \
    DCHECK_EQ(SetRC, i.OutputRCBit());                                 \
  } while (0)


#define ASSEMBLE_FLOAT_COMPARE(cmp_instr)                                 \
  do {                                                                    \
    const CRegister cr = cr0;                                             \
    __ cmp_instr(i.InputDoubleRegister(0), i.InputDoubleRegister(1), cr); \
    DCHECK_EQ(SetRC, i.OutputRCBit());                                    \
  } while (0)


#define ASSEMBLE_MODULO(div_instr, mul_instr)                        \
  do {                                                               \
    const Register scratch = kScratchReg;                            \
    __ div_instr(scratch, i.InputRegister(0), i.InputRegister(1));   \
    __ mul_instr(scratch, scratch, i.InputRegister(1));              \
    __ sub(i.OutputRegister(), i.InputRegister(0), scratch, LeaveOE, \
           i.OutputRCBit());                                         \
  } while (0)


#define ASSEMBLE_FLOAT_MODULO()                                               \
  do {                                                                        \
    FrameScope scope(masm(), StackFrame::MANUAL);                             \
    __ PrepareCallCFunction(0, 2, kScratchReg);                               \
    __ MovToFloatParameters(i.InputDoubleRegister(0),                         \
                            i.InputDoubleRegister(1));                        \
    __ CallCFunction(ExternalReference::mod_two_doubles_operation(isolate()), \
                     0, 2);                                                   \
    __ MovFromFloatResult(i.OutputDoubleRegister());                          \
    DCHECK_EQ(LeaveRC, i.OutputRCBit());                                      \
  } while (0)

#define ASSEMBLE_IEEE754_UNOP(name)                                            \
  do {                                                                         \
    /* TODO(bmeurer): We should really get rid of this special instruction, */ \
    /* and generate a CallAddress instruction instead. */                      \
    FrameScope scope(masm(), StackFrame::MANUAL);                              \
    __ PrepareCallCFunction(0, 1, kScratchReg);                                \
    __ MovToFloatParameter(i.InputDoubleRegister(0));                          \
    __ CallCFunction(ExternalReference::ieee754_##name##_function(isolate()),  \
                     0, 1);                                                    \
    /* Move the result in the double result register. */                       \
    __ MovFromFloatResult(i.OutputDoubleRegister());                           \
    DCHECK_EQ(LeaveRC, i.OutputRCBit());                                       \
  } while (0)

#define ASSEMBLE_IEEE754_BINOP(name)                                           \
  do {                                                                         \
    /* TODO(bmeurer): We should really get rid of this special instruction, */ \
    /* and generate a CallAddress instruction instead. */                      \
    FrameScope scope(masm(), StackFrame::MANUAL);                              \
    __ PrepareCallCFunction(0, 2, kScratchReg);                                \
    __ MovToFloatParameters(i.InputDoubleRegister(0),                          \
                           i.InputDoubleRegister(1));                          \
    __ CallCFunction(ExternalReference::ieee754_##name##_function(isolate()),  \
                     0, 2);                                                    \
    /* Move the result in the double result register. */                       \
    __ MovFromFloatResult(i.OutputDoubleRegister());                           \
    DCHECK_EQ(LeaveRC, i.OutputRCBit());                                       \
  } while (0)

#define ASSEMBLE_FLOAT_MAX()                                                  \
  do {                                                                        \
    DoubleRegister left_reg = i.InputDoubleRegister(0);                       \
    DoubleRegister right_reg = i.InputDoubleRegister(1);                      \
    DoubleRegister result_reg = i.OutputDoubleRegister();                     \
    Label check_nan_left, check_zero, return_left, return_right, done;        \
    __ fcmpu(left_reg, right_reg);                                            \
    __ bunordered(&check_nan_left);                                           \
    __ beq(&check_zero);                                                      \
    __ bge(&return_left);                                                     \
    __ b(&return_right);                                                      \
                                                                              \
    __ bind(&check_zero);                                                     \
    __ fcmpu(left_reg, kDoubleRegZero);                                       \
    /* left == right != 0. */                                                 \
    __ bne(&return_left);                                                     \
    /* At this point, both left and right are either 0 or -0. */              \
    __ fadd(result_reg, left_reg, right_reg);                                 \
    __ b(&done);                                                              \
                                                                              \
    __ bind(&check_nan_left);                                                 \
    __ fcmpu(left_reg, left_reg);                                             \
    /* left == NaN. */                                                        \
    __ bunordered(&return_left);                                              \
    __ bind(&return_right);                                                   \
    if (!right_reg.is(result_reg)) {                                          \
      __ fmr(result_reg, right_reg);                                          \
    }                                                                         \
    __ b(&done);                                                              \
                                                                              \
    __ bind(&return_left);                                                    \
    if (!left_reg.is(result_reg)) {                                           \
      __ fmr(result_reg, left_reg);                                           \
    }                                                                         \
    __ bind(&done);                                                           \
  } while (0)                                                                 \


#define ASSEMBLE_FLOAT_MIN()                                                   \
  do {                                                                         \
    DoubleRegister left_reg = i.InputDoubleRegister(0);                        \
    DoubleRegister right_reg = i.InputDoubleRegister(1);                       \
    DoubleRegister result_reg = i.OutputDoubleRegister();                      \
    Label check_nan_left, check_zero, return_left, return_right, done;         \
    __ fcmpu(left_reg, right_reg);                                             \
    __ bunordered(&check_nan_left);                                            \
    __ beq(&check_zero);                                                       \
    __ ble(&return_left);                                                      \
    __ b(&return_right);                                                       \
                                                                               \
    __ bind(&check_zero);                                                      \
    __ fcmpu(left_reg, kDoubleRegZero);                                        \
    /* left == right != 0. */                                                  \
    __ bne(&return_left);                                                      \
    /* At this point, both left and right are either 0 or -0. */               \
    /* Min: The algorithm is: -((-L) + (-R)), which in case of L and R being */\
    /* different registers is most efficiently expressed as -((-L) - R). */    \
    __ fneg(left_reg, left_reg);                                               \
    if (left_reg.is(right_reg)) {                                              \
      __ fadd(result_reg, left_reg, right_reg);                                \
    } else {                                                                   \
      __ fsub(result_reg, left_reg, right_reg);                                \
    }                                                                          \
    __ fneg(result_reg, result_reg);                                           \
    __ b(&done);                                                               \
                                                                               \
    __ bind(&check_nan_left);                                                  \
    __ fcmpu(left_reg, left_reg);                                              \
    /* left == NaN. */                                                         \
    __ bunordered(&return_left);                                               \
                                                                               \
    __ bind(&return_right);                                                    \
    if (!right_reg.is(result_reg)) {                                           \
      __ fmr(result_reg, right_reg);                                           \
    }                                                                          \
    __ b(&done);                                                               \
                                                                               \
    __ bind(&return_left);                                                     \
    if (!left_reg.is(result_reg)) {                                            \
      __ fmr(result_reg, left_reg);                                            \
    }                                                                          \
    __ bind(&done);                                                            \
  } while (0)


#define ASSEMBLE_LOAD_FLOAT(asm_instr, asm_instrx)    \
  do {                                                \
    DoubleRegister result = i.OutputDoubleRegister(); \
    AddressingMode mode = kMode_None;                 \
    MemOperand operand = i.MemoryOperand(&mode);      \
    if (mode == kMode_MRI) {                          \
      __ asm_instr(result, operand);                  \
    } else {                                          \
      __ asm_instrx(result, operand);                 \
    }                                                 \
    DCHECK_EQ(LeaveRC, i.OutputRCBit());              \
  } while (0)


#define ASSEMBLE_LOAD_INTEGER(asm_instr, asm_instrx) \
  do {                                               \
    Register result = i.OutputRegister();            \
    AddressingMode mode = kMode_None;                \
    MemOperand operand = i.MemoryOperand(&mode);     \
    if (mode == kMode_MRI) {                         \
      __ asm_instr(result, operand);                 \
    } else {                                         \
      __ asm_instrx(result, operand);                \
    }                                                \
    DCHECK_EQ(LeaveRC, i.OutputRCBit());             \
  } while (0)


#define ASSEMBLE_STORE_FLOAT32()                         \
  do {                                                   \
    size_t index = 0;                                    \
    AddressingMode mode = kMode_None;                    \
    MemOperand operand = i.MemoryOperand(&mode, &index); \
    DoubleRegister value = i.InputDoubleRegister(index); \
    __ frsp(kScratchDoubleReg, value);                   \
    if (mode == kMode_MRI) {                             \
      __ stfs(kScratchDoubleReg, operand);               \
    } else {                                             \
      __ stfsx(kScratchDoubleReg, operand);              \
    }                                                    \
    DCHECK_EQ(LeaveRC, i.OutputRCBit());                 \
  } while (0)


#define ASSEMBLE_STORE_DOUBLE()                          \
  do {                                                   \
    size_t index = 0;                                    \
    AddressingMode mode = kMode_None;                    \
    MemOperand operand = i.MemoryOperand(&mode, &index); \
    DoubleRegister value = i.InputDoubleRegister(index); \
    if (mode == kMode_MRI) {                             \
      __ stfd(value, operand);                           \
    } else {                                             \
      __ stfdx(value, operand);                          \
    }                                                    \
    DCHECK_EQ(LeaveRC, i.OutputRCBit());                 \
  } while (0)


#define ASSEMBLE_STORE_INTEGER(asm_instr, asm_instrx)    \
  do {                                                   \
    size_t index = 0;                                    \
    AddressingMode mode = kMode_None;                    \
    MemOperand operand = i.MemoryOperand(&mode, &index); \
    Register value = i.InputRegister(index);             \
    if (mode == kMode_MRI) {                             \
      __ asm_instr(value, operand);                      \
    } else {                                             \
      __ asm_instrx(value, operand);                     \
    }                                                    \
    DCHECK_EQ(LeaveRC, i.OutputRCBit());                 \
  } while (0)

#if V8_TARGET_ARCH_PPC64
// TODO(mbrandy): fix paths that produce garbage in offset's upper 32-bits.
#define CleanUInt32(x) __ ClearLeftImm(x, x, Operand(32))
#else
#define CleanUInt32(x)
#endif

#define ASSEMBLE_CHECKED_LOAD_FLOAT(asm_instr, asm_instrx, width)  \
  do {                                                             \
    DoubleRegister result = i.OutputDoubleRegister();              \
    size_t index = 0;                                              \
    AddressingMode mode = kMode_None;                              \
    MemOperand operand = i.MemoryOperand(&mode, index);            \
    DCHECK_EQ(kMode_MRR, mode);                                    \
    Register offset = operand.rb();                                \
    if (HasRegisterInput(instr, 2)) {                              \
      __ cmplw(offset, i.InputRegister(2));                        \
    } else {                                                       \
      __ cmplwi(offset, i.InputImmediate(2));                      \
    }                                                              \
    auto ool = new (zone()) OutOfLineLoadNAN##width(this, result); \
    __ bge(ool->entry());                                          \
    if (mode == kMode_MRI) {                                       \
      __ asm_instr(result, operand);                               \
    } else {                                                       \
      CleanUInt32(offset);                                         \
      __ asm_instrx(result, operand);                              \
    }                                                              \
    __ bind(ool->exit());                                          \
    DCHECK_EQ(LeaveRC, i.OutputRCBit());                           \
  } while (0)

#define ASSEMBLE_CHECKED_LOAD_INTEGER(asm_instr, asm_instrx) \
  do {                                                       \
    Register result = i.OutputRegister();                    \
    size_t index = 0;                                        \
    AddressingMode mode = kMode_None;                        \
    MemOperand operand = i.MemoryOperand(&mode, index);      \
    DCHECK_EQ(kMode_MRR, mode);                              \
    Register offset = operand.rb();                          \
    if (HasRegisterInput(instr, 2)) {                        \
      __ cmplw(offset, i.InputRegister(2));                  \
    } else {                                                 \
      __ cmplwi(offset, i.InputImmediate(2));                \
    }                                                        \
    auto ool = new (zone()) OutOfLineLoadZero(this, result); \
    __ bge(ool->entry());                                    \
    if (mode == kMode_MRI) {                                 \
      __ asm_instr(result, operand);                         \
    } else {                                                 \
      CleanUInt32(offset);                                   \
      __ asm_instrx(result, operand);                        \
    }                                                        \
    __ bind(ool->exit());                                    \
    DCHECK_EQ(LeaveRC, i.OutputRCBit());                     \
  } while (0)

#define ASSEMBLE_CHECKED_STORE_FLOAT32()                \
  do {                                                  \
    Label done;                                         \
    size_t index = 0;                                   \
    AddressingMode mode = kMode_None;                   \
    MemOperand operand = i.MemoryOperand(&mode, index); \
    DCHECK_EQ(kMode_MRR, mode);                         \
    Register offset = operand.rb();                     \
    if (HasRegisterInput(instr, 2)) {                   \
      __ cmplw(offset, i.InputRegister(2));             \
    } else {                                            \
      __ cmplwi(offset, i.InputImmediate(2));           \
    }                                                   \
    __ bge(&done);                                      \
    DoubleRegister value = i.InputDoubleRegister(3);    \
    __ frsp(kScratchDoubleReg, value);                  \
    if (mode == kMode_MRI) {                            \
      __ stfs(kScratchDoubleReg, operand);              \
    } else {                                            \
      CleanUInt32(offset);                              \
      __ stfsx(kScratchDoubleReg, operand);             \
    }                                                   \
    __ bind(&done);                                     \
    DCHECK_EQ(LeaveRC, i.OutputRCBit());                \
  } while (0)

#define ASSEMBLE_CHECKED_STORE_DOUBLE()                 \
  do {                                                  \
    Label done;                                         \
    size_t index = 0;                                   \
    AddressingMode mode = kMode_None;                   \
    MemOperand operand = i.MemoryOperand(&mode, index); \
    DCHECK_EQ(kMode_MRR, mode);                         \
    Register offset = operand.rb();                     \
    if (HasRegisterInput(instr, 2)) {                   \
      __ cmplw(offset, i.InputRegister(2));             \
    } else {                                            \
      __ cmplwi(offset, i.InputImmediate(2));           \
    }                                                   \
    __ bge(&done);                                      \
    DoubleRegister value = i.InputDoubleRegister(3);    \
    if (mode == kMode_MRI) {                            \
      __ stfd(value, operand);                          \
    } else {                                            \
      CleanUInt32(offset);                              \
      __ stfdx(value, operand);                         \
    }                                                   \
    __ bind(&done);                                     \
    DCHECK_EQ(LeaveRC, i.OutputRCBit());                \
  } while (0)

#define ASSEMBLE_CHECKED_STORE_INTEGER(asm_instr, asm_instrx) \
  do {                                                        \
    Label done;                                               \
    size_t index = 0;                                         \
    AddressingMode mode = kMode_None;                         \
    MemOperand operand = i.MemoryOperand(&mode, index);       \
    DCHECK_EQ(kMode_MRR, mode);                               \
    Register offset = operand.rb();                           \
    if (HasRegisterInput(instr, 2)) {                         \
      __ cmplw(offset, i.InputRegister(2));                   \
    } else {                                                  \
      __ cmplwi(offset, i.InputImmediate(2));                 \
    }                                                         \
    __ bge(&done);                                            \
    Register value = i.InputRegister(3);                      \
    if (mode == kMode_MRI) {                                  \
      __ asm_instr(value, operand);                           \
    } else {                                                  \
      CleanUInt32(offset);                                    \
      __ asm_instrx(value, operand);                          \
    }                                                         \
    __ bind(&done);                                           \
    DCHECK_EQ(LeaveRC, i.OutputRCBit());                      \
  } while (0)

#define ASSEMBLE_ATOMIC_LOAD_INTEGER(asm_instr, asm_instrx)   \
  do {                                                        \
    Label done;                                               \
    Register result = i.OutputRegister();                     \
    AddressingMode mode = kMode_None;                         \
    MemOperand operand = i.MemoryOperand(&mode);              \
    __ sync();                                                \
    if (mode == kMode_MRI) {                                  \
    __ asm_instr(result, operand);                            \
    } else {                                                  \
    __ asm_instrx(result, operand);                           \
    }                                                         \
    __ bind(&done);                                           \
    __ cmp(result, result);                                   \
    __ bne(&done);                                            \
    __ isync();                                               \
  } while (0)
#define ASSEMBLE_ATOMIC_STORE_INTEGER(asm_instr, asm_instrx)  \
  do {                                                        \
    size_t index = 0;                                         \
    AddressingMode mode = kMode_None;                         \
    MemOperand operand = i.MemoryOperand(&mode, &index);      \
    Register value = i.InputRegister(index);                  \
    __ sync();                                                \
    if (mode == kMode_MRI) {                                  \
      __ asm_instr(value, operand);                           \
    } else {                                                  \
      __ asm_instrx(value, operand);                          \
    }                                                         \
    DCHECK_EQ(LeaveRC, i.OutputRCBit());                      \
  } while (0)

void CodeGenerator::AssembleDeconstructFrame() {
  __ LeaveFrame(StackFrame::MANUAL);
}

void CodeGenerator::AssemblePrepareTailCall() {
  if (frame_access_state()->has_frame()) {
    __ RestoreFrameStateForTailCall();
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
  __ LoadP(scratch1, MemOperand(fp, StandardFrameConstants::kContextOffset));
  __ CmpSmiLiteral(scratch1, Smi::FromInt(StackFrame::ARGUMENTS_ADAPTOR), r0);
  __ bne(&done);

  // Load arguments count from current arguments adaptor frame (note, it
  // does not include receiver).
  Register caller_args_count_reg = scratch1;
  __ LoadP(caller_args_count_reg,
           MemOperand(fp, ArgumentsAdaptorFrameConstants::kLengthOffset));
  __ SmiUntag(caller_args_count_reg);

  ParameterCount callee_args_count(args_reg);
  __ PrepareForTailCall(callee_args_count, caller_args_count_reg, scratch2,
                        scratch3);
  __ bind(&done);
}

namespace {

void FlushPendingPushRegisters(MacroAssembler* masm,
                               FrameAccessState* frame_access_state,
                               ZoneVector<Register>* pending_pushes) {
  switch (pending_pushes->size()) {
    case 0:
      break;
    case 1:
      masm->Push((*pending_pushes)[0]);
      break;
    case 2:
      masm->Push((*pending_pushes)[0], (*pending_pushes)[1]);
      break;
    case 3:
      masm->Push((*pending_pushes)[0], (*pending_pushes)[1],
                 (*pending_pushes)[2]);
      break;
    default:
      UNREACHABLE();
      break;
  }
  frame_access_state->IncreaseSPDelta(pending_pushes->size());
  pending_pushes->resize(0);
}

void AddPendingPushRegister(MacroAssembler* masm,
                            FrameAccessState* frame_access_state,
                            ZoneVector<Register>* pending_pushes,
                            Register reg) {
  pending_pushes->push_back(reg);
  if (pending_pushes->size() == 3 || reg.is(ip)) {
    FlushPendingPushRegisters(masm, frame_access_state, pending_pushes);
  }
}

void AdjustStackPointerForTailCall(
    MacroAssembler* masm, FrameAccessState* state, int new_slot_above_sp,
    ZoneVector<Register>* pending_pushes = nullptr,
    bool allow_shrinkage = true) {
  int current_sp_offset = state->GetSPToFPSlotCount() +
                          StandardFrameConstants::kFixedSlotCountAboveFp;
  int stack_slot_delta = new_slot_above_sp - current_sp_offset;
  if (stack_slot_delta > 0) {
    if (pending_pushes != nullptr) {
      FlushPendingPushRegisters(masm, state, pending_pushes);
    }
    masm->Add(sp, sp, -stack_slot_delta * kPointerSize, r0);
    state->IncreaseSPDelta(stack_slot_delta);
  } else if (allow_shrinkage && stack_slot_delta < 0) {
    if (pending_pushes != nullptr) {
      FlushPendingPushRegisters(masm, state, pending_pushes);
    }
    masm->Add(sp, sp, -stack_slot_delta * kPointerSize, r0);
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
    PPCOperandConverter g(this, instr);
    ZoneVector<Register> pending_pushes(zone());
    for (auto move : pushes) {
      LocationOperand destination_location(
          LocationOperand::cast(move->destination()));
      InstructionOperand source(move->source());
      AdjustStackPointerForTailCall(
          masm(), frame_access_state(),
          destination_location.index() - pending_pushes.size(),
          &pending_pushes);
      if (source.IsStackSlot()) {
        LocationOperand source_location(LocationOperand::cast(source));
        __ LoadP(ip, g.SlotToMemOperand(source_location.index()));
        AddPendingPushRegister(masm(), frame_access_state(), &pending_pushes,
                               ip);
      } else if (source.IsRegister()) {
        LocationOperand source_location(LocationOperand::cast(source));
        AddPendingPushRegister(masm(), frame_access_state(), &pending_pushes,
                               source_location.GetRegister());
      } else if (source.IsImmediate()) {
        AddPendingPushRegister(masm(), frame_access_state(), &pending_pushes,
                               ip);
      } else {
        // Pushes of non-scalar data types is not supported.
        UNIMPLEMENTED();
      }
      move->Eliminate();
    }
    FlushPendingPushRegisters(masm(), frame_access_state(), &pending_pushes);
  }
  AdjustStackPointerForTailCall(masm(), frame_access_state(),
                                first_unused_stack_slot, nullptr, false);
}

void CodeGenerator::AssembleTailCallAfterGap(Instruction* instr,
                                             int first_unused_stack_slot) {
  AdjustStackPointerForTailCall(masm(), frame_access_state(),
                                first_unused_stack_slot);
}


// Assembles an instruction after register allocation, producing machine code.
CodeGenerator::CodeGenResult CodeGenerator::AssembleArchInstruction(
    Instruction* instr) {
  PPCOperandConverter i(this, instr);
  ArchOpcode opcode = ArchOpcodeField::decode(instr->opcode());

  switch (opcode) {
    case kArchCallCodeObject: {
      v8::internal::Assembler::BlockTrampolinePoolScope block_trampoline_pool(
          masm());
      EnsureSpaceForLazyDeopt();
      if (HasRegisterInput(instr, 0)) {
        __ addi(ip, i.InputRegister(0),
                Operand(Code::kHeaderSize - kHeapObjectTag));
        __ Call(ip);
      } else {
        __ Call(Handle<Code>::cast(i.InputHeapObject(0)),
                RelocInfo::CODE_TARGET);
      }
      RecordCallPosition(instr);
      DCHECK_EQ(LeaveRC, i.OutputRCBit());
      frame_access_state()->ClearSPDelta();
      break;
    }
    case kArchTailCallCodeObjectFromJSFunction:
    case kArchTailCallCodeObject: {
      if (opcode == kArchTailCallCodeObjectFromJSFunction) {
        AssemblePopArgumentsAdaptorFrame(kJavaScriptCallArgCountRegister,
                                         i.TempRegister(0), i.TempRegister(1),
                                         i.TempRegister(2));
      }
      if (HasRegisterInput(instr, 0)) {
        __ addi(ip, i.InputRegister(0),
                Operand(Code::kHeaderSize - kHeapObjectTag));
        __ Jump(ip);
      } else {
        // We cannot use the constant pool to load the target since
        // we've already restored the caller's frame.
        ConstantPoolUnavailableScope constant_pool_unavailable(masm());
        __ Jump(Handle<Code>::cast(i.InputHeapObject(0)),
                RelocInfo::CODE_TARGET);
      }
      DCHECK_EQ(LeaveRC, i.OutputRCBit());
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
      v8::internal::Assembler::BlockTrampolinePoolScope block_trampoline_pool(
          masm());
      EnsureSpaceForLazyDeopt();
      Register func = i.InputRegister(0);
      if (FLAG_debug_code) {
        // Check the function's context matches the context argument.
        __ LoadP(kScratchReg,
                 FieldMemOperand(func, JSFunction::kContextOffset));
        __ cmp(cp, kScratchReg);
        __ Assert(eq, kWrongFunctionContext);
      }
      __ LoadP(ip, FieldMemOperand(func, JSFunction::kCodeEntryOffset));
      __ Call(ip);
      RecordCallPosition(instr);
      DCHECK_EQ(LeaveRC, i.OutputRCBit());
      frame_access_state()->ClearSPDelta();
      break;
    }
    case kArchTailCallJSFunctionFromJSFunction:
    case kArchTailCallJSFunction: {
      Register func = i.InputRegister(0);
      if (FLAG_debug_code) {
        // Check the function's context matches the context argument.
        __ LoadP(kScratchReg,
                 FieldMemOperand(func, JSFunction::kContextOffset));
        __ cmp(cp, kScratchReg);
        __ Assert(eq, kWrongFunctionContext);
      }
      if (opcode == kArchTailCallJSFunctionFromJSFunction) {
        AssemblePopArgumentsAdaptorFrame(kJavaScriptCallArgCountRegister,
                                         i.TempRegister(0), i.TempRegister(1),
                                         i.TempRegister(2));
      }
      __ LoadP(ip, FieldMemOperand(func, JSFunction::kCodeEntryOffset));
      __ Jump(ip);
      DCHECK_EQ(LeaveRC, i.OutputRCBit());
      frame_access_state()->ClearSPDelta();
      frame_access_state()->SetFrameAccessToDefault();
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
    case kArchComment: {
      Address comment_string = i.InputExternalReference(0).address();
      __ RecordComment(reinterpret_cast<const char*>(comment_string));
      break;
    }
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
      DCHECK_EQ(LeaveRC, i.OutputRCBit());
      break;
    case kArchLookupSwitch:
      AssembleArchLookupSwitch(instr);
      DCHECK_EQ(LeaveRC, i.OutputRCBit());
      break;
    case kArchTableSwitch:
      AssembleArchTableSwitch(instr);
      DCHECK_EQ(LeaveRC, i.OutputRCBit());
      break;
    case kArchDebugBreak:
      __ stop("kArchDebugBreak");
      break;
    case kArchNop:
    case kArchThrowTerminator:
      // don't emit code for nops.
      DCHECK_EQ(LeaveRC, i.OutputRCBit());
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
      AssembleReturn();
      DCHECK_EQ(LeaveRC, i.OutputRCBit());
      break;
    case kArchStackPointer:
      __ mr(i.OutputRegister(), sp);
      DCHECK_EQ(LeaveRC, i.OutputRCBit());
      break;
    case kArchFramePointer:
      __ mr(i.OutputRegister(), fp);
      DCHECK_EQ(LeaveRC, i.OutputRCBit());
      break;
    case kArchParentFramePointer:
      if (frame_access_state()->has_frame()) {
        __ LoadP(i.OutputRegister(), MemOperand(fp, 0));
      } else {
        __ mr(i.OutputRegister(), fp);
      }
      break;
    case kArchTruncateDoubleToI:
      // TODO(mbrandy): move slow call to stub out of line.
      __ TruncateDoubleToI(i.OutputRegister(), i.InputDoubleRegister(0));
      DCHECK_EQ(LeaveRC, i.OutputRCBit());
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
      if (addressing_mode == kMode_MRI) {
        int32_t offset = i.InputInt32(1);
        ool = new (zone()) OutOfLineRecordWrite(this, object, offset, value,
                                                scratch0, scratch1, mode);
        __ StoreP(value, MemOperand(object, offset));
      } else {
        DCHECK_EQ(kMode_MRR, addressing_mode);
        Register offset(i.InputRegister(1));
        ool = new (zone()) OutOfLineRecordWrite(this, object, offset, value,
                                                scratch0, scratch1, mode);
        __ StorePX(value, MemOperand(object, offset));
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
      __ addi(i.OutputRegister(), offset.from_stack_pointer() ? sp : fp,
              Operand(offset.offset()));
      break;
    }
    case kPPC_And:
      if (HasRegisterInput(instr, 1)) {
        __ and_(i.OutputRegister(), i.InputRegister(0), i.InputRegister(1),
                i.OutputRCBit());
      } else {
        __ andi(i.OutputRegister(), i.InputRegister(0), i.InputImmediate(1));
      }
      break;
    case kPPC_AndComplement:
      __ andc(i.OutputRegister(), i.InputRegister(0), i.InputRegister(1),
              i.OutputRCBit());
      break;
    case kPPC_Or:
      if (HasRegisterInput(instr, 1)) {
        __ orx(i.OutputRegister(), i.InputRegister(0), i.InputRegister(1),
               i.OutputRCBit());
      } else {
        __ ori(i.OutputRegister(), i.InputRegister(0), i.InputImmediate(1));
        DCHECK_EQ(LeaveRC, i.OutputRCBit());
      }
      break;
    case kPPC_OrComplement:
      __ orc(i.OutputRegister(), i.InputRegister(0), i.InputRegister(1),
             i.OutputRCBit());
      break;
    case kPPC_Xor:
      if (HasRegisterInput(instr, 1)) {
        __ xor_(i.OutputRegister(), i.InputRegister(0), i.InputRegister(1),
                i.OutputRCBit());
      } else {
        __ xori(i.OutputRegister(), i.InputRegister(0), i.InputImmediate(1));
        DCHECK_EQ(LeaveRC, i.OutputRCBit());
      }
      break;
    case kPPC_ShiftLeft32:
      ASSEMBLE_BINOP_RC(slw, slwi);
      break;
#if V8_TARGET_ARCH_PPC64
    case kPPC_ShiftLeft64:
      ASSEMBLE_BINOP_RC(sld, sldi);
      break;
#endif
    case kPPC_ShiftRight32:
      ASSEMBLE_BINOP_RC(srw, srwi);
      break;
#if V8_TARGET_ARCH_PPC64
    case kPPC_ShiftRight64:
      ASSEMBLE_BINOP_RC(srd, srdi);
      break;
#endif
    case kPPC_ShiftRightAlg32:
      ASSEMBLE_BINOP_INT_RC(sraw, srawi);
      break;
#if V8_TARGET_ARCH_PPC64
    case kPPC_ShiftRightAlg64:
      ASSEMBLE_BINOP_INT_RC(srad, sradi);
      break;
#endif
#if !V8_TARGET_ARCH_PPC64
    case kPPC_AddPair:
      // i.InputRegister(0) ... left low word.
      // i.InputRegister(1) ... left high word.
      // i.InputRegister(2) ... right low word.
      // i.InputRegister(3) ... right high word.
      __ addc(i.OutputRegister(0), i.InputRegister(0), i.InputRegister(2));
      __ adde(i.OutputRegister(1), i.InputRegister(1), i.InputRegister(3));
      DCHECK_EQ(LeaveRC, i.OutputRCBit());
      break;
    case kPPC_SubPair:
      // i.InputRegister(0) ... left low word.
      // i.InputRegister(1) ... left high word.
      // i.InputRegister(2) ... right low word.
      // i.InputRegister(3) ... right high word.
      __ subc(i.OutputRegister(0), i.InputRegister(0), i.InputRegister(2));
      __ sube(i.OutputRegister(1), i.InputRegister(1), i.InputRegister(3));
      DCHECK_EQ(LeaveRC, i.OutputRCBit());
      break;
    case kPPC_MulPair:
      // i.InputRegister(0) ... left low word.
      // i.InputRegister(1) ... left high word.
      // i.InputRegister(2) ... right low word.
      // i.InputRegister(3) ... right high word.
      __ mullw(i.TempRegister(0), i.InputRegister(0), i.InputRegister(3));
      __ mullw(i.TempRegister(1), i.InputRegister(2), i.InputRegister(1));
      __ add(i.TempRegister(0), i.TempRegister(0), i.TempRegister(1));
      __ mullw(i.OutputRegister(0), i.InputRegister(0), i.InputRegister(2));
      __ mulhwu(i.OutputRegister(1), i.InputRegister(0), i.InputRegister(2));
      __ add(i.OutputRegister(1), i.OutputRegister(1), i.TempRegister(0));
      break;
    case kPPC_ShiftLeftPair:
      if (instr->InputAt(2)->IsImmediate()) {
        __ ShiftLeftPair(i.OutputRegister(0), i.OutputRegister(1),
                         i.InputRegister(0), i.InputRegister(1),
                         i.InputInt32(2));
      } else {
        __ ShiftLeftPair(i.OutputRegister(0), i.OutputRegister(1),
                         i.InputRegister(0), i.InputRegister(1), kScratchReg,
                         i.InputRegister(2));
      }
      break;
    case kPPC_ShiftRightPair:
      if (instr->InputAt(2)->IsImmediate()) {
        __ ShiftRightPair(i.OutputRegister(0), i.OutputRegister(1),
                          i.InputRegister(0), i.InputRegister(1),
                          i.InputInt32(2));
      } else {
        __ ShiftRightPair(i.OutputRegister(0), i.OutputRegister(1),
                          i.InputRegister(0), i.InputRegister(1), kScratchReg,
                          i.InputRegister(2));
      }
      break;
    case kPPC_ShiftRightAlgPair:
      if (instr->InputAt(2)->IsImmediate()) {
        __ ShiftRightAlgPair(i.OutputRegister(0), i.OutputRegister(1),
                             i.InputRegister(0), i.InputRegister(1),
                             i.InputInt32(2));
      } else {
        __ ShiftRightAlgPair(i.OutputRegister(0), i.OutputRegister(1),
                             i.InputRegister(0), i.InputRegister(1),
                             kScratchReg, i.InputRegister(2));
      }
      break;
#endif
    case kPPC_RotRight32:
      if (HasRegisterInput(instr, 1)) {
        __ subfic(kScratchReg, i.InputRegister(1), Operand(32));
        __ rotlw(i.OutputRegister(), i.InputRegister(0), kScratchReg,
                 i.OutputRCBit());
      } else {
        int sh = i.InputInt32(1);
        __ rotrwi(i.OutputRegister(), i.InputRegister(0), sh, i.OutputRCBit());
      }
      break;
#if V8_TARGET_ARCH_PPC64
    case kPPC_RotRight64:
      if (HasRegisterInput(instr, 1)) {
        __ subfic(kScratchReg, i.InputRegister(1), Operand(64));
        __ rotld(i.OutputRegister(), i.InputRegister(0), kScratchReg,
                 i.OutputRCBit());
      } else {
        int sh = i.InputInt32(1);
        __ rotrdi(i.OutputRegister(), i.InputRegister(0), sh, i.OutputRCBit());
      }
      break;
#endif
    case kPPC_Not:
      __ notx(i.OutputRegister(), i.InputRegister(0), i.OutputRCBit());
      break;
    case kPPC_RotLeftAndMask32:
      __ rlwinm(i.OutputRegister(), i.InputRegister(0), i.InputInt32(1),
                31 - i.InputInt32(2), 31 - i.InputInt32(3), i.OutputRCBit());
      break;
#if V8_TARGET_ARCH_PPC64
    case kPPC_RotLeftAndClear64:
      __ rldic(i.OutputRegister(), i.InputRegister(0), i.InputInt32(1),
               63 - i.InputInt32(2), i.OutputRCBit());
      break;
    case kPPC_RotLeftAndClearLeft64:
      __ rldicl(i.OutputRegister(), i.InputRegister(0), i.InputInt32(1),
                63 - i.InputInt32(2), i.OutputRCBit());
      break;
    case kPPC_RotLeftAndClearRight64:
      __ rldicr(i.OutputRegister(), i.InputRegister(0), i.InputInt32(1),
                63 - i.InputInt32(2), i.OutputRCBit());
      break;
#endif
    case kPPC_Add:
#if V8_TARGET_ARCH_PPC64
      if (FlagsModeField::decode(instr->opcode()) != kFlags_none) {
        ASSEMBLE_ADD_WITH_OVERFLOW();
      } else {
#endif
        if (HasRegisterInput(instr, 1)) {
          __ add(i.OutputRegister(), i.InputRegister(0), i.InputRegister(1),
                 LeaveOE, i.OutputRCBit());
        } else {
          __ addi(i.OutputRegister(), i.InputRegister(0), i.InputImmediate(1));
          DCHECK_EQ(LeaveRC, i.OutputRCBit());
        }
#if V8_TARGET_ARCH_PPC64
      }
#endif
      break;
    case kPPC_AddWithOverflow32:
      ASSEMBLE_ADD_WITH_OVERFLOW32();
      break;
    case kPPC_AddDouble:
      ASSEMBLE_FLOAT_BINOP_RC(fadd, MiscField::decode(instr->opcode()));
      break;
    case kPPC_Sub:
#if V8_TARGET_ARCH_PPC64
      if (FlagsModeField::decode(instr->opcode()) != kFlags_none) {
        ASSEMBLE_SUB_WITH_OVERFLOW();
      } else {
#endif
        if (HasRegisterInput(instr, 1)) {
          __ sub(i.OutputRegister(), i.InputRegister(0), i.InputRegister(1),
                 LeaveOE, i.OutputRCBit());
        } else {
          __ subi(i.OutputRegister(), i.InputRegister(0), i.InputImmediate(1));
          DCHECK_EQ(LeaveRC, i.OutputRCBit());
        }
#if V8_TARGET_ARCH_PPC64
      }
#endif
      break;
    case kPPC_SubWithOverflow32:
      ASSEMBLE_SUB_WITH_OVERFLOW32();
      break;
    case kPPC_SubDouble:
      ASSEMBLE_FLOAT_BINOP_RC(fsub, MiscField::decode(instr->opcode()));
      break;
    case kPPC_Mul32:
      __ mullw(i.OutputRegister(), i.InputRegister(0), i.InputRegister(1),
               LeaveOE, i.OutputRCBit());
      break;
#if V8_TARGET_ARCH_PPC64
    case kPPC_Mul64:
      __ mulld(i.OutputRegister(), i.InputRegister(0), i.InputRegister(1),
               LeaveOE, i.OutputRCBit());
      break;
#endif

    case kPPC_Mul32WithHigh32:
      if (i.OutputRegister(0).is(i.InputRegister(0)) ||
          i.OutputRegister(0).is(i.InputRegister(1)) ||
          i.OutputRegister(1).is(i.InputRegister(0)) ||
          i.OutputRegister(1).is(i.InputRegister(1))) {
        __ mullw(kScratchReg,
                 i.InputRegister(0), i.InputRegister(1));  // low
        __ mulhw(i.OutputRegister(1),
                 i.InputRegister(0), i.InputRegister(1));  // high
        __ mr(i.OutputRegister(0), kScratchReg);
      } else {
        __ mullw(i.OutputRegister(0),
                 i.InputRegister(0), i.InputRegister(1));  // low
        __ mulhw(i.OutputRegister(1),
                 i.InputRegister(0), i.InputRegister(1));  // high
      }
      break;
    case kPPC_MulHigh32:
      __ mulhw(i.OutputRegister(), i.InputRegister(0), i.InputRegister(1),
               i.OutputRCBit());
      break;
    case kPPC_MulHighU32:
      __ mulhwu(i.OutputRegister(), i.InputRegister(0), i.InputRegister(1),
                i.OutputRCBit());
      break;
    case kPPC_MulDouble:
      ASSEMBLE_FLOAT_BINOP_RC(fmul, MiscField::decode(instr->opcode()));
      break;
    case kPPC_Div32:
      __ divw(i.OutputRegister(), i.InputRegister(0), i.InputRegister(1));
      DCHECK_EQ(LeaveRC, i.OutputRCBit());
      break;
#if V8_TARGET_ARCH_PPC64
    case kPPC_Div64:
      __ divd(i.OutputRegister(), i.InputRegister(0), i.InputRegister(1));
      DCHECK_EQ(LeaveRC, i.OutputRCBit());
      break;
#endif
    case kPPC_DivU32:
      __ divwu(i.OutputRegister(), i.InputRegister(0), i.InputRegister(1));
      DCHECK_EQ(LeaveRC, i.OutputRCBit());
      break;
#if V8_TARGET_ARCH_PPC64
    case kPPC_DivU64:
      __ divdu(i.OutputRegister(), i.InputRegister(0), i.InputRegister(1));
      DCHECK_EQ(LeaveRC, i.OutputRCBit());
      break;
#endif
    case kPPC_DivDouble:
      ASSEMBLE_FLOAT_BINOP_RC(fdiv, MiscField::decode(instr->opcode()));
      break;
    case kPPC_Mod32:
      ASSEMBLE_MODULO(divw, mullw);
      break;
#if V8_TARGET_ARCH_PPC64
    case kPPC_Mod64:
      ASSEMBLE_MODULO(divd, mulld);
      break;
#endif
    case kPPC_ModU32:
      ASSEMBLE_MODULO(divwu, mullw);
      break;
#if V8_TARGET_ARCH_PPC64
    case kPPC_ModU64:
      ASSEMBLE_MODULO(divdu, mulld);
      break;
#endif
    case kPPC_ModDouble:
      // TODO(bmeurer): We should really get rid of this special instruction,
      // and generate a CallAddress instruction instead.
      ASSEMBLE_FLOAT_MODULO();
      break;
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
    case kIeee754Float64Atan2:
      ASSEMBLE_IEEE754_BINOP(atan2);
      break;
    case kIeee754Float64Atanh:
      ASSEMBLE_IEEE754_UNOP(atanh);
      break;
    case kIeee754Float64Tan:
      ASSEMBLE_IEEE754_UNOP(tan);
      break;
    case kIeee754Float64Tanh:
      ASSEMBLE_IEEE754_UNOP(tanh);
      break;
    case kIeee754Float64Cbrt:
      ASSEMBLE_IEEE754_UNOP(cbrt);
      break;
    case kIeee754Float64Sin:
      ASSEMBLE_IEEE754_UNOP(sin);
      break;
    case kIeee754Float64Sinh:
      ASSEMBLE_IEEE754_UNOP(sinh);
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
      MathPowStub stub(isolate(), MathPowStub::DOUBLE);
      __ CallStub(&stub);
      __ Move(d1, d3);
      break;
    }
    case kPPC_Neg:
      __ neg(i.OutputRegister(), i.InputRegister(0), LeaveOE, i.OutputRCBit());
      break;
    case kPPC_MaxDouble:
      ASSEMBLE_FLOAT_MAX();
      break;
    case kPPC_MinDouble:
      ASSEMBLE_FLOAT_MIN();
      break;
    case kPPC_AbsDouble:
      ASSEMBLE_FLOAT_UNOP_RC(fabs, 0);
      break;
    case kPPC_SqrtDouble:
      ASSEMBLE_FLOAT_UNOP_RC(fsqrt, MiscField::decode(instr->opcode()));
      break;
    case kPPC_FloorDouble:
      ASSEMBLE_FLOAT_UNOP_RC(frim, MiscField::decode(instr->opcode()));
      break;
    case kPPC_CeilDouble:
      ASSEMBLE_FLOAT_UNOP_RC(frip, MiscField::decode(instr->opcode()));
      break;
    case kPPC_TruncateDouble:
      ASSEMBLE_FLOAT_UNOP_RC(friz, MiscField::decode(instr->opcode()));
      break;
    case kPPC_RoundDouble:
      ASSEMBLE_FLOAT_UNOP_RC(frin, MiscField::decode(instr->opcode()));
      break;
    case kPPC_NegDouble:
      ASSEMBLE_FLOAT_UNOP_RC(fneg, 0);
      break;
    case kPPC_Cntlz32:
      __ cntlzw_(i.OutputRegister(), i.InputRegister(0));
      DCHECK_EQ(LeaveRC, i.OutputRCBit());
      break;
#if V8_TARGET_ARCH_PPC64
    case kPPC_Cntlz64:
      __ cntlzd_(i.OutputRegister(), i.InputRegister(0));
      DCHECK_EQ(LeaveRC, i.OutputRCBit());
      break;
#endif
    case kPPC_Popcnt32:
      __ popcntw(i.OutputRegister(), i.InputRegister(0));
      DCHECK_EQ(LeaveRC, i.OutputRCBit());
      break;
#if V8_TARGET_ARCH_PPC64
    case kPPC_Popcnt64:
      __ popcntd(i.OutputRegister(), i.InputRegister(0));
      DCHECK_EQ(LeaveRC, i.OutputRCBit());
      break;
#endif
    case kPPC_Cmp32:
      ASSEMBLE_COMPARE(cmpw, cmplw);
      break;
#if V8_TARGET_ARCH_PPC64
    case kPPC_Cmp64:
      ASSEMBLE_COMPARE(cmp, cmpl);
      break;
#endif
    case kPPC_CmpDouble:
      ASSEMBLE_FLOAT_COMPARE(fcmpu);
      break;
    case kPPC_Tst32:
      if (HasRegisterInput(instr, 1)) {
        __ and_(r0, i.InputRegister(0), i.InputRegister(1), i.OutputRCBit());
      } else {
        __ andi(r0, i.InputRegister(0), i.InputImmediate(1));
      }
#if V8_TARGET_ARCH_PPC64
      __ extsw(r0, r0, i.OutputRCBit());
#endif
      DCHECK_EQ(SetRC, i.OutputRCBit());
      break;
#if V8_TARGET_ARCH_PPC64
    case kPPC_Tst64:
      if (HasRegisterInput(instr, 1)) {
        __ and_(r0, i.InputRegister(0), i.InputRegister(1), i.OutputRCBit());
      } else {
        __ andi(r0, i.InputRegister(0), i.InputImmediate(1));
      }
      DCHECK_EQ(SetRC, i.OutputRCBit());
      break;
#endif
    case kPPC_Float64SilenceNaN: {
      DoubleRegister value = i.InputDoubleRegister(0);
      DoubleRegister result = i.OutputDoubleRegister();
      __ CanonicalizeNaN(result, value);
      break;
    }
    case kPPC_Push:
      if (instr->InputAt(0)->IsFPRegister()) {
        __ stfdu(i.InputDoubleRegister(0), MemOperand(sp, -kDoubleSize));
        frame_access_state()->IncreaseSPDelta(kDoubleSize / kPointerSize);
      } else {
        __ Push(i.InputRegister(0));
        frame_access_state()->IncreaseSPDelta(1);
      }
      DCHECK_EQ(LeaveRC, i.OutputRCBit());
      break;
    case kPPC_PushFrame: {
      int num_slots = i.InputInt32(1);
      if (instr->InputAt(0)->IsFPRegister()) {
        LocationOperand* op = LocationOperand::cast(instr->InputAt(0));
        if (op->representation() == MachineRepresentation::kFloat64) {
          __ StoreDoubleU(i.InputDoubleRegister(0),
                        MemOperand(sp, -num_slots * kPointerSize), r0);
        } else {
          DCHECK(op->representation() == MachineRepresentation::kFloat32);
          __ StoreSingleU(i.InputDoubleRegister(0),
                        MemOperand(sp, -num_slots * kPointerSize), r0);
        }
      } else {
        __ StorePU(i.InputRegister(0),
                   MemOperand(sp, -num_slots * kPointerSize), r0);
      }
      break;
    }
    case kPPC_StoreToStackSlot: {
      int slot = i.InputInt32(1);
      if (instr->InputAt(0)->IsFPRegister()) {
        LocationOperand* op = LocationOperand::cast(instr->InputAt(0));
        if (op->representation() == MachineRepresentation::kFloat64) {
          __ StoreDouble(i.InputDoubleRegister(0),
                        MemOperand(sp, slot * kPointerSize), r0);
        } else {
          DCHECK(op->representation() == MachineRepresentation::kFloat32);
          __ StoreSingle(i.InputDoubleRegister(0),
                        MemOperand(sp, slot * kPointerSize), r0);
        }
      } else {
        __ StoreP(i.InputRegister(0), MemOperand(sp, slot * kPointerSize), r0);
      }
      break;
    }
    case kPPC_ExtendSignWord8:
      __ extsb(i.OutputRegister(), i.InputRegister(0));
      DCHECK_EQ(LeaveRC, i.OutputRCBit());
      break;
    case kPPC_ExtendSignWord16:
      __ extsh(i.OutputRegister(), i.InputRegister(0));
      DCHECK_EQ(LeaveRC, i.OutputRCBit());
      break;
#if V8_TARGET_ARCH_PPC64
    case kPPC_ExtendSignWord32:
      __ extsw(i.OutputRegister(), i.InputRegister(0));
      DCHECK_EQ(LeaveRC, i.OutputRCBit());
      break;
    case kPPC_Uint32ToUint64:
      // Zero extend
      __ clrldi(i.OutputRegister(), i.InputRegister(0), Operand(32));
      DCHECK_EQ(LeaveRC, i.OutputRCBit());
      break;
    case kPPC_Int64ToInt32:
      __ extsw(i.OutputRegister(), i.InputRegister(0));
      DCHECK_EQ(LeaveRC, i.OutputRCBit());
      break;
    case kPPC_Int64ToFloat32:
      __ ConvertInt64ToFloat(i.InputRegister(0), i.OutputDoubleRegister());
      DCHECK_EQ(LeaveRC, i.OutputRCBit());
      break;
    case kPPC_Int64ToDouble:
      __ ConvertInt64ToDouble(i.InputRegister(0), i.OutputDoubleRegister());
      DCHECK_EQ(LeaveRC, i.OutputRCBit());
      break;
    case kPPC_Uint64ToFloat32:
      __ ConvertUnsignedInt64ToFloat(i.InputRegister(0),
                                     i.OutputDoubleRegister());
      DCHECK_EQ(LeaveRC, i.OutputRCBit());
      break;
    case kPPC_Uint64ToDouble:
      __ ConvertUnsignedInt64ToDouble(i.InputRegister(0),
                                      i.OutputDoubleRegister());
      DCHECK_EQ(LeaveRC, i.OutputRCBit());
      break;
#endif
    case kPPC_Int32ToFloat32:
      __ ConvertIntToFloat(i.InputRegister(0), i.OutputDoubleRegister());
      DCHECK_EQ(LeaveRC, i.OutputRCBit());
      break;
    case kPPC_Int32ToDouble:
      __ ConvertIntToDouble(i.InputRegister(0), i.OutputDoubleRegister());
      DCHECK_EQ(LeaveRC, i.OutputRCBit());
      break;
    case kPPC_Uint32ToFloat32:
      __ ConvertUnsignedIntToFloat(i.InputRegister(0),
                                   i.OutputDoubleRegister());
      DCHECK_EQ(LeaveRC, i.OutputRCBit());
      break;
    case kPPC_Uint32ToDouble:
      __ ConvertUnsignedIntToDouble(i.InputRegister(0),
                                    i.OutputDoubleRegister());
      DCHECK_EQ(LeaveRC, i.OutputRCBit());
      break;
    case kPPC_DoubleToInt32:
    case kPPC_DoubleToUint32:
    case kPPC_DoubleToInt64: {
#if V8_TARGET_ARCH_PPC64
      bool check_conversion =
          (opcode == kPPC_DoubleToInt64 && i.OutputCount() > 1);
      if (check_conversion) {
        __ mtfsb0(VXCVI);  // clear FPSCR:VXCVI bit
      }
#endif
      __ ConvertDoubleToInt64(i.InputDoubleRegister(0),
#if !V8_TARGET_ARCH_PPC64
                              kScratchReg,
#endif
                              i.OutputRegister(0), kScratchDoubleReg);
#if V8_TARGET_ARCH_PPC64
      if (check_conversion) {
        // Set 2nd output to zero if conversion fails.
        CRegister cr = cr7;
        int crbit = v8::internal::Assembler::encode_crbit(
            cr, static_cast<CRBit>(VXCVI % CRWIDTH));
        __ mcrfs(cr, VXCVI);  // extract FPSCR field containing VXCVI into cr7
        if (CpuFeatures::IsSupported(ISELECT)) {
          __ li(i.OutputRegister(1), Operand(1));
          __ isel(i.OutputRegister(1), r0, i.OutputRegister(1), crbit);
        } else {
          __ li(i.OutputRegister(1), Operand::Zero());
          __ bc(v8::internal::Assembler::kInstrSize * 2, BT, crbit);
          __ li(i.OutputRegister(1), Operand(1));
        }
      }
#endif
      DCHECK_EQ(LeaveRC, i.OutputRCBit());
      break;
    }
#if V8_TARGET_ARCH_PPC64
    case kPPC_DoubleToUint64: {
      bool check_conversion = (i.OutputCount() > 1);
      if (check_conversion) {
        __ mtfsb0(VXCVI);  // clear FPSCR:VXCVI bit
      }
      __ ConvertDoubleToUnsignedInt64(i.InputDoubleRegister(0),
                                      i.OutputRegister(0), kScratchDoubleReg);
      if (check_conversion) {
        // Set 2nd output to zero if conversion fails.
        CRegister cr = cr7;
        int crbit = v8::internal::Assembler::encode_crbit(
            cr, static_cast<CRBit>(VXCVI % CRWIDTH));
        __ mcrfs(cr, VXCVI);  // extract FPSCR field containing VXCVI into cr7
        if (CpuFeatures::IsSupported(ISELECT)) {
          __ li(i.OutputRegister(1), Operand(1));
          __ isel(i.OutputRegister(1), r0, i.OutputRegister(1), crbit);
        } else {
          __ li(i.OutputRegister(1), Operand::Zero());
          __ bc(v8::internal::Assembler::kInstrSize * 2, BT, crbit);
          __ li(i.OutputRegister(1), Operand(1));
        }
      }
      DCHECK_EQ(LeaveRC, i.OutputRCBit());
      break;
    }
#endif
    case kPPC_DoubleToFloat32:
      ASSEMBLE_FLOAT_UNOP_RC(frsp, 0);
      break;
    case kPPC_Float32ToDouble:
      // Nothing to do.
      __ Move(i.OutputDoubleRegister(), i.InputDoubleRegister(0));
      DCHECK_EQ(LeaveRC, i.OutputRCBit());
      break;
    case kPPC_DoubleExtractLowWord32:
      __ MovDoubleLowToInt(i.OutputRegister(), i.InputDoubleRegister(0));
      DCHECK_EQ(LeaveRC, i.OutputRCBit());
      break;
    case kPPC_DoubleExtractHighWord32:
      __ MovDoubleHighToInt(i.OutputRegister(), i.InputDoubleRegister(0));
      DCHECK_EQ(LeaveRC, i.OutputRCBit());
      break;
    case kPPC_DoubleInsertLowWord32:
      __ InsertDoubleLow(i.OutputDoubleRegister(), i.InputRegister(1), r0);
      DCHECK_EQ(LeaveRC, i.OutputRCBit());
      break;
    case kPPC_DoubleInsertHighWord32:
      __ InsertDoubleHigh(i.OutputDoubleRegister(), i.InputRegister(1), r0);
      DCHECK_EQ(LeaveRC, i.OutputRCBit());
      break;
    case kPPC_DoubleConstruct:
#if V8_TARGET_ARCH_PPC64
      __ MovInt64ComponentsToDouble(i.OutputDoubleRegister(),
                                    i.InputRegister(0), i.InputRegister(1), r0);
#else
      __ MovInt64ToDouble(i.OutputDoubleRegister(), i.InputRegister(0),
                          i.InputRegister(1));
#endif
      DCHECK_EQ(LeaveRC, i.OutputRCBit());
      break;
    case kPPC_BitcastFloat32ToInt32:
      __ MovFloatToInt(i.OutputRegister(), i.InputDoubleRegister(0));
      break;
    case kPPC_BitcastInt32ToFloat32:
      __ MovIntToFloat(i.OutputDoubleRegister(), i.InputRegister(0));
      break;
#if V8_TARGET_ARCH_PPC64
    case kPPC_BitcastDoubleToInt64:
      __ MovDoubleToInt64(i.OutputRegister(), i.InputDoubleRegister(0));
      break;
    case kPPC_BitcastInt64ToDouble:
      __ MovInt64ToDouble(i.OutputDoubleRegister(), i.InputRegister(0));
      break;
#endif
    case kPPC_LoadWordU8:
      ASSEMBLE_LOAD_INTEGER(lbz, lbzx);
      break;
    case kPPC_LoadWordS8:
      ASSEMBLE_LOAD_INTEGER(lbz, lbzx);
      __ extsb(i.OutputRegister(), i.OutputRegister());
      break;
    case kPPC_LoadWordU16:
      ASSEMBLE_LOAD_INTEGER(lhz, lhzx);
      break;
    case kPPC_LoadWordS16:
      ASSEMBLE_LOAD_INTEGER(lha, lhax);
      break;
    case kPPC_LoadWordU32:
      ASSEMBLE_LOAD_INTEGER(lwz, lwzx);
      break;
    case kPPC_LoadWordS32:
      ASSEMBLE_LOAD_INTEGER(lwa, lwax);
      break;
#if V8_TARGET_ARCH_PPC64
    case kPPC_LoadWord64:
      ASSEMBLE_LOAD_INTEGER(ld, ldx);
      break;
#endif
    case kPPC_LoadFloat32:
      ASSEMBLE_LOAD_FLOAT(lfs, lfsx);
      break;
    case kPPC_LoadDouble:
      ASSEMBLE_LOAD_FLOAT(lfd, lfdx);
      break;
    case kPPC_StoreWord8:
      ASSEMBLE_STORE_INTEGER(stb, stbx);
      break;
    case kPPC_StoreWord16:
      ASSEMBLE_STORE_INTEGER(sth, sthx);
      break;
    case kPPC_StoreWord32:
      ASSEMBLE_STORE_INTEGER(stw, stwx);
      break;
#if V8_TARGET_ARCH_PPC64
    case kPPC_StoreWord64:
      ASSEMBLE_STORE_INTEGER(std, stdx);
      break;
#endif
    case kPPC_StoreFloat32:
      ASSEMBLE_STORE_FLOAT32();
      break;
    case kPPC_StoreDouble:
      ASSEMBLE_STORE_DOUBLE();
      break;
    case kCheckedLoadInt8:
      ASSEMBLE_CHECKED_LOAD_INTEGER(lbz, lbzx);
      __ extsb(i.OutputRegister(), i.OutputRegister());
      break;
    case kCheckedLoadUint8:
      ASSEMBLE_CHECKED_LOAD_INTEGER(lbz, lbzx);
      break;
    case kCheckedLoadInt16:
      ASSEMBLE_CHECKED_LOAD_INTEGER(lha, lhax);
      break;
    case kCheckedLoadUint16:
      ASSEMBLE_CHECKED_LOAD_INTEGER(lhz, lhzx);
      break;
    case kCheckedLoadWord32:
      ASSEMBLE_CHECKED_LOAD_INTEGER(lwz, lwzx);
      break;
    case kCheckedLoadWord64:
#if V8_TARGET_ARCH_PPC64
      ASSEMBLE_CHECKED_LOAD_INTEGER(ld, ldx);
#else
      UNREACHABLE();
#endif
      break;
    case kCheckedLoadFloat32:
      ASSEMBLE_CHECKED_LOAD_FLOAT(lfs, lfsx, 32);
      break;
    case kCheckedLoadFloat64:
      ASSEMBLE_CHECKED_LOAD_FLOAT(lfd, lfdx, 64);
      break;
    case kCheckedStoreWord8:
      ASSEMBLE_CHECKED_STORE_INTEGER(stb, stbx);
      break;
    case kCheckedStoreWord16:
      ASSEMBLE_CHECKED_STORE_INTEGER(sth, sthx);
      break;
    case kCheckedStoreWord32:
      ASSEMBLE_CHECKED_STORE_INTEGER(stw, stwx);
      break;
    case kCheckedStoreWord64:
#if V8_TARGET_ARCH_PPC64
      ASSEMBLE_CHECKED_STORE_INTEGER(std, stdx);
#else
      UNREACHABLE();
#endif
      break;
    case kCheckedStoreFloat32:
      ASSEMBLE_CHECKED_STORE_FLOAT32();
      break;
    case kCheckedStoreFloat64:
      ASSEMBLE_CHECKED_STORE_DOUBLE();
      break;

    case kAtomicLoadInt8:
      ASSEMBLE_ATOMIC_LOAD_INTEGER(lbz, lbzx);
      __ extsb(i.OutputRegister(), i.OutputRegister());
      break;
    case kAtomicLoadUint8:
      ASSEMBLE_ATOMIC_LOAD_INTEGER(lbz, lbzx);
      break;
    case kAtomicLoadInt16:
      ASSEMBLE_ATOMIC_LOAD_INTEGER(lha, lhax);
      break;
    case kAtomicLoadUint16:
      ASSEMBLE_ATOMIC_LOAD_INTEGER(lhz, lhzx);
      break;
    case kAtomicLoadWord32:
      ASSEMBLE_ATOMIC_LOAD_INTEGER(lwz, lwzx);
      break;

    case kAtomicStoreWord8:
      ASSEMBLE_ATOMIC_STORE_INTEGER(stb, stbx);
      break;
    case kAtomicStoreWord16:
      ASSEMBLE_ATOMIC_STORE_INTEGER(sth, sthx);
      break;
    case kAtomicStoreWord32:
      ASSEMBLE_ATOMIC_STORE_INTEGER(stw, stwx);
      break;
    default:
      UNREACHABLE();
      break;
  }
  return kSuccess;
}  // NOLINT(readability/fn_size)


// Assembles branches after an instruction.
void CodeGenerator::AssembleArchBranch(Instruction* instr, BranchInfo* branch) {
  PPCOperandConverter i(this, instr);
  Label* tlabel = branch->true_label;
  Label* flabel = branch->false_label;
  ArchOpcode op = instr->arch_opcode();
  FlagsCondition condition = branch->condition;
  CRegister cr = cr0;

  Condition cond = FlagsConditionToCondition(condition, op);
  if (op == kPPC_CmpDouble) {
    // check for unordered if necessary
    if (cond == le) {
      __ bunordered(flabel, cr);
      // Unnecessary for eq/lt since only FU bit will be set.
    } else if (cond == gt) {
      __ bunordered(tlabel, cr);
      // Unnecessary for ne/ge since only FU bit will be set.
    }
  }
  __ b(cond, tlabel, cr);
  if (!branch->fallthru) __ b(flabel);  // no fallthru to flabel.
}


void CodeGenerator::AssembleArchJump(RpoNumber target) {
  if (!IsNextInAssemblyOrder(target)) __ b(GetLabel(target));
}


// Assembles boolean materializations after an instruction.
void CodeGenerator::AssembleArchBoolean(Instruction* instr,
                                        FlagsCondition condition) {
  PPCOperandConverter i(this, instr);
  Label done;
  ArchOpcode op = instr->arch_opcode();
  CRegister cr = cr0;
  int reg_value = -1;

  // Materialize a full 32-bit 1 or 0 value. The result register is always the
  // last output of the instruction.
  DCHECK_NE(0u, instr->OutputCount());
  Register reg = i.OutputRegister(instr->OutputCount() - 1);

  Condition cond = FlagsConditionToCondition(condition, op);
  if (op == kPPC_CmpDouble) {
    // check for unordered if necessary
    if (cond == le) {
      reg_value = 0;
      __ li(reg, Operand::Zero());
      __ bunordered(&done, cr);
    } else if (cond == gt) {
      reg_value = 1;
      __ li(reg, Operand(1));
      __ bunordered(&done, cr);
    }
    // Unnecessary for eq/lt & ne/ge since only FU bit will be set.
  }

  if (CpuFeatures::IsSupported(ISELECT)) {
    switch (cond) {
      case eq:
      case lt:
      case gt:
        if (reg_value != 1) __ li(reg, Operand(1));
        __ li(kScratchReg, Operand::Zero());
        __ isel(cond, reg, reg, kScratchReg, cr);
        break;
      case ne:
      case ge:
      case le:
        if (reg_value != 1) __ li(reg, Operand(1));
        // r0 implies logical zero in this form
        __ isel(NegateCondition(cond), reg, r0, reg, cr);
        break;
    default:
      UNREACHABLE();
      break;
    }
  } else {
    if (reg_value != 0) __ li(reg, Operand::Zero());
    __ b(NegateCondition(cond), &done, cr);
    __ li(reg, Operand(1));
  }
  __ bind(&done);
}


void CodeGenerator::AssembleArchLookupSwitch(Instruction* instr) {
  PPCOperandConverter i(this, instr);
  Register input = i.InputRegister(0);
  for (size_t index = 2; index < instr->InputCount(); index += 2) {
    __ Cmpwi(input, Operand(i.InputInt32(index + 0)), r0);
    __ beq(GetLabel(i.InputRpo(index + 1)));
  }
  AssembleArchJump(i.InputRpo(1));
}


void CodeGenerator::AssembleArchTableSwitch(Instruction* instr) {
  PPCOperandConverter i(this, instr);
  Register input = i.InputRegister(0);
  int32_t const case_count = static_cast<int32_t>(instr->InputCount() - 2);
  Label** cases = zone()->NewArray<Label*>(case_count);
  for (int32_t index = 0; index < case_count; ++index) {
    cases[index] = GetLabel(i.InputRpo(index + 2));
  }
  Label* const table = AddJumpTable(cases, case_count);
  __ Cmpli(input, Operand(case_count), r0);
  __ bge(GetLabel(i.InputRpo(1)));
  __ mov_label_addr(kScratchReg, table);
  __ ShiftLeftImm(r0, input, Operand(kPointerSizeLog2));
  __ LoadPX(kScratchReg, MemOperand(kScratchReg, r0));
  __ Jump(kScratchReg);
}

CodeGenerator::CodeGenResult CodeGenerator::AssembleDeoptimizerCall(
    int deoptimization_id, Deoptimizer::BailoutType bailout_type,
    SourcePosition pos) {
  Address deopt_entry = Deoptimizer::GetDeoptimizationEntry(
      isolate(), deoptimization_id, bailout_type);
  // TODO(turbofan): We should be able to generate better code by sharing the
  // actual final call site and just bl'ing to it here, similar to what we do
  // in the lithium backend.
  if (deopt_entry == nullptr) return kTooManyDeoptimizationBailouts;
  DeoptimizeReason deoptimization_reason =
      GetDeoptimizationReason(deoptimization_id);
  __ RecordDeoptReason(deoptimization_reason, pos.raw(), deoptimization_id);
  __ Call(deopt_entry, RelocInfo::RUNTIME_ENTRY);
  return kSuccess;
}

void CodeGenerator::FinishFrame(Frame* frame) {
  CallDescriptor* descriptor = linkage()->GetIncomingDescriptor();
  const RegList double_saves = descriptor->CalleeSavedFPRegisters();

  // Save callee-saved Double registers.
  if (double_saves != 0) {
    frame->AlignSavedCalleeRegisterSlots();
    DCHECK(kNumCalleeSavedDoubles ==
           base::bits::CountPopulation32(double_saves));
    frame->AllocateSavedCalleeRegisterSlots(kNumCalleeSavedDoubles *
                                             (kDoubleSize / kPointerSize));
  }
  // Save callee-saved registers.
  const RegList saves =
      FLAG_enable_embedded_constant_pool
          ? descriptor->CalleeSavedRegisters() & ~kConstantPoolRegister.bit()
          : descriptor->CalleeSavedRegisters();
  if (saves != 0) {
    // register save area does not include the fp or constant pool pointer.
    const int num_saves =
        kNumCalleeSaved - 1 - (FLAG_enable_embedded_constant_pool ? 1 : 0);
    DCHECK(num_saves == base::bits::CountPopulation32(saves));
    frame->AllocateSavedCalleeRegisterSlots(num_saves);
  }
}

void CodeGenerator::AssembleConstructFrame() {
  CallDescriptor* descriptor = linkage()->GetIncomingDescriptor();
  if (frame_access_state()->has_frame()) {
    if (descriptor->IsCFunctionCall()) {
      __ function_descriptor();
      __ mflr(r0);
      if (FLAG_enable_embedded_constant_pool) {
        __ Push(r0, fp, kConstantPoolRegister);
        // Adjust FP to point to saved FP.
        __ subi(fp, sp, Operand(StandardFrameConstants::kConstantPoolOffset));
      } else {
        __ Push(r0, fp);
        __ mr(fp, sp);
      }
    } else if (descriptor->IsJSFunctionCall()) {
      __ Prologue(this->info()->GeneratePreagedPrologue(), ip);
    } else {
      StackFrame::Type type = info()->GetOutputStackFrameType();
      // TODO(mbrandy): Detect cases where ip is the entrypoint (for
      // efficient intialization of the constant pool pointer register).
      __ StubPrologue(type);
    }
  }

  int shrink_slots = frame()->GetSpillSlotCount();
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

  const RegList double_saves = descriptor->CalleeSavedFPRegisters();
  if (shrink_slots > 0) {
    __ Add(sp, sp, -shrink_slots * kPointerSize, r0);
  }

  // Save callee-saved Double registers.
  if (double_saves != 0) {
    __ MultiPushDoubles(double_saves);
    DCHECK(kNumCalleeSavedDoubles ==
           base::bits::CountPopulation32(double_saves));
  }

  // Save callee-saved registers.
  const RegList saves =
      FLAG_enable_embedded_constant_pool
          ? descriptor->CalleeSavedRegisters() & ~kConstantPoolRegister.bit()
          : descriptor->CalleeSavedRegisters();
  if (saves != 0) {
    __ MultiPush(saves);
    // register save area does not include the fp or constant pool pointer.
  }
}


void CodeGenerator::AssembleReturn() {
  CallDescriptor* descriptor = linkage()->GetIncomingDescriptor();
  int pop_count = static_cast<int>(descriptor->StackParameterCount());

  // Restore registers.
  const RegList saves =
      FLAG_enable_embedded_constant_pool
          ? descriptor->CalleeSavedRegisters() & ~kConstantPoolRegister.bit()
          : descriptor->CalleeSavedRegisters();
  if (saves != 0) {
    __ MultiPop(saves);
  }

  // Restore double registers.
  const RegList double_saves = descriptor->CalleeSavedFPRegisters();
  if (double_saves != 0) {
    __ MultiPopDoubles(double_saves);
  }

  if (descriptor->IsCFunctionCall()) {
    AssembleDeconstructFrame();
  } else if (frame_access_state()->has_frame()) {
    // Canonicalize JSFunction return sites for now.
    if (return_label_.is_bound()) {
      __ b(&return_label_);
      return;
    } else {
      __ bind(&return_label_);
      AssembleDeconstructFrame();
    }
  }
  __ Ret(pop_count);
}


void CodeGenerator::AssembleMove(InstructionOperand* source,
                                 InstructionOperand* destination) {
  PPCOperandConverter g(this, nullptr);
  // Dispatch on the source and destination operand kinds.  Not all
  // combinations are possible.
  if (source->IsRegister()) {
    DCHECK(destination->IsRegister() || destination->IsStackSlot());
    Register src = g.ToRegister(source);
    if (destination->IsRegister()) {
      __ Move(g.ToRegister(destination), src);
    } else {
      __ StoreP(src, g.ToMemOperand(destination), r0);
    }
  } else if (source->IsStackSlot()) {
    DCHECK(destination->IsRegister() || destination->IsStackSlot());
    MemOperand src = g.ToMemOperand(source);
    if (destination->IsRegister()) {
      __ LoadP(g.ToRegister(destination), src, r0);
    } else {
      Register temp = kScratchReg;
      __ LoadP(temp, src, r0);
      __ StoreP(temp, g.ToMemOperand(destination), r0);
    }
  } else if (source->IsConstant()) {
    Constant src = g.ToConstant(source);
    if (destination->IsRegister() || destination->IsStackSlot()) {
      Register dst =
          destination->IsRegister() ? g.ToRegister(destination) : kScratchReg;
      switch (src.type()) {
        case Constant::kInt32:
#if V8_TARGET_ARCH_PPC64
          if (src.rmode() == RelocInfo::WASM_MEMORY_SIZE_REFERENCE) {
#else
          if (src.rmode() == RelocInfo::WASM_MEMORY_REFERENCE ||
              src.rmode() == RelocInfo::WASM_GLOBAL_REFERENCE ||
              src.rmode() == RelocInfo::WASM_MEMORY_SIZE_REFERENCE) {
#endif
            __ mov(dst, Operand(src.ToInt32(), src.rmode()));
          } else {
            __ mov(dst, Operand(src.ToInt32()));
          }
          break;
        case Constant::kInt64:
#if V8_TARGET_ARCH_PPC64
          if (src.rmode() == RelocInfo::WASM_MEMORY_REFERENCE ||
              src.rmode() == RelocInfo::WASM_GLOBAL_REFERENCE) {
            __ mov(dst, Operand(src.ToInt64(), src.rmode()));
          } else {
            DCHECK(src.rmode() != RelocInfo::WASM_MEMORY_SIZE_REFERENCE);
#endif
            __ mov(dst, Operand(src.ToInt64()));
#if V8_TARGET_ARCH_PPC64
          }
#endif
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
          if (IsMaterializableFromRoot(src_object, &index)) {
            __ LoadRoot(dst, index);
          } else {
            __ Move(dst, src_object);
          }
          break;
        }
        case Constant::kRpoNumber:
          UNREACHABLE();  // TODO(dcarney): loading RPO constants on PPC.
          break;
      }
      if (destination->IsStackSlot()) {
        __ StoreP(dst, g.ToMemOperand(destination), r0);
      }
    } else {
      DoubleRegister dst = destination->IsFPRegister()
                               ? g.ToDoubleRegister(destination)
                               : kScratchDoubleReg;
      double value = (src.type() == Constant::kFloat32) ? src.ToFloat32()
                                                        : src.ToFloat64();
      __ LoadDoubleLiteral(dst, value, kScratchReg);
      if (destination->IsFPStackSlot()) {
        __ StoreDouble(dst, g.ToMemOperand(destination), r0);
      }
    }
  } else if (source->IsFPRegister()) {
    DoubleRegister src = g.ToDoubleRegister(source);
    if (destination->IsFPRegister()) {
      DoubleRegister dst = g.ToDoubleRegister(destination);
      __ Move(dst, src);
    } else {
      DCHECK(destination->IsFPStackSlot());
      LocationOperand* op = LocationOperand::cast(source);
      if (op->representation() == MachineRepresentation::kFloat64) {
        __ StoreDouble(src, g.ToMemOperand(destination), r0);
      } else {
        __ StoreSingle(src, g.ToMemOperand(destination), r0);
      }
    }
  } else if (source->IsFPStackSlot()) {
    DCHECK(destination->IsFPRegister() || destination->IsFPStackSlot());
    MemOperand src = g.ToMemOperand(source);
    if (destination->IsFPRegister()) {
      LocationOperand* op = LocationOperand::cast(source);
      if (op->representation() == MachineRepresentation::kFloat64) {
        __ LoadDouble(g.ToDoubleRegister(destination), src, r0);
      } else {
        __ LoadSingle(g.ToDoubleRegister(destination), src, r0);
      }
    } else {
      LocationOperand* op = LocationOperand::cast(source);
      DoubleRegister temp = kScratchDoubleReg;
      if (op->representation() == MachineRepresentation::kFloat64) {
        __ LoadDouble(temp, src, r0);
        __ StoreDouble(temp, g.ToMemOperand(destination), r0);
      } else {
        __ LoadSingle(temp, src, r0);
        __ StoreSingle(temp, g.ToMemOperand(destination), r0);
      }
    }
  } else {
    UNREACHABLE();
  }
}


void CodeGenerator::AssembleSwap(InstructionOperand* source,
                                 InstructionOperand* destination) {
  PPCOperandConverter g(this, nullptr);
  // Dispatch on the source and destination operand kinds.  Not all
  // combinations are possible.
  if (source->IsRegister()) {
    // Register-register.
    Register temp = kScratchReg;
    Register src = g.ToRegister(source);
    if (destination->IsRegister()) {
      Register dst = g.ToRegister(destination);
      __ mr(temp, src);
      __ mr(src, dst);
      __ mr(dst, temp);
    } else {
      DCHECK(destination->IsStackSlot());
      MemOperand dst = g.ToMemOperand(destination);
      __ mr(temp, src);
      __ LoadP(src, dst);
      __ StoreP(temp, dst);
    }
#if V8_TARGET_ARCH_PPC64
  } else if (source->IsStackSlot() || source->IsFPStackSlot()) {
#else
  } else if (source->IsStackSlot()) {
    DCHECK(destination->IsStackSlot());
#endif
    Register temp_0 = kScratchReg;
    Register temp_1 = r0;
    MemOperand src = g.ToMemOperand(source);
    MemOperand dst = g.ToMemOperand(destination);
    __ LoadP(temp_0, src);
    __ LoadP(temp_1, dst);
    __ StoreP(temp_0, dst);
    __ StoreP(temp_1, src);
  } else if (source->IsFPRegister()) {
    DoubleRegister temp = kScratchDoubleReg;
    DoubleRegister src = g.ToDoubleRegister(source);
    if (destination->IsFPRegister()) {
      DoubleRegister dst = g.ToDoubleRegister(destination);
      __ fmr(temp, src);
      __ fmr(src, dst);
      __ fmr(dst, temp);
    } else {
      DCHECK(destination->IsFPStackSlot());
      MemOperand dst = g.ToMemOperand(destination);
      __ fmr(temp, src);
      __ lfd(src, dst);
      __ stfd(temp, dst);
    }
#if !V8_TARGET_ARCH_PPC64
  } else if (source->IsFPStackSlot()) {
    DCHECK(destination->IsFPStackSlot());
    DoubleRegister temp_0 = kScratchDoubleReg;
    DoubleRegister temp_1 = d0;
    MemOperand src = g.ToMemOperand(source);
    MemOperand dst = g.ToMemOperand(destination);
    __ lfd(temp_0, src);
    __ lfd(temp_1, dst);
    __ stfd(temp_0, dst);
    __ stfd(temp_1, src);
#endif
  } else {
    // No other combinations are possible.
    UNREACHABLE();
  }
}


void CodeGenerator::AssembleJumpTable(Label** targets, size_t target_count) {
  for (size_t index = 0; index < target_count; ++index) {
    __ emit_label_addr(targets[index]);
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
