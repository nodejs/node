// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if V8_TARGET_ARCH_MIPS

// Note on Mips implementation:
//
// The result_register() for mips is the 'v0' register, which is defined
// by the ABI to contain function return values. However, the first
// parameter to a function is defined to be 'a0'. So there are many
// places where we have to move a previous result in v0 to a0 for the
// next call: mov(a0, v0). This is not needed on the other architectures.

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

#include "src/mips/code-stubs-mips.h"
#include "src/mips/macro-assembler-mips.h"

namespace v8 {
namespace internal {

#define __ ACCESS_MASM(masm())

// A patch site is a location in the code which it is possible to patch. This
// class has a number of methods to emit the code which is patchable and the
// method EmitPatchInfo to record a marker back to the patchable code. This
// marker is a andi zero_reg, rx, #yyyy instruction, and rx * 0x0000ffff + yyyy
// (raw 16 bit immediate value is used) is the delta from the pc to the first
// instruction of the patchable code.
// The marker instruction is effectively a NOP (dest is zero_reg) and will
// never be emitted by normal code.
class JumpPatchSite BASE_EMBEDDED {
 public:
  explicit JumpPatchSite(MacroAssembler* masm) : masm_(masm) {
#ifdef DEBUG
    info_emitted_ = false;
#endif
  }

  ~JumpPatchSite() {
    DCHECK(patch_site_.is_bound() == info_emitted_);
  }

  // When initially emitting this ensure that a jump is always generated to skip
  // the inlined smi code.
  void EmitJumpIfNotSmi(Register reg, Label* target) {
    DCHECK(!patch_site_.is_bound() && !info_emitted_);
    Assembler::BlockTrampolinePoolScope block_trampoline_pool(masm_);
    __ bind(&patch_site_);
    __ andi(at, reg, 0);
    // Always taken before patched.
    __ BranchShort(target, eq, at, Operand(zero_reg));
  }

  // When initially emitting this ensure that a jump is never generated to skip
  // the inlined smi code.
  void EmitJumpIfSmi(Register reg, Label* target) {
    Assembler::BlockTrampolinePoolScope block_trampoline_pool(masm_);
    DCHECK(!patch_site_.is_bound() && !info_emitted_);
    __ bind(&patch_site_);
    __ andi(at, reg, 0);
    // Never taken before patched.
    __ BranchShort(target, ne, at, Operand(zero_reg));
  }

