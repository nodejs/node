// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/crankshaft/ppc/lithium-codegen-ppc.h"

#include "src/base/bits.h"
#include "src/code-factory.h"
#include "src/code-stubs.h"
#include "src/crankshaft/hydrogen-osr.h"
#include "src/crankshaft/ppc/lithium-gap-resolver-ppc.h"
#include "src/ic/ic.h"
#include "src/ic/stub-cache.h"
#include "src/profiler/cpu-profiler.h"

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


#define __ masm()->

bool LCodeGen::GenerateCode() {
  LPhase phase("Z_Code generation", chunk());
  DCHECK(is_unused());
  status_ = GENERATING;

  // Open a frame scope to indicate that there is a frame on the stack.  The
  // NONE indicates that the scope shouldn't actually generate code to set up
  // the frame (that is done in GeneratePrologue).
  FrameScope frame_scope(masm_, StackFrame::NONE);

  bool rc = GeneratePrologue() && GenerateBody() && GenerateDeferredCode() &&
            GenerateJumpTable() && GenerateSafepointTable();
  if (FLAG_enable_embedded_constant_pool && !rc) {
    masm()->AbortConstantPoolBuilding();
  }
  return rc;
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
    __ stfd(DoubleRegister::from_code(save_iterator.Current()),
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
    __ lfd(DoubleRegister::from_code(save_iterator.Current()),
           MemOperand(sp, count * kDoubleSize));
    save_iterator.Advance();
    count++;
  }
}


bool LCodeGen::GeneratePrologue() {
  DCHECK(is_generating());

  if (info()->IsOptimizing()) {
    ProfileEntryHookStub::MaybeCallEntryHook(masm_);

    // r4: Callee's JS function.
    // cp: Callee's context.
    // pp: Callee's constant pool pointer (if enabled)
    // fp: Caller's frame pointer.
    // lr: Caller's pc.
    // ip: Our own function entry (required by the prologue)
  }

  int prologue_offset = masm_->pc_offset();

  if (prologue_offset) {
    // Prologue logic requires it's starting address in ip and the
    // corresponding offset from the function entry.
    prologue_offset += Instruction::kInstrSize;
    __ addi(ip, ip, Operand(prologue_offset));
  }
  info()->set_prologue_offset(prologue_offset);
  if (NeedsEagerFrame()) {
    if (info()->IsStub()) {
      __ StubPrologue(ip, prologue_offset);
    } else {
      __ Prologue(info()->GeneratePreagedPrologue(), ip, prologue_offset);
    }
    frame_is_built_ = true;
  }

  // Reserve space for the stack slots needed by the code.
  int slots = GetStackSlotCount();
  if (slots > 0) {
    __ subi(sp, sp, Operand(slots * kPointerSize));
    if (FLAG_debug_code) {
      __ Push(r3, r4);
      __ li(r0, Operand(slots));
      __ mtctr(r0);
      __ addi(r3, sp, Operand((slots + 2) * kPointerSize));
      __ mov(r4, Operand(kSlotsZapValue));
      Label loop;
      __ bind(&loop);
      __ StorePU(r4, MemOperand(r3, -kPointerSize));
      __ bdnz(&loop);
      __ Pop(r3, r4);
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
  if (info()->scope()->num_heap_slots() > 0) {
    Comment(";;; Allocate local context");
    bool need_write_barrier = true;
    // Argument to NewContext is the function, which is in r4.
    int slots = info()->scope()->num_heap_slots() - Context::MIN_CONTEXT_SLOTS;
    Safepoint::DeoptMode deopt_mode = Safepoint::kNoLazyDeopt;
    if (info()->scope()->is_script_scope()) {
      __ push(r4);
      __ Push(info()->scope()->GetScopeInfo(info()->isolate()));
      __ CallRuntime(Runtime::kNewScriptContext);
      deopt_mode = Safepoint::kLazyDeopt;
    } else if (slots <= FastNewContextStub::kMaximumSlots) {
      FastNewContextStub stub(isolate(), slots);
      __ CallStub(&stub);
      // Result of FastNewContextStub is always in new space.
      need_write_barrier = false;
    } else {
      __ push(r4);
      __ CallRuntime(Runtime::kNewFunctionContext);
    }
    RecordSafepoint(deopt_mode);

    // Context is returned in both r3 and cp.  It replaces the context
    // passed to us.  It's saved in the stack and kept live in cp.
    __ mr(cp, r3);
    __ StoreP(r3, MemOperand(fp, StandardFrameConstants::kContextOffset));
    // Copy any necessary parameters into the context.
    int num_parameters = scope()->num_parameters();
    int first_parameter = scope()->has_this_declaration() ? -1 : 0;
    for (int i = first_parameter; i < num_parameters; i++) {
      Variable* var = (i == -1) ? scope()->receiver() : scope()->parameter(i);
      if (var->IsContextSlot()) {
        int parameter_offset = StandardFrameConstants::kCallerSPOffset +
                               (num_parameters - 1 - i) * kPointerSize;
        // Load parameter from stack.
        __ LoadP(r3, MemOperand(fp, parameter_offset));
        // Store it in the context.
        MemOperand target = ContextMemOperand(cp, var->index());
        __ StoreP(r3, target, r0);
        // Update the write barrier. This clobbers r6 and r3.
        if (need_write_barrier) {
          __ RecordWriteContextSlot(cp, target.offset(), r3, r6,
                                    GetLinkRegisterState(), kSaveFPRegs);
        } else if (FLAG_debug_code) {
          Label done;
          __ JumpIfInNewSpace(cp, r3, &done);
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
  __ subi(sp, sp, Operand(slots * kPointerSize));
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
      RecordAndWritePosition(
          chunk()->graph()->SourcePositionToScriptPosition(value->position()));

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
        __ LoadSmiLiteral(scratch0(), Smi::FromInt(StackFrame::STUB));
        __ PushFixedFrame(scratch0());
        __ addi(fp, sp, Operand(StandardFrameConstants::kFixedFrameSizeFromFp));
        Comment(";;; Deferred code");
      }
      code->Generate();
      if (NeedsDeferredFrame()) {
        Comment(";;; Destroy frame");
        DCHECK(frame_is_built_);
        __ PopFixedFrame(ip);
        frame_is_built_ = false;
      }
      __ b(code->exit());
    }
  }

  return !is_aborted();
}


bool LCodeGen::GenerateJumpTable() {
  // Check that the jump table is accessible from everywhere in the function
  // code, i.e. that offsets to the table can be encoded in the 24bit signed
  // immediate of a branch instruction.
  // To simplify we consider the code size from the first instruction to the
  // end of the jump table. We also don't consider the pc load delta.
  // Each entry in the jump table generates one instruction and inlines one
  // 32bit data after it.
  if (!is_int24((masm()->pc_offset() / Assembler::kInstrSize) +
                jump_table_.length() * 7)) {
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
        __ PushFixedFrame();
        __ b(&needs_frame, SetLK);
      } else {
        __ b(&call_deopt_entry, SetLK);
      }
      info()->LogDeoptCallPosition(masm()->pc_offset(),
                                   table_entry->deopt_info.inlining_id);
    }

    if (needs_frame.is_linked()) {
      __ bind(&needs_frame);
      // This variant of deopt can only be used with stubs. Since we don't
      // have a function pointer to install in the stack frame that we're
      // building, install a special marker there instead.
      DCHECK(info()->IsStub());
      __ LoadSmiLiteral(ip, Smi::FromInt(StackFrame::STUB));
      __ push(ip);
      __ addi(fp, sp, Operand(StandardFrameConstants::kFixedFrameSizeFromFp));
    }

    Comment(";;; call deopt");
    __ bind(&call_deopt_entry);

    if (info()->saves_caller_doubles()) {
      DCHECK(info()->IsStub());
      RestoreCallerDoubles();
    }

    // Add the base address to the offset previously loaded in entry_offset.
    __ mov(ip, Operand(ExternalReference::ForDeoptEntry(base)));
    __ add(ip, entry_offset, ip);
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
                            Deoptimizer::DeoptReason deopt_reason,
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
    CRegister alt_cr = cr6;
    Register scratch = scratch0();
    ExternalReference count = ExternalReference::stress_deopt_count(isolate());
    Label no_deopt;
    DCHECK(!alt_cr.is(cr));
    __ Push(r4, scratch);
    __ mov(scratch, Operand(count));
    __ lwz(r4, MemOperand(scratch));
    __ subi(r4, r4, Operand(1));
    __ cmpi(r4, Operand::Zero(), alt_cr);
    __ bne(&no_deopt, alt_cr);
    __ li(r4, Operand(FLAG_deopt_every_n_times));
    __ stw(r4, MemOperand(scratch));
    __ Pop(r4, scratch);

    __ Call(entry, RelocInfo::RUNTIME_ENTRY);
    __ bind(&no_deopt);
    __ stw(r4, MemOperand(scratch));
    __ Pop(r4, scratch);
  }

  if (info()->ShouldTrapOnDeopt()) {
    __ stop("trap_on_deopt", cond, kDefaultStopCode, cr);
  }

  Deoptimizer::DeoptInfo deopt_info = MakeDeoptInfo(instr, deopt_reason);

  DCHECK(info()->IsStub() || frame_is_built_);
  // Go through jump table if we need to handle condition, build frame, or
  // restore caller doubles.
  if (cond == al && frame_is_built_ && !info()->saves_caller_doubles()) {
    DeoptComment(deopt_info);
    __ Call(entry, RelocInfo::RUNTIME_ENTRY);
    info()->LogDeoptCallPosition(masm()->pc_offset(), deopt_info.inlining_id);
  } else {
    Deoptimizer::JumpTableEntry table_entry(entry, deopt_info, bailout_type,
                                            !frame_is_built_);
    // We often have several deopts to the same entry, reuse the last
    // jump entry if this is the case.
    if (FLAG_trace_deopt || isolate()->cpu_profiler()->is_profiling() ||
        jump_table_.is_empty() ||
        !table_entry.IsEquivalentTo(jump_table_.last())) {
      jump_table_.Add(table_entry, zone());
    }
    __ b(cond, &jump_table_.last().label, cr);
  }
}


void LCodeGen::DeoptimizeIf(Condition condition, LInstruction* instr,
                            Deoptimizer::DeoptReason deopt_reason,
                            CRegister cr) {
  Deoptimizer::BailoutType bailout_type =
      info()->IsStub() ? Deoptimizer::LAZY : Deoptimizer::EAGER;
  DeoptimizeIf(condition, instr, deopt_reason, bailout_type, cr);
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


void LCodeGen::RecordAndWritePosition(int position) {
  if (position == RelocInfo::kNoPosition) return;
  masm()->positions_recorder()->RecordPosition(position);
  masm()->positions_recorder()->WriteRecordedPositions();
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
    __ cmpwi(dividend, Operand::Zero());
    __ bge(&dividend_is_not_negative);
    if (shift) {
      // Note that this is correct even for kMinInt operands.
      __ neg(dividend, dividend);
      __ ExtractBitRange(dividend, dividend, shift - 1, 0);
      __ neg(dividend, dividend, LeaveOE, SetRC);
      if (hmod->CheckFlag(HValue::kBailoutOnMinusZero)) {
        DeoptimizeIf(eq, instr, Deoptimizer::kMinusZero, cr0);
      }
    } else if (!hmod->CheckFlag(HValue::kBailoutOnMinusZero)) {
      __ li(dividend, Operand::Zero());
    } else {
      DeoptimizeIf(al, instr, Deoptimizer::kMinusZero);
    }
    __ b(&done);
  }

  __ bind(&dividend_is_not_negative);
  if (shift) {
    __ ExtractBitRange(dividend, dividend, shift - 1, 0);
  } else {
    __ li(dividend, Operand::Zero());
  }
  __ bind(&done);
}


void LCodeGen::DoModByConstI(LModByConstI* instr) {
  Register dividend = ToRegister(instr->dividend());
  int32_t divisor = instr->divisor();
  Register result = ToRegister(instr->result());
  DCHECK(!dividend.is(result));

  if (divisor == 0) {
    DeoptimizeIf(al, instr, Deoptimizer::kDivisionByZero);
    return;
  }

  __ TruncatingDiv(result, dividend, Abs(divisor));
  __ mov(ip, Operand(Abs(divisor)));
  __ mullw(result, result, ip);
  __ sub(result, dividend, result, LeaveOE, SetRC);

  // Check for negative zero.
  HMod* hmod = instr->hydrogen();
  if (hmod->CheckFlag(HValue::kBailoutOnMinusZero)) {
    Label remainder_not_zero;
    __ bne(&remainder_not_zero, cr0);
    __ cmpwi(dividend, Operand::Zero());
    DeoptimizeIf(lt, instr, Deoptimizer::kMinusZero);
    __ bind(&remainder_not_zero);
  }
}


void LCodeGen::DoModI(LModI* instr) {
  HMod* hmod = instr->hydrogen();
  Register left_reg = ToRegister(instr->left());
  Register right_reg = ToRegister(instr->right());
  Register result_reg = ToRegister(instr->result());
  Register scratch = scratch0();
  bool can_overflow = hmod->CheckFlag(HValue::kCanOverflow);
  Label done;

  if (can_overflow) {
    __ li(r0, Operand::Zero());  // clear xer
    __ mtxer(r0);
  }

  __ divw(scratch, left_reg, right_reg, SetOE, SetRC);

  // Check for x % 0.
  if (hmod->CheckFlag(HValue::kCanBeDivByZero)) {
    __ cmpwi(right_reg, Operand::Zero());
    DeoptimizeIf(eq, instr, Deoptimizer::kDivisionByZero);
  }

  // Check for kMinInt % -1, divw will return undefined, which is not what we
  // want. We have to deopt if we care about -0, because we can't return that.
  if (can_overflow) {
    if (hmod->CheckFlag(HValue::kBailoutOnMinusZero)) {
      DeoptimizeIf(overflow, instr, Deoptimizer::kMinusZero, cr0);
    } else {
      if (CpuFeatures::IsSupported(ISELECT)) {
        __ isel(overflow, result_reg, r0, result_reg, cr0);
        __ boverflow(&done, cr0);
      } else {
        Label no_overflow_possible;
        __ bnooverflow(&no_overflow_possible, cr0);
        __ li(result_reg, Operand::Zero());
        __ b(&done);
        __ bind(&no_overflow_possible);
      }
    }
  }

  __ mullw(scratch, right_reg, scratch);
  __ sub(result_reg, left_reg, scratch, LeaveOE, SetRC);

  // If we care about -0, test if the dividend is <0 and the result is 0.
  if (hmod->CheckFlag(HValue::kBailoutOnMinusZero)) {
    __ bne(&done, cr0);
    __ cmpwi(left_reg, Operand::Zero());
    DeoptimizeIf(lt, instr, Deoptimizer::kMinusZero);
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
    __ cmpwi(dividend, Operand::Zero());
    DeoptimizeIf(eq, instr, Deoptimizer::kMinusZero);
  }
  // Check for (kMinInt / -1).
  if (hdiv->CheckFlag(HValue::kCanOverflow) && divisor == -1) {
    __ lis(r0, Operand(SIGN_EXT_IMM16(0x8000)));
    __ cmpw(dividend, r0);
    DeoptimizeIf(eq, instr, Deoptimizer::kOverflow);
  }

  int32_t shift = WhichPowerOf2Abs(divisor);

  // Deoptimize if remainder will not be 0.
  if (!hdiv->CheckFlag(HInstruction::kAllUsesTruncatingToInt32) && shift) {
    __ TestBitRange(dividend, shift - 1, 0, r0);
    DeoptimizeIf(ne, instr, Deoptimizer::kLostPrecision, cr0);
  }

  if (divisor == -1) {  // Nice shortcut, not needed for correctness.
    __ neg(result, dividend);
    return;
  }
  if (shift == 0) {
    __ mr(result, dividend);
  } else {
    if (shift == 1) {
      __ srwi(result, dividend, Operand(31));
    } else {
      __ srawi(result, dividend, 31);
      __ srwi(result, result, Operand(32 - shift));
    }
    __ add(result, dividend, result);
    __ srawi(result, result, shift);
  }
  if (divisor < 0) __ neg(result, result);
}


void LCodeGen::DoDivByConstI(LDivByConstI* instr) {
  Register dividend = ToRegister(instr->dividend());
  int32_t divisor = instr->divisor();
  Register result = ToRegister(instr->result());
  DCHECK(!dividend.is(result));

  if (divisor == 0) {
    DeoptimizeIf(al, instr, Deoptimizer::kDivisionByZero);
    return;
  }

  // Check for (0 / -x) that will produce negative zero.
  HDiv* hdiv = instr->hydrogen();
  if (hdiv->CheckFlag(HValue::kBailoutOnMinusZero) && divisor < 0) {
    __ cmpwi(dividend, Operand::Zero());
    DeoptimizeIf(eq, instr, Deoptimizer::kMinusZero);
  }

  __ TruncatingDiv(result, dividend, Abs(divisor));
  if (divisor < 0) __ neg(result, result);

  if (!hdiv->CheckFlag(HInstruction::kAllUsesTruncatingToInt32)) {
    Register scratch = scratch0();
    __ mov(ip, Operand(divisor));
    __ mullw(scratch, result, ip);
    __ cmpw(scratch, dividend);
    DeoptimizeIf(ne, instr, Deoptimizer::kLostPrecision);
  }
}


// TODO(svenpanne) Refactor this to avoid code duplication with DoFlooringDivI.
void LCodeGen::DoDivI(LDivI* instr) {
  HBinaryOperation* hdiv = instr->hydrogen();
  const Register dividend = ToRegister(instr->dividend());
  const Register divisor = ToRegister(instr->divisor());
  Register result = ToRegister(instr->result());
  bool can_overflow = hdiv->CheckFlag(HValue::kCanOverflow);

  DCHECK(!dividend.is(result));
  DCHECK(!divisor.is(result));

  if (can_overflow) {
    __ li(r0, Operand::Zero());  // clear xer
    __ mtxer(r0);
  }

  __ divw(result, dividend, divisor, SetOE, SetRC);

  // Check for x / 0.
  if (hdiv->CheckFlag(HValue::kCanBeDivByZero)) {
    __ cmpwi(divisor, Operand::Zero());
    DeoptimizeIf(eq, instr, Deoptimizer::kDivisionByZero);
  }

  // Check for (0 / -x) that will produce negative zero.
  if (hdiv->CheckFlag(HValue::kBailoutOnMinusZero)) {
    Label dividend_not_zero;
    __ cmpwi(dividend, Operand::Zero());
    __ bne(&dividend_not_zero);
    __ cmpwi(divisor, Operand::Zero());
    DeoptimizeIf(lt, instr, Deoptimizer::kMinusZero);
    __ bind(&dividend_not_zero);
  }

  // Check for (kMinInt / -1).
  if (can_overflow) {
    if (!hdiv->CheckFlag(HValue::kAllUsesTruncatingToInt32)) {
      DeoptimizeIf(overflow, instr, Deoptimizer::kOverflow, cr0);
    } else {
      // When truncating, we want kMinInt / -1 = kMinInt.
      if (CpuFeatures::IsSupported(ISELECT)) {
        __ isel(overflow, result, dividend, result, cr0);
      } else {
        Label no_overflow_possible;
        __ bnooverflow(&no_overflow_possible, cr0);
        __ mr(result, dividend);
        __ bind(&no_overflow_possible);
      }
    }
  }

  if (!hdiv->CheckFlag(HInstruction::kAllUsesTruncatingToInt32)) {
    // Deoptimize if remainder is not 0.
    Register scratch = scratch0();
    __ mullw(scratch, divisor, result);
    __ cmpw(dividend, scratch);
    DeoptimizeIf(ne, instr, Deoptimizer::kLostPrecision);
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
      __ srawi(result, dividend, shift);
    }
    return;
  }

  // If the divisor is negative, we have to negate and handle edge cases.
  OEBit oe = LeaveOE;
#if V8_TARGET_ARCH_PPC64
  if (divisor == -1 && can_overflow) {
    __ lis(r0, Operand(SIGN_EXT_IMM16(0x8000)));
    __ cmpw(dividend, r0);
    DeoptimizeIf(eq, instr, Deoptimizer::kOverflow);
  }
#else
  if (can_overflow) {
    __ li(r0, Operand::Zero());  // clear xer
    __ mtxer(r0);
    oe = SetOE;
  }
#endif

  __ neg(result, dividend, oe, SetRC);
  if (hdiv->CheckFlag(HValue::kBailoutOnMinusZero)) {
    DeoptimizeIf(eq, instr, Deoptimizer::kMinusZero, cr0);
  }

// If the negation could not overflow, simply shifting is OK.
#if !V8_TARGET_ARCH_PPC64
  if (!can_overflow) {
#endif
    if (shift) {
      __ ShiftRightArithImm(result, result, shift);
    }
    return;
#if !V8_TARGET_ARCH_PPC64
  }

  // Dividing by -1 is basically negation, unless we overflow.
  if (divisor == -1) {
    DeoptimizeIf(overflow, instr, Deoptimizer::kOverflow, cr0);
    return;
  }

  Label overflow, done;
  __ boverflow(&overflow, cr0);
  __ srawi(result, result, shift);
  __ b(&done);
  __ bind(&overflow);
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
    DeoptimizeIf(al, instr, Deoptimizer::kDivisionByZero);
    return;
  }

  // Check for (0 / -x) that will produce negative zero.
  HMathFloorOfDiv* hdiv = instr->hydrogen();
  if (hdiv->CheckFlag(HValue::kBailoutOnMinusZero) && divisor < 0) {
    __ cmpwi(dividend, Operand::Zero());
    DeoptimizeIf(eq, instr, Deoptimizer::kMinusZero);
  }

  // Easy case: We need no dynamic check for the dividend and the flooring
  // division is the same as the truncating division.
  if ((divisor > 0 && !hdiv->CheckFlag(HValue::kLeftCanBeNegative)) ||
      (divisor < 0 && !hdiv->CheckFlag(HValue::kLeftCanBePositive))) {
    __ TruncatingDiv(result, dividend, Abs(divisor));
    if (divisor < 0) __ neg(result, result);
    return;
  }

  // In the general case we may need to adjust before and after the truncating
  // division to get a flooring division.
  Register temp = ToRegister(instr->temp());
  DCHECK(!temp.is(dividend) && !temp.is(result));
  Label needs_adjustment, done;
  __ cmpwi(dividend, Operand::Zero());
  __ b(divisor > 0 ? lt : gt, &needs_adjustment);
  __ TruncatingDiv(result, dividend, Abs(divisor));
  if (divisor < 0) __ neg(result, result);
  __ b(&done);
  __ bind(&needs_adjustment);
  __ addi(temp, dividend, Operand(divisor > 0 ? 1 : -1));
  __ TruncatingDiv(result, temp, Abs(divisor));
  if (divisor < 0) __ neg(result, result);
  __ subi(result, result, Operand(1));
  __ bind(&done);
}


