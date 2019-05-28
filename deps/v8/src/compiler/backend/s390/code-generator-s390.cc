// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/backend/code-generator.h"

#include "src/assembler-inl.h"
#include "src/callable.h"
#include "src/compiler/backend/code-generator-impl.h"
#include "src/compiler/backend/gap-resolver.h"
#include "src/compiler/node-matchers.h"
#include "src/compiler/osr.h"
#include "src/heap/heap-inl.h"  // crbug.com/v8/8499
#include "src/macro-assembler.h"
#include "src/optimized-compilation-info.h"
#include "src/wasm/wasm-code-manager.h"
#include "src/wasm/wasm-objects.h"

namespace v8 {
namespace internal {
namespace compiler {

#define __ tasm()->

#define kScratchReg ip

// Adds S390-specific methods to convert InstructionOperands.
class S390OperandConverter final : public InstructionOperandConverter {
 public:
  S390OperandConverter(CodeGenerator* gen, Instruction* instr)
      : InstructionOperandConverter(gen, instr) {}

  size_t OutputCount() { return instr_->OutputCount(); }

  bool Is64BitOperand(int index) {
    return LocationOperand::cast(instr_->InputAt(index))->representation() ==
           MachineRepresentation::kWord64;
  }

  bool Is32BitOperand(int index) {
    return LocationOperand::cast(instr_->InputAt(index))->representation() ==
           MachineRepresentation::kWord32;
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
#if V8_TARGET_ARCH_S390X
        return Operand(constant.ToInt64());
#endif
      case Constant::kExternalReference:
        return Operand(constant.ToExternalReference());
      case Constant::kDelayedStringConstant:
        return Operand::EmbeddedStringConstant(
            constant.ToDelayedStringConstant());
      case Constant::kHeapObject:
      case Constant::kRpoNumber:
        break;
    }
    UNREACHABLE();
  }

  MemOperand MemoryOperand(AddressingMode* mode, size_t* first_index) {
    const size_t index = *first_index;
    if (mode) *mode = AddressingModeField::decode(instr_->opcode());
    switch (AddressingModeField::decode(instr_->opcode())) {
      case kMode_None:
        break;
      case kMode_MR:
        *first_index += 1;
        return MemOperand(InputRegister(index + 0), 0);
      case kMode_MRI:
        *first_index += 2;
        return MemOperand(InputRegister(index + 0), InputInt32(index + 1));
      case kMode_MRR:
        *first_index += 2;
        return MemOperand(InputRegister(index + 0), InputRegister(index + 1));
      case kMode_MRRI:
        *first_index += 3;
        return MemOperand(InputRegister(index + 0), InputRegister(index + 1),
                          InputInt32(index + 2));
    }
    UNREACHABLE();
  }

  MemOperand MemoryOperand(AddressingMode* mode = nullptr,
                           size_t first_index = 0) {
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

  MemOperand InputStackSlot(size_t index) {
    InstructionOperand* op = instr_->InputAt(index);
    return SlotToMemOperand(AllocatedOperand::cast(op)->index());
  }

  MemOperand InputStackSlot32(size_t index) {
#if V8_TARGET_ARCH_S390X && !V8_TARGET_LITTLE_ENDIAN
    // We want to read the 32-bits directly from memory
    MemOperand mem = InputStackSlot(index);
    return MemOperand(mem.rb(), mem.rx(), mem.offset() + 4);
#else
    return InputStackSlot(index);
#endif
  }
};

static inline bool HasRegisterOutput(Instruction* instr, int index = 0) {
  return instr->OutputCount() > 0 && instr->OutputAt(index)->IsRegister();
}

static inline bool HasFPRegisterInput(Instruction* instr, int index) {
  return instr->InputAt(index)->IsFPRegister();
}

static inline bool HasRegisterInput(Instruction* instr, int index) {
  return instr->InputAt(index)->IsRegister() ||
         HasFPRegisterInput(instr, index);
}

static inline bool HasImmediateInput(Instruction* instr, size_t index) {
  return instr->InputAt(index)->IsImmediate();
}

static inline bool HasFPStackSlotInput(Instruction* instr, size_t index) {
  return instr->InputAt(index)->IsFPStackSlot();
}

static inline bool HasStackSlotInput(Instruction* instr, size_t index) {
  return instr->InputAt(index)->IsStackSlot() ||
         HasFPStackSlotInput(instr, index);
}

namespace {

class OutOfLineRecordWrite final : public OutOfLineCode {
 public:
  OutOfLineRecordWrite(CodeGenerator* gen, Register object, Register offset,
                       Register value, Register scratch0, Register scratch1,
                       RecordWriteMode mode, StubCallMode stub_mode)
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
        zone_(gen->zone()) {}

  OutOfLineRecordWrite(CodeGenerator* gen, Register object, int32_t offset,
                       Register value, Register scratch0, Register scratch1,
                       RecordWriteMode mode, StubCallMode stub_mode)
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
        zone_(gen->zone()) {}

