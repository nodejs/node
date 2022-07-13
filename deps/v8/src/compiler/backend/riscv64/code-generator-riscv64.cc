// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/codegen/assembler-inl.h"
#include "src/codegen/callable.h"
#include "src/codegen/macro-assembler.h"
#include "src/codegen/optimized-compilation-info.h"
#include "src/codegen/riscv64/constants-riscv64.h"
#include "src/compiler/backend/code-generator-impl.h"
#include "src/compiler/backend/code-generator.h"
#include "src/compiler/backend/gap-resolver.h"
#include "src/compiler/node-matchers.h"
#include "src/compiler/osr.h"
#include "src/heap/memory-chunk.h"
#include "src/wasm/wasm-code-manager.h"

namespace v8 {
namespace internal {
namespace compiler {

#define __ tasm()->

// TODO(plind): consider renaming these macros.
#define TRACE_MSG(msg)                                                      \
  PrintF("code_gen: \'%s\' in function %s at line %d\n", msg, __FUNCTION__, \
         __LINE__)

#define TRACE_UNIMPL()                                            \
  PrintF("UNIMPLEMENTED code_generator_riscv64: %s at line %d\n", \
         __FUNCTION__, __LINE__)

// Adds RISC-V-specific methods to convert InstructionOperands.
class RiscvOperandConverter final : public InstructionOperandConverter {
 public:
  RiscvOperandConverter(CodeGenerator* gen, Instruction* instr)
      : InstructionOperandConverter(gen, instr) {}

  FloatRegister OutputSingleRegister(size_t index = 0) {
    return ToSingleRegister(instr_->OutputAt(index));
  }

  FloatRegister InputSingleRegister(size_t index) {
    return ToSingleRegister(instr_->InputAt(index));
  }

  FloatRegister ToSingleRegister(InstructionOperand* op) {
    // Single (Float) and Double register namespace is same on RISC-V,
    // both are typedefs of FPURegister.
    return ToDoubleRegister(op);
  }

  Register InputOrZeroRegister(size_t index) {
    if (instr_->InputAt(index)->IsImmediate()) {
      Constant constant = ToConstant(instr_->InputAt(index));
      switch (constant.type()) {
        case Constant::kInt32:
        case Constant::kInt64:
          DCHECK_EQ(0, InputInt32(index));
          break;
        case Constant::kFloat32:
          DCHECK_EQ(0, bit_cast<int32_t>(InputFloat32(index)));
          break;
        case Constant::kFloat64:
          DCHECK_EQ(0, bit_cast<int64_t>(InputDouble(index)));
          break;
        default:
          UNREACHABLE();
      }
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
      case Constant::kCompressedHeapObject:
      case Constant::kHeapObject:
        // TODO(plind): Maybe we should handle ExtRef & HeapObj here?
        //    maybe not done on arm due to const pool ??
        break;
      case Constant::kDelayedStringConstant:
        return Operand::EmbeddedStringConstant(
            constant.ToDelayedStringConstant());
      case Constant::kRpoNumber:
        UNREACHABLE();  // TODO(titzer): RPO immediates
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
      case kMode_Root:
        return MemOperand(kRootRegister, InputInt32(index));
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

class OutOfLineRecordWrite final : public OutOfLineCode {
 public:
  OutOfLineRecordWrite(CodeGenerator* gen, Register object, Register index,
                       Register value, Register scratch0, Register scratch1,
                       RecordWriteMode mode, StubCallMode stub_mode)
      : OutOfLineCode(gen),
        object_(object),
        index_(index),
        value_(value),
        scratch0_(scratch0),
        scratch1_(scratch1),
        mode_(mode),
        stub_mode_(stub_mode),
        must_save_lr_(!gen->frame_access_state()->has_frame()),
        zone_(gen->zone()) {
    DCHECK(!AreAliased(object, index, scratch0, scratch1));
    DCHECK(!AreAliased(value, index, scratch0, scratch1));
  }

  void Generate() final {
    if (COMPRESS_POINTERS_BOOL) {
      __ DecompressTaggedPointer(value_, value_);
    }
    __ CheckPageFlag(value_, scratch0_,
                     MemoryChunk::kPointersToHereAreInterestingMask, eq,
                     exit());
    __ Add64(scratch1_, object_, index_);
    RememberedSetAction const remembered_set_action =
        mode_ > RecordWriteMode::kValueIsMap ? RememberedSetAction::kEmit
                                             : RememberedSetAction::kOmit;
    SaveFPRegsMode const save_fp_mode = frame()->DidAllocateDoubleRegisters()
                                            ? SaveFPRegsMode::kSave
                                            : SaveFPRegsMode::kIgnore;
    if (must_save_lr_) {
      // We need to save and restore ra if the frame was elided.
      __ Push(ra);
    }
    if (mode_ == RecordWriteMode::kValueIsEphemeronKey) {
      __ CallEphemeronKeyBarrier(object_, scratch1_, save_fp_mode);
    } else if (stub_mode_ == StubCallMode::kCallWasmRuntimeStub) {
      // A direct call to a wasm runtime stub defined in this module.
      // Just encode the stub index. This will be patched when the code
      // is added to the native module and copied into wasm code space.
      __ CallRecordWriteStubSaveRegisters(object_, scratch1_,
                                          remembered_set_action, save_fp_mode,
                                          StubCallMode::kCallWasmRuntimeStub);
    } else {
      __ CallRecordWriteStubSaveRegisters(object_, scratch1_,
                                          remembered_set_action, save_fp_mode);
    }
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
  StubCallMode const stub_mode_;
  bool must_save_lr_;
  Zone* zone_;
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
      return Uless;
    case kUnsignedGreaterThanOrEqual:
      return Ugreater_equal;
    case kUnsignedLessThanOrEqual:
      return Uless_equal;
    case kUnsignedGreaterThan:
      return Ugreater;
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

FPUCondition FlagsConditionToConditionCmpFPU(bool* predicate,
                                             FlagsCondition condition) {
  switch (condition) {
    case kEqual:
      *predicate = true;
      return EQ;
    case kNotEqual:
      *predicate = false;
      return EQ;
    case kUnsignedLessThan:
      *predicate = true;
      return LT;
    case kUnsignedGreaterThanOrEqual:
      *predicate = false;
      return LT;
    case kUnsignedLessThanOrEqual:
      *predicate = true;
      return LE;
    case kUnsignedGreaterThan:
      *predicate = false;
      return LE;
    case kUnorderedEqual:
    case kUnorderedNotEqual:
      *predicate = true;
      break;
    default:
      *predicate = true;
      break;
  }
  UNREACHABLE();
}

}  // namespace

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

#define ASSEMBLE_ATOMIC_BINOP(load_linked, store_conditional, bin_instr)       \
  do {                                                                         \
    Label binop;                                                               \
    __ Add64(i.TempRegister(0), i.InputRegister(0), i.InputRegister(1));       \
    __ sync();                                                                 \
    __ bind(&binop);                                                           \
    __ load_linked(i.OutputRegister(0), MemOperand(i.TempRegister(0), 0));     \
    __ bin_instr(i.TempRegister(1), i.OutputRegister(0),                       \
                 Operand(i.InputRegister(2)));                                 \
    __ store_conditional(i.TempRegister(1), MemOperand(i.TempRegister(0), 0)); \
    __ BranchShort(&binop, ne, i.TempRegister(1), Operand(zero_reg));          \
    __ sync();                                                                 \
  } while (0)

#define ASSEMBLE_ATOMIC_BINOP_EXT(load_linked, store_conditional, sign_extend, \
                                  size, bin_instr, representation)             \
  do {                                                                         \
    Label binop;                                                               \
    __ Add64(i.TempRegister(0), i.InputRegister(0), i.InputRegister(1));       \
    if (representation == 32) {                                                \
      __ And(i.TempRegister(3), i.TempRegister(0), 0x3);                       \
    } else {                                                                   \
      DCHECK_EQ(representation, 64);                                           \
      __ And(i.TempRegister(3), i.TempRegister(0), 0x7);                       \
    }                                                                          \
    __ Sub64(i.TempRegister(0), i.TempRegister(0),                             \
             Operand(i.TempRegister(3)));                                      \
    __ Sll32(i.TempRegister(3), i.TempRegister(3), 3);                         \
    __ sync();                                                                 \
    __ bind(&binop);                                                           \
    __ load_linked(i.TempRegister(1), MemOperand(i.TempRegister(0), 0));       \
    __ ExtractBits(i.OutputRegister(0), i.TempRegister(1), i.TempRegister(3),  \
                   size, sign_extend);                                         \
    __ bin_instr(i.TempRegister(2), i.OutputRegister(0),                       \
                 Operand(i.InputRegister(2)));                                 \
    __ InsertBits(i.TempRegister(1), i.TempRegister(2), i.TempRegister(3),     \
                  size);                                                       \
    __ store_conditional(i.TempRegister(1), MemOperand(i.TempRegister(0), 0)); \
    __ BranchShort(&binop, ne, i.TempRegister(1), Operand(zero_reg));          \
    __ sync();                                                                 \
  } while (0)

#define ASSEMBLE_ATOMIC_EXCHANGE_INTEGER(load_linked, store_conditional)       \
  do {                                                                         \
    Label exchange;                                                            \
    __ sync();                                                                 \
    __ bind(&exchange);                                                        \
    __ Add64(i.TempRegister(0), i.InputRegister(0), i.InputRegister(1));       \
    __ load_linked(i.OutputRegister(0), MemOperand(i.TempRegister(0), 0));     \
    __ Move(i.TempRegister(1), i.InputRegister(2));                            \
    __ store_conditional(i.TempRegister(1), MemOperand(i.TempRegister(0), 0)); \
    __ BranchShort(&exchange, ne, i.TempRegister(1), Operand(zero_reg));       \
    __ sync();                                                                 \
  } while (0)

#define ASSEMBLE_ATOMIC_EXCHANGE_INTEGER_EXT(                                  \
    load_linked, store_conditional, sign_extend, size, representation)         \
  do {                                                                         \
    Label exchange;                                                            \
    __ Add64(i.TempRegister(0), i.InputRegister(0), i.InputRegister(1));       \
    if (representation == 32) {                                                \
      __ And(i.TempRegister(1), i.TempRegister(0), 0x3);                       \
    } else {                                                                   \
      DCHECK_EQ(representation, 64);                                           \
      __ And(i.TempRegister(1), i.TempRegister(0), 0x7);                       \
    }                                                                          \
    __ Sub64(i.TempRegister(0), i.TempRegister(0),                             \
             Operand(i.TempRegister(1)));                                      \
    __ Sll32(i.TempRegister(1), i.TempRegister(1), 3);                         \
    __ sync();                                                                 \
    __ bind(&exchange);                                                        \
    __ load_linked(i.TempRegister(2), MemOperand(i.TempRegister(0), 0));       \
    __ ExtractBits(i.OutputRegister(0), i.TempRegister(2), i.TempRegister(1),  \
                   size, sign_extend);                                         \
    __ InsertBits(i.TempRegister(2), i.InputRegister(2), i.TempRegister(1),    \
                  size);                                                       \
    __ store_conditional(i.TempRegister(2), MemOperand(i.TempRegister(0), 0)); \
    __ BranchShort(&exchange, ne, i.TempRegister(2), Operand(zero_reg));       \
    __ sync();                                                                 \
  } while (0)

#define ASSEMBLE_ATOMIC_COMPARE_EXCHANGE_INTEGER(load_linked,                  \
                                                 store_conditional)            \
  do {                                                                         \
    Label compareExchange;                                                     \
    Label exit;                                                                \
    __ Add64(i.TempRegister(0), i.InputRegister(0), i.InputRegister(1));       \
    __ sync();                                                                 \
    __ bind(&compareExchange);                                                 \
    __ load_linked(i.OutputRegister(0), MemOperand(i.TempRegister(0), 0));     \
    __ BranchShort(&exit, ne, i.InputRegister(2),                              \
                   Operand(i.OutputRegister(0)));                              \
    __ Move(i.TempRegister(2), i.InputRegister(3));                            \
    __ store_conditional(i.TempRegister(2), MemOperand(i.TempRegister(0), 0)); \
    __ BranchShort(&compareExchange, ne, i.TempRegister(2),                    \
                   Operand(zero_reg));                                         \
    __ bind(&exit);                                                            \
    __ sync();                                                                 \
  } while (0)

#define ASSEMBLE_ATOMIC_COMPARE_EXCHANGE_INTEGER_EXT(                          \
    load_linked, store_conditional, sign_extend, size, representation)         \
  do {                                                                         \
    Label compareExchange;                                                     \
    Label exit;                                                                \
    __ Add64(i.TempRegister(0), i.InputRegister(0), i.InputRegister(1));       \
    if (representation == 32) {                                                \
      __ And(i.TempRegister(1), i.TempRegister(0), 0x3);                       \
    } else {                                                                   \
      DCHECK_EQ(representation, 64);                                           \
      __ And(i.TempRegister(1), i.TempRegister(0), 0x7);                       \
    }                                                                          \
    __ Sub64(i.TempRegister(0), i.TempRegister(0),                             \
             Operand(i.TempRegister(1)));                                      \
    __ Sll32(i.TempRegister(1), i.TempRegister(1), 3);                         \
    __ sync();                                                                 \
    __ bind(&compareExchange);                                                 \
    __ load_linked(i.TempRegister(2), MemOperand(i.TempRegister(0), 0));       \
    __ ExtractBits(i.OutputRegister(0), i.TempRegister(2), i.TempRegister(1),  \
                   size, sign_extend);                                         \
    __ ExtractBits(i.InputRegister(2), i.InputRegister(2), 0, size,            \
                   sign_extend);                                               \
    __ BranchShort(&exit, ne, i.InputRegister(2),                              \
                   Operand(i.OutputRegister(0)));                              \
    __ InsertBits(i.TempRegister(2), i.InputRegister(3), i.TempRegister(1),    \
                  size);                                                       \
    __ store_conditional(i.TempRegister(2), MemOperand(i.TempRegister(0), 0)); \
    __ BranchShort(&compareExchange, ne, i.TempRegister(2),                    \
                   Operand(zero_reg));                                         \
    __ bind(&exit);                                                            \
    __ sync();                                                                 \
  } while (0)

#define ASSEMBLE_IEEE754_BINOP(name)                                        \
  do {                                                                      \
    FrameScope scope(tasm(), StackFrame::MANUAL);                           \
    __ PrepareCallCFunction(0, 2, kScratchReg);                             \
    __ MovToFloatParameters(i.InputDoubleRegister(0),                       \
                            i.InputDoubleRegister(1));                      \
    __ CallCFunction(ExternalReference::ieee754_##name##_function(), 0, 2); \
    /* Move the result in the double result register. */                    \
    __ MovFromFloatResult(i.OutputDoubleRegister());                        \
  } while (0)

#define ASSEMBLE_IEEE754_UNOP(name)                                         \
  do {                                                                      \
    FrameScope scope(tasm(), StackFrame::MANUAL);                           \
    __ PrepareCallCFunction(0, 1, kScratchReg);                             \
    __ MovToFloatParameter(i.InputDoubleRegister(0));                       \
    __ CallCFunction(ExternalReference::ieee754_##name##_function(), 0, 1); \
    /* Move the result in the double result register. */                    \
    __ MovFromFloatResult(i.OutputDoubleRegister());                        \
  } while (0)

#define ASSEMBLE_F64X2_ARITHMETIC_BINOP(op)                     \
  do {                                                          \
    __ op(i.OutputSimd128Register(), i.InputSimd128Register(0), \
          i.InputSimd128Register(1));                           \
  } while (0)

#define ASSEMBLE_RVV_BINOP_INTEGER(instr, OP)                   \
  case kRiscvI8x16##instr: {                                    \
    __ VU.set(kScratchReg, E8, m1);                             \
    __ OP(i.OutputSimd128Register(), i.InputSimd128Register(0), \
          i.InputSimd128Register(1));                           \
    break;                                                      \
  }                                                             \
  case kRiscvI16x8##instr: {                                    \
    __ VU.set(kScratchReg, E16, m1);                            \
    __ OP(i.OutputSimd128Register(), i.InputSimd128Register(0), \
          i.InputSimd128Register(1));                           \
    break;                                                      \
  }                                                             \
  case kRiscvI32x4##instr: {                                    \
    __ VU.set(kScratchReg, E32, m1);                            \
    __ OP(i.OutputSimd128Register(), i.InputSimd128Register(0), \
          i.InputSimd128Register(1));                           \
    break;                                                      \
  }

#define ASSEMBLE_RVV_UNOP_INTEGER_VR(instr, OP)           \
  case kRiscvI8x16##instr: {                              \
    __ VU.set(kScratchReg, E8, m1);                       \
    __ OP(i.OutputSimd128Register(), i.InputRegister(0)); \
    break;                                                \
  }                                                       \
  case kRiscvI16x8##instr: {                              \
    __ VU.set(kScratchReg, E16, m1);                      \
    __ OP(i.OutputSimd128Register(), i.InputRegister(0)); \
    break;                                                \
  }                                                       \
  case kRiscvI32x4##instr: {                              \
    __ VU.set(kScratchReg, E32, m1);                      \
    __ OP(i.OutputSimd128Register(), i.InputRegister(0)); \
    break;                                                \
  }                                                       \
  case kRiscvI64x2##instr: {                              \
    __ VU.set(kScratchReg, E64, m1);                      \
    __ OP(i.OutputSimd128Register(), i.InputRegister(0)); \
    break;                                                \
  }

#define ASSEMBLE_RVV_UNOP_INTEGER_VV(instr, OP)                  \
  case kRiscvI8x16##instr: {                                     \
    __ VU.set(kScratchReg, E8, m1);                              \
    __ OP(i.OutputSimd128Register(), i.InputSimd128Register(0)); \
    break;                                                       \
  }                                                              \
  case kRiscvI16x8##instr: {                                     \
    __ VU.set(kScratchReg, E16, m1);                             \
    __ OP(i.OutputSimd128Register(), i.InputSimd128Register(0)); \
    break;                                                       \
  }                                                              \
  case kRiscvI32x4##instr: {                                     \
    __ VU.set(kScratchReg, E32, m1);                             \
    __ OP(i.OutputSimd128Register(), i.InputSimd128Register(0)); \
    break;                                                       \
  }                                                              \
  case kRiscvI64x2##instr: {                                     \
    __ VU.set(kScratchReg, E64, m1);                             \
    __ OP(i.OutputSimd128Register(), i.InputSimd128Register(0)); \
    break;                                                       \
  }

void CodeGenerator::AssembleDeconstructFrame() {
  __ Move(sp, fp);
  __ Pop(ra, fp);
}

void CodeGenerator::AssemblePrepareTailCall() {
  if (frame_access_state()->has_frame()) {
    __ Ld(ra, MemOperand(fp, StandardFrameConstants::kCallerPCOffset));
    __ Ld(fp, MemOperand(fp, StandardFrameConstants::kCallerFPOffset));
  }
  frame_access_state()->SetFrameAccessToSP();
}

void CodeGenerator::AssembleArchSelect(Instruction* instr,
                                       FlagsCondition condition) {
  UNIMPLEMENTED();
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
    tasm->Sub64(sp, sp, stack_slot_delta * kSystemPointerSize);
    state->IncreaseSPDelta(stack_slot_delta);
  } else if (allow_shrinkage && stack_slot_delta < 0) {
    tasm->Add64(sp, sp, -stack_slot_delta * kSystemPointerSize);
    state->IncreaseSPDelta(stack_slot_delta);
  }
}

}  // namespace

void CodeGenerator::AssembleTailCallBeforeGap(Instruction* instr,
                                              int first_unused_slot_offset) {
  AdjustStackPointerForTailCall(tasm(), frame_access_state(),
                                first_unused_slot_offset, false);
}

void CodeGenerator::AssembleTailCallAfterGap(Instruction* instr,
                                             int first_unused_slot_offset) {
  AdjustStackPointerForTailCall(tasm(), frame_access_state(),
                                first_unused_slot_offset);
}

// Check that {kJavaScriptCallCodeStartRegister} is correct.
void CodeGenerator::AssembleCodeStartRegisterCheck() {
  __ ComputeCodeStartAddress(kScratchReg);
  __ Assert(eq, AbortReason::kWrongFunctionCodeStart,
            kJavaScriptCallCodeStartRegister, Operand(kScratchReg));
}

// Check if the code object is marked for deoptimization. If it is, then it
// jumps to the CompileLazyDeoptimizedCode builtin. In order to do this we need
// to:
//    1. read from memory the word that contains that bit, which can be found in
//       the flags in the referenced {CodeDataContainer} object;
//    2. test kMarkedForDeoptimizationBit in those flags; and
//    3. if it is not zero then it jumps to the builtin.
void CodeGenerator::BailoutIfDeoptimized() {
  int offset = Code::kCodeDataContainerOffset - Code::kHeaderSize;
  __ LoadTaggedPointerField(
      kScratchReg, MemOperand(kJavaScriptCallCodeStartRegister, offset));
  __ Lw(kScratchReg,
        FieldMemOperand(kScratchReg,
                        CodeDataContainer::kKindSpecificFlagsOffset));
  __ And(kScratchReg, kScratchReg,
         Operand(1 << Code::kMarkedForDeoptimizationBit));
  __ Jump(BUILTIN_CODE(isolate(), CompileLazyDeoptimizedCode),
          RelocInfo::CODE_TARGET, ne, kScratchReg, Operand(zero_reg));
}

