// Copyright 2014 the V8 project authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/crankshaft/s390/lithium-codegen-s390.h"

#include "src/base/bits.h"
#include "src/builtins/builtins-constructor.h"
#include "src/code-factory.h"
#include "src/code-stubs.h"
#include "src/crankshaft/hydrogen-osr.h"
#include "src/crankshaft/s390/lithium-gap-resolver-s390.h"
#include "src/ic/ic.h"
#include "src/ic/stub-cache.h"

namespace v8 {
namespace internal {

class SafepointGenerator final : public CallWrapper {
 public:
  SafepointGenerator(LCodeGen* codegen, LPointerMap* pointers,
                     Safepoint::DeoptMode mode)
      : codegen_(codegen), pointers_(pointers), deopt_mode_(mode) {}
  virtual ~SafepointGenerator() {}

  void BeforeCall(int call_size) const override {}

  void AfterCall() const override {
    codegen_->RecordSafepoint(pointers_, deopt_mode_);
  }

 private:
  LCodeGen* codegen_;
  LPointerMap* pointers_;
  Safepoint::DeoptMode deopt_mode_;
};

LCodeGen::PushSafepointRegistersScope::PushSafepointRegistersScope(
    LCodeGen* codegen)
    : codegen_(codegen) {
  DCHECK(codegen_->info()->is_calling());
  DCHECK(codegen_->expected_safepoint_kind_ == Safepoint::kSimple);
  codegen_->expected_safepoint_kind_ = Safepoint::kWithRegisters;
  StoreRegistersStateStub stub(codegen_->isolate());
  codegen_->masm_->CallStub(&stub);
}

LCodeGen::PushSafepointRegistersScope::~PushSafepointRegistersScope() {
  DCHECK(codegen_->expected_safepoint_kind_ == Safepoint::kWithRegisters);
  RestoreRegistersStateStub stub(codegen_->isolate());
  codegen_->masm_->CallStub(&stub);
  codegen_->expected_safepoint_kind_ = Safepoint::kSimple;
}

#define __ masm()->

bool LCodeGen::GenerateCode() {
  LPhase phase("Z_Code generation", chunk());
  DCHECK(is_unused());
  status_ = GENERATING;

  // Open a frame scope to indicate that there is a frame on the stack.  The
  // NONE indicates that the scope shouldn't actually generate code to set up
  // the frame (that is done in GeneratePrologue).
  FrameScope frame_scope(masm_, StackFrame::NONE);

  return GeneratePrologue() && GenerateBody() && GenerateDeferredCode() &&
         GenerateJumpTable() && GenerateSafepointTable();
}

void LCodeGen::FinishCode(Handle<Code> code) {
  DCHECK(is_done());
  code->set_stack_slots(GetTotalFrameSlotCount());
  code->set_safepoint_table_offset(safepoints_.GetCodeOffset());
  PopulateDeoptimizationData(code);
}

void LCodeGen::SaveCallerDoubles() {
  DCHECK(info()->saves_caller_doubles());
  DCHECK(NeedsEagerFrame());
  Comment(";;; Save clobbered callee double registers");
  int count = 0;
  BitVector* doubles = chunk()->allocated_double_registers();
  BitVector::Iterator save_iterator(doubles);
  while (!save_iterator.Done()) {
    __ StoreDouble(DoubleRegister::from_code(save_iterator.Current()),
                   MemOperand(sp, count * kDoubleSize));
    save_iterator.Advance();
    count++;
  }
}

void LCodeGen::RestoreCallerDoubles() {
  DCHECK(info()->saves_caller_doubles());
  DCHECK(NeedsEagerFrame());
  Comment(";;; Restore clobbered callee double registers");
  BitVector* doubles = chunk()->allocated_double_registers();
  BitVector::Iterator save_iterator(doubles);
  int count = 0;
  while (!save_iterator.Done()) {
    __ LoadDouble(DoubleRegister::from_code(save_iterator.Current()),
                  MemOperand(sp, count * kDoubleSize));
    save_iterator.Advance();
    count++;
  }
}

bool LCodeGen::GeneratePrologue() {
  DCHECK(is_generating());

  if (info()->IsOptimizing()) {
    ProfileEntryHookStub::MaybeCallEntryHook(masm_);

    // r3: Callee's JS function.
    // cp: Callee's context.
    // fp: Caller's frame pointer.
    // lr: Caller's pc.
    // ip: Our own function entry (required by the prologue)
  }

  int prologue_offset = masm_->pc_offset();

  if (prologue_offset) {
    // Prologue logic requires its starting address in ip and the
    // corresponding offset from the function entry.  Need to add
    // 4 bytes for the size of AHI/AGHI that AddP expands into.
    prologue_offset += sizeof(FourByteInstr);
    __ AddP(ip, ip, Operand(prologue_offset));
  }
  info()->set_prologue_offset(prologue_offset);
  if (NeedsEagerFrame()) {
    if (info()->IsStub()) {
      __ StubPrologue(StackFrame::STUB, ip, prologue_offset);
    } else {
      __ Prologue(info()->GeneratePreagedPrologue(), ip, prologue_offset);
    }
    frame_is_built_ = true;
  }

  // Reserve space for the stack slots needed by the code.
  int slots = GetStackSlotCount();
  if (slots > 0) {
    __ lay(sp, MemOperand(sp, -(slots * kPointerSize)));
    if (FLAG_debug_code) {
      __ Push(r2, r3);
      __ mov(r2, Operand(slots * kPointerSize));
      __ mov(r3, Operand(kSlotsZapValue));
      Label loop;
      __ bind(&loop);
      __ StoreP(r3, MemOperand(sp, r2, kPointerSize));
      __ lay(r2, MemOperand(r2, -kPointerSize));
      __ CmpP(r2, Operand::Zero());
      __ bne(&loop);
      __ Pop(r2, r3);
    }
  }

  if (info()->saves_caller_doubles()) {
    SaveCallerDoubles();
  }
  return !is_aborted();
}

void LCodeGen::DoPrologue(LPrologue* instr) {
  Comment(";;; Prologue begin");

  // Possibly allocate a local context.
  if (info()->scope()->NeedsContext()) {
    Comment(";;; Allocate local context");
    bool need_write_barrier = true;
    // Argument to NewContext is the function, which is in r3.
    int slots = info()->scope()->num_heap_slots() - Context::MIN_CONTEXT_SLOTS;
    Safepoint::DeoptMode deopt_mode = Safepoint::kNoLazyDeopt;
    if (info()->scope()->is_script_scope()) {
      __ push(r3);
      __ Push(info()->scope()->scope_info());
      __ CallRuntime(Runtime::kNewScriptContext);
      deopt_mode = Safepoint::kLazyDeopt;
    } else {
      if (slots <= ConstructorBuiltins::MaximumFunctionContextSlots()) {
        Callable callable = CodeFactory::FastNewFunctionContext(
            isolate(), info()->scope()->scope_type());
        __ mov(FastNewFunctionContextDescriptor::SlotsRegister(),
               Operand(slots));
        __ Call(callable.code(), RelocInfo::CODE_TARGET);
        // Result of the FastNewFunctionContext builtin is always in new space.
        need_write_barrier = false;
      } else {
        __ push(r3);
        __ Push(Smi::FromInt(info()->scope()->scope_type()));
        __ CallRuntime(Runtime::kNewFunctionContext);
      }
    }
    RecordSafepoint(deopt_mode);

    // Context is returned in both r2 and cp.  It replaces the context
    // passed to us.  It's saved in the stack and kept live in cp.
    __ LoadRR(cp, r2);
    __ StoreP(r2, MemOperand(fp, StandardFrameConstants::kContextOffset));
    // Copy any necessary parameters into the context.
    int num_parameters = info()->scope()->num_parameters();
    int first_parameter = info()->scope()->has_this_declaration() ? -1 : 0;
    for (int i = first_parameter; i < num_parameters; i++) {
      Variable* var = (i == -1) ? info()->scope()->receiver()
                                : info()->scope()->parameter(i);
      if (var->IsContextSlot()) {
        int parameter_offset = StandardFrameConstants::kCallerSPOffset +
                               (num_parameters - 1 - i) * kPointerSize;
        // Load parameter from stack.
        __ LoadP(r2, MemOperand(fp, parameter_offset));
        // Store it in the context.
        MemOperand target = ContextMemOperand(cp, var->index());
        __ StoreP(r2, target);
        // Update the write barrier. This clobbers r5 and r2.
        if (need_write_barrier) {
          __ RecordWriteContextSlot(cp, target.offset(), r2, r5,
                                    GetLinkRegisterState(), kSaveFPRegs);
        } else if (FLAG_debug_code) {
          Label done;
          __ JumpIfInNewSpace(cp, r2, &done);
          __ Abort(kExpectedNewSpaceObject);
          __ bind(&done);
        }
      }
    }
    Comment(";;; End allocate local context");
  }

  Comment(";;; Prologue end");
}

void LCodeGen::GenerateOsrPrologue() {
  // Generate the OSR entry prologue at the first unknown OSR value, or if there
  // are none, at the OSR entrypoint instruction.
  if (osr_pc_offset_ >= 0) return;

  osr_pc_offset_ = masm()->pc_offset();

  // Adjust the frame size, subsuming the unoptimized frame into the
  // optimized frame.
  int slots = GetStackSlotCount() - graph()->osr()->UnoptimizedFrameSlots();
  DCHECK(slots >= 0);
  __ lay(sp, MemOperand(sp, -slots * kPointerSize));
}

void LCodeGen::GenerateBodyInstructionPre(LInstruction* instr) {
  if (instr->IsCall()) {
    EnsureSpaceForLazyDeopt(Deoptimizer::patch_size());
  }
  if (!instr->IsLazyBailout() && !instr->IsGap()) {
    safepoints_.BumpLastLazySafepointIndex();
  }
}

bool LCodeGen::GenerateDeferredCode() {
  DCHECK(is_generating());
  if (deferred_.length() > 0) {
    for (int i = 0; !is_aborted() && i < deferred_.length(); i++) {
      LDeferredCode* code = deferred_[i];

      HValue* value =
          instructions_->at(code->instruction_index())->hydrogen_value();
      RecordAndWritePosition(value->position());

      Comment(
          ";;; <@%d,#%d> "
          "-------------------- Deferred %s --------------------",
          code->instruction_index(), code->instr()->hydrogen_value()->id(),
          code->instr()->Mnemonic());
      __ bind(code->entry());
      if (NeedsDeferredFrame()) {
        Comment(";;; Build frame");
        DCHECK(!frame_is_built_);
        DCHECK(info()->IsStub());
        frame_is_built_ = true;
        __ Load(scratch0(),
                Operand(StackFrame::TypeToMarker(StackFrame::STUB)));
        __ PushCommonFrame(scratch0());
        Comment(";;; Deferred code");
      }
      code->Generate();
      if (NeedsDeferredFrame()) {
        Comment(";;; Destroy frame");
        DCHECK(frame_is_built_);
        __ PopCommonFrame(scratch0());
        frame_is_built_ = false;
      }
      __ b(code->exit());
    }
  }

  return !is_aborted();
}

bool LCodeGen::GenerateJumpTable() {
  // Check that the jump table is accessible from everywhere in the function
  // code, i.e. that offsets in halfworld to the table can be encoded in the
  // 32-bit signed immediate of a branch instruction.
  // To simplify we consider the code size from the first instruction to the
  // end of the jump table. We also don't consider the pc load delta.
  // Each entry in the jump table generates one instruction and inlines one
  // 32bit data after it.
  // TODO(joransiu): The Int24 condition can likely be relaxed for S390
  if (!is_int24(masm()->pc_offset() + jump_table_.length() * 7)) {
    Abort(kGeneratedCodeIsTooLarge);
  }

  if (jump_table_.length() > 0) {
    Label needs_frame, call_deopt_entry;

    Comment(";;; -------------------- Jump table --------------------");
    Address base = jump_table_[0].address;

    Register entry_offset = scratch0();

    int length = jump_table_.length();
    for (int i = 0; i < length; i++) {
      Deoptimizer::JumpTableEntry* table_entry = &jump_table_[i];
      __ bind(&table_entry->label);

      DCHECK_EQ(jump_table_[0].bailout_type, table_entry->bailout_type);
      Address entry = table_entry->address;
      DeoptComment(table_entry->deopt_info);

      // Second-level deopt table entries are contiguous and small, so instead
      // of loading the full, absolute address of each one, load an immediate
      // offset which will be added to the base address later.
      __ mov(entry_offset, Operand(entry - base));

      if (table_entry->needs_frame) {
        DCHECK(!info()->saves_caller_doubles());
        Comment(";;; call deopt with frame");
        __ PushCommonFrame();
        __ b(r14, &needs_frame);
      } else {
        __ b(r14, &call_deopt_entry);
      }
    }

    if (needs_frame.is_linked()) {
      __ bind(&needs_frame);
      // This variant of deopt can only be used with stubs. Since we don't
      // have a function pointer to install in the stack frame that we're
      // building, install a special marker there instead.
      DCHECK(info()->IsStub());
      __ Load(ip, Operand(StackFrame::TypeToMarker(StackFrame::STUB)));
      __ push(ip);
      DCHECK(info()->IsStub());
    }

    Comment(";;; call deopt");
    __ bind(&call_deopt_entry);

    if (info()->saves_caller_doubles()) {
      DCHECK(info()->IsStub());
      RestoreCallerDoubles();
    }

    // Add the base address to the offset previously loaded in entry_offset.
    __ mov(ip, Operand(ExternalReference::ForDeoptEntry(base)));
    __ AddP(ip, entry_offset, ip);
    __ Jump(ip);
  }

  // The deoptimization jump table is the last part of the instruction
  // sequence. Mark the generated code as done unless we bailed out.
  if (!is_aborted()) status_ = DONE;
  return !is_aborted();
}

bool LCodeGen::GenerateSafepointTable() {
  DCHECK(is_done());
  safepoints_.Emit(masm(), GetTotalFrameSlotCount());
  return !is_aborted();
}

Register LCodeGen::ToRegister(int code) const {
  return Register::from_code(code);
}

DoubleRegister LCodeGen::ToDoubleRegister(int code) const {
  return DoubleRegister::from_code(code);
}

Register LCodeGen::ToRegister(LOperand* op) const {
  DCHECK(op->IsRegister());
  return ToRegister(op->index());
}

Register LCodeGen::EmitLoadRegister(LOperand* op, Register scratch) {
  if (op->IsRegister()) {
    return ToRegister(op->index());
  } else if (op->IsConstantOperand()) {
    LConstantOperand* const_op = LConstantOperand::cast(op);
    HConstant* constant = chunk_->LookupConstant(const_op);
    Handle<Object> literal = constant->handle(isolate());
    Representation r = chunk_->LookupLiteralRepresentation(const_op);
    if (r.IsInteger32()) {
      AllowDeferredHandleDereference get_number;
      DCHECK(literal->IsNumber());
      __ LoadIntLiteral(scratch, static_cast<int32_t>(literal->Number()));
    } else if (r.IsDouble()) {
      Abort(kEmitLoadRegisterUnsupportedDoubleImmediate);
    } else {
      DCHECK(r.IsSmiOrTagged());
      __ Move(scratch, literal);
    }
    return scratch;
  } else if (op->IsStackSlot()) {
    __ LoadP(scratch, ToMemOperand(op));
    return scratch;
  }
  UNREACHABLE();
  return scratch;
}

void LCodeGen::EmitLoadIntegerConstant(LConstantOperand* const_op,
                                       Register dst) {
  DCHECK(IsInteger32(const_op));
  HConstant* constant = chunk_->LookupConstant(const_op);
  int32_t value = constant->Integer32Value();
  if (IsSmi(const_op)) {
    __ LoadSmiLiteral(dst, Smi::FromInt(value));
  } else {
    __ LoadIntLiteral(dst, value);
  }
}

DoubleRegister LCodeGen::ToDoubleRegister(LOperand* op) const {
  DCHECK(op->IsDoubleRegister());
  return ToDoubleRegister(op->index());
}

Handle<Object> LCodeGen::ToHandle(LConstantOperand* op) const {
  HConstant* constant = chunk_->LookupConstant(op);
  DCHECK(chunk_->LookupLiteralRepresentation(op).IsSmiOrTagged());
  return constant->handle(isolate());
}

bool LCodeGen::IsInteger32(LConstantOperand* op) const {
  return chunk_->LookupLiteralRepresentation(op).IsSmiOrInteger32();
}

bool LCodeGen::IsSmi(LConstantOperand* op) const {
  return chunk_->LookupLiteralRepresentation(op).IsSmi();
}

int32_t LCodeGen::ToInteger32(LConstantOperand* op) const {
  return ToRepresentation(op, Representation::Integer32());
}

intptr_t LCodeGen::ToRepresentation(LConstantOperand* op,
                                    const Representation& r) const {
  HConstant* constant = chunk_->LookupConstant(op);
  int32_t value = constant->Integer32Value();
  if (r.IsInteger32()) return value;
  DCHECK(r.IsSmiOrTagged());
  return reinterpret_cast<intptr_t>(Smi::FromInt(value));
}

Smi* LCodeGen::ToSmi(LConstantOperand* op) const {
  HConstant* constant = chunk_->LookupConstant(op);
  return Smi::FromInt(constant->Integer32Value());
}

double LCodeGen::ToDouble(LConstantOperand* op) const {
  HConstant* constant = chunk_->LookupConstant(op);
  DCHECK(constant->HasDoubleValue());
  return constant->DoubleValue();
}

Operand LCodeGen::ToOperand(LOperand* op) {
  if (op->IsConstantOperand()) {
    LConstantOperand* const_op = LConstantOperand::cast(op);
    HConstant* constant = chunk()->LookupConstant(const_op);
    Representation r = chunk_->LookupLiteralRepresentation(const_op);
    if (r.IsSmi()) {
      DCHECK(constant->HasSmiValue());
      return Operand(Smi::FromInt(constant->Integer32Value()));
    } else if (r.IsInteger32()) {
      DCHECK(constant->HasInteger32Value());
      return Operand(constant->Integer32Value());
    } else if (r.IsDouble()) {
      Abort(kToOperandUnsupportedDoubleImmediate);
    }
    DCHECK(r.IsTagged());
    return Operand(constant->handle(isolate()));
  } else if (op->IsRegister()) {
    return Operand(ToRegister(op));
  } else if (op->IsDoubleRegister()) {
    Abort(kToOperandIsDoubleRegisterUnimplemented);
    return Operand::Zero();
  }
  // Stack slots not implemented, use ToMemOperand instead.
  UNREACHABLE();
  return Operand::Zero();
}

static int ArgumentsOffsetWithoutFrame(int index) {
  DCHECK(index < 0);
  return -(index + 1) * kPointerSize;
}

MemOperand LCodeGen::ToMemOperand(LOperand* op) const {
  DCHECK(!op->IsRegister());
  DCHECK(!op->IsDoubleRegister());
  DCHECK(op->IsStackSlot() || op->IsDoubleStackSlot());
  if (NeedsEagerFrame()) {
    return MemOperand(fp, FrameSlotToFPOffset(op->index()));
  } else {
    // Retrieve parameter without eager stack-frame relative to the
    // stack-pointer.
    return MemOperand(sp, ArgumentsOffsetWithoutFrame(op->index()));
  }
}

MemOperand LCodeGen::ToHighMemOperand(LOperand* op) const {
  DCHECK(op->IsDoubleStackSlot());
  if (NeedsEagerFrame()) {
    return MemOperand(fp, FrameSlotToFPOffset(op->index()) + kPointerSize);
  } else {
    // Retrieve parameter without eager stack-frame relative to the
    // stack-pointer.
    return MemOperand(sp,
                      ArgumentsOffsetWithoutFrame(op->index()) + kPointerSize);
  }
}

void LCodeGen::WriteTranslation(LEnvironment* environment,
                                Translation* translation) {
  if (environment == NULL) return;

  // The translation includes one command per value in the environment.
  int translation_size = environment->translation_size();

  WriteTranslation(environment->outer(), translation);
  WriteTranslationFrame(environment, translation);

  int object_index = 0;
  int dematerialized_index = 0;
  for (int i = 0; i < translation_size; ++i) {
    LOperand* value = environment->values()->at(i);
    AddToTranslation(
        environment, translation, value, environment->HasTaggedValueAt(i),
        environment->HasUint32ValueAt(i), &object_index, &dematerialized_index);
  }
}

void LCodeGen::AddToTranslation(LEnvironment* environment,
                                Translation* translation, LOperand* op,
                                bool is_tagged, bool is_uint32,
                                int* object_index_pointer,
                                int* dematerialized_index_pointer) {
  if (op == LEnvironment::materialization_marker()) {
    int object_index = (*object_index_pointer)++;
    if (environment->ObjectIsDuplicateAt(object_index)) {
      int dupe_of = environment->ObjectDuplicateOfAt(object_index);
      translation->DuplicateObject(dupe_of);
      return;
    }
    int object_length = environment->ObjectLengthAt(object_index);
    if (environment->ObjectIsArgumentsAt(object_index)) {
      translation->BeginArgumentsObject(object_length);
    } else {
      translation->BeginCapturedObject(object_length);
    }
    int dematerialized_index = *dematerialized_index_pointer;
    int env_offset = environment->translation_size() + dematerialized_index;
    *dematerialized_index_pointer += object_length;
    for (int i = 0; i < object_length; ++i) {
      LOperand* value = environment->values()->at(env_offset + i);
      AddToTranslation(environment, translation, value,
                       environment->HasTaggedValueAt(env_offset + i),
                       environment->HasUint32ValueAt(env_offset + i),
                       object_index_pointer, dematerialized_index_pointer);
    }
    return;
  }

  if (op->IsStackSlot()) {
    int index = op->index();
    if (is_tagged) {
      translation->StoreStackSlot(index);
    } else if (is_uint32) {
      translation->StoreUint32StackSlot(index);
    } else {
      translation->StoreInt32StackSlot(index);
    }
  } else if (op->IsDoubleStackSlot()) {
    int index = op->index();
    translation->StoreDoubleStackSlot(index);
  } else if (op->IsRegister()) {
    Register reg = ToRegister(op);
    if (is_tagged) {
      translation->StoreRegister(reg);
    } else if (is_uint32) {
      translation->StoreUint32Register(reg);
    } else {
      translation->StoreInt32Register(reg);
    }
  } else if (op->IsDoubleRegister()) {
    DoubleRegister reg = ToDoubleRegister(op);
    translation->StoreDoubleRegister(reg);
  } else if (op->IsConstantOperand()) {
    HConstant* constant = chunk()->LookupConstant(LConstantOperand::cast(op));
    int src_index = DefineDeoptimizationLiteral(constant->handle(isolate()));
    translation->StoreLiteral(src_index);
  } else {
    UNREACHABLE();
  }
}

void LCodeGen::CallCode(Handle<Code> code, RelocInfo::Mode mode,
                        LInstruction* instr) {
  CallCodeGeneric(code, mode, instr, RECORD_SIMPLE_SAFEPOINT);
}

void LCodeGen::CallCodeGeneric(Handle<Code> code, RelocInfo::Mode mode,
                               LInstruction* instr,
                               SafepointMode safepoint_mode) {
  DCHECK(instr != NULL);
  __ Call(code, mode);
  RecordSafepointWithLazyDeopt(instr, safepoint_mode);

  // Signal that we don't inline smi code before these stubs in the
  // optimizing code generator.
  if (code->kind() == Code::BINARY_OP_IC || code->kind() == Code::COMPARE_IC) {
    __ nop();
  }
}

void LCodeGen::CallRuntime(const Runtime::Function* function, int num_arguments,
                           LInstruction* instr, SaveFPRegsMode save_doubles) {
  DCHECK(instr != NULL);

  __ CallRuntime(function, num_arguments, save_doubles);

  RecordSafepointWithLazyDeopt(instr, RECORD_SIMPLE_SAFEPOINT);
}

void LCodeGen::LoadContextFromDeferred(LOperand* context) {
  if (context->IsRegister()) {
    __ Move(cp, ToRegister(context));
  } else if (context->IsStackSlot()) {
    __ LoadP(cp, ToMemOperand(context));
  } else if (context->IsConstantOperand()) {
    HConstant* constant =
        chunk_->LookupConstant(LConstantOperand::cast(context));
    __ Move(cp, Handle<Object>::cast(constant->handle(isolate())));
  } else {
    UNREACHABLE();
  }
}

void LCodeGen::CallRuntimeFromDeferred(Runtime::FunctionId id, int argc,
                                       LInstruction* instr, LOperand* context) {
  LoadContextFromDeferred(context);
  __ CallRuntimeSaveDoubles(id);
  RecordSafepointWithRegisters(instr->pointer_map(), argc,
                               Safepoint::kNoLazyDeopt);
}

void LCodeGen::RegisterEnvironmentForDeoptimization(LEnvironment* environment,
                                                    Safepoint::DeoptMode mode) {
  environment->set_has_been_used();
  if (!environment->HasBeenRegistered()) {
    // Physical stack frame layout:
    // -x ............. -4  0 ..................................... y
    // [incoming arguments] [spill slots] [pushed outgoing arguments]

    // Layout of the environment:
    // 0 ..................................................... size-1
    // [parameters] [locals] [expression stack including arguments]

    // Layout of the translation:
    // 0 ........................................................ size - 1 + 4
    // [expression stack including arguments] [locals] [4 words] [parameters]
    // |>------------  translation_size ------------<|

    int frame_count = 0;
    int jsframe_count = 0;
    for (LEnvironment* e = environment; e != NULL; e = e->outer()) {
      ++frame_count;
      if (e->frame_type() == JS_FUNCTION) {
        ++jsframe_count;
      }
    }
    Translation translation(&translations_, frame_count, jsframe_count, zone());
    WriteTranslation(environment, &translation);
    int deoptimization_index = deoptimizations_.length();
    int pc_offset = masm()->pc_offset();
    environment->Register(deoptimization_index, translation.index(),
                          (mode == Safepoint::kLazyDeopt) ? pc_offset : -1);
    deoptimizations_.Add(environment, zone());
  }
}

void LCodeGen::DeoptimizeIf(Condition cond, LInstruction* instr,
                            DeoptimizeReason deopt_reason,
                            Deoptimizer::BailoutType bailout_type,
                            CRegister cr) {
  LEnvironment* environment = instr->environment();
  RegisterEnvironmentForDeoptimization(environment, Safepoint::kNoLazyDeopt);
  DCHECK(environment->HasBeenRegistered());
  int id = environment->deoptimization_index();
  Address entry =
      Deoptimizer::GetDeoptimizationEntry(isolate(), id, bailout_type);
  if (entry == NULL) {
    Abort(kBailoutWasNotPrepared);
    return;
  }

  if (FLAG_deopt_every_n_times != 0 && !info()->IsStub()) {
    Register scratch = scratch0();
    ExternalReference count = ExternalReference::stress_deopt_count(isolate());
    Label no_deopt;

    // Store the condition on the stack if necessary
    if (cond != al) {
      Label done;
      __ LoadImmP(scratch, Operand::Zero());
      __ b(NegateCondition(cond), &done, Label::kNear);
      __ LoadImmP(scratch, Operand(1));
      __ bind(&done);
      __ push(scratch);
    }

    Label done;
    __ Push(r3);
    __ mov(scratch, Operand(count));
    __ LoadW(r3, MemOperand(scratch));
    __ Sub32(r3, r3, Operand(1));
    __ Cmp32(r3, Operand::Zero());
    __ bne(&no_deopt, Label::kNear);

    __ LoadImmP(r3, Operand(FLAG_deopt_every_n_times));
    __ StoreW(r3, MemOperand(scratch));
    __ Pop(r3);

    if (cond != al) {
      // Clean up the stack before the deoptimizer call
      __ pop(scratch);
    }

    __ Call(entry, RelocInfo::RUNTIME_ENTRY);

    __ b(&done);

    __ bind(&no_deopt);
    __ StoreW(r3, MemOperand(scratch));
    __ Pop(r3);

    if (cond != al) {
      // Clean up the stack before the deoptimizer call
      __ pop(scratch);
    }

    __ bind(&done);

    if (cond != al) {
      cond = ne;
      __ CmpP(scratch, Operand::Zero());
    }
  }

  if (info()->ShouldTrapOnDeopt()) {
    __ stop("trap_on_deopt", cond, kDefaultStopCode, cr);
  }

  Deoptimizer::DeoptInfo deopt_info = MakeDeoptInfo(instr, deopt_reason, id);

  DCHECK(info()->IsStub() || frame_is_built_);
  // Go through jump table if we need to handle condition, build frame, or
  // restore caller doubles.
  if (cond == al && frame_is_built_ && !info()->saves_caller_doubles()) {
    __ Call(entry, RelocInfo::RUNTIME_ENTRY);
  } else {
    Deoptimizer::JumpTableEntry table_entry(entry, deopt_info, bailout_type,
                                            !frame_is_built_);
    // We often have several deopts to the same entry, reuse the last
    // jump entry if this is the case.
    if (FLAG_trace_deopt || isolate()->is_profiling() ||
        jump_table_.is_empty() ||
        !table_entry.IsEquivalentTo(jump_table_.last())) {
      jump_table_.Add(table_entry, zone());
    }
    __ b(cond, &jump_table_.last().label /*, cr*/);
  }
}

void LCodeGen::DeoptimizeIf(Condition cond, LInstruction* instr,
                            DeoptimizeReason deopt_reason, CRegister cr) {
  Deoptimizer::BailoutType bailout_type =
      info()->IsStub() ? Deoptimizer::LAZY : Deoptimizer::EAGER;
  DeoptimizeIf(cond, instr, deopt_reason, bailout_type, cr);
}

void LCodeGen::RecordSafepointWithLazyDeopt(LInstruction* instr,
                                            SafepointMode safepoint_mode) {
  if (safepoint_mode == RECORD_SIMPLE_SAFEPOINT) {
    RecordSafepoint(instr->pointer_map(), Safepoint::kLazyDeopt);
  } else {
    DCHECK(safepoint_mode == RECORD_SAFEPOINT_WITH_REGISTERS_AND_NO_ARGUMENTS);
    RecordSafepointWithRegisters(instr->pointer_map(), 0,
                                 Safepoint::kLazyDeopt);
  }
}

void LCodeGen::RecordSafepoint(LPointerMap* pointers, Safepoint::Kind kind,
                               int arguments, Safepoint::DeoptMode deopt_mode) {
  DCHECK(expected_safepoint_kind_ == kind);

  const ZoneList<LOperand*>* operands = pointers->GetNormalizedOperands();
  Safepoint safepoint =
      safepoints_.DefineSafepoint(masm(), kind, arguments, deopt_mode);
  for (int i = 0; i < operands->length(); i++) {
    LOperand* pointer = operands->at(i);
    if (pointer->IsStackSlot()) {
      safepoint.DefinePointerSlot(pointer->index(), zone());
    } else if (pointer->IsRegister() && (kind & Safepoint::kWithRegisters)) {
      safepoint.DefinePointerRegister(ToRegister(pointer), zone());
    }
  }
}

void LCodeGen::RecordSafepoint(LPointerMap* pointers,
                               Safepoint::DeoptMode deopt_mode) {
  RecordSafepoint(pointers, Safepoint::kSimple, 0, deopt_mode);
}

void LCodeGen::RecordSafepoint(Safepoint::DeoptMode deopt_mode) {
  LPointerMap empty_pointers(zone());
  RecordSafepoint(&empty_pointers, deopt_mode);
}

void LCodeGen::RecordSafepointWithRegisters(LPointerMap* pointers,
                                            int arguments,
                                            Safepoint::DeoptMode deopt_mode) {
  RecordSafepoint(pointers, Safepoint::kWithRegisters, arguments, deopt_mode);
}

static const char* LabelType(LLabel* label) {
  if (label->is_loop_header()) return " (loop header)";
  if (label->is_osr_entry()) return " (OSR entry)";
  return "";
}

void LCodeGen::DoLabel(LLabel* label) {
  Comment(";;; <@%d,#%d> -------------------- B%d%s --------------------",
          current_instruction_, label->hydrogen_value()->id(),
          label->block_id(), LabelType(label));
  __ bind(label->label());
  current_block_ = label->block_id();
  DoGap(label);
}

void LCodeGen::DoParallelMove(LParallelMove* move) { resolver_.Resolve(move); }

void LCodeGen::DoGap(LGap* gap) {
  for (int i = LGap::FIRST_INNER_POSITION; i <= LGap::LAST_INNER_POSITION;
       i++) {
    LGap::InnerPosition inner_pos = static_cast<LGap::InnerPosition>(i);
    LParallelMove* move = gap->GetParallelMove(inner_pos);
    if (move != NULL) DoParallelMove(move);
  }
}

void LCodeGen::DoInstructionGap(LInstructionGap* instr) { DoGap(instr); }

void LCodeGen::DoParameter(LParameter* instr) {
  // Nothing to do.
}

void LCodeGen::DoUnknownOSRValue(LUnknownOSRValue* instr) {
  GenerateOsrPrologue();
}

void LCodeGen::DoModByPowerOf2I(LModByPowerOf2I* instr) {
  Register dividend = ToRegister(instr->dividend());
  int32_t divisor = instr->divisor();
  DCHECK(dividend.is(ToRegister(instr->result())));

  // Theoretically, a variation of the branch-free code for integer division by
  // a power of 2 (calculating the remainder via an additional multiplication
  // (which gets simplified to an 'and') and subtraction) should be faster, and
  // this is exactly what GCC and clang emit. Nevertheless, benchmarks seem to
  // indicate that positive dividends are heavily favored, so the branching
  // version performs better.
  HMod* hmod = instr->hydrogen();
  int32_t shift = WhichPowerOf2Abs(divisor);
  Label dividend_is_not_negative, done;
  if (hmod->CheckFlag(HValue::kLeftCanBeNegative)) {
    __ CmpP(dividend, Operand::Zero());
    __ bge(&dividend_is_not_negative, Label::kNear);
    if (shift) {
      // Note that this is correct even for kMinInt operands.
      __ LoadComplementRR(dividend, dividend);
      __ ExtractBitRange(dividend, dividend, shift - 1, 0);
      __ LoadComplementRR(dividend, dividend);
      if (hmod->CheckFlag(HValue::kBailoutOnMinusZero)) {
        DeoptimizeIf(eq, instr, DeoptimizeReason::kMinusZero);
      }
    } else if (!hmod->CheckFlag(HValue::kBailoutOnMinusZero)) {
      __ mov(dividend, Operand::Zero());
    } else {
      DeoptimizeIf(al, instr, DeoptimizeReason::kMinusZero);
    }
    __ b(&done, Label::kNear);
  }

  __ bind(&dividend_is_not_negative);
  if (shift) {
    __ ExtractBitRange(dividend, dividend, shift - 1, 0);
  } else {
    __ mov(dividend, Operand::Zero());
  }
  __ bind(&done);
}

void LCodeGen::DoModByConstI(LModByConstI* instr) {
  Register dividend = ToRegister(instr->dividend());
  int32_t divisor = instr->divisor();
  Register result = ToRegister(instr->result());
  DCHECK(!dividend.is(result));

  if (divisor == 0) {
    DeoptimizeIf(al, instr, DeoptimizeReason::kDivisionByZero);
    return;
  }

  __ TruncatingDiv(result, dividend, Abs(divisor));
  __ mov(ip, Operand(Abs(divisor)));
  __ Mul(result, result, ip);
  __ SubP(result, dividend, result /*, LeaveOE, SetRC*/);

  // Check for negative zero.
  HMod* hmod = instr->hydrogen();
  if (hmod->CheckFlag(HValue::kBailoutOnMinusZero)) {
    Label remainder_not_zero;
    __ bne(&remainder_not_zero, Label::kNear /*, cr0*/);
    __ Cmp32(dividend, Operand::Zero());
    DeoptimizeIf(lt, instr, DeoptimizeReason::kMinusZero);
    __ bind(&remainder_not_zero);
  }
}

void LCodeGen::DoModI(LModI* instr) {
  HMod* hmod = instr->hydrogen();
  Register left_reg = ToRegister(instr->left());
  Register right_reg = ToRegister(instr->right());
  Register result_reg = ToRegister(instr->result());
  Label done;

  // Check for x % 0.
  if (hmod->CheckFlag(HValue::kCanBeDivByZero)) {
    __ Cmp32(right_reg, Operand::Zero());
    DeoptimizeIf(eq, instr, DeoptimizeReason::kDivisionByZero);
  }

  // Check for kMinInt % -1, dr will return undefined, which is not what we
  // want. We have to deopt if we care about -0, because we can't return that.
  if (hmod->CheckFlag(HValue::kCanOverflow)) {
    Label no_overflow_possible;
    __ Cmp32(left_reg, Operand(kMinInt));
    __ bne(&no_overflow_possible, Label::kNear);
    __ Cmp32(right_reg, Operand(-1));
    if (hmod->CheckFlag(HValue::kBailoutOnMinusZero)) {
      DeoptimizeIf(eq, instr, DeoptimizeReason::kMinusZero);
    } else {
      __ b(ne, &no_overflow_possible, Label::kNear);
      __ mov(result_reg, Operand::Zero());
      __ b(&done, Label::kNear);
    }
    __ bind(&no_overflow_possible);
  }

  // Divide instruction dr will implicity use register pair
  // r0 & r1 below.
  DCHECK(!left_reg.is(r1));
  DCHECK(!right_reg.is(r1));
  DCHECK(!result_reg.is(r1));
  __ LoadRR(r0, left_reg);
  __ srda(r0, Operand(32));
  __ dr(r0, right_reg);  // R0:R1 = R1 / divisor - R0 remainder

  __ LoadAndTestP_ExtendSrc(result_reg, r0);  // Copy remainder to resultreg

  // If we care about -0, test if the dividend is <0 and the result is 0.
  if (hmod->CheckFlag(HValue::kBailoutOnMinusZero)) {
    __ bne(&done, Label::kNear);
    __ Cmp32(left_reg, Operand::Zero());
    DeoptimizeIf(lt, instr, DeoptimizeReason::kMinusZero);
  }

  __ bind(&done);
}

void LCodeGen::DoDivByPowerOf2I(LDivByPowerOf2I* instr) {
  Register dividend = ToRegister(instr->dividend());
  int32_t divisor = instr->divisor();
  Register result = ToRegister(instr->result());
  DCHECK(divisor == kMinInt || base::bits::IsPowerOfTwo32(Abs(divisor)));
  DCHECK(!result.is(dividend));

  // Check for (0 / -x) that will produce negative zero.
  HDiv* hdiv = instr->hydrogen();
  if (hdiv->CheckFlag(HValue::kBailoutOnMinusZero) && divisor < 0) {
    __ Cmp32(dividend, Operand::Zero());
    DeoptimizeIf(eq, instr, DeoptimizeReason::kMinusZero);
  }
  // Check for (kMinInt / -1).
  if (hdiv->CheckFlag(HValue::kCanOverflow) && divisor == -1) {
    __ Cmp32(dividend, Operand(0x80000000));
    DeoptimizeIf(eq, instr, DeoptimizeReason::kOverflow);
  }

  int32_t shift = WhichPowerOf2Abs(divisor);

  // Deoptimize if remainder will not be 0.
  if (!hdiv->CheckFlag(HInstruction::kAllUsesTruncatingToInt32) && shift) {
    __ TestBitRange(dividend, shift - 1, 0, r0);
    DeoptimizeIf(ne, instr, DeoptimizeReason::kLostPrecision, cr0);
  }

  if (divisor == -1) {  // Nice shortcut, not needed for correctness.
    __ LoadComplementRR(result, dividend);
    return;
  }
  if (shift == 0) {
    __ LoadRR(result, dividend);
  } else {
    if (shift == 1) {
      __ ShiftRight(result, dividend, Operand(31));
    } else {
      __ ShiftRightArith(result, dividend, Operand(31));
      __ ShiftRight(result, result, Operand(32 - shift));
    }
    __ AddP(result, dividend, result);
    __ ShiftRightArith(result, result, Operand(shift));
#if V8_TARGET_ARCH_S390X
    __ lgfr(result, result);
#endif
  }
  if (divisor < 0) __ LoadComplementRR(result, result);
}

void LCodeGen::DoDivByConstI(LDivByConstI* instr) {
  Register dividend = ToRegister(instr->dividend());
  int32_t divisor = instr->divisor();
  Register result = ToRegister(instr->result());
  DCHECK(!dividend.is(result));

  if (divisor == 0) {
    DeoptimizeIf(al, instr, DeoptimizeReason::kDivisionByZero);
    return;
  }

  // Check for (0 / -x) that will produce negative zero.
  HDiv* hdiv = instr->hydrogen();
  if (hdiv->CheckFlag(HValue::kBailoutOnMinusZero) && divisor < 0) {
    __ Cmp32(dividend, Operand::Zero());
    DeoptimizeIf(eq, instr, DeoptimizeReason::kMinusZero);
  }

  __ TruncatingDiv(result, dividend, Abs(divisor));
  if (divisor < 0) __ LoadComplementRR(result, result);

  if (!hdiv->CheckFlag(HInstruction::kAllUsesTruncatingToInt32)) {
    Register scratch = scratch0();
    __ mov(ip, Operand(divisor));
    __ Mul(scratch, result, ip);
    __ Cmp32(scratch, dividend);
    DeoptimizeIf(ne, instr, DeoptimizeReason::kLostPrecision);
  }
}

// TODO(svenpanne) Refactor this to avoid code duplication with DoFlooringDivI.
void LCodeGen::DoDivI(LDivI* instr) {
  HBinaryOperation* hdiv = instr->hydrogen();
  const Register dividend = ToRegister(instr->dividend());
  const Register divisor = ToRegister(instr->divisor());
  Register result = ToRegister(instr->result());

  DCHECK(!dividend.is(result));
  DCHECK(!divisor.is(result));

  // Check for x / 0.
  if (hdiv->CheckFlag(HValue::kCanBeDivByZero)) {
    __ Cmp32(divisor, Operand::Zero());
    DeoptimizeIf(eq, instr, DeoptimizeReason::kDivisionByZero);
  }

  // Check for (0 / -x) that will produce negative zero.
  if (hdiv->CheckFlag(HValue::kBailoutOnMinusZero)) {
    Label dividend_not_zero;
    __ Cmp32(dividend, Operand::Zero());
    __ bne(&dividend_not_zero, Label::kNear);
    __ Cmp32(divisor, Operand::Zero());
    DeoptimizeIf(lt, instr, DeoptimizeReason::kMinusZero);
    __ bind(&dividend_not_zero);
  }

  // Check for (kMinInt / -1).
  if (hdiv->CheckFlag(HValue::kCanOverflow)) {
    Label dividend_not_min_int;
    __ Cmp32(dividend, Operand(kMinInt));
    __ bne(&dividend_not_min_int, Label::kNear);
    __ Cmp32(divisor, Operand(-1));
    DeoptimizeIf(eq, instr, DeoptimizeReason::kOverflow);
    __ bind(&dividend_not_min_int);
  }

  __ LoadRR(r0, dividend);
  __ srda(r0, Operand(32));
  __ dr(r0, divisor);  // R0:R1 = R1 / divisor - R0 remainder - R1 quotient

  __ LoadAndTestP_ExtendSrc(result, r1);  // Move quotient to result register

  if (!hdiv->CheckFlag(HInstruction::kAllUsesTruncatingToInt32)) {
    // Deoptimize if remainder is not 0.
    __ Cmp32(r0, Operand::Zero());
    DeoptimizeIf(ne, instr, DeoptimizeReason::kLostPrecision);
  }
}

void LCodeGen::DoFlooringDivByPowerOf2I(LFlooringDivByPowerOf2I* instr) {
  HBinaryOperation* hdiv = instr->hydrogen();
  Register dividend = ToRegister(instr->dividend());
  Register result = ToRegister(instr->result());
  int32_t divisor = instr->divisor();
  bool can_overflow = hdiv->CheckFlag(HValue::kLeftCanBeMinInt);

  // If the divisor is positive, things are easy: There can be no deopts and we
  // can simply do an arithmetic right shift.
  int32_t shift = WhichPowerOf2Abs(divisor);
  if (divisor > 0) {
    if (shift || !result.is(dividend)) {
      __ ShiftRightArith(result, dividend, Operand(shift));
#if V8_TARGET_ARCH_S390X
      __ lgfr(result, result);
#endif
    }
    return;
  }

// If the divisor is negative, we have to negate and handle edge cases.
#if V8_TARGET_ARCH_S390X
  if (divisor == -1 && can_overflow) {
    __ Cmp32(dividend, Operand(0x80000000));
    DeoptimizeIf(eq, instr, DeoptimizeReason::kOverflow);
  }
#endif

  __ LoadComplementRR(result, dividend);
  if (hdiv->CheckFlag(HValue::kBailoutOnMinusZero)) {
    DeoptimizeIf(eq, instr, DeoptimizeReason::kMinusZero, cr0);
  }

// If the negation could not overflow, simply shifting is OK.
#if !V8_TARGET_ARCH_S390X
  if (!can_overflow) {
#endif
    if (shift) {
      __ ShiftRightArithP(result, result, Operand(shift));
    }
    return;
#if !V8_TARGET_ARCH_S390X
  }

  // Dividing by -1 is basically negation, unless we overflow.
  if (divisor == -1) {
    DeoptimizeIf(overflow, instr, DeoptimizeReason::kOverflow, cr0);
    return;
  }

  Label overflow_label, done;
  __ b(overflow, &overflow_label, Label::kNear);
  __ ShiftRightArith(result, result, Operand(shift));
#if V8_TARGET_ARCH_S390X
  __ lgfr(result, result);
#endif
  __ b(&done, Label::kNear);
  __ bind(&overflow_label);
  __ mov(result, Operand(kMinInt / divisor));
  __ bind(&done);
#endif
}

void LCodeGen::DoFlooringDivByConstI(LFlooringDivByConstI* instr) {
  Register dividend = ToRegister(instr->dividend());
  int32_t divisor = instr->divisor();
  Register result = ToRegister(instr->result());
  DCHECK(!dividend.is(result));

  if (divisor == 0) {
    DeoptimizeIf(al, instr, DeoptimizeReason::kDivisionByZero);
    return;
  }

  // Check for (0 / -x) that will produce negative zero.
  HMathFloorOfDiv* hdiv = instr->hydrogen();
  if (hdiv->CheckFlag(HValue::kBailoutOnMinusZero) && divisor < 0) {
    __ Cmp32(dividend, Operand::Zero());
    DeoptimizeIf(eq, instr, DeoptimizeReason::kMinusZero);
  }

  // Easy case: We need no dynamic check for the dividend and the flooring
  // division is the same as the truncating division.
  if ((divisor > 0 && !hdiv->CheckFlag(HValue::kLeftCanBeNegative)) ||
      (divisor < 0 && !hdiv->CheckFlag(HValue::kLeftCanBePositive))) {
    __ TruncatingDiv(result, dividend, Abs(divisor));
    if (divisor < 0) __ LoadComplementRR(result, result);
    return;
  }

  // In the general case we may need to adjust before and after the truncating
  // division to get a flooring division.
  Register temp = ToRegister(instr->temp());
  DCHECK(!temp.is(dividend) && !temp.is(result));
  Label needs_adjustment, done;
  __ Cmp32(dividend, Operand::Zero());
  __ b(divisor > 0 ? lt : gt, &needs_adjustment);
  __ TruncatingDiv(result, dividend, Abs(divisor));
  if (divisor < 0) __ LoadComplementRR(result, result);
  __ b(&done, Label::kNear);
  __ bind(&needs_adjustment);
  __ AddP(temp, dividend, Operand(divisor > 0 ? 1 : -1));
  __ TruncatingDiv(result, temp, Abs(divisor));
  if (divisor < 0) __ LoadComplementRR(result, result);
  __ SubP(result, result, Operand(1));
  __ bind(&done);
}

// TODO(svenpanne) Refactor this to avoid code duplication with DoDivI.
void LCodeGen::DoFlooringDivI(LFlooringDivI* instr) {
  HBinaryOperation* hdiv = instr->hydrogen();
  const Register dividend = ToRegister(instr->dividend());
  const Register divisor = ToRegister(instr->divisor());
  Register result = ToRegister(instr->result());

  DCHECK(!dividend.is(result));
  DCHECK(!divisor.is(result));

  // Check for x / 0.
  if (hdiv->CheckFlag(HValue::kCanBeDivByZero)) {
    __ Cmp32(divisor, Operand::Zero());
    DeoptimizeIf(eq, instr, DeoptimizeReason::kDivisionByZero);
  }

  // Check for (0 / -x) that will produce negative zero.
  if (hdiv->CheckFlag(HValue::kBailoutOnMinusZero)) {
    Label dividend_not_zero;
    __ Cmp32(dividend, Operand::Zero());
    __ bne(&dividend_not_zero, Label::kNear);
    __ Cmp32(divisor, Operand::Zero());
    DeoptimizeIf(lt, instr, DeoptimizeReason::kMinusZero);
    __ bind(&dividend_not_zero);
  }

  // Check for (kMinInt / -1).
  if (hdiv->CheckFlag(HValue::kCanOverflow)) {
    Label no_overflow_possible;
    __ Cmp32(dividend, Operand(kMinInt));
    __ bne(&no_overflow_possible, Label::kNear);
    __ Cmp32(divisor, Operand(-1));
    if (!hdiv->CheckFlag(HValue::kAllUsesTruncatingToInt32)) {
      DeoptimizeIf(eq, instr, DeoptimizeReason::kOverflow);
    } else {
      __ bne(&no_overflow_possible, Label::kNear);
      __ LoadRR(result, dividend);
    }
    __ bind(&no_overflow_possible);
  }

  __ LoadRR(r0, dividend);
  __ srda(r0, Operand(32));
  __ dr(r0, divisor);  // R0:R1 = R1 / divisor - R0 remainder - R1 quotient

  __ lr(result, r1);  // Move quotient to result register

  Label done;
  Register scratch = scratch0();
  // If both operands have the same sign then we are done.
  __ Xor(scratch, dividend, divisor);
  __ ltr(scratch, scratch);  // use 32 bit version LoadAndTestRR even in 64 bit
  __ bge(&done, Label::kNear);

  // If there is no remainder then we are done.
  if (CpuFeatures::IsSupported(MISC_INSTR_EXT2)) {
    __ msrkc(scratch, result, divisor);
  } else {
    __ lr(scratch, result);
    __ msr(scratch, divisor);
  }
  __ Cmp32(dividend, scratch);
  __ beq(&done, Label::kNear);

  // We performed a truncating division. Correct the result.
  __ Sub32(result, result, Operand(1));
  __ bind(&done);
}

void LCodeGen::DoMultiplyAddD(LMultiplyAddD* instr) {
  DoubleRegister addend = ToDoubleRegister(instr->addend());
  DoubleRegister multiplier = ToDoubleRegister(instr->multiplier());
  DoubleRegister multiplicand = ToDoubleRegister(instr->multiplicand());
  DoubleRegister result = ToDoubleRegister(instr->result());

  // Unable to use madbr as the intermediate value is not rounded
  // to proper precision
  __ ldr(result, multiplier);
  __ mdbr(result, multiplicand);
  __ adbr(result, addend);
}

void LCodeGen::DoMultiplySubD(LMultiplySubD* instr) {
  DoubleRegister minuend = ToDoubleRegister(instr->minuend());
  DoubleRegister multiplier = ToDoubleRegister(instr->multiplier());
  DoubleRegister multiplicand = ToDoubleRegister(instr->multiplicand());
  DoubleRegister result = ToDoubleRegister(instr->result());

  // Unable to use msdbr as the intermediate value is not rounded
  // to proper precision
  __ ldr(result, multiplier);
  __ mdbr(result, multiplicand);
  __ sdbr(result, minuend);
}

void LCodeGen::DoMulI(LMulI* instr) {
  Register scratch = scratch0();
  Register result = ToRegister(instr->result());
  // Note that result may alias left.
  Register left = ToRegister(instr->left());
  LOperand* right_op = instr->right();

  bool bailout_on_minus_zero =
      instr->hydrogen()->CheckFlag(HValue::kBailoutOnMinusZero);
  bool can_overflow = instr->hydrogen()->CheckFlag(HValue::kCanOverflow);

  if (right_op->IsConstantOperand()) {
    int32_t constant = ToInteger32(LConstantOperand::cast(right_op));

    if (bailout_on_minus_zero && (constant < 0)) {
      // The case of a null constant will be handled separately.
      // If constant is negative and left is null, the result should be -0.
      __ CmpP(left, Operand::Zero());
      DeoptimizeIf(eq, instr, DeoptimizeReason::kMinusZero);
    }

    switch (constant) {
      case -1:
        if (can_overflow) {
#if V8_TARGET_ARCH_S390X
          if (instr->hydrogen()->representation().IsSmi()) {
#endif
            __ LoadComplementRR(result, left);
            DeoptimizeIf(overflow, instr, DeoptimizeReason::kOverflow);
#if V8_TARGET_ARCH_S390X
          } else {
            __ LoadComplementRR(result, left);
            __ TestIfInt32(result);
            DeoptimizeIf(ne, instr, DeoptimizeReason::kOverflow);
          }
#endif
        } else {
          __ LoadComplementRR(result, left);
        }
        break;
      case 0:
        if (bailout_on_minus_zero) {
// If left is strictly negative and the constant is null, the
// result is -0. Deoptimize if required, otherwise return 0.
#if V8_TARGET_ARCH_S390X
          if (instr->hydrogen()->representation().IsSmi()) {
#endif
            __ Cmp32(left, Operand::Zero());
#if V8_TARGET_ARCH_S390X
          } else {
            __ Cmp32(left, Operand::Zero());
          }
#endif
          DeoptimizeIf(lt, instr, DeoptimizeReason::kMinusZero);
        }
        __ LoadImmP(result, Operand::Zero());
        break;
      case 1:
        __ Move(result, left);
        break;
      default:
        // Multiplying by powers of two and powers of two plus or minus
        // one can be done faster with shifted operands.
        // For other constants we emit standard code.
        int32_t mask = constant >> 31;
        uint32_t constant_abs = (constant + mask) ^ mask;

        if (base::bits::IsPowerOfTwo32(constant_abs)) {
          int32_t shift = WhichPowerOf2(constant_abs);
          __ ShiftLeftP(result, left, Operand(shift));
          // Correct the sign of the result if the constant is negative.
          if (constant < 0) __ LoadComplementRR(result, result);
        } else if (base::bits::IsPowerOfTwo32(constant_abs - 1)) {
          int32_t shift = WhichPowerOf2(constant_abs - 1);
          __ ShiftLeftP(scratch, left, Operand(shift));
          __ AddP(result, scratch, left);
          // Correct the sign of the result if the constant is negative.
          if (constant < 0) __ LoadComplementRR(result, result);
        } else if (base::bits::IsPowerOfTwo32(constant_abs + 1)) {
          int32_t shift = WhichPowerOf2(constant_abs + 1);
          __ ShiftLeftP(scratch, left, Operand(shift));
          __ SubP(result, scratch, left);
          // Correct the sign of the result if the constant is negative.
          if (constant < 0) __ LoadComplementRR(result, result);
        } else {
          // Generate standard code.
          __ Move(result, left);
          __ MulP(result, Operand(constant));
        }
    }

  } else {
    DCHECK(right_op->IsRegister());
    Register right = ToRegister(right_op);

    if (can_overflow) {
      if (CpuFeatures::IsSupported(MISC_INSTR_EXT2)) {
        // result = left * right.
        if (instr->hydrogen()->representation().IsSmi()) {
          __ SmiUntag(scratch, right);
          __ MulPWithCondition(result, left, scratch);
        } else {
          __ msrkc(result, left, right);
          __ LoadW(result, result);
        }
        DeoptimizeIf(overflow, instr, DeoptimizeReason::kOverflow);
      } else {
#if V8_TARGET_ARCH_S390X
        // result = left * right.
        if (instr->hydrogen()->representation().IsSmi()) {
          __ SmiUntag(result, left);
          __ SmiUntag(scratch, right);
          __ msgr(result, scratch);
        } else {
          __ LoadRR(result, left);
          __ msgr(result, right);
        }
        __ TestIfInt32(result);
        DeoptimizeIf(ne, instr, DeoptimizeReason::kOverflow);
        if (instr->hydrogen()->representation().IsSmi()) {
          __ SmiTag(result);
        }
#else
        // r0:scratch = scratch * right
        if (instr->hydrogen()->representation().IsSmi()) {
          __ SmiUntag(result, left);
          __ lgfr(result, result);
          __ msgfr(result, right);
        } else {
          // r0:scratch = scratch * right
          __ lgfr(result, left);
          __ msgfr(result, right);
        }
        __ TestIfInt32(result);
        DeoptimizeIf(ne, instr, DeoptimizeReason::kOverflow);
#endif
      }
    } else {
      if (instr->hydrogen()->representation().IsSmi()) {
        __ SmiUntag(result, left);
        __ Mul(result, result, right);
      } else {
        __ Mul(result, left, right);
      }
    }

    if (bailout_on_minus_zero) {
      Label done;
#if V8_TARGET_ARCH_S390X
      if (instr->hydrogen()->representation().IsSmi()) {
#endif
        __ XorP(r0, left, right);
        __ LoadAndTestRR(r0, r0);
        __ bge(&done, Label::kNear);
#if V8_TARGET_ARCH_S390X
      } else {
        __ XorP(r0, left, right);
        __ Cmp32(r0, Operand::Zero());
        __ bge(&done, Label::kNear);
      }
#endif
      // Bail out if the result is minus zero.
      __ CmpP(result, Operand::Zero());
      DeoptimizeIf(eq, instr, DeoptimizeReason::kMinusZero);
      __ bind(&done);
    }
  }
}

void LCodeGen::DoBitI(LBitI* instr) {
  LOperand* left_op = instr->left();
  LOperand* right_op = instr->right();
  DCHECK(left_op->IsRegister());
  Register left = ToRegister(left_op);
  Register result = ToRegister(instr->result());

  if (right_op->IsConstantOperand()) {
    switch (instr->op()) {
      case Token::BIT_AND:
        __ AndP(result, left, Operand(ToOperand(right_op)));
        break;
      case Token::BIT_OR:
        __ OrP(result, left, Operand(ToOperand(right_op)));
        break;
      case Token::BIT_XOR:
        __ XorP(result, left, Operand(ToOperand(right_op)));
        break;
      default:
        UNREACHABLE();
        break;
    }
  } else if (right_op->IsStackSlot()) {
    // Reg-Mem instruction clobbers, so copy src to dst first.
    if (!left.is(result)) __ LoadRR(result, left);
    switch (instr->op()) {
      case Token::BIT_AND:
        __ AndP(result, ToMemOperand(right_op));
        break;
      case Token::BIT_OR:
        __ OrP(result, ToMemOperand(right_op));
        break;
      case Token::BIT_XOR:
        __ XorP(result, ToMemOperand(right_op));
        break;
      default:
        UNREACHABLE();
        break;
    }
  } else {
    DCHECK(right_op->IsRegister());

    switch (instr->op()) {
      case Token::BIT_AND:
        __ AndP(result, left, ToRegister(right_op));
        break;
      case Token::BIT_OR:
        __ OrP(result, left, ToRegister(right_op));
        break;
      case Token::BIT_XOR:
        __ XorP(result, left, ToRegister(right_op));
        break;
      default:
        UNREACHABLE();
        break;
    }
  }
}

void LCodeGen::DoShiftI(LShiftI* instr) {
  // Both 'left' and 'right' are "used at start" (see LCodeGen::DoShift), so
  // result may alias either of them.
  LOperand* right_op = instr->right();
  Register left = ToRegister(instr->left());
  Register result = ToRegister(instr->result());
  Register scratch = scratch0();
  if (right_op->IsRegister()) {
    // Mask the right_op operand.
    __ AndP(scratch, ToRegister(right_op), Operand(0x1F));
    switch (instr->op()) {
      case Token::ROR:
        // rotate_right(a, b) == rotate_left(a, 32 - b)
        __ LoadComplementRR(scratch, scratch);
        __ rll(result, left, scratch, Operand(32));
#if V8_TARGET_ARCH_S390X
        __ lgfr(result, result);
#endif
        break;
      case Token::SAR:
        __ ShiftRightArith(result, left, scratch);
#if V8_TARGET_ARCH_S390X
        __ lgfr(result, result);
#endif
        break;
      case Token::SHR:
        __ ShiftRight(result, left, scratch);
#if V8_TARGET_ARCH_S390X
        __ lgfr(result, result);
#endif
        if (instr->can_deopt()) {
#if V8_TARGET_ARCH_S390X
          __ ltgfr(result, result /*, SetRC*/);
#else
          __ ltr(result, result);  // Set the <,==,> condition
#endif
          DeoptimizeIf(lt, instr, DeoptimizeReason::kNegativeValue, cr0);
        }
        break;
      case Token::SHL:
        __ ShiftLeft(result, left, scratch);
#if V8_TARGET_ARCH_S390X
        __ lgfr(result, result);
#endif
        break;
      default:
        UNREACHABLE();
        break;
    }
  } else {
    // Mask the right_op operand.
    int value = ToInteger32(LConstantOperand::cast(right_op));
    uint8_t shift_count = static_cast<uint8_t>(value & 0x1F);
    switch (instr->op()) {
      case Token::ROR:
        if (shift_count != 0) {
          __ rll(result, left, Operand(32 - shift_count));
#if V8_TARGET_ARCH_S390X
          __ lgfr(result, result);
#endif
        } else {
          __ Move(result, left);
        }
        break;
      case Token::SAR:
        if (shift_count != 0) {
          __ ShiftRightArith(result, left, Operand(shift_count));
#if V8_TARGET_ARCH_S390X
          __ lgfr(result, result);
#endif
        } else {
          __ Move(result, left);
        }
        break;
      case Token::SHR:
        if (shift_count != 0) {
          __ ShiftRight(result, left, Operand(shift_count));
#if V8_TARGET_ARCH_S390X
          __ lgfr(result, result);
#endif
        } else {
          if (instr->can_deopt()) {
            __ Cmp32(left, Operand::Zero());
            DeoptimizeIf(lt, instr, DeoptimizeReason::kNegativeValue);
          }
          __ Move(result, left);
        }
        break;
      case Token::SHL:
        if (shift_count != 0) {
#if V8_TARGET_ARCH_S390X
          if (instr->hydrogen_value()->representation().IsSmi()) {
            __ ShiftLeftP(result, left, Operand(shift_count));
#else
          if (instr->hydrogen_value()->representation().IsSmi() &&
              instr->can_deopt()) {
            if (shift_count != 1) {
              __ ShiftLeft(result, left, Operand(shift_count - 1));
#if V8_TARGET_ARCH_S390X
              __ lgfr(result, result);
#endif
              __ SmiTagCheckOverflow(result, result, scratch);
            } else {
              __ SmiTagCheckOverflow(result, left, scratch);
            }
            DeoptimizeIf(lt, instr, DeoptimizeReason::kOverflow, cr0);
#endif
          } else {
            __ ShiftLeft(result, left, Operand(shift_count));
#if V8_TARGET_ARCH_S390X
            __ lgfr(result, result);
#endif
          }
        } else {
          __ Move(result, left);
        }
        break;
      default:
        UNREACHABLE();
        break;
    }
  }
}

void LCodeGen::DoSubI(LSubI* instr) {
  LOperand* left = instr->left();
  LOperand* right = instr->right();
  LOperand* result = instr->result();

  bool isInteger = !(instr->hydrogen()->representation().IsSmi() ||
                     instr->hydrogen()->representation().IsExternal());

#if V8_TARGET_ARCH_S390X
  // The overflow detection needs to be tested on the lower 32-bits.
  // As a result, on 64-bit, we need to force 32-bit arithmetic operations
  // to set the CC overflow bit properly.  The result is then sign-extended.
  bool checkOverflow = instr->hydrogen()->CheckFlag(HValue::kCanOverflow);
#else
  bool checkOverflow = true;
#endif

  if (right->IsConstantOperand()) {
    if (!isInteger || !checkOverflow) {
      __ SubP(ToRegister(result), ToRegister(left), ToOperand(right));
    } else {
      // -(MinInt) will overflow
      if (ToInteger32(LConstantOperand::cast(right)) == kMinInt) {
        __ Load(scratch0(), ToOperand(right));
        __ Sub32(ToRegister(result), ToRegister(left), scratch0());
      } else {
        __ Sub32(ToRegister(result), ToRegister(left), ToOperand(right));
      }
    }
  } else if (right->IsRegister()) {
    if (!isInteger)
      __ SubP(ToRegister(result), ToRegister(left), ToRegister(right));
    else if (!checkOverflow)
      __ SubP_ExtendSrc(ToRegister(result), ToRegister(left),
                        ToRegister(right));
    else
      __ Sub32(ToRegister(result), ToRegister(left), ToRegister(right));
  } else {
    if (!left->Equals(instr->result()))
      __ LoadRR(ToRegister(result), ToRegister(left));

    MemOperand mem = ToMemOperand(right);
    if (!isInteger) {
      __ SubP(ToRegister(result), mem);
    } else {
#if V8_TARGET_ARCH_S390X && !V8_TARGET_LITTLE_ENDIAN
      // We want to read the 32-bits directly from memory
      MemOperand Upper32Mem = MemOperand(mem.rb(), mem.rx(), mem.offset() + 4);
#else
      MemOperand Upper32Mem = ToMemOperand(right);
#endif
      if (checkOverflow) {
        __ Sub32(ToRegister(result), Upper32Mem);
      } else {
        __ SubP_ExtendSrc(ToRegister(result), Upper32Mem);
      }
    }
  }

#if V8_TARGET_ARCH_S390X
  if (isInteger && checkOverflow)
    __ lgfr(ToRegister(result), ToRegister(result));
#endif
  if (instr->hydrogen()->CheckFlag(HValue::kCanOverflow)) {
    DeoptimizeIf(overflow, instr, DeoptimizeReason::kOverflow);
  }
}

void LCodeGen::DoConstantI(LConstantI* instr) {
  Register dst = ToRegister(instr->result());
  if (instr->value() == 0)
    __ XorP(dst, dst);
  else
    __ Load(dst, Operand(instr->value()));
}

void LCodeGen::DoConstantS(LConstantS* instr) {
  __ LoadSmiLiteral(ToRegister(instr->result()), instr->value());
}

void LCodeGen::DoConstantD(LConstantD* instr) {
  DCHECK(instr->result()->IsDoubleRegister());
  DoubleRegister result = ToDoubleRegister(instr->result());
  uint64_t bits = instr->bits();
  __ LoadDoubleLiteral(result, bits, scratch0());
}

void LCodeGen::DoConstantE(LConstantE* instr) {
  __ mov(ToRegister(instr->result()), Operand(instr->value()));
}

void LCodeGen::DoConstantT(LConstantT* instr) {
  Handle<Object> object = instr->value(isolate());
  AllowDeferredHandleDereference smi_check;
  __ Move(ToRegister(instr->result()), object);
}

MemOperand LCodeGen::BuildSeqStringOperand(Register string, LOperand* index,
                                           String::Encoding encoding) {
  if (index->IsConstantOperand()) {
    int offset = ToInteger32(LConstantOperand::cast(index));
    if (encoding == String::TWO_BYTE_ENCODING) {
      offset *= kUC16Size;
    }
    STATIC_ASSERT(kCharSize == 1);
    return FieldMemOperand(string, SeqString::kHeaderSize + offset);
  }
  Register scratch = scratch0();
  DCHECK(!scratch.is(string));
  DCHECK(!scratch.is(ToRegister(index)));
  // TODO(joransiu) : Fold Add into FieldMemOperand
  if (encoding == String::ONE_BYTE_ENCODING) {
    __ AddP(scratch, string, ToRegister(index));
  } else {
    STATIC_ASSERT(kUC16Size == 2);
    __ ShiftLeftP(scratch, ToRegister(index), Operand(1));
    __ AddP(scratch, string, scratch);
  }
  return FieldMemOperand(scratch, SeqString::kHeaderSize);
}

void LCodeGen::DoSeqStringGetChar(LSeqStringGetChar* instr) {
  String::Encoding encoding = instr->hydrogen()->encoding();
  Register string = ToRegister(instr->string());
  Register result = ToRegister(instr->result());

  if (FLAG_debug_code) {
    Register scratch = scratch0();
    __ LoadP(scratch, FieldMemOperand(string, HeapObject::kMapOffset));
    __ llc(scratch, FieldMemOperand(scratch, Map::kInstanceTypeOffset));

    __ AndP(scratch, scratch,
            Operand(kStringRepresentationMask | kStringEncodingMask));
    static const uint32_t one_byte_seq_type = kSeqStringTag | kOneByteStringTag;
    static const uint32_t two_byte_seq_type = kSeqStringTag | kTwoByteStringTag;
    __ CmpP(scratch,
            Operand(encoding == String::ONE_BYTE_ENCODING ? one_byte_seq_type
                                                          : two_byte_seq_type));
    __ Check(eq, kUnexpectedStringType);
  }

  MemOperand operand = BuildSeqStringOperand(string, instr->index(), encoding);
  if (encoding == String::ONE_BYTE_ENCODING) {
    __ llc(result, operand);
  } else {
    __ llh(result, operand);
  }
}

void LCodeGen::DoSeqStringSetChar(LSeqStringSetChar* instr) {
  String::Encoding encoding = instr->hydrogen()->encoding();
  Register string = ToRegister(instr->string());
  Register value = ToRegister(instr->value());

  if (FLAG_debug_code) {
    Register index = ToRegister(instr->index());
    static const uint32_t one_byte_seq_type = kSeqStringTag | kOneByteStringTag;
    static const uint32_t two_byte_seq_type = kSeqStringTag | kTwoByteStringTag;
    int encoding_mask =
        instr->hydrogen()->encoding() == String::ONE_BYTE_ENCODING
            ? one_byte_seq_type
            : two_byte_seq_type;
    __ EmitSeqStringSetCharCheck(string, index, value, encoding_mask);
  }

  MemOperand operand = BuildSeqStringOperand(string, instr->index(), encoding);
  if (encoding == String::ONE_BYTE_ENCODING) {
    __ stc(value, operand);
  } else {
    __ sth(value, operand);
  }
}

void LCodeGen::DoAddI(LAddI* instr) {
  LOperand* left = instr->left();
  LOperand* right = instr->right();
  LOperand* result = instr->result();
  bool isInteger = !(instr->hydrogen()->representation().IsSmi() ||
                     instr->hydrogen()->representation().IsExternal());
#if V8_TARGET_ARCH_S390X
  // The overflow detection needs to be tested on the lower 32-bits.
  // As a result, on 64-bit, we need to force 32-bit arithmetic operations
  // to set the CC overflow bit properly.  The result is then sign-extended.
  bool checkOverflow = instr->hydrogen()->CheckFlag(HValue::kCanOverflow);
#else
  bool checkOverflow = true;
#endif

  if (right->IsConstantOperand()) {
    if (!isInteger || !checkOverflow)
      __ AddP(ToRegister(result), ToRegister(left), ToOperand(right));
    else
      __ Add32(ToRegister(result), ToRegister(left), ToOperand(right));
  } else if (right->IsRegister()) {
    if (!isInteger)
      __ AddP(ToRegister(result), ToRegister(left), ToRegister(right));
    else if (!checkOverflow)
      __ AddP_ExtendSrc(ToRegister(result), ToRegister(left),
                        ToRegister(right));
    else
      __ Add32(ToRegister(result), ToRegister(left), ToRegister(right));
  } else {
    if (!left->Equals(instr->result()))
      __ LoadRR(ToRegister(result), ToRegister(left));

    MemOperand mem = ToMemOperand(right);
    if (!isInteger) {
      __ AddP(ToRegister(result), mem);
    } else {
#if V8_TARGET_ARCH_S390X && !V8_TARGET_LITTLE_ENDIAN
      // We want to read the 32-bits directly from memory
      MemOperand Upper32Mem = MemOperand(mem.rb(), mem.rx(), mem.offset() + 4);
#else
      MemOperand Upper32Mem = ToMemOperand(right);
#endif
      if (checkOverflow) {
        __ Add32(ToRegister(result), Upper32Mem);
      } else {
        __ AddP_ExtendSrc(ToRegister(result), Upper32Mem);
      }
    }
  }

#if V8_TARGET_ARCH_S390X
  if (isInteger && checkOverflow)
    __ lgfr(ToRegister(result), ToRegister(result));
#endif
  // Doptimize on overflow
  if (instr->hydrogen()->CheckFlag(HValue::kCanOverflow)) {
    DeoptimizeIf(overflow, instr, DeoptimizeReason::kOverflow);
  }
}

void LCodeGen::DoMathMinMax(LMathMinMax* instr) {
  LOperand* left = instr->left();
  LOperand* right = instr->right();
  HMathMinMax::Operation operation = instr->hydrogen()->operation();
  Condition cond = (operation == HMathMinMax::kMathMin) ? le : ge;
  if (instr->hydrogen()->representation().IsSmiOrInteger32()) {
    Register left_reg = ToRegister(left);
    Register right_reg = EmitLoadRegister(right, ip);
    Register result_reg = ToRegister(instr->result());
    Label return_left, done;
#if V8_TARGET_ARCH_S390X
    if (instr->hydrogen_value()->representation().IsSmi()) {
#endif
      __ CmpP(left_reg, right_reg);
#if V8_TARGET_ARCH_S390X
    } else {
      __ Cmp32(left_reg, right_reg);
    }
#endif
    __ b(cond, &return_left, Label::kNear);
    __ Move(result_reg, right_reg);
    __ b(&done, Label::kNear);
    __ bind(&return_left);
    __ Move(result_reg, left_reg);
    __ bind(&done);
  } else {
    DCHECK(instr->hydrogen()->representation().IsDouble());
    DoubleRegister left_reg = ToDoubleRegister(left);
    DoubleRegister right_reg = ToDoubleRegister(right);
    DoubleRegister result_reg = ToDoubleRegister(instr->result());
    Label check_nan_left, check_zero, return_left, return_right, done;
    __ cdbr(left_reg, right_reg);
    __ bunordered(&check_nan_left, Label::kNear);
    __ beq(&check_zero);
    __ b(cond, &return_left, Label::kNear);
    __ b(&return_right, Label::kNear);

    __ bind(&check_zero);
    __ lzdr(kDoubleRegZero);
    __ cdbr(left_reg, kDoubleRegZero);
    __ bne(&return_left, Label::kNear);  // left == right != 0.

    // At this point, both left and right are either 0 or -0.
    // N.B. The following works because +0 + -0 == +0
    if (operation == HMathMinMax::kMathMin) {
      // For min we want logical-or of sign bit: -(-L + -R)
      __ lcdbr(left_reg, left_reg);
      __ ldr(result_reg, left_reg);
      if (left_reg.is(right_reg)) {
        __ adbr(result_reg, right_reg);
      } else {
        __ sdbr(result_reg, right_reg);
      }
      __ lcdbr(result_reg, result_reg);
    } else {
      // For max we want logical-and of sign bit: (L + R)
      __ ldr(result_reg, left_reg);
      __ adbr(result_reg, right_reg);
    }
    __ b(&done, Label::kNear);

    __ bind(&check_nan_left);
    __ cdbr(left_reg, left_reg);
    __ bunordered(&return_left, Label::kNear);  // left == NaN.

    __ bind(&return_right);
    if (!right_reg.is(result_reg)) {
      __ ldr(result_reg, right_reg);
    }
    __ b(&done, Label::kNear);

    __ bind(&return_left);
    if (!left_reg.is(result_reg)) {
      __ ldr(result_reg, left_reg);
    }
    __ bind(&done);
  }
}

void LCodeGen::DoArithmeticD(LArithmeticD* instr) {
  DoubleRegister left = ToDoubleRegister(instr->left());
  DoubleRegister right = ToDoubleRegister(instr->right());
  DoubleRegister result = ToDoubleRegister(instr->result());
  switch (instr->op()) {
    case Token::ADD:
      if (CpuFeatures::IsSupported(VECTOR_FACILITY)) {
        __ vfa(result, left, right);
      } else {
        DCHECK(result.is(left));
        __ adbr(result, right);
      }
      break;
    case Token::SUB:
      if (CpuFeatures::IsSupported(VECTOR_FACILITY)) {
        __ vfs(result, left, right);
      } else {
        DCHECK(result.is(left));
        __ sdbr(result, right);
      }
      break;
    case Token::MUL:
      if (CpuFeatures::IsSupported(VECTOR_FACILITY)) {
        __ vfm(result, left, right);
      } else {
        DCHECK(result.is(left));
        __ mdbr(result, right);
      }
      break;
    case Token::DIV:
      if (CpuFeatures::IsSupported(VECTOR_FACILITY)) {
        __ vfd(result, left, right);
      } else {
        DCHECK(result.is(left));
        __ ddbr(result, right);
      }
      break;
    case Token::MOD: {
      __ PrepareCallCFunction(0, 2, scratch0());
      __ MovToFloatParameters(left, right);
      __ CallCFunction(ExternalReference::mod_two_doubles_operation(isolate()),
                       0, 2);
      // Move the result in the double result register.
      __ MovFromFloatResult(result);
      break;
    }
    default:
      UNREACHABLE();
      break;
  }
}

void LCodeGen::DoArithmeticT(LArithmeticT* instr) {
  DCHECK(ToRegister(instr->context()).is(cp));
  DCHECK(ToRegister(instr->left()).is(r3));
  DCHECK(ToRegister(instr->right()).is(r2));
  DCHECK(ToRegister(instr->result()).is(r2));

  Handle<Code> code = CodeFactory::BinaryOpIC(isolate(), instr->op()).code();
  CallCode(code, RelocInfo::CODE_TARGET, instr);
}

template <class InstrType>
void LCodeGen::EmitBranch(InstrType instr, Condition cond) {
  int left_block = instr->TrueDestination(chunk_);
  int right_block = instr->FalseDestination(chunk_);

  int next_block = GetNextEmittedBlock();

  if (right_block == left_block || cond == al) {
    EmitGoto(left_block);
  } else if (left_block == next_block) {
    __ b(NegateCondition(cond), chunk_->GetAssemblyLabel(right_block));
  } else if (right_block == next_block) {
    __ b(cond, chunk_->GetAssemblyLabel(left_block));
  } else {
    __ b(cond, chunk_->GetAssemblyLabel(left_block));
    __ b(chunk_->GetAssemblyLabel(right_block));
  }
}

template <class InstrType>
void LCodeGen::EmitTrueBranch(InstrType instr, Condition cond) {
  int true_block = instr->TrueDestination(chunk_);
  __ b(cond, chunk_->GetAssemblyLabel(true_block));
}

template <class InstrType>
void LCodeGen::EmitFalseBranch(InstrType instr, Condition cond) {
  int false_block = instr->FalseDestination(chunk_);
  __ b(cond, chunk_->GetAssemblyLabel(false_block));
}

void LCodeGen::DoDebugBreak(LDebugBreak* instr) { __ stop("LBreak"); }

void LCodeGen::DoBranch(LBranch* instr) {
  Representation r = instr->hydrogen()->value()->representation();
  DoubleRegister dbl_scratch = double_scratch0();

  if (r.IsInteger32()) {
    DCHECK(!info()->IsStub());
    Register reg = ToRegister(instr->value());
    __ Cmp32(reg, Operand::Zero());
    EmitBranch(instr, ne);
  } else if (r.IsSmi()) {
    DCHECK(!info()->IsStub());
    Register reg = ToRegister(instr->value());
    __ CmpP(reg, Operand::Zero());
    EmitBranch(instr, ne);
  } else if (r.IsDouble()) {
    DCHECK(!info()->IsStub());
    DoubleRegister reg = ToDoubleRegister(instr->value());
    __ lzdr(kDoubleRegZero);
    __ cdbr(reg, kDoubleRegZero);
    // Test the double value. Zero and NaN are false.
    Condition lt_gt = static_cast<Condition>(lt | gt);

    EmitBranch(instr, lt_gt);
  } else {
    DCHECK(r.IsTagged());
    Register reg = ToRegister(instr->value());
    HType type = instr->hydrogen()->value()->type();
    if (type.IsBoolean()) {
      DCHECK(!info()->IsStub());
      __ CompareRoot(reg, Heap::kTrueValueRootIndex);
      EmitBranch(instr, eq);
    } else if (type.IsSmi()) {
      DCHECK(!info()->IsStub());
      __ CmpP(reg, Operand::Zero());
      EmitBranch(instr, ne);
    } else if (type.IsJSArray()) {
      DCHECK(!info()->IsStub());
      EmitBranch(instr, al);
    } else if (type.IsHeapNumber()) {
      DCHECK(!info()->IsStub());
      __ LoadDouble(dbl_scratch,
                    FieldMemOperand(reg, HeapNumber::kValueOffset));
      // Test the double value. Zero and NaN are false.
      __ lzdr(kDoubleRegZero);
      __ cdbr(dbl_scratch, kDoubleRegZero);
      Condition lt_gt = static_cast<Condition>(lt | gt);
      EmitBranch(instr, lt_gt);
    } else if (type.IsString()) {
      DCHECK(!info()->IsStub());
      __ LoadP(ip, FieldMemOperand(reg, String::kLengthOffset));
      __ CmpP(ip, Operand::Zero());
      EmitBranch(instr, ne);
    } else {
      ToBooleanHints expected = instr->hydrogen()->expected_input_types();
      // Avoid deopts in the case where we've never executed this path before.
      if (expected == ToBooleanHint::kNone) expected = ToBooleanHint::kAny;

      if (expected & ToBooleanHint::kUndefined) {
        // undefined -> false.
        __ CompareRoot(reg, Heap::kUndefinedValueRootIndex);
        __ beq(instr->FalseLabel(chunk_));
      }
      if (expected & ToBooleanHint::kBoolean) {
        // Boolean -> its value.
        __ CompareRoot(reg, Heap::kTrueValueRootIndex);
        __ beq(instr->TrueLabel(chunk_));
        __ CompareRoot(reg, Heap::kFalseValueRootIndex);
        __ beq(instr->FalseLabel(chunk_));
      }
      if (expected & ToBooleanHint::kNull) {
        // 'null' -> false.
        __ CompareRoot(reg, Heap::kNullValueRootIndex);
        __ beq(instr->FalseLabel(chunk_));
      }

      if (expected & ToBooleanHint::kSmallInteger) {
        // Smis: 0 -> false, all other -> true.
        __ CmpP(reg, Operand::Zero());
        __ beq(instr->FalseLabel(chunk_));
        __ JumpIfSmi(reg, instr->TrueLabel(chunk_));
      } else if (expected & ToBooleanHint::kNeedsMap) {
        // If we need a map later and have a Smi -> deopt.
        __ TestIfSmi(reg);
        DeoptimizeIf(eq, instr, DeoptimizeReason::kSmi, cr0);
      }

      const Register map = scratch0();
      if (expected & ToBooleanHint::kNeedsMap) {
        __ LoadP(map, FieldMemOperand(reg, HeapObject::kMapOffset));

        if (expected & ToBooleanHint::kCanBeUndetectable) {
          // Undetectable -> false.
          __ tm(FieldMemOperand(map, Map::kBitFieldOffset),
                Operand(1 << Map::kIsUndetectable));
          __ bne(instr->FalseLabel(chunk_));
        }
      }

      if (expected & ToBooleanHint::kReceiver) {
        // spec object -> true.
        __ CompareInstanceType(map, ip, FIRST_JS_RECEIVER_TYPE);
        __ bge(instr->TrueLabel(chunk_));
      }

      if (expected & ToBooleanHint::kString) {
        // String value -> false iff empty.
        Label not_string;
        __ CompareInstanceType(map, ip, FIRST_NONSTRING_TYPE);
        __ bge(&not_string, Label::kNear);
        __ LoadP(ip, FieldMemOperand(reg, String::kLengthOffset));
        __ CmpP(ip, Operand::Zero());
        __ bne(instr->TrueLabel(chunk_));
        __ b(instr->FalseLabel(chunk_));
        __ bind(&not_string);
      }

      if (expected & ToBooleanHint::kSymbol) {
        // Symbol value -> true.
        __ CompareInstanceType(map, ip, SYMBOL_TYPE);
        __ beq(instr->TrueLabel(chunk_));
      }

      if (expected & ToBooleanHint::kHeapNumber) {
        // heap number -> false iff +0, -0, or NaN.
        Label not_heap_number;
        __ CompareRoot(map, Heap::kHeapNumberMapRootIndex);
        __ bne(&not_heap_number, Label::kNear);
        __ LoadDouble(dbl_scratch,
                      FieldMemOperand(reg, HeapNumber::kValueOffset));
        __ lzdr(kDoubleRegZero);
        __ cdbr(dbl_scratch, kDoubleRegZero);
        __ bunordered(instr->FalseLabel(chunk_));  // NaN -> false.
        __ beq(instr->FalseLabel(chunk_));         // +0, -0 -> false.
        __ b(instr->TrueLabel(chunk_));
        __ bind(&not_heap_number);
      }

      if (expected != ToBooleanHint::kAny) {
        // We've seen something for the first time -> deopt.
        // This can only happen if we are not generic already.
        DeoptimizeIf(al, instr, DeoptimizeReason::kUnexpectedObject);
      }
    }
  }
}

void LCodeGen::EmitGoto(int block) {
  if (!IsNextEmittedBlock(block)) {
    __ b(chunk_->GetAssemblyLabel(LookupDestination(block)));
  }
}

void LCodeGen::DoGoto(LGoto* instr) { EmitGoto(instr->block_id()); }

Condition LCodeGen::TokenToCondition(Token::Value op) {
  Condition cond = kNoCondition;
  switch (op) {
    case Token::EQ:
    case Token::EQ_STRICT:
      cond = eq;
      break;
    case Token::NE:
    case Token::NE_STRICT:
      cond = ne;
      break;
    case Token::LT:
      cond = lt;
      break;
    case Token::GT:
      cond = gt;
      break;
    case Token::LTE:
      cond = le;
      break;
    case Token::GTE:
      cond = ge;
      break;
    case Token::IN:
    case Token::INSTANCEOF:
    default:
      UNREACHABLE();
  }
  return cond;
}

void LCodeGen::DoCompareNumericAndBranch(LCompareNumericAndBranch* instr) {
  LOperand* left = instr->left();
  LOperand* right = instr->right();
  bool is_unsigned =
      instr->hydrogen()->left()->CheckFlag(HInstruction::kUint32) ||
      instr->hydrogen()->right()->CheckFlag(HInstruction::kUint32);
  Condition cond = TokenToCondition(instr->op());

  if (left->IsConstantOperand() && right->IsConstantOperand()) {
    // We can statically evaluate the comparison.
    double left_val = ToDouble(LConstantOperand::cast(left));
    double right_val = ToDouble(LConstantOperand::cast(right));
    int next_block = Token::EvalComparison(instr->op(), left_val, right_val)
                         ? instr->TrueDestination(chunk_)
                         : instr->FalseDestination(chunk_);
    EmitGoto(next_block);
  } else {
    if (instr->is_double()) {
      // Compare left and right operands as doubles and load the
      // resulting flags into the normal status register.
      __ cdbr(ToDoubleRegister(left), ToDoubleRegister(right));
      // If a NaN is involved, i.e. the result is unordered,
      // jump to false block label.
      __ bunordered(instr->FalseLabel(chunk_));
    } else {
      if (right->IsConstantOperand()) {
        int32_t value = ToInteger32(LConstantOperand::cast(right));
        if (instr->hydrogen_value()->representation().IsSmi()) {
          if (is_unsigned) {
            __ CmpLogicalSmiLiteral(ToRegister(left), Smi::FromInt(value), r0);
          } else {
            __ CmpSmiLiteral(ToRegister(left), Smi::FromInt(value), r0);
          }
        } else {
          if (is_unsigned) {
            __ CmpLogical32(ToRegister(left), ToOperand(right));
          } else {
            __ Cmp32(ToRegister(left), ToOperand(right));
          }
        }
      } else if (left->IsConstantOperand()) {
        int32_t value = ToInteger32(LConstantOperand::cast(left));
        if (instr->hydrogen_value()->representation().IsSmi()) {
          if (is_unsigned) {
            __ CmpLogicalSmiLiteral(ToRegister(right), Smi::FromInt(value), r0);
          } else {
            __ CmpSmiLiteral(ToRegister(right), Smi::FromInt(value), r0);
          }
        } else {
          if (is_unsigned) {
            __ CmpLogical32(ToRegister(right), ToOperand(left));
          } else {
            __ Cmp32(ToRegister(right), ToOperand(left));
          }
        }
        // We commuted the operands, so commute the condition.
        cond = CommuteCondition(cond);
      } else if (instr->hydrogen_value()->representation().IsSmi()) {
        if (is_unsigned) {
          __ CmpLogicalP(ToRegister(left), ToRegister(right));
        } else {
          __ CmpP(ToRegister(left), ToRegister(right));
        }
      } else {
        if (is_unsigned) {
          __ CmpLogical32(ToRegister(left), ToRegister(right));
        } else {
          __ Cmp32(ToRegister(left), ToRegister(right));
        }
      }
    }
    EmitBranch(instr, cond);
  }
}

void LCodeGen::DoCmpObjectEqAndBranch(LCmpObjectEqAndBranch* instr) {
  Register left = ToRegister(instr->left());
  Register right = ToRegister(instr->right());

  __ CmpP(left, right);
  EmitBranch(instr, eq);
}

void LCodeGen::DoCmpHoleAndBranch(LCmpHoleAndBranch* instr) {
  if (instr->hydrogen()->representation().IsTagged()) {
    Register input_reg = ToRegister(instr->object());
    __ CmpP(input_reg, Operand(factory()->the_hole_value()));
    EmitBranch(instr, eq);
    return;
  }

  DoubleRegister input_reg = ToDoubleRegister(instr->object());
  __ cdbr(input_reg, input_reg);
  EmitFalseBranch(instr, ordered);

  Register scratch = scratch0();
  // Convert to GPR and examine the upper 32 bits
  __ lgdr(scratch, input_reg);
  __ srlg(scratch, scratch, Operand(32));
  __ Cmp32(scratch, Operand(kHoleNanUpper32));
  EmitBranch(instr, eq);
}

Condition LCodeGen::EmitIsString(Register input, Register temp1,
                                 Label* is_not_string,
                                 SmiCheck check_needed = INLINE_SMI_CHECK) {
  if (check_needed == INLINE_SMI_CHECK) {
    __ JumpIfSmi(input, is_not_string);
  }
  __ CompareObjectType(input, temp1, temp1, FIRST_NONSTRING_TYPE);

  return lt;
}

void LCodeGen::DoIsStringAndBranch(LIsStringAndBranch* instr) {
  Register reg = ToRegister(instr->value());
  Register temp1 = ToRegister(instr->temp());

  SmiCheck check_needed = instr->hydrogen()->value()->type().IsHeapObject()
                              ? OMIT_SMI_CHECK
                              : INLINE_SMI_CHECK;
  Condition true_cond =
      EmitIsString(reg, temp1, instr->FalseLabel(chunk_), check_needed);

  EmitBranch(instr, true_cond);
}

void LCodeGen::DoIsSmiAndBranch(LIsSmiAndBranch* instr) {
  Register input_reg = EmitLoadRegister(instr->value(), ip);
  __ TestIfSmi(input_reg);
  EmitBranch(instr, eq);
}

void LCodeGen::DoIsUndetectableAndBranch(LIsUndetectableAndBranch* instr) {
  Register input = ToRegister(instr->value());
  Register temp = ToRegister(instr->temp());

  if (!instr->hydrogen()->value()->type().IsHeapObject()) {
    __ JumpIfSmi(input, instr->FalseLabel(chunk_));
  }
  __ LoadP(temp, FieldMemOperand(input, HeapObject::kMapOffset));
  __ tm(FieldMemOperand(temp, Map::kBitFieldOffset),
        Operand(1 << Map::kIsUndetectable));
  EmitBranch(instr, ne);
}

static Condition ComputeCompareCondition(Token::Value op) {
  switch (op) {
    case Token::EQ_STRICT:
    case Token::EQ:
      return eq;
    case Token::LT:
      return lt;
    case Token::GT:
      return gt;
    case Token::LTE:
      return le;
    case Token::GTE:
      return ge;
    default:
      UNREACHABLE();
      return kNoCondition;
  }
}

void LCodeGen::DoStringCompareAndBranch(LStringCompareAndBranch* instr) {
  DCHECK(ToRegister(instr->context()).is(cp));
  DCHECK(ToRegister(instr->left()).is(r3));
  DCHECK(ToRegister(instr->right()).is(r2));

  Handle<Code> code = CodeFactory::StringCompare(isolate(), instr->op()).code();
  CallCode(code, RelocInfo::CODE_TARGET, instr);
  __ CompareRoot(r2, Heap::kTrueValueRootIndex);
  EmitBranch(instr, eq);
}

static InstanceType TestType(HHasInstanceTypeAndBranch* instr) {
  InstanceType from = instr->from();
  InstanceType to = instr->to();
  if (from == FIRST_TYPE) return to;
  DCHECK(from == to || to == LAST_TYPE);
  return from;
}

static Condition BranchCondition(HHasInstanceTypeAndBranch* instr) {
  InstanceType from = instr->from();
  InstanceType to = instr->to();
  if (from == to) return eq;
  if (to == LAST_TYPE) return ge;
  if (from == FIRST_TYPE) return le;
  UNREACHABLE();
  return eq;
}

void LCodeGen::DoHasInstanceTypeAndBranch(LHasInstanceTypeAndBranch* instr) {
  Register scratch = scratch0();
  Register input = ToRegister(instr->value());

  if (!instr->hydrogen()->value()->type().IsHeapObject()) {
    __ JumpIfSmi(input, instr->FalseLabel(chunk_));
  }

  __ CompareObjectType(input, scratch, scratch, TestType(instr->hydrogen()));
  EmitBranch(instr, BranchCondition(instr->hydrogen()));
}

// Branches to a label or falls through with the answer in flags.  Trashes
// the temp registers, but not the input.
void LCodeGen::EmitClassOfTest(Label* is_true, Label* is_false,
                               Handle<String> class_name, Register input,
                               Register temp, Register temp2) {
  DCHECK(!input.is(temp));
  DCHECK(!input.is(temp2));
  DCHECK(!temp.is(temp2));

  __ JumpIfSmi(input, is_false);

  __ CompareObjectType(input, temp, temp2, FIRST_FUNCTION_TYPE);
  STATIC_ASSERT(LAST_FUNCTION_TYPE == LAST_TYPE);
  if (String::Equals(isolate()->factory()->Function_string(), class_name)) {
    __ bge(is_true);
  } else {
    __ bge(is_false);
  }

  // Check if the constructor in the map is a function.
  Register instance_type = ip;
  __ GetMapConstructor(temp, temp, temp2, instance_type);

  // Objects with a non-function constructor have class 'Object'.
  __ CmpP(instance_type, Operand(JS_FUNCTION_TYPE));
  if (String::Equals(isolate()->factory()->Object_string(), class_name)) {
    __ bne(is_true);
  } else {
    __ bne(is_false);
  }

  // temp now contains the constructor function. Grab the
  // instance class name from there.
  __ LoadP(temp, FieldMemOperand(temp, JSFunction::kSharedFunctionInfoOffset));
  __ LoadP(temp,
           FieldMemOperand(temp, SharedFunctionInfo::kInstanceClassNameOffset));
  // The class name we are testing against is internalized since it's a literal.
  // The name in the constructor is internalized because of the way the context
  // is booted.  This routine isn't expected to work for random API-created
  // classes and it doesn't have to because you can't access it with natives
  // syntax.  Since both sides are internalized it is sufficient to use an
  // identity comparison.
  __ CmpP(temp, Operand(class_name));
  // End with the answer in flags.
}

void LCodeGen::DoClassOfTestAndBranch(LClassOfTestAndBranch* instr) {
  Register input = ToRegister(instr->value());
  Register temp = scratch0();
  Register temp2 = ToRegister(instr->temp());
  Handle<String> class_name = instr->hydrogen()->class_name();

  EmitClassOfTest(instr->TrueLabel(chunk_), instr->FalseLabel(chunk_),
                  class_name, input, temp, temp2);

  EmitBranch(instr, eq);
}

void LCodeGen::DoCmpMapAndBranch(LCmpMapAndBranch* instr) {
  Register reg = ToRegister(instr->value());
  Register temp = ToRegister(instr->temp());

  __ mov(temp, Operand(instr->map()));
  __ CmpP(temp, FieldMemOperand(reg, HeapObject::kMapOffset));
  EmitBranch(instr, eq);
}

void LCodeGen::DoHasInPrototypeChainAndBranch(
    LHasInPrototypeChainAndBranch* instr) {
  Register const object = ToRegister(instr->object());
  Register const object_map = scratch0();
  Register const object_instance_type = ip;
  Register const object_prototype = object_map;
  Register const prototype = ToRegister(instr->prototype());

  // The {object} must be a spec object.  It's sufficient to know that {object}
  // is not a smi, since all other non-spec objects have {null} prototypes and
  // will be ruled out below.
  if (instr->hydrogen()->ObjectNeedsSmiCheck()) {
    __ TestIfSmi(object);
    EmitFalseBranch(instr, eq);
  }
  // Loop through the {object}s prototype chain looking for the {prototype}.
  __ LoadP(object_map, FieldMemOperand(object, HeapObject::kMapOffset));
  Label loop;
  __ bind(&loop);

  // Deoptimize if the object needs to be access checked.
  __ LoadlB(object_instance_type,
            FieldMemOperand(object_map, Map::kBitFieldOffset));
  __ TestBit(object_instance_type, Map::kIsAccessCheckNeeded, r0);
  DeoptimizeIf(ne, instr, DeoptimizeReason::kAccessCheck, cr0);
  // Deoptimize for proxies.
  __ CompareInstanceType(object_map, object_instance_type, JS_PROXY_TYPE);
  DeoptimizeIf(eq, instr, DeoptimizeReason::kProxy);
  __ LoadP(object_prototype,
           FieldMemOperand(object_map, Map::kPrototypeOffset));
  __ CompareRoot(object_prototype, Heap::kNullValueRootIndex);
  EmitFalseBranch(instr, eq);
  __ CmpP(object_prototype, prototype);
  EmitTrueBranch(instr, eq);
  __ LoadP(object_map,
           FieldMemOperand(object_prototype, HeapObject::kMapOffset));
  __ b(&loop);
}

void LCodeGen::DoCmpT(LCmpT* instr) {
  DCHECK(ToRegister(instr->context()).is(cp));
  Token::Value op = instr->op();

  Handle<Code> ic = CodeFactory::CompareIC(isolate(), op).code();
  CallCode(ic, RelocInfo::CODE_TARGET, instr);
  // This instruction also signals no smi code inlined
  __ CmpP(r2, Operand::Zero());

  Condition condition = ComputeCompareCondition(op);
  Label true_value, done;

  __ b(condition, &true_value, Label::kNear);

  __ LoadRoot(ToRegister(instr->result()), Heap::kFalseValueRootIndex);
  __ b(&done, Label::kNear);

  __ bind(&true_value);
  __ LoadRoot(ToRegister(instr->result()), Heap::kTrueValueRootIndex);

  __ bind(&done);
}

void LCodeGen::DoReturn(LReturn* instr) {
  if (FLAG_trace && info()->IsOptimizing()) {
    // Push the return value on the stack as the parameter.
    // Runtime::TraceExit returns its parameter in r2.  We're leaving the code
    // managed by the register allocator and tearing down the frame, it's
    // safe to write to the context register.
    __ push(r2);
    __ LoadP(cp, MemOperand(fp, StandardFrameConstants::kContextOffset));
    __ CallRuntime(Runtime::kTraceExit);
  }
  if (info()->saves_caller_doubles()) {
    RestoreCallerDoubles();
  }
  if (instr->has_constant_parameter_count()) {
    int parameter_count = ToInteger32(instr->constant_parameter_count());
    int32_t sp_delta = (parameter_count + 1) * kPointerSize;
    if (NeedsEagerFrame()) {
      masm_->LeaveFrame(StackFrame::JAVA_SCRIPT, sp_delta);
    } else if (sp_delta != 0) {
      // TODO(joransiu): Clean this up into Macro Assembler
      if (sp_delta >= 0 && sp_delta < 4096)
        __ la(sp, MemOperand(sp, sp_delta));
      else
        __ lay(sp, MemOperand(sp, sp_delta));
    }
  } else {
    DCHECK(info()->IsStub());  // Functions would need to drop one more value.
    Register reg = ToRegister(instr->parameter_count());
    // The argument count parameter is a smi
    if (NeedsEagerFrame()) {
      masm_->LeaveFrame(StackFrame::JAVA_SCRIPT);
    }
    __ SmiToPtrArrayOffset(r0, reg);
    __ AddP(sp, sp, r0);
  }

  __ Ret();
}

void LCodeGen::DoLoadContextSlot(LLoadContextSlot* instr) {
  Register context = ToRegister(instr->context());
  Register result = ToRegister(instr->result());
  __ LoadP(result, ContextMemOperand(context, instr->slot_index()));
  if (instr->hydrogen()->RequiresHoleCheck()) {
    __ CompareRoot(result, Heap::kTheHoleValueRootIndex);
    if (instr->hydrogen()->DeoptimizesOnHole()) {
      DeoptimizeIf(eq, instr, DeoptimizeReason::kHole);
    } else {
      Label skip;
      __ bne(&skip, Label::kNear);
      __ mov(result, Operand(factory()->undefined_value()));
      __ bind(&skip);
    }
  }
}

void LCodeGen::DoStoreContextSlot(LStoreContextSlot* instr) {
  Register context = ToRegister(instr->context());
  Register value = ToRegister(instr->value());
  Register scratch = scratch0();
  MemOperand target = ContextMemOperand(context, instr->slot_index());

  Label skip_assignment;

  if (instr->hydrogen()->RequiresHoleCheck()) {
    __ LoadP(scratch, target);
    __ CompareRoot(scratch, Heap::kTheHoleValueRootIndex);
    if (instr->hydrogen()->DeoptimizesOnHole()) {
      DeoptimizeIf(eq, instr, DeoptimizeReason::kHole);
    } else {
      __ bne(&skip_assignment);
    }
  }

  __ StoreP(value, target);
  if (instr->hydrogen()->NeedsWriteBarrier()) {
    SmiCheck check_needed = instr->hydrogen()->value()->type().IsHeapObject()
                                ? OMIT_SMI_CHECK
                                : INLINE_SMI_CHECK;
    __ RecordWriteContextSlot(context, target.offset(), value, scratch,
                              GetLinkRegisterState(), kSaveFPRegs,
                              EMIT_REMEMBERED_SET, check_needed);
  }

  __ bind(&skip_assignment);
}

void LCodeGen::DoLoadNamedField(LLoadNamedField* instr) {
  HObjectAccess access = instr->hydrogen()->access();
  int offset = access.offset();
  Register object = ToRegister(instr->object());

  if (access.IsExternalMemory()) {
    Register result = ToRegister(instr->result());
    MemOperand operand = MemOperand(object, offset);
    __ LoadRepresentation(result, operand, access.representation(), r0);
    return;
  }

  if (instr->hydrogen()->representation().IsDouble()) {
    DCHECK(access.IsInobject());
    DoubleRegister result = ToDoubleRegister(instr->result());
    __ LoadDouble(result, FieldMemOperand(object, offset));
    return;
  }

  Register result = ToRegister(instr->result());
  if (!access.IsInobject()) {
    __ LoadP(result, FieldMemOperand(object, JSObject::kPropertiesOffset));
    object = result;
  }

  Representation representation = access.representation();

#if V8_TARGET_ARCH_S390X
  // 64-bit Smi optimization
  if (representation.IsSmi() &&
      instr->hydrogen()->representation().IsInteger32()) {
    // Read int value directly from upper half of the smi.
    offset = SmiWordOffset(offset);
    representation = Representation::Integer32();
  }
#endif

  __ LoadRepresentation(result, FieldMemOperand(object, offset), representation,
                        r0);
}

void LCodeGen::DoLoadFunctionPrototype(LLoadFunctionPrototype* instr) {
  Register scratch = scratch0();
  Register function = ToRegister(instr->function());
  Register result = ToRegister(instr->result());

  // Get the prototype or initial map from the function.
  __ LoadP(result,
           FieldMemOperand(function, JSFunction::kPrototypeOrInitialMapOffset));

  // Check that the function has a prototype or an initial map.
  __ CompareRoot(result, Heap::kTheHoleValueRootIndex);
  DeoptimizeIf(eq, instr, DeoptimizeReason::kHole);

  // If the function does not have an initial map, we're done.
  Label done;
  __ CompareObjectType(result, scratch, scratch, MAP_TYPE);
  __ bne(&done, Label::kNear);

  // Get the prototype from the initial map.
  __ LoadP(result, FieldMemOperand(result, Map::kPrototypeOffset));

  // All done.
  __ bind(&done);
}

void LCodeGen::DoLoadRoot(LLoadRoot* instr) {
  Register result = ToRegister(instr->result());
  __ LoadRoot(result, instr->index());
}

void LCodeGen::DoAccessArgumentsAt(LAccessArgumentsAt* instr) {
  Register arguments = ToRegister(instr->arguments());
  Register result = ToRegister(instr->result());
  // There are two words between the frame pointer and the last argument.
  // Subtracting from length accounts for one of them add one more.
  if (instr->length()->IsConstantOperand()) {
    int const_length = ToInteger32(LConstantOperand::cast(instr->length()));
    if (instr->index()->IsConstantOperand()) {
      int const_index = ToInteger32(LConstantOperand::cast(instr->index()));
      int index = (const_length - const_index) + 1;
      __ LoadP(result, MemOperand(arguments, index * kPointerSize));
    } else {
      Register index = ToRegister(instr->index());
      __ SubP(result, index, Operand(const_length + 1));
      __ LoadComplementRR(result, result);
      __ ShiftLeftP(result, result, Operand(kPointerSizeLog2));
      __ LoadP(result, MemOperand(arguments, result));
    }
  } else if (instr->index()->IsConstantOperand()) {
    Register length = ToRegister(instr->length());
    int const_index = ToInteger32(LConstantOperand::cast(instr->index()));
    int loc = const_index - 1;
    if (loc != 0) {
      __ SubP(result, length, Operand(loc));
      __ ShiftLeftP(result, result, Operand(kPointerSizeLog2));
      __ LoadP(result, MemOperand(arguments, result));
    } else {
      __ ShiftLeftP(result, length, Operand(kPointerSizeLog2));
      __ LoadP(result, MemOperand(arguments, result));
    }
  } else {
    Register length = ToRegister(instr->length());
    Register index = ToRegister(instr->index());
    __ SubP(result, length, index);
    __ AddP(result, result, Operand(1));
    __ ShiftLeftP(result, result, Operand(kPointerSizeLog2));
    __ LoadP(result, MemOperand(arguments, result));
  }
}

void LCodeGen::DoLoadKeyedExternalArray(LLoadKeyed* instr) {
  Register external_pointer = ToRegister(instr->elements());
  Register key = no_reg;
  ElementsKind elements_kind = instr->elements_kind();
  bool key_is_constant = instr->key()->IsConstantOperand();
  int constant_key = 0;
  if (key_is_constant) {
    constant_key = ToInteger32(LConstantOperand::cast(instr->key()));
    if (constant_key & 0xF0000000) {
      Abort(kArrayIndexConstantValueTooBig);
    }
  } else {
    key = ToRegister(instr->key());
  }
  int element_size_shift = ElementsKindToShiftSize(elements_kind);
  bool key_is_smi = instr->hydrogen()->key()->representation().IsSmi();
  bool keyMaybeNegative = instr->hydrogen()->IsDehoisted();
  int base_offset = instr->base_offset();
  bool use_scratch = false;

  if (elements_kind == FLOAT32_ELEMENTS || elements_kind == FLOAT64_ELEMENTS) {
    DoubleRegister result = ToDoubleRegister(instr->result());
    if (key_is_constant) {
      base_offset += constant_key << element_size_shift;
      if (!is_int20(base_offset)) {
        __ mov(scratch0(), Operand(base_offset));
        base_offset = 0;
        use_scratch = true;
      }
    } else {
      __ IndexToArrayOffset(scratch0(), key, element_size_shift, key_is_smi,
                            keyMaybeNegative);
      use_scratch = true;
    }
    if (elements_kind == FLOAT32_ELEMENTS) {
      if (!use_scratch) {
        __ ldeb(result, MemOperand(external_pointer, base_offset));
      } else {
        __ ldeb(result, MemOperand(scratch0(), external_pointer, base_offset));
      }
    } else {  // i.e. elements_kind == EXTERNAL_DOUBLE_ELEMENTS
      if (!use_scratch) {
        __ LoadDouble(result, MemOperand(external_pointer, base_offset));
      } else {
        __ LoadDouble(result,
                      MemOperand(scratch0(), external_pointer, base_offset));
      }
    }
  } else {
    Register result = ToRegister(instr->result());
    MemOperand mem_operand =
        PrepareKeyedOperand(key, external_pointer, key_is_constant, key_is_smi,
                            constant_key, element_size_shift, base_offset,
                            keyMaybeNegative);
    switch (elements_kind) {
      case INT8_ELEMENTS:
        __ LoadB(result, mem_operand);
        break;
      case UINT8_ELEMENTS:
      case UINT8_CLAMPED_ELEMENTS:
        __ LoadlB(result, mem_operand);
        break;
      case INT16_ELEMENTS:
        __ LoadHalfWordP(result, mem_operand);
        break;
      case UINT16_ELEMENTS:
        __ LoadLogicalHalfWordP(result, mem_operand);
        break;
      case INT32_ELEMENTS:
        __ LoadW(result, mem_operand, r0);
        break;
      case UINT32_ELEMENTS:
        __ LoadlW(result, mem_operand, r0);
        if (!instr->hydrogen()->CheckFlag(HInstruction::kUint32)) {
          __ CmpLogical32(result, Operand(0x80000000));
          DeoptimizeIf(ge, instr, DeoptimizeReason::kNegativeValue);
        }
        break;
      case FLOAT32_ELEMENTS:
      case FLOAT64_ELEMENTS:
      case FAST_HOLEY_DOUBLE_ELEMENTS:
      case FAST_HOLEY_ELEMENTS:
      case FAST_HOLEY_SMI_ELEMENTS:
      case FAST_DOUBLE_ELEMENTS:
      case FAST_ELEMENTS:
      case FAST_SMI_ELEMENTS:
      case DICTIONARY_ELEMENTS:
      case FAST_SLOPPY_ARGUMENTS_ELEMENTS:
      case SLOW_SLOPPY_ARGUMENTS_ELEMENTS:
      case FAST_STRING_WRAPPER_ELEMENTS:
      case SLOW_STRING_WRAPPER_ELEMENTS:
      case NO_ELEMENTS:
        UNREACHABLE();
        break;
    }
  }
}

void LCodeGen::DoLoadKeyedFixedDoubleArray(LLoadKeyed* instr) {
  Register elements = ToRegister(instr->elements());
  bool key_is_constant = instr->key()->IsConstantOperand();
  Register key = no_reg;
  DoubleRegister result = ToDoubleRegister(instr->result());
  Register scratch = scratch0();

  int element_size_shift = ElementsKindToShiftSize(FAST_DOUBLE_ELEMENTS);
  bool key_is_smi = instr->hydrogen()->key()->representation().IsSmi();
  bool keyMaybeNegative = instr->hydrogen()->IsDehoisted();
  int constant_key = 0;
  if (key_is_constant) {
    constant_key = ToInteger32(LConstantOperand::cast(instr->key()));
    if (constant_key & 0xF0000000) {
      Abort(kArrayIndexConstantValueTooBig);
    }
  } else {
    key = ToRegister(instr->key());
  }

  bool use_scratch = false;
  intptr_t base_offset = instr->base_offset() + constant_key * kDoubleSize;
  if (!key_is_constant) {
    use_scratch = true;
    __ IndexToArrayOffset(scratch, key, element_size_shift, key_is_smi,
                          keyMaybeNegative);
  }

  // Memory references support up to 20-bits signed displacement in RXY form
  // Include Register::kExponentOffset in check, so we are guaranteed not to
  // overflow displacement later.
  if (!is_int20(base_offset + Register::kExponentOffset)) {
    use_scratch = true;
    if (key_is_constant) {
      __ mov(scratch, Operand(base_offset));
    } else {
      __ AddP(scratch, Operand(base_offset));
    }
    base_offset = 0;
  }

  if (!use_scratch) {
    __ LoadDouble(result, MemOperand(elements, base_offset));
  } else {
    __ LoadDouble(result, MemOperand(scratch, elements, base_offset));
  }

  if (instr->hydrogen()->RequiresHoleCheck()) {
    if (!use_scratch) {
      __ LoadlW(r0,
                MemOperand(elements, base_offset + Register::kExponentOffset));
    } else {
      __ LoadlW(r0, MemOperand(scratch, elements,
                               base_offset + Register::kExponentOffset));
    }
    __ Cmp32(r0, Operand(kHoleNanUpper32));
    DeoptimizeIf(eq, instr, DeoptimizeReason::kHole);
  }
}

void LCodeGen::DoLoadKeyedFixedArray(LLoadKeyed* instr) {
  HLoadKeyed* hinstr = instr->hydrogen();
  Register elements = ToRegister(instr->elements());
  Register result = ToRegister(instr->result());
  Register scratch = scratch0();
  int offset = instr->base_offset();

  if (instr->key()->IsConstantOperand()) {
    LConstantOperand* const_operand = LConstantOperand::cast(instr->key());
    offset += ToInteger32(const_operand) * kPointerSize;
  } else {
    Register key = ToRegister(instr->key());
    // Even though the HLoadKeyed instruction forces the input
    // representation for the key to be an integer, the input gets replaced
    // during bound check elimination with the index argument to the bounds
    // check, which can be tagged, so that case must be handled here, too.
    if (hinstr->key()->representation().IsSmi()) {
      __ SmiToPtrArrayOffset(scratch, key);
    } else {
      __ ShiftLeftP(scratch, key, Operand(kPointerSizeLog2));
    }
  }

  bool requires_hole_check = hinstr->RequiresHoleCheck();
  Representation representation = hinstr->representation();

#if V8_TARGET_ARCH_S390X
  // 64-bit Smi optimization
  if (representation.IsInteger32() &&
      hinstr->elements_kind() == FAST_SMI_ELEMENTS) {
    DCHECK(!requires_hole_check);
    // Read int value directly from upper half of the smi.
    offset = SmiWordOffset(offset);
  }
#endif

  if (instr->key()->IsConstantOperand()) {
    __ LoadRepresentation(result, MemOperand(elements, offset), representation,
                          r1);
  } else {
    __ LoadRepresentation(result, MemOperand(scratch, elements, offset),
                          representation, r1);
  }

  // Check for the hole value.
  if (requires_hole_check) {
    if (IsFastSmiElementsKind(hinstr->elements_kind())) {
      __ TestIfSmi(result);
      DeoptimizeIf(ne, instr, DeoptimizeReason::kNotASmi, cr0);
    } else {
      __ CompareRoot(result, Heap::kTheHoleValueRootIndex);
      DeoptimizeIf(eq, instr, DeoptimizeReason::kHole);
    }
  } else if (instr->hydrogen()->hole_mode() == CONVERT_HOLE_TO_UNDEFINED) {
    DCHECK(instr->hydrogen()->elements_kind() == FAST_HOLEY_ELEMENTS);
    Label done;
    __ LoadRoot(scratch, Heap::kTheHoleValueRootIndex);
    __ CmpP(result, scratch);
    __ bne(&done);
    if (info()->IsStub()) {
      // A stub can safely convert the hole to undefined only if the array
      // protector cell contains (Smi) Isolate::kProtectorValid. Otherwise
      // it needs to bail out.
      __ LoadRoot(result, Heap::kArrayProtectorRootIndex);
      __ LoadP(result, FieldMemOperand(result, PropertyCell::kValueOffset));
      __ CmpSmiLiteral(result, Smi::FromInt(Isolate::kProtectorValid), r0);
      DeoptimizeIf(ne, instr, DeoptimizeReason::kHole);
    }
    __ LoadRoot(result, Heap::kUndefinedValueRootIndex);
    __ bind(&done);
  }
}

void LCodeGen::DoLoadKeyed(LLoadKeyed* instr) {
  if (instr->is_fixed_typed_array()) {
    DoLoadKeyedExternalArray(instr);
  } else if (instr->hydrogen()->representation().IsDouble()) {
    DoLoadKeyedFixedDoubleArray(instr);
  } else {
    DoLoadKeyedFixedArray(instr);
  }
}

MemOperand LCodeGen::PrepareKeyedOperand(Register key, Register base,
                                         bool key_is_constant, bool key_is_smi,
                                         int constant_key,
                                         int element_size_shift,
                                         int base_offset,
                                         bool keyMaybeNegative) {
  Register scratch = scratch0();

  if (key_is_constant) {
    int offset = (base_offset + (constant_key << element_size_shift));
    if (!is_int20(offset)) {
      __ mov(scratch, Operand(offset));
      return MemOperand(base, scratch);
    } else {
      return MemOperand(base,
                        (constant_key << element_size_shift) + base_offset);
    }
  }

  bool needs_shift =
      (element_size_shift != (key_is_smi ? kSmiTagSize + kSmiShiftSize : 0));

  if (needs_shift) {
    __ IndexToArrayOffset(scratch, key, element_size_shift, key_is_smi,
                          keyMaybeNegative);
  } else {
    scratch = key;
  }

  if (!is_int20(base_offset)) {
    __ AddP(scratch, Operand(base_offset));
    base_offset = 0;
  }
  return MemOperand(scratch, base, base_offset);
}

void LCodeGen::DoArgumentsElements(LArgumentsElements* instr) {
  Register scratch = scratch0();
  Register result = ToRegister(instr->result());

  if (instr->hydrogen()->from_inlined()) {
    __ lay(result, MemOperand(sp, -2 * kPointerSize));
  } else if (instr->hydrogen()->arguments_adaptor()) {
    // Check if the calling frame is an arguments adaptor frame.
    Label done, adapted;
    __ LoadP(scratch, MemOperand(fp, StandardFrameConstants::kCallerFPOffset));
    __ LoadP(
        result,
        MemOperand(scratch, CommonFrameConstants::kContextOrFrameTypeOffset));
    __ CmpP(result,
            Operand(StackFrame::TypeToMarker(StackFrame::ARGUMENTS_ADAPTOR)));

    // Result is the frame pointer for the frame if not adapted and for the real
    // frame below the adaptor frame if adapted.
    __ beq(&adapted, Label::kNear);
    __ LoadRR(result, fp);
    __ b(&done, Label::kNear);

    __ bind(&adapted);
    __ LoadRR(result, scratch);
    __ bind(&done);
  } else {
    __ LoadRR(result, fp);
  }
}

void LCodeGen::DoArgumentsLength(LArgumentsLength* instr) {
  Register elem = ToRegister(instr->elements());
  Register result = ToRegister(instr->result());

  Label done;

  // If no arguments adaptor frame the number of arguments is fixed.
  __ CmpP(fp, elem);
  __ mov(result, Operand(scope()->num_parameters()));
  __ beq(&done, Label::kNear);

  // Arguments adaptor frame present. Get argument length from there.
  __ LoadP(result, MemOperand(fp, StandardFrameConstants::kCallerFPOffset));
  __ LoadP(result,
           MemOperand(result, ArgumentsAdaptorFrameConstants::kLengthOffset));
  __ SmiUntag(result);

  // Argument length is in result register.
  __ bind(&done);
}

void LCodeGen::DoWrapReceiver(LWrapReceiver* instr) {
  Register receiver = ToRegister(instr->receiver());
  Register function = ToRegister(instr->function());
  Register result = ToRegister(instr->result());
  Register scratch = scratch0();

  // If the receiver is null or undefined, we have to pass the global
  // object as a receiver to normal functions. Values have to be
  // passed unchanged to builtins and strict-mode functions.
  Label global_object, result_in_receiver;

  if (!instr->hydrogen()->known_function()) {
    // Do not transform the receiver to object for strict mode
    // functions or builtins.
    __ LoadP(scratch,
             FieldMemOperand(function, JSFunction::kSharedFunctionInfoOffset));
    __ LoadlW(scratch, FieldMemOperand(
                           scratch, SharedFunctionInfo::kCompilerHintsOffset));
    __ AndP(r0, scratch, Operand((1 << SharedFunctionInfo::kStrictModeBit) |
                                 (1 << SharedFunctionInfo::kNativeBit)));
    __ bne(&result_in_receiver, Label::kNear);
  }

  // Normal function. Replace undefined or null with global receiver.
  __ CompareRoot(receiver, Heap::kNullValueRootIndex);
  __ beq(&global_object, Label::kNear);
  __ CompareRoot(receiver, Heap::kUndefinedValueRootIndex);
  __ beq(&global_object, Label::kNear);

  // Deoptimize if the receiver is not a JS object.
  __ TestIfSmi(receiver);
  DeoptimizeIf(eq, instr, DeoptimizeReason::kSmi, cr0);
  __ CompareObjectType(receiver, scratch, scratch, FIRST_JS_RECEIVER_TYPE);
  DeoptimizeIf(lt, instr, DeoptimizeReason::kNotAJavaScriptObject);

  __ b(&result_in_receiver, Label::kNear);
  __ bind(&global_object);
  __ LoadP(result, FieldMemOperand(function, JSFunction::kContextOffset));
  __ LoadP(result, ContextMemOperand(result, Context::NATIVE_CONTEXT_INDEX));
  __ LoadP(result, ContextMemOperand(result, Context::GLOBAL_PROXY_INDEX));

  if (result.is(receiver)) {
    __ bind(&result_in_receiver);
  } else {
    Label result_ok;
    __ b(&result_ok, Label::kNear);
    __ bind(&result_in_receiver);
    __ LoadRR(result, receiver);
    __ bind(&result_ok);
  }
}

void LCodeGen::DoApplyArguments(LApplyArguments* instr) {
  Register receiver = ToRegister(instr->receiver());
  Register function = ToRegister(instr->function());
  Register length = ToRegister(instr->length());
  Register elements = ToRegister(instr->elements());
  Register scratch = scratch0();
  DCHECK(receiver.is(r2));  // Used for parameter count.
  DCHECK(function.is(r3));  // Required by InvokeFunction.
  DCHECK(ToRegister(instr->result()).is(r2));

  // Copy the arguments to this function possibly from the
  // adaptor frame below it.
  const uint32_t kArgumentsLimit = 1 * KB;
  __ CmpLogicalP(length, Operand(kArgumentsLimit));
  DeoptimizeIf(gt, instr, DeoptimizeReason::kTooManyArguments);

  // Push the receiver and use the register to keep the original
  // number of arguments.
  __ push(receiver);
  __ LoadRR(receiver, length);
  // The arguments are at a one pointer size offset from elements.
  __ AddP(elements, Operand(1 * kPointerSize));

  // Loop through the arguments pushing them onto the execution
  // stack.
  Label invoke, loop;
  // length is a small non-negative integer, due to the test above.
  __ CmpP(length, Operand::Zero());
  __ beq(&invoke, Label::kNear);
  __ bind(&loop);
  __ ShiftLeftP(r1, length, Operand(kPointerSizeLog2));
  __ LoadP(scratch, MemOperand(elements, r1));
  __ push(scratch);
  __ BranchOnCount(length, &loop);

  __ bind(&invoke);

  InvokeFlag flag = CALL_FUNCTION;
  if (instr->hydrogen()->tail_call_mode() == TailCallMode::kAllow) {
    DCHECK(!info()->saves_caller_doubles());
    // TODO(ishell): drop current frame before pushing arguments to the stack.
    flag = JUMP_FUNCTION;
    ParameterCount actual(r2);
    // It is safe to use r5, r6 and r7 as scratch registers here given that
    // 1) we are not going to return to caller function anyway,
    // 2) r5 (new.target) will be initialized below.
    PrepareForTailCall(actual, r5, r6, r7);
  }

  DCHECK(instr->HasPointerMap());
  LPointerMap* pointers = instr->pointer_map();
  SafepointGenerator safepoint_generator(this, pointers, Safepoint::kLazyDeopt);
  // The number of arguments is stored in receiver which is r2, as expected
  // by InvokeFunction.
  ParameterCount actual(receiver);
  __ InvokeFunction(function, no_reg, actual, flag, safepoint_generator);
}

void LCodeGen::DoPushArgument(LPushArgument* instr) {
  LOperand* argument = instr->value();
  if (argument->IsDoubleRegister() || argument->IsDoubleStackSlot()) {
    Abort(kDoPushArgumentNotImplementedForDoubleType);
  } else {
    Register argument_reg = EmitLoadRegister(argument, ip);
    __ push(argument_reg);
  }
}

void LCodeGen::DoDrop(LDrop* instr) { __ Drop(instr->count()); }

void LCodeGen::DoThisFunction(LThisFunction* instr) {
  Register result = ToRegister(instr->result());
  __ LoadP(result, MemOperand(fp, JavaScriptFrameConstants::kFunctionOffset));
}

void LCodeGen::DoContext(LContext* instr) {
  // If there is a non-return use, the context must be moved to a register.
  Register result = ToRegister(instr->result());
  if (info()->IsOptimizing()) {
    __ LoadP(result, MemOperand(fp, StandardFrameConstants::kContextOffset));
  } else {
    // If there is no frame, the context must be in cp.
    DCHECK(result.is(cp));
  }
}

void LCodeGen::DoDeclareGlobals(LDeclareGlobals* instr) {
  DCHECK(ToRegister(instr->context()).is(cp));
  __ Move(scratch0(), instr->hydrogen()->declarations());
  __ push(scratch0());
  __ LoadSmiLiteral(scratch0(), Smi::FromInt(instr->hydrogen()->flags()));
  __ push(scratch0());
  __ Move(scratch0(), instr->hydrogen()->feedback_vector());
  __ push(scratch0());
  CallRuntime(Runtime::kDeclareGlobals, instr);
}

void LCodeGen::CallKnownFunction(Handle<JSFunction> function,
                                 int formal_parameter_count, int arity,
                                 bool is_tail_call, LInstruction* instr) {
  bool dont_adapt_arguments =
      formal_parameter_count == SharedFunctionInfo::kDontAdaptArgumentsSentinel;
  bool can_invoke_directly =
      dont_adapt_arguments || formal_parameter_count == arity;

  Register function_reg = r3;

  LPointerMap* pointers = instr->pointer_map();

  if (can_invoke_directly) {
    // Change context.
    __ LoadP(cp, FieldMemOperand(function_reg, JSFunction::kContextOffset));

    // Always initialize new target and number of actual arguments.
    __ LoadRoot(r5, Heap::kUndefinedValueRootIndex);
    __ mov(r2, Operand(arity));

    bool is_self_call = function.is_identical_to(info()->closure());

    // Invoke function.
    if (is_self_call) {
      Handle<Code> self(reinterpret_cast<Code**>(__ CodeObject().location()));
      if (is_tail_call) {
        __ Jump(self, RelocInfo::CODE_TARGET);
      } else {
        __ Call(self, RelocInfo::CODE_TARGET);
      }
    } else {
      __ LoadP(ip, FieldMemOperand(function_reg, JSFunction::kCodeEntryOffset));
      if (is_tail_call) {
        __ JumpToJSEntry(ip);
      } else {
        __ CallJSEntry(ip);
      }
    }

    if (!is_tail_call) {
      // Set up deoptimization.
      RecordSafepointWithLazyDeopt(instr, RECORD_SIMPLE_SAFEPOINT);
    }
  } else {
    SafepointGenerator generator(this, pointers, Safepoint::kLazyDeopt);
    ParameterCount actual(arity);
    ParameterCount expected(formal_parameter_count);
    InvokeFlag flag = is_tail_call ? JUMP_FUNCTION : CALL_FUNCTION;
    __ InvokeFunction(function_reg, expected, actual, flag, generator);
  }
}

void LCodeGen::DoDeferredMathAbsTaggedHeapNumber(LMathAbs* instr) {
  DCHECK(instr->context() != NULL);
  DCHECK(ToRegister(instr->context()).is(cp));
  Register input = ToRegister(instr->value());
  Register result = ToRegister(instr->result());
  Register scratch = scratch0();

  // Deoptimize if not a heap number.
  __ LoadP(scratch, FieldMemOperand(input, HeapObject::kMapOffset));
  __ CompareRoot(scratch, Heap::kHeapNumberMapRootIndex);
  DeoptimizeIf(ne, instr, DeoptimizeReason::kNotAHeapNumber);

  Label done;
  Register exponent = scratch0();
  scratch = no_reg;
  __ LoadlW(exponent, FieldMemOperand(input, HeapNumber::kExponentOffset));
  // Check the sign of the argument. If the argument is positive, just
  // return it.
  __ Cmp32(exponent, Operand::Zero());
  // Move the input to the result if necessary.
  __ Move(result, input);
  __ bge(&done);

  // Input is negative. Reverse its sign.
  // Preserve the value of all registers.
  {
    PushSafepointRegistersScope scope(this);

    // Registers were saved at the safepoint, so we can use
    // many scratch registers.
    Register tmp1 = input.is(r3) ? r2 : r3;
    Register tmp2 = input.is(r4) ? r2 : r4;
    Register tmp3 = input.is(r5) ? r2 : r5;
    Register tmp4 = input.is(r6) ? r2 : r6;

    // exponent: floating point exponent value.

    Label allocated, slow;
    __ LoadRoot(tmp4, Heap::kHeapNumberMapRootIndex);
    __ AllocateHeapNumber(tmp1, tmp2, tmp3, tmp4, &slow);
    __ b(&allocated);

    // Slow case: Call the runtime system to do the number allocation.
    __ bind(&slow);

    CallRuntimeFromDeferred(Runtime::kAllocateHeapNumber, 0, instr,
                            instr->context());
    // Set the pointer to the new heap number in tmp.
    if (!tmp1.is(r2)) __ LoadRR(tmp1, r2);
    // Restore input_reg after call to runtime.
    __ LoadFromSafepointRegisterSlot(input, input);
    __ LoadlW(exponent, FieldMemOperand(input, HeapNumber::kExponentOffset));

    __ bind(&allocated);
    // exponent: floating point exponent value.
    // tmp1: allocated heap number.

    // Clear the sign bit.
    __ nilf(exponent, Operand(~HeapNumber::kSignMask));
    __ StoreW(exponent, FieldMemOperand(tmp1, HeapNumber::kExponentOffset));
    __ LoadlW(tmp2, FieldMemOperand(input, HeapNumber::kMantissaOffset));
    __ StoreW(tmp2, FieldMemOperand(tmp1, HeapNumber::kMantissaOffset));

    __ StoreToSafepointRegisterSlot(tmp1, result);
  }

  __ bind(&done);
}

void LCodeGen::EmitMathAbs(LMathAbs* instr) {
  Register input = ToRegister(instr->value());
  Register result = ToRegister(instr->result());
  __ LoadPositiveP(result, input);
  // Deoptimize on overflow.
  DeoptimizeIf(overflow, instr, DeoptimizeReason::kOverflow, cr0);
}

#if V8_TARGET_ARCH_S390X
void LCodeGen::EmitInteger32MathAbs(LMathAbs* instr) {
  Register input = ToRegister(instr->value());
  Register result = ToRegister(instr->result());
  __ LoadPositive32(result, input);
  DeoptimizeIf(overflow, instr, DeoptimizeReason::kOverflow);
}
#endif

void LCodeGen::DoMathAbs(LMathAbs* instr) {
  // Class for deferred case.
  class DeferredMathAbsTaggedHeapNumber final : public LDeferredCode {
   public:
    DeferredMathAbsTaggedHeapNumber(LCodeGen* codegen, LMathAbs* instr)
        : LDeferredCode(codegen), instr_(instr) {}
    void Generate() override {
      codegen()->DoDeferredMathAbsTaggedHeapNumber(instr_);
    }
    LInstruction* instr() override { return instr_; }

   private:
    LMathAbs* instr_;
  };

  Representation r = instr->hydrogen()->value()->representation();
  if (r.IsDouble()) {
    DoubleRegister input = ToDoubleRegister(instr->value());
    DoubleRegister result = ToDoubleRegister(instr->result());
    __ lpdbr(result, input);
#if V8_TARGET_ARCH_S390X
  } else if (r.IsInteger32()) {
    EmitInteger32MathAbs(instr);
  } else if (r.IsSmi()) {
#else
  } else if (r.IsSmiOrInteger32()) {
#endif
    EmitMathAbs(instr);
  } else {
    // Representation is tagged.
    DeferredMathAbsTaggedHeapNumber* deferred =
        new (zone()) DeferredMathAbsTaggedHeapNumber(this, instr);
    Register input = ToRegister(instr->value());
    // Smi check.
    __ JumpIfNotSmi(input, deferred->entry());
    // If smi, handle it directly.
    EmitMathAbs(instr);
    __ bind(deferred->exit());
  }
}

void LCodeGen::DoMathFloor(LMathFloor* instr) {
  DoubleRegister input = ToDoubleRegister(instr->value());
  Register result = ToRegister(instr->result());
  Register input_high = scratch0();
  Register scratch = ip;
  Label done, exact;

  __ TryInt32Floor(result, input, input_high, scratch, double_scratch0(), &done,
                   &exact);
  DeoptimizeIf(al, instr, DeoptimizeReason::kLostPrecisionOrNaN);

  __ bind(&exact);
  if (instr->hydrogen()->CheckFlag(HValue::kBailoutOnMinusZero)) {
    // Test for -0.
    __ CmpP(result, Operand::Zero());
    __ bne(&done, Label::kNear);
    __ Cmp32(input_high, Operand::Zero());
    DeoptimizeIf(lt, instr, DeoptimizeReason::kMinusZero);
  }
  __ bind(&done);
}

void LCodeGen::DoMathRound(LMathRound* instr) {
  DoubleRegister input = ToDoubleRegister(instr->value());
  Register result = ToRegister(instr->result());
  DoubleRegister double_scratch1 = ToDoubleRegister(instr->temp());
  DoubleRegister input_plus_dot_five = double_scratch1;
  Register scratch1 = scratch0();
  Register scratch2 = ip;
  DoubleRegister dot_five = double_scratch0();
  Label convert, done;

  __ LoadDoubleLiteral(dot_five, 0.5, r0);
  __ lpdbr(double_scratch1, input);
  __ cdbr(double_scratch1, dot_five);
  DeoptimizeIf(unordered, instr, DeoptimizeReason::kLostPrecisionOrNaN);
  // If input is in [-0.5, -0], the result is -0.
  // If input is in [+0, +0.5[, the result is +0.
  // If the input is +0.5, the result is 1.
  __ bgt(&convert, Label::kNear);  // Out of [-0.5, +0.5].
  if (instr->hydrogen()->CheckFlag(HValue::kBailoutOnMinusZero)) {
    // [-0.5, -0] (negative) yields minus zero.
    __ TestDoubleSign(input, scratch1);
    DeoptimizeIf(lt, instr, DeoptimizeReason::kMinusZero);
  }
  Label return_zero;
  __ cdbr(input, dot_five);
  __ bne(&return_zero, Label::kNear);
  __ LoadImmP(result, Operand(1));  // +0.5.
  __ b(&done, Label::kNear);
  // Remaining cases: [+0, +0.5[ or [-0.5, +0.5[, depending on
  // flag kBailoutOnMinusZero.
  __ bind(&return_zero);
  __ LoadImmP(result, Operand::Zero());
  __ b(&done, Label::kNear);

  __ bind(&convert);
  __ ldr(input_plus_dot_five, input);
  __ adbr(input_plus_dot_five, dot_five);
  // Reuse dot_five (double_scratch0) as we no longer need this value.
  __ TryInt32Floor(result, input_plus_dot_five, scratch1, scratch2,
                   double_scratch0(), &done, &done);
  DeoptimizeIf(al, instr, DeoptimizeReason::kLostPrecisionOrNaN);
  __ bind(&done);
}

void LCodeGen::DoMathFround(LMathFround* instr) {
  DoubleRegister input_reg = ToDoubleRegister(instr->value());
  DoubleRegister output_reg = ToDoubleRegister(instr->result());

  // Round double to float
  __ ledbr(output_reg, input_reg);
  // Extend from float to double
  __ ldebr(output_reg, output_reg);
}

void LCodeGen::DoMathSqrt(LMathSqrt* instr) {
  DoubleRegister result = ToDoubleRegister(instr->result());
  LOperand* input = instr->value();
  if (input->IsDoubleRegister()) {
    __ Sqrt(result, ToDoubleRegister(instr->value()));
  } else {
    __ Sqrt(result, ToMemOperand(input));
  }
}

void LCodeGen::DoMathPowHalf(LMathPowHalf* instr) {
  DoubleRegister input = ToDoubleRegister(instr->value());
  DoubleRegister result = ToDoubleRegister(instr->result());
  DoubleRegister temp = double_scratch0();

  // Note that according to ECMA-262 15.8.2.13:
  // Math.pow(-Infinity, 0.5) == Infinity
  // Math.sqrt(-Infinity) == NaN
  Label skip, done;

  __ LoadDoubleLiteral(temp, -V8_INFINITY, scratch0());
  __ cdbr(input, temp);
  __ bne(&skip, Label::kNear);
  __ lcdbr(result, temp);
  __ b(&done, Label::kNear);

  // Add +0 to convert -0 to +0.
  __ bind(&skip);
  __ ldr(result, input);
  __ lzdr(kDoubleRegZero);
  __ adbr(result, kDoubleRegZero);
  __ sqdbr(result, result);
  __ bind(&done);
}

void LCodeGen::DoPower(LPower* instr) {
  Representation exponent_type = instr->hydrogen()->right()->representation();
  // Having marked this as a call, we can use any registers.
  // Just make sure that the input/output registers are the expected ones.
  Register tagged_exponent = MathPowTaggedDescriptor::exponent();
  DCHECK(!instr->right()->IsDoubleRegister() ||
         ToDoubleRegister(instr->right()).is(d2));
  DCHECK(!instr->right()->IsRegister() ||
         ToRegister(instr->right()).is(tagged_exponent));
  DCHECK(ToDoubleRegister(instr->left()).is(d1));
  DCHECK(ToDoubleRegister(instr->result()).is(d3));

  if (exponent_type.IsSmi()) {
    MathPowStub stub(isolate(), MathPowStub::TAGGED);
    __ CallStub(&stub);
  } else if (exponent_type.IsTagged()) {
    Label no_deopt;
    __ JumpIfSmi(tagged_exponent, &no_deopt);
    __ LoadP(r9, FieldMemOperand(tagged_exponent, HeapObject::kMapOffset));
    __ CompareRoot(r9, Heap::kHeapNumberMapRootIndex);
    DeoptimizeIf(ne, instr, DeoptimizeReason::kNotAHeapNumber);
    __ bind(&no_deopt);
    MathPowStub stub(isolate(), MathPowStub::TAGGED);
    __ CallStub(&stub);
  } else if (exponent_type.IsInteger32()) {
    MathPowStub stub(isolate(), MathPowStub::INTEGER);
    __ CallStub(&stub);
  } else {
    DCHECK(exponent_type.IsDouble());
    MathPowStub stub(isolate(), MathPowStub::DOUBLE);
    __ CallStub(&stub);
  }
}

void LCodeGen::DoMathCos(LMathCos* instr) {
  __ PrepareCallCFunction(0, 1, scratch0());
  __ MovToFloatParameter(ToDoubleRegister(instr->value()));
  __ CallCFunction(ExternalReference::ieee754_cos_function(isolate()), 0, 1);
  __ MovFromFloatResult(ToDoubleRegister(instr->result()));
}

void LCodeGen::DoMathSin(LMathSin* instr) {
  __ PrepareCallCFunction(0, 1, scratch0());
  __ MovToFloatParameter(ToDoubleRegister(instr->value()));
  __ CallCFunction(ExternalReference::ieee754_sin_function(isolate()), 0, 1);
  __ MovFromFloatResult(ToDoubleRegister(instr->result()));
}

void LCodeGen::DoMathExp(LMathExp* instr) {
  __ PrepareCallCFunction(0, 1, scratch0());
  __ MovToFloatParameter(ToDoubleRegister(instr->value()));
  __ CallCFunction(ExternalReference::ieee754_exp_function(isolate()), 0, 1);
  __ MovFromFloatResult(ToDoubleRegister(instr->result()));
}

void LCodeGen::DoMathLog(LMathLog* instr) {
  __ PrepareCallCFunction(0, 1, scratch0());
  __ MovToFloatParameter(ToDoubleRegister(instr->value()));
  __ CallCFunction(ExternalReference::ieee754_log_function(isolate()), 0, 1);
  __ MovFromFloatResult(ToDoubleRegister(instr->result()));
}

void LCodeGen::DoMathClz32(LMathClz32* instr) {
  Register input = ToRegister(instr->value());
  Register result = ToRegister(instr->result());
  Label done;
  __ llgfr(result, input);
  __ flogr(r0, result);
  __ LoadRR(result, r0);
  __ CmpP(r0, Operand::Zero());
  __ beq(&done, Label::kNear);
  __ SubP(result, Operand(32));
  __ bind(&done);
}

void LCodeGen::PrepareForTailCall(const ParameterCount& actual,
                                  Register scratch1, Register scratch2,
                                  Register scratch3) {
#if DEBUG
  if (actual.is_reg()) {
    DCHECK(!AreAliased(actual.reg(), scratch1, scratch2, scratch3));
  } else {
    DCHECK(!AreAliased(scratch1, scratch2, scratch3));
  }
#endif
  if (FLAG_code_comments) {
    if (actual.is_reg()) {
      Comment(";;; PrepareForTailCall, actual: %s {",
              RegisterConfiguration::Crankshaft()->GetGeneralRegisterName(
                  actual.reg().code()));
    } else {
      Comment(";;; PrepareForTailCall, actual: %d {", actual.immediate());
    }
  }

  // Check if next frame is an arguments adaptor frame.
  Register caller_args_count_reg = scratch1;
  Label no_arguments_adaptor, formal_parameter_count_loaded;
  __ LoadP(scratch2, MemOperand(fp, StandardFrameConstants::kCallerFPOffset));
  __ LoadP(scratch3,
           MemOperand(scratch2, StandardFrameConstants::kContextOffset));
  __ CmpP(scratch3,
          Operand(StackFrame::TypeToMarker(StackFrame::ARGUMENTS_ADAPTOR)));
  __ bne(&no_arguments_adaptor);

  // Drop current frame and load arguments count from arguments adaptor frame.
  __ LoadRR(fp, scratch2);
  __ LoadP(caller_args_count_reg,
           MemOperand(fp, ArgumentsAdaptorFrameConstants::kLengthOffset));
  __ SmiUntag(caller_args_count_reg);
  __ b(&formal_parameter_count_loaded);

  __ bind(&no_arguments_adaptor);
  // Load caller's formal parameter count
  __ mov(caller_args_count_reg, Operand(info()->literal()->parameter_count()));

  __ bind(&formal_parameter_count_loaded);
  __ PrepareForTailCall(actual, caller_args_count_reg, scratch2, scratch3);

  Comment(";;; }");
}

void LCodeGen::DoInvokeFunction(LInvokeFunction* instr) {
  HInvokeFunction* hinstr = instr->hydrogen();
  DCHECK(ToRegister(instr->context()).is(cp));
  DCHECK(ToRegister(instr->function()).is(r3));
  DCHECK(instr->HasPointerMap());

  bool is_tail_call = hinstr->tail_call_mode() == TailCallMode::kAllow;

  if (is_tail_call) {
    DCHECK(!info()->saves_caller_doubles());
    ParameterCount actual(instr->arity());
    // It is safe to use r5, r6 and r7 as scratch registers here given that
    // 1) we are not going to return to caller function anyway,
    // 2) r5 (new.target) will be initialized below.
    PrepareForTailCall(actual, r5, r6, r7);
  }

  Handle<JSFunction> known_function = hinstr->known_function();
  if (known_function.is_null()) {
    LPointerMap* pointers = instr->pointer_map();
    SafepointGenerator generator(this, pointers, Safepoint::kLazyDeopt);
    ParameterCount actual(instr->arity());
    InvokeFlag flag = is_tail_call ? JUMP_FUNCTION : CALL_FUNCTION;
    __ InvokeFunction(r3, no_reg, actual, flag, generator);
  } else {
    CallKnownFunction(known_function, hinstr->formal_parameter_count(),
                      instr->arity(), is_tail_call, instr);
  }
}

void LCodeGen::DoCallWithDescriptor(LCallWithDescriptor* instr) {
  DCHECK(ToRegister(instr->result()).is(r2));

  if (instr->hydrogen()->IsTailCall()) {
    if (NeedsEagerFrame()) __ LeaveFrame(StackFrame::INTERNAL);

    if (instr->target()->IsConstantOperand()) {
      LConstantOperand* target = LConstantOperand::cast(instr->target());
      Handle<Code> code = Handle<Code>::cast(ToHandle(target));
      __ Jump(code, RelocInfo::CODE_TARGET);
    } else {
      DCHECK(instr->target()->IsRegister());
      Register target = ToRegister(instr->target());
      __ AddP(ip, target, Operand(Code::kHeaderSize - kHeapObjectTag));
      __ JumpToJSEntry(ip);
    }
  } else {
    LPointerMap* pointers = instr->pointer_map();
    SafepointGenerator generator(this, pointers, Safepoint::kLazyDeopt);

    if (instr->target()->IsConstantOperand()) {
      LConstantOperand* target = LConstantOperand::cast(instr->target());
      Handle<Code> code = Handle<Code>::cast(ToHandle(target));
      generator.BeforeCall(__ CallSize(code, RelocInfo::CODE_TARGET));
      __ Call(code, RelocInfo::CODE_TARGET);
    } else {
      DCHECK(instr->target()->IsRegister());
      Register target = ToRegister(instr->target());
      generator.BeforeCall(__ CallSize(target));
      __ AddP(ip, target, Operand(Code::kHeaderSize - kHeapObjectTag));
      __ CallJSEntry(ip);
    }
    generator.AfterCall();
  }
}

void LCodeGen::DoCallNewArray(LCallNewArray* instr) {
  DCHECK(ToRegister(instr->context()).is(cp));
  DCHECK(ToRegister(instr->constructor()).is(r3));
  DCHECK(ToRegister(instr->result()).is(r2));

  __ mov(r2, Operand(instr->arity()));
  __ Move(r4, instr->hydrogen()->site());

  ElementsKind kind = instr->hydrogen()->elements_kind();
  AllocationSiteOverrideMode override_mode =
      (AllocationSite::GetMode(kind) == TRACK_ALLOCATION_SITE)
          ? DISABLE_ALLOCATION_SITES
          : DONT_OVERRIDE;

  if (instr->arity() == 0) {
    ArrayNoArgumentConstructorStub stub(isolate(), kind, override_mode);
    CallCode(stub.GetCode(), RelocInfo::CODE_TARGET, instr);
  } else if (instr->arity() == 1) {
    Label done;
    if (IsFastPackedElementsKind(kind)) {
      Label packed_case;
      // We might need a change here
      // look at the first argument
      __ LoadP(r7, MemOperand(sp, 0));
      __ CmpP(r7, Operand::Zero());
      __ beq(&packed_case, Label::kNear);

      ElementsKind holey_kind = GetHoleyElementsKind(kind);
      ArraySingleArgumentConstructorStub stub(isolate(), holey_kind,
                                              override_mode);
      CallCode(stub.GetCode(), RelocInfo::CODE_TARGET, instr);
      __ b(&done, Label::kNear);
      __ bind(&packed_case);
    }

    ArraySingleArgumentConstructorStub stub(isolate(), kind, override_mode);
    CallCode(stub.GetCode(), RelocInfo::CODE_TARGET, instr);
    __ bind(&done);
  } else {
    ArrayNArgumentsConstructorStub stub(isolate());
    CallCode(stub.GetCode(), RelocInfo::CODE_TARGET, instr);
  }
}

void LCodeGen::DoCallRuntime(LCallRuntime* instr) {
  CallRuntime(instr->function(), instr->arity(), instr);
}

void LCodeGen::DoStoreCodeEntry(LStoreCodeEntry* instr) {
  Register function = ToRegister(instr->function());
  Register code_object = ToRegister(instr->code_object());
  __ lay(code_object,
         MemOperand(code_object, Code::kHeaderSize - kHeapObjectTag));
  __ StoreP(code_object,
            FieldMemOperand(function, JSFunction::kCodeEntryOffset), r0);
}

void LCodeGen::DoInnerAllocatedObject(LInnerAllocatedObject* instr) {
  Register result = ToRegister(instr->result());
  Register base = ToRegister(instr->base_object());
  if (instr->offset()->IsConstantOperand()) {
    LConstantOperand* offset = LConstantOperand::cast(instr->offset());
    __ lay(result, MemOperand(base, ToInteger32(offset)));
  } else {
    Register offset = ToRegister(instr->offset());
    __ lay(result, MemOperand(base, offset));
  }
}

void LCodeGen::DoStoreNamedField(LStoreNamedField* instr) {
  HStoreNamedField* hinstr = instr->hydrogen();
  Representation representation = instr->representation();

  Register object = ToRegister(instr->object());
  Register scratch = scratch0();
  HObjectAccess access = hinstr->access();
  int offset = access.offset();

  if (access.IsExternalMemory()) {
    Register value = ToRegister(instr->value());
    MemOperand operand = MemOperand(object, offset);
    __ StoreRepresentation(value, operand, representation, r0);
    return;
  }

  __ AssertNotSmi(object);

#if V8_TARGET_ARCH_S390X
  DCHECK(!representation.IsSmi() || !instr->value()->IsConstantOperand() ||
         IsInteger32(LConstantOperand::cast(instr->value())));
#else
  DCHECK(!representation.IsSmi() || !instr->value()->IsConstantOperand() ||
         IsSmi(LConstantOperand::cast(instr->value())));
#endif
  if (!FLAG_unbox_double_fields && representation.IsDouble()) {
    DCHECK(access.IsInobject());
    DCHECK(!hinstr->has_transition());
    DCHECK(!hinstr->NeedsWriteBarrier());
    DoubleRegister value = ToDoubleRegister(instr->value());
    DCHECK(offset >= 0);
    __ StoreDouble(value, FieldMemOperand(object, offset));
    return;
  }

  if (hinstr->has_transition()) {
    Handle<Map> transition = hinstr->transition_map();
    AddDeprecationDependency(transition);
    __ mov(scratch, Operand(transition));
    __ StoreP(scratch, FieldMemOperand(object, HeapObject::kMapOffset), r0);
    if (hinstr->NeedsWriteBarrierForMap()) {
      Register temp = ToRegister(instr->temp());
      // Update the write barrier for the map field.
      __ RecordWriteForMap(object, scratch, temp, GetLinkRegisterState(),
                           kSaveFPRegs);
    }
  }

  // Do the store.
  Register record_dest = object;
  Register record_value = no_reg;
  Register record_scratch = scratch;
#if V8_TARGET_ARCH_S390X
  if (FLAG_unbox_double_fields && representation.IsDouble()) {
    DCHECK(access.IsInobject());
    DoubleRegister value = ToDoubleRegister(instr->value());
    __ StoreDouble(value, FieldMemOperand(object, offset));
    if (hinstr->NeedsWriteBarrier()) {
      record_value = ToRegister(instr->value());
    }
  } else {
    if (representation.IsSmi() &&
        hinstr->value()->representation().IsInteger32()) {
      DCHECK(hinstr->store_mode() == STORE_TO_INITIALIZED_ENTRY);
      // 64-bit Smi optimization
      // Store int value directly to upper half of the smi.
      offset = SmiWordOffset(offset);
      representation = Representation::Integer32();
    }
#endif
    if (access.IsInobject()) {
      Register value = ToRegister(instr->value());
      MemOperand operand = FieldMemOperand(object, offset);
      __ StoreRepresentation(value, operand, representation, r0);
      record_value = value;
    } else {
      Register value = ToRegister(instr->value());
      __ LoadP(scratch, FieldMemOperand(object, JSObject::kPropertiesOffset));
      MemOperand operand = FieldMemOperand(scratch, offset);
      __ StoreRepresentation(value, operand, representation, r0);
      record_dest = scratch;
      record_value = value;
      record_scratch = object;
    }
#if V8_TARGET_ARCH_S390X
  }
#endif

  if (hinstr->NeedsWriteBarrier()) {
    __ RecordWriteField(record_dest, offset, record_value, record_scratch,
                        GetLinkRegisterState(), kSaveFPRegs,
                        EMIT_REMEMBERED_SET, hinstr->SmiCheckForWriteBarrier(),
                        hinstr->PointersToHereCheckForValue());
  }
}

void LCodeGen::DoBoundsCheck(LBoundsCheck* instr) {
  Representation representation = instr->hydrogen()->length()->representation();
  DCHECK(representation.Equals(instr->hydrogen()->index()->representation()));
  DCHECK(representation.IsSmiOrInteger32());
  Register temp = scratch0();

  Condition cc = instr->hydrogen()->allow_equality() ? lt : le;
  if (instr->length()->IsConstantOperand()) {
    int32_t length = ToInteger32(LConstantOperand::cast(instr->length()));
    Register index = ToRegister(instr->index());
    if (representation.IsSmi()) {
      __ CmpLogicalSmiLiteral(index, Smi::FromInt(length), temp);
    } else {
      __ CmpLogical32(index, Operand(length));
    }
    cc = CommuteCondition(cc);
  } else if (instr->index()->IsConstantOperand()) {
    int32_t index = ToInteger32(LConstantOperand::cast(instr->index()));
    Register length = ToRegister(instr->length());
    if (representation.IsSmi()) {
      __ CmpLogicalSmiLiteral(length, Smi::FromInt(index), temp);
    } else {
      __ CmpLogical32(length, Operand(index));
    }
  } else {
    Register index = ToRegister(instr->index());
    Register length = ToRegister(instr->length());
    if (representation.IsSmi()) {
      __ CmpLogicalP(length, index);
    } else {
      __ CmpLogical32(length, index);
    }
  }
  if (FLAG_debug_code && instr->hydrogen()->skip_check()) {
    Label done;
    __ b(NegateCondition(cc), &done, Label::kNear);
    __ stop("eliminated bounds check failed");
    __ bind(&done);
  } else {
    DeoptimizeIf(cc, instr, DeoptimizeReason::kOutOfBounds);
  }
}

void LCodeGen::DoStoreKeyedExternalArray(LStoreKeyed* instr) {
  Register external_pointer = ToRegister(instr->elements());
  Register key = no_reg;
  ElementsKind elements_kind = instr->elements_kind();
  bool key_is_constant = instr->key()->IsConstantOperand();
  int constant_key = 0;
  if (key_is_constant) {
    constant_key = ToInteger32(LConstantOperand::cast(instr->key()));
    if (constant_key & 0xF0000000) {
      Abort(kArrayIndexConstantValueTooBig);
    }
  } else {
    key = ToRegister(instr->key());
  }
  int element_size_shift = ElementsKindToShiftSize(elements_kind);
  bool key_is_smi = instr->hydrogen()->key()->representation().IsSmi();
  bool keyMaybeNegative = instr->hydrogen()->IsDehoisted();
  int base_offset = instr->base_offset();

  if (elements_kind == FLOAT32_ELEMENTS || elements_kind == FLOAT64_ELEMENTS) {
    Register address = scratch0();
    DoubleRegister value(ToDoubleRegister(instr->value()));
    if (key_is_constant) {
      if (constant_key != 0) {
        base_offset += constant_key << element_size_shift;
        if (!is_int20(base_offset)) {
          __ mov(address, Operand(base_offset));
          __ AddP(address, external_pointer);
        } else {
          __ AddP(address, external_pointer, Operand(base_offset));
        }
        base_offset = 0;
      } else {
        address = external_pointer;
      }
    } else {
      __ IndexToArrayOffset(address, key, element_size_shift, key_is_smi,
                            keyMaybeNegative);
      __ AddP(address, external_pointer);
    }
    if (elements_kind == FLOAT32_ELEMENTS) {
      __ ledbr(double_scratch0(), value);
      __ StoreFloat32(double_scratch0(), MemOperand(address, base_offset));
    } else {  // Storing doubles, not floats.
      __ StoreDouble(value, MemOperand(address, base_offset));
    }
  } else {
    Register value(ToRegister(instr->value()));
    MemOperand mem_operand =
        PrepareKeyedOperand(key, external_pointer, key_is_constant, key_is_smi,
                            constant_key, element_size_shift, base_offset,
                            keyMaybeNegative);
    switch (elements_kind) {
      case UINT8_ELEMENTS:
      case UINT8_CLAMPED_ELEMENTS:
      case INT8_ELEMENTS:
        if (key_is_constant) {
          __ StoreByte(value, mem_operand, r0);
        } else {
          __ StoreByte(value, mem_operand);
        }
        break;
      case INT16_ELEMENTS:
      case UINT16_ELEMENTS:
        if (key_is_constant) {
          __ StoreHalfWord(value, mem_operand, r0);
        } else {
          __ StoreHalfWord(value, mem_operand);
        }
        break;
      case INT32_ELEMENTS:
      case UINT32_ELEMENTS:
        if (key_is_constant) {
          __ StoreW(value, mem_operand, r0);
        } else {
          __ StoreW(value, mem_operand);
        }
        break;
      case FLOAT32_ELEMENTS:
      case FLOAT64_ELEMENTS:
      case FAST_DOUBLE_ELEMENTS:
      case FAST_ELEMENTS:
      case FAST_SMI_ELEMENTS:
      case FAST_HOLEY_DOUBLE_ELEMENTS:
      case FAST_HOLEY_ELEMENTS:
      case FAST_HOLEY_SMI_ELEMENTS:
      case DICTIONARY_ELEMENTS:
      case FAST_SLOPPY_ARGUMENTS_ELEMENTS:
      case SLOW_SLOPPY_ARGUMENTS_ELEMENTS:
      case FAST_STRING_WRAPPER_ELEMENTS:
      case SLOW_STRING_WRAPPER_ELEMENTS:
      case NO_ELEMENTS:
        UNREACHABLE();
        break;
    }
  }
}

void LCodeGen::DoStoreKeyedFixedDoubleArray(LStoreKeyed* instr) {
  DoubleRegister value = ToDoubleRegister(instr->value());
  Register elements = ToRegister(instr->elements());
  Register key = no_reg;
  Register scratch = scratch0();
  DoubleRegister double_scratch = double_scratch0();
  bool key_is_constant = instr->key()->IsConstantOperand();
  int constant_key = 0;

  // Calculate the effective address of the slot in the array to store the
  // double value.
  if (key_is_constant) {
    constant_key = ToInteger32(LConstantOperand::cast(instr->key()));
    if (constant_key & 0xF0000000) {
      Abort(kArrayIndexConstantValueTooBig);
    }
  } else {
    key = ToRegister(instr->key());
  }
  int element_size_shift = ElementsKindToShiftSize(FAST_DOUBLE_ELEMENTS);
  bool key_is_smi = instr->hydrogen()->key()->representation().IsSmi();
  bool keyMaybeNegative = instr->hydrogen()->IsDehoisted();
  int base_offset = instr->base_offset() + constant_key * kDoubleSize;
  bool use_scratch = false;
  intptr_t address_offset = base_offset;

  if (key_is_constant) {
    // Memory references support up to 20-bits signed displacement in RXY form
    if (!is_int20((address_offset))) {
      __ mov(scratch, Operand(address_offset));
      address_offset = 0;
      use_scratch = true;
    }
  } else {
    use_scratch = true;
    __ IndexToArrayOffset(scratch, key, element_size_shift, key_is_smi,
                          keyMaybeNegative);
    // Memory references support up to 20-bits signed displacement in RXY form
    if (!is_int20((address_offset))) {
      __ AddP(scratch, Operand(address_offset));
      address_offset = 0;
    }
  }

  if (instr->NeedsCanonicalization()) {
    // Turn potential sNaN value into qNaN.
    __ CanonicalizeNaN(double_scratch, value);
    DCHECK(address_offset >= 0);
    if (use_scratch)
      __ StoreDouble(double_scratch,
                     MemOperand(scratch, elements, address_offset));
    else
      __ StoreDouble(double_scratch, MemOperand(elements, address_offset));
  } else {
    if (use_scratch)
      __ StoreDouble(value, MemOperand(scratch, elements, address_offset));
    else
      __ StoreDouble(value, MemOperand(elements, address_offset));
  }
}

void LCodeGen::DoStoreKeyedFixedArray(LStoreKeyed* instr) {
  HStoreKeyed* hinstr = instr->hydrogen();
  Register value = ToRegister(instr->value());
  Register elements = ToRegister(instr->elements());
  Register key = instr->key()->IsRegister() ? ToRegister(instr->key()) : no_reg;
  Register scratch = scratch0();
  int offset = instr->base_offset();

  // Do the store.
  if (instr->key()->IsConstantOperand()) {
    DCHECK(!hinstr->NeedsWriteBarrier());
    LConstantOperand* const_operand = LConstantOperand::cast(instr->key());
    offset += ToInteger32(const_operand) * kPointerSize;
  } else {
    // Even though the HLoadKeyed instruction forces the input
    // representation for the key to be an integer, the input gets replaced
    // during bound check elimination with the index argument to the bounds
    // check, which can be tagged, so that case must be handled here, too.
    if (hinstr->key()->representation().IsSmi()) {
      __ SmiToPtrArrayOffset(scratch, key);
    } else {
      if (instr->hydrogen()->IsDehoisted() ||
          !CpuFeatures::IsSupported(GENERAL_INSTR_EXT)) {
#if V8_TARGET_ARCH_S390X
        // If array access is dehoisted, the key, being an int32, can contain
        // a negative value, as needs to be sign-extended to 64-bit for
        // memory access.
        __ lgfr(key, key);
#endif
        __ ShiftLeftP(scratch, key, Operand(kPointerSizeLog2));
      } else {
        // Small optimization to reduce pathlength.  After Bounds Check,
        // the key is guaranteed to be non-negative.  Leverage RISBG,
        // which also performs zero-extension.
        __ risbg(scratch, key, Operand(32 - kPointerSizeLog2),
                 Operand(63 - kPointerSizeLog2), Operand(kPointerSizeLog2),
                 true);
      }
    }
  }

  Representation representation = hinstr->value()->representation();

#if V8_TARGET_ARCH_S390X
  // 64-bit Smi optimization
  if (representation.IsInteger32()) {
    DCHECK(hinstr->store_mode() == STORE_TO_INITIALIZED_ENTRY);
    DCHECK(hinstr->elements_kind() == FAST_SMI_ELEMENTS);
    // Store int value directly to upper half of the smi.
    offset = SmiWordOffset(offset);
  }
#endif

  if (instr->key()->IsConstantOperand()) {
    __ StoreRepresentation(value, MemOperand(elements, offset), representation,
                           scratch);
  } else {
    __ StoreRepresentation(value, MemOperand(scratch, elements, offset),
                           representation, r0);
  }

  if (hinstr->NeedsWriteBarrier()) {
    SmiCheck check_needed = hinstr->value()->type().IsHeapObject()
                                ? OMIT_SMI_CHECK
                                : INLINE_SMI_CHECK;
    // Compute address of modified element and store it into key register.
    if (instr->key()->IsConstantOperand()) {
      __ lay(key, MemOperand(elements, offset));
    } else {
      __ lay(key, MemOperand(scratch, elements, offset));
    }
    __ RecordWrite(elements, key, value, GetLinkRegisterState(), kSaveFPRegs,
                   EMIT_REMEMBERED_SET, check_needed,
                   hinstr->PointersToHereCheckForValue());
  }
}

void LCodeGen::DoStoreKeyed(LStoreKeyed* instr) {
  // By cases: external, fast double
  if (instr->is_fixed_typed_array()) {
    DoStoreKeyedExternalArray(instr);
  } else if (instr->hydrogen()->value()->representation().IsDouble()) {
    DoStoreKeyedFixedDoubleArray(instr);
  } else {
    DoStoreKeyedFixedArray(instr);
  }
}

void LCodeGen::DoMaybeGrowElements(LMaybeGrowElements* instr) {
  class DeferredMaybeGrowElements final : public LDeferredCode {
   public:
    DeferredMaybeGrowElements(LCodeGen* codegen, LMaybeGrowElements* instr)
        : LDeferredCode(codegen), instr_(instr) {}
    void Generate() override { codegen()->DoDeferredMaybeGrowElements(instr_); }
    LInstruction* instr() override { return instr_; }

   private:
    LMaybeGrowElements* instr_;
  };

  Register result = r2;
  DeferredMaybeGrowElements* deferred =
      new (zone()) DeferredMaybeGrowElements(this, instr);
  LOperand* key = instr->key();
  LOperand* current_capacity = instr->current_capacity();

  DCHECK(instr->hydrogen()->key()->representation().IsInteger32());
  DCHECK(instr->hydrogen()->current_capacity()->representation().IsInteger32());
  DCHECK(key->IsConstantOperand() || key->IsRegister());
  DCHECK(current_capacity->IsConstantOperand() ||
         current_capacity->IsRegister());

  if (key->IsConstantOperand() && current_capacity->IsConstantOperand()) {
    int32_t constant_key = ToInteger32(LConstantOperand::cast(key));
    int32_t constant_capacity =
        ToInteger32(LConstantOperand::cast(current_capacity));
    if (constant_key >= constant_capacity) {
      // Deferred case.
      __ b(deferred->entry());
    }
  } else if (key->IsConstantOperand()) {
    int32_t constant_key = ToInteger32(LConstantOperand::cast(key));
    __ Cmp32(ToRegister(current_capacity), Operand(constant_key));
    __ ble(deferred->entry());
  } else if (current_capacity->IsConstantOperand()) {
    int32_t constant_capacity =
        ToInteger32(LConstantOperand::cast(current_capacity));
    __ Cmp32(ToRegister(key), Operand(constant_capacity));
    __ bge(deferred->entry());
  } else {
    __ Cmp32(ToRegister(key), ToRegister(current_capacity));
    __ bge(deferred->entry());
  }

  if (instr->elements()->IsRegister()) {
    __ Move(result, ToRegister(instr->elements()));
  } else {
    __ LoadP(result, ToMemOperand(instr->elements()));
  }

  __ bind(deferred->exit());
}

void LCodeGen::DoDeferredMaybeGrowElements(LMaybeGrowElements* instr) {
  // TODO(3095996): Get rid of this. For now, we need to make the
  // result register contain a valid pointer because it is already
  // contained in the register pointer map.
  Register result = r2;
  __ LoadImmP(result, Operand::Zero());

  // We have to call a stub.
  {
    PushSafepointRegistersScope scope(this);
    if (instr->object()->IsRegister()) {
      __ Move(result, ToRegister(instr->object()));
    } else {
      __ LoadP(result, ToMemOperand(instr->object()));
    }

    LOperand* key = instr->key();
    if (key->IsConstantOperand()) {
      LConstantOperand* constant_key = LConstantOperand::cast(key);
      int32_t int_key = ToInteger32(constant_key);
      if (Smi::IsValid(int_key)) {
        __ LoadSmiLiteral(r5, Smi::FromInt(int_key));
      } else {
        Abort(kArrayIndexConstantValueTooBig);
      }
    } else {
      Label is_smi;
#if V8_TARGET_ARCH_S390X
      __ SmiTag(r5, ToRegister(key));
#else
      // Deopt if the key is outside Smi range. The stub expects Smi and would
      // bump the elements into dictionary mode (and trigger a deopt) anyways.
      __ Add32(r5, ToRegister(key), ToRegister(key));
      __ b(nooverflow, &is_smi);
      __ PopSafepointRegisters();
      DeoptimizeIf(al, instr, DeoptimizeReason::kOverflow, cr0);
      __ bind(&is_smi);
#endif
    }

    GrowArrayElementsStub stub(isolate(), instr->hydrogen()->kind());
    __ CallStub(&stub);
    RecordSafepointWithLazyDeopt(
        instr, RECORD_SAFEPOINT_WITH_REGISTERS_AND_NO_ARGUMENTS);
    __ StoreToSafepointRegisterSlot(result, result);
  }

  // Deopt on smi, which means the elements array changed to dictionary mode.
  __ TestIfSmi(result);
  DeoptimizeIf(eq, instr, DeoptimizeReason::kSmi, cr0);
}

void LCodeGen::DoTransitionElementsKind(LTransitionElementsKind* instr) {
  Register object_reg = ToRegister(instr->object());
  Register scratch = scratch0();

  Handle<Map> from_map = instr->original_map();
  Handle<Map> to_map = instr->transitioned_map();
  ElementsKind from_kind = instr->from_kind();
  ElementsKind to_kind = instr->to_kind();

  Label not_applicable;
  __ LoadP(scratch, FieldMemOperand(object_reg, HeapObject::kMapOffset));
  __ CmpP(scratch, Operand(from_map));
  __ bne(&not_applicable);

  if (IsSimpleMapChangeTransition(from_kind, to_kind)) {
    Register new_map_reg = ToRegister(instr->new_map_temp());
    __ mov(new_map_reg, Operand(to_map));
    __ StoreP(new_map_reg, FieldMemOperand(object_reg, HeapObject::kMapOffset));
    // Write barrier.
    __ RecordWriteForMap(object_reg, new_map_reg, scratch,
                         GetLinkRegisterState(), kDontSaveFPRegs);
  } else {
    DCHECK(ToRegister(instr->context()).is(cp));
    DCHECK(object_reg.is(r2));
    PushSafepointRegistersScope scope(this);
    __ Move(r3, to_map);
    TransitionElementsKindStub stub(isolate(), from_kind, to_kind);
    __ CallStub(&stub);
    RecordSafepointWithRegisters(instr->pointer_map(), 0,
                                 Safepoint::kLazyDeopt);
  }
  __ bind(&not_applicable);
}

void LCodeGen::DoTrapAllocationMemento(LTrapAllocationMemento* instr) {
  Register object = ToRegister(instr->object());
  Register temp1 = ToRegister(instr->temp1());
  Register temp2 = ToRegister(instr->temp2());
  Label no_memento_found;
  __ TestJSArrayForAllocationMemento(object, temp1, temp2, &no_memento_found);
  DeoptimizeIf(eq, instr, DeoptimizeReason::kMementoFound);
  __ bind(&no_memento_found);
}

void LCodeGen::DoStringAdd(LStringAdd* instr) {
  DCHECK(ToRegister(instr->context()).is(cp));
  DCHECK(ToRegister(instr->left()).is(r3));
  DCHECK(ToRegister(instr->right()).is(r2));
  StringAddStub stub(isolate(), instr->hydrogen()->flags(),
                     instr->hydrogen()->pretenure_flag());
  CallCode(stub.GetCode(), RelocInfo::CODE_TARGET, instr);
}

void LCodeGen::DoStringCharCodeAt(LStringCharCodeAt* instr) {
  class DeferredStringCharCodeAt final : public LDeferredCode {
   public:
    DeferredStringCharCodeAt(LCodeGen* codegen, LStringCharCodeAt* instr)
        : LDeferredCode(codegen), instr_(instr) {}
    void Generate() override { codegen()->DoDeferredStringCharCodeAt(instr_); }
    LInstruction* instr() override { return instr_; }

   private:
    LStringCharCodeAt* instr_;
  };

  DeferredStringCharCodeAt* deferred =
      new (zone()) DeferredStringCharCodeAt(this, instr);

  StringCharLoadGenerator::Generate(
      masm(), ToRegister(instr->string()), ToRegister(instr->index()),
      ToRegister(instr->result()), deferred->entry());
  __ bind(deferred->exit());
}

void LCodeGen::DoDeferredStringCharCodeAt(LStringCharCodeAt* instr) {
  Register string = ToRegister(instr->string());
  Register result = ToRegister(instr->result());
  Register scratch = scratch0();

  // TODO(3095996): Get rid of this. For now, we need to make the
  // result register contain a valid pointer because it is already
  // contained in the register pointer map.
  __ LoadImmP(result, Operand::Zero());

  PushSafepointRegistersScope scope(this);
  __ push(string);
  // Push the index as a smi. This is safe because of the checks in
  // DoStringCharCodeAt above.
  if (instr->index()->IsConstantOperand()) {
    int const_index = ToInteger32(LConstantOperand::cast(instr->index()));
    __ LoadSmiLiteral(scratch, Smi::FromInt(const_index));
    __ push(scratch);
  } else {
    Register index = ToRegister(instr->index());
    __ SmiTag(index);
    __ push(index);
  }
  CallRuntimeFromDeferred(Runtime::kStringCharCodeAtRT, 2, instr,
                          instr->context());
  __ AssertSmi(r2);
  __ SmiUntag(r2);
  __ StoreToSafepointRegisterSlot(r2, result);
}

void LCodeGen::DoStringCharFromCode(LStringCharFromCode* instr) {
  class DeferredStringCharFromCode final : public LDeferredCode {
   public:
    DeferredStringCharFromCode(LCodeGen* codegen, LStringCharFromCode* instr)
        : LDeferredCode(codegen), instr_(instr) {}
    void Generate() override {
      codegen()->DoDeferredStringCharFromCode(instr_);
    }
    LInstruction* instr() override { return instr_; }

   private:
    LStringCharFromCode* instr_;
  };

  DeferredStringCharFromCode* deferred =
      new (zone()) DeferredStringCharFromCode(this, instr);

  DCHECK(instr->hydrogen()->value()->representation().IsInteger32());
  Register char_code = ToRegister(instr->char_code());
  Register result = ToRegister(instr->result());
  DCHECK(!char_code.is(result));

  __ CmpLogicalP(char_code, Operand(String::kMaxOneByteCharCode));
  __ bgt(deferred->entry());
  __ LoadRoot(result, Heap::kSingleCharacterStringCacheRootIndex);
  __ ShiftLeftP(r0, char_code, Operand(kPointerSizeLog2));
  __ AddP(result, r0);
  __ LoadP(result, FieldMemOperand(result, FixedArray::kHeaderSize));
  __ CompareRoot(result, Heap::kUndefinedValueRootIndex);
  __ beq(deferred->entry());
  __ bind(deferred->exit());
}

void LCodeGen::DoDeferredStringCharFromCode(LStringCharFromCode* instr) {
  Register char_code = ToRegister(instr->char_code());
  Register result = ToRegister(instr->result());

  // TODO(3095996): Get rid of this. For now, we need to make the
  // result register contain a valid pointer because it is already
  // contained in the register pointer map.
  __ LoadImmP(result, Operand::Zero());

  PushSafepointRegistersScope scope(this);
  __ SmiTag(char_code);
  __ push(char_code);
  CallRuntimeFromDeferred(Runtime::kStringCharFromCode, 1, instr,
                          instr->context());
  __ StoreToSafepointRegisterSlot(r2, result);
}

void LCodeGen::DoInteger32ToDouble(LInteger32ToDouble* instr) {
  LOperand* input = instr->value();
  DCHECK(input->IsRegister() || input->IsStackSlot());
  LOperand* output = instr->result();
  DCHECK(output->IsDoubleRegister());
  if (input->IsStackSlot()) {
    Register scratch = scratch0();
    __ LoadP(scratch, ToMemOperand(input));
    __ ConvertIntToDouble(ToDoubleRegister(output), scratch);
  } else {
    __ ConvertIntToDouble(ToDoubleRegister(output), ToRegister(input));
  }
}

void LCodeGen::DoUint32ToDouble(LUint32ToDouble* instr) {
  LOperand* input = instr->value();
  LOperand* output = instr->result();
  __ ConvertUnsignedIntToDouble(ToDoubleRegister(output), ToRegister(input));
}

void LCodeGen::DoNumberTagI(LNumberTagI* instr) {
  class DeferredNumberTagI final : public LDeferredCode {
   public:
    DeferredNumberTagI(LCodeGen* codegen, LNumberTagI* instr)
        : LDeferredCode(codegen), instr_(instr) {}
    void Generate() override {
      codegen()->DoDeferredNumberTagIU(instr_, instr_->value(), instr_->temp1(),
                                       instr_->temp2(), SIGNED_INT32);
    }
    LInstruction* instr() override { return instr_; }

   private:
    LNumberTagI* instr_;
  };

  Register src = ToRegister(instr->value());
  Register dst = ToRegister(instr->result());

  DeferredNumberTagI* deferred = new (zone()) DeferredNumberTagI(this, instr);
#if V8_TARGET_ARCH_S390X
  __ SmiTag(dst, src);
#else
  // Add src to itself to defect SMI overflow.
  __ Add32(dst, src, src);
  __ b(overflow, deferred->entry());
#endif
  __ bind(deferred->exit());
}

void LCodeGen::DoNumberTagU(LNumberTagU* instr) {
  class DeferredNumberTagU final : public LDeferredCode {
   public:
    DeferredNumberTagU(LCodeGen* codegen, LNumberTagU* instr)
        : LDeferredCode(codegen), instr_(instr) {}
    void Generate() override {
      codegen()->DoDeferredNumberTagIU(instr_, instr_->value(), instr_->temp1(),
                                       instr_->temp2(), UNSIGNED_INT32);
    }
    LInstruction* instr() override { return instr_; }

   private:
    LNumberTagU* instr_;
  };

  Register input = ToRegister(instr->value());
  Register result = ToRegister(instr->result());

  DeferredNumberTagU* deferred = new (zone()) DeferredNumberTagU(this, instr);
  __ CmpLogicalP(input, Operand(Smi::kMaxValue));
  __ bgt(deferred->entry());
  __ SmiTag(result, input);
  __ bind(deferred->exit());
}

void LCodeGen::DoDeferredNumberTagIU(LInstruction* instr, LOperand* value,
                                     LOperand* temp1, LOperand* temp2,
                                     IntegerSignedness signedness) {
  Label done, slow;
  Register src = ToRegister(value);
  Register dst = ToRegister(instr->result());
  Register tmp1 = scratch0();
  Register tmp2 = ToRegister(temp1);
  Register tmp3 = ToRegister(temp2);
  DoubleRegister dbl_scratch = double_scratch0();

  if (signedness == SIGNED_INT32) {
    // There was overflow, so bits 30 and 31 of the original integer
    // disagree. Try to allocate a heap number in new space and store
    // the value in there. If that fails, call the runtime system.
    if (dst.is(src)) {
      __ SmiUntag(src, dst);
      __ xilf(src, Operand(HeapNumber::kSignMask));
    }
    __ ConvertIntToDouble(dbl_scratch, src);
  } else {
    __ ConvertUnsignedIntToDouble(dbl_scratch, src);
  }

  if (FLAG_inline_new) {
    __ LoadRoot(tmp3, Heap::kHeapNumberMapRootIndex);
    __ AllocateHeapNumber(dst, tmp1, tmp2, tmp3, &slow);
    __ b(&done);
  }

  // Slow case: Call the runtime system to do the number allocation.
  __ bind(&slow);
  {
    // TODO(3095996): Put a valid pointer value in the stack slot where the
    // result register is stored, as this register is in the pointer map, but
    // contains an integer value.
    __ LoadImmP(dst, Operand::Zero());

    // Preserve the value of all registers.
    PushSafepointRegistersScope scope(this);
    // Reset the context register.
    if (!dst.is(cp)) {
      __ LoadImmP(cp, Operand::Zero());
    }
    __ CallRuntimeSaveDoubles(Runtime::kAllocateHeapNumber);
    RecordSafepointWithRegisters(instr->pointer_map(), 0,
                                 Safepoint::kNoLazyDeopt);
    __ StoreToSafepointRegisterSlot(r2, dst);
  }

  // Done. Put the value in dbl_scratch into the value of the allocated heap
  // number.
  __ bind(&done);
  __ StoreDouble(dbl_scratch, FieldMemOperand(dst, HeapNumber::kValueOffset));
}

void LCodeGen::DoNumberTagD(LNumberTagD* instr) {
  class DeferredNumberTagD final : public LDeferredCode {
   public:
    DeferredNumberTagD(LCodeGen* codegen, LNumberTagD* instr)
        : LDeferredCode(codegen), instr_(instr) {}
    void Generate() override { codegen()->DoDeferredNumberTagD(instr_); }
    LInstruction* instr() override { return instr_; }

   private:
    LNumberTagD* instr_;
  };

  DoubleRegister input_reg = ToDoubleRegister(instr->value());
  Register scratch = scratch0();
  Register reg = ToRegister(instr->result());
  Register temp1 = ToRegister(instr->temp());
  Register temp2 = ToRegister(instr->temp2());

  DeferredNumberTagD* deferred = new (zone()) DeferredNumberTagD(this, instr);
  if (FLAG_inline_new) {
    __ LoadRoot(scratch, Heap::kHeapNumberMapRootIndex);
    __ AllocateHeapNumber(reg, temp1, temp2, scratch, deferred->entry());
  } else {
    __ b(deferred->entry());
  }
  __ bind(deferred->exit());
  __ StoreDouble(input_reg, FieldMemOperand(reg, HeapNumber::kValueOffset));
}

void LCodeGen::DoDeferredNumberTagD(LNumberTagD* instr) {
  // TODO(3095996): Get rid of this. For now, we need to make the
  // result register contain a valid pointer because it is already
  // contained in the register pointer map.
  Register reg = ToRegister(instr->result());
  __ LoadImmP(reg, Operand::Zero());

  PushSafepointRegistersScope scope(this);
  // Reset the context register.
  if (!reg.is(cp)) {
    __ LoadImmP(cp, Operand::Zero());
  }
  __ CallRuntimeSaveDoubles(Runtime::kAllocateHeapNumber);
  RecordSafepointWithRegisters(instr->pointer_map(), 0,
                               Safepoint::kNoLazyDeopt);
  __ StoreToSafepointRegisterSlot(r2, reg);
}

void LCodeGen::DoSmiTag(LSmiTag* instr) {
  HChange* hchange = instr->hydrogen();
  Register input = ToRegister(instr->value());
  Register output = ToRegister(instr->result());
  if (hchange->CheckFlag(HValue::kCanOverflow) &&
      hchange->value()->CheckFlag(HValue::kUint32)) {
    __ TestUnsignedSmiCandidate(input, r0);
    DeoptimizeIf(ne, instr, DeoptimizeReason::kOverflow, cr0);
  }
#if !V8_TARGET_ARCH_S390X
  if (hchange->CheckFlag(HValue::kCanOverflow) &&
      !hchange->value()->CheckFlag(HValue::kUint32)) {
    __ SmiTagCheckOverflow(output, input, r0);
    DeoptimizeIf(lt, instr, DeoptimizeReason::kOverflow, cr0);
  } else {
#endif
    __ SmiTag(output, input);
#if !V8_TARGET_ARCH_S390X
  }
#endif
}

void LCodeGen::DoSmiUntag(LSmiUntag* instr) {
  Register input = ToRegister(instr->value());
  Register result = ToRegister(instr->result());
  if (instr->needs_check()) {
    __ tmll(input, Operand(kHeapObjectTag));
    DeoptimizeIf(ne, instr, DeoptimizeReason::kNotASmi, cr0);
    __ SmiUntag(result, input);
  } else {
    __ SmiUntag(result, input);
  }
}

void LCodeGen::EmitNumberUntagD(LNumberUntagD* instr, Register input_reg,
                                DoubleRegister result_reg,
                                NumberUntagDMode mode) {
  bool can_convert_undefined_to_nan = instr->truncating();
  bool deoptimize_on_minus_zero = instr->hydrogen()->deoptimize_on_minus_zero();

  Register scratch = scratch0();
  DCHECK(!result_reg.is(double_scratch0()));

  Label convert, load_smi, done;

  if (mode == NUMBER_CANDIDATE_IS_ANY_TAGGED) {
    // Smi check.
    __ UntagAndJumpIfSmi(scratch, input_reg, &load_smi);

    // Heap number map check.
    __ LoadP(scratch, FieldMemOperand(input_reg, HeapObject::kMapOffset));
    __ CmpP(scratch, RootMemOperand(Heap::kHeapNumberMapRootIndex));

    if (can_convert_undefined_to_nan) {
      __ bne(&convert, Label::kNear);
    } else {
      DeoptimizeIf(ne, instr, DeoptimizeReason::kNotAHeapNumber);
    }
    // load heap number
    __ LoadDouble(result_reg,
                  FieldMemOperand(input_reg, HeapNumber::kValueOffset));
    if (deoptimize_on_minus_zero) {
      __ TestDoubleIsMinusZero(result_reg, scratch, ip);
      DeoptimizeIf(eq, instr, DeoptimizeReason::kMinusZero);
    }
    __ b(&done, Label::kNear);
    if (can_convert_undefined_to_nan) {
      __ bind(&convert);
      // Convert undefined (and hole) to NaN.
      __ CompareRoot(input_reg, Heap::kUndefinedValueRootIndex);
      DeoptimizeIf(ne, instr, DeoptimizeReason::kNotAHeapNumberUndefined);
      __ LoadRoot(scratch, Heap::kNanValueRootIndex);
      __ LoadDouble(result_reg,
                    FieldMemOperand(scratch, HeapNumber::kValueOffset));
      __ b(&done, Label::kNear);
    }
  } else {
    __ SmiUntag(scratch, input_reg);
    DCHECK(mode == NUMBER_CANDIDATE_IS_SMI);
  }
  // Smi to double register conversion
  __ bind(&load_smi);
  // scratch: untagged value of input_reg
  __ ConvertIntToDouble(result_reg, scratch);
  __ bind(&done);
}

void LCodeGen::DoDeferredTaggedToI(LTaggedToI* instr) {
  Register input_reg = ToRegister(instr->value());
  Register scratch1 = scratch0();
  Register scratch2 = ToRegister(instr->temp());
  DoubleRegister double_scratch = double_scratch0();
  DoubleRegister double_scratch2 = ToDoubleRegister(instr->temp2());

  DCHECK(!scratch1.is(input_reg) && !scratch1.is(scratch2));
  DCHECK(!scratch2.is(input_reg) && !scratch2.is(scratch1));

  Label done;

  // Heap number map check.
  __ LoadP(scratch1, FieldMemOperand(input_reg, HeapObject::kMapOffset));
  __ CompareRoot(scratch1, Heap::kHeapNumberMapRootIndex);

  if (instr->truncating()) {
    Label truncate;
    __ beq(&truncate);
    __ CompareInstanceType(scratch1, scratch1, ODDBALL_TYPE);
    DeoptimizeIf(ne, instr, DeoptimizeReason::kNotANumberOrOddball);
    __ bind(&truncate);
    __ LoadRR(scratch2, input_reg);
    __ TruncateHeapNumberToI(input_reg, scratch2);
  } else {
    // Deoptimize if we don't have a heap number.
    DeoptimizeIf(ne, instr, DeoptimizeReason::kNotAHeapNumber);

    __ LoadDouble(double_scratch2,
                  FieldMemOperand(input_reg, HeapNumber::kValueOffset));
    if (instr->hydrogen()->CheckFlag(HValue::kBailoutOnMinusZero)) {
      // preserve heap number pointer in scratch2 for minus zero check below
      __ LoadRR(scratch2, input_reg);
    }
    __ TryDoubleToInt32Exact(input_reg, double_scratch2, scratch1,
                             double_scratch);
    DeoptimizeIf(ne, instr, DeoptimizeReason::kLostPrecisionOrNaN);

    if (instr->hydrogen()->CheckFlag(HValue::kBailoutOnMinusZero)) {
      __ CmpP(input_reg, Operand::Zero());
      __ bne(&done, Label::kNear);
      __ TestHeapNumberSign(scratch2, scratch1);
      DeoptimizeIf(lt, instr, DeoptimizeReason::kMinusZero);
    }
  }
  __ bind(&done);
}

void LCodeGen::DoTaggedToI(LTaggedToI* instr) {
  class DeferredTaggedToI final : public LDeferredCode {
   public:
    DeferredTaggedToI(LCodeGen* codegen, LTaggedToI* instr)
        : LDeferredCode(codegen), instr_(instr) {}
    void Generate() override { codegen()->DoDeferredTaggedToI(instr_); }
    LInstruction* instr() override { return instr_; }

   private:
    LTaggedToI* instr_;
  };

  LOperand* input = instr->value();
  DCHECK(input->IsRegister());
  DCHECK(input->Equals(instr->result()));

  Register input_reg = ToRegister(input);

  if (instr->hydrogen()->value()->representation().IsSmi()) {
    __ SmiUntag(input_reg);
  } else {
    DeferredTaggedToI* deferred = new (zone()) DeferredTaggedToI(this, instr);

    // Branch to deferred code if the input is a HeapObject.
    __ JumpIfNotSmi(input_reg, deferred->entry());

    __ SmiUntag(input_reg);
    __ bind(deferred->exit());
  }
}

void LCodeGen::DoNumberUntagD(LNumberUntagD* instr) {
  LOperand* input = instr->value();
  DCHECK(input->IsRegister());
  LOperand* result = instr->result();
  DCHECK(result->IsDoubleRegister());

  Register input_reg = ToRegister(input);
  DoubleRegister result_reg = ToDoubleRegister(result);

  HValue* value = instr->hydrogen()->value();
  NumberUntagDMode mode = value->representation().IsSmi()
                              ? NUMBER_CANDIDATE_IS_SMI
                              : NUMBER_CANDIDATE_IS_ANY_TAGGED;

  EmitNumberUntagD(instr, input_reg, result_reg, mode);
}

void LCodeGen::DoDoubleToI(LDoubleToI* instr) {
  Register result_reg = ToRegister(instr->result());
  Register scratch1 = scratch0();
  DoubleRegister double_input = ToDoubleRegister(instr->value());
  DoubleRegister double_scratch = double_scratch0();

  if (instr->truncating()) {
    __ TruncateDoubleToI(result_reg, double_input);
  } else {
    __ TryDoubleToInt32Exact(result_reg, double_input, scratch1,
                             double_scratch);
    // Deoptimize if the input wasn't a int32 (inside a double).
    DeoptimizeIf(ne, instr, DeoptimizeReason::kLostPrecisionOrNaN);
    if (instr->hydrogen()->CheckFlag(HValue::kBailoutOnMinusZero)) {
      Label done;
      __ CmpP(result_reg, Operand::Zero());
      __ bne(&done, Label::kNear);
      __ TestDoubleSign(double_input, scratch1);
      DeoptimizeIf(lt, instr, DeoptimizeReason::kMinusZero);
      __ bind(&done);
    }
  }
}

void LCodeGen::DoDoubleToSmi(LDoubleToSmi* instr) {
  Register result_reg = ToRegister(instr->result());
  Register scratch1 = scratch0();
  DoubleRegister double_input = ToDoubleRegister(instr->value());
  DoubleRegister double_scratch = double_scratch0();

  if (instr->truncating()) {
    __ TruncateDoubleToI(result_reg, double_input);
  } else {
    __ TryDoubleToInt32Exact(result_reg, double_input, scratch1,
                             double_scratch);
    // Deoptimize if the input wasn't a int32 (inside a double).
    DeoptimizeIf(ne, instr, DeoptimizeReason::kLostPrecisionOrNaN);
    if (instr->hydrogen()->CheckFlag(HValue::kBailoutOnMinusZero)) {
      Label done;
      __ CmpP(result_reg, Operand::Zero());
      __ bne(&done, Label::kNear);
      __ TestDoubleSign(double_input, scratch1);
      DeoptimizeIf(lt, instr, DeoptimizeReason::kMinusZero);
      __ bind(&done);
    }
  }
#if V8_TARGET_ARCH_S390X
  __ SmiTag(result_reg);
#else
  __ SmiTagCheckOverflow(result_reg, r0);
  DeoptimizeIf(lt, instr, DeoptimizeReason::kOverflow, cr0);
#endif
}

void LCodeGen::DoCheckSmi(LCheckSmi* instr) {
  LOperand* input = instr->value();
  if (input->IsRegister()) {
    __ TestIfSmi(ToRegister(input));
  } else if (input->IsStackSlot()) {
    MemOperand value = ToMemOperand(input);
#if !V8_TARGET_LITTLE_ENDIAN
#if V8_TARGET_ARCH_S390X
    __ TestIfSmi(MemOperand(value.rb(), value.offset() + 7));
#else
    __ TestIfSmi(MemOperand(value.rb(), value.offset() + 3));
#endif
#else
    __ TestIfSmi(value);
#endif
  }
  DeoptimizeIf(ne, instr, DeoptimizeReason::kNotASmi, cr0);
}

void LCodeGen::DoCheckNonSmi(LCheckNonSmi* instr) {
  if (!instr->hydrogen()->value()->type().IsHeapObject()) {
    LOperand* input = instr->value();
    if (input->IsRegister()) {
      __ TestIfSmi(ToRegister(input));
    } else if (input->IsStackSlot()) {
      MemOperand value = ToMemOperand(input);
#if !V8_TARGET_LITTLE_ENDIAN
#if V8_TARGET_ARCH_S390X
      __ TestIfSmi(MemOperand(value.rb(), value.offset() + 7));
#else
      __ TestIfSmi(MemOperand(value.rb(), value.offset() + 3));
#endif
#else
      __ TestIfSmi(value);
#endif
    } else {
      UNIMPLEMENTED();
    }
    DeoptimizeIf(eq, instr, DeoptimizeReason::kSmi, cr0);
  }
}

void LCodeGen::DoCheckArrayBufferNotNeutered(
    LCheckArrayBufferNotNeutered* instr) {
  Register view = ToRegister(instr->view());
  Register scratch = scratch0();

  __ LoadP(scratch, FieldMemOperand(view, JSArrayBufferView::kBufferOffset));
  __ LoadlW(scratch, FieldMemOperand(scratch, JSArrayBuffer::kBitFieldOffset));
  __ And(r0, scratch, Operand(1 << JSArrayBuffer::WasNeutered::kShift));
  DeoptimizeIf(ne, instr, DeoptimizeReason::kOutOfBounds, cr0);
}

void LCodeGen::DoCheckInstanceType(LCheckInstanceType* instr) {
  Register input = ToRegister(instr->value());
  Register scratch = scratch0();

  __ LoadP(scratch, FieldMemOperand(input, HeapObject::kMapOffset));

  if (instr->hydrogen()->is_interval_check()) {
    InstanceType first;
    InstanceType last;
    instr->hydrogen()->GetCheckInterval(&first, &last);

    __ CmpLogicalByte(FieldMemOperand(scratch, Map::kInstanceTypeOffset),
                      Operand(first));

    // If there is only one type in the interval check for equality.
    if (first == last) {
      DeoptimizeIf(ne, instr, DeoptimizeReason::kWrongInstanceType);
    } else {
      DeoptimizeIf(lt, instr, DeoptimizeReason::kWrongInstanceType);
      // Omit check for the last type.
      if (last != LAST_TYPE) {
        __ CmpLogicalByte(FieldMemOperand(scratch, Map::kInstanceTypeOffset),
                          Operand(last));
        DeoptimizeIf(gt, instr, DeoptimizeReason::kWrongInstanceType);
      }
    }
  } else {
    uint8_t mask;
    uint8_t tag;
    instr->hydrogen()->GetCheckMaskAndTag(&mask, &tag);

    __ LoadlB(scratch, FieldMemOperand(scratch, Map::kInstanceTypeOffset));

    if (base::bits::IsPowerOfTwo32(mask)) {
      DCHECK(tag == 0 || base::bits::IsPowerOfTwo32(tag));
      __ AndP(scratch, Operand(mask));
      DeoptimizeIf(tag == 0 ? ne : eq, instr,
                   DeoptimizeReason::kWrongInstanceType);
    } else {
      __ AndP(scratch, Operand(mask));
      __ CmpP(scratch, Operand(tag));
      DeoptimizeIf(ne, instr, DeoptimizeReason::kWrongInstanceType);
    }
  }
}

void LCodeGen::DoCheckValue(LCheckValue* instr) {
  Register reg = ToRegister(instr->value());
  Handle<HeapObject> object = instr->hydrogen()->object().handle();
  AllowDeferredHandleDereference smi_check;
  if (isolate()->heap()->InNewSpace(*object)) {
    Register reg = ToRegister(instr->value());
    Handle<Cell> cell = isolate()->factory()->NewCell(object);
    __ mov(ip, Operand(cell));
    __ CmpP(reg, FieldMemOperand(ip, Cell::kValueOffset));
  } else {
    __ CmpP(reg, Operand(object));
  }
  DeoptimizeIf(ne, instr, DeoptimizeReason::kValueMismatch);
}

void LCodeGen::DoDeferredInstanceMigration(LCheckMaps* instr, Register object) {
  Register temp = ToRegister(instr->temp());
  Label deopt, done;
  // If the map is not deprecated the migration attempt does not make sense.
  __ LoadP(temp, FieldMemOperand(object, HeapObject::kMapOffset));
  __ LoadlW(temp, FieldMemOperand(temp, Map::kBitField3Offset));
  __ TestBitMask(temp, Map::Deprecated::kMask, r0);
  __ beq(&deopt);

  {
    PushSafepointRegistersScope scope(this);
    __ push(object);
    __ LoadImmP(cp, Operand::Zero());
    __ CallRuntimeSaveDoubles(Runtime::kTryMigrateInstance);
    RecordSafepointWithRegisters(instr->pointer_map(), 1,
                                 Safepoint::kNoLazyDeopt);
    __ StoreToSafepointRegisterSlot(r2, temp);
  }
  __ TestIfSmi(temp);
  __ bne(&done);

  __ bind(&deopt);
  // In case of "al" condition the operand is not used so just pass cr0 there.
  DeoptimizeIf(al, instr, DeoptimizeReason::kInstanceMigrationFailed, cr0);

  __ bind(&done);
}

void LCodeGen::DoCheckMaps(LCheckMaps* instr) {
  class DeferredCheckMaps final : public LDeferredCode {
   public:
    DeferredCheckMaps(LCodeGen* codegen, LCheckMaps* instr, Register object)
        : LDeferredCode(codegen), instr_(instr), object_(object) {
      SetExit(check_maps());
    }
    void Generate() override {
      codegen()->DoDeferredInstanceMigration(instr_, object_);
    }
    Label* check_maps() { return &check_maps_; }
    LInstruction* instr() override { return instr_; }

   private:
    LCheckMaps* instr_;
    Label check_maps_;
    Register object_;
  };

  if (instr->hydrogen()->IsStabilityCheck()) {
    const UniqueSet<Map>* maps = instr->hydrogen()->maps();
    for (int i = 0; i < maps->size(); ++i) {
      AddStabilityDependency(maps->at(i).handle());
    }
    return;
  }

  LOperand* input = instr->value();
  DCHECK(input->IsRegister());
  Register reg = ToRegister(input);

  DeferredCheckMaps* deferred = NULL;
  if (instr->hydrogen()->HasMigrationTarget()) {
    deferred = new (zone()) DeferredCheckMaps(this, instr, reg);
    __ bind(deferred->check_maps());
  }

  const UniqueSet<Map>* maps = instr->hydrogen()->maps();
  Label success;
  for (int i = 0; i < maps->size() - 1; i++) {
    Handle<Map> map = maps->at(i).handle();
    __ CompareMap(reg, map, &success);
    __ beq(&success);
  }

  Handle<Map> map = maps->at(maps->size() - 1).handle();
  __ CompareMap(reg, map, &success);
  if (instr->hydrogen()->HasMigrationTarget()) {
    __ bne(deferred->entry());
  } else {
    DeoptimizeIf(ne, instr, DeoptimizeReason::kWrongMap);
  }

  __ bind(&success);
}

void LCodeGen::DoClampDToUint8(LClampDToUint8* instr) {
  DoubleRegister value_reg = ToDoubleRegister(instr->unclamped());
  Register result_reg = ToRegister(instr->result());
  __ ClampDoubleToUint8(result_reg, value_reg, double_scratch0());
}

void LCodeGen::DoClampIToUint8(LClampIToUint8* instr) {
  Register unclamped_reg = ToRegister(instr->unclamped());
  Register result_reg = ToRegister(instr->result());
  __ ClampUint8(result_reg, unclamped_reg);
}

void LCodeGen::DoClampTToUint8(LClampTToUint8* instr) {
  Register scratch = scratch0();
  Register input_reg = ToRegister(instr->unclamped());
  Register result_reg = ToRegister(instr->result());
  DoubleRegister temp_reg = ToDoubleRegister(instr->temp());
  Label is_smi, done, heap_number;

  // Both smi and heap number cases are handled.
  __ UntagAndJumpIfSmi(result_reg, input_reg, &is_smi);

  // Check for heap number
  __ LoadP(scratch, FieldMemOperand(input_reg, HeapObject::kMapOffset));
  __ CmpP(scratch, Operand(factory()->heap_number_map()));
  __ beq(&heap_number, Label::kNear);

  // Check for undefined. Undefined is converted to zero for clamping
  // conversions.
  __ CmpP(input_reg, Operand(factory()->undefined_value()));
  DeoptimizeIf(ne, instr, DeoptimizeReason::kNotAHeapNumberUndefined);
  __ LoadImmP(result_reg, Operand::Zero());
  __ b(&done, Label::kNear);

  // Heap number
  __ bind(&heap_number);
  __ LoadDouble(temp_reg, FieldMemOperand(input_reg, HeapNumber::kValueOffset));
  __ ClampDoubleToUint8(result_reg, temp_reg, double_scratch0());
  __ b(&done, Label::kNear);

  // smi
  __ bind(&is_smi);
  __ ClampUint8(result_reg, result_reg);

  __ bind(&done);
}

void LCodeGen::DoAllocate(LAllocate* instr) {
  class DeferredAllocate final : public LDeferredCode {
   public:
    DeferredAllocate(LCodeGen* codegen, LAllocate* instr)
        : LDeferredCode(codegen), instr_(instr) {}
    void Generate() override { codegen()->DoDeferredAllocate(instr_); }
    LInstruction* instr() override { return instr_; }

   private:
    LAllocate* instr_;
  };

  DeferredAllocate* deferred = new (zone()) DeferredAllocate(this, instr);

  Register result = ToRegister(instr->result());
  Register scratch = ToRegister(instr->temp1());
  Register scratch2 = ToRegister(instr->temp2());

  // Allocate memory for the object.
  AllocationFlags flags = NO_ALLOCATION_FLAGS;
  if (instr->hydrogen()->MustAllocateDoubleAligned()) {
    flags = static_cast<AllocationFlags>(flags | DOUBLE_ALIGNMENT);
  }
  if (instr->hydrogen()->IsOldSpaceAllocation()) {
    DCHECK(!instr->hydrogen()->IsNewSpaceAllocation());
    flags = static_cast<AllocationFlags>(flags | PRETENURE);
  }

  if (instr->hydrogen()->IsAllocationFoldingDominator()) {
    flags = static_cast<AllocationFlags>(flags | ALLOCATION_FOLDING_DOMINATOR);
  }

  DCHECK(!instr->hydrogen()->IsAllocationFolded());

  if (instr->size()->IsConstantOperand()) {
    int32_t size = ToInteger32(LConstantOperand::cast(instr->size()));
    CHECK(size <= kMaxRegularHeapObjectSize);
    __ Allocate(size, result, scratch, scratch2, deferred->entry(), flags);
  } else {
    Register size = ToRegister(instr->size());
    __ Allocate(size, result, scratch, scratch2, deferred->entry(), flags);
  }

  __ bind(deferred->exit());

  if (instr->hydrogen()->MustPrefillWithFiller()) {
    if (instr->size()->IsConstantOperand()) {
      int32_t size = ToInteger32(LConstantOperand::cast(instr->size()));
      __ LoadIntLiteral(scratch, size);
    } else {
      scratch = ToRegister(instr->size());
    }
    __ lay(scratch, MemOperand(scratch, -kPointerSize));
    Label loop;
    __ mov(scratch2, Operand(isolate()->factory()->one_pointer_filler_map()));
    __ bind(&loop);
    __ StoreP(scratch2, MemOperand(scratch, result, -kHeapObjectTag));
#if V8_TARGET_ARCH_S390X
    __ lay(scratch, MemOperand(scratch, -kPointerSize));
#else
    // TODO(joransiu): Improve the following sequence.
    // Need to use AHI instead of LAY as top nibble is not set with LAY, causing
    // incorrect result with the signed compare
    __ AddP(scratch, Operand(-kPointerSize));
#endif
    __ CmpP(scratch, Operand::Zero());
    __ bge(&loop);
  }
}

void LCodeGen::DoDeferredAllocate(LAllocate* instr) {
  Register result = ToRegister(instr->result());

  // TODO(3095996): Get rid of this. For now, we need to make the
  // result register contain a valid pointer because it is already
  // contained in the register pointer map.
  __ LoadSmiLiteral(result, Smi::kZero);

  PushSafepointRegistersScope scope(this);
  if (instr->size()->IsRegister()) {
    Register size = ToRegister(instr->size());
    DCHECK(!size.is(result));
    __ SmiTag(size);
    __ push(size);
  } else {
    int32_t size = ToInteger32(LConstantOperand::cast(instr->size()));
#if !V8_TARGET_ARCH_S390X
    if (size >= 0 && size <= Smi::kMaxValue) {
#endif
      __ Push(Smi::FromInt(size));
#if !V8_TARGET_ARCH_S390X
    } else {
      // We should never get here at runtime => abort
      __ stop("invalid allocation size");
      return;
    }
#endif
  }

  int flags = AllocateDoubleAlignFlag::encode(
      instr->hydrogen()->MustAllocateDoubleAligned());
  if (instr->hydrogen()->IsOldSpaceAllocation()) {
    DCHECK(!instr->hydrogen()->IsNewSpaceAllocation());
    flags = AllocateTargetSpace::update(flags, OLD_SPACE);
  } else {
    flags = AllocateTargetSpace::update(flags, NEW_SPACE);
  }
  __ Push(Smi::FromInt(flags));

  CallRuntimeFromDeferred(Runtime::kAllocateInTargetSpace, 2, instr,
                          instr->context());
  __ StoreToSafepointRegisterSlot(r2, result);

  if (instr->hydrogen()->IsAllocationFoldingDominator()) {
    AllocationFlags allocation_flags = NO_ALLOCATION_FLAGS;
    if (instr->hydrogen()->IsOldSpaceAllocation()) {
      DCHECK(!instr->hydrogen()->IsNewSpaceAllocation());
      allocation_flags = static_cast<AllocationFlags>(flags | PRETENURE);
    }
    // If the allocation folding dominator allocate triggered a GC, allocation
    // happend in the runtime. We have to reset the top pointer to virtually
    // undo the allocation.
    ExternalReference allocation_top =
        AllocationUtils::GetAllocationTopReference(isolate(), allocation_flags);
    Register top_address = scratch0();
    __ SubP(r2, r2, Operand(kHeapObjectTag));
    __ mov(top_address, Operand(allocation_top));
    __ StoreP(r2, MemOperand(top_address));
    __ AddP(r2, r2, Operand(kHeapObjectTag));
  }
}

void LCodeGen::DoFastAllocate(LFastAllocate* instr) {
  DCHECK(instr->hydrogen()->IsAllocationFolded());
  DCHECK(!instr->hydrogen()->IsAllocationFoldingDominator());
  Register result = ToRegister(instr->result());
  Register scratch1 = ToRegister(instr->temp1());
  Register scratch2 = ToRegister(instr->temp2());

  AllocationFlags flags = ALLOCATION_FOLDED;
  if (instr->hydrogen()->MustAllocateDoubleAligned()) {
    flags = static_cast<AllocationFlags>(flags | DOUBLE_ALIGNMENT);
  }
  if (instr->hydrogen()->IsOldSpaceAllocation()) {
    DCHECK(!instr->hydrogen()->IsNewSpaceAllocation());
    flags = static_cast<AllocationFlags>(flags | PRETENURE);
  }
  if (instr->size()->IsConstantOperand()) {
    int32_t size = ToInteger32(LConstantOperand::cast(instr->size()));
    CHECK(size <= kMaxRegularHeapObjectSize);
    __ FastAllocate(size, result, scratch1, scratch2, flags);
  } else {
    Register size = ToRegister(instr->size());
    __ FastAllocate(size, result, scratch1, scratch2, flags);
  }
}

void LCodeGen::DoTypeof(LTypeof* instr) {
  DCHECK(ToRegister(instr->value()).is(r5));
  DCHECK(ToRegister(instr->result()).is(r2));
  Label end, do_call;
  Register value_register = ToRegister(instr->value());
  __ JumpIfNotSmi(value_register, &do_call);
  __ mov(r2, Operand(isolate()->factory()->number_string()));
  __ b(&end);
  __ bind(&do_call);
  Callable callable = CodeFactory::Typeof(isolate());
  CallCode(callable.code(), RelocInfo::CODE_TARGET, instr);
  __ bind(&end);
}

void LCodeGen::DoTypeofIsAndBranch(LTypeofIsAndBranch* instr) {
  Register input = ToRegister(instr->value());

  Condition final_branch_condition =
      EmitTypeofIs(instr->TrueLabel(chunk_), instr->FalseLabel(chunk_), input,
                   instr->type_literal());
  if (final_branch_condition != kNoCondition) {
    EmitBranch(instr, final_branch_condition);
  }
}

Condition LCodeGen::EmitTypeofIs(Label* true_label, Label* false_label,
                                 Register input, Handle<String> type_name) {
  Condition final_branch_condition = kNoCondition;
  Register scratch = scratch0();
  Factory* factory = isolate()->factory();
  if (String::Equals(type_name, factory->number_string())) {
    __ JumpIfSmi(input, true_label);
    __ LoadP(scratch, FieldMemOperand(input, HeapObject::kMapOffset));
    __ CompareRoot(scratch, Heap::kHeapNumberMapRootIndex);
    final_branch_condition = eq;

  } else if (String::Equals(type_name, factory->string_string())) {
    __ JumpIfSmi(input, false_label);
    __ CompareObjectType(input, scratch, no_reg, FIRST_NONSTRING_TYPE);
    final_branch_condition = lt;

  } else if (String::Equals(type_name, factory->symbol_string())) {
    __ JumpIfSmi(input, false_label);
    __ CompareObjectType(input, scratch, no_reg, SYMBOL_TYPE);
    final_branch_condition = eq;

  } else if (String::Equals(type_name, factory->boolean_string())) {
    __ CompareRoot(input, Heap::kTrueValueRootIndex);
    __ beq(true_label);
    __ CompareRoot(input, Heap::kFalseValueRootIndex);
    final_branch_condition = eq;

  } else if (String::Equals(type_name, factory->undefined_string())) {
    __ CompareRoot(input, Heap::kNullValueRootIndex);
    __ beq(false_label);
    __ JumpIfSmi(input, false_label);
    // Check for undetectable objects => true.
    __ LoadP(scratch, FieldMemOperand(input, HeapObject::kMapOffset));
    __ LoadlB(scratch, FieldMemOperand(scratch, Map::kBitFieldOffset));
    __ ExtractBit(r0, scratch, Map::kIsUndetectable);
    __ CmpP(r0, Operand::Zero());
    final_branch_condition = ne;

  } else if (String::Equals(type_name, factory->function_string())) {
    __ JumpIfSmi(input, false_label);
    __ LoadP(scratch, FieldMemOperand(input, HeapObject::kMapOffset));
    __ LoadlB(scratch, FieldMemOperand(scratch, Map::kBitFieldOffset));
    __ AndP(scratch, scratch,
            Operand((1 << Map::kIsCallable) | (1 << Map::kIsUndetectable)));
    __ CmpP(scratch, Operand(1 << Map::kIsCallable));
    final_branch_condition = eq;

  } else if (String::Equals(type_name, factory->object_string())) {
    __ JumpIfSmi(input, false_label);
    __ CompareRoot(input, Heap::kNullValueRootIndex);
    __ beq(true_label);
    STATIC_ASSERT(LAST_JS_RECEIVER_TYPE == LAST_TYPE);
    __ CompareObjectType(input, scratch, ip, FIRST_JS_RECEIVER_TYPE);
    __ blt(false_label);
    // Check for callable or undetectable objects => false.
    __ LoadlB(scratch, FieldMemOperand(scratch, Map::kBitFieldOffset));
    __ AndP(r0, scratch,
            Operand((1 << Map::kIsCallable) | (1 << Map::kIsUndetectable)));
    __ CmpP(r0, Operand::Zero());
    final_branch_condition = eq;

  } else {
    __ b(false_label);
  }

  return final_branch_condition;
}

void LCodeGen::EnsureSpaceForLazyDeopt(int space_needed) {
  if (info()->ShouldEnsureSpaceForLazyDeopt()) {
    // Ensure that we have enough space after the previous lazy-bailout
    // instruction for patching the code here.
    int current_pc = masm()->pc_offset();
    if (current_pc < last_lazy_deopt_pc_ + space_needed) {
      int padding_size = last_lazy_deopt_pc_ + space_needed - current_pc;
      DCHECK_EQ(0, padding_size % 2);
      while (padding_size > 0) {
        __ nop();
        padding_size -= 2;
      }
    }
  }
  last_lazy_deopt_pc_ = masm()->pc_offset();
}

void LCodeGen::DoLazyBailout(LLazyBailout* instr) {
  last_lazy_deopt_pc_ = masm()->pc_offset();
  DCHECK(instr->HasEnvironment());
  LEnvironment* env = instr->environment();
  RegisterEnvironmentForDeoptimization(env, Safepoint::kLazyDeopt);
  safepoints_.RecordLazyDeoptimizationIndex(env->deoptimization_index());
}

void LCodeGen::DoDeoptimize(LDeoptimize* instr) {
  Deoptimizer::BailoutType type = instr->hydrogen()->type();
  // TODO(danno): Stubs expect all deopts to be lazy for historical reasons (the
  // needed return address), even though the implementation of LAZY and EAGER is
  // now identical. When LAZY is eventually completely folded into EAGER, remove
  // the special case below.
  if (info()->IsStub() && type == Deoptimizer::EAGER) {
    type = Deoptimizer::LAZY;
  }

  DeoptimizeIf(al, instr, instr->hydrogen()->reason(), type);
}

void LCodeGen::DoDummy(LDummy* instr) {
  // Nothing to see here, move on!
}

void LCodeGen::DoDummyUse(LDummyUse* instr) {
  // Nothing to see here, move on!
}

void LCodeGen::DoDeferredStackCheck(LStackCheck* instr) {
  PushSafepointRegistersScope scope(this);
  LoadContextFromDeferred(instr->context());
  __ CallRuntimeSaveDoubles(Runtime::kStackGuard);
  RecordSafepointWithLazyDeopt(
      instr, RECORD_SAFEPOINT_WITH_REGISTERS_AND_NO_ARGUMENTS);
  DCHECK(instr->HasEnvironment());
  LEnvironment* env = instr->environment();
  safepoints_.RecordLazyDeoptimizationIndex(env->deoptimization_index());
}

void LCodeGen::DoStackCheck(LStackCheck* instr) {
  class DeferredStackCheck final : public LDeferredCode {
   public:
    DeferredStackCheck(LCodeGen* codegen, LStackCheck* instr)
        : LDeferredCode(codegen), instr_(instr) {}
    void Generate() override { codegen()->DoDeferredStackCheck(instr_); }
    LInstruction* instr() override { return instr_; }

   private:
    LStackCheck* instr_;
  };

  DCHECK(instr->HasEnvironment());
  LEnvironment* env = instr->environment();
  // There is no LLazyBailout instruction for stack-checks. We have to
  // prepare for lazy deoptimization explicitly here.
  if (instr->hydrogen()->is_function_entry()) {
    // Perform stack overflow check.
    Label done;
    __ CmpLogicalP(sp, RootMemOperand(Heap::kStackLimitRootIndex));
    __ bge(&done, Label::kNear);
    DCHECK(instr->context()->IsRegister());
    DCHECK(ToRegister(instr->context()).is(cp));
    CallCode(isolate()->builtins()->StackCheck(), RelocInfo::CODE_TARGET,
             instr);
    __ bind(&done);
  } else {
    DCHECK(instr->hydrogen()->is_backwards_branch());
    // Perform stack overflow check if this goto needs it before jumping.
    DeferredStackCheck* deferred_stack_check =
        new (zone()) DeferredStackCheck(this, instr);
    __ CmpLogicalP(sp, RootMemOperand(Heap::kStackLimitRootIndex));
    __ blt(deferred_stack_check->entry());
    EnsureSpaceForLazyDeopt(Deoptimizer::patch_size());
    __ bind(instr->done_label());
    deferred_stack_check->SetExit(instr->done_label());
    RegisterEnvironmentForDeoptimization(env, Safepoint::kLazyDeopt);
    // Don't record a deoptimization index for the safepoint here.
    // This will be done explicitly when emitting call and the safepoint in
    // the deferred code.
  }
}

void LCodeGen::DoOsrEntry(LOsrEntry* instr) {
  // This is a pseudo-instruction that ensures that the environment here is
  // properly registered for deoptimization and records the assembler's PC
  // offset.
  LEnvironment* environment = instr->environment();

  // If the environment were already registered, we would have no way of
  // backpatching it with the spill slot operands.
  DCHECK(!environment->HasBeenRegistered());
  RegisterEnvironmentForDeoptimization(environment, Safepoint::kNoLazyDeopt);

  GenerateOsrPrologue();
}

void LCodeGen::DoForInPrepareMap(LForInPrepareMap* instr) {
  Label use_cache, call_runtime;
  __ CheckEnumCache(&call_runtime);

  __ LoadP(r2, FieldMemOperand(r2, HeapObject::kMapOffset));
  __ b(&use_cache);

  // Get the set of properties to enumerate.
  __ bind(&call_runtime);
  __ push(r2);
  CallRuntime(Runtime::kForInEnumerate, instr);
  __ bind(&use_cache);
}

void LCodeGen::DoForInCacheArray(LForInCacheArray* instr) {
  Register map = ToRegister(instr->map());
  Register result = ToRegister(instr->result());
  Label load_cache, done;
  __ EnumLength(result, map);
  __ CmpSmiLiteral(result, Smi::kZero, r0);
  __ bne(&load_cache, Label::kNear);
  __ mov(result, Operand(isolate()->factory()->empty_fixed_array()));
  __ b(&done, Label::kNear);

  __ bind(&load_cache);
  __ LoadInstanceDescriptors(map, result);
  __ LoadP(result,
           FieldMemOperand(result, DescriptorArray::kEnumCacheBridgeOffset));
  __ LoadP(result, FieldMemOperand(result, FixedArray::SizeFor(instr->idx())));
  __ CmpP(result, Operand::Zero());
  DeoptimizeIf(eq, instr, DeoptimizeReason::kNoCache);

  __ bind(&done);
}

void LCodeGen::DoCheckMapValue(LCheckMapValue* instr) {
  Register object = ToRegister(instr->value());
  Register map = ToRegister(instr->map());
  __ LoadP(scratch0(), FieldMemOperand(object, HeapObject::kMapOffset));
  __ CmpP(map, scratch0());
  DeoptimizeIf(ne, instr, DeoptimizeReason::kWrongMap);
}

void LCodeGen::DoDeferredLoadMutableDouble(LLoadFieldByIndex* instr,
                                           Register result, Register object,
                                           Register index) {
  PushSafepointRegistersScope scope(this);
  __ Push(object, index);
  __ LoadImmP(cp, Operand::Zero());
  __ CallRuntimeSaveDoubles(Runtime::kLoadMutableDouble);
  RecordSafepointWithRegisters(instr->pointer_map(), 2,
                               Safepoint::kNoLazyDeopt);
  __ StoreToSafepointRegisterSlot(r2, result);
}

void LCodeGen::DoLoadFieldByIndex(LLoadFieldByIndex* instr) {
  class DeferredLoadMutableDouble final : public LDeferredCode {
   public:
    DeferredLoadMutableDouble(LCodeGen* codegen, LLoadFieldByIndex* instr,
                              Register result, Register object, Register index)
        : LDeferredCode(codegen),
          instr_(instr),
          result_(result),
          object_(object),
          index_(index) {}
    void Generate() override {
      codegen()->DoDeferredLoadMutableDouble(instr_, result_, object_, index_);
    }
    LInstruction* instr() override { return instr_; }

   private:
    LLoadFieldByIndex* instr_;
    Register result_;
    Register object_;
    Register index_;
  };

  Register object = ToRegister(instr->object());
  Register index = ToRegister(instr->index());
  Register result = ToRegister(instr->result());
  Register scratch = scratch0();

  DeferredLoadMutableDouble* deferred;
  deferred = new (zone())
      DeferredLoadMutableDouble(this, instr, result, object, index);

  Label out_of_object, done;

  __ TestBitMask(index, reinterpret_cast<uintptr_t>(Smi::FromInt(1)), r0);
  __ bne(deferred->entry());
  __ ShiftRightArithP(index, index, Operand(1));

  __ CmpP(index, Operand::Zero());
  __ blt(&out_of_object, Label::kNear);

  __ SmiToPtrArrayOffset(r0, index);
  __ AddP(scratch, object, r0);
  __ LoadP(result, FieldMemOperand(scratch, JSObject::kHeaderSize));

  __ b(&done, Label::kNear);

  __ bind(&out_of_object);
  __ LoadP(result, FieldMemOperand(object, JSObject::kPropertiesOffset));
  // Index is equal to negated out of object property index plus 1.
  __ SmiToPtrArrayOffset(r0, index);
  __ SubP(scratch, result, r0);
  __ LoadP(result,
           FieldMemOperand(scratch, FixedArray::kHeaderSize - kPointerSize));
  __ bind(deferred->exit());
  __ bind(&done);
}

#undef __

}  // namespace internal
}  // namespace v8
