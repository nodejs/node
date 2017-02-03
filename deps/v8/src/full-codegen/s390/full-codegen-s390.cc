// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if V8_TARGET_ARCH_S390

#include "src/full-codegen/full-codegen.h"
#include "src/ast/compile-time-value.h"
#include "src/ast/scopes.h"
#include "src/code-factory.h"
#include "src/code-stubs.h"
#include "src/codegen.h"
#include "src/compilation-info.h"
#include "src/compiler.h"
#include "src/debug/debug.h"
#include "src/ic/ic.h"

#include "src/s390/code-stubs-s390.h"
#include "src/s390/macro-assembler-s390.h"

namespace v8 {
namespace internal {

#define __ ACCESS_MASM(masm())

// A patch site is a location in the code which it is possible to patch. This
// class has a number of methods to emit the code which is patchable and the
// method EmitPatchInfo to record a marker back to the patchable code. This
// marker is a cmpi rx, #yyy instruction, and x * 0x0000ffff + yyy (raw 16 bit
// immediate value is used) is the delta from the pc to the first instruction of
// the patchable code.
// See PatchInlinedSmiCode in ic-s390.cc for the code that patches it
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
    __ bind(&patch_site_);
    __ CmpP(reg, reg);
// Emit the Nop to make bigger place for patching on 31-bit
// as the TestIfSmi sequence uses 4-byte TMLL
#ifndef V8_TARGET_ARCH_S390X
    __ nop();
#endif
    __ beq(target);  // Always taken before patched.
  }

  // When initially emitting this ensure that a jump is never generated to skip
  // the inlined smi code.
  void EmitJumpIfSmi(Register reg, Label* target) {
    DCHECK(!patch_site_.is_bound() && !info_emitted_);
    __ bind(&patch_site_);
    __ CmpP(reg, reg);
// Emit the Nop to make bigger place for patching on 31-bit
// as the TestIfSmi sequence uses 4-byte TMLL
#ifndef V8_TARGET_ARCH_S390X
    __ nop();
#endif
    __ bne(target);  // Never taken before patched.
  }

  void EmitPatchInfo() {
    if (patch_site_.is_bound()) {
      int delta_to_patch_site = masm_->SizeOfCodeGeneratedSince(&patch_site_);
      DCHECK(is_int16(delta_to_patch_site));
      __ chi(r0, Operand(delta_to_patch_site));
#ifdef DEBUG
      info_emitted_ = true;
#endif
    } else {
      __ nop();
      __ nop();
    }
  }

 private:
  MacroAssembler* masm() { return masm_; }
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
//   o r3: the JS function object being called (i.e., ourselves)
//   o r5: the new target value
//   o cp: our context
//   o fp: our caller's frame pointer
//   o sp: stack pointer
//   o lr: return address
//   o ip: our own function entry (required by the prologue)
//
// The function builds a JS frame.  Please see JavaScriptFrameConstants in
// frames-s390.h for its layout.
void FullCodeGenerator::Generate() {
  CompilationInfo* info = info_;
  profiling_counter_ = isolate()->factory()->NewCell(
      Handle<Smi>(Smi::FromInt(FLAG_interrupt_budget), isolate()));
  SetFunctionPosition(literal());
  Comment cmnt(masm_, "[ function compiled by full code generator");

  ProfileEntryHookStub::MaybeCallEntryHook(masm_);

  if (FLAG_debug_code && info->ExpectsJSReceiverAsReceiver()) {
    int receiver_offset = info->scope()->num_parameters() * kPointerSize;
    __ LoadP(r4, MemOperand(sp, receiver_offset), r0);
    __ AssertNotSmi(r4);
    __ CompareObjectType(r4, r4, no_reg, FIRST_JS_RECEIVER_TYPE);
    __ Assert(ge, kSloppyFunctionExpectsJSReceiverReceiver);
  }

  // Open a frame scope to indicate that there is a frame on the stack.  The
  // MANUAL indicates that the scope shouldn't actually generate code to set up
  // the frame (that is done below).
  FrameScope frame_scope(masm_, StackFrame::MANUAL);
  int prologue_offset = masm_->pc_offset();

  info->set_prologue_offset(prologue_offset);
  __ Prologue(info->GeneratePreagedPrologue(), ip, prologue_offset);

  // Increment invocation count for the function.
  {
    Comment cmnt(masm_, "[ Increment invocation count");
    __ LoadP(r6, FieldMemOperand(r3, JSFunction::kLiteralsOffset));
    __ LoadP(r6, FieldMemOperand(r6, LiteralsArray::kFeedbackVectorOffset));
    __ LoadP(r1, FieldMemOperand(r6, TypeFeedbackVector::kInvocationCountIndex *
                                             kPointerSize +
                                         TypeFeedbackVector::kHeaderSize));
    __ AddSmiLiteral(r1, r1, Smi::FromInt(1), r0);
    __ StoreP(r1,
              FieldMemOperand(
                  r6, TypeFeedbackVector::kInvocationCountIndex * kPointerSize +
                          TypeFeedbackVector::kHeaderSize));
  }

  {
    Comment cmnt(masm_, "[ Allocate locals");
    int locals_count = info->scope()->num_stack_slots();
    // Generators allocate locals, if any, in context slots.
    DCHECK(!IsGeneratorFunction(info->literal()->kind()) || locals_count == 0);
    OperandStackDepthIncrement(locals_count);
    if (locals_count > 0) {
      if (locals_count >= 128) {
        Label ok;
        __ AddP(ip, sp, Operand(-(locals_count * kPointerSize)));
        __ LoadRoot(r5, Heap::kRealStackLimitRootIndex);
        __ CmpLogicalP(ip, r5);
        __ bge(&ok, Label::kNear);
        __ CallRuntime(Runtime::kThrowStackOverflow);
        __ bind(&ok);
      }
      __ LoadRoot(ip, Heap::kUndefinedValueRootIndex);
      int kMaxPushes = FLAG_optimize_for_size ? 4 : 32;
      if (locals_count >= kMaxPushes) {
        int loop_iterations = locals_count / kMaxPushes;
        __ mov(r4, Operand(loop_iterations));
        Label loop_header;
        __ bind(&loop_header);
        // Do pushes.
        // TODO(joransiu): Use MVC for better performance
        __ lay(sp, MemOperand(sp, -kMaxPushes * kPointerSize));
        for (int i = 0; i < kMaxPushes; i++) {
          __ StoreP(ip, MemOperand(sp, i * kPointerSize));
        }
        // Continue loop if not done.
        __ BranchOnCount(r4, &loop_header);
      }
      int remaining = locals_count % kMaxPushes;
      // Emit the remaining pushes.
      // TODO(joransiu): Use MVC for better performance
      if (remaining > 0) {
        __ lay(sp, MemOperand(sp, -remaining * kPointerSize));
        for (int i = 0; i < remaining; i++) {
          __ StoreP(ip, MemOperand(sp, i * kPointerSize));
        }
      }
    }
  }

  bool function_in_register_r3 = true;

  // Possibly allocate a local context.
  if (info->scope()->NeedsContext()) {
    // Argument to NewContext is the function, which is still in r3.
    Comment cmnt(masm_, "[ Allocate context");
    bool need_write_barrier = true;
    int slots = info->scope()->num_heap_slots() - Context::MIN_CONTEXT_SLOTS;
    if (info->scope()->is_script_scope()) {
      __ push(r3);
      __ Push(info->scope()->scope_info());
      __ CallRuntime(Runtime::kNewScriptContext);
      PrepareForBailoutForId(BailoutId::ScriptContext(),
                             BailoutState::TOS_REGISTER);
      // The new target value is not used, clobbering is safe.
      DCHECK_NULL(info->scope()->new_target_var());
    } else {
      if (info->scope()->new_target_var() != nullptr) {
        __ push(r5);  // Preserve new target.
      }
      if (slots <= FastNewFunctionContextStub::kMaximumSlots) {
        FastNewFunctionContextStub stub(isolate());
        __ mov(FastNewFunctionContextDescriptor::SlotsRegister(),
               Operand(slots));
        __ CallStub(&stub);
        // Result of FastNewFunctionContextStub is always in new space.
        need_write_barrier = false;
      } else {
        __ push(r3);
        __ CallRuntime(Runtime::kNewFunctionContext);
      }
      if (info->scope()->new_target_var() != nullptr) {
        __ pop(r5);  // Preserve new target.
      }
    }
    function_in_register_r3 = false;
    // Context is returned in r2.  It replaces the context passed to us.
    // It's saved in the stack and kept live in cp.
    __ LoadRR(cp, r2);
    __ StoreP(r2, MemOperand(fp, StandardFrameConstants::kContextOffset));
    // Copy any necessary parameters into the context.
    int num_parameters = info->scope()->num_parameters();
    int first_parameter = info->scope()->has_this_declaration() ? -1 : 0;
    for (int i = first_parameter; i < num_parameters; i++) {
      Variable* var =
          (i == -1) ? info->scope()->receiver() : info->scope()->parameter(i);
      if (var->IsContextSlot()) {
        int parameter_offset = StandardFrameConstants::kCallerSPOffset +
                               (num_parameters - 1 - i) * kPointerSize;
        // Load parameter from stack.
        __ LoadP(r2, MemOperand(fp, parameter_offset), r0);
        // Store it in the context.
        MemOperand target = ContextMemOperand(cp, var->index());
        __ StoreP(r2, target);

        // Update the write barrier.
        if (need_write_barrier) {
          __ RecordWriteContextSlot(cp, target.offset(), r2, r4,
                                    kLRHasBeenSaved, kDontSaveFPRegs);
        } else if (FLAG_debug_code) {
          Label done;
          __ JumpIfInNewSpace(cp, r2, &done);
          __ Abort(kExpectedNewSpaceObject);
          __ bind(&done);
        }
      }
    }
  }

  // Register holding this function and new target are both trashed in case we
  // bailout here. But since that can happen only when new target is not used
  // and we allocate a context, the value of |function_in_register| is correct.
  PrepareForBailoutForId(BailoutId::FunctionContext(),
                         BailoutState::NO_REGISTERS);

  // Possibly set up a local binding to the this function which is used in
  // derived constructors with super calls.
  Variable* this_function_var = info->scope()->this_function_var();
  if (this_function_var != nullptr) {
    Comment cmnt(masm_, "[ This function");
    if (!function_in_register_r3) {
      __ LoadP(r3, MemOperand(fp, JavaScriptFrameConstants::kFunctionOffset));
      // The write barrier clobbers register again, keep it marked as such.
    }
    SetVar(this_function_var, r3, r2, r4);
  }

  // Possibly set up a local binding to the new target value.
  Variable* new_target_var = info->scope()->new_target_var();
  if (new_target_var != nullptr) {
    Comment cmnt(masm_, "[ new.target");
    SetVar(new_target_var, r5, r2, r4);
  }

  // Possibly allocate RestParameters
  Variable* rest_param = info->scope()->rest_parameter();
  if (rest_param != nullptr) {
    Comment cmnt(masm_, "[ Allocate rest parameter array");

    if (!function_in_register_r3) {
      __ LoadP(r3, MemOperand(fp, JavaScriptFrameConstants::kFunctionOffset));
    }
    FastNewRestParameterStub stub(isolate());
    __ CallStub(&stub);

    function_in_register_r3 = false;
    SetVar(rest_param, r2, r3, r4);
  }

  Variable* arguments = info->scope()->arguments();
  if (arguments != NULL) {
    // Function uses arguments object.
    Comment cmnt(masm_, "[ Allocate arguments object");
    if (!function_in_register_r3) {
      // Load this again, if it's used by the local context below.
      __ LoadP(r3, MemOperand(fp, JavaScriptFrameConstants::kFunctionOffset));
    }
    if (is_strict(language_mode()) || !has_simple_parameters()) {
      FastNewStrictArgumentsStub stub(isolate());
      __ CallStub(&stub);
    } else if (literal()->has_duplicate_parameters()) {
      __ Push(r3);
      __ CallRuntime(Runtime::kNewSloppyArguments_Generic);
    } else {
      FastNewSloppyArgumentsStub stub(isolate());
      __ CallStub(&stub);
    }

    SetVar(arguments, r2, r3, r4);
  }

  if (FLAG_trace) {
    __ CallRuntime(Runtime::kTraceEnter);
  }

  // Visit the declarations and body.
  PrepareForBailoutForId(BailoutId::FunctionEntry(),
                         BailoutState::NO_REGISTERS);
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
    PrepareForBailoutForId(BailoutId::Declarations(),
                           BailoutState::NO_REGISTERS);
    Label ok;
    __ LoadRoot(ip, Heap::kStackLimitRootIndex);
    __ CmpLogicalP(sp, ip);
    __ bge(&ok, Label::kNear);
    __ Call(isolate()->builtins()->StackCheck(), RelocInfo::CODE_TARGET);
    __ bind(&ok);
  }

  {
    Comment cmnt(masm_, "[ Body");
    DCHECK(loop_depth() == 0);
    VisitStatements(literal()->body());
    DCHECK(loop_depth() == 0);
  }

  // Always emit a 'return undefined' in case control fell off the end of
  // the body.
  {
    Comment cmnt(masm_, "[ return <undefined>;");
    __ LoadRoot(r2, Heap::kUndefinedValueRootIndex);
  }
  EmitReturnSequence();
}

void FullCodeGenerator::ClearAccumulator() {
  __ LoadSmiLiteral(r2, Smi::FromInt(0));
}

