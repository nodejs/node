// Copyright 2013 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/code-generator.h"

#include <limits>

#include "src/compiler/code-generator-impl.h"
#include "src/compiler/gap-resolver.h"
#include "src/compiler/node-matchers.h"
#include "src/compiler/osr.h"
#include "src/heap/heap-inl.h"
#include "src/optimized-compilation-info.h"
#include "src/wasm/wasm-code-manager.h"
#include "src/wasm/wasm-objects.h"
#include "src/x64/assembler-x64.h"
#include "src/x64/macro-assembler-x64.h"

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

bool HasImmediateInput(Instruction* instr, size_t index) {
  return instr->InputAt(index)->IsImmediate();
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
        stub_mode_(stub_mode),
        unwinding_info_writer_(unwinding_info_writer),
        isolate_(gen->isolate()),
        zone_(gen->zone()) {}

  void Generate() final {
    __ subp(rsp, Immediate(kDoubleSize));
    unwinding_info_writer_->MaybeIncreaseBaseOffsetAt(__ pc_offset(),
                                                      kDoubleSize);
    __ Movsd(MemOperand(rsp, 0), input_);
    if (stub_mode_ == StubCallMode::kCallWasmRuntimeStub) {
      // A direct call to a wasm runtime stub defined in this module.
      // Just encode the stub index. This will be patched at relocation.
      __ near_call(wasm::WasmCode::kDoubleToI, RelocInfo::WASM_STUB_CALL);
    } else {
      __ Call(BUILTIN_CODE(isolate_, DoubleToI), RelocInfo::CODE_TARGET);
    }
    __ movl(result_, MemOperand(rsp, 0));
    __ addp(rsp, Immediate(kDoubleSize));
    unwinding_info_writer_->MaybeIncreaseBaseOffsetAt(__ pc_offset(),
                                                      -kDoubleSize);
  }

 private:
  Register const result_;
  XMMRegister const input_;
  StubCallMode stub_mode_;
  UnwindingInfoWriter* const unwinding_info_writer_;
  Isolate* isolate_;
  Zone* zone_;
};


class OutOfLineRecordWrite final : public OutOfLineCode {
 public:
  OutOfLineRecordWrite(CodeGenerator* gen, Register object, Operand operand,
                       Register value, Register scratch0, Register scratch1,
                       RecordWriteMode mode)
      : OutOfLineCode(gen),
        object_(object),
        operand_(operand),
        value_(value),
        scratch0_(scratch0),
        scratch1_(scratch1),
        mode_(mode),
        zone_(gen->zone()) {}

  void Generate() final {
    if (mode_ > RecordWriteMode::kValueIsPointer) {
      __ JumpIfSmi(value_, exit());
    }
    __ CheckPageFlag(value_, scratch0_,
                     MemoryChunk::kPointersToHereAreInterestingMask, zero,
                     exit());
    __ leap(scratch1_, operand_);

    RememberedSetAction const remembered_set_action =
        mode_ > RecordWriteMode::kValueIsMap ? EMIT_REMEMBERED_SET
                                             : OMIT_REMEMBERED_SET;
    SaveFPRegsMode const save_fp_mode =
        frame()->DidAllocateDoubleRegisters() ? kSaveFPRegs : kDontSaveFPRegs;

    __ CallRecordWriteStub(object_, scratch1_, remembered_set_action,
                           save_fp_mode);
  }

 private:
  Register const object_;
  Operand const operand_;
  Register const value_;
  Register const scratch0_;
  Register const scratch1_;
  RecordWriteMode const mode_;
  Zone* zone_;
};

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
      __ LeaveFrame(StackFrame::WASM_COMPILED);
      auto call_descriptor = gen_->linkage()->GetIncomingDescriptor();
      size_t pop_size = call_descriptor->StackParameterCount() * kPointerSize;
      // Use rcx as a scratch register, we return anyways immediately.
      __ Ret(static_cast<int>(pop_size), rcx);
    } else {
      gen_->AssembleSourcePosition(instr_);
      // A direct call to a wasm runtime stub defined in this module.
      // Just encode the stub index. This will be patched at relocation.
      __ near_call(static_cast<Address>(trap_id), RelocInfo::WASM_STUB_CALL);
      ReferenceMap* reference_map =
          new (gen_->zone()) ReferenceMap(gen_->zone());
      gen_->RecordSafepoint(reference_map, Safepoint::kSimple, 0,
                            Safepoint::kNoLazyDeopt);
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
    gen_->AddProtectedInstructionLanding(pc_, __ pc_offset());
    GenerateWithTrapId(TrapId::kTrapMemOutOfBounds);
  }

 private:
  int pc_;
};

void EmitOOLTrapIfNeeded(Zone* zone, CodeGenerator* codegen,
                         InstructionCode opcode, Instruction* instr,
                         X64OperandConverter& i, int pc) {
  const MemoryAccessMode access_mode =
      static_cast<MemoryAccessMode>(MiscField::decode(opcode));
  if (access_mode == kMemoryAccessProtected) {
    new (zone) WasmProtectedInstructionTrap(codegen, pc, instr);
  }
}

