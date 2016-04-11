// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if V8_TARGET_ARCH_IA32

#include "src/ast/scopes.h"
#include "src/code-factory.h"
#include "src/code-stubs.h"
#include "src/codegen.h"
#include "src/debug/debug.h"
#include "src/full-codegen/full-codegen.h"
#include "src/ia32/frames-ia32.h"
#include "src/ic/ic.h"
#include "src/parsing/parser.h"

namespace v8 {
namespace internal {

#define __ ACCESS_MASM(masm_)


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
// frames-ia32.h for its layout.
void FullCodeGenerator::Generate() {
  CompilationInfo* info = info_;
  profiling_counter_ = isolate()->factory()->NewCell(
      Handle<Smi>(Smi::FromInt(FLAG_interrupt_budget), isolate()));
  SetFunctionPosition(literal());
  Comment cmnt(masm_, "[ function compiled by full code generator");

  ProfileEntryHookStub::MaybeCallEntryHook(masm_);

#ifdef DEBUG
  if (strlen(FLAG_stop_at) > 0 &&
      literal()->name()->IsUtf8EqualTo(CStrVector(FLAG_stop_at))) {
    __ int3();
  }
#endif

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

  { Comment cmnt(masm_, "[ Allocate locals");
    int locals_count = info->scope()->num_stack_slots();
    // Generators allocate locals, if any, in context slots.
    DCHECK(!IsGeneratorFunction(literal()->kind()) || locals_count == 0);
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
  if (info->scope()->num_heap_slots() > 0) {
    Comment cmnt(masm_, "[ Allocate context");
    bool need_write_barrier = true;
    int slots = info->scope()->num_heap_slots() - Context::MIN_CONTEXT_SLOTS;
    // Argument to NewContext is the function, which is still in edi.
    if (info->scope()->is_script_scope()) {
      __ push(edi);
      __ Push(info->scope()->GetScopeInfo(info->isolate()));
      __ CallRuntime(Runtime::kNewScriptContext);
      PrepareForBailoutForId(BailoutId::ScriptContext(), TOS_REG);
      // The new target value is not used, clobbering is safe.
      DCHECK_NULL(info->scope()->new_target_var());
    } else {
      if (info->scope()->new_target_var() != nullptr) {
        __ push(edx);  // Preserve new target.
      }
      if (slots <= FastNewContextStub::kMaximumSlots) {
        FastNewContextStub stub(isolate(), slots);
        __ CallStub(&stub);
        // Result of FastNewContextStub is always in new space.
        need_write_barrier = false;
      } else {
        __ push(edi);
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
      Variable* var = (i == -1) ? scope()->receiver() : scope()->parameter(i);
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
          __ RecordWriteContextSlot(esi,
                                    context_offset,
                                    eax,
                                    ebx,
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
  PrepareForBailoutForId(BailoutId::FunctionContext(), NO_REGISTERS);

  // Possibly set up a local binding to the this function which is used in
  // derived constructors with super calls.
  Variable* this_function_var = scope()->this_function_var();
  if (this_function_var != nullptr) {
    Comment cmnt(masm_, "[ This function");
    if (!function_in_register) {
      __ mov(edi, Operand(ebp, JavaScriptFrameConstants::kFunctionOffset));
      // The write barrier clobbers register again, keep it marked as such.
    }
    SetVar(this_function_var, edi, ebx, ecx);
  }

  // Possibly set up a local binding to the new target value.
  Variable* new_target_var = scope()->new_target_var();
  if (new_target_var != nullptr) {
    Comment cmnt(masm_, "[ new.target");
    SetVar(new_target_var, edx, ebx, ecx);
  }

  // Possibly allocate RestParameters
  int rest_index;
  Variable* rest_param = scope()->rest_parameter(&rest_index);
  if (rest_param) {
    Comment cmnt(masm_, "[ Allocate rest parameter array");

    int num_parameters = info->scope()->num_parameters();
    int offset = num_parameters * kPointerSize;

    __ mov(RestParamAccessDescriptor::parameter_count(),
           Immediate(Smi::FromInt(num_parameters)));
    __ lea(RestParamAccessDescriptor::parameter_pointer(),
           Operand(ebp, StandardFrameConstants::kCallerSPOffset + offset));
    __ mov(RestParamAccessDescriptor::rest_parameter_index(),
           Immediate(Smi::FromInt(rest_index)));
    function_in_register = false;

    RestParamAccessStub stub(isolate());
    __ CallStub(&stub);
    SetVar(rest_param, eax, ebx, edx);
  }

  Variable* arguments = scope()->arguments();
  if (arguments != NULL) {
    // Function uses arguments object.
    Comment cmnt(masm_, "[ Allocate arguments object");
    DCHECK(edi.is(ArgumentsAccessNewDescriptor::function()));
    if (!function_in_register) {
      __ mov(edi, Operand(ebp, JavaScriptFrameConstants::kFunctionOffset));
    }
    // Receiver is just before the parameters on the caller's stack.
    int num_parameters = info->scope()->num_parameters();
    int offset = num_parameters * kPointerSize;
    __ mov(ArgumentsAccessNewDescriptor::parameter_count(),
           Immediate(Smi::FromInt(num_parameters)));
    __ lea(ArgumentsAccessNewDescriptor::parameter_pointer(),
           Operand(ebp, StandardFrameConstants::kCallerSPOffset + offset));

    // Arguments to ArgumentsAccessStub:
    //   function, parameter pointer, parameter count.
    // The stub will rewrite parameter pointer and parameter count if the
    // previous stack frame was an arguments adapter frame.
    bool is_unmapped = is_strict(language_mode()) || !has_simple_parameters();
    ArgumentsAccessStub::Type type = ArgumentsAccessStub::ComputeType(
        is_unmapped, literal()->has_duplicate_parameters());
    ArgumentsAccessStub stub(isolate(), type);
    __ CallStub(&stub);

    SetVar(arguments, eax, ebx, edx);
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
    { Comment cmnt(masm_, "[ Declarations");
      VisitDeclarations(scope()->declarations());
    }

    // Assert that the declarations do not use ICs. Otherwise the debugger
    // won't be able to redirect a PC at an IC to the correct IC in newly
    // recompiled code.
    DCHECK_EQ(0, ic_total_count_);

    { Comment cmnt(masm_, "[ Stack check");
      PrepareForBailoutForId(BailoutId::Declarations(), NO_REGISTERS);
      Label ok;
      ExternalReference stack_limit
          = ExternalReference::address_of_stack_limit(isolate());
      __ cmp(esp, Operand::StaticVariable(stack_limit));
      __ j(above_equal, &ok, Label::kNear);
      __ call(isolate()->builtins()->StackCheck(), RelocInfo::CODE_TARGET);
      __ bind(&ok);
    }

    { Comment cmnt(masm_, "[ Body");
      DCHECK(loop_depth() == 0);
      VisitStatements(literal()->body());
      DCHECK(loop_depth() == 0);
    }
  }

  // Always emit a 'return undefined' in case control fell off the end of
  // the body.
  { Comment cmnt(masm_, "[ return <undefined>;");
    __ mov(eax, isolate()->factory()->undefined_value());
    EmitReturnSequence();
  }
}


void FullCodeGenerator::ClearAccumulator() {
  __ Move(eax, Immediate(Smi::FromInt(0)));
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
  PrepareForBailoutForId(stmt->EntryId(), NO_REGISTERS);
  // Record a mapping of the OSR id to this PC.  This is used if the OSR
  // entry becomes the target of a bailout.  We don't expect it to be, but
  // we want it to work if it is.
  PrepareForBailoutForId(stmt->OsrEntryId(), NO_REGISTERS);
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
    // Pretend that the exit is a backwards jump to the entry.
    int weight = 1;
    if (info_->ShouldSelfOptimize()) {
      weight = FLAG_interrupt_budget / FLAG_self_opt_count;
    } else {
      int distance = masm_->pc_offset();
      weight = Min(kMaxBackEdgeWeight,
                   Max(1, distance / kCodeSizeMultiplier));
    }
    EmitProfilingCounterDecrement(weight);
    Label ok;
    __ j(positive, &ok, Label::kNear);
    __ push(eax);
    __ call(isolate()->builtins()->InterruptCheck(),
            RelocInfo::CODE_TARGET);
    __ pop(eax);
    EmitProfilingCounterReset();
    __ bind(&ok);

    SetReturnPosition(literal());
    __ leave();

    int arg_count = info_->scope()->num_parameters() + 1;
    int arguments_bytes = arg_count * kPointerSize;
    __ Ret(arguments_bytes, ecx);
  }
}


void FullCodeGenerator::StackValueContext::Plug(Variable* var) const {
  DCHECK(var->IsStackAllocated() || var->IsContextSlot());
  MemOperand operand = codegen()->VarOperand(var, result_register());
  // Memory operands can be pushed directly.
  __ push(operand);
}


void FullCodeGenerator::EffectContext::Plug(Heap::RootListIndex index) const {
  UNREACHABLE();  // Not used on IA32.
}


void FullCodeGenerator::AccumulatorValueContext::Plug(
    Heap::RootListIndex index) const {
  UNREACHABLE();  // Not used on IA32.
}


void FullCodeGenerator::StackValueContext::Plug(
    Heap::RootListIndex index) const {
  UNREACHABLE();  // Not used on IA32.
}


void FullCodeGenerator::TestContext::Plug(Heap::RootListIndex index) const {
  UNREACHABLE();  // Not used on IA32.
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
  DCHECK(!lit->IsUndetectableObject());  // There are no undetectable literals.
  if (lit->IsUndefined() || lit->IsNull() || lit->IsFalse()) {
    if (false_label_ != fall_through_) __ jmp(false_label_);
  } else if (lit->IsTrue() || lit->IsJSObject()) {
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


void FullCodeGenerator::EffectContext::DropAndPlug(int count,
                                                   Register reg) const {
  DCHECK(count > 0);
  __ Drop(count);
}


void FullCodeGenerator::AccumulatorValueContext::DropAndPlug(
    int count,
    Register reg) const {
  DCHECK(count > 0);
  __ Drop(count);
  __ Move(result_register(), reg);
}


void FullCodeGenerator::StackValueContext::DropAndPlug(int count,
                                                       Register reg) const {
  DCHECK(count > 0);
  if (count > 1) __ Drop(count - 1);
  __ mov(Operand(esp, 0), reg);
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
  Handle<Code> ic = ToBooleanStub::GetUninitialized(isolate());
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
  PrepareForBailout(expr, TOS_REG);
  if (should_normalize) {
    __ cmp(eax, isolate()->factory()->true_value());
    Split(equal, if_true, if_false, NULL);
    __ bind(&skip);
  }
}


void FullCodeGenerator::EmitDebugCheckDeclarationContext(Variable* variable) {
  // The variable in the declaration always resides in the current context.
  DCHECK_EQ(0, scope()->ContextChainLength(variable->scope()));
  if (generate_debug_code_) {
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
                        : isolate()->factory()->undefined_value(), zone());
      break;

    case VariableLocation::PARAMETER:
    case VariableLocation::LOCAL:
      if (hole_init) {
        Comment cmnt(masm_, "[ VariableDeclaration");
        __ mov(StackOperand(variable),
               Immediate(isolate()->factory()->the_hole_value()));
      }
      break;

    case VariableLocation::CONTEXT:
      if (hole_init) {
        Comment cmnt(masm_, "[ VariableDeclaration");
        EmitDebugCheckDeclarationContext(variable);
        __ mov(ContextOperand(esi, variable->index()),
               Immediate(isolate()->factory()->the_hole_value()));
        // No write barrier since the hole value is in old space.
        PrepareForBailoutForId(proxy->id(), NO_REGISTERS);
      }
      break;

    case VariableLocation::LOOKUP: {
      Comment cmnt(masm_, "[ VariableDeclaration");
      __ push(Immediate(variable->name()));
      // VariableDeclaration nodes are always introduced in one of four modes.
      DCHECK(IsDeclaredVariableMode(mode));
      // Push initial value, if any.
      // Note: For variables we must not push an initial value (such as
      // 'undefined') because we may have a (legal) redeclaration and we
      // must not destroy the current value.
      if (hole_init) {
        __ push(Immediate(isolate()->factory()->the_hole_value()));
      } else {
        __ push(Immediate(Smi::FromInt(0)));  // Indicates no initial value.
      }
      __ push(
          Immediate(Smi::FromInt(variable->DeclarationPropertyAttributes())));
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
      __ mov(StackOperand(variable), result_register());
      break;
    }

    case VariableLocation::CONTEXT: {
      Comment cmnt(masm_, "[ FunctionDeclaration");
      EmitDebugCheckDeclarationContext(variable);
      VisitForAccumulatorValue(declaration->fun());
      __ mov(ContextOperand(esi, variable->index()), result_register());
      // We know that we have written a function, which is not a smi.
      __ RecordWriteContextSlot(esi,
                                Context::SlotOffset(variable->index()),
                                result_register(),
                                ecx,
                                kDontSaveFPRegs,
                                EMIT_REMEMBERED_SET,
                                OMIT_SMI_CHECK);
      PrepareForBailoutForId(proxy->id(), NO_REGISTERS);
      break;
    }

    case VariableLocation::LOOKUP: {
      Comment cmnt(masm_, "[ FunctionDeclaration");
      __ push(Immediate(variable->name()));
      VisitForStackValue(declaration->fun());
      __ push(
          Immediate(Smi::FromInt(variable->DeclarationPropertyAttributes())));
      __ CallRuntime(Runtime::kDeclareLookupSlot);
      break;
    }
  }
}


void FullCodeGenerator::DeclareGlobals(Handle<FixedArray> pairs) {
  // Call the runtime to declare the globals.
  __ Push(pairs);
  __ Push(Smi::FromInt(DeclareGlobalsFlags()));
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
    Handle<Code> ic = CodeFactory::CompareIC(isolate(), Token::EQ_STRICT,
                                             strength(language_mode())).code();
    CallIC(ic, clause->CompareId());
    patch_site.EmitPatchInfo();

    Label skip;
    __ jmp(&skip, Label::kNear);
    PrepareForBailout(clause, TOS_REG);
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
  __ Drop(1);  // Switch value is no longer needed.
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
  __ cmp(eax, isolate()->factory()->undefined_value());
  __ j(equal, &exit);
  __ cmp(eax, isolate()->factory()->null_value());
  __ j(equal, &exit);

  PrepareForBailoutForId(stmt->PrepareId(), TOS_REG);

  // Convert the object to a JS object.
  Label convert, done_convert;
  __ JumpIfSmi(eax, &convert, Label::kNear);
  __ CmpObjectType(eax, FIRST_JS_RECEIVER_TYPE, ecx);
  __ j(above_equal, &done_convert, Label::kNear);
  __ bind(&convert);
  ToObjectStub stub(isolate());
  __ CallStub(&stub);
  __ bind(&done_convert);
  PrepareForBailoutForId(stmt->ToObjectId(), TOS_REG);
  __ push(eax);

  // Check for proxies.
  Label call_runtime, use_cache, fixed_array;
  __ CmpObjectType(eax, JS_PROXY_TYPE, ecx);
  __ j(equal, &call_runtime);

  // Check cache validity in generated code. This is a fast case for
  // the JSObject::IsSimpleEnum cache validity checks. If we cannot
  // guarantee cache validity, call the runtime system to check cache
  // validity or get the property names in a fixed array.
  __ CheckEnumCache(&call_runtime);

  __ mov(eax, FieldOperand(eax, HeapObject::kMapOffset));
  __ jmp(&use_cache, Label::kNear);

  // Get the set of properties to enumerate.
  __ bind(&call_runtime);
  __ push(eax);
  __ CallRuntime(Runtime::kGetPropertyNamesFast);
  PrepareForBailoutForId(stmt->EnumId(), TOS_REG);
  __ cmp(FieldOperand(eax, HeapObject::kMapOffset),
         isolate()->factory()->meta_map());
  __ j(not_equal, &fixed_array);


  // We got a map in register eax. Get the enumeration cache from it.
  Label no_descriptors;
  __ bind(&use_cache);

  __ EnumLength(edx, eax);
  __ cmp(edx, Immediate(Smi::FromInt(0)));
  __ j(equal, &no_descriptors);

  __ LoadInstanceDescriptors(eax, ecx);
  __ mov(ecx, FieldOperand(ecx, DescriptorArray::kEnumCacheOffset));
  __ mov(ecx, FieldOperand(ecx, DescriptorArray::kEnumCacheBridgeCacheOffset));

  // Set up the four remaining stack slots.
  __ push(eax);  // Map.
  __ push(ecx);  // Enumeration cache.
  __ push(edx);  // Number of valid entries for the map in the enum cache.
  __ push(Immediate(Smi::FromInt(0)));  // Initial index.
  __ jmp(&loop);

  __ bind(&no_descriptors);
  __ add(esp, Immediate(kPointerSize));
  __ jmp(&exit);

  // We got a fixed array in register eax. Iterate through that.
  __ bind(&fixed_array);

  // No need for a write barrier, we are storing a Smi in the feedback vector.
  __ EmitLoadTypeFeedbackVector(ebx);
  int vector_index = SmiFromSlot(slot)->value();
  __ mov(FieldOperand(ebx, FixedArray::OffsetOfElementAt(vector_index)),
         Immediate(TypeFeedbackVector::MegamorphicSentinel(isolate())));
  __ push(Immediate(Smi::FromInt(1)));  // Smi(1) indicates slow check
  __ push(eax);  // Array
  __ mov(eax, FieldOperand(eax, FixedArray::kLengthOffset));
  __ push(eax);  // Fixed array length (as smi).
  __ push(Immediate(Smi::FromInt(0)));  // Initial index.

  // Generate code for doing the condition check.
  __ bind(&loop);
  SetExpressionAsStatementPosition(stmt->each());

  __ mov(eax, Operand(esp, 0 * kPointerSize));  // Get the current index.
  __ cmp(eax, Operand(esp, 1 * kPointerSize));  // Compare to the array length.
  __ j(above_equal, loop_statement.break_label());

  // Get the current entry of the array into register ebx.
  __ mov(ebx, Operand(esp, 2 * kPointerSize));
  __ mov(ebx, FieldOperand(ebx, eax, times_2, FixedArray::kHeaderSize));

  // Get the expected map from the stack or a smi in the
  // permanent slow case into register edx.
  __ mov(edx, Operand(esp, 3 * kPointerSize));

  // Check if the expected map still matches that of the enumerable.
  // If not, we may have to filter the key.
  Label update_each;
  __ mov(ecx, Operand(esp, 4 * kPointerSize));
  __ cmp(edx, FieldOperand(ecx, HeapObject::kMapOffset));
  __ j(equal, &update_each, Label::kNear);

  // Convert the entry to a string or null if it isn't a property
  // anymore. If the property has been removed while iterating, we
  // just skip it.
  __ push(ecx);  // Enumerable.
  __ push(ebx);  // Current entry.
  __ CallRuntime(Runtime::kForInFilter);
  PrepareForBailoutForId(stmt->FilterId(), TOS_REG);
  __ cmp(eax, isolate()->factory()->undefined_value());
  __ j(equal, loop_statement.continue_label());
  __ mov(ebx, eax);

  // Update the 'each' property or variable from the possibly filtered
  // entry in register ebx.
  __ bind(&update_each);
  __ mov(result_register(), ebx);
  // Perform the assignment as if via '='.
  { EffectContext context(this);
    EmitAssignment(stmt->each(), stmt->EachFeedbackSlot());
    PrepareForBailoutForId(stmt->AssignmentId(), NO_REGISTERS);
  }

  // Both Crankshaft and Turbofan expect BodyId to be right before stmt->body().
  PrepareForBailoutForId(stmt->BodyId(), NO_REGISTERS);
  // Generate code for the body of the loop.
  Visit(stmt->body());

  // Generate code for going to the next element by incrementing the
  // index (smi) stored on top of the stack.
  __ bind(loop_statement.continue_label());
  __ add(Operand(esp, 0 * kPointerSize), Immediate(Smi::FromInt(1)));

  EmitBackEdgeBookkeeping(stmt, &loop);
  __ jmp(&loop);

  // Remove the pointers stored on the stack.
  __ bind(loop_statement.break_label());
  __ add(esp, Immediate(5 * kPointerSize));

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
  if (!FLAG_always_opt &&
      !FLAG_prepare_always_opt &&
      !pretenure &&
      scope()->is_function_scope() &&
      info->num_literals() == 0) {
    FastNewClosureStub stub(isolate(), info->language_mode(), info->kind());
    __ mov(ebx, Immediate(info));
    __ CallStub(&stub);
  } else {
    __ push(Immediate(info));
    __ CallRuntime(pretenure ? Runtime::kNewClosure_Tenured
                             : Runtime::kNewClosure);
  }
  context()->Plug(eax);
}


void FullCodeGenerator::EmitSetHomeObject(Expression* initializer, int offset,
                                          FeedbackVectorSlot slot) {
  DCHECK(NeedsHomeObject(initializer));
  __ mov(StoreDescriptor::ReceiverRegister(), Operand(esp, 0));
  __ mov(StoreDescriptor::NameRegister(),
         Immediate(isolate()->factory()->home_object_symbol()));
  __ mov(StoreDescriptor::ValueRegister(), Operand(esp, offset * kPointerSize));
  EmitLoadStoreICSlot(slot);
  CallStoreIC();
}


void FullCodeGenerator::EmitSetHomeObjectAccumulator(Expression* initializer,
                                                     int offset,
                                                     FeedbackVectorSlot slot) {
  DCHECK(NeedsHomeObject(initializer));
  __ mov(StoreDescriptor::ReceiverRegister(), eax);
  __ mov(StoreDescriptor::NameRegister(),
         Immediate(isolate()->factory()->home_object_symbol()));
  __ mov(StoreDescriptor::ValueRegister(), Operand(esp, offset * kPointerSize));
  EmitLoadStoreICSlot(slot);
  CallStoreIC();
}


void FullCodeGenerator::EmitLoadGlobalCheckExtensions(VariableProxy* proxy,
                                                      TypeofMode typeof_mode,
                                                      Label* slow) {
  Register context = esi;
  Register temp = edx;

  Scope* s = scope();
  while (s != NULL) {
    if (s->num_heap_slots() > 0) {
      if (s->calls_sloppy_eval()) {
        // Check that extension is "the hole".
        __ JumpIfNotRoot(ContextOperand(context, Context::EXTENSION_INDEX),
                         Heap::kTheHoleValueRootIndex, slow);
      }
      // Load next context in chain.
      __ mov(temp, ContextOperand(context, Context::PREVIOUS_INDEX));
      // Walk the rest of the chain without clobbering esi.
      context = temp;
    }
    // If no outer scope calls eval, we do not need to check more
    // context extensions.  If we have reached an eval scope, we check
    // all extensions from this point.
    if (!s->outer_scope_calls_sloppy_eval() || s->is_eval_scope()) break;
    s = s->outer_scope();
  }

  if (s != NULL && s->is_eval_scope()) {
    // Loop up the context chain.  There is no frame effect so it is
    // safe to use raw labels here.
    Label next, fast;
    if (!context.is(temp)) {
      __ mov(temp, context);
    }
    __ bind(&next);
    // Terminate at native context.
    __ cmp(FieldOperand(temp, HeapObject::kMapOffset),
           Immediate(isolate()->factory()->native_context_map()));
    __ j(equal, &fast, Label::kNear);
    // Check that extension is "the hole".
    __ JumpIfNotRoot(ContextOperand(temp, Context::EXTENSION_INDEX),
                     Heap::kTheHoleValueRootIndex, slow);
    // Load next context in chain.
    __ mov(temp, ContextOperand(temp, Context::PREVIOUS_INDEX));
    __ jmp(&next);
    __ bind(&fast);
  }

  // All extension objects were empty and it is safe to use a normal global
  // load machinery.
  EmitGlobalVariableLoad(proxy, typeof_mode);
}


MemOperand FullCodeGenerator::ContextSlotOperandCheckExtensions(Variable* var,
                                                                Label* slow) {
  DCHECK(var->IsContextSlot());
  Register context = esi;
  Register temp = ebx;

  for (Scope* s = scope(); s != var->scope(); s = s->outer_scope()) {
    if (s->num_heap_slots() > 0) {
      if (s->calls_sloppy_eval()) {
        // Check that extension is "the hole".
        __ JumpIfNotRoot(ContextOperand(context, Context::EXTENSION_INDEX),
                         Heap::kTheHoleValueRootIndex, slow);
      }
      __ mov(temp, ContextOperand(context, Context::PREVIOUS_INDEX));
      // Walk the rest of the chain without clobbering esi.
      context = temp;
    }
  }
  // Check that last extension is "the hole".
  __ JumpIfNotRoot(ContextOperand(context, Context::EXTENSION_INDEX),
                   Heap::kTheHoleValueRootIndex, slow);

  // This function is used only for loads, not stores, so it's safe to
  // return an esi-based operand (the write barrier cannot be allowed to
  // destroy the esi register).
  return ContextOperand(context, var->index());
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
    __ jmp(done);
  } else if (var->mode() == DYNAMIC_LOCAL) {
    Variable* local = var->local_if_not_shadowed();
    __ mov(eax, ContextSlotOperandCheckExtensions(local, slow));
    if (local->mode() == LET || local->mode() == CONST ||
        local->mode() == CONST_LEGACY) {
      __ cmp(eax, isolate()->factory()->the_hole_value());
      __ j(not_equal, done);
      if (local->mode() == CONST_LEGACY) {
        __ mov(eax, isolate()->factory()->undefined_value());
      } else {  // LET || CONST
        __ push(Immediate(var->name()));
        __ CallRuntime(Runtime::kThrowReferenceError);
      }
    }
    __ jmp(done);
  }
}


void FullCodeGenerator::EmitGlobalVariableLoad(VariableProxy* proxy,
                                               TypeofMode typeof_mode) {
  Variable* var = proxy->var();
  DCHECK(var->IsUnallocatedOrGlobalSlot() ||
         (var->IsLookupSlot() && var->mode() == DYNAMIC_GLOBAL));
  __ mov(LoadDescriptor::ReceiverRegister(), NativeContextOperand());
  __ mov(LoadDescriptor::ReceiverRegister(),
         ContextOperand(LoadDescriptor::ReceiverRegister(),
                        Context::EXTENSION_INDEX));
  __ mov(LoadDescriptor::NameRegister(), var->name());
  __ mov(LoadDescriptor::SlotRegister(),
         Immediate(SmiFromSlot(proxy->VariableFeedbackSlot())));
  CallLoadIC(typeof_mode);
}


void FullCodeGenerator::EmitVariableLoad(VariableProxy* proxy,
                                         TypeofMode typeof_mode) {
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
      context()->Plug(eax);
      break;
    }

    case VariableLocation::PARAMETER:
    case VariableLocation::LOCAL:
    case VariableLocation::CONTEXT: {
      DCHECK_EQ(NOT_INSIDE_TYPEOF, typeof_mode);
      Comment cmnt(masm_, var->IsContextSlot() ? "[ Context variable"
                                               : "[ Stack variable");

      if (NeedsHoleCheckForLoad(proxy)) {
        // Let and const need a read barrier.
        Label done;
        GetVar(eax, var);
        __ cmp(eax, isolate()->factory()->the_hole_value());
        __ j(not_equal, &done, Label::kNear);
        if (var->mode() == LET || var->mode() == CONST) {
          // Throw a reference error when using an uninitialized let/const
          // binding in harmony mode.
          __ push(Immediate(var->name()));
          __ CallRuntime(Runtime::kThrowReferenceError);
        } else {
          // Uninitialized legacy const bindings are unholed.
          DCHECK(var->mode() == CONST_LEGACY);
          __ mov(eax, isolate()->factory()->undefined_value());
        }
        __ bind(&done);
        context()->Plug(eax);
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
      __ push(esi);  // Context.
      __ push(Immediate(var->name()));
      Runtime::FunctionId function_id =
          typeof_mode == NOT_INSIDE_TYPEOF
              ? Runtime::kLoadLookupSlot
              : Runtime::kLoadLookupSlotNoReferenceError;
      __ CallRuntime(function_id);
      __ bind(&done);
      context()->Plug(eax);
      break;
    }
  }
}


void FullCodeGenerator::VisitRegExpLiteral(RegExpLiteral* expr) {
  Comment cmnt(masm_, "[ RegExpLiteral");
  __ mov(edi, Operand(ebp, JavaScriptFrameConstants::kFunctionOffset));
  __ Move(eax, Immediate(Smi::FromInt(expr->literal_index())));
  __ Move(ecx, Immediate(expr->pattern()));
  __ Move(edx, Immediate(Smi::FromInt(expr->flags())));
  FastCloneRegExpStub stub(isolate());
  __ CallStub(&stub);
  context()->Plug(eax);
}


void FullCodeGenerator::EmitAccessor(ObjectLiteralProperty* property) {
  Expression* expression = (property == NULL) ? NULL : property->value();
  if (expression == NULL) {
    __ push(Immediate(isolate()->factory()->null_value()));
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
    FastCloneShallowObjectStub stub(isolate(), expr->properties_count());
    __ CallStub(&stub);
  }
  PrepareForBailoutForId(expr->CreateLiteralId(), TOS_REG);

  // If result_saved is true the result is on top of the stack.  If
  // result_saved is false the result is in eax.
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
      __ push(eax);  // Save result on the stack
      result_saved = true;
    }
    switch (property->kind()) {
      case ObjectLiteral::Property::CONSTANT:
        UNREACHABLE();
      case ObjectLiteral::Property::MATERIALIZED_LITERAL:
        DCHECK(!CompileTimeValue::IsCompileTimeValue(value));
        // Fall through.
      case ObjectLiteral::Property::COMPUTED:
        // It is safe to use [[Put]] here because the boilerplate already
        // contains computed properties with an uninitialized value.
        if (key->value()->IsInternalizedString()) {
          if (property->emit_store()) {
            VisitForAccumulatorValue(value);
            DCHECK(StoreDescriptor::ValueRegister().is(eax));
            __ mov(StoreDescriptor::NameRegister(), Immediate(key->value()));
            __ mov(StoreDescriptor::ReceiverRegister(), Operand(esp, 0));
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
        __ push(Operand(esp, 0));  // Duplicate receiver.
        VisitForStackValue(key);
        VisitForStackValue(value);
        if (property->emit_store()) {
          if (NeedsHomeObject(value)) {
            EmitSetHomeObject(value, 2, property->GetSlot());
          }
          __ push(Immediate(Smi::FromInt(SLOPPY)));  // Language mode
          __ CallRuntime(Runtime::kSetProperty);
        } else {
          __ Drop(3);
        }
        break;
      case ObjectLiteral::Property::PROTOTYPE:
        __ push(Operand(esp, 0));  // Duplicate receiver.
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
       it != accessor_table.end();
       ++it) {
    __ push(Operand(esp, 0));  // Duplicate receiver.
    VisitForStackValue(it->first);

    EmitAccessor(it->second->getter);
    EmitAccessor(it->second->setter);

    __ push(Immediate(Smi::FromInt(NONE)));
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
      __ push(eax);  // Save result on the stack
      result_saved = true;
    }

    __ push(Operand(esp, 0));  // Duplicate receiver.

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
            __ push(Immediate(Smi::FromInt(NONE)));
            __ CallRuntime(Runtime::kDefineDataPropertyUnchecked);
          } else {
            __ Drop(3);
          }
          break;

        case ObjectLiteral::Property::PROTOTYPE:
          UNREACHABLE();
          break;

        case ObjectLiteral::Property::GETTER:
          __ push(Immediate(Smi::FromInt(NONE)));
          __ CallRuntime(Runtime::kDefineGetterPropertyUnchecked);
          break;

        case ObjectLiteral::Property::SETTER:
          __ push(Immediate(Smi::FromInt(NONE)));
          __ CallRuntime(Runtime::kDefineSetterPropertyUnchecked);
          break;
      }
    }
  }

  if (expr->has_function()) {
    DCHECK(result_saved);
    __ push(Operand(esp, 0));
    __ CallRuntime(Runtime::kToFastProperties);
  }

  if (result_saved) {
    context()->PlugTOS();
  } else {
    context()->Plug(eax);
  }
}


void FullCodeGenerator::VisitArrayLiteral(ArrayLiteral* expr) {
  Comment cmnt(masm_, "[ ArrayLiteral");

  Handle<FixedArray> constant_elements = expr->constant_elements();
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
      __ push(eax);  // array literal.
      result_saved = true;
    }
    VisitForAccumulatorValue(subexpr);

