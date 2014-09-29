// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/v8.h"

#if V8_TARGET_ARCH_X64

#include "src/code-stubs.h"
#include "src/codegen.h"
#include "src/compiler.h"
#include "src/debug.h"
#include "src/full-codegen.h"
#include "src/isolate-inl.h"
#include "src/parser.h"
#include "src/scopes.h"
#include "src/stub-cache.h"

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
      DCHECK(is_uint8(delta_to_patch_site));
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
    DCHECK(!patch_site_.is_bound() && !info_emitted_);
    DCHECK(cc == carry || cc == not_carry);
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
    StackArgumentsAccessor args(rsp, info->scope()->num_parameters());
    __ movp(rcx, args.GetReceiverOperand());

    __ CompareRoot(rcx, Heap::kUndefinedValueRootIndex);
    __ j(not_equal, &ok, Label::kNear);

    __ movp(rcx, GlobalObjectOperand());
    __ movp(rcx, FieldOperand(rcx, GlobalObject::kGlobalProxyOffset));

    __ movp(args.GetReceiverOperand(), rcx);

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
      __ PushRoot(Heap::kUndefinedValueRootIndex);
    } else if (locals_count > 1) {
      if (locals_count >= 128) {
        Label ok;
        __ movp(rcx, rsp);
        __ subp(rcx, Immediate(locals_count * kPointerSize));
        __ CompareRoot(rcx, Heap::kRealStackLimitRootIndex);
        __ j(above_equal, &ok, Label::kNear);
        __ InvokeBuiltin(Builtins::STACK_OVERFLOW, CALL_FUNCTION);
        __ bind(&ok);
      }
      __ LoadRoot(rdx, Heap::kUndefinedValueRootIndex);
      const int kMaxPushes = 32;
      if (locals_count >= kMaxPushes) {
        int loop_iterations = locals_count / kMaxPushes;
        __ movp(rcx, Immediate(loop_iterations));
        Label loop_header;
        __ bind(&loop_header);
        // Do pushes.
        for (int i = 0; i < kMaxPushes; i++) {
          __ Push(rdx);
        }
        // Continue loop if not done.
        __ decp(rcx);
        __ j(not_zero, &loop_header, Label::kNear);
      }
      int remaining = locals_count % kMaxPushes;
      // Emit the remaining pushes.
      for (int i  = 0; i < remaining; i++) {
        __ Push(rdx);
      }
    }
  }

  bool function_in_register = true;

  // Possibly allocate a local context.
  int heap_slots = info->scope()->num_heap_slots() - Context::MIN_CONTEXT_SLOTS;
  if (heap_slots > 0) {
    Comment cmnt(masm_, "[ Allocate context");
    bool need_write_barrier = true;
    // Argument to NewContext is the function, which is still in rdi.
    if (FLAG_harmony_scoping && info->scope()->is_global_scope()) {
      __ Push(rdi);
      __ Push(info->scope()->GetScopeInfo());
      __ CallRuntime(Runtime::kNewGlobalContext, 2);
    } else if (heap_slots <= FastNewContextStub::kMaximumSlots) {
      FastNewContextStub stub(isolate(), heap_slots);
      __ CallStub(&stub);
      // Result of FastNewContextStub is always in new space.
      need_write_barrier = false;
    } else {
      __ Push(rdi);
      __ CallRuntime(Runtime::kNewFunctionContext, 1);
    }
    function_in_register = false;
    // Context is returned in rax.  It replaces the context passed to us.
    // It's saved in the stack and kept live in rsi.
    __ movp(rsi, rax);
    __ movp(Operand(rbp, StandardFrameConstants::kContextOffset), rax);

    // Copy any necessary parameters into the context.
    int num_parameters = info->scope()->num_parameters();
    for (int i = 0; i < num_parameters; i++) {
      Variable* var = scope()->parameter(i);
      if (var->IsContextSlot()) {
        int parameter_offset = StandardFrameConstants::kCallerSPOffset +
            (num_parameters - 1 - i) * kPointerSize;
        // Load parameter from stack.
        __ movp(rax, Operand(rbp, parameter_offset));
        // Store it in the context.
        int context_offset = Context::SlotOffset(var->index());
        __ movp(Operand(rsi, context_offset), rax);
        // Update the write barrier.  This clobbers rax and rbx.
        if (need_write_barrier) {
          __ RecordWriteContextSlot(
              rsi, context_offset, rax, rbx, kDontSaveFPRegs);
        } else if (FLAG_debug_code) {
          Label done;
          __ JumpIfInNewSpace(rsi, rax, &done, Label::kNear);
          __ Abort(kExpectedNewSpaceObject);
          __ bind(&done);
        }
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
      __ Push(rdi);
    } else {
      __ Push(Operand(rbp, JavaScriptFrameConstants::kFunctionOffset));
    }
    // The receiver is just before the parameters on the caller's stack.
    int num_parameters = info->scope()->num_parameters();
    int offset = num_parameters * kPointerSize;
    __ leap(rdx,
           Operand(rbp, StandardFrameConstants::kCallerSPOffset + offset));
    __ Push(rdx);
    __ Push(Smi::FromInt(num_parameters));
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
       __ CompareRoot(rsp, Heap::kStackLimitRootIndex);
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
    __ LoadRoot(rax, Heap::kUndefinedValueRootIndex);
    EmitReturnSequence();
  }
}


void FullCodeGenerator::ClearAccumulator() {
  __ Set(rax, 0);
}


void FullCodeGenerator::EmitProfilingCounterDecrement(int delta) {
  __ Move(rbx, profiling_counter_, RelocInfo::EMBEDDED_OBJECT);
  __ SmiAddConstant(FieldOperand(rbx, Cell::kValueOffset),
                    Smi::FromInt(-delta));
}


void FullCodeGenerator::EmitProfilingCounterReset() {
  int reset_value = FLAG_interrupt_budget;
  __ Move(rbx, profiling_counter_, RelocInfo::EMBEDDED_OBJECT);
  __ Move(kScratchRegister, Smi::FromInt(reset_value));
  __ movp(FieldOperand(rbx, Cell::kValueOffset), kScratchRegister);
}


static const byte kJnsOffset = kPointerSize == kInt64Size ? 0x1d : 0x14;


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
  {
    PredictableCodeSizeScope predictible_code_size_scope(masm_, kJnsOffset);
    DontEmitDebugCodeScope dont_emit_debug_code_scope(masm_);
    __ call(isolate()->builtins()->InterruptCheck(), RelocInfo::CODE_TARGET);

    // Record a mapping of this PC offset to the OSR id.  This is used to find
    // the AST id from the unoptimized code in order to use it as a key into
    // the deoptimization input data found in the optimized code.
    RecordBackEdge(stmt->OsrEntryId());

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
      __ Push(rax);
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
    __ Push(rax);
    __ call(isolate()->builtins()->InterruptCheck(),
            RelocInfo::CODE_TARGET);
    __ Pop(rax);
    EmitProfilingCounterReset();
    __ bind(&ok);
#ifdef DEBUG
    // Add a label for checking the size of the code used for returning.
    Label check_exit_codesize;
    masm_->bind(&check_exit_codesize);
#endif
    CodeGenerator::RecordPositions(masm_, function()->end_position() - 1);
    __ RecordJSReturn();
    // Do not use the leave instruction here because it is too short to
    // patch with the code required by the debugger.
    __ movp(rsp, rbp);
    __ popq(rbp);
    int no_frame_start = masm_->pc_offset();

    int arguments_bytes = (info_->scope()->num_parameters() + 1) * kPointerSize;
    __ Ret(arguments_bytes, rcx);

    // Add padding that will be overwritten by a debugger breakpoint.  We
    // have just generated at least 7 bytes: "movp rsp, rbp; pop rbp; ret k"
    // (3 + 1 + 3) for x64 and at least 6 (2 + 1 + 3) bytes for x32.
    const int kPadding = Assembler::kJSReturnSequenceLength -
                         kPointerSize == kInt64Size ? 7 : 6;
    for (int i = 0; i < kPadding; ++i) {
      masm_->int3();
    }
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
  __ Push(operand);
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
  if (lit->IsSmi()) {
    __ SafeMove(result_register(), Smi::cast(*lit));
  } else {
    __ Move(result_register(), lit);
  }
}