void FullCodeGenerator::EmitProfilingCounterDecrement(int delta) {
  __ mov(r4, Operand(profiling_counter_));
  intptr_t smi_delta = reinterpret_cast<intptr_t>(Smi::FromInt(delta));
  if (CpuFeatures::IsSupported(GENERAL_INSTR_EXT) && is_int8(-smi_delta)) {
    __ AddP(FieldMemOperand(r4, Cell::kValueOffset), Operand(-smi_delta));
    __ LoadP(r5, FieldMemOperand(r4, Cell::kValueOffset));
  } else {
    __ LoadP(r5, FieldMemOperand(r4, Cell::kValueOffset));
    __ SubSmiLiteral(r5, r5, Smi::FromInt(delta), r0);
    __ StoreP(r5, FieldMemOperand(r4, Cell::kValueOffset));
  }
}

void FullCodeGenerator::EmitProfilingCounterReset() {
  int reset_value = FLAG_interrupt_budget;
  __ mov(r4, Operand(profiling_counter_));
  __ LoadSmiLiteral(r5, Smi::FromInt(reset_value));
  __ StoreP(r5, FieldMemOperand(r4, Cell::kValueOffset));
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
    // BackEdgeTable::PatchAt manipulates this sequence.
    __ bge(&ok, Label::kNear);
    __ Call(isolate()->builtins()->InterruptCheck(), RelocInfo::CODE_TARGET);

    // Record a mapping of this PC offset to the OSR id.  This is used to find
    // the AST id from the unoptimized code in order to use it as a key into
    // the deoptimization input data found in the optimized code.
    RecordBackEdge(stmt->OsrEntryId());
  }
  EmitProfilingCounterReset();

  __ bind(&ok);
  PrepareForBailoutForId(stmt->EntryId(), BailoutState::NO_REGISTERS);
  // Record a mapping of the OSR id to this PC.  This is used if the OSR
  // entry becomes the target of a bailout.  We don't expect it to be, but
  // we want it to work if it is.
  PrepareForBailoutForId(stmt->OsrEntryId(), BailoutState::NO_REGISTERS);
}

void FullCodeGenerator::EmitProfilingCounterHandlingForReturnSequence(
    bool is_tail_call) {
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
  __ CmpP(r5, Operand::Zero());
  __ bge(&ok);
  // Don't need to save result register if we are going to do a tail call.
  if (!is_tail_call) {
    __ push(r2);
  }
  __ Call(isolate()->builtins()->InterruptCheck(), RelocInfo::CODE_TARGET);
  if (!is_tail_call) {
    __ pop(r2);
  }
  EmitProfilingCounterReset();
  __ bind(&ok);
}

void FullCodeGenerator::EmitReturnSequence() {
  Comment cmnt(masm_, "[ Return sequence");
  if (return_label_.is_bound()) {
    __ b(&return_label_);
  } else {
    __ bind(&return_label_);
    if (FLAG_trace) {
      // Push the return value on the stack as the parameter.
      // Runtime::TraceExit returns its parameter in r2
      __ push(r2);
      __ CallRuntime(Runtime::kTraceExit);
    }
    EmitProfilingCounterHandlingForReturnSequence(false);

    // Make sure that the constant pool is not emitted inside of the return
    // sequence.
    {
      int32_t arg_count = info_->scope()->num_parameters() + 1;
      int32_t sp_delta = arg_count * kPointerSize;
      SetReturnPosition(literal());
      __ LeaveFrame(StackFrame::JAVA_SCRIPT, sp_delta);

      __ Ret();
    }
  }
}

void FullCodeGenerator::RestoreContext() {
  __ LoadP(cp, MemOperand(fp, StandardFrameConstants::kContextOffset));
}

void FullCodeGenerator::StackValueContext::Plug(Variable* var) const {
  DCHECK(var->IsStackAllocated() || var->IsContextSlot());
  codegen()->GetVar(result_register(), var);
  codegen()->PushOperand(result_register());
}

void FullCodeGenerator::EffectContext::Plug(Heap::RootListIndex index) const {}

void FullCodeGenerator::AccumulatorValueContext::Plug(
    Heap::RootListIndex index) const {
  __ LoadRoot(result_register(), index);
}

void FullCodeGenerator::StackValueContext::Plug(
    Heap::RootListIndex index) const {
  __ LoadRoot(result_register(), index);
  codegen()->PushOperand(result_register());
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
  codegen()->PushOperand(result_register());
}

