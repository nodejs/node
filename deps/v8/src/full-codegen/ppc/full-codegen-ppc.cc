// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if V8_TARGET_ARCH_PPC

#include "src/ast/scopes.h"
#include "src/code-factory.h"
#include "src/code-stubs.h"
#include "src/codegen.h"
#include "src/debug/debug.h"
#include "src/full-codegen/full-codegen.h"
#include "src/ic/ic.h"
#include "src/parsing/parser.h"

#include "src/ppc/code-stubs-ppc.h"
#include "src/ppc/macro-assembler-ppc.h"

namespace v8 {
namespace internal {

#define __ ACCESS_MASM(masm_)

// A patch site is a location in the code which it is possible to patch. This
// class has a number of methods to emit the code which is patchable and the
// method EmitPatchInfo to record a marker back to the patchable code. This
// marker is a cmpi rx, #yyy instruction, and x * 0x0000ffff + yyy (raw 16 bit
// immediate value is used) is the delta from the pc to the first instruction of
// the patchable code.
// See PatchInlinedSmiCode in ic-ppc.cc for the code that patches it
class JumpPatchSite BASE_EMBEDDED {
 public:
  explicit JumpPatchSite(MacroAssembler* masm) : masm_(masm) {
#ifdef DEBUG
    info_emitted_ = false;
#endif
  }

  ~JumpPatchSite() { DCHECK(patch_site_.is_bound() == info_emitted_); }

  // When initially emitting this ensure that a jump is always generated to skip
  // the inlined smi code.
  void EmitJumpIfNotSmi(Register reg, Label* target) {
    DCHECK(!patch_site_.is_bound() && !info_emitted_);
    Assembler::BlockTrampolinePoolScope block_trampoline_pool(masm_);
    __ bind(&patch_site_);
    __ cmp(reg, reg, cr0);
    __ beq(target, cr0);  // Always taken before patched.
  }

  // When initially emitting this ensure that a jump is never generated to skip
  // the inlined smi code.
  void EmitJumpIfSmi(Register reg, Label* target) {
    Assembler::BlockTrampolinePoolScope block_trampoline_pool(masm_);
    DCHECK(!patch_site_.is_bound() && !info_emitted_);
    __ bind(&patch_site_);
    __ cmp(reg, reg, cr0);
    __ bne(target, cr0);  // Never taken before patched.
  }

  void EmitPatchInfo() {
    if (patch_site_.is_bound()) {
      int delta_to_patch_site = masm_->InstructionsGeneratedSince(&patch_site_);
      Register reg;
      // I believe this is using reg as the high bits of of the offset
      reg.set_code(delta_to_patch_site / kOff16Mask);
      __ cmpi(reg, Operand(delta_to_patch_site % kOff16Mask));
#ifdef DEBUG
      info_emitted_ = true;
#endif
    } else {
      __ nop();  // Signals no inlined code.
    }
  }