  void Generate() final {
    if (mode_ > RecordWriteMode::kValueIsPointer) {
      __ JumpIfSmi(value_, exit());
    }
    __ CheckPageFlag(value_, scratch0_,
                     MemoryChunk::kPointersToHereAreInterestingMask, eq,
                     exit());
    if (offset_ == no_reg) {
      __ AddP(scratch1_, object_, Operand(offset_immediate_));
    } else {
      DCHECK_EQ(0, offset_immediate_);
      __ AddP(scratch1_, object_, offset_);
    }
    RememberedSetAction const remembered_set_action =
        mode_ > RecordWriteMode::kValueIsMap ? EMIT_REMEMBERED_SET
                                             : OMIT_REMEMBERED_SET;
    SaveFPRegsMode const save_fp_mode =
        frame()->DidAllocateDoubleRegisters() ? kSaveFPRegs : kDontSaveFPRegs;
    if (must_save_lr_) {
      // We need to save and restore r14 if the frame was elided.
      __ Push(r14);
    }
    if (mode_ == RecordWriteMode::kValueIsEphemeronKey) {
      __ CallEphemeronKeyBarrier(object_, scratch1_, save_fp_mode);
    } else if (stub_mode_ == StubCallMode::kCallWasmRuntimeStub) {
      __ CallRecordWriteStub(object_, scratch1_, remembered_set_action,
                             save_fp_mode, wasm::WasmCode::kWasmRecordWrite);
    } else {
      __ CallRecordWriteStub(object_, scratch1_, remembered_set_action,
                             save_fp_mode);
    }
    if (must_save_lr_) {
      // We need to save and restore r14 if the frame was elided.
      __ Pop(r14);
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
  Zone* zone_;
};

Condition FlagsConditionToCondition(FlagsCondition condition, ArchOpcode op) {
  switch (condition) {
    case kEqual:
      return eq;
    case kNotEqual:
      return ne;
    case kUnsignedLessThan:
      // unsigned number never less than 0
      if (op == kS390_LoadAndTestWord32 || op == kS390_LoadAndTestWord64)
        return CC_NOP;
      V8_FALLTHROUGH;
    case kSignedLessThan:
      return lt;
    case kUnsignedGreaterThanOrEqual:
      // unsigned number always greater than or equal 0
      if (op == kS390_LoadAndTestWord32 || op == kS390_LoadAndTestWord64)
        return CC_ALWAYS;
      V8_FALLTHROUGH;
    case kSignedGreaterThanOrEqual:
      return ge;
    case kUnsignedLessThanOrEqual:
      // unsigned number never less than 0
      if (op == kS390_LoadAndTestWord32 || op == kS390_LoadAndTestWord64)
        return CC_EQ;
      V8_FALLTHROUGH;
    case kSignedLessThanOrEqual:
      return le;
    case kUnsignedGreaterThan:
      // unsigned number always greater than or equal 0
      if (op == kS390_LoadAndTestWord32 || op == kS390_LoadAndTestWord64)
        return ne;
      V8_FALLTHROUGH;
    case kSignedGreaterThan:
      return gt;
    case kOverflow:
      // Overflow checked for AddP/SubP only.
      switch (op) {
        case kS390_Add32:
        case kS390_Add64:
        case kS390_Sub32:
        case kS390_Sub64:
        case kS390_Abs64:
        case kS390_Abs32:
        case kS390_Mul32:
          return overflow;
        default:
          break;
      }
      break;
    case kNotOverflow:
      switch (op) {
        case kS390_Add32:
        case kS390_Add64:
        case kS390_Sub32:
        case kS390_Sub64:
        case kS390_Abs64:
        case kS390_Abs32:
        case kS390_Mul32:
          return nooverflow;
        default:
          break;
      }
      break;
    default:
      break;
  }
  UNREACHABLE();
}

#define GET_MEMOPERAND32(ret, fi)                                       \
  ([&](int& ret) {                                                      \
    AddressingMode mode = AddressingModeField::decode(instr->opcode()); \
    MemOperand mem(r0);                                                 \
    if (mode != kMode_None) {                                           \
      size_t first_index = (fi);                                        \
      mem = i.MemoryOperand(&mode, &first_index);                       \
      ret = first_index;                                                \
    } else {                                                            \
      mem = i.InputStackSlot32(fi);                                     \
    }                                                                   \
    return mem;                                                         \
  })(ret)

#define GET_MEMOPERAND(ret, fi)                                         \
  ([&](int& ret) {                                                      \
    AddressingMode mode = AddressingModeField::decode(instr->opcode()); \
    MemOperand mem(r0);                                                 \
    if (mode != kMode_None) {                                           \
      size_t first_index = (fi);                                        \
      mem = i.MemoryOperand(&mode, &first_index);                       \
      ret = first_index;                                                \
    } else {                                                            \
      mem = i.InputStackSlot(fi);                                       \
    }                                                                   \
    return mem;                                                         \
  })(ret)

#define RRInstr(instr)                                \
  [&]() {                                             \
    DCHECK(i.OutputRegister() == i.InputRegister(0)); \
    __ instr(i.OutputRegister(), i.InputRegister(1)); \
    return 2;                                         \
  }
#define RIInstr(instr)                                 \
  [&]() {                                              \
    DCHECK(i.OutputRegister() == i.InputRegister(0));  \
    __ instr(i.OutputRegister(), i.InputImmediate(1)); \
    return 2;                                          \
  }
#define RMInstr(instr, GETMEM)                        \
  [&]() {                                             \
    DCHECK(i.OutputRegister() == i.InputRegister(0)); \
    int ret = 2;                                      \
    __ instr(i.OutputRegister(), GETMEM(ret, 1));     \
    return ret;                                       \
  }
#define RM32Instr(instr) RMInstr(instr, GET_MEMOPERAND32)
#define RM64Instr(instr) RMInstr(instr, GET_MEMOPERAND)

#define RRRInstr(instr)                                                   \
  [&]() {                                                                 \
    __ instr(i.OutputRegister(), i.InputRegister(0), i.InputRegister(1)); \
    return 2;                                                             \
  }
#define RRIInstr(instr)                                                    \
  [&]() {                                                                  \
    __ instr(i.OutputRegister(), i.InputRegister(0), i.InputImmediate(1)); \
    return 2;                                                              \
  }
#define RRMInstr(instr, GETMEM)                                       \
  [&]() {                                                             \
    int ret = 2;                                                      \
    __ instr(i.OutputRegister(), i.InputRegister(0), GETMEM(ret, 1)); \
    return ret;                                                       \
  }
#define RRM32Instr(instr) RRMInstr(instr, GET_MEMOPERAND32)
#define RRM64Instr(instr) RRMInstr(instr, GET_MEMOPERAND)

#define DDInstr(instr)                                            \
  [&]() {                                                         \
    DCHECK(i.OutputDoubleRegister() == i.InputDoubleRegister(0)); \
    __ instr(i.OutputDoubleRegister(), i.InputDoubleRegister(1)); \
    return 2;                                                     \
  }

#define DMInstr(instr)                                            \
  [&]() {                                                         \
    DCHECK(i.OutputDoubleRegister() == i.InputDoubleRegister(0)); \
    int ret = 2;                                                  \
    __ instr(i.OutputDoubleRegister(), GET_MEMOPERAND(ret, 1));   \
    return ret;                                                   \
  }

#define DMTInstr(instr)                                           \
  [&]() {                                                         \
    DCHECK(i.OutputDoubleRegister() == i.InputDoubleRegister(0)); \
    int ret = 2;                                                  \
    __ instr(i.OutputDoubleRegister(), GET_MEMOPERAND(ret, 1),    \
             kScratchDoubleReg);                                  \
    return ret;                                                   \
  }

#define R_MInstr(instr)                                   \
  [&]() {                                                 \
    int ret = 2;                                          \
    __ instr(i.OutputRegister(), GET_MEMOPERAND(ret, 0)); \
    return ret;                                           \
  }

#define R_DInstr(instr)                                     \
  [&]() {                                                   \
    __ instr(i.OutputRegister(), i.InputDoubleRegister(0)); \
    return 2;                                               \
  }

#define D_DInstr(instr)                                           \
  [&]() {                                                         \
    __ instr(i.OutputDoubleRegister(), i.InputDoubleRegister(0)); \
    return 2;                                                     \
  }

#define D_MInstr(instr)                                         \
  [&]() {                                                       \
    int ret = 2;                                                \
    __ instr(i.OutputDoubleRegister(), GET_MEMOPERAND(ret, 0)); \
    return ret;                                                 \
  }

#define D_MTInstr(instr)                                       \
  [&]() {                                                      \
    int ret = 2;                                               \
    __ instr(i.OutputDoubleRegister(), GET_MEMOPERAND(ret, 0), \
             kScratchDoubleReg);                               \
    return ret;                                                \
  }

static int nullInstr() { UNREACHABLE(); }

template <int numOfOperand, class RType, class MType, class IType>
static inline int AssembleOp(Instruction* instr, RType r, MType m, IType i) {
  AddressingMode mode = AddressingModeField::decode(instr->opcode());
  if (mode != kMode_None || HasStackSlotInput(instr, numOfOperand - 1)) {
    return m();
  } else if (HasRegisterInput(instr, numOfOperand - 1)) {
    return r();
  } else if (HasImmediateInput(instr, numOfOperand - 1)) {
    return i();
  } else {
    UNREACHABLE();
  }
}

template <class _RR, class _RM, class _RI>
static inline int AssembleBinOp(Instruction* instr, _RR _rr, _RM _rm, _RI _ri) {
  return AssembleOp<2>(instr, _rr, _rm, _ri);
}

template <class _R, class _M, class _I>
static inline int AssembleUnaryOp(Instruction* instr, _R _r, _M _m, _I _i) {
  return AssembleOp<1>(instr, _r, _m, _i);
}

#define ASSEMBLE_BIN_OP(_rr, _rm, _ri) AssembleBinOp(instr, _rr, _rm, _ri)
#define ASSEMBLE_UNARY_OP(_r, _m, _i) AssembleUnaryOp(instr, _r, _m, _i)

#ifdef V8_TARGET_ARCH_S390X
#define CHECK_AND_ZERO_EXT_OUTPUT(num)                                \
  ([&](int index) {                                                   \
    DCHECK(HasImmediateInput(instr, (index)));                        \
    int doZeroExt = i.InputInt32(index);                              \
    if (doZeroExt) __ LoadlW(i.OutputRegister(), i.OutputRegister()); \
  })(num)

#define ASSEMBLE_BIN32_OP(_rr, _rm, _ri) \
  { CHECK_AND_ZERO_EXT_OUTPUT(AssembleBinOp(instr, _rr, _rm, _ri)); }
#else
#define ASSEMBLE_BIN32_OP ASSEMBLE_BIN_OP
#define CHECK_AND_ZERO_EXT_OUTPUT(num)
#endif

}  // namespace

#define ASSEMBLE_FLOAT_UNOP(asm_instr)                                \
  do {                                                                \
    __ asm_instr(i.OutputDoubleRegister(), i.InputDoubleRegister(0)); \
  } while (0)

#define ASSEMBLE_FLOAT_BINOP(asm_instr)                              \
  do {                                                               \
    __ asm_instr(i.OutputDoubleRegister(), i.InputDoubleRegister(0), \
                 i.InputDoubleRegister(1));                          \
  } while (0)

#define ASSEMBLE_COMPARE(cmp_instr, cmpl_instr)                         \
  do {                                                                  \
    AddressingMode mode = AddressingModeField::decode(instr->opcode()); \
    if (mode != kMode_None) {                                           \
      size_t first_index = 1;                                           \
      MemOperand operand = i.MemoryOperand(&mode, &first_index);        \
      if (i.CompareLogical()) {                                         \
        __ cmpl_instr(i.InputRegister(0), operand);                     \
      } else {                                                          \
        __ cmp_instr(i.InputRegister(0), operand);                      \
      }                                                                 \
    } else if (HasRegisterInput(instr, 1)) {                            \
      if (i.CompareLogical()) {                                         \
        __ cmpl_instr(i.InputRegister(0), i.InputRegister(1));          \
      } else {                                                          \
        __ cmp_instr(i.InputRegister(0), i.InputRegister(1));           \
      }                                                                 \
    } else if (HasImmediateInput(instr, 1)) {                           \
      if (i.CompareLogical()) {                                         \
        __ cmpl_instr(i.InputRegister(0), i.InputImmediate(1));         \
      } else {                                                          \
        __ cmp_instr(i.InputRegister(0), i.InputImmediate(1));          \
      }                                                                 \
    } else {                                                            \
      DCHECK(HasStackSlotInput(instr, 1));                              \
      if (i.CompareLogical()) {                                         \
        __ cmpl_instr(i.InputRegister(0), i.InputStackSlot(1));         \
      } else {                                                          \
        __ cmp_instr(i.InputRegister(0), i.InputStackSlot(1));          \
      }                                                                 \
    }                                                                   \
  } while (0)

#define ASSEMBLE_COMPARE32(cmp_instr, cmpl_instr)                       \
  do {                                                                  \
    AddressingMode mode = AddressingModeField::decode(instr->opcode()); \
    if (mode != kMode_None) {                                           \
      size_t first_index = 1;                                           \
      MemOperand operand = i.MemoryOperand(&mode, &first_index);        \
      if (i.CompareLogical()) {                                         \
        __ cmpl_instr(i.InputRegister(0), operand);                     \
      } else {                                                          \
        __ cmp_instr(i.InputRegister(0), operand);                      \
      }                                                                 \
    } else if (HasRegisterInput(instr, 1)) {                            \
      if (i.CompareLogical()) {                                         \
        __ cmpl_instr(i.InputRegister(0), i.InputRegister(1));          \
      } else {                                                          \
        __ cmp_instr(i.InputRegister(0), i.InputRegister(1));           \
      }                                                                 \
    } else if (HasImmediateInput(instr, 1)) {                           \
      if (i.CompareLogical()) {                                         \
        __ cmpl_instr(i.InputRegister(0), i.InputImmediate(1));         \
      } else {                                                          \
        __ cmp_instr(i.InputRegister(0), i.InputImmediate(1));          \
      }                                                                 \
    } else {                                                            \
      DCHECK(HasStackSlotInput(instr, 1));                              \
      if (i.CompareLogical()) {                                         \
        __ cmpl_instr(i.InputRegister(0), i.InputStackSlot32(1));       \
      } else {                                                          \
        __ cmp_instr(i.InputRegister(0), i.InputStackSlot32(1));        \
      }                                                                 \
    }                                                                   \
  } while (0)

#define ASSEMBLE_FLOAT_COMPARE(cmp_rr_instr, cmp_rm_instr, load_instr)     \
  do {                                                                     \
    AddressingMode mode = AddressingModeField::decode(instr->opcode());    \
    if (mode != kMode_None) {                                              \
      size_t first_index = 1;                                              \
      MemOperand operand = i.MemoryOperand(&mode, &first_index);           \
      __ cmp_rm_instr(i.InputDoubleRegister(0), operand);                  \
    } else if (HasFPRegisterInput(instr, 1)) {                             \
      __ cmp_rr_instr(i.InputDoubleRegister(0), i.InputDoubleRegister(1)); \
    } else {                                                               \
      USE(HasFPStackSlotInput);                                            \
      DCHECK(HasFPStackSlotInput(instr, 1));                               \
      MemOperand operand = i.InputStackSlot(1);                            \
      if (operand.offset() >= 0) {                                         \
        __ cmp_rm_instr(i.InputDoubleRegister(0), operand);                \
      } else {                                                             \
        __ load_instr(kScratchDoubleReg, operand);                         \
        __ cmp_rr_instr(i.InputDoubleRegister(0), kScratchDoubleReg);      \
      }                                                                    \
    }                                                                      \
  } while (0)

// Divide instruction dr will implicity use register pair
// r0 & r1 below.
// R0:R1 = R1 / divisor - R0 remainder
// Copy remainder to output reg
#define ASSEMBLE_MODULO(div_instr, shift_instr) \
  do {                                          \
    __ LoadRR(r0, i.InputRegister(0));          \
    __ shift_instr(r0, Operand(32));            \
    __ div_instr(r0, i.InputRegister(1));       \
    __ LoadlW(i.OutputRegister(), r0);          \
  } while (0)

#define ASSEMBLE_FLOAT_MODULO()                                             \
  do {                                                                      \
    FrameScope scope(tasm(), StackFrame::MANUAL);                           \
    __ PrepareCallCFunction(0, 2, kScratchReg);                             \
    __ MovToFloatParameters(i.InputDoubleRegister(0),                       \
                            i.InputDoubleRegister(1));                      \
    __ CallCFunction(ExternalReference::mod_two_doubles_operation(), 0, 2); \
    __ MovFromFloatResult(i.OutputDoubleRegister());                        \
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
  } while (0)

#define ASSEMBLE_DOUBLE_MAX()                                          \
  do {                                                                 \
    DoubleRegister left_reg = i.InputDoubleRegister(0);                \
    DoubleRegister right_reg = i.InputDoubleRegister(1);               \
    DoubleRegister result_reg = i.OutputDoubleRegister();              \
    Label check_nan_left, check_zero, return_left, return_right, done; \
    __ cdbr(left_reg, right_reg);                                      \
    __ bunordered(&check_nan_left, Label::kNear);                      \
    __ beq(&check_zero);                                               \
    __ bge(&return_left, Label::kNear);                                \
    __ b(&return_right, Label::kNear);                                 \
                                                                       \
    __ bind(&check_zero);                                              \
    __ lzdr(kDoubleRegZero);                                           \
    __ cdbr(left_reg, kDoubleRegZero);                                 \
    /* left == right != 0. */                                          \
    __ bne(&return_left, Label::kNear);                                \
    /* At this point, both left and right are either 0 or -0. */       \
    /* N.B. The following works because +0 + -0 == +0 */               \
    /* For max we want logical-and of sign bit: (L + R) */             \
    __ ldr(result_reg, left_reg);                                      \
    __ adbr(result_reg, right_reg);                                    \
    __ b(&done, Label::kNear);                                         \
                                                                       \
    __ bind(&check_nan_left);                                          \
    __ cdbr(left_reg, left_reg);                                       \
    /* left == NaN. */                                                 \
    __ bunordered(&return_left, Label::kNear);                         \
                                                                       \
    __ bind(&return_right);                                            \
    if (right_reg != result_reg) {                                     \
      __ ldr(result_reg, right_reg);                                   \
    }                                                                  \
    __ b(&done, Label::kNear);                                         \
                                                                       \
    __ bind(&return_left);                                             \
    if (left_reg != result_reg) {                                      \
      __ ldr(result_reg, left_reg);                                    \
    }                                                                  \
    __ bind(&done);                                                    \
  } while (0)

#define ASSEMBLE_DOUBLE_MIN()                                          \
  do {                                                                 \
    DoubleRegister left_reg = i.InputDoubleRegister(0);                \
    DoubleRegister right_reg = i.InputDoubleRegister(1);               \
    DoubleRegister result_reg = i.OutputDoubleRegister();              \
    Label check_nan_left, check_zero, return_left, return_right, done; \
    __ cdbr(left_reg, right_reg);                                      \
    __ bunordered(&check_nan_left, Label::kNear);                      \
    __ beq(&check_zero);                                               \
    __ ble(&return_left, Label::kNear);                                \
    __ b(&return_right, Label::kNear);                                 \
                                                                       \
    __ bind(&check_zero);                                              \
    __ lzdr(kDoubleRegZero);                                           \
    __ cdbr(left_reg, kDoubleRegZero);                                 \
    /* left == right != 0. */                                          \
    __ bne(&return_left, Label::kNear);                                \
    /* At this point, both left and right are either 0 or -0. */       \
    /* N.B. The following works because +0 + -0 == +0 */               \
    /* For min we want logical-or of sign bit: -(-L + -R) */           \
    __ lcdbr(left_reg, left_reg);                                      \
    __ ldr(result_reg, left_reg);                                      \
    if (left_reg == right_reg) {                                       \
      __ adbr(result_reg, right_reg);                                  \
    } else {                                                           \
      __ sdbr(result_reg, right_reg);                                  \
    }                                                                  \
    __ lcdbr(result_reg, result_reg);                                  \
    __ b(&done, Label::kNear);                                         \
                                                                       \
    __ bind(&check_nan_left);                                          \
    __ cdbr(left_reg, left_reg);                                       \
    /* left == NaN. */                                                 \
    __ bunordered(&return_left, Label::kNear);                         \
                                                                       \
    __ bind(&return_right);                                            \
    if (right_reg != result_reg) {                                     \
      __ ldr(result_reg, right_reg);                                   \
    }                                                                  \
    __ b(&done, Label::kNear);                                         \
                                                                       \
    __ bind(&return_left);                                             \
    if (left_reg != result_reg) {                                      \
      __ ldr(result_reg, left_reg);                                    \
    }                                                                  \
    __ bind(&done);                                                    \
  } while (0)

#define ASSEMBLE_FLOAT_MAX()                                           \
  do {                                                                 \
    DoubleRegister left_reg = i.InputDoubleRegister(0);                \
    DoubleRegister right_reg = i.InputDoubleRegister(1);               \
    DoubleRegister result_reg = i.OutputDoubleRegister();              \
    Label check_nan_left, check_zero, return_left, return_right, done; \
    __ cebr(left_reg, right_reg);                                      \
    __ bunordered(&check_nan_left, Label::kNear);                      \
    __ beq(&check_zero);                                               \
    __ bge(&return_left, Label::kNear);                                \
    __ b(&return_right, Label::kNear);                                 \
                                                                       \
    __ bind(&check_zero);                                              \
    __ lzdr(kDoubleRegZero);                                           \
    __ cebr(left_reg, kDoubleRegZero);                                 \
    /* left == right != 0. */                                          \
    __ bne(&return_left, Label::kNear);                                \
    /* At this point, both left and right are either 0 or -0. */       \
    /* N.B. The following works because +0 + -0 == +0 */               \
    /* For max we want logical-and of sign bit: (L + R) */             \
    __ ldr(result_reg, left_reg);                                      \
    __ aebr(result_reg, right_reg);                                    \
    __ b(&done, Label::kNear);                                         \
                                                                       \
    __ bind(&check_nan_left);                                          \
    __ cebr(left_reg, left_reg);                                       \
    /* left == NaN. */                                                 \
    __ bunordered(&return_left, Label::kNear);                         \
                                                                       \
    __ bind(&return_right);                                            \
    if (right_reg != result_reg) {                                     \
      __ ldr(result_reg, right_reg);                                   \
    }                                                                  \
    __ b(&done, Label::kNear);                                         \
                                                                       \
    __ bind(&return_left);                                             \
    if (left_reg != result_reg) {                                      \
      __ ldr(result_reg, left_reg);                                    \
    }                                                                  \
    __ bind(&done);                                                    \
  } while (0)

#define ASSEMBLE_FLOAT_MIN()                                           \
  do {                                                                 \
    DoubleRegister left_reg = i.InputDoubleRegister(0);                \
    DoubleRegister right_reg = i.InputDoubleRegister(1);               \
    DoubleRegister result_reg = i.OutputDoubleRegister();              \
    Label check_nan_left, check_zero, return_left, return_right, done; \
    __ cebr(left_reg, right_reg);                                      \
    __ bunordered(&check_nan_left, Label::kNear);                      \
    __ beq(&check_zero);                                               \
    __ ble(&return_left, Label::kNear);                                \
    __ b(&return_right, Label::kNear);                                 \
                                                                       \
    __ bind(&check_zero);                                              \
    __ lzdr(kDoubleRegZero);                                           \
    __ cebr(left_reg, kDoubleRegZero);                                 \
    /* left == right != 0. */                                          \
    __ bne(&return_left, Label::kNear);                                \
    /* At this point, both left and right are either 0 or -0. */       \
    /* N.B. The following works because +0 + -0 == +0 */               \
    /* For min we want logical-or of sign bit: -(-L + -R) */           \
    __ lcebr(left_reg, left_reg);                                      \
    __ ldr(result_reg, left_reg);                                      \
    if (left_reg == right_reg) {                                       \
      __ aebr(result_reg, right_reg);                                  \
    } else {                                                           \
      __ sebr(result_reg, right_reg);                                  \
    }                                                                  \
    __ lcebr(result_reg, result_reg);                                  \
    __ b(&done, Label::kNear);                                         \
                                                                       \
    __ bind(&check_nan_left);                                          \
    __ cebr(left_reg, left_reg);                                       \
    /* left == NaN. */                                                 \
    __ bunordered(&return_left, Label::kNear);                         \
                                                                       \
    __ bind(&return_right);                                            \
    if (right_reg != result_reg) {                                     \
      __ ldr(result_reg, right_reg);                                   \
    }                                                                  \
    __ b(&done, Label::kNear);                                         \
                                                                       \
    __ bind(&return_left);                                             \
    if (left_reg != result_reg) {                                      \
      __ ldr(result_reg, left_reg);                                    \
    }                                                                  \
    __ bind(&done);                                                    \
  } while (0)
//
// Only MRI mode for these instructions available
#define ASSEMBLE_LOAD_FLOAT(asm_instr)                \
  do {                                                \
    DoubleRegister result = i.OutputDoubleRegister(); \
    AddressingMode mode = kMode_None;                 \
    MemOperand operand = i.MemoryOperand(&mode);      \
    __ asm_instr(result, operand);                    \
  } while (0)

#define ASSEMBLE_LOAD_INTEGER(asm_instr)         \
  do {                                           \
    Register result = i.OutputRegister();        \
    AddressingMode mode = kMode_None;            \
    MemOperand operand = i.MemoryOperand(&mode); \
    __ asm_instr(result, operand);               \
  } while (0)

#define ASSEMBLE_LOADANDTEST64(asm_instr_rr, asm_instr_rm)              \
  {                                                                     \
    AddressingMode mode = AddressingModeField::decode(instr->opcode()); \
    Register dst = HasRegisterOutput(instr) ? i.OutputRegister() : r0;  \
    if (mode != kMode_None) {                                           \
      size_t first_index = 0;                                           \
      MemOperand operand = i.MemoryOperand(&mode, &first_index);        \
      __ asm_instr_rm(dst, operand);                                    \
    } else if (HasRegisterInput(instr, 0)) {                            \
      __ asm_instr_rr(dst, i.InputRegister(0));                         \
    } else {                                                            \
      DCHECK(HasStackSlotInput(instr, 0));                              \
      __ asm_instr_rm(dst, i.InputStackSlot(0));                        \
    }                                                                   \
  }

#define ASSEMBLE_LOADANDTEST32(asm_instr_rr, asm_instr_rm)              \
  {                                                                     \
    AddressingMode mode = AddressingModeField::decode(instr->opcode()); \
    Register dst = HasRegisterOutput(instr) ? i.OutputRegister() : r0;  \
    if (mode != kMode_None) {                                           \
      size_t first_index = 0;                                           \
      MemOperand operand = i.MemoryOperand(&mode, &first_index);        \
      __ asm_instr_rm(dst, operand);                                    \
    } else if (HasRegisterInput(instr, 0)) {                            \
      __ asm_instr_rr(dst, i.InputRegister(0));                         \
    } else {                                                            \
      DCHECK(HasStackSlotInput(instr, 0));                              \
      __ asm_instr_rm(dst, i.InputStackSlot32(0));                      \
    }                                                                   \
  }

#define ASSEMBLE_STORE_FLOAT32()                         \
  do {                                                   \
    size_t index = 0;                                    \
    AddressingMode mode = kMode_None;                    \
    MemOperand operand = i.MemoryOperand(&mode, &index); \
    DoubleRegister value = i.InputDoubleRegister(index); \
    __ StoreFloat32(value, operand);                     \
  } while (0)

#define ASSEMBLE_STORE_DOUBLE()                          \
  do {                                                   \
    size_t index = 0;                                    \
    AddressingMode mode = kMode_None;                    \
    MemOperand operand = i.MemoryOperand(&mode, &index); \
    DoubleRegister value = i.InputDoubleRegister(index); \
    __ StoreDouble(value, operand);                      \
  } while (0)

#define ASSEMBLE_STORE_INTEGER(asm_instr)                \
  do {                                                   \
    size_t index = 0;                                    \
    AddressingMode mode = kMode_None;                    \
    MemOperand operand = i.MemoryOperand(&mode, &index); \
    Register value = i.InputRegister(index);             \
    __ asm_instr(value, operand);                        \
  } while (0)

#define ATOMIC_COMP_EXCHANGE(start, end, shift_amount, offset)              \
  {                                                                         \
    __ LoadlW(temp0, MemOperand(addr, offset));                             \
    __ llgfr(temp1, temp0);                                                 \
    __ RotateInsertSelectBits(temp0, old_val, Operand(start), Operand(end), \
                              Operand(shift_amount), false);                \
    __ RotateInsertSelectBits(temp1, new_val, Operand(start), Operand(end), \
                              Operand(shift_amount), false);                \
    __ CmpAndSwap(temp0, temp1, MemOperand(addr, offset));                  \
    __ RotateInsertSelectBits(output, temp0, Operand(start + shift_amount), \
                              Operand(end + shift_amount),                  \
                              Operand(64 - shift_amount), true);            \
  }

#ifdef V8_TARGET_BIG_ENDIAN
#define ATOMIC_COMP_EXCHANGE_BYTE(i)                             \
  {                                                              \
    constexpr int idx = (i);                                     \
    static_assert(idx <= 3 && idx >= 0, "idx is out of range!"); \
    constexpr int start = 32 + 8 * idx;                          \
    constexpr int end = start + 7;                               \
    constexpr int shift_amount = (3 - idx) * 8;                  \
    ATOMIC_COMP_EXCHANGE(start, end, shift_amount, -idx);        \
  }
#define ATOMIC_COMP_EXCHANGE_HALFWORD(i)                         \
  {                                                              \
    constexpr int idx = (i);                                     \
    static_assert(idx <= 1 && idx >= 0, "idx is out of range!"); \
    constexpr int start = 32 + 16 * idx;                         \
    constexpr int end = start + 15;                              \
    constexpr int shift_amount = (1 - idx) * 16;                 \
    ATOMIC_COMP_EXCHANGE(start, end, shift_amount, -idx * 2);    \
  }
#else
#define ATOMIC_COMP_EXCHANGE_BYTE(i)                             \
  {                                                              \
    constexpr int idx = (i);                                     \
    static_assert(idx <= 3 && idx >= 0, "idx is out of range!"); \
    constexpr int start = 32 + 8 * (3 - idx);                    \
    constexpr int end = start + 7;                               \
    constexpr int shift_amount = idx * 8;                        \
    ATOMIC_COMP_EXCHANGE(start, end, shift_amount, -idx);        \
  }
#define ATOMIC_COMP_EXCHANGE_HALFWORD(i)                         \
  {                                                              \
    constexpr int idx = (i);                                     \
    static_assert(idx <= 1 && idx >= 0, "idx is out of range!"); \
    constexpr int start = 32 + 16 * (1 - idx);                   \
    constexpr int end = start + 15;                              \
    constexpr int shift_amount = idx * 16;                       \
    ATOMIC_COMP_EXCHANGE(start, end, shift_amount, -idx * 2);    \
  }
#endif

#define ASSEMBLE_ATOMIC_COMPARE_EXCHANGE_BYTE(load_and_ext) \
  do {                                                      \
    Register old_val = i.InputRegister(0);                  \
    Register new_val = i.InputRegister(1);                  \
    Register output = i.OutputRegister();                   \
    Register addr = kScratchReg;                            \
    Register temp0 = r0;                                    \
    Register temp1 = r1;                                    \
    size_t index = 2;                                       \
    AddressingMode mode = kMode_None;                       \
    MemOperand op = i.MemoryOperand(&mode, &index);         \
    Label three, two, one, done;                            \
    __ lay(addr, op);                                       \
    __ tmll(addr, Operand(3));                              \
    __ b(Condition(1), &three);                             \
    __ b(Condition(2), &two);                               \
    __ b(Condition(4), &one);                               \
    /* ending with 0b00 */                                  \
    ATOMIC_COMP_EXCHANGE_BYTE(0);                           \
    __ b(&done);                                            \
    /* ending with 0b01 */                                  \
    __ bind(&one);                                          \
    ATOMIC_COMP_EXCHANGE_BYTE(1);                           \
    __ b(&done);                                            \
    /* ending with 0b10 */                                  \
    __ bind(&two);                                          \
    ATOMIC_COMP_EXCHANGE_BYTE(2);                           \
    __ b(&done);                                            \
    /* ending with 0b11 */                                  \
    __ bind(&three);                                        \
    ATOMIC_COMP_EXCHANGE_BYTE(3);                           \
    __ bind(&done);                                         \
    __ load_and_ext(output, output);                        \
  } while (false)

#define ASSEMBLE_ATOMIC_COMPARE_EXCHANGE_HALFWORD(load_and_ext) \
  do {                                                          \
    Register old_val = i.InputRegister(0);                      \
    Register new_val = i.InputRegister(1);                      \
    Register output = i.OutputRegister();                       \
    Register addr = kScratchReg;                                \
    Register temp0 = r0;                                        \
    Register temp1 = r1;                                        \
    size_t index = 2;                                           \
    AddressingMode mode = kMode_None;                           \
    MemOperand op = i.MemoryOperand(&mode, &index);             \
    Label two, done;                                            \
    __ lay(addr, op);                                           \
    __ tmll(addr, Operand(3));                                  \
    __ b(Condition(2), &two);                                   \
    ATOMIC_COMP_EXCHANGE_HALFWORD(0);                           \
    __ b(&done);                                                \
    __ bind(&two);                                              \
    ATOMIC_COMP_EXCHANGE_HALFWORD(1);                           \
    __ bind(&done);                                             \
    __ load_and_ext(output, output);                            \
  } while (false)

#define ASSEMBLE_ATOMIC_COMPARE_EXCHANGE_WORD()       \
  do {                                                \
    Register new_val = i.InputRegister(1);            \
    Register output = i.OutputRegister();             \
    Register addr = kScratchReg;                      \
    size_t index = 2;                                 \
    AddressingMode mode = kMode_None;                 \
    MemOperand op = i.MemoryOperand(&mode, &index);   \
    __ lay(addr, op);                                 \
    __ CmpAndSwap(output, new_val, MemOperand(addr)); \
    __ LoadlW(output, output);                        \
  } while (false)

#define ASSEMBLE_ATOMIC_BINOP_WORD(load_and_op)      \
  do {                                               \
    Register value = i.InputRegister(2);             \
    Register result = i.OutputRegister(0);           \
    Register addr = r1;                              \
    AddressingMode mode = kMode_None;                \
    MemOperand op = i.MemoryOperand(&mode);          \
    __ lay(addr, op);                                \
    __ load_and_op(result, value, MemOperand(addr)); \
    __ LoadlW(result, result);                       \
  } while (false)

#define ASSEMBLE_ATOMIC_BINOP_WORD64(load_and_op)    \
  do {                                               \
    Register value = i.InputRegister(2);             \
    Register result = i.OutputRegister(0);           \
    Register addr = r1;                              \
    AddressingMode mode = kMode_None;                \
    MemOperand op = i.MemoryOperand(&mode);          \
    __ lay(addr, op);                                \
    __ load_and_op(result, value, MemOperand(addr)); \
  } while (false)

#define ATOMIC_BIN_OP(bin_inst, offset, shift_amount, start, end)           \
  do {                                                                      \
    Label do_cs;                                                            \
    __ LoadlW(prev, MemOperand(addr, offset));                              \
    __ bind(&do_cs);                                                        \
    __ RotateInsertSelectBits(temp, value, Operand(start), Operand(end),    \
                              Operand(static_cast<intptr_t>(shift_amount)), \
                              true);                                        \
    __ bin_inst(new_val, prev, temp);                                       \
    __ lr(temp, prev);                                                      \
    __ RotateInsertSelectBits(temp, new_val, Operand(start), Operand(end),  \
                              Operand::Zero(), false);                      \
    __ CmpAndSwap(prev, temp, MemOperand(addr, offset));                    \
    __ bne(&do_cs, Label::kNear);                                           \
  } while (false)

#ifdef V8_TARGET_BIG_ENDIAN
#define ATOMIC_BIN_OP_HALFWORD(bin_inst, index, extract_result) \
  {                                                             \
    constexpr int offset = -(2 * index);                        \
    constexpr int shift_amount = 16 - (index * 16);             \
    constexpr int start = 48 - shift_amount;                    \
    constexpr int end = start + 15;                             \
    ATOMIC_BIN_OP(bin_inst, offset, shift_amount, start, end);  \
    extract_result();                                           \
  }
#define ATOMIC_BIN_OP_BYTE(bin_inst, index, extract_result)    \
  {                                                            \
    constexpr int offset = -(index);                           \
    constexpr int shift_amount = 24 - (index * 8);             \
    constexpr int start = 56 - shift_amount;                   \
    constexpr int end = start + 7;                             \
    ATOMIC_BIN_OP(bin_inst, offset, shift_amount, start, end); \
    extract_result();                                          \
  }
#else
#define ATOMIC_BIN_OP_HALFWORD(bin_inst, index, extract_result) \
  {                                                             \
    constexpr int offset = -(2 * index);                        \
    constexpr int shift_amount = index * 16;                    \
    constexpr int start = 48 - shift_amount;                    \
    constexpr int end = start + 15;                             \
    ATOMIC_BIN_OP(bin_inst, offset, shift_amount, start, end);  \
    extract_result();                                           \
  }
#define ATOMIC_BIN_OP_BYTE(bin_inst, index, extract_result)    \
  {                                                            \
    constexpr int offset = -(index);                           \
    constexpr int shift_amount = index * 8;                    \
    constexpr int start = 56 - shift_amount;                   \
    constexpr int end = start + 7;                             \
    ATOMIC_BIN_OP(bin_inst, offset, shift_amount, start, end); \
    extract_result();                                          \
  }
#endif  // V8_TARGET_BIG_ENDIAN

#define ASSEMBLE_ATOMIC_BINOP_HALFWORD(bin_inst, extract_result) \
  do {                                                           \
    Register value = i.InputRegister(2);                         \
    Register result = i.OutputRegister(0);                       \
    Register prev = i.TempRegister(0);                           \
    Register new_val = r0;                                       \
    Register addr = r1;                                          \
    Register temp = kScratchReg;                                 \
    AddressingMode mode = kMode_None;                            \
    MemOperand op = i.MemoryOperand(&mode);                      \
    Label two, done;                                             \
    __ lay(addr, op);                                            \
    __ tmll(addr, Operand(3));                                   \
    __ b(Condition(2), &two);                                    \
    /* word boundary */                                          \
    ATOMIC_BIN_OP_HALFWORD(bin_inst, 0, extract_result);         \
    __ b(&done);                                                 \
    __ bind(&two);                                               \
    /* halfword boundary */                                      \
    ATOMIC_BIN_OP_HALFWORD(bin_inst, 1, extract_result);         \
    __ bind(&done);                                              \
  } while (false)

#define ASSEMBLE_ATOMIC_BINOP_BYTE(bin_inst, extract_result) \
  do {                                                       \
    Register value = i.InputRegister(2);                     \
    Register result = i.OutputRegister(0);                   \
    Register addr = i.TempRegister(0);                       \
    Register prev = r0;                                      \
    Register new_val = r1;                                   \
    Register temp = kScratchReg;                             \
    AddressingMode mode = kMode_None;                        \
    MemOperand op = i.MemoryOperand(&mode);                  \
    Label done, one, two, three;                             \
    __ lay(addr, op);                                        \
    __ tmll(addr, Operand(3));                               \
    __ b(Condition(1), &three);                              \
    __ b(Condition(2), &two);                                \
    __ b(Condition(4), &one);                                \
    /* ending with 0b00 (word boundary) */                   \
    ATOMIC_BIN_OP_BYTE(bin_inst, 0, extract_result);         \
    __ b(&done);                                             \
    /* ending with 0b01 */                                   \
    __ bind(&one);                                           \
    ATOMIC_BIN_OP_BYTE(bin_inst, 1, extract_result);         \
    __ b(&done);                                             \
    /* ending with 0b10 (hw boundary) */                     \
    __ bind(&two);                                           \
    ATOMIC_BIN_OP_BYTE(bin_inst, 2, extract_result);         \
    __ b(&done);                                             \
    /* ending with 0b11 */                                   \
    __ bind(&three);                                         \
    ATOMIC_BIN_OP_BYTE(bin_inst, 3, extract_result);         \
    __ bind(&done);                                          \
  } while (false)

#define ASSEMBLE_ATOMIC64_COMP_EXCHANGE_WORD64()        \
  do {                                                  \
    Register new_val = i.InputRegister(1);              \
    Register output = i.OutputRegister();               \
    Register addr = kScratchReg;                        \
    size_t index = 2;                                   \
    AddressingMode mode = kMode_None;                   \
    MemOperand op = i.MemoryOperand(&mode, &index);     \
    __ lay(addr, op);                                   \
    __ CmpAndSwap64(output, new_val, MemOperand(addr)); \
  } while (false)

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
  __ CmpP(scratch1,
          Operand(StackFrame::TypeToMarker(StackFrame::ARGUMENTS_ADAPTOR)));
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
    tasm->AddP(sp, sp, Operand(-stack_slot_delta * kSystemPointerSize));
    state->IncreaseSPDelta(stack_slot_delta);
  } else if (allow_shrinkage && stack_slot_delta < 0) {
    if (pending_pushes != nullptr) {
      FlushPendingPushRegisters(tasm, state, pending_pushes);
    }
    tasm->AddP(sp, sp, Operand(-stack_slot_delta * kSystemPointerSize));
    state->IncreaseSPDelta(stack_slot_delta);
  }
}

void EmitWordLoadPoisoningIfNeeded(CodeGenerator* codegen, Instruction* instr,
                                   S390OperandConverter& i) {
  const MemoryAccessMode access_mode =
      static_cast<MemoryAccessMode>(MiscField::decode(instr->opcode()));
  if (access_mode == kMemoryAccessPoisoned) {
    Register value = i.OutputRegister();
    codegen->tasm()->AndP(value, kSpeculationPoisonRegister);
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
    S390OperandConverter g(this, instr);
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
  Register scratch = r1;
  __ ComputeCodeStartAddress(scratch);
  __ CmpP(scratch, kJavaScriptCallCodeStartRegister);
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
    __ CmpP(ip, kJavaScriptCallCodeStartRegister);
    __ Assert(eq, AbortReason::kWrongFunctionCodeStart);
  }

  int offset = Code::kCodeDataContainerOffset - Code::kHeaderSize;
  __ LoadP(ip, MemOperand(kJavaScriptCallCodeStartRegister, offset));
  __ LoadW(ip,
           FieldMemOperand(ip, CodeDataContainer::kKindSpecificFlagsOffset));
  __ TestBit(ip, Code::kMarkedForDeoptimizationBit);
  __ Jump(BUILTIN_CODE(isolate(), CompileLazyDeoptimizedCode),
          RelocInfo::CODE_TARGET, ne);
}

void CodeGenerator::GenerateSpeculationPoisonFromCodeStartRegister() {
  Register scratch = r1;

  __ ComputeCodeStartAddress(scratch);

  // Calculate a mask which has all bits set in the normal case, but has all
  // bits cleared if we are speculatively executing the wrong PC.
  __ LoadImmP(kSpeculationPoisonRegister, Operand::Zero());
  __ LoadImmP(r0, Operand(-1));
  __ CmpP(kJavaScriptCallCodeStartRegister, scratch);
  __ LoadOnConditionP(eq, kSpeculationPoisonRegister, r0);
}

void CodeGenerator::AssembleRegisterArgumentPoisoning() {
  __ AndP(kJSFunctionRegister, kJSFunctionRegister, kSpeculationPoisonRegister);
  __ AndP(kContextRegister, kContextRegister, kSpeculationPoisonRegister);
  __ AndP(sp, sp, kSpeculationPoisonRegister);
}

// Assembles an instruction after register allocation, producing machine code.
CodeGenerator::CodeGenResult CodeGenerator::AssembleArchInstruction(
    Instruction* instr) {
  S390OperandConverter i(this, instr);
  ArchOpcode opcode = ArchOpcodeField::decode(instr->opcode());

  switch (opcode) {
    case kArchComment:
#ifdef V8_TARGET_ARCH_S390X
      __ RecordComment(reinterpret_cast<const char*>(i.InputInt64(0)));
#else
      __ RecordComment(reinterpret_cast<const char*>(i.InputInt32(0)));
#endif
      break;
    case kArchCallCodeObject: {
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
      // We must not share code targets for calls to builtins for wasm code, as
      // they might need to be patched individually.
      if (instr->InputAt(0)->IsImmediate()) {
        Constant constant = i.ToConstant(instr->InputAt(0));
#ifdef V8_TARGET_ARCH_S390X
        Address wasm_code = static_cast<Address>(constant.ToInt64());
#else
        Address wasm_code = static_cast<Address>(constant.ToInt32());
#endif
        __ Call(wasm_code, constant.rmode());
      } else {
        __ Call(i.InputRegister(0));
      }
      RecordCallPosition(instr);
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
      Register func = i.InputRegister(0);
      if (FLAG_debug_code) {
        // Check the function's context matches the context argument.
        __ LoadP(kScratchReg,
                 FieldMemOperand(func, JSFunction::kContextOffset));
        __ CmpP(cp, kScratchReg);
        __ Assert(eq, AbortReason::kWrongFunctionContext);
      }
      static_assert(kJavaScriptCallCodeStartRegister == r4, "ABI mismatch");
      __ LoadP(r4, FieldMemOperand(func, JSFunction::kCodeOffset));
      __ CallCodeObject(r4);
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
        frame_access_state()->IncreaseSPDelta(bytes / kSystemPointerSize);
      }
      break;
    }
    case kArchJmp:
      AssembleArchJump(i.InputRpo(0));
      break;
    case kArchBinarySearchSwitch:
      AssembleArchBinarySearchSwitch(instr);
      break;
    case kArchLookupSwitch:
      AssembleArchLookupSwitch(instr);
      break;
    case kArchTableSwitch:
      AssembleArchTableSwitch(instr);
      break;
    case kArchDebugAbort:
      DCHECK(i.InputRegister(0) == r3);
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
      __ LoadRR(i.OutputRegister(), sp);
      break;
    case kArchFramePointer:
      __ LoadRR(i.OutputRegister(), fp);
      break;
    case kArchParentFramePointer:
      if (frame_access_state()->has_frame()) {
        __ LoadP(i.OutputRegister(), MemOperand(fp, 0));
      } else {
        __ LoadRR(i.OutputRegister(), fp);
      }
      break;
    case kArchTruncateDoubleToI:
      __ TruncateDoubleToI(isolate(), zone(), i.OutputRegister(),
                           i.InputDoubleRegister(0), DetermineStubCallMode());
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
        ool = new (zone())
            OutOfLineRecordWrite(this, object, offset, value, scratch0,
                                 scratch1, mode, DetermineStubCallMode());
        __ StoreP(value, MemOperand(object, offset));
      } else {
        DCHECK_EQ(kMode_MRR, addressing_mode);
        Register offset(i.InputRegister(1));
        ool = new (zone())
            OutOfLineRecordWrite(this, object, offset, value, scratch0,
                                 scratch1, mode, DetermineStubCallMode());
        __ StoreP(value, MemOperand(object, offset));
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
      __ AddP(i.OutputRegister(), offset.from_stack_pointer() ? sp : fp,
              Operand(offset.offset()));
      break;
    }
    case kArchWordPoisonOnSpeculation:
      DCHECK_EQ(i.OutputRegister(), i.InputRegister(0));
      __ AndP(i.InputRegister(0), kSpeculationPoisonRegister);
      break;
    case kS390_Peek: {
      // The incoming value is 0-based, but we need a 1-based value.
      int reverse_slot = i.InputInt32(0) + 1;
      int offset =
          FrameSlotToFPOffset(frame()->GetTotalFrameSlotCount() - reverse_slot);
      if (instr->OutputAt(0)->IsFPRegister()) {
        LocationOperand* op = LocationOperand::cast(instr->OutputAt(0));
        if (op->representation() == MachineRepresentation::kFloat64) {
          __ LoadDouble(i.OutputDoubleRegister(), MemOperand(fp, offset));
        } else {
          DCHECK_EQ(MachineRepresentation::kFloat32, op->representation());
          __ LoadFloat32(i.OutputFloatRegister(), MemOperand(fp, offset));
        }
      } else {
        __ LoadP(i.OutputRegister(), MemOperand(fp, offset));
      }
      break;
    }
    case kS390_Abs32:
      // TODO(john.yan): zero-ext
      __ lpr(i.OutputRegister(0), i.InputRegister(0));
      break;
    case kS390_Abs64:
      __ lpgr(i.OutputRegister(0), i.InputRegister(0));
      break;
    case kS390_And32:
      // zero-ext
      if (CpuFeatures::IsSupported(DISTINCT_OPS)) {
        ASSEMBLE_BIN32_OP(RRRInstr(nrk), RM32Instr(And), RIInstr(nilf));
      } else {
        ASSEMBLE_BIN32_OP(RRInstr(nr), RM32Instr(And), RIInstr(nilf));
      }
      break;
    case kS390_And64:
      if (CpuFeatures::IsSupported(DISTINCT_OPS)) {
        ASSEMBLE_BIN_OP(RRRInstr(ngrk), RM64Instr(ng), nullInstr);
      } else {
        ASSEMBLE_BIN_OP(RRInstr(ngr), RM64Instr(ng), nullInstr);
      }
      break;
    case kS390_Or32:
      // zero-ext
      if (CpuFeatures::IsSupported(DISTINCT_OPS)) {
        ASSEMBLE_BIN32_OP(RRRInstr(ork), RM32Instr(Or), RIInstr(oilf));
      } else {
        ASSEMBLE_BIN32_OP(RRInstr(or_z), RM32Instr(Or), RIInstr(oilf));
      }
      break;
    case kS390_Or64:
      if (CpuFeatures::IsSupported(DISTINCT_OPS)) {
        ASSEMBLE_BIN_OP(RRRInstr(ogrk), RM64Instr(og), nullInstr);
      } else {
        ASSEMBLE_BIN_OP(RRInstr(ogr), RM64Instr(og), nullInstr);
      }
      break;
    case kS390_Xor32:
      // zero-ext
      if (CpuFeatures::IsSupported(DISTINCT_OPS)) {
        ASSEMBLE_BIN32_OP(RRRInstr(xrk), RM32Instr(Xor), RIInstr(xilf));
      } else {
        ASSEMBLE_BIN32_OP(RRInstr(xr), RM32Instr(Xor), RIInstr(xilf));
      }
      break;
    case kS390_Xor64:
      if (CpuFeatures::IsSupported(DISTINCT_OPS)) {
        ASSEMBLE_BIN_OP(RRRInstr(xgrk), RM64Instr(xg), nullInstr);
      } else {
        ASSEMBLE_BIN_OP(RRInstr(xgr), RM64Instr(xg), nullInstr);
      }
      break;
    case kS390_ShiftLeft32:
      // zero-ext
      if (CpuFeatures::IsSupported(DISTINCT_OPS)) {
        ASSEMBLE_BIN32_OP(RRRInstr(ShiftLeft), nullInstr, RRIInstr(ShiftLeft));
      } else {
        ASSEMBLE_BIN32_OP(RRInstr(sll), nullInstr, RIInstr(sll));
      }
      break;
    case kS390_ShiftLeft64:
      ASSEMBLE_BIN_OP(RRRInstr(sllg), nullInstr, RRIInstr(sllg));
      break;
    case kS390_ShiftRight32:
      // zero-ext
      if (CpuFeatures::IsSupported(DISTINCT_OPS)) {
        ASSEMBLE_BIN32_OP(RRRInstr(srlk), nullInstr, RRIInstr(srlk));
      } else {
        ASSEMBLE_BIN32_OP(RRInstr(srl), nullInstr, RIInstr(srl));
      }
      break;
    case kS390_ShiftRight64:
      ASSEMBLE_BIN_OP(RRRInstr(srlg), nullInstr, RRIInstr(srlg));
      break;
    case kS390_ShiftRightArith32:
      // zero-ext
      if (CpuFeatures::IsSupported(DISTINCT_OPS)) {
        ASSEMBLE_BIN32_OP(RRRInstr(srak), nullInstr, RRIInstr(srak));
      } else {
        ASSEMBLE_BIN32_OP(RRInstr(sra), nullInstr, RIInstr(sra));
      }
      break;
    case kS390_ShiftRightArith64:
      ASSEMBLE_BIN_OP(RRRInstr(srag), nullInstr, RRIInstr(srag));
      break;
#if !V8_TARGET_ARCH_S390X
    case kS390_AddPair:
      // i.InputRegister(0) ... left low word.
      // i.InputRegister(1) ... left high word.
      // i.InputRegister(2) ... right low word.
      // i.InputRegister(3) ... right high word.
      __ AddLogical32(i.OutputRegister(0), i.InputRegister(0),
                      i.InputRegister(2));
      __ AddLogicalWithCarry32(i.OutputRegister(1), i.InputRegister(1),
                               i.InputRegister(3));
      break;
    case kS390_SubPair:
      // i.InputRegister(0) ... left low word.
      // i.InputRegister(1) ... left high word.
      // i.InputRegister(2) ... right low word.
      // i.InputRegister(3) ... right high word.
      __ SubLogical32(i.OutputRegister(0), i.InputRegister(0),
                      i.InputRegister(2));
      __ SubLogicalWithBorrow32(i.OutputRegister(1), i.InputRegister(1),
                                i.InputRegister(3));
      break;
    case kS390_MulPair:
      // i.InputRegister(0) ... left low word.
      // i.InputRegister(1) ... left high word.
      // i.InputRegister(2) ... right low word.
      // i.InputRegister(3) ... right high word.
      __ sllg(r0, i.InputRegister(1), Operand(32));
      __ sllg(r1, i.InputRegister(3), Operand(32));
      __ lr(r0, i.InputRegister(0));
      __ lr(r1, i.InputRegister(2));
      __ msgr(r1, r0);
      __ lr(i.OutputRegister(0), r1);
      __ srag(i.OutputRegister(1), r1, Operand(32));
      break;
    case kS390_ShiftLeftPair: {
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
    case kS390_ShiftRightPair: {
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
    case kS390_ShiftRightArithPair: {
      Register second_output =
          instr->OutputCount() >= 2 ? i.OutputRegister(1) : i.TempRegister(0);
      if (instr->InputAt(2)->IsImmediate()) {
        __ ShiftRightArithPair(i.OutputRegister(0), second_output,
                               i.InputRegister(0), i.InputRegister(1),
                               i.InputInt32(2));
      } else {
        __ ShiftRightArithPair(i.OutputRegister(0), second_output,
                               i.InputRegister(0), i.InputRegister(1),
                               kScratchReg, i.InputRegister(2));
      }
      break;
    }
#endif
    case kS390_RotRight32: {
      // zero-ext
      if (HasRegisterInput(instr, 1)) {
        __ LoadComplementRR(kScratchReg, i.InputRegister(1));
        __ rll(i.OutputRegister(), i.InputRegister(0), kScratchReg);
      } else {
        __ rll(i.OutputRegister(), i.InputRegister(0),
               Operand(32 - i.InputInt32(1)));
      }
      CHECK_AND_ZERO_EXT_OUTPUT(2);
      break;
    }
    case kS390_RotRight64:
      if (HasRegisterInput(instr, 1)) {
        __ lcgr(kScratchReg, i.InputRegister(1));
        __ rllg(i.OutputRegister(), i.InputRegister(0), kScratchReg);
      } else {
        DCHECK(HasImmediateInput(instr, 1));
        __ rllg(i.OutputRegister(), i.InputRegister(0),
                Operand(64 - i.InputInt32(1)));
      }
      break;
    // TODO(john.yan): clean up kS390_RotLeftAnd...
    case kS390_RotLeftAndClear64:
      if (CpuFeatures::IsSupported(GENERAL_INSTR_EXT)) {
        int shiftAmount = i.InputInt32(1);
        int endBit = 63 - shiftAmount;
        int startBit = 63 - i.InputInt32(2);
        __ RotateInsertSelectBits(i.OutputRegister(), i.InputRegister(0),
                                  Operand(startBit), Operand(endBit),
                                  Operand(shiftAmount), true);
      } else {
        int shiftAmount = i.InputInt32(1);
        int clearBit = 63 - i.InputInt32(2);
        __ rllg(i.OutputRegister(), i.InputRegister(0), Operand(shiftAmount));
        __ sllg(i.OutputRegister(), i.OutputRegister(), Operand(clearBit));
        __ srlg(i.OutputRegister(), i.OutputRegister(),
                Operand(clearBit + shiftAmount));
        __ sllg(i.OutputRegister(), i.OutputRegister(), Operand(shiftAmount));
      }
      break;
    case kS390_RotLeftAndClearLeft64:
      if (CpuFeatures::IsSupported(GENERAL_INSTR_EXT)) {
        int shiftAmount = i.InputInt32(1);
        int endBit = 63;
        int startBit = 63 - i.InputInt32(2);
        __ RotateInsertSelectBits(i.OutputRegister(), i.InputRegister(0),
                                  Operand(startBit), Operand(endBit),
                                  Operand(shiftAmount), true);
      } else {
        int shiftAmount = i.InputInt32(1);
        int clearBit = 63 - i.InputInt32(2);
        __ rllg(i.OutputRegister(), i.InputRegister(0), Operand(shiftAmount));
        __ sllg(i.OutputRegister(), i.OutputRegister(), Operand(clearBit));
        __ srlg(i.OutputRegister(), i.OutputRegister(), Operand(clearBit));
      }
      break;
    case kS390_RotLeftAndClearRight64:
      if (CpuFeatures::IsSupported(GENERAL_INSTR_EXT)) {
        int shiftAmount = i.InputInt32(1);
        int endBit = 63 - i.InputInt32(2);
        int startBit = 0;
        __ RotateInsertSelectBits(i.OutputRegister(), i.InputRegister(0),
                                  Operand(startBit), Operand(endBit),
                                  Operand(shiftAmount), true);
      } else {
        int shiftAmount = i.InputInt32(1);
        int clearBit = i.InputInt32(2);
        __ rllg(i.OutputRegister(), i.InputRegister(0), Operand(shiftAmount));
        __ srlg(i.OutputRegister(), i.OutputRegister(), Operand(clearBit));
        __ sllg(i.OutputRegister(), i.OutputRegister(), Operand(clearBit));
      }
      break;
    case kS390_Add32: {
      // zero-ext
      if (CpuFeatures::IsSupported(DISTINCT_OPS)) {
        ASSEMBLE_BIN32_OP(RRRInstr(ark), RM32Instr(Add32), RRIInstr(Add32));
      } else {
        ASSEMBLE_BIN32_OP(RRInstr(ar), RM32Instr(Add32), RIInstr(Add32));
      }
      break;
    }
    case kS390_Add64:
      if (CpuFeatures::IsSupported(DISTINCT_OPS)) {
        ASSEMBLE_BIN_OP(RRRInstr(agrk), RM64Instr(ag), RRIInstr(AddP));
      } else {
        ASSEMBLE_BIN_OP(RRInstr(agr), RM64Instr(ag), RIInstr(agfi));
      }
      break;
    case kS390_AddFloat:
      ASSEMBLE_BIN_OP(DDInstr(aebr), DMTInstr(AddFloat32), nullInstr);
      break;
    case kS390_AddDouble:
      ASSEMBLE_BIN_OP(DDInstr(adbr), DMTInstr(AddFloat64), nullInstr);
      break;
    case kS390_Sub32:
      // zero-ext
      if (CpuFeatures::IsSupported(DISTINCT_OPS)) {
        ASSEMBLE_BIN32_OP(RRRInstr(srk), RM32Instr(Sub32), RRIInstr(Sub32));
      } else {
        ASSEMBLE_BIN32_OP(RRInstr(sr), RM32Instr(Sub32), RIInstr(Sub32));
      }
      break;
    case kS390_Sub64:
      if (CpuFeatures::IsSupported(DISTINCT_OPS)) {
        ASSEMBLE_BIN_OP(RRRInstr(sgrk), RM64Instr(sg), RRIInstr(SubP));
      } else {
        ASSEMBLE_BIN_OP(RRInstr(sgr), RM64Instr(sg), RIInstr(SubP));
      }
      break;
    case kS390_SubFloat:
      ASSEMBLE_BIN_OP(DDInstr(sebr), DMTInstr(SubFloat32), nullInstr);
      break;
    case kS390_SubDouble:
      ASSEMBLE_BIN_OP(DDInstr(sdbr), DMTInstr(SubFloat64), nullInstr);
      break;
    case kS390_Mul32:
      // zero-ext
      if (CpuFeatures::IsSupported(MISC_INSTR_EXT2)) {
        ASSEMBLE_BIN32_OP(RRRInstr(msrkc), RM32Instr(msc), RIInstr(Mul32));
      } else {
        ASSEMBLE_BIN32_OP(RRInstr(Mul32), RM32Instr(Mul32), RIInstr(Mul32));
      }
      break;
    case kS390_Mul32WithOverflow:
      // zero-ext
      ASSEMBLE_BIN32_OP(RRRInstr(Mul32WithOverflowIfCCUnequal),
                        RRM32Instr(Mul32WithOverflowIfCCUnequal),
                        RRIInstr(Mul32WithOverflowIfCCUnequal));
      break;
    case kS390_Mul64:
      ASSEMBLE_BIN_OP(RRInstr(Mul64), RM64Instr(Mul64), RIInstr(Mul64));
      break;
    case kS390_MulHigh32:
      // zero-ext
      ASSEMBLE_BIN_OP(RRRInstr(MulHigh32), RRM32Instr(MulHigh32),
                      RRIInstr(MulHigh32));
      break;
    case kS390_MulHighU32:
      // zero-ext
      ASSEMBLE_BIN_OP(RRRInstr(MulHighU32), RRM32Instr(MulHighU32),
                      RRIInstr(MulHighU32));
      break;
    case kS390_MulFloat:
      ASSEMBLE_BIN_OP(DDInstr(meebr), DMTInstr(MulFloat32), nullInstr);
      break;
    case kS390_MulDouble:
      ASSEMBLE_BIN_OP(DDInstr(mdbr), DMTInstr(MulFloat64), nullInstr);
      break;
    case kS390_Div64:
      ASSEMBLE_BIN_OP(RRRInstr(Div64), RRM64Instr(Div64), nullInstr);
      break;
    case kS390_Div32: {
      // zero-ext
      ASSEMBLE_BIN_OP(RRRInstr(Div32), RRM32Instr(Div32), nullInstr);
      break;
    }
    case kS390_DivU64:
      ASSEMBLE_BIN_OP(RRRInstr(DivU64), RRM64Instr(DivU64), nullInstr);
      break;
    case kS390_DivU32: {
      // zero-ext
      ASSEMBLE_BIN_OP(RRRInstr(DivU32), RRM32Instr(DivU32), nullInstr);
      break;
    }
    case kS390_DivFloat:
      ASSEMBLE_BIN_OP(DDInstr(debr), DMTInstr(DivFloat32), nullInstr);
      break;
    case kS390_DivDouble:
      ASSEMBLE_BIN_OP(DDInstr(ddbr), DMTInstr(DivFloat64), nullInstr);
      break;
    case kS390_Mod32:
      // zero-ext
      ASSEMBLE_BIN_OP(RRRInstr(Mod32), RRM32Instr(Mod32), nullInstr);
      break;
    case kS390_ModU32:
      // zero-ext
      ASSEMBLE_BIN_OP(RRRInstr(ModU32), RRM32Instr(ModU32), nullInstr);
      break;
    case kS390_Mod64:
      ASSEMBLE_BIN_OP(RRRInstr(Mod64), RRM64Instr(Mod64), nullInstr);
      break;
    case kS390_ModU64:
      ASSEMBLE_BIN_OP(RRRInstr(ModU64), RRM64Instr(ModU64), nullInstr);
      break;
    case kS390_AbsFloat:
      __ lpebr(i.OutputDoubleRegister(), i.InputDoubleRegister(0));
      break;
    case kS390_SqrtFloat:
      ASSEMBLE_UNARY_OP(D_DInstr(sqebr), nullInstr, nullInstr);
      break;
    case kS390_SqrtDouble:
      ASSEMBLE_UNARY_OP(D_DInstr(sqdbr), nullInstr, nullInstr);
      break;
    case kS390_FloorFloat:
      __ fiebra(v8::internal::Assembler::FIDBRA_ROUND_TOWARD_NEG_INF,
                i.OutputDoubleRegister(), i.InputDoubleRegister(0));
      break;
    case kS390_CeilFloat:
      __ fiebra(v8::internal::Assembler::FIDBRA_ROUND_TOWARD_POS_INF,
                i.OutputDoubleRegister(), i.InputDoubleRegister(0));
      break;
    case kS390_TruncateFloat:
      __ fiebra(v8::internal::Assembler::FIDBRA_ROUND_TOWARD_0,
                i.OutputDoubleRegister(), i.InputDoubleRegister(0));
      break;
    //  Double operations
    case kS390_ModDouble:
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
    case kIeee754Float64Atanh:
      ASSEMBLE_IEEE754_UNOP(atanh);
      break;
    case kIeee754Float64Atan:
      ASSEMBLE_IEEE754_UNOP(atan);
      break;
    case kIeee754Float64Atan2:
      ASSEMBLE_IEEE754_BINOP(atan2);
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
    case kS390_Neg32:
      __ lcr(i.OutputRegister(), i.InputRegister(0));
      CHECK_AND_ZERO_EXT_OUTPUT(1);
      break;
    case kS390_Neg64:
      __ lcgr(i.OutputRegister(), i.InputRegister(0));
      break;
    case kS390_MaxFloat:
      ASSEMBLE_FLOAT_MAX();
      break;
    case kS390_MaxDouble:
      ASSEMBLE_DOUBLE_MAX();
      break;
    case kS390_MinFloat:
      ASSEMBLE_FLOAT_MIN();
      break;
    case kS390_MinDouble:
      ASSEMBLE_DOUBLE_MIN();
      break;
    case kS390_AbsDouble:
      __ lpdbr(i.OutputDoubleRegister(), i.InputDoubleRegister(0));
      break;
    case kS390_FloorDouble:
      __ fidbra(v8::internal::Assembler::FIDBRA_ROUND_TOWARD_NEG_INF,
                i.OutputDoubleRegister(), i.InputDoubleRegister(0));
      break;
    case kS390_CeilDouble:
      __ fidbra(v8::internal::Assembler::FIDBRA_ROUND_TOWARD_POS_INF,
                i.OutputDoubleRegister(), i.InputDoubleRegister(0));
      break;
    case kS390_TruncateDouble:
      __ fidbra(v8::internal::Assembler::FIDBRA_ROUND_TOWARD_0,
                i.OutputDoubleRegister(), i.InputDoubleRegister(0));
      break;
    case kS390_RoundDouble:
      __ fidbra(v8::internal::Assembler::FIDBRA_ROUND_TO_NEAREST_AWAY_FROM_0,
                i.OutputDoubleRegister(), i.InputDoubleRegister(0));
      break;
    case kS390_NegFloat:
      ASSEMBLE_UNARY_OP(D_DInstr(lcebr), nullInstr, nullInstr);
      break;
    case kS390_NegDouble:
      ASSEMBLE_UNARY_OP(D_DInstr(lcdbr), nullInstr, nullInstr);
      break;
    case kS390_Cntlz32: {
      __ llgfr(i.OutputRegister(), i.InputRegister(0));
      __ flogr(r0, i.OutputRegister());
      __ Add32(i.OutputRegister(), r0, Operand(-32));
      // No need to zero-ext b/c llgfr is done already
      break;
    }
#if V8_TARGET_ARCH_S390X
    case kS390_Cntlz64: {
      __ flogr(r0, i.InputRegister(0));
      __ LoadRR(i.OutputRegister(), r0);
      break;
    }
#endif
    case kS390_Popcnt32:
      __ Popcnt32(i.OutputRegister(), i.InputRegister(0));
      break;
#if V8_TARGET_ARCH_S390X
    case kS390_Popcnt64:
      __ Popcnt64(i.OutputRegister(), i.InputRegister(0));
      break;
#endif
    case kS390_Cmp32:
      ASSEMBLE_COMPARE32(Cmp32, CmpLogical32);
      break;
#if V8_TARGET_ARCH_S390X
    case kS390_Cmp64:
      ASSEMBLE_COMPARE(CmpP, CmpLogicalP);
      break;
#endif
    case kS390_CmpFloat:
      ASSEMBLE_FLOAT_COMPARE(cebr, ceb, ley);
      // __ cebr(i.InputDoubleRegister(0), i.InputDoubleRegister(1));
      break;
    case kS390_CmpDouble:
      ASSEMBLE_FLOAT_COMPARE(cdbr, cdb, ldy);
      // __ cdbr(i.InputDoubleRegister(0), i.InputDoubleRegister(1));
      break;
    case kS390_Tst32:
      if (HasRegisterInput(instr, 1)) {
        __ And(r0, i.InputRegister(0), i.InputRegister(1));
      } else {
        // detect tmlh/tmhl/tmhh case
        Operand opnd = i.InputImmediate(1);
        if (is_uint16(opnd.immediate())) {
          __ tmll(i.InputRegister(0), opnd);
        } else {
          __ lr(r0, i.InputRegister(0));
          __ nilf(r0, opnd);
        }
      }
      break;
    case kS390_Tst64:
      if (HasRegisterInput(instr, 1)) {
        __ AndP(r0, i.InputRegister(0), i.InputRegister(1));
      } else {
        Operand opnd = i.InputImmediate(1);
        if (is_uint16(opnd.immediate())) {
          __ tmll(i.InputRegister(0), opnd);
        } else {
          __ AndP(r0, i.InputRegister(0), opnd);
        }
      }
      break;
    case kS390_Float64SilenceNaN: {
      DoubleRegister value = i.InputDoubleRegister(0);
      DoubleRegister result = i.OutputDoubleRegister();
      __ CanonicalizeNaN(result, value);
      break;
    }
    case kS390_StackClaim: {
      int num_slots = i.InputInt32(0);
      __ lay(sp, MemOperand(sp, -num_slots * kSystemPointerSize));
      frame_access_state()->IncreaseSPDelta(num_slots);
      break;
    }
    case kS390_Push:
      if (instr->InputAt(0)->IsFPRegister()) {
        LocationOperand* op = LocationOperand::cast(instr->InputAt(0));
        if (op->representation() == MachineRepresentation::kFloat64) {
          __ lay(sp, MemOperand(sp, -kDoubleSize));
          __ StoreDouble(i.InputDoubleRegister(0), MemOperand(sp));
          frame_access_state()->IncreaseSPDelta(kDoubleSize /
                                                kSystemPointerSize);
        } else {
          DCHECK_EQ(MachineRepresentation::kFloat32, op->representation());
          __ lay(sp, MemOperand(sp, -kSystemPointerSize));
          __ StoreFloat32(i.InputDoubleRegister(0), MemOperand(sp));
          frame_access_state()->IncreaseSPDelta(1);
        }
      } else {
        __ Push(i.InputRegister(0));
        frame_access_state()->IncreaseSPDelta(1);
      }
      break;
    case kS390_PushFrame: {
      int num_slots = i.InputInt32(1);
      __ lay(sp, MemOperand(sp, -num_slots * kSystemPointerSize));
      if (instr->InputAt(0)->IsFPRegister()) {
        LocationOperand* op = LocationOperand::cast(instr->InputAt(0));
        if (op->representation() == MachineRepresentation::kFloat64) {
          __ StoreDouble(i.InputDoubleRegister(0), MemOperand(sp));
        } else {
          DCHECK_EQ(MachineRepresentation::kFloat32, op->representation());
          __ StoreFloat32(i.InputDoubleRegister(0), MemOperand(sp));
        }
      } else {
        __ StoreP(i.InputRegister(0), MemOperand(sp));
      }
      break;
    }
    case kS390_StoreToStackSlot: {
      int slot = i.InputInt32(1);
      if (instr->InputAt(0)->IsFPRegister()) {
        LocationOperand* op = LocationOperand::cast(instr->InputAt(0));
        if (op->representation() == MachineRepresentation::kFloat64) {
          __ StoreDouble(i.InputDoubleRegister(0),
                         MemOperand(sp, slot * kSystemPointerSize));
        } else {
          DCHECK_EQ(MachineRepresentation::kFloat32, op->representation());
          __ StoreFloat32(i.InputDoubleRegister(0),
                          MemOperand(sp, slot * kSystemPointerSize));
        }
      } else {
        __ StoreP(i.InputRegister(0),
                  MemOperand(sp, slot * kSystemPointerSize));
      }
      break;
    }
    case kS390_SignExtendWord8ToInt32:
      __ lbr(i.OutputRegister(), i.InputRegister(0));
      CHECK_AND_ZERO_EXT_OUTPUT(1);
      break;
    case kS390_SignExtendWord16ToInt32:
      __ lhr(i.OutputRegister(), i.InputRegister(0));
      CHECK_AND_ZERO_EXT_OUTPUT(1);
      break;
    case kS390_SignExtendWord8ToInt64:
      __ lgbr(i.OutputRegister(), i.InputRegister(0));
      break;
    case kS390_SignExtendWord16ToInt64:
      __ lghr(i.OutputRegister(), i.InputRegister(0));
      break;
    case kS390_SignExtendWord32ToInt64:
      __ lgfr(i.OutputRegister(), i.InputRegister(0));
      break;
    case kS390_Uint32ToUint64:
      // Zero extend
      __ llgfr(i.OutputRegister(), i.InputRegister(0));
      break;
    case kS390_Int64ToInt32:
      // sign extend
      __ lgfr(i.OutputRegister(), i.InputRegister(0));
      break;
    // Convert Fixed to Floating Point
    case kS390_Int64ToFloat32:
      __ ConvertInt64ToFloat(i.OutputDoubleRegister(), i.InputRegister(0));
      break;
    case kS390_Int64ToDouble:
      __ ConvertInt64ToDouble(i.OutputDoubleRegister(), i.InputRegister(0));
      break;
    case kS390_Uint64ToFloat32:
      __ ConvertUnsignedInt64ToFloat(i.OutputDoubleRegister(),
                                     i.InputRegister(0));
      break;
    case kS390_Uint64ToDouble:
      __ ConvertUnsignedInt64ToDouble(i.OutputDoubleRegister(),
                                      i.InputRegister(0));
      break;
    case kS390_Int32ToFloat32:
      __ ConvertIntToFloat(i.OutputDoubleRegister(), i.InputRegister(0));
      break;
    case kS390_Int32ToDouble:
      __ ConvertIntToDouble(i.OutputDoubleRegister(), i.InputRegister(0));
      break;
    case kS390_Uint32ToFloat32:
      __ ConvertUnsignedIntToFloat(i.OutputDoubleRegister(),
                                   i.InputRegister(0));
      break;
    case kS390_Uint32ToDouble:
      __ ConvertUnsignedIntToDouble(i.OutputDoubleRegister(),
                                    i.InputRegister(0));
      break;
    case kS390_DoubleToInt32: {
      Label done;
      __ ConvertDoubleToInt32(i.OutputRegister(0), i.InputDoubleRegister(0),
                              kRoundToNearest);
      __ b(Condition(0xE), &done, Label::kNear);  // normal case
      __ lghi(i.OutputRegister(0), Operand::Zero());
      __ bind(&done);
      break;
    }
    case kS390_DoubleToUint32: {
      Label done;
      __ ConvertDoubleToUnsignedInt32(i.OutputRegister(0),
                                      i.InputDoubleRegister(0));
      __ b(Condition(0xE), &done, Label::kNear);  // normal case
      __ lghi(i.OutputRegister(0), Operand::Zero());
      __ bind(&done);
      break;
    }
    case kS390_DoubleToInt64: {
      Label done;
      if (i.OutputCount() > 1) {
        __ lghi(i.OutputRegister(1), Operand(1));
      }
      __ ConvertDoubleToInt64(i.OutputRegister(0), i.InputDoubleRegister(0));
      __ b(Condition(0xE), &done, Label::kNear);  // normal case
      if (i.OutputCount() > 1) {
        __ lghi(i.OutputRegister(1), Operand::Zero());
      } else {
        __ lghi(i.OutputRegister(0), Operand::Zero());
      }
      __ bind(&done);
      break;
    }
    case kS390_DoubleToUint64: {
      Label done;
      if (i.OutputCount() > 1) {
        __ lghi(i.OutputRegister(1), Operand(1));
      }
      __ ConvertDoubleToUnsignedInt64(i.OutputRegister(0),
                                      i.InputDoubleRegister(0));
      __ b(Condition(0xE), &done, Label::kNear);  // normal case
      if (i.OutputCount() > 1) {
        __ lghi(i.OutputRegister(1), Operand::Zero());
      } else {
        __ lghi(i.OutputRegister(0), Operand::Zero());
      }
      __ bind(&done);
      break;
    }
    case kS390_Float32ToInt32: {
      Label done;
      __ ConvertFloat32ToInt32(i.OutputRegister(0), i.InputDoubleRegister(0),
                               kRoundToZero);
      __ b(Condition(0xE), &done, Label::kNear);  // normal case
      __ lghi(i.OutputRegister(0), Operand::Zero());
      __ bind(&done);
      break;
    }
    case kS390_Float32ToUint32: {
      Label done;
      __ ConvertFloat32ToUnsignedInt32(i.OutputRegister(0),
                                       i.InputDoubleRegister(0));
      __ b(Condition(0xE), &done, Label::kNear);  // normal case
      __ lghi(i.OutputRegister(0), Operand::Zero());
      __ bind(&done);
      break;
    }
    case kS390_Float32ToUint64: {
      Label done;
      if (i.OutputCount() > 1) {
        __ lghi(i.OutputRegister(1), Operand(1));
      }
      __ ConvertFloat32ToUnsignedInt64(i.OutputRegister(0),
                                       i.InputDoubleRegister(0));
      __ b(Condition(0xE), &done, Label::kNear);  // normal case
      if (i.OutputCount() > 1) {
        __ lghi(i.OutputRegister(1), Operand::Zero());
      } else {
        __ lghi(i.OutputRegister(0), Operand::Zero());
      }
      __ bind(&done);
      break;
    }
    case kS390_Float32ToInt64: {
      Label done;
      if (i.OutputCount() > 1) {
        __ lghi(i.OutputRegister(1), Operand(1));
      }
      __ ConvertFloat32ToInt64(i.OutputRegister(0), i.InputDoubleRegister(0));
      __ b(Condition(0xE), &done, Label::kNear);  // normal case
      if (i.OutputCount() > 1) {
        __ lghi(i.OutputRegister(1), Operand::Zero());
      } else {
        __ lghi(i.OutputRegister(0), Operand::Zero());
      }
      __ bind(&done);
      break;
    }
    case kS390_DoubleToFloat32:
      ASSEMBLE_UNARY_OP(D_DInstr(ledbr), nullInstr, nullInstr);
      break;
    case kS390_Float32ToDouble:
      ASSEMBLE_UNARY_OP(D_DInstr(ldebr), D_MTInstr(LoadFloat32ToDouble),
                        nullInstr);
      break;
    case kS390_DoubleExtractLowWord32:
      __ lgdr(i.OutputRegister(), i.InputDoubleRegister(0));
      __ llgfr(i.OutputRegister(), i.OutputRegister());
      break;
    case kS390_DoubleExtractHighWord32:
      __ lgdr(i.OutputRegister(), i.InputDoubleRegister(0));
      __ srlg(i.OutputRegister(), i.OutputRegister(), Operand(32));
      break;
    case kS390_DoubleInsertLowWord32:
      __ lgdr(kScratchReg, i.InputDoubleRegister(0));
      __ lr(kScratchReg, i.InputRegister(1));
      __ ldgr(i.OutputDoubleRegister(), kScratchReg);
      break;
    case kS390_DoubleInsertHighWord32:
      __ sllg(kScratchReg, i.InputRegister(1), Operand(32));
      __ lgdr(r0, i.InputDoubleRegister(0));
      __ lr(kScratchReg, r0);
      __ ldgr(i.OutputDoubleRegister(), kScratchReg);
      break;
    case kS390_DoubleConstruct:
      __ sllg(kScratchReg, i.InputRegister(0), Operand(32));
      __ lr(kScratchReg, i.InputRegister(1));

      // Bitwise convert from GPR to FPR
      __ ldgr(i.OutputDoubleRegister(), kScratchReg);
      break;
    case kS390_LoadWordS8:
      ASSEMBLE_LOAD_INTEGER(LoadB);
      EmitWordLoadPoisoningIfNeeded(this, instr, i);
      break;
    case kS390_BitcastFloat32ToInt32:
      ASSEMBLE_UNARY_OP(R_DInstr(MovFloatToInt), R_MInstr(LoadlW), nullInstr);
      break;
    case kS390_BitcastInt32ToFloat32:
      __ MovIntToFloat(i.OutputDoubleRegister(), i.InputRegister(0));
      break;
#if V8_TARGET_ARCH_S390X
    case kS390_BitcastDoubleToInt64:
      __ MovDoubleToInt64(i.OutputRegister(), i.InputDoubleRegister(0));
      break;
    case kS390_BitcastInt64ToDouble:
      __ MovInt64ToDouble(i.OutputDoubleRegister(), i.InputRegister(0));
      break;
#endif
    case kS390_LoadWordU8:
      ASSEMBLE_LOAD_INTEGER(LoadlB);
      EmitWordLoadPoisoningIfNeeded(this, instr, i);
      break;
    case kS390_LoadWordU16:
      ASSEMBLE_LOAD_INTEGER(LoadLogicalHalfWordP);
      EmitWordLoadPoisoningIfNeeded(this, instr, i);
      break;
    case kS390_LoadWordS16:
      ASSEMBLE_LOAD_INTEGER(LoadHalfWordP);
      EmitWordLoadPoisoningIfNeeded(this, instr, i);
      break;
    case kS390_LoadWordU32:
      ASSEMBLE_LOAD_INTEGER(LoadlW);
      EmitWordLoadPoisoningIfNeeded(this, instr, i);
      break;
    case kS390_LoadWordS32:
      ASSEMBLE_LOAD_INTEGER(LoadW);
      EmitWordLoadPoisoningIfNeeded(this, instr, i);
      break;
    case kS390_LoadReverse16:
      ASSEMBLE_LOAD_INTEGER(lrvh);
      EmitWordLoadPoisoningIfNeeded(this, instr, i);
      break;
    case kS390_LoadReverse32:
      ASSEMBLE_LOAD_INTEGER(lrv);
      EmitWordLoadPoisoningIfNeeded(this, instr, i);
      break;
    case kS390_LoadReverse64:
      ASSEMBLE_LOAD_INTEGER(lrvg);
      EmitWordLoadPoisoningIfNeeded(this, instr, i);
      break;
    case kS390_LoadReverse16RR:
      __ lrvr(i.OutputRegister(), i.InputRegister(0));
      __ rll(i.OutputRegister(), i.OutputRegister(), Operand(16));
      break;
    case kS390_LoadReverse32RR:
      __ lrvr(i.OutputRegister(), i.InputRegister(0));
      break;
    case kS390_LoadReverse64RR:
      __ lrvgr(i.OutputRegister(), i.InputRegister(0));
      break;
    case kS390_LoadWord64:
      ASSEMBLE_LOAD_INTEGER(lg);
      EmitWordLoadPoisoningIfNeeded(this, instr, i);
      break;
    case kS390_LoadAndTestWord32: {
      ASSEMBLE_LOADANDTEST32(ltr, lt_z);
      break;
    }
    case kS390_LoadAndTestWord64: {
      ASSEMBLE_LOADANDTEST64(ltgr, ltg);
      break;
    }
    case kS390_LoadFloat32:
      ASSEMBLE_LOAD_FLOAT(LoadFloat32);
      break;
    case kS390_LoadDouble:
      ASSEMBLE_LOAD_FLOAT(LoadDouble);
      break;
    case kS390_StoreWord8:
      ASSEMBLE_STORE_INTEGER(StoreByte);
      break;
    case kS390_StoreWord16:
      ASSEMBLE_STORE_INTEGER(StoreHalfWord);
      break;
    case kS390_StoreWord32:
      ASSEMBLE_STORE_INTEGER(StoreW);
      break;
#if V8_TARGET_ARCH_S390X
    case kS390_StoreWord64:
      ASSEMBLE_STORE_INTEGER(StoreP);
      break;
#endif
    case kS390_StoreReverse16:
      ASSEMBLE_STORE_INTEGER(strvh);
      break;
    case kS390_StoreReverse32:
      ASSEMBLE_STORE_INTEGER(strv);
      break;
    case kS390_StoreReverse64:
      ASSEMBLE_STORE_INTEGER(strvg);
      break;
    case kS390_StoreFloat32:
      ASSEMBLE_STORE_FLOAT32();
      break;
    case kS390_StoreDouble:
      ASSEMBLE_STORE_DOUBLE();
      break;
    case kS390_Lay:
      __ lay(i.OutputRegister(), i.MemoryOperand());
      break;
//         0x aa bb cc dd
// index =    3..2..1..0
#define ATOMIC_EXCHANGE(start, end, shift_amount, offset)              \
  {                                                                    \
    Label do_cs;                                                       \
    __ LoadlW(output, MemOperand(r1, offset));                         \
    __ bind(&do_cs);                                                   \
    __ llgfr(r0, output);                                              \
    __ RotateInsertSelectBits(r0, value, Operand(start), Operand(end), \
                              Operand(shift_amount), false);           \
    __ csy(output, r0, MemOperand(r1, offset));                        \
    __ bne(&do_cs, Label::kNear);                                      \
    __ srl(output, Operand(shift_amount));                             \
  }
#ifdef V8_TARGET_BIG_ENDIAN
#define ATOMIC_EXCHANGE_BYTE(i)                                  \
  {                                                              \
    constexpr int idx = (i);                                     \
    static_assert(idx <= 3 && idx >= 0, "idx is out of range!"); \
    constexpr int start = 32 + 8 * idx;                          \
    constexpr int end = start + 7;                               \
    constexpr int shift_amount = (3 - idx) * 8;                  \
    ATOMIC_EXCHANGE(start, end, shift_amount, -idx);             \
  }
#define ATOMIC_EXCHANGE_HALFWORD(i)                              \
  {                                                              \
    constexpr int idx = (i);                                     \
    static_assert(idx <= 1 && idx >= 0, "idx is out of range!"); \
    constexpr int start = 32 + 16 * idx;                         \
    constexpr int end = start + 15;                              \
    constexpr int shift_amount = (1 - idx) * 16;                 \
    ATOMIC_EXCHANGE(start, end, shift_amount, -idx * 2);         \
  }
#else
#define ATOMIC_EXCHANGE_BYTE(i)                                  \
  {                                                              \
    constexpr int idx = (i);                                     \
    static_assert(idx <= 3 && idx >= 0, "idx is out of range!"); \
    constexpr int start = 32 + 8 * (3 - idx);                    \
    constexpr int end = start + 7;                               \
    constexpr int shift_amount = idx * 8;                        \
    ATOMIC_EXCHANGE(start, end, shift_amount, -idx);             \
  }
#define ATOMIC_EXCHANGE_HALFWORD(i)                              \
  {                                                              \
    constexpr int idx = (i);                                     \
    static_assert(idx <= 1 && idx >= 0, "idx is out of range!"); \
    constexpr int start = 32 + 16 * (1 - idx);                   \
    constexpr int end = start + 15;                              \
    constexpr int shift_amount = idx * 16;                       \
    ATOMIC_EXCHANGE(start, end, shift_amount, -idx * 2);         \
  }
#endif
    case kS390_Word64AtomicExchangeUint8:
    case kWord32AtomicExchangeInt8:
    case kWord32AtomicExchangeUint8: {
      Register base = i.InputRegister(0);
      Register index = i.InputRegister(1);
      Register value = i.InputRegister(2);
      Register output = i.OutputRegister();
      Label three, two, one, done;
      __ la(r1, MemOperand(base, index));
      __ tmll(r1, Operand(3));
      __ b(Condition(1), &three);
      __ b(Condition(2), &two);
      __ b(Condition(4), &one);

      // end with 0b00
      ATOMIC_EXCHANGE_BYTE(0);
      __ b(&done);

      // ending with 0b01
      __ bind(&one);
      ATOMIC_EXCHANGE_BYTE(1);
      __ b(&done);

      // ending with 0b10
      __ bind(&two);
      ATOMIC_EXCHANGE_BYTE(2);
      __ b(&done);

      // ending with 0b11
      __ bind(&three);
      ATOMIC_EXCHANGE_BYTE(3);

      __ bind(&done);
      if (opcode == kWord32AtomicExchangeInt8) {
        __ lgbr(output, output);
      } else {
        __ llgcr(output, output);
      }
      break;
    }
    case kS390_Word64AtomicExchangeUint16:
    case kWord32AtomicExchangeInt16:
    case kWord32AtomicExchangeUint16: {
      Register base = i.InputRegister(0);
      Register index = i.InputRegister(1);
      Register value = i.InputRegister(2);
      Register output = i.OutputRegister();
      Label two, done;
      __ la(r1, MemOperand(base, index));
      __ tmll(r1, Operand(3));
      __ b(Condition(2), &two);

      // end with 0b00
      ATOMIC_EXCHANGE_HALFWORD(0);
      __ b(&done);

      // ending with 0b10
      __ bind(&two);
      ATOMIC_EXCHANGE_HALFWORD(1);

      __ bind(&done);
      if (opcode == kWord32AtomicExchangeInt16) {
        __ lghr(output, output);
      } else {
        __ llghr(output, output);
      }
      break;
    }
    case kS390_Word64AtomicExchangeUint32:
    case kWord32AtomicExchangeWord32: {
      Register base = i.InputRegister(0);
      Register index = i.InputRegister(1);
      Register value = i.InputRegister(2);
      Register output = i.OutputRegister();
      Label do_cs;
      __ lay(r1, MemOperand(base, index));
      __ LoadlW(output, MemOperand(r1));
      __ bind(&do_cs);
      __ cs(output, value, MemOperand(r1));
      __ bne(&do_cs, Label::kNear);
      break;
    }
    case kWord32AtomicCompareExchangeInt8:
      ASSEMBLE_ATOMIC_COMPARE_EXCHANGE_BYTE(LoadB);
      break;
    case kS390_Word64AtomicCompareExchangeUint8:
    case kWord32AtomicCompareExchangeUint8:
      ASSEMBLE_ATOMIC_COMPARE_EXCHANGE_BYTE(LoadlB);
      break;
    case kWord32AtomicCompareExchangeInt16:
      ASSEMBLE_ATOMIC_COMPARE_EXCHANGE_HALFWORD(LoadHalfWordP);
      break;
    case kS390_Word64AtomicCompareExchangeUint16:
    case kWord32AtomicCompareExchangeUint16:
      ASSEMBLE_ATOMIC_COMPARE_EXCHANGE_HALFWORD(LoadLogicalHalfWordP);
      break;
    case kS390_Word64AtomicCompareExchangeUint32:
    case kWord32AtomicCompareExchangeWord32:
      ASSEMBLE_ATOMIC_COMPARE_EXCHANGE_WORD();
      break;
#define ATOMIC_BINOP_CASE(op, inst)                                          \
  case kWord32Atomic##op##Int8:                                              \
    ASSEMBLE_ATOMIC_BINOP_BYTE(inst, [&]() {                                 \
      intptr_t shift_right = static_cast<intptr_t>(shift_amount);            \
      __ srlk(result, prev, Operand(shift_right));                           \
      __ LoadB(result, result);                                              \
    });                                                                      \
    break;                                                                   \
  case kS390_Word64Atomic##op##Uint8:                                        \
  case kWord32Atomic##op##Uint8:                                             \
    ASSEMBLE_ATOMIC_BINOP_BYTE(inst, [&]() {                                 \
      int rotate_left = shift_amount == 0 ? 0 : 64 - shift_amount;           \
      __ RotateInsertSelectBits(result, prev, Operand(56), Operand(63),      \
                                Operand(static_cast<intptr_t>(rotate_left)), \
                                true);                                       \
    });                                                                      \
    break;                                                                   \
  case kWord32Atomic##op##Int16:                                             \
    ASSEMBLE_ATOMIC_BINOP_HALFWORD(inst, [&]() {                             \
      intptr_t shift_right = static_cast<intptr_t>(shift_amount);            \
      __ srlk(result, prev, Operand(shift_right));                           \
      __ LoadHalfWordP(result, result);                                      \
    });                                                                      \
    break;                                                                   \
  case kS390_Word64Atomic##op##Uint16:                                       \
  case kWord32Atomic##op##Uint16:                                            \
    ASSEMBLE_ATOMIC_BINOP_HALFWORD(inst, [&]() {                             \
      int rotate_left = shift_amount == 0 ? 0 : 64 - shift_amount;           \
      __ RotateInsertSelectBits(result, prev, Operand(48), Operand(63),      \
                                Operand(static_cast<intptr_t>(rotate_left)), \
                                true);                                       \
    });                                                                      \
    break;
      ATOMIC_BINOP_CASE(Add, Add32)
      ATOMIC_BINOP_CASE(Sub, Sub32)
      ATOMIC_BINOP_CASE(And, And)
      ATOMIC_BINOP_CASE(Or, Or)
      ATOMIC_BINOP_CASE(Xor, Xor)
#undef ATOMIC_BINOP_CASE
    case kS390_Word64AtomicAddUint32:
    case kWord32AtomicAddWord32:
      ASSEMBLE_ATOMIC_BINOP_WORD(laa);
      break;
    case kS390_Word64AtomicSubUint32:
    case kWord32AtomicSubWord32:
      ASSEMBLE_ATOMIC_BINOP_WORD(LoadAndSub32);
      break;
    case kS390_Word64AtomicAndUint32:
    case kWord32AtomicAndWord32:
      ASSEMBLE_ATOMIC_BINOP_WORD(lan);
      break;
    case kS390_Word64AtomicOrUint32:
    case kWord32AtomicOrWord32:
      ASSEMBLE_ATOMIC_BINOP_WORD(lao);
      break;
    case kS390_Word64AtomicXorUint32:
    case kWord32AtomicXorWord32:
      ASSEMBLE_ATOMIC_BINOP_WORD(lax);
      break;
    case kS390_Word64AtomicAddUint64:
      ASSEMBLE_ATOMIC_BINOP_WORD64(laag);
      break;
    case kS390_Word64AtomicSubUint64:
      ASSEMBLE_ATOMIC_BINOP_WORD64(LoadAndSub64);
      break;
    case kS390_Word64AtomicAndUint64:
      ASSEMBLE_ATOMIC_BINOP_WORD64(lang);
      break;
    case kS390_Word64AtomicOrUint64:
      ASSEMBLE_ATOMIC_BINOP_WORD64(laog);
      break;
    case kS390_Word64AtomicXorUint64:
      ASSEMBLE_ATOMIC_BINOP_WORD64(laxg);
      break;
    case kS390_Word64AtomicExchangeUint64: {
      Register base = i.InputRegister(0);
      Register index = i.InputRegister(1);
      Register value = i.InputRegister(2);
      Register output = i.OutputRegister();
      Label do_cs;
      __ la(r1, MemOperand(base, index));
      __ lg(output, MemOperand(r1));
      __ bind(&do_cs);
      __ csg(output, value, MemOperand(r1));
      __ bne(&do_cs, Label::kNear);
      break;
    }
    case kS390_Word64AtomicCompareExchangeUint64:
      ASSEMBLE_ATOMIC64_COMP_EXCHANGE_WORD64();
      break;
    default:
      UNREACHABLE();
      break;
  }
  return kSuccess;
}  // NOLINT(readability/fn_size)

// Assembles branches after an instruction.
void CodeGenerator::AssembleArchBranch(Instruction* instr, BranchInfo* branch) {
  S390OperandConverter i(this, instr);
  Label* tlabel = branch->true_label;
  Label* flabel = branch->false_label;
  ArchOpcode op = instr->arch_opcode();
  FlagsCondition condition = branch->condition;

  Condition cond = FlagsConditionToCondition(condition, op);
  if (op == kS390_CmpFloat || op == kS390_CmpDouble) {
    // check for unordered if necessary
    // Branching to flabel/tlabel according to what's expected by tests
    if (cond == le || cond == eq || cond == lt) {
      __ bunordered(flabel);
    } else if (cond == gt || cond == ne || cond == ge) {
      __ bunordered(tlabel);
    }
  }
  __ b(cond, tlabel);
  if (!branch->fallthru) __ b(flabel);  // no fallthru to flabel.
}

void CodeGenerator::AssembleBranchPoisoning(FlagsCondition condition,
                                            Instruction* instr) {
  // TODO(John) Handle float comparisons (kUnordered[Not]Equal).
  if (condition == kUnorderedEqual || condition == kUnorderedNotEqual ||
      condition == kOverflow || condition == kNotOverflow) {
    return;
  }

  condition = NegateFlagsCondition(condition);
  __ LoadImmP(r0, Operand::Zero());
  __ LoadOnConditionP(FlagsConditionToCondition(condition, kArchNop),
                      kSpeculationPoisonRegister, r0);
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
      S390OperandConverter i(gen_, instr_);
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
        gen_->RecordSafepoint(reference_map, Safepoint::kSimple,
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
  Label end;

  ArchOpcode op = instr->arch_opcode();
  Condition cond = FlagsConditionToCondition(condition, op);
  if (op == kS390_CmpFloat || op == kS390_CmpDouble) {
    // check for unordered if necessary
    if (cond == le || cond == eq || cond == lt) {
      __ bunordered(&end);
    } else if (cond == gt || cond == ne || cond == ge) {
      __ bunordered(tlabel);
    }
  }
  __ b(cond, tlabel);
  __ bind(&end);
}

// Assembles boolean materializations after an instruction.
void CodeGenerator::AssembleArchBoolean(Instruction* instr,
                                        FlagsCondition condition) {
  S390OperandConverter i(this, instr);
  ArchOpcode op = instr->arch_opcode();
  bool check_unordered = (op == kS390_CmpDouble || op == kS390_CmpFloat);

  // Overflow checked for add/sub only.
  DCHECK((condition != kOverflow && condition != kNotOverflow) ||
         (op == kS390_Add32 || op == kS390_Add64 || op == kS390_Sub32 ||
          op == kS390_Sub64 || op == kS390_Mul32));

  // Materialize a full 32-bit 1 or 0 value. The result register is always the
  // last output of the instruction.
  DCHECK_NE(0u, instr->OutputCount());
  Register reg = i.OutputRegister(instr->OutputCount() - 1);
  Condition cond = FlagsConditionToCondition(condition, op);
  Label done;
  if (check_unordered) {
    __ LoadImmP(reg, (cond == eq || cond == le || cond == lt) ? Operand::Zero()
                                                              : Operand(1));
    __ bunordered(&done);
  }

  // TODO(john.yan): use load imm high on condition here
  __ LoadImmP(reg, Operand::Zero());
  __ LoadImmP(kScratchReg, Operand(1));
  // locr is sufficient since reg's upper 32 is guarrantee to be 0
  __ locr(cond, reg, kScratchReg);
  __ bind(&done);
}

void CodeGenerator::AssembleArchBinarySearchSwitch(Instruction* instr) {
  S390OperandConverter i(this, instr);
  Register input = i.InputRegister(0);
  std::vector<std::pair<int32_t, Label*>> cases;
  for (size_t index = 2; index < instr->InputCount(); index += 2) {
    cases.push_back({i.InputInt32(index + 0), GetLabel(i.InputRpo(index + 1))});
  }
  AssembleArchBinarySearchSwitchRange(input, i.InputRpo(1), cases.data(),
                                      cases.data() + cases.size());
}

void CodeGenerator::AssembleArchLookupSwitch(Instruction* instr) {
  S390OperandConverter i(this, instr);
  Register input = i.InputRegister(0);
  for (size_t index = 2; index < instr->InputCount(); index += 2) {
    __ Cmp32(input, Operand(i.InputInt32(index + 0)));
    __ beq(GetLabel(i.InputRpo(index + 1)));
  }
  AssembleArchJump(i.InputRpo(1));
}

void CodeGenerator::AssembleArchTableSwitch(Instruction* instr) {
  S390OperandConverter i(this, instr);
  Register input = i.InputRegister(0);
  int32_t const case_count = static_cast<int32_t>(instr->InputCount() - 2);
  Label** cases = zone()->NewArray<Label*>(case_count);
  for (int32_t index = 0; index < case_count; ++index) {
    cases[index] = GetLabel(i.InputRpo(index + 2));
  }
  Label* const table = AddJumpTable(cases, case_count);
  __ CmpLogicalP(input, Operand(case_count));
  __ bge(GetLabel(i.InputRpo(1)));
  __ larl(kScratchReg, table);
  __ ShiftLeftP(r1, input, Operand(kSystemPointerSizeLog2));
  __ LoadP(kScratchReg, MemOperand(kScratchReg, r1));
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
  const RegList saves = call_descriptor->CalleeSavedRegisters();
  if (saves != 0) {
    // register save area does not include the fp or constant pool pointer.
    const int num_saves = kNumCalleeSaved - 1;
    DCHECK(num_saves == base::bits::CountPopulation(saves));
    frame->AllocateSavedCalleeRegisterSlots(num_saves);
  }
}

void CodeGenerator::AssembleConstructFrame() {
  auto call_descriptor = linkage()->GetIncomingDescriptor();

  if (frame_access_state()->has_frame()) {
    if (call_descriptor->IsCFunctionCall()) {
      __ Push(r14, fp);
      __ LoadRR(fp, sp);
    } else if (call_descriptor->IsJSFunctionCall()) {
      __ Prologue(ip);
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
      } else if (call_descriptor->IsWasmImportWrapper()) {
        // WASM import wrappers are passed a tuple in the place of the instance.
        // Unpack the tuple into the instance and the target callable.
        // This must be done here in the codegen because it cannot be expressed
        // properly in the graph.
        __ LoadP(kJSFunctionRegister,
                 FieldMemOperand(kWasmInstanceRegister, Tuple2::kValue2Offset));
        __ LoadP(kWasmInstanceRegister,
                 FieldMemOperand(kWasmInstanceRegister, Tuple2::kValue1Offset));
        __ Push(kWasmInstanceRegister);
      }
    }
  }

  int required_slots = frame()->GetTotalFrameSlotCount() -
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
    required_slots -= osr_helper()->UnoptimizedFrameSlots();
    ResetSpeculationPoison();
  }