void FullCodeGenerator::TestContext::Plug(Handle<Object> lit) const {
  codegen()->PrepareForBailoutBeforeSplit(condition(), true, true_label_,
                                          false_label_);
  DCHECK(lit->IsNull(isolate()) || lit->IsUndefined(isolate()) ||
         !lit->IsUndetectable());
  if (lit->IsUndefined(isolate()) || lit->IsNull(isolate()) ||
      lit->IsFalse(isolate())) {
    if (false_label_ != fall_through_) __ b(false_label_);
  } else if (lit->IsTrue(isolate()) || lit->IsJSObject()) {
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

void FullCodeGenerator::StackValueContext::DropAndPlug(int count,
                                                       Register reg) const {
  DCHECK(count > 0);
  if (count > 1) codegen()->DropOperands(count - 1);
  __ StoreP(reg, MemOperand(sp, 0));
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
  __ b(&done, Label::kNear);
  __ bind(materialize_false);
  __ LoadRoot(result_register(), Heap::kFalseValueRootIndex);
  __ bind(&done);
}

void FullCodeGenerator::StackValueContext::Plug(
    Label* materialize_true, Label* materialize_false) const {
  Label done;
  __ bind(materialize_true);
  __ LoadRoot(ip, Heap::kTrueValueRootIndex);
  __ b(&done, Label::kNear);
  __ bind(materialize_false);
  __ LoadRoot(ip, Heap::kFalseValueRootIndex);
  __ bind(&done);
  codegen()->PushOperand(ip);
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
  codegen()->PushOperand(ip);
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
  Handle<Code> ic = ToBooleanICStub::GetUninitialized(isolate());
  CallIC(ic, condition->test_id());
  __ CompareRoot(result_register(), Heap::kTrueValueRootIndex);
  Split(eq, if_true, if_false, fall_through);
}

void FullCodeGenerator::Split(Condition cond, Label* if_true, Label* if_false,
                              Label* fall_through) {
  if (if_false == fall_through) {
    __ b(cond, if_true);
  } else if (if_true == fall_through) {
    __ b(NegateCondition(cond), if_false);
  } else {
    __ b(cond, if_true);
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
  __ StoreP(src, location);

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
  PrepareForBailout(expr, BailoutState::TOS_REGISTER);
  if (should_normalize) {
    __ CompareRoot(r2, Heap::kTrueValueRootIndex);
    Split(eq, if_true, if_false, NULL);
    __ bind(&skip);
  }
}

void FullCodeGenerator::EmitDebugCheckDeclarationContext(Variable* variable) {
  // The variable in the declaration always resides in the current function
  // context.
  DCHECK_EQ(0, scope()->ContextChainLength(variable->scope()));
  if (FLAG_debug_code) {
    // Check that we're not inside a with or catch context.
    __ LoadP(r3, FieldMemOperand(cp, HeapObject::kMapOffset));
    __ CompareRoot(r3, Heap::kWithContextMapRootIndex);
    __ Check(ne, kDeclarationInWithContext);
    __ CompareRoot(r3, Heap::kCatchContextMapRootIndex);
    __ Check(ne, kDeclarationInCatchContext);
  }
}

void FullCodeGenerator::VisitVariableDeclaration(
    VariableDeclaration* declaration) {
  VariableProxy* proxy = declaration->proxy();
  Variable* variable = proxy->var();
  switch (variable->location()) {
    case VariableLocation::UNALLOCATED: {
      DCHECK(!variable->binding_needs_init());
      FeedbackVectorSlot slot = proxy->VariableFeedbackSlot();
      DCHECK(!slot.IsInvalid());
      globals_->Add(handle(Smi::FromInt(slot.ToInt()), isolate()), zone());
      globals_->Add(isolate()->factory()->undefined_value(), zone());
      break;
    }
    case VariableLocation::PARAMETER:
    case VariableLocation::LOCAL:
      if (variable->binding_needs_init()) {
        Comment cmnt(masm_, "[ VariableDeclaration");
        __ LoadRoot(ip, Heap::kTheHoleValueRootIndex);
        __ StoreP(ip, StackOperand(variable));
      }
      break;

    case VariableLocation::CONTEXT:
      if (variable->binding_needs_init()) {
        Comment cmnt(masm_, "[ VariableDeclaration");
        EmitDebugCheckDeclarationContext(variable);
        __ LoadRoot(ip, Heap::kTheHoleValueRootIndex);
        __ StoreP(ip, ContextMemOperand(cp, variable->index()));
        // No write barrier since the_hole_value is in old space.
        PrepareForBailoutForId(proxy->id(), BailoutState::NO_REGISTERS);
      }
      break;

    case VariableLocation::LOOKUP: {
      Comment cmnt(masm_, "[ VariableDeclaration");
      DCHECK_EQ(VAR, variable->mode());
      DCHECK(!variable->binding_needs_init());
      __ mov(r4, Operand(variable->name()));
      __ Push(r4);
      __ CallRuntime(Runtime::kDeclareEvalVar);
      PrepareForBailoutForId(proxy->id(), BailoutState::NO_REGISTERS);
      break;
    }

    case VariableLocation::MODULE:
      UNREACHABLE();
  }
}

void FullCodeGenerator::VisitFunctionDeclaration(
    FunctionDeclaration* declaration) {
  VariableProxy* proxy = declaration->proxy();
  Variable* variable = proxy->var();
  switch (variable->location()) {
    case VariableLocation::UNALLOCATED: {
      FeedbackVectorSlot slot = proxy->VariableFeedbackSlot();
      DCHECK(!slot.IsInvalid());
      globals_->Add(handle(Smi::FromInt(slot.ToInt()), isolate()), zone());
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
      __ StoreP(result_register(), ContextMemOperand(cp, variable->index()));
      int offset = Context::SlotOffset(variable->index());
      // We know that we have written a function, which is not a smi.
      __ RecordWriteContextSlot(cp, offset, result_register(), r4,
                                kLRHasBeenSaved, kDontSaveFPRegs,
                                EMIT_REMEMBERED_SET, OMIT_SMI_CHECK);
      PrepareForBailoutForId(proxy->id(), BailoutState::NO_REGISTERS);
      break;
    }

    case VariableLocation::LOOKUP: {
      Comment cmnt(masm_, "[ FunctionDeclaration");
      __ mov(r4, Operand(variable->name()));
      PushOperand(r4);
      // Push initial value for function declaration.
      VisitForStackValue(declaration->fun());
      CallRuntimeWithOperands(Runtime::kDeclareEvalFunction);
      PrepareForBailoutForId(proxy->id(), BailoutState::NO_REGISTERS);
      break;
    }

    case VariableLocation::MODULE:
      UNREACHABLE();
  }
}

void FullCodeGenerator::DeclareGlobals(Handle<FixedArray> pairs) {
  // Call the runtime to declare the globals.
  __ mov(r3, Operand(pairs));
  __ LoadSmiLiteral(r2, Smi::FromInt(DeclareGlobalsFlags()));
  __ EmitLoadTypeFeedbackVector(r4);
  __ Push(r3, r2, r4);
  __ CallRuntime(Runtime::kDeclareGlobals);
  // Return value is ignored.
}

void FullCodeGenerator::VisitSwitchStatement(SwitchStatement* stmt) {
  Comment cmnt(masm_, "[ SwitchStatement");
  Breakable nested_statement(this, stmt);
  SetStatementPosition(stmt);

  // Keep the switch value on the stack until a case matches.
  VisitForStackValue(stmt->tag());
  PrepareForBailoutForId(stmt->EntryId(), BailoutState::NO_REGISTERS);

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
    __ LoadP(r3, MemOperand(sp, 0));  // Switch value.
    bool inline_smi_code = ShouldInlineSmiCase(Token::EQ_STRICT);
    JumpPatchSite patch_site(masm_);
    if (inline_smi_code) {
      Label slow_case;
      __ LoadRR(r4, r2);
      __ OrP(r4, r3);
      patch_site.EmitJumpIfNotSmi(r4, &slow_case);

      __ CmpP(r3, r2);
      __ bne(&next_test);
      __ Drop(1);  // Switch value is no longer needed.
      __ b(clause->body_target());
      __ bind(&slow_case);
    }

    // Record position before stub call for type feedback.
    SetExpressionPosition(clause);
    Handle<Code> ic =
        CodeFactory::CompareIC(isolate(), Token::EQ_STRICT).code();
    CallIC(ic, clause->CompareId());
    patch_site.EmitPatchInfo();

    Label skip;
    __ b(&skip);
    PrepareForBailout(clause, BailoutState::TOS_REGISTER);
    __ CompareRoot(r2, Heap::kTrueValueRootIndex);
    __ bne(&next_test);
    __ Drop(1);
    __ b(clause->body_target());
    __ bind(&skip);

    __ CmpP(r2, Operand::Zero());
    __ bne(&next_test);
    __ Drop(1);  // Switch value is no longer needed.
    __ b(clause->body_target());
  }

  // Discard the test value and jump to the default if present, otherwise to
  // the end of the statement.
  __ bind(&next_test);
  DropOperands(1);  // Switch value is no longer needed.
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
    PrepareForBailoutForId(clause->EntryId(), BailoutState::NO_REGISTERS);
    VisitStatements(clause->statements());
  }

  __ bind(nested_statement.break_label());
  PrepareForBailoutForId(stmt->ExitId(), BailoutState::NO_REGISTERS);
}

void FullCodeGenerator::VisitForInStatement(ForInStatement* stmt) {
  Comment cmnt(masm_, "[ ForInStatement");
  SetStatementPosition(stmt, SKIP_BREAK);

  FeedbackVectorSlot slot = stmt->ForInFeedbackSlot();

  // Get the object to enumerate over.
  SetExpressionAsStatementPosition(stmt->enumerable());
  VisitForAccumulatorValue(stmt->enumerable());
  OperandStackDepthIncrement(5);

  Label loop, exit;
  Iteration loop_statement(this, stmt);
  increment_loop_depth();

  // If the object is null or undefined, skip over the loop, otherwise convert
  // it to a JS receiver.  See ECMA-262 version 5, section 12.6.4.
  Label convert, done_convert;
  __ JumpIfSmi(r2, &convert);
  __ CompareObjectType(r2, r3, r3, FIRST_JS_RECEIVER_TYPE);
  __ bge(&done_convert);
  __ CompareRoot(r2, Heap::kNullValueRootIndex);
  __ beq(&exit);
  __ CompareRoot(r2, Heap::kUndefinedValueRootIndex);
  __ beq(&exit);
  __ bind(&convert);
  ToObjectStub stub(isolate());
  __ CallStub(&stub);
  RestoreContext();
  __ bind(&done_convert);
  PrepareForBailoutForId(stmt->ToObjectId(), BailoutState::TOS_REGISTER);
  __ push(r2);

  // Check cache validity in generated code. If we cannot guarantee cache
  // validity, call the runtime system to check cache validity or get the
  // property names in a fixed array. Note: Proxies never have an enum cache,
  // so will always take the slow path.
  Label call_runtime;
  __ CheckEnumCache(&call_runtime);

  // The enum cache is valid.  Load the map of the object being
  // iterated over and use the cache for the iteration.
  Label use_cache;
  __ LoadP(r2, FieldMemOperand(r2, HeapObject::kMapOffset));
  __ b(&use_cache);

  // Get the set of properties to enumerate.
  __ bind(&call_runtime);
  __ push(r2);  // Duplicate the enumerable object on the stack.
  __ CallRuntime(Runtime::kForInEnumerate);
  PrepareForBailoutForId(stmt->EnumId(), BailoutState::TOS_REGISTER);

  // If we got a map from the runtime call, we can do a fast
  // modification check. Otherwise, we got a fixed array, and we have
  // to do a slow check.
  Label fixed_array;
  __ LoadP(r4, FieldMemOperand(r2, HeapObject::kMapOffset));
  __ CompareRoot(r4, Heap::kMetaMapRootIndex);
  __ bne(&fixed_array);

  // We got a map in register r2. Get the enumeration cache from it.
  Label no_descriptors;
  __ bind(&use_cache);

  __ EnumLength(r3, r2);
  __ CmpSmiLiteral(r3, Smi::FromInt(0), r0);
  __ beq(&no_descriptors, Label::kNear);

  __ LoadInstanceDescriptors(r2, r4);
  __ LoadP(r4, FieldMemOperand(r4, DescriptorArray::kEnumCacheOffset));
  __ LoadP(r4,
           FieldMemOperand(r4, DescriptorArray::kEnumCacheBridgeCacheOffset));

  // Set up the four remaining stack slots.
  __ push(r2);  // Map.
  __ LoadSmiLiteral(r2, Smi::FromInt(0));
  // Push enumeration cache, enumeration cache length (as smi) and zero.
  __ Push(r4, r3, r2);
  __ b(&loop);

  __ bind(&no_descriptors);
  __ Drop(1);
  __ b(&exit);

  // We got a fixed array in register r2. Iterate through that.
  __ bind(&fixed_array);

  __ LoadSmiLiteral(r3, Smi::FromInt(1));  // Smi(1) indicates slow check
  __ Push(r3, r2);                         // Smi and array
  __ LoadP(r3, FieldMemOperand(r2, FixedArray::kLengthOffset));
  __ Push(r3);  // Fixed array length (as smi).
  PrepareForBailoutForId(stmt->PrepareId(), BailoutState::NO_REGISTERS);
  __ LoadSmiLiteral(r2, Smi::FromInt(0));
  __ Push(r2);  // Initial index.

  // Generate code for doing the condition check.
  __ bind(&loop);
  SetExpressionAsStatementPosition(stmt->each());

  // Load the current count to r2, load the length to r3.
  __ LoadP(r2, MemOperand(sp, 0 * kPointerSize));
  __ LoadP(r3, MemOperand(sp, 1 * kPointerSize));
  __ CmpLogicalP(r2, r3);  // Compare to the array length.
  __ bge(loop_statement.break_label());

  // Get the current entry of the array into register r5.
  __ LoadP(r4, MemOperand(sp, 2 * kPointerSize));
  __ AddP(r4, Operand(FixedArray::kHeaderSize - kHeapObjectTag));
  __ SmiToPtrArrayOffset(r5, r2);
  __ LoadP(r5, MemOperand(r5, r4));

  // Get the expected map from the stack or a smi in the
  // permanent slow case into register r4.
  __ LoadP(r4, MemOperand(sp, 3 * kPointerSize));

  // Check if the expected map still matches that of the enumerable.
  // If not, we may have to filter the key.
  Label update_each;
  __ LoadP(r3, MemOperand(sp, 4 * kPointerSize));
  __ LoadP(r6, FieldMemOperand(r3, HeapObject::kMapOffset));
  __ CmpP(r6, r4);
  __ beq(&update_each);

  // We need to filter the key, record slow-path here.
  int const vector_index = SmiFromSlot(slot)->value();
  __ EmitLoadTypeFeedbackVector(r2);
  __ mov(r4, Operand(TypeFeedbackVector::MegamorphicSentinel(isolate())));
  __ StoreP(
      r4, FieldMemOperand(r2, FixedArray::OffsetOfElementAt(vector_index)), r0);

  // Convert the entry to a string or (smi) 0 if it isn't a property
  // any more. If the property has been removed while iterating, we
  // just skip it.
  __ Push(r3, r5);  // Enumerable and current entry.
  __ CallRuntime(Runtime::kForInFilter);
  PrepareForBailoutForId(stmt->FilterId(), BailoutState::TOS_REGISTER);
  __ LoadRR(r5, r2);
  __ LoadRoot(r0, Heap::kUndefinedValueRootIndex);
  __ CmpP(r2, r0);
  __ beq(loop_statement.continue_label());

  // Update the 'each' property or variable from the possibly filtered
  // entry in register r5.
  __ bind(&update_each);
  __ LoadRR(result_register(), r5);
  // Perform the assignment as if via '='.
  {
    EffectContext context(this);
    EmitAssignment(stmt->each(), stmt->EachFeedbackSlot());
    PrepareForBailoutForId(stmt->AssignmentId(), BailoutState::NO_REGISTERS);
  }

  // Both Crankshaft and Turbofan expect BodyId to be right before stmt->body().
  PrepareForBailoutForId(stmt->BodyId(), BailoutState::NO_REGISTERS);
  // Generate code for the body of the loop.
  Visit(stmt->body());

  // Generate code for the going to the next element by incrementing
  // the index (smi) stored on top of the stack.
  __ bind(loop_statement.continue_label());
  PrepareForBailoutForId(stmt->IncrementId(), BailoutState::NO_REGISTERS);
  __ pop(r2);
  __ AddSmiLiteral(r2, r2, Smi::FromInt(1), r0);
  __ push(r2);

  EmitBackEdgeBookkeeping(stmt, &loop);
  __ b(&loop);

  // Remove the pointers stored on the stack.
  __ bind(loop_statement.break_label());
  DropOperands(5);

  // Exit and decrement the loop depth.
  PrepareForBailoutForId(stmt->ExitId(), BailoutState::NO_REGISTERS);
  __ bind(&exit);
  decrement_loop_depth();
}

void FullCodeGenerator::EmitSetHomeObject(Expression* initializer, int offset,
                                          FeedbackVectorSlot slot) {
  DCHECK(NeedsHomeObject(initializer));
  __ LoadP(StoreDescriptor::ReceiverRegister(), MemOperand(sp));
  __ LoadP(StoreDescriptor::ValueRegister(),
           MemOperand(sp, offset * kPointerSize));
  CallStoreIC(slot, isolate()->factory()->home_object_symbol());
}

void FullCodeGenerator::EmitSetHomeObjectAccumulator(Expression* initializer,
                                                     int offset,
                                                     FeedbackVectorSlot slot) {
  DCHECK(NeedsHomeObject(initializer));
  __ Move(StoreDescriptor::ReceiverRegister(), r2);
  __ LoadP(StoreDescriptor::ValueRegister(),
           MemOperand(sp, offset * kPointerSize));
  CallStoreIC(slot, isolate()->factory()->home_object_symbol());
}

void FullCodeGenerator::EmitLoadGlobalCheckExtensions(VariableProxy* proxy,
                                                      TypeofMode typeof_mode,
                                                      Label* slow) {
  Register current = cp;
  Register next = r3;
  Register temp = r4;

  int to_check = scope()->ContextChainLengthUntilOutermostSloppyEval();
  for (Scope* s = scope(); to_check > 0; s = s->outer_scope()) {
    if (!s->NeedsContext()) continue;
    if (s->calls_sloppy_eval()) {
      // Check that extension is "the hole".
      __ LoadP(temp, ContextMemOperand(current, Context::EXTENSION_INDEX));
      __ JumpIfNotRoot(temp, Heap::kTheHoleValueRootIndex, slow);
    }
    // Load next context in chain.
    __ LoadP(next, ContextMemOperand(current, Context::PREVIOUS_INDEX));
    // Walk the rest of the chain without clobbering cp.
    current = next;
    to_check--;
  }

  // All extension objects were empty and it is safe to use a normal global
  // load machinery.
  EmitGlobalVariableLoad(proxy, typeof_mode);
}

MemOperand FullCodeGenerator::ContextSlotOperandCheckExtensions(Variable* var,
                                                                Label* slow) {
  DCHECK(var->IsContextSlot());
  Register context = cp;
  Register next = r5;
  Register temp = r6;

  for (Scope* s = scope(); s != var->scope(); s = s->outer_scope()) {
    if (s->NeedsContext()) {
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
    __ LoadP(r2, ContextSlotOperandCheckExtensions(local, slow));
    if (local->binding_needs_init()) {
      __ CompareRoot(r2, Heap::kTheHoleValueRootIndex);
      __ bne(done);
      __ mov(r2, Operand(var->name()));
      __ push(r2);
      __ CallRuntime(Runtime::kThrowReferenceError);
    } else {
      __ b(done);
    }
  }
}

void FullCodeGenerator::EmitVariableLoad(VariableProxy* proxy,
                                         TypeofMode typeof_mode) {
  // Record position before possible IC call.
  SetExpressionPosition(proxy);
  PrepareForBailoutForId(proxy->BeforeId(), BailoutState::NO_REGISTERS);
  Variable* var = proxy->var();

  // Three cases: global variables, lookup variables, and all other types of
  // variables.
  switch (var->location()) {
    case VariableLocation::UNALLOCATED: {
      Comment cmnt(masm_, "[ Global variable");
      EmitGlobalVariableLoad(proxy, typeof_mode);
      context()->Plug(r2);
      break;
    }

    case VariableLocation::PARAMETER:
    case VariableLocation::LOCAL:
    case VariableLocation::CONTEXT: {
      DCHECK_EQ(NOT_INSIDE_TYPEOF, typeof_mode);
      Comment cmnt(masm_, var->IsContextSlot() ? "[ Context variable"
                                               : "[ Stack variable");
      if (NeedsHoleCheckForLoad(proxy)) {
        // Throw a reference error when using an uninitialized let/const
        // binding in harmony mode.
        Label done;
        GetVar(r2, var);
        __ CompareRoot(r2, Heap::kTheHoleValueRootIndex);
        __ bne(&done);
        __ mov(r2, Operand(var->name()));
        __ push(r2);
        __ CallRuntime(Runtime::kThrowReferenceError);
        __ bind(&done);
        context()->Plug(r2);
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
      __ Push(var->name());
      Runtime::FunctionId function_id =
          typeof_mode == NOT_INSIDE_TYPEOF
              ? Runtime::kLoadLookupSlot
              : Runtime::kLoadLookupSlotInsideTypeof;
      __ CallRuntime(function_id);
      __ bind(&done);
      context()->Plug(r2);
      break;
    }

    case VariableLocation::MODULE:
      UNREACHABLE();
  }
}

void FullCodeGenerator::EmitAccessor(ObjectLiteralProperty* property) {
  Expression* expression = (property == NULL) ? NULL : property->value();
  if (expression == NULL) {
    __ LoadRoot(r3, Heap::kNullValueRootIndex);
    PushOperand(r3);
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
  __ LoadP(r5, MemOperand(fp, JavaScriptFrameConstants::kFunctionOffset));
  __ LoadSmiLiteral(r4, Smi::FromInt(expr->literal_index()));
  __ mov(r3, Operand(constant_properties));
  int flags = expr->ComputeFlags();
  __ LoadSmiLiteral(r2, Smi::FromInt(flags));
  if (MustCreateObjectLiteralWithRuntime(expr)) {
    __ Push(r5, r4, r3, r2);
    __ CallRuntime(Runtime::kCreateObjectLiteral);
  } else {
    FastCloneShallowObjectStub stub(isolate(), expr->properties_count());
    __ CallStub(&stub);
    RestoreContext();
  }
  PrepareForBailoutForId(expr->CreateLiteralId(), BailoutState::TOS_REGISTER);

  // If result_saved is true the result is on top of the stack.  If
  // result_saved is false the result is in r2.
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
      PushOperand(r2);  // Save result on stack
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
        if (key->IsStringLiteral()) {
          DCHECK(key->IsPropertyName());
          if (property->emit_store()) {
            VisitForAccumulatorValue(value);
            DCHECK(StoreDescriptor::ValueRegister().is(r2));
            __ LoadP(StoreDescriptor::ReceiverRegister(), MemOperand(sp));
            CallStoreIC(property->GetSlot(0), key->value());
            PrepareForBailoutForId(key->id(), BailoutState::NO_REGISTERS);

            if (NeedsHomeObject(value)) {
              EmitSetHomeObjectAccumulator(value, 0, property->GetSlot(1));
            }
          } else {
            VisitForEffect(value);
          }
          break;
        }
        // Duplicate receiver on stack.
        __ LoadP(r2, MemOperand(sp));
        PushOperand(r2);
        VisitForStackValue(key);
        VisitForStackValue(value);
        if (property->emit_store()) {
          if (NeedsHomeObject(value)) {
            EmitSetHomeObject(value, 2, property->GetSlot());
          }
          __ LoadSmiLiteral(r2, Smi::FromInt(SLOPPY));  // PropertyAttributes
          PushOperand(r2);
          CallRuntimeWithOperands(Runtime::kSetProperty);
        } else {
          DropOperands(3);
        }
        break;
      case ObjectLiteral::Property::PROTOTYPE:
        // Duplicate receiver on stack.
        __ LoadP(r2, MemOperand(sp));
        PushOperand(r2);
        VisitForStackValue(value);
        DCHECK(property->emit_store());
        CallRuntimeWithOperands(Runtime::kInternalSetPrototype);
        PrepareForBailoutForId(expr->GetIdForPropertySet(property_index),
                               BailoutState::NO_REGISTERS);
        break;
      case ObjectLiteral::Property::GETTER:
        if (property->emit_store()) {
          AccessorTable::Iterator it = accessor_table.lookup(key);
          it->second->bailout_id = expr->GetIdForPropertySet(property_index);
          it->second->getter = property;
        }
        break;
      case ObjectLiteral::Property::SETTER:
        if (property->emit_store()) {
          AccessorTable::Iterator it = accessor_table.lookup(key);
          it->second->bailout_id = expr->GetIdForPropertySet(property_index);
          it->second->setter = property;
        }
        break;
    }
  }

  // Emit code to define accessors, using only a single call to the runtime for
  // each pair of corresponding getters and setters.
  for (AccessorTable::Iterator it = accessor_table.begin();
       it != accessor_table.end(); ++it) {
    __ LoadP(r2, MemOperand(sp));  // Duplicate receiver.
    PushOperand(r2);
    VisitForStackValue(it->first);
    EmitAccessor(it->second->getter);
    EmitAccessor(it->second->setter);
    __ LoadSmiLiteral(r2, Smi::FromInt(NONE));
    PushOperand(r2);
    CallRuntimeWithOperands(Runtime::kDefineAccessorPropertyUnchecked);
    PrepareForBailoutForId(it->second->bailout_id, BailoutState::NO_REGISTERS);
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
      PushOperand(r2);  // Save result on the stack
      result_saved = true;
    }

    __ LoadP(r2, MemOperand(sp));  // Duplicate receiver.
    PushOperand(r2);

    if (property->kind() == ObjectLiteral::Property::PROTOTYPE) {
      DCHECK(!property->is_computed_name());
      VisitForStackValue(value);
      DCHECK(property->emit_store());
      CallRuntimeWithOperands(Runtime::kInternalSetPrototype);
      PrepareForBailoutForId(expr->GetIdForPropertySet(property_index),
                             BailoutState::NO_REGISTERS);
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
            PushOperand(Smi::FromInt(NONE));
            PushOperand(Smi::FromInt(property->NeedsSetFunctionName()));
            CallRuntimeWithOperands(Runtime::kDefineDataPropertyInLiteral);
            PrepareForBailoutForId(expr->GetIdForPropertySet(property_index),
                                   BailoutState::NO_REGISTERS);
          } else {
            DropOperands(3);
          }
          break;

        case ObjectLiteral::Property::PROTOTYPE:
          UNREACHABLE();
          break;

        case ObjectLiteral::Property::GETTER:
          PushOperand(Smi::FromInt(NONE));
          CallRuntimeWithOperands(Runtime::kDefineGetterPropertyUnchecked);
          break;

        case ObjectLiteral::Property::SETTER:
          PushOperand(Smi::FromInt(NONE));
          CallRuntimeWithOperands(Runtime::kDefineSetterPropertyUnchecked);
          break;
      }
    }
  }

  if (result_saved) {
    context()->PlugTOS();
  } else {
    context()->Plug(r2);
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

  __ LoadP(r5, MemOperand(fp, JavaScriptFrameConstants::kFunctionOffset));
  __ LoadSmiLiteral(r4, Smi::FromInt(expr->literal_index()));
  __ mov(r3, Operand(constant_elements));
  if (MustCreateArrayLiteralWithRuntime(expr)) {
    __ LoadSmiLiteral(r2, Smi::FromInt(expr->ComputeFlags()));
    __ Push(r5, r4, r3, r2);
    __ CallRuntime(Runtime::kCreateArrayLiteral);
  } else {
    FastCloneShallowArrayStub stub(isolate(), allocation_site_mode);
    __ CallStub(&stub);
    RestoreContext();
  }
  PrepareForBailoutForId(expr->CreateLiteralId(), BailoutState::TOS_REGISTER);

  bool result_saved = false;  // Is the result saved to the stack?
  ZoneList<Expression*>* subexprs = expr->values();
  int length = subexprs->length();

  // Emit code to evaluate all the non-constant subexpressions and to store
  // them into the newly cloned array.
  for (int array_index = 0; array_index < length; array_index++) {
    Expression* subexpr = subexprs->at(array_index);
    DCHECK(!subexpr->IsSpread());
    // If the subexpression is a literal or a simple materialized literal it
    // is already set in the cloned array.
    if (CompileTimeValue::IsCompileTimeValue(subexpr)) continue;

    if (!result_saved) {
      PushOperand(r2);
      result_saved = true;
    }
    VisitForAccumulatorValue(subexpr);

    __ LoadSmiLiteral(StoreDescriptor::NameRegister(),
                      Smi::FromInt(array_index));
    __ LoadP(StoreDescriptor::ReceiverRegister(), MemOperand(sp, 0));
    CallKeyedStoreIC(expr->LiteralFeedbackSlot());

    PrepareForBailoutForId(expr->GetIdForElement(array_index),
                           BailoutState::NO_REGISTERS);
  }

  if (result_saved) {
    context()->PlugTOS();
  } else {
    context()->Plug(r2);
  }
}