  void EmitPatchInfo() {
    if (patch_site_.is_bound()) {
      int delta_to_patch_site = masm_->InstructionsGeneratedSince(&patch_site_);
      Register reg = Register::from_code(delta_to_patch_site / kImm16Mask);
      __ andi(zero_reg, reg, delta_to_patch_site % kImm16Mask);
#ifdef DEBUG
      info_emitted_ = true;
#endif
    } else {
      __ nop();  // Signals no inlined code.
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
//   o a1: the JS function object being called (i.e. ourselves)
//   o a3: the new target value
//   o cp: our context
//   o fp: our caller's frame pointer
//   o sp: stack pointer
//   o ra: return address
//
// The function builds a JS frame.  Please see JavaScriptFrameConstants in
// frames-mips.h for its layout.
void FullCodeGenerator::Generate() {
  CompilationInfo* info = info_;
  profiling_counter_ = isolate()->factory()->NewCell(
      Handle<Smi>(Smi::FromInt(FLAG_interrupt_budget), isolate()));
  SetFunctionPosition(literal());
  Comment cmnt(masm_, "[ function compiled by full code generator");

  ProfileEntryHookStub::MaybeCallEntryHook(masm_);

  if (FLAG_debug_code && info->ExpectsJSReceiverAsReceiver()) {
    int receiver_offset = info->scope()->num_parameters() * kPointerSize;
    __ lw(a2, MemOperand(sp, receiver_offset));
    __ AssertNotSmi(a2);
    __ GetObjectType(a2, a2, a2);
    __ Check(ge, kSloppyFunctionExpectsJSReceiverReceiver, a2,
             Operand(FIRST_JS_RECEIVER_TYPE));
  }

  // Open a frame scope to indicate that there is a frame on the stack.  The
  // MANUAL indicates that the scope shouldn't actually generate code to set up
  // the frame (that is done below).
  FrameScope frame_scope(masm_, StackFrame::MANUAL);

  info->set_prologue_offset(masm_->pc_offset());
  __ Prologue(info->GeneratePreagedPrologue());

  // Increment invocation count for the function.
  {
    Comment cmnt(masm_, "[ Increment invocation count");
    __ lw(a0, FieldMemOperand(a1, JSFunction::kLiteralsOffset));
    __ lw(a0, FieldMemOperand(a0, LiteralsArray::kFeedbackVectorOffset));
    __ lw(t0, FieldMemOperand(
                  a0, TypeFeedbackVector::kInvocationCountIndex * kPointerSize +
                          TypeFeedbackVector::kHeaderSize));
    __ Addu(t0, t0, Operand(Smi::FromInt(1)));
    __ sw(t0, FieldMemOperand(
                  a0, TypeFeedbackVector::kInvocationCountIndex * kPointerSize +
                          TypeFeedbackVector::kHeaderSize));
  }

  { Comment cmnt(masm_, "[ Allocate locals");
    int locals_count = info->scope()->num_stack_slots();
    // Generators allocate locals, if any, in context slots.
    DCHECK(!IsGeneratorFunction(info->literal()->kind()) || locals_count == 0);
    OperandStackDepthIncrement(locals_count);
    if (locals_count > 0) {
      if (locals_count >= 128) {
        Label ok;
        __ Subu(t5, sp, Operand(locals_count * kPointerSize));
        __ LoadRoot(a2, Heap::kRealStackLimitRootIndex);
        __ Branch(&ok, hs, t5, Operand(a2));
        __ CallRuntime(Runtime::kThrowStackOverflow);
        __ bind(&ok);
      }
      __ LoadRoot(t5, Heap::kUndefinedValueRootIndex);
      int kMaxPushes = FLAG_optimize_for_size ? 4 : 32;
      if (locals_count >= kMaxPushes) {
        int loop_iterations = locals_count / kMaxPushes;
        __ li(a2, Operand(loop_iterations));
        Label loop_header;
        __ bind(&loop_header);
        // Do pushes.
        __ Subu(sp, sp, Operand(kMaxPushes * kPointerSize));
        for (int i = 0; i < kMaxPushes; i++) {
          __ sw(t5, MemOperand(sp, i * kPointerSize));
        }
        // Continue loop if not done.
        __ Subu(a2, a2, Operand(1));
        __ Branch(&loop_header, ne, a2, Operand(zero_reg));
      }
      int remaining = locals_count % kMaxPushes;
      // Emit the remaining pushes.
      __ Subu(sp, sp, Operand(remaining * kPointerSize));
      for (int i  = 0; i < remaining; i++) {
        __ sw(t5, MemOperand(sp, i * kPointerSize));
      }
    }
  }

  bool function_in_register_a1 = true;

  // Possibly allocate a local context.
  if (info->scope()->NeedsContext()) {
    Comment cmnt(masm_, "[ Allocate context");
    // Argument to NewContext is the function, which is still in a1.
    bool need_write_barrier = true;
    int slots = info->scope()->num_heap_slots() - Context::MIN_CONTEXT_SLOTS;
    if (info->scope()->is_script_scope()) {
      __ push(a1);
      __ Push(info->scope()->scope_info());
      __ CallRuntime(Runtime::kNewScriptContext);
      PrepareForBailoutForId(BailoutId::ScriptContext(),
                             BailoutState::TOS_REGISTER);
      // The new target value is not used, clobbering is safe.
      DCHECK_NULL(info->scope()->new_target_var());
    } else {
      if (info->scope()->new_target_var() != nullptr) {
        __ push(a3);  // Preserve new target.
      }
      if (slots <= FastNewFunctionContextStub::kMaximumSlots) {
        FastNewFunctionContextStub stub(isolate());
        __ li(FastNewFunctionContextDescriptor::SlotsRegister(),
              Operand(slots));
        __ CallStub(&stub);
        // Result of FastNewFunctionContextStub is always in new space.
        need_write_barrier = false;
      } else {
        __ push(a1);
        __ CallRuntime(Runtime::kNewFunctionContext);
      }
      if (info->scope()->new_target_var() != nullptr) {
        __ pop(a3);  // Restore new target.
      }
    }
    function_in_register_a1 = false;
    // Context is returned in v0. It replaces the context passed to us.
    // It's saved in the stack and kept live in cp.
    __ mov(cp, v0);
    __ sw(v0, MemOperand(fp, StandardFrameConstants::kContextOffset));
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
        __ lw(a0, MemOperand(fp, parameter_offset));
        // Store it in the context.
        MemOperand target = ContextMemOperand(cp, var->index());
        __ sw(a0, target);

        // Update the write barrier.
        if (need_write_barrier) {
          __ RecordWriteContextSlot(cp, target.offset(), a0, a2,
                                    kRAHasBeenSaved, kDontSaveFPRegs);
        } else if (FLAG_debug_code) {
          Label done;
          __ JumpIfInNewSpace(cp, a0, &done);
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
    if (!function_in_register_a1) {
      __ lw(a1, MemOperand(fp, JavaScriptFrameConstants::kFunctionOffset));
      // The write barrier clobbers register again, keep it marked as such.
    }
    SetVar(this_function_var, a1, a0, a2);
  }

  // Possibly set up a local binding to the new target value.
  Variable* new_target_var = info->scope()->new_target_var();
  if (new_target_var != nullptr) {
    Comment cmnt(masm_, "[ new.target");
    SetVar(new_target_var, a3, a0, a2);
  }

  // Possibly allocate RestParameters
  Variable* rest_param = info->scope()->rest_parameter();
  if (rest_param != nullptr) {
    Comment cmnt(masm_, "[ Allocate rest parameter array");
    if (!function_in_register_a1) {
      __ lw(a1, MemOperand(fp, JavaScriptFrameConstants::kFunctionOffset));
    }
    FastNewRestParameterStub stub(isolate());
    __ CallStub(&stub);
    function_in_register_a1 = false;
    SetVar(rest_param, v0, a1, a2);
  }

  Variable* arguments = info->scope()->arguments();
  if (arguments != NULL) {
    // Function uses arguments object.
    Comment cmnt(masm_, "[ Allocate arguments object");
    if (!function_in_register_a1) {
      // Load this again, if it's used by the local context below.
      __ lw(a1, MemOperand(fp, JavaScriptFrameConstants::kFunctionOffset));
    }
    if (is_strict(language_mode()) || !has_simple_parameters()) {
      FastNewStrictArgumentsStub stub(isolate());
      __ CallStub(&stub);
    } else if (literal()->has_duplicate_parameters()) {
      __ Push(a1);
      __ CallRuntime(Runtime::kNewSloppyArguments_Generic);
    } else {
      FastNewSloppyArgumentsStub stub(isolate());
      __ CallStub(&stub);
    }

    SetVar(arguments, v0, a1, a2);
  }

  if (FLAG_trace) {
    __ CallRuntime(Runtime::kTraceEnter);
  }

  // Visit the declarations and body unless there is an illegal
  // redeclaration.
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
    __ LoadRoot(at, Heap::kStackLimitRootIndex);
    __ Branch(&ok, hs, sp, Operand(at));
    Handle<Code> stack_check = isolate()->builtins()->StackCheck();
    PredictableCodeSizeScope predictable(
        masm_, masm_->CallSize(stack_check, RelocInfo::CODE_TARGET));
    __ Call(stack_check, RelocInfo::CODE_TARGET);
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
  { Comment cmnt(masm_, "[ return <undefined>;");
    __ LoadRoot(v0, Heap::kUndefinedValueRootIndex);
  }
  EmitReturnSequence();
}


void FullCodeGenerator::ClearAccumulator() {
  DCHECK(Smi::FromInt(0) == 0);
  __ mov(v0, zero_reg);
}


void FullCodeGenerator::EmitProfilingCounterDecrement(int delta) {
  __ li(a2, Operand(profiling_counter_));
  __ lw(a3, FieldMemOperand(a2, Cell::kValueOffset));
  __ Subu(a3, a3, Operand(Smi::FromInt(delta)));
  __ sw(a3, FieldMemOperand(a2, Cell::kValueOffset));
}


void FullCodeGenerator::EmitProfilingCounterReset() {
  int reset_value = FLAG_interrupt_budget;
  if (info_->is_debug()) {
    // Detect debug break requests as soon as possible.
    reset_value = FLAG_interrupt_budget >> 4;
  }
  __ li(a2, Operand(profiling_counter_));
  __ li(a3, Operand(Smi::FromInt(reset_value)));
  __ sw(a3, FieldMemOperand(a2, Cell::kValueOffset));
}


void FullCodeGenerator::EmitBackEdgeBookkeeping(IterationStatement* stmt,
                                                Label* back_edge_target) {
  // The generated code is used in Deoptimizer::PatchStackCheckCodeAt so we need
  // to make sure it is constant. Branch may emit a skip-or-jump sequence
  // instead of the normal Branch. It seems that the "skip" part of that
  // sequence is about as long as this Branch would be so it is safe to ignore
  // that.
  Assembler::BlockTrampolinePoolScope block_trampoline_pool(masm_);
  Comment cmnt(masm_, "[ Back edge bookkeeping");
  Label ok;
  DCHECK(back_edge_target->is_bound());
  int distance = masm_->SizeOfCodeGeneratedSince(back_edge_target);
  int weight = Min(kMaxBackEdgeWeight,
                   Max(1, distance / kCodeSizeMultiplier));
  EmitProfilingCounterDecrement(weight);
  __ slt(at, a3, zero_reg);
  __ beq(at, zero_reg, &ok);
  // Call will emit a li t9 first, so it is safe to use the delay slot.
  __ Call(isolate()->builtins()->InterruptCheck(), RelocInfo::CODE_TARGET);
  // Record a mapping of this PC offset to the OSR id.  This is used to find
  // the AST id from the unoptimized code in order to use it as a key into
  // the deoptimization input data found in the optimized code.
  RecordBackEdge(stmt->OsrEntryId());
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
    int distance = masm_->pc_offset();
    weight = Min(kMaxBackEdgeWeight, Max(1, distance / kCodeSizeMultiplier));
  }
  EmitProfilingCounterDecrement(weight);
  Label ok;
  __ Branch(&ok, ge, a3, Operand(zero_reg));
  // Don't need to save result register if we are going to do a tail call.
  if (!is_tail_call) {
    __ push(v0);
  }
  __ Call(isolate()->builtins()->InterruptCheck(), RelocInfo::CODE_TARGET);
  if (!is_tail_call) {
    __ pop(v0);
  }
  EmitProfilingCounterReset();
  __ bind(&ok);
}

void FullCodeGenerator::EmitReturnSequence() {
  Comment cmnt(masm_, "[ Return sequence");
  if (return_label_.is_bound()) {
    __ Branch(&return_label_);
  } else {
    __ bind(&return_label_);
    if (FLAG_trace) {
      // Push the return value on the stack as the parameter.
      // Runtime::TraceExit returns its parameter in v0.
      __ push(v0);
      __ CallRuntime(Runtime::kTraceExit);
    }
    EmitProfilingCounterHandlingForReturnSequence(false);

    // Make sure that the constant pool is not emitted inside of the return
    // sequence.
    { Assembler::BlockTrampolinePoolScope block_trampoline_pool(masm_);
      int32_t arg_count = info_->scope()->num_parameters() + 1;
      int32_t sp_delta = arg_count * kPointerSize;
      SetReturnPosition(literal());
      __ mov(sp, fp);
      __ MultiPop(static_cast<RegList>(fp.bit() | ra.bit()));
      __ Addu(sp, sp, Operand(sp_delta));
      __ Jump(ra);
    }
  }
}

void FullCodeGenerator::RestoreContext() {
  __ lw(cp, MemOperand(fp, StandardFrameConstants::kContextOffset));
}

void FullCodeGenerator::StackValueContext::Plug(Variable* var) const {
  DCHECK(var->IsStackAllocated() || var->IsContextSlot());
  codegen()->GetVar(result_register(), var);
  codegen()->PushOperand(result_register());
}


void FullCodeGenerator::EffectContext::Plug(Heap::RootListIndex index) const {
}


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
  codegen()->PrepareForBailoutBeforeSplit(condition(),
                                          true,
                                          true_label_,
                                          false_label_);
  if (index == Heap::kUndefinedValueRootIndex ||
      index == Heap::kNullValueRootIndex ||
      index == Heap::kFalseValueRootIndex) {
    if (false_label_ != fall_through_) __ Branch(false_label_);
  } else if (index == Heap::kTrueValueRootIndex) {
    if (true_label_ != fall_through_) __ Branch(true_label_);
  } else {
    __ LoadRoot(result_register(), index);
    codegen()->DoTest(this);
  }
}


void FullCodeGenerator::EffectContext::Plug(Handle<Object> lit) const {
}


void FullCodeGenerator::AccumulatorValueContext::Plug(
    Handle<Object> lit) const {
  __ li(result_register(), Operand(lit));
}


void FullCodeGenerator::StackValueContext::Plug(Handle<Object> lit) const {
  // Immediates cannot be pushed directly.
  __ li(result_register(), Operand(lit));
  codegen()->PushOperand(result_register());
}


void FullCodeGenerator::TestContext::Plug(Handle<Object> lit) const {
  codegen()->PrepareForBailoutBeforeSplit(condition(),
                                          true,
                                          true_label_,
                                          false_label_);
  DCHECK(lit->IsNull(isolate()) || lit->IsUndefined(isolate()) ||
         !lit->IsUndetectable());
  if (lit->IsUndefined(isolate()) || lit->IsNull(isolate()) ||
      lit->IsFalse(isolate())) {
    if (false_label_ != fall_through_) __ Branch(false_label_);
  } else if (lit->IsTrue(isolate()) || lit->IsJSObject()) {
    if (true_label_ != fall_through_) __ Branch(true_label_);
  } else if (lit->IsString()) {
    if (String::cast(*lit)->length() == 0) {
      if (false_label_ != fall_through_) __ Branch(false_label_);
    } else {
      if (true_label_ != fall_through_) __ Branch(true_label_);
    }
  } else if (lit->IsSmi()) {
    if (Smi::cast(*lit)->value() == 0) {
      if (false_label_ != fall_through_) __ Branch(false_label_);
    } else {
      if (true_label_ != fall_through_) __ Branch(true_label_);
    }
  } else {
    // For simplicity we always test the accumulator register.
    __ li(result_register(), Operand(lit));
    codegen()->DoTest(this);
  }
}


void FullCodeGenerator::StackValueContext::DropAndPlug(int count,
                                                       Register reg) const {
  DCHECK(count > 0);
  if (count > 1) codegen()->DropOperands(count - 1);
  __ sw(reg, MemOperand(sp, 0));
}


void FullCodeGenerator::EffectContext::Plug(Label* materialize_true,
                                            Label* materialize_false) const {
  DCHECK(materialize_true == materialize_false);
  __ bind(materialize_true);
}


void FullCodeGenerator::AccumulatorValueContext::Plug(
    Label* materialize_true,
    Label* materialize_false) const {
  Label done;
  __ bind(materialize_true);
  __ LoadRoot(result_register(), Heap::kTrueValueRootIndex);
  __ Branch(&done);
  __ bind(materialize_false);
  __ LoadRoot(result_register(), Heap::kFalseValueRootIndex);
  __ bind(&done);
}


void FullCodeGenerator::StackValueContext::Plug(
    Label* materialize_true,
    Label* materialize_false) const {
  codegen()->OperandStackDepthIncrement(1);
  Label done;
  __ bind(materialize_true);
  __ LoadRoot(at, Heap::kTrueValueRootIndex);
  // Push the value as the following branch can clobber at in long branch mode.
  __ push(at);
  __ Branch(&done);
  __ bind(materialize_false);
  __ LoadRoot(at, Heap::kFalseValueRootIndex);
  __ push(at);
  __ bind(&done);
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
  __ LoadRoot(at, value_root_index);
  codegen()->PushOperand(at);
}


void FullCodeGenerator::TestContext::Plug(bool flag) const {
  codegen()->PrepareForBailoutBeforeSplit(condition(),
                                          true,
                                          true_label_,
                                          false_label_);
  if (flag) {
    if (true_label_ != fall_through_) __ Branch(true_label_);
  } else {
    if (false_label_ != fall_through_) __ Branch(false_label_);
  }
}


void FullCodeGenerator::DoTest(Expression* condition,
                               Label* if_true,
                               Label* if_false,
                               Label* fall_through) {
  __ mov(a0, result_register());
  Handle<Code> ic = ToBooleanICStub::GetUninitialized(isolate());
  CallIC(ic, condition->test_id());
  __ LoadRoot(at, Heap::kTrueValueRootIndex);
  Split(eq, result_register(), Operand(at), if_true, if_false, fall_through);
}


void FullCodeGenerator::Split(Condition cc,
                              Register lhs,
                              const Operand&  rhs,
                              Label* if_true,
                              Label* if_false,
                              Label* fall_through) {
  if (if_false == fall_through) {
    __ Branch(if_true, cc, lhs, rhs);
  } else if (if_true == fall_through) {
    __ Branch(if_false, NegateCondition(cc), lhs, rhs);
  } else {
    __ Branch(if_true, cc, lhs, rhs);
    __ Branch(if_false);
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
  __ lw(dest, location);
}


void FullCodeGenerator::SetVar(Variable* var,
                               Register src,
                               Register scratch0,
                               Register scratch1) {
  DCHECK(var->IsContextSlot() || var->IsStackAllocated());
  DCHECK(!scratch0.is(src));
  DCHECK(!scratch0.is(scratch1));
  DCHECK(!scratch1.is(src));
  MemOperand location = VarOperand(var, scratch0);
  __ sw(src, location);
  // Emit the write barrier code if the location is in the heap.
  if (var->IsContextSlot()) {
    __ RecordWriteContextSlot(scratch0,
                              location.offset(),
                              src,
                              scratch1,
                              kRAHasBeenSaved,
                              kDontSaveFPRegs);
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
  if (should_normalize) __ Branch(&skip);
  PrepareForBailout(expr, BailoutState::TOS_REGISTER);
  if (should_normalize) {
    __ LoadRoot(t0, Heap::kTrueValueRootIndex);
    Split(eq, v0, Operand(t0), if_true, if_false, NULL);
    __ bind(&skip);
  }
}


void FullCodeGenerator::EmitDebugCheckDeclarationContext(Variable* variable) {
  // The variable in the declaration always resides in the current function
  // context.
  DCHECK_EQ(0, scope()->ContextChainLength(variable->scope()));
  if (FLAG_debug_code) {
    // Check that we're not inside a with or catch context.
    __ lw(a1, FieldMemOperand(cp, HeapObject::kMapOffset));
    __ LoadRoot(t0, Heap::kWithContextMapRootIndex);
    __ Check(ne, kDeclarationInWithContext,
        a1, Operand(t0));
    __ LoadRoot(t0, Heap::kCatchContextMapRootIndex);
    __ Check(ne, kDeclarationInCatchContext,
        a1, Operand(t0));
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
        __ LoadRoot(t0, Heap::kTheHoleValueRootIndex);
        __ sw(t0, StackOperand(variable));
      }
      break;

    case VariableLocation::CONTEXT:
      if (variable->binding_needs_init()) {
        Comment cmnt(masm_, "[ VariableDeclaration");
        EmitDebugCheckDeclarationContext(variable);
          __ LoadRoot(at, Heap::kTheHoleValueRootIndex);
          __ sw(at, ContextMemOperand(cp, variable->index()));
          // No write barrier since the_hole_value is in old space.
          PrepareForBailoutForId(proxy->id(), BailoutState::NO_REGISTERS);
      }
      break;

    case VariableLocation::LOOKUP: {
      Comment cmnt(masm_, "[ VariableDeclaration");
      DCHECK_EQ(VAR, variable->mode());
      DCHECK(!variable->binding_needs_init());
      __ li(a2, Operand(variable->name()));
      __ Push(a2);
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
      __ sw(result_register(), StackOperand(variable));
      break;
    }

    case VariableLocation::CONTEXT: {
      Comment cmnt(masm_, "[ FunctionDeclaration");
      EmitDebugCheckDeclarationContext(variable);
      VisitForAccumulatorValue(declaration->fun());
      __ sw(result_register(), ContextMemOperand(cp, variable->index()));
      int offset = Context::SlotOffset(variable->index());
      // We know that we have written a function, which is not a smi.
      __ RecordWriteContextSlot(cp,
                                offset,
                                result_register(),
                                a2,
                                kRAHasBeenSaved,
                                kDontSaveFPRegs,
                                EMIT_REMEMBERED_SET,
                                OMIT_SMI_CHECK);
      PrepareForBailoutForId(proxy->id(), BailoutState::NO_REGISTERS);
      break;
    }

    case VariableLocation::LOOKUP: {
      Comment cmnt(masm_, "[ FunctionDeclaration");
      __ li(a2, Operand(variable->name()));
      PushOperand(a2);
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
  __ li(a1, Operand(pairs));
  __ li(a0, Operand(Smi::FromInt(DeclareGlobalsFlags())));
  __ EmitLoadTypeFeedbackVector(a2);
  __ Push(a1, a0, a2);
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
    __ mov(a0, result_register());  // CompareStub requires args in a0, a1.

    // Perform the comparison as if via '==='.
    __ lw(a1, MemOperand(sp, 0));  // Switch value.
    bool inline_smi_code = ShouldInlineSmiCase(Token::EQ_STRICT);
    JumpPatchSite patch_site(masm_);
    if (inline_smi_code) {
      Label slow_case;
      __ or_(a2, a1, a0);
      patch_site.EmitJumpIfNotSmi(a2, &slow_case);

      __ Branch(&next_test, ne, a1, Operand(a0));
      __ Drop(1);  // Switch value is no longer needed.
      __ Branch(clause->body_target());

      __ bind(&slow_case);
    }

    // Record position before stub call for type feedback.
    SetExpressionPosition(clause);
    Handle<Code> ic =
        CodeFactory::CompareIC(isolate(), Token::EQ_STRICT).code();
    CallIC(ic, clause->CompareId());
    patch_site.EmitPatchInfo();

    Label skip;
    __ Branch(&skip);
    PrepareForBailout(clause, BailoutState::TOS_REGISTER);
    __ LoadRoot(at, Heap::kTrueValueRootIndex);
    __ Branch(&next_test, ne, v0, Operand(at));
    __ Drop(1);
    __ Branch(clause->body_target());
    __ bind(&skip);

    __ Branch(&next_test, ne, v0, Operand(zero_reg));
    __ Drop(1);  // Switch value is no longer needed.
    __ Branch(clause->body_target());
  }

  // Discard the test value and jump to the default if present, otherwise to
  // the end of the statement.
  __ bind(&next_test);
  DropOperands(1);  // Switch value is no longer needed.
  if (default_clause == NULL) {
    __ Branch(nested_statement.break_label());
  } else {
    __ Branch(default_clause->body_target());
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
  __ mov(a0, result_register());
  OperandStackDepthIncrement(5);

  Label loop, exit;
  Iteration loop_statement(this, stmt);
  increment_loop_depth();

  // If the object is null or undefined, skip over the loop, otherwise convert
  // it to a JS receiver.  See ECMA-262 version 5, section 12.6.4.
  Label convert, done_convert;
  __ JumpIfSmi(a0, &convert);
  __ GetObjectType(a0, a1, a1);
  __ Branch(USE_DELAY_SLOT, &done_convert, ge, a1,
            Operand(FIRST_JS_RECEIVER_TYPE));
  __ LoadRoot(at, Heap::kNullValueRootIndex);  // In delay slot.
  __ Branch(USE_DELAY_SLOT, &exit, eq, a0, Operand(at));
  __ LoadRoot(at, Heap::kUndefinedValueRootIndex);  // In delay slot.
  __ Branch(&exit, eq, a0, Operand(at));
  __ bind(&convert);
  ToObjectStub stub(isolate());
  __ CallStub(&stub);
  RestoreContext();
  __ mov(a0, v0);
  __ bind(&done_convert);
  PrepareForBailoutForId(stmt->ToObjectId(), BailoutState::TOS_REGISTER);
  __ push(a0);

  // Check cache validity in generated code. If we cannot guarantee cache
  // validity, call the runtime system to check cache validity or get the
  // property names in a fixed array. Note: Proxies never have an enum cache,
  // so will always take the slow path.
  Label call_runtime;
  __ CheckEnumCache(&call_runtime);

  // The enum cache is valid.  Load the map of the object being
  // iterated over and use the cache for the iteration.
  Label use_cache;
  __ lw(v0, FieldMemOperand(a0, HeapObject::kMapOffset));
  __ Branch(&use_cache);

  // Get the set of properties to enumerate.
  __ bind(&call_runtime);
  __ push(a0);  // Duplicate the enumerable object on the stack.
  __ CallRuntime(Runtime::kForInEnumerate);
  PrepareForBailoutForId(stmt->EnumId(), BailoutState::TOS_REGISTER);

  // If we got a map from the runtime call, we can do a fast
  // modification check. Otherwise, we got a fixed array, and we have
  // to do a slow check.
  Label fixed_array;
  __ lw(a2, FieldMemOperand(v0, HeapObject::kMapOffset));
  __ LoadRoot(at, Heap::kMetaMapRootIndex);
  __ Branch(&fixed_array, ne, a2, Operand(at));

  // We got a map in register v0. Get the enumeration cache from it.
  Label no_descriptors;
  __ bind(&use_cache);

  __ EnumLength(a1, v0);
  __ Branch(&no_descriptors, eq, a1, Operand(Smi::FromInt(0)));

  __ LoadInstanceDescriptors(v0, a2);
  __ lw(a2, FieldMemOperand(a2, DescriptorArray::kEnumCacheOffset));
  __ lw(a2, FieldMemOperand(a2, DescriptorArray::kEnumCacheBridgeCacheOffset));

  // Set up the four remaining stack slots.
  __ li(a0, Operand(Smi::FromInt(0)));
  // Push map, enumeration cache, enumeration cache length (as smi) and zero.
  __ Push(v0, a2, a1, a0);
  __ jmp(&loop);

  __ bind(&no_descriptors);
  __ Drop(1);
  __ jmp(&exit);

  // We got a fixed array in register v0. Iterate through that.
  __ bind(&fixed_array);

  __ li(a1, Operand(Smi::FromInt(1)));  // Smi(1) indicates slow check
  __ Push(a1, v0);  // Smi and array
  __ lw(a1, FieldMemOperand(v0, FixedArray::kLengthOffset));
  __ Push(a1);  // Fixed array length (as smi).
  PrepareForBailoutForId(stmt->PrepareId(), BailoutState::NO_REGISTERS);
  __ li(a0, Operand(Smi::FromInt(0)));
  __ Push(a0);  // Initial index.

  // Generate code for doing the condition check.
  __ bind(&loop);
  SetExpressionAsStatementPosition(stmt->each());

  // Load the current count to a0, load the length to a1.
  __ lw(a0, MemOperand(sp, 0 * kPointerSize));
  __ lw(a1, MemOperand(sp, 1 * kPointerSize));
  __ Branch(loop_statement.break_label(), hs, a0, Operand(a1));

  // Get the current entry of the array into result_register.
  __ lw(a2, MemOperand(sp, 2 * kPointerSize));
  __ Addu(a2, a2, Operand(FixedArray::kHeaderSize - kHeapObjectTag));
  __ Lsa(t0, a2, a0, kPointerSizeLog2 - kSmiTagSize);
  __ lw(result_register(), MemOperand(t0));  // Current entry.

  // Get the expected map from the stack or a smi in the
  // permanent slow case into register a2.
  __ lw(a2, MemOperand(sp, 3 * kPointerSize));

  // Check if the expected map still matches that of the enumerable.
  // If not, we may have to filter the key.
  Label update_each;
  __ lw(a1, MemOperand(sp, 4 * kPointerSize));
  __ lw(t0, FieldMemOperand(a1, HeapObject::kMapOffset));
  __ Branch(&update_each, eq, t0, Operand(a2));

  // We need to filter the key, record slow-path here.
  int const vector_index = SmiFromSlot(slot)->value();
  __ EmitLoadTypeFeedbackVector(a3);
  __ li(a2, Operand(TypeFeedbackVector::MegamorphicSentinel(isolate())));
  __ sw(a2, FieldMemOperand(a3, FixedArray::OffsetOfElementAt(vector_index)));

  __ mov(a0, result_register());
  // a0 contains the key. The receiver in a1 is the second argument to the
  // ForInFilterStub. ForInFilter returns undefined if the receiver doesn't
  // have the key or returns the name-converted key.
  ForInFilterStub filter_stub(isolate());
  __ CallStub(&filter_stub);
  RestoreContext();
  PrepareForBailoutForId(stmt->FilterId(), BailoutState::TOS_REGISTER);
  __ LoadRoot(at, Heap::kUndefinedValueRootIndex);
  __ Branch(loop_statement.continue_label(), eq, result_register(),
            Operand(at));

  // Update the 'each' property or variable from the possibly filtered
  // entry in the result_register.
  __ bind(&update_each);
  // Perform the assignment as if via '='.
  { EffectContext context(this);
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
  __ pop(a0);
  __ Addu(a0, a0, Operand(Smi::FromInt(1)));
  __ push(a0);

  EmitBackEdgeBookkeeping(stmt, &loop);
  __ Branch(&loop);

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
  __ lw(StoreDescriptor::ReceiverRegister(), MemOperand(sp));
  __ lw(StoreDescriptor::ValueRegister(),
        MemOperand(sp, offset * kPointerSize));
  CallStoreIC(slot, isolate()->factory()->home_object_symbol());
}


void FullCodeGenerator::EmitSetHomeObjectAccumulator(Expression* initializer,
                                                     int offset,
                                                     FeedbackVectorSlot slot) {
  DCHECK(NeedsHomeObject(initializer));
  __ Move(StoreDescriptor::ReceiverRegister(), v0);
  __ lw(StoreDescriptor::ValueRegister(),
        MemOperand(sp, offset * kPointerSize));
  CallStoreIC(slot, isolate()->factory()->home_object_symbol());
}


void FullCodeGenerator::EmitLoadGlobalCheckExtensions(VariableProxy* proxy,
                                                      TypeofMode typeof_mode,
                                                      Label* slow) {
  Register current = cp;
  Register next = a1;
  Register temp = a2;

  int to_check = scope()->ContextChainLengthUntilOutermostSloppyEval();
  for (Scope* s = scope(); to_check > 0; s = s->outer_scope()) {
    if (!s->NeedsContext()) continue;
    if (s->calls_sloppy_eval()) {
      // Check that extension is "the hole".
      __ lw(temp, ContextMemOperand(current, Context::EXTENSION_INDEX));
      __ JumpIfNotRoot(temp, Heap::kTheHoleValueRootIndex, slow);
    }
    // Load next context in chain.
    __ lw(next, ContextMemOperand(current, Context::PREVIOUS_INDEX));
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
  Register next = a3;
  Register temp = t0;

  for (Scope* s = scope(); s != var->scope(); s = s->outer_scope()) {
    if (s->NeedsContext()) {
      if (s->calls_sloppy_eval()) {
        // Check that extension is "the hole".
        __ lw(temp, ContextMemOperand(context, Context::EXTENSION_INDEX));
        __ JumpIfNotRoot(temp, Heap::kTheHoleValueRootIndex, slow);
      }
      __ lw(next, ContextMemOperand(context, Context::PREVIOUS_INDEX));
      // Walk the rest of the chain without clobbering cp.
      context = next;
    }
  }
  // Check that last extension is "the hole".
  __ lw(temp, ContextMemOperand(context, Context::EXTENSION_INDEX));
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
    __ Branch(done);
  } else if (var->mode() == DYNAMIC_LOCAL) {
    Variable* local = var->local_if_not_shadowed();
    __ lw(v0, ContextSlotOperandCheckExtensions(local, slow));
    if (local->binding_needs_init()) {
      __ LoadRoot(at, Heap::kTheHoleValueRootIndex);
      __ subu(at, v0, at);  // Sub as compare: at == 0 on eq.
      __ Branch(done, ne, at, Operand(zero_reg));
      __ li(a0, Operand(var->name()));
      __ push(a0);
      __ CallRuntime(Runtime::kThrowReferenceError);
    } else {
      __ Branch(done);
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
      context()->Plug(v0);
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
        GetVar(v0, var);
        __ LoadRoot(at, Heap::kTheHoleValueRootIndex);
        __ subu(at, v0, at);  // Sub as compare: at == 0 on eq.
        __ Branch(&done, ne, at, Operand(zero_reg));
        __ li(a0, Operand(var->name()));
        __ push(a0);
        __ CallRuntime(Runtime::kThrowReferenceError);
        __ bind(&done);
        context()->Plug(v0);
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
      context()->Plug(v0);
      break;
    }

    case VariableLocation::MODULE:
      UNREACHABLE();
  }
}


void FullCodeGenerator::EmitAccessor(ObjectLiteralProperty* property) {
  Expression* expression = (property == NULL) ? NULL : property->value();
  if (expression == NULL) {
    __ LoadRoot(a1, Heap::kNullValueRootIndex);
    PushOperand(a1);
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
  __ lw(a3, MemOperand(fp, JavaScriptFrameConstants::kFunctionOffset));
  __ li(a2, Operand(Smi::FromInt(expr->literal_index())));
  __ li(a1, Operand(constant_properties));
  __ li(a0, Operand(Smi::FromInt(expr->ComputeFlags())));
  if (MustCreateObjectLiteralWithRuntime(expr)) {
    __ Push(a3, a2, a1, a0);
    __ CallRuntime(Runtime::kCreateObjectLiteral);
  } else {
    FastCloneShallowObjectStub stub(isolate(), expr->properties_count());
    __ CallStub(&stub);
    RestoreContext();
  }
  PrepareForBailoutForId(expr->CreateLiteralId(), BailoutState::TOS_REGISTER);

  // If result_saved is true the result is on top of the stack.  If
  // result_saved is false the result is in v0.
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
      PushOperand(v0);  // Save result on stack.
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
            __ mov(StoreDescriptor::ValueRegister(), result_register());
            DCHECK(StoreDescriptor::ValueRegister().is(a0));
            __ lw(StoreDescriptor::ReceiverRegister(), MemOperand(sp));
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
        __ lw(a0, MemOperand(sp));
        PushOperand(a0);
        VisitForStackValue(key);
        VisitForStackValue(value);
        if (property->emit_store()) {
          if (NeedsHomeObject(value)) {
            EmitSetHomeObject(value, 2, property->GetSlot());
          }
          __ li(a0, Operand(Smi::FromInt(SLOPPY)));  // PropertyAttributes.
          PushOperand(a0);
          CallRuntimeWithOperands(Runtime::kSetProperty);
        } else {
          DropOperands(3);
        }
        break;
      case ObjectLiteral::Property::PROTOTYPE:
        // Duplicate receiver on stack.
        __ lw(a0, MemOperand(sp));
        PushOperand(a0);
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
       it != accessor_table.end();
       ++it) {
    __ lw(a0, MemOperand(sp));  // Duplicate receiver.
    PushOperand(a0);
    VisitForStackValue(it->first);
    EmitAccessor(it->second->getter);
    EmitAccessor(it->second->setter);
    __ li(a0, Operand(Smi::FromInt(NONE)));
    PushOperand(a0);
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
      PushOperand(v0);  // Save result on the stack
      result_saved = true;
    }

    __ lw(a0, MemOperand(sp));  // Duplicate receiver.
    PushOperand(a0);

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
    context()->Plug(v0);
  }
}


void FullCodeGenerator::VisitArrayLiteral(ArrayLiteral* expr) {
  Comment cmnt(masm_, "[ ArrayLiteral");

  Handle<FixedArray> constant_elements = expr->constant_elements();
  bool has_fast_elements =
      IsFastObjectElementsKind(expr->constant_elements_kind());

  AllocationSiteMode allocation_site_mode = TRACK_ALLOCATION_SITE;
  if (has_fast_elements && !FLAG_allocation_site_pretenuring) {
    // If the only customer of allocation sites is transitioning, then
    // we can turn it off if we don't have anywhere else to transition to.
    allocation_site_mode = DONT_TRACK_ALLOCATION_SITE;
  }

  __ mov(a0, result_register());
  __ lw(a3, MemOperand(fp, JavaScriptFrameConstants::kFunctionOffset));
  __ li(a2, Operand(Smi::FromInt(expr->literal_index())));
  __ li(a1, Operand(constant_elements));
  if (MustCreateArrayLiteralWithRuntime(expr)) {
    __ li(a0, Operand(Smi::FromInt(expr->ComputeFlags())));
    __ Push(a3, a2, a1, a0);
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
      PushOperand(v0);  // array literal
      result_saved = true;
    }

    VisitForAccumulatorValue(subexpr);

    __ li(StoreDescriptor::NameRegister(), Operand(Smi::FromInt(array_index)));
    __ lw(StoreDescriptor::ReceiverRegister(), MemOperand(sp, 0));
    __ mov(StoreDescriptor::ValueRegister(), result_register());
    CallKeyedStoreIC(expr->LiteralFeedbackSlot());

    PrepareForBailoutForId(expr->GetIdForElement(array_index),
                           BailoutState::NO_REGISTERS);
  }

  if (result_saved) {
    context()->PlugTOS();
  } else {
    context()->Plug(v0);
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
        __ lw(LoadDescriptor::ReceiverRegister(), MemOperand(sp, 0));
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
        const Register scratch = a1;
        __ lw(scratch, MemOperand(sp, kPointerSize));
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
        const Register scratch1 = t0;
        const Register scratch2 = a1;
        __ lw(scratch1, MemOperand(sp, 2 * kPointerSize));
        __ lw(scratch2, MemOperand(sp, 1 * kPointerSize));
        PushOperands(scratch1, scratch2, result_register());
      }
      break;
    }
    case KEYED_PROPERTY:
      // We need the key and receiver on both the stack and in v0 and a1.
      if (expr->is_compound()) {
        VisitForStackValue(property->obj());
        VisitForStackValue(property->key());
        __ lw(LoadDescriptor::ReceiverRegister(),
              MemOperand(sp, 1 * kPointerSize));
        __ lw(LoadDescriptor::NameRegister(), MemOperand(sp, 0));
      } else {
        VisitForStackValue(property->obj());
        VisitForStackValue(property->key());
      }
      break;
  }

  // For compound assignments we need another deoptimization point after the
  // variable/property load.
  if (expr->is_compound()) {
    { AccumulatorValueContext context(this);
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
    PushOperand(v0);  // Left operand goes on the stack.
    VisitForAccumulatorValue(expr->value());

    AccumulatorValueContext context(this);
    if (ShouldInlineSmiCase(op)) {
      EmitInlineSmiBinaryOp(expr->binary_operation(),
                            op,
                            expr->target(),
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
      context()->Plug(v0);
      break;
    case NAMED_PROPERTY:
      EmitNamedPropertyAssignment(expr);
      break;
    case NAMED_SUPER_PROPERTY:
      EmitNamedSuperPropertyStore(property);
      context()->Plug(v0);
      break;
    case KEYED_SUPER_PROPERTY:
      EmitKeyedSuperPropertyStore(property);
      context()->Plug(v0);
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

  __ jmp(&suspend);
  __ bind(&continuation);
  // When we arrive here, v0 holds the generator object.
  __ RecordGeneratorContinuation();
  __ lw(a1, FieldMemOperand(v0, JSGeneratorObject::kResumeModeOffset));
  __ lw(v0, FieldMemOperand(v0, JSGeneratorObject::kInputOrDebugPosOffset));
  __ Branch(&resume, eq, a1, Operand(Smi::FromInt(JSGeneratorObject::kNext)));
  __ Push(result_register());
  __ Branch(&exception, eq, a1,
            Operand(Smi::FromInt(JSGeneratorObject::kThrow)));
  EmitCreateIteratorResult(true);
  EmitUnwindAndReturn();

  __ bind(&exception);
  __ CallRuntime(expr->rethrow_on_exception() ? Runtime::kReThrow
                                              : Runtime::kThrow);

  __ bind(&suspend);
  OperandStackDepthIncrement(1);  // Not popped on this path.
  VisitForAccumulatorValue(expr->generator_object());
  DCHECK(continuation.pos() > 0 && Smi::IsValid(continuation.pos()));
  __ li(a1, Operand(Smi::FromInt(continuation.pos())));
  __ sw(a1, FieldMemOperand(v0, JSGeneratorObject::kContinuationOffset));
  __ sw(cp, FieldMemOperand(v0, JSGeneratorObject::kContextOffset));
  __ mov(a1, cp);
  __ RecordWriteField(v0, JSGeneratorObject::kContextOffset, a1, a2,
                      kRAHasBeenSaved, kDontSaveFPRegs);
  __ Addu(a1, fp, Operand(StandardFrameConstants::kExpressionsOffset));
  __ Branch(&post_runtime, eq, sp, Operand(a1));
  __ push(v0);  // generator object
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
    __ Subu(v0, fp, sp);
    __ Assert(eq, kUnexpectedStackDepth, v0, Operand(expected_diff));
  }
}

void FullCodeGenerator::EmitCreateIteratorResult(bool done) {
  Label allocate, done_allocate;

  __ Allocate(JSIteratorResult::kSize, v0, a2, a3, &allocate,
              NO_ALLOCATION_FLAGS);
  __ jmp(&done_allocate);

  __ bind(&allocate);
  __ Push(Smi::FromInt(JSIteratorResult::kSize));
  __ CallRuntime(Runtime::kAllocateInNewSpace);

  __ bind(&done_allocate);
  __ LoadNativeContextSlot(Context::ITERATOR_RESULT_MAP_INDEX, a1);
  PopOperand(a2);
  __ LoadRoot(a3,
              done ? Heap::kTrueValueRootIndex : Heap::kFalseValueRootIndex);
  __ LoadRoot(t0, Heap::kEmptyFixedArrayRootIndex);
  __ sw(a1, FieldMemOperand(v0, HeapObject::kMapOffset));
  __ sw(t0, FieldMemOperand(v0, JSObject::kPropertiesOffset));
  __ sw(t0, FieldMemOperand(v0, JSObject::kElementsOffset));
  __ sw(a2, FieldMemOperand(v0, JSIteratorResult::kValueOffset));
  __ sw(a3, FieldMemOperand(v0, JSIteratorResult::kDoneOffset));
  STATIC_ASSERT(JSIteratorResult::kSize == 5 * kPointerSize);
}


void FullCodeGenerator::EmitInlineSmiBinaryOp(BinaryOperation* expr,
                                              Token::Value op,
                                              Expression* left_expr,
                                              Expression* right_expr) {
  Label done, smi_case, stub_call;

  Register scratch1 = a2;
  Register scratch2 = a3;

  // Get the arguments.
  Register left = a1;
  Register right = a0;
  PopOperand(left);
  __ mov(a0, result_register());

  // Perform combined smi check on both operands.
  __ Or(scratch1, left, Operand(right));
  STATIC_ASSERT(kSmiTag == 0);
  JumpPatchSite patch_site(masm_);
  patch_site.EmitJumpIfSmi(scratch1, &smi_case);

  __ bind(&stub_call);
  Handle<Code> code = CodeFactory::BinaryOpIC(isolate(), op).code();
  CallIC(code, expr->BinaryOperationFeedbackId());
  patch_site.EmitPatchInfo();
  __ jmp(&done);

  __ bind(&smi_case);
  // Smi case. This code works the same way as the smi-smi case in the type
  // recording binary operation stub, see
  switch (op) {
    case Token::SAR:
      __ GetLeastBitsFromSmi(scratch1, right, 5);
      __ srav(right, left, scratch1);
      __ And(v0, right, Operand(~kSmiTagMask));
      break;
    case Token::SHL: {
      __ SmiUntag(scratch1, left);
      __ GetLeastBitsFromSmi(scratch2, right, 5);
      __ sllv(scratch1, scratch1, scratch2);
      __ Addu(scratch2, scratch1, Operand(0x40000000));
      __ Branch(&stub_call, lt, scratch2, Operand(zero_reg));
      __ SmiTag(v0, scratch1);
      break;
    }
    case Token::SHR: {
      __ SmiUntag(scratch1, left);
      __ GetLeastBitsFromSmi(scratch2, right, 5);
      __ srlv(scratch1, scratch1, scratch2);
      __ And(scratch2, scratch1, 0xc0000000);
      __ Branch(&stub_call, ne, scratch2, Operand(zero_reg));
      __ SmiTag(v0, scratch1);
      break;
    }
    case Token::ADD:
      __ AddBranchOvf(v0, left, Operand(right), &stub_call);
      break;
    case Token::SUB:
      __ SubBranchOvf(v0, left, Operand(right), &stub_call);
      break;
    case Token::MUL: {
      __ SmiUntag(scratch1, right);
      __ Mul(scratch2, v0, left, scratch1);
      __ sra(scratch1, v0, 31);
      __ Branch(&stub_call, ne, scratch1, Operand(scratch2));
      __ Branch(&done, ne, v0, Operand(zero_reg));
      __ Addu(scratch2, right, left);
      __ Branch(&stub_call, lt, scratch2, Operand(zero_reg));
      DCHECK(Smi::FromInt(0) == 0);
      __ mov(v0, zero_reg);
      break;
    }
    case Token::BIT_OR:
      __ Or(v0, left, Operand(right));
      break;
    case Token::BIT_AND:
      __ And(v0, left, Operand(right));
      break;
    case Token::BIT_XOR:
      __ Xor(v0, left, Operand(right));
      break;
    default:
      UNREACHABLE();
  }

  __ bind(&done);
  context()->Plug(v0);
}


void FullCodeGenerator::EmitClassDefineProperties(ClassLiteral* lit) {
  for (int i = 0; i < lit->properties()->length(); i++) {
    ClassLiteral::Property* property = lit->properties()->at(i);
    Expression* value = property->value();

    Register scratch = a1;
    if (property->is_static()) {
      __ lw(scratch, MemOperand(sp, kPointerSize));  // constructor
    } else {
      __ lw(scratch, MemOperand(sp, 0));  // prototype
    }
    PushOperand(scratch);
    EmitPropertyKey(property, lit->GetIdForProperty(i));

    // The static prototype property is read only. We handle the non computed
    // property name case in the parser. Since this is the only case where we
    // need to check for an own read only property we special case this so we do
    // not need to do this for every property.
    if (property->is_static() && property->is_computed_name()) {
      __ CallRuntime(Runtime::kThrowIfStaticPrototype);
      __ push(v0);
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
  __ mov(a0, result_register());
  PopOperand(a1);
  Handle<Code> code = CodeFactory::BinaryOpIC(isolate(), op).code();
  JumpPatchSite patch_site(masm_);    // unbound, signals no inlined smi code.
  CallIC(code, expr->BinaryOperationFeedbackId());
  patch_site.EmitPatchInfo();
  context()->Plug(v0);
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
      PushOperand(result_register());  // Preserve value.
      VisitForAccumulatorValue(prop->obj());
      __ mov(StoreDescriptor::ReceiverRegister(), result_register());
      PopOperand(StoreDescriptor::ValueRegister());  // Restore value.
      CallStoreIC(slot, prop->key()->AsLiteral()->value());
      break;
    }
    case NAMED_SUPER_PROPERTY: {
      PushOperand(v0);
      VisitForStackValue(prop->obj()->AsSuperPropertyReference()->this_var());
      VisitForAccumulatorValue(
          prop->obj()->AsSuperPropertyReference()->home_object());
      // stack: value, this; v0: home_object
      Register scratch = a2;
      Register scratch2 = a3;
      __ mov(scratch, result_register());             // home_object
      __ lw(v0, MemOperand(sp, kPointerSize));        // value
      __ lw(scratch2, MemOperand(sp, 0));             // this
      __ sw(scratch2, MemOperand(sp, kPointerSize));  // this
      __ sw(scratch, MemOperand(sp, 0));              // home_object
      // stack: this, home_object; v0: value
      EmitNamedSuperPropertyStore(prop);
      break;
    }
    case KEYED_SUPER_PROPERTY: {
      PushOperand(v0);
      VisitForStackValue(prop->obj()->AsSuperPropertyReference()->this_var());
      VisitForStackValue(
          prop->obj()->AsSuperPropertyReference()->home_object());
      VisitForAccumulatorValue(prop->key());
      Register scratch = a2;
      Register scratch2 = a3;
      __ lw(scratch2, MemOperand(sp, 2 * kPointerSize));  // value
      // stack: value, this, home_object; v0: key, a3: value
      __ lw(scratch, MemOperand(sp, kPointerSize));  // this
      __ sw(scratch, MemOperand(sp, 2 * kPointerSize));
      __ lw(scratch, MemOperand(sp, 0));  // home_object
      __ sw(scratch, MemOperand(sp, kPointerSize));
      __ sw(v0, MemOperand(sp, 0));
      __ Move(v0, scratch2);
      // stack: this, home_object, key; v0: value.
      EmitKeyedSuperPropertyStore(prop);
      break;
    }
    case KEYED_PROPERTY: {
      PushOperand(result_register());  // Preserve value.
      VisitForStackValue(prop->obj());
      VisitForAccumulatorValue(prop->key());
      __ mov(StoreDescriptor::NameRegister(), result_register());
      PopOperands(StoreDescriptor::ValueRegister(),
                  StoreDescriptor::ReceiverRegister());
      CallKeyedStoreIC(slot);
      break;
    }
  }
  context()->Plug(v0);
}


void FullCodeGenerator::EmitStoreToStackLocalOrContextSlot(
    Variable* var, MemOperand location) {
  __ sw(result_register(), location);
  if (var->IsContextSlot()) {
    // RecordWrite may destroy all its register arguments.
    __ Move(a3, result_register());
    int offset = Context::SlotOffset(var->index());
    __ RecordWriteContextSlot(
        a1, offset, a3, a2, kRAHasBeenSaved, kDontSaveFPRegs);
  }
}


void FullCodeGenerator::EmitVariableAssignment(Variable* var, Token::Value op,
                                               FeedbackVectorSlot slot) {
  if (var->IsUnallocated()) {
    // Global var, const, or let.
    __ mov(StoreDescriptor::ValueRegister(), result_register());
    __ LoadGlobalObject(StoreDescriptor::ReceiverRegister());
    CallStoreIC(slot, var->name());

  } else if (IsLexicalVariableMode(var->mode()) && op != Token::INIT) {
    DCHECK(!var->IsLookupSlot());
    DCHECK(var->IsStackAllocated() || var->IsContextSlot());
    MemOperand location = VarOperand(var, a1);
    // Perform an initialization check for lexically declared variables.
    if (var->binding_needs_init()) {
      Label assign;
      __ lw(a3, location);
      __ LoadRoot(t0, Heap::kTheHoleValueRootIndex);
      __ Branch(&assign, ne, a3, Operand(t0));
      __ li(a3, Operand(var->name()));
      __ push(a3);
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
    MemOperand location = VarOperand(var, a1);
    __ lw(a3, location);
    __ LoadRoot(at, Heap::kTheHoleValueRootIndex);
    __ Branch(&uninitialized_this, eq, a3, Operand(at));
    __ li(a0, Operand(var->name()));
    __ Push(a0);
    __ CallRuntime(Runtime::kThrowReferenceError);
    __ bind(&uninitialized_this);
    EmitStoreToStackLocalOrContextSlot(var, location);

  } else {
    DCHECK(var->mode() != CONST || op == Token::INIT);
    if (var->IsLookupSlot()) {
      // Assignment to var.
      __ Push(var->name());
      __ Push(v0);
      __ CallRuntime(is_strict(language_mode())
                         ? Runtime::kStoreLookupSlot_Strict
                         : Runtime::kStoreLookupSlot_Sloppy);
    } else {
      // Assignment to var or initializing assignment to let/const in harmony
      // mode.
      DCHECK((var->IsStackAllocated() || var->IsContextSlot()));
      MemOperand location = VarOperand(var, a1);
      if (FLAG_debug_code && var->mode() == LET && op == Token::INIT) {
        // Check for an uninitialized let binding.
        __ lw(a2, location);
        __ LoadRoot(t0, Heap::kTheHoleValueRootIndex);
        __ Check(eq, kLetBindingReInitialization, a2, Operand(t0));
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

  __ mov(StoreDescriptor::ValueRegister(), result_register());
  PopOperand(StoreDescriptor::ReceiverRegister());
  CallStoreIC(expr->AssignmentSlot(), prop->key()->AsLiteral()->value());

  PrepareForBailoutForId(expr->AssignmentId(), BailoutState::TOS_REGISTER);
  context()->Plug(v0);
}


void FullCodeGenerator::EmitNamedSuperPropertyStore(Property* prop) {
  // Assignment to named property of super.
  // v0 : value
  // stack : receiver ('this'), home_object
  DCHECK(prop != NULL);
  Literal* key = prop->key()->AsLiteral();
  DCHECK(key != NULL);

  PushOperand(key->value());
  PushOperand(v0);
  CallRuntimeWithOperands(is_strict(language_mode())
                              ? Runtime::kStoreToSuper_Strict
                              : Runtime::kStoreToSuper_Sloppy);
}


void FullCodeGenerator::EmitKeyedSuperPropertyStore(Property* prop) {
  // Assignment to named property of super.
  // v0 : value
  // stack : receiver ('this'), home_object, key
  DCHECK(prop != NULL);

  PushOperand(v0);
  CallRuntimeWithOperands(is_strict(language_mode())
                              ? Runtime::kStoreKeyedToSuper_Strict
                              : Runtime::kStoreKeyedToSuper_Sloppy);
}


void FullCodeGenerator::EmitKeyedPropertyAssignment(Assignment* expr) {
  // Assignment to a property, using a keyed store IC.
  // Call keyed store IC.
  // The arguments are:
  // - a0 is the value,
  // - a1 is the key,
  // - a2 is the receiver.
  __ mov(StoreDescriptor::ValueRegister(), result_register());
  PopOperands(StoreDescriptor::ReceiverRegister(),
              StoreDescriptor::NameRegister());
  DCHECK(StoreDescriptor::ValueRegister().is(a0));

  CallKeyedStoreIC(expr->AssignmentSlot());

  PrepareForBailoutForId(expr->AssignmentId(), BailoutState::TOS_REGISTER);
  context()->Plug(v0);
}


void FullCodeGenerator::CallIC(Handle<Code> code,
                               TypeFeedbackId id) {
  ic_total_count_++;
  __ Call(code, RelocInfo::CODE_TARGET, id);
}


// Code common for calls using the IC.
void FullCodeGenerator::EmitCallWithLoadIC(Call* expr) {
  Expression* callee = expr->expression();

  // Get the target function.
  ConvertReceiverMode convert_mode;
  if (callee->IsVariableProxy()) {
    { StackValueContext context(this);
      EmitVariableLoad(callee->AsVariableProxy());
      PrepareForBailout(callee, BailoutState::NO_REGISTERS);
    }
    // Push undefined as receiver. This is patched in the method prologue if it
    // is a sloppy mode method.
    __ LoadRoot(at, Heap::kUndefinedValueRootIndex);
    PushOperand(at);
    convert_mode = ConvertReceiverMode::kNullOrUndefined;
  } else {
    // Load the function from the receiver.
    DCHECK(callee->IsProperty());
    DCHECK(!callee->AsProperty()->IsSuperAccess());
    __ lw(LoadDescriptor::ReceiverRegister(), MemOperand(sp, 0));
    EmitNamedPropertyLoad(callee->AsProperty());
    PrepareForBailoutForId(callee->AsProperty()->LoadId(),
                           BailoutState::TOS_REGISTER);
    // Push the target function under the receiver.
    __ lw(at, MemOperand(sp, 0));
    PushOperand(at);
    __ sw(v0, MemOperand(sp, kPointerSize));
    convert_mode = ConvertReceiverMode::kNotNullOrUndefined;
  }

  EmitCall(expr, convert_mode);
}


void FullCodeGenerator::EmitSuperCallWithLoadIC(Call* expr) {
  SetExpressionPosition(expr);
  Expression* callee = expr->expression();
  DCHECK(callee->IsProperty());
  Property* prop = callee->AsProperty();
  DCHECK(prop->IsSuperAccess());

  Literal* key = prop->key()->AsLiteral();
  DCHECK(!key->value()->IsSmi());
  // Load the function from the receiver.
  const Register scratch = a1;
  SuperPropertyReference* super_ref = prop->obj()->AsSuperPropertyReference();
  VisitForAccumulatorValue(super_ref->home_object());
  __ mov(scratch, v0);
  VisitForAccumulatorValue(super_ref->this_var());
  PushOperands(scratch, v0, v0, scratch);
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
  __ sw(v0, MemOperand(sp, kPointerSize));

  // Stack here:
  // - target function
  // - this (receiver)
  EmitCall(expr);
}


// Code common for calls using the IC.
void FullCodeGenerator::EmitKeyedCallWithLoadIC(Call* expr,
                                                Expression* key) {
  // Load the key.
  VisitForAccumulatorValue(key);

  Expression* callee = expr->expression();

  // Load the function from the receiver.
  DCHECK(callee->IsProperty());
  __ lw(LoadDescriptor::ReceiverRegister(), MemOperand(sp, 0));
  __ Move(LoadDescriptor::NameRegister(), v0);
  EmitKeyedPropertyLoad(callee->AsProperty());
  PrepareForBailoutForId(callee->AsProperty()->LoadId(),
                         BailoutState::TOS_REGISTER);

  // Push the target function under the receiver.
  __ lw(at, MemOperand(sp, 0));
  PushOperand(at);
  __ sw(v0, MemOperand(sp, kPointerSize));

  EmitCall(expr, ConvertReceiverMode::kNotNullOrUndefined);
}


void FullCodeGenerator::EmitKeyedSuperCallWithLoadIC(Call* expr) {
  Expression* callee = expr->expression();
  DCHECK(callee->IsProperty());
  Property* prop = callee->AsProperty();
  DCHECK(prop->IsSuperAccess());

  SetExpressionPosition(prop);
  // Load the function from the receiver.
  const Register scratch = a1;
  SuperPropertyReference* super_ref = prop->obj()->AsSuperPropertyReference();
  VisitForAccumulatorValue(super_ref->home_object());
  __ Move(scratch, v0);
  VisitForAccumulatorValue(super_ref->this_var());
  PushOperands(scratch, v0, v0, scratch);
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
  __ sw(v0, MemOperand(sp, kPointerSize));

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
  // Record source position of the IC call.
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
  __ li(a3, Operand(SmiFromSlot(expr->CallFeedbackICSlot())));
  __ lw(a1, MemOperand(sp, (arg_count + 1) * kPointerSize));
  // Don't assign a type feedback id to the IC, since type feedback is provided
  // by the vector above.
  CallIC(ic);
  OperandStackDepthDecrement(arg_count + 1);

  RecordJSReturnSite(expr);
  RestoreContext();
  context()->DropAndPlug(1, v0);
}

void FullCodeGenerator::EmitResolvePossiblyDirectEval(Call* expr) {
  int arg_count = expr->arguments()->length();
  // t4: copy of the first argument or undefined if it doesn't exist.
  if (arg_count > 0) {
    __ lw(t4, MemOperand(sp, arg_count * kPointerSize));
  } else {
    __ LoadRoot(t4, Heap::kUndefinedValueRootIndex);
  }

  // t3: the receiver of the enclosing function.
  __ lw(t3, MemOperand(fp, JavaScriptFrameConstants::kFunctionOffset));

  // t2: the language mode.
  __ li(t2, Operand(Smi::FromInt(language_mode())));

  // t1: the start position of the scope the calls resides in.
  __ li(t1, Operand(Smi::FromInt(scope()->start_position())));

  // t0: the source position of the eval call.
  __ li(t0, Operand(Smi::FromInt(expr->position())));

  // Do the runtime call.
  __ Push(t4, t3, t2, t1, t0);
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
    // Call the runtime to find the function to call (returned in v0)
    // and the object holding it (returned in v1).
    __ Push(callee->name());
    __ CallRuntime(Runtime::kLoadLookupSlotForCall);
    PushOperands(v0, v1);  // Function, receiver.
    PrepareForBailoutForId(expr->LookupId(), BailoutState::NO_REGISTERS);

    // If fast case code has been generated, emit code to push the
    // function and receiver and have the slow path jump around this
    // code.
    if (done.is_linked()) {
      Label call;
      __ Branch(&call);
      __ bind(&done);
      // Push function.
      __ push(v0);
      // The receiver is implicitly the global receiver. Indicate this
      // by passing the hole to the call function stub.
      __ LoadRoot(a1, Heap::kUndefinedValueRootIndex);
      __ push(a1);
      __ bind(&call);
    }
  } else {
    VisitForStackValue(callee);
    // refEnv.WithBaseObject()
    __ LoadRoot(a2, Heap::kUndefinedValueRootIndex);
    PushOperand(a2);  // Reserved receiver slot.
  }
}


void FullCodeGenerator::EmitPossiblyEvalCall(Call* expr) {
  // In a call to eval, we first call Runtime_ResolvePossiblyDirectEval
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
  __ lw(a1, MemOperand(sp, (arg_count + 1) * kPointerSize));
  __ push(a1);
  EmitResolvePossiblyDirectEval(expr);

  // Touch up the stack with the resolved function.
  __ sw(v0, MemOperand(sp, (arg_count + 1) * kPointerSize));

  PrepareForBailoutForId(expr->EvalId(), BailoutState::NO_REGISTERS);
  // Record source position for debugger.
  SetCallPosition(expr);
  __ lw(a1, MemOperand(sp, (arg_count + 1) * kPointerSize));
  __ li(a0, Operand(arg_count));
  __ Call(isolate()->builtins()->Call(ConvertReceiverMode::kAny,
                                      expr->tail_call_mode()),
          RelocInfo::CODE_TARGET);
  OperandStackDepthDecrement(arg_count + 1);
  RecordJSReturnSite(expr);
  RestoreContext();
  context()->DropAndPlug(1, v0);
}


void FullCodeGenerator::VisitCallNew(CallNew* expr) {
  Comment cmnt(masm_, "[ CallNew");
  // According to ECMA-262, section 11.2.2, page 44, the function
  // expression in new calls must be evaluated before the
  // arguments.

  // Push constructor on the stack.  If it's not a function it's used as
  // receiver for CALL_NON_FUNCTION, otherwise the value on the stack is
  // ignored.g
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

  // Load function and argument count into a1 and a0.
  __ li(a0, Operand(arg_count));
  __ lw(a1, MemOperand(sp, arg_count * kPointerSize));

  // Record call targets in unoptimized code.
  __ EmitLoadTypeFeedbackVector(a2);
  __ li(a3, Operand(SmiFromSlot(expr->CallNewFeedbackSlot())));

  CallConstructStub stub(isolate());
  __ Call(stub.GetCode(), RelocInfo::CODE_TARGET);
  OperandStackDepthDecrement(arg_count + 1);
  PrepareForBailoutForId(expr->ReturnId(), BailoutState::TOS_REGISTER);
  RestoreContext();
  context()->Plug(v0);
}


void FullCodeGenerator::EmitSuperConstructorCall(Call* expr) {
  SuperCallReference* super_call_ref =
      expr->expression()->AsSuperCallReference();
  DCHECK_NOT_NULL(super_call_ref);

  // Push the super constructor target on the stack (may be null,
  // but the Construct builtin can deal with that properly).
  VisitForAccumulatorValue(super_call_ref->this_function_var());
  __ AssertFunction(result_register());
  __ lw(result_register(),
        FieldMemOperand(result_register(), HeapObject::kMapOffset));
  __ lw(result_register(),
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

  // Load new target into a3.
  VisitForAccumulatorValue(super_call_ref->new_target_var());
  __ mov(a3, result_register());

  // Load function and argument count into a1 and a0.
  __ li(a0, Operand(arg_count));
  __ lw(a1, MemOperand(sp, arg_count * kPointerSize));

  __ Call(isolate()->builtins()->Construct(), RelocInfo::CODE_TARGET);
  OperandStackDepthDecrement(arg_count + 1);

  RecordJSReturnSite(expr);
  RestoreContext();
  context()->Plug(v0);
}


void FullCodeGenerator::EmitIsSmi(CallRuntime* expr) {
  ZoneList<Expression*>* args = expr->arguments();
  DCHECK(args->length() == 1);

  VisitForAccumulatorValue(args->at(0));

  Label materialize_true, materialize_false;
  Label* if_true = NULL;
  Label* if_false = NULL;
  Label* fall_through = NULL;
  context()->PrepareTest(&materialize_true, &materialize_false,
                         &if_true, &if_false, &fall_through);

  PrepareForBailoutBeforeSplit(expr, true, if_true, if_false);
  __ SmiTst(v0, t0);
  Split(eq, t0, Operand(zero_reg), if_true, if_false, fall_through);

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
  context()->PrepareTest(&materialize_true, &materialize_false,
                         &if_true, &if_false, &fall_through);

  __ JumpIfSmi(v0, if_false);
  __ GetObjectType(v0, a1, a1);
  PrepareForBailoutBeforeSplit(expr, true, if_true, if_false);
  Split(ge, a1, Operand(FIRST_JS_RECEIVER_TYPE),
        if_true, if_false, fall_through);

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
  context()->PrepareTest(&materialize_true, &materialize_false,
                         &if_true, &if_false, &fall_through);

  __ JumpIfSmi(v0, if_false);
  __ GetObjectType(v0, a1, a1);
  PrepareForBailoutBeforeSplit(expr, true, if_true, if_false);
  Split(eq, a1, Operand(JS_ARRAY_TYPE),
        if_true, if_false, fall_through);

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

  __ JumpIfSmi(v0, if_false);
  __ GetObjectType(v0, a1, a1);
  PrepareForBailoutBeforeSplit(expr, true, if_true, if_false);
  Split(eq, a1, Operand(JS_TYPED_ARRAY_TYPE), if_true, if_false, fall_through);

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
  context()->PrepareTest(&materialize_true, &materialize_false,
                         &if_true, &if_false, &fall_through);

  __ JumpIfSmi(v0, if_false);
  __ GetObjectType(v0, a1, a1);
  PrepareForBailoutBeforeSplit(expr, true, if_true, if_false);
  Split(eq, a1, Operand(JS_REGEXP_TYPE), if_true, if_false, fall_through);

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

  __ JumpIfSmi(v0, if_false);
  __ GetObjectType(v0, a1, a1);
  PrepareForBailoutBeforeSplit(expr, true, if_true, if_false);
  Split(eq, a1, Operand(JS_PROXY_TYPE), if_true, if_false, fall_through);

  context()->Plug(if_true, if_false);
}


void FullCodeGenerator::EmitClassOf(CallRuntime* expr) {
  ZoneList<Expression*>* args = expr->arguments();
  DCHECK(args->length() == 1);
  Label done, null, function, non_function_constructor;

  VisitForAccumulatorValue(args->at(0));

  // If the object is not a JSReceiver, we return null.
  __ JumpIfSmi(v0, &null);
  STATIC_ASSERT(LAST_JS_RECEIVER_TYPE == LAST_TYPE);
  __ GetObjectType(v0, v0, a1);  // Map is now in v0.
  __ Branch(&null, lt, a1, Operand(FIRST_JS_RECEIVER_TYPE));

  // Return 'Function' for JSFunction and JSBoundFunction objects.
  STATIC_ASSERT(LAST_FUNCTION_TYPE == LAST_TYPE);
  __ Branch(&function, hs, a1, Operand(FIRST_FUNCTION_TYPE));

  // Check if the constructor in the map is a JS function.
  Register instance_type = a2;
  __ GetMapConstructor(v0, v0, a1, instance_type);
  __ Branch(&non_function_constructor, ne, instance_type,
            Operand(JS_FUNCTION_TYPE));

  // v0 now contains the constructor function. Grab the
  // instance class name from there.
  __ lw(v0, FieldMemOperand(v0, JSFunction::kSharedFunctionInfoOffset));
  __ lw(v0, FieldMemOperand(v0, SharedFunctionInfo::kInstanceClassNameOffset));
  __ Branch(&done);

  // Functions have class 'Function'.
  __ bind(&function);
  __ LoadRoot(v0, Heap::kFunction_stringRootIndex);
  __ jmp(&done);

  // Objects with a non-function constructor have class 'Object'.
  __ bind(&non_function_constructor);
  __ LoadRoot(v0, Heap::kObject_stringRootIndex);
  __ jmp(&done);

  // Non-JS objects have class null.
  __ bind(&null);
  __ LoadRoot(v0, Heap::kNullValueRootIndex);

  // All done.
  __ bind(&done);

  context()->Plug(v0);
}


void FullCodeGenerator::EmitStringCharCodeAt(CallRuntime* expr) {
  ZoneList<Expression*>* args = expr->arguments();
  DCHECK(args->length() == 2);

  VisitForStackValue(args->at(0));
  VisitForAccumulatorValue(args->at(1));
  __ mov(a0, result_register());

  Register object = a1;
  Register index = a0;
  Register result = v0;

  PopOperand(object);

  Label need_conversion;
  Label index_out_of_range;
  Label done;
  StringCharCodeAtGenerator generator(object, index, result, &need_conversion,
                                      &need_conversion, &index_out_of_range);
  generator.GenerateFast(masm_);
  __ jmp(&done);

  __ bind(&index_out_of_range);
  // When the index is out of range, the spec requires us to return
  // NaN.
  __ LoadRoot(result, Heap::kNanValueRootIndex);
  __ jmp(&done);

  __ bind(&need_conversion);
  // Load the undefined value into the result register, which will
  // trigger conversion.
  __ LoadRoot(result, Heap::kUndefinedValueRootIndex);
  __ jmp(&done);

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
  // Move target to a1.
  int const argc = args->length() - 2;
  __ lw(a1, MemOperand(sp, (argc + 1) * kPointerSize));
  // Call the target.
  __ li(a0, Operand(argc));
  __ Call(isolate()->builtins()->Call(), RelocInfo::CODE_TARGET);
  OperandStackDepthDecrement(argc + 1);
  RestoreContext();
  // Discard the function left on TOS.
  context()->DropAndPlug(1, v0);
}


void FullCodeGenerator::EmitHasCachedArrayIndex(CallRuntime* expr) {
  ZoneList<Expression*>* args = expr->arguments();
  VisitForAccumulatorValue(args->at(0));

  Label materialize_true, materialize_false;
  Label* if_true = NULL;
  Label* if_false = NULL;
  Label* fall_through = NULL;
  context()->PrepareTest(&materialize_true, &materialize_false,
                         &if_true, &if_false, &fall_through);

  __ lw(a0, FieldMemOperand(v0, String::kHashFieldOffset));
  __ And(a0, a0, Operand(String::kContainsCachedArrayIndexMask));

  PrepareForBailoutBeforeSplit(expr, true, if_true, if_false);
  Split(eq, a0, Operand(zero_reg), if_true, if_false, fall_through);

  context()->Plug(if_true, if_false);
}


void FullCodeGenerator::EmitGetCachedArrayIndex(CallRuntime* expr) {
  ZoneList<Expression*>* args = expr->arguments();
  DCHECK(args->length() == 1);
  VisitForAccumulatorValue(args->at(0));

  __ AssertString(v0);

  __ lw(v0, FieldMemOperand(v0, String::kHashFieldOffset));
  __ IndexFromHash(v0, v0);

  context()->Plug(v0);
}


void FullCodeGenerator::EmitGetSuperConstructor(CallRuntime* expr) {
  ZoneList<Expression*>* args = expr->arguments();
  DCHECK_EQ(1, args->length());
  VisitForAccumulatorValue(args->at(0));
  __ AssertFunction(v0);
  __ lw(v0, FieldMemOperand(v0, HeapObject::kMapOffset));
  __ lw(v0, FieldMemOperand(v0, Map::kPrototypeOffset));
  context()->Plug(v0);
}

void FullCodeGenerator::EmitDebugIsActive(CallRuntime* expr) {
  DCHECK(expr->arguments()->length() == 0);
  ExternalReference debug_is_active =
      ExternalReference::debug_is_active_address(isolate());
  __ li(at, Operand(debug_is_active));
  __ lb(v0, MemOperand(at));
  __ SmiTag(v0);
  context()->Plug(v0);
}


void FullCodeGenerator::EmitCreateIterResultObject(CallRuntime* expr) {
  ZoneList<Expression*>* args = expr->arguments();
  DCHECK_EQ(2, args->length());
  VisitForStackValue(args->at(0));
  VisitForStackValue(args->at(1));

  Label runtime, done;

  __ Allocate(JSIteratorResult::kSize, v0, a2, a3, &runtime,
              NO_ALLOCATION_FLAGS);
  __ LoadNativeContextSlot(Context::ITERATOR_RESULT_MAP_INDEX, a1);
  __ Pop(a2, a3);
  __ LoadRoot(t0, Heap::kEmptyFixedArrayRootIndex);
  __ sw(a1, FieldMemOperand(v0, HeapObject::kMapOffset));
  __ sw(t0, FieldMemOperand(v0, JSObject::kPropertiesOffset));
  __ sw(t0, FieldMemOperand(v0, JSObject::kElementsOffset));
  __ sw(a2, FieldMemOperand(v0, JSIteratorResult::kValueOffset));
  __ sw(a3, FieldMemOperand(v0, JSIteratorResult::kDoneOffset));
  STATIC_ASSERT(JSIteratorResult::kSize == 5 * kPointerSize);
  __ jmp(&done);

  __ bind(&runtime);
  CallRuntimeWithOperands(Runtime::kCreateIterResultObject);

  __ bind(&done);
  context()->Plug(v0);
}


void FullCodeGenerator::EmitLoadJSRuntimeFunction(CallRuntime* expr) {
  // Push function.
  __ LoadNativeContextSlot(expr->context_index(), v0);
  PushOperand(v0);

  // Push undefined as the receiver.
  __ LoadRoot(v0, Heap::kUndefinedValueRootIndex);
  PushOperand(v0);
}


void FullCodeGenerator::EmitCallJSRuntimeFunction(CallRuntime* expr) {
  ZoneList<Expression*>* args = expr->arguments();
  int arg_count = args->length();

  SetCallPosition(expr);
  __ lw(a1, MemOperand(sp, (arg_count + 1) * kPointerSize));
  __ li(a0, Operand(arg_count));
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
        context()->Plug(v0);
      } else if (proxy != NULL) {
        Variable* var = proxy->var();
        // Delete of an unqualified identifier is disallowed in strict mode but
        // "delete this" is allowed.
        bool is_this = var->is_this();
        DCHECK(is_sloppy(language_mode()) || is_this);
        if (var->IsUnallocated()) {
          __ LoadGlobalObject(a2);
          __ li(a1, Operand(var->name()));
          __ Push(a2, a1);
          __ CallRuntime(Runtime::kDeleteProperty_Sloppy);
          context()->Plug(v0);
        } else if (var->IsStackAllocated() || var->IsContextSlot()) {
          // Result of deleting non-global, non-dynamic variables is false.
          // The subexpression does not have side effects.
          context()->Plug(is_this);
        } else {
          // Non-global variable.  Call the runtime to try to delete from the
          // context where the variable was introduced.
          __ Push(var->name());
          __ CallRuntime(Runtime::kDeleteLookupSlot);
          context()->Plug(v0);
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
        VisitForControl(expr->expression(),
                        test->false_label(),
                        test->true_label(),
                        test->fall_through());
        context()->Plug(test->true_label(), test->false_label());
      } else {
        // We handle value contexts explicitly rather than simply visiting
        // for control and plugging the control flow into the context,
        // because we need to prepare a pair of extra administrative AST ids
        // for the optimizing compiler.
        DCHECK(context()->IsAccumulatorValue() || context()->IsStackValue());
        Label materialize_true, materialize_false, done;
        VisitForControl(expr->expression(),
                        &materialize_false,
                        &materialize_true,
                        &materialize_true);
        if (!context()->IsAccumulatorValue()) OperandStackDepthIncrement(1);
        __ bind(&materialize_true);
        PrepareForBailoutForId(expr->MaterializeTrueId(),
                               BailoutState::NO_REGISTERS);
        __ LoadRoot(v0, Heap::kTrueValueRootIndex);
        if (context()->IsStackValue()) __ push(v0);
        __ jmp(&done);
        __ bind(&materialize_false);
        PrepareForBailoutForId(expr->MaterializeFalseId(),
                               BailoutState::NO_REGISTERS);
        __ LoadRoot(v0, Heap::kFalseValueRootIndex);
        if (context()->IsStackValue()) __ push(v0);
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
      __ mov(a3, v0);
      TypeofStub typeof_stub(isolate());
      __ CallStub(&typeof_stub);
      context()->Plug(v0);
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
      __ li(at, Operand(Smi::FromInt(0)));
      PushOperand(at);
    }
    switch (assign_type) {
      case NAMED_PROPERTY: {
        // Put the object both on the stack and in the register.
        VisitForStackValue(prop->obj());
        __ lw(LoadDescriptor::ReceiverRegister(), MemOperand(sp, 0));
        EmitNamedPropertyLoad(prop);
        break;
      }

      case NAMED_SUPER_PROPERTY: {
        VisitForStackValue(prop->obj()->AsSuperPropertyReference()->this_var());
        VisitForAccumulatorValue(
            prop->obj()->AsSuperPropertyReference()->home_object());
        const Register scratch = a1;
        __ lw(scratch, MemOperand(sp, 0));  // this
        PushOperands(result_register(), scratch, result_register());
        EmitNamedSuperPropertyLoad(prop);
        break;
      }

      case KEYED_SUPER_PROPERTY: {
        VisitForStackValue(prop->obj()->AsSuperPropertyReference()->this_var());
        VisitForStackValue(
            prop->obj()->AsSuperPropertyReference()->home_object());
        VisitForAccumulatorValue(prop->key());
        const Register scratch1 = a1;
        const Register scratch2 = t0;
        __ lw(scratch1, MemOperand(sp, 1 * kPointerSize));  // this
        __ lw(scratch2, MemOperand(sp, 0 * kPointerSize));  // home object
        PushOperands(result_register(), scratch1, scratch2, result_register());
        EmitKeyedSuperPropertyLoad(prop);
        break;
      }

      case KEYED_PROPERTY: {
        VisitForStackValue(prop->obj());
        VisitForStackValue(prop->key());
        __ lw(LoadDescriptor::ReceiverRegister(),
              MemOperand(sp, 1 * kPointerSize));
        __ lw(LoadDescriptor::NameRegister(), MemOperand(sp, 0));
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
  __ mov(a0, v0);
  if (ShouldInlineSmiCase(expr->op())) {
    Label slow;
    patch_site.EmitJumpIfNotSmi(v0, &slow);

    // Save result for postfix expressions.
    if (expr->is_postfix()) {
      if (!context()->IsEffect()) {
        // Save the result on the stack. If we have a named or keyed property
        // we store the result under the receiver that is currently on top
        // of the stack.
        switch (assign_type) {
          case VARIABLE:
            __ push(v0);
            break;
          case NAMED_PROPERTY:
            __ sw(v0, MemOperand(sp, kPointerSize));
            break;
          case NAMED_SUPER_PROPERTY:
            __ sw(v0, MemOperand(sp, 2 * kPointerSize));
            break;
          case KEYED_PROPERTY:
            __ sw(v0, MemOperand(sp, 2 * kPointerSize));
            break;
          case KEYED_SUPER_PROPERTY:
            __ sw(v0, MemOperand(sp, 3 * kPointerSize));
            break;
        }
      }
    }

    Register scratch1 = a1;
    __ li(scratch1, Operand(Smi::FromInt(count_value)));
    __ AddBranchNoOvf(v0, v0, Operand(scratch1), &done);
    // Call stub. Undo operation first.
    __ Move(v0, a0);
    __ jmp(&stub_call);
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
          PushOperand(v0);
          break;
        case NAMED_PROPERTY:
          __ sw(v0, MemOperand(sp, kPointerSize));
          break;
        case NAMED_SUPER_PROPERTY:
          __ sw(v0, MemOperand(sp, 2 * kPointerSize));
          break;
        case KEYED_PROPERTY:
          __ sw(v0, MemOperand(sp, 2 * kPointerSize));
          break;
        case KEYED_SUPER_PROPERTY:
          __ sw(v0, MemOperand(sp, 3 * kPointerSize));
          break;
      }
    }
  }

  __ bind(&stub_call);
  __ mov(a1, v0);
  __ li(a0, Operand(Smi::FromInt(count_value)));

  SetExpressionPosition(expr);

  Handle<Code> code = CodeFactory::BinaryOpIC(isolate(), Token::ADD).code();
  CallIC(code, expr->CountBinOpFeedbackId());
  patch_site.EmitPatchInfo();
  __ bind(&done);

  // Store the value returned in v0.
  switch (assign_type) {
    case VARIABLE:
      if (expr->is_postfix()) {
        { EffectContext context(this);
          EmitVariableAssignment(expr->expression()->AsVariableProxy()->var(),
                                 Token::ASSIGN, expr->CountSlot());
          PrepareForBailoutForId(expr->AssignmentId(),
                                 BailoutState::TOS_REGISTER);
          context.Plug(v0);
        }
        // For all contexts except EffectConstant we have the result on
        // top of the stack.
        if (!context()->IsEffect()) {
          context()->PlugTOS();
        }
      } else {
        EmitVariableAssignment(expr->expression()->AsVariableProxy()->var(),
                               Token::ASSIGN, expr->CountSlot());
        PrepareForBailoutForId(expr->AssignmentId(),
                               BailoutState::TOS_REGISTER);
        context()->Plug(v0);
      }
      break;
    case NAMED_PROPERTY: {
      __ mov(StoreDescriptor::ValueRegister(), result_register());
      PopOperand(StoreDescriptor::ReceiverRegister());
      CallStoreIC(expr->CountSlot(), prop->key()->AsLiteral()->value());
      PrepareForBailoutForId(expr->AssignmentId(), BailoutState::TOS_REGISTER);
      if (expr->is_postfix()) {
        if (!context()->IsEffect()) {
          context()->PlugTOS();
        }
      } else {
        context()->Plug(v0);
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
        context()->Plug(v0);
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
        context()->Plug(v0);
      }
      break;
    }
    case KEYED_PROPERTY: {
      __ mov(StoreDescriptor::ValueRegister(), result_register());
      PopOperands(StoreDescriptor::ReceiverRegister(),
                  StoreDescriptor::NameRegister());
      CallKeyedStoreIC(expr->CountSlot());
      PrepareForBailoutForId(expr->AssignmentId(), BailoutState::TOS_REGISTER);
      if (expr->is_postfix()) {
        if (!context()->IsEffect()) {
          context()->PlugTOS();
        }
      } else {
        context()->Plug(v0);
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
  context()->PrepareTest(&materialize_true, &materialize_false,
                         &if_true, &if_false, &fall_through);

  { AccumulatorValueContext context(this);
    VisitForTypeofValue(sub_expr);
  }
  PrepareForBailoutBeforeSplit(expr, true, if_true, if_false);

  Factory* factory = isolate()->factory();
  if (String::Equals(check, factory->number_string())) {
    __ JumpIfSmi(v0, if_true);
    __ lw(v0, FieldMemOperand(v0, HeapObject::kMapOffset));
    __ LoadRoot(at, Heap::kHeapNumberMapRootIndex);
    Split(eq, v0, Operand(at), if_true, if_false, fall_through);
  } else if (String::Equals(check, factory->string_string())) {
    __ JumpIfSmi(v0, if_false);
    __ GetObjectType(v0, v0, a1);
    Split(lt, a1, Operand(FIRST_NONSTRING_TYPE), if_true, if_false,
          fall_through);
  } else if (String::Equals(check, factory->symbol_string())) {
    __ JumpIfSmi(v0, if_false);
    __ GetObjectType(v0, v0, a1);
    Split(eq, a1, Operand(SYMBOL_TYPE), if_true, if_false, fall_through);
  } else if (String::Equals(check, factory->boolean_string())) {
    __ LoadRoot(at, Heap::kTrueValueRootIndex);
    __ Branch(if_true, eq, v0, Operand(at));
    __ LoadRoot(at, Heap::kFalseValueRootIndex);
    Split(eq, v0, Operand(at), if_true, if_false, fall_through);
  } else if (String::Equals(check, factory->undefined_string())) {
    __ LoadRoot(at, Heap::kNullValueRootIndex);
    __ Branch(if_false, eq, v0, Operand(at));
    __ JumpIfSmi(v0, if_false);
    // Check for undetectable objects => true.
    __ lw(v0, FieldMemOperand(v0, HeapObject::kMapOffset));
    __ lbu(a1, FieldMemOperand(v0, Map::kBitFieldOffset));
    __ And(a1, a1, Operand(1 << Map::kIsUndetectable));
    Split(ne, a1, Operand(zero_reg), if_true, if_false, fall_through);
  } else if (String::Equals(check, factory->function_string())) {
    __ JumpIfSmi(v0, if_false);
    __ lw(v0, FieldMemOperand(v0, HeapObject::kMapOffset));
    __ lbu(a1, FieldMemOperand(v0, Map::kBitFieldOffset));
    __ And(a1, a1,
           Operand((1 << Map::kIsCallable) | (1 << Map::kIsUndetectable)));
    Split(eq, a1, Operand(1 << Map::kIsCallable), if_true, if_false,
          fall_through);
  } else if (String::Equals(check, factory->object_string())) {
    __ JumpIfSmi(v0, if_false);
    __ LoadRoot(at, Heap::kNullValueRootIndex);
    __ Branch(if_true, eq, v0, Operand(at));
    STATIC_ASSERT(LAST_JS_RECEIVER_TYPE == LAST_TYPE);
    __ GetObjectType(v0, v0, a1);
    __ Branch(if_false, lt, a1, Operand(FIRST_JS_RECEIVER_TYPE));
    // Check for callable or undetectable objects => false.
    __ lbu(a1, FieldMemOperand(v0, Map::kBitFieldOffset));
    __ And(a1, a1,
           Operand((1 << Map::kIsCallable) | (1 << Map::kIsUndetectable)));
    Split(eq, a1, Operand(zero_reg), if_true, if_false, fall_through);
// clang-format off
#define SIMD128_TYPE(TYPE, Type, type, lane_count, lane_type)    \
  } else if (String::Equals(check, factory->type##_string())) {  \
    __ JumpIfSmi(v0, if_false);                                  \
    __ lw(v0, FieldMemOperand(v0, HeapObject::kMapOffset));      \
    __ LoadRoot(at, Heap::k##Type##MapRootIndex);                \
    Split(eq, v0, Operand(at), if_true, if_false, fall_through);
  SIMD128_TYPES(SIMD128_TYPE)
#undef SIMD128_TYPE
    // clang-format on
  } else {
    if (if_false != fall_through) __ jmp(if_false);
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
  context()->PrepareTest(&materialize_true, &materialize_false,
                         &if_true, &if_false, &fall_through);

  Token::Value op = expr->op();
  VisitForStackValue(expr->left());
  switch (op) {
    case Token::IN:
      VisitForStackValue(expr->right());
      SetExpressionPosition(expr);
      EmitHasProperty();
      PrepareForBailoutBeforeSplit(expr, false, NULL, NULL);
      __ LoadRoot(t0, Heap::kTrueValueRootIndex);
      Split(eq, v0, Operand(t0), if_true, if_false, fall_through);
      break;

    case Token::INSTANCEOF: {
      VisitForAccumulatorValue(expr->right());
      SetExpressionPosition(expr);
      __ mov(a0, result_register());
      PopOperand(a1);
      InstanceOfStub stub(isolate());
      __ CallStub(&stub);
      PrepareForBailoutBeforeSplit(expr, false, NULL, NULL);
      __ LoadRoot(at, Heap::kTrueValueRootIndex);
      Split(eq, v0, Operand(at), if_true, if_false, fall_through);
      break;
    }

    default: {
      VisitForAccumulatorValue(expr->right());
      SetExpressionPosition(expr);
      Condition cc = CompareIC::ComputeCondition(op);
      __ mov(a0, result_register());
      PopOperand(a1);

      bool inline_smi_code = ShouldInlineSmiCase(op);
      JumpPatchSite patch_site(masm_);
      if (inline_smi_code) {
        Label slow_case;
        __ Or(a2, a0, Operand(a1));
        patch_site.EmitJumpIfNotSmi(a2, &slow_case);
        Split(cc, a1, Operand(a0), if_true, if_false, NULL);
        __ bind(&slow_case);
      }

      Handle<Code> ic = CodeFactory::CompareIC(isolate(), op).code();
      CallIC(ic, expr->CompareOperationFeedbackId());
      patch_site.EmitPatchInfo();
      PrepareForBailoutBeforeSplit(expr, true, if_true, if_false);
      Split(cc, v0, Operand(zero_reg), if_true, if_false, fall_through);
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
  context()->PrepareTest(&materialize_true, &materialize_false,
                         &if_true, &if_false, &fall_through);

  VisitForAccumulatorValue(sub_expr);
  PrepareForBailoutBeforeSplit(expr, true, if_true, if_false);
  if (expr->op() == Token::EQ_STRICT) {
    Heap::RootListIndex nil_value = nil == kNullValue ?
        Heap::kNullValueRootIndex :
        Heap::kUndefinedValueRootIndex;
    __ LoadRoot(a1, nil_value);
    Split(eq, v0, Operand(a1), if_true, if_false, fall_through);
  } else {
    __ JumpIfSmi(v0, if_false);
    __ lw(v0, FieldMemOperand(v0, HeapObject::kMapOffset));
    __ lbu(a1, FieldMemOperand(v0, Map::kBitFieldOffset));
    __ And(a1, a1, Operand(1 << Map::kIsUndetectable));
    Split(ne, a1, Operand(zero_reg), if_true, if_false, fall_through);
  }
  context()->Plug(if_true, if_false);
}


Register FullCodeGenerator::result_register() {
  return v0;
}


Register FullCodeGenerator::context_register() {
  return cp;
}

void FullCodeGenerator::LoadFromFrameField(int frame_offset, Register value) {
  DCHECK_EQ(POINTER_SIZE_ALIGN(frame_offset), frame_offset);
  __ lw(value, MemOperand(fp, frame_offset));
}

void FullCodeGenerator::StoreToFrameField(int frame_offset, Register value) {
  DCHECK_EQ(POINTER_SIZE_ALIGN(frame_offset), frame_offset);
  __ sw(value, MemOperand(fp, frame_offset));
}


void FullCodeGenerator::LoadContextField(Register dst, int context_index) {
  __ lw(dst, ContextMemOperand(cp, context_index));
}


void FullCodeGenerator::PushFunctionArgumentForContextAllocation() {
  DeclarationScope* closure_scope = scope()->GetClosureScope();
  if (closure_scope->is_script_scope() ||
      closure_scope->is_module_scope()) {
    // Contexts nested in the native context have a canonical empty function
    // as their closure, not the anonymous closure containing the global
    // code.
    __ LoadNativeContextSlot(Context::CLOSURE_INDEX, at);
  } else if (closure_scope->is_eval_scope()) {
    // Contexts created by a call to eval have the same closure as the
    // context calling eval, not the anonymous closure containing the eval
    // code.  Fetch it from the context.
    __ lw(at, ContextMemOperand(cp, Context::CLOSURE_INDEX));
  } else {
    DCHECK(closure_scope->is_function_scope());
    __ lw(at, MemOperand(fp, JavaScriptFrameConstants::kFunctionOffset));
  }
  PushOperand(at);
}


// ----------------------------------------------------------------------------
// Non-local control flow support.

void FullCodeGenerator::EnterFinallyBlock() {
  DCHECK(!result_register().is(a1));
  // Store pending message while executing finally block.
  ExternalReference pending_message_obj =
      ExternalReference::address_of_pending_message_obj(isolate());
  __ li(at, Operand(pending_message_obj));
  __ lw(a1, MemOperand(at));
  PushOperand(a1);

  ClearPendingMessage();
}


void FullCodeGenerator::ExitFinallyBlock() {
  DCHECK(!result_register().is(a1));
  // Restore pending message from stack.
  PopOperand(a1);
  ExternalReference pending_message_obj =
      ExternalReference::address_of_pending_message_obj(isolate());
  __ li(at, Operand(pending_message_obj));
  __ sw(a1, MemOperand(at));
}


void FullCodeGenerator::ClearPendingMessage() {
  DCHECK(!result_register().is(a1));
  ExternalReference pending_message_obj =
      ExternalReference::address_of_pending_message_obj(isolate());
  __ LoadRoot(a1, Heap::kTheHoleValueRootIndex);
  __ li(at, Operand(pending_message_obj));
  __ sw(a1, MemOperand(at));
}


void FullCodeGenerator::DeferredCommands::EmitCommands() {
  DCHECK(!result_register().is(a1));
  __ Pop(result_register());  // Restore the accumulator.
  __ Pop(a1);                 // Get the token.
  for (DeferredCommand cmd : commands_) {
    Label skip;
    __ li(at, Operand(Smi::FromInt(cmd.token)));
    __ Branch(&skip, ne, a1, Operand(at));
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


void BackEdgeTable::PatchAt(Code* unoptimized_code,
                            Address pc,
                            BackEdgeState target_state,
                            Code* replacement_code) {
  static const int kInstrSize = Assembler::kInstrSize;
  Address pc_immediate_load_address =
      Assembler::target_address_from_return_address(pc);
  Address branch_address = pc_immediate_load_address - 2 * kInstrSize;
  Isolate* isolate = unoptimized_code->GetIsolate();
  CodePatcher patcher(isolate, branch_address, 1);

  switch (target_state) {
    case INTERRUPT:
      // slt at, a3, zero_reg (in case of count based interrupts)
      // beq at, zero_reg, ok
      // lui t9, <interrupt stub address> upper
      // ori t9, <interrupt stub address> lower
      // jalr t9
      // nop
      // ok-label ----- pc_after points here
      patcher.masm()->slt(at, a3, zero_reg);
      break;
    case ON_STACK_REPLACEMENT:
      // addiu at, zero_reg, 1
      // beq at, zero_reg, ok  ;; Not changed
      // lui t9, <on-stack replacement address> upper
      // ori t9, <on-stack replacement address> lower
      // jalr t9  ;; Not changed
      // nop  ;; Not changed
      // ok-label ----- pc_after points here
      patcher.masm()->addiu(at, zero_reg, 1);
      break;
  }
  // Replace the stack check address in the load-immediate (lui/ori pair)
  // with the entry address of the replacement code.
  Assembler::set_target_address_at(isolate, pc_immediate_load_address,
                                   replacement_code->entry());

  unoptimized_code->GetHeap()->incremental_marking()->RecordCodeTargetPatch(
      unoptimized_code, pc_immediate_load_address, replacement_code);
}


BackEdgeTable::BackEdgeState BackEdgeTable::GetBackEdgeState(
    Isolate* isolate,
    Code* unoptimized_code,
    Address pc) {
  static const int kInstrSize = Assembler::kInstrSize;
  Address pc_immediate_load_address =
      Assembler::target_address_from_return_address(pc);
  Address branch_address = pc_immediate_load_address - 2 * kInstrSize;

  DCHECK(Assembler::IsBeq(Assembler::instr_at(branch_address + kInstrSize)));
  if (!Assembler::IsAddImmediate(Assembler::instr_at(branch_address))) {
    DCHECK(reinterpret_cast<uint32_t>(
        Assembler::target_address_at(pc_immediate_load_address)) ==
           reinterpret_cast<uint32_t>(
               isolate->builtins()->InterruptCheck()->entry()));
    return INTERRUPT;
  }

  DCHECK(Assembler::IsAddImmediate(Assembler::instr_at(branch_address)));

  DCHECK(reinterpret_cast<uint32_t>(
             Assembler::target_address_at(pc_immediate_load_address)) ==
         reinterpret_cast<uint32_t>(
             isolate->builtins()->OnStackReplacement()->entry()));
  return ON_STACK_REPLACEMENT;
}


}  // namespace internal
}  // namespace v8

#endif  // V8_TARGET_ARCH_MIPS