  const RegList saves_fp = call_descriptor->CalleeSavedFPRegisters();
  const RegList saves = call_descriptor->CalleeSavedRegisters();

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
        Register scratch = r1;
        __ LoadP(
            scratch,
            FieldMemOperand(kWasmInstanceRegister,
                            WasmInstanceObject::kRealStackLimitAddressOffset));
        __ LoadP(scratch, MemOperand(scratch));
        __ AddP(scratch, scratch, Operand(required_slots * kSystemPointerSize));
        __ CmpLogicalP(sp, scratch);
        __ bge(&done);
      }

      __ Call(wasm::WasmCode::kWasmStackOverflow, RelocInfo::WASM_STUB_CALL);
      // We come from WebAssembly, there are no references for the GC.
      ReferenceMap* reference_map = new (zone()) ReferenceMap(zone());
      RecordSafepoint(reference_map, Safepoint::kSimple,
                      Safepoint::kNoLazyDeopt);
      if (FLAG_debug_code) {
        __ stop(GetAbortReason(AbortReason::kUnexpectedReturnFromThrow));
      }

      __ bind(&done);
    }

    // Skip callee-saved and return slots, which are pushed below.
    required_slots -= base::bits::CountPopulation(saves);
    required_slots -= frame()->GetReturnSlotCount();
    required_slots -= (kDoubleSize / kSystemPointerSize) *
                      base::bits::CountPopulation(saves_fp);
    __ lay(sp, MemOperand(sp, -required_slots * kSystemPointerSize));
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
    __ lay(sp, MemOperand(sp, -returns * kSystemPointerSize));
  }
}

