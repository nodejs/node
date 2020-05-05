// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/backend/code-generator.h"

#include "src/codegen/assembler-inl.h"
#include "src/codegen/callable.h"
#include "src/codegen/macro-assembler.h"
#include "src/codegen/optimized-compilation-info.h"
#include "src/compiler/backend/code-generator-impl.h"
#include "src/compiler/backend/gap-resolver.h"
#include "src/compiler/node-matchers.h"
#include "src/compiler/osr.h"
#include "src/heap/heap-inl.h"  // crbug.com/v8/8499
#include "src/numbers/double.h"
#include "src/wasm/wasm-code-manager.h"
#include "src/wasm/wasm-objects.h"

namespace v8 {
namespace internal {
namespace compiler {

#define __ tasm()->

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
      case kFlags_branch_and_poison:
      case kFlags_deoptimize:
      case kFlags_deoptimize_and_poison:
      case kFlags_set:
      case kFlags_trap:
        return SetRC;
      case kFlags_none:
        return LeaveRC;
    }
    UNREACHABLE();
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
  }

  Operand InputImmediate(size_t index) {
    Constant constant = ToConstant(instr_->InputAt(index));
    switch (constant.type()) {
      case Constant::kInt32:
        return Operand(constant.ToInt32());
      case Constant::kFloat32:
        return Operand::EmbeddedNumber(constant.ToFloat32());
      case Constant::kFloat64:
        return Operand::EmbeddedNumber(constant.ToFloat64().value());
      case Constant::kInt64:
#if V8_TARGET_ARCH_PPC64
        return Operand(constant.ToInt64());
#endif
      case Constant::kExternalReference:
        return Operand(constant.ToExternalReference());
      case Constant::kDelayedStringConstant:
        return Operand::EmbeddedStringConstant(
            constant.ToDelayedStringConstant());
      case Constant::kCompressedHeapObject:
      case Constant::kHeapObject:
      case Constant::kRpoNumber:
        break;
    }
    UNREACHABLE();
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

class OutOfLineRecordWrite final : public OutOfLineCode {
 public:
  OutOfLineRecordWrite(CodeGenerator* gen, Register object, Register offset,
                       Register value, Register scratch0, Register scratch1,
                       RecordWriteMode mode, StubCallMode stub_mode,
                       UnwindingInfoWriter* unwinding_info_writer)
      : OutOfLineCode(gen),
        object_(object),
        offset_(offset),
        offset_immediate_(0),
        value_(value),
        scratch0_(scratch0),
        scratch1_(scratch1),
        mode_(mode),
        stub_mode_(stub_mode),
        must_save_lr_(!gen->frame_access_state()->has_frame()),
        unwinding_info_writer_(unwinding_info_writer),
        zone_(gen->zone()) {}

  OutOfLineRecordWrite(CodeGenerator* gen, Register object, int32_t offset,
                       Register value, Register scratch0, Register scratch1,
                       RecordWriteMode mode, StubCallMode stub_mode,
                       UnwindingInfoWriter* unwinding_info_writer)
      : OutOfLineCode(gen),
        object_(object),
        offset_(no_reg),
        offset_immediate_(offset),
        value_(value),
        scratch0_(scratch0),
        scratch1_(scratch1),
        mode_(mode),
        stub_mode_(stub_mode),
        must_save_lr_(!gen->frame_access_state()->has_frame()),
        unwinding_info_writer_(unwinding_info_writer),
        zone_(gen->zone()) {}

  void Generate() final {
    ConstantPoolUnavailableScope constant_pool_unavailable(tasm());
    if (mode_ > RecordWriteMode::kValueIsPointer) {
      __ JumpIfSmi(value_, exit());
    }
    __ CheckPageFlag(value_, scratch0_,
                     MemoryChunk::kPointersToHereAreInterestingMask, eq,
                     exit());
    if (offset_ == no_reg) {
      __ addi(scratch1_, object_, Operand(offset_immediate_));
    } else {
      DCHECK_EQ(0, offset_immediate_);
      __ add(scratch1_, object_, offset_);
    }
    RememberedSetAction const remembered_set_action =
        mode_ > RecordWriteMode::kValueIsMap ? EMIT_REMEMBERED_SET
                                             : OMIT_REMEMBERED_SET;
    SaveFPRegsMode const save_fp_mode =
        frame()->DidAllocateDoubleRegisters() ? kSaveFPRegs : kDontSaveFPRegs;
    if (must_save_lr_) {
      // We need to save and restore lr if the frame was elided.
      __ mflr(scratch0_);
      __ Push(scratch0_);
      unwinding_info_writer_->MarkLinkRegisterOnTopOfStack(__ pc_offset());
    }
    if (mode_ == RecordWriteMode::kValueIsEphemeronKey) {
      __ CallEphemeronKeyBarrier(object_, scratch1_, save_fp_mode);
    } else if (stub_mode_ == StubCallMode::kCallWasmRuntimeStub) {
      __ CallRecordWriteStub(object_, scratch1_, remembered_set_action,
                             save_fp_mode, wasm::WasmCode::kRecordWrite);
    } else {
      __ CallRecordWriteStub(object_, scratch1_, remembered_set_action,
                             save_fp_mode);
    }
    if (must_save_lr_) {
      // We need to save and restore lr if the frame was elided.
      __ Pop(scratch0_);
      __ mtlr(scratch0_);
      unwinding_info_writer_->MarkPopLinkRegisterFromTopOfStack(__ pc_offset());
    }
  }

 private:
  Register const object_;
  Register const offset_;
  int32_t const offset_immediate_;  // Valid if offset_ == no_reg.
  Register const value_;
  Register const scratch0_;
  Register const scratch1_;
  RecordWriteMode const mode_;
  StubCallMode stub_mode_;
  bool must_save_lr_;
  UnwindingInfoWriter* const unwinding_info_writer_;
  Zone* zone_;
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
        case kPPC_Add32:
        case kPPC_Add64:
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
        case kPPC_Add32:
        case kPPC_Add64:
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
}

void EmitWordLoadPoisoningIfNeeded(CodeGenerator* codegen, Instruction* instr,
                                   PPCOperandConverter const& i) {
  const MemoryAccessMode access_mode =
      static_cast<MemoryAccessMode>(MiscField::decode(instr->opcode()));
  if (access_mode == kMemoryAccessPoisoned) {
    Register value = i.OutputRegister();
    codegen->tasm()->and_(value, value, kSpeculationPoisonRegister);
  }
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

#define ASSEMBLE_FLOAT_MODULO()                                             \
  do {                                                                      \
    FrameScope scope(tasm(), StackFrame::MANUAL);                           \
    __ PrepareCallCFunction(0, 2, kScratchReg);                             \
    __ MovToFloatParameters(i.InputDoubleRegister(0),                       \
                            i.InputDoubleRegister(1));                      \
    __ CallCFunction(ExternalReference::mod_two_doubles_operation(), 0, 2); \
    __ MovFromFloatResult(i.OutputDoubleRegister());                        \
    DCHECK_EQ(LeaveRC, i.OutputRCBit());                                    \
  } while (0)

#define ASSEMBLE_IEEE754_UNOP(name)                                            \
  do {                                                                         \
    /* TODO(bmeurer): We should really get rid of this special instruction, */ \
    /* and generate a CallAddress instruction instead. */                      \
    FrameScope scope(tasm(), StackFrame::MANUAL);                              \
    __ PrepareCallCFunction(0, 1, kScratchReg);                                \
    __ MovToFloatParameter(i.InputDoubleRegister(0));                          \
    __ CallCFunction(ExternalReference::ieee754_##name##_function(), 0, 1);    \
    /* Move the result in the double result register. */                       \
    __ MovFromFloatResult(i.OutputDoubleRegister());                           \
    DCHECK_EQ(LeaveRC, i.OutputRCBit());                                       \
  } while (0)

#define ASSEMBLE_IEEE754_BINOP(name)                                           \
  do {                                                                         \
    /* TODO(bmeurer): We should really get rid of this special instruction, */ \
    /* and generate a CallAddress instruction instead. */                      \
    FrameScope scope(tasm(), StackFrame::MANUAL);                              \
    __ PrepareCallCFunction(0, 2, kScratchReg);                                \
    __ MovToFloatParameters(i.InputDoubleRegister(0),                          \
                            i.InputDoubleRegister(1));                         \
    __ CallCFunction(ExternalReference::ieee754_##name##_function(), 0, 2);    \
    /* Move the result in the double result register. */                       \
    __ MovFromFloatResult(i.OutputDoubleRegister());                           \
    DCHECK_EQ(LeaveRC, i.OutputRCBit());                                       \
  } while (0)

#define ASSEMBLE_FLOAT_MAX()                                           \
  do {                                                                 \
    DoubleRegister left_reg = i.InputDoubleRegister(0);                \
    DoubleRegister right_reg = i.InputDoubleRegister(1);               \
    DoubleRegister result_reg = i.OutputDoubleRegister();              \
    Label check_nan_left, check_zero, return_left, return_right, done; \
    __ fcmpu(left_reg, right_reg);                                     \
    __ bunordered(&check_nan_left);                                    \
    __ beq(&check_zero);                                               \
    __ bge(&return_left);                                              \
    __ b(&return_right);                                               \
                                                                       \
    __ bind(&check_zero);                                              \
    __ fcmpu(left_reg, kDoubleRegZero);                                \
    /* left == right != 0. */                                          \
    __ bne(&return_left);                                              \
    /* At this point, both left and right are either 0 or -0. */       \
    __ fadd(result_reg, left_reg, right_reg);                          \
    __ b(&done);                                                       \
                                                                       \
    __ bind(&check_nan_left);                                          \
    __ fcmpu(left_reg, left_reg);                                      \
    /* left == NaN. */                                                 \
    __ bunordered(&return_left);                                       \
    __ bind(&return_right);                                            \
    if (right_reg != result_reg) {                                     \
      __ fmr(result_reg, right_reg);                                   \
    }                                                                  \
    __ b(&done);                                                       \
                                                                       \
    __ bind(&return_left);                                             \
    if (left_reg != result_reg) {                                      \
      __ fmr(result_reg, left_reg);                                    \
    }                                                                  \
    __ bind(&done);                                                    \
  } while (0)

#define ASSEMBLE_FLOAT_MIN()                                              \
  do {                                                                    \
    DoubleRegister left_reg = i.InputDoubleRegister(0);                   \
    DoubleRegister right_reg = i.InputDoubleRegister(1);                  \
    DoubleRegister result_reg = i.OutputDoubleRegister();                 \
    Label check_nan_left, check_zero, return_left, return_right, done;    \
    __ fcmpu(left_reg, right_reg);                                        \
    __ bunordered(&check_nan_left);                                       \
    __ beq(&check_zero);                                                  \
    __ ble(&return_left);                                                 \
    __ b(&return_right);                                                  \
                                                                          \
    __ bind(&check_zero);                                                 \
    __ fcmpu(left_reg, kDoubleRegZero);                                   \
    /* left == right != 0. */                                             \
    __ bne(&return_left);                                                 \
    /* At this point, both left and right are either 0 or -0. */          \
    /* Min: The algorithm is: -((-L) + (-R)), which in case of L and R */ \
    /* being different registers is most efficiently expressed */         \
    /* as -((-L) - R). */                                                 \
    __ fneg(kScratchDoubleReg, left_reg);                                 \
    if (kScratchDoubleReg == right_reg) {                                 \
      __ fadd(result_reg, kScratchDoubleReg, right_reg);                  \
    } else {                                                              \
      __ fsub(result_reg, kScratchDoubleReg, right_reg);                  \
    }                                                                     \
    __ fneg(result_reg, result_reg);                                      \
    __ b(&done);                                                          \
                                                                          \
    __ bind(&check_nan_left);                                             \
    __ fcmpu(left_reg, left_reg);                                         \
    /* left == NaN. */                                                    \
    __ bunordered(&return_left);                                          \
                                                                          \
    __ bind(&return_right);                                               \
    if (right_reg != result_reg) {                                        \
      __ fmr(result_reg, right_reg);                                      \
    }                                                                     \
    __ b(&done);                                                          \
                                                                          \
    __ bind(&return_left);                                                \
    if (left_reg != result_reg) {                                         \
      __ fmr(result_reg, left_reg);                                       \
    }                                                                     \
    __ bind(&done);                                                       \
  } while (0)

#define ASSEMBLE_LOAD_FLOAT(asm_instr, asm_instrx)    \
  do {                                                \
    DoubleRegister result = i.OutputDoubleRegister(); \
    AddressingMode mode = kMode_None;                 \
    MemOperand operand = i.MemoryOperand(&mode);      \
    bool is_atomic = i.InputInt32(2);                 \
    if (mode == kMode_MRI) {                          \
      __ asm_instr(result, operand);                  \
    } else {                                          \
      __ asm_instrx(result, operand);                 \
    }                                                 \
    if (is_atomic) __ lwsync();                       \
    DCHECK_EQ(LeaveRC, i.OutputRCBit());              \
  } while (0)

#define ASSEMBLE_LOAD_INTEGER(asm_instr, asm_instrx) \
  do {                                               \
    Register result = i.OutputRegister();            \
    AddressingMode mode = kMode_None;                \
    MemOperand operand = i.MemoryOperand(&mode);     \
    bool is_atomic = i.InputInt32(2);                \
    if (mode == kMode_MRI) {                         \
      __ asm_instr(result, operand);                 \
    } else {                                         \
      __ asm_instrx(result, operand);                \
    }                                                \
    if (is_atomic) __ lwsync();                      \
    DCHECK_EQ(LeaveRC, i.OutputRCBit());             \
  } while (0)

#define ASSEMBLE_STORE_FLOAT(asm_instr, asm_instrx)      \
  do {                                                   \
    size_t index = 0;                                    \
    AddressingMode mode = kMode_None;                    \
    MemOperand operand = i.MemoryOperand(&mode, &index); \
    DoubleRegister value = i.InputDoubleRegister(index); \
    bool is_atomic = i.InputInt32(3);                    \
    if (is_atomic) __ lwsync();                          \
    /* removed frsp as instruction-selector checked */   \
    /* value to be kFloat32 */                           \
    if (mode == kMode_MRI) {                             \
      __ asm_instr(value, operand);                      \
    } else {                                             \
      __ asm_instrx(value, operand);                     \
    }                                                    \
    if (is_atomic) __ sync();                            \
    DCHECK_EQ(LeaveRC, i.OutputRCBit());                 \
  } while (0)

#define ASSEMBLE_STORE_INTEGER(asm_instr, asm_instrx)    \
  do {                                                   \
    size_t index = 0;                                    \
    AddressingMode mode = kMode_None;                    \
    MemOperand operand = i.MemoryOperand(&mode, &index); \
    Register value = i.InputRegister(index);             \
    bool is_atomic = i.InputInt32(3);                    \
    if (is_atomic) __ lwsync();                          \
    if (mode == kMode_MRI) {                             \
      __ asm_instr(value, operand);                      \
    } else {                                             \
      __ asm_instrx(value, operand);                     \
    }                                                    \
    if (is_atomic) __ sync();                            \
    DCHECK_EQ(LeaveRC, i.OutputRCBit());                 \
  } while (0)

#if V8_TARGET_ARCH_PPC64
// TODO(mbrandy): fix paths that produce garbage in offset's upper 32-bits.
#define CleanUInt32(x) __ ClearLeftImm(x, x, Operand(32))
#else
#define CleanUInt32(x)
#endif

#define ASSEMBLE_ATOMIC_EXCHANGE_INTEGER(load_instr, store_instr)       \
  do {                                                                  \
    Label exchange;                                                     \
    __ lwsync();                                                        \
    __ bind(&exchange);                                                 \
    __ load_instr(i.OutputRegister(0),                                  \
                  MemOperand(i.InputRegister(0), i.InputRegister(1)));  \
    __ store_instr(i.InputRegister(2),                                  \
                   MemOperand(i.InputRegister(0), i.InputRegister(1))); \
    __ bne(&exchange, cr0);                                             \
    __ sync();                                                          \
  } while (0)

#define ASSEMBLE_ATOMIC_BINOP(bin_inst, load_inst, store_inst)               \
  do {                                                                       \
    MemOperand operand = MemOperand(i.InputRegister(0), i.InputRegister(1)); \
    Label binop;                                                             \
    __ lwsync();                                                             \
    __ bind(&binop);                                                         \
    __ load_inst(i.OutputRegister(), operand);                               \
    __ bin_inst(kScratchReg, i.OutputRegister(), i.InputRegister(2));        \
    __ store_inst(kScratchReg, operand);                                     \
    __ bne(&binop, cr0);                                                     \
    __ sync();                                                               \
  } while (false)

#define ASSEMBLE_ATOMIC_BINOP_SIGN_EXT(bin_inst, load_inst, store_inst,      \
                                       ext_instr)                            \
  do {                                                                       \
    MemOperand operand = MemOperand(i.InputRegister(0), i.InputRegister(1)); \
    Label binop;                                                             \
    __ lwsync();                                                             \
    __ bind(&binop);                                                         \
    __ load_inst(i.OutputRegister(), operand);                               \
    __ ext_instr(i.OutputRegister(), i.OutputRegister());                    \
    __ bin_inst(kScratchReg, i.OutputRegister(), i.InputRegister(2));        \
    __ store_inst(kScratchReg, operand);                                     \
    __ bne(&binop, cr0);                                                     \
    __ sync();                                                               \
  } while (false)

#define ASSEMBLE_ATOMIC_COMPARE_EXCHANGE(cmp_inst, load_inst, store_inst,    \
                                         input_ext)                          \
  do {                                                                       \
    MemOperand operand = MemOperand(i.InputRegister(0), i.InputRegister(1)); \
    Label loop;                                                              \
    Label exit;                                                              \
    __ input_ext(r0, i.InputRegister(2));                                    \
    __ lwsync();                                                             \
    __ bind(&loop);                                                          \
    __ load_inst(i.OutputRegister(), operand);                               \
    __ cmp_inst(i.OutputRegister(), r0, cr0);                                \
    __ bne(&exit, cr0);                                                      \
    __ store_inst(i.InputRegister(3), operand);                              \
    __ bne(&loop, cr0);                                                      \
    __ bind(&exit);                                                          \
    __ sync();                                                               \
  } while (false)

#define ASSEMBLE_ATOMIC_COMPARE_EXCHANGE_SIGN_EXT(cmp_inst, load_inst,       \
                                                  store_inst, ext_instr)     \
  do {                                                                       \
    MemOperand operand = MemOperand(i.InputRegister(0), i.InputRegister(1)); \
    Label loop;                                                              \
    Label exit;                                                              \
    __ ext_instr(r0, i.InputRegister(2));                                    \
    __ lwsync();                                                             \
    __ bind(&loop);                                                          \
    __ load_inst(i.OutputRegister(), operand);                               \
    __ ext_instr(i.OutputRegister(), i.OutputRegister());                    \
    __ cmp_inst(i.OutputRegister(), r0, cr0);                                \
    __ bne(&exit, cr0);                                                      \
    __ store_inst(i.InputRegister(3), operand);                              \
    __ bne(&loop, cr0);                                                      \
    __ bind(&exit);                                                          \
    __ sync();                                                               \
  } while (false)

void CodeGenerator::AssembleDeconstructFrame() {
  __ LeaveFrame(StackFrame::MANUAL);
  unwinding_info_writer_.MarkFrameDeconstructed(__ pc_offset());
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
  __ cmpi(scratch1,
          Operand(StackFrame::TypeToMarker(StackFrame::ARGUMENTS_ADAPTOR)));
  __ bne(&done);

  // Load arguments count from current arguments adaptor frame (note, it
  // does not include receiver).
  Register caller_args_count_reg = scratch1;
  __ LoadP(caller_args_count_reg,
           MemOperand(fp, ArgumentsAdaptorFrameConstants::kLengthOffset));
  __ SmiUntag(caller_args_count_reg);

  __ PrepareForTailCall(args_reg, caller_args_count_reg, scratch2, scratch3);
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
      tasm->Push((*pending_pushes)[0]);
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
    tasm->Add(sp, sp, -stack_slot_delta * kSystemPointerSize, r0);
    state->IncreaseSPDelta(stack_slot_delta);
  } else if (allow_shrinkage && stack_slot_delta < 0) {
    if (pending_pushes != nullptr) {
      FlushPendingPushRegisters(tasm, state, pending_pushes);
    }
    tasm->Add(sp, sp, -stack_slot_delta * kSystemPointerSize, r0);
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
    PPCOperandConverter g(this, instr);
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
  Register scratch = kScratchReg;
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
  if (FLAG_debug_code) {
    // Check that {kJavaScriptCallCodeStartRegister} is correct.
    __ ComputeCodeStartAddress(ip);
    __ cmp(ip, kJavaScriptCallCodeStartRegister);
    __ Assert(eq, AbortReason::kWrongFunctionCodeStart);
  }

  int offset = Code::kCodeDataContainerOffset - Code::kHeaderSize;
  __ LoadP(r11, MemOperand(kJavaScriptCallCodeStartRegister, offset));
  __ LoadWordArith(
      r11, FieldMemOperand(r11, CodeDataContainer::kKindSpecificFlagsOffset));
  __ TestBit(r11, Code::kMarkedForDeoptimizationBit);
  __ Jump(BUILTIN_CODE(isolate(), CompileLazyDeoptimizedCode),
          RelocInfo::CODE_TARGET, ne, cr0);
}

void CodeGenerator::GenerateSpeculationPoisonFromCodeStartRegister() {
  Register scratch = kScratchReg;

  __ ComputeCodeStartAddress(scratch);

  // Calculate a mask which has all bits set in the normal case, but has all
  // bits cleared if we are speculatively executing the wrong PC.
  __ cmp(kJavaScriptCallCodeStartRegister, scratch);
  __ li(scratch, Operand::Zero());
  __ notx(kSpeculationPoisonRegister, scratch);
  __ isel(eq, kSpeculationPoisonRegister, kSpeculationPoisonRegister, scratch);
}

void CodeGenerator::AssembleRegisterArgumentPoisoning() {
  __ and_(kJSFunctionRegister, kJSFunctionRegister, kSpeculationPoisonRegister);
  __ and_(kContextRegister, kContextRegister, kSpeculationPoisonRegister);
  __ and_(sp, sp, kSpeculationPoisonRegister);
}

// Assembles an instruction after register allocation, producing machine code.
CodeGenerator::CodeGenResult CodeGenerator::AssembleArchInstruction(
    Instruction* instr) {
  PPCOperandConverter i(this, instr);
  ArchOpcode opcode = ArchOpcodeField::decode(instr->opcode());

  switch (opcode) {
    case kArchCallCodeObject: {
      v8::internal::Assembler::BlockTrampolinePoolScope block_trampoline_pool(
          tasm());
      if (HasRegisterInput(instr, 0)) {
        Register reg = i.InputRegister(0);
        DCHECK_IMPLIES(
            HasCallDescriptorFlag(instr, CallDescriptor::kFixedTargetRegister),
            reg == kJavaScriptCallCodeStartRegister);
        __ CallCodeObject(reg);
      } else {
        __ Call(i.InputCode(0), RelocInfo::CODE_TARGET);
      }
      RecordCallPosition(instr);
      DCHECK_EQ(LeaveRC, i.OutputRCBit());
      frame_access_state()->ClearSPDelta();
      break;
    }
    case kArchCallBuiltinPointer: {
      DCHECK(!instr->InputAt(0)->IsImmediate());
      Register builtin_index = i.InputRegister(0);
      __ CallBuiltinByIndex(builtin_index);
      RecordCallPosition(instr);
      frame_access_state()->ClearSPDelta();
      break;
    }
    case kArchCallWasmFunction: {
      // We must not share code targets for calls to builtins for wasm code, as
      // they might need to be patched individually.
      if (instr->InputAt(0)->IsImmediate()) {
        Constant constant = i.ToConstant(instr->InputAt(0));
#ifdef V8_TARGET_ARCH_PPC64
        Address wasm_code = static_cast<Address>(constant.ToInt64());
#else
        Address wasm_code = static_cast<Address>(constant.ToInt32());
#endif
        __ Call(wasm_code, constant.rmode());
      } else {
        __ Call(i.InputRegister(0));
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
        Register reg = i.InputRegister(0);
        DCHECK_IMPLIES(
            HasCallDescriptorFlag(instr, CallDescriptor::kFixedTargetRegister),
            reg == kJavaScriptCallCodeStartRegister);
        __ JumpCodeObject(reg);
      } else {
        // We cannot use the constant pool to load the target since
        // we've already restored the caller's frame.
        ConstantPoolUnavailableScope constant_pool_unavailable(tasm());
        __ Jump(i.InputCode(0), RelocInfo::CODE_TARGET);
      }
      DCHECK_EQ(LeaveRC, i.OutputRCBit());
      frame_access_state()->ClearSPDelta();
      frame_access_state()->SetFrameAccessToDefault();
      break;
    }
    case kArchTailCallWasm: {
      // We must not share code targets for calls to builtins for wasm code, as
      // they might need to be patched individually.
      if (instr->InputAt(0)->IsImmediate()) {
        Constant constant = i.ToConstant(instr->InputAt(0));
#ifdef V8_TARGET_ARCH_S390X
        Address wasm_code = static_cast<Address>(constant.ToInt64());
#else
        Address wasm_code = static_cast<Address>(constant.ToInt32());
#endif
        __ Jump(wasm_code, constant.rmode());
      } else {
        __ Jump(i.InputRegister(0));
      }
      DCHECK_EQ(LeaveRC, i.OutputRCBit());
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
      frame_access_state()->ClearSPDelta();
      frame_access_state()->SetFrameAccessToDefault();
      break;
    }
    case kArchCallJSFunction: {
      v8::internal::Assembler::BlockTrampolinePoolScope block_trampoline_pool(
          tasm());
      Register func = i.InputRegister(0);
      if (FLAG_debug_code) {
        // Check the function's context matches the context argument.
        __ LoadP(kScratchReg,
                 FieldMemOperand(func, JSFunction::kContextOffset));
        __ cmp(cp, kScratchReg);
        __ Assert(eq, AbortReason::kWrongFunctionContext);
      }
      static_assert(kJavaScriptCallCodeStartRegister == r5, "ABI mismatch");
      __ LoadP(r5, FieldMemOperand(func, JSFunction::kCodeOffset));
      __ CallCodeObject(r5);
      RecordCallPosition(instr);
      DCHECK_EQ(LeaveRC, i.OutputRCBit());
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
    case kArchComment:
#ifdef V8_TARGET_ARCH_PPC64
      __ RecordComment(reinterpret_cast<const char*>(i.InputInt64(0)));
#else
      __ RecordComment(reinterpret_cast<const char*>(i.InputInt32(0)));
#endif
      break;
    case kArchCallCFunction: {
      int misc_field = MiscField::decode(instr->opcode());
      int num_parameters = misc_field;
      bool has_function_descriptor = false;
      Label start_call;
      bool isWasmCapiFunction =
          linkage()->GetIncomingDescriptor()->IsWasmCapiFunction();
      int offset = 20 * kInstrSize;
#if defined(_AIX)
      // AIX/PPC64BE Linux uses a function descriptor
      int kNumParametersMask = kHasFunctionDescriptorBitMask - 1;
      num_parameters = kNumParametersMask & misc_field;
      has_function_descriptor =
          (misc_field & kHasFunctionDescriptorBitMask) != 0;
      // AIX may emit 2 extra Load instructions under CallCFunctionHelper
      // due to having function descriptor.
      if (has_function_descriptor) {
        offset = 22 * kInstrSize;
      }
#endif
      if (isWasmCapiFunction) {
        __ mflr(r0);
        __ bind(&start_call);
        __ LoadPC(kScratchReg);
        __ addi(kScratchReg, kScratchReg, Operand(offset));
        __ StoreP(kScratchReg,
                  MemOperand(fp, WasmExitFrameConstants::kCallingPCOffset));
        __ mtlr(r0);
      }
      if (instr->InputAt(0)->IsImmediate()) {
        ExternalReference ref = i.InputExternalReference(0);
        __ CallCFunction(ref, num_parameters, has_function_descriptor);
      } else {
        Register func = i.InputRegister(0);
        __ CallCFunction(func, num_parameters, has_function_descriptor);
      }
      // TODO(miladfar): In the above block, kScratchReg must be populated with
      // the strictly-correct PC, which is the return address at this spot. The
      // offset is set to 36 (9 * kInstrSize) on pLinux and 44 on AIX, which is
      // counted from where we are binding to the label and ends at this spot.
      // If failed, replace it with the correct offset suggested. More info on
      // f5ab7d3.
      if (isWasmCapiFunction) {
        CHECK_EQ(offset, __ SizeOfCodeGeneratedSince(&start_call));
        RecordSafepoint(instr->reference_map(), Safepoint::kNoLazyDeopt);
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
      DCHECK_EQ(LeaveRC, i.OutputRCBit());
      break;
    case kArchBinarySearchSwitch:
      AssembleArchBinarySearchSwitch(instr);
      break;
    case kArchTableSwitch:
      AssembleArchTableSwitch(instr);
      DCHECK_EQ(LeaveRC, i.OutputRCBit());
      break;
    case kArchAbortCSAAssert:
      DCHECK(i.InputRegister(0) == r4);
      {
        // We don't actually want to generate a pile of code for this, so just
        // claim there is a stack frame, without generating one.
        FrameScope scope(tasm(), StackFrame::NONE);
        __ Call(
            isolate()->builtins()->builtin_handle(Builtins::kAbortCSAAssert),
            RelocInfo::CODE_TARGET);
      }
      __ stop();
      break;
    case kArchDebugBreak:
      __ DebugBreak();
      break;
    case kArchNop:
    case kArchThrowTerminator:
      // don't emit code for nops.
      DCHECK_EQ(LeaveRC, i.OutputRCBit());
      break;
    case kArchDeoptimize: {
      DeoptimizationExit* exit =
          BuildTranslation(instr, -1, 0, OutputFrameStateCombine::Ignore());
      CodeGenResult result = AssembleDeoptimizerCall(exit);
      if (result != kSuccess) return result;
      break;
    }
    case kArchRet:
      AssembleReturn(instr->InputAt(0));
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
    case kArchStackPointerGreaterThan: {
      // Potentially apply an offset to the current stack pointer before the
      // comparison to consider the size difference of an optimized frame versus
      // the contained unoptimized frames.

      Register lhs_register = sp;
      uint32_t offset;

      if (ShouldApplyOffsetToStackCheck(instr, &offset)) {
        lhs_register = i.TempRegister(0);
        if (is_int16(offset)) {
          __ subi(lhs_register, sp, Operand(offset));
        } else {
          __ mov(kScratchReg, Operand(offset));
          __ sub(lhs_register, sp, kScratchReg);
        }
      }

      constexpr size_t kValueIndex = 0;
      DCHECK(instr->InputAt(kValueIndex)->IsRegister());
      __ cmpl(lhs_register, i.InputRegister(kValueIndex), cr0);
      break;
    }
    case kArchStackCheckOffset:
      __ LoadSmiLiteral(i.OutputRegister(),
                        Smi::FromInt(GetStackCheckOffset()));
      break;
    case kArchTruncateDoubleToI:
      __ TruncateDoubleToI(isolate(), zone(), i.OutputRegister(),
                           i.InputDoubleRegister(0), DetermineStubCallMode());
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
        ool = new (zone()) OutOfLineRecordWrite(
            this, object, offset, value, scratch0, scratch1, mode,
            DetermineStubCallMode(), &unwinding_info_writer_);
        __ StoreP(value, MemOperand(object, offset));
      } else {
        DCHECK_EQ(kMode_MRR, addressing_mode);
        Register offset(i.InputRegister(1));
        ool = new (zone()) OutOfLineRecordWrite(
            this, object, offset, value, scratch0, scratch1, mode,
            DetermineStubCallMode(), &unwinding_info_writer_);
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
    case kArchWordPoisonOnSpeculation:
      __ and_(i.OutputRegister(), i.InputRegister(0),
              kSpeculationPoisonRegister);
      break;
    case kPPC_Peek: {
      // The incoming value is 0-based, but we need a 1-based value.
      int reverse_slot = i.InputInt32(0) + 1;
      int offset =
          FrameSlotToFPOffset(frame()->GetTotalFrameSlotCount() - reverse_slot);
      if (instr->OutputAt(0)->IsFPRegister()) {
        LocationOperand* op = LocationOperand::cast(instr->OutputAt(0));
        if (op->representation() == MachineRepresentation::kFloat64) {
          __ LoadDouble(i.OutputDoubleRegister(), MemOperand(fp, offset), r0);
        } else {
          DCHECK_EQ(MachineRepresentation::kFloat32, op->representation());
          __ LoadFloat32(i.OutputFloatRegister(), MemOperand(fp, offset), r0);
        }
      } else {
        __ LoadP(i.OutputRegister(), MemOperand(fp, offset), r0);
      }
      break;
    }
    case kPPC_Sync: {
      __ sync();
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
    case kPPC_ShiftLeftPair: {
      Register second_output =
          instr->OutputCount() >= 2 ? i.OutputRegister(1) : i.TempRegister(0);
      if (instr->InputAt(2)->IsImmediate()) {
        __ ShiftLeftPair(i.OutputRegister(0), second_output, i.InputRegister(0),
                         i.InputRegister(1), i.InputInt32(2));
      } else {
        __ ShiftLeftPair(i.OutputRegister(0), second_output, i.InputRegister(0),
                         i.InputRegister(1), kScratchReg, i.InputRegister(2));
      }
      break;
    }
    case kPPC_ShiftRightPair: {
      Register second_output =
          instr->OutputCount() >= 2 ? i.OutputRegister(1) : i.TempRegister(0);
      if (instr->InputAt(2)->IsImmediate()) {
        __ ShiftRightPair(i.OutputRegister(0), second_output,
                          i.InputRegister(0), i.InputRegister(1),
                          i.InputInt32(2));
      } else {
        __ ShiftRightPair(i.OutputRegister(0), second_output,
                          i.InputRegister(0), i.InputRegister(1), kScratchReg,
                          i.InputRegister(2));
      }
      break;
    }
    case kPPC_ShiftRightAlgPair: {
      Register second_output =
          instr->OutputCount() >= 2 ? i.OutputRegister(1) : i.TempRegister(0);
      if (instr->InputAt(2)->IsImmediate()) {
        __ ShiftRightAlgPair(i.OutputRegister(0), second_output,
                             i.InputRegister(0), i.InputRegister(1),
                             i.InputInt32(2));
      } else {
        __ ShiftRightAlgPair(i.OutputRegister(0), second_output,
                             i.InputRegister(0), i.InputRegister(1),
                             kScratchReg, i.InputRegister(2));
      }
      break;
    }
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
    case kPPC_Add32:
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
        __ extsw(i.OutputRegister(), i.OutputRegister());
#if V8_TARGET_ARCH_PPC64
      }
#endif
      break;
#if V8_TARGET_ARCH_PPC64
    case kPPC_Add64:
      if (FlagsModeField::decode(instr->opcode()) != kFlags_none) {
        ASSEMBLE_ADD_WITH_OVERFLOW();
      } else {
        if (HasRegisterInput(instr, 1)) {
          __ add(i.OutputRegister(), i.InputRegister(0), i.InputRegister(1),
                 LeaveOE, i.OutputRCBit());
        } else {
          __ addi(i.OutputRegister(), i.InputRegister(0), i.InputImmediate(1));
          DCHECK_EQ(LeaveRC, i.OutputRCBit());
        }
      }
      break;
#endif
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
          if (is_int16(i.InputImmediate(1).immediate())) {
            __ subi(i.OutputRegister(), i.InputRegister(0),
                    i.InputImmediate(1));
            DCHECK_EQ(LeaveRC, i.OutputRCBit());
          } else {
            __ mov(kScratchReg, i.InputImmediate(1));
            __ sub(i.OutputRegister(), i.InputRegister(0), kScratchReg, LeaveOE,
                   i.OutputRCBit());
          }
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
      if (i.OutputRegister(0) == i.InputRegister(0) ||
          i.OutputRegister(0) == i.InputRegister(1) ||
          i.OutputRegister(1) == i.InputRegister(0) ||
          i.OutputRegister(1) == i.InputRegister(1)) {
        __ mullw(kScratchReg, i.InputRegister(0), i.InputRegister(1));  // low
        __ mulhw(i.OutputRegister(1), i.InputRegister(0),
                 i.InputRegister(1));  // high
        __ mr(i.OutputRegister(0), kScratchReg);
      } else {
        __ mullw(i.OutputRegister(0), i.InputRegister(0),
                 i.InputRegister(1));  // low
        __ mulhw(i.OutputRegister(1), i.InputRegister(0),
                 i.InputRegister(1));  // high
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
      if (CpuFeatures::IsSupported(MODULO)) {
        __ modsw(i.OutputRegister(), i.InputRegister(0), i.InputRegister(1));
      } else {
        ASSEMBLE_MODULO(divw, mullw);
      }
      break;
#if V8_TARGET_ARCH_PPC64
    case kPPC_Mod64:
      if (CpuFeatures::IsSupported(MODULO)) {
        __ modsd(i.OutputRegister(), i.InputRegister(0), i.InputRegister(1));
      } else {
        ASSEMBLE_MODULO(divd, mulld);
      }
      break;
#endif
    case kPPC_ModU32:
      if (CpuFeatures::IsSupported(MODULO)) {
        __ moduw(i.OutputRegister(), i.InputRegister(0), i.InputRegister(1));
      } else {
        ASSEMBLE_MODULO(divwu, mullw);
      }
      break;
#if V8_TARGET_ARCH_PPC64
    case kPPC_ModU64:
      if (CpuFeatures::IsSupported(MODULO)) {
        __ modud(i.OutputRegister(), i.InputRegister(0), i.InputRegister(1));
      } else {
        ASSEMBLE_MODULO(divdu, mulld);
      }
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
    case kIeee754Float64Pow:
      ASSEMBLE_IEEE754_BINOP(pow);
      break;
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
      __ cntlzw(i.OutputRegister(), i.InputRegister(0));
      DCHECK_EQ(LeaveRC, i.OutputRCBit());
      break;
#if V8_TARGET_ARCH_PPC64
    case kPPC_Cntlz64:
      __ cntlzd(i.OutputRegister(), i.InputRegister(0));
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
        LocationOperand* op = LocationOperand::cast(instr->InputAt(0));
        if (op->representation() == MachineRepresentation::kFloat64) {
          __ StoreDoubleU(i.InputDoubleRegister(0),
                          MemOperand(sp, -kDoubleSize), r0);
          frame_access_state()->IncreaseSPDelta(kDoubleSize /
                                                kSystemPointerSize);
        } else {
          DCHECK_EQ(MachineRepresentation::kFloat32, op->representation());
          __ StoreSingleU(i.InputDoubleRegister(0),
                          MemOperand(sp, -kSystemPointerSize), r0);
          frame_access_state()->IncreaseSPDelta(1);
        }
      } else {
        __ StorePU(i.InputRegister(0), MemOperand(sp, -kSystemPointerSize), r0);
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
                          MemOperand(sp, -num_slots * kSystemPointerSize), r0);
        } else {
          DCHECK_EQ(MachineRepresentation::kFloat32, op->representation());
          __ StoreSingleU(i.InputDoubleRegister(0),
                          MemOperand(sp, -num_slots * kSystemPointerSize), r0);
        }
      } else {
        __ StorePU(i.InputRegister(0),
                   MemOperand(sp, -num_slots * kSystemPointerSize), r0);
      }
      break;
    }
    case kPPC_StoreToStackSlot: {
      int slot = i.InputInt32(1);
      if (instr->InputAt(0)->IsFPRegister()) {
        LocationOperand* op = LocationOperand::cast(instr->InputAt(0));
        if (op->representation() == MachineRepresentation::kFloat64) {
          __ StoreDouble(i.InputDoubleRegister(0),
                         MemOperand(sp, slot * kSystemPointerSize), r0);
        } else {
          DCHECK_EQ(MachineRepresentation::kFloat32, op->representation());
          __ StoreSingle(i.InputDoubleRegister(0),
                         MemOperand(sp, slot * kSystemPointerSize), r0);
        }
      } else {
        __ StoreP(i.InputRegister(0), MemOperand(sp, slot * kSystemPointerSize),
                  r0);
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
          __ bc(v8::internal::kInstrSize * 2, BT, crbit);
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
          __ bc(v8::internal::kInstrSize * 2, BT, crbit);
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
      EmitWordLoadPoisoningIfNeeded(this, instr, i);
      break;
    case kPPC_LoadWordS8:
      ASSEMBLE_LOAD_INTEGER(lbz, lbzx);
      __ extsb(i.OutputRegister(), i.OutputRegister());
      EmitWordLoadPoisoningIfNeeded(this, instr, i);
      break;
    case kPPC_LoadWordU16:
      ASSEMBLE_LOAD_INTEGER(lhz, lhzx);
      EmitWordLoadPoisoningIfNeeded(this, instr, i);
      break;
    case kPPC_LoadWordS16:
      ASSEMBLE_LOAD_INTEGER(lha, lhax);
      EmitWordLoadPoisoningIfNeeded(this, instr, i);
      break;
    case kPPC_LoadWordU32:
      ASSEMBLE_LOAD_INTEGER(lwz, lwzx);
      EmitWordLoadPoisoningIfNeeded(this, instr, i);
      break;
    case kPPC_LoadWordS32:
      ASSEMBLE_LOAD_INTEGER(lwa, lwax);
      EmitWordLoadPoisoningIfNeeded(this, instr, i);
      break;
#if V8_TARGET_ARCH_PPC64
    case kPPC_LoadWord64:
      ASSEMBLE_LOAD_INTEGER(ld, ldx);
      EmitWordLoadPoisoningIfNeeded(this, instr, i);
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
      ASSEMBLE_STORE_FLOAT(stfs, stfsx);
      break;
    case kPPC_StoreDouble:
      ASSEMBLE_STORE_FLOAT(stfd, stfdx);
      break;
    case kWord32AtomicLoadInt8:
    case kPPC_AtomicLoadUint8:
    case kWord32AtomicLoadInt16:
    case kPPC_AtomicLoadUint16:
    case kPPC_AtomicLoadWord32:
    case kPPC_AtomicLoadWord64:
    case kPPC_AtomicStoreUint8:
    case kPPC_AtomicStoreUint16:
    case kPPC_AtomicStoreWord32:
    case kPPC_AtomicStoreWord64:
      UNREACHABLE();
    case kWord32AtomicExchangeInt8:
      ASSEMBLE_ATOMIC_EXCHANGE_INTEGER(lbarx, stbcx);
      __ extsb(i.OutputRegister(0), i.OutputRegister(0));
      break;
    case kPPC_AtomicExchangeUint8:
      ASSEMBLE_ATOMIC_EXCHANGE_INTEGER(lbarx, stbcx);
      break;
    case kWord32AtomicExchangeInt16:
      ASSEMBLE_ATOMIC_EXCHANGE_INTEGER(lharx, sthcx);
      __ extsh(i.OutputRegister(0), i.OutputRegister(0));
      break;
    case kPPC_AtomicExchangeUint16:
      ASSEMBLE_ATOMIC_EXCHANGE_INTEGER(lharx, sthcx);
      break;
    case kPPC_AtomicExchangeWord32:
      ASSEMBLE_ATOMIC_EXCHANGE_INTEGER(lwarx, stwcx);
      break;
    case kPPC_AtomicExchangeWord64:
      ASSEMBLE_ATOMIC_EXCHANGE_INTEGER(ldarx, stdcx);
      break;
    case kWord32AtomicCompareExchangeInt8:
      ASSEMBLE_ATOMIC_COMPARE_EXCHANGE_SIGN_EXT(cmp, lbarx, stbcx, extsb);
      break;
    case kPPC_AtomicCompareExchangeUint8:
      ASSEMBLE_ATOMIC_COMPARE_EXCHANGE(cmp, lbarx, stbcx, ZeroExtByte);
      break;
    case kWord32AtomicCompareExchangeInt16:
      ASSEMBLE_ATOMIC_COMPARE_EXCHANGE_SIGN_EXT(cmp, lharx, sthcx, extsh);
      break;
    case kPPC_AtomicCompareExchangeUint16:
      ASSEMBLE_ATOMIC_COMPARE_EXCHANGE(cmp, lharx, sthcx, ZeroExtHalfWord);
      break;
    case kPPC_AtomicCompareExchangeWord32:
      ASSEMBLE_ATOMIC_COMPARE_EXCHANGE(cmpw, lwarx, stwcx, ZeroExtWord32);
      break;
    case kPPC_AtomicCompareExchangeWord64:
      ASSEMBLE_ATOMIC_COMPARE_EXCHANGE(cmp, ldarx, stdcx, mr);
      break;

#define ATOMIC_BINOP_CASE(op, inst)                            \
  case kPPC_Atomic##op##Int8:                                  \
    ASSEMBLE_ATOMIC_BINOP_SIGN_EXT(inst, lbarx, stbcx, extsb); \
    break;                                                     \
  case kPPC_Atomic##op##Uint8:                                 \
    ASSEMBLE_ATOMIC_BINOP(inst, lbarx, stbcx);                 \
    break;                                                     \
  case kPPC_Atomic##op##Int16:                                 \
    ASSEMBLE_ATOMIC_BINOP_SIGN_EXT(inst, lharx, sthcx, extsh); \
    break;                                                     \
  case kPPC_Atomic##op##Uint16:                                \
    ASSEMBLE_ATOMIC_BINOP(inst, lharx, sthcx);                 \
    break;                                                     \
  case kPPC_Atomic##op##Int32:                                 \
    ASSEMBLE_ATOMIC_BINOP_SIGN_EXT(inst, lwarx, stwcx, extsw); \
    break;                                                     \
  case kPPC_Atomic##op##Uint32:                                \
    ASSEMBLE_ATOMIC_BINOP(inst, lwarx, stwcx);                 \
    break;                                                     \
  case kPPC_Atomic##op##Int64:                                 \
  case kPPC_Atomic##op##Uint64:                                \
    ASSEMBLE_ATOMIC_BINOP(inst, ldarx, stdcx);                 \
    break;
      ATOMIC_BINOP_CASE(Add, add)
      ATOMIC_BINOP_CASE(Sub, sub)
      ATOMIC_BINOP_CASE(And, and_)
      ATOMIC_BINOP_CASE(Or, orx)
      ATOMIC_BINOP_CASE(Xor, xor_)
#undef ATOMIC_BINOP_CASE

    case kPPC_ByteRev32: {
      Register input = i.InputRegister(0);
      Register output = i.OutputRegister();
      Register temp1 = r0;
      __ rotlwi(temp1, input, 8);
      __ rlwimi(temp1, input, 24, 0, 7);
      __ rlwimi(temp1, input, 24, 16, 23);
      __ extsw(output, temp1);
      break;
    }
#ifdef V8_TARGET_ARCH_PPC64
    case kPPC_ByteRev64: {
      Register input = i.InputRegister(0);
      Register output = i.OutputRegister();
      Register temp1 = r0;
      Register temp2 = kScratchReg;
      Register temp3 = i.TempRegister(0);
      __ rldicl(temp1, input, 32, 32);
      __ rotlwi(temp2, input, 8);
      __ rlwimi(temp2, input, 24, 0, 7);
      __ rotlwi(temp3, temp1, 8);
      __ rlwimi(temp2, input, 24, 16, 23);
      __ rlwimi(temp3, temp1, 24, 0, 7);
      __ rlwimi(temp3, temp1, 24, 16, 23);
      __ rldicr(temp2, temp2, 32, 31);
      __ orx(output, temp2, temp3);
      break;
    }
#endif  // V8_TARGET_ARCH_PPC64
    default:
      UNREACHABLE();
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

void CodeGenerator::AssembleBranchPoisoning(FlagsCondition condition,
                                            Instruction* instr) {
  // TODO(John) Handle float comparisons (kUnordered[Not]Equal).
  if (condition == kUnorderedEqual || condition == kUnorderedNotEqual ||
      condition == kOverflow || condition == kNotOverflow) {
    return;
  }

  ArchOpcode op = instr->arch_opcode();
  condition = NegateFlagsCondition(condition);
  __ li(kScratchReg, Operand::Zero());
  __ isel(FlagsConditionToCondition(condition, op), kSpeculationPoisonRegister,
          kScratchReg, kSpeculationPoisonRegister, cr0);
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
      PPCOperandConverter i(gen_, instr_);
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
        __ PrepareCallCFunction(0, 0, cp);
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
        // Just encode the stub index. This will be patched when the code
        // is added to the native module and copied into wasm code space.
        __ Call(static_cast<Address>(trap_id), RelocInfo::WASM_STUB_CALL);
        ReferenceMap* reference_map =
            new (gen_->zone()) ReferenceMap(gen_->zone());
        gen_->RecordSafepoint(reference_map, Safepoint::kNoLazyDeopt);
        if (FLAG_debug_code) {
          __ stop();
        }
      }
    }

    Instruction* instr_;
    CodeGenerator* gen_;
  };
  auto ool = new (zone()) OutOfLineTrap(this, instr);
  Label* tlabel = ool->entry();
  Label end;

  ArchOpcode op = instr->arch_opcode();
  CRegister cr = cr0;
  Condition cond = FlagsConditionToCondition(condition, op);
  if (op == kPPC_CmpDouble) {
    // check for unordered if necessary
    if (cond == le) {
      __ bunordered(&end, cr);
      // Unnecessary for eq/lt since only FU bit will be set.
    } else if (cond == gt) {
      __ bunordered(tlabel, cr);
      // Unnecessary for ne/ge since only FU bit will be set.
    }
  }
  __ b(cond, tlabel, cr);
  __ bind(&end);
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

void CodeGenerator::AssembleArchBinarySearchSwitch(Instruction* instr) {
  PPCOperandConverter i(this, instr);
  Register input = i.InputRegister(0);
  std::vector<std::pair<int32_t, Label*>> cases;
  for (size_t index = 2; index < instr->InputCount(); index += 2) {
    cases.push_back({i.InputInt32(index + 0), GetLabel(i.InputRpo(index + 1))});
  }
  AssembleArchBinarySearchSwitchRange(input, i.InputRpo(1), cases.data(),
                                      cases.data() + cases.size());
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
  __ ShiftLeftImm(r0, input, Operand(kSystemPointerSizeLog2));
  __ LoadPX(kScratchReg, MemOperand(kScratchReg, r0));
  __ Jump(kScratchReg);
}

void CodeGenerator::FinishFrame(Frame* frame) {
  auto call_descriptor = linkage()->GetIncomingDescriptor();
  const RegList double_saves = call_descriptor->CalleeSavedFPRegisters();

  // Save callee-saved Double registers.
  if (double_saves != 0) {
    frame->AlignSavedCalleeRegisterSlots();
    DCHECK_EQ(kNumCalleeSavedDoubles,
              base::bits::CountPopulation(double_saves));
    frame->AllocateSavedCalleeRegisterSlots(kNumCalleeSavedDoubles *
                                            (kDoubleSize / kSystemPointerSize));
  }
  // Save callee-saved registers.
  const RegList saves = FLAG_enable_embedded_constant_pool
                            ? call_descriptor->CalleeSavedRegisters() &
                                  ~kConstantPoolRegister.bit()
                            : call_descriptor->CalleeSavedRegisters();
  if (saves != 0) {
    // register save area does not include the fp or constant pool pointer.
    const int num_saves =
        kNumCalleeSaved - 1 - (FLAG_enable_embedded_constant_pool ? 1 : 0);
    DCHECK(num_saves == base::bits::CountPopulation(saves));
    frame->AllocateSavedCalleeRegisterSlots(num_saves);
  }
}

void CodeGenerator::AssembleConstructFrame() {
  auto call_descriptor = linkage()->GetIncomingDescriptor();
  if (frame_access_state()->has_frame()) {
    if (call_descriptor->IsCFunctionCall()) {
      if (info()->GetOutputStackFrameType() == StackFrame::C_WASM_ENTRY) {
        __ StubPrologue(StackFrame::C_WASM_ENTRY);
        // Reserve stack space for saving the c_entry_fp later.
        __ addi(sp, sp, Operand(-kSystemPointerSize));
      } else {
        __ mflr(r0);
        if (FLAG_enable_embedded_constant_pool) {
          __ Push(r0, fp, kConstantPoolRegister);
          // Adjust FP to point to saved FP.
          __ subi(fp, sp, Operand(StandardFrameConstants::kConstantPoolOffset));
        } else {
          __ Push(r0, fp);
          __ mr(fp, sp);
        }
      }
    } else if (call_descriptor->IsJSFunctionCall()) {
      __ Prologue();
      if (call_descriptor->PushArgumentCount()) {
        __ Push(kJavaScriptCallArgCountRegister);
      }
    } else {
      StackFrame::Type type = info()->GetOutputStackFrameType();
      // TODO(mbrandy): Detect cases where ip is the entrypoint (for
      // efficient intialization of the constant pool pointer register).
      __ StubPrologue(type);
      if (call_descriptor->IsWasmFunctionCall()) {
        __ Push(kWasmInstanceRegister);
      } else if (call_descriptor->IsWasmImportWrapper() ||
                 call_descriptor->IsWasmCapiFunction()) {
        // Wasm import wrappers are passed a tuple in the place of the instance.
        // Unpack the tuple into the instance and the target callable.
        // This must be done here in the codegen because it cannot be expressed
        // properly in the graph.
        __ LoadP(kJSFunctionRegister,
                 FieldMemOperand(kWasmInstanceRegister, Tuple2::kValue2Offset));
        __ LoadP(kWasmInstanceRegister,
                 FieldMemOperand(kWasmInstanceRegister, Tuple2::kValue1Offset));
        __ Push(kWasmInstanceRegister);
        if (call_descriptor->IsWasmCapiFunction()) {
          // Reserve space for saving the PC later.
          __ addi(sp, sp, Operand(-kSystemPointerSize));
        }
      }
    }
    unwinding_info_writer_.MarkFrameConstructed(__ pc_offset());
  }

  int required_slots =
      frame()->GetTotalFrameSlotCount() - frame()->GetFixedSlotCount();
  if (info()->is_osr()) {
    // TurboFan OSR-compiled functions cannot be entered directly.
    __ Abort(AbortReason::kShouldNotDirectlyEnterOsrFunction);

    // Unoptimized code jumps directly to this entrypoint while the unoptimized
    // frame is still on the stack. Optimized code uses OSR values directly from
    // the unoptimized frame. Thus, all that needs to be done is to allocate the
    // remaining stack slots.
    if (FLAG_code_comments) __ RecordComment("-- OSR entrypoint --");
    osr_pc_offset_ = __ pc_offset();
    required_slots -= osr_helper()->UnoptimizedFrameSlots();
    ResetSpeculationPoison();
  }

  const RegList saves_fp = call_descriptor->CalleeSavedFPRegisters();
  const RegList saves = FLAG_enable_embedded_constant_pool
                            ? call_descriptor->CalleeSavedRegisters() &
                                  ~kConstantPoolRegister.bit()
                            : call_descriptor->CalleeSavedRegisters();

  if (required_slots > 0) {
    if (info()->IsWasm() && required_slots > 128) {
      // For WebAssembly functions with big frames we have to do the stack
      // overflow check before we construct the frame. Otherwise we may not
      // have enough space on the stack to call the runtime for the stack
      // overflow.
      Label done;

      // If the frame is bigger than the stack, we throw the stack overflow
      // exception unconditionally. Thereby we can avoid the integer overflow
      // check in the condition code.
      if ((required_slots * kSystemPointerSize) < (FLAG_stack_size * 1024)) {
        Register scratch = ip;
        __ LoadP(
            scratch,
            FieldMemOperand(kWasmInstanceRegister,
                            WasmInstanceObject::kRealStackLimitAddressOffset));
        __ LoadP(scratch, MemOperand(scratch), r0);
        __ Add(scratch, scratch, required_slots * kSystemPointerSize, r0);
        __ cmpl(sp, scratch);
        __ bge(&done);
      }

      __ Call(wasm::WasmCode::kWasmStackOverflow, RelocInfo::WASM_STUB_CALL);
      // We come from WebAssembly, there are no references for the GC.
      ReferenceMap* reference_map = new (zone()) ReferenceMap(zone());
      RecordSafepoint(reference_map, Safepoint::kNoLazyDeopt);
      if (FLAG_debug_code) {
        __ stop();
      }

      __ bind(&done);
    }

    // Skip callee-saved and return slots, which are pushed below.
    required_slots -= base::bits::CountPopulation(saves);
    required_slots -= frame()->GetReturnSlotCount();
    required_slots -= (kDoubleSize / kSystemPointerSize) *
                      base::bits::CountPopulation(saves_fp);
    __ Add(sp, sp, -required_slots * kSystemPointerSize, r0);
  }

  // Save callee-saved Double registers.
  if (saves_fp != 0) {
    __ MultiPushDoubles(saves_fp);
    DCHECK_EQ(kNumCalleeSavedDoubles, base::bits::CountPopulation(saves_fp));
  }

  // Save callee-saved registers.
  if (saves != 0) {
    __ MultiPush(saves);
    // register save area does not include the fp or constant pool pointer.
  }

  const int returns = frame()->GetReturnSlotCount();
  if (returns != 0) {
    // Create space for returns.
    __ Add(sp, sp, -returns * kSystemPointerSize, r0);
  }
}

void CodeGenerator::AssembleReturn(InstructionOperand* pop) {
  auto call_descriptor = linkage()->GetIncomingDescriptor();
  int pop_count = static_cast<int>(call_descriptor->StackParameterCount());

  const int returns = frame()->GetReturnSlotCount();
  if (returns != 0) {
    // Create space for returns.
    __ Add(sp, sp, returns * kSystemPointerSize, r0);
  }

  // Restore registers.
  const RegList saves = FLAG_enable_embedded_constant_pool
                            ? call_descriptor->CalleeSavedRegisters() &
                                  ~kConstantPoolRegister.bit()
                            : call_descriptor->CalleeSavedRegisters();
  if (saves != 0) {
    __ MultiPop(saves);
  }

  // Restore double registers.
  const RegList double_saves = call_descriptor->CalleeSavedFPRegisters();
  if (double_saves != 0) {
    __ MultiPopDoubles(double_saves);
  }
  PPCOperandConverter g(this, nullptr);
  unwinding_info_writer_.MarkBlockWillExit();

  if (call_descriptor->IsCFunctionCall()) {
    AssembleDeconstructFrame();
  } else if (frame_access_state()->has_frame()) {
    // Canonicalize JSFunction return sites for now unless they have an variable
    // number of stack slot pops
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
  // Constant pool is unavailable since the frame has been destructed
  ConstantPoolUnavailableScope constant_pool_unavailable(tasm());
  if (pop->IsImmediate()) {
    DCHECK(Constant::kInt32 == g.ToConstant(pop).type() ||
           Constant::kInt64 == g.ToConstant(pop).type());
    pop_count += g.ToConstant(pop).ToInt32();
  } else {
    __ Drop(g.ToRegister(pop));
  }
  __ Drop(pop_count);
  __ Ret();
}

void CodeGenerator::FinishCode() {}

void CodeGenerator::PrepareForDeoptimizationExits(int deopt_count) {}

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
          if (false) {
#else
          if (RelocInfo::IsWasmReference(src.rmode())) {
#endif
            __ mov(dst, Operand(src.ToInt32(), src.rmode()));
          } else {
            __ mov(dst, Operand(src.ToInt32()));
          }
          break;
        case Constant::kInt64:
#if V8_TARGET_ARCH_PPC64
          if (RelocInfo::IsWasmReference(src.rmode())) {
            __ mov(dst, Operand(src.ToInt64(), src.rmode()));
          } else {
#endif
            __ mov(dst, Operand(src.ToInt64()));
#if V8_TARGET_ARCH_PPC64
          }
#endif
          break;
        case Constant::kFloat32:
          __ mov(dst, Operand::EmbeddedNumber(src.ToFloat32()));
          break;
        case Constant::kFloat64:
          __ mov(dst, Operand::EmbeddedNumber(src.ToFloat64().value()));
          break;
        case Constant::kExternalReference:
          __ Move(dst, src.ToExternalReference());
          break;
        case Constant::kDelayedStringConstant:
          __ mov(dst, Operand::EmbeddedStringConstant(
                          src.ToDelayedStringConstant()));
          break;
        case Constant::kHeapObject: {
          Handle<HeapObject> src_object = src.ToHeapObject();
          RootIndex index;
          if (IsMaterializableFromRoot(src_object, &index)) {
            __ LoadRoot(dst, index);
          } else {
            __ Move(dst, src_object);
          }
          break;
        }
        case Constant::kCompressedHeapObject:
          UNREACHABLE();
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
      Double value;
#if V8_HOST_ARCH_IA32 || V8_HOST_ARCH_X64
      // casting double precision snan to single precision
      // converts it to qnan on ia32/x64
      if (src.type() == Constant::kFloat32) {
        uint32_t val = src.ToFloat32AsInt();
        if ((val & 0x7F800000) == 0x7F800000) {
          uint64_t dval = static_cast<uint64_t>(val);
          dval = ((dval & 0xC0000000) << 32) | ((dval & 0x40000000) << 31) |
                 ((dval & 0x40000000) << 30) | ((dval & 0x7FFFFFFF) << 29);
          value = Double(dval);
        } else {
          value = Double(static_cast<double>(src.ToFloat32()));
        }
      } else {
        value = Double(src.ToFloat64());
      }
#else
      value = src.type() == Constant::kFloat32
                  ? Double(static_cast<double>(src.ToFloat32()))
                  : Double(src.ToFloat64());
#endif
      __ LoadDoubleLiteral(dst, value, kScratchReg);
      if (destination->IsDoubleStackSlot()) {
        __ StoreDouble(dst, g.ToMemOperand(destination), r0);
      } else if (destination->IsFloatStackSlot()) {
        __ StoreSingle(dst, g.ToMemOperand(destination), r0);
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

// Swaping contents in source and destination.
// source and destination could be:
//   Register,
//   FloatRegister,
//   DoubleRegister,
//   StackSlot,
//   FloatStackSlot,
//   or DoubleStackSlot
void CodeGenerator::AssembleSwap(InstructionOperand* source,
                                 InstructionOperand* destination) {
  PPCOperandConverter g(this, nullptr);
  if (source->IsRegister()) {
    Register src = g.ToRegister(source);
    if (destination->IsRegister()) {
      __ SwapP(src, g.ToRegister(destination), kScratchReg);
    } else {
      DCHECK(destination->IsStackSlot());
      __ SwapP(src, g.ToMemOperand(destination), kScratchReg);
    }
  } else if (source->IsStackSlot()) {
    DCHECK(destination->IsStackSlot());
    __ SwapP(g.ToMemOperand(source), g.ToMemOperand(destination), kScratchReg,
             r0);
  } else if (source->IsFloatRegister()) {
    DoubleRegister src = g.ToDoubleRegister(source);
    if (destination->IsFloatRegister()) {
      __ SwapFloat32(src, g.ToDoubleRegister(destination), kScratchDoubleReg);
    } else {
      DCHECK(destination->IsFloatStackSlot());
      __ SwapFloat32(src, g.ToMemOperand(destination), kScratchDoubleReg);
    }
  } else if (source->IsDoubleRegister()) {
    DoubleRegister src = g.ToDoubleRegister(source);
    if (destination->IsDoubleRegister()) {
      __ SwapDouble(src, g.ToDoubleRegister(destination), kScratchDoubleReg);
    } else {
      DCHECK(destination->IsDoubleStackSlot());
      __ SwapDouble(src, g.ToMemOperand(destination), kScratchDoubleReg);
    }
  } else if (source->IsFloatStackSlot()) {
    DCHECK(destination->IsFloatStackSlot());
    __ SwapFloat32(g.ToMemOperand(source), g.ToMemOperand(destination),
                   kScratchDoubleReg, d0);
  } else if (source->IsDoubleStackSlot()) {
    DCHECK(destination->IsDoubleStackSlot());
    __ SwapDouble(g.ToMemOperand(source), g.ToMemOperand(destination),
                  kScratchDoubleReg, d0);
  } else if (source->IsSimd128Register()) {
    UNREACHABLE();
  } else {
    UNREACHABLE();
  }

  return;
}

void CodeGenerator::AssembleJumpTable(Label** targets, size_t target_count) {
  for (size_t index = 0; index < target_count; ++index) {
    __ emit_label_addr(targets[index]);
  }
}

#undef __

}  // namespace compiler
}  // namespace internal
}  // namespace v8
