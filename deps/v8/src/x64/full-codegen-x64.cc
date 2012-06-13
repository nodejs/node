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

#if defined(V8_TARGET_ARCH_X64)

#include "code-stubs.h"
#include "codegen.h"
#include "compiler.h"
#include "debug.h"
#include "full-codegen.h"
#include "isolate-inl.h"
#include "parser.h"
#include "scopes.h"
#include "stub-cache.h"

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
    ASSERT(patch_site_.is_bound() == info_emitted_);
  }

  void EmitJumpIfNotSmi(Register reg,
                        Label* target,
                        Label::Distance near_jump = Label::kFar) {
    __ testb(reg, Immediate(kSmiTagMask));
    EmitJump(not_carry, target, near_jump);   // Always taken before patched.
  }

  void EmitJumpIfSmi(Register reg,
                     Label* target,
                     Label::Distance near_jump = Label::kFar) {
    __ testb(reg, Immediate(kSmiTagMask));
    EmitJump(carry, target, near_jump);  // Never taken before patched.
  }

  void EmitPatchInfo() {
    if (patch_site_.is_bound()) {
      int delta_to_patch_site = masm_->SizeOfCodeGeneratedSince(&patch_site_);
      ASSERT(is_int8(delta_to_patch_site));
      __ testl(rax, Immediate(delta_to_patch_site));
#ifdef DEBUG
      info_emitted_ = true;
#endif
    } else {
      __ nop();  // Signals no inlined code.
    }
  }

 private:
  // jc will be patched with jz, jnc will become jnz.
  void EmitJump(Condition cc, Label* target, Label::Distance near_jump) {
    ASSERT(!patch_site_.is_bound() && !info_emitted_);
    ASSERT(cc == carry || cc == not_carry);
    __ bind(&patch_site_);
    __ j(cc, target, near_jump);
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
//   o rdi: the JS function object being called (i.e. ourselves)
//   o rsi: our context
//   o rbp: our caller's frame pointer
//   o rsp: stack pointer (pointing to return address)
//
// The function builds a JS frame.  Please see JavaScriptFrameConstants in
// frames-x64.h for its layout.
void FullCodeGenerator::Generate() {
  CompilationInfo* info = info_;
  handler_table_ =
      isolate()->factory()->NewFixedArray(function()->handler_count(), TENURED);
  profiling_counter_ = isolate()->factory()->NewJSGlobalPropertyCell(
      Handle<Smi>(Smi::FromInt(FLAG_interrupt_budget)));
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
  // receiver object). rcx is zero for method calls and non-zero for
  // function calls.
  if (!info->is_classic_mode() || info->is_native()) {
    Label ok;
    __ testq(rcx, rcx);
    __ j(zero, &ok, Label::kNear);
    // +1 for return address.
    int receiver_offset = (info->scope()->num_parameters() + 1) * kPointerSize;
    __ LoadRoot(kScratchRegister, Heap::kUndefinedValueRootIndex);
    __ movq(Operand(rsp, receiver_offset), kScratchRegister);
    __ bind(&ok);
  }

  // Open a frame scope to indicate that there is a frame on the stack.  The
  // MANUAL indicates that the scope shouldn't actually generate code to set up
  // the frame (that is done below).
  FrameScope frame_scope(masm_, StackFrame::MANUAL);

  __ push(rbp);  // Caller's frame pointer.
  __ movq(rbp, rsp);
  __ push(rsi);  // Callee's context.
  __ push(rdi);  // Callee's JS Function.

  { Comment cmnt(masm_, "[ Allocate locals");
    int locals_count = info->scope()->num_stack_slots();
    if (locals_count == 1) {
      __ PushRoot(Heap::kUndefinedValueRootIndex);
    } else if (locals_count > 1) {
      __ LoadRoot(rdx, Heap::kUndefinedValueRootIndex);
      for (int i = 0; i < locals_count; i++) {
        __ push(rdx);
      }
    }
  }

  bool function_in_register = true;

  // Possibly allocate a local context.
  int heap_slots = info->scope()->num_heap_slots() - Context::MIN_CONTEXT_SLOTS;
  if (heap_slots > 0) {
    Comment cmnt(masm_, "[ Allocate local context");
    // Argument to NewContext is the function, which is still in rdi.
    __ push(rdi);
    if (heap_slots <= FastNewContextStub::kMaximumSlots) {
      FastNewContextStub stub(heap_slots);
      __ CallStub(&stub);
    } else {
      __ CallRuntime(Runtime::kNewFunctionContext, 1);
    }
    function_in_register = false;
    // Context is returned in both rax and rsi.  It replaces the context
    // passed to us.  It's saved in the stack and kept live in rsi.
    __ movq(Operand(rbp, StandardFrameConstants::kContextOffset), rsi);

    // Copy any necessary parameters into the context.
    int num_parameters = info->scope()->num_parameters();
    for (int i = 0; i < num_parameters; i++) {
      Variable* var = scope()->parameter(i);
      if (var->IsContextSlot()) {
        int parameter_offset = StandardFrameConstants::kCallerSPOffset +
            (num_parameters - 1 - i) * kPointerSize;
        // Load parameter from stack.
        __ movq(rax, Operand(rbp, parameter_offset));
        // Store it in the context.
        int context_offset = Context::SlotOffset(var->index());
        __ movq(Operand(rsi, context_offset), rax);
        // Update the write barrier.  This clobbers rax and rbx.
        __ RecordWriteContextSlot(
            rsi, context_offset, rax, rbx, kDontSaveFPRegs);
      }
    }
  }

  // Possibly allocate an arguments object.
  Variable* arguments = scope()->arguments();
  if (arguments != NULL) {
    // Arguments object must be allocated after the context object, in
    // case the "arguments" or ".arguments" variables are in the context.
    Comment cmnt(masm_, "[ Allocate arguments object");
    if (function_in_register) {
      __ push(rdi);
    } else {
      __ push(Operand(rbp, JavaScriptFrameConstants::kFunctionOffset));
    }
    // The receiver is just before the parameters on the caller's stack.
    int num_parameters = info->scope()->num_parameters();
    int offset = num_parameters * kPointerSize;
    __ lea(rdx,
           Operand(rbp, StandardFrameConstants::kCallerSPOffset + offset));
    __ push(rdx);
    __ Push(Smi::FromInt(num_parameters));
    // Arguments to ArgumentsAccessStub:
    //   function, receiver address, parameter count.
    // The stub will rewrite receiver and parameter count if the previous
    // stack frame was an arguments adapter frame.
    ArgumentsAccessStub::Type type;
    if (!is_classic_mode()) {
      type = ArgumentsAccessStub::NEW_STRICT;
    } else if (function()->has_duplicate_parameters()) {
      type = ArgumentsAccessStub::NEW_NON_STRICT_SLOW;
    } else {
      type = ArgumentsAccessStub::NEW_NON_STRICT_FAST;
    }
    ArgumentsAccessStub stub(type);
    __ CallStub(&stub);

    SetVar(arguments, rax, rbx, rdx);
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
        VariableDeclaration* function = scope()->function();
        ASSERT(function->proxy()->var()->mode() == CONST ||
               function->proxy()->var()->mode() == CONST_HARMONY);
        ASSERT(function->proxy()->var()->location() != Variable::UNALLOCATED);
        VisitVariableDeclaration(function);
      }
      VisitDeclarations(scope()->declarations());
    }

    { Comment cmnt(masm_, "[ Stack check");
      PrepareForBailoutForId(AstNode::kDeclarationsId, NO_REGISTERS);
      Label ok;
      __ CompareRoot(rsp, Heap::kStackLimitRootIndex);
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
    __ LoadRoot(rax, Heap::kUndefinedValueRootIndex);
    EmitReturnSequence();
  }
}


void FullCodeGenerator::ClearAccumulator() {
  __ Set(rax, 0);
}


void FullCodeGenerator::EmitProfilingCounterDecrement(int delta) {
  __ movq(rbx, profiling_counter_, RelocInfo::EMBEDDED_OBJECT);
  __ SmiAddConstant(FieldOperand(rbx, JSGlobalPropertyCell::kValueOffset),
                    Smi::FromInt(-delta));
}


void FullCodeGenerator::EmitProfilingCounterReset() {
  int reset_value = FLAG_interrupt_budget;
  if (info_->ShouldSelfOptimize() && !FLAG_retry_self_opt) {
    // Self-optimization is a one-off thing; if it fails, don't try again.
    reset_value = Smi::kMaxValue;
  }
  __ movq(rbx, profiling_counter_, RelocInfo::EMBEDDED_OBJECT);
  __ movq(kScratchRegister,
          reinterpret_cast<uint64_t>(Smi::FromInt(reset_value)),
          RelocInfo::NONE);
  __ movq(FieldOperand(rbx, JSGlobalPropertyCell::kValueOffset),
          kScratchRegister);
}


static const int kMaxBackEdgeWeight = 127;
static const int kBackEdgeDistanceDivisor = 162;


void FullCodeGenerator::EmitStackCheck(IterationStatement* stmt,
                                       Label* back_edge_target) {
  Comment cmnt(masm_, "[ Stack check");
  Label ok;

  if (FLAG_count_based_interrupts) {
    int weight = 1;
    if (FLAG_weighted_back_edges) {
      ASSERT(back_edge_target->is_bound());
      int distance = masm_->SizeOfCodeGeneratedSince(back_edge_target);
      weight = Min(kMaxBackEdgeWeight,
                   Max(1, distance / kBackEdgeDistanceDivisor));
    }
    EmitProfilingCounterDecrement(weight);
    __ j(positive, &ok, Label::kNear);
    InterruptStub stub;
    __ CallStub(&stub);
  } else {
    __ CompareRoot(rsp, Heap::kStackLimitRootIndex);
    __ j(above_equal, &ok, Label::kNear);
    StackCheckStub stub;
    __ CallStub(&stub);
  }

  // Record a mapping of this PC offset to the OSR id.  This is used to find
  // the AST id from the unoptimized code in order to use it as a key into
  // the deoptimization input data found in the optimized code.
  RecordStackCheck(stmt->OsrEntryId());

  // Loop stack checks can be patched to perform on-stack replacement. In
  // order to decide whether or not to perform OSR we embed the loop depth
  // in a test instruction after the call so we can extract it from the OSR
  // builtin.
  ASSERT(loop_depth() > 0);
  __ testl(rax, Immediate(Min(loop_depth(), Code::kMaxLoopNestingMarker)));

  if (FLAG_count_based_interrupts) {
    EmitProfilingCounterReset();
  }

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
    __ bind(&return_label_);
    if (FLAG_trace) {
      __ push(rax);
      __ CallRuntime(Runtime::kTraceExit, 1);
    }
    if (FLAG_interrupt_at_exit || FLAG_self_optimization) {
      // Pretend that the exit is a backwards jump to the entry.
      int weight = 1;
      if (info_->ShouldSelfOptimize()) {
        weight = FLAG_interrupt_budget / FLAG_self_opt_count;
      } else if (FLAG_weighted_back_edges) {
        int distance = masm_->pc_offset();
        weight = Min(kMaxBackEdgeWeight,
                     Max(1, distance = kBackEdgeDistanceDivisor));
      }
      EmitProfilingCounterDecrement(weight);
      Label ok;
      __ j(positive, &ok, Label::kNear);
      __ push(rax);
      if (info_->ShouldSelfOptimize() && FLAG_direct_self_opt) {
        __ push(Operand(rbp, JavaScriptFrameConstants::kFunctionOffset));
        __ CallRuntime(Runtime::kOptimizeFunctionOnNextCall, 1);
      } else {
        InterruptStub stub;
        __ CallStub(&stub);
      }
      __ pop(rax);
      EmitProfilingCounterReset();
      __ bind(&ok);
    }
#ifdef DEBUG
    // Add a label for checking the size of the code used for returning.
    Label check_exit_codesize;
    masm_->bind(&check_exit_codesize);
#endif
    CodeGenerator::RecordPositions(masm_, function()->end_position() - 1);
    __ RecordJSReturn();
    // Do not use the leave instruction here because it is too short to
    // patch with the code required by the debugger.
    __ movq(rsp, rbp);
    __ pop(rbp);

    int arguments_bytes = (info_->scope()->num_parameters() + 1) * kPointerSize;
    __ Ret(arguments_bytes, rcx);

#ifdef ENABLE_DEBUGGER_SUPPORT
    // Add padding that will be overwritten by a debugger breakpoint.  We
    // have just generated at least 7 bytes: "movq rsp, rbp; pop rbp; ret k"
    // (3 + 1 + 3).
    const int kPadding = Assembler::kJSReturnSequenceLength - 7;
    for (int i = 0; i < kPadding; ++i) {
      masm_->int3();
    }
    // Check that the size of the code used for returning is large enough
    // for the debugger's requirements.
    ASSERT(Assembler::kJSReturnSequenceLength <=
           masm_->SizeOfCodeGeneratedSince(&check_exit_codesize));
#endif
  }
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
  __ push(operand);
}


void FullCodeGenerator::TestContext::Plug(Variable* var) const {
  codegen()->GetVar(result_register(), var);
  codegen()->PrepareForBailoutBeforeSplit(condition(), false, NULL, NULL);
  codegen()->DoTest(this);
}


void FullCodeGenerator::EffectContext::Plug(Heap::RootListIndex index) const {
}


void FullCodeGenerator::AccumulatorValueContext::Plug(
    Heap::RootListIndex index) const {
  __ LoadRoot(result_register(), index);
}


void FullCodeGenerator::StackValueContext::Plug(
    Heap::RootListIndex index) const {
  __ PushRoot(index);
}


void FullCodeGenerator::TestContext::Plug(Heap::RootListIndex index) const {
  codegen()->PrepareForBailoutBeforeSplit(condition(),
                                          true,
                                          true_label_,
                                          false_label_);
  if (index == Heap::kUndefinedValueRootIndex ||
      index == Heap::kNullValueRootIndex ||
      index == Heap::kFalseValueRootIndex) {
    if (false_label_ != fall_through_) __ jmp(false_label_);
  } else if (index == Heap::kTrueValueRootIndex) {
    if (true_label_ != fall_through_) __ jmp(true_label_);
  } else {
    __ LoadRoot(result_register(), index);
    codegen()->DoTest(this);
  }
}


void FullCodeGenerator::EffectContext::Plug(Handle<Object> lit) const {
}


void FullCodeGenerator::AccumulatorValueContext::Plug(
    Handle<Object> lit) const {
  __ Move(result_register(), lit);
}


void FullCodeGenerator::StackValueContext::Plug(Handle<Object> lit) const {
  __ Push(lit);
}


void FullCodeGenerator::TestContext::Plug(Handle<Object> lit) const {
  codegen()->PrepareForBailoutBeforeSplit(condition(),
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
    __ Move(result_register(), lit);
    codegen()->DoTest(this);
  }
}


void FullCodeGenerator::EffectContext::DropAndPlug(int count,
                                                   Register reg) const {
  ASSERT(count > 0);
  __ Drop(count);
}


void FullCodeGenerator::AccumulatorValueContext::DropAndPlug(
    int count,
    Register reg) const {
  ASSERT(count > 0);
  __ Drop(count);
  __ Move(result_register(), reg);
}


void FullCodeGenerator::StackValueContext::DropAndPlug(int count,
                                                       Register reg) const {
  ASSERT(count > 0);
  if (count > 1) __ Drop(count - 1);
  __ movq(Operand(rsp, 0), reg);
}


void FullCodeGenerator::TestContext::DropAndPlug(int count,
                                                 Register reg) const {
  ASSERT(count > 0);
  // For simplicity we always test the accumulator register.
  __ Drop(count);
  __ Move(result_register(), reg);
  codegen()->PrepareForBailoutBeforeSplit(condition(), false, NULL, NULL);
  codegen()->DoTest(this);
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
  __ Move(result_register(), isolate()->factory()->true_value());
  __ jmp(&done, Label::kNear);
  __ bind(materialize_false);
  __ Move(result_register(), isolate()->factory()->false_value());
  __ bind(&done);
}


void FullCodeGenerator::StackValueContext::Plug(
    Label* materialize_true,
    Label* materialize_false) const {
  Label done;
  __ bind(materialize_true);
  __ Push(isolate()->factory()->true_value());
  __ jmp(&done, Label::kNear);
  __ bind(materialize_false);
  __ Push(isolate()->factory()->false_value());
  __ bind(&done);
}


void FullCodeGenerator::TestContext::Plug(Label* materialize_true,
                                          Label* materialize_false) const {
  ASSERT(materialize_true == true_label_);
  ASSERT(materialize_false == false_label_);
}


void FullCodeGenerator::EffectContext::Plug(bool flag) const {
}


void FullCodeGenerator::AccumulatorValueContext::Plug(bool flag) const {
  Heap::RootListIndex value_root_index =
      flag ? Heap::kTrueValueRootIndex : Heap::kFalseValueRootIndex;
  __ LoadRoot(result_register(), value_root_index);
}