void FullCodeGenerator::VisitAssignment(Assignment* expr) {
  DCHECK(expr->target()->IsValidReferenceExpressionOrThis());

  Comment cmnt(masm_, "[ Assignment");

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
      PushOperand(result_register());
      if (expr->is_compound()) {
        const Register scratch = r3;
        __ LoadP(scratch, MemOperand(sp, kPointerSize));
        PushOperands(scratch, result_register());
      }
      break;
    case KEYED_SUPER_PROPERTY: {
      VisitForStackValue(
          property->obj()->AsSuperPropertyReference()->this_var());
      VisitForStackValue(
          property->obj()->AsSuperPropertyReference()->home_object());
      VisitForAccumulatorValue(property->key());
      PushOperand(result_register());
      if (expr->is_compound()) {
        const Register scratch1 = r4;
        const Register scratch2 = r3;
        __ LoadP(scratch1, MemOperand(sp, 2 * kPointerSize));
        __ LoadP(scratch2, MemOperand(sp, 1 * kPointerSize));
        PushOperands(scratch1, scratch2, result_register());
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
          PrepareForBailout(expr->target(), BailoutState::TOS_REGISTER);
          break;
        case NAMED_PROPERTY:
          EmitNamedPropertyLoad(property);
          PrepareForBailoutForId(property->LoadId(),
                                 BailoutState::TOS_REGISTER);
          break;
        case NAMED_SUPER_PROPERTY:
          EmitNamedSuperPropertyLoad(property);
          PrepareForBailoutForId(property->LoadId(),
                                 BailoutState::TOS_REGISTER);
          break;
        case KEYED_SUPER_PROPERTY:
          EmitKeyedSuperPropertyLoad(property);
          PrepareForBailoutForId(property->LoadId(),
                                 BailoutState::TOS_REGISTER);
          break;
        case KEYED_PROPERTY:
          EmitKeyedPropertyLoad(property);
          PrepareForBailoutForId(property->LoadId(),
                                 BailoutState::TOS_REGISTER);
          break;
      }
    }

    Token::Value op = expr->binary_op();
    PushOperand(r2);  // Left operand goes on the stack.
    VisitForAccumulatorValue(expr->value());

    AccumulatorValueContext context(this);
    if (ShouldInlineSmiCase(op)) {
      EmitInlineSmiBinaryOp(expr->binary_operation(), op, expr->target(),
                            expr->value());
    } else {
      EmitBinaryOp(expr->binary_operation(), op);
    }

    // Deoptimization point in case the binary operation may have side effects.
    PrepareForBailout(expr->binary_operation(), BailoutState::TOS_REGISTER);
  } else {
    VisitForAccumulatorValue(expr->value());
  }

  SetExpressionPosition(expr);

  // Store the value.
  switch (assign_type) {
    case VARIABLE:
      EmitVariableAssignment(expr->target()->AsVariableProxy()->var(),
                             expr->op(), expr->AssignmentSlot());
      PrepareForBailoutForId(expr->AssignmentId(), BailoutState::TOS_REGISTER);
      context()->Plug(r2);
      break;
    case NAMED_PROPERTY:
      EmitNamedPropertyAssignment(expr);
      break;
    case NAMED_SUPER_PROPERTY:
      EmitNamedSuperPropertyStore(property);
      context()->Plug(r2);
      break;
    case KEYED_SUPER_PROPERTY:
      EmitKeyedSuperPropertyStore(property);
      context()->Plug(r2);
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

  Label suspend, continuation, post_runtime, resume, exception;

  __ b(&suspend);
  __ bind(&continuation);
  // When we arrive here, r2 holds the generator object.
  __ RecordGeneratorContinuation();
  __ LoadP(r3, FieldMemOperand(r2, JSGeneratorObject::kResumeModeOffset));
  __ LoadP(r2, FieldMemOperand(r2, JSGeneratorObject::kInputOrDebugPosOffset));
  STATIC_ASSERT(JSGeneratorObject::kNext < JSGeneratorObject::kReturn);
  STATIC_ASSERT(JSGeneratorObject::kThrow > JSGeneratorObject::kReturn);
  __ CmpSmiLiteral(r3, Smi::FromInt(JSGeneratorObject::kReturn), r0);
  __ blt(&resume);
  __ Push(result_register());
  __ bgt(&exception);
  EmitCreateIteratorResult(true);
  EmitUnwindAndReturn();

  __ bind(&exception);
  __ CallRuntime(expr->rethrow_on_exception() ? Runtime::kReThrow
                                              : Runtime::kThrow);

  __ bind(&suspend);
  OperandStackDepthIncrement(1);  // Not popped on this path.
  VisitForAccumulatorValue(expr->generator_object());
  DCHECK(continuation.pos() > 0 && Smi::IsValid(continuation.pos()));
  __ LoadSmiLiteral(r3, Smi::FromInt(continuation.pos()));
  __ StoreP(r3, FieldMemOperand(r2, JSGeneratorObject::kContinuationOffset),
            r0);
  __ StoreP(cp, FieldMemOperand(r2, JSGeneratorObject::kContextOffset), r0);
  __ LoadRR(r3, cp);
  __ RecordWriteField(r2, JSGeneratorObject::kContextOffset, r3, r4,
                      kLRHasBeenSaved, kDontSaveFPRegs);
  __ AddP(r3, fp, Operand(StandardFrameConstants::kExpressionsOffset));
  __ CmpP(sp, r3);
  __ beq(&post_runtime);
  __ push(r2);  // generator object
  __ CallRuntime(Runtime::kSuspendJSGeneratorObject, 1);
  RestoreContext();
  __ bind(&post_runtime);
  PopOperand(result_register());
  EmitReturnSequence();

  __ bind(&resume);
  context()->Plug(result_register());
}

void FullCodeGenerator::PushOperands(Register reg1, Register reg2) {
  OperandStackDepthIncrement(2);
  __ Push(reg1, reg2);
}

void FullCodeGenerator::PushOperands(Register reg1, Register reg2,
                                     Register reg3) {
  OperandStackDepthIncrement(3);
  __ Push(reg1, reg2, reg3);
}

void FullCodeGenerator::PushOperands(Register reg1, Register reg2,
                                     Register reg3, Register reg4) {
  OperandStackDepthIncrement(4);
  __ Push(reg1, reg2, reg3, reg4);
}

void FullCodeGenerator::PopOperands(Register reg1, Register reg2) {
  OperandStackDepthDecrement(2);
  __ Pop(reg1, reg2);
}

void FullCodeGenerator::EmitOperandStackDepthCheck() {
  if (FLAG_debug_code) {
    int expected_diff = StandardFrameConstants::kFixedFrameSizeFromFp +
                        operand_stack_depth_ * kPointerSize;
    __ SubP(r2, fp, sp);
    __ CmpP(r2, Operand(expected_diff));
    __ Assert(eq, kUnexpectedStackDepth);
  }
}

void FullCodeGenerator::EmitCreateIteratorResult(bool done) {
  Label allocate, done_allocate;

  __ Allocate(JSIteratorResult::kSize, r2, r4, r5, &allocate,
              NO_ALLOCATION_FLAGS);
  __ b(&done_allocate);

  __ bind(&allocate);
  __ Push(Smi::FromInt(JSIteratorResult::kSize));
  __ CallRuntime(Runtime::kAllocateInNewSpace);

  __ bind(&done_allocate);
  __ LoadNativeContextSlot(Context::ITERATOR_RESULT_MAP_INDEX, r3);
  PopOperand(r4);
  __ LoadRoot(r5,
              done ? Heap::kTrueValueRootIndex : Heap::kFalseValueRootIndex);
  __ LoadRoot(r6, Heap::kEmptyFixedArrayRootIndex);
  __ StoreP(r3, FieldMemOperand(r2, HeapObject::kMapOffset), r0);
  __ StoreP(r6, FieldMemOperand(r2, JSObject::kPropertiesOffset), r0);
  __ StoreP(r6, FieldMemOperand(r2, JSObject::kElementsOffset), r0);
  __ StoreP(r4, FieldMemOperand(r2, JSIteratorResult::kValueOffset), r0);
  __ StoreP(r5, FieldMemOperand(r2, JSIteratorResult::kDoneOffset), r0);
}