    __ mov(StoreDescriptor::NameRegister(),
           Immediate(Smi::FromInt(array_index)));
    __ mov(StoreDescriptor::ReceiverRegister(), Operand(esp, 0));
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
    __ Pop(eax);
    result_saved = false;
  }
  for (; array_index < length; array_index++) {
    Expression* subexpr = subexprs->at(array_index);

    __ Push(eax);
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
    context()->Plug(eax);
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
    case NAMED_SUPER_PROPERTY:
      VisitForStackValue(
          property->obj()->AsSuperPropertyReference()->this_var());
      VisitForAccumulatorValue(
          property->obj()->AsSuperPropertyReference()->home_object());
      __ push(result_register());
      if (expr->is_compound()) {
        __ push(MemOperand(esp, kPointerSize));
        __ push(result_register());
      }
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
    case KEYED_SUPER_PROPERTY:
      VisitForStackValue(
          property->obj()->AsSuperPropertyReference()->this_var());
      VisitForStackValue(
          property->obj()->AsSuperPropertyReference()->home_object());
      VisitForAccumulatorValue(property->key());
      __ Push(result_register());
      if (expr->is_compound()) {
        __ push(MemOperand(esp, 2 * kPointerSize));
        __ push(MemOperand(esp, 2 * kPointerSize));
        __ push(result_register());
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
  }

  // For compound assignments we need another deoptimization point after the
  // variable/property load.
  if (expr->is_compound()) {
    AccumulatorValueContext result_context(this);
    { AccumulatorValueContext left_operand_context(this);
      switch (assign_type) {
        case VARIABLE:
          EmitVariableLoad(expr->target()->AsVariableProxy());
          PrepareForBailout(expr->target(), TOS_REG);
          break;
        case NAMED_SUPER_PROPERTY:
          EmitNamedSuperPropertyLoad(property);
          PrepareForBailoutForId(property->LoadId(), TOS_REG);
          break;
        case NAMED_PROPERTY:
          EmitNamedPropertyLoad(property);
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
    __ push(eax);  // Left operand goes on the stack.
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
      context()->Plug(eax);
      break;
    case NAMED_PROPERTY:
      EmitNamedPropertyAssignment(expr);
      break;
    case NAMED_SUPER_PROPERTY:
      EmitNamedSuperPropertyStore(property);
      context()->Plug(result_register());
      break;
    case KEYED_SUPER_PROPERTY:
      EmitKeyedSuperPropertyStore(property);
      context()->Plug(result_register());
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

      __ jmp(&suspend);
      __ bind(&continuation);
      __ RecordGeneratorContinuation();
      __ jmp(&resume);

      __ bind(&suspend);
      VisitForAccumulatorValue(expr->generator_object());
      DCHECK(continuation.pos() > 0 && Smi::IsValid(continuation.pos()));
      __ mov(FieldOperand(eax, JSGeneratorObject::kContinuationOffset),
             Immediate(Smi::FromInt(continuation.pos())));
      __ mov(FieldOperand(eax, JSGeneratorObject::kContextOffset), esi);
      __ mov(ecx, esi);
      __ RecordWriteField(eax, JSGeneratorObject::kContextOffset, ecx, edx,
                          kDontSaveFPRegs);
      __ lea(ebx, Operand(ebp, StandardFrameConstants::kExpressionsOffset));
      __ cmp(esp, ebx);
      __ j(equal, &post_runtime);
      __ push(eax);  // generator object
      __ CallRuntime(Runtime::kSuspendJSGeneratorObject, 1);
      __ mov(context_register(),
             Operand(ebp, StandardFrameConstants::kContextOffset));
      __ bind(&post_runtime);
      __ pop(result_register());
      EmitReturnSequence();

      __ bind(&resume);
      context()->Plug(result_register());
      break;
    }

    case Yield::kFinal: {
      VisitForAccumulatorValue(expr->generator_object());
      __ mov(FieldOperand(result_register(),
                          JSGeneratorObject::kContinuationOffset),
             Immediate(Smi::FromInt(JSGeneratorObject::kGeneratorClosed)));
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
      Label l_next, l_call, l_loop;
      Register load_receiver = LoadDescriptor::ReceiverRegister();
      Register load_name = LoadDescriptor::NameRegister();

      // Initial send value is undefined.
      __ mov(eax, isolate()->factory()->undefined_value());
      __ jmp(&l_next);

      // catch (e) { receiver = iter; f = 'throw'; arg = e; goto l_call; }
      __ bind(&l_catch);
      __ mov(load_name, isolate()->factory()->throw_string());  // "throw"
      __ push(load_name);                                       // "throw"
      __ push(Operand(esp, 2 * kPointerSize));                  // iter
      __ push(eax);                                             // exception
      __ jmp(&l_call);

      // try { received = %yield result }
      // Shuffle the received result above a try handler and yield it without
      // re-boxing.
      __ bind(&l_try);
      __ pop(eax);                                       // result
      int handler_index = NewHandlerTableEntry();
      EnterTryBlock(handler_index, &l_catch);
      const int try_block_size = TryCatch::kElementCount * kPointerSize;
      __ push(eax);                                      // result

      __ jmp(&l_suspend);
      __ bind(&l_continuation);
      __ RecordGeneratorContinuation();
      __ jmp(&l_resume);

      __ bind(&l_suspend);
      const int generator_object_depth = kPointerSize + try_block_size;
      __ mov(eax, Operand(esp, generator_object_depth));
      __ push(eax);                                      // g
      __ push(Immediate(Smi::FromInt(handler_index)));   // handler-index
      DCHECK(l_continuation.pos() > 0 && Smi::IsValid(l_continuation.pos()));
      __ mov(FieldOperand(eax, JSGeneratorObject::kContinuationOffset),
             Immediate(Smi::FromInt(l_continuation.pos())));
      __ mov(FieldOperand(eax, JSGeneratorObject::kContextOffset), esi);
      __ mov(ecx, esi);
      __ RecordWriteField(eax, JSGeneratorObject::kContextOffset, ecx, edx,
                          kDontSaveFPRegs);
      __ CallRuntime(Runtime::kSuspendJSGeneratorObject, 2);
      __ mov(context_register(),
             Operand(ebp, StandardFrameConstants::kContextOffset));
      __ pop(eax);                                       // result
      EmitReturnSequence();
      __ bind(&l_resume);                                // received in eax
      ExitTryBlock(handler_index);

      // receiver = iter; f = iter.next; arg = received;
      __ bind(&l_next);

      __ mov(load_name, isolate()->factory()->next_string());
      __ push(load_name);                           // "next"
      __ push(Operand(esp, 2 * kPointerSize));      // iter
      __ push(eax);                                 // received

      // result = receiver[f](arg);
      __ bind(&l_call);
      __ mov(load_receiver, Operand(esp, kPointerSize));
      __ mov(LoadDescriptor::SlotRegister(),
             Immediate(SmiFromSlot(expr->KeyedLoadFeedbackSlot())));
      Handle<Code> ic = CodeFactory::KeyedLoadIC(isolate(), SLOPPY).code();
      CallIC(ic, TypeFeedbackId::None());
      __ mov(edi, eax);
      __ mov(Operand(esp, 2 * kPointerSize), edi);
      SetCallPosition(expr);
      __ Set(eax, 1);
      __ Call(
          isolate()->builtins()->Call(ConvertReceiverMode::kNotNullOrUndefined),
          RelocInfo::CODE_TARGET);

      __ mov(esi, Operand(ebp, StandardFrameConstants::kContextOffset));
      __ Drop(1);  // The function is still on the stack; drop it.

      // if (!result.done) goto l_try;
      __ bind(&l_loop);
      __ push(eax);                                      // save result
      __ Move(load_receiver, eax);                       // result
      __ mov(load_name,
             isolate()->factory()->done_string());       // "done"
      __ mov(LoadDescriptor::SlotRegister(),
             Immediate(SmiFromSlot(expr->DoneFeedbackSlot())));
      CallLoadIC(NOT_INSIDE_TYPEOF);  // result.done in eax
      Handle<Code> bool_ic = ToBooleanStub::GetUninitialized(isolate());
      CallIC(bool_ic);
      __ CompareRoot(result_register(), Heap::kTrueValueRootIndex);
      __ j(not_equal, &l_try);

      // result.value
      __ pop(load_receiver);                              // result
      __ mov(load_name,
             isolate()->factory()->value_string());       // "value"
      __ mov(LoadDescriptor::SlotRegister(),
             Immediate(SmiFromSlot(expr->ValueFeedbackSlot())));
      CallLoadIC(NOT_INSIDE_TYPEOF);                      // result.value in eax
      context()->DropAndPlug(2, eax);                     // drop iter and g
      break;
    }
  }
}


void FullCodeGenerator::EmitGeneratorResume(Expression *generator,
    Expression *value,
    JSGeneratorObject::ResumeMode resume_mode) {
  // The value stays in eax, and is ultimately read by the resumed generator, as
  // if CallRuntime(Runtime::kSuspendJSGeneratorObject) returned it. Or it
  // is read to throw the value when the resumed generator is already closed.
  // ebx will hold the generator object until the activation has been resumed.
  VisitForStackValue(generator);
  VisitForAccumulatorValue(value);
  __ pop(ebx);

  // Load suspended function and context.
  __ mov(esi, FieldOperand(ebx, JSGeneratorObject::kContextOffset));
  __ mov(edi, FieldOperand(ebx, JSGeneratorObject::kFunctionOffset));

  // Push receiver.
  __ push(FieldOperand(ebx, JSGeneratorObject::kReceiverOffset));

  // Push holes for arguments to generator function.
  __ mov(edx, FieldOperand(edi, JSFunction::kSharedFunctionInfoOffset));
  __ mov(edx,
         FieldOperand(edx, SharedFunctionInfo::kFormalParameterCountOffset));
  __ mov(ecx, isolate()->factory()->the_hole_value());
  Label push_argument_holes, push_frame;
  __ bind(&push_argument_holes);
  __ sub(edx, Immediate(Smi::FromInt(1)));
  __ j(carry, &push_frame);
  __ push(ecx);
  __ jmp(&push_argument_holes);

  // Enter a new JavaScript frame, and initialize its slots as they were when
  // the generator was suspended.
  Label resume_frame, done;
  __ bind(&push_frame);
  __ call(&resume_frame);
  __ jmp(&done);
  __ bind(&resume_frame);
  __ push(ebp);  // Caller's frame pointer.
  __ mov(ebp, esp);
  __ push(esi);  // Callee's context.
  __ push(edi);  // Callee's JS Function.

  // Load the operand stack size.
  __ mov(edx, FieldOperand(ebx, JSGeneratorObject::kOperandStackOffset));
  __ mov(edx, FieldOperand(edx, FixedArray::kLengthOffset));
  __ SmiUntag(edx);

  // If we are sending a value and there is no operand stack, we can jump back
  // in directly.
  if (resume_mode == JSGeneratorObject::NEXT) {
    Label slow_resume;
    __ cmp(edx, Immediate(0));
    __ j(not_zero, &slow_resume);
    __ mov(edx, FieldOperand(edi, JSFunction::kCodeEntryOffset));
    __ mov(ecx, FieldOperand(ebx, JSGeneratorObject::kContinuationOffset));
    __ SmiUntag(ecx);
    __ add(edx, ecx);
    __ mov(FieldOperand(ebx, JSGeneratorObject::kContinuationOffset),
           Immediate(Smi::FromInt(JSGeneratorObject::kGeneratorExecuting)));
    __ jmp(edx);
    __ bind(&slow_resume);
  }

  // Otherwise, we push holes for the operand stack and call the runtime to fix
  // up the stack and the handlers.
  Label push_operand_holes, call_resume;
  __ bind(&push_operand_holes);
  __ sub(edx, Immediate(1));
  __ j(carry, &call_resume);
  __ push(ecx);
  __ jmp(&push_operand_holes);
  __ bind(&call_resume);
  __ push(ebx);
  __ push(result_register());
  __ Push(Smi::FromInt(resume_mode));
  __ CallRuntime(Runtime::kResumeJSGeneratorObject);
  // Not reached: the runtime call returns elsewhere.
  __ Abort(kGeneratorFailedToResume);

  __ bind(&done);
  context()->Plug(result_register());
}


void FullCodeGenerator::EmitCreateIteratorResult(bool done) {
  Label allocate, done_allocate;

  __ Allocate(JSIteratorResult::kSize, eax, ecx, edx, &allocate, TAG_OBJECT);
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
}


void FullCodeGenerator::EmitNamedPropertyLoad(Property* prop) {
  SetExpressionPosition(prop);
  Literal* key = prop->key()->AsLiteral();
  DCHECK(!key->value()->IsSmi());
  DCHECK(!prop->IsSuperAccess());

  __ mov(LoadDescriptor::NameRegister(), Immediate(key->value()));
  __ mov(LoadDescriptor::SlotRegister(),
         Immediate(SmiFromSlot(prop->PropertyFeedbackSlot())));
  CallLoadIC(NOT_INSIDE_TYPEOF, language_mode());
}


void FullCodeGenerator::EmitNamedSuperPropertyLoad(Property* prop) {
  // Stack: receiver, home_object.
  SetExpressionPosition(prop);
  Literal* key = prop->key()->AsLiteral();
  DCHECK(!key->value()->IsSmi());
  DCHECK(prop->IsSuperAccess());

  __ push(Immediate(key->value()));
  __ push(Immediate(Smi::FromInt(language_mode())));
  __ CallRuntime(Runtime::kLoadFromSuper);
}


void FullCodeGenerator::EmitKeyedPropertyLoad(Property* prop) {
  SetExpressionPosition(prop);
  Handle<Code> ic = CodeFactory::KeyedLoadIC(isolate(), language_mode()).code();
  __ mov(LoadDescriptor::SlotRegister(),
         Immediate(SmiFromSlot(prop->PropertyFeedbackSlot())));
  CallIC(ic);
}


void FullCodeGenerator::EmitKeyedSuperPropertyLoad(Property* prop) {
  // Stack: receiver, home_object, key.
  SetExpressionPosition(prop);
  __ push(Immediate(Smi::FromInt(language_mode())));
  __ CallRuntime(Runtime::kLoadKeyedFromSuper);
}


void FullCodeGenerator::EmitInlineSmiBinaryOp(BinaryOperation* expr,
                                              Token::Value op,
                                              Expression* left,
                                              Expression* right) {
  // Do combined smi check of the operands. Left operand is on the
  // stack. Right operand is in eax.
  Label smi_case, done, stub_call;
  __ pop(edx);
  __ mov(ecx, eax);
  __ or_(eax, edx);
  JumpPatchSite patch_site(masm_);
  patch_site.EmitJumpIfSmi(eax, &smi_case, Label::kNear);

  __ bind(&stub_call);
  __ mov(eax, ecx);
  Handle<Code> code =
      CodeFactory::BinaryOpIC(isolate(), op, strength(language_mode())).code();
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


void FullCodeGenerator::EmitClassDefineProperties(ClassLiteral* lit) {
  // Constructor is in eax.
  DCHECK(lit != NULL);
  __ push(eax);

  // No access check is needed here since the constructor is created by the
  // class literal.
  Register scratch = ebx;
  __ mov(scratch, FieldOperand(eax, JSFunction::kPrototypeOrInitialMapOffset));
  __ Push(scratch);

  for (int i = 0; i < lit->properties()->length(); i++) {
    ObjectLiteral::Property* property = lit->properties()->at(i);
    Expression* value = property->value();

    if (property->is_static()) {
      __ push(Operand(esp, kPointerSize));  // constructor
    } else {
      __ push(Operand(esp, 0));  // prototype
    }
    EmitPropertyKey(property, lit->GetIdForProperty(i));

    // The static prototype property is read only. We handle the non computed
    // property name case in the parser. Since this is the only case where we
    // need to check for an own read only property we special case this so we do
    // not need to do this for every property.
    if (property->is_static() && property->is_computed_name()) {
      __ CallRuntime(Runtime::kThrowIfStaticPrototype);
      __ push(eax);
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
        __ push(Immediate(Smi::FromInt(DONT_ENUM)));
        __ CallRuntime(Runtime::kDefineGetterPropertyUnchecked);
        break;

      case ObjectLiteral::Property::SETTER:
        __ push(Immediate(Smi::FromInt(DONT_ENUM)));
        __ CallRuntime(Runtime::kDefineSetterPropertyUnchecked);
        break;
    }
  }

  // Set both the prototype and constructor to have fast properties, and also
  // freeze them in strong mode.
  __ CallRuntime(Runtime::kFinalizeClassDefinition);
}


void FullCodeGenerator::EmitBinaryOp(BinaryOperation* expr, Token::Value op) {
  __ pop(edx);
  Handle<Code> code =
      CodeFactory::BinaryOpIC(isolate(), op, strength(language_mode())).code();
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
      Variable* var = expr->AsVariableProxy()->var();
      EffectContext context(this);
      EmitVariableAssignment(var, Token::ASSIGN, slot);
      break;
    }
    case NAMED_PROPERTY: {
      __ push(eax);  // Preserve value.
      VisitForAccumulatorValue(prop->obj());
      __ Move(StoreDescriptor::ReceiverRegister(), eax);
      __ pop(StoreDescriptor::ValueRegister());  // Restore value.
      __ mov(StoreDescriptor::NameRegister(),
             prop->key()->AsLiteral()->value());
      EmitLoadStoreICSlot(slot);
      CallStoreIC();
      break;
    }
    case NAMED_SUPER_PROPERTY: {
      __ push(eax);
      VisitForStackValue(prop->obj()->AsSuperPropertyReference()->this_var());
      VisitForAccumulatorValue(
          prop->obj()->AsSuperPropertyReference()->home_object());
      // stack: value, this; eax: home_object
      Register scratch = ecx;
      Register scratch2 = edx;
      __ mov(scratch, result_register());               // home_object
      __ mov(eax, MemOperand(esp, kPointerSize));       // value
      __ mov(scratch2, MemOperand(esp, 0));             // this
      __ mov(MemOperand(esp, kPointerSize), scratch2);  // this
      __ mov(MemOperand(esp, 0), scratch);              // home_object
      // stack: this, home_object. eax: value
      EmitNamedSuperPropertyStore(prop);
      break;
    }
    case KEYED_SUPER_PROPERTY: {
      __ push(eax);
      VisitForStackValue(prop->obj()->AsSuperPropertyReference()->this_var());
      VisitForStackValue(
          prop->obj()->AsSuperPropertyReference()->home_object());
      VisitForAccumulatorValue(prop->key());
      Register scratch = ecx;
      Register scratch2 = edx;
      __ mov(scratch2, MemOperand(esp, 2 * kPointerSize));  // value
      // stack: value, this, home_object; eax: key, edx: value
      __ mov(scratch, MemOperand(esp, kPointerSize));  // this
      __ mov(MemOperand(esp, 2 * kPointerSize), scratch);
      __ mov(scratch, MemOperand(esp, 0));  // home_object
      __ mov(MemOperand(esp, kPointerSize), scratch);
      __ mov(MemOperand(esp, 0), eax);
      __ mov(eax, scratch2);
      // stack: this, home_object, key; eax: value.
      EmitKeyedSuperPropertyStore(prop);
      break;
    }
    case KEYED_PROPERTY: {
      __ push(eax);  // Preserve value.
      VisitForStackValue(prop->obj());
      VisitForAccumulatorValue(prop->key());
      __ Move(StoreDescriptor::NameRegister(), eax);
      __ pop(StoreDescriptor::ReceiverRegister());  // Receiver.
      __ pop(StoreDescriptor::ValueRegister());     // Restore value.
      EmitLoadStoreICSlot(slot);
      Handle<Code> ic =
          CodeFactory::KeyedStoreIC(isolate(), language_mode()).code();
      CallIC(ic);
      break;
    }
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
                                               FeedbackVectorSlot slot) {
  if (var->IsUnallocated()) {
    // Global var, const, or let.
    __ mov(StoreDescriptor::NameRegister(), var->name());
    __ mov(StoreDescriptor::ReceiverRegister(), NativeContextOperand());
    __ mov(StoreDescriptor::ReceiverRegister(),
           ContextOperand(StoreDescriptor::ReceiverRegister(),
                          Context::EXTENSION_INDEX));
    EmitLoadStoreICSlot(slot);
    CallStoreIC();

  } else if (var->mode() == LET && op != Token::INIT) {
    // Non-initializing assignment to let variable needs a write barrier.
    DCHECK(!var->IsLookupSlot());
    DCHECK(var->IsStackAllocated() || var->IsContextSlot());
    Label assign;
    MemOperand location = VarOperand(var, ecx);
    __ mov(edx, location);
    __ cmp(edx, isolate()->factory()->the_hole_value());
    __ j(not_equal, &assign, Label::kNear);
    __ push(Immediate(var->name()));
    __ CallRuntime(Runtime::kThrowReferenceError);
    __ bind(&assign);
    EmitStoreToStackLocalOrContextSlot(var, location);

  } else if (var->mode() == CONST && op != Token::INIT) {
    // Assignment to const variable needs a write barrier.
    DCHECK(!var->IsLookupSlot());
    DCHECK(var->IsStackAllocated() || var->IsContextSlot());
    Label const_error;
    MemOperand location = VarOperand(var, ecx);
    __ mov(edx, location);
    __ cmp(edx, isolate()->factory()->the_hole_value());
    __ j(not_equal, &const_error, Label::kNear);
    __ push(Immediate(var->name()));
    __ CallRuntime(Runtime::kThrowReferenceError);
    __ bind(&const_error);
    __ CallRuntime(Runtime::kThrowConstAssignError);

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

  } else if (!var->is_const_mode() ||
             (var->mode() == CONST && op == Token::INIT)) {
    if (var->IsLookupSlot()) {
      // Assignment to var.
      __ push(eax);  // Value.
      __ push(esi);  // Context.
      __ push(Immediate(var->name()));
      __ push(Immediate(Smi::FromInt(language_mode())));
      __ CallRuntime(Runtime::kStoreLookupSlot);
    } else {
      // Assignment to var or initializing assignment to let/const in harmony
      // mode.
      DCHECK(var->IsStackAllocated() || var->IsContextSlot());
      MemOperand location = VarOperand(var, ecx);
      if (generate_debug_code_ && var->mode() == LET && op == Token::INIT) {
        // Check for an uninitialized let binding.
        __ mov(edx, location);
        __ cmp(edx, isolate()->factory()->the_hole_value());
        __ Check(equal, kLetBindingReInitialization);
      }
      EmitStoreToStackLocalOrContextSlot(var, location);
    }

  } else if (var->mode() == CONST_LEGACY && op == Token::INIT) {
    // Const initializers need a write barrier.
    DCHECK(!var->IsParameter());  // No const parameters.
    if (var->IsLookupSlot()) {
      __ push(eax);
      __ push(esi);
      __ push(Immediate(var->name()));
      __ CallRuntime(Runtime::kInitializeLegacyConstLookupSlot);
    } else {
      DCHECK(var->IsStackLocal() || var->IsContextSlot());
      Label skip;
      MemOperand location = VarOperand(var, ecx);
      __ mov(edx, location);
      __ cmp(edx, isolate()->factory()->the_hole_value());
      __ j(not_equal, &skip, Label::kNear);
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
  // eax    : value
  // esp[0] : receiver
  Property* prop = expr->target()->AsProperty();
  DCHECK(prop != NULL);
  DCHECK(prop->key()->IsLiteral());

  __ mov(StoreDescriptor::NameRegister(), prop->key()->AsLiteral()->value());
  __ pop(StoreDescriptor::ReceiverRegister());
  EmitLoadStoreICSlot(expr->AssignmentSlot());
  CallStoreIC();
  PrepareForBailoutForId(expr->AssignmentId(), TOS_REG);
  context()->Plug(eax);
}


void FullCodeGenerator::EmitNamedSuperPropertyStore(Property* prop) {
  // Assignment to named property of super.
  // eax : value
  // stack : receiver ('this'), home_object
  DCHECK(prop != NULL);
  Literal* key = prop->key()->AsLiteral();
  DCHECK(key != NULL);

  __ push(Immediate(key->value()));
  __ push(eax);
  __ CallRuntime((is_strict(language_mode()) ? Runtime::kStoreToSuper_Strict
                                             : Runtime::kStoreToSuper_Sloppy));
}


void FullCodeGenerator::EmitKeyedSuperPropertyStore(Property* prop) {
  // Assignment to named property of super.
  // eax : value
  // stack : receiver ('this'), home_object, key

  __ push(eax);
  __ CallRuntime((is_strict(language_mode())
                      ? Runtime::kStoreKeyedToSuper_Strict
                      : Runtime::kStoreKeyedToSuper_Sloppy));
}


void FullCodeGenerator::EmitKeyedPropertyAssignment(Assignment* expr) {
  // Assignment to a property, using a keyed store IC.
  // eax               : value
  // esp[0]            : key
  // esp[kPointerSize] : receiver

  __ pop(StoreDescriptor::NameRegister());  // Key.
  __ pop(StoreDescriptor::ReceiverRegister());
  DCHECK(StoreDescriptor::ValueRegister().is(eax));
  Handle<Code> ic =
      CodeFactory::KeyedStoreIC(isolate(), language_mode()).code();
  EmitLoadStoreICSlot(expr->AssignmentSlot());
  CallIC(ic);
  PrepareForBailoutForId(expr->AssignmentId(), TOS_REG);
  context()->Plug(eax);
}


void FullCodeGenerator::VisitProperty(Property* expr) {
  Comment cmnt(masm_, "[ Property");
  SetExpressionPosition(expr);

  Expression* key = expr->key();

  if (key->IsPropertyName()) {
    if (!expr->IsSuperAccess()) {
      VisitForAccumulatorValue(expr->obj());
      __ Move(LoadDescriptor::ReceiverRegister(), result_register());
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
      __ pop(LoadDescriptor::ReceiverRegister());                  // Object.
      __ Move(LoadDescriptor::NameRegister(), result_register());  // Key.
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
  context()->Plug(eax);
}


void FullCodeGenerator::CallIC(Handle<Code> code,
                               TypeFeedbackId ast_id) {
  ic_total_count_++;
  __ call(code, RelocInfo::CODE_TARGET, ast_id);
}


// Code common for calls using the IC.
void FullCodeGenerator::EmitCallWithLoadIC(Call* expr) {
  Expression* callee = expr->expression();

  // Get the target function.
  ConvertReceiverMode convert_mode;
  if (callee->IsVariableProxy()) {
    { StackValueContext context(this);
      EmitVariableLoad(callee->AsVariableProxy());
      PrepareForBailout(callee, NO_REGISTERS);
    }
    // Push undefined as receiver. This is patched in the method prologue if it
    // is a sloppy mode method.
    __ push(Immediate(isolate()->factory()->undefined_value()));
    convert_mode = ConvertReceiverMode::kNullOrUndefined;
  } else {
    // Load the function from the receiver.
    DCHECK(callee->IsProperty());
    DCHECK(!callee->AsProperty()->IsSuperAccess());
    __ mov(LoadDescriptor::ReceiverRegister(), Operand(esp, 0));
    EmitNamedPropertyLoad(callee->AsProperty());
    PrepareForBailoutForId(callee->AsProperty()->LoadId(), TOS_REG);
    // Push the target function under the receiver.
    __ push(Operand(esp, 0));
    __ mov(Operand(esp, kPointerSize), eax);
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
  SuperPropertyReference* super_ref = prop->obj()->AsSuperPropertyReference();
  VisitForStackValue(super_ref->home_object());
  VisitForAccumulatorValue(super_ref->this_var());
  __ push(eax);
  __ push(eax);
  __ push(Operand(esp, kPointerSize * 2));
  __ push(Immediate(key->value()));
  __ push(Immediate(Smi::FromInt(language_mode())));
  // Stack here:
  //  - home_object
  //  - this (receiver)
  //  - this (receiver) <-- LoadFromSuper will pop here and below.
  //  - home_object
  //  - key
  //  - language_mode
  __ CallRuntime(Runtime::kLoadFromSuper);

  // Replace home_object with target function.
  __ mov(Operand(esp, kPointerSize), eax);

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
  __ mov(LoadDescriptor::ReceiverRegister(), Operand(esp, 0));
  __ mov(LoadDescriptor::NameRegister(), eax);
  EmitKeyedPropertyLoad(callee->AsProperty());
  PrepareForBailoutForId(callee->AsProperty()->LoadId(), TOS_REG);

  // Push the target function under the receiver.
  __ push(Operand(esp, 0));
  __ mov(Operand(esp, kPointerSize), eax);

  EmitCall(expr, ConvertReceiverMode::kNotNullOrUndefined);
}


void FullCodeGenerator::EmitKeyedSuperCallWithLoadIC(Call* expr) {
  Expression* callee = expr->expression();
  DCHECK(callee->IsProperty());
  Property* prop = callee->AsProperty();
  DCHECK(prop->IsSuperAccess());

  SetExpressionPosition(prop);
  // Load the function from the receiver.
  SuperPropertyReference* super_ref = prop->obj()->AsSuperPropertyReference();
  VisitForStackValue(super_ref->home_object());
  VisitForAccumulatorValue(super_ref->this_var());
  __ push(eax);
  __ push(eax);
  __ push(Operand(esp, kPointerSize * 2));
  VisitForStackValue(prop->key());
  __ push(Immediate(Smi::FromInt(language_mode())));
  // Stack here:
  //  - home_object
  //  - this (receiver)
  //  - this (receiver) <-- LoadKeyedFromSuper will pop here and below.
  //  - home_object
  //  - key
  //  - language_mode
  __ CallRuntime(Runtime::kLoadKeyedFromSuper);

  // Replace home_object with target function.
  __ mov(Operand(esp, kPointerSize), eax);

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
  __ Move(edx, Immediate(SmiFromSlot(expr->CallFeedbackICSlot())));
  __ mov(edi, Operand(esp, (arg_count + 1) * kPointerSize));
  // Don't assign a type feedback id to the IC, since type feedback is provided
  // by the vector above.
  CallIC(ic);

  RecordJSReturnSite(expr);

  // Restore context register.
  __ mov(esi, Operand(ebp, StandardFrameConstants::kContextOffset));

  context()->DropAndPlug(1, eax);
}


void FullCodeGenerator::EmitResolvePossiblyDirectEval(int arg_count) {
  // Push copy of the first argument or undefined if it doesn't exist.
  if (arg_count > 0) {
    __ push(Operand(esp, arg_count * kPointerSize));
  } else {
    __ push(Immediate(isolate()->factory()->undefined_value()));
  }

  // Push the enclosing function.
  __ push(Operand(ebp, JavaScriptFrameConstants::kFunctionOffset));

  // Push the language mode.
  __ push(Immediate(Smi::FromInt(language_mode())));

  // Push the start position of the scope the calls resides in.
  __ push(Immediate(Smi::FromInt(scope()->start_position())));

  // Do the runtime call.
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
    // Call the runtime to find the function to call (returned in eax) and
    // the object holding it (returned in edx).
    __ push(context_register());
    __ push(Immediate(callee->name()));
    __ CallRuntime(Runtime::kLoadLookupSlot);
    __ push(eax);  // Function.
    __ push(edx);  // Receiver.
    PrepareForBailoutForId(expr->LookupId(), NO_REGISTERS);

    // If fast case code has been generated, emit code to push the function
    // and receiver and have the slow path jump around this code.
    if (done.is_linked()) {
      Label call;
      __ jmp(&call, Label::kNear);
      __ bind(&done);
      // Push function.
      __ push(eax);
      // The receiver is implicitly the global receiver. Indicate this by
      // passing the hole to the call function stub.
      __ push(Immediate(isolate()->factory()->undefined_value()));
      __ bind(&call);
    }
  } else {
    VisitForStackValue(callee);
    // refEnv.WithBaseObject()
    __ push(Immediate(isolate()->factory()->undefined_value()));
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
  __ push(Operand(esp, (arg_count + 1) * kPointerSize));
  EmitResolvePossiblyDirectEval(arg_count);

  // Touch up the stack with the resolved function.
  __ mov(Operand(esp, (arg_count + 1) * kPointerSize), eax);

  PrepareForBailoutForId(expr->EvalId(), NO_REGISTERS);

  SetCallPosition(expr);
  __ mov(edi, Operand(esp, (arg_count + 1) * kPointerSize));
  __ Set(eax, arg_count);
  __ Call(isolate()->builtins()->Call(), RelocInfo::CODE_TARGET);
  RecordJSReturnSite(expr);
  // Restore context register.
  __ mov(esi, Operand(ebp, StandardFrameConstants::kContextOffset));
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
  __ EmitLoadTypeFeedbackVector(ebx);
  __ mov(edx, Immediate(SmiFromSlot(expr->CallNewFeedbackSlot())));

  CallConstructStub stub(isolate());
  __ call(stub.GetCode(), RelocInfo::CODE_TARGET);
  PrepareForBailoutForId(expr->ReturnId(), TOS_REG);
  // Restore context register.
  __ mov(esi, Operand(ebp, StandardFrameConstants::kContextOffset));
  context()->Plug(eax);
}


void FullCodeGenerator::EmitSuperConstructorCall(Call* expr) {
  SuperCallReference* super_call_ref =
      expr->expression()->AsSuperCallReference();
  DCHECK_NOT_NULL(super_call_ref);

  // Push the super constructor target on the stack (may be null,
  // but the Construct builtin can deal with that properly).
  VisitForAccumulatorValue(super_call_ref->this_function_var());
  __ AssertFunction(result_register());
  __ mov(result_register(),
         FieldOperand(result_register(), HeapObject::kMapOffset));
  __ Push(FieldOperand(result_register(), Map::kPrototypeOffset));

  // Push the arguments ("left-to-right") on the stack.
  ZoneList<Expression*>* args = expr->arguments();
  int arg_count = args->length();
  for (int i = 0; i < arg_count; i++) {
    VisitForStackValue(args->at(i));
  }

  // Call the construct call builtin that handles allocation and
  // constructor invocation.
  SetConstructCallPosition(expr);

  // Load new target into edx.
  VisitForAccumulatorValue(super_call_ref->new_target_var());
  __ mov(edx, result_register());

  // Load function and argument count into edi and eax.
  __ Move(eax, Immediate(arg_count));
  __ mov(edi, Operand(esp, arg_count * kPointerSize));

  __ Call(isolate()->builtins()->Construct(), RelocInfo::CODE_TARGET);

  RecordJSReturnSite(expr);

  // Restore context register.
  __ mov(esi, Operand(ebp, StandardFrameConstants::kContextOffset));
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

  __ JumpIfSmi(eax, if_false);
  __ CmpObjectType(eax, SIMD128_VALUE_TYPE, ebx);
  PrepareForBailoutBeforeSplit(expr, true, if_true, if_false);
  Split(equal, if_true, if_false, fall_through);

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
  context()->PrepareTest(&materialize_true, &materialize_false,
                         &if_true, &if_false, &fall_through);

  __ JumpIfSmi(eax, if_false);
  __ CmpObjectType(eax, FIRST_FUNCTION_TYPE, ebx);
  PrepareForBailoutBeforeSplit(expr, true, if_true, if_false);
  Split(above_equal, if_true, if_false, fall_through);

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
  context()->PrepareTest(&materialize_true, &materialize_false,
                         &if_true, &if_false, &fall_through);

  Handle<Map> map = masm()->isolate()->factory()->heap_number_map();
  __ CheckMap(eax, map, if_false, DO_SMI_CHECK);
  // Check if the exponent half is 0x80000000. Comparing against 1 and
  // checking for overflow is the shortest possible encoding.
  __ cmp(FieldOperand(eax, HeapNumber::kExponentOffset), Immediate(0x1));
  __ j(no_overflow, if_false);
  __ cmp(FieldOperand(eax, HeapNumber::kMantissaOffset), Immediate(0x0));
  PrepareForBailoutBeforeSplit(expr, true, if_true, if_false);
  Split(equal, if_true, if_false, fall_through);

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

  __ JumpIfSmi(eax, if_false);
  __ CmpObjectType(eax, JS_REGEXP_TYPE, ebx);
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
  context()->PrepareTest(&materialize_true, &materialize_false,
                         &if_true, &if_false, &fall_through);

  __ pop(ebx);
  __ cmp(eax, ebx);
  PrepareForBailoutBeforeSplit(expr, true, if_true, if_false);
  Split(equal, if_true, if_false, fall_through);

  context()->Plug(if_true, if_false);
}


void FullCodeGenerator::EmitArguments(CallRuntime* expr) {
  ZoneList<Expression*>* args = expr->arguments();
  DCHECK(args->length() == 1);

  // ArgumentsAccessStub expects the key in edx and the formal
  // parameter count in eax.
  VisitForAccumulatorValue(args->at(0));
  __ mov(edx, eax);
  __ Move(eax, Immediate(Smi::FromInt(info_->scope()->num_parameters())));
  ArgumentsAccessStub stub(isolate(), ArgumentsAccessStub::READ_ELEMENT);
  __ CallStub(&stub);
  context()->Plug(eax);
}


void FullCodeGenerator::EmitArgumentsLength(CallRuntime* expr) {
  DCHECK(expr->arguments()->length() == 0);

  Label exit;
  // Get the number of formal parameters.
  __ Move(eax, Immediate(Smi::FromInt(info_->scope()->num_parameters())));

  // Check if the calling frame is an arguments adaptor frame.
  __ mov(ebx, Operand(ebp, StandardFrameConstants::kCallerFPOffset));
  __ cmp(Operand(ebx, StandardFrameConstants::kContextOffset),
         Immediate(Smi::FromInt(StackFrame::ARGUMENTS_ADAPTOR)));
  __ j(not_equal, &exit);

  // Arguments adaptor case: Read the arguments length from the
  // adaptor frame.
  __ mov(eax, Operand(ebx, ArgumentsAdaptorFrameConstants::kLengthOffset));

  __ bind(&exit);
  __ AssertSmi(eax);
  context()->Plug(eax);
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

  // Return 'Function' for JSFunction objects.
  __ CmpInstanceType(eax, JS_FUNCTION_TYPE);
  __ j(equal, &function, Label::kNear);

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


void FullCodeGenerator::EmitValueOf(CallRuntime* expr) {
  ZoneList<Expression*>* args = expr->arguments();
  DCHECK(args->length() == 1);

  VisitForAccumulatorValue(args->at(0));  // Load the object.

  Label done;
  // If the object is a smi return the object.
  __ JumpIfSmi(eax, &done, Label::kNear);
  // If the object is not a value type, return the object.
  __ CmpObjectType(eax, JS_VALUE_TYPE, ebx);
  __ j(not_equal, &done, Label::kNear);
  __ mov(eax, FieldOperand(eax, JSValue::kValueOffset));

  __ bind(&done);
  context()->Plug(eax);
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

  __ JumpIfSmi(eax, if_false);
  __ CmpObjectType(eax, JS_DATE_TYPE, ebx);
  PrepareForBailoutBeforeSplit(expr, true, if_true, if_false);
  Split(equal, if_true, if_false, fall_through);

  context()->Plug(if_true, if_false);
}


void FullCodeGenerator::EmitOneByteSeqStringSetChar(CallRuntime* expr) {
  ZoneList<Expression*>* args = expr->arguments();
  DCHECK_EQ(3, args->length());

  Register string = eax;
  Register index = ebx;
  Register value = ecx;

  VisitForStackValue(args->at(0));        // index
  VisitForStackValue(args->at(1));        // value
  VisitForAccumulatorValue(args->at(2));  // string

  __ pop(value);
  __ pop(index);

  if (FLAG_debug_code) {
    __ test(value, Immediate(kSmiTagMask));
    __ Check(zero, kNonSmiValue);
    __ test(index, Immediate(kSmiTagMask));
    __ Check(zero, kNonSmiValue);
  }

  __ SmiUntag(value);
  __ SmiUntag(index);

  if (FLAG_debug_code) {
    static const uint32_t one_byte_seq_type = kSeqStringTag | kOneByteStringTag;
    __ EmitSeqStringSetCharCheck(string, index, value, one_byte_seq_type);
  }

  __ mov_b(FieldOperand(string, index, times_1, SeqOneByteString::kHeaderSize),
           value);
  context()->Plug(string);
}


void FullCodeGenerator::EmitTwoByteSeqStringSetChar(CallRuntime* expr) {
  ZoneList<Expression*>* args = expr->arguments();
  DCHECK_EQ(3, args->length());

  Register string = eax;
  Register index = ebx;
  Register value = ecx;

  VisitForStackValue(args->at(0));        // index
  VisitForStackValue(args->at(1));        // value
  VisitForAccumulatorValue(args->at(2));  // string
  __ pop(value);
  __ pop(index);

  if (FLAG_debug_code) {
    __ test(value, Immediate(kSmiTagMask));
    __ Check(zero, kNonSmiValue);
    __ test(index, Immediate(kSmiTagMask));
    __ Check(zero, kNonSmiValue);
    __ SmiUntag(index);
    static const uint32_t two_byte_seq_type = kSeqStringTag | kTwoByteStringTag;
    __ EmitSeqStringSetCharCheck(string, index, value, two_byte_seq_type);
    __ SmiTag(index);
  }

  __ SmiUntag(value);
  // No need to untag a smi for two-byte addressing.
  __ mov_w(FieldOperand(string, index, times_1, SeqTwoByteString::kHeaderSize),
           value);
  context()->Plug(string);
}


void FullCodeGenerator::EmitSetValueOf(CallRuntime* expr) {
  ZoneList<Expression*>* args = expr->arguments();
  DCHECK(args->length() == 2);

  VisitForStackValue(args->at(0));  // Load the object.
  VisitForAccumulatorValue(args->at(1));  // Load the value.
  __ pop(ebx);  // eax = value. ebx = object.

  Label done;
  // If the object is a smi, return the value.
  __ JumpIfSmi(ebx, &done, Label::kNear);

  // If the object is not a value type, return the value.
  __ CmpObjectType(ebx, JS_VALUE_TYPE, ecx);
  __ j(not_equal, &done, Label::kNear);

  // Store the value.
  __ mov(FieldOperand(ebx, JSValue::kValueOffset), eax);

  // Update the write barrier.  Save the value as it will be
  // overwritten by the write barrier code and is needed afterward.
  __ mov(edx, eax);
  __ RecordWriteField(ebx, JSValue::kValueOffset, edx, ecx, kDontSaveFPRegs);

  __ bind(&done);
  context()->Plug(eax);
}


void FullCodeGenerator::EmitToInteger(CallRuntime* expr) {
  ZoneList<Expression*>* args = expr->arguments();
  DCHECK_EQ(1, args->length());

  // Load the argument into eax and convert it.
  VisitForAccumulatorValue(args->at(0));

  // Convert the object to an integer.
  Label done_convert;
  __ JumpIfSmi(eax, &done_convert, Label::kNear);
  __ Push(eax);
  __ CallRuntime(Runtime::kToInteger);
  __ bind(&done_convert);
  context()->Plug(eax);
}


void FullCodeGenerator::EmitToName(CallRuntime* expr) {
  ZoneList<Expression*>* args = expr->arguments();
  DCHECK_EQ(1, args->length());

  // Load the argument into eax and convert it.
  VisitForAccumulatorValue(args->at(0));

  // Convert the object to a name.
  Label convert, done_convert;
  __ JumpIfSmi(eax, &convert, Label::kNear);
  STATIC_ASSERT(FIRST_NAME_TYPE == FIRST_TYPE);
  __ CmpObjectType(eax, LAST_NAME_TYPE, ecx);
  __ j(below_equal, &done_convert, Label::kNear);
  __ bind(&convert);
  __ Push(eax);
  __ CallRuntime(Runtime::kToName);
  __ bind(&done_convert);
  context()->Plug(eax);
}


void FullCodeGenerator::EmitStringCharFromCode(CallRuntime* expr) {
  ZoneList<Expression*>* args = expr->arguments();
  DCHECK(args->length() == 1);

  VisitForAccumulatorValue(args->at(0));

  Label done;
  StringCharFromCodeGenerator generator(eax, ebx);
  generator.GenerateFast(masm_);
  __ jmp(&done);

  NopRuntimeCallHelper call_helper;
  generator.GenerateSlow(masm_, call_helper);

  __ bind(&done);
  context()->Plug(ebx);
}


void FullCodeGenerator::EmitStringCharCodeAt(CallRuntime* expr) {
  ZoneList<Expression*>* args = expr->arguments();
  DCHECK(args->length() == 2);

  VisitForStackValue(args->at(0));
  VisitForAccumulatorValue(args->at(1));

  Register object = ebx;
  Register index = eax;
  Register result = edx;

  __ pop(object);

  Label need_conversion;
  Label index_out_of_range;
  Label done;
  StringCharCodeAtGenerator generator(object,
                                      index,
                                      result,
                                      &need_conversion,
                                      &need_conversion,
                                      &index_out_of_range,
                                      STRING_INDEX_IS_NUMBER);
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


void FullCodeGenerator::EmitStringCharAt(CallRuntime* expr) {
  ZoneList<Expression*>* args = expr->arguments();
  DCHECK(args->length() == 2);

  VisitForStackValue(args->at(0));
  VisitForAccumulatorValue(args->at(1));

  Register object = ebx;
  Register index = eax;
  Register scratch = edx;
  Register result = eax;

  __ pop(object);

  Label need_conversion;
  Label index_out_of_range;
  Label done;
  StringCharAtGenerator generator(object,
                                  index,
                                  scratch,
                                  result,
                                  &need_conversion,
                                  &need_conversion,
                                  &index_out_of_range,
                                  STRING_INDEX_IS_NUMBER);
  generator.GenerateFast(masm_);
  __ jmp(&done);

  __ bind(&index_out_of_range);
  // When the index is out of range, the spec requires us to return
  // the empty string.
  __ Move(result, Immediate(isolate()->factory()->empty_string()));
  __ jmp(&done);

  __ bind(&need_conversion);
  // Move smi zero into the result register, which will trigger
  // conversion.
  __ Move(result, Immediate(Smi::FromInt(0)));
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
  PrepareForBailoutForId(expr->CallId(), NO_REGISTERS);
  // Move target to edi.
  int const argc = args->length() - 2;
  __ mov(edi, Operand(esp, (argc + 1) * kPointerSize));
  // Call the target.
  __ mov(eax, Immediate(argc));
  __ Call(isolate()->builtins()->Call(), RelocInfo::CODE_TARGET);
  // Restore context register.
  __ mov(esi, Operand(ebp, StandardFrameConstants::kContextOffset));
  // Discard the function left on TOS.
  context()->DropAndPlug(1, eax);
}


void FullCodeGenerator::EmitHasCachedArrayIndex(CallRuntime* expr) {
  ZoneList<Expression*>* args = expr->arguments();
  DCHECK(args->length() == 1);

  VisitForAccumulatorValue(args->at(0));

  __ AssertString(eax);

  Label materialize_true, materialize_false;
  Label* if_true = NULL;
  Label* if_false = NULL;
  Label* fall_through = NULL;
  context()->PrepareTest(&materialize_true, &materialize_false,
                         &if_true, &if_false, &fall_through);

  __ test(FieldOperand(eax, String::kHashFieldOffset),
          Immediate(String::kContainsCachedArrayIndexMask));
  PrepareForBailoutBeforeSplit(expr, true, if_true, if_false);
  Split(zero, if_true, if_false, fall_through);

  context()->Plug(if_true, if_false);
}


void FullCodeGenerator::EmitGetCachedArrayIndex(CallRuntime* expr) {
  ZoneList<Expression*>* args = expr->arguments();
  DCHECK(args->length() == 1);
  VisitForAccumulatorValue(args->at(0));

  __ AssertString(eax);

  __ mov(eax, FieldOperand(eax, String::kHashFieldOffset));
  __ IndexFromHash(eax, eax);

  context()->Plug(eax);
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


void FullCodeGenerator::EmitFastOneByteArrayJoin(CallRuntime* expr) {
  Label bailout, done, one_char_separator, long_separator,
      non_trivial_array, not_size_one_array, loop,
      loop_1, loop_1_condition, loop_2, loop_2_entry, loop_3, loop_3_entry;

  ZoneList<Expression*>* args = expr->arguments();
  DCHECK(args->length() == 2);
  // We will leave the separator on the stack until the end of the function.
  VisitForStackValue(args->at(1));
  // Load this to eax (= array)
  VisitForAccumulatorValue(args->at(0));
  // All aliases of the same register have disjoint lifetimes.
  Register array = eax;
  Register elements = no_reg;  // Will be eax.

  Register index = edx;

  Register string_length = ecx;

  Register string = esi;

  Register scratch = ebx;

  Register array_length = edi;
  Register result_pos = no_reg;  // Will be edi.

  // Separator operand is already pushed.
  Operand separator_operand = Operand(esp, 2 * kPointerSize);
  Operand result_operand = Operand(esp, 1 * kPointerSize);
  Operand array_length_operand = Operand(esp, 0);
  __ sub(esp, Immediate(2 * kPointerSize));
  __ cld();
  // Check that the array is a JSArray
  __ JumpIfSmi(array, &bailout);
  __ CmpObjectType(array, JS_ARRAY_TYPE, scratch);
  __ j(not_equal, &bailout);

  // Check that the array has fast elements.
  __ CheckFastElements(scratch, &bailout);

  // If the array has length zero, return the empty string.
  __ mov(array_length, FieldOperand(array, JSArray::kLengthOffset));
  __ SmiUntag(array_length);
  __ j(not_zero, &non_trivial_array);
  __ mov(result_operand, isolate()->factory()->empty_string());
  __ jmp(&done);

  // Save the array length.
  __ bind(&non_trivial_array);
  __ mov(array_length_operand, array_length);

  // Save the FixedArray containing array's elements.
  // End of array's live range.
  elements = array;
  __ mov(elements, FieldOperand(array, JSArray::kElementsOffset));
  array = no_reg;


  // Check that all array elements are sequential one-byte strings, and
  // accumulate the sum of their lengths, as a smi-encoded value.
  __ Move(index, Immediate(0));
  __ Move(string_length, Immediate(0));
  // Loop condition: while (index < length).
  // Live loop registers: index, array_length, string,
  //                      scratch, string_length, elements.
  if (generate_debug_code_) {
    __ cmp(index, array_length);
    __ Assert(less, kNoEmptyArraysHereInEmitFastOneByteArrayJoin);
  }
  __ bind(&loop);
  __ mov(string, FieldOperand(elements,
                              index,
                              times_pointer_size,
                              FixedArray::kHeaderSize));
  __ JumpIfSmi(string, &bailout);
  __ mov(scratch, FieldOperand(string, HeapObject::kMapOffset));
  __ movzx_b(scratch, FieldOperand(scratch, Map::kInstanceTypeOffset));
  __ and_(scratch, Immediate(
      kIsNotStringMask | kStringEncodingMask | kStringRepresentationMask));
  __ cmp(scratch, kStringTag | kOneByteStringTag | kSeqStringTag);
  __ j(not_equal, &bailout);
  __ add(string_length,
         FieldOperand(string, SeqOneByteString::kLengthOffset));
  __ j(overflow, &bailout);
  __ add(index, Immediate(1));
  __ cmp(index, array_length);
  __ j(less, &loop);

  // If array_length is 1, return elements[0], a string.
  __ cmp(array_length, 1);
  __ j(not_equal, &not_size_one_array);
  __ mov(scratch, FieldOperand(elements, FixedArray::kHeaderSize));
  __ mov(result_operand, scratch);
  __ jmp(&done);

  __ bind(&not_size_one_array);

  // End of array_length live range.
  result_pos = array_length;
  array_length = no_reg;

  // Live registers:
  // string_length: Sum of string lengths, as a smi.
  // elements: FixedArray of strings.

  // Check that the separator is a flat one-byte string.
  __ mov(string, separator_operand);
  __ JumpIfSmi(string, &bailout);
  __ mov(scratch, FieldOperand(string, HeapObject::kMapOffset));
  __ movzx_b(scratch, FieldOperand(scratch, Map::kInstanceTypeOffset));
  __ and_(scratch, Immediate(
      kIsNotStringMask | kStringEncodingMask | kStringRepresentationMask));
  __ cmp(scratch, kStringTag | kOneByteStringTag | kSeqStringTag);
  __ j(not_equal, &bailout);

  // Add (separator length times array_length) - separator length
  // to string_length.
  __ mov(scratch, separator_operand);
  __ mov(scratch, FieldOperand(scratch, SeqOneByteString::kLengthOffset));
  __ sub(string_length, scratch);  // May be negative, temporarily.
  __ imul(scratch, array_length_operand);
  __ j(overflow, &bailout);
  __ add(string_length, scratch);
  __ j(overflow, &bailout);

  __ shr(string_length, 1);

  // Bailout for large object allocations.
  __ cmp(string_length, Page::kMaxRegularHeapObjectSize);
  __ j(greater, &bailout);

  // Live registers and stack values:
  //   string_length
  //   elements
  __ AllocateOneByteString(result_pos, string_length, scratch, index, string,
                           &bailout);
  __ mov(result_operand, result_pos);
  __ lea(result_pos, FieldOperand(result_pos, SeqOneByteString::kHeaderSize));


  __ mov(string, separator_operand);
  __ cmp(FieldOperand(string, SeqOneByteString::kLengthOffset),
         Immediate(Smi::FromInt(1)));
  __ j(equal, &one_char_separator);
  __ j(greater, &long_separator);


  // Empty separator case
  __ mov(index, Immediate(0));
  __ jmp(&loop_1_condition);
  // Loop condition: while (index < length).
  __ bind(&loop_1);
  // Each iteration of the loop concatenates one string to the result.
  // Live values in registers:
  //   index: which element of the elements array we are adding to the result.
  //   result_pos: the position to which we are currently copying characters.
  //   elements: the FixedArray of strings we are joining.

  // Get string = array[index].
  __ mov(string, FieldOperand(elements, index,
                              times_pointer_size,
                              FixedArray::kHeaderSize));
  __ mov(string_length,
         FieldOperand(string, String::kLengthOffset));
  __ shr(string_length, 1);
  __ lea(string,
         FieldOperand(string, SeqOneByteString::kHeaderSize));
  __ CopyBytes(string, result_pos, string_length, scratch);
  __ add(index, Immediate(1));
  __ bind(&loop_1_condition);
  __ cmp(index, array_length_operand);
  __ j(less, &loop_1);  // End while (index < length).
  __ jmp(&done);



  // One-character separator case
  __ bind(&one_char_separator);
  // Replace separator with its one-byte character value.
  __ mov_b(scratch, FieldOperand(string, SeqOneByteString::kHeaderSize));
  __ mov_b(separator_operand, scratch);

  __ Move(index, Immediate(0));
  // Jump into the loop after the code that copies the separator, so the first
  // element is not preceded by a separator
  __ jmp(&loop_2_entry);
  // Loop condition: while (index < length).
  __ bind(&loop_2);
  // Each iteration of the loop concatenates one string to the result.
  // Live values in registers:
  //   index: which element of the elements array we are adding to the result.
  //   result_pos: the position to which we are currently copying characters.

  // Copy the separator character to the result.
  __ mov_b(scratch, separator_operand);
  __ mov_b(Operand(result_pos, 0), scratch);
  __ inc(result_pos);

  __ bind(&loop_2_entry);
  // Get string = array[index].
  __ mov(string, FieldOperand(elements, index,
                              times_pointer_size,
                              FixedArray::kHeaderSize));
  __ mov(string_length,
         FieldOperand(string, String::kLengthOffset));
  __ shr(string_length, 1);
  __ lea(string,
         FieldOperand(string, SeqOneByteString::kHeaderSize));
  __ CopyBytes(string, result_pos, string_length, scratch);
  __ add(index, Immediate(1));

  __ cmp(index, array_length_operand);
  __ j(less, &loop_2);  // End while (index < length).
  __ jmp(&done);


  // Long separator case (separator is more than one character).
  __ bind(&long_separator);

  __ Move(index, Immediate(0));
  // Jump into the loop after the code that copies the separator, so the first
  // element is not preceded by a separator
  __ jmp(&loop_3_entry);
  // Loop condition: while (index < length).
  __ bind(&loop_3);
  // Each iteration of the loop concatenates one string to the result.
  // Live values in registers:
  //   index: which element of the elements array we are adding to the result.
  //   result_pos: the position to which we are currently copying characters.

  // Copy the separator to the result.
  __ mov(string, separator_operand);
  __ mov(string_length,
         FieldOperand(string, String::kLengthOffset));
  __ shr(string_length, 1);
  __ lea(string,
         FieldOperand(string, SeqOneByteString::kHeaderSize));
  __ CopyBytes(string, result_pos, string_length, scratch);

  __ bind(&loop_3_entry);
  // Get string = array[index].
  __ mov(string, FieldOperand(elements, index,
                              times_pointer_size,
                              FixedArray::kHeaderSize));
  __ mov(string_length,
         FieldOperand(string, String::kLengthOffset));
  __ shr(string_length, 1);
  __ lea(string,
         FieldOperand(string, SeqOneByteString::kHeaderSize));
  __ CopyBytes(string, result_pos, string_length, scratch);
  __ add(index, Immediate(1));

  __ cmp(index, array_length_operand);
  __ j(less, &loop_3);  // End while (index < length).
  __ jmp(&done);


  __ bind(&bailout);
  __ mov(result_operand, isolate()->factory()->undefined_value());
  __ bind(&done);
  __ mov(eax, result_operand);
  // Drop temp values from the stack, and restore context register.
  __ add(esp, Immediate(3 * kPointerSize));

  __ mov(esi, Operand(ebp, StandardFrameConstants::kContextOffset));
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

  __ Allocate(JSIteratorResult::kSize, eax, ecx, edx, &runtime, TAG_OBJECT);
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
  __ CallRuntime(Runtime::kCreateIterResultObject);

  __ bind(&done);
  context()->Plug(eax);
}


void FullCodeGenerator::EmitLoadJSRuntimeFunction(CallRuntime* expr) {
  // Push undefined as receiver.
  __ push(Immediate(isolate()->factory()->undefined_value()));

  __ LoadGlobalFunction(expr->context_index(), eax);
}


void FullCodeGenerator::EmitCallJSRuntimeFunction(CallRuntime* expr) {
  ZoneList<Expression*>* args = expr->arguments();
  int arg_count = args->length();

  SetCallPosition(expr);
  __ mov(edi, Operand(esp, (arg_count + 1) * kPointerSize));
  __ Set(eax, arg_count);
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
    __ push(Operand(esp, 0));
    __ mov(Operand(esp, kPointerSize), eax);

    // Push the arguments ("left-to-right").
    for (int i = 0; i < arg_count; i++) {
      VisitForStackValue(args->at(i));
    }

    PrepareForBailoutForId(expr->CallId(), NO_REGISTERS);
    EmitCallJSRuntimeFunction(expr);

    // Restore context register.
    __ mov(esi, Operand(ebp, StandardFrameConstants::kContextOffset));
    context()->DropAndPlug(1, eax);

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
        context()->Plug(eax);
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
        context()->Plug(eax);
      } else if (proxy != NULL) {
        Variable* var = proxy->var();
        // Delete of an unqualified identifier is disallowed in strict mode but
        // "delete this" is allowed.
        bool is_this = var->HasThisName(isolate());
        DCHECK(is_sloppy(language_mode()) || is_this);
        if (var->IsUnallocatedOrGlobalSlot()) {
          __ mov(eax, NativeContextOperand());
          __ push(ContextOperand(eax, Context::EXTENSION_INDEX));
          __ push(Immediate(var->name()));
          __ CallRuntime(Runtime::kDeleteProperty_Sloppy);
          context()->Plug(eax);
        } else if (var->IsStackAllocated() || var->IsContextSlot()) {
          // Result of deleting non-global variables is false.  'this' is
          // not really a variable, though we implement it as one.  The
          // subexpression does not have side effects.
          context()->Plug(is_this);
        } else {
          // Non-global variable.  Call the runtime to try to delete from the
          // context where the variable was introduced.
          __ push(context_register());
          __ push(Immediate(var->name()));
          __ CallRuntime(Runtime::kDeleteLookupSlot);
          context()->Plug(eax);
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
        __ bind(&materialize_true);
        PrepareForBailoutForId(expr->MaterializeTrueId(), NO_REGISTERS);
        if (context()->IsAccumulatorValue()) {
          __ mov(eax, isolate()->factory()->true_value());
        } else {
          __ Push(isolate()->factory()->true_value());
        }
        __ jmp(&done, Label::kNear);
        __ bind(&materialize_false);
        PrepareForBailoutForId(expr->MaterializeFalseId(), NO_REGISTERS);
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
      TypeofStub typeof_stub(isolate());
      __ CallStub(&typeof_stub);
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
      __ push(Immediate(Smi::FromInt(0)));
    }
    switch (assign_type) {
      case NAMED_PROPERTY: {
        // Put the object both on the stack and in the register.
        VisitForStackValue(prop->obj());
        __ mov(LoadDescriptor::ReceiverRegister(), Operand(esp, 0));
        EmitNamedPropertyLoad(prop);
        break;
      }

      case NAMED_SUPER_PROPERTY: {
        VisitForStackValue(prop->obj()->AsSuperPropertyReference()->this_var());
        VisitForAccumulatorValue(
            prop->obj()->AsSuperPropertyReference()->home_object());
        __ push(result_register());
        __ push(MemOperand(esp, kPointerSize));
        __ push(result_register());
        EmitNamedSuperPropertyLoad(prop);
        break;
      }

      case KEYED_SUPER_PROPERTY: {
        VisitForStackValue(prop->obj()->AsSuperPropertyReference()->this_var());
        VisitForStackValue(
            prop->obj()->AsSuperPropertyReference()->home_object());
        VisitForAccumulatorValue(prop->key());
        __ push(result_register());
        __ push(MemOperand(esp, 2 * kPointerSize));
        __ push(MemOperand(esp, 2 * kPointerSize));
        __ push(result_register());
        EmitKeyedSuperPropertyLoad(prop);
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
          case NAMED_SUPER_PROPERTY:
            __ mov(Operand(esp, 2 * kPointerSize), eax);
            break;
          case KEYED_PROPERTY:
            __ mov(Operand(esp, 2 * kPointerSize), eax);
            break;
          case KEYED_SUPER_PROPERTY:
            __ mov(Operand(esp, 3 * kPointerSize), eax);
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
          __ push(eax);
          break;
        case NAMED_PROPERTY:
          __ mov(Operand(esp, kPointerSize), eax);
          break;
        case NAMED_SUPER_PROPERTY:
          __ mov(Operand(esp, 2 * kPointerSize), eax);
          break;
        case KEYED_PROPERTY:
          __ mov(Operand(esp, 2 * kPointerSize), eax);
          break;
        case KEYED_SUPER_PROPERTY:
          __ mov(Operand(esp, 3 * kPointerSize), eax);
          break;
      }
    }
  }

  SetExpressionPosition(expr);

  // Call stub for +1/-1.
  __ bind(&stub_call);
  __ mov(edx, eax);
  __ mov(eax, Immediate(Smi::FromInt(1)));
  Handle<Code> code = CodeFactory::BinaryOpIC(isolate(), expr->binary_op(),
                                              strength(language_mode())).code();
  CallIC(code, expr->CountBinOpFeedbackId());
  patch_site.EmitPatchInfo();
  __ bind(&done);

  if (is_strong(language_mode())) {
    PrepareForBailoutForId(expr->ToNumberId(), TOS_REG);
  }
  // Store the value returned in eax.
  switch (assign_type) {
    case VARIABLE:
      if (expr->is_postfix()) {
        // Perform the assignment as if via '='.
        { EffectContext context(this);
          EmitVariableAssignment(expr->expression()->AsVariableProxy()->var(),
                                 Token::ASSIGN, expr->CountSlot());
          PrepareForBailoutForId(expr->AssignmentId(), TOS_REG);
          context.Plug(eax);
        }
        // For all contexts except EffectContext We have the result on
        // top of the stack.
        if (!context()->IsEffect()) {
          context()->PlugTOS();
        }
      } else {
        // Perform the assignment as if via '='.
        EmitVariableAssignment(expr->expression()->AsVariableProxy()->var(),
                               Token::ASSIGN, expr->CountSlot());
        PrepareForBailoutForId(expr->AssignmentId(), TOS_REG);
        context()->Plug(eax);
      }
      break;
    case NAMED_PROPERTY: {
      __ mov(StoreDescriptor::NameRegister(),
             prop->key()->AsLiteral()->value());
      __ pop(StoreDescriptor::ReceiverRegister());
      EmitLoadStoreICSlot(expr->CountSlot());
      CallStoreIC();
      PrepareForBailoutForId(expr->AssignmentId(), TOS_REG);
      if (expr->is_postfix()) {
        if (!context()->IsEffect()) {
          context()->PlugTOS();
        }
      } else {
        context()->Plug(eax);
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
        context()->Plug(eax);
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
        context()->Plug(eax);
      }
      break;
    }
    case KEYED_PROPERTY: {
      __ pop(StoreDescriptor::NameRegister());
      __ pop(StoreDescriptor::ReceiverRegister());
      Handle<Code> ic =
          CodeFactory::KeyedStoreIC(isolate(), language_mode()).code();
      EmitLoadStoreICSlot(expr->CountSlot());
      CallIC(ic);
      PrepareForBailoutForId(expr->AssignmentId(), TOS_REG);
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
    __ cmp(eax, isolate()->factory()->undefined_value());
    __ j(equal, if_true);
    __ JumpIfSmi(eax, if_false);
    // Check for undetectable objects => true.
    __ mov(edx, FieldOperand(eax, HeapObject::kMapOffset));
    __ test_b(FieldOperand(edx, Map::kBitFieldOffset),
              1 << Map::kIsUndetectable);
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
              (1 << Map::kIsCallable) | (1 << Map::kIsUndetectable));
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
  context()->PrepareTest(&materialize_true, &materialize_false,
                         &if_true, &if_false, &fall_through);

  Token::Value op = expr->op();
  VisitForStackValue(expr->left());
  switch (op) {
    case Token::IN:
      VisitForStackValue(expr->right());
      __ CallRuntime(Runtime::kHasProperty);
      PrepareForBailoutBeforeSplit(expr, false, NULL, NULL);
      __ cmp(eax, isolate()->factory()->true_value());
      Split(equal, if_true, if_false, fall_through);
      break;

    case Token::INSTANCEOF: {
      VisitForAccumulatorValue(expr->right());
      __ Pop(edx);
      InstanceOfStub stub(isolate());
      __ CallStub(&stub);
      PrepareForBailoutBeforeSplit(expr, false, NULL, NULL);
      __ cmp(eax, isolate()->factory()->true_value());
      Split(equal, if_true, if_false, fall_through);
      break;
    }

    default: {
      VisitForAccumulatorValue(expr->right());
      Condition cc = CompareIC::ComputeCondition(op);
      __ pop(edx);

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

      Handle<Code> ic = CodeFactory::CompareIC(
                            isolate(), op, strength(language_mode())).code();
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
    Handle<Code> ic = CompareNilICStub::GetUninitialized(isolate(), nil);
    CallIC(ic, expr->CompareOperationFeedbackId());
    __ cmp(eax, isolate()->factory()->true_value());
    Split(equal, if_true, if_false, fall_through);
  }
  context()->Plug(if_true, if_false);
}


void FullCodeGenerator::VisitThisFunction(ThisFunction* expr) {
  __ mov(eax, Operand(ebp, JavaScriptFrameConstants::kFunctionOffset));
  context()->Plug(eax);
}


Register FullCodeGenerator::result_register() {
  return eax;
}


Register FullCodeGenerator::context_register() {
  return esi;
}


void FullCodeGenerator::StoreToFrameField(int frame_offset, Register value) {
  DCHECK_EQ(POINTER_SIZE_ALIGN(frame_offset), frame_offset);
  __ mov(Operand(ebp, frame_offset), value);
}


void FullCodeGenerator::LoadContextField(Register dst, int context_index) {
  __ mov(dst, ContextOperand(esi, context_index));
}


void FullCodeGenerator::PushFunctionArgumentForContextAllocation() {
  Scope* closure_scope = scope()->ClosureScope();
  if (closure_scope->is_script_scope() ||
      closure_scope->is_module_scope()) {
    // Contexts nested in the native context have a canonical empty function
    // as their closure, not the anonymous closure containing the global
    // code.
    __ mov(eax, NativeContextOperand());
    __ push(ContextOperand(eax, Context::CLOSURE_INDEX));
  } else if (closure_scope->is_eval_scope()) {
    // Contexts nested inside eval code have the same closure as the context
    // calling eval, not the anonymous closure containing the eval code.
    // Fetch it from the context.
    __ push(ContextOperand(esi, Context::CLOSURE_INDEX));
  } else {
    DCHECK(closure_scope->is_function_scope());
    __ push(Operand(ebp, JavaScriptFrameConstants::kFunctionOffset));
  }
}


// ----------------------------------------------------------------------------
// Non-local control flow support.

void FullCodeGenerator::EnterFinallyBlock() {
  // Cook return address on top of stack (smi encoded Code* delta)
  DCHECK(!result_register().is(edx));
  __ pop(edx);
  __ sub(edx, Immediate(masm_->CodeObject()));
  STATIC_ASSERT(kSmiTagSize + kSmiShiftSize == 1);
  STATIC_ASSERT(kSmiTag == 0);
  __ SmiTag(edx);
  __ push(edx);

  // Store result register while executing finally block.
  __ push(result_register());

  // Store pending message while executing finally block.
  ExternalReference pending_message_obj =
      ExternalReference::address_of_pending_message_obj(isolate());
  __ mov(edx, Operand::StaticVariable(pending_message_obj));
  __ push(edx);

  ClearPendingMessage();
}


void FullCodeGenerator::ExitFinallyBlock() {
  DCHECK(!result_register().is(edx));
  // Restore pending message from stack.
  __ pop(edx);
  ExternalReference pending_message_obj =
      ExternalReference::address_of_pending_message_obj(isolate());
  __ mov(Operand::StaticVariable(pending_message_obj), edx);

  // Restore result register from stack.
  __ pop(result_register());

  // Uncook return address.
  __ pop(edx);
  __ SmiUntag(edx);
  __ add(edx, Immediate(masm_->CodeObject()));
  __ jmp(edx);
}


void FullCodeGenerator::ClearPendingMessage() {
  DCHECK(!result_register().is(edx));
  ExternalReference pending_message_obj =
      ExternalReference::address_of_pending_message_obj(isolate());
  __ mov(edx, Immediate(isolate()->factory()->the_hole_value()));
  __ mov(Operand::StaticVariable(pending_message_obj), edx);
}


void FullCodeGenerator::EmitLoadStoreICSlot(FeedbackVectorSlot slot) {
  DCHECK(!slot.IsInvalid());
  __ mov(VectorStoreICTrampolineDescriptor::SlotRegister(),
         Immediate(SmiFromSlot(slot)));
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
    case OSR_AFTER_STACK_CHECK:
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

  if (Assembler::target_address_at(call_target_address, unoptimized_code) ==
      isolate->builtins()->OnStackReplacement()->entry()) {
    return ON_STACK_REPLACEMENT;
  }

  DCHECK_EQ(isolate->builtins()->OsrAfterStackCheck()->entry(),
            Assembler::target_address_at(call_target_address,
                                         unoptimized_code));
  return OSR_AFTER_STACK_CHECK;
}


}  // namespace internal
}  // namespace v8

#endif  // V8_TARGET_ARCH_IA32
