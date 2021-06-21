// Copyright 2013 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <limits>

#include "src/base/overflowing-math.h"
#include "src/codegen/assembler.h"
#include "src/codegen/cpu-features.h"
#include "src/codegen/external-reference.h"
#include "src/codegen/macro-assembler.h"
#include "src/codegen/optimized-compilation-info.h"
#include "src/codegen/x64/assembler-x64.h"
#include "src/codegen/x64/register-x64.h"
#include "src/compiler/backend/code-generator-impl.h"
#include "src/compiler/backend/code-generator.h"
#include "src/compiler/backend/gap-resolver.h"
#include "src/compiler/backend/instruction-codes.h"
#include "src/compiler/node-matchers.h"
#include "src/compiler/osr.h"
#include "src/heap/memory-chunk.h"
#include "src/objects/smi.h"

#if V8_ENABLE_WEBASSEMBLY
#include "src/wasm/wasm-code-manager.h"
#include "src/wasm/wasm-objects.h"
#endif  // V8_ENABLE_WEBASSEMBLY

namespace v8 {
namespace internal {
namespace compiler {

#define __ tasm()->

// Adds X64 specific methods for decoding operands.
class X64OperandConverter : public InstructionOperandConverter {
 public:
  X64OperandConverter(CodeGenerator* gen, Instruction* instr)
      : InstructionOperandConverter(gen, instr) {}

  Immediate InputImmediate(size_t index) {
    return ToImmediate(instr_->InputAt(index));
  }

  Operand InputOperand(size_t index, int extra = 0) {
    return ToOperand(instr_->InputAt(index), extra);
  }

  Operand OutputOperand() { return ToOperand(instr_->Output()); }

  Immediate ToImmediate(InstructionOperand* operand) {
    Constant constant = ToConstant(operand);
    if (constant.type() == Constant::kFloat64) {
      DCHECK_EQ(0, constant.ToFloat64().AsUint64());
      return Immediate(0);
    }
    if (RelocInfo::IsWasmReference(constant.rmode())) {
      return Immediate(constant.ToInt32(), constant.rmode());
    }
    return Immediate(constant.ToInt32());
  }

  Operand ToOperand(InstructionOperand* op, int extra = 0) {
    DCHECK(op->IsStackSlot() || op->IsFPStackSlot());
    return SlotToOperand(AllocatedOperand::cast(op)->index(), extra);
  }

  Operand SlotToOperand(int slot_index, int extra = 0) {
    FrameOffset offset = frame_access_state()->GetFrameOffset(slot_index);
    return Operand(offset.from_stack_pointer() ? rsp : rbp,
                   offset.offset() + extra);
  }

  static size_t NextOffset(size_t* offset) {
    size_t i = *offset;
    (*offset)++;
    return i;
  }

  static ScaleFactor ScaleFor(AddressingMode one, AddressingMode mode) {
    STATIC_ASSERT(0 == static_cast<int>(times_1));
    STATIC_ASSERT(1 == static_cast<int>(times_2));
    STATIC_ASSERT(2 == static_cast<int>(times_4));
    STATIC_ASSERT(3 == static_cast<int>(times_8));
    int scale = static_cast<int>(mode - one);
    DCHECK(scale >= 0 && scale < 4);
    return static_cast<ScaleFactor>(scale);
  }

  Operand MemoryOperand(size_t* offset) {
    AddressingMode mode = AddressingModeField::decode(instr_->opcode());
    switch (mode) {
      case kMode_MR: {
        Register base = InputRegister(NextOffset(offset));
        int32_t disp = 0;
        return Operand(base, disp);
      }
      case kMode_MRI: {
        Register base = InputRegister(NextOffset(offset));
        int32_t disp = InputInt32(NextOffset(offset));
        return Operand(base, disp);
      }
      case kMode_MR1:
      case kMode_MR2:
      case kMode_MR4:
      case kMode_MR8: {
        Register base = InputRegister(NextOffset(offset));
        Register index = InputRegister(NextOffset(offset));
        ScaleFactor scale = ScaleFor(kMode_MR1, mode);
        int32_t disp = 0;
        return Operand(base, index, scale, disp);
      }
      case kMode_MR1I:
      case kMode_MR2I:
      case kMode_MR4I:
      case kMode_MR8I: {
        Register base = InputRegister(NextOffset(offset));
        Register index = InputRegister(NextOffset(offset));
        ScaleFactor scale = ScaleFor(kMode_MR1I, mode);
        int32_t disp = InputInt32(NextOffset(offset));
        return Operand(base, index, scale, disp);
      }
      case kMode_M1: {
        Register base = InputRegister(NextOffset(offset));
        int32_t disp = 0;
        return Operand(base, disp);
      }
      case kMode_M2:
        UNREACHABLE();  // Should use kModeMR with more compact encoding instead
        return Operand(no_reg, 0);
      case kMode_M4:
      case kMode_M8: {
        Register index = InputRegister(NextOffset(offset));
        ScaleFactor scale = ScaleFor(kMode_M1, mode);
        int32_t disp = 0;
        return Operand(index, scale, disp);
      }
      case kMode_M1I:
      case kMode_M2I:
      case kMode_M4I:
      case kMode_M8I: {
        Register index = InputRegister(NextOffset(offset));
        ScaleFactor scale = ScaleFor(kMode_M1I, mode);
        int32_t disp = InputInt32(NextOffset(offset));
        return Operand(index, scale, disp);
      }
      case kMode_Root: {
        Register base = kRootRegister;
        int32_t disp = InputInt32(NextOffset(offset));
        return Operand(base, disp);
      }
      case kMode_None:
        UNREACHABLE();
    }
    UNREACHABLE();
  }

  Operand MemoryOperand(size_t first_input = 0) {
    return MemoryOperand(&first_input);
  }
};

namespace {

bool HasAddressingMode(Instruction* instr) {
  return instr->addressing_mode() != kMode_None;
}

bool HasImmediateInput(Instruction* instr, size_t index) {
  return instr->InputAt(index)->IsImmediate();
}

bool HasRegisterInput(Instruction* instr, size_t index) {
  return instr->InputAt(index)->IsRegister();
}

class OutOfLineLoadFloat32NaN final : public OutOfLineCode {
 public:
  OutOfLineLoadFloat32NaN(CodeGenerator* gen, XMMRegister result)
      : OutOfLineCode(gen), result_(result) {}

  void Generate() final {
    __ Xorps(result_, result_);
    __ Divss(result_, result_);
  }

 private:
  XMMRegister const result_;
};

class OutOfLineLoadFloat64NaN final : public OutOfLineCode {
 public:
  OutOfLineLoadFloat64NaN(CodeGenerator* gen, XMMRegister result)
      : OutOfLineCode(gen), result_(result) {}

  void Generate() final {
    __ Xorpd(result_, result_);
    __ Divsd(result_, result_);
  }

 private:
  XMMRegister const result_;
};

class OutOfLineTruncateDoubleToI final : public OutOfLineCode {
 public:
  OutOfLineTruncateDoubleToI(CodeGenerator* gen, Register result,
                             XMMRegister input, StubCallMode stub_mode,
                             UnwindingInfoWriter* unwinding_info_writer)
      : OutOfLineCode(gen),
        result_(result),
        input_(input),
#if V8_ENABLE_WEBASSEMBLY
        stub_mode_(stub_mode),
#endif  // V8_ENABLE_WEBASSEMBLY
        unwinding_info_writer_(unwinding_info_writer),
        isolate_(gen->isolate()),
        zone_(gen->zone()) {
  }

  void Generate() final {
    __ AllocateStackSpace(kDoubleSize);
    unwinding_info_writer_->MaybeIncreaseBaseOffsetAt(__ pc_offset(),
                                                      kDoubleSize);
    __ Movsd(MemOperand(rsp, 0), input_);
#if V8_ENABLE_WEBASSEMBLY
    if (stub_mode_ == StubCallMode::kCallWasmRuntimeStub) {
      // A direct call to a wasm runtime stub defined in this module.
      // Just encode the stub index. This will be patched when the code
      // is added to the native module and copied into wasm code space.
      __ near_call(wasm::WasmCode::kDoubleToI, RelocInfo::WASM_STUB_CALL);
#else
    // For balance.
    if (false) {
#endif  // V8_ENABLE_WEBASSEMBLY
    } else if (tasm()->options().inline_offheap_trampolines) {
      // With embedded builtins we do not need the isolate here. This allows
      // the call to be generated asynchronously.
      __ CallBuiltin(Builtins::kDoubleToI);
    } else {
      __ Call(BUILTIN_CODE(isolate_, DoubleToI), RelocInfo::CODE_TARGET);
    }
    __ movl(result_, MemOperand(rsp, 0));
    __ addq(rsp, Immediate(kDoubleSize));
    unwinding_info_writer_->MaybeIncreaseBaseOffsetAt(__ pc_offset(),
                                                      -kDoubleSize);
  }

 private:
  Register const result_;
  XMMRegister const input_;
#if V8_ENABLE_WEBASSEMBLY
  StubCallMode stub_mode_;
#endif  // V8_ENABLE_WEBASSEMBLY
  UnwindingInfoWriter* const unwinding_info_writer_;
  Isolate* isolate_;
  Zone* zone_;
};

class OutOfLineRecordWrite final : public OutOfLineCode {
 public:
  OutOfLineRecordWrite(CodeGenerator* gen, Register object, Operand operand,
                       Register value, Register scratch0, Register scratch1,
                       RecordWriteMode mode, StubCallMode stub_mode)
      : OutOfLineCode(gen),
        object_(object),
        operand_(operand),
        value_(value),
        scratch0_(scratch0),
        scratch1_(scratch1),
        mode_(mode),
#if V8_ENABLE_WEBASSEMBLY
        stub_mode_(stub_mode),
#endif  // V8_ENABLE_WEBASSEMBLY
        zone_(gen->zone()) {
  }

  void Generate() final {
    if (mode_ > RecordWriteMode::kValueIsPointer) {
      __ JumpIfSmi(value_, exit());
    }
    if (COMPRESS_POINTERS_BOOL) {
      __ DecompressTaggedPointer(value_, value_);
    }
    __ CheckPageFlag(value_, scratch0_,
                     MemoryChunk::kPointersToHereAreInterestingMask, zero,
                     exit());
    __ leaq(scratch1_, operand_);

    RememberedSetAction const remembered_set_action =
        mode_ > RecordWriteMode::kValueIsMap ? EMIT_REMEMBERED_SET
                                             : OMIT_REMEMBERED_SET;
    SaveFPRegsMode const save_fp_mode =
        frame()->DidAllocateDoubleRegisters() ? kSaveFPRegs : kDontSaveFPRegs;

    if (mode_ == RecordWriteMode::kValueIsEphemeronKey) {
      __ CallEphemeronKeyBarrier(object_, scratch1_, save_fp_mode);
#if V8_ENABLE_WEBASSEMBLY
    } else if (stub_mode_ == StubCallMode::kCallWasmRuntimeStub) {
      // A direct call to a wasm runtime stub defined in this module.
      // Just encode the stub index. This will be patched when the code
      // is added to the native module and copied into wasm code space.
      __ CallRecordWriteStub(object_, scratch1_, remembered_set_action,
                             save_fp_mode, wasm::WasmCode::kRecordWrite);
#endif  // V8_ENABLE_WEBASSEMBLY
    } else {
      __ CallRecordWriteStub(object_, scratch1_, remembered_set_action,
                             save_fp_mode);
    }
  }

 private:
  Register const object_;
  Operand const operand_;
  Register const value_;
  Register const scratch0_;
  Register const scratch1_;
  RecordWriteMode const mode_;
#if V8_ENABLE_WEBASSEMBLY
  StubCallMode const stub_mode_;
#endif  // V8_ENABLE_WEBASSEMBLY
  Zone* zone_;
};

#if V8_ENABLE_WEBASSEMBLY
class WasmOutOfLineTrap : public OutOfLineCode {
 public:
  WasmOutOfLineTrap(CodeGenerator* gen, Instruction* instr)
      : OutOfLineCode(gen), gen_(gen), instr_(instr) {}

  void Generate() override {
    X64OperandConverter i(gen_, instr_);
    TrapId trap_id =
        static_cast<TrapId>(i.InputInt32(instr_->InputCount() - 1));
    GenerateWithTrapId(trap_id);
  }

 protected:
  CodeGenerator* gen_;

  void GenerateWithTrapId(TrapId trap_id) { GenerateCallToTrap(trap_id); }

 private:
  void GenerateCallToTrap(TrapId trap_id) {
    if (!gen_->wasm_runtime_exception_support()) {
      // We cannot test calls to the runtime in cctest/test-run-wasm.
      // Therefore we emit a call to C here instead of a call to the runtime.
      __ PrepareCallCFunction(0);
      __ CallCFunction(ExternalReference::wasm_call_trap_callback_for_testing(),
                       0);
      __ LeaveFrame(StackFrame::WASM);
      auto call_descriptor = gen_->linkage()->GetIncomingDescriptor();
      size_t pop_size =
          call_descriptor->ParameterSlotCount() * kSystemPointerSize;
      // Use rcx as a scratch register, we return anyways immediately.
      __ Ret(static_cast<int>(pop_size), rcx);
    } else {
      gen_->AssembleSourcePosition(instr_);
      // A direct call to a wasm runtime stub defined in this module.
      // Just encode the stub index. This will be patched when the code
      // is added to the native module and copied into wasm code space.
      __ near_call(static_cast<Address>(trap_id), RelocInfo::WASM_STUB_CALL);
      ReferenceMap* reference_map =
          gen_->zone()->New<ReferenceMap>(gen_->zone());
      gen_->RecordSafepoint(reference_map);
      __ AssertUnreachable(AbortReason::kUnexpectedReturnFromWasmTrap);
    }
  }

  Instruction* instr_;
};

class WasmProtectedInstructionTrap final : public WasmOutOfLineTrap {
 public:
  WasmProtectedInstructionTrap(CodeGenerator* gen, int pc, Instruction* instr)
      : WasmOutOfLineTrap(gen, instr), pc_(pc) {}

  void Generate() final {
    DCHECK(FLAG_wasm_bounds_checks && FLAG_wasm_trap_handler);
    gen_->AddProtectedInstructionLanding(pc_, __ pc_offset());
    GenerateWithTrapId(TrapId::kTrapMemOutOfBounds);
  }