void FullCodeGenerator::EmitInlineSmiBinaryOp(BinaryOperation* expr,
                                              Token::Value op,
                                              Expression* left_expr,
                                              Expression* right_expr) {
  Label done, smi_case, stub_call;

  Register scratch1 = r4;
  Register scratch2 = r5;

  // Get the arguments.
  Register left = r3;
  Register right = r2;
  PopOperand(left);

  // Perform combined smi check on both operands.
  __ LoadRR(scratch1, right);
  __ OrP(scratch1, left);
  STATIC_ASSERT(kSmiTag == 0);
  JumpPatchSite patch_site(masm_);
  patch_site.EmitJumpIfSmi(scratch1, &smi_case);

  __ bind(&stub_call);
  Handle<Code> code = CodeFactory::BinaryOpIC(isolate(), op).code();
  CallIC(code, expr->BinaryOperationFeedbackId());
  patch_site.EmitPatchInfo();
  __ b(&done);

  __ bind(&smi_case);
  // Smi case. This code works the same way as the smi-smi case in the type
  // recording binary operation stub.
  switch (op) {
    case Token::SAR:
      __ GetLeastBitsFromSmi(scratch1, right, 5);
      __ ShiftRightArithP(right, left, scratch1);
      __ ClearRightImm(right, right, Operand(kSmiTagSize + kSmiShiftSize));
      break;
    case Token::SHL: {
      __ GetLeastBitsFromSmi(scratch2, right, 5);
#if V8_TARGET_ARCH_S390X
      __ ShiftLeftP(right, left, scratch2);
#else
      __ SmiUntag(scratch1, left);
      __ ShiftLeftP(scratch1, scratch1, scratch2);
      // Check that the *signed* result fits in a smi
      __ JumpIfNotSmiCandidate(scratch1, scratch2, &stub_call);
      __ SmiTag(right, scratch1);
#endif
      break;
    }
    case Token::SHR: {
      __ SmiUntag(scratch1, left);
      __ GetLeastBitsFromSmi(scratch2, right, 5);
      __ srl(scratch1, scratch2);
      // Unsigned shift is not allowed to produce a negative number.
      __ JumpIfNotUnsignedSmiCandidate(scratch1, r0, &stub_call);
      __ SmiTag(right, scratch1);
      break;
    }
    case Token::ADD: {
      __ AddP(scratch1, left, right);
      __ b(overflow, &stub_call);
      __ LoadRR(right, scratch1);
      break;
    }
    case Token::SUB: {
      __ SubP(scratch1, left, right);
      __ b(overflow, &stub_call);
      __ LoadRR(right, scratch1);
      break;
    }
    case Token::MUL: {
      Label mul_zero;
#if V8_TARGET_ARCH_S390X
      // Remove tag from both operands.
      __ SmiUntag(ip, right);
      __ SmiUntag(scratch2, left);
      __ mr_z(scratch1, ip);
      // Check for overflowing the smi range - no overflow if higher 33 bits of
      // the result are identical.
      __ lr(ip, scratch2);  // 32 bit load
      __ sra(ip, Operand(31));
      __ cr_z(ip, scratch1);  // 32 bit compare
      __ bne(&stub_call);
#else
      __ SmiUntag(ip, right);
      __ LoadRR(scratch2, left);  // load into low order of reg pair
      __ mr_z(scratch1, ip);      // R4:R5 = R5 * ip
      // Check for overflowing the smi range - no overflow if higher 33 bits of
      // the result are identical.
      __ TestIfInt32(scratch1, scratch2, ip);
      __ bne(&stub_call);
#endif
      // Go slow on zero result to handle -0.
      __ chi(scratch2, Operand::Zero());
      __ beq(&mul_zero, Label::kNear);
#if V8_TARGET_ARCH_S390X
      __ SmiTag(right, scratch2);
#else
      __ LoadRR(right, scratch2);
#endif
      __ b(&done);
      // We need -0 if we were multiplying a negative number with 0 to get 0.
      // We know one of them was zero.
      __ bind(&mul_zero);
      __ AddP(scratch2, right, left);
      __ CmpP(scratch2, Operand::Zero());
      __ blt(&stub_call);
      __ LoadSmiLiteral(right, Smi::FromInt(0));
      break;
    }
    case Token::BIT_OR:
      __ OrP(right, left);
      break;
    case Token::BIT_AND:
      __ AndP(right, left);
      break;
    case Token::BIT_XOR:
      __ XorP(right, left);
      break;
    default:
      UNREACHABLE();
  }

  __ bind(&done);
  context()->Plug(r2);
}

void FullCodeGenerator::EmitClassDefineProperties(ClassLiteral* lit) {
  for (int i = 0; i < lit->properties()->length(); i++) {
    ClassLiteral::Property* property = lit->properties()->at(i);
    Expression* value = property->value();

    Register scratch = r3;
    if (property->is_static()) {
      __ LoadP(scratch, MemOperand(sp, kPointerSize));  // constructor
    } else {
      __ LoadP(scratch, MemOperand(sp, 0));  // prototype
    }
    PushOperand(scratch);
    EmitPropertyKey(property, lit->GetIdForProperty(i));

    // The static prototype property is read only. We handle the non computed
    // property name case in the parser. Since this is the only case where we
    // need to check for an own read only property we special case this so we do
    // not need to do this for every property.
    if (property->is_static() && property->is_computed_name()) {
      __ CallRuntime(Runtime::kThrowIfStaticPrototype);
      __ push(r2);
    }

    VisitForStackValue(value);
    if (NeedsHomeObject(value)) {
      EmitSetHomeObject(value, 2, property->GetSlot());
    }

    switch (property->kind()) {
      case ClassLiteral::Property::METHOD:
        PushOperand(Smi::FromInt(DONT_ENUM));
        PushOperand(Smi::FromInt(property->NeedsSetFunctionName()));
        CallRuntimeWithOperands(Runtime::kDefineDataPropertyInLiteral);
        break;

      case ClassLiteral::Property::GETTER:
        PushOperand(Smi::FromInt(DONT_ENUM));
        CallRuntimeWithOperands(Runtime::kDefineGetterPropertyUnchecked);
        break;

      case ClassLiteral::Property::SETTER:
        PushOperand(Smi::FromInt(DONT_ENUM));
        CallRuntimeWithOperands(Runtime::kDefineSetterPropertyUnchecked);
        break;

      case ClassLiteral::Property::FIELD:
      default:
        UNREACHABLE();
    }
  }
}

void FullCodeGenerator::EmitBinaryOp(BinaryOperation* expr, Token::Value op) {
  PopOperand(r3);
  Handle<Code> code = CodeFactory::BinaryOpIC(isolate(), op).code();
  JumpPatchSite patch_site(masm_);  // unbound, signals no inlined smi code.
  CallIC(code, expr->BinaryOperationFeedbackId());
  patch_site.EmitPatchInfo();
  context()->Plug(r2);
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
      PushOperand(r2);  // Preserve value.
      VisitForAccumulatorValue(prop->obj());
      __ Move(StoreDescriptor::ReceiverRegister(), r2);
      PopOperand(StoreDescriptor::ValueRegister());  // Restore value.
      CallStoreIC(slot, prop->key()->AsLiteral()->value());
      break;
    }
    case NAMED_SUPER_PROPERTY: {
      PushOperand(r2);
      VisitForStackValue(prop->obj()->AsSuperPropertyReference()->this_var());
      VisitForAccumulatorValue(
          prop->obj()->AsSuperPropertyReference()->home_object());
      // stack: value, this; r2: home_object
      Register scratch = r4;
      Register scratch2 = r5;
      __ LoadRR(scratch, result_register());              // home_object
      __ LoadP(r2, MemOperand(sp, kPointerSize));         // value
      __ LoadP(scratch2, MemOperand(sp, 0));              // this
      __ StoreP(scratch2, MemOperand(sp, kPointerSize));  // this
      __ StoreP(scratch, MemOperand(sp, 0));              // home_object
      // stack: this, home_object; r2: value
      EmitNamedSuperPropertyStore(prop);
      break;
    }
    case KEYED_SUPER_PROPERTY: {
      PushOperand(r2);
      VisitForStackValue(prop->obj()->AsSuperPropertyReference()->this_var());
      VisitForStackValue(
          prop->obj()->AsSuperPropertyReference()->home_object());
      VisitForAccumulatorValue(prop->key());
      Register scratch = r4;
      Register scratch2 = r5;
      __ LoadP(scratch2, MemOperand(sp, 2 * kPointerSize));  // value
      // stack: value, this, home_object; r3: key, r6: value
      __ LoadP(scratch, MemOperand(sp, kPointerSize));  // this
      __ StoreP(scratch, MemOperand(sp, 2 * kPointerSize));
      __ LoadP(scratch, MemOperand(sp, 0));  // home_object
      __ StoreP(scratch, MemOperand(sp, kPointerSize));
      __ StoreP(r2, MemOperand(sp, 0));
      __ Move(r2, scratch2);
      // stack: this, home_object, key; r2: value.
      EmitKeyedSuperPropertyStore(prop);
      break;
    }
    case KEYED_PROPERTY: {
      PushOperand(r2);  // Preserve value.
      VisitForStackValue(prop->obj());
      VisitForAccumulatorValue(prop->key());
      __ Move(StoreDescriptor::NameRegister(), r2);
      PopOperands(StoreDescriptor::ValueRegister(),
                  StoreDescriptor::ReceiverRegister());
      CallKeyedStoreIC(slot);
      break;
    }
  }
  context()->Plug(r2);
}

void FullCodeGenerator::EmitStoreToStackLocalOrContextSlot(
    Variable* var, MemOperand location) {
  __ StoreP(result_register(), location);
  if (var->IsContextSlot()) {
    // RecordWrite may destroy all its register arguments.
    __ LoadRR(r5, result_register());
    int offset = Context::SlotOffset(var->index());
    __ RecordWriteContextSlot(r3, offset, r5, r4, kLRHasBeenSaved,
                              kDontSaveFPRegs);
  }
}

void FullCodeGenerator::EmitVariableAssignment(Variable* var, Token::Value op,
                                               FeedbackVectorSlot slot) {
  if (var->IsUnallocated()) {
    // Global var, const, or let.
    __ LoadGlobalObject(StoreDescriptor::ReceiverRegister());
    CallStoreIC(slot, var->name());

  } else if (IsLexicalVariableMode(var->mode()) && op != Token::INIT) {
    // Non-initializing assignment to let variable needs a write barrier.
    DCHECK(!var->IsLookupSlot());
    DCHECK(var->IsStackAllocated() || var->IsContextSlot());
    MemOperand location = VarOperand(var, r3);
    // Perform an initialization check for lexically declared variables.
    if (var->binding_needs_init()) {
      Label assign;
      __ LoadP(r5, location);
      __ CompareRoot(r5, Heap::kTheHoleValueRootIndex);
      __ bne(&assign);
      __ mov(r5, Operand(var->name()));
      __ push(r5);
      __ CallRuntime(Runtime::kThrowReferenceError);
      __ bind(&assign);
    }
    if (var->mode() != CONST) {
      EmitStoreToStackLocalOrContextSlot(var, location);
    } else if (var->throw_on_const_assignment(language_mode())) {
      __ CallRuntime(Runtime::kThrowConstAssignError);
    }
  } else if (var->is_this() && var->mode() == CONST && op == Token::INIT) {
    // Initializing assignment to const {this} needs a write barrier.
    DCHECK(var->IsStackAllocated() || var->IsContextSlot());
    Label uninitialized_this;
    MemOperand location = VarOperand(var, r3);
    __ LoadP(r5, location);
    __ CompareRoot(r5, Heap::kTheHoleValueRootIndex);
    __ beq(&uninitialized_this);
    __ mov(r3, Operand(var->name()));
    __ push(r3);
    __ CallRuntime(Runtime::kThrowReferenceError);
    __ bind(&uninitialized_this);
    EmitStoreToStackLocalOrContextSlot(var, location);
  } else {
    DCHECK(var->mode() != CONST || op == Token::INIT);
    if (var->IsLookupSlot()) {
      // Assignment to var.
      __ Push(var->name());
      __ Push(r2);
      __ CallRuntime(is_strict(language_mode())
                         ? Runtime::kStoreLookupSlot_Strict
                         : Runtime::kStoreLookupSlot_Sloppy);
    } else {
      // Assignment to var or initializing assignment to let/const in harmony
      // mode.
      DCHECK((var->IsStackAllocated() || var->IsContextSlot()));
      MemOperand location = VarOperand(var, r3);
      if (FLAG_debug_code && var->mode() == LET && op == Token::INIT) {
        // Check for an uninitialized let binding.
        __ LoadP(r4, location);
        __ CompareRoot(r4, Heap::kTheHoleValueRootIndex);
        __ Check(eq, kLetBindingReInitialization);
      }
      EmitStoreToStackLocalOrContextSlot(var, location);
    }
  }
}

void FullCodeGenerator::EmitNamedPropertyAssignment(Assignment* expr) {
  // Assignment to a property, using a named store IC.
  Property* prop = expr->target()->AsProperty();
  DCHECK(prop != NULL);
  DCHECK(prop->key()->IsLiteral());

  PopOperand(StoreDescriptor::ReceiverRegister());
  CallStoreIC(expr->AssignmentSlot(), prop->key()->AsLiteral()->value());

  PrepareForBailoutForId(expr->AssignmentId(), BailoutState::TOS_REGISTER);
  context()->Plug(r2);
}