void FullCodeGenerator::StackValueContext::Plug(bool flag) const {
  Heap::RootListIndex value_root_index =
      flag ? Heap::kTrueValueRootIndex : Heap::kFalseValueRootIndex;
  __ PushRoot(value_root_index);
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
  ToBooleanStub stub(result_register());
  __ push(result_register());
  __ CallStub(&stub, condition->test_id());
  __ testq(result_register(), result_register());
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
  return Operand(rbp, offset);
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
  __ movq(dest, location);
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
  __ movq(location, src);

  // Emit the write barrier code if the location is in the heap.
  if (var->IsContextSlot()) {
    int offset = Context::SlotOffset(var->index());
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
    __ CompareRoot(rax, Heap::kTrueValueRootIndex);
    Split(equal, if_true, if_false, NULL);
    __ bind(&skip);
  }
}


void FullCodeGenerator::EmitDebugCheckDeclarationContext(Variable* variable) {
  // The variable in the declaration always resides in the current function
  // context.
  ASSERT_EQ(0, scope()->ContextChainLength(variable->scope()));
  if (FLAG_debug_code) {
    // Check that we're not inside a with or catch context.
    __ movq(rbx, FieldOperand(rsi, HeapObject::kMapOffset));
    __ CompareRoot(rbx, Heap::kWithContextMapRootIndex);
    __ Check(not_equal, "Declaration in with context.");
    __ CompareRoot(rbx, Heap::kCatchContextMapRootIndex);
    __ Check(not_equal, "Declaration in catch context.");
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
  bool hole_init = mode == CONST || mode == CONST_HARMONY || mode == LET;
  switch (variable->location()) {
    case Variable::UNALLOCATED:
      globals_->Add(variable->name(), zone());
      globals_->Add(variable->binding_needs_init()
                        ? isolate()->factory()->the_hole_value()
                    : isolate()->factory()->undefined_value(),
                    zone());
      break;

    case Variable::PARAMETER:
    case Variable::LOCAL:
      if (hole_init) {
        Comment cmnt(masm_, "[ VariableDeclaration");
        __ LoadRoot(kScratchRegister, Heap::kTheHoleValueRootIndex);
        __ movq(StackOperand(variable), kScratchRegister);
      }
      break;

    case Variable::CONTEXT:
      if (hole_init) {
        Comment cmnt(masm_, "[ VariableDeclaration");
        EmitDebugCheckDeclarationContext(variable);
        __ LoadRoot(kScratchRegister, Heap::kTheHoleValueRootIndex);
        __ movq(ContextOperand(rsi, variable->index()), kScratchRegister);
        // No write barrier since the hole value is in old space.
        PrepareForBailoutForId(proxy->id(), NO_REGISTERS);
      }
      break;

    case Variable::LOOKUP: {
      Comment cmnt(masm_, "[ VariableDeclaration");
      __ push(rsi);
      __ Push(variable->name());
      // Declaration nodes are always introduced in one of four modes.
      ASSERT(mode == VAR || mode == LET ||
             mode == CONST || mode == CONST_HARMONY);
      PropertyAttributes attr =
          (mode == CONST || mode == CONST_HARMONY) ? READ_ONLY : NONE;
      __ Push(Smi::FromInt(attr));
      // Push initial value, if any.
      // Note: For variables we must not push an initial value (such as
      // 'undefined') because we may have a (legal) redeclaration and we
      // must not destroy the current value.
      if (hole_init) {
        __ PushRoot(Heap::kTheHoleValueRootIndex);
      } else {
        __ Push(Smi::FromInt(0));  // Indicates no initial value.
      }
      __ CallRuntime(Runtime::kDeclareContextSlot, 4);
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
          Compiler::BuildFunctionInfo(declaration->fun(), script());
      // Check for stack-overflow exception.
      if (function.is_null()) return SetStackOverflow();
      globals_->Add(function, zone());
      break;
    }

    case Variable::PARAMETER:
    case Variable::LOCAL: {
      Comment cmnt(masm_, "[ FunctionDeclaration");
      VisitForAccumulatorValue(declaration->fun());
      __ movq(StackOperand(variable), result_register());
      break;
    }

    case Variable::CONTEXT: {
      Comment cmnt(masm_, "[ FunctionDeclaration");
      EmitDebugCheckDeclarationContext(variable);
      VisitForAccumulatorValue(declaration->fun());
      __ movq(ContextOperand(rsi, variable->index()), result_register());
      int offset = Context::SlotOffset(variable->index());
      // We know that we have written a function, which is not a smi.
      __ RecordWriteContextSlot(rsi,
                                offset,
                                result_register(),
                                rcx,
                                kDontSaveFPRegs,
                                EMIT_REMEMBERED_SET,
                                OMIT_SMI_CHECK);
      PrepareForBailoutForId(proxy->id(), NO_REGISTERS);
      break;
    }

    case Variable::LOOKUP: {
      Comment cmnt(masm_, "[ FunctionDeclaration");
      __ push(rsi);
      __ Push(variable->name());
      __ Push(Smi::FromInt(NONE));
      VisitForStackValue(declaration->fun());
      __ CallRuntime(Runtime::kDeclareContextSlot, 4);
      break;
    }
  }
}


void FullCodeGenerator::VisitModuleDeclaration(ModuleDeclaration* declaration) {
  VariableProxy* proxy = declaration->proxy();
  Variable* variable = proxy->var();
  Handle<JSModule> instance = declaration->module()->interface()->Instance();
  ASSERT(!instance.is_null());

  switch (variable->location()) {
    case Variable::UNALLOCATED: {
      Comment cmnt(masm_, "[ ModuleDeclaration");
      globals_->Add(variable->name(), zone());
      globals_->Add(instance, zone());
      Visit(declaration->module());
      break;
    }

    case Variable::CONTEXT: {
      Comment cmnt(masm_, "[ ModuleDeclaration");
      EmitDebugCheckDeclarationContext(variable);
      __ Move(ContextOperand(rsi, variable->index()), instance);
      Visit(declaration->module());
      break;
    }

    case Variable::PARAMETER:
    case Variable::LOCAL:
    case Variable::LOOKUP:
      UNREACHABLE();
  }
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
  __ push(rsi);  // The context is the first argument.
  __ Push(pairs);
  __ Push(Smi::FromInt(DeclareGlobalsFlags()));
  __ CallRuntime(Runtime::kDeclareGlobals, 3);
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
    __ movq(rdx, Operand(rsp, 0));  // Switch value.
    bool inline_smi_code = ShouldInlineSmiCase(Token::EQ_STRICT);
    JumpPatchSite patch_site(masm_);
    if (inline_smi_code) {
      Label slow_case;
      __ movq(rcx, rdx);
      __ or_(rcx, rax);
      patch_site.EmitJumpIfNotSmi(rcx, &slow_case, Label::kNear);

      __ cmpq(rdx, rax);
      __ j(not_equal, &next_test);
      __ Drop(1);  // Switch value is no longer needed.
      __ jmp(clause->body_target());
      __ bind(&slow_case);
    }

    // Record position before stub call for type feedback.
    SetSourcePosition(clause->position());
    Handle<Code> ic = CompareIC::GetUninitialized(Token::EQ_STRICT);
    CallIC(ic, RelocInfo::CODE_TARGET, clause->CompareId());
    patch_site.EmitPatchInfo();

    __ testq(rax, rax);
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
  SetStatementPosition(stmt);

  Label loop, exit;
  ForIn loop_statement(this, stmt);
  increment_loop_depth();

  // Get the object to enumerate over. Both SpiderMonkey and JSC
  // ignore null and undefined in contrast to the specification; see
  // ECMA-262 section 12.6.4.
  VisitForAccumulatorValue(stmt->enumerable());
  __ CompareRoot(rax, Heap::kUndefinedValueRootIndex);
  __ j(equal, &exit);
  Register null_value = rdi;
  __ LoadRoot(null_value, Heap::kNullValueRootIndex);
  __ cmpq(rax, null_value);
  __ j(equal, &exit);

  PrepareForBailoutForId(stmt->PrepareId(), TOS_REG);

  // Convert the object to a JS object.
  Label convert, done_convert;
  __ JumpIfSmi(rax, &convert);
  __ CmpObjectType(rax, FIRST_SPEC_OBJECT_TYPE, rcx);
  __ j(above_equal, &done_convert);
  __ bind(&convert);
  __ push(rax);
  __ InvokeBuiltin(Builtins::TO_OBJECT, CALL_FUNCTION);
  __ bind(&done_convert);
  __ push(rax);

  // Check for proxies.
  Label call_runtime;
  STATIC_ASSERT(FIRST_JS_PROXY_TYPE == FIRST_SPEC_OBJECT_TYPE);
  __ CmpObjectType(rax, LAST_JS_PROXY_TYPE, rcx);
  __ j(below_equal, &call_runtime);

  // Check cache validity in generated code. This is a fast case for
  // the JSObject::IsSimpleEnum cache validity checks. If we cannot
  // guarantee cache validity, call the runtime system to check cache
  // validity or get the property names in a fixed array.
  __ CheckEnumCache(null_value, &call_runtime);

  // The enum cache is valid.  Load the map of the object being
  // iterated over and use the cache for the iteration.
  Label use_cache;
  __ movq(rax, FieldOperand(rax, HeapObject::kMapOffset));
  __ jmp(&use_cache, Label::kNear);

  // Get the set of properties to enumerate.
  __ bind(&call_runtime);
  __ push(rax);  // Duplicate the enumerable object on the stack.
  __ CallRuntime(Runtime::kGetPropertyNamesFast, 1);

  // If we got a map from the runtime call, we can do a fast
  // modification check. Otherwise, we got a fixed array, and we have
  // to do a slow check.
  Label fixed_array;
  __ CompareRoot(FieldOperand(rax, HeapObject::kMapOffset),
                 Heap::kMetaMapRootIndex);
  __ j(not_equal, &fixed_array, Label::kNear);

  // We got a map in register rax. Get the enumeration cache from it.
  __ bind(&use_cache);
  __ LoadInstanceDescriptors(rax, rcx);
  __ movq(rcx, FieldOperand(rcx, DescriptorArray::kEnumerationIndexOffset));
  __ movq(rdx, FieldOperand(rcx, DescriptorArray::kEnumCacheBridgeCacheOffset));

  // Set up the four remaining stack slots.
  __ push(rax);  // Map.
  __ push(rdx);  // Enumeration cache.
  __ movq(rax, FieldOperand(rdx, FixedArray::kLengthOffset));
  __ push(rax);  // Enumeration cache length (as smi).
  __ Push(Smi::FromInt(0));  // Initial index.
  __ jmp(&loop);

  // We got a fixed array in register rax. Iterate through that.
  Label non_proxy;
  __ bind(&fixed_array);

  Handle<JSGlobalPropertyCell> cell =
      isolate()->factory()->NewJSGlobalPropertyCell(
          Handle<Object>(
              Smi::FromInt(TypeFeedbackCells::kForInFastCaseMarker)));
  RecordTypeFeedbackCell(stmt->PrepareId(), cell);
  __ LoadHeapObject(rbx, cell);
  __ Move(FieldOperand(rbx, JSGlobalPropertyCell::kValueOffset),
          Smi::FromInt(TypeFeedbackCells::kForInSlowCaseMarker));

  __ Move(rbx, Smi::FromInt(1));  // Smi indicates slow check
  __ movq(rcx, Operand(rsp, 0 * kPointerSize));  // Get enumerated object
  STATIC_ASSERT(FIRST_JS_PROXY_TYPE == FIRST_SPEC_OBJECT_TYPE);
  __ CmpObjectType(rcx, LAST_JS_PROXY_TYPE, rcx);
  __ j(above, &non_proxy);
  __ Move(rbx, Smi::FromInt(0));  // Zero indicates proxy
  __ bind(&non_proxy);
  __ push(rbx);  // Smi
  __ push(rax);  // Array
  __ movq(rax, FieldOperand(rax, FixedArray::kLengthOffset));
  __ push(rax);  // Fixed array length (as smi).
  __ Push(Smi::FromInt(0));  // Initial index.

  // Generate code for doing the condition check.
  PrepareForBailoutForId(stmt->BodyId(), NO_REGISTERS);
  __ bind(&loop);
  __ movq(rax, Operand(rsp, 0 * kPointerSize));  // Get the current index.
  __ cmpq(rax, Operand(rsp, 1 * kPointerSize));  // Compare to the array length.
  __ j(above_equal, loop_statement.break_label());

  // Get the current entry of the array into register rbx.
  __ movq(rbx, Operand(rsp, 2 * kPointerSize));
  SmiIndex index = masm()->SmiToIndex(rax, rax, kPointerSizeLog2);
  __ movq(rbx, FieldOperand(rbx,
                            index.reg,
                            index.scale,
                            FixedArray::kHeaderSize));

  // Get the expected map from the stack or a smi in the
  // permanent slow case into register rdx.
  __ movq(rdx, Operand(rsp, 3 * kPointerSize));

  // Check if the expected map still matches that of the enumerable.
  // If not, we may have to filter the key.
  Label update_each;
  __ movq(rcx, Operand(rsp, 4 * kPointerSize));
  __ cmpq(rdx, FieldOperand(rcx, HeapObject::kMapOffset));
  __ j(equal, &update_each, Label::kNear);

  // For proxies, no filtering is done.
  // TODO(rossberg): What if only a prototype is a proxy? Not specified yet.
  __ Cmp(rdx, Smi::FromInt(0));
  __ j(equal, &update_each, Label::kNear);

  // Convert the entry to a string or null if it isn't a property
  // anymore. If the property has been removed while iterating, we
  // just skip it.
  __ push(rcx);  // Enumerable.
  __ push(rbx);  // Current entry.
  __ InvokeBuiltin(Builtins::FILTER_KEY, CALL_FUNCTION);
  __ Cmp(rax, Smi::FromInt(0));
  __ j(equal, loop_statement.continue_label());
  __ movq(rbx, rax);

  // Update the 'each' property or variable from the possibly filtered
  // entry in register rbx.
  __ bind(&update_each);
  __ movq(result_register(), rbx);
  // Perform the assignment as if via '='.
  { EffectContext context(this);
    EmitAssignment(stmt->each());
  }

  // Generate code for the body of the loop.
  Visit(stmt->body());

  // Generate code for going to the next element by incrementing the
  // index (smi) stored on top of the stack.
  __ bind(loop_statement.continue_label());
  __ SmiAddConstant(Operand(rsp, 0 * kPointerSize), Smi::FromInt(1));

  EmitStackCheck(stmt, &loop);
  __ jmp(&loop);

  // Remove the pointers stored on the stack.
  __ bind(loop_statement.break_label());
  __ addq(rsp, Immediate(5 * kPointerSize));

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
    FastNewClosureStub stub(info->language_mode());
    __ Push(info);
    __ CallStub(&stub);
  } else {
    __ push(rsi);
    __ Push(info);
    __ Push(pretenure
            ? isolate()->factory()->true_value()
            : isolate()->factory()->false_value());
    __ CallRuntime(Runtime::kNewClosure, 3);
  }
  context()->Plug(rax);
}


void FullCodeGenerator::VisitVariableProxy(VariableProxy* expr) {
  Comment cmnt(masm_, "[ VariableProxy");
  EmitVariableLoad(expr);
}


void FullCodeGenerator::EmitLoadGlobalCheckExtensions(Variable* var,
                                                      TypeofState typeof_state,
                                                      Label* slow) {
  Register context = rsi;
  Register temp = rdx;

  Scope* s = scope();
  while (s != NULL) {
    if (s->num_heap_slots() > 0) {
      if (s->calls_non_strict_eval()) {
        // Check that extension is NULL.
        __ cmpq(ContextOperand(context, Context::EXTENSION_INDEX),
                Immediate(0));
        __ j(not_equal, slow);
      }
      // Load next context in chain.
      __ movq(temp, ContextOperand(context, Context::PREVIOUS_INDEX));
      // Walk the rest of the chain without clobbering rsi.
      context = temp;
    }
    // If no outer scope calls eval, we do not need to check more
    // context extensions.  If we have reached an eval scope, we check
    // all extensions from this point.
    if (!s->outer_scope_calls_non_strict_eval() || s->is_eval_scope()) break;
    s = s->outer_scope();
  }

  if (s != NULL && s->is_eval_scope()) {
    // Loop up the context chain.  There is no frame effect so it is
    // safe to use raw labels here.
    Label next, fast;
    if (!context.is(temp)) {
      __ movq(temp, context);
    }
    // Load map for comparison into register, outside loop.
    __ LoadRoot(kScratchRegister, Heap::kGlobalContextMapRootIndex);
    __ bind(&next);
    // Terminate at global context.
    __ cmpq(kScratchRegister, FieldOperand(temp, HeapObject::kMapOffset));
    __ j(equal, &fast, Label::kNear);
    // Check that extension is NULL.
    __ cmpq(ContextOperand(temp, Context::EXTENSION_INDEX), Immediate(0));
    __ j(not_equal, slow);
    // Load next context in chain.
    __ movq(temp, ContextOperand(temp, Context::PREVIOUS_INDEX));
    __ jmp(&next);
    __ bind(&fast);
  }

  // All extension objects were empty and it is safe to use a global
  // load IC call.
  __ movq(rax, GlobalObjectOperand());
  __ Move(rcx, var->name());
  Handle<Code> ic = isolate()->builtins()->LoadIC_Initialize();
  RelocInfo::Mode mode = (typeof_state == INSIDE_TYPEOF)
      ? RelocInfo::CODE_TARGET
      : RelocInfo::CODE_TARGET_CONTEXT;
  CallIC(ic, mode);
}