// Assembles an instruction after register allocation, producing machine code.
CodeGenerator::CodeGenResult CodeGenerator::AssembleArchInstruction(
    Instruction* instr) {
  RiscvOperandConverter i(this, instr);
  InstructionCode opcode = instr->opcode();
  ArchOpcode arch_opcode = ArchOpcodeField::decode(opcode);
  switch (arch_opcode) {
    case kArchCallCodeObject: {
      if (instr->InputAt(0)->IsImmediate()) {
        __ Call(i.InputCode(0), RelocInfo::CODE_TARGET);
      } else {
        Register reg = i.InputRegister(0);
        DCHECK_IMPLIES(
            instr->HasCallDescriptorFlag(CallDescriptor::kFixedTargetRegister),
            reg == kJavaScriptCallCodeStartRegister);
        __ CallCodeObject(reg);
      }
      RecordCallPosition(instr);
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
      if (instr->InputAt(0)->IsImmediate()) {
        Constant constant = i.ToConstant(instr->InputAt(0));
        Address wasm_code = static_cast<Address>(constant.ToInt64());
        __ Call(wasm_code, constant.rmode());
      } else {
        __ Add64(t6, i.InputOrZeroRegister(0), 0);
        __ Call(t6);
      }
      RecordCallPosition(instr);
      frame_access_state()->ClearSPDelta();
      break;
    }
    case kArchTailCallCodeObject: {
      if (instr->InputAt(0)->IsImmediate()) {
        __ Jump(i.InputCode(0), RelocInfo::CODE_TARGET);
      } else {
        Register reg = i.InputOrZeroRegister(0);
        DCHECK_IMPLIES(
            instr->HasCallDescriptorFlag(CallDescriptor::kFixedTargetRegister),
            reg == kJavaScriptCallCodeStartRegister);
        __ JumpCodeObject(reg);
      }
      frame_access_state()->ClearSPDelta();
      frame_access_state()->SetFrameAccessToDefault();
      break;
    }
    case kArchTailCallWasm: {
      if (instr->InputAt(0)->IsImmediate()) {
        Constant constant = i.ToConstant(instr->InputAt(0));
        Address wasm_code = static_cast<Address>(constant.ToInt64());
        __ Jump(wasm_code, constant.rmode());
      } else {
        __ Add64(kScratchReg, i.InputOrZeroRegister(0), 0);
        __ Jump(kScratchReg);
      }
      frame_access_state()->ClearSPDelta();
      frame_access_state()->SetFrameAccessToDefault();
      break;
    }
    case kArchTailCallAddress: {
      CHECK(!instr->InputAt(0)->IsImmediate());
      Register reg = i.InputOrZeroRegister(0);
      DCHECK_IMPLIES(
          instr->HasCallDescriptorFlag(CallDescriptor::kFixedTargetRegister),
          reg == kJavaScriptCallCodeStartRegister);
      __ Jump(reg);
      frame_access_state()->ClearSPDelta();
      frame_access_state()->SetFrameAccessToDefault();
      break;
    }
    case kArchCallJSFunction: {
      Register func = i.InputOrZeroRegister(0);
      if (FLAG_debug_code) {
        // Check the function's context matches the context argument.
        __ LoadTaggedPointerField(
            kScratchReg, FieldMemOperand(func, JSFunction::kContextOffset));
        __ Assert(eq, AbortReason::kWrongFunctionContext, cp,
                  Operand(kScratchReg));
      }
      static_assert(kJavaScriptCallCodeStartRegister == a2, "ABI mismatch");
      __ LoadTaggedPointerField(a2,
                                FieldMemOperand(func, JSFunction::kCodeOffset));
      __ CallCodeObject(a2);
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
    case kArchCallCFunction: {
      int const num_gp_parameters = ParamField::decode(instr->opcode());
      int const num_fp_parameters = FPParamField::decode(instr->opcode());
      Label after_call;
      bool isWasmCapiFunction =
          linkage()->GetIncomingDescriptor()->IsWasmCapiFunction();
      if (isWasmCapiFunction) {
        // Put the return address in a stack slot.
        __ LoadAddress(kScratchReg, &after_call, RelocInfo::EXTERNAL_REFERENCE);
        __ Sd(kScratchReg,
              MemOperand(fp, WasmExitFrameConstants::kCallingPCOffset));
      }
      if (instr->InputAt(0)->IsImmediate()) {
        ExternalReference ref = i.InputExternalReference(0);
        __ CallCFunction(ref, num_gp_parameters, num_fp_parameters);
      } else {
        Register func = i.InputOrZeroRegister(0);
        __ CallCFunction(func, num_gp_parameters, num_fp_parameters);
      }
      __ bind(&after_call);
      if (isWasmCapiFunction) {
        RecordSafepoint(instr->reference_map());
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
    case kArchTableSwitch:
      AssembleArchTableSwitch(instr);
      break;
    case kArchAbortCSADcheck:
      DCHECK(i.InputRegister(0) == a0);
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
    case kArchComment:
      __ RecordComment(reinterpret_cast<const char*>(i.InputInt64(0)));
      break;
    case kArchNop:
    case kArchThrowTerminator:
      // don't emit code for nops.
      break;
    case kArchDeoptimize: {
      DeoptimizationExit* exit =
          BuildTranslation(instr, -1, 0, 0, OutputFrameStateCombine::Ignore());
      __ Branch(exit->label());
      break;
    }
    case kArchRet:
      AssembleReturn(instr->InputAt(0));
      break;
    case kArchStackPointerGreaterThan:
      // Pseudo-instruction used for cmp/branch. No opcode emitted here.
      break;
    case kArchStackCheckOffset:
      __ Move(i.OutputRegister(), Smi::FromInt(GetStackCheckOffset()));
      break;
    case kArchFramePointer:
      __ Move(i.OutputRegister(), fp);
      break;
    case kArchParentFramePointer:
      if (frame_access_state()->has_frame()) {
        __ Ld(i.OutputRegister(), MemOperand(fp, 0));
      } else {
        __ Move(i.OutputRegister(), fp);
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
      Register index = i.InputRegister(1);
      Register value = i.InputRegister(2);
      Register scratch0 = i.TempRegister(0);
      Register scratch1 = i.TempRegister(1);
      auto ool = zone()->New<OutOfLineRecordWrite>(this, object, index, value,
                                                   scratch0, scratch1, mode,
                                                   DetermineStubCallMode());
      __ Add64(kScratchReg, object, index);
      __ StoreTaggedField(value, MemOperand(kScratchReg));
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
      Register base_reg = offset.from_stack_pointer() ? sp : fp;
      __ Add64(i.OutputRegister(), base_reg, Operand(offset.offset()));
      int alignment = i.InputInt32(1);
      DCHECK(alignment == 0 || alignment == 4 || alignment == 8 ||
             alignment == 16);
      if (FLAG_debug_code && alignment > 0) {
        // Verify that the output_register is properly aligned
        __ And(kScratchReg, i.OutputRegister(),
               Operand(kSystemPointerSize - 1));
        __ Assert(eq, AbortReason::kAllocationIsNotDoubleAligned, kScratchReg,
                  Operand(zero_reg));
      }
      if (alignment == 2 * kSystemPointerSize) {
        Label done;
        __ Add64(kScratchReg, base_reg, Operand(offset.offset()));
        __ And(kScratchReg, kScratchReg, Operand(alignment - 1));
        __ BranchShort(&done, eq, kScratchReg, Operand(zero_reg));
        __ Add64(i.OutputRegister(), i.OutputRegister(), kSystemPointerSize);
        __ bind(&done);
      } else if (alignment > 2 * kSystemPointerSize) {
        Label done;
        __ Add64(kScratchReg, base_reg, Operand(offset.offset()));
        __ And(kScratchReg, kScratchReg, Operand(alignment - 1));
        __ BranchShort(&done, eq, kScratchReg, Operand(zero_reg));
        __ li(kScratchReg2, alignment);
        __ Sub64(kScratchReg2, kScratchReg2, Operand(kScratchReg));
        __ Add64(i.OutputRegister(), i.OutputRegister(), kScratchReg2);
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
    case kIeee754Float64Pow:
      ASSEMBLE_IEEE754_BINOP(pow);
      break;
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
    case kRiscvAdd32:
      __ Add32(i.OutputRegister(), i.InputOrZeroRegister(0), i.InputOperand(1));
      break;
    case kRiscvAdd64:
      __ Add64(i.OutputRegister(), i.InputOrZeroRegister(0), i.InputOperand(1));
      break;
    case kRiscvAddOvf64:
      __ AddOverflow64(i.OutputRegister(), i.InputOrZeroRegister(0),
                       i.InputOperand(1), kScratchReg);
      break;
    case kRiscvSub32:
      __ Sub32(i.OutputRegister(), i.InputOrZeroRegister(0), i.InputOperand(1));
      break;
    case kRiscvSub64:
      __ Sub64(i.OutputRegister(), i.InputOrZeroRegister(0), i.InputOperand(1));
      break;
    case kRiscvSubOvf64:
      __ SubOverflow64(i.OutputRegister(), i.InputOrZeroRegister(0),
                       i.InputOperand(1), kScratchReg);
      break;
    case kRiscvMul32:
      __ Mul32(i.OutputRegister(), i.InputOrZeroRegister(0), i.InputOperand(1));
      break;
    case kRiscvMulOvf32:
      __ MulOverflow32(i.OutputRegister(), i.InputOrZeroRegister(0),
                       i.InputOperand(1), kScratchReg);
      break;
    case kRiscvMulHigh32:
      __ Mulh32(i.OutputRegister(), i.InputOrZeroRegister(0),
                i.InputOperand(1));
      break;
    case kRiscvMulHighU32:
      __ Mulhu32(i.OutputRegister(), i.InputOrZeroRegister(0),
                 i.InputOperand(1), kScratchReg, kScratchReg2);
      break;
    case kRiscvMulHigh64:
      __ Mulh64(i.OutputRegister(), i.InputOrZeroRegister(0),
                i.InputOperand(1));
      break;
    case kRiscvDiv32: {
      __ Div32(i.OutputRegister(), i.InputOrZeroRegister(0), i.InputOperand(1));
      // Set ouput to zero if divisor == 0
      __ LoadZeroIfConditionZero(i.OutputRegister(), i.InputRegister(1));
      break;
    }
    case kRiscvDivU32: {
      __ Divu32(i.OutputRegister(), i.InputOrZeroRegister(0),
                i.InputOperand(1));
      // Set ouput to zero if divisor == 0
      __ LoadZeroIfConditionZero(i.OutputRegister(), i.InputRegister(1));
      break;
    }
    case kRiscvMod32:
      __ Mod32(i.OutputRegister(), i.InputOrZeroRegister(0), i.InputOperand(1));
      break;
    case kRiscvModU32:
      __ Modu32(i.OutputRegister(), i.InputOrZeroRegister(0),
                i.InputOperand(1));
      break;
    case kRiscvMul64:
      __ Mul64(i.OutputRegister(), i.InputOrZeroRegister(0), i.InputOperand(1));
      break;
    case kRiscvDiv64: {
      __ Div64(i.OutputRegister(), i.InputOrZeroRegister(0), i.InputOperand(1));
      // Set ouput to zero if divisor == 0
      __ LoadZeroIfConditionZero(i.OutputRegister(), i.InputRegister(1));
      break;
    }
    case kRiscvDivU64: {
      __ Divu64(i.OutputRegister(), i.InputOrZeroRegister(0),
                i.InputOperand(1));
      // Set ouput to zero if divisor == 0
      __ LoadZeroIfConditionZero(i.OutputRegister(), i.InputRegister(1));
      break;
    }
    case kRiscvMod64:
      __ Mod64(i.OutputRegister(), i.InputOrZeroRegister(0), i.InputOperand(1));
      break;
    case kRiscvModU64:
      __ Modu64(i.OutputRegister(), i.InputOrZeroRegister(0),
                i.InputOperand(1));
      break;
    case kRiscvAnd:
      __ And(i.OutputRegister(), i.InputOrZeroRegister(0), i.InputOperand(1));
      break;
    case kRiscvAnd32:
      __ And(i.OutputRegister(), i.InputOrZeroRegister(0), i.InputOperand(1));
      __ Sll32(i.OutputRegister(), i.OutputRegister(), 0x0);
      break;
    case kRiscvOr:
      __ Or(i.OutputRegister(), i.InputOrZeroRegister(0), i.InputOperand(1));
      break;
    case kRiscvOr32:
      __ Or(i.OutputRegister(), i.InputOrZeroRegister(0), i.InputOperand(1));
      __ Sll32(i.OutputRegister(), i.OutputRegister(), 0x0);
      break;
    case kRiscvNor:
      if (instr->InputAt(1)->IsRegister()) {
        __ Nor(i.OutputRegister(), i.InputOrZeroRegister(0), i.InputOperand(1));
      } else {
        DCHECK_EQ(0, i.InputOperand(1).immediate());
        __ Nor(i.OutputRegister(), i.InputOrZeroRegister(0), zero_reg);
      }
      break;
    case kRiscvNor32:
      if (instr->InputAt(1)->IsRegister()) {
        __ Nor(i.OutputRegister(), i.InputOrZeroRegister(0), i.InputOperand(1));
        __ Sll32(i.OutputRegister(), i.OutputRegister(), 0x0);
      } else {
        DCHECK_EQ(0, i.InputOperand(1).immediate());
        __ Nor(i.OutputRegister(), i.InputOrZeroRegister(0), zero_reg);
        __ Sll32(i.OutputRegister(), i.OutputRegister(), 0x0);
      }
      break;
    case kRiscvXor:
      __ Xor(i.OutputRegister(), i.InputOrZeroRegister(0), i.InputOperand(1));
      break;
    case kRiscvXor32:
      __ Xor(i.OutputRegister(), i.InputOrZeroRegister(0), i.InputOperand(1));
      __ Sll32(i.OutputRegister(), i.OutputRegister(), 0x0);
      break;
    case kRiscvClz32:
      __ Clz32(i.OutputRegister(), i.InputOrZeroRegister(0));
      break;
    case kRiscvClz64:
      __ Clz64(i.OutputRegister(), i.InputOrZeroRegister(0));
      break;
    case kRiscvCtz32: {
      Register src = i.InputRegister(0);
      Register dst = i.OutputRegister();
      __ Ctz32(dst, src);
    } break;
    case kRiscvCtz64: {
      Register src = i.InputRegister(0);
      Register dst = i.OutputRegister();
      __ Ctz64(dst, src);
    } break;
    case kRiscvPopcnt32: {
      Register src = i.InputRegister(0);
      Register dst = i.OutputRegister();
      __ Popcnt32(dst, src, kScratchReg);
    } break;
    case kRiscvPopcnt64: {
      Register src = i.InputRegister(0);
      Register dst = i.OutputRegister();
      __ Popcnt64(dst, src, kScratchReg);
    } break;
    case kRiscvShl32:
      if (instr->InputAt(1)->IsRegister()) {
        __ Sll32(i.OutputRegister(), i.InputRegister(0), i.InputRegister(1));
      } else {
        int64_t imm = i.InputOperand(1).immediate();
        __ Sll32(i.OutputRegister(), i.InputRegister(0),
                 static_cast<uint16_t>(imm));
      }
      break;
    case kRiscvShr32:
      if (instr->InputAt(1)->IsRegister()) {
        __ Srl32(i.OutputRegister(), i.InputRegister(0), i.InputRegister(1));
      } else {
        int64_t imm = i.InputOperand(1).immediate();
        __ Srl32(i.OutputRegister(), i.InputRegister(0),
                 static_cast<uint16_t>(imm));
      }
      break;
    case kRiscvSar32:
      if (instr->InputAt(1)->IsRegister()) {
        __ Sra32(i.OutputRegister(), i.InputRegister(0), i.InputRegister(1));
      } else {
        int64_t imm = i.InputOperand(1).immediate();
        __ Sra32(i.OutputRegister(), i.InputRegister(0),
                 static_cast<uint16_t>(imm));
      }
      break;
    case kRiscvZeroExtendWord: {
      __ ZeroExtendWord(i.OutputRegister(), i.InputRegister(0));
      break;
    }
    case kRiscvSignExtendWord: {
      __ SignExtendWord(i.OutputRegister(), i.InputRegister(0));
      break;
    }
    case kRiscvShl64:
      __ Sll64(i.OutputRegister(), i.InputRegister(0), i.InputOperand(1));
      break;
    case kRiscvShr64:
      __ Srl64(i.OutputRegister(), i.InputRegister(0), i.InputOperand(1));
      break;
    case kRiscvSar64:
      __ Sra64(i.OutputRegister(), i.InputRegister(0), i.InputOperand(1));
      break;
    case kRiscvRor32:
      __ Ror(i.OutputRegister(), i.InputRegister(0), i.InputOperand(1));
      break;
    case kRiscvRor64:
      __ Dror(i.OutputRegister(), i.InputRegister(0), i.InputOperand(1));
      break;
    case kRiscvTst:
      __ And(kScratchReg, i.InputRegister(0), i.InputOperand(1));
      // Pseudo-instruction used for cmp/branch. No opcode emitted here.
      break;
    case kRiscvCmp:
      // Pseudo-instruction used for cmp/branch. No opcode emitted here.
      break;
    case kRiscvCmpZero:
      // Pseudo-instruction used for cmpzero/branch. No opcode emitted here.
      break;
    case kRiscvMov:
      // TODO(plind): Should we combine mov/li like this, or use separate instr?
      //    - Also see x64 ASSEMBLE_BINOP & RegisterOrOperandType
      if (HasRegisterInput(instr, 0)) {
        __ Move(i.OutputRegister(), i.InputRegister(0));
      } else {
        __ li(i.OutputRegister(), i.InputOperand(0));
      }
      break;

    case kRiscvCmpS: {
      FPURegister left = i.InputOrZeroSingleRegister(0);
      FPURegister right = i.InputOrZeroSingleRegister(1);
      bool predicate;
      FPUCondition cc =
          FlagsConditionToConditionCmpFPU(&predicate, instr->flags_condition());

      if ((left == kDoubleRegZero || right == kDoubleRegZero) &&
          !__ IsSingleZeroRegSet()) {
        __ LoadFPRImmediate(kDoubleRegZero, 0.0f);
      }
      // compare result set to kScratchReg
      __ CompareF32(kScratchReg, cc, left, right);
    } break;
    case kRiscvAddS:
      // TODO(plind): add special case: combine mult & add.
      __ fadd_s(i.OutputDoubleRegister(), i.InputDoubleRegister(0),
                i.InputDoubleRegister(1));
      break;
    case kRiscvSubS:
      __ fsub_s(i.OutputDoubleRegister(), i.InputDoubleRegister(0),
                i.InputDoubleRegister(1));
      break;
    case kRiscvMulS:
      // TODO(plind): add special case: right op is -1.0, see arm port.
      __ fmul_s(i.OutputDoubleRegister(), i.InputDoubleRegister(0),
                i.InputDoubleRegister(1));
      break;
    case kRiscvDivS:
      __ fdiv_s(i.OutputDoubleRegister(), i.InputDoubleRegister(0),
                i.InputDoubleRegister(1));
      break;
    case kRiscvModS: {
      // TODO(bmeurer): We should really get rid of this special instruction,
      // and generate a CallAddress instruction instead.
      FrameScope scope(tasm(), StackFrame::MANUAL);
      __ PrepareCallCFunction(0, 2, kScratchReg);
      __ MovToFloatParameters(i.InputDoubleRegister(0),
                              i.InputDoubleRegister(1));
      // TODO(balazs.kilvady): implement mod_two_floats_operation(isolate())
      __ CallCFunction(ExternalReference::mod_two_doubles_operation(), 0, 2);
      // Move the result in the double result register.
      __ MovFromFloatResult(i.OutputSingleRegister());
      break;
    }
    case kRiscvAbsS:
      __ fabs_s(i.OutputSingleRegister(), i.InputSingleRegister(0));
      break;
    case kRiscvNegS:
      __ Neg_s(i.OutputSingleRegister(), i.InputSingleRegister(0));
      break;
    case kRiscvSqrtS: {
      __ fsqrt_s(i.OutputDoubleRegister(), i.InputDoubleRegister(0));
      break;
    }
    case kRiscvMaxS:
      __ fmax_s(i.OutputDoubleRegister(), i.InputDoubleRegister(0),
                i.InputDoubleRegister(1));
      break;
    case kRiscvMinS:
      __ fmin_s(i.OutputDoubleRegister(), i.InputDoubleRegister(0),
                i.InputDoubleRegister(1));
      break;
    case kRiscvCmpD: {
      FPURegister left = i.InputOrZeroDoubleRegister(0);
      FPURegister right = i.InputOrZeroDoubleRegister(1);
      bool predicate;
      FPUCondition cc =
          FlagsConditionToConditionCmpFPU(&predicate, instr->flags_condition());
      if ((left == kDoubleRegZero || right == kDoubleRegZero) &&
          !__ IsDoubleZeroRegSet()) {
        __ LoadFPRImmediate(kDoubleRegZero, 0.0);
      }
      // compare result set to kScratchReg
      __ CompareF64(kScratchReg, cc, left, right);
    } break;
    case kRiscvAddD:
      // TODO(plind): add special case: combine mult & add.
      __ fadd_d(i.OutputDoubleRegister(), i.InputDoubleRegister(0),
                i.InputDoubleRegister(1));
      break;
    case kRiscvSubD:
      __ fsub_d(i.OutputDoubleRegister(), i.InputDoubleRegister(0),
                i.InputDoubleRegister(1));
      break;
    case kRiscvMulD:
      // TODO(plind): add special case: right op is -1.0, see arm port.
      __ fmul_d(i.OutputDoubleRegister(), i.InputDoubleRegister(0),
                i.InputDoubleRegister(1));
      break;
    case kRiscvDivD:
      __ fdiv_d(i.OutputDoubleRegister(), i.InputDoubleRegister(0),
                i.InputDoubleRegister(1));
      break;
    case kRiscvModD: {
      // TODO(bmeurer): We should really get rid of this special instruction,
      // and generate a CallAddress instruction instead.
      FrameScope scope(tasm(), StackFrame::MANUAL);
      __ PrepareCallCFunction(0, 2, kScratchReg);
      __ MovToFloatParameters(i.InputDoubleRegister(0),
                              i.InputDoubleRegister(1));
      __ CallCFunction(ExternalReference::mod_two_doubles_operation(), 0, 2);
      // Move the result in the double result register.
      __ MovFromFloatResult(i.OutputDoubleRegister());
      break;
    }
    case kRiscvAbsD:
      __ fabs_d(i.OutputDoubleRegister(), i.InputDoubleRegister(0));
      break;
    case kRiscvNegD:
      __ Neg_d(i.OutputDoubleRegister(), i.InputDoubleRegister(0));
      break;
    case kRiscvSqrtD: {
      __ fsqrt_d(i.OutputDoubleRegister(), i.InputDoubleRegister(0));
      break;
    }
    case kRiscvMaxD:
      __ fmax_d(i.OutputDoubleRegister(), i.InputDoubleRegister(0),
                i.InputDoubleRegister(1));
      break;
    case kRiscvMinD:
      __ fmin_d(i.OutputDoubleRegister(), i.InputDoubleRegister(0),
                i.InputDoubleRegister(1));
      break;
    case kRiscvFloat64RoundDown: {
      __ Floor_d_d(i.OutputDoubleRegister(), i.InputDoubleRegister(0),
                   kScratchDoubleReg);
      break;
    }
    case kRiscvFloat32RoundDown: {
      __ Floor_s_s(i.OutputSingleRegister(), i.InputSingleRegister(0),
                   kScratchDoubleReg);
      break;
    }
    case kRiscvFloat64RoundTruncate: {
      __ Trunc_d_d(i.OutputDoubleRegister(), i.InputDoubleRegister(0),
                   kScratchDoubleReg);
      break;
    }
    case kRiscvFloat32RoundTruncate: {
      __ Trunc_s_s(i.OutputSingleRegister(), i.InputSingleRegister(0),
                   kScratchDoubleReg);
      break;
    }
    case kRiscvFloat64RoundUp: {
      __ Ceil_d_d(i.OutputDoubleRegister(), i.InputDoubleRegister(0),
                  kScratchDoubleReg);
      break;
    }
    case kRiscvFloat32RoundUp: {
      __ Ceil_s_s(i.OutputSingleRegister(), i.InputSingleRegister(0),
                  kScratchDoubleReg);
      break;
    }
    case kRiscvFloat64RoundTiesEven: {
      __ Round_d_d(i.OutputDoubleRegister(), i.InputDoubleRegister(0),
                   kScratchDoubleReg);
      break;
    }
    case kRiscvFloat32RoundTiesEven: {
      __ Round_s_s(i.OutputSingleRegister(), i.InputSingleRegister(0),
                   kScratchDoubleReg);
      break;
    }
    case kRiscvFloat32Max: {
      __ Float32Max(i.OutputSingleRegister(), i.InputSingleRegister(0),
                    i.InputSingleRegister(1));
      break;
    }
    case kRiscvFloat64Max: {
      __ Float64Max(i.OutputSingleRegister(), i.InputSingleRegister(0),
                    i.InputSingleRegister(1));
      break;
    }
    case kRiscvFloat32Min: {
      __ Float32Min(i.OutputSingleRegister(), i.InputSingleRegister(0),
                    i.InputSingleRegister(1));
      break;
    }
    case kRiscvFloat64Min: {
      __ Float64Min(i.OutputSingleRegister(), i.InputSingleRegister(0),
                    i.InputSingleRegister(1));
      break;
    }
    case kRiscvFloat64SilenceNaN:
      __ FPUCanonicalizeNaN(i.OutputDoubleRegister(), i.InputDoubleRegister(0));
      break;
    case kRiscvCvtSD:
      __ fcvt_s_d(i.OutputSingleRegister(), i.InputDoubleRegister(0));
      break;
    case kRiscvCvtDS:
      __ fcvt_d_s(i.OutputDoubleRegister(), i.InputSingleRegister(0));
      break;
    case kRiscvCvtDW: {
      __ fcvt_d_w(i.OutputDoubleRegister(), i.InputRegister(0));
      break;
    }
    case kRiscvCvtSW: {
      __ fcvt_s_w(i.OutputDoubleRegister(), i.InputRegister(0));
      break;
    }
    case kRiscvCvtSUw: {
      __ Cvt_s_uw(i.OutputDoubleRegister(), i.InputRegister(0));
      break;
    }
    case kRiscvCvtSL: {
      __ fcvt_s_l(i.OutputDoubleRegister(), i.InputRegister(0));
      break;
    }
    case kRiscvCvtDL: {
      __ fcvt_d_l(i.OutputDoubleRegister(), i.InputRegister(0));
      break;
    }
    case kRiscvCvtDUw: {
      __ Cvt_d_uw(i.OutputDoubleRegister(), i.InputRegister(0));
      break;
    }
    case kRiscvCvtDUl: {
      __ Cvt_d_ul(i.OutputDoubleRegister(), i.InputRegister(0));
      break;
    }
    case kRiscvCvtSUl: {
      __ Cvt_s_ul(i.OutputDoubleRegister(), i.InputRegister(0));
      break;
    }
    case kRiscvFloorWD: {
      Register result = instr->OutputCount() > 1 ? i.OutputRegister(1) : no_reg;
      __ Floor_w_d(i.OutputRegister(), i.InputDoubleRegister(0), result);
      break;
    }
    case kRiscvCeilWD: {
      Register result = instr->OutputCount() > 1 ? i.OutputRegister(1) : no_reg;
      __ Ceil_w_d(i.OutputRegister(), i.InputDoubleRegister(0), result);
      break;
    }
    case kRiscvRoundWD: {
      Register result = instr->OutputCount() > 1 ? i.OutputRegister(1) : no_reg;
      __ Round_w_d(i.OutputRegister(), i.InputDoubleRegister(0), result);
      break;
    }
    case kRiscvTruncWD: {
      Register result = instr->OutputCount() > 1 ? i.OutputRegister(1) : no_reg;
      __ Trunc_w_d(i.OutputRegister(), i.InputDoubleRegister(0), result);
      break;
    }
    case kRiscvFloorWS: {
      Register result = instr->OutputCount() > 1 ? i.OutputRegister(1) : no_reg;
      __ Floor_w_s(i.OutputRegister(), i.InputDoubleRegister(0), result);
      break;
    }
    case kRiscvCeilWS: {
      Register result = instr->OutputCount() > 1 ? i.OutputRegister(1) : no_reg;
      __ Ceil_w_s(i.OutputRegister(), i.InputDoubleRegister(0), result);
      break;
    }
    case kRiscvRoundWS: {
      Register result = instr->OutputCount() > 1 ? i.OutputRegister(1) : no_reg;
      __ Round_w_s(i.OutputRegister(), i.InputDoubleRegister(0), result);
      break;
    }
    case kRiscvTruncWS: {
      Label done;
      Register result = instr->OutputCount() > 1 ? i.OutputRegister(1) : no_reg;
      bool set_overflow_to_min_i32 = MiscField::decode(instr->opcode());
      __ Trunc_w_s(i.OutputRegister(), i.InputDoubleRegister(0), result);

      // On RISCV, if the input value exceeds INT32_MAX, the result of fcvt
      // is INT32_MAX. Note that, since INT32_MAX means the lower 31-bits are
      // all 1s, INT32_MAX cannot be represented precisely as a float, so an
      // fcvt result of INT32_MAX always indicate overflow.
      //
      // In wasm_compiler, to detect overflow in converting a FP value, fval, to
      // integer, V8 checks whether I2F(F2I(fval)) equals fval. However, if fval
      // == INT32_MAX+1, the value of I2F(F2I(fval)) happens to be fval. So,
      // INT32_MAX is not a good value to indicate overflow. Instead, we will
      // use INT32_MIN as the converted result of an out-of-range FP value,
      // exploiting the fact that INT32_MAX+1 is INT32_MIN.
      //
      // If the result of conversion overflow, the result will be set to
      // INT32_MIN. Here we detect overflow by testing whether output + 1 <
      // output (i.e., kScratchReg  < output)
      if (set_overflow_to_min_i32) {
        __ Add32(kScratchReg, i.OutputRegister(), 1);
        __ BranchShort(&done, lt, i.OutputRegister(), Operand(kScratchReg));
        __ Move(i.OutputRegister(), kScratchReg);
        __ bind(&done);
      }
      break;
    }
    case kRiscvTruncLS: {
      Register result = instr->OutputCount() > 1 ? i.OutputRegister(1) : no_reg;
      __ Trunc_l_s(i.OutputRegister(), i.InputDoubleRegister(0), result);
      break;
    }
    case kRiscvTruncLD: {
      Label done;
      Register result = instr->OutputCount() > 1 ? i.OutputRegister(1) : no_reg;
      bool set_overflow_to_min_i64 = MiscField::decode(instr->opcode());
      __ Trunc_l_d(i.OutputRegister(), i.InputDoubleRegister(0), result);
      if (set_overflow_to_min_i64) {
        __ Add64(kScratchReg, i.OutputRegister(), 1);
        __ BranchShort(&done, lt, i.OutputRegister(), Operand(kScratchReg));
        __ Move(i.OutputRegister(), kScratchReg);
        __ bind(&done);
      }
      break;
    }
    case kRiscvTruncUwD: {
      Register result = instr->OutputCount() > 1 ? i.OutputRegister(1) : no_reg;
      __ Trunc_uw_d(i.OutputRegister(), i.InputDoubleRegister(0), result);
      break;
    }
    case kRiscvTruncUwS: {
      Register result = instr->OutputCount() > 1 ? i.OutputRegister(1) : no_reg;
      bool set_overflow_to_min_u32 = MiscField::decode(instr->opcode());
      __ Trunc_uw_s(i.OutputRegister(), i.InputDoubleRegister(0), result);

      // On RISCV, if the input value exceeds UINT32_MAX, the result of fcvt
      // is UINT32_MAX. Note that, since UINT32_MAX means all 32-bits are 1s,
      // UINT32_MAX cannot be represented precisely as float, so an fcvt result
      // of UINT32_MAX always indicates overflow.
      //
      // In wasm_compiler.cc, to detect overflow in converting a FP value, fval,
      // to integer, V8 checks whether I2F(F2I(fval)) equals fval. However, if
      // fval == UINT32_MAX+1, the value of I2F(F2I(fval)) happens to be fval.
      // So, UINT32_MAX is not a good value to indicate overflow. Instead, we
      // will use 0 as the converted result of an out-of-range FP value,
      // exploiting the fact that UINT32_MAX+1 is 0.
      if (set_overflow_to_min_u32) {
        __ Add32(kScratchReg, i.OutputRegister(), 1);
        // Set ouput to zero if result overflows (i.e., UINT32_MAX)
        __ LoadZeroIfConditionZero(i.OutputRegister(), kScratchReg);
      }
      break;
    }
    case kRiscvTruncUlS: {
      Register result = instr->OutputCount() > 1 ? i.OutputRegister(1) : no_reg;
      __ Trunc_ul_s(i.OutputRegister(), i.InputDoubleRegister(0), result);
      break;
    }
    case kRiscvTruncUlD: {
      Register result = instr->OutputCount() > 1 ? i.OutputRegister(1) : no_reg;
      __ Trunc_ul_d(i.OutputRegister(0), i.InputDoubleRegister(0), result);
      break;
    }
    case kRiscvBitcastDL:
      __ fmv_x_d(i.OutputRegister(), i.InputDoubleRegister(0));
      break;
    case kRiscvBitcastLD:
      __ fmv_d_x(i.OutputDoubleRegister(), i.InputRegister(0));
      break;
    case kRiscvBitcastInt32ToFloat32:
      __ fmv_w_x(i.OutputDoubleRegister(), i.InputRegister(0));
      break;
    case kRiscvBitcastFloat32ToInt32:
      __ fmv_x_w(i.OutputRegister(), i.InputDoubleRegister(0));
      break;
    case kRiscvFloat64ExtractLowWord32:
      __ ExtractLowWordFromF64(i.OutputRegister(), i.InputDoubleRegister(0));
      break;
    case kRiscvFloat64ExtractHighWord32:
      __ ExtractHighWordFromF64(i.OutputRegister(), i.InputDoubleRegister(0));
      break;
    case kRiscvFloat64InsertLowWord32:
      __ InsertLowWordF64(i.OutputDoubleRegister(), i.InputRegister(1));
      break;
    case kRiscvFloat64InsertHighWord32:
      __ InsertHighWordF64(i.OutputDoubleRegister(), i.InputRegister(1));
      break;
      // ... more basic instructions ...

    case kRiscvSignExtendByte:
      __ SignExtendByte(i.OutputRegister(), i.InputRegister(0));
      break;
    case kRiscvSignExtendShort:
      __ SignExtendShort(i.OutputRegister(), i.InputRegister(0));
      break;
    case kRiscvLbu:
      __ Lbu(i.OutputRegister(), i.MemoryOperand());
      break;
    case kRiscvLb:
      __ Lb(i.OutputRegister(), i.MemoryOperand());
      break;
    case kRiscvSb:
      __ Sb(i.InputOrZeroRegister(2), i.MemoryOperand());
      break;
    case kRiscvLhu:
      __ Lhu(i.OutputRegister(), i.MemoryOperand());
      break;
    case kRiscvUlhu:
      __ Ulhu(i.OutputRegister(), i.MemoryOperand());
      break;
    case kRiscvLh:
      __ Lh(i.OutputRegister(), i.MemoryOperand());
      break;
    case kRiscvUlh:
      __ Ulh(i.OutputRegister(), i.MemoryOperand());
      break;
    case kRiscvSh:
      __ Sh(i.InputOrZeroRegister(2), i.MemoryOperand());
      break;
    case kRiscvUsh:
      __ Ush(i.InputOrZeroRegister(2), i.MemoryOperand());
      break;
    case kRiscvLw:
      __ Lw(i.OutputRegister(), i.MemoryOperand());
      break;
    case kRiscvUlw:
      __ Ulw(i.OutputRegister(), i.MemoryOperand());
      break;
    case kRiscvLwu:
      __ Lwu(i.OutputRegister(), i.MemoryOperand());
      break;
    case kRiscvUlwu:
      __ Ulwu(i.OutputRegister(), i.MemoryOperand());
      break;
    case kRiscvLd:
      __ Ld(i.OutputRegister(), i.MemoryOperand());
      break;
    case kRiscvUld:
      __ Uld(i.OutputRegister(), i.MemoryOperand());
      break;
    case kRiscvSw:
      __ Sw(i.InputOrZeroRegister(2), i.MemoryOperand());
      break;
    case kRiscvUsw:
      __ Usw(i.InputOrZeroRegister(2), i.MemoryOperand());
      break;
    case kRiscvSd:
      __ Sd(i.InputOrZeroRegister(2), i.MemoryOperand());
      break;
    case kRiscvUsd:
      __ Usd(i.InputOrZeroRegister(2), i.MemoryOperand());
      break;
    case kRiscvLoadFloat: {
      __ LoadFloat(i.OutputSingleRegister(), i.MemoryOperand());
      break;
    }
    case kRiscvULoadFloat: {
      __ ULoadFloat(i.OutputSingleRegister(), i.MemoryOperand(), kScratchReg);
      break;
    }
    case kRiscvStoreFloat: {
      size_t index = 0;
      MemOperand operand = i.MemoryOperand(&index);
      FPURegister ft = i.InputOrZeroSingleRegister(index);
      if (ft == kDoubleRegZero && !__ IsSingleZeroRegSet()) {
        __ LoadFPRImmediate(kDoubleRegZero, 0.0f);
      }
      __ StoreFloat(ft, operand);
      break;
    }
    case kRiscvUStoreFloat: {
      size_t index = 0;
      MemOperand operand = i.MemoryOperand(&index);
      FPURegister ft = i.InputOrZeroSingleRegister(index);
      if (ft == kDoubleRegZero && !__ IsSingleZeroRegSet()) {
        __ LoadFPRImmediate(kDoubleRegZero, 0.0f);
      }
      __ UStoreFloat(ft, operand, kScratchReg);
      break;
    }
    case kRiscvLoadDouble:
      __ LoadDouble(i.OutputDoubleRegister(), i.MemoryOperand());
      break;
    case kRiscvULoadDouble:
      __ ULoadDouble(i.OutputDoubleRegister(), i.MemoryOperand(), kScratchReg);
      break;
    case kRiscvStoreDouble: {
      FPURegister ft = i.InputOrZeroDoubleRegister(2);
      if (ft == kDoubleRegZero && !__ IsDoubleZeroRegSet()) {
        __ LoadFPRImmediate(kDoubleRegZero, 0.0);
      }
      __ StoreDouble(ft, i.MemoryOperand());
      break;
    }
    case kRiscvUStoreDouble: {
      FPURegister ft = i.InputOrZeroDoubleRegister(2);
      if (ft == kDoubleRegZero && !__ IsDoubleZeroRegSet()) {
        __ LoadFPRImmediate(kDoubleRegZero, 0.0);
      }
      __ UStoreDouble(ft, i.MemoryOperand(), kScratchReg);
      break;
    }
    case kRiscvSync: {
      __ sync();
      break;
    }
    case kRiscvPush:
      if (instr->InputAt(0)->IsFPRegister()) {
        __ StoreDouble(i.InputDoubleRegister(0), MemOperand(sp, -kDoubleSize));
        __ Sub32(sp, sp, Operand(kDoubleSize));
        frame_access_state()->IncreaseSPDelta(kDoubleSize / kSystemPointerSize);
      } else {
        __ Push(i.InputOrZeroRegister(0));
        frame_access_state()->IncreaseSPDelta(1);
      }
      break;
    case kRiscvPeek: {
      int reverse_slot = i.InputInt32(0);
      int offset =
          FrameSlotToFPOffset(frame()->GetTotalFrameSlotCount() - reverse_slot);
      if (instr->OutputAt(0)->IsFPRegister()) {
        LocationOperand* op = LocationOperand::cast(instr->OutputAt(0));
        if (op->representation() == MachineRepresentation::kFloat64) {
          __ LoadDouble(i.OutputDoubleRegister(), MemOperand(fp, offset));
        } else {
          DCHECK_EQ(op->representation(), MachineRepresentation::kFloat32);
          __ LoadFloat(
              i.OutputSingleRegister(0),
              MemOperand(fp, offset + kLessSignificantWordInDoublewordOffset));
        }
      } else {
        __ Ld(i.OutputRegister(0), MemOperand(fp, offset));
      }
      break;
    }
    case kRiscvStackClaim: {
      __ Sub64(sp, sp, Operand(i.InputInt32(0)));
      frame_access_state()->IncreaseSPDelta(i.InputInt32(0) /
                                            kSystemPointerSize);
      break;
    }
    case kRiscvStoreToStackSlot: {
      if (instr->InputAt(0)->IsFPRegister()) {
        if (instr->InputAt(0)->IsSimd128Register()) {
          Register dst = sp;
          if (i.InputInt32(1) != 0) {
            dst = kScratchReg2;
            __ Add64(kScratchReg2, sp, Operand(i.InputInt32(1)));
          }
          __ VU.set(kScratchReg, E8, m1);
          __ vs(i.InputSimd128Register(0), dst, 0, E8);
        } else {
          __ StoreDouble(i.InputDoubleRegister(0),
                         MemOperand(sp, i.InputInt32(1)));
        }
      } else {
        __ Sd(i.InputOrZeroRegister(0), MemOperand(sp, i.InputInt32(1)));
      }
      break;
    }
    case kRiscvByteSwap64: {
      __ ByteSwap(i.OutputRegister(0), i.InputRegister(0), 8, kScratchReg);
      break;
    }
    case kRiscvByteSwap32: {
      __ ByteSwap(i.OutputRegister(0), i.InputRegister(0), 4, kScratchReg);
      break;
    }
    case kAtomicLoadInt8:
      DCHECK_EQ(AtomicWidthField::decode(opcode), AtomicWidth::kWord32);
      ASSEMBLE_ATOMIC_LOAD_INTEGER(Lb);
      break;
    case kAtomicLoadUint8:
      ASSEMBLE_ATOMIC_LOAD_INTEGER(Lbu);
      break;
    case kAtomicLoadInt16:
      DCHECK_EQ(AtomicWidthField::decode(opcode), AtomicWidth::kWord32);
      ASSEMBLE_ATOMIC_LOAD_INTEGER(Lh);
      break;
    case kAtomicLoadUint16:
      ASSEMBLE_ATOMIC_LOAD_INTEGER(Lhu);
      break;
    case kAtomicLoadWord32:
      ASSEMBLE_ATOMIC_LOAD_INTEGER(Lw);
      break;
    case kRiscvWord64AtomicLoadUint64:
      ASSEMBLE_ATOMIC_LOAD_INTEGER(Ld);
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
    case kRiscvWord64AtomicStoreWord64:
      ASSEMBLE_ATOMIC_STORE_INTEGER(Sd);
      break;
    case kAtomicExchangeInt8:
      DCHECK_EQ(AtomicWidthField::decode(opcode), AtomicWidth::kWord32);
      ASSEMBLE_ATOMIC_EXCHANGE_INTEGER_EXT(Ll, Sc, true, 8, 32);
      break;
    case kAtomicExchangeUint8:
      switch (AtomicWidthField::decode(opcode)) {
        case AtomicWidth::kWord32:
          ASSEMBLE_ATOMIC_EXCHANGE_INTEGER_EXT(Ll, Sc, false, 8, 32);
          break;
        case AtomicWidth::kWord64:
          ASSEMBLE_ATOMIC_EXCHANGE_INTEGER_EXT(Lld, Scd, false, 8, 64);
          break;
      }
      break;
    case kAtomicExchangeInt16:
      DCHECK_EQ(AtomicWidthField::decode(opcode), AtomicWidth::kWord32);
      ASSEMBLE_ATOMIC_EXCHANGE_INTEGER_EXT(Ll, Sc, true, 16, 32);
      break;
    case kAtomicExchangeUint16:
      switch (AtomicWidthField::decode(opcode)) {
        case AtomicWidth::kWord32:
          ASSEMBLE_ATOMIC_EXCHANGE_INTEGER_EXT(Ll, Sc, false, 16, 32);
          break;
        case AtomicWidth::kWord64:
          ASSEMBLE_ATOMIC_EXCHANGE_INTEGER_EXT(Lld, Scd, false, 16, 64);
          break;
      }
      break;
    case kAtomicExchangeWord32:
      switch (AtomicWidthField::decode(opcode)) {
        case AtomicWidth::kWord32:
          ASSEMBLE_ATOMIC_EXCHANGE_INTEGER(Ll, Sc);
          break;
        case AtomicWidth::kWord64:
          ASSEMBLE_ATOMIC_EXCHANGE_INTEGER_EXT(Lld, Scd, false, 32, 64);
          break;
      }
      break;
    case kRiscvWord64AtomicExchangeUint64:
      ASSEMBLE_ATOMIC_EXCHANGE_INTEGER(Lld, Scd);
      break;
    case kAtomicCompareExchangeInt8:
      DCHECK_EQ(AtomicWidthField::decode(opcode), AtomicWidth::kWord32);
      ASSEMBLE_ATOMIC_COMPARE_EXCHANGE_INTEGER_EXT(Ll, Sc, true, 8, 32);
      break;
    case kAtomicCompareExchangeUint8:
      switch (AtomicWidthField::decode(opcode)) {
        case AtomicWidth::kWord32:
          ASSEMBLE_ATOMIC_COMPARE_EXCHANGE_INTEGER_EXT(Ll, Sc, false, 8, 32);
          break;
        case AtomicWidth::kWord64:
          ASSEMBLE_ATOMIC_COMPARE_EXCHANGE_INTEGER_EXT(Lld, Scd, false, 8, 64);
          break;
      }
      break;
    case kAtomicCompareExchangeInt16:
      DCHECK_EQ(AtomicWidthField::decode(opcode), AtomicWidth::kWord32);
      ASSEMBLE_ATOMIC_COMPARE_EXCHANGE_INTEGER_EXT(Ll, Sc, true, 16, 32);
      break;
    case kAtomicCompareExchangeUint16:
      switch (AtomicWidthField::decode(opcode)) {
        case AtomicWidth::kWord32:
          ASSEMBLE_ATOMIC_COMPARE_EXCHANGE_INTEGER_EXT(Ll, Sc, false, 16, 32);
          break;
        case AtomicWidth::kWord64:
          ASSEMBLE_ATOMIC_COMPARE_EXCHANGE_INTEGER_EXT(Lld, Scd, false, 16, 64);
          break;
      }
      break;
    case kAtomicCompareExchangeWord32:
      switch (AtomicWidthField::decode(opcode)) {
        case AtomicWidth::kWord32:
          __ Sll32(i.InputRegister(2), i.InputRegister(2), 0);
          ASSEMBLE_ATOMIC_COMPARE_EXCHANGE_INTEGER(Ll, Sc);
          break;
        case AtomicWidth::kWord64:
          ASSEMBLE_ATOMIC_COMPARE_EXCHANGE_INTEGER_EXT(Lld, Scd, false, 32, 64);
          break;
      }
      break;
    case kRiscvWord64AtomicCompareExchangeUint64:
      ASSEMBLE_ATOMIC_COMPARE_EXCHANGE_INTEGER(Lld, Scd);
      break;
#define ATOMIC_BINOP_CASE(op, inst32, inst64)                          \
  case kAtomic##op##Int8:                                              \
    DCHECK_EQ(AtomicWidthField::decode(opcode), AtomicWidth::kWord32); \
    ASSEMBLE_ATOMIC_BINOP_EXT(Ll, Sc, true, 8, inst32, 32);            \
    break;                                                             \
  case kAtomic##op##Uint8:                                             \
    switch (AtomicWidthField::decode(opcode)) {                        \
      case AtomicWidth::kWord32:                                       \
        ASSEMBLE_ATOMIC_BINOP_EXT(Ll, Sc, false, 8, inst32, 32);       \
        break;                                                         \
      case AtomicWidth::kWord64:                                       \
        ASSEMBLE_ATOMIC_BINOP_EXT(Lld, Scd, false, 8, inst64, 64);     \
        break;                                                         \
    }                                                                  \
    break;                                                             \
  case kAtomic##op##Int16:                                             \
    DCHECK_EQ(AtomicWidthField::decode(opcode), AtomicWidth::kWord32); \
    ASSEMBLE_ATOMIC_BINOP_EXT(Ll, Sc, true, 16, inst32, 32);           \
    break;                                                             \
  case kAtomic##op##Uint16:                                            \
    switch (AtomicWidthField::decode(opcode)) {                        \
      case AtomicWidth::kWord32:                                       \
        ASSEMBLE_ATOMIC_BINOP_EXT(Ll, Sc, false, 16, inst32, 32);      \
        break;                                                         \
      case AtomicWidth::kWord64:                                       \
        ASSEMBLE_ATOMIC_BINOP_EXT(Lld, Scd, false, 16, inst64, 64);    \
        break;                                                         \
    }                                                                  \
    break;                                                             \
  case kAtomic##op##Word32:                                            \
    switch (AtomicWidthField::decode(opcode)) {                        \
      case AtomicWidth::kWord32:                                       \
        ASSEMBLE_ATOMIC_BINOP(Ll, Sc, inst32);                         \
        break;                                                         \
      case AtomicWidth::kWord64:                                       \
        ASSEMBLE_ATOMIC_BINOP_EXT(Lld, Scd, false, 32, inst64, 64);    \
        break;                                                         \
    }                                                                  \
    break;                                                             \
  case kRiscvWord64Atomic##op##Uint64:                                 \
    ASSEMBLE_ATOMIC_BINOP(Lld, Scd, inst64);                           \
    break;
      ATOMIC_BINOP_CASE(Add, Add32, Add64)
      ATOMIC_BINOP_CASE(Sub, Sub32, Sub64)
      ATOMIC_BINOP_CASE(And, And, And)
      ATOMIC_BINOP_CASE(Or, Or, Or)
      ATOMIC_BINOP_CASE(Xor, Xor, Xor)
#undef ATOMIC_BINOP_CASE
    case kRiscvAssertEqual:
      __ Assert(eq, static_cast<AbortReason>(i.InputOperand(2).immediate()),
                i.InputRegister(0), Operand(i.InputRegister(1)));
      break;
    case kRiscvStoreCompressTagged: {
      size_t index = 0;
      MemOperand operand = i.MemoryOperand(&index);
      __ StoreTaggedField(i.InputOrZeroRegister(index), operand);
      break;
    }
    case kRiscvLoadDecompressTaggedSigned: {
      CHECK(instr->HasOutput());
      Register result = i.OutputRegister();
      MemOperand operand = i.MemoryOperand();
      __ DecompressTaggedSigned(result, operand);
      break;
    }
    case kRiscvLoadDecompressTaggedPointer: {
      CHECK(instr->HasOutput());
      Register result = i.OutputRegister();
      MemOperand operand = i.MemoryOperand();
      __ DecompressTaggedPointer(result, operand);
      break;
    }
    case kRiscvLoadDecompressAnyTagged: {
      CHECK(instr->HasOutput());
      Register result = i.OutputRegister();
      MemOperand operand = i.MemoryOperand();
      __ DecompressAnyTagged(result, operand);
      break;
    }
    case kRiscvRvvSt: {
      (__ VU).set(kScratchReg, VSew::E8, Vlmul::m1);
      Register dst = i.MemoryOperand().offset() == 0 ? i.MemoryOperand().rm()
                                                     : kScratchReg;
      if (i.MemoryOperand().offset() != 0) {
        __ Add64(dst, i.MemoryOperand().rm(), i.MemoryOperand().offset());
      }
      __ vs(i.InputSimd128Register(2), dst, 0, VSew::E8);
      break;
    }
    case kRiscvRvvLd: {
      (__ VU).set(kScratchReg, VSew::E8, Vlmul::m1);
      Register src = i.MemoryOperand().offset() == 0 ? i.MemoryOperand().rm()
                                                     : kScratchReg;
      if (i.MemoryOperand().offset() != 0) {
        __ Add64(src, i.MemoryOperand().rm(), i.MemoryOperand().offset());
      }
      __ vl(i.OutputSimd128Register(), src, 0, VSew::E8);
      break;
    }
    case kRiscvS128Zero: {
      Simd128Register dst = i.OutputSimd128Register();
      __ VU.set(kScratchReg, E8, m1);
      __ vmv_vx(dst, zero_reg);
      break;
    }
    case kRiscvS128Load32Zero: {
      Simd128Register dst = i.OutputSimd128Register();
      __ VU.set(kScratchReg, E32, m1);
      __ Lwu(kScratchReg, i.MemoryOperand());
      __ vmv_sx(dst, kScratchReg);
      break;
    }
    case kRiscvS128Load64Zero: {
      Simd128Register dst = i.OutputSimd128Register();
      __ VU.set(kScratchReg, E64, m1);
      __ Ld(kScratchReg, i.MemoryOperand());
      __ vmv_sx(dst, kScratchReg);
      break;
    }
    case kRiscvS128LoadLane: {
      Simd128Register dst = i.OutputSimd128Register();
      DCHECK_EQ(dst, i.InputSimd128Register(0));
      auto sz = static_cast<int>(MiscField::decode(instr->opcode()));
      __ LoadLane(sz, dst, i.InputUint8(1), i.MemoryOperand(2));
      break;
    }
    case kRiscvS128StoreLane: {
      Simd128Register src = i.InputSimd128Register(0);
      DCHECK_EQ(src, i.InputSimd128Register(0));
      auto sz = static_cast<int>(MiscField::decode(instr->opcode()));
      __ StoreLane(sz, src, i.InputUint8(1), i.MemoryOperand(2));
      break;
    }
    case kRiscvS128Load64ExtendS: {
      __ VU.set(kScratchReg, E64, m1);
      __ Ld(kScratchReg, i.MemoryOperand());
      __ vmv_vx(kSimd128ScratchReg, kScratchReg);
      __ VU.set(kScratchReg, i.InputInt8(2), m1);
      __ vsext_vf2(i.OutputSimd128Register(), kSimd128ScratchReg);
      break;
    }
    case kRiscvS128Load64ExtendU: {
      __ VU.set(kScratchReg, E64, m1);
      __ Ld(kScratchReg, i.MemoryOperand());
      __ vmv_vx(kSimd128ScratchReg, kScratchReg);
      __ VU.set(kScratchReg, i.InputInt8(2), m1);
      __ vzext_vf2(i.OutputSimd128Register(), kSimd128ScratchReg);
      break;
    }
    case kRiscvS128LoadSplat: {
      __ VU.set(kScratchReg, i.InputInt8(2), i.InputInt8(3));
      switch (i.InputInt8(2)) {
        case E8:
          __ Lb(kScratchReg, i.MemoryOperand());
          break;
        case E16:
          __ Lh(kScratchReg, i.MemoryOperand());
          break;
        case E32:
          __ Lw(kScratchReg, i.MemoryOperand());
          break;
        case E64:
          __ Ld(kScratchReg, i.MemoryOperand());
          break;
        default:
          UNREACHABLE();
      }
      __ vmv_vx(i.OutputSimd128Register(), kScratchReg);
      break;
    }
    case kRiscvS128AllOnes: {
      __ VU.set(kScratchReg, E8, m1);
      __ vmv_vx(i.OutputSimd128Register(), zero_reg);
      __ vnot_vv(i.OutputSimd128Register(), i.OutputSimd128Register());
      break;
    }
    case kRiscvS128Select: {
      __ VU.set(kScratchReg, E8, m1);
      __ vand_vv(kSimd128ScratchReg, i.InputSimd128Register(1),
                 i.InputSimd128Register(0));
      __ vnot_vv(kSimd128ScratchReg2, i.InputSimd128Register(0));
      __ vand_vv(kSimd128ScratchReg2, i.InputSimd128Register(2),
                 kSimd128ScratchReg2);
      __ vor_vv(i.OutputSimd128Register(), kSimd128ScratchReg,
                kSimd128ScratchReg2);
      break;
    }
    case kRiscvS128And: {
      (__ VU).set(kScratchReg, VSew::E8, Vlmul::m1);
      __ vand_vv(i.OutputSimd128Register(), i.InputSimd128Register(0),
                 i.InputSimd128Register(1));
      break;
    }
    case kRiscvS128Or: {
      (__ VU).set(kScratchReg, VSew::E8, Vlmul::m1);
      __ vor_vv(i.OutputSimd128Register(), i.InputSimd128Register(0),
                i.InputSimd128Register(1));
      break;
    }
    case kRiscvS128Xor: {
      (__ VU).set(kScratchReg, VSew::E8, Vlmul::m1);
      __ vxor_vv(i.OutputSimd128Register(), i.InputSimd128Register(0),
                 i.InputSimd128Register(1));
      break;
    }
    case kRiscvS128Not: {
      (__ VU).set(kScratchReg, VSew::E8, Vlmul::m1);
      __ vnot_vv(i.OutputSimd128Register(), i.InputSimd128Register(0));
      break;
    }
    case kRiscvS128AndNot: {
      (__ VU).set(kScratchReg, VSew::E8, Vlmul::m1);
      __ vnot_vv(i.OutputSimd128Register(), i.InputSimd128Register(1));
      __ vand_vv(i.OutputSimd128Register(), i.InputSimd128Register(0),
                 i.OutputSimd128Register());
      break;
    }
    case kRiscvS128Const: {
      Simd128Register dst = i.OutputSimd128Register();
      uint8_t imm[16];
      *reinterpret_cast<uint64_t*>(imm) =
          make_uint64(i.InputUint32(1), i.InputUint32(0));
      *(reinterpret_cast<uint64_t*>(imm) + 1) =
          make_uint64(i.InputUint32(3), i.InputUint32(2));
      __ WasmRvvS128const(dst, imm);
      break;
    }
    case kRiscvI64x2Mul: {
      (__ VU).set(kScratchReg, VSew::E64, Vlmul::m1);
      __ vmul_vv(i.OutputSimd128Register(), i.InputSimd128Register(0),
                 i.InputSimd128Register(1));
      break;
    }
    case kRiscvI64x2Add: {
      (__ VU).set(kScratchReg, VSew::E64, Vlmul::m1);
      __ vadd_vv(i.OutputSimd128Register(), i.InputSimd128Register(0),
                 i.InputSimd128Register(1));
      break;
    }
    case kRiscvVrgather: {
      Simd128Register index = i.InputSimd128Register(0);
      if (!(instr->InputAt(1)->IsImmediate())) {
        index = i.InputSimd128Register(1);
      } else {
        __ VU.set(kScratchReg, E64, m1);
        __ li(kScratchReg, i.InputInt64(1));
        __ vmv_sx(kSimd128ScratchReg3, kScratchReg);
        index = kSimd128ScratchReg3;
      }
      __ VU.set(kScratchReg, i.InputInt8(2), i.InputInt8(3));
      if (i.OutputSimd128Register() == i.InputSimd128Register(0)) {
        __ vrgather_vv(kSimd128ScratchReg, i.InputSimd128Register(0), index);
        __ vmv_vv(i.OutputSimd128Register(), kSimd128ScratchReg);
      } else {
        __ vrgather_vv(i.OutputSimd128Register(), i.InputSimd128Register(0),
                       index);
      }
      break;
    }
    case kRiscvVslidedown: {
      __ VU.set(kScratchReg, i.InputInt8(2), i.InputInt8(3));
      if (instr->InputAt(1)->IsImmediate()) {
        DCHECK(is_uint5(i.InputInt32(1)));
        __ vslidedown_vi(i.OutputSimd128Register(), i.InputSimd128Register(0),
                         i.InputInt5(1));
      } else {
        __ vslidedown_vx(i.OutputSimd128Register(), i.InputSimd128Register(0),
                         i.InputRegister(1));
      }
      break;
    }
    case kRiscvI8x16RoundingAverageU: {
      __ VU.set(kScratchReg2, E8, m1);
      __ vwaddu_vv(kSimd128ScratchReg, i.InputSimd128Register(0),
                   i.InputSimd128Register(1));
      __ li(kScratchReg, 1);
      __ vwaddu_wx(kSimd128ScratchReg3, kSimd128ScratchReg, kScratchReg);
      __ li(kScratchReg, 2);
      __ VU.set(kScratchReg2, E16, m2);
      __ vdivu_vx(kSimd128ScratchReg3, kSimd128ScratchReg3, kScratchReg);
      __ VU.set(kScratchReg2, E8, m1);
      __ vnclipu_vi(i.OutputSimd128Register(), kSimd128ScratchReg3, 0);
      break;
    }
    case kRiscvI16x8RoundingAverageU: {
      __ VU.set(kScratchReg2, E16, m1);
      __ vwaddu_vv(kSimd128ScratchReg, i.InputSimd128Register(0),
                   i.InputSimd128Register(1));
      __ li(kScratchReg, 1);
      __ vwaddu_wx(kSimd128ScratchReg3, kSimd128ScratchReg, kScratchReg);
      __ li(kScratchReg, 2);
      __ VU.set(kScratchReg2, E32, m2);
      __ vdivu_vx(kSimd128ScratchReg3, kSimd128ScratchReg3, kScratchReg);
      __ VU.set(kScratchReg2, E16, m1);
      __ vnclipu_vi(i.OutputSimd128Register(), kSimd128ScratchReg3, 0);
      break;
    }
    case kRiscvI16x8Mul: {
      (__ VU).set(kScratchReg, VSew::E16, Vlmul::m1);
      __ vmv_vx(kSimd128ScratchReg, zero_reg);
      __ vmul_vv(i.OutputSimd128Register(), i.InputSimd128Register(0),
                 i.InputSimd128Register(1));
      break;
    }
    case kRiscvI16x8Q15MulRSatS: {
      __ VU.set(kScratchReg, E16, m1);
      __ vsmul_vv(i.OutputSimd128Register(), i.InputSimd128Register(0),
                  i.InputSimd128Register(1));
      break;
    }
    case kRiscvI16x8AddSatS: {
      (__ VU).set(kScratchReg, VSew::E16, Vlmul::m1);
      __ vsadd_vv(i.OutputSimd128Register(), i.InputSimd128Register(0),
                  i.InputSimd128Register(1));
      break;
    }
    case kRiscvI16x8AddSatU: {
      (__ VU).set(kScratchReg, VSew::E16, Vlmul::m1);
      __ vsaddu_vv(i.OutputSimd128Register(), i.InputSimd128Register(0),
                   i.InputSimd128Register(1));
      break;
    }
    case kRiscvI8x16AddSatS: {
      (__ VU).set(kScratchReg, VSew::E8, Vlmul::m1);
      __ vsadd_vv(i.OutputSimd128Register(), i.InputSimd128Register(0),
                  i.InputSimd128Register(1));
      break;
    }
    case kRiscvI8x16AddSatU: {
      (__ VU).set(kScratchReg, VSew::E8, Vlmul::m1);
      __ vsaddu_vv(i.OutputSimd128Register(), i.InputSimd128Register(0),
                   i.InputSimd128Register(1));
      break;
    }
    case kRiscvI64x2Sub: {
      (__ VU).set(kScratchReg, VSew::E64, Vlmul::m1);
      __ vsub_vv(i.OutputSimd128Register(), i.InputSimd128Register(0),
                 i.InputSimd128Register(1));
      break;
    }
    case kRiscvI16x8SubSatS: {
      (__ VU).set(kScratchReg, VSew::E16, Vlmul::m1);
      __ vssub_vv(i.OutputSimd128Register(), i.InputSimd128Register(0),
                  i.InputSimd128Register(1));
      break;
    }
    case kRiscvI16x8SubSatU: {
      (__ VU).set(kScratchReg, VSew::E16, Vlmul::m1);
      __ vssubu_vv(i.OutputSimd128Register(), i.InputSimd128Register(0),
                   i.InputSimd128Register(1));
      break;
    }
    case kRiscvI8x16SubSatS: {
      (__ VU).set(kScratchReg, VSew::E8, Vlmul::m1);
      __ vssub_vv(i.OutputSimd128Register(), i.InputSimd128Register(0),
                  i.InputSimd128Register(1));
      break;
    }
    case kRiscvI8x16SubSatU: {
      (__ VU).set(kScratchReg, VSew::E8, Vlmul::m1);
      __ vssubu_vv(i.OutputSimd128Register(), i.InputSimd128Register(0),
                   i.InputSimd128Register(1));
      break;
    }
    case kRiscvI8x16ExtractLaneU: {
      __ VU.set(kScratchReg, E8, m1);
      __ vslidedown_vi(kSimd128ScratchReg, i.InputSimd128Register(0),
                       i.InputInt8(1));
      __ vmv_xs(i.OutputRegister(), kSimd128ScratchReg);
      __ slli(i.OutputRegister(), i.OutputRegister(), 64 - 8);
      __ srli(i.OutputRegister(), i.OutputRegister(), 64 - 8);
      break;
    }
    case kRiscvI8x16ExtractLaneS: {
      __ VU.set(kScratchReg, E8, m1);
      __ vslidedown_vi(kSimd128ScratchReg, i.InputSimd128Register(0),
                       i.InputInt8(1));
      __ vmv_xs(i.OutputRegister(), kSimd128ScratchReg);
      break;
    }
    case kRiscvI16x8ExtractLaneU: {
      __ VU.set(kScratchReg, E16, m1);
      __ vslidedown_vi(kSimd128ScratchReg, i.InputSimd128Register(0),
                       i.InputInt8(1));
      __ vmv_xs(i.OutputRegister(), kSimd128ScratchReg);
      __ slli(i.OutputRegister(), i.OutputRegister(), 64 - 16);
      __ srli(i.OutputRegister(), i.OutputRegister(), 64 - 16);
      break;
    }
    case kRiscvI16x8ExtractLaneS: {
      __ VU.set(kScratchReg, E16, m1);
      __ vslidedown_vi(kSimd128ScratchReg, i.InputSimd128Register(0),
                       i.InputInt8(1));
      __ vmv_xs(i.OutputRegister(), kSimd128ScratchReg);
      break;
    }
    case kRiscvI8x16ShrU: {
      __ VU.set(kScratchReg, E8, m1);
      if (instr->InputAt(1)->IsRegister()) {
        __ andi(i.InputRegister(1), i.InputRegister(1), 8 - 1);
        __ vsrl_vx(i.OutputSimd128Register(), i.InputSimd128Register(0),
                   i.InputRegister(1));
      } else {
        __ vsrl_vi(i.OutputSimd128Register(), i.InputSimd128Register(0),
                   i.InputInt5(1) % 8);
      }
      break;
    }
    case kRiscvI16x8ShrU: {
      __ VU.set(kScratchReg, E16, m1);
      if (instr->InputAt(1)->IsRegister()) {
        __ andi(i.InputRegister(1), i.InputRegister(1), 16 - 1);
        __ vsrl_vx(i.OutputSimd128Register(), i.InputSimd128Register(0),
                   i.InputRegister(1));
      } else {
        __ vsrl_vi(i.OutputSimd128Register(), i.InputSimd128Register(0),
                   i.InputInt5(1) % 16);
      }
      break;
    }
    case kRiscvI32x4Mul: {
      __ VU.set(kScratchReg, E32, m1);
      __ vmul_vv(i.OutputSimd128Register(), i.InputSimd128Register(0),
                 i.InputSimd128Register(1));
      break;
    }
    case kRiscvI32x4TruncSatF64x2SZero: {
      __ VU.set(kScratchReg, E64, m1);
      __ vmv_vx(kSimd128ScratchReg, zero_reg);
      __ vmfeq_vv(v0, i.InputSimd128Register(0), i.InputSimd128Register(0));
      __ vmv_vv(kSimd128ScratchReg3, i.InputSimd128Register(0));
      __ VU.set(kScratchReg, E32, m1);
      __ VU.set(RoundingMode::RTZ);
      __ vfncvt_x_f_w(kSimd128ScratchReg, kSimd128ScratchReg3, MaskType::Mask);
      __ vmv_vv(i.OutputSimd128Register(), kSimd128ScratchReg);
      break;
    }
    case kRiscvI32x4TruncSatF64x2UZero: {
      __ VU.set(kScratchReg, E64, m1);
      __ vmv_vx(kSimd128ScratchReg, zero_reg);
      __ vmfeq_vv(v0, i.InputSimd128Register(0), i.InputSimd128Register(0));
      __ vmv_vv(kSimd128ScratchReg3, i.InputSimd128Register(0));
      __ VU.set(kScratchReg, E32, m1);
      __ VU.set(RoundingMode::RTZ);
      __ vfncvt_xu_f_w(kSimd128ScratchReg, kSimd128ScratchReg3, MaskType::Mask);
      __ vmv_vv(i.OutputSimd128Register(), kSimd128ScratchReg);
      break;
    }
    case kRiscvI32x4ShrU: {
      __ VU.set(kScratchReg, E32, m1);
      if (instr->InputAt(1)->IsRegister()) {
        __ andi(i.InputRegister(1), i.InputRegister(1), 32 - 1);
        __ vsrl_vx(i.OutputSimd128Register(), i.InputSimd128Register(0),
                   i.InputRegister(1));
      } else {
        __ vsrl_vi(i.OutputSimd128Register(), i.InputSimd128Register(0),
                   i.InputInt5(1) % 32);
      }
      break;
    }
    case kRiscvI64x2ShrU: {
      __ VU.set(kScratchReg, E64, m1);
      if (instr->InputAt(1)->IsRegister()) {
        __ andi(i.InputRegister(1), i.InputRegister(1), 64 - 1);
        __ vsrl_vx(i.OutputSimd128Register(), i.InputSimd128Register(0),
                   i.InputRegister(1));
      } else {
        if (is_uint5(i.InputInt6(1) % 64)) {
          __ vsrl_vi(i.OutputSimd128Register(), i.InputSimd128Register(0),
                     i.InputInt6(1) % 64);
        } else {
          __ li(kScratchReg, i.InputInt6(1) % 64);
          __ vsrl_vx(i.OutputSimd128Register(), i.InputSimd128Register(0),
                     kScratchReg);
        }
      }
      break;
    }
    case kRiscvI8x16ShrS: {
      __ VU.set(kScratchReg, E8, m1);
      if (instr->InputAt(1)->IsRegister()) {
        __ andi(i.InputRegister(1), i.InputRegister(1), 8 - 1);
        __ vsra_vx(i.OutputSimd128Register(), i.InputSimd128Register(0),
                   i.InputRegister(1));
      } else {
        __ vsra_vi(i.OutputSimd128Register(), i.InputSimd128Register(0),
                   i.InputInt5(1) % 8);
      }
      break;
    }
    case kRiscvI16x8ShrS: {
      __ VU.set(kScratchReg, E16, m1);
      if (instr->InputAt(1)->IsRegister()) {
        __ andi(i.InputRegister(1), i.InputRegister(1), 16 - 1);
        __ vsra_vx(i.OutputSimd128Register(), i.InputSimd128Register(0),
                   i.InputRegister(1));
      } else {
        __ vsra_vi(i.OutputSimd128Register(), i.InputSimd128Register(0),
                   i.InputInt5(1) % 16);
      }
      break;
    }
    case kRiscvI32x4ShrS: {
      __ VU.set(kScratchReg, E32, m1);
      if (instr->InputAt(1)->IsRegister()) {
        __ andi(i.InputRegister(1), i.InputRegister(1), 32 - 1);
        __ vsra_vx(i.OutputSimd128Register(), i.InputSimd128Register(0),
                   i.InputRegister(1));
      } else {
        __ vsra_vi(i.OutputSimd128Register(), i.InputSimd128Register(0),
                   i.InputInt5(1) % 32);
      }
      break;
    }
    case kRiscvI64x2ShrS: {
      __ VU.set(kScratchReg, E64, m1);
      if (instr->InputAt(1)->IsRegister()) {
        __ andi(i.InputRegister(1), i.InputRegister(1), 64 - 1);
        __ vsra_vx(i.OutputSimd128Register(), i.InputSimd128Register(0),
                   i.InputRegister(1));
      } else {
        if (is_uint5(i.InputInt6(1) % 64)) {
          __ vsra_vi(i.OutputSimd128Register(), i.InputSimd128Register(0),
                     i.InputInt6(1) % 64);
        } else {
          __ li(kScratchReg, i.InputInt6(1) % 64);
          __ vsra_vx(i.OutputSimd128Register(), i.InputSimd128Register(0),
                     kScratchReg);
        }
      }
      break;
    }
    case kRiscvI32x4ExtractLane: {
      __ WasmRvvExtractLane(i.OutputRegister(), i.InputSimd128Register(0),
                            i.InputInt8(1), E32, m1);
      break;
    }
    case kRiscvI32x4Abs: {
      __ VU.set(kScratchReg, E32, m1);
      __ vmv_vv(i.OutputSimd128Register(), i.InputSimd128Register(0));
      __ vmv_vx(kSimd128RegZero, zero_reg);
      __ vmslt_vv(v0, i.InputSimd128Register(0), kSimd128RegZero);
      __ vneg_vv(i.OutputSimd128Register(), i.InputSimd128Register(0),
                 MaskType::Mask);
      break;
    }
    case kRiscvI16x8Abs: {
      __ VU.set(kScratchReg, E16, m1);
      __ vmv_vv(i.OutputSimd128Register(), i.InputSimd128Register(0));
      __ vmv_vx(kSimd128RegZero, zero_reg);
      __ vmslt_vv(v0, i.InputSimd128Register(0), kSimd128RegZero);
      __ vneg_vv(i.OutputSimd128Register(), i.InputSimd128Register(0),
                 MaskType::Mask);
      break;
    }
    case kRiscvI8x16Abs: {
      __ VU.set(kScratchReg, E8, m1);
      __ vmv_vx(kSimd128RegZero, zero_reg);
      __ vmv_vv(i.OutputSimd128Register(), i.InputSimd128Register(0));
      __ vmslt_vv(v0, i.InputSimd128Register(0), kSimd128RegZero);
      __ vneg_vv(i.OutputSimd128Register(), i.InputSimd128Register(0),
                 MaskType::Mask);
      break;
    }
    case kRiscvI64x2Abs: {
      __ VU.set(kScratchReg, E64, m1);
      __ vmv_vv(i.OutputSimd128Register(), i.InputSimd128Register(0));
      __ vmv_vx(kSimd128RegZero, zero_reg);
      __ vmslt_vv(v0, i.InputSimd128Register(0), kSimd128RegZero);
      __ vneg_vv(i.OutputSimd128Register(), i.InputSimd128Register(0),
                 MaskType::Mask);
      break;
    }
    case kRiscvI64x2ExtractLane: {
      __ WasmRvvExtractLane(i.OutputRegister(), i.InputSimd128Register(0),
                            i.InputInt8(1), E64, m1);
      break;
    }
    case kRiscvI8x16Eq: {
      __ WasmRvvEq(i.OutputSimd128Register(), i.InputSimd128Register(0),
                   i.InputSimd128Register(1), E8, m1);
      break;
    }
    case kRiscvI16x8Eq: {
      __ WasmRvvEq(i.OutputSimd128Register(), i.InputSimd128Register(0),
                   i.InputSimd128Register(1), E16, m1);
      break;
    }
    case kRiscvI32x4Eq: {
      __ WasmRvvEq(i.OutputSimd128Register(), i.InputSimd128Register(0),
                   i.InputSimd128Register(1), E32, m1);
      break;
    }
    case kRiscvI64x2Eq: {
      __ WasmRvvEq(i.OutputSimd128Register(), i.InputSimd128Register(0),
                   i.InputSimd128Register(1), E64, m1);
      break;
    }
    case kRiscvI8x16Ne: {
      __ WasmRvvNe(i.OutputSimd128Register(), i.InputSimd128Register(0),
                   i.InputSimd128Register(1), E8, m1);
      break;
    }
    case kRiscvI16x8Ne: {
      __ WasmRvvNe(i.OutputSimd128Register(), i.InputSimd128Register(0),
                   i.InputSimd128Register(1), E16, m1);
      break;
    }
    case kRiscvI32x4Ne: {
      __ WasmRvvNe(i.OutputSimd128Register(), i.InputSimd128Register(0),
                   i.InputSimd128Register(1), E32, m1);
      break;
    }
    case kRiscvI64x2Ne: {
      __ WasmRvvNe(i.OutputSimd128Register(), i.InputSimd128Register(0),
                   i.InputSimd128Register(1), E64, m1);
      break;
    }
    case kRiscvI8x16GeS: {
      __ WasmRvvGeS(i.OutputSimd128Register(), i.InputSimd128Register(0),
                    i.InputSimd128Register(1), E8, m1);
      break;
    }
    case kRiscvI16x8GeS: {
      __ WasmRvvGeS(i.OutputSimd128Register(), i.InputSimd128Register(0),
                    i.InputSimd128Register(1), E16, m1);
      break;
    }
    case kRiscvI32x4GeS: {
      __ WasmRvvGeS(i.OutputSimd128Register(), i.InputSimd128Register(0),
                    i.InputSimd128Register(1), E32, m1);
      break;
    }
    case kRiscvI64x2GeS: {
      __ WasmRvvGeS(i.OutputSimd128Register(), i.InputSimd128Register(0),
                    i.InputSimd128Register(1), E64, m1);
      break;
    }
    case kRiscvI8x16GeU: {
      __ WasmRvvGeU(i.OutputSimd128Register(), i.InputSimd128Register(0),
                    i.InputSimd128Register(1), E8, m1);
      break;
    }
    case kRiscvI16x8GeU: {
      __ WasmRvvGeU(i.OutputSimd128Register(), i.InputSimd128Register(0),
                    i.InputSimd128Register(1), E16, m1);
      break;
    }
    case kRiscvI32x4GeU: {
      __ WasmRvvGeU(i.OutputSimd128Register(), i.InputSimd128Register(0),
                    i.InputSimd128Register(1), E32, m1);
      break;
    }
    case kRiscvI8x16GtS: {
      __ WasmRvvGtS(i.OutputSimd128Register(), i.InputSimd128Register(0),
                    i.InputSimd128Register(1), E8, m1);
      break;
    }
    case kRiscvI16x8GtS: {
      __ WasmRvvGtS(i.OutputSimd128Register(), i.InputSimd128Register(0),
                    i.InputSimd128Register(1), E16, m1);
      break;
    }
    case kRiscvI32x4GtS: {
      __ WasmRvvGtS(i.OutputSimd128Register(), i.InputSimd128Register(0),
                    i.InputSimd128Register(1), E32, m1);
      break;
    }
    case kRiscvI64x2GtS: {
      __ WasmRvvGtS(i.OutputSimd128Register(), i.InputSimd128Register(0),
                    i.InputSimd128Register(1), E64, m1);
      break;
    }
    case kRiscvI8x16GtU: {
      __ WasmRvvGtU(i.OutputSimd128Register(), i.InputSimd128Register(0),
                    i.InputSimd128Register(1), E8, m1);
      break;
    }
    case kRiscvI16x8GtU: {
      __ WasmRvvGtU(i.OutputSimd128Register(), i.InputSimd128Register(0),
                    i.InputSimd128Register(1), E16, m1);
      break;
    }
    case kRiscvI32x4GtU: {
      __ WasmRvvGtU(i.OutputSimd128Register(), i.InputSimd128Register(0),
                    i.InputSimd128Register(1), E32, m1);
      break;
    }
    case kRiscvI8x16Shl: {
      __ VU.set(kScratchReg, E8, m1);
      if (instr->InputAt(1)->IsRegister()) {
        __ andi(i.InputRegister(1), i.InputRegister(1), 8 - 1);
        __ vsll_vx(i.OutputSimd128Register(), i.InputSimd128Register(0),
                   i.InputRegister(1));
      } else {
        __ vsll_vi(i.OutputSimd128Register(), i.InputSimd128Register(0),
                   i.InputInt5(1) % 8);
      }
      break;
    }
    case kRiscvI16x8Shl: {
      __ VU.set(kScratchReg, E16, m1);
      if (instr->InputAt(1)->IsRegister()) {
        __ andi(i.InputRegister(1), i.InputRegister(1), 16 - 1);
        __ vsll_vx(i.OutputSimd128Register(), i.InputSimd128Register(0),
                   i.InputRegister(1));
      } else {
        __ vsll_vi(i.OutputSimd128Register(), i.InputSimd128Register(0),
                   i.InputInt5(1) % 16);
      }
      break;
    }
    case kRiscvI32x4Shl: {
      __ VU.set(kScratchReg, E32, m1);
      if (instr->InputAt(1)->IsRegister()) {
        __ andi(i.InputRegister(1), i.InputRegister(1), 32 - 1);
        __ vsll_vx(i.OutputSimd128Register(), i.InputSimd128Register(0),
                   i.InputRegister(1));
      } else {
        __ vsll_vi(i.OutputSimd128Register(), i.InputSimd128Register(0),
                   i.InputInt5(1) % 32);
      }
      break;
    }
    case kRiscvI64x2Shl: {
      __ VU.set(kScratchReg, E64, m1);
      if (instr->InputAt(1)->IsRegister()) {
        __ andi(i.InputRegister(1), i.InputRegister(1), 64 - 1);
        __ vsll_vx(i.OutputSimd128Register(), i.InputSimd128Register(0),
                   i.InputRegister(1));
      } else {
        if (is_int5(i.InputInt6(1) % 64)) {
          __ vsll_vi(i.OutputSimd128Register(), i.InputSimd128Register(0),
                     i.InputInt6(1) % 64);
        } else {
          __ li(kScratchReg, i.InputInt6(1) % 64);
          __ vsll_vx(i.OutputSimd128Register(), i.InputSimd128Register(0),
                     kScratchReg);
        }
      }
      break;
    }
    case kRiscvI8x16ReplaceLane: {
      Simd128Register src = i.InputSimd128Register(0);
      Simd128Register dst = i.OutputSimd128Register();
      __ VU.set(kScratchReg, E64, m1);
      __ li(kScratchReg, 0x1 << i.InputInt8(1));
      __ vmv_sx(v0, kScratchReg);
      __ VU.set(kScratchReg, E8, m1);
      __ vmerge_vx(dst, i.InputRegister(2), src);
      break;
    }
    case kRiscvI16x8ReplaceLane: {
      Simd128Register src = i.InputSimd128Register(0);
      Simd128Register dst = i.OutputSimd128Register();
      __ VU.set(kScratchReg, E16, m1);
      __ li(kScratchReg, 0x1 << i.InputInt8(1));
      __ vmv_sx(v0, kScratchReg);
      __ vmerge_vx(dst, i.InputRegister(2), src);
      break;
    }
    case kRiscvI64x2ReplaceLane: {
      Simd128Register src = i.InputSimd128Register(0);
      Simd128Register dst = i.OutputSimd128Register();
      __ VU.set(kScratchReg, E64, m1);
      __ li(kScratchReg, 0x1 << i.InputInt8(1));
      __ vmv_sx(v0, kScratchReg);
      __ vmerge_vx(dst, i.InputRegister(2), src);
      break;
    }
    case kRiscvI32x4ReplaceLane: {
      Simd128Register src = i.InputSimd128Register(0);
      Simd128Register dst = i.OutputSimd128Register();
      __ VU.set(kScratchReg, E32, m1);
      __ li(kScratchReg, 0x1 << i.InputInt8(1));
      __ vmv_sx(v0, kScratchReg);
      __ vmerge_vx(dst, i.InputRegister(2), src);
      break;
    }
    case kRiscvI8x16BitMask: {
      Register dst = i.OutputRegister();
      Simd128Register src = i.InputSimd128Register(0);
      __ VU.set(kScratchReg, E8, m1);
      __ vmv_vx(kSimd128RegZero, zero_reg);
      __ vmslt_vv(kSimd128ScratchReg, src, kSimd128RegZero);
      __ VU.set(kScratchReg, E32, m1);
      __ vmv_xs(dst, kSimd128ScratchReg);
      break;
    }
    case kRiscvI16x8BitMask: {
      Register dst = i.OutputRegister();
      Simd128Register src = i.InputSimd128Register(0);
      __ VU.set(kScratchReg, E16, m1);
      __ vmv_vx(kSimd128RegZero, zero_reg);
      __ vmslt_vv(kSimd128ScratchReg, src, kSimd128RegZero);
      __ VU.set(kScratchReg, E32, m1);
      __ vmv_xs(dst, kSimd128ScratchReg);
      break;
    }
    case kRiscvI32x4BitMask: {
      Register dst = i.OutputRegister();
      Simd128Register src = i.InputSimd128Register(0);
      __ VU.set(kScratchReg, E32, m1);
      __ vmv_vx(kSimd128RegZero, zero_reg);
      __ vmslt_vv(kSimd128ScratchReg, src, kSimd128RegZero);
      __ vmv_xs(dst, kSimd128ScratchReg);
      break;
    }
    case kRiscvI64x2BitMask: {
      Register dst = i.OutputRegister();
      Simd128Register src = i.InputSimd128Register(0);
      __ VU.set(kScratchReg, E64, m1);
      __ vmv_vx(kSimd128RegZero, zero_reg);
      __ vmslt_vv(kSimd128ScratchReg, src, kSimd128RegZero);
      __ VU.set(kScratchReg, E32, m1);
      __ vmv_xs(dst, kSimd128ScratchReg);
      break;
    }
    case kRiscvV128AnyTrue: {
      __ VU.set(kScratchReg, E8, m1);
      Register dst = i.OutputRegister();
      Label t;
      __ vmv_sx(kSimd128ScratchReg, zero_reg);
      __ vredmaxu_vs(kSimd128ScratchReg, i.InputSimd128Register(0),
                     kSimd128ScratchReg);
      __ vmv_xs(dst, kSimd128ScratchReg);
      __ beq(dst, zero_reg, &t);
      __ li(dst, 1);
      __ bind(&t);
      break;
    }
    case kRiscvI64x2AllTrue: {
      __ VU.set(kScratchReg, E64, m1);
      Register dst = i.OutputRegister();
      Label all_true;
      __ li(kScratchReg, -1);
      __ vmv_sx(kSimd128ScratchReg, kScratchReg);
      __ vredminu_vs(kSimd128ScratchReg, i.InputSimd128Register(0),
                     kSimd128ScratchReg);
      __ vmv_xs(dst, kSimd128ScratchReg);
      __ beqz(dst, &all_true);
      __ li(dst, 1);
      __ bind(&all_true);
      break;
    }
    case kRiscvI32x4AllTrue: {
      __ VU.set(kScratchReg, E32, m1);
      Register dst = i.OutputRegister();
      Label all_true;
      __ li(kScratchReg, -1);
      __ vmv_sx(kSimd128ScratchReg, kScratchReg);
      __ vredminu_vs(kSimd128ScratchReg, i.InputSimd128Register(0),
                     kSimd128ScratchReg);
      __ vmv_xs(dst, kSimd128ScratchReg);
      __ beqz(dst, &all_true);
      __ li(dst, 1);
      __ bind(&all_true);
      break;
    }
    case kRiscvI16x8AllTrue: {
      __ VU.set(kScratchReg, E16, m1);
      Register dst = i.OutputRegister();
      Label all_true;
      __ li(kScratchReg, -1);
      __ vmv_sx(kSimd128ScratchReg, kScratchReg);
      __ vredminu_vs(kSimd128ScratchReg, i.InputSimd128Register(0),
                     kSimd128ScratchReg);
      __ vmv_xs(dst, kSimd128ScratchReg);
      __ beqz(dst, &all_true);
      __ li(dst, 1);
      __ bind(&all_true);
      break;
    }
    case kRiscvI8x16AllTrue: {
      __ VU.set(kScratchReg, E8, m1);
      Register dst = i.OutputRegister();
      Label all_true;
      __ li(kScratchReg, -1);
      __ vmv_sx(kSimd128ScratchReg, kScratchReg);
      __ vredminu_vs(kSimd128ScratchReg, i.InputSimd128Register(0),
                     kSimd128ScratchReg);
      __ vmv_xs(dst, kSimd128ScratchReg);
      __ beqz(dst, &all_true);
      __ li(dst, 1);
      __ bind(&all_true);
      break;
    }
    case kRiscvI8x16Shuffle: {
      VRegister dst = i.OutputSimd128Register(),
                src0 = i.InputSimd128Register(0),
                src1 = i.InputSimd128Register(1);

      int64_t imm1 = make_uint64(i.InputInt32(3), i.InputInt32(2));
      int64_t imm2 = make_uint64(i.InputInt32(5), i.InputInt32(4));
      __ VU.set(kScratchReg, VSew::E64, Vlmul::m1);
      __ li(kScratchReg, imm2);
      __ vmv_sx(kSimd128ScratchReg2, kScratchReg);
      __ vslideup_vi(kSimd128ScratchReg, kSimd128ScratchReg2, 1);
      __ li(kScratchReg, imm1);
      __ vmv_sx(kSimd128ScratchReg, kScratchReg);

      __ VU.set(kScratchReg, E8, m1);
      if (dst == src0) {
        __ vmv_vv(kSimd128ScratchReg2, src0);
        src0 = kSimd128ScratchReg2;
      } else if (dst == src1) {
        __ vmv_vv(kSimd128ScratchReg2, src1);
        src1 = kSimd128ScratchReg2;
      }
      __ vrgather_vv(dst, src0, kSimd128ScratchReg);
      __ vadd_vi(kSimd128ScratchReg, kSimd128ScratchReg, -16);
      __ vrgather_vv(kSimd128ScratchReg3, src1, kSimd128ScratchReg);
      __ vor_vv(dst, dst, kSimd128ScratchReg3);
      break;
    }
    case kRiscvI8x16Popcnt: {
      VRegister dst = i.OutputSimd128Register(),
                src = i.InputSimd128Register(0);
      Label t;

      __ VU.set(kScratchReg, E8, m1);
      __ vmv_vv(kSimd128ScratchReg, src);
      __ vmv_vv(dst, kSimd128RegZero);

      __ bind(&t);
      __ vmsne_vv(v0, kSimd128ScratchReg, kSimd128RegZero);
      __ vadd_vi(dst, dst, 1, Mask);
      __ vadd_vi(kSimd128ScratchReg2, kSimd128ScratchReg, -1, Mask);
      __ vand_vv(kSimd128ScratchReg, kSimd128ScratchReg, kSimd128ScratchReg2);
      // kScratchReg = -1 if kSimd128ScratchReg == 0 i.e. no active element
      __ vfirst_m(kScratchReg, kSimd128ScratchReg);
      __ bgez(kScratchReg, &t);
      break;
    }
    case kRiscvF64x2NearestInt: {
      __ Round_d(i.OutputSimd128Register(), i.InputSimd128Register(0),
                 kScratchReg, kSimd128ScratchReg);
      break;
    }
    case kRiscvF64x2Trunc: {
      __ Trunc_d(i.OutputSimd128Register(), i.InputSimd128Register(0),
                 kScratchReg, kSimd128ScratchReg);
      break;
    }
    case kRiscvF64x2Sqrt: {
      __ VU.set(kScratchReg, E64, m1);
      __ vfsqrt_v(i.OutputSimd128Register(), i.InputSimd128Register(0));
      break;
    }
    case kRiscvF64x2Splat: {
      (__ VU).set(kScratchReg, E64, m1);
      __ fmv_x_d(kScratchReg, i.InputDoubleRegister(0));
      __ vmv_vx(i.OutputSimd128Register(), kScratchReg);
      break;
    }
    case kRiscvF64x2Abs: {
      __ VU.set(kScratchReg, VSew::E64, Vlmul::m1);
      __ vfabs_vv(i.OutputSimd128Register(), i.InputSimd128Register(0));
      break;
    }
    case kRiscvF64x2Neg: {
      __ VU.set(kScratchReg, VSew::E64, Vlmul::m1);
      __ vfneg_vv(i.OutputSimd128Register(), i.InputSimd128Register(0));
      break;
    }
    case kRiscvF64x2Add: {
      __ VU.set(kScratchReg, E64, m1);
      __ vfadd_vv(i.OutputSimd128Register(), i.InputSimd128Register(0),
                  i.InputSimd128Register(1));
      break;
    }
    case kRiscvF64x2Sub: {
      __ VU.set(kScratchReg, E64, m1);
      __ vfsub_vv(i.OutputSimd128Register(), i.InputSimd128Register(0),
                  i.InputSimd128Register(1));
      break;
    }
    case kRiscvF64x2Ceil: {
      __ Ceil_d(i.OutputSimd128Register(), i.InputSimd128Register(0),
                kScratchReg, kSimd128ScratchReg);
      break;
    }
    case kRiscvF64x2Floor: {
      __ Floor_d(i.OutputSimd128Register(), i.InputSimd128Register(0),
                 kScratchReg, kSimd128ScratchReg);
      break;
    }
    case kRiscvF64x2Ne: {
      __ VU.set(kScratchReg, E64, m1);
      __ vmfne_vv(v0, i.InputSimd128Register(1), i.InputSimd128Register(0));
      __ vmv_vx(i.OutputSimd128Register(), zero_reg);
      __ vmerge_vi(i.OutputSimd128Register(), -1, i.OutputSimd128Register());
      break;
    }
    case kRiscvF64x2Eq: {
      __ VU.set(kScratchReg, E64, m1);
      __ vmfeq_vv(v0, i.InputSimd128Register(1), i.InputSimd128Register(0));
      __ vmv_vx(i.OutputSimd128Register(), zero_reg);
      __ vmerge_vi(i.OutputSimd128Register(), -1, i.OutputSimd128Register());
      break;
    }
    case kRiscvF64x2ReplaceLane: {
      __ VU.set(kScratchReg, E64, m1);
      __ li(kScratchReg, 0x1 << i.InputInt8(1));
      __ vmv_sx(v0, kScratchReg);
      __ fmv_x_d(kScratchReg, i.InputSingleRegister(2));
      __ vmerge_vx(i.OutputSimd128Register(), kScratchReg,
                   i.InputSimd128Register(0));
      break;
    }
    case kRiscvF64x2Lt: {
      __ VU.set(kScratchReg, E64, m1);
      __ vmflt_vv(v0, i.InputSimd128Register(0), i.InputSimd128Register(1));
      __ vmv_vx(i.OutputSimd128Register(), zero_reg);
      __ vmerge_vi(i.OutputSimd128Register(), -1, i.OutputSimd128Register());
      break;
    }
    case kRiscvF64x2Le: {
      __ VU.set(kScratchReg, E64, m1);
      __ vmfle_vv(v0, i.InputSimd128Register(0), i.InputSimd128Register(1));
      __ vmv_vx(i.OutputSimd128Register(), zero_reg);
      __ vmerge_vi(i.OutputSimd128Register(), -1, i.OutputSimd128Register());
      break;
    }
    case kRiscvF64x2Pmax: {
      __ VU.set(kScratchReg, E64, m1);
      __ vmflt_vv(v0, i.InputSimd128Register(0), i.InputSimd128Register(1));
      __ vmerge_vv(i.OutputSimd128Register(), i.InputSimd128Register(1),
                   i.InputSimd128Register(0));
      break;
    }
    case kRiscvF64x2Pmin: {
      __ VU.set(kScratchReg, E64, m1);
      __ vmflt_vv(v0, i.InputSimd128Register(1), i.InputSimd128Register(0));
      __ vmerge_vv(i.OutputSimd128Register(), i.InputSimd128Register(1),
                   i.InputSimd128Register(0));
      break;
    }
    case kRiscvF64x2Min: {
      __ VU.set(kScratchReg, E64, m1);
      const int64_t kNaN = 0x7ff8000000000000L;
      __ vmfeq_vv(v0, i.InputSimd128Register(0), i.InputSimd128Register(0));
      __ vmfeq_vv(kSimd128ScratchReg, i.InputSimd128Register(1),
                  i.InputSimd128Register(1));
      __ vand_vv(v0, v0, kSimd128ScratchReg);
      __ li(kScratchReg, kNaN);
      __ vmv_vx(kSimd128ScratchReg, kScratchReg);
      __ vfmin_vv(kSimd128ScratchReg, i.InputSimd128Register(1),
                  i.InputSimd128Register(0), Mask);
      __ vmv_vv(i.OutputSimd128Register(), kSimd128ScratchReg);
      break;
    }
    case kRiscvF64x2Max: {
      __ VU.set(kScratchReg, E64, m1);
      const int64_t kNaN = 0x7ff8000000000000L;
      __ vmfeq_vv(v0, i.InputSimd128Register(0), i.InputSimd128Register(0));
      __ vmfeq_vv(kSimd128ScratchReg, i.InputSimd128Register(1),
                  i.InputSimd128Register(1));
      __ vand_vv(v0, v0, kSimd128ScratchReg);
      __ li(kScratchReg, kNaN);
      __ vmv_vx(kSimd128ScratchReg, kScratchReg);
      __ vfmax_vv(kSimd128ScratchReg, i.InputSimd128Register(1),
                  i.InputSimd128Register(0), Mask);
      __ vmv_vv(i.OutputSimd128Register(), kSimd128ScratchReg);
      break;
    }
    case kRiscvF64x2Div: {
      __ VU.set(kScratchReg, E64, m1);
      __ vfdiv_vv(i.OutputSimd128Register(), i.InputSimd128Register(0),
                  i.InputSimd128Register(1));
      break;
    }
    case kRiscvF64x2Mul: {
      __ VU.set(kScratchReg, E64, m1);
      __ VU.set(RoundingMode::RTZ);
      __ vfmul_vv(i.OutputSimd128Register(), i.InputSimd128Register(0),
                  i.InputSimd128Register(1));
      break;
    }
    case kRiscvF64x2ExtractLane: {
      __ VU.set(kScratchReg, E64, m1);
      if (is_uint5(i.InputInt8(1))) {
        __ vslidedown_vi(kSimd128ScratchReg, i.InputSimd128Register(0),
                         i.InputInt8(1));
      } else {
        __ li(kScratchReg, i.InputInt8(1));
        __ vslidedown_vx(kSimd128ScratchReg, i.InputSimd128Register(0),
                         kScratchReg);
      }
      __ vfmv_fs(i.OutputDoubleRegister(), kSimd128ScratchReg);
      break;
    }
    case kRiscvF64x2PromoteLowF32x4: {
      __ VU.set(kScratchReg, E32, mf2);
      if (i.OutputSimd128Register() != i.InputSimd128Register(0)) {
        __ vfwcvt_f_f_v(i.OutputSimd128Register(), i.InputSimd128Register(0));
      } else {
        __ vfwcvt_f_f_v(kSimd128ScratchReg3, i.InputSimd128Register(0));
        __ VU.set(kScratchReg, E64, m1);
        __ vmv_vv(i.OutputSimd128Register(), kSimd128ScratchReg3);
      }
      break;
    }
    case kRiscvF64x2ConvertLowI32x4S: {
      __ VU.set(kScratchReg, E32, mf2);
      if (i.OutputSimd128Register() != i.InputSimd128Register(0)) {
        __ vfwcvt_f_x_v(i.OutputSimd128Register(), i.InputSimd128Register(0));
      } else {
        __ vfwcvt_f_x_v(kSimd128ScratchReg3, i.InputSimd128Register(0));
        __ VU.set(kScratchReg, E64, m1);
        __ vmv_vv(i.OutputSimd128Register(), kSimd128ScratchReg3);
      }
      break;
    }
    case kRiscvF64x2ConvertLowI32x4U: {
      __ VU.set(kScratchReg, E32, mf2);
      if (i.OutputSimd128Register() != i.InputSimd128Register(0)) {
        __ vfwcvt_f_xu_v(i.OutputSimd128Register(), i.InputSimd128Register(0));
      } else {
        __ vfwcvt_f_xu_v(kSimd128ScratchReg3, i.InputSimd128Register(0));
        __ VU.set(kScratchReg, E64, m1);
        __ vmv_vv(i.OutputSimd128Register(), kSimd128ScratchReg3);
      }
      break;
    }
    case kRiscvF64x2Qfma: {
      __ VU.set(kScratchReg, E64, m1);
      __ vfmadd_vv(i.InputSimd128Register(1), i.InputSimd128Register(2),
                   i.InputSimd128Register(0));
      __ vmv_vv(i.OutputSimd128Register(), i.InputSimd128Register(1));
      break;
    }
    case kRiscvF64x2Qfms: {
      __ VU.set(kScratchReg, E64, m1);
      __ vfnmsub_vv(i.InputSimd128Register(1), i.InputSimd128Register(2),
                    i.InputSimd128Register(0));
      __ vmv_vv(i.OutputSimd128Register(), i.InputSimd128Register(1));
      break;
    }
    case kRiscvF32x4ExtractLane: {
      __ VU.set(kScratchReg, E32, m1);
      __ vslidedown_vi(kSimd128ScratchReg, i.InputSimd128Register(0),
                       i.InputInt8(1));
      __ vfmv_fs(i.OutputDoubleRegister(), kSimd128ScratchReg);
      break;
    }
    case kRiscvF32x4Trunc: {
      __ Trunc_f(i.OutputSimd128Register(), i.InputSimd128Register(0),
                 kScratchReg, kSimd128ScratchReg);
      break;
    }
    case kRiscvF32x4NearestInt: {
      __ Round_f(i.OutputSimd128Register(), i.InputSimd128Register(0),
                 kScratchReg, kSimd128ScratchReg);
      break;
    }
    case kRiscvF32x4DemoteF64x2Zero: {
      __ VU.set(kScratchReg, E32, mf2);
      __ vfncvt_f_f_w(i.OutputSimd128Register(), i.InputSimd128Register(0));
      __ VU.set(kScratchReg, E32, m1);
      __ vmv_vi(v0, 12);
      __ vmerge_vx(i.OutputSimd128Register(), zero_reg,
                   i.OutputSimd128Register());
      break;
    }
    case kRiscvF32x4Neg: {
      __ VU.set(kScratchReg, VSew::E32, Vlmul::m1);
      __ vfneg_vv(i.OutputSimd128Register(), i.InputSimd128Register(0));
      break;
    }
    case kRiscvF32x4Abs: {
      __ VU.set(kScratchReg, VSew::E32, Vlmul::m1);
      __ vfabs_vv(i.OutputSimd128Register(), i.InputSimd128Register(0));
      break;
    }
    case kRiscvF32x4Splat: {
      (__ VU).set(kScratchReg, E32, m1);
      __ fmv_x_w(kScratchReg, i.InputSingleRegister(0));
      __ vmv_vx(i.OutputSimd128Register(), kScratchReg);
      break;
    }
    case kRiscvF32x4Add: {
      __ VU.set(kScratchReg, E32, m1);
      __ vfadd_vv(i.OutputSimd128Register(), i.InputSimd128Register(0),
                  i.InputSimd128Register(1));
      break;
    }
    case kRiscvF32x4Sub: {
      __ VU.set(kScratchReg, E32, m1);
      __ vfsub_vv(i.OutputSimd128Register(), i.InputSimd128Register(0),
                  i.InputSimd128Register(1));
      break;
    }
    case kRiscvF32x4Ceil: {
      __ Ceil_f(i.OutputSimd128Register(), i.InputSimd128Register(0),
                kScratchReg, kSimd128ScratchReg);
      break;
    }
    case kRiscvF32x4Floor: {
      __ Floor_f(i.OutputSimd128Register(), i.InputSimd128Register(0),
                 kScratchReg, kSimd128ScratchReg);
      break;
    }
    case kRiscvF32x4UConvertI32x4: {
      __ VU.set(kScratchReg, E32, m1);
      __ VU.set(RoundingMode::RTZ);
      __ vfcvt_f_xu_v(i.OutputSimd128Register(), i.InputSimd128Register(0));
      break;
    }
    case kRiscvF32x4SConvertI32x4: {
      __ VU.set(kScratchReg, E32, m1);
      __ VU.set(RoundingMode::RTZ);
      __ vfcvt_f_x_v(i.OutputSimd128Register(), i.InputSimd128Register(0));
      break;
    }
    case kRiscvF32x4Div: {
      __ VU.set(kScratchReg, E32, m1);
      __ VU.set(RoundingMode::RTZ);
      __ vfdiv_vv(i.OutputSimd128Register(), i.InputSimd128Register(0),
                  i.InputSimd128Register(1));
      break;
    }
    case kRiscvF32x4Mul: {
      __ VU.set(kScratchReg, E32, m1);
      __ VU.set(RoundingMode::RTZ);
      __ vfmul_vv(i.OutputSimd128Register(), i.InputSimd128Register(1),
                  i.InputSimd128Register(0));
      break;
    }
    case kRiscvF32x4Eq: {
      __ VU.set(kScratchReg, E32, m1);
      __ vmfeq_vv(v0, i.InputSimd128Register(1), i.InputSimd128Register(0));
      __ vmv_vx(i.OutputSimd128Register(), zero_reg);
      __ vmerge_vi(i.OutputSimd128Register(), -1, i.OutputSimd128Register());
      break;
    }
    case kRiscvF32x4Ne: {
      __ VU.set(kScratchReg, E32, m1);
      __ vmfne_vv(v0, i.InputSimd128Register(1), i.InputSimd128Register(0));
      __ vmv_vx(i.OutputSimd128Register(), zero_reg);
      __ vmerge_vi(i.OutputSimd128Register(), -1, i.OutputSimd128Register());
      break;
    }
    case kRiscvF32x4ReplaceLane: {
      __ VU.set(kScratchReg, E32, m1);
      __ li(kScratchReg, 0x1 << i.InputInt8(1));
      __ vmv_sx(v0, kScratchReg);
      __ fmv_x_w(kScratchReg, i.InputSingleRegister(2));
      __ vmerge_vx(i.OutputSimd128Register(), kScratchReg,
                   i.InputSimd128Register(0));
      break;
    }
    case kRiscvF32x4Lt: {
      __ VU.set(kScratchReg, E32, m1);
      __ vmflt_vv(v0, i.InputSimd128Register(0), i.InputSimd128Register(1));
      __ vmv_vx(i.OutputSimd128Register(), zero_reg);
      __ vmerge_vi(i.OutputSimd128Register(), -1, i.OutputSimd128Register());
      break;
    }
    case kRiscvF32x4Le: {
      __ VU.set(kScratchReg, E32, m1);
      __ vmfle_vv(v0, i.InputSimd128Register(0), i.InputSimd128Register(1));
      __ vmv_vx(i.OutputSimd128Register(), zero_reg);
      __ vmerge_vi(i.OutputSimd128Register(), -1, i.OutputSimd128Register());
      break;
    }
    case kRiscvF32x4Pmax: {
      __ VU.set(kScratchReg, E32, m1);
      __ vmflt_vv(v0, i.InputSimd128Register(0), i.InputSimd128Register(1));
      __ vmerge_vv(i.OutputSimd128Register(), i.InputSimd128Register(1),
                   i.InputSimd128Register(0));
      break;
    }
    case kRiscvF32x4Pmin: {
      __ VU.set(kScratchReg, E32, m1);
      __ vmflt_vv(v0, i.InputSimd128Register(1), i.InputSimd128Register(0));
      __ vmerge_vv(i.OutputSimd128Register(), i.InputSimd128Register(1),
                   i.InputSimd128Register(0));
      break;
    }
    case kRiscvF32x4Sqrt: {
      __ VU.set(kScratchReg, E32, m1);
      __ vfsqrt_v(i.OutputSimd128Register(), i.InputSimd128Register(0));
      break;
    }
    case kRiscvF32x4Max: {
      __ VU.set(kScratchReg, E32, m1);
      const int32_t kNaN = 0x7FC00000;
      __ vmfeq_vv(v0, i.InputSimd128Register(0), i.InputSimd128Register(0));
      __ vmfeq_vv(kSimd128ScratchReg, i.InputSimd128Register(1),
                  i.InputSimd128Register(1));
      __ vand_vv(v0, v0, kSimd128ScratchReg);
      __ li(kScratchReg, kNaN);
      __ vmv_vx(kSimd128ScratchReg, kScratchReg);
      __ vfmax_vv(kSimd128ScratchReg, i.InputSimd128Register(1),
                  i.InputSimd128Register(0), Mask);
      __ vmv_vv(i.OutputSimd128Register(), kSimd128ScratchReg);
      break;
    }
    case kRiscvF32x4Min: {
      __ VU.set(kScratchReg, E32, m1);
      const int32_t kNaN = 0x7FC00000;
      __ vmfeq_vv(v0, i.InputSimd128Register(0), i.InputSimd128Register(0));
      __ vmfeq_vv(kSimd128ScratchReg, i.InputSimd128Register(1),
                  i.InputSimd128Register(1));
      __ vand_vv(v0, v0, kSimd128ScratchReg);
      __ li(kScratchReg, kNaN);
      __ vmv_vx(kSimd128ScratchReg, kScratchReg);
      __ vfmin_vv(kSimd128ScratchReg, i.InputSimd128Register(1),
                  i.InputSimd128Register(0), Mask);
      __ vmv_vv(i.OutputSimd128Register(), kSimd128ScratchReg);
      break;
    }
    case kRiscvF32x4RecipApprox: {
      __ VU.set(kScratchReg, E32, m1);
      __ vfrec7_v(i.OutputSimd128Register(), i.InputSimd128Register(0));
      break;
    }
    case kRiscvF32x4RecipSqrtApprox: {
      __ VU.set(kScratchReg, E32, m1);
      __ vfrsqrt7_v(i.OutputSimd128Register(), i.InputSimd128Register(0));
      break;
    }
    case kRiscvF32x4Qfma: {
      __ VU.set(kScratchReg, E32, m1);
      __ vfmadd_vv(i.InputSimd128Register(1), i.InputSimd128Register(2),
                   i.InputSimd128Register(0));
      __ vmv_vv(i.OutputSimd128Register(), i.InputSimd128Register(1));
      break;
    }
    case kRiscvF32x4Qfms: {
      __ VU.set(kScratchReg, E32, m1);
      __ vfnmsub_vv(i.InputSimd128Register(1), i.InputSimd128Register(2),
                    i.InputSimd128Register(0));
      __ vmv_vv(i.OutputSimd128Register(), i.InputSimd128Register(1));
      break;
    }
    case kRiscvI64x2SConvertI32x4Low: {
      __ VU.set(kScratchReg, E64, m1);
      __ vmv_vv(kSimd128ScratchReg, i.InputSimd128Register(0));
      __ vsext_vf2(i.OutputSimd128Register(), kSimd128ScratchReg);

      break;
    }
    case kRiscvI64x2SConvertI32x4High: {
      __ VU.set(kScratchReg, E32, m1);
      __ vslidedown_vi(kSimd128ScratchReg, i.InputSimd128Register(0), 2);
      __ VU.set(kScratchReg, E64, m1);
      __ vsext_vf2(i.OutputSimd128Register(), kSimd128ScratchReg);
      break;
    }
    case kRiscvI64x2UConvertI32x4Low: {
      __ VU.set(kScratchReg, E64, m1);
      __ vmv_vv(kSimd128ScratchReg, i.InputSimd128Register(0));
      __ vzext_vf2(i.OutputSimd128Register(), kSimd128ScratchReg);
      break;
    }
    case kRiscvI64x2UConvertI32x4High: {
      __ VU.set(kScratchReg, E32, m1);
      __ vslidedown_vi(kSimd128ScratchReg, i.InputSimd128Register(0), 2);
      __ VU.set(kScratchReg, E64, m1);
      __ vzext_vf2(i.OutputSimd128Register(), kSimd128ScratchReg);
      break;
    }
    case kRiscvI32x4SConvertI16x8Low: {
      __ VU.set(kScratchReg, E32, m1);
      __ vmv_vv(kSimd128ScratchReg, i.InputSimd128Register(0));
      __ vsext_vf2(i.OutputSimd128Register(), kSimd128ScratchReg);
      break;
    }
    case kRiscvI32x4SConvertI16x8High: {
      __ VU.set(kScratchReg, E16, m1);
      __ vslidedown_vi(kSimd128ScratchReg, i.InputSimd128Register(0), 4);
      __ VU.set(kScratchReg, E32, m1);
      __ vsext_vf2(i.OutputSimd128Register(), kSimd128ScratchReg);
      break;
    }
    case kRiscvI32x4SConvertF32x4: {
      __ VU.set(kScratchReg, E32, m1);
      __ VU.set(RoundingMode::RTZ);
      __ vmfeq_vv(v0, i.InputSimd128Register(0), i.InputSimd128Register(0));
      if (i.OutputSimd128Register() != i.InputSimd128Register(0)) {
        __ vmv_vx(i.OutputSimd128Register(), zero_reg);
        __ vfcvt_x_f_v(i.OutputSimd128Register(), i.InputSimd128Register(0),
                       Mask);
      } else {
        __ vmv_vx(kSimd128ScratchReg, zero_reg);
        __ vfcvt_x_f_v(kSimd128ScratchReg, i.InputSimd128Register(0), Mask);
        __ vmv_vv(i.OutputSimd128Register(), kSimd128ScratchReg);
      }
      break;
    }
    case kRiscvI32x4UConvertF32x4: {
      __ VU.set(kScratchReg, E32, m1);
      __ VU.set(RoundingMode::RTZ);
      __ vmfeq_vv(v0, i.InputSimd128Register(0), i.InputSimd128Register(0));
      if (i.OutputSimd128Register() != i.InputSimd128Register(0)) {
        __ vmv_vx(i.OutputSimd128Register(), zero_reg);
        __ vfcvt_xu_f_v(i.OutputSimd128Register(), i.InputSimd128Register(0),
                        Mask);
      } else {
        __ vmv_vx(kSimd128ScratchReg, zero_reg);
        __ vfcvt_xu_f_v(kSimd128ScratchReg, i.InputSimd128Register(0), Mask);
        __ vmv_vv(i.OutputSimd128Register(), kSimd128ScratchReg);
      }
      break;
    }
    case kRiscvI32x4UConvertI16x8Low: {
      __ VU.set(kScratchReg, E32, m1);
      __ vmv_vv(kSimd128ScratchReg, i.InputSimd128Register(0));
      __ vzext_vf2(i.OutputSimd128Register(), kSimd128ScratchReg);
      break;
    }
    case kRiscvI32x4UConvertI16x8High: {
      __ VU.set(kScratchReg, E16, m1);
      __ vslidedown_vi(kSimd128ScratchReg, i.InputSimd128Register(0), 4);
      __ VU.set(kScratchReg, E32, m1);
      __ vzext_vf2(i.OutputSimd128Register(), kSimd128ScratchReg);
      break;
    }
    case kRiscvI16x8SConvertI8x16Low: {
      __ VU.set(kScratchReg, E16, m1);
      __ vmv_vv(kSimd128ScratchReg, i.InputSimd128Register(0));
      __ vsext_vf2(i.OutputSimd128Register(), kSimd128ScratchReg);
      break;
    }
    case kRiscvI16x8SConvertI8x16High: {
      __ VU.set(kScratchReg, E8, m1);
      __ vslidedown_vi(kSimd128ScratchReg, i.InputSimd128Register(0), 8);
      __ VU.set(kScratchReg, E16, m1);
      __ vsext_vf2(i.OutputSimd128Register(), kSimd128ScratchReg);
      break;
    }
    case kRiscvI16x8UConvertI8x16Low: {
      __ VU.set(kScratchReg, E16, m1);
      __ vmv_vv(kSimd128ScratchReg, i.InputSimd128Register(0));
      __ vzext_vf2(i.OutputSimd128Register(), kSimd128ScratchReg);
      break;
    }
    case kRiscvI16x8UConvertI8x16High: {
      __ VU.set(kScratchReg, E8, m1);
      __ vslidedown_vi(kSimd128ScratchReg, i.InputSimd128Register(0), 8);
      __ VU.set(kScratchReg, E16, m1);
      __ vzext_vf2(i.OutputSimd128Register(), kSimd128ScratchReg);
      break;
    }
    case kRiscvI8x16SConvertI16x8: {
      __ VU.set(kScratchReg, E16, m1);
      __ vmv_vv(v26, i.InputSimd128Register(0));
      __ vmv_vv(v27, i.InputSimd128Register(1));
      __ VU.set(kScratchReg, E8, m1);
      __ VU.set(RoundingMode::RNE);
      __ vnclip_vi(i.OutputSimd128Register(), v26, 0);
      break;
    }
    case kRiscvI8x16UConvertI16x8: {
      __ VU.set(kScratchReg, E16, m1);
      __ vmv_vv(v26, i.InputSimd128Register(0));
      __ vmv_vv(v27, i.InputSimd128Register(1));
      __ VU.set(kScratchReg, E16, m2);
      __ vmax_vx(v26, v26, zero_reg);
      __ VU.set(kScratchReg, E8, m1);
      __ VU.set(RoundingMode::RNE);
      __ vnclipu_vi(i.OutputSimd128Register(), v26, 0);
      break;
    }
    case kRiscvI16x8SConvertI32x4: {
      __ VU.set(kScratchReg, E32, m1);
      __ vmv_vv(v26, i.InputSimd128Register(0));
      __ vmv_vv(v27, i.InputSimd128Register(1));
      __ VU.set(kScratchReg, E16, m1);
      __ VU.set(RoundingMode::RNE);
      __ vnclip_vi(i.OutputSimd128Register(), v26, 0);
      break;
    }
    case kRiscvI16x8UConvertI32x4: {
      __ VU.set(kScratchReg, E32, m1);
      __ vmv_vv(v26, i.InputSimd128Register(0));
      __ vmv_vv(v27, i.InputSimd128Register(1));
      __ VU.set(kScratchReg, E32, m2);
      __ vmax_vx(v26, v26, zero_reg);
      __ VU.set(kScratchReg, E16, m1);
      __ VU.set(RoundingMode::RNE);
      __ vnclipu_vi(i.OutputSimd128Register(), v26, 0);
      break;
    }
      ASSEMBLE_RVV_UNOP_INTEGER_VV(Neg, vneg_vv)
      ASSEMBLE_RVV_BINOP_INTEGER(MaxU, vmaxu_vv)
      ASSEMBLE_RVV_BINOP_INTEGER(MaxS, vmax_vv)
      ASSEMBLE_RVV_BINOP_INTEGER(MinU, vminu_vv)
      ASSEMBLE_RVV_BINOP_INTEGER(MinS, vmin_vv)
      ASSEMBLE_RVV_UNOP_INTEGER_VR(Splat, vmv_vx)
      ASSEMBLE_RVV_BINOP_INTEGER(Add, vadd_vv)
      ASSEMBLE_RVV_BINOP_INTEGER(Sub, vsub_vv)
    case kRiscvVwadd: {
      __ VU.set(kScratchReg, i.InputInt8(2), i.InputInt8(3));
      __ vwadd_vv(i.OutputSimd128Register(), i.InputSimd128Register(0),
                  i.InputSimd128Register(1));
      break;
    }
    case kRiscvVwaddu: {
      __ VU.set(kScratchReg, i.InputInt8(2), i.InputInt8(3));
      __ vwaddu_vv(i.OutputSimd128Register(), i.InputSimd128Register(0),
                   i.InputSimd128Register(1));
      break;
    }
    case kRiscvVwmul: {
      __ VU.set(kScratchReg, i.InputInt8(2), i.InputInt8(3));
      __ vwmul_vv(i.OutputSimd128Register(), i.InputSimd128Register(0),
                  i.InputSimd128Register(1));
      break;
    }
    case kRiscvVwmulu: {
      __ VU.set(kScratchReg, i.InputInt8(2), i.InputInt8(3));
      __ vwmulu_vv(i.OutputSimd128Register(), i.InputSimd128Register(0),
                   i.InputSimd128Register(1));
      break;
    }
    case kRiscvVmvSx: {
      __ VU.set(kScratchReg, i.InputInt8(2), i.InputInt8(3));
      if (instr->InputAt(0)->IsRegister()) {
        __ vmv_sx(i.OutputSimd128Register(), i.InputRegister(0));
      } else {
        DCHECK(instr->InputAt(0)->IsImmediate());
        __ li(kScratchReg, i.InputInt64(0));
        __ vmv_sx(i.OutputSimd128Register(), kScratchReg);
      }
      break;
    }
    case kRiscvVcompress: {
      __ VU.set(kScratchReg, i.InputInt8(2), i.InputInt8(3));
      if (instr->InputAt(1)->IsSimd128Register()) {
        __ vcompress_vv(i.OutputSimd128Register(), i.InputSimd128Register(0),
                        i.InputSimd128Register(1));
      } else {
        DCHECK(instr->InputAt(1)->IsImmediate());
        __ li(kScratchReg, i.InputInt64(1));
        __ vmv_sx(v0, kScratchReg);
        __ vcompress_vv(i.OutputSimd128Register(), i.InputSimd128Register(0),
                        v0);
      }
      break;
    }
    case kRiscvVaddVv: {
      __ VU.set(kScratchReg, i.InputInt8(2), i.InputInt8(3));
      __ vadd_vv(i.OutputSimd128Register(), i.InputSimd128Register(0),
                 i.InputSimd128Register(1));
      break;
    }
    default:
#ifdef DEBUG
      switch (arch_opcode) {
#define Print(name)       \
  case k##name:           \
    printf("k%s", #name); \
    break;
        TARGET_ARCH_OPCODE_LIST(Print);
#undef Print
        default:
          break;
      }
#endif
      UNIMPLEMENTED();
  }
  return kSuccess;
}

#define UNSUPPORTED_COND(opcode, condition)                                    \
  StdoutStream{} << "Unsupported " << #opcode << " condition: \"" << condition \
                 << "\"";                                                      \
  UNIMPLEMENTED();

bool IsInludeEqual(Condition cc) {
  switch (cc) {
    case equal:
    case greater_equal:
    case less_equal:
    case Uless_equal:
    case Ugreater_equal:
      return true;
    default:
      return false;
  }
}

void AssembleBranchToLabels(CodeGenerator* gen, TurboAssembler* tasm,
                            Instruction* instr, FlagsCondition condition,
                            Label* tlabel, Label* flabel, bool fallthru) {
#undef __
#define __ tasm->
  RiscvOperandConverter i(gen, instr);

  Condition cc = kNoCondition;
  // RISC-V does not have condition code flags, so compare and branch are
  // implemented differently than on the other arch's. The compare operations
  // emit riscv64 pseudo-instructions, which are handled here by branch
  // instructions that do the actual comparison. Essential that the input
  // registers to compare pseudo-op are not modified before this branch op, as
  // they are tested here.

  if (instr->arch_opcode() == kRiscvTst) {
    cc = FlagsConditionToConditionTst(condition);
    __ Branch(tlabel, cc, kScratchReg, Operand(zero_reg));
  } else if (instr->arch_opcode() == kRiscvAdd64 ||
             instr->arch_opcode() == kRiscvSub64) {
    cc = FlagsConditionToConditionOvf(condition);
    __ Sra64(kScratchReg, i.OutputRegister(), 32);
    __ Sra64(kScratchReg2, i.OutputRegister(), 31);
    __ Branch(tlabel, cc, kScratchReg2, Operand(kScratchReg));
  } else if (instr->arch_opcode() == kRiscvAddOvf64 ||
             instr->arch_opcode() == kRiscvSubOvf64) {
    switch (condition) {
      // Overflow occurs if overflow register is negative
      case kOverflow:
        __ Branch(tlabel, lt, kScratchReg, Operand(zero_reg));
        break;
      case kNotOverflow:
        __ Branch(tlabel, ge, kScratchReg, Operand(zero_reg));
        break;
      default:
        UNSUPPORTED_COND(instr->arch_opcode(), condition);
    }
  } else if (instr->arch_opcode() == kRiscvMulOvf32) {
    // Overflow occurs if overflow register is not zero
    switch (condition) {
      case kOverflow:
        __ Branch(tlabel, ne, kScratchReg, Operand(zero_reg));
        break;
      case kNotOverflow:
        __ Branch(tlabel, eq, kScratchReg, Operand(zero_reg));
        break;
      default:
        UNSUPPORTED_COND(kRiscvMulOvf32, condition);
    }
  } else if (instr->arch_opcode() == kRiscvCmp) {
    cc = FlagsConditionToConditionCmp(condition);
    __ Branch(tlabel, cc, i.InputRegister(0), i.InputOperand(1));
  } else if (instr->arch_opcode() == kRiscvCmpZero) {
    cc = FlagsConditionToConditionCmp(condition);
    if (i.InputOrZeroRegister(0) == zero_reg && IsInludeEqual(cc)) {
      __ Branch(tlabel);
    } else if (i.InputOrZeroRegister(0) != zero_reg) {
      __ Branch(tlabel, cc, i.InputRegister(0), Operand(zero_reg));
    }
  } else if (instr->arch_opcode() == kArchStackPointerGreaterThan) {
    cc = FlagsConditionToConditionCmp(condition);
    Register lhs_register = sp;
    uint32_t offset;
    if (gen->ShouldApplyOffsetToStackCheck(instr, &offset)) {
      lhs_register = i.TempRegister(0);
      __ Sub64(lhs_register, sp, offset);
    }
    __ Branch(tlabel, cc, lhs_register, Operand(i.InputRegister(0)));
  } else if (instr->arch_opcode() == kRiscvCmpS ||
             instr->arch_opcode() == kRiscvCmpD) {
    bool predicate;
    FlagsConditionToConditionCmpFPU(&predicate, condition);
    // floating-point compare result is set in kScratchReg
    if (predicate) {
      __ BranchTrueF(kScratchReg, tlabel);
    } else {
      __ BranchFalseF(kScratchReg, tlabel);
    }
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

#undef UNSUPPORTED_COND

void CodeGenerator::AssembleArchDeoptBranch(Instruction* instr,
                                            BranchInfo* branch) {
  AssembleArchBranch(instr, branch);
}

void CodeGenerator::AssembleArchJumpRegardlessOfAssemblyOrder(
    RpoNumber target) {
  __ Branch(GetLabel(target));
}

void CodeGenerator::AssembleArchTrap(Instruction* instr,
                                     FlagsCondition condition) {
  class OutOfLineTrap final : public OutOfLineCode {
   public:
    OutOfLineTrap(CodeGenerator* gen, Instruction* instr)
        : OutOfLineCode(gen), instr_(instr), gen_(gen) {}
    void Generate() final {
      RiscvOperandConverter i(gen_, instr_);
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
        pop_count += (pop_count & 1);  // align
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
  AssembleBranchToLabels(this, tasm(), instr, condition, tlabel, nullptr, true);
}

// Assembles boolean materializations after an instruction.
void CodeGenerator::AssembleArchBoolean(Instruction* instr,
                                        FlagsCondition condition) {
  RiscvOperandConverter i(this, instr);

  // Materialize a full 32-bit 1 or 0 value. The result register is always the
  // last output of the instruction.
  DCHECK_NE(0u, instr->OutputCount());
  Register result = i.OutputRegister(instr->OutputCount() - 1);
  Condition cc = kNoCondition;
  // RISC-V does not have condition code flags, so compare and branch are
  // implemented differently than on the other arch's. The compare operations
  // emit riscv64 pseudo-instructions, which are checked and handled here.

  if (instr->arch_opcode() == kRiscvTst) {
    cc = FlagsConditionToConditionTst(condition);
    if (cc == eq) {
      __ Sltu(result, kScratchReg, 1);
    } else {
      __ Sltu(result, zero_reg, kScratchReg);
    }
    return;
  } else if (instr->arch_opcode() == kRiscvAdd64 ||
             instr->arch_opcode() == kRiscvSub64) {
    cc = FlagsConditionToConditionOvf(condition);
    // Check for overflow creates 1 or 0 for result.
    __ Srl64(kScratchReg, i.OutputRegister(), 63);
    __ Srl32(kScratchReg2, i.OutputRegister(), 31);
    __ Xor(result, kScratchReg, kScratchReg2);
    if (cc == eq)  // Toggle result for not overflow.
      __ Xor(result, result, 1);
    return;
  } else if (instr->arch_opcode() == kRiscvAddOvf64 ||
             instr->arch_opcode() == kRiscvSubOvf64) {
    // Overflow occurs if overflow register is negative
    __ Slt(result, kScratchReg, zero_reg);
  } else if (instr->arch_opcode() == kRiscvMulOvf32) {
    // Overflow occurs if overflow register is not zero
    __ Sgtu(result, kScratchReg, zero_reg);
  } else if (instr->arch_opcode() == kRiscvCmp) {
    cc = FlagsConditionToConditionCmp(condition);
    switch (cc) {
      case eq:
      case ne: {
        Register left = i.InputOrZeroRegister(0);
        Operand right = i.InputOperand(1);
        if (instr->InputAt(1)->IsImmediate()) {
          if (is_int12(-right.immediate())) {
            if (right.immediate() == 0) {
              if (cc == eq) {
                __ Sltu(result, left, 1);
              } else {
                __ Sltu(result, zero_reg, left);
              }
            } else {
              __ Add64(result, left, Operand(-right.immediate()));
              if (cc == eq) {
                __ Sltu(result, result, 1);
              } else {
                __ Sltu(result, zero_reg, result);
              }
            }
          } else {
            if (is_uint12(right.immediate())) {
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
        Register left = i.InputOrZeroRegister(0);
        Operand right = i.InputOperand(1);
        __ Slt(result, left, right);
        if (cc == ge) {
          __ Xor(result, result, 1);
        }
      } break;
      case gt:
      case le: {
        Register left = i.InputOrZeroRegister(1);
        Operand right = i.InputOperand(0);
        __ Slt(result, left, right);
        if (cc == le) {
          __ Xor(result, result, 1);
        }
      } break;
      case Uless:
      case Ugreater_equal: {
        Register left = i.InputOrZeroRegister(0);
        Operand right = i.InputOperand(1);
        __ Sltu(result, left, right);
        if (cc == Ugreater_equal) {
          __ Xor(result, result, 1);
        }
      } break;
      case Ugreater:
      case Uless_equal: {
        Register left = i.InputRegister(1);
        Operand right = i.InputOperand(0);
        __ Sltu(result, left, right);
        if (cc == Uless_equal) {
          __ Xor(result, result, 1);
        }
      } break;
      default:
        UNREACHABLE();
    }
    return;
  } else if (instr->arch_opcode() == kRiscvCmpZero) {
    cc = FlagsConditionToConditionCmp(condition);
    switch (cc) {
      case eq: {
        Register left = i.InputOrZeroRegister(0);
        __ Sltu(result, left, 1);
        break;
      }
      case ne: {
        Register left = i.InputOrZeroRegister(0);
        __ Sltu(result, zero_reg, left);
        break;
      }
      case lt:
      case ge: {
        Register left = i.InputOrZeroRegister(0);
        Operand right = Operand(zero_reg);
        __ Slt(result, left, right);
        if (cc == ge) {
          __ Xor(result, result, 1);
        }
      } break;
      case gt:
      case le: {
        Operand left = i.InputOperand(0);
        __ Slt(result, zero_reg, left);
        if (cc == le) {
          __ Xor(result, result, 1);
        }
      } break;
      case Uless:
      case Ugreater_equal: {
        Register left = i.InputOrZeroRegister(0);
        Operand right = Operand(zero_reg);
        __ Sltu(result, left, right);
        if (cc == Ugreater_equal) {
          __ Xor(result, result, 1);
        }
      } break;
      case Ugreater:
      case Uless_equal: {
        Register left = zero_reg;
        Operand right = i.InputOperand(0);
        __ Sltu(result, left, right);
        if (cc == Uless_equal) {
          __ Xor(result, result, 1);
        }
      } break;
      default:
        UNREACHABLE();
    }
    return;
  } else if (instr->arch_opcode() == kArchStackPointerGreaterThan) {
    cc = FlagsConditionToConditionCmp(condition);
    Register lhs_register = sp;
    uint32_t offset;
    if (ShouldApplyOffsetToStackCheck(instr, &offset)) {
      lhs_register = i.TempRegister(0);
      __ Sub64(lhs_register, sp, offset);
    }
    __ Sgtu(result, lhs_register, Operand(i.InputRegister(0)));
    return;
  } else if (instr->arch_opcode() == kRiscvCmpD ||
             instr->arch_opcode() == kRiscvCmpS) {
    FPURegister left = i.InputOrZeroDoubleRegister(0);
    FPURegister right = i.InputOrZeroDoubleRegister(1);
    if ((instr->arch_opcode() == kRiscvCmpD) &&
        (left == kDoubleRegZero || right == kDoubleRegZero) &&
        !__ IsDoubleZeroRegSet()) {
      __ LoadFPRImmediate(kDoubleRegZero, 0.0);
    } else if ((instr->arch_opcode() == kRiscvCmpS) &&
               (left == kDoubleRegZero || right == kDoubleRegZero) &&
               !__ IsSingleZeroRegSet()) {
      __ LoadFPRImmediate(kDoubleRegZero, 0.0f);
    }
    bool predicate;
    FlagsConditionToConditionCmpFPU(&predicate, condition);
    // RISCV compare returns 0 or 1, do nothing when predicate; otherwise
    // toggle kScratchReg (i.e., 0 -> 1, 1 -> 0)
    if (predicate) {
      __ Move(result, kScratchReg);
    } else {
      __ Xor(result, kScratchReg, 1);
    }
    return;
  } else {
    PrintF("AssembleArchBranch Unimplemented arch_opcode is : %d\n",
           instr->arch_opcode());
    TRACE_UNIMPL();
    UNIMPLEMENTED();
  }
}

void CodeGenerator::AssembleArchBinarySearchSwitch(Instruction* instr) {
  RiscvOperandConverter i(this, instr);
  Register input = i.InputRegister(0);
  std::vector<std::pair<int32_t, Label*>> cases;
  for (size_t index = 2; index < instr->InputCount(); index += 2) {
    cases.push_back({i.InputInt32(index + 0), GetLabel(i.InputRpo(index + 1))});
  }
  AssembleArchBinarySearchSwitchRange(input, i.InputRpo(1), cases.data(),
                                      cases.data() + cases.size());
}

void CodeGenerator::AssembleArchTableSwitch(Instruction* instr) {
  RiscvOperandConverter i(this, instr);
  Register input = i.InputRegister(0);
  size_t const case_count = instr->InputCount() - 2;

  __ Branch(GetLabel(i.InputRpo(1)), Ugreater_equal, input,
            Operand(case_count));
  __ GenerateSwitchTable(input, case_count, [&i, this](size_t index) {
    return GetLabel(i.InputRpo(index + 2));
  });
}

void CodeGenerator::FinishFrame(Frame* frame) {
  auto call_descriptor = linkage()->GetIncomingDescriptor();

  const DoubleRegList saves_fpu = call_descriptor->CalleeSavedFPRegisters();
  if (!saves_fpu.is_empty()) {
    int count = saves_fpu.Count();
    DCHECK_EQ(kNumCalleeSavedFPU, count);
    frame->AllocateSavedCalleeRegisterSlots(count *
                                            (kDoubleSize / kSystemPointerSize));
  }

  const RegList saves = call_descriptor->CalleeSavedRegisters();
  if (!saves.is_empty()) {
    int count = saves.Count();
    frame->AllocateSavedCalleeRegisterSlots(count);
  }
}

void CodeGenerator::AssembleConstructFrame() {
  auto call_descriptor = linkage()->GetIncomingDescriptor();

  if (frame_access_state()->has_frame()) {
    if (call_descriptor->IsCFunctionCall()) {
      if (info()->GetOutputStackFrameType() == StackFrame::C_WASM_ENTRY) {
        __ StubPrologue(StackFrame::C_WASM_ENTRY);
        // Reserve stack space for saving the c_entry_fp later.
        __ Sub64(sp, sp, Operand(kSystemPointerSize));
      } else {
        __ Push(ra, fp);
        __ Move(fp, sp);
      }
    } else if (call_descriptor->IsJSFunctionCall()) {
      __ Prologue();
    } else {
      __ StubPrologue(info()->GetOutputStackFrameType());
      if (call_descriptor->IsWasmFunctionCall() ||
          call_descriptor->IsWasmImportWrapper() ||
          call_descriptor->IsWasmCapiFunction()) {
        __ Push(kWasmInstanceRegister);
      }
      if (call_descriptor->IsWasmCapiFunction()) {
        // Reserve space for saving the PC later.
        __ Sub64(sp, sp, Operand(kSystemPointerSize));
      }
    }
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

  const RegList saves = call_descriptor->CalleeSavedRegisters();
  const DoubleRegList saves_fpu = call_descriptor->CalleeSavedFPRegisters();

  if (required_slots > 0) {
    DCHECK(frame_access_state()->has_frame());
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
        __ Ld(
            kScratchReg,
            FieldMemOperand(kWasmInstanceRegister,
                            WasmInstanceObject::kRealStackLimitAddressOffset));
        __ Ld(kScratchReg, MemOperand(kScratchReg));
        __ Add64(kScratchReg, kScratchReg,
                 Operand(required_slots * kSystemPointerSize));
        __ BranchShort(&done, uge, sp, Operand(kScratchReg));
      }

      __ Call(wasm::WasmCode::kWasmStackOverflow, RelocInfo::WASM_STUB_CALL);
      // We come from WebAssembly, there are no references for the GC.
      ReferenceMap* reference_map = zone()->New<ReferenceMap>(zone());
      RecordSafepoint(reference_map);
      if (FLAG_debug_code) {
        __ stop();
      }

      __ bind(&done);
    }
  }

  const int returns = frame()->GetReturnSlotCount();

  // Skip callee-saved and return slots, which are pushed below.
  required_slots -= saves.Count();
  required_slots -= saves_fpu.Count();
  required_slots -= returns;
  if (required_slots > 0) {
    __ Sub64(sp, sp, Operand(required_slots * kSystemPointerSize));
  }

  if (!saves_fpu.is_empty()) {
    // Save callee-saved FPU registers.
    __ MultiPushFPU(saves_fpu);
    DCHECK_EQ(kNumCalleeSavedFPU, saves_fpu.Count());
  }

  if (!saves.is_empty()) {
    // Save callee-saved registers.
    __ MultiPush(saves);
  }

  if (returns != 0) {
    // Create space for returns.
    __ Sub64(sp, sp, Operand(returns * kSystemPointerSize));
  }
}

void CodeGenerator::AssembleReturn(InstructionOperand* additional_pop_count) {
  auto call_descriptor = linkage()->GetIncomingDescriptor();

  const int returns = frame()->GetReturnSlotCount();
  if (returns != 0) {
    __ Add64(sp, sp, Operand(returns * kSystemPointerSize));
  }

  // Restore GP registers.
  const RegList saves = call_descriptor->CalleeSavedRegisters();
  if (!saves.is_empty()) {
    __ MultiPop(saves);
  }

  // Restore FPU registers.
  const DoubleRegList saves_fpu = call_descriptor->CalleeSavedFPRegisters();
  if (!saves_fpu.is_empty()) {
    __ MultiPopFPU(saves_fpu);
  }

  RiscvOperandConverter g(this, nullptr);

  const int parameter_slots =
      static_cast<int>(call_descriptor->ParameterSlotCount());

  // {aditional_pop_count} is only greater than zero if {parameter_slots = 0}.
  // Check RawMachineAssembler::PopAndReturn.
  if (parameter_slots != 0) {
    if (additional_pop_count->IsImmediate()) {
      DCHECK_EQ(g.ToConstant(additional_pop_count).ToInt32(), 0);
    } else if (FLAG_debug_code) {
      __ Assert(eq, AbortReason::kUnexpectedAdditionalPopValue,
                g.ToRegister(additional_pop_count),
                Operand(static_cast<int64_t>(0)));
    }
  }

  // Functions with JS linkage have at least one parameter (the receiver).
  // If {parameter_slots} == 0, it means it is a builtin with
  // kDontAdaptArgumentsSentinel, which takes care of JS arguments popping
  // itself.
  const bool drop_jsargs = frame_access_state()->has_frame() &&
                           call_descriptor->IsJSFunctionCall() &&
                           parameter_slots != 0;

  if (call_descriptor->IsCFunctionCall()) {
    AssembleDeconstructFrame();
  } else if (frame_access_state()->has_frame()) {
    // Canonicalize JSFunction return sites for now unless they have an variable
    // number of stack slot pops.
    if (additional_pop_count->IsImmediate() &&
        g.ToConstant(additional_pop_count).ToInt32() == 0) {
      if (return_label_.is_bound()) {
        __ Branch(&return_label_);
        return;
      } else {
        __ bind(&return_label_);
      }
    }
    if (drop_jsargs) {
      // Get the actual argument count
      __ Ld(t0, MemOperand(fp, StandardFrameConstants::kArgCOffset));
    }
    AssembleDeconstructFrame();
  }
  if (drop_jsargs) {
    // We must pop all arguments from the stack (including the receiver). This
    // number of arguments is given by max(1 + argc_reg, parameter_slots).
    if (parameter_slots > 1) {
      Label done;
      __ li(kScratchReg, parameter_slots);
      __ BranchShort(&done, ge, t0, Operand(kScratchReg));
      __ Move(t0, kScratchReg);
      __ bind(&done);
    }
    __ Sll64(t0, t0, kSystemPointerSizeLog2);
    __ Add64(sp, sp, t0);
  } else if (additional_pop_count->IsImmediate()) {
    // it should be a kInt32 or a kInt64
    DCHECK_LE(g.ToConstant(additional_pop_count).type(), Constant::kInt64);
    int additional_count = g.ToConstant(additional_pop_count).ToInt32();
    __ Drop(parameter_slots + additional_count);
  } else {
    Register pop_reg = g.ToRegister(additional_pop_count);
    __ Drop(parameter_slots);
    __ Sll64(pop_reg, pop_reg, kSystemPointerSizeLog2);
    __ Add64(sp, sp, pop_reg);
  }
  __ Ret();
}

void CodeGenerator::FinishCode() { __ ForceConstantPoolEmissionWithoutJump(); }

void CodeGenerator::PrepareForDeoptimizationExits(
    ZoneDeque<DeoptimizationExit*>* exits) {
  __ ForceConstantPoolEmissionWithoutJump();
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
  RiscvOperandConverter g(this, nullptr);
  // Dispatch on the source and destination operand kinds.  Not all
  // combinations are possible.
  if (source->IsRegister()) {
    DCHECK(destination->IsRegister() || destination->IsStackSlot());
    Register src = g.ToRegister(source);
    if (destination->IsRegister()) {
      __ Move(g.ToRegister(destination), src);
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
          if (src.ToInt32() == 0 && destination->IsStackSlot()) {
            dst = zero_reg;
          } else {
            __ li(dst, Operand(src.ToInt32()));
          }
          break;
        case Constant::kFloat32:
          __ li(dst, Operand::EmbeddedNumber(src.ToFloat32()));
          break;
        case Constant::kInt64:
          if (RelocInfo::IsWasmReference(src.rmode())) {
            __ li(dst, Operand(src.ToInt64(), src.rmode()));
          } else {
            if (src.ToInt64() == 0 && destination->IsStackSlot()) {
              dst = zero_reg;
            } else {
              __ li(dst, Operand(src.ToInt64()));
            }
          }
          break;
        case Constant::kFloat64:
          __ li(dst, Operand::EmbeddedNumber(src.ToFloat64().value()));
          break;
        case Constant::kExternalReference:
          __ li(dst, src.ToExternalReference());
          break;
        case Constant::kDelayedStringConstant:
          __ li(dst, src.ToDelayedStringConstant());
          break;
        case Constant::kHeapObject: {
          Handle<HeapObject> src_object = src.ToHeapObject();
          RootIndex index;
          if (IsMaterializableFromRoot(src_object, &index)) {
            __ LoadRoot(dst, index);
          } else {
            __ li(dst, src_object);
          }
          break;
        }
        case Constant::kCompressedHeapObject: {
          Handle<HeapObject> src_object = src.ToHeapObject();
          RootIndex index;
          if (IsMaterializableFromRoot(src_object, &index)) {
            __ LoadRoot(dst, index);
          } else {
            __ li(dst, src_object, RelocInfo::COMPRESSED_EMBEDDED_OBJECT);
          }
          break;
        }
        case Constant::kRpoNumber:
          UNREACHABLE();  // TODO(titzer): loading RPO numbers
      }
      if (destination->IsStackSlot()) __ Sd(dst, g.ToMemOperand(destination));
    } else if (src.type() == Constant::kFloat32) {
      if (destination->IsFPStackSlot()) {
        MemOperand dst = g.ToMemOperand(destination);
        if (bit_cast<int32_t>(src.ToFloat32()) == 0) {
          __ Sw(zero_reg, dst);
        } else {
          __ li(kScratchReg, Operand(bit_cast<int32_t>(src.ToFloat32())));
          __ Sw(kScratchReg, dst);
        }
      } else {
        DCHECK(destination->IsFPRegister());
        FloatRegister dst = g.ToSingleRegister(destination);
        __ LoadFPRImmediate(dst, src.ToFloat32());
      }
    } else {
      DCHECK_EQ(Constant::kFloat64, src.type());
      DoubleRegister dst = destination->IsFPRegister()
                               ? g.ToDoubleRegister(destination)
                               : kScratchDoubleReg;
      __ LoadFPRImmediate(dst, src.ToFloat64().value());
      if (destination->IsFPStackSlot()) {
        __ StoreDouble(dst, g.ToMemOperand(destination));
      }
    }
  } else if (source->IsFPRegister()) {
    MachineRepresentation rep = LocationOperand::cast(source)->representation();
    if (rep == MachineRepresentation::kSimd128) {
      VRegister src = g.ToSimd128Register(source);
      if (destination->IsSimd128Register()) {
        VRegister dst = g.ToSimd128Register(destination);
        __ VU.set(kScratchReg, E8, m1);
        __ vmv_vv(dst, src);
      } else {
        DCHECK(destination->IsSimd128StackSlot());
        __ VU.set(kScratchReg, E8, m1);
        MemOperand dst = g.ToMemOperand(destination);
        Register dst_r = dst.rm();
        if (dst.offset() != 0) {
          dst_r = kScratchReg;
          __ Add64(dst_r, dst.rm(), dst.offset());
        }
        __ vs(src, dst_r, 0, E8);
      }
    } else {
      FPURegister src = g.ToDoubleRegister(source);
      if (destination->IsFPRegister()) {
        FPURegister dst = g.ToDoubleRegister(destination);
        __ Move(dst, src);
      } else {
        DCHECK(destination->IsFPStackSlot());
        if (rep == MachineRepresentation::kFloat32) {
          __ StoreFloat(src, g.ToMemOperand(destination));
        } else {
          DCHECK_EQ(rep, MachineRepresentation::kFloat64);
          __ StoreDouble(src, g.ToMemOperand(destination));
        }
      }
    }
  } else if (source->IsFPStackSlot()) {
    DCHECK(destination->IsFPRegister() || destination->IsFPStackSlot());
    MemOperand src = g.ToMemOperand(source);
    MachineRepresentation rep = LocationOperand::cast(source)->representation();
    if (rep == MachineRepresentation::kSimd128) {
      __ VU.set(kScratchReg, E8, m1);
      Register src_r = src.rm();
      if (src.offset() != 0) {
        src_r = kScratchReg;
        __ Add64(src_r, src.rm(), src.offset());
      }
      if (destination->IsSimd128Register()) {
        __ vl(g.ToSimd128Register(destination), src_r, 0, E8);
      } else {
        DCHECK(destination->IsSimd128StackSlot());
        VRegister temp = kSimd128ScratchReg;
        MemOperand dst = g.ToMemOperand(destination);
        Register dst_r = dst.rm();
        if (dst.offset() != 0) {
          dst_r = kScratchReg2;
          __ Add64(dst_r, dst.rm(), dst.offset());
        }
        __ vl(temp, src_r, 0, E8);
        __ vs(temp, dst_r, 0, E8);
      }
    } else {
      if (destination->IsFPRegister()) {
        if (rep == MachineRepresentation::kFloat32) {
          __ LoadFloat(g.ToDoubleRegister(destination), src);
        } else {
          DCHECK_EQ(rep, MachineRepresentation::kFloat64);
          __ LoadDouble(g.ToDoubleRegister(destination), src);
        }
      } else {
        DCHECK(destination->IsFPStackSlot());
        FPURegister temp = kScratchDoubleReg;
        if (rep == MachineRepresentation::kFloat32) {
          __ LoadFloat(temp, src);
          __ StoreFloat(temp, g.ToMemOperand(destination));
        } else {
          DCHECK_EQ(rep, MachineRepresentation::kFloat64);
          __ LoadDouble(temp, src);
          __ StoreDouble(temp, g.ToMemOperand(destination));
        }
      }
    }
  } else {
    UNREACHABLE();
  }
}

void CodeGenerator::AssembleSwap(InstructionOperand* source,
                                 InstructionOperand* destination) {
  RiscvOperandConverter g(this, nullptr);
  switch (MoveType::InferSwap(source, destination)) {
    case MoveType::kRegisterToRegister:
      if (source->IsRegister()) {
        Register temp = kScratchReg;
        Register src = g.ToRegister(source);
        Register dst = g.ToRegister(destination);
        __ Move(temp, src);
        __ Move(src, dst);
        __ Move(dst, temp);
      } else {
        if (source->IsFloatRegister() || source->IsDoubleRegister()) {
          FPURegister temp = kScratchDoubleReg;
          FPURegister src = g.ToDoubleRegister(source);
          FPURegister dst = g.ToDoubleRegister(destination);
          __ Move(temp, src);
          __ Move(src, dst);
          __ Move(dst, temp);
        } else {
          DCHECK(source->IsSimd128Register());
          VRegister src = g.ToDoubleRegister(source).toV();
          VRegister dst = g.ToDoubleRegister(destination).toV();
          VRegister temp = kSimd128ScratchReg;
          __ VU.set(kScratchReg, E8, m1);
          __ vmv_vv(temp, src);
          __ vmv_vv(src, dst);
          __ vmv_vv(dst, temp);
        }
      }
      return;
    case MoveType::kRegisterToStack: {
      MemOperand dst = g.ToMemOperand(destination);
      if (source->IsRegister()) {
        Register temp = kScratchReg;
        Register src = g.ToRegister(source);
        __ mv(temp, src);
        __ Ld(src, dst);
        __ Sd(temp, dst);
      } else {
        MemOperand dst = g.ToMemOperand(destination);
        if (source->IsFloatRegister()) {
          DoubleRegister src = g.ToDoubleRegister(source);
          DoubleRegister temp = kScratchDoubleReg;
          __ fmv_s(temp, src);
          __ LoadFloat(src, dst);
          __ StoreFloat(temp, dst);
        } else if (source->IsDoubleRegister()) {
          DoubleRegister src = g.ToDoubleRegister(source);
          DoubleRegister temp = kScratchDoubleReg;
          __ fmv_d(temp, src);
          __ LoadDouble(src, dst);
          __ StoreDouble(temp, dst);
        } else {
          DCHECK(source->IsSimd128Register());
          VRegister src = g.ToDoubleRegister(source).toV();
          VRegister temp = kSimd128ScratchReg;
          __ VU.set(kScratchReg, E8, m1);
          __ vmv_vv(temp, src);
          Register dst_v = dst.rm();
          if (dst.offset() != 0) {
            dst_v = kScratchReg2;
            __ Add64(dst_v, dst.rm(), Operand(dst.offset()));
          }
          __ vl(src, dst_v, 0, E8);
          __ vs(temp, dst_v, 0, E8);
        }
      }
      return;
    }
    case MoveType::kStackToStack: {
      MemOperand src = g.ToMemOperand(source);
      MemOperand dst = g.ToMemOperand(destination);
      if (source->IsSimd128StackSlot()) {
        __ VU.set(kScratchReg, E8, m1);
        Register src_v = src.rm();
        Register dst_v = dst.rm();
        if (src.offset() != 0) {
          src_v = kScratchReg;
          __ Add64(src_v, src.rm(), Operand(src.offset()));
        }
        if (dst.offset() != 0) {
          dst_v = kScratchReg2;
          __ Add64(dst_v, dst.rm(), Operand(dst.offset()));
        }
        __ vl(kSimd128ScratchReg, src_v, 0, E8);
        __ vl(kSimd128ScratchReg2, dst_v, 0, E8);
        __ vs(kSimd128ScratchReg, dst_v, 0, E8);
        __ vs(kSimd128ScratchReg2, src_v, 0, E8);
      } else {
        UseScratchRegisterScope scope(tasm());
        Register temp_0 = kScratchReg;
        Register temp_1 = kScratchReg2;
        __ Ld(temp_0, src);
        __ Ld(temp_1, dst);
        __ Sd(temp_0, dst);
        __ Sd(temp_1, src);
      }
      return;
    }
    default:
      UNREACHABLE();
  }
}

void CodeGenerator::AssembleJumpTable(Label** targets, size_t target_count) {
  // On 64-bit RISC-V we emit the jump tables inline.
  UNREACHABLE();
}

#undef ASSEMBLE_ATOMIC_LOAD_INTEGER
#undef ASSEMBLE_ATOMIC_STORE_INTEGER
#undef ASSEMBLE_ATOMIC_BINOP
#undef ASSEMBLE_ATOMIC_BINOP_EXT
#undef ASSEMBLE_ATOMIC_EXCHANGE_INTEGER
#undef ASSEMBLE_ATOMIC_EXCHANGE_INTEGER_EXT
#undef ASSEMBLE_ATOMIC_COMPARE_EXCHANGE_INTEGER
#undef ASSEMBLE_ATOMIC_COMPARE_EXCHANGE_INTEGER_EXT
#undef ASSEMBLE_IEEE754_BINOP
#undef ASSEMBLE_IEEE754_UNOP

#undef TRACE_MSG
#undef TRACE_UNIMPL
#undef __

}  // namespace compiler
}  // namespace internal
}  // namespace v8