void FullCodeGenerator::EmitNamedSuperPropertyStore(Property* prop) {
  // Assignment to named property of super.
  // r2 : value
  // stack : receiver ('this'), home_object
  DCHECK(prop != NULL);
  Literal* key = prop->key()->AsLiteral();
  DCHECK(key != NULL);

  PushOperand(key->value());
  PushOperand(r2);
  CallRuntimeWithOperands((is_strict(language_mode())
                               ? Runtime::kStoreToSuper_Strict
                               : Runtime::kStoreToSuper_Sloppy));
}

void FullCodeGenerator::EmitKeyedSuperPropertyStore(Property* prop) {
  // Assignment to named property of super.
  // r2 : value
  // stack : receiver ('this'), home_object, key
  DCHECK(prop != NULL);

  PushOperand(r2);
  CallRuntimeWithOperands((is_strict(language_mode())
                               ? Runtime::kStoreKeyedToSuper_Strict
                               : Runtime::kStoreKeyedToSuper_Sloppy));
}

void FullCodeGenerator::EmitKeyedPropertyAssignment(Assignment* expr) {
  // Assignment to a property, using a keyed store IC.
  PopOperands(StoreDescriptor::ReceiverRegister(),
              StoreDescriptor::NameRegister());
  DCHECK(StoreDescriptor::ValueRegister().is(r2));

  CallKeyedStoreIC(expr->AssignmentSlot());

  PrepareForBailoutForId(expr->AssignmentId(), BailoutState::TOS_REGISTER);
  context()->Plug(r2);
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
      PrepareForBailout(callee, BailoutState::NO_REGISTERS);
    }
    // Push undefined as receiver. This is patched in the method prologue if it
    // is a sloppy mode method.
    __ LoadRoot(r1, Heap::kUndefinedValueRootIndex);
    PushOperand(r1);
    convert_mode = ConvertReceiverMode::kNullOrUndefined;
  } else {
    // Load the function from the receiver.
    DCHECK(callee->IsProperty());
    DCHECK(!callee->AsProperty()->IsSuperAccess());
    __ LoadP(LoadDescriptor::ReceiverRegister(), MemOperand(sp, 0));
    EmitNamedPropertyLoad(callee->AsProperty());
    PrepareForBailoutForId(callee->AsProperty()->LoadId(),
                           BailoutState::TOS_REGISTER);
    // Push the target function under the receiver.
    __ LoadP(r1, MemOperand(sp, 0));
    PushOperand(r1);
    __ StoreP(r2, MemOperand(sp, kPointerSize));
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
  const Register scratch = r3;
  SuperPropertyReference* super_ref = prop->obj()->AsSuperPropertyReference();
  VisitForAccumulatorValue(super_ref->home_object());
  __ LoadRR(scratch, r2);
  VisitForAccumulatorValue(super_ref->this_var());
  PushOperands(scratch, r2, r2, scratch);
  PushOperand(key->value());

  // Stack here:
  //  - home_object
  //  - this (receiver)
  //  - this (receiver) <-- LoadFromSuper will pop here and below.
  //  - home_object
  //  - key
  CallRuntimeWithOperands(Runtime::kLoadFromSuper);
  PrepareForBailoutForId(prop->LoadId(), BailoutState::TOS_REGISTER);

  // Replace home_object with target function.
  __ StoreP(r2, MemOperand(sp, kPointerSize));

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
  __ Move(LoadDescriptor::NameRegister(), r2);
  EmitKeyedPropertyLoad(callee->AsProperty());
  PrepareForBailoutForId(callee->AsProperty()->LoadId(),
                         BailoutState::TOS_REGISTER);

  // Push the target function under the receiver.
  __ LoadP(ip, MemOperand(sp, 0));
  PushOperand(ip);
  __ StoreP(r2, MemOperand(sp, kPointerSize));

  EmitCall(expr, ConvertReceiverMode::kNotNullOrUndefined);
}

void FullCodeGenerator::EmitKeyedSuperCallWithLoadIC(Call* expr) {
  Expression* callee = expr->expression();
  DCHECK(callee->IsProperty());
  Property* prop = callee->AsProperty();
  DCHECK(prop->IsSuperAccess());

  SetExpressionPosition(prop);
  // Load the function from the receiver.
  const Register scratch = r3;
  SuperPropertyReference* super_ref = prop->obj()->AsSuperPropertyReference();
  VisitForAccumulatorValue(super_ref->home_object());
  __ LoadRR(scratch, r2);
  VisitForAccumulatorValue(super_ref->this_var());
  PushOperands(scratch, r2, r2, scratch);
  VisitForStackValue(prop->key());

  // Stack here:
  //  - home_object
  //  - this (receiver)
  //  - this (receiver) <-- LoadKeyedFromSuper will pop here and below.
  //  - home_object
  //  - key
  CallRuntimeWithOperands(Runtime::kLoadKeyedFromSuper);
  PrepareForBailoutForId(prop->LoadId(), BailoutState::TOS_REGISTER);

  // Replace home_object with target function.
  __ StoreP(r2, MemOperand(sp, kPointerSize));

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

  PrepareForBailoutForId(expr->CallId(), BailoutState::NO_REGISTERS);
  SetCallPosition(expr, expr->tail_call_mode());
  if (expr->tail_call_mode() == TailCallMode::kAllow) {
    if (FLAG_trace) {
      __ CallRuntime(Runtime::kTraceTailCall);
    }
    // Update profiling counters before the tail call since we will
    // not return to this function.
    EmitProfilingCounterHandlingForReturnSequence(true);
  }
  Handle<Code> ic =
      CodeFactory::CallIC(isolate(), arg_count, mode, expr->tail_call_mode())
          .code();
  __ LoadSmiLiteral(r5, SmiFromSlot(expr->CallFeedbackICSlot()));
  __ LoadP(r3, MemOperand(sp, (arg_count + 1) * kPointerSize), r0);
  // Don't assign a type feedback id to the IC, since type feedback is provided
  // by the vector above.
  CallIC(ic);
  OperandStackDepthDecrement(arg_count + 1);

  RecordJSReturnSite(expr);
  RestoreContext();
  context()->DropAndPlug(1, r2);
}

void FullCodeGenerator::EmitResolvePossiblyDirectEval(Call* expr) {
  int arg_count = expr->arguments()->length();
  // r6: copy of the first argument or undefined if it doesn't exist.
  if (arg_count > 0) {
    __ LoadP(r6, MemOperand(sp, arg_count * kPointerSize), r0);
  } else {
    __ LoadRoot(r6, Heap::kUndefinedValueRootIndex);
  }

  // r5: the receiver of the enclosing function.
  __ LoadP(r5, MemOperand(fp, JavaScriptFrameConstants::kFunctionOffset));

  // r4: language mode.
  __ LoadSmiLiteral(r4, Smi::FromInt(language_mode()));

  // r3: the start position of the scope the calls resides in.
  __ LoadSmiLiteral(r3, Smi::FromInt(scope()->start_position()));

  // r2: the source position of the eval call.
  __ LoadSmiLiteral(r2, Smi::FromInt(expr->position()));

  // Do the runtime call.
  __ Push(r6, r5, r4, r3, r2);
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
    // Call the runtime to find the function to call (returned in r2) and
    // the object holding it (returned in r3).
    __ Push(callee->name());
    __ CallRuntime(Runtime::kLoadLookupSlotForCall);
    PushOperands(r2, r3);  // Function, receiver.
    PrepareForBailoutForId(expr->LookupId(), BailoutState::NO_REGISTERS);

    // If fast case code has been generated, emit code to push the function
    // and receiver and have the slow path jump around this code.
    if (done.is_linked()) {
      Label call;
      __ b(&call);
      __ bind(&done);
      // Push function.
      __ push(r2);
      // Pass undefined as the receiver, which is the WithBaseObject of a
      // non-object environment record.  If the callee is sloppy, it will patch
      // it up to be the global receiver.
      __ LoadRoot(r3, Heap::kUndefinedValueRootIndex);
      __ push(r3);
      __ bind(&call);
    }
  } else {
    VisitForStackValue(callee);
    // refEnv.WithBaseObject()
    __ LoadRoot(r4, Heap::kUndefinedValueRootIndex);
    PushOperand(r4);  // Reserved receiver slot.
  }
}

void FullCodeGenerator::EmitPossiblyEvalCall(Call* expr) {
  // In a call to eval, we first call
  // Runtime_ResolvePossiblyDirectEval to resolve the function we need
  // to call.  Then we call the resolved function using the given arguments.
  ZoneList<Expression*>* args = expr->arguments();
  int arg_count = args->length();

  PushCalleeAndWithBaseObject(expr);

  // Push the arguments.
  for (int i = 0; i < arg_count; i++) {
    VisitForStackValue(args->at(i));
  }

  // Push a copy of the function (found below the arguments) and
  // resolve eval.
  __ LoadP(r3, MemOperand(sp, (arg_count + 1) * kPointerSize), r0);
  __ push(r3);
  EmitResolvePossiblyDirectEval(expr);

  // Touch up the stack with the resolved function.
  __ StoreP(r2, MemOperand(sp, (arg_count + 1) * kPointerSize), r0);

  PrepareForBailoutForId(expr->EvalId(), BailoutState::NO_REGISTERS);

  // Record source position for debugger.
  SetCallPosition(expr);
  __ LoadP(r3, MemOperand(sp, (arg_count + 1) * kPointerSize), r0);
  __ mov(r2, Operand(arg_count));
  __ Call(isolate()->builtins()->Call(ConvertReceiverMode::kAny,
                                      expr->tail_call_mode()),
          RelocInfo::CODE_TARGET);
  OperandStackDepthDecrement(arg_count + 1);
  RecordJSReturnSite(expr);
  RestoreContext();
  context()->DropAndPlug(1, r2);
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

  // Load function and argument count into r3 and r2.
  __ mov(r2, Operand(arg_count));
  __ LoadP(r3, MemOperand(sp, arg_count * kPointerSize), r0);

  // Record call targets in unoptimized code.
  __ EmitLoadTypeFeedbackVector(r4);
  __ LoadSmiLiteral(r5, SmiFromSlot(expr->CallNewFeedbackSlot()));

  CallConstructStub stub(isolate());
  __ Call(stub.GetCode(), RelocInfo::CODE_TARGET);
  OperandStackDepthDecrement(arg_count + 1);
  PrepareForBailoutForId(expr->ReturnId(), BailoutState::TOS_REGISTER);
  RestoreContext();
  context()->Plug(r2);
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
  PushOperand(result_register());

  // Push the arguments ("left-to-right") on the stack.
  ZoneList<Expression*>* args = expr->arguments();
  int arg_count = args->length();
  for (int i = 0; i < arg_count; i++) {
    VisitForStackValue(args->at(i));
  }

  // Call the construct call builtin that handles allocation and
  // constructor invocation.
  SetConstructCallPosition(expr);

  // Load new target into r5.
  VisitForAccumulatorValue(super_call_ref->new_target_var());
  __ LoadRR(r5, result_register());

  // Load function and argument count into r1 and r0.
  __ mov(r2, Operand(arg_count));
  __ LoadP(r3, MemOperand(sp, arg_count * kPointerSize));

  __ Call(isolate()->builtins()->Construct(), RelocInfo::CODE_TARGET);
  OperandStackDepthDecrement(arg_count + 1);

  RecordJSReturnSite(expr);
  RestoreContext();
  context()->Plug(r2);
}

void FullCodeGenerator::EmitIsSmi(CallRuntime* expr) {
  ZoneList<Expression*>* args = expr->arguments();
  DCHECK(args->length() == 1);

  VisitForAccumulatorValue(args->at(0));

  Label materialize_true, materialize_false, skip_lookup;
  Label* if_true = NULL;
  Label* if_false = NULL;
  Label* fall_through = NULL;
  context()->PrepareTest(&materialize_true, &materialize_false, &if_true,
                         &if_false, &fall_through);

  PrepareForBailoutBeforeSplit(expr, true, if_true, if_false);
  __ TestIfSmi(r2);
  Split(eq, if_true, if_false, fall_through);

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

  __ JumpIfSmi(r2, if_false);
  __ CompareObjectType(r2, r3, r3, FIRST_JS_RECEIVER_TYPE);
  PrepareForBailoutBeforeSplit(expr, true, if_true, if_false);
  Split(ge, if_true, if_false, fall_through);

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

  __ JumpIfSmi(r2, if_false);
  __ CompareObjectType(r2, r3, r3, JS_ARRAY_TYPE);
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

  __ JumpIfSmi(r2, if_false);
  __ CompareObjectType(r2, r3, r3, JS_TYPED_ARRAY_TYPE);
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

  __ JumpIfSmi(r2, if_false);
  __ CompareObjectType(r2, r3, r3, JS_REGEXP_TYPE);
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

  __ JumpIfSmi(r2, if_false);
  __ CompareObjectType(r2, r3, r3, JS_PROXY_TYPE);
  PrepareForBailoutBeforeSplit(expr, true, if_true, if_false);
  Split(eq, if_true, if_false, fall_through);

  context()->Plug(if_true, if_false);
}