MemOperand FullCodeGenerator::ContextSlotOperandCheckExtensions(Variable* var,
                                                                Label* slow) {
  ASSERT(var->IsContextSlot());
  Register context = rsi;
  Register temp = rbx;

  for (Scope* s = scope(); s != var->scope(); s = s->outer_scope()) {
    if (s->num_heap_slots() > 0) {
      if (s->calls_non_strict_eval()) {
        // Check that extension is NULL.
        __ cmpq(ContextOperand(context, Context::EXTENSION_INDEX),
                Immediate(0));
        __ j(not_equal, slow);
      }
      __ movq(temp, ContextOperand(context, Context::PREVIOUS_INDEX));
      // Walk the rest of the chain without clobbering rsi.
      context = temp;
    }
  }
  // Check that last extension is NULL.
  __ cmpq(ContextOperand(context, Context::EXTENSION_INDEX), Immediate(0));
  __ j(not_equal, slow);

  // This function is used only for loads, not stores, so it's safe to
  // return an rsi-based operand (the write barrier cannot be allowed to
  // destroy the rsi register).
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
  if (var->mode() == DYNAMIC_GLOBAL) {
    EmitLoadGlobalCheckExtensions(var, typeof_state, slow);
    __ jmp(done);
  } else if (var->mode() == DYNAMIC_LOCAL) {
    Variable* local = var->local_if_not_shadowed();
    __ movq(rax, ContextSlotOperandCheckExtensions(local, slow));
    if (local->mode() == CONST ||
        local->mode() == CONST_HARMONY ||
        local->mode() == LET) {
      __ CompareRoot(rax, Heap::kTheHoleValueRootIndex);
      __ j(not_equal, done);
      if (local->mode() == CONST) {
        __ LoadRoot(rax, Heap::kUndefinedValueRootIndex);
      } else {  // LET || CONST_HARMONY
        __ Push(var->name());
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
      Comment cmnt(masm_, "Global variable");
      // Use inline caching. Variable name is passed in rcx and the global
      // object on the stack.
      __ Move(rcx, var->name());
      __ movq(rax, GlobalObjectOperand());
      Handle<Code> ic = isolate()->builtins()->LoadIC_Initialize();
      CallIC(ic, RelocInfo::CODE_TARGET_CONTEXT);
      context()->Plug(rax);
      break;
    }

    case Variable::PARAMETER:
    case Variable::LOCAL:
    case Variable::CONTEXT: {
      Comment cmnt(masm_, var->IsContextSlot() ? "Context slot" : "Stack slot");
      if (var->binding_needs_init()) {
        // var->scope() may be NULL when the proxy is located in eval code and
        // refers to a potential outside binding. Currently those bindings are
        // always looked up dynamically, i.e. in that case
        //     var->location() == LOOKUP.
        // always holds.
        ASSERT(var->scope() != NULL);

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
          ASSERT(var->initializer_position() != RelocInfo::kNoPosition);
          ASSERT(proxy->position() != RelocInfo::kNoPosition);
          skip_init_check = var->mode() != CONST &&
              var->initializer_position() < proxy->position();
        }

        if (!skip_init_check) {
          // Let and const need a read barrier.
          Label done;
          GetVar(rax, var);
          __ CompareRoot(rax, Heap::kTheHoleValueRootIndex);
          __ j(not_equal, &done, Label::kNear);
          if (var->mode() == LET || var->mode() == CONST_HARMONY) {
            // Throw a reference error when using an uninitialized let/const
            // binding in harmony mode.
            __ Push(var->name());
            __ CallRuntime(Runtime::kThrowReferenceError, 1);
          } else {
            // Uninitalized const bindings outside of harmony mode are unholed.
            ASSERT(var->mode() == CONST);
            __ LoadRoot(rax, Heap::kUndefinedValueRootIndex);
          }
          __ bind(&done);
          context()->Plug(rax);
          break;
        }
      }
      context()->Plug(var);
      break;
    }

    case Variable::LOOKUP: {
      Label done, slow;
      // Generate code for loading from variables potentially shadowed
      // by eval-introduced variables.
      EmitDynamicLookupFastCase(var, NOT_INSIDE_TYPEOF, &slow, &done);
      __ bind(&slow);
      Comment cmnt(masm_, "Lookup slot");
      __ push(rsi);  // Context.
      __ Push(var->name());
      __ CallRuntime(Runtime::kLoadContextSlot, 2);
      __ bind(&done);
      context()->Plug(rax);
      break;
    }
  }
}


void FullCodeGenerator::VisitRegExpLiteral(RegExpLiteral* expr) {
  Comment cmnt(masm_, "[ RegExpLiteral");
  Label materialized;
  // Registers will be used as follows:
  // rdi = JS function.
  // rcx = literals array.
  // rbx = regexp literal.
  // rax = regexp literal clone.
  __ movq(rdi, Operand(rbp, JavaScriptFrameConstants::kFunctionOffset));
  __ movq(rcx, FieldOperand(rdi, JSFunction::kLiteralsOffset));
  int literal_offset =
      FixedArray::kHeaderSize + expr->literal_index() * kPointerSize;
  __ movq(rbx, FieldOperand(rcx, literal_offset));
  __ CompareRoot(rbx, Heap::kUndefinedValueRootIndex);
  __ j(not_equal, &materialized, Label::kNear);

  // Create regexp literal using runtime function
  // Result will be in rax.
  __ push(rcx);
  __ Push(Smi::FromInt(expr->literal_index()));
  __ Push(expr->pattern());
  __ Push(expr->flags());
  __ CallRuntime(Runtime::kMaterializeRegExpLiteral, 4);
  __ movq(rbx, rax);

  __ bind(&materialized);
  int size = JSRegExp::kSize + JSRegExp::kInObjectFieldCount * kPointerSize;
  Label allocated, runtime_allocate;
  __ AllocateInNewSpace(size, rax, rcx, rdx, &runtime_allocate, TAG_OBJECT);
  __ jmp(&allocated);

  __ bind(&runtime_allocate);
  __ push(rbx);
  __ Push(Smi::FromInt(size));
  __ CallRuntime(Runtime::kAllocateInNewSpace, 1);
  __ pop(rbx);

  __ bind(&allocated);
  // Copy the content into the newly allocated memory.
  // (Unroll copy loop once for better throughput).
  for (int i = 0; i < size - kPointerSize; i += 2 * kPointerSize) {
    __ movq(rdx, FieldOperand(rbx, i));
    __ movq(rcx, FieldOperand(rbx, i + kPointerSize));
    __ movq(FieldOperand(rax, i), rdx);
    __ movq(FieldOperand(rax, i + kPointerSize), rcx);
  }
  if ((size % (2 * kPointerSize)) != 0) {
    __ movq(rdx, FieldOperand(rbx, size - kPointerSize));
    __ movq(FieldOperand(rax, size - kPointerSize), rdx);
  }
  context()->Plug(rax);
}


void FullCodeGenerator::EmitAccessor(Expression* expression) {
  if (expression == NULL) {
    __ PushRoot(Heap::kNullValueRootIndex);
  } else {
    VisitForStackValue(expression);
  }
}