// TODO(svenpanne) Refactor this to avoid code duplication with DoDivI.
void LCodeGen::DoFlooringDivI(LFlooringDivI* instr) {
  HBinaryOperation* hdiv = instr->hydrogen();
  const Register dividend = ToRegister(instr->dividend());
  const Register divisor = ToRegister(instr->divisor());
  Register result = ToRegister(instr->result());
  bool can_overflow = hdiv->CheckFlag(HValue::kCanOverflow);

  DCHECK(!dividend.is(result));
  DCHECK(!divisor.is(result));

  if (can_overflow) {
    __ li(r0, Operand::Zero());  // clear xer
    __ mtxer(r0);
  }

  __ divw(result, dividend, divisor, SetOE, SetRC);

  // Check for x / 0.
  if (hdiv->CheckFlag(HValue::kCanBeDivByZero)) {
    __ cmpwi(divisor, Operand::Zero());
    DeoptimizeIf(eq, instr, Deoptimizer::kDivisionByZero);
  }

  // Check for (0 / -x) that will produce negative zero.
  if (hdiv->CheckFlag(HValue::kBailoutOnMinusZero)) {
    Label dividend_not_zero;
    __ cmpwi(dividend, Operand::Zero());
    __ bne(&dividend_not_zero);
    __ cmpwi(divisor, Operand::Zero());
    DeoptimizeIf(lt, instr, Deoptimizer::kMinusZero);
    __ bind(&dividend_not_zero);
  }

  // Check for (kMinInt / -1).
  if (can_overflow) {
    if (!hdiv->CheckFlag(HValue::kAllUsesTruncatingToInt32)) {
      DeoptimizeIf(overflow, instr, Deoptimizer::kOverflow, cr0);
    } else {
      // When truncating, we want kMinInt / -1 = kMinInt.
      if (CpuFeatures::IsSupported(ISELECT)) {
        __ isel(overflow, result, dividend, result, cr0);
      } else {
        Label no_overflow_possible;
        __ bnooverflow(&no_overflow_possible, cr0);
        __ mr(result, dividend);
        __ bind(&no_overflow_possible);
      }
    }
  }

  Label done;
  Register scratch = scratch0();
// If both operands have the same sign then we are done.
#if V8_TARGET_ARCH_PPC64
  __ xor_(scratch, dividend, divisor);
  __ cmpwi(scratch, Operand::Zero());
  __ bge(&done);
#else
  __ xor_(scratch, dividend, divisor, SetRC);
  __ bge(&done, cr0);
#endif

  // If there is no remainder then we are done.
  __ mullw(scratch, divisor, result);
  __ cmpw(dividend, scratch);
  __ beq(&done);

  // We performed a truncating division. Correct the result.
  __ subi(result, result, Operand(1));
  __ bind(&done);
}


void LCodeGen::DoMultiplyAddD(LMultiplyAddD* instr) {
  DoubleRegister addend = ToDoubleRegister(instr->addend());
  DoubleRegister multiplier = ToDoubleRegister(instr->multiplier());
  DoubleRegister multiplicand = ToDoubleRegister(instr->multiplicand());
  DoubleRegister result = ToDoubleRegister(instr->result());

  __ fmadd(result, multiplier, multiplicand, addend);
}