void FullCodeGenerator::StackValueContext::Plug(Handle<Object> lit) const {
  if (lit->IsSmi()) {
    __ SafePush(Smi::cast(*lit));
  } else {
    __ Push(lit);
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
    __ Move(result_register(), lit);
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
  __ movp(Operand(rsp, 0), reg);
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
  DCHECK(materialize_true == true_label_);
  DCHECK(materialize_false == false_label_);
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
  Handle<Code> ic = ToBooleanStub::GetUninitialized(isolate());
  CallIC(ic, condition->test_id());
  __ testp(result_register(), result_register());
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
    offset += kFPOnStackSize + kPCOnStackSize +
              (info_->scope()->num_parameters() - 1) * kPointerSize;
  } else {
    offset += JavaScriptFrameConstants::kLocal0Offset;
  }
  return Operand(rbp, offset);
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
  __ movp(dest, location);
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
  __ movp(location, src);

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
  // The variable in the declaration always resides in the current context.
  DCHECK_EQ(0, scope()->ContextChainLength(variable->scope()));
  if (generate_debug_code_) {
    // Check that we're not inside a with or catch context.
    __ movp(rbx, FieldOperand(rsi, HeapObject::kMapOffset));
    __ CompareRoot(rbx, Heap::kWithContextMapRootIndex);
    __ Check(not_equal, kDeclarationInWithContext);
    __ CompareRoot(rbx, Heap::kCatchContextMapRootIndex);
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
                    : isolate()->factory()->undefined_value(),
                    zone());
      break;

    case Variable::PARAMETER:
    case Variable::LOCAL:
      if (hole_init) {
        Comment cmnt(masm_, "[ VariableDeclaration");
        __ LoadRoot(kScratchRegister, Heap::kTheHoleValueRootIndex);
        __ movp(StackOperand(variable), kScratchRegister);
      }
      break;

    case Variable::CONTEXT:
      if (hole_init) {
        Comment cmnt(masm_, "[ VariableDeclaration");
        EmitDebugCheckDeclarationContext(variable);
        __ LoadRoot(kScratchRegister, Heap::kTheHoleValueRootIndex);
        __ movp(ContextOperand(rsi, variable->index()), kScratchRegister);
        // No write barrier since the hole value is in old space.
        PrepareForBailoutForId(proxy->id(), NO_REGISTERS);
      }
      break;

    case Variable::LOOKUP: {
      Comment cmnt(masm_, "[ VariableDeclaration");
      __ Push(rsi);
      __ Push(variable->name());
      // Declaration nodes are always introduced in one of four modes.
      DCHECK(IsDeclaredVariableMode(mode));
      PropertyAttributes attr =
          IsImmutableVariableMode(mode) ? READ_ONLY : NONE;
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
      __ movp(StackOperand(variable), result_register());
      break;
    }

    case Variable::CONTEXT: {
      Comment cmnt(masm_, "[ FunctionDeclaration");
      EmitDebugCheckDeclarationContext(variable);
      VisitForAccumulatorValue(declaration->fun());
      __ movp(ContextOperand(rsi, variable->index()), result_register());
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
      __ Push(rsi);
      __ Push(variable->name());
      __ Push(Smi::FromInt(NONE));
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
  __ LoadContext(rax, scope_->ContextChainLength(scope_->GlobalScope()));
  __ movp(rax, ContextOperand(rax, variable->interface()->Index()));
  __ movp(rax, ContextOperand(rax, Context::EXTENSION_INDEX));

  // Assign it.
  __ movp(ContextOperand(rsi, variable->index()), rax);
  // We know that we have written a module, which is not a smi.
  __ RecordWriteContextSlot(rsi,
                            Context::SlotOffset(variable->index()),
                            rax,
                            rcx,
                            kDontSaveFPRegs,
                            EMIT_REMEMBERED_SET,
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
  __ Push(rsi);  // The context is the first argument.
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
    __ movp(rdx, Operand(rsp, 0));  // Switch value.
    bool inline_smi_code = ShouldInlineSmiCase(Token::EQ_STRICT);
    JumpPatchSite patch_site(masm_);
    if (inline_smi_code) {
      Label slow_case;
      __ movp(rcx, rdx);
      __ orp(rcx, rax);
      patch_site.EmitJumpIfNotSmi(rcx, &slow_case, Label::kNear);

      __ cmpp(rdx, rax);
      __ j(not_equal, &next_test);
      __ Drop(1);  // Switch value is no longer needed.
      __ jmp(clause->body_target());
      __ bind(&slow_case);
    }

    // Record position before stub call for type feedback.
    SetSourcePosition(clause->position());
    Handle<Code> ic = CompareIC::GetUninitialized(isolate(), Token::EQ_STRICT);
    CallIC(ic, clause->CompareId());
    patch_site.EmitPatchInfo();

    Label skip;
    __ jmp(&skip, Label::kNear);
    PrepareForBailout(clause, TOS_REG);
    __ CompareRoot(rax, Heap::kTrueValueRootIndex);
    __ j(not_equal, &next_test);
    __ Drop(1);
    __ jmp(clause->body_target());
    __ bind(&skip);

    __ testp(rax, rax);
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
  int slot = stmt->ForInFeedbackSlot();
  SetStatementPosition(stmt);

  Label loop, exit;
  ForIn loop_statement(this, stmt);
  increment_loop_depth();

  // Get the object to enumerate over. If the object is null or undefined, skip
  // over the loop.  See ECMA-262 version 5, section 12.6.4.
  VisitForAccumulatorValue(stmt->enumerable());
  __ CompareRoot(rax, Heap::kUndefinedValueRootIndex);
  __ j(equal, &exit);
  Register null_value = rdi;
  __ LoadRoot(null_value, Heap::kNullValueRootIndex);
  __ cmpp(rax, null_value);
  __ j(equal, &exit);

  PrepareForBailoutForId(stmt->PrepareId(), TOS_REG);

  // Convert the object to a JS object.
  Label convert, done_convert;
  __ JumpIfSmi(rax, &convert);
  __ CmpObjectType(rax, FIRST_SPEC_OBJECT_TYPE, rcx);
  __ j(above_equal, &done_convert);
  __ bind(&convert);
  __ Push(rax);
  __ InvokeBuiltin(Builtins::TO_OBJECT, CALL_FUNCTION);
  __ bind(&done_convert);
  __ Push(rax);

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
  __ movp(rax, FieldOperand(rax, HeapObject::kMapOffset));
  __ jmp(&use_cache, Label::kNear);

  // Get the set of properties to enumerate.
  __ bind(&call_runtime);
  __ Push(rax);  // Duplicate the enumerable object on the stack.
  __ CallRuntime(Runtime::kGetPropertyNamesFast, 1);

  // If we got a map from the runtime call, we can do a fast
  // modification check. Otherwise, we got a fixed array, and we have
  // to do a slow check.
  Label fixed_array;
  __ CompareRoot(FieldOperand(rax, HeapObject::kMapOffset),
                 Heap::kMetaMapRootIndex);
  __ j(not_equal, &fixed_array);

  // We got a map in register rax. Get the enumeration cache from it.
  __ bind(&use_cache);

  Label no_descriptors;

  __ EnumLength(rdx, rax);
  __ Cmp(rdx, Smi::FromInt(0));
  __ j(equal, &no_descriptors);

  __ LoadInstanceDescriptors(rax, rcx);
  __ movp(rcx, FieldOperand(rcx, DescriptorArray::kEnumCacheOffset));
  __ movp(rcx, FieldOperand(rcx, DescriptorArray::kEnumCacheBridgeCacheOffset));

  // Set up the four remaining stack slots.
  __ Push(rax);  // Map.
  __ Push(rcx);  // Enumeration cache.
  __ Push(rdx);  // Number of valid entries for the map in the enum cache.
  __ Push(Smi::FromInt(0));  // Initial index.
  __ jmp(&loop);

  __ bind(&no_descriptors);
  __ addp(rsp, Immediate(kPointerSize));
  __ jmp(&exit);

  // We got a fixed array in register rax. Iterate through that.
  Label non_proxy;
  __ bind(&fixed_array);

  // No need for a write barrier, we are storing a Smi in the feedback vector.
  __ Move(rbx, FeedbackVector());
  __ Move(FieldOperand(rbx, FixedArray::OffsetOfElementAt(slot)),
          TypeFeedbackInfo::MegamorphicSentinel(isolate()));
  __ Move(rbx, Smi::FromInt(1));  // Smi indicates slow check
  __ movp(rcx, Operand(rsp, 0 * kPointerSize));  // Get enumerated object
  STATIC_ASSERT(FIRST_JS_PROXY_TYPE == FIRST_SPEC_OBJECT_TYPE);
  __ CmpObjectType(rcx, LAST_JS_PROXY_TYPE, rcx);
  __ j(above, &non_proxy);
  __ Move(rbx, Smi::FromInt(0));  // Zero indicates proxy
  __ bind(&non_proxy);
  __ Push(rbx);  // Smi
  __ Push(rax);  // Array
  __ movp(rax, FieldOperand(rax, FixedArray::kLengthOffset));
  __ Push(rax);  // Fixed array length (as smi).
  __ Push(Smi::FromInt(0));  // Initial index.

  // Generate code for doing the condition check.
  PrepareForBailoutForId(stmt->BodyId(), NO_REGISTERS);
  __ bind(&loop);
  __ movp(rax, Operand(rsp, 0 * kPointerSize));  // Get the current index.
  __ cmpp(rax, Operand(rsp, 1 * kPointerSize));  // Compare to the array length.
  __ j(above_equal, loop_statement.break_label());

  // Get the current entry of the array into register rbx.
  __ movp(rbx, Operand(rsp, 2 * kPointerSize));
  SmiIndex index = masm()->SmiToIndex(rax, rax, kPointerSizeLog2);
  __ movp(rbx, FieldOperand(rbx,
                            index.reg,
                            index.scale,
                            FixedArray::kHeaderSize));

  // Get the expected map from the stack or a smi in the
  // permanent slow case into register rdx.
  __ movp(rdx, Operand(rsp, 3 * kPointerSize));

  // Check if the expected map still matches that of the enumerable.
  // If not, we may have to filter the key.
  Label update_each;
  __ movp(rcx, Operand(rsp, 4 * kPointerSize));
  __ cmpp(rdx, FieldOperand(rcx, HeapObject::kMapOffset));
  __ j(equal, &update_each, Label::kNear);

  // For proxies, no filtering is done.
  // TODO(rossberg): What if only a prototype is a proxy? Not specified yet.
  __ Cmp(rdx, Smi::FromInt(0));
  __ j(equal, &update_each, Label::kNear);

  // Convert the entry to a string or null if it isn't a property
  // anymore. If the property has been removed while iterating, we
  // just skip it.
  __ Push(rcx);  // Enumerable.
  __ Push(rbx);  // Current entry.
  __ InvokeBuiltin(Builtins::FILTER_KEY, CALL_FUNCTION);
  __ Cmp(rax, Smi::FromInt(0));
  __ j(equal, loop_statement.continue_label());
  __ movp(rbx, rax);

  // Update the 'each' property or variable from the possibly filtered
  // entry in register rbx.
  __ bind(&update_each);
  __ movp(result_register(), rbx);
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

  EmitBackEdgeBookkeeping(stmt, &loop);
  __ jmp(&loop);

  // Remove the pointers stored on the stack.
  __ bind(loop_statement.break_label());
  __ addp(rsp, Immediate(5 * kPointerSize));

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
    FastNewClosureStub stub(isolate(),
                            info->strict_mode(),
                            info->is_generator());
    __ Move(rbx, info);
    __ CallStub(&stub);
  } else {
    __ Push(rsi);
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


void FullCodeGenerator::EmitLoadGlobalCheckExtensions(VariableProxy* proxy,
                                                      TypeofState typeof_state,
                                                      Label* slow) {
  Register context = rsi;
  Register temp = rdx;

  Scope* s = scope();
  while (s != NULL) {
    if (s->num_heap_slots() > 0) {
      if (s->calls_sloppy_eval()) {
        // Check that extension is NULL.
        __ cmpp(ContextOperand(context, Context::EXTENSION_INDEX),
                Immediate(0));
        __ j(not_equal, slow);
      }
      // Load next context in chain.
      __ movp(temp, ContextOperand(context, Context::PREVIOUS_INDEX));
      // Walk the rest of the chain without clobbering rsi.
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
      __ movp(temp, context);
    }
    // Load map for comparison into register, outside loop.
    __ LoadRoot(kScratchRegister, Heap::kNativeContextMapRootIndex);
    __ bind(&next);
    // Terminate at native context.
    __ cmpp(kScratchRegister, FieldOperand(temp, HeapObject::kMapOffset));
    __ j(equal, &fast, Label::kNear);
    // Check that extension is NULL.
    __ cmpp(ContextOperand(temp, Context::EXTENSION_INDEX), Immediate(0));
    __ j(not_equal, slow);
    // Load next context in chain.
    __ movp(temp, ContextOperand(temp, Context::PREVIOUS_INDEX));
    __ jmp(&next);
    __ bind(&fast);
  }

  // All extension objects were empty and it is safe to use a global
  // load IC call.
  __ movp(LoadIC::ReceiverRegister(), GlobalObjectOperand());
  __ Move(LoadIC::NameRegister(), proxy->var()->name());
  if (FLAG_vector_ics) {
    __ Move(LoadIC::SlotRegister(),
            Smi::FromInt(proxy->VariableFeedbackSlot()));
  }

  ContextualMode mode = (typeof_state == INSIDE_TYPEOF)
      ? NOT_CONTEXTUAL
      : CONTEXTUAL;
  CallLoadIC(mode);
}


MemOperand FullCodeGenerator::ContextSlotOperandCheckExtensions(Variable* var,
                                                                Label* slow) {
  DCHECK(var->IsContextSlot());
  Register context = rsi;
  Register temp = rbx;

  for (Scope* s = scope(); s != var->scope(); s = s->outer_scope()) {
    if (s->num_heap_slots() > 0) {
      if (s->calls_sloppy_eval()) {
        // Check that extension is NULL.
        __ cmpp(ContextOperand(context, Context::EXTENSION_INDEX),
                Immediate(0));
        __ j(not_equal, slow);
      }
      __ movp(temp, ContextOperand(context, Context::PREVIOUS_INDEX));
      // Walk the rest of the chain without clobbering rsi.
      context = temp;
    }
  }
  // Check that last extension is NULL.
  __ cmpp(ContextOperand(context, Context::EXTENSION_INDEX), Immediate(0));
  __ j(not_equal, slow);

  // This function is used only for loads, not stores, so it's safe to
  // return an rsi-based operand (the write barrier cannot be allowed to
  // destroy the rsi register).
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
    __ movp(rax, ContextSlotOperandCheckExtensions(local, slow));
    if (local->mode() == LET || local->mode() == CONST ||
        local->mode() == CONST_LEGACY) {
      __ CompareRoot(rax, Heap::kTheHoleValueRootIndex);
      __ j(not_equal, done);
      if (local->mode() == CONST_LEGACY) {
        __ LoadRoot(rax, Heap::kUndefinedValueRootIndex);
      } else {  // LET || CONST
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
      Comment cmnt(masm_, "[ Global variable");
      __ Move(LoadIC::NameRegister(), var->name());
      __ movp(LoadIC::ReceiverRegister(), GlobalObjectOperand());
      if (FLAG_vector_ics) {
        __ Move(LoadIC::SlotRegister(),
                Smi::FromInt(proxy->VariableFeedbackSlot()));
      }
      CallLoadIC(CONTEXTUAL);
      context()->Plug(rax);
      break;
    }

    case Variable::PARAMETER:
    case Variable::LOCAL:
    case Variable::CONTEXT: {
      Comment cmnt(masm_, var->IsContextSlot() ? "[ Context slot"
                                               : "[ Stack slot");
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
          GetVar(rax, var);
          __ CompareRoot(rax, Heap::kTheHoleValueRootIndex);
          __ j(not_equal, &done, Label::kNear);
          if (var->mode() == LET || var->mode() == CONST) {
            // Throw a reference error when using an uninitialized let/const
            // binding in harmony mode.
            __ Push(var->name());
            __ CallRuntime(Runtime::kThrowReferenceError, 1);
          } else {
            // Uninitalized const bindings outside of harmony mode are unholed.
            DCHECK(var->mode() == CONST_LEGACY);
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
      Comment cmnt(masm_, "[ Lookup slot");
      Label done, slow;
      // Generate code for loading from variables potentially shadowed
      // by eval-introduced variables.
      EmitDynamicLookupFastCase(proxy, NOT_INSIDE_TYPEOF, &slow, &done);
      __ bind(&slow);
      __ Push(rsi);  // Context.
      __ Push(var->name());
      __ CallRuntime(Runtime::kLoadLookupSlot, 2);
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
  __ movp(rdi, Operand(rbp, JavaScriptFrameConstants::kFunctionOffset));
  __ movp(rcx, FieldOperand(rdi, JSFunction::kLiteralsOffset));
  int literal_offset =
      FixedArray::kHeaderSize + expr->literal_index() * kPointerSize;
  __ movp(rbx, FieldOperand(rcx, literal_offset));
  __ CompareRoot(rbx, Heap::kUndefinedValueRootIndex);
  __ j(not_equal, &materialized, Label::kNear);

  // Create regexp literal using runtime function
  // Result will be in rax.
  __ Push(rcx);
  __ Push(Smi::FromInt(expr->literal_index()));
  __ Push(expr->pattern());
  __ Push(expr->flags());
  __ CallRuntime(Runtime::kMaterializeRegExpLiteral, 4);
  __ movp(rbx, rax);

  __ bind(&materialized);
  int size = JSRegExp::kSize + JSRegExp::kInObjectFieldCount * kPointerSize;
  Label allocated, runtime_allocate;
  __ Allocate(size, rax, rcx, rdx, &runtime_allocate, TAG_OBJECT);
  __ jmp(&allocated);

  __ bind(&runtime_allocate);
  __ Push(rbx);
  __ Push(Smi::FromInt(size));
  __ CallRuntime(Runtime::kAllocateInNewSpace, 1);
  __ Pop(rbx);

  __ bind(&allocated);
  // Copy the content into the newly allocated memory.
  // (Unroll copy loop once for better throughput).
  for (int i = 0; i < size - kPointerSize; i += 2 * kPointerSize) {
    __ movp(rdx, FieldOperand(rbx, i));
    __ movp(rcx, FieldOperand(rbx, i + kPointerSize));
    __ movp(FieldOperand(rax, i), rdx);
    __ movp(FieldOperand(rax, i + kPointerSize), rcx);
  }
  if ((size % (2 * kPointerSize)) != 0) {
    __ movp(rdx, FieldOperand(rbx, size - kPointerSize));
    __ movp(FieldOperand(rax, size - kPointerSize), rdx);
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
      masm()->serializer_enabled() || flags != ObjectLiteral::kFastElements ||
      properties_count > FastCloneShallowObjectStub::kMaximumClonedProperties) {
    __ movp(rdi, Operand(rbp, JavaScriptFrameConstants::kFunctionOffset));
    __ Push(FieldOperand(rdi, JSFunction::kLiteralsOffset));
    __ Push(Smi::FromInt(expr->literal_index()));
    __ Push(constant_properties);
    __ Push(Smi::FromInt(flags));
    __ CallRuntime(Runtime::kCreateObjectLiteral, 4);
  } else {
    __ movp(rdi, Operand(rbp, JavaScriptFrameConstants::kFunctionOffset));
    __ movp(rax, FieldOperand(rdi, JSFunction::kLiteralsOffset));
    __ Move(rbx, Smi::FromInt(expr->literal_index()));
    __ Move(rcx, constant_properties);
    __ Move(rdx, Smi::FromInt(flags));
    FastCloneShallowObjectStub stub(isolate(), properties_count);
    __ CallStub(&stub);
  }

  // If result_saved is true the result is on top of the stack.  If
  // result_saved is false the result is in rax.
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
      __ Push(rax);  // Save result on the stack
      result_saved = true;
    }
    switch (property->kind()) {
      case ObjectLiteral::Property::CONSTANT:
        UNREACHABLE();
      case ObjectLiteral::Property::MATERIALIZED_LITERAL:
        DCHECK(!CompileTimeValue::IsCompileTimeValue(value));
        // Fall through.
      case ObjectLiteral::Property::COMPUTED:
        if (key->value()->IsInternalizedString()) {
          if (property->emit_store()) {
            VisitForAccumulatorValue(value);
            DCHECK(StoreIC::ValueRegister().is(rax));
            __ Move(StoreIC::NameRegister(), key->value());
            __ movp(StoreIC::ReceiverRegister(), Operand(rsp, 0));
            CallStoreIC(key->LiteralFeedbackId());
            PrepareForBailoutForId(key->id(), NO_REGISTERS);
          } else {
            VisitForEffect(value);
          }
          break;
        }
        __ Push(Operand(rsp, 0));  // Duplicate receiver.
        VisitForStackValue(key);
        VisitForStackValue(value);
        if (property->emit_store()) {
          __ Push(Smi::FromInt(SLOPPY));  // Strict mode
          __ CallRuntime(Runtime::kSetProperty, 4);
        } else {
          __ Drop(3);
        }
        break;
      case ObjectLiteral::Property::PROTOTYPE:
        __ Push(Operand(rsp, 0));  // Duplicate receiver.
        VisitForStackValue(value);
        if (property->emit_store()) {
          __ CallRuntime(Runtime::kSetPrototype, 2);
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
    __ Push(Operand(rsp, 0));  // Duplicate receiver.
    VisitForStackValue(it->first);
    EmitAccessor(it->second->getter);
    EmitAccessor(it->second->setter);
    __ Push(Smi::FromInt(NONE));
    __ CallRuntime(Runtime::kDefineAccessorPropertyUnchecked, 5);
  }

  if (expr->has_function()) {
    DCHECK(result_saved);
    __ Push(Operand(rsp, 0));
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
    __ movp(rbx, Operand(rbp, JavaScriptFrameConstants::kFunctionOffset));
    __ Push(FieldOperand(rbx, JSFunction::kLiteralsOffset));
    __ Push(Smi::FromInt(expr->literal_index()));
    __ Push(constant_elements);
    __ Push(Smi::FromInt(flags));
    __ CallRuntime(Runtime::kCreateArrayLiteral, 4);
  } else {
    __ movp(rbx, Operand(rbp, JavaScriptFrameConstants::kFunctionOffset));
    __ movp(rax, FieldOperand(rbx, JSFunction::kLiteralsOffset));
    __ Move(rbx, Smi::FromInt(expr->literal_index()));
    __ Move(rcx, constant_elements);
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
      __ Push(rax);  // array literal
      __ Push(Smi::FromInt(expr->literal_index()));
      result_saved = true;
    }
    VisitForAccumulatorValue(subexpr);

    if (IsFastObjectElementsKind(constant_elements_kind)) {
      // Fast-case array literal with ElementsKind of FAST_*_ELEMENTS, they
      // cannot transition and don't need to call the runtime stub.
      int offset = FixedArray::kHeaderSize + (i * kPointerSize);
      __ movp(rbx, Operand(rsp, kPointerSize));  // Copy of array literal.
      __ movp(rbx, FieldOperand(rbx, JSObject::kElementsOffset));
      // Store the subexpression value in the array's elements.
      __ movp(FieldOperand(rbx, offset), result_register());
      // Update the write barrier for the array store.
      __ RecordWriteField(rbx, offset, result_register(), rcx,
                          kDontSaveFPRegs,
                          EMIT_REMEMBERED_SET,
                          INLINE_SMI_CHECK);
    } else {
      // Store the subexpression value in the array's elements.
      __ Move(rcx, Smi::FromInt(i));
      StoreArrayLiteralElementStub stub(isolate());
      __ CallStub(&stub);
    }

    PrepareForBailoutForId(expr->GetIdForElement(i), NO_REGISTERS);
  }

  if (result_saved) {
    __ addp(rsp, Immediate(kPointerSize));  // literal index
    context()->PlugTOS();
  } else {
    context()->Plug(rax);
  }
}


void FullCodeGenerator::VisitAssignment(Assignment* expr) {
  DCHECK(expr->target()->IsValidReferenceExpression());

  Comment cmnt(masm_, "[ Assignment");

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
        // We need the receiver both on the stack and in the register.
        VisitForStackValue(property->obj());
        __ movp(LoadIC::ReceiverRegister(), Operand(rsp, 0));
      } else {
        VisitForStackValue(property->obj());
      }
      break;
    case KEYED_PROPERTY: {
      if (expr->is_compound()) {
        VisitForStackValue(property->obj());
        VisitForStackValue(property->key());
        __ movp(LoadIC::ReceiverRegister(), Operand(rsp, kPointerSize));
        __ movp(LoadIC::NameRegister(), Operand(rsp, 0));
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
          PrepareForBailoutForId(property->LoadId(), TOS_REG);
          break;
        case KEYED_PROPERTY:
          EmitKeyedPropertyLoad(property);
          PrepareForBailoutForId(property->LoadId(), TOS_REG);
          break;
      }
    }

    Token::Value op = expr->binary_op();
    __ Push(rax);  // Left operand goes on the stack.
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


void FullCodeGenerator::VisitYield(Yield* expr) {
  Comment cmnt(masm_, "[ Yield");
  // Evaluate yielded value first; the initial iterator definition depends on
  // this.  It stays on the stack while we update the iterator.
  VisitForStackValue(expr->expression());

  switch (expr->yield_kind()) {
    case Yield::SUSPEND:
      // Pop value from top-of-stack slot; box result into result register.
      EmitCreateIteratorResult(false);
      __ Push(result_register());
      // Fall through.
    case Yield::INITIAL: {
      Label suspend, continuation, post_runtime, resume;

      __ jmp(&suspend);

      __ bind(&continuation);
      __ jmp(&resume);

      __ bind(&suspend);
      VisitForAccumulatorValue(expr->generator_object());
      DCHECK(continuation.pos() > 0 && Smi::IsValid(continuation.pos()));
      __ Move(FieldOperand(rax, JSGeneratorObject::kContinuationOffset),
              Smi::FromInt(continuation.pos()));
      __ movp(FieldOperand(rax, JSGeneratorObject::kContextOffset), rsi);
      __ movp(rcx, rsi);
      __ RecordWriteField(rax, JSGeneratorObject::kContextOffset, rcx, rdx,
                          kDontSaveFPRegs);
      __ leap(rbx, Operand(rbp, StandardFrameConstants::kExpressionsOffset));
      __ cmpp(rsp, rbx);
      __ j(equal, &post_runtime);
      __ Push(rax);  // generator object
      __ CallRuntime(Runtime::kSuspendJSGeneratorObject, 1);
      __ movp(context_register(),
              Operand(rbp, StandardFrameConstants::kContextOffset));
      __ bind(&post_runtime);

      __ Pop(result_register());
      EmitReturnSequence();

      __ bind(&resume);
      context()->Plug(result_register());
      break;
    }

    case Yield::FINAL: {
      VisitForAccumulatorValue(expr->generator_object());
      __ Move(FieldOperand(result_register(),
                           JSGeneratorObject::kContinuationOffset),
              Smi::FromInt(JSGeneratorObject::kGeneratorClosed));
      // Pop value from top-of-stack slot, box result into result register.
      EmitCreateIteratorResult(true);
      EmitUnwindBeforeReturn();
      EmitReturnSequence();
      break;
    }

    case Yield::DELEGATING: {
      VisitForStackValue(expr->generator_object());

      // Initial stack layout is as follows:
      // [sp + 1 * kPointerSize] iter
      // [sp + 0 * kPointerSize] g

      Label l_catch, l_try, l_suspend, l_continuation, l_resume;
      Label l_next, l_call, l_loop;
      Register load_receiver = LoadIC::ReceiverRegister();
      Register load_name = LoadIC::NameRegister();

      // Initial send value is undefined.
      __ LoadRoot(rax, Heap::kUndefinedValueRootIndex);
      __ jmp(&l_next);

      // catch (e) { receiver = iter; f = 'throw'; arg = e; goto l_call; }
      __ bind(&l_catch);
      handler_table()->set(expr->index(), Smi::FromInt(l_catch.pos()));
      __ LoadRoot(load_name, Heap::kthrow_stringRootIndex);  // "throw"
      __ Push(load_name);
      __ Push(Operand(rsp, 2 * kPointerSize));               // iter
      __ Push(rax);                                          // exception
      __ jmp(&l_call);

      // try { received = %yield result }
      // Shuffle the received result above a try handler and yield it without
      // re-boxing.
      __ bind(&l_try);
      __ Pop(rax);                                       // result
      __ PushTryHandler(StackHandler::CATCH, expr->index());
      const int handler_size = StackHandlerConstants::kSize;
      __ Push(rax);                                      // result
      __ jmp(&l_suspend);
      __ bind(&l_continuation);
      __ jmp(&l_resume);
      __ bind(&l_suspend);
      const int generator_object_depth = kPointerSize + handler_size;
      __ movp(rax, Operand(rsp, generator_object_depth));
      __ Push(rax);                                      // g
      DCHECK(l_continuation.pos() > 0 && Smi::IsValid(l_continuation.pos()));
      __ Move(FieldOperand(rax, JSGeneratorObject::kContinuationOffset),
              Smi::FromInt(l_continuation.pos()));
      __ movp(FieldOperand(rax, JSGeneratorObject::kContextOffset), rsi);
      __ movp(rcx, rsi);
      __ RecordWriteField(rax, JSGeneratorObject::kContextOffset, rcx, rdx,
                          kDontSaveFPRegs);
      __ CallRuntime(Runtime::kSuspendJSGeneratorObject, 1);
      __ movp(context_register(),
              Operand(rbp, StandardFrameConstants::kContextOffset));
      __ Pop(rax);                                       // result
      EmitReturnSequence();
      __ bind(&l_resume);                                // received in rax
      __ PopTryHandler();

      // receiver = iter; f = 'next'; arg = received;
      __ bind(&l_next);

      __ LoadRoot(load_name, Heap::knext_stringRootIndex);
      __ Push(load_name);                           // "next"
      __ Push(Operand(rsp, 2 * kPointerSize));      // iter
      __ Push(rax);                                 // received

      // result = receiver[f](arg);
      __ bind(&l_call);
      __ movp(load_receiver, Operand(rsp, kPointerSize));
      if (FLAG_vector_ics) {
        __ Move(LoadIC::SlotRegister(),
                Smi::FromInt(expr->KeyedLoadFeedbackSlot()));
      }
      Handle<Code> ic = isolate()->builtins()->KeyedLoadIC_Initialize();
      CallIC(ic, TypeFeedbackId::None());
      __ movp(rdi, rax);
      __ movp(Operand(rsp, 2 * kPointerSize), rdi);
      CallFunctionStub stub(isolate(), 1, CALL_AS_METHOD);
      __ CallStub(&stub);

      __ movp(rsi, Operand(rbp, StandardFrameConstants::kContextOffset));
      __ Drop(1);  // The function is still on the stack; drop it.

      // if (!result.done) goto l_try;
      __ bind(&l_loop);
      __ Move(load_receiver, rax);
      __ Push(load_receiver);                               // save result
      __ LoadRoot(load_name, Heap::kdone_stringRootIndex);  // "done"
      if (FLAG_vector_ics) {
        __ Move(LoadIC::SlotRegister(), Smi::FromInt(expr->DoneFeedbackSlot()));
      }
      CallLoadIC(NOT_CONTEXTUAL);                           // rax=result.done
      Handle<Code> bool_ic = ToBooleanStub::GetUninitialized(isolate());
      CallIC(bool_ic);
      __ testp(result_register(), result_register());
      __ j(zero, &l_try);

      // result.value
      __ Pop(load_receiver);                             // result
      __ LoadRoot(load_name, Heap::kvalue_stringRootIndex);  // "value"
      if (FLAG_vector_ics) {
        __ Move(LoadIC::SlotRegister(),
                Smi::FromInt(expr->ValueFeedbackSlot()));
      }
      CallLoadIC(NOT_CONTEXTUAL);                        // result.value in rax
      context()->DropAndPlug(2, rax);                    // drop iter and g
      break;
    }
  }
}


void FullCodeGenerator::EmitGeneratorResume(Expression *generator,
    Expression *value,
    JSGeneratorObject::ResumeMode resume_mode) {
  // The value stays in rax, and is ultimately read by the resumed generator, as
  // if CallRuntime(Runtime::kSuspendJSGeneratorObject) returned it. Or it
  // is read to throw the value when the resumed generator is already closed.
  // rbx will hold the generator object until the activation has been resumed.
  VisitForStackValue(generator);
  VisitForAccumulatorValue(value);
  __ Pop(rbx);

  // Check generator state.
  Label wrong_state, closed_state, done;
  STATIC_ASSERT(JSGeneratorObject::kGeneratorExecuting < 0);
  STATIC_ASSERT(JSGeneratorObject::kGeneratorClosed == 0);
  __ SmiCompare(FieldOperand(rbx, JSGeneratorObject::kContinuationOffset),
                Smi::FromInt(0));
  __ j(equal, &closed_state);
  __ j(less, &wrong_state);

  // Load suspended function and context.
  __ movp(rsi, FieldOperand(rbx, JSGeneratorObject::kContextOffset));
  __ movp(rdi, FieldOperand(rbx, JSGeneratorObject::kFunctionOffset));

  // Push receiver.
  __ Push(FieldOperand(rbx, JSGeneratorObject::kReceiverOffset));

  // Push holes for arguments to generator function.
  __ movp(rdx, FieldOperand(rdi, JSFunction::kSharedFunctionInfoOffset));
  __ LoadSharedFunctionInfoSpecialField(rdx, rdx,
      SharedFunctionInfo::kFormalParameterCountOffset);
  __ LoadRoot(rcx, Heap::kTheHoleValueRootIndex);
  Label push_argument_holes, push_frame;
  __ bind(&push_argument_holes);
  __ subp(rdx, Immediate(1));
  __ j(carry, &push_frame);
  __ Push(rcx);
  __ jmp(&push_argument_holes);

  // Enter a new JavaScript frame, and initialize its slots as they were when
  // the generator was suspended.
  Label resume_frame;
  __ bind(&push_frame);
  __ call(&resume_frame);
  __ jmp(&done);
  __ bind(&resume_frame);
  __ pushq(rbp);  // Caller's frame pointer.
  __ movp(rbp, rsp);
  __ Push(rsi);  // Callee's context.
  __ Push(rdi);  // Callee's JS Function.

  // Load the operand stack size.
  __ movp(rdx, FieldOperand(rbx, JSGeneratorObject::kOperandStackOffset));
  __ movp(rdx, FieldOperand(rdx, FixedArray::kLengthOffset));
  __ SmiToInteger32(rdx, rdx);

  // If we are sending a value and there is no operand stack, we can jump back
  // in directly.
  if (resume_mode == JSGeneratorObject::NEXT) {
    Label slow_resume;
    __ cmpp(rdx, Immediate(0));
    __ j(not_zero, &slow_resume);
    __ movp(rdx, FieldOperand(rdi, JSFunction::kCodeEntryOffset));
    __ SmiToInteger64(rcx,
        FieldOperand(rbx, JSGeneratorObject::kContinuationOffset));
    __ addp(rdx, rcx);
    __ Move(FieldOperand(rbx, JSGeneratorObject::kContinuationOffset),
            Smi::FromInt(JSGeneratorObject::kGeneratorExecuting));
    __ jmp(rdx);
    __ bind(&slow_resume);
  }

  // Otherwise, we push holes for the operand stack and call the runtime to fix
  // up the stack and the handlers.
  Label push_operand_holes, call_resume;
  __ bind(&push_operand_holes);
  __ subp(rdx, Immediate(1));
  __ j(carry, &call_resume);
  __ Push(rcx);
  __ jmp(&push_operand_holes);
  __ bind(&call_resume);
  __ Push(rbx);
  __ Push(result_register());
  __ Push(Smi::FromInt(resume_mode));
  __ CallRuntime(Runtime::kResumeJSGeneratorObject, 3);
  // Not reached: the runtime call returns elsewhere.
  __ Abort(kGeneratorFailedToResume);

  // Reach here when generator is closed.
  __ bind(&closed_state);
  if (resume_mode == JSGeneratorObject::NEXT) {
    // Return completed iterator result when generator is closed.
    __ PushRoot(Heap::kUndefinedValueRootIndex);
    // Pop value from top-of-stack slot; box result into result register.
    EmitCreateIteratorResult(true);
  } else {
    // Throw the provided value.
    __ Push(rax);
    __ CallRuntime(Runtime::kThrow, 1);
  }
  __ jmp(&done);

  // Throw error if we attempt to operate on a running generator.
  __ bind(&wrong_state);
  __ Push(rbx);
  __ CallRuntime(Runtime::kThrowGeneratorStateError, 1);

  __ bind(&done);
  context()->Plug(result_register());
}


void FullCodeGenerator::EmitCreateIteratorResult(bool done) {
  Label gc_required;
  Label allocated;

  Handle<Map> map(isolate()->native_context()->iterator_result_map());

  __ Allocate(map->instance_size(), rax, rcx, rdx, &gc_required, TAG_OBJECT);
  __ jmp(&allocated);

  __ bind(&gc_required);
  __ Push(Smi::FromInt(map->instance_size()));
  __ CallRuntime(Runtime::kAllocateInNewSpace, 1);
  __ movp(context_register(),
          Operand(rbp, StandardFrameConstants::kContextOffset));

  __ bind(&allocated);
  __ Move(rbx, map);
  __ Pop(rcx);
  __ Move(rdx, isolate()->factory()->ToBoolean(done));
  DCHECK_EQ(map->instance_size(), 5 * kPointerSize);
  __ movp(FieldOperand(rax, HeapObject::kMapOffset), rbx);
  __ Move(FieldOperand(rax, JSObject::kPropertiesOffset),
          isolate()->factory()->empty_fixed_array());
  __ Move(FieldOperand(rax, JSObject::kElementsOffset),
          isolate()->factory()->empty_fixed_array());
  __ movp(FieldOperand(rax, JSGeneratorObject::kResultValuePropertyOffset),
          rcx);
  __ movp(FieldOperand(rax, JSGeneratorObject::kResultDonePropertyOffset),
          rdx);

  // Only the value field needs a write barrier, as the other values are in the
  // root set.
  __ RecordWriteField(rax, JSGeneratorObject::kResultValuePropertyOffset,
                      rcx, rdx, kDontSaveFPRegs);
}


void FullCodeGenerator::EmitNamedPropertyLoad(Property* prop) {
  SetSourcePosition(prop->position());
  Literal* key = prop->key()->AsLiteral();
  __ Move(LoadIC::NameRegister(), key->value());
  if (FLAG_vector_ics) {
    __ Move(LoadIC::SlotRegister(), Smi::FromInt(prop->PropertyFeedbackSlot()));
    CallLoadIC(NOT_CONTEXTUAL);
  } else {
    CallLoadIC(NOT_CONTEXTUAL, prop->PropertyFeedbackId());
  }
}


void FullCodeGenerator::EmitKeyedPropertyLoad(Property* prop) {
  SetSourcePosition(prop->position());
  Handle<Code> ic = isolate()->builtins()->KeyedLoadIC_Initialize();
  if (FLAG_vector_ics) {
    __ Move(LoadIC::SlotRegister(), Smi::FromInt(prop->PropertyFeedbackSlot()));
    CallIC(ic);
  } else {
    CallIC(ic, prop->PropertyFeedbackId());
  }
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
  __ Pop(rdx);
  __ movp(rcx, rax);
  __ orp(rax, rdx);
  JumpPatchSite patch_site(masm_);
  patch_site.EmitJumpIfSmi(rax, &smi_case, Label::kNear);

  __ bind(&stub_call);
  __ movp(rax, rcx);
  BinaryOpICStub stub(isolate(), op, mode);
  CallIC(stub.GetCode(), expr->BinaryOperationFeedbackId());
  patch_site.EmitPatchInfo();
  __ jmp(&done, Label::kNear);

  __ bind(&smi_case);
  switch (op) {
    case Token::SAR:
      __ SmiShiftArithmeticRight(rax, rdx, rcx);
      break;
    case Token::SHL:
      __ SmiShiftLeft(rax, rdx, rcx, &stub_call);
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
  __ Pop(rdx);
  BinaryOpICStub stub(isolate(), op, mode);
  JumpPatchSite patch_site(masm_);    // unbound, signals no inlined smi code.
  CallIC(stub.GetCode(), expr->BinaryOperationFeedbackId());
  patch_site.EmitPatchInfo();
  context()->Plug(rax);
}


void FullCodeGenerator::EmitAssignment(Expression* expr) {
  DCHECK(expr->IsValidReferenceExpression());

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
      __ Push(rax);  // Preserve value.
      VisitForAccumulatorValue(prop->obj());
      __ Move(StoreIC::ReceiverRegister(), rax);
      __ Pop(StoreIC::ValueRegister());  // Restore value.
      __ Move(StoreIC::NameRegister(), prop->key()->AsLiteral()->value());
      CallStoreIC();
      break;
    }
    case KEYED_PROPERTY: {
      __ Push(rax);  // Preserve value.
      VisitForStackValue(prop->obj());
      VisitForAccumulatorValue(prop->key());
      __ Move(KeyedStoreIC::NameRegister(), rax);
      __ Pop(KeyedStoreIC::ReceiverRegister());
      __ Pop(KeyedStoreIC::ValueRegister());  // Restore value.
      Handle<Code> ic = strict_mode() == SLOPPY
          ? isolate()->builtins()->KeyedStoreIC_Initialize()
          : isolate()->builtins()->KeyedStoreIC_Initialize_Strict();
      CallIC(ic);
      break;
    }
  }
  context()->Plug(rax);
}


void FullCodeGenerator::EmitStoreToStackLocalOrContextSlot(
    Variable* var, MemOperand location) {
  __ movp(location, rax);
  if (var->IsContextSlot()) {
    __ movp(rdx, rax);
    __ RecordWriteContextSlot(
        rcx, Context::SlotOffset(var->index()), rdx, rbx, kDontSaveFPRegs);
  }
}


void FullCodeGenerator::EmitVariableAssignment(Variable* var,
                                               Token::Value op) {
  if (var->IsUnallocated()) {
    // Global var, const, or let.
    __ Move(StoreIC::NameRegister(), var->name());
    __ movp(StoreIC::ReceiverRegister(), GlobalObjectOperand());
    CallStoreIC();

  } else if (op == Token::INIT_CONST_LEGACY) {
    // Const initializers need a write barrier.
    DCHECK(!var->IsParameter());  // No const parameters.
    if (var->IsLookupSlot()) {
      __ Push(rax);
      __ Push(rsi);
      __ Push(var->name());
      __ CallRuntime(Runtime::kInitializeLegacyConstLookupSlot, 3);
    } else {
      DCHECK(var->IsStackLocal() || var->IsContextSlot());
      Label skip;
      MemOperand location = VarOperand(var, rcx);
      __ movp(rdx, location);
      __ CompareRoot(rdx, Heap::kTheHoleValueRootIndex);
      __ j(not_equal, &skip);
      EmitStoreToStackLocalOrContextSlot(var, location);
      __ bind(&skip);
    }

  } else if (var->mode() == LET && op != Token::INIT_LET) {
    // Non-initializing assignment to let variable needs a write barrier.
    DCHECK(!var->IsLookupSlot());
    DCHECK(var->IsStackAllocated() || var->IsContextSlot());
    Label assign;
    MemOperand location = VarOperand(var, rcx);
    __ movp(rdx, location);
    __ CompareRoot(rdx, Heap::kTheHoleValueRootIndex);
    __ j(not_equal, &assign, Label::kNear);
    __ Push(var->name());
    __ CallRuntime(Runtime::kThrowReferenceError, 1);
    __ bind(&assign);
    EmitStoreToStackLocalOrContextSlot(var, location);

  } else if (!var->is_const_mode() || op == Token::INIT_CONST) {
    if (var->IsLookupSlot()) {
      // Assignment to var.
      __ Push(rax);  // Value.
      __ Push(rsi);  // Context.
      __ Push(var->name());
      __ Push(Smi::FromInt(strict_mode()));
      __ CallRuntime(Runtime::kStoreLookupSlot, 4);
    } else {
      // Assignment to var or initializing assignment to let/const in harmony
      // mode.
      DCHECK(var->IsStackAllocated() || var->IsContextSlot());
      MemOperand location = VarOperand(var, rcx);
      if (generate_debug_code_ && op == Token::INIT_LET) {
        // Check for an uninitialized let binding.
        __ movp(rdx, location);
        __ CompareRoot(rdx, Heap::kTheHoleValueRootIndex);
        __ Check(equal, kLetBindingReInitialization);
      }
      EmitStoreToStackLocalOrContextSlot(var, location);
    }
  }
  // Non-initializing assignments to consts are ignored.
}


void FullCodeGenerator::EmitNamedPropertyAssignment(Assignment* expr) {
  // Assignment to a property, using a named store IC.
  Property* prop = expr->target()->AsProperty();
  DCHECK(prop != NULL);
  DCHECK(prop->key()->IsLiteral());

  // Record source code position before IC call.
  SetSourcePosition(expr->position());
  __ Move(StoreIC::NameRegister(), prop->key()->AsLiteral()->value());
  __ Pop(StoreIC::ReceiverRegister());
  CallStoreIC(expr->AssignmentFeedbackId());

  PrepareForBailoutForId(expr->AssignmentId(), TOS_REG);
  context()->Plug(rax);
}


void FullCodeGenerator::EmitKeyedPropertyAssignment(Assignment* expr) {
  // Assignment to a property, using a keyed store IC.

  __ Pop(KeyedStoreIC::NameRegister());  // Key.
  __ Pop(KeyedStoreIC::ReceiverRegister());
  DCHECK(KeyedStoreIC::ValueRegister().is(rax));
  // Record source code position before IC call.
  SetSourcePosition(expr->position());
  Handle<Code> ic = strict_mode() == SLOPPY
      ? isolate()->builtins()->KeyedStoreIC_Initialize()
      : isolate()->builtins()->KeyedStoreIC_Initialize_Strict();
  CallIC(ic, expr->AssignmentFeedbackId());

  PrepareForBailoutForId(expr->AssignmentId(), TOS_REG);
  context()->Plug(rax);
}


void FullCodeGenerator::VisitProperty(Property* expr) {
  Comment cmnt(masm_, "[ Property");
  Expression* key = expr->key();

  if (key->IsPropertyName()) {
    VisitForAccumulatorValue(expr->obj());
    DCHECK(!rax.is(LoadIC::ReceiverRegister()));
    __ movp(LoadIC::ReceiverRegister(), rax);
    EmitNamedPropertyLoad(expr);
    PrepareForBailoutForId(expr->LoadId(), TOS_REG);
    context()->Plug(rax);
  } else {
    VisitForStackValue(expr->obj());
    VisitForAccumulatorValue(expr->key());
    __ Move(LoadIC::NameRegister(), rax);
    __ Pop(LoadIC::ReceiverRegister());
    EmitKeyedPropertyLoad(expr);
    context()->Plug(rax);
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

  CallIC::CallType call_type = callee->IsVariableProxy()
      ? CallIC::FUNCTION
      : CallIC::METHOD;
  // Get the target function.
  if (call_type == CallIC::FUNCTION) {
    { StackValueContext context(this);
      EmitVariableLoad(callee->AsVariableProxy());
      PrepareForBailout(callee, NO_REGISTERS);
    }
    // Push undefined as receiver. This is patched in the method prologue if it
    // is a sloppy mode method.
    __ Push(isolate()->factory()->undefined_value());
  } else {
    // Load the function from the receiver.
    DCHECK(callee->IsProperty());
    __ movp(LoadIC::ReceiverRegister(), Operand(rsp, 0));
    EmitNamedPropertyLoad(callee->AsProperty());
    PrepareForBailoutForId(callee->AsProperty()->LoadId(), TOS_REG);
    // Push the target function under the receiver.
    __ Push(Operand(rsp, 0));
    __ movp(Operand(rsp, kPointerSize), rax);
  }

  EmitCall(expr, call_type);
}


// Common code for calls using the IC.
void FullCodeGenerator::EmitKeyedCallWithLoadIC(Call* expr,
                                                Expression* key) {
  // Load the key.
  VisitForAccumulatorValue(key);

  Expression* callee = expr->expression();

  // Load the function from the receiver.
  DCHECK(callee->IsProperty());
  __ movp(LoadIC::ReceiverRegister(), Operand(rsp, 0));
  __ Move(LoadIC::NameRegister(), rax);
  EmitKeyedPropertyLoad(callee->AsProperty());
  PrepareForBailoutForId(callee->AsProperty()->LoadId(), TOS_REG);

  // Push the target function under the receiver.
  __ Push(Operand(rsp, 0));
  __ movp(Operand(rsp, kPointerSize), rax);

  EmitCall(expr, CallIC::METHOD);
}


void FullCodeGenerator::EmitCall(Call* expr, CallIC::CallType call_type) {
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
  __ Move(rdx, Smi::FromInt(expr->CallFeedbackSlot()));
  __ movp(rdi, Operand(rsp, (arg_count + 1) * kPointerSize));
  // Don't assign a type feedback id to the IC, since type feedback is provided
  // by the vector above.
  CallIC(ic);

  RecordJSReturnSite(expr);

  // Restore context register.
  __ movp(rsi, Operand(rbp, StandardFrameConstants::kContextOffset));
  // Discard the function left on TOS.
  context()->DropAndPlug(1, rax);
}


void FullCodeGenerator::EmitResolvePossiblyDirectEval(int arg_count) {
  // Push copy of the first argument or undefined if it doesn't exist.
  if (arg_count > 0) {
    __ Push(Operand(rsp, arg_count * kPointerSize));
  } else {
    __ PushRoot(Heap::kUndefinedValueRootIndex);
  }

  // Push the receiver of the enclosing function and do runtime call.
  StackArgumentsAccessor args(rbp, info_->scope()->num_parameters());
  __ Push(args.GetReceiverOperand());

  // Push the language mode.
  __ Push(Smi::FromInt(strict_mode()));

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
  Call::CallType call_type = expr->GetCallType(isolate());

  if (call_type == Call::POSSIBLY_EVAL_CALL) {
    // In a call to eval, we first call RuntimeHidden_ResolvePossiblyDirectEval
    // to resolve the function we need to call and the receiver of the call.
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
      __ Push(Operand(rsp, (arg_count + 1) * kPointerSize));
      EmitResolvePossiblyDirectEval(arg_count);

      // The runtime call returns a pair of values in rax (function) and
      // rdx (receiver). Touch up the stack with the right values.
      __ movp(Operand(rsp, (arg_count + 0) * kPointerSize), rdx);
      __ movp(Operand(rsp, (arg_count + 1) * kPointerSize), rax);
    }
    // Record source position for debugger.
    SetSourcePosition(expr->position());
    CallFunctionStub stub(isolate(), arg_count, NO_CALL_FUNCTION_FLAGS);
    __ movp(rdi, Operand(rsp, (arg_count + 1) * kPointerSize));
    __ CallStub(&stub);
    RecordJSReturnSite(expr);
    // Restore context register.
    __ movp(rsi, Operand(rbp, StandardFrameConstants::kContextOffset));
    context()->DropAndPlug(1, rax);
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
    // Call the runtime to find the function to call (returned in rax) and
    // the object holding it (returned in rdx).
    __ Push(context_register());
    __ Push(proxy->name());
    __ CallRuntime(Runtime::kLoadLookupSlot, 2);
    __ Push(rax);  // Function.
    __ Push(rdx);  // Receiver.

    // If fast case code has been generated, emit code to push the function
    // and receiver and have the slow path jump around this code.
    if (done.is_linked()) {
      Label call;
      __ jmp(&call, Label::kNear);
      __ bind(&done);
      // Push function.
      __ Push(rax);
      // The receiver is implicitly the global receiver. Indicate this by
      // passing the hole to the call function stub.
      __ PushRoot(Heap::kUndefinedValueRootIndex);
      __ bind(&call);
    }

    // The receiver is either the global receiver or an object found by
    // LoadContextSlot.
    EmitCall(expr);
  } else if (call_type == Call::PROPERTY_CALL) {
    Property* property = callee->AsProperty();
    { PreservePositionScope scope(masm()->positions_recorder());
      VisitForStackValue(property->obj());
    }
    if (property->key()->IsPropertyName()) {
      EmitCallWithLoadIC(expr);
    } else {
      EmitKeyedCallWithLoadIC(expr, property->key());
    }
  } else {
    DCHECK(call_type == Call::OTHER_CALL);
    // Call to an arbitrary expression not handled specially above.
    { PreservePositionScope scope(masm()->positions_recorder());
      VisitForStackValue(callee);
    }
    __ PushRoot(Heap::kUndefinedValueRootIndex);
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
  __ movp(rdi, Operand(rsp, arg_count * kPointerSize));

  // Record call targets in unoptimized code, but not in the snapshot.
  if (FLAG_pretenuring_call_new) {
    EnsureSlotContainsAllocationSite(expr->AllocationSiteFeedbackSlot());
    DCHECK(expr->AllocationSiteFeedbackSlot() ==
           expr->CallNewFeedbackSlot() + 1);
  }

  __ Move(rbx, FeedbackVector());
  __ Move(rdx, Smi::FromInt(expr->CallNewFeedbackSlot()));

  CallConstructStub stub(isolate(), RECORD_CONSTRUCTOR_TARGET);
  __ Call(stub.GetCode(), RelocInfo::CONSTRUCT_CALL);
  PrepareForBailoutForId(expr->ReturnId(), TOS_REG);
  context()->Plug(rax);
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
  __ JumpIfSmi(rax, if_true);
  __ jmp(if_false);

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
  Condition non_negative_smi = masm()->CheckNonNegativeSmi(rax);
  Split(non_negative_smi, if_true, if_false, fall_through);

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

  __ JumpIfSmi(rax, if_false);
  __ CompareRoot(rax, Heap::kNullValueRootIndex);
  __ j(equal, if_true);
  __ movp(rbx, FieldOperand(rax, HeapObject::kMapOffset));
  // Undetectable objects behave like undefined when tested with typeof.
  __ testb(FieldOperand(rbx, Map::kBitFieldOffset),
           Immediate(1 << Map::kIsUndetectable));
  __ j(not_zero, if_false);
  __ movzxbp(rbx, FieldOperand(rbx, Map::kInstanceTypeOffset));
  __ cmpp(rbx, Immediate(FIRST_NONCALLABLE_SPEC_OBJECT_TYPE));
  __ j(below, if_false);
  __ cmpp(rbx, Immediate(LAST_NONCALLABLE_SPEC_OBJECT_TYPE));
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

  __ JumpIfSmi(rax, if_false);
  __ CmpObjectType(rax, FIRST_SPEC_OBJECT_TYPE, rbx);
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

  __ JumpIfSmi(rax, if_false);
  __ movp(rbx, FieldOperand(rax, HeapObject::kMapOffset));
  __ testb(FieldOperand(rbx, Map::kBitFieldOffset),
           Immediate(1 << Map::kIsUndetectable));
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

  __ AssertNotSmi(rax);

  // Check whether this map has already been checked to be safe for default
  // valueOf.
  __ movp(rbx, FieldOperand(rax, HeapObject::kMapOffset));
  __ testb(FieldOperand(rbx, Map::kBitField2Offset),
           Immediate(1 << Map::kStringWrapperSafeForDefaultValueOf));
  __ j(not_zero, &skip_lookup);

  // Check for fast case object. Generate false result for slow case object.
  __ movp(rcx, FieldOperand(rax, JSObject::kPropertiesOffset));
  __ movp(rcx, FieldOperand(rcx, HeapObject::kMapOffset));
  __ CompareRoot(rcx, Heap::kHashTableMapRootIndex);
  __ j(equal, if_false);

  // Look for valueOf string in the descriptor array, and indicate false if
  // found. Since we omit an enumeration index check, if it is added via a
  // transition that shares its descriptor array, this is a false positive.
  Label entry, loop, done;

  // Skip loop if no descriptors are valid.
  __ NumberOfOwnDescriptors(rcx, rbx);
  __ cmpp(rcx, Immediate(0));
  __ j(equal, &done);

  __ LoadInstanceDescriptors(rbx, r8);
  // rbx: descriptor array.
  // rcx: valid entries in the descriptor array.
  // Calculate the end of the descriptor array.
  __ imulp(rcx, rcx, Immediate(DescriptorArray::kDescriptorSize));
  __ leap(rcx,
          Operand(r8, rcx, times_pointer_size, DescriptorArray::kFirstOffset));
  // Calculate location of the first key name.
  __ addp(r8, Immediate(DescriptorArray::kFirstOffset));
  // Loop through all the keys in the descriptor array. If one of these is the
  // internalized string "valueOf" the result is false.
  __ jmp(&entry);
  __ bind(&loop);
  __ movp(rdx, FieldOperand(r8, 0));
  __ Cmp(rdx, isolate()->factory()->value_of_string());
  __ j(equal, if_false);
  __ addp(r8, Immediate(DescriptorArray::kDescriptorSize * kPointerSize));
  __ bind(&entry);
  __ cmpp(r8, rcx);
  __ j(not_equal, &loop);

  __ bind(&done);

  // Set the bit in the map to indicate that there is no local valueOf field.
  __ orp(FieldOperand(rbx, Map::kBitField2Offset),
         Immediate(1 << Map::kStringWrapperSafeForDefaultValueOf));

  __ bind(&skip_lookup);

  // If a valueOf property is not found on the object check that its
  // prototype is the un-modified String prototype. If not result is false.
  __ movp(rcx, FieldOperand(rbx, Map::kPrototypeOffset));
  __ testp(rcx, Immediate(kSmiTagMask));
  __ j(zero, if_false);
  __ movp(rcx, FieldOperand(rcx, HeapObject::kMapOffset));
  __ movp(rdx, Operand(rsi, Context::SlotOffset(Context::GLOBAL_OBJECT_INDEX)));
  __ movp(rdx, FieldOperand(rdx, GlobalObject::kNativeContextOffset));
  __ cmpp(rcx,
          ContextOperand(rdx, Context::STRING_FUNCTION_PROTOTYPE_MAP_INDEX));
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

  __ JumpIfSmi(rax, if_false);
  __ CmpObjectType(rax, JS_FUNCTION_TYPE, rbx);
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
  __ CheckMap(rax, map, if_false, DO_SMI_CHECK);
  __ cmpl(FieldOperand(rax, HeapNumber::kExponentOffset),
          Immediate(0x1));
  __ j(no_overflow, if_false);
  __ cmpl(FieldOperand(rax, HeapNumber::kMantissaOffset),
          Immediate(0x00000000));
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

  __ JumpIfSmi(rax, if_false);
  __ CmpObjectType(rax, JS_ARRAY_TYPE, rbx);
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

  __ JumpIfSmi(rax, if_false);
  __ CmpObjectType(rax, JS_REGEXP_TYPE, rbx);
  PrepareForBailoutBeforeSplit(expr, true, if_true, if_false);
  Split(equal, if_true, if_false, fall_through);

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
  __ movp(rax, Operand(rbp, StandardFrameConstants::kCallerFPOffset));

  // Skip the arguments adaptor frame if it exists.
  Label check_frame_marker;
  __ Cmp(Operand(rax, StandardFrameConstants::kContextOffset),
         Smi::FromInt(StackFrame::ARGUMENTS_ADAPTOR));
  __ j(not_equal, &check_frame_marker);
  __ movp(rax, Operand(rax, StandardFrameConstants::kCallerFPOffset));

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

  __ Pop(rbx);
  __ cmpp(rax, rbx);
  PrepareForBailoutBeforeSplit(expr, true, if_true, if_false);
  Split(equal, if_true, if_false, fall_through);

  context()->Plug(if_true, if_false);
}


void FullCodeGenerator::EmitArguments(CallRuntime* expr) {
  ZoneList<Expression*>* args = expr->arguments();
  DCHECK(args->length() == 1);

  // ArgumentsAccessStub expects the key in rdx and the formal
  // parameter count in rax.
  VisitForAccumulatorValue(args->at(0));
  __ movp(rdx, rax);
  __ Move(rax, Smi::FromInt(info_->scope()->num_parameters()));
  ArgumentsAccessStub stub(isolate(), ArgumentsAccessStub::READ_ELEMENT);
  __ CallStub(&stub);
  context()->Plug(rax);
}


void FullCodeGenerator::EmitArgumentsLength(CallRuntime* expr) {
  DCHECK(expr->arguments()->length() == 0);

  Label exit;
  // Get the number of formal parameters.
  __ Move(rax, Smi::FromInt(info_->scope()->num_parameters()));

  // Check if the calling frame is an arguments adaptor frame.
  __ movp(rbx, Operand(rbp, StandardFrameConstants::kCallerFPOffset));
  __ Cmp(Operand(rbx, StandardFrameConstants::kContextOffset),
         Smi::FromInt(StackFrame::ARGUMENTS_ADAPTOR));
  __ j(not_equal, &exit, Label::kNear);

  // Arguments adaptor case: Read the arguments length from the
  // adaptor frame.
  __ movp(rax, Operand(rbx, ArgumentsAdaptorFrameConstants::kLengthOffset));

  __ bind(&exit);
  __ AssertSmi(rax);
  context()->Plug(rax);
}


void FullCodeGenerator::EmitClassOf(CallRuntime* expr) {
  ZoneList<Expression*>* args = expr->arguments();
  DCHECK(args->length() == 1);
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
  __ movp(rax, FieldOperand(rax, Map::kConstructorOffset));
  __ CmpObjectType(rax, JS_FUNCTION_TYPE, rbx);
  __ j(not_equal, &non_function_constructor);

  // rax now contains the constructor function. Grab the
  // instance class name from there.
  __ movp(rax, FieldOperand(rax, JSFunction::kSharedFunctionInfoOffset));
  __ movp(rax, FieldOperand(rax, SharedFunctionInfo::kInstanceClassNameOffset));
  __ jmp(&done);

  // Functions have class 'Function'.
  __ bind(&function);
  __ Move(rax, isolate()->factory()->function_class_string());
  __ jmp(&done);

  // Objects with a non-function constructor have class 'Object'.
  __ bind(&non_function_constructor);
  __ Move(rax, isolate()->factory()->Object_string());
  __ jmp(&done);

  // Non-JS objects have class null.
  __ bind(&null);
  __ LoadRoot(rax, Heap::kNullValueRootIndex);

  // All done.
  __ bind(&done);

  context()->Plug(rax);
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
  context()->Plug(rax);
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
  context()->Plug(rax);
}


void FullCodeGenerator::EmitValueOf(CallRuntime* expr) {
  ZoneList<Expression*>* args = expr->arguments();
  DCHECK(args->length() == 1);

  VisitForAccumulatorValue(args->at(0));  // Load the object.

  Label done;
  // If the object is a smi return the object.
  __ JumpIfSmi(rax, &done);
  // If the object is not a value type, return the object.
  __ CmpObjectType(rax, JS_VALUE_TYPE, rbx);
  __ j(not_equal, &done);
  __ movp(rax, FieldOperand(rax, JSValue::kValueOffset));

  __ bind(&done);
  context()->Plug(rax);
}


void FullCodeGenerator::EmitDateField(CallRuntime* expr) {
  ZoneList<Expression*>* args = expr->arguments();
  DCHECK(args->length() == 2);
  DCHECK_NE(NULL, args->at(1)->AsLiteral());
  Smi* index = Smi::cast(*(args->at(1)->AsLiteral()->value()));

  VisitForAccumulatorValue(args->at(0));  // Load the object.

  Label runtime, done, not_date_object;
  Register object = rax;
  Register result = rax;
  Register scratch = rcx;

  __ JumpIfSmi(object, &not_date_object);
  __ CmpObjectType(object, JS_DATE_TYPE, scratch);
  __ j(not_equal, &not_date_object);

  if (index->value() == 0) {
    __ movp(result, FieldOperand(object, JSDate::kValueOffset));
    __ jmp(&done);
  } else {
    if (index->value() < JSDate::kFirstUncachedField) {
      ExternalReference stamp = ExternalReference::date_cache_stamp(isolate());
      Operand stamp_operand = __ ExternalOperand(stamp);
      __ movp(scratch, stamp_operand);
      __ cmpp(scratch, FieldOperand(object, JSDate::kCacheStampOffset));
      __ j(not_equal, &runtime, Label::kNear);
      __ movp(result, FieldOperand(object, JSDate::kValueOffset +
                                           kPointerSize * index->value()));
      __ jmp(&done);
    }
    __ bind(&runtime);
    __ PrepareCallCFunction(2);
    __ movp(arg_reg_1, object);
    __ Move(arg_reg_2, index, Assembler::RelocInfoNone());
    __ CallCFunction(ExternalReference::get_date_field_function(isolate()), 2);
    __ movp(rsi, Operand(rbp, StandardFrameConstants::kContextOffset));
    __ jmp(&done);
  }

  __ bind(&not_date_object);
  __ CallRuntime(Runtime::kThrowNotDateError, 0);
  __ bind(&done);
  context()->Plug(rax);
}


void FullCodeGenerator::EmitOneByteSeqStringSetChar(CallRuntime* expr) {
  ZoneList<Expression*>* args = expr->arguments();
  DCHECK_EQ(3, args->length());

  Register string = rax;
  Register index = rbx;
  Register value = rcx;

  VisitForStackValue(args->at(1));  // index
  VisitForStackValue(args->at(2));  // value
  VisitForAccumulatorValue(args->at(0));  // string
  __ Pop(value);
  __ Pop(index);

  if (FLAG_debug_code) {
    __ Check(__ CheckSmi(value), kNonSmiValue);
    __ Check(__ CheckSmi(index), kNonSmiValue);
  }

  __ SmiToInteger32(value, value);
  __ SmiToInteger32(index, index);

  if (FLAG_debug_code) {
    static const uint32_t one_byte_seq_type = kSeqStringTag | kOneByteStringTag;
    __ EmitSeqStringSetCharCheck(string, index, value, one_byte_seq_type);
  }

  __ movb(FieldOperand(string, index, times_1, SeqOneByteString::kHeaderSize),
          value);
  context()->Plug(string);
}


void FullCodeGenerator::EmitTwoByteSeqStringSetChar(CallRuntime* expr) {
  ZoneList<Expression*>* args = expr->arguments();
  DCHECK_EQ(3, args->length());

  Register string = rax;
  Register index = rbx;
  Register value = rcx;

  VisitForStackValue(args->at(1));  // index
  VisitForStackValue(args->at(2));  // value
  VisitForAccumulatorValue(args->at(0));  // string
  __ Pop(value);
  __ Pop(index);

  if (FLAG_debug_code) {
    __ Check(__ CheckSmi(value), kNonSmiValue);
    __ Check(__ CheckSmi(index), kNonSmiValue);
  }

  __ SmiToInteger32(value, value);
  __ SmiToInteger32(index, index);

  if (FLAG_debug_code) {
    static const uint32_t two_byte_seq_type = kSeqStringTag | kTwoByteStringTag;
    __ EmitSeqStringSetCharCheck(string, index, value, two_byte_seq_type);
  }

  __ movw(FieldOperand(string, index, times_2, SeqTwoByteString::kHeaderSize),
          value);
  context()->Plug(rax);
}


void FullCodeGenerator::EmitMathPow(CallRuntime* expr) {
  // Load the arguments on the stack and call the runtime function.
  ZoneList<Expression*>* args = expr->arguments();
  DCHECK(args->length() == 2);
  VisitForStackValue(args->at(0));
  VisitForStackValue(args->at(1));
  MathPowStub stub(isolate(), MathPowStub::ON_STACK);
  __ CallStub(&stub);
  context()->Plug(rax);
}


void FullCodeGenerator::EmitSetValueOf(CallRuntime* expr) {
  ZoneList<Expression*>* args = expr->arguments();
  DCHECK(args->length() == 2);

  VisitForStackValue(args->at(0));  // Load the object.
  VisitForAccumulatorValue(args->at(1));  // Load the value.
  __ Pop(rbx);  // rax = value. rbx = object.

  Label done;
  // If the object is a smi, return the value.
  __ JumpIfSmi(rbx, &done);

  // If the object is not a value type, return the value.
  __ CmpObjectType(rbx, JS_VALUE_TYPE, rcx);
  __ j(not_equal, &done);

  // Store the value.
  __ movp(FieldOperand(rbx, JSValue::kValueOffset), rax);
  // Update the write barrier.  Save the value as it will be
  // overwritten by the write barrier code and is needed afterward.
  __ movp(rdx, rax);
  __ RecordWriteField(rbx, JSValue::kValueOffset, rdx, rcx, kDontSaveFPRegs);

  __ bind(&done);
  context()->Plug(rax);
}


void FullCodeGenerator::EmitNumberToString(CallRuntime* expr) {
  ZoneList<Expression*>* args = expr->arguments();
  DCHECK_EQ(args->length(), 1);

  // Load the argument into rax and call the stub.
  VisitForAccumulatorValue(args->at(0));

  NumberToStringStub stub(isolate());
  __ CallStub(&stub);
  context()->Plug(rax);
}


void FullCodeGenerator::EmitStringCharFromCode(CallRuntime* expr) {
  ZoneList<Expression*>* args = expr->arguments();
  DCHECK(args->length() == 1);

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
  DCHECK(args->length() == 2);

  VisitForStackValue(args->at(0));
  VisitForAccumulatorValue(args->at(1));

  Register object = rbx;
  Register index = rax;
  Register result = rdx;

  __ Pop(object);

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
  DCHECK(args->length() == 2);

  VisitForStackValue(args->at(0));
  VisitForAccumulatorValue(args->at(1));

  Register object = rbx;
  Register index = rax;
  Register scratch = rdx;
  Register result = rax;

  __ Pop(object);

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
  __ LoadRoot(result, Heap::kempty_stringRootIndex);
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
  DCHECK_EQ(2, args->length());
  VisitForStackValue(args->at(0));
  VisitForAccumulatorValue(args->at(1));

  __ Pop(rdx);
  StringAddStub stub(isolate(), STRING_ADD_CHECK_BOTH, NOT_TENURED);
  __ CallStub(&stub);
  context()->Plug(rax);
}


void FullCodeGenerator::EmitStringCompare(CallRuntime* expr) {
  ZoneList<Expression*>* args = expr->arguments();
  DCHECK_EQ(2, args->length());

  VisitForStackValue(args->at(0));
  VisitForStackValue(args->at(1));

  StringCompareStub stub(isolate());
  __ CallStub(&stub);
  context()->Plug(rax);
}


void FullCodeGenerator::EmitCallFunction(CallRuntime* expr) {
  ZoneList<Expression*>* args = expr->arguments();
  DCHECK(args->length() >= 2);

  int arg_count = args->length() - 2;  // 2 ~ receiver and function.
  for (int i = 0; i < arg_count + 1; i++) {
    VisitForStackValue(args->at(i));
  }
  VisitForAccumulatorValue(args->last());  // Function.

  Label runtime, done;
  // Check for non-function argument (including proxy).
  __ JumpIfSmi(rax, &runtime);
  __ CmpObjectType(rax, JS_FUNCTION_TYPE, rbx);
  __ j(not_equal, &runtime);

  // InvokeFunction requires the function in rdi. Move it in there.
  __ movp(rdi, result_register());
  ParameterCount count(arg_count);
  __ InvokeFunction(rdi, count, CALL_FUNCTION, NullCallWrapper());
  __ movp(rsi, Operand(rbp, StandardFrameConstants::kContextOffset));
  __ jmp(&done);

  __ bind(&runtime);
  __ Push(rax);
  __ CallRuntime(Runtime::kCall, args->length());
  __ bind(&done);

  context()->Plug(rax);
}


void FullCodeGenerator::EmitRegExpConstructResult(CallRuntime* expr) {
  RegExpConstructResultStub stub(isolate());
  ZoneList<Expression*>* args = expr->arguments();
  DCHECK(args->length() == 3);
  VisitForStackValue(args->at(0));
  VisitForStackValue(args->at(1));
  VisitForAccumulatorValue(args->at(2));
  __ Pop(rbx);
  __ Pop(rcx);
  __ CallStub(&stub);
  context()->Plug(rax);
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
    __ LoadRoot(rax, Heap::kUndefinedValueRootIndex);
    context()->Plug(rax);
    return;
  }

  VisitForAccumulatorValue(args->at(1));

  Register key = rax;
  Register cache = rbx;
  Register tmp = rcx;
  __ movp(cache, ContextOperand(rsi, Context::GLOBAL_OBJECT_INDEX));
  __ movp(cache,
          FieldOperand(cache, GlobalObject::kNativeContextOffset));
  __ movp(cache,
          ContextOperand(cache, Context::JSFUNCTION_RESULT_CACHES_INDEX));
  __ movp(cache,
          FieldOperand(cache, FixedArray::OffsetOfElementAt(cache_id)));

  Label done, not_found;
  STATIC_ASSERT(kSmiTag == 0 && kSmiTagSize == 1);
  __ movp(tmp, FieldOperand(cache, JSFunctionResultCache::kFingerOffset));
  // tmp now holds finger offset as a smi.
  SmiIndex index =
      __ SmiToIndex(kScratchRegister, tmp, kPointerSizeLog2);
  __ cmpp(key, FieldOperand(cache,
                            index.reg,
                            index.scale,
                            FixedArray::kHeaderSize));
  __ j(not_equal, &not_found, Label::kNear);
  __ movp(rax, FieldOperand(cache,
                            index.reg,
                            index.scale,
                            FixedArray::kHeaderSize + kPointerSize));
  __ jmp(&done, Label::kNear);

  __ bind(&not_found);
  // Call runtime to perform the lookup.
  __ Push(cache);
  __ Push(key);
  __ CallRuntime(Runtime::kGetFromCache, 2);

  __ bind(&done);
  context()->Plug(rax);
}


void FullCodeGenerator::EmitHasCachedArrayIndex(CallRuntime* expr) {
  ZoneList<Expression*>* args = expr->arguments();
  DCHECK(args->length() == 1);

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
  DCHECK(args->length() == 1);
  VisitForAccumulatorValue(args->at(0));

  __ AssertString(rax);

  __ movl(rax, FieldOperand(rax, String::kHashFieldOffset));
  DCHECK(String::kHashShift >= kSmiTagSize);
  __ IndexFromHash(rax, rax);

  context()->Plug(rax);
}


void FullCodeGenerator::EmitFastAsciiArrayJoin(CallRuntime* expr) {
  Label bailout, return_result, done, one_char_separator, long_separator,
      non_trivial_array, not_size_one_array, loop,
      loop_1, loop_1_condition, loop_2, loop_2_entry, loop_3, loop_3_entry;
  ZoneList<Expression*>* args = expr->arguments();
  DCHECK(args->length() == 2);
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
  __ subp(rsp, Immediate(2 * kPointerSize));
  __ cld();
  // Check that the array is a JSArray
  __ JumpIfSmi(array, &bailout);
  __ CmpObjectType(array, JS_ARRAY_TYPE, scratch);
  __ j(not_equal, &bailout);

  // Check that the array has fast elements.
  __ CheckFastElements(scratch, &bailout);

  // Array has fast elements, so its length must be a smi.
  // If the array has length zero, return the empty string.
  __ movp(array_length, FieldOperand(array, JSArray::kLengthOffset));
  __ SmiCompare(array_length, Smi::FromInt(0));
  __ j(not_zero, &non_trivial_array);
  __ LoadRoot(rax, Heap::kempty_stringRootIndex);
  __ jmp(&return_result);

  // Save the array length on the stack.
  __ bind(&non_trivial_array);
  __ SmiToInteger32(array_length, array_length);
  __ movl(array_length_operand, array_length);

  // Save the FixedArray containing array's elements.
  // End of array's live range.
  elements = array;
  __ movp(elements, FieldOperand(array, JSArray::kElementsOffset));
  array = no_reg;


  // Check that all array elements are sequential ASCII strings, and
  // accumulate the sum of their lengths, as a smi-encoded value.
  __ Set(index, 0);
  __ Set(string_length, 0);
  // Loop condition: while (index < array_length).
  // Live loop registers: index(int32), array_length(int32), string(String*),
  //                      scratch, string_length(int32), elements(FixedArray*).
  if (generate_debug_code_) {
    __ cmpp(index, array_length);
    __ Assert(below, kNoEmptyArraysHereInEmitFastAsciiArrayJoin);
  }
  __ bind(&loop);
  __ movp(string, FieldOperand(elements,
                               index,
                               times_pointer_size,
                               FixedArray::kHeaderSize));
  __ JumpIfSmi(string, &bailout);
  __ movp(scratch, FieldOperand(string, HeapObject::kMapOffset));
  __ movzxbl(scratch, FieldOperand(scratch, Map::kInstanceTypeOffset));
  __ andb(scratch, Immediate(
      kIsNotStringMask | kStringEncodingMask | kStringRepresentationMask));
  __ cmpb(scratch, Immediate(kStringTag | kOneByteStringTag | kSeqStringTag));
  __ j(not_equal, &bailout);
  __ AddSmiField(string_length,
                 FieldOperand(string, SeqOneByteString::kLengthOffset));
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
  __ movp(rax, FieldOperand(elements, FixedArray::kHeaderSize));
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
  __ movp(string, separator_operand);
  __ JumpIfSmi(string, &bailout);
  __ movp(scratch, FieldOperand(string, HeapObject::kMapOffset));
  __ movzxbl(scratch, FieldOperand(scratch, Map::kInstanceTypeOffset));
  __ andb(scratch, Immediate(
      kIsNotStringMask | kStringEncodingMask | kStringRepresentationMask));
  __ cmpb(scratch, Immediate(kStringTag | kOneByteStringTag | kSeqStringTag));
  __ j(not_equal, &bailout);

  // Live registers:
  // string_length: Sum of string lengths.
  // elements: FixedArray of strings.
  // index: Array length.
  // string: Separator string.

  // Add (separator length times (array_length - 1)) to string_length.
  __ SmiToInteger32(scratch,
                    FieldOperand(string, SeqOneByteString::kLengthOffset));
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
  __ movp(result_operand, result_pos);
  __ leap(result_pos, FieldOperand(result_pos, SeqOneByteString::kHeaderSize));

  __ movp(string, separator_operand);
  __ SmiCompare(FieldOperand(string, SeqOneByteString::kLengthOffset),
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
  __ movp(string, FieldOperand(elements, index,
                               times_pointer_size,
                               FixedArray::kHeaderSize));
  __ SmiToInteger32(string_length,
                    FieldOperand(string, String::kLengthOffset));
  __ leap(string,
         FieldOperand(string, SeqOneByteString::kHeaderSize));
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
  __ movzxbl(scratch, FieldOperand(string, SeqOneByteString::kHeaderSize));
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
  __ incp(result_pos);

  __ bind(&loop_2_entry);
  // Get string = array[index].
  __ movp(string, FieldOperand(elements, index,
                               times_pointer_size,
                               FixedArray::kHeaderSize));
  __ SmiToInteger32(string_length,
                    FieldOperand(string, String::kLengthOffset));
  __ leap(string,
         FieldOperand(string, SeqOneByteString::kHeaderSize));
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
  __ leap(elements, FieldOperand(elements, index, times_pointer_size,
                                FixedArray::kHeaderSize));
  __ negq(index);

  // Replace separator string with pointer to its first character, and
  // make scratch be its length.
  __ movp(string, separator_operand);
  __ SmiToInteger32(scratch,
                    FieldOperand(string, String::kLengthOffset));
  __ leap(string,
         FieldOperand(string, SeqOneByteString::kHeaderSize));
  __ movp(separator_operand, string);

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
  __ movp(string, separator_operand);
  __ movl(string_length, scratch);
  __ CopyBytes(result_pos, string, string_length, 2);

  __ bind(&loop_3_entry);
  // Get string = array[index].
  __ movp(string, Operand(elements, index, times_pointer_size, 0));
  __ SmiToInteger32(string_length,
                    FieldOperand(string, String::kLengthOffset));
  __ leap(string,
         FieldOperand(string, SeqOneByteString::kHeaderSize));
  __ CopyBytes(result_pos, string, string_length);
  __ incq(index);
  __ j(not_equal, &loop_3);  // Loop while (index < 0).

  __ bind(&done);
  __ movp(rax, result_operand);

  __ bind(&return_result);
  // Drop temp values from the stack, and restore context register.
  __ addp(rsp, Immediate(3 * kPointerSize));
  __ movp(rsi, Operand(rbp, StandardFrameConstants::kContextOffset));
  context()->Plug(rax);
}


void FullCodeGenerator::EmitDebugIsActive(CallRuntime* expr) {
  DCHECK(expr->arguments()->length() == 0);
  ExternalReference debug_is_active =
      ExternalReference::debug_is_active_address(isolate());
  __ Move(kScratchRegister, debug_is_active);
  __ movzxbp(rax, Operand(kScratchRegister, 0));
  __ Integer32ToSmi(rax, rax);
  context()->Plug(rax);
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
  int arg_count = args->length();

  if (expr->is_jsruntime()) {
    // Push the builtins object as receiver.
    __ movp(rax, GlobalObjectOperand());
    __ Push(FieldOperand(rax, GlobalObject::kBuiltinsOffset));

    // Load the function from the receiver.
    __ movp(LoadIC::ReceiverRegister(), Operand(rsp, 0));
    __ Move(LoadIC::NameRegister(), expr->name());
    if (FLAG_vector_ics) {
      __ Move(LoadIC::SlotRegister(),
              Smi::FromInt(expr->CallRuntimeFeedbackSlot()));
      CallLoadIC(NOT_CONTEXTUAL);
    } else {
      CallLoadIC(NOT_CONTEXTUAL, expr->CallRuntimeFeedbackId());
    }

    // Push the target function under the receiver.
    __ Push(Operand(rsp, 0));
    __ movp(Operand(rsp, kPointerSize), rax);

    // Push the arguments ("left-to-right").
    for (int i = 0; i < arg_count; i++) {
      VisitForStackValue(args->at(i));
    }

    // Record source position of the IC call.
    SetSourcePosition(expr->position());
    CallFunctionStub stub(isolate(), arg_count, NO_CALL_FUNCTION_FLAGS);
    __ movp(rdi, Operand(rsp, (arg_count + 1) * kPointerSize));
    __ CallStub(&stub);

    // Restore context register.
    __ movp(rsi, Operand(rbp, StandardFrameConstants::kContextOffset));
    context()->DropAndPlug(1, rax);

  } else {
    // Push the arguments ("left-to-right").
    for (int i = 0; i < arg_count; i++) {
      VisitForStackValue(args->at(i));
    }

    // Call the C runtime.
    __ CallRuntime(expr->function(), arg_count);
    context()->Plug(rax);
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
        __ Push(Smi::FromInt(strict_mode()));
        __ InvokeBuiltin(Builtins::DELETE, CALL_FUNCTION);
        context()->Plug(rax);
      } else if (proxy != NULL) {
        Variable* var = proxy->var();
        // Delete of an unqualified identifier is disallowed in strict mode
        // but "delete this" is allowed.
        DCHECK(strict_mode() == SLOPPY || var->is_this());
        if (var->IsUnallocated()) {
          __ Push(GlobalObjectOperand());
          __ Push(var->name());
          __ Push(Smi::FromInt(SLOPPY));
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
          __ Push(context_register());
          __ Push(var->name());
          __ CallRuntime(Runtime::kDeleteLookupSlot, 2);
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
        DCHECK(context()->IsAccumulatorValue() || context()->IsStackValue());
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

    default:
      UNREACHABLE();
  }
}


void FullCodeGenerator::VisitCountOperation(CountOperation* expr) {
  DCHECK(expr->expression()->IsValidReferenceExpression());

  Comment cmnt(masm_, "[ CountOperation");
  SetSourcePosition(expr->position());

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
    DCHECK(expr->expression()->AsVariableProxy()->var() != NULL);
    AccumulatorValueContext context(this);
    EmitVariableLoad(expr->expression()->AsVariableProxy());
  } else {
    // Reserve space for result of postfix operation.
    if (expr->is_postfix() && !context()->IsEffect()) {
      __ Push(Smi::FromInt(0));
    }
    if (assign_type == NAMED_PROPERTY) {
      VisitForStackValue(prop->obj());
      __ movp(LoadIC::ReceiverRegister(), Operand(rsp, 0));
      EmitNamedPropertyLoad(prop);
    } else {
      VisitForStackValue(prop->obj());
      VisitForStackValue(prop->key());
      // Leave receiver on stack
      __ movp(LoadIC::ReceiverRegister(), Operand(rsp, kPointerSize));
      // Copy of key, needed for later store.
      __ movp(LoadIC::NameRegister(), Operand(rsp, 0));
      EmitKeyedPropertyLoad(prop);
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
    patch_site.EmitJumpIfNotSmi(rax, &slow, Label::kNear);

    // Save result for postfix expressions.
    if (expr->is_postfix()) {
      if (!context()->IsEffect()) {
        // Save the result on the stack. If we have a named or keyed property
        // we store the result under the receiver that is currently on top
        // of the stack.
        switch (assign_type) {
          case VARIABLE:
            __ Push(rax);
            break;
          case NAMED_PROPERTY:
            __ movp(Operand(rsp, kPointerSize), rax);
            break;
          case KEYED_PROPERTY:
            __ movp(Operand(rsp, 2 * kPointerSize), rax);
            break;
        }
      }
    }

    SmiOperationExecutionMode mode;
    mode.Add(PRESERVE_SOURCE_REGISTER);
    mode.Add(BAILOUT_ON_NO_OVERFLOW);
    if (expr->op() == Token::INC) {
      __ SmiAddConstant(rax, rax, Smi::FromInt(1), mode, &done, Label::kNear);
    } else {
      __ SmiSubConstant(rax, rax, Smi::FromInt(1), mode, &done, Label::kNear);
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
          __ Push(rax);
          break;
        case NAMED_PROPERTY:
          __ movp(Operand(rsp, kPointerSize), rax);
          break;
        case KEYED_PROPERTY:
          __ movp(Operand(rsp, 2 * kPointerSize), rax);
          break;
      }
    }
  }

  // Record position before stub call.
  SetSourcePosition(expr->position());

  // Call stub for +1/-1.
  __ bind(&stub_call);
  __ movp(rdx, rax);
  __ Move(rax, Smi::FromInt(1));
  BinaryOpICStub stub(isolate(), expr->binary_op(), NO_OVERWRITE);
  CallIC(stub.GetCode(), expr->CountBinOpFeedbackId());
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
      __ Move(StoreIC::NameRegister(), prop->key()->AsLiteral()->value());
      __ Pop(StoreIC::ReceiverRegister());
      CallStoreIC(expr->CountStoreFeedbackId());
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
      __ Pop(KeyedStoreIC::NameRegister());
      __ Pop(KeyedStoreIC::ReceiverRegister());
      Handle<Code> ic = strict_mode() == SLOPPY
          ? isolate()->builtins()->KeyedStoreIC_Initialize()
          : isolate()->builtins()->KeyedStoreIC_Initialize_Strict();
      CallIC(ic, expr->CountStoreFeedbackId());
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
  DCHECK(!context()->IsEffect());
  DCHECK(!context()->IsTest());

  if (proxy != NULL && proxy->var()->IsUnallocated()) {
    Comment cmnt(masm_, "[ Global variable");
    __ Move(LoadIC::NameRegister(), proxy->name());
    __ movp(LoadIC::ReceiverRegister(), GlobalObjectOperand());
    if (FLAG_vector_ics) {
      __ Move(LoadIC::SlotRegister(),
              Smi::FromInt(proxy->VariableFeedbackSlot()));
    }
    // Use a regular load, not a contextual load, to avoid a reference
    // error.
    CallLoadIC(NOT_CONTEXTUAL);
    PrepareForBailout(expr, TOS_REG);
    context()->Plug(rax);
  } else if (proxy != NULL && proxy->var()->IsLookupSlot()) {
    Comment cmnt(masm_, "[ Lookup slot");
    Label done, slow;

    // Generate code for loading from variables potentially shadowed
    // by eval-introduced variables.
    EmitDynamicLookupFastCase(proxy, INSIDE_TYPEOF, &slow, &done);

    __ bind(&slow);
    __ Push(rsi);
    __ Push(proxy->name());
    __ CallRuntime(Runtime::kLoadLookupSlotNoReferenceError, 2);
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

  Factory* factory = isolate()->factory();
  if (String::Equals(check, factory->number_string())) {
    __ JumpIfSmi(rax, if_true);
    __ movp(rax, FieldOperand(rax, HeapObject::kMapOffset));
    __ CompareRoot(rax, Heap::kHeapNumberMapRootIndex);
    Split(equal, if_true, if_false, fall_through);
  } else if (String::Equals(check, factory->string_string())) {
    __ JumpIfSmi(rax, if_false);
    // Check for undetectable objects => false.
    __ CmpObjectType(rax, FIRST_NONSTRING_TYPE, rdx);
    __ j(above_equal, if_false);
    __ testb(FieldOperand(rdx, Map::kBitFieldOffset),
             Immediate(1 << Map::kIsUndetectable));
    Split(zero, if_true, if_false, fall_through);
  } else if (String::Equals(check, factory->symbol_string())) {
    __ JumpIfSmi(rax, if_false);
    __ CmpObjectType(rax, SYMBOL_TYPE, rdx);
    Split(equal, if_true, if_false, fall_through);
  } else if (String::Equals(check, factory->boolean_string())) {
    __ CompareRoot(rax, Heap::kTrueValueRootIndex);
    __ j(equal, if_true);
    __ CompareRoot(rax, Heap::kFalseValueRootIndex);
    Split(equal, if_true, if_false, fall_through);
  } else if (String::Equals(check, factory->undefined_string())) {
    __ CompareRoot(rax, Heap::kUndefinedValueRootIndex);
    __ j(equal, if_true);
    __ JumpIfSmi(rax, if_false);
    // Check for undetectable objects => true.
    __ movp(rdx, FieldOperand(rax, HeapObject::kMapOffset));
    __ testb(FieldOperand(rdx, Map::kBitFieldOffset),
             Immediate(1 << Map::kIsUndetectable));
    Split(not_zero, if_true, if_false, fall_through);
  } else if (String::Equals(check, factory->function_string())) {
    __ JumpIfSmi(rax, if_false);
    STATIC_ASSERT(NUM_OF_CALLABLE_SPEC_OBJECT_TYPES == 2);
    __ CmpObjectType(rax, JS_FUNCTION_TYPE, rdx);
    __ j(equal, if_true);
    __ CmpInstanceType(rdx, JS_FUNCTION_PROXY_TYPE);
    Split(equal, if_true, if_false, fall_through);
  } else if (String::Equals(check, factory->object_string())) {
    __ JumpIfSmi(rax, if_false);
    __ CompareRoot(rax, Heap::kNullValueRootIndex);
    __ j(equal, if_true);
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
      InstanceofStub stub(isolate(), InstanceofStub::kNoFlags);
      __ CallStub(&stub);
      PrepareForBailoutBeforeSplit(expr, true, if_true, if_false);
      __ testp(rax, rax);
       // The stub returns 0 for true.
      Split(zero, if_true, if_false, fall_through);
      break;
    }

    default: {
      VisitForAccumulatorValue(expr->right());
      Condition cc = CompareIC::ComputeCondition(op);
      __ Pop(rdx);

      bool inline_smi_code = ShouldInlineSmiCase(op);
      JumpPatchSite patch_site(masm_);
      if (inline_smi_code) {
        Label slow_case;
        __ movp(rcx, rdx);
        __ orp(rcx, rax);
        patch_site.EmitJumpIfNotSmi(rcx, &slow_case, Label::kNear);
        __ cmpp(rdx, rax);
        Split(cc, if_true, if_false, NULL);
        __ bind(&slow_case);
      }

      // Record position and call the compare IC.
      SetSourcePosition(expr->position());
      Handle<Code> ic = CompareIC::GetUninitialized(isolate(), op);
      CallIC(ic, expr->CompareOperationFeedbackId());
      patch_site.EmitPatchInfo();

      PrepareForBailoutBeforeSplit(expr, true, if_true, if_false);
      __ testp(rax, rax);
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
  if (expr->op() == Token::EQ_STRICT) {
    Heap::RootListIndex nil_value = nil == kNullValue ?
        Heap::kNullValueRootIndex :
        Heap::kUndefinedValueRootIndex;
    __ CompareRoot(rax, nil_value);
    Split(equal, if_true, if_false, fall_through);
  } else {
    Handle<Code> ic = CompareNilICStub::GetUninitialized(isolate(), nil);
    CallIC(ic, expr->CompareOperationFeedbackId());
    __ testp(rax, rax);
    Split(not_zero, if_true, if_false, fall_through);
  }
  context()->Plug(if_true, if_false);
}


void FullCodeGenerator::VisitThisFunction(ThisFunction* expr) {
  __ movp(rax, Operand(rbp, JavaScriptFrameConstants::kFunctionOffset));
  context()->Plug(rax);
}


Register FullCodeGenerator::result_register() {
  return rax;
}


Register FullCodeGenerator::context_register() {
  return rsi;
}


void FullCodeGenerator::StoreToFrameField(int frame_offset, Register value) {
  DCHECK(IsAligned(frame_offset, kPointerSize));
  __ movp(Operand(rbp, frame_offset), value);
}


void FullCodeGenerator::LoadContextField(Register dst, int context_index) {
  __ movp(dst, ContextOperand(rsi, context_index));
}


void FullCodeGenerator::PushFunctionArgumentForContextAllocation() {
  Scope* declaration_scope = scope()->DeclarationScope();
  if (declaration_scope->is_global_scope() ||
      declaration_scope->is_module_scope()) {
    // Contexts nested in the native context have a canonical empty function
    // as their closure, not the anonymous closure containing the global
    // code.  Pass a smi sentinel and let the runtime look up the empty
    // function.
    __ Push(Smi::FromInt(0));
  } else if (declaration_scope->is_eval_scope()) {
    // Contexts created by a call to eval have the same closure as the
    // context calling eval, not the anonymous closure containing the eval
    // code.  Fetch it from the context.
    __ Push(ContextOperand(rsi, Context::CLOSURE_INDEX));
  } else {
    DCHECK(declaration_scope->is_function_scope());
    __ Push(Operand(rbp, JavaScriptFrameConstants::kFunctionOffset));
  }
}


// ----------------------------------------------------------------------------
// Non-local control flow support.


void FullCodeGenerator::EnterFinallyBlock() {
  DCHECK(!result_register().is(rdx));
  DCHECK(!result_register().is(rcx));
  // Cook return address on top of stack (smi encoded Code* delta)
  __ PopReturnAddressTo(rdx);
  __ Move(rcx, masm_->CodeObject());
  __ subp(rdx, rcx);
  __ Integer32ToSmi(rdx, rdx);
  __ Push(rdx);

  // Store result register while executing finally block.
  __ Push(result_register());

  // Store pending message while executing finally block.
  ExternalReference pending_message_obj =
      ExternalReference::address_of_pending_message_obj(isolate());
  __ Load(rdx, pending_message_obj);
  __ Push(rdx);

  ExternalReference has_pending_message =
      ExternalReference::address_of_has_pending_message(isolate());
  __ Load(rdx, has_pending_message);
  __ Integer32ToSmi(rdx, rdx);
  __ Push(rdx);

  ExternalReference pending_message_script =
      ExternalReference::address_of_pending_message_script(isolate());
  __ Load(rdx, pending_message_script);
  __ Push(rdx);
}


void FullCodeGenerator::ExitFinallyBlock() {
  DCHECK(!result_register().is(rdx));
  DCHECK(!result_register().is(rcx));
  // Restore pending message from stack.
  __ Pop(rdx);
  ExternalReference pending_message_script =
      ExternalReference::address_of_pending_message_script(isolate());
  __ Store(pending_message_script, rdx);

  __ Pop(rdx);
  __ SmiToInteger32(rdx, rdx);
  ExternalReference has_pending_message =
      ExternalReference::address_of_has_pending_message(isolate());
  __ Store(has_pending_message, rdx);

  __ Pop(rdx);
  ExternalReference pending_message_obj =
      ExternalReference::address_of_pending_message_obj(isolate());
  __ Store(pending_message_obj, rdx);

  // Restore result register from stack.
  __ Pop(result_register());

  // Uncook return address.
  __ Pop(rdx);
  __ SmiToInteger32(rdx, rdx);
  __ Move(rcx, masm_->CodeObject());
  __ addp(rdx, rcx);
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
    __ movp(rsi, Operand(rsp, StackHandlerConstants::kContextOffset));
    __ movp(Operand(rbp, StandardFrameConstants::kContextOffset), rsi);
  }
  __ PopTryHandler();
  __ call(finally_entry_);

  *stack_depth = 0;
  *context_length = 0;
  return previous_;
}


#undef __


static const byte kJnsInstruction = 0x79;
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

  if (Assembler::target_address_at(call_target_address,
                                   unoptimized_code) ==
      isolate->builtins()->OnStackReplacement()->entry()) {
    return ON_STACK_REPLACEMENT;
  }

  DCHECK_EQ(isolate->builtins()->OsrAfterStackCheck()->entry(),
            Assembler::target_address_at(call_target_address,
                                         unoptimized_code));
  return OSR_AFTER_STACK_CHECK;
}


} }  // namespace v8::internal

#endif  // V8_TARGET_ARCH_X64