 private:
  MacroAssembler* masm_;
  Label patch_site_;
#ifdef DEBUG
  bool info_emitted_;
#endif
};


// Generate code for a JS function.  On entry to the function the receiver
// and arguments have been pushed on the stack left to right.  The actual
// argument count matches the formal parameter count expected by the
// function.
//
// The live registers are:
//   o r4: the JS function object being called (i.e., ourselves)
//   o r6: the new target value
//   o cp: our context
//   o fp: our caller's frame pointer (aka r31)
//   o sp: stack pointer
//   o lr: return address
//   o ip: our own function entry (required by the prologue)
//
// The function builds a JS frame.  Please see JavaScriptFrameConstants in
// frames-ppc.h for its layout.
void FullCodeGenerator::Generate() {
  CompilationInfo* info = info_;
  profiling_counter_ = isolate()->factory()->NewCell(
      Handle<Smi>(Smi::FromInt(FLAG_interrupt_budget), isolate()));
  SetFunctionPosition(literal());
  Comment cmnt(masm_, "[ function compiled by full code generator");

  ProfileEntryHookStub::MaybeCallEntryHook(masm_);

#ifdef DEBUG
  if (strlen(FLAG_stop_at) > 0 &&
      info->literal()->name()->IsUtf8EqualTo(CStrVector(FLAG_stop_at))) {
    __ stop("stop-at");
  }
#endif

  if (FLAG_debug_code && info->ExpectsJSReceiverAsReceiver()) {
    int receiver_offset = info->scope()->num_parameters() * kPointerSize;
    __ LoadP(r5, MemOperand(sp, receiver_offset), r0);
    __ AssertNotSmi(r5);
    __ CompareObjectType(r5, r5, no_reg, FIRST_JS_RECEIVER_TYPE);
    __ Assert(ge, kSloppyFunctionExpectsJSReceiverReceiver);
  }

  // Open a frame scope to indicate that there is a frame on the stack.  The
  // MANUAL indicates that the scope shouldn't actually generate code to set up
  // the frame (that is done below).
  FrameScope frame_scope(masm_, StackFrame::MANUAL);
  int prologue_offset = masm_->pc_offset();

  if (prologue_offset) {
    // Prologue logic requires it's starting address in ip and the
    // corresponding offset from the function entry.
    prologue_offset += Instruction::kInstrSize;
    __ addi(ip, ip, Operand(prologue_offset));
  }
  info->set_prologue_offset(prologue_offset);
  __ Prologue(info->GeneratePreagedPrologue(), ip, prologue_offset);

  {
    Comment cmnt(masm_, "[ Allocate locals");
    int locals_count = info->scope()->num_stack_slots();
    // Generators allocate locals, if any, in context slots.
    DCHECK(!IsGeneratorFunction(info->literal()->kind()) || locals_count == 0);
    if (locals_count > 0) {
      if (locals_count >= 128) {
        Label ok;
        __ Add(ip, sp, -(locals_count * kPointerSize), r0);
        __ LoadRoot(r5, Heap::kRealStackLimitRootIndex);
        __ cmpl(ip, r5);
        __ bc_short(ge, &ok);
        __ CallRuntime(Runtime::kThrowStackOverflow);
        __ bind(&ok);
      }
      __ LoadRoot(ip, Heap::kUndefinedValueRootIndex);
      int kMaxPushes = FLAG_optimize_for_size ? 4 : 32;
      if (locals_count >= kMaxPushes) {
        int loop_iterations = locals_count / kMaxPushes;
        __ mov(r5, Operand(loop_iterations));
        __ mtctr(r5);
        Label loop_header;
        __ bind(&loop_header);
        // Do pushes.
        for (int i = 0; i < kMaxPushes; i++) {
          __ push(ip);
        }
        // Continue loop if not done.
        __ bdnz(&loop_header);
      }
      int remaining = locals_count % kMaxPushes;
      // Emit the remaining pushes.
      for (int i = 0; i < remaining; i++) {
        __ push(ip);
      }
    }
  }

  bool function_in_register_r4 = true;

  // Possibly allocate a local context.
  if (info->scope()->num_heap_slots() > 0) {
    // Argument to NewContext is the function, which is still in r4.
    Comment cmnt(masm_, "[ Allocate context");
    bool need_write_barrier = true;
    int slots = info->scope()->num_heap_slots() - Context::MIN_CONTEXT_SLOTS;
    if (info->scope()->is_script_scope()) {
      __ push(r4);
      __ Push(info->scope()->GetScopeInfo(info->isolate()));
      __ CallRuntime(Runtime::kNewScriptContext);
      PrepareForBailoutForId(BailoutId::ScriptContext(), TOS_REG);
      // The new target value is not used, clobbering is safe.
      DCHECK_NULL(info->scope()->new_target_var());
    } else {
      if (info->scope()->new_target_var() != nullptr) {
        __ push(r6);  // Preserve new target.
      }
      if (slots <= FastNewContextStub::kMaximumSlots) {
        FastNewContextStub stub(isolate(), slots);
        __ CallStub(&stub);
        // Result of FastNewContextStub is always in new space.
        need_write_barrier = false;
      } else {
        __ push(r4);
        __ CallRuntime(Runtime::kNewFunctionContext);
      }
      if (info->scope()->new_target_var() != nullptr) {
        __ pop(r6);  // Preserve new target.
      }
    }
    function_in_register_r4 = false;
    // Context is returned in r3.  It replaces the context passed to us.
    // It's saved in the stack and kept live in cp.
    __ mr(cp, r3);
    __ StoreP(r3, MemOperand(fp, StandardFrameConstants::kContextOffset));
    // Copy any necessary parameters into the context.
    int num_parameters = info->scope()->num_parameters();
    int first_parameter = info->scope()->has_this_declaration() ? -1 : 0;
    for (int i = first_parameter; i < num_parameters; i++) {
      Variable* var = (i == -1) ? scope()->receiver() : scope()->parameter(i);
      if (var->IsContextSlot()) {
        int parameter_offset = StandardFrameConstants::kCallerSPOffset +
                               (num_parameters - 1 - i) * kPointerSize;
        // Load parameter from stack.
        __ LoadP(r3, MemOperand(fp, parameter_offset), r0);
        // Store it in the context.
        MemOperand target = ContextMemOperand(cp, var->index());
        __ StoreP(r3, target, r0);

        // Update the write barrier.
        if (need_write_barrier) {
          __ RecordWriteContextSlot(cp, target.offset(), r3, r5,
                                    kLRHasBeenSaved, kDontSaveFPRegs);
        } else if (FLAG_debug_code) {
          Label done;
          __ JumpIfInNewSpace(cp, r3, &done);
          __ Abort(kExpectedNewSpaceObject);
          __ bind(&done);
        }
      }
    }
  }

  // Register holding this function and new target are both trashed in case we
  // bailout here. But since that can happen only when new target is not used
  // and we allocate a context, the value of |function_in_register| is correct.
  PrepareForBailoutForId(BailoutId::FunctionContext(), NO_REGISTERS);

  // Possibly set up a local binding to the this function which is used in
  // derived constructors with super calls.
  Variable* this_function_var = scope()->this_function_var();
  if (this_function_var != nullptr) {
    Comment cmnt(masm_, "[ This function");
    if (!function_in_register_r4) {
      __ LoadP(r4, MemOperand(fp, JavaScriptFrameConstants::kFunctionOffset));
      // The write barrier clobbers register again, keep it marked as such.
    }
    SetVar(this_function_var, r4, r3, r5);
  }

  // Possibly set up a local binding to the new target value.
  Variable* new_target_var = scope()->new_target_var();
  if (new_target_var != nullptr) {
    Comment cmnt(masm_, "[ new.target");
    SetVar(new_target_var, r6, r3, r5);
  }

  // Possibly allocate RestParameters
  int rest_index;
  Variable* rest_param = scope()->rest_parameter(&rest_index);
  if (rest_param) {
    Comment cmnt(masm_, "[ Allocate rest parameter array");

    int num_parameters = info->scope()->num_parameters();
    int offset = num_parameters * kPointerSize;

    __ LoadSmiLiteral(RestParamAccessDescriptor::parameter_count(),
                      Smi::FromInt(num_parameters));
    __ addi(RestParamAccessDescriptor::parameter_pointer(), fp,
            Operand(StandardFrameConstants::kCallerSPOffset + offset));
    __ LoadSmiLiteral(RestParamAccessDescriptor::rest_parameter_index(),
                      Smi::FromInt(rest_index));
    function_in_register_r4 = false;

    RestParamAccessStub stub(isolate());
    __ CallStub(&stub);

    SetVar(rest_param, r3, r4, r5);
  }

  Variable* arguments = scope()->arguments();
  if (arguments != NULL) {
    // Function uses arguments object.
    Comment cmnt(masm_, "[ Allocate arguments object");
    DCHECK(r4.is(ArgumentsAccessNewDescriptor::function()));
    if (!function_in_register_r4) {
      // Load this again, if it's used by the local context below.
      __ LoadP(r4, MemOperand(fp, JavaScriptFrameConstants::kFunctionOffset));
    }
    // Receiver is just before the parameters on the caller's stack.
    int num_parameters = info->scope()->num_parameters();
    int offset = num_parameters * kPointerSize;
    __ LoadSmiLiteral(ArgumentsAccessNewDescriptor::parameter_count(),
                      Smi::FromInt(num_parameters));
    __ addi(ArgumentsAccessNewDescriptor::parameter_pointer(), fp,
            Operand(StandardFrameConstants::kCallerSPOffset + offset));

    // Arguments to ArgumentsAccessStub:
    //   function, parameter pointer, parameter count.
    // The stub will rewrite parameter pointer and parameter count if the
    // previous stack frame was an arguments adapter frame.
    bool is_unmapped = is_strict(language_mode()) || !has_simple_parameters();
    ArgumentsAccessStub::Type type = ArgumentsAccessStub::ComputeType(
        is_unmapped, literal()->has_duplicate_parameters());
    ArgumentsAccessStub stub(isolate(), type);
    __ CallStub(&stub);

    SetVar(arguments, r3, r4, r5);
  }

  if (FLAG_trace) {
    __ CallRuntime(Runtime::kTraceEnter);
  }

  // Visit the declarations and body unless there is an illegal
  // redeclaration.
  if (scope()->HasIllegalRedeclaration()) {
    Comment cmnt(masm_, "[ Declarations");
    VisitForEffect(scope()->GetIllegalRedeclaration());

  } else {
    PrepareForBailoutForId(BailoutId::FunctionEntry(), NO_REGISTERS);
    {
      Comment cmnt(masm_, "[ Declarations");
      VisitDeclarations(scope()->declarations());
    }

    // Assert that the declarations do not use ICs. Otherwise the debugger
    // won't be able to redirect a PC at an IC to the correct IC in newly
    // recompiled code.
    DCHECK_EQ(0, ic_total_count_);

    {
      Comment cmnt(masm_, "[ Stack check");
      PrepareForBailoutForId(BailoutId::Declarations(), NO_REGISTERS);
      Label ok;
      __ LoadRoot(ip, Heap::kStackLimitRootIndex);
      __ cmpl(sp, ip);
      __ bc_short(ge, &ok);
      __ Call(isolate()->builtins()->StackCheck(), RelocInfo::CODE_TARGET);
      __ bind(&ok);
    }

    {
      Comment cmnt(masm_, "[ Body");
      DCHECK(loop_depth() == 0);
      VisitStatements(literal()->body());
      DCHECK(loop_depth() == 0);
    }
  }

  // Always emit a 'return undefined' in case control fell off the end of
  // the body.
  {
    Comment cmnt(masm_, "[ return <undefined>;");
    __ LoadRoot(r3, Heap::kUndefinedValueRootIndex);
  }
  EmitReturnSequence();

  if (HasStackOverflow()) {
    masm_->AbortConstantPoolBuilding();
  }
}


void FullCodeGenerator::ClearAccumulator() {
  __ LoadSmiLiteral(r3, Smi::FromInt(0));
}


void FullCodeGenerator::EmitProfilingCounterDecrement(int delta) {
  __ mov(r5, Operand(profiling_counter_));
  __ LoadP(r6, FieldMemOperand(r5, Cell::kValueOffset));
  __ SubSmiLiteral(r6, r6, Smi::FromInt(delta), r0);
  __ StoreP(r6, FieldMemOperand(r5, Cell::kValueOffset), r0);
}


void FullCodeGenerator::EmitProfilingCounterReset() {
  int reset_value = FLAG_interrupt_budget;
  __ mov(r5, Operand(profiling_counter_));
  __ LoadSmiLiteral(r6, Smi::FromInt(reset_value));
  __ StoreP(r6, FieldMemOperand(r5, Cell::kValueOffset), r0);
}


void FullCodeGenerator::EmitBackEdgeBookkeeping(IterationStatement* stmt,
                                                Label* back_edge_target) {
  Comment cmnt(masm_, "[ Back edge bookkeeping");
  Label ok;

  DCHECK(back_edge_target->is_bound());
  int distance = masm_->SizeOfCodeGeneratedSince(back_edge_target) +
                 kCodeSizeMultiplier / 2;
  int weight = Min(kMaxBackEdgeWeight, Max(1, distance / kCodeSizeMultiplier));
  EmitProfilingCounterDecrement(weight);
  {
    Assembler::BlockTrampolinePoolScope block_trampoline_pool(masm_);
    Assembler::BlockConstantPoolEntrySharingScope prevent_entry_sharing(masm_);
    // BackEdgeTable::PatchAt manipulates this sequence.
    __ cmpi(r6, Operand::Zero());
    __ bc_short(ge, &ok);
    __ Call(isolate()->builtins()->InterruptCheck(), RelocInfo::CODE_TARGET);

    // Record a mapping of this PC offset to the OSR id.  This is used to find
    // the AST id from the unoptimized code in order to use it as a key into
    // the deoptimization input data found in the optimized code.
    RecordBackEdge(stmt->OsrEntryId());
  }
  EmitProfilingCounterReset();

  __ bind(&ok);
  PrepareForBailoutForId(stmt->EntryId(), NO_REGISTERS);
  // Record a mapping of the OSR id to this PC.  This is used if the OSR
  // entry becomes the target of a bailout.  We don't expect it to be, but
  // we want it to work if it is.
  PrepareForBailoutForId(stmt->OsrEntryId(), NO_REGISTERS);
}


void FullCodeGenerator::EmitReturnSequence() {
  Comment cmnt(masm_, "[ Return sequence");
  if (return_label_.is_bound()) {
    __ b(&return_label_);
  } else {
    __ bind(&return_label_);
    if (FLAG_trace) {
      // Push the return value on the stack as the parameter.
      // Runtime::TraceExit returns its parameter in r3
      __ push(r3);
      __ CallRuntime(Runtime::kTraceExit);
    }
    // Pretend that the exit is a backwards jump to the entry.
    int weight = 1;
    if (info_->ShouldSelfOptimize()) {
      weight = FLAG_interrupt_budget / FLAG_self_opt_count;
    } else {
      int distance = masm_->pc_offset() + kCodeSizeMultiplier / 2;
      weight = Min(kMaxBackEdgeWeight, Max(1, distance / kCodeSizeMultiplier));
    }
    EmitProfilingCounterDecrement(weight);
    Label ok;
    __ cmpi(r6, Operand::Zero());
    __ bge(&ok);
    __ push(r3);
    __ Call(isolate()->builtins()->InterruptCheck(), RelocInfo::CODE_TARGET);
    __ pop(r3);
    EmitProfilingCounterReset();
    __ bind(&ok);

    // Make sure that the constant pool is not emitted inside of the return
    // sequence.
    {
      Assembler::BlockTrampolinePoolScope block_trampoline_pool(masm_);
      int32_t arg_count = info_->scope()->num_parameters() + 1;
      int32_t sp_delta = arg_count * kPointerSize;
      SetReturnPosition(literal());
      __ LeaveFrame(StackFrame::JAVA_SCRIPT, sp_delta);
      __ blr();
    }
  }
}


void FullCodeGenerator::StackValueContext::Plug(Variable* var) const {
  DCHECK(var->IsStackAllocated() || var->IsContextSlot());
  codegen()->GetVar(result_register(), var);
  __ push(result_register());
}


void FullCodeGenerator::EffectContext::Plug(Heap::RootListIndex index) const {}


void FullCodeGenerator::AccumulatorValueContext::Plug(
    Heap::RootListIndex index) const {
  __ LoadRoot(result_register(), index);
}


void FullCodeGenerator::StackValueContext::Plug(
    Heap::RootListIndex index) const {
  __ LoadRoot(result_register(), index);
  __ push(result_register());
}


void FullCodeGenerator::TestContext::Plug(Heap::RootListIndex index) const {
  codegen()->PrepareForBailoutBeforeSplit(condition(), true, true_label_,
                                          false_label_);
  if (index == Heap::kUndefinedValueRootIndex ||
      index == Heap::kNullValueRootIndex ||
      index == Heap::kFalseValueRootIndex) {
    if (false_label_ != fall_through_) __ b(false_label_);
  } else if (index == Heap::kTrueValueRootIndex) {
    if (true_label_ != fall_through_) __ b(true_label_);
  } else {
    __ LoadRoot(result_register(), index);
    codegen()->DoTest(this);
  }
}


void FullCodeGenerator::EffectContext::Plug(Handle<Object> lit) const {}


void FullCodeGenerator::AccumulatorValueContext::Plug(
    Handle<Object> lit) const {
  __ mov(result_register(), Operand(lit));
}


void FullCodeGenerator::StackValueContext::Plug(Handle<Object> lit) const {
  // Immediates cannot be pushed directly.
  __ mov(result_register(), Operand(lit));
  __ push(result_register());
}


void FullCodeGenerator::TestContext::Plug(Handle<Object> lit) const {
  codegen()->PrepareForBailoutBeforeSplit(condition(), true, true_label_,
                                          false_label_);
  DCHECK(!lit->IsUndetectableObject());  // There are no undetectable literals.
  if (lit->IsUndefined() || lit->IsNull() || lit->IsFalse()) {
    if (false_label_ != fall_through_) __ b(false_label_);
  } else if (lit->IsTrue() || lit->IsJSObject()) {
    if (true_label_ != fall_through_) __ b(true_label_);
  } else if (lit->IsString()) {
    if (String::cast(*lit)->length() == 0) {
      if (false_label_ != fall_through_) __ b(false_label_);
    } else {
      if (true_label_ != fall_through_) __ b(true_label_);
    }
  } else if (lit->IsSmi()) {
    if (Smi::cast(*lit)->value() == 0) {
      if (false_label_ != fall_through_) __ b(false_label_);
    } else {
      if (true_label_ != fall_through_) __ b(true_label_);
    }
  } else {
    // For simplicity we always test the accumulator register.
    __ mov(result_register(), Operand(lit));
    codegen()->DoTest(this);
  }
}


void FullCodeGenerator::EffectContext::DropAndPlug(int count,
                                                   Register reg) const {
  DCHECK(count > 0);
  __ Drop(count);
}


void FullCodeGenerator::AccumulatorValueContext::DropAndPlug(
    int count, Register reg) const {
  DCHECK(count > 0);
  __ Drop(count);
  __ Move(result_register(), reg);
}


void FullCodeGenerator::StackValueContext::DropAndPlug(int count,
                                                       Register reg) const {
  DCHECK(count > 0);
  if (count > 1) __ Drop(count - 1);
  __ StoreP(reg, MemOperand(sp, 0));
}


void FullCodeGenerator::TestContext::DropAndPlug(int count,
                                                 Register reg) const {
  DCHECK(count > 0);
  // For simplicity we always test the accumulator register.
  __ Drop(count);
  __ Move(result_register(), reg);
  codegen()->PrepareForBailoutBeforeSplit(condition(), false, NULL, NULL);
  codegen()->DoTest(this);
}


void FullCodeGenerator::EffectContext::Plug(Label* materialize_true,
                                            Label* materialize_false) const {
  DCHECK(materialize_true == materialize_false);
  __ bind(materialize_true);
}


void FullCodeGenerator::AccumulatorValueContext::Plug(
    Label* materialize_true, Label* materialize_false) const {
  Label done;
  __ bind(materialize_true);
  __ LoadRoot(result_register(), Heap::kTrueValueRootIndex);
  __ b(&done);
  __ bind(materialize_false);
  __ LoadRoot(result_register(), Heap::kFalseValueRootIndex);
  __ bind(&done);
}


void FullCodeGenerator::StackValueContext::Plug(
    Label* materialize_true, Label* materialize_false) const {
  Label done;
  __ bind(materialize_true);
  __ LoadRoot(ip, Heap::kTrueValueRootIndex);
  __ b(&done);
  __ bind(materialize_false);
  __ LoadRoot(ip, Heap::kFalseValueRootIndex);
  __ bind(&done);
  __ push(ip);
}


void FullCodeGenerator::TestContext::Plug(Label* materialize_true,
                                          Label* materialize_false) const {
  DCHECK(materialize_true == true_label_);
  DCHECK(materialize_false == false_label_);
}


void FullCodeGenerator::AccumulatorValueContext::Plug(bool flag) const {
  Heap::RootListIndex value_root_index =
      flag ? Heap::kTrueValueRootIndex : Heap::kFalseValueRootIndex;
  __ LoadRoot(result_register(), value_root_index);
}


void FullCodeGenerator::StackValueContext::Plug(bool flag) const {
  Heap::RootListIndex value_root_index =
      flag ? Heap::kTrueValueRootIndex : Heap::kFalseValueRootIndex;
  __ LoadRoot(ip, value_root_index);
  __ push(ip);
}


void FullCodeGenerator::TestContext::Plug(bool flag) const {
  codegen()->PrepareForBailoutBeforeSplit(condition(), true, true_label_,
                                          false_label_);
  if (flag) {
    if (true_label_ != fall_through_) __ b(true_label_);
  } else {
    if (false_label_ != fall_through_) __ b(false_label_);
  }
}


void FullCodeGenerator::DoTest(Expression* condition, Label* if_true,
                               Label* if_false, Label* fall_through) {
  Handle<Code> ic = ToBooleanStub::GetUninitialized(isolate());
  CallIC(ic, condition->test_id());
  __ CompareRoot(result_register(), Heap::kTrueValueRootIndex);
  Split(eq, if_true, if_false, fall_through);
}


void FullCodeGenerator::Split(Condition cond, Label* if_true, Label* if_false,
                              Label* fall_through, CRegister cr) {
  if (if_false == fall_through) {
    __ b(cond, if_true, cr);
  } else if (if_true == fall_through) {
    __ b(NegateCondition(cond), if_false, cr);
  } else {
    __ b(cond, if_true, cr);
    __ b(if_false);
  }
}


MemOperand FullCodeGenerator::StackOperand(Variable* var) {
  DCHECK(var->IsStackAllocated());
  // Offset is negative because higher indexes are at lower addresses.
  int offset = -var->index() * kPointerSize;
  // Adjust by a (parameter or local) base offset.
  if (var->IsParameter()) {
    offset += (info_->scope()->num_parameters() + 1) * kPointerSize;
  } else {
    offset += JavaScriptFrameConstants::kLocal0Offset;
  }
  return MemOperand(fp, offset);
}


MemOperand FullCodeGenerator::VarOperand(Variable* var, Register scratch) {
  DCHECK(var->IsContextSlot() || var->IsStackAllocated());
  if (var->IsContextSlot()) {
    int context_chain_length = scope()->ContextChainLength(var->scope());
    __ LoadContext(scratch, context_chain_length);
    return ContextMemOperand(scratch, var->index());
  } else {
    return StackOperand(var);
  }
}


void FullCodeGenerator::GetVar(Register dest, Variable* var) {
  // Use destination as scratch.
  MemOperand location = VarOperand(var, dest);
  __ LoadP(dest, location, r0);
}


void FullCodeGenerator::SetVar(Variable* var, Register src, Register scratch0,
                               Register scratch1) {
  DCHECK(var->IsContextSlot() || var->IsStackAllocated());
  DCHECK(!scratch0.is(src));
  DCHECK(!scratch0.is(scratch1));
  DCHECK(!scratch1.is(src));
  MemOperand location = VarOperand(var, scratch0);
  __ StoreP(src, location, r0);

  // Emit the write barrier code if the location is in the heap.
  if (var->IsContextSlot()) {
    __ RecordWriteContextSlot(scratch0, location.offset(), src, scratch1,
                              kLRHasBeenSaved, kDontSaveFPRegs);
  }
}


void FullCodeGenerator::PrepareForBailoutBeforeSplit(Expression* expr,
                                                     bool should_normalize,
                                                     Label* if_true,
                                                     Label* if_false) {
  // Only prepare for bailouts before splits if we're in a test
  // context. Otherwise, we let the Visit function deal with the
  // preparation to avoid preparing with the same AST id twice.
  if (!context()->IsTest()) return;

  Label skip;
  if (should_normalize) __ b(&skip);
  PrepareForBailout(expr, TOS_REG);
  if (should_normalize) {
    __ LoadRoot(ip, Heap::kTrueValueRootIndex);
    __ cmp(r3, ip);
    Split(eq, if_true, if_false, NULL);
    __ bind(&skip);
  }
}


void FullCodeGenerator::EmitDebugCheckDeclarationContext(Variable* variable) {
  // The variable in the declaration always resides in the current function
  // context.
  DCHECK_EQ(0, scope()->ContextChainLength(variable->scope()));
  if (generate_debug_code_) {
    // Check that we're not inside a with or catch context.
    __ LoadP(r4, FieldMemOperand(cp, HeapObject::kMapOffset));
    __ CompareRoot(r4, Heap::kWithContextMapRootIndex);
    __ Check(ne, kDeclarationInWithContext);
    __ CompareRoot(r4, Heap::kCatchContextMapRootIndex);
    __ Check(ne, kDeclarationInCatchContext);
  }
}


void FullCodeGenerator::VisitVariableDeclaration(
    VariableDeclaration* declaration) {
  // If it was not possible to allocate the variable at compile time, we
  // need to "declare" it at runtime to make sure it actually exists in the
  // local context.
  VariableProxy* proxy = declaration->proxy();
  VariableMode mode = declaration->mode();
  Variable* variable = proxy->var();
  bool hole_init = mode == LET || mode == CONST || mode == CONST_LEGACY;
  switch (variable->location()) {
    case VariableLocation::GLOBAL:
    case VariableLocation::UNALLOCATED:
      globals_->Add(variable->name(), zone());
      globals_->Add(variable->binding_needs_init()
                        ? isolate()->factory()->the_hole_value()
                        : isolate()->factory()->undefined_value(),
                    zone());
      break;

    case VariableLocation::PARAMETER:
    case VariableLocation::LOCAL:
      if (hole_init) {
        Comment cmnt(masm_, "[ VariableDeclaration");
        __ LoadRoot(ip, Heap::kTheHoleValueRootIndex);
        __ StoreP(ip, StackOperand(variable));
      }
      break;

    case VariableLocation::CONTEXT:
      if (hole_init) {
        Comment cmnt(masm_, "[ VariableDeclaration");
        EmitDebugCheckDeclarationContext(variable);
        __ LoadRoot(ip, Heap::kTheHoleValueRootIndex);
        __ StoreP(ip, ContextMemOperand(cp, variable->index()), r0);
        // No write barrier since the_hole_value is in old space.
        PrepareForBailoutForId(proxy->id(), NO_REGISTERS);
      }
      break;

    case VariableLocation::LOOKUP: {
      Comment cmnt(masm_, "[ VariableDeclaration");
      __ mov(r5, Operand(variable->name()));
      // Declaration nodes are always introduced in one of four modes.
      DCHECK(IsDeclaredVariableMode(mode));
      // Push initial value, if any.
      // Note: For variables we must not push an initial value (such as
      // 'undefined') because we may have a (legal) redeclaration and we
      // must not destroy the current value.
      if (hole_init) {
        __ LoadRoot(r3, Heap::kTheHoleValueRootIndex);
      } else {
        __ LoadSmiLiteral(r3, Smi::FromInt(0));  // Indicates no initial value.
      }
      __ Push(r5, r3);
      __ Push(Smi::FromInt(variable->DeclarationPropertyAttributes()));
      __ CallRuntime(Runtime::kDeclareLookupSlot);
      break;
    }
  }
}


void FullCodeGenerator::VisitFunctionDeclaration(
    FunctionDeclaration* declaration) {
  VariableProxy* proxy = declaration->proxy();
  Variable* variable = proxy->var();
  switch (variable->location()) {
    case VariableLocation::GLOBAL:
    case VariableLocation::UNALLOCATED: {
      globals_->Add(variable->name(), zone());
      Handle<SharedFunctionInfo> function =
          Compiler::GetSharedFunctionInfo(declaration->fun(), script(), info_);
      // Check for stack-overflow exception.
      if (function.is_null()) return SetStackOverflow();
      globals_->Add(function, zone());
      break;
    }

    case VariableLocation::PARAMETER:
    case VariableLocation::LOCAL: {
      Comment cmnt(masm_, "[ FunctionDeclaration");
      VisitForAccumulatorValue(declaration->fun());
      __ StoreP(result_register(), StackOperand(variable));
      break;
    }

    case VariableLocation::CONTEXT: {
      Comment cmnt(masm_, "[ FunctionDeclaration");
      EmitDebugCheckDeclarationContext(variable);
      VisitForAccumulatorValue(declaration->fun());
      __ StoreP(result_register(), ContextMemOperand(cp, variable->index()),
                r0);
      int offset = Context::SlotOffset(variable->index());
      // We know that we have written a function, which is not a smi.
      __ RecordWriteContextSlot(cp, offset, result_register(), r5,
                                kLRHasBeenSaved, kDontSaveFPRegs,
                                EMIT_REMEMBERED_SET, OMIT_SMI_CHECK);
      PrepareForBailoutForId(proxy->id(), NO_REGISTERS);
      break;
    }

    case VariableLocation::LOOKUP: {
      Comment cmnt(masm_, "[ FunctionDeclaration");
      __ mov(r5, Operand(variable->name()));
      __ Push(r5);
      // Push initial value for function declaration.
      VisitForStackValue(declaration->fun());
      __ Push(Smi::FromInt(variable->DeclarationPropertyAttributes()));
      __ CallRuntime(Runtime::kDeclareLookupSlot);
      break;
    }
  }
}


void FullCodeGenerator::DeclareGlobals(Handle<FixedArray> pairs) {
  // Call the runtime to declare the globals.
  __ mov(r4, Operand(pairs));
  __ LoadSmiLiteral(r3, Smi::FromInt(DeclareGlobalsFlags()));
  __ Push(r4, r3);
  __ CallRuntime(Runtime::kDeclareGlobals);
  // Return value is ignored.
}


void FullCodeGenerator::DeclareModules(Handle<FixedArray> descriptions) {
  // Call the runtime to declare the modules.
  __ Push(descriptions);
  __ CallRuntime(Runtime::kDeclareModules);
  // Return value is ignored.
}


void FullCodeGenerator::VisitSwitchStatement(SwitchStatement* stmt) {
  Comment cmnt(masm_, "[ SwitchStatement");
  Breakable nested_statement(this, stmt);
  SetStatementPosition(stmt);

  // Keep the switch value on the stack until a case matches.
  VisitForStackValue(stmt->tag());
  PrepareForBailoutForId(stmt->EntryId(), NO_REGISTERS);

  ZoneList<CaseClause*>* clauses = stmt->cases();
  CaseClause* default_clause = NULL;  // Can occur anywhere in the list.

  Label next_test;  // Recycled for each test.
  // Compile all the tests with branches to their bodies.
  for (int i = 0; i < clauses->length(); i++) {
    CaseClause* clause = clauses->at(i);
    clause->body_target()->Unuse();

    // The default is not a test, but remember it as final fall through.
    if (clause->is_default()) {
      default_clause = clause;
      continue;
    }

    Comment cmnt(masm_, "[ Case comparison");
    __ bind(&next_test);
    next_test.Unuse();

    // Compile the label expression.
    VisitForAccumulatorValue(clause->label());

    // Perform the comparison as if via '==='.
    __ LoadP(r4, MemOperand(sp, 0));  // Switch value.
    bool inline_smi_code = ShouldInlineSmiCase(Token::EQ_STRICT);
    JumpPatchSite patch_site(masm_);
    if (inline_smi_code) {
      Label slow_case;
      __ orx(r5, r4, r3);
      patch_site.EmitJumpIfNotSmi(r5, &slow_case);

      __ cmp(r4, r3);
      __ bne(&next_test);
      __ Drop(1);  // Switch value is no longer needed.
      __ b(clause->body_target());
      __ bind(&slow_case);
    }

    // Record position before stub call for type feedback.
    SetExpressionPosition(clause);
    Handle<Code> ic = CodeFactory::CompareIC(isolate(), Token::EQ_STRICT,
                                             strength(language_mode())).code();
    CallIC(ic, clause->CompareId());
    patch_site.EmitPatchInfo();

    Label skip;
    __ b(&skip);
    PrepareForBailout(clause, TOS_REG);
    __ LoadRoot(ip, Heap::kTrueValueRootIndex);
    __ cmp(r3, ip);
    __ bne(&next_test);
    __ Drop(1);
    __ b(clause->body_target());
    __ bind(&skip);

    __ cmpi(r3, Operand::Zero());
    __ bne(&next_test);
    __ Drop(1);  // Switch value is no longer needed.
    __ b(clause->body_target());
  }

  // Discard the test value and jump to the default if present, otherwise to
  // the end of the statement.
  __ bind(&next_test);
  __ Drop(1);  // Switch value is no longer needed.
  if (default_clause == NULL) {
    __ b(nested_statement.break_label());
  } else {
    __ b(default_clause->body_target());
  }

  // Compile all the case bodies.
  for (int i = 0; i < clauses->length(); i++) {
    Comment cmnt(masm_, "[ Case body");
    CaseClause* clause = clauses->at(i);
    __ bind(clause->body_target());
    PrepareForBailoutForId(clause->EntryId(), NO_REGISTERS);
    VisitStatements(clause->statements());
  }

  __ bind(nested_statement.break_label());
  PrepareForBailoutForId(stmt->ExitId(), NO_REGISTERS);
}


void FullCodeGenerator::VisitForInStatement(ForInStatement* stmt) {
  Comment cmnt(masm_, "[ ForInStatement");
  SetStatementPosition(stmt, SKIP_BREAK);

  FeedbackVectorSlot slot = stmt->ForInFeedbackSlot();

  Label loop, exit;
  ForIn loop_statement(this, stmt);
  increment_loop_depth();

  // Get the object to enumerate over. If the object is null or undefined, skip
  // over the loop.  See ECMA-262 version 5, section 12.6.4.
  SetExpressionAsStatementPosition(stmt->enumerable());
  VisitForAccumulatorValue(stmt->enumerable());
  __ LoadRoot(ip, Heap::kUndefinedValueRootIndex);
  __ cmp(r3, ip);
  __ beq(&exit);
  Register null_value = r7;
  __ LoadRoot(null_value, Heap::kNullValueRootIndex);
  __ cmp(r3, null_value);
  __ beq(&exit);

  PrepareForBailoutForId(stmt->PrepareId(), TOS_REG);

  // Convert the object to a JS object.
  Label convert, done_convert;
  __ JumpIfSmi(r3, &convert);
  __ CompareObjectType(r3, r4, r4, FIRST_JS_RECEIVER_TYPE);
  __ bge(&done_convert);
  __ bind(&convert);
  ToObjectStub stub(isolate());
  __ CallStub(&stub);
  __ bind(&done_convert);
  PrepareForBailoutForId(stmt->ToObjectId(), TOS_REG);
  __ push(r3);

  // Check for proxies.
  Label call_runtime;
  __ CompareObjectType(r3, r4, r4, JS_PROXY_TYPE);
  __ beq(&call_runtime);

  // Check cache validity in generated code. This is a fast case for
  // the JSObject::IsSimpleEnum cache validity checks. If we cannot
  // guarantee cache validity, call the runtime system to check cache
  // validity or get the property names in a fixed array.
  __ CheckEnumCache(null_value, &call_runtime);

  // The enum cache is valid.  Load the map of the object being
  // iterated over and use the cache for the iteration.
  Label use_cache;
  __ LoadP(r3, FieldMemOperand(r3, HeapObject::kMapOffset));
  __ b(&use_cache);

  // Get the set of properties to enumerate.
  __ bind(&call_runtime);
  __ push(r3);  // Duplicate the enumerable object on the stack.
  __ CallRuntime(Runtime::kGetPropertyNamesFast);
  PrepareForBailoutForId(stmt->EnumId(), TOS_REG);

  // If we got a map from the runtime call, we can do a fast
  // modification check. Otherwise, we got a fixed array, and we have
  // to do a slow check.
  Label fixed_array;
  __ LoadP(r5, FieldMemOperand(r3, HeapObject::kMapOffset));
  __ LoadRoot(ip, Heap::kMetaMapRootIndex);
  __ cmp(r5, ip);
  __ bne(&fixed_array);

  // We got a map in register r3. Get the enumeration cache from it.
  Label no_descriptors;
  __ bind(&use_cache);

  __ EnumLength(r4, r3);
  __ CmpSmiLiteral(r4, Smi::FromInt(0), r0);
  __ beq(&no_descriptors);

  __ LoadInstanceDescriptors(r3, r5);
  __ LoadP(r5, FieldMemOperand(r5, DescriptorArray::kEnumCacheOffset));
  __ LoadP(r5,
           FieldMemOperand(r5, DescriptorArray::kEnumCacheBridgeCacheOffset));

  // Set up the four remaining stack slots.
  __ push(r3);  // Map.
  __ LoadSmiLiteral(r3, Smi::FromInt(0));
  // Push enumeration cache, enumeration cache length (as smi) and zero.
  __ Push(r5, r4, r3);
  __ b(&loop);

  __ bind(&no_descriptors);
  __ Drop(1);
  __ b(&exit);

  // We got a fixed array in register r3. Iterate through that.
  __ bind(&fixed_array);

  __ EmitLoadTypeFeedbackVector(r4);
  __ mov(r5, Operand(TypeFeedbackVector::MegamorphicSentinel(isolate())));
  int vector_index = SmiFromSlot(slot)->value();
  __ StoreP(
      r5, FieldMemOperand(r4, FixedArray::OffsetOfElementAt(vector_index)), r0);
  __ LoadSmiLiteral(r4, Smi::FromInt(1));  // Smi(1) indicates slow check
  __ Push(r4, r3);  // Smi and array
  __ LoadP(r4, FieldMemOperand(r3, FixedArray::kLengthOffset));
  __ LoadSmiLiteral(r3, Smi::FromInt(0));
  __ Push(r4, r3);  // Fixed array length (as smi) and initial index.

  // Generate code for doing the condition check.
  __ bind(&loop);
  SetExpressionAsStatementPosition(stmt->each());

  // Load the current count to r3, load the length to r4.
  __ LoadP(r3, MemOperand(sp, 0 * kPointerSize));
  __ LoadP(r4, MemOperand(sp, 1 * kPointerSize));
  __ cmpl(r3, r4);  // Compare to the array length.
  __ bge(loop_statement.break_label());

  // Get the current entry of the array into register r6.
  __ LoadP(r5, MemOperand(sp, 2 * kPointerSize));
  __ addi(r5, r5, Operand(FixedArray::kHeaderSize - kHeapObjectTag));
  __ SmiToPtrArrayOffset(r6, r3);
  __ LoadPX(r6, MemOperand(r6, r5));

  // Get the expected map from the stack or a smi in the
  // permanent slow case into register r5.
  __ LoadP(r5, MemOperand(sp, 3 * kPointerSize));

  // Check if the expected map still matches that of the enumerable.
  // If not, we may have to filter the key.
  Label update_each;
  __ LoadP(r4, MemOperand(sp, 4 * kPointerSize));
  __ LoadP(r7, FieldMemOperand(r4, HeapObject::kMapOffset));
  __ cmp(r7, r5);
  __ beq(&update_each);

  // Convert the entry to a string or (smi) 0 if it isn't a property
  // any more. If the property has been removed while iterating, we
  // just skip it.
  __ Push(r4, r6);  // Enumerable and current entry.
  __ CallRuntime(Runtime::kForInFilter);
  PrepareForBailoutForId(stmt->FilterId(), TOS_REG);
  __ mr(r6, r3);
  __ LoadRoot(r0, Heap::kUndefinedValueRootIndex);
  __ cmp(r3, r0);
  __ beq(loop_statement.continue_label());

  // Update the 'each' property or variable from the possibly filtered
  // entry in register r6.
  __ bind(&update_each);
  __ mr(result_register(), r6);
  // Perform the assignment as if via '='.
  {
    EffectContext context(this);
    EmitAssignment(stmt->each(), stmt->EachFeedbackSlot());
    PrepareForBailoutForId(stmt->AssignmentId(), NO_REGISTERS);
  }

  // Both Crankshaft and Turbofan expect BodyId to be right before stmt->body().
  PrepareForBailoutForId(stmt->BodyId(), NO_REGISTERS);
  // Generate code for the body of the loop.
  Visit(stmt->body());

  // Generate code for the going to the next element by incrementing
  // the index (smi) stored on top of the stack.
  __ bind(loop_statement.continue_label());
  __ pop(r3);
  __ AddSmiLiteral(r3, r3, Smi::FromInt(1), r0);
  __ push(r3);

  EmitBackEdgeBookkeeping(stmt, &loop);
  __ b(&loop);

  // Remove the pointers stored on the stack.
  __ bind(loop_statement.break_label());
  __ Drop(5);

  // Exit and decrement the loop depth.
  PrepareForBailoutForId(stmt->ExitId(), NO_REGISTERS);
  __ bind(&exit);
  decrement_loop_depth();
}


void FullCodeGenerator::EmitNewClosure(Handle<SharedFunctionInfo> info,
                                       bool pretenure) {
  // Use the fast case closure allocation code that allocates in new
  // space for nested functions that don't need literals cloning. If
  // we're running with the --always-opt or the --prepare-always-opt
  // flag, we need to use the runtime function so that the new function
  // we are creating here gets a chance to have its code optimized and
  // doesn't just get a copy of the existing unoptimized code.
  if (!FLAG_always_opt && !FLAG_prepare_always_opt && !pretenure &&
      scope()->is_function_scope() && info->num_literals() == 0) {
    FastNewClosureStub stub(isolate(), info->language_mode(), info->kind());
    __ mov(r5, Operand(info));
    __ CallStub(&stub);
  } else {
    __ Push(info);
    __ CallRuntime(pretenure ? Runtime::kNewClosure_Tenured
                             : Runtime::kNewClosure);
  }
  context()->Plug(r3);
}


void FullCodeGenerator::EmitSetHomeObject(Expression* initializer, int offset,
                                          FeedbackVectorSlot slot) {
  DCHECK(NeedsHomeObject(initializer));
  __ LoadP(StoreDescriptor::ReceiverRegister(), MemOperand(sp));
  __ mov(StoreDescriptor::NameRegister(),
         Operand(isolate()->factory()->home_object_symbol()));
  __ LoadP(StoreDescriptor::ValueRegister(),
           MemOperand(sp, offset * kPointerSize));
  EmitLoadStoreICSlot(slot);
  CallStoreIC();
}


void FullCodeGenerator::EmitSetHomeObjectAccumulator(Expression* initializer,
                                                     int offset,
                                                     FeedbackVectorSlot slot) {
  DCHECK(NeedsHomeObject(initializer));
  __ Move(StoreDescriptor::ReceiverRegister(), r3);
  __ mov(StoreDescriptor::NameRegister(),
         Operand(isolate()->factory()->home_object_symbol()));
  __ LoadP(StoreDescriptor::ValueRegister(),
           MemOperand(sp, offset * kPointerSize));
  EmitLoadStoreICSlot(slot);
  CallStoreIC();
}


void FullCodeGenerator::EmitLoadGlobalCheckExtensions(VariableProxy* proxy,
                                                      TypeofMode typeof_mode,
                                                      Label* slow) {
  Register current = cp;
  Register next = r4;
  Register temp = r5;

  Scope* s = scope();
  while (s != NULL) {
    if (s->num_heap_slots() > 0) {
      if (s->calls_sloppy_eval()) {
        // Check that extension is "the hole".
        __ LoadP(temp, ContextMemOperand(current, Context::EXTENSION_INDEX));
        __ JumpIfNotRoot(temp, Heap::kTheHoleValueRootIndex, slow);
      }
      // Load next context in chain.
      __ LoadP(next, ContextMemOperand(current, Context::PREVIOUS_INDEX));
      // Walk the rest of the chain without clobbering cp.
      current = next;
    }
    // If no outer scope calls eval, we do not need to check more
    // context extensions.
    if (!s->outer_scope_calls_sloppy_eval() || s->is_eval_scope()) break;
    s = s->outer_scope();
  }

  if (s->is_eval_scope()) {
    Label loop, fast;
    if (!current.is(next)) {
      __ Move(next, current);
    }
    __ bind(&loop);
    // Terminate at native context.
    __ LoadP(temp, FieldMemOperand(next, HeapObject::kMapOffset));
    __ LoadRoot(ip, Heap::kNativeContextMapRootIndex);
    __ cmp(temp, ip);
    __ beq(&fast);
    // Check that extension is "the hole".
    __ LoadP(temp, ContextMemOperand(next, Context::EXTENSION_INDEX));
    __ JumpIfNotRoot(temp, Heap::kTheHoleValueRootIndex, slow);
    // Load next context in chain.
    __ LoadP(next, ContextMemOperand(next, Context::PREVIOUS_INDEX));
    __ b(&loop);
    __ bind(&fast);
  }

  // All extension objects were empty and it is safe to use a normal global
  // load machinery.
  EmitGlobalVariableLoad(proxy, typeof_mode);
}


MemOperand FullCodeGenerator::ContextSlotOperandCheckExtensions(Variable* var,
                                                                Label* slow) {
  DCHECK(var->IsContextSlot());
  Register context = cp;
  Register next = r6;
  Register temp = r7;

  for (Scope* s = scope(); s != var->scope(); s = s->outer_scope()) {
    if (s->num_heap_slots() > 0) {
      if (s->calls_sloppy_eval()) {
        // Check that extension is "the hole".
        __ LoadP(temp, ContextMemOperand(context, Context::EXTENSION_INDEX));
        __ JumpIfNotRoot(temp, Heap::kTheHoleValueRootIndex, slow);
      }
      __ LoadP(next, ContextMemOperand(context, Context::PREVIOUS_INDEX));
      // Walk the rest of the chain without clobbering cp.
      context = next;
    }
  }
  // Check that last extension is "the hole".
  __ LoadP(temp, ContextMemOperand(context, Context::EXTENSION_INDEX));
  __ JumpIfNotRoot(temp, Heap::kTheHoleValueRootIndex, slow);

  // This function is used only for loads, not stores, so it's safe to
  // return an cp-based operand (the write barrier cannot be allowed to
  // destroy the cp register).
  return ContextMemOperand(context, var->index());
}


void FullCodeGenerator::EmitDynamicLookupFastCase(VariableProxy* proxy,
                                                  TypeofMode typeof_mode,
                                                  Label* slow, Label* done) {
  // Generate fast-case code for variables that might be shadowed by
  // eval-introduced variables.  Eval is used a lot without
  // introducing variables.  In those cases, we do not want to
  // perform a runtime call for all variables in the scope
  // containing the eval.
  Variable* var = proxy->var();
  if (var->mode() == DYNAMIC_GLOBAL) {
    EmitLoadGlobalCheckExtensions(proxy, typeof_mode, slow);
    __ b(done);
  } else if (var->mode() == DYNAMIC_LOCAL) {
    Variable* local = var->local_if_not_shadowed();
    __ LoadP(r3, ContextSlotOperandCheckExtensions(local, slow));
    if (local->mode() == LET || local->mode() == CONST ||
        local->mode() == CONST_LEGACY) {
      __ CompareRoot(r3, Heap::kTheHoleValueRootIndex);
      __ bne(done);
      if (local->mode() == CONST_LEGACY) {
        __ LoadRoot(r3, Heap::kUndefinedValueRootIndex);
      } else {  // LET || CONST
        __ mov(r3, Operand(var->name()));
        __ push(r3);
        __ CallRuntime(Runtime::kThrowReferenceError);
      }
    }
    __ b(done);
  }
}


void FullCodeGenerator::EmitGlobalVariableLoad(VariableProxy* proxy,
                                               TypeofMode typeof_mode) {
  Variable* var = proxy->var();
  DCHECK(var->IsUnallocatedOrGlobalSlot() ||
         (var->IsLookupSlot() && var->mode() == DYNAMIC_GLOBAL));
  __ LoadGlobalObject(LoadDescriptor::ReceiverRegister());
  __ mov(LoadDescriptor::NameRegister(), Operand(var->name()));
  __ mov(LoadDescriptor::SlotRegister(),
         Operand(SmiFromSlot(proxy->VariableFeedbackSlot())));
  CallLoadIC(typeof_mode);
}


void FullCodeGenerator::EmitVariableLoad(VariableProxy* proxy,
                                         TypeofMode typeof_mode) {
  // Record position before possible IC call.
  SetExpressionPosition(proxy);
  PrepareForBailoutForId(proxy->BeforeId(), NO_REGISTERS);
  Variable* var = proxy->var();

  // Three cases: global variables, lookup variables, and all other types of
  // variables.
  switch (var->location()) {
    case VariableLocation::GLOBAL:
    case VariableLocation::UNALLOCATED: {
      Comment cmnt(masm_, "[ Global variable");
      EmitGlobalVariableLoad(proxy, typeof_mode);
      context()->Plug(r3);
      break;
    }

    case VariableLocation::PARAMETER:
    case VariableLocation::LOCAL:
    case VariableLocation::CONTEXT: {
      DCHECK_EQ(NOT_INSIDE_TYPEOF, typeof_mode);
      Comment cmnt(masm_, var->IsContextSlot() ? "[ Context variable"
                                               : "[ Stack variable");
      if (NeedsHoleCheckForLoad(proxy)) {
        Label done;
        // Let and const need a read barrier.
        GetVar(r3, var);
        __ CompareRoot(r3, Heap::kTheHoleValueRootIndex);
        __ bne(&done);
        if (var->mode() == LET || var->mode() == CONST) {
          // Throw a reference error when using an uninitialized let/const
          // binding in harmony mode.
          __ mov(r3, Operand(var->name()));
          __ push(r3);
          __ CallRuntime(Runtime::kThrowReferenceError);
        } else {
          // Uninitialized legacy const bindings are unholed.
          DCHECK(var->mode() == CONST_LEGACY);
          __ LoadRoot(r3, Heap::kUndefinedValueRootIndex);
        }
        __ bind(&done);
        context()->Plug(r3);
        break;
      }
      context()->Plug(var);
      break;
    }

    case VariableLocation::LOOKUP: {
      Comment cmnt(masm_, "[ Lookup variable");
      Label done, slow;
      // Generate code for loading from variables potentially shadowed
      // by eval-introduced variables.
      EmitDynamicLookupFastCase(proxy, typeof_mode, &slow, &done);
      __ bind(&slow);
      __ mov(r4, Operand(var->name()));
      __ Push(cp, r4);  // Context and name.
      Runtime::FunctionId function_id =
          typeof_mode == NOT_INSIDE_TYPEOF
              ? Runtime::kLoadLookupSlot
              : Runtime::kLoadLookupSlotNoReferenceError;
      __ CallRuntime(function_id);
      __ bind(&done);
      context()->Plug(r3);
    }
  }
}


void FullCodeGenerator::VisitRegExpLiteral(RegExpLiteral* expr) {
  Comment cmnt(masm_, "[ RegExpLiteral");
  __ LoadP(r6, MemOperand(fp, JavaScriptFrameConstants::kFunctionOffset));
  __ LoadSmiLiteral(r5, Smi::FromInt(expr->literal_index()));
  __ mov(r4, Operand(expr->pattern()));
  __ LoadSmiLiteral(r3, Smi::FromInt(expr->flags()));
  FastCloneRegExpStub stub(isolate());
  __ CallStub(&stub);
  context()->Plug(r3);
}


void FullCodeGenerator::EmitAccessor(ObjectLiteralProperty* property) {
  Expression* expression = (property == NULL) ? NULL : property->value();
  if (expression == NULL) {
    __ LoadRoot(r4, Heap::kNullValueRootIndex);
    __ push(r4);
  } else {
    VisitForStackValue(expression);
    if (NeedsHomeObject(expression)) {
      DCHECK(property->kind() == ObjectLiteral::Property::GETTER ||
             property->kind() == ObjectLiteral::Property::SETTER);
      int offset = property->kind() == ObjectLiteral::Property::GETTER ? 2 : 3;
      EmitSetHomeObject(expression, offset, property->GetSlot());
    }
  }
}


void FullCodeGenerator::VisitObjectLiteral(ObjectLiteral* expr) {
  Comment cmnt(masm_, "[ ObjectLiteral");

  Handle<FixedArray> constant_properties = expr->constant_properties();
  __ LoadP(r6, MemOperand(fp, JavaScriptFrameConstants::kFunctionOffset));
  __ LoadSmiLiteral(r5, Smi::FromInt(expr->literal_index()));
  __ mov(r4, Operand(constant_properties));
  int flags = expr->ComputeFlags();
  __ LoadSmiLiteral(r3, Smi::FromInt(flags));
  if (MustCreateObjectLiteralWithRuntime(expr)) {
    __ Push(r6, r5, r4, r3);
    __ CallRuntime(Runtime::kCreateObjectLiteral);
  } else {
    FastCloneShallowObjectStub stub(isolate(), expr->properties_count());
    __ CallStub(&stub);
  }
  PrepareForBailoutForId(expr->CreateLiteralId(), TOS_REG);

  // If result_saved is true the result is on top of the stack.  If
  // result_saved is false the result is in r3.
  bool result_saved = false;

  AccessorTable accessor_table(zone());
  int property_index = 0;
  for (; property_index < expr->properties()->length(); property_index++) {
    ObjectLiteral::Property* property = expr->properties()->at(property_index);
    if (property->is_computed_name()) break;
    if (property->IsCompileTimeValue()) continue;

    Literal* key = property->key()->AsLiteral();
    Expression* value = property->value();
    if (!result_saved) {
      __ push(r3);  // Save result on stack
      result_saved = true;
    }
    switch (property->kind()) {
      case ObjectLiteral::Property::CONSTANT:
        UNREACHABLE();
      case ObjectLiteral::Property::MATERIALIZED_LITERAL:
        DCHECK(!CompileTimeValue::IsCompileTimeValue(property->value()));
      // Fall through.
      case ObjectLiteral::Property::COMPUTED:
        // It is safe to use [[Put]] here because the boilerplate already
        // contains computed properties with an uninitialized value.
        if (key->value()->IsInternalizedString()) {
          if (property->emit_store()) {
            VisitForAccumulatorValue(value);
            DCHECK(StoreDescriptor::ValueRegister().is(r3));
            __ mov(StoreDescriptor::NameRegister(), Operand(key->value()));
            __ LoadP(StoreDescriptor::ReceiverRegister(), MemOperand(sp));
            EmitLoadStoreICSlot(property->GetSlot(0));
            CallStoreIC();
            PrepareForBailoutForId(key->id(), NO_REGISTERS);

            if (NeedsHomeObject(value)) {
              EmitSetHomeObjectAccumulator(value, 0, property->GetSlot(1));
            }
          } else {
            VisitForEffect(value);
          }
          break;
        }
        // Duplicate receiver on stack.
        __ LoadP(r3, MemOperand(sp));
        __ push(r3);
        VisitForStackValue(key);
        VisitForStackValue(value);
        if (property->emit_store()) {
          if (NeedsHomeObject(value)) {
            EmitSetHomeObject(value, 2, property->GetSlot());
          }
          __ LoadSmiLiteral(r3, Smi::FromInt(SLOPPY));  // PropertyAttributes
          __ push(r3);
          __ CallRuntime(Runtime::kSetProperty);
        } else {
          __ Drop(3);
        }
        break;
      case ObjectLiteral::Property::PROTOTYPE:
        // Duplicate receiver on stack.
        __ LoadP(r3, MemOperand(sp));
        __ push(r3);
        VisitForStackValue(value);
        DCHECK(property->emit_store());
        __ CallRuntime(Runtime::kInternalSetPrototype);
        PrepareForBailoutForId(expr->GetIdForPropertySet(property_index),
                               NO_REGISTERS);
        break;
      case ObjectLiteral::Property::GETTER:
        if (property->emit_store()) {
          accessor_table.lookup(key)->second->getter = property;
        }
        break;
      case ObjectLiteral::Property::SETTER:
        if (property->emit_store()) {
          accessor_table.lookup(key)->second->setter = property;
        }
        break;
    }
  }

  // Emit code to define accessors, using only a single call to the runtime for
  // each pair of corresponding getters and setters.
  for (AccessorTable::Iterator it = accessor_table.begin();
       it != accessor_table.end(); ++it) {
    __ LoadP(r3, MemOperand(sp));  // Duplicate receiver.
    __ push(r3);
    VisitForStackValue(it->first);
    EmitAccessor(it->second->getter);
    EmitAccessor(it->second->setter);
    __ LoadSmiLiteral(r3, Smi::FromInt(NONE));
    __ push(r3);
    __ CallRuntime(Runtime::kDefineAccessorPropertyUnchecked);
  }

  // Object literals have two parts. The "static" part on the left contains no
  // computed property names, and so we can compute its map ahead of time; see
  // runtime.cc::CreateObjectLiteralBoilerplate. The second "dynamic" part
  // starts with the first computed property name, and continues with all
  // properties to its right.  All the code from above initializes the static
  // component of the object literal, and arranges for the map of the result to
  // reflect the static order in which the keys appear. For the dynamic
  // properties, we compile them into a series of "SetOwnProperty" runtime
  // calls. This will preserve insertion order.
  for (; property_index < expr->properties()->length(); property_index++) {
    ObjectLiteral::Property* property = expr->properties()->at(property_index);

    Expression* value = property->value();
    if (!result_saved) {
      __ push(r3);  // Save result on the stack
      result_saved = true;
    }

    __ LoadP(r3, MemOperand(sp));  // Duplicate receiver.
    __ push(r3);

    if (property->kind() == ObjectLiteral::Property::PROTOTYPE) {
      DCHECK(!property->is_computed_name());
      VisitForStackValue(value);
      DCHECK(property->emit_store());
      __ CallRuntime(Runtime::kInternalSetPrototype);
      PrepareForBailoutForId(expr->GetIdForPropertySet(property_index),
                             NO_REGISTERS);
    } else {
      EmitPropertyKey(property, expr->GetIdForPropertyName(property_index));
      VisitForStackValue(value);
      if (NeedsHomeObject(value)) {
        EmitSetHomeObject(value, 2, property->GetSlot());
      }

      switch (property->kind()) {
        case ObjectLiteral::Property::CONSTANT:
        case ObjectLiteral::Property::MATERIALIZED_LITERAL:
        case ObjectLiteral::Property::COMPUTED:
          if (property->emit_store()) {
            __ LoadSmiLiteral(r3, Smi::FromInt(NONE));
            __ push(r3);
            __ CallRuntime(Runtime::kDefineDataPropertyUnchecked);
          } else {
            __ Drop(3);
          }
          break;

        case ObjectLiteral::Property::PROTOTYPE:
          UNREACHABLE();
          break;

        case ObjectLiteral::Property::GETTER:
          __ mov(r3, Operand(Smi::FromInt(NONE)));
          __ push(r3);
          __ CallRuntime(Runtime::kDefineGetterPropertyUnchecked);
          break;

        case ObjectLiteral::Property::SETTER:
          __ mov(r3, Operand(Smi::FromInt(NONE)));
          __ push(r3);
          __ CallRuntime(Runtime::kDefineSetterPropertyUnchecked);
          break;
      }
    }
  }

  if (expr->has_function()) {
    DCHECK(result_saved);
    __ LoadP(r3, MemOperand(sp));
    __ push(r3);
    __ CallRuntime(Runtime::kToFastProperties);
  }

  if (result_saved) {
    context()->PlugTOS();
  } else {
    context()->Plug(r3);
  }
}


void FullCodeGenerator::VisitArrayLiteral(ArrayLiteral* expr) {
  Comment cmnt(masm_, "[ ArrayLiteral");

  Handle<FixedArray> constant_elements = expr->constant_elements();
  bool has_fast_elements =
      IsFastObjectElementsKind(expr->constant_elements_kind());
  Handle<FixedArrayBase> constant_elements_values(
      FixedArrayBase::cast(constant_elements->get(1)));

  AllocationSiteMode allocation_site_mode = TRACK_ALLOCATION_SITE;
  if (has_fast_elements && !FLAG_allocation_site_pretenuring) {
    // If the only customer of allocation sites is transitioning, then
    // we can turn it off if we don't have anywhere else to transition to.
    allocation_site_mode = DONT_TRACK_ALLOCATION_SITE;
  }

  __ LoadP(r6, MemOperand(fp, JavaScriptFrameConstants::kFunctionOffset));
  __ LoadSmiLiteral(r5, Smi::FromInt(expr->literal_index()));
  __ mov(r4, Operand(constant_elements));
  if (MustCreateArrayLiteralWithRuntime(expr)) {
    __ LoadSmiLiteral(r3, Smi::FromInt(expr->ComputeFlags()));
    __ Push(r6, r5, r4, r3);
    __ CallRuntime(Runtime::kCreateArrayLiteral);
  } else {
    FastCloneShallowArrayStub stub(isolate(), allocation_site_mode);
    __ CallStub(&stub);
  }
  PrepareForBailoutForId(expr->CreateLiteralId(), TOS_REG);

  bool result_saved = false;  // Is the result saved to the stack?
  ZoneList<Expression*>* subexprs = expr->values();
  int length = subexprs->length();

  // Emit code to evaluate all the non-constant subexpressions and to store
  // them into the newly cloned array.
  int array_index = 0;
  for (; array_index < length; array_index++) {
    Expression* subexpr = subexprs->at(array_index);
    if (subexpr->IsSpread()) break;
    // If the subexpression is a literal or a simple materialized literal it
    // is already set in the cloned array.
    if (CompileTimeValue::IsCompileTimeValue(subexpr)) continue;

    if (!result_saved) {
      __ push(r3);
      result_saved = true;
    }
    VisitForAccumulatorValue(subexpr);

    __ LoadSmiLiteral(StoreDescriptor::NameRegister(),
                      Smi::FromInt(array_index));
    __ LoadP(StoreDescriptor::ReceiverRegister(), MemOperand(sp, 0));
    EmitLoadStoreICSlot(expr->LiteralFeedbackSlot());
    Handle<Code> ic =
        CodeFactory::KeyedStoreIC(isolate(), language_mode()).code();
    CallIC(ic);

    PrepareForBailoutForId(expr->GetIdForElement(array_index), NO_REGISTERS);
  }

  // In case the array literal contains spread expressions it has two parts. The
  // first part is  the "static" array which has a literal index is  handled
  // above. The second part is the part after the first spread expression
  // (inclusive) and these elements gets appended to the array. Note that the
  // number elements an iterable produces is unknown ahead of time.
  if (array_index < length && result_saved) {
    __ Pop(r3);
    result_saved = false;
  }
  for (; array_index < length; array_index++) {
    Expression* subexpr = subexprs->at(array_index);

    __ Push(r3);
    if (subexpr->IsSpread()) {
      VisitForStackValue(subexpr->AsSpread()->expression());
      __ InvokeBuiltin(Context::CONCAT_ITERABLE_TO_ARRAY_BUILTIN_INDEX,
                       CALL_FUNCTION);
    } else {
      VisitForStackValue(subexpr);
      __ CallRuntime(Runtime::kAppendElement);
    }

    PrepareForBailoutForId(expr->GetIdForElement(array_index), NO_REGISTERS);
  }

  if (result_saved) {
    context()->PlugTOS();
  } else {
    context()->Plug(r3);
  }
}


void FullCodeGenerator::VisitAssignment(Assignment* expr) {
  DCHECK(expr->target()->IsValidReferenceExpressionOrThis());

  Comment cmnt(masm_, "[ Assignment");
  SetExpressionPosition(expr, INSERT_BREAK);

  Property* property = expr->target()->AsProperty();
  LhsKind assign_type = Property::GetAssignType(property);

  // Evaluate LHS expression.
  switch (assign_type) {
    case VARIABLE:
      // Nothing to do here.
      break;
    case NAMED_PROPERTY:
      if (expr->is_compound()) {
        // We need the receiver both on the stack and in the register.
        VisitForStackValue(property->obj());
        __ LoadP(LoadDescriptor::ReceiverRegister(), MemOperand(sp, 0));
      } else {
        VisitForStackValue(property->obj());
      }
      break;
    case NAMED_SUPER_PROPERTY:
      VisitForStackValue(
          property->obj()->AsSuperPropertyReference()->this_var());
      VisitForAccumulatorValue(
          property->obj()->AsSuperPropertyReference()->home_object());
      __ Push(result_register());
      if (expr->is_compound()) {
        const Register scratch = r4;
        __ LoadP(scratch, MemOperand(sp, kPointerSize));
        __ Push(scratch, result_register());
      }
      break;
    case KEYED_SUPER_PROPERTY: {
      const Register scratch = r4;
      VisitForStackValue(
          property->obj()->AsSuperPropertyReference()->this_var());
      VisitForAccumulatorValue(
          property->obj()->AsSuperPropertyReference()->home_object());
      __ mr(scratch, result_register());
      VisitForAccumulatorValue(property->key());
      __ Push(scratch, result_register());
      if (expr->is_compound()) {
        const Register scratch1 = r5;
        __ LoadP(scratch1, MemOperand(sp, 2 * kPointerSize));
        __ Push(scratch1, scratch, result_register());
      }
      break;
    }
    case KEYED_PROPERTY:
      if (expr->is_compound()) {
        VisitForStackValue(property->obj());
        VisitForStackValue(property->key());
        __ LoadP(LoadDescriptor::ReceiverRegister(),
                 MemOperand(sp, 1 * kPointerSize));
        __ LoadP(LoadDescriptor::NameRegister(), MemOperand(sp, 0));
      } else {
        VisitForStackValue(property->obj());
        VisitForStackValue(property->key());
      }
      break;
  }

  // For compound assignments we need another deoptimization point after the
  // variable/property load.
  if (expr->is_compound()) {
    {
      AccumulatorValueContext context(this);
      switch (assign_type) {
        case VARIABLE:
          EmitVariableLoad(expr->target()->AsVariableProxy());
          PrepareForBailout(expr->target(), TOS_REG);
          break;
        case NAMED_PROPERTY:
          EmitNamedPropertyLoad(property);
          PrepareForBailoutForId(property->LoadId(), TOS_REG);
          break;
        case NAMED_SUPER_PROPERTY:
          EmitNamedSuperPropertyLoad(property);
          PrepareForBailoutForId(property->LoadId(), TOS_REG);
          break;
        case KEYED_SUPER_PROPERTY:
          EmitKeyedSuperPropertyLoad(property);
          PrepareForBailoutForId(property->LoadId(), TOS_REG);
          break;
        case KEYED_PROPERTY:
          EmitKeyedPropertyLoad(property);
          PrepareForBailoutForId(property->LoadId(), TOS_REG);
          break;
      }
    }

    Token::Value op = expr->binary_op();
    __ push(r3);  // Left operand goes on the stack.
    VisitForAccumulatorValue(expr->value());

    AccumulatorValueContext context(this);
    if (ShouldInlineSmiCase(op)) {
      EmitInlineSmiBinaryOp(expr->binary_operation(), op, expr->target(),
                            expr->value());
    } else {
      EmitBinaryOp(expr->binary_operation(), op);
    }

    // Deoptimization point in case the binary operation may have side effects.
    PrepareForBailout(expr->binary_operation(), TOS_REG);
  } else {
    VisitForAccumulatorValue(expr->value());
  }

  SetExpressionPosition(expr);

  // Store the value.
  switch (assign_type) {
    case VARIABLE:
      EmitVariableAssignment(expr->target()->AsVariableProxy()->var(),
                             expr->op(), expr->AssignmentSlot());
      PrepareForBailoutForId(expr->AssignmentId(), TOS_REG);
      context()->Plug(r3);
      break;
    case NAMED_PROPERTY:
      EmitNamedPropertyAssignment(expr);
      break;
    case NAMED_SUPER_PROPERTY:
      EmitNamedSuperPropertyStore(property);
      context()->Plug(r3);
      break;
    case KEYED_SUPER_PROPERTY:
      EmitKeyedSuperPropertyStore(property);
      context()->Plug(r3);
      break;
    case KEYED_PROPERTY:
      EmitKeyedPropertyAssignment(expr);
      break;
  }
}


void FullCodeGenerator::VisitYield(Yield* expr) {
  Comment cmnt(masm_, "[ Yield");
  SetExpressionPosition(expr);

  // Evaluate yielded value first; the initial iterator definition depends on
  // this.  It stays on the stack while we update the iterator.
  VisitForStackValue(expr->expression());

  switch (expr->yield_kind()) {
    case Yield::kSuspend:
      // Pop value from top-of-stack slot; box result into result register.
      EmitCreateIteratorResult(false);
      __ push(result_register());
    // Fall through.
    case Yield::kInitial: {
      Label suspend, continuation, post_runtime, resume;

      __ b(&suspend);
      __ bind(&continuation);
      __ RecordGeneratorContinuation();
      __ b(&resume);

      __ bind(&suspend);
      VisitForAccumulatorValue(expr->generator_object());
      DCHECK(continuation.pos() > 0 && Smi::IsValid(continuation.pos()));
      __ LoadSmiLiteral(r4, Smi::FromInt(continuation.pos()));
      __ StoreP(r4, FieldMemOperand(r3, JSGeneratorObject::kContinuationOffset),
                r0);
      __ StoreP(cp, FieldMemOperand(r3, JSGeneratorObject::kContextOffset), r0);
      __ mr(r4, cp);
      __ RecordWriteField(r3, JSGeneratorObject::kContextOffset, r4, r5,
                          kLRHasBeenSaved, kDontSaveFPRegs);
      __ addi(r4, fp, Operand(StandardFrameConstants::kExpressionsOffset));
      __ cmp(sp, r4);
      __ beq(&post_runtime);
      __ push(r3);  // generator object
      __ CallRuntime(Runtime::kSuspendJSGeneratorObject, 1);
      __ LoadP(cp, MemOperand(fp, StandardFrameConstants::kContextOffset));
      __ bind(&post_runtime);
      __ pop(result_register());
      EmitReturnSequence();

      __ bind(&resume);
      context()->Plug(result_register());
      break;
    }

    case Yield::kFinal: {
      VisitForAccumulatorValue(expr->generator_object());
      __ LoadSmiLiteral(r4, Smi::FromInt(JSGeneratorObject::kGeneratorClosed));
      __ StoreP(r4, FieldMemOperand(result_register(),
                                    JSGeneratorObject::kContinuationOffset),
                r0);
      // Pop value from top-of-stack slot, box result into result register.
      EmitCreateIteratorResult(true);
      EmitUnwindBeforeReturn();
      EmitReturnSequence();
      break;
    }

    case Yield::kDelegating: {
      VisitForStackValue(expr->generator_object());

      // Initial stack layout is as follows:
      // [sp + 1 * kPointerSize] iter
      // [sp + 0 * kPointerSize] g

      Label l_catch, l_try, l_suspend, l_continuation, l_resume;
      Label l_next, l_call;
      Register load_receiver = LoadDescriptor::ReceiverRegister();
      Register load_name = LoadDescriptor::NameRegister();

      // Initial send value is undefined.
      __ LoadRoot(r3, Heap::kUndefinedValueRootIndex);
      __ b(&l_next);

      // catch (e) { receiver = iter; f = 'throw'; arg = e; goto l_call; }
      __ bind(&l_catch);
      __ LoadRoot(load_name, Heap::kthrow_stringRootIndex);  // "throw"
      __ LoadP(r6, MemOperand(sp, 1 * kPointerSize));        // iter
      __ Push(load_name, r6, r3);  // "throw", iter, except
      __ b(&l_call);

      // try { received = %yield result }
      // Shuffle the received result above a try handler and yield it without
      // re-boxing.
      __ bind(&l_try);
      __ pop(r3);  // result
      int handler_index = NewHandlerTableEntry();
      EnterTryBlock(handler_index, &l_catch);
      const int try_block_size = TryCatch::kElementCount * kPointerSize;
      __ push(r3);  // result

      __ b(&l_suspend);
      __ bind(&l_continuation);
      __ RecordGeneratorContinuation();
      __ b(&l_resume);

      __ bind(&l_suspend);
      const int generator_object_depth = kPointerSize + try_block_size;
      __ LoadP(r3, MemOperand(sp, generator_object_depth));
      __ push(r3);  // g
      __ Push(Smi::FromInt(handler_index));  // handler-index
      DCHECK(l_continuation.pos() > 0 && Smi::IsValid(l_continuation.pos()));
      __ LoadSmiLiteral(r4, Smi::FromInt(l_continuation.pos()));
      __ StoreP(r4, FieldMemOperand(r3, JSGeneratorObject::kContinuationOffset),
                r0);
      __ StoreP(cp, FieldMemOperand(r3, JSGeneratorObject::kContextOffset), r0);
      __ mr(r4, cp);
      __ RecordWriteField(r3, JSGeneratorObject::kContextOffset, r4, r5,
                          kLRHasBeenSaved, kDontSaveFPRegs);
      __ CallRuntime(Runtime::kSuspendJSGeneratorObject, 2);
      __ LoadP(cp, MemOperand(fp, StandardFrameConstants::kContextOffset));
      __ pop(r3);  // result
      EmitReturnSequence();
      __ bind(&l_resume);  // received in r3
      ExitTryBlock(handler_index);

      // receiver = iter; f = 'next'; arg = received;
      __ bind(&l_next);

      __ LoadRoot(load_name, Heap::knext_stringRootIndex);  // "next"
      __ LoadP(r6, MemOperand(sp, 1 * kPointerSize));       // iter
      __ Push(load_name, r6, r3);  // "next", iter, received

      // result = receiver[f](arg);
      __ bind(&l_call);
      __ LoadP(load_receiver, MemOperand(sp, kPointerSize));
      __ LoadP(load_name, MemOperand(sp, 2 * kPointerSize));
      __ mov(LoadDescriptor::SlotRegister(),
             Operand(SmiFromSlot(expr->KeyedLoadFeedbackSlot())));
      Handle<Code> ic = CodeFactory::KeyedLoadIC(isolate(), SLOPPY).code();
      CallIC(ic, TypeFeedbackId::None());
      __ mr(r4, r3);
      __ StoreP(r4, MemOperand(sp, 2 * kPointerSize));
      SetCallPosition(expr);
      __ li(r3, Operand(1));
      __ Call(
          isolate()->builtins()->Call(ConvertReceiverMode::kNotNullOrUndefined),
          RelocInfo::CODE_TARGET);

      __ LoadP(cp, MemOperand(fp, StandardFrameConstants::kContextOffset));
      __ Drop(1);  // The function is still on the stack; drop it.

      // if (!result.done) goto l_try;
      __ Move(load_receiver, r3);

      __ push(load_receiver);                               // save result
      __ LoadRoot(load_name, Heap::kdone_stringRootIndex);  // "done"
      __ mov(LoadDescriptor::SlotRegister(),
             Operand(SmiFromSlot(expr->DoneFeedbackSlot())));
      CallLoadIC(NOT_INSIDE_TYPEOF);  // r0=result.done
      Handle<Code> bool_ic = ToBooleanStub::GetUninitialized(isolate());
      CallIC(bool_ic);
      __ CompareRoot(result_register(), Heap::kTrueValueRootIndex);
      __ bne(&l_try);

      // result.value
      __ pop(load_receiver);                                 // result
      __ LoadRoot(load_name, Heap::kvalue_stringRootIndex);  // "value"
      __ mov(LoadDescriptor::SlotRegister(),
             Operand(SmiFromSlot(expr->ValueFeedbackSlot())));
      CallLoadIC(NOT_INSIDE_TYPEOF);  // r3=result.value
      context()->DropAndPlug(2, r3);  // drop iter and g
      break;
    }
  }
}


void FullCodeGenerator::EmitGeneratorResume(
    Expression* generator, Expression* value,
    JSGeneratorObject::ResumeMode resume_mode) {
  // The value stays in r3, and is ultimately read by the resumed generator, as
  // if CallRuntime(Runtime::kSuspendJSGeneratorObject) returned it. Or it
  // is read to throw the value when the resumed generator is already closed.
  // r4 will hold the generator object until the activation has been resumed.
  VisitForStackValue(generator);
  VisitForAccumulatorValue(value);
  __ pop(r4);

  // Load suspended function and context.
  __ LoadP(cp, FieldMemOperand(r4, JSGeneratorObject::kContextOffset));
  __ LoadP(r7, FieldMemOperand(r4, JSGeneratorObject::kFunctionOffset));

  // Load receiver and store as the first argument.
  __ LoadP(r5, FieldMemOperand(r4, JSGeneratorObject::kReceiverOffset));
  __ push(r5);

  // Push holes for the rest of the arguments to the generator function.
  __ LoadP(r6, FieldMemOperand(r7, JSFunction::kSharedFunctionInfoOffset));
  __ LoadWordArith(
      r6, FieldMemOperand(r6, SharedFunctionInfo::kFormalParameterCountOffset));
  __ LoadRoot(r5, Heap::kTheHoleValueRootIndex);
  Label argument_loop, push_frame;
#if V8_TARGET_ARCH_PPC64
  __ cmpi(r6, Operand::Zero());
  __ beq(&push_frame);
#else
  __ SmiUntag(r6, SetRC);
  __ beq(&push_frame, cr0);
#endif
  __ mtctr(r6);
  __ bind(&argument_loop);
  __ push(r5);
  __ bdnz(&argument_loop);

  // Enter a new JavaScript frame, and initialize its slots as they were when
  // the generator was suspended.
  Label resume_frame, done;
  __ bind(&push_frame);
  __ b(&resume_frame, SetLK);
  __ b(&done);
  __ bind(&resume_frame);
  // lr = return address.
  // fp = caller's frame pointer.
  // cp = callee's context,
  // r7 = callee's JS function.
  __ PushFixedFrame(r7);
  // Adjust FP to point to saved FP.
  __ addi(fp, sp, Operand(StandardFrameConstants::kFixedFrameSizeFromFp));

  // Load the operand stack size.
  __ LoadP(r6, FieldMemOperand(r4, JSGeneratorObject::kOperandStackOffset));
  __ LoadP(r6, FieldMemOperand(r6, FixedArray::kLengthOffset));
  __ SmiUntag(r6, SetRC);

  // If we are sending a value and there is no operand stack, we can jump back
  // in directly.
  Label call_resume;
  if (resume_mode == JSGeneratorObject::NEXT) {
    Label slow_resume;
    __ bne(&slow_resume, cr0);
    __ LoadP(ip, FieldMemOperand(r7, JSFunction::kCodeEntryOffset));
    {
      ConstantPoolUnavailableScope constant_pool_unavailable(masm_);
      if (FLAG_enable_embedded_constant_pool) {
        __ LoadConstantPoolPointerRegisterFromCodeTargetAddress(ip);
      }
      __ LoadP(r5, FieldMemOperand(r4, JSGeneratorObject::kContinuationOffset));
      __ SmiUntag(r5);
      __ add(ip, ip, r5);
      __ LoadSmiLiteral(r5,
                        Smi::FromInt(JSGeneratorObject::kGeneratorExecuting));
      __ StoreP(r5, FieldMemOperand(r4, JSGeneratorObject::kContinuationOffset),
                r0);
      __ Jump(ip);
      __ bind(&slow_resume);
    }
  } else {
    __ beq(&call_resume, cr0);
  }

  // Otherwise, we push holes for the operand stack and call the runtime to fix
  // up the stack and the handlers.
  Label operand_loop;
  __ mtctr(r6);
  __ bind(&operand_loop);
  __ push(r5);
  __ bdnz(&operand_loop);

  __ bind(&call_resume);
  DCHECK(!result_register().is(r4));
  __ Push(r4, result_register());
  __ Push(Smi::FromInt(resume_mode));
  __ CallRuntime(Runtime::kResumeJSGeneratorObject);
  // Not reached: the runtime call returns elsewhere.
  __ stop("not-reached");

  __ bind(&done);
  context()->Plug(result_register());
}


void FullCodeGenerator::EmitCreateIteratorResult(bool done) {
  Label allocate, done_allocate;

  __ Allocate(JSIteratorResult::kSize, r3, r5, r6, &allocate, TAG_OBJECT);
  __ b(&done_allocate);

  __ bind(&allocate);
  __ Push(Smi::FromInt(JSIteratorResult::kSize));
  __ CallRuntime(Runtime::kAllocateInNewSpace);

  __ bind(&done_allocate);
  __ LoadNativeContextSlot(Context::ITERATOR_RESULT_MAP_INDEX, r4);
  __ pop(r5);
  __ LoadRoot(r6,
              done ? Heap::kTrueValueRootIndex : Heap::kFalseValueRootIndex);
  __ LoadRoot(r7, Heap::kEmptyFixedArrayRootIndex);
  __ StoreP(r4, FieldMemOperand(r3, HeapObject::kMapOffset), r0);
  __ StoreP(r7, FieldMemOperand(r3, JSObject::kPropertiesOffset), r0);
  __ StoreP(r7, FieldMemOperand(r3, JSObject::kElementsOffset), r0);
  __ StoreP(r5, FieldMemOperand(r3, JSIteratorResult::kValueOffset), r0);
  __ StoreP(r6, FieldMemOperand(r3, JSIteratorResult::kDoneOffset), r0);
}


void FullCodeGenerator::EmitNamedPropertyLoad(Property* prop) {
  SetExpressionPosition(prop);
  Literal* key = prop->key()->AsLiteral();
  DCHECK(!prop->IsSuperAccess());

  __ mov(LoadDescriptor::NameRegister(), Operand(key->value()));
  __ mov(LoadDescriptor::SlotRegister(),
         Operand(SmiFromSlot(prop->PropertyFeedbackSlot())));
  CallLoadIC(NOT_INSIDE_TYPEOF, language_mode());
}


void FullCodeGenerator::EmitNamedSuperPropertyLoad(Property* prop) {
  // Stack: receiver, home_object.
  SetExpressionPosition(prop);
  Literal* key = prop->key()->AsLiteral();
  DCHECK(!key->value()->IsSmi());
  DCHECK(prop->IsSuperAccess());

  __ Push(key->value());
  __ Push(Smi::FromInt(language_mode()));
  __ CallRuntime(Runtime::kLoadFromSuper);
}


void FullCodeGenerator::EmitKeyedPropertyLoad(Property* prop) {
  SetExpressionPosition(prop);
  Handle<Code> ic = CodeFactory::KeyedLoadIC(isolate(), language_mode()).code();
  __ mov(LoadDescriptor::SlotRegister(),
         Operand(SmiFromSlot(prop->PropertyFeedbackSlot())));
  CallIC(ic);
}


void FullCodeGenerator::EmitKeyedSuperPropertyLoad(Property* prop) {
  // Stack: receiver, home_object, key.
  SetExpressionPosition(prop);
  __ Push(Smi::FromInt(language_mode()));
  __ CallRuntime(Runtime::kLoadKeyedFromSuper);
}


void FullCodeGenerator::EmitInlineSmiBinaryOp(BinaryOperation* expr,
                                              Token::Value op,
                                              Expression* left_expr,
                                              Expression* right_expr) {
  Label done, smi_case, stub_call;

  Register scratch1 = r5;
  Register scratch2 = r6;

  // Get the arguments.
  Register left = r4;
  Register right = r3;
  __ pop(left);

  // Perform combined smi check on both operands.
  __ orx(scratch1, left, right);
  STATIC_ASSERT(kSmiTag == 0);
  JumpPatchSite patch_site(masm_);
  patch_site.EmitJumpIfSmi(scratch1, &smi_case);

  __ bind(&stub_call);
  Handle<Code> code =
      CodeFactory::BinaryOpIC(isolate(), op, strength(language_mode())).code();
  CallIC(code, expr->BinaryOperationFeedbackId());
  patch_site.EmitPatchInfo();
  __ b(&done);

  __ bind(&smi_case);
  // Smi case. This code works the same way as the smi-smi case in the type
  // recording binary operation stub.
  switch (op) {
    case Token::SAR:
      __ GetLeastBitsFromSmi(scratch1, right, 5);
      __ ShiftRightArith(right, left, scratch1);
      __ ClearRightImm(right, right, Operand(kSmiTagSize + kSmiShiftSize));
      break;
    case Token::SHL: {
      __ GetLeastBitsFromSmi(scratch2, right, 5);
#if V8_TARGET_ARCH_PPC64
      __ ShiftLeft_(right, left, scratch2);
#else
      __ SmiUntag(scratch1, left);
      __ ShiftLeft_(scratch1, scratch1, scratch2);
      // Check that the *signed* result fits in a smi
      __ JumpIfNotSmiCandidate(scratch1, scratch2, &stub_call);
      __ SmiTag(right, scratch1);
#endif
      break;
    }
    case Token::SHR: {
      __ SmiUntag(scratch1, left);
      __ GetLeastBitsFromSmi(scratch2, right, 5);
      __ srw(scratch1, scratch1, scratch2);
      // Unsigned shift is not allowed to produce a negative number.
      __ JumpIfNotUnsignedSmiCandidate(scratch1, r0, &stub_call);
      __ SmiTag(right, scratch1);
      break;
    }
    case Token::ADD: {
      __ AddAndCheckForOverflow(scratch1, left, right, scratch2, r0);
      __ BranchOnOverflow(&stub_call);
      __ mr(right, scratch1);
      break;
    }
    case Token::SUB: {
      __ SubAndCheckForOverflow(scratch1, left, right, scratch2, r0);
      __ BranchOnOverflow(&stub_call);
      __ mr(right, scratch1);
      break;
    }
    case Token::MUL: {
      Label mul_zero;
#if V8_TARGET_ARCH_PPC64
      // Remove tag from both operands.
      __ SmiUntag(ip, right);
      __ SmiUntag(r0, left);
      __ Mul(scratch1, r0, ip);
      // Check for overflowing the smi range - no overflow if higher 33 bits of
      // the result are identical.
      __ TestIfInt32(scratch1, r0);
      __ bne(&stub_call);
#else
      __ SmiUntag(ip, right);
      __ mullw(scratch1, left, ip);
      __ mulhw(scratch2, left, ip);
      // Check for overflowing the smi range - no overflow if higher 33 bits of
      // the result are identical.
      __ TestIfInt32(scratch2, scratch1, ip);
      __ bne(&stub_call);
#endif
      // Go slow on zero result to handle -0.
      __ cmpi(scratch1, Operand::Zero());
      __ beq(&mul_zero);
#if V8_TARGET_ARCH_PPC64
      __ SmiTag(right, scratch1);
#else
      __ mr(right, scratch1);
#endif
      __ b(&done);
      // We need -0 if we were multiplying a negative number with 0 to get 0.
      // We know one of them was zero.
      __ bind(&mul_zero);
      __ add(scratch2, right, left);
      __ cmpi(scratch2, Operand::Zero());
      __ blt(&stub_call);
      __ LoadSmiLiteral(right, Smi::FromInt(0));
      break;
    }
    case Token::BIT_OR:
      __ orx(right, left, right);
      break;
    case Token::BIT_AND:
      __ and_(right, left, right);
      break;
    case Token::BIT_XOR:
      __ xor_(right, left, right);
      break;
    default:
      UNREACHABLE();
  }

  __ bind(&done);
  context()->Plug(r3);
}


void FullCodeGenerator::EmitClassDefineProperties(ClassLiteral* lit) {
  // Constructor is in r3.
  DCHECK(lit != NULL);
  __ push(r3);

  // No access check is needed here since the constructor is created by the
  // class literal.
  Register scratch = r4;
  __ LoadP(scratch,
           FieldMemOperand(r3, JSFunction::kPrototypeOrInitialMapOffset));
  __ push(scratch);

  for (int i = 0; i < lit->properties()->length(); i++) {
    ObjectLiteral::Property* property = lit->properties()->at(i);
    Expression* value = property->value();

    if (property->is_static()) {
      __ LoadP(scratch, MemOperand(sp, kPointerSize));  // constructor
    } else {
      __ LoadP(scratch, MemOperand(sp, 0));  // prototype
    }
    __ push(scratch);
    EmitPropertyKey(property, lit->GetIdForProperty(i));

    // The static prototype property is read only. We handle the non computed
    // property name case in the parser. Since this is the only case where we
    // need to check for an own read only property we special case this so we do
    // not need to do this for every property.
    if (property->is_static() && property->is_computed_name()) {
      __ CallRuntime(Runtime::kThrowIfStaticPrototype);
      __ push(r3);
    }

    VisitForStackValue(value);
    if (NeedsHomeObject(value)) {
      EmitSetHomeObject(value, 2, property->GetSlot());
    }

    switch (property->kind()) {
      case ObjectLiteral::Property::CONSTANT:
      case ObjectLiteral::Property::MATERIALIZED_LITERAL:
      case ObjectLiteral::Property::PROTOTYPE:
        UNREACHABLE();
      case ObjectLiteral::Property::COMPUTED:
        __ CallRuntime(Runtime::kDefineClassMethod);
        break;

      case ObjectLiteral::Property::GETTER:
        __ mov(r3, Operand(Smi::FromInt(DONT_ENUM)));
        __ push(r3);
        __ CallRuntime(Runtime::kDefineGetterPropertyUnchecked);
        break;

      case ObjectLiteral::Property::SETTER:
        __ mov(r3, Operand(Smi::FromInt(DONT_ENUM)));
        __ push(r3);
        __ CallRuntime(Runtime::kDefineSetterPropertyUnchecked);
        break;

      default:
        UNREACHABLE();
    }
  }

  // Set both the prototype and constructor to have fast properties, and also
  // freeze them in strong mode.
  __ CallRuntime(Runtime::kFinalizeClassDefinition);
}


void FullCodeGenerator::EmitBinaryOp(BinaryOperation* expr, Token::Value op) {
  __ pop(r4);
  Handle<Code> code =
      CodeFactory::BinaryOpIC(isolate(), op, strength(language_mode())).code();
  JumpPatchSite patch_site(masm_);  // unbound, signals no inlined smi code.
  CallIC(code, expr->BinaryOperationFeedbackId());
  patch_site.EmitPatchInfo();
  context()->Plug(r3);
}


void FullCodeGenerator::EmitAssignment(Expression* expr,
                                       FeedbackVectorSlot slot) {
  DCHECK(expr->IsValidReferenceExpressionOrThis());

  Property* prop = expr->AsProperty();
  LhsKind assign_type = Property::GetAssignType(prop);

  switch (assign_type) {
    case VARIABLE: {
      Variable* var = expr->AsVariableProxy()->var();
      EffectContext context(this);
      EmitVariableAssignment(var, Token::ASSIGN, slot);
      break;
    }
    case NAMED_PROPERTY: {
      __ push(r3);  // Preserve value.
      VisitForAccumulatorValue(prop->obj());
      __ Move(StoreDescriptor::ReceiverRegister(), r3);
      __ pop(StoreDescriptor::ValueRegister());  // Restore value.
      __ mov(StoreDescriptor::NameRegister(),
             Operand(prop->key()->AsLiteral()->value()));
      EmitLoadStoreICSlot(slot);
      CallStoreIC();
      break;
    }
    case NAMED_SUPER_PROPERTY: {
      __ Push(r3);
      VisitForStackValue(prop->obj()->AsSuperPropertyReference()->this_var());
      VisitForAccumulatorValue(
          prop->obj()->AsSuperPropertyReference()->home_object());
      // stack: value, this; r3: home_object
      Register scratch = r5;
      Register scratch2 = r6;
      __ mr(scratch, result_register());                  // home_object
      __ LoadP(r3, MemOperand(sp, kPointerSize));         // value
      __ LoadP(scratch2, MemOperand(sp, 0));              // this
      __ StoreP(scratch2, MemOperand(sp, kPointerSize));  // this
      __ StoreP(scratch, MemOperand(sp, 0));              // home_object
      // stack: this, home_object; r3: value
      EmitNamedSuperPropertyStore(prop);
      break;
    }
    case KEYED_SUPER_PROPERTY: {
      __ Push(r3);
      VisitForStackValue(prop->obj()->AsSuperPropertyReference()->this_var());
      VisitForStackValue(
          prop->obj()->AsSuperPropertyReference()->home_object());
      VisitForAccumulatorValue(prop->key());
      Register scratch = r5;
      Register scratch2 = r6;
      __ LoadP(scratch2, MemOperand(sp, 2 * kPointerSize));  // value
      // stack: value, this, home_object; r3: key, r6: value
      __ LoadP(scratch, MemOperand(sp, kPointerSize));  // this
      __ StoreP(scratch, MemOperand(sp, 2 * kPointerSize));
      __ LoadP(scratch, MemOperand(sp, 0));  // home_object
      __ StoreP(scratch, MemOperand(sp, kPointerSize));
      __ StoreP(r3, MemOperand(sp, 0));
      __ Move(r3, scratch2);
      // stack: this, home_object, key; r3: value.
      EmitKeyedSuperPropertyStore(prop);
      break;
    }
    case KEYED_PROPERTY: {
      __ push(r3);  // Preserve value.
      VisitForStackValue(prop->obj());
      VisitForAccumulatorValue(prop->key());
      __ Move(StoreDescriptor::NameRegister(), r3);
      __ Pop(StoreDescriptor::ValueRegister(),
             StoreDescriptor::ReceiverRegister());
      EmitLoadStoreICSlot(slot);
      Handle<Code> ic =
          CodeFactory::KeyedStoreIC(isolate(), language_mode()).code();
      CallIC(ic);
      break;
    }
  }
  context()->Plug(r3);
}


void FullCodeGenerator::EmitStoreToStackLocalOrContextSlot(
    Variable* var, MemOperand location) {
  __ StoreP(result_register(), location, r0);
  if (var->IsContextSlot()) {
    // RecordWrite may destroy all its register arguments.
    __ mr(r6, result_register());
    int offset = Context::SlotOffset(var->index());
    __ RecordWriteContextSlot(r4, offset, r6, r5, kLRHasBeenSaved,
                              kDontSaveFPRegs);
  }
}


void FullCodeGenerator::EmitVariableAssignment(Variable* var, Token::Value op,
                                               FeedbackVectorSlot slot) {
  if (var->IsUnallocated()) {
    // Global var, const, or let.
    __ mov(StoreDescriptor::NameRegister(), Operand(var->name()));
    __ LoadGlobalObject(StoreDescriptor::ReceiverRegister());
    EmitLoadStoreICSlot(slot);
    CallStoreIC();

  } else if (var->mode() == LET && op != Token::INIT) {
    // Non-initializing assignment to let variable needs a write barrier.
    DCHECK(!var->IsLookupSlot());
    DCHECK(var->IsStackAllocated() || var->IsContextSlot());
    Label assign;
    MemOperand location = VarOperand(var, r4);
    __ LoadP(r6, location);
    __ CompareRoot(r6, Heap::kTheHoleValueRootIndex);
    __ bne(&assign);
    __ mov(r6, Operand(var->name()));
    __ push(r6);
    __ CallRuntime(Runtime::kThrowReferenceError);
    // Perform the assignment.
    __ bind(&assign);
    EmitStoreToStackLocalOrContextSlot(var, location);

  } else if (var->mode() == CONST && op != Token::INIT) {
    // Assignment to const variable needs a write barrier.
    DCHECK(!var->IsLookupSlot());
    DCHECK(var->IsStackAllocated() || var->IsContextSlot());
    Label const_error;
    MemOperand location = VarOperand(var, r4);
    __ LoadP(r6, location);
    __ CompareRoot(r6, Heap::kTheHoleValueRootIndex);
    __ bne(&const_error);
    __ mov(r6, Operand(var->name()));
    __ push(r6);
    __ CallRuntime(Runtime::kThrowReferenceError);
    __ bind(&const_error);
    __ CallRuntime(Runtime::kThrowConstAssignError);

  } else if (var->is_this() && var->mode() == CONST && op == Token::INIT) {
    // Initializing assignment to const {this} needs a write barrier.
    DCHECK(var->IsStackAllocated() || var->IsContextSlot());
    Label uninitialized_this;
    MemOperand location = VarOperand(var, r4);
    __ LoadP(r6, location);
    __ CompareRoot(r6, Heap::kTheHoleValueRootIndex);
    __ beq(&uninitialized_this);
    __ mov(r4, Operand(var->name()));
    __ push(r4);
    __ CallRuntime(Runtime::kThrowReferenceError);
    __ bind(&uninitialized_this);
    EmitStoreToStackLocalOrContextSlot(var, location);

  } else if (!var->is_const_mode() ||
             (var->mode() == CONST && op == Token::INIT)) {
    if (var->IsLookupSlot()) {
      // Assignment to var.
      __ push(r3);  // Value.
      __ mov(r4, Operand(var->name()));
      __ mov(r3, Operand(Smi::FromInt(language_mode())));
      __ Push(cp, r4, r3);  // Context, name, language mode.
      __ CallRuntime(Runtime::kStoreLookupSlot);
    } else {
      // Assignment to var or initializing assignment to let/const in harmony
      // mode.
      DCHECK((var->IsStackAllocated() || var->IsContextSlot()));
      MemOperand location = VarOperand(var, r4);
      if (generate_debug_code_ && var->mode() == LET && op == Token::INIT) {
        // Check for an uninitialized let binding.
        __ LoadP(r5, location);
        __ CompareRoot(r5, Heap::kTheHoleValueRootIndex);
        __ Check(eq, kLetBindingReInitialization);
      }
      EmitStoreToStackLocalOrContextSlot(var, location);
    }
  } else if (var->mode() == CONST_LEGACY && op == Token::INIT) {
    // Const initializers need a write barrier.
    DCHECK(!var->IsParameter());  // No const parameters.
    if (var->IsLookupSlot()) {
      __ push(r3);
      __ mov(r3, Operand(var->name()));
      __ Push(cp, r3);  // Context and name.
      __ CallRuntime(Runtime::kInitializeLegacyConstLookupSlot);
    } else {
      DCHECK(var->IsStackAllocated() || var->IsContextSlot());
      Label skip;
      MemOperand location = VarOperand(var, r4);
      __ LoadP(r5, location);
      __ CompareRoot(r5, Heap::kTheHoleValueRootIndex);
      __ bne(&skip);
      EmitStoreToStackLocalOrContextSlot(var, location);
      __ bind(&skip);
    }

  } else {
    DCHECK(var->mode() == CONST_LEGACY && op != Token::INIT);
    if (is_strict(language_mode())) {
      __ CallRuntime(Runtime::kThrowConstAssignError);
    }
    // Silently ignore store in sloppy mode.
  }
}


void FullCodeGenerator::EmitNamedPropertyAssignment(Assignment* expr) {
  // Assignment to a property, using a named store IC.
  Property* prop = expr->target()->AsProperty();
  DCHECK(prop != NULL);
  DCHECK(prop->key()->IsLiteral());

  __ mov(StoreDescriptor::NameRegister(),
         Operand(prop->key()->AsLiteral()->value()));
  __ pop(StoreDescriptor::ReceiverRegister());
  EmitLoadStoreICSlot(expr->AssignmentSlot());
  CallStoreIC();

  PrepareForBailoutForId(expr->AssignmentId(), TOS_REG);
  context()->Plug(r3);
}


void FullCodeGenerator::EmitNamedSuperPropertyStore(Property* prop) {
  // Assignment to named property of super.
  // r3 : value
  // stack : receiver ('this'), home_object
  DCHECK(prop != NULL);
  Literal* key = prop->key()->AsLiteral();
  DCHECK(key != NULL);

  __ Push(key->value());
  __ Push(r3);
  __ CallRuntime((is_strict(language_mode()) ? Runtime::kStoreToSuper_Strict
                                             : Runtime::kStoreToSuper_Sloppy));
}


void FullCodeGenerator::EmitKeyedSuperPropertyStore(Property* prop) {
  // Assignment to named property of super.
  // r3 : value
  // stack : receiver ('this'), home_object, key
  DCHECK(prop != NULL);

  __ Push(r3);
  __ CallRuntime((is_strict(language_mode())
                      ? Runtime::kStoreKeyedToSuper_Strict
                      : Runtime::kStoreKeyedToSuper_Sloppy));
}


void FullCodeGenerator::EmitKeyedPropertyAssignment(Assignment* expr) {
  // Assignment to a property, using a keyed store IC.
  __ Pop(StoreDescriptor::ReceiverRegister(), StoreDescriptor::NameRegister());
  DCHECK(StoreDescriptor::ValueRegister().is(r3));

  Handle<Code> ic =
      CodeFactory::KeyedStoreIC(isolate(), language_mode()).code();
  EmitLoadStoreICSlot(expr->AssignmentSlot());
  CallIC(ic);

  PrepareForBailoutForId(expr->AssignmentId(), TOS_REG);
  context()->Plug(r3);
}


void FullCodeGenerator::VisitProperty(Property* expr) {
  Comment cmnt(masm_, "[ Property");
  SetExpressionPosition(expr);

  Expression* key = expr->key();

  if (key->IsPropertyName()) {
    if (!expr->IsSuperAccess()) {
      VisitForAccumulatorValue(expr->obj());
      __ Move(LoadDescriptor::ReceiverRegister(), r3);
      EmitNamedPropertyLoad(expr);
    } else {
      VisitForStackValue(expr->obj()->AsSuperPropertyReference()->this_var());
      VisitForStackValue(
          expr->obj()->AsSuperPropertyReference()->home_object());
      EmitNamedSuperPropertyLoad(expr);
    }
  } else {
    if (!expr->IsSuperAccess()) {
      VisitForStackValue(expr->obj());
      VisitForAccumulatorValue(expr->key());
      __ Move(LoadDescriptor::NameRegister(), r3);
      __ pop(LoadDescriptor::ReceiverRegister());
      EmitKeyedPropertyLoad(expr);
    } else {
      VisitForStackValue(expr->obj()->AsSuperPropertyReference()->this_var());
      VisitForStackValue(
          expr->obj()->AsSuperPropertyReference()->home_object());
      VisitForStackValue(expr->key());
      EmitKeyedSuperPropertyLoad(expr);
    }
  }
  PrepareForBailoutForId(expr->LoadId(), TOS_REG);
  context()->Plug(r3);
}


void FullCodeGenerator::CallIC(Handle<Code> code, TypeFeedbackId ast_id) {
  ic_total_count_++;
  __ Call(code, RelocInfo::CODE_TARGET, ast_id);
}


// Code common for calls using the IC.
void FullCodeGenerator::EmitCallWithLoadIC(Call* expr) {
  Expression* callee = expr->expression();

  // Get the target function.
  ConvertReceiverMode convert_mode;
  if (callee->IsVariableProxy()) {
    {
      StackValueContext context(this);
      EmitVariableLoad(callee->AsVariableProxy());
      PrepareForBailout(callee, NO_REGISTERS);
    }
    // Push undefined as receiver. This is patched in the method prologue if it
    // is a sloppy mode method.
    __ LoadRoot(r0, Heap::kUndefinedValueRootIndex);
    __ push(r0);
    convert_mode = ConvertReceiverMode::kNullOrUndefined;
  } else {
    // Load the function from the receiver.
    DCHECK(callee->IsProperty());
    DCHECK(!callee->AsProperty()->IsSuperAccess());
    __ LoadP(LoadDescriptor::ReceiverRegister(), MemOperand(sp, 0));
    EmitNamedPropertyLoad(callee->AsProperty());
    PrepareForBailoutForId(callee->AsProperty()->LoadId(), TOS_REG);
    // Push the target function under the receiver.
    __ LoadP(r0, MemOperand(sp, 0));
    __ push(r0);
    __ StoreP(r3, MemOperand(sp, kPointerSize));
    convert_mode = ConvertReceiverMode::kNotNullOrUndefined;
  }

  EmitCall(expr, convert_mode);
}


void FullCodeGenerator::EmitSuperCallWithLoadIC(Call* expr) {
  Expression* callee = expr->expression();
  DCHECK(callee->IsProperty());
  Property* prop = callee->AsProperty();
  DCHECK(prop->IsSuperAccess());
  SetExpressionPosition(prop);

  Literal* key = prop->key()->AsLiteral();
  DCHECK(!key->value()->IsSmi());
  // Load the function from the receiver.
  const Register scratch = r4;
  SuperPropertyReference* super_ref = prop->obj()->AsSuperPropertyReference();
  VisitForAccumulatorValue(super_ref->home_object());
  __ mr(scratch, r3);
  VisitForAccumulatorValue(super_ref->this_var());
  __ Push(scratch, r3, r3, scratch);
  __ Push(key->value());
  __ Push(Smi::FromInt(language_mode()));

  // Stack here:
  //  - home_object
  //  - this (receiver)
  //  - this (receiver) <-- LoadFromSuper will pop here and below.
  //  - home_object
  //  - key
  //  - language_mode
  __ CallRuntime(Runtime::kLoadFromSuper);

  // Replace home_object with target function.
  __ StoreP(r3, MemOperand(sp, kPointerSize));

  // Stack here:
  // - target function
  // - this (receiver)
  EmitCall(expr);
}


// Code common for calls using the IC.
void FullCodeGenerator::EmitKeyedCallWithLoadIC(Call* expr, Expression* key) {
  // Load the key.
  VisitForAccumulatorValue(key);

  Expression* callee = expr->expression();

  // Load the function from the receiver.
  DCHECK(callee->IsProperty());
  __ LoadP(LoadDescriptor::ReceiverRegister(), MemOperand(sp, 0));
  __ Move(LoadDescriptor::NameRegister(), r3);
  EmitKeyedPropertyLoad(callee->AsProperty());
  PrepareForBailoutForId(callee->AsProperty()->LoadId(), TOS_REG);

  // Push the target function under the receiver.
  __ LoadP(ip, MemOperand(sp, 0));
  __ push(ip);
  __ StoreP(r3, MemOperand(sp, kPointerSize));

  EmitCall(expr, ConvertReceiverMode::kNotNullOrUndefined);
}


void FullCodeGenerator::EmitKeyedSuperCallWithLoadIC(Call* expr) {
  Expression* callee = expr->expression();
  DCHECK(callee->IsProperty());
  Property* prop = callee->AsProperty();
  DCHECK(prop->IsSuperAccess());

  SetExpressionPosition(prop);
  // Load the function from the receiver.
  const Register scratch = r4;
  SuperPropertyReference* super_ref = prop->obj()->AsSuperPropertyReference();
  VisitForAccumulatorValue(super_ref->home_object());
  __ mr(scratch, r3);
  VisitForAccumulatorValue(super_ref->this_var());
  __ Push(scratch, r3, r3, scratch);
  VisitForStackValue(prop->key());
  __ Push(Smi::FromInt(language_mode()));

  // Stack here:
  //  - home_object
  //  - this (receiver)
  //  - this (receiver) <-- LoadKeyedFromSuper will pop here and below.
  //  - home_object
  //  - key
  //  - language_mode
  __ CallRuntime(Runtime::kLoadKeyedFromSuper);

  // Replace home_object with target function.
  __ StoreP(r3, MemOperand(sp, kPointerSize));

  // Stack here:
  // - target function
  // - this (receiver)
  EmitCall(expr);
}


void FullCodeGenerator::EmitCall(Call* expr, ConvertReceiverMode mode) {
  // Load the arguments.
  ZoneList<Expression*>* args = expr->arguments();
  int arg_count = args->length();
  for (int i = 0; i < arg_count; i++) {
    VisitForStackValue(args->at(i));
  }

  PrepareForBailoutForId(expr->CallId(), NO_REGISTERS);
  SetCallPosition(expr);
  Handle<Code> ic = CodeFactory::CallIC(isolate(), arg_count, mode).code();
  __ LoadSmiLiteral(r6, SmiFromSlot(expr->CallFeedbackICSlot()));
  __ LoadP(r4, MemOperand(sp, (arg_count + 1) * kPointerSize), r0);
  // Don't assign a type feedback id to the IC, since type feedback is provided
  // by the vector above.
  CallIC(ic);

  RecordJSReturnSite(expr);
  // Restore context register.
  __ LoadP(cp, MemOperand(fp, StandardFrameConstants::kContextOffset));
  context()->DropAndPlug(1, r3);
}


void FullCodeGenerator::EmitResolvePossiblyDirectEval(int arg_count) {
  // r7: copy of the first argument or undefined if it doesn't exist.
  if (arg_count > 0) {
    __ LoadP(r7, MemOperand(sp, arg_count * kPointerSize), r0);
  } else {
    __ LoadRoot(r7, Heap::kUndefinedValueRootIndex);
  }

  // r6: the receiver of the enclosing function.
  __ LoadP(r6, MemOperand(fp, JavaScriptFrameConstants::kFunctionOffset));

  // r5: language mode.
  __ LoadSmiLiteral(r5, Smi::FromInt(language_mode()));

  // r4: the start position of the scope the calls resides in.
  __ LoadSmiLiteral(r4, Smi::FromInt(scope()->start_position()));

  // Do the runtime call.
  __ Push(r7, r6, r5, r4);
  __ CallRuntime(Runtime::kResolvePossiblyDirectEval);
}


// See http://www.ecma-international.org/ecma-262/6.0/#sec-function-calls.
void FullCodeGenerator::PushCalleeAndWithBaseObject(Call* expr) {
  VariableProxy* callee = expr->expression()->AsVariableProxy();
  if (callee->var()->IsLookupSlot()) {
    Label slow, done;
    SetExpressionPosition(callee);
    // Generate code for loading from variables potentially shadowed by
    // eval-introduced variables.
    EmitDynamicLookupFastCase(callee, NOT_INSIDE_TYPEOF, &slow, &done);

    __ bind(&slow);
    // Call the runtime to find the function to call (returned in r3) and
    // the object holding it (returned in r4).
    DCHECK(!context_register().is(r5));
    __ mov(r5, Operand(callee->name()));
    __ Push(context_register(), r5);
    __ CallRuntime(Runtime::kLoadLookupSlot);
    __ Push(r3, r4);  // Function, receiver.
    PrepareForBailoutForId(expr->LookupId(), NO_REGISTERS);

    // If fast case code has been generated, emit code to push the function
    // and receiver and have the slow path jump around this code.
    if (done.is_linked()) {
      Label call;
      __ b(&call);
      __ bind(&done);
      // Push function.
      __ push(r3);
      // Pass undefined as the receiver, which is the WithBaseObject of a
      // non-object environment record.  If the callee is sloppy, it will patch
      // it up to be the global receiver.
      __ LoadRoot(r4, Heap::kUndefinedValueRootIndex);
      __ push(r4);
      __ bind(&call);
    }
  } else {
    VisitForStackValue(callee);
    // refEnv.WithBaseObject()
    __ LoadRoot(r5, Heap::kUndefinedValueRootIndex);
    __ push(r5);  // Reserved receiver slot.
  }
}


void FullCodeGenerator::EmitPossiblyEvalCall(Call* expr) {
  // In a call to eval, we first call RuntimeHidden_ResolvePossiblyDirectEval
  // to resolve the function we need to call.  Then we call the resolved
  // function using the given arguments.
  ZoneList<Expression*>* args = expr->arguments();
  int arg_count = args->length();

  PushCalleeAndWithBaseObject(expr);

  // Push the arguments.
  for (int i = 0; i < arg_count; i++) {
    VisitForStackValue(args->at(i));
  }

  // Push a copy of the function (found below the arguments) and
  // resolve eval.
  __ LoadP(r4, MemOperand(sp, (arg_count + 1) * kPointerSize), r0);
  __ push(r4);
  EmitResolvePossiblyDirectEval(arg_count);

  // Touch up the stack with the resolved function.
  __ StoreP(r3, MemOperand(sp, (arg_count + 1) * kPointerSize), r0);

  PrepareForBailoutForId(expr->EvalId(), NO_REGISTERS);

  // Record source position for debugger.
  SetCallPosition(expr);
  __ LoadP(r4, MemOperand(sp, (arg_count + 1) * kPointerSize), r0);
  __ mov(r3, Operand(arg_count));
  __ Call(isolate()->builtins()->Call(), RelocInfo::CODE_TARGET);
  RecordJSReturnSite(expr);
  // Restore context register.
  __ LoadP(cp, MemOperand(fp, StandardFrameConstants::kContextOffset));
  context()->DropAndPlug(1, r3);
}


void FullCodeGenerator::VisitCallNew(CallNew* expr) {
  Comment cmnt(masm_, "[ CallNew");
  // According to ECMA-262, section 11.2.2, page 44, the function
  // expression in new calls must be evaluated before the
  // arguments.

  // Push constructor on the stack.  If it's not a function it's used as
  // receiver for CALL_NON_FUNCTION, otherwise the value on the stack is
  // ignored.
  DCHECK(!expr->expression()->IsSuperPropertyReference());
  VisitForStackValue(expr->expression());

  // Push the arguments ("left-to-right") on the stack.
  ZoneList<Expression*>* args = expr->arguments();
  int arg_count = args->length();
  for (int i = 0; i < arg_count; i++) {
    VisitForStackValue(args->at(i));
  }

  // Call the construct call builtin that handles allocation and
  // constructor invocation.
  SetConstructCallPosition(expr);

  // Load function and argument count into r4 and r3.
  __ mov(r3, Operand(arg_count));
  __ LoadP(r4, MemOperand(sp, arg_count * kPointerSize), r0);

  // Record call targets in unoptimized code.
  __ EmitLoadTypeFeedbackVector(r5);
  __ LoadSmiLiteral(r6, SmiFromSlot(expr->CallNewFeedbackSlot()));

  CallConstructStub stub(isolate());
  __ Call(stub.GetCode(), RelocInfo::CODE_TARGET);
  PrepareForBailoutForId(expr->ReturnId(), TOS_REG);
  // Restore context register.
  __ LoadP(cp, MemOperand(fp, StandardFrameConstants::kContextOffset));
  context()->Plug(r3);
}


void FullCodeGenerator::EmitSuperConstructorCall(Call* expr) {
  SuperCallReference* super_call_ref =
      expr->expression()->AsSuperCallReference();
  DCHECK_NOT_NULL(super_call_ref);

  // Push the super constructor target on the stack (may be null,
  // but the Construct builtin can deal with that properly).
  VisitForAccumulatorValue(super_call_ref->this_function_var());
  __ AssertFunction(result_register());
  __ LoadP(result_register(),
           FieldMemOperand(result_register(), HeapObject::kMapOffset));
  __ LoadP(result_register(),
           FieldMemOperand(result_register(), Map::kPrototypeOffset));
  __ Push(result_register());

  // Push the arguments ("left-to-right") on the stack.
  ZoneList<Expression*>* args = expr->arguments();
  int arg_count = args->length();
  for (int i = 0; i < arg_count; i++) {
    VisitForStackValue(args->at(i));
  }

  // Call the construct call builtin that handles allocation and
  // constructor invocation.
  SetConstructCallPosition(expr);

  // Load new target into r6.
  VisitForAccumulatorValue(super_call_ref->new_target_var());
  __ mr(r6, result_register());

  // Load function and argument count into r1 and r0.
  __ mov(r3, Operand(arg_count));
  __ LoadP(r4, MemOperand(sp, arg_count * kPointerSize));

  __ Call(isolate()->builtins()->Construct(), RelocInfo::CODE_TARGET);

  RecordJSReturnSite(expr);

  // Restore context register.
  __ LoadP(cp, MemOperand(fp, StandardFrameConstants::kContextOffset));
  context()->Plug(r3);
}


void FullCodeGenerator::EmitIsSmi(CallRuntime* expr) {
  ZoneList<Expression*>* args = expr->arguments();
  DCHECK(args->length() == 1);

  VisitForAccumulatorValue(args->at(0));

  Label materialize_true, materialize_false;
  Label* if_true = NULL;
  Label* if_false = NULL;
  Label* fall_through = NULL;
  context()->PrepareTest(&materialize_true, &materialize_false, &if_true,
                         &if_false, &fall_through);

  PrepareForBailoutBeforeSplit(expr, true, if_true, if_false);
  __ TestIfSmi(r3, r0);
  Split(eq, if_true, if_false, fall_through, cr0);

  context()->Plug(if_true, if_false);
}


void FullCodeGenerator::EmitIsJSReceiver(CallRuntime* expr) {
  ZoneList<Expression*>* args = expr->arguments();
  DCHECK(args->length() == 1);

  VisitForAccumulatorValue(args->at(0));

  Label materialize_true, materialize_false;
  Label* if_true = NULL;
  Label* if_false = NULL;
  Label* fall_through = NULL;
  context()->PrepareTest(&materialize_true, &materialize_false, &if_true,
                         &if_false, &fall_through);

  __ JumpIfSmi(r3, if_false);
  __ CompareObjectType(r3, r4, r4, FIRST_JS_RECEIVER_TYPE);
  PrepareForBailoutBeforeSplit(expr, true, if_true, if_false);
  Split(ge, if_true, if_false, fall_through);

  context()->Plug(if_true, if_false);
}


void FullCodeGenerator::EmitIsSimdValue(CallRuntime* expr) {
  ZoneList<Expression*>* args = expr->arguments();
  DCHECK(args->length() == 1);

  VisitForAccumulatorValue(args->at(0));

  Label materialize_true, materialize_false;
  Label* if_true = NULL;
  Label* if_false = NULL;
  Label* fall_through = NULL;
  context()->PrepareTest(&materialize_true, &materialize_false, &if_true,
                         &if_false, &fall_through);

  __ JumpIfSmi(r3, if_false);
  __ CompareObjectType(r3, r4, r4, SIMD128_VALUE_TYPE);
  PrepareForBailoutBeforeSplit(expr, true, if_true, if_false);
  Split(eq, if_true, if_false, fall_through);

  context()->Plug(if_true, if_false);
}


void FullCodeGenerator::EmitIsFunction(CallRuntime* expr) {
  ZoneList<Expression*>* args = expr->arguments();
  DCHECK(args->length() == 1);

  VisitForAccumulatorValue(args->at(0));

  Label materialize_true, materialize_false;
  Label* if_true = NULL;
  Label* if_false = NULL;
  Label* fall_through = NULL;
  context()->PrepareTest(&materialize_true, &materialize_false, &if_true,
                         &if_false, &fall_through);

  __ JumpIfSmi(r3, if_false);
  __ CompareObjectType(r3, r4, r5, FIRST_FUNCTION_TYPE);
  PrepareForBailoutBeforeSplit(expr, true, if_true, if_false);
  Split(ge, if_true, if_false, fall_through);

  context()->Plug(if_true, if_false);
}


void FullCodeGenerator::EmitIsMinusZero(CallRuntime* expr) {
  ZoneList<Expression*>* args = expr->arguments();
  DCHECK(args->length() == 1);

  VisitForAccumulatorValue(args->at(0));

  Label materialize_true, materialize_false;
  Label* if_true = NULL;
  Label* if_false = NULL;
  Label* fall_through = NULL;
  context()->PrepareTest(&materialize_true, &materialize_false, &if_true,
                         &if_false, &fall_through);

  __ CheckMap(r3, r4, Heap::kHeapNumberMapRootIndex, if_false, DO_SMI_CHECK);
#if V8_TARGET_ARCH_PPC64
  __ LoadP(r4, FieldMemOperand(r3, HeapNumber::kValueOffset));
  __ li(r5, Operand(1));
  __ rotrdi(r5, r5, 1);  // r5 = 0x80000000_00000000
  __ cmp(r4, r5);
#else
  __ lwz(r5, FieldMemOperand(r3, HeapNumber::kExponentOffset));
  __ lwz(r4, FieldMemOperand(r3, HeapNumber::kMantissaOffset));
  Label skip;
  __ lis(r0, Operand(SIGN_EXT_IMM16(0x8000)));
  __ cmp(r5, r0);
  __ bne(&skip);
  __ cmpi(r4, Operand::Zero());
  __ bind(&skip);
#endif

  PrepareForBailoutBeforeSplit(expr, true, if_true, if_false);
  Split(eq, if_true, if_false, fall_through);

  context()->Plug(if_true, if_false);
}


void FullCodeGenerator::EmitIsArray(CallRuntime* expr) {
  ZoneList<Expression*>* args = expr->arguments();
  DCHECK(args->length() == 1);

  VisitForAccumulatorValue(args->at(0));

  Label materialize_true, materialize_false;
  Label* if_true = NULL;
  Label* if_false = NULL;
  Label* fall_through = NULL;
  context()->PrepareTest(&materialize_true, &materialize_false, &if_true,
                         &if_false, &fall_through);

  __ JumpIfSmi(r3, if_false);
  __ CompareObjectType(r3, r4, r4, JS_ARRAY_TYPE);
  PrepareForBailoutBeforeSplit(expr, true, if_true, if_false);
  Split(eq, if_true, if_false, fall_through);

  context()->Plug(if_true, if_false);
}


void FullCodeGenerator::EmitIsTypedArray(CallRuntime* expr) {
  ZoneList<Expression*>* args = expr->arguments();
  DCHECK(args->length() == 1);

  VisitForAccumulatorValue(args->at(0));

  Label materialize_true, materialize_false;
  Label* if_true = NULL;
  Label* if_false = NULL;
  Label* fall_through = NULL;
  context()->PrepareTest(&materialize_true, &materialize_false, &if_true,
                         &if_false, &fall_through);

  __ JumpIfSmi(r3, if_false);
  __ CompareObjectType(r3, r4, r4, JS_TYPED_ARRAY_TYPE);
  PrepareForBailoutBeforeSplit(expr, true, if_true, if_false);
  Split(eq, if_true, if_false, fall_through);

  context()->Plug(if_true, if_false);
}


void FullCodeGenerator::EmitIsRegExp(CallRuntime* expr) {
  ZoneList<Expression*>* args = expr->arguments();
  DCHECK(args->length() == 1);

  VisitForAccumulatorValue(args->at(0));

  Label materialize_true, materialize_false;
  Label* if_true = NULL;
  Label* if_false = NULL;
  Label* fall_through = NULL;
  context()->PrepareTest(&materialize_true, &materialize_false, &if_true,
                         &if_false, &fall_through);

  __ JumpIfSmi(r3, if_false);
  __ CompareObjectType(r3, r4, r4, JS_REGEXP_TYPE);
  PrepareForBailoutBeforeSplit(expr, true, if_true, if_false);
  Split(eq, if_true, if_false, fall_through);

  context()->Plug(if_true, if_false);
}


void FullCodeGenerator::EmitIsJSProxy(CallRuntime* expr) {
  ZoneList<Expression*>* args = expr->arguments();
  DCHECK(args->length() == 1);

  VisitForAccumulatorValue(args->at(0));

  Label materialize_true, materialize_false;
  Label* if_true = NULL;
  Label* if_false = NULL;
  Label* fall_through = NULL;
  context()->PrepareTest(&materialize_true, &materialize_false, &if_true,
                         &if_false, &fall_through);

  __ JumpIfSmi(r3, if_false);
  __ CompareObjectType(r3, r4, r4, JS_PROXY_TYPE);
  PrepareForBailoutBeforeSplit(expr, true, if_true, if_false);
  Split(eq, if_true, if_false, fall_through);

  context()->Plug(if_true, if_false);
}


void FullCodeGenerator::EmitObjectEquals(CallRuntime* expr) {
  ZoneList<Expression*>* args = expr->arguments();
  DCHECK(args->length() == 2);

  // Load the two objects into registers and perform the comparison.
  VisitForStackValue(args->at(0));
  VisitForAccumulatorValue(args->at(1));

  Label materialize_true, materialize_false;
  Label* if_true = NULL;
  Label* if_false = NULL;
  Label* fall_through = NULL;
  context()->PrepareTest(&materialize_true, &materialize_false, &if_true,
                         &if_false, &fall_through);

  __ pop(r4);
  __ cmp(r3, r4);
  PrepareForBailoutBeforeSplit(expr, true, if_true, if_false);
  Split(eq, if_true, if_false, fall_through);

  context()->Plug(if_true, if_false);
}


void FullCodeGenerator::EmitArguments(CallRuntime* expr) {
  ZoneList<Expression*>* args = expr->arguments();
  DCHECK(args->length() == 1);

  // ArgumentsAccessStub expects the key in r4 and the formal
  // parameter count in r3.
  VisitForAccumulatorValue(args->at(0));
  __ mr(r4, r3);
  __ LoadSmiLiteral(r3, Smi::FromInt(info_->scope()->num_parameters()));
  ArgumentsAccessStub stub(isolate(), ArgumentsAccessStub::READ_ELEMENT);
  __ CallStub(&stub);
  context()->Plug(r3);
}


void FullCodeGenerator::EmitArgumentsLength(CallRuntime* expr) {
  DCHECK(expr->arguments()->length() == 0);
  Label exit;
  // Get the number of formal parameters.
  __ LoadSmiLiteral(r3, Smi::FromInt(info_->scope()->num_parameters()));

  // Check if the calling frame is an arguments adaptor frame.
  __ LoadP(r5, MemOperand(fp, StandardFrameConstants::kCallerFPOffset));
  __ LoadP(r6, MemOperand(r5, StandardFrameConstants::kContextOffset));
  __ CmpSmiLiteral(r6, Smi::FromInt(StackFrame::ARGUMENTS_ADAPTOR), r0);
  __ bne(&exit);

  // Arguments adaptor case: Read the arguments length from the
  // adaptor frame.
  __ LoadP(r3, MemOperand(r5, ArgumentsAdaptorFrameConstants::kLengthOffset));

  __ bind(&exit);
  context()->Plug(r3);
}


void FullCodeGenerator::EmitClassOf(CallRuntime* expr) {
  ZoneList<Expression*>* args = expr->arguments();
  DCHECK(args->length() == 1);
  Label done, null, function, non_function_constructor;

  VisitForAccumulatorValue(args->at(0));

  // If the object is not a JSReceiver, we return null.
  __ JumpIfSmi(r3, &null);
  STATIC_ASSERT(LAST_JS_RECEIVER_TYPE == LAST_TYPE);
  __ CompareObjectType(r3, r3, r4, FIRST_JS_RECEIVER_TYPE);
  // Map is now in r3.
  __ blt(&null);

  // Return 'Function' for JSFunction objects.
  __ cmpi(r4, Operand(JS_FUNCTION_TYPE));
  __ beq(&function);

  // Check if the constructor in the map is a JS function.
  Register instance_type = r5;
  __ GetMapConstructor(r3, r3, r4, instance_type);
  __ cmpi(instance_type, Operand(JS_FUNCTION_TYPE));
  __ bne(&non_function_constructor);

  // r3 now contains the constructor function. Grab the
  // instance class name from there.
  __ LoadP(r3, FieldMemOperand(r3, JSFunction::kSharedFunctionInfoOffset));
  __ LoadP(r3,
           FieldMemOperand(r3, SharedFunctionInfo::kInstanceClassNameOffset));
  __ b(&done);

  // Functions have class 'Function'.
  __ bind(&function);
  __ LoadRoot(r3, Heap::kFunction_stringRootIndex);
  __ b(&done);

  // Objects with a non-function constructor have class 'Object'.
  __ bind(&non_function_constructor);
  __ LoadRoot(r3, Heap::kObject_stringRootIndex);
  __ b(&done);

  // Non-JS objects have class null.
  __ bind(&null);
  __ LoadRoot(r3, Heap::kNullValueRootIndex);

  // All done.
  __ bind(&done);

  context()->Plug(r3);
}


void FullCodeGenerator::EmitValueOf(CallRuntime* expr) {
  ZoneList<Expression*>* args = expr->arguments();
  DCHECK(args->length() == 1);
  VisitForAccumulatorValue(args->at(0));  // Load the object.

  Label done;
  // If the object is a smi return the object.
  __ JumpIfSmi(r3, &done);
  // If the object is not a value type, return the object.
  __ CompareObjectType(r3, r4, r4, JS_VALUE_TYPE);
  __ bne(&done);
  __ LoadP(r3, FieldMemOperand(r3, JSValue::kValueOffset));

  __ bind(&done);
  context()->Plug(r3);
}


void FullCodeGenerator::EmitIsDate(CallRuntime* expr) {
  ZoneList<Expression*>* args = expr->arguments();
  DCHECK_EQ(1, args->length());

  VisitForAccumulatorValue(args->at(0));

  Label materialize_true, materialize_false;
  Label* if_true = nullptr;
  Label* if_false = nullptr;
  Label* fall_through = nullptr;
  context()->PrepareTest(&materialize_true, &materialize_false, &if_true,
                         &if_false, &fall_through);

  __ JumpIfSmi(r3, if_false);
  __ CompareObjectType(r3, r4, r4, JS_DATE_TYPE);
  PrepareForBailoutBeforeSplit(expr, true, if_true, if_false);
  Split(eq, if_true, if_false, fall_through);

  context()->Plug(if_true, if_false);
}


void FullCodeGenerator::EmitOneByteSeqStringSetChar(CallRuntime* expr) {
  ZoneList<Expression*>* args = expr->arguments();
  DCHECK_EQ(3, args->length());

  Register string = r3;
  Register index = r4;
  Register value = r5;

  VisitForStackValue(args->at(0));        // index
  VisitForStackValue(args->at(1));        // value
  VisitForAccumulatorValue(args->at(2));  // string
  __ Pop(index, value);

  if (FLAG_debug_code) {
    __ TestIfSmi(value, r0);
    __ Check(eq, kNonSmiValue, cr0);
    __ TestIfSmi(index, r0);
    __ Check(eq, kNonSmiIndex, cr0);
    __ SmiUntag(index, index);
    static const uint32_t one_byte_seq_type = kSeqStringTag | kOneByteStringTag;
    __ EmitSeqStringSetCharCheck(string, index, value, one_byte_seq_type);
    __ SmiTag(index, index);
  }

  __ SmiUntag(value);
  __ addi(ip, string, Operand(SeqOneByteString::kHeaderSize - kHeapObjectTag));
  __ SmiToByteArrayOffset(r0, index);
  __ stbx(value, MemOperand(ip, r0));
  context()->Plug(string);
}


void FullCodeGenerator::EmitTwoByteSeqStringSetChar(CallRuntime* expr) {
  ZoneList<Expression*>* args = expr->arguments();
  DCHECK_EQ(3, args->length());

  Register string = r3;
  Register index = r4;
  Register value = r5;

  VisitForStackValue(args->at(0));        // index
  VisitForStackValue(args->at(1));        // value
  VisitForAccumulatorValue(args->at(2));  // string
  __ Pop(index, value);

  if (FLAG_debug_code) {
    __ TestIfSmi(value, r0);
    __ Check(eq, kNonSmiValue, cr0);
    __ TestIfSmi(index, r0);
    __ Check(eq, kNonSmiIndex, cr0);
    __ SmiUntag(index, index);
    static const uint32_t two_byte_seq_type = kSeqStringTag | kTwoByteStringTag;
    __ EmitSeqStringSetCharCheck(string, index, value, two_byte_seq_type);
    __ SmiTag(index, index);
  }

  __ SmiUntag(value);
  __ addi(ip, string, Operand(SeqTwoByteString::kHeaderSize - kHeapObjectTag));
  __ SmiToShortArrayOffset(r0, index);
  __ sthx(value, MemOperand(ip, r0));
  context()->Plug(string);
}


void FullCodeGenerator::EmitSetValueOf(CallRuntime* expr) {
  ZoneList<Expression*>* args = expr->arguments();
  DCHECK(args->length() == 2);
  VisitForStackValue(args->at(0));        // Load the object.
  VisitForAccumulatorValue(args->at(1));  // Load the value.
  __ pop(r4);                             // r3 = value. r4 = object.

  Label done;
  // If the object is a smi, return the value.
  __ JumpIfSmi(r4, &done);

  // If the object is not a value type, return the value.
  __ CompareObjectType(r4, r5, r5, JS_VALUE_TYPE);
  __ bne(&done);

  // Store the value.
  __ StoreP(r3, FieldMemOperand(r4, JSValue::kValueOffset), r0);
  // Update the write barrier.  Save the value as it will be
  // overwritten by the write barrier code and is needed afterward.
  __ mr(r5, r3);
  __ RecordWriteField(r4, JSValue::kValueOffset, r5, r6, kLRHasBeenSaved,
                      kDontSaveFPRegs);

  __ bind(&done);
  context()->Plug(r3);
}


void FullCodeGenerator::EmitToInteger(CallRuntime* expr) {
  ZoneList<Expression*>* args = expr->arguments();
  DCHECK_EQ(1, args->length());

  // Load the argument into r3 and convert it.
  VisitForAccumulatorValue(args->at(0));

  // Convert the object to an integer.
  Label done_convert;
  __ JumpIfSmi(r3, &done_convert);
  __ Push(r3);
  __ CallRuntime(Runtime::kToInteger);
  __ bind(&done_convert);
  context()->Plug(r3);
}


void FullCodeGenerator::EmitToName(CallRuntime* expr) {
  ZoneList<Expression*>* args = expr->arguments();
  DCHECK_EQ(1, args->length());

  // Load the argument into r3 and convert it.
  VisitForAccumulatorValue(args->at(0));

  Label convert, done_convert;
  __ JumpIfSmi(r3, &convert);
  STATIC_ASSERT(FIRST_NAME_TYPE == FIRST_TYPE);
  __ CompareObjectType(r3, r4, r4, LAST_NAME_TYPE);
  __ ble(&done_convert);
  __ bind(&convert);
  __ Push(r3);
  __ CallRuntime(Runtime::kToName);
  __ bind(&done_convert);
  context()->Plug(r3);
}


void FullCodeGenerator::EmitStringCharFromCode(CallRuntime* expr) {
  ZoneList<Expression*>* args = expr->arguments();
  DCHECK(args->length() == 1);
  VisitForAccumulatorValue(args->at(0));

  Label done;
  StringCharFromCodeGenerator generator(r3, r4);
  generator.GenerateFast(masm_);
  __ b(&done);

  NopRuntimeCallHelper call_helper;
  generator.GenerateSlow(masm_, call_helper);

  __ bind(&done);
  context()->Plug(r4);
}


void FullCodeGenerator::EmitStringCharCodeAt(CallRuntime* expr) {
  ZoneList<Expression*>* args = expr->arguments();
  DCHECK(args->length() == 2);
  VisitForStackValue(args->at(0));
  VisitForAccumulatorValue(args->at(1));

  Register object = r4;
  Register index = r3;
  Register result = r6;

  __ pop(object);

  Label need_conversion;
  Label index_out_of_range;
  Label done;
  StringCharCodeAtGenerator generator(object, index, result, &need_conversion,
                                      &need_conversion, &index_out_of_range,
                                      STRING_INDEX_IS_NUMBER);
  generator.GenerateFast(masm_);
  __ b(&done);

  __ bind(&index_out_of_range);
  // When the index is out of range, the spec requires us to return
  // NaN.
  __ LoadRoot(result, Heap::kNanValueRootIndex);
  __ b(&done);

  __ bind(&need_conversion);
  // Load the undefined value into the result register, which will
  // trigger conversion.
  __ LoadRoot(result, Heap::kUndefinedValueRootIndex);
  __ b(&done);

  NopRuntimeCallHelper call_helper;
  generator.GenerateSlow(masm_, NOT_PART_OF_IC_HANDLER, call_helper);

  __ bind(&done);
  context()->Plug(result);
}


void FullCodeGenerator::EmitStringCharAt(CallRuntime* expr) {
  ZoneList<Expression*>* args = expr->arguments();
  DCHECK(args->length() == 2);
  VisitForStackValue(args->at(0));
  VisitForAccumulatorValue(args->at(1));

  Register object = r4;
  Register index = r3;
  Register scratch = r6;
  Register result = r3;

  __ pop(object);

  Label need_conversion;
  Label index_out_of_range;
  Label done;
  StringCharAtGenerator generator(object, index, scratch, result,
                                  &need_conversion, &need_conversion,
                                  &index_out_of_range, STRING_INDEX_IS_NUMBER);
  generator.GenerateFast(masm_);
  __ b(&done);

  __ bind(&index_out_of_range);
  // When the index is out of range, the spec requires us to return
  // the empty string.
  __ LoadRoot(result, Heap::kempty_stringRootIndex);
  __ b(&done);

  __ bind(&need_conversion);
  // Move smi zero into the result register, which will trigger
  // conversion.
  __ LoadSmiLiteral(result, Smi::FromInt(0));
  __ b(&done);

  NopRuntimeCallHelper call_helper;
  generator.GenerateSlow(masm_, NOT_PART_OF_IC_HANDLER, call_helper);

  __ bind(&done);
  context()->Plug(result);
}


void FullCodeGenerator::EmitCall(CallRuntime* expr) {
  ZoneList<Expression*>* args = expr->arguments();
  DCHECK_LE(2, args->length());
  // Push target, receiver and arguments onto the stack.
  for (Expression* const arg : *args) {
    VisitForStackValue(arg);
  }
  PrepareForBailoutForId(expr->CallId(), NO_REGISTERS);
  // Move target to r4.
  int const argc = args->length() - 2;
  __ LoadP(r4, MemOperand(sp, (argc + 1) * kPointerSize));
  // Call the target.
  __ mov(r3, Operand(argc));
  __ Call(isolate()->builtins()->Call(), RelocInfo::CODE_TARGET);
  // Restore context register.
  __ LoadP(cp, MemOperand(fp, StandardFrameConstants::kContextOffset));
  // Discard the function left on TOS.
  context()->DropAndPlug(1, r3);
}


void FullCodeGenerator::EmitHasCachedArrayIndex(CallRuntime* expr) {
  ZoneList<Expression*>* args = expr->arguments();
  VisitForAccumulatorValue(args->at(0));

  Label materialize_true, materialize_false;
  Label* if_true = NULL;
  Label* if_false = NULL;
  Label* fall_through = NULL;
  context()->PrepareTest(&materialize_true, &materialize_false, &if_true,
                         &if_false, &fall_through);

  __ lwz(r3, FieldMemOperand(r3, String::kHashFieldOffset));
  // PPC - assume ip is free
  __ mov(ip, Operand(String::kContainsCachedArrayIndexMask));
  __ and_(r0, r3, ip, SetRC);
  PrepareForBailoutBeforeSplit(expr, true, if_true, if_false);
  Split(eq, if_true, if_false, fall_through, cr0);

  context()->Plug(if_true, if_false);
}


void FullCodeGenerator::EmitGetCachedArrayIndex(CallRuntime* expr) {
  ZoneList<Expression*>* args = expr->arguments();
  DCHECK(args->length() == 1);
  VisitForAccumulatorValue(args->at(0));

  __ AssertString(r3);

  __ lwz(r3, FieldMemOperand(r3, String::kHashFieldOffset));
  __ IndexFromHash(r3, r3);

  context()->Plug(r3);
}


void FullCodeGenerator::EmitGetSuperConstructor(CallRuntime* expr) {
  ZoneList<Expression*>* args = expr->arguments();
  DCHECK_EQ(1, args->length());
  VisitForAccumulatorValue(args->at(0));
  __ AssertFunction(r3);
  __ LoadP(r3, FieldMemOperand(r3, HeapObject::kMapOffset));
  __ LoadP(r3, FieldMemOperand(r3, Map::kPrototypeOffset));
  context()->Plug(r3);
}


void FullCodeGenerator::EmitFastOneByteArrayJoin(CallRuntime* expr) {
  Label bailout, done, one_char_separator, long_separator, non_trivial_array,
      not_size_one_array, loop, empty_separator_loop, one_char_separator_loop,
      one_char_separator_loop_entry, long_separator_loop;
  ZoneList<Expression*>* args = expr->arguments();
  DCHECK(args->length() == 2);
  VisitForStackValue(args->at(1));
  VisitForAccumulatorValue(args->at(0));

  // All aliases of the same register have disjoint lifetimes.
  Register array = r3;
  Register elements = no_reg;  // Will be r3.
  Register result = no_reg;    // Will be r3.
  Register separator = r4;
  Register array_length = r5;
  Register result_pos = no_reg;  // Will be r5
  Register string_length = r6;
  Register string = r7;
  Register element = r8;
  Register elements_end = r9;
  Register scratch1 = r10;
  Register scratch2 = r11;

  // Separator operand is on the stack.
  __ pop(separator);

  // Check that the array is a JSArray.
  __ JumpIfSmi(array, &bailout);
  __ CompareObjectType(array, scratch1, scratch2, JS_ARRAY_TYPE);
  __ bne(&bailout);

  // Check that the array has fast elements.
  __ CheckFastElements(scratch1, scratch2, &bailout);

  // If the array has length zero, return the empty string.
  __ LoadP(array_length, FieldMemOperand(array, JSArray::kLengthOffset));
  __ SmiUntag(array_length);
  __ cmpi(array_length, Operand::Zero());
  __ bne(&non_trivial_array);
  __ LoadRoot(r3, Heap::kempty_stringRootIndex);
  __ b(&done);

  __ bind(&non_trivial_array);

  // Get the FixedArray containing array's elements.
  elements = array;
  __ LoadP(elements, FieldMemOperand(array, JSArray::kElementsOffset));
  array = no_reg;  // End of array's live range.

  // Check that all array elements are sequential one-byte strings, and
  // accumulate the sum of their lengths, as a smi-encoded value.
  __ li(string_length, Operand::Zero());
  __ addi(element, elements, Operand(FixedArray::kHeaderSize - kHeapObjectTag));
  __ ShiftLeftImm(elements_end, array_length, Operand(kPointerSizeLog2));
  __ add(elements_end, element, elements_end);
  // Loop condition: while (element < elements_end).
  // Live values in registers:
  //   elements: Fixed array of strings.
  //   array_length: Length of the fixed array of strings (not smi)
  //   separator: Separator string
  //   string_length: Accumulated sum of string lengths (smi).
  //   element: Current array element.
  //   elements_end: Array end.
  if (generate_debug_code_) {
    __ cmpi(array_length, Operand::Zero());
    __ Assert(gt, kNoEmptyArraysHereInEmitFastOneByteArrayJoin);
  }
  __ bind(&loop);
  __ LoadP(string, MemOperand(element));
  __ addi(element, element, Operand(kPointerSize));
  __ JumpIfSmi(string, &bailout);
  __ LoadP(scratch1, FieldMemOperand(string, HeapObject::kMapOffset));
  __ lbz(scratch1, FieldMemOperand(scratch1, Map::kInstanceTypeOffset));
  __ JumpIfInstanceTypeIsNotSequentialOneByte(scratch1, scratch2, &bailout);
  __ LoadP(scratch1, FieldMemOperand(string, SeqOneByteString::kLengthOffset));

  __ AddAndCheckForOverflow(string_length, string_length, scratch1, scratch2,
                            r0);
  __ BranchOnOverflow(&bailout);

  __ cmp(element, elements_end);
  __ blt(&loop);

  // If array_length is 1, return elements[0], a string.
  __ cmpi(array_length, Operand(1));
  __ bne(&not_size_one_array);
  __ LoadP(r3, FieldMemOperand(elements, FixedArray::kHeaderSize));
  __ b(&done);

  __ bind(&not_size_one_array);

  // Live values in registers:
  //   separator: Separator string
  //   array_length: Length of the array.
  //   string_length: Sum of string lengths (smi).
  //   elements: FixedArray of strings.

  // Check that the separator is a flat one-byte string.
  __ JumpIfSmi(separator, &bailout);
  __ LoadP(scratch1, FieldMemOperand(separator, HeapObject::kMapOffset));
  __ lbz(scratch1, FieldMemOperand(scratch1, Map::kInstanceTypeOffset));
  __ JumpIfInstanceTypeIsNotSequentialOneByte(scratch1, scratch2, &bailout);

  // Add (separator length times array_length) - separator length to the
  // string_length to get the length of the result string.
  __ LoadP(scratch1,
           FieldMemOperand(separator, SeqOneByteString::kLengthOffset));
  __ sub(string_length, string_length, scratch1);
#if V8_TARGET_ARCH_PPC64
  __ SmiUntag(scratch1, scratch1);
  __ Mul(scratch2, array_length, scratch1);
  // Check for smi overflow. No overflow if higher 33 bits of 64-bit result are
  // zero.
  __ ShiftRightImm(ip, scratch2, Operand(31), SetRC);
  __ bne(&bailout, cr0);
  __ SmiTag(scratch2, scratch2);
#else
  // array_length is not smi but the other values are, so the result is a smi
  __ mullw(scratch2, array_length, scratch1);
  __ mulhw(ip, array_length, scratch1);
  // Check for smi overflow. No overflow if higher 33 bits of 64-bit result are
  // zero.
  __ cmpi(ip, Operand::Zero());
  __ bne(&bailout);
  __ cmpwi(scratch2, Operand::Zero());
  __ blt(&bailout);
#endif

  __ AddAndCheckForOverflow(string_length, string_length, scratch2, scratch1,
                            r0);
  __ BranchOnOverflow(&bailout);
  __ SmiUntag(string_length);

  // Bailout for large object allocations.
  __ Cmpi(string_length, Operand(Page::kMaxRegularHeapObjectSize), r0);
  __ bgt(&bailout);

  // Get first element in the array to free up the elements register to be used
  // for the result.
  __ addi(element, elements, Operand(FixedArray::kHeaderSize - kHeapObjectTag));
  result = elements;  // End of live range for elements.
  elements = no_reg;
  // Live values in registers:
  //   element: First array element
  //   separator: Separator string
  //   string_length: Length of result string (not smi)
  //   array_length: Length of the array.
  __ AllocateOneByteString(result, string_length, scratch1, scratch2,
                           elements_end, &bailout);
  // Prepare for looping. Set up elements_end to end of the array. Set
  // result_pos to the position of the result where to write the first
  // character.
  __ ShiftLeftImm(elements_end, array_length, Operand(kPointerSizeLog2));
  __ add(elements_end, element, elements_end);
  result_pos = array_length;  // End of live range for array_length.
  array_length = no_reg;
  __ addi(result_pos, result,
          Operand(SeqOneByteString::kHeaderSize - kHeapObjectTag));

  // Check the length of the separator.
  __ LoadP(scratch1,
           FieldMemOperand(separator, SeqOneByteString::kLengthOffset));
  __ CmpSmiLiteral(scratch1, Smi::FromInt(1), r0);
  __ beq(&one_char_separator);
  __ bgt(&long_separator);

  // Empty separator case
  __ bind(&empty_separator_loop);
  // Live values in registers:
  //   result_pos: the position to which we are currently copying characters.
  //   element: Current array element.
  //   elements_end: Array end.

  // Copy next array element to the result.
  __ LoadP(string, MemOperand(element));
  __ addi(element, element, Operand(kPointerSize));
  __ LoadP(string_length, FieldMemOperand(string, String::kLengthOffset));
  __ SmiUntag(string_length);
  __ addi(string, string,
          Operand(SeqOneByteString::kHeaderSize - kHeapObjectTag));
  __ CopyBytes(string, result_pos, string_length, scratch1);
  __ cmp(element, elements_end);
  __ blt(&empty_separator_loop);  // End while (element < elements_end).
  DCHECK(result.is(r3));
  __ b(&done);

  // One-character separator case
  __ bind(&one_char_separator);
  // Replace separator with its one-byte character value.
  __ lbz(separator, FieldMemOperand(separator, SeqOneByteString::kHeaderSize));
  // Jump into the loop after the code that copies the separator, so the first
  // element is not preceded by a separator
  __ b(&one_char_separator_loop_entry);

  __ bind(&one_char_separator_loop);
  // Live values in registers:
  //   result_pos: the position to which we are currently copying characters.
  //   element: Current array element.
  //   elements_end: Array end.
  //   separator: Single separator one-byte char (in lower byte).

  // Copy the separator character to the result.
  __ stb(separator, MemOperand(result_pos));
  __ addi(result_pos, result_pos, Operand(1));

  // Copy next array element to the result.
  __ bind(&one_char_separator_loop_entry);
  __ LoadP(string, MemOperand(element));
  __ addi(element, element, Operand(kPointerSize));
  __ LoadP(string_length, FieldMemOperand(string, String::kLengthOffset));
  __ SmiUntag(string_length);
  __ addi(string, string,
          Operand(SeqOneByteString::kHeaderSize - kHeapObjectTag));
  __ CopyBytes(string, result_pos, string_length, scratch1);
  __ cmpl(element, elements_end);
  __ blt(&one_char_separator_loop);  // End while (element < elements_end).
  DCHECK(result.is(r3));
  __ b(&done);

  // Long separator case (separator is more than one character). Entry is at the
  // label long_separator below.
  __ bind(&long_separator_loop);
  // Live values in registers:
  //   result_pos: the position to which we are currently copying characters.
  //   element: Current array element.
  //   elements_end: Array end.
  //   separator: Separator string.

  // Copy the separator to the result.
  __ LoadP(string_length, FieldMemOperand(separator, String::kLengthOffset));
  __ SmiUntag(string_length);
  __ addi(string, separator,
          Operand(SeqOneByteString::kHeaderSize - kHeapObjectTag));
  __ CopyBytes(string, result_pos, string_length, scratch1);

  __ bind(&long_separator);
  __ LoadP(string, MemOperand(element));
  __ addi(element, element, Operand(kPointerSize));
  __ LoadP(string_length, FieldMemOperand(string, String::kLengthOffset));
  __ SmiUntag(string_length);
  __ addi(string, string,
          Operand(SeqOneByteString::kHeaderSize - kHeapObjectTag));
  __ CopyBytes(string, result_pos, string_length, scratch1);
  __ cmpl(element, elements_end);
  __ blt(&long_separator_loop);  // End while (element < elements_end).
  DCHECK(result.is(r3));
  __ b(&done);

  __ bind(&bailout);
  __ LoadRoot(r3, Heap::kUndefinedValueRootIndex);
  __ bind(&done);
  context()->Plug(r3);
}


void FullCodeGenerator::EmitDebugIsActive(CallRuntime* expr) {
  DCHECK(expr->arguments()->length() == 0);
  ExternalReference debug_is_active =
      ExternalReference::debug_is_active_address(isolate());
  __ mov(ip, Operand(debug_is_active));
  __ lbz(r3, MemOperand(ip));
  __ SmiTag(r3);
  context()->Plug(r3);
}


void FullCodeGenerator::EmitCreateIterResultObject(CallRuntime* expr) {
  ZoneList<Expression*>* args = expr->arguments();
  DCHECK_EQ(2, args->length());
  VisitForStackValue(args->at(0));
  VisitForStackValue(args->at(1));

  Label runtime, done;

  __ Allocate(JSIteratorResult::kSize, r3, r5, r6, &runtime, TAG_OBJECT);
  __ LoadNativeContextSlot(Context::ITERATOR_RESULT_MAP_INDEX, r4);
  __ Pop(r5, r6);
  __ LoadRoot(r7, Heap::kEmptyFixedArrayRootIndex);
  __ StoreP(r4, FieldMemOperand(r3, HeapObject::kMapOffset), r0);
  __ StoreP(r7, FieldMemOperand(r3, JSObject::kPropertiesOffset), r0);
  __ StoreP(r7, FieldMemOperand(r3, JSObject::kElementsOffset), r0);
  __ StoreP(r5, FieldMemOperand(r3, JSIteratorResult::kValueOffset), r0);
  __ StoreP(r6, FieldMemOperand(r3, JSIteratorResult::kDoneOffset), r0);
  STATIC_ASSERT(JSIteratorResult::kSize == 5 * kPointerSize);
  __ b(&done);

  __ bind(&runtime);
  __ CallRuntime(Runtime::kCreateIterResultObject);

  __ bind(&done);
  context()->Plug(r3);
}


void FullCodeGenerator::EmitLoadJSRuntimeFunction(CallRuntime* expr) {
  // Push undefined as the receiver.
  __ LoadRoot(r3, Heap::kUndefinedValueRootIndex);
  __ push(r3);

  __ LoadNativeContextSlot(expr->context_index(), r3);
}


void FullCodeGenerator::EmitCallJSRuntimeFunction(CallRuntime* expr) {
  ZoneList<Expression*>* args = expr->arguments();
  int arg_count = args->length();

  SetCallPosition(expr);
  __ LoadP(r4, MemOperand(sp, (arg_count + 1) * kPointerSize), r0);
  __ mov(r3, Operand(arg_count));
  __ Call(isolate()->builtins()->Call(ConvertReceiverMode::kNullOrUndefined),
          RelocInfo::CODE_TARGET);
}


void FullCodeGenerator::VisitCallRuntime(CallRuntime* expr) {
  ZoneList<Expression*>* args = expr->arguments();
  int arg_count = args->length();

  if (expr->is_jsruntime()) {
    Comment cmnt(masm_, "[ CallRuntime");
    EmitLoadJSRuntimeFunction(expr);

    // Push the target function under the receiver.
    __ LoadP(ip, MemOperand(sp, 0));
    __ push(ip);
    __ StoreP(r3, MemOperand(sp, kPointerSize));

    // Push the arguments ("left-to-right").
    for (int i = 0; i < arg_count; i++) {
      VisitForStackValue(args->at(i));
    }

    PrepareForBailoutForId(expr->CallId(), NO_REGISTERS);
    EmitCallJSRuntimeFunction(expr);

    // Restore context register.
    __ LoadP(cp, MemOperand(fp, StandardFrameConstants::kContextOffset));

    context()->DropAndPlug(1, r3);

  } else {
    const Runtime::Function* function = expr->function();
    switch (function->function_id) {
#define CALL_INTRINSIC_GENERATOR(Name)     \
  case Runtime::kInline##Name: {           \
    Comment cmnt(masm_, "[ Inline" #Name); \
    return Emit##Name(expr);               \
  }
      FOR_EACH_FULL_CODE_INTRINSIC(CALL_INTRINSIC_GENERATOR)
#undef CALL_INTRINSIC_GENERATOR
      default: {
        Comment cmnt(masm_, "[ CallRuntime for unhandled intrinsic");
        // Push the arguments ("left-to-right").
        for (int i = 0; i < arg_count; i++) {
          VisitForStackValue(args->at(i));
        }

        // Call the C runtime function.
        PrepareForBailoutForId(expr->CallId(), NO_REGISTERS);
        __ CallRuntime(expr->function(), arg_count);
        context()->Plug(r3);
      }
    }
  }
}


void FullCodeGenerator::VisitUnaryOperation(UnaryOperation* expr) {
  switch (expr->op()) {
    case Token::DELETE: {
      Comment cmnt(masm_, "[ UnaryOperation (DELETE)");
      Property* property = expr->expression()->AsProperty();
      VariableProxy* proxy = expr->expression()->AsVariableProxy();

      if (property != NULL) {
        VisitForStackValue(property->obj());
        VisitForStackValue(property->key());
        __ CallRuntime(is_strict(language_mode())
                           ? Runtime::kDeleteProperty_Strict
                           : Runtime::kDeleteProperty_Sloppy);
        context()->Plug(r3);
      } else if (proxy != NULL) {
        Variable* var = proxy->var();
        // Delete of an unqualified identifier is disallowed in strict mode but
        // "delete this" is allowed.
        bool is_this = var->HasThisName(isolate());
        DCHECK(is_sloppy(language_mode()) || is_this);
        if (var->IsUnallocatedOrGlobalSlot()) {
          __ LoadGlobalObject(r5);
          __ mov(r4, Operand(var->name()));
          __ Push(r5, r4);
          __ CallRuntime(Runtime::kDeleteProperty_Sloppy);
          context()->Plug(r3);
        } else if (var->IsStackAllocated() || var->IsContextSlot()) {
          // Result of deleting non-global, non-dynamic variables is false.
          // The subexpression does not have side effects.
          context()->Plug(is_this);
        } else {
          // Non-global variable.  Call the runtime to try to delete from the
          // context where the variable was introduced.
          DCHECK(!context_register().is(r5));
          __ mov(r5, Operand(var->name()));
          __ Push(context_register(), r5);
          __ CallRuntime(Runtime::kDeleteLookupSlot);
          context()->Plug(r3);
        }
      } else {
        // Result of deleting non-property, non-variable reference is true.
        // The subexpression may have side effects.
        VisitForEffect(expr->expression());
        context()->Plug(true);
      }
      break;
    }

    case Token::VOID: {
      Comment cmnt(masm_, "[ UnaryOperation (VOID)");
      VisitForEffect(expr->expression());
      context()->Plug(Heap::kUndefinedValueRootIndex);
      break;
    }

    case Token::NOT: {
      Comment cmnt(masm_, "[ UnaryOperation (NOT)");
      if (context()->IsEffect()) {
        // Unary NOT has no side effects so it's only necessary to visit the
        // subexpression.  Match the optimizing compiler by not branching.
        VisitForEffect(expr->expression());
      } else if (context()->IsTest()) {
        const TestContext* test = TestContext::cast(context());
        // The labels are swapped for the recursive call.
        VisitForControl(expr->expression(), test->false_label(),
                        test->true_label(), test->fall_through());
        context()->Plug(test->true_label(), test->false_label());
      } else {
        // We handle value contexts explicitly rather than simply visiting
        // for control and plugging the control flow into the context,
        // because we need to prepare a pair of extra administrative AST ids
        // for the optimizing compiler.
        DCHECK(context()->IsAccumulatorValue() || context()->IsStackValue());
        Label materialize_true, materialize_false, done;
        VisitForControl(expr->expression(), &materialize_false,
                        &materialize_true, &materialize_true);
        __ bind(&materialize_true);
        PrepareForBailoutForId(expr->MaterializeTrueId(), NO_REGISTERS);
        __ LoadRoot(r3, Heap::kTrueValueRootIndex);
        if (context()->IsStackValue()) __ push(r3);
        __ b(&done);
        __ bind(&materialize_false);
        PrepareForBailoutForId(expr->MaterializeFalseId(), NO_REGISTERS);
        __ LoadRoot(r3, Heap::kFalseValueRootIndex);
        if (context()->IsStackValue()) __ push(r3);
        __ bind(&done);
      }
      break;
    }

    case Token::TYPEOF: {
      Comment cmnt(masm_, "[ UnaryOperation (TYPEOF)");
      {
        AccumulatorValueContext context(this);
        VisitForTypeofValue(expr->expression());
      }
      __ mr(r6, r3);
      TypeofStub typeof_stub(isolate());
      __ CallStub(&typeof_stub);
      context()->Plug(r3);
      break;
    }

    default:
      UNREACHABLE();
  }
}


void FullCodeGenerator::VisitCountOperation(CountOperation* expr) {
  DCHECK(expr->expression()->IsValidReferenceExpressionOrThis());

  Comment cmnt(masm_, "[ CountOperation");

  Property* prop = expr->expression()->AsProperty();
  LhsKind assign_type = Property::GetAssignType(prop);

  // Evaluate expression and get value.
  if (assign_type == VARIABLE) {
    DCHECK(expr->expression()->AsVariableProxy()->var() != NULL);
    AccumulatorValueContext context(this);
    EmitVariableLoad(expr->expression()->AsVariableProxy());
  } else {
    // Reserve space for result of postfix operation.
    if (expr->is_postfix() && !context()->IsEffect()) {
      __ LoadSmiLiteral(ip, Smi::FromInt(0));
      __ push(ip);
    }
    switch (assign_type) {
      case NAMED_PROPERTY: {
        // Put the object both on the stack and in the register.
        VisitForStackValue(prop->obj());
        __ LoadP(LoadDescriptor::ReceiverRegister(), MemOperand(sp, 0));
        EmitNamedPropertyLoad(prop);
        break;
      }

      case NAMED_SUPER_PROPERTY: {
        VisitForStackValue(prop->obj()->AsSuperPropertyReference()->this_var());
        VisitForAccumulatorValue(
            prop->obj()->AsSuperPropertyReference()->home_object());
        __ Push(result_register());
        const Register scratch = r4;
        __ LoadP(scratch, MemOperand(sp, kPointerSize));
        __ Push(scratch, result_register());
        EmitNamedSuperPropertyLoad(prop);
        break;
      }

      case KEYED_SUPER_PROPERTY: {
        VisitForStackValue(prop->obj()->AsSuperPropertyReference()->this_var());
        VisitForAccumulatorValue(
            prop->obj()->AsSuperPropertyReference()->home_object());
        const Register scratch = r4;
        const Register scratch1 = r5;
        __ mr(scratch, result_register());
        VisitForAccumulatorValue(prop->key());
        __ Push(scratch, result_register());
        __ LoadP(scratch1, MemOperand(sp, 2 * kPointerSize));
        __ Push(scratch1, scratch, result_register());
        EmitKeyedSuperPropertyLoad(prop);
        break;
      }

      case KEYED_PROPERTY: {
        VisitForStackValue(prop->obj());
        VisitForStackValue(prop->key());
        __ LoadP(LoadDescriptor::ReceiverRegister(),
                 MemOperand(sp, 1 * kPointerSize));
        __ LoadP(LoadDescriptor::NameRegister(), MemOperand(sp, 0));
        EmitKeyedPropertyLoad(prop);
        break;
      }

      case VARIABLE:
        UNREACHABLE();
    }
  }

  // We need a second deoptimization point after loading the value
  // in case evaluating the property load my have a side effect.
  if (assign_type == VARIABLE) {
    PrepareForBailout(expr->expression(), TOS_REG);
  } else {
    PrepareForBailoutForId(prop->LoadId(), TOS_REG);
  }

  // Inline smi case if we are in a loop.
  Label stub_call, done;
  JumpPatchSite patch_site(masm_);

  int count_value = expr->op() == Token::INC ? 1 : -1;
  if (ShouldInlineSmiCase(expr->op())) {
    Label slow;
    patch_site.EmitJumpIfNotSmi(r3, &slow);

    // Save result for postfix expressions.
    if (expr->is_postfix()) {
      if (!context()->IsEffect()) {
        // Save the result on the stack. If we have a named or keyed property
        // we store the result under the receiver that is currently on top
        // of the stack.
        switch (assign_type) {
          case VARIABLE:
            __ push(r3);
            break;
          case NAMED_PROPERTY:
            __ StoreP(r3, MemOperand(sp, kPointerSize));
            break;
          case NAMED_SUPER_PROPERTY:
            __ StoreP(r3, MemOperand(sp, 2 * kPointerSize));
            break;
          case KEYED_PROPERTY:
            __ StoreP(r3, MemOperand(sp, 2 * kPointerSize));
            break;
          case KEYED_SUPER_PROPERTY:
            __ StoreP(r3, MemOperand(sp, 3 * kPointerSize));
            break;
        }
      }
    }

    Register scratch1 = r4;
    Register scratch2 = r5;
    __ LoadSmiLiteral(scratch1, Smi::FromInt(count_value));
    __ AddAndCheckForOverflow(r3, r3, scratch1, scratch2, r0);
    __ BranchOnNoOverflow(&done);
    // Call stub. Undo operation first.
    __ sub(r3, r3, scratch1);
    __ b(&stub_call);
    __ bind(&slow);
  }
  if (!is_strong(language_mode())) {
    ToNumberStub convert_stub(isolate());
    __ CallStub(&convert_stub);
    PrepareForBailoutForId(expr->ToNumberId(), TOS_REG);
  }

  // Save result for postfix expressions.
  if (expr->is_postfix()) {
    if (!context()->IsEffect()) {
      // Save the result on the stack. If we have a named or keyed property
      // we store the result under the receiver that is currently on top
      // of the stack.
      switch (assign_type) {
        case VARIABLE:
          __ push(r3);
          break;
        case NAMED_PROPERTY:
          __ StoreP(r3, MemOperand(sp, kPointerSize));
          break;
        case NAMED_SUPER_PROPERTY:
          __ StoreP(r3, MemOperand(sp, 2 * kPointerSize));
          break;
        case KEYED_PROPERTY:
          __ StoreP(r3, MemOperand(sp, 2 * kPointerSize));
          break;
        case KEYED_SUPER_PROPERTY:
          __ StoreP(r3, MemOperand(sp, 3 * kPointerSize));
          break;
      }
    }
  }

  __ bind(&stub_call);
  __ mr(r4, r3);
  __ LoadSmiLiteral(r3, Smi::FromInt(count_value));

  SetExpressionPosition(expr);

  Handle<Code> code = CodeFactory::BinaryOpIC(isolate(), Token::ADD,
                                              strength(language_mode())).code();
  CallIC(code, expr->CountBinOpFeedbackId());
  patch_site.EmitPatchInfo();
  __ bind(&done);

  if (is_strong(language_mode())) {
    PrepareForBailoutForId(expr->ToNumberId(), TOS_REG);
  }
  // Store the value returned in r3.
  switch (assign_type) {
    case VARIABLE:
      if (expr->is_postfix()) {
        {
          EffectContext context(this);
          EmitVariableAssignment(expr->expression()->AsVariableProxy()->var(),
                                 Token::ASSIGN, expr->CountSlot());
          PrepareForBailoutForId(expr->AssignmentId(), TOS_REG);
          context.Plug(r3);
        }
        // For all contexts except EffectConstant We have the result on
        // top of the stack.
        if (!context()->IsEffect()) {
          context()->PlugTOS();
        }
      } else {
        EmitVariableAssignment(expr->expression()->AsVariableProxy()->var(),
                               Token::ASSIGN, expr->CountSlot());
        PrepareForBailoutForId(expr->AssignmentId(), TOS_REG);
        context()->Plug(r3);
      }
      break;
    case NAMED_PROPERTY: {
      __ mov(StoreDescriptor::NameRegister(),
             Operand(prop->key()->AsLiteral()->value()));
      __ pop(StoreDescriptor::ReceiverRegister());
      EmitLoadStoreICSlot(expr->CountSlot());
      CallStoreIC();
      PrepareForBailoutForId(expr->AssignmentId(), TOS_REG);
      if (expr->is_postfix()) {
        if (!context()->IsEffect()) {
          context()->PlugTOS();
        }
      } else {
        context()->Plug(r3);
      }
      break;
    }
    case NAMED_SUPER_PROPERTY: {
      EmitNamedSuperPropertyStore(prop);
      if (expr->is_postfix()) {
        if (!context()->IsEffect()) {
          context()->PlugTOS();
        }
      } else {
        context()->Plug(r3);
      }
      break;
    }
    case KEYED_SUPER_PROPERTY: {
      EmitKeyedSuperPropertyStore(prop);
      if (expr->is_postfix()) {
        if (!context()->IsEffect()) {
          context()->PlugTOS();
        }
      } else {
        context()->Plug(r3);
      }
      break;
    }
    case KEYED_PROPERTY: {
      __ Pop(StoreDescriptor::ReceiverRegister(),
             StoreDescriptor::NameRegister());
      Handle<Code> ic =
          CodeFactory::KeyedStoreIC(isolate(), language_mode()).code();
      EmitLoadStoreICSlot(expr->CountSlot());
      CallIC(ic);
      PrepareForBailoutForId(expr->AssignmentId(), TOS_REG);
      if (expr->is_postfix()) {
        if (!context()->IsEffect()) {
          context()->PlugTOS();
        }
      } else {
        context()->Plug(r3);
      }
      break;
    }
  }
}


void FullCodeGenerator::EmitLiteralCompareTypeof(Expression* expr,
                                                 Expression* sub_expr,
                                                 Handle<String> check) {
  Label materialize_true, materialize_false;
  Label* if_true = NULL;
  Label* if_false = NULL;
  Label* fall_through = NULL;
  context()->PrepareTest(&materialize_true, &materialize_false, &if_true,
                         &if_false, &fall_through);

  {
    AccumulatorValueContext context(this);
    VisitForTypeofValue(sub_expr);
  }
  PrepareForBailoutBeforeSplit(expr, true, if_true, if_false);

  Factory* factory = isolate()->factory();
  if (String::Equals(check, factory->number_string())) {
    __ JumpIfSmi(r3, if_true);
    __ LoadP(r3, FieldMemOperand(r3, HeapObject::kMapOffset));
    __ LoadRoot(ip, Heap::kHeapNumberMapRootIndex);
    __ cmp(r3, ip);
    Split(eq, if_true, if_false, fall_through);
  } else if (String::Equals(check, factory->string_string())) {
    __ JumpIfSmi(r3, if_false);
    __ CompareObjectType(r3, r3, r4, FIRST_NONSTRING_TYPE);
    Split(lt, if_true, if_false, fall_through);
  } else if (String::Equals(check, factory->symbol_string())) {
    __ JumpIfSmi(r3, if_false);
    __ CompareObjectType(r3, r3, r4, SYMBOL_TYPE);
    Split(eq, if_true, if_false, fall_through);
  } else if (String::Equals(check, factory->boolean_string())) {
    __ CompareRoot(r3, Heap::kTrueValueRootIndex);
    __ beq(if_true);
    __ CompareRoot(r3, Heap::kFalseValueRootIndex);
    Split(eq, if_true, if_false, fall_through);
  } else if (String::Equals(check, factory->undefined_string())) {
    __ CompareRoot(r3, Heap::kUndefinedValueRootIndex);
    __ beq(if_true);
    __ JumpIfSmi(r3, if_false);
    // Check for undetectable objects => true.
    __ LoadP(r3, FieldMemOperand(r3, HeapObject::kMapOffset));
    __ lbz(r4, FieldMemOperand(r3, Map::kBitFieldOffset));
    __ andi(r0, r4, Operand(1 << Map::kIsUndetectable));
    Split(ne, if_true, if_false, fall_through, cr0);

  } else if (String::Equals(check, factory->function_string())) {
    __ JumpIfSmi(r3, if_false);
    __ LoadP(r3, FieldMemOperand(r3, HeapObject::kMapOffset));
    __ lbz(r4, FieldMemOperand(r3, Map::kBitFieldOffset));
    __ andi(r4, r4,
            Operand((1 << Map::kIsCallable) | (1 << Map::kIsUndetectable)));
    __ cmpi(r4, Operand(1 << Map::kIsCallable));
    Split(eq, if_true, if_false, fall_through);
  } else if (String::Equals(check, factory->object_string())) {
    __ JumpIfSmi(r3, if_false);
    __ CompareRoot(r3, Heap::kNullValueRootIndex);
    __ beq(if_true);
    STATIC_ASSERT(LAST_JS_RECEIVER_TYPE == LAST_TYPE);
    __ CompareObjectType(r3, r3, r4, FIRST_JS_RECEIVER_TYPE);
    __ blt(if_false);
    // Check for callable or undetectable objects => false.
    __ lbz(r4, FieldMemOperand(r3, Map::kBitFieldOffset));
    __ andi(r0, r4,
            Operand((1 << Map::kIsCallable) | (1 << Map::kIsUndetectable)));
    Split(eq, if_true, if_false, fall_through, cr0);
// clang-format off
#define SIMD128_TYPE(TYPE, Type, type, lane_count, lane_type)   \
  } else if (String::Equals(check, factory->type##_string())) { \
    __ JumpIfSmi(r3, if_false);                                 \
    __ LoadP(r3, FieldMemOperand(r3, HeapObject::kMapOffset));    \
    __ CompareRoot(r3, Heap::k##Type##MapRootIndex);            \
    Split(eq, if_true, if_false, fall_through);
  SIMD128_TYPES(SIMD128_TYPE)
#undef SIMD128_TYPE
    // clang-format on
  } else {
    if (if_false != fall_through) __ b(if_false);
  }
  context()->Plug(if_true, if_false);
}


void FullCodeGenerator::VisitCompareOperation(CompareOperation* expr) {
  Comment cmnt(masm_, "[ CompareOperation");
  SetExpressionPosition(expr);

  // First we try a fast inlined version of the compare when one of
  // the operands is a literal.
  if (TryLiteralCompare(expr)) return;

  // Always perform the comparison for its control flow.  Pack the result
  // into the expression's context after the comparison is performed.
  Label materialize_true, materialize_false;
  Label* if_true = NULL;
  Label* if_false = NULL;
  Label* fall_through = NULL;
  context()->PrepareTest(&materialize_true, &materialize_false, &if_true,
                         &if_false, &fall_through);

  Token::Value op = expr->op();
  VisitForStackValue(expr->left());
  switch (op) {
    case Token::IN:
      VisitForStackValue(expr->right());
      __ CallRuntime(Runtime::kHasProperty);
      PrepareForBailoutBeforeSplit(expr, false, NULL, NULL);
      __ CompareRoot(r3, Heap::kTrueValueRootIndex);
      Split(eq, if_true, if_false, fall_through);
      break;

    case Token::INSTANCEOF: {
      VisitForAccumulatorValue(expr->right());
      __ pop(r4);
      InstanceOfStub stub(isolate());
      __ CallStub(&stub);
      PrepareForBailoutBeforeSplit(expr, false, NULL, NULL);
      __ CompareRoot(r3, Heap::kTrueValueRootIndex);
      Split(eq, if_true, if_false, fall_through);
      break;
    }

    default: {
      VisitForAccumulatorValue(expr->right());
      Condition cond = CompareIC::ComputeCondition(op);
      __ pop(r4);

      bool inline_smi_code = ShouldInlineSmiCase(op);
      JumpPatchSite patch_site(masm_);
      if (inline_smi_code) {
        Label slow_case;
        __ orx(r5, r3, r4);
        patch_site.EmitJumpIfNotSmi(r5, &slow_case);
        __ cmp(r4, r3);
        Split(cond, if_true, if_false, NULL);
        __ bind(&slow_case);
      }

      Handle<Code> ic = CodeFactory::CompareIC(
                            isolate(), op, strength(language_mode())).code();
      CallIC(ic, expr->CompareOperationFeedbackId());
      patch_site.EmitPatchInfo();
      PrepareForBailoutBeforeSplit(expr, true, if_true, if_false);
      __ cmpi(r3, Operand::Zero());
      Split(cond, if_true, if_false, fall_through);
    }
  }

  // Convert the result of the comparison into one expected for this
  // expression's context.
  context()->Plug(if_true, if_false);
}


void FullCodeGenerator::EmitLiteralCompareNil(CompareOperation* expr,
                                              Expression* sub_expr,
                                              NilValue nil) {
  Label materialize_true, materialize_false;
  Label* if_true = NULL;
  Label* if_false = NULL;
  Label* fall_through = NULL;
  context()->PrepareTest(&materialize_true, &materialize_false, &if_true,
                         &if_false, &fall_through);

  VisitForAccumulatorValue(sub_expr);
  PrepareForBailoutBeforeSplit(expr, true, if_true, if_false);
  if (expr->op() == Token::EQ_STRICT) {
    Heap::RootListIndex nil_value = nil == kNullValue
                                        ? Heap::kNullValueRootIndex
                                        : Heap::kUndefinedValueRootIndex;
    __ LoadRoot(r4, nil_value);
    __ cmp(r3, r4);
    Split(eq, if_true, if_false, fall_through);
  } else {
    Handle<Code> ic = CompareNilICStub::GetUninitialized(isolate(), nil);
    CallIC(ic, expr->CompareOperationFeedbackId());
    __ CompareRoot(r3, Heap::kTrueValueRootIndex);
    Split(eq, if_true, if_false, fall_through);
  }
  context()->Plug(if_true, if_false);
}


void FullCodeGenerator::VisitThisFunction(ThisFunction* expr) {
  __ LoadP(r3, MemOperand(fp, JavaScriptFrameConstants::kFunctionOffset));
  context()->Plug(r3);
}


Register FullCodeGenerator::result_register() { return r3; }


Register FullCodeGenerator::context_register() { return cp; }


void FullCodeGenerator::StoreToFrameField(int frame_offset, Register value) {
  DCHECK_EQ(static_cast<int>(POINTER_SIZE_ALIGN(frame_offset)), frame_offset);
  __ StoreP(value, MemOperand(fp, frame_offset), r0);
}


void FullCodeGenerator::LoadContextField(Register dst, int context_index) {
  __ LoadP(dst, ContextMemOperand(cp, context_index), r0);
}


void FullCodeGenerator::PushFunctionArgumentForContextAllocation() {
  Scope* closure_scope = scope()->ClosureScope();
  if (closure_scope->is_script_scope() ||
      closure_scope->is_module_scope()) {
    // Contexts nested in the native context have a canonical empty function
    // as their closure, not the anonymous closure containing the global
    // code.
    __ LoadNativeContextSlot(Context::CLOSURE_INDEX, ip);
  } else if (closure_scope->is_eval_scope()) {
    // Contexts created by a call to eval have the same closure as the
    // context calling eval, not the anonymous closure containing the eval
    // code.  Fetch it from the context.
    __ LoadP(ip, ContextMemOperand(cp, Context::CLOSURE_INDEX));
  } else {
    DCHECK(closure_scope->is_function_scope());
    __ LoadP(ip, MemOperand(fp, JavaScriptFrameConstants::kFunctionOffset));
  }
  __ push(ip);
}


// ----------------------------------------------------------------------------
// Non-local control flow support.

void FullCodeGenerator::EnterFinallyBlock() {
  DCHECK(!result_register().is(r4));
  // Store result register while executing finally block.
  __ push(result_register());
  // Cook return address in link register to stack (smi encoded Code* delta)
  __ mflr(r4);
  __ mov(ip, Operand(masm_->CodeObject()));
  __ sub(r4, r4, ip);
  __ SmiTag(r4);

  // Store result register while executing finally block.
  __ push(r4);

  // Store pending message while executing finally block.
  ExternalReference pending_message_obj =
      ExternalReference::address_of_pending_message_obj(isolate());
  __ mov(ip, Operand(pending_message_obj));
  __ LoadP(r4, MemOperand(ip));
  __ push(r4);

  ClearPendingMessage();
}


void FullCodeGenerator::ExitFinallyBlock() {
  DCHECK(!result_register().is(r4));
  // Restore pending message from stack.
  __ pop(r4);
  ExternalReference pending_message_obj =
      ExternalReference::address_of_pending_message_obj(isolate());
  __ mov(ip, Operand(pending_message_obj));
  __ StoreP(r4, MemOperand(ip));

  // Restore result register from stack.
  __ pop(r4);

  // Uncook return address and return.
  __ pop(result_register());
  __ SmiUntag(r4);
  __ mov(ip, Operand(masm_->CodeObject()));
  __ add(ip, ip, r4);
  __ mtctr(ip);
  __ bctr();
}


void FullCodeGenerator::ClearPendingMessage() {
  DCHECK(!result_register().is(r4));
  ExternalReference pending_message_obj =
      ExternalReference::address_of_pending_message_obj(isolate());
  __ LoadRoot(r4, Heap::kTheHoleValueRootIndex);
  __ mov(ip, Operand(pending_message_obj));
  __ StoreP(r4, MemOperand(ip));
}


void FullCodeGenerator::EmitLoadStoreICSlot(FeedbackVectorSlot slot) {
  DCHECK(!slot.IsInvalid());
  __ mov(VectorStoreICTrampolineDescriptor::SlotRegister(),
         Operand(SmiFromSlot(slot)));
}


#undef __


void BackEdgeTable::PatchAt(Code* unoptimized_code, Address pc,
                            BackEdgeState target_state,
                            Code* replacement_code) {
  Address mov_address = Assembler::target_address_from_return_address(pc);
  Address cmp_address = mov_address - 2 * Assembler::kInstrSize;
  Isolate* isolate = unoptimized_code->GetIsolate();
  CodePatcher patcher(isolate, cmp_address, 1);

  switch (target_state) {
    case INTERRUPT: {
      //  <decrement profiling counter>
      //         cmpi    r6, 0
      //         bge     <ok>            ;; not changed
      //         mov     r12, <interrupt stub address>
      //         mtlr    r12
      //         blrl
      //  <reset profiling counter>
      //  ok-label
      patcher.masm()->cmpi(r6, Operand::Zero());
      break;
    }
    case ON_STACK_REPLACEMENT:
    case OSR_AFTER_STACK_CHECK:
      //  <decrement profiling counter>
      //         crset
      //         bge     <ok>            ;; not changed
      //         mov     r12, <on-stack replacement address>
      //         mtlr    r12
      //         blrl
      //  <reset profiling counter>
      //  ok-label ----- pc_after points here

      // Set the LT bit such that bge is a NOP
      patcher.masm()->crset(Assembler::encode_crbit(cr7, CR_LT));
      break;
  }

  // Replace the stack check address in the mov sequence with the
  // entry address of the replacement code.
  Assembler::set_target_address_at(isolate, mov_address, unoptimized_code,
                                   replacement_code->entry());

  unoptimized_code->GetHeap()->incremental_marking()->RecordCodeTargetPatch(
      unoptimized_code, mov_address, replacement_code);
}


BackEdgeTable::BackEdgeState BackEdgeTable::GetBackEdgeState(
    Isolate* isolate, Code* unoptimized_code, Address pc) {
  Address mov_address = Assembler::target_address_from_return_address(pc);
  Address cmp_address = mov_address - 2 * Assembler::kInstrSize;
  Address interrupt_address =
      Assembler::target_address_at(mov_address, unoptimized_code);

  if (Assembler::IsCmpImmediate(Assembler::instr_at(cmp_address))) {
    DCHECK(interrupt_address == isolate->builtins()->InterruptCheck()->entry());
    return INTERRUPT;
  }

  DCHECK(Assembler::IsCrSet(Assembler::instr_at(cmp_address)));

  if (interrupt_address == isolate->builtins()->OnStackReplacement()->entry()) {
    return ON_STACK_REPLACEMENT;
  }

  DCHECK(interrupt_address ==
         isolate->builtins()->OsrAfterStackCheck()->entry());
  return OSR_AFTER_STACK_CHECK;
}
}  // namespace internal
}  // namespace v8
#endif  // V8_TARGET_ARCH_PPC