void CodeGenerator::AssembleReturn(InstructionOperand* pop) {
  auto call_descriptor = linkage()->GetIncomingDescriptor();
  int pop_count = static_cast<int>(call_descriptor->StackParameterCount());

  const int returns = frame()->GetReturnSlotCount();
  if (returns != 0) {
    // Create space for returns.
    __ lay(sp, MemOperand(sp, returns * kSystemPointerSize));
  }

  // Restore registers.
  const RegList saves = call_descriptor->CalleeSavedRegisters();
  if (saves != 0) {
    __ MultiPop(saves);
  }

  // Restore double registers.
  const RegList double_saves = call_descriptor->CalleeSavedFPRegisters();
  if (double_saves != 0) {
    __ MultiPopDoubles(double_saves);
  }

  S390OperandConverter g(this, nullptr);
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
  if (pop->IsImmediate()) {
    pop_count += g.ToConstant(pop).ToInt32();
  } else {
    __ Drop(g.ToRegister(pop));
  }
  __ Drop(pop_count);
  __ Ret();
}

void CodeGenerator::FinishCode() {}

void CodeGenerator::AssembleMove(InstructionOperand* source,
                                 InstructionOperand* destination) {
  S390OperandConverter g(this, nullptr);
  // Dispatch on the source and destination operand kinds.  Not all
  // combinations are possible.
  if (source->IsRegister()) {
    DCHECK(destination->IsRegister() || destination->IsStackSlot());
    Register src = g.ToRegister(source);
    if (destination->IsRegister()) {
      __ Move(g.ToRegister(destination), src);
    } else {
      __ StoreP(src, g.ToMemOperand(destination));
    }
  } else if (source->IsStackSlot()) {
    DCHECK(destination->IsRegister() || destination->IsStackSlot());
    MemOperand src = g.ToMemOperand(source);
    if (destination->IsRegister()) {
      __ LoadP(g.ToRegister(destination), src);
    } else {
      Register temp = kScratchReg;
      __ LoadP(temp, src, r0);
      __ StoreP(temp, g.ToMemOperand(destination));
    }
  } else if (source->IsConstant()) {
    Constant src = g.ToConstant(source);
    if (destination->IsRegister() || destination->IsStackSlot()) {
      Register dst =
          destination->IsRegister() ? g.ToRegister(destination) : kScratchReg;
      switch (src.type()) {
        case Constant::kInt32:
#if V8_TARGET_ARCH_S390X
          if (false) {
#else
          if (RelocInfo::IsWasmReference(src.rmode())) {
#endif
            __ mov(dst, Operand(src.ToInt32(), src.rmode()));
          } else {
            __ Load(dst, Operand(src.ToInt32()));
          }
          break;
        case Constant::kInt64:
#if V8_TARGET_ARCH_S390X
          if (RelocInfo::IsWasmReference(src.rmode())) {
            __ mov(dst, Operand(src.ToInt64(), src.rmode()));
          } else {
            __ Load(dst, Operand(src.ToInt64()));
          }
#else
          __ mov(dst, Operand(src.ToInt64()));
#endif  // V8_TARGET_ARCH_S390X
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
        case Constant::kRpoNumber:
          UNREACHABLE();  // TODO(dcarney): loading RPO constants on S390.
          break;
      }
      if (destination->IsStackSlot()) {
        __ StoreP(dst, g.ToMemOperand(destination), r0);
      }
    } else {
      DoubleRegister dst = destination->IsFPRegister()
                               ? g.ToDoubleRegister(destination)
                               : kScratchDoubleReg;
      double value = (src.type() == Constant::kFloat32)
                         ? src.ToFloat32()
                         : src.ToFloat64().value();
      if (src.type() == Constant::kFloat32) {
        __ LoadFloat32Literal(dst, src.ToFloat32(), kScratchReg);
      } else {
        __ LoadDoubleLiteral(dst, value, kScratchReg);
      }

      if (destination->IsFloatStackSlot()) {
        __ StoreFloat32(dst, g.ToMemOperand(destination));
      } else if (destination->IsDoubleStackSlot()) {
        __ StoreDouble(dst, g.ToMemOperand(destination));
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
        __ StoreDouble(src, g.ToMemOperand(destination));
      } else {
        __ StoreFloat32(src, g.ToMemOperand(destination));
      }
    }
  } else if (source->IsFPStackSlot()) {
    DCHECK(destination->IsFPRegister() || destination->IsFPStackSlot());
    MemOperand src = g.ToMemOperand(source);
    if (destination->IsFPRegister()) {
      LocationOperand* op = LocationOperand::cast(source);
      if (op->representation() == MachineRepresentation::kFloat64) {
        __ LoadDouble(g.ToDoubleRegister(destination), src);
      } else {
        __ LoadFloat32(g.ToDoubleRegister(destination), src);
      }
    } else {
      LocationOperand* op = LocationOperand::cast(source);
      DoubleRegister temp = kScratchDoubleReg;
      if (op->representation() == MachineRepresentation::kFloat64) {
        __ LoadDouble(temp, src);
        __ StoreDouble(temp, g.ToMemOperand(destination));
      } else {
        __ LoadFloat32(temp, src);
        __ StoreFloat32(temp, g.ToMemOperand(destination));
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
  S390OperandConverter g(this, nullptr);
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