void EmitWordLoadPoisoningIfNeeded(CodeGenerator* codegen,
                                   InstructionCode opcode, Instruction* instr,
                                   X64OperandConverter& i) {
  const MemoryAccessMode access_mode =
      static_cast<MemoryAccessMode>(MiscField::decode(opcode));
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

#define ASSEMBLE_BINOP(asm_instr)                                     \
  do {                                                                \
    if (AddressingModeField::decode(instr->opcode()) != kMode_None) { \
      size_t index = 1;                                               \
      Operand right = i.MemoryOperand(&index);                        \
      __ asm_instr(i.InputRegister(0), right);                        \
    } else {                                                          \
      if (HasImmediateInput(instr, 1)) {                              \
        if (instr->InputAt(0)->IsRegister()) {                        \
          __ asm_instr(i.InputRegister(0), i.InputImmediate(1));      \
        } else {                                                      \
          __ asm_instr(i.InputOperand(0), i.InputImmediate(1));       \
        }                                                             \
      } else {                                                        \
        if (instr->InputAt(1)->IsRegister()) {                        \
          __ asm_instr(i.InputRegister(0), i.InputRegister(1));       \
        } else {                                                      \
          __ asm_instr(i.InputRegister(0), i.InputOperand(1));        \
        }                                                             \
      }                                                               \
    }                                                                 \
  } while (false)

#define ASSEMBLE_COMPARE(asm_instr)                                   \
  do {                                                                \
    if (AddressingModeField::decode(instr->opcode()) != kMode_None) { \
      size_t index = 0;                                               \
      Operand left = i.MemoryOperand(&index);                         \
      if (HasImmediateInput(instr, index)) {                          \
        __ asm_instr(left, i.InputImmediate(index));                  \
      } else {                                                        \
        __ asm_instr(left, i.InputRegister(index));                   \
      }                                                               \
    } else {                                                          \
      if (HasImmediateInput(instr, 1)) {                              \
        if (instr->InputAt(0)->IsRegister()) {                        \
          __ asm_instr(i.InputRegister(0), i.InputImmediate(1));      \
        } else {                                                      \
          __ asm_instr(i.InputOperand(0), i.InputImmediate(1));       \
        }                                                             \
      } else {                                                        \
        if (instr->InputAt(1)->IsRegister()) {                        \
          __ asm_instr(i.InputRegister(0), i.InputRegister(1));       \
        } else {                                                      \
          __ asm_instr(i.InputRegister(0), i.InputOperand(1));        \
        }                                                             \
      }                                                               \
    }                                                                 \
  } while (false)

#define ASSEMBLE_MULT(asm_instr)                              \
  do {                                                        \
    if (HasImmediateInput(instr, 1)) {                        \
      if (instr->InputAt(0)->IsRegister()) {                  \
        __ asm_instr(i.OutputRegister(), i.InputRegister(0),  \
                     i.InputImmediate(1));                    \
      } else {                                                \
        __ asm_instr(i.OutputRegister(), i.InputOperand(0),   \
                     i.InputImmediate(1));                    \
      }                                                       \
    } else {                                                  \
      if (instr->InputAt(1)->IsRegister()) {                  \
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
    if (instr->addressing_mode() != kMode_None) {           \
      __ asm_instr(i.OutputRegister(), i.MemoryOperand());  \
    } else if (instr->InputAt(0)->IsRegister()) {           \
      __ asm_instr(i.OutputRegister(), i.InputRegister(0)); \
    } else {                                                \
      __ asm_instr(i.OutputRegister(), i.InputOperand(0));  \
    }                                                       \
  } while (false)

#define ASSEMBLE_SSE_BINOP(asm_instr)                                   \
  do {                                                                  \
    if (instr->InputAt(1)->IsFPRegister()) {                            \
      __ asm_instr(i.InputDoubleRegister(0), i.InputDoubleRegister(1)); \
    } else {                                                            \
      __ asm_instr(i.InputDoubleRegister(0), i.InputOperand(1));        \
    }                                                                   \
  } while (false)

#define ASSEMBLE_SSE_UNOP(asm_instr)                                    \
  do {                                                                  \
    if (instr->InputAt(0)->IsFPRegister()) {                            \
      __ asm_instr(i.OutputDoubleRegister(), i.InputDoubleRegister(0)); \
    } else {                                                            \
      __ asm_instr(i.OutputDoubleRegister(), i.InputOperand(0));        \
    }                                                                   \
  } while (false)

#define ASSEMBLE_AVX_BINOP(asm_instr)                                  \
  do {                                                                 \
    CpuFeatureScope avx_scope(tasm(), AVX);                            \
    if (instr->InputAt(1)->IsFPRegister()) {                           \
      __ asm_instr(i.OutputDoubleRegister(), i.InputDoubleRegister(0), \
                   i.InputDoubleRegister(1));                          \
    } else {                                                           \
      __ asm_instr(i.OutputDoubleRegister(), i.InputDoubleRegister(0), \
                   i.InputOperand(1));                                 \
    }                                                                  \
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

void CodeGenerator::AssemblePopArgumentsAdaptorFrame(Register args_reg,
                                                     Register scratch1,
                                                     Register scratch2,
                                                     Register scratch3) {
  DCHECK(!AreAliased(args_reg, scratch1, scratch2, scratch3));
  Label done;

  // Check if current frame is an arguments adaptor frame.
  __ cmpp(Operand(rbp, CommonFrameConstants::kContextOrFrameTypeOffset),
          Immediate(StackFrame::TypeToMarker(StackFrame::ARGUMENTS_ADAPTOR)));
  __ j(not_equal, &done, Label::kNear);

  // Load arguments count from current arguments adaptor frame (note, it
  // does not include receiver).
  Register caller_args_count_reg = scratch1;
  __ SmiUntag(caller_args_count_reg,
              Operand(rbp, ArgumentsAdaptorFrameConstants::kLengthOffset));

  ParameterCount callee_args_count(args_reg);
  __ PrepareForTailCall(callee_args_count, caller_args_count_reg, scratch2,
                        scratch3);
  __ bind(&done);
}

namespace {

void AdjustStackPointerForTailCall(Assembler* assembler,
                                   FrameAccessState* state,
                                   int new_slot_above_sp,
                                   bool allow_shrinkage = true) {
  int current_sp_offset = state->GetSPToFPSlotCount() +
                          StandardFrameConstants::kFixedSlotCountAboveFp;
  int stack_slot_delta = new_slot_above_sp - current_sp_offset;
  if (stack_slot_delta > 0) {
    assembler->subq(rsp, Immediate(stack_slot_delta * kPointerSize));
    state->IncreaseSPDelta(stack_slot_delta);
  } else if (allow_shrinkage && stack_slot_delta < 0) {
    assembler->addq(rsp, Immediate(-stack_slot_delta * kPointerSize));
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
    X64OperandConverter g(this, instr);
    for (auto move : pushes) {
      LocationOperand destination_location(
          LocationOperand::cast(move->destination()));
      InstructionOperand source(move->source());
      AdjustStackPointerForTailCall(tasm(), frame_access_state(),
                                    destination_location.index());
      if (source.IsStackSlot()) {
        LocationOperand source_location(LocationOperand::cast(source));
        __ Push(g.SlotToOperand(source_location.index()));
      } else if (source.IsRegister()) {
        LocationOperand source_location(LocationOperand::cast(source));
        __ Push(source_location.GetRegister());
      } else if (source.IsImmediate()) {
        __ Push(Immediate(ImmediateOperand::cast(source).inline_value()));
      } else {
        // Pushes of non-scalar data types is not supported.
        UNIMPLEMENTED();
      }
      frame_access_state()->IncreaseSPDelta(1);
      move->Eliminate();
    }
  }
  AdjustStackPointerForTailCall(tasm(), frame_access_state(),
                                first_unused_stack_slot, false);
}

void CodeGenerator::AssembleTailCallAfterGap(Instruction* instr,
                                             int first_unused_stack_slot) {
  AdjustStackPointerForTailCall(tasm(), frame_access_state(),
                                first_unused_stack_slot);
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
  __ movp(rbx, Operand(kJavaScriptCallCodeStartRegister, offset));
  __ testl(FieldOperand(rbx, CodeDataContainer::kKindSpecificFlagsOffset),
           Immediate(1 << Code::kMarkedForDeoptimizationBit));
  // Ensure we're not serializing (otherwise we'd need to use an indirection to
  // access the builtin below).
  DCHECK(!isolate()->ShouldLoadConstantsFromRootList());
  Handle<Code> code = isolate()->builtins()->builtin_handle(
      Builtins::kCompileLazyDeoptimizedCode);
  __ j(not_zero, code, RelocInfo::CODE_TARGET);
}

void CodeGenerator::GenerateSpeculationPoisonFromCodeStartRegister() {
  // Set a mask which has all bits set in the normal case, but has all
  // bits cleared if we are speculatively executing the wrong PC.
  __ ComputeCodeStartAddress(rbx);
  __ xorq(kSpeculationPoisonRegister, kSpeculationPoisonRegister);
  __ cmpp(kJavaScriptCallCodeStartRegister, rbx);
  __ movp(rbx, Immediate(-1));
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
            HasCallDescriptorFlag(instr, CallDescriptor::kFixedTargetRegister),
            reg == kJavaScriptCallCodeStartRegister);
        __ addp(reg, Immediate(Code::kHeaderSize - kHeapObjectTag));
        if (HasCallDescriptorFlag(instr, CallDescriptor::kRetpoline)) {
          __ RetpolineCall(reg);
        } else {
          __ call(reg);
        }
      }
      RecordCallPosition(instr);
      frame_access_state()->ClearSPDelta();
      break;
    }
    case kArchCallWasmFunction: {
      if (HasImmediateInput(instr, 0)) {
        Constant constant = i.ToConstant(instr->InputAt(0));
        Address wasm_code = static_cast<Address>(constant.ToInt64());
        if (DetermineStubCallMode() == StubCallMode::kCallWasmRuntimeStub) {
          __ near_call(wasm_code, constant.rmode());
        } else {
          if (HasCallDescriptorFlag(instr, CallDescriptor::kRetpoline)) {
            __ RetpolineCall(wasm_code, constant.rmode());
          } else {
            __ Call(wasm_code, constant.rmode());
          }
        }
      } else {
        Register reg = i.InputRegister(0);
        if (HasCallDescriptorFlag(instr, CallDescriptor::kRetpoline)) {
          __ RetpolineCall(reg);
        } else {
          __ call(reg);
        }
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
      if (HasImmediateInput(instr, 0)) {
        Handle<Code> code = i.InputCode(0);
        __ Jump(code, RelocInfo::CODE_TARGET);
      } else {
        Register reg = i.InputRegister(0);
        DCHECK_IMPLIES(
            HasCallDescriptorFlag(instr, CallDescriptor::kFixedTargetRegister),
            reg == kJavaScriptCallCodeStartRegister);
        __ addp(reg, Immediate(Code::kHeaderSize - kHeapObjectTag));
        if (HasCallDescriptorFlag(instr, CallDescriptor::kRetpoline)) {
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
        if (HasCallDescriptorFlag(instr, CallDescriptor::kRetpoline)) {
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
          HasCallDescriptorFlag(instr, CallDescriptor::kFixedTargetRegister),
          reg == kJavaScriptCallCodeStartRegister);
      if (HasCallDescriptorFlag(instr, CallDescriptor::kRetpoline)) {
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
        __ cmpp(rsi, FieldOperand(func, JSFunction::kContextOffset));
        __ Assert(equal, AbortReason::kWrongFunctionContext);
      }
      static_assert(kJavaScriptCallCodeStartRegister == rcx, "ABI mismatch");
      __ movp(rcx, FieldOperand(func, JSFunction::kCodeOffset));
      __ addp(rcx, Immediate(Code::kHeaderSize - kHeapObjectTag));
      __ call(rcx);
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
      if (HasImmediateInput(instr, 0)) {
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
      // TODO(tebbi): Do we need an lfence here?
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
    case kArchComment:
      __ RecordComment(reinterpret_cast<const char*>(i.InputInt64(0)));
      break;
    case kArchDebugAbort:
      DCHECK(i.InputRegister(0) == rdx);
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
      __ int3();
      unwinding_info_writer_.MarkBlockWillExit();
      break;
    case kArchDebugBreak:
      __ int3();
      break;
    case kArchThrowTerminator:
      unwinding_info_writer_.MarkBlockWillExit();
      break;
    case kArchNop:
      // don't emit code for nops.
      break;
    case kArchDeoptimize: {
      int deopt_state_id =
          BuildTranslation(instr, -1, 0, OutputFrameStateCombine::Ignore());
      CodeGenResult result =
          AssembleDeoptimizerCall(deopt_state_id, current_source_position_);
      if (result != kSuccess) return result;
      unwinding_info_writer_.MarkBlockWillExit();
      break;
    }
    case kArchRet:
      AssembleReturn(instr->InputAt(0));
      break;
    case kArchStackPointer:
      __ movq(i.OutputRegister(), rsp);
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
    case kArchTruncateDoubleToI: {
      auto result = i.OutputRegister();
      auto input = i.InputDoubleRegister(0);
      auto ool = new (zone()) OutOfLineTruncateDoubleToI(
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
      auto ool = new (zone()) OutOfLineRecordWrite(this, object, operand, value,
                                                   scratch0, scratch1, mode);
      __ movp(operand, value);
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
    case kLFence:
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
    case kIeee754Float64Pow: {
      // TODO(bmeurer): Improve integration of the stub.
      __ Movsd(xmm2, xmm0);
      __ Call(BUILTIN_CODE(isolate(), MathPowInternal), RelocInfo::CODE_TARGET);
      __ Movsd(xmm0, xmm3);
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
      if (instr->InputAt(1)->IsRegister()) {
        __ imull(i.InputRegister(1));
      } else {
        __ imull(i.InputOperand(1));
      }
      break;
    case kX64UmulHigh32:
      if (instr->InputAt(1)->IsRegister()) {
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
    case kX64Ror32:
      ASSEMBLE_SHIFT(rorl, 5);
      break;
    case kX64Ror:
      ASSEMBLE_SHIFT(rorq, 6);
      break;
    case kX64Lzcnt:
      if (instr->InputAt(0)->IsRegister()) {
        __ Lzcntq(i.OutputRegister(), i.InputRegister(0));
      } else {
        __ Lzcntq(i.OutputRegister(), i.InputOperand(0));
      }
      break;
    case kX64Lzcnt32:
      if (instr->InputAt(0)->IsRegister()) {
        __ Lzcntl(i.OutputRegister(), i.InputRegister(0));
      } else {
        __ Lzcntl(i.OutputRegister(), i.InputOperand(0));
      }
      break;
    case kX64Tzcnt:
      if (instr->InputAt(0)->IsRegister()) {
        __ Tzcntq(i.OutputRegister(), i.InputRegister(0));
      } else {
        __ Tzcntq(i.OutputRegister(), i.InputOperand(0));
      }
      break;
    case kX64Tzcnt32:
      if (instr->InputAt(0)->IsRegister()) {
        __ Tzcntl(i.OutputRegister(), i.InputRegister(0));
      } else {
        __ Tzcntl(i.OutputRegister(), i.InputOperand(0));
      }
      break;
    case kX64Popcnt:
      if (instr->InputAt(0)->IsRegister()) {
        __ Popcntq(i.OutputRegister(), i.InputRegister(0));
      } else {
        __ Popcntq(i.OutputRegister(), i.InputOperand(0));
      }
      break;
    case kX64Popcnt32:
      if (instr->InputAt(0)->IsRegister()) {
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
      __ pcmpeqd(kScratchDoubleReg, kScratchDoubleReg);
      __ psrlq(kScratchDoubleReg, 33);
      __ andps(i.OutputDoubleRegister(), kScratchDoubleReg);
      break;
    }
    case kSSEFloat32Neg: {
      // TODO(bmeurer): Use RIP relative 128-bit constants.
      __ pcmpeqd(kScratchDoubleReg, kScratchDoubleReg);
      __ psllq(kScratchDoubleReg, 31);
      __ xorps(i.OutputDoubleRegister(), kScratchDoubleReg);
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
      __ subq(rsp, Immediate(kDoubleSize));
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
                                                         kPointerSize);
        __ popfq();
        unwinding_info_writer_.MaybeIncreaseBaseOffsetAt(__ pc_offset(),
                                                         -kPointerSize);
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
      Label compare_nan, compare_swap, done_compare;
      if (instr->InputAt(1)->IsFPRegister()) {
        __ Ucomiss(i.InputDoubleRegister(0), i.InputDoubleRegister(1));
      } else {
        __ Ucomiss(i.InputDoubleRegister(0), i.InputOperand(1));
      }
      auto ool =
          new (zone()) OutOfLineLoadFloat32NaN(this, i.OutputDoubleRegister());
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
          new (zone()) OutOfLineLoadFloat32NaN(this, i.OutputDoubleRegister());
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
      Label compare_nan, compare_swap, done_compare;
      if (instr->InputAt(1)->IsFPRegister()) {
        __ Ucomisd(i.InputDoubleRegister(0), i.InputDoubleRegister(1));
      } else {
        __ Ucomisd(i.InputDoubleRegister(0), i.InputOperand(1));
      }
      auto ool =
          new (zone()) OutOfLineLoadFloat64NaN(this, i.OutputDoubleRegister());
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
          new (zone()) OutOfLineLoadFloat64NaN(this, i.OutputDoubleRegister());
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
    case kSSEFloat64Abs: {
      // TODO(bmeurer): Use RIP relative 128-bit constants.
      __ pcmpeqd(kScratchDoubleReg, kScratchDoubleReg);
      __ psrlq(kScratchDoubleReg, 1);
      __ andpd(i.OutputDoubleRegister(), kScratchDoubleReg);
      break;
    }
    case kSSEFloat64Neg: {
      // TODO(bmeurer): Use RIP relative 128-bit constants.
      __ pcmpeqd(kScratchDoubleReg, kScratchDoubleReg);
      __ psllq(kScratchDoubleReg, 63);
      __ xorpd(i.OutputDoubleRegister(), kScratchDoubleReg);
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
        __ j(parity_even, &fail);
        // If the input is INT64_MIN, then the conversion succeeds.
        __ j(equal, &done);
        __ cmpq(i.OutputRegister(0), Immediate(1));
        // If the conversion results in INT64_MIN, but the input was not
        // INT64_MIN, then the conversion fails.
        __ j(no_overflow, &done);
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
        __ j(parity_even, &fail);
        // If the input is INT64_MIN, then the conversion succeeds.
        __ j(equal, &done);
        __ cmpq(i.OutputRegister(0), Immediate(1));
        // If the conversion results in INT64_MIN, but the input was not
        // INT64_MIN, then the conversion fails.
        __ j(no_overflow, &done);
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
      if (instr->InputAt(0)->IsRegister()) {
        __ Cvtlsi2sd(i.OutputDoubleRegister(), i.InputRegister(0));
      } else {
        __ Cvtlsi2sd(i.OutputDoubleRegister(), i.InputOperand(0));
      }
      break;
    case kSSEInt32ToFloat32:
      if (instr->InputAt(0)->IsRegister()) {
        __ Cvtlsi2ss(i.OutputDoubleRegister(), i.InputRegister(0));
      } else {
        __ Cvtlsi2ss(i.OutputDoubleRegister(), i.InputOperand(0));
      }
      break;
    case kSSEInt64ToFloat32:
      if (instr->InputAt(0)->IsRegister()) {
        __ Cvtqsi2ss(i.OutputDoubleRegister(), i.InputRegister(0));
      } else {
        __ Cvtqsi2ss(i.OutputDoubleRegister(), i.InputOperand(0));
      }
      break;
    case kSSEInt64ToFloat64:
      if (instr->InputAt(0)->IsRegister()) {
        __ Cvtqsi2sd(i.OutputDoubleRegister(), i.InputRegister(0));
      } else {
        __ Cvtqsi2sd(i.OutputDoubleRegister(), i.InputOperand(0));
      }
      break;
    case kSSEUint64ToFloat32:
      if (instr->InputAt(0)->IsRegister()) {
        __ Cvtqui2ss(i.OutputDoubleRegister(), i.InputRegister(0));
      } else {
        __ Cvtqui2ss(i.OutputDoubleRegister(), i.InputOperand(0));
      }
      break;
    case kSSEUint64ToFloat64:
      if (instr->InputAt(0)->IsRegister()) {
        __ Cvtqui2sd(i.OutputDoubleRegister(), i.InputRegister(0));
      } else {
        __ Cvtqui2sd(i.OutputDoubleRegister(), i.InputOperand(0));
      }
      break;
    case kSSEUint32ToFloat64:
      if (instr->InputAt(0)->IsRegister()) {
        __ Cvtlui2sd(i.OutputDoubleRegister(), i.InputRegister(0));
      } else {
        __ Cvtlui2sd(i.OutputDoubleRegister(), i.InputOperand(0));
      }
      break;
    case kSSEUint32ToFloat32:
      if (instr->InputAt(0)->IsRegister()) {
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
      if (instr->InputAt(1)->IsRegister()) {
        __ Pinsrd(i.OutputDoubleRegister(), i.InputRegister(1), 0);
      } else {
        __ Pinsrd(i.OutputDoubleRegister(), i.InputOperand(1), 0);
      }
      break;
    case kSSEFloat64InsertHighWord32:
      if (instr->InputAt(1)->IsRegister()) {
        __ Pinsrd(i.OutputDoubleRegister(), i.InputRegister(1), 1);
      } else {
        __ Pinsrd(i.OutputDoubleRegister(), i.InputOperand(1), 1);
      }
      break;
    case kSSEFloat64LoadLowWord32:
      if (instr->InputAt(0)->IsRegister()) {
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
      __ vpcmpeqd(kScratchDoubleReg, kScratchDoubleReg, kScratchDoubleReg);
      __ vpsrlq(kScratchDoubleReg, kScratchDoubleReg, 33);
      if (instr->InputAt(0)->IsFPRegister()) {
        __ vandps(i.OutputDoubleRegister(), kScratchDoubleReg,
                  i.InputDoubleRegister(0));
      } else {
        __ vandps(i.OutputDoubleRegister(), kScratchDoubleReg,
                  i.InputOperand(0));
      }
      break;
    }
    case kAVXFloat32Neg: {
      // TODO(bmeurer): Use RIP relative 128-bit constants.
      CpuFeatureScope avx_scope(tasm(), AVX);
      __ vpcmpeqd(kScratchDoubleReg, kScratchDoubleReg, kScratchDoubleReg);
      __ vpsllq(kScratchDoubleReg, kScratchDoubleReg, 31);
      if (instr->InputAt(0)->IsFPRegister()) {
        __ vxorps(i.OutputDoubleRegister(), kScratchDoubleReg,
                  i.InputDoubleRegister(0));
      } else {
        __ vxorps(i.OutputDoubleRegister(), kScratchDoubleReg,
                  i.InputOperand(0));
      }
      break;
    }
    case kAVXFloat64Abs: {
      // TODO(bmeurer): Use RIP relative 128-bit constants.
      CpuFeatureScope avx_scope(tasm(), AVX);
      __ vpcmpeqd(kScratchDoubleReg, kScratchDoubleReg, kScratchDoubleReg);
      __ vpsrlq(kScratchDoubleReg, kScratchDoubleReg, 1);
      if (instr->InputAt(0)->IsFPRegister()) {
        __ vandpd(i.OutputDoubleRegister(), kScratchDoubleReg,
                  i.InputDoubleRegister(0));
      } else {
        __ vandpd(i.OutputDoubleRegister(), kScratchDoubleReg,
                  i.InputOperand(0));
      }
      break;
    }
    case kAVXFloat64Neg: {
      // TODO(bmeurer): Use RIP relative 128-bit constants.
      CpuFeatureScope avx_scope(tasm(), AVX);
      __ vpcmpeqd(kScratchDoubleReg, kScratchDoubleReg, kScratchDoubleReg);
      __ vpsllq(kScratchDoubleReg, kScratchDoubleReg, 63);
      if (instr->InputAt(0)->IsFPRegister()) {
        __ vxorpd(i.OutputDoubleRegister(), kScratchDoubleReg,
                  i.InputDoubleRegister(0));
      } else {
        __ vxorpd(i.OutputDoubleRegister(), kScratchDoubleReg,
                  i.InputOperand(0));
      }
      break;
    }
    case kSSEFloat64SilenceNaN:
      __ Xorpd(kScratchDoubleReg, kScratchDoubleReg);
      __ Subsd(i.InputDoubleRegister(0), kScratchDoubleReg);
      break;
    case kX64Movsxbl:
      EmitOOLTrapIfNeeded(zone(), this, opcode, instr, i, __ pc_offset());
      ASSEMBLE_MOVX(movsxbl);
      __ AssertZeroExtended(i.OutputRegister());
      EmitWordLoadPoisoningIfNeeded(this, opcode, instr, i);
      break;
    case kX64Movzxbl:
      EmitOOLTrapIfNeeded(zone(), this, opcode, instr, i, __ pc_offset());
      ASSEMBLE_MOVX(movzxbl);
      __ AssertZeroExtended(i.OutputRegister());
      EmitWordLoadPoisoningIfNeeded(this, opcode, instr, i);
      break;
    case kX64Movsxbq:
      EmitOOLTrapIfNeeded(zone(), this, opcode, instr, i, __ pc_offset());
      ASSEMBLE_MOVX(movsxbq);
      EmitWordLoadPoisoningIfNeeded(this, opcode, instr, i);
      break;
    case kX64Movzxbq:
      EmitOOLTrapIfNeeded(zone(), this, opcode, instr, i, __ pc_offset());
      ASSEMBLE_MOVX(movzxbq);
      __ AssertZeroExtended(i.OutputRegister());
      EmitWordLoadPoisoningIfNeeded(this, opcode, instr, i);
      break;
    case kX64Movb: {
      EmitOOLTrapIfNeeded(zone(), this, opcode, instr, i, __ pc_offset());
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
      EmitOOLTrapIfNeeded(zone(), this, opcode, instr, i, __ pc_offset());
      ASSEMBLE_MOVX(movsxwl);
      __ AssertZeroExtended(i.OutputRegister());
      EmitWordLoadPoisoningIfNeeded(this, opcode, instr, i);
      break;
    case kX64Movzxwl:
      EmitOOLTrapIfNeeded(zone(), this, opcode, instr, i, __ pc_offset());
      ASSEMBLE_MOVX(movzxwl);
      __ AssertZeroExtended(i.OutputRegister());
      EmitWordLoadPoisoningIfNeeded(this, opcode, instr, i);
      break;
    case kX64Movsxwq:
      EmitOOLTrapIfNeeded(zone(), this, opcode, instr, i, __ pc_offset());
      ASSEMBLE_MOVX(movsxwq);
      break;
    case kX64Movzxwq:
      EmitOOLTrapIfNeeded(zone(), this, opcode, instr, i, __ pc_offset());
      ASSEMBLE_MOVX(movzxwq);
      __ AssertZeroExtended(i.OutputRegister());
      EmitWordLoadPoisoningIfNeeded(this, opcode, instr, i);
      break;
    case kX64Movw: {
      EmitOOLTrapIfNeeded(zone(), this, opcode, instr, i, __ pc_offset());
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
      EmitOOLTrapIfNeeded(zone(), this, opcode, instr, i, __ pc_offset());
      if (instr->HasOutput()) {
        if (instr->addressing_mode() == kMode_None) {
          if (instr->InputAt(0)->IsRegister()) {
            __ movl(i.OutputRegister(), i.InputRegister(0));
          } else {
            __ movl(i.OutputRegister(), i.InputOperand(0));
          }
        } else {
          __ movl(i.OutputRegister(), i.MemoryOperand());
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
      EmitOOLTrapIfNeeded(zone(), this, opcode, instr, i, __ pc_offset());
      ASSEMBLE_MOVX(movsxlq);
      EmitWordLoadPoisoningIfNeeded(this, opcode, instr, i);
      break;
    case kX64Movq:
      EmitOOLTrapIfNeeded(zone(), this, opcode, instr, i, __ pc_offset());
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
      EmitOOLTrapIfNeeded(zone(), this, opcode, instr, i, __ pc_offset());
      if (instr->HasOutput()) {
        __ movss(i.OutputDoubleRegister(), i.MemoryOperand());
      } else {
        size_t index = 0;
        Operand operand = i.MemoryOperand(&index);
        __ movss(operand, i.InputDoubleRegister(index));
      }
      break;
    case kX64Movsd: {
      EmitOOLTrapIfNeeded(zone(), this, opcode, instr, i, __ pc_offset());
      if (instr->HasOutput()) {
        const MemoryAccessMode access_mode =
            static_cast<MemoryAccessMode>(MiscField::decode(opcode));
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
      CpuFeatureScope sse_scope(tasm(), SSSE3);
      EmitOOLTrapIfNeeded(zone(), this, opcode, instr, i, __ pc_offset());
      if (instr->HasOutput()) {
        __ movdqu(i.OutputSimd128Register(), i.MemoryOperand());
      } else {
        size_t index = 0;
        Operand operand = i.MemoryOperand(&index);
        __ movdqu(operand, i.InputSimd128Register(index));
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
      if (instr->InputAt(0)->IsRegister()) {
        __ Movd(i.OutputDoubleRegister(), i.InputRegister(0));
      } else {
        __ movss(i.OutputDoubleRegister(), i.InputOperand(0));
      }
      break;
    case kX64BitcastLD:
      if (instr->InputAt(0)->IsRegister()) {
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
            __ subl(i.OutputRegister(), Immediate(-constant_summand));
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
    case kX64Push:
      if (AddressingModeField::decode(instr->opcode()) != kMode_None) {
        size_t index = 0;
        Operand operand = i.MemoryOperand(&index);
        __ pushq(operand);
        frame_access_state()->IncreaseSPDelta(1);
        unwinding_info_writer_.MaybeIncreaseBaseOffsetAt(__ pc_offset(),
                                                         kPointerSize);
      } else if (HasImmediateInput(instr, 0)) {
        __ pushq(i.InputImmediate(0));
        frame_access_state()->IncreaseSPDelta(1);
        unwinding_info_writer_.MaybeIncreaseBaseOffsetAt(__ pc_offset(),
                                                         kPointerSize);
      } else if (instr->InputAt(0)->IsRegister()) {
        __ pushq(i.InputRegister(0));
        frame_access_state()->IncreaseSPDelta(1);
        unwinding_info_writer_.MaybeIncreaseBaseOffsetAt(__ pc_offset(),
                                                         kPointerSize);
      } else if (instr->InputAt(0)->IsFloatRegister() ||
                 instr->InputAt(0)->IsDoubleRegister()) {
        // TODO(titzer): use another machine instruction?
        __ subq(rsp, Immediate(kDoubleSize));
        frame_access_state()->IncreaseSPDelta(kDoubleSize / kPointerSize);
        unwinding_info_writer_.MaybeIncreaseBaseOffsetAt(__ pc_offset(),
                                                         kDoubleSize);
        __ Movsd(Operand(rsp, 0), i.InputDoubleRegister(0));
      } else if (instr->InputAt(0)->IsSimd128Register()) {
        // TODO(titzer): use another machine instruction?
        __ subq(rsp, Immediate(kSimd128Size));
        frame_access_state()->IncreaseSPDelta(kSimd128Size / kPointerSize);
        unwinding_info_writer_.MaybeIncreaseBaseOffsetAt(__ pc_offset(),
                                                         kSimd128Size);
        __ Movups(Operand(rsp, 0), i.InputSimd128Register(0));
      } else if (instr->InputAt(0)->IsStackSlot() ||
                 instr->InputAt(0)->IsFloatStackSlot() ||
                 instr->InputAt(0)->IsDoubleStackSlot()) {
        __ pushq(i.InputOperand(0));
        frame_access_state()->IncreaseSPDelta(1);
        unwinding_info_writer_.MaybeIncreaseBaseOffsetAt(__ pc_offset(),
                                                         kPointerSize);
      } else {
        DCHECK(instr->InputAt(0)->IsSimd128StackSlot());
        __ Movups(kScratchDoubleReg, i.InputOperand(0));
        // TODO(titzer): use another machine instruction?
        __ subq(rsp, Immediate(kSimd128Size));
        frame_access_state()->IncreaseSPDelta(kSimd128Size / kPointerSize);
        unwinding_info_writer_.MaybeIncreaseBaseOffsetAt(__ pc_offset(),
                                                         kSimd128Size);
        __ Movups(Operand(rsp, 0), kScratchDoubleReg);
      }
      break;
    case kX64Poke: {
      int slot = MiscField::decode(instr->opcode());
      if (HasImmediateInput(instr, 0)) {
        __ movq(Operand(rsp, slot * kPointerSize), i.InputImmediate(0));
      } else {
        __ movq(Operand(rsp, slot * kPointerSize), i.InputRegister(0));
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
        } else {
          DCHECK_EQ(MachineRepresentation::kFloat32, op->representation());
          __ Movss(i.OutputFloatRegister(), Operand(rbp, offset));
        }
      } else {
        __ movq(i.OutputRegister(), Operand(rbp, offset));
      }
      break;
    }
    // TODO(gdeepti): Get rid of redundant moves for F32x4Splat/Extract below
    case kX64F32x4Splat: {
      XMMRegister dst = i.OutputSimd128Register();
      if (instr->InputAt(0)->IsFPRegister()) {
        __ movss(dst, i.InputDoubleRegister(0));
      } else {
        __ movss(dst, i.InputOperand(0));
      }
      __ shufps(dst, dst, 0x0);
      break;
    }
    case kX64F32x4ExtractLane: {
      CpuFeatureScope sse_scope(tasm(), SSE4_1);
      __ extractps(kScratchRegister, i.InputSimd128Register(0), i.InputInt8(1));
      __ movd(i.OutputDoubleRegister(), kScratchRegister);
      break;
    }
    case kX64F32x4ReplaceLane: {
      CpuFeatureScope sse_scope(tasm(), SSE4_1);
      // The insertps instruction uses imm8[5:4] to indicate the lane
      // that needs to be replaced.
      byte select = i.InputInt8(1) << 4 & 0x30;
      __ insertps(i.OutputSimd128Register(), i.InputDoubleRegister(2), select);
      break;
    }
    case kX64F32x4SConvertI32x4: {
      __ cvtdq2ps(i.OutputSimd128Register(), i.InputSimd128Register(0));
      break;
    }
    case kX64F32x4UConvertI32x4: {
      DCHECK_EQ(i.OutputSimd128Register(), i.InputSimd128Register(0));
      DCHECK_NE(i.OutputSimd128Register(), kScratchDoubleReg);
      CpuFeatureScope sse_scope(tasm(), SSE4_1);
      XMMRegister dst = i.OutputSimd128Register();
      __ pxor(kScratchDoubleReg, kScratchDoubleReg);      // zeros
      __ pblendw(kScratchDoubleReg, dst, 0x55);           // get lo 16 bits
      __ psubd(dst, kScratchDoubleReg);                   // get hi 16 bits
      __ cvtdq2ps(kScratchDoubleReg, kScratchDoubleReg);  // convert lo exactly
      __ psrld(dst, 1);                  // divide by 2 to get in unsigned range
      __ cvtdq2ps(dst, dst);             // convert hi exactly
      __ addps(dst, dst);                // double hi, exactly
      __ addps(dst, kScratchDoubleReg);  // add hi and lo, may round.
      break;
    }
    case kX64F32x4Abs: {
      XMMRegister dst = i.OutputSimd128Register();
      XMMRegister src = i.InputSimd128Register(0);
      if (dst == src) {
        __ pcmpeqd(kScratchDoubleReg, kScratchDoubleReg);
        __ psrld(kScratchDoubleReg, 1);
        __ andps(i.OutputSimd128Register(), kScratchDoubleReg);
      } else {
        __ pcmpeqd(dst, dst);
        __ psrld(dst, 1);
        __ andps(dst, i.InputSimd128Register(0));
      }
      break;
    }
    case kX64F32x4Neg: {
      XMMRegister dst = i.OutputSimd128Register();
      XMMRegister src = i.InputSimd128Register(0);
      if (dst == src) {
        __ pcmpeqd(kScratchDoubleReg, kScratchDoubleReg);
        __ pslld(kScratchDoubleReg, 31);
        __ xorps(i.OutputSimd128Register(), kScratchDoubleReg);
      } else {
        __ pcmpeqd(dst, dst);
        __ pslld(dst, 31);
        __ xorps(dst, i.InputSimd128Register(0));
      }
      break;
    }
    case kX64F32x4RecipApprox: {
      __ rcpps(i.OutputSimd128Register(), i.InputSimd128Register(0));
      break;
    }
    case kX64F32x4RecipSqrtApprox: {
      __ rsqrtps(i.OutputSimd128Register(), i.InputSimd128Register(0));
      break;
    }
    case kX64F32x4Add: {
      DCHECK_EQ(i.OutputSimd128Register(), i.InputSimd128Register(0));
      __ addps(i.OutputSimd128Register(), i.InputSimd128Register(1));
      break;
    }
    case kX64F32x4AddHoriz: {
      DCHECK_EQ(i.OutputSimd128Register(), i.InputSimd128Register(0));
      CpuFeatureScope sse_scope(tasm(), SSE3);
      __ haddps(i.OutputSimd128Register(), i.InputSimd128Register(1));
      break;
    }
    case kX64F32x4Sub: {
      DCHECK_EQ(i.OutputSimd128Register(), i.InputSimd128Register(0));
      __ subps(i.OutputSimd128Register(), i.InputSimd128Register(1));
      break;
    }
    case kX64F32x4Mul: {
      DCHECK_EQ(i.OutputSimd128Register(), i.InputSimd128Register(0));
      __ mulps(i.OutputSimd128Register(), i.InputSimd128Register(1));
      break;
    }
    case kX64F32x4Min: {
      DCHECK_EQ(i.OutputSimd128Register(), i.InputSimd128Register(0));
      __ minps(i.OutputSimd128Register(), i.InputSimd128Register(1));
      break;
    }
    case kX64F32x4Max: {
      DCHECK_EQ(i.OutputSimd128Register(), i.InputSimd128Register(0));
      __ maxps(i.OutputSimd128Register(), i.InputSimd128Register(1));
      break;
    }
    case kX64F32x4Eq: {
      DCHECK_EQ(i.OutputSimd128Register(), i.InputSimd128Register(0));
      __ cmpps(i.OutputSimd128Register(), i.InputSimd128Register(1), 0x0);
      break;
    }
    case kX64F32x4Ne: {
      DCHECK_EQ(i.OutputSimd128Register(), i.InputSimd128Register(0));
      __ cmpps(i.OutputSimd128Register(), i.InputSimd128Register(1), 0x4);
      break;
    }
    case kX64F32x4Lt: {
      DCHECK_EQ(i.OutputSimd128Register(), i.InputSimd128Register(0));
      __ cmpltps(i.OutputSimd128Register(), i.InputSimd128Register(1));
      break;
    }
    case kX64F32x4Le: {
      DCHECK_EQ(i.OutputSimd128Register(), i.InputSimd128Register(0));
      __ cmpleps(i.OutputSimd128Register(), i.InputSimd128Register(1));
      break;
    }
    case kX64I32x4Splat: {
      XMMRegister dst = i.OutputSimd128Register();
      __ movd(dst, i.InputRegister(0));
      __ pshufd(dst, dst, 0x0);
      break;
    }
    case kX64I32x4ExtractLane: {
      CpuFeatureScope sse_scope(tasm(), SSE4_1);
      __ Pextrd(i.OutputRegister(), i.InputSimd128Register(0), i.InputInt8(1));
      break;
    }
    case kX64I32x4ReplaceLane: {
      CpuFeatureScope sse_scope(tasm(), SSE4_1);
      if (instr->InputAt(2)->IsRegister()) {
        __ Pinsrd(i.OutputSimd128Register(), i.InputRegister(2),
                  i.InputInt8(1));
      } else {
        __ Pinsrd(i.OutputSimd128Register(), i.InputOperand(2), i.InputInt8(1));
      }
      break;
    }
    case kX64I32x4SConvertF32x4: {
      DCHECK_EQ(i.OutputSimd128Register(), i.InputSimd128Register(0));
      XMMRegister dst = i.OutputSimd128Register();
      // NAN->0
      __ movaps(kScratchDoubleReg, dst);
      __ cmpeqps(kScratchDoubleReg, kScratchDoubleReg);
      __ pand(dst, kScratchDoubleReg);
      // Set top bit if >= 0 (but not -0.0!)
      __ pxor(kScratchDoubleReg, dst);
      // Convert
      __ cvttps2dq(dst, dst);
      // Set top bit if >=0 is now < 0
      __ pand(kScratchDoubleReg, dst);
      __ psrad(kScratchDoubleReg, 31);
      // Set positive overflow lanes to 0x7FFFFFFF
      __ pxor(dst, kScratchDoubleReg);
      break;
    }
    case kX64I32x4SConvertI16x8Low: {
      CpuFeatureScope sse_scope(tasm(), SSE4_1);
      __ pmovsxwd(i.OutputSimd128Register(), i.InputSimd128Register(0));
      break;
    }
    case kX64I32x4SConvertI16x8High: {
      CpuFeatureScope sse_scope(tasm(), SSE4_1);
      XMMRegister dst = i.OutputSimd128Register();
      __ palignr(dst, i.InputSimd128Register(0), 8);
      __ pmovsxwd(dst, dst);
      break;
    }
    case kX64I32x4Neg: {
      CpuFeatureScope sse_scope(tasm(), SSSE3);
      XMMRegister dst = i.OutputSimd128Register();
      XMMRegister src = i.InputSimd128Register(0);
      if (dst == src) {
        __ pcmpeqd(kScratchDoubleReg, kScratchDoubleReg);
        __ psignd(dst, kScratchDoubleReg);
      } else {
        __ pxor(dst, dst);
        __ psubd(dst, src);
      }
      break;
    }
    case kX64I32x4Shl: {
      __ pslld(i.OutputSimd128Register(), i.InputInt8(1));
      break;
    }
    case kX64I32x4ShrS: {
      __ psrad(i.OutputSimd128Register(), i.InputInt8(1));
      break;
    }
    case kX64I32x4Add: {
      __ paddd(i.OutputSimd128Register(), i.InputSimd128Register(1));
      break;
    }
    case kX64I32x4AddHoriz: {
      CpuFeatureScope sse_scope(tasm(), SSSE3);
      __ phaddd(i.OutputSimd128Register(), i.InputSimd128Register(1));
      break;
    }
    case kX64I32x4Sub: {
      __ psubd(i.OutputSimd128Register(), i.InputSimd128Register(1));
      break;
    }
    case kX64I32x4Mul: {
      CpuFeatureScope sse_scope(tasm(), SSE4_1);
      __ pmulld(i.OutputSimd128Register(), i.InputSimd128Register(1));
      break;
    }
    case kX64I32x4MinS: {
      CpuFeatureScope sse_scope(tasm(), SSE4_1);
      __ pminsd(i.OutputSimd128Register(), i.InputSimd128Register(1));
      break;
    }
    case kX64I32x4MaxS: {
      CpuFeatureScope sse_scope(tasm(), SSE4_1);
      __ pmaxsd(i.OutputSimd128Register(), i.InputSimd128Register(1));
      break;
    }
    case kX64I32x4Eq: {
      __ pcmpeqd(i.OutputSimd128Register(), i.InputSimd128Register(1));
      break;
    }
    case kX64I32x4Ne: {
      __ pcmpeqd(i.OutputSimd128Register(), i.InputSimd128Register(1));
      __ pcmpeqd(kScratchDoubleReg, kScratchDoubleReg);
      __ pxor(i.OutputSimd128Register(), kScratchDoubleReg);
      break;
    }
    case kX64I32x4GtS: {
      __ pcmpgtd(i.OutputSimd128Register(), i.InputSimd128Register(1));
      break;
    }
    case kX64I32x4GeS: {
      CpuFeatureScope sse_scope(tasm(), SSE4_1);
      XMMRegister dst = i.OutputSimd128Register();
      XMMRegister src = i.InputSimd128Register(1);
      __ pminsd(dst, src);
      __ pcmpeqd(dst, src);
      break;
    }
    case kX64I32x4UConvertF32x4: {
      DCHECK_EQ(i.OutputSimd128Register(), i.InputSimd128Register(0));
      CpuFeatureScope sse_scope(tasm(), SSE4_1);
      XMMRegister dst = i.OutputSimd128Register();
      XMMRegister tmp = i.ToSimd128Register(instr->TempAt(0));
      // NAN->0, negative->0
      __ pxor(kScratchDoubleReg, kScratchDoubleReg);
      __ maxps(dst, kScratchDoubleReg);
      // scratch: float representation of max_signed
      __ pcmpeqd(kScratchDoubleReg, kScratchDoubleReg);
      __ psrld(kScratchDoubleReg, 1);                     // 0x7fffffff
      __ cvtdq2ps(kScratchDoubleReg, kScratchDoubleReg);  // 0x4f000000
      // tmp: convert (src-max_signed).
      // Positive overflow lanes -> 0x7FFFFFFF
      // Negative lanes -> 0
      __ movaps(tmp, dst);
      __ subps(tmp, kScratchDoubleReg);
      __ cmpleps(kScratchDoubleReg, tmp);
      __ cvttps2dq(tmp, tmp);
      __ pxor(tmp, kScratchDoubleReg);
      __ pxor(kScratchDoubleReg, kScratchDoubleReg);
      __ pmaxsd(tmp, kScratchDoubleReg);
      // convert. Overflow lanes above max_signed will be 0x80000000
      __ cvttps2dq(dst, dst);
      // Add (src-max_signed) for overflow lanes.
      __ paddd(dst, tmp);
      break;
    }
    case kX64I32x4UConvertI16x8Low: {
      CpuFeatureScope sse_scope(tasm(), SSE4_1);
      __ pmovzxwd(i.OutputSimd128Register(), i.InputSimd128Register(0));
      break;
    }
    case kX64I32x4UConvertI16x8High: {
      CpuFeatureScope sse_scope(tasm(), SSE4_1);
      XMMRegister dst = i.OutputSimd128Register();
      __ palignr(dst, i.InputSimd128Register(0), 8);
      __ pmovzxwd(dst, dst);
      break;
    }
    case kX64I32x4ShrU: {
      __ psrld(i.OutputSimd128Register(), i.InputInt8(1));
      break;
    }
    case kX64I32x4MinU: {
      CpuFeatureScope sse_scope(tasm(), SSE4_1);
      __ pminud(i.OutputSimd128Register(), i.InputSimd128Register(1));
      break;
    }
    case kX64I32x4MaxU: {
      CpuFeatureScope sse_scope(tasm(), SSE4_1);
      __ pmaxud(i.OutputSimd128Register(), i.InputSimd128Register(1));
      break;
    }
    case kX64I32x4GtU: {
      CpuFeatureScope sse_scope(tasm(), SSE4_1);
      XMMRegister dst = i.OutputSimd128Register();
      XMMRegister src = i.InputSimd128Register(1);
      __ pmaxud(dst, src);
      __ pcmpeqd(dst, src);
      __ pcmpeqd(kScratchDoubleReg, kScratchDoubleReg);
      __ pxor(dst, kScratchDoubleReg);
      break;
    }
    case kX64I32x4GeU: {
      CpuFeatureScope sse_scope(tasm(), SSE4_1);
      XMMRegister dst = i.OutputSimd128Register();
      XMMRegister src = i.InputSimd128Register(1);
      __ pminud(dst, src);
      __ pcmpeqd(dst, src);
      break;
    }
    case kX64S128Zero: {
      XMMRegister dst = i.OutputSimd128Register();
      __ xorps(dst, dst);
      break;
    }
    case kX64I16x8Splat: {
      XMMRegister dst = i.OutputSimd128Register();
      __ movd(dst, i.InputRegister(0));
      __ pshuflw(dst, dst, 0x0);
      __ pshufd(dst, dst, 0x0);
      break;
    }
    case kX64I16x8ExtractLane: {
      CpuFeatureScope sse_scope(tasm(), SSE4_1);
      Register dst = i.OutputRegister();
      __ pextrw(dst, i.InputSimd128Register(0), i.InputInt8(1));
      __ movsxwl(dst, dst);
      break;
    }
    case kX64I16x8ReplaceLane: {
      CpuFeatureScope sse_scope(tasm(), SSE4_1);
      if (instr->InputAt(2)->IsRegister()) {
        __ pinsrw(i.OutputSimd128Register(), i.InputRegister(2),
                  i.InputInt8(1));
      } else {
        __ pinsrw(i.OutputSimd128Register(), i.InputOperand(2), i.InputInt8(1));
      }
      break;
    }
    case kX64I16x8SConvertI8x16Low: {
      CpuFeatureScope sse_scope(tasm(), SSE4_1);
      __ pmovsxbw(i.OutputSimd128Register(), i.InputSimd128Register(0));
      break;
    }
    case kX64I16x8SConvertI8x16High: {
      CpuFeatureScope sse_scope(tasm(), SSE4_1);
      XMMRegister dst = i.OutputSimd128Register();
      __ palignr(dst, i.InputSimd128Register(0), 8);
      __ pmovsxbw(dst, dst);
      break;
    }
    case kX64I16x8Neg: {
      CpuFeatureScope sse_scope(tasm(), SSSE3);
      XMMRegister dst = i.OutputSimd128Register();
      XMMRegister src = i.InputSimd128Register(0);
      if (dst == src) {
        __ pcmpeqd(kScratchDoubleReg, kScratchDoubleReg);
        __ psignw(dst, kScratchDoubleReg);
      } else {
        __ pxor(dst, dst);
        __ psubw(dst, src);
      }
      break;
    }
    case kX64I16x8Shl: {
      __ psllw(i.OutputSimd128Register(), i.InputInt8(1));
      break;
    }
    case kX64I16x8ShrS: {
      __ psraw(i.OutputSimd128Register(), i.InputInt8(1));
      break;
    }
    case kX64I16x8SConvertI32x4: {
      DCHECK_EQ(i.OutputSimd128Register(), i.InputSimd128Register(0));
      __ packssdw(i.OutputSimd128Register(), i.InputSimd128Register(1));
      break;
    }
    case kX64I16x8Add: {
      __ paddw(i.OutputSimd128Register(), i.InputSimd128Register(1));
      break;
    }
    case kX64I16x8AddSaturateS: {
      __ paddsw(i.OutputSimd128Register(), i.InputSimd128Register(1));
      break;
    }
    case kX64I16x8AddHoriz: {
      CpuFeatureScope sse_scope(tasm(), SSSE3);
      __ phaddw(i.OutputSimd128Register(), i.InputSimd128Register(1));
      break;
    }
    case kX64I16x8Sub: {
      __ psubw(i.OutputSimd128Register(), i.InputSimd128Register(1));
      break;
    }
    case kX64I16x8SubSaturateS: {
      __ psubsw(i.OutputSimd128Register(), i.InputSimd128Register(1));
      break;
    }
    case kX64I16x8Mul: {
      CpuFeatureScope sse_scope(tasm(), SSE4_1);
      __ pmullw(i.OutputSimd128Register(), i.InputSimd128Register(1));
      break;
    }
    case kX64I16x8MinS: {
      CpuFeatureScope sse_scope(tasm(), SSE4_1);
      __ pminsw(i.OutputSimd128Register(), i.InputSimd128Register(1));
      break;
    }
    case kX64I16x8MaxS: {
      CpuFeatureScope sse_scope(tasm(), SSE4_1);
      __ pmaxsw(i.OutputSimd128Register(), i.InputSimd128Register(1));
      break;
    }
    case kX64I16x8Eq: {
      __ pcmpeqw(i.OutputSimd128Register(), i.InputSimd128Register(1));
      break;
    }
    case kX64I16x8Ne: {
      __ pcmpeqw(i.OutputSimd128Register(), i.InputSimd128Register(1));
      __ pcmpeqw(kScratchDoubleReg, kScratchDoubleReg);
      __ pxor(i.OutputSimd128Register(), kScratchDoubleReg);
      break;
    }
    case kX64I16x8GtS: {
      __ pcmpgtw(i.OutputSimd128Register(), i.InputSimd128Register(1));
      break;
    }
    case kX64I16x8GeS: {
      CpuFeatureScope sse_scope(tasm(), SSE4_1);
      XMMRegister dst = i.OutputSimd128Register();
      XMMRegister src = i.InputSimd128Register(1);
      __ pminsw(dst, src);
      __ pcmpeqw(dst, src);
      break;
    }
    case kX64I16x8UConvertI8x16Low: {
      CpuFeatureScope sse_scope(tasm(), SSE4_1);
      __ pmovzxbw(i.OutputSimd128Register(), i.InputSimd128Register(0));
      break;
    }
    case kX64I16x8UConvertI8x16High: {
      CpuFeatureScope sse_scope(tasm(), SSE4_1);
      XMMRegister dst = i.OutputSimd128Register();
      __ palignr(dst, i.InputSimd128Register(0), 8);
      __ pmovzxbw(dst, dst);
      break;
    }
    case kX64I16x8ShrU: {
      __ psrlw(i.OutputSimd128Register(), i.InputInt8(1));
      break;
    }
    case kX64I16x8UConvertI32x4: {
      DCHECK_EQ(i.OutputSimd128Register(), i.InputSimd128Register(0));
      CpuFeatureScope sse_scope(tasm(), SSE4_1);
      XMMRegister dst = i.OutputSimd128Register();
      // Change negative lanes to 0x7FFFFFFF
      __ pcmpeqd(kScratchDoubleReg, kScratchDoubleReg);
      __ psrld(kScratchDoubleReg, 1);
      __ pminud(dst, kScratchDoubleReg);
      __ pminud(kScratchDoubleReg, i.InputSimd128Register(1));
      __ packusdw(dst, kScratchDoubleReg);
      break;
    }
    case kX64I16x8AddSaturateU: {
      __ paddusw(i.OutputSimd128Register(), i.InputSimd128Register(1));
      break;
    }
    case kX64I16x8SubSaturateU: {
      __ psubusw(i.OutputSimd128Register(), i.InputSimd128Register(1));
      break;
    }
    case kX64I16x8MinU: {
      CpuFeatureScope sse_scope(tasm(), SSE4_1);
      __ pminuw(i.OutputSimd128Register(), i.InputSimd128Register(1));
      break;
    }
    case kX64I16x8MaxU: {
      CpuFeatureScope sse_scope(tasm(), SSE4_1);
      __ pmaxuw(i.OutputSimd128Register(), i.InputSimd128Register(1));
      break;
    }
    case kX64I16x8GtU: {
      CpuFeatureScope sse_scope(tasm(), SSE4_1);
      XMMRegister dst = i.OutputSimd128Register();
      XMMRegister src = i.InputSimd128Register(1);
      __ pmaxuw(dst, src);
      __ pcmpeqw(dst, src);
      __ pcmpeqw(kScratchDoubleReg, kScratchDoubleReg);
      __ pxor(dst, kScratchDoubleReg);
      break;
    }
    case kX64I16x8GeU: {
      CpuFeatureScope sse_scope(tasm(), SSE4_1);
      XMMRegister dst = i.OutputSimd128Register();
      XMMRegister src = i.InputSimd128Register(1);
      __ pminuw(dst, src);
      __ pcmpeqw(dst, src);
      break;
    }
    case kX64I8x16Splat: {
      CpuFeatureScope sse_scope(tasm(), SSSE3);
      XMMRegister dst = i.OutputSimd128Register();
      __ movd(dst, i.InputRegister(0));
      __ xorps(kScratchDoubleReg, kScratchDoubleReg);
      __ pshufb(dst, kScratchDoubleReg);
      break;
    }
    case kX64I8x16ExtractLane: {
      CpuFeatureScope sse_scope(tasm(), SSE4_1);
      Register dst = i.OutputRegister();
      __ pextrb(dst, i.InputSimd128Register(0), i.InputInt8(1));
      __ movsxbl(dst, dst);
      break;
    }
    case kX64I8x16ReplaceLane: {
      CpuFeatureScope sse_scope(tasm(), SSE4_1);
      if (instr->InputAt(2)->IsRegister()) {
        __ pinsrb(i.OutputSimd128Register(), i.InputRegister(2),
                  i.InputInt8(1));
      } else {
        __ pinsrb(i.OutputSimd128Register(), i.InputOperand(2), i.InputInt8(1));
      }
      break;
    }
    case kX64I8x16SConvertI16x8: {
      DCHECK_EQ(i.OutputSimd128Register(), i.InputSimd128Register(0));
      __ packsswb(i.OutputSimd128Register(), i.InputSimd128Register(1));
      break;
    }
    case kX64I8x16Neg: {
      CpuFeatureScope sse_scope(tasm(), SSSE3);
      XMMRegister dst = i.OutputSimd128Register();
      XMMRegister src = i.InputSimd128Register(0);
      if (dst == src) {
        __ pcmpeqd(kScratchDoubleReg, kScratchDoubleReg);
        __ psignb(dst, kScratchDoubleReg);
      } else {
        __ pxor(dst, dst);
        __ psubb(dst, src);
      }
      break;
    }
    case kX64I8x16Add: {
      __ paddb(i.OutputSimd128Register(), i.InputSimd128Register(1));
      break;
    }
    case kX64I8x16AddSaturateS: {
      __ paddsb(i.OutputSimd128Register(), i.InputSimd128Register(1));
      break;
    }
    case kX64I8x16Sub: {
      __ psubb(i.OutputSimd128Register(), i.InputSimd128Register(1));
      break;
    }
    case kX64I8x16SubSaturateS: {
      __ psubsb(i.OutputSimd128Register(), i.InputSimd128Register(1));
      break;
    }
    case kX64I8x16MinS: {
      CpuFeatureScope sse_scope(tasm(), SSE4_1);
      __ pminsb(i.OutputSimd128Register(), i.InputSimd128Register(1));
      break;
    }
    case kX64I8x16MaxS: {
      CpuFeatureScope sse_scope(tasm(), SSE4_1);
      __ pmaxsb(i.OutputSimd128Register(), i.InputSimd128Register(1));
      break;
    }
    case kX64I8x16Eq: {
      __ pcmpeqb(i.OutputSimd128Register(), i.InputSimd128Register(1));
      break;
    }
    case kX64I8x16Ne: {
      __ pcmpeqb(i.OutputSimd128Register(), i.InputSimd128Register(1));
      __ pcmpeqb(kScratchDoubleReg, kScratchDoubleReg);
      __ pxor(i.OutputSimd128Register(), kScratchDoubleReg);
      break;
    }
    case kX64I8x16GtS: {
      __ pcmpgtb(i.OutputSimd128Register(), i.InputSimd128Register(1));
      break;
    }
    case kX64I8x16GeS: {
      CpuFeatureScope sse_scope(tasm(), SSE4_1);
      XMMRegister dst = i.OutputSimd128Register();
      XMMRegister src = i.InputSimd128Register(1);
      __ pminsb(dst, src);
      __ pcmpeqb(dst, src);
      break;
    }
    case kX64I8x16UConvertI16x8: {
      DCHECK_EQ(i.OutputSimd128Register(), i.InputSimd128Register(0));
      CpuFeatureScope sse_scope(tasm(), SSE4_1);
      XMMRegister dst = i.OutputSimd128Register();
      // Change negative lanes to 0x7FFF
      __ pcmpeqw(kScratchDoubleReg, kScratchDoubleReg);
      __ psrlw(kScratchDoubleReg, 1);
      __ pminuw(dst, kScratchDoubleReg);
      __ pminuw(kScratchDoubleReg, i.InputSimd128Register(1));
      __ packuswb(dst, kScratchDoubleReg);
      break;
    }
    case kX64I8x16AddSaturateU: {
      __ paddusb(i.OutputSimd128Register(), i.InputSimd128Register(1));
      break;
    }
    case kX64I8x16SubSaturateU: {
      __ psubusb(i.OutputSimd128Register(), i.InputSimd128Register(1));
      break;
    }
    case kX64I8x16MinU: {
      CpuFeatureScope sse_scope(tasm(), SSE4_1);
      __ pminub(i.OutputSimd128Register(), i.InputSimd128Register(1));
      break;
    }
    case kX64I8x16MaxU: {
      CpuFeatureScope sse_scope(tasm(), SSE4_1);
      __ pmaxub(i.OutputSimd128Register(), i.InputSimd128Register(1));
      break;
    }
    case kX64I8x16GtU: {
      CpuFeatureScope sse_scope(tasm(), SSE4_1);
      XMMRegister dst = i.OutputSimd128Register();
      XMMRegister src = i.InputSimd128Register(1);
      __ pmaxub(dst, src);
      __ pcmpeqb(dst, src);
      __ pcmpeqb(kScratchDoubleReg, kScratchDoubleReg);
      __ pxor(dst, kScratchDoubleReg);
      break;
    }
    case kX64I8x16GeU: {
      CpuFeatureScope sse_scope(tasm(), SSE4_1);
      XMMRegister dst = i.OutputSimd128Register();
      XMMRegister src = i.InputSimd128Register(1);
      __ pminub(dst, src);
      __ pcmpeqb(dst, src);
      break;
    }
    case kX64S128And: {
      __ pand(i.OutputSimd128Register(), i.InputSimd128Register(1));
      break;
    }
    case kX64S128Or: {
      __ por(i.OutputSimd128Register(), i.InputSimd128Register(1));
      break;
    }
    case kX64S128Xor: {
      __ pxor(i.OutputSimd128Register(), i.InputSimd128Register(1));
      break;
    }
    case kX64S128Not: {
      XMMRegister dst = i.OutputSimd128Register();
      XMMRegister src = i.InputSimd128Register(0);
      if (dst == src) {
        __ movaps(kScratchDoubleReg, dst);
        __ pcmpeqd(dst, dst);
        __ pxor(dst, kScratchDoubleReg);
      } else {
        __ pcmpeqd(dst, dst);
        __ pxor(dst, src);
      }

      break;
    }
    case kX64S128Select: {
      // Mask used here is stored in dst.
      XMMRegister dst = i.OutputSimd128Register();
      __ movaps(kScratchDoubleReg, i.InputSimd128Register(1));
      __ xorps(kScratchDoubleReg, i.InputSimd128Register(2));
      __ andps(dst, kScratchDoubleReg);
      __ xorps(dst, i.InputSimd128Register(2));
      break;
    }
    case kX64S1x4AnyTrue:
    case kX64S1x8AnyTrue:
    case kX64S1x16AnyTrue: {
      CpuFeatureScope sse_scope(tasm(), SSE4_1);
      Register dst = i.OutputRegister();
      XMMRegister src = i.InputSimd128Register(0);
      Register tmp = i.TempRegister(0);
      __ xorq(tmp, tmp);
      __ movq(dst, Immediate(-1));
      __ ptest(src, src);
      __ cmovq(zero, dst, tmp);
      break;
    }
    case kX64S1x4AllTrue:
    case kX64S1x8AllTrue:
    case kX64S1x16AllTrue: {
      CpuFeatureScope sse_scope(tasm(), SSE4_1);
      Register dst = i.OutputRegister();
      XMMRegister src = i.InputSimd128Register(0);
      Register tmp = i.TempRegister(0);
      __ movq(tmp, Immediate(-1));
      __ xorq(dst, dst);
      // Compare all src lanes to false.
      __ pxor(kScratchDoubleReg, kScratchDoubleReg);
      if (arch_opcode == kX64S1x4AllTrue) {
        __ pcmpeqd(kScratchDoubleReg, src);
      } else if (arch_opcode == kX64S1x8AllTrue) {
        __ pcmpeqw(kScratchDoubleReg, src);
      } else {
        __ pcmpeqb(kScratchDoubleReg, src);
      }
      // If kScratchDoubleReg is all zero, none of src lanes are false.
      __ ptest(kScratchDoubleReg, kScratchDoubleReg);
      __ cmovq(zero, dst, tmp);
      break;
    }
    case kX64StackCheck:
      __ CompareRoot(rsp, RootIndex::kStackLimit);
      break;
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
    case kX64Word64AtomicLoadUint8:
    case kX64Word64AtomicLoadUint16:
    case kX64Word64AtomicLoadUint32:
    case kX64Word64AtomicLoadUint64:
    case kX64Word64AtomicStoreWord8:
    case kX64Word64AtomicStoreWord16:
    case kX64Word64AtomicStoreWord32:
    case kX64Word64AtomicStoreWord64:
      UNREACHABLE();  // Won't be generated by instruction selector.
      break;
  }
  return kSuccess;
}  // NOLadability/fn_size)

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
    __ j(not_zero, &nodeopt);

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

void CodeGenerator::AssembleArchTrap(Instruction* instr,
                                     FlagsCondition condition) {
  auto ool = new (zone()) WasmOutOfLineTrap(this, instr);
  Label* tlabel = ool->entry();
  Label end;
  if (condition == kUnorderedEqual) {
    __ j(parity_even, &end);
  } else if (condition == kUnorderedNotEqual) {
    __ j(parity_even, tlabel);
  }
  __ j(FlagsConditionToCondition(condition), tlabel);
  __ bind(&end);
}

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

void CodeGenerator::AssembleArchLookupSwitch(Instruction* instr) {
  X64OperandConverter i(this, instr);
  Register input = i.InputRegister(0);
  for (size_t index = 2; index < instr->InputCount(); index += 2) {
    __ cmpl(input, Immediate(i.InputInt32(index + 0)));
    __ j(equal, GetLabel(i.InputRpo(index + 1)));
  }
  AssembleArchJump(i.InputRpo(1));
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

namespace {

static const int kQuadWordSize = 16;

}  // namespace

void CodeGenerator::FinishFrame(Frame* frame) {
  auto call_descriptor = linkage()->GetIncomingDescriptor();

  const RegList saves_fp = call_descriptor->CalleeSavedFPRegisters();
  if (saves_fp != 0) {
    frame->AlignSavedCalleeRegisterSlots();
    if (saves_fp != 0) {  // Save callee-saved XMM registers.
      const uint32_t saves_fp_count = base::bits::CountPopulation(saves_fp);
      frame->AllocateSavedCalleeRegisterSlots(saves_fp_count *
                                              (kQuadWordSize / kPointerSize));
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
    } else if (call_descriptor->IsJSFunctionCall()) {
      __ Prologue();
      if (call_descriptor->PushArgumentCount()) {
        __ pushq(kJavaScriptCallArgCountRegister);
      }
    } else {
      __ StubPrologue(info()->GetOutputStackFrameType());
      if (call_descriptor->IsWasmFunctionCall()) {
        __ pushq(kWasmInstanceRegister);
      }
    }

    unwinding_info_writer_.MarkFrameConstructed(pc_base);
  }
  int shrink_slots = frame()->GetTotalFrameSlotCount() -
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
    shrink_slots -= static_cast<int>(osr_helper()->UnoptimizedFrameSlots());
    ResetSpeculationPoison();
  }

  const RegList saves = call_descriptor->CalleeSavedRegisters();
  const RegList saves_fp = call_descriptor->CalleeSavedFPRegisters();

  if (shrink_slots > 0) {
    DCHECK(frame_access_state()->has_frame());
    if (info()->IsWasm() && shrink_slots > 128) {
      // For WebAssembly functions with big frames we have to do the stack
      // overflow check before we construct the frame. Otherwise we may not
      // have enough space on the stack to call the runtime for the stack
      // overflow.
      Label done;

      // If the frame is bigger than the stack, we throw the stack overflow
      // exception unconditionally. Thereby we can avoid the integer overflow
      // check in the condition code.
      if (shrink_slots * kPointerSize < FLAG_stack_size * 1024) {
        __ movq(kScratchRegister,
                FieldOperand(kWasmInstanceRegister,
                             WasmInstanceObject::kRealStackLimitAddressOffset));
        __ movq(kScratchRegister, Operand(kScratchRegister, 0));
        __ addq(kScratchRegister, Immediate(shrink_slots * kPointerSize));
        __ cmpq(rsp, kScratchRegister);
        __ j(above_equal, &done);
      }
      __ movp(rcx, FieldOperand(kWasmInstanceRegister,
                                WasmInstanceObject::kCEntryStubOffset));
      __ Move(rsi, Smi::kZero);
      __ CallRuntimeWithCEntry(Runtime::kThrowWasmStackOverflow, rcx);
      ReferenceMap* reference_map = new (zone()) ReferenceMap(zone());
      RecordSafepoint(reference_map, Safepoint::kSimple, 0,
                      Safepoint::kNoLazyDeopt);
      __ AssertUnreachable(AbortReason::kUnexpectedReturnFromWasmTrap);
      __ bind(&done);
    }

    // Skip callee-saved and return slots, which are created below.
    shrink_slots -= base::bits::CountPopulation(saves);
    shrink_slots -=
        base::bits::CountPopulation(saves_fp) * (kQuadWordSize / kPointerSize);
    shrink_slots -= frame()->GetReturnSlotCount();
    if (shrink_slots > 0) {
      __ subq(rsp, Immediate(shrink_slots * kPointerSize));
    }
  }

  if (saves_fp != 0) {  // Save callee-saved XMM registers.
    const uint32_t saves_fp_count = base::bits::CountPopulation(saves_fp);
    const int stack_size = saves_fp_count * kQuadWordSize;
    // Adjust the stack pointer.
    __ subp(rsp, Immediate(stack_size));
    // Store the registers on the stack.
    int slot_idx = 0;
    for (int i = 0; i < XMMRegister::kNumRegisters; i++) {
      if (!((1 << i) & saves_fp)) continue;
      __ movdqu(Operand(rsp, kQuadWordSize * slot_idx),
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
    __ subq(rsp, Immediate(frame()->GetReturnSlotCount() * kPointerSize));
  }
}

void CodeGenerator::AssembleReturn(InstructionOperand* pop) {
  auto call_descriptor = linkage()->GetIncomingDescriptor();

  // Restore registers.
  const RegList saves = call_descriptor->CalleeSavedRegisters();
  if (saves != 0) {
    const int returns = frame()->GetReturnSlotCount();
    if (returns != 0) {
      __ addq(rsp, Immediate(returns * kPointerSize));
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
      __ movdqu(XMMRegister::from_code(i),
                Operand(rsp, kQuadWordSize * slot_idx));
      slot_idx++;
    }
    // Adjust the stack pointer.
    __ addp(rsp, Immediate(stack_size));
  }

  unwinding_info_writer_.MarkBlockWillExit();

  // Might need rcx for scratch if pop_size is too big or if there is a variable
  // pop count.
  DCHECK_EQ(0u, call_descriptor->CalleeSavedRegisters() & rcx.bit());
  DCHECK_EQ(0u, call_descriptor->CalleeSavedRegisters() & rdx.bit());
  size_t pop_size = call_descriptor->StackParameterCount() * kPointerSize;
  X64OperandConverter g(this, nullptr);
  if (call_descriptor->IsCFunctionCall()) {
    AssembleDeconstructFrame();
  } else if (frame_access_state()->has_frame()) {
    if (pop->IsImmediate() && g.ToConstant(pop).ToInt32() == 0) {
      // Canonicalize JSFunction return sites for now.
      if (return_label_.is_bound()) {
        __ jmp(&return_label_);
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
    pop_size += g.ToConstant(pop).ToInt32() * kPointerSize;
    CHECK_LT(pop_size, static_cast<size_t>(std::numeric_limits<int>::max()));
    __ Ret(static_cast<int>(pop_size), rcx);
  } else {
    Register pop_reg = g.ToRegister(pop);
    Register scratch_reg = pop_reg == rcx ? rdx : rcx;
    __ popq(scratch_reg);
    __ leaq(rsp, Operand(rsp, pop_reg, times_8, static_cast<int>(pop_size)));
    __ jmp(scratch_reg);
  }
}

void CodeGenerator::FinishCode() { tasm()->PatchConstPool(); }

void CodeGenerator::AssembleMove(InstructionOperand* source,
                                 InstructionOperand* destination) {
  X64OperandConverter g(this, nullptr);
  // Helper function to write the given constant to the dst register.
  auto MoveConstantToRegister = [&](Register dst, Constant src) {
    switch (src.type()) {
      case Constant::kInt32: {
        if (RelocInfo::IsWasmPtrReference(src.rmode())) {
          __ movq(dst, src.ToInt64(), src.rmode());
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
        if (RelocInfo::IsWasmPtrReference(src.rmode())) {
          __ movq(dst, src.ToInt64(), src.rmode());
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
    if (!RelocInfo::IsWasmPtrReference(src.rmode())) {
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
        __ pushq(src);
        frame_access_state()->IncreaseSPDelta(1);
        unwinding_info_writer_.MaybeIncreaseBaseOffsetAt(__ pc_offset(),
                                                         kPointerSize);
        __ movq(src, g.ToOperand(destination));
        frame_access_state()->IncreaseSPDelta(-1);
        __ popq(g.ToOperand(destination));
        unwinding_info_writer_.MaybeIncreaseBaseOffsetAt(__ pc_offset(),
                                                         -kPointerSize);
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
                                                         kPointerSize);
        __ popq(dst);
        unwinding_info_writer_.MaybeIncreaseBaseOffsetAt(__ pc_offset(),
                                                         -kPointerSize);
        __ movq(src, tmp);
      } else {
        // Without AVX, misaligned reads and writes will trap. Move using the
        // stack, in two parts.
        __ movups(kScratchDoubleReg, dst);  // Save dst in scratch register.
        __ pushq(src);  // Then use stack to copy src to destination.
        unwinding_info_writer_.MaybeIncreaseBaseOffsetAt(__ pc_offset(),
                                                         kPointerSize);
        __ popq(dst);
        unwinding_info_writer_.MaybeIncreaseBaseOffsetAt(__ pc_offset(),
                                                         -kPointerSize);
        __ pushq(g.ToOperand(source, kPointerSize));
        unwinding_info_writer_.MaybeIncreaseBaseOffsetAt(__ pc_offset(),
                                                         kPointerSize);
        __ popq(g.ToOperand(destination, kPointerSize));
        unwinding_info_writer_.MaybeIncreaseBaseOffsetAt(__ pc_offset(),
                                                         -kPointerSize);
        __ movups(src, kScratchDoubleReg);
      }
      return;
    }
    default:
      UNREACHABLE();
      break;
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