void FullCodeGenerator::EmitClassOf(CallRuntime* expr) {
  ZoneList<Expression*>* args = expr->arguments();
  DCHECK(args->length() == 1);
  Label done, null, function, non_function_constructor;

  VisitForAccumulatorValue(args->at(0));

  // If the object is not a JSReceiver, we return null.
  __ JumpIfSmi(r2, &null);
  STATIC_ASSERT(LAST_JS_RECEIVER_TYPE == LAST_TYPE);
  __ CompareObjectType(r2, r2, r3, FIRST_JS_RECEIVER_TYPE);
  // Map is now in r2.
  __ blt(&null);

  // Return 'Function' for JSFunction and JSBoundFunction objects.
  __ CmpLogicalP(r3, Operand(FIRST_FUNCTION_TYPE));
  STATIC_ASSERT(LAST_FUNCTION_TYPE == LAST_TYPE);
  __ bge(&function);

  // Check if the constructor in the map is a JS function.
  Register instance_type = r4;
  __ GetMapConstructor(r2, r2, r3, instance_type);
  __ CmpP(instance_type, Operand(JS_FUNCTION_TYPE));
  __ bne(&non_function_constructor, Label::kNear);

  // r2 now contains the constructor function. Grab the
  // instance class name from there.
  __ LoadP(r2, FieldMemOperand(r2, JSFunction::kSharedFunctionInfoOffset));
  __ LoadP(r2,
           FieldMemOperand(r2, SharedFunctionInfo::kInstanceClassNameOffset));
  __ b(&done, Label::kNear);

  // Functions have class 'Function'.
  __ bind(&function);
  __ LoadRoot(r2, Heap::kFunction_stringRootIndex);
  __ b(&done, Label::kNear);

  // Objects with a non-function constructor have class 'Object'.
  __ bind(&non_function_constructor);
  __ LoadRoot(r2, Heap::kObject_stringRootIndex);
  __ b(&done, Label::kNear);

  // Non-JS objects have class null.
  __ bind(&null);
  __ LoadRoot(r2, Heap::kNullValueRootIndex);

  // All done.
  __ bind(&done);

  context()->Plug(r2);
}

