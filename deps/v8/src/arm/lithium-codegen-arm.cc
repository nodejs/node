// Copyright 2012 the V8 project authors. All rights reserved.
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
//       copyright notice, this list of conditions and the following
//       disclaimer in the documentation and/or other materials provided
//       with the distribution.
//     * Neither the name of Google Inc. nor the names of its
//       contributors may be used to endorse or promote products derived
//       from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include "v8.h"

#include "arm/lithium-codegen-arm.h"
#include "arm/lithium-gap-resolver-arm.h"
#include "code-stubs.h"
#include "stub-cache.h"
#include "hydrogen-osr.h"

namespace v8 {
namespace internal {


class SafepointGenerator V8_FINAL : public CallWrapper {
 public:
  SafepointGenerator(LCodeGen* codegen,
                     LPointerMap* pointers,
                     Safepoint::DeoptMode mode)
      : codegen_(codegen),
        pointers_(pointers),
        deopt_mode_(mode) { }
  virtual ~SafepointGenerator() {}

  virtual void BeforeCall(int call_size) const V8_OVERRIDE {}

  virtual void AfterCall() const V8_OVERRIDE {
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
  ASSERT(is_unused());
  status_ = GENERATING;

  // Open a frame scope to indicate that there is a frame on the stack.  The
  // NONE indicates that the scope shouldn't actually generate code to set up
  // the frame (that is done in GeneratePrologue).
  FrameScope frame_scope(masm_, StackFrame::NONE);

  return GeneratePrologue() &&
      GenerateBody() &&
      GenerateDeferredCode() &&
      GenerateDeoptJumpTable() &&
      GenerateSafepointTable();
}


void LCodeGen::FinishCode(Handle<Code> code) {
  ASSERT(is_done());
  code->set_stack_slots(GetStackSlotCount());
  code->set_safepoint_table_offset(safepoints_.GetCodeOffset());
  if (FLAG_weak_embedded_maps_in_optimized_code) {
    RegisterDependentCodeForEmbeddedMaps(code);
  }
  PopulateDeoptimizationData(code);
  info()->CommitDependencies(code);
}


void LCodeGen::Abort(BailoutReason reason) {
  info()->set_bailout_reason(reason);
  status_ = ABORTED;
}


void LCodeGen::Comment(const char* format, ...) {
  if (!FLAG_code_comments) return;
  char buffer[4 * KB];
  StringBuilder builder(buffer, ARRAY_SIZE(buffer));
  va_list arguments;
  va_start(arguments, format);
  builder.AddFormattedList(format, arguments);
  va_end(arguments);

  // Copy the string before recording it in the assembler to avoid
  // issues when the stack allocated buffer goes out of scope.
  size_t length = builder.position();
  Vector<char> copy = Vector<char>::New(length + 1);
  OS::MemCopy(copy.start(), builder.Finalize(), copy.length());
  masm()->RecordComment(copy.start());
}


bool LCodeGen::GeneratePrologue() {
  ASSERT(is_generating());

  if (info()->IsOptimizing()) {
    ProfileEntryHookStub::MaybeCallEntryHook(masm_);

#ifdef DEBUG
    if (strlen(FLAG_stop_at) > 0 &&
        info_->function()->name()->IsUtf8EqualTo(CStrVector(FLAG_stop_at))) {
      __ stop("stop_at");
    }
#endif

    // r1: Callee's JS function.
    // cp: Callee's context.
    // fp: Caller's frame pointer.
    // lr: Caller's pc.

    // Strict mode functions and builtins need to replace the receiver
    // with undefined when called as functions (without an explicit
    // receiver object). r5 is zero for method calls and non-zero for
    // function calls.
    if (!info_->is_classic_mode() || info_->is_native()) {
      Label ok;
      __ cmp(r5, Operand::Zero());
      __ b(eq, &ok);
      int receiver_offset = scope()->num_parameters() * kPointerSize;
      __ LoadRoot(r2, Heap::kUndefinedValueRootIndex);
      __ str(r2, MemOperand(sp, receiver_offset));
      __ bind(&ok);
    }
  }

  info()->set_prologue_offset(masm_->pc_offset());
  if (NeedsEagerFrame()) {
    if (info()->IsStub()) {
      __ stm(db_w, sp, cp.bit() | fp.bit() | lr.bit());
      __ Push(Smi::FromInt(StackFrame::STUB));
      // Adjust FP to point to saved FP.
      __ add(fp, sp, Operand(2 * kPointerSize));
    } else {
      PredictableCodeSizeScope predictible_code_size_scope(
          masm_, kNoCodeAgeSequenceLength * Assembler::kInstrSize);
      // The following three instructions must remain together and unmodified
      // for code aging to work properly.
      __ stm(db_w, sp, r1.bit() | cp.bit() | fp.bit() | lr.bit());
      __ nop(ip.code());
      // Adjust FP to point to saved FP.
      __ add(fp, sp, Operand(2 * kPointerSize));
    }
    frame_is_built_ = true;
    info_->AddNoFrameRange(0, masm_->pc_offset());
  }

  // Reserve space for the stack slots needed by the code.
  int slots = GetStackSlotCount();
  if (slots > 0) {
    if (FLAG_debug_code) {
      __ sub(sp,  sp, Operand(slots * kPointerSize));
      __ push(r0);
      __ push(r1);
      __ add(r0, sp, Operand(slots *  kPointerSize));
      __ mov(r1, Operand(kSlotsZapValue));
      Label loop;
      __ bind(&loop);
      __ sub(r0, r0, Operand(kPointerSize));
      __ str(r1, MemOperand(r0, 2 * kPointerSize));
      __ cmp(r0, sp);
      __ b(ne, &loop);
      __ pop(r1);
      __ pop(r0);
    } else {
      __ sub(sp,  sp, Operand(slots * kPointerSize));
    }
  }

  if (info()->saves_caller_doubles()) {
    Comment(";;; Save clobbered callee double registers");
    int count = 0;
    BitVector* doubles = chunk()->allocated_double_registers();
    BitVector::Iterator save_iterator(doubles);
    while (!save_iterator.Done()) {
      __ vstr(DwVfpRegister::FromAllocationIndex(save_iterator.Current()),
              MemOperand(sp, count * kDoubleSize));
      save_iterator.Advance();
      count++;
    }
  }

  // Possibly allocate a local context.
  int heap_slots = info()->num_heap_slots() - Context::MIN_CONTEXT_SLOTS;
  if (heap_slots > 0) {
    Comment(";;; Allocate local context");
    // Argument to NewContext is the function, which is in r1.
    __ push(r1);
    if (heap_slots <= FastNewContextStub::kMaximumSlots) {
      FastNewContextStub stub(heap_slots);
      __ CallStub(&stub);
    } else {
      __ CallRuntime(Runtime::kNewFunctionContext, 1);
    }
    RecordSafepoint(Safepoint::kNoLazyDeopt);
    // Context is returned in both r0 and cp.  It replaces the context
    // passed to us.  It's saved in the stack and kept live in cp.
    __ str(cp, MemOperand(fp, StandardFrameConstants::kContextOffset));
    // Copy any necessary parameters into the context.
    int num_parameters = scope()->num_parameters();
    for (int i = 0; i < num_parameters; i++) {
      Variable* var = scope()->parameter(i);
      if (var->IsContextSlot()) {
        int parameter_offset = StandardFrameConstants::kCallerSPOffset +
            (num_parameters - 1 - i) * kPointerSize;
        // Load parameter from stack.
        __ ldr(r0, MemOperand(fp, parameter_offset));
        // Store it in the context.
        MemOperand target = ContextOperand(cp, var->index());
        __ str(r0, target);
        // Update the write barrier. This clobbers r3 and r0.
        __ RecordWriteContextSlot(
            cp,
            target.offset(),
            r0,
            r3,
            GetLinkRegisterState(),
            kSaveFPRegs);
      }
    }
    Comment(";;; End allocate local context");
  }

  // Trace the call.
  if (FLAG_trace && info()->IsOptimizing()) {
    __ CallRuntime(Runtime::kTraceEnter, 0);
  }
  return !is_aborted();
}


void LCodeGen::GenerateOsrPrologue() {
  // Generate the OSR entry prologue at the first unknown OSR value, or if there
  // are none, at the OSR entrypoint instruction.
  if (osr_pc_offset_ >= 0) return;

  osr_pc_offset_ = masm()->pc_offset();

  // Adjust the frame size, subsuming the unoptimized frame into the
  // optimized frame.
  int slots = GetStackSlotCount() - graph()->osr()->UnoptimizedFrameSlots();
  ASSERT(slots >= 0);
  __ sub(sp, sp, Operand(slots * kPointerSize));
}


bool LCodeGen::GenerateBody() {
  ASSERT(is_generating());
  bool emit_instructions = true;
  for (current_instruction_ = 0;
       !is_aborted() && current_instruction_ < instructions_->length();
       current_instruction_++) {
    LInstruction* instr = instructions_->at(current_instruction_);

    // Don't emit code for basic blocks with a replacement.
    if (instr->IsLabel()) {
      emit_instructions = !LLabel::cast(instr)->HasReplacement();
    }
    if (!emit_instructions) continue;

    if (FLAG_code_comments && instr->HasInterestingComment(this)) {
      Comment(";;; <@%d,#%d> %s",
              current_instruction_,
              instr->hydrogen_value()->id(),
              instr->Mnemonic());
    }

    RecordAndUpdatePosition(instr->position());

    instr->CompileToNative(this);
  }
  EnsureSpaceForLazyDeopt();
  last_lazy_deopt_pc_ = masm()->pc_offset();
  return !is_aborted();
}


bool LCodeGen::GenerateDeferredCode() {
  ASSERT(is_generating());
  if (deferred_.length() > 0) {
    for (int i = 0; !is_aborted() && i < deferred_.length(); i++) {
      LDeferredCode* code = deferred_[i];

      int pos = instructions_->at(code->instruction_index())->position();
      RecordAndUpdatePosition(pos);

      Comment(";;; <@%d,#%d> "
              "-------------------- Deferred %s --------------------",
              code->instruction_index(),
              code->instr()->hydrogen_value()->id(),
              code->instr()->Mnemonic());
      __ bind(code->entry());
      if (NeedsDeferredFrame()) {
        Comment(";;; Build frame");
        ASSERT(!frame_is_built_);
        ASSERT(info()->IsStub());
        frame_is_built_ = true;
        __ stm(db_w, sp, cp.bit() | fp.bit() | lr.bit());
        __ mov(scratch0(), Operand(Smi::FromInt(StackFrame::STUB)));
        __ push(scratch0());
        __ add(fp, sp, Operand(2 * kPointerSize));
        Comment(";;; Deferred code");
      }
      code->Generate();
      if (NeedsDeferredFrame()) {
        Comment(";;; Destroy frame");
        ASSERT(frame_is_built_);
        __ pop(ip);
        __ ldm(ia_w, sp, cp.bit() | fp.bit() | lr.bit());
        frame_is_built_ = false;
      }
      __ jmp(code->exit());
    }
  }

  // Force constant pool emission at the end of the deferred code to make
  // sure that no constant pools are emitted after.
  masm()->CheckConstPool(true, false);

  return !is_aborted();
}


bool LCodeGen::GenerateDeoptJumpTable() {
  // Check that the jump table is accessible from everywhere in the function
  // code, i.e. that offsets to the table can be encoded in the 24bit signed
  // immediate of a branch instruction.
  // To simplify we consider the code size from the first instruction to the
  // end of the jump table. We also don't consider the pc load delta.
  // Each entry in the jump table generates one instruction and inlines one
  // 32bit data after it.
  if (!is_int24((masm()->pc_offset() / Assembler::kInstrSize) +
      deopt_jump_table_.length() * 7)) {
    Abort(kGeneratedCodeIsTooLarge);
  }

  if (deopt_jump_table_.length() > 0) {
    Comment(";;; -------------------- Jump table --------------------");
  }
  Label table_start;
  __ bind(&table_start);
  Label needs_frame;
  for (int i = 0; i < deopt_jump_table_.length(); i++) {
    __ bind(&deopt_jump_table_[i].label);
    Address entry = deopt_jump_table_[i].address;
    Deoptimizer::BailoutType type = deopt_jump_table_[i].bailout_type;
    int id = Deoptimizer::GetDeoptimizationId(isolate(), entry, type);
    if (id == Deoptimizer::kNotDeoptimizationEntry) {
      Comment(";;; jump table entry %d.", i);
    } else {
      Comment(";;; jump table entry %d: deoptimization bailout %d.", i, id);
    }
    if (deopt_jump_table_[i].needs_frame) {
      __ mov(ip, Operand(ExternalReference::ForDeoptEntry(entry)));
      if (needs_frame.is_bound()) {
        __ b(&needs_frame);
      } else {
        __ bind(&needs_frame);
        __ stm(db_w, sp, cp.bit() | fp.bit() | lr.bit());
        // This variant of deopt can only be used with stubs. Since we don't
        // have a function pointer to install in the stack frame that we're
        // building, install a special marker there instead.
        ASSERT(info()->IsStub());
        __ mov(scratch0(), Operand(Smi::FromInt(StackFrame::STUB)));
        __ push(scratch0());
        __ add(fp, sp, Operand(2 * kPointerSize));
        __ mov(lr, Operand(pc), LeaveCC, al);
        __ mov(pc, ip);
      }
    } else {
      __ mov(lr, Operand(pc), LeaveCC, al);
      __ mov(pc, Operand(ExternalReference::ForDeoptEntry(entry)));
    }
    masm()->CheckConstPool(false, false);
  }

  // Force constant pool emission at the end of the deopt jump table to make
  // sure that no constant pools are emitted after.
  masm()->CheckConstPool(true, false);

  // The deoptimization jump table is the last part of the instruction
  // sequence. Mark the generated code as done unless we bailed out.
  if (!is_aborted()) status_ = DONE;
  return !is_aborted();
}


bool LCodeGen::GenerateSafepointTable() {
  ASSERT(is_done());
  safepoints_.Emit(masm(), GetStackSlotCount());
  return !is_aborted();
}


Register LCodeGen::ToRegister(int index) const {
  return Register::FromAllocationIndex(index);
}


DwVfpRegister LCodeGen::ToDoubleRegister(int index) const {
  return DwVfpRegister::FromAllocationIndex(index);
}


Register LCodeGen::ToRegister(LOperand* op) const {
  ASSERT(op->IsRegister());
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
      ASSERT(literal->IsNumber());
      __ mov(scratch, Operand(static_cast<int32_t>(literal->Number())));
    } else if (r.IsDouble()) {
      Abort(kEmitLoadRegisterUnsupportedDoubleImmediate);
    } else {
      ASSERT(r.IsSmiOrTagged());
      __ LoadObject(scratch, literal);
    }
    return scratch;
  } else if (op->IsStackSlot() || op->IsArgument()) {
    __ ldr(scratch, ToMemOperand(op));
    return scratch;
  }
  UNREACHABLE();
  return scratch;
}


DwVfpRegister LCodeGen::ToDoubleRegister(LOperand* op) const {
  ASSERT(op->IsDoubleRegister());
  return ToDoubleRegister(op->index());
}


DwVfpRegister LCodeGen::EmitLoadDoubleRegister(LOperand* op,
                                               SwVfpRegister flt_scratch,
                                               DwVfpRegister dbl_scratch) {
  if (op->IsDoubleRegister()) {
    return ToDoubleRegister(op->index());
  } else if (op->IsConstantOperand()) {
    LConstantOperand* const_op = LConstantOperand::cast(op);
    HConstant* constant = chunk_->LookupConstant(const_op);
    Handle<Object> literal = constant->handle(isolate());
    Representation r = chunk_->LookupLiteralRepresentation(const_op);
    if (r.IsInteger32()) {
      ASSERT(literal->IsNumber());
      __ mov(ip, Operand(static_cast<int32_t>(literal->Number())));
      __ vmov(flt_scratch, ip);
      __ vcvt_f64_s32(dbl_scratch, flt_scratch);
      return dbl_scratch;
    } else if (r.IsDouble()) {
      Abort(kUnsupportedDoubleImmediate);
    } else if (r.IsTagged()) {
      Abort(kUnsupportedTaggedImmediate);
    }
  } else if (op->IsStackSlot() || op->IsArgument()) {
    // TODO(regis): Why is vldr not taking a MemOperand?
    // __ vldr(dbl_scratch, ToMemOperand(op));
    MemOperand mem_op = ToMemOperand(op);
    __ vldr(dbl_scratch, mem_op.rn(), mem_op.offset());
    return dbl_scratch;
  }
  UNREACHABLE();
  return dbl_scratch;
}


Handle<Object> LCodeGen::ToHandle(LConstantOperand* op) const {
  HConstant* constant = chunk_->LookupConstant(op);
  ASSERT(chunk_->LookupLiteralRepresentation(op).IsSmiOrTagged());
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


int32_t LCodeGen::ToRepresentation(LConstantOperand* op,
                                   const Representation& r) const {
  HConstant* constant = chunk_->LookupConstant(op);
  int32_t value = constant->Integer32Value();
  if (r.IsInteger32()) return value;
  ASSERT(r.IsSmiOrTagged());
  return reinterpret_cast<int32_t>(Smi::FromInt(value));
}


Smi* LCodeGen::ToSmi(LConstantOperand* op) const {
  HConstant* constant = chunk_->LookupConstant(op);
  return Smi::FromInt(constant->Integer32Value());
}


double LCodeGen::ToDouble(LConstantOperand* op) const {
  HConstant* constant = chunk_->LookupConstant(op);
  ASSERT(constant->HasDoubleValue());
  return constant->DoubleValue();
}


Operand LCodeGen::ToOperand(LOperand* op) {
  if (op->IsConstantOperand()) {
    LConstantOperand* const_op = LConstantOperand::cast(op);
    HConstant* constant = chunk()->LookupConstant(const_op);
    Representation r = chunk_->LookupLiteralRepresentation(const_op);
    if (r.IsSmi()) {
      ASSERT(constant->HasSmiValue());
      return Operand(Smi::FromInt(constant->Integer32Value()));
    } else if (r.IsInteger32()) {
      ASSERT(constant->HasInteger32Value());
      return Operand(constant->Integer32Value());
    } else if (r.IsDouble()) {
      Abort(kToOperandUnsupportedDoubleImmediate);
    }
    ASSERT(r.IsTagged());
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


MemOperand LCodeGen::ToMemOperand(LOperand* op) const {
  ASSERT(!op->IsRegister());
  ASSERT(!op->IsDoubleRegister());
  ASSERT(op->IsStackSlot() || op->IsDoubleStackSlot());
  return MemOperand(fp, StackSlotOffset(op->index()));
}


MemOperand LCodeGen::ToHighMemOperand(LOperand* op) const {
  ASSERT(op->IsDoubleStackSlot());
  return MemOperand(fp, StackSlotOffset(op->index()) + kPointerSize);
}


void LCodeGen::WriteTranslation(LEnvironment* environment,
                                Translation* translation) {
  if (environment == NULL) return;

  // The translation includes one command per value in the environment.
  int translation_size = environment->translation_size();
  // The output frame height does not include the parameters.
  int height = translation_size - environment->parameter_count();

  WriteTranslation(environment->outer(), translation);
  bool has_closure_id = !info()->closure().is_null() &&
      !info()->closure().is_identical_to(environment->closure());
  int closure_id = has_closure_id
      ? DefineDeoptimizationLiteral(environment->closure())
      : Translation::kSelfLiteralId;

  switch (environment->frame_type()) {
    case JS_FUNCTION:
      translation->BeginJSFrame(environment->ast_id(), closure_id, height);
      break;
    case JS_CONSTRUCT:
      translation->BeginConstructStubFrame(closure_id, translation_size);
      break;
    case JS_GETTER:
      ASSERT(translation_size == 1);
      ASSERT(height == 0);
      translation->BeginGetterStubFrame(closure_id);
      break;
    case JS_SETTER:
      ASSERT(translation_size == 2);
      ASSERT(height == 0);
      translation->BeginSetterStubFrame(closure_id);
      break;
    case STUB:
      translation->BeginCompiledStubFrame();
      break;
    case ARGUMENTS_ADAPTOR:
      translation->BeginArgumentsAdaptorFrame(closure_id, translation_size);
      break;
  }

  int object_index = 0;
  int dematerialized_index = 0;
  for (int i = 0; i < translation_size; ++i) {
    LOperand* value = environment->values()->at(i);
    AddToTranslation(environment,
                     translation,
                     value,
                     environment->HasTaggedValueAt(i),
                     environment->HasUint32ValueAt(i),
                     &object_index,
                     &dematerialized_index);
  }
}


void LCodeGen::AddToTranslation(LEnvironment* environment,
                                Translation* translation,
                                LOperand* op,
                                bool is_tagged,
                                bool is_uint32,
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
      AddToTranslation(environment,
                       translation,
                       value,
                       environment->HasTaggedValueAt(env_offset + i),
                       environment->HasUint32ValueAt(env_offset + i),
                       object_index_pointer,
                       dematerialized_index_pointer);
    }
    return;
  }

  if (op->IsStackSlot()) {
    if (is_tagged) {
      translation->StoreStackSlot(op->index());
    } else if (is_uint32) {
      translation->StoreUint32StackSlot(op->index());
    } else {
      translation->StoreInt32StackSlot(op->index());
    }
  } else if (op->IsDoubleStackSlot()) {
    translation->StoreDoubleStackSlot(op->index());
  } else if (op->IsArgument()) {
    ASSERT(is_tagged);
    int src_index = GetStackSlotCount() + op->index();
    translation->StoreStackSlot(src_index);
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


void LCodeGen::CallCode(Handle<Code> code,
                        RelocInfo::Mode mode,
                        LInstruction* instr,
                        TargetAddressStorageMode storage_mode) {
  CallCodeGeneric(code, mode, instr, RECORD_SIMPLE_SAFEPOINT, storage_mode);
}


void LCodeGen::CallCodeGeneric(Handle<Code> code,
                               RelocInfo::Mode mode,
                               LInstruction* instr,
                               SafepointMode safepoint_mode,
                               TargetAddressStorageMode storage_mode) {
  EnsureSpaceForLazyDeopt();
  ASSERT(instr != NULL);
  // Block literal pool emission to ensure nop indicating no inlined smi code
  // is in the correct position.
  Assembler::BlockConstPoolScope block_const_pool(masm());
  LPointerMap* pointers = instr->pointer_map();
  RecordPosition(pointers->position());
  __ Call(code, mode, TypeFeedbackId::None(), al, storage_mode);
  RecordSafepointWithLazyDeopt(instr, safepoint_mode);

  // Signal that we don't inline smi code before these stubs in the
  // optimizing code generator.
  if (code->kind() == Code::BINARY_OP_IC ||
      code->kind() == Code::COMPARE_IC) {
    __ nop();
  }
}


void LCodeGen::CallRuntime(const Runtime::Function* function,
                           int num_arguments,
                           LInstruction* instr) {
  ASSERT(instr != NULL);
  LPointerMap* pointers = instr->pointer_map();
  ASSERT(pointers != NULL);
  RecordPosition(pointers->position());

  __ CallRuntime(function, num_arguments);
  RecordSafepointWithLazyDeopt(instr, RECORD_SIMPLE_SAFEPOINT);
}


void LCodeGen::CallRuntimeFromDeferred(Runtime::FunctionId id,
                                       int argc,
                                       LInstruction* instr) {
  __ CallRuntimeSaveDoubles(id);
  RecordSafepointWithRegisters(
      instr->pointer_map(), argc, Safepoint::kNoLazyDeopt);
}


void LCodeGen::RegisterEnvironmentForDeoptimization(LEnvironment* environment,
                                                    Safepoint::DeoptMode mode) {
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
    environment->Register(deoptimization_index,
                          translation.index(),
                          (mode == Safepoint::kLazyDeopt) ? pc_offset : -1);
    deoptimizations_.Add(environment, zone());
  }
}


void LCodeGen::DeoptimizeIf(Condition condition,
                            LEnvironment* environment,
                            Deoptimizer::BailoutType bailout_type) {
  RegisterEnvironmentForDeoptimization(environment, Safepoint::kNoLazyDeopt);
  ASSERT(environment->HasBeenRegistered());
  int id = environment->deoptimization_index();
  ASSERT(info()->IsOptimizing() || info()->IsStub());
  Address entry =
      Deoptimizer::GetDeoptimizationEntry(isolate(), id, bailout_type);
  if (entry == NULL) {
    Abort(kBailoutWasNotPrepared);
    return;
  }

  ASSERT(FLAG_deopt_every_n_times < 2);  // Other values not supported on ARM.
  if (FLAG_deopt_every_n_times == 1 &&
      !info()->IsStub() &&
      info()->opt_count() == id) {
    ASSERT(frame_is_built_);
    __ Call(entry, RelocInfo::RUNTIME_ENTRY);
    return;
  }

  if (info()->ShouldTrapOnDeopt()) {
    __ stop("trap_on_deopt", condition);
  }

  ASSERT(info()->IsStub() || frame_is_built_);
  if (condition == al && frame_is_built_) {
    __ Call(entry, RelocInfo::RUNTIME_ENTRY);
  } else {
    // We often have several deopts to the same entry, reuse the last
    // jump entry if this is the case.
    if (deopt_jump_table_.is_empty() ||
        (deopt_jump_table_.last().address != entry) ||
        (deopt_jump_table_.last().bailout_type != bailout_type) ||
        (deopt_jump_table_.last().needs_frame != !frame_is_built_)) {
      Deoptimizer::JumpTableEntry table_entry(entry,
                                              bailout_type,
                                              !frame_is_built_);
      deopt_jump_table_.Add(table_entry, zone());
    }
    __ b(condition, &deopt_jump_table_.last().label);
  }
}


void LCodeGen::DeoptimizeIf(Condition condition,
                            LEnvironment* environment) {
  Deoptimizer::BailoutType bailout_type = info()->IsStub()
      ? Deoptimizer::LAZY
      : Deoptimizer::EAGER;
  DeoptimizeIf(condition, environment, bailout_type);
}


void LCodeGen::RegisterDependentCodeForEmbeddedMaps(Handle<Code> code) {
  ZoneList<Handle<Map> > maps(1, zone());
  int mode_mask = RelocInfo::ModeMask(RelocInfo::EMBEDDED_OBJECT);
  for (RelocIterator it(*code, mode_mask); !it.done(); it.next()) {
    RelocInfo::Mode mode = it.rinfo()->rmode();
    if (mode == RelocInfo::EMBEDDED_OBJECT &&
        it.rinfo()->target_object()->IsMap()) {
      Handle<Map> map(Map::cast(it.rinfo()->target_object()));
      if (map->CanTransition()) {
        maps.Add(map, zone());
      }
    }
  }
#ifdef VERIFY_HEAP
  // This disables verification of weak embedded maps after full GC.
  // AddDependentCode can cause a GC, which would observe the state where
  // this code is not yet in the depended code lists of the embedded maps.
  NoWeakEmbeddedMapsVerificationScope disable_verification_of_embedded_maps;
#endif
  for (int i = 0; i < maps.length(); i++) {
    maps.at(i)->AddDependentCode(DependentCode::kWeaklyEmbeddedGroup, code);
  }
}


void LCodeGen::PopulateDeoptimizationData(Handle<Code> code) {
  int length = deoptimizations_.length();
  if (length == 0) return;
  Handle<DeoptimizationInputData> data =
      factory()->NewDeoptimizationInputData(length, TENURED);

  Handle<ByteArray> translations =
      translations_.CreateByteArray(isolate()->factory());
  data->SetTranslationByteArray(*translations);
  data->SetInlinedFunctionCount(Smi::FromInt(inlined_function_count_));

  Handle<FixedArray> literals =
      factory()->NewFixedArray(deoptimization_literals_.length(), TENURED);
  { AllowDeferredHandleDereference copy_handles;
    for (int i = 0; i < deoptimization_literals_.length(); i++) {
      literals->set(i, *deoptimization_literals_[i]);
    }
    data->SetLiteralArray(*literals);
  }

  data->SetOsrAstId(Smi::FromInt(info_->osr_ast_id().ToInt()));
  data->SetOsrPcOffset(Smi::FromInt(osr_pc_offset_));

  // Populate the deoptimization entries.
  for (int i = 0; i < length; i++) {
    LEnvironment* env = deoptimizations_[i];
    data->SetAstId(i, env->ast_id());
    data->SetTranslationIndex(i, Smi::FromInt(env->translation_index()));
    data->SetArgumentsStackHeight(i,
                                  Smi::FromInt(env->arguments_stack_height()));
    data->SetPc(i, Smi::FromInt(env->pc_offset()));
  }
  code->set_deoptimization_data(*data);
}


int LCodeGen::DefineDeoptimizationLiteral(Handle<Object> literal) {
  int result = deoptimization_literals_.length();
  for (int i = 0; i < deoptimization_literals_.length(); ++i) {
    if (deoptimization_literals_[i].is_identical_to(literal)) return i;
  }
  deoptimization_literals_.Add(literal, zone());
  return result;
}


void LCodeGen::PopulateDeoptimizationLiteralsWithInlinedFunctions() {
  ASSERT(deoptimization_literals_.length() == 0);

  const ZoneList<Handle<JSFunction> >* inlined_closures =
      chunk()->inlined_closures();

  for (int i = 0, length = inlined_closures->length();
       i < length;
       i++) {
    DefineDeoptimizationLiteral(inlined_closures->at(i));
  }

  inlined_function_count_ = deoptimization_literals_.length();
}


void LCodeGen::RecordSafepointWithLazyDeopt(
    LInstruction* instr, SafepointMode safepoint_mode) {
  if (safepoint_mode == RECORD_SIMPLE_SAFEPOINT) {
    RecordSafepoint(instr->pointer_map(), Safepoint::kLazyDeopt);
  } else {
    ASSERT(safepoint_mode == RECORD_SAFEPOINT_WITH_REGISTERS_AND_NO_ARGUMENTS);
    RecordSafepointWithRegisters(
        instr->pointer_map(), 0, Safepoint::kLazyDeopt);
  }
}


void LCodeGen::RecordSafepoint(
    LPointerMap* pointers,
    Safepoint::Kind kind,
    int arguments,
    Safepoint::DeoptMode deopt_mode) {
  ASSERT(expected_safepoint_kind_ == kind);

  const ZoneList<LOperand*>* operands = pointers->GetNormalizedOperands();
  Safepoint safepoint = safepoints_.DefineSafepoint(masm(),
      kind, arguments, deopt_mode);
  for (int i = 0; i < operands->length(); i++) {
    LOperand* pointer = operands->at(i);
    if (pointer->IsStackSlot()) {
      safepoint.DefinePointerSlot(pointer->index(), zone());
    } else if (pointer->IsRegister() && (kind & Safepoint::kWithRegisters)) {
      safepoint.DefinePointerRegister(ToRegister(pointer), zone());
    }
  }
  if (kind & Safepoint::kWithRegisters) {
    // Register cp always contains a pointer to the context.
    safepoint.DefinePointerRegister(cp, zone());
  }
}


void LCodeGen::RecordSafepoint(LPointerMap* pointers,
                               Safepoint::DeoptMode deopt_mode) {
  RecordSafepoint(pointers, Safepoint::kSimple, 0, deopt_mode);
}


void LCodeGen::RecordSafepoint(Safepoint::DeoptMode deopt_mode) {
  LPointerMap empty_pointers(RelocInfo::kNoPosition, zone());
  RecordSafepoint(&empty_pointers, deopt_mode);
}


void LCodeGen::RecordSafepointWithRegisters(LPointerMap* pointers,
                                            int arguments,
                                            Safepoint::DeoptMode deopt_mode) {
  RecordSafepoint(
      pointers, Safepoint::kWithRegisters, arguments, deopt_mode);
}


void LCodeGen::RecordSafepointWithRegistersAndDoubles(
    LPointerMap* pointers,
    int arguments,
    Safepoint::DeoptMode deopt_mode) {
  RecordSafepoint(
      pointers, Safepoint::kWithRegistersAndDoubles, arguments, deopt_mode);
}


void LCodeGen::RecordPosition(int position) {
  if (position == RelocInfo::kNoPosition) return;
  masm()->positions_recorder()->RecordPosition(position);
}


void LCodeGen::RecordAndUpdatePosition(int position) {
  if (position >= 0 && position != old_position_) {
    masm()->positions_recorder()->RecordPosition(position);
    old_position_ = position;
  }
}


static const char* LabelType(LLabel* label) {
  if (label->is_loop_header()) return " (loop header)";
  if (label->is_osr_entry()) return " (OSR entry)";
  return "";
}


void LCodeGen::DoLabel(LLabel* label) {
  Comment(";;; <@%d,#%d> -------------------- B%d%s --------------------",
          current_instruction_,
          label->hydrogen_value()->id(),
          label->block_id(),
          LabelType(label));
  __ bind(label->label());
  current_block_ = label->block_id();
  DoGap(label);
}


void LCodeGen::DoParallelMove(LParallelMove* move) {
  resolver_.Resolve(move);
}


void LCodeGen::DoGap(LGap* gap) {
  for (int i = LGap::FIRST_INNER_POSITION;
       i <= LGap::LAST_INNER_POSITION;
       i++) {
    LGap::InnerPosition inner_pos = static_cast<LGap::InnerPosition>(i);
    LParallelMove* move = gap->GetParallelMove(inner_pos);
    if (move != NULL) DoParallelMove(move);
  }
}


void LCodeGen::DoInstructionGap(LInstructionGap* instr) {
  DoGap(instr);
}


void LCodeGen::DoParameter(LParameter* instr) {
  // Nothing to do.
}


void LCodeGen::DoCallStub(LCallStub* instr) {
  ASSERT(ToRegister(instr->result()).is(r0));
  switch (instr->hydrogen()->major_key()) {
    case CodeStub::RegExpConstructResult: {
      RegExpConstructResultStub stub;
      CallCode(stub.GetCode(isolate()), RelocInfo::CODE_TARGET, instr);
      break;
    }
    case CodeStub::RegExpExec: {
      RegExpExecStub stub;
      CallCode(stub.GetCode(isolate()), RelocInfo::CODE_TARGET, instr);
      break;
    }
    case CodeStub::SubString: {
      SubStringStub stub;
      CallCode(stub.GetCode(isolate()), RelocInfo::CODE_TARGET, instr);
      break;
    }
    case CodeStub::NumberToString: {
      NumberToStringStub stub;
      CallCode(stub.GetCode(isolate()), RelocInfo::CODE_TARGET, instr);
      break;
    }
    case CodeStub::StringCompare: {
      StringCompareStub stub;
      CallCode(stub.GetCode(isolate()), RelocInfo::CODE_TARGET, instr);
      break;
    }
    case CodeStub::TranscendentalCache: {
      __ ldr(r0, MemOperand(sp, 0));
      TranscendentalCacheStub stub(instr->transcendental_type(),
                                   TranscendentalCacheStub::TAGGED);
      CallCode(stub.GetCode(isolate()), RelocInfo::CODE_TARGET, instr);
      break;
    }
    default:
      UNREACHABLE();
  }
}


void LCodeGen::DoUnknownOSRValue(LUnknownOSRValue* instr) {
  GenerateOsrPrologue();
}


void LCodeGen::DoModI(LModI* instr) {
  HMod* hmod = instr->hydrogen();
  HValue* left = hmod->left();
  HValue* right = hmod->right();
  if (hmod->HasPowerOf2Divisor()) {
    // TODO(svenpanne) We should really do the strength reduction on the
    // Hydrogen level.
    Register left_reg = ToRegister(instr->left());
    Register result_reg = ToRegister(instr->result());

    // Note: The code below even works when right contains kMinInt.
    int32_t divisor = Abs(right->GetInteger32Constant());

    Label left_is_not_negative, done;
    if (left->CanBeNegative()) {
      __ cmp(left_reg, Operand::Zero());
      __ b(pl, &left_is_not_negative);
      __ rsb(result_reg, left_reg, Operand::Zero());
      __ and_(result_reg, result_reg, Operand(divisor - 1));
      __ rsb(result_reg, result_reg, Operand::Zero(), SetCC);
      if (hmod->CheckFlag(HValue::kBailoutOnMinusZero)) {
        DeoptimizeIf(eq, instr->environment());
      }
      __ b(&done);
    }

    __ bind(&left_is_not_negative);
    __ and_(result_reg, left_reg, Operand(divisor - 1));
    __ bind(&done);

  } else if (hmod->fixed_right_arg().has_value) {
    Register left_reg = ToRegister(instr->left());
    Register right_reg = ToRegister(instr->right());
    Register result_reg = ToRegister(instr->result());

    int32_t divisor = hmod->fixed_right_arg().value;
    ASSERT(IsPowerOf2(divisor));

    // Check if our assumption of a fixed right operand still holds.
    __ cmp(right_reg, Operand(divisor));
    DeoptimizeIf(ne, instr->environment());

    Label left_is_not_negative, done;
    if (left->CanBeNegative()) {
      __ cmp(left_reg, Operand::Zero());
      __ b(pl, &left_is_not_negative);
      __ rsb(result_reg, left_reg, Operand::Zero());
      __ and_(result_reg, result_reg, Operand(divisor - 1));
      __ rsb(result_reg, result_reg, Operand::Zero(), SetCC);
      if (hmod->CheckFlag(HValue::kBailoutOnMinusZero)) {
        DeoptimizeIf(eq, instr->environment());
      }
      __ b(&done);
    }

    __ bind(&left_is_not_negative);
    __ and_(result_reg, left_reg, Operand(divisor - 1));
    __ bind(&done);

  } else if (CpuFeatures::IsSupported(SUDIV)) {
    CpuFeatureScope scope(masm(), SUDIV);

    Register left_reg = ToRegister(instr->left());
    Register right_reg = ToRegister(instr->right());
    Register result_reg = ToRegister(instr->result());

    Label done;
    // Check for x % 0, sdiv might signal an exception. We have to deopt in this
    // case because we can't return a NaN.
    if (right->CanBeZero()) {
      __ cmp(right_reg, Operand::Zero());
      DeoptimizeIf(eq, instr->environment());
    }

    // Check for kMinInt % -1, sdiv will return kMinInt, which is not what we
    // want. We have to deopt if we care about -0, because we can't return that.
    if (left->RangeCanInclude(kMinInt) && right->RangeCanInclude(-1)) {
      Label no_overflow_possible;
      __ cmp(left_reg, Operand(kMinInt));
      __ b(ne, &no_overflow_possible);
      __ cmp(right_reg, Operand(-1));
      if (hmod->CheckFlag(HValue::kBailoutOnMinusZero)) {
        DeoptimizeIf(eq, instr->environment());
      } else {
        __ b(ne, &no_overflow_possible);
        __ mov(result_reg, Operand::Zero());
        __ jmp(&done);
      }
      __ bind(&no_overflow_possible);
    }

    // For 'r3 = r1 % r2' we can have the following ARM code:
    //   sdiv r3, r1, r2
    //   mls r3, r3, r2, r1

    __ sdiv(result_reg, left_reg, right_reg);
    __ mls(result_reg, result_reg, right_reg, left_reg);

    // If we care about -0, test if the dividend is <0 and the result is 0.
    if (left->CanBeNegative() &&
        hmod->CanBeZero() &&
        hmod->CheckFlag(HValue::kBailoutOnMinusZero)) {
      __ cmp(result_reg, Operand::Zero());
      __ b(ne, &done);
      __ cmp(left_reg, Operand::Zero());
      DeoptimizeIf(lt, instr->environment());
    }
    __ bind(&done);

  } else {
    // General case, without any SDIV support.
    Register left_reg = ToRegister(instr->left());
    Register right_reg = ToRegister(instr->right());
    Register result_reg = ToRegister(instr->result());
    Register scratch = scratch0();
    ASSERT(!scratch.is(left_reg));
    ASSERT(!scratch.is(right_reg));
    ASSERT(!scratch.is(result_reg));
    DwVfpRegister dividend = ToDoubleRegister(instr->temp());
    DwVfpRegister divisor = ToDoubleRegister(instr->temp2());
    ASSERT(!divisor.is(dividend));
    LowDwVfpRegister quotient = double_scratch0();
    ASSERT(!quotient.is(dividend));
    ASSERT(!quotient.is(divisor));

    Label done;
    // Check for x % 0, we have to deopt in this case because we can't return a
    // NaN.
    if (right->CanBeZero()) {
      __ cmp(right_reg, Operand::Zero());
      DeoptimizeIf(eq, instr->environment());
    }

    __ Move(result_reg, left_reg);
    // Load the arguments in VFP registers. The divisor value is preloaded
    // before. Be careful that 'right_reg' is only live on entry.
    // TODO(svenpanne) The last comments seems to be wrong nowadays.
    __ vmov(double_scratch0().low(), left_reg);
    __ vcvt_f64_s32(dividend, double_scratch0().low());
    __ vmov(double_scratch0().low(), right_reg);
    __ vcvt_f64_s32(divisor, double_scratch0().low());

    // We do not care about the sign of the divisor. Note that we still handle
    // the kMinInt % -1 case correctly, though.
    __ vabs(divisor, divisor);
    // Compute the quotient and round it to a 32bit integer.
    __ vdiv(quotient, dividend, divisor);
    __ vcvt_s32_f64(quotient.low(), quotient);
    __ vcvt_f64_s32(quotient, quotient.low());

    // Compute the remainder in result.
    __ vmul(double_scratch0(), divisor, quotient);
    __ vcvt_s32_f64(double_scratch0().low(), double_scratch0());
    __ vmov(scratch, double_scratch0().low());
    __ sub(result_reg, left_reg, scratch, SetCC);

    // If we care about -0, test if the dividend is <0 and the result is 0.
    if (left->CanBeNegative() &&
        hmod->CanBeZero() &&
        hmod->CheckFlag(HValue::kBailoutOnMinusZero)) {
      __ b(ne, &done);
      __ cmp(left_reg, Operand::Zero());
      DeoptimizeIf(mi, instr->environment());
    }
    __ bind(&done);
  }
}


void LCodeGen::EmitSignedIntegerDivisionByConstant(
    Register result,
    Register dividend,
    int32_t divisor,
    Register remainder,
    Register scratch,
    LEnvironment* environment) {
  ASSERT(!AreAliased(dividend, scratch, ip));
  ASSERT(LChunkBuilder::HasMagicNumberForDivisor(divisor));

  uint32_t divisor_abs = abs(divisor);

  int32_t power_of_2_factor =
    CompilerIntrinsics::CountTrailingZeros(divisor_abs);

  switch (divisor_abs) {
    case 0:
      DeoptimizeIf(al, environment);
      return;

    case 1:
      if (divisor > 0) {
        __ Move(result, dividend);
      } else {
        __ rsb(result, dividend, Operand::Zero(), SetCC);
        DeoptimizeIf(vs, environment);
      }
      // Compute the remainder.
      __ mov(remainder, Operand::Zero());
      return;

    default:
      if (IsPowerOf2(divisor_abs)) {
        // Branch and condition free code for integer division by a power
        // of two.
        int32_t power = WhichPowerOf2(divisor_abs);
        if (power > 1) {
          __ mov(scratch, Operand(dividend, ASR, power - 1));
        }
        __ add(scratch, dividend, Operand(scratch, LSR, 32 - power));
        __ mov(result, Operand(scratch, ASR, power));
        // Negate if necessary.
        // We don't need to check for overflow because the case '-1' is
        // handled separately.
        if (divisor < 0) {
          ASSERT(divisor != -1);
          __ rsb(result, result, Operand::Zero());
        }
        // Compute the remainder.
        if (divisor > 0) {
          __ sub(remainder, dividend, Operand(result, LSL, power));
        } else {
          __ add(remainder, dividend, Operand(result, LSL, power));
        }
        return;
      } else {
        // Use magic numbers for a few specific divisors.
        // Details and proofs can be found in:
        // - Hacker's Delight, Henry S. Warren, Jr.
        // - The PowerPC Compiler Writer’s Guide
        // and probably many others.
        //
        // We handle
        //   <divisor with magic numbers> * <power of 2>
        // but not
        //   <divisor with magic numbers> * <other divisor with magic numbers>
        DivMagicNumbers magic_numbers =
          DivMagicNumberFor(divisor_abs >> power_of_2_factor);
        // Branch and condition free code for integer division by a power
        // of two.
        const int32_t M = magic_numbers.M;
        const int32_t s = magic_numbers.s + power_of_2_factor;

        __ mov(ip, Operand(M));
        __ smull(ip, scratch, dividend, ip);
        if (M < 0) {
          __ add(scratch, scratch, Operand(dividend));
        }
        if (s > 0) {
          __ mov(scratch, Operand(scratch, ASR, s));
        }
        __ add(result, scratch, Operand(dividend, LSR, 31));
        if (divisor < 0) __ rsb(result, result, Operand::Zero());
        // Compute the remainder.
        __ mov(ip, Operand(divisor));
        // This sequence could be replaced with 'mls' when
        // it gets implemented.
        __ mul(scratch, result, ip);
        __ sub(remainder, dividend, scratch);
      }
  }
}


void LCodeGen::DoDivI(LDivI* instr) {
  if (instr->hydrogen()->HasPowerOf2Divisor()) {
    Register dividend = ToRegister(instr->left());
    int32_t divisor = instr->hydrogen()->right()->GetInteger32Constant();
    int32_t test_value = 0;
    int32_t power = 0;

    if (divisor > 0) {
      test_value = divisor - 1;
      power = WhichPowerOf2(divisor);
    } else {
      // Check for (0 / -x) that will produce negative zero.
      if (instr->hydrogen()->CheckFlag(HValue::kBailoutOnMinusZero)) {
        __ tst(dividend, Operand(dividend));
        DeoptimizeIf(eq, instr->environment());
      }
      // Check for (kMinInt / -1).
      if (divisor == -1 && instr->hydrogen()->CheckFlag(HValue::kCanOverflow)) {
        __ cmp(dividend, Operand(kMinInt));
        DeoptimizeIf(eq, instr->environment());
      }
      test_value = - divisor - 1;
      power = WhichPowerOf2(-divisor);
    }

    if (test_value != 0) {
      if (instr->hydrogen()->CheckFlag(
          HInstruction::kAllUsesTruncatingToInt32)) {
        __ cmp(dividend, Operand(0));
        __ rsb(dividend, dividend, Operand(0), LeaveCC, lt);
        __ mov(dividend, Operand(dividend, ASR, power));
        if (divisor > 0) __ rsb(dividend, dividend, Operand(0), LeaveCC, lt);
        if (divisor < 0) __ rsb(dividend, dividend, Operand(0), LeaveCC, gt);
        return;  // Don't fall through to "__ rsb" below.
      } else {
        // Deoptimize if remainder is not 0.
        __ tst(dividend, Operand(test_value));
        DeoptimizeIf(ne, instr->environment());
        __ mov(dividend, Operand(dividend, ASR, power));
      }
    }
    if (divisor < 0) __ rsb(dividend, dividend, Operand(0));

    return;
  }

  const Register left = ToRegister(instr->left());
  const Register right = ToRegister(instr->right());
  const Register result = ToRegister(instr->result());

  // Check for x / 0.
  if (instr->hydrogen()->CheckFlag(HValue::kCanBeDivByZero)) {
    __ cmp(right, Operand::Zero());
    DeoptimizeIf(eq, instr->environment());
  }

  // Check for (0 / -x) that will produce negative zero.
  if (instr->hydrogen()->CheckFlag(HValue::kBailoutOnMinusZero)) {
    Label left_not_zero;
    __ cmp(left, Operand::Zero());
    __ b(ne, &left_not_zero);
    __ cmp(right, Operand::Zero());
    DeoptimizeIf(mi, instr->environment());
    __ bind(&left_not_zero);
  }

  // Check for (kMinInt / -1).
  if (instr->hydrogen()->CheckFlag(HValue::kCanOverflow)) {
    Label left_not_min_int;
    __ cmp(left, Operand(kMinInt));
    __ b(ne, &left_not_min_int);
    __ cmp(right, Operand(-1));
    DeoptimizeIf(eq, instr->environment());
    __ bind(&left_not_min_int);
  }

  if (CpuFeatures::IsSupported(SUDIV)) {
    CpuFeatureScope scope(masm(), SUDIV);
    __ sdiv(result, left, right);

    if (!instr->hydrogen()->CheckFlag(
        HInstruction::kAllUsesTruncatingToInt32)) {
      // Compute remainder and deopt if it's not zero.
      const Register remainder = scratch0();
      __ mls(remainder, result, right, left);
      __ cmp(remainder, Operand::Zero());
      DeoptimizeIf(ne, instr->environment());
    }
  } else {
    const DoubleRegister vleft = ToDoubleRegister(instr->temp());
    const DoubleRegister vright = double_scratch0();
    __ vmov(double_scratch0().low(), left);
    __ vcvt_f64_s32(vleft, double_scratch0().low());
    __ vmov(double_scratch0().low(), right);
    __ vcvt_f64_s32(vright, double_scratch0().low());
    __ vdiv(vleft, vleft, vright);  // vleft now contains the result.
    __ vcvt_s32_f64(double_scratch0().low(), vleft);
    __ vmov(result, double_scratch0().low());

    if (!instr->hydrogen()->CheckFlag(
        HInstruction::kAllUsesTruncatingToInt32)) {
      // Deopt if exact conversion to integer was not possible.
      // Use vright as scratch register.
      __ vcvt_f64_s32(double_scratch0(), double_scratch0().low());
      __ VFPCompareAndSetFlags(vleft, double_scratch0());
      DeoptimizeIf(ne, instr->environment());
    }
  }
}


void LCodeGen::DoMultiplyAddD(LMultiplyAddD* instr) {
  DwVfpRegister addend = ToDoubleRegister(instr->addend());
  DwVfpRegister multiplier = ToDoubleRegister(instr->multiplier());
  DwVfpRegister multiplicand = ToDoubleRegister(instr->multiplicand());

  // This is computed in-place.
  ASSERT(addend.is(ToDoubleRegister(instr->result())));

  __ vmla(addend, multiplier, multiplicand);
}


void LCodeGen::DoMultiplySubD(LMultiplySubD* instr) {
  DwVfpRegister minuend = ToDoubleRegister(instr->minuend());
  DwVfpRegister multiplier = ToDoubleRegister(instr->multiplier());
  DwVfpRegister multiplicand = ToDoubleRegister(instr->multiplicand());

  // This is computed in-place.
  ASSERT(minuend.is(ToDoubleRegister(instr->result())));

  __ vmls(minuend, multiplier, multiplicand);
}


void LCodeGen::DoMathFloorOfDiv(LMathFloorOfDiv* instr) {
  const Register result = ToRegister(instr->result());
  const Register left = ToRegister(instr->left());
  const Register remainder = ToRegister(instr->temp());
  const Register scratch = scratch0();

  if (!CpuFeatures::IsSupported(SUDIV)) {
    // If the CPU doesn't support sdiv instruction, we only optimize when we
    // have magic numbers for the divisor. The standard integer division routine
    // is usually slower than transitionning to VFP.
    ASSERT(instr->right()->IsConstantOperand());
    int32_t divisor = ToInteger32(LConstantOperand::cast(instr->right()));
    ASSERT(LChunkBuilder::HasMagicNumberForDivisor(divisor));
    if (divisor < 0) {
      __ cmp(left, Operand::Zero());
      DeoptimizeIf(eq, instr->environment());
    }
    EmitSignedIntegerDivisionByConstant(result,
                                        left,
                                        divisor,
                                        remainder,
                                        scratch,
                                        instr->environment());
    // We performed a truncating division. Correct the result if necessary.
    __ cmp(remainder, Operand::Zero());
    __ teq(remainder, Operand(divisor), ne);
    __ sub(result, result, Operand(1), LeaveCC, mi);
  } else {
    CpuFeatureScope scope(masm(), SUDIV);
    const Register right = ToRegister(instr->right());

    // Check for x / 0.
    __ cmp(right, Operand::Zero());
    DeoptimizeIf(eq, instr->environment());

    // Check for (kMinInt / -1).
    if (instr->hydrogen()->CheckFlag(HValue::kCanOverflow)) {
      Label left_not_min_int;
      __ cmp(left, Operand(kMinInt));
      __ b(ne, &left_not_min_int);
      __ cmp(right, Operand(-1));
      DeoptimizeIf(eq, instr->environment());
      __ bind(&left_not_min_int);
    }

    // Check for (0 / -x) that will produce negative zero.
    if (instr->hydrogen()->CheckFlag(HValue::kBailoutOnMinusZero)) {
      __ cmp(right, Operand::Zero());
      __ cmp(left, Operand::Zero(), mi);
      // "right" can't be null because the code would have already been
      // deoptimized. The Z flag is set only if (right < 0) and (left == 0).
      // In this case we need to deoptimize to produce a -0.
      DeoptimizeIf(eq, instr->environment());
    }

    Label done;
    __ sdiv(result, left, right);
    // If both operands have the same sign then we are done.
    __ eor(remainder, left, Operand(right), SetCC);
    __ b(pl, &done);

    // Check if the result needs to be corrected.
    __ mls(remainder, result, right, left);
    __ cmp(remainder, Operand::Zero());
    __ sub(result, result, Operand(1), LeaveCC, ne);

    __ bind(&done);
  }
}


void LCodeGen::DoMulI(LMulI* instr) {
  Register result = ToRegister(instr->result());
  // Note that result may alias left.
  Register left = ToRegister(instr->left());
  LOperand* right_op = instr->right();

  bool bailout_on_minus_zero =
    instr->hydrogen()->CheckFlag(HValue::kBailoutOnMinusZero);
  bool overflow = instr->hydrogen()->CheckFlag(HValue::kCanOverflow);

  if (right_op->IsConstantOperand()) {
    int32_t constant = ToInteger32(LConstantOperand::cast(right_op));

    if (bailout_on_minus_zero && (constant < 0)) {
      // The case of a null constant will be handled separately.
      // If constant is negative and left is null, the result should be -0.
      __ cmp(left, Operand::Zero());
      DeoptimizeIf(eq, instr->environment());
    }

    switch (constant) {
      case -1:
        if (overflow) {
          __ rsb(result, left, Operand::Zero(), SetCC);
          DeoptimizeIf(vs, instr->environment());
        } else {
          __ rsb(result, left, Operand::Zero());
        }
        break;
      case 0:
        if (bailout_on_minus_zero) {
          // If left is strictly negative and the constant is null, the
          // result is -0. Deoptimize if required, otherwise return 0.
          __ cmp(left, Operand::Zero());
          DeoptimizeIf(mi, instr->environment());
        }
        __ mov(result, Operand::Zero());
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

        if (IsPowerOf2(constant_abs)) {
          int32_t shift = WhichPowerOf2(constant_abs);
          __ mov(result, Operand(left, LSL, shift));
          // Correct the sign of the result is the constant is negative.
          if (constant < 0)  __ rsb(result, result, Operand::Zero());
        } else if (IsPowerOf2(constant_abs - 1)) {
          int32_t shift = WhichPowerOf2(constant_abs - 1);
          __ add(result, left, Operand(left, LSL, shift));
          // Correct the sign of the result is the constant is negative.
          if (constant < 0)  __ rsb(result, result, Operand::Zero());
        } else if (IsPowerOf2(constant_abs + 1)) {
          int32_t shift = WhichPowerOf2(constant_abs + 1);
          __ rsb(result, left, Operand(left, LSL, shift));
          // Correct the sign of the result is the constant is negative.
          if (constant < 0)  __ rsb(result, result, Operand::Zero());
        } else {
          // Generate standard code.
          __ mov(ip, Operand(constant));
          __ mul(result, left, ip);
        }
    }

  } else {
    ASSERT(right_op->IsRegister());
    Register right = ToRegister(right_op);

    if (overflow) {
      Register scratch = scratch0();
      // scratch:result = left * right.
      if (instr->hydrogen()->representation().IsSmi()) {
        __ SmiUntag(result, left);
        __ smull(result, scratch, result, right);
      } else {
        __ smull(result, scratch, left, right);
      }
      __ cmp(scratch, Operand(result, ASR, 31));
      DeoptimizeIf(ne, instr->environment());
    } else {
      if (instr->hydrogen()->representation().IsSmi()) {
        __ SmiUntag(result, left);
        __ mul(result, result, right);
      } else {
        __ mul(result, left, right);
      }
    }

    if (bailout_on_minus_zero) {
      Label done;
      __ teq(left, Operand(right));
      __ b(pl, &done);
      // Bail out if the result is minus zero.
      __ cmp(result, Operand::Zero());
      DeoptimizeIf(eq, instr->environment());
      __ bind(&done);
    }
  }
}


void LCodeGen::DoBitI(LBitI* instr) {
  LOperand* left_op = instr->left();
  LOperand* right_op = instr->right();
  ASSERT(left_op->IsRegister());
  Register left = ToRegister(left_op);
  Register result = ToRegister(instr->result());
  Operand right(no_reg);

  if (right_op->IsStackSlot() || right_op->IsArgument()) {
    right = Operand(EmitLoadRegister(right_op, ip));
  } else {
    ASSERT(right_op->IsRegister() || right_op->IsConstantOperand());
    right = ToOperand(right_op);
  }

  switch (instr->op()) {
    case Token::BIT_AND:
      __ and_(result, left, right);
      break;
    case Token::BIT_OR:
      __ orr(result, left, right);
      break;
    case Token::BIT_XOR:
      if (right_op->IsConstantOperand() && right.immediate() == int32_t(~0)) {
        __ mvn(result, Operand(left));
      } else {
        __ eor(result, left, right);
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
    __ and_(scratch, ToRegister(right_op), Operand(0x1F));
    switch (instr->op()) {
      case Token::ROR:
        __ mov(result, Operand(left, ROR, scratch));
        break;
      case Token::SAR:
        __ mov(result, Operand(left, ASR, scratch));
        break;
      case Token::SHR:
        if (instr->can_deopt()) {
          __ mov(result, Operand(left, LSR, scratch), SetCC);
          DeoptimizeIf(mi, instr->environment());
        } else {
          __ mov(result, Operand(left, LSR, scratch));
        }
        break;
      case Token::SHL:
        __ mov(result, Operand(left, LSL, scratch));
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
          __ mov(result, Operand(left, ROR, shift_count));
        } else {
          __ Move(result, left);
        }
        break;
      case Token::SAR:
        if (shift_count != 0) {
          __ mov(result, Operand(left, ASR, shift_count));
        } else {
          __ Move(result, left);
        }
        break;
      case Token::SHR:
        if (shift_count != 0) {
          __ mov(result, Operand(left, LSR, shift_count));
        } else {
          if (instr->can_deopt()) {
            __ tst(left, Operand(0x80000000));
            DeoptimizeIf(ne, instr->environment());
          }
          __ Move(result, left);
        }
        break;
      case Token::SHL:
        if (shift_count != 0) {
          if (instr->hydrogen_value()->representation().IsSmi() &&
              instr->can_deopt()) {
            if (shift_count != 1) {
              __ mov(result, Operand(left, LSL, shift_count - 1));
              __ SmiTag(result, result, SetCC);
            } else {
              __ SmiTag(result, left, SetCC);
            }
            DeoptimizeIf(vs, instr->environment());
          } else {
            __ mov(result, Operand(left, LSL, shift_count));
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
  bool can_overflow = instr->hydrogen()->CheckFlag(HValue::kCanOverflow);
  SBit set_cond = can_overflow ? SetCC : LeaveCC;

  if (right->IsStackSlot() || right->IsArgument()) {
    Register right_reg = EmitLoadRegister(right, ip);
    __ sub(ToRegister(result), ToRegister(left), Operand(right_reg), set_cond);
  } else {
    ASSERT(right->IsRegister() || right->IsConstantOperand());
    __ sub(ToRegister(result), ToRegister(left), ToOperand(right), set_cond);
  }

  if (can_overflow) {
    DeoptimizeIf(vs, instr->environment());
  }
}


void LCodeGen::DoRSubI(LRSubI* instr) {
  LOperand* left = instr->left();
  LOperand* right = instr->right();
  LOperand* result = instr->result();
  bool can_overflow = instr->hydrogen()->CheckFlag(HValue::kCanOverflow);
  SBit set_cond = can_overflow ? SetCC : LeaveCC;

  if (right->IsStackSlot() || right->IsArgument()) {
    Register right_reg = EmitLoadRegister(right, ip);
    __ rsb(ToRegister(result), ToRegister(left), Operand(right_reg), set_cond);
  } else {
    ASSERT(right->IsRegister() || right->IsConstantOperand());
    __ rsb(ToRegister(result), ToRegister(left), ToOperand(right), set_cond);
  }

  if (can_overflow) {
    DeoptimizeIf(vs, instr->environment());
  }
}


void LCodeGen::DoConstantI(LConstantI* instr) {
  __ mov(ToRegister(instr->result()), Operand(instr->value()));
}


void LCodeGen::DoConstantS(LConstantS* instr) {
  __ mov(ToRegister(instr->result()), Operand(instr->value()));
}


void LCodeGen::DoConstantD(LConstantD* instr) {
  ASSERT(instr->result()->IsDoubleRegister());
  DwVfpRegister result = ToDoubleRegister(instr->result());
  double v = instr->value();
  __ Vmov(result, v, scratch0());
}


void LCodeGen::DoConstantE(LConstantE* instr) {
  __ mov(ToRegister(instr->result()), Operand(instr->value()));
}


void LCodeGen::DoConstantT(LConstantT* instr) {
  Handle<Object> value = instr->value(isolate());
  AllowDeferredHandleDereference smi_check;
  __ LoadObject(ToRegister(instr->result()), value);
}


void LCodeGen::DoMapEnumLength(LMapEnumLength* instr) {
  Register result = ToRegister(instr->result());
  Register map = ToRegister(instr->value());
  __ EnumLength(result, map);
}


void LCodeGen::DoElementsKind(LElementsKind* instr) {
  Register result = ToRegister(instr->result());
  Register input = ToRegister(instr->value());

  // Load map into |result|.
  __ ldr(result, FieldMemOperand(input, HeapObject::kMapOffset));
  // Load the map's "bit field 2" into |result|. We only need the first byte,
  // but the following bit field extraction takes care of that anyway.
  __ ldr(result, FieldMemOperand(result, Map::kBitField2Offset));
  // Retrieve elements_kind from bit field 2.
  __ ubfx(result, result, Map::kElementsKindShift, Map::kElementsKindBitCount);
}


void LCodeGen::DoValueOf(LValueOf* instr) {
  Register input = ToRegister(instr->value());
  Register result = ToRegister(instr->result());
  Register map = ToRegister(instr->temp());
  Label done;

  if (!instr->hydrogen()->value()->IsHeapObject()) {
    // If the object is a smi return the object.
    __ SmiTst(input);
    __ Move(result, input, eq);
    __ b(eq, &done);
  }

  // If the object is not a value type, return the object.
  __ CompareObjectType(input, map, map, JS_VALUE_TYPE);
  __ Move(result, input, ne);
  __ b(ne, &done);
  __ ldr(result, FieldMemOperand(input, JSValue::kValueOffset));

  __ bind(&done);
}


void LCodeGen::DoDateField(LDateField* instr) {
  Register object = ToRegister(instr->date());
  Register result = ToRegister(instr->result());
  Register scratch = ToRegister(instr->temp());
  Smi* index = instr->index();
  Label runtime, done;
  ASSERT(object.is(result));
  ASSERT(object.is(r0));
  ASSERT(!scratch.is(scratch0()));
  ASSERT(!scratch.is(object));

  __ SmiTst(object);
  DeoptimizeIf(eq, instr->environment());
  __ CompareObjectType(object, scratch, scratch, JS_DATE_TYPE);
  DeoptimizeIf(ne, instr->environment());

  if (index->value() == 0) {
    __ ldr(result, FieldMemOperand(object, JSDate::kValueOffset));
  } else {
    if (index->value() < JSDate::kFirstUncachedField) {
      ExternalReference stamp = ExternalReference::date_cache_stamp(isolate());
      __ mov(scratch, Operand(stamp));
      __ ldr(scratch, MemOperand(scratch));
      __ ldr(scratch0(), FieldMemOperand(object, JSDate::kCacheStampOffset));
      __ cmp(scratch, scratch0());
      __ b(ne, &runtime);
      __ ldr(result, FieldMemOperand(object, JSDate::kValueOffset +
                                             kPointerSize * index->value()));
      __ jmp(&done);
    }
    __ bind(&runtime);
    __ PrepareCallCFunction(2, scratch);
    __ mov(r1, Operand(index));
    __ CallCFunction(ExternalReference::get_date_field_function(isolate()), 2);
    __ bind(&done);
  }
}


void LCodeGen::DoSeqStringSetChar(LSeqStringSetChar* instr) {
  Register string = ToRegister(instr->string());
  Register index = ToRegister(instr->index());
  Register value = ToRegister(instr->value());
  String::Encoding encoding = instr->encoding();

  if (FLAG_debug_code) {
    __ ldr(ip, FieldMemOperand(string, HeapObject::kMapOffset));
    __ ldrb(ip, FieldMemOperand(ip, Map::kInstanceTypeOffset));

    __ and_(ip, ip, Operand(kStringRepresentationMask | kStringEncodingMask));
    static const uint32_t one_byte_seq_type = kSeqStringTag | kOneByteStringTag;
    static const uint32_t two_byte_seq_type = kSeqStringTag | kTwoByteStringTag;
    __ cmp(ip, Operand(encoding == String::ONE_BYTE_ENCODING
                           ? one_byte_seq_type : two_byte_seq_type));
    __ Check(eq, kUnexpectedStringType);
  }

  __ add(ip,
         string,
         Operand(SeqString::kHeaderSize - kHeapObjectTag));
  if (encoding == String::ONE_BYTE_ENCODING) {
    __ strb(value, MemOperand(ip, index));
  } else {
    // MemOperand with ip as the base register is not allowed for strh, so
    // we do the address calculation explicitly.
    __ add(ip, ip, Operand(index, LSL, 1));
    __ strh(value, MemOperand(ip));
  }
}


void LCodeGen::DoThrow(LThrow* instr) {
  Register input_reg = EmitLoadRegister(instr->value(), ip);
  __ push(input_reg);
  CallRuntime(Runtime::kThrow, 1, instr);

  if (FLAG_debug_code) {
    __ stop("Unreachable code.");
  }
}


void LCodeGen::DoAddI(LAddI* instr) {
  LOperand* left = instr->left();
  LOperand* right = instr->right();
  LOperand* result = instr->result();
  bool can_overflow = instr->hydrogen()->CheckFlag(HValue::kCanOverflow);
  SBit set_cond = can_overflow ? SetCC : LeaveCC;

  if (right->IsStackSlot() || right->IsArgument()) {
    Register right_reg = EmitLoadRegister(right, ip);
    __ add(ToRegister(result), ToRegister(left), Operand(right_reg), set_cond);
  } else {
    ASSERT(right->IsRegister() || right->IsConstantOperand());
    __ add(ToRegister(result), ToRegister(left), ToOperand(right), set_cond);
  }

  if (can_overflow) {
    DeoptimizeIf(vs, instr->environment());
  }
}


void LCodeGen::DoMathMinMax(LMathMinMax* instr) {
  LOperand* left = instr->left();
  LOperand* right = instr->right();
  HMathMinMax::Operation operation = instr->hydrogen()->operation();
  if (instr->hydrogen()->representation().IsSmiOrInteger32()) {
    Condition condition = (operation == HMathMinMax::kMathMin) ? le : ge;
    Register left_reg = ToRegister(left);
    Operand right_op = (right->IsRegister() || right->IsConstantOperand())
        ? ToOperand(right)
        : Operand(EmitLoadRegister(right, ip));
    Register result_reg = ToRegister(instr->result());
    __ cmp(left_reg, right_op);
    __ Move(result_reg, left_reg, condition);
    __ mov(result_reg, right_op, LeaveCC, NegateCondition(condition));
  } else {
    ASSERT(instr->hydrogen()->representation().IsDouble());
    DwVfpRegister left_reg = ToDoubleRegister(left);
    DwVfpRegister right_reg = ToDoubleRegister(right);
    DwVfpRegister result_reg = ToDoubleRegister(instr->result());
    Label result_is_nan, return_left, return_right, check_zero, done;
    __ VFPCompareAndSetFlags(left_reg, right_reg);
    if (operation == HMathMinMax::kMathMin) {
      __ b(mi, &return_left);
      __ b(gt, &return_right);
    } else {
      __ b(mi, &return_right);
      __ b(gt, &return_left);
    }
    __ b(vs, &result_is_nan);
    // Left equals right => check for -0.
    __ VFPCompareAndSetFlags(left_reg, 0.0);
    if (left_reg.is(result_reg) || right_reg.is(result_reg)) {
      __ b(ne, &done);  // left == right != 0.
    } else {
      __ b(ne, &return_left);  // left == right != 0.
    }
    // At this point, both left and right are either 0 or -0.
    if (operation == HMathMinMax::kMathMin) {
      // We could use a single 'vorr' instruction here if we had NEON support.
      __ vneg(left_reg, left_reg);
      __ vsub(result_reg, left_reg, right_reg);
      __ vneg(result_reg, result_reg);
    } else {
      // Since we operate on +0 and/or -0, vadd and vand have the same effect;
      // the decision for vadd is easy because vand is a NEON instruction.
      __ vadd(result_reg, left_reg, right_reg);
    }
    __ b(&done);

    __ bind(&result_is_nan);
    __ vadd(result_reg, left_reg, right_reg);
    __ b(&done);

    __ bind(&return_right);
    __ Move(result_reg, right_reg);
    if (!left_reg.is(result_reg)) {
      __ b(&done);
    }

    __ bind(&return_left);
    __ Move(result_reg, left_reg);

    __ bind(&done);
  }
}


void LCodeGen::DoArithmeticD(LArithmeticD* instr) {
  DwVfpRegister left = ToDoubleRegister(instr->left());
  DwVfpRegister right = ToDoubleRegister(instr->right());
  DwVfpRegister result = ToDoubleRegister(instr->result());
  switch (instr->op()) {
    case Token::ADD:
      __ vadd(result, left, right);
      break;
    case Token::SUB:
      __ vsub(result, left, right);
      break;
    case Token::MUL:
      __ vmul(result, left, right);
      break;
    case Token::DIV:
      __ vdiv(result, left, right);
      break;
    case Token::MOD: {
      // Save r0-r3 on the stack.
      __ stm(db_w, sp, r0.bit() | r1.bit() | r2.bit() | r3.bit());

      __ PrepareCallCFunction(0, 2, scratch0());
      __ SetCallCDoubleArguments(left, right);
      __ CallCFunction(
          ExternalReference::double_fp_operation(Token::MOD, isolate()),
          0, 2);
      // Move the result in the double result register.
      __ GetCFunctionDoubleResult(result);

      // Restore r0-r3.
      __ ldm(ia_w, sp, r0.bit() | r1.bit() | r2.bit() | r3.bit());
      break;
    }
    default:
      UNREACHABLE();
      break;
  }
}


void LCodeGen::DoArithmeticT(LArithmeticT* instr) {
  ASSERT(ToRegister(instr->left()).is(r1));
  ASSERT(ToRegister(instr->right()).is(r0));
  ASSERT(ToRegister(instr->result()).is(r0));

  BinaryOpStub stub(instr->op(), NO_OVERWRITE);
  // Block literal pool emission to ensure nop indicating no inlined smi code
  // is in the correct position.
  Assembler::BlockConstPoolScope block_const_pool(masm());
  CallCode(stub.GetCode(isolate()), RelocInfo::CODE_TARGET, instr);
  __ nop();  // Signals no inlined code.
}


int LCodeGen::GetNextEmittedBlock() const {
  for (int i = current_block_ + 1; i < graph()->blocks()->length(); ++i) {
    if (!chunk_->GetLabel(i)->HasReplacement()) return i;
  }
  return -1;
}

template<class InstrType>
void LCodeGen::EmitBranch(InstrType instr, Condition condition) {
  int left_block = instr->TrueDestination(chunk_);
  int right_block = instr->FalseDestination(chunk_);

  int next_block = GetNextEmittedBlock();

  if (right_block == left_block || condition == al) {
    EmitGoto(left_block);
  } else if (left_block == next_block) {
    __ b(NegateCondition(condition), chunk_->GetAssemblyLabel(right_block));
  } else if (right_block == next_block) {
    __ b(condition, chunk_->GetAssemblyLabel(left_block));
  } else {
    __ b(condition, chunk_->GetAssemblyLabel(left_block));
    __ b(chunk_->GetAssemblyLabel(right_block));
  }
}


template<class InstrType>
void LCodeGen::EmitFalseBranch(InstrType instr, Condition condition) {
  int false_block = instr->FalseDestination(chunk_);
  __ b(condition, chunk_->GetAssemblyLabel(false_block));
}


void LCodeGen::DoDebugBreak(LDebugBreak* instr) {
  __ stop("LBreak");
}


void LCodeGen::DoIsNumberAndBranch(LIsNumberAndBranch* instr) {
  Representation r = instr->hydrogen()->value()->representation();
  if (r.IsSmiOrInteger32() || r.IsDouble()) {
    EmitBranch(instr, al);
  } else {
    ASSERT(r.IsTagged());
    Register reg = ToRegister(instr->value());
    HType type = instr->hydrogen()->value()->type();
    if (type.IsTaggedNumber()) {
      EmitBranch(instr, al);
    }
    __ JumpIfSmi(reg, instr->TrueLabel(chunk_));
    __ ldr(scratch0(), FieldMemOperand(reg, HeapObject::kMapOffset));
    __ CompareRoot(scratch0(), Heap::kHeapNumberMapRootIndex);
    EmitBranch(instr, eq);
  }
}


void LCodeGen::DoBranch(LBranch* instr) {
  Representation r = instr->hydrogen()->value()->representation();
  if (r.IsInteger32() || r.IsSmi()) {
    ASSERT(!info()->IsStub());
    Register reg = ToRegister(instr->value());
    __ cmp(reg, Operand::Zero());
    EmitBranch(instr, ne);
  } else if (r.IsDouble()) {
    ASSERT(!info()->IsStub());
    DwVfpRegister reg = ToDoubleRegister(instr->value());
    // Test the double value. Zero and NaN are false.
    __ VFPCompareAndSetFlags(reg, 0.0);
    __ cmp(r0, r0, vs);  // If NaN, set the Z flag. (NaN -> false)
    EmitBranch(instr, ne);
  } else {
    ASSERT(r.IsTagged());
    Register reg = ToRegister(instr->value());
    HType type = instr->hydrogen()->value()->type();
    if (type.IsBoolean()) {
      ASSERT(!info()->IsStub());
      __ CompareRoot(reg, Heap::kTrueValueRootIndex);
      EmitBranch(instr, eq);
    } else if (type.IsSmi()) {
      ASSERT(!info()->IsStub());
      __ cmp(reg, Operand::Zero());
      EmitBranch(instr, ne);
    } else if (type.IsJSArray()) {
      ASSERT(!info()->IsStub());
      EmitBranch(instr, al);
    } else if (type.IsHeapNumber()) {
      ASSERT(!info()->IsStub());
      DwVfpRegister dbl_scratch = double_scratch0();
      __ vldr(dbl_scratch, FieldMemOperand(reg, HeapNumber::kValueOffset));
      // Test the double value. Zero and NaN are false.
      __ VFPCompareAndSetFlags(dbl_scratch, 0.0);
      __ cmp(r0, r0, vs);  // If NaN, set the Z flag. (NaN)
      EmitBranch(instr, ne);
    } else if (type.IsString()) {
      ASSERT(!info()->IsStub());
      __ ldr(ip, FieldMemOperand(reg, String::kLengthOffset));
      __ cmp(ip, Operand::Zero());
      EmitBranch(instr, ne);
    } else {
      ToBooleanStub::Types expected = instr->hydrogen()->expected_input_types();
      // Avoid deopts in the case where we've never executed this path before.
      if (expected.IsEmpty()) expected = ToBooleanStub::Types::Generic();

      if (expected.Contains(ToBooleanStub::UNDEFINED)) {
        // undefined -> false.
        __ CompareRoot(reg, Heap::kUndefinedValueRootIndex);
        __ b(eq, instr->FalseLabel(chunk_));
      }
      if (expected.Contains(ToBooleanStub::BOOLEAN)) {
        // Boolean -> its value.
        __ CompareRoot(reg, Heap::kTrueValueRootIndex);
        __ b(eq, instr->TrueLabel(chunk_));
        __ CompareRoot(reg, Heap::kFalseValueRootIndex);
        __ b(eq, instr->FalseLabel(chunk_));
      }
      if (expected.Contains(ToBooleanStub::NULL_TYPE)) {
        // 'null' -> false.
        __ CompareRoot(reg, Heap::kNullValueRootIndex);
        __ b(eq, instr->FalseLabel(chunk_));
      }

      if (expected.Contains(ToBooleanStub::SMI)) {
        // Smis: 0 -> false, all other -> true.
        __ cmp(reg, Operand::Zero());
        __ b(eq, instr->FalseLabel(chunk_));
        __ JumpIfSmi(reg, instr->TrueLabel(chunk_));
      } else if (expected.NeedsMap()) {
        // If we need a map later and have a Smi -> deopt.
        __ SmiTst(reg);
        DeoptimizeIf(eq, instr->environment());
      }

      const Register map = scratch0();
      if (expected.NeedsMap()) {
        __ ldr(map, FieldMemOperand(reg, HeapObject::kMapOffset));

        if (expected.CanBeUndetectable()) {
          // Undetectable -> false.
          __ ldrb(ip, FieldMemOperand(map, Map::kBitFieldOffset));
          __ tst(ip, Operand(1 << Map::kIsUndetectable));
          __ b(ne, instr->FalseLabel(chunk_));
        }
      }

      if (expected.Contains(ToBooleanStub::SPEC_OBJECT)) {
        // spec object -> true.
        __ CompareInstanceType(map, ip, FIRST_SPEC_OBJECT_TYPE);
        __ b(ge, instr->TrueLabel(chunk_));
      }

      if (expected.Contains(ToBooleanStub::STRING)) {
        // String value -> false iff empty.
        Label not_string;
        __ CompareInstanceType(map, ip, FIRST_NONSTRING_TYPE);
        __ b(ge, &not_string);
        __ ldr(ip, FieldMemOperand(reg, String::kLengthOffset));
        __ cmp(ip, Operand::Zero());
        __ b(ne, instr->TrueLabel(chunk_));
        __ b(instr->FalseLabel(chunk_));
        __ bind(&not_string);
      }

      if (expected.Contains(ToBooleanStub::SYMBOL)) {
        // Symbol value -> true.
        __ CompareInstanceType(map, ip, SYMBOL_TYPE);
        __ b(eq, instr->TrueLabel(chunk_));
      }

      if (expected.Contains(ToBooleanStub::HEAP_NUMBER)) {
        // heap number -> false iff +0, -0, or NaN.
        DwVfpRegister dbl_scratch = double_scratch0();
        Label not_heap_number;
        __ CompareRoot(map, Heap::kHeapNumberMapRootIndex);
        __ b(ne, &not_heap_number);
        __ vldr(dbl_scratch, FieldMemOperand(reg, HeapNumber::kValueOffset));
        __ VFPCompareAndSetFlags(dbl_scratch, 0.0);
        __ cmp(r0, r0, vs);  // NaN -> false.
        __ b(eq, instr->FalseLabel(chunk_));  // +0, -0 -> false.
        __ b(instr->TrueLabel(chunk_));
        __ bind(&not_heap_number);
      }

      if (!expected.IsGeneric()) {
        // We've seen something for the first time -> deopt.
        // This can only happen if we are not generic already.
        DeoptimizeIf(al, instr->environment());
      }
    }
  }
}


void LCodeGen::EmitGoto(int block) {
  if (!IsNextEmittedBlock(block)) {
    __ jmp(chunk_->GetAssemblyLabel(LookupDestination(block)));
  }
}


void LCodeGen::DoGoto(LGoto* instr) {
  EmitGoto(instr->block_id());
}


Condition LCodeGen::TokenToCondition(Token::Value op, bool is_unsigned) {
  Condition cond = kNoCondition;
  switch (op) {
    case Token::EQ:
    case Token::EQ_STRICT:
      cond = eq;
      break;
    case Token::LT:
      cond = is_unsigned ? lo : lt;
      break;
    case Token::GT:
      cond = is_unsigned ? hi : gt;
      break;
    case Token::LTE:
      cond = is_unsigned ? ls : le;
      break;
    case Token::GTE:
      cond = is_unsigned ? hs : ge;
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
  Condition cond = TokenToCondition(instr->op(), false);

  if (left->IsConstantOperand() && right->IsConstantOperand()) {
    // We can statically evaluate the comparison.
    double left_val = ToDouble(LConstantOperand::cast(left));
    double right_val = ToDouble(LConstantOperand::cast(right));
    int next_block = EvalComparison(instr->op(), left_val, right_val) ?
        instr->TrueDestination(chunk_) : instr->FalseDestination(chunk_);
    EmitGoto(next_block);
  } else {
    if (instr->is_double()) {
      // Compare left and right operands as doubles and load the
      // resulting flags into the normal status register.
      __ VFPCompareAndSetFlags(ToDoubleRegister(left), ToDoubleRegister(right));
      // If a NaN is involved, i.e. the result is unordered (V set),
      // jump to false block label.
      __ b(vs, instr->FalseLabel(chunk_));
    } else {
      if (right->IsConstantOperand()) {
        int32_t value = ToInteger32(LConstantOperand::cast(right));
        if (instr->hydrogen_value()->representation().IsSmi()) {
          __ cmp(ToRegister(left), Operand(Smi::FromInt(value)));
        } else {
          __ cmp(ToRegister(left), Operand(value));
        }
      } else if (left->IsConstantOperand()) {
        int32_t value = ToInteger32(LConstantOperand::cast(left));
        if (instr->hydrogen_value()->representation().IsSmi()) {
          __ cmp(ToRegister(right), Operand(Smi::FromInt(value)));
        } else {
          __ cmp(ToRegister(right), Operand(value));
        }
        // We transposed the operands. Reverse the condition.
        cond = ReverseCondition(cond);
      } else {
        __ cmp(ToRegister(left), ToRegister(right));
      }
    }
    EmitBranch(instr, cond);
  }
}


void LCodeGen::DoCmpObjectEqAndBranch(LCmpObjectEqAndBranch* instr) {
  Register left = ToRegister(instr->left());
  Register right = ToRegister(instr->right());

  __ cmp(left, Operand(right));
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

  DwVfpRegister input_reg = ToDoubleRegister(instr->object());
  __ VFPCompareAndSetFlags(input_reg, input_reg);
  EmitFalseBranch(instr, vc);

  Register scratch = scratch0();
  __ VmovHigh(scratch, input_reg);
  __ cmp(scratch, Operand(kHoleNanUpper32));
  EmitBranch(instr, eq);
}


Condition LCodeGen::EmitIsObject(Register input,
                                 Register temp1,
                                 Label* is_not_object,
                                 Label* is_object) {
  Register temp2 = scratch0();
  __ JumpIfSmi(input, is_not_object);

  __ LoadRoot(temp2, Heap::kNullValueRootIndex);
  __ cmp(input, temp2);
  __ b(eq, is_object);

  // Load map.
  __ ldr(temp1, FieldMemOperand(input, HeapObject::kMapOffset));
  // Undetectable objects behave like undefined.
  __ ldrb(temp2, FieldMemOperand(temp1, Map::kBitFieldOffset));
  __ tst(temp2, Operand(1 << Map::kIsUndetectable));
  __ b(ne, is_not_object);

  // Load instance type and check that it is in object type range.
  __ ldrb(temp2, FieldMemOperand(temp1, Map::kInstanceTypeOffset));
  __ cmp(temp2, Operand(FIRST_NONCALLABLE_SPEC_OBJECT_TYPE));
  __ b(lt, is_not_object);
  __ cmp(temp2, Operand(LAST_NONCALLABLE_SPEC_OBJECT_TYPE));
  return le;
}


void LCodeGen::DoIsObjectAndBranch(LIsObjectAndBranch* instr) {
  Register reg = ToRegister(instr->value());
  Register temp1 = ToRegister(instr->temp());

  Condition true_cond =
      EmitIsObject(reg, temp1,
          instr->FalseLabel(chunk_), instr->TrueLabel(chunk_));

  EmitBranch(instr, true_cond);
}


Condition LCodeGen::EmitIsString(Register input,
                                 Register temp1,
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

  SmiCheck check_needed =
      instr->hydrogen()->value()->IsHeapObject()
          ? OMIT_SMI_CHECK : INLINE_SMI_CHECK;
  Condition true_cond =
      EmitIsString(reg, temp1, instr->FalseLabel(chunk_), check_needed);

  EmitBranch(instr, true_cond);
}


void LCodeGen::DoIsSmiAndBranch(LIsSmiAndBranch* instr) {
  Register input_reg = EmitLoadRegister(instr->value(), ip);
  __ SmiTst(input_reg);
  EmitBranch(instr, eq);
}


void LCodeGen::DoIsUndetectableAndBranch(LIsUndetectableAndBranch* instr) {
  Register input = ToRegister(instr->value());
  Register temp = ToRegister(instr->temp());

  if (!instr->hydrogen()->value()->IsHeapObject()) {
    __ JumpIfSmi(input, instr->FalseLabel(chunk_));
  }
  __ ldr(temp, FieldMemOperand(input, HeapObject::kMapOffset));
  __ ldrb(temp, FieldMemOperand(temp, Map::kBitFieldOffset));
  __ tst(temp, Operand(1 << Map::kIsUndetectable));
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
  Token::Value op = instr->op();

  Handle<Code> ic = CompareIC::GetUninitialized(isolate(), op);
  CallCode(ic, RelocInfo::CODE_TARGET, instr);
  // This instruction also signals no smi code inlined.
  __ cmp(r0, Operand::Zero());

  Condition condition = ComputeCompareCondition(op);

  EmitBranch(instr, condition);
}


static InstanceType TestType(HHasInstanceTypeAndBranch* instr) {
  InstanceType from = instr->from();
  InstanceType to = instr->to();
  if (from == FIRST_TYPE) return to;
  ASSERT(from == to || to == LAST_TYPE);
  return from;
}


static Condition BranchCondition(HHasInstanceTypeAndBranch* instr) {
  InstanceType from = instr->from();
  InstanceType to = instr->to();
  if (from == to) return eq;
  if (to == LAST_TYPE) return hs;
  if (from == FIRST_TYPE) return ls;
  UNREACHABLE();
  return eq;
}


void LCodeGen::DoHasInstanceTypeAndBranch(LHasInstanceTypeAndBranch* instr) {
  Register scratch = scratch0();
  Register input = ToRegister(instr->value());

  if (!instr->hydrogen()->value()->IsHeapObject()) {
    __ JumpIfSmi(input, instr->FalseLabel(chunk_));
  }

  __ CompareObjectType(input, scratch, scratch, TestType(instr->hydrogen()));
  EmitBranch(instr, BranchCondition(instr->hydrogen()));
}


void LCodeGen::DoGetCachedArrayIndex(LGetCachedArrayIndex* instr) {
  Register input = ToRegister(instr->value());
  Register result = ToRegister(instr->result());

  __ AssertString(input);

  __ ldr(result, FieldMemOperand(input, String::kHashFieldOffset));
  __ IndexFromHash(result, result);
}


void LCodeGen::DoHasCachedArrayIndexAndBranch(
    LHasCachedArrayIndexAndBranch* instr) {
  Register input = ToRegister(instr->value());
  Register scratch = scratch0();

  __ ldr(scratch,
         FieldMemOperand(input, String::kHashFieldOffset));
  __ tst(scratch, Operand(String::kContainsCachedArrayIndexMask));
  EmitBranch(instr, eq);
}


// Branches to a label or falls through with the answer in flags.  Trashes
// the temp registers, but not the input.
void LCodeGen::EmitClassOfTest(Label* is_true,
                               Label* is_false,
                               Handle<String>class_name,
                               Register input,
                               Register temp,
                               Register temp2) {
  ASSERT(!input.is(temp));
  ASSERT(!input.is(temp2));
  ASSERT(!temp.is(temp2));

  __ JumpIfSmi(input, is_false);

  if (class_name->IsOneByteEqualTo(STATIC_ASCII_VECTOR("Function"))) {
    // Assuming the following assertions, we can use the same compares to test
    // for both being a function type and being in the object type range.
    STATIC_ASSERT(NUM_OF_CALLABLE_SPEC_OBJECT_TYPES == 2);
    STATIC_ASSERT(FIRST_NONCALLABLE_SPEC_OBJECT_TYPE ==
                  FIRST_SPEC_OBJECT_TYPE + 1);
    STATIC_ASSERT(LAST_NONCALLABLE_SPEC_OBJECT_TYPE ==
                  LAST_SPEC_OBJECT_TYPE - 1);
    STATIC_ASSERT(LAST_SPEC_OBJECT_TYPE == LAST_TYPE);
    __ CompareObjectType(input, temp, temp2, FIRST_SPEC_OBJECT_TYPE);
    __ b(lt, is_false);
    __ b(eq, is_true);
    __ cmp(temp2, Operand(LAST_SPEC_OBJECT_TYPE));
    __ b(eq, is_true);
  } else {
    // Faster code path to avoid two compares: subtract lower bound from the
    // actual type and do a signed compare with the width of the type range.
    __ ldr(temp, FieldMemOperand(input, HeapObject::kMapOffset));
    __ ldrb(temp2, FieldMemOperand(temp, Map::kInstanceTypeOffset));
    __ sub(temp2, temp2, Operand(FIRST_NONCALLABLE_SPEC_OBJECT_TYPE));
    __ cmp(temp2, Operand(LAST_NONCALLABLE_SPEC_OBJECT_TYPE -
                          FIRST_NONCALLABLE_SPEC_OBJECT_TYPE));
    __ b(gt, is_false);
  }

  // Now we are in the FIRST-LAST_NONCALLABLE_SPEC_OBJECT_TYPE range.
  // Check if the constructor in the map is a function.
  __ ldr(temp, FieldMemOperand(temp, Map::kConstructorOffset));

  // Objects with a non-function constructor have class 'Object'.
  __ CompareObjectType(temp, temp2, temp2, JS_FUNCTION_TYPE);
  if (class_name->IsOneByteEqualTo(STATIC_ASCII_VECTOR("Object"))) {
    __ b(ne, is_true);
  } else {
    __ b(ne, is_false);
  }

  // temp now contains the constructor function. Grab the
  // instance class name from there.
  __ ldr(temp, FieldMemOperand(temp, JSFunction::kSharedFunctionInfoOffset));
  __ ldr(temp, FieldMemOperand(temp,
                               SharedFunctionInfo::kInstanceClassNameOffset));
  // The class name we are testing against is internalized since it's a literal.
  // The name in the constructor is internalized because of the way the context
  // is booted.  This routine isn't expected to work for random API-created
  // classes and it doesn't have to because you can't access it with natives
  // syntax.  Since both sides are internalized it is sufficient to use an
  // identity comparison.
  __ cmp(temp, Operand(class_name));
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

  __ ldr(temp, FieldMemOperand(reg, HeapObject::kMapOffset));
  __ cmp(temp, Operand(instr->map()));
  EmitBranch(instr, eq);
}


void LCodeGen::DoInstanceOf(LInstanceOf* instr) {
  ASSERT(ToRegister(instr->left()).is(r0));  // Object is in r0.
  ASSERT(ToRegister(instr->right()).is(r1));  // Function is in r1.

  InstanceofStub stub(InstanceofStub::kArgsInRegisters);
  CallCode(stub.GetCode(isolate()), RelocInfo::CODE_TARGET, instr);

  __ cmp(r0, Operand::Zero());
  __ mov(r0, Operand(factory()->false_value()), LeaveCC, ne);
  __ mov(r0, Operand(factory()->true_value()), LeaveCC, eq);
}


void LCodeGen::DoInstanceOfKnownGlobal(LInstanceOfKnownGlobal* instr) {
  class DeferredInstanceOfKnownGlobal V8_FINAL : public LDeferredCode {
   public:
    DeferredInstanceOfKnownGlobal(LCodeGen* codegen,
                                  LInstanceOfKnownGlobal* instr)
        : LDeferredCode(codegen), instr_(instr) { }
    virtual void Generate() V8_OVERRIDE {
      codegen()->DoDeferredInstanceOfKnownGlobal(instr_, &map_check_);
    }
    virtual LInstruction* instr() V8_OVERRIDE { return instr_; }
    Label* map_check() { return &map_check_; }
   private:
    LInstanceOfKnownGlobal* instr_;
    Label map_check_;
  };

  DeferredInstanceOfKnownGlobal* deferred;
  deferred = new(zone()) DeferredInstanceOfKnownGlobal(this, instr);

  Label done, false_result;
  Register object = ToRegister(instr->value());
  Register temp = ToRegister(instr->temp());
  Register result = ToRegister(instr->result());

  ASSERT(object.is(r0));
  ASSERT(result.is(r0));

  // A Smi is not instance of anything.
  __ JumpIfSmi(object, &false_result);

  // This is the inlined call site instanceof cache. The two occurences of the
  // hole value will be patched to the last map/result pair generated by the
  // instanceof stub.
  Label cache_miss;
  Register map = temp;
  __ ldr(map, FieldMemOperand(object, HeapObject::kMapOffset));
  {
    // Block constant pool emission to ensure the positions of instructions are
    // as expected by the patcher. See InstanceofStub::Generate().
    Assembler::BlockConstPoolScope block_const_pool(masm());
    __ bind(deferred->map_check());  // Label for calculating code patching.
    // We use Factory::the_hole_value() on purpose instead of loading from the
    // root array to force relocation to be able to later patch with
    // the cached map.
    PredictableCodeSizeScope predictable(masm_, 5 * Assembler::kInstrSize);
    Handle<Cell> cell = factory()->NewCell(factory()->the_hole_value());
    __ mov(ip, Operand(Handle<Object>(cell)));
    __ ldr(ip, FieldMemOperand(ip, PropertyCell::kValueOffset));
    __ cmp(map, Operand(ip));
    __ b(ne, &cache_miss);
    // We use Factory::the_hole_value() on purpose instead of loading from the
    // root array to force relocation to be able to later patch
    // with true or false.
    __ mov(result, Operand(factory()->the_hole_value()));
  }
  __ b(&done);

  // The inlined call site cache did not match. Check null and string before
  // calling the deferred code.
  __ bind(&cache_miss);
  // Null is not instance of anything.
  __ LoadRoot(ip, Heap::kNullValueRootIndex);
  __ cmp(object, Operand(ip));
  __ b(eq, &false_result);

  // String values is not instance of anything.
  Condition is_string = masm_->IsObjectStringType(object, temp);
  __ b(is_string, &false_result);

  // Go to the deferred code.
  __ b(deferred->entry());

  __ bind(&false_result);
  __ LoadRoot(result, Heap::kFalseValueRootIndex);

  // Here result has either true or false. Deferred code also produces true or
  // false object.
  __ bind(deferred->exit());
  __ bind(&done);
}


void LCodeGen::DoDeferredInstanceOfKnownGlobal(LInstanceOfKnownGlobal* instr,
                                               Label* map_check) {
  Register result = ToRegister(instr->result());
  ASSERT(result.is(r0));

  InstanceofStub::Flags flags = InstanceofStub::kNoFlags;
  flags = static_cast<InstanceofStub::Flags>(
      flags | InstanceofStub::kArgsInRegisters);
  flags = static_cast<InstanceofStub::Flags>(
      flags | InstanceofStub::kCallSiteInlineCheck);
  flags = static_cast<InstanceofStub::Flags>(
      flags | InstanceofStub::kReturnTrueFalseObject);
  InstanceofStub stub(flags);

  PushSafepointRegistersScope scope(this, Safepoint::kWithRegisters);

  // Get the temp register reserved by the instruction. This needs to be r4 as
  // its slot of the pushing of safepoint registers is used to communicate the
  // offset to the location of the map check.
  Register temp = ToRegister(instr->temp());
  ASSERT(temp.is(r4));
  __ LoadHeapObject(InstanceofStub::right(), instr->function());
  static const int kAdditionalDelta = 5;
  // Make sure that code size is predicable, since we use specific constants
  // offsets in the code to find embedded values..
  PredictableCodeSizeScope predictable(masm_, 6 * Assembler::kInstrSize);
  int delta = masm_->InstructionsGeneratedSince(map_check) + kAdditionalDelta;
  Label before_push_delta;
  __ bind(&before_push_delta);
  __ BlockConstPoolFor(kAdditionalDelta);
  __ mov(temp, Operand(delta * kPointerSize));
  // The mov above can generate one or two instructions. The delta was computed
  // for two instructions, so we need to pad here in case of one instruction.
  if (masm_->InstructionsGeneratedSince(&before_push_delta) != 2) {
    ASSERT_EQ(1, masm_->InstructionsGeneratedSince(&before_push_delta));
    __ nop();
  }
  __ StoreToSafepointRegisterSlot(temp, temp);
  CallCodeGeneric(stub.GetCode(isolate()),
                  RelocInfo::CODE_TARGET,
                  instr,
                  RECORD_SAFEPOINT_WITH_REGISTERS_AND_NO_ARGUMENTS);
  LEnvironment* env = instr->GetDeferredLazyDeoptimizationEnvironment();
  safepoints_.RecordLazyDeoptimizationIndex(env->deoptimization_index());
  // Put the result value into the result register slot and
  // restore all registers.
  __ StoreToSafepointRegisterSlot(result, result);
}


void LCodeGen::DoInstanceSize(LInstanceSize* instr) {
  Register object = ToRegister(instr->object());
  Register result = ToRegister(instr->result());
  __ ldr(result, FieldMemOperand(object, HeapObject::kMapOffset));
  __ ldrb(result, FieldMemOperand(result, Map::kInstanceSizeOffset));
}


void LCodeGen::DoCmpT(LCmpT* instr) {
  Token::Value op = instr->op();

  Handle<Code> ic = CompareIC::GetUninitialized(isolate(), op);
  CallCode(ic, RelocInfo::CODE_TARGET, instr);
  // This instruction also signals no smi code inlined.
  __ cmp(r0, Operand::Zero());

  Condition condition = ComputeCompareCondition(op);
  __ LoadRoot(ToRegister(instr->result()),
              Heap::kTrueValueRootIndex,
              condition);
  __ LoadRoot(ToRegister(instr->result()),
              Heap::kFalseValueRootIndex,
              NegateCondition(condition));
}


void LCodeGen::DoReturn(LReturn* instr) {
  if (FLAG_trace && info()->IsOptimizing()) {
    // Push the return value on the stack as the parameter.
    // Runtime::TraceExit returns its parameter in r0.
    __ push(r0);
    __ CallRuntime(Runtime::kTraceExit, 1);
  }
  if (info()->saves_caller_doubles()) {
    ASSERT(NeedsEagerFrame());
    BitVector* doubles = chunk()->allocated_double_registers();
    BitVector::Iterator save_iterator(doubles);
    int count = 0;
    while (!save_iterator.Done()) {
      __ vldr(DwVfpRegister::FromAllocationIndex(save_iterator.Current()),
               MemOperand(sp, count * kDoubleSize));
      save_iterator.Advance();
      count++;
    }
  }
  int no_frame_start = -1;
  if (NeedsEagerFrame()) {
    __ mov(sp, fp);
    no_frame_start = masm_->pc_offset();
    __ ldm(ia_w, sp, fp.bit() | lr.bit());
  }
  if (instr->has_constant_parameter_count()) {
    int parameter_count = ToInteger32(instr->constant_parameter_count());
    int32_t sp_delta = (parameter_count + 1) * kPointerSize;
    if (sp_delta != 0) {
      __ add(sp, sp, Operand(sp_delta));
    }
  } else {
    Register reg = ToRegister(instr->parameter_count());
    // The argument count parameter is a smi
    __ SmiUntag(reg);
    __ add(sp, sp, Operand(reg, LSL, kPointerSizeLog2));
  }

  __ Jump(lr);

  if (no_frame_start != -1) {
    info_->AddNoFrameRange(no_frame_start, masm_->pc_offset());
  }
}


void LCodeGen::DoLoadGlobalCell(LLoadGlobalCell* instr) {
  Register result = ToRegister(instr->result());
  __ mov(ip, Operand(Handle<Object>(instr->hydrogen()->cell())));
  __ ldr(result, FieldMemOperand(ip, Cell::kValueOffset));
  if (instr->hydrogen()->RequiresHoleCheck()) {
    __ LoadRoot(ip, Heap::kTheHoleValueRootIndex);
    __ cmp(result, ip);
    DeoptimizeIf(eq, instr->environment());
  }
}


void LCodeGen::DoLoadGlobalGeneric(LLoadGlobalGeneric* instr) {
  ASSERT(ToRegister(instr->global_object()).is(r0));
  ASSERT(ToRegister(instr->result()).is(r0));

  __ mov(r2, Operand(instr->name()));
  RelocInfo::Mode mode = instr->for_typeof() ? RelocInfo::CODE_TARGET
                                             : RelocInfo::CODE_TARGET_CONTEXT;
  Handle<Code> ic = isolate()->builtins()->LoadIC_Initialize();
  CallCode(ic, mode, instr);
}


void LCodeGen::DoStoreGlobalCell(LStoreGlobalCell* instr) {
  Register value = ToRegister(instr->value());
  Register cell = scratch0();

  // Load the cell.
  __ mov(cell, Operand(instr->hydrogen()->cell()));

  // If the cell we are storing to contains the hole it could have
  // been deleted from the property dictionary. In that case, we need
  // to update the property details in the property dictionary to mark
  // it as no longer deleted.
  if (instr->hydrogen()->RequiresHoleCheck()) {
    // We use a temp to check the payload (CompareRoot might clobber ip).
    Register payload = ToRegister(instr->temp());
    __ ldr(payload, FieldMemOperand(cell, Cell::kValueOffset));
    __ CompareRoot(payload, Heap::kTheHoleValueRootIndex);
    DeoptimizeIf(eq, instr->environment());
  }

  // Store the value.
  __ str(value, FieldMemOperand(cell, Cell::kValueOffset));
  // Cells are always rescanned, so no write barrier here.
}


void LCodeGen::DoStoreGlobalGeneric(LStoreGlobalGeneric* instr) {
  ASSERT(ToRegister(instr->global_object()).is(r1));
  ASSERT(ToRegister(instr->value()).is(r0));

  __ mov(r2, Operand(instr->name()));
  Handle<Code> ic = (instr->strict_mode_flag() == kStrictMode)
      ? isolate()->builtins()->StoreIC_Initialize_Strict()
      : isolate()->builtins()->StoreIC_Initialize();
  CallCode(ic, RelocInfo::CODE_TARGET_CONTEXT, instr);
}


void LCodeGen::DoLoadContextSlot(LLoadContextSlot* instr) {
  Register context = ToRegister(instr->context());
  Register result = ToRegister(instr->result());
  __ ldr(result, ContextOperand(context, instr->slot_index()));
  if (instr->hydrogen()->RequiresHoleCheck()) {
    __ LoadRoot(ip, Heap::kTheHoleValueRootIndex);
    __ cmp(result, ip);
    if (instr->hydrogen()->DeoptimizesOnHole()) {
      DeoptimizeIf(eq, instr->environment());
    } else {
      __ mov(result, Operand(factory()->undefined_value()), LeaveCC, eq);
    }
  }
}


void LCodeGen::DoStoreContextSlot(LStoreContextSlot* instr) {
  Register context = ToRegister(instr->context());
  Register value = ToRegister(instr->value());
  Register scratch = scratch0();
  MemOperand target = ContextOperand(context, instr->slot_index());

  Label skip_assignment;

  if (instr->hydrogen()->RequiresHoleCheck()) {
    __ ldr(scratch, target);
    __ LoadRoot(ip, Heap::kTheHoleValueRootIndex);
    __ cmp(scratch, ip);
    if (instr->hydrogen()->DeoptimizesOnHole()) {
      DeoptimizeIf(eq, instr->environment());
    } else {
      __ b(ne, &skip_assignment);
    }
  }

  __ str(value, target);
  if (instr->hydrogen()->NeedsWriteBarrier()) {
    SmiCheck check_needed =
        instr->hydrogen()->value()->IsHeapObject()
            ? OMIT_SMI_CHECK : INLINE_SMI_CHECK;
    __ RecordWriteContextSlot(context,
                              target.offset(),
                              value,
                              scratch,
                              GetLinkRegisterState(),
                              kSaveFPRegs,
                              EMIT_REMEMBERED_SET,
                              check_needed);
  }

  __ bind(&skip_assignment);
}


void LCodeGen::DoLoadNamedField(LLoadNamedField* instr) {
  HObjectAccess access = instr->hydrogen()->access();
  int offset = access.offset();
  Register object = ToRegister(instr->object());

  if (access.IsExternalMemory()) {
    Register result = ToRegister(instr->result());
    __ ldr(result, MemOperand(object, offset));
    return;
  }

  if (instr->hydrogen()->representation().IsDouble()) {
    DwVfpRegister result = ToDoubleRegister(instr->result());
    __ vldr(result, FieldMemOperand(object, offset));
    return;
  }

  Register result = ToRegister(instr->result());
  if (access.IsInobject()) {
    __ ldr(result, FieldMemOperand(object, offset));
  } else {
    __ ldr(result, FieldMemOperand(object, JSObject::kPropertiesOffset));
    __ ldr(result, FieldMemOperand(result, offset));
  }
}


void LCodeGen::DoLoadNamedGeneric(LLoadNamedGeneric* instr) {
  ASSERT(ToRegister(instr->object()).is(r0));
  ASSERT(ToRegister(instr->result()).is(r0));

  // Name is always in r2.
  __ mov(r2, Operand(instr->name()));
  Handle<Code> ic = isolate()->builtins()->LoadIC_Initialize();
  CallCode(ic, RelocInfo::CODE_TARGET, instr, NEVER_INLINE_TARGET_ADDRESS);
}


void LCodeGen::DoLoadFunctionPrototype(LLoadFunctionPrototype* instr) {
  Register scratch = scratch0();
  Register function = ToRegister(instr->function());
  Register result = ToRegister(instr->result());

  // Check that the function really is a function. Load map into the
  // result register.
  __ CompareObjectType(function, result, scratch, JS_FUNCTION_TYPE);
  DeoptimizeIf(ne, instr->environment());

  // Make sure that the function has an instance prototype.
  Label non_instance;
  __ ldrb(scratch, FieldMemOperand(result, Map::kBitFieldOffset));
  __ tst(scratch, Operand(1 << Map::kHasNonInstancePrototype));
  __ b(ne, &non_instance);

  // Get the prototype or initial map from the function.
  __ ldr(result,
         FieldMemOperand(function, JSFunction::kPrototypeOrInitialMapOffset));

  // Check that the function has a prototype or an initial map.
  __ LoadRoot(ip, Heap::kTheHoleValueRootIndex);
  __ cmp(result, ip);
  DeoptimizeIf(eq, instr->environment());

  // If the function does not have an initial map, we're done.
  Label done;
  __ CompareObjectType(result, scratch, scratch, MAP_TYPE);
  __ b(ne, &done);

  // Get the prototype from the initial map.
  __ ldr(result, FieldMemOperand(result, Map::kPrototypeOffset));
  __ jmp(&done);

  // Non-instance prototype: Fetch prototype from constructor field
  // in initial map.
  __ bind(&non_instance);
  __ ldr(result, FieldMemOperand(result, Map::kConstructorOffset));

  // All done.
  __ bind(&done);
}


void LCodeGen::DoLoadExternalArrayPointer(
    LLoadExternalArrayPointer* instr) {
  Register to_reg = ToRegister(instr->result());
  Register from_reg  = ToRegister(instr->object());
  __ ldr(to_reg, FieldMemOperand(from_reg,
                                 ExternalArray::kExternalPointerOffset));
}


void LCodeGen::DoAccessArgumentsAt(LAccessArgumentsAt* instr) {
  Register arguments = ToRegister(instr->arguments());
  Register result = ToRegister(instr->result());
  if (instr->length()->IsConstantOperand() &&
      instr->index()->IsConstantOperand()) {
    int const_index = ToInteger32(LConstantOperand::cast(instr->index()));
    int const_length = ToInteger32(LConstantOperand::cast(instr->length()));
    int index = (const_length - const_index) + 1;
    __ ldr(result, MemOperand(arguments, index * kPointerSize));
  } else {
    Register length = ToRegister(instr->length());
    Register index = ToRegister(instr->index());
    // There are two words between the frame pointer and the last argument.
    // Subtracting from length accounts for one of them add one more.
    __ sub(length, length, index);
    __ add(length, length, Operand(1));
    __ ldr(result, MemOperand(arguments, length, LSL, kPointerSizeLog2));
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
  int shift_size = (instr->hydrogen()->key()->representation().IsSmi())
      ? (element_size_shift - kSmiTagSize) : element_size_shift;
  int additional_offset = instr->additional_index() << element_size_shift;

  if (elements_kind == EXTERNAL_FLOAT_ELEMENTS ||
      elements_kind == EXTERNAL_DOUBLE_ELEMENTS) {
    DwVfpRegister result = ToDoubleRegister(instr->result());
    Operand operand = key_is_constant
        ? Operand(constant_key << element_size_shift)
        : Operand(key, LSL, shift_size);
    __ add(scratch0(), external_pointer, operand);
    if (elements_kind == EXTERNAL_FLOAT_ELEMENTS) {
      __ vldr(double_scratch0().low(), scratch0(), additional_offset);
      __ vcvt_f64_f32(result, double_scratch0().low());
    } else  {  // i.e. elements_kind == EXTERNAL_DOUBLE_ELEMENTS
      __ vldr(result, scratch0(), additional_offset);
    }
  } else {
    Register result = ToRegister(instr->result());
    MemOperand mem_operand = PrepareKeyedOperand(
        key, external_pointer, key_is_constant, constant_key,
        element_size_shift, shift_size,
        instr->additional_index(), additional_offset);
    switch (elements_kind) {
      case EXTERNAL_BYTE_ELEMENTS:
        __ ldrsb(result, mem_operand);
        break;
      case EXTERNAL_PIXEL_ELEMENTS:
      case EXTERNAL_UNSIGNED_BYTE_ELEMENTS:
        __ ldrb(result, mem_operand);
        break;
      case EXTERNAL_SHORT_ELEMENTS:
        __ ldrsh(result, mem_operand);
        break;
      case EXTERNAL_UNSIGNED_SHORT_ELEMENTS:
        __ ldrh(result, mem_operand);
        break;
      case EXTERNAL_INT_ELEMENTS:
        __ ldr(result, mem_operand);
        break;
      case EXTERNAL_UNSIGNED_INT_ELEMENTS:
        __ ldr(result, mem_operand);
        if (!instr->hydrogen()->CheckFlag(HInstruction::kUint32)) {
          __ cmp(result, Operand(0x80000000));
          DeoptimizeIf(cs, instr->environment());
        }
        break;
      case EXTERNAL_FLOAT_ELEMENTS:
      case EXTERNAL_DOUBLE_ELEMENTS:
      case FAST_HOLEY_DOUBLE_ELEMENTS:
      case FAST_HOLEY_ELEMENTS:
      case FAST_HOLEY_SMI_ELEMENTS:
      case FAST_DOUBLE_ELEMENTS:
      case FAST_ELEMENTS:
      case FAST_SMI_ELEMENTS:
      case DICTIONARY_ELEMENTS:
      case NON_STRICT_ARGUMENTS_ELEMENTS:
        UNREACHABLE();
        break;
    }
  }
}


void LCodeGen::DoLoadKeyedFixedDoubleArray(LLoadKeyed* instr) {
  Register elements = ToRegister(instr->elements());
  bool key_is_constant = instr->key()->IsConstantOperand();
  Register key = no_reg;
  DwVfpRegister result = ToDoubleRegister(instr->result());
  Register scratch = scratch0();

  int element_size_shift = ElementsKindToShiftSize(FAST_DOUBLE_ELEMENTS);
  int shift_size = (instr->hydrogen()->key()->representation().IsSmi())
      ? (element_size_shift - kSmiTagSize) : element_size_shift;
  int constant_key = 0;
  if (key_is_constant) {
    constant_key = ToInteger32(LConstantOperand::cast(instr->key()));
    if (constant_key & 0xF0000000) {
      Abort(kArrayIndexConstantValueTooBig);
    }
  } else {
    key = ToRegister(instr->key());
  }

  int base_offset = (FixedDoubleArray::kHeaderSize - kHeapObjectTag) +
      ((constant_key + instr->additional_index()) << element_size_shift);
  if (!key_is_constant) {
    __ add(elements, elements, Operand(key, LSL, shift_size));
  }
  __ add(elements, elements, Operand(base_offset));
  __ vldr(result, elements, 0);
  if (instr->hydrogen()->RequiresHoleCheck()) {
    __ ldr(scratch, MemOperand(elements, sizeof(kHoleNanLower32)));
    __ cmp(scratch, Operand(kHoleNanUpper32));
    DeoptimizeIf(eq, instr->environment());
  }
}


void LCodeGen::DoLoadKeyedFixedArray(LLoadKeyed* instr) {
  Register elements = ToRegister(instr->elements());
  Register result = ToRegister(instr->result());
  Register scratch = scratch0();
  Register store_base = scratch;
  int offset = 0;

  if (instr->key()->IsConstantOperand()) {
    LConstantOperand* const_operand = LConstantOperand::cast(instr->key());
    offset = FixedArray::OffsetOfElementAt(ToInteger32(const_operand) +
                                           instr->additional_index());
    store_base = elements;
  } else {
    Register key = EmitLoadRegister(instr->key(), scratch0());
    // Even though the HLoadKeyed instruction forces the input
    // representation for the key to be an integer, the input gets replaced
    // during bound check elimination with the index argument to the bounds
    // check, which can be tagged, so that case must be handled here, too.
    if (instr->hydrogen()->key()->representation().IsSmi()) {
      __ add(scratch, elements, Operand::PointerOffsetFromSmiKey(key));
    } else {
      __ add(scratch, elements, Operand(key, LSL, kPointerSizeLog2));
    }
    offset = FixedArray::OffsetOfElementAt(instr->additional_index());
  }
  __ ldr(result, FieldMemOperand(store_base, offset));

  // Check for the hole value.
  if (instr->hydrogen()->RequiresHoleCheck()) {
    if (IsFastSmiElementsKind(instr->hydrogen()->elements_kind())) {
      __ SmiTst(result);
      DeoptimizeIf(ne, instr->environment());
    } else {
      __ LoadRoot(scratch, Heap::kTheHoleValueRootIndex);
      __ cmp(result, scratch);
      DeoptimizeIf(eq, instr->environment());
    }
  }
}


void LCodeGen::DoLoadKeyed(LLoadKeyed* instr) {
  if (instr->is_external()) {
    DoLoadKeyedExternalArray(instr);
  } else if (instr->hydrogen()->representation().IsDouble()) {
    DoLoadKeyedFixedDoubleArray(instr);
  } else {
    DoLoadKeyedFixedArray(instr);
  }
}


MemOperand LCodeGen::PrepareKeyedOperand(Register key,
                                         Register base,
                                         bool key_is_constant,
                                         int constant_key,
                                         int element_size,
                                         int shift_size,
                                         int additional_index,
                                         int additional_offset) {
  if (additional_index != 0 && !key_is_constant) {
    additional_index *= 1 << (element_size - shift_size);
    __ add(scratch0(), key, Operand(additional_index));
  }

  if (key_is_constant) {
    return MemOperand(base,
                      (constant_key << element_size) + additional_offset);
  }

  if (additional_index == 0) {
    if (shift_size >= 0) {
      return MemOperand(base, key, LSL, shift_size);
    } else {
      ASSERT_EQ(-1, shift_size);
      return MemOperand(base, key, LSR, 1);
    }
  }

  if (shift_size >= 0) {
    return MemOperand(base, scratch0(), LSL, shift_size);
  } else {
    ASSERT_EQ(-1, shift_size);
    return MemOperand(base, scratch0(), LSR, 1);
  }
}


void LCodeGen::DoLoadKeyedGeneric(LLoadKeyedGeneric* instr) {
  ASSERT(ToRegister(instr->object()).is(r1));
  ASSERT(ToRegister(instr->key()).is(r0));

  Handle<Code> ic = isolate()->builtins()->KeyedLoadIC_Initialize();
  CallCode(ic, RelocInfo::CODE_TARGET, instr, NEVER_INLINE_TARGET_ADDRESS);
}


void LCodeGen::DoArgumentsElements(LArgumentsElements* instr) {
  Register scratch = scratch0();
  Register result = ToRegister(instr->result());

  if (instr->hydrogen()->from_inlined()) {
    __ sub(result, sp, Operand(2 * kPointerSize));
  } else {
    // Check if the calling frame is an arguments adaptor frame.
    Label done, adapted;
    __ ldr(scratch, MemOperand(fp, StandardFrameConstants::kCallerFPOffset));
    __ ldr(result, MemOperand(scratch, StandardFrameConstants::kContextOffset));
    __ cmp(result, Operand(Smi::FromInt(StackFrame::ARGUMENTS_ADAPTOR)));

    // Result is the frame pointer for the frame if not adapted and for the real
    // frame below the adaptor frame if adapted.
    __ mov(result, fp, LeaveCC, ne);
    __ mov(result, scratch, LeaveCC, eq);
  }
}


void LCodeGen::DoArgumentsLength(LArgumentsLength* instr) {
  Register elem = ToRegister(instr->elements());
  Register result = ToRegister(instr->result());

  Label done;

  // If no arguments adaptor frame the number of arguments is fixed.
  __ cmp(fp, elem);
  __ mov(result, Operand(scope()->num_parameters()));
  __ b(eq, &done);

  // Arguments adaptor frame present. Get argument length from there.
  __ ldr(result, MemOperand(fp, StandardFrameConstants::kCallerFPOffset));
  __ ldr(result,
         MemOperand(result, ArgumentsAdaptorFrameConstants::kLengthOffset));
  __ SmiUntag(result);

  // Argument length is in result register.
  __ bind(&done);
}


void LCodeGen::DoWrapReceiver(LWrapReceiver* instr) {
  Register receiver = ToRegister(instr->receiver());
  Register function = ToRegister(instr->function());
  Register scratch = scratch0();

  // If the receiver is null or undefined, we have to pass the global
  // object as a receiver to normal functions. Values have to be
  // passed unchanged to builtins and strict-mode functions.
  Label global_object, receiver_ok;

  // Do not transform the receiver to object for strict mode
  // functions.
  __ ldr(scratch,
         FieldMemOperand(function, JSFunction::kSharedFunctionInfoOffset));
  __ ldr(scratch,
         FieldMemOperand(scratch, SharedFunctionInfo::kCompilerHintsOffset));
  __ tst(scratch,
         Operand(1 << (SharedFunctionInfo::kStrictModeFunction + kSmiTagSize)));
  __ b(ne, &receiver_ok);

  // Do not transform the receiver to object for builtins.
  __ tst(scratch, Operand(1 << (SharedFunctionInfo::kNative + kSmiTagSize)));
  __ b(ne, &receiver_ok);

  // Normal function. Replace undefined or null with global receiver.
  __ LoadRoot(scratch, Heap::kNullValueRootIndex);
  __ cmp(receiver, scratch);
  __ b(eq, &global_object);
  __ LoadRoot(scratch, Heap::kUndefinedValueRootIndex);
  __ cmp(receiver, scratch);
  __ b(eq, &global_object);

  // Deoptimize if the receiver is not a JS object.
  __ SmiTst(receiver);
  DeoptimizeIf(eq, instr->environment());
  __ CompareObjectType(receiver, scratch, scratch, FIRST_SPEC_OBJECT_TYPE);
  DeoptimizeIf(lt, instr->environment());
  __ jmp(&receiver_ok);

  __ bind(&global_object);
  __ ldr(receiver, GlobalObjectOperand());
  __ ldr(receiver,
         FieldMemOperand(receiver, JSGlobalObject::kGlobalReceiverOffset));
  __ bind(&receiver_ok);
}


void LCodeGen::DoApplyArguments(LApplyArguments* instr) {
  Register receiver = ToRegister(instr->receiver());
  Register function = ToRegister(instr->function());
  Register length = ToRegister(instr->length());
  Register elements = ToRegister(instr->elements());
  Register scratch = scratch0();
  ASSERT(receiver.is(r0));  // Used for parameter count.
  ASSERT(function.is(r1));  // Required by InvokeFunction.
  ASSERT(ToRegister(instr->result()).is(r0));

  // Copy the arguments to this function possibly from the
  // adaptor frame below it.
  const uint32_t kArgumentsLimit = 1 * KB;
  __ cmp(length, Operand(kArgumentsLimit));
  DeoptimizeIf(hi, instr->environment());

  // Push the receiver and use the register to keep the original
  // number of arguments.
  __ push(receiver);
  __ mov(receiver, length);
  // The arguments are at a one pointer size offset from elements.
  __ add(elements, elements, Operand(1 * kPointerSize));

  // Loop through the arguments pushing them onto the execution
  // stack.
  Label invoke, loop;
  // length is a small non-negative integer, due to the test above.
  __ cmp(length, Operand::Zero());
  __ b(eq, &invoke);
  __ bind(&loop);
  __ ldr(scratch, MemOperand(elements, length, LSL, 2));
  __ push(scratch);
  __ sub(length, length, Operand(1), SetCC);
  __ b(ne, &loop);

  __ bind(&invoke);
  ASSERT(instr->HasPointerMap());
  LPointerMap* pointers = instr->pointer_map();
  RecordPosition(pointers->position());
  SafepointGenerator safepoint_generator(
      this, pointers, Safepoint::kLazyDeopt);
  // The number of arguments is stored in receiver which is r0, as expected
  // by InvokeFunction.
  ParameterCount actual(receiver);
  __ InvokeFunction(function, actual, CALL_FUNCTION,
                    safepoint_generator, CALL_AS_METHOD);
  __ ldr(cp, MemOperand(fp, StandardFrameConstants::kContextOffset));
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


void LCodeGen::DoDrop(LDrop* instr) {
  __ Drop(instr->count());
}


void LCodeGen::DoThisFunction(LThisFunction* instr) {
  Register result = ToRegister(instr->result());
  __ ldr(result, MemOperand(fp, JavaScriptFrameConstants::kFunctionOffset));
}


void LCodeGen::DoContext(LContext* instr) {
  // If there is a non-return use, the context must be moved to a register.
  Register result = ToRegister(instr->result());
  for (HUseIterator it(instr->hydrogen()->uses()); !it.Done(); it.Advance()) {
    if (!it.value()->IsReturn()) {
      __ mov(result, cp);
      return;
    }
  }
}


void LCodeGen::DoOuterContext(LOuterContext* instr) {
  Register context = ToRegister(instr->context());
  Register result = ToRegister(instr->result());
  __ ldr(result,
         MemOperand(context, Context::SlotOffset(Context::PREVIOUS_INDEX)));
}


void LCodeGen::DoDeclareGlobals(LDeclareGlobals* instr) {
  __ push(cp);  // The context is the first argument.
  __ LoadHeapObject(scratch0(), instr->hydrogen()->pairs());
  __ push(scratch0());
  __ mov(scratch0(), Operand(Smi::FromInt(instr->hydrogen()->flags())));
  __ push(scratch0());
  CallRuntime(Runtime::kDeclareGlobals, 3, instr);
}


void LCodeGen::DoGlobalObject(LGlobalObject* instr) {
  Register result = ToRegister(instr->result());
  __ ldr(result, ContextOperand(cp, Context::GLOBAL_OBJECT_INDEX));
}


void LCodeGen::DoGlobalReceiver(LGlobalReceiver* instr) {
  Register global = ToRegister(instr->global_object());
  Register result = ToRegister(instr->result());
  __ ldr(result, FieldMemOperand(global, GlobalObject::kGlobalReceiverOffset));
}


void LCodeGen::CallKnownFunction(Handle<JSFunction> function,
                                 int formal_parameter_count,
                                 int arity,
                                 LInstruction* instr,
                                 CallKind call_kind,
                                 R1State r1_state) {
  bool dont_adapt_arguments =
      formal_parameter_count == SharedFunctionInfo::kDontAdaptArgumentsSentinel;
  bool can_invoke_directly =
      dont_adapt_arguments || formal_parameter_count == arity;

  LPointerMap* pointers = instr->pointer_map();
  RecordPosition(pointers->position());

  if (can_invoke_directly) {
    if (r1_state == R1_UNINITIALIZED) {
      __ LoadHeapObject(r1, function);
    }

    // Change context.
    __ ldr(cp, FieldMemOperand(r1, JSFunction::kContextOffset));

    // Set r0 to arguments count if adaption is not needed. Assumes that r0
    // is available to write to at this point.
    if (dont_adapt_arguments) {
      __ mov(r0, Operand(arity));
    }

    // Invoke function.
    __ SetCallKind(r5, call_kind);
    __ ldr(ip, FieldMemOperand(r1, JSFunction::kCodeEntryOffset));
    __ Call(ip);

    // Set up deoptimization.
    RecordSafepointWithLazyDeopt(instr, RECORD_SIMPLE_SAFEPOINT);
  } else {
    SafepointGenerator generator(this, pointers, Safepoint::kLazyDeopt);
    ParameterCount count(arity);
    ParameterCount expected(formal_parameter_count);
    __ InvokeFunction(
        function, expected, count, CALL_FUNCTION, generator, call_kind);
  }

  // Restore context.
  __ ldr(cp, MemOperand(fp, StandardFrameConstants::kContextOffset));
}


void LCodeGen::DoCallConstantFunction(LCallConstantFunction* instr) {
  ASSERT(ToRegister(instr->result()).is(r0));
  CallKnownFunction(instr->hydrogen()->function(),
                    instr->hydrogen()->formal_parameter_count(),
                    instr->arity(),
                    instr,
                    CALL_AS_METHOD,
                    R1_UNINITIALIZED);
}


void LCodeGen::DoDeferredMathAbsTaggedHeapNumber(LMathAbs* instr) {
  Register input = ToRegister(instr->value());
  Register result = ToRegister(instr->result());
  Register scratch = scratch0();

  // Deoptimize if not a heap number.
  __ ldr(scratch, FieldMemOperand(input, HeapObject::kMapOffset));
  __ LoadRoot(ip, Heap::kHeapNumberMapRootIndex);
  __ cmp(scratch, Operand(ip));
  DeoptimizeIf(ne, instr->environment());

  Label done;
  Register exponent = scratch0();
  scratch = no_reg;
  __ ldr(exponent, FieldMemOperand(input, HeapNumber::kExponentOffset));
  // Check the sign of the argument. If the argument is positive, just
  // return it.
  __ tst(exponent, Operand(HeapNumber::kSignMask));
  // Move the input to the result if necessary.
  __ Move(result, input);
  __ b(eq, &done);

  // Input is negative. Reverse its sign.
  // Preserve the value of all registers.
  {
    PushSafepointRegistersScope scope(this, Safepoint::kWithRegisters);

    // Registers were saved at the safepoint, so we can use
    // many scratch registers.
    Register tmp1 = input.is(r1) ? r0 : r1;
    Register tmp2 = input.is(r2) ? r0 : r2;
    Register tmp3 = input.is(r3) ? r0 : r3;
    Register tmp4 = input.is(r4) ? r0 : r4;

    // exponent: floating point exponent value.

    Label allocated, slow;
    __ LoadRoot(tmp4, Heap::kHeapNumberMapRootIndex);
    __ AllocateHeapNumber(tmp1, tmp2, tmp3, tmp4, &slow);
    __ b(&allocated);

    // Slow case: Call the runtime system to do the number allocation.
    __ bind(&slow);

    CallRuntimeFromDeferred(Runtime::kAllocateHeapNumber, 0, instr);
    // Set the pointer to the new heap number in tmp.
    if (!tmp1.is(r0)) __ mov(tmp1, Operand(r0));
    // Restore input_reg after call to runtime.
    __ LoadFromSafepointRegisterSlot(input, input);
    __ ldr(exponent, FieldMemOperand(input, HeapNumber::kExponentOffset));

    __ bind(&allocated);
    // exponent: floating point exponent value.
    // tmp1: allocated heap number.
    __ bic(exponent, exponent, Operand(HeapNumber::kSignMask));
    __ str(exponent, FieldMemOperand(tmp1, HeapNumber::kExponentOffset));
    __ ldr(tmp2, FieldMemOperand(input, HeapNumber::kMantissaOffset));
    __ str(tmp2, FieldMemOperand(tmp1, HeapNumber::kMantissaOffset));

    __ StoreToSafepointRegisterSlot(tmp1, result);
  }

  __ bind(&done);
}


void LCodeGen::EmitIntegerMathAbs(LMathAbs* instr) {
  Register input = ToRegister(instr->value());
  Register result = ToRegister(instr->result());
  __ cmp(input, Operand::Zero());
  __ Move(result, input, pl);
  // We can make rsb conditional because the previous cmp instruction
  // will clear the V (overflow) flag and rsb won't set this flag
  // if input is positive.
  __ rsb(result, input, Operand::Zero(), SetCC, mi);
  // Deoptimize on overflow.
  DeoptimizeIf(vs, instr->environment());
}


void LCodeGen::DoMathAbs(LMathAbs* instr) {
  // Class for deferred case.
  class DeferredMathAbsTaggedHeapNumber V8_FINAL : public LDeferredCode {
   public:
    DeferredMathAbsTaggedHeapNumber(LCodeGen* codegen, LMathAbs* instr)
        : LDeferredCode(codegen), instr_(instr) { }
    virtual void Generate() V8_OVERRIDE {
      codegen()->DoDeferredMathAbsTaggedHeapNumber(instr_);
    }
    virtual LInstruction* instr() V8_OVERRIDE { return instr_; }
   private:
    LMathAbs* instr_;
  };

  Representation r = instr->hydrogen()->value()->representation();
  if (r.IsDouble()) {
    DwVfpRegister input = ToDoubleRegister(instr->value());
    DwVfpRegister result = ToDoubleRegister(instr->result());
    __ vabs(result, input);
  } else if (r.IsSmiOrInteger32()) {
    EmitIntegerMathAbs(instr);
  } else {
    // Representation is tagged.
    DeferredMathAbsTaggedHeapNumber* deferred =
        new(zone()) DeferredMathAbsTaggedHeapNumber(this, instr);
    Register input = ToRegister(instr->value());
    // Smi check.
    __ JumpIfNotSmi(input, deferred->entry());
    // If smi, handle it directly.
    EmitIntegerMathAbs(instr);
    __ bind(deferred->exit());
  }
}


void LCodeGen::DoMathFloor(LMathFloor* instr) {
  DwVfpRegister input = ToDoubleRegister(instr->value());
  Register result = ToRegister(instr->result());
  Register input_high = scratch0();
  Label done, exact;

  __ TryInt32Floor(result, input, input_high, double_scratch0(), &done, &exact);
  DeoptimizeIf(al, instr->environment());

  __ bind(&exact);
  if (instr->hydrogen()->CheckFlag(HValue::kBailoutOnMinusZero)) {
    // Test for -0.
    __ cmp(result, Operand::Zero());
    __ b(ne, &done);
    __ cmp(input_high, Operand::Zero());
    DeoptimizeIf(mi, instr->environment());
  }
  __ bind(&done);
}


void LCodeGen::DoMathRound(LMathRound* instr) {
  DwVfpRegister input = ToDoubleRegister(instr->value());
  Register result = ToRegister(instr->result());
  DwVfpRegister double_scratch1 = ToDoubleRegister(instr->temp());
  DwVfpRegister input_plus_dot_five = double_scratch1;
  Register input_high = scratch0();
  DwVfpRegister dot_five = double_scratch0();
  Label convert, done;

  __ Vmov(dot_five, 0.5, scratch0());
  __ vabs(double_scratch1, input);
  __ VFPCompareAndSetFlags(double_scratch1, dot_five);
  // If input is in [-0.5, -0], the result is -0.
  // If input is in [+0, +0.5[, the result is +0.
  // If the input is +0.5, the result is 1.
  __ b(hi, &convert);  // Out of [-0.5, +0.5].
  if (instr->hydrogen()->CheckFlag(HValue::kBailoutOnMinusZero)) {
    __ VmovHigh(input_high, input);
    __ cmp(input_high, Operand::Zero());
    DeoptimizeIf(mi, instr->environment());  // [-0.5, -0].
  }
  __ VFPCompareAndSetFlags(input, dot_five);
  __ mov(result, Operand(1), LeaveCC, eq);  // +0.5.
  // Remaining cases: [+0, +0.5[ or [-0.5, +0.5[, depending on
  // flag kBailoutOnMinusZero.
  __ mov(result, Operand::Zero(), LeaveCC, ne);
  __ b(&done);

  __ bind(&convert);
  __ vadd(input_plus_dot_five, input, dot_five);
  // Reuse dot_five (double_scratch0) as we no longer need this value.
  __ TryInt32Floor(result, input_plus_dot_five, input_high, double_scratch0(),
                   &done, &done);
  DeoptimizeIf(al, instr->environment());
  __ bind(&done);
}


void LCodeGen::DoMathSqrt(LMathSqrt* instr) {
  DwVfpRegister input = ToDoubleRegister(instr->value());
  DwVfpRegister result = ToDoubleRegister(instr->result());
  __ vsqrt(result, input);
}


void LCodeGen::DoMathPowHalf(LMathPowHalf* instr) {
  DwVfpRegister input = ToDoubleRegister(instr->value());
  DwVfpRegister result = ToDoubleRegister(instr->result());
  DwVfpRegister temp = ToDoubleRegister(instr->temp());

  // Note that according to ECMA-262 15.8.2.13:
  // Math.pow(-Infinity, 0.5) == Infinity
  // Math.sqrt(-Infinity) == NaN
  Label done;
  __ vmov(temp, -V8_INFINITY, scratch0());
  __ VFPCompareAndSetFlags(input, temp);
  __ vneg(result, temp, eq);
  __ b(&done, eq);

  // Add +0 to convert -0 to +0.
  __ vadd(result, input, kDoubleRegZero);
  __ vsqrt(result, result);
  __ bind(&done);
}


void LCodeGen::DoPower(LPower* instr) {
  Representation exponent_type = instr->hydrogen()->right()->representation();
  // Having marked this as a call, we can use any registers.
  // Just make sure that the input/output registers are the expected ones.
  ASSERT(!instr->right()->IsDoubleRegister() ||
         ToDoubleRegister(instr->right()).is(d2));
  ASSERT(!instr->right()->IsRegister() ||
         ToRegister(instr->right()).is(r2));
  ASSERT(ToDoubleRegister(instr->left()).is(d1));
  ASSERT(ToDoubleRegister(instr->result()).is(d3));

  if (exponent_type.IsSmi()) {
    MathPowStub stub(MathPowStub::TAGGED);
    __ CallStub(&stub);
  } else if (exponent_type.IsTagged()) {
    Label no_deopt;
    __ JumpIfSmi(r2, &no_deopt);
    __ ldr(r7, FieldMemOperand(r2, HeapObject::kMapOffset));
    __ LoadRoot(ip, Heap::kHeapNumberMapRootIndex);
    __ cmp(r7, Operand(ip));
    DeoptimizeIf(ne, instr->environment());
    __ bind(&no_deopt);
    MathPowStub stub(MathPowStub::TAGGED);
    __ CallStub(&stub);
  } else if (exponent_type.IsInteger32()) {
    MathPowStub stub(MathPowStub::INTEGER);
    __ CallStub(&stub);
  } else {
    ASSERT(exponent_type.IsDouble());
    MathPowStub stub(MathPowStub::DOUBLE);
    __ CallStub(&stub);
  }
}


void LCodeGen::DoRandom(LRandom* instr) {
  // Assert that the register size is indeed the size of each seed.
  static const int kSeedSize = sizeof(uint32_t);
  STATIC_ASSERT(kPointerSize == kSeedSize);

  // Load native context
  Register global_object = ToRegister(instr->global_object());
  Register native_context = global_object;
  __ ldr(native_context, FieldMemOperand(
          global_object, GlobalObject::kNativeContextOffset));

  // Load state (FixedArray of the native context's random seeds)
  static const int kRandomSeedOffset =
      FixedArray::kHeaderSize + Context::RANDOM_SEED_INDEX * kPointerSize;
  Register state = native_context;
  __ ldr(state, FieldMemOperand(native_context, kRandomSeedOffset));

  // Load state[0].
  Register state0 = ToRegister(instr->scratch());
  __ ldr(state0, FieldMemOperand(state, ByteArray::kHeaderSize));
  // Load state[1].
  Register state1 = ToRegister(instr->scratch2());
  __ ldr(state1, FieldMemOperand(state, ByteArray::kHeaderSize + kSeedSize));

  // state[0] = 18273 * (state[0] & 0xFFFF) + (state[0] >> 16)
  Register scratch3 = ToRegister(instr->scratch3());
  Register scratch4 = scratch0();
  __ and_(scratch3, state0, Operand(0xFFFF));
  __ mov(scratch4, Operand(18273));
  __ mul(scratch3, scratch3, scratch4);
  __ add(state0, scratch3, Operand(state0, LSR, 16));
  // Save state[0].
  __ str(state0, FieldMemOperand(state, ByteArray::kHeaderSize));

  // state[1] = 36969 * (state[1] & 0xFFFF) + (state[1] >> 16)
  __ and_(scratch3, state1, Operand(0xFFFF));
  __ mov(scratch4, Operand(36969));
  __ mul(scratch3, scratch3, scratch4);
  __ add(state1, scratch3, Operand(state1, LSR, 16));
  // Save state[1].
  __ str(state1, FieldMemOperand(state, ByteArray::kHeaderSize + kSeedSize));

  // Random bit pattern = (state[0] << 14) + (state[1] & 0x3FFFF)
  Register random = scratch4;
  __ and_(random, state1, Operand(0x3FFFF));
  __ add(random, random, Operand(state0, LSL, 14));

  // 0x41300000 is the top half of 1.0 x 2^20 as a double.
  // Create this constant using mov/orr to avoid PC relative load.
  __ mov(scratch3, Operand(0x41000000));
  __ orr(scratch3, scratch3, Operand(0x300000));
  // Move 0x41300000xxxxxxxx (x = random bits) to VFP.
  DwVfpRegister result = ToDoubleRegister(instr->result());
  __ vmov(result, random, scratch3);
  // Move 0x4130000000000000 to VFP.
  __ mov(scratch4, Operand::Zero());
  DwVfpRegister scratch5 = double_scratch0();
  __ vmov(scratch5, scratch4, scratch3);
  __ vsub(result, result, scratch5);
}


void LCodeGen::DoMathExp(LMathExp* instr) {
  DwVfpRegister input = ToDoubleRegister(instr->value());
  DwVfpRegister result = ToDoubleRegister(instr->result());
  DwVfpRegister double_scratch1 = ToDoubleRegister(instr->double_temp());
  DwVfpRegister double_scratch2 = double_scratch0();
  Register temp1 = ToRegister(instr->temp1());
  Register temp2 = ToRegister(instr->temp2());

  MathExpGenerator::EmitMathExp(
      masm(), input, result, double_scratch1, double_scratch2,
      temp1, temp2, scratch0());
}


void LCodeGen::DoMathLog(LMathLog* instr) {
  ASSERT(ToDoubleRegister(instr->result()).is(d2));
  TranscendentalCacheStub stub(TranscendentalCache::LOG,
                               TranscendentalCacheStub::UNTAGGED);
  CallCode(stub.GetCode(isolate()), RelocInfo::CODE_TARGET, instr);
}


void LCodeGen::DoMathTan(LMathTan* instr) {
  ASSERT(ToDoubleRegister(instr->result()).is(d2));
  TranscendentalCacheStub stub(TranscendentalCache::TAN,
                               TranscendentalCacheStub::UNTAGGED);
  CallCode(stub.GetCode(isolate()), RelocInfo::CODE_TARGET, instr);
}


void LCodeGen::DoMathCos(LMathCos* instr) {
  ASSERT(ToDoubleRegister(instr->result()).is(d2));
  TranscendentalCacheStub stub(TranscendentalCache::COS,
                               TranscendentalCacheStub::UNTAGGED);
  CallCode(stub.GetCode(isolate()), RelocInfo::CODE_TARGET, instr);
}


void LCodeGen::DoMathSin(LMathSin* instr) {
  ASSERT(ToDoubleRegister(instr->result()).is(d2));
  TranscendentalCacheStub stub(TranscendentalCache::SIN,
                               TranscendentalCacheStub::UNTAGGED);
  CallCode(stub.GetCode(isolate()), RelocInfo::CODE_TARGET, instr);
}


void LCodeGen::DoInvokeFunction(LInvokeFunction* instr) {
  ASSERT(ToRegister(instr->function()).is(r1));
  ASSERT(instr->HasPointerMap());

  Handle<JSFunction> known_function = instr->hydrogen()->known_function();
  if (known_function.is_null()) {
    LPointerMap* pointers = instr->pointer_map();
    RecordPosition(pointers->position());
    SafepointGenerator generator(this, pointers, Safepoint::kLazyDeopt);
    ParameterCount count(instr->arity());
    __ InvokeFunction(r1, count, CALL_FUNCTION, generator, CALL_AS_METHOD);
    __ ldr(cp, MemOperand(fp, StandardFrameConstants::kContextOffset));
  } else {
    CallKnownFunction(known_function,
                      instr->hydrogen()->formal_parameter_count(),
                      instr->arity(),
                      instr,
                      CALL_AS_METHOD,
                      R1_CONTAINS_TARGET);
  }
}


void LCodeGen::DoCallKeyed(LCallKeyed* instr) {
  ASSERT(ToRegister(instr->result()).is(r0));

  int arity = instr->arity();
  Handle<Code> ic =
      isolate()->stub_cache()->ComputeKeyedCallInitialize(arity);
  CallCode(ic, RelocInfo::CODE_TARGET, instr, NEVER_INLINE_TARGET_ADDRESS);
  __ ldr(cp, MemOperand(fp, StandardFrameConstants::kContextOffset));
}


void LCodeGen::DoCallNamed(LCallNamed* instr) {
  ASSERT(ToRegister(instr->result()).is(r0));

  int arity = instr->arity();
  RelocInfo::Mode mode = RelocInfo::CODE_TARGET;
  Handle<Code> ic =
      isolate()->stub_cache()->ComputeCallInitialize(arity, mode);
  __ mov(r2, Operand(instr->name()));
  CallCode(ic, mode, instr, NEVER_INLINE_TARGET_ADDRESS);
  // Restore context register.
  __ ldr(cp, MemOperand(fp, StandardFrameConstants::kContextOffset));
}


void LCodeGen::DoCallFunction(LCallFunction* instr) {
  ASSERT(ToRegister(instr->function()).is(r1));
  ASSERT(ToRegister(instr->result()).is(r0));

  int arity = instr->arity();
  CallFunctionStub stub(arity, NO_CALL_FUNCTION_FLAGS);
  CallCode(stub.GetCode(isolate()), RelocInfo::CODE_TARGET, instr);
  __ ldr(cp, MemOperand(fp, StandardFrameConstants::kContextOffset));
}


void LCodeGen::DoCallGlobal(LCallGlobal* instr) {
  ASSERT(ToRegister(instr->result()).is(r0));

  int arity = instr->arity();
  RelocInfo::Mode mode = RelocInfo::CODE_TARGET_CONTEXT;
  Handle<Code> ic =
      isolate()->stub_cache()->ComputeCallInitialize(arity, mode);
  __ mov(r2, Operand(instr->name()));
  CallCode(ic, mode, instr, NEVER_INLINE_TARGET_ADDRESS);
  __ ldr(cp, MemOperand(fp, StandardFrameConstants::kContextOffset));
}


void LCodeGen::DoCallKnownGlobal(LCallKnownGlobal* instr) {
  ASSERT(ToRegister(instr->result()).is(r0));
  CallKnownFunction(instr->hydrogen()->target(),
                    instr->hydrogen()->formal_parameter_count(),
                    instr->arity(),
                    instr,
                    CALL_AS_FUNCTION,
                    R1_UNINITIALIZED);
}


void LCodeGen::DoCallNew(LCallNew* instr) {
  ASSERT(ToRegister(instr->constructor()).is(r1));
  ASSERT(ToRegister(instr->result()).is(r0));

  __ mov(r0, Operand(instr->arity()));
  // No cell in r2 for construct type feedback in optimized code
  Handle<Object> undefined_value(isolate()->factory()->undefined_value());
  __ mov(r2, Operand(undefined_value));
  CallConstructStub stub(NO_CALL_FUNCTION_FLAGS);
  CallCode(stub.GetCode(isolate()), RelocInfo::CONSTRUCT_CALL, instr);
}


void LCodeGen::DoCallNewArray(LCallNewArray* instr) {
  ASSERT(ToRegister(instr->constructor()).is(r1));
  ASSERT(ToRegister(instr->result()).is(r0));

  __ mov(r0, Operand(instr->arity()));
  __ mov(r2, Operand(instr->hydrogen()->property_cell()));
  ElementsKind kind = instr->hydrogen()->elements_kind();
  AllocationSiteOverrideMode override_mode =
      (AllocationSite::GetMode(kind) == TRACK_ALLOCATION_SITE)
          ? DISABLE_ALLOCATION_SITES
          : DONT_OVERRIDE;
  ContextCheckMode context_mode = CONTEXT_CHECK_NOT_REQUIRED;

  if (instr->arity() == 0) {
    ArrayNoArgumentConstructorStub stub(kind, context_mode, override_mode);
    CallCode(stub.GetCode(isolate()), RelocInfo::CONSTRUCT_CALL, instr);
  } else if (instr->arity() == 1) {
    Label done;
    if (IsFastPackedElementsKind(kind)) {
      Label packed_case;
      // We might need a change here
      // look at the first argument
      __ ldr(r5, MemOperand(sp, 0));
      __ cmp(r5, Operand::Zero());
      __ b(eq, &packed_case);

      ElementsKind holey_kind = GetHoleyElementsKind(kind);
      ArraySingleArgumentConstructorStub stub(holey_kind, context_mode,
                                              override_mode);
      CallCode(stub.GetCode(isolate()), RelocInfo::CONSTRUCT_CALL, instr);
      __ jmp(&done);
      __ bind(&packed_case);
    }

    ArraySingleArgumentConstructorStub stub(kind, context_mode, override_mode);
    CallCode(stub.GetCode(isolate()), RelocInfo::CONSTRUCT_CALL, instr);
    __ bind(&done);
  } else {
    ArrayNArgumentsConstructorStub stub(kind, context_mode, override_mode);
    CallCode(stub.GetCode(isolate()), RelocInfo::CONSTRUCT_CALL, instr);
  }
}


void LCodeGen::DoCallRuntime(LCallRuntime* instr) {
  CallRuntime(instr->function(), instr->arity(), instr);
}


void LCodeGen::DoStoreCodeEntry(LStoreCodeEntry* instr) {
  Register function = ToRegister(instr->function());
  Register code_object = ToRegister(instr->code_object());
  __ add(code_object, code_object, Operand(Code::kHeaderSize - kHeapObjectTag));
  __ str(code_object,
         FieldMemOperand(function, JSFunction::kCodeEntryOffset));
}


void LCodeGen::DoInnerAllocatedObject(LInnerAllocatedObject* instr) {
  Register result = ToRegister(instr->result());
  Register base = ToRegister(instr->base_object());
  __ add(result, base, Operand(instr->offset()));
}


void LCodeGen::DoStoreNamedField(LStoreNamedField* instr) {
  Representation representation = instr->representation();

  Register object = ToRegister(instr->object());
  Register scratch = scratch0();
  HObjectAccess access = instr->hydrogen()->access();
  int offset = access.offset();

  if (access.IsExternalMemory()) {
    Register value = ToRegister(instr->value());
    __ str(value, MemOperand(object, offset));
    return;
  }

  Handle<Map> transition = instr->transition();

  if (FLAG_track_heap_object_fields && representation.IsHeapObject()) {
    Register value = ToRegister(instr->value());
    if (!instr->hydrogen()->value()->type().IsHeapObject()) {
      __ SmiTst(value);
      DeoptimizeIf(eq, instr->environment());
    }
  } else if (FLAG_track_double_fields && representation.IsDouble()) {
    ASSERT(transition.is_null());
    ASSERT(access.IsInobject());
    ASSERT(!instr->hydrogen()->NeedsWriteBarrier());
    DwVfpRegister value = ToDoubleRegister(instr->value());
    __ vstr(value, FieldMemOperand(object, offset));
    return;
  }

  if (!transition.is_null()) {
    __ mov(scratch, Operand(transition));
    __ str(scratch, FieldMemOperand(object, HeapObject::kMapOffset));
    if (instr->hydrogen()->NeedsWriteBarrierForMap()) {
      Register temp = ToRegister(instr->temp());
      // Update the write barrier for the map field.
      __ RecordWriteField(object,
                          HeapObject::kMapOffset,
                          scratch,
                          temp,
                          GetLinkRegisterState(),
                          kSaveFPRegs,
                          OMIT_REMEMBERED_SET,
                          OMIT_SMI_CHECK);
    }
  }

  // Do the store.
  Register value = ToRegister(instr->value());
  ASSERT(!object.is(value));
  SmiCheck check_needed =
      instr->hydrogen()->value()->IsHeapObject()
          ? OMIT_SMI_CHECK : INLINE_SMI_CHECK;
  if (access.IsInobject()) {
    __ str(value, FieldMemOperand(object, offset));
    if (instr->hydrogen()->NeedsWriteBarrier()) {
      // Update the write barrier for the object for in-object properties.
      __ RecordWriteField(object,
                          offset,
                          value,
                          scratch,
                          GetLinkRegisterState(),
                          kSaveFPRegs,
                          EMIT_REMEMBERED_SET,
                          check_needed);
    }
  } else {
    __ ldr(scratch, FieldMemOperand(object, JSObject::kPropertiesOffset));
    __ str(value, FieldMemOperand(scratch, offset));
    if (instr->hydrogen()->NeedsWriteBarrier()) {
      // Update the write barrier for the properties array.
      // object is used as a scratch register.
      __ RecordWriteField(scratch,
                          offset,
                          value,
                          object,
                          GetLinkRegisterState(),
                          kSaveFPRegs,
                          EMIT_REMEMBERED_SET,
                          check_needed);
    }
  }
}


void LCodeGen::DoStoreNamedGeneric(LStoreNamedGeneric* instr) {
  ASSERT(ToRegister(instr->object()).is(r1));
  ASSERT(ToRegister(instr->value()).is(r0));

  // Name is always in r2.
  __ mov(r2, Operand(instr->name()));
  Handle<Code> ic = (instr->strict_mode_flag() == kStrictMode)
      ? isolate()->builtins()->StoreIC_Initialize_Strict()
      : isolate()->builtins()->StoreIC_Initialize();
  CallCode(ic, RelocInfo::CODE_TARGET, instr, NEVER_INLINE_TARGET_ADDRESS);
}


void LCodeGen::ApplyCheckIf(Condition condition, LBoundsCheck* check) {
  if (FLAG_debug_code && check->hydrogen()->skip_check()) {
    Label done;
    __ b(NegateCondition(condition), &done);
    __ stop("eliminated bounds check failed");
    __ bind(&done);
  } else {
    DeoptimizeIf(condition, check->environment());
  }
}


void LCodeGen::DoBoundsCheck(LBoundsCheck* instr) {
  if (instr->hydrogen()->skip_check()) return;

  if (instr->index()->IsConstantOperand()) {
    int constant_index =
        ToInteger32(LConstantOperand::cast(instr->index()));
    if (instr->hydrogen()->length()->representation().IsSmi()) {
      __ mov(ip, Operand(Smi::FromInt(constant_index)));
    } else {
      __ mov(ip, Operand(constant_index));
    }
    __ cmp(ip, ToRegister(instr->length()));
  } else {
    __ cmp(ToRegister(instr->index()), ToRegister(instr->length()));
  }
  Condition condition = instr->hydrogen()->allow_equality() ? hi : hs;
  ApplyCheckIf(condition, instr);
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
  int shift_size = (instr->hydrogen()->key()->representation().IsSmi())
      ? (element_size_shift - kSmiTagSize) : element_size_shift;
  int additional_offset = instr->additional_index() << element_size_shift;

  if (elements_kind == EXTERNAL_FLOAT_ELEMENTS ||
      elements_kind == EXTERNAL_DOUBLE_ELEMENTS) {
    DwVfpRegister value(ToDoubleRegister(instr->value()));
    Operand operand(key_is_constant
                    ? Operand(constant_key << element_size_shift)
                    : Operand(key, LSL, shift_size));
    __ add(scratch0(), external_pointer, operand);
    if (elements_kind == EXTERNAL_FLOAT_ELEMENTS) {
      __ vcvt_f32_f64(double_scratch0().low(), value);
      __ vstr(double_scratch0().low(), scratch0(), additional_offset);
    } else {  // i.e. elements_kind == EXTERNAL_DOUBLE_ELEMENTS
      __ vstr(value, scratch0(), additional_offset);
    }
  } else {
    Register value(ToRegister(instr->value()));
    MemOperand mem_operand = PrepareKeyedOperand(
        key, external_pointer, key_is_constant, constant_key,
        element_size_shift, shift_size,
        instr->additional_index(), additional_offset);
    switch (elements_kind) {
      case EXTERNAL_PIXEL_ELEMENTS:
      case EXTERNAL_BYTE_ELEMENTS:
      case EXTERNAL_UNSIGNED_BYTE_ELEMENTS:
        __ strb(value, mem_operand);
        break;
      case EXTERNAL_SHORT_ELEMENTS:
      case EXTERNAL_UNSIGNED_SHORT_ELEMENTS:
        __ strh(value, mem_operand);
        break;
      case EXTERNAL_INT_ELEMENTS:
      case EXTERNAL_UNSIGNED_INT_ELEMENTS:
        __ str(value, mem_operand);
        break;
      case EXTERNAL_FLOAT_ELEMENTS:
      case EXTERNAL_DOUBLE_ELEMENTS:
      case FAST_DOUBLE_ELEMENTS:
      case FAST_ELEMENTS:
      case FAST_SMI_ELEMENTS:
      case FAST_HOLEY_DOUBLE_ELEMENTS:
      case FAST_HOLEY_ELEMENTS:
      case FAST_HOLEY_SMI_ELEMENTS:
      case DICTIONARY_ELEMENTS:
      case NON_STRICT_ARGUMENTS_ELEMENTS:
        UNREACHABLE();
        break;
    }
  }
}


void LCodeGen::DoStoreKeyedFixedDoubleArray(LStoreKeyed* instr) {
  DwVfpRegister value = ToDoubleRegister(instr->value());
  Register elements = ToRegister(instr->elements());
  Register key = no_reg;
  Register scratch = scratch0();
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
  int shift_size = (instr->hydrogen()->key()->representation().IsSmi())
      ? (element_size_shift - kSmiTagSize) : element_size_shift;
  Operand operand = key_is_constant
      ? Operand((constant_key << element_size_shift) +
                FixedDoubleArray::kHeaderSize - kHeapObjectTag)
      : Operand(key, LSL, shift_size);
  __ add(scratch, elements, operand);
  if (!key_is_constant) {
    __ add(scratch, scratch,
           Operand(FixedDoubleArray::kHeaderSize - kHeapObjectTag));
  }

  if (instr->NeedsCanonicalization()) {
    // Force a canonical NaN.
    if (masm()->emit_debug_code()) {
      __ vmrs(ip);
      __ tst(ip, Operand(kVFPDefaultNaNModeControlBit));
      __ Assert(ne, kDefaultNaNModeNotSet);
    }
    __ VFPCanonicalizeNaN(value);
  }
  __ vstr(value, scratch, instr->additional_index() << element_size_shift);
}


void LCodeGen::DoStoreKeyedFixedArray(LStoreKeyed* instr) {
  Register value = ToRegister(instr->value());
  Register elements = ToRegister(instr->elements());
  Register key = instr->key()->IsRegister() ? ToRegister(instr->key())
      : no_reg;
  Register scratch = scratch0();
  Register store_base = scratch;
  int offset = 0;

  // Do the store.
  if (instr->key()->IsConstantOperand()) {
    ASSERT(!instr->hydrogen()->NeedsWriteBarrier());
    LConstantOperand* const_operand = LConstantOperand::cast(instr->key());
    offset = FixedArray::OffsetOfElementAt(ToInteger32(const_operand) +
                                           instr->additional_index());
    store_base = elements;
  } else {
    // Even though the HLoadKeyed instruction forces the input
    // representation for the key to be an integer, the input gets replaced
    // during bound check elimination with the index argument to the bounds
    // check, which can be tagged, so that case must be handled here, too.
    if (instr->hydrogen()->key()->representation().IsSmi()) {
      __ add(scratch, elements, Operand::PointerOffsetFromSmiKey(key));
    } else {
      __ add(scratch, elements, Operand(key, LSL, kPointerSizeLog2));
    }
    offset = FixedArray::OffsetOfElementAt(instr->additional_index());
  }
  __ str(value, FieldMemOperand(store_base, offset));

  if (instr->hydrogen()->NeedsWriteBarrier()) {
    SmiCheck check_needed =
        instr->hydrogen()->value()->IsHeapObject()
            ? OMIT_SMI_CHECK : INLINE_SMI_CHECK;
    // Compute address of modified element and store it into key register.
    __ add(key, store_base, Operand(offset - kHeapObjectTag));
    __ RecordWrite(elements,
                   key,
                   value,
                   GetLinkRegisterState(),
                   kSaveFPRegs,
                   EMIT_REMEMBERED_SET,
                   check_needed);
  }
}


void LCodeGen::DoStoreKeyed(LStoreKeyed* instr) {
  // By cases: external, fast double
  if (instr->is_external()) {
    DoStoreKeyedExternalArray(instr);
  } else if (instr->hydrogen()->value()->representation().IsDouble()) {
    DoStoreKeyedFixedDoubleArray(instr);
  } else {
    DoStoreKeyedFixedArray(instr);
  }
}


void LCodeGen::DoStoreKeyedGeneric(LStoreKeyedGeneric* instr) {
  ASSERT(ToRegister(instr->object()).is(r2));
  ASSERT(ToRegister(instr->key()).is(r1));
  ASSERT(ToRegister(instr->value()).is(r0));

  Handle<Code> ic = (instr->strict_mode_flag() == kStrictMode)
      ? isolate()->builtins()->KeyedStoreIC_Initialize_Strict()
      : isolate()->builtins()->KeyedStoreIC_Initialize();
  CallCode(ic, RelocInfo::CODE_TARGET, instr, NEVER_INLINE_TARGET_ADDRESS);
}


void LCodeGen::DoTransitionElementsKind(LTransitionElementsKind* instr) {
  Register object_reg = ToRegister(instr->object());
  Register scratch = scratch0();

  Handle<Map> from_map = instr->original_map();
  Handle<Map> to_map = instr->transitioned_map();
  ElementsKind from_kind = instr->from_kind();
  ElementsKind to_kind = instr->to_kind();

  Label not_applicable;
  __ ldr(scratch, FieldMemOperand(object_reg, HeapObject::kMapOffset));
  __ cmp(scratch, Operand(from_map));
  __ b(ne, &not_applicable);

  if (IsSimpleMapChangeTransition(from_kind, to_kind)) {
    Register new_map_reg = ToRegister(instr->new_map_temp());
    __ mov(new_map_reg, Operand(to_map));
    __ str(new_map_reg, FieldMemOperand(object_reg, HeapObject::kMapOffset));
    // Write barrier.
    __ RecordWriteField(object_reg, HeapObject::kMapOffset, new_map_reg,
                        scratch, GetLinkRegisterState(), kDontSaveFPRegs);
  } else {
    PushSafepointRegistersScope scope(
        this, Safepoint::kWithRegistersAndDoubles);
    __ Move(r0, object_reg);
    __ Move(r1, to_map);
    TransitionElementsKindStub stub(from_kind, to_kind);
    __ CallStub(&stub);
    RecordSafepointWithRegistersAndDoubles(
        instr->pointer_map(), 0, Safepoint::kNoLazyDeopt);
  }
  __ bind(&not_applicable);
}


void LCodeGen::DoTrapAllocationMemento(LTrapAllocationMemento* instr) {
  Register object = ToRegister(instr->object());
  Register temp = ToRegister(instr->temp());
  __ TestJSArrayForAllocationMemento(object, temp);
  DeoptimizeIf(eq, instr->environment());
}


void LCodeGen::DoStringAdd(LStringAdd* instr) {
  __ push(ToRegister(instr->left()));
  __ push(ToRegister(instr->right()));
  StringAddStub stub(instr->hydrogen()->flags());
  CallCode(stub.GetCode(isolate()), RelocInfo::CODE_TARGET, instr);
}


void LCodeGen::DoStringCharCodeAt(LStringCharCodeAt* instr) {
  class DeferredStringCharCodeAt V8_FINAL : public LDeferredCode {
   public:
    DeferredStringCharCodeAt(LCodeGen* codegen, LStringCharCodeAt* instr)
        : LDeferredCode(codegen), instr_(instr) { }
    virtual void Generate() V8_OVERRIDE {
      codegen()->DoDeferredStringCharCodeAt(instr_);
    }
    virtual LInstruction* instr() V8_OVERRIDE { return instr_; }
   private:
    LStringCharCodeAt* instr_;
  };

  DeferredStringCharCodeAt* deferred =
      new(zone()) DeferredStringCharCodeAt(this, instr);

  StringCharLoadGenerator::Generate(masm(),
                                    ToRegister(instr->string()),
                                    ToRegister(instr->index()),
                                    ToRegister(instr->result()),
                                    deferred->entry());
  __ bind(deferred->exit());
}


void LCodeGen::DoDeferredStringCharCodeAt(LStringCharCodeAt* instr) {
  Register string = ToRegister(instr->string());
  Register result = ToRegister(instr->result());
  Register scratch = scratch0();

  // TODO(3095996): Get rid of this. For now, we need to make the
  // result register contain a valid pointer because it is already
  // contained in the register pointer map.
  __ mov(result, Operand::Zero());

  PushSafepointRegistersScope scope(this, Safepoint::kWithRegisters);
  __ push(string);
  // Push the index as a smi. This is safe because of the checks in
  // DoStringCharCodeAt above.
  if (instr->index()->IsConstantOperand()) {
    int const_index = ToInteger32(LConstantOperand::cast(instr->index()));
    __ mov(scratch, Operand(Smi::FromInt(const_index)));
    __ push(scratch);
  } else {
    Register index = ToRegister(instr->index());
    __ SmiTag(index);
    __ push(index);
  }
  CallRuntimeFromDeferred(Runtime::kStringCharCodeAt, 2, instr);
  __ AssertSmi(r0);
  __ SmiUntag(r0);
  __ StoreToSafepointRegisterSlot(r0, result);
}


void LCodeGen::DoStringCharFromCode(LStringCharFromCode* instr) {
  class DeferredStringCharFromCode V8_FINAL : public LDeferredCode {
   public:
    DeferredStringCharFromCode(LCodeGen* codegen, LStringCharFromCode* instr)
        : LDeferredCode(codegen), instr_(instr) { }
    virtual void Generate() V8_OVERRIDE {
      codegen()->DoDeferredStringCharFromCode(instr_);
    }
    virtual LInstruction* instr() V8_OVERRIDE { return instr_; }
   private:
    LStringCharFromCode* instr_;
  };

  DeferredStringCharFromCode* deferred =
      new(zone()) DeferredStringCharFromCode(this, instr);

  ASSERT(instr->hydrogen()->value()->representation().IsInteger32());
  Register char_code = ToRegister(instr->char_code());
  Register result = ToRegister(instr->result());
  ASSERT(!char_code.is(result));

  __ cmp(char_code, Operand(String::kMaxOneByteCharCode));
  __ b(hi, deferred->entry());
  __ LoadRoot(result, Heap::kSingleCharacterStringCacheRootIndex);
  __ add(result, result, Operand(char_code, LSL, kPointerSizeLog2));
  __ ldr(result, FieldMemOperand(result, FixedArray::kHeaderSize));
  __ LoadRoot(ip, Heap::kUndefinedValueRootIndex);
  __ cmp(result, ip);
  __ b(eq, deferred->entry());
  __ bind(deferred->exit());
}


void LCodeGen::DoDeferredStringCharFromCode(LStringCharFromCode* instr) {
  Register char_code = ToRegister(instr->char_code());
  Register result = ToRegister(instr->result());

  // TODO(3095996): Get rid of this. For now, we need to make the
  // result register contain a valid pointer because it is already
  // contained in the register pointer map.
  __ mov(result, Operand::Zero());

  PushSafepointRegistersScope scope(this, Safepoint::kWithRegisters);
  __ SmiTag(char_code);
  __ push(char_code);
  CallRuntimeFromDeferred(Runtime::kCharFromCode, 1, instr);
  __ StoreToSafepointRegisterSlot(r0, result);
}


void LCodeGen::DoInteger32ToDouble(LInteger32ToDouble* instr) {
  LOperand* input = instr->value();
  ASSERT(input->IsRegister() || input->IsStackSlot());
  LOperand* output = instr->result();
  ASSERT(output->IsDoubleRegister());
  SwVfpRegister single_scratch = double_scratch0().low();
  if (input->IsStackSlot()) {
    Register scratch = scratch0();
    __ ldr(scratch, ToMemOperand(input));
    __ vmov(single_scratch, scratch);
  } else {
    __ vmov(single_scratch, ToRegister(input));
  }
  __ vcvt_f64_s32(ToDoubleRegister(output), single_scratch);
}


void LCodeGen::DoInteger32ToSmi(LInteger32ToSmi* instr) {
  LOperand* input = instr->value();
  ASSERT(input->IsRegister());
  LOperand* output = instr->result();
  ASSERT(output->IsRegister());
  __ SmiTag(ToRegister(output), ToRegister(input), SetCC);
  if (!instr->hydrogen()->value()->HasRange() ||
      !instr->hydrogen()->value()->range()->IsInSmiRange()) {
    DeoptimizeIf(vs, instr->environment());
  }
}


void LCodeGen::DoUint32ToDouble(LUint32ToDouble* instr) {
  LOperand* input = instr->value();
  LOperand* output = instr->result();

  SwVfpRegister flt_scratch = double_scratch0().low();
  __ vmov(flt_scratch, ToRegister(input));
  __ vcvt_f64_u32(ToDoubleRegister(output), flt_scratch);
}


void LCodeGen::DoNumberTagI(LNumberTagI* instr) {
  class DeferredNumberTagI V8_FINAL : public LDeferredCode {
   public:
    DeferredNumberTagI(LCodeGen* codegen, LNumberTagI* instr)
        : LDeferredCode(codegen), instr_(instr) { }
    virtual void Generate() V8_OVERRIDE {
      codegen()->DoDeferredNumberTagI(instr_,
                                      instr_->value(),
                                      SIGNED_INT32);
    }
    virtual LInstruction* instr() V8_OVERRIDE { return instr_; }
   private:
    LNumberTagI* instr_;
  };

  Register src = ToRegister(instr->value());
  Register dst = ToRegister(instr->result());

  DeferredNumberTagI* deferred = new(zone()) DeferredNumberTagI(this, instr);
  __ SmiTag(dst, src, SetCC);
  __ b(vs, deferred->entry());
  __ bind(deferred->exit());
}


void LCodeGen::DoNumberTagU(LNumberTagU* instr) {
  class DeferredNumberTagU V8_FINAL : public LDeferredCode {
   public:
    DeferredNumberTagU(LCodeGen* codegen, LNumberTagU* instr)
        : LDeferredCode(codegen), instr_(instr) { }
    virtual void Generate() V8_OVERRIDE {
      codegen()->DoDeferredNumberTagI(instr_,
                                      instr_->value(),
                                      UNSIGNED_INT32);
    }
    virtual LInstruction* instr() V8_OVERRIDE { return instr_; }
   private:
    LNumberTagU* instr_;
  };

  LOperand* input = instr->value();
  ASSERT(input->IsRegister() && input->Equals(instr->result()));
  Register reg = ToRegister(input);

  DeferredNumberTagU* deferred = new(zone()) DeferredNumberTagU(this, instr);
  __ cmp(reg, Operand(Smi::kMaxValue));
  __ b(hi, deferred->entry());
  __ SmiTag(reg, reg);
  __ bind(deferred->exit());
}


void LCodeGen::DoDeferredNumberTagI(LInstruction* instr,
                                    LOperand* value,
                                    IntegerSignedness signedness) {
  Label slow;
  Register src = ToRegister(value);
  Register dst = ToRegister(instr->result());
  LowDwVfpRegister dbl_scratch = double_scratch0();

  // Preserve the value of all registers.
  PushSafepointRegistersScope scope(this, Safepoint::kWithRegisters);

  Label done;
  if (signedness == SIGNED_INT32) {
    // There was overflow, so bits 30 and 31 of the original integer
    // disagree. Try to allocate a heap number in new space and store
    // the value in there. If that fails, call the runtime system.
    if (dst.is(src)) {
      __ SmiUntag(src, dst);
      __ eor(src, src, Operand(0x80000000));
    }
    __ vmov(dbl_scratch.low(), src);
    __ vcvt_f64_s32(dbl_scratch, dbl_scratch.low());
  } else {
    __ vmov(dbl_scratch.low(), src);
    __ vcvt_f64_u32(dbl_scratch, dbl_scratch.low());
  }

  if (FLAG_inline_new) {
    __ LoadRoot(scratch0(), Heap::kHeapNumberMapRootIndex);
    __ AllocateHeapNumber(r5, r3, r4, scratch0(), &slow, DONT_TAG_RESULT);
    __ Move(dst, r5);
    __ b(&done);
  }

  // Slow case: Call the runtime system to do the number allocation.
  __ bind(&slow);

  // TODO(3095996): Put a valid pointer value in the stack slot where the result
  // register is stored, as this register is in the pointer map, but contains an
  // integer value.
  __ mov(ip, Operand::Zero());
  __ StoreToSafepointRegisterSlot(ip, dst);
  CallRuntimeFromDeferred(Runtime::kAllocateHeapNumber, 0, instr);
  __ Move(dst, r0);
  __ sub(dst, dst, Operand(kHeapObjectTag));

  // Done. Put the value in dbl_scratch into the value of the allocated heap
  // number.
  __ bind(&done);
  __ vstr(dbl_scratch, dst, HeapNumber::kValueOffset);
  __ add(dst, dst, Operand(kHeapObjectTag));
  __ StoreToSafepointRegisterSlot(dst, dst);
}


void LCodeGen::DoNumberTagD(LNumberTagD* instr) {
  class DeferredNumberTagD V8_FINAL : public LDeferredCode {
   public:
    DeferredNumberTagD(LCodeGen* codegen, LNumberTagD* instr)
        : LDeferredCode(codegen), instr_(instr) { }
    virtual void Generate() V8_OVERRIDE {
      codegen()->DoDeferredNumberTagD(instr_);
    }
    virtual LInstruction* instr() V8_OVERRIDE { return instr_; }
   private:
    LNumberTagD* instr_;
  };

  DwVfpRegister input_reg = ToDoubleRegister(instr->value());
  Register scratch = scratch0();
  Register reg = ToRegister(instr->result());
  Register temp1 = ToRegister(instr->temp());
  Register temp2 = ToRegister(instr->temp2());

  DeferredNumberTagD* deferred = new(zone()) DeferredNumberTagD(this, instr);
  if (FLAG_inline_new) {
    __ LoadRoot(scratch, Heap::kHeapNumberMapRootIndex);
    // We want the untagged address first for performance
    __ AllocateHeapNumber(reg, temp1, temp2, scratch, deferred->entry(),
                          DONT_TAG_RESULT);
  } else {
    __ jmp(deferred->entry());
  }
  __ bind(deferred->exit());
  __ vstr(input_reg, reg, HeapNumber::kValueOffset);
  // Now that we have finished with the object's real address tag it
  __ add(reg, reg, Operand(kHeapObjectTag));
}


void LCodeGen::DoDeferredNumberTagD(LNumberTagD* instr) {
  // TODO(3095996): Get rid of this. For now, we need to make the
  // result register contain a valid pointer because it is already
  // contained in the register pointer map.
  Register reg = ToRegister(instr->result());
  __ mov(reg, Operand::Zero());

  PushSafepointRegistersScope scope(this, Safepoint::kWithRegisters);
  CallRuntimeFromDeferred(Runtime::kAllocateHeapNumber, 0, instr);
  __ sub(r0, r0, Operand(kHeapObjectTag));
  __ StoreToSafepointRegisterSlot(r0, reg);
}


void LCodeGen::DoSmiTag(LSmiTag* instr) {
  ASSERT(!instr->hydrogen_value()->CheckFlag(HValue::kCanOverflow));
  __ SmiTag(ToRegister(instr->result()), ToRegister(instr->value()));
}


void LCodeGen::DoSmiUntag(LSmiUntag* instr) {
  Register input = ToRegister(instr->value());
  Register result = ToRegister(instr->result());
  if (instr->needs_check()) {
    STATIC_ASSERT(kHeapObjectTag == 1);
    // If the input is a HeapObject, SmiUntag will set the carry flag.
    __ SmiUntag(result, input, SetCC);
    DeoptimizeIf(cs, instr->environment());
  } else {
    __ SmiUntag(result, input);
  }
}


void LCodeGen::EmitNumberUntagD(Register input_reg,
                                DwVfpRegister result_reg,
                                bool can_convert_undefined_to_nan,
                                bool deoptimize_on_minus_zero,
                                LEnvironment* env,
                                NumberUntagDMode mode) {
  Register scratch = scratch0();
  SwVfpRegister flt_scratch = double_scratch0().low();
  ASSERT(!result_reg.is(double_scratch0()));

  Label load_smi, heap_number, done;

  if (mode == NUMBER_CANDIDATE_IS_ANY_TAGGED) {
    // Smi check.
    __ UntagAndJumpIfSmi(scratch, input_reg, &load_smi);

    // Heap number map check.
    __ ldr(scratch, FieldMemOperand(input_reg, HeapObject::kMapOffset));
    __ LoadRoot(ip, Heap::kHeapNumberMapRootIndex);
    __ cmp(scratch, Operand(ip));
    if (!can_convert_undefined_to_nan) {
      DeoptimizeIf(ne, env);
    } else {
      Label heap_number, convert;
      __ b(eq, &heap_number);

      // Convert undefined (and hole) to NaN.
      __ LoadRoot(ip, Heap::kUndefinedValueRootIndex);
      __ cmp(input_reg, Operand(ip));
      DeoptimizeIf(ne, env);

      __ bind(&convert);
      __ LoadRoot(scratch, Heap::kNanValueRootIndex);
      __ vldr(result_reg, scratch, HeapNumber::kValueOffset - kHeapObjectTag);
      __ jmp(&done);

      __ bind(&heap_number);
    }
    // Heap number to double register conversion.
    __ vldr(result_reg, input_reg, HeapNumber::kValueOffset - kHeapObjectTag);
    if (deoptimize_on_minus_zero) {
      __ VmovLow(scratch, result_reg);
      __ cmp(scratch, Operand::Zero());
      __ b(ne, &done);
      __ VmovHigh(scratch, result_reg);
      __ cmp(scratch, Operand(HeapNumber::kSignMask));
      DeoptimizeIf(eq, env);
    }
    __ jmp(&done);
  } else {
    __ SmiUntag(scratch, input_reg);
    ASSERT(mode == NUMBER_CANDIDATE_IS_SMI);
  }

  // Smi to double register conversion
  __ bind(&load_smi);
  // scratch: untagged value of input_reg
  __ vmov(flt_scratch, scratch);
  __ vcvt_f64_s32(result_reg, flt_scratch);
  __ bind(&done);
}


void LCodeGen::DoDeferredTaggedToI(LTaggedToI* instr) {
  Register input_reg = ToRegister(instr->value());
  Register scratch1 = scratch0();
  Register scratch2 = ToRegister(instr->temp());
  LowDwVfpRegister double_scratch = double_scratch0();
  DwVfpRegister double_scratch2 = ToDoubleRegister(instr->temp2());

  ASSERT(!scratch1.is(input_reg) && !scratch1.is(scratch2));
  ASSERT(!scratch2.is(input_reg) && !scratch2.is(scratch1));

  Label done;

  // The input was optimistically untagged; revert it.
  // The carry flag is set when we reach this deferred code as we just executed
  // SmiUntag(heap_object, SetCC)
  STATIC_ASSERT(kHeapObjectTag == 1);
  __ adc(scratch2, input_reg, Operand(input_reg));

  // Heap number map check.
  __ ldr(scratch1, FieldMemOperand(scratch2, HeapObject::kMapOffset));
  __ LoadRoot(ip, Heap::kHeapNumberMapRootIndex);
  __ cmp(scratch1, Operand(ip));

  if (instr->truncating()) {
    // Performs a truncating conversion of a floating point number as used by
    // the JS bitwise operations.
    Label heap_number;
    __ b(eq, &heap_number);
    // Check for undefined. Undefined is converted to zero for truncating
    // conversions.
    __ LoadRoot(ip, Heap::kUndefinedValueRootIndex);
    __ cmp(scratch2, Operand(ip));
    DeoptimizeIf(ne, instr->environment());
    __ mov(input_reg, Operand::Zero());
    __ b(&done);

    __ bind(&heap_number);
    __ TruncateHeapNumberToI(input_reg, scratch2);
  } else {
    // Deoptimize if we don't have a heap number.
    DeoptimizeIf(ne, instr->environment());

    __ sub(ip, scratch2, Operand(kHeapObjectTag));
    __ vldr(double_scratch2, ip, HeapNumber::kValueOffset);
    __ TryDoubleToInt32Exact(input_reg, double_scratch2, double_scratch);
    DeoptimizeIf(ne, instr->environment());

    if (instr->hydrogen()->CheckFlag(HValue::kBailoutOnMinusZero)) {
      __ cmp(input_reg, Operand::Zero());
      __ b(ne, &done);
      __ VmovHigh(scratch1, double_scratch2);
      __ tst(scratch1, Operand(HeapNumber::kSignMask));
      DeoptimizeIf(ne, instr->environment());
    }
  }
  __ bind(&done);
}


void LCodeGen::DoTaggedToI(LTaggedToI* instr) {
  class DeferredTaggedToI V8_FINAL : public LDeferredCode {
   public:
    DeferredTaggedToI(LCodeGen* codegen, LTaggedToI* instr)
        : LDeferredCode(codegen), instr_(instr) { }
    virtual void Generate() V8_OVERRIDE {
      codegen()->DoDeferredTaggedToI(instr_);
    }
    virtual LInstruction* instr() V8_OVERRIDE { return instr_; }
   private:
    LTaggedToI* instr_;
  };

  LOperand* input = instr->value();
  ASSERT(input->IsRegister());
  ASSERT(input->Equals(instr->result()));

  Register input_reg = ToRegister(input);

  DeferredTaggedToI* deferred = new(zone()) DeferredTaggedToI(this, instr);

  // Optimistically untag the input.
  // If the input is a HeapObject, SmiUntag will set the carry flag.
  __ SmiUntag(input_reg, SetCC);
  // Branch to deferred code if the input was tagged.
  // The deferred code will take care of restoring the tag.
  __ b(cs, deferred->entry());
  __ bind(deferred->exit());
}


void LCodeGen::DoNumberUntagD(LNumberUntagD* instr) {
  LOperand* input = instr->value();
  ASSERT(input->IsRegister());
  LOperand* result = instr->result();
  ASSERT(result->IsDoubleRegister());

  Register input_reg = ToRegister(input);
  DwVfpRegister result_reg = ToDoubleRegister(result);

  HValue* value = instr->hydrogen()->value();
  NumberUntagDMode mode = value->representation().IsSmi()
      ? NUMBER_CANDIDATE_IS_SMI : NUMBER_CANDIDATE_IS_ANY_TAGGED;

  EmitNumberUntagD(input_reg, result_reg,
                   instr->hydrogen()->can_convert_undefined_to_nan(),
                   instr->hydrogen()->deoptimize_on_minus_zero(),
                   instr->environment(),
                   mode);
}


void LCodeGen::DoDoubleToI(LDoubleToI* instr) {
  Register result_reg = ToRegister(instr->result());
  Register scratch1 = scratch0();
  DwVfpRegister double_input = ToDoubleRegister(instr->value());
  LowDwVfpRegister double_scratch = double_scratch0();

  if (instr->truncating()) {
    __ TruncateDoubleToI(result_reg, double_input);
  } else {
    __ TryDoubleToInt32Exact(result_reg, double_input, double_scratch);
    // Deoptimize if the input wasn't a int32 (inside a double).
    DeoptimizeIf(ne, instr->environment());
    if (instr->hydrogen()->CheckFlag(HValue::kBailoutOnMinusZero)) {
      Label done;
      __ cmp(result_reg, Operand::Zero());
      __ b(ne, &done);
      __ VmovHigh(scratch1, double_input);
      __ tst(scratch1, Operand(HeapNumber::kSignMask));
      DeoptimizeIf(ne, instr->environment());
      __ bind(&done);
    }
  }
}


void LCodeGen::DoDoubleToSmi(LDoubleToSmi* instr) {
  Register result_reg = ToRegister(instr->result());
  Register scratch1 = scratch0();
  DwVfpRegister double_input = ToDoubleRegister(instr->value());
  LowDwVfpRegister double_scratch = double_scratch0();

  if (instr->truncating()) {
    __ TruncateDoubleToI(result_reg, double_input);
  } else {
    __ TryDoubleToInt32Exact(result_reg, double_input, double_scratch);
    // Deoptimize if the input wasn't a int32 (inside a double).
    DeoptimizeIf(ne, instr->environment());
    if (instr->hydrogen()->CheckFlag(HValue::kBailoutOnMinusZero)) {
      Label done;
      __ cmp(result_reg, Operand::Zero());
      __ b(ne, &done);
      __ VmovHigh(scratch1, double_input);
      __ tst(scratch1, Operand(HeapNumber::kSignMask));
      DeoptimizeIf(ne, instr->environment());
      __ bind(&done);
    }
  }
  __ SmiTag(result_reg, SetCC);
  DeoptimizeIf(vs, instr->environment());
}


void LCodeGen::DoCheckSmi(LCheckSmi* instr) {
  LOperand* input = instr->value();
  __ SmiTst(ToRegister(input));
  DeoptimizeIf(ne, instr->environment());
}


void LCodeGen::DoCheckNonSmi(LCheckNonSmi* instr) {
  if (!instr->hydrogen()->value()->IsHeapObject()) {
    LOperand* input = instr->value();
    __ SmiTst(ToRegister(input));
    DeoptimizeIf(eq, instr->environment());
  }
}


void LCodeGen::DoCheckInstanceType(LCheckInstanceType* instr) {
  Register input = ToRegister(instr->value());
  Register scratch = scratch0();

  __ ldr(scratch, FieldMemOperand(input, HeapObject::kMapOffset));
  __ ldrb(scratch, FieldMemOperand(scratch, Map::kInstanceTypeOffset));

  if (instr->hydrogen()->is_interval_check()) {
    InstanceType first;
    InstanceType last;
    instr->hydrogen()->GetCheckInterval(&first, &last);

    __ cmp(scratch, Operand(first));

    // If there is only one type in the interval check for equality.
    if (first == last) {
      DeoptimizeIf(ne, instr->environment());
    } else {
      DeoptimizeIf(lo, instr->environment());
      // Omit check for the last type.
      if (last != LAST_TYPE) {
        __ cmp(scratch, Operand(last));
        DeoptimizeIf(hi, instr->environment());
      }
    }
  } else {
    uint8_t mask;
    uint8_t tag;
    instr->hydrogen()->GetCheckMaskAndTag(&mask, &tag);

    if (IsPowerOf2(mask)) {
      ASSERT(tag == 0 || IsPowerOf2(tag));
      __ tst(scratch, Operand(mask));
      DeoptimizeIf(tag == 0 ? ne : eq, instr->environment());
    } else {
      __ and_(scratch, scratch, Operand(mask));
      __ cmp(scratch, Operand(tag));
      DeoptimizeIf(ne, instr->environment());
    }
  }
}


void LCodeGen::DoCheckValue(LCheckValue* instr) {
  Register reg = ToRegister(instr->value());
  Handle<HeapObject> object = instr->hydrogen()->object();
  AllowDeferredHandleDereference smi_check;
  if (isolate()->heap()->InNewSpace(*object)) {
    Register reg = ToRegister(instr->value());
    Handle<Cell> cell = isolate()->factory()->NewCell(object);
    __ mov(ip, Operand(Handle<Object>(cell)));
    __ ldr(ip, FieldMemOperand(ip, Cell::kValueOffset));
    __ cmp(reg, ip);
  } else {
    __ cmp(reg, Operand(object));
  }
  DeoptimizeIf(ne, instr->environment());
}


void LCodeGen::DoDeferredInstanceMigration(LCheckMaps* instr, Register object) {
  {
    PushSafepointRegistersScope scope(this, Safepoint::kWithRegisters);
    __ push(object);
    CallRuntimeFromDeferred(Runtime::kMigrateInstance, 1, instr);
    __ StoreToSafepointRegisterSlot(r0, scratch0());
  }
  __ tst(scratch0(), Operand(kSmiTagMask));
  DeoptimizeIf(eq, instr->environment());
}


void LCodeGen::DoCheckMaps(LCheckMaps* instr) {
  class DeferredCheckMaps V8_FINAL : public LDeferredCode {
   public:
    DeferredCheckMaps(LCodeGen* codegen, LCheckMaps* instr, Register object)
        : LDeferredCode(codegen), instr_(instr), object_(object) {
      SetExit(check_maps());
    }
    virtual void Generate() V8_OVERRIDE {
      codegen()->DoDeferredInstanceMigration(instr_, object_);
    }
    Label* check_maps() { return &check_maps_; }
    virtual LInstruction* instr() V8_OVERRIDE { return instr_; }
   private:
    LCheckMaps* instr_;
    Label check_maps_;
    Register object_;
  };

  if (instr->hydrogen()->CanOmitMapChecks()) return;
  Register map_reg = scratch0();

  LOperand* input = instr->value();
  ASSERT(input->IsRegister());
  Register reg = ToRegister(input);

  SmallMapList* map_set = instr->hydrogen()->map_set();
  __ ldr(map_reg, FieldMemOperand(reg, HeapObject::kMapOffset));

  DeferredCheckMaps* deferred = NULL;
  if (instr->hydrogen()->has_migration_target()) {
    deferred = new(zone()) DeferredCheckMaps(this, instr, reg);
    __ bind(deferred->check_maps());
  }

  Label success;
  for (int i = 0; i < map_set->length() - 1; i++) {
    Handle<Map> map = map_set->at(i);
    __ CompareMap(map_reg, map, &success);
    __ b(eq, &success);
  }

  Handle<Map> map = map_set->last();
  __ CompareMap(map_reg, map, &success);
  if (instr->hydrogen()->has_migration_target()) {
    __ b(ne, deferred->entry());
  } else {
    DeoptimizeIf(ne, instr->environment());
  }

  __ bind(&success);
}


void LCodeGen::DoClampDToUint8(LClampDToUint8* instr) {
  DwVfpRegister value_reg = ToDoubleRegister(instr->unclamped());
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
  DwVfpRegister temp_reg = ToDoubleRegister(instr->temp());
  Label is_smi, done, heap_number;

  // Both smi and heap number cases are handled.
  __ UntagAndJumpIfSmi(result_reg, input_reg, &is_smi);

  // Check for heap number
  __ ldr(scratch, FieldMemOperand(input_reg, HeapObject::kMapOffset));
  __ cmp(scratch, Operand(factory()->heap_number_map()));
  __ b(eq, &heap_number);

  // Check for undefined. Undefined is converted to zero for clamping
  // conversions.
  __ cmp(input_reg, Operand(factory()->undefined_value()));
  DeoptimizeIf(ne, instr->environment());
  __ mov(result_reg, Operand::Zero());
  __ jmp(&done);

  // Heap number
  __ bind(&heap_number);
  __ vldr(temp_reg, FieldMemOperand(input_reg, HeapNumber::kValueOffset));
  __ ClampDoubleToUint8(result_reg, temp_reg, double_scratch0());
  __ jmp(&done);

  // smi
  __ bind(&is_smi);
  __ ClampUint8(result_reg, result_reg);

  __ bind(&done);
}


void LCodeGen::DoAllocate(LAllocate* instr) {
  class DeferredAllocate V8_FINAL : public LDeferredCode {
   public:
    DeferredAllocate(LCodeGen* codegen, LAllocate* instr)
        : LDeferredCode(codegen), instr_(instr) { }
    virtual void Generate() V8_OVERRIDE {
      codegen()->DoDeferredAllocate(instr_);
    }
    virtual LInstruction* instr() V8_OVERRIDE { return instr_; }
   private:
    LAllocate* instr_;
  };

  DeferredAllocate* deferred =
      new(zone()) DeferredAllocate(this, instr);

  Register result = ToRegister(instr->result());
  Register scratch = ToRegister(instr->temp1());
  Register scratch2 = ToRegister(instr->temp2());

  // Allocate memory for the object.
  AllocationFlags flags = TAG_OBJECT;
  if (instr->hydrogen()->MustAllocateDoubleAligned()) {
    flags = static_cast<AllocationFlags>(flags | DOUBLE_ALIGNMENT);
  }
  if (instr->hydrogen()->IsOldPointerSpaceAllocation()) {
    ASSERT(!instr->hydrogen()->IsOldDataSpaceAllocation());
    ASSERT(!instr->hydrogen()->IsNewSpaceAllocation());
    flags = static_cast<AllocationFlags>(flags | PRETENURE_OLD_POINTER_SPACE);
  } else if (instr->hydrogen()->IsOldDataSpaceAllocation()) {
    ASSERT(!instr->hydrogen()->IsNewSpaceAllocation());
    flags = static_cast<AllocationFlags>(flags | PRETENURE_OLD_DATA_SPACE);
  }

  if (instr->size()->IsConstantOperand()) {
    int32_t size = ToInteger32(LConstantOperand::cast(instr->size()));
    __ Allocate(size, result, scratch, scratch2, deferred->entry(), flags);
  } else {
    Register size = ToRegister(instr->size());
    __ Allocate(size,
                result,
                scratch,
                scratch2,
                deferred->entry(),
                flags);
  }

  __ bind(deferred->exit());

  if (instr->hydrogen()->MustPrefillWithFiller()) {
    if (instr->size()->IsConstantOperand()) {
      int32_t size = ToInteger32(LConstantOperand::cast(instr->size()));
      __ mov(scratch, Operand(size));
    } else {
      scratch = ToRegister(instr->size());
    }
    __ sub(scratch, scratch, Operand(kPointerSize));
    __ sub(result, result, Operand(kHeapObjectTag));
    Label loop;
    __ bind(&loop);
    __ mov(scratch2, Operand(isolate()->factory()->one_pointer_filler_map()));
    __ str(scratch2, MemOperand(result, scratch));
    __ sub(scratch, scratch, Operand(kPointerSize));
    __ cmp(scratch, Operand(0));
    __ b(ge, &loop);
    __ add(result, result, Operand(kHeapObjectTag));
  }
}


void LCodeGen::DoDeferredAllocate(LAllocate* instr) {
  Register result = ToRegister(instr->result());

  // TODO(3095996): Get rid of this. For now, we need to make the
  // result register contain a valid pointer because it is already
  // contained in the register pointer map.
  __ mov(result, Operand(Smi::FromInt(0)));

  PushSafepointRegistersScope scope(this, Safepoint::kWithRegisters);
  if (instr->size()->IsRegister()) {
    Register size = ToRegister(instr->size());
    ASSERT(!size.is(result));
    __ SmiTag(size);
    __ push(size);
  } else {
    int32_t size = ToInteger32(LConstantOperand::cast(instr->size()));
    __ Push(Smi::FromInt(size));
  }

  if (instr->hydrogen()->IsOldPointerSpaceAllocation()) {
    ASSERT(!instr->hydrogen()->IsOldDataSpaceAllocation());
    ASSERT(!instr->hydrogen()->IsNewSpaceAllocation());
    CallRuntimeFromDeferred(Runtime::kAllocateInOldPointerSpace, 1, instr);
  } else if (instr->hydrogen()->IsOldDataSpaceAllocation()) {
    ASSERT(!instr->hydrogen()->IsNewSpaceAllocation());
    CallRuntimeFromDeferred(Runtime::kAllocateInOldDataSpace, 1, instr);
  } else {
    CallRuntimeFromDeferred(Runtime::kAllocateInNewSpace, 1, instr);
  }
  __ StoreToSafepointRegisterSlot(r0, result);
}


void LCodeGen::DoToFastProperties(LToFastProperties* instr) {
  ASSERT(ToRegister(instr->value()).is(r0));
  __ push(r0);
  CallRuntime(Runtime::kToFastProperties, 1, instr);
}


void LCodeGen::DoRegExpLiteral(LRegExpLiteral* instr) {
  Label materialized;
  // Registers will be used as follows:
  // r7 = literals array.
  // r1 = regexp literal.
  // r0 = regexp literal clone.
  // r2 and r4-r6 are used as temporaries.
  int literal_offset =
      FixedArray::OffsetOfElementAt(instr->hydrogen()->literal_index());
  __ LoadHeapObject(r7, instr->hydrogen()->literals());
  __ ldr(r1, FieldMemOperand(r7, literal_offset));
  __ LoadRoot(ip, Heap::kUndefinedValueRootIndex);
  __ cmp(r1, ip);
  __ b(ne, &materialized);

  // Create regexp literal using runtime function
  // Result will be in r0.
  __ mov(r6, Operand(Smi::FromInt(instr->hydrogen()->literal_index())));
  __ mov(r5, Operand(instr->hydrogen()->pattern()));
  __ mov(r4, Operand(instr->hydrogen()->flags()));
  __ Push(r7, r6, r5, r4);
  CallRuntime(Runtime::kMaterializeRegExpLiteral, 4, instr);
  __ mov(r1, r0);

  __ bind(&materialized);
  int size = JSRegExp::kSize + JSRegExp::kInObjectFieldCount * kPointerSize;
  Label allocated, runtime_allocate;

  __ Allocate(size, r0, r2, r3, &runtime_allocate, TAG_OBJECT);
  __ jmp(&allocated);

  __ bind(&runtime_allocate);
  __ mov(r0, Operand(Smi::FromInt(size)));
  __ Push(r1, r0);
  CallRuntime(Runtime::kAllocateInNewSpace, 1, instr);
  __ pop(r1);

  __ bind(&allocated);
  // Copy the content into the newly allocated memory.
  __ CopyFields(r0, r1, double_scratch0(), size / kPointerSize);
}


void LCodeGen::DoFunctionLiteral(LFunctionLiteral* instr) {
  // Use the fast case closure allocation code that allocates in new
  // space for nested functions that don't need literals cloning.
  bool pretenure = instr->hydrogen()->pretenure();
  if (!pretenure && instr->hydrogen()->has_no_literals()) {
    FastNewClosureStub stub(instr->hydrogen()->language_mode(),
                            instr->hydrogen()->is_generator());
    __ mov(r2, Operand(instr->hydrogen()->shared_info()));
    CallCode(stub.GetCode(isolate()), RelocInfo::CODE_TARGET, instr);
  } else {
    __ mov(r2, Operand(instr->hydrogen()->shared_info()));
    __ mov(r1, Operand(pretenure ? factory()->true_value()
                                 : factory()->false_value()));
    __ Push(cp, r2, r1);
    CallRuntime(Runtime::kNewClosure, 3, instr);
  }
}


void LCodeGen::DoTypeof(LTypeof* instr) {
  Register input = ToRegister(instr->value());
  __ push(input);
  CallRuntime(Runtime::kTypeof, 1, instr);
}


void LCodeGen::DoTypeofIsAndBranch(LTypeofIsAndBranch* instr) {
  Register input = ToRegister(instr->value());

  Condition final_branch_condition = EmitTypeofIs(instr->TrueLabel(chunk_),
                                                  instr->FalseLabel(chunk_),
                                                  input,
                                                  instr->type_literal());
  if (final_branch_condition != kNoCondition) {
    EmitBranch(instr, final_branch_condition);
  }
}


Condition LCodeGen::EmitTypeofIs(Label* true_label,
                                 Label* false_label,
                                 Register input,
                                 Handle<String> type_name) {
  Condition final_branch_condition = kNoCondition;
  Register scratch = scratch0();
  if (type_name->Equals(heap()->number_string())) {
    __ JumpIfSmi(input, true_label);
    __ ldr(input, FieldMemOperand(input, HeapObject::kMapOffset));
    __ LoadRoot(ip, Heap::kHeapNumberMapRootIndex);
    __ cmp(input, Operand(ip));
    final_branch_condition = eq;

  } else if (type_name->Equals(heap()->string_string())) {
    __ JumpIfSmi(input, false_label);
    __ CompareObjectType(input, input, scratch, FIRST_NONSTRING_TYPE);
    __ b(ge, false_label);
    __ ldrb(ip, FieldMemOperand(input, Map::kBitFieldOffset));
    __ tst(ip, Operand(1 << Map::kIsUndetectable));
    final_branch_condition = eq;

  } else if (type_name->Equals(heap()->symbol_string())) {
    __ JumpIfSmi(input, false_label);
    __ CompareObjectType(input, input, scratch, SYMBOL_TYPE);
    final_branch_condition = eq;

  } else if (type_name->Equals(heap()->boolean_string())) {
    __ CompareRoot(input, Heap::kTrueValueRootIndex);
    __ b(eq, true_label);
    __ CompareRoot(input, Heap::kFalseValueRootIndex);
    final_branch_condition = eq;

  } else if (FLAG_harmony_typeof && type_name->Equals(heap()->null_string())) {
    __ CompareRoot(input, Heap::kNullValueRootIndex);
    final_branch_condition = eq;

  } else if (type_name->Equals(heap()->undefined_string())) {
    __ CompareRoot(input, Heap::kUndefinedValueRootIndex);
    __ b(eq, true_label);
    __ JumpIfSmi(input, false_label);
    // Check for undetectable objects => true.
    __ ldr(input, FieldMemOperand(input, HeapObject::kMapOffset));
    __ ldrb(ip, FieldMemOperand(input, Map::kBitFieldOffset));
    __ tst(ip, Operand(1 << Map::kIsUndetectable));
    final_branch_condition = ne;

  } else if (type_name->Equals(heap()->function_string())) {
    STATIC_ASSERT(NUM_OF_CALLABLE_SPEC_OBJECT_TYPES == 2);
    __ JumpIfSmi(input, false_label);
    __ CompareObjectType(input, scratch, input, JS_FUNCTION_TYPE);
    __ b(eq, true_label);
    __ cmp(input, Operand(JS_FUNCTION_PROXY_TYPE));
    final_branch_condition = eq;

  } else if (type_name->Equals(heap()->object_string())) {
    __ JumpIfSmi(input, false_label);
    if (!FLAG_harmony_typeof) {
      __ CompareRoot(input, Heap::kNullValueRootIndex);
      __ b(eq, true_label);
    }
    __ CompareObjectType(input, input, scratch,
                         FIRST_NONCALLABLE_SPEC_OBJECT_TYPE);
    __ b(lt, false_label);
    __ CompareInstanceType(input, scratch, LAST_NONCALLABLE_SPEC_OBJECT_TYPE);
    __ b(gt, false_label);
    // Check for undetectable objects => false.
    __ ldrb(ip, FieldMemOperand(input, Map::kBitFieldOffset));
    __ tst(ip, Operand(1 << Map::kIsUndetectable));
    final_branch_condition = eq;

  } else {
    __ b(false_label);
  }

  return final_branch_condition;
}


void LCodeGen::DoIsConstructCallAndBranch(LIsConstructCallAndBranch* instr) {
  Register temp1 = ToRegister(instr->temp());

  EmitIsConstructCall(temp1, scratch0());
  EmitBranch(instr, eq);
}


void LCodeGen::EmitIsConstructCall(Register temp1, Register temp2) {
  ASSERT(!temp1.is(temp2));
  // Get the frame pointer for the calling frame.
  __ ldr(temp1, MemOperand(fp, StandardFrameConstants::kCallerFPOffset));

  // Skip the arguments adaptor frame if it exists.
  Label check_frame_marker;
  __ ldr(temp2, MemOperand(temp1, StandardFrameConstants::kContextOffset));
  __ cmp(temp2, Operand(Smi::FromInt(StackFrame::ARGUMENTS_ADAPTOR)));
  __ b(ne, &check_frame_marker);
  __ ldr(temp1, MemOperand(temp1, StandardFrameConstants::kCallerFPOffset));

  // Check the marker in the calling frame.
  __ bind(&check_frame_marker);
  __ ldr(temp1, MemOperand(temp1, StandardFrameConstants::kMarkerOffset));
  __ cmp(temp1, Operand(Smi::FromInt(StackFrame::CONSTRUCT)));
}


void LCodeGen::EnsureSpaceForLazyDeopt() {
  if (info()->IsStub()) return;
  // Ensure that we have enough space after the previous lazy-bailout
  // instruction for patching the code here.
  int current_pc = masm()->pc_offset();
  int patch_size = Deoptimizer::patch_size();
  if (current_pc < last_lazy_deopt_pc_ + patch_size) {
    // Block literal pool emission for duration of padding.
    Assembler::BlockConstPoolScope block_const_pool(masm());
    int padding_size = last_lazy_deopt_pc_ + patch_size - current_pc;
    ASSERT_EQ(0, padding_size % Assembler::kInstrSize);
    while (padding_size > 0) {
      __ nop();
      padding_size -= Assembler::kInstrSize;
    }
  }
}


void LCodeGen::DoLazyBailout(LLazyBailout* instr) {
  EnsureSpaceForLazyDeopt();
  last_lazy_deopt_pc_ = masm()->pc_offset();
  ASSERT(instr->HasEnvironment());
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

  Comment(";;; deoptimize: %s", instr->hydrogen()->reason());
  DeoptimizeIf(al, instr->environment(), type);
}


void LCodeGen::DoDummyUse(LDummyUse* instr) {
  // Nothing to see here, move on!
}


void LCodeGen::DoDeferredStackCheck(LStackCheck* instr) {
  PushSafepointRegistersScope scope(this, Safepoint::kWithRegisters);
  __ CallRuntimeSaveDoubles(Runtime::kStackGuard);
  RecordSafepointWithLazyDeopt(
      instr, RECORD_SAFEPOINT_WITH_REGISTERS_AND_NO_ARGUMENTS);
  ASSERT(instr->HasEnvironment());
  LEnvironment* env = instr->environment();
  safepoints_.RecordLazyDeoptimizationIndex(env->deoptimization_index());
}


void LCodeGen::DoStackCheck(LStackCheck* instr) {
  class DeferredStackCheck V8_FINAL : public LDeferredCode {
   public:
    DeferredStackCheck(LCodeGen* codegen, LStackCheck* instr)
        : LDeferredCode(codegen), instr_(instr) { }
    virtual void Generate() V8_OVERRIDE {
      codegen()->DoDeferredStackCheck(instr_);
    }
    virtual LInstruction* instr() V8_OVERRIDE { return instr_; }
   private:
    LStackCheck* instr_;
  };

  ASSERT(instr->HasEnvironment());
  LEnvironment* env = instr->environment();
  // There is no LLazyBailout instruction for stack-checks. We have to
  // prepare for lazy deoptimization explicitly here.
  if (instr->hydrogen()->is_function_entry()) {
    // Perform stack overflow check.
    Label done;
    __ LoadRoot(ip, Heap::kStackLimitRootIndex);
    __ cmp(sp, Operand(ip));
    __ b(hs, &done);
    PredictableCodeSizeScope predictable(masm_, 2 * Assembler::kInstrSize);
    CallCode(isolate()->builtins()->StackCheck(),
             RelocInfo::CODE_TARGET,
             instr);
    EnsureSpaceForLazyDeopt();
    last_lazy_deopt_pc_ = masm()->pc_offset();
    __ bind(&done);
    RegisterEnvironmentForDeoptimization(env, Safepoint::kLazyDeopt);
    safepoints_.RecordLazyDeoptimizationIndex(env->deoptimization_index());
  } else {
    ASSERT(instr->hydrogen()->is_backwards_branch());
    // Perform stack overflow check if this goto needs it before jumping.
    DeferredStackCheck* deferred_stack_check =
        new(zone()) DeferredStackCheck(this, instr);
    __ LoadRoot(ip, Heap::kStackLimitRootIndex);
    __ cmp(sp, Operand(ip));
    __ b(lo, deferred_stack_check->entry());
    EnsureSpaceForLazyDeopt();
    last_lazy_deopt_pc_ = masm()->pc_offset();
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
  ASSERT(!environment->HasBeenRegistered());
  RegisterEnvironmentForDeoptimization(environment, Safepoint::kNoLazyDeopt);

  GenerateOsrPrologue();
}


void LCodeGen::DoForInPrepareMap(LForInPrepareMap* instr) {
  __ LoadRoot(ip, Heap::kUndefinedValueRootIndex);
  __ cmp(r0, ip);
  DeoptimizeIf(eq, instr->environment());

  Register null_value = r5;
  __ LoadRoot(null_value, Heap::kNullValueRootIndex);
  __ cmp(r0, null_value);
  DeoptimizeIf(eq, instr->environment());

  __ SmiTst(r0);
  DeoptimizeIf(eq, instr->environment());

  STATIC_ASSERT(FIRST_JS_PROXY_TYPE == FIRST_SPEC_OBJECT_TYPE);
  __ CompareObjectType(r0, r1, r1, LAST_JS_PROXY_TYPE);
  DeoptimizeIf(le, instr->environment());

  Label use_cache, call_runtime;
  __ CheckEnumCache(null_value, &call_runtime);

  __ ldr(r0, FieldMemOperand(r0, HeapObject::kMapOffset));
  __ b(&use_cache);

  // Get the set of properties to enumerate.
  __ bind(&call_runtime);
  __ push(r0);
  CallRuntime(Runtime::kGetPropertyNamesFast, 1, instr);

  __ ldr(r1, FieldMemOperand(r0, HeapObject::kMapOffset));
  __ LoadRoot(ip, Heap::kMetaMapRootIndex);
  __ cmp(r1, ip);
  DeoptimizeIf(ne, instr->environment());
  __ bind(&use_cache);
}


void LCodeGen::DoForInCacheArray(LForInCacheArray* instr) {
  Register map = ToRegister(instr->map());
  Register result = ToRegister(instr->result());
  Label load_cache, done;
  __ EnumLength(result, map);
  __ cmp(result, Operand(Smi::FromInt(0)));
  __ b(ne, &load_cache);
  __ mov(result, Operand(isolate()->factory()->empty_fixed_array()));
  __ jmp(&done);

  __ bind(&load_cache);
  __ LoadInstanceDescriptors(map, result);
  __ ldr(result,
         FieldMemOperand(result, DescriptorArray::kEnumCacheOffset));
  __ ldr(result,
         FieldMemOperand(result, FixedArray::SizeFor(instr->idx())));
  __ cmp(result, Operand::Zero());
  DeoptimizeIf(eq, instr->environment());

  __ bind(&done);
}


void LCodeGen::DoCheckMapValue(LCheckMapValue* instr) {
  Register object = ToRegister(instr->value());
  Register map = ToRegister(instr->map());
  __ ldr(scratch0(), FieldMemOperand(object, HeapObject::kMapOffset));
  __ cmp(map, scratch0());
  DeoptimizeIf(ne, instr->environment());
}


void LCodeGen::DoLoadFieldByIndex(LLoadFieldByIndex* instr) {
  Register object = ToRegister(instr->object());
  Register index = ToRegister(instr->index());
  Register result = ToRegister(instr->result());
  Register scratch = scratch0();

  Label out_of_object, done;
  __ cmp(index, Operand::Zero());
  __ b(lt, &out_of_object);

  __ add(scratch, object, Operand::PointerOffsetFromSmiKey(index));
  __ ldr(result, FieldMemOperand(scratch, JSObject::kHeaderSize));

  __ b(&done);

  __ bind(&out_of_object);
  __ ldr(result, FieldMemOperand(object, JSObject::kPropertiesOffset));
  // Index is equal to negated out of object property index plus 1.
  STATIC_ASSERT(kSmiTag == 0 && kSmiTagSize < kPointerSizeLog2);
  __ sub(scratch, result, Operand::PointerOffsetFromSmiKey(index));
  __ ldr(result, FieldMemOperand(scratch,
                                 FixedArray::kHeaderSize - kPointerSize));
  __ bind(&done);
}


#undef __

} }  // namespace v8::internal
