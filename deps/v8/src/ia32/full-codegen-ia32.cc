// Copyright 2011 the V8 project authors. All rights reserved.
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

#if defined(V8_TARGET_ARCH_IA32)

#include "code-stubs.h"
#include "codegen.h"
#include "compiler.h"
#include "debug.h"
#include "full-codegen.h"
#include "parser.h"
#include "scopes.h"
#include "stub-cache.h"

namespace v8 {
namespace internal {

#define __ ACCESS_MASM(masm_)


static unsigned GetPropertyId(Property* property) {
  return property->id();
}


class JumpPatchSite BASE_EMBEDDED {
 public:
  explicit JumpPatchSite(MacroAssembler* masm) : masm_(masm) {
#ifdef DEBUG
    info_emitted_ = false;
#endif
  }

  ~JumpPatchSite() {
    ASSERT(patch_site_.is_bound() == info_emitted_);
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
      ASSERT(is_int8(delta_to_patch_site));
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
    ASSERT(!patch_site_.is_bound() && !info_emitted_);
    ASSERT(cc == carry || cc == not_carry);
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
//   o edi: the JS function object being called (ie, ourselves)
//   o esi: our context
//   o ebp: our caller's frame pointer
//   o esp: stack pointer (pointing to return address)
//
// The function builds a JS frame.  Please see JavaScriptFrameConstants in
// frames-ia32.h for its layout.
void FullCodeGenerator::Generate(CompilationInfo* info) {
  ASSERT(info_ == NULL);
  info_ = info;
  scope_ = info->scope();
  SetFunctionPosition(function());
  Comment cmnt(masm_, "[ function compiled by full code generator");

#ifdef DEBUG
  if (strlen(FLAG_stop_at) > 0 &&
      info->function()->name()->IsEqualTo(CStrVector(FLAG_stop_at))) {
    __ int3();
  }
#endif

  // Strict mode functions and builtins need to replace the receiver
  // with undefined when called as functions (without an explicit
  // receiver object). ecx is zero for method calls and non-zero for
  // function calls.
  if (info->is_strict_mode() || info->is_native()) {
    Label ok;
    __ test(ecx, Operand(ecx));
    __ j(zero, &ok, Label::kNear);
    // +1 for return address.
    int receiver_offset = (info->scope()->num_parameters() + 1) * kPointerSize;
    __ mov(Operand(esp, receiver_offset),
           Immediate(isolate()->factory()->undefined_value()));
    __ bind(&ok);
  }

  __ push(ebp);  // Caller's frame pointer.
  __ mov(ebp, esp);
  __ push(esi);  // Callee's context.
  __ push(edi);  // Callee's JS Function.

  { Comment cmnt(masm_, "[ Allocate locals");
    int locals_count = info->scope()->num_stack_slots();
    if (locals_count == 1) {
      __ push(Immediate(isolate()->factory()->undefined_value()));
    } else if (locals_count > 1) {
      __ mov(eax, Immediate(isolate()->factory()->undefined_value()));
      for (int i = 0; i < locals_count; i++) {
        __ push(eax);
      }
    }
  }

  set_stack_height(2 + scope()->num_stack_slots());
  if (FLAG_verify_stack_height) {
    verify_stack_height();
  }

  bool function_in_register = true;

  // Possibly allocate a local context.
  int heap_slots = info->scope()->num_heap_slots() - Context::MIN_CONTEXT_SLOTS;
  if (heap_slots > 0) {
    Comment cmnt(masm_, "[ Allocate local context");
    // Argument to NewContext is the function, which is still in edi.
    __ push(edi);
    if (heap_slots <= FastNewContextStub::kMaximumSlots) {
      FastNewContextStub stub(heap_slots);
      __ CallStub(&stub);
    } else {
      __ CallRuntime(Runtime::kNewFunctionContext, 1);
    }
    function_in_register = false;
    // Context is returned in both eax and esi.  It replaces the context
    // passed to us.  It's saved in the stack and kept live in esi.
    __ mov(Operand(ebp, StandardFrameConstants::kContextOffset), esi);

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
        // Update the write barrier. This clobbers all involved
        // registers, so we have use a third register to avoid
        // clobbering esi.
        __ mov(ecx, esi);
        __ RecordWrite(ecx, context_offset, eax, ebx);
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
    __ SafePush(Immediate(Smi::FromInt(num_parameters)));
    // Arguments to ArgumentsAccessStub and/or New...:
    //   function, receiver address, parameter count.
    // The stub will rewrite receiver and parameter count if the previous
    // stack frame was an arguments adapter frame.
    ArgumentsAccessStub::Type type;
    if (is_strict_mode()) {
      type = ArgumentsAccessStub::NEW_STRICT;
    } else if (function()->has_duplicate_parameters()) {
      type = ArgumentsAccessStub::NEW_NON_STRICT_SLOW;
    } else {
      type = ArgumentsAccessStub::NEW_NON_STRICT_FAST;
    }
    ArgumentsAccessStub stub(type);
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
    PrepareForBailoutForId(AstNode::kFunctionEntryId, NO_REGISTERS);
    { Comment cmnt(masm_, "[ Declarations");
      // For named function expressions, declare the function name as a
      // constant.
      if (scope()->is_function_scope() && scope()->function() != NULL) {
        int ignored = 0;
        EmitDeclaration(scope()->function(), Variable::CONST, NULL, &ignored);
      }
      VisitDeclarations(scope()->declarations());
    }

    { Comment cmnt(masm_, "[ Stack check");
      PrepareForBailoutForId(AstNode::kDeclarationsId, NO_REGISTERS);
      Label ok;
      ExternalReference stack_limit =
          ExternalReference::address_of_stack_limit(isolate());
      __ cmp(esp, Operand::StaticVariable(stack_limit));
      __ j(above_equal, &ok, Label::kNear);
      StackCheckStub stub;
      __ CallStub(&stub);
      __ bind(&ok);
    }

    { Comment cmnt(masm_, "[ Body");
      ASSERT(loop_depth() == 0);
      VisitStatements(function()->body());
      ASSERT(loop_depth() == 0);
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
  __ Set(eax, Immediate(Smi::FromInt(0)));
}


void FullCodeGenerator::EmitStackCheck(IterationStatement* stmt) {
  Comment cmnt(masm_, "[ Stack check");
  Label ok;
  ExternalReference stack_limit =
      ExternalReference::address_of_stack_limit(isolate());
  __ cmp(esp, Operand::StaticVariable(stack_limit));
  __ j(above_equal, &ok, Label::kNear);
  StackCheckStub stub;
  __ CallStub(&stub);
  // Record a mapping of this PC offset to the OSR id.  This is used to find
  // the AST id from the unoptimized code in order to use it as a key into
  // the deoptimization input data found in the optimized code.
  RecordStackCheck(stmt->OsrEntryId());

  // Loop stack checks can be patched to perform on-stack replacement. In
  // order to decide whether or not to perform OSR we embed the loop depth
  // in a test instruction after the call so we can extract it from the OSR
  // builtin.
  ASSERT(loop_depth() > 0);
  __ test(eax, Immediate(Min(loop_depth(), Code::kMaxLoopNestingMarker)));

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
    __ pop(ebp);

    int arguments_bytes = (info_->scope()->num_parameters() + 1) * kPointerSize;
    __ Ret(arguments_bytes, ecx);
#ifdef ENABLE_DEBUGGER_SUPPORT
    // Check that the size of the code used for returning is large enough
    // for the debugger's requirements.
    ASSERT(Assembler::kJSReturnSequenceLength <=
           masm_->SizeOfCodeGeneratedSince(&check_exit_codesize));
#endif
  }
}


void FullCodeGenerator::verify_stack_height() {
  ASSERT(FLAG_verify_stack_height);
  __ sub(Operand(ebp), Immediate(kPointerSize * stack_height()));
  __ cmp(ebp, Operand(esp));
  __ Assert(equal, "Full codegen stack height not as expected.");
  __ add(Operand(ebp), Immediate(kPointerSize * stack_height()));
}


void FullCodeGenerator::EffectContext::Plug(Variable* var) const {
  ASSERT(var->IsStackAllocated() || var->IsContextSlot());
}


void FullCodeGenerator::AccumulatorValueContext::Plug(Variable* var) const {
  ASSERT(var->IsStackAllocated() || var->IsContextSlot());
  codegen()->GetVar(result_register(), var);
}


void FullCodeGenerator::StackValueContext::Plug(Variable* var) const {
  ASSERT(var->IsStackAllocated() || var->IsContextSlot());
  MemOperand operand = codegen()->VarOperand(var, result_register());
  // Memory operands can be pushed directly.
  __ push(operand);
  codegen()->increment_stack_height();
}


void FullCodeGenerator::TestContext::Plug(Variable* var) const {
  // For simplicity we always test the accumulator register.
  codegen()->GetVar(result_register(), var);
  codegen()->PrepareForBailoutBeforeSplit(TOS_REG, false, NULL, NULL);
  codegen()->DoTest(this);
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
    __ SafeSet(result_register(), Immediate(lit));
  } else {
    __ Set(result_register(), Immediate(lit));
  }
}


void FullCodeGenerator::StackValueContext::Plug(Handle<Object> lit) const {
  if (lit->IsSmi()) {
    __ SafePush(Immediate(lit));
  } else {
    __ push(Immediate(lit));
  }
  codegen()->increment_stack_height();
}


void FullCodeGenerator::TestContext::Plug(Handle<Object> lit) const {
  codegen()->PrepareForBailoutBeforeSplit(TOS_REG,
                                          true,
                                          true_label_,
                                          false_label_);
  ASSERT(!lit->IsUndetectableObject());  // There are no undetectable literals.
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
  ASSERT(count > 0);
  __ Drop(count);
  codegen()->decrement_stack_height(count);
}


void FullCodeGenerator::AccumulatorValueContext::DropAndPlug(
    int count,
    Register reg) const {
  ASSERT(count > 0);
  __ Drop(count);
  __ Move(result_register(), reg);
  codegen()->decrement_stack_height(count);
}


void FullCodeGenerator::StackValueContext::DropAndPlug(int count,
                                                       Register reg) const {
  ASSERT(count > 0);
  if (count > 1) __ Drop(count - 1);
  __ mov(Operand(esp, 0), reg);
  codegen()->decrement_stack_height(count - 1);
}


void FullCodeGenerator::TestContext::DropAndPlug(int count,
                                                 Register reg) const {
  ASSERT(count > 0);
  // For simplicity we always test the accumulator register.
  __ Drop(count);
  __ Move(result_register(), reg);
  codegen()->PrepareForBailoutBeforeSplit(TOS_REG, false, NULL, NULL);
  codegen()->DoTest(this);
  codegen()->decrement_stack_height(count);
}


void FullCodeGenerator::EffectContext::Plug(Label* materialize_true,
                                            Label* materialize_false) const {
  ASSERT(materialize_true == materialize_false);
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
  codegen()->increment_stack_height();
}


void FullCodeGenerator::TestContext::Plug(Label* materialize_true,
                                          Label* materialize_false) const {
  ASSERT(materialize_true == true_label_);
  ASSERT(materialize_false == false_label_);
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
  codegen()->increment_stack_height();
}


void FullCodeGenerator::TestContext::Plug(bool flag) const {
  codegen()->PrepareForBailoutBeforeSplit(TOS_REG,
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
  ToBooleanStub stub(result_register());
  __ push(result_register());
  __ CallStub(&stub, condition->test_id());
  __ test(result_register(), Operand(result_register()));
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
  ASSERT(var->IsStackAllocated());
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
  ASSERT(var->IsContextSlot() || var->IsStackAllocated());
  if (var->IsContextSlot()) {
    int context_chain_length = scope()->ContextChainLength(var->scope());
    __ LoadContext(scratch, context_chain_length);
    return ContextOperand(scratch, var->index());
  } else {
    return StackOperand(var);
  }
}


void FullCodeGenerator::GetVar(Register dest, Variable* var) {
  ASSERT(var->IsContextSlot() || var->IsStackAllocated());
  MemOperand location = VarOperand(var, dest);
  __ mov(dest, location);
}


void FullCodeGenerator::SetVar(Variable* var,
                               Register src,
                               Register scratch0,
                               Register scratch1) {
  ASSERT(var->IsContextSlot() || var->IsStackAllocated());
  ASSERT(!scratch0.is(src));
  ASSERT(!scratch0.is(scratch1));
  ASSERT(!scratch1.is(src));
  MemOperand location = VarOperand(var, scratch0);
  __ mov(location, src);
  // Emit the write barrier code if the location is in the heap.
  if (var->IsContextSlot()) {
    int offset = Context::SlotOffset(var->index());
    ASSERT(!scratch0.is(esi) && !src.is(esi) && !scratch1.is(esi));
    __ RecordWrite(scratch0, offset, src, scratch1);
  }
}


void FullCodeGenerator::PrepareForBailoutBeforeSplit(State state,
                                                     bool should_normalize,
                                                     Label* if_true,
                                                     Label* if_false) {
  // Only prepare for bailouts before splits if we're in a test
  // context. Otherwise, we let the Visit function deal with the
  // preparation to avoid preparing with the same AST id twice.
  if (!context()->IsTest() || !info_->IsOptimizable()) return;

  Label skip;
  if (should_normalize) __ jmp(&skip, Label::kNear);

  ForwardBailoutStack* current = forward_bailout_stack_;
  while (current != NULL) {
    PrepareForBailout(current->expr(), state);
    current = current->parent();
  }

  if (should_normalize) {
    __ cmp(eax, isolate()->factory()->true_value());
    Split(equal, if_true, if_false, NULL);
    __ bind(&skip);
  }
}


void FullCodeGenerator::EmitDeclaration(VariableProxy* proxy,
                                        Variable::Mode mode,
                                        FunctionLiteral* function,
                                        int* global_count) {
  // If it was not possible to allocate the variable at compile time, we
  // need to "declare" it at runtime to make sure it actually exists in the
  // local context.
  Variable* variable = proxy->var();
  switch (variable->location()) {
    case Variable::UNALLOCATED:
      ++(*global_count);
      break;

    case Variable::PARAMETER:
    case Variable::LOCAL:
      if (function != NULL) {
        Comment cmnt(masm_, "[ Declaration");
        VisitForAccumulatorValue(function);
        __ mov(StackOperand(variable), result_register());
      } else if (mode == Variable::CONST || mode == Variable::LET) {
        Comment cmnt(masm_, "[ Declaration");
        __ mov(StackOperand(variable),
               Immediate(isolate()->factory()->the_hole_value()));
      }
      break;

    case Variable::CONTEXT:
      // The variable in the decl always resides in the current function
      // context.
      ASSERT_EQ(0, scope()->ContextChainLength(variable->scope()));
      if (FLAG_debug_code) {
        // Check that we're not inside a with or catch context.
        __ mov(ebx, FieldOperand(esi, HeapObject::kMapOffset));
        __ cmp(ebx, isolate()->factory()->with_context_map());
        __ Check(not_equal, "Declaration in with context.");
        __ cmp(ebx, isolate()->factory()->catch_context_map());
        __ Check(not_equal, "Declaration in catch context.");
      }
      if (function != NULL) {
        Comment cmnt(masm_, "[ Declaration");
        VisitForAccumulatorValue(function);
        __ mov(ContextOperand(esi, variable->index()), result_register());
        int offset = Context::SlotOffset(variable->index());
        __ mov(ebx, esi);
        __ RecordWrite(ebx, offset, result_register(), ecx);
        PrepareForBailoutForId(proxy->id(), NO_REGISTERS);
      } else if (mode == Variable::CONST || mode == Variable::LET) {
        Comment cmnt(masm_, "[ Declaration");
        __ mov(ContextOperand(esi, variable->index()),
               Immediate(isolate()->factory()->the_hole_value()));
        // No write barrier since the hole value is in old space.
        PrepareForBailoutForId(proxy->id(), NO_REGISTERS);
      }
      break;

    case Variable::LOOKUP: {
      Comment cmnt(masm_, "[ Declaration");
      __ push(esi);
      __ push(Immediate(variable->name()));
      // Declaration nodes are always introduced in one of three modes.
      ASSERT(mode == Variable::VAR ||
             mode == Variable::CONST ||
             mode == Variable::LET);
      PropertyAttributes attr = (mode == Variable::CONST) ? READ_ONLY : NONE;
      __ push(Immediate(Smi::FromInt(attr)));
      // Push initial value, if any.
      // Note: For variables we must not push an initial value (such as
      // 'undefined') because we may have a (legal) redeclaration and we
      // must not destroy the current value.
      increment_stack_height(3);
      if (function != NULL) {
        VisitForStackValue(function);
      } else if (mode == Variable::CONST || mode == Variable::LET) {
        __ push(Immediate(isolate()->factory()->the_hole_value()));
        increment_stack_height();
      } else {
        __ push(Immediate(Smi::FromInt(0)));  // Indicates no initial value.
        increment_stack_height();
      }
      __ CallRuntime(Runtime::kDeclareContextSlot, 4);
      decrement_stack_height(4);
      break;
    }
  }
}


void FullCodeGenerator::VisitDeclaration(Declaration* decl) { }


void FullCodeGenerator::DeclareGlobals(Handle<FixedArray> pairs) {
  // Call the runtime to declare the globals.
  __ push(esi);  // The context is the first argument.
  __ push(Immediate(pairs));
  __ push(Immediate(Smi::FromInt(DeclareGlobalsFlags())));
  __ CallRuntime(Runtime::kDeclareGlobals, 3);
  // Return value is ignored.
}


void FullCodeGenerator::VisitSwitchStatement(SwitchStatement* stmt) {
  Comment cmnt(masm_, "[ SwitchStatement");
  Breakable nested_statement(this, stmt);
  SetStatementPosition(stmt);

  int switch_clause_stack_height = stack_height();
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
      __ or_(ecx, Operand(eax));
      patch_site.EmitJumpIfNotSmi(ecx, &slow_case, Label::kNear);

      __ cmp(edx, Operand(eax));
      __ j(not_equal, &next_test);
      __ Drop(1);  // Switch value is no longer needed.
      __ jmp(clause->body_target());
      __ bind(&slow_case);
    }

    // Record position before stub call for type feedback.
    SetSourcePosition(clause->position());
    Handle<Code> ic = CompareIC::GetUninitialized(Token::EQ_STRICT);
    __ call(ic, RelocInfo::CODE_TARGET, clause->CompareId());
    patch_site.EmitPatchInfo();
    __ test(eax, Operand(eax));
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

  set_stack_height(switch_clause_stack_height);
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
  SetStatementPosition(stmt);

  Label loop, exit;
  ForIn loop_statement(this, stmt);
  increment_loop_depth();

  // Get the object to enumerate over. Both SpiderMonkey and JSC
  // ignore null and undefined in contrast to the specification; see
  // ECMA-262 section 12.6.4.
  VisitForAccumulatorValue(stmt->enumerable());
  __ cmp(eax, isolate()->factory()->undefined_value());
  __ j(equal, &exit);
  __ cmp(eax, isolate()->factory()->null_value());
  __ j(equal, &exit);

  // Convert the object to a JS object.
  Label convert, done_convert;
  __ JumpIfSmi(eax, &convert, Label::kNear);
  __ CmpObjectType(eax, FIRST_SPEC_OBJECT_TYPE, ecx);
  __ j(above_equal, &done_convert, Label::kNear);
  __ bind(&convert);
  __ push(eax);
  __ InvokeBuiltin(Builtins::TO_OBJECT, CALL_FUNCTION);
  __ bind(&done_convert);
  __ push(eax);
  increment_stack_height();

  // Check cache validity in generated code. This is a fast case for
  // the JSObject::IsSimpleEnum cache validity checks. If we cannot
  // guarantee cache validity, call the runtime system to check cache
  // validity or get the property names in a fixed array.
  Label next, call_runtime;
  __ mov(ecx, eax);
  __ bind(&next);

  // Check that there are no elements.  Register ecx contains the
  // current JS object we've reached through the prototype chain.
  __ cmp(FieldOperand(ecx, JSObject::kElementsOffset),
         isolate()->factory()->empty_fixed_array());
  __ j(not_equal, &call_runtime);

  // Check that instance descriptors are not empty so that we can
  // check for an enum cache.  Leave the map in ebx for the subsequent
  // prototype load.
  __ mov(ebx, FieldOperand(ecx, HeapObject::kMapOffset));
  __ mov(edx, FieldOperand(ebx, Map::kInstanceDescriptorsOrBitField3Offset));
  __ JumpIfSmi(edx, &call_runtime);

  // Check that there is an enum cache in the non-empty instance
  // descriptors (edx).  This is the case if the next enumeration
  // index field does not contain a smi.
  __ mov(edx, FieldOperand(edx, DescriptorArray::kEnumerationIndexOffset));
  __ JumpIfSmi(edx, &call_runtime);

  // For all objects but the receiver, check that the cache is empty.
  Label check_prototype;
  __ cmp(ecx, Operand(eax));
  __ j(equal, &check_prototype, Label::kNear);
  __ mov(edx, FieldOperand(edx, DescriptorArray::kEnumCacheBridgeCacheOffset));
  __ cmp(edx, isolate()->factory()->empty_fixed_array());
  __ j(not_equal, &call_runtime);

  // Load the prototype from the map and loop if non-null.
  __ bind(&check_prototype);
  __ mov(ecx, FieldOperand(ebx, Map::kPrototypeOffset));
  __ cmp(ecx, isolate()->factory()->null_value());
  __ j(not_equal, &next);

  // The enum cache is valid.  Load the map of the object being
  // iterated over and use the cache for the iteration.
  Label use_cache;
  __ mov(eax, FieldOperand(eax, HeapObject::kMapOffset));
  __ jmp(&use_cache, Label::kNear);

  // Get the set of properties to enumerate.
  __ bind(&call_runtime);
  __ push(eax);  // Duplicate the enumerable object on the stack.
  __ CallRuntime(Runtime::kGetPropertyNamesFast, 1);

  // If we got a map from the runtime call, we can do a fast
  // modification check. Otherwise, we got a fixed array, and we have
  // to do a slow check.
  Label fixed_array;
  __ cmp(FieldOperand(eax, HeapObject::kMapOffset),
         isolate()->factory()->meta_map());
  __ j(not_equal, &fixed_array, Label::kNear);

  // We got a map in register eax. Get the enumeration cache from it.
  __ bind(&use_cache);
  __ LoadInstanceDescriptors(eax, ecx);
  __ mov(ecx, FieldOperand(ecx, DescriptorArray::kEnumerationIndexOffset));
  __ mov(edx, FieldOperand(ecx, DescriptorArray::kEnumCacheBridgeCacheOffset));

  // Setup the four remaining stack slots.
  __ push(eax);  // Map.
  __ push(edx);  // Enumeration cache.
  __ mov(eax, FieldOperand(edx, FixedArray::kLengthOffset));
  __ push(eax);  // Enumeration cache length (as smi).
  __ push(Immediate(Smi::FromInt(0)));  // Initial index.
  __ jmp(&loop);

  // We got a fixed array in register eax. Iterate through that.
  __ bind(&fixed_array);
  __ push(Immediate(Smi::FromInt(0)));  // Map (0) - force slow check.
  __ push(eax);
  __ mov(eax, FieldOperand(eax, FixedArray::kLengthOffset));
  __ push(eax);  // Fixed array length (as smi).
  __ push(Immediate(Smi::FromInt(0)));  // Initial index.

  // 1 ~ The object has already been pushed.
  increment_stack_height(ForIn::kElementCount - 1);
  // Generate code for doing the condition check.
  __ bind(&loop);
  __ mov(eax, Operand(esp, 0 * kPointerSize));  // Get the current index.
  __ cmp(eax, Operand(esp, 1 * kPointerSize));  // Compare to the array length.
  __ j(above_equal, loop_statement.break_label());

  // Get the current entry of the array into register ebx.
  __ mov(ebx, Operand(esp, 2 * kPointerSize));
  __ mov(ebx, FieldOperand(ebx, eax, times_2, FixedArray::kHeaderSize));

  // Get the expected map from the stack or a zero map in the
  // permanent slow case into register edx.
  __ mov(edx, Operand(esp, 3 * kPointerSize));

  // Check if the expected map still matches that of the enumerable.
  // If not, we have to filter the key.
  Label update_each;
  __ mov(ecx, Operand(esp, 4 * kPointerSize));
  __ cmp(edx, FieldOperand(ecx, HeapObject::kMapOffset));
  __ j(equal, &update_each, Label::kNear);

  // Convert the entry to a string or null if it isn't a property
  // anymore. If the property has been removed while iterating, we
  // just skip it.
  __ push(ecx);  // Enumerable.
  __ push(ebx);  // Current entry.
  __ InvokeBuiltin(Builtins::FILTER_KEY, CALL_FUNCTION);
  __ test(eax, Operand(eax));
  __ j(equal, loop_statement.continue_label());
  __ mov(ebx, Operand(eax));

  // Update the 'each' property or variable from the possibly filtered
  // entry in register ebx.
  __ bind(&update_each);
  __ mov(result_register(), ebx);
  // Perform the assignment as if via '='.
  { EffectContext context(this);
    EmitAssignment(stmt->each(), stmt->AssignmentId());
  }

  // Generate code for the body of the loop.
  Visit(stmt->body());

  // Generate code for going to the next element by incrementing the
  // index (smi) stored on top of the stack.
  __ bind(loop_statement.continue_label());
  __ add(Operand(esp, 0 * kPointerSize), Immediate(Smi::FromInt(1)));

  EmitStackCheck(stmt);
  __ jmp(&loop);

  // Remove the pointers stored on the stack.
  __ bind(loop_statement.break_label());
  __ add(Operand(esp), Immediate(5 * kPointerSize));

  decrement_stack_height(ForIn::kElementCount);
  // Exit and decrement the loop depth.
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
    FastNewClosureStub stub(info->strict_mode() ? kStrictMode : kNonStrictMode);
    __ push(Immediate(info));
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


void FullCodeGenerator::EmitLoadGlobalCheckExtensions(Variable* var,
                                                      TypeofState typeof_state,
                                                      Label* slow) {
  Register context = esi;
  Register temp = edx;

  Scope* s = scope();
  while (s != NULL) {
    if (s->num_heap_slots() > 0) {
      if (s->calls_eval()) {
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
    if (!s->outer_scope_calls_eval() || s->is_eval_scope()) break;
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
    // Terminate at global context.
    __ cmp(FieldOperand(temp, HeapObject::kMapOffset),
           Immediate(isolate()->factory()->global_context_map()));
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
  __ mov(eax, GlobalObjectOperand());
  __ mov(ecx, var->name());
  Handle<Code> ic = isolate()->builtins()->LoadIC_Initialize();
  RelocInfo::Mode mode = (typeof_state == INSIDE_TYPEOF)
      ? RelocInfo::CODE_TARGET
      : RelocInfo::CODE_TARGET_CONTEXT;
  __ call(ic, mode);
}


MemOperand FullCodeGenerator::ContextSlotOperandCheckExtensions(Variable* var,
                                                                Label* slow) {
  ASSERT(var->IsContextSlot());
  Register context = esi;
  Register temp = ebx;

  for (Scope* s = scope(); s != var->scope(); s = s->outer_scope()) {
    if (s->num_heap_slots() > 0) {
      if (s->calls_eval()) {
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


void FullCodeGenerator::EmitDynamicLookupFastCase(Variable* var,
                                                  TypeofState typeof_state,
                                                  Label* slow,
                                                  Label* done) {
  // Generate fast-case code for variables that might be shadowed by
  // eval-introduced variables.  Eval is used a lot without
  // introducing variables.  In those cases, we do not want to
  // perform a runtime call for all variables in the scope
  // containing the eval.
  if (var->mode() == Variable::DYNAMIC_GLOBAL) {
    EmitLoadGlobalCheckExtensions(var, typeof_state, slow);
    __ jmp(done);
  } else if (var->mode() == Variable::DYNAMIC_LOCAL) {
    Variable* local = var->local_if_not_shadowed();
    __ mov(eax, ContextSlotOperandCheckExtensions(local, slow));
    if (local->mode() == Variable::CONST) {
      __ cmp(eax, isolate()->factory()->the_hole_value());
      __ j(not_equal, done);
      __ mov(eax, isolate()->factory()->undefined_value());
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
      Comment cmnt(masm_, "Global variable");
      // Use inline caching. Variable name is passed in ecx and the global
      // object in eax.
      __ mov(eax, GlobalObjectOperand());
      __ mov(ecx, var->name());
      Handle<Code> ic = isolate()->builtins()->LoadIC_Initialize();
      __ call(ic, RelocInfo::CODE_TARGET_CONTEXT);
      context()->Plug(eax);
      break;
    }

    case Variable::PARAMETER:
    case Variable::LOCAL:
    case Variable::CONTEXT: {
      Comment cmnt(masm_, var->IsContextSlot()
                              ? "Context variable"
                              : "Stack variable");
      if (var->mode() != Variable::LET && var->mode() != Variable::CONST) {
        context()->Plug(var);
      } else {
        // Let and const need a read barrier.
        Label done;
        GetVar(eax, var);
        __ cmp(eax, isolate()->factory()->the_hole_value());
        __ j(not_equal, &done, Label::kNear);
        if (var->mode() == Variable::LET) {
          __ push(Immediate(var->name()));
          __ CallRuntime(Runtime::kThrowReferenceError, 1);
        } else {  // Variable::CONST
          __ mov(eax, isolate()->factory()->undefined_value());
        }
        __ bind(&done);
        context()->Plug(eax);
      }
      break;
    }

    case Variable::LOOKUP: {
      Label done, slow;
      // Generate code for loading from variables potentially shadowed
      // by eval-introduced variables.
      EmitDynamicLookupFastCase(var, NOT_INSIDE_TYPEOF, &slow, &done);
      __ bind(&slow);
      Comment cmnt(masm_, "Lookup variable");
      __ push(esi);  // Context.
      __ push(Immediate(var->name()));
      __ CallRuntime(Runtime::kLoadContextSlot, 2);
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
  __ AllocateInNewSpace(size, eax, ecx, edx, &runtime_allocate, TAG_OBJECT);
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


void FullCodeGenerator::VisitObjectLiteral(ObjectLiteral* expr) {
  Comment cmnt(masm_, "[ ObjectLiteral");
  __ mov(edi, Operand(ebp, JavaScriptFrameConstants::kFunctionOffset));
  __ push(FieldOperand(edi, JSFunction::kLiteralsOffset));
  __ push(Immediate(Smi::FromInt(expr->literal_index())));
  __ push(Immediate(expr->constant_properties()));
  int flags = expr->fast_elements()
      ? ObjectLiteral::kFastElements
      : ObjectLiteral::kNoFlags;
  flags |= expr->has_function()
      ? ObjectLiteral::kHasFunction
      : ObjectLiteral::kNoFlags;
  __ push(Immediate(Smi::FromInt(flags)));
  if (expr->depth() > 1) {
    __ CallRuntime(Runtime::kCreateObjectLiteral, 4);
  } else {
    __ CallRuntime(Runtime::kCreateObjectLiteralShallow, 4);
  }

  // If result_saved is true the result is on top of the stack.  If
  // result_saved is false the result is in eax.
  bool result_saved = false;

  // Mark all computed expressions that are bound to a key that
  // is shadowed by a later occurrence of the same key. For the
  // marked expressions, no store code is emitted.
  expr->CalculateEmitStore();

  for (int i = 0; i < expr->properties()->length(); i++) {
    ObjectLiteral::Property* property = expr->properties()->at(i);
    if (property->IsCompileTimeValue()) continue;

    Literal* key = property->key();
    Expression* value = property->value();
    if (!result_saved) {
      __ push(eax);  // Save result on the stack
      result_saved = true;
      increment_stack_height();
    }
    switch (property->kind()) {
      case ObjectLiteral::Property::MATERIALIZED_LITERAL:
        ASSERT(!CompileTimeValue::IsCompileTimeValue(value));
        // Fall through.
      case ObjectLiteral::Property::COMPUTED:
        if (key->handle()->IsSymbol()) {
          if (property->emit_store()) {
            VisitForAccumulatorValue(value);
            __ mov(ecx, Immediate(key->handle()));
            __ mov(edx, Operand(esp, 0));
            Handle<Code> ic = is_strict_mode()
                ? isolate()->builtins()->StoreIC_Initialize_Strict()
                : isolate()->builtins()->StoreIC_Initialize();
            __ call(ic, RelocInfo::CODE_TARGET, key->id());
            PrepareForBailoutForId(key->id(), NO_REGISTERS);
          } else {
            VisitForEffect(value);
          }
          break;
        }
        // Fall through.
      case ObjectLiteral::Property::PROTOTYPE:
        __ push(Operand(esp, 0));  // Duplicate receiver.
        increment_stack_height();
        VisitForStackValue(key);
        VisitForStackValue(value);
        if (property->emit_store()) {
          __ push(Immediate(Smi::FromInt(NONE)));  // PropertyAttributes
          __ CallRuntime(Runtime::kSetProperty, 4);
        } else {
          __ Drop(3);
        }
        decrement_stack_height(3);
        break;
      case ObjectLiteral::Property::SETTER:
      case ObjectLiteral::Property::GETTER:
        __ push(Operand(esp, 0));  // Duplicate receiver.
        increment_stack_height();
        VisitForStackValue(key);
        __ push(Immediate(property->kind() == ObjectLiteral::Property::SETTER ?
                          Smi::FromInt(1) :
                          Smi::FromInt(0)));
        increment_stack_height();
        VisitForStackValue(value);
        __ CallRuntime(Runtime::kDefineAccessor, 4);
        decrement_stack_height(4);
        break;
      default: UNREACHABLE();
    }
  }

  if (expr->has_function()) {
    ASSERT(result_saved);
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

  ZoneList<Expression*>* subexprs = expr->values();
  int length = subexprs->length();

  __ mov(ebx, Operand(ebp, JavaScriptFrameConstants::kFunctionOffset));
  __ push(FieldOperand(ebx, JSFunction::kLiteralsOffset));
  __ push(Immediate(Smi::FromInt(expr->literal_index())));
  __ push(Immediate(expr->constant_elements()));
  if (expr->constant_elements()->map() ==
      isolate()->heap()->fixed_cow_array_map()) {
    ASSERT(expr->depth() == 1);
    FastCloneShallowArrayStub stub(
        FastCloneShallowArrayStub::COPY_ON_WRITE_ELEMENTS, length);
    __ CallStub(&stub);
    __ IncrementCounter(isolate()->counters()->cow_arrays_created_stub(), 1);
  } else if (expr->depth() > 1) {
    __ CallRuntime(Runtime::kCreateArrayLiteral, 3);
  } else if (length > FastCloneShallowArrayStub::kMaximumClonedLength) {
    __ CallRuntime(Runtime::kCreateArrayLiteralShallow, 3);
  } else {
    FastCloneShallowArrayStub stub(
        FastCloneShallowArrayStub::CLONE_ELEMENTS, length);
    __ CallStub(&stub);
  }

  bool result_saved = false;  // Is the result saved to the stack?

  // Emit code to evaluate all the non-constant subexpressions and to store
  // them into the newly cloned array.
  for (int i = 0; i < length; i++) {
    Expression* subexpr = subexprs->at(i);
    // If the subexpression is a literal or a simple materialized literal it
    // is already set in the cloned array.
    if (subexpr->AsLiteral() != NULL ||
        CompileTimeValue::IsCompileTimeValue(subexpr)) {
      continue;
    }

    if (!result_saved) {
      __ push(eax);
      result_saved = true;
      increment_stack_height();
    }
    VisitForAccumulatorValue(subexpr);

    // Store the subexpression value in the array's elements.
    __ mov(ebx, Operand(esp, 0));  // Copy of array literal.
    __ mov(ebx, FieldOperand(ebx, JSObject::kElementsOffset));
    int offset = FixedArray::kHeaderSize + (i * kPointerSize);
    __ mov(FieldOperand(ebx, offset), result_register());

    // Update the write barrier for the array store.
    __ RecordWrite(ebx, offset, result_register(), ecx);

    PrepareForBailoutForId(expr->GetIdForElement(i), NO_REGISTERS);
  }

  if (result_saved) {
    context()->PlugTOS();
  } else {
    context()->Plug(eax);
  }
}


void FullCodeGenerator::VisitAssignment(Assignment* expr) {
  Comment cmnt(masm_, "[ Assignment");
  // Invalid left-hand sides are rewritten to have a 'throw ReferenceError'
  // on the left-hand side.
  if (!expr->target()->IsValidLeftHandSide()) {
    ASSERT(expr->target()->AsThrow() != NULL);
    VisitInCurrentContext(expr->target());  // Throw does not plug the context
    context()->Plug(eax);
    return;
  }

  // Left-hand side can only be a property, a global or a (parameter or local)
  // slot.
  enum LhsKind { VARIABLE, NAMED_PROPERTY, KEYED_PROPERTY };
  LhsKind assign_type = VARIABLE;
  Property* property = expr->target()->AsProperty();
  if (property != NULL) {
    assign_type = (property->key()->IsPropertyName())
        ? NAMED_PROPERTY
        : KEYED_PROPERTY;
  }

  // Evaluate LHS expression.
  switch (assign_type) {
    case VARIABLE:
      // Nothing to do here.
      break;
    case NAMED_PROPERTY:
      if (expr->is_compound()) {
        // We need the receiver both on the stack and in the accumulator.
        VisitForAccumulatorValue(property->obj());
        __ push(result_register());
        increment_stack_height();
      } else {
        VisitForStackValue(property->obj());
      }
      break;
    case KEYED_PROPERTY: {
      if (expr->is_compound()) {
        VisitForStackValue(property->obj());
        VisitForAccumulatorValue(property->key());
        __ mov(edx, Operand(esp, 0));
        __ push(eax);
        increment_stack_height();
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
        case NAMED_PROPERTY:
          EmitNamedPropertyLoad(property);
          PrepareForBailoutForId(expr->CompoundLoadId(), TOS_REG);
          break;
        case KEYED_PROPERTY:
          EmitKeyedPropertyLoad(property);
          PrepareForBailoutForId(expr->CompoundLoadId(), TOS_REG);
          break;
      }
    }

    Token::Value op = expr->binary_op();
    __ push(eax);  // Left operand goes on the stack.
    increment_stack_height();
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
    case KEYED_PROPERTY:
      EmitKeyedPropertyAssignment(expr);
      break;
  }
}


void FullCodeGenerator::EmitNamedPropertyLoad(Property* prop) {
  SetSourcePosition(prop->position());
  Literal* key = prop->key()->AsLiteral();
  ASSERT(!key->handle()->IsSmi());
  __ mov(ecx, Immediate(key->handle()));
  Handle<Code> ic = isolate()->builtins()->LoadIC_Initialize();
  __ call(ic, RelocInfo::CODE_TARGET, GetPropertyId(prop));
}


void FullCodeGenerator::EmitKeyedPropertyLoad(Property* prop) {
  SetSourcePosition(prop->position());
  Handle<Code> ic = isolate()->builtins()->KeyedLoadIC_Initialize();
  __ call(ic, RelocInfo::CODE_TARGET, GetPropertyId(prop));
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
  decrement_stack_height();
  __ mov(ecx, eax);
  __ or_(eax, Operand(edx));
  JumpPatchSite patch_site(masm_);
  patch_site.EmitJumpIfSmi(eax, &smi_case, Label::kNear);

  __ bind(&stub_call);
  __ mov(eax, ecx);
  BinaryOpStub stub(op, mode);
  __ call(stub.GetCode(), RelocInfo::CODE_TARGET, expr->id());
  patch_site.EmitPatchInfo();
  __ jmp(&done, Label::kNear);

  // Smi case.
  __ bind(&smi_case);
  __ mov(eax, edx);  // Copy left operand in case of a stub call.

  switch (op) {
    case Token::SAR:
      __ SmiUntag(eax);
      __ SmiUntag(ecx);
      __ sar_cl(eax);  // No checks of result necessary
      __ SmiTag(eax);
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
      __ add(eax, Operand(ecx));
      __ j(overflow, &stub_call);
      break;
    case Token::SUB:
      __ sub(eax, Operand(ecx));
      __ j(overflow, &stub_call);
      break;
    case Token::MUL: {
      __ SmiUntag(eax);
      __ imul(eax, Operand(ecx));
      __ j(overflow, &stub_call);
      __ test(eax, Operand(eax));
      __ j(not_zero, &done, Label::kNear);
      __ mov(ebx, edx);
      __ or_(ebx, Operand(ecx));
      __ j(negative, &stub_call);
      break;
    }
    case Token::BIT_OR:
      __ or_(eax, Operand(ecx));
      break;
    case Token::BIT_AND:
      __ and_(eax, Operand(ecx));
      break;
    case Token::BIT_XOR:
      __ xor_(eax, Operand(ecx));
      break;
    default:
      UNREACHABLE();
  }

  __ bind(&done);
  context()->Plug(eax);
}


void FullCodeGenerator::EmitBinaryOp(BinaryOperation* expr,
                                     Token::Value op,
                                     OverwriteMode mode) {
  __ pop(edx);
  decrement_stack_height();
  BinaryOpStub stub(op, mode);
  JumpPatchSite patch_site(masm_);    // unbound, signals no inlined smi code.
  __ call(stub.GetCode(), RelocInfo::CODE_TARGET, expr->id());
  patch_site.EmitPatchInfo();
  context()->Plug(eax);
}


void FullCodeGenerator::EmitAssignment(Expression* expr, int bailout_ast_id) {
  // Invalid left-hand sides are rewritten to have a 'throw
  // ReferenceError' on the left-hand side.
  if (!expr->IsValidLeftHandSide()) {
    ASSERT(expr->AsThrow() != NULL);
    VisitInCurrentContext(expr);  // Throw does not plug the context
    context()->Plug(eax);
    return;
  }

  // Left-hand side can only be a property, a global or a (parameter or local)
  // slot.
  enum LhsKind { VARIABLE, NAMED_PROPERTY, KEYED_PROPERTY };
  LhsKind assign_type = VARIABLE;
  Property* prop = expr->AsProperty();
  if (prop != NULL) {
    assign_type = (prop->key()->IsPropertyName())
        ? NAMED_PROPERTY
        : KEYED_PROPERTY;
  }

  switch (assign_type) {
    case VARIABLE: {
      Variable* var = expr->AsVariableProxy()->var();
      EffectContext context(this);
      EmitVariableAssignment(var, Token::ASSIGN);
      break;
    }
    case NAMED_PROPERTY: {
      __ push(eax);  // Preserve value.
      increment_stack_height();
      VisitForAccumulatorValue(prop->obj());
      __ mov(edx, eax);
      __ pop(eax);  // Restore value.
      decrement_stack_height();
      __ mov(ecx, prop->key()->AsLiteral()->handle());
      Handle<Code> ic = is_strict_mode()
          ? isolate()->builtins()->StoreIC_Initialize_Strict()
          : isolate()->builtins()->StoreIC_Initialize();
      __ call(ic);
      break;
    }
    case KEYED_PROPERTY: {
      __ push(eax);  // Preserve value.
      increment_stack_height();
      VisitForStackValue(prop->obj());
      VisitForAccumulatorValue(prop->key());
      __ mov(ecx, eax);
      __ pop(edx);
      decrement_stack_height();
      __ pop(eax);  // Restore value.
      decrement_stack_height();
      Handle<Code> ic = is_strict_mode()
          ? isolate()->builtins()->KeyedStoreIC_Initialize_Strict()
          : isolate()->builtins()->KeyedStoreIC_Initialize();
      __ call(ic);
      break;
    }
  }
  PrepareForBailoutForId(bailout_ast_id, TOS_REG);
  context()->Plug(eax);
}


void FullCodeGenerator::EmitVariableAssignment(Variable* var,
                                               Token::Value op) {
  if (var->IsUnallocated()) {
    // Global var, const, or let.
    __ mov(ecx, var->name());
    __ mov(edx, GlobalObjectOperand());
    Handle<Code> ic = is_strict_mode()
        ? isolate()->builtins()->StoreIC_Initialize_Strict()
        : isolate()->builtins()->StoreIC_Initialize();
    __ call(ic, RelocInfo::CODE_TARGET_CONTEXT);

  } else if (op == Token::INIT_CONST) {
    // Const initializers need a write barrier.
    ASSERT(!var->IsParameter());  // No const parameters.
    if (var->IsStackLocal()) {
      Label skip;
      __ mov(edx, StackOperand(var));
      __ cmp(edx, isolate()->factory()->the_hole_value());
      __ j(not_equal, &skip);
      __ mov(StackOperand(var), eax);
      __ bind(&skip);
    } else {
      ASSERT(var->IsContextSlot() || var->IsLookupSlot());
      // Like var declarations, const declarations are hoisted to function
      // scope.  However, unlike var initializers, const initializers are
      // able to drill a hole to that function context, even from inside a
      // 'with' context.  We thus bypass the normal static scope lookup for
      // var->IsContextSlot().
      __ push(eax);
      __ push(esi);
      __ push(Immediate(var->name()));
      __ CallRuntime(Runtime::kInitializeConstContextSlot, 3);
    }

  } else if (var->mode() == Variable::LET && op != Token::INIT_LET) {
    // Non-initializing assignment to let variable needs a write barrier.
    if (var->IsLookupSlot()) {
      __ push(eax);  // Value.
      __ push(esi);  // Context.
      __ push(Immediate(var->name()));
      __ push(Immediate(Smi::FromInt(strict_mode_flag())));
      __ CallRuntime(Runtime::kStoreContextSlot, 4);
    } else {
      ASSERT(var->IsStackAllocated() || var->IsContextSlot());
      Label assign;
      MemOperand location = VarOperand(var, ecx);
      __ mov(edx, location);
      __ cmp(edx, isolate()->factory()->the_hole_value());
      __ j(not_equal, &assign, Label::kNear);
      __ push(Immediate(var->name()));
      __ CallRuntime(Runtime::kThrowReferenceError, 1);
      __ bind(&assign);
      __ mov(location, eax);
      if (var->IsContextSlot()) {
        __ mov(edx, eax);
        __ RecordWrite(ecx, Context::SlotOffset(var->index()), edx, ebx);
      }
    }

  } else if (var->mode() != Variable::CONST) {
    // Assignment to var or initializing assignment to let.
    if (var->IsStackAllocated() || var->IsContextSlot()) {
      MemOperand location = VarOperand(var, ecx);
      if (FLAG_debug_code && op == Token::INIT_LET) {
        // Check for an uninitialized let binding.
        __ mov(edx, location);
        __ cmp(edx, isolate()->factory()->the_hole_value());
        __ Check(equal, "Let binding re-initialization.");
      }
      // Perform the assignment.
      __ mov(location, eax);
      if (var->IsContextSlot()) {
        __ mov(edx, eax);
        __ RecordWrite(ecx, Context::SlotOffset(var->index()), edx, ebx);
      }
    } else {
      ASSERT(var->IsLookupSlot());
      __ push(eax);  // Value.
      __ push(esi);  // Context.
      __ push(Immediate(var->name()));
      __ push(Immediate(Smi::FromInt(strict_mode_flag())));
      __ CallRuntime(Runtime::kStoreContextSlot, 4);
    }
  }
  // Non-initializing assignments to consts are ignored.
}


void FullCodeGenerator::EmitNamedPropertyAssignment(Assignment* expr) {
  // Assignment to a property, using a named store IC.
  Property* prop = expr->target()->AsProperty();
  ASSERT(prop != NULL);
  ASSERT(prop->key()->AsLiteral() != NULL);

  // If the assignment starts a block of assignments to the same object,
  // change to slow case to avoid the quadratic behavior of repeatedly
  // adding fast properties.
  if (expr->starts_initialization_block()) {
    __ push(result_register());
    __ push(Operand(esp, kPointerSize));  // Receiver is now under value.
    __ CallRuntime(Runtime::kToSlowProperties, 1);
    __ pop(result_register());
  }

  // Record source code position before IC call.
  SetSourcePosition(expr->position());
  __ mov(ecx, prop->key()->AsLiteral()->handle());
  if (expr->ends_initialization_block()) {
    __ mov(edx, Operand(esp, 0));
  } else {
    __ pop(edx);
    decrement_stack_height();
  }
  Handle<Code> ic = is_strict_mode()
      ? isolate()->builtins()->StoreIC_Initialize_Strict()
      : isolate()->builtins()->StoreIC_Initialize();
  __ call(ic, RelocInfo::CODE_TARGET, expr->id());

  // If the assignment ends an initialization block, revert to fast case.
  if (expr->ends_initialization_block()) {
    __ push(eax);  // Result of assignment, saved even if not needed.
    __ push(Operand(esp, kPointerSize));  // Receiver is under value.
    __ CallRuntime(Runtime::kToFastProperties, 1);
    __ pop(eax);
    __ Drop(1);
    decrement_stack_height();
  }
  PrepareForBailoutForId(expr->AssignmentId(), TOS_REG);
  context()->Plug(eax);
}


void FullCodeGenerator::EmitKeyedPropertyAssignment(Assignment* expr) {
  // Assignment to a property, using a keyed store IC.

  // If the assignment starts a block of assignments to the same object,
  // change to slow case to avoid the quadratic behavior of repeatedly
  // adding fast properties.
  if (expr->starts_initialization_block()) {
    __ push(result_register());
    // Receiver is now under the key and value.
    __ push(Operand(esp, 2 * kPointerSize));
    __ CallRuntime(Runtime::kToSlowProperties, 1);
    __ pop(result_register());
  }

  __ pop(ecx);
  decrement_stack_height();
  if (expr->ends_initialization_block()) {
    __ mov(edx, Operand(esp, 0));  // Leave receiver on the stack for later.
  } else {
    __ pop(edx);
    decrement_stack_height();
  }
  // Record source code position before IC call.
  SetSourcePosition(expr->position());
  Handle<Code> ic = is_strict_mode()
      ? isolate()->builtins()->KeyedStoreIC_Initialize_Strict()
      : isolate()->builtins()->KeyedStoreIC_Initialize();
  __ call(ic, RelocInfo::CODE_TARGET, expr->id());

  // If the assignment ends an initialization block, revert to fast case.
  if (expr->ends_initialization_block()) {
    __ pop(edx);
    __ push(eax);  // Result of assignment, saved even if not needed.
    __ push(edx);
    __ CallRuntime(Runtime::kToFastProperties, 1);
    __ pop(eax);
    decrement_stack_height();
  }

  PrepareForBailoutForId(expr->AssignmentId(), TOS_REG);
  context()->Plug(eax);
}


void FullCodeGenerator::VisitProperty(Property* expr) {
  Comment cmnt(masm_, "[ Property");
  Expression* key = expr->key();

  if (key->IsPropertyName()) {
    VisitForAccumulatorValue(expr->obj());
    EmitNamedPropertyLoad(expr);
    context()->Plug(eax);
  } else {
    VisitForStackValue(expr->obj());
    VisitForAccumulatorValue(expr->key());
    __ pop(edx);
    decrement_stack_height();
    EmitKeyedPropertyLoad(expr);
    context()->Plug(eax);
  }
}


void FullCodeGenerator::EmitCallWithIC(Call* expr,
                                       Handle<Object> name,
                                       RelocInfo::Mode mode) {
  // Code common for calls using the IC.
  ZoneList<Expression*>* args = expr->arguments();
  int arg_count = args->length();
  { PreservePositionScope scope(masm()->positions_recorder());
    for (int i = 0; i < arg_count; i++) {
      VisitForStackValue(args->at(i));
    }
    __ Set(ecx, Immediate(name));
  }
  // Record source position of the IC call.
  SetSourcePosition(expr->position());
  Handle<Code> ic =
      isolate()->stub_cache()->ComputeCallInitialize(arg_count, mode);
  __ call(ic, mode, expr->id());
  RecordJSReturnSite(expr);
  // Restore context register.
  __ mov(esi, Operand(ebp, StandardFrameConstants::kContextOffset));
  decrement_stack_height(arg_count + 1);
  context()->Plug(eax);
}


void FullCodeGenerator::EmitKeyedCallWithIC(Call* expr,
                                            Expression* key) {
  // Load the key.
  VisitForAccumulatorValue(key);

  // Swap the name of the function and the receiver on the stack to follow
  // the calling convention for call ICs.
  __ pop(ecx);
  __ push(eax);
  __ push(ecx);
  increment_stack_height();

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
  Handle<Code> ic =
      isolate()->stub_cache()->ComputeKeyedCallInitialize(arg_count);
  __ mov(ecx, Operand(esp, (arg_count + 1) * kPointerSize));  // Key.
  __ call(ic, RelocInfo::CODE_TARGET, expr->id());
  RecordJSReturnSite(expr);
  // Restore context register.
  __ mov(esi, Operand(ebp, StandardFrameConstants::kContextOffset));
  decrement_stack_height(arg_count + 1);
  context()->DropAndPlug(1, eax);  // Drop the key still on the stack.
}


void FullCodeGenerator::EmitCallWithStub(Call* expr, CallFunctionFlags flags) {
  // Code common for calls using the call stub.
  ZoneList<Expression*>* args = expr->arguments();
  int arg_count = args->length();
  { PreservePositionScope scope(masm()->positions_recorder());
    for (int i = 0; i < arg_count; i++) {
      VisitForStackValue(args->at(i));
    }
  }
  // Record source position for debugger.
  SetSourcePosition(expr->position());
  CallFunctionStub stub(arg_count, flags);
  __ CallStub(&stub);
  RecordJSReturnSite(expr);
  // Restore context register.
  __ mov(esi, Operand(ebp, StandardFrameConstants::kContextOffset));

  decrement_stack_height(arg_count + 1);
  context()->DropAndPlug(1, eax);
}


void FullCodeGenerator::EmitResolvePossiblyDirectEval(ResolveEvalFlag flag,
                                                      int arg_count) {
  // Push copy of the first argument or undefined if it doesn't exist.
  if (arg_count > 0) {
    __ push(Operand(esp, arg_count * kPointerSize));
  } else {
    __ push(Immediate(isolate()->factory()->undefined_value()));
  }

  // Push the receiver of the enclosing function.
  __ push(Operand(ebp, (2 + info_->scope()->num_parameters()) * kPointerSize));

  // Push the strict mode flag. In harmony mode every eval call
  // is a strict mode eval call.
  StrictModeFlag strict_mode = strict_mode_flag();
  if (FLAG_harmony_block_scoping) {
    strict_mode = kStrictMode;
  }
  __ push(Immediate(Smi::FromInt(strict_mode)));

  __ CallRuntime(flag == SKIP_CONTEXT_LOOKUP
                 ? Runtime::kResolvePossiblyDirectEvalNoLookup
                 : Runtime::kResolvePossiblyDirectEval, 4);
}


void FullCodeGenerator::VisitCall(Call* expr) {
#ifdef DEBUG
  // We want to verify that RecordJSReturnSite gets called on all paths
  // through this function.  Avoid early returns.
  expr->return_is_recorded_ = false;
#endif

  Comment cmnt(masm_, "[ Call");
  Expression* callee = expr->expression();
  VariableProxy* proxy = callee->AsVariableProxy();
  Property* property = callee->AsProperty();

  if (proxy != NULL && proxy->var()->is_possibly_eval()) {
    // In a call to eval, we first call %ResolvePossiblyDirectEval to
    // resolve the function we need to call and the receiver of the call.
    // Then we call the resolved function using the given arguments.
    ZoneList<Expression*>* args = expr->arguments();
    int arg_count = args->length();
    { PreservePositionScope pos_scope(masm()->positions_recorder());
      VisitForStackValue(callee);
      // Reserved receiver slot.
      __ push(Immediate(isolate()->factory()->undefined_value()));
      increment_stack_height();
      // Push the arguments.
      for (int i = 0; i < arg_count; i++) {
        VisitForStackValue(args->at(i));
      }

      // If we know that eval can only be shadowed by eval-introduced
      // variables we attempt to load the global eval function directly in
      // generated code. If we succeed, there is no need to perform a
      // context lookup in the runtime system.
      Label done;
      Variable* var = proxy->var();
      if (!var->IsUnallocated() && var->mode() == Variable::DYNAMIC_GLOBAL) {
        Label slow;
        EmitLoadGlobalCheckExtensions(var, NOT_INSIDE_TYPEOF, &slow);
        // Push the function and resolve eval.
        __ push(eax);
        EmitResolvePossiblyDirectEval(SKIP_CONTEXT_LOOKUP, arg_count);
        __ jmp(&done);
        __ bind(&slow);
      }

      // Push a copy of the function (found below the arguments) and
      // resolve eval.
      __ push(Operand(esp, (arg_count + 1) * kPointerSize));
      EmitResolvePossiblyDirectEval(PERFORM_CONTEXT_LOOKUP, arg_count);
      __ bind(&done);

      // The runtime call returns a pair of values in eax (function) and
      // edx (receiver). Touch up the stack with the right values.
      __ mov(Operand(esp, (arg_count + 0) * kPointerSize), edx);
      __ mov(Operand(esp, (arg_count + 1) * kPointerSize), eax);
    }
    // Record source position for debugger.
    SetSourcePosition(expr->position());
    CallFunctionStub stub(arg_count, RECEIVER_MIGHT_BE_IMPLICIT);
    __ CallStub(&stub);
    RecordJSReturnSite(expr);
    // Restore context register.
    __ mov(esi, Operand(ebp, StandardFrameConstants::kContextOffset));
    decrement_stack_height(arg_count + 1);  // Function is left on the stack.
    context()->DropAndPlug(1, eax);

  } else if (proxy != NULL && proxy->var()->IsUnallocated()) {
    // Push global object as receiver for the call IC.
    __ push(GlobalObjectOperand());
    increment_stack_height();
    EmitCallWithIC(expr, proxy->name(), RelocInfo::CODE_TARGET_CONTEXT);

  } else if (proxy != NULL && proxy->var()->IsLookupSlot()) {
    // Call to a lookup slot (dynamically introduced variable).
    Label slow, done;
    { PreservePositionScope scope(masm()->positions_recorder());
      // Generate code for loading from variables potentially shadowed by
      // eval-introduced variables.
      EmitDynamicLookupFastCase(proxy->var(), NOT_INSIDE_TYPEOF, &slow, &done);
    }
    __ bind(&slow);
    // Call the runtime to find the function to call (returned in eax) and
    // the object holding it (returned in edx).
    __ push(context_register());
    __ push(Immediate(proxy->name()));
    __ CallRuntime(Runtime::kLoadContextSlot, 2);
    __ push(eax);  // Function.
    __ push(edx);  // Receiver.
    increment_stack_height(2);

    // If fast case code has been generated, emit code to push the function
    // and receiver and have the slow path jump around this code.
    if (done.is_linked()) {
      Label call;
      __ jmp(&call, Label::kNear);
      __ bind(&done);
      // Push function.  Stack height already incremented in slow case
      // above.
      __ push(eax);
      // The receiver is implicitly the global receiver. Indicate this by
      // passing the hole to the call function stub.
      __ push(Immediate(isolate()->factory()->the_hole_value()));
      __ bind(&call);
    }

    // The receiver is either the global receiver or an object found by
    // LoadContextSlot. That object could be the hole if the receiver is
    // implicitly the global object.
    EmitCallWithStub(expr, RECEIVER_MIGHT_BE_IMPLICIT);

  } else if (property != NULL) {
    { PreservePositionScope scope(masm()->positions_recorder());
      VisitForStackValue(property->obj());
    }
    if (property->key()->IsPropertyName()) {
      EmitCallWithIC(expr,
                     property->key()->AsLiteral()->handle(),
                     RelocInfo::CODE_TARGET);
    } else {
      EmitKeyedCallWithIC(expr, property->key());
    }

  } else {
    // Call to an arbitrary expression not handled specially above.
    { PreservePositionScope scope(masm()->positions_recorder());
      VisitForStackValue(callee);
    }
    // Load global receiver object.
    __ mov(ebx, GlobalObjectOperand());
    __ push(FieldOperand(ebx, GlobalObject::kGlobalReceiverOffset));
    increment_stack_height();
    // Emit function call.
    EmitCallWithStub(expr, NO_CALL_FUNCTION_FLAGS);
  }

#ifdef DEBUG
  // RecordJSReturnSite should have been called.
  ASSERT(expr->return_is_recorded_);
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
  VisitForStackValue(expr->expression());

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
  __ SafeSet(eax, Immediate(arg_count));
  __ mov(edi, Operand(esp, arg_count * kPointerSize));

  Handle<Code> construct_builtin =
      isolate()->builtins()->JSConstructCall();
  __ call(construct_builtin, RelocInfo::CONSTRUCT_CALL);

  decrement_stack_height(arg_count + 1);
  context()->Plug(eax);
}


void FullCodeGenerator::EmitIsSmi(ZoneList<Expression*>* args) {
  ASSERT(args->length() == 1);

  VisitForAccumulatorValue(args->at(0));

  Label materialize_true, materialize_false;
  Label* if_true = NULL;
  Label* if_false = NULL;
  Label* fall_through = NULL;
  context()->PrepareTest(&materialize_true, &materialize_false,
                         &if_true, &if_false, &fall_through);

  PrepareForBailoutBeforeSplit(TOS_REG, true, if_true, if_false);
  __ test(eax, Immediate(kSmiTagMask));
  Split(zero, if_true, if_false, fall_through);

  context()->Plug(if_true, if_false);
}


void FullCodeGenerator::EmitIsNonNegativeSmi(ZoneList<Expression*>* args) {
  ASSERT(args->length() == 1);

  VisitForAccumulatorValue(args->at(0));

  Label materialize_true, materialize_false;
  Label* if_true = NULL;
  Label* if_false = NULL;
  Label* fall_through = NULL;
  context()->PrepareTest(&materialize_true, &materialize_false,
                         &if_true, &if_false, &fall_through);

  PrepareForBailoutBeforeSplit(TOS_REG, true, if_true, if_false);
  __ test(eax, Immediate(kSmiTagMask | 0x80000000));
  Split(zero, if_true, if_false, fall_through);

  context()->Plug(if_true, if_false);
}


void FullCodeGenerator::EmitIsObject(ZoneList<Expression*>* args) {
  ASSERT(args->length() == 1);

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
  PrepareForBailoutBeforeSplit(TOS_REG, true, if_true, if_false);
  Split(below_equal, if_true, if_false, fall_through);

  context()->Plug(if_true, if_false);
}


void FullCodeGenerator::EmitIsSpecObject(ZoneList<Expression*>* args) {
  ASSERT(args->length() == 1);

  VisitForAccumulatorValue(args->at(0));

  Label materialize_true, materialize_false;
  Label* if_true = NULL;
  Label* if_false = NULL;
  Label* fall_through = NULL;
  context()->PrepareTest(&materialize_true, &materialize_false,
                         &if_true, &if_false, &fall_through);

  __ JumpIfSmi(eax, if_false);
  __ CmpObjectType(eax, FIRST_SPEC_OBJECT_TYPE, ebx);
  PrepareForBailoutBeforeSplit(TOS_REG, true, if_true, if_false);
  Split(above_equal, if_true, if_false, fall_through);

  context()->Plug(if_true, if_false);
}


void FullCodeGenerator::EmitIsUndetectableObject(ZoneList<Expression*>* args) {
  ASSERT(args->length() == 1);

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
  PrepareForBailoutBeforeSplit(TOS_REG, true, if_true, if_false);
  Split(not_zero, if_true, if_false, fall_through);

  context()->Plug(if_true, if_false);
}


void FullCodeGenerator::EmitIsStringWrapperSafeForDefaultValueOf(
    ZoneList<Expression*>* args) {
  ASSERT(args->length() == 1);

  VisitForAccumulatorValue(args->at(0));

  Label materialize_true, materialize_false;
  Label* if_true = NULL;
  Label* if_false = NULL;
  Label* fall_through = NULL;
  context()->PrepareTest(&materialize_true, &materialize_false,
                         &if_true, &if_false, &fall_through);

  if (FLAG_debug_code) __ AbortIfSmi(eax);

  // Check whether this map has already been checked to be safe for default
  // valueOf.
  __ mov(ebx, FieldOperand(eax, HeapObject::kMapOffset));
  __ test_b(FieldOperand(ebx, Map::kBitField2Offset),
            1 << Map::kStringWrapperSafeForDefaultValueOf);
  __ j(not_zero, if_true);

  // Check for fast case object. Return false for slow case objects.
  __ mov(ecx, FieldOperand(eax, JSObject::kPropertiesOffset));
  __ mov(ecx, FieldOperand(ecx, HeapObject::kMapOffset));
  __ cmp(ecx, FACTORY->hash_table_map());
  __ j(equal, if_false);

  // Look for valueOf symbol in the descriptor array, and indicate false if
  // found. The type is not checked, so if it is a transition it is a false
  // negative.
  __ LoadInstanceDescriptors(ebx, ebx);
  __ mov(ecx, FieldOperand(ebx, FixedArray::kLengthOffset));
  // ebx: descriptor array
  // ecx: length of descriptor array
  // Calculate the end of the descriptor array.
  STATIC_ASSERT(kSmiTag == 0);
  STATIC_ASSERT(kSmiTagSize == 1);
  STATIC_ASSERT(kPointerSize == 4);
  __ lea(ecx, Operand(ebx, ecx, times_2, FixedArray::kHeaderSize));
  // Calculate location of the first key name.
  __ add(Operand(ebx),
           Immediate(FixedArray::kHeaderSize +
                     DescriptorArray::kFirstIndex * kPointerSize));
  // Loop through all the keys in the descriptor array. If one of these is the
  // symbol valueOf the result is false.
  Label entry, loop;
  __ jmp(&entry);
  __ bind(&loop);
  __ mov(edx, FieldOperand(ebx, 0));
  __ cmp(edx, FACTORY->value_of_symbol());
  __ j(equal, if_false);
  __ add(Operand(ebx), Immediate(kPointerSize));
  __ bind(&entry);
  __ cmp(ebx, Operand(ecx));
  __ j(not_equal, &loop);

  // Reload map as register ebx was used as temporary above.
  __ mov(ebx, FieldOperand(eax, HeapObject::kMapOffset));

  // If a valueOf property is not found on the object check that it's
  // prototype is the un-modified String prototype. If not result is false.
  __ mov(ecx, FieldOperand(ebx, Map::kPrototypeOffset));
  __ JumpIfSmi(ecx, if_false);
  __ mov(ecx, FieldOperand(ecx, HeapObject::kMapOffset));
  __ mov(edx, Operand(esi, Context::SlotOffset(Context::GLOBAL_INDEX)));
  __ mov(edx,
         FieldOperand(edx, GlobalObject::kGlobalContextOffset));
  __ cmp(ecx,
         ContextOperand(edx,
                        Context::STRING_FUNCTION_PROTOTYPE_MAP_INDEX));
  __ j(not_equal, if_false);
  // Set the bit in the map to indicate that it has been checked safe for
  // default valueOf and set true result.
  __ or_(FieldOperand(ebx, Map::kBitField2Offset),
         Immediate(1 << Map::kStringWrapperSafeForDefaultValueOf));
  __ jmp(if_true);

  PrepareForBailoutBeforeSplit(TOS_REG, true, if_true, if_false);
  context()->Plug(if_true, if_false);
}


void FullCodeGenerator::EmitIsFunction(ZoneList<Expression*>* args) {
  ASSERT(args->length() == 1);

  VisitForAccumulatorValue(args->at(0));

  Label materialize_true, materialize_false;
  Label* if_true = NULL;
  Label* if_false = NULL;
  Label* fall_through = NULL;
  context()->PrepareTest(&materialize_true, &materialize_false,
                         &if_true, &if_false, &fall_through);

  __ JumpIfSmi(eax, if_false);
  __ CmpObjectType(eax, JS_FUNCTION_TYPE, ebx);
  PrepareForBailoutBeforeSplit(TOS_REG, true, if_true, if_false);
  Split(equal, if_true, if_false, fall_through);

  context()->Plug(if_true, if_false);
}


void FullCodeGenerator::EmitIsArray(ZoneList<Expression*>* args) {
  ASSERT(args->length() == 1);

  VisitForAccumulatorValue(args->at(0));

  Label materialize_true, materialize_false;
  Label* if_true = NULL;
  Label* if_false = NULL;
  Label* fall_through = NULL;
  context()->PrepareTest(&materialize_true, &materialize_false,
                         &if_true, &if_false, &fall_through);

  __ JumpIfSmi(eax, if_false);
  __ CmpObjectType(eax, JS_ARRAY_TYPE, ebx);
  PrepareForBailoutBeforeSplit(TOS_REG, true, if_true, if_false);
  Split(equal, if_true, if_false, fall_through);

  context()->Plug(if_true, if_false);
}


void FullCodeGenerator::EmitIsRegExp(ZoneList<Expression*>* args) {
  ASSERT(args->length() == 1);

  VisitForAccumulatorValue(args->at(0));

  Label materialize_true, materialize_false;
  Label* if_true = NULL;
  Label* if_false = NULL;
  Label* fall_through = NULL;
  context()->PrepareTest(&materialize_true, &materialize_false,
                         &if_true, &if_false, &fall_through);

  __ JumpIfSmi(eax, if_false);
  __ CmpObjectType(eax, JS_REGEXP_TYPE, ebx);
  PrepareForBailoutBeforeSplit(TOS_REG, true, if_true, if_false);
  Split(equal, if_true, if_false, fall_through);

  context()->Plug(if_true, if_false);
}



void FullCodeGenerator::EmitIsConstructCall(ZoneList<Expression*>* args) {
  ASSERT(args->length() == 0);

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
  PrepareForBailoutBeforeSplit(TOS_REG, true, if_true, if_false);
  Split(equal, if_true, if_false, fall_through);

  context()->Plug(if_true, if_false);
}


void FullCodeGenerator::EmitObjectEquals(ZoneList<Expression*>* args) {
  ASSERT(args->length() == 2);

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
  decrement_stack_height();
  __ cmp(eax, Operand(ebx));
  PrepareForBailoutBeforeSplit(TOS_REG, true, if_true, if_false);
  Split(equal, if_true, if_false, fall_through);

  context()->Plug(if_true, if_false);
}


void FullCodeGenerator::EmitArguments(ZoneList<Expression*>* args) {
  ASSERT(args->length() == 1);

  // ArgumentsAccessStub expects the key in edx and the formal
  // parameter count in eax.
  VisitForAccumulatorValue(args->at(0));
  __ mov(edx, eax);
  __ SafeSet(eax, Immediate(Smi::FromInt(info_->scope()->num_parameters())));
  ArgumentsAccessStub stub(ArgumentsAccessStub::READ_ELEMENT);
  __ CallStub(&stub);
  context()->Plug(eax);
}


void FullCodeGenerator::EmitArgumentsLength(ZoneList<Expression*>* args) {
  ASSERT(args->length() == 0);

  Label exit;
  // Get the number of formal parameters.
  __ SafeSet(eax, Immediate(Smi::FromInt(info_->scope()->num_parameters())));

  // Check if the calling frame is an arguments adaptor frame.
  __ mov(ebx, Operand(ebp, StandardFrameConstants::kCallerFPOffset));
  __ cmp(Operand(ebx, StandardFrameConstants::kContextOffset),
         Immediate(Smi::FromInt(StackFrame::ARGUMENTS_ADAPTOR)));
  __ j(not_equal, &exit);

  // Arguments adaptor case: Read the arguments length from the
  // adaptor frame.
  __ mov(eax, Operand(ebx, ArgumentsAdaptorFrameConstants::kLengthOffset));

  __ bind(&exit);
  if (FLAG_debug_code) __ AbortIfNotSmi(eax);
  context()->Plug(eax);
}


void FullCodeGenerator::EmitClassOf(ZoneList<Expression*>* args) {
  ASSERT(args->length() == 1);
  Label done, null, function, non_function_constructor;

  VisitForAccumulatorValue(args->at(0));

  // If the object is a smi, we return null.
  __ JumpIfSmi(eax, &null);

  // Check that the object is a JS object but take special care of JS
  // functions to make sure they have 'Function' as their class.
  __ CmpObjectType(eax, FIRST_SPEC_OBJECT_TYPE, eax);
  // Map is now in eax.
  __ j(below, &null);

  // As long as LAST_CALLABLE_SPEC_OBJECT_TYPE is the last instance type, and
  // FIRST_CALLABLE_SPEC_OBJECT_TYPE comes right after
  // LAST_NONCALLABLE_SPEC_OBJECT_TYPE, we can avoid checking for the latter.
  STATIC_ASSERT(LAST_TYPE == LAST_CALLABLE_SPEC_OBJECT_TYPE);
  STATIC_ASSERT(FIRST_CALLABLE_SPEC_OBJECT_TYPE ==
                LAST_NONCALLABLE_SPEC_OBJECT_TYPE + 1);
  __ CmpInstanceType(eax, FIRST_CALLABLE_SPEC_OBJECT_TYPE);
  __ j(above_equal, &function);

  // Check if the constructor in the map is a function.
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
  __ mov(eax, isolate()->factory()->function_class_symbol());
  __ jmp(&done);

  // Objects with a non-function constructor have class 'Object'.
  __ bind(&non_function_constructor);
  __ mov(eax, isolate()->factory()->Object_symbol());
  __ jmp(&done);

  // Non-JS objects have class null.
  __ bind(&null);
  __ mov(eax, isolate()->factory()->null_value());

  // All done.
  __ bind(&done);

  context()->Plug(eax);
}


void FullCodeGenerator::EmitLog(ZoneList<Expression*>* args) {
  // Conditionally generate a log call.
  // Args:
  //   0 (literal string): The type of logging (corresponds to the flags).
  //     This is used to determine whether or not to generate the log call.
  //   1 (string): Format string.  Access the string at argument index 2
  //     with '%2s' (see Logger::LogRuntime for all the formats).
  //   2 (array): Arguments to the format string.
  ASSERT_EQ(args->length(), 3);
  if (CodeGenerator::ShouldGenerateLog(args->at(0))) {
    VisitForStackValue(args->at(1));
    VisitForStackValue(args->at(2));
    __ CallRuntime(Runtime::kLog, 2);
    decrement_stack_height(2);
  }
  // Finally, we're expected to leave a value on the top of the stack.
  __ mov(eax, isolate()->factory()->undefined_value());
  context()->Plug(eax);
}


void FullCodeGenerator::EmitRandomHeapNumber(ZoneList<Expression*>* args) {
  ASSERT(args->length() == 0);

  Label slow_allocate_heapnumber;
  Label heapnumber_allocated;

  __ AllocateHeapNumber(edi, ebx, ecx, &slow_allocate_heapnumber);
  __ jmp(&heapnumber_allocated);

  __ bind(&slow_allocate_heapnumber);
  // Allocate a heap number.
  __ CallRuntime(Runtime::kNumberAlloc, 0);
  __ mov(edi, eax);

  __ bind(&heapnumber_allocated);

  __ PrepareCallCFunction(1, ebx);
  __ mov(Operand(esp, 0), Immediate(ExternalReference::isolate_address()));
  __ CallCFunction(ExternalReference::random_uint32_function(isolate()),
                   1);

  // Convert 32 random bits in eax to 0.(32 random bits) in a double
  // by computing:
  // ( 1.(20 0s)(32 random bits) x 2^20 ) - (1.0 x 2^20)).
  // This is implemented on both SSE2 and FPU.
  if (CpuFeatures::IsSupported(SSE2)) {
    CpuFeatures::Scope fscope(SSE2);
    __ mov(ebx, Immediate(0x49800000));  // 1.0 x 2^20 as single.
    __ movd(xmm1, Operand(ebx));
    __ movd(xmm0, Operand(eax));
    __ cvtss2sd(xmm1, xmm1);
    __ xorps(xmm0, xmm1);
    __ subsd(xmm0, xmm1);
    __ movdbl(FieldOperand(edi, HeapNumber::kValueOffset), xmm0);
  } else {
    // 0x4130000000000000 is 1.0 x 2^20 as a double.
    __ mov(FieldOperand(edi, HeapNumber::kExponentOffset),
           Immediate(0x41300000));
    __ mov(FieldOperand(edi, HeapNumber::kMantissaOffset), eax);
    __ fld_d(FieldOperand(edi, HeapNumber::kValueOffset));
    __ mov(FieldOperand(edi, HeapNumber::kMantissaOffset), Immediate(0));
    __ fld_d(FieldOperand(edi, HeapNumber::kValueOffset));
    __ fsubp(1);
    __ fstp_d(FieldOperand(edi, HeapNumber::kValueOffset));
  }
  __ mov(eax, edi);
  context()->Plug(eax);
}


void FullCodeGenerator::EmitSubString(ZoneList<Expression*>* args) {
  // Load the arguments on the stack and call the stub.
  SubStringStub stub;
  ASSERT(args->length() == 3);
  VisitForStackValue(args->at(0));
  VisitForStackValue(args->at(1));
  VisitForStackValue(args->at(2));
  __ CallStub(&stub);
  decrement_stack_height(3);
  context()->Plug(eax);
}


void FullCodeGenerator::EmitRegExpExec(ZoneList<Expression*>* args) {
  // Load the arguments on the stack and call the stub.
  RegExpExecStub stub;
  ASSERT(args->length() == 4);
  VisitForStackValue(args->at(0));
  VisitForStackValue(args->at(1));
  VisitForStackValue(args->at(2));
  VisitForStackValue(args->at(3));
  __ CallStub(&stub);
  decrement_stack_height(4);
  context()->Plug(eax);
}


void FullCodeGenerator::EmitValueOf(ZoneList<Expression*>* args) {
  ASSERT(args->length() == 1);

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


void FullCodeGenerator::EmitMathPow(ZoneList<Expression*>* args) {
  // Load the arguments on the stack and call the runtime function.
  ASSERT(args->length() == 2);
  VisitForStackValue(args->at(0));
  VisitForStackValue(args->at(1));

  if (CpuFeatures::IsSupported(SSE2)) {
    MathPowStub stub;
    __ CallStub(&stub);
  } else {
    __ CallRuntime(Runtime::kMath_pow, 2);
  }
  decrement_stack_height(2);
  context()->Plug(eax);
}


void FullCodeGenerator::EmitSetValueOf(ZoneList<Expression*>* args) {
  ASSERT(args->length() == 2);

  VisitForStackValue(args->at(0));  // Load the object.
  VisitForAccumulatorValue(args->at(1));  // Load the value.
  __ pop(ebx);  // eax = value. ebx = object.
  decrement_stack_height();

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
  __ RecordWrite(ebx, JSValue::kValueOffset, edx, ecx);

  __ bind(&done);
  context()->Plug(eax);
}


void FullCodeGenerator::EmitNumberToString(ZoneList<Expression*>* args) {
  ASSERT_EQ(args->length(), 1);

  // Load the argument on the stack and call the stub.
  VisitForStackValue(args->at(0));

  NumberToStringStub stub;
  __ CallStub(&stub);
  decrement_stack_height();
  context()->Plug(eax);
}


void FullCodeGenerator::EmitStringCharFromCode(ZoneList<Expression*>* args) {
  ASSERT(args->length() == 1);

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


void FullCodeGenerator::EmitStringCharCodeAt(ZoneList<Expression*>* args) {
  ASSERT(args->length() == 2);

  VisitForStackValue(args->at(0));
  VisitForAccumulatorValue(args->at(1));

  Register object = ebx;
  Register index = eax;
  Register scratch = ecx;
  Register result = edx;

  __ pop(object);
  decrement_stack_height();

  Label need_conversion;
  Label index_out_of_range;
  Label done;
  StringCharCodeAtGenerator generator(object,
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
  // NaN.
  __ Set(result, Immediate(isolate()->factory()->nan_value()));
  __ jmp(&done);

  __ bind(&need_conversion);
  // Move the undefined value into the result register, which will
  // trigger conversion.
  __ Set(result, Immediate(isolate()->factory()->undefined_value()));
  __ jmp(&done);

  NopRuntimeCallHelper call_helper;
  generator.GenerateSlow(masm_, call_helper);

  __ bind(&done);
  context()->Plug(result);
}


void FullCodeGenerator::EmitStringCharAt(ZoneList<Expression*>* args) {
  ASSERT(args->length() == 2);

  VisitForStackValue(args->at(0));
  VisitForAccumulatorValue(args->at(1));

  Register object = ebx;
  Register index = eax;
  Register scratch1 = ecx;
  Register scratch2 = edx;
  Register result = eax;

  __ pop(object);
  decrement_stack_height();

  Label need_conversion;
  Label index_out_of_range;
  Label done;
  StringCharAtGenerator generator(object,
                                  index,
                                  scratch1,
                                  scratch2,
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
  __ Set(result, Immediate(isolate()->factory()->empty_string()));
  __ jmp(&done);

  __ bind(&need_conversion);
  // Move smi zero into the result register, which will trigger
  // conversion.
  __ Set(result, Immediate(Smi::FromInt(0)));
  __ jmp(&done);

  NopRuntimeCallHelper call_helper;
  generator.GenerateSlow(masm_, call_helper);

  __ bind(&done);
  context()->Plug(result);
}


void FullCodeGenerator::EmitStringAdd(ZoneList<Expression*>* args) {
  ASSERT_EQ(2, args->length());

  VisitForStackValue(args->at(0));
  VisitForStackValue(args->at(1));

  StringAddStub stub(NO_STRING_ADD_FLAGS);
  __ CallStub(&stub);
  decrement_stack_height(2);
  context()->Plug(eax);
}


void FullCodeGenerator::EmitStringCompare(ZoneList<Expression*>* args) {
  ASSERT_EQ(2, args->length());

  VisitForStackValue(args->at(0));
  VisitForStackValue(args->at(1));

  StringCompareStub stub;
  __ CallStub(&stub);
  decrement_stack_height(2);
  context()->Plug(eax);
}


void FullCodeGenerator::EmitMathSin(ZoneList<Expression*>* args) {
  // Load the argument on the stack and call the stub.
  TranscendentalCacheStub stub(TranscendentalCache::SIN,
                               TranscendentalCacheStub::TAGGED);
  ASSERT(args->length() == 1);
  VisitForStackValue(args->at(0));
  __ CallStub(&stub);
  decrement_stack_height();
  context()->Plug(eax);
}


void FullCodeGenerator::EmitMathCos(ZoneList<Expression*>* args) {
  // Load the argument on the stack and call the stub.
  TranscendentalCacheStub stub(TranscendentalCache::COS,
                               TranscendentalCacheStub::TAGGED);
  ASSERT(args->length() == 1);
  VisitForStackValue(args->at(0));
  __ CallStub(&stub);
  decrement_stack_height();
  context()->Plug(eax);
}


void FullCodeGenerator::EmitMathLog(ZoneList<Expression*>* args) {
  // Load the argument on the stack and call the stub.
  TranscendentalCacheStub stub(TranscendentalCache::LOG,
                               TranscendentalCacheStub::TAGGED);
  ASSERT(args->length() == 1);
  VisitForStackValue(args->at(0));
  __ CallStub(&stub);
  decrement_stack_height();
  context()->Plug(eax);
}


void FullCodeGenerator::EmitMathSqrt(ZoneList<Expression*>* args) {
  // Load the argument on the stack and call the runtime function.
  ASSERT(args->length() == 1);
  VisitForStackValue(args->at(0));
  __ CallRuntime(Runtime::kMath_sqrt, 1);
  decrement_stack_height();
  context()->Plug(eax);
}


void FullCodeGenerator::EmitCallFunction(ZoneList<Expression*>* args) {
  ASSERT(args->length() >= 2);

  int arg_count = args->length() - 2;  // 2 ~ receiver and function.
  for (int i = 0; i < arg_count + 1; ++i) {
    VisitForStackValue(args->at(i));
  }
  VisitForAccumulatorValue(args->last());  // Function.

  // InvokeFunction requires the function in edi. Move it in there.
  __ mov(edi, result_register());
  ParameterCount count(arg_count);
  __ InvokeFunction(edi, count, CALL_FUNCTION,
                    NullCallWrapper(), CALL_AS_METHOD);
  __ mov(esi, Operand(ebp, StandardFrameConstants::kContextOffset));
  decrement_stack_height(arg_count + 1);
  context()->Plug(eax);
}


void FullCodeGenerator::EmitRegExpConstructResult(ZoneList<Expression*>* args) {
  // Load the arguments on the stack and call the stub.
  RegExpConstructResultStub stub;
  ASSERT(args->length() == 3);
  VisitForStackValue(args->at(0));
  VisitForStackValue(args->at(1));
  VisitForStackValue(args->at(2));
  __ CallStub(&stub);
  decrement_stack_height(3);
  context()->Plug(eax);
}


void FullCodeGenerator::EmitSwapElements(ZoneList<Expression*>* args) {
  ASSERT(args->length() == 3);
  VisitForStackValue(args->at(0));
  VisitForStackValue(args->at(1));
  VisitForStackValue(args->at(2));
  Label done;
  Label slow_case;
  Register object = eax;
  Register index_1 = ebx;
  Register index_2 = ecx;
  Register elements = edi;
  Register temp = edx;
  __ mov(object, Operand(esp, 2 * kPointerSize));
  // Fetch the map and check if array is in fast case.
  // Check that object doesn't require security checks and
  // has no indexed interceptor.
  __ CmpObjectType(object, JS_ARRAY_TYPE, temp);
  __ j(not_equal, &slow_case);
  __ test_b(FieldOperand(temp, Map::kBitFieldOffset),
            KeyedLoadIC::kSlowCaseBitFieldMask);
  __ j(not_zero, &slow_case);

  // Check the object's elements are in fast case and writable.
  __ mov(elements, FieldOperand(object, JSObject::kElementsOffset));
  __ cmp(FieldOperand(elements, HeapObject::kMapOffset),
         Immediate(isolate()->factory()->fixed_array_map()));
  __ j(not_equal, &slow_case);

  // Check that both indices are smis.
  __ mov(index_1, Operand(esp, 1 * kPointerSize));
  __ mov(index_2, Operand(esp, 0));
  __ mov(temp, index_1);
  __ or_(temp, Operand(index_2));
  __ JumpIfNotSmi(temp, &slow_case);

  // Check that both indices are valid.
  __ mov(temp, FieldOperand(object, JSArray::kLengthOffset));
  __ cmp(temp, Operand(index_1));
  __ j(below_equal, &slow_case);
  __ cmp(temp, Operand(index_2));
  __ j(below_equal, &slow_case);

  // Bring addresses into index1 and index2.
  __ lea(index_1, CodeGenerator::FixedArrayElementOperand(elements, index_1));
  __ lea(index_2, CodeGenerator::FixedArrayElementOperand(elements, index_2));

  // Swap elements.  Use object and temp as scratch registers.
  __ mov(object, Operand(index_1, 0));
  __ mov(temp,   Operand(index_2, 0));
  __ mov(Operand(index_2, 0), object);
  __ mov(Operand(index_1, 0), temp);

  Label new_space;
  __ InNewSpace(elements, temp, equal, &new_space);

  __ mov(object, elements);
  __ RecordWriteHelper(object, index_1, temp);
  __ RecordWriteHelper(elements, index_2, temp);

  __ bind(&new_space);
  // We are done. Drop elements from the stack, and return undefined.
  __ add(Operand(esp), Immediate(3 * kPointerSize));
  __ mov(eax, isolate()->factory()->undefined_value());
  __ jmp(&done);

  __ bind(&slow_case);
  __ CallRuntime(Runtime::kSwapElements, 3);

  __ bind(&done);
  decrement_stack_height(3);
  context()->Plug(eax);
}


void FullCodeGenerator::EmitGetFromCache(ZoneList<Expression*>* args) {
  ASSERT_EQ(2, args->length());

  ASSERT_NE(NULL, args->at(0)->AsLiteral());
  int cache_id = Smi::cast(*(args->at(0)->AsLiteral()->handle()))->value();

  Handle<FixedArray> jsfunction_result_caches(
      isolate()->global_context()->jsfunction_result_caches());
  if (jsfunction_result_caches->length() <= cache_id) {
    __ Abort("Attempt to use undefined cache.");
    __ mov(eax, isolate()->factory()->undefined_value());
    context()->Plug(eax);
    return;
  }

  VisitForAccumulatorValue(args->at(1));

  Register key = eax;
  Register cache = ebx;
  Register tmp = ecx;
  __ mov(cache, ContextOperand(esi, Context::GLOBAL_INDEX));
  __ mov(cache,
         FieldOperand(cache, GlobalObject::kGlobalContextOffset));
  __ mov(cache, ContextOperand(cache, Context::JSFUNCTION_RESULT_CACHES_INDEX));
  __ mov(cache,
         FieldOperand(cache, FixedArray::OffsetOfElementAt(cache_id)));

  Label done, not_found;
  // tmp now holds finger offset as a smi.
  STATIC_ASSERT(kSmiTag == 0 && kSmiTagSize == 1);
  __ mov(tmp, FieldOperand(cache, JSFunctionResultCache::kFingerOffset));
  __ cmp(key, CodeGenerator::FixedArrayElementOperand(cache, tmp));
  __ j(not_equal, &not_found);

  __ mov(eax, CodeGenerator::FixedArrayElementOperand(cache, tmp, 1));
  __ jmp(&done);

  __ bind(&not_found);
  // Call runtime to perform the lookup.
  __ push(cache);
  __ push(key);
  __ CallRuntime(Runtime::kGetFromCache, 2);

  __ bind(&done);
  context()->Plug(eax);
}


void FullCodeGenerator::EmitIsRegExpEquivalent(ZoneList<Expression*>* args) {
  ASSERT_EQ(2, args->length());

  Register right = eax;
  Register left = ebx;
  Register tmp = ecx;

  VisitForStackValue(args->at(0));
  VisitForAccumulatorValue(args->at(1));
  __ pop(left);

  Label done, fail, ok;
  __ cmp(left, Operand(right));
  __ j(equal, &ok);
  // Fail if either is a non-HeapObject.
  __ mov(tmp, left);
  __ and_(Operand(tmp), right);
  __ JumpIfSmi(tmp, &fail);
  __ mov(tmp, FieldOperand(left, HeapObject::kMapOffset));
  __ CmpInstanceType(tmp, JS_REGEXP_TYPE);
  __ j(not_equal, &fail);
  __ cmp(tmp, FieldOperand(right, HeapObject::kMapOffset));
  __ j(not_equal, &fail);
  __ mov(tmp, FieldOperand(left, JSRegExp::kDataOffset));
  __ cmp(tmp, FieldOperand(right, JSRegExp::kDataOffset));
  __ j(equal, &ok);
  __ bind(&fail);
  __ mov(eax, Immediate(isolate()->factory()->false_value()));
  __ jmp(&done);
  __ bind(&ok);
  __ mov(eax, Immediate(isolate()->factory()->true_value()));
  __ bind(&done);

  decrement_stack_height();
  context()->Plug(eax);
}


void FullCodeGenerator::EmitHasCachedArrayIndex(ZoneList<Expression*>* args) {
  ASSERT(args->length() == 1);

  VisitForAccumulatorValue(args->at(0));

  if (FLAG_debug_code) {
    __ AbortIfNotString(eax);
  }

  Label materialize_true, materialize_false;
  Label* if_true = NULL;
  Label* if_false = NULL;
  Label* fall_through = NULL;
  context()->PrepareTest(&materialize_true, &materialize_false,
                         &if_true, &if_false, &fall_through);

  __ test(FieldOperand(eax, String::kHashFieldOffset),
          Immediate(String::kContainsCachedArrayIndexMask));
  PrepareForBailoutBeforeSplit(TOS_REG, true, if_true, if_false);
  Split(zero, if_true, if_false, fall_through);

  context()->Plug(if_true, if_false);
}


void FullCodeGenerator::EmitGetCachedArrayIndex(ZoneList<Expression*>* args) {
  ASSERT(args->length() == 1);
  VisitForAccumulatorValue(args->at(0));

  if (FLAG_debug_code) {
    __ AbortIfNotString(eax);
  }

  __ mov(eax, FieldOperand(eax, String::kHashFieldOffset));
  __ IndexFromHash(eax, eax);

  context()->Plug(eax);
}


void FullCodeGenerator::EmitFastAsciiArrayJoin(ZoneList<Expression*>* args) {
  Label bailout, done, one_char_separator, long_separator,
      non_trivial_array, not_size_one_array, loop,
      loop_1, loop_1_condition, loop_2, loop_2_entry, loop_3, loop_3_entry;

  ASSERT(args->length() == 2);
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
  __ sub(Operand(esp), Immediate(2 * kPointerSize));
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


  // Check that all array elements are sequential ASCII strings, and
  // accumulate the sum of their lengths, as a smi-encoded value.
  __ Set(index, Immediate(0));
  __ Set(string_length, Immediate(0));
  // Loop condition: while (index < length).
  // Live loop registers: index, array_length, string,
  //                      scratch, string_length, elements.
  if (FLAG_debug_code) {
    __ cmp(index, Operand(array_length));
    __ Assert(less, "No empty arrays here in EmitFastAsciiArrayJoin");
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
  __ cmp(scratch, kStringTag | kAsciiStringTag | kSeqStringTag);
  __ j(not_equal, &bailout);
  __ add(string_length,
         FieldOperand(string, SeqAsciiString::kLengthOffset));
  __ j(overflow, &bailout);
  __ add(Operand(index), Immediate(1));
  __ cmp(index, Operand(array_length));
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

  // Check that the separator is a flat ASCII string.
  __ mov(string, separator_operand);
  __ JumpIfSmi(string, &bailout);
  __ mov(scratch, FieldOperand(string, HeapObject::kMapOffset));
  __ movzx_b(scratch, FieldOperand(scratch, Map::kInstanceTypeOffset));
  __ and_(scratch, Immediate(
      kIsNotStringMask | kStringEncodingMask | kStringRepresentationMask));
  __ cmp(scratch, ASCII_STRING_TYPE);
  __ j(not_equal, &bailout);

  // Add (separator length times array_length) - separator length
  // to string_length.
  __ mov(scratch, separator_operand);
  __ mov(scratch, FieldOperand(scratch, SeqAsciiString::kLengthOffset));
  __ sub(string_length, Operand(scratch));  // May be negative, temporarily.
  __ imul(scratch, array_length_operand);
  __ j(overflow, &bailout);
  __ add(string_length, Operand(scratch));
  __ j(overflow, &bailout);

  __ shr(string_length, 1);
  // Live registers and stack values:
  //   string_length
  //   elements
  __ AllocateAsciiString(result_pos, string_length, scratch,
                         index, string, &bailout);
  __ mov(result_operand, result_pos);
  __ lea(result_pos, FieldOperand(result_pos, SeqAsciiString::kHeaderSize));


  __ mov(string, separator_operand);
  __ cmp(FieldOperand(string, SeqAsciiString::kLengthOffset),
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
         FieldOperand(string, SeqAsciiString::kHeaderSize));
  __ CopyBytes(string, result_pos, string_length, scratch);
  __ add(Operand(index), Immediate(1));
  __ bind(&loop_1_condition);
  __ cmp(index, array_length_operand);
  __ j(less, &loop_1);  // End while (index < length).
  __ jmp(&done);



  // One-character separator case
  __ bind(&one_char_separator);
  // Replace separator with its ascii character value.
  __ mov_b(scratch, FieldOperand(string, SeqAsciiString::kHeaderSize));
  __ mov_b(separator_operand, scratch);

  __ Set(index, Immediate(0));
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
         FieldOperand(string, SeqAsciiString::kHeaderSize));
  __ CopyBytes(string, result_pos, string_length, scratch);
  __ add(Operand(index), Immediate(1));

  __ cmp(index, array_length_operand);
  __ j(less, &loop_2);  // End while (index < length).
  __ jmp(&done);


  // Long separator case (separator is more than one character).
  __ bind(&long_separator);

  __ Set(index, Immediate(0));
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
         FieldOperand(string, SeqAsciiString::kHeaderSize));
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
         FieldOperand(string, SeqAsciiString::kHeaderSize));
  __ CopyBytes(string, result_pos, string_length, scratch);
  __ add(Operand(index), Immediate(1));

  __ cmp(index, array_length_operand);
  __ j(less, &loop_3);  // End while (index < length).
  __ jmp(&done);


  __ bind(&bailout);
  __ mov(result_operand, isolate()->factory()->undefined_value());
  __ bind(&done);
  __ mov(eax, result_operand);
  // Drop temp values from the stack, and restore context register.
  __ add(Operand(esp), Immediate(3 * kPointerSize));

  __ mov(esi, Operand(ebp, StandardFrameConstants::kContextOffset));
  decrement_stack_height();
  context()->Plug(eax);
}


void FullCodeGenerator::VisitCallRuntime(CallRuntime* expr) {
  Handle<String> name = expr->name();
  if (name->length() > 0 && name->Get(0) == '_') {
    Comment cmnt(masm_, "[ InlineRuntimeCall");
    EmitInlineRuntimeCall(expr);
    return;
  }

  Comment cmnt(masm_, "[ CallRuntime");
  ZoneList<Expression*>* args = expr->arguments();

  if (expr->is_jsruntime()) {
    // Prepare for calling JS runtime function.
    __ mov(eax, GlobalObjectOperand());
    __ push(FieldOperand(eax, GlobalObject::kBuiltinsOffset));
    increment_stack_height();
  }

  // Push the arguments ("left-to-right").
  int arg_count = args->length();
  for (int i = 0; i < arg_count; i++) {
    VisitForStackValue(args->at(i));
  }

  if (expr->is_jsruntime()) {
    // Call the JS runtime function via a call IC.
    __ Set(ecx, Immediate(expr->name()));
    RelocInfo::Mode mode = RelocInfo::CODE_TARGET;
    Handle<Code> ic =
        isolate()->stub_cache()->ComputeCallInitialize(arg_count, mode);
    __ call(ic, mode, expr->id());
    // Restore context register.
    __ mov(esi, Operand(ebp, StandardFrameConstants::kContextOffset));
  } else {
    // Call the C runtime function.
    __ CallRuntime(expr->function(), arg_count);
  }
  decrement_stack_height(arg_count);
  if (expr->is_jsruntime()) {
    decrement_stack_height();
  }

  context()->Plug(eax);
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
        __ push(Immediate(Smi::FromInt(strict_mode_flag())));
        __ InvokeBuiltin(Builtins::DELETE, CALL_FUNCTION);
        decrement_stack_height(2);
        context()->Plug(eax);
      } else if (proxy != NULL) {
        Variable* var = proxy->var();
        // Delete of an unqualified identifier is disallowed in strict mode
        // but "delete this" is allowed.
        ASSERT(strict_mode_flag() == kNonStrictMode || var->is_this());
        if (var->IsUnallocated()) {
          __ push(GlobalObjectOperand());
          __ push(Immediate(var->name()));
          __ push(Immediate(Smi::FromInt(kNonStrictMode)));
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
          __ CallRuntime(Runtime::kDeleteContextSlot, 2);
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
      } else {
        Label materialize_true, materialize_false;
        Label* if_true = NULL;
        Label* if_false = NULL;
        Label* fall_through = NULL;

        // Notice that the labels are swapped.
        context()->PrepareTest(&materialize_true, &materialize_false,
                               &if_false, &if_true, &fall_through);
        if (context()->IsTest()) ForwardBailoutToChild(expr);
        VisitForControl(expr->expression(), if_true, if_false, fall_through);
        context()->Plug(if_false, if_true);  // Labels swapped.
      }
      break;
    }

    case Token::TYPEOF: {
      Comment cmnt(masm_, "[ UnaryOperation (TYPEOF)");
      { StackValueContext context(this);
        VisitForTypeofValue(expr->expression());
      }
      __ CallRuntime(Runtime::kTypeof, 1);
      decrement_stack_height();
      context()->Plug(eax);
      break;
    }

    case Token::ADD: {
      Comment cmt(masm_, "[ UnaryOperation (ADD)");
      VisitForAccumulatorValue(expr->expression());
      Label no_conversion;
      __ JumpIfSmi(result_register(), &no_conversion);
      ToNumberStub convert_stub;
      __ CallStub(&convert_stub);
      __ bind(&no_conversion);
      context()->Plug(result_register());
      break;
    }

    case Token::SUB:
      EmitUnaryOperation(expr, "[ UnaryOperation (SUB)");
      break;

    case Token::BIT_NOT:
      EmitUnaryOperation(expr, "[ UnaryOperation (BIT_NOT)");
      break;

    default:
      UNREACHABLE();
  }
}


void FullCodeGenerator::EmitUnaryOperation(UnaryOperation* expr,
                                           const char* comment) {
  Comment cmt(masm_, comment);
  bool can_overwrite = expr->expression()->ResultOverwriteAllowed();
  UnaryOverwriteMode overwrite =
      can_overwrite ? UNARY_OVERWRITE : UNARY_NO_OVERWRITE;
  UnaryOpStub stub(expr->op(), overwrite);
  // UnaryOpStub expects the argument to be in the
  // accumulator register eax.
  VisitForAccumulatorValue(expr->expression());
  SetSourcePosition(expr->position());
  __ call(stub.GetCode(), RelocInfo::CODE_TARGET, expr->id());
  context()->Plug(eax);
}


void FullCodeGenerator::VisitCountOperation(CountOperation* expr) {
  Comment cmnt(masm_, "[ CountOperation");
  SetSourcePosition(expr->position());

  // Invalid left-hand sides are rewritten to have a 'throw ReferenceError'
  // as the left-hand side.
  if (!expr->expression()->IsValidLeftHandSide()) {
    ASSERT(expr->expression()->AsThrow() != NULL);
    VisitInCurrentContext(expr->expression());
    // Visiting Throw does not plug the context.
    context()->Plug(eax);
    return;
  }

  // Expression can only be a property, a global or a (parameter or local)
  // slot.
  enum LhsKind { VARIABLE, NAMED_PROPERTY, KEYED_PROPERTY };
  LhsKind assign_type = VARIABLE;
  Property* prop = expr->expression()->AsProperty();
  // In case of a property we use the uninitialized expression context
  // of the key to detect a named property.
  if (prop != NULL) {
    assign_type =
        (prop->key()->IsPropertyName()) ? NAMED_PROPERTY : KEYED_PROPERTY;
  }

  // Evaluate expression and get value.
  if (assign_type == VARIABLE) {
    ASSERT(expr->expression()->AsVariableProxy()->var() != NULL);
    AccumulatorValueContext context(this);
    EmitVariableLoad(expr->expression()->AsVariableProxy());
  } else {
    // Reserve space for result of postfix operation.
    if (expr->is_postfix() && !context()->IsEffect()) {
      __ push(Immediate(Smi::FromInt(0)));
      increment_stack_height();
    }
    if (assign_type == NAMED_PROPERTY) {
      // Put the object both on the stack and in the accumulator.
      VisitForAccumulatorValue(prop->obj());
      __ push(eax);
      increment_stack_height();
      EmitNamedPropertyLoad(prop);
    } else {
      VisitForStackValue(prop->obj());
      VisitForAccumulatorValue(prop->key());
      __ mov(edx, Operand(esp, 0));
      __ push(eax);
      increment_stack_height();
      EmitKeyedPropertyLoad(prop);
    }
  }

  // We need a second deoptimization point after loading the value
  // in case evaluating the property load my have a side effect.
  if (assign_type == VARIABLE) {
    PrepareForBailout(expr->expression(), TOS_REG);
  } else {
    PrepareForBailoutForId(expr->CountId(), TOS_REG);
  }

  // Call ToNumber only if operand is not a smi.
  Label no_conversion;
  if (ShouldInlineSmiCase(expr->op())) {
    __ JumpIfSmi(eax, &no_conversion, Label::kNear);
  }
  ToNumberStub convert_stub;
  __ CallStub(&convert_stub);
  __ bind(&no_conversion);

  // Save result for postfix expressions.
  if (expr->is_postfix()) {
    if (!context()->IsEffect()) {
      // Save the result on the stack. If we have a named or keyed property
      // we store the result under the receiver that is currently on top
      // of the stack.
      switch (assign_type) {
        case VARIABLE:
          __ push(eax);
          increment_stack_height();
          break;
        case NAMED_PROPERTY:
          __ mov(Operand(esp, kPointerSize), eax);
          break;
        case KEYED_PROPERTY:
          __ mov(Operand(esp, 2 * kPointerSize), eax);
          break;
      }
    }
  }

  // Inline smi case if we are in a loop.
  Label done, stub_call;
  JumpPatchSite patch_site(masm_);

  if (ShouldInlineSmiCase(expr->op())) {
    if (expr->op() == Token::INC) {
      __ add(Operand(eax), Immediate(Smi::FromInt(1)));
    } else {
      __ sub(Operand(eax), Immediate(Smi::FromInt(1)));
    }
    __ j(overflow, &stub_call, Label::kNear);
    // We could eliminate this smi check if we split the code at
    // the first smi check before calling ToNumber.
    patch_site.EmitJumpIfSmi(eax, &done, Label::kNear);

    __ bind(&stub_call);
    // Call stub. Undo operation first.
    if (expr->op() == Token::INC) {
      __ sub(Operand(eax), Immediate(Smi::FromInt(1)));
    } else {
      __ add(Operand(eax), Immediate(Smi::FromInt(1)));
    }
  }

  // Record position before stub call.
  SetSourcePosition(expr->position());

  // Call stub for +1/-1.
  __ mov(edx, eax);
  __ mov(eax, Immediate(Smi::FromInt(1)));
  BinaryOpStub stub(expr->binary_op(), NO_OVERWRITE);
  __ call(stub.GetCode(), RelocInfo::CODE_TARGET, expr->CountId());
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
      __ mov(ecx, prop->key()->AsLiteral()->handle());
      __ pop(edx);
      decrement_stack_height();
      Handle<Code> ic = is_strict_mode()
          ? isolate()->builtins()->StoreIC_Initialize_Strict()
          : isolate()->builtins()->StoreIC_Initialize();
      __ call(ic, RelocInfo::CODE_TARGET, expr->id());
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
    case KEYED_PROPERTY: {
      __ pop(ecx);
      __ pop(edx);
      decrement_stack_height();
      decrement_stack_height();
      Handle<Code> ic = is_strict_mode()
          ? isolate()->builtins()->KeyedStoreIC_Initialize_Strict()
          : isolate()->builtins()->KeyedStoreIC_Initialize();
      __ call(ic, RelocInfo::CODE_TARGET, expr->id());
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
  ASSERT(!context()->IsEffect());
  ASSERT(!context()->IsTest());

  if (proxy != NULL && proxy->var()->IsUnallocated()) {
    Comment cmnt(masm_, "Global variable");
    __ mov(eax, GlobalObjectOperand());
    __ mov(ecx, Immediate(proxy->name()));
    Handle<Code> ic = isolate()->builtins()->LoadIC_Initialize();
    // Use a regular load, not a contextual load, to avoid a reference
    // error.
    __ call(ic);
    PrepareForBailout(expr, TOS_REG);
    context()->Plug(eax);
  } else if (proxy != NULL && proxy->var()->IsLookupSlot()) {
    Label done, slow;

    // Generate code for loading from variables potentially shadowed
    // by eval-introduced variables.
    EmitDynamicLookupFastCase(proxy->var(), INSIDE_TYPEOF, &slow, &done);

    __ bind(&slow);
    __ push(esi);
    __ push(Immediate(proxy->name()));
    __ CallRuntime(Runtime::kLoadContextSlotNoReferenceError, 2);
    PrepareForBailout(expr, TOS_REG);
    __ bind(&done);

    context()->Plug(eax);
  } else {
    // This expression cannot throw a reference error at the top level.
    VisitInCurrentContext(expr);
  }
}


void FullCodeGenerator::EmitLiteralCompareTypeof(Expression* expr,
                                                 Handle<String> check,
                                                 Label* if_true,
                                                 Label* if_false,
                                                 Label* fall_through) {
  { AccumulatorValueContext context(this);
    VisitForTypeofValue(expr);
  }
  PrepareForBailoutBeforeSplit(TOS_REG, true, if_true, if_false);

  if (check->Equals(isolate()->heap()->number_symbol())) {
    __ JumpIfSmi(eax, if_true);
    __ cmp(FieldOperand(eax, HeapObject::kMapOffset),
           isolate()->factory()->heap_number_map());
    Split(equal, if_true, if_false, fall_through);
  } else if (check->Equals(isolate()->heap()->string_symbol())) {
    __ JumpIfSmi(eax, if_false);
    __ CmpObjectType(eax, FIRST_NONSTRING_TYPE, edx);
    __ j(above_equal, if_false);
    // Check for undetectable objects => false.
    __ test_b(FieldOperand(edx, Map::kBitFieldOffset),
              1 << Map::kIsUndetectable);
    Split(zero, if_true, if_false, fall_through);
  } else if (check->Equals(isolate()->heap()->boolean_symbol())) {
    __ cmp(eax, isolate()->factory()->true_value());
    __ j(equal, if_true);
    __ cmp(eax, isolate()->factory()->false_value());
    Split(equal, if_true, if_false, fall_through);
  } else if (FLAG_harmony_typeof &&
             check->Equals(isolate()->heap()->null_symbol())) {
    __ cmp(eax, isolate()->factory()->null_value());
    Split(equal, if_true, if_false, fall_through);
  } else if (check->Equals(isolate()->heap()->undefined_symbol())) {
    __ cmp(eax, isolate()->factory()->undefined_value());
    __ j(equal, if_true);
    __ JumpIfSmi(eax, if_false);
    // Check for undetectable objects => true.
    __ mov(edx, FieldOperand(eax, HeapObject::kMapOffset));
    __ movzx_b(ecx, FieldOperand(edx, Map::kBitFieldOffset));
    __ test(ecx, Immediate(1 << Map::kIsUndetectable));
    Split(not_zero, if_true, if_false, fall_through);
  } else if (check->Equals(isolate()->heap()->function_symbol())) {
    __ JumpIfSmi(eax, if_false);
    __ CmpObjectType(eax, FIRST_CALLABLE_SPEC_OBJECT_TYPE, edx);
    Split(above_equal, if_true, if_false, fall_through);
  } else if (check->Equals(isolate()->heap()->object_symbol())) {
    __ JumpIfSmi(eax, if_false);
    if (!FLAG_harmony_typeof) {
      __ cmp(eax, isolate()->factory()->null_value());
      __ j(equal, if_true);
    }
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
}


void FullCodeGenerator::EmitLiteralCompareUndefined(Expression* expr,
                                                    Label* if_true,
                                                    Label* if_false,
                                                    Label* fall_through) {
  VisitForAccumulatorValue(expr);
  PrepareForBailoutBeforeSplit(TOS_REG, true, if_true, if_false);

  __ cmp(eax, isolate()->factory()->undefined_value());
  Split(equal, if_true, if_false, fall_through);
}


void FullCodeGenerator::VisitCompareOperation(CompareOperation* expr) {
  Comment cmnt(masm_, "[ CompareOperation");
  SetSourcePosition(expr->position());

  // Always perform the comparison for its control flow.  Pack the result
  // into the expression's context after the comparison is performed.

  Label materialize_true, materialize_false;
  Label* if_true = NULL;
  Label* if_false = NULL;
  Label* fall_through = NULL;
  context()->PrepareTest(&materialize_true, &materialize_false,
                         &if_true, &if_false, &fall_through);

  // First we try a fast inlined version of the compare when one of
  // the operands is a literal.
  if (TryLiteralCompare(expr, if_true, if_false, fall_through)) {
    context()->Plug(if_true, if_false);
    return;
  }

  Token::Value op = expr->op();
  VisitForStackValue(expr->left());
  switch (expr->op()) {
    case Token::IN:
      VisitForStackValue(expr->right());
      __ InvokeBuiltin(Builtins::IN, CALL_FUNCTION);
      decrement_stack_height(2);
      PrepareForBailoutBeforeSplit(TOS_REG, false, NULL, NULL);
      __ cmp(eax, isolate()->factory()->true_value());
      Split(equal, if_true, if_false, fall_through);
      break;

    case Token::INSTANCEOF: {
      VisitForStackValue(expr->right());
      InstanceofStub stub(InstanceofStub::kNoFlags);
      __ CallStub(&stub);
      decrement_stack_height(2);
      PrepareForBailoutBeforeSplit(TOS_REG, true, if_true, if_false);
      __ test(eax, Operand(eax));
      // The stub returns 0 for true.
      Split(zero, if_true, if_false, fall_through);
      break;
    }

    default: {
      VisitForAccumulatorValue(expr->right());
      Condition cc = no_condition;
      switch (op) {
        case Token::EQ_STRICT:
        case Token::EQ:
          cc = equal;
          __ pop(edx);
          break;
        case Token::LT:
          cc = less;
          __ pop(edx);
          break;
        case Token::GT:
          // Reverse left and right sizes to obtain ECMA-262 conversion order.
          cc = less;
          __ mov(edx, result_register());
          __ pop(eax);
         break;
        case Token::LTE:
          // Reverse left and right sizes to obtain ECMA-262 conversion order.
          cc = greater_equal;
          __ mov(edx, result_register());
          __ pop(eax);
          break;
        case Token::GTE:
          cc = greater_equal;
          __ pop(edx);
          break;
        case Token::IN:
        case Token::INSTANCEOF:
        default:
          UNREACHABLE();
      }
      decrement_stack_height();

      bool inline_smi_code = ShouldInlineSmiCase(op);
      JumpPatchSite patch_site(masm_);
      if (inline_smi_code) {
        Label slow_case;
        __ mov(ecx, Operand(edx));
        __ or_(ecx, Operand(eax));
        patch_site.EmitJumpIfNotSmi(ecx, &slow_case, Label::kNear);
        __ cmp(edx, Operand(eax));
        Split(cc, if_true, if_false, NULL);
        __ bind(&slow_case);
      }

      // Record position and call the compare IC.
      SetSourcePosition(expr->position());
      Handle<Code> ic = CompareIC::GetUninitialized(op);
      __ call(ic, RelocInfo::CODE_TARGET, expr->id());
      patch_site.EmitPatchInfo();

      PrepareForBailoutBeforeSplit(TOS_REG, true, if_true, if_false);
      __ test(eax, Operand(eax));
      Split(cc, if_true, if_false, fall_through);
    }
  }

  // Convert the result of the comparison into one expected for this
  // expression's context.
  context()->Plug(if_true, if_false);
}


void FullCodeGenerator::VisitCompareToNull(CompareToNull* expr) {
  Label materialize_true, materialize_false;
  Label* if_true = NULL;
  Label* if_false = NULL;
  Label* fall_through = NULL;
  context()->PrepareTest(&materialize_true, &materialize_false,
                         &if_true, &if_false, &fall_through);

  VisitForAccumulatorValue(expr->expression());
  PrepareForBailoutBeforeSplit(TOS_REG, true, if_true, if_false);

  __ cmp(eax, isolate()->factory()->null_value());
  if (expr->is_strict()) {
    Split(equal, if_true, if_false, fall_through);
  } else {
    __ j(equal, if_true);
    __ cmp(eax, isolate()->factory()->undefined_value());
    __ j(equal, if_true);
    __ JumpIfSmi(eax, if_false);
    // It can be an undetectable object.
    __ mov(edx, FieldOperand(eax, HeapObject::kMapOffset));
    __ movzx_b(edx, FieldOperand(edx, Map::kBitFieldOffset));
    __ test(edx, Immediate(1 << Map::kIsUndetectable));
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
  ASSERT_EQ(POINTER_SIZE_ALIGN(frame_offset), frame_offset);
  __ mov(Operand(ebp, frame_offset), value);
}


void FullCodeGenerator::LoadContextField(Register dst, int context_index) {
  __ mov(dst, ContextOperand(esi, context_index));
}


void FullCodeGenerator::PushFunctionArgumentForContextAllocation() {
  Scope* declaration_scope = scope()->DeclarationScope();
  if (declaration_scope->is_global_scope()) {
    // Contexts nested in the global context have a canonical empty function
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
    ASSERT(declaration_scope->is_function_scope());
    __ push(Operand(ebp, JavaScriptFrameConstants::kFunctionOffset));
  }
}


// ----------------------------------------------------------------------------
// Non-local control flow support.

void FullCodeGenerator::EnterFinallyBlock() {
  // Cook return address on top of stack (smi encoded Code* delta)
  ASSERT(!result_register().is(edx));
  __ pop(edx);
  __ sub(Operand(edx), Immediate(masm_->CodeObject()));
  STATIC_ASSERT(kSmiTagSize + kSmiShiftSize == 1);
  STATIC_ASSERT(kSmiTag == 0);
  __ SmiTag(edx);
  __ push(edx);
  // Store result register while executing finally block.
  __ push(result_register());
}


void FullCodeGenerator::ExitFinallyBlock() {
  ASSERT(!result_register().is(edx));
  __ pop(result_register());
  // Uncook return address.
  __ pop(edx);
  __ SmiUntag(edx);
  __ add(Operand(edx), Immediate(masm_->CodeObject()));
  __ jmp(Operand(edx));
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

} }  // namespace v8::internal

#endif  // V8_TARGET_ARCH_IA32