void FullCodeGenerator::EmitStringCharCodeAt(CallRuntime* expr) {
  ZoneList<Expression*>* args = expr->arguments();
  DCHECK(args->length() == 2);
  VisitForStackValue(args->at(0));
  VisitForAccumulatorValue(args->at(1));

  Register object = r3;
  Register index = r2;
  Register result = r5;

  PopOperand(object);

  Label need_conversion;
  Label index_out_of_range;
  Label done;
  StringCharCodeAtGenerator generator(object, index, result, &need_conversion,
                                      &need_conversion, &index_out_of_range);
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

void FullCodeGenerator::EmitCall(CallRuntime* expr) {
  ZoneList<Expression*>* args = expr->arguments();
  DCHECK_LE(2, args->length());
  // Push target, receiver and arguments onto the stack.
  for (Expression* const arg : *args) {
    VisitForStackValue(arg);
  }
  PrepareForBailoutForId(expr->CallId(), BailoutState::NO_REGISTERS);
  // Move target to r3.
  int const argc = args->length() - 2;
  __ LoadP(r3, MemOperand(sp, (argc + 1) * kPointerSize));
  // Call the target.
  __ mov(r2, Operand(argc));
  __ Call(isolate()->builtins()->Call(), RelocInfo::CODE_TARGET);
  OperandStackDepthDecrement(argc + 1);
  RestoreContext();
  // Discard the function left on TOS.
  context()->DropAndPlug(1, r2);
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

  __ LoadlW(r2, FieldMemOperand(r2, String::kHashFieldOffset));
  __ AndP(r0, r2, Operand(String::kContainsCachedArrayIndexMask));
  PrepareForBailoutBeforeSplit(expr, true, if_true, if_false);
  Split(eq, if_true, if_false, fall_through);

  context()->Plug(if_true, if_false);
}

void FullCodeGenerator::EmitGetCachedArrayIndex(CallRuntime* expr) {
  ZoneList<Expression*>* args = expr->arguments();
  DCHECK(args->length() == 1);
  VisitForAccumulatorValue(args->at(0));

  __ AssertString(r2);

  __ LoadlW(r2, FieldMemOperand(r2, String::kHashFieldOffset));
  __ IndexFromHash(r2, r2);

  context()->Plug(r2);
}

void FullCodeGenerator::EmitGetSuperConstructor(CallRuntime* expr) {
  ZoneList<Expression*>* args = expr->arguments();
  DCHECK_EQ(1, args->length());
  VisitForAccumulatorValue(args->at(0));
  __ AssertFunction(r2);
  __ LoadP(r2, FieldMemOperand(r2, HeapObject::kMapOffset));
  __ LoadP(r2, FieldMemOperand(r2, Map::kPrototypeOffset));
  context()->Plug(r2);
}

void FullCodeGenerator::EmitDebugIsActive(CallRuntime* expr) {
  DCHECK(expr->arguments()->length() == 0);
  ExternalReference debug_is_active =
      ExternalReference::debug_is_active_address(isolate());
  __ mov(ip, Operand(debug_is_active));
  __ LoadlB(r2, MemOperand(ip));
  __ SmiTag(r2);
  context()->Plug(r2);
}

void FullCodeGenerator::EmitCreateIterResultObject(CallRuntime* expr) {
  ZoneList<Expression*>* args = expr->arguments();
  DCHECK_EQ(2, args->length());
  VisitForStackValue(args->at(0));
  VisitForStackValue(args->at(1));

  Label runtime, done;

  __ Allocate(JSIteratorResult::kSize, r2, r4, r5, &runtime,
              NO_ALLOCATION_FLAGS);
  __ LoadNativeContextSlot(Context::ITERATOR_RESULT_MAP_INDEX, r3);
  __ Pop(r4, r5);
  __ LoadRoot(r6, Heap::kEmptyFixedArrayRootIndex);
  __ StoreP(r3, FieldMemOperand(r2, HeapObject::kMapOffset), r0);
  __ StoreP(r6, FieldMemOperand(r2, JSObject::kPropertiesOffset), r0);
  __ StoreP(r6, FieldMemOperand(r2, JSObject::kElementsOffset), r0);
  __ StoreP(r4, FieldMemOperand(r2, JSIteratorResult::kValueOffset), r0);
  __ StoreP(r5, FieldMemOperand(r2, JSIteratorResult::kDoneOffset), r0);
  STATIC_ASSERT(JSIteratorResult::kSize == 5 * kPointerSize);
  __ b(&done);

  __ bind(&runtime);
  CallRuntimeWithOperands(Runtime::kCreateIterResultObject);

  __ bind(&done);
  context()->Plug(r2);
}

void FullCodeGenerator::EmitLoadJSRuntimeFunction(CallRuntime* expr) {
  // Push function.
  __ LoadNativeContextSlot(expr->context_index(), r2);
  PushOperand(r2);

  // Push undefined as the receiver.
  __ LoadRoot(r2, Heap::kUndefinedValueRootIndex);
  PushOperand(r2);
}

void FullCodeGenerator::EmitCallJSRuntimeFunction(CallRuntime* expr) {
  ZoneList<Expression*>* args = expr->arguments();
  int arg_count = args->length();

  SetCallPosition(expr);
  __ LoadP(r3, MemOperand(sp, (arg_count + 1) * kPointerSize), r0);
  __ mov(r2, Operand(arg_count));
  __ Call(isolate()->builtins()->Call(ConvertReceiverMode::kNullOrUndefined),
          RelocInfo::CODE_TARGET);
  OperandStackDepthDecrement(arg_count + 1);
  RestoreContext();
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
        CallRuntimeWithOperands(is_strict(language_mode())
                                    ? Runtime::kDeleteProperty_Strict
                                    : Runtime::kDeleteProperty_Sloppy);
        context()->Plug(r2);
      } else if (proxy != NULL) {
        Variable* var = proxy->var();
        // Delete of an unqualified identifier is disallowed in strict mode but
        // "delete this" is allowed.
        bool is_this = var->is_this();
        DCHECK(is_sloppy(language_mode()) || is_this);
        if (var->IsUnallocated()) {
          __ LoadGlobalObject(r4);
          __ mov(r3, Operand(var->name()));
          __ Push(r4, r3);
          __ CallRuntime(Runtime::kDeleteProperty_Sloppy);
          context()->Plug(r2);
        } else if (var->IsStackAllocated() || var->IsContextSlot()) {
          // Result of deleting non-global, non-dynamic variables is false.
          // The subexpression does not have side effects.
          context()->Plug(is_this);
        } else {
          // Non-global variable.  Call the runtime to try to delete from the
          // context where the variable was introduced.
          __ Push(var->name());
          __ CallRuntime(Runtime::kDeleteLookupSlot);
          context()->Plug(r2);
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
        if (!context()->IsAccumulatorValue()) OperandStackDepthIncrement(1);
        __ bind(&materialize_true);
        PrepareForBailoutForId(expr->MaterializeTrueId(),
                               BailoutState::NO_REGISTERS);
        __ LoadRoot(r2, Heap::kTrueValueRootIndex);
        if (context()->IsStackValue()) __ push(r2);
        __ b(&done);
        __ bind(&materialize_false);
        PrepareForBailoutForId(expr->MaterializeFalseId(),
                               BailoutState::NO_REGISTERS);
        __ LoadRoot(r2, Heap::kFalseValueRootIndex);
        if (context()->IsStackValue()) __ push(r2);
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
      __ LoadRR(r5, r2);
      TypeofStub typeof_stub(isolate());
      __ CallStub(&typeof_stub);
      context()->Plug(r2);
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
      PushOperand(ip);
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
        const Register scratch = r3;
        __ LoadP(scratch, MemOperand(sp, 0));  // this
        PushOperands(result_register(), scratch, result_register());
        EmitNamedSuperPropertyLoad(prop);
        break;
      }

      case KEYED_SUPER_PROPERTY: {
        VisitForStackValue(prop->obj()->AsSuperPropertyReference()->this_var());
        VisitForStackValue(
            prop->obj()->AsSuperPropertyReference()->home_object());
        VisitForAccumulatorValue(prop->key());
        const Register scratch1 = r3;
        const Register scratch2 = r4;
        __ LoadP(scratch1, MemOperand(sp, 1 * kPointerSize));  // this
        __ LoadP(scratch2, MemOperand(sp, 0 * kPointerSize));  // home object
        PushOperands(result_register(), scratch1, scratch2, result_register());
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
    PrepareForBailout(expr->expression(), BailoutState::TOS_REGISTER);
  } else {
    PrepareForBailoutForId(prop->LoadId(), BailoutState::TOS_REGISTER);
  }

  // Inline smi case if we are in a loop.
  Label stub_call, done;
  JumpPatchSite patch_site(masm_);

  int count_value = expr->op() == Token::INC ? 1 : -1;
  if (ShouldInlineSmiCase(expr->op())) {
    Label slow;
    patch_site.EmitJumpIfNotSmi(r2, &slow);

    // Save result for postfix expressions.
    if (expr->is_postfix()) {
      if (!context()->IsEffect()) {
        // Save the result on the stack. If we have a named or keyed property
        // we store the result under the receiver that is currently on top
        // of the stack.
        switch (assign_type) {
          case VARIABLE:
            __ push(r2);
            break;
          case NAMED_PROPERTY:
            __ StoreP(r2, MemOperand(sp, kPointerSize));
            break;
          case NAMED_SUPER_PROPERTY:
            __ StoreP(r2, MemOperand(sp, 2 * kPointerSize));
            break;
          case KEYED_PROPERTY:
            __ StoreP(r2, MemOperand(sp, 2 * kPointerSize));
            break;
          case KEYED_SUPER_PROPERTY:
            __ StoreP(r2, MemOperand(sp, 3 * kPointerSize));
            break;
        }
      }
    }

    Register scratch1 = r3;
    Register scratch2 = r4;
    __ LoadSmiLiteral(scratch1, Smi::FromInt(count_value));
    __ AddP(scratch2, r2, scratch1);
    __ LoadOnConditionP(nooverflow, r2, scratch2);
    __ b(nooverflow, &done);
    // Call stub. Undo operation first.
    __ b(&stub_call);
    __ bind(&slow);
  }

  // Convert old value into a number.
  __ Call(isolate()->builtins()->ToNumber(), RelocInfo::CODE_TARGET);
  RestoreContext();
  PrepareForBailoutForId(expr->ToNumberId(), BailoutState::TOS_REGISTER);

  // Save result for postfix expressions.
  if (expr->is_postfix()) {
    if (!context()->IsEffect()) {
      // Save the result on the stack. If we have a named or keyed property
      // we store the result under the receiver that is currently on top
      // of the stack.
      switch (assign_type) {
        case VARIABLE:
          PushOperand(r2);
          break;
        case NAMED_PROPERTY:
          __ StoreP(r2, MemOperand(sp, kPointerSize));
          break;
        case NAMED_SUPER_PROPERTY:
          __ StoreP(r2, MemOperand(sp, 2 * kPointerSize));
          break;
        case KEYED_PROPERTY:
          __ StoreP(r2, MemOperand(sp, 2 * kPointerSize));
          break;
        case KEYED_SUPER_PROPERTY:
          __ StoreP(r2, MemOperand(sp, 3 * kPointerSize));
          break;
      }
    }
  }

  __ bind(&stub_call);
  __ LoadRR(r3, r2);
  __ LoadSmiLiteral(r2, Smi::FromInt(count_value));

  SetExpressionPosition(expr);

  Handle<Code> code = CodeFactory::BinaryOpIC(isolate(), Token::ADD).code();
  CallIC(code, expr->CountBinOpFeedbackId());
  patch_site.EmitPatchInfo();
  __ bind(&done);

  // Store the value returned in r2.
  switch (assign_type) {
    case VARIABLE:
      if (expr->is_postfix()) {
        {
          EffectContext context(this);
          EmitVariableAssignment(expr->expression()->AsVariableProxy()->var(),
                                 Token::ASSIGN, expr->CountSlot());
          PrepareForBailoutForId(expr->AssignmentId(),
                                 BailoutState::TOS_REGISTER);
          context.Plug(r2);
        }
        // For all contexts except EffectConstant We have the result on
        // top of the stack.
        if (!context()->IsEffect()) {
          context()->PlugTOS();
        }
      } else {
        EmitVariableAssignment(expr->expression()->AsVariableProxy()->var(),
                               Token::ASSIGN, expr->CountSlot());
        PrepareForBailoutForId(expr->AssignmentId(),
                               BailoutState::TOS_REGISTER);
        context()->Plug(r2);
      }
      break;
    case NAMED_PROPERTY: {
      PopOperand(StoreDescriptor::ReceiverRegister());
      CallStoreIC(expr->CountSlot(), prop->key()->AsLiteral()->value());
      PrepareForBailoutForId(expr->AssignmentId(), BailoutState::TOS_REGISTER);
      if (expr->is_postfix()) {
        if (!context()->IsEffect()) {
          context()->PlugTOS();
        }
      } else {
        context()->Plug(r2);
      }
      break;
    }
    case NAMED_SUPER_PROPERTY: {
      EmitNamedSuperPropertyStore(prop);
      PrepareForBailoutForId(expr->AssignmentId(), BailoutState::TOS_REGISTER);
      if (expr->is_postfix()) {
        if (!context()->IsEffect()) {
          context()->PlugTOS();
        }
      } else {
        context()->Plug(r2);
      }
      break;
    }
    case KEYED_SUPER_PROPERTY: {
      EmitKeyedSuperPropertyStore(prop);
      PrepareForBailoutForId(expr->AssignmentId(), BailoutState::TOS_REGISTER);
      if (expr->is_postfix()) {
        if (!context()->IsEffect()) {
          context()->PlugTOS();
        }
      } else {
        context()->Plug(r2);
      }
      break;
    }
    case KEYED_PROPERTY: {
      PopOperands(StoreDescriptor::ReceiverRegister(),
                  StoreDescriptor::NameRegister());
      CallKeyedStoreIC(expr->CountSlot());
      PrepareForBailoutForId(expr->AssignmentId(), BailoutState::TOS_REGISTER);
      if (expr->is_postfix()) {
        if (!context()->IsEffect()) {
          context()->PlugTOS();
        }
      } else {
        context()->Plug(r2);
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
    __ JumpIfSmi(r2, if_true);
    __ LoadP(r2, FieldMemOperand(r2, HeapObject::kMapOffset));
    __ CompareRoot(r2, Heap::kHeapNumberMapRootIndex);
    Split(eq, if_true, if_false, fall_through);
  } else if (String::Equals(check, factory->string_string())) {
    __ JumpIfSmi(r2, if_false);
    __ CompareObjectType(r2, r2, r3, FIRST_NONSTRING_TYPE);
    Split(lt, if_true, if_false, fall_through);
  } else if (String::Equals(check, factory->symbol_string())) {
    __ JumpIfSmi(r2, if_false);
    __ CompareObjectType(r2, r2, r3, SYMBOL_TYPE);
    Split(eq, if_true, if_false, fall_through);
  } else if (String::Equals(check, factory->boolean_string())) {
    __ CompareRoot(r2, Heap::kTrueValueRootIndex);
    __ beq(if_true);
    __ CompareRoot(r2, Heap::kFalseValueRootIndex);
    Split(eq, if_true, if_false, fall_through);
  } else if (String::Equals(check, factory->undefined_string())) {
    __ CompareRoot(r2, Heap::kNullValueRootIndex);
    __ beq(if_false);
    __ JumpIfSmi(r2, if_false);
    // Check for undetectable objects => true.
    __ LoadP(r2, FieldMemOperand(r2, HeapObject::kMapOffset));
    __ tm(FieldMemOperand(r2, Map::kBitFieldOffset),
          Operand(1 << Map::kIsUndetectable));
    Split(ne, if_true, if_false, fall_through);

  } else if (String::Equals(check, factory->function_string())) {
    __ JumpIfSmi(r2, if_false);
    __ LoadP(r2, FieldMemOperand(r2, HeapObject::kMapOffset));
    __ LoadlB(r3, FieldMemOperand(r2, Map::kBitFieldOffset));
    __ AndP(r3, r3,
            Operand((1 << Map::kIsCallable) | (1 << Map::kIsUndetectable)));
    __ CmpP(r3, Operand(1 << Map::kIsCallable));
    Split(eq, if_true, if_false, fall_through);
  } else if (String::Equals(check, factory->object_string())) {
    __ JumpIfSmi(r2, if_false);
    __ CompareRoot(r2, Heap::kNullValueRootIndex);
    __ beq(if_true);
    STATIC_ASSERT(LAST_JS_RECEIVER_TYPE == LAST_TYPE);
    __ CompareObjectType(r2, r2, r3, FIRST_JS_RECEIVER_TYPE);
    __ blt(if_false);
    __ tm(FieldMemOperand(r2, Map::kBitFieldOffset),
          Operand((1 << Map::kIsCallable) | (1 << Map::kIsUndetectable)));
    Split(eq, if_true, if_false, fall_through);
// clang-format off
#define SIMD128_TYPE(TYPE, Type, type, lane_count, lane_type)   \
  } else if (String::Equals(check, factory->type##_string())) { \
    __ JumpIfSmi(r2, if_false);                                 \
    __ LoadP(r2, FieldMemOperand(r2, HeapObject::kMapOffset));  \
    __ CompareRoot(r2, Heap::k##Type##MapRootIndex);            \
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
      SetExpressionPosition(expr);
      EmitHasProperty();
      PrepareForBailoutBeforeSplit(expr, false, NULL, NULL);
      __ CompareRoot(r2, Heap::kTrueValueRootIndex);
      Split(eq, if_true, if_false, fall_through);
      break;

    case Token::INSTANCEOF: {
      VisitForAccumulatorValue(expr->right());
      SetExpressionPosition(expr);
      PopOperand(r3);
      InstanceOfStub stub(isolate());
      __ CallStub(&stub);
      PrepareForBailoutBeforeSplit(expr, false, NULL, NULL);
      __ CompareRoot(r2, Heap::kTrueValueRootIndex);
      Split(eq, if_true, if_false, fall_through);
      break;
    }

    default: {
      VisitForAccumulatorValue(expr->right());
      SetExpressionPosition(expr);
      Condition cond = CompareIC::ComputeCondition(op);
      PopOperand(r3);

      bool inline_smi_code = ShouldInlineSmiCase(op);
      JumpPatchSite patch_site(masm_);
      if (inline_smi_code) {
        Label slow_case;
        __ LoadRR(r4, r3);
        __ OrP(r4, r2);
        patch_site.EmitJumpIfNotSmi(r4, &slow_case);
        __ CmpP(r3, r2);
        Split(cond, if_true, if_false, NULL);
        __ bind(&slow_case);
      }

      Handle<Code> ic = CodeFactory::CompareIC(isolate(), op).code();
      CallIC(ic, expr->CompareOperationFeedbackId());
      patch_site.EmitPatchInfo();
      PrepareForBailoutBeforeSplit(expr, true, if_true, if_false);
      __ CmpP(r2, Operand::Zero());
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
    __ CompareRoot(r2, nil_value);
    Split(eq, if_true, if_false, fall_through);
  } else {
    __ JumpIfSmi(r2, if_false);
    __ LoadP(r2, FieldMemOperand(r2, HeapObject::kMapOffset));
    __ LoadlB(r3, FieldMemOperand(r2, Map::kBitFieldOffset));
    __ AndP(r0, r3, Operand(1 << Map::kIsUndetectable));
    Split(ne, if_true, if_false, fall_through);
  }
  context()->Plug(if_true, if_false);
}
Register FullCodeGenerator::result_register() { return r2; }

Register FullCodeGenerator::context_register() { return cp; }

void FullCodeGenerator::LoadFromFrameField(int frame_offset, Register value) {
  DCHECK_EQ(static_cast<int>(POINTER_SIZE_ALIGN(frame_offset)), frame_offset);
  __ LoadP(value, MemOperand(fp, frame_offset));
}

void FullCodeGenerator::StoreToFrameField(int frame_offset, Register value) {
  DCHECK_EQ(static_cast<int>(POINTER_SIZE_ALIGN(frame_offset)), frame_offset);
  __ StoreP(value, MemOperand(fp, frame_offset));
}

void FullCodeGenerator::LoadContextField(Register dst, int context_index) {
  __ LoadP(dst, ContextMemOperand(cp, context_index), r0);
}

void FullCodeGenerator::PushFunctionArgumentForContextAllocation() {
  DeclarationScope* closure_scope = scope()->GetClosureScope();
  if (closure_scope->is_script_scope() || closure_scope->is_module_scope()) {
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
  PushOperand(ip);
}

// ----------------------------------------------------------------------------
// Non-local control flow support.

void FullCodeGenerator::EnterFinallyBlock() {
  DCHECK(!result_register().is(r3));
  // Store pending message while executing finally block.
  ExternalReference pending_message_obj =
      ExternalReference::address_of_pending_message_obj(isolate());
  __ mov(ip, Operand(pending_message_obj));
  __ LoadP(r3, MemOperand(ip));
  PushOperand(r3);

  ClearPendingMessage();
}

void FullCodeGenerator::ExitFinallyBlock() {
  DCHECK(!result_register().is(r3));
  // Restore pending message from stack.
  PopOperand(r3);
  ExternalReference pending_message_obj =
      ExternalReference::address_of_pending_message_obj(isolate());
  __ mov(ip, Operand(pending_message_obj));
  __ StoreP(r3, MemOperand(ip));
}

void FullCodeGenerator::ClearPendingMessage() {
  DCHECK(!result_register().is(r3));
  ExternalReference pending_message_obj =
      ExternalReference::address_of_pending_message_obj(isolate());
  __ LoadRoot(r3, Heap::kTheHoleValueRootIndex);
  __ mov(ip, Operand(pending_message_obj));
  __ StoreP(r3, MemOperand(ip));
}

void FullCodeGenerator::DeferredCommands::EmitCommands() {
  DCHECK(!result_register().is(r3));
  // Restore the accumulator (r2) and token (r3).
  __ Pop(r3, result_register());
  for (DeferredCommand cmd : commands_) {
    Label skip;
    __ CmpSmiLiteral(r3, Smi::FromInt(cmd.token), r0);
    __ bne(&skip);
    switch (cmd.command) {
      case kReturn:
        codegen_->EmitUnwindAndReturn();
        break;
      case kThrow:
        __ Push(result_register());
        __ CallRuntime(Runtime::kReThrow);
        break;
      case kContinue:
        codegen_->EmitContinue(cmd.target);
        break;
      case kBreak:
        codegen_->EmitBreak(cmd.target);
        break;
    }
    __ bind(&skip);
  }
}

#undef __

#if V8_TARGET_ARCH_S390X
static const FourByteInstr kInterruptBranchInstruction = 0xA7A40011;
static const FourByteInstr kOSRBranchInstruction = 0xA7040011;
static const int16_t kBackEdgeBranchOffset = 0x11 * 2;
#else
static const FourByteInstr kInterruptBranchInstruction = 0xA7A4000D;
static const FourByteInstr kOSRBranchInstruction = 0xA704000D;
static const int16_t kBackEdgeBranchOffset = 0xD * 2;
#endif

void BackEdgeTable::PatchAt(Code* unoptimized_code, Address pc,
                            BackEdgeState target_state,
                            Code* replacement_code) {
  Address call_address = Assembler::target_address_from_return_address(pc);
  Address branch_address = call_address - 4;
  Isolate* isolate = unoptimized_code->GetIsolate();
  CodePatcher patcher(isolate, branch_address, 4);

  switch (target_state) {
    case INTERRUPT: {
      //  <decrement profiling counter>
      //         bge     <ok>            ;; patched to GE BRC
      //         brasrl    r14, <interrupt stub address>
      //  <reset profiling counter>
      //  ok-label
      patcher.masm()->brc(ge, Operand(kBackEdgeBranchOffset));
      break;
    }
    case ON_STACK_REPLACEMENT:
      //  <decrement profiling counter>
      //         brc   0x0, <ok>            ;;  patched to NOP BRC
      //         brasrl    r14, <interrupt stub address>
      //  <reset profiling counter>
      //  ok-label ----- pc_after points here
      patcher.masm()->brc(CC_NOP, Operand(kBackEdgeBranchOffset));
      break;
  }

  // Replace the stack check address in the mov sequence with the
  // entry address of the replacement code.
  Assembler::set_target_address_at(isolate, call_address, unoptimized_code,
                                   replacement_code->entry());

  unoptimized_code->GetHeap()->incremental_marking()->RecordCodeTargetPatch(
      unoptimized_code, call_address, replacement_code);
}

BackEdgeTable::BackEdgeState BackEdgeTable::GetBackEdgeState(
    Isolate* isolate, Code* unoptimized_code, Address pc) {
  Address call_address = Assembler::target_address_from_return_address(pc);
  Address branch_address = call_address - 4;
#ifdef DEBUG
  Address interrupt_address =
      Assembler::target_address_at(call_address, unoptimized_code);
#endif

  DCHECK(BRC == Instruction::S390OpcodeValue(branch_address));
  // For interrupt, we expect a branch greater than or equal
  // i.e. BRC 0xa, +XXXX  (0xA7A4XXXX)
  FourByteInstr br_instr = Instruction::InstructionBits(
      reinterpret_cast<const byte*>(branch_address));
  if (kInterruptBranchInstruction == br_instr) {
    DCHECK(interrupt_address == isolate->builtins()->InterruptCheck()->entry());
    return INTERRUPT;
  }

  // Expect BRC to be patched to NOP branch.
  // i.e. BRC 0x0, +XXXX (0xA704XXXX)
  USE(kOSRBranchInstruction);
  DCHECK(kOSRBranchInstruction == br_instr);

  DCHECK(interrupt_address ==
         isolate->builtins()->OnStackReplacement()->entry());
  return ON_STACK_REPLACEMENT;
}

}  // namespace internal
}  // namespace v8
#endif  // V8_TARGET_ARCH_S390