 private:
  int pc_;
};

void EmitOOLTrapIfNeeded(Zone* zone, CodeGenerator* codegen,
                         InstructionCode opcode, Instruction* instr,
                         int pc) {
  const MemoryAccessMode access_mode = AccessModeField::decode(opcode);
  if (access_mode == kMemoryAccessProtected) {
    zone->New<WasmProtectedInstructionTrap>(codegen, pc, instr);
  }
}

#else

void EmitOOLTrapIfNeeded(Zone* zone, CodeGenerator* codegen,
                         InstructionCode opcode, Instruction* instr, int pc) {
  DCHECK_NE(kMemoryAccessProtected, AccessModeField::decode(opcode));
}

#endif  // V8_ENABLE_WEBASSEMBLY

void EmitWordLoadPoisoningIfNeeded(CodeGenerator* codegen,
                                   InstructionCode opcode, Instruction* instr,
                                   X64OperandConverter const& i) {
  const MemoryAccessMode access_mode = AccessModeField::decode(opcode);
  if (access_mode == kMemoryAccessPoisoned) {
    Register value = i.OutputRegister();
    codegen->tasm()->andq(value, kSpeculationPoisonRegister);
  }
}

}  // namespace

#define ASSEMBLE_UNOP(asm_instr)         \
  do {                                   \
    if (instr->Output()->IsRegister()) { \
      __ asm_instr(i.OutputRegister());  \
    } else {                             \
      __ asm_instr(i.OutputOperand());   \
    }                                    \
  } while (false)

#define ASSEMBLE_BINOP(asm_instr)                                \
  do {                                                           \
    if (HasAddressingMode(instr)) {                              \
      size_t index = 1;                                          \
      Operand right = i.MemoryOperand(&index);                   \
      __ asm_instr(i.InputRegister(0), right);                   \
    } else {                                                     \
      if (HasImmediateInput(instr, 1)) {                         \
        if (HasRegisterInput(instr, 0)) {                        \
          __ asm_instr(i.InputRegister(0), i.InputImmediate(1)); \
        } else {                                                 \
          __ asm_instr(i.InputOperand(0), i.InputImmediate(1));  \
        }                                                        \
      } else {                                                   \
        if (HasRegisterInput(instr, 1)) {                        \
          __ asm_instr(i.InputRegister(0), i.InputRegister(1));  \
        } else {                                                 \
          __ asm_instr(i.InputRegister(0), i.InputOperand(1));   \
        }                                                        \
      }                                                          \
    }                                                            \
  } while (false)

#define ASSEMBLE_COMPARE(asm_instr)                              \
  do {                                                           \
    if (HasAddressingMode(instr)) {                              \
      size_t index = 0;                                          \
      Operand left = i.MemoryOperand(&index);                    \
      if (HasImmediateInput(instr, index)) {                     \
        __ asm_instr(left, i.InputImmediate(index));             \
      } else {                                                   \
        __ asm_instr(left, i.InputRegister(index));              \
      }                                                          \
    } else {                                                     \
      if (HasImmediateInput(instr, 1)) {                         \
        if (HasRegisterInput(instr, 0)) {                        \
          __ asm_instr(i.InputRegister(0), i.InputImmediate(1)); \
        } else {                                                 \
          __ asm_instr(i.InputOperand(0), i.InputImmediate(1));  \
        }                                                        \
      } else {                                                   \
        if (HasRegisterInput(instr, 1)) {                        \
          __ asm_instr(i.InputRegister(0), i.InputRegister(1));  \
        } else {                                                 \
          __ asm_instr(i.InputRegister(0), i.InputOperand(1));   \
        }                                                        \
      }                                                          \
    }                                                            \
  } while (false)

#define ASSEMBLE_MULT(asm_instr)                              \
  do {                                                        \
    if (HasImmediateInput(instr, 1)) {                        \
      if (HasRegisterInput(instr, 0)) {                       \
        __ asm_instr(i.OutputRegister(), i.InputRegister(0),  \
                     i.InputImmediate(1));                    \
      } else {                                                \
        __ asm_instr(i.OutputRegister(), i.InputOperand(0),   \
                     i.InputImmediate(1));                    \
      }                                                       \
    } else {                                                  \
      if (HasRegisterInput(instr, 1)) {                       \
        __ asm_instr(i.OutputRegister(), i.InputRegister(1)); \
      } else {                                                \
        __ asm_instr(i.OutputRegister(), i.InputOperand(1));  \
      }                                                       \
    }                                                         \
  } while (false)

#define ASSEMBLE_SHIFT(asm_instr, width)                                   \
  do {                                                                     \
    if (HasImmediateInput(instr, 1)) {                                     \
      if (instr->Output()->IsRegister()) {                                 \
        __ asm_instr(i.OutputRegister(), Immediate(i.InputInt##width(1))); \
      } else {                                                             \
        __ asm_instr(i.OutputOperand(), Immediate(i.InputInt##width(1)));  \
      }                                                                    \
    } else {                                                               \
      if (instr->Output()->IsRegister()) {                                 \
        __ asm_instr##_cl(i.OutputRegister());                             \
      } else {                                                             \
        __ asm_instr##_cl(i.OutputOperand());                              \
      }                                                                    \
    }                                                                      \
  } while (false)

#define ASSEMBLE_MOVX(asm_instr)                            \
  do {                                                      \
    if (HasAddressingMode(instr)) {                         \
      __ asm_instr(i.OutputRegister(), i.MemoryOperand());  \
    } else if (HasRegisterInput(instr, 0)) {                \
      __ asm_instr(i.OutputRegister(), i.InputRegister(0)); \
    } else {                                                \
      __ asm_instr(i.OutputRegister(), i.InputOperand(0));  \
    }                                                       \
  } while (false)

#define ASSEMBLE_SSE_BINOP(asm_instr)                                     \
  do {                                                                    \
    if (HasAddressingMode(instr)) {                                       \
      size_t index = 1;                                                   \
      Operand right = i.MemoryOperand(&index);                            \
      __ asm_instr(i.InputDoubleRegister(0), right);                      \
    } else {                                                              \
      if (instr->InputAt(1)->IsFPRegister()) {                            \
        __ asm_instr(i.InputDoubleRegister(0), i.InputDoubleRegister(1)); \
      } else {                                                            \
        __ asm_instr(i.InputDoubleRegister(0), i.InputOperand(1));        \
      }                                                                   \
    }                                                                     \
  } while (false)

#define ASSEMBLE_SSE_UNOP(asm_instr)                                    \
  do {                                                                  \
    if (instr->InputAt(0)->IsFPRegister()) {                            \
      __ asm_instr(i.OutputDoubleRegister(), i.InputDoubleRegister(0)); \
    } else {                                                            \
      __ asm_instr(i.OutputDoubleRegister(), i.InputOperand(0));        \
    }                                                                   \
  } while (false)

#define ASSEMBLE_AVX_BINOP(asm_instr)                                          \
  do {                                                                         \
    CpuFeatureScope avx_scope(tasm(), AVX);                                    \
    if (HasAddressingMode(instr)) {                                            \
      size_t index = 1;                                                        \
      Operand right = i.MemoryOperand(&index);                                 \
      __ asm_instr(i.OutputDoubleRegister(), i.InputDoubleRegister(0), right); \
    } else {                                                                   \
      if (instr->InputAt(1)->IsFPRegister()) {                                 \
        __ asm_instr(i.OutputDoubleRegister(), i.InputDoubleRegister(0),       \
                     i.InputDoubleRegister(1));                                \
      } else {                                                                 \
        __ asm_instr(i.OutputDoubleRegister(), i.InputDoubleRegister(0),       \
                     i.InputOperand(1));                                       \
      }                                                                        \
    }                                                                          \
  } while (false)

#define ASSEMBLE_IEEE754_BINOP(name)                                     \
  do {                                                                   \
    __ PrepareCallCFunction(2);                                          \
    __ CallCFunction(ExternalReference::ieee754_##name##_function(), 2); \
  } while (false)

#define ASSEMBLE_IEEE754_UNOP(name)                                      \
  do {                                                                   \
    __ PrepareCallCFunction(1);                                          \
    __ CallCFunction(ExternalReference::ieee754_##name##_function(), 1); \
  } while (false)

#define ASSEMBLE_ATOMIC_BINOP(bin_inst, mov_inst, cmpxchg_inst) \
  do {                                                          \
    Label binop;                                                \
    __ bind(&binop);                                            \
    __ mov_inst(rax, i.MemoryOperand(1));                       \
    __ movl(i.TempRegister(0), rax);                            \
    __ bin_inst(i.TempRegister(0), i.InputRegister(0));         \
    __ lock();                                                  \
    __ cmpxchg_inst(i.MemoryOperand(1), i.TempRegister(0));     \
    __ j(not_equal, &binop);                                    \
  } while (false)

#define ASSEMBLE_ATOMIC64_BINOP(bin_inst, mov_inst, cmpxchg_inst) \
  do {                                                            \
    Label binop;                                                  \
    __ bind(&binop);                                              \
    __ mov_inst(rax, i.MemoryOperand(1));                         \
    __ movq(i.TempRegister(0), rax);                              \
    __ bin_inst(i.TempRegister(0), i.InputRegister(0));           \
    __ lock();                                                    \
    __ cmpxchg_inst(i.MemoryOperand(1), i.TempRegister(0));       \
    __ j(not_equal, &binop);                                      \
  } while (false)

// Handles both SSE and AVX codegen. For SSE we use DefineSameAsFirst, so the
// dst and first src will be the same. For AVX we don't restrict it that way, so
// we will omit unnecessary moves.
#define ASSEMBLE_SIMD_BINOP(opcode)                                      \
  do {                                                                   \
    if (CpuFeatures::IsSupported(AVX)) {                                 \
      CpuFeatureScope avx_scope(tasm(), AVX);                            \
      __ v##opcode(i.OutputSimd128Register(), i.InputSimd128Register(0), \
                   i.InputSimd128Register(1));                           \
    } else {                                                             \
      DCHECK_EQ(i.OutputSimd128Register(), i.InputSimd128Register(0));   \
      __ opcode(i.OutputSimd128Register(), i.InputSimd128Register(1));   \
    }                                                                    \
  } while (false)

#define ASSEMBLE_SIMD_INSTR(opcode, dst_operand, index)      \
  do {                                                       \
    if (instr->InputAt(index)->IsSimd128Register()) {        \
      __ opcode(dst_operand, i.InputSimd128Register(index)); \
    } else {                                                 \
      __ opcode(dst_operand, i.InputOperand(index));         \
    }                                                        \
  } while (false)

#define ASSEMBLE_SIMD_IMM_INSTR(opcode, dst_operand, index, imm)  \
  do {                                                            \
    if (instr->InputAt(index)->IsSimd128Register()) {             \
      __ opcode(dst_operand, i.InputSimd128Register(index), imm); \
    } else {                                                      \
      __ opcode(dst_operand, i.InputOperand(index), imm);         \
    }                                                             \
  } while (false)

#define ASSEMBLE_SIMD_PUNPCK_SHUFFLE(opcode)                    \
  do {                                                          \
    XMMRegister dst = i.OutputSimd128Register();                \
    byte input_index = instr->InputCount() == 2 ? 1 : 0;        \
    if (CpuFeatures::IsSupported(AVX)) {                        \
      CpuFeatureScope avx_scope(tasm(), AVX);                   \
      DCHECK(instr->InputAt(input_index)->IsSimd128Register()); \
      __ v##opcode(dst, i.InputSimd128Register(0),              \
                   i.InputSimd128Register(input_index));        \
    } else {                                                    \
      DCHECK_EQ(dst, i.InputSimd128Register(0));                \
      ASSEMBLE_SIMD_INSTR(opcode, dst, input_index);            \
    }                                                           \
  } while (false)

#define ASSEMBLE_SIMD_IMM_SHUFFLE(opcode, imm)                \
  do {                                                        \
    XMMRegister dst = i.OutputSimd128Register();              \
    XMMRegister src = i.InputSimd128Register(0);              \
    if (CpuFeatures::IsSupported(AVX)) {                      \
      CpuFeatureScope avx_scope(tasm(), AVX);                 \
      DCHECK(instr->InputAt(1)->IsSimd128Register());         \
      __ v##opcode(dst, src, i.InputSimd128Register(1), imm); \
    } else {                                                  \
      DCHECK_EQ(dst, src);                                    \
      if (instr->InputAt(1)->IsSimd128Register()) {           \
        __ opcode(dst, i.InputSimd128Register(1), imm);       \
      } else {                                                \
        __ opcode(dst, i.InputOperand(1), imm);               \
      }                                                       \
    }                                                         \
  } while (false)

#define ASSEMBLE_SIMD_ALL_TRUE(opcode)                       \
  do {                                                       \
    Register dst = i.OutputRegister();                       \
    __ xorq(dst, dst);                                       \
    __ Pxor(kScratchDoubleReg, kScratchDoubleReg);           \
    __ opcode(kScratchDoubleReg, i.InputSimd128Register(0)); \
    __ Ptest(kScratchDoubleReg, kScratchDoubleReg);          \
    __ setcc(equal, dst);                                    \
  } while (false)

// This macro will directly emit the opcode if the shift is an immediate - the
// shift value will be taken modulo 2^width. Otherwise, it will emit code to
// perform the modulus operation.
#define ASSEMBLE_SIMD_SHIFT(opcode, width)                               \
  do {                                                                   \
    XMMRegister dst = i.OutputSimd128Register();                         \
    if (HasImmediateInput(instr, 1)) {                                   \
      if (CpuFeatures::IsSupported(AVX)) {                               \
        CpuFeatureScope avx_scope(tasm(), AVX);                          \
        __ v##opcode(dst, i.InputSimd128Register(0),                     \
                     byte{i.InputInt##width(1)});                        \
      } else {                                                           \
        DCHECK_EQ(dst, i.InputSimd128Register(0));                       \
        __ opcode(dst, byte{i.InputInt##width(1)});                      \
      }                                                                  \
    } else {                                                             \
      constexpr int mask = (1 << width) - 1;                             \
      __ movq(kScratchRegister, i.InputRegister(1));                     \
      __ andq(kScratchRegister, Immediate(mask));                        \
      __ Movq(kScratchDoubleReg, kScratchRegister);                      \
      if (CpuFeatures::IsSupported(AVX)) {                               \
        CpuFeatureScope avx_scope(tasm(), AVX);                          \
        __ v##opcode(dst, i.InputSimd128Register(0), kScratchDoubleReg); \
      } else {                                                           \
        DCHECK_EQ(dst, i.InputSimd128Register(0));                       \
        __ opcode(dst, kScratchDoubleReg);                               \
      }                                                                  \
    }                                                                    \
  } while (false)

#define ASSEMBLE_PINSR(ASM_INSTR)                                     \
  do {                                                                \
    EmitOOLTrapIfNeeded(zone(), this, opcode, instr, __ pc_offset()); \
    XMMRegister dst = i.OutputSimd128Register();                      \
    XMMRegister src = i.InputSimd128Register(0);                      \
    uint8_t laneidx = i.InputUint8(1);                                \
    if (HasAddressingMode(instr)) {                                   \
      __ ASM_INSTR(dst, src, i.MemoryOperand(2), laneidx);            \
      break;                                                          \
    }                                                                 \
    if (instr->InputAt(2)->IsFPRegister()) {                          \
      __ Movq(kScratchRegister, i.InputDoubleRegister(2));            \
      __ ASM_INSTR(dst, src, kScratchRegister, laneidx);              \
    } else if (instr->InputAt(2)->IsRegister()) {                     \
      __ ASM_INSTR(dst, src, i.InputRegister(2), laneidx);            \
    } else {                                                          \
      __ ASM_INSTR(dst, src, i.InputOperand(2), laneidx);             \
    }                                                                 \
  } while (false)

void CodeGenerator::AssembleDeconstructFrame() {
  unwinding_info_writer_.MarkFrameDeconstructed(__ pc_offset());
  __ movq(rsp, rbp);
  __ popq(rbp);
}

void CodeGenerator::AssemblePrepareTailCall() {
  if (frame_access_state()->has_frame()) {
    __ movq(rbp, MemOperand(rbp, 0));
  }
  frame_access_state()->SetFrameAccessToSP();
}

namespace {

void AdjustStackPointerForTailCall(Instruction* instr,
                                   TurboAssembler* assembler, Linkage* linkage,
                                   OptimizedCompilationInfo* info,
                                   FrameAccessState* state,
                                   int new_slot_above_sp,
                                   bool allow_shrinkage = true) {
  int stack_slot_delta;
  if (instr->HasCallDescriptorFlag(CallDescriptor::kIsTailCallForTierUp)) {
    // For this special tail-call mode, the callee has the same arguments and
    // linkage as the caller, and arguments adapter frames must be preserved.
    // Thus we simply have reset the stack pointer register to its original
    // value before frame construction.
    // See also: AssembleConstructFrame.
    DCHECK(!info->is_osr());
    DCHECK_EQ(linkage->GetIncomingDescriptor()->CalleeSavedRegisters(), 0);
    DCHECK_EQ(linkage->GetIncomingDescriptor()->CalleeSavedFPRegisters(), 0);
    DCHECK_EQ(state->frame()->GetReturnSlotCount(), 0);
    stack_slot_delta = (state->frame()->GetTotalFrameSlotCount() -
                        kReturnAddressStackSlotCount) *
                       -1;
    DCHECK_LE(stack_slot_delta, 0);
  } else {
    int current_sp_offset = state->GetSPToFPSlotCount() +
                            StandardFrameConstants::kFixedSlotCountAboveFp;
    stack_slot_delta = new_slot_above_sp - current_sp_offset;
  }

  if (stack_slot_delta > 0) {
    assembler->AllocateStackSpace(stack_slot_delta * kSystemPointerSize);
    state->IncreaseSPDelta(stack_slot_delta);
  } else if (allow_shrinkage && stack_slot_delta < 0) {
    assembler->addq(rsp, Immediate(-stack_slot_delta * kSystemPointerSize));
    state->IncreaseSPDelta(stack_slot_delta);
  }
}

void SetupSimdImmediateInRegister(TurboAssembler* assembler, uint32_t* imms,
                                  XMMRegister reg) {
  assembler->Move(reg, make_uint64(imms[3], imms[2]),
                  make_uint64(imms[1], imms[0]));
}

}  // namespace

void CodeGenerator::AssembleTailCallBeforeGap(Instruction* instr,
                                              int first_unused_slot_offset) {
  CodeGenerator::PushTypeFlags flags(kImmediatePush | kScalarPush);
  ZoneVector<MoveOperands*> pushes(zone());
  GetPushCompatibleMoves(instr, flags, &pushes);

  if (!pushes.empty() &&
      (LocationOperand::cast(pushes.back()->destination()).index() + 1 ==
       first_unused_slot_offset)) {
    DCHECK(!instr->HasCallDescriptorFlag(CallDescriptor::kIsTailCallForTierUp));
    X64OperandConverter g(this, instr);
    for (auto move : pushes) {
      LocationOperand destination_location(
          LocationOperand::cast(move->destination()));
      InstructionOperand source(move->source());
      AdjustStackPointerForTailCall(instr, tasm(), linkage(), info(),
                                    frame_access_state(),
                                    destination_location.index());
      if (source.IsStackSlot()) {
        LocationOperand source_location(LocationOperand::cast(source));
        __ Push(g.SlotToOperand(source_location.index()));
      } else if (source.IsRegister()) {
        LocationOperand source_location(LocationOperand::cast(source));
        __ Push(source_location.GetRegister());
      } else if (source.IsImmediate()) {
        __ Push(Immediate(ImmediateOperand::cast(source).inline_int32_value()));
      } else {
        // Pushes of non-scalar data types is not supported.
        UNIMPLEMENTED();
      }
      frame_access_state()->IncreaseSPDelta(1);
      move->Eliminate();
    }
  }
  AdjustStackPointerForTailCall(instr, tasm(), linkage(), info(),
                                frame_access_state(), first_unused_slot_offset,
                                false);
}

void CodeGenerator::AssembleTailCallAfterGap(Instruction* instr,
                                             int first_unused_slot_offset) {
  AdjustStackPointerForTailCall(instr, tasm(), linkage(), info(),
                                frame_access_state(), first_unused_slot_offset);
}

// Check that {kJavaScriptCallCodeStartRegister} is correct.
void CodeGenerator::AssembleCodeStartRegisterCheck() {
  __ ComputeCodeStartAddress(rbx);
  __ cmpq(rbx, kJavaScriptCallCodeStartRegister);
  __ Assert(equal, AbortReason::kWrongFunctionCodeStart);
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
  __ LoadTaggedPointerField(rbx,
                            Operand(kJavaScriptCallCodeStartRegister, offset));
  __ testl(FieldOperand(rbx, CodeDataContainer::kKindSpecificFlagsOffset),
           Immediate(1 << Code::kMarkedForDeoptimizationBit));
  __ Jump(BUILTIN_CODE(isolate(), CompileLazyDeoptimizedCode),
          RelocInfo::CODE_TARGET, not_zero);
}

void CodeGenerator::GenerateSpeculationPoisonFromCodeStartRegister() {
  // Set a mask which has all bits set in the normal case, but has all
  // bits cleared if we are speculatively executing the wrong PC.
  __ ComputeCodeStartAddress(rbx);
  __ xorq(kSpeculationPoisonRegister, kSpeculationPoisonRegister);
  __ cmpq(kJavaScriptCallCodeStartRegister, rbx);
  __ movq(rbx, Immediate(-1));
  __ cmovq(equal, kSpeculationPoisonRegister, rbx);
}

void CodeGenerator::AssembleRegisterArgumentPoisoning() {
  __ andq(kJSFunctionRegister, kSpeculationPoisonRegister);
  __ andq(kContextRegister, kSpeculationPoisonRegister);
  __ andq(rsp, kSpeculationPoisonRegister);
}

// Assembles an instruction after register allocation, producing machine code.
CodeGenerator::CodeGenResult CodeGenerator::AssembleArchInstruction(
    Instruction* instr) {
  X64OperandConverter i(this, instr);
  InstructionCode opcode = instr->opcode();
  ArchOpcode arch_opcode = ArchOpcodeField::decode(opcode);
  switch (arch_opcode) {
    case kArchCallCodeObject: {
      if (HasImmediateInput(instr, 0)) {
        Handle<Code> code = i.InputCode(0);
        __ Call(code, RelocInfo::CODE_TARGET);
      } else {
        Register reg = i.InputRegister(0);
        DCHECK_IMPLIES(
            instr->HasCallDescriptorFlag(CallDescriptor::kFixedTargetRegister),
            reg == kJavaScriptCallCodeStartRegister);
        __ LoadCodeObjectEntry(reg, reg);
        if (instr->HasCallDescriptorFlag(CallDescriptor::kRetpoline)) {
          __ RetpolineCall(reg);
        } else {
          __ call(reg);
        }
      }
      RecordCallPosition(instr);
      frame_access_state()->ClearSPDelta();
      break;
    }
    case kArchCallBuiltinPointer: {
      DCHECK(!HasImmediateInput(instr, 0));
      Register builtin_index = i.InputRegister(0);
      __ CallBuiltinByIndex(builtin_index);
      RecordCallPosition(instr);
      frame_access_state()->ClearSPDelta();
      break;
    }
#if V8_ENABLE_WEBASSEMBLY
    case kArchCallWasmFunction: {
      if (HasImmediateInput(instr, 0)) {
        Constant constant = i.ToConstant(instr->InputAt(0));
        Address wasm_code = static_cast<Address>(constant.ToInt64());
        if (DetermineStubCallMode() == StubCallMode::kCallWasmRuntimeStub) {
          __ near_call(wasm_code, constant.rmode());
        } else {
          if (instr->HasCallDescriptorFlag(CallDescriptor::kRetpoline)) {
            __ RetpolineCall(wasm_code, constant.rmode());
          } else {
            __ Call(wasm_code, constant.rmode());
          }
        }
      } else {
        Register reg = i.InputRegister(0);
        if (instr->HasCallDescriptorFlag(CallDescriptor::kRetpoline)) {
          __ RetpolineCall(reg);
        } else {
          __ call(reg);
        }
      }
      RecordCallPosition(instr);
      frame_access_state()->ClearSPDelta();
      break;
    }
    case kArchTailCallWasm: {
      if (HasImmediateInput(instr, 0)) {
        Constant constant = i.ToConstant(instr->InputAt(0));
        Address wasm_code = static_cast<Address>(constant.ToInt64());
        if (DetermineStubCallMode() == StubCallMode::kCallWasmRuntimeStub) {
          __ near_jmp(wasm_code, constant.rmode());
        } else {
          __ Move(kScratchRegister, wasm_code, constant.rmode());
          __ jmp(kScratchRegister);
        }
      } else {
        Register reg = i.InputRegister(0);
        if (instr->HasCallDescriptorFlag(CallDescriptor::kRetpoline)) {
          __ RetpolineJump(reg);
        } else {
          __ jmp(reg);
        }
      }
      unwinding_info_writer_.MarkBlockWillExit();
      frame_access_state()->ClearSPDelta();
      frame_access_state()->SetFrameAccessToDefault();
      break;
    }
#endif  // V8_ENABLE_WEBASSEMBLY
    case kArchTailCallCodeObject: {
      if (HasImmediateInput(instr, 0)) {
        Handle<Code> code = i.InputCode(0);
        __ Jump(code, RelocInfo::CODE_TARGET);
      } else {
        Register reg = i.InputRegister(0);
        DCHECK_IMPLIES(
            instr->HasCallDescriptorFlag(CallDescriptor::kFixedTargetRegister),
            reg == kJavaScriptCallCodeStartRegister);
        __ LoadCodeObjectEntry(reg, reg);
        if (instr->HasCallDescriptorFlag(CallDescriptor::kRetpoline)) {
          __ RetpolineJump(reg);
        } else {
          __ jmp(reg);
        }
      }
      unwinding_info_writer_.MarkBlockWillExit();
      frame_access_state()->ClearSPDelta();
      frame_access_state()->SetFrameAccessToDefault();
      break;
    }
    case kArchTailCallAddress: {
      CHECK(!HasImmediateInput(instr, 0));
      Register reg = i.InputRegister(0);
      DCHECK_IMPLIES(
          instr->HasCallDescriptorFlag(CallDescriptor::kFixedTargetRegister),
          reg == kJavaScriptCallCodeStartRegister);
      if (instr->HasCallDescriptorFlag(CallDescriptor::kRetpoline)) {
        __ RetpolineJump(reg);
      } else {
        __ jmp(reg);
      }
      unwinding_info_writer_.MarkBlockWillExit();
      frame_access_state()->ClearSPDelta();
      frame_access_state()->SetFrameAccessToDefault();
      break;
    }
    case kArchCallJSFunction: {
      Register func = i.InputRegister(0);
      if (FLAG_debug_code) {
        // Check the function's context matches the context argument.
        __ cmp_tagged(rsi, FieldOperand(func, JSFunction::kContextOffset));
        __ Assert(equal, AbortReason::kWrongFunctionContext);
      }
      static_assert(kJavaScriptCallCodeStartRegister == rcx, "ABI mismatch");
      __ LoadTaggedPointerField(rcx,
                                FieldOperand(func, JSFunction::kCodeOffset));
      __ CallCodeObject(rcx);
      frame_access_state()->ClearSPDelta();
      RecordCallPosition(instr);
      break;
    }
    case kArchPrepareCallCFunction: {
      // Frame alignment requires using FP-relative frame addressing.
      frame_access_state()->SetFrameAccessToFP();
      int const num_parameters = MiscField::decode(instr->opcode());
      __ PrepareCallCFunction(num_parameters);
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
      Label return_location;
#if V8_ENABLE_WEBASSEMBLY
      if (linkage()->GetIncomingDescriptor()->IsWasmCapiFunction()) {
        // Put the return address in a stack slot.
        __ leaq(kScratchRegister, Operand(&return_location, 0));
        __ movq(MemOperand(rbp, WasmExitFrameConstants::kCallingPCOffset),
                kScratchRegister);
      }
#endif  // V8_ENABLE_WEBASSEMBLY
      if (HasImmediateInput(instr, 0)) {
        ExternalReference ref = i.InputExternalReference(0);
        __ CallCFunction(ref, num_parameters);
      } else {
        Register func = i.InputRegister(0);
        __ CallCFunction(func, num_parameters);
      }
      __ bind(&return_location);
#if V8_ENABLE_WEBASSEMBLY
      if (linkage()->GetIncomingDescriptor()->IsWasmCapiFunction()) {
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
      // TODO(turbofan): Do we need an lfence here?
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
    case kArchComment:
      __ RecordComment(reinterpret_cast<const char*>(i.InputInt64(0)));
      break;
    case kArchAbortCSAAssert:
      DCHECK(i.InputRegister(0) == rdx);
      {
        // We don't actually want to generate a pile of code for this, so just
        // claim there is a stack frame, without generating one.
        FrameScope scope(tasm(), StackFrame::NONE);
        __ Call(
            isolate()->builtins()->builtin_handle(Builtins::kAbortCSAAssert),
            RelocInfo::CODE_TARGET);
      }
      __ int3();
      unwinding_info_writer_.MarkBlockWillExit();
      break;
    case kArchDebugBreak:
      __ DebugBreak();
      break;
    case kArchThrowTerminator:
      unwinding_info_writer_.MarkBlockWillExit();
      break;
    case kArchNop:
      // don't emit code for nops.
      break;
    case kArchDeoptimize: {
      DeoptimizationExit* exit =
          BuildTranslation(instr, -1, 0, 0, OutputFrameStateCombine::Ignore());
      __ jmp(exit->label());
      break;
    }
    case kArchRet:
      AssembleReturn(instr->InputAt(0));
      break;
    case kArchFramePointer:
      __ movq(i.OutputRegister(), rbp);
      break;
    case kArchParentFramePointer:
      if (frame_access_state()->has_frame()) {
        __ movq(i.OutputRegister(), Operand(rbp, 0));
      } else {
        __ movq(i.OutputRegister(), rbp);
      }
      break;
    case kArchStackPointerGreaterThan: {
      // Potentially apply an offset to the current stack pointer before the
      // comparison to consider the size difference of an optimized frame versus
      // the contained unoptimized frames.

      Register lhs_register = rsp;
      uint32_t offset;

      if (ShouldApplyOffsetToStackCheck(instr, &offset)) {
        lhs_register = kScratchRegister;
        __ leaq(lhs_register, Operand(rsp, static_cast<int32_t>(offset) * -1));
      }

      constexpr size_t kValueIndex = 0;
      if (HasAddressingMode(instr)) {
        __ cmpq(lhs_register, i.MemoryOperand(kValueIndex));
      } else {
        __ cmpq(lhs_register, i.InputRegister(kValueIndex));
      }
      break;
    }
    case kArchStackCheckOffset:
      __ Move(i.OutputRegister(), Smi::FromInt(GetStackCheckOffset()));
      break;
    case kArchTruncateDoubleToI: {
      auto result = i.OutputRegister();
      auto input = i.InputDoubleRegister(0);
      auto ool = zone()->New<OutOfLineTruncateDoubleToI>(
          this, result, input, DetermineStubCallMode(),
          &unwinding_info_writer_);
      // We use Cvttsd2siq instead of Cvttsd2si due to performance reasons. The
      // use of Cvttsd2siq requires the movl below to avoid sign extension.
      __ Cvttsd2siq(result, input);
      __ cmpq(result, Immediate(1));
      __ j(overflow, ool->entry());
      __ bind(ool->exit());
      __ movl(result, result);
      break;
    }
    case kArchStoreWithWriteBarrier: {
      RecordWriteMode mode =
          static_cast<RecordWriteMode>(MiscField::decode(instr->opcode()));
      Register object = i.InputRegister(0);
      size_t index = 0;
      Operand operand = i.MemoryOperand(&index);
      Register value = i.InputRegister(index);
      Register scratch0 = i.TempRegister(0);
      Register scratch1 = i.TempRegister(1);
      auto ool = zone()->New<OutOfLineRecordWrite>(this, object, operand, value,
                                                   scratch0, scratch1, mode,
                                                   DetermineStubCallMode());
      __ StoreTaggedField(operand, value);
      __ CheckPageFlag(object, scratch0,
                       MemoryChunk::kPointersFromHereAreInterestingMask,
                       not_zero, ool->entry());
      __ bind(ool->exit());
      break;
    }
    case kArchWordPoisonOnSpeculation:
      DCHECK_EQ(i.OutputRegister(), i.InputRegister(0));
      __ andq(i.InputRegister(0), kSpeculationPoisonRegister);
      break;
    case kX64MFence:
      __ mfence();
      break;
    case kX64LFence:
      __ lfence();
      break;
    case kArchStackSlot: {
      FrameOffset offset =
          frame_access_state()->GetFrameOffset(i.InputInt32(0));
      Register base = offset.from_stack_pointer() ? rsp : rbp;
      __ leaq(i.OutputRegister(), Operand(base, offset.offset()));
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
    case kX64Add32:
      ASSEMBLE_BINOP(addl);
      break;
    case kX64Add:
      ASSEMBLE_BINOP(addq);
      break;
    case kX64Sub32:
      ASSEMBLE_BINOP(subl);
      break;
    case kX64Sub:
      ASSEMBLE_BINOP(subq);
      break;
    case kX64And32:
      ASSEMBLE_BINOP(andl);
      break;
    case kX64And:
      ASSEMBLE_BINOP(andq);
      break;
    case kX64Cmp8:
      ASSEMBLE_COMPARE(cmpb);
      break;
    case kX64Cmp16:
      ASSEMBLE_COMPARE(cmpw);
      break;
    case kX64Cmp32:
      ASSEMBLE_COMPARE(cmpl);
      break;
    case kX64Cmp:
      ASSEMBLE_COMPARE(cmpq);
      break;
    case kX64Test8:
      ASSEMBLE_COMPARE(testb);
      break;
    case kX64Test16:
      ASSEMBLE_COMPARE(testw);
      break;
    case kX64Test32:
      ASSEMBLE_COMPARE(testl);
      break;
    case kX64Test:
      ASSEMBLE_COMPARE(testq);
      break;
    case kX64Imul32:
      ASSEMBLE_MULT(imull);
      break;
    case kX64Imul:
      ASSEMBLE_MULT(imulq);
      break;
    case kX64ImulHigh32:
      if (HasRegisterInput(instr, 1)) {
        __ imull(i.InputRegister(1));
      } else {
        __ imull(i.InputOperand(1));
      }
      break;
    case kX64UmulHigh32:
      if (HasRegisterInput(instr, 1)) {
        __ mull(i.InputRegister(1));
      } else {
        __ mull(i.InputOperand(1));
      }
      break;
    case kX64Idiv32:
      __ cdq();
      __ idivl(i.InputRegister(1));
      break;
    case kX64Idiv:
      __ cqo();
      __ idivq(i.InputRegister(1));
      break;
    case kX64Udiv32:
      __ xorl(rdx, rdx);
      __ divl(i.InputRegister(1));
      break;
    case kX64Udiv:
      __ xorq(rdx, rdx);
      __ divq(i.InputRegister(1));
      break;
    case kX64Not:
      ASSEMBLE_UNOP(notq);
      break;
    case kX64Not32:
      ASSEMBLE_UNOP(notl);
      break;
    case kX64Neg:
      ASSEMBLE_UNOP(negq);
      break;
    case kX64Neg32:
      ASSEMBLE_UNOP(negl);
      break;
    case kX64Or32:
      ASSEMBLE_BINOP(orl);
      break;
    case kX64Or:
      ASSEMBLE_BINOP(orq);
      break;
    case kX64Xor32:
      ASSEMBLE_BINOP(xorl);
      break;
    case kX64Xor:
      ASSEMBLE_BINOP(xorq);
      break;
    case kX64Shl32:
      ASSEMBLE_SHIFT(shll, 5);
      break;
    case kX64Shl:
      ASSEMBLE_SHIFT(shlq, 6);
      break;
    case kX64Shr32:
      ASSEMBLE_SHIFT(shrl, 5);
      break;
    case kX64Shr:
      ASSEMBLE_SHIFT(shrq, 6);
      break;
    case kX64Sar32:
      ASSEMBLE_SHIFT(sarl, 5);
      break;
    case kX64Sar:
      ASSEMBLE_SHIFT(sarq, 6);
      break;
    case kX64Rol32:
      ASSEMBLE_SHIFT(roll, 5);
      break;
    case kX64Rol:
      ASSEMBLE_SHIFT(rolq, 6);
      break;
    case kX64Ror32:
      ASSEMBLE_SHIFT(rorl, 5);
      break;
    case kX64Ror:
      ASSEMBLE_SHIFT(rorq, 6);
      break;
    case kX64Lzcnt:
      if (HasRegisterInput(instr, 0)) {
        __ Lzcntq(i.OutputRegister(), i.InputRegister(0));
      } else {
        __ Lzcntq(i.OutputRegister(), i.InputOperand(0));
      }
      break;
    case kX64Lzcnt32:
      if (HasRegisterInput(instr, 0)) {
        __ Lzcntl(i.OutputRegister(), i.InputRegister(0));
      } else {
        __ Lzcntl(i.OutputRegister(), i.InputOperand(0));
      }
      break;
    case kX64Tzcnt:
      if (HasRegisterInput(instr, 0)) {
        __ Tzcntq(i.OutputRegister(), i.InputRegister(0));
      } else {
        __ Tzcntq(i.OutputRegister(), i.InputOperand(0));
      }
      break;
    case kX64Tzcnt32:
      if (HasRegisterInput(instr, 0)) {
        __ Tzcntl(i.OutputRegister(), i.InputRegister(0));
      } else {
        __ Tzcntl(i.OutputRegister(), i.InputOperand(0));
      }
      break;
    case kX64Popcnt:
      if (HasRegisterInput(instr, 0)) {
        __ Popcntq(i.OutputRegister(), i.InputRegister(0));
      } else {
        __ Popcntq(i.OutputRegister(), i.InputOperand(0));
      }
      break;
    case kX64Popcnt32:
      if (HasRegisterInput(instr, 0)) {
        __ Popcntl(i.OutputRegister(), i.InputRegister(0));
      } else {
        __ Popcntl(i.OutputRegister(), i.InputOperand(0));
      }
      break;
    case kX64Bswap:
      __ bswapq(i.OutputRegister());
      break;
    case kX64Bswap32:
      __ bswapl(i.OutputRegister());
      break;
    case kSSEFloat32Cmp:
      ASSEMBLE_SSE_BINOP(Ucomiss);
      break;
    case kSSEFloat32Add:
      ASSEMBLE_SSE_BINOP(addss);
      break;
    case kSSEFloat32Sub:
      ASSEMBLE_SSE_BINOP(subss);
      break;
    case kSSEFloat32Mul:
      ASSEMBLE_SSE_BINOP(mulss);
      break;
    case kSSEFloat32Div:
      ASSEMBLE_SSE_BINOP(divss);
      // Don't delete this mov. It may improve performance on some CPUs,
      // when there is a (v)mulss depending on the result.
      __ movaps(i.OutputDoubleRegister(), i.OutputDoubleRegister());
      break;
    case kSSEFloat32Abs: {
      // TODO(bmeurer): Use RIP relative 128-bit constants.
      XMMRegister tmp = i.ToDoubleRegister(instr->TempAt(0));
      __ Pcmpeqd(tmp, tmp);
      __ Psrlq(tmp, 33);
      __ Andps(i.OutputDoubleRegister(), tmp);
      break;
    }
    case kSSEFloat32Neg: {
      // TODO(bmeurer): Use RIP relative 128-bit constants.
      XMMRegister tmp = i.ToDoubleRegister(instr->TempAt(0));
      __ Pcmpeqd(tmp, tmp);
      __ Psllq(tmp, 31);
      __ Xorps(i.OutputDoubleRegister(), tmp);
      break;
    }
    case kSSEFloat32Sqrt:
      ASSEMBLE_SSE_UNOP(sqrtss);
      break;
    case kSSEFloat32ToFloat64:
      ASSEMBLE_SSE_UNOP(Cvtss2sd);
      break;
    case kSSEFloat32Round: {
      CpuFeatureScope sse_scope(tasm(), SSE4_1);
      RoundingMode const mode =
          static_cast<RoundingMode>(MiscField::decode(instr->opcode()));
      __ Roundss(i.OutputDoubleRegister(), i.InputDoubleRegister(0), mode);
      break;
    }
    case kSSEFloat32ToInt32:
      if (instr->InputAt(0)->IsFPRegister()) {
        __ Cvttss2si(i.OutputRegister(), i.InputDoubleRegister(0));
      } else {
        __ Cvttss2si(i.OutputRegister(), i.InputOperand(0));
      }
      break;
    case kSSEFloat32ToUint32: {
      if (instr->InputAt(0)->IsFPRegister()) {
        __ Cvttss2siq(i.OutputRegister(), i.InputDoubleRegister(0));
      } else {
        __ Cvttss2siq(i.OutputRegister(), i.InputOperand(0));
      }
      break;
    }
    case kSSEFloat64Cmp:
      ASSEMBLE_SSE_BINOP(Ucomisd);
      break;
    case kSSEFloat64Add:
      ASSEMBLE_SSE_BINOP(addsd);
      break;
    case kSSEFloat64Sub:
      ASSEMBLE_SSE_BINOP(subsd);
      break;
    case kSSEFloat64Mul:
      ASSEMBLE_SSE_BINOP(mulsd);
      break;
    case kSSEFloat64Div:
      ASSEMBLE_SSE_BINOP(divsd);
      // Don't delete this mov. It may improve performance on some CPUs,
      // when there is a (v)mulsd depending on the result.
      __ Movapd(i.OutputDoubleRegister(), i.OutputDoubleRegister());
      break;
    case kSSEFloat64Mod: {
      __ AllocateStackSpace(kDoubleSize);
      unwinding_info_writer_.MaybeIncreaseBaseOffsetAt(__ pc_offset(),
                                                       kDoubleSize);
      // Move values to st(0) and st(1).
      __ Movsd(Operand(rsp, 0), i.InputDoubleRegister(1));
      __ fld_d(Operand(rsp, 0));
      __ Movsd(Operand(rsp, 0), i.InputDoubleRegister(0));
      __ fld_d(Operand(rsp, 0));
      // Loop while fprem isn't done.
      Label mod_loop;
      __ bind(&mod_loop);
      // This instructions traps on all kinds inputs, but we are assuming the
      // floating point control word is set to ignore them all.
      __ fprem();
      // The following 2 instruction implicitly use rax.
      __ fnstsw_ax();
      if (CpuFeatures::IsSupported(SAHF)) {
        CpuFeatureScope sahf_scope(tasm(), SAHF);
        __ sahf();
      } else {
        __ shrl(rax, Immediate(8));
        __ andl(rax, Immediate(0xFF));
        __ pushq(rax);
        unwinding_info_writer_.MaybeIncreaseBaseOffsetAt(__ pc_offset(),
                                                         kSystemPointerSize);
        __ popfq();
        unwinding_info_writer_.MaybeIncreaseBaseOffsetAt(__ pc_offset(),
                                                         -kSystemPointerSize);
      }
      __ j(parity_even, &mod_loop);
      // Move output to stack and clean up.
      __ fstp(1);
      __ fstp_d(Operand(rsp, 0));
      __ Movsd(i.OutputDoubleRegister(), Operand(rsp, 0));
      __ addq(rsp, Immediate(kDoubleSize));
      unwinding_info_writer_.MaybeIncreaseBaseOffsetAt(__ pc_offset(),
                                                       -kDoubleSize);
      break;
    }
    case kSSEFloat32Max: {
      Label compare_swap, done_compare;
      if (instr->InputAt(1)->IsFPRegister()) {
        __ Ucomiss(i.InputDoubleRegister(0), i.InputDoubleRegister(1));
      } else {
        __ Ucomiss(i.InputDoubleRegister(0), i.InputOperand(1));
      }
      auto ool =
          zone()->New<OutOfLineLoadFloat32NaN>(this, i.OutputDoubleRegister());
      __ j(parity_even, ool->entry());
      __ j(above, &done_compare, Label::kNear);
      __ j(below, &compare_swap, Label::kNear);
      __ Movmskps(kScratchRegister, i.InputDoubleRegister(0));
      __ testl(kScratchRegister, Immediate(1));
      __ j(zero, &done_compare, Label::kNear);
      __ bind(&compare_swap);
      if (instr->InputAt(1)->IsFPRegister()) {
        __ Movss(i.InputDoubleRegister(0), i.InputDoubleRegister(1));
      } else {
        __ Movss(i.InputDoubleRegister(0), i.InputOperand(1));
      }
      __ bind(&done_compare);
      __ bind(ool->exit());
      break;
    }
    case kSSEFloat32Min: {
      Label compare_swap, done_compare;
      if (instr->InputAt(1)->IsFPRegister()) {
        __ Ucomiss(i.InputDoubleRegister(0), i.InputDoubleRegister(1));
      } else {
        __ Ucomiss(i.InputDoubleRegister(0), i.InputOperand(1));
      }
      auto ool =
          zone()->New<OutOfLineLoadFloat32NaN>(this, i.OutputDoubleRegister());
      __ j(parity_even, ool->entry());
      __ j(below, &done_compare, Label::kNear);
      __ j(above, &compare_swap, Label::kNear);
      if (instr->InputAt(1)->IsFPRegister()) {
        __ Movmskps(kScratchRegister, i.InputDoubleRegister(1));
      } else {
        __ Movss(kScratchDoubleReg, i.InputOperand(1));
        __ Movmskps(kScratchRegister, kScratchDoubleReg);
      }
      __ testl(kScratchRegister, Immediate(1));
      __ j(zero, &done_compare, Label::kNear);
      __ bind(&compare_swap);
      if (instr->InputAt(1)->IsFPRegister()) {
        __ Movss(i.InputDoubleRegister(0), i.InputDoubleRegister(1));
      } else {
        __ Movss(i.InputDoubleRegister(0), i.InputOperand(1));
      }
      __ bind(&done_compare);
      __ bind(ool->exit());
      break;
    }
    case kSSEFloat64Max: {
      Label compare_swap, done_compare;
      if (instr->InputAt(1)->IsFPRegister()) {
        __ Ucomisd(i.InputDoubleRegister(0), i.InputDoubleRegister(1));
      } else {
        __ Ucomisd(i.InputDoubleRegister(0), i.InputOperand(1));
      }
      auto ool =
          zone()->New<OutOfLineLoadFloat64NaN>(this, i.OutputDoubleRegister());
      __ j(parity_even, ool->entry());
      __ j(above, &done_compare, Label::kNear);
      __ j(below, &compare_swap, Label::kNear);
      __ Movmskpd(kScratchRegister, i.InputDoubleRegister(0));
      __ testl(kScratchRegister, Immediate(1));
      __ j(zero, &done_compare, Label::kNear);
      __ bind(&compare_swap);
      if (instr->InputAt(1)->IsFPRegister()) {
        __ Movsd(i.InputDoubleRegister(0), i.InputDoubleRegister(1));
      } else {
        __ Movsd(i.InputDoubleRegister(0), i.InputOperand(1));
      }
      __ bind(&done_compare);
      __ bind(ool->exit());
      break;
    }
    case kSSEFloat64Min: {
      Label compare_swap, done_compare;
      if (instr->InputAt(1)->IsFPRegister()) {
        __ Ucomisd(i.InputDoubleRegister(0), i.InputDoubleRegister(1));
      } else {
        __ Ucomisd(i.InputDoubleRegister(0), i.InputOperand(1));
      }
      auto ool =
          zone()->New<OutOfLineLoadFloat64NaN>(this, i.OutputDoubleRegister());
      __ j(parity_even, ool->entry());
      __ j(below, &done_compare, Label::kNear);
      __ j(above, &compare_swap, Label::kNear);
      if (instr->InputAt(1)->IsFPRegister()) {
        __ Movmskpd(kScratchRegister, i.InputDoubleRegister(1));
      } else {
        __ Movsd(kScratchDoubleReg, i.InputOperand(1));
        __ Movmskpd(kScratchRegister, kScratchDoubleReg);
      }
      __ testl(kScratchRegister, Immediate(1));
      __ j(zero, &done_compare, Label::kNear);
      __ bind(&compare_swap);
      if (instr->InputAt(1)->IsFPRegister()) {
        __ Movsd(i.InputDoubleRegister(0), i.InputDoubleRegister(1));
      } else {
        __ Movsd(i.InputDoubleRegister(0), i.InputOperand(1));
      }
      __ bind(&done_compare);
      __ bind(ool->exit());
      break;
    }
    case kX64F64x2Abs:
    case kSSEFloat64Abs: {
      __ Abspd(i.OutputDoubleRegister());
      break;
    }
    case kX64F64x2Neg:
    case kSSEFloat64Neg: {
      __ Negpd(i.OutputDoubleRegister());
      break;
    }
    case kSSEFloat64Sqrt:
      ASSEMBLE_SSE_UNOP(Sqrtsd);
      break;
    case kSSEFloat64Round: {
      CpuFeatureScope sse_scope(tasm(), SSE4_1);
      RoundingMode const mode =
          static_cast<RoundingMode>(MiscField::decode(instr->opcode()));
      __ Roundsd(i.OutputDoubleRegister(), i.InputDoubleRegister(0), mode);
      break;
    }
    case kSSEFloat64ToFloat32:
      ASSEMBLE_SSE_UNOP(Cvtsd2ss);
      break;
    case kSSEFloat64ToInt32:
      if (instr->InputAt(0)->IsFPRegister()) {
        __ Cvttsd2si(i.OutputRegister(), i.InputDoubleRegister(0));
      } else {
        __ Cvttsd2si(i.OutputRegister(), i.InputOperand(0));
      }
      break;
    case kSSEFloat64ToUint32: {
      if (instr->InputAt(0)->IsFPRegister()) {
        __ Cvttsd2siq(i.OutputRegister(), i.InputDoubleRegister(0));
      } else {
        __ Cvttsd2siq(i.OutputRegister(), i.InputOperand(0));
      }
      if (MiscField::decode(instr->opcode())) {
        __ AssertZeroExtended(i.OutputRegister());
      }
      break;
    }
    case kSSEFloat32ToInt64:
      if (instr->InputAt(0)->IsFPRegister()) {
        __ Cvttss2siq(i.OutputRegister(), i.InputDoubleRegister(0));
      } else {
        __ Cvttss2siq(i.OutputRegister(), i.InputOperand(0));
      }
      if (instr->OutputCount() > 1) {
        __ Set(i.OutputRegister(1), 1);
        Label done;
        Label fail;
        __ Move(kScratchDoubleReg, static_cast<float>(INT64_MIN));
        if (instr->InputAt(0)->IsFPRegister()) {
          __ Ucomiss(kScratchDoubleReg, i.InputDoubleRegister(0));
        } else {
          __ Ucomiss(kScratchDoubleReg, i.InputOperand(0));
        }
        // If the input is NaN, then the conversion fails.
        __ j(parity_even, &fail, Label::kNear);
        // If the input is INT64_MIN, then the conversion succeeds.
        __ j(equal, &done, Label::kNear);
        __ cmpq(i.OutputRegister(0), Immediate(1));
        // If the conversion results in INT64_MIN, but the input was not
        // INT64_MIN, then the conversion fails.
        __ j(no_overflow, &done, Label::kNear);
        __ bind(&fail);
        __ Set(i.OutputRegister(1), 0);
        __ bind(&done);
      }
      break;
    case kSSEFloat64ToInt64:
      if (instr->InputAt(0)->IsFPRegister()) {
        __ Cvttsd2siq(i.OutputRegister(0), i.InputDoubleRegister(0));
      } else {
        __ Cvttsd2siq(i.OutputRegister(0), i.InputOperand(0));
      }
      if (instr->OutputCount() > 1) {
        __ Set(i.OutputRegister(1), 1);
        Label done;
        Label fail;
        __ Move(kScratchDoubleReg, static_cast<double>(INT64_MIN));
        if (instr->InputAt(0)->IsFPRegister()) {
          __ Ucomisd(kScratchDoubleReg, i.InputDoubleRegister(0));
        } else {
          __ Ucomisd(kScratchDoubleReg, i.InputOperand(0));
        }
        // If the input is NaN, then the conversion fails.
        __ j(parity_even, &fail, Label::kNear);
        // If the input is INT64_MIN, then the conversion succeeds.
        __ j(equal, &done, Label::kNear);
        __ cmpq(i.OutputRegister(0), Immediate(1));
        // If the conversion results in INT64_MIN, but the input was not
        // INT64_MIN, then the conversion fails.
        __ j(no_overflow, &done, Label::kNear);
        __ bind(&fail);
        __ Set(i.OutputRegister(1), 0);
        __ bind(&done);
      }
      break;
    case kSSEFloat32ToUint64: {
      Label fail;
      if (instr->OutputCount() > 1) __ Set(i.OutputRegister(1), 0);
      if (instr->InputAt(0)->IsFPRegister()) {
        __ Cvttss2uiq(i.OutputRegister(), i.InputDoubleRegister(0), &fail);
      } else {
        __ Cvttss2uiq(i.OutputRegister(), i.InputOperand(0), &fail);
      }
      if (instr->OutputCount() > 1) __ Set(i.OutputRegister(1), 1);
      __ bind(&fail);
      break;
    }
    case kSSEFloat64ToUint64: {
      Label fail;
      if (instr->OutputCount() > 1) __ Set(i.OutputRegister(1), 0);
      if (instr->InputAt(0)->IsFPRegister()) {
        __ Cvttsd2uiq(i.OutputRegister(), i.InputDoubleRegister(0), &fail);
      } else {
        __ Cvttsd2uiq(i.OutputRegister(), i.InputOperand(0), &fail);
      }
      if (instr->OutputCount() > 1) __ Set(i.OutputRegister(1), 1);
      __ bind(&fail);
      break;
    }
    case kSSEInt32ToFloat64:
      if (HasRegisterInput(instr, 0)) {
        __ Cvtlsi2sd(i.OutputDoubleRegister(), i.InputRegister(0));
      } else {
        __ Cvtlsi2sd(i.OutputDoubleRegister(), i.InputOperand(0));
      }
      break;
    case kSSEInt32ToFloat32:
      if (HasRegisterInput(instr, 0)) {
        __ Cvtlsi2ss(i.OutputDoubleRegister(), i.InputRegister(0));
      } else {
        __ Cvtlsi2ss(i.OutputDoubleRegister(), i.InputOperand(0));
      }
      break;
    case kSSEInt64ToFloat32:
      if (HasRegisterInput(instr, 0)) {
        __ Cvtqsi2ss(i.OutputDoubleRegister(), i.InputRegister(0));
      } else {
        __ Cvtqsi2ss(i.OutputDoubleRegister(), i.InputOperand(0));
      }
      break;
    case kSSEInt64ToFloat64:
      if (HasRegisterInput(instr, 0)) {
        __ Cvtqsi2sd(i.OutputDoubleRegister(), i.InputRegister(0));
      } else {
        __ Cvtqsi2sd(i.OutputDoubleRegister(), i.InputOperand(0));
      }
      break;
    case kSSEUint64ToFloat32:
      if (HasRegisterInput(instr, 0)) {
        __ Cvtqui2ss(i.OutputDoubleRegister(), i.InputRegister(0));
      } else {
        __ Cvtqui2ss(i.OutputDoubleRegister(), i.InputOperand(0));
      }
      break;
    case kSSEUint64ToFloat64:
      if (HasRegisterInput(instr, 0)) {
        __ Cvtqui2sd(i.OutputDoubleRegister(), i.InputRegister(0));
      } else {
        __ Cvtqui2sd(i.OutputDoubleRegister(), i.InputOperand(0));
      }
      break;
    case kSSEUint32ToFloat64:
      if (HasRegisterInput(instr, 0)) {
        __ Cvtlui2sd(i.OutputDoubleRegister(), i.InputRegister(0));
      } else {
        __ Cvtlui2sd(i.OutputDoubleRegister(), i.InputOperand(0));
      }
      break;
    case kSSEUint32ToFloat32:
      if (HasRegisterInput(instr, 0)) {
        __ Cvtlui2ss(i.OutputDoubleRegister(), i.InputRegister(0));
      } else {
        __ Cvtlui2ss(i.OutputDoubleRegister(), i.InputOperand(0));
      }
      break;
    case kSSEFloat64ExtractLowWord32:
      if (instr->InputAt(0)->IsFPStackSlot()) {
        __ movl(i.OutputRegister(), i.InputOperand(0));
      } else {
        __ Movd(i.OutputRegister(), i.InputDoubleRegister(0));
      }
      break;
    case kSSEFloat64ExtractHighWord32:
      if (instr->InputAt(0)->IsFPStackSlot()) {
        __ movl(i.OutputRegister(), i.InputOperand(0, kDoubleSize / 2));
      } else {
        __ Pextrd(i.OutputRegister(), i.InputDoubleRegister(0), 1);
      }
      break;
    case kSSEFloat64InsertLowWord32:
      if (HasRegisterInput(instr, 1)) {
        __ Pinsrd(i.OutputDoubleRegister(), i.InputRegister(1), 0);
      } else {
        __ Pinsrd(i.OutputDoubleRegister(), i.InputOperand(1), 0);
      }
      break;
    case kSSEFloat64InsertHighWord32:
      if (HasRegisterInput(instr, 1)) {
        __ Pinsrd(i.OutputDoubleRegister(), i.InputRegister(1), 1);
      } else {
        __ Pinsrd(i.OutputDoubleRegister(), i.InputOperand(1), 1);
      }
      break;
    case kSSEFloat64LoadLowWord32:
      if (HasRegisterInput(instr, 0)) {
        __ Movd(i.OutputDoubleRegister(), i.InputRegister(0));
      } else {
        __ Movd(i.OutputDoubleRegister(), i.InputOperand(0));
      }
      break;
    case kAVXFloat32Cmp: {
      CpuFeatureScope avx_scope(tasm(), AVX);
      if (instr->InputAt(1)->IsFPRegister()) {
        __ vucomiss(i.InputDoubleRegister(0), i.InputDoubleRegister(1));
      } else {
        __ vucomiss(i.InputDoubleRegister(0), i.InputOperand(1));
      }
      break;
    }
    case kAVXFloat32Add:
      ASSEMBLE_AVX_BINOP(vaddss);
      break;
    case kAVXFloat32Sub:
      ASSEMBLE_AVX_BINOP(vsubss);
      break;
    case kAVXFloat32Mul:
      ASSEMBLE_AVX_BINOP(vmulss);
      break;
    case kAVXFloat32Div:
      ASSEMBLE_AVX_BINOP(vdivss);
      // Don't delete this mov. It may improve performance on some CPUs,
      // when there is a (v)mulss depending on the result.
      __ Movaps(i.OutputDoubleRegister(), i.OutputDoubleRegister());
      break;
    case kAVXFloat64Cmp: {
      CpuFeatureScope avx_scope(tasm(), AVX);
      if (instr->InputAt(1)->IsFPRegister()) {
        __ vucomisd(i.InputDoubleRegister(0), i.InputDoubleRegister(1));
      } else {
        __ vucomisd(i.InputDoubleRegister(0), i.InputOperand(1));
      }
      break;
    }
    case kAVXFloat64Add:
      ASSEMBLE_AVX_BINOP(vaddsd);
      break;
    case kAVXFloat64Sub:
      ASSEMBLE_AVX_BINOP(vsubsd);
      break;
    case kAVXFloat64Mul:
      ASSEMBLE_AVX_BINOP(vmulsd);
      break;
    case kAVXFloat64Div:
      ASSEMBLE_AVX_BINOP(vdivsd);
      // Don't delete this mov. It may improve performance on some CPUs,
      // when there is a (v)mulsd depending on the result.
      __ Movapd(i.OutputDoubleRegister(), i.OutputDoubleRegister());
      break;
    case kAVXFloat32Abs: {
      // TODO(bmeurer): Use RIP relative 128-bit constants.
      CpuFeatureScope avx_scope(tasm(), AVX);
      XMMRegister tmp = i.ToDoubleRegister(instr->TempAt(0));
      __ vpcmpeqd(tmp, tmp, tmp);
      __ vpsrlq(tmp, tmp, 33);
      if (instr->InputAt(0)->IsFPRegister()) {
        __ vandps(i.OutputDoubleRegister(), tmp, i.InputDoubleRegister(0));
      } else {
        __ vandps(i.OutputDoubleRegister(), tmp, i.InputOperand(0));
      }
      break;
    }
    case kAVXFloat32Neg: {
      // TODO(bmeurer): Use RIP relative 128-bit constants.
      CpuFeatureScope avx_scope(tasm(), AVX);
      XMMRegister tmp = i.ToDoubleRegister(instr->TempAt(0));
      __ vpcmpeqd(tmp, tmp, tmp);
      __ vpsllq(tmp, tmp, 31);
      if (instr->InputAt(0)->IsFPRegister()) {
        __ vxorps(i.OutputDoubleRegister(), tmp, i.InputDoubleRegister(0));
      } else {
        __ vxorps(i.OutputDoubleRegister(), tmp, i.InputOperand(0));
      }
      break;
    }
    case kAVXFloat64Abs: {
      // TODO(bmeurer): Use RIP relative 128-bit constants.
      CpuFeatureScope avx_scope(tasm(), AVX);
      XMMRegister tmp = i.ToDoubleRegister(instr->TempAt(0));
      __ vpcmpeqd(tmp, tmp, tmp);
      __ vpsrlq(tmp, tmp, 1);
      if (instr->InputAt(0)->IsFPRegister()) {
        __ vandpd(i.OutputDoubleRegister(), tmp, i.InputDoubleRegister(0));
      } else {
        __ vandpd(i.OutputDoubleRegister(), tmp, i.InputOperand(0));
      }
      break;
    }
    case kAVXFloat64Neg: {
      // TODO(bmeurer): Use RIP relative 128-bit constants.
      CpuFeatureScope avx_scope(tasm(), AVX);
      XMMRegister tmp = i.ToDoubleRegister(instr->TempAt(0));
      __ vpcmpeqd(tmp, tmp, tmp);
      __ vpsllq(tmp, tmp, 63);
      if (instr->InputAt(0)->IsFPRegister()) {
        __ vxorpd(i.OutputDoubleRegister(), tmp, i.InputDoubleRegister(0));
      } else {
        __ vxorpd(i.OutputDoubleRegister(), tmp, i.InputOperand(0));
      }
      break;
    }
    case kSSEFloat64SilenceNaN:
      __ Xorpd(kScratchDoubleReg, kScratchDoubleReg);
      __ Subsd(i.InputDoubleRegister(0), kScratchDoubleReg);
      break;
    case kX64Movsxbl:
      EmitOOLTrapIfNeeded(zone(), this, opcode, instr, __ pc_offset());
      ASSEMBLE_MOVX(movsxbl);
      __ AssertZeroExtended(i.OutputRegister());
      EmitWordLoadPoisoningIfNeeded(this, opcode, instr, i);
      break;
    case kX64Movzxbl:
      EmitOOLTrapIfNeeded(zone(), this, opcode, instr, __ pc_offset());
      ASSEMBLE_MOVX(movzxbl);
      __ AssertZeroExtended(i.OutputRegister());
      EmitWordLoadPoisoningIfNeeded(this, opcode, instr, i);
      break;
    case kX64Movsxbq:
      EmitOOLTrapIfNeeded(zone(), this, opcode, instr, __ pc_offset());
      ASSEMBLE_MOVX(movsxbq);
      EmitWordLoadPoisoningIfNeeded(this, opcode, instr, i);
      break;
    case kX64Movzxbq:
      EmitOOLTrapIfNeeded(zone(), this, opcode, instr, __ pc_offset());
      ASSEMBLE_MOVX(movzxbq);
      __ AssertZeroExtended(i.OutputRegister());
      EmitWordLoadPoisoningIfNeeded(this, opcode, instr, i);
      break;
    case kX64Movb: {
      EmitOOLTrapIfNeeded(zone(), this, opcode, instr, __ pc_offset());
      size_t index = 0;
      Operand operand = i.MemoryOperand(&index);
      if (HasImmediateInput(instr, index)) {
        __ movb(operand, Immediate(i.InputInt8(index)));
      } else {
        __ movb(operand, i.InputRegister(index));
      }
      EmitWordLoadPoisoningIfNeeded(this, opcode, instr, i);
      break;
    }
    case kX64Movsxwl:
      EmitOOLTrapIfNeeded(zone(), this, opcode, instr, __ pc_offset());
      ASSEMBLE_MOVX(movsxwl);
      __ AssertZeroExtended(i.OutputRegister());
      EmitWordLoadPoisoningIfNeeded(this, opcode, instr, i);
      break;
    case kX64Movzxwl:
      EmitOOLTrapIfNeeded(zone(), this, opcode, instr, __ pc_offset());
      ASSEMBLE_MOVX(movzxwl);
      __ AssertZeroExtended(i.OutputRegister());
      EmitWordLoadPoisoningIfNeeded(this, opcode, instr, i);
      break;
    case kX64Movsxwq:
      EmitOOLTrapIfNeeded(zone(), this, opcode, instr, __ pc_offset());
      ASSEMBLE_MOVX(movsxwq);
      break;
    case kX64Movzxwq:
      EmitOOLTrapIfNeeded(zone(), this, opcode, instr, __ pc_offset());
      ASSEMBLE_MOVX(movzxwq);
      __ AssertZeroExtended(i.OutputRegister());
      EmitWordLoadPoisoningIfNeeded(this, opcode, instr, i);
      break;
    case kX64Movw: {
      EmitOOLTrapIfNeeded(zone(), this, opcode, instr, __ pc_offset());
      size_t index = 0;
      Operand operand = i.MemoryOperand(&index);
      if (HasImmediateInput(instr, index)) {
        __ movw(operand, Immediate(i.InputInt16(index)));
      } else {
        __ movw(operand, i.InputRegister(index));
      }
      EmitWordLoadPoisoningIfNeeded(this, opcode, instr, i);
      break;
    }
    case kX64Movl:
      EmitOOLTrapIfNeeded(zone(), this, opcode, instr, __ pc_offset());
      if (instr->HasOutput()) {
        if (HasAddressingMode(instr)) {
          __ movl(i.OutputRegister(), i.MemoryOperand());
        } else {
          if (HasRegisterInput(instr, 0)) {
            __ movl(i.OutputRegister(), i.InputRegister(0));
          } else {
            __ movl(i.OutputRegister(), i.InputOperand(0));
          }
        }
        __ AssertZeroExtended(i.OutputRegister());
      } else {
        size_t index = 0;
        Operand operand = i.MemoryOperand(&index);
        if (HasImmediateInput(instr, index)) {
          __ movl(operand, i.InputImmediate(index));
        } else {
          __ movl(operand, i.InputRegister(index));
        }
      }
      EmitWordLoadPoisoningIfNeeded(this, opcode, instr, i);
      break;
    case kX64Movsxlq:
      EmitOOLTrapIfNeeded(zone(), this, opcode, instr, __ pc_offset());
      ASSEMBLE_MOVX(movsxlq);
      EmitWordLoadPoisoningIfNeeded(this, opcode, instr, i);
      break;
    case kX64MovqDecompressTaggedSigned: {
      CHECK(instr->HasOutput());
      __ DecompressTaggedSigned(i.OutputRegister(), i.MemoryOperand());
      EmitWordLoadPoisoningIfNeeded(this, opcode, instr, i);
      break;
    }
    case kX64MovqDecompressTaggedPointer: {
      CHECK(instr->HasOutput());
      __ DecompressTaggedPointer(i.OutputRegister(), i.MemoryOperand());
      EmitWordLoadPoisoningIfNeeded(this, opcode, instr, i);
      break;
    }
    case kX64MovqDecompressAnyTagged: {
      CHECK(instr->HasOutput());
      __ DecompressAnyTagged(i.OutputRegister(), i.MemoryOperand());
      EmitWordLoadPoisoningIfNeeded(this, opcode, instr, i);
      break;
    }
    case kX64MovqCompressTagged: {
      CHECK(!instr->HasOutput());
      size_t index = 0;
      Operand operand = i.MemoryOperand(&index);
      if (HasImmediateInput(instr, index)) {
        __ StoreTaggedField(operand, i.InputImmediate(index));
      } else {
        __ StoreTaggedField(operand, i.InputRegister(index));
      }
      break;
    }
    case kX64Movq:
      EmitOOLTrapIfNeeded(zone(), this, opcode, instr, __ pc_offset());
      if (instr->HasOutput()) {
        __ movq(i.OutputRegister(), i.MemoryOperand());
      } else {
        size_t index = 0;
        Operand operand = i.MemoryOperand(&index);
        if (HasImmediateInput(instr, index)) {
          __ movq(operand, i.InputImmediate(index));
        } else {
          __ movq(operand, i.InputRegister(index));
        }
      }
      EmitWordLoadPoisoningIfNeeded(this, opcode, instr, i);
      break;
    case kX64Movss:
      EmitOOLTrapIfNeeded(zone(), this, opcode, instr, __ pc_offset());
      if (instr->HasOutput()) {
        __ Movss(i.OutputDoubleRegister(), i.MemoryOperand());
      } else {
        size_t index = 0;
        Operand operand = i.MemoryOperand(&index);
        __ Movss(operand, i.InputDoubleRegister(index));
      }
      break;
    case kX64Movsd: {
      EmitOOLTrapIfNeeded(zone(), this, opcode, instr, __ pc_offset());
      if (instr->HasOutput()) {
        const MemoryAccessMode access_mode = AccessModeField::decode(opcode);
        if (access_mode == kMemoryAccessPoisoned) {
          // If we have to poison the loaded value, we load into a general
          // purpose register first, mask it with the poison, and move the
          // value from the general purpose register into the double register.
          __ movq(kScratchRegister, i.MemoryOperand());
          __ andq(kScratchRegister, kSpeculationPoisonRegister);
          __ Movq(i.OutputDoubleRegister(), kScratchRegister);
        } else {
          __ Movsd(i.OutputDoubleRegister(), i.MemoryOperand());
        }
      } else {
        size_t index = 0;
        Operand operand = i.MemoryOperand(&index);
        __ Movsd(operand, i.InputDoubleRegister(index));
      }
      break;
    }
    case kX64Movdqu: {
      EmitOOLTrapIfNeeded(zone(), this, opcode, instr, __ pc_offset());
      if (instr->HasOutput()) {
        __ Movdqu(i.OutputSimd128Register(), i.MemoryOperand());
      } else {
        size_t index = 0;
        Operand operand = i.MemoryOperand(&index);
        __ Movdqu(operand, i.InputSimd128Register(index));
      }
      break;
    }
    case kX64BitcastFI:
      if (instr->InputAt(0)->IsFPStackSlot()) {
        __ movl(i.OutputRegister(), i.InputOperand(0));
      } else {
        __ Movd(i.OutputRegister(), i.InputDoubleRegister(0));
      }
      break;
    case kX64BitcastDL:
      if (instr->InputAt(0)->IsFPStackSlot()) {
        __ movq(i.OutputRegister(), i.InputOperand(0));
      } else {
        __ Movq(i.OutputRegister(), i.InputDoubleRegister(0));
      }
      break;
    case kX64BitcastIF:
      if (HasRegisterInput(instr, 0)) {
        __ Movd(i.OutputDoubleRegister(), i.InputRegister(0));
      } else {
        __ Movss(i.OutputDoubleRegister(), i.InputOperand(0));
      }
      break;
    case kX64BitcastLD:
      if (HasRegisterInput(instr, 0)) {
        __ Movq(i.OutputDoubleRegister(), i.InputRegister(0));
      } else {
        __ Movsd(i.OutputDoubleRegister(), i.InputOperand(0));
      }
      break;
    case kX64Lea32: {
      AddressingMode mode = AddressingModeField::decode(instr->opcode());
      // Shorten "leal" to "addl", "subl" or "shll" if the register allocation
      // and addressing mode just happens to work out. The "addl"/"subl" forms
      // in these cases are faster based on measurements.
      if (i.InputRegister(0) == i.OutputRegister()) {
        if (mode == kMode_MRI) {
          int32_t constant_summand = i.InputInt32(1);
          DCHECK_NE(0, constant_summand);
          if (constant_summand > 0) {
            __ addl(i.OutputRegister(), Immediate(constant_summand));
          } else {
            __ subl(i.OutputRegister(),
                    Immediate(base::NegateWithWraparound(constant_summand)));
          }
        } else if (mode == kMode_MR1) {
          if (i.InputRegister(1) == i.OutputRegister()) {
            __ shll(i.OutputRegister(), Immediate(1));
          } else {
            __ addl(i.OutputRegister(), i.InputRegister(1));
          }
        } else if (mode == kMode_M2) {
          __ shll(i.OutputRegister(), Immediate(1));
        } else if (mode == kMode_M4) {
          __ shll(i.OutputRegister(), Immediate(2));
        } else if (mode == kMode_M8) {
          __ shll(i.OutputRegister(), Immediate(3));
        } else {
          __ leal(i.OutputRegister(), i.MemoryOperand());
        }
      } else if (mode == kMode_MR1 &&
                 i.InputRegister(1) == i.OutputRegister()) {
        __ addl(i.OutputRegister(), i.InputRegister(0));
      } else {
        __ leal(i.OutputRegister(), i.MemoryOperand());
      }
      __ AssertZeroExtended(i.OutputRegister());
      break;
    }
    case kX64Lea: {
      AddressingMode mode = AddressingModeField::decode(instr->opcode());
      // Shorten "leaq" to "addq", "subq" or "shlq" if the register allocation
      // and addressing mode just happens to work out. The "addq"/"subq" forms
      // in these cases are faster based on measurements.
      if (i.InputRegister(0) == i.OutputRegister()) {
        if (mode == kMode_MRI) {
          int32_t constant_summand = i.InputInt32(1);
          if (constant_summand > 0) {
            __ addq(i.OutputRegister(), Immediate(constant_summand));
          } else if (constant_summand < 0) {
            __ subq(i.OutputRegister(), Immediate(-constant_summand));
          }
        } else if (mode == kMode_MR1) {
          if (i.InputRegister(1) == i.OutputRegister()) {
            __ shlq(i.OutputRegister(), Immediate(1));
          } else {
            __ addq(i.OutputRegister(), i.InputRegister(1));
          }
        } else if (mode == kMode_M2) {
          __ shlq(i.OutputRegister(), Immediate(1));
        } else if (mode == kMode_M4) {
          __ shlq(i.OutputRegister(), Immediate(2));
        } else if (mode == kMode_M8) {
          __ shlq(i.OutputRegister(), Immediate(3));
        } else {
          __ leaq(i.OutputRegister(), i.MemoryOperand());
        }
      } else if (mode == kMode_MR1 &&
                 i.InputRegister(1) == i.OutputRegister()) {
        __ addq(i.OutputRegister(), i.InputRegister(0));
      } else {
        __ leaq(i.OutputRegister(), i.MemoryOperand());
      }
      break;
    }
    case kX64Dec32:
      __ decl(i.OutputRegister());
      break;
    case kX64Inc32:
      __ incl(i.OutputRegister());
      break;
    case kX64Push: {
      int stack_decrement = i.InputInt32(0);
      int slots = stack_decrement / kSystemPointerSize;
      // Whenever codegen uses pushq, we need to check if stack_decrement
      // contains any extra padding and adjust the stack before the pushq.
      if (HasImmediateInput(instr, 1)) {
        __ AllocateStackSpace(stack_decrement - kSystemPointerSize);
        __ pushq(i.InputImmediate(1));
      } else if (HasAddressingMode(instr)) {
        __ AllocateStackSpace(stack_decrement - kSystemPointerSize);
        size_t index = 1;
        Operand operand = i.MemoryOperand(&index);
        __ pushq(operand);
      } else {
        InstructionOperand* input = instr->InputAt(1);
        if (input->IsRegister()) {
          __ AllocateStackSpace(stack_decrement - kSystemPointerSize);
          __ pushq(i.InputRegister(1));
        } else if (input->IsFloatRegister() || input->IsDoubleRegister()) {
          DCHECK_GE(stack_decrement, kSystemPointerSize);
          __ AllocateStackSpace(stack_decrement);
          __ Movsd(Operand(rsp, 0), i.InputDoubleRegister(1));
        } else if (input->IsSimd128Register()) {
          DCHECK_GE(stack_decrement, kSimd128Size);
          __ AllocateStackSpace(stack_decrement);
          // TODO(bbudge) Use Movaps when slots are aligned.
          __ Movups(Operand(rsp, 0), i.InputSimd128Register(1));
        } else if (input->IsStackSlot() || input->IsFloatStackSlot() ||
                   input->IsDoubleStackSlot()) {
          __ AllocateStackSpace(stack_decrement - kSystemPointerSize);
          __ pushq(i.InputOperand(1));
        } else {
          DCHECK(input->IsSimd128StackSlot());
          DCHECK_GE(stack_decrement, kSimd128Size);
          // TODO(bbudge) Use Movaps when slots are aligned.
          __ Movups(kScratchDoubleReg, i.InputOperand(1));
          __ AllocateStackSpace(stack_decrement);
          __ Movups(Operand(rsp, 0), kScratchDoubleReg);
        }
      }
      frame_access_state()->IncreaseSPDelta(slots);
      unwinding_info_writer_.MaybeIncreaseBaseOffsetAt(__ pc_offset(),
                                                       stack_decrement);
      break;
    }
    case kX64Poke: {
      int slot = MiscField::decode(instr->opcode());
      if (HasImmediateInput(instr, 0)) {
        __ movq(Operand(rsp, slot * kSystemPointerSize), i.InputImmediate(0));
      } else if (instr->InputAt(0)->IsFPRegister()) {
        LocationOperand* op = LocationOperand::cast(instr->InputAt(0));
        if (op->representation() == MachineRepresentation::kFloat64) {
          __ Movsd(Operand(rsp, slot * kSystemPointerSize),
                   i.InputDoubleRegister(0));
        } else {
          DCHECK_EQ(MachineRepresentation::kFloat32, op->representation());
          __ Movss(Operand(rsp, slot * kSystemPointerSize),
                   i.InputFloatRegister(0));
        }
      } else {
        __ movq(Operand(rsp, slot * kSystemPointerSize), i.InputRegister(0));
      }
      break;
    }
    case kX64Peek: {
      int reverse_slot = i.InputInt32(0);
      int offset =
          FrameSlotToFPOffset(frame()->GetTotalFrameSlotCount() - reverse_slot);
      if (instr->OutputAt(0)->IsFPRegister()) {
        LocationOperand* op = LocationOperand::cast(instr->OutputAt(0));
        if (op->representation() == MachineRepresentation::kFloat64) {
          __ Movsd(i.OutputDoubleRegister(), Operand(rbp, offset));
        } else if (op->representation() == MachineRepresentation::kFloat32) {
          __ Movss(i.OutputFloatRegister(), Operand(rbp, offset));
        } else {
          DCHECK_EQ(MachineRepresentation::kSimd128, op->representation());
          __ Movdqu(i.OutputSimd128Register(), Operand(rbp, offset));
        }
      } else {
        __ movq(i.OutputRegister(), Operand(rbp, offset));
      }
      break;
    }
    case kX64F64x2Splat: {
      XMMRegister dst = i.OutputSimd128Register();
      if (instr->InputAt(0)->IsFPRegister()) {
        __ Movddup(dst, i.InputDoubleRegister(0));
      } else {
        __ Movddup(dst, i.InputOperand(0));
      }
      break;
    }
    case kX64F64x2ExtractLane: {
      DoubleRegister dst = i.OutputDoubleRegister();
      XMMRegister src = i.InputSimd128Register(0);
      uint8_t lane = i.InputUint8(1);
      if (lane == 0) {
        __ Move(dst, src);
      } else {
        DCHECK_EQ(1, lane);
        if (CpuFeatures::IsSupported(AVX)) {
          CpuFeatureScope avx_scope(tasm(), AVX);
          // Pass src as operand to avoid false-dependency on dst.
          __ vmovhlps(dst, src, src);
        } else {
          __ movhlps(dst, src);
        }
      }
      break;
    }
    case kX64F64x2Sqrt: {
      __ Sqrtpd(i.OutputSimd128Register(), i.InputSimd128Register(0));
      break;
    }
    case kX64F64x2Add: {
      ASSEMBLE_SIMD_BINOP(addpd);
      break;
    }
    case kX64F64x2Sub: {
      ASSEMBLE_SIMD_BINOP(subpd);
      break;
    }
    case kX64F64x2Mul: {
      ASSEMBLE_SIMD_BINOP(mulpd);
      break;
    }
    case kX64F64x2Div: {
      ASSEMBLE_SIMD_BINOP(divpd);
      break;
    }
    case kX64F64x2Min: {
      XMMRegister src1 = i.InputSimd128Register(1),
                  dst = i.OutputSimd128Register();
      DCHECK_EQ(dst, i.InputSimd128Register(0));
      // The minpd instruction doesn't propagate NaNs and +0's in its first
      // operand. Perform minpd in both orders, merge the resuls, and adjust.
      __ Movapd(kScratchDoubleReg, src1);
      __ Minpd(kScratchDoubleReg, dst);
      __ Minpd(dst, src1);
      // propagate -0's and NaNs, which may be non-canonical.
      __ Orpd(kScratchDoubleReg, dst);
      // Canonicalize NaNs by quieting and clearing the payload.
      __ Cmppd(dst, kScratchDoubleReg, int8_t{3});
      __ Orpd(kScratchDoubleReg, dst);
      __ Psrlq(dst, 13);
      __ Andnpd(dst, kScratchDoubleReg);
      break;
    }
    case kX64F64x2Max: {
      XMMRegister src1 = i.InputSimd128Register(1),
                  dst = i.OutputSimd128Register();
      DCHECK_EQ(dst, i.InputSimd128Register(0));
      // The maxpd instruction doesn't propagate NaNs and +0's in its first
      // operand. Perform maxpd in both orders, merge the resuls, and adjust.
      __ Movapd(kScratchDoubleReg, src1);
      __ Maxpd(kScratchDoubleReg, dst);
      __ Maxpd(dst, src1);
      // Find discrepancies.
      __ Xorpd(dst, kScratchDoubleReg);
      // Propagate NaNs, which may be non-canonical.
      __ Orpd(kScratchDoubleReg, dst);
      // Propagate sign discrepancy and (subtle) quiet NaNs.
      __ Subpd(kScratchDoubleReg, dst);
      // Canonicalize NaNs by clearing the payload. Sign is non-deterministic.
      __ Cmppd(dst, kScratchDoubleReg, int8_t{3});
      __ Psrlq(dst, 13);
      __ Andnpd(dst, kScratchDoubleReg);
      break;
    }
    case kX64F64x2Eq: {
      ASSEMBLE_SIMD_BINOP(cmpeqpd);
      break;
    }
    case kX64F64x2Ne: {
      ASSEMBLE_SIMD_BINOP(cmpneqpd);
      break;
    }
    case kX64F64x2Lt: {
      ASSEMBLE_SIMD_BINOP(cmpltpd);
      break;
    }
    case kX64F64x2Le: {
      ASSEMBLE_SIMD_BINOP(cmplepd);
      break;
    }
    case kX64F64x2Qfma: {
      if (CpuFeatures::IsSupported(FMA3)) {
        CpuFeatureScope fma3_scope(tasm(), FMA3);
        __ vfmadd231pd(i.OutputSimd128Register(), i.InputSimd128Register(1),
                       i.InputSimd128Register(2));
      } else {
        __ Movapd(kScratchDoubleReg, i.InputSimd128Register(2));
        __ Mulpd(kScratchDoubleReg, i.InputSimd128Register(1));
        __ Addpd(i.OutputSimd128Register(), kScratchDoubleReg);
      }
      break;
    }
    case kX64F64x2Qfms: {
      if (CpuFeatures::IsSupported(FMA3)) {
        CpuFeatureScope fma3_scope(tasm(), FMA3);
        __ vfnmadd231pd(i.OutputSimd128Register(), i.InputSimd128Register(1),
                        i.InputSimd128Register(2));
      } else {
        __ Movapd(kScratchDoubleReg, i.InputSimd128Register(2));
        __ Mulpd(kScratchDoubleReg, i.InputSimd128Register(1));
        __ Subpd(i.OutputSimd128Register(), kScratchDoubleReg);
      }
      break;
    }
    case kX64F64x2ConvertLowI32x4S: {
      __ Cvtdq2pd(i.OutputSimd128Register(), i.InputSimd128Register(0));
      break;
    }
    case kX64F64x2ConvertLowI32x4U: {
      __ F64x2ConvertLowI32x4U(i.OutputSimd128Register(),
                               i.InputSimd128Register(0));
      break;
    }
    case kX64F64x2PromoteLowF32x4: {
      __ Cvtps2pd(i.OutputSimd128Register(), i.InputSimd128Register(0));
      break;
    }
    case kX64F32x4DemoteF64x2Zero: {
      __ Cvtpd2ps(i.OutputSimd128Register(), i.InputSimd128Register(0));
      break;
    }
    case kX64I32x4TruncSatF64x2SZero: {
      __ I32x4TruncSatF64x2SZero(i.OutputSimd128Register(),
                                 i.InputSimd128Register(0));
      break;
    }
    case kX64I32x4TruncSatF64x2UZero: {
      __ I32x4TruncSatF64x2UZero(i.OutputSimd128Register(),
                                 i.InputSimd128Register(0));
      break;
    }
    case kX64F32x4Splat: {
      XMMRegister dst = i.OutputSimd128Register();
      XMMRegister src = i.InputDoubleRegister(0);
      if (CpuFeatures::IsSupported(AVX2)) {
        CpuFeatureScope avx2_scope(tasm(), AVX2);
        __ vbroadcastss(dst, src);
      } else if (CpuFeatures::IsSupported(AVX)) {
        CpuFeatureScope avx_scope(tasm(), AVX);
        __ vshufps(dst, src, src, 0);
      } else {
        if (dst == src) {
          // 1 byte shorter than pshufd.
          __ shufps(dst, src, 0);
        } else {
          __ pshufd(dst, src, 0);
        }
      }
      break;
    }
    case kX64F32x4ExtractLane: {
      XMMRegister dst = i.OutputDoubleRegister();
      XMMRegister src = i.InputSimd128Register(0);
      uint8_t lane = i.InputUint8(1);
      DCHECK_LT(lane, 4);
      // These instructions are shorter than insertps, but will leave junk in
      // the top lanes of dst.
      if (lane == 0) {
        __ Move(dst, src);
      } else if (lane == 1) {
        __ Movshdup(dst, src);
      } else if (lane == 2 && dst == src) {
        // Check dst == src to avoid false dependency on dst.
        __ Movhlps(dst, src);
      } else if (dst == src) {
        __ Shufps(dst, src, src, lane);
      } else {
        __ Pshufd(dst, src, lane);
      }
      break;
    }
    case kX64F32x4ReplaceLane: {
      // The insertps instruction uses imm8[5:4] to indicate the lane
      // that needs to be replaced.
      byte select = i.InputInt8(1) << 4 & 0x30;
      if (instr->InputAt(2)->IsFPRegister()) {
        __ Insertps(i.OutputSimd128Register(), i.InputDoubleRegister(2),
                    select);
      } else {
        __ Insertps(i.OutputSimd128Register(), i.InputOperand(2), select);
      }
      break;
    }
    case kX64F32x4SConvertI32x4: {
      __ Cvtdq2ps(i.OutputSimd128Register(), i.InputSimd128Register(0));
      break;
    }
    case kX64F32x4UConvertI32x4: {
      DCHECK_EQ(i.OutputSimd128Register(), i.InputSimd128Register(0));
      DCHECK_NE(i.OutputSimd128Register(), kScratchDoubleReg);
      XMMRegister dst = i.OutputSimd128Register();
      __ Pxor(kScratchDoubleReg, kScratchDoubleReg);  // zeros
      __ Pblendw(kScratchDoubleReg, dst, uint8_t{0x55});  // get lo 16 bits
      __ Psubd(dst, kScratchDoubleReg);                   // get hi 16 bits
      __ Cvtdq2ps(kScratchDoubleReg, kScratchDoubleReg);  // convert lo exactly
      __ Psrld(dst, byte{1});            // divide by 2 to get in unsigned range
      __ Cvtdq2ps(dst, dst);             // convert hi exactly
      __ Addps(dst, dst);                // double hi, exactly
      __ Addps(dst, kScratchDoubleReg);  // add hi and lo, may round.
      break;
    }
    case kX64F32x4Abs: {
      XMMRegister dst = i.OutputSimd128Register();
      XMMRegister src = i.InputSimd128Register(0);
      if (dst == src) {
        __ Pcmpeqd(kScratchDoubleReg, kScratchDoubleReg);
        __ Psrld(kScratchDoubleReg, byte{1});
        __ Andps(dst, kScratchDoubleReg);
      } else {
        __ Pcmpeqd(dst, dst);
        __ Psrld(dst, byte{1});
        __ Andps(dst, src);
      }
      break;
    }
    case kX64F32x4Neg: {
      XMMRegister dst = i.OutputSimd128Register();
      XMMRegister src = i.InputSimd128Register(0);
      if (dst == src) {
        __ Pcmpeqd(kScratchDoubleReg, kScratchDoubleReg);
        __ Pslld(kScratchDoubleReg, byte{31});
        __ Xorps(dst, kScratchDoubleReg);
      } else {
        __ Pcmpeqd(dst, dst);
        __ Pslld(dst, byte{31});
        __ Xorps(dst, src);
      }
      break;
    }
    case kX64F32x4Sqrt: {
      __ Sqrtps(i.OutputSimd128Register(), i.InputSimd128Register(0));
      break;
    }
    case kX64F32x4RecipApprox: {
      __ Rcpps(i.OutputSimd128Register(), i.InputSimd128Register(0));
      break;
    }
    case kX64F32x4RecipSqrtApprox: {
      __ Rsqrtps(i.OutputSimd128Register(), i.InputSimd128Register(0));
      break;
    }
    case kX64F32x4Add: {
      ASSEMBLE_SIMD_BINOP(addps);
      break;
    }
    case kX64F32x4Sub: {
      ASSEMBLE_SIMD_BINOP(subps);
      break;
    }
    case kX64F32x4Mul: {
      ASSEMBLE_SIMD_BINOP(mulps);
      break;
    }
    case kX64F32x4Div: {
      ASSEMBLE_SIMD_BINOP(divps);
      break;
    }
    case kX64F32x4Min: {
      XMMRegister src1 = i.InputSimd128Register(1),
                  dst = i.OutputSimd128Register();
      DCHECK_EQ(dst, i.InputSimd128Register(0));
      // The minps instruction doesn't propagate NaNs and +0's in its first
      // operand. Perform minps in both orders, merge the resuls, and adjust.
      __ Movaps(kScratchDoubleReg, src1);
      __ Minps(kScratchDoubleReg, dst);
      __ Minps(dst, src1);
      // propagate -0's and NaNs, which may be non-canonical.
      __ Orps(kScratchDoubleReg, dst);
      // Canonicalize NaNs by quieting and clearing the payload.
      __ Cmpps(dst, kScratchDoubleReg, int8_t{3});
      __ Orps(kScratchDoubleReg, dst);
      __ Psrld(dst, byte{10});
      __ Andnps(dst, kScratchDoubleReg);
      break;
    }
    case kX64F32x4Max: {
      XMMRegister src1 = i.InputSimd128Register(1),
                  dst = i.OutputSimd128Register();
      DCHECK_EQ(dst, i.InputSimd128Register(0));
      // The maxps instruction doesn't propagate NaNs and +0's in its first
      // operand. Perform maxps in both orders, merge the resuls, and adjust.
      __ Movaps(kScratchDoubleReg, src1);
      __ Maxps(kScratchDoubleReg, dst);
      __ Maxps(dst, src1);
      // Find discrepancies.
      __ Xorps(dst, kScratchDoubleReg);
      // Propagate NaNs, which may be non-canonical.
      __ Orps(kScratchDoubleReg, dst);
      // Propagate sign discrepancy and (subtle) quiet NaNs.
      __ Subps(kScratchDoubleReg, dst);
      // Canonicalize NaNs by clearing the payload. Sign is non-deterministic.
      __ Cmpps(dst, kScratchDoubleReg, int8_t{3});
      __ Psrld(dst, byte{10});
      __ Andnps(dst, kScratchDoubleReg);
      break;
    }
    case kX64F32x4Eq: {
      ASSEMBLE_SIMD_BINOP(cmpeqps);
      break;
    }
    case kX64F32x4Ne: {
      ASSEMBLE_SIMD_BINOP(cmpneqps);
      break;
    }
    case kX64F32x4Lt: {
      ASSEMBLE_SIMD_BINOP(cmpltps);
      break;
    }
    case kX64F32x4Le: {
      ASSEMBLE_SIMD_BINOP(cmpleps);
      break;
    }
    case kX64F32x4Qfma: {
      if (CpuFeatures::IsSupported(FMA3)) {
        CpuFeatureScope fma3_scope(tasm(), FMA3);
        __ vfmadd231ps(i.OutputSimd128Register(), i.InputSimd128Register(1),
                       i.InputSimd128Register(2));
      } else {
        __ Movaps(kScratchDoubleReg, i.InputSimd128Register(2));
        __ Mulps(kScratchDoubleReg, i.InputSimd128Register(1));
        __ Addps(i.OutputSimd128Register(), kScratchDoubleReg);
      }
      break;
    }
    case kX64F32x4Qfms: {
      if (CpuFeatures::IsSupported(FMA3)) {
        CpuFeatureScope fma3_scope(tasm(), FMA3);
        __ vfnmadd231ps(i.OutputSimd128Register(), i.InputSimd128Register(1),
                        i.InputSimd128Register(2));
      } else {
        __ Movaps(kScratchDoubleReg, i.InputSimd128Register(2));
        __ Mulps(kScratchDoubleReg, i.InputSimd128Register(1));
        __ Subps(i.OutputSimd128Register(), kScratchDoubleReg);
      }
      break;
    }
    case kX64F32x4Pmin: {
      ASSEMBLE_SIMD_BINOP(minps);
      break;
    }
    case kX64F32x4Pmax: {
      ASSEMBLE_SIMD_BINOP(maxps);
      break;
    }
    case kX64F32x4Round: {
      RoundingMode const mode =
          static_cast<RoundingMode>(MiscField::decode(instr->opcode()));
      __ Roundps(i.OutputSimd128Register(), i.InputSimd128Register(0), mode);
      break;
    }
    case kX64F64x2Round: {
      RoundingMode const mode =
          static_cast<RoundingMode>(MiscField::decode(instr->opcode()));
      __ Roundpd(i.OutputSimd128Register(), i.InputSimd128Register(0), mode);
      break;
    }
    case kX64F64x2Pmin: {
      ASSEMBLE_SIMD_BINOP(minpd);
      break;
    }
    case kX64F64x2Pmax: {
      ASSEMBLE_SIMD_BINOP(maxpd);
      break;
    }
    case kX64I64x2Splat: {
      XMMRegister dst = i.OutputSimd128Register();
      if (HasRegisterInput(instr, 0)) {
        __ Movq(dst, i.InputRegister(0));
        __ Movddup(dst, dst);
      } else {
        __ Movddup(dst, i.InputOperand(0));
      }
      break;
    }
    case kX64I64x2ExtractLane: {
      __ Pextrq(i.OutputRegister(), i.InputSimd128Register(0), i.InputInt8(1));
      break;
    }
    case kX64I64x2Abs: {
      __ I64x2Abs(i.OutputSimd128Register(), i.InputSimd128Register(0),
                  kScratchDoubleReg);
      break;
    }
    case kX64I64x2Neg: {
      XMMRegister dst = i.OutputSimd128Register();
      XMMRegister src = i.InputSimd128Register(0);
      if (dst == src) {
        __ Movdqa(kScratchDoubleReg, src);
        src = kScratchDoubleReg;
      }
      __ Pxor(dst, dst);
      __ Psubq(dst, src);
      break;
    }
    case kX64I64x2BitMask: {
      __ Movmskpd(i.OutputRegister(), i.InputSimd128Register(0));
      break;
    }
    case kX64I64x2Shl: {
      // Take shift value modulo 2^6.
      ASSEMBLE_SIMD_SHIFT(psllq, 6);
      break;
    }
    case kX64I64x2ShrS: {
      // TODO(zhin): there is vpsraq but requires AVX512
      // ShrS on each quadword one at a time
      XMMRegister dst = i.OutputSimd128Register();
      XMMRegister src = i.InputSimd128Register(0);
      Register tmp = i.ToRegister(instr->TempAt(0));
      // Modulo 64 not required as sarq_cl will mask cl to 6 bits.

      // lower quadword
      __ Pextrq(tmp, src, int8_t{0x0});
      __ sarq_cl(tmp);
      __ Pinsrq(dst, tmp, uint8_t{0x0});

      // upper quadword
      __ Pextrq(tmp, src, int8_t{0x1});
      __ sarq_cl(tmp);
      __ Pinsrq(dst, tmp, uint8_t{0x1});
      break;
    }
    case kX64I64x2Add: {
      ASSEMBLE_SIMD_BINOP(paddq);
      break;
    }
    case kX64I64x2Sub: {
      ASSEMBLE_SIMD_BINOP(psubq);
      break;
    }
    case kX64I64x2Mul: {
      DCHECK_EQ(i.OutputSimd128Register(), i.InputSimd128Register(0));
      XMMRegister left = i.InputSimd128Register(0);
      XMMRegister right = i.InputSimd128Register(1);
      XMMRegister tmp1 = i.TempSimd128Register(0);
      XMMRegister tmp2 = kScratchDoubleReg;

      __ Movdqa(tmp1, left);
      __ Movdqa(tmp2, right);

      // Multiply high dword of each qword of left with right.
      __ Psrlq(tmp1, 32);
      __ Pmuludq(tmp1, right);

      // Multiply high dword of each qword of right with left.
      __ Psrlq(tmp2, 32);
      __ Pmuludq(tmp2, left);

      __ Paddq(tmp2, tmp1);
      __ Psllq(tmp2, 32);

      __ Pmuludq(left, right);
      __ Paddq(left, tmp2);  // left == dst
      break;
    }
    case kX64I64x2Eq: {
      CpuFeatureScope sse_scope(tasm(), SSE4_1);
      ASSEMBLE_SIMD_BINOP(pcmpeqq);
      break;
    }
    case kX64I64x2Ne: {
      DCHECK_EQ(i.OutputSimd128Register(), i.InputSimd128Register(0));
      __ Pcmpeqq(i.OutputSimd128Register(), i.InputSimd128Register(1));
      __ Pcmpeqq(kScratchDoubleReg, kScratchDoubleReg);
      __ Pxor(i.OutputSimd128Register(), kScratchDoubleReg);
      break;
    }
    case kX64I64x2GtS: {
      __ I64x2GtS(i.OutputSimd128Register(), i.InputSimd128Register(0),
                  i.InputSimd128Register(1), kScratchDoubleReg);
      break;
    }
    case kX64I64x2GeS: {
      __ I64x2GeS(i.OutputSimd128Register(), i.InputSimd128Register(0),
                  i.InputSimd128Register(1), kScratchDoubleReg);
      break;
    }
    case kX64I64x2ShrU: {
      // Take shift value modulo 2^6.
      ASSEMBLE_SIMD_SHIFT(psrlq, 6);
      break;
    }
    case kX64I64x2ExtMulLowI32x4S: {
      __ I64x2ExtMul(i.OutputSimd128Register(), i.InputSimd128Register(0),
                     i.InputSimd128Register(1), kScratchDoubleReg, /*low=*/true,
                     /*is_signed=*/true);
      break;
    }
    case kX64I64x2ExtMulHighI32x4S: {
      __ I64x2ExtMul(i.OutputSimd128Register(), i.InputSimd128Register(0),
                     i.InputSimd128Register(1), kScratchDoubleReg,
                     /*low=*/false,
                     /*is_signed=*/true);
      break;
    }
    case kX64I64x2ExtMulLowI32x4U: {
      __ I64x2ExtMul(i.OutputSimd128Register(), i.InputSimd128Register(0),
                     i.InputSimd128Register(1), kScratchDoubleReg, /*low=*/true,
                     /*is_signed=*/false);
      break;
    }
    case kX64I64x2ExtMulHighI32x4U: {
      __ I64x2ExtMul(i.OutputSimd128Register(), i.InputSimd128Register(0),
                     i.InputSimd128Register(1), kScratchDoubleReg,
                     /*low=*/false,
                     /*is_signed=*/false);
      break;
    }
    case kX64I64x2SConvertI32x4Low: {
      __ Pmovsxdq(i.OutputSimd128Register(), i.InputSimd128Register(0));
      break;
    }
    case kX64I64x2SConvertI32x4High: {
      __ I64x2SConvertI32x4High(i.OutputSimd128Register(),
                                i.InputSimd128Register(0));
      break;
    }
    case kX64I64x2UConvertI32x4Low: {
      __ Pmovzxdq(i.OutputSimd128Register(), i.InputSimd128Register(0));
      break;
    }
    case kX64I64x2UConvertI32x4High: {
      __ I64x2UConvertI32x4High(i.OutputSimd128Register(),
                                i.InputSimd128Register(0), kScratchDoubleReg);
      break;
    }
    case kX64I32x4Splat: {
      XMMRegister dst = i.OutputSimd128Register();
      if (HasRegisterInput(instr, 0)) {
        __ Movd(dst, i.InputRegister(0));
      } else {
        // TODO(v8:9198): Pshufd can load from aligned memory once supported.
        __ Movd(dst, i.InputOperand(0));
      }
      __ Pshufd(dst, dst, uint8_t{0x0});
      break;
    }
    case kX64I32x4ExtractLane: {
      __ Pextrd(i.OutputRegister(), i.InputSimd128Register(0), i.InputInt8(1));
      break;
    }
    case kX64I32x4SConvertF32x4: {
      DCHECK_EQ(i.OutputSimd128Register(), i.InputSimd128Register(0));
      XMMRegister dst = i.OutputSimd128Register();
      // NAN->0
      __ Movaps(kScratchDoubleReg, dst);
      __ Cmpeqps(kScratchDoubleReg, kScratchDoubleReg);
      __ Pand(dst, kScratchDoubleReg);
      // Set top bit if >= 0 (but not -0.0!)
      __ Pxor(kScratchDoubleReg, dst);
      // Convert
      __ Cvttps2dq(dst, dst);
      // Set top bit if >=0 is now < 0
      __ Pand(kScratchDoubleReg, dst);
      __ Psrad(kScratchDoubleReg, byte{31});
      // Set positive overflow lanes to 0x7FFFFFFF
      __ Pxor(dst, kScratchDoubleReg);
      break;
    }
    case kX64I32x4SConvertI16x8Low: {
      __ Pmovsxwd(i.OutputSimd128Register(), i.InputSimd128Register(0));
      break;
    }
    case kX64I32x4SConvertI16x8High: {
      __ I32x4SConvertI16x8High(i.OutputSimd128Register(),
                                i.InputSimd128Register(0));
      break;
    }
    case kX64I32x4Neg: {
      XMMRegister dst = i.OutputSimd128Register();
      XMMRegister src = i.InputSimd128Register(0);
      if (dst == src) {
        __ Pcmpeqd(kScratchDoubleReg, kScratchDoubleReg);
        __ Psignd(dst, kScratchDoubleReg);
      } else {
        __ Pxor(dst, dst);
        __ Psubd(dst, src);
      }
      break;
    }
    case kX64I32x4Shl: {
      // Take shift value modulo 2^5.
      ASSEMBLE_SIMD_SHIFT(pslld, 5);
      break;
    }
    case kX64I32x4ShrS: {
      // Take shift value modulo 2^5.
      ASSEMBLE_SIMD_SHIFT(psrad, 5);
      break;
    }
    case kX64I32x4Add: {
      ASSEMBLE_SIMD_BINOP(paddd);
      break;
    }
    case kX64I32x4Sub: {
      ASSEMBLE_SIMD_BINOP(psubd);
      break;
    }
    case kX64I32x4Mul: {
      ASSEMBLE_SIMD_BINOP(pmulld);
      break;
    }
    case kX64I32x4MinS: {
      ASSEMBLE_SIMD_BINOP(pminsd);
      break;
    }
    case kX64I32x4MaxS: {
      ASSEMBLE_SIMD_BINOP(pmaxsd);
      break;
    }
    case kX64I32x4Eq: {
      ASSEMBLE_SIMD_BINOP(pcmpeqd);
      break;
    }
    case kX64I32x4Ne: {
      __ Pcmpeqd(i.OutputSimd128Register(), i.InputSimd128Register(1));
      __ Pcmpeqd(kScratchDoubleReg, kScratchDoubleReg);
      __ Pxor(i.OutputSimd128Register(), kScratchDoubleReg);
      break;
    }
    case kX64I32x4GtS: {
      ASSEMBLE_SIMD_BINOP(pcmpgtd);
      break;
    }
    case kX64I32x4GeS: {
      XMMRegister dst = i.OutputSimd128Register();
      XMMRegister src = i.InputSimd128Register(1);
      __ Pminsd(dst, src);
      __ Pcmpeqd(dst, src);
      break;
    }
    case kX64I32x4UConvertF32x4: {
      DCHECK_EQ(i.OutputSimd128Register(), i.InputSimd128Register(0));
      XMMRegister dst = i.OutputSimd128Register();
      XMMRegister tmp = i.TempSimd128Register(0);
      XMMRegister tmp2 = i.TempSimd128Register(1);
      // NAN->0, negative->0
      __ Pxor(tmp2, tmp2);
      __ Maxps(dst, tmp2);
      // scratch: float representation of max_signed
      __ Pcmpeqd(tmp2, tmp2);
      __ Psrld(tmp2, uint8_t{1});  // 0x7fffffff
      __ Cvtdq2ps(tmp2, tmp2);     // 0x4f000000
      // tmp: convert (src-max_signed).
      // Positive overflow lanes -> 0x7FFFFFFF
      // Negative lanes -> 0
      __ Movaps(tmp, dst);
      __ Subps(tmp, tmp2);
      __ Cmpleps(tmp2, tmp);
      __ Cvttps2dq(tmp, tmp);
      __ Pxor(tmp, tmp2);
      __ Pxor(tmp2, tmp2);
      __ Pmaxsd(tmp, tmp2);
      // convert. Overflow lanes above max_signed will be 0x80000000
      __ Cvttps2dq(dst, dst);
      // Add (src-max_signed) for overflow lanes.
      __ Paddd(dst, tmp);
      break;
    }
    case kX64I32x4UConvertI16x8Low: {
      __ Pmovzxwd(i.OutputSimd128Register(), i.InputSimd128Register(0));
      break;
    }
    case kX64I32x4UConvertI16x8High: {
      __ I32x4UConvertI16x8High(i.OutputSimd128Register(),
                                i.InputSimd128Register(0), kScratchDoubleReg);
      break;
    }
    case kX64I32x4ShrU: {
      // Take shift value modulo 2^5.
      ASSEMBLE_SIMD_SHIFT(psrld, 5);
      break;
    }
    case kX64I32x4MinU: {
      ASSEMBLE_SIMD_BINOP(pminud);
      break;
    }
    case kX64I32x4MaxU: {
      ASSEMBLE_SIMD_BINOP(pmaxud);
      break;
    }
    case kX64I32x4GtU: {
      XMMRegister dst = i.OutputSimd128Register();
      XMMRegister src = i.InputSimd128Register(1);
      __ Pmaxud(dst, src);
      __ Pcmpeqd(dst, src);
      __ Pcmpeqd(kScratchDoubleReg, kScratchDoubleReg);
      __ Pxor(dst, kScratchDoubleReg);
      break;
    }
    case kX64I32x4GeU: {
      XMMRegister dst = i.OutputSimd128Register();
      XMMRegister src = i.InputSimd128Register(1);
      __ Pminud(dst, src);
      __ Pcmpeqd(dst, src);
      break;
    }
    case kX64I32x4Abs: {
      __ Pabsd(i.OutputSimd128Register(), i.InputSimd128Register(0));
      break;
    }
    case kX64I32x4BitMask: {
      __ Movmskps(i.OutputRegister(), i.InputSimd128Register(0));
      break;
    }
    case kX64I32x4DotI16x8S: {
      ASSEMBLE_SIMD_BINOP(pmaddwd);
      break;
    }
    case kX64I32x4ExtAddPairwiseI16x8S: {
      XMMRegister dst = i.OutputSimd128Register();
      XMMRegister src1 = i.InputSimd128Register(0);
      // pmaddwd multiplies signed words in src1 and src2, producing signed
      // doublewords, then adds pairwise.
      // src1 = |a|b|c|d|e|f|g|h|
      // src2 = |1|1|1|1|1|1|1|1|
      // dst = | a*1 + b*1 | c*1 + d*1 | e*1 + f*1 | g*1 + h*1 |
      Operand src2 = __ ExternalReferenceAsOperand(
          ExternalReference::address_of_wasm_i16x8_splat_0x0001());
      __ Pmaddwd(dst, src1, src2);
      break;
    }
    case kX64I32x4ExtAddPairwiseI16x8U: {
      __ I32x4ExtAddPairwiseI16x8U(i.OutputSimd128Register(),
                                   i.InputSimd128Register(0));
      break;
    }
    case kX64S128Const: {
      // Emit code for generic constants as all zeros, or ones cases will be
      // handled separately by the selector.
      XMMRegister dst = i.OutputSimd128Register();
      uint32_t imm[4] = {};
      for (int j = 0; j < 4; j++) {
        imm[j] = i.InputUint32(j);
      }
      SetupSimdImmediateInRegister(tasm(), imm, dst);
      break;
    }
    case kX64S128Zero: {
      XMMRegister dst = i.OutputSimd128Register();
      __ Pxor(dst, dst);
      break;
    }
    case kX64S128AllOnes: {
      XMMRegister dst = i.OutputSimd128Register();
      __ Pcmpeqd(dst, dst);
      break;
    }
    case kX64I16x8Splat: {
      XMMRegister dst = i.OutputSimd128Register();
      if (HasRegisterInput(instr, 0)) {
        __ Movd(dst, i.InputRegister(0));
      } else {
        __ Movd(dst, i.InputOperand(0));
      }
      __ Pshuflw(dst, dst, uint8_t{0x0});
      __ Pshufd(dst, dst, uint8_t{0x0});
      break;
    }
    case kX64I16x8ExtractLaneS: {
      Register dst = i.OutputRegister();
      __ Pextrw(dst, i.InputSimd128Register(0), i.InputUint8(1));
      __ movsxwl(dst, dst);
      break;
    }
    case kX64I16x8SConvertI8x16Low: {
      __ Pmovsxbw(i.OutputSimd128Register(), i.InputSimd128Register(0));
      break;
    }
    case kX64I16x8SConvertI8x16High: {
      __ I16x8SConvertI8x16High(i.OutputSimd128Register(),
                                i.InputSimd128Register(0));
      break;
    }
    case kX64I16x8Neg: {
      XMMRegister dst = i.OutputSimd128Register();
      XMMRegister src = i.InputSimd128Register(0);
      if (dst == src) {
        __ Pcmpeqd(kScratchDoubleReg, kScratchDoubleReg);
        __ Psignw(dst, kScratchDoubleReg);
      } else {
        __ Pxor(dst, dst);
        __ Psubw(dst, src);
      }
      break;
    }
    case kX64I16x8Shl: {
      // Take shift value modulo 2^4.
      ASSEMBLE_SIMD_SHIFT(psllw, 4);
      break;
    }
    case kX64I16x8ShrS: {
      // Take shift value modulo 2^4.
      ASSEMBLE_SIMD_SHIFT(psraw, 4);
      break;
    }
    case kX64I16x8SConvertI32x4: {
      ASSEMBLE_SIMD_BINOP(packssdw);
      break;
    }
    case kX64I16x8Add: {
      ASSEMBLE_SIMD_BINOP(paddw);
      break;
    }
    case kX64I16x8AddSatS: {
      ASSEMBLE_SIMD_BINOP(paddsw);
      break;
    }
    case kX64I16x8Sub: {
      ASSEMBLE_SIMD_BINOP(psubw);
      break;
    }
    case kX64I16x8SubSatS: {
      ASSEMBLE_SIMD_BINOP(psubsw);
      break;
    }
    case kX64I16x8Mul: {
      ASSEMBLE_SIMD_BINOP(pmullw);
      break;
    }
    case kX64I16x8MinS: {
      ASSEMBLE_SIMD_BINOP(pminsw);
      break;
    }
    case kX64I16x8MaxS: {
      ASSEMBLE_SIMD_BINOP(pmaxsw);
      break;
    }
    case kX64I16x8Eq: {
      ASSEMBLE_SIMD_BINOP(pcmpeqw);
      break;
    }
    case kX64I16x8Ne: {
      XMMRegister dst = i.OutputSimd128Register();
      __ Pcmpeqw(dst, i.InputSimd128Register(1));
      __ Pcmpeqw(kScratchDoubleReg, kScratchDoubleReg);
      __ Pxor(dst, kScratchDoubleReg);
      break;
    }
    case kX64I16x8GtS: {
      ASSEMBLE_SIMD_BINOP(pcmpgtw);
      break;
    }
    case kX64I16x8GeS: {
      XMMRegister dst = i.OutputSimd128Register();
      XMMRegister src = i.InputSimd128Register(1);
      __ Pminsw(dst, src);
      __ Pcmpeqw(dst, src);
      break;
    }
    case kX64I16x8UConvertI8x16Low: {
      __ Pmovzxbw(i.OutputSimd128Register(), i.InputSimd128Register(0));
      break;
    }
    case kX64I16x8UConvertI8x16High: {
      __ I16x8UConvertI8x16High(i.OutputSimd128Register(),
                                i.InputSimd128Register(0), kScratchDoubleReg);
      break;
    }
    case kX64I16x8ShrU: {
      // Take shift value modulo 2^4.
      ASSEMBLE_SIMD_SHIFT(psrlw, 4);
      break;
    }
    case kX64I16x8UConvertI32x4: {
      ASSEMBLE_SIMD_BINOP(packusdw);
      break;
    }
    case kX64I16x8AddSatU: {
      ASSEMBLE_SIMD_BINOP(paddusw);
      break;
    }
    case kX64I16x8SubSatU: {
      ASSEMBLE_SIMD_BINOP(psubusw);
      break;
    }
    case kX64I16x8MinU: {
      ASSEMBLE_SIMD_BINOP(pminuw);
      break;
    }
    case kX64I16x8MaxU: {
      ASSEMBLE_SIMD_BINOP(pmaxuw);
      break;
    }
    case kX64I16x8GtU: {
      XMMRegister dst = i.OutputSimd128Register();
      XMMRegister src = i.InputSimd128Register(1);
      __ Pmaxuw(dst, src);
      __ Pcmpeqw(dst, src);
      __ Pcmpeqw(kScratchDoubleReg, kScratchDoubleReg);
      __ Pxor(dst, kScratchDoubleReg);
      break;
    }
    case kX64I16x8GeU: {
      XMMRegister dst = i.OutputSimd128Register();
      XMMRegister src = i.InputSimd128Register(1);
      __ Pminuw(dst, src);
      __ Pcmpeqw(dst, src);
      break;
    }
    case kX64I16x8RoundingAverageU: {
      ASSEMBLE_SIMD_BINOP(pavgw);
      break;
    }
    case kX64I16x8Abs: {
      __ Pabsw(i.OutputSimd128Register(), i.InputSimd128Register(0));
      break;
    }
    case kX64I16x8BitMask: {
      Register dst = i.OutputRegister();
      __ Packsswb(kScratchDoubleReg, i.InputSimd128Register(0));
      __ Pmovmskb(dst, kScratchDoubleReg);
      __ shrq(dst, Immediate(8));
      break;
    }
    case kX64I16x8ExtMulLowI8x16S: {
      __ I16x8ExtMulLow(i.OutputSimd128Register(), i.InputSimd128Register(0),
                        i.InputSimd128Register(1), kScratchDoubleReg,
                        /*is_signed=*/true);
      break;
    }
    case kX64I16x8ExtMulHighI8x16S: {
      __ I16x8ExtMulHighS(i.OutputSimd128Register(), i.InputSimd128Register(0),
                          i.InputSimd128Register(1), kScratchDoubleReg);
      break;
    }
    case kX64I16x8ExtMulLowI8x16U: {
      __ I16x8ExtMulLow(i.OutputSimd128Register(), i.InputSimd128Register(0),
                        i.InputSimd128Register(1), kScratchDoubleReg,
                        /*is_signed=*/false);
      break;
    }
    case kX64I16x8ExtMulHighI8x16U: {
      __ I16x8ExtMulHighU(i.OutputSimd128Register(), i.InputSimd128Register(0),
                          i.InputSimd128Register(1), kScratchDoubleReg);
      break;
    }
    case kX64I16x8ExtAddPairwiseI8x16S: {
      __ I16x8ExtAddPairwiseI8x16S(i.OutputSimd128Register(),
                                   i.InputSimd128Register(0));
      break;
    }
    case kX64I16x8ExtAddPairwiseI8x16U: {
      XMMRegister dst = i.OutputSimd128Register();
      XMMRegister src1 = i.InputSimd128Register(0);
      Operand src2 = __ ExternalReferenceAsOperand(
          ExternalReference::address_of_wasm_i8x16_splat_0x01());
      __ Pmaddubsw(dst, src1, src2);
      break;
    }
    case kX64I16x8Q15MulRSatS: {
      __ I16x8Q15MulRSatS(i.OutputSimd128Register(), i.InputSimd128Register(0),
                          i.InputSimd128Register(1));
      break;
    }
    case kX64I8x16Splat: {
      XMMRegister dst = i.OutputSimd128Register();
      if (CpuFeatures::IsSupported(AVX2)) {
        CpuFeatureScope avx_scope(tasm(), AVX);
        CpuFeatureScope avx2_scope(tasm(), AVX2);
        if (HasRegisterInput(instr, 0)) {
          __ vmovd(kScratchDoubleReg, i.InputRegister(0));
          __ vpbroadcastb(dst, kScratchDoubleReg);
        } else {
          __ vpbroadcastb(dst, i.InputOperand(0));
        }
      } else {
        if (HasRegisterInput(instr, 0)) {
          __ Movd(dst, i.InputRegister(0));
        } else {
          __ Movd(dst, i.InputOperand(0));
        }
        __ Xorps(kScratchDoubleReg, kScratchDoubleReg);
        __ Pshufb(dst, kScratchDoubleReg);
      }

      break;
    }
    case kX64Pextrb: {
      EmitOOLTrapIfNeeded(zone(), this, opcode, instr, __ pc_offset());
      size_t index = 0;
      if (HasAddressingMode(instr)) {
        Operand operand = i.MemoryOperand(&index);
        __ Pextrb(operand, i.InputSimd128Register(index),
                  i.InputUint8(index + 1));
      } else {
        __ Pextrb(i.OutputRegister(), i.InputSimd128Register(0),
                  i.InputUint8(1));
      }
      break;
    }
    case kX64Pextrw: {
      EmitOOLTrapIfNeeded(zone(), this, opcode, instr, __ pc_offset());
      size_t index = 0;
      if (HasAddressingMode(instr)) {
        Operand operand = i.MemoryOperand(&index);
        __ Pextrw(operand, i.InputSimd128Register(index),
                  i.InputUint8(index + 1));
      } else {
        __ Pextrw(i.OutputRegister(), i.InputSimd128Register(0),
                  i.InputUint8(1));
      }
      break;
    }
    case kX64I8x16ExtractLaneS: {
      Register dst = i.OutputRegister();
      __ Pextrb(dst, i.InputSimd128Register(0), i.InputUint8(1));
      __ movsxbl(dst, dst);
      break;
    }
    case kX64Pinsrb: {
      ASSEMBLE_PINSR(Pinsrb);
      break;
    }
    case kX64Pinsrw: {
      ASSEMBLE_PINSR(Pinsrw);
      break;
    }
    case kX64Pinsrd: {
      ASSEMBLE_PINSR(Pinsrd);
      break;
    }
    case kX64Pinsrq: {
      ASSEMBLE_PINSR(Pinsrq);
      break;
    }
    case kX64I8x16SConvertI16x8: {
      ASSEMBLE_SIMD_BINOP(packsswb);
      break;
    }
    case kX64I8x16Neg: {
      XMMRegister dst = i.OutputSimd128Register();
      XMMRegister src = i.InputSimd128Register(0);
      if (dst == src) {
        __ Pcmpeqd(kScratchDoubleReg, kScratchDoubleReg);
        __ Psignb(dst, kScratchDoubleReg);
      } else {
        __ Pxor(dst, dst);
        __ Psubb(dst, src);
      }
      break;
    }
    case kX64I8x16Shl: {
      XMMRegister dst = i.OutputSimd128Register();
      DCHECK_EQ(dst, i.InputSimd128Register(0));
      // Temp registers for shift mask and additional moves to XMM registers.
      Register tmp = i.ToRegister(instr->TempAt(0));
      XMMRegister tmp_simd = i.TempSimd128Register(1);
      if (HasImmediateInput(instr, 1)) {
        // Perform 16-bit shift, then mask away low bits.
        uint8_t shift = i.InputInt3(1);
        __ Psllw(dst, byte{shift});

        uint8_t bmask = static_cast<uint8_t>(0xff << shift);
        uint32_t mask = bmask << 24 | bmask << 16 | bmask << 8 | bmask;
        __ movl(tmp, Immediate(mask));
        __ Movd(tmp_simd, tmp);
        __ Pshufd(tmp_simd, tmp_simd, uint8_t{0});
        __ Pand(dst, tmp_simd);
      } else {
        // Mask off the unwanted bits before word-shifting.
        __ Pcmpeqw(kScratchDoubleReg, kScratchDoubleReg);
        // Take shift value modulo 8.
        __ movq(tmp, i.InputRegister(1));
        __ andq(tmp, Immediate(7));
        __ addq(tmp, Immediate(8));
        __ Movq(tmp_simd, tmp);
        __ Psrlw(kScratchDoubleReg, tmp_simd);
        __ Packuswb(kScratchDoubleReg, kScratchDoubleReg);
        __ Pand(dst, kScratchDoubleReg);
        // TODO(zhin): subq here to avoid asking for another temporary register,
        // examine codegen for other i8x16 shifts, they use less instructions.
        __ subq(tmp, Immediate(8));
        __ Movq(tmp_simd, tmp);
        __ Psllw(dst, tmp_simd);
      }
      break;
    }
    case kX64I8x16ShrS: {
      XMMRegister dst = i.OutputSimd128Register();
      DCHECK_EQ(dst, i.InputSimd128Register(0));
      if (HasImmediateInput(instr, 1)) {
        __ Punpckhbw(kScratchDoubleReg, dst);
        __ Punpcklbw(dst, dst);
        uint8_t shift = i.InputInt3(1) + 8;
        __ Psraw(kScratchDoubleReg, shift);
        __ Psraw(dst, shift);
        __ Packsswb(dst, kScratchDoubleReg);
      } else {
        // Temp registers for shift mask andadditional moves to XMM registers.
        Register tmp = i.ToRegister(instr->TempAt(0));
        XMMRegister tmp_simd = i.TempSimd128Register(1);
        // Unpack the bytes into words, do arithmetic shifts, and repack.
        __ Punpckhbw(kScratchDoubleReg, dst);
        __ Punpcklbw(dst, dst);
        // Prepare shift value
        __ movq(tmp, i.InputRegister(1));
        // Take shift value modulo 8.
        __ andq(tmp, Immediate(7));
        __ addq(tmp, Immediate(8));
        __ Movq(tmp_simd, tmp);
        __ Psraw(kScratchDoubleReg, tmp_simd);
        __ Psraw(dst, tmp_simd);
        __ Packsswb(dst, kScratchDoubleReg);
      }
      break;
    }
    case kX64I8x16Add: {
      ASSEMBLE_SIMD_BINOP(paddb);
      break;
    }
    case kX64I8x16AddSatS: {
      ASSEMBLE_SIMD_BINOP(paddsb);
      break;
    }
    case kX64I8x16Sub: {
      ASSEMBLE_SIMD_BINOP(psubb);
      break;
    }
    case kX64I8x16SubSatS: {
      ASSEMBLE_SIMD_BINOP(psubsb);
      break;
    }
    case kX64I8x16MinS: {
      ASSEMBLE_SIMD_BINOP(pminsb);
      break;
    }
    case kX64I8x16MaxS: {
      ASSEMBLE_SIMD_BINOP(pmaxsb);
      break;
    }
    case kX64I8x16Eq: {
      ASSEMBLE_SIMD_BINOP(pcmpeqb);
      break;
    }
    case kX64I8x16Ne: {
      XMMRegister dst = i.OutputSimd128Register();
      __ Pcmpeqb(dst, i.InputSimd128Register(1));
      __ Pcmpeqb(kScratchDoubleReg, kScratchDoubleReg);
      __ Pxor(dst, kScratchDoubleReg);
      break;
    }
    case kX64I8x16GtS: {
      ASSEMBLE_SIMD_BINOP(pcmpgtb);
      break;
    }
    case kX64I8x16GeS: {
      XMMRegister dst = i.OutputSimd128Register();
      XMMRegister src = i.InputSimd128Register(1);
      __ Pminsb(dst, src);
      __ Pcmpeqb(dst, src);
      break;
    }
    case kX64I8x16UConvertI16x8: {
      ASSEMBLE_SIMD_BINOP(packuswb);
      break;
    }
    case kX64I8x16ShrU: {
      XMMRegister dst = i.OutputSimd128Register();
      // Unpack the bytes into words, do logical shifts, and repack.
      DCHECK_EQ(dst, i.InputSimd128Register(0));
      // Temp registers for shift mask andadditional moves to XMM registers.
      Register tmp = i.ToRegister(instr->TempAt(0));
      XMMRegister tmp_simd = i.TempSimd128Register(1);
      if (HasImmediateInput(instr, 1)) {
        // Perform 16-bit shift, then mask away high bits.
        uint8_t shift = i.InputInt3(1);
        __ Psrlw(dst, byte{shift});

        uint8_t bmask = 0xff >> shift;
        uint32_t mask = bmask << 24 | bmask << 16 | bmask << 8 | bmask;
        __ movl(tmp, Immediate(mask));
        __ Movd(tmp_simd, tmp);
        __ Pshufd(tmp_simd, tmp_simd, byte{0});
        __ Pand(dst, tmp_simd);
      } else {
        __ Punpckhbw(kScratchDoubleReg, dst);
        __ Punpcklbw(dst, dst);
        // Prepare shift value
        __ movq(tmp, i.InputRegister(1));
        // Take shift value modulo 8.
        __ andq(tmp, Immediate(7));
        __ addq(tmp, Immediate(8));
        __ Movq(tmp_simd, tmp);
        __ Psrlw(kScratchDoubleReg, tmp_simd);
        __ Psrlw(dst, tmp_simd);
        __ Packuswb(dst, kScratchDoubleReg);
      }
      break;
    }
    case kX64I8x16AddSatU: {
      ASSEMBLE_SIMD_BINOP(paddusb);
      break;
    }
    case kX64I8x16SubSatU: {
      ASSEMBLE_SIMD_BINOP(psubusb);
      break;
    }
    case kX64I8x16MinU: {
      ASSEMBLE_SIMD_BINOP(pminub);
      break;
    }
    case kX64I8x16MaxU: {
      ASSEMBLE_SIMD_BINOP(pmaxub);
      break;
    }
    case kX64I8x16GtU: {
      XMMRegister dst = i.OutputSimd128Register();
      XMMRegister src = i.InputSimd128Register(1);
      __ Pmaxub(dst, src);
      __ Pcmpeqb(dst, src);
      __ Pcmpeqb(kScratchDoubleReg, kScratchDoubleReg);
      __ Pxor(dst, kScratchDoubleReg);
      break;
    }
    case kX64I8x16GeU: {
      XMMRegister dst = i.OutputSimd128Register();
      XMMRegister src = i.InputSimd128Register(1);
      __ Pminub(dst, src);
      __ Pcmpeqb(dst, src);
      break;
    }
    case kX64I8x16RoundingAverageU: {
      ASSEMBLE_SIMD_BINOP(pavgb);
      break;
    }
    case kX64I8x16Abs: {
      __ Pabsb(i.OutputSimd128Register(), i.InputSimd128Register(0));
      break;
    }
    case kX64I8x16BitMask: {
      __ Pmovmskb(i.OutputRegister(), i.InputSimd128Register(0));
      break;
    }
    case kX64I32x4ExtMulLowI16x8S: {
      __ I32x4ExtMul(i.OutputSimd128Register(), i.InputSimd128Register(0),
                     i.InputSimd128Register(1), kScratchDoubleReg, /*low=*/true,
                     /*is_signed=*/true);
      break;
    }
    case kX64I32x4ExtMulHighI16x8S: {
      __ I32x4ExtMul(i.OutputSimd128Register(), i.InputSimd128Register(0),
                     i.InputSimd128Register(1), kScratchDoubleReg,
                     /*low=*/false,
                     /*is_signed=*/true);
      break;
    }
    case kX64I32x4ExtMulLowI16x8U: {
      __ I32x4ExtMul(i.OutputSimd128Register(), i.InputSimd128Register(0),
                     i.InputSimd128Register(1), kScratchDoubleReg, /*low=*/true,
                     /*is_signed=*/false);
      break;
    }
    case kX64I32x4ExtMulHighI16x8U: {
      __ I32x4ExtMul(i.OutputSimd128Register(), i.InputSimd128Register(0),
                     i.InputSimd128Register(1), kScratchDoubleReg,
                     /*low=*/false,
                     /*is_signed=*/false);
      break;
    }
    case kX64S128And: {
      ASSEMBLE_SIMD_BINOP(pand);
      break;
    }
    case kX64S128Or: {
      ASSEMBLE_SIMD_BINOP(por);
      break;
    }
    case kX64S128Xor: {
      ASSEMBLE_SIMD_BINOP(pxor);
      break;
    }
    case kX64S128Not: {
      XMMRegister dst = i.OutputSimd128Register();
      XMMRegister src = i.InputSimd128Register(0);
      if (dst == src) {
        __ Pcmpeqd(kScratchDoubleReg, kScratchDoubleReg);
        __ Pxor(dst, kScratchDoubleReg);
      } else {
        __ Pcmpeqd(dst, dst);
        __ Pxor(dst, src);
      }
      break;
    }
    case kX64S128Select: {
      __ S128Select(i.OutputSimd128Register(), i.InputSimd128Register(0),
                    i.InputSimd128Register(1), i.InputSimd128Register(2),
                    kScratchDoubleReg);
      break;
    }
    case kX64S128AndNot: {
      XMMRegister dst = i.OutputSimd128Register();
      DCHECK_EQ(dst, i.InputSimd128Register(0));
      // The inputs have been inverted by instruction selector, so we can call
      // andnps here without any modifications.
      __ Andnps(dst, i.InputSimd128Register(1));
      break;
    }
    case kX64I8x16Swizzle: {
      bool omit_add = MiscField::decode(instr->opcode());
      __ I8x16Swizzle(i.OutputSimd128Register(), i.InputSimd128Register(0),
                      i.InputSimd128Register(1), omit_add);
      break;
    }
    case kX64I8x16Shuffle: {
      XMMRegister dst = i.OutputSimd128Register();
      XMMRegister tmp_simd = i.TempSimd128Register(0);
      DCHECK_NE(tmp_simd, i.InputSimd128Register(0));
      if (instr->InputCount() == 5) {  // only one input operand
        uint32_t mask[4] = {};
        DCHECK_EQ(i.OutputSimd128Register(), i.InputSimd128Register(0));
        for (int j = 4; j > 0; j--) {
          mask[j - 1] = i.InputUint32(j);
        }

        SetupSimdImmediateInRegister(tasm(), mask, tmp_simd);
        __ Pshufb(dst, tmp_simd);
      } else {  // two input operands
        DCHECK_NE(tmp_simd, i.InputSimd128Register(1));
        DCHECK_EQ(6, instr->InputCount());
        ASSEMBLE_SIMD_INSTR(Movdqu, kScratchDoubleReg, 0);
        uint32_t mask1[4] = {};
        for (int j = 5; j > 1; j--) {
          uint32_t lanes = i.InputUint32(j);
          for (int k = 0; k < 32; k += 8) {
            uint8_t lane = lanes >> k;
            mask1[j - 2] |= (lane < kSimd128Size ? lane : 0x80) << k;
          }
        }
        SetupSimdImmediateInRegister(tasm(), mask1, tmp_simd);
        __ Pshufb(kScratchDoubleReg, tmp_simd);
        uint32_t mask2[4] = {};
        if (instr->InputAt(1)->IsSimd128Register()) {
          XMMRegister src1 = i.InputSimd128Register(1);
          if (src1 != dst) __ Movdqa(dst, src1);
        } else {
          __ Movdqu(dst, i.InputOperand(1));
        }
        for (int j = 5; j > 1; j--) {
          uint32_t lanes = i.InputUint32(j);
          for (int k = 0; k < 32; k += 8) {
            uint8_t lane = lanes >> k;
            mask2[j - 2] |= (lane >= kSimd128Size ? (lane & 0x0F) : 0x80) << k;
          }
        }
        SetupSimdImmediateInRegister(tasm(), mask2, tmp_simd);
        __ Pshufb(dst, tmp_simd);
        __ Por(dst, kScratchDoubleReg);
      }
      break;
    }
    case kX64I8x16Popcnt: {
      __ I8x16Popcnt(i.OutputSimd128Register(), i.InputSimd128Register(0),
                     i.TempSimd128Register(0));
      break;
    }
    case kX64S128Load8Splat: {
      EmitOOLTrapIfNeeded(zone(), this, opcode, instr, __ pc_offset());
      XMMRegister dst = i.OutputSimd128Register();
      if (CpuFeatures::IsSupported(AVX2)) {
        CpuFeatureScope avx2_scope(tasm(), AVX2);
        __ vpbroadcastb(dst, i.MemoryOperand());
      } else {
        __ Pinsrb(dst, dst, i.MemoryOperand(), 0);
        __ Pxor(kScratchDoubleReg, kScratchDoubleReg);
        __ Pshufb(dst, kScratchDoubleReg);
      }
      break;
    }
    case kX64S128Load16Splat: {
      EmitOOLTrapIfNeeded(zone(), this, opcode, instr, __ pc_offset());
      XMMRegister dst = i.OutputSimd128Register();
      if (CpuFeatures::IsSupported(AVX2)) {
        CpuFeatureScope avx2_scope(tasm(), AVX2);
        __ vpbroadcastw(dst, i.MemoryOperand());
      } else {
        __ Pinsrw(dst, dst, i.MemoryOperand(), 0);
        __ Pshuflw(dst, dst, uint8_t{0});
        __ Punpcklqdq(dst, dst);
      }
      break;
    }
    case kX64S128Load32Splat: {
      EmitOOLTrapIfNeeded(zone(), this, opcode, instr, __ pc_offset());
      if (CpuFeatures::IsSupported(AVX)) {
        CpuFeatureScope avx_scope(tasm(), AVX);
        __ vbroadcastss(i.OutputSimd128Register(), i.MemoryOperand());
      } else {
        __ movss(i.OutputSimd128Register(), i.MemoryOperand());
        __ shufps(i.OutputSimd128Register(), i.OutputSimd128Register(),
                  byte{0});
      }
      break;
    }
    case kX64S128Load64Splat: {
      EmitOOLTrapIfNeeded(zone(), this, opcode, instr, __ pc_offset());
      __ Movddup(i.OutputSimd128Register(), i.MemoryOperand());
      break;
    }
    case kX64S128Load8x8S: {
      EmitOOLTrapIfNeeded(zone(), this, opcode, instr, __ pc_offset());
      __ Pmovsxbw(i.OutputSimd128Register(), i.MemoryOperand());
      break;
    }
    case kX64S128Load8x8U: {
      EmitOOLTrapIfNeeded(zone(), this, opcode, instr, __ pc_offset());
      __ Pmovzxbw(i.OutputSimd128Register(), i.MemoryOperand());
      break;
    }
    case kX64S128Load16x4S: {
      EmitOOLTrapIfNeeded(zone(), this, opcode, instr, __ pc_offset());
      __ Pmovsxwd(i.OutputSimd128Register(), i.MemoryOperand());
      break;
    }
    case kX64S128Load16x4U: {
      EmitOOLTrapIfNeeded(zone(), this, opcode, instr, __ pc_offset());
      __ Pmovzxwd(i.OutputSimd128Register(), i.MemoryOperand());
      break;
    }
    case kX64S128Load32x2S: {
      EmitOOLTrapIfNeeded(zone(), this, opcode, instr, __ pc_offset());
      __ Pmovsxdq(i.OutputSimd128Register(), i.MemoryOperand());
      break;
    }
    case kX64S128Load32x2U: {
      EmitOOLTrapIfNeeded(zone(), this, opcode, instr, __ pc_offset());
      __ Pmovzxdq(i.OutputSimd128Register(), i.MemoryOperand());
      break;
    }
    case kX64S128Store32Lane: {
      EmitOOLTrapIfNeeded(zone(), this, opcode, instr, __ pc_offset());
      size_t index = 0;
      Operand operand = i.MemoryOperand(&index);
      uint8_t lane = i.InputUint8(index + 1);
      __ S128Store32Lane(operand, i.InputSimd128Register(index), lane);
      break;
    }
    case kX64S128Store64Lane: {
      EmitOOLTrapIfNeeded(zone(), this, opcode, instr, __ pc_offset());
      size_t index = 0;
      Operand operand = i.MemoryOperand(&index);
      uint8_t lane = i.InputUint8(index + 1);
      __ S128Store64Lane(operand, i.InputSimd128Register(index), lane);
      break;
    }
    case kX64Shufps: {
      __ Shufps(i.OutputSimd128Register(), i.InputSimd128Register(0),
                i.InputSimd128Register(1), i.InputUint8(2));
      break;
    }
    case kX64S32x4Rotate: {
      XMMRegister dst = i.OutputSimd128Register();
      XMMRegister src = i.InputSimd128Register(0);
      uint8_t mask = i.InputUint8(1);
      if (dst == src) {
        // 1-byte shorter encoding than pshufd.
        __ Shufps(dst, src, src, mask);
      } else {
        __ Pshufd(dst, src, mask);
      }
      break;
    }
    case kX64S32x4Swizzle: {
      DCHECK_EQ(2, instr->InputCount());
      ASSEMBLE_SIMD_IMM_INSTR(Pshufd, i.OutputSimd128Register(), 0,
                              i.InputUint8(1));
      break;
    }
    case kX64S32x4Shuffle: {
      DCHECK_EQ(4, instr->InputCount());  // Swizzles should be handled above.
      uint8_t shuffle = i.InputUint8(2);
      DCHECK_NE(0xe4, shuffle);  // A simple blend should be handled below.
      ASSEMBLE_SIMD_IMM_INSTR(Pshufd, kScratchDoubleReg, 1, shuffle);
      ASSEMBLE_SIMD_IMM_INSTR(Pshufd, i.OutputSimd128Register(), 0, shuffle);
      __ Pblendw(i.OutputSimd128Register(), kScratchDoubleReg, i.InputUint8(3));
      break;
    }
    case kX64S16x8Blend: {
      ASSEMBLE_SIMD_IMM_SHUFFLE(pblendw, i.InputUint8(2));
      break;
    }
    case kX64S16x8HalfShuffle1: {
      XMMRegister dst = i.OutputSimd128Register();
      ASSEMBLE_SIMD_IMM_INSTR(Pshuflw, dst, 0, i.InputUint8(1));
      __ Pshufhw(dst, dst, i.InputUint8(2));
      break;
    }
    case kX64S16x8HalfShuffle2: {
      XMMRegister dst = i.OutputSimd128Register();
      ASSEMBLE_SIMD_IMM_INSTR(Pshuflw, kScratchDoubleReg, 1, i.InputUint8(2));
      __ Pshufhw(kScratchDoubleReg, kScratchDoubleReg, i.InputUint8(3));
      ASSEMBLE_SIMD_IMM_INSTR(Pshuflw, dst, 0, i.InputUint8(2));
      __ Pshufhw(dst, dst, i.InputUint8(3));
      __ Pblendw(dst, kScratchDoubleReg, i.InputUint8(4));
      break;
    }
    case kX64S8x16Alignr: {
      ASSEMBLE_SIMD_IMM_SHUFFLE(palignr, i.InputUint8(2));
      break;
    }
    case kX64S16x8Dup: {
      XMMRegister dst = i.OutputSimd128Register();
      uint8_t lane = i.InputInt8(1) & 0x7;
      uint8_t lane4 = lane & 0x3;
      uint8_t half_dup = lane4 | (lane4 << 2) | (lane4 << 4) | (lane4 << 6);
      if (lane < 4) {
        ASSEMBLE_SIMD_IMM_INSTR(Pshuflw, dst, 0, half_dup);
        __ Pshufd(dst, dst, uint8_t{0});
      } else {
        ASSEMBLE_SIMD_IMM_INSTR(Pshufhw, dst, 0, half_dup);
        __ Pshufd(dst, dst, uint8_t{0xaa});
      }
      break;
    }
    case kX64S8x16Dup: {
      XMMRegister dst = i.OutputSimd128Register();
      uint8_t lane = i.InputInt8(1) & 0xf;
      DCHECK_EQ(dst, i.InputSimd128Register(0));
      if (lane < 8) {
        __ Punpcklbw(dst, dst);
      } else {
        __ Punpckhbw(dst, dst);
      }
      lane &= 0x7;
      uint8_t lane4 = lane & 0x3;
      uint8_t half_dup = lane4 | (lane4 << 2) | (lane4 << 4) | (lane4 << 6);
      if (lane < 4) {
        __ Pshuflw(dst, dst, half_dup);
        __ Pshufd(dst, dst, uint8_t{0});
      } else {
        __ Pshufhw(dst, dst, half_dup);
        __ Pshufd(dst, dst, uint8_t{0xaa});
      }
      break;
    }
    case kX64S64x2UnpackHigh:
      ASSEMBLE_SIMD_PUNPCK_SHUFFLE(punpckhqdq);
      break;
    case kX64S32x4UnpackHigh:
      ASSEMBLE_SIMD_PUNPCK_SHUFFLE(punpckhdq);
      break;
    case kX64S16x8UnpackHigh:
      ASSEMBLE_SIMD_PUNPCK_SHUFFLE(punpckhwd);
      break;
    case kX64S8x16UnpackHigh:
      ASSEMBLE_SIMD_PUNPCK_SHUFFLE(punpckhbw);
      break;
    case kX64S64x2UnpackLow:
      ASSEMBLE_SIMD_PUNPCK_SHUFFLE(punpcklqdq);
      break;
    case kX64S32x4UnpackLow:
      ASSEMBLE_SIMD_PUNPCK_SHUFFLE(punpckldq);
      break;
    case kX64S16x8UnpackLow:
      ASSEMBLE_SIMD_PUNPCK_SHUFFLE(punpcklwd);
      break;
    case kX64S8x16UnpackLow:
      ASSEMBLE_SIMD_PUNPCK_SHUFFLE(punpcklbw);
      break;
    case kX64S16x8UnzipHigh: {
      XMMRegister dst = i.OutputSimd128Register();
      XMMRegister src2 = dst;
      DCHECK_EQ(dst, i.InputSimd128Register(0));
      if (instr->InputCount() == 2) {
        ASSEMBLE_SIMD_INSTR(Movdqu, kScratchDoubleReg, 1);
        __ Psrld(kScratchDoubleReg, byte{16});
        src2 = kScratchDoubleReg;
      }
      __ Psrld(dst, byte{16});
      __ Packusdw(dst, src2);
      break;
    }
    case kX64S16x8UnzipLow: {
      XMMRegister dst = i.OutputSimd128Register();
      XMMRegister src2 = dst;
      DCHECK_EQ(dst, i.InputSimd128Register(0));
      __ Pxor(kScratchDoubleReg, kScratchDoubleReg);
      if (instr->InputCount() == 2) {
        ASSEMBLE_SIMD_IMM_INSTR(Pblendw, kScratchDoubleReg, 1, uint8_t{0x55});
        src2 = kScratchDoubleReg;
      }
      __ Pblendw(dst, kScratchDoubleReg, uint8_t{0xaa});
      __ Packusdw(dst, src2);
      break;
    }
    case kX64S8x16UnzipHigh: {
      XMMRegister dst = i.OutputSimd128Register();
      XMMRegister src2 = dst;
      DCHECK_EQ(dst, i.InputSimd128Register(0));
      if (instr->InputCount() == 2) {
        ASSEMBLE_SIMD_INSTR(Movdqu, kScratchDoubleReg, 1);
        __ Psrlw(kScratchDoubleReg, byte{8});
        src2 = kScratchDoubleReg;
      }
      __ Psrlw(dst, byte{8});
      __ Packuswb(dst, src2);
      break;
    }
    case kX64S8x16UnzipLow: {
      XMMRegister dst = i.OutputSimd128Register();
      XMMRegister src2 = dst;
      DCHECK_EQ(dst, i.InputSimd128Register(0));
      if (instr->InputCount() == 2) {
        ASSEMBLE_SIMD_INSTR(Movdqu, kScratchDoubleReg, 1);
        __ Psllw(kScratchDoubleReg, byte{8});
        __ Psrlw(kScratchDoubleReg, byte{8});
        src2 = kScratchDoubleReg;
      }
      __ Psllw(dst, byte{8});
      __ Psrlw(dst, byte{8});
      __ Packuswb(dst, src2);
      break;
    }
    case kX64S8x16TransposeLow: {
      XMMRegister dst = i.OutputSimd128Register();
      DCHECK_EQ(dst, i.InputSimd128Register(0));
      __ Psllw(dst, byte{8});
      if (instr->InputCount() == 1) {
        __ Movdqa(kScratchDoubleReg, dst);
      } else {
        DCHECK_EQ(2, instr->InputCount());
        ASSEMBLE_SIMD_INSTR(Movdqu, kScratchDoubleReg, 1);
        __ Psllw(kScratchDoubleReg, byte{8});
      }
      __ Psrlw(dst, byte{8});
      __ Por(dst, kScratchDoubleReg);
      break;
    }
    case kX64S8x16TransposeHigh: {
      XMMRegister dst = i.OutputSimd128Register();
      DCHECK_EQ(dst, i.InputSimd128Register(0));
      __ Psrlw(dst, byte{8});
      if (instr->InputCount() == 1) {
        __ Movdqa(kScratchDoubleReg, dst);
      } else {
        DCHECK_EQ(2, instr->InputCount());
        ASSEMBLE_SIMD_INSTR(Movdqu, kScratchDoubleReg, 1);
        __ Psrlw(kScratchDoubleReg, byte{8});
      }
      __ Psllw(kScratchDoubleReg, byte{8});
      __ Por(dst, kScratchDoubleReg);
      break;
    }
    case kX64S8x8Reverse:
    case kX64S8x4Reverse:
    case kX64S8x2Reverse: {
      DCHECK_EQ(1, instr->InputCount());
      XMMRegister dst = i.OutputSimd128Register();
      DCHECK_EQ(dst, i.InputSimd128Register(0));
      if (arch_opcode != kX64S8x2Reverse) {
        // First shuffle words into position.
        uint8_t shuffle_mask = arch_opcode == kX64S8x4Reverse ? 0xB1 : 0x1B;
        __ Pshuflw(dst, dst, shuffle_mask);
        __ Pshufhw(dst, dst, shuffle_mask);
      }
      __ Movdqa(kScratchDoubleReg, dst);
      __ Psrlw(kScratchDoubleReg, byte{8});
      __ Psllw(dst, byte{8});
      __ Por(dst, kScratchDoubleReg);
      break;
    }
    case kX64V128AnyTrue: {
      Register dst = i.OutputRegister();
      XMMRegister src = i.InputSimd128Register(0);

      __ xorq(dst, dst);
      __ Ptest(src, src);
      __ setcc(not_equal, dst);
      break;
    }
    // Need to split up all the different lane structures because the
    // comparison instruction used matters, e.g. given 0xff00, pcmpeqb returns
    // 0x0011, pcmpeqw returns 0x0000, ptest will set ZF to 0 and 1
    // respectively.
    case kX64I64x2AllTrue: {
      ASSEMBLE_SIMD_ALL_TRUE(Pcmpeqq);
      break;
    }
    case kX64I32x4AllTrue: {
      ASSEMBLE_SIMD_ALL_TRUE(Pcmpeqd);
      break;
    }
    case kX64I16x8AllTrue: {
      ASSEMBLE_SIMD_ALL_TRUE(Pcmpeqw);
      break;
    }
    case kX64I8x16AllTrue: {
      ASSEMBLE_SIMD_ALL_TRUE(Pcmpeqb);
      break;
    }
    case kWord32AtomicExchangeInt8: {
      __ xchgb(i.InputRegister(0), i.MemoryOperand(1));
      __ movsxbl(i.InputRegister(0), i.InputRegister(0));
      break;
    }
    case kWord32AtomicExchangeUint8: {
      __ xchgb(i.InputRegister(0), i.MemoryOperand(1));
      __ movzxbl(i.InputRegister(0), i.InputRegister(0));
      break;
    }
    case kWord32AtomicExchangeInt16: {
      __ xchgw(i.InputRegister(0), i.MemoryOperand(1));
      __ movsxwl(i.InputRegister(0), i.InputRegister(0));
      break;
    }
    case kWord32AtomicExchangeUint16: {
      __ xchgw(i.InputRegister(0), i.MemoryOperand(1));
      __ movzxwl(i.InputRegister(0), i.InputRegister(0));
      break;
    }
    case kWord32AtomicExchangeWord32: {
      __ xchgl(i.InputRegister(0), i.MemoryOperand(1));
      break;
    }
    case kWord32AtomicCompareExchangeInt8: {
      __ lock();
      __ cmpxchgb(i.MemoryOperand(2), i.InputRegister(1));
      __ movsxbl(rax, rax);
      break;
    }
    case kWord32AtomicCompareExchangeUint8: {
      __ lock();
      __ cmpxchgb(i.MemoryOperand(2), i.InputRegister(1));
      __ movzxbl(rax, rax);
      break;
    }
    case kWord32AtomicCompareExchangeInt16: {
      __ lock();
      __ cmpxchgw(i.MemoryOperand(2), i.InputRegister(1));
      __ movsxwl(rax, rax);
      break;
    }
    case kWord32AtomicCompareExchangeUint16: {
      __ lock();
      __ cmpxchgw(i.MemoryOperand(2), i.InputRegister(1));
      __ movzxwl(rax, rax);
      break;
    }
    case kWord32AtomicCompareExchangeWord32: {
      __ lock();
      __ cmpxchgl(i.MemoryOperand(2), i.InputRegister(1));
      break;
    }
#define ATOMIC_BINOP_CASE(op, inst)              \
  case kWord32Atomic##op##Int8:                  \
    ASSEMBLE_ATOMIC_BINOP(inst, movb, cmpxchgb); \
    __ movsxbl(rax, rax);                        \
    break;                                       \
  case kWord32Atomic##op##Uint8:                 \
    ASSEMBLE_ATOMIC_BINOP(inst, movb, cmpxchgb); \
    __ movzxbl(rax, rax);                        \
    break;                                       \
  case kWord32Atomic##op##Int16:                 \
    ASSEMBLE_ATOMIC_BINOP(inst, movw, cmpxchgw); \
    __ movsxwl(rax, rax);                        \
    break;                                       \
  case kWord32Atomic##op##Uint16:                \
    ASSEMBLE_ATOMIC_BINOP(inst, movw, cmpxchgw); \
    __ movzxwl(rax, rax);                        \
    break;                                       \
  case kWord32Atomic##op##Word32:                \
    ASSEMBLE_ATOMIC_BINOP(inst, movl, cmpxchgl); \
    break;
      ATOMIC_BINOP_CASE(Add, addl)
      ATOMIC_BINOP_CASE(Sub, subl)
      ATOMIC_BINOP_CASE(And, andl)
      ATOMIC_BINOP_CASE(Or, orl)
      ATOMIC_BINOP_CASE(Xor, xorl)
#undef ATOMIC_BINOP_CASE
    case kX64Word64AtomicExchangeUint8: {
      __ xchgb(i.InputRegister(0), i.MemoryOperand(1));
      __ movzxbq(i.InputRegister(0), i.InputRegister(0));
      break;
    }
    case kX64Word64AtomicExchangeUint16: {
      __ xchgw(i.InputRegister(0), i.MemoryOperand(1));
      __ movzxwq(i.InputRegister(0), i.InputRegister(0));
      break;
    }
    case kX64Word64AtomicExchangeUint32: {
      __ xchgl(i.InputRegister(0), i.MemoryOperand(1));
      break;
    }
    case kX64Word64AtomicExchangeUint64: {
      __ xchgq(i.InputRegister(0), i.MemoryOperand(1));
      break;
    }
    case kX64Word64AtomicCompareExchangeUint8: {
      __ lock();
      __ cmpxchgb(i.MemoryOperand(2), i.InputRegister(1));
      __ movzxbq(rax, rax);
      break;
    }
    case kX64Word64AtomicCompareExchangeUint16: {
      __ lock();
      __ cmpxchgw(i.MemoryOperand(2), i.InputRegister(1));
      __ movzxwq(rax, rax);
      break;
    }
    case kX64Word64AtomicCompareExchangeUint32: {
      __ lock();
      __ cmpxchgl(i.MemoryOperand(2), i.InputRegister(1));
      // Zero-extend the 32 bit value to 64 bit.
      __ movl(rax, rax);
      break;
    }
    case kX64Word64AtomicCompareExchangeUint64: {
      __ lock();
      __ cmpxchgq(i.MemoryOperand(2), i.InputRegister(1));
      break;
    }
#define ATOMIC64_BINOP_CASE(op, inst)              \
  case kX64Word64Atomic##op##Uint8:                \
    ASSEMBLE_ATOMIC64_BINOP(inst, movb, cmpxchgb); \
    __ movzxbq(rax, rax);                          \
    break;                                         \
  case kX64Word64Atomic##op##Uint16:               \
    ASSEMBLE_ATOMIC64_BINOP(inst, movw, cmpxchgw); \
    __ movzxwq(rax, rax);                          \
    break;                                         \
  case kX64Word64Atomic##op##Uint32:               \
    ASSEMBLE_ATOMIC64_BINOP(inst, movl, cmpxchgl); \
    break;                                         \
  case kX64Word64Atomic##op##Uint64:               \
    ASSEMBLE_ATOMIC64_BINOP(inst, movq, cmpxchgq); \
    break;
      ATOMIC64_BINOP_CASE(Add, addq)
      ATOMIC64_BINOP_CASE(Sub, subq)
      ATOMIC64_BINOP_CASE(And, andq)
      ATOMIC64_BINOP_CASE(Or, orq)
      ATOMIC64_BINOP_CASE(Xor, xorq)
#undef ATOMIC64_BINOP_CASE
    case kWord32AtomicLoadInt8:
    case kWord32AtomicLoadUint8:
    case kWord32AtomicLoadInt16:
    case kWord32AtomicLoadUint16:
    case kWord32AtomicLoadWord32:
    case kWord32AtomicStoreWord8:
    case kWord32AtomicStoreWord16:
    case kWord32AtomicStoreWord32:
      UNREACHABLE();  // Won't be generated by instruction selector.
      break;
  }
  return kSuccess;
}  // NOLadability/fn_size)

#undef ASSEMBLE_PINSR
#undef ASSEMBLE_UNOP
#undef ASSEMBLE_BINOP
#undef ASSEMBLE_COMPARE
#undef ASSEMBLE_MULT
#undef ASSEMBLE_SHIFT
#undef ASSEMBLE_MOVX
#undef ASSEMBLE_SSE_BINOP
#undef ASSEMBLE_SSE_UNOP
#undef ASSEMBLE_AVX_BINOP
#undef ASSEMBLE_IEEE754_BINOP
#undef ASSEMBLE_IEEE754_UNOP
#undef ASSEMBLE_ATOMIC_BINOP
#undef ASSEMBLE_ATOMIC64_BINOP
#undef ASSEMBLE_SIMD_INSTR
#undef ASSEMBLE_SIMD_IMM_INSTR
#undef ASSEMBLE_SIMD_PUNPCK_SHUFFLE
#undef ASSEMBLE_SIMD_IMM_SHUFFLE
#undef ASSEMBLE_SIMD_ALL_TRUE
#undef ASSEMBLE_SIMD_SHIFT

namespace {

Condition FlagsConditionToCondition(FlagsCondition condition) {
  switch (condition) {
    case kUnorderedEqual:
    case kEqual:
      return equal;
    case kUnorderedNotEqual:
    case kNotEqual:
      return not_equal;
    case kSignedLessThan:
      return less;
    case kSignedGreaterThanOrEqual:
      return greater_equal;
    case kSignedLessThanOrEqual:
      return less_equal;
    case kSignedGreaterThan:
      return greater;
    case kUnsignedLessThan:
      return below;
    case kUnsignedGreaterThanOrEqual:
      return above_equal;
    case kUnsignedLessThanOrEqual:
      return below_equal;
    case kUnsignedGreaterThan:
      return above;
    case kOverflow:
      return overflow;
    case kNotOverflow:
      return no_overflow;
    default:
      break;
  }
  UNREACHABLE();
}

}  // namespace

// Assembles branches after this instruction.
void CodeGenerator::AssembleArchBranch(Instruction* instr, BranchInfo* branch) {
  Label::Distance flabel_distance =
      branch->fallthru ? Label::kNear : Label::kFar;
  Label* tlabel = branch->true_label;
  Label* flabel = branch->false_label;
  if (branch->condition == kUnorderedEqual) {
    __ j(parity_even, flabel, flabel_distance);
  } else if (branch->condition == kUnorderedNotEqual) {
    __ j(parity_even, tlabel);
  }
  __ j(FlagsConditionToCondition(branch->condition), tlabel);

  if (!branch->fallthru) __ jmp(flabel, flabel_distance);
}

void CodeGenerator::AssembleBranchPoisoning(FlagsCondition condition,
                                            Instruction* instr) {
  // TODO(jarin) Handle float comparisons (kUnordered[Not]Equal).
  if (condition == kUnorderedEqual || condition == kUnorderedNotEqual) {
    return;
  }

  condition = NegateFlagsCondition(condition);
  __ movl(kScratchRegister, Immediate(0));
  __ cmovq(FlagsConditionToCondition(condition), kSpeculationPoisonRegister,
           kScratchRegister);
}

void CodeGenerator::AssembleArchDeoptBranch(Instruction* instr,
                                            BranchInfo* branch) {
  Label::Distance flabel_distance =
      branch->fallthru ? Label::kNear : Label::kFar;
  Label* tlabel = branch->true_label;
  Label* flabel = branch->false_label;
  Label nodeopt;
  if (branch->condition == kUnorderedEqual) {
    __ j(parity_even, flabel, flabel_distance);
  } else if (branch->condition == kUnorderedNotEqual) {
    __ j(parity_even, tlabel);
  }
  __ j(FlagsConditionToCondition(branch->condition), tlabel);

  if (FLAG_deopt_every_n_times > 0) {
    ExternalReference counter =
        ExternalReference::stress_deopt_count(isolate());

    __ pushfq();
    __ pushq(rax);
    __ load_rax(counter);
    __ decl(rax);
    __ j(not_zero, &nodeopt, Label::kNear);

    __ Set(rax, FLAG_deopt_every_n_times);
    __ store_rax(counter);
    __ popq(rax);
    __ popfq();
    __ jmp(tlabel);

    __ bind(&nodeopt);
    __ store_rax(counter);
    __ popq(rax);
    __ popfq();
  }

  if (!branch->fallthru) {
    __ jmp(flabel, flabel_distance);
  }
}

void CodeGenerator::AssembleArchJump(RpoNumber target) {
  if (!IsNextInAssemblyOrder(target)) __ jmp(GetLabel(target));
}

#if V8_ENABLE_WEBASSEMBLY
void CodeGenerator::AssembleArchTrap(Instruction* instr,
                                     FlagsCondition condition) {
  auto ool = zone()->New<WasmOutOfLineTrap>(this, instr);
  Label* tlabel = ool->entry();
  Label end;
  if (condition == kUnorderedEqual) {
    __ j(parity_even, &end, Label::kNear);
  } else if (condition == kUnorderedNotEqual) {
    __ j(parity_even, tlabel);
  }
  __ j(FlagsConditionToCondition(condition), tlabel);
  __ bind(&end);
}
#endif  // V8_ENABLE_WEBASSEMBLY

// Assembles boolean materializations after this instruction.
void CodeGenerator::AssembleArchBoolean(Instruction* instr,
                                        FlagsCondition condition) {
  X64OperandConverter i(this, instr);
  Label done;

  // Materialize a full 64-bit 1 or 0 value. The result register is always the
  // last output of the instruction.
  Label check;
  DCHECK_NE(0u, instr->OutputCount());
  Register reg = i.OutputRegister(instr->OutputCount() - 1);
  if (condition == kUnorderedEqual) {
    __ j(parity_odd, &check, Label::kNear);
    __ movl(reg, Immediate(0));
    __ jmp(&done, Label::kNear);
  } else if (condition == kUnorderedNotEqual) {
    __ j(parity_odd, &check, Label::kNear);
    __ movl(reg, Immediate(1));
    __ jmp(&done, Label::kNear);
  }
  __ bind(&check);
  __ setcc(FlagsConditionToCondition(condition), reg);
  __ movzxbl(reg, reg);
  __ bind(&done);
}

void CodeGenerator::AssembleArchBinarySearchSwitch(Instruction* instr) {
  X64OperandConverter i(this, instr);
  Register input = i.InputRegister(0);
  std::vector<std::pair<int32_t, Label*>> cases;
  for (size_t index = 2; index < instr->InputCount(); index += 2) {
    cases.push_back({i.InputInt32(index + 0), GetLabel(i.InputRpo(index + 1))});
  }
  AssembleArchBinarySearchSwitchRange(input, i.InputRpo(1), cases.data(),
                                      cases.data() + cases.size());
}

void CodeGenerator::AssembleArchTableSwitch(Instruction* instr) {
  X64OperandConverter i(this, instr);
  Register input = i.InputRegister(0);
  int32_t const case_count = static_cast<int32_t>(instr->InputCount() - 2);
  Label** cases = zone()->NewArray<Label*>(case_count);
  for (int32_t index = 0; index < case_count; ++index) {
    cases[index] = GetLabel(i.InputRpo(index + 2));
  }
  Label* const table = AddJumpTable(cases, case_count);
  __ cmpl(input, Immediate(case_count));
  __ j(above_equal, GetLabel(i.InputRpo(1)));
  __ leaq(kScratchRegister, Operand(table));
  __ jmp(Operand(kScratchRegister, input, times_8, 0));
}

void CodeGenerator::AssembleArchSelect(Instruction* instr,
                                       FlagsCondition condition) {
  UNIMPLEMENTED();
}

namespace {

static const int kQuadWordSize = 16;

}  // namespace

void CodeGenerator::FinishFrame(Frame* frame) {
  CallDescriptor* call_descriptor = linkage()->GetIncomingDescriptor();

  const RegList saves_fp = call_descriptor->CalleeSavedFPRegisters();
  if (saves_fp != 0) {
    frame->AlignSavedCalleeRegisterSlots();
    if (saves_fp != 0) {  // Save callee-saved XMM registers.
      const uint32_t saves_fp_count = base::bits::CountPopulation(saves_fp);
      frame->AllocateSavedCalleeRegisterSlots(
          saves_fp_count * (kQuadWordSize / kSystemPointerSize));
    }
  }
  const RegList saves = call_descriptor->CalleeSavedRegisters();
  if (saves != 0) {  // Save callee-saved registers.
    int count = 0;
    for (int i = Register::kNumRegisters - 1; i >= 0; i--) {
      if (((1 << i) & saves)) {
        ++count;
      }
    }
    frame->AllocateSavedCalleeRegisterSlots(count);
  }
}

void CodeGenerator::AssembleConstructFrame() {
  auto call_descriptor = linkage()->GetIncomingDescriptor();
  if (frame_access_state()->has_frame()) {
    int pc_base = __ pc_offset();

    if (call_descriptor->IsCFunctionCall()) {
      __ pushq(rbp);
      __ movq(rbp, rsp);
#if V8_ENABLE_WEBASSEMBLY
      if (info()->GetOutputStackFrameType() == StackFrame::C_WASM_ENTRY) {
        __ Push(Immediate(StackFrame::TypeToMarker(StackFrame::C_WASM_ENTRY)));
        // Reserve stack space for saving the c_entry_fp later.
        __ AllocateStackSpace(kSystemPointerSize);
      }
#endif  // V8_ENABLE_WEBASSEMBLY
    } else if (call_descriptor->IsJSFunctionCall()) {
      __ Prologue();
    } else {
      __ StubPrologue(info()->GetOutputStackFrameType());
#if V8_ENABLE_WEBASSEMBLY
      if (call_descriptor->IsWasmFunctionCall()) {
        __ pushq(kWasmInstanceRegister);
      } else if (call_descriptor->IsWasmImportWrapper() ||
                 call_descriptor->IsWasmCapiFunction()) {
        // Wasm import wrappers are passed a tuple in the place of the instance.
        // Unpack the tuple into the instance and the target callable.
        // This must be done here in the codegen because it cannot be expressed
        // properly in the graph.
        __ LoadTaggedPointerField(
            kJSFunctionRegister,
            FieldOperand(kWasmInstanceRegister, Tuple2::kValue2Offset));
        __ LoadTaggedPointerField(
            kWasmInstanceRegister,
            FieldOperand(kWasmInstanceRegister, Tuple2::kValue1Offset));
        __ pushq(kWasmInstanceRegister);
        if (call_descriptor->IsWasmCapiFunction()) {
          // Reserve space for saving the PC later.
          __ AllocateStackSpace(kSystemPointerSize);
        }
      }
#endif  // V8_ENABLE_WEBASSEMBLY
    }

    unwinding_info_writer_.MarkFrameConstructed(pc_base);
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
    required_slots -= static_cast<int>(osr_helper()->UnoptimizedFrameSlots());
    ResetSpeculationPoison();
  }

  const RegList saves = call_descriptor->CalleeSavedRegisters();
  const RegList saves_fp = call_descriptor->CalleeSavedFPRegisters();

  if (required_slots > 0) {
    DCHECK(frame_access_state()->has_frame());
#if V8_ENABLE_WEBASSEMBLY
    if (info()->IsWasm() && required_slots > 128) {
      // For WebAssembly functions with big frames we have to do the stack
      // overflow check before we construct the frame. Otherwise we may not
      // have enough space on the stack to call the runtime for the stack
      // overflow.
      Label done;

      // If the frame is bigger than the stack, we throw the stack overflow
      // exception unconditionally. Thereby we can avoid the integer overflow
      // check in the condition code.
      if (required_slots * kSystemPointerSize < FLAG_stack_size * 1024) {
        __ movq(kScratchRegister,
                FieldOperand(kWasmInstanceRegister,
                             WasmInstanceObject::kRealStackLimitAddressOffset));
        __ movq(kScratchRegister, Operand(kScratchRegister, 0));
        __ addq(kScratchRegister,
                Immediate(required_slots * kSystemPointerSize));
        __ cmpq(rsp, kScratchRegister);
        __ j(above_equal, &done, Label::kNear);
      }

      __ near_call(wasm::WasmCode::kWasmStackOverflow,
                   RelocInfo::WASM_STUB_CALL);
      ReferenceMap* reference_map = zone()->New<ReferenceMap>(zone());
      RecordSafepoint(reference_map);
      __ AssertUnreachable(AbortReason::kUnexpectedReturnFromWasmTrap);
      __ bind(&done);
    }
#endif  // V8_ENABLE_WEBASSEMBLY

    // Skip callee-saved and return slots, which are created below.
    required_slots -= base::bits::CountPopulation(saves);
    required_slots -= base::bits::CountPopulation(saves_fp) *
                      (kQuadWordSize / kSystemPointerSize);
    required_slots -= frame()->GetReturnSlotCount();
    if (required_slots > 0) {
      __ AllocateStackSpace(required_slots * kSystemPointerSize);
    }
  }

  if (saves_fp != 0) {  // Save callee-saved XMM registers.
    const uint32_t saves_fp_count = base::bits::CountPopulation(saves_fp);
    const int stack_size = saves_fp_count * kQuadWordSize;
    // Adjust the stack pointer.
    __ AllocateStackSpace(stack_size);
    // Store the registers on the stack.
    int slot_idx = 0;
    for (int i = 0; i < XMMRegister::kNumRegisters; i++) {
      if (!((1 << i) & saves_fp)) continue;
      __ Movdqu(Operand(rsp, kQuadWordSize * slot_idx),
                XMMRegister::from_code(i));
      slot_idx++;
    }
  }

  if (saves != 0) {  // Save callee-saved registers.
    for (int i = Register::kNumRegisters - 1; i >= 0; i--) {
      if (!((1 << i) & saves)) continue;
      __ pushq(Register::from_code(i));
    }
  }

  // Allocate return slots (located after callee-saved).
  if (frame()->GetReturnSlotCount() > 0) {
    __ AllocateStackSpace(frame()->GetReturnSlotCount() * kSystemPointerSize);
  }
}

void CodeGenerator::AssembleReturn(InstructionOperand* additional_pop_count) {
  auto call_descriptor = linkage()->GetIncomingDescriptor();

  // Restore registers.
  const RegList saves = call_descriptor->CalleeSavedRegisters();
  if (saves != 0) {
    const int returns = frame()->GetReturnSlotCount();
    if (returns != 0) {
      __ addq(rsp, Immediate(returns * kSystemPointerSize));
    }
    for (int i = 0; i < Register::kNumRegisters; i++) {
      if (!((1 << i) & saves)) continue;
      __ popq(Register::from_code(i));
    }
  }
  const RegList saves_fp = call_descriptor->CalleeSavedFPRegisters();
  if (saves_fp != 0) {
    const uint32_t saves_fp_count = base::bits::CountPopulation(saves_fp);
    const int stack_size = saves_fp_count * kQuadWordSize;
    // Load the registers from the stack.
    int slot_idx = 0;
    for (int i = 0; i < XMMRegister::kNumRegisters; i++) {
      if (!((1 << i) & saves_fp)) continue;
      __ Movdqu(XMMRegister::from_code(i),
                Operand(rsp, kQuadWordSize * slot_idx));
      slot_idx++;
    }
    // Adjust the stack pointer.
    __ addq(rsp, Immediate(stack_size));
  }

  unwinding_info_writer_.MarkBlockWillExit();

  // We might need rcx and r10 for scratch.
  DCHECK_EQ(0u, call_descriptor->CalleeSavedRegisters() & rcx.bit());
  DCHECK_EQ(0u, call_descriptor->CalleeSavedRegisters() & r10.bit());
  X64OperandConverter g(this, nullptr);
  int parameter_slots = static_cast<int>(call_descriptor->ParameterSlotCount());

  // {aditional_pop_count} is only greater than zero if {parameter_slots = 0}.
  // Check RawMachineAssembler::PopAndReturn.
  if (parameter_slots != 0) {
    if (additional_pop_count->IsImmediate()) {
      DCHECK_EQ(g.ToConstant(additional_pop_count).ToInt32(), 0);
    } else if (__ emit_debug_code()) {
      __ cmpq(g.ToRegister(additional_pop_count), Immediate(0));
      __ Assert(equal, AbortReason::kUnexpectedAdditionalPopValue);
    }
  }

  Register argc_reg = rcx;
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
    if (additional_pop_count->IsImmediate() &&
        g.ToConstant(additional_pop_count).ToInt32() == 0) {
      // Canonicalize JSFunction return sites for now.
      if (return_label_.is_bound()) {
        __ jmp(&return_label_);
        return;
      } else {
        __ bind(&return_label_);
      }
    }
    if (drop_jsargs) {
      // Get the actual argument count.
      __ movq(argc_reg, Operand(rbp, StandardFrameConstants::kArgCOffset));
    }
    AssembleDeconstructFrame();
  }

  if (drop_jsargs) {
    // We must pop all arguments from the stack (including the receiver). This
    // number of arguments is given by max(1 + argc_reg, parameter_slots).
    int parameter_slots_without_receiver =
        parameter_slots - 1;  // Exclude the receiver to simplify the
                              // computation. We'll account for it at the end.
    Label mismatch_return;
    Register scratch_reg = r10;
    DCHECK_NE(argc_reg, scratch_reg);
    __ cmpq(argc_reg, Immediate(parameter_slots_without_receiver));
    __ j(greater, &mismatch_return, Label::kNear);
    __ Ret(parameter_slots * kSystemPointerSize, scratch_reg);
    __ bind(&mismatch_return);
    __ PopReturnAddressTo(scratch_reg);
    __ leaq(rsp, Operand(rsp, argc_reg, times_system_pointer_size,
                         kSystemPointerSize));  // Also pop the receiver.
    // We use a return instead of a jump for better return address prediction.
    __ PushReturnAddressFrom(scratch_reg);
    __ Ret();
  } else if (additional_pop_count->IsImmediate()) {
    Register scratch_reg = r10;
    int additional_count = g.ToConstant(additional_pop_count).ToInt32();
    size_t pop_size = (parameter_slots + additional_count) * kSystemPointerSize;
    CHECK_LE(pop_size, static_cast<size_t>(std::numeric_limits<int>::max()));
    __ Ret(static_cast<int>(pop_size), scratch_reg);
  } else {
    Register pop_reg = g.ToRegister(additional_pop_count);
    Register scratch_reg = pop_reg == r10 ? rcx : r10;
    int pop_size = static_cast<int>(parameter_slots * kSystemPointerSize);
    __ PopReturnAddressTo(scratch_reg);
    __ leaq(rsp, Operand(rsp, pop_reg, times_system_pointer_size,
                         static_cast<int>(pop_size)));
    __ PushReturnAddressFrom(scratch_reg);
    __ Ret();
  }
}

void CodeGenerator::FinishCode() { tasm()->PatchConstPool(); }

void CodeGenerator::PrepareForDeoptimizationExits(
    ZoneDeque<DeoptimizationExit*>* exits) {}

void CodeGenerator::IncrementStackAccessCounter(
    InstructionOperand* source, InstructionOperand* destination) {
  DCHECK(FLAG_trace_turbo_stack_accesses);
  if (!info()->IsOptimizing()) {
#if V8_ENABLE_WEBASSEMBLY
    if (!info()->IsWasm()) return;
#else
    return;
#endif  // V8_ENABLE_WEBASSEMBLY
  }
  DCHECK_NOT_NULL(debug_name_);
  auto IncrementCounter = [&](ExternalReference counter) {
    __ incl(__ ExternalReferenceAsOperand(counter));
  };
  if (source->IsAnyStackSlot()) {
    IncrementCounter(
        ExternalReference::address_of_load_from_stack_count(debug_name_));
  }
  if (destination->IsAnyStackSlot()) {
    IncrementCounter(
        ExternalReference::address_of_store_to_stack_count(debug_name_));
  }
}

void CodeGenerator::AssembleMove(InstructionOperand* source,
                                 InstructionOperand* destination) {
  X64OperandConverter g(this, nullptr);
  // Helper function to write the given constant to the dst register.
  auto MoveConstantToRegister = [&](Register dst, Constant src) {
    switch (src.type()) {
      case Constant::kInt32: {
        if (RelocInfo::IsWasmReference(src.rmode())) {
          __ movq(dst, Immediate64(src.ToInt64(), src.rmode()));
        } else {
          int32_t value = src.ToInt32();
          if (value == 0) {
            __ xorl(dst, dst);
          } else {
            __ movl(dst, Immediate(value));
          }
        }
        break;
      }
      case Constant::kInt64:
        if (RelocInfo::IsWasmReference(src.rmode())) {
          __ movq(dst, Immediate64(src.ToInt64(), src.rmode()));
        } else {
          __ Set(dst, src.ToInt64());
        }
        break;
      case Constant::kFloat32:
        __ MoveNumber(dst, src.ToFloat32());
        break;
      case Constant::kFloat64:
        __ MoveNumber(dst, src.ToFloat64().value());
        break;
      case Constant::kExternalReference:
        __ Move(dst, src.ToExternalReference());
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
          __ Move(dst, src_object, RelocInfo::COMPRESSED_EMBEDDED_OBJECT);
        }
        break;
      }
      case Constant::kDelayedStringConstant: {
        const StringConstantBase* src_constant = src.ToDelayedStringConstant();
        __ MoveStringConstant(dst, src_constant);
        break;
      }
      case Constant::kRpoNumber:
        UNREACHABLE();  // TODO(dcarney): load of labels on x64.
        break;
    }
  };
  // Helper function to write the given constant to the stack.
  auto MoveConstantToSlot = [&](Operand dst, Constant src) {
    if (!RelocInfo::IsWasmReference(src.rmode())) {
      switch (src.type()) {
        case Constant::kInt32:
          __ movq(dst, Immediate(src.ToInt32()));
          return;
        case Constant::kInt64:
          __ Set(dst, src.ToInt64());
          return;
        default:
          break;
      }
    }
    MoveConstantToRegister(kScratchRegister, src);
    __ movq(dst, kScratchRegister);
  };

  if (FLAG_trace_turbo_stack_accesses) {
    IncrementStackAccessCounter(source, destination);
  }

  // Dispatch on the source and destination operand kinds.
  switch (MoveType::InferMove(source, destination)) {
    case MoveType::kRegisterToRegister:
      if (source->IsRegister()) {
        __ movq(g.ToRegister(destination), g.ToRegister(source));
      } else {
        DCHECK(source->IsFPRegister());
        __ Movapd(g.ToDoubleRegister(destination), g.ToDoubleRegister(source));
      }
      return;
    case MoveType::kRegisterToStack: {
      Operand dst = g.ToOperand(destination);
      if (source->IsRegister()) {
        __ movq(dst, g.ToRegister(source));
      } else {
        DCHECK(source->IsFPRegister());
        XMMRegister src = g.ToDoubleRegister(source);
        MachineRepresentation rep =
            LocationOperand::cast(source)->representation();
        if (rep != MachineRepresentation::kSimd128) {
          __ Movsd(dst, src);
        } else {
          __ Movups(dst, src);
        }
      }
      return;
    }
    case MoveType::kStackToRegister: {
      Operand src = g.ToOperand(source);
      if (source->IsStackSlot()) {
        __ movq(g.ToRegister(destination), src);
      } else {
        DCHECK(source->IsFPStackSlot());
        XMMRegister dst = g.ToDoubleRegister(destination);
        MachineRepresentation rep =
            LocationOperand::cast(source)->representation();
        if (rep != MachineRepresentation::kSimd128) {
          __ Movsd(dst, src);
        } else {
          __ Movups(dst, src);
        }
      }
      return;
    }
    case MoveType::kStackToStack: {
      Operand src = g.ToOperand(source);
      Operand dst = g.ToOperand(destination);
      if (source->IsStackSlot()) {
        // Spill on demand to use a temporary register for memory-to-memory
        // moves.
        __ movq(kScratchRegister, src);
        __ movq(dst, kScratchRegister);
      } else {
        MachineRepresentation rep =
            LocationOperand::cast(source)->representation();
        if (rep != MachineRepresentation::kSimd128) {
          __ Movsd(kScratchDoubleReg, src);
          __ Movsd(dst, kScratchDoubleReg);
        } else {
          DCHECK(source->IsSimd128StackSlot());
          __ Movups(kScratchDoubleReg, src);
          __ Movups(dst, kScratchDoubleReg);
        }
      }
      return;
    }
    case MoveType::kConstantToRegister: {
      Constant src = g.ToConstant(source);
      if (destination->IsRegister()) {
        MoveConstantToRegister(g.ToRegister(destination), src);
      } else {
        DCHECK(destination->IsFPRegister());
        XMMRegister dst = g.ToDoubleRegister(destination);
        if (src.type() == Constant::kFloat32) {
          // TODO(turbofan): Can we do better here?
          __ Move(dst, bit_cast<uint32_t>(src.ToFloat32()));
        } else {
          DCHECK_EQ(src.type(), Constant::kFloat64);
          __ Move(dst, src.ToFloat64().AsUint64());
        }
      }
      return;
    }
    case MoveType::kConstantToStack: {
      Constant src = g.ToConstant(source);
      Operand dst = g.ToOperand(destination);
      if (destination->IsStackSlot()) {
        MoveConstantToSlot(dst, src);
      } else {
        DCHECK(destination->IsFPStackSlot());
        if (src.type() == Constant::kFloat32) {
          __ movl(dst, Immediate(bit_cast<uint32_t>(src.ToFloat32())));
        } else {
          DCHECK_EQ(src.type(), Constant::kFloat64);
          __ movq(kScratchRegister, src.ToFloat64().AsUint64());
          __ movq(dst, kScratchRegister);
        }
      }
      return;
    }
  }
  UNREACHABLE();
}

void CodeGenerator::AssembleSwap(InstructionOperand* source,
                                 InstructionOperand* destination) {
  if (FLAG_trace_turbo_stack_accesses) {
    IncrementStackAccessCounter(source, destination);
    IncrementStackAccessCounter(destination, source);
  }

  X64OperandConverter g(this, nullptr);
  // Dispatch on the source and destination operand kinds.  Not all
  // combinations are possible.
  switch (MoveType::InferSwap(source, destination)) {
    case MoveType::kRegisterToRegister: {
      if (source->IsRegister()) {
        Register src = g.ToRegister(source);
        Register dst = g.ToRegister(destination);
        __ movq(kScratchRegister, src);
        __ movq(src, dst);
        __ movq(dst, kScratchRegister);
      } else {
        DCHECK(source->IsFPRegister());
        XMMRegister src = g.ToDoubleRegister(source);
        XMMRegister dst = g.ToDoubleRegister(destination);
        __ Movapd(kScratchDoubleReg, src);
        __ Movapd(src, dst);
        __ Movapd(dst, kScratchDoubleReg);
      }
      return;
    }
    case MoveType::kRegisterToStack: {
      if (source->IsRegister()) {
        Register src = g.ToRegister(source);
        Operand dst = g.ToOperand(destination);
        __ movq(kScratchRegister, src);
        __ movq(src, dst);
        __ movq(dst, kScratchRegister);
      } else {
        DCHECK(source->IsFPRegister());
        XMMRegister src = g.ToDoubleRegister(source);
        Operand dst = g.ToOperand(destination);
        MachineRepresentation rep =
            LocationOperand::cast(source)->representation();
        if (rep != MachineRepresentation::kSimd128) {
          __ Movsd(kScratchDoubleReg, src);
          __ Movsd(src, dst);
          __ Movsd(dst, kScratchDoubleReg);
        } else {
          __ Movups(kScratchDoubleReg, src);
          __ Movups(src, dst);
          __ Movups(dst, kScratchDoubleReg);
        }
      }
      return;
    }
    case MoveType::kStackToStack: {
      Operand src = g.ToOperand(source);
      Operand dst = g.ToOperand(destination);
      MachineRepresentation rep =
          LocationOperand::cast(source)->representation();
      if (rep != MachineRepresentation::kSimd128) {
        Register tmp = kScratchRegister;
        __ movq(tmp, dst);
        __ pushq(src);  // Then use stack to copy src to destination.
        unwinding_info_writer_.MaybeIncreaseBaseOffsetAt(__ pc_offset(),
                                                         kSystemPointerSize);
        __ popq(dst);
        unwinding_info_writer_.MaybeIncreaseBaseOffsetAt(__ pc_offset(),
                                                         -kSystemPointerSize);
        __ movq(src, tmp);
      } else {
        // Without AVX, misaligned reads and writes will trap. Move using the
        // stack, in two parts.
        __ movups(kScratchDoubleReg, dst);  // Save dst in scratch register.
        __ pushq(src);  // Then use stack to copy src to destination.
        unwinding_info_writer_.MaybeIncreaseBaseOffsetAt(__ pc_offset(),
                                                         kSystemPointerSize);
        __ popq(dst);
        unwinding_info_writer_.MaybeIncreaseBaseOffsetAt(__ pc_offset(),
                                                         -kSystemPointerSize);
        __ pushq(g.ToOperand(source, kSystemPointerSize));
        unwinding_info_writer_.MaybeIncreaseBaseOffsetAt(__ pc_offset(),
                                                         kSystemPointerSize);
        __ popq(g.ToOperand(destination, kSystemPointerSize));
        unwinding_info_writer_.MaybeIncreaseBaseOffsetAt(__ pc_offset(),
                                                         -kSystemPointerSize);
        __ movups(src, kScratchDoubleReg);
      }
      return;
    }
    default:
      UNREACHABLE();
  }
}

void CodeGenerator::AssembleJumpTable(Label** targets, size_t target_count) {
  for (size_t index = 0; index < target_count; ++index) {
    __ dq(targets[index]);
  }
}

#undef __

}  // namespace compiler
}  // namespace internal
}  // namespace v8
