// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/base/numbers/double.h"
#include "src/codegen/assembler-inl.h"
#include "src/codegen/callable.h"
#include "src/codegen/macro-assembler.h"
#include "src/codegen/optimized-compilation-info.h"
#include "src/compiler/backend/code-generator-impl.h"
#include "src/compiler/backend/code-generator.h"
#include "src/compiler/backend/gap-resolver.h"
#include "src/compiler/node-matchers.h"
#include "src/compiler/osr.h"
#include "src/heap/memory-chunk.h"

#if V8_ENABLE_WEBASSEMBLY
#include "src/wasm/wasm-code-manager.h"
#include "src/wasm/wasm-objects.h"
#endif  // V8_ENABLE_WEBASSEMBLY

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
      case kFlags_deoptimize:
      case kFlags_set:
      case kFlags_trap:
      case kFlags_select:
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
    AddressingMode addr_mode = AddressingModeField::decode(instr_->opcode());
    if (mode) *mode = addr_mode;
    switch (addr_mode) {
      case kMode_None:
        break;
      case kMode_MRI:
        *first_index += 2;
        return MemOperand(InputRegister(index + 0), InputInt64(index + 1));
      case kMode_MRR:
        *first_index += 2;
        return MemOperand(InputRegister(index + 0), InputRegister(index + 1));
    }
    UNREACHABLE();
  }

  MemOperand MemoryOperand(AddressingMode* mode = NULL,
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
#if V8_ENABLE_WEBASSEMBLY
        stub_mode_(stub_mode),
#endif  // V8_ENABLE_WEBASSEMBLY
        must_save_lr_(!gen->frame_access_state()->has_frame()),
        unwinding_info_writer_(unwinding_info_writer),
        zone_(gen->zone()) {
    DCHECK(!AreAliased(object, offset, scratch0, scratch1));
    DCHECK(!AreAliased(value, offset, scratch0, scratch1));
  }

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
#if V8_ENABLE_WEBASSEMBLY
        stub_mode_(stub_mode),
#endif  // V8_ENABLE_WEBASSEMBLY
        must_save_lr_(!gen->frame_access_state()->has_frame()),
        unwinding_info_writer_(unwinding_info_writer),
        zone_(gen->zone()) {
  }

  void Generate() final {
    ConstantPoolUnavailableScope constant_pool_unavailable(tasm());
    if (COMPRESS_POINTERS_BOOL) {
      __ DecompressTaggedPointer(value_, value_);
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
        mode_ > RecordWriteMode::kValueIsMap ||
                FLAG_use_full_record_write_builtin
            ? RememberedSetAction::kEmit
            : RememberedSetAction::kOmit;
    SaveFPRegsMode const save_fp_mode = frame()->DidAllocateDoubleRegisters()
                                            ? SaveFPRegsMode::kSave
                                            : SaveFPRegsMode::kIgnore;
    if (must_save_lr_) {
      // We need to save and restore lr if the frame was elided.
      __ mflr(scratch0_);
      __ Push(scratch0_);
      unwinding_info_writer_->MarkLinkRegisterOnTopOfStack(__ pc_offset());
    }
    if (mode_ == RecordWriteMode::kValueIsEphemeronKey) {
      __ CallEphemeronKeyBarrier(object_, scratch1_, save_fp_mode);
#if V8_ENABLE_WEBASSEMBLY
    } else if (stub_mode_ == StubCallMode::kCallWasmRuntimeStub) {
      __ CallRecordWriteStubSaveRegisters(object_, scratch1_,
                                          remembered_set_action, save_fp_mode,
                                          StubCallMode::kCallWasmRuntimeStub);
#endif  // V8_ENABLE_WEBASSEMBLY
    } else {
      __ CallRecordWriteStubSaveRegisters(object_, scratch1_,
                                          remembered_set_action, save_fp_mode);
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
#if V8_ENABLE_WEBASSEMBLY
  StubCallMode stub_mode_;
#endif  // V8_ENABLE_WEBASSEMBLY
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
                       i.InputImmediate(1), i.OutputRCBit());  \
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

#define ASSEMBLE_LOAD_FLOAT(asm_instr, asm_instrp, asm_instrx) \
  do {                                                         \
    DoubleRegister result = i.OutputDoubleRegister();          \
    AddressingMode mode = kMode_None;                          \
    MemOperand operand = i.MemoryOperand(&mode);               \
    bool is_atomic = i.InputInt32(2);                          \
    if (mode == kMode_MRI) {                                   \
      if (CpuFeatures::IsSupported(PPC_10_PLUS)) {             \
        __ asm_instrp(result, operand);                        \
      } else {                                                 \
        __ asm_instr(result, operand);                         \
      }                                                        \
    } else {                                                   \
      __ asm_instrx(result, operand);                          \
    }                                                          \
    if (is_atomic) __ lwsync();                                \
    DCHECK_EQ(LeaveRC, i.OutputRCBit());                       \
  } while (0)

#define ASSEMBLE_LOAD_INTEGER(asm_instr, asm_instrp, asm_instrx) \
  do {                                                           \
    Register result = i.OutputRegister();                        \
    AddressingMode mode = kMode_None;                            \
    MemOperand operand = i.MemoryOperand(&mode);                 \
    bool is_atomic = i.InputInt32(2);                            \
    if (mode == kMode_MRI) {                                     \
      if (CpuFeatures::IsSupported(PPC_10_PLUS)) {               \
        __ asm_instrp(result, operand);                          \
      } else {                                                   \
        __ asm_instr(result, operand);                           \
      }                                                          \
    } else {                                                     \
      __ asm_instrx(result, operand);                            \
    }                                                            \
    if (is_atomic) __ lwsync();                                  \
    DCHECK_EQ(LeaveRC, i.OutputRCBit());                         \
  } while (0)

#define ASSEMBLE_LOAD_INTEGER_RR(asm_instr)      \
  do {                                           \
    Register result = i.OutputRegister();        \
    AddressingMode mode = kMode_None;            \
    MemOperand operand = i.MemoryOperand(&mode); \
    DCHECK_EQ(mode, kMode_MRR);                  \
    bool is_atomic = i.InputInt32(2);            \
    __ asm_instr(result, operand);               \
    if (is_atomic) __ lwsync();                  \
    DCHECK_EQ(LeaveRC, i.OutputRCBit());         \
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

#define ASSEMBLE_STORE_INTEGER_RR(asm_instr)             \
  do {                                                   \
    size_t index = 0;                                    \
    AddressingMode mode = kMode_None;                    \
    MemOperand operand = i.MemoryOperand(&mode, &index); \
    DCHECK_EQ(mode, kMode_MRR);                          \
    Register value = i.InputRegister(index);             \
    bool is_atomic = i.InputInt32(3);                    \
    if (is_atomic) __ lwsync();                          \
    __ asm_instr(value, operand);                        \
    if (is_atomic) __ sync();                            \
    DCHECK_EQ(LeaveRC, i.OutputRCBit());                 \
  } while (0)

#if V8_TARGET_ARCH_PPC64
// TODO(mbrandy): fix paths that produce garbage in offset's upper 32-bits.
#define CleanUInt32(x) __ ClearLeftImm(x, x, Operand(32))
#else
#define CleanUInt32(x)
#endif

static inline bool is_wasm_on_be(bool IsWasm) {
#if V8_TARGET_BIG_ENDIAN
  return IsWasm;
#else
  return false;
#endif
}

#define MAYBE_REVERSE_IF_WASM(dst, src, op, reset) \
  if (is_wasm_on_be(info()->IsWasm())) {           \
    __ op(dst, src, kScratchReg);                  \
    if (reset) src = dst;                          \
  }

#define ASSEMBLE_ATOMIC_EXCHANGE(_type, reverse_op)                    \
  do {                                                                 \
    Register val = i.InputRegister(2);                                 \
    Register dst = i.OutputRegister();                                 \
    MAYBE_REVERSE_IF_WASM(ip, val, reverse_op, true);                  \
    __ AtomicExchange<_type>(                                          \
        MemOperand(i.InputRegister(0), i.InputRegister(1)), val, dst); \
    MAYBE_REVERSE_IF_WASM(dst, dst, reverse_op, false);                \
  } while (false)

#define ASSEMBLE_ATOMIC_COMPARE_EXCHANGE(_type, reverse_op)               \
  do {                                                                    \
    Register expected_val = i.InputRegister(2);                           \
    Register new_val = i.InputRegister(3);                                \
    Register dst = i.OutputRegister();                                    \
    MAYBE_REVERSE_IF_WASM(ip, expected_val, reverse_op, true);            \
    MAYBE_REVERSE_IF_WASM(r0, new_val, reverse_op, true);                 \
    __ AtomicCompareExchange<_type>(                                      \
        MemOperand(i.InputRegister(0), i.InputRegister(1)), expected_val, \
        new_val, dst, kScratchReg);                                       \
    MAYBE_REVERSE_IF_WASM(dst, dst, reverse_op, false);                   \
  } while (false)

#define ASSEMBLE_ATOMIC_BINOP_BYTE(bin_inst, _type)                          \
  do {                                                                       \
    auto bin_op = [&](Register dst, Register lhs, Register rhs) {            \
      if (std::is_signed<_type>::value) {                                    \
        __ extsb(dst, lhs);                                                  \
        __ bin_inst(dst, dst, rhs);                                          \
      } else {                                                               \
        __ bin_inst(dst, lhs, rhs);                                          \
      }                                                                      \
    };                                                                       \
    MemOperand dst_operand =                                                 \
        MemOperand(i.InputRegister(0), i.InputRegister(1));                  \
    __ AtomicOps<_type>(dst_operand, i.InputRegister(2), i.OutputRegister(), \
                        kScratchReg, bin_op);                                \
    break;                                                                   \
  } while (false)

#define ASSEMBLE_ATOMIC_BINOP(bin_inst, _type, reverse_op, scratch)          \
  do {                                                                       \
    auto bin_op = [&](Register dst, Register lhs, Register rhs) {            \
      Register _lhs = lhs;                                                   \
      if (is_wasm_on_be(info()->IsWasm())) {                                 \
        __ reverse_op(dst, lhs, scratch);                                    \
        _lhs = dst;                                                          \
      }                                                                      \
      if (std::is_signed<_type>::value) {                                    \
        switch (sizeof(_type)) {                                             \
          case 1:                                                            \
            UNREACHABLE();                                                   \
            break;                                                           \
          case 2:                                                            \
            __ extsh(dst, _lhs);                                             \
            break;                                                           \
          case 4:                                                            \
            __ extsw(dst, _lhs);                                             \
            break;                                                           \
          case 8:                                                            \
            break;                                                           \
          default:                                                           \
            UNREACHABLE();                                                   \
        }                                                                    \
      }                                                                      \
      __ bin_inst(dst, _lhs, rhs);                                           \
      if (is_wasm_on_be(info()->IsWasm())) {                                 \
        __ reverse_op(dst, dst, scratch);                                    \
      }                                                                      \
    };                                                                       \
    MemOperand dst_operand =                                                 \
        MemOperand(i.InputRegister(0), i.InputRegister(1));                  \
    __ AtomicOps<_type>(dst_operand, i.InputRegister(2), i.OutputRegister(), \
                        kScratchReg, bin_op);                                \
    if (is_wasm_on_be(info()->IsWasm())) {                                   \
      __ reverse_op(i.OutputRegister(), i.OutputRegister(), scratch);        \
    }                                                                        \
    break;                                                                   \
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
    tasm->AddS64(sp, sp, Operand(-stack_slot_delta * kSystemPointerSize), r0);
    state->IncreaseSPDelta(stack_slot_delta);
  } else if (allow_shrinkage && stack_slot_delta < 0) {
    if (pending_pushes != nullptr) {
      FlushPendingPushRegisters(tasm, state, pending_pushes);
    }
    tasm->AddS64(sp, sp, Operand(-stack_slot_delta * kSystemPointerSize), r0);
    state->IncreaseSPDelta(stack_slot_delta);
  }
}

}  // namespace

void CodeGenerator::AssembleTailCallBeforeGap(Instruction* instr,
                                              int first_unused_slot_offset) {
  ZoneVector<MoveOperands*> pushes(zone());
  GetPushCompatibleMoves(instr, kRegisterPush, &pushes);

  if (!pushes.empty() &&
      (LocationOperand::cast(pushes.back()->destination()).index() + 1 ==
       first_unused_slot_offset)) {
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
                                first_unused_slot_offset, nullptr, false);
}

void CodeGenerator::AssembleTailCallAfterGap(Instruction* instr,
                                             int first_unused_slot_offset) {
  AdjustStackPointerForTailCall(tasm(), frame_access_state(),
                                first_unused_slot_offset);
}

// Check that {kJavaScriptCallCodeStartRegister} is correct.
void CodeGenerator::AssembleCodeStartRegisterCheck() {
  Register scratch = kScratchReg;
  __ ComputeCodeStartAddress(scratch);
  __ CmpS64(scratch, kJavaScriptCallCodeStartRegister);
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
    __ CmpS64(ip, kJavaScriptCallCodeStartRegister);
    __ Assert(eq, AbortReason::kWrongFunctionCodeStart);
  }

  int offset = Code::kCodeDataContainerOffset - Code::kHeaderSize;
  __ LoadTaggedPointerField(
      r11, MemOperand(kJavaScriptCallCodeStartRegister, offset), r0);
  __ LoadS32(r11,
             FieldMemOperand(r11, CodeDataContainer::kKindSpecificFlagsOffset),
             r0);
  __ TestBit(r11, Code::kMarkedForDeoptimizationBit);
  __ Jump(BUILTIN_CODE(isolate(), CompileLazyDeoptimizedCode),
          RelocInfo::CODE_TARGET, ne, cr0);
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
            instr->HasCallDescriptorFlag(CallDescriptor::kFixedTargetRegister),
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
#if V8_ENABLE_WEBASSEMBLY
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
    case kArchTailCallWasm: {
      // We must not share code targets for calls to builtins for wasm code, as
      // they might need to be patched individually.
      if (instr->InputAt(0)->IsImmediate()) {
        Constant constant = i.ToConstant(instr->InputAt(0));
#ifdef V8_TARGET_ARCH_PPC64
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
#endif  // V8_ENABLE_WEBASSEMBLY
    case kArchTailCallCodeObject: {
      if (HasRegisterInput(instr, 0)) {
        Register reg = i.InputRegister(0);
        DCHECK_IMPLIES(
            instr->HasCallDescriptorFlag(CallDescriptor::kFixedTargetRegister),
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
    case kArchTailCallAddress: {
      CHECK(!instr->InputAt(0)->IsImmediate());
      Register reg = i.InputRegister(0);
      DCHECK_IMPLIES(
          instr->HasCallDescriptorFlag(CallDescriptor::kFixedTargetRegister),
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
        __ LoadTaggedPointerField(
            kScratchReg, FieldMemOperand(func, JSFunction::kContextOffset), r0);
        __ CmpS64(cp, kScratchReg);
        __ Assert(eq, AbortReason::kWrongFunctionContext);
      }
      static_assert(kJavaScriptCallCodeStartRegister == r5, "ABI mismatch");
      __ LoadTaggedPointerField(
          r5, FieldMemOperand(func, JSFunction::kCodeOffset), r0);
      __ CallCodeObject(r5);
      RecordCallPosition(instr);
      DCHECK_EQ(LeaveRC, i.OutputRCBit());
      frame_access_state()->ClearSPDelta();
      break;
    }
    case kArchPrepareCallCFunction: {
      int const num_gp_parameters = ParamField::decode(instr->opcode());
      int const num_fp_parameters = FPParamField::decode(instr->opcode());
      __ PrepareCallCFunction(num_gp_parameters + num_fp_parameters,
                              kScratchReg);
      // Frame alignment requires using FP-relative frame addressing.
      frame_access_state()->SetFrameAccessToFP();
      break;
    }
    case kArchSaveCallerRegisters: {
      fp_mode_ =
          static_cast<SaveFPRegsMode>(MiscField::decode(instr->opcode()));
      DCHECK(fp_mode_ == SaveFPRegsMode::kIgnore ||
             fp_mode_ == SaveFPRegsMode::kSave);
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
      DCHECK(fp_mode_ == SaveFPRegsMode::kIgnore ||
             fp_mode_ == SaveFPRegsMode::kSave);
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
      int const num_gp_parameters = ParamField::decode(instr->opcode());
      int const fp_param_field = FPParamField::decode(instr->opcode());
      int num_fp_parameters = fp_param_field;
      bool has_function_descriptor = false;
#if ABI_USES_FUNCTION_DESCRIPTORS
      // AIX/PPC64BE Linux uses a function descriptor
      int kNumFPParametersMask = kHasFunctionDescriptorBitMask - 1;
      num_fp_parameters = kNumFPParametersMask & fp_param_field;
      has_function_descriptor =
          (fp_param_field & kHasFunctionDescriptorBitMask) != 0;
#endif
#if V8_ENABLE_WEBASSEMBLY
      Label start_call;
      int start_pc_offset = 0;
      bool isWasmCapiFunction =
          linkage()->GetIncomingDescriptor()->IsWasmCapiFunction();
      if (isWasmCapiFunction) {
        __ mflr(r0);
        __ LoadPC(kScratchReg);
        __ bind(&start_call);
        start_pc_offset = __ pc_offset();
        // We are going to patch this instruction after emitting
        // CallCFunction, using a zero offset here as placeholder for now.
        // patch_wasm_cpi_return_address assumes `addi` is used here to
        // add the offset to pc.
        __ addi(kScratchReg, kScratchReg, Operand::Zero());
        __ StoreU64(kScratchReg,
                    MemOperand(fp, WasmExitFrameConstants::kCallingPCOffset));
        __ mtlr(r0);
      }
#endif  // V8_ENABLE_WEBASSEMBLY
      if (instr->InputAt(0)->IsImmediate()) {
        ExternalReference ref = i.InputExternalReference(0);
        __ CallCFunction(ref, num_gp_parameters, num_fp_parameters,
                         has_function_descriptor);
      } else {
        Register func = i.InputRegister(0);
        __ CallCFunction(func, num_gp_parameters, num_fp_parameters,
                         has_function_descriptor);
      }
#if V8_ENABLE_WEBASSEMBLY
      if (isWasmCapiFunction) {
        int offset_since_start_call = __ SizeOfCodeGeneratedSince(&start_call);
        // Here we are going to patch the `addi` instruction above to use the
        // correct offset.
        // LoadPC emits two instructions and pc is the address of its
        // second emitted instruction therefore there is one more instruction to
        // count.
        offset_since_start_call += kInstrSize;
        __ patch_wasm_cpi_return_address(kScratchReg, start_pc_offset,
                                         offset_since_start_call);
        RecordSafepoint(instr->reference_map());
      }
#endif  // V8_ENABLE_WEBASSEMBLY
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
    case kArchAbortCSADcheck:
      DCHECK(i.InputRegister(0) == r4);
      {
        // We don't actually want to generate a pile of code for this, so just
        // claim there is a stack frame, without generating one.
        FrameScope scope(tasm(), StackFrame::NO_FRAME_TYPE);
        __ Call(isolate()->builtins()->code_handle(Builtin::kAbortCSADcheck),
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
          BuildTranslation(instr, -1, 0, 0, OutputFrameStateCombine::Ignore());
      __ b(exit->label());
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
        __ LoadU64(i.OutputRegister(), MemOperand(fp, 0));
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
        __ SubS64(lhs_register, sp, Operand(offset), kScratchReg);
      }

      constexpr size_t kValueIndex = 0;
      DCHECK(instr->InputAt(kValueIndex)->IsRegister());
      __ CmpU64(lhs_register, i.InputRegister(kValueIndex), cr0);
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

      if (FLAG_debug_code) {
        // Checking that |value| is not a cleared weakref: our write barrier
        // does not support that for now.
        __ CmpS64(value, Operand(kClearedWeakHeapObjectLower32), kScratchReg);
        __ Check(ne, AbortReason::kOperandIsCleared);
      }

      AddressingMode addressing_mode =
          AddressingModeField::decode(instr->opcode());
      if (addressing_mode == kMode_MRI) {
        int32_t offset = i.InputInt32(1);
        ool = zone()->New<OutOfLineRecordWrite>(
            this, object, offset, value, scratch0, scratch1, mode,
            DetermineStubCallMode(), &unwinding_info_writer_);
        __ StoreTaggedField(value, MemOperand(object, offset), r0);
      } else {
        DCHECK_EQ(kMode_MRR, addressing_mode);
        Register offset(i.InputRegister(1));
        ool = zone()->New<OutOfLineRecordWrite>(
            this, object, offset, value, scratch0, scratch1, mode,
            DetermineStubCallMode(), &unwinding_info_writer_);
        __ StoreTaggedField(value, MemOperand(object, offset), r0);
      }
      if (mode > RecordWriteMode::kValueIsPointer) {
        __ JumpIfSmi(value, ool->exit());
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
      __ AddS64(i.OutputRegister(), offset.from_stack_pointer() ? sp : fp,
                Operand(offset.offset()), r0);
      break;
    }
    case kPPC_Peek: {
      int reverse_slot = i.InputInt32(0);
      int offset =
          FrameSlotToFPOffset(frame()->GetTotalFrameSlotCount() - reverse_slot);
      if (instr->OutputAt(0)->IsFPRegister()) {
        LocationOperand* op = LocationOperand::cast(instr->OutputAt(0));
        if (op->representation() == MachineRepresentation::kFloat64) {
          __ LoadF64(i.OutputDoubleRegister(), MemOperand(fp, offset), r0);
        } else if (op->representation() == MachineRepresentation::kFloat32) {
          __ LoadF32(i.OutputFloatRegister(), MemOperand(fp, offset), r0);
        } else {
          DCHECK_EQ(MachineRepresentation::kSimd128, op->representation());
          __ mov(ip, Operand(offset));
          __ LoadSimd128(i.OutputSimd128Register(), MemOperand(fp, ip));
        }
      } else {
        __ LoadU64(i.OutputRegister(), MemOperand(fp, offset), r0);
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
      ASSEMBLE_BINOP_RC(ShiftLeftU32, ShiftLeftU32);
      break;
    case kPPC_ShiftLeft64:
      ASSEMBLE_BINOP_RC(ShiftLeftU64, ShiftLeftU64);
      break;
    case kPPC_ShiftRight32:
      ASSEMBLE_BINOP_RC(ShiftRightU32, ShiftRightU32);
      break;
    case kPPC_ShiftRight64:
      ASSEMBLE_BINOP_RC(ShiftRightU64, ShiftRightU64);
      break;
    case kPPC_ShiftRightAlg32:
      ASSEMBLE_BINOP_INT_RC(ShiftRightS32, ShiftRightS32);
      break;
    case kPPC_ShiftRightAlg64:
      ASSEMBLE_BINOP_INT_RC(ShiftRightS64, ShiftRightS64);
      break;
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
          __ AddS64(i.OutputRegister(), i.InputRegister(0), i.InputImmediate(1),
                    r0, LeaveOE, i.OutputRCBit());
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
          __ AddS64(i.OutputRegister(), i.InputRegister(0), i.InputImmediate(1),
                    r0, LeaveOE, i.OutputRCBit());
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
          __ SubS64(i.OutputRegister(), i.InputRegister(0), i.InputImmediate(1),
                    r0, LeaveOE, i.OutputRCBit());
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
      // High 32 bits are undefined and need to be cleared.
      CleanUInt32(i.OutputRegister());
      break;
    case kPPC_MulHighU32:
      __ mulhwu(i.OutputRegister(), i.InputRegister(0), i.InputRegister(1),
                i.OutputRCBit());
      // High 32 bits are undefined and need to be cleared.
      CleanUInt32(i.OutputRegister());
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
      if (CpuFeatures::IsSupported(PPC_9_PLUS)) {
        __ modsw(i.OutputRegister(), i.InputRegister(0), i.InputRegister(1));
      } else {
        ASSEMBLE_MODULO(divw, mullw);
      }
      break;
#if V8_TARGET_ARCH_PPC64
    case kPPC_Mod64:
      if (CpuFeatures::IsSupported(PPC_9_PLUS)) {
        __ modsd(i.OutputRegister(), i.InputRegister(0), i.InputRegister(1));
      } else {
        ASSEMBLE_MODULO(divd, mulld);
      }
      break;
#endif
    case kPPC_ModU32:
      if (CpuFeatures::IsSupported(PPC_9_PLUS)) {
        __ moduw(i.OutputRegister(), i.InputRegister(0), i.InputRegister(1));
      } else {
        ASSEMBLE_MODULO(divwu, mullw);
      }
      break;
#if V8_TARGET_ARCH_PPC64
    case kPPC_ModU64:
      if (CpuFeatures::IsSupported(PPC_9_PLUS)) {
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
      __ MaxF64(i.OutputDoubleRegister(), i.InputDoubleRegister(0),
                i.InputDoubleRegister(1), kScratchDoubleReg);
      break;
    case kPPC_MinDouble:
      __ MinF64(i.OutputDoubleRegister(), i.InputDoubleRegister(0),
                i.InputDoubleRegister(1), kScratchDoubleReg);
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
      __ Popcnt32(i.OutputRegister(), i.InputRegister(0));
      DCHECK_EQ(LeaveRC, i.OutputRCBit());
      break;
#if V8_TARGET_ARCH_PPC64
    case kPPC_Popcnt64:
      __ Popcnt64(i.OutputRegister(), i.InputRegister(0));
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
    case kPPC_Push: {
      int stack_decrement = i.InputInt32(0);
      int slots = stack_decrement / kSystemPointerSize;
      LocationOperand* op = LocationOperand::cast(instr->InputAt(1));
      MachineRepresentation rep = op->representation();
      int pushed_slots = ElementSizeInPointers(rep);
      // Slot-sized arguments are never padded but there may be a gap if
      // the slot allocator reclaimed other padding slots. Adjust the stack
      // here to skip any gap.
      __ AllocateStackSpace((slots - pushed_slots) * kSystemPointerSize);
      switch (rep) {
        case MachineRepresentation::kFloat32:
          __ StoreF32WithUpdate(i.InputDoubleRegister(1),
                                MemOperand(sp, -kSystemPointerSize), r0);
          break;
        case MachineRepresentation::kFloat64:
          __ StoreF64WithUpdate(i.InputDoubleRegister(1),
                                MemOperand(sp, -kDoubleSize), r0);
          break;
        case MachineRepresentation::kSimd128:
          __ addi(sp, sp, Operand(-kSimd128Size));
          __ StoreSimd128(i.InputSimd128Register(1), MemOperand(r0, sp));
          break;
        default:
          __ StoreU64WithUpdate(i.InputRegister(1),
                                MemOperand(sp, -kSystemPointerSize), r0);
          break;
      }
      frame_access_state()->IncreaseSPDelta(slots);
      DCHECK_EQ(LeaveRC, i.OutputRCBit());
      break;
    }
    case kPPC_PushFrame: {
      int num_slots = i.InputInt32(1);
      if (instr->InputAt(0)->IsFPRegister()) {
        LocationOperand* op = LocationOperand::cast(instr->InputAt(0));
        if (op->representation() == MachineRepresentation::kFloat64) {
          __ StoreF64WithUpdate(i.InputDoubleRegister(0),
                                MemOperand(sp, -num_slots * kSystemPointerSize),
                                r0);
        } else {
          DCHECK_EQ(MachineRepresentation::kFloat32, op->representation());
          __ StoreF32WithUpdate(i.InputDoubleRegister(0),
                                MemOperand(sp, -num_slots * kSystemPointerSize),
                                r0);
        }
      } else {
        __ StoreU64WithUpdate(i.InputRegister(0),
                              MemOperand(sp, -num_slots * kSystemPointerSize),
                              r0);
      }
      break;
    }
    case kPPC_StoreToStackSlot: {
      int slot = i.InputInt32(1);
      if (instr->InputAt(0)->IsFPRegister()) {
        LocationOperand* op = LocationOperand::cast(instr->InputAt(0));
        if (op->representation() == MachineRepresentation::kFloat64) {
          __ StoreF64(i.InputDoubleRegister(0),
                      MemOperand(sp, slot * kSystemPointerSize), r0);
        } else if (op->representation() == MachineRepresentation::kFloat32) {
          __ StoreF32(i.InputDoubleRegister(0),
                      MemOperand(sp, slot * kSystemPointerSize), r0);
        } else {
          DCHECK_EQ(MachineRepresentation::kSimd128, op->representation());
          __ mov(ip, Operand(slot * kSystemPointerSize));
          __ StoreSimd128(i.InputSimd128Register(0), MemOperand(ip, sp));
        }
      } else {
        __ StoreU64(i.InputRegister(0),
                    MemOperand(sp, slot * kSystemPointerSize), r0);
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
    case kPPC_Float32ToInt32: {
      bool set_overflow_to_min_i32 = MiscField::decode(instr->opcode());
      if (set_overflow_to_min_i32) {
        __ mtfsb0(VXCVI);  // clear FPSCR:VXCVI bit
      }
      __ fctiwz(kScratchDoubleReg, i.InputDoubleRegister(0));
      __ MovDoubleLowToInt(i.OutputRegister(), kScratchDoubleReg);
      if (set_overflow_to_min_i32) {
        // Avoid INT32_MAX as an overflow indicator and use INT32_MIN instead,
        // because INT32_MIN allows easier out-of-bounds detection.
        CRegister cr = cr7;
        int crbit = v8::internal::Assembler::encode_crbit(
            cr, static_cast<CRBit>(VXCVI % CRWIDTH));
        __ mcrfs(cr, VXCVI);  // extract FPSCR field containing VXCVI into cr7
        __ li(kScratchReg, Operand(1));
        __ ShiftLeftU64(kScratchReg, kScratchReg,
                        Operand(31));  // generate INT32_MIN.
        __ isel(i.OutputRegister(0), kScratchReg, i.OutputRegister(0), crbit);
      }
      break;
    }
    case kPPC_Float32ToUint32: {
      bool set_overflow_to_min_u32 = MiscField::decode(instr->opcode());
      if (set_overflow_to_min_u32) {
        __ mtfsb0(VXCVI);  // clear FPSCR:VXCVI bit
      }
      __ fctiwuz(kScratchDoubleReg, i.InputDoubleRegister(0));
      __ MovDoubleLowToInt(i.OutputRegister(), kScratchDoubleReg);
      if (set_overflow_to_min_u32) {
        // Avoid UINT32_MAX as an overflow indicator and use 0 instead,
        // because 0 allows easier out-of-bounds detection.
        CRegister cr = cr7;
        int crbit = v8::internal::Assembler::encode_crbit(
            cr, static_cast<CRBit>(VXCVI % CRWIDTH));
        __ mcrfs(cr, VXCVI);  // extract FPSCR field containing VXCVI into cr7
        __ li(kScratchReg, Operand::Zero());
        __ isel(i.OutputRegister(0), kScratchReg, i.OutputRegister(0), crbit);
      }
      break;
    }
    case kPPC_DoubleToInt32:
    case kPPC_DoubleToUint32:
    case kPPC_DoubleToInt64: {
#if V8_TARGET_ARCH_PPC64
      bool check_conversion =
          (opcode == kPPC_DoubleToInt64 && i.OutputCount() > 1);
        __ mtfsb0(VXCVI);  // clear FPSCR:VXCVI bit
#endif
      __ ConvertDoubleToInt64(i.InputDoubleRegister(0),
#if !V8_TARGET_ARCH_PPC64
                              kScratchReg,
#endif
                              i.OutputRegister(0), kScratchDoubleReg);
#if V8_TARGET_ARCH_PPC64
        CRegister cr = cr7;
        int crbit = v8::internal::Assembler::encode_crbit(
            cr, static_cast<CRBit>(VXCVI % CRWIDTH));
        __ mcrfs(cr, VXCVI);  // extract FPSCR field containing VXCVI into cr7
        // Handle conversion failures (such as overflow).
        if (CpuFeatures::IsSupported(PPC_7_PLUS)) {
          if (check_conversion) {
            __ li(i.OutputRegister(1), Operand(1));
            __ isel(i.OutputRegister(1), r0, i.OutputRegister(1), crbit);
          } else {
            __ isel(i.OutputRegister(0), r0, i.OutputRegister(0), crbit);
          }
        } else {
          if (check_conversion) {
            __ li(i.OutputRegister(1), Operand::Zero());
            __ bc(v8::internal::kInstrSize * 2, BT, crbit);
            __ li(i.OutputRegister(1), Operand(1));
          } else {
            __ mr(ip, i.OutputRegister(0));
            __ li(i.OutputRegister(0), Operand::Zero());
            __ bc(v8::internal::kInstrSize * 2, BT, crbit);
            __ mr(i.OutputRegister(0), ip);
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
        if (CpuFeatures::IsSupported(PPC_7_PLUS)) {
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
      __ MovFloatToInt(i.OutputRegister(), i.InputDoubleRegister(0),
                       kScratchDoubleReg);
      break;
    case kPPC_BitcastInt32ToFloat32:
      __ MovIntToFloat(i.OutputDoubleRegister(), i.InputRegister(0), ip);
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
      ASSEMBLE_LOAD_INTEGER(lbz, plbz, lbzx);
      break;
    case kPPC_LoadWordS8:
      ASSEMBLE_LOAD_INTEGER(lbz, plbz, lbzx);
      __ extsb(i.OutputRegister(), i.OutputRegister());
      break;
    case kPPC_LoadWordU16:
      ASSEMBLE_LOAD_INTEGER(lhz, plhz, lhzx);
      break;
    case kPPC_LoadWordS16:
      ASSEMBLE_LOAD_INTEGER(lha, plha, lhax);
      break;
    case kPPC_LoadWordU32:
      ASSEMBLE_LOAD_INTEGER(lwz, plwz, lwzx);
      break;
    case kPPC_LoadWordS32:
      ASSEMBLE_LOAD_INTEGER(lwa, plwa, lwax);
      break;
#if V8_TARGET_ARCH_PPC64
    case kPPC_LoadWord64:
      ASSEMBLE_LOAD_INTEGER(ld, pld, ldx);
      break;
#endif
    case kPPC_LoadFloat32:
      ASSEMBLE_LOAD_FLOAT(lfs, plfs, lfsx);
      break;
    case kPPC_LoadDouble:
      ASSEMBLE_LOAD_FLOAT(lfd, plfd, lfdx);
      break;
    case kPPC_LoadSimd128: {
      Simd128Register result = i.OutputSimd128Register();
      AddressingMode mode = kMode_None;
      MemOperand operand = i.MemoryOperand(&mode);
      bool is_atomic = i.InputInt32(2);
      DCHECK_EQ(mode, kMode_MRR);
      __ LoadSimd128(result, operand);
      if (is_atomic) __ lwsync();
      DCHECK_EQ(LeaveRC, i.OutputRCBit());
      break;
    }
    case kPPC_LoadReverseSimd128RR: {
      __ xxbrq(i.OutputSimd128Register(), i.InputSimd128Register(0));
      break;
    }
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
    case kPPC_StoreSimd128: {
      size_t index = 0;
      AddressingMode mode = kMode_None;
      MemOperand operand = i.MemoryOperand(&mode, &index);
      Simd128Register value = i.InputSimd128Register(index);
      bool is_atomic = i.InputInt32(3);
      if (is_atomic) __ lwsync();
      DCHECK_EQ(mode, kMode_MRR);
      __ StoreSimd128(value, operand);
      if (is_atomic) __ sync();
      DCHECK_EQ(LeaveRC, i.OutputRCBit());
      break;
    }
    case kAtomicLoadInt8:
    case kAtomicLoadInt16:
      UNREACHABLE();
    case kAtomicExchangeInt8:
      __ AtomicExchange<int8_t>(
          MemOperand(i.InputRegister(0), i.InputRegister(1)),
          i.InputRegister(2), i.OutputRegister());
      break;
    case kPPC_AtomicExchangeUint8:
      __ AtomicExchange<uint8_t>(
          MemOperand(i.InputRegister(0), i.InputRegister(1)),
          i.InputRegister(2), i.OutputRegister());
      break;
    case kAtomicExchangeInt16: {
      ASSEMBLE_ATOMIC_EXCHANGE(int16_t, ByteReverseU16);
      __ extsh(i.OutputRegister(), i.OutputRegister());
      break;
    }
    case kPPC_AtomicExchangeUint16: {
      ASSEMBLE_ATOMIC_EXCHANGE(uint16_t, ByteReverseU16);
      break;
    }
    case kPPC_AtomicExchangeWord32: {
      ASSEMBLE_ATOMIC_EXCHANGE(uint32_t, ByteReverseU32);
      break;
    }
    case kPPC_AtomicExchangeWord64: {
      ASSEMBLE_ATOMIC_EXCHANGE(uint64_t, ByteReverseU64);
      break;
    }
    case kAtomicCompareExchangeInt8:
      __ AtomicCompareExchange<int8_t>(
          MemOperand(i.InputRegister(0), i.InputRegister(1)),
          i.InputRegister(2), i.InputRegister(3), i.OutputRegister(),
          kScratchReg);
      break;
    case kPPC_AtomicCompareExchangeUint8:
      __ AtomicCompareExchange<uint8_t>(
          MemOperand(i.InputRegister(0), i.InputRegister(1)),
          i.InputRegister(2), i.InputRegister(3), i.OutputRegister(),
          kScratchReg);
      break;
    case kAtomicCompareExchangeInt16: {
      ASSEMBLE_ATOMIC_COMPARE_EXCHANGE(int16_t, ByteReverseU16);
      __ extsh(i.OutputRegister(), i.OutputRegister());
      break;
    }
    case kPPC_AtomicCompareExchangeUint16: {
      ASSEMBLE_ATOMIC_COMPARE_EXCHANGE(uint16_t, ByteReverseU16);
      break;
    }
    case kPPC_AtomicCompareExchangeWord32: {
      ASSEMBLE_ATOMIC_COMPARE_EXCHANGE(uint32_t, ByteReverseU32);
      break;
    }
    case kPPC_AtomicCompareExchangeWord64: {
      ASSEMBLE_ATOMIC_COMPARE_EXCHANGE(uint64_t, ByteReverseU64);
    } break;

#define ATOMIC_BINOP_CASE(op, inst)                            \
  case kPPC_Atomic##op##Int8:                                  \
    ASSEMBLE_ATOMIC_BINOP_BYTE(inst, int8_t);                  \
    __ extsb(i.OutputRegister(), i.OutputRegister());          \
    break;                                                     \
  case kPPC_Atomic##op##Uint8:                                 \
    ASSEMBLE_ATOMIC_BINOP_BYTE(inst, uint8_t);                 \
    break;                                                     \
  case kPPC_Atomic##op##Int16:                                 \
    ASSEMBLE_ATOMIC_BINOP(inst, int16_t, ByteReverseU16, r0);  \
    __ extsh(i.OutputRegister(), i.OutputRegister());          \
    break;                                                     \
  case kPPC_Atomic##op##Uint16:                                \
    ASSEMBLE_ATOMIC_BINOP(inst, uint16_t, ByteReverseU16, r0); \
    break;                                                     \
  case kPPC_Atomic##op##Int32:                                 \
    ASSEMBLE_ATOMIC_BINOP(inst, int32_t, ByteReverseU32, r0);  \
    __ extsw(i.OutputRegister(), i.OutputRegister());          \
    break;                                                     \
  case kPPC_Atomic##op##Uint32:                                \
    ASSEMBLE_ATOMIC_BINOP(inst, uint32_t, ByteReverseU32, r0); \
    break;                                                     \
  case kPPC_Atomic##op##Int64:                                 \
  case kPPC_Atomic##op##Uint64:                                \
    ASSEMBLE_ATOMIC_BINOP(inst, uint64_t, ByteReverseU64, r0); \
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
      if (CpuFeatures::IsSupported(PPC_10_PLUS)) {
        __ brw(output, input);
        __ extsw(output, output);
        break;
      }
      __ rotlwi(temp1, input, 8);
      __ rlwimi(temp1, input, 24, 0, 7);
      __ rlwimi(temp1, input, 24, 16, 23);
      __ extsw(output, temp1);
      break;
    }
    case kPPC_LoadByteRev32: {
      ASSEMBLE_LOAD_INTEGER_RR(lwbrx);
      break;
    }
    case kPPC_StoreByteRev32: {
      ASSEMBLE_STORE_INTEGER_RR(stwbrx);
      break;
    }
    case kPPC_ByteRev64: {
      Register input = i.InputRegister(0);
      Register output = i.OutputRegister();
      Register temp1 = r0;
      Register temp2 = kScratchReg;
      Register temp3 = i.TempRegister(0);
      if (CpuFeatures::IsSupported(PPC_10_PLUS)) {
        __ brd(output, input);
        break;
      }
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
    case kPPC_LoadByteRev64: {
      ASSEMBLE_LOAD_INTEGER_RR(ldbrx);
      break;
    }
    case kPPC_StoreByteRev64: {
      ASSEMBLE_STORE_INTEGER_RR(stdbrx);
      break;
    }
    case kPPC_F64x2Splat: {
      constexpr int lane_width_in_bytes = 8;
      Simd128Register dst = i.OutputSimd128Register();
      __ MovDoubleToInt64(kScratchReg, i.InputDoubleRegister(0));
      __ mtvsrd(dst, kScratchReg);
      __ vinsertd(dst, dst, Operand(1 * lane_width_in_bytes));
      break;
    }
    case kPPC_F32x4Splat: {
      Simd128Register dst = i.OutputSimd128Register();
      __ MovFloatToInt(kScratchReg, i.InputDoubleRegister(0),
                       kScratchDoubleReg);
      __ mtvsrd(dst, kScratchReg);
      __ vspltw(dst, dst, Operand(1));
      break;
    }
    case kPPC_I64x2Splat: {
      constexpr int lane_width_in_bytes = 8;
      Simd128Register dst = i.OutputSimd128Register();
      __ mtvsrd(dst, i.InputRegister(0));
      __ vinsertd(dst, dst, Operand(1 * lane_width_in_bytes));
      break;
    }
    case kPPC_I32x4Splat: {
      Simd128Register dst = i.OutputSimd128Register();
      __ mtvsrd(dst, i.InputRegister(0));
      __ vspltw(dst, dst, Operand(1));
      break;
    }
    case kPPC_I16x8Splat: {
      Simd128Register dst = i.OutputSimd128Register();
      __ mtvsrd(dst, i.InputRegister(0));
      __ vsplth(dst, dst, Operand(3));
      break;
    }
    case kPPC_I8x16Splat: {
      Simd128Register dst = i.OutputSimd128Register();
      __ mtvsrd(dst, i.InputRegister(0));
      __ vspltb(dst, dst, Operand(7));
      break;
    }
    case kPPC_F64x2ExtractLane: {
      constexpr int lane_width_in_bytes = 8;
      __ vextractd(kScratchSimd128Reg, i.InputSimd128Register(0),
                   Operand((1 - i.InputInt8(1)) * lane_width_in_bytes));
      __ mfvsrd(kScratchReg, kScratchSimd128Reg);
      __ MovInt64ToDouble(i.OutputDoubleRegister(), kScratchReg);
      break;
    }
    case kPPC_F32x4ExtractLane: {
      constexpr int lane_width_in_bytes = 4;
      __ vextractuw(kScratchSimd128Reg, i.InputSimd128Register(0),
                    Operand((3 - i.InputInt8(1)) * lane_width_in_bytes));
      __ mfvsrd(kScratchReg, kScratchSimd128Reg);
      __ MovIntToFloat(i.OutputDoubleRegister(), kScratchReg, ip);
      break;
    }
    case kPPC_I64x2ExtractLane: {
      constexpr int lane_width_in_bytes = 8;
      __ vextractd(kScratchSimd128Reg, i.InputSimd128Register(0),
                   Operand((1 - i.InputInt8(1)) * lane_width_in_bytes));
      __ mfvsrd(i.OutputRegister(), kScratchSimd128Reg);
      break;
    }
    case kPPC_I32x4ExtractLane: {
      constexpr int lane_width_in_bytes = 4;
      __ vextractuw(kScratchSimd128Reg, i.InputSimd128Register(0),
                    Operand((3 - i.InputInt8(1)) * lane_width_in_bytes));
      __ mfvsrd(i.OutputRegister(), kScratchSimd128Reg);
      break;
    }
    case kPPC_I16x8ExtractLaneU: {
      constexpr int lane_width_in_bytes = 2;
      __ vextractuh(kScratchSimd128Reg, i.InputSimd128Register(0),
                    Operand((7 - i.InputInt8(1)) * lane_width_in_bytes));
      __ mfvsrd(i.OutputRegister(), kScratchSimd128Reg);
      break;
    }
    case kPPC_I16x8ExtractLaneS: {
      constexpr int lane_width_in_bytes = 2;
      __ vextractuh(kScratchSimd128Reg, i.InputSimd128Register(0),
                    Operand((7 - i.InputInt8(1)) * lane_width_in_bytes));
      __ mfvsrd(kScratchReg, kScratchSimd128Reg);
      __ extsh(i.OutputRegister(), kScratchReg);
      break;
    }
    case kPPC_I8x16ExtractLaneU: {
      __ vextractub(kScratchSimd128Reg, i.InputSimd128Register(0),
                    Operand(15 - i.InputInt8(1)));
      __ mfvsrd(i.OutputRegister(), kScratchSimd128Reg);
      break;
    }
    case kPPC_I8x16ExtractLaneS: {
      __ vextractub(kScratchSimd128Reg, i.InputSimd128Register(0),
                    Operand(15 - i.InputInt8(1)));
      __ mfvsrd(kScratchReg, kScratchSimd128Reg);
      __ extsb(i.OutputRegister(), kScratchReg);
      break;
    }
    case kPPC_F64x2ReplaceLane: {
      DCHECK_EQ(i.OutputSimd128Register(), i.InputSimd128Register(0));
      constexpr int lane_width_in_bytes = 8;
      Simd128Register dst = i.OutputSimd128Register();
      __ MovDoubleToInt64(r0, i.InputDoubleRegister(2));
      if (CpuFeatures::IsSupported(PPC_10_PLUS)) {
        __ vinsd(dst, r0, Operand((1 - i.InputInt8(1)) * lane_width_in_bytes));
      } else {
        __ mtvsrd(kScratchSimd128Reg, r0);
        __ vinsertd(dst, kScratchSimd128Reg,
                    Operand((1 - i.InputInt8(1)) * lane_width_in_bytes));
      }
      break;
    }
    case kPPC_F32x4ReplaceLane: {
      DCHECK_EQ(i.OutputSimd128Register(), i.InputSimd128Register(0));
      constexpr int lane_width_in_bytes = 4;
      Simd128Register dst = i.OutputSimd128Register();
      __ MovFloatToInt(r0, i.InputDoubleRegister(2), kScratchDoubleReg);
      if (CpuFeatures::IsSupported(PPC_10_PLUS)) {
        __ vinsw(dst, r0, Operand((3 - i.InputInt8(1)) * lane_width_in_bytes));
      } else {
        __ mtvsrd(kScratchSimd128Reg, r0);
        __ vinsertw(dst, kScratchSimd128Reg,
                    Operand((3 - i.InputInt8(1)) * lane_width_in_bytes));
      }
      break;
    }
    case kPPC_I64x2ReplaceLane: {
      DCHECK_EQ(i.OutputSimd128Register(), i.InputSimd128Register(0));
      constexpr int lane_width_in_bytes = 8;
      Simd128Register dst = i.OutputSimd128Register();
      if (CpuFeatures::IsSupported(PPC_10_PLUS)) {
        __ vinsd(dst, i.InputRegister(2),
                 Operand((1 - i.InputInt8(1)) * lane_width_in_bytes));
      } else {
        __ mtvsrd(kScratchSimd128Reg, i.InputRegister(2));
        __ vinsertd(dst, kScratchSimd128Reg,
                    Operand((1 - i.InputInt8(1)) * lane_width_in_bytes));
      }
      break;
    }
    case kPPC_I32x4ReplaceLane: {
      DCHECK_EQ(i.OutputSimd128Register(), i.InputSimd128Register(0));
      constexpr int lane_width_in_bytes = 4;
      Simd128Register dst = i.OutputSimd128Register();
      if (CpuFeatures::IsSupported(PPC_10_PLUS)) {
        __ vinsw(dst, i.InputRegister(2),
                 Operand((3 - i.InputInt8(1)) * lane_width_in_bytes));
      } else {
        __ mtvsrd(kScratchSimd128Reg, i.InputRegister(2));
        __ vinsertw(dst, kScratchSimd128Reg,
                    Operand((3 - i.InputInt8(1)) * lane_width_in_bytes));
      }
      break;
    }
    case kPPC_I16x8ReplaceLane: {
      DCHECK_EQ(i.OutputSimd128Register(), i.InputSimd128Register(0));
      constexpr int lane_width_in_bytes = 2;
      Simd128Register dst = i.OutputSimd128Register();
      __ mtvsrd(kScratchSimd128Reg, i.InputRegister(2));
      __ vinserth(dst, kScratchSimd128Reg,
                  Operand((7 - i.InputInt8(1)) * lane_width_in_bytes));
      break;
    }
    case kPPC_I8x16ReplaceLane: {
      DCHECK_EQ(i.OutputSimd128Register(), i.InputSimd128Register(0));
      Simd128Register dst = i.OutputSimd128Register();
      __ mtvsrd(kScratchSimd128Reg, i.InputRegister(2));
      __ vinsertb(dst, kScratchSimd128Reg, Operand(15 - i.InputInt8(1)));
      break;
    }
    case kPPC_F64x2Add: {
      __ xvadddp(i.OutputSimd128Register(), i.InputSimd128Register(0),
                 i.InputSimd128Register(1));
      break;
    }
    case kPPC_F64x2Sub: {
      __ xvsubdp(i.OutputSimd128Register(), i.InputSimd128Register(0),
                 i.InputSimd128Register(1));
      break;
    }
    case kPPC_F64x2Mul: {
      __ xvmuldp(i.OutputSimd128Register(), i.InputSimd128Register(0),
                 i.InputSimd128Register(1));
      break;
    }
    case kPPC_F32x4Add: {
      __ vaddfp(i.OutputSimd128Register(), i.InputSimd128Register(0),
                i.InputSimd128Register(1));
      break;
    }
    case kPPC_F32x4Sub: {
      __ vsubfp(i.OutputSimd128Register(), i.InputSimd128Register(0),
                i.InputSimd128Register(1));
      break;
    }
    case kPPC_F32x4Mul: {
      __ xvmulsp(i.OutputSimd128Register(), i.InputSimd128Register(0),
                 i.InputSimd128Register(1));
      break;
    }
    case kPPC_I64x2Add: {
      __ vaddudm(i.OutputSimd128Register(), i.InputSimd128Register(0),
                 i.InputSimd128Register(1));
      break;
    }
    case kPPC_I64x2Sub: {
      __ vsubudm(i.OutputSimd128Register(), i.InputSimd128Register(0),
                 i.InputSimd128Register(1));
      break;
    }
    case kPPC_I64x2Mul: {
      constexpr int lane_width_in_bytes = 8;
      Simd128Register src0 = i.InputSimd128Register(0);
      Simd128Register src1 = i.InputSimd128Register(1);
      Simd128Register tempFPReg0 = i.ToSimd128Register(instr->TempAt(0));
      Register tempReg1 = i.ToRegister(instr->TempAt(2));
      Register scratch_0 = ip;
      Register scratch_1 = r0;
      Simd128Register dst = i.OutputSimd128Register();
      if (CpuFeatures::IsSupported(PPC_10_PLUS)) {
        __ vmulld(dst, src0, src1);
      } else {
        for (int i = 0; i < 2; i++) {
          if (i > 0) {
            __ vextractd(kScratchSimd128Reg, src0,
                         Operand(1 * lane_width_in_bytes));
            __ vextractd(tempFPReg0, src1, Operand(1 * lane_width_in_bytes));
            src0 = kScratchSimd128Reg;
            src1 = tempFPReg0;
          }
          __ mfvsrd(scratch_0, src0);
          __ mfvsrd(scratch_1, src1);
          __ mulld(scratch_0, scratch_0, scratch_1);
          scratch_0 = r0;
          scratch_1 = tempReg1;
        }
        __ mtvsrdd(dst, ip, r0);
      }
      break;
    }
    case kPPC_I32x4Add: {
      __ vadduwm(i.OutputSimd128Register(), i.InputSimd128Register(0),
                 i.InputSimd128Register(1));
      break;
    }
    case kPPC_I32x4Sub: {
      __ vsubuwm(i.OutputSimd128Register(), i.InputSimd128Register(0),
                 i.InputSimd128Register(1));
      break;
    }
    case kPPC_I32x4Mul: {
      __ vmuluwm(i.OutputSimd128Register(), i.InputSimd128Register(0),
                 i.InputSimd128Register(1));
      break;
    }
    case kPPC_I16x8Add: {
      __ vadduhm(i.OutputSimd128Register(), i.InputSimd128Register(0),
                 i.InputSimd128Register(1));
      break;
    }
    case kPPC_I16x8Sub: {
      __ vsubuhm(i.OutputSimd128Register(), i.InputSimd128Register(0),
                 i.InputSimd128Register(1));
      break;
    }
    case kPPC_I16x8Mul: {
      Simd128Register src0 = i.InputSimd128Register(0);
      Simd128Register src1 = i.InputSimd128Register(1);
      Simd128Register dst = i.OutputSimd128Register();
      __ vxor(kScratchSimd128Reg, kScratchSimd128Reg, kScratchSimd128Reg);
      __ vmladduhm(dst, src0, src1, kScratchSimd128Reg);
      break;
    }
    case kPPC_I8x16Add: {
      __ vaddubm(i.OutputSimd128Register(), i.InputSimd128Register(0),
                 i.InputSimd128Register(1));
      break;
    }
    case kPPC_I8x16Sub: {
      __ vsububm(i.OutputSimd128Register(), i.InputSimd128Register(0),
                 i.InputSimd128Register(1));
      break;
    }
    case kPPC_I32x4MinS: {
      __ vminsw(i.OutputSimd128Register(), i.InputSimd128Register(0),
                i.InputSimd128Register(1));
      break;
    }
    case kPPC_I32x4MinU: {
      __ vminuw(i.OutputSimd128Register(), i.InputSimd128Register(0),
                i.InputSimd128Register(1));
      break;
    }
    case kPPC_I16x8MinS: {
      __ vminsh(i.OutputSimd128Register(), i.InputSimd128Register(0),
                i.InputSimd128Register(1));
      break;
    }
    case kPPC_I16x8MinU: {
      __ vminuh(i.OutputSimd128Register(), i.InputSimd128Register(0),
                i.InputSimd128Register(1));
      break;
    }
    case kPPC_I8x16MinS: {
      __ vminsb(i.OutputSimd128Register(), i.InputSimd128Register(0),
                i.InputSimd128Register(1));
      break;
    }
    case kPPC_I8x16MinU: {
      __ vminub(i.OutputSimd128Register(), i.InputSimd128Register(0),
                i.InputSimd128Register(1));
      break;
    }
    case kPPC_I32x4MaxS: {
      __ vmaxsw(i.OutputSimd128Register(), i.InputSimd128Register(0),
                i.InputSimd128Register(1));
      break;
    }
    case kPPC_I32x4MaxU: {
      __ vmaxuw(i.OutputSimd128Register(), i.InputSimd128Register(0),
                i.InputSimd128Register(1));
      break;
    }
    case kPPC_I16x8MaxS: {
      __ vmaxsh(i.OutputSimd128Register(), i.InputSimd128Register(0),
                i.InputSimd128Register(1));
      break;
    }
    case kPPC_I16x8MaxU: {
      __ vmaxuh(i.OutputSimd128Register(), i.InputSimd128Register(0),
                i.InputSimd128Register(1));
      break;
    }
    case kPPC_I8x16MaxS: {
      __ vmaxsb(i.OutputSimd128Register(), i.InputSimd128Register(0),
                i.InputSimd128Register(1));
      break;
    }
    case kPPC_I8x16MaxU: {
      __ vmaxub(i.OutputSimd128Register(), i.InputSimd128Register(0),
                i.InputSimd128Register(1));
      break;
    }
    case kPPC_F64x2Eq: {
      __ xvcmpeqdp(i.OutputSimd128Register(), i.InputSimd128Register(0),
                   i.InputSimd128Register(1));
      break;
    }
    case kPPC_F64x2Ne: {
      __ xvcmpeqdp(kScratchSimd128Reg, i.InputSimd128Register(0),
                   i.InputSimd128Register(1));
      __ vnor(i.OutputSimd128Register(), kScratchSimd128Reg,
              kScratchSimd128Reg);
      break;
    }
    case kPPC_F64x2Le: {
      __ xvcmpgedp(i.OutputSimd128Register(), i.InputSimd128Register(1),
                   i.InputSimd128Register(0));
      break;
    }
    case kPPC_F64x2Lt: {
      __ xvcmpgtdp(i.OutputSimd128Register(), i.InputSimd128Register(1),
                   i.InputSimd128Register(0));
      break;
    }
    case kPPC_F32x4Eq: {
      __ xvcmpeqsp(i.OutputSimd128Register(), i.InputSimd128Register(0),
                   i.InputSimd128Register(1));
      break;
    }
    case kPPC_I64x2Eq: {
      __ vcmpequd(i.OutputSimd128Register(), i.InputSimd128Register(0),
                  i.InputSimd128Register(1));
      break;
    }
    case kPPC_I32x4Eq: {
      __ vcmpequw(i.OutputSimd128Register(), i.InputSimd128Register(0),
                  i.InputSimd128Register(1));
      break;
    }
    case kPPC_I16x8Eq: {
      __ vcmpequh(i.OutputSimd128Register(), i.InputSimd128Register(0),
                  i.InputSimd128Register(1));
      break;
    }
    case kPPC_I8x16Eq: {
      __ vcmpequb(i.OutputSimd128Register(), i.InputSimd128Register(0),
                  i.InputSimd128Register(1));
      break;
    }
    case kPPC_F32x4Ne: {
      __ xvcmpeqsp(kScratchSimd128Reg, i.InputSimd128Register(0),
                   i.InputSimd128Register(1));
      __ vnor(i.OutputSimd128Register(), kScratchSimd128Reg,
              kScratchSimd128Reg);
      break;
    }
    case kPPC_I64x2Ne: {
      __ vcmpequd(kScratchSimd128Reg, i.InputSimd128Register(0),
                  i.InputSimd128Register(1));
      __ vnor(i.OutputSimd128Register(), kScratchSimd128Reg,
              kScratchSimd128Reg);
      break;
    }
    case kPPC_I32x4Ne: {
      __ vcmpequw(kScratchSimd128Reg, i.InputSimd128Register(0),
                  i.InputSimd128Register(1));
      __ vnor(i.OutputSimd128Register(), kScratchSimd128Reg,
              kScratchSimd128Reg);
      break;
    }
    case kPPC_I16x8Ne: {
      __ vcmpequh(kScratchSimd128Reg, i.InputSimd128Register(0),
                  i.InputSimd128Register(1));
      __ vnor(i.OutputSimd128Register(), kScratchSimd128Reg,
              kScratchSimd128Reg);
      break;
    }
    case kPPC_I8x16Ne: {
      __ vcmpequb(kScratchSimd128Reg, i.InputSimd128Register(0),
                  i.InputSimd128Register(1));
      __ vnor(i.OutputSimd128Register(), kScratchSimd128Reg,
              kScratchSimd128Reg);
      break;
    }
    case kPPC_F32x4Lt: {
      __ xvcmpgtsp(i.OutputSimd128Register(), i.InputSimd128Register(1),
                   i.InputSimd128Register(0));
      break;
    }
    case kPPC_F32x4Le: {
      __ xvcmpgesp(i.OutputSimd128Register(), i.InputSimd128Register(1),
                   i.InputSimd128Register(0));
      break;
    }
    case kPPC_I64x2GtS: {
      __ vcmpgtsd(i.OutputSimd128Register(), i.InputSimd128Register(0),
                  i.InputSimd128Register(1));
      break;
    }
    case kPPC_I32x4GtS: {
      __ vcmpgtsw(i.OutputSimd128Register(), i.InputSimd128Register(0),
                  i.InputSimd128Register(1));
      break;
    }
    case kPPC_I64x2GeS: {
      __ vcmpgtsd(kScratchSimd128Reg, i.InputSimd128Register(1),
                  i.InputSimd128Register(0));
      __ vnor(i.OutputSimd128Register(), kScratchSimd128Reg,
              kScratchSimd128Reg);
      break;
    }
    case kPPC_I32x4GeS: {
      __ vcmpgtsw(kScratchSimd128Reg, i.InputSimd128Register(1),
                  i.InputSimd128Register(0));
      __ vnor(i.OutputSimd128Register(), kScratchSimd128Reg,
              kScratchSimd128Reg);
      break;
    }
    case kPPC_I32x4GtU: {
      __ vcmpgtuw(i.OutputSimd128Register(), i.InputSimd128Register(0),
                  i.InputSimd128Register(1));

      break;
    }
    case kPPC_I32x4GeU: {
      __ vcmpequw(kScratchSimd128Reg, i.InputSimd128Register(0),
                  i.InputSimd128Register(1));
      __ vcmpgtuw(i.OutputSimd128Register(), i.InputSimd128Register(0),
                  i.InputSimd128Register(1));
      __ vor(i.OutputSimd128Register(), i.OutputSimd128Register(),
             kScratchSimd128Reg);
      break;
    }
    case kPPC_I16x8GtS: {
      __ vcmpgtsh(i.OutputSimd128Register(), i.InputSimd128Register(0),
                  i.InputSimd128Register(1));
      break;
    }
    case kPPC_I16x8GeS: {
      __ vcmpgtsh(kScratchSimd128Reg, i.InputSimd128Register(1),
                  i.InputSimd128Register(0));
      __ vnor(i.OutputSimd128Register(), kScratchSimd128Reg,
              kScratchSimd128Reg);
      break;
    }
    case kPPC_I16x8GtU: {
      __ vcmpgtuh(i.OutputSimd128Register(), i.InputSimd128Register(0),
                  i.InputSimd128Register(1));
      break;
    }
    case kPPC_I16x8GeU: {
      __ vcmpequh(kScratchSimd128Reg, i.InputSimd128Register(0),
                  i.InputSimd128Register(1));
      __ vcmpgtuh(i.OutputSimd128Register(), i.InputSimd128Register(0),
                  i.InputSimd128Register(1));
      __ vor(i.OutputSimd128Register(), i.OutputSimd128Register(),
             kScratchSimd128Reg);
      break;
    }
    case kPPC_I8x16GtS: {
      __ vcmpgtsb(i.OutputSimd128Register(), i.InputSimd128Register(0),
                  i.InputSimd128Register(1));
      break;
    }
    case kPPC_I8x16GeS: {
      __ vcmpgtsb(kScratchSimd128Reg, i.InputSimd128Register(1),
                  i.InputSimd128Register(0));
      __ vnor(i.OutputSimd128Register(), kScratchSimd128Reg,
              kScratchSimd128Reg);
      break;
    }
    case kPPC_I8x16GtU: {
      __ vcmpgtub(i.OutputSimd128Register(), i.InputSimd128Register(0),
                  i.InputSimd128Register(1));
      break;
    }
    case kPPC_I8x16GeU: {
      __ vcmpequb(kScratchSimd128Reg, i.InputSimd128Register(0),
                  i.InputSimd128Register(1));
      __ vcmpgtub(i.OutputSimd128Register(), i.InputSimd128Register(0),
                  i.InputSimd128Register(1));
      __ vor(i.OutputSimd128Register(), i.OutputSimd128Register(),
             kScratchSimd128Reg);
      break;
    }
#define VECTOR_SHIFT(op)                                           \
  {                                                                \
    __ mtvsrd(kScratchSimd128Reg, i.InputRegister(1));             \
    __ vspltb(kScratchSimd128Reg, kScratchSimd128Reg, Operand(7)); \
    __ op(i.OutputSimd128Register(), i.InputSimd128Register(0),    \
          kScratchSimd128Reg);                                     \
  }
    case kPPC_I64x2Shl: {
      VECTOR_SHIFT(vsld)
      break;
    }
    case kPPC_I64x2ShrS: {
      VECTOR_SHIFT(vsrad)
      break;
    }
    case kPPC_I64x2ShrU: {
      VECTOR_SHIFT(vsrd)
      break;
    }
    case kPPC_I32x4Shl: {
      VECTOR_SHIFT(vslw)
      break;
    }
    case kPPC_I32x4ShrS: {
      VECTOR_SHIFT(vsraw)
      break;
    }
    case kPPC_I32x4ShrU: {
      VECTOR_SHIFT(vsrw)
      break;
    }
    case kPPC_I16x8Shl: {
      VECTOR_SHIFT(vslh)
      break;
    }
    case kPPC_I16x8ShrS: {
      VECTOR_SHIFT(vsrah)
      break;
    }
    case kPPC_I16x8ShrU: {
      VECTOR_SHIFT(vsrh)
      break;
    }
    case kPPC_I8x16Shl: {
      VECTOR_SHIFT(vslb)
      break;
    }
    case kPPC_I8x16ShrS: {
      VECTOR_SHIFT(vsrab)
      break;
    }
    case kPPC_I8x16ShrU: {
      VECTOR_SHIFT(vsrb)
      break;
    }
#undef VECTOR_SHIFT
    case kPPC_S128And: {
      Simd128Register dst = i.OutputSimd128Register();
      Simd128Register src = i.InputSimd128Register(1);
      __ vand(dst, i.InputSimd128Register(0), src);
      break;
    }
    case kPPC_S128Or: {
      Simd128Register dst = i.OutputSimd128Register();
      Simd128Register src = i.InputSimd128Register(1);
      __ vor(dst, i.InputSimd128Register(0), src);
      break;
    }
    case kPPC_S128Xor: {
      Simd128Register dst = i.OutputSimd128Register();
      Simd128Register src = i.InputSimd128Register(1);
      __ vxor(dst, i.InputSimd128Register(0), src);
      break;
    }
    case kPPC_S128Const: {
      uint64_t low = make_uint64(i.InputUint32(1), i.InputUint32(0));
      uint64_t high = make_uint64(i.InputUint32(3), i.InputUint32(2));
      __ mov(r0, Operand(low));
      __ mov(ip, Operand(high));
      __ mtvsrdd(i.OutputSimd128Register(), ip, r0);
      break;
    }
    case kPPC_S128Zero: {
      Simd128Register dst = i.OutputSimd128Register();
      __ vxor(dst, dst, dst);
      break;
    }
    case kPPC_S128AllOnes: {
      Simd128Register dst = i.OutputSimd128Register();
      __ vcmpequb(dst, dst, dst);
      break;
    }
    case kPPC_S128Not: {
      Simd128Register dst = i.OutputSimd128Register();
      Simd128Register src = i.InputSimd128Register(0);
      __ vnor(dst, src, src);
      break;
    }
    case kPPC_S128Select: {
      Simd128Register dst = i.OutputSimd128Register();
      Simd128Register mask = i.InputSimd128Register(0);
      Simd128Register src1 = i.InputSimd128Register(1);
      Simd128Register src2 = i.InputSimd128Register(2);
      __ vsel(dst, src2, src1, mask);
      break;
    }
    case kPPC_F64x2Abs: {
      __ xvabsdp(i.OutputSimd128Register(), i.InputSimd128Register(0));
      break;
    }
    case kPPC_F64x2Neg: {
      __ xvnegdp(i.OutputSimd128Register(), i.InputSimd128Register(0));
      break;
    }
    case kPPC_F64x2Sqrt: {
      __ xvsqrtdp(i.OutputSimd128Register(), i.InputSimd128Register(0));
      break;
    }
    case kPPC_F32x4Abs: {
      __ xvabssp(i.OutputSimd128Register(), i.InputSimd128Register(0));
      break;
    }
    case kPPC_F32x4Neg: {
      __ xvnegsp(i.OutputSimd128Register(), i.InputSimd128Register(0));
      break;
    }
    case kPPC_F32x4RecipApprox: {
      __ xvresp(i.OutputSimd128Register(), i.InputSimd128Register(0));
      break;
    }
    case kPPC_F32x4RecipSqrtApprox: {
      __ xvrsqrtesp(i.OutputSimd128Register(), i.InputSimd128Register(0));
      break;
    }
    case kPPC_F32x4Sqrt: {
      __ xvsqrtsp(i.OutputSimd128Register(), i.InputSimd128Register(0));
      break;
    }
    case kPPC_I64x2Neg: {
      __ vnegd(i.OutputSimd128Register(), i.InputSimd128Register(0));
      break;
    }
    case kPPC_I32x4Neg: {
      __ vnegw(i.OutputSimd128Register(), i.InputSimd128Register(0));
      break;
    }
    case kPPC_I64x2Abs: {
      Simd128Register tempFPReg1 = i.ToSimd128Register(instr->TempAt(0));
      Simd128Register src = i.InputSimd128Register(0);
      constexpr int shift_bits = 63;
      __ xxspltib(kScratchSimd128Reg, Operand(shift_bits));
      __ vsrad(kScratchSimd128Reg, src, kScratchSimd128Reg);
      __ vxor(tempFPReg1, src, kScratchSimd128Reg);
      __ vsubudm(i.OutputSimd128Register(), tempFPReg1, kScratchSimd128Reg);
      break;
    }
    case kPPC_I32x4Abs: {
      Simd128Register tempFPReg1 = i.ToSimd128Register(instr->TempAt(0));
      Simd128Register src = i.InputSimd128Register(0);
      constexpr int shift_bits = 31;
      __ xxspltib(kScratchSimd128Reg, Operand(shift_bits));
      __ vsraw(kScratchSimd128Reg, src, kScratchSimd128Reg);
      __ vxor(tempFPReg1, src, kScratchSimd128Reg);
      __ vsubuwm(i.OutputSimd128Register(), tempFPReg1, kScratchSimd128Reg);
      break;
    }
    case kPPC_I16x8Neg: {
      Simd128Register tempFPReg1 = i.ToSimd128Register(instr->TempAt(0));
      __ vspltish(kScratchSimd128Reg, Operand(1));
      __ vnor(tempFPReg1, i.InputSimd128Register(0), i.InputSimd128Register(0));
      __ vadduhm(i.OutputSimd128Register(), kScratchSimd128Reg, tempFPReg1);
      break;
    }
    case kPPC_I16x8Abs: {
      Simd128Register tempFPReg1 = i.ToSimd128Register(instr->TempAt(0));
      Simd128Register src = i.InputSimd128Register(0);
      constexpr int shift_bits = 15;
      __ xxspltib(kScratchSimd128Reg, Operand(shift_bits));
      __ vsrah(kScratchSimd128Reg, src, kScratchSimd128Reg);
      __ vxor(tempFPReg1, src, kScratchSimd128Reg);
      __ vsubuhm(i.OutputSimd128Register(), tempFPReg1, kScratchSimd128Reg);
      break;
    }
    case kPPC_I8x16Neg: {
      Simd128Register tempFPReg1 = i.ToSimd128Register(instr->TempAt(0));
      __ xxspltib(kScratchSimd128Reg, Operand(1));
      __ vnor(tempFPReg1, i.InputSimd128Register(0), i.InputSimd128Register(0));
      __ vaddubm(i.OutputSimd128Register(), kScratchSimd128Reg, tempFPReg1);
      break;
    }
    case kPPC_I8x16Abs: {
      Simd128Register tempFPReg1 = i.ToSimd128Register(instr->TempAt(0));
      Simd128Register src = i.InputSimd128Register(0);
      constexpr int shift_bits = 7;
      __ xxspltib(kScratchSimd128Reg, Operand(shift_bits));
      __ vsrab(kScratchSimd128Reg, src, kScratchSimd128Reg);
      __ vxor(tempFPReg1, src, kScratchSimd128Reg);
      __ vsububm(i.OutputSimd128Register(), tempFPReg1, kScratchSimd128Reg);
      break;
    }
    case kPPC_V128AnyTrue: {
      Simd128Register src = i.InputSimd128Register(0);
      Register dst = i.OutputRegister();
      constexpr uint8_t fxm = 0x2;  // field mask.
      constexpr int bit_number = 24;
      __ li(r0, Operand(0));
      __ li(ip, Operand(1));
      // Check if both lanes are 0, if so then return false.
      __ vxor(kScratchSimd128Reg, kScratchSimd128Reg, kScratchSimd128Reg);
      __ mtcrf(r0, fxm);  // Clear cr6.
      __ vcmpequd(kScratchSimd128Reg, src, kScratchSimd128Reg, SetRC);
      __ isel(dst, r0, ip, bit_number);
      break;
    }
#define SIMD_ALL_TRUE(opcode)                                          \
  Simd128Register src = i.InputSimd128Register(0);                     \
  Register dst = i.OutputRegister();                                   \
  constexpr uint8_t fxm = 0x2; /* field mask. */                       \
  constexpr int bit_number = 24;                                       \
  __ li(r0, Operand(0));                                               \
  __ li(ip, Operand(1));                                               \
  /* Check if all lanes > 0, if not then return false.*/               \
  __ vxor(kScratchSimd128Reg, kScratchSimd128Reg, kScratchSimd128Reg); \
  __ mtcrf(r0, fxm); /* Clear cr6.*/                                   \
  __ opcode(kScratchSimd128Reg, src, kScratchSimd128Reg, SetRC);       \
  __ isel(dst, ip, r0, bit_number);
    case kPPC_I64x2AllTrue: {
      SIMD_ALL_TRUE(vcmpgtud)
      break;
    }
    case kPPC_I32x4AllTrue: {
      SIMD_ALL_TRUE(vcmpgtuw)
      break;
    }
    case kPPC_I16x8AllTrue: {
      SIMD_ALL_TRUE(vcmpgtuh)
      break;
    }
    case kPPC_I8x16AllTrue: {
      SIMD_ALL_TRUE(vcmpgtub)
      break;
    }
#undef SIMD_ALL_TRUE
    case kPPC_I32x4SConvertF32x4: {
      Simd128Register src = i.InputSimd128Register(0);
      // NaN to 0
      __ vor(kScratchSimd128Reg, src, src);
      __ xvcmpeqsp(kScratchSimd128Reg, kScratchSimd128Reg, kScratchSimd128Reg);
      __ vand(kScratchSimd128Reg, src, kScratchSimd128Reg);
      __ xvcvspsxws(i.OutputSimd128Register(), kScratchSimd128Reg);
      break;
    }
    case kPPC_I32x4UConvertF32x4: {
      __ xvcvspuxws(i.OutputSimd128Register(), i.InputSimd128Register(0));
      break;
    }
    case kPPC_F32x4SConvertI32x4: {
      __ xvcvsxwsp(i.OutputSimd128Register(), i.InputSimd128Register(0));
      break;
    }
    case kPPC_F32x4UConvertI32x4: {
      __ xvcvuxwsp(i.OutputSimd128Register(), i.InputSimd128Register(0));
      break;
    }

    case kPPC_I64x2SConvertI32x4Low: {
      __ vupklsw(i.OutputSimd128Register(), i.InputSimd128Register(0));
      break;
    }
    case kPPC_I64x2SConvertI32x4High: {
      __ vupkhsw(i.OutputSimd128Register(), i.InputSimd128Register(0));
      break;
    }
    case kPPC_I64x2UConvertI32x4Low: {
      constexpr int lane_width_in_bytes = 8;
      __ vupklsw(i.OutputSimd128Register(), i.InputSimd128Register(0));
      // Zero extend.
      __ mov(ip, Operand(0xFFFFFFFF));
      __ mtvsrd(kScratchSimd128Reg, ip);
      __ vinsertd(kScratchSimd128Reg, kScratchSimd128Reg,
                  Operand(1 * lane_width_in_bytes));
      __ vand(i.OutputSimd128Register(), kScratchSimd128Reg,
              i.OutputSimd128Register());
      break;
    }
    case kPPC_I64x2UConvertI32x4High: {
      constexpr int lane_width_in_bytes = 8;
      __ vupkhsw(i.OutputSimd128Register(), i.InputSimd128Register(0));
      // Zero extend.
      __ mov(ip, Operand(0xFFFFFFFF));
      __ mtvsrd(kScratchSimd128Reg, ip);
      __ vinsertd(kScratchSimd128Reg, kScratchSimd128Reg,
                  Operand(1 * lane_width_in_bytes));
      __ vand(i.OutputSimd128Register(), kScratchSimd128Reg,
              i.OutputSimd128Register());
      break;
    }

    case kPPC_I32x4SConvertI16x8Low: {
      __ vupklsh(i.OutputSimd128Register(), i.InputSimd128Register(0));
      break;
    }
    case kPPC_I32x4SConvertI16x8High: {
      __ vupkhsh(i.OutputSimd128Register(), i.InputSimd128Register(0));
      break;
    }
    case kPPC_I32x4UConvertI16x8Low: {
      __ vupklsh(i.OutputSimd128Register(), i.InputSimd128Register(0));
      // Zero extend.
      __ mov(ip, Operand(0xFFFF));
      __ mtvsrd(kScratchSimd128Reg, ip);
      __ vspltw(kScratchSimd128Reg, kScratchSimd128Reg, Operand(1));
      __ vand(i.OutputSimd128Register(), kScratchSimd128Reg,
              i.OutputSimd128Register());
      break;
    }
    case kPPC_I32x4UConvertI16x8High: {
      __ vupkhsh(i.OutputSimd128Register(), i.InputSimd128Register(0));
      // Zero extend.
      __ mov(ip, Operand(0xFFFF));
      __ mtvsrd(kScratchSimd128Reg, ip);
      __ vspltw(kScratchSimd128Reg, kScratchSimd128Reg, Operand(1));
      __ vand(i.OutputSimd128Register(), kScratchSimd128Reg,
              i.OutputSimd128Register());
      break;
    }

    case kPPC_I16x8SConvertI8x16Low: {
      __ vupklsb(i.OutputSimd128Register(), i.InputSimd128Register(0));
      break;
    }
    case kPPC_I16x8SConvertI8x16High: {
      __ vupkhsb(i.OutputSimd128Register(), i.InputSimd128Register(0));
      break;
    }
    case kPPC_I16x8UConvertI8x16Low: {
      __ vupklsb(i.OutputSimd128Register(), i.InputSimd128Register(0));
      // Zero extend.
      __ li(ip, Operand(0xFF));
      __ mtvsrd(kScratchSimd128Reg, ip);
      __ vsplth(kScratchSimd128Reg, kScratchSimd128Reg, Operand(3));
      __ vand(i.OutputSimd128Register(), kScratchSimd128Reg,
              i.OutputSimd128Register());
      break;
    }
    case kPPC_I16x8UConvertI8x16High: {
      __ vupkhsb(i.OutputSimd128Register(), i.InputSimd128Register(0));
      // Zero extend.
      __ li(ip, Operand(0xFF));
      __ mtvsrd(kScratchSimd128Reg, ip);
      __ vsplth(kScratchSimd128Reg, kScratchSimd128Reg, Operand(3));
      __ vand(i.OutputSimd128Register(), kScratchSimd128Reg,
              i.OutputSimd128Register());
      break;
    }
    case kPPC_I16x8SConvertI32x4: {
      __ vpkswss(i.OutputSimd128Register(), i.InputSimd128Register(1),
                 i.InputSimd128Register(0));
      break;
    }
    case kPPC_I16x8UConvertI32x4: {
      __ vpkswus(i.OutputSimd128Register(), i.InputSimd128Register(1),
                 i.InputSimd128Register(0));
      break;
    }
    case kPPC_I8x16SConvertI16x8: {
      __ vpkshss(i.OutputSimd128Register(), i.InputSimd128Register(1),
                 i.InputSimd128Register(0));
      break;
    }
    case kPPC_I8x16UConvertI16x8: {
      __ vpkshus(i.OutputSimd128Register(), i.InputSimd128Register(1),
                 i.InputSimd128Register(0));
      break;
    }
    case kPPC_I8x16Shuffle: {
      Simd128Register dst = i.OutputSimd128Register(),
                      src0 = i.InputSimd128Register(0),
                      src1 = i.InputSimd128Register(1);
      uint64_t low = make_uint64(i.InputUint32(3), i.InputUint32(2));
      uint64_t high = make_uint64(i.InputUint32(5), i.InputUint32(4));
      __ mov(r0, Operand(low));
      __ mov(ip, Operand(high));
      __ mtvsrdd(dst, ip, r0);
      __ vperm(dst, src0, src1, dst);
      break;
    }
    case kPPC_I16x8AddSatS: {
      __ vaddshs(i.OutputSimd128Register(), i.InputSimd128Register(0),
                 i.InputSimd128Register(1));
      break;
    }
    case kPPC_I16x8SubSatS: {
      __ vsubshs(i.OutputSimd128Register(), i.InputSimd128Register(0),
                 i.InputSimd128Register(1));
      break;
    }
    case kPPC_I16x8AddSatU: {
      __ vadduhs(i.OutputSimd128Register(), i.InputSimd128Register(0),
                 i.InputSimd128Register(1));
      break;
    }
    case kPPC_I16x8SubSatU: {
      __ vsubuhs(i.OutputSimd128Register(), i.InputSimd128Register(0),
                 i.InputSimd128Register(1));
      break;
    }
    case kPPC_I8x16AddSatS: {
      __ vaddsbs(i.OutputSimd128Register(), i.InputSimd128Register(0),
                 i.InputSimd128Register(1));
      break;
    }
    case kPPC_I8x16SubSatS: {
      __ vsubsbs(i.OutputSimd128Register(), i.InputSimd128Register(0),
                 i.InputSimd128Register(1));
      break;
    }
    case kPPC_I8x16AddSatU: {
      __ vaddubs(i.OutputSimd128Register(), i.InputSimd128Register(0),
                 i.InputSimd128Register(1));
      break;
    }
    case kPPC_I8x16SubSatU: {
      __ vsububs(i.OutputSimd128Register(), i.InputSimd128Register(0),
                 i.InputSimd128Register(1));
      break;
    }
    case kPPC_I8x16Swizzle: {
      Simd128Register dst = i.OutputSimd128Register(),
                      src0 = i.InputSimd128Register(0),
                      src1 = i.InputSimd128Register(1),
                      tempFPReg1 = i.ToSimd128Register(instr->TempAt(0));
      // Saturate the indices to 5 bits. Input indices more than 31 should
      // return 0.
      __ xxspltib(tempFPReg1, Operand(31));
      __ vminub(tempFPReg1, src1, tempFPReg1);
      //  input needs to be reversed.
      __ xxbrq(dst, src0);
      __ vxor(kScratchSimd128Reg, kScratchSimd128Reg, kScratchSimd128Reg);
      __ vperm(dst, dst, kScratchSimd128Reg, tempFPReg1);
      break;
    }
    case kPPC_F64x2Qfma: {
      Simd128Register src0 = i.InputSimd128Register(0);
      Simd128Register src1 = i.InputSimd128Register(1);
      Simd128Register src2 = i.InputSimd128Register(2);
      Simd128Register dst = i.OutputSimd128Register();
      __ vor(kScratchSimd128Reg, src1, src1);
      __ xvmaddmdp(kScratchSimd128Reg, src2, src0);
      __ vor(dst, kScratchSimd128Reg, kScratchSimd128Reg);
      break;
    }
    case kPPC_F64x2Qfms: {
      Simd128Register src0 = i.InputSimd128Register(0);
      Simd128Register src1 = i.InputSimd128Register(1);
      Simd128Register src2 = i.InputSimd128Register(2);
      Simd128Register dst = i.OutputSimd128Register();
      __ vor(kScratchSimd128Reg, src1, src1);
      __ xvnmsubmdp(kScratchSimd128Reg, src2, src0);
      __ vor(dst, kScratchSimd128Reg, kScratchSimd128Reg);
      break;
    }
    case kPPC_F32x4Qfma: {
      Simd128Register src0 = i.InputSimd128Register(0);
      Simd128Register src1 = i.InputSimd128Register(1);
      Simd128Register src2 = i.InputSimd128Register(2);
      Simd128Register dst = i.OutputSimd128Register();
      __ vor(kScratchSimd128Reg, src1, src1);
      __ xvmaddmsp(kScratchSimd128Reg, src2, src0);
      __ vor(dst, kScratchSimd128Reg, kScratchSimd128Reg);
      break;
    }
    case kPPC_F32x4Qfms: {
      Simd128Register src0 = i.InputSimd128Register(0);
      Simd128Register src1 = i.InputSimd128Register(1);
      Simd128Register src2 = i.InputSimd128Register(2);
      Simd128Register dst = i.OutputSimd128Register();
      __ vor(kScratchSimd128Reg, src1, src1);
      __ xvnmsubmsp(kScratchSimd128Reg, src2, src0);
      __ vor(dst, kScratchSimd128Reg, kScratchSimd128Reg);
      break;
    }
    case kPPC_I16x8RoundingAverageU: {
      __ vavguh(i.OutputSimd128Register(), i.InputSimd128Register(0),
                i.InputSimd128Register(1));
      break;
    }
    case kPPC_I8x16RoundingAverageU: {
      __ vavgub(i.OutputSimd128Register(), i.InputSimd128Register(0),
                i.InputSimd128Register(1));
      break;
    }
    case kPPC_S128AndNot: {
      Simd128Register dst = i.OutputSimd128Register();
      Simd128Register src = i.InputSimd128Register(0);
      __ vandc(dst, src, i.InputSimd128Register(1));
      break;
    }
    case kPPC_F64x2Div: {
      __ xvdivdp(i.OutputSimd128Register(), i.InputSimd128Register(0),
                 i.InputSimd128Register(1));
      break;
    }
#define F64X2_MIN_MAX_NAN(result)                                       \
  Simd128Register tempFPReg1 = i.ToSimd128Register(instr->TempAt(0));   \
  __ xvcmpeqdp(tempFPReg1, i.InputSimd128Register(0),                   \
               i.InputSimd128Register(0));                              \
  __ vsel(result, i.InputSimd128Register(0), result, tempFPReg1);       \
  __ xvcmpeqdp(tempFPReg1, i.InputSimd128Register(1),                   \
               i.InputSimd128Register(1));                              \
  __ vsel(i.OutputSimd128Register(), i.InputSimd128Register(1), result, \
          tempFPReg1);                                                  \
  /* Use xvmindp to turn any selected SNANs to QNANs. */                \
  __ xvmindp(i.OutputSimd128Register(), i.OutputSimd128Register(),      \
             i.OutputSimd128Register());
    case kPPC_F64x2Min: {
      __ xvmindp(kScratchSimd128Reg, i.InputSimd128Register(0),
                 i.InputSimd128Register(1));
      // We need to check if an input is NAN and preserve it.
      F64X2_MIN_MAX_NAN(kScratchSimd128Reg)
      break;
    }
    case kPPC_F64x2Max: {
      __ xvmaxdp(kScratchSimd128Reg, i.InputSimd128Register(0),
                 i.InputSimd128Register(1));
      // We need to check if an input is NAN and preserve it.
      F64X2_MIN_MAX_NAN(kScratchSimd128Reg)
      break;
    }
#undef F64X2_MIN_MAX_NAN
    case kPPC_F32x4Div: {
      __ xvdivsp(i.OutputSimd128Register(), i.InputSimd128Register(0),
                 i.InputSimd128Register(1));
      break;
    }
    case kPPC_F32x4Min: {
      __ vminfp(i.OutputSimd128Register(), i.InputSimd128Register(0),
                i.InputSimd128Register(1));
      break;
    }
    case kPPC_F32x4Max: {
      __ vmaxfp(i.OutputSimd128Register(), i.InputSimd128Register(0),
                i.InputSimd128Register(1));
      break;
    }
    case kPPC_F64x2Ceil: {
      __ xvrdpip(i.OutputSimd128Register(), i.InputSimd128Register(0));
      break;
    }
    case kPPC_F64x2Floor: {
      __ xvrdpim(i.OutputSimd128Register(), i.InputSimd128Register(0));
      break;
    }
    case kPPC_F64x2Trunc: {
      __ xvrdpiz(i.OutputSimd128Register(), i.InputSimd128Register(0));
      break;
    }
    case kPPC_F32x4Ceil: {
      __ xvrspip(i.OutputSimd128Register(), i.InputSimd128Register(0));
      break;
    }
    case kPPC_F32x4Floor: {
      __ xvrspim(i.OutputSimd128Register(), i.InputSimd128Register(0));
      break;
    }
    case kPPC_F32x4Trunc: {
      __ xvrspiz(i.OutputSimd128Register(), i.InputSimd128Register(0));
      break;
    }
    case kPPC_I64x2BitMask: {
      if (CpuFeatures::IsSupported(PPC_10_PLUS)) {
        __ vextractdm(i.OutputRegister(), i.InputSimd128Register(0));
      } else {
        __ mov(kScratchReg,
               Operand(0x8080808080800040));  // Select 0 for the high bits.
        __ mtvsrd(kScratchSimd128Reg, kScratchReg);
        __ vbpermq(kScratchSimd128Reg, i.InputSimd128Register(0),
                   kScratchSimd128Reg);
        __ vextractub(kScratchSimd128Reg, kScratchSimd128Reg, Operand(6));
        __ mfvsrd(i.OutputRegister(), kScratchSimd128Reg);
      }
      break;
    }
    case kPPC_I32x4BitMask: {
      if (CpuFeatures::IsSupported(PPC_10_PLUS)) {
        __ vextractwm(i.OutputRegister(), i.InputSimd128Register(0));
      } else {
        __ mov(kScratchReg,
               Operand(0x8080808000204060));  // Select 0 for the high bits.
        __ mtvsrd(kScratchSimd128Reg, kScratchReg);
        __ vbpermq(kScratchSimd128Reg, i.InputSimd128Register(0),
                   kScratchSimd128Reg);
        __ vextractub(kScratchSimd128Reg, kScratchSimd128Reg, Operand(6));
        __ mfvsrd(i.OutputRegister(), kScratchSimd128Reg);
      }
      break;
    }
    case kPPC_I16x8BitMask: {
      if (CpuFeatures::IsSupported(PPC_10_PLUS)) {
        __ vextracthm(i.OutputRegister(), i.InputSimd128Register(0));
      } else {
        __ mov(kScratchReg, Operand(0x10203040506070));
        __ mtvsrd(kScratchSimd128Reg, kScratchReg);
        __ vbpermq(kScratchSimd128Reg, i.InputSimd128Register(0),
                   kScratchSimd128Reg);
        __ vextractub(kScratchSimd128Reg, kScratchSimd128Reg, Operand(6));
        __ mfvsrd(i.OutputRegister(), kScratchSimd128Reg);
      }
      break;
    }
    case kPPC_I8x16BitMask: {
      if (CpuFeatures::IsSupported(PPC_10_PLUS)) {
        __ vextractbm(i.OutputRegister(), i.InputSimd128Register(0));
      } else {
        Register temp = i.ToRegister(instr->TempAt(0));
        __ mov(temp, Operand(0x8101820283038));
        __ mov(ip, Operand(0x4048505860687078));
        __ mtvsrdd(kScratchSimd128Reg, temp, ip);
        __ vbpermq(kScratchSimd128Reg, i.InputSimd128Register(0),
                   kScratchSimd128Reg);
        __ vextractuh(kScratchSimd128Reg, kScratchSimd128Reg, Operand(6));
        __ mfvsrd(i.OutputRegister(), kScratchSimd128Reg);
      }
      break;
    }
    case kPPC_I32x4DotI16x8S: {
      __ vxor(kScratchSimd128Reg, kScratchSimd128Reg, kScratchSimd128Reg);
      __ vmsumshm(i.OutputSimd128Register(), i.InputSimd128Register(0),
                  i.InputSimd128Register(1), kScratchSimd128Reg);
      break;
    }
    case kPPC_F32x4Pmin: {
      Simd128Register dst = i.OutputSimd128Register(),
                      src0 = i.InputSimd128Register(0),
                      src1 = i.InputSimd128Register(1);
      __ xvcmpgtsp(kScratchSimd128Reg, src0, src1);
      __ vsel(dst, src0, src1, kScratchSimd128Reg);
      break;
    }
    case kPPC_F32x4Pmax: {
      Simd128Register dst = i.OutputSimd128Register(),
                      src0 = i.InputSimd128Register(0),
                      src1 = i.InputSimd128Register(1);
      __ xvcmpgtsp(kScratchSimd128Reg, src1, src0);
      __ vsel(dst, src0, src1, kScratchSimd128Reg);
      break;
    }
    case kPPC_F64x2Pmin: {
      Simd128Register dst = i.OutputSimd128Register(),
                      src0 = i.InputSimd128Register(0),
                      src1 = i.InputSimd128Register(1);
      __ xvcmpgtdp(kScratchSimd128Reg, src0, src1);
      __ vsel(dst, src0, src1, kScratchSimd128Reg);
      break;
    }
    case kPPC_F64x2Pmax: {
      Simd128Register dst = i.OutputSimd128Register(),
                      src0 = i.InputSimd128Register(0),
                      src1 = i.InputSimd128Register(1);
      __ xvcmpgtdp(kScratchSimd128Reg, src1, src0);
      __ vsel(dst, src0, src1, kScratchSimd128Reg);
      break;
    }
#define ASSEMBLE_LOAD_TRANSFORM(scratch, load_instr) \
  AddressingMode mode = kMode_None;                  \
  MemOperand operand = i.MemoryOperand(&mode);       \
  DCHECK_EQ(mode, kMode_MRR);                        \
  __ load_instr(scratch, operand);
#if V8_TARGET_BIG_ENDIAN
#define MAYBE_REVERSE_BYTES(reg, instr) __ instr(reg, reg);
#else
#define MAYBE_REVERSE_BYTES(reg, instr)
#endif
    case kPPC_S128Load8Splat: {
      Simd128Register dst = i.OutputSimd128Register();
      ASSEMBLE_LOAD_TRANSFORM(kScratchSimd128Reg, lxsibzx)
      __ vspltb(dst, kScratchSimd128Reg, Operand(7));
      break;
    }
    case kPPC_S128Load16Splat: {
      Simd128Register dst = i.OutputSimd128Register();
      ASSEMBLE_LOAD_TRANSFORM(kScratchSimd128Reg, lxsihzx)
      MAYBE_REVERSE_BYTES(kScratchSimd128Reg, xxbrh)
      __ vsplth(dst, kScratchSimd128Reg, Operand(3));
      break;
    }
    case kPPC_S128Load32Splat: {
      Simd128Register dst = i.OutputSimd128Register();
      ASSEMBLE_LOAD_TRANSFORM(kScratchSimd128Reg, lxsiwzx)
      MAYBE_REVERSE_BYTES(kScratchSimd128Reg, xxbrw)
      __ vspltw(dst, kScratchSimd128Reg, Operand(1));
      break;
    }
    case kPPC_S128Load64Splat: {
      constexpr int lane_width_in_bytes = 8;
      Simd128Register dst = i.OutputSimd128Register();
      ASSEMBLE_LOAD_TRANSFORM(dst, lxsdx)
      MAYBE_REVERSE_BYTES(dst, xxbrd)
      __ vinsertd(dst, dst, Operand(1 * lane_width_in_bytes));
      break;
    }
    case kPPC_S128Load8x8S: {
      Simd128Register dst = i.OutputSimd128Register();
      ASSEMBLE_LOAD_TRANSFORM(kScratchSimd128Reg, lxsdx)
      MAYBE_REVERSE_BYTES(kScratchSimd128Reg, xxbrd)
      __ vupkhsb(dst, kScratchSimd128Reg);
      break;
    }
    case kPPC_S128Load8x8U: {
      Simd128Register dst = i.OutputSimd128Register();
      ASSEMBLE_LOAD_TRANSFORM(kScratchSimd128Reg, lxsdx)
      MAYBE_REVERSE_BYTES(kScratchSimd128Reg, xxbrd)
      __ vupkhsb(dst, kScratchSimd128Reg);
      // Zero extend.
      __ li(ip, Operand(0xFF));
      __ mtvsrd(kScratchSimd128Reg, ip);
      __ vsplth(kScratchSimd128Reg, kScratchSimd128Reg, Operand(3));
      __ vand(dst, kScratchSimd128Reg, dst);
      break;
    }
    case kPPC_S128Load16x4S: {
      Simd128Register dst = i.OutputSimd128Register();
      ASSEMBLE_LOAD_TRANSFORM(kScratchSimd128Reg, lxsdx)
      MAYBE_REVERSE_BYTES(kScratchSimd128Reg, xxbrd)
      __ vupkhsh(dst, kScratchSimd128Reg);
      break;
    }
    case kPPC_S128Load16x4U: {
      Simd128Register dst = i.OutputSimd128Register();
      ASSEMBLE_LOAD_TRANSFORM(kScratchSimd128Reg, lxsdx)
      MAYBE_REVERSE_BYTES(kScratchSimd128Reg, xxbrd)
      __ vupkhsh(dst, kScratchSimd128Reg);
      // Zero extend.
      __ mov(ip, Operand(0xFFFF));
      __ mtvsrd(kScratchSimd128Reg, ip);
      __ vspltw(kScratchSimd128Reg, kScratchSimd128Reg, Operand(1));
      __ vand(dst, kScratchSimd128Reg, dst);

      break;
    }
    case kPPC_S128Load32x2S: {
      Simd128Register dst = i.OutputSimd128Register();
      ASSEMBLE_LOAD_TRANSFORM(kScratchSimd128Reg, lxsdx)
      MAYBE_REVERSE_BYTES(kScratchSimd128Reg, xxbrd)
      __ vupkhsw(dst, kScratchSimd128Reg);
      break;
    }
    case kPPC_S128Load32x2U: {
      constexpr int lane_width_in_bytes = 8;
      Simd128Register dst = i.OutputSimd128Register();
      ASSEMBLE_LOAD_TRANSFORM(kScratchSimd128Reg, lxsdx)
      MAYBE_REVERSE_BYTES(kScratchSimd128Reg, xxbrd)
      __ vupkhsw(dst, kScratchSimd128Reg);
      // Zero extend.
      __ mov(ip, Operand(0xFFFFFFFF));
      __ mtvsrd(kScratchSimd128Reg, ip);
      __ vinsertd(kScratchSimd128Reg, kScratchSimd128Reg,
                  Operand(1 * lane_width_in_bytes));
      __ vand(dst, kScratchSimd128Reg, dst);
      break;
    }
    case kPPC_S128Load32Zero: {
      constexpr int lane_width_in_bytes = 4;
      Simd128Register dst = i.OutputSimd128Register();
      ASSEMBLE_LOAD_TRANSFORM(kScratchSimd128Reg, lxsiwzx)
      MAYBE_REVERSE_BYTES(kScratchSimd128Reg, xxbrw)
      __ vxor(dst, dst, dst);
      __ vinsertw(dst, kScratchSimd128Reg, Operand(3 * lane_width_in_bytes));
      break;
    }
    case kPPC_S128Load64Zero: {
      constexpr int lane_width_in_bytes = 8;
      Simd128Register dst = i.OutputSimd128Register();
      ASSEMBLE_LOAD_TRANSFORM(kScratchSimd128Reg, lxsdx)
      MAYBE_REVERSE_BYTES(kScratchSimd128Reg, xxbrd)
      __ vxor(dst, dst, dst);
      __ vinsertd(dst, kScratchSimd128Reg, Operand(1 * lane_width_in_bytes));
      break;
    }
#undef ASSEMBLE_LOAD_TRANSFORM
    case kPPC_S128Load8Lane: {
      Simd128Register dst = i.OutputSimd128Register();
      DCHECK_EQ(dst, i.InputSimd128Register(0));
      AddressingMode mode = kMode_None;
      size_t index = 1;
      MemOperand operand = i.MemoryOperand(&mode, &index);
      DCHECK_EQ(mode, kMode_MRR);
      __ lxsibzx(kScratchSimd128Reg, operand);
      __ vinsertb(dst, kScratchSimd128Reg, Operand(15 - i.InputUint8(3)));
      break;
    }
    case kPPC_S128Load16Lane: {
      Simd128Register dst = i.OutputSimd128Register();
      DCHECK_EQ(dst, i.InputSimd128Register(0));
      constexpr int lane_width_in_bytes = 2;
      AddressingMode mode = kMode_None;
      size_t index = 1;
      MemOperand operand = i.MemoryOperand(&mode, &index);
      DCHECK_EQ(mode, kMode_MRR);
      __ lxsihzx(kScratchSimd128Reg, operand);
      MAYBE_REVERSE_BYTES(kScratchSimd128Reg, xxbrh)
      __ vinserth(dst, kScratchSimd128Reg,
                  Operand((7 - i.InputUint8(3)) * lane_width_in_bytes));
      break;
    }
    case kPPC_S128Load32Lane: {
      Simd128Register dst = i.OutputSimd128Register();
      DCHECK_EQ(dst, i.InputSimd128Register(0));
      constexpr int lane_width_in_bytes = 4;
      AddressingMode mode = kMode_None;
      size_t index = 1;
      MemOperand operand = i.MemoryOperand(&mode, &index);
      DCHECK_EQ(mode, kMode_MRR);
      __ lxsiwzx(kScratchSimd128Reg, operand);
      MAYBE_REVERSE_BYTES(kScratchSimd128Reg, xxbrw)
      __ vinsertw(dst, kScratchSimd128Reg,
                  Operand((3 - i.InputUint8(3)) * lane_width_in_bytes));
      break;
    }
    case kPPC_S128Load64Lane: {
      Simd128Register dst = i.OutputSimd128Register();
      DCHECK_EQ(dst, i.InputSimd128Register(0));
      constexpr int lane_width_in_bytes = 8;
      AddressingMode mode = kMode_None;
      size_t index = 1;
      MemOperand operand = i.MemoryOperand(&mode, &index);
      DCHECK_EQ(mode, kMode_MRR);
      __ lxsdx(kScratchSimd128Reg, operand);
      MAYBE_REVERSE_BYTES(kScratchSimd128Reg, xxbrd)
      __ vinsertd(dst, kScratchSimd128Reg,
                  Operand((1 - i.InputUint8(3)) * lane_width_in_bytes));
      break;
    }
    case kPPC_S128Store8Lane: {
      AddressingMode mode = kMode_None;
      size_t index = 1;
      MemOperand operand = i.MemoryOperand(&mode, &index);
      DCHECK_EQ(mode, kMode_MRR);
      __ vextractub(kScratchSimd128Reg, i.InputSimd128Register(0),
                    Operand(15 - i.InputUint8(3)));
      __ stxsibx(kScratchSimd128Reg, operand);
      break;
    }
    case kPPC_S128Store16Lane: {
      AddressingMode mode = kMode_None;
      constexpr int lane_width_in_bytes = 2;
      size_t index = 1;
      MemOperand operand = i.MemoryOperand(&mode, &index);
      DCHECK_EQ(mode, kMode_MRR);
      __ vextractuh(kScratchSimd128Reg, i.InputSimd128Register(0),
                    Operand((7 - i.InputUint8(3)) * lane_width_in_bytes));
      MAYBE_REVERSE_BYTES(kScratchSimd128Reg, xxbrh)
      __ stxsihx(kScratchSimd128Reg, operand);
      break;
    }
    case kPPC_S128Store32Lane: {
      AddressingMode mode = kMode_None;
      constexpr int lane_width_in_bytes = 4;
      size_t index = 1;
      MemOperand operand = i.MemoryOperand(&mode, &index);
      DCHECK_EQ(mode, kMode_MRR);
      __ vextractuw(kScratchSimd128Reg, i.InputSimd128Register(0),
                    Operand((3 - i.InputUint8(3)) * lane_width_in_bytes));
      MAYBE_REVERSE_BYTES(kScratchSimd128Reg, xxbrw)
      __ stxsiwx(kScratchSimd128Reg, operand);
      break;
    }
    case kPPC_S128Store64Lane: {
      AddressingMode mode = kMode_None;
      constexpr int lane_width_in_bytes = 8;
      size_t index = 1;
      MemOperand operand = i.MemoryOperand(&mode, &index);
      DCHECK_EQ(mode, kMode_MRR);
      __ vextractd(kScratchSimd128Reg, i.InputSimd128Register(0),
                   Operand((1 - i.InputUint8(3)) * lane_width_in_bytes));
      MAYBE_REVERSE_BYTES(kScratchSimd128Reg, xxbrd)
      __ stxsdx(kScratchSimd128Reg, operand);
      break;
    }
#undef MAYBE_REVERSE_BYTES
#define EXT_ADD_PAIRWISE(mul_even, mul_odd, add)           \
  __ mul_even(tempFPReg1, src, kScratchSimd128Reg);        \
  __ mul_odd(kScratchSimd128Reg, src, kScratchSimd128Reg); \
  __ add(dst, tempFPReg1, kScratchSimd128Reg);
    case kPPC_I32x4ExtAddPairwiseI16x8S: {
      Simd128Register src = i.InputSimd128Register(0);
      Simd128Register dst = i.OutputSimd128Register();
      Simd128Register tempFPReg1 = i.ToSimd128Register(instr->TempAt(0));
      __ vspltish(kScratchSimd128Reg, Operand(1));
      EXT_ADD_PAIRWISE(vmulesh, vmulosh, vadduwm)
      break;
    }
    case kPPC_I32x4ExtAddPairwiseI16x8U: {
      Simd128Register src = i.InputSimd128Register(0);
      Simd128Register dst = i.OutputSimd128Register();
      Simd128Register tempFPReg1 = i.ToSimd128Register(instr->TempAt(0));
      __ vspltish(kScratchSimd128Reg, Operand(1));
      EXT_ADD_PAIRWISE(vmuleuh, vmulouh, vadduwm)
      break;
    }

    case kPPC_I16x8ExtAddPairwiseI8x16S: {
      Simd128Register src = i.InputSimd128Register(0);
      Simd128Register dst = i.OutputSimd128Register();
      Simd128Register tempFPReg1 = i.ToSimd128Register(instr->TempAt(0));
      __ xxspltib(kScratchSimd128Reg, Operand(1));
      EXT_ADD_PAIRWISE(vmulesb, vmulosb, vadduhm)
      break;
    }
    case kPPC_I16x8ExtAddPairwiseI8x16U: {
      Simd128Register src = i.InputSimd128Register(0);
      Simd128Register dst = i.OutputSimd128Register();
      Simd128Register tempFPReg1 = i.ToSimd128Register(instr->TempAt(0));
      __ xxspltib(kScratchSimd128Reg, Operand(1));
      EXT_ADD_PAIRWISE(vmuleub, vmuloub, vadduhm)
      break;
    }
#undef EXT_ADD_PAIRWISE
    case kPPC_I16x8Q15MulRSatS: {
      __ vxor(kScratchSimd128Reg, kScratchSimd128Reg, kScratchSimd128Reg);
      __ vmhraddshs(i.OutputSimd128Register(), i.InputSimd128Register(0),
                    i.InputSimd128Register(1), kScratchSimd128Reg);
      break;
    }
#define EXT_MUL(mul_even, mul_odd)                  \
  Simd128Register dst = i.OutputSimd128Register(),  \
                  src0 = i.InputSimd128Register(0), \
                  src1 = i.InputSimd128Register(1); \
  __ mul_even(dst, src0, src1);                     \
  __ mul_odd(kScratchSimd128Reg, src0, src1);
    case kPPC_I64x2ExtMulLowI32x4S: {
      constexpr int lane_width_in_bytes = 8;
      EXT_MUL(vmulesw, vmulosw)
      __ vextractd(dst, dst, Operand(1 * lane_width_in_bytes));
      __ vextractd(kScratchSimd128Reg, kScratchSimd128Reg,
                   Operand(1 * lane_width_in_bytes));
      __ vinsertd(dst, kScratchSimd128Reg, Operand(1 * lane_width_in_bytes));
      break;
    }
    case kPPC_I64x2ExtMulHighI32x4S: {
      constexpr int lane_width_in_bytes = 8;
      EXT_MUL(vmulesw, vmulosw)
      __ vinsertd(dst, kScratchSimd128Reg, Operand(1 * lane_width_in_bytes));
      break;
    }
    case kPPC_I64x2ExtMulLowI32x4U: {
      constexpr int lane_width_in_bytes = 8;
      EXT_MUL(vmuleuw, vmulouw)
      __ vextractd(dst, dst, Operand(1 * lane_width_in_bytes));
      __ vextractd(kScratchSimd128Reg, kScratchSimd128Reg,
                   Operand(1 * lane_width_in_bytes));
      __ vinsertd(dst, kScratchSimd128Reg, Operand(1 * lane_width_in_bytes));
      break;
    }
    case kPPC_I64x2ExtMulHighI32x4U: {
      constexpr int lane_width_in_bytes = 8;
      EXT_MUL(vmuleuw, vmulouw)
      __ vinsertd(dst, kScratchSimd128Reg, Operand(1 * lane_width_in_bytes));
      break;
    }
    case kPPC_I32x4ExtMulLowI16x8S: {
      EXT_MUL(vmulesh, vmulosh)
      __ vmrglw(dst, dst, kScratchSimd128Reg);
      break;
    }
    case kPPC_I32x4ExtMulHighI16x8S: {
      EXT_MUL(vmulesh, vmulosh)
      __ vmrghw(dst, dst, kScratchSimd128Reg);
      break;
    }
    case kPPC_I32x4ExtMulLowI16x8U: {
      EXT_MUL(vmuleuh, vmulouh)
      __ vmrglw(dst, dst, kScratchSimd128Reg);
      break;
    }
    case kPPC_I32x4ExtMulHighI16x8U: {
      EXT_MUL(vmuleuh, vmulouh)
      __ vmrghw(dst, dst, kScratchSimd128Reg);
      break;
    }
    case kPPC_I16x8ExtMulLowI8x16S: {
      EXT_MUL(vmulesb, vmulosb)
      __ vmrglh(dst, dst, kScratchSimd128Reg);
      break;
    }
    case kPPC_I16x8ExtMulHighI8x16S: {
      EXT_MUL(vmulesb, vmulosb)
      __ vmrghh(dst, dst, kScratchSimd128Reg);
      break;
    }
    case kPPC_I16x8ExtMulLowI8x16U: {
      EXT_MUL(vmuleub, vmuloub)
      __ vmrglh(dst, dst, kScratchSimd128Reg);
      break;
    }
    case kPPC_I16x8ExtMulHighI8x16U: {
      EXT_MUL(vmuleub, vmuloub)
      __ vmrghh(dst, dst, kScratchSimd128Reg);
      break;
    }
#undef EXT_MUL
    case kPPC_F64x2ConvertLowI32x4S: {
      __ vupklsw(kScratchSimd128Reg, i.InputSimd128Register(0));
      __ xvcvsxddp(i.OutputSimd128Register(), kScratchSimd128Reg);
      break;
    }
    case kPPC_F64x2ConvertLowI32x4U: {
      Simd128Register dst = i.OutputSimd128Register();
      constexpr int lane_width_in_bytes = 8;
      __ vupklsw(dst, i.InputSimd128Register(0));
      // Zero extend.
      __ mov(ip, Operand(0xFFFFFFFF));
      __ mtvsrd(kScratchSimd128Reg, ip);
      __ vinsertd(kScratchSimd128Reg, kScratchSimd128Reg,
                  Operand(1 * lane_width_in_bytes));
      __ vand(dst, kScratchSimd128Reg, dst);
      __ xvcvuxddp(dst, dst);
      break;
    }
    case kPPC_F64x2PromoteLowF32x4: {
      constexpr int lane_number = 8;
      Simd128Register src = i.InputSimd128Register(0);
      Simd128Register dst = i.OutputSimd128Register();
      __ vextractd(kScratchSimd128Reg, src, Operand(lane_number));
      __ vinsertw(kScratchSimd128Reg, kScratchSimd128Reg, Operand(lane_number));
      __ xvcvspdp(dst, kScratchSimd128Reg);
      break;
    }
    case kPPC_F32x4DemoteF64x2Zero: {
      constexpr int lane_number = 8;
      Simd128Register src = i.InputSimd128Register(0);
      Simd128Register dst = i.OutputSimd128Register();
      __ xvcvdpsp(kScratchSimd128Reg, src);
      __ vextractuw(dst, kScratchSimd128Reg, Operand(lane_number));
      __ vinsertw(kScratchSimd128Reg, dst, Operand(4));
      __ vxor(dst, dst, dst);
      __ vinsertd(dst, kScratchSimd128Reg, Operand(lane_number));
      break;
    }
    case kPPC_I32x4TruncSatF64x2SZero: {
      constexpr int lane_number = 8;
      Simd128Register src = i.InputSimd128Register(0);
      Simd128Register dst = i.OutputSimd128Register();
      // NaN to 0.
      __ vor(kScratchSimd128Reg, src, src);
      __ xvcmpeqdp(kScratchSimd128Reg, kScratchSimd128Reg, kScratchSimd128Reg);
      __ vand(kScratchSimd128Reg, src, kScratchSimd128Reg);
      __ xvcvdpsxws(kScratchSimd128Reg, kScratchSimd128Reg);
      __ vextractuw(dst, kScratchSimd128Reg, Operand(lane_number));
      __ vinsertw(kScratchSimd128Reg, dst, Operand(4));
      __ vxor(dst, dst, dst);
      __ vinsertd(dst, kScratchSimd128Reg, Operand(lane_number));
      break;
    }
    case kPPC_I32x4TruncSatF64x2UZero: {
      constexpr int lane_number = 8;
      Simd128Register src = i.InputSimd128Register(0);
      Simd128Register dst = i.OutputSimd128Register();
      __ xvcvdpuxws(kScratchSimd128Reg, src);
      __ vextractuw(dst, kScratchSimd128Reg, Operand(lane_number));
      __ vinsertw(kScratchSimd128Reg, dst, Operand(4));
      __ vxor(dst, dst, dst);
      __ vinsertd(dst, kScratchSimd128Reg, Operand(lane_number));
      break;
    }
    case kPPC_I8x16Popcnt: {
      __ vpopcntb(i.OutputSimd128Register(), i.InputSimd128Register(0));
      break;
    }
    case kPPC_StoreCompressTagged: {
      ASSEMBLE_STORE_INTEGER(StoreTaggedField, StoreTaggedField);
      break;
    }
    case kPPC_LoadDecompressTaggedSigned: {
      CHECK(instr->HasOutput());
      ASSEMBLE_LOAD_INTEGER(lwz, plwz, lwzx);
      break;
    }
    case kPPC_LoadDecompressTaggedPointer: {
      CHECK(instr->HasOutput());
      ASSEMBLE_LOAD_INTEGER(lwz, plwz, lwzx);
      __ add(i.OutputRegister(), i.OutputRegister(), kRootRegister);
      break;
    }
    case kPPC_LoadDecompressAnyTagged: {
      CHECK(instr->HasOutput());
      ASSEMBLE_LOAD_INTEGER(lwz, plwz, lwzx);
      __ add(i.OutputRegister(), i.OutputRegister(), kRootRegister);
      break;
    }
    default:
      UNREACHABLE();
  }
  return kSuccess;
}

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

void CodeGenerator::AssembleArchDeoptBranch(Instruction* instr,
                                            BranchInfo* branch) {
  AssembleArchBranch(instr, branch);
}

void CodeGenerator::AssembleArchJumpRegardlessOfAssemblyOrder(
    RpoNumber target) {
  __ b(GetLabel(target));
}

#if V8_ENABLE_WEBASSEMBLY
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
        __ LeaveFrame(StackFrame::WASM);
        auto call_descriptor = gen_->linkage()->GetIncomingDescriptor();
        int pop_count = static_cast<int>(call_descriptor->ParameterSlotCount());
        __ Drop(pop_count);
        __ Ret();
      } else {
        gen_->AssembleSourcePosition(instr_);
        // A direct call to a wasm runtime stub defined in this module.
        // Just encode the stub index. This will be patched when the code
        // is added to the native module and copied into wasm code space.
        __ Call(static_cast<Address>(trap_id), RelocInfo::WASM_STUB_CALL);
        ReferenceMap* reference_map =
            gen_->zone()->New<ReferenceMap>(gen_->zone());
        gen_->RecordSafepoint(reference_map);
        if (FLAG_debug_code) {
          __ stop();
        }
      }
    }

    Instruction* instr_;
    CodeGenerator* gen_;
  };
  auto ool = zone()->New<OutOfLineTrap>(this, instr);
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
#endif  // V8_ENABLE_WEBASSEMBLY

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

  if (CpuFeatures::IsSupported(PPC_7_PLUS)) {
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
  __ CmpU64(input, Operand(case_count), r0);
  __ bge(GetLabel(i.InputRpo(1)));
  __ mov_label_addr(kScratchReg, table);
  __ ShiftLeftU64(r0, input, Operand(kSystemPointerSizeLog2));
  __ LoadU64(kScratchReg, MemOperand(kScratchReg, r0));
  __ Jump(kScratchReg);
}

void CodeGenerator::AssembleArchSelect(Instruction* instr,
                                       FlagsCondition condition) {
  UNIMPLEMENTED();
}

void CodeGenerator::FinishFrame(Frame* frame) {
  auto call_descriptor = linkage()->GetIncomingDescriptor();
  const DoubleRegList double_saves = call_descriptor->CalleeSavedFPRegisters();

  // Save callee-saved Double registers.
  if (!double_saves.is_empty()) {
    frame->AlignSavedCalleeRegisterSlots();
    DCHECK_EQ(kNumCalleeSavedDoubles, double_saves.Count());
    frame->AllocateSavedCalleeRegisterSlots(kNumCalleeSavedDoubles *
                                            (kDoubleSize / kSystemPointerSize));
  }
  // Save callee-saved registers.
  const RegList saves =
      FLAG_enable_embedded_constant_pool
          ? call_descriptor->CalleeSavedRegisters() - kConstantPoolRegister
          : call_descriptor->CalleeSavedRegisters();
  if (!saves.is_empty()) {
    // register save area does not include the fp or constant pool pointer.
    const int num_saves =
        kNumCalleeSaved - 1 - (FLAG_enable_embedded_constant_pool ? 1 : 0);
    frame->AllocateSavedCalleeRegisterSlots(num_saves);
  }
}

void CodeGenerator::AssembleConstructFrame() {
  auto call_descriptor = linkage()->GetIncomingDescriptor();
  if (frame_access_state()->has_frame()) {
    if (call_descriptor->IsCFunctionCall()) {
#if V8_ENABLE_WEBASSEMBLY
      if (info()->GetOutputStackFrameType() == StackFrame::C_WASM_ENTRY) {
        __ StubPrologue(StackFrame::C_WASM_ENTRY);
        // Reserve stack space for saving the c_entry_fp later.
        __ addi(sp, sp, Operand(-kSystemPointerSize));
#else
      // For balance.
      if (false) {
#endif  // V8_ENABLE_WEBASSEMBLY
      } else {
        __ mflr(r0);
        if (FLAG_enable_embedded_constant_pool) {
          __ Push(r0, fp, kConstantPoolRegister);
          // Adjust FP to point to saved FP.
          __ SubS64(fp, sp,
                    Operand(StandardFrameConstants::kConstantPoolOffset), r0);
        } else {
          __ Push(r0, fp);
          __ mr(fp, sp);
        }
      }
    } else if (call_descriptor->IsJSFunctionCall()) {
      __ Prologue();
    } else {
      StackFrame::Type type = info()->GetOutputStackFrameType();
      // TODO(mbrandy): Detect cases where ip is the entrypoint (for
      // efficient initialization of the constant pool pointer register).
      __ StubPrologue(type);
#if V8_ENABLE_WEBASSEMBLY
      if (call_descriptor->IsWasmFunctionCall() ||
          call_descriptor->IsWasmImportWrapper() ||
          call_descriptor->IsWasmCapiFunction()) {
        __ Push(kWasmInstanceRegister);
      }
      if (call_descriptor->IsWasmCapiFunction()) {
        // Reserve space for saving the PC later.
        __ addi(sp, sp, Operand(-kSystemPointerSize));
      }
#endif  // V8_ENABLE_WEBASSEMBLY
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
    __ RecordComment("-- OSR entrypoint --");
    osr_pc_offset_ = __ pc_offset();
    required_slots -= osr_helper()->UnoptimizedFrameSlots();
  }

  const DoubleRegList saves_fp = call_descriptor->CalleeSavedFPRegisters();
  const RegList saves =
      FLAG_enable_embedded_constant_pool
          ? call_descriptor->CalleeSavedRegisters() - kConstantPoolRegister
          : call_descriptor->CalleeSavedRegisters();

  if (required_slots > 0) {
#if V8_ENABLE_WEBASSEMBLY
    if (info()->IsWasm() && required_slots * kSystemPointerSize > 4 * KB) {
      // For WebAssembly functions with big frames we have to do the stack
      // overflow check before we construct the frame. Otherwise we may not
      // have enough space on the stack to call the runtime for the stack
      // overflow.
      Label done;

      // If the frame is bigger than the stack, we throw the stack overflow
      // exception unconditionally. Thereby we can avoid the integer overflow
      // check in the condition code.
      if (required_slots * kSystemPointerSize < FLAG_stack_size * KB) {
        Register scratch = ip;
        __ LoadU64(
            scratch,
            FieldMemOperand(kWasmInstanceRegister,
                            WasmInstanceObject::kRealStackLimitAddressOffset),
            r0);
        __ LoadU64(scratch, MemOperand(scratch), r0);
        __ AddS64(scratch, scratch,
                  Operand(required_slots * kSystemPointerSize), r0);
        __ CmpU64(sp, scratch);
        __ bge(&done);
      }

      __ Call(wasm::WasmCode::kWasmStackOverflow, RelocInfo::WASM_STUB_CALL);
      // The call does not return, hence we can ignore any references and just
      // define an empty safepoint.
      ReferenceMap* reference_map = zone()->New<ReferenceMap>(zone());
      RecordSafepoint(reference_map);
      if (FLAG_debug_code) __ stop();

      __ bind(&done);
    }
#endif  // V8_ENABLE_WEBASSEMBLY

    // Skip callee-saved and return slots, which are pushed below.
    required_slots -= saves.Count();
    required_slots -= frame()->GetReturnSlotCount();
    required_slots -= (kDoubleSize / kSystemPointerSize) * saves_fp.Count();
    __ AddS64(sp, sp, Operand(-required_slots * kSystemPointerSize), r0);
  }

  // Save callee-saved Double registers.
  if (!saves_fp.is_empty()) {
    __ MultiPushDoubles(saves_fp);
    DCHECK_EQ(kNumCalleeSavedDoubles, saves_fp.Count());
  }

  // Save callee-saved registers.
  if (!saves.is_empty()) {
    __ MultiPush(saves);
    // register save area does not include the fp or constant pool pointer.
  }

  const int returns = frame()->GetReturnSlotCount();
  // Create space for returns.
  __ AllocateStackSpace(returns * kSystemPointerSize);
}

void CodeGenerator::AssembleReturn(InstructionOperand* additional_pop_count) {
  auto call_descriptor = linkage()->GetIncomingDescriptor();

  const int returns = frame()->GetReturnSlotCount();
  if (returns != 0) {
    // Create space for returns.
    __ AddS64(sp, sp, Operand(returns * kSystemPointerSize), r0);
  }

  // Restore registers.
  const RegList saves =
      FLAG_enable_embedded_constant_pool
          ? call_descriptor->CalleeSavedRegisters() - kConstantPoolRegister
          : call_descriptor->CalleeSavedRegisters();
  if (!saves.is_empty()) {
    __ MultiPop(saves);
  }

  // Restore double registers.
  const DoubleRegList double_saves = call_descriptor->CalleeSavedFPRegisters();
  if (!double_saves.is_empty()) {
    __ MultiPopDoubles(double_saves);
  }

  unwinding_info_writer_.MarkBlockWillExit();

  PPCOperandConverter g(this, nullptr);
  const int parameter_slots =
      static_cast<int>(call_descriptor->ParameterSlotCount());

  // {aditional_pop_count} is only greater than zero if {parameter_slots = 0}.
  // Check RawMachineAssembler::PopAndReturn.
  if (parameter_slots != 0) {
    if (additional_pop_count->IsImmediate()) {
      DCHECK_EQ(g.ToConstant(additional_pop_count).ToInt32(), 0);
    } else if (FLAG_debug_code) {
      __ cmpi(g.ToRegister(additional_pop_count), Operand(0));
      __ Assert(eq, AbortReason::kUnexpectedAdditionalPopValue);
    }
  }

  Register argc_reg = r6;
  // Functions with JS linkage have at least one parameter (the receiver).
  // If {parameter_slots} == 0, it means it is a builtin with
  // kDontAdaptArgumentsSentinel, which takes care of JS arguments popping
  // itself.
  const bool drop_jsargs = parameter_slots != 0 &&
                           frame_access_state()->has_frame() &&
                           call_descriptor->IsJSFunctionCall();

  if (call_descriptor->IsCFunctionCall()) {
    AssembleDeconstructFrame();
  } else if (frame_access_state()->has_frame()) {
    // Canonicalize JSFunction return sites for now unless they have an variable
    // number of stack slot pops
    if (additional_pop_count->IsImmediate() &&
        g.ToConstant(additional_pop_count).ToInt32() == 0) {
      if (return_label_.is_bound()) {
        __ b(&return_label_);
        return;
      } else {
        __ bind(&return_label_);
      }
    }
    if (drop_jsargs) {
      // Get the actual argument count.
      DCHECK(!call_descriptor->CalleeSavedRegisters().has(argc_reg));
      __ LoadU64(argc_reg, MemOperand(fp, StandardFrameConstants::kArgCOffset));
    }
    AssembleDeconstructFrame();
  }
  // Constant pool is unavailable since the frame has been destructed
  ConstantPoolUnavailableScope constant_pool_unavailable(tasm());
  if (drop_jsargs) {
    // We must pop all arguments from the stack (including the receiver).
    // The number of arguments without the receiver is
    // max(argc_reg, parameter_slots-1), and the receiver is added in
    // DropArguments().
    DCHECK(!call_descriptor->CalleeSavedRegisters().has(argc_reg));
    if (parameter_slots > 1) {
      Label skip;
      __ CmpS64(argc_reg, Operand(parameter_slots), r0);
      __ bgt(&skip);
      __ mov(argc_reg, Operand(parameter_slots));
      __ bind(&skip);
    }
    __ DropArguments(argc_reg, TurboAssembler::kCountIsInteger,
                     TurboAssembler::kCountIncludesReceiver);
  } else if (additional_pop_count->IsImmediate()) {
    int additional_count = g.ToConstant(additional_pop_count).ToInt32();
    __ Drop(parameter_slots + additional_count);
  } else if (parameter_slots == 0) {
    __ Drop(g.ToRegister(additional_pop_count));
  } else {
    // {additional_pop_count} is guaranteed to be zero if {parameter_slots !=
    // 0}. Check RawMachineAssembler::PopAndReturn.
    __ Drop(parameter_slots);
  }
  __ Ret();
}

void CodeGenerator::FinishCode() {}

void CodeGenerator::PrepareForDeoptimizationExits(
    ZoneDeque<DeoptimizationExit*>* exits) {
  int total_size = 0;
  for (DeoptimizationExit* exit : deoptimization_exits_) {
    total_size += (exit->kind() == DeoptimizeKind::kLazy)
                      ? Deoptimizer::kLazyDeoptExitSize
                      : Deoptimizer::kEagerDeoptExitSize;
  }

  __ CheckTrampolinePoolQuick(total_size);
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
      __ StoreU64(src, g.ToMemOperand(destination), r0);
    }
  } else if (source->IsStackSlot()) {
    DCHECK(destination->IsRegister() || destination->IsStackSlot());
    MemOperand src = g.ToMemOperand(source);
    if (destination->IsRegister()) {
      __ LoadU64(g.ToRegister(destination), src, r0);
    } else {
      Register temp = kScratchReg;
      __ LoadU64(temp, src, r0);
      __ StoreU64(temp, g.ToMemOperand(destination), r0);
    }
  } else if (source->IsConstant()) {
    Constant src = g.ToConstant(source);
    if (destination->IsRegister() || destination->IsStackSlot()) {
      Register dst =
          destination->IsRegister() ? g.ToRegister(destination) : kScratchReg;
      switch (src.type()) {
        case Constant::kInt32:
#if V8_ENABLE_WEBASSEMBLY && !V8_TARGET_ARCH_PPC64
          if (RelocInfo::IsWasmReference(src.rmode())) {
            __ mov(dst, Operand(src.ToInt32(), src.rmode()));
            break;
          }
#endif  // V8_ENABLE_WEBASSEMBLY && !V8_TARGET_ARCH_PPC64
          __ mov(dst, Operand(src.ToInt32()));
          break;
        case Constant::kInt64:
#if V8_ENABLE_WEBASSEMBLY && V8_TARGET_ARCH_PPC64
          if (RelocInfo::IsWasmReference(src.rmode())) {
            __ mov(dst, Operand(src.ToInt64(), src.rmode()));
            break;
          }
#endif  // V8_ENABLE_WEBASSEMBLY && V8_TARGET_ARCH_PPC64
          __ mov(dst, Operand(src.ToInt64()));
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
        case Constant::kCompressedHeapObject: {
          Handle<HeapObject> src_object = src.ToHeapObject();
          RootIndex index;
          if (IsMaterializableFromRoot(src_object, &index)) {
            __ LoadRoot(dst, index);
          } else {
            // TODO(v8:7703, jyan@ca.ibm.com): Turn into a
            // COMPRESSED_EMBEDDED_OBJECT when the constant pool entry size is
            // tagged size.
            __ Move(dst, src_object, RelocInfo::FULL_EMBEDDED_OBJECT);
          }
          break;
        }
        case Constant::kRpoNumber:
          UNREACHABLE();  // TODO(dcarney): loading RPO constants on PPC.
      }
      if (destination->IsStackSlot()) {
        __ StoreU64(dst, g.ToMemOperand(destination), r0);
      }
    } else {
      DoubleRegister dst = destination->IsFPRegister()
                               ? g.ToDoubleRegister(destination)
                               : kScratchDoubleReg;
      base::Double value;
#if V8_HOST_ARCH_IA32 || V8_HOST_ARCH_X64
      // casting double precision snan to single precision
      // converts it to qnan on ia32/x64
      if (src.type() == Constant::kFloat32) {
        uint32_t val = src.ToFloat32AsInt();
        if ((val & 0x7F800000) == 0x7F800000) {
          uint64_t dval = static_cast<uint64_t>(val);
          dval = ((dval & 0xC0000000) << 32) | ((dval & 0x40000000) << 31) |
                 ((dval & 0x40000000) << 30) | ((dval & 0x7FFFFFFF) << 29);
          value = base::Double(dval);
        } else {
          value = base::Double(static_cast<double>(src.ToFloat32()));
        }
      } else {
        value = base::Double(src.ToFloat64());
      }
#else
      value = src.type() == Constant::kFloat32
                  ? base::Double(static_cast<double>(src.ToFloat32()))
                  : base::Double(src.ToFloat64());
#endif
      __ LoadDoubleLiteral(dst, value, kScratchReg);
      if (destination->IsDoubleStackSlot()) {
        __ StoreF64(dst, g.ToMemOperand(destination), r0);
      } else if (destination->IsFloatStackSlot()) {
        __ StoreF32(dst, g.ToMemOperand(destination), r0);
      }
    }
  } else if (source->IsFPRegister()) {
    MachineRepresentation rep = LocationOperand::cast(source)->representation();
    if (rep == MachineRepresentation::kSimd128) {
      if (destination->IsSimd128Register()) {
        __ vor(g.ToSimd128Register(destination), g.ToSimd128Register(source),
               g.ToSimd128Register(source));
      } else {
        DCHECK(destination->IsSimd128StackSlot());
        MemOperand dst = g.ToMemOperand(destination);
        __ mov(ip, Operand(dst.offset()));
        __ StoreSimd128(g.ToSimd128Register(source), MemOperand(dst.ra(), ip));
      }
    } else {
      DoubleRegister src = g.ToDoubleRegister(source);
      if (destination->IsFPRegister()) {
        DoubleRegister dst = g.ToDoubleRegister(destination);
        __ Move(dst, src);
      } else {
        DCHECK(destination->IsFPStackSlot());
        LocationOperand* op = LocationOperand::cast(source);
        if (op->representation() == MachineRepresentation::kFloat64) {
          __ StoreF64(src, g.ToMemOperand(destination), r0);
        } else {
          __ StoreF32(src, g.ToMemOperand(destination), r0);
        }
      }
    }
  } else if (source->IsFPStackSlot()) {
    DCHECK(destination->IsFPRegister() || destination->IsFPStackSlot());
    MemOperand src = g.ToMemOperand(source);
    if (destination->IsFPRegister()) {
      LocationOperand* op = LocationOperand::cast(source);
      if (op->representation() == MachineRepresentation::kFloat64) {
        __ LoadF64(g.ToDoubleRegister(destination), src, r0);
      } else if (op->representation() == MachineRepresentation::kFloat32) {
        __ LoadF32(g.ToDoubleRegister(destination), src, r0);
      } else {
        DCHECK_EQ(MachineRepresentation::kSimd128, op->representation());
        MemOperand src = g.ToMemOperand(source);
        __ mov(ip, Operand(src.offset()));
        __ LoadSimd128(g.ToSimd128Register(destination),
                       MemOperand(src.ra(), ip));
      }
    } else {
      LocationOperand* op = LocationOperand::cast(source);
      DoubleRegister temp = kScratchDoubleReg;
      if (op->representation() == MachineRepresentation::kFloat64) {
        __ LoadF64(temp, src, r0);
        __ StoreF64(temp, g.ToMemOperand(destination), r0);
      } else if (op->representation() == MachineRepresentation::kFloat32) {
        __ LoadF32(temp, src, r0);
        __ StoreF32(temp, g.ToMemOperand(destination), r0);
      } else {
        DCHECK_EQ(MachineRepresentation::kSimd128, op->representation());
        // push v0, to be used as scratch
        __ addi(sp, sp, Operand(-kSimd128Size));
        __ StoreSimd128(v0, MemOperand(r0, sp));
        MemOperand src = g.ToMemOperand(source);
        MemOperand dst = g.ToMemOperand(destination);
        __ mov(ip, Operand(src.offset()));
        __ LoadSimd128(v0, MemOperand(src.ra(), ip));
        __ mov(ip, Operand(dst.offset()));
        __ StoreSimd128(v0, MemOperand(dst.ra(), ip));
        // restore v0
        __ LoadSimd128(v0, MemOperand(r0, sp));
        __ addi(sp, sp, Operand(kSimd128Size));
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
    Simd128Register src = g.ToSimd128Register(source);
    if (destination->IsSimd128Register()) {
      __ SwapSimd128(src, g.ToSimd128Register(destination), kScratchSimd128Reg);
    } else {
      DCHECK(destination->IsSimd128StackSlot());
      __ SwapSimd128(src, g.ToMemOperand(destination), kScratchSimd128Reg);
    }
  } else if (source->IsSimd128StackSlot()) {
    DCHECK(destination->IsSimd128StackSlot());
    __ SwapSimd128(g.ToMemOperand(source), g.ToMemOperand(destination),
                   kScratchSimd128Reg);

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
