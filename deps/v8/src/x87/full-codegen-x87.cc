// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/v8.h"

#if V8_TARGET_ARCH_X87

#include "src/code-factory.h"
#include "src/code-stubs.h"
#include "src/codegen.h"
#include "src/compiler.h"
#include "src/debug.h"
#include "src/full-codegen.h"
#include "src/ic/ic.h"
#include "src/isolate-inl.h"
#include "src/parser.h"
#include "src/scopes.h"

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
//   o esi: our context
//   o ebp: our caller's frame pointer
//   o esp: stack pointer (pointing to return address)
//
// The function builds a JS frame.  Please see JavaScriptFrameConstants in
// frames-x87.h for its layout.
void FullCodeGenerator::Generate() {
  CompilationInfo* info = info_;
  handler_table_ =
      isolate()->factory()->NewFixedArray(function()->handler_count(), TENURED);

  profiling_counter_ = isolate()->factory()->NewCell(
      Handle<Smi>(Smi::FromInt(FLAG_interrupt_budget), isolate()));
  SetFunctionPosition(function());
  Comment cmnt(masm_, "[ function compiled by full code generator");

  ProfileEntryHookStub::MaybeCallEntryHook(masm_);

#ifdef DEBUG
  if (strlen(FLAG_stop_at) > 0 &&
      info->function()->name()->IsUtf8EqualTo(CStrVector(FLAG_stop_at))) {
    __ int3();
  }
#endif

  // Sloppy mode functions and builtins need to replace the receiver with the
  // global proxy when called as functions (without an explicit receiver
  // object).
  if (info->strict_mode() == SLOPPY && !info->is_native()) {
    Label ok;
    // +1 for return address.
    int receiver_offset = (info->scope()->num_parameters() + 1) * kPointerSize;
    __ mov(ecx, Operand(esp, receiver_offset));

    __ cmp(ecx, isolate()->factory()->undefined_value());
    __ j(not_equal, &ok, Label::kNear);

    __ mov(ecx, GlobalObjectOperand());
    __ mov(ecx, FieldOperand(ecx, GlobalObject::kGlobalProxyOffset));

    __ mov(Operand(esp, receiver_offset), ecx);

    __ bind(&ok);
  }

  // Open a frame scope to indicate that there is a frame on the stack.  The
  // MANUAL indicates that the scope shouldn't actually generate code to set up
  // the frame (that is done below).
  FrameScope frame_scope(masm_, StackFrame::MANUAL);

  info->set_prologue_offset(masm_->pc_offset());
  __ Prologue(info->IsCodePreAgingActive());
  info->AddNoFrameRange(0, masm_->pc_offset());

  { Comment cmnt(masm_, "[ Allocate locals");
    int locals_count = info->scope()->num_stack_slots();
    // Generators allocate locals, if any, in context slots.
    DCHECK(!info->function()->is_generator() || locals_count == 0);
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
        __ InvokeBuiltin(Builtins::STACK_OVERFLOW, CALL_FUNCTION);
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
  int heap_slots = info->scope()->num_heap_slots() - Context::MIN_CONTEXT_SLOTS;
  if (heap_slots > 0) {
    Comment cmnt(masm_, "[ Allocate context");
    bool need_write_barrier = true;
    // Argument to NewContext is the function, which is still in edi.
    if (FLAG_harmony_scoping && info->scope()->is_global_scope()) {
      __ push(edi);
      __ Push(info->scope()->GetScopeInfo());
      __ CallRuntime(Runtime::kNewGlobalContext, 2);
    } else if (heap_slots <= FastNewContextStub::kMaximumSlots) {
      FastNewContextStub stub(isolate(), heap_slots);
      __ CallStub(&stub);
      // Result of FastNewContextStub is always in new space.
      need_write_barrier = false;
    } else {
      __ push(edi);
      __ CallRuntime(Runtime::kNewFunctionContext, 1);
    }
    function_in_register = false;
    // Context is returned in eax.  It replaces the context passed to us.
    // It's saved in the stack and kept live in esi.
    __ mov(esi, eax);
    __ mov(Operand(ebp, StandardFrameConstants::kContextOffset), eax);

    // Copy parameters into context if necessary.
    int num_parameters = info->scope()->num_parameters();
    for (int i = 0; i < num_parameters; i++) {
      Variable* var = scope()->parameter(i);
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

  Variable* arguments = scope()->arguments();
  if (arguments != NULL) {
    // Function uses arguments object.
    Comment cmnt(masm_, "[ Allocate arguments object");
    if (function_in_register) {
      __ push(edi);
    } else {
      __ push(Operand(ebp, JavaScriptFrameConstants::kFunctionOffset));
    }
    // Receiver is just before the parameters on the caller's stack.
    int num_parameters = info->scope()->num_parameters();
    int offset = num_parameters * kPointerSize;
    __ lea(edx,
           Operand(ebp, StandardFrameConstants::kCallerSPOffset + offset));
    __ push(edx);
    __ push(Immediate(Smi::FromInt(num_parameters)));
    // Arguments to ArgumentsAccessStub:
    //   function, receiver address, parameter count.
    // The stub will rewrite receiver and parameter count if the previous
    // stack frame was an arguments adapter frame.
    ArgumentsAccessStub::Type type;
    if (strict_mode() == STRICT) {
      type = ArgumentsAccessStub::NEW_STRICT;
    } else if (function()->has_duplicate_parameters()) {
      type = ArgumentsAccessStub::NEW_SLOPPY_SLOW;
    } else {
      type = ArgumentsAccessStub::NEW_SLOPPY_FAST;
    }
    ArgumentsAccessStub stub(isolate(), type);
    __ CallStub(&stub);

    SetVar(arguments, eax, ebx, edx);
  }

  if (FLAG_trace) {
    __ CallRuntime(Runtime::kTraceEnter, 0);
  }

  // Visit the declarations and body unless there is an illegal
  // redeclaration.
  if (scope()->HasIllegalRedeclaration()) {
    Comment cmnt(masm_, "[ Declarations");
    scope()->VisitIllegalRedeclaration(this);

  } else {
    PrepareForBailoutForId(BailoutId::FunctionEntry(), NO_REGISTERS);
    { Comment cmnt(masm_, "[ Declarations");
      // For named function expressions, declare the function name as a
      // constant.
      if (scope()->is_function_scope() && scope()->function() != NULL) {
        VariableDeclaration* function = scope()->function();
        DCHECK(function->proxy()->var()->mode() == CONST ||
               function->proxy()->var()->mode() == CONST_LEGACY);
        DCHECK(function->proxy()->var()->location() != Variable::UNALLOCATED);
        VisitVariableDeclaration(function);
      }
      VisitDeclarations(scope()->declarations());
    }

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
      VisitStatements(function()->body());
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
      __ CallRuntime(Runtime::kTraceExit, 1);
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
#ifdef DEBUG
    // Add a label for checking the size of the code used for returning.
    Label check_exit_codesize;
    masm_->bind(&check_exit_codesize);
#endif
    SetSourcePosition(function()->end_position() - 1);
    __ RecordJSReturn();
    // Do not use the leave instruction here because it is too short to
    // patch with the code required by the debugger.
    __ mov(esp, ebp);
    int no_frame_start = masm_->pc_offset();
    __ pop(ebp);

    int arguments_bytes = (info_->scope()->num_parameters() + 1) * kPointerSize;
    __ Ret(arguments_bytes, ecx);
    // Check that the size of the code used for returning is large enough
    // for the debugger's requirements.
    DCHECK(Assembler::kJSReturnSequenceLength <=
           masm_->SizeOfCodeGeneratedSince(&check_exit_codesize));
    info_->AddNoFrameRange(no_frame_start, masm_->pc_offset());
  }
}


void FullCodeGenerator::EffectContext::Plug(Variable* var) const {
  DCHECK(var->IsStackAllocated() || var->IsContextSlot());
}


void FullCodeGenerator::AccumulatorValueContext::Plug(Variable* var) const {
  DCHECK(var->IsStackAllocated() || var->IsContextSlot());
  codegen()->GetVar(result_register(), var);
}


void FullCodeGenerator::StackValueContext::Plug(Variable* var) const {
  DCHECK(var->IsStackAllocated() || var->IsContextSlot());
  MemOperand operand = codegen()->VarOperand(var, result_register());
  // Memory operands can be pushed directly.
  __ push(operand);
}


void FullCodeGenerator::TestContext::Plug(Variable* var) const {
  // For simplicity we always test the accumulator register.
  codegen()->GetVar(result_register(), var);
  codegen()->PrepareForBailoutBeforeSplit(condition(), false, NULL, NULL);
  codegen()->DoTest(this);
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


void FullCodeGenerator::EffectContext::Plug(bool flag) const {
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
  __ test(result_register(), result_register());
  // The stub returns nonzero for true.
  Split(not_zero, if_true, if_false, fall_through);
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
  if (!context()->IsTest() || !info_->IsOptimizable()) return;

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
    case Variable::UNALLOCATED:
      globals_->Add(variable->name(), zone());
      globals_->Add(variable->binding_needs_init()
                        ? isolate()->factory()->the_hole_value()
                        : isolate()->factory()->undefined_value(), zone());
      break;

    case Variable::PARAMETER:
    case Variable::LOCAL:
      if (hole_init) {
        Comment cmnt(masm_, "[ VariableDeclaration");
        __ mov(StackOperand(variable),
               Immediate(isolate()->factory()->the_hole_value()));
      }
      break;

    case Variable::CONTEXT:
      if (hole_init) {
        Comment cmnt(masm_, "[ VariableDeclaration");
        EmitDebugCheckDeclarationContext(variable);
        __ mov(ContextOperand(esi, variable->index()),
               Immediate(isolate()->factory()->the_hole_value()));
        // No write barrier since the hole value is in old space.
        PrepareForBailoutForId(proxy->id(), NO_REGISTERS);
      }
      break;

    case Variable::LOOKUP: {
      Comment cmnt(masm_, "[ VariableDeclaration");
      __ push(esi);
      __ push(Immediate(variable->name()));
      // VariableDeclaration nodes are always introduced in one of four modes.
      DCHECK(IsDeclaredVariableMode(mode));
      PropertyAttributes attr =
          IsImmutableVariableMode(mode) ? READ_ONLY : NONE;
      __ push(Immediate(Smi::FromInt(attr)));
      // Push initial value, if any.
      // Note: For variables we must not push an initial value (such as
      // 'undefined') because we may have a (legal) redeclaration and we
      // must not destroy the current value.
      if (hole_init) {
        __ push(Immediate(isolate()->factory()->the_hole_value()));
      } else {
        __ push(Immediate(Smi::FromInt(0)));  // Indicates no initial value.
      }
      __ CallRuntime(Runtime::kDeclareLookupSlot, 4);
      break;
    }
  }
}


void FullCodeGenerator::VisitFunctionDeclaration(
    FunctionDeclaration* declaration) {
  VariableProxy* proxy = declaration->proxy();
  Variable* variable = proxy->var();
  switch (variable->location()) {
    case Variable::UNALLOCATED: {
      globals_->Add(variable->name(), zone());
      Handle<SharedFunctionInfo> function =
          Compiler::BuildFunctionInfo(declaration->fun(), script(), info_);
      // Check for stack-overflow exception.
      if (function.is_null()) return SetStackOverflow();
      globals_->Add(function, zone());
      break;
    }

    case Variable::PARAMETER:
    case Variable::LOCAL: {
      Comment cmnt(masm_, "[ FunctionDeclaration");
      VisitForAccumulatorValue(declaration->fun());
      __ mov(StackOperand(variable), result_register());
      break;
    }

    case Variable::CONTEXT: {
      Comment cmnt(masm_, "[ FunctionDeclaration");
      EmitDebugCheckDeclarationContext(variable);
      VisitForAccumulatorValue(declaration->fun());
      __ mov(ContextOperand(esi, variable->index()), result_register());
      // We know that we have written a function, which is not a smi.
      __ RecordWriteContextSlot(esi, Context::SlotOffset(variable->index()),
                                result_register(), ecx, kDontSaveFPRegs,
                                EMIT_REMEMBERED_SET, OMIT_SMI_CHECK);
      PrepareForBailoutForId(proxy->id(), NO_REGISTERS);
      break;
    }

    case Variable::LOOKUP: {
      Comment cmnt(masm_, "[ FunctionDeclaration");
      __ push(esi);
      __ push(Immediate(variable->name()));
      __ push(Immediate(Smi::FromInt(NONE)));
      VisitForStackValue(declaration->fun());
      __ CallRuntime(Runtime::kDeclareLookupSlot, 4);
      break;
    }
  }
}


void FullCodeGenerator::VisitModuleDeclaration(ModuleDeclaration* declaration) {
  Variable* variable = declaration->proxy()->var();
  DCHECK(variable->location() == Variable::CONTEXT);
  DCHECK(variable->interface()->IsFrozen());

  Comment cmnt(masm_, "[ ModuleDeclaration");
  EmitDebugCheckDeclarationContext(variable);

  // Load instance object.
  __ LoadContext(eax, scope_->ContextChainLength(scope_->GlobalScope()));
  __ mov(eax, ContextOperand(eax, variable->interface()->Index()));
  __ mov(eax, ContextOperand(eax, Context::EXTENSION_INDEX));

  // Assign it.
  __ mov(ContextOperand(esi, variable->index()), eax);
  // We know that we have written a module, which is not a smi.
  __ RecordWriteContextSlot(esi, Context::SlotOffset(variable->index()), eax,
                            ecx, kDontSaveFPRegs, EMIT_REMEMBERED_SET,
                            OMIT_SMI_CHECK);
  PrepareForBailoutForId(declaration->proxy()->id(), NO_REGISTERS);

  // Traverse into body.
  Visit(declaration->module());
}


void FullCodeGenerator::VisitImportDeclaration(ImportDeclaration* declaration) {
  VariableProxy* proxy = declaration->proxy();
  Variable* variable = proxy->var();
  switch (variable->location()) {
    case Variable::UNALLOCATED:
      // TODO(rossberg)
      break;

    case Variable::CONTEXT: {
      Comment cmnt(masm_, "[ ImportDeclaration");
      EmitDebugCheckDeclarationContext(variable);
      // TODO(rossberg)
      break;
    }

    case Variable::PARAMETER:
    case Variable::LOCAL:
    case Variable::LOOKUP:
      UNREACHABLE();
  }
}


void FullCodeGenerator::VisitExportDeclaration(ExportDeclaration* declaration) {
  // TODO(rossberg)
}


void FullCodeGenerator::DeclareGlobals(Handle<FixedArray> pairs) {
  // Call the runtime to declare the globals.
  __ push(esi);  // The context is the first argument.
  __ Push(pairs);
  __ Push(Smi::FromInt(DeclareGlobalsFlags()));
  __ CallRuntime(Runtime::kDeclareGlobals, 3);
  // Return value is ignored.
}


void FullCodeGenerator::DeclareModules(Handle<FixedArray> descriptions) {
  // Call the runtime to declare the modules.
  __ Push(descriptions);
  __ CallRuntime(Runtime::kDeclareModules, 1);
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

    // Record position before stub call for type feedback.
    SetSourcePosition(clause->position());
    Handle<Code> ic =
        CodeFactory::CompareIC(isolate(), Token::EQ_STRICT).code();
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
  FeedbackVectorSlot slot = stmt->ForInFeedbackSlot();

  SetStatementPosition(stmt);

  Label loop, exit;
  ForIn loop_statement(this, stmt);
  increment_loop_depth();

  // Get the object to enumerate over. If the object is null or undefined, skip
  // over the loop.  See ECMA-262 version 5, section 12.6.4.
  VisitForAccumulatorValue(stmt->enumerable());
  __ cmp(eax, isolate()->factory()->undefined_value());
  __ j(equal, &exit);
  __ cmp(eax, isolate()->factory()->null_value());
  __ j(equal, &exit);

  PrepareForBailoutForId(stmt->PrepareId(), TOS_REG);

  // Convert the object to a JS object.
  Label convert, done_convert;
  __ JumpIfSmi(eax, &convert, Label::kNear);
  __ CmpObjectType(eax, FIRST_SPEC_OBJECT_TYPE, ecx);
  __ j(above_equal, &done_convert, Label::kNear);
  __ bind(&convert);
  __ push(eax);
  __ InvokeBuiltin(Builtins::TO_OBJECT, CALL_FUNCTION);
  __ bind(&done_convert);
  PrepareForBailoutForId(stmt->ToObjectId(), TOS_REG);
  __ push(eax);

  // Check for proxies.
  Label call_runtime, use_cache, fixed_array;
  STATIC_ASSERT(FIRST_JS_PROXY_TYPE == FIRST_SPEC_OBJECT_TYPE);
  __ CmpObjectType(eax, LAST_JS_PROXY_TYPE, ecx);
  __ j(below_equal, &call_runtime);

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
  __ CallRuntime(Runtime::kGetPropertyNamesFast, 1);
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
  Label non_proxy;
  __ bind(&fixed_array);

  // No need for a write barrier, we are storing a Smi in the feedback vector.
  __ LoadHeapObject(ebx, FeedbackVector());
  int vector_index = FeedbackVector()->GetIndex(slot);
  __ mov(FieldOperand(ebx, FixedArray::OffsetOfElementAt(vector_index)),
         Immediate(TypeFeedbackVector::MegamorphicSentinel(isolate())));

  __ mov(ebx, Immediate(Smi::FromInt(1)));  // Smi indicates slow check
  __ mov(ecx, Operand(esp, 0 * kPointerSize));  // Get enumerated object
  STATIC_ASSERT(FIRST_JS_PROXY_TYPE == FIRST_SPEC_OBJECT_TYPE);
  __ CmpObjectType(ecx, LAST_JS_PROXY_TYPE, ecx);
  __ j(above, &non_proxy);
  __ Move(ebx, Immediate(Smi::FromInt(0)));  // Zero indicates proxy
  __ bind(&non_proxy);
  __ push(ebx);  // Smi
  __ push(eax);  // Array
  __ mov(eax, FieldOperand(eax, FixedArray::kLengthOffset));
  __ push(eax);  // Fixed array length (as smi).
  __ push(Immediate(Smi::FromInt(0)));  // Initial index.

  // Generate code for doing the condition check.
  PrepareForBailoutForId(stmt->BodyId(), NO_REGISTERS);
  __ bind(&loop);
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

  // For proxies, no filtering is done.
  // TODO(rossberg): What if only a prototype is a proxy? Not specified yet.
  DCHECK(Smi::FromInt(0) == 0);
  __ test(edx, edx);
  __ j(zero, &update_each);

  // Convert the entry to a string or null if it isn't a property
  // anymore. If the property has been removed while iterating, we
  // just skip it.
  __ push(ecx);  // Enumerable.
  __ push(ebx);  // Current entry.
  __ InvokeBuiltin(Builtins::FILTER_KEY, CALL_FUNCTION);
  __ test(eax, eax);
  __ j(equal, loop_statement.continue_label());
  __ mov(ebx, eax);

  // Update the 'each' property or variable from the possibly filtered
  // entry in register ebx.
  __ bind(&update_each);
  __ mov(result_register(), ebx);
  // Perform the assignment as if via '='.
  { EffectContext context(this);
    EmitAssignment(stmt->each());
  }

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


void FullCodeGenerator::VisitForOfStatement(ForOfStatement* stmt) {
  Comment cmnt(masm_, "[ ForOfStatement");
  SetStatementPosition(stmt);

  Iteration loop_statement(this, stmt);
  increment_loop_depth();

  // var iterator = iterable[Symbol.iterator]();
  VisitForEffect(stmt->assign_iterator());

  // Loop entry.
  __ bind(loop_statement.continue_label());

  // result = iterator.next()
  VisitForEffect(stmt->next_result());

  // if (result.done) break;
  Label result_not_done;
  VisitForControl(stmt->result_done(),
                  loop_statement.break_label(),
                  &result_not_done,
                  &result_not_done);
  __ bind(&result_not_done);

  // each = result.value
  VisitForEffect(stmt->assign_each());

  // Generate code for the body of the loop.
  Visit(stmt->body());

  // Check stack before looping.
  PrepareForBailoutForId(stmt->BackEdgeId(), NO_REGISTERS);
  EmitBackEdgeBookkeeping(stmt, loop_statement.continue_label());
  __ jmp(loop_statement.continue_label());

  // Exit and decrement the loop depth.
  PrepareForBailoutForId(stmt->ExitId(), NO_REGISTERS);
  __ bind(loop_statement.break_label());
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
    FastNewClosureStub stub(isolate(), info->strict_mode(), info->kind());
    __ mov(ebx, Immediate(info));
    __ CallStub(&stub);
  } else {
    __ push(esi);
    __ push(Immediate(info));
    __ push(Immediate(pretenure
                      ? isolate()->factory()->true_value()
                      : isolate()->factory()->false_value()));
    __ CallRuntime(Runtime::kNewClosure, 3);
  }
  context()->Plug(eax);
}


void FullCodeGenerator::VisitVariableProxy(VariableProxy* expr) {
  Comment cmnt(masm_, "[ VariableProxy");
  EmitVariableLoad(expr);
}


void FullCodeGenerator::EmitLoadHomeObject(SuperReference* expr) {
  Comment cnmt(masm_, "[ SuperReference ");

  __ mov(LoadDescriptor::ReceiverRegister(),
         Operand(ebp, JavaScriptFrameConstants::kFunctionOffset));

  Handle<Symbol> home_object_symbol(isolate()->heap()->home_object_symbol());
  __ mov(LoadDescriptor::NameRegister(), home_object_symbol);

  if (FLAG_vector_ics) {
    __ mov(VectorLoadICDescriptor::SlotRegister(),
           Immediate(SmiFromSlot(expr->HomeObjectFeedbackSlot())));
    CallLoadIC(NOT_CONTEXTUAL);
  } else {
    CallLoadIC(NOT_CONTEXTUAL, expr->HomeObjectFeedbackId());
  }

  __ cmp(eax, isolate()->factory()->undefined_value());
  Label done;
  __ j(not_equal, &done);
  __ CallRuntime(Runtime::kThrowNonMethodError, 0);
  __ bind(&done);
}


void FullCodeGenerator::EmitLoadGlobalCheckExtensions(VariableProxy* proxy,
                                                      TypeofState typeof_state,
                                                      Label* slow) {
  Register context = esi;
  Register temp = edx;

  Scope* s = scope();
  while (s != NULL) {
    if (s->num_heap_slots() > 0) {
      if (s->calls_sloppy_eval()) {
        // Check that extension is NULL.
        __ cmp(ContextOperand(context, Context::EXTENSION_INDEX),
               Immediate(0));
        __ j(not_equal, slow);
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
    // Check that extension is NULL.
    __ cmp(ContextOperand(temp, Context::EXTENSION_INDEX), Immediate(0));
    __ j(not_equal, slow);
    // Load next context in chain.
    __ mov(temp, ContextOperand(temp, Context::PREVIOUS_INDEX));
    __ jmp(&next);
    __ bind(&fast);
  }

  // All extension objects were empty and it is safe to use a global
  // load IC call.
  __ mov(LoadDescriptor::ReceiverRegister(), GlobalObjectOperand());
  __ mov(LoadDescriptor::NameRegister(), proxy->var()->name());
  if (FLAG_vector_ics) {
    __ mov(VectorLoadICDescriptor::SlotRegister(),
           Immediate(SmiFromSlot(proxy->VariableFeedbackSlot())));
  }

  ContextualMode mode = (typeof_state == INSIDE_TYPEOF)
      ? NOT_CONTEXTUAL
      : CONTEXTUAL;

  CallLoadIC(mode);
}


MemOperand FullCodeGenerator::ContextSlotOperandCheckExtensions(Variable* var,
                                                                Label* slow) {
  DCHECK(var->IsContextSlot());
  Register context = esi;
  Register temp = ebx;

  for (Scope* s = scope(); s != var->scope(); s = s->outer_scope()) {
    if (s->num_heap_slots() > 0) {
      if (s->calls_sloppy_eval()) {
        // Check that extension is NULL.
        __ cmp(ContextOperand(context, Context::EXTENSION_INDEX),
               Immediate(0));
        __ j(not_equal, slow);
      }
      __ mov(temp, ContextOperand(context, Context::PREVIOUS_INDEX));
      // Walk the rest of the chain without clobbering esi.
      context = temp;
    }
  }
  // Check that last extension is NULL.
  __ cmp(ContextOperand(context, Context::EXTENSION_INDEX), Immediate(0));
  __ j(not_equal, slow);

  // This function is used only for loads, not stores, so it's safe to
  // return an esi-based operand (the write barrier cannot be allowed to
  // destroy the esi register).
  return ContextOperand(context, var->index());
}


void FullCodeGenerator::EmitDynamicLookupFastCase(VariableProxy* proxy,
                                                  TypeofState typeof_state,
                                                  Label* slow,
                                                  Label* done) {
  // Generate fast-case code for variables that might be shadowed by
  // eval-introduced variables.  Eval is used a lot without
  // introducing variables.  In those cases, we do not want to
  // perform a runtime call for all variables in the scope
  // containing the eval.
  Variable* var = proxy->var();
  if (var->mode() == DYNAMIC_GLOBAL) {
    EmitLoadGlobalCheckExtensions(proxy, typeof_state, slow);
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
        __ CallRuntime(Runtime::kThrowReferenceError, 1);
      }
    }
    __ jmp(done);
  }
}


void FullCodeGenerator::EmitVariableLoad(VariableProxy* proxy) {
  // Record position before possible IC call.
  SetSourcePosition(proxy->position());
  Variable* var = proxy->var();

  // Three cases: global variables, lookup variables, and all other types of
  // variables.
  switch (var->location()) {
    case Variable::UNALLOCATED: {
      Comment cmnt(masm_, "[ Global variable");
      __ mov(LoadDescriptor::ReceiverRegister(), GlobalObjectOperand());
      __ mov(LoadDescriptor::NameRegister(), var->name());
      if (FLAG_vector_ics) {
        __ mov(VectorLoadICDescriptor::SlotRegister(),
               Immediate(SmiFromSlot(proxy->VariableFeedbackSlot())));
      }
      CallLoadIC(CONTEXTUAL);
      context()->Plug(eax);
      break;
    }

    case Variable::PARAMETER:
    case Variable::LOCAL:
    case Variable::CONTEXT: {
      Comment cmnt(masm_, var->IsContextSlot() ? "[ Context variable"
                                               : "[ Stack variable");
      if (var->binding_needs_init()) {
        // var->scope() may be NULL when the proxy is located in eval code and
        // refers to a potential outside binding. Currently those bindings are
        // always looked up dynamically, i.e. in that case
        //     var->location() == LOOKUP.
        // always holds.
        DCHECK(var->scope() != NULL);

        // Check if the binding really needs an initialization check. The check
        // can be skipped in the following situation: we have a LET or CONST
        // binding in harmony mode, both the Variable and the VariableProxy have
        // the same declaration scope (i.e. they are both in global code, in the
        // same function or in the same eval code) and the VariableProxy is in
        // the source physically located after the initializer of the variable.
        //
        // We cannot skip any initialization checks for CONST in non-harmony
        // mode because const variables may be declared but never initialized:
        //   if (false) { const x; }; var y = x;
        //
        // The condition on the declaration scopes is a conservative check for
        // nested functions that access a binding and are called before the
        // binding is initialized:
        //   function() { f(); let x = 1; function f() { x = 2; } }
        //
        bool skip_init_check;
        if (var->scope()->DeclarationScope() != scope()->DeclarationScope()) {
          skip_init_check = false;
        } else {
          // Check that we always have valid source position.
          DCHECK(var->initializer_position() != RelocInfo::kNoPosition);
          DCHECK(proxy->position() != RelocInfo::kNoPosition);
          skip_init_check = var->mode() != CONST_LEGACY &&
              var->initializer_position() < proxy->position();
        }

        if (!skip_init_check) {
          // Let and const need a read barrier.
          Label done;
          GetVar(eax, var);
          __ cmp(eax, isolate()->factory()->the_hole_value());
          __ j(not_equal, &done, Label::kNear);
          if (var->mode() == LET || var->mode() == CONST) {
            // Throw a reference error when using an uninitialized let/const
            // binding in harmony mode.
            __ push(Immediate(var->name()));
            __ CallRuntime(Runtime::kThrowReferenceError, 1);
          } else {
            // Uninitalized const bindings outside of harmony mode are unholed.
            DCHECK(var->mode() == CONST_LEGACY);
            __ mov(eax, isolate()->factory()->undefined_value());
          }
          __ bind(&done);
          context()->Plug(eax);
          break;
        }
      }
      context()->Plug(var);
      break;
    }

    case Variable::LOOKUP: {
      Comment cmnt(masm_, "[ Lookup variable");
      Label done, slow;
      // Generate code for loading from variables potentially shadowed
      // by eval-introduced variables.
      EmitDynamicLookupFastCase(proxy, NOT_INSIDE_TYPEOF, &slow, &done);
      __ bind(&slow);
      __ push(esi);  // Context.
      __ push(Immediate(var->name()));
      __ CallRuntime(Runtime::kLoadLookupSlot, 2);
      __ bind(&done);
      context()->Plug(eax);
      break;
    }
  }
}


void FullCodeGenerator::VisitRegExpLiteral(RegExpLiteral* expr) {
  Comment cmnt(masm_, "[ RegExpLiteral");
  Label materialized;
  // Registers will be used as follows:
  // edi = JS function.
  // ecx = literals array.
  // ebx = regexp literal.
  // eax = regexp literal clone.
  __ mov(edi, Operand(ebp, JavaScriptFrameConstants::kFunctionOffset));
  __ mov(ecx, FieldOperand(edi, JSFunction::kLiteralsOffset));
  int literal_offset =
      FixedArray::kHeaderSize + expr->literal_index() * kPointerSize;
  __ mov(ebx, FieldOperand(ecx, literal_offset));
  __ cmp(ebx, isolate()->factory()->undefined_value());
  __ j(not_equal, &materialized, Label::kNear);

  // Create regexp literal using runtime function
  // Result will be in eax.
  __ push(ecx);
  __ push(Immediate(Smi::FromInt(expr->literal_index())));
  __ push(Immediate(expr->pattern()));
  __ push(Immediate(expr->flags()));
  __ CallRuntime(Runtime::kMaterializeRegExpLiteral, 4);
  __ mov(ebx, eax);

  __ bind(&materialized);
  int size = JSRegExp::kSize + JSRegExp::kInObjectFieldCount * kPointerSize;
  Label allocated, runtime_allocate;
  __ Allocate(size, eax, ecx, edx, &runtime_allocate, TAG_OBJECT);
  __ jmp(&allocated);

  __ bind(&runtime_allocate);
  __ push(ebx);
  __ push(Immediate(Smi::FromInt(size)));
  __ CallRuntime(Runtime::kAllocateInNewSpace, 1);
  __ pop(ebx);

  __ bind(&allocated);
  // Copy the content into the newly allocated memory.
  // (Unroll copy loop once for better throughput).
  for (int i = 0; i < size - kPointerSize; i += 2 * kPointerSize) {
    __ mov(edx, FieldOperand(ebx, i));
    __ mov(ecx, FieldOperand(ebx, i + kPointerSize));
    __ mov(FieldOperand(eax, i), edx);
    __ mov(FieldOperand(eax, i + kPointerSize), ecx);
  }
  if ((size % (2 * kPointerSize)) != 0) {
    __ mov(edx, FieldOperand(ebx, size - kPointerSize));
    __ mov(FieldOperand(eax, size - kPointerSize), edx);
  }
  context()->Plug(eax);
}


void FullCodeGenerator::EmitAccessor(Expression* expression) {
  if (expression == NULL) {
    __ push(Immediate(isolate()->factory()->null_value()));
  } else {
    VisitForStackValue(expression);
  }
}


void FullCodeGenerator::VisitObjectLiteral(ObjectLiteral* expr) {
  Comment cmnt(masm_, "[ ObjectLiteral");

  expr->BuildConstantProperties(isolate());
  Handle<FixedArray> constant_properties = expr->constant_properties();
  int flags = expr->fast_elements()
      ? ObjectLiteral::kFastElements
      : ObjectLiteral::kNoFlags;
  flags |= expr->has_function()
      ? ObjectLiteral::kHasFunction
      : ObjectLiteral::kNoFlags;
  int properties_count = constant_properties->length() / 2;
  if (expr->may_store_doubles() || expr->depth() > 1 ||
      masm()->serializer_enabled() ||
      flags != ObjectLiteral::kFastElements ||
      properties_count > FastCloneShallowObjectStub::kMaximumClonedProperties) {
    __ mov(edi, Operand(ebp, JavaScriptFrameConstants::kFunctionOffset));
    __ push(FieldOperand(edi, JSFunction::kLiteralsOffset));
    __ push(Immediate(Smi::FromInt(expr->literal_index())));
    __ push(Immediate(constant_properties));
    __ push(Immediate(Smi::FromInt(flags)));
    __ CallRuntime(Runtime::kCreateObjectLiteral, 4);
  } else {
    __ mov(edi, Operand(ebp, JavaScriptFrameConstants::kFunctionOffset));
    __ mov(eax, FieldOperand(edi, JSFunction::kLiteralsOffset));
    __ mov(ebx, Immediate(Smi::FromInt(expr->literal_index())));
    __ mov(ecx, Immediate(constant_properties));
    __ mov(edx, Immediate(Smi::FromInt(flags)));
    FastCloneShallowObjectStub stub(isolate(), properties_count);
    __ CallStub(&stub);
  }
  PrepareForBailoutForId(expr->CreateLiteralId(), TOS_REG);

  // If result_saved is true the result is on top of the stack.  If
  // result_saved is false the result is in eax.
  bool result_saved = false;

  // Mark all computed expressions that are bound to a key that
  // is shadowed by a later occurrence of the same key. For the
  // marked expressions, no store code is emitted.
  expr->CalculateEmitStore(zone());

  AccessorTable accessor_table(zone());
  for (int i = 0; i < expr->properties()->length(); i++) {
    ObjectLiteral::Property* property = expr->properties()->at(i);
    if (property->IsCompileTimeValue()) continue;

    Literal* key = property->key();
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
            CallStoreIC(key->LiteralFeedbackId());
            PrepareForBailoutForId(key->id(), NO_REGISTERS);
          } else {
            VisitForEffect(value);
          }
          break;
        }
        __ push(Operand(esp, 0));  // Duplicate receiver.
        VisitForStackValue(key);
        VisitForStackValue(value);
        if (property->emit_store()) {
          __ push(Immediate(Smi::FromInt(SLOPPY)));  // Strict mode
          __ CallRuntime(Runtime::kSetProperty, 4);
        } else {
          __ Drop(3);
        }
        break;
      case ObjectLiteral::Property::PROTOTYPE:
        __ push(Operand(esp, 0));  // Duplicate receiver.
        VisitForStackValue(value);
        if (property->emit_store()) {
          __ CallRuntime(Runtime::kInternalSetPrototype, 2);
        } else {
          __ Drop(2);
        }
        break;
      case ObjectLiteral::Property::GETTER:
        accessor_table.lookup(key)->second->getter = value;
        break;
      case ObjectLiteral::Property::SETTER:
        accessor_table.lookup(key)->second->setter = value;
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
    __ CallRuntime(Runtime::kDefineAccessorPropertyUnchecked, 5);
  }

  if (expr->has_function()) {
    DCHECK(result_saved);
    __ push(Operand(esp, 0));
    __ CallRuntime(Runtime::kToFastProperties, 1);
  }

  if (result_saved) {
    context()->PlugTOS();
  } else {
    context()->Plug(eax);
  }
}


void FullCodeGenerator::VisitArrayLiteral(ArrayLiteral* expr) {
  Comment cmnt(masm_, "[ ArrayLiteral");

  expr->BuildConstantElements(isolate());
  int flags = expr->depth() == 1
      ? ArrayLiteral::kShallowElements
      : ArrayLiteral::kNoFlags;

  ZoneList<Expression*>* subexprs = expr->values();
  int length = subexprs->length();
  Handle<FixedArray> constant_elements = expr->constant_elements();
  DCHECK_EQ(2, constant_elements->length());
  ElementsKind constant_elements_kind =
      static_cast<ElementsKind>(Smi::cast(constant_elements->get(0))->value());
  bool has_constant_fast_elements =
      IsFastObjectElementsKind(constant_elements_kind);
  Handle<FixedArrayBase> constant_elements_values(
      FixedArrayBase::cast(constant_elements->get(1)));

  AllocationSiteMode allocation_site_mode = TRACK_ALLOCATION_SITE;
  if (has_constant_fast_elements && !FLAG_allocation_site_pretenuring) {
    // If the only customer of allocation sites is transitioning, then
    // we can turn it off if we don't have anywhere else to transition to.
    allocation_site_mode = DONT_TRACK_ALLOCATION_SITE;
  }

  if (expr->depth() > 1 || length > JSObject::kInitialMaxFastElementArray) {
    __ mov(ebx, Operand(ebp, JavaScriptFrameConstants::kFunctionOffset));
    __ push(FieldOperand(ebx, JSFunction::kLiteralsOffset));
    __ push(Immediate(Smi::FromInt(expr->literal_index())));
    __ push(Immediate(constant_elements));
    __ push(Immediate(Smi::FromInt(flags)));
    __ CallRuntime(Runtime::kCreateArrayLiteral, 4);
  } else {
    __ mov(ebx, Operand(ebp, JavaScriptFrameConstants::kFunctionOffset));
    __ mov(eax, FieldOperand(ebx, JSFunction::kLiteralsOffset));
    __ mov(ebx, Immediate(Smi::FromInt(expr->literal_index())));
    __ mov(ecx, Immediate(constant_elements));
    FastCloneShallowArrayStub stub(isolate(), allocation_site_mode);
    __ CallStub(&stub);
  }

  bool result_saved = false;  // Is the result saved to the stack?

  // Emit code to evaluate all the non-constant subexpressions and to store
  // them into the newly cloned array.
  for (int i = 0; i < length; i++) {
    Expression* subexpr = subexprs->at(i);
    // If the subexpression is a literal or a simple materialized literal it
    // is already set in the cloned array.
    if (CompileTimeValue::IsCompileTimeValue(subexpr)) continue;

    if (!result_saved) {
      __ push(eax);  // array literal.
      __ push(Immediate(Smi::FromInt(expr->literal_index())));
      result_saved = true;
    }
    VisitForAccumulatorValue(subexpr);

    if (IsFastObjectElementsKind(constant_elements_kind)) {
      // Fast-case array literal with ElementsKind of FAST_*_ELEMENTS, they
      // cannot transition and don't need to call the runtime stub.
      int offset = FixedArray::kHeaderSize + (i * kPointerSize);
      __ mov(ebx, Operand(esp, kPointerSize));  // Copy of array literal.
      __ mov(ebx, FieldOperand(ebx, JSObject::kElementsOffset));
      // Store the subexpression value in the array's elements.
      __ mov(FieldOperand(ebx, offset), result_register());
      // Update the write barrier for the array store.
      __ RecordWriteField(ebx, offset, result_register(), ecx, kDontSaveFPRegs,
                          EMIT_REMEMBERED_SET, INLINE_SMI_CHECK);
    } else {
      // Store the subexpression value in the array's elements.
      __ mov(ecx, Immediate(Smi::FromInt(i)));
      StoreArrayLiteralElementStub stub(isolate());
      __ CallStub(&stub);
    }

    PrepareForBailoutForId(expr->GetIdForElement(i), NO_REGISTERS);
  }

  if (result_saved) {
    __ add(esp, Immediate(kPointerSize));  // literal index
    context()->PlugTOS();
  } else {
    context()->Plug(eax);
  }
}


void FullCodeGenerator::VisitAssignment(Assignment* expr) {
  DCHECK(expr->target()->IsValidReferenceExpression());

  Comment cmnt(masm_, "[ Assignment");

  Property* property = expr->target()->AsProperty();
  LhsKind assign_type = GetAssignType(property);

  // Evaluate LHS expression.
  switch (assign_type) {
    case VARIABLE:
      // Nothing to do here.
      break;
    case NAMED_SUPER_PROPERTY:
      VisitForStackValue(property->obj()->AsSuperReference()->this_var());
      EmitLoadHomeObject(property->obj()->AsSuperReference());
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
      VisitForStackValue(property->obj()->AsSuperReference()->this_var());
      EmitLoadHomeObject(property->obj()->AsSuperReference());
      __ Push(result_register());
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

    OverwriteMode mode = expr->value()->ResultOverwriteAllowed()
        ? OVERWRITE_RIGHT
        : NO_OVERWRITE;
    SetSourcePosition(expr->position() + 1);
    if (ShouldInlineSmiCase(op)) {
      EmitInlineSmiBinaryOp(expr->binary_operation(),
                            op,
                            mode,
                            expr->target(),
                            expr->value());
    } else {
      EmitBinaryOp(expr->binary_operation(), op, mode);
    }

    // Deoptimization point in case the binary operation may have side effects.
    PrepareForBailout(expr->binary_operation(), TOS_REG);
  } else {
    VisitForAccumulatorValue(expr->value());
  }

  // Record source position before possible IC call.
  SetSourcePosition(expr->position());

  // Store the value.
  switch (assign_type) {
    case VARIABLE:
      EmitVariableAssignment(expr->target()->AsVariableProxy()->var(),
                             expr->op());
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
      handler_table()->set(expr->index(), Smi::FromInt(l_catch.pos()));
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
      __ PushTryHandler(StackHandler::CATCH, expr->index());
      const int handler_size = StackHandlerConstants::kSize;
      __ push(eax);                                      // result
      __ jmp(&l_suspend);
      __ bind(&l_continuation);
      __ jmp(&l_resume);
      __ bind(&l_suspend);
      const int generator_object_depth = kPointerSize + handler_size;
      __ mov(eax, Operand(esp, generator_object_depth));
      __ push(eax);                                      // g
      DCHECK(l_continuation.pos() > 0 && Smi::IsValid(l_continuation.pos()));
      __ mov(FieldOperand(eax, JSGeneratorObject::kContinuationOffset),
             Immediate(Smi::FromInt(l_continuation.pos())));
      __ mov(FieldOperand(eax, JSGeneratorObject::kContextOffset), esi);
      __ mov(ecx, esi);
      __ RecordWriteField(eax, JSGeneratorObject::kContextOffset, ecx, edx,
                          kDontSaveFPRegs);
      __ CallRuntime(Runtime::kSuspendJSGeneratorObject, 1);
      __ mov(context_register(),
             Operand(ebp, StandardFrameConstants::kContextOffset));
      __ pop(eax);                                       // result
      EmitReturnSequence();
      __ bind(&l_resume);                                // received in eax
      __ PopTryHandler();

      // receiver = iter; f = iter.next; arg = received;
      __ bind(&l_next);

      __ mov(load_name, isolate()->factory()->next_string());
      __ push(load_name);                           // "next"
      __ push(Operand(esp, 2 * kPointerSize));      // iter
      __ push(eax);                                 // received

      // result = receiver[f](arg);
      __ bind(&l_call);
      __ mov(load_receiver, Operand(esp, kPointerSize));
      if (FLAG_vector_ics) {
        __ mov(VectorLoadICDescriptor::SlotRegister(),
               Immediate(SmiFromSlot(expr->KeyedLoadFeedbackSlot())));
      }
      Handle<Code> ic = CodeFactory::KeyedLoadIC(isolate()).code();
      CallIC(ic, TypeFeedbackId::None());
      __ mov(edi, eax);
      __ mov(Operand(esp, 2 * kPointerSize), edi);
      CallFunctionStub stub(isolate(), 1, CALL_AS_METHOD);
      __ CallStub(&stub);

      __ mov(esi, Operand(ebp, StandardFrameConstants::kContextOffset));
      __ Drop(1);  // The function is still on the stack; drop it.

      // if (!result.done) goto l_try;
      __ bind(&l_loop);
      __ push(eax);                                      // save result
      __ Move(load_receiver, eax);                       // result
      __ mov(load_name,
             isolate()->factory()->done_string());       // "done"
      if (FLAG_vector_ics) {
        __ mov(VectorLoadICDescriptor::SlotRegister(),
               Immediate(SmiFromSlot(expr->DoneFeedbackSlot())));
      }
      CallLoadIC(NOT_CONTEXTUAL);                        // result.done in eax
      Handle<Code> bool_ic = ToBooleanStub::GetUninitialized(isolate());
      CallIC(bool_ic);
      __ test(eax, eax);
      __ j(zero, &l_try);

      // result.value
      __ pop(load_receiver);                              // result
      __ mov(load_name,
             isolate()->factory()->value_string());       // "value"
      if (FLAG_vector_ics) {
        __ mov(VectorLoadICDescriptor::SlotRegister(),
               Immediate(SmiFromSlot(expr->ValueFeedbackSlot())));
      }
      CallLoadIC(NOT_CONTEXTUAL);                         // result.value in eax
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

  // Check generator state.
  Label wrong_state, closed_state, done;
  STATIC_ASSERT(JSGeneratorObject::kGeneratorExecuting < 0);
  STATIC_ASSERT(JSGeneratorObject::kGeneratorClosed == 0);
  __ cmp(FieldOperand(ebx, JSGeneratorObject::kContinuationOffset),
         Immediate(Smi::FromInt(0)));
  __ j(equal, &closed_state);
  __ j(less, &wrong_state);

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
  Label resume_frame;
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
  __ CallRuntime(Runtime::kResumeJSGeneratorObject, 3);
  // Not reached: the runtime call returns elsewhere.
  __ Abort(kGeneratorFailedToResume);

  // Reach here when generator is closed.
  __ bind(&closed_state);
  if (resume_mode == JSGeneratorObject::NEXT) {
    // Return completed iterator result when generator is closed.
    __ push(Immediate(isolate()->factory()->undefined_value()));
    // Pop value from top-of-stack slot; box result into result register.
    EmitCreateIteratorResult(true);
  } else {
    // Throw the provided value.
    __ push(eax);
    __ CallRuntime(Runtime::kThrow, 1);
  }
  __ jmp(&done);

  // Throw error if we attempt to operate on a running generator.
  __ bind(&wrong_state);
  __ push(ebx);
  __ CallRuntime(Runtime::kThrowGeneratorStateError, 1);

  __ bind(&done);
  context()->Plug(result_register());
}


void FullCodeGenerator::EmitCreateIteratorResult(bool done) {
  Label gc_required;
  Label allocated;

  const int instance_size = 5 * kPointerSize;
  DCHECK_EQ(isolate()->native_context()->iterator_result_map()->instance_size(),
            instance_size);

  __ Allocate(instance_size, eax, ecx, edx, &gc_required, TAG_OBJECT);
  __ jmp(&allocated);

  __ bind(&gc_required);
  __ Push(Smi::FromInt(instance_size));
  __ CallRuntime(Runtime::kAllocateInNewSpace, 1);
  __ mov(context_register(),
         Operand(ebp, StandardFrameConstants::kContextOffset));

  __ bind(&allocated);
  __ mov(ebx, Operand(esi, Context::SlotOffset(Context::GLOBAL_OBJECT_INDEX)));
  __ mov(ebx, FieldOperand(ebx, GlobalObject::kNativeContextOffset));
  __ mov(ebx, ContextOperand(ebx, Context::ITERATOR_RESULT_MAP_INDEX));
  __ pop(ecx);
  __ mov(edx, isolate()->factory()->ToBoolean(done));
  __ mov(FieldOperand(eax, HeapObject::kMapOffset), ebx);
  __ mov(FieldOperand(eax, JSObject::kPropertiesOffset),
         isolate()->factory()->empty_fixed_array());
  __ mov(FieldOperand(eax, JSObject::kElementsOffset),
         isolate()->factory()->empty_fixed_array());
  __ mov(FieldOperand(eax, JSGeneratorObject::kResultValuePropertyOffset), ecx);
  __ mov(FieldOperand(eax, JSGeneratorObject::kResultDonePropertyOffset), edx);

  // Only the value field needs a write barrier, as the other values are in the
  // root set.
  __ RecordWriteField(eax, JSGeneratorObject::kResultValuePropertyOffset, ecx,
                      edx, kDontSaveFPRegs);
}


void FullCodeGenerator::EmitNamedPropertyLoad(Property* prop) {
  SetSourcePosition(prop->position());
  Literal* key = prop->key()->AsLiteral();
  DCHECK(!key->value()->IsSmi());
  DCHECK(!prop->IsSuperAccess());

  __ mov(LoadDescriptor::NameRegister(), Immediate(key->value()));
  if (FLAG_vector_ics) {
    __ mov(VectorLoadICDescriptor::SlotRegister(),
           Immediate(SmiFromSlot(prop->PropertyFeedbackSlot())));
    CallLoadIC(NOT_CONTEXTUAL);
  } else {
    CallLoadIC(NOT_CONTEXTUAL, prop->PropertyFeedbackId());
  }
}


void FullCodeGenerator::EmitNamedSuperPropertyLoad(Property* prop) {
  // Stack: receiver, home_object.
  SetSourcePosition(prop->position());
  Literal* key = prop->key()->AsLiteral();
  DCHECK(!key->value()->IsSmi());
  DCHECK(prop->IsSuperAccess());

  __ push(Immediate(key->value()));
  __ CallRuntime(Runtime::kLoadFromSuper, 3);
}


void FullCodeGenerator::EmitKeyedPropertyLoad(Property* prop) {
  SetSourcePosition(prop->position());
  Handle<Code> ic = CodeFactory::KeyedLoadIC(isolate()).code();
  if (FLAG_vector_ics) {
    __ mov(VectorLoadICDescriptor::SlotRegister(),
           Immediate(SmiFromSlot(prop->PropertyFeedbackSlot())));
    CallIC(ic);
  } else {
    CallIC(ic, prop->PropertyFeedbackId());
  }
}


void FullCodeGenerator::EmitKeyedSuperPropertyLoad(Property* prop) {
  // Stack: receiver, home_object, key.
  SetSourcePosition(prop->position());

  __ CallRuntime(Runtime::kLoadKeyedFromSuper, 3);
}


void FullCodeGenerator::EmitInlineSmiBinaryOp(BinaryOperation* expr,
                                              Token::Value op,
                                              OverwriteMode mode,
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
  Handle<Code> code = CodeFactory::BinaryOpIC(isolate(), op, mode).code();
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
    Literal* key = property->key()->AsLiteral();
    Expression* value = property->value();
    DCHECK(key != NULL);

    if (property->is_static()) {
      __ push(Operand(esp, kPointerSize));  // constructor
    } else {
      __ push(Operand(esp, 0));  // prototype
    }
    VisitForStackValue(key);
    VisitForStackValue(value);

    switch (property->kind()) {
      case ObjectLiteral::Property::CONSTANT:
      case ObjectLiteral::Property::MATERIALIZED_LITERAL:
      case ObjectLiteral::Property::COMPUTED:
      case ObjectLiteral::Property::PROTOTYPE:
        __ CallRuntime(Runtime::kDefineClassMethod, 3);
        break;

      case ObjectLiteral::Property::GETTER:
        __ CallRuntime(Runtime::kDefineClassGetter, 3);
        break;

      case ObjectLiteral::Property::SETTER:
        __ CallRuntime(Runtime::kDefineClassSetter, 3);
        break;

      default:
        UNREACHABLE();
    }
  }

  // prototype
  __ CallRuntime(Runtime::kToFastProperties, 1);

  // constructor
  __ CallRuntime(Runtime::kToFastProperties, 1);
}


void FullCodeGenerator::EmitBinaryOp(BinaryOperation* expr,
                                     Token::Value op,
                                     OverwriteMode mode) {
  __ pop(edx);
  Handle<Code> code = CodeFactory::BinaryOpIC(isolate(), op, mode).code();
  JumpPatchSite patch_site(masm_);    // unbound, signals no inlined smi code.
  CallIC(code, expr->BinaryOperationFeedbackId());
  patch_site.EmitPatchInfo();
  context()->Plug(eax);
}


void FullCodeGenerator::EmitAssignment(Expression* expr) {
  DCHECK(expr->IsValidReferenceExpression());

  Property* prop = expr->AsProperty();
  LhsKind assign_type = GetAssignType(prop);

  switch (assign_type) {
    case VARIABLE: {
      Variable* var = expr->AsVariableProxy()->var();
      EffectContext context(this);
      EmitVariableAssignment(var, Token::ASSIGN);
      break;
    }
    case NAMED_PROPERTY: {
      __ push(eax);  // Preserve value.
      VisitForAccumulatorValue(prop->obj());
      __ Move(StoreDescriptor::ReceiverRegister(), eax);
      __ pop(StoreDescriptor::ValueRegister());  // Restore value.
      __ mov(StoreDescriptor::NameRegister(),
             prop->key()->AsLiteral()->value());
      CallStoreIC();
      break;
    }
    case NAMED_SUPER_PROPERTY: {
      __ push(eax);
      VisitForStackValue(prop->obj()->AsSuperReference()->this_var());
      EmitLoadHomeObject(prop->obj()->AsSuperReference());
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
      VisitForStackValue(prop->obj()->AsSuperReference()->this_var());
      EmitLoadHomeObject(prop->obj()->AsSuperReference());
      __ push(result_register());
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
      Handle<Code> ic =
          CodeFactory::KeyedStoreIC(isolate(), strict_mode()).code();
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


void FullCodeGenerator::EmitVariableAssignment(Variable* var,
                                               Token::Value op) {
  if (var->IsUnallocated()) {
    // Global var, const, or let.
    __ mov(StoreDescriptor::NameRegister(), var->name());
    __ mov(StoreDescriptor::ReceiverRegister(), GlobalObjectOperand());
    CallStoreIC();

  } else if (op == Token::INIT_CONST_LEGACY) {
    // Const initializers need a write barrier.
    DCHECK(!var->IsParameter());  // No const parameters.
    if (var->IsLookupSlot()) {
      __ push(eax);
      __ push(esi);
      __ push(Immediate(var->name()));
      __ CallRuntime(Runtime::kInitializeLegacyConstLookupSlot, 3);
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

  } else if (var->mode() == LET && op != Token::INIT_LET) {
    // Non-initializing assignment to let variable needs a write barrier.
    DCHECK(!var->IsLookupSlot());
    DCHECK(var->IsStackAllocated() || var->IsContextSlot());
    Label assign;
    MemOperand location = VarOperand(var, ecx);
    __ mov(edx, location);
    __ cmp(edx, isolate()->factory()->the_hole_value());
    __ j(not_equal, &assign, Label::kNear);
    __ push(Immediate(var->name()));
    __ CallRuntime(Runtime::kThrowReferenceError, 1);
    __ bind(&assign);
    EmitStoreToStackLocalOrContextSlot(var, location);

  } else if (!var->is_const_mode() || op == Token::INIT_CONST) {
    if (var->IsLookupSlot()) {
      // Assignment to var.
      __ push(eax);  // Value.
      __ push(esi);  // Context.
      __ push(Immediate(var->name()));
      __ push(Immediate(Smi::FromInt(strict_mode())));
      __ CallRuntime(Runtime::kStoreLookupSlot, 4);
    } else {
      // Assignment to var or initializing assignment to let/const in harmony
      // mode.
      DCHECK(var->IsStackAllocated() || var->IsContextSlot());
      MemOperand location = VarOperand(var, ecx);
      if (generate_debug_code_ && op == Token::INIT_LET) {
        // Check for an uninitialized let binding.
        __ mov(edx, location);
        __ cmp(edx, isolate()->factory()->the_hole_value());
        __ Check(equal, kLetBindingReInitialization);
      }
      EmitStoreToStackLocalOrContextSlot(var, location);
    }
  }
  // Non-initializing assignments to consts are ignored.
}


void FullCodeGenerator::EmitNamedPropertyAssignment(Assignment* expr) {
  // Assignment to a property, using a named store IC.
  // eax    : value
  // esp[0] : receiver

  Property* prop = expr->target()->AsProperty();
  DCHECK(prop != NULL);
  DCHECK(prop->key()->IsLiteral());

  // Record source code position before IC call.
  SetSourcePosition(expr->position());
  __ mov(StoreDescriptor::NameRegister(), prop->key()->AsLiteral()->value());
  __ pop(StoreDescriptor::ReceiverRegister());
  CallStoreIC(expr->AssignmentFeedbackId());
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
  __ CallRuntime((strict_mode() == STRICT ? Runtime::kStoreToSuper_Strict
                                          : Runtime::kStoreToSuper_Sloppy),
                 4);
}


void FullCodeGenerator::EmitKeyedSuperPropertyStore(Property* prop) {
  // Assignment to named property of super.
  // eax : value
  // stack : receiver ('this'), home_object, key

  __ push(eax);
  __ CallRuntime((strict_mode() == STRICT ? Runtime::kStoreKeyedToSuper_Strict
                                          : Runtime::kStoreKeyedToSuper_Sloppy),
                 4);
}


void FullCodeGenerator::EmitKeyedPropertyAssignment(Assignment* expr) {
  // Assignment to a property, using a keyed store IC.
  // eax               : value
  // esp[0]            : key
  // esp[kPointerSize] : receiver

  __ pop(StoreDescriptor::NameRegister());  // Key.
  __ pop(StoreDescriptor::ReceiverRegister());
  DCHECK(StoreDescriptor::ValueRegister().is(eax));
  // Record source code position before IC call.
  SetSourcePosition(expr->position());
  Handle<Code> ic = CodeFactory::KeyedStoreIC(isolate(), strict_mode()).code();
  CallIC(ic, expr->AssignmentFeedbackId());

  PrepareForBailoutForId(expr->AssignmentId(), TOS_REG);
  context()->Plug(eax);
}


void FullCodeGenerator::VisitProperty(Property* expr) {
  Comment cmnt(masm_, "[ Property");
  Expression* key = expr->key();

  if (key->IsPropertyName()) {
    if (!expr->IsSuperAccess()) {
      VisitForAccumulatorValue(expr->obj());
      __ Move(LoadDescriptor::ReceiverRegister(), result_register());
      EmitNamedPropertyLoad(expr);
    } else {
      VisitForStackValue(expr->obj()->AsSuperReference()->this_var());
      EmitLoadHomeObject(expr->obj()->AsSuperReference());
      __ push(result_register());
      EmitNamedSuperPropertyLoad(expr);
    }
    PrepareForBailoutForId(expr->LoadId(), TOS_REG);
    context()->Plug(eax);
  } else {
    if (!expr->IsSuperAccess()) {
      VisitForStackValue(expr->obj());
      VisitForAccumulatorValue(expr->key());
      __ pop(LoadDescriptor::ReceiverRegister());                  // Object.
      __ Move(LoadDescriptor::NameRegister(), result_register());  // Key.
      EmitKeyedPropertyLoad(expr);
    } else {
      VisitForStackValue(expr->obj()->AsSuperReference()->this_var());
      EmitLoadHomeObject(expr->obj()->AsSuperReference());
      __ push(result_register());
      VisitForStackValue(expr->key());
      EmitKeyedSuperPropertyLoad(expr);
    }
    context()->Plug(eax);
  }
}


void FullCodeGenerator::CallIC(Handle<Code> code,
                               TypeFeedbackId ast_id) {
  ic_total_count_++;
  __ call(code, RelocInfo::CODE_TARGET, ast_id);
}


// Code common for calls using the IC.
void FullCodeGenerator::EmitCallWithLoadIC(Call* expr) {
  Expression* callee = expr->expression();

  CallICState::CallType call_type =
      callee->IsVariableProxy() ? CallICState::FUNCTION : CallICState::METHOD;
  // Get the target function.
  if (call_type == CallICState::FUNCTION) {
    { StackValueContext context(this);
      EmitVariableLoad(callee->AsVariableProxy());
      PrepareForBailout(callee, NO_REGISTERS);
    }
    // Push undefined as receiver. This is patched in the method prologue if it
    // is a sloppy mode method.
    __ push(Immediate(isolate()->factory()->undefined_value()));
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
  }

  EmitCall(expr, call_type);
}


void FullCodeGenerator::EmitSuperCallWithLoadIC(Call* expr) {
  Expression* callee = expr->expression();
  DCHECK(callee->IsProperty());
  Property* prop = callee->AsProperty();
  DCHECK(prop->IsSuperAccess());

  SetSourcePosition(prop->position());
  Literal* key = prop->key()->AsLiteral();
  DCHECK(!key->value()->IsSmi());
  // Load the function from the receiver.
  SuperReference* super_ref = callee->AsProperty()->obj()->AsSuperReference();
  EmitLoadHomeObject(super_ref);
  __ push(eax);
  VisitForAccumulatorValue(super_ref->this_var());
  __ push(eax);
  __ push(eax);
  __ push(Operand(esp, kPointerSize * 2));
  __ push(Immediate(key->value()));
  // Stack here:
  //  - home_object
  //  - this (receiver)
  //  - this (receiver) <-- LoadFromSuper will pop here and below.
  //  - home_object
  //  - key
  __ CallRuntime(Runtime::kLoadFromSuper, 3);

  // Replace home_object with target function.
  __ mov(Operand(esp, kPointerSize), eax);

  // Stack here:
  // - target function
  // - this (receiver)
  EmitCall(expr, CallICState::METHOD);
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

  EmitCall(expr, CallICState::METHOD);
}


void FullCodeGenerator::EmitKeyedSuperCallWithLoadIC(Call* expr) {
  Expression* callee = expr->expression();
  DCHECK(callee->IsProperty());
  Property* prop = callee->AsProperty();
  DCHECK(prop->IsSuperAccess());

  SetSourcePosition(prop->position());
  // Load the function from the receiver.
  SuperReference* super_ref = callee->AsProperty()->obj()->AsSuperReference();
  EmitLoadHomeObject(super_ref);
  __ push(eax);
  VisitForAccumulatorValue(super_ref->this_var());
  __ push(eax);
  __ push(eax);
  __ push(Operand(esp, kPointerSize * 2));
  VisitForStackValue(prop->key());
  // Stack here:
  //  - home_object
  //  - this (receiver)
  //  - this (receiver) <-- LoadKeyedFromSuper will pop here and below.
  //  - home_object
  //  - key
  __ CallRuntime(Runtime::kLoadKeyedFromSuper, 3);

  // Replace home_object with target function.
  __ mov(Operand(esp, kPointerSize), eax);

  // Stack here:
  // - target function
  // - this (receiver)
  EmitCall(expr, CallICState::METHOD);
}


void FullCodeGenerator::EmitCall(Call* expr, CallICState::CallType call_type) {
  // Load the arguments.
  ZoneList<Expression*>* args = expr->arguments();
  int arg_count = args->length();
  { PreservePositionScope scope(masm()->positions_recorder());
    for (int i = 0; i < arg_count; i++) {
      VisitForStackValue(args->at(i));
    }
  }

  // Record source position of the IC call.
  SetSourcePosition(expr->position());
  Handle<Code> ic = CallIC::initialize_stub(
      isolate(), arg_count, call_type);
  __ Move(edx, Immediate(SmiFromSlot(expr->CallFeedbackSlot())));
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
  // Push the receiver of the enclosing function.
  __ push(Operand(ebp, (2 + info_->scope()->num_parameters()) * kPointerSize));
  // Push the language mode.
  __ push(Immediate(Smi::FromInt(strict_mode())));

  // Push the start position of the scope the calls resides in.
  __ push(Immediate(Smi::FromInt(scope()->start_position())));

  // Do the runtime call.
  __ CallRuntime(Runtime::kResolvePossiblyDirectEval, 6);
}


void FullCodeGenerator::EmitLoadSuperConstructor(SuperReference* super_ref) {
  DCHECK(super_ref != NULL);
  __ push(Operand(ebp, JavaScriptFrameConstants::kFunctionOffset));
  __ CallRuntime(Runtime::kGetPrototype, 1);
}


void FullCodeGenerator::VisitCall(Call* expr) {
#ifdef DEBUG
  // We want to verify that RecordJSReturnSite gets called on all paths
  // through this function.  Avoid early returns.
  expr->return_is_recorded_ = false;
#endif

  Comment cmnt(masm_, "[ Call");
  Expression* callee = expr->expression();
  Call::CallType call_type = expr->GetCallType(isolate());

  if (call_type == Call::POSSIBLY_EVAL_CALL) {
    // In a call to eval, we first call RuntimeHidden_ResolvePossiblyDirectEval
    // to resolve the function we need to call and the receiver of the call.
    // Then we call the resolved function using the given arguments.
    ZoneList<Expression*>* args = expr->arguments();
    int arg_count = args->length();
    { PreservePositionScope pos_scope(masm()->positions_recorder());
      VisitForStackValue(callee);
      // Reserved receiver slot.
      __ push(Immediate(isolate()->factory()->undefined_value()));
      // Push the arguments.
      for (int i = 0; i < arg_count; i++) {
        VisitForStackValue(args->at(i));
      }

      // Push a copy of the function (found below the arguments) and
      // resolve eval.
      __ push(Operand(esp, (arg_count + 1) * kPointerSize));
      EmitResolvePossiblyDirectEval(arg_count);

      // The runtime call returns a pair of values in eax (function) and
      // edx (receiver). Touch up the stack with the right values.
      __ mov(Operand(esp, (arg_count + 0) * kPointerSize), edx);
      __ mov(Operand(esp, (arg_count + 1) * kPointerSize), eax);

      PrepareForBailoutForId(expr->EvalOrLookupId(), NO_REGISTERS);
    }
    // Record source position for debugger.
    SetSourcePosition(expr->position());
    CallFunctionStub stub(isolate(), arg_count, NO_CALL_FUNCTION_FLAGS);
    __ mov(edi, Operand(esp, (arg_count + 1) * kPointerSize));
    __ CallStub(&stub);
    RecordJSReturnSite(expr);
    // Restore context register.
    __ mov(esi, Operand(ebp, StandardFrameConstants::kContextOffset));
    context()->DropAndPlug(1, eax);

  } else if (call_type == Call::GLOBAL_CALL) {
    EmitCallWithLoadIC(expr);

  } else if (call_type == Call::LOOKUP_SLOT_CALL) {
    // Call to a lookup slot (dynamically introduced variable).
    VariableProxy* proxy = callee->AsVariableProxy();
    Label slow, done;
    { PreservePositionScope scope(masm()->positions_recorder());
      // Generate code for loading from variables potentially shadowed by
      // eval-introduced variables.
      EmitDynamicLookupFastCase(proxy, NOT_INSIDE_TYPEOF, &slow, &done);
    }
    __ bind(&slow);
    // Call the runtime to find the function to call (returned in eax) and
    // the object holding it (returned in edx).
    __ push(context_register());
    __ push(Immediate(proxy->name()));
    __ CallRuntime(Runtime::kLoadLookupSlot, 2);
    __ push(eax);  // Function.
    __ push(edx);  // Receiver.
    PrepareForBailoutForId(expr->EvalOrLookupId(), NO_REGISTERS);

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

    // The receiver is either the global receiver or an object found by
    // LoadContextSlot.
    EmitCall(expr);

  } else if (call_type == Call::PROPERTY_CALL) {
    Property* property = callee->AsProperty();
    bool is_named_call = property->key()->IsPropertyName();
    if (property->IsSuperAccess()) {
      if (is_named_call) {
        EmitSuperCallWithLoadIC(expr);
      } else {
        EmitKeyedSuperCallWithLoadIC(expr);
      }
    } else {
      {
        PreservePositionScope scope(masm()->positions_recorder());
        VisitForStackValue(property->obj());
      }
      if (is_named_call) {
        EmitCallWithLoadIC(expr);
      } else {
        EmitKeyedCallWithLoadIC(expr, property->key());
      }
    }
  } else if (call_type == Call::SUPER_CALL) {
    SuperReference* super_ref = callee->AsSuperReference();
    EmitLoadSuperConstructor(super_ref);
    __ push(result_register());
    VisitForStackValue(super_ref->this_var());
    EmitCall(expr, CallICState::METHOD);
  } else {
    DCHECK(call_type == Call::OTHER_CALL);
    // Call to an arbitrary expression not handled specially above.
    { PreservePositionScope scope(masm()->positions_recorder());
      VisitForStackValue(callee);
    }
    __ push(Immediate(isolate()->factory()->undefined_value()));
    // Emit function call.
    EmitCall(expr);
  }

#ifdef DEBUG
  // RecordJSReturnSite should have been called.
  DCHECK(expr->return_is_recorded_);
#endif
}


void FullCodeGenerator::VisitCallNew(CallNew* expr) {
  Comment cmnt(masm_, "[ CallNew");
  // According to ECMA-262, section 11.2.2, page 44, the function
  // expression in new calls must be evaluated before the
  // arguments.

  // Push constructor on the stack.  If it's not a function it's used as
  // receiver for CALL_NON_FUNCTION, otherwise the value on the stack is
  // ignored.
  if (expr->expression()->IsSuperReference()) {
    EmitLoadSuperConstructor(expr->expression()->AsSuperReference());
    __ push(result_register());
  } else {
    VisitForStackValue(expr->expression());
  }

  // Push the arguments ("left-to-right") on the stack.
  ZoneList<Expression*>* args = expr->arguments();
  int arg_count = args->length();
  for (int i = 0; i < arg_count; i++) {
    VisitForStackValue(args->at(i));
  }

  // Call the construct call builtin that handles allocation and
  // constructor invocation.
  SetSourcePosition(expr->position());

  // Load function and argument count into edi and eax.
  __ Move(eax, Immediate(arg_count));
  __ mov(edi, Operand(esp, arg_count * kPointerSize));

  // Record call targets in unoptimized code.
  if (FLAG_pretenuring_call_new) {
    EnsureSlotContainsAllocationSite(expr->AllocationSiteFeedbackSlot());
    DCHECK(expr->AllocationSiteFeedbackSlot().ToInt() ==
           expr->CallNewFeedbackSlot().ToInt() + 1);
  }

  __ LoadHeapObject(ebx, FeedbackVector());
  __ mov(edx, Immediate(SmiFromSlot(expr->CallNewFeedbackSlot())));

  CallConstructStub stub(isolate(), RECORD_CONSTRUCTOR_TARGET);
  __ call(stub.GetCode(), RelocInfo::CONSTRUCT_CALL);
  PrepareForBailoutForId(expr->ReturnId(), TOS_REG);
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


void FullCodeGenerator::EmitIsNonNegativeSmi(CallRuntime* expr) {
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
  __ test(eax, Immediate(kSmiTagMask | 0x80000000));
  Split(zero, if_true, if_false, fall_through);

  context()->Plug(if_true, if_false);
}


void FullCodeGenerator::EmitIsObject(CallRuntime* expr) {
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
  __ cmp(eax, isolate()->factory()->null_value());
  __ j(equal, if_true);
  __ mov(ebx, FieldOperand(eax, HeapObject::kMapOffset));
  // Undetectable objects behave like undefined when tested with typeof.
  __ movzx_b(ecx, FieldOperand(ebx, Map::kBitFieldOffset));
  __ test(ecx, Immediate(1 << Map::kIsUndetectable));
  __ j(not_zero, if_false);
  __ movzx_b(ecx, FieldOperand(ebx, Map::kInstanceTypeOffset));
  __ cmp(ecx, FIRST_NONCALLABLE_SPEC_OBJECT_TYPE);
  __ j(below, if_false);
  __ cmp(ecx, LAST_NONCALLABLE_SPEC_OBJECT_TYPE);
  PrepareForBailoutBeforeSplit(expr, true, if_true, if_false);
  Split(below_equal, if_true, if_false, fall_through);

  context()->Plug(if_true, if_false);
}


void FullCodeGenerator::EmitIsSpecObject(CallRuntime* expr) {
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
  __ CmpObjectType(eax, FIRST_SPEC_OBJECT_TYPE, ebx);
  PrepareForBailoutBeforeSplit(expr, true, if_true, if_false);
  Split(above_equal, if_true, if_false, fall_through);

  context()->Plug(if_true, if_false);
}


void FullCodeGenerator::EmitIsUndetectableObject(CallRuntime* expr) {
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
  __ mov(ebx, FieldOperand(eax, HeapObject::kMapOffset));
  __ movzx_b(ebx, FieldOperand(ebx, Map::kBitFieldOffset));
  __ test(ebx, Immediate(1 << Map::kIsUndetectable));
  PrepareForBailoutBeforeSplit(expr, true, if_true, if_false);
  Split(not_zero, if_true, if_false, fall_through);

  context()->Plug(if_true, if_false);
}


void FullCodeGenerator::EmitIsStringWrapperSafeForDefaultValueOf(
    CallRuntime* expr) {
  ZoneList<Expression*>* args = expr->arguments();
  DCHECK(args->length() == 1);

  VisitForAccumulatorValue(args->at(0));

  Label materialize_true, materialize_false, skip_lookup;
  Label* if_true = NULL;
  Label* if_false = NULL;
  Label* fall_through = NULL;
  context()->PrepareTest(&materialize_true, &materialize_false,
                         &if_true, &if_false, &fall_through);

  __ AssertNotSmi(eax);

  // Check whether this map has already been checked to be safe for default
  // valueOf.
  __ mov(ebx, FieldOperand(eax, HeapObject::kMapOffset));
  __ test_b(FieldOperand(ebx, Map::kBitField2Offset),
            1 << Map::kStringWrapperSafeForDefaultValueOf);
  __ j(not_zero, &skip_lookup);

  // Check for fast case object. Return false for slow case objects.
  __ mov(ecx, FieldOperand(eax, JSObject::kPropertiesOffset));
  __ mov(ecx, FieldOperand(ecx, HeapObject::kMapOffset));
  __ cmp(ecx, isolate()->factory()->hash_table_map());
  __ j(equal, if_false);

  // Look for valueOf string in the descriptor array, and indicate false if
  // found. Since we omit an enumeration index check, if it is added via a
  // transition that shares its descriptor array, this is a false positive.
  Label entry, loop, done;

  // Skip loop if no descriptors are valid.
  __ NumberOfOwnDescriptors(ecx, ebx);
  __ cmp(ecx, 0);
  __ j(equal, &done);

  __ LoadInstanceDescriptors(ebx, ebx);
  // ebx: descriptor array.
  // ecx: valid entries in the descriptor array.
  // Calculate the end of the descriptor array.
  STATIC_ASSERT(kSmiTag == 0);
  STATIC_ASSERT(kSmiTagSize == 1);
  STATIC_ASSERT(kPointerSize == 4);
  __ imul(ecx, ecx, DescriptorArray::kDescriptorSize);
  __ lea(ecx, Operand(ebx, ecx, times_4, DescriptorArray::kFirstOffset));
  // Calculate location of the first key name.
  __ add(ebx, Immediate(DescriptorArray::kFirstOffset));
  // Loop through all the keys in the descriptor array. If one of these is the
  // internalized string "valueOf" the result is false.
  __ jmp(&entry);
  __ bind(&loop);
  __ mov(edx, FieldOperand(ebx, 0));
  __ cmp(edx, isolate()->factory()->value_of_string());
  __ j(equal, if_false);
  __ add(ebx, Immediate(DescriptorArray::kDescriptorSize * kPointerSize));
  __ bind(&entry);
  __ cmp(ebx, ecx);
  __ j(not_equal, &loop);

  __ bind(&done);

  // Reload map as register ebx was used as temporary above.
  __ mov(ebx, FieldOperand(eax, HeapObject::kMapOffset));

  // Set the bit in the map to indicate that there is no local valueOf field.
  __ or_(FieldOperand(ebx, Map::kBitField2Offset),
         Immediate(1 << Map::kStringWrapperSafeForDefaultValueOf));

  __ bind(&skip_lookup);

  // If a valueOf property is not found on the object check that its
  // prototype is the un-modified String prototype. If not result is false.
  __ mov(ecx, FieldOperand(ebx, Map::kPrototypeOffset));
  __ JumpIfSmi(ecx, if_false);
  __ mov(ecx, FieldOperand(ecx, HeapObject::kMapOffset));
  __ mov(edx, Operand(esi, Context::SlotOffset(Context::GLOBAL_OBJECT_INDEX)));
  __ mov(edx,
         FieldOperand(edx, GlobalObject::kNativeContextOffset));
  __ cmp(ecx,
         ContextOperand(edx,
                        Context::STRING_FUNCTION_PROTOTYPE_MAP_INDEX));
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
  __ CmpObjectType(eax, JS_FUNCTION_TYPE, ebx);
  PrepareForBailoutBeforeSplit(expr, true, if_true, if_false);
  Split(equal, if_true, if_false, fall_through);

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
  Register map = ebx;
  __ mov(map, FieldOperand(eax, HeapObject::kMapOffset));
  __ CmpInstanceType(map, FIRST_JS_PROXY_TYPE);
  __ j(less, if_false);
  __ CmpInstanceType(map, LAST_JS_PROXY_TYPE);
  PrepareForBailoutBeforeSplit(expr, true, if_true, if_false);
  Split(less_equal, if_true, if_false, fall_through);

  context()->Plug(if_true, if_false);
}


void FullCodeGenerator::EmitIsConstructCall(CallRuntime* expr) {
  DCHECK(expr->arguments()->length() == 0);

  Label materialize_true, materialize_false;
  Label* if_true = NULL;
  Label* if_false = NULL;
  Label* fall_through = NULL;
  context()->PrepareTest(&materialize_true, &materialize_false,
                         &if_true, &if_false, &fall_through);

  // Get the frame pointer for the calling frame.
  __ mov(eax, Operand(ebp, StandardFrameConstants::kCallerFPOffset));

  // Skip the arguments adaptor frame if it exists.
  Label check_frame_marker;
  __ cmp(Operand(eax, StandardFrameConstants::kContextOffset),
         Immediate(Smi::FromInt(StackFrame::ARGUMENTS_ADAPTOR)));
  __ j(not_equal, &check_frame_marker);
  __ mov(eax, Operand(eax, StandardFrameConstants::kCallerFPOffset));

  // Check the marker in the calling frame.
  __ bind(&check_frame_marker);
  __ cmp(Operand(eax, StandardFrameConstants::kMarkerOffset),
         Immediate(Smi::FromInt(StackFrame::CONSTRUCT)));
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

  // If the object is a smi, we return null.
  __ JumpIfSmi(eax, &null);

  // Check that the object is a JS object but take special care of JS
  // functions to make sure they have 'Function' as their class.
  // Assume that there are only two callable types, and one of them is at
  // either end of the type range for JS object types. Saves extra comparisons.
  STATIC_ASSERT(NUM_OF_CALLABLE_SPEC_OBJECT_TYPES == 2);
  __ CmpObjectType(eax, FIRST_SPEC_OBJECT_TYPE, eax);
  // Map is now in eax.
  __ j(below, &null);
  STATIC_ASSERT(FIRST_NONCALLABLE_SPEC_OBJECT_TYPE ==
                FIRST_SPEC_OBJECT_TYPE + 1);
  __ j(equal, &function);

  __ CmpInstanceType(eax, LAST_SPEC_OBJECT_TYPE);
  STATIC_ASSERT(LAST_NONCALLABLE_SPEC_OBJECT_TYPE ==
                LAST_SPEC_OBJECT_TYPE - 1);
  __ j(equal, &function);
  // Assume that there is no larger type.
  STATIC_ASSERT(LAST_NONCALLABLE_SPEC_OBJECT_TYPE == LAST_TYPE - 1);

  // Check if the constructor in the map is a JS function.
  __ mov(eax, FieldOperand(eax, Map::kConstructorOffset));
  __ CmpObjectType(eax, JS_FUNCTION_TYPE, ebx);
  __ j(not_equal, &non_function_constructor);

  // eax now contains the constructor function. Grab the
  // instance class name from there.
  __ mov(eax, FieldOperand(eax, JSFunction::kSharedFunctionInfoOffset));
  __ mov(eax, FieldOperand(eax, SharedFunctionInfo::kInstanceClassNameOffset));
  __ jmp(&done);

  // Functions have class 'Function'.
  __ bind(&function);
  __ mov(eax, isolate()->factory()->Function_string());
  __ jmp(&done);

  // Objects with a non-function constructor have class 'Object'.
  __ bind(&non_function_constructor);
  __ mov(eax, isolate()->factory()->Object_string());
  __ jmp(&done);

  // Non-JS objects have class null.
  __ bind(&null);
  __ mov(eax, isolate()->factory()->null_value());

  // All done.
  __ bind(&done);

  context()->Plug(eax);
}


void FullCodeGenerator::EmitSubString(CallRuntime* expr) {
  // Load the arguments on the stack and call the stub.
  SubStringStub stub(isolate());
  ZoneList<Expression*>* args = expr->arguments();
  DCHECK(args->length() == 3);
  VisitForStackValue(args->at(0));
  VisitForStackValue(args->at(1));
  VisitForStackValue(args->at(2));
  __ CallStub(&stub);
  context()->Plug(eax);
}


void FullCodeGenerator::EmitRegExpExec(CallRuntime* expr) {
  // Load the arguments on the stack and call the stub.
  RegExpExecStub stub(isolate());
  ZoneList<Expression*>* args = expr->arguments();
  DCHECK(args->length() == 4);
  VisitForStackValue(args->at(0));
  VisitForStackValue(args->at(1));
  VisitForStackValue(args->at(2));
  VisitForStackValue(args->at(3));
  __ CallStub(&stub);
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


void FullCodeGenerator::EmitDateField(CallRuntime* expr) {
  ZoneList<Expression*>* args = expr->arguments();
  DCHECK(args->length() == 2);
  DCHECK_NE(NULL, args->at(1)->AsLiteral());
  Smi* index = Smi::cast(*(args->at(1)->AsLiteral()->value()));

  VisitForAccumulatorValue(args->at(0));  // Load the object.

  Label runtime, done, not_date_object;
  Register object = eax;
  Register result = eax;
  Register scratch = ecx;

  __ JumpIfSmi(object, &not_date_object);
  __ CmpObjectType(object, JS_DATE_TYPE, scratch);
  __ j(not_equal, &not_date_object);

  if (index->value() == 0) {
    __ mov(result, FieldOperand(object, JSDate::kValueOffset));
    __ jmp(&done);
  } else {
    if (index->value() < JSDate::kFirstUncachedField) {
      ExternalReference stamp = ExternalReference::date_cache_stamp(isolate());
      __ mov(scratch, Operand::StaticVariable(stamp));
      __ cmp(scratch, FieldOperand(object, JSDate::kCacheStampOffset));
      __ j(not_equal, &runtime, Label::kNear);
      __ mov(result, FieldOperand(object, JSDate::kValueOffset +
                                          kPointerSize * index->value()));
      __ jmp(&done);
    }
    __ bind(&runtime);
    __ PrepareCallCFunction(2, scratch);
    __ mov(Operand(esp, 0), object);
    __ mov(Operand(esp, 1 * kPointerSize), Immediate(index));
    __ CallCFunction(ExternalReference::get_date_field_function(isolate()), 2);
    __ jmp(&done);
  }

  __ bind(&not_date_object);
  __ CallRuntime(Runtime::kThrowNotDateError, 0);
  __ bind(&done);
  context()->Plug(result);
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


void FullCodeGenerator::EmitMathPow(CallRuntime* expr) {
  // Load the arguments on the stack and call the runtime function.
  ZoneList<Expression*>* args = expr->arguments();
  DCHECK(args->length() == 2);
  VisitForStackValue(args->at(0));
  VisitForStackValue(args->at(1));

  __ CallRuntime(Runtime::kMathPowSlow, 2);
  context()->Plug(eax);
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


void FullCodeGenerator::EmitNumberToString(CallRuntime* expr) {
  ZoneList<Expression*>* args = expr->arguments();
  DCHECK_EQ(args->length(), 1);

  // Load the argument into eax and call the stub.
  VisitForAccumulatorValue(args->at(0));

  NumberToStringStub stub(isolate());
  __ CallStub(&stub);
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
  generator.GenerateSlow(masm_, call_helper);

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
  generator.GenerateSlow(masm_, call_helper);

  __ bind(&done);
  context()->Plug(result);
}


void FullCodeGenerator::EmitStringAdd(CallRuntime* expr) {
  ZoneList<Expression*>* args = expr->arguments();
  DCHECK_EQ(2, args->length());
  VisitForStackValue(args->at(0));
  VisitForAccumulatorValue(args->at(1));

  __ pop(edx);
  StringAddStub stub(isolate(), STRING_ADD_CHECK_BOTH, NOT_TENURED);
  __ CallStub(&stub);
  context()->Plug(eax);
}


void FullCodeGenerator::EmitStringCompare(CallRuntime* expr) {
  ZoneList<Expression*>* args = expr->arguments();
  DCHECK_EQ(2, args->length());

  VisitForStackValue(args->at(0));
  VisitForStackValue(args->at(1));

  StringCompareStub stub(isolate());
  __ CallStub(&stub);
  context()->Plug(eax);
}


void FullCodeGenerator::EmitCallFunction(CallRuntime* expr) {
  ZoneList<Expression*>* args = expr->arguments();
  DCHECK(args->length() >= 2);

  int arg_count = args->length() - 2;  // 2 ~ receiver and function.
  for (int i = 0; i < arg_count + 1; ++i) {
    VisitForStackValue(args->at(i));
  }
  VisitForAccumulatorValue(args->last());  // Function.

  Label runtime, done;
  // Check for non-function argument (including proxy).
  __ JumpIfSmi(eax, &runtime);
  __ CmpObjectType(eax, JS_FUNCTION_TYPE, ebx);
  __ j(not_equal, &runtime);

  // InvokeFunction requires the function in edi. Move it in there.
  __ mov(edi, result_register());
  ParameterCount count(arg_count);
  __ InvokeFunction(edi, count, CALL_FUNCTION, NullCallWrapper());
  __ mov(esi, Operand(ebp, StandardFrameConstants::kContextOffset));
  __ jmp(&done);

  __ bind(&runtime);
  __ push(eax);
  __ CallRuntime(Runtime::kCall, args->length());
  __ bind(&done);

  context()->Plug(eax);
}


void FullCodeGenerator::EmitRegExpConstructResult(CallRuntime* expr) {
  // Load the arguments on the stack and call the stub.
  RegExpConstructResultStub stub(isolate());
  ZoneList<Expression*>* args = expr->arguments();
  DCHECK(args->length() == 3);
  VisitForStackValue(args->at(0));
  VisitForStackValue(args->at(1));
  VisitForAccumulatorValue(args->at(2));
  __ pop(ebx);
  __ pop(ecx);
  __ CallStub(&stub);
  context()->Plug(eax);
}


void FullCodeGenerator::EmitGetFromCache(CallRuntime* expr) {
  ZoneList<Expression*>* args = expr->arguments();
  DCHECK_EQ(2, args->length());

  DCHECK_NE(NULL, args->at(0)->AsLiteral());
  int cache_id = Smi::cast(*(args->at(0)->AsLiteral()->value()))->value();

  Handle<FixedArray> jsfunction_result_caches(
      isolate()->native_context()->jsfunction_result_caches());
  if (jsfunction_result_caches->length() <= cache_id) {
    __ Abort(kAttemptToUseUndefinedCache);
    __ mov(eax, isolate()->factory()->undefined_value());
    context()->Plug(eax);
    return;
  }

  VisitForAccumulatorValue(args->at(1));

  Register key = eax;
  Register cache = ebx;
  Register tmp = ecx;
  __ mov(cache, ContextOperand(esi, Context::GLOBAL_OBJECT_INDEX));
  __ mov(cache,
         FieldOperand(cache, GlobalObject::kNativeContextOffset));
  __ mov(cache, ContextOperand(cache, Context::JSFUNCTION_RESULT_CACHES_INDEX));
  __ mov(cache,
         FieldOperand(cache, FixedArray::OffsetOfElementAt(cache_id)));

  Label done, not_found;
  STATIC_ASSERT(kSmiTag == 0 && kSmiTagSize == 1);
  __ mov(tmp, FieldOperand(cache, JSFunctionResultCache::kFingerOffset));
  // tmp now holds finger offset as a smi.
  __ cmp(key, FixedArrayElementOperand(cache, tmp));
  __ j(not_equal, &not_found);

  __ mov(eax, FixedArrayElementOperand(cache, tmp, 1));
  __ jmp(&done);

  __ bind(&not_found);
  // Call runtime to perform the lookup.
  __ push(cache);
  __ push(key);
  __ CallRuntime(Runtime::kGetFromCache, 2);

  __ bind(&done);
  context()->Plug(eax);
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


void FullCodeGenerator::VisitCallRuntime(CallRuntime* expr) {
  if (expr->function() != NULL &&
      expr->function()->intrinsic_type == Runtime::INLINE) {
    Comment cmnt(masm_, "[ InlineRuntimeCall");
    EmitInlineRuntimeCall(expr);
    return;
  }

  Comment cmnt(masm_, "[ CallRuntime");
  ZoneList<Expression*>* args = expr->arguments();

  if (expr->is_jsruntime()) {
    // Push the builtins object as receiver.
    __ mov(eax, GlobalObjectOperand());
    __ push(FieldOperand(eax, GlobalObject::kBuiltinsOffset));

    // Load the function from the receiver.
    __ mov(LoadDescriptor::ReceiverRegister(), Operand(esp, 0));
    __ mov(LoadDescriptor::NameRegister(), Immediate(expr->name()));
    if (FLAG_vector_ics) {
      __ mov(VectorLoadICDescriptor::SlotRegister(),
             Immediate(SmiFromSlot(expr->CallRuntimeFeedbackSlot())));
      CallLoadIC(NOT_CONTEXTUAL);
    } else {
      CallLoadIC(NOT_CONTEXTUAL, expr->CallRuntimeFeedbackId());
    }

    // Push the target function under the receiver.
    __ push(Operand(esp, 0));
    __ mov(Operand(esp, kPointerSize), eax);

    // Code common for calls using the IC.
    ZoneList<Expression*>* args = expr->arguments();
    int arg_count = args->length();
    for (int i = 0; i < arg_count; i++) {
      VisitForStackValue(args->at(i));
    }

    // Record source position of the IC call.
    SetSourcePosition(expr->position());
    CallFunctionStub stub(isolate(), arg_count, NO_CALL_FUNCTION_FLAGS);
    __ mov(edi, Operand(esp, (arg_count + 1) * kPointerSize));
    __ CallStub(&stub);
    // Restore context register.
    __ mov(esi, Operand(ebp, StandardFrameConstants::kContextOffset));
    context()->DropAndPlug(1, eax);

  } else {
    // Push the arguments ("left-to-right").
    int arg_count = args->length();
    for (int i = 0; i < arg_count; i++) {
      VisitForStackValue(args->at(i));
    }

    // Call the C runtime function.
    __ CallRuntime(expr->function(), arg_count);

    context()->Plug(eax);
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
        __ push(Immediate(Smi::FromInt(strict_mode())));
        __ InvokeBuiltin(Builtins::DELETE, CALL_FUNCTION);
        context()->Plug(eax);
      } else if (proxy != NULL) {
        Variable* var = proxy->var();
        // Delete of an unqualified identifier is disallowed in strict mode
        // but "delete this" is allowed.
        DCHECK(strict_mode() == SLOPPY || var->is_this());
        if (var->IsUnallocated()) {
          __ push(GlobalObjectOperand());
          __ push(Immediate(var->name()));
          __ push(Immediate(Smi::FromInt(SLOPPY)));
          __ InvokeBuiltin(Builtins::DELETE, CALL_FUNCTION);
          context()->Plug(eax);
        } else if (var->IsStackAllocated() || var->IsContextSlot()) {
          // Result of deleting non-global variables is false.  'this' is
          // not really a variable, though we implement it as one.  The
          // subexpression does not have side effects.
          context()->Plug(var->is_this());
        } else {
          // Non-global variable.  Call the runtime to try to delete from the
          // context where the variable was introduced.
          __ push(context_register());
          __ push(Immediate(var->name()));
          __ CallRuntime(Runtime::kDeleteLookupSlot, 2);
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
      { StackValueContext context(this);
        VisitForTypeofValue(expr->expression());
      }
      __ CallRuntime(Runtime::kTypeof, 1);
      context()->Plug(eax);
      break;
    }

    default:
      UNREACHABLE();
  }
}


void FullCodeGenerator::VisitCountOperation(CountOperation* expr) {
  DCHECK(expr->expression()->IsValidReferenceExpression());

  Comment cmnt(masm_, "[ CountOperation");
  SetSourcePosition(expr->position());

  Property* prop = expr->expression()->AsProperty();
  LhsKind assign_type = GetAssignType(prop);

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
        VisitForStackValue(prop->obj()->AsSuperReference()->this_var());
        EmitLoadHomeObject(prop->obj()->AsSuperReference());
        __ push(result_register());
        __ push(MemOperand(esp, kPointerSize));
        __ push(result_register());
        EmitNamedSuperPropertyLoad(prop);
        break;
      }

      case KEYED_SUPER_PROPERTY: {
        VisitForStackValue(prop->obj()->AsSuperReference()->this_var());
        EmitLoadHomeObject(prop->obj()->AsSuperReference());
        __ push(result_register());
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
  ToNumberStub convert_stub(isolate());
  __ CallStub(&convert_stub);

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

  // Record position before stub call.
  SetSourcePosition(expr->position());

  // Call stub for +1/-1.
  __ bind(&stub_call);
  __ mov(edx, eax);
  __ mov(eax, Immediate(Smi::FromInt(1)));
  Handle<Code> code = CodeFactory::BinaryOpIC(isolate(), expr->binary_op(),
                                              NO_OVERWRITE).code();
  CallIC(code, expr->CountBinOpFeedbackId());
  patch_site.EmitPatchInfo();
  __ bind(&done);

  // Store the value returned in eax.
  switch (assign_type) {
    case VARIABLE:
      if (expr->is_postfix()) {
        // Perform the assignment as if via '='.
        { EffectContext context(this);
          EmitVariableAssignment(expr->expression()->AsVariableProxy()->var(),
                                 Token::ASSIGN);
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
                               Token::ASSIGN);
        PrepareForBailoutForId(expr->AssignmentId(), TOS_REG);
        context()->Plug(eax);
      }
      break;
    case NAMED_PROPERTY: {
      __ mov(StoreDescriptor::NameRegister(),
             prop->key()->AsLiteral()->value());
      __ pop(StoreDescriptor::ReceiverRegister());
      CallStoreIC(expr->CountStoreFeedbackId());
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
          CodeFactory::KeyedStoreIC(isolate(), strict_mode()).code();
      CallIC(ic, expr->CountStoreFeedbackId());
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


void FullCodeGenerator::VisitForTypeofValue(Expression* expr) {
  VariableProxy* proxy = expr->AsVariableProxy();
  DCHECK(!context()->IsEffect());
  DCHECK(!context()->IsTest());

  if (proxy != NULL && proxy->var()->IsUnallocated()) {
    Comment cmnt(masm_, "[ Global variable");
    __ mov(LoadDescriptor::ReceiverRegister(), GlobalObjectOperand());
    __ mov(LoadDescriptor::NameRegister(), Immediate(proxy->name()));
    if (FLAG_vector_ics) {
      __ mov(VectorLoadICDescriptor::SlotRegister(),
             Immediate(SmiFromSlot(proxy->VariableFeedbackSlot())));
    }
    // Use a regular load, not a contextual load, to avoid a reference
    // error.
    CallLoadIC(NOT_CONTEXTUAL);
    PrepareForBailout(expr, TOS_REG);
    context()->Plug(eax);
  } else if (proxy != NULL && proxy->var()->IsLookupSlot()) {
    Comment cmnt(masm_, "[ Lookup slot");
    Label done, slow;

    // Generate code for loading from variables potentially shadowed
    // by eval-introduced variables.
    EmitDynamicLookupFastCase(proxy, INSIDE_TYPEOF, &slow, &done);

    __ bind(&slow);
    __ push(esi);
    __ push(Immediate(proxy->name()));
    __ CallRuntime(Runtime::kLoadLookupSlotNoReferenceError, 2);
    PrepareForBailout(expr, TOS_REG);
    __ bind(&done);

    context()->Plug(eax);
  } else {
    // This expression cannot throw a reference error at the top level.
    VisitInDuplicateContext(expr);
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
    __ j(above_equal, if_false);
    // Check for undetectable objects => false.
    __ test_b(FieldOperand(edx, Map::kBitFieldOffset),
              1 << Map::kIsUndetectable);
    Split(zero, if_true, if_false, fall_through);
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
    __ movzx_b(ecx, FieldOperand(edx, Map::kBitFieldOffset));
    __ test(ecx, Immediate(1 << Map::kIsUndetectable));
    Split(not_zero, if_true, if_false, fall_through);
  } else if (String::Equals(check, factory->function_string())) {
    __ JumpIfSmi(eax, if_false);
    STATIC_ASSERT(NUM_OF_CALLABLE_SPEC_OBJECT_TYPES == 2);
    __ CmpObjectType(eax, JS_FUNCTION_TYPE, edx);
    __ j(equal, if_true);
    __ CmpInstanceType(edx, JS_FUNCTION_PROXY_TYPE);
    Split(equal, if_true, if_false, fall_through);
  } else if (String::Equals(check, factory->object_string())) {
    __ JumpIfSmi(eax, if_false);
    __ cmp(eax, isolate()->factory()->null_value());
    __ j(equal, if_true);
    __ CmpObjectType(eax, FIRST_NONCALLABLE_SPEC_OBJECT_TYPE, edx);
    __ j(below, if_false);
    __ CmpInstanceType(edx, LAST_NONCALLABLE_SPEC_OBJECT_TYPE);
    __ j(above, if_false);
    // Check for undetectable objects => false.
    __ test_b(FieldOperand(edx, Map::kBitFieldOffset),
              1 << Map::kIsUndetectable);
    Split(zero, if_true, if_false, fall_through);
  } else {
    if (if_false != fall_through) __ jmp(if_false);
  }
  context()->Plug(if_true, if_false);
}


void FullCodeGenerator::VisitCompareOperation(CompareOperation* expr) {
  Comment cmnt(masm_, "[ CompareOperation");
  SetSourcePosition(expr->position());

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
      __ InvokeBuiltin(Builtins::IN, CALL_FUNCTION);
      PrepareForBailoutBeforeSplit(expr, false, NULL, NULL);
      __ cmp(eax, isolate()->factory()->true_value());
      Split(equal, if_true, if_false, fall_through);
      break;

    case Token::INSTANCEOF: {
      VisitForStackValue(expr->right());
      InstanceofStub stub(isolate(), InstanceofStub::kNoFlags);
      __ CallStub(&stub);
      PrepareForBailoutBeforeSplit(expr, true, if_true, if_false);
      __ test(eax, eax);
      // The stub returns 0 for true.
      Split(zero, if_true, if_false, fall_through);
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

      // Record position and call the compare IC.
      SetSourcePosition(expr->position());
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
    Handle<Code> ic = CompareNilICStub::GetUninitialized(isolate(), nil);
    CallIC(ic, expr->CompareOperationFeedbackId());
    __ test(eax, eax);
    Split(not_zero, if_true, if_false, fall_through);
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
  Scope* declaration_scope = scope()->DeclarationScope();
  if (declaration_scope->is_global_scope() ||
      declaration_scope->is_module_scope()) {
    // Contexts nested in the native context have a canonical empty function
    // as their closure, not the anonymous closure containing the global
    // code.  Pass a smi sentinel and let the runtime look up the empty
    // function.
    __ push(Immediate(Smi::FromInt(0)));
  } else if (declaration_scope->is_eval_scope()) {
    // Contexts nested inside eval code have the same closure as the context
    // calling eval, not the anonymous closure containing the eval code.
    // Fetch it from the context.
    __ push(ContextOperand(esi, Context::CLOSURE_INDEX));
  } else {
    DCHECK(declaration_scope->is_function_scope());
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

  ExternalReference has_pending_message =
      ExternalReference::address_of_has_pending_message(isolate());
  __ mov(edx, Operand::StaticVariable(has_pending_message));
  __ SmiTag(edx);
  __ push(edx);

  ExternalReference pending_message_script =
      ExternalReference::address_of_pending_message_script(isolate());
  __ mov(edx, Operand::StaticVariable(pending_message_script));
  __ push(edx);
}


void FullCodeGenerator::ExitFinallyBlock() {
  DCHECK(!result_register().is(edx));
  // Restore pending message from stack.
  __ pop(edx);
  ExternalReference pending_message_script =
      ExternalReference::address_of_pending_message_script(isolate());
  __ mov(Operand::StaticVariable(pending_message_script), edx);

  __ pop(edx);
  __ SmiUntag(edx);
  ExternalReference has_pending_message =
      ExternalReference::address_of_has_pending_message(isolate());
  __ mov(Operand::StaticVariable(has_pending_message), edx);

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


#undef __

#define __ ACCESS_MASM(masm())

FullCodeGenerator::NestedStatement* FullCodeGenerator::TryFinally::Exit(
    int* stack_depth,
    int* context_length) {
  // The macros used here must preserve the result register.

  // Because the handler block contains the context of the finally
  // code, we can restore it directly from there for the finally code
  // rather than iteratively unwinding contexts via their previous
  // links.
  __ Drop(*stack_depth);  // Down to the handler block.
  if (*context_length > 0) {
    // Restore the context to its dedicated register and the stack.
    __ mov(esi, Operand(esp, StackHandlerConstants::kContextOffset));
    __ mov(Operand(ebp, StandardFrameConstants::kContextOffset), esi);
  }
  __ PopTryHandler();
  __ call(finally_entry_);

  *stack_depth = 0;
  *context_length = 0;
  return previous_;
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

  Assembler::set_target_address_at(call_target_address,
                                   unoptimized_code,
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


} }  // namespace v8::internal

#endif  // V8_TARGET_ARCH_X87