void FullCodeGenerator::VisitObjectLiteral(ObjectLiteral* expr) {
  Comment cmnt(masm_, "[ ObjectLiteral");
  Handle<FixedArray> constant_properties = expr->constant_properties();
  __ movq(rdi, Operand(rbp, JavaScriptFrameConstants::kFunctionOffset));
  __ push(FieldOperand(rdi, JSFunction::kLiteralsOffset));
  __ Push(Smi::FromInt(expr->literal_index()));
  __ Push(constant_properties);
  int flags = expr->fast_elements()
      ? ObjectLiteral::kFastElements
      : ObjectLiteral::kNoFlags;
  flags |= expr->has_function()
      ? ObjectLiteral::kHasFunction
      : ObjectLiteral::kNoFlags;
  __ Push(Smi::FromInt(flags));
  int properties_count = constant_properties->length() / 2;
  if (expr->depth() > 1) {
    __ CallRuntime(Runtime::kCreateObjectLiteral, 4);
  } else if (flags != ObjectLiteral::kFastElements ||
      properties_count > FastCloneShallowObjectStub::kMaximumClonedProperties) {
    __ CallRuntime(Runtime::kCreateObjectLiteralShallow, 4);
  } else {
    FastCloneShallowObjectStub stub(properties_count);
    __ CallStub(&stub);
  }

  // If result_saved is true the result is on top of the stack.  If
  // result_saved is false the result is in rax.
  bool result_saved = false;

  // Mark all computed expressions that are bound to a key that
  // is shadowed by a later occurrence of the same key. For the
  // marked expressions, no store code is emitted.
  expr->CalculateEmitStore(zone());

  AccessorTable accessor_table(isolate()->zone());
  for (int i = 0; i < expr->properties()->length(); i++) {
    ObjectLiteral::Property* property = expr->properties()->at(i);
    if (property->IsCompileTimeValue()) continue;

    Literal* key = property->key();
    Expression* value = property->value();
    if (!result_saved) {
      __ push(rax);  // Save result on the stack
      result_saved = true;
    }
    switch (property->kind()) {
      case ObjectLiteral::Property::CONSTANT:
        UNREACHABLE();
      case ObjectLiteral::Property::MATERIALIZED_LITERAL:
        ASSERT(!CompileTimeValue::IsCompileTimeValue(value));
        // Fall through.
      case ObjectLiteral::Property::COMPUTED:
        if (key->handle()->IsSymbol()) {
          if (property->emit_store()) {
            VisitForAccumulatorValue(value);
            __ Move(rcx, key->handle());
            __ movq(rdx, Operand(rsp, 0));
            Handle<Code> ic = is_classic_mode()
                ? isolate()->builtins()->StoreIC_Initialize()
                : isolate()->builtins()->StoreIC_Initialize_Strict();
            CallIC(ic, RelocInfo::CODE_TARGET, key->id());
            PrepareForBailoutForId(key->id(), NO_REGISTERS);
          } else {
            VisitForEffect(value);
          }
          break;
        }
        // Fall through.
      case ObjectLiteral::Property::PROTOTYPE:
        __ push(Operand(rsp, 0));  // Duplicate receiver.
        VisitForStackValue(key);
        VisitForStackValue(value);
        if (property->emit_store()) {
          __ Push(Smi::FromInt(NONE));    // PropertyAttributes
          __ CallRuntime(Runtime::kSetProperty, 4);
        } else {
          __ Drop(3);
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
    __ push(Operand(rsp, 0));  // Duplicate receiver.
    VisitForStackValue(it->first);
    EmitAccessor(it->second->getter);
    EmitAccessor(it->second->setter);
    __ Push(Smi::FromInt(NONE));
    __ CallRuntime(Runtime::kDefineOrRedefineAccessorProperty, 5);
  }

  if (expr->has_function()) {
    ASSERT(result_saved);
    __ push(Operand(rsp, 0));
    __ CallRuntime(Runtime::kToFastProperties, 1);
  }

  if (result_saved) {
    context()->PlugTOS();
  } else {
    context()->Plug(rax);
  }
}


void FullCodeGenerator::VisitArrayLiteral(ArrayLiteral* expr) {
  Comment cmnt(masm_, "[ ArrayLiteral");

  ZoneList<Expression*>* subexprs = expr->values();
  int length = subexprs->length();
  Handle<FixedArray> constant_elements = expr->constant_elements();
  ASSERT_EQ(2, constant_elements->length());
  ElementsKind constant_elements_kind =
      static_cast<ElementsKind>(Smi::cast(constant_elements->get(0))->value());
  bool has_constant_fast_elements =
      IsFastObjectElementsKind(constant_elements_kind);
  Handle<FixedArrayBase> constant_elements_values(
      FixedArrayBase::cast(constant_elements->get(1)));

  __ movq(rbx, Operand(rbp, JavaScriptFrameConstants::kFunctionOffset));
  __ push(FieldOperand(rbx, JSFunction::kLiteralsOffset));
  __ Push(Smi::FromInt(expr->literal_index()));
  __ Push(constant_elements);
  Heap* heap = isolate()->heap();
  if (has_constant_fast_elements &&
      constant_elements_values->map() == heap->fixed_cow_array_map()) {
    // If the elements are already FAST_*_ELEMENTS, the boilerplate cannot
    // change, so it's possible to specialize the stub in advance.
    __ IncrementCounter(isolate()->counters()->cow_arrays_created_stub(), 1);
    FastCloneShallowArrayStub stub(
        FastCloneShallowArrayStub::COPY_ON_WRITE_ELEMENTS,
        length);
    __ CallStub(&stub);
  } else if (expr->depth() > 1) {
    __ CallRuntime(Runtime::kCreateArrayLiteral, 3);
  } else if (length > FastCloneShallowArrayStub::kMaximumClonedLength) {
    __ CallRuntime(Runtime::kCreateArrayLiteralShallow, 3);
  } else {
    ASSERT(IsFastSmiOrObjectElementsKind(constant_elements_kind) ||
           FLAG_smi_only_arrays);
    // If the elements are already FAST_*_ELEMENTS, the boilerplate cannot
    // change, so it's possible to specialize the stub in advance.
    FastCloneShallowArrayStub::Mode mode = has_constant_fast_elements
        ? FastCloneShallowArrayStub::CLONE_ELEMENTS
        : FastCloneShallowArrayStub::CLONE_ANY_ELEMENTS;
    FastCloneShallowArrayStub stub(mode, length);
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
      __ push(rax);
      result_saved = true;
    }
    VisitForAccumulatorValue(subexpr);

    if (IsFastObjectElementsKind(constant_elements_kind)) {
      // Fast-case array literal with ElementsKind of FAST_*_ELEMENTS, they
      // cannot transition and don't need to call the runtime stub.
      int offset = FixedArray::kHeaderSize + (i * kPointerSize);
      __ movq(rbx, Operand(rsp, 0));  // Copy of array literal.
      __ movq(rbx, FieldOperand(rbx, JSObject::kElementsOffset));
      // Store the subexpression value in the array's elements.
      __ movq(FieldOperand(rbx, offset), result_register());
      // Update the write barrier for the array store.
      __ RecordWriteField(rbx, offset, result_register(), rcx,
                          kDontSaveFPRegs,
                          EMIT_REMEMBERED_SET,
                          INLINE_SMI_CHECK);
    } else {
      // Store the subexpression value in the array's elements.
      __ movq(rbx, Operand(rsp, 0));  // Copy of array literal.
      __ movq(rdi, FieldOperand(rbx, JSObject::kMapOffset));
      __ Move(rcx, Smi::FromInt(i));
      __ Move(rdx, Smi::FromInt(expr->literal_index()));
      StoreArrayLiteralElementStub stub;
      __ CallStub(&stub);
    }

    PrepareForBailoutForId(expr->GetIdForElement(i), NO_REGISTERS);
  }

  if (result_saved) {
    context()->PlugTOS();
  } else {
    context()->Plug(rax);
  }
}


void FullCodeGenerator::VisitAssignment(Assignment* expr) {
  Comment cmnt(masm_, "[ Assignment");
  // Invalid left-hand sides are rewritten to have a 'throw ReferenceError'
  // on the left-hand side.
  if (!expr->target()->IsValidLeftHandSide()) {
    VisitForEffect(expr->target());
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
      } else {
        VisitForStackValue(property->obj());
      }
      break;
    case KEYED_PROPERTY: {
      if (expr->is_compound()) {
        VisitForStackValue(property->obj());
        VisitForAccumulatorValue(property->key());
        __ movq(rdx, Operand(rsp, 0));
        __ push(rax);
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
    { AccumulatorValueContext context(this);
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
    __ push(rax);  // Left operand goes on the stack.
    VisitForAccumulatorValue(expr->value());

    OverwriteMode mode = expr->value()->ResultOverwriteAllowed()
        ? OVERWRITE_RIGHT
        : NO_OVERWRITE;
    SetSourcePosition(expr->position() + 1);
    AccumulatorValueContext context(this);
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
      context()->Plug(rax);
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
  __ Move(rcx, key->handle());
  Handle<Code> ic = isolate()->builtins()->LoadIC_Initialize();
  CallIC(ic, RelocInfo::CODE_TARGET, prop->id());
}


void FullCodeGenerator::EmitKeyedPropertyLoad(Property* prop) {
  SetSourcePosition(prop->position());
  Handle<Code> ic = isolate()->builtins()->KeyedLoadIC_Initialize();
  CallIC(ic, RelocInfo::CODE_TARGET, prop->id());
}


void FullCodeGenerator::EmitInlineSmiBinaryOp(BinaryOperation* expr,
                                              Token::Value op,
                                              OverwriteMode mode,
                                              Expression* left,
                                              Expression* right) {
  // Do combined smi check of the operands. Left operand is on the
  // stack (popped into rdx). Right operand is in rax but moved into
  // rcx to make the shifts easier.
  Label done, stub_call, smi_case;
  __ pop(rdx);
  __ movq(rcx, rax);
  __ or_(rax, rdx);
  JumpPatchSite patch_site(masm_);
  patch_site.EmitJumpIfSmi(rax, &smi_case, Label::kNear);

  __ bind(&stub_call);
  __ movq(rax, rcx);
  BinaryOpStub stub(op, mode);
  CallIC(stub.GetCode(), RelocInfo::CODE_TARGET, expr->id());
  patch_site.EmitPatchInfo();
  __ jmp(&done, Label::kNear);

  __ bind(&smi_case);
  switch (op) {
    case Token::SAR:
      __ SmiShiftArithmeticRight(rax, rdx, rcx);
      break;
    case Token::SHL:
      __ SmiShiftLeft(rax, rdx, rcx);
      break;
    case Token::SHR:
      __ SmiShiftLogicalRight(rax, rdx, rcx, &stub_call);
      break;
    case Token::ADD:
      __ SmiAdd(rax, rdx, rcx, &stub_call);
      break;
    case Token::SUB:
      __ SmiSub(rax, rdx, rcx, &stub_call);
      break;
    case Token::MUL:
      __ SmiMul(rax, rdx, rcx, &stub_call);
      break;
    case Token::BIT_OR:
      __ SmiOr(rax, rdx, rcx);
      break;
    case Token::BIT_AND:
      __ SmiAnd(rax, rdx, rcx);
      break;
    case Token::BIT_XOR:
      __ SmiXor(rax, rdx, rcx);
      break;
    default:
      UNREACHABLE();
      break;
  }

  __ bind(&done);
  context()->Plug(rax);
}


void FullCodeGenerator::EmitBinaryOp(BinaryOperation* expr,
                                     Token::Value op,
                                     OverwriteMode mode) {
  __ pop(rdx);
  BinaryOpStub stub(op, mode);
  JumpPatchSite patch_site(masm_);    // unbound, signals no inlined smi code.
  CallIC(stub.GetCode(), RelocInfo::CODE_TARGET, expr->id());
  patch_site.EmitPatchInfo();
  context()->Plug(rax);
}


void FullCodeGenerator::EmitAssignment(Expression* expr) {
  // Invalid left-hand sides are rewritten to have a 'throw
  // ReferenceError' on the left-hand side.
  if (!expr->IsValidLeftHandSide()) {
    VisitForEffect(expr);
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
      __ push(rax);  // Preserve value.
      VisitForAccumulatorValue(prop->obj());
      __ movq(rdx, rax);
      __ pop(rax);  // Restore value.
      __ Move(rcx, prop->key()->AsLiteral()->handle());
      Handle<Code> ic = is_classic_mode()
          ? isolate()->builtins()->StoreIC_Initialize()
          : isolate()->builtins()->StoreIC_Initialize_Strict();
      CallIC(ic);
      break;
    }
    case KEYED_PROPERTY: {
      __ push(rax);  // Preserve value.
      VisitForStackValue(prop->obj());
      VisitForAccumulatorValue(prop->key());
      __ movq(rcx, rax);
      __ pop(rdx);
      __ pop(rax);  // Restore value.
      Handle<Code> ic = is_classic_mode()
          ? isolate()->builtins()->KeyedStoreIC_Initialize()
          : isolate()->builtins()->KeyedStoreIC_Initialize_Strict();
      CallIC(ic);
      break;
    }
  }
  context()->Plug(rax);
}


void FullCodeGenerator::EmitVariableAssignment(Variable* var,
                                               Token::Value op) {
  if (var->IsUnallocated()) {
    // Global var, const, or let.
    __ Move(rcx, var->name());
    __ movq(rdx, GlobalObjectOperand());
    Handle<Code> ic = is_classic_mode()
        ? isolate()->builtins()->StoreIC_Initialize()
        : isolate()->builtins()->StoreIC_Initialize_Strict();
    CallIC(ic, RelocInfo::CODE_TARGET_CONTEXT);
  } else if (op == Token::INIT_CONST) {
    // Const initializers need a write barrier.
    ASSERT(!var->IsParameter());  // No const parameters.
    if (var->IsStackLocal()) {
      Label skip;
      __ movq(rdx, StackOperand(var));
      __ CompareRoot(rdx, Heap::kTheHoleValueRootIndex);
      __ j(not_equal, &skip);
      __ movq(StackOperand(var), rax);
      __ bind(&skip);
    } else {
      ASSERT(var->IsContextSlot() || var->IsLookupSlot());
      // Like var declarations, const declarations are hoisted to function
      // scope.  However, unlike var initializers, const initializers are
      // able to drill a hole to that function context, even from inside a
      // 'with' context.  We thus bypass the normal static scope lookup for
      // var->IsContextSlot().
      __ push(rax);
      __ push(rsi);
      __ Push(var->name());
      __ CallRuntime(Runtime::kInitializeConstContextSlot, 3);
    }

  } else if (var->mode() == LET && op != Token::INIT_LET) {
    // Non-initializing assignment to let variable needs a write barrier.
    if (var->IsLookupSlot()) {
      __ push(rax);  // Value.
      __ push(rsi);  // Context.
      __ Push(var->name());
      __ Push(Smi::FromInt(language_mode()));
      __ CallRuntime(Runtime::kStoreContextSlot, 4);
    } else {
      ASSERT(var->IsStackAllocated() || var->IsContextSlot());
      Label assign;
      MemOperand location = VarOperand(var, rcx);
      __ movq(rdx, location);
      __ CompareRoot(rdx, Heap::kTheHoleValueRootIndex);
      __ j(not_equal, &assign, Label::kNear);
      __ Push(var->name());
      __ CallRuntime(Runtime::kThrowReferenceError, 1);
      __ bind(&assign);
      __ movq(location, rax);
      if (var->IsContextSlot()) {
        __ movq(rdx, rax);
        __ RecordWriteContextSlot(
            rcx, Context::SlotOffset(var->index()), rdx, rbx, kDontSaveFPRegs);
      }
    }

  } else if (!var->is_const_mode() || op == Token::INIT_CONST_HARMONY) {
    // Assignment to var or initializing assignment to let/const
    // in harmony mode.
    if (var->IsStackAllocated() || var->IsContextSlot()) {
      MemOperand location = VarOperand(var, rcx);
      if (FLAG_debug_code && op == Token::INIT_LET) {
        // Check for an uninitialized let binding.
        __ movq(rdx, location);
        __ CompareRoot(rdx, Heap::kTheHoleValueRootIndex);
        __ Check(equal, "Let binding re-initialization.");
      }
      // Perform the assignment.
      __ movq(location, rax);
      if (var->IsContextSlot()) {
        __ movq(rdx, rax);
        __ RecordWriteContextSlot(
            rcx, Context::SlotOffset(var->index()), rdx, rbx, kDontSaveFPRegs);
      }
    } else {
      ASSERT(var->IsLookupSlot());
      __ push(rax);  // Value.
      __ push(rsi);  // Context.
      __ Push(var->name());
      __ Push(Smi::FromInt(language_mode()));
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
    __ push(Operand(rsp, kPointerSize));  // Receiver is now under value.
    __ CallRuntime(Runtime::kToSlowProperties, 1);
    __ pop(result_register());
  }

  // Record source code position before IC call.
  SetSourcePosition(expr->position());
  __ Move(rcx, prop->key()->AsLiteral()->handle());
  if (expr->ends_initialization_block()) {
    __ movq(rdx, Operand(rsp, 0));
  } else {
    __ pop(rdx);
  }
  Handle<Code> ic = is_classic_mode()
      ? isolate()->builtins()->StoreIC_Initialize()
      : isolate()->builtins()->StoreIC_Initialize_Strict();
  CallIC(ic, RelocInfo::CODE_TARGET, expr->id());

  // If the assignment ends an initialization block, revert to fast case.
  if (expr->ends_initialization_block()) {
    __ push(rax);  // Result of assignment, saved even if not needed.
    __ push(Operand(rsp, kPointerSize));  // Receiver is under value.
    __ CallRuntime(Runtime::kToFastProperties, 1);
    __ pop(rax);
    __ Drop(1);
  }
  PrepareForBailoutForId(expr->AssignmentId(), TOS_REG);
  context()->Plug(rax);
}


void FullCodeGenerator::EmitKeyedPropertyAssignment(Assignment* expr) {
  // Assignment to a property, using a keyed store IC.

  // If the assignment starts a block of assignments to the same object,
  // change to slow case to avoid the quadratic behavior of repeatedly
  // adding fast properties.
  if (expr->starts_initialization_block()) {
    __ push(result_register());
    // Receiver is now under the key and value.
    __ push(Operand(rsp, 2 * kPointerSize));
    __ CallRuntime(Runtime::kToSlowProperties, 1);
    __ pop(result_register());
  }

  __ pop(rcx);
  if (expr->ends_initialization_block()) {
    __ movq(rdx, Operand(rsp, 0));  // Leave receiver on the stack for later.
  } else {
    __ pop(rdx);
  }
  // Record source code position before IC call.
  SetSourcePosition(expr->position());
  Handle<Code> ic = is_classic_mode()
      ? isolate()->builtins()->KeyedStoreIC_Initialize()
      : isolate()->builtins()->KeyedStoreIC_Initialize_Strict();
  CallIC(ic, RelocInfo::CODE_TARGET, expr->id());

  // If the assignment ends an initialization block, revert to fast case.
  if (expr->ends_initialization_block()) {
    __ pop(rdx);
    __ push(rax);  // Result of assignment, saved even if not needed.
    __ push(rdx);
    __ CallRuntime(Runtime::kToFastProperties, 1);
    __ pop(rax);
  }

  PrepareForBailoutForId(expr->AssignmentId(), TOS_REG);
  context()->Plug(rax);
}


void FullCodeGenerator::VisitProperty(Property* expr) {
  Comment cmnt(masm_, "[ Property");
  Expression* key = expr->key();

  if (key->IsPropertyName()) {
    VisitForAccumulatorValue(expr->obj());
    EmitNamedPropertyLoad(expr);
    context()->Plug(rax);
  } else {
    VisitForStackValue(expr->obj());
    VisitForAccumulatorValue(expr->key());
    __ pop(rdx);
    EmitKeyedPropertyLoad(expr);
    context()->Plug(rax);
  }
}


void FullCodeGenerator::CallIC(Handle<Code> code,
                               RelocInfo::Mode rmode,
                               unsigned ast_id) {
  ic_total_count_++;
  __ call(code, rmode, ast_id);
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
    __ Move(rcx, name);
  }
  // Record source position for debugger.
  SetSourcePosition(expr->position());
  // Call the IC initialization code.
  Handle<Code> ic =
      isolate()->stub_cache()->ComputeCallInitialize(arg_count, mode);
  CallIC(ic, mode, expr->id());
  RecordJSReturnSite(expr);
  // Restore context register.
  __ movq(rsi, Operand(rbp, StandardFrameConstants::kContextOffset));
  context()->Plug(rax);
}


void FullCodeGenerator::EmitKeyedCallWithIC(Call* expr,
                                            Expression* key) {
  // Load the key.
  VisitForAccumulatorValue(key);

  // Swap the name of the function and the receiver on the stack to follow
  // the calling convention for call ICs.
  __ pop(rcx);
  __ push(rax);
  __ push(rcx);

  // Load the arguments.
  ZoneList<Expression*>* args = expr->arguments();
  int arg_count = args->length();
  { PreservePositionScope scope(masm()->positions_recorder());
    for (int i = 0; i < arg_count; i++) {
      VisitForStackValue(args->at(i));
    }
  }
  // Record source position for debugger.
  SetSourcePosition(expr->position());
  // Call the IC initialization code.
  Handle<Code> ic =
      isolate()->stub_cache()->ComputeKeyedCallInitialize(arg_count);
  __ movq(rcx, Operand(rsp, (arg_count + 1) * kPointerSize));  // Key.
  CallIC(ic, RelocInfo::CODE_TARGET, expr->id());
  RecordJSReturnSite(expr);
  // Restore context register.
  __ movq(rsi, Operand(rbp, StandardFrameConstants::kContextOffset));
  context()->DropAndPlug(1, rax);  // Drop the key still on the stack.
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

  // Record call targets in unoptimized code, but not in the snapshot.
  if (!Serializer::enabled()) {
    flags = static_cast<CallFunctionFlags>(flags | RECORD_CALL_TARGET);
    Handle<Object> uninitialized =
        TypeFeedbackCells::UninitializedSentinel(isolate());
    Handle<JSGlobalPropertyCell> cell =
        isolate()->factory()->NewJSGlobalPropertyCell(uninitialized);
    RecordTypeFeedbackCell(expr->id(), cell);
    __ Move(rbx, cell);
  }

  CallFunctionStub stub(arg_count, flags);
  __ movq(rdi, Operand(rsp, (arg_count + 1) * kPointerSize));
  __ CallStub(&stub, expr->id());
  RecordJSReturnSite(expr);
  // Restore context register.
  __ movq(rsi, Operand(rbp, StandardFrameConstants::kContextOffset));
  // Discard the function left on TOS.
  context()->DropAndPlug(1, rax);
}


void FullCodeGenerator::EmitResolvePossiblyDirectEval(int arg_count) {
  // Push copy of the first argument or undefined if it doesn't exist.
  if (arg_count > 0) {
    __ push(Operand(rsp, arg_count * kPointerSize));
  } else {
    __ PushRoot(Heap::kUndefinedValueRootIndex);
  }

  // Push the receiver of the enclosing function and do runtime call.
  __ push(Operand(rbp, (2 + info_->scope()->num_parameters()) * kPointerSize));

  // Push the language mode.
  __ Push(Smi::FromInt(language_mode()));

  // Push the start position of the scope the calls resides in.
  __ Push(Smi::FromInt(scope()->start_position()));

  // Do the runtime call.
  __ CallRuntime(Runtime::kResolvePossiblyDirectEval, 5);
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
      __ PushRoot(Heap::kUndefinedValueRootIndex);  // Reserved receiver slot.

      // Push the arguments.
      for (int i = 0; i < arg_count; i++) {
        VisitForStackValue(args->at(i));
      }

      // Push a copy of the function (found below the arguments) and resolve
      // eval.
      __ push(Operand(rsp, (arg_count + 1) * kPointerSize));
      EmitResolvePossiblyDirectEval(arg_count);

      // The runtime call returns a pair of values in rax (function) and
      // rdx (receiver). Touch up the stack with the right values.
      __ movq(Operand(rsp, (arg_count + 0) * kPointerSize), rdx);
      __ movq(Operand(rsp, (arg_count + 1) * kPointerSize), rax);
    }
    // Record source position for debugger.
    SetSourcePosition(expr->position());
    CallFunctionStub stub(arg_count, RECEIVER_MIGHT_BE_IMPLICIT);
    __ movq(rdi, Operand(rsp, (arg_count + 1) * kPointerSize));
    __ CallStub(&stub);
    RecordJSReturnSite(expr);
    // Restore context register.
    __ movq(rsi, Operand(rbp, StandardFrameConstants::kContextOffset));
    context()->DropAndPlug(1, rax);
  } else if (proxy != NULL && proxy->var()->IsUnallocated()) {
    // Call to a global variable.  Push global object as receiver for the
    // call IC lookup.
    __ push(GlobalObjectOperand());
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
    // Call the runtime to find the function to call (returned in rax) and
    // the object holding it (returned in rdx).
    __ push(context_register());
    __ Push(proxy->name());
    __ CallRuntime(Runtime::kLoadContextSlot, 2);
    __ push(rax);  // Function.
    __ push(rdx);  // Receiver.

    // If fast case code has been generated, emit code to push the function
    // and receiver and have the slow path jump around this code.
    if (done.is_linked()) {
      Label call;
      __ jmp(&call, Label::kNear);
      __ bind(&done);
      // Push function.
      __ push(rax);
      // The receiver is implicitly the global receiver. Indicate this by
      // passing the hole to the call function stub.
      __ PushRoot(Heap::kTheHoleValueRootIndex);
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
    __ movq(rbx, GlobalObjectOperand());
    __ push(FieldOperand(rbx, GlobalObject::kGlobalReceiverOffset));
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

  // Load function and argument count into rdi and rax.
  __ Set(rax, arg_count);
  __ movq(rdi, Operand(rsp, arg_count * kPointerSize));

  // Record call targets in unoptimized code, but not in the snapshot.
  CallFunctionFlags flags;
  if (!Serializer::enabled()) {
    flags = RECORD_CALL_TARGET;
    Handle<Object> uninitialized =
        TypeFeedbackCells::UninitializedSentinel(isolate());
    Handle<JSGlobalPropertyCell> cell =
        isolate()->factory()->NewJSGlobalPropertyCell(uninitialized);
    RecordTypeFeedbackCell(expr->id(), cell);
    __ Move(rbx, cell);
  } else {
    flags = NO_CALL_FUNCTION_FLAGS;
  }

  CallConstructStub stub(flags);
  __ Call(stub.GetCode(), RelocInfo::CONSTRUCT_CALL);
  PrepareForBailoutForId(expr->ReturnId(), TOS_REG);
  context()->Plug(rax);
}


void FullCodeGenerator::EmitIsSmi(CallRuntime* expr) {
  ZoneList<Expression*>* args = expr->arguments();
  ASSERT(args->length() == 1);

  VisitForAccumulatorValue(args->at(0));

  Label materialize_true, materialize_false;
  Label* if_true = NULL;
  Label* if_false = NULL;
  Label* fall_through = NULL;
  context()->PrepareTest(&materialize_true, &materialize_false,
                         &if_true, &if_false, &fall_through);

  PrepareForBailoutBeforeSplit(expr, true, if_true, if_false);
  __ JumpIfSmi(rax, if_true);
  __ jmp(if_false);

  context()->Plug(if_true, if_false);
}


void FullCodeGenerator::EmitIsNonNegativeSmi(CallRuntime* expr) {
  ZoneList<Expression*>* args = expr->arguments();
  ASSERT(args->length() == 1);

  VisitForAccumulatorValue(args->at(0));

  Label materialize_true, materialize_false;
  Label* if_true = NULL;
  Label* if_false = NULL;
  Label* fall_through = NULL;
  context()->PrepareTest(&materialize_true, &materialize_false,
                         &if_true, &if_false, &fall_through);

  PrepareForBailoutBeforeSplit(expr, true, if_true, if_false);
  Condition non_negative_smi = masm()->CheckNonNegativeSmi(rax);
  Split(non_negative_smi, if_true, if_false, fall_through);

  context()->Plug(if_true, if_false);
}


void FullCodeGenerator::EmitIsObject(CallRuntime* expr) {
  ZoneList<Expression*>* args = expr->arguments();
  ASSERT(args->length() == 1);

  VisitForAccumulatorValue(args->at(0));

  Label materialize_true, materialize_false;
  Label* if_true = NULL;
  Label* if_false = NULL;
  Label* fall_through = NULL;
  context()->PrepareTest(&materialize_true, &materialize_false,
                         &if_true, &if_false, &fall_through);

  __ JumpIfSmi(rax, if_false);
  __ CompareRoot(rax, Heap::kNullValueRootIndex);
  __ j(equal, if_true);
  __ movq(rbx, FieldOperand(rax, HeapObject::kMapOffset));
  // Undetectable objects behave like undefined when tested with typeof.
  __ testb(FieldOperand(rbx, Map::kBitFieldOffset),
           Immediate(1 << Map::kIsUndetectable));
  __ j(not_zero, if_false);
  __ movzxbq(rbx, FieldOperand(rbx, Map::kInstanceTypeOffset));
  __ cmpq(rbx, Immediate(FIRST_NONCALLABLE_SPEC_OBJECT_TYPE));
  __ j(below, if_false);
  __ cmpq(rbx, Immediate(LAST_NONCALLABLE_SPEC_OBJECT_TYPE));
  PrepareForBailoutBeforeSplit(expr, true, if_true, if_false);
  Split(below_equal, if_true, if_false, fall_through);

  context()->Plug(if_true, if_false);
}


void FullCodeGenerator::EmitIsSpecObject(CallRuntime* expr) {
  ZoneList<Expression*>* args = expr->arguments();
  ASSERT(args->length() == 1);

  VisitForAccumulatorValue(args->at(0));

  Label materialize_true, materialize_false;
  Label* if_true = NULL;
  Label* if_false = NULL;
  Label* fall_through = NULL;
  context()->PrepareTest(&materialize_true, &materialize_false,
                         &if_true, &if_false, &fall_through);

  __ JumpIfSmi(rax, if_false);
  __ CmpObjectType(rax, FIRST_SPEC_OBJECT_TYPE, rbx);
  PrepareForBailoutBeforeSplit(expr, true, if_true, if_false);
  Split(above_equal, if_true, if_false, fall_through);

  context()->Plug(if_true, if_false);
}


void FullCodeGenerator::EmitIsUndetectableObject(CallRuntime* expr) {
  ZoneList<Expression*>* args = expr->arguments();
  ASSERT(args->length() == 1);

  VisitForAccumulatorValue(args->at(0));

  Label materialize_true, materialize_false;
  Label* if_true = NULL;
  Label* if_false = NULL;
  Label* fall_through = NULL;
  context()->PrepareTest(&materialize_true, &materialize_false,
                         &if_true, &if_false, &fall_through);

  __ JumpIfSmi(rax, if_false);
  __ movq(rbx, FieldOperand(rax, HeapObject::kMapOffset));
  __ testb(FieldOperand(rbx, Map::kBitFieldOffset),
           Immediate(1 << Map::kIsUndetectable));
  PrepareForBailoutBeforeSplit(expr, true, if_true, if_false);
  Split(not_zero, if_true, if_false, fall_through);

  context()->Plug(if_true, if_false);
}


void FullCodeGenerator::EmitIsStringWrapperSafeForDefaultValueOf(
    CallRuntime* expr) {
  ZoneList<Expression*>* args = expr->arguments();
  ASSERT(args->length() == 1);

  VisitForAccumulatorValue(args->at(0));

  Label materialize_true, materialize_false;
  Label* if_true = NULL;
  Label* if_false = NULL;
  Label* fall_through = NULL;
  context()->PrepareTest(&materialize_true, &materialize_false,
                         &if_true, &if_false, &fall_through);

  if (FLAG_debug_code) __ AbortIfSmi(rax);

  // Check whether this map has already been checked to be safe for default
  // valueOf.
  __ movq(rbx, FieldOperand(rax, HeapObject::kMapOffset));
  __ testb(FieldOperand(rbx, Map::kBitField2Offset),
           Immediate(1 << Map::kStringWrapperSafeForDefaultValueOf));
  __ j(not_zero, if_true);

  // Check for fast case object. Generate false result for slow case object.
  __ movq(rcx, FieldOperand(rax, JSObject::kPropertiesOffset));
  __ movq(rcx, FieldOperand(rcx, HeapObject::kMapOffset));
  __ CompareRoot(rcx, Heap::kHashTableMapRootIndex);
  __ j(equal, if_false);

  // Look for valueOf symbol in the descriptor array, and indicate false if
  // found. The type is not checked, so if it is a transition it is a false
  // negative.
  __ LoadInstanceDescriptors(rbx, rbx);
  __ movq(rcx, FieldOperand(rbx, FixedArray::kLengthOffset));
  // rbx: descriptor array
  // rcx: length of descriptor array
  // Calculate the end of the descriptor array.
  SmiIndex index = masm_->SmiToIndex(rdx, rcx, kPointerSizeLog2);
  __ lea(rcx,
         Operand(
             rbx, index.reg, index.scale, FixedArray::kHeaderSize));
  // Calculate location of the first key name.
  __ addq(rbx,
          Immediate(FixedArray::kHeaderSize +
                    DescriptorArray::kFirstIndex * kPointerSize));
  // Loop through all the keys in the descriptor array. If one of these is the
  // symbol valueOf the result is false.
  Label entry, loop;
  __ jmp(&entry);
  __ bind(&loop);
  __ movq(rdx, FieldOperand(rbx, 0));
  __ Cmp(rdx, FACTORY->value_of_symbol());
  __ j(equal, if_false);
  __ addq(rbx, Immediate(kPointerSize));
  __ bind(&entry);
  __ cmpq(rbx, rcx);
  __ j(not_equal, &loop);

  // Reload map as register rbx was used as temporary above.
  __ movq(rbx, FieldOperand(rax, HeapObject::kMapOffset));

  // If a valueOf property is not found on the object check that it's
  // prototype is the un-modified String prototype. If not result is false.
  __ movq(rcx, FieldOperand(rbx, Map::kPrototypeOffset));
  __ testq(rcx, Immediate(kSmiTagMask));
  __ j(zero, if_false);
  __ movq(rcx, FieldOperand(rcx, HeapObject::kMapOffset));
  __ movq(rdx, Operand(rsi, Context::SlotOffset(Context::GLOBAL_INDEX)));
  __ movq(rdx, FieldOperand(rdx, GlobalObject::kGlobalContextOffset));
  __ cmpq(rcx,
          ContextOperand(rdx, Context::STRING_FUNCTION_PROTOTYPE_MAP_INDEX));
  __ j(not_equal, if_false);
  // Set the bit in the map to indicate that it has been checked safe for
  // default valueOf and set true result.
  __ or_(FieldOperand(rbx, Map::kBitField2Offset),
         Immediate(1 << Map::kStringWrapperSafeForDefaultValueOf));
  __ jmp(if_true);

  PrepareForBailoutBeforeSplit(expr, true, if_true, if_false);
  context()->Plug(if_true, if_false);
}


void FullCodeGenerator::EmitIsFunction(CallRuntime* expr) {
  ZoneList<Expression*>* args = expr->arguments();
  ASSERT(args->length() == 1);

  VisitForAccumulatorValue(args->at(0));

  Label materialize_true, materialize_false;
  Label* if_true = NULL;
  Label* if_false = NULL;
  Label* fall_through = NULL;
  context()->PrepareTest(&materialize_true, &materialize_false,
                         &if_true, &if_false, &fall_through);

  __ JumpIfSmi(rax, if_false);
  __ CmpObjectType(rax, JS_FUNCTION_TYPE, rbx);
  PrepareForBailoutBeforeSplit(expr, true, if_true, if_false);
  Split(equal, if_true, if_false, fall_through);

  context()->Plug(if_true, if_false);
}


void FullCodeGenerator::EmitIsArray(CallRuntime* expr) {
  ZoneList<Expression*>* args = expr->arguments();
  ASSERT(args->length() == 1);

  VisitForAccumulatorValue(args->at(0));

  Label materialize_true, materialize_false;
  Label* if_true = NULL;
  Label* if_false = NULL;
  Label* fall_through = NULL;
  context()->PrepareTest(&materialize_true, &materialize_false,
                         &if_true, &if_false, &fall_through);

  __ JumpIfSmi(rax, if_false);
  __ CmpObjectType(rax, JS_ARRAY_TYPE, rbx);
  PrepareForBailoutBeforeSplit(expr, true, if_true, if_false);
  Split(equal, if_true, if_false, fall_through);

  context()->Plug(if_true, if_false);
}


void FullCodeGenerator::EmitIsRegExp(CallRuntime* expr) {
  ZoneList<Expression*>* args = expr->arguments();
  ASSERT(args->length() == 1);

  VisitForAccumulatorValue(args->at(0));

  Label materialize_true, materialize_false;
  Label* if_true = NULL;
  Label* if_false = NULL;
  Label* fall_through = NULL;
  context()->PrepareTest(&materialize_true, &materialize_false,
                         &if_true, &if_false, &fall_through);

  __ JumpIfSmi(rax, if_false);
  __ CmpObjectType(rax, JS_REGEXP_TYPE, rbx);
  PrepareForBailoutBeforeSplit(expr, true, if_true, if_false);
  Split(equal, if_true, if_false, fall_through);

  context()->Plug(if_true, if_false);
}



void FullCodeGenerator::EmitIsConstructCall(CallRuntime* expr) {
  ASSERT(expr->arguments()->length() == 0);

  Label materialize_true, materialize_false;
  Label* if_true = NULL;
  Label* if_false = NULL;
  Label* fall_through = NULL;
  context()->PrepareTest(&materialize_true, &materialize_false,
                         &if_true, &if_false, &fall_through);

  // Get the frame pointer for the calling frame.
  __ movq(rax, Operand(rbp, StandardFrameConstants::kCallerFPOffset));

  // Skip the arguments adaptor frame if it exists.
  Label check_frame_marker;
  __ Cmp(Operand(rax, StandardFrameConstants::kContextOffset),
         Smi::FromInt(StackFrame::ARGUMENTS_ADAPTOR));
  __ j(not_equal, &check_frame_marker);
  __ movq(rax, Operand(rax, StandardFrameConstants::kCallerFPOffset));

  // Check the marker in the calling frame.
  __ bind(&check_frame_marker);
  __ Cmp(Operand(rax, StandardFrameConstants::kMarkerOffset),
         Smi::FromInt(StackFrame::CONSTRUCT));
  PrepareForBailoutBeforeSplit(expr, true, if_true, if_false);
  Split(equal, if_true, if_false, fall_through);

  context()->Plug(if_true, if_false);
}


void FullCodeGenerator::EmitObjectEquals(CallRuntime* expr) {
  ZoneList<Expression*>* args = expr->arguments();
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

  __ pop(rbx);
  __ cmpq(rax, rbx);
  PrepareForBailoutBeforeSplit(expr, true, if_true, if_false);
  Split(equal, if_true, if_false, fall_through);

  context()->Plug(if_true, if_false);
}


void FullCodeGenerator::EmitArguments(CallRuntime* expr) {
  ZoneList<Expression*>* args = expr->arguments();
  ASSERT(args->length() == 1);

  // ArgumentsAccessStub expects the key in rdx and the formal
  // parameter count in rax.
  VisitForAccumulatorValue(args->at(0));
  __ movq(rdx, rax);
  __ Move(rax, Smi::FromInt(info_->scope()->num_parameters()));
  ArgumentsAccessStub stub(ArgumentsAccessStub::READ_ELEMENT);
  __ CallStub(&stub);
  context()->Plug(rax);
}


void FullCodeGenerator::EmitArgumentsLength(CallRuntime* expr) {
  ASSERT(expr->arguments()->length() == 0);

  Label exit;
  // Get the number of formal parameters.
  __ Move(rax, Smi::FromInt(info_->scope()->num_parameters()));

  // Check if the calling frame is an arguments adaptor frame.
  __ movq(rbx, Operand(rbp, StandardFrameConstants::kCallerFPOffset));
  __ Cmp(Operand(rbx, StandardFrameConstants::kContextOffset),
         Smi::FromInt(StackFrame::ARGUMENTS_ADAPTOR));
  __ j(not_equal, &exit, Label::kNear);

  // Arguments adaptor case: Read the arguments length from the
  // adaptor frame.
  __ movq(rax, Operand(rbx, ArgumentsAdaptorFrameConstants::kLengthOffset));

  __ bind(&exit);
  if (FLAG_debug_code) __ AbortIfNotSmi(rax);
  context()->Plug(rax);
}


void FullCodeGenerator::EmitClassOf(CallRuntime* expr) {
  ZoneList<Expression*>* args = expr->arguments();
  ASSERT(args->length() == 1);
  Label done, null, function, non_function_constructor;

  VisitForAccumulatorValue(args->at(0));

  // If the object is a smi, we return null.
  __ JumpIfSmi(rax, &null);

  // Check that the object is a JS object but take special care of JS
  // functions to make sure they have 'Function' as their class.
  // Assume that there are only two callable types, and one of them is at
  // either end of the type range for JS object types. Saves extra comparisons.
  STATIC_ASSERT(NUM_OF_CALLABLE_SPEC_OBJECT_TYPES == 2);
  __ CmpObjectType(rax, FIRST_SPEC_OBJECT_TYPE, rax);
  // Map is now in rax.
  __ j(below, &null);
  STATIC_ASSERT(FIRST_NONCALLABLE_SPEC_OBJECT_TYPE ==
                FIRST_SPEC_OBJECT_TYPE + 1);
  __ j(equal, &function);

  __ CmpInstanceType(rax, LAST_SPEC_OBJECT_TYPE);
  STATIC_ASSERT(LAST_NONCALLABLE_SPEC_OBJECT_TYPE ==
                LAST_SPEC_OBJECT_TYPE - 1);
  __ j(equal, &function);
  // Assume that there is no larger type.
  STATIC_ASSERT(LAST_NONCALLABLE_SPEC_OBJECT_TYPE == LAST_TYPE - 1);

  // Check if the constructor in the map is a JS function.
  __ movq(rax, FieldOperand(rax, Map::kConstructorOffset));
  __ CmpObjectType(rax, JS_FUNCTION_TYPE, rbx);
  __ j(not_equal, &non_function_constructor);

  // rax now contains the constructor function. Grab the
  // instance class name from there.
  __ movq(rax, FieldOperand(rax, JSFunction::kSharedFunctionInfoOffset));
  __ movq(rax, FieldOperand(rax, SharedFunctionInfo::kInstanceClassNameOffset));
  __ jmp(&done);

  // Functions have class 'Function'.
  __ bind(&function);
  __ Move(rax, isolate()->factory()->function_class_symbol());
  __ jmp(&done);

  // Objects with a non-function constructor have class 'Object'.
  __ bind(&non_function_constructor);
  __ Move(rax, isolate()->factory()->Object_symbol());
  __ jmp(&done);

  // Non-JS objects have class null.
  __ bind(&null);
  __ LoadRoot(rax, Heap::kNullValueRootIndex);

  // All done.
  __ bind(&done);

  context()->Plug(rax);
}


void FullCodeGenerator::EmitLog(CallRuntime* expr) {
  // Conditionally generate a log call.
  // Args:
  //   0 (literal string): The type of logging (corresponds to the flags).
  //     This is used to determine whether or not to generate the log call.
  //   1 (string): Format string.  Access the string at argument index 2
  //     with '%2s' (see Logger::LogRuntime for all the formats).
  //   2 (array): Arguments to the format string.
  ZoneList<Expression*>* args = expr->arguments();
  ASSERT_EQ(args->length(), 3);
  if (CodeGenerator::ShouldGenerateLog(args->at(0))) {
    VisitForStackValue(args->at(1));
    VisitForStackValue(args->at(2));
    __ CallRuntime(Runtime::kLog, 2);
  }
  // Finally, we're expected to leave a value on the top of the stack.
  __ LoadRoot(rax, Heap::kUndefinedValueRootIndex);
  context()->Plug(rax);
}


void FullCodeGenerator::EmitRandomHeapNumber(CallRuntime* expr) {
  ASSERT(expr->arguments()->length() == 0);

  Label slow_allocate_heapnumber;
  Label heapnumber_allocated;

  __ AllocateHeapNumber(rbx, rcx, &slow_allocate_heapnumber);
  __ jmp(&heapnumber_allocated);

  __ bind(&slow_allocate_heapnumber);
  // Allocate a heap number.
  __ CallRuntime(Runtime::kNumberAlloc, 0);
  __ movq(rbx, rax);

  __ bind(&heapnumber_allocated);

  // Return a random uint32 number in rax.
  // The fresh HeapNumber is in rbx, which is callee-save on both x64 ABIs.
  __ PrepareCallCFunction(1);
#ifdef _WIN64
  __ movq(rcx, ContextOperand(context_register(), Context::GLOBAL_INDEX));
  __ movq(rcx, FieldOperand(rcx, GlobalObject::kGlobalContextOffset));

#else
  __ movq(rdi, ContextOperand(context_register(), Context::GLOBAL_INDEX));
  __ movq(rdi, FieldOperand(rdi, GlobalObject::kGlobalContextOffset));
#endif
  __ CallCFunction(ExternalReference::random_uint32_function(isolate()), 1);

  // Convert 32 random bits in rax to 0.(32 random bits) in a double
  // by computing:
  // ( 1.(20 0s)(32 random bits) x 2^20 ) - (1.0 x 2^20)).
  __ movl(rcx, Immediate(0x49800000));  // 1.0 x 2^20 as single.
  __ movd(xmm1, rcx);
  __ movd(xmm0, rax);
  __ cvtss2sd(xmm1, xmm1);
  __ xorps(xmm0, xmm1);
  __ subsd(xmm0, xmm1);
  __ movsd(FieldOperand(rbx, HeapNumber::kValueOffset), xmm0);

  __ movq(rax, rbx);
  context()->Plug(rax);
}


void FullCodeGenerator::EmitSubString(CallRuntime* expr) {
  // Load the arguments on the stack and call the stub.
  SubStringStub stub;
  ZoneList<Expression*>* args = expr->arguments();
  ASSERT(args->length() == 3);
  VisitForStackValue(args->at(0));
  VisitForStackValue(args->at(1));
  VisitForStackValue(args->at(2));
  __ CallStub(&stub);
  context()->Plug(rax);
}


void FullCodeGenerator::EmitRegExpExec(CallRuntime* expr) {
  // Load the arguments on the stack and call the stub.
  RegExpExecStub stub;
  ZoneList<Expression*>* args = expr->arguments();
  ASSERT(args->length() == 4);
  VisitForStackValue(args->at(0));
  VisitForStackValue(args->at(1));
  VisitForStackValue(args->at(2));
  VisitForStackValue(args->at(3));
  __ CallStub(&stub);
  context()->Plug(rax);
}


void FullCodeGenerator::EmitValueOf(CallRuntime* expr) {
  ZoneList<Expression*>* args = expr->arguments();
  ASSERT(args->length() == 1);

  VisitForAccumulatorValue(args->at(0));  // Load the object.

  Label done;
  // If the object is a smi return the object.
  __ JumpIfSmi(rax, &done);
  // If the object is not a value type, return the object.
  __ CmpObjectType(rax, JS_VALUE_TYPE, rbx);
  __ j(not_equal, &done);
  __ movq(rax, FieldOperand(rax, JSValue::kValueOffset));

  __ bind(&done);
  context()->Plug(rax);
}


void FullCodeGenerator::EmitDateField(CallRuntime* expr) {
  ZoneList<Expression*>* args = expr->arguments();
  ASSERT(args->length() == 2);
  ASSERT_NE(NULL, args->at(1)->AsLiteral());
  Smi* index = Smi::cast(*(args->at(1)->AsLiteral()->handle()));

  VisitForAccumulatorValue(args->at(0));  // Load the object.

  Label runtime, done;
  Register object = rax;
  Register result = rax;
  Register scratch = rcx;

#ifdef DEBUG
  __ AbortIfSmi(object);
  __ CmpObjectType(object, JS_DATE_TYPE, scratch);
  __ Assert(equal, "Trying to get date field from non-date.");
#endif

  if (index->value() == 0) {
    __ movq(result, FieldOperand(object, JSDate::kValueOffset));
  } else {
    if (index->value() < JSDate::kFirstUncachedField) {
      ExternalReference stamp = ExternalReference::date_cache_stamp(isolate());
      __ movq(scratch, stamp);
      __ cmpq(scratch, FieldOperand(object, JSDate::kCacheStampOffset));
      __ j(not_equal, &runtime, Label::kNear);
      __ movq(result, FieldOperand(object, JSDate::kValueOffset +
                                           kPointerSize * index->value()));
      __ jmp(&done);
    }
    __ bind(&runtime);
    __ PrepareCallCFunction(2);
#ifdef _WIN64
  __ movq(rcx, object);
  __ movq(rdx, index, RelocInfo::NONE);
#else
  __ movq(rdi, object);
  __ movq(rsi, index, RelocInfo::NONE);
#endif
    __ CallCFunction(ExternalReference::get_date_field_function(isolate()), 2);
    __ movq(rsi, Operand(rbp, StandardFrameConstants::kContextOffset));
    __ bind(&done);
  }
  context()->Plug(rax);
}


void FullCodeGenerator::EmitMathPow(CallRuntime* expr) {
  // Load the arguments on the stack and call the runtime function.
  ZoneList<Expression*>* args = expr->arguments();
  ASSERT(args->length() == 2);
  VisitForStackValue(args->at(0));
  VisitForStackValue(args->at(1));
  MathPowStub stub(MathPowStub::ON_STACK);
  __ CallStub(&stub);
  context()->Plug(rax);
}


void FullCodeGenerator::EmitSetValueOf(CallRuntime* expr) {
  ZoneList<Expression*>* args = expr->arguments();
  ASSERT(args->length() == 2);

  VisitForStackValue(args->at(0));  // Load the object.
  VisitForAccumulatorValue(args->at(1));  // Load the value.
  __ pop(rbx);  // rax = value. rbx = object.

  Label done;
  // If the object is a smi, return the value.
  __ JumpIfSmi(rbx, &done);

  // If the object is not a value type, return the value.
  __ CmpObjectType(rbx, JS_VALUE_TYPE, rcx);
  __ j(not_equal, &done);

  // Store the value.
  __ movq(FieldOperand(rbx, JSValue::kValueOffset), rax);
  // Update the write barrier.  Save the value as it will be
  // overwritten by the write barrier code and is needed afterward.
  __ movq(rdx, rax);
  __ RecordWriteField(rbx, JSValue::kValueOffset, rdx, rcx, kDontSaveFPRegs);

  __ bind(&done);
  context()->Plug(rax);
}


void FullCodeGenerator::EmitNumberToString(CallRuntime* expr) {
  ZoneList<Expression*>* args = expr->arguments();
  ASSERT_EQ(args->length(), 1);

  // Load the argument on the stack and call the stub.
  VisitForStackValue(args->at(0));

  NumberToStringStub stub;
  __ CallStub(&stub);
  context()->Plug(rax);
}


void FullCodeGenerator::EmitStringCharFromCode(CallRuntime* expr) {
  ZoneList<Expression*>* args = expr->arguments();
  ASSERT(args->length() == 1);

  VisitForAccumulatorValue(args->at(0));

  Label done;
  StringCharFromCodeGenerator generator(rax, rbx);
  generator.GenerateFast(masm_);
  __ jmp(&done);

  NopRuntimeCallHelper call_helper;
  generator.GenerateSlow(masm_, call_helper);

  __ bind(&done);
  context()->Plug(rbx);
}


void FullCodeGenerator::EmitStringCharCodeAt(CallRuntime* expr) {
  ZoneList<Expression*>* args = expr->arguments();
  ASSERT(args->length() == 2);

  VisitForStackValue(args->at(0));
  VisitForAccumulatorValue(args->at(1));

  Register object = rbx;
  Register index = rax;
  Register result = rdx;

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
  __ LoadRoot(result, Heap::kNanValueRootIndex);
  __ jmp(&done);

  __ bind(&need_conversion);
  // Move the undefined value into the result register, which will
  // trigger conversion.
  __ LoadRoot(result, Heap::kUndefinedValueRootIndex);
  __ jmp(&done);

  NopRuntimeCallHelper call_helper;
  generator.GenerateSlow(masm_, call_helper);

  __ bind(&done);
  context()->Plug(result);
}


void FullCodeGenerator::EmitStringCharAt(CallRuntime* expr) {
  ZoneList<Expression*>* args = expr->arguments();
  ASSERT(args->length() == 2);

  VisitForStackValue(args->at(0));
  VisitForAccumulatorValue(args->at(1));

  Register object = rbx;
  Register index = rax;
  Register scratch = rdx;
  Register result = rax;

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
  __ LoadRoot(result, Heap::kEmptyStringRootIndex);
  __ jmp(&done);

  __ bind(&need_conversion);
  // Move smi zero into the result register, which will trigger
  // conversion.
  __ Move(result, Smi::FromInt(0));
  __ jmp(&done);

  NopRuntimeCallHelper call_helper;
  generator.GenerateSlow(masm_, call_helper);

  __ bind(&done);
  context()->Plug(result);
}


void FullCodeGenerator::EmitStringAdd(CallRuntime* expr) {
  ZoneList<Expression*>* args = expr->arguments();
  ASSERT_EQ(2, args->length());

  VisitForStackValue(args->at(0));
  VisitForStackValue(args->at(1));

  StringAddStub stub(NO_STRING_ADD_FLAGS);
  __ CallStub(&stub);
  context()->Plug(rax);
}


void FullCodeGenerator::EmitStringCompare(CallRuntime* expr) {
  ZoneList<Expression*>* args = expr->arguments();
  ASSERT_EQ(2, args->length());

  VisitForStackValue(args->at(0));
  VisitForStackValue(args->at(1));

  StringCompareStub stub;
  __ CallStub(&stub);
  context()->Plug(rax);
}


void FullCodeGenerator::EmitMathSin(CallRuntime* expr) {
  // Load the argument on the stack and call the stub.
  TranscendentalCacheStub stub(TranscendentalCache::SIN,
                               TranscendentalCacheStub::TAGGED);
  ZoneList<Expression*>* args = expr->arguments();
  ASSERT(args->length() == 1);
  VisitForStackValue(args->at(0));
  __ CallStub(&stub);
  context()->Plug(rax);
}


void FullCodeGenerator::EmitMathCos(CallRuntime* expr) {
  // Load the argument on the stack and call the stub.
  TranscendentalCacheStub stub(TranscendentalCache::COS,
                               TranscendentalCacheStub::TAGGED);
  ZoneList<Expression*>* args = expr->arguments();
  ASSERT(args->length() == 1);
  VisitForStackValue(args->at(0));
  __ CallStub(&stub);
  context()->Plug(rax);
}


void FullCodeGenerator::EmitMathTan(CallRuntime* expr) {
  // Load the argument on the stack and call the stub.
  TranscendentalCacheStub stub(TranscendentalCache::TAN,
                               TranscendentalCacheStub::TAGGED);
  ZoneList<Expression*>* args = expr->arguments();
  ASSERT(args->length() == 1);
  VisitForStackValue(args->at(0));
  __ CallStub(&stub);
  context()->Plug(rax);
}


void FullCodeGenerator::EmitMathLog(CallRuntime* expr) {
  // Load the argument on the stack and call the stub.
  TranscendentalCacheStub stub(TranscendentalCache::LOG,
                               TranscendentalCacheStub::TAGGED);
  ZoneList<Expression*>* args = expr->arguments();
  ASSERT(args->length() == 1);
  VisitForStackValue(args->at(0));
  __ CallStub(&stub);
  context()->Plug(rax);
}


void FullCodeGenerator::EmitMathSqrt(CallRuntime* expr) {
  // Load the argument on the stack and call the runtime function.
  ZoneList<Expression*>* args = expr->arguments();
  ASSERT(args->length() == 1);
  VisitForStackValue(args->at(0));
  __ CallRuntime(Runtime::kMath_sqrt, 1);
  context()->Plug(rax);
}


void FullCodeGenerator::EmitCallFunction(CallRuntime* expr) {
  ZoneList<Expression*>* args = expr->arguments();
  ASSERT(args->length() >= 2);

  int arg_count = args->length() - 2;  // 2 ~ receiver and function.
  for (int i = 0; i < arg_count + 1; i++) {
    VisitForStackValue(args->at(i));
  }
  VisitForAccumulatorValue(args->last());  // Function.

  // Check for proxy.
  Label proxy, done;
  __ CmpObjectType(rax, JS_FUNCTION_PROXY_TYPE, rbx);
  __ j(equal, &proxy);

  // InvokeFunction requires the function in rdi. Move it in there.
  __ movq(rdi, result_register());
  ParameterCount count(arg_count);
  __ InvokeFunction(rdi, count, CALL_FUNCTION,
                    NullCallWrapper(), CALL_AS_METHOD);
  __ movq(rsi, Operand(rbp, StandardFrameConstants::kContextOffset));
  __ jmp(&done);

  __ bind(&proxy);
  __ push(rax);
  __ CallRuntime(Runtime::kCall, args->length());
  __ bind(&done);

  context()->Plug(rax);
}


void FullCodeGenerator::EmitRegExpConstructResult(CallRuntime* expr) {
  RegExpConstructResultStub stub;
  ZoneList<Expression*>* args = expr->arguments();
  ASSERT(args->length() == 3);
  VisitForStackValue(args->at(0));
  VisitForStackValue(args->at(1));
  VisitForStackValue(args->at(2));
  __ CallStub(&stub);
  context()->Plug(rax);
}


void FullCodeGenerator::EmitGetFromCache(CallRuntime* expr) {
  ZoneList<Expression*>* args = expr->arguments();
  ASSERT_EQ(2, args->length());

  ASSERT_NE(NULL, args->at(0)->AsLiteral());
  int cache_id = Smi::cast(*(args->at(0)->AsLiteral()->handle()))->value();

  Handle<FixedArray> jsfunction_result_caches(
      isolate()->global_context()->jsfunction_result_caches());
  if (jsfunction_result_caches->length() <= cache_id) {
    __ Abort("Attempt to use undefined cache.");
    __ LoadRoot(rax, Heap::kUndefinedValueRootIndex);
    context()->Plug(rax);
    return;
  }

  VisitForAccumulatorValue(args->at(1));

  Register key = rax;
  Register cache = rbx;
  Register tmp = rcx;
  __ movq(cache, ContextOperand(rsi, Context::GLOBAL_INDEX));
  __ movq(cache,
          FieldOperand(cache, GlobalObject::kGlobalContextOffset));
  __ movq(cache,
          ContextOperand(cache, Context::JSFUNCTION_RESULT_CACHES_INDEX));
  __ movq(cache,
          FieldOperand(cache, FixedArray::OffsetOfElementAt(cache_id)));

  Label done, not_found;
  // tmp now holds finger offset as a smi.
  STATIC_ASSERT(kSmiTag == 0 && kSmiTagSize == 1);
  __ movq(tmp, FieldOperand(cache, JSFunctionResultCache::kFingerOffset));
  SmiIndex index =
      __ SmiToIndex(kScratchRegister, tmp, kPointerSizeLog2);
  __ cmpq(key, FieldOperand(cache,
                            index.reg,
                            index.scale,
                            FixedArray::kHeaderSize));
  __ j(not_equal, &not_found, Label::kNear);
  __ movq(rax, FieldOperand(cache,
                            index.reg,
                            index.scale,
                            FixedArray::kHeaderSize + kPointerSize));
  __ jmp(&done, Label::kNear);

  __ bind(&not_found);
  // Call runtime to perform the lookup.
  __ push(cache);
  __ push(key);
  __ CallRuntime(Runtime::kGetFromCache, 2);

  __ bind(&done);
  context()->Plug(rax);
}


void FullCodeGenerator::EmitIsRegExpEquivalent(CallRuntime* expr) {
  ZoneList<Expression*>* args = expr->arguments();
  ASSERT_EQ(2, args->length());

  Register right = rax;
  Register left = rbx;
  Register tmp = rcx;

  VisitForStackValue(args->at(0));
  VisitForAccumulatorValue(args->at(1));
  __ pop(left);

  Label done, fail, ok;
  __ cmpq(left, right);
  __ j(equal, &ok, Label::kNear);
  // Fail if either is a non-HeapObject.
  Condition either_smi = masm()->CheckEitherSmi(left, right, tmp);
  __ j(either_smi, &fail, Label::kNear);
  __ j(zero, &fail, Label::kNear);
  __ movq(tmp, FieldOperand(left, HeapObject::kMapOffset));
  __ cmpb(FieldOperand(tmp, Map::kInstanceTypeOffset),
          Immediate(JS_REGEXP_TYPE));
  __ j(not_equal, &fail, Label::kNear);
  __ cmpq(tmp, FieldOperand(right, HeapObject::kMapOffset));
  __ j(not_equal, &fail, Label::kNear);
  __ movq(tmp, FieldOperand(left, JSRegExp::kDataOffset));
  __ cmpq(tmp, FieldOperand(right, JSRegExp::kDataOffset));
  __ j(equal, &ok, Label::kNear);
  __ bind(&fail);
  __ Move(rax, isolate()->factory()->false_value());
  __ jmp(&done, Label::kNear);
  __ bind(&ok);
  __ Move(rax, isolate()->factory()->true_value());
  __ bind(&done);

  context()->Plug(rax);
}


void FullCodeGenerator::EmitHasCachedArrayIndex(CallRuntime* expr) {
  ZoneList<Expression*>* args = expr->arguments();
  ASSERT(args->length() == 1);

  VisitForAccumulatorValue(args->at(0));

  Label materialize_true, materialize_false;
  Label* if_true = NULL;
  Label* if_false = NULL;
  Label* fall_through = NULL;
  context()->PrepareTest(&materialize_true, &materialize_false,
                         &if_true, &if_false, &fall_through);

  __ testl(FieldOperand(rax, String::kHashFieldOffset),
           Immediate(String::kContainsCachedArrayIndexMask));
  PrepareForBailoutBeforeSplit(expr, true, if_true, if_false);
  __ j(zero, if_true);
  __ jmp(if_false);

  context()->Plug(if_true, if_false);
}


void FullCodeGenerator::EmitGetCachedArrayIndex(CallRuntime* expr) {
  ZoneList<Expression*>* args = expr->arguments();
  ASSERT(args->length() == 1);
  VisitForAccumulatorValue(args->at(0));

  if (FLAG_debug_code) {
    __ AbortIfNotString(rax);
  }

  __ movl(rax, FieldOperand(rax, String::kHashFieldOffset));
  ASSERT(String::kHashShift >= kSmiTagSize);
  __ IndexFromHash(rax, rax);

  context()->Plug(rax);
}


void FullCodeGenerator::EmitFastAsciiArrayJoin(CallRuntime* expr) {
  Label bailout, return_result, done, one_char_separator, long_separator,
      non_trivial_array, not_size_one_array, loop,
      loop_1, loop_1_condition, loop_2, loop_2_entry, loop_3, loop_3_entry;
  ZoneList<Expression*>* args = expr->arguments();
  ASSERT(args->length() == 2);
  // We will leave the separator on the stack until the end of the function.
  VisitForStackValue(args->at(1));
  // Load this to rax (= array)
  VisitForAccumulatorValue(args->at(0));
  // All aliases of the same register have disjoint lifetimes.
  Register array = rax;
  Register elements = no_reg;  // Will be rax.

  Register index = rdx;

  Register string_length = rcx;

  Register string = rsi;

  Register scratch = rbx;

  Register array_length = rdi;
  Register result_pos = no_reg;  // Will be rdi.

  Operand separator_operand =    Operand(rsp, 2 * kPointerSize);
  Operand result_operand =       Operand(rsp, 1 * kPointerSize);
  Operand array_length_operand = Operand(rsp, 0 * kPointerSize);
  // Separator operand is already pushed. Make room for the two
  // other stack fields, and clear the direction flag in anticipation
  // of calling CopyBytes.
  __ subq(rsp, Immediate(2 * kPointerSize));
  __ cld();
  // Check that the array is a JSArray
  __ JumpIfSmi(array, &bailout);
  __ CmpObjectType(array, JS_ARRAY_TYPE, scratch);
  __ j(not_equal, &bailout);

  // Check that the array has fast elements.
  __ CheckFastElements(scratch, &bailout);

  // Array has fast elements, so its length must be a smi.
  // If the array has length zero, return the empty string.
  __ movq(array_length, FieldOperand(array, JSArray::kLengthOffset));
  __ SmiCompare(array_length, Smi::FromInt(0));
  __ j(not_zero, &non_trivial_array);
  __ LoadRoot(rax, Heap::kEmptyStringRootIndex);
  __ jmp(&return_result);

  // Save the array length on the stack.
  __ bind(&non_trivial_array);
  __ SmiToInteger32(array_length, array_length);
  __ movl(array_length_operand, array_length);

  // Save the FixedArray containing array's elements.
  // End of array's live range.
  elements = array;
  __ movq(elements, FieldOperand(array, JSArray::kElementsOffset));
  array = no_reg;


  // Check that all array elements are sequential ASCII strings, and
  // accumulate the sum of their lengths, as a smi-encoded value.
  __ Set(index, 0);
  __ Set(string_length, 0);
  // Loop condition: while (index < array_length).
  // Live loop registers: index(int32), array_length(int32), string(String*),
  //                      scratch, string_length(int32), elements(FixedArray*).
  if (FLAG_debug_code) {
    __ cmpq(index, array_length);
    __ Assert(below, "No empty arrays here in EmitFastAsciiArrayJoin");
  }
  __ bind(&loop);
  __ movq(string, FieldOperand(elements,
                               index,
                               times_pointer_size,
                               FixedArray::kHeaderSize));
  __ JumpIfSmi(string, &bailout);
  __ movq(scratch, FieldOperand(string, HeapObject::kMapOffset));
  __ movzxbl(scratch, FieldOperand(scratch, Map::kInstanceTypeOffset));
  __ andb(scratch, Immediate(
      kIsNotStringMask | kStringEncodingMask | kStringRepresentationMask));
  __ cmpb(scratch, Immediate(kStringTag | kAsciiStringTag | kSeqStringTag));
  __ j(not_equal, &bailout);
  __ AddSmiField(string_length,
                 FieldOperand(string, SeqAsciiString::kLengthOffset));
  __ j(overflow, &bailout);
  __ incl(index);
  __ cmpl(index, array_length);
  __ j(less, &loop);

  // Live registers:
  // string_length: Sum of string lengths.
  // elements: FixedArray of strings.
  // index: Array length.
  // array_length: Array length.

  // If array_length is 1, return elements[0], a string.
  __ cmpl(array_length, Immediate(1));
  __ j(not_equal, &not_size_one_array);
  __ movq(rax, FieldOperand(elements, FixedArray::kHeaderSize));
  __ jmp(&return_result);

  __ bind(&not_size_one_array);

  // End of array_length live range.
  result_pos = array_length;
  array_length = no_reg;

  // Live registers:
  // string_length: Sum of string lengths.
  // elements: FixedArray of strings.
  // index: Array length.

  // Check that the separator is a sequential ASCII string.
  __ movq(string, separator_operand);
  __ JumpIfSmi(string, &bailout);
  __ movq(scratch, FieldOperand(string, HeapObject::kMapOffset));
  __ movzxbl(scratch, FieldOperand(scratch, Map::kInstanceTypeOffset));
  __ andb(scratch, Immediate(
      kIsNotStringMask | kStringEncodingMask | kStringRepresentationMask));
  __ cmpb(scratch, Immediate(kStringTag | kAsciiStringTag | kSeqStringTag));
  __ j(not_equal, &bailout);

  // Live registers:
  // string_length: Sum of string lengths.
  // elements: FixedArray of strings.
  // index: Array length.
  // string: Separator string.

  // Add (separator length times (array_length - 1)) to string_length.
  __ SmiToInteger32(scratch,
                    FieldOperand(string, SeqAsciiString::kLengthOffset));
  __ decl(index);
  __ imull(scratch, index);
  __ j(overflow, &bailout);
  __ addl(string_length, scratch);
  __ j(overflow, &bailout);

  // Live registers and stack values:
  //   string_length: Total length of result string.
  //   elements: FixedArray of strings.
  __ AllocateAsciiString(result_pos, string_length, scratch,
                         index, string, &bailout);
  __ movq(result_operand, result_pos);
  __ lea(result_pos, FieldOperand(result_pos, SeqAsciiString::kHeaderSize));

  __ movq(string, separator_operand);
  __ SmiCompare(FieldOperand(string, SeqAsciiString::kLengthOffset),
                Smi::FromInt(1));
  __ j(equal, &one_char_separator);
  __ j(greater, &long_separator);


  // Empty separator case:
  __ Set(index, 0);
  __ movl(scratch, array_length_operand);
  __ jmp(&loop_1_condition);
  // Loop condition: while (index < array_length).
  __ bind(&loop_1);
  // Each iteration of the loop concatenates one string to the result.
  // Live values in registers:
  //   index: which element of the elements array we are adding to the result.
  //   result_pos: the position to which we are currently copying characters.
  //   elements: the FixedArray of strings we are joining.
  //   scratch: array length.

  // Get string = array[index].
  __ movq(string, FieldOperand(elements, index,
                               times_pointer_size,
                               FixedArray::kHeaderSize));
  __ SmiToInteger32(string_length,
                    FieldOperand(string, String::kLengthOffset));
  __ lea(string,
         FieldOperand(string, SeqAsciiString::kHeaderSize));
  __ CopyBytes(result_pos, string, string_length);
  __ incl(index);
  __ bind(&loop_1_condition);
  __ cmpl(index, scratch);
  __ j(less, &loop_1);  // Loop while (index < array_length).
  __ jmp(&done);

  // Generic bailout code used from several places.
  __ bind(&bailout);
  __ LoadRoot(rax, Heap::kUndefinedValueRootIndex);
  __ jmp(&return_result);


  // One-character separator case
  __ bind(&one_char_separator);
  // Get the separator ASCII character value.
  // Register "string" holds the separator.
  __ movzxbl(scratch, FieldOperand(string, SeqAsciiString::kHeaderSize));
  __ Set(index, 0);
  // Jump into the loop after the code that copies the separator, so the first
  // element is not preceded by a separator
  __ jmp(&loop_2_entry);
  // Loop condition: while (index < length).
  __ bind(&loop_2);
  // Each iteration of the loop concatenates one string to the result.
  // Live values in registers:
  //   elements: The FixedArray of strings we are joining.
  //   index: which element of the elements array we are adding to the result.
  //   result_pos: the position to which we are currently copying characters.
  //   scratch: Separator character.

  // Copy the separator character to the result.
  __ movb(Operand(result_pos, 0), scratch);
  __ incq(result_pos);

  __ bind(&loop_2_entry);
  // Get string = array[index].
  __ movq(string, FieldOperand(elements, index,
                               times_pointer_size,
                               FixedArray::kHeaderSize));
  __ SmiToInteger32(string_length,
                    FieldOperand(string, String::kLengthOffset));
  __ lea(string,
         FieldOperand(string, SeqAsciiString::kHeaderSize));
  __ CopyBytes(result_pos, string, string_length);
  __ incl(index);
  __ cmpl(index, array_length_operand);
  __ j(less, &loop_2);  // End while (index < length).
  __ jmp(&done);


  // Long separator case (separator is more than one character).
  __ bind(&long_separator);

  // Make elements point to end of elements array, and index
  // count from -array_length to zero, so we don't need to maintain
  // a loop limit.
  __ movl(index, array_length_operand);
  __ lea(elements, FieldOperand(elements, index, times_pointer_size,
                                FixedArray::kHeaderSize));
  __ neg(index);

  // Replace separator string with pointer to its first character, and
  // make scratch be its length.
  __ movq(string, separator_operand);
  __ SmiToInteger32(scratch,
                    FieldOperand(string, String::kLengthOffset));
  __ lea(string,
         FieldOperand(string, SeqAsciiString::kHeaderSize));
  __ movq(separator_operand, string);

  // Jump into the loop after the code that copies the separator, so the first
  // element is not preceded by a separator
  __ jmp(&loop_3_entry);
  // Loop condition: while (index < length).
  __ bind(&loop_3);
  // Each iteration of the loop concatenates one string to the result.
  // Live values in registers:
  //   index: which element of the elements array we are adding to the result.
  //   result_pos: the position to which we are currently copying characters.
  //   scratch: Separator length.
  //   separator_operand (rsp[0x10]): Address of first char of separator.

  // Copy the separator to the result.
  __ movq(string, separator_operand);
  __ movl(string_length, scratch);
  __ CopyBytes(result_pos, string, string_length, 2);

  __ bind(&loop_3_entry);
  // Get string = array[index].
  __ movq(string, Operand(elements, index, times_pointer_size, 0));
  __ SmiToInteger32(string_length,
                    FieldOperand(string, String::kLengthOffset));
  __ lea(string,
         FieldOperand(string, SeqAsciiString::kHeaderSize));
  __ CopyBytes(result_pos, string, string_length);
  __ incq(index);
  __ j(not_equal, &loop_3);  // Loop while (index < 0).

  __ bind(&done);
  __ movq(rax, result_operand);

  __ bind(&return_result);
  // Drop temp values from the stack, and restore context register.
  __ addq(rsp, Immediate(3 * kPointerSize));
  __ movq(rsi, Operand(rbp, StandardFrameConstants::kContextOffset));
  context()->Plug(rax);
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
    __ movq(rax, GlobalObjectOperand());
    __ push(FieldOperand(rax, GlobalObject::kBuiltinsOffset));
  }

  // Push the arguments ("left-to-right").
  int arg_count = args->length();
  for (int i = 0; i < arg_count; i++) {
    VisitForStackValue(args->at(i));
  }

  if (expr->is_jsruntime()) {
    // Call the JS runtime function using a call IC.
    __ Move(rcx, expr->name());
    RelocInfo::Mode mode = RelocInfo::CODE_TARGET;
    Handle<Code> ic =
        isolate()->stub_cache()->ComputeCallInitialize(arg_count, mode);
    CallIC(ic, mode, expr->id());
    // Restore context register.
    __ movq(rsi, Operand(rbp, StandardFrameConstants::kContextOffset));
  } else {
    __ CallRuntime(expr->function(), arg_count);
  }
  context()->Plug(rax);
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
        StrictModeFlag strict_mode_flag = (language_mode() == CLASSIC_MODE)
            ? kNonStrictMode : kStrictMode;
        __ Push(Smi::FromInt(strict_mode_flag));
        __ InvokeBuiltin(Builtins::DELETE, CALL_FUNCTION);
        context()->Plug(rax);
      } else if (proxy != NULL) {
        Variable* var = proxy->var();
        // Delete of an unqualified identifier is disallowed in strict mode
        // but "delete this" is allowed.
        ASSERT(language_mode() == CLASSIC_MODE || var->is_this());
        if (var->IsUnallocated()) {
          __ push(GlobalObjectOperand());
          __ Push(var->name());
          __ Push(Smi::FromInt(kNonStrictMode));
          __ InvokeBuiltin(Builtins::DELETE, CALL_FUNCTION);
          context()->Plug(rax);
        } else if (var->IsStackAllocated() || var->IsContextSlot()) {
          // Result of deleting non-global variables is false.  'this' is
          // not really a variable, though we implement it as one.  The
          // subexpression does not have side effects.
          context()->Plug(var->is_this());
        } else {
          // Non-global variable.  Call the runtime to try to delete from the
          // context where the variable was introduced.
          __ push(context_register());
          __ Push(var->name());
          __ CallRuntime(Runtime::kDeleteContextSlot, 2);
          context()->Plug(rax);
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
        ASSERT(context()->IsAccumulatorValue() || context()->IsStackValue());
        Label materialize_true, materialize_false, done;
        VisitForControl(expr->expression(),
                        &materialize_false,
                        &materialize_true,
                        &materialize_true);
        __ bind(&materialize_true);
        PrepareForBailoutForId(expr->MaterializeTrueId(), NO_REGISTERS);
        if (context()->IsAccumulatorValue()) {
          __ LoadRoot(rax, Heap::kTrueValueRootIndex);
        } else {
          __ PushRoot(Heap::kTrueValueRootIndex);
        }
        __ jmp(&done, Label::kNear);
        __ bind(&materialize_false);
        PrepareForBailoutForId(expr->MaterializeFalseId(), NO_REGISTERS);
        if (context()->IsAccumulatorValue()) {
          __ LoadRoot(rax, Heap::kFalseValueRootIndex);
        } else {
          __ PushRoot(Heap::kFalseValueRootIndex);
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
      context()->Plug(rax);
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
  // TODO(svenpanne): Allowing format strings in Comment would be nice here...
  Comment cmt(masm_, comment);
  bool can_overwrite = expr->expression()->ResultOverwriteAllowed();
  UnaryOverwriteMode overwrite =
      can_overwrite ? UNARY_OVERWRITE : UNARY_NO_OVERWRITE;
  UnaryOpStub stub(expr->op(), overwrite);
  // UnaryOpStub expects the argument to be in the
  // accumulator register rax.
  VisitForAccumulatorValue(expr->expression());
  SetSourcePosition(expr->position());
  CallIC(stub.GetCode(), RelocInfo::CODE_TARGET, expr->id());
  context()->Plug(rax);
}


void FullCodeGenerator::VisitCountOperation(CountOperation* expr) {
  Comment cmnt(masm_, "[ CountOperation");
  SetSourcePosition(expr->position());

  // Invalid left-hand-sides are rewritten to have a 'throw
  // ReferenceError' as the left-hand side.
  if (!expr->expression()->IsValidLeftHandSide()) {
    VisitForEffect(expr->expression());
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
      __ Push(Smi::FromInt(0));
    }
    if (assign_type == NAMED_PROPERTY) {
      VisitForAccumulatorValue(prop->obj());
      __ push(rax);  // Copy of receiver, needed for later store.
      EmitNamedPropertyLoad(prop);
    } else {
      VisitForStackValue(prop->obj());
      VisitForAccumulatorValue(prop->key());
      __ movq(rdx, Operand(rsp, 0));  // Leave receiver on stack
      __ push(rax);  // Copy of key, needed for later store.
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
  __ JumpIfSmi(rax, &no_conversion, Label::kNear);
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
          __ push(rax);
          break;
        case NAMED_PROPERTY:
          __ movq(Operand(rsp, kPointerSize), rax);
          break;
        case KEYED_PROPERTY:
          __ movq(Operand(rsp, 2 * kPointerSize), rax);
          break;
      }
    }
  }

  // Inline smi case if we are in a loop.
  Label done, stub_call;
  JumpPatchSite patch_site(masm_);

  if (ShouldInlineSmiCase(expr->op())) {
    if (expr->op() == Token::INC) {
      __ SmiAddConstant(rax, rax, Smi::FromInt(1));
    } else {
      __ SmiSubConstant(rax, rax, Smi::FromInt(1));
    }
    __ j(overflow, &stub_call, Label::kNear);
    // We could eliminate this smi check if we split the code at
    // the first smi check before calling ToNumber.
    patch_site.EmitJumpIfSmi(rax, &done, Label::kNear);

    __ bind(&stub_call);
    // Call stub. Undo operation first.
    if (expr->op() == Token::INC) {
      __ SmiSubConstant(rax, rax, Smi::FromInt(1));
    } else {
      __ SmiAddConstant(rax, rax, Smi::FromInt(1));
    }
  }

  // Record position before stub call.
  SetSourcePosition(expr->position());

  // Call stub for +1/-1.
  BinaryOpStub stub(expr->binary_op(), NO_OVERWRITE);
  if (expr->op() == Token::INC) {
    __ Move(rdx, Smi::FromInt(1));
  } else {
    __ movq(rdx, rax);
    __ Move(rax, Smi::FromInt(1));
  }
  CallIC(stub.GetCode(), RelocInfo::CODE_TARGET, expr->CountId());
  patch_site.EmitPatchInfo();
  __ bind(&done);

  // Store the value returned in rax.
  switch (assign_type) {
    case VARIABLE:
      if (expr->is_postfix()) {
        // Perform the assignment as if via '='.
        { EffectContext context(this);
          EmitVariableAssignment(expr->expression()->AsVariableProxy()->var(),
                                 Token::ASSIGN);
          PrepareForBailoutForId(expr->AssignmentId(), TOS_REG);
          context.Plug(rax);
        }
        // For all contexts except kEffect: We have the result on
        // top of the stack.
        if (!context()->IsEffect()) {
          context()->PlugTOS();
        }
      } else {
        // Perform the assignment as if via '='.
        EmitVariableAssignment(expr->expression()->AsVariableProxy()->var(),
                               Token::ASSIGN);
        PrepareForBailoutForId(expr->AssignmentId(), TOS_REG);
        context()->Plug(rax);
      }
      break;
    case NAMED_PROPERTY: {
      __ Move(rcx, prop->key()->AsLiteral()->handle());
      __ pop(rdx);
      Handle<Code> ic = is_classic_mode()
          ? isolate()->builtins()->StoreIC_Initialize()
          : isolate()->builtins()->StoreIC_Initialize_Strict();
      CallIC(ic, RelocInfo::CODE_TARGET, expr->id());
      PrepareForBailoutForId(expr->AssignmentId(), TOS_REG);
      if (expr->is_postfix()) {
        if (!context()->IsEffect()) {
          context()->PlugTOS();
        }
      } else {
        context()->Plug(rax);
      }
      break;
    }
    case KEYED_PROPERTY: {
      __ pop(rcx);
      __ pop(rdx);
      Handle<Code> ic = is_classic_mode()
          ? isolate()->builtins()->KeyedStoreIC_Initialize()
          : isolate()->builtins()->KeyedStoreIC_Initialize_Strict();
      CallIC(ic, RelocInfo::CODE_TARGET, expr->id());
      PrepareForBailoutForId(expr->AssignmentId(), TOS_REG);
      if (expr->is_postfix()) {
        if (!context()->IsEffect()) {
          context()->PlugTOS();
        }
      } else {
        context()->Plug(rax);
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
    __ Move(rcx, proxy->name());
    __ movq(rax, GlobalObjectOperand());
    Handle<Code> ic = isolate()->builtins()->LoadIC_Initialize();
    // Use a regular load, not a contextual load, to avoid a reference
    // error.
    CallIC(ic);
    PrepareForBailout(expr, TOS_REG);
    context()->Plug(rax);
  } else if (proxy != NULL && proxy->var()->IsLookupSlot()) {
    Label done, slow;

    // Generate code for loading from variables potentially shadowed
    // by eval-introduced variables.
    EmitDynamicLookupFastCase(proxy->var(), INSIDE_TYPEOF, &slow, &done);

    __ bind(&slow);
    __ push(rsi);
    __ Push(proxy->name());
    __ CallRuntime(Runtime::kLoadContextSlotNoReferenceError, 2);
    PrepareForBailout(expr, TOS_REG);
    __ bind(&done);

    context()->Plug(rax);
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

  if (check->Equals(isolate()->heap()->number_symbol())) {
    __ JumpIfSmi(rax, if_true);
    __ movq(rax, FieldOperand(rax, HeapObject::kMapOffset));
    __ CompareRoot(rax, Heap::kHeapNumberMapRootIndex);
    Split(equal, if_true, if_false, fall_through);
  } else if (check->Equals(isolate()->heap()->string_symbol())) {
    __ JumpIfSmi(rax, if_false);
    // Check for undetectable objects => false.
    __ CmpObjectType(rax, FIRST_NONSTRING_TYPE, rdx);
    __ j(above_equal, if_false);
    __ testb(FieldOperand(rdx, Map::kBitFieldOffset),
             Immediate(1 << Map::kIsUndetectable));
    Split(zero, if_true, if_false, fall_through);
  } else if (check->Equals(isolate()->heap()->boolean_symbol())) {
    __ CompareRoot(rax, Heap::kTrueValueRootIndex);
    __ j(equal, if_true);
    __ CompareRoot(rax, Heap::kFalseValueRootIndex);
    Split(equal, if_true, if_false, fall_through);
  } else if (FLAG_harmony_typeof &&
             check->Equals(isolate()->heap()->null_symbol())) {
    __ CompareRoot(rax, Heap::kNullValueRootIndex);
    Split(equal, if_true, if_false, fall_through);
  } else if (check->Equals(isolate()->heap()->undefined_symbol())) {
    __ CompareRoot(rax, Heap::kUndefinedValueRootIndex);
    __ j(equal, if_true);
    __ JumpIfSmi(rax, if_false);
    // Check for undetectable objects => true.
    __ movq(rdx, FieldOperand(rax, HeapObject::kMapOffset));
    __ testb(FieldOperand(rdx, Map::kBitFieldOffset),
             Immediate(1 << Map::kIsUndetectable));
    Split(not_zero, if_true, if_false, fall_through);
  } else if (check->Equals(isolate()->heap()->function_symbol())) {
    __ JumpIfSmi(rax, if_false);
    STATIC_ASSERT(NUM_OF_CALLABLE_SPEC_OBJECT_TYPES == 2);
    __ CmpObjectType(rax, JS_FUNCTION_TYPE, rdx);
    __ j(equal, if_true);
    __ CmpInstanceType(rdx, JS_FUNCTION_PROXY_TYPE);
    Split(equal, if_true, if_false, fall_through);
  } else if (check->Equals(isolate()->heap()->object_symbol())) {
    __ JumpIfSmi(rax, if_false);
    if (!FLAG_harmony_typeof) {
      __ CompareRoot(rax, Heap::kNullValueRootIndex);
      __ j(equal, if_true);
    }
    __ CmpObjectType(rax, FIRST_NONCALLABLE_SPEC_OBJECT_TYPE, rdx);
    __ j(below, if_false);
    __ CmpInstanceType(rdx, LAST_NONCALLABLE_SPEC_OBJECT_TYPE);
    __ j(above, if_false);
    // Check for undetectable objects => false.
    __ testb(FieldOperand(rdx, Map::kBitFieldOffset),
             Immediate(1 << Map::kIsUndetectable));
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
      __ CompareRoot(rax, Heap::kTrueValueRootIndex);
      Split(equal, if_true, if_false, fall_through);
      break;

    case Token::INSTANCEOF: {
      VisitForStackValue(expr->right());
      InstanceofStub stub(InstanceofStub::kNoFlags);
      __ CallStub(&stub);
      PrepareForBailoutBeforeSplit(expr, true, if_true, if_false);
      __ testq(rax, rax);
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
          break;
        case Token::LT:
          cc = less;
          break;
        case Token::GT:
          cc = greater;
         break;
        case Token::LTE:
          cc = less_equal;
          break;
        case Token::GTE:
          cc = greater_equal;
          break;
        case Token::IN:
        case Token::INSTANCEOF:
        default:
          UNREACHABLE();
      }
      __ pop(rdx);

      bool inline_smi_code = ShouldInlineSmiCase(op);
      JumpPatchSite patch_site(masm_);
      if (inline_smi_code) {
        Label slow_case;
        __ movq(rcx, rdx);
        __ or_(rcx, rax);
        patch_site.EmitJumpIfNotSmi(rcx, &slow_case, Label::kNear);
        __ cmpq(rdx, rax);
        Split(cc, if_true, if_false, NULL);
        __ bind(&slow_case);
      }

      // Record position and call the compare IC.
      SetSourcePosition(expr->position());
      Handle<Code> ic = CompareIC::GetUninitialized(op);
      CallIC(ic, RelocInfo::CODE_TARGET, expr->id());
      patch_site.EmitPatchInfo();

      PrepareForBailoutBeforeSplit(expr, true, if_true, if_false);
      __ testq(rax, rax);
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
  Heap::RootListIndex nil_value = nil == kNullValue ?
      Heap::kNullValueRootIndex :
      Heap::kUndefinedValueRootIndex;
  __ CompareRoot(rax, nil_value);
  if (expr->op() == Token::EQ_STRICT) {
    Split(equal, if_true, if_false, fall_through);
  } else {
    Heap::RootListIndex other_nil_value = nil == kNullValue ?
        Heap::kUndefinedValueRootIndex :
        Heap::kNullValueRootIndex;
    __ j(equal, if_true);
    __ CompareRoot(rax, other_nil_value);
    __ j(equal, if_true);
    __ JumpIfSmi(rax, if_false);
    // It can be an undetectable object.
    __ movq(rdx, FieldOperand(rax, HeapObject::kMapOffset));
    __ testb(FieldOperand(rdx, Map::kBitFieldOffset),
             Immediate(1 << Map::kIsUndetectable));
    Split(not_zero, if_true, if_false, fall_through);
  }
  context()->Plug(if_true, if_false);
}


void FullCodeGenerator::VisitThisFunction(ThisFunction* expr) {
  __ movq(rax, Operand(rbp, JavaScriptFrameConstants::kFunctionOffset));
  context()->Plug(rax);
}


Register FullCodeGenerator::result_register() {
  return rax;
}


Register FullCodeGenerator::context_register() {
  return rsi;
}


void FullCodeGenerator::StoreToFrameField(int frame_offset, Register value) {
  ASSERT(IsAligned(frame_offset, kPointerSize));
  __ movq(Operand(rbp, frame_offset), value);
}


void FullCodeGenerator::LoadContextField(Register dst, int context_index) {
  __ movq(dst, ContextOperand(rsi, context_index));
}


void FullCodeGenerator::PushFunctionArgumentForContextAllocation() {
  Scope* declaration_scope = scope()->DeclarationScope();
  if (declaration_scope->is_global_scope() ||
      declaration_scope->is_module_scope()) {
    // Contexts nested in the global context have a canonical empty function
    // as their closure, not the anonymous closure containing the global
    // code.  Pass a smi sentinel and let the runtime look up the empty
    // function.
    __ Push(Smi::FromInt(0));
  } else if (declaration_scope->is_eval_scope()) {
    // Contexts created by a call to eval have the same closure as the
    // context calling eval, not the anonymous closure containing the eval
    // code.  Fetch it from the context.
    __ push(ContextOperand(rsi, Context::CLOSURE_INDEX));
  } else {
    ASSERT(declaration_scope->is_function_scope());
    __ push(Operand(rbp, JavaScriptFrameConstants::kFunctionOffset));
  }
}


// ----------------------------------------------------------------------------
// Non-local control flow support.


void FullCodeGenerator::EnterFinallyBlock() {
  ASSERT(!result_register().is(rdx));
  ASSERT(!result_register().is(rcx));
  // Cook return address on top of stack (smi encoded Code* delta)
  __ pop(rdx);
  __ Move(rcx, masm_->CodeObject());
  __ subq(rdx, rcx);
  __ Integer32ToSmi(rdx, rdx);
  __ push(rdx);

  // Store result register while executing finally block.
  __ push(result_register());

  // Store pending message while executing finally block.
  ExternalReference pending_message_obj =
      ExternalReference::address_of_pending_message_obj(isolate());
  __ Load(rdx, pending_message_obj);
  __ push(rdx);

  ExternalReference has_pending_message =
      ExternalReference::address_of_has_pending_message(isolate());
  __ Load(rdx, has_pending_message);
  __ push(rdx);

  ExternalReference pending_message_script =
      ExternalReference::address_of_pending_message_script(isolate());
  __ Load(rdx, pending_message_script);
  __ push(rdx);
}


void FullCodeGenerator::ExitFinallyBlock() {
  ASSERT(!result_register().is(rdx));
  ASSERT(!result_register().is(rcx));
  // Restore pending message from stack.
  __ pop(rdx);
  ExternalReference pending_message_script =
      ExternalReference::address_of_pending_message_script(isolate());
  __ Store(pending_message_script, rdx);

  __ pop(rdx);
  ExternalReference has_pending_message =
      ExternalReference::address_of_has_pending_message(isolate());
  __ Store(has_pending_message, rdx);

  __ pop(rdx);
  ExternalReference pending_message_obj =
      ExternalReference::address_of_pending_message_obj(isolate());
  __ Store(pending_message_obj, rdx);

  // Restore result register from stack.
  __ pop(result_register());

  // Uncook return address.
  __ pop(rdx);
  __ SmiToInteger32(rdx, rdx);
  __ Move(rcx, masm_->CodeObject());
  __ addq(rdx, rcx);
  __ jmp(rdx);
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
    __ movq(rsi, Operand(rsp, StackHandlerConstants::kContextOffset));
    __ movq(Operand(rbp, StandardFrameConstants::kContextOffset), rsi);
  }
  __ PopTryHandler();
  __ call(finally_entry_);

  *stack_depth = 0;
  *context_length = 0;
  return previous_;
}


#undef __

} }  // namespace v8::internal

#endif  // V8_TARGET_ARCH_X64