void LCodeGen::DoMultiplySubD(LMultiplySubD* instr) {
  DoubleRegister minuend = ToDoubleRegister(instr->minuend());
  DoubleRegister multiplier = ToDoubleRegister(instr->multiplier());
  DoubleRegister multiplicand = ToDoubleRegister(instr->multiplicand());
  DoubleRegister result = ToDoubleRegister(instr->result());

  __ fmsub(result, multiplier, multiplicand, minuend);
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
      __ cmpi(left, Operand::Zero());
      DeoptimizeIf(eq, instr, Deoptimizer::kMinusZero);
    }

    switch (constant) {
      case -1:
        if (can_overflow) {
#if V8_TARGET_ARCH_PPC64
          if (instr->hydrogen()->representation().IsSmi()) {
#endif
            __ li(r0, Operand::Zero());  // clear xer
            __ mtxer(r0);
            __ neg(result, left, SetOE, SetRC);
            DeoptimizeIf(overflow, instr, Deoptimizer::kOverflow, cr0);
#if V8_TARGET_ARCH_PPC64
          } else {
            __ neg(result, left);
            __ TestIfInt32(result, r0);
            DeoptimizeIf(ne, instr, Deoptimizer::kOverflow);
          }
#endif
        } else {
          __ neg(result, left);
        }
        break;
      case 0:
        if (bailout_on_minus_zero) {
// If left is strictly negative and the constant is null, the
// result is -0. Deoptimize if required, otherwise return 0.
#if V8_TARGET_ARCH_PPC64
          if (instr->hydrogen()->representation().IsSmi()) {
#endif
            __ cmpi(left, Operand::Zero());
#if V8_TARGET_ARCH_PPC64
          } else {
            __ cmpwi(left, Operand::Zero());
          }
#endif
          DeoptimizeIf(lt, instr, Deoptimizer::kMinusZero);
        }
        __ li(result, Operand::Zero());
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
          __ ShiftLeftImm(result, left, Operand(shift));
          // Correct the sign of the result if the constant is negative.
          if (constant < 0) __ neg(result, result);
        } else if (base::bits::IsPowerOfTwo32(constant_abs - 1)) {
          int32_t shift = WhichPowerOf2(constant_abs - 1);
          __ ShiftLeftImm(scratch, left, Operand(shift));
          __ add(result, scratch, left);
          // Correct the sign of the result if the constant is negative.
          if (constant < 0) __ neg(result, result);
        } else if (base::bits::IsPowerOfTwo32(constant_abs + 1)) {
          int32_t shift = WhichPowerOf2(constant_abs + 1);
          __ ShiftLeftImm(scratch, left, Operand(shift));
          __ sub(result, scratch, left);
          // Correct the sign of the result if the constant is negative.
          if (constant < 0) __ neg(result, result);
        } else {
          // Generate standard code.
          __ mov(ip, Operand(constant));
          __ Mul(result, left, ip);
        }
    }

  } else {
    DCHECK(right_op->IsRegister());
    Register right = ToRegister(right_op);

    if (can_overflow) {
#if V8_TARGET_ARCH_PPC64
      // result = left * right.
      if (instr->hydrogen()->representation().IsSmi()) {
        __ SmiUntag(result, left);
        __ SmiUntag(scratch, right);
        __ Mul(result, result, scratch);
      } else {
        __ Mul(result, left, right);
      }
      __ TestIfInt32(result, r0);
      DeoptimizeIf(ne, instr, Deoptimizer::kOverflow);
      if (instr->hydrogen()->representation().IsSmi()) {
        __ SmiTag(result);
      }
#else
      // scratch:result = left * right.
      if (instr->hydrogen()->representation().IsSmi()) {
        __ SmiUntag(result, left);
        __ mulhw(scratch, result, right);
        __ mullw(result, result, right);
      } else {
        __ mulhw(scratch, left, right);
        __ mullw(result, left, right);
      }
      __ TestIfInt32(scratch, result, r0);
      DeoptimizeIf(ne, instr, Deoptimizer::kOverflow);
#endif
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
#if V8_TARGET_ARCH_PPC64
      if (instr->hydrogen()->representation().IsSmi()) {
#endif
        __ xor_(r0, left, right, SetRC);
        __ bge(&done, cr0);
#if V8_TARGET_ARCH_PPC64
      } else {
        __ xor_(r0, left, right);
        __ cmpwi(r0, Operand::Zero());
        __ bge(&done);
      }
#endif
      // Bail out if the result is minus zero.
      __ cmpi(result, Operand::Zero());
      DeoptimizeIf(eq, instr, Deoptimizer::kMinusZero);
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
  Operand right(no_reg);

  if (right_op->IsStackSlot()) {
    right = Operand(EmitLoadRegister(right_op, ip));
  } else {
    DCHECK(right_op->IsRegister() || right_op->IsConstantOperand());
    right = ToOperand(right_op);

    if (right_op->IsConstantOperand() && is_uint16(right.immediate())) {
      switch (instr->op()) {
        case Token::BIT_AND:
          __ andi(result, left, right);
          break;
        case Token::BIT_OR:
          __ ori(result, left, right);
          break;
        case Token::BIT_XOR:
          __ xori(result, left, right);
          break;
        default:
          UNREACHABLE();
          break;
      }
      return;
    }
  }

  switch (instr->op()) {
    case Token::BIT_AND:
      __ And(result, left, right);
      break;
    case Token::BIT_OR:
      __ Or(result, left, right);
      break;
    case Token::BIT_XOR:
      if (right_op->IsConstantOperand() && right.immediate() == int32_t(~0)) {
        __ notx(result, left);
      } else {
        __ Xor(result, left, right);
      }
      break;
    default:
      UNREACHABLE();
      break;
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
    __ andi(scratch, ToRegister(right_op), Operand(0x1F));
    switch (instr->op()) {
      case Token::ROR:
        // rotate_right(a, b) == rotate_left(a, 32 - b)
        __ subfic(scratch, scratch, Operand(32));
        __ rotlw(result, left, scratch);
        break;
      case Token::SAR:
        __ sraw(result, left, scratch);
        break;
      case Token::SHR:
        if (instr->can_deopt()) {
          __ srw(result, left, scratch, SetRC);
#if V8_TARGET_ARCH_PPC64
          __ extsw(result, result, SetRC);
#endif
          DeoptimizeIf(lt, instr, Deoptimizer::kNegativeValue, cr0);
        } else {
          __ srw(result, left, scratch);
        }
        break;
      case Token::SHL:
        __ slw(result, left, scratch);
#if V8_TARGET_ARCH_PPC64
        __ extsw(result, result);
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
          __ rotrwi(result, left, shift_count);
        } else {
          __ Move(result, left);
        }
        break;
      case Token::SAR:
        if (shift_count != 0) {
          __ srawi(result, left, shift_count);
        } else {
          __ Move(result, left);
        }
        break;
      case Token::SHR:
        if (shift_count != 0) {
          __ srwi(result, left, Operand(shift_count));
        } else {
          if (instr->can_deopt()) {
            __ cmpwi(left, Operand::Zero());
            DeoptimizeIf(lt, instr, Deoptimizer::kNegativeValue);
          }
          __ Move(result, left);
        }
        break;
      case Token::SHL:
        if (shift_count != 0) {
#if V8_TARGET_ARCH_PPC64
          if (instr->hydrogen_value()->representation().IsSmi()) {
            __ sldi(result, left, Operand(shift_count));
#else
          if (instr->hydrogen_value()->representation().IsSmi() &&
              instr->can_deopt()) {
            if (shift_count != 1) {
              __ slwi(result, left, Operand(shift_count - 1));
              __ SmiTagCheckOverflow(result, result, scratch);
            } else {
              __ SmiTagCheckOverflow(result, left, scratch);
            }
            DeoptimizeIf(lt, instr, Deoptimizer::kOverflow, cr0);
#endif
          } else {
            __ slwi(result, left, Operand(shift_count));
#if V8_TARGET_ARCH_PPC64
            __ extsw(result, result);
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
  LOperand* right = instr->right();
  Register left = ToRegister(instr->left());
  Register result = ToRegister(instr->result());
  bool can_overflow = instr->hydrogen()->CheckFlag(HValue::kCanOverflow);
#if V8_TARGET_ARCH_PPC64
  const bool isInteger = !instr->hydrogen()->representation().IsSmi();
#else
  const bool isInteger = false;
#endif
  if (!can_overflow || isInteger) {
    if (right->IsConstantOperand()) {
      __ Add(result, left, -(ToOperand(right).immediate()), r0);
    } else {
      __ sub(result, left, EmitLoadRegister(right, ip));
    }
#if V8_TARGET_ARCH_PPC64
    if (can_overflow) {
      __ TestIfInt32(result, r0);
      DeoptimizeIf(ne, instr, Deoptimizer::kOverflow);
    }
#endif
  } else {
    if (right->IsConstantOperand()) {
      __ AddAndCheckForOverflow(result, left, -(ToOperand(right).immediate()),
                                scratch0(), r0);
    } else {
      __ SubAndCheckForOverflow(result, left, EmitLoadRegister(right, ip),
                                scratch0(), r0);
    }
    DeoptimizeIf(lt, instr, Deoptimizer::kOverflow, cr0);
  }
}


void LCodeGen::DoRSubI(LRSubI* instr) {
  LOperand* left = instr->left();
  LOperand* right = instr->right();
  LOperand* result = instr->result();

  DCHECK(!instr->hydrogen()->CheckFlag(HValue::kCanOverflow) &&
         right->IsConstantOperand());

  Operand right_operand = ToOperand(right);
  if (is_int16(right_operand.immediate())) {
    __ subfic(ToRegister(result), ToRegister(left), right_operand);
  } else {
    __ mov(r0, right_operand);
    __ sub(ToRegister(result), r0, ToRegister(left));
  }
}


void LCodeGen::DoConstantI(LConstantI* instr) {
  __ mov(ToRegister(instr->result()), Operand(instr->value()));
}


void LCodeGen::DoConstantS(LConstantS* instr) {
  __ LoadSmiLiteral(ToRegister(instr->result()), instr->value());
}


void LCodeGen::DoConstantD(LConstantD* instr) {
  DCHECK(instr->result()->IsDoubleRegister());
  DoubleRegister result = ToDoubleRegister(instr->result());
#if V8_HOST_ARCH_IA32
  // Need some crappy work-around for x87 sNaN -> qNaN breakage in simulator
  // builds.
  uint64_t bits = instr->bits();
  if ((bits & V8_UINT64_C(0x7FF8000000000000)) ==
      V8_UINT64_C(0x7FF0000000000000)) {
    uint32_t lo = static_cast<uint32_t>(bits);
    uint32_t hi = static_cast<uint32_t>(bits >> 32);
    __ mov(ip, Operand(lo));
    __ mov(scratch0(), Operand(hi));
    __ MovInt64ToDouble(result, scratch0(), ip);
    return;
  }
#endif
  double v = instr->value();
  __ LoadDoubleLiteral(result, v, scratch0());
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
  if (encoding == String::ONE_BYTE_ENCODING) {
    __ add(scratch, string, ToRegister(index));
  } else {
    STATIC_ASSERT(kUC16Size == 2);
    __ ShiftLeftImm(scratch, ToRegister(index), Operand(1));
    __ add(scratch, string, scratch);
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
    __ lbz(scratch, FieldMemOperand(scratch, Map::kInstanceTypeOffset));

    __ andi(scratch, scratch,
            Operand(kStringRepresentationMask | kStringEncodingMask));
    static const uint32_t one_byte_seq_type = kSeqStringTag | kOneByteStringTag;
    static const uint32_t two_byte_seq_type = kSeqStringTag | kTwoByteStringTag;
    __ cmpi(scratch,
            Operand(encoding == String::ONE_BYTE_ENCODING ? one_byte_seq_type
                                                          : two_byte_seq_type));
    __ Check(eq, kUnexpectedStringType);
  }

  MemOperand operand = BuildSeqStringOperand(string, instr->index(), encoding);
  if (encoding == String::ONE_BYTE_ENCODING) {
    __ lbz(result, operand);
  } else {
    __ lhz(result, operand);
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
    __ stb(value, operand);
  } else {
    __ sth(value, operand);
  }
}


void LCodeGen::DoAddI(LAddI* instr) {
  LOperand* right = instr->right();
  Register left = ToRegister(instr->left());
  Register result = ToRegister(instr->result());
  bool can_overflow = instr->hydrogen()->CheckFlag(HValue::kCanOverflow);
#if V8_TARGET_ARCH_PPC64
  const bool isInteger = !(instr->hydrogen()->representation().IsSmi() ||
                           instr->hydrogen()->representation().IsExternal());
#else
  const bool isInteger = false;
#endif

  if (!can_overflow || isInteger) {
    if (right->IsConstantOperand()) {
      __ Add(result, left, ToOperand(right).immediate(), r0);
    } else {
      __ add(result, left, EmitLoadRegister(right, ip));
    }
#if V8_TARGET_ARCH_PPC64
    if (can_overflow) {
      __ TestIfInt32(result, r0);
      DeoptimizeIf(ne, instr, Deoptimizer::kOverflow);
    }
#endif
  } else {
    if (right->IsConstantOperand()) {
      __ AddAndCheckForOverflow(result, left, ToOperand(right).immediate(),
                                scratch0(), r0);
    } else {
      __ AddAndCheckForOverflow(result, left, EmitLoadRegister(right, ip),
                                scratch0(), r0);
    }
    DeoptimizeIf(lt, instr, Deoptimizer::kOverflow, cr0);
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
#if V8_TARGET_ARCH_PPC64
    if (instr->hydrogen_value()->representation().IsSmi()) {
#endif
      __ cmp(left_reg, right_reg);
#if V8_TARGET_ARCH_PPC64
    } else {
      __ cmpw(left_reg, right_reg);
    }
#endif
    if (CpuFeatures::IsSupported(ISELECT)) {
      __ isel(cond, result_reg, left_reg, right_reg);
    } else {
      __ b(cond, &return_left);
      __ Move(result_reg, right_reg);
      __ b(&done);
      __ bind(&return_left);
      __ Move(result_reg, left_reg);
      __ bind(&done);
    }
  } else {
    DCHECK(instr->hydrogen()->representation().IsDouble());
    DoubleRegister left_reg = ToDoubleRegister(left);
    DoubleRegister right_reg = ToDoubleRegister(right);
    DoubleRegister result_reg = ToDoubleRegister(instr->result());
    Label check_nan_left, check_zero, return_left, return_right, done;
    __ fcmpu(left_reg, right_reg);
    __ bunordered(&check_nan_left);
    __ beq(&check_zero);
    __ b(cond, &return_left);
    __ b(&return_right);

    __ bind(&check_zero);
    __ fcmpu(left_reg, kDoubleRegZero);
    __ bne(&return_left);  // left == right != 0.

    // At this point, both left and right are either 0 or -0.
    if (operation == HMathMinMax::kMathMin) {
      // Min: The algorithm is: -((-L) + (-R)), which in case of L and R being
      // different registers is most efficiently expressed as -((-L) - R).
      __ fneg(left_reg, left_reg);
      if (left_reg.is(right_reg)) {
        __ fadd(result_reg, left_reg, right_reg);
      } else {
        __ fsub(result_reg, left_reg, right_reg);
      }
      __ fneg(result_reg, result_reg);
    } else {
      // Max: The following works because +0 + -0 == +0
      __ fadd(result_reg, left_reg, right_reg);
    }
    __ b(&done);

    __ bind(&check_nan_left);
    __ fcmpu(left_reg, left_reg);
    __ bunordered(&return_left);  // left == NaN.

    __ bind(&return_right);
    if (!right_reg.is(result_reg)) {
      __ fmr(result_reg, right_reg);
    }
    __ b(&done);

    __ bind(&return_left);
    if (!left_reg.is(result_reg)) {
      __ fmr(result_reg, left_reg);
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
      __ fadd(result, left, right);
      break;
    case Token::SUB:
      __ fsub(result, left, right);
      break;
    case Token::MUL:
      __ fmul(result, left, right);
      break;
    case Token::DIV:
      __ fdiv(result, left, right);
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
  DCHECK(ToRegister(instr->left()).is(r4));
  DCHECK(ToRegister(instr->right()).is(r3));
  DCHECK(ToRegister(instr->result()).is(r3));

  Handle<Code> code = CodeFactory::BinaryOpIC(isolate(), instr->op()).code();
  CallCode(code, RelocInfo::CODE_TARGET, instr);
}


template <class InstrType>
void LCodeGen::EmitBranch(InstrType instr, Condition cond, CRegister cr) {
  int left_block = instr->TrueDestination(chunk_);
  int right_block = instr->FalseDestination(chunk_);

  int next_block = GetNextEmittedBlock();

  if (right_block == left_block || cond == al) {
    EmitGoto(left_block);
  } else if (left_block == next_block) {
    __ b(NegateCondition(cond), chunk_->GetAssemblyLabel(right_block), cr);
  } else if (right_block == next_block) {
    __ b(cond, chunk_->GetAssemblyLabel(left_block), cr);
  } else {
    __ b(cond, chunk_->GetAssemblyLabel(left_block), cr);
    __ b(chunk_->GetAssemblyLabel(right_block));
  }
}


template <class InstrType>
void LCodeGen::EmitTrueBranch(InstrType instr, Condition cond, CRegister cr) {
  int true_block = instr->TrueDestination(chunk_);
  __ b(cond, chunk_->GetAssemblyLabel(true_block), cr);
}


template <class InstrType>
void LCodeGen::EmitFalseBranch(InstrType instr, Condition cond, CRegister cr) {
  int false_block = instr->FalseDestination(chunk_);
  __ b(cond, chunk_->GetAssemblyLabel(false_block), cr);
}


void LCodeGen::DoDebugBreak(LDebugBreak* instr) { __ stop("LBreak"); }


void LCodeGen::DoBranch(LBranch* instr) {
  Representation r = instr->hydrogen()->value()->representation();
  DoubleRegister dbl_scratch = double_scratch0();
  const uint crZOrNaNBits = (1 << (31 - Assembler::encode_crbit(cr7, CR_EQ)) |
                             1 << (31 - Assembler::encode_crbit(cr7, CR_FU)));

  if (r.IsInteger32()) {
    DCHECK(!info()->IsStub());
    Register reg = ToRegister(instr->value());
    __ cmpwi(reg, Operand::Zero());
    EmitBranch(instr, ne);
  } else if (r.IsSmi()) {
    DCHECK(!info()->IsStub());
    Register reg = ToRegister(instr->value());
    __ cmpi(reg, Operand::Zero());
    EmitBranch(instr, ne);
  } else if (r.IsDouble()) {
    DCHECK(!info()->IsStub());
    DoubleRegister reg = ToDoubleRegister(instr->value());
    // Test the double value. Zero and NaN are false.
    __ fcmpu(reg, kDoubleRegZero, cr7);
    __ mfcr(r0);
    __ andi(r0, r0, Operand(crZOrNaNBits));
    EmitBranch(instr, eq, cr0);
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
      __ cmpi(reg, Operand::Zero());
      EmitBranch(instr, ne);
    } else if (type.IsJSArray()) {
      DCHECK(!info()->IsStub());
      EmitBranch(instr, al);
    } else if (type.IsHeapNumber()) {
      DCHECK(!info()->IsStub());
      __ lfd(dbl_scratch, FieldMemOperand(reg, HeapNumber::kValueOffset));
      // Test the double value. Zero and NaN are false.
      __ fcmpu(dbl_scratch, kDoubleRegZero, cr7);
      __ mfcr(r0);
      __ andi(r0, r0, Operand(crZOrNaNBits));
      EmitBranch(instr, eq, cr0);
    } else if (type.IsString()) {
      DCHECK(!info()->IsStub());
      __ LoadP(ip, FieldMemOperand(reg, String::kLengthOffset));
      __ cmpi(ip, Operand::Zero());
      EmitBranch(instr, ne);
    } else {
      ToBooleanStub::Types expected = instr->hydrogen()->expected_input_types();
      // Avoid deopts in the case where we've never executed this path before.
      if (expected.IsEmpty()) expected = ToBooleanStub::Types::Generic();

      if (expected.Contains(ToBooleanStub::UNDEFINED)) {
        // undefined -> false.
        __ CompareRoot(reg, Heap::kUndefinedValueRootIndex);
        __ beq(instr->FalseLabel(chunk_));
      }
      if (expected.Contains(ToBooleanStub::BOOLEAN)) {
        // Boolean -> its value.
        __ CompareRoot(reg, Heap::kTrueValueRootIndex);
        __ beq(instr->TrueLabel(chunk_));
        __ CompareRoot(reg, Heap::kFalseValueRootIndex);
        __ beq(instr->FalseLabel(chunk_));
      }
      if (expected.Contains(ToBooleanStub::NULL_TYPE)) {
        // 'null' -> false.
        __ CompareRoot(reg, Heap::kNullValueRootIndex);
        __ beq(instr->FalseLabel(chunk_));
      }

      if (expected.Contains(ToBooleanStub::SMI)) {
        // Smis: 0 -> false, all other -> true.
        __ cmpi(reg, Operand::Zero());
        __ beq(instr->FalseLabel(chunk_));
        __ JumpIfSmi(reg, instr->TrueLabel(chunk_));
      } else if (expected.NeedsMap()) {
        // If we need a map later and have a Smi -> deopt.
        __ TestIfSmi(reg, r0);
        DeoptimizeIf(eq, instr, Deoptimizer::kSmi, cr0);
      }

      const Register map = scratch0();
      if (expected.NeedsMap()) {
        __ LoadP(map, FieldMemOperand(reg, HeapObject::kMapOffset));

        if (expected.CanBeUndetectable()) {
          // Undetectable -> false.
          __ lbz(ip, FieldMemOperand(map, Map::kBitFieldOffset));
          __ TestBit(ip, Map::kIsUndetectable, r0);
          __ bne(instr->FalseLabel(chunk_), cr0);
        }
      }

      if (expected.Contains(ToBooleanStub::SPEC_OBJECT)) {
        // spec object -> true.
        __ CompareInstanceType(map, ip, FIRST_JS_RECEIVER_TYPE);
        __ bge(instr->TrueLabel(chunk_));
      }

      if (expected.Contains(ToBooleanStub::STRING)) {
        // String value -> false iff empty.
        Label not_string;
        __ CompareInstanceType(map, ip, FIRST_NONSTRING_TYPE);
        __ bge(&not_string);
        __ LoadP(ip, FieldMemOperand(reg, String::kLengthOffset));
        __ cmpi(ip, Operand::Zero());
        __ bne(instr->TrueLabel(chunk_));
        __ b(instr->FalseLabel(chunk_));
        __ bind(&not_string);
      }

      if (expected.Contains(ToBooleanStub::SYMBOL)) {
        // Symbol value -> true.
        __ CompareInstanceType(map, ip, SYMBOL_TYPE);
        __ beq(instr->TrueLabel(chunk_));
      }

      if (expected.Contains(ToBooleanStub::SIMD_VALUE)) {
        // SIMD value -> true.
        Label not_simd;
        __ CompareInstanceType(map, ip, SIMD128_VALUE_TYPE);
        __ beq(instr->TrueLabel(chunk_));
      }

      if (expected.Contains(ToBooleanStub::HEAP_NUMBER)) {
        // heap number -> false iff +0, -0, or NaN.
        Label not_heap_number;
        __ CompareRoot(map, Heap::kHeapNumberMapRootIndex);
        __ bne(&not_heap_number);
        __ lfd(dbl_scratch, FieldMemOperand(reg, HeapNumber::kValueOffset));
        // Test the double value. Zero and NaN are false.
        __ fcmpu(dbl_scratch, kDoubleRegZero, cr7);
        __ mfcr(r0);
        __ andi(r0, r0, Operand(crZOrNaNBits));
        __ bne(instr->FalseLabel(chunk_), cr0);
        __ b(instr->TrueLabel(chunk_));
        __ bind(&not_heap_number);
      }

      if (!expected.IsGeneric()) {
        // We've seen something for the first time -> deopt.
        // This can only happen if we are not generic already.
        DeoptimizeIf(al, instr, Deoptimizer::kUnexpectedObject);
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
      __ fcmpu(ToDoubleRegister(left), ToDoubleRegister(right));
      // If a NaN is involved, i.e. the result is unordered,
      // jump to false block label.
      __ bunordered(instr->FalseLabel(chunk_));
    } else {
      if (right->IsConstantOperand()) {
        int32_t value = ToInteger32(LConstantOperand::cast(right));
        if (instr->hydrogen_value()->representation().IsSmi()) {
          if (is_unsigned) {
            __ CmplSmiLiteral(ToRegister(left), Smi::FromInt(value), r0);
          } else {
            __ CmpSmiLiteral(ToRegister(left), Smi::FromInt(value), r0);
          }
        } else {
          if (is_unsigned) {
            __ Cmplwi(ToRegister(left), Operand(value), r0);
          } else {
            __ Cmpwi(ToRegister(left), Operand(value), r0);
          }
        }
      } else if (left->IsConstantOperand()) {
        int32_t value = ToInteger32(LConstantOperand::cast(left));
        if (instr->hydrogen_value()->representation().IsSmi()) {
          if (is_unsigned) {
            __ CmplSmiLiteral(ToRegister(right), Smi::FromInt(value), r0);
          } else {
            __ CmpSmiLiteral(ToRegister(right), Smi::FromInt(value), r0);
          }
        } else {
          if (is_unsigned) {
            __ Cmplwi(ToRegister(right), Operand(value), r0);
          } else {
            __ Cmpwi(ToRegister(right), Operand(value), r0);
          }
        }
        // We commuted the operands, so commute the condition.
        cond = CommuteCondition(cond);
      } else if (instr->hydrogen_value()->representation().IsSmi()) {
        if (is_unsigned) {
          __ cmpl(ToRegister(left), ToRegister(right));
        } else {
          __ cmp(ToRegister(left), ToRegister(right));
        }
      } else {
        if (is_unsigned) {
          __ cmplw(ToRegister(left), ToRegister(right));
        } else {
          __ cmpw(ToRegister(left), ToRegister(right));
        }
      }
    }
    EmitBranch(instr, cond);
  }
}


void LCodeGen::DoCmpObjectEqAndBranch(LCmpObjectEqAndBranch* instr) {
  Register left = ToRegister(instr->left());
  Register right = ToRegister(instr->right());

  __ cmp(left, right);
  EmitBranch(instr, eq);
}


void LCodeGen::DoCmpHoleAndBranch(LCmpHoleAndBranch* instr) {
  if (instr->hydrogen()->representation().IsTagged()) {
    Register input_reg = ToRegister(instr->object());
    __ mov(ip, Operand(factory()->the_hole_value()));
    __ cmp(input_reg, ip);
    EmitBranch(instr, eq);
    return;
  }

  DoubleRegister input_reg = ToDoubleRegister(instr->object());
  __ fcmpu(input_reg, input_reg);
  EmitFalseBranch(instr, ordered);

  Register scratch = scratch0();
  __ MovDoubleHighToInt(scratch, input_reg);
  __ Cmpi(scratch, Operand(kHoleNanUpper32), r0);
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
  __ TestIfSmi(input_reg, r0);
  EmitBranch(instr, eq, cr0);
}


void LCodeGen::DoIsUndetectableAndBranch(LIsUndetectableAndBranch* instr) {
  Register input = ToRegister(instr->value());
  Register temp = ToRegister(instr->temp());

  if (!instr->hydrogen()->value()->type().IsHeapObject()) {
    __ JumpIfSmi(input, instr->FalseLabel(chunk_));
  }
  __ LoadP(temp, FieldMemOperand(input, HeapObject::kMapOffset));
  __ lbz(temp, FieldMemOperand(temp, Map::kBitFieldOffset));
  __ TestBit(temp, Map::kIsUndetectable, r0);
  EmitBranch(instr, ne, cr0);
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
  DCHECK(ToRegister(instr->left()).is(r4));
  DCHECK(ToRegister(instr->right()).is(r3));

  Handle<Code> code = CodeFactory::StringCompare(isolate()).code();
  CallCode(code, RelocInfo::CODE_TARGET, instr);
  __ cmpi(r3, Operand::Zero());

  EmitBranch(instr, ComputeCompareCondition(instr->op()));
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


void LCodeGen::DoGetCachedArrayIndex(LGetCachedArrayIndex* instr) {
  Register input = ToRegister(instr->value());
  Register result = ToRegister(instr->result());

  __ AssertString(input);

  __ lwz(result, FieldMemOperand(input, String::kHashFieldOffset));
  __ IndexFromHash(result, result);
}


void LCodeGen::DoHasCachedArrayIndexAndBranch(
    LHasCachedArrayIndexAndBranch* instr) {
  Register input = ToRegister(instr->value());
  Register scratch = scratch0();

  __ lwz(scratch, FieldMemOperand(input, String::kHashFieldOffset));
  __ mov(r0, Operand(String::kContainsCachedArrayIndexMask));
  __ and_(r0, scratch, r0, SetRC);
  EmitBranch(instr, eq, cr0);
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
  __ cmpi(instance_type, Operand(JS_FUNCTION_TYPE));
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
  __ Cmpi(temp, Operand(class_name), r0);
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

  __ LoadP(temp, FieldMemOperand(reg, HeapObject::kMapOffset));
  __ Cmpi(temp, Operand(instr->map()), r0);
  EmitBranch(instr, eq);
}


void LCodeGen::DoInstanceOf(LInstanceOf* instr) {
  DCHECK(ToRegister(instr->context()).is(cp));
  DCHECK(ToRegister(instr->left()).is(InstanceOfDescriptor::LeftRegister()));
  DCHECK(ToRegister(instr->right()).is(InstanceOfDescriptor::RightRegister()));
  DCHECK(ToRegister(instr->result()).is(r3));
  InstanceOfStub stub(isolate());
  CallCode(stub.GetCode(), RelocInfo::CODE_TARGET, instr);
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
    __ TestIfSmi(object, r0);
    EmitFalseBranch(instr, eq, cr0);
  }

  // Loop through the {object}s prototype chain looking for the {prototype}.
  __ LoadP(object_map, FieldMemOperand(object, HeapObject::kMapOffset));
  Label loop;
  __ bind(&loop);

  // Deoptimize if the object needs to be access checked.
  __ lbz(object_instance_type,
         FieldMemOperand(object_map, Map::kBitFieldOffset));
  __ TestBit(object_instance_type, Map::kIsAccessCheckNeeded, r0);
  DeoptimizeIf(ne, instr, Deoptimizer::kAccessCheck, cr0);
  // Deoptimize for proxies.
  __ CompareInstanceType(object_map, object_instance_type, JS_PROXY_TYPE);
  DeoptimizeIf(eq, instr, Deoptimizer::kProxy);
  __ LoadP(object_prototype,
           FieldMemOperand(object_map, Map::kPrototypeOffset));
  __ cmp(object_prototype, prototype);
  EmitTrueBranch(instr, eq);
  __ CompareRoot(object_prototype, Heap::kNullValueRootIndex);
  EmitFalseBranch(instr, eq);
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
  __ cmpi(r3, Operand::Zero());

  Condition condition = ComputeCompareCondition(op);
  if (CpuFeatures::IsSupported(ISELECT)) {
    __ LoadRoot(r4, Heap::kTrueValueRootIndex);
    __ LoadRoot(r5, Heap::kFalseValueRootIndex);
    __ isel(condition, ToRegister(instr->result()), r4, r5);
  } else {
    Label true_value, done;

    __ b(condition, &true_value);

    __ LoadRoot(ToRegister(instr->result()), Heap::kFalseValueRootIndex);
    __ b(&done);

    __ bind(&true_value);
    __ LoadRoot(ToRegister(instr->result()), Heap::kTrueValueRootIndex);

    __ bind(&done);
  }
}


void LCodeGen::DoReturn(LReturn* instr) {
  if (FLAG_trace && info()->IsOptimizing()) {
    // Push the return value on the stack as the parameter.
    // Runtime::TraceExit returns its parameter in r3.  We're leaving the code
    // managed by the register allocator and tearing down the frame, it's
    // safe to write to the context register.
    __ push(r3);
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
      __ addi(sp, sp, Operand(sp_delta));
    }
  } else {
    DCHECK(info()->IsStub());  // Functions would need to drop one more value.
    Register reg = ToRegister(instr->parameter_count());
    // The argument count parameter is a smi
    if (NeedsEagerFrame()) {
      masm_->LeaveFrame(StackFrame::JAVA_SCRIPT);
    }
    __ SmiToPtrArrayOffset(r0, reg);
    __ add(sp, sp, r0);
  }

  __ blr();
}


template <class T>
void LCodeGen::EmitVectorLoadICRegisters(T* instr) {
  Register vector_register = ToRegister(instr->temp_vector());
  Register slot_register = LoadDescriptor::SlotRegister();
  DCHECK(vector_register.is(LoadWithVectorDescriptor::VectorRegister()));
  DCHECK(slot_register.is(r3));

  AllowDeferredHandleDereference vector_structure_check;
  Handle<TypeFeedbackVector> vector = instr->hydrogen()->feedback_vector();
  __ Move(vector_register, vector);
  // No need to allocate this register.
  FeedbackVectorSlot slot = instr->hydrogen()->slot();
  int index = vector->GetIndex(slot);
  __ LoadSmiLiteral(slot_register, Smi::FromInt(index));
}


template <class T>
void LCodeGen::EmitVectorStoreICRegisters(T* instr) {
  Register vector_register = ToRegister(instr->temp_vector());
  Register slot_register = ToRegister(instr->temp_slot());

  AllowDeferredHandleDereference vector_structure_check;
  Handle<TypeFeedbackVector> vector = instr->hydrogen()->feedback_vector();
  __ Move(vector_register, vector);
  FeedbackVectorSlot slot = instr->hydrogen()->slot();
  int index = vector->GetIndex(slot);
  __ LoadSmiLiteral(slot_register, Smi::FromInt(index));
}


void LCodeGen::DoLoadGlobalGeneric(LLoadGlobalGeneric* instr) {
  DCHECK(ToRegister(instr->context()).is(cp));
  DCHECK(ToRegister(instr->global_object())
             .is(LoadDescriptor::ReceiverRegister()));
  DCHECK(ToRegister(instr->result()).is(r3));

  __ mov(LoadDescriptor::NameRegister(), Operand(instr->name()));
  EmitVectorLoadICRegisters<LLoadGlobalGeneric>(instr);
  Handle<Code> ic = CodeFactory::LoadICInOptimizedCode(
                        isolate(), instr->typeof_mode(), PREMONOMORPHIC)
                        .code();
  CallCode(ic, RelocInfo::CODE_TARGET, instr);
}


void LCodeGen::DoLoadContextSlot(LLoadContextSlot* instr) {
  Register context = ToRegister(instr->context());
  Register result = ToRegister(instr->result());
  __ LoadP(result, ContextMemOperand(context, instr->slot_index()));
  if (instr->hydrogen()->RequiresHoleCheck()) {
    __ LoadRoot(ip, Heap::kTheHoleValueRootIndex);
    if (instr->hydrogen()->DeoptimizesOnHole()) {
      __ cmp(result, ip);
      DeoptimizeIf(eq, instr, Deoptimizer::kHole);
    } else {
      if (CpuFeatures::IsSupported(ISELECT)) {
        Register scratch = scratch0();
        __ mov(scratch, Operand(factory()->undefined_value()));
        __ cmp(result, ip);
        __ isel(eq, result, scratch, result);
      } else {
        Label skip;
        __ cmp(result, ip);
        __ bne(&skip);
        __ mov(result, Operand(factory()->undefined_value()));
        __ bind(&skip);
      }
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
    __ LoadRoot(ip, Heap::kTheHoleValueRootIndex);
    __ cmp(scratch, ip);
    if (instr->hydrogen()->DeoptimizesOnHole()) {
      DeoptimizeIf(eq, instr, Deoptimizer::kHole);
    } else {
      __ bne(&skip_assignment);
    }
  }

  __ StoreP(value, target, r0);
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
    __ lfd(result, FieldMemOperand(object, offset));
    return;
  }

  Register result = ToRegister(instr->result());
  if (!access.IsInobject()) {
    __ LoadP(result, FieldMemOperand(object, JSObject::kPropertiesOffset));
    object = result;
  }

  Representation representation = access.representation();

#if V8_TARGET_ARCH_PPC64
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


void LCodeGen::DoLoadNamedGeneric(LLoadNamedGeneric* instr) {
  DCHECK(ToRegister(instr->context()).is(cp));
  DCHECK(ToRegister(instr->object()).is(LoadDescriptor::ReceiverRegister()));
  DCHECK(ToRegister(instr->result()).is(r3));

  // Name is always in r5.
  __ mov(LoadDescriptor::NameRegister(), Operand(instr->name()));
  EmitVectorLoadICRegisters<LLoadNamedGeneric>(instr);
  Handle<Code> ic = CodeFactory::LoadICInOptimizedCode(
                        isolate(), NOT_INSIDE_TYPEOF,
                        instr->hydrogen()->initialization_state())
                        .code();
  CallCode(ic, RelocInfo::CODE_TARGET, instr);
}


void LCodeGen::DoLoadFunctionPrototype(LLoadFunctionPrototype* instr) {
  Register scratch = scratch0();
  Register function = ToRegister(instr->function());
  Register result = ToRegister(instr->result());

  // Get the prototype or initial map from the function.
  __ LoadP(result,
           FieldMemOperand(function, JSFunction::kPrototypeOrInitialMapOffset));

  // Check that the function has a prototype or an initial map.
  __ LoadRoot(ip, Heap::kTheHoleValueRootIndex);
  __ cmp(result, ip);
  DeoptimizeIf(eq, instr, Deoptimizer::kHole);

  // If the function does not have an initial map, we're done.
  if (CpuFeatures::IsSupported(ISELECT)) {
    // Get the prototype from the initial map (optimistic).
    __ LoadP(ip, FieldMemOperand(result, Map::kPrototypeOffset));
    __ CompareObjectType(result, scratch, scratch, MAP_TYPE);
    __ isel(eq, result, ip, result);
  } else {
    Label done;
    __ CompareObjectType(result, scratch, scratch, MAP_TYPE);
    __ bne(&done);

    // Get the prototype from the initial map.
    __ LoadP(result, FieldMemOperand(result, Map::kPrototypeOffset));

    // All done.
    __ bind(&done);
  }
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
      __ LoadP(result, MemOperand(arguments, index * kPointerSize), r0);
    } else {
      Register index = ToRegister(instr->index());
      __ subfic(result, index, Operand(const_length + 1));
      __ ShiftLeftImm(result, result, Operand(kPointerSizeLog2));
      __ LoadPX(result, MemOperand(arguments, result));
    }
  } else if (instr->index()->IsConstantOperand()) {
    Register length = ToRegister(instr->length());
    int const_index = ToInteger32(LConstantOperand::cast(instr->index()));
    int loc = const_index - 1;
    if (loc != 0) {
      __ subi(result, length, Operand(loc));
      __ ShiftLeftImm(result, result, Operand(kPointerSizeLog2));
      __ LoadPX(result, MemOperand(arguments, result));
    } else {
      __ ShiftLeftImm(result, length, Operand(kPointerSizeLog2));
      __ LoadPX(result, MemOperand(arguments, result));
    }
  } else {
    Register length = ToRegister(instr->length());
    Register index = ToRegister(instr->index());
    __ sub(result, length, index);
    __ addi(result, result, Operand(1));
    __ ShiftLeftImm(result, result, Operand(kPointerSizeLog2));
    __ LoadPX(result, MemOperand(arguments, result));
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
  int base_offset = instr->base_offset();

  if (elements_kind == FLOAT32_ELEMENTS || elements_kind == FLOAT64_ELEMENTS) {
    DoubleRegister result = ToDoubleRegister(instr->result());
    if (key_is_constant) {
      __ Add(scratch0(), external_pointer, constant_key << element_size_shift,
             r0);
    } else {
      __ IndexToArrayOffset(r0, key, element_size_shift, key_is_smi);
      __ add(scratch0(), external_pointer, r0);
    }
    if (elements_kind == FLOAT32_ELEMENTS) {
      __ lfs(result, MemOperand(scratch0(), base_offset));
    } else {  // i.e. elements_kind == EXTERNAL_DOUBLE_ELEMENTS
      __ lfd(result, MemOperand(scratch0(), base_offset));
    }
  } else {
    Register result = ToRegister(instr->result());
    MemOperand mem_operand =
        PrepareKeyedOperand(key, external_pointer, key_is_constant, key_is_smi,
                            constant_key, element_size_shift, base_offset);
    switch (elements_kind) {
      case INT8_ELEMENTS:
        if (key_is_constant) {
          __ LoadByte(result, mem_operand, r0);
        } else {
          __ lbzx(result, mem_operand);
        }
        __ extsb(result, result);
        break;
      case UINT8_ELEMENTS:
      case UINT8_CLAMPED_ELEMENTS:
        if (key_is_constant) {
          __ LoadByte(result, mem_operand, r0);
        } else {
          __ lbzx(result, mem_operand);
        }
        break;
      case INT16_ELEMENTS:
        if (key_is_constant) {
          __ LoadHalfWordArith(result, mem_operand, r0);
        } else {
          __ lhax(result, mem_operand);
        }
        break;
      case UINT16_ELEMENTS:
        if (key_is_constant) {
          __ LoadHalfWord(result, mem_operand, r0);
        } else {
          __ lhzx(result, mem_operand);
        }
        break;
      case INT32_ELEMENTS:
        if (key_is_constant) {
          __ LoadWordArith(result, mem_operand, r0);
        } else {
          __ lwax(result, mem_operand);
        }
        break;
      case UINT32_ELEMENTS:
        if (key_is_constant) {
          __ LoadWord(result, mem_operand, r0);
        } else {
          __ lwzx(result, mem_operand);
        }
        if (!instr->hydrogen()->CheckFlag(HInstruction::kUint32)) {
          __ lis(r0, Operand(SIGN_EXT_IMM16(0x8000)));
          __ cmplw(result, r0);
          DeoptimizeIf(ge, instr, Deoptimizer::kNegativeValue);
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
  int constant_key = 0;
  if (key_is_constant) {
    constant_key = ToInteger32(LConstantOperand::cast(instr->key()));
    if (constant_key & 0xF0000000) {
      Abort(kArrayIndexConstantValueTooBig);
    }
  } else {
    key = ToRegister(instr->key());
  }

  int base_offset = instr->base_offset() + constant_key * kDoubleSize;
  if (!key_is_constant) {
    __ IndexToArrayOffset(r0, key, element_size_shift, key_is_smi);
    __ add(scratch, elements, r0);
    elements = scratch;
  }
  if (!is_int16(base_offset)) {
    __ Add(scratch, elements, base_offset, r0);
    base_offset = 0;
    elements = scratch;
  }
  __ lfd(result, MemOperand(elements, base_offset));

  if (instr->hydrogen()->RequiresHoleCheck()) {
    if (is_int16(base_offset + Register::kExponentOffset)) {
      __ lwz(scratch,
             MemOperand(elements, base_offset + Register::kExponentOffset));
    } else {
      __ addi(scratch, elements, Operand(base_offset));
      __ lwz(scratch, MemOperand(scratch, Register::kExponentOffset));
    }
    __ Cmpi(scratch, Operand(kHoleNanUpper32), r0);
    DeoptimizeIf(eq, instr, Deoptimizer::kHole);
  }
}


void LCodeGen::DoLoadKeyedFixedArray(LLoadKeyed* instr) {
  HLoadKeyed* hinstr = instr->hydrogen();
  Register elements = ToRegister(instr->elements());
  Register result = ToRegister(instr->result());
  Register scratch = scratch0();
  Register store_base = scratch;
  int offset = instr->base_offset();

  if (instr->key()->IsConstantOperand()) {
    LConstantOperand* const_operand = LConstantOperand::cast(instr->key());
    offset += ToInteger32(const_operand) * kPointerSize;
    store_base = elements;
  } else {
    Register key = ToRegister(instr->key());
    // Even though the HLoadKeyed instruction forces the input
    // representation for the key to be an integer, the input gets replaced
    // during bound check elimination with the index argument to the bounds
    // check, which can be tagged, so that case must be handled here, too.
    if (hinstr->key()->representation().IsSmi()) {
      __ SmiToPtrArrayOffset(r0, key);
    } else {
      __ ShiftLeftImm(r0, key, Operand(kPointerSizeLog2));
    }
    __ add(scratch, elements, r0);
  }

  bool requires_hole_check = hinstr->RequiresHoleCheck();
  Representation representation = hinstr->representation();

#if V8_TARGET_ARCH_PPC64
  // 64-bit Smi optimization
  if (representation.IsInteger32() &&
      hinstr->elements_kind() == FAST_SMI_ELEMENTS) {
    DCHECK(!requires_hole_check);
    // Read int value directly from upper half of the smi.
    offset = SmiWordOffset(offset);
  }
#endif

  __ LoadRepresentation(result, MemOperand(store_base, offset), representation,
                        r0);

  // Check for the hole value.
  if (requires_hole_check) {
    if (IsFastSmiElementsKind(hinstr->elements_kind())) {
      __ TestIfSmi(result, r0);
      DeoptimizeIf(ne, instr, Deoptimizer::kNotASmi, cr0);
    } else {
      __ LoadRoot(scratch, Heap::kTheHoleValueRootIndex);
      __ cmp(result, scratch);
      DeoptimizeIf(eq, instr, Deoptimizer::kHole);
    }
  } else if (instr->hydrogen()->hole_mode() == CONVERT_HOLE_TO_UNDEFINED) {
    DCHECK(instr->hydrogen()->elements_kind() == FAST_HOLEY_ELEMENTS);
    Label done;
    __ LoadRoot(scratch, Heap::kTheHoleValueRootIndex);
    __ cmp(result, scratch);
    __ bne(&done);
    if (info()->IsStub()) {
      // A stub can safely convert the hole to undefined only if the array
      // protector cell contains (Smi) Isolate::kArrayProtectorValid. Otherwise
      // it needs to bail out.
      __ LoadRoot(result, Heap::kArrayProtectorRootIndex);
      __ LoadP(result, FieldMemOperand(result, Cell::kValueOffset));
      __ CmpSmiLiteral(result, Smi::FromInt(Isolate::kArrayProtectorValid), r0);
      DeoptimizeIf(ne, instr, Deoptimizer::kHole);
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
                                         int base_offset) {
  Register scratch = scratch0();

  if (key_is_constant) {
    return MemOperand(base, (constant_key << element_size_shift) + base_offset);
  }

  bool needs_shift =
      (element_size_shift != (key_is_smi ? kSmiTagSize + kSmiShiftSize : 0));

  if (!(base_offset || needs_shift)) {
    return MemOperand(base, key);
  }

  if (needs_shift) {
    __ IndexToArrayOffset(scratch, key, element_size_shift, key_is_smi);
    key = scratch;
  }

  if (base_offset) {
    __ Add(scratch, key, base_offset, r0);
  }

  return MemOperand(base, scratch);
}


void LCodeGen::DoLoadKeyedGeneric(LLoadKeyedGeneric* instr) {
  DCHECK(ToRegister(instr->context()).is(cp));
  DCHECK(ToRegister(instr->object()).is(LoadDescriptor::ReceiverRegister()));
  DCHECK(ToRegister(instr->key()).is(LoadDescriptor::NameRegister()));

  if (instr->hydrogen()->HasVectorAndSlot()) {
    EmitVectorLoadICRegisters<LLoadKeyedGeneric>(instr);
  }

  Handle<Code> ic = CodeFactory::KeyedLoadICInOptimizedCode(
                        isolate(), instr->hydrogen()->initialization_state())
                        .code();
  CallCode(ic, RelocInfo::CODE_TARGET, instr);
}


void LCodeGen::DoArgumentsElements(LArgumentsElements* instr) {
  Register scratch = scratch0();
  Register result = ToRegister(instr->result());

  if (instr->hydrogen()->from_inlined()) {
    __ subi(result, sp, Operand(2 * kPointerSize));
  } else {
    // Check if the calling frame is an arguments adaptor frame.
    __ LoadP(scratch, MemOperand(fp, StandardFrameConstants::kCallerFPOffset));
    __ LoadP(result,
             MemOperand(scratch, StandardFrameConstants::kContextOffset));
    __ CmpSmiLiteral(result, Smi::FromInt(StackFrame::ARGUMENTS_ADAPTOR), r0);

    // Result is the frame pointer for the frame if not adapted and for the real
    // frame below the adaptor frame if adapted.
    if (CpuFeatures::IsSupported(ISELECT)) {
      __ isel(eq, result, scratch, fp);
    } else {
      Label done, adapted;
      __ beq(&adapted);
      __ mr(result, fp);
      __ b(&done);

      __ bind(&adapted);
      __ mr(result, scratch);
      __ bind(&done);
    }
  }
}


void LCodeGen::DoArgumentsLength(LArgumentsLength* instr) {
  Register elem = ToRegister(instr->elements());
  Register result = ToRegister(instr->result());

  Label done;

  // If no arguments adaptor frame the number of arguments is fixed.
  __ cmp(fp, elem);
  __ mov(result, Operand(scope()->num_parameters()));
  __ beq(&done);

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
    __ lwz(scratch,
           FieldMemOperand(scratch, SharedFunctionInfo::kCompilerHintsOffset));
    __ andi(r0, scratch, Operand((1 << SharedFunctionInfo::kStrictModeBit) |
                                 (1 << SharedFunctionInfo::kNativeBit)));
    __ bne(&result_in_receiver, cr0);
  }

  // Normal function. Replace undefined or null with global receiver.
  __ LoadRoot(scratch, Heap::kNullValueRootIndex);
  __ cmp(receiver, scratch);
  __ beq(&global_object);
  __ LoadRoot(scratch, Heap::kUndefinedValueRootIndex);
  __ cmp(receiver, scratch);
  __ beq(&global_object);

  // Deoptimize if the receiver is not a JS object.
  __ TestIfSmi(receiver, r0);
  DeoptimizeIf(eq, instr, Deoptimizer::kSmi, cr0);
  __ CompareObjectType(receiver, scratch, scratch, FIRST_JS_RECEIVER_TYPE);
  DeoptimizeIf(lt, instr, Deoptimizer::kNotAJavaScriptObject);

  __ b(&result_in_receiver);
  __ bind(&global_object);
  __ LoadP(result, FieldMemOperand(function, JSFunction::kContextOffset));
  __ LoadP(result, ContextMemOperand(result, Context::NATIVE_CONTEXT_INDEX));
  __ LoadP(result, ContextMemOperand(result, Context::GLOBAL_PROXY_INDEX));

  if (result.is(receiver)) {
    __ bind(&result_in_receiver);
  } else {
    Label result_ok;
    __ b(&result_ok);
    __ bind(&result_in_receiver);
    __ mr(result, receiver);
    __ bind(&result_ok);
  }
}


void LCodeGen::DoApplyArguments(LApplyArguments* instr) {
  Register receiver = ToRegister(instr->receiver());
  Register function = ToRegister(instr->function());
  Register length = ToRegister(instr->length());
  Register elements = ToRegister(instr->elements());
  Register scratch = scratch0();
  DCHECK(receiver.is(r3));  // Used for parameter count.
  DCHECK(function.is(r4));  // Required by InvokeFunction.
  DCHECK(ToRegister(instr->result()).is(r3));

  // Copy the arguments to this function possibly from the
  // adaptor frame below it.
  const uint32_t kArgumentsLimit = 1 * KB;
  __ cmpli(length, Operand(kArgumentsLimit));
  DeoptimizeIf(gt, instr, Deoptimizer::kTooManyArguments);

  // Push the receiver and use the register to keep the original
  // number of arguments.
  __ push(receiver);
  __ mr(receiver, length);
  // The arguments are at a one pointer size offset from elements.
  __ addi(elements, elements, Operand(1 * kPointerSize));

  // Loop through the arguments pushing them onto the execution
  // stack.
  Label invoke, loop;
  // length is a small non-negative integer, due to the test above.
  __ cmpi(length, Operand::Zero());
  __ beq(&invoke);
  __ mtctr(length);
  __ bind(&loop);
  __ ShiftLeftImm(r0, length, Operand(kPointerSizeLog2));
  __ LoadPX(scratch, MemOperand(elements, r0));
  __ push(scratch);
  __ addi(length, length, Operand(-1));
  __ bdnz(&loop);

  __ bind(&invoke);
  DCHECK(instr->HasPointerMap());
  LPointerMap* pointers = instr->pointer_map();
  SafepointGenerator safepoint_generator(this, pointers, Safepoint::kLazyDeopt);
  // The number of arguments is stored in receiver which is r3, as expected
  // by InvokeFunction.
  ParameterCount actual(receiver);
  __ InvokeFunction(function, no_reg, actual, CALL_FUNCTION,
                    safepoint_generator);
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
  __ Move(scratch0(), instr->hydrogen()->pairs());
  __ push(scratch0());
  __ LoadSmiLiteral(scratch0(), Smi::FromInt(instr->hydrogen()->flags()));
  __ push(scratch0());
  CallRuntime(Runtime::kDeclareGlobals, instr);
}


void LCodeGen::CallKnownFunction(Handle<JSFunction> function,
                                 int formal_parameter_count, int arity,
                                 LInstruction* instr) {
  bool dont_adapt_arguments =
      formal_parameter_count == SharedFunctionInfo::kDontAdaptArgumentsSentinel;
  bool can_invoke_directly =
      dont_adapt_arguments || formal_parameter_count == arity;

  Register function_reg = r4;

  LPointerMap* pointers = instr->pointer_map();

  if (can_invoke_directly) {
    // Change context.
    __ LoadP(cp, FieldMemOperand(function_reg, JSFunction::kContextOffset));

    // Always initialize new target and number of actual arguments.
    __ LoadRoot(r6, Heap::kUndefinedValueRootIndex);
    __ mov(r3, Operand(arity));

    bool is_self_call = function.is_identical_to(info()->closure());

    // Invoke function.
    if (is_self_call) {
      __ CallSelf();
    } else {
      __ LoadP(ip, FieldMemOperand(function_reg, JSFunction::kCodeEntryOffset));
      __ CallJSEntry(ip);
    }

    // Set up deoptimization.
    RecordSafepointWithLazyDeopt(instr, RECORD_SIMPLE_SAFEPOINT);
  } else {
    SafepointGenerator generator(this, pointers, Safepoint::kLazyDeopt);
    ParameterCount count(arity);
    ParameterCount expected(formal_parameter_count);
    __ InvokeFunction(function_reg, expected, count, CALL_FUNCTION, generator);
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
  __ LoadRoot(ip, Heap::kHeapNumberMapRootIndex);
  __ cmp(scratch, ip);
  DeoptimizeIf(ne, instr, Deoptimizer::kNotAHeapNumber);

  Label done;
  Register exponent = scratch0();
  scratch = no_reg;
  __ lwz(exponent, FieldMemOperand(input, HeapNumber::kExponentOffset));
  // Check the sign of the argument. If the argument is positive, just
  // return it.
  __ cmpwi(exponent, Operand::Zero());
  // Move the input to the result if necessary.
  __ Move(result, input);
  __ bge(&done);

  // Input is negative. Reverse its sign.
  // Preserve the value of all registers.
  {
    PushSafepointRegistersScope scope(this);

    // Registers were saved at the safepoint, so we can use
    // many scratch registers.
    Register tmp1 = input.is(r4) ? r3 : r4;
    Register tmp2 = input.is(r5) ? r3 : r5;
    Register tmp3 = input.is(r6) ? r3 : r6;
    Register tmp4 = input.is(r7) ? r3 : r7;

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
    if (!tmp1.is(r3)) __ mr(tmp1, r3);
    // Restore input_reg after call to runtime.
    __ LoadFromSafepointRegisterSlot(input, input);
    __ lwz(exponent, FieldMemOperand(input, HeapNumber::kExponentOffset));

    __ bind(&allocated);
    // exponent: floating point exponent value.
    // tmp1: allocated heap number.
    STATIC_ASSERT(HeapNumber::kSignMask == 0x80000000u);
    __ clrlwi(exponent, exponent, Operand(1));  // clear sign bit
    __ stw(exponent, FieldMemOperand(tmp1, HeapNumber::kExponentOffset));
    __ lwz(tmp2, FieldMemOperand(input, HeapNumber::kMantissaOffset));
    __ stw(tmp2, FieldMemOperand(tmp1, HeapNumber::kMantissaOffset));

    __ StoreToSafepointRegisterSlot(tmp1, result);
  }

  __ bind(&done);
}


void LCodeGen::EmitMathAbs(LMathAbs* instr) {
  Register input = ToRegister(instr->value());
  Register result = ToRegister(instr->result());
  Label done;
  __ cmpi(input, Operand::Zero());
  __ Move(result, input);
  __ bge(&done);
  __ li(r0, Operand::Zero());  // clear xer
  __ mtxer(r0);
  __ neg(result, result, SetOE, SetRC);
  // Deoptimize on overflow.
  DeoptimizeIf(overflow, instr, Deoptimizer::kOverflow, cr0);
  __ bind(&done);
}


#if V8_TARGET_ARCH_PPC64
void LCodeGen::EmitInteger32MathAbs(LMathAbs* instr) {
  Register input = ToRegister(instr->value());
  Register result = ToRegister(instr->result());
  Label done;
  __ cmpwi(input, Operand::Zero());
  __ Move(result, input);
  __ bge(&done);

  // Deoptimize on overflow.
  __ lis(r0, Operand(SIGN_EXT_IMM16(0x8000)));
  __ cmpw(input, r0);
  DeoptimizeIf(eq, instr, Deoptimizer::kOverflow);

  __ neg(result, result);
  __ bind(&done);
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
    __ fabs(result, input);
#if V8_TARGET_ARCH_PPC64
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
  DeoptimizeIf(al, instr, Deoptimizer::kLostPrecisionOrNaN);

  __ bind(&exact);
  if (instr->hydrogen()->CheckFlag(HValue::kBailoutOnMinusZero)) {
    // Test for -0.
    __ cmpi(result, Operand::Zero());
    __ bne(&done);
    __ cmpwi(input_high, Operand::Zero());
    DeoptimizeIf(lt, instr, Deoptimizer::kMinusZero);
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
  __ fabs(double_scratch1, input);
  __ fcmpu(double_scratch1, dot_five);
  DeoptimizeIf(unordered, instr, Deoptimizer::kLostPrecisionOrNaN);
  // If input is in [-0.5, -0], the result is -0.
  // If input is in [+0, +0.5[, the result is +0.
  // If the input is +0.5, the result is 1.
  __ bgt(&convert);  // Out of [-0.5, +0.5].
  if (instr->hydrogen()->CheckFlag(HValue::kBailoutOnMinusZero)) {
    // [-0.5, -0] (negative) yields minus zero.
    __ TestDoubleSign(input, scratch1);
    DeoptimizeIf(lt, instr, Deoptimizer::kMinusZero);
  }
  __ fcmpu(input, dot_five);
  if (CpuFeatures::IsSupported(ISELECT)) {
    __ li(result, Operand(1));
    __ isel(lt, result, r0, result);
    __ b(&done);
  } else {
    Label return_zero;
    __ bne(&return_zero);
    __ li(result, Operand(1));  // +0.5.
    __ b(&done);
    // Remaining cases: [+0, +0.5[ or [-0.5, +0.5[, depending on
    // flag kBailoutOnMinusZero.
    __ bind(&return_zero);
    __ li(result, Operand::Zero());
    __ b(&done);
  }

  __ bind(&convert);
  __ fadd(input_plus_dot_five, input, dot_five);
  // Reuse dot_five (double_scratch0) as we no longer need this value.
  __ TryInt32Floor(result, input_plus_dot_five, scratch1, scratch2,
                   double_scratch0(), &done, &done);
  DeoptimizeIf(al, instr, Deoptimizer::kLostPrecisionOrNaN);
  __ bind(&done);
}


void LCodeGen::DoMathFround(LMathFround* instr) {
  DoubleRegister input_reg = ToDoubleRegister(instr->value());
  DoubleRegister output_reg = ToDoubleRegister(instr->result());
  __ frsp(output_reg, input_reg);
}


void LCodeGen::DoMathSqrt(LMathSqrt* instr) {
  DoubleRegister input = ToDoubleRegister(instr->value());
  DoubleRegister result = ToDoubleRegister(instr->result());
  __ fsqrt(result, input);
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
  __ fcmpu(input, temp);
  __ bne(&skip);
  __ fneg(result, temp);
  __ b(&done);

  // Add +0 to convert -0 to +0.
  __ bind(&skip);
  __ fadd(result, input, kDoubleRegZero);
  __ fsqrt(result, result);
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
    DCHECK(!r10.is(tagged_exponent));
    __ LoadP(r10, FieldMemOperand(tagged_exponent, HeapObject::kMapOffset));
    __ LoadRoot(ip, Heap::kHeapNumberMapRootIndex);
    __ cmp(r10, ip);
    DeoptimizeIf(ne, instr, Deoptimizer::kNotAHeapNumber);
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


void LCodeGen::DoMathExp(LMathExp* instr) {
  DoubleRegister input = ToDoubleRegister(instr->value());
  DoubleRegister result = ToDoubleRegister(instr->result());
  DoubleRegister double_scratch1 = ToDoubleRegister(instr->double_temp());
  DoubleRegister double_scratch2 = double_scratch0();
  Register temp1 = ToRegister(instr->temp1());
  Register temp2 = ToRegister(instr->temp2());

  MathExpGenerator::EmitMathExp(masm(), input, result, double_scratch1,
                                double_scratch2, temp1, temp2, scratch0());
}


void LCodeGen::DoMathLog(LMathLog* instr) {
  __ PrepareCallCFunction(0, 1, scratch0());
  __ MovToFloatParameter(ToDoubleRegister(instr->value()));
  __ CallCFunction(ExternalReference::math_log_double_function(isolate()), 0,
                   1);
  __ MovFromFloatResult(ToDoubleRegister(instr->result()));
}


void LCodeGen::DoMathClz32(LMathClz32* instr) {
  Register input = ToRegister(instr->value());
  Register result = ToRegister(instr->result());
  __ cntlzw_(result, input);
}


void LCodeGen::DoInvokeFunction(LInvokeFunction* instr) {
  DCHECK(ToRegister(instr->context()).is(cp));
  DCHECK(ToRegister(instr->function()).is(r4));
  DCHECK(instr->HasPointerMap());

  Handle<JSFunction> known_function = instr->hydrogen()->known_function();
  if (known_function.is_null()) {
    LPointerMap* pointers = instr->pointer_map();
    SafepointGenerator generator(this, pointers, Safepoint::kLazyDeopt);
    ParameterCount count(instr->arity());
    __ InvokeFunction(r4, no_reg, count, CALL_FUNCTION, generator);
  } else {
    CallKnownFunction(known_function,
                      instr->hydrogen()->formal_parameter_count(),
                      instr->arity(), instr);
  }
}


void LCodeGen::DoCallWithDescriptor(LCallWithDescriptor* instr) {
  DCHECK(ToRegister(instr->result()).is(r3));

  if (instr->hydrogen()->IsTailCall()) {
    if (NeedsEagerFrame()) __ LeaveFrame(StackFrame::INTERNAL);

    if (instr->target()->IsConstantOperand()) {
      LConstantOperand* target = LConstantOperand::cast(instr->target());
      Handle<Code> code = Handle<Code>::cast(ToHandle(target));
      __ Jump(code, RelocInfo::CODE_TARGET);
    } else {
      DCHECK(instr->target()->IsRegister());
      Register target = ToRegister(instr->target());
      __ addi(ip, target, Operand(Code::kHeaderSize - kHeapObjectTag));
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
      __ addi(ip, target, Operand(Code::kHeaderSize - kHeapObjectTag));
      __ CallJSEntry(ip);
    }
    generator.AfterCall();
  }
}


void LCodeGen::DoCallJSFunction(LCallJSFunction* instr) {
  DCHECK(ToRegister(instr->function()).is(r4));
  DCHECK(ToRegister(instr->result()).is(r3));

  // Change context.
  __ LoadP(cp, FieldMemOperand(r4, JSFunction::kContextOffset));

  // Always initialize new target and number of actual arguments.
  __ LoadRoot(r6, Heap::kUndefinedValueRootIndex);
  __ mov(r3, Operand(instr->arity()));

  bool is_self_call = false;
  if (instr->hydrogen()->function()->IsConstant()) {
    HConstant* fun_const = HConstant::cast(instr->hydrogen()->function());
    Handle<JSFunction> jsfun =
        Handle<JSFunction>::cast(fun_const->handle(isolate()));
    is_self_call = jsfun.is_identical_to(info()->closure());
  }

  if (is_self_call) {
    __ CallSelf();
  } else {
    __ LoadP(ip, FieldMemOperand(r4, JSFunction::kCodeEntryOffset));
    __ CallJSEntry(ip);
  }

  RecordSafepointWithLazyDeopt(instr, RECORD_SIMPLE_SAFEPOINT);
}


void LCodeGen::DoCallFunction(LCallFunction* instr) {
  HCallFunction* hinstr = instr->hydrogen();
  DCHECK(ToRegister(instr->context()).is(cp));
  DCHECK(ToRegister(instr->function()).is(r4));
  DCHECK(ToRegister(instr->result()).is(r3));

  int arity = instr->arity();
  ConvertReceiverMode mode = hinstr->convert_mode();
  if (hinstr->HasVectorAndSlot()) {
    Register slot_register = ToRegister(instr->temp_slot());
    Register vector_register = ToRegister(instr->temp_vector());
    DCHECK(slot_register.is(r6));
    DCHECK(vector_register.is(r5));

    AllowDeferredHandleDereference vector_structure_check;
    Handle<TypeFeedbackVector> vector = hinstr->feedback_vector();
    int index = vector->GetIndex(hinstr->slot());

    __ Move(vector_register, vector);
    __ LoadSmiLiteral(slot_register, Smi::FromInt(index));

    Handle<Code> ic =
        CodeFactory::CallICInOptimizedCode(isolate(), arity, mode).code();
    CallCode(ic, RelocInfo::CODE_TARGET, instr);
  } else {
    __ mov(r3, Operand(arity));
    CallCode(isolate()->builtins()->Call(mode), RelocInfo::CODE_TARGET, instr);
  }
}


void LCodeGen::DoCallNewArray(LCallNewArray* instr) {
  DCHECK(ToRegister(instr->context()).is(cp));
  DCHECK(ToRegister(instr->constructor()).is(r4));
  DCHECK(ToRegister(instr->result()).is(r3));

  __ mov(r3, Operand(instr->arity()));
  if (instr->arity() == 1) {
    // We only need the allocation site for the case we have a length argument.
    // The case may bail out to the runtime, which will determine the correct
    // elements kind with the site.
    __ Move(r5, instr->hydrogen()->site());
  } else {
    __ LoadRoot(r5, Heap::kUndefinedValueRootIndex);
  }
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
      __ LoadP(r8, MemOperand(sp, 0));
      __ cmpi(r8, Operand::Zero());
      __ beq(&packed_case);

      ElementsKind holey_kind = GetHoleyElementsKind(kind);
      ArraySingleArgumentConstructorStub stub(isolate(), holey_kind,
                                              override_mode);
      CallCode(stub.GetCode(), RelocInfo::CODE_TARGET, instr);
      __ b(&done);
      __ bind(&packed_case);
    }

    ArraySingleArgumentConstructorStub stub(isolate(), kind, override_mode);
    CallCode(stub.GetCode(), RelocInfo::CODE_TARGET, instr);
    __ bind(&done);
  } else {
    ArrayNArgumentsConstructorStub stub(isolate(), kind, override_mode);
    CallCode(stub.GetCode(), RelocInfo::CODE_TARGET, instr);
  }
}


void LCodeGen::DoCallRuntime(LCallRuntime* instr) {
  CallRuntime(instr->function(), instr->arity(), instr);
}


void LCodeGen::DoStoreCodeEntry(LStoreCodeEntry* instr) {
  Register function = ToRegister(instr->function());
  Register code_object = ToRegister(instr->code_object());
  __ addi(code_object, code_object,
          Operand(Code::kHeaderSize - kHeapObjectTag));
  __ StoreP(code_object,
            FieldMemOperand(function, JSFunction::kCodeEntryOffset), r0);
}


void LCodeGen::DoInnerAllocatedObject(LInnerAllocatedObject* instr) {
  Register result = ToRegister(instr->result());
  Register base = ToRegister(instr->base_object());
  if (instr->offset()->IsConstantOperand()) {
    LConstantOperand* offset = LConstantOperand::cast(instr->offset());
    __ Add(result, base, ToInteger32(offset), r0);
  } else {
    Register offset = ToRegister(instr->offset());
    __ add(result, base, offset);
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

#if V8_TARGET_ARCH_PPC64
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
    __ stfd(value, FieldMemOperand(object, offset));
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
#if V8_TARGET_ARCH_PPC64
  if (FLAG_unbox_double_fields && representation.IsDouble()) {
    DCHECK(access.IsInobject());
    DoubleRegister value = ToDoubleRegister(instr->value());
    __ stfd(value, FieldMemOperand(object, offset));
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
#if V8_TARGET_ARCH_PPC64
  }
#endif

  if (hinstr->NeedsWriteBarrier()) {
    __ RecordWriteField(record_dest, offset, record_value, record_scratch,
                        GetLinkRegisterState(), kSaveFPRegs,
                        EMIT_REMEMBERED_SET, hinstr->SmiCheckForWriteBarrier(),
                        hinstr->PointersToHereCheckForValue());
  }
}


void LCodeGen::DoStoreNamedGeneric(LStoreNamedGeneric* instr) {
  DCHECK(ToRegister(instr->context()).is(cp));
  DCHECK(ToRegister(instr->object()).is(StoreDescriptor::ReceiverRegister()));
  DCHECK(ToRegister(instr->value()).is(StoreDescriptor::ValueRegister()));

  if (instr->hydrogen()->HasVectorAndSlot()) {
    EmitVectorStoreICRegisters<LStoreNamedGeneric>(instr);
  }

  __ mov(StoreDescriptor::NameRegister(), Operand(instr->name()));
  Handle<Code> ic = CodeFactory::StoreICInOptimizedCode(
                        isolate(), instr->language_mode(),
                        instr->hydrogen()->initialization_state()).code();
  CallCode(ic, RelocInfo::CODE_TARGET, instr);
}


void LCodeGen::DoBoundsCheck(LBoundsCheck* instr) {
  Representation representation = instr->hydrogen()->length()->representation();
  DCHECK(representation.Equals(instr->hydrogen()->index()->representation()));
  DCHECK(representation.IsSmiOrInteger32());

  Condition cc = instr->hydrogen()->allow_equality() ? lt : le;
  if (instr->length()->IsConstantOperand()) {
    int32_t length = ToInteger32(LConstantOperand::cast(instr->length()));
    Register index = ToRegister(instr->index());
    if (representation.IsSmi()) {
      __ Cmpli(index, Operand(Smi::FromInt(length)), r0);
    } else {
      __ Cmplwi(index, Operand(length), r0);
    }
    cc = CommuteCondition(cc);
  } else if (instr->index()->IsConstantOperand()) {
    int32_t index = ToInteger32(LConstantOperand::cast(instr->index()));
    Register length = ToRegister(instr->length());
    if (representation.IsSmi()) {
      __ Cmpli(length, Operand(Smi::FromInt(index)), r0);
    } else {
      __ Cmplwi(length, Operand(index), r0);
    }
  } else {
    Register index = ToRegister(instr->index());
    Register length = ToRegister(instr->length());
    if (representation.IsSmi()) {
      __ cmpl(length, index);
    } else {
      __ cmplw(length, index);
    }
  }
  if (FLAG_debug_code && instr->hydrogen()->skip_check()) {
    Label done;
    __ b(NegateCondition(cc), &done);
    __ stop("eliminated bounds check failed");
    __ bind(&done);
  } else {
    DeoptimizeIf(cc, instr, Deoptimizer::kOutOfBounds);
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
  int base_offset = instr->base_offset();

  if (elements_kind == FLOAT32_ELEMENTS || elements_kind == FLOAT64_ELEMENTS) {
    Register address = scratch0();
    DoubleRegister value(ToDoubleRegister(instr->value()));
    if (key_is_constant) {
      if (constant_key != 0) {
        __ Add(address, external_pointer, constant_key << element_size_shift,
               r0);
      } else {
        address = external_pointer;
      }
    } else {
      __ IndexToArrayOffset(r0, key, element_size_shift, key_is_smi);
      __ add(address, external_pointer, r0);
    }
    if (elements_kind == FLOAT32_ELEMENTS) {
      __ frsp(double_scratch0(), value);
      __ stfs(double_scratch0(), MemOperand(address, base_offset));
    } else {  // Storing doubles, not floats.
      __ stfd(value, MemOperand(address, base_offset));
    }
  } else {
    Register value(ToRegister(instr->value()));
    MemOperand mem_operand =
        PrepareKeyedOperand(key, external_pointer, key_is_constant, key_is_smi,
                            constant_key, element_size_shift, base_offset);
    switch (elements_kind) {
      case UINT8_ELEMENTS:
      case UINT8_CLAMPED_ELEMENTS:
      case INT8_ELEMENTS:
        if (key_is_constant) {
          __ StoreByte(value, mem_operand, r0);
        } else {
          __ stbx(value, mem_operand);
        }
        break;
      case INT16_ELEMENTS:
      case UINT16_ELEMENTS:
        if (key_is_constant) {
          __ StoreHalfWord(value, mem_operand, r0);
        } else {
          __ sthx(value, mem_operand);
        }
        break;
      case INT32_ELEMENTS:
      case UINT32_ELEMENTS:
        if (key_is_constant) {
          __ StoreWord(value, mem_operand, r0);
        } else {
          __ stwx(value, mem_operand);
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
  int base_offset = instr->base_offset() + constant_key * kDoubleSize;
  if (!key_is_constant) {
    __ IndexToArrayOffset(scratch, key, element_size_shift, key_is_smi);
    __ add(scratch, elements, scratch);
    elements = scratch;
  }
  if (!is_int16(base_offset)) {
    __ Add(scratch, elements, base_offset, r0);
    base_offset = 0;
    elements = scratch;
  }

  if (instr->NeedsCanonicalization()) {
    // Turn potential sNaN value into qNaN.
    __ CanonicalizeNaN(double_scratch, value);
    __ stfd(double_scratch, MemOperand(elements, base_offset));
  } else {
    __ stfd(value, MemOperand(elements, base_offset));
  }
}


void LCodeGen::DoStoreKeyedFixedArray(LStoreKeyed* instr) {
  HStoreKeyed* hinstr = instr->hydrogen();
  Register value = ToRegister(instr->value());
  Register elements = ToRegister(instr->elements());
  Register key = instr->key()->IsRegister() ? ToRegister(instr->key()) : no_reg;
  Register scratch = scratch0();
  Register store_base = scratch;
  int offset = instr->base_offset();

  // Do the store.
  if (instr->key()->IsConstantOperand()) {
    DCHECK(!hinstr->NeedsWriteBarrier());
    LConstantOperand* const_operand = LConstantOperand::cast(instr->key());
    offset += ToInteger32(const_operand) * kPointerSize;
    store_base = elements;
  } else {
    // Even though the HLoadKeyed instruction forces the input
    // representation for the key to be an integer, the input gets replaced
    // during bound check elimination with the index argument to the bounds
    // check, which can be tagged, so that case must be handled here, too.
    if (hinstr->key()->representation().IsSmi()) {
      __ SmiToPtrArrayOffset(scratch, key);
    } else {
      __ ShiftLeftImm(scratch, key, Operand(kPointerSizeLog2));
    }
    __ add(scratch, elements, scratch);
  }

  Representation representation = hinstr->value()->representation();

#if V8_TARGET_ARCH_PPC64
  // 64-bit Smi optimization
  if (representation.IsInteger32()) {
    DCHECK(hinstr->store_mode() == STORE_TO_INITIALIZED_ENTRY);
    DCHECK(hinstr->elements_kind() == FAST_SMI_ELEMENTS);
    // Store int value directly to upper half of the smi.
    offset = SmiWordOffset(offset);
  }
#endif

  __ StoreRepresentation(value, MemOperand(store_base, offset), representation,
                         r0);

  if (hinstr->NeedsWriteBarrier()) {
    SmiCheck check_needed = hinstr->value()->type().IsHeapObject()
                                ? OMIT_SMI_CHECK
                                : INLINE_SMI_CHECK;
    // Compute address of modified element and store it into key register.
    __ Add(key, store_base, offset, r0);
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


void LCodeGen::DoStoreKeyedGeneric(LStoreKeyedGeneric* instr) {
  DCHECK(ToRegister(instr->context()).is(cp));
  DCHECK(ToRegister(instr->object()).is(StoreDescriptor::ReceiverRegister()));
  DCHECK(ToRegister(instr->key()).is(StoreDescriptor::NameRegister()));
  DCHECK(ToRegister(instr->value()).is(StoreDescriptor::ValueRegister()));

  if (instr->hydrogen()->HasVectorAndSlot()) {
    EmitVectorStoreICRegisters<LStoreKeyedGeneric>(instr);
  }

  Handle<Code> ic = CodeFactory::KeyedStoreICInOptimizedCode(
                        isolate(), instr->language_mode(),
                        instr->hydrogen()->initialization_state()).code();
  CallCode(ic, RelocInfo::CODE_TARGET, instr);
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

  Register result = r3;
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
    __ Cmpwi(ToRegister(current_capacity), Operand(constant_key), r0);
    __ ble(deferred->entry());
  } else if (current_capacity->IsConstantOperand()) {
    int32_t constant_capacity =
        ToInteger32(LConstantOperand::cast(current_capacity));
    __ Cmpwi(ToRegister(key), Operand(constant_capacity), r0);
    __ bge(deferred->entry());
  } else {
    __ cmpw(ToRegister(key), ToRegister(current_capacity));
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
  Register result = r3;
  __ li(result, Operand::Zero());

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
      __ LoadSmiLiteral(r6, ToSmi(LConstantOperand::cast(key)));
    } else {
      __ SmiTag(r6, ToRegister(key));
    }

    GrowArrayElementsStub stub(isolate(), instr->hydrogen()->is_js_array(),
                               instr->hydrogen()->kind());
    __ CallStub(&stub);
    RecordSafepointWithLazyDeopt(
        instr, RECORD_SAFEPOINT_WITH_REGISTERS_AND_NO_ARGUMENTS);
    __ StoreToSafepointRegisterSlot(result, result);
  }

  // Deopt on smi, which means the elements array changed to dictionary mode.
  __ TestIfSmi(result, r0);
  DeoptimizeIf(eq, instr, Deoptimizer::kSmi, cr0);
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
  __ Cmpi(scratch, Operand(from_map), r0);
  __ bne(&not_applicable);

  if (IsSimpleMapChangeTransition(from_kind, to_kind)) {
    Register new_map_reg = ToRegister(instr->new_map_temp());
    __ mov(new_map_reg, Operand(to_map));
    __ StoreP(new_map_reg, FieldMemOperand(object_reg, HeapObject::kMapOffset),
              r0);
    // Write barrier.
    __ RecordWriteForMap(object_reg, new_map_reg, scratch,
                         GetLinkRegisterState(), kDontSaveFPRegs);
  } else {
    DCHECK(ToRegister(instr->context()).is(cp));
    DCHECK(object_reg.is(r3));
    PushSafepointRegistersScope scope(this);
    __ Move(r4, to_map);
    bool is_js_array = from_map->instance_type() == JS_ARRAY_TYPE;
    TransitionElementsKindStub stub(isolate(), from_kind, to_kind, is_js_array);
    __ CallStub(&stub);
    RecordSafepointWithRegisters(instr->pointer_map(), 0,
                                 Safepoint::kLazyDeopt);
  }
  __ bind(&not_applicable);
}


void LCodeGen::DoTrapAllocationMemento(LTrapAllocationMemento* instr) {
  Register object = ToRegister(instr->object());
  Register temp = ToRegister(instr->temp());
  Label no_memento_found;
  __ TestJSArrayForAllocationMemento(object, temp, &no_memento_found);
  DeoptimizeIf(eq, instr, Deoptimizer::kMementoFound);
  __ bind(&no_memento_found);
}


void LCodeGen::DoStringAdd(LStringAdd* instr) {
  DCHECK(ToRegister(instr->context()).is(cp));
  DCHECK(ToRegister(instr->left()).is(r4));
  DCHECK(ToRegister(instr->right()).is(r3));
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
  __ li(result, Operand::Zero());

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
  __ AssertSmi(r3);
  __ SmiUntag(r3);
  __ StoreToSafepointRegisterSlot(r3, result);
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

  __ cmpli(char_code, Operand(String::kMaxOneByteCharCode));
  __ bgt(deferred->entry());
  __ LoadRoot(result, Heap::kSingleCharacterStringCacheRootIndex);
  __ ShiftLeftImm(r0, char_code, Operand(kPointerSizeLog2));
  __ add(result, result, r0);
  __ LoadP(result, FieldMemOperand(result, FixedArray::kHeaderSize));
  __ LoadRoot(ip, Heap::kUndefinedValueRootIndex);
  __ cmp(result, ip);
  __ beq(deferred->entry());
  __ bind(deferred->exit());
}


void LCodeGen::DoDeferredStringCharFromCode(LStringCharFromCode* instr) {
  Register char_code = ToRegister(instr->char_code());
  Register result = ToRegister(instr->result());

  // TODO(3095996): Get rid of this. For now, we need to make the
  // result register contain a valid pointer because it is already
  // contained in the register pointer map.
  __ li(result, Operand::Zero());

  PushSafepointRegistersScope scope(this);
  __ SmiTag(char_code);
  __ push(char_code);
  CallRuntimeFromDeferred(Runtime::kStringCharFromCode, 1, instr,
                          instr->context());
  __ StoreToSafepointRegisterSlot(r3, result);
}


void LCodeGen::DoInteger32ToDouble(LInteger32ToDouble* instr) {
  LOperand* input = instr->value();
  DCHECK(input->IsRegister() || input->IsStackSlot());
  LOperand* output = instr->result();
  DCHECK(output->IsDoubleRegister());
  if (input->IsStackSlot()) {
    Register scratch = scratch0();
    __ LoadP(scratch, ToMemOperand(input));
    __ ConvertIntToDouble(scratch, ToDoubleRegister(output));
  } else {
    __ ConvertIntToDouble(ToRegister(input), ToDoubleRegister(output));
  }
}


void LCodeGen::DoUint32ToDouble(LUint32ToDouble* instr) {
  LOperand* input = instr->value();
  LOperand* output = instr->result();
  __ ConvertUnsignedIntToDouble(ToRegister(input), ToDoubleRegister(output));
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
#if V8_TARGET_ARCH_PPC64
  __ SmiTag(dst, src);
#else
  __ SmiTagCheckOverflow(dst, src, r0);
  __ BranchOnOverflow(deferred->entry());
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
  __ Cmpli(input, Operand(Smi::kMaxValue), r0);
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
      __ xoris(src, src, Operand(HeapNumber::kSignMask >> 16));
    }
    __ ConvertIntToDouble(src, dbl_scratch);
  } else {
    __ ConvertUnsignedIntToDouble(src, dbl_scratch);
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
    __ li(dst, Operand::Zero());

    // Preserve the value of all registers.
    PushSafepointRegistersScope scope(this);

    // NumberTagI and NumberTagD use the context from the frame, rather than
    // the environment's HContext or HInlinedContext value.
    // They only call Runtime::kAllocateHeapNumber.
    // The corresponding HChange instructions are added in a phase that does
    // not have easy access to the local context.
    __ LoadP(cp, MemOperand(fp, StandardFrameConstants::kContextOffset));
    __ CallRuntimeSaveDoubles(Runtime::kAllocateHeapNumber);
    RecordSafepointWithRegisters(instr->pointer_map(), 0,
                                 Safepoint::kNoLazyDeopt);
    __ StoreToSafepointRegisterSlot(r3, dst);
  }

  // Done. Put the value in dbl_scratch into the value of the allocated heap
  // number.
  __ bind(&done);
  __ stfd(dbl_scratch, FieldMemOperand(dst, HeapNumber::kValueOffset));
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
  __ stfd(input_reg, FieldMemOperand(reg, HeapNumber::kValueOffset));
}


void LCodeGen::DoDeferredNumberTagD(LNumberTagD* instr) {
  // TODO(3095996): Get rid of this. For now, we need to make the
  // result register contain a valid pointer because it is already
  // contained in the register pointer map.
  Register reg = ToRegister(instr->result());
  __ li(reg, Operand::Zero());

  PushSafepointRegistersScope scope(this);
  // NumberTagI and NumberTagD use the context from the frame, rather than
  // the environment's HContext or HInlinedContext value.
  // They only call Runtime::kAllocateHeapNumber.
  // The corresponding HChange instructions are added in a phase that does
  // not have easy access to the local context.
  __ LoadP(cp, MemOperand(fp, StandardFrameConstants::kContextOffset));
  __ CallRuntimeSaveDoubles(Runtime::kAllocateHeapNumber);
  RecordSafepointWithRegisters(instr->pointer_map(), 0,
                               Safepoint::kNoLazyDeopt);
  __ StoreToSafepointRegisterSlot(r3, reg);
}


void LCodeGen::DoSmiTag(LSmiTag* instr) {
  HChange* hchange = instr->hydrogen();
  Register input = ToRegister(instr->value());
  Register output = ToRegister(instr->result());
  if (hchange->CheckFlag(HValue::kCanOverflow) &&
      hchange->value()->CheckFlag(HValue::kUint32)) {
    __ TestUnsignedSmiCandidate(input, r0);
    DeoptimizeIf(ne, instr, Deoptimizer::kOverflow, cr0);
  }
#if !V8_TARGET_ARCH_PPC64
  if (hchange->CheckFlag(HValue::kCanOverflow) &&
      !hchange->value()->CheckFlag(HValue::kUint32)) {
    __ SmiTagCheckOverflow(output, input, r0);
    DeoptimizeIf(lt, instr, Deoptimizer::kOverflow, cr0);
  } else {
#endif
    __ SmiTag(output, input);
#if !V8_TARGET_ARCH_PPC64
  }
#endif
}


void LCodeGen::DoSmiUntag(LSmiUntag* instr) {
  Register scratch = scratch0();
  Register input = ToRegister(instr->value());
  Register result = ToRegister(instr->result());
  if (instr->needs_check()) {
    // If the input is a HeapObject, value of scratch won't be zero.
    __ andi(scratch, input, Operand(kHeapObjectTag));
    __ SmiUntag(result, input);
    DeoptimizeIf(ne, instr, Deoptimizer::kNotASmi, cr0);
  } else {
    __ SmiUntag(result, input);
  }
}


void LCodeGen::EmitNumberUntagD(LNumberUntagD* instr, Register input_reg,
                                DoubleRegister result_reg,
                                NumberUntagDMode mode) {
  bool can_convert_undefined_to_nan =
      instr->hydrogen()->can_convert_undefined_to_nan();
  bool deoptimize_on_minus_zero = instr->hydrogen()->deoptimize_on_minus_zero();

  Register scratch = scratch0();
  DCHECK(!result_reg.is(double_scratch0()));

  Label convert, load_smi, done;

  if (mode == NUMBER_CANDIDATE_IS_ANY_TAGGED) {
    // Smi check.
    __ UntagAndJumpIfSmi(scratch, input_reg, &load_smi);

    // Heap number map check.
    __ LoadP(scratch, FieldMemOperand(input_reg, HeapObject::kMapOffset));
    __ LoadRoot(ip, Heap::kHeapNumberMapRootIndex);
    __ cmp(scratch, ip);
    if (can_convert_undefined_to_nan) {
      __ bne(&convert);
    } else {
      DeoptimizeIf(ne, instr, Deoptimizer::kNotAHeapNumber);
    }
    // load heap number
    __ lfd(result_reg, FieldMemOperand(input_reg, HeapNumber::kValueOffset));
    if (deoptimize_on_minus_zero) {
      __ TestDoubleIsMinusZero(result_reg, scratch, ip);
      DeoptimizeIf(eq, instr, Deoptimizer::kMinusZero);
    }
    __ b(&done);
    if (can_convert_undefined_to_nan) {
      __ bind(&convert);
      // Convert undefined (and hole) to NaN.
      __ LoadRoot(ip, Heap::kUndefinedValueRootIndex);
      __ cmp(input_reg, ip);
      DeoptimizeIf(ne, instr, Deoptimizer::kNotAHeapNumberUndefined);
      __ LoadRoot(scratch, Heap::kNanValueRootIndex);
      __ lfd(result_reg, FieldMemOperand(scratch, HeapNumber::kValueOffset));
      __ b(&done);
    }
  } else {
    __ SmiUntag(scratch, input_reg);
    DCHECK(mode == NUMBER_CANDIDATE_IS_SMI);
  }
  // Smi to double register conversion
  __ bind(&load_smi);
  // scratch: untagged value of input_reg
  __ ConvertIntToDouble(scratch, result_reg);
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
  __ LoadRoot(ip, Heap::kHeapNumberMapRootIndex);
  __ cmp(scratch1, ip);

  if (instr->truncating()) {
    // Performs a truncating conversion of a floating point number as used by
    // the JS bitwise operations.
    Label no_heap_number, check_bools, check_false;
    __ bne(&no_heap_number);
    __ mr(scratch2, input_reg);
    __ TruncateHeapNumberToI(input_reg, scratch2);
    __ b(&done);

    // Check for Oddballs. Undefined/False is converted to zero and True to one
    // for truncating conversions.
    __ bind(&no_heap_number);
    __ LoadRoot(ip, Heap::kUndefinedValueRootIndex);
    __ cmp(input_reg, ip);
    __ bne(&check_bools);
    __ li(input_reg, Operand::Zero());
    __ b(&done);

    __ bind(&check_bools);
    __ LoadRoot(ip, Heap::kTrueValueRootIndex);
    __ cmp(input_reg, ip);
    __ bne(&check_false);
    __ li(input_reg, Operand(1));
    __ b(&done);

    __ bind(&check_false);
    __ LoadRoot(ip, Heap::kFalseValueRootIndex);
    __ cmp(input_reg, ip);
    DeoptimizeIf(ne, instr, Deoptimizer::kNotAHeapNumberUndefinedBoolean);
    __ li(input_reg, Operand::Zero());
  } else {
    DeoptimizeIf(ne, instr, Deoptimizer::kNotAHeapNumber);

    __ lfd(double_scratch2,
           FieldMemOperand(input_reg, HeapNumber::kValueOffset));
    if (instr->hydrogen()->CheckFlag(HValue::kBailoutOnMinusZero)) {
      // preserve heap number pointer in scratch2 for minus zero check below
      __ mr(scratch2, input_reg);
    }
    __ TryDoubleToInt32Exact(input_reg, double_scratch2, scratch1,
                             double_scratch);
    DeoptimizeIf(ne, instr, Deoptimizer::kLostPrecisionOrNaN);

    if (instr->hydrogen()->CheckFlag(HValue::kBailoutOnMinusZero)) {
      __ cmpi(input_reg, Operand::Zero());
      __ bne(&done);
      __ TestHeapNumberSign(scratch2, scratch1);
      DeoptimizeIf(lt, instr, Deoptimizer::kMinusZero);
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
    DeoptimizeIf(ne, instr, Deoptimizer::kLostPrecisionOrNaN);
    if (instr->hydrogen()->CheckFlag(HValue::kBailoutOnMinusZero)) {
      Label done;
      __ cmpi(result_reg, Operand::Zero());
      __ bne(&done);
      __ TestDoubleSign(double_input, scratch1);
      DeoptimizeIf(lt, instr, Deoptimizer::kMinusZero);
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
    DeoptimizeIf(ne, instr, Deoptimizer::kLostPrecisionOrNaN);
    if (instr->hydrogen()->CheckFlag(HValue::kBailoutOnMinusZero)) {
      Label done;
      __ cmpi(result_reg, Operand::Zero());
      __ bne(&done);
      __ TestDoubleSign(double_input, scratch1);
      DeoptimizeIf(lt, instr, Deoptimizer::kMinusZero);
      __ bind(&done);
    }
  }
#if V8_TARGET_ARCH_PPC64
  __ SmiTag(result_reg);
#else
  __ SmiTagCheckOverflow(result_reg, r0);
  DeoptimizeIf(lt, instr, Deoptimizer::kOverflow, cr0);
#endif
}


void LCodeGen::DoCheckSmi(LCheckSmi* instr) {
  LOperand* input = instr->value();
  __ TestIfSmi(ToRegister(input), r0);
  DeoptimizeIf(ne, instr, Deoptimizer::kNotASmi, cr0);
}


void LCodeGen::DoCheckNonSmi(LCheckNonSmi* instr) {
  if (!instr->hydrogen()->value()->type().IsHeapObject()) {
    LOperand* input = instr->value();
    __ TestIfSmi(ToRegister(input), r0);
    DeoptimizeIf(eq, instr, Deoptimizer::kSmi, cr0);
  }
}


void LCodeGen::DoCheckArrayBufferNotNeutered(
    LCheckArrayBufferNotNeutered* instr) {
  Register view = ToRegister(instr->view());
  Register scratch = scratch0();

  __ LoadP(scratch, FieldMemOperand(view, JSArrayBufferView::kBufferOffset));
  __ lwz(scratch, FieldMemOperand(scratch, JSArrayBuffer::kBitFieldOffset));
  __ andi(r0, scratch, Operand(1 << JSArrayBuffer::WasNeutered::kShift));
  DeoptimizeIf(ne, instr, Deoptimizer::kOutOfBounds, cr0);
}


void LCodeGen::DoCheckInstanceType(LCheckInstanceType* instr) {
  Register input = ToRegister(instr->value());
  Register scratch = scratch0();

  __ LoadP(scratch, FieldMemOperand(input, HeapObject::kMapOffset));
  __ lbz(scratch, FieldMemOperand(scratch, Map::kInstanceTypeOffset));

  if (instr->hydrogen()->is_interval_check()) {
    InstanceType first;
    InstanceType last;
    instr->hydrogen()->GetCheckInterval(&first, &last);

    __ cmpli(scratch, Operand(first));

    // If there is only one type in the interval check for equality.
    if (first == last) {
      DeoptimizeIf(ne, instr, Deoptimizer::kWrongInstanceType);
    } else {
      DeoptimizeIf(lt, instr, Deoptimizer::kWrongInstanceType);
      // Omit check for the last type.
      if (last != LAST_TYPE) {
        __ cmpli(scratch, Operand(last));
        DeoptimizeIf(gt, instr, Deoptimizer::kWrongInstanceType);
      }
    }
  } else {
    uint8_t mask;
    uint8_t tag;
    instr->hydrogen()->GetCheckMaskAndTag(&mask, &tag);

    if (base::bits::IsPowerOfTwo32(mask)) {
      DCHECK(tag == 0 || base::bits::IsPowerOfTwo32(tag));
      __ andi(r0, scratch, Operand(mask));
      DeoptimizeIf(tag == 0 ? ne : eq, instr, Deoptimizer::kWrongInstanceType,
                   cr0);
    } else {
      __ andi(scratch, scratch, Operand(mask));
      __ cmpi(scratch, Operand(tag));
      DeoptimizeIf(ne, instr, Deoptimizer::kWrongInstanceType);
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
    __ LoadP(ip, FieldMemOperand(ip, Cell::kValueOffset));
    __ cmp(reg, ip);
  } else {
    __ Cmpi(reg, Operand(object), r0);
  }
  DeoptimizeIf(ne, instr, Deoptimizer::kValueMismatch);
}


void LCodeGen::DoDeferredInstanceMigration(LCheckMaps* instr, Register object) {
  Register temp = ToRegister(instr->temp());
  {
    PushSafepointRegistersScope scope(this);
    __ push(object);
    __ li(cp, Operand::Zero());
    __ CallRuntimeSaveDoubles(Runtime::kTryMigrateInstance);
    RecordSafepointWithRegisters(instr->pointer_map(), 1,
                                 Safepoint::kNoLazyDeopt);
    __ StoreToSafepointRegisterSlot(r3, temp);
  }
  __ TestIfSmi(temp, r0);
  DeoptimizeIf(eq, instr, Deoptimizer::kInstanceMigrationFailed, cr0);
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

  Register object = ToRegister(instr->value());
  Register map_reg = ToRegister(instr->temp());

  __ LoadP(map_reg, FieldMemOperand(object, HeapObject::kMapOffset));

  DeferredCheckMaps* deferred = NULL;
  if (instr->hydrogen()->HasMigrationTarget()) {
    deferred = new (zone()) DeferredCheckMaps(this, instr, object);
    __ bind(deferred->check_maps());
  }

  const UniqueSet<Map>* maps = instr->hydrogen()->maps();
  Label success;
  for (int i = 0; i < maps->size() - 1; i++) {
    Handle<Map> map = maps->at(i).handle();
    __ CompareMap(map_reg, map, &success);
    __ beq(&success);
  }

  Handle<Map> map = maps->at(maps->size() - 1).handle();
  __ CompareMap(map_reg, map, &success);
  if (instr->hydrogen()->HasMigrationTarget()) {
    __ bne(deferred->entry());
  } else {
    DeoptimizeIf(ne, instr, Deoptimizer::kWrongMap);
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
  __ Cmpi(scratch, Operand(factory()->heap_number_map()), r0);
  __ beq(&heap_number);

  // Check for undefined. Undefined is converted to zero for clamping
  // conversions.
  __ Cmpi(input_reg, Operand(factory()->undefined_value()), r0);
  DeoptimizeIf(ne, instr, Deoptimizer::kNotAHeapNumberUndefined);
  __ li(result_reg, Operand::Zero());
  __ b(&done);

  // Heap number
  __ bind(&heap_number);
  __ lfd(temp_reg, FieldMemOperand(input_reg, HeapNumber::kValueOffset));
  __ ClampDoubleToUint8(result_reg, temp_reg, double_scratch0());
  __ b(&done);

  // smi
  __ bind(&is_smi);
  __ ClampUint8(result_reg, result_reg);

  __ bind(&done);
}


void LCodeGen::DoDoubleBits(LDoubleBits* instr) {
  DoubleRegister value_reg = ToDoubleRegister(instr->value());
  Register result_reg = ToRegister(instr->result());

  if (instr->hydrogen()->bits() == HDoubleBits::HIGH) {
    __ MovDoubleHighToInt(result_reg, value_reg);
  } else {
    __ MovDoubleLowToInt(result_reg, value_reg);
  }
}


void LCodeGen::DoConstructDouble(LConstructDouble* instr) {
  Register hi_reg = ToRegister(instr->hi());
  Register lo_reg = ToRegister(instr->lo());
  DoubleRegister result_reg = ToDoubleRegister(instr->result());
#if V8_TARGET_ARCH_PPC64
  __ MovInt64ComponentsToDouble(result_reg, hi_reg, lo_reg, r0);
#else
  __ MovInt64ToDouble(result_reg, hi_reg, lo_reg);
#endif
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
  AllocationFlags flags = TAG_OBJECT;
  if (instr->hydrogen()->MustAllocateDoubleAligned()) {
    flags = static_cast<AllocationFlags>(flags | DOUBLE_ALIGNMENT);
  }
  if (instr->hydrogen()->IsOldSpaceAllocation()) {
    DCHECK(!instr->hydrogen()->IsNewSpaceAllocation());
    flags = static_cast<AllocationFlags>(flags | PRETENURE);
  }

  if (instr->size()->IsConstantOperand()) {
    int32_t size = ToInteger32(LConstantOperand::cast(instr->size()));
    CHECK(size <= Page::kMaxRegularHeapObjectSize);
    __ Allocate(size, result, scratch, scratch2, deferred->entry(), flags);
  } else {
    Register size = ToRegister(instr->size());
    __ Allocate(size, result, scratch, scratch2, deferred->entry(), flags);
  }

  __ bind(deferred->exit());

  if (instr->hydrogen()->MustPrefillWithFiller()) {
    if (instr->size()->IsConstantOperand()) {
      int32_t size = ToInteger32(LConstantOperand::cast(instr->size()));
      __ LoadIntLiteral(scratch, size - kHeapObjectTag);
    } else {
      __ subi(scratch, ToRegister(instr->size()), Operand(kHeapObjectTag));
    }
    __ mov(scratch2, Operand(isolate()->factory()->one_pointer_filler_map()));
    Label loop;
    __ bind(&loop);
    __ subi(scratch, scratch, Operand(kPointerSize));
    __ StorePX(scratch2, MemOperand(result, scratch));
    __ cmpi(scratch, Operand::Zero());
    __ bge(&loop);
  }
}


void LCodeGen::DoDeferredAllocate(LAllocate* instr) {
  Register result = ToRegister(instr->result());

  // TODO(3095996): Get rid of this. For now, we need to make the
  // result register contain a valid pointer because it is already
  // contained in the register pointer map.
  __ LoadSmiLiteral(result, Smi::FromInt(0));

  PushSafepointRegistersScope scope(this);
  if (instr->size()->IsRegister()) {
    Register size = ToRegister(instr->size());
    DCHECK(!size.is(result));
    __ SmiTag(size);
    __ push(size);
  } else {
    int32_t size = ToInteger32(LConstantOperand::cast(instr->size()));
#if !V8_TARGET_ARCH_PPC64
    if (size >= 0 && size <= Smi::kMaxValue) {
#endif
      __ Push(Smi::FromInt(size));
#if !V8_TARGET_ARCH_PPC64
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
  __ StoreToSafepointRegisterSlot(r3, result);
}


void LCodeGen::DoToFastProperties(LToFastProperties* instr) {
  DCHECK(ToRegister(instr->value()).is(r3));
  __ push(r3);
  CallRuntime(Runtime::kToFastProperties, 1, instr);
}


void LCodeGen::DoTypeof(LTypeof* instr) {
  DCHECK(ToRegister(instr->value()).is(r6));
  DCHECK(ToRegister(instr->result()).is(r3));
  Label end, do_call;
  Register value_register = ToRegister(instr->value());
  __ JumpIfNotSmi(value_register, &do_call);
  __ mov(r3, Operand(isolate()->factory()->number_string()));
  __ b(&end);
  __ bind(&do_call);
  TypeofStub stub(isolate());
  CallCode(stub.GetCode(), RelocInfo::CODE_TARGET, instr);
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
    __ lbz(scratch, FieldMemOperand(scratch, Map::kBitFieldOffset));
    __ ExtractBit(r0, scratch, Map::kIsUndetectable);
    __ cmpi(r0, Operand::Zero());
    final_branch_condition = ne;

  } else if (String::Equals(type_name, factory->function_string())) {
    __ JumpIfSmi(input, false_label);
    __ LoadP(scratch, FieldMemOperand(input, HeapObject::kMapOffset));
    __ lbz(scratch, FieldMemOperand(scratch, Map::kBitFieldOffset));
    __ andi(scratch, scratch,
            Operand((1 << Map::kIsCallable) | (1 << Map::kIsUndetectable)));
    __ cmpi(scratch, Operand(1 << Map::kIsCallable));
    final_branch_condition = eq;

  } else if (String::Equals(type_name, factory->object_string())) {
    __ JumpIfSmi(input, false_label);
    __ CompareRoot(input, Heap::kNullValueRootIndex);
    __ beq(true_label);
    STATIC_ASSERT(LAST_JS_RECEIVER_TYPE == LAST_TYPE);
    __ CompareObjectType(input, scratch, ip, FIRST_JS_RECEIVER_TYPE);
    __ blt(false_label);
    // Check for callable or undetectable objects => false.
    __ lbz(scratch, FieldMemOperand(scratch, Map::kBitFieldOffset));
    __ andi(r0, scratch,
            Operand((1 << Map::kIsCallable) | (1 << Map::kIsUndetectable)));
    __ cmpi(r0, Operand::Zero());
    final_branch_condition = eq;

// clang-format off
#define SIMD128_TYPE(TYPE, Type, type, lane_count, lane_type)        \
  } else if (String::Equals(type_name, factory->type##_string())) {  \
    __ JumpIfSmi(input, false_label);                                \
    __ LoadP(scratch, FieldMemOperand(input, HeapObject::kMapOffset)); \
    __ CompareRoot(scratch, Heap::k##Type##MapRootIndex);            \
    final_branch_condition = eq;
  SIMD128_TYPES(SIMD128_TYPE)
#undef SIMD128_TYPE
    // clang-format on

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
      DCHECK_EQ(0, padding_size % Assembler::kInstrSize);
      while (padding_size > 0) {
        __ nop();
        padding_size -= Assembler::kInstrSize;
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
    __ LoadRoot(ip, Heap::kStackLimitRootIndex);
    __ cmpl(sp, ip);
    __ bge(&done);
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
    __ LoadRoot(ip, Heap::kStackLimitRootIndex);
    __ cmpl(sp, ip);
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

  __ LoadP(r3, FieldMemOperand(r3, HeapObject::kMapOffset));
  __ b(&use_cache);

  // Get the set of properties to enumerate.
  __ bind(&call_runtime);
  __ push(r3);
  CallRuntime(Runtime::kForInEnumerate, instr);
  __ bind(&use_cache);
}


void LCodeGen::DoForInCacheArray(LForInCacheArray* instr) {
  Register map = ToRegister(instr->map());
  Register result = ToRegister(instr->result());
  Label load_cache, done;
  __ EnumLength(result, map);
  __ CmpSmiLiteral(result, Smi::FromInt(0), r0);
  __ bne(&load_cache);
  __ mov(result, Operand(isolate()->factory()->empty_fixed_array()));
  __ b(&done);

  __ bind(&load_cache);
  __ LoadInstanceDescriptors(map, result);
  __ LoadP(result, FieldMemOperand(result, DescriptorArray::kEnumCacheOffset));
  __ LoadP(result, FieldMemOperand(result, FixedArray::SizeFor(instr->idx())));
  __ cmpi(result, Operand::Zero());
  DeoptimizeIf(eq, instr, Deoptimizer::kNoCache);

  __ bind(&done);
}


void LCodeGen::DoCheckMapValue(LCheckMapValue* instr) {
  Register object = ToRegister(instr->value());
  Register map = ToRegister(instr->map());
  __ LoadP(scratch0(), FieldMemOperand(object, HeapObject::kMapOffset));
  __ cmp(map, scratch0());
  DeoptimizeIf(ne, instr, Deoptimizer::kWrongMap);
}


void LCodeGen::DoDeferredLoadMutableDouble(LLoadFieldByIndex* instr,
                                           Register result, Register object,
                                           Register index) {
  PushSafepointRegistersScope scope(this);
  __ Push(object, index);
  __ li(cp, Operand::Zero());
  __ CallRuntimeSaveDoubles(Runtime::kLoadMutableDouble);
  RecordSafepointWithRegisters(instr->pointer_map(), 2,
                               Safepoint::kNoLazyDeopt);
  __ StoreToSafepointRegisterSlot(r3, result);
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
  __ bne(deferred->entry(), cr0);
  __ ShiftRightArithImm(index, index, 1);

  __ cmpi(index, Operand::Zero());
  __ blt(&out_of_object);

  __ SmiToPtrArrayOffset(r0, index);
  __ add(scratch, object, r0);
  __ LoadP(result, FieldMemOperand(scratch, JSObject::kHeaderSize));

  __ b(&done);

  __ bind(&out_of_object);
  __ LoadP(result, FieldMemOperand(object, JSObject::kPropertiesOffset));
  // Index is equal to negated out of object property index plus 1.
  __ SmiToPtrArrayOffset(r0, index);
  __ sub(scratch, result, r0);
  __ LoadP(result,
           FieldMemOperand(scratch, FixedArray::kHeaderSize - kPointerSize));
  __ bind(deferred->exit());
  __ bind(&done);
}


void LCodeGen::DoStoreFrameContext(LStoreFrameContext* instr) {
  Register context = ToRegister(instr->context());
  __ StoreP(context, MemOperand(fp, StandardFrameConstants::kContextOffset));
}


#undef __
}  // namespace internal
}  // namespace v8
