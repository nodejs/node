// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if V8_TARGET_ARCH_X87

#include "src/ast/compile-time-value.h"
#include "src/ast/scopes.h"
#include "src/builtins/builtins-constructor.h"
#include "src/code-factory.h"
#include "src/code-stubs.h"
#include "src/codegen.h"
#include "src/compilation-info.h"
#include "src/compiler.h"
#include "src/debug/debug.h"
#include "src/full-codegen/full-codegen.h"
#include "src/ic/ic.h"
#include "src/x87/frames-x87.h"

namespace v8 {
namespace internal {

#define __ ACCESS_MASM(masm())

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

  void EmitJumpIfNotSmi(Register reg,
                        Label* target,
                        Label::Distance distance = Label::kFar) {
    __ test(reg, Immediate(kSmiTagMask));
    EmitJump(not_carry, target, distance);  // Always taken before patched.
  }

  void EmitJumpIfSmi(Register reg,
                     Label* target,
                     Label::Distance distance = Label::kFar) {
    __ test(reg, Immediate(kSmiTagMask));
    EmitJump(carry, target, distance);  // Never taken before patched.
  }

  void EmitPatchInfo() {
    if (patch_site_.is_bound()) {
      int delta_to_patch_site = masm_->SizeOfCodeGeneratedSince(&patch_site_);
      DCHECK(is_uint8(delta_to_patch_site));
      __ test(eax, Immediate(delta_to_patch_site));
#ifdef DEBUG
      info_emitted_ = true;
#endif
    } else {
      __ nop();  // Signals no inlined code.
    }
  }

 private:
  // jc will be patched with jz, jnc will become jnz.
  void EmitJump(Condition cc, Label* target, Label::Distance distance) {
    DCHECK(!patch_site_.is_bound() && !info_emitted_);
    DCHECK(cc == carry || cc == not_carry);
    __ bind(&patch_site_);
    __ j(cc, target, distance);
  }

  MacroAssembler* masm() { return masm_; }
  MacroAssembler* masm_;
  Label patch_site_;
#ifdef DEBUG
  bool info_emitted_;
#endif
};


// Generate code for a JS function.  On entry to the function the receiver
// and arguments have been pushed on the stack left to right, with the
// return address on top of them.  The actual argument count matches the
// formal parameter count expected by the function.
//
// The live registers are:
//   o edi: the JS function object being called (i.e. ourselves)
//   o edx: the new target value
//   o esi: our context
//   o ebp: our caller's frame pointer
//   o esp: stack pointer (pointing to return address)
//
// The function builds a JS frame.  Please see JavaScriptFrameConstants in
// frames-x87.h for its layout.
void FullCodeGenerator::Generate() {
  CompilationInfo* info = info_;
  profiling_counter_ = isolate()->factory()->NewCell(
      Handle<Smi>(Smi::FromInt(FLAG_interrupt_budget), isolate()));
  SetFunctionPosition(literal());
  Comment cmnt(masm_, "[ function compiled by full code generator");

  ProfileEntryHookStub::MaybeCallEntryHook(masm_);

  if (FLAG_debug_code && info->ExpectsJSReceiverAsReceiver()) {
    int receiver_offset = (info->scope()->num_parameters() + 1) * kPointerSize;
    __ mov(ecx, Operand(esp, receiver_offset));
    __ AssertNotSmi(ecx);
    __ CmpObjectType(ecx, FIRST_JS_RECEIVER_TYPE, ecx);
    __ Assert(above_equal, kSloppyFunctionExpectsJSReceiverReceiver);
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
    __ mov(ecx, FieldOperand(edi, JSFunction::kLiteralsOffset));
    __ mov(ecx, FieldOperand(ecx, LiteralsArray::kFeedbackVectorOffset));
    __ add(FieldOperand(
               ecx, FeedbackVector::kInvocationCountIndex * kPointerSize +
                        FeedbackVector::kHeaderSize),
           Immediate(Smi::FromInt(1)));
  }

  { Comment cmnt(masm_, "[ Allocate locals");
    int locals_count = info->scope()->num_stack_slots();
    OperandStackDepthIncrement(locals_count);
    if (locals_count == 1) {
      __ push(Immediate(isolate()->factory()->undefined_value()));
    } else if (locals_count > 1) {
      if (locals_count >= 128) {
        Label ok;
        __ mov(ecx, esp);
        __ sub(ecx, Immediate(locals_count * kPointerSize));
        ExternalReference stack_limit =
            ExternalReference::address_of_real_stack_limit(isolate());
        __ cmp(ecx, Operand::StaticVariable(stack_limit));
        __ j(above_equal, &ok, Label::kNear);
        __ CallRuntime(Runtime::kThrowStackOverflow);
        __ bind(&ok);
      }
      __ mov(eax, Immediate(isolate()->factory()->undefined_value()));
      const int kMaxPushes = 32;
      if (locals_count >= kMaxPushes) {
        int loop_iterations = locals_count / kMaxPushes;
        __ mov(ecx, loop_iterations);
        Label loop_header;
        __ bind(&loop_header);
        // Do pushes.
        for (int i = 0; i < kMaxPushes; i++) {
          __ push(eax);
        }
        __ dec(ecx);
        __ j(not_zero, &loop_header, Label::kNear);
      }
      int remaining = locals_count % kMaxPushes;
      // Emit the remaining pushes.
      for (int i  = 0; i < remaining; i++) {
        __ push(eax);
      }
    }
  }

  bool function_in_register = true;

  // Possibly allocate a local context.
  if (info->scope()->NeedsContext()) {
    Comment cmnt(masm_, "[ Allocate context");
    bool need_write_barrier = true;
    int slots = info->scope()->num_heap_slots() - Context::MIN_CONTEXT_SLOTS;
    // Argument to NewContext is the function, which is still in edi.
    if (info->scope()->is_script_scope()) {
      __ push(edi);
      __ Push(info->scope()->scope_info());
      __ CallRuntime(Runtime::kNewScriptContext);
      PrepareForBailoutForId(BailoutId::ScriptContext(),
                             BailoutState::TOS_REGISTER);
      // The new target value is not used, clobbering is safe.
      DCHECK_NULL(info->scope()->new_target_var());
    } else {
      if (info->scope()->new_target_var() != nullptr) {
        __ push(edx);  // Preserve new target.
      }
      if (slots <=
          ConstructorBuiltinsAssembler::MaximumFunctionContextSlots()) {
        Callable callable = CodeFactory::FastNewFunctionContext(
            isolate(), info->scope()->scope_type());
        __ mov(FastNewFunctionContextDescriptor::SlotsRegister(),
               Immediate(slots));
        __ Call(callable.code(), RelocInfo::CODE_TARGET);
        // Result of the FastNewFunctionContext builtin is always in new space.
        need_write_barrier = false;
      } else {
        __ push(edi);
        __ Push(Smi::FromInt(info->scope()->scope_type()));
        __ CallRuntime(Runtime::kNewFunctionContext);
      }
      if (info->scope()->new_target_var() != nullptr) {
        __ pop(edx);  // Restore new target.
      }
    }
    function_in_register = false;
    // Context is returned in eax.  It replaces the context passed to us.
    // It's saved in the stack and kept live in esi.
    __ mov(esi, eax);
    __ mov(Operand(ebp, StandardFrameConstants::kContextOffset), eax);

    // Copy parameters into context if necessary.
    int num_parameters = info->scope()->num_parameters();
    int first_parameter = info->scope()->has_this_declaration() ? -1 : 0;
    for (int i = first_parameter; i < num_parameters; i++) {
      Variable* var =
          (i == -1) ? info->scope()->receiver() : info->scope()->parameter(i);
      if (var->IsContextSlot()) {
        int parameter_offset = StandardFrameConstants::kCallerSPOffset +
            (num_parameters - 1 - i) * kPointerSize;
        // Load parameter from stack.
        __ mov(eax, Operand(ebp, parameter_offset));
        // Store it in the context.
        int context_offset = Context::SlotOffset(var->index());
        __ mov(Operand(esi, context_offset), eax);
        // Update the write barrier. This clobbers eax and ebx.
        if (need_write_barrier) {
          __ RecordWriteContextSlot(esi, context_offset, eax, ebx,
                                    kDontSaveFPRegs);
        } else if (FLAG_debug_code) {
          Label done;
          __ JumpIfInNewSpace(esi, eax, &done, Label::kNear);
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

  // We don't support new.target and rest parameters here.
  DCHECK_NULL(info->scope()->new_target_var());
  DCHECK_NULL(info->scope()->rest_parameter());
  DCHECK_NULL(info->scope()->this_function_var());

  Variable* arguments = info->scope()->arguments();
  if (arguments != NULL) {
    // Arguments object must be allocated after the context object, in
    // case the "arguments" or ".arguments" variables are in the context.
    Comment cmnt(masm_, "[ Allocate arguments object");
    if (!function_in_register) {
      __ mov(edi, Operand(ebp, JavaScriptFrameConstants::kFunctionOffset));
    }
    if (is_strict(language_mode()) || !has_simple_parameters()) {
      FastNewStrictArgumentsStub stub(isolate());
      __ CallStub(&stub);
    } else if (literal()->has_duplicate_parameters()) {
      __ Push(edi);
      __ CallRuntime(Runtime::kNewSloppyArguments_Generic);
    } else {
      FastNewSloppyArgumentsStub stub(isolate());
      __ CallStub(&stub);
    }

    SetVar(arguments, eax, ebx, edx);
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
    ExternalReference stack_limit =
        ExternalReference::address_of_stack_limit(isolate());
    __ cmp(esp, Operand::StaticVariable(stack_limit));
    __ j(above_equal, &ok, Label::kNear);
    __ call(isolate()->builtins()->StackCheck(), RelocInfo::CODE_TARGET);
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
    __ mov(eax, isolate()->factory()->undefined_value());
    EmitReturnSequence();
  }
}


void FullCodeGenerator::ClearAccumulator() {
  __ Move(eax, Immediate(Smi::kZero));
}


void FullCodeGenerator::EmitProfilingCounterDecrement(int delta) {
  __ mov(ebx, Immediate(profiling_counter_));
  __ sub(FieldOperand(ebx, Cell::kValueOffset),
         Immediate(Smi::FromInt(delta)));
}


void FullCodeGenerator::EmitProfilingCounterReset() {
  int reset_value = FLAG_interrupt_budget;
  __ mov(ebx, Immediate(profiling_counter_));
  __ mov(FieldOperand(ebx, Cell::kValueOffset),
         Immediate(Smi::FromInt(reset_value)));
}


void FullCodeGenerator::EmitBackEdgeBookkeeping(IterationStatement* stmt,
                                                Label* back_edge_target) {
  Comment cmnt(masm_, "[ Back edge bookkeeping");
  Label ok;

  DCHECK(back_edge_target->is_bound());
  int distance = masm_->SizeOfCodeGeneratedSince(back_edge_target);
  int weight = Min(kMaxBackEdgeWeight,
                   Max(1, distance / kCodeSizeMultiplier));
  EmitProfilingCounterDecrement(weight);
  __ j(positive, &ok, Label::kNear);
  __ call(isolate()->builtins()->InterruptCheck(), RelocInfo::CODE_TARGET);

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
  __ j(positive, &ok, Label::kNear);
  // Don't need to save result register if we are going to do a tail call.
  if (!is_tail_call) {
    __ push(eax);
  }
  __ call(isolate()->builtins()->InterruptCheck(), RelocInfo::CODE_TARGET);
  if (!is_tail_call) {
    __ pop(eax);
  }
  EmitProfilingCounterReset();
  __ bind(&ok);
}

void FullCodeGenerator::EmitReturnSequence() {
  Comment cmnt(masm_, "[ Return sequence");
  if (return_label_.is_bound()) {
    __ jmp(&return_label_);
  } else {
    // Common return label
    __ bind(&return_label_);
    if (FLAG_trace) {
      __ push(eax);
      __ CallRuntime(Runtime::kTraceExit);
    }
    EmitProfilingCounterHandlingForReturnSequence(false);

    SetReturnPosition(literal());
    __ leave();

    int arg_count = info_->scope()->num_parameters() + 1;
    int arguments_bytes = arg_count * kPointerSize;
    __ Ret(arguments_bytes, ecx);
  }
}

void FullCodeGenerator::RestoreContext() {
  __ mov(esi, Operand(ebp, StandardFrameConstants::kContextOffset));
}

void FullCodeGenerator::StackValueContext::Plug(Variable* var) const {
  DCHECK(var->IsStackAllocated() || var->IsContextSlot());
  MemOperand operand = codegen()->VarOperand(var, result_register());
  // Memory operands can be pushed directly.
  codegen()->PushOperand(operand);
}


void FullCodeGenerator::EffectContext::Plug(Heap::RootListIndex index) const {
  UNREACHABLE();  // Not used on X87.
}


void FullCodeGenerator::AccumulatorValueContext::Plug(
    Heap::RootListIndex index) const {
  UNREACHABLE();  // Not used on X87.
}


void FullCodeGenerator::StackValueContext::Plug(
    Heap::RootListIndex index) const {
  UNREACHABLE();  // Not used on X87.
}


void FullCodeGenerator::TestContext::Plug(Heap::RootListIndex index) const {
  UNREACHABLE();  // Not used on X87.
}


void FullCodeGenerator::EffectContext::Plug(Handle<Object> lit) const {
}


void FullCodeGenerator::AccumulatorValueContext::Plug(
    Handle<Object> lit) const {
  if (lit->IsSmi()) {
    __ SafeMove(result_register(), Immediate(lit));
  } else {
    __ Move(result_register(), Immediate(lit));
  }
}


void FullCodeGenerator::StackValueContext::Plug(Handle<Object> lit) const {
  codegen()->OperandStackDepthIncrement(1);
  if (lit->IsSmi()) {
    __ SafePush(Immediate(lit));
  } else {
    __ push(Immediate(lit));
  }
}


void FullCodeGenerator::TestContext::Plug(Handle<Object> lit) const {
  codegen()->PrepareForBailoutBeforeSplit(condition(),
                                          true,
                                          true_label_,
                                          false_label_);
  DCHECK(lit->IsNullOrUndefined(isolate()) || !lit->IsUndetectable());
  if (lit->IsNullOrUndefined(isolate()) || lit->IsFalse(isolate())) {
    if (false_label_ != fall_through_) __ jmp(false_label_);
  } else if (lit->IsTrue(isolate()) || lit->IsJSObject()) {
    if (true_label_ != fall_through_) __ jmp(true_label_);
  } else if (lit->IsString()) {
    if (String::cast(*lit)->length() == 0) {
      if (false_label_ != fall_through_) __ jmp(false_label_);
    } else {
      if (true_label_ != fall_through_) __ jmp(true_label_);
    }
  } else if (lit->IsSmi()) {
    if (Smi::cast(*lit)->value() == 0) {
      if (false_label_ != fall_through_) __ jmp(false_label_);
    } else {
      if (true_label_ != fall_through_) __ jmp(true_label_);
    }
  } else {
    // For simplicity we always test the accumulator register.
    __ mov(result_register(), lit);
    codegen()->DoTest(this);
  }
}


void FullCodeGenerator::StackValueContext::DropAndPlug(int count,
                                                       Register reg) const {
  DCHECK(count > 0);
  if (count > 1) codegen()->DropOperands(count - 1);
  __ mov(Operand(esp, 0), reg);
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
  __ mov(result_register(), isolate()->factory()->true_value());
  __ jmp(&done, Label::kNear);
  __ bind(materialize_false);
  __ mov(result_register(), isolate()->factory()->false_value());
  __ bind(&done);
}


void FullCodeGenerator::StackValueContext::Plug(
    Label* materialize_true,
    Label* materialize_false) const {
  codegen()->OperandStackDepthIncrement(1);
  Label done;
  __ bind(materialize_true);
  __ push(Immediate(isolate()->factory()->true_value()));
  __ jmp(&done, Label::kNear);
  __ bind(materialize_false);
  __ push(Immediate(isolate()->factory()->false_value()));
  __ bind(&done);
}


void FullCodeGenerator::TestContext::Plug(Label* materialize_true,
                                          Label* materialize_false) const {
  DCHECK(materialize_true == true_label_);
  DCHECK(materialize_false == false_label_);
}


void FullCodeGenerator::AccumulatorValueContext::Plug(bool flag) const {
  Handle<Object> value = flag
      ? isolate()->factory()->true_value()
      : isolate()->factory()->false_value();
  __ mov(result_register(), value);
}


void FullCodeGenerator::StackValueContext::Plug(bool flag) const {
  codegen()->OperandStackDepthIncrement(1);
  Handle<Object> value = flag
      ? isolate()->factory()->true_value()
      : isolate()->factory()->false_value();
  __ push(Immediate(value));
}


void FullCodeGenerator::TestContext::Plug(bool flag) const {
  codegen()->PrepareForBailoutBeforeSplit(condition(),
                                          true,
                                          true_label_,
                                          false_label_);
  if (flag) {
    if (true_label_ != fall_through_) __ jmp(true_label_);
  } else {
    if (false_label_ != fall_through_) __ jmp(false_label_);
  }
}


void FullCodeGenerator::DoTest(Expression* condition,
                               Label* if_true,
                               Label* if_false,
                               Label* fall_through) {
  Handle<Code> ic = ToBooleanICStub::GetUninitialized(isolate());
  CallIC(ic, condition->test_id());
  __ CompareRoot(result_register(), Heap::kTrueValueRootIndex);
  Split(equal, if_true, if_false, fall_through);
}


void FullCodeGenerator::Split(Condition cc,
                              Label* if_true,
                              Label* if_false,
                              Label* fall_through) {
  if (if_false == fall_through) {
    __ j(cc, if_true);
  } else if (if_true == fall_through) {
    __ j(NegateCondition(cc), if_false);
  } else {
    __ j(cc, if_true);
    __ jmp(if_false);
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
  return Operand(ebp, offset);
}


MemOperand FullCodeGenerator::VarOperand(Variable* var, Register scratch) {
  DCHECK(var->IsContextSlot() || var->IsStackAllocated());
  if (var->IsContextSlot()) {
    int context_chain_length = scope()->ContextChainLength(var->scope());
    __ LoadContext(scratch, context_chain_length);
    return ContextOperand(scratch, var->index());
  } else {
    return StackOperand(var);
  }
}


void FullCodeGenerator::GetVar(Register dest, Variable* var) {
  DCHECK(var->IsContextSlot() || var->IsStackAllocated());
  MemOperand location = VarOperand(var, dest);
  __ mov(dest, location);
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
  __ mov(location, src);

  // Emit the write barrier code if the location is in the heap.
  if (var->IsContextSlot()) {
    int offset = Context::SlotOffset(var->index());
    DCHECK(!scratch0.is(esi) && !src.is(esi) && !scratch1.is(esi));
    __ RecordWriteContextSlot(scratch0, offset, src, scratch1, kDontSaveFPRegs);
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
  if (should_normalize) __ jmp(&skip, Label::kNear);
  PrepareForBailout(expr, BailoutState::TOS_REGISTER);
  if (should_normalize) {
    __ cmp(eax, isolate()->factory()->true_value());
    Split(equal, if_true, if_false, NULL);
    __ bind(&skip);
  }
}


void FullCodeGenerator::EmitDebugCheckDeclarationContext(Variable* variable) {
  // The variable in the declaration always resides in the current context.
  DCHECK_EQ(0, scope()->ContextChainLength(variable->scope()));
  if (FLAG_debug_code) {
    // Check that we're not inside a with or catch context.
    __ mov(ebx, FieldOperand(esi, HeapObject::kMapOffset));
    __ cmp(ebx, isolate()->factory()->with_context_map());
    __ Check(not_equal, kDeclarationInWithContext);
    __ cmp(ebx, isolate()->factory()->catch_context_map());
    __ Check(not_equal, kDeclarationInCatchContext);
  }
}


void FullCodeGenerator::VisitVariableDeclaration(
    VariableDeclaration* declaration) {
  VariableProxy* proxy = declaration->proxy();
  Variable* variable = proxy->var();
  switch (variable->location()) {
    case VariableLocation::UNALLOCATED: {
      DCHECK(!variable->binding_needs_init());
      globals_->Add(variable->name(), zone());
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
        __ mov(StackOperand(variable),
               Immediate(isolate()->factory()->the_hole_value()));
      }
      break;

    case VariableLocation::CONTEXT:
      if (variable->binding_needs_init()) {
        Comment cmnt(masm_, "[ VariableDeclaration");
        EmitDebugCheckDeclarationContext(variable);
        __ mov(ContextOperand(esi, variable->index()),
               Immediate(isolate()->factory()->the_hole_value()));
        // No write barrier since the hole value is in old space.
        PrepareForBailoutForId(proxy->id(), BailoutState::NO_REGISTERS);
      }
      break;

    case VariableLocation::LOOKUP:
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
      globals_->Add(variable->name(), zone());
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
      __ mov(StackOperand(variable), result_register());
      break;
    }

    case VariableLocation::CONTEXT: {
      Comment cmnt(masm_, "[ FunctionDeclaration");
      EmitDebugCheckDeclarationContext(variable);
      VisitForAccumulatorValue(declaration->fun());
      __ mov(ContextOperand(esi, variable->index()), result_register());
      // We know that we have written a function, which is not a smi.
      __ RecordWriteContextSlot(esi, Context::SlotOffset(variable->index()),
                                result_register(), ecx, kDontSaveFPRegs,
                                EMIT_REMEMBERED_SET, OMIT_SMI_CHECK);
      PrepareForBailoutForId(proxy->id(), BailoutState::NO_REGISTERS);
      break;
    }

    case VariableLocation::LOOKUP:
    case VariableLocation::MODULE:
      UNREACHABLE();
  }
}


void FullCodeGenerator::DeclareGlobals(Handle<FixedArray> pairs) {
  // Call the runtime to declare the globals.
  __ Push(pairs);
  __ Push(Smi::FromInt(DeclareGlobalsFlags()));
  __ EmitLoadFeedbackVector(eax);
  __ Push(eax);
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
    __ mov(edx, Operand(esp, 0));  // Switch value.
    bool inline_smi_code = ShouldInlineSmiCase(Token::EQ_STRICT);
    JumpPatchSite patch_site(masm_);
    if (inline_smi_code) {
      Label slow_case;
      __ mov(ecx, edx);
      __ or_(ecx, eax);
      patch_site.EmitJumpIfNotSmi(ecx, &slow_case, Label::kNear);

      __ cmp(edx, eax);
      __ j(not_equal, &next_test);
      __ Drop(1);  // Switch value is no longer needed.
      __ jmp(clause->body_target());
      __ bind(&slow_case);
    }

    SetExpressionPosition(clause);
    Handle<Code> ic =
        CodeFactory::CompareIC(isolate(), Token::EQ_STRICT).code();
    CallIC(ic, clause->CompareId());
    patch_site.EmitPatchInfo();

    Label skip;
    __ jmp(&skip, Label::kNear);
    PrepareForBailout(clause, BailoutState::TOS_REGISTER);
    __ cmp(eax, isolate()->factory()->true_value());
    __ j(not_equal, &next_test);
    __ Drop(1);
    __ jmp(clause->body_target());
    __ bind(&skip);

    __ test(eax, eax);
    __ j(not_equal, &next_test);
    __ Drop(1);  // Switch value is no longer needed.
    __ jmp(clause->body_target());
  }

  // Discard the test value and jump to the default if present, otherwise to
  // the end of the statement.
  __ bind(&next_test);
  DropOperands(1);  // Switch value is no longer needed.
  if (default_clause == NULL) {
    __ jmp(nested_statement.break_label());
  } else {
    __ jmp(default_clause->body_target());
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
  __ JumpIfSmi(eax, &convert, Label::kNear);
  __ CmpObjectType(eax, FIRST_JS_RECEIVER_TYPE, ecx);
  __ j(above_equal, &done_convert, Label::kNear);
  __ cmp(eax, isolate()->factory()->undefined_value());
  __ j(equal, &exit);
  __ cmp(eax, isolate()->factory()->null_value());
  __ j(equal, &exit);
  __ bind(&convert);
  __ Call(isolate()->builtins()->ToObject(), RelocInfo::CODE_TARGET);
  RestoreContext();
  __ bind(&done_convert);
  PrepareForBailoutForId(stmt->ToObjectId(), BailoutState::TOS_REGISTER);
  __ push(eax);

  // Check cache validity in generated code. If we cannot guarantee cache
  // validity, call the runtime system to check cache validity or get the
  // property names in a fixed array. Note: Proxies never have an enum cache,
  // so will always take the slow path.
  Label call_runtime, use_cache, fixed_array;
  __ CheckEnumCache(&call_runtime);

  __ mov(eax, FieldOperand(eax, HeapObject::kMapOffset));
  __ jmp(&use_cache, Label::kNear);

  // Get the set of properties to enumerate.
  __ bind(&call_runtime);
  __ push(eax);
  __ CallRuntime(Runtime::kForInEnumerate);
  PrepareForBailoutForId(stmt->EnumId(), BailoutState::TOS_REGISTER);
  __ cmp(FieldOperand(eax, HeapObject::kMapOffset),
         isolate()->factory()->meta_map());
  __ j(not_equal, &fixed_array);


  // We got a map in register eax. Get the enumeration cache from it.
  Label no_descriptors;
  __ bind(&use_cache);

  __ EnumLength(edx, eax);
  __ cmp(edx, Immediate(Smi::kZero));
  __ j(equal, &no_descriptors);

  __ LoadInstanceDescriptors(eax, ecx);
  __ mov(ecx, FieldOperand(ecx, DescriptorArray::kEnumCacheOffset));
  __ mov(ecx, FieldOperand(ecx, DescriptorArray::kEnumCacheBridgeCacheOffset));

  // Set up the four remaining stack slots.
  __ push(eax);  // Map.
  __ push(ecx);  // Enumeration cache.
  __ push(edx);  // Number of valid entries for the map in the enum cache.
  __ push(Immediate(Smi::kZero));  // Initial index.
  __ jmp(&loop);

  __ bind(&no_descriptors);
  __ add(esp, Immediate(kPointerSize));
  __ jmp(&exit);

  // We got a fixed array in register eax. Iterate through that.
  __ bind(&fixed_array);

  __ push(Immediate(Smi::FromInt(1)));  // Smi(1) indicates slow check
  __ push(eax);  // Array
  __ mov(eax, FieldOperand(eax, FixedArray::kLengthOffset));
  __ push(eax);  // Fixed array length (as smi).
  PrepareForBailoutForId(stmt->PrepareId(), BailoutState::NO_REGISTERS);
  __ push(Immediate(Smi::kZero));  // Initial index.

  // Generate code for doing the condition check.
  __ bind(&loop);
  SetExpressionAsStatementPosition(stmt->each());

  __ mov(eax, Operand(esp, 0 * kPointerSize));  // Get the current index.
  __ cmp(eax, Operand(esp, 1 * kPointerSize));  // Compare to the array length.
  __ j(above_equal, loop_statement.break_label());

  // Get the current entry of the array into register eax.
  __ mov(ebx, Operand(esp, 2 * kPointerSize));
  __ mov(eax, FieldOperand(ebx, eax, times_2, FixedArray::kHeaderSize));

  // Get the expected map from the stack or a smi in the
  // permanent slow case into register edx.
  __ mov(edx, Operand(esp, 3 * kPointerSize));

  // Check if the expected map still matches that of the enumerable.
  // If not, we may have to filter the key.
  Label update_each;
  __ mov(ebx, Operand(esp, 4 * kPointerSize));
  __ cmp(edx, FieldOperand(ebx, HeapObject::kMapOffset));
  __ j(equal, &update_each, Label::kNear);

  // We need to filter the key, record slow-path here.
  int const vector_index = SmiFromSlot(slot)->value();
  __ EmitLoadFeedbackVector(edx);
  __ mov(FieldOperand(edx, FixedArray::OffsetOfElementAt(vector_index)),
         Immediate(FeedbackVector::MegamorphicSentinel(isolate())));

  // eax contains the key.  The receiver in ebx is the second argument to the
  // ForInFilter.  ForInFilter returns undefined if the receiver doesn't
  // have the key or returns the name-converted key.
  __ Call(isolate()->builtins()->ForInFilter(), RelocInfo::CODE_TARGET);
  RestoreContext();
  PrepareForBailoutForId(stmt->FilterId(), BailoutState::TOS_REGISTER);
  __ JumpIfRoot(result_register(), Heap::kUndefinedValueRootIndex,
                loop_statement.continue_label());

  // Update the 'each' property or variable from the possibly filtered
  // entry in register eax.
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

  // Generate code for going to the next element by incrementing the
  // index (smi) stored on top of the stack.
  __ bind(loop_statement.continue_label());
  PrepareForBailoutForId(stmt->IncrementId(), BailoutState::NO_REGISTERS);
  __ add(Operand(esp, 0 * kPointerSize), Immediate(Smi::FromInt(1)));

  EmitBackEdgeBookkeeping(stmt, &loop);
  __ jmp(&loop);

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
  __ mov(StoreDescriptor::ReceiverRegister(), Operand(esp, 0));
  __ mov(StoreDescriptor::ValueRegister(), Operand(esp, offset * kPointerSize));
  CallStoreIC(slot, isolate()->factory()->home_object_symbol());
}


void FullCodeGenerator::EmitSetHomeObjectAccumulator(Expression* initializer,
                                                     int offset,
                                                     FeedbackVectorSlot slot) {
  DCHECK(NeedsHomeObject(initializer));
  __ mov(StoreDescriptor::ReceiverRegister(), eax);
  __ mov(StoreDescriptor::ValueRegister(), Operand(esp, offset * kPointerSize));
  CallStoreIC(slot, isolate()->factory()->home_object_symbol());
}

void FullCodeGenerator::EmitVariableLoad(VariableProxy* proxy,
                                         TypeofMode typeof_mode) {
  SetExpressionPosition(proxy);
  PrepareForBailoutForId(proxy->BeforeId(), BailoutState::NO_REGISTERS);
  Variable* var = proxy->var();

  // Two cases: global variables and all other types of variables.
  switch (var->location()) {
    case VariableLocation::UNALLOCATED: {
      Comment cmnt(masm_, "[ Global variable");
      EmitGlobalVariableLoad(proxy, typeof_mode);
      context()->Plug(eax);
      break;
    }

    case VariableLocation::PARAMETER:
    case VariableLocation::LOCAL:
    case VariableLocation::CONTEXT: {
      DCHECK_EQ(NOT_INSIDE_TYPEOF, typeof_mode);
      Comment cmnt(masm_, var->IsContextSlot() ? "[ Context variable"
                                               : "[ Stack variable");

      if (proxy->hole_check_mode() == HoleCheckMode::kRequired) {
        // Throw a reference error when using an uninitialized let/const
        // binding in harmony mode.
        Label done;
        GetVar(eax, var);
        __ cmp(eax, isolate()->factory()->the_hole_value());
        __ j(not_equal, &done, Label::kNear);
        __ push(Immediate(var->name()));
        __ CallRuntime(Runtime::kThrowReferenceError);
        __ bind(&done);
        context()->Plug(eax);
        break;
      }
      context()->Plug(var);
      break;
    }

    case VariableLocation::LOOKUP:
    case VariableLocation::MODULE:
      UNREACHABLE();
  }
}


void FullCodeGenerator::EmitAccessor(ObjectLiteralProperty* property) {
  Expression* expression = (property == NULL) ? NULL : property->value();
  if (expression == NULL) {
    PushOperand(isolate()->factory()->null_value());
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

  Handle<FixedArray> constant_properties =
      expr->GetOrBuildConstantProperties(isolate());
  int flags = expr->ComputeFlags();
  // If any of the keys would store to the elements array, then we shouldn't
  // allow it.
  if (MustCreateObjectLiteralWithRuntime(expr)) {
    __ push(Operand(ebp, JavaScriptFrameConstants::kFunctionOffset));
    __ push(Immediate(Smi::FromInt(expr->literal_index())));
    __ push(Immediate(constant_properties));
    __ push(Immediate(Smi::FromInt(flags)));
    __ CallRuntime(Runtime::kCreateObjectLiteral);
  } else {
    __ mov(eax, Operand(ebp, JavaScriptFrameConstants::kFunctionOffset));
    __ mov(ebx, Immediate(Smi::FromInt(expr->literal_index())));
    __ mov(ecx, Immediate(constant_properties));
    __ mov(edx, Immediate(Smi::FromInt(flags)));
    Callable callable = CodeFactory::FastCloneShallowObject(
        isolate(), expr->properties_count());
    __ Call(callable.code(), RelocInfo::CODE_TARGET);
    RestoreContext();
  }
  PrepareForBailoutForId(expr->CreateLiteralId(), BailoutState::TOS_REGISTER);

  // If result_saved is true the result is on top of the stack.  If
  // result_saved is false the result is in eax.
  bool result_saved = false;

  AccessorTable accessor_table(zone());
  for (int i = 0; i < expr->properties()->length(); i++) {
    ObjectLiteral::Property* property = expr->properties()->at(i);
    DCHECK(!property->is_computed_name());
    if (property->IsCompileTimeValue()) continue;

    Literal* key = property->key()->AsLiteral();
    Expression* value = property->value();
    if (!result_saved) {
      PushOperand(eax);  // Save result on the stack
      result_saved = true;
    }
    switch (property->kind()) {
      case ObjectLiteral::Property::SPREAD:
      case ObjectLiteral::Property::CONSTANT:
        UNREACHABLE();
      case ObjectLiteral::Property::MATERIALIZED_LITERAL:
        DCHECK(!CompileTimeValue::IsCompileTimeValue(value));
        // Fall through.
      case ObjectLiteral::Property::COMPUTED:
        // It is safe to use [[Put]] here because the boilerplate already
        // contains computed properties with an uninitialized value.
        if (key->IsStringLiteral()) {
          DCHECK(key->IsPropertyName());
          if (property->emit_store()) {
            VisitForAccumulatorValue(value);
            DCHECK(StoreDescriptor::ValueRegister().is(eax));
            __ mov(StoreDescriptor::ReceiverRegister(), Operand(esp, 0));
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
        PushOperand(Operand(esp, 0));  // Duplicate receiver.
        VisitForStackValue(key);
        VisitForStackValue(value);
        if (property->emit_store()) {
          if (NeedsHomeObject(value)) {
            EmitSetHomeObject(value, 2, property->GetSlot());
          }
          PushOperand(Smi::FromInt(SLOPPY));  // Language mode
          CallRuntimeWithOperands(Runtime::kSetProperty);
        } else {
          DropOperands(3);
        }
        break;
      case ObjectLiteral::Property::PROTOTYPE:
        PushOperand(Operand(esp, 0));  // Duplicate receiver.
        VisitForStackValue(value);
        DCHECK(property->emit_store());
        CallRuntimeWithOperands(Runtime::kInternalSetPrototype);
        PrepareForBailoutForId(expr->GetIdForPropertySet(i),
                               BailoutState::NO_REGISTERS);
        break;
      case ObjectLiteral::Property::GETTER:
        if (property->emit_store()) {
          AccessorTable::Iterator it = accessor_table.lookup(key);
          it->second->bailout_id = expr->GetIdForPropertySet(i);
          it->second->getter = property;
        }
        break;
      case ObjectLiteral::Property::SETTER:
        if (property->emit_store()) {
          AccessorTable::Iterator it = accessor_table.lookup(key);
          it->second->bailout_id = expr->GetIdForPropertySet(i);
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
    PushOperand(Operand(esp, 0));  // Duplicate receiver.
    VisitForStackValue(it->first);

    EmitAccessor(it->second->getter);
    EmitAccessor(it->second->setter);

    PushOperand(Smi::FromInt(NONE));
    CallRuntimeWithOperands(Runtime::kDefineAccessorPropertyUnchecked);
    PrepareForBailoutForId(it->second->bailout_id, BailoutState::NO_REGISTERS);
  }

  if (result_saved) {
    context()->PlugTOS();
  } else {
    context()->Plug(eax);
  }
}


void FullCodeGenerator::VisitArrayLiteral(ArrayLiteral* expr) {
  Comment cmnt(masm_, "[ ArrayLiteral");

  Handle<ConstantElementsPair> constant_elements =
      expr->GetOrBuildConstantElements(isolate());
  bool has_constant_fast_elements =
      IsFastObjectElementsKind(expr->constant_elements_kind());

  AllocationSiteMode allocation_site_mode = TRACK_ALLOCATION_SITE;
  if (has_constant_fast_elements && !FLAG_allocation_site_pretenuring) {
    // If the only customer of allocation sites is transitioning, then
    // we can turn it off if we don't have anywhere else to transition to.
    allocation_site_mode = DONT_TRACK_ALLOCATION_SITE;
  }

  if (MustCreateArrayLiteralWithRuntime(expr)) {
    __ push(Operand(ebp, JavaScriptFrameConstants::kFunctionOffset));
    __ push(Immediate(Smi::FromInt(expr->literal_index())));
    __ push(Immediate(constant_elements));
    __ push(Immediate(Smi::FromInt(expr->ComputeFlags())));
    __ CallRuntime(Runtime::kCreateArrayLiteral);
  } else {
    __ mov(eax, Operand(ebp, JavaScriptFrameConstants::kFunctionOffset));
    __ mov(ebx, Immediate(Smi::FromInt(expr->literal_index())));
    __ mov(ecx, Immediate(constant_elements));
    Callable callable =
        CodeFactory::FastCloneShallowArray(isolate(), allocation_site_mode);
    __ Call(callable.code(), RelocInfo::CODE_TARGET);
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
      PushOperand(eax);  // array literal.
      result_saved = true;
    }
    VisitForAccumulatorValue(subexpr);

    __ mov(StoreDescriptor::NameRegister(),
           Immediate(Smi::FromInt(array_index)));
    __ mov(StoreDescriptor::ReceiverRegister(), Operand(esp, 0));
    CallKeyedStoreIC(expr->LiteralFeedbackSlot());
    PrepareForBailoutForId(expr->GetIdForElement(array_index),
                           BailoutState::NO_REGISTERS);
  }

  if (result_saved) {
    context()->PlugTOS();
  } else {
    context()->Plug(eax);
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
        __ mov(LoadDescriptor::ReceiverRegister(), Operand(esp, 0));
      } else {
        VisitForStackValue(property->obj());
      }
      break;
    case KEYED_PROPERTY: {
      if (expr->is_compound()) {
        VisitForStackValue(property->obj());
        VisitForStackValue(property->key());
        __ mov(LoadDescriptor::ReceiverRegister(), Operand(esp, kPointerSize));
        __ mov(LoadDescriptor::NameRegister(), Operand(esp, 0));
      } else {
        VisitForStackValue(property->obj());
        VisitForStackValue(property->key());
      }
      break;
    }
    case NAMED_SUPER_PROPERTY:
    case KEYED_SUPER_PROPERTY:
      UNREACHABLE();
      break;
  }

  // For compound assignments we need another deoptimization point after the
  // variable/property load.
  if (expr->is_compound()) {
    AccumulatorValueContext result_context(this);
    { AccumulatorValueContext left_operand_context(this);
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
        case KEYED_PROPERTY:
          EmitKeyedPropertyLoad(property);
          PrepareForBailoutForId(property->LoadId(),
                                 BailoutState::TOS_REGISTER);
          break;
        case NAMED_SUPER_PROPERTY:
        case KEYED_SUPER_PROPERTY:
          UNREACHABLE();
          break;
      }
    }

    Token::Value op = expr->binary_op();
    PushOperand(eax);  // Left operand goes on the stack.
    VisitForAccumulatorValue(expr->value());

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
    case VARIABLE: {
      VariableProxy* proxy = expr->target()->AsVariableProxy();
      EmitVariableAssignment(proxy->var(), expr->op(), expr->AssignmentSlot(),
                             proxy->hole_check_mode());
      PrepareForBailoutForId(expr->AssignmentId(), BailoutState::TOS_REGISTER);
      context()->Plug(eax);
      break;
    }
    case NAMED_PROPERTY:
      EmitNamedPropertyAssignment(expr);
      break;
    case KEYED_PROPERTY:
      EmitKeyedPropertyAssignment(expr);
      break;
    case NAMED_SUPER_PROPERTY:
    case KEYED_SUPER_PROPERTY:
      UNREACHABLE();
      break;
  }
}


void FullCodeGenerator::VisitYield(Yield* expr) {
  // Resumable functions are not supported.
  UNREACHABLE();
}

void FullCodeGenerator::PushOperand(MemOperand operand) {
  OperandStackDepthIncrement(1);
  __ Push(operand);
}

void FullCodeGenerator::EmitOperandStackDepthCheck() {
  if (FLAG_debug_code) {
    int expected_diff = StandardFrameConstants::kFixedFrameSizeFromFp +
                        operand_stack_depth_ * kPointerSize;
    __ mov(eax, ebp);
    __ sub(eax, esp);
    __ cmp(eax, Immediate(expected_diff));
    __ Assert(equal, kUnexpectedStackDepth);
  }
}

void FullCodeGenerator::EmitCreateIteratorResult(bool done) {
  Label allocate, done_allocate;

  __ Allocate(JSIteratorResult::kSize, eax, ecx, edx, &allocate,
              NO_ALLOCATION_FLAGS);
  __ jmp(&done_allocate, Label::kNear);

  __ bind(&allocate);
  __ Push(Smi::FromInt(JSIteratorResult::kSize));
  __ CallRuntime(Runtime::kAllocateInNewSpace);

  __ bind(&done_allocate);
  __ mov(ebx, NativeContextOperand());
  __ mov(ebx, ContextOperand(ebx, Context::ITERATOR_RESULT_MAP_INDEX));
  __ mov(FieldOperand(eax, HeapObject::kMapOffset), ebx);
  __ mov(FieldOperand(eax, JSObject::kPropertiesOffset),
         isolate()->factory()->empty_fixed_array());
  __ mov(FieldOperand(eax, JSObject::kElementsOffset),
         isolate()->factory()->empty_fixed_array());
  __ pop(FieldOperand(eax, JSIteratorResult::kValueOffset));
  __ mov(FieldOperand(eax, JSIteratorResult::kDoneOffset),
         isolate()->factory()->ToBoolean(done));
  STATIC_ASSERT(JSIteratorResult::kSize == 5 * kPointerSize);
  OperandStackDepthDecrement(1);
}


void FullCodeGenerator::EmitInlineSmiBinaryOp(BinaryOperation* expr,
                                              Token::Value op,
                                              Expression* left,
                                              Expression* right) {
  // Do combined smi check of the operands. Left operand is on the
  // stack. Right operand is in eax.
  Label smi_case, done, stub_call;
  PopOperand(edx);
  __ mov(ecx, eax);
  __ or_(eax, edx);
  JumpPatchSite patch_site(masm_);
  patch_site.EmitJumpIfSmi(eax, &smi_case, Label::kNear);

  __ bind(&stub_call);
  __ mov(eax, ecx);
  Handle<Code> code = CodeFactory::BinaryOpIC(isolate(), op).code();
  CallIC(code, expr->BinaryOperationFeedbackId());
  patch_site.EmitPatchInfo();
  __ jmp(&done, Label::kNear);

  // Smi case.
  __ bind(&smi_case);
  __ mov(eax, edx);  // Copy left operand in case of a stub call.

  switch (op) {
    case Token::SAR:
      __ SmiUntag(ecx);
      __ sar_cl(eax);  // No checks of result necessary
      __ and_(eax, Immediate(~kSmiTagMask));
      break;
    case Token::SHL: {
      Label result_ok;
      __ SmiUntag(eax);
      __ SmiUntag(ecx);
      __ shl_cl(eax);
      // Check that the *signed* result fits in a smi.
      __ cmp(eax, 0xc0000000);
      __ j(positive, &result_ok);
      __ SmiTag(ecx);
      __ jmp(&stub_call);
      __ bind(&result_ok);
      __ SmiTag(eax);
      break;
    }
    case Token::SHR: {
      Label result_ok;
      __ SmiUntag(eax);
      __ SmiUntag(ecx);
      __ shr_cl(eax);
      __ test(eax, Immediate(0xc0000000));
      __ j(zero, &result_ok);
      __ SmiTag(ecx);
      __ jmp(&stub_call);
      __ bind(&result_ok);
      __ SmiTag(eax);
      break;
    }
    case Token::ADD:
      __ add(eax, ecx);
      __ j(overflow, &stub_call);
      break;
    case Token::SUB:
      __ sub(eax, ecx);
      __ j(overflow, &stub_call);
      break;
    case Token::MUL: {
      __ SmiUntag(eax);
      __ imul(eax, ecx);
      __ j(overflow, &stub_call);
      __ test(eax, eax);
      __ j(not_zero, &done, Label::kNear);
      __ mov(ebx, edx);
      __ or_(ebx, ecx);
      __ j(negative, &stub_call);
      break;
    }
    case Token::BIT_OR:
      __ or_(eax, ecx);
      break;
    case Token::BIT_AND:
      __ and_(eax, ecx);
      break;
    case Token::BIT_XOR:
      __ xor_(eax, ecx);
      break;
    default:
      UNREACHABLE();
  }

  __ bind(&done);
  context()->Plug(eax);
}

void FullCodeGenerator::EmitBinaryOp(BinaryOperation* expr, Token::Value op) {
  PopOperand(edx);
  Handle<Code> code = CodeFactory::BinaryOpIC(isolate(), op).code();
  JumpPatchSite patch_site(masm_);    // unbound, signals no inlined smi code.
  CallIC(code, expr->BinaryOperationFeedbackId());
  patch_site.EmitPatchInfo();
  context()->Plug(eax);
}


void FullCodeGenerator::EmitAssignment(Expression* expr,
                                       FeedbackVectorSlot slot) {
  DCHECK(expr->IsValidReferenceExpressionOrThis());

  Property* prop = expr->AsProperty();
  LhsKind assign_type = Property::GetAssignType(prop);

  switch (assign_type) {
    case VARIABLE: {
      VariableProxy* proxy = expr->AsVariableProxy();
      EffectContext context(this);
      EmitVariableAssignment(proxy->var(), Token::ASSIGN, slot,
                             proxy->hole_check_mode());
      break;
    }
    case NAMED_PROPERTY: {
      PushOperand(eax);  // Preserve value.
      VisitForAccumulatorValue(prop->obj());
      __ Move(StoreDescriptor::ReceiverRegister(), eax);
      PopOperand(StoreDescriptor::ValueRegister());  // Restore value.
      CallStoreIC(slot, prop->key()->AsLiteral()->value());
      break;
    }
    case KEYED_PROPERTY: {
      PushOperand(eax);  // Preserve value.
      VisitForStackValue(prop->obj());
      VisitForAccumulatorValue(prop->key());
      __ Move(StoreDescriptor::NameRegister(), eax);
      PopOperand(StoreDescriptor::ReceiverRegister());  // Receiver.
      PopOperand(StoreDescriptor::ValueRegister());     // Restore value.
      CallKeyedStoreIC(slot);
      break;
    }
    case NAMED_SUPER_PROPERTY:
    case KEYED_SUPER_PROPERTY:
      UNREACHABLE();
      break;
  }
  context()->Plug(eax);
}


void FullCodeGenerator::EmitStoreToStackLocalOrContextSlot(
    Variable* var, MemOperand location) {
  __ mov(location, eax);
  if (var->IsContextSlot()) {
    __ mov(edx, eax);
    int offset = Context::SlotOffset(var->index());
    __ RecordWriteContextSlot(ecx, offset, edx, ebx, kDontSaveFPRegs);
  }
}

void FullCodeGenerator::EmitVariableAssignment(Variable* var, Token::Value op,
                                               FeedbackVectorSlot slot,
                                               HoleCheckMode hole_check_mode) {
  if (var->IsUnallocated()) {
    // Global var, const, or let.
    __ mov(StoreDescriptor::ReceiverRegister(), NativeContextOperand());
    __ mov(StoreDescriptor::ReceiverRegister(),
           ContextOperand(StoreDescriptor::ReceiverRegister(),
                          Context::EXTENSION_INDEX));
    CallStoreIC(slot, var->name());

  } else if (IsLexicalVariableMode(var->mode()) && op != Token::INIT) {
    DCHECK(!var->IsLookupSlot());
    DCHECK(var->IsStackAllocated() || var->IsContextSlot());
    MemOperand location = VarOperand(var, ecx);
    // Perform an initialization check for lexically declared variables.
    if (hole_check_mode == HoleCheckMode::kRequired) {
      Label assign;
      __ mov(edx, location);
      __ cmp(edx, isolate()->factory()->the_hole_value());
      __ j(not_equal, &assign, Label::kNear);
      __ push(Immediate(var->name()));
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
    MemOperand location = VarOperand(var, ecx);
    __ mov(edx, location);
    __ cmp(edx, isolate()->factory()->the_hole_value());
    __ j(equal, &uninitialized_this);
    __ push(Immediate(var->name()));
    __ CallRuntime(Runtime::kThrowReferenceError);
    __ bind(&uninitialized_this);
    EmitStoreToStackLocalOrContextSlot(var, location);

  } else {
    DCHECK(var->mode() != CONST || op == Token::INIT);
    DCHECK(var->IsStackAllocated() || var->IsContextSlot());
    DCHECK(!var->IsLookupSlot());
    // Assignment to var or initializing assignment to let/const in harmony
    // mode.
    MemOperand location = VarOperand(var, ecx);
    if (FLAG_debug_code && var->mode() == LET && op == Token::INIT) {
      // Check for an uninitialized let binding.
      __ mov(edx, location);
      __ cmp(edx, isolate()->factory()->the_hole_value());
      __ Check(equal, kLetBindingReInitialization);
    }
    EmitStoreToStackLocalOrContextSlot(var, location);
  }
}


void FullCodeGenerator::EmitNamedPropertyAssignment(Assignment* expr) {
  // Assignment to a property, using a named store IC.
  // eax    : value
  // esp[0] : receiver
  Property* prop = expr->target()->AsProperty();
  DCHECK(prop != NULL);
  DCHECK(prop->key()->IsLiteral());

  PopOperand(StoreDescriptor::ReceiverRegister());
  CallStoreIC(expr->AssignmentSlot(), prop->key()->AsLiteral()->value());
  PrepareForBailoutForId(expr->AssignmentId(), BailoutState::TOS_REGISTER);
  context()->Plug(eax);
}


void FullCodeGenerator::EmitKeyedPropertyAssignment(Assignment* expr) {
  // Assignment to a property, using a keyed store IC.
  // eax               : value
  // esp[0]            : key
  // esp[kPointerSize] : receiver

  PopOperand(StoreDescriptor::NameRegister());  // Key.
  PopOperand(StoreDescriptor::ReceiverRegister());
  DCHECK(StoreDescriptor::ValueRegister().is(eax));
  CallKeyedStoreIC(expr->AssignmentSlot());
  PrepareForBailoutForId(expr->AssignmentId(), BailoutState::TOS_REGISTER);
  context()->Plug(eax);
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
    PushOperand(isolate()->factory()->undefined_value());
    convert_mode = ConvertReceiverMode::kNullOrUndefined;
  } else {
    // Load the function from the receiver.
    DCHECK(callee->IsProperty());
    DCHECK(!callee->AsProperty()->IsSuperAccess());
    __ mov(LoadDescriptor::ReceiverRegister(), Operand(esp, 0));
    EmitNamedPropertyLoad(callee->AsProperty());
    PrepareForBailoutForId(callee->AsProperty()->LoadId(),
                           BailoutState::TOS_REGISTER);
    // Push the target function under the receiver.
    PushOperand(Operand(esp, 0));
    __ mov(Operand(esp, kPointerSize), eax);
    convert_mode = ConvertReceiverMode::kNotNullOrUndefined;
  }

  EmitCall(expr, convert_mode);
}


// Code common for calls using the IC.
void FullCodeGenerator::EmitKeyedCallWithLoadIC(Call* expr,
                                                Expression* key) {
  // Load the key.
  VisitForAccumulatorValue(key);

  Expression* callee = expr->expression();

  // Load the function from the receiver.
  DCHECK(callee->IsProperty());
  __ mov(LoadDescriptor::ReceiverRegister(), Operand(esp, 0));
  __ mov(LoadDescriptor::NameRegister(), eax);
  EmitKeyedPropertyLoad(callee->AsProperty());
  PrepareForBailoutForId(callee->AsProperty()->LoadId(),
                         BailoutState::TOS_REGISTER);

  // Push the target function under the receiver.
  PushOperand(Operand(esp, 0));
  __ mov(Operand(esp, kPointerSize), eax);

  EmitCall(expr, ConvertReceiverMode::kNotNullOrUndefined);
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
  Handle<Code> code =
      CodeFactory::CallIC(isolate(), mode, expr->tail_call_mode()).code();
  __ Move(edx, Immediate(SmiFromSlot(expr->CallFeedbackICSlot())));
  __ mov(edi, Operand(esp, (arg_count + 1) * kPointerSize));
  __ Move(eax, Immediate(arg_count));
  CallIC(code);
  OperandStackDepthDecrement(arg_count + 1);

  RecordJSReturnSite(expr);
  RestoreContext();
  context()->DropAndPlug(1, eax);
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

  // Load function and argument count into edi and eax.
  __ Move(eax, Immediate(arg_count));
  __ mov(edi, Operand(esp, arg_count * kPointerSize));

  // Record call targets in unoptimized code.
  __ EmitLoadFeedbackVector(ebx);
  __ mov(edx, Immediate(SmiFromSlot(expr->CallNewFeedbackSlot())));

  CallConstructStub stub(isolate());
  CallIC(stub.GetCode());
  OperandStackDepthDecrement(arg_count + 1);
  PrepareForBailoutForId(expr->ReturnId(), BailoutState::TOS_REGISTER);
  RestoreContext();
  context()->Plug(eax);
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
  __ test(eax, Immediate(kSmiTagMask));
  Split(zero, if_true, if_false, fall_through);

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

  __ JumpIfSmi(eax, if_false);
  __ CmpObjectType(eax, FIRST_JS_RECEIVER_TYPE, ebx);
  PrepareForBailoutBeforeSplit(expr, true, if_true, if_false);
  Split(above_equal, if_true, if_false, fall_through);

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

  __ JumpIfSmi(eax, if_false);
  __ CmpObjectType(eax, JS_ARRAY_TYPE, ebx);
  PrepareForBailoutBeforeSplit(expr, true, if_true, if_false);
  Split(equal, if_true, if_false, fall_through);

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

  __ JumpIfSmi(eax, if_false);
  __ CmpObjectType(eax, JS_TYPED_ARRAY_TYPE, ebx);
  PrepareForBailoutBeforeSplit(expr, true, if_true, if_false);
  Split(equal, if_true, if_false, fall_through);

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

  __ JumpIfSmi(eax, if_false);
  __ CmpObjectType(eax, JS_PROXY_TYPE, ebx);
  PrepareForBailoutBeforeSplit(expr, true, if_true, if_false);
  Split(equal, if_true, if_false, fall_through);

  context()->Plug(if_true, if_false);
}


void FullCodeGenerator::EmitClassOf(CallRuntime* expr) {
  ZoneList<Expression*>* args = expr->arguments();
  DCHECK(args->length() == 1);
  Label done, null, function, non_function_constructor;

  VisitForAccumulatorValue(args->at(0));

  // If the object is not a JSReceiver, we return null.
  __ JumpIfSmi(eax, &null, Label::kNear);
  STATIC_ASSERT(LAST_JS_RECEIVER_TYPE == LAST_TYPE);
  __ CmpObjectType(eax, FIRST_JS_RECEIVER_TYPE, eax);
  __ j(below, &null, Label::kNear);

  // Return 'Function' for JSFunction and JSBoundFunction objects.
  __ CmpInstanceType(eax, FIRST_FUNCTION_TYPE);
  STATIC_ASSERT(LAST_FUNCTION_TYPE == LAST_TYPE);
  __ j(above_equal, &function, Label::kNear);

  // Check if the constructor in the map is a JS function.
  __ GetMapConstructor(eax, eax, ebx);
  __ CmpInstanceType(ebx, JS_FUNCTION_TYPE);
  __ j(not_equal, &non_function_constructor, Label::kNear);

  // eax now contains the constructor function. Grab the
  // instance class name from there.
  __ mov(eax, FieldOperand(eax, JSFunction::kSharedFunctionInfoOffset));
  __ mov(eax, FieldOperand(eax, SharedFunctionInfo::kInstanceClassNameOffset));
  __ jmp(&done, Label::kNear);

  // Non-JS objects have class null.
  __ bind(&null);
  __ mov(eax, isolate()->factory()->null_value());
  __ jmp(&done, Label::kNear);

  // Functions have class 'Function'.
  __ bind(&function);
  __ mov(eax, isolate()->factory()->Function_string());
  __ jmp(&done, Label::kNear);

  // Objects with a non-function constructor have class 'Object'.
  __ bind(&non_function_constructor);
  __ mov(eax, isolate()->factory()->Object_string());

  // All done.
  __ bind(&done);

  context()->Plug(eax);
}


void FullCodeGenerator::EmitStringCharCodeAt(CallRuntime* expr) {
  ZoneList<Expression*>* args = expr->arguments();
  DCHECK(args->length() == 2);

  VisitForStackValue(args->at(0));
  VisitForAccumulatorValue(args->at(1));

  Register object = ebx;
  Register index = eax;
  Register result = edx;

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
  __ Move(result, Immediate(isolate()->factory()->nan_value()));
  __ jmp(&done);

  __ bind(&need_conversion);
  // Move the undefined value into the result register, which will
  // trigger conversion.
  __ Move(result, Immediate(isolate()->factory()->undefined_value()));
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
  // Move target to edi.
  int const argc = args->length() - 2;
  __ mov(edi, Operand(esp, (argc + 1) * kPointerSize));
  // Call the target.
  __ mov(eax, Immediate(argc));
  __ Call(isolate()->builtins()->Call(), RelocInfo::CODE_TARGET);
  OperandStackDepthDecrement(argc + 1);
  RestoreContext();
  // Discard the function left on TOS.
  context()->DropAndPlug(1, eax);
}

void FullCodeGenerator::EmitGetSuperConstructor(CallRuntime* expr) {
  ZoneList<Expression*>* args = expr->arguments();
  DCHECK_EQ(1, args->length());
  VisitForAccumulatorValue(args->at(0));
  __ AssertFunction(eax);
  __ mov(eax, FieldOperand(eax, HeapObject::kMapOffset));
  __ mov(eax, FieldOperand(eax, Map::kPrototypeOffset));
  context()->Plug(eax);
}

void FullCodeGenerator::EmitDebugIsActive(CallRuntime* expr) {
  DCHECK(expr->arguments()->length() == 0);
  ExternalReference debug_is_active =
      ExternalReference::debug_is_active_address(isolate());
  __ movzx_b(eax, Operand::StaticVariable(debug_is_active));
  __ SmiTag(eax);
  context()->Plug(eax);
}


void FullCodeGenerator::EmitCreateIterResultObject(CallRuntime* expr) {
  ZoneList<Expression*>* args = expr->arguments();
  DCHECK_EQ(2, args->length());
  VisitForStackValue(args->at(0));
  VisitForStackValue(args->at(1));

  Label runtime, done;

  __ Allocate(JSIteratorResult::kSize, eax, ecx, edx, &runtime,
              NO_ALLOCATION_FLAGS);
  __ mov(ebx, NativeContextOperand());
  __ mov(ebx, ContextOperand(ebx, Context::ITERATOR_RESULT_MAP_INDEX));
  __ mov(FieldOperand(eax, HeapObject::kMapOffset), ebx);
  __ mov(FieldOperand(eax, JSObject::kPropertiesOffset),
         isolate()->factory()->empty_fixed_array());
  __ mov(FieldOperand(eax, JSObject::kElementsOffset),
         isolate()->factory()->empty_fixed_array());
  __ pop(FieldOperand(eax, JSIteratorResult::kDoneOffset));
  __ pop(FieldOperand(eax, JSIteratorResult::kValueOffset));
  STATIC_ASSERT(JSIteratorResult::kSize == 5 * kPointerSize);
  __ jmp(&done, Label::kNear);

  __ bind(&runtime);
  CallRuntimeWithOperands(Runtime::kCreateIterResultObject);

  __ bind(&done);
  context()->Plug(eax);
}


void FullCodeGenerator::EmitLoadJSRuntimeFunction(CallRuntime* expr) {
  // Push function.
  __ LoadGlobalFunction(expr->context_index(), eax);
  PushOperand(eax);

  // Push undefined as receiver.
  PushOperand(isolate()->factory()->undefined_value());
}


void FullCodeGenerator::EmitCallJSRuntimeFunction(CallRuntime* expr) {
  ZoneList<Expression*>* args = expr->arguments();
  int arg_count = args->length();

  SetCallPosition(expr);
  __ mov(edi, Operand(esp, (arg_count + 1) * kPointerSize));
  __ Set(eax, arg_count);
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
        context()->Plug(eax);
      } else if (proxy != NULL) {
        Variable* var = proxy->var();
        // Delete of an unqualified identifier is disallowed in strict mode but
        // "delete this" is allowed.
        bool is_this = var->is_this();
        DCHECK(is_sloppy(language_mode()) || is_this);
        if (var->IsUnallocated()) {
          __ mov(eax, NativeContextOperand());
          __ push(ContextOperand(eax, Context::EXTENSION_INDEX));
          __ push(Immediate(var->name()));
          __ CallRuntime(Runtime::kDeleteProperty_Sloppy);
          context()->Plug(eax);
        } else {
          DCHECK(!var->IsLookupSlot());
          DCHECK(var->IsStackAllocated() || var->IsContextSlot());
          // Result of deleting non-global variables is false.  'this' is
          // not really a variable, though we implement it as one.  The
          // subexpression does not have side effects.
          context()->Plug(is_this);
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
      context()->Plug(isolate()->factory()->undefined_value());
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
        if (context()->IsAccumulatorValue()) {
          __ mov(eax, isolate()->factory()->true_value());
        } else {
          __ Push(isolate()->factory()->true_value());
        }
        __ jmp(&done, Label::kNear);
        __ bind(&materialize_false);
        PrepareForBailoutForId(expr->MaterializeFalseId(),
                               BailoutState::NO_REGISTERS);
        if (context()->IsAccumulatorValue()) {
          __ mov(eax, isolate()->factory()->false_value());
        } else {
          __ Push(isolate()->factory()->false_value());
        }
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
      __ mov(ebx, eax);
      __ Call(isolate()->builtins()->Typeof(), RelocInfo::CODE_TARGET);
      context()->Plug(eax);
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
      PushOperand(Smi::kZero);
    }
    switch (assign_type) {
      case NAMED_PROPERTY: {
        // Put the object both on the stack and in the register.
        VisitForStackValue(prop->obj());
        __ mov(LoadDescriptor::ReceiverRegister(), Operand(esp, 0));
        EmitNamedPropertyLoad(prop);
        break;
      }

      case KEYED_PROPERTY: {
        VisitForStackValue(prop->obj());
        VisitForStackValue(prop->key());
        __ mov(LoadDescriptor::ReceiverRegister(),
               Operand(esp, kPointerSize));                       // Object.
        __ mov(LoadDescriptor::NameRegister(), Operand(esp, 0));  // Key.
        EmitKeyedPropertyLoad(prop);
        break;
      }

      case NAMED_SUPER_PROPERTY:
      case KEYED_SUPER_PROPERTY:
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
  Label done, stub_call;
  JumpPatchSite patch_site(masm_);
  if (ShouldInlineSmiCase(expr->op())) {
    Label slow;
    patch_site.EmitJumpIfNotSmi(eax, &slow, Label::kNear);

    // Save result for postfix expressions.
    if (expr->is_postfix()) {
      if (!context()->IsEffect()) {
        // Save the result on the stack. If we have a named or keyed property
        // we store the result under the receiver that is currently on top
        // of the stack.
        switch (assign_type) {
          case VARIABLE:
            __ push(eax);
            break;
          case NAMED_PROPERTY:
            __ mov(Operand(esp, kPointerSize), eax);
            break;
          case KEYED_PROPERTY:
            __ mov(Operand(esp, 2 * kPointerSize), eax);
            break;
          case NAMED_SUPER_PROPERTY:
          case KEYED_SUPER_PROPERTY:
            UNREACHABLE();
            break;
        }
      }
    }

    if (expr->op() == Token::INC) {
      __ add(eax, Immediate(Smi::FromInt(1)));
    } else {
      __ sub(eax, Immediate(Smi::FromInt(1)));
    }
    __ j(no_overflow, &done, Label::kNear);
    // Call stub. Undo operation first.
    if (expr->op() == Token::INC) {
      __ sub(eax, Immediate(Smi::FromInt(1)));
    } else {
      __ add(eax, Immediate(Smi::FromInt(1)));
    }
    __ jmp(&stub_call, Label::kNear);
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
          PushOperand(eax);
          break;
        case NAMED_PROPERTY:
          __ mov(Operand(esp, kPointerSize), eax);
          break;
        case KEYED_PROPERTY:
          __ mov(Operand(esp, 2 * kPointerSize), eax);
          break;
        case NAMED_SUPER_PROPERTY:
        case KEYED_SUPER_PROPERTY:
          UNREACHABLE();
          break;
      }
    }
  }

  SetExpressionPosition(expr);

  // Call stub for +1/-1.
  __ bind(&stub_call);
  __ mov(edx, eax);
  __ mov(eax, Immediate(Smi::FromInt(1)));
  Handle<Code> code =
      CodeFactory::BinaryOpIC(isolate(), expr->binary_op()).code();
  CallIC(code, expr->CountBinOpFeedbackId());
  patch_site.EmitPatchInfo();
  __ bind(&done);

  // Store the value returned in eax.
  switch (assign_type) {
    case VARIABLE: {
      VariableProxy* proxy = expr->expression()->AsVariableProxy();
      if (expr->is_postfix()) {
        // Perform the assignment as if via '='.
        { EffectContext context(this);
          EmitVariableAssignment(proxy->var(), Token::ASSIGN, expr->CountSlot(),
                                 proxy->hole_check_mode());
          PrepareForBailoutForId(expr->AssignmentId(),
                                 BailoutState::TOS_REGISTER);
          context.Plug(eax);
        }
        // For all contexts except EffectContext We have the result on
        // top of the stack.
        if (!context()->IsEffect()) {
          context()->PlugTOS();
        }
      } else {
        // Perform the assignment as if via '='.
        EmitVariableAssignment(proxy->var(), Token::ASSIGN, expr->CountSlot(),
                               proxy->hole_check_mode());
        PrepareForBailoutForId(expr->AssignmentId(),
                               BailoutState::TOS_REGISTER);
        context()->Plug(eax);
      }
      break;
    }
    case NAMED_PROPERTY: {
      PopOperand(StoreDescriptor::ReceiverRegister());
      CallStoreIC(expr->CountSlot(), prop->key()->AsLiteral()->value());
      PrepareForBailoutForId(expr->AssignmentId(), BailoutState::TOS_REGISTER);
      if (expr->is_postfix()) {
        if (!context()->IsEffect()) {
          context()->PlugTOS();
        }
      } else {
        context()->Plug(eax);
      }
      break;
    }
    case KEYED_PROPERTY: {
      PopOperand(StoreDescriptor::NameRegister());
      PopOperand(StoreDescriptor::ReceiverRegister());
      CallKeyedStoreIC(expr->CountSlot());
      PrepareForBailoutForId(expr->AssignmentId(), BailoutState::TOS_REGISTER);
      if (expr->is_postfix()) {
        // Result is on the stack
        if (!context()->IsEffect()) {
          context()->PlugTOS();
        }
      } else {
        context()->Plug(eax);
      }
      break;
    }
    case NAMED_SUPER_PROPERTY:
    case KEYED_SUPER_PROPERTY:
      UNREACHABLE();
      break;
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
    __ JumpIfSmi(eax, if_true);
    __ cmp(FieldOperand(eax, HeapObject::kMapOffset),
           isolate()->factory()->heap_number_map());
    Split(equal, if_true, if_false, fall_through);
  } else if (String::Equals(check, factory->string_string())) {
    __ JumpIfSmi(eax, if_false);
    __ CmpObjectType(eax, FIRST_NONSTRING_TYPE, edx);
    Split(below, if_true, if_false, fall_through);
  } else if (String::Equals(check, factory->symbol_string())) {
    __ JumpIfSmi(eax, if_false);
    __ CmpObjectType(eax, SYMBOL_TYPE, edx);
    Split(equal, if_true, if_false, fall_through);
  } else if (String::Equals(check, factory->boolean_string())) {
    __ cmp(eax, isolate()->factory()->true_value());
    __ j(equal, if_true);
    __ cmp(eax, isolate()->factory()->false_value());
    Split(equal, if_true, if_false, fall_through);
  } else if (String::Equals(check, factory->undefined_string())) {
    __ cmp(eax, isolate()->factory()->null_value());
    __ j(equal, if_false);
    __ JumpIfSmi(eax, if_false);
    // Check for undetectable objects => true.
    __ mov(edx, FieldOperand(eax, HeapObject::kMapOffset));
    __ test_b(FieldOperand(edx, Map::kBitFieldOffset),
              Immediate(1 << Map::kIsUndetectable));
    Split(not_zero, if_true, if_false, fall_through);
  } else if (String::Equals(check, factory->function_string())) {
    __ JumpIfSmi(eax, if_false);
    // Check for callable and not undetectable objects => true.
    __ mov(edx, FieldOperand(eax, HeapObject::kMapOffset));
    __ movzx_b(ecx, FieldOperand(edx, Map::kBitFieldOffset));
    __ and_(ecx, (1 << Map::kIsCallable) | (1 << Map::kIsUndetectable));
    __ cmp(ecx, 1 << Map::kIsCallable);
    Split(equal, if_true, if_false, fall_through);
  } else if (String::Equals(check, factory->object_string())) {
    __ JumpIfSmi(eax, if_false);
    __ cmp(eax, isolate()->factory()->null_value());
    __ j(equal, if_true);
    STATIC_ASSERT(LAST_JS_RECEIVER_TYPE == LAST_TYPE);
    __ CmpObjectType(eax, FIRST_JS_RECEIVER_TYPE, edx);
    __ j(below, if_false);
    // Check for callable or undetectable objects => false.
    __ test_b(FieldOperand(edx, Map::kBitFieldOffset),
              Immediate((1 << Map::kIsCallable) | (1 << Map::kIsUndetectable)));
    Split(zero, if_true, if_false, fall_through);
// clang-format off
#define SIMD128_TYPE(TYPE, Type, type, lane_count, lane_type)   \
  } else if (String::Equals(check, factory->type##_string())) { \
    __ JumpIfSmi(eax, if_false);                                \
    __ cmp(FieldOperand(eax, HeapObject::kMapOffset),           \
           isolate()->factory()->type##_map());                 \
    Split(equal, if_true, if_false, fall_through);
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
      __ cmp(eax, isolate()->factory()->true_value());
      Split(equal, if_true, if_false, fall_through);
      break;

    case Token::INSTANCEOF: {
      VisitForAccumulatorValue(expr->right());
      SetExpressionPosition(expr);
      PopOperand(edx);
      __ Call(isolate()->builtins()->InstanceOf(), RelocInfo::CODE_TARGET);
      PrepareForBailoutBeforeSplit(expr, false, NULL, NULL);
      __ cmp(eax, isolate()->factory()->true_value());
      Split(equal, if_true, if_false, fall_through);
      break;
    }

    default: {
      VisitForAccumulatorValue(expr->right());
      SetExpressionPosition(expr);
      Condition cc = CompareIC::ComputeCondition(op);
      PopOperand(edx);

      bool inline_smi_code = ShouldInlineSmiCase(op);
      JumpPatchSite patch_site(masm_);
      if (inline_smi_code) {
        Label slow_case;
        __ mov(ecx, edx);
        __ or_(ecx, eax);
        patch_site.EmitJumpIfNotSmi(ecx, &slow_case, Label::kNear);
        __ cmp(edx, eax);
        Split(cc, if_true, if_false, NULL);
        __ bind(&slow_case);
      }

      Handle<Code> ic = CodeFactory::CompareIC(isolate(), op).code();
      CallIC(ic, expr->CompareOperationFeedbackId());
      patch_site.EmitPatchInfo();

      PrepareForBailoutBeforeSplit(expr, true, if_true, if_false);
      __ test(eax, eax);
      Split(cc, if_true, if_false, fall_through);
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

  Handle<Object> nil_value = nil == kNullValue
      ? isolate()->factory()->null_value()
      : isolate()->factory()->undefined_value();
  if (expr->op() == Token::EQ_STRICT) {
    __ cmp(eax, nil_value);
    Split(equal, if_true, if_false, fall_through);
  } else {
    __ JumpIfSmi(eax, if_false);
    __ mov(eax, FieldOperand(eax, HeapObject::kMapOffset));
    __ test_b(FieldOperand(eax, Map::kBitFieldOffset),
              Immediate(1 << Map::kIsUndetectable));
    Split(not_zero, if_true, if_false, fall_through);
  }
  context()->Plug(if_true, if_false);
}


Register FullCodeGenerator::result_register() {
  return eax;
}


Register FullCodeGenerator::context_register() {
  return esi;
}

void FullCodeGenerator::LoadFromFrameField(int frame_offset, Register value) {
  DCHECK_EQ(POINTER_SIZE_ALIGN(frame_offset), frame_offset);
  __ mov(value, Operand(ebp, frame_offset));
}

void FullCodeGenerator::StoreToFrameField(int frame_offset, Register value) {
  DCHECK_EQ(POINTER_SIZE_ALIGN(frame_offset), frame_offset);
  __ mov(Operand(ebp, frame_offset), value);
}


void FullCodeGenerator::LoadContextField(Register dst, int context_index) {
  __ mov(dst, ContextOperand(esi, context_index));
}


void FullCodeGenerator::PushFunctionArgumentForContextAllocation() {
  DeclarationScope* closure_scope = scope()->GetClosureScope();
  if (closure_scope->is_script_scope() ||
      closure_scope->is_module_scope()) {
    // Contexts nested in the native context have a canonical empty function
    // as their closure, not the anonymous closure containing the global
    // code.
    __ mov(eax, NativeContextOperand());
    PushOperand(ContextOperand(eax, Context::CLOSURE_INDEX));
  } else if (closure_scope->is_eval_scope()) {
    // Contexts nested inside eval code have the same closure as the context
    // calling eval, not the anonymous closure containing the eval code.
    // Fetch it from the context.
    PushOperand(ContextOperand(esi, Context::CLOSURE_INDEX));
  } else {
    DCHECK(closure_scope->is_function_scope());
    PushOperand(Operand(ebp, JavaScriptFrameConstants::kFunctionOffset));
  }
}


#undef __


static const byte kJnsInstruction = 0x79;
static const byte kJnsOffset = 0x11;
static const byte kNopByteOne = 0x66;
static const byte kNopByteTwo = 0x90;
#ifdef DEBUG
static const byte kCallInstruction = 0xe8;
#endif


void BackEdgeTable::PatchAt(Code* unoptimized_code,
                            Address pc,
                            BackEdgeState target_state,
                            Code* replacement_code) {
  Address call_target_address = pc - kIntSize;
  Address jns_instr_address = call_target_address - 3;
  Address jns_offset_address = call_target_address - 2;

  switch (target_state) {
    case INTERRUPT:
      //     sub <profiling_counter>, <delta>  ;; Not changed
      //     jns ok
      //     call <interrupt stub>
      //   ok:
      *jns_instr_address = kJnsInstruction;
      *jns_offset_address = kJnsOffset;
      break;
    case ON_STACK_REPLACEMENT:
      //     sub <profiling_counter>, <delta>  ;; Not changed
      //     nop
      //     nop
      //     call <on-stack replacment>
      //   ok:
      *jns_instr_address = kNopByteOne;
      *jns_offset_address = kNopByteTwo;
      break;
  }

  Assembler::set_target_address_at(unoptimized_code->GetIsolate(),
                                   call_target_address, unoptimized_code,
                                   replacement_code->entry());
  unoptimized_code->GetHeap()->incremental_marking()->RecordCodeTargetPatch(
      unoptimized_code, call_target_address, replacement_code);
}


BackEdgeTable::BackEdgeState BackEdgeTable::GetBackEdgeState(
    Isolate* isolate,
    Code* unoptimized_code,
    Address pc) {
  Address call_target_address = pc - kIntSize;
  Address jns_instr_address = call_target_address - 3;
  DCHECK_EQ(kCallInstruction, *(call_target_address - 1));

  if (*jns_instr_address == kJnsInstruction) {
    DCHECK_EQ(kJnsOffset, *(call_target_address - 2));
    DCHECK_EQ(isolate->builtins()->InterruptCheck()->entry(),
              Assembler::target_address_at(call_target_address,
                                           unoptimized_code));
    return INTERRUPT;
  }

  DCHECK_EQ(kNopByteOne, *jns_instr_address);
  DCHECK_EQ(kNopByteTwo, *(call_target_address - 2));

  DCHECK_EQ(
      isolate->builtins()->OnStackReplacement()->entry(),
      Assembler::target_address_at(call_target_address, unoptimized_code));
  return ON_STACK_REPLACEMENT;
}


}  // namespace internal
}  // namespace v8

#endif  // V8_TARGET_ARCH_X87
