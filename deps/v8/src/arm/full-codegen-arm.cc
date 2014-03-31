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

#if V8_TARGET_ARCH_ARM

#include "code-stubs.h"
#include "codegen.h"
#include "compiler.h"
#include "debug.h"
#include "full-codegen.h"
#include "isolate-inl.h"
#include "parser.h"
#include "scopes.h"
#include "stub-cache.h"

#include "arm/code-stubs-arm.h"
#include "arm/macro-assembler-arm.h"

namespace v8 {
namespace internal {

#define __ ACCESS_MASM(masm_)


// A patch site is a location in the code which it is possible to patch. This
// class has a number of methods to emit the code which is patchable and the
// method EmitPatchInfo to record a marker back to the patchable code. This
// marker is a cmp rx, #yyy instruction, and x * 0x00000fff + yyy (raw 12 bit
// immediate value is used) is the delta from the pc to the first instruction of
// the patchable code.
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

  // When initially emitting this ensure that a jump is always generated to skip
  // the inlined smi code.
  void EmitJumpIfNotSmi(Register reg, Label* target) {
    ASSERT(!patch_site_.is_bound() && !info_emitted_);
    Assembler::BlockConstPoolScope block_const_pool(masm_);
    __ bind(&patch_site_);
    __ cmp(reg, Operand(reg));
    __ b(eq, target);  // Always taken before patched.
  }

  // When initially emitting this ensure that a jump is never generated to skip
  // the inlined smi code.
  void EmitJumpIfSmi(Register reg, Label* target) {
    ASSERT(!patch_site_.is_bound() && !info_emitted_);
    Assembler::BlockConstPoolScope block_const_pool(masm_);
    __ bind(&patch_site_);
    __ cmp(reg, Operand(reg));
    __ b(ne, target);  // Never taken before patched.
  }

  void EmitPatchInfo() {
    // Block literal pool emission whilst recording patch site information.
    Assembler::BlockConstPoolScope block_const_pool(masm_);
    if (patch_site_.is_bound()) {
      int delta_to_patch_site = masm_->InstructionsGeneratedSince(&patch_site_);
      Register reg;
      reg.set_code(delta_to_patch_site / kOff12Mask);
      __ cmp_raw_immediate(reg, delta_to_patch_site % kOff12Mask);
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


static void EmitStackCheck(MacroAssembler* masm_,
                           Register stack_limit_scratch,
                           int pointers = 0,
                           Register scratch = sp) {
    Isolate* isolate = masm_->isolate();
  Label ok;
  ASSERT(scratch.is(sp) == (pointers == 0));
  if (pointers != 0) {
    __ sub(scratch, sp, Operand(pointers * kPointerSize));
  }
  __ LoadRoot(stack_limit_scratch, Heap::kStackLimitRootIndex);
  __ cmp(scratch, Operand(stack_limit_scratch));
  __ b(hs, &ok);
  PredictableCodeSizeScope predictable(masm_, 2 * Assembler::kInstrSize);
  __ Call(isolate->builtins()->StackCheck(), RelocInfo::CODE_TARGET);
  __ bind(&ok);
}


// Generate code for a JS function.  On entry to the function the receiver
// and arguments have been pushed on the stack left to right.  The actual
// argument count matches the formal parameter count expected by the
// function.
//
// The live registers are:
//   o r1: the JS function object being called (i.e., ourselves)
//   o cp: our context
//   o pp: our caller's constant pool pointer (if FLAG_enable_ool_constant_pool)
//   o fp: our caller's frame pointer
//   o sp: stack pointer
//   o lr: return address
//
// The function builds a JS frame.  Please see JavaScriptFrameConstants in
// frames-arm.h for its layout.
void FullCodeGenerator::Generate() {
  CompilationInfo* info = info_;
  handler_table_ =
      isolate()->factory()->NewFixedArray(function()->handler_count(), TENURED);

  InitializeFeedbackVector();

  profiling_counter_ = isolate()->factory()->NewCell(
      Handle<Smi>(Smi::FromInt(FLAG_interrupt_budget), isolate()));
  SetFunctionPosition(function());
  Comment cmnt(masm_, "[ function compiled by full code generator");

  ProfileEntryHookStub::MaybeCallEntryHook(masm_);

#ifdef DEBUG
  if (strlen(FLAG_stop_at) > 0 &&
      info->function()->name()->IsUtf8EqualTo(CStrVector(FLAG_stop_at))) {
    __ stop("stop-at");
  }
#endif

  // Sloppy mode functions and builtins need to replace the receiver with the
  // global proxy when called as functions (without an explicit receiver
  // object).
  if (info->strict_mode() == SLOPPY && !info->is_native()) {
    Label ok;
    int receiver_offset = info->scope()->num_parameters() * kPointerSize;
    __ ldr(r2, MemOperand(sp, receiver_offset));
    __ CompareRoot(r2, Heap::kUndefinedValueRootIndex);
    __ b(ne, &ok);

    __ ldr(r2, GlobalObjectOperand());
    __ ldr(r2, FieldMemOperand(r2, GlobalObject::kGlobalReceiverOffset));

    __ str(r2, MemOperand(sp, receiver_offset));

    __ bind(&ok);
  }

  // Open a frame scope to indicate that there is a frame on the stack.  The
  // MANUAL indicates that the scope shouldn't actually generate code to set up
  // the frame (that is done below).
  FrameScope frame_scope(masm_, StackFrame::MANUAL);

  info->set_prologue_offset(masm_->pc_offset());
  __ Prologue(BUILD_FUNCTION_FRAME);
  info->AddNoFrameRange(0, masm_->pc_offset());

  { Comment cmnt(masm_, "[ Allocate locals");
    int locals_count = info->scope()->num_stack_slots();
    // Generators allocate locals, if any, in context slots.
    ASSERT(!info->function()->is_generator() || locals_count == 0);
    if (locals_count > 0) {
      if (locals_count >= 128) {
        EmitStackCheck(masm_, r2, locals_count, r9);
      }
      __ LoadRoot(r9, Heap::kUndefinedValueRootIndex);
      int kMaxPushes = FLAG_optimize_for_size ? 4 : 32;
      if (locals_count >= kMaxPushes) {
        int loop_iterations = locals_count / kMaxPushes;
        __ mov(r2, Operand(loop_iterations));
        Label loop_header;
        __ bind(&loop_header);
        // Do pushes.
        for (int i = 0; i < kMaxPushes; i++) {
          __ push(r9);
        }
        // Continue loop if not done.
        __ sub(r2, r2, Operand(1), SetCC);
        __ b(&loop_header, ne);
      }
      int remaining = locals_count % kMaxPushes;
      // Emit the remaining pushes.
      for (int i  = 0; i < remaining; i++) {
        __ push(r9);
      }
    }
  }

  bool function_in_register = true;

  // Possibly allocate a local context.
  int heap_slots = info->scope()->num_heap_slots() - Context::MIN_CONTEXT_SLOTS;
  if (heap_slots > 0) {
    // Argument to NewContext is the function, which is still in r1.
    Comment cmnt(masm_, "[ Allocate context");
    if (FLAG_harmony_scoping && info->scope()->is_global_scope()) {
      __ push(r1);
      __ Push(info->scope()->GetScopeInfo());
      __ CallRuntime(Runtime::kHiddenNewGlobalContext, 2);
    } else if (heap_slots <= FastNewContextStub::kMaximumSlots) {
      FastNewContextStub stub(heap_slots);
      __ CallStub(&stub);
    } else {
      __ push(r1);
      __ CallRuntime(Runtime::kHiddenNewFunctionContext, 1);
    }
    function_in_register = false;
    // Context is returned in r0.  It replaces the context passed to us.
    // It's saved in the stack and kept live in cp.
    __ mov(cp, r0);
    __ str(r0, MemOperand(fp, StandardFrameConstants::kContextOffset));
    // Copy any necessary parameters into the context.
    int num_parameters = info->scope()->num_parameters();
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

        // Update the write barrier.
        __ RecordWriteContextSlot(
            cp, target.offset(), r0, r3, kLRHasBeenSaved, kDontSaveFPRegs);
      }
    }
  }

  Variable* arguments = scope()->arguments();
  if (arguments != NULL) {
    // Function uses arguments object.
    Comment cmnt(masm_, "[ Allocate arguments object");
    if (!function_in_register) {
      // Load this again, if it's used by the local context below.
      __ ldr(r3, MemOperand(fp, JavaScriptFrameConstants::kFunctionOffset));
    } else {
      __ mov(r3, r1);
    }
    // Receiver is just before the parameters on the caller's stack.
    int num_parameters = info->scope()->num_parameters();
    int offset = num_parameters * kPointerSize;
    __ add(r2, fp,
           Operand(StandardFrameConstants::kCallerSPOffset + offset));
    __ mov(r1, Operand(Smi::FromInt(num_parameters)));
    __ Push(r3, r2, r1);

    // Arguments to ArgumentsAccessStub:
    //   function, receiver address, parameter count.
    // The stub will rewrite receiever and parameter count if the previous
    // stack frame was an arguments adapter frame.
    ArgumentsAccessStub::Type type;
    if (strict_mode() == STRICT) {
      type = ArgumentsAccessStub::NEW_STRICT;
    } else if (function()->has_duplicate_parameters()) {
      type = ArgumentsAccessStub::NEW_SLOPPY_SLOW;
    } else {
      type = ArgumentsAccessStub::NEW_SLOPPY_FAST;
    }
    ArgumentsAccessStub stub(type);
    __ CallStub(&stub);

    SetVar(arguments, r0, r1, r2);
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
        ASSERT(function->proxy()->var()->mode() == CONST ||
               function->proxy()->var()->mode() == CONST_LEGACY);
        ASSERT(function->proxy()->var()->location() != Variable::UNALLOCATED);
        VisitVariableDeclaration(function);
      }
      VisitDeclarations(scope()->declarations());
    }

    { Comment cmnt(masm_, "[ Stack check");
      PrepareForBailoutForId(BailoutId::Declarations(), NO_REGISTERS);
      EmitStackCheck(masm_, ip);
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
    __ LoadRoot(r0, Heap::kUndefinedValueRootIndex);
  }
  EmitReturnSequence();

  // Force emit the constant pool, so it doesn't get emitted in the middle
  // of the back edge table.
  masm()->CheckConstPool(true, false);
}


void FullCodeGenerator::ClearAccumulator() {
  __ mov(r0, Operand(Smi::FromInt(0)));
}


void FullCodeGenerator::EmitProfilingCounterDecrement(int delta) {
  __ mov(r2, Operand(profiling_counter_));
  __ ldr(r3, FieldMemOperand(r2, Cell::kValueOffset));
  __ sub(r3, r3, Operand(Smi::FromInt(delta)), SetCC);
  __ str(r3, FieldMemOperand(r2, Cell::kValueOffset));
}


void FullCodeGenerator::EmitProfilingCounterReset() {
  int reset_value = FLAG_interrupt_budget;
  if (isolate()->IsDebuggerActive()) {
    // Detect debug break requests as soon as possible.
    reset_value = FLAG_interrupt_budget >> 4;
  }
  __ mov(r2, Operand(profiling_counter_));
  __ mov(r3, Operand(Smi::FromInt(reset_value)));
  __ str(r3, FieldMemOperand(r2, Cell::kValueOffset));
}


void FullCodeGenerator::EmitBackEdgeBookkeeping(IterationStatement* stmt,
                                                Label* back_edge_target) {
  Comment cmnt(masm_, "[ Back edge bookkeeping");
  // Block literal pools whilst emitting back edge code.
  Assembler::BlockConstPoolScope block_const_pool(masm_);
  Label ok;

  ASSERT(back_edge_target->is_bound());
  int distance = masm_->SizeOfCodeGeneratedSince(back_edge_target);
  int weight = Min(kMaxBackEdgeWeight,
                   Max(1, distance / kCodeSizeMultiplier));
  EmitProfilingCounterDecrement(weight);
  __ b(pl, &ok);
  __ Call(isolate()->builtins()->InterruptCheck(), RelocInfo::CODE_TARGET);

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
    __ b(&return_label_);
  } else {
    __ bind(&return_label_);
    if (FLAG_trace) {
      // Push the return value on the stack as the parameter.
      // Runtime::TraceExit returns its parameter in r0.
      __ push(r0);
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
    __ b(pl, &ok);
    __ push(r0);
    __ Call(isolate()->builtins()->InterruptCheck(),
            RelocInfo::CODE_TARGET);
    __ pop(r0);
    EmitProfilingCounterReset();
    __ bind(&ok);

#ifdef DEBUG
    // Add a label for checking the size of the code used for returning.
    Label check_exit_codesize;
    __ bind(&check_exit_codesize);
#endif
    // Make sure that the constant pool is not emitted inside of the return
    // sequence.
    { Assembler::BlockConstPoolScope block_const_pool(masm_);
      int32_t sp_delta = (info_->scope()->num_parameters() + 1) * kPointerSize;
      CodeGenerator::RecordPositions(masm_, function()->end_position() - 1);
      // TODO(svenpanne) The code below is sometimes 4 words, sometimes 5!
      PredictableCodeSizeScope predictable(masm_, -1);
      __ RecordJSReturn();
      int no_frame_start = __ LeaveFrame(StackFrame::JAVA_SCRIPT);
      __ add(sp, sp, Operand(sp_delta));
      __ Jump(lr);
      info_->AddNoFrameRange(no_frame_start, masm_->pc_offset());
    }

#ifdef DEBUG
    // Check that the size of the code used for returning is large enough
    // for the debugger's requirements.
    ASSERT(Assembler::kJSReturnSequenceInstructions <=
           masm_->InstructionsGeneratedSince(&check_exit_codesize));
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
  codegen()->GetVar(result_register(), var);
  __ push(result_register());
}


void FullCodeGenerator::TestContext::Plug(Variable* var) const {
  ASSERT(var->IsStackAllocated() || var->IsContextSlot());
  // For simplicity we always test the accumulator register.
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
  __ LoadRoot(result_register(), index);
  __ push(result_register());
}


void FullCodeGenerator::TestContext::Plug(Heap::RootListIndex index) const {
  codegen()->PrepareForBailoutBeforeSplit(condition(),
                                          true,
                                          true_label_,
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


void FullCodeGenerator::EffectContext::Plug(Handle<Object> lit) const {
}


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
  codegen()->PrepareForBailoutBeforeSplit(condition(),
                                          true,
                                          true_label_,
                                          false_label_);
  ASSERT(!lit->IsUndetectableObject());  // There are no undetectable literals.
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
  __ str(reg, MemOperand(sp, 0));
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
  __ LoadRoot(result_register(), Heap::kTrueValueRootIndex);
  __ jmp(&done);
  __ bind(materialize_false);
  __ LoadRoot(result_register(), Heap::kFalseValueRootIndex);
  __ bind(&done);
}


void FullCodeGenerator::StackValueContext::Plug(
    Label* materialize_true,
    Label* materialize_false) const {
  Label done;
  __ bind(materialize_true);
  __ LoadRoot(ip, Heap::kTrueValueRootIndex);
  __ jmp(&done);
  __ bind(materialize_false);
  __ LoadRoot(ip, Heap::kFalseValueRootIndex);
  __ bind(&done);
  __ push(ip);
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
  __ LoadRoot(ip, value_root_index);
  __ push(ip);
}


void FullCodeGenerator::TestContext::Plug(bool flag) const {
  codegen()->PrepareForBailoutBeforeSplit(condition(),
                                          true,
                                          true_label_,
                                          false_label_);
  if (flag) {
    if (true_label_ != fall_through_) __ b(true_label_);
  } else {
    if (false_label_ != fall_through_) __ b(false_label_);
  }
}


void FullCodeGenerator::DoTest(Expression* condition,
                               Label* if_true,
                               Label* if_false,
                               Label* fall_through) {
  Handle<Code> ic = ToBooleanStub::GetUninitialized(isolate());
  CallIC(ic, condition->test_id());
  __ tst(result_register(), result_register());
  Split(ne, if_true, if_false, fall_through);
}


void FullCodeGenerator::Split(Condition cond,
                              Label* if_true,
                              Label* if_false,
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
  ASSERT(var->IsStackAllocated());
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
  // Use destination as scratch.
  MemOperand location = VarOperand(var, dest);
  __ ldr(dest, location);
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
  __ str(src, location);

  // Emit the write barrier code if the location is in the heap.
  if (var->IsContextSlot()) {
    __ RecordWriteContextSlot(scratch0,
                              location.offset(),
                              src,
                              scratch1,
                              kLRHasBeenSaved,
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
  if (!context()->IsTest() || !info_->IsOptimizable()) return;

  Label skip;
  if (should_normalize) __ b(&skip);
  PrepareForBailout(expr, TOS_REG);
  if (should_normalize) {
    __ LoadRoot(ip, Heap::kTrueValueRootIndex);
    __ cmp(r0, ip);
    Split(eq, if_true, if_false, NULL);
    __ bind(&skip);
  }
}


void FullCodeGenerator::EmitDebugCheckDeclarationContext(Variable* variable) {
  // The variable in the declaration always resides in the current function
  // context.
  ASSERT_EQ(0, scope()->ContextChainLength(variable->scope()));
  if (generate_debug_code_) {
    // Check that we're not inside a with or catch context.
    __ ldr(r1, FieldMemOperand(cp, HeapObject::kMapOffset));
    __ CompareRoot(r1, Heap::kWithContextMapRootIndex);
    __ Check(ne, kDeclarationInWithContext);
    __ CompareRoot(r1, Heap::kCatchContextMapRootIndex);
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
        __ LoadRoot(ip, Heap::kTheHoleValueRootIndex);
        __ str(ip, StackOperand(variable));
      }
      break;

    case Variable::CONTEXT:
      if (hole_init) {
        Comment cmnt(masm_, "[ VariableDeclaration");
        EmitDebugCheckDeclarationContext(variable);
        __ LoadRoot(ip, Heap::kTheHoleValueRootIndex);
        __ str(ip, ContextOperand(cp, variable->index()));
        // No write barrier since the_hole_value is in old space.
        PrepareForBailoutForId(proxy->id(), NO_REGISTERS);
      }
      break;

    case Variable::LOOKUP: {
      Comment cmnt(masm_, "[ VariableDeclaration");
      __ mov(r2, Operand(variable->name()));
      // Declaration nodes are always introduced in one of four modes.
      ASSERT(IsDeclaredVariableMode(mode));
      PropertyAttributes attr =
          IsImmutableVariableMode(mode) ? READ_ONLY : NONE;
      __ mov(r1, Operand(Smi::FromInt(attr)));
      // Push initial value, if any.
      // Note: For variables we must not push an initial value (such as
      // 'undefined') because we may have a (legal) redeclaration and we
      // must not destroy the current value.
      if (hole_init) {
        __ LoadRoot(r0, Heap::kTheHoleValueRootIndex);
        __ Push(cp, r2, r1, r0);
      } else {
        __ mov(r0, Operand(Smi::FromInt(0)));  // Indicates no initial value.
        __ Push(cp, r2, r1, r0);
      }
      __ CallRuntime(Runtime::kHiddenDeclareContextSlot, 4);
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
      __ str(result_register(), StackOperand(variable));
      break;
    }

    case Variable::CONTEXT: {
      Comment cmnt(masm_, "[ FunctionDeclaration");
      EmitDebugCheckDeclarationContext(variable);
      VisitForAccumulatorValue(declaration->fun());
      __ str(result_register(), ContextOperand(cp, variable->index()));
      int offset = Context::SlotOffset(variable->index());
      // We know that we have written a function, which is not a smi.
      __ RecordWriteContextSlot(cp,
                                offset,
                                result_register(),
                                r2,
                                kLRHasBeenSaved,
                                kDontSaveFPRegs,
                                EMIT_REMEMBERED_SET,
                                OMIT_SMI_CHECK);
      PrepareForBailoutForId(proxy->id(), NO_REGISTERS);
      break;
    }

    case Variable::LOOKUP: {
      Comment cmnt(masm_, "[ FunctionDeclaration");
      __ mov(r2, Operand(variable->name()));
      __ mov(r1, Operand(Smi::FromInt(NONE)));
      __ Push(cp, r2, r1);
      // Push initial value for function declaration.
      VisitForStackValue(declaration->fun());
      __ CallRuntime(Runtime::kHiddenDeclareContextSlot, 4);
      break;
    }
  }
}


void FullCodeGenerator::VisitModuleDeclaration(ModuleDeclaration* declaration) {
  Variable* variable = declaration->proxy()->var();
  ASSERT(variable->location() == Variable::CONTEXT);
  ASSERT(variable->interface()->IsFrozen());

  Comment cmnt(masm_, "[ ModuleDeclaration");
  EmitDebugCheckDeclarationContext(variable);

  // Load instance object.
  __ LoadContext(r1, scope_->ContextChainLength(scope_->GlobalScope()));
  __ ldr(r1, ContextOperand(r1, variable->interface()->Index()));
  __ ldr(r1, ContextOperand(r1, Context::EXTENSION_INDEX));

  // Assign it.
  __ str(r1, ContextOperand(cp, variable->index()));
  // We know that we have written a module, which is not a smi.
  __ RecordWriteContextSlot(cp,
                            Context::SlotOffset(variable->index()),
                            r1,
                            r3,
                            kLRHasBeenSaved,
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
  // The context is the first argument.
  __ mov(r1, Operand(pairs));
  __ mov(r0, Operand(Smi::FromInt(DeclareGlobalsFlags())));
  __ Push(cp, r1, r0);
  __ CallRuntime(Runtime::kHiddenDeclareGlobals, 3);
  // Return value is ignored.
}


void FullCodeGenerator::DeclareModules(Handle<FixedArray> descriptions) {
  // Call the runtime to declare the modules.
  __ Push(descriptions);
  __ CallRuntime(Runtime::kHiddenDeclareModules, 1);
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
    __ ldr(r1, MemOperand(sp, 0));  // Switch value.
    bool inline_smi_code = ShouldInlineSmiCase(Token::EQ_STRICT);
    JumpPatchSite patch_site(masm_);
    if (inline_smi_code) {
      Label slow_case;
      __ orr(r2, r1, r0);
      patch_site.EmitJumpIfNotSmi(r2, &slow_case);

      __ cmp(r1, r0);
      __ b(ne, &next_test);
      __ Drop(1);  // Switch value is no longer needed.
      __ b(clause->body_target());
      __ bind(&slow_case);
    }

    // Record position before stub call for type feedback.
    SetSourcePosition(clause->position());
    Handle<Code> ic = CompareIC::GetUninitialized(isolate(), Token::EQ_STRICT);
    CallIC(ic, clause->CompareId());
    patch_site.EmitPatchInfo();

    Label skip;
    __ b(&skip);
    PrepareForBailout(clause, TOS_REG);
    __ LoadRoot(ip, Heap::kTrueValueRootIndex);
    __ cmp(r0, ip);
    __ b(ne, &next_test);
    __ Drop(1);
    __ jmp(clause->body_target());
    __ bind(&skip);

    __ cmp(r0, Operand::Zero());
    __ b(ne, &next_test);
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
  int slot = stmt->ForInFeedbackSlot();
  SetStatementPosition(stmt);

  Label loop, exit;
  ForIn loop_statement(this, stmt);
  increment_loop_depth();

  // Get the object to enumerate over. If the object is null or undefined, skip
  // over the loop.  See ECMA-262 version 5, section 12.6.4.
  VisitForAccumulatorValue(stmt->enumerable());
  __ LoadRoot(ip, Heap::kUndefinedValueRootIndex);
  __ cmp(r0, ip);
  __ b(eq, &exit);
  Register null_value = r5;
  __ LoadRoot(null_value, Heap::kNullValueRootIndex);
  __ cmp(r0, null_value);
  __ b(eq, &exit);

  PrepareForBailoutForId(stmt->PrepareId(), TOS_REG);

  // Convert the object to a JS object.
  Label convert, done_convert;
  __ JumpIfSmi(r0, &convert);
  __ CompareObjectType(r0, r1, r1, FIRST_SPEC_OBJECT_TYPE);
  __ b(ge, &done_convert);
  __ bind(&convert);
  __ push(r0);
  __ InvokeBuiltin(Builtins::TO_OBJECT, CALL_FUNCTION);
  __ bind(&done_convert);
  __ push(r0);

  // Check for proxies.
  Label call_runtime;
  STATIC_ASSERT(FIRST_JS_PROXY_TYPE == FIRST_SPEC_OBJECT_TYPE);
  __ CompareObjectType(r0, r1, r1, LAST_JS_PROXY_TYPE);
  __ b(le, &call_runtime);

  // Check cache validity in generated code. This is a fast case for
  // the JSObject::IsSimpleEnum cache validity checks. If we cannot
  // guarantee cache validity, call the runtime system to check cache
  // validity or get the property names in a fixed array.
  __ CheckEnumCache(null_value, &call_runtime);

  // The enum cache is valid.  Load the map of the object being
  // iterated over and use the cache for the iteration.
  Label use_cache;
  __ ldr(r0, FieldMemOperand(r0, HeapObject::kMapOffset));
  __ b(&use_cache);

  // Get the set of properties to enumerate.
  __ bind(&call_runtime);
  __ push(r0);  // Duplicate the enumerable object on the stack.
  __ CallRuntime(Runtime::kGetPropertyNamesFast, 1);

  // If we got a map from the runtime call, we can do a fast
  // modification check. Otherwise, we got a fixed array, and we have
  // to do a slow check.
  Label fixed_array;
  __ ldr(r2, FieldMemOperand(r0, HeapObject::kMapOffset));
  __ LoadRoot(ip, Heap::kMetaMapRootIndex);
  __ cmp(r2, ip);
  __ b(ne, &fixed_array);

  // We got a map in register r0. Get the enumeration cache from it.
  Label no_descriptors;
  __ bind(&use_cache);

  __ EnumLength(r1, r0);
  __ cmp(r1, Operand(Smi::FromInt(0)));
  __ b(eq, &no_descriptors);

  __ LoadInstanceDescriptors(r0, r2);
  __ ldr(r2, FieldMemOperand(r2, DescriptorArray::kEnumCacheOffset));
  __ ldr(r2, FieldMemOperand(r2, DescriptorArray::kEnumCacheBridgeCacheOffset));

  // Set up the four remaining stack slots.
  __ push(r0);  // Map.
  __ mov(r0, Operand(Smi::FromInt(0)));
  // Push enumeration cache, enumeration cache length (as smi) and zero.
  __ Push(r2, r1, r0);
  __ jmp(&loop);

  __ bind(&no_descriptors);
  __ Drop(1);
  __ jmp(&exit);

  // We got a fixed array in register r0. Iterate through that.
  Label non_proxy;
  __ bind(&fixed_array);

  Handle<Object> feedback = Handle<Object>(
      Smi::FromInt(TypeFeedbackInfo::kForInFastCaseMarker),
      isolate());
  StoreFeedbackVectorSlot(slot, feedback);
  __ Move(r1, FeedbackVector());
  __ mov(r2, Operand(Smi::FromInt(TypeFeedbackInfo::kForInSlowCaseMarker)));
  __ str(r2, FieldMemOperand(r1, FixedArray::OffsetOfElementAt(slot)));

  __ mov(r1, Operand(Smi::FromInt(1)));  // Smi indicates slow check
  __ ldr(r2, MemOperand(sp, 0 * kPointerSize));  // Get enumerated object
  STATIC_ASSERT(FIRST_JS_PROXY_TYPE == FIRST_SPEC_OBJECT_TYPE);
  __ CompareObjectType(r2, r3, r3, LAST_JS_PROXY_TYPE);
  __ b(gt, &non_proxy);
  __ mov(r1, Operand(Smi::FromInt(0)));  // Zero indicates proxy
  __ bind(&non_proxy);
  __ Push(r1, r0);  // Smi and array
  __ ldr(r1, FieldMemOperand(r0, FixedArray::kLengthOffset));
  __ mov(r0, Operand(Smi::FromInt(0)));
  __ Push(r1, r0);  // Fixed array length (as smi) and initial index.

  // Generate code for doing the condition check.
  PrepareForBailoutForId(stmt->BodyId(), NO_REGISTERS);
  __ bind(&loop);
  // Load the current count to r0, load the length to r1.
  __ Ldrd(r0, r1, MemOperand(sp, 0 * kPointerSize));
  __ cmp(r0, r1);  // Compare to the array length.
  __ b(hs, loop_statement.break_label());

  // Get the current entry of the array into register r3.
  __ ldr(r2, MemOperand(sp, 2 * kPointerSize));
  __ add(r2, r2, Operand(FixedArray::kHeaderSize - kHeapObjectTag));
  __ ldr(r3, MemOperand::PointerAddressFromSmiKey(r2, r0));

  // Get the expected map from the stack or a smi in the
  // permanent slow case into register r2.
  __ ldr(r2, MemOperand(sp, 3 * kPointerSize));

  // Check if the expected map still matches that of the enumerable.
  // If not, we may have to filter the key.
  Label update_each;
  __ ldr(r1, MemOperand(sp, 4 * kPointerSize));
  __ ldr(r4, FieldMemOperand(r1, HeapObject::kMapOffset));
  __ cmp(r4, Operand(r2));
  __ b(eq, &update_each);

  // For proxies, no filtering is done.
  // TODO(rossberg): What if only a prototype is a proxy? Not specified yet.
  __ cmp(r2, Operand(Smi::FromInt(0)));
  __ b(eq, &update_each);

  // Convert the entry to a string or (smi) 0 if it isn't a property
  // any more. If the property has been removed while iterating, we
  // just skip it.
  __ push(r1);  // Enumerable.
  __ push(r3);  // Current entry.
  __ InvokeBuiltin(Builtins::FILTER_KEY, CALL_FUNCTION);
  __ mov(r3, Operand(r0), SetCC);
  __ b(eq, loop_statement.continue_label());

  // Update the 'each' property or variable from the possibly filtered
  // entry in register r3.
  __ bind(&update_each);
  __ mov(result_register(), r3);
  // Perform the assignment as if via '='.
  { EffectContext context(this);
    EmitAssignment(stmt->each());
  }

  // Generate code for the body of the loop.
  Visit(stmt->body());

  // Generate code for the going to the next element by incrementing
  // the index (smi) stored on top of the stack.
  __ bind(loop_statement.continue_label());
  __ pop(r0);
  __ add(r0, r0, Operand(Smi::FromInt(1)));
  __ push(r0);

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


void FullCodeGenerator::VisitForOfStatement(ForOfStatement* stmt) {
  Comment cmnt(masm_, "[ ForOfStatement");
  SetStatementPosition(stmt);

  Iteration loop_statement(this, stmt);
  increment_loop_depth();

  // var iterator = iterable[@@iterator]()
  VisitForAccumulatorValue(stmt->assign_iterator());

  // As with for-in, skip the loop if the iterator is null or undefined.
  __ CompareRoot(r0, Heap::kUndefinedValueRootIndex);
  __ b(eq, loop_statement.break_label());
  __ CompareRoot(r0, Heap::kNullValueRootIndex);
  __ b(eq, loop_statement.break_label());

  // Convert the iterator to a JS object.
  Label convert, done_convert;
  __ JumpIfSmi(r0, &convert);
  __ CompareObjectType(r0, r1, r1, FIRST_SPEC_OBJECT_TYPE);
  __ b(ge, &done_convert);
  __ bind(&convert);
  __ push(r0);
  __ InvokeBuiltin(Builtins::TO_OBJECT, CALL_FUNCTION);
  __ bind(&done_convert);
  __ push(r0);

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
    FastNewClosureStub stub(info->strict_mode(), info->is_generator());
    __ mov(r2, Operand(info));
    __ CallStub(&stub);
  } else {
    __ mov(r0, Operand(info));
    __ LoadRoot(r1, pretenure ? Heap::kTrueValueRootIndex
                              : Heap::kFalseValueRootIndex);
    __ Push(cp, r0, r1);
    __ CallRuntime(Runtime::kHiddenNewClosure, 3);
  }
  context()->Plug(r0);
}


void FullCodeGenerator::VisitVariableProxy(VariableProxy* expr) {
  Comment cmnt(masm_, "[ VariableProxy");
  EmitVariableLoad(expr);
}


void FullCodeGenerator::EmitLoadGlobalCheckExtensions(Variable* var,
                                                      TypeofState typeof_state,
                                                      Label* slow) {
  Register current = cp;
  Register next = r1;
  Register temp = r2;

  Scope* s = scope();
  while (s != NULL) {
    if (s->num_heap_slots() > 0) {
      if (s->calls_sloppy_eval()) {
        // Check that extension is NULL.
        __ ldr(temp, ContextOperand(current, Context::EXTENSION_INDEX));
        __ tst(temp, temp);
        __ b(ne, slow);
      }
      // Load next context in chain.
      __ ldr(next, ContextOperand(current, Context::PREVIOUS_INDEX));
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
    __ ldr(temp, FieldMemOperand(next, HeapObject::kMapOffset));
    __ LoadRoot(ip, Heap::kNativeContextMapRootIndex);
    __ cmp(temp, ip);
    __ b(eq, &fast);
    // Check that extension is NULL.
    __ ldr(temp, ContextOperand(next, Context::EXTENSION_INDEX));
    __ tst(temp, temp);
    __ b(ne, slow);
    // Load next context in chain.
    __ ldr(next, ContextOperand(next, Context::PREVIOUS_INDEX));
    __ b(&loop);
    __ bind(&fast);
  }

  __ ldr(r0, GlobalObjectOperand());
  __ mov(r2, Operand(var->name()));
  ContextualMode mode = (typeof_state == INSIDE_TYPEOF)
      ? NOT_CONTEXTUAL
      : CONTEXTUAL;
  CallLoadIC(mode);
}


MemOperand FullCodeGenerator::ContextSlotOperandCheckExtensions(Variable* var,
                                                                Label* slow) {
  ASSERT(var->IsContextSlot());
  Register context = cp;
  Register next = r3;
  Register temp = r4;

  for (Scope* s = scope(); s != var->scope(); s = s->outer_scope()) {
    if (s->num_heap_slots() > 0) {
      if (s->calls_sloppy_eval()) {
        // Check that extension is NULL.
        __ ldr(temp, ContextOperand(context, Context::EXTENSION_INDEX));
        __ tst(temp, temp);
        __ b(ne, slow);
      }
      __ ldr(next, ContextOperand(context, Context::PREVIOUS_INDEX));
      // Walk the rest of the chain without clobbering cp.
      context = next;
    }
  }
  // Check that last extension is NULL.
  __ ldr(temp, ContextOperand(context, Context::EXTENSION_INDEX));
  __ tst(temp, temp);
  __ b(ne, slow);

  // This function is used only for loads, not stores, so it's safe to
  // return an cp-based operand (the write barrier cannot be allowed to
  // destroy the cp register).
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
    __ ldr(r0, ContextSlotOperandCheckExtensions(local, slow));
    if (local->mode() == LET || local->mode() == CONST ||
        local->mode() == CONST_LEGACY) {
      __ CompareRoot(r0, Heap::kTheHoleValueRootIndex);
      if (local->mode() == CONST_LEGACY) {
        __ LoadRoot(r0, Heap::kUndefinedValueRootIndex, eq);
      } else {  // LET || CONST
        __ b(ne, done);
        __ mov(r0, Operand(var->name()));
        __ push(r0);
        __ CallRuntime(Runtime::kHiddenThrowReferenceError, 1);
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
      // Use inline caching. Variable name is passed in r2 and the global
      // object (receiver) in r0.
      __ ldr(r0, GlobalObjectOperand());
      __ mov(r2, Operand(var->name()));
      CallLoadIC(CONTEXTUAL);
      context()->Plug(r0);
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
          skip_init_check = var->mode() != CONST_LEGACY &&
              var->initializer_position() < proxy->position();
        }

        if (!skip_init_check) {
          // Let and const need a read barrier.
          GetVar(r0, var);
          __ CompareRoot(r0, Heap::kTheHoleValueRootIndex);
          if (var->mode() == LET || var->mode() == CONST) {
            // Throw a reference error when using an uninitialized let/const
            // binding in harmony mode.
            Label done;
            __ b(ne, &done);
            __ mov(r0, Operand(var->name()));
            __ push(r0);
            __ CallRuntime(Runtime::kHiddenThrowReferenceError, 1);
            __ bind(&done);
          } else {
            // Uninitalized const bindings outside of harmony mode are unholed.
            ASSERT(var->mode() == CONST_LEGACY);
            __ LoadRoot(r0, Heap::kUndefinedValueRootIndex, eq);
          }
          context()->Plug(r0);
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
      EmitDynamicLookupFastCase(var, NOT_INSIDE_TYPEOF, &slow, &done);
      __ bind(&slow);
      __ mov(r1, Operand(var->name()));
      __ Push(cp, r1);  // Context and name.
      __ CallRuntime(Runtime::kHiddenLoadContextSlot, 2);
      __ bind(&done);
      context()->Plug(r0);
    }
  }
}


void FullCodeGenerator::VisitRegExpLiteral(RegExpLiteral* expr) {
  Comment cmnt(masm_, "[ RegExpLiteral");
  Label materialized;
  // Registers will be used as follows:
  // r5 = materialized value (RegExp literal)
  // r4 = JS function, literals array
  // r3 = literal index
  // r2 = RegExp pattern
  // r1 = RegExp flags
  // r0 = RegExp literal clone
  __ ldr(r0, MemOperand(fp, JavaScriptFrameConstants::kFunctionOffset));
  __ ldr(r4, FieldMemOperand(r0, JSFunction::kLiteralsOffset));
  int literal_offset =
      FixedArray::kHeaderSize + expr->literal_index() * kPointerSize;
  __ ldr(r5, FieldMemOperand(r4, literal_offset));
  __ LoadRoot(ip, Heap::kUndefinedValueRootIndex);
  __ cmp(r5, ip);
  __ b(ne, &materialized);

  // Create regexp literal using runtime function.
  // Result will be in r0.
  __ mov(r3, Operand(Smi::FromInt(expr->literal_index())));
  __ mov(r2, Operand(expr->pattern()));
  __ mov(r1, Operand(expr->flags()));
  __ Push(r4, r3, r2, r1);
  __ CallRuntime(Runtime::kHiddenMaterializeRegExpLiteral, 4);
  __ mov(r5, r0);

  __ bind(&materialized);
  int size = JSRegExp::kSize + JSRegExp::kInObjectFieldCount * kPointerSize;
  Label allocated, runtime_allocate;
  __ Allocate(size, r0, r2, r3, &runtime_allocate, TAG_OBJECT);
  __ jmp(&allocated);

  __ bind(&runtime_allocate);
  __ mov(r0, Operand(Smi::FromInt(size)));
  __ Push(r5, r0);
  __ CallRuntime(Runtime::kHiddenAllocateInNewSpace, 1);
  __ pop(r5);

  __ bind(&allocated);
  // After this, registers are used as follows:
  // r0: Newly allocated regexp.
  // r5: Materialized regexp.
  // r2: temp.
  __ CopyFields(r0, r5, d0, size / kPointerSize);
  context()->Plug(r0);
}


void FullCodeGenerator::EmitAccessor(Expression* expression) {
  if (expression == NULL) {
    __ LoadRoot(r1, Heap::kNullValueRootIndex);
    __ push(r1);
  } else {
    VisitForStackValue(expression);
  }
}


void FullCodeGenerator::VisitObjectLiteral(ObjectLiteral* expr) {
  Comment cmnt(masm_, "[ ObjectLiteral");

  expr->BuildConstantProperties(isolate());
  Handle<FixedArray> constant_properties = expr->constant_properties();
  __ ldr(r3, MemOperand(fp, JavaScriptFrameConstants::kFunctionOffset));
  __ ldr(r3, FieldMemOperand(r3, JSFunction::kLiteralsOffset));
  __ mov(r2, Operand(Smi::FromInt(expr->literal_index())));
  __ mov(r1, Operand(constant_properties));
  int flags = expr->fast_elements()
      ? ObjectLiteral::kFastElements
      : ObjectLiteral::kNoFlags;
  flags |= expr->has_function()
      ? ObjectLiteral::kHasFunction
      : ObjectLiteral::kNoFlags;
  __ mov(r0, Operand(Smi::FromInt(flags)));
  int properties_count = constant_properties->length() / 2;
  if (expr->may_store_doubles() || expr->depth() > 1 || Serializer::enabled() ||
      flags != ObjectLiteral::kFastElements ||
      properties_count > FastCloneShallowObjectStub::kMaximumClonedProperties) {
    __ Push(r3, r2, r1, r0);
    __ CallRuntime(Runtime::kHiddenCreateObjectLiteral, 4);
  } else {
    FastCloneShallowObjectStub stub(properties_count);
    __ CallStub(&stub);
  }

  // If result_saved is true the result is on top of the stack.  If
  // result_saved is false the result is in r0.
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
      __ push(r0);  // Save result on stack
      result_saved = true;
    }
    switch (property->kind()) {
      case ObjectLiteral::Property::CONSTANT:
        UNREACHABLE();
      case ObjectLiteral::Property::MATERIALIZED_LITERAL:
        ASSERT(!CompileTimeValue::IsCompileTimeValue(property->value()));
        // Fall through.
      case ObjectLiteral::Property::COMPUTED:
        if (key->value()->IsInternalizedString()) {
          if (property->emit_store()) {
            VisitForAccumulatorValue(value);
            __ mov(r2, Operand(key->value()));
            __ ldr(r1, MemOperand(sp));
            CallStoreIC(key->LiteralFeedbackId());
            PrepareForBailoutForId(key->id(), NO_REGISTERS);
          } else {
            VisitForEffect(value);
          }
          break;
        }
        // Duplicate receiver on stack.
        __ ldr(r0, MemOperand(sp));
        __ push(r0);
        VisitForStackValue(key);
        VisitForStackValue(value);
        if (property->emit_store()) {
          __ mov(r0, Operand(Smi::FromInt(NONE)));  // PropertyAttributes
          __ push(r0);
          __ CallRuntime(Runtime::kSetProperty, 4);
        } else {
          __ Drop(3);
        }
        break;
      case ObjectLiteral::Property::PROTOTYPE:
        // Duplicate receiver on stack.
        __ ldr(r0, MemOperand(sp));
        __ push(r0);
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
    __ ldr(r0, MemOperand(sp));  // Duplicate receiver.
    __ push(r0);
    VisitForStackValue(it->first);
    EmitAccessor(it->second->getter);
    EmitAccessor(it->second->setter);
    __ mov(r0, Operand(Smi::FromInt(NONE)));
    __ push(r0);
    __ CallRuntime(Runtime::kDefineOrRedefineAccessorProperty, 5);
  }

  if (expr->has_function()) {
    ASSERT(result_saved);
    __ ldr(r0, MemOperand(sp));
    __ push(r0);
    __ CallRuntime(Runtime::kToFastProperties, 1);
  }

  if (result_saved) {
    context()->PlugTOS();
  } else {
    context()->Plug(r0);
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
  ASSERT_EQ(2, constant_elements->length());
  ElementsKind constant_elements_kind =
      static_cast<ElementsKind>(Smi::cast(constant_elements->get(0))->value());
  bool has_fast_elements = IsFastObjectElementsKind(constant_elements_kind);
  Handle<FixedArrayBase> constant_elements_values(
      FixedArrayBase::cast(constant_elements->get(1)));

  AllocationSiteMode allocation_site_mode = TRACK_ALLOCATION_SITE;
  if (has_fast_elements && !FLAG_allocation_site_pretenuring) {
    // If the only customer of allocation sites is transitioning, then
    // we can turn it off if we don't have anywhere else to transition to.
    allocation_site_mode = DONT_TRACK_ALLOCATION_SITE;
  }

  __ ldr(r3, MemOperand(fp, JavaScriptFrameConstants::kFunctionOffset));
  __ ldr(r3, FieldMemOperand(r3, JSFunction::kLiteralsOffset));
  __ mov(r2, Operand(Smi::FromInt(expr->literal_index())));
  __ mov(r1, Operand(constant_elements));
  if (has_fast_elements && constant_elements_values->map() ==
      isolate()->heap()->fixed_cow_array_map()) {
    FastCloneShallowArrayStub stub(
        FastCloneShallowArrayStub::COPY_ON_WRITE_ELEMENTS,
        allocation_site_mode,
        length);
    __ CallStub(&stub);
    __ IncrementCounter(
        isolate()->counters()->cow_arrays_created_stub(), 1, r1, r2);
  } else if (expr->depth() > 1 || Serializer::enabled() ||
             length > FastCloneShallowArrayStub::kMaximumClonedLength) {
    __ mov(r0, Operand(Smi::FromInt(flags)));
    __ Push(r3, r2, r1, r0);
    __ CallRuntime(Runtime::kHiddenCreateArrayLiteral, 4);
  } else {
    ASSERT(IsFastSmiOrObjectElementsKind(constant_elements_kind) ||
           FLAG_smi_only_arrays);
    FastCloneShallowArrayStub::Mode mode =
        FastCloneShallowArrayStub::CLONE_ANY_ELEMENTS;

    if (has_fast_elements) {
      mode = FastCloneShallowArrayStub::CLONE_ELEMENTS;
    }

    FastCloneShallowArrayStub stub(mode, allocation_site_mode, length);
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
      __ push(r0);
      __ Push(Smi::FromInt(expr->literal_index()));
      result_saved = true;
    }
    VisitForAccumulatorValue(subexpr);

    if (IsFastObjectElementsKind(constant_elements_kind)) {
      int offset = FixedArray::kHeaderSize + (i * kPointerSize);
      __ ldr(r6, MemOperand(sp, kPointerSize));  // Copy of array literal.
      __ ldr(r1, FieldMemOperand(r6, JSObject::kElementsOffset));
      __ str(result_register(), FieldMemOperand(r1, offset));
      // Update the write barrier for the array store.
      __ RecordWriteField(r1, offset, result_register(), r2,
                          kLRHasBeenSaved, kDontSaveFPRegs,
                          EMIT_REMEMBERED_SET, INLINE_SMI_CHECK);
    } else {
      __ mov(r3, Operand(Smi::FromInt(i)));
      StoreArrayLiteralElementStub stub;
      __ CallStub(&stub);
    }

    PrepareForBailoutForId(expr->GetIdForElement(i), NO_REGISTERS);
  }

  if (result_saved) {
    __ pop();  // literal index
    context()->PlugTOS();
  } else {
    context()->Plug(r0);
  }
}


void FullCodeGenerator::VisitAssignment(Assignment* expr) {
  ASSERT(expr->target()->IsValidLeftHandSide());

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
        // We need the receiver both on the stack and in the accumulator.
        VisitForAccumulatorValue(property->obj());
        __ push(result_register());
      } else {
        VisitForStackValue(property->obj());
      }
      break;
    case KEYED_PROPERTY:
      if (expr->is_compound()) {
        VisitForStackValue(property->obj());
        VisitForAccumulatorValue(property->key());
        __ ldr(r1, MemOperand(sp, 0));
        __ push(r0);
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
    __ push(r0);  // Left operand goes on the stack.
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
      context()->Plug(r0);
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
      __ push(result_register());
      // Fall through.
    case Yield::INITIAL: {
      Label suspend, continuation, post_runtime, resume;

      __ jmp(&suspend);

      __ bind(&continuation);
      __ jmp(&resume);

      __ bind(&suspend);
      VisitForAccumulatorValue(expr->generator_object());
      ASSERT(continuation.pos() > 0 && Smi::IsValid(continuation.pos()));
      __ mov(r1, Operand(Smi::FromInt(continuation.pos())));
      __ str(r1, FieldMemOperand(r0, JSGeneratorObject::kContinuationOffset));
      __ str(cp, FieldMemOperand(r0, JSGeneratorObject::kContextOffset));
      __ mov(r1, cp);
      __ RecordWriteField(r0, JSGeneratorObject::kContextOffset, r1, r2,
                          kLRHasBeenSaved, kDontSaveFPRegs);
      __ add(r1, fp, Operand(StandardFrameConstants::kExpressionsOffset));
      __ cmp(sp, r1);
      __ b(eq, &post_runtime);
      __ push(r0);  // generator object
      __ CallRuntime(Runtime::kHiddenSuspendJSGeneratorObject, 1);
      __ ldr(cp, MemOperand(fp, StandardFrameConstants::kContextOffset));
      __ bind(&post_runtime);
      __ pop(result_register());
      EmitReturnSequence();

      __ bind(&resume);
      context()->Plug(result_register());
      break;
    }

    case Yield::FINAL: {
      VisitForAccumulatorValue(expr->generator_object());
      __ mov(r1, Operand(Smi::FromInt(JSGeneratorObject::kGeneratorClosed)));
      __ str(r1, FieldMemOperand(result_register(),
                                 JSGeneratorObject::kContinuationOffset));
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
      // Initial send value is undefined.
      __ LoadRoot(r0, Heap::kUndefinedValueRootIndex);
      __ b(&l_next);

      // catch (e) { receiver = iter; f = 'throw'; arg = e; goto l_call; }
      __ bind(&l_catch);
      handler_table()->set(expr->index(), Smi::FromInt(l_catch.pos()));
      __ LoadRoot(r2, Heap::kthrow_stringRootIndex);  // "throw"
      __ ldr(r3, MemOperand(sp, 1 * kPointerSize));   // iter
      __ Push(r2, r3, r0);                            // "throw", iter, except
      __ jmp(&l_call);

      // try { received = %yield result }
      // Shuffle the received result above a try handler and yield it without
      // re-boxing.
      __ bind(&l_try);
      __ pop(r0);                                        // result
      __ PushTryHandler(StackHandler::CATCH, expr->index());
      const int handler_size = StackHandlerConstants::kSize;
      __ push(r0);                                       // result
      __ jmp(&l_suspend);
      __ bind(&l_continuation);
      __ jmp(&l_resume);
      __ bind(&l_suspend);
      const int generator_object_depth = kPointerSize + handler_size;
      __ ldr(r0, MemOperand(sp, generator_object_depth));
      __ push(r0);                                       // g
      ASSERT(l_continuation.pos() > 0 && Smi::IsValid(l_continuation.pos()));
      __ mov(r1, Operand(Smi::FromInt(l_continuation.pos())));
      __ str(r1, FieldMemOperand(r0, JSGeneratorObject::kContinuationOffset));
      __ str(cp, FieldMemOperand(r0, JSGeneratorObject::kContextOffset));
      __ mov(r1, cp);
      __ RecordWriteField(r0, JSGeneratorObject::kContextOffset, r1, r2,
                          kLRHasBeenSaved, kDontSaveFPRegs);
      __ CallRuntime(Runtime::kHiddenSuspendJSGeneratorObject, 1);
      __ ldr(cp, MemOperand(fp, StandardFrameConstants::kContextOffset));
      __ pop(r0);                                      // result
      EmitReturnSequence();
      __ bind(&l_resume);                              // received in r0
      __ PopTryHandler();

      // receiver = iter; f = 'next'; arg = received;
      __ bind(&l_next);
      __ LoadRoot(r2, Heap::knext_stringRootIndex);    // "next"
      __ ldr(r3, MemOperand(sp, 1 * kPointerSize));    // iter
      __ Push(r2, r3, r0);                             // "next", iter, received

      // result = receiver[f](arg);
      __ bind(&l_call);
      __ ldr(r1, MemOperand(sp, kPointerSize));
      __ ldr(r0, MemOperand(sp, 2 * kPointerSize));
      Handle<Code> ic = isolate()->builtins()->KeyedLoadIC_Initialize();
      CallIC(ic, TypeFeedbackId::None());
      __ mov(r1, r0);
      __ str(r1, MemOperand(sp, 2 * kPointerSize));
      CallFunctionStub stub(1, CALL_AS_METHOD);
      __ CallStub(&stub);

      __ ldr(cp, MemOperand(fp, StandardFrameConstants::kContextOffset));
      __ Drop(1);  // The function is still on the stack; drop it.

      // if (!result.done) goto l_try;
      __ bind(&l_loop);
      __ push(r0);                                       // save result
      __ LoadRoot(r2, Heap::kdone_stringRootIndex);      // "done"
      CallLoadIC(NOT_CONTEXTUAL);                        // result.done in r0
      Handle<Code> bool_ic = ToBooleanStub::GetUninitialized(isolate());
      CallIC(bool_ic);
      __ cmp(r0, Operand(0));
      __ b(eq, &l_try);

      // result.value
      __ pop(r0);                                        // result
      __ LoadRoot(r2, Heap::kvalue_stringRootIndex);     // "value"
      CallLoadIC(NOT_CONTEXTUAL);                        // result.value in r0
      context()->DropAndPlug(2, r0);                     // drop iter and g
      break;
    }
  }
}


void FullCodeGenerator::EmitGeneratorResume(Expression *generator,
    Expression *value,
    JSGeneratorObject::ResumeMode resume_mode) {
  // The value stays in r0, and is ultimately read by the resumed generator, as
  // if CallRuntime(Runtime::kHiddenSuspendJSGeneratorObject) returned it. Or it
  // is read to throw the value when the resumed generator is already closed.
  // r1 will hold the generator object until the activation has been resumed.
  VisitForStackValue(generator);
  VisitForAccumulatorValue(value);
  __ pop(r1);

  // Check generator state.
  Label wrong_state, closed_state, done;
  __ ldr(r3, FieldMemOperand(r1, JSGeneratorObject::kContinuationOffset));
  STATIC_ASSERT(JSGeneratorObject::kGeneratorExecuting < 0);
  STATIC_ASSERT(JSGeneratorObject::kGeneratorClosed == 0);
  __ cmp(r3, Operand(Smi::FromInt(0)));
  __ b(eq, &closed_state);
  __ b(lt, &wrong_state);

  // Load suspended function and context.
  __ ldr(cp, FieldMemOperand(r1, JSGeneratorObject::kContextOffset));
  __ ldr(r4, FieldMemOperand(r1, JSGeneratorObject::kFunctionOffset));

  // Load receiver and store as the first argument.
  __ ldr(r2, FieldMemOperand(r1, JSGeneratorObject::kReceiverOffset));
  __ push(r2);

  // Push holes for the rest of the arguments to the generator function.
  __ ldr(r3, FieldMemOperand(r4, JSFunction::kSharedFunctionInfoOffset));
  __ ldr(r3,
         FieldMemOperand(r3, SharedFunctionInfo::kFormalParameterCountOffset));
  __ LoadRoot(r2, Heap::kTheHoleValueRootIndex);
  Label push_argument_holes, push_frame;
  __ bind(&push_argument_holes);
  __ sub(r3, r3, Operand(Smi::FromInt(1)), SetCC);
  __ b(mi, &push_frame);
  __ push(r2);
  __ jmp(&push_argument_holes);

  // Enter a new JavaScript frame, and initialize its slots as they were when
  // the generator was suspended.
  Label resume_frame;
  __ bind(&push_frame);
  __ bl(&resume_frame);
  __ jmp(&done);
  __ bind(&resume_frame);
  // lr = return address.
  // fp = caller's frame pointer.
  // pp = caller's constant pool (if FLAG_enable_ool_constant_pool),
  // cp = callee's context,
  // r4 = callee's JS function.
  __ PushFixedFrame(r4);
  // Adjust FP to point to saved FP.
  __ add(fp, sp, Operand(StandardFrameConstants::kFixedFrameSizeFromFp));

  // Load the operand stack size.
  __ ldr(r3, FieldMemOperand(r1, JSGeneratorObject::kOperandStackOffset));
  __ ldr(r3, FieldMemOperand(r3, FixedArray::kLengthOffset));
  __ SmiUntag(r3);

  // If we are sending a value and there is no operand stack, we can jump back
  // in directly.
  if (resume_mode == JSGeneratorObject::NEXT) {
    Label slow_resume;
    __ cmp(r3, Operand(0));
    __ b(ne, &slow_resume);
    __ ldr(r3, FieldMemOperand(r4, JSFunction::kCodeEntryOffset));

    { ConstantPoolUnavailableScope constant_pool_unavailable(masm_);
      if (FLAG_enable_ool_constant_pool) {
        // Load the new code object's constant pool pointer.
        __ ldr(pp,
               MemOperand(r3, Code::kConstantPoolOffset - Code::kHeaderSize));
      }

      __ ldr(r2, FieldMemOperand(r1, JSGeneratorObject::kContinuationOffset));
      __ SmiUntag(r2);
      __ add(r3, r3, r2);
      __ mov(r2, Operand(Smi::FromInt(JSGeneratorObject::kGeneratorExecuting)));
      __ str(r2, FieldMemOperand(r1, JSGeneratorObject::kContinuationOffset));
      __ Jump(r3);
    }
    __ bind(&slow_resume);
  }

  // Otherwise, we push holes for the operand stack and call the runtime to fix
  // up the stack and the handlers.
  Label push_operand_holes, call_resume;
  __ bind(&push_operand_holes);
  __ sub(r3, r3, Operand(1), SetCC);
  __ b(mi, &call_resume);
  __ push(r2);
  __ b(&push_operand_holes);
  __ bind(&call_resume);
  ASSERT(!result_register().is(r1));
  __ Push(r1, result_register());
  __ Push(Smi::FromInt(resume_mode));
  __ CallRuntime(Runtime::kHiddenResumeJSGeneratorObject, 3);
  // Not reached: the runtime call returns elsewhere.
  __ stop("not-reached");

  // Reach here when generator is closed.
  __ bind(&closed_state);
  if (resume_mode == JSGeneratorObject::NEXT) {
    // Return completed iterator result when generator is closed.
    __ LoadRoot(r2, Heap::kUndefinedValueRootIndex);
    __ push(r2);
    // Pop value from top-of-stack slot; box result into result register.
    EmitCreateIteratorResult(true);
  } else {
    // Throw the provided value.
    __ push(r0);
    __ CallRuntime(Runtime::kHiddenThrow, 1);
  }
  __ jmp(&done);

  // Throw error if we attempt to operate on a running generator.
  __ bind(&wrong_state);
  __ push(r1);
  __ CallRuntime(Runtime::kHiddenThrowGeneratorStateError, 1);

  __ bind(&done);
  context()->Plug(result_register());
}


void FullCodeGenerator::EmitCreateIteratorResult(bool done) {
  Label gc_required;
  Label allocated;

  Handle<Map> map(isolate()->native_context()->generator_result_map());

  __ Allocate(map->instance_size(), r0, r2, r3, &gc_required, TAG_OBJECT);
  __ jmp(&allocated);

  __ bind(&gc_required);
  __ Push(Smi::FromInt(map->instance_size()));
  __ CallRuntime(Runtime::kHiddenAllocateInNewSpace, 1);
  __ ldr(context_register(),
         MemOperand(fp, StandardFrameConstants::kContextOffset));

  __ bind(&allocated);
  __ mov(r1, Operand(map));
  __ pop(r2);
  __ mov(r3, Operand(isolate()->factory()->ToBoolean(done)));
  __ mov(r4, Operand(isolate()->factory()->empty_fixed_array()));
  ASSERT_EQ(map->instance_size(), 5 * kPointerSize);
  __ str(r1, FieldMemOperand(r0, HeapObject::kMapOffset));
  __ str(r4, FieldMemOperand(r0, JSObject::kPropertiesOffset));
  __ str(r4, FieldMemOperand(r0, JSObject::kElementsOffset));
  __ str(r2,
         FieldMemOperand(r0, JSGeneratorObject::kResultValuePropertyOffset));
  __ str(r3,
         FieldMemOperand(r0, JSGeneratorObject::kResultDonePropertyOffset));

  // Only the value field needs a write barrier, as the other values are in the
  // root set.
  __ RecordWriteField(r0, JSGeneratorObject::kResultValuePropertyOffset,
                      r2, r3, kLRHasBeenSaved, kDontSaveFPRegs);
}


void FullCodeGenerator::EmitNamedPropertyLoad(Property* prop) {
  SetSourcePosition(prop->position());
  Literal* key = prop->key()->AsLiteral();
  __ mov(r2, Operand(key->value()));
  // Call load IC. It has arguments receiver and property name r0 and r2.
  CallLoadIC(NOT_CONTEXTUAL, prop->PropertyFeedbackId());
}


void FullCodeGenerator::EmitKeyedPropertyLoad(Property* prop) {
  SetSourcePosition(prop->position());
  // Call keyed load IC. It has arguments key and receiver in r0 and r1.
  Handle<Code> ic = isolate()->builtins()->KeyedLoadIC_Initialize();
  CallIC(ic, prop->PropertyFeedbackId());
}


void FullCodeGenerator::EmitInlineSmiBinaryOp(BinaryOperation* expr,
                                              Token::Value op,
                                              OverwriteMode mode,
                                              Expression* left_expr,
                                              Expression* right_expr) {
  Label done, smi_case, stub_call;

  Register scratch1 = r2;
  Register scratch2 = r3;

  // Get the arguments.
  Register left = r1;
  Register right = r0;
  __ pop(left);

  // Perform combined smi check on both operands.
  __ orr(scratch1, left, Operand(right));
  STATIC_ASSERT(kSmiTag == 0);
  JumpPatchSite patch_site(masm_);
  patch_site.EmitJumpIfSmi(scratch1, &smi_case);

  __ bind(&stub_call);
  BinaryOpICStub stub(op, mode);
  CallIC(stub.GetCode(isolate()), expr->BinaryOperationFeedbackId());
  patch_site.EmitPatchInfo();
  __ jmp(&done);

  __ bind(&smi_case);
  // Smi case. This code works the same way as the smi-smi case in the type
  // recording binary operation stub, see
  switch (op) {
    case Token::SAR:
      __ GetLeastBitsFromSmi(scratch1, right, 5);
      __ mov(right, Operand(left, ASR, scratch1));
      __ bic(right, right, Operand(kSmiTagMask));
      break;
    case Token::SHL: {
      __ SmiUntag(scratch1, left);
      __ GetLeastBitsFromSmi(scratch2, right, 5);
      __ mov(scratch1, Operand(scratch1, LSL, scratch2));
      __ TrySmiTag(right, scratch1, &stub_call);
      break;
    }
    case Token::SHR: {
      __ SmiUntag(scratch1, left);
      __ GetLeastBitsFromSmi(scratch2, right, 5);
      __ mov(scratch1, Operand(scratch1, LSR, scratch2));
      __ tst(scratch1, Operand(0xc0000000));
      __ b(ne, &stub_call);
      __ SmiTag(right, scratch1);
      break;
    }
    case Token::ADD:
      __ add(scratch1, left, Operand(right), SetCC);
      __ b(vs, &stub_call);
      __ mov(right, scratch1);
      break;
    case Token::SUB:
      __ sub(scratch1, left, Operand(right), SetCC);
      __ b(vs, &stub_call);
      __ mov(right, scratch1);
      break;
    case Token::MUL: {
      __ SmiUntag(ip, right);
      __ smull(scratch1, scratch2, left, ip);
      __ mov(ip, Operand(scratch1, ASR, 31));
      __ cmp(ip, Operand(scratch2));
      __ b(ne, &stub_call);
      __ cmp(scratch1, Operand::Zero());
      __ mov(right, Operand(scratch1), LeaveCC, ne);
      __ b(ne, &done);
      __ add(scratch2, right, Operand(left), SetCC);
      __ mov(right, Operand(Smi::FromInt(0)), LeaveCC, pl);
      __ b(mi, &stub_call);
      break;
    }
    case Token::BIT_OR:
      __ orr(right, left, Operand(right));
      break;
    case Token::BIT_AND:
      __ and_(right, left, Operand(right));
      break;
    case Token::BIT_XOR:
      __ eor(right, left, Operand(right));
      break;
    default:
      UNREACHABLE();
  }

  __ bind(&done);
  context()->Plug(r0);
}


void FullCodeGenerator::EmitBinaryOp(BinaryOperation* expr,
                                     Token::Value op,
                                     OverwriteMode mode) {
  __ pop(r1);
  BinaryOpICStub stub(op, mode);
  JumpPatchSite patch_site(masm_);    // unbound, signals no inlined smi code.
  CallIC(stub.GetCode(isolate()), expr->BinaryOperationFeedbackId());
  patch_site.EmitPatchInfo();
  context()->Plug(r0);
}


void FullCodeGenerator::EmitAssignment(Expression* expr) {
  ASSERT(expr->IsValidLeftHandSide());

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
      __ push(r0);  // Preserve value.
      VisitForAccumulatorValue(prop->obj());
      __ mov(r1, r0);
      __ pop(r0);  // Restore value.
      __ mov(r2, Operand(prop->key()->AsLiteral()->value()));
      CallStoreIC();
      break;
    }
    case KEYED_PROPERTY: {
      __ push(r0);  // Preserve value.
      VisitForStackValue(prop->obj());
      VisitForAccumulatorValue(prop->key());
      __ mov(r1, r0);
      __ Pop(r0, r2);  // r0 = restored value.
      Handle<Code> ic = strict_mode() == SLOPPY
          ? isolate()->builtins()->KeyedStoreIC_Initialize()
          : isolate()->builtins()->KeyedStoreIC_Initialize_Strict();
      CallIC(ic);
      break;
    }
  }
  context()->Plug(r0);
}


void FullCodeGenerator::EmitStoreToStackLocalOrContextSlot(
    Variable* var, MemOperand location) {
  __ str(result_register(), location);
  if (var->IsContextSlot()) {
    // RecordWrite may destroy all its register arguments.
    __ mov(r3, result_register());
    int offset = Context::SlotOffset(var->index());
    __ RecordWriteContextSlot(
        r1, offset, r3, r2, kLRHasBeenSaved, kDontSaveFPRegs);
  }
}


void FullCodeGenerator::EmitCallStoreContextSlot(
    Handle<String> name, StrictMode strict_mode) {
  __ push(r0);  // Value.
  __ mov(r1, Operand(name));
  __ mov(r0, Operand(Smi::FromInt(strict_mode)));
  __ Push(cp, r1, r0);  // Context, name, strict mode.
  __ CallRuntime(Runtime::kHiddenStoreContextSlot, 4);
}


void FullCodeGenerator::EmitVariableAssignment(Variable* var, Token::Value op) {
  if (var->IsUnallocated()) {
    // Global var, const, or let.
    __ mov(r2, Operand(var->name()));
    __ ldr(r1, GlobalObjectOperand());
    CallStoreIC();

  } else if (op == Token::INIT_CONST_LEGACY) {
    // Const initializers need a write barrier.
    ASSERT(!var->IsParameter());  // No const parameters.
    if (var->IsLookupSlot()) {
      __ push(r0);
      __ mov(r0, Operand(var->name()));
      __ Push(cp, r0);  // Context and name.
      __ CallRuntime(Runtime::kHiddenInitializeConstContextSlot, 3);
    } else {
      ASSERT(var->IsStackAllocated() || var->IsContextSlot());
      Label skip;
      MemOperand location = VarOperand(var, r1);
      __ ldr(r2, location);
      __ CompareRoot(r2, Heap::kTheHoleValueRootIndex);
      __ b(ne, &skip);
      EmitStoreToStackLocalOrContextSlot(var, location);
      __ bind(&skip);
    }

  } else if (var->mode() == LET && op != Token::INIT_LET) {
    // Non-initializing assignment to let variable needs a write barrier.
    if (var->IsLookupSlot()) {
      EmitCallStoreContextSlot(var->name(), strict_mode());
    } else {
      ASSERT(var->IsStackAllocated() || var->IsContextSlot());
      Label assign;
      MemOperand location = VarOperand(var, r1);
      __ ldr(r3, location);
      __ CompareRoot(r3, Heap::kTheHoleValueRootIndex);
      __ b(ne, &assign);
      __ mov(r3, Operand(var->name()));
      __ push(r3);
      __ CallRuntime(Runtime::kHiddenThrowReferenceError, 1);
      // Perform the assignment.
      __ bind(&assign);
      EmitStoreToStackLocalOrContextSlot(var, location);
    }

  } else if (!var->is_const_mode() || op == Token::INIT_CONST) {
    // Assignment to var or initializing assignment to let/const
    // in harmony mode.
    if (var->IsLookupSlot()) {
      EmitCallStoreContextSlot(var->name(), strict_mode());
    } else {
      ASSERT((var->IsStackAllocated() || var->IsContextSlot()));
      MemOperand location = VarOperand(var, r1);
      if (generate_debug_code_ && op == Token::INIT_LET) {
        // Check for an uninitialized let binding.
        __ ldr(r2, location);
        __ CompareRoot(r2, Heap::kTheHoleValueRootIndex);
        __ Check(eq, kLetBindingReInitialization);
      }
      EmitStoreToStackLocalOrContextSlot(var, location);
    }
  }
  // Non-initializing assignments to consts are ignored.
}


void FullCodeGenerator::EmitNamedPropertyAssignment(Assignment* expr) {
  // Assignment to a property, using a named store IC.
  Property* prop = expr->target()->AsProperty();
  ASSERT(prop != NULL);
  ASSERT(prop->key()->AsLiteral() != NULL);

  // Record source code position before IC call.
  SetSourcePosition(expr->position());
  __ mov(r2, Operand(prop->key()->AsLiteral()->value()));
  __ pop(r1);

  CallStoreIC(expr->AssignmentFeedbackId());

  PrepareForBailoutForId(expr->AssignmentId(), TOS_REG);
  context()->Plug(r0);
}


void FullCodeGenerator::EmitKeyedPropertyAssignment(Assignment* expr) {
  // Assignment to a property, using a keyed store IC.

  // Record source code position before IC call.
  SetSourcePosition(expr->position());
  __ Pop(r2, r1);  // r1 = key.

  Handle<Code> ic = strict_mode() == SLOPPY
      ? isolate()->builtins()->KeyedStoreIC_Initialize()
      : isolate()->builtins()->KeyedStoreIC_Initialize_Strict();
  CallIC(ic, expr->AssignmentFeedbackId());

  PrepareForBailoutForId(expr->AssignmentId(), TOS_REG);
  context()->Plug(r0);
}


void FullCodeGenerator::VisitProperty(Property* expr) {
  Comment cmnt(masm_, "[ Property");
  Expression* key = expr->key();

  if (key->IsPropertyName()) {
    VisitForAccumulatorValue(expr->obj());
    EmitNamedPropertyLoad(expr);
    PrepareForBailoutForId(expr->LoadId(), TOS_REG);
    context()->Plug(r0);
  } else {
    VisitForStackValue(expr->obj());
    VisitForAccumulatorValue(expr->key());
    __ pop(r1);
    EmitKeyedPropertyLoad(expr);
    context()->Plug(r0);
  }
}


void FullCodeGenerator::CallIC(Handle<Code> code,
                               TypeFeedbackId ast_id) {
  ic_total_count_++;
  // All calls must have a predictable size in full-codegen code to ensure that
  // the debugger can patch them correctly.
  __ Call(code, RelocInfo::CODE_TARGET, ast_id, al,
          NEVER_INLINE_TARGET_ADDRESS);
}


// Code common for calls using the IC.
void FullCodeGenerator::EmitCallWithIC(Call* expr) {
  Expression* callee = expr->expression();
  ZoneList<Expression*>* args = expr->arguments();
  int arg_count = args->length();

  CallFunctionFlags flags;
  // Get the target function.
  if (callee->IsVariableProxy()) {
    { StackValueContext context(this);
      EmitVariableLoad(callee->AsVariableProxy());
      PrepareForBailout(callee, NO_REGISTERS);
    }
    // Push undefined as receiver. This is patched in the method prologue if it
    // is a sloppy mode method.
    __ Push(isolate()->factory()->undefined_value());
    flags = NO_CALL_FUNCTION_FLAGS;
  } else {
    // Load the function from the receiver.
    ASSERT(callee->IsProperty());
    __ ldr(r0, MemOperand(sp, 0));
    EmitNamedPropertyLoad(callee->AsProperty());
    PrepareForBailoutForId(callee->AsProperty()->LoadId(), TOS_REG);
    // Push the target function under the receiver.
    __ ldr(ip, MemOperand(sp, 0));
    __ push(ip);
    __ str(r0, MemOperand(sp, kPointerSize));
    flags = CALL_AS_METHOD;
  }

  // Load the arguments.
  { PreservePositionScope scope(masm()->positions_recorder());
    for (int i = 0; i < arg_count; i++) {
      VisitForStackValue(args->at(i));
    }
  }

  // Record source position for debugger.
  SetSourcePosition(expr->position());
  CallFunctionStub stub(arg_count, flags);
  __ ldr(r1, MemOperand(sp, (arg_count + 1) * kPointerSize));
  __ CallStub(&stub);

  RecordJSReturnSite(expr);

  // Restore context register.
  __ ldr(cp, MemOperand(fp, StandardFrameConstants::kContextOffset));

  context()->DropAndPlug(1, r0);
}


// Code common for calls using the IC.
void FullCodeGenerator::EmitKeyedCallWithIC(Call* expr,
                                            Expression* key) {
  // Load the key.
  VisitForAccumulatorValue(key);

  Expression* callee = expr->expression();
  ZoneList<Expression*>* args = expr->arguments();
  int arg_count = args->length();

  // Load the function from the receiver.
  ASSERT(callee->IsProperty());
  __ ldr(r1, MemOperand(sp, 0));
  EmitKeyedPropertyLoad(callee->AsProperty());
  PrepareForBailoutForId(callee->AsProperty()->LoadId(), TOS_REG);

  // Push the target function under the receiver.
  __ ldr(ip, MemOperand(sp, 0));
  __ push(ip);
  __ str(r0, MemOperand(sp, kPointerSize));

  { PreservePositionScope scope(masm()->positions_recorder());
    for (int i = 0; i < arg_count; i++) {
      VisitForStackValue(args->at(i));
    }
  }

  // Record source position for debugger.
  SetSourcePosition(expr->position());
  CallFunctionStub stub(arg_count, CALL_AS_METHOD);
  __ ldr(r1, MemOperand(sp, (arg_count + 1) * kPointerSize));
  __ CallStub(&stub);

  RecordJSReturnSite(expr);
  // Restore context register.
  __ ldr(cp, MemOperand(fp, StandardFrameConstants::kContextOffset));

  context()->DropAndPlug(1, r0);
}


void FullCodeGenerator::EmitCallWithStub(Call* expr) {
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

  Handle<Object> uninitialized =
      TypeFeedbackInfo::UninitializedSentinel(isolate());
  StoreFeedbackVectorSlot(expr->CallFeedbackSlot(), uninitialized);
  __ Move(r2, FeedbackVector());
  __ mov(r3, Operand(Smi::FromInt(expr->CallFeedbackSlot())));

  // Record call targets in unoptimized code.
  CallFunctionStub stub(arg_count, RECORD_CALL_TARGET);
  __ ldr(r1, MemOperand(sp, (arg_count + 1) * kPointerSize));
  __ CallStub(&stub);
  RecordJSReturnSite(expr);
  // Restore context register.
  __ ldr(cp, MemOperand(fp, StandardFrameConstants::kContextOffset));
  context()->DropAndPlug(1, r0);
}


void FullCodeGenerator::EmitResolvePossiblyDirectEval(int arg_count) {
  // r4: copy of the first argument or undefined if it doesn't exist.
  if (arg_count > 0) {
    __ ldr(r4, MemOperand(sp, arg_count * kPointerSize));
  } else {
    __ LoadRoot(r4, Heap::kUndefinedValueRootIndex);
  }

  // r3: the receiver of the enclosing function.
  int receiver_offset = 2 + info_->scope()->num_parameters();
  __ ldr(r3, MemOperand(fp, receiver_offset * kPointerSize));

  // r2: strict mode.
  __ mov(r2, Operand(Smi::FromInt(strict_mode())));

  // r1: the start position of the scope the calls resides in.
  __ mov(r1, Operand(Smi::FromInt(scope()->start_position())));

  // Do the runtime call.
  __ Push(r4, r3, r2, r1);
  __ CallRuntime(Runtime::kHiddenResolvePossiblyDirectEval, 5);
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
    // to resolve the function we need to call and the receiver of the
    // call.  Then we call the resolved function using the given
    // arguments.
    ZoneList<Expression*>* args = expr->arguments();
    int arg_count = args->length();

    { PreservePositionScope pos_scope(masm()->positions_recorder());
      VisitForStackValue(callee);
      __ LoadRoot(r2, Heap::kUndefinedValueRootIndex);
      __ push(r2);  // Reserved receiver slot.

      // Push the arguments.
      for (int i = 0; i < arg_count; i++) {
        VisitForStackValue(args->at(i));
      }

      // Push a copy of the function (found below the arguments) and
      // resolve eval.
      __ ldr(r1, MemOperand(sp, (arg_count + 1) * kPointerSize));
      __ push(r1);
      EmitResolvePossiblyDirectEval(arg_count);

      // The runtime call returns a pair of values in r0 (function) and
      // r1 (receiver). Touch up the stack with the right values.
      __ str(r0, MemOperand(sp, (arg_count + 1) * kPointerSize));
      __ str(r1, MemOperand(sp, arg_count * kPointerSize));
    }

    // Record source position for debugger.
    SetSourcePosition(expr->position());
    CallFunctionStub stub(arg_count, NO_CALL_FUNCTION_FLAGS);
    __ ldr(r1, MemOperand(sp, (arg_count + 1) * kPointerSize));
    __ CallStub(&stub);
    RecordJSReturnSite(expr);
    // Restore context register.
    __ ldr(cp, MemOperand(fp, StandardFrameConstants::kContextOffset));
    context()->DropAndPlug(1, r0);
  } else if (call_type == Call::GLOBAL_CALL) {
    EmitCallWithIC(expr);

  } else if (call_type == Call::LOOKUP_SLOT_CALL) {
    // Call to a lookup slot (dynamically introduced variable).
    VariableProxy* proxy = callee->AsVariableProxy();
    Label slow, done;

    { PreservePositionScope scope(masm()->positions_recorder());
      // Generate code for loading from variables potentially shadowed
      // by eval-introduced variables.
      EmitDynamicLookupFastCase(proxy->var(), NOT_INSIDE_TYPEOF, &slow, &done);
    }

    __ bind(&slow);
    // Call the runtime to find the function to call (returned in r0)
    // and the object holding it (returned in edx).
    ASSERT(!context_register().is(r2));
    __ mov(r2, Operand(proxy->name()));
    __ Push(context_register(), r2);
    __ CallRuntime(Runtime::kHiddenLoadContextSlot, 2);
    __ Push(r0, r1);  // Function, receiver.

    // If fast case code has been generated, emit code to push the
    // function and receiver and have the slow path jump around this
    // code.
    if (done.is_linked()) {
      Label call;
      __ b(&call);
      __ bind(&done);
      // Push function.
      __ push(r0);
      // The receiver is implicitly the global receiver. Indicate this
      // by passing the hole to the call function stub.
      __ LoadRoot(r1, Heap::kUndefinedValueRootIndex);
      __ push(r1);
      __ bind(&call);
    }

    // The receiver is either the global receiver or an object found
    // by LoadContextSlot.
    EmitCallWithStub(expr);
  } else if (call_type == Call::PROPERTY_CALL) {
    Property* property = callee->AsProperty();
    { PreservePositionScope scope(masm()->positions_recorder());
      VisitForStackValue(property->obj());
    }
    if (property->key()->IsPropertyName()) {
      EmitCallWithIC(expr);
    } else {
      EmitKeyedCallWithIC(expr, property->key());
    }
  } else {
    ASSERT(call_type == Call::OTHER_CALL);
    // Call to an arbitrary expression not handled specially above.
    { PreservePositionScope scope(masm()->positions_recorder());
      VisitForStackValue(callee);
    }
    __ LoadRoot(r1, Heap::kUndefinedValueRootIndex);
    __ push(r1);
    // Emit function call.
    EmitCallWithStub(expr);
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

  // Load function and argument count into r1 and r0.
  __ mov(r0, Operand(arg_count));
  __ ldr(r1, MemOperand(sp, arg_count * kPointerSize));

  // Record call targets in unoptimized code.
  Handle<Object> uninitialized =
      TypeFeedbackInfo::UninitializedSentinel(isolate());
  StoreFeedbackVectorSlot(expr->CallNewFeedbackSlot(), uninitialized);
  if (FLAG_pretenuring_call_new) {
    StoreFeedbackVectorSlot(expr->AllocationSiteFeedbackSlot(),
                            isolate()->factory()->NewAllocationSite());
    ASSERT(expr->AllocationSiteFeedbackSlot() ==
           expr->CallNewFeedbackSlot() + 1);
  }

  __ Move(r2, FeedbackVector());
  __ mov(r3, Operand(Smi::FromInt(expr->CallNewFeedbackSlot())));

  CallConstructStub stub(RECORD_CALL_TARGET);
  __ Call(stub.GetCode(isolate()), RelocInfo::CONSTRUCT_CALL);
  PrepareForBailoutForId(expr->ReturnId(), TOS_REG);
  context()->Plug(r0);
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
  __ SmiTst(r0);
  Split(eq, if_true, if_false, fall_through);

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
  __ NonNegativeSmiTst(r0);
  Split(eq, if_true, if_false, fall_through);

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

  __ JumpIfSmi(r0, if_false);
  __ LoadRoot(ip, Heap::kNullValueRootIndex);
  __ cmp(r0, ip);
  __ b(eq, if_true);
  __ ldr(r2, FieldMemOperand(r0, HeapObject::kMapOffset));
  // Undetectable objects behave like undefined when tested with typeof.
  __ ldrb(r1, FieldMemOperand(r2, Map::kBitFieldOffset));
  __ tst(r1, Operand(1 << Map::kIsUndetectable));
  __ b(ne, if_false);
  __ ldrb(r1, FieldMemOperand(r2, Map::kInstanceTypeOffset));
  __ cmp(r1, Operand(FIRST_NONCALLABLE_SPEC_OBJECT_TYPE));
  __ b(lt, if_false);
  __ cmp(r1, Operand(LAST_NONCALLABLE_SPEC_OBJECT_TYPE));
  PrepareForBailoutBeforeSplit(expr, true, if_true, if_false);
  Split(le, if_true, if_false, fall_through);

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

  __ JumpIfSmi(r0, if_false);
  __ CompareObjectType(r0, r1, r1, FIRST_SPEC_OBJECT_TYPE);
  PrepareForBailoutBeforeSplit(expr, true, if_true, if_false);
  Split(ge, if_true, if_false, fall_through);

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

  __ JumpIfSmi(r0, if_false);
  __ ldr(r1, FieldMemOperand(r0, HeapObject::kMapOffset));
  __ ldrb(r1, FieldMemOperand(r1, Map::kBitFieldOffset));
  __ tst(r1, Operand(1 << Map::kIsUndetectable));
  PrepareForBailoutBeforeSplit(expr, true, if_true, if_false);
  Split(ne, if_true, if_false, fall_through);

  context()->Plug(if_true, if_false);
}


void FullCodeGenerator::EmitIsStringWrapperSafeForDefaultValueOf(
    CallRuntime* expr) {
  ZoneList<Expression*>* args = expr->arguments();
  ASSERT(args->length() == 1);

  VisitForAccumulatorValue(args->at(0));

  Label materialize_true, materialize_false, skip_lookup;
  Label* if_true = NULL;
  Label* if_false = NULL;
  Label* fall_through = NULL;
  context()->PrepareTest(&materialize_true, &materialize_false,
                         &if_true, &if_false, &fall_through);

  __ AssertNotSmi(r0);

  __ ldr(r1, FieldMemOperand(r0, HeapObject::kMapOffset));
  __ ldrb(ip, FieldMemOperand(r1, Map::kBitField2Offset));
  __ tst(ip, Operand(1 << Map::kStringWrapperSafeForDefaultValueOf));
  __ b(ne, &skip_lookup);

  // Check for fast case object. Generate false result for slow case object.
  __ ldr(r2, FieldMemOperand(r0, JSObject::kPropertiesOffset));
  __ ldr(r2, FieldMemOperand(r2, HeapObject::kMapOffset));
  __ LoadRoot(ip, Heap::kHashTableMapRootIndex);
  __ cmp(r2, ip);
  __ b(eq, if_false);

  // Look for valueOf name in the descriptor array, and indicate false if
  // found. Since we omit an enumeration index check, if it is added via a
  // transition that shares its descriptor array, this is a false positive.
  Label entry, loop, done;

  // Skip loop if no descriptors are valid.
  __ NumberOfOwnDescriptors(r3, r1);
  __ cmp(r3, Operand::Zero());
  __ b(eq, &done);

  __ LoadInstanceDescriptors(r1, r4);
  // r4: descriptor array.
  // r3: valid entries in the descriptor array.
  __ mov(ip, Operand(DescriptorArray::kDescriptorSize));
  __ mul(r3, r3, ip);
  // Calculate location of the first key name.
  __ add(r4, r4, Operand(DescriptorArray::kFirstOffset - kHeapObjectTag));
  // Calculate the end of the descriptor array.
  __ mov(r2, r4);
  __ add(r2, r2, Operand::PointerOffsetFromSmiKey(r3));

  // Loop through all the keys in the descriptor array. If one of these is the
  // string "valueOf" the result is false.
  // The use of ip to store the valueOf string assumes that it is not otherwise
  // used in the loop below.
  __ mov(ip, Operand(isolate()->factory()->value_of_string()));
  __ jmp(&entry);
  __ bind(&loop);
  __ ldr(r3, MemOperand(r4, 0));
  __ cmp(r3, ip);
  __ b(eq, if_false);
  __ add(r4, r4, Operand(DescriptorArray::kDescriptorSize * kPointerSize));
  __ bind(&entry);
  __ cmp(r4, Operand(r2));
  __ b(ne, &loop);

  __ bind(&done);

  // Set the bit in the map to indicate that there is no local valueOf field.
  __ ldrb(r2, FieldMemOperand(r1, Map::kBitField2Offset));
  __ orr(r2, r2, Operand(1 << Map::kStringWrapperSafeForDefaultValueOf));
  __ strb(r2, FieldMemOperand(r1, Map::kBitField2Offset));

  __ bind(&skip_lookup);

  // If a valueOf property is not found on the object check that its
  // prototype is the un-modified String prototype. If not result is false.
  __ ldr(r2, FieldMemOperand(r1, Map::kPrototypeOffset));
  __ JumpIfSmi(r2, if_false);
  __ ldr(r2, FieldMemOperand(r2, HeapObject::kMapOffset));
  __ ldr(r3, ContextOperand(cp, Context::GLOBAL_OBJECT_INDEX));
  __ ldr(r3, FieldMemOperand(r3, GlobalObject::kNativeContextOffset));
  __ ldr(r3, ContextOperand(r3, Context::STRING_FUNCTION_PROTOTYPE_MAP_INDEX));
  __ cmp(r2, r3);
  PrepareForBailoutBeforeSplit(expr, true, if_true, if_false);
  Split(eq, if_true, if_false, fall_through);

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

  __ JumpIfSmi(r0, if_false);
  __ CompareObjectType(r0, r1, r2, JS_FUNCTION_TYPE);
  PrepareForBailoutBeforeSplit(expr, true, if_true, if_false);
  Split(eq, if_true, if_false, fall_through);

  context()->Plug(if_true, if_false);
}


void FullCodeGenerator::EmitIsMinusZero(CallRuntime* expr) {
  ZoneList<Expression*>* args = expr->arguments();
  ASSERT(args->length() == 1);

  VisitForAccumulatorValue(args->at(0));

  Label materialize_true, materialize_false;
  Label* if_true = NULL;
  Label* if_false = NULL;
  Label* fall_through = NULL;
  context()->PrepareTest(&materialize_true, &materialize_false,
                         &if_true, &if_false, &fall_through);

  __ CheckMap(r0, r1, Heap::kHeapNumberMapRootIndex, if_false, DO_SMI_CHECK);
  __ ldr(r2, FieldMemOperand(r0, HeapNumber::kExponentOffset));
  __ ldr(r1, FieldMemOperand(r0, HeapNumber::kMantissaOffset));
  __ cmp(r2, Operand(0x80000000));
  __ cmp(r1, Operand(0x00000000), eq);

  PrepareForBailoutBeforeSplit(expr, true, if_true, if_false);
  Split(eq, if_true, if_false, fall_through);

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

  __ JumpIfSmi(r0, if_false);
  __ CompareObjectType(r0, r1, r1, JS_ARRAY_TYPE);
  PrepareForBailoutBeforeSplit(expr, true, if_true, if_false);
  Split(eq, if_true, if_false, fall_through);

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

  __ JumpIfSmi(r0, if_false);
  __ CompareObjectType(r0, r1, r1, JS_REGEXP_TYPE);
  PrepareForBailoutBeforeSplit(expr, true, if_true, if_false);
  Split(eq, if_true, if_false, fall_through);

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
  __ ldr(r2, MemOperand(fp, StandardFrameConstants::kCallerFPOffset));

  // Skip the arguments adaptor frame if it exists.
  __ ldr(r1, MemOperand(r2, StandardFrameConstants::kContextOffset));
  __ cmp(r1, Operand(Smi::FromInt(StackFrame::ARGUMENTS_ADAPTOR)));
  __ ldr(r2, MemOperand(r2, StandardFrameConstants::kCallerFPOffset), eq);

  // Check the marker in the calling frame.
  __ ldr(r1, MemOperand(r2, StandardFrameConstants::kMarkerOffset));
  __ cmp(r1, Operand(Smi::FromInt(StackFrame::CONSTRUCT)));
  PrepareForBailoutBeforeSplit(expr, true, if_true, if_false);
  Split(eq, if_true, if_false, fall_through);

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

  __ pop(r1);
  __ cmp(r0, r1);
  PrepareForBailoutBeforeSplit(expr, true, if_true, if_false);
  Split(eq, if_true, if_false, fall_through);

  context()->Plug(if_true, if_false);
}


void FullCodeGenerator::EmitArguments(CallRuntime* expr) {
  ZoneList<Expression*>* args = expr->arguments();
  ASSERT(args->length() == 1);

  // ArgumentsAccessStub expects the key in edx and the formal
  // parameter count in r0.
  VisitForAccumulatorValue(args->at(0));
  __ mov(r1, r0);
  __ mov(r0, Operand(Smi::FromInt(info_->scope()->num_parameters())));
  ArgumentsAccessStub stub(ArgumentsAccessStub::READ_ELEMENT);
  __ CallStub(&stub);
  context()->Plug(r0);
}


void FullCodeGenerator::EmitArgumentsLength(CallRuntime* expr) {
  ASSERT(expr->arguments()->length() == 0);

  // Get the number of formal parameters.
  __ mov(r0, Operand(Smi::FromInt(info_->scope()->num_parameters())));

  // Check if the calling frame is an arguments adaptor frame.
  __ ldr(r2, MemOperand(fp, StandardFrameConstants::kCallerFPOffset));
  __ ldr(r3, MemOperand(r2, StandardFrameConstants::kContextOffset));
  __ cmp(r3, Operand(Smi::FromInt(StackFrame::ARGUMENTS_ADAPTOR)));

  // Arguments adaptor case: Read the arguments length from the
  // adaptor frame.
  __ ldr(r0, MemOperand(r2, ArgumentsAdaptorFrameConstants::kLengthOffset), eq);

  context()->Plug(r0);
}


void FullCodeGenerator::EmitClassOf(CallRuntime* expr) {
  ZoneList<Expression*>* args = expr->arguments();
  ASSERT(args->length() == 1);
  Label done, null, function, non_function_constructor;

  VisitForAccumulatorValue(args->at(0));

  // If the object is a smi, we return null.
  __ JumpIfSmi(r0, &null);

  // Check that the object is a JS object but take special care of JS
  // functions to make sure they have 'Function' as their class.
  // Assume that there are only two callable types, and one of them is at
  // either end of the type range for JS object types. Saves extra comparisons.
  STATIC_ASSERT(NUM_OF_CALLABLE_SPEC_OBJECT_TYPES == 2);
  __ CompareObjectType(r0, r0, r1, FIRST_SPEC_OBJECT_TYPE);
  // Map is now in r0.
  __ b(lt, &null);
  STATIC_ASSERT(FIRST_NONCALLABLE_SPEC_OBJECT_TYPE ==
                FIRST_SPEC_OBJECT_TYPE + 1);
  __ b(eq, &function);

  __ cmp(r1, Operand(LAST_SPEC_OBJECT_TYPE));
  STATIC_ASSERT(LAST_NONCALLABLE_SPEC_OBJECT_TYPE ==
                LAST_SPEC_OBJECT_TYPE - 1);
  __ b(eq, &function);
  // Assume that there is no larger type.
  STATIC_ASSERT(LAST_NONCALLABLE_SPEC_OBJECT_TYPE == LAST_TYPE - 1);

  // Check if the constructor in the map is a JS function.
  __ ldr(r0, FieldMemOperand(r0, Map::kConstructorOffset));
  __ CompareObjectType(r0, r1, r1, JS_FUNCTION_TYPE);
  __ b(ne, &non_function_constructor);

  // r0 now contains the constructor function. Grab the
  // instance class name from there.
  __ ldr(r0, FieldMemOperand(r0, JSFunction::kSharedFunctionInfoOffset));
  __ ldr(r0, FieldMemOperand(r0, SharedFunctionInfo::kInstanceClassNameOffset));
  __ b(&done);

  // Functions have class 'Function'.
  __ bind(&function);
  __ LoadRoot(r0, Heap::kfunction_class_stringRootIndex);
  __ jmp(&done);

  // Objects with a non-function constructor have class 'Object'.
  __ bind(&non_function_constructor);
  __ LoadRoot(r0, Heap::kObject_stringRootIndex);
  __ jmp(&done);

  // Non-JS objects have class null.
  __ bind(&null);
  __ LoadRoot(r0, Heap::kNullValueRootIndex);

  // All done.
  __ bind(&done);

  context()->Plug(r0);
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
  if (CodeGenerator::ShouldGenerateLog(isolate(), args->at(0))) {
    VisitForStackValue(args->at(1));
    VisitForStackValue(args->at(2));
    __ CallRuntime(Runtime::kHiddenLog, 2);
  }

  // Finally, we're expected to leave a value on the top of the stack.
  __ LoadRoot(r0, Heap::kUndefinedValueRootIndex);
  context()->Plug(r0);
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
  context()->Plug(r0);
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
  context()->Plug(r0);
}


void FullCodeGenerator::EmitValueOf(CallRuntime* expr) {
  ZoneList<Expression*>* args = expr->arguments();
  ASSERT(args->length() == 1);
  VisitForAccumulatorValue(args->at(0));  // Load the object.

  Label done;
  // If the object is a smi return the object.
  __ JumpIfSmi(r0, &done);
  // If the object is not a value type, return the object.
  __ CompareObjectType(r0, r1, r1, JS_VALUE_TYPE);
  __ ldr(r0, FieldMemOperand(r0, JSValue::kValueOffset), eq);

  __ bind(&done);
  context()->Plug(r0);
}


void FullCodeGenerator::EmitDateField(CallRuntime* expr) {
  ZoneList<Expression*>* args = expr->arguments();
  ASSERT(args->length() == 2);
  ASSERT_NE(NULL, args->at(1)->AsLiteral());
  Smi* index = Smi::cast(*(args->at(1)->AsLiteral()->value()));

  VisitForAccumulatorValue(args->at(0));  // Load the object.

  Label runtime, done, not_date_object;
  Register object = r0;
  Register result = r0;
  Register scratch0 = r9;
  Register scratch1 = r1;

  __ JumpIfSmi(object, &not_date_object);
  __ CompareObjectType(object, scratch1, scratch1, JS_DATE_TYPE);
  __ b(ne, &not_date_object);

  if (index->value() == 0) {
    __ ldr(result, FieldMemOperand(object, JSDate::kValueOffset));
    __ jmp(&done);
  } else {
    if (index->value() < JSDate::kFirstUncachedField) {
      ExternalReference stamp = ExternalReference::date_cache_stamp(isolate());
      __ mov(scratch1, Operand(stamp));
      __ ldr(scratch1, MemOperand(scratch1));
      __ ldr(scratch0, FieldMemOperand(object, JSDate::kCacheStampOffset));
      __ cmp(scratch1, scratch0);
      __ b(ne, &runtime);
      __ ldr(result, FieldMemOperand(object, JSDate::kValueOffset +
                                             kPointerSize * index->value()));
      __ jmp(&done);
    }
    __ bind(&runtime);
    __ PrepareCallCFunction(2, scratch1);
    __ mov(r1, Operand(index));
    __ CallCFunction(ExternalReference::get_date_field_function(isolate()), 2);
    __ jmp(&done);
  }

  __ bind(&not_date_object);
  __ CallRuntime(Runtime::kHiddenThrowNotDateError, 0);
  __ bind(&done);
  context()->Plug(r0);
}


void FullCodeGenerator::EmitOneByteSeqStringSetChar(CallRuntime* expr) {
  ZoneList<Expression*>* args = expr->arguments();
  ASSERT_EQ(3, args->length());

  Register string = r0;
  Register index = r1;
  Register value = r2;

  VisitForStackValue(args->at(1));  // index
  VisitForStackValue(args->at(2));  // value
  VisitForAccumulatorValue(args->at(0));  // string
  __ Pop(index, value);

  if (FLAG_debug_code) {
    __ SmiTst(value);
    __ Check(eq, kNonSmiValue);
    __ SmiTst(index);
    __ Check(eq, kNonSmiIndex);
    __ SmiUntag(index, index);
    static const uint32_t one_byte_seq_type = kSeqStringTag | kOneByteStringTag;
    __ EmitSeqStringSetCharCheck(string, index, value, one_byte_seq_type);
    __ SmiTag(index, index);
  }

  __ SmiUntag(value, value);
  __ add(ip,
         string,
         Operand(SeqOneByteString::kHeaderSize - kHeapObjectTag));
  __ strb(value, MemOperand(ip, index, LSR, kSmiTagSize));
  context()->Plug(string);
}


void FullCodeGenerator::EmitTwoByteSeqStringSetChar(CallRuntime* expr) {
  ZoneList<Expression*>* args = expr->arguments();
  ASSERT_EQ(3, args->length());

  Register string = r0;
  Register index = r1;
  Register value = r2;

  VisitForStackValue(args->at(1));  // index
  VisitForStackValue(args->at(2));  // value
  VisitForAccumulatorValue(args->at(0));  // string
  __ Pop(index, value);

  if (FLAG_debug_code) {
    __ SmiTst(value);
    __ Check(eq, kNonSmiValue);
    __ SmiTst(index);
    __ Check(eq, kNonSmiIndex);
    __ SmiUntag(index, index);
    static const uint32_t two_byte_seq_type = kSeqStringTag | kTwoByteStringTag;
    __ EmitSeqStringSetCharCheck(string, index, value, two_byte_seq_type);
    __ SmiTag(index, index);
  }

  __ SmiUntag(value, value);
  __ add(ip,
         string,
         Operand(SeqTwoByteString::kHeaderSize - kHeapObjectTag));
  STATIC_ASSERT(kSmiTagSize == 1 && kSmiTag == 0);
  __ strh(value, MemOperand(ip, index));
  context()->Plug(string);
}



void FullCodeGenerator::EmitMathPow(CallRuntime* expr) {
  // Load the arguments on the stack and call the runtime function.
  ZoneList<Expression*>* args = expr->arguments();
  ASSERT(args->length() == 2);
  VisitForStackValue(args->at(0));
  VisitForStackValue(args->at(1));
  MathPowStub stub(MathPowStub::ON_STACK);
  __ CallStub(&stub);
  context()->Plug(r0);
}


void FullCodeGenerator::EmitSetValueOf(CallRuntime* expr) {
  ZoneList<Expression*>* args = expr->arguments();
  ASSERT(args->length() == 2);
  VisitForStackValue(args->at(0));  // Load the object.
  VisitForAccumulatorValue(args->at(1));  // Load the value.
  __ pop(r1);  // r0 = value. r1 = object.

  Label done;
  // If the object is a smi, return the value.
  __ JumpIfSmi(r1, &done);

  // If the object is not a value type, return the value.
  __ CompareObjectType(r1, r2, r2, JS_VALUE_TYPE);
  __ b(ne, &done);

  // Store the value.
  __ str(r0, FieldMemOperand(r1, JSValue::kValueOffset));
  // Update the write barrier.  Save the value as it will be
  // overwritten by the write barrier code and is needed afterward.
  __ mov(r2, r0);
  __ RecordWriteField(
      r1, JSValue::kValueOffset, r2, r3, kLRHasBeenSaved, kDontSaveFPRegs);

  __ bind(&done);
  context()->Plug(r0);
}


void FullCodeGenerator::EmitNumberToString(CallRuntime* expr) {
  ZoneList<Expression*>* args = expr->arguments();
  ASSERT_EQ(args->length(), 1);
  // Load the argument into r0 and call the stub.
  VisitForAccumulatorValue(args->at(0));

  NumberToStringStub stub;
  __ CallStub(&stub);
  context()->Plug(r0);
}


void FullCodeGenerator::EmitStringCharFromCode(CallRuntime* expr) {
  ZoneList<Expression*>* args = expr->arguments();
  ASSERT(args->length() == 1);
  VisitForAccumulatorValue(args->at(0));

  Label done;
  StringCharFromCodeGenerator generator(r0, r1);
  generator.GenerateFast(masm_);
  __ jmp(&done);

  NopRuntimeCallHelper call_helper;
  generator.GenerateSlow(masm_, call_helper);

  __ bind(&done);
  context()->Plug(r1);
}


void FullCodeGenerator::EmitStringCharCodeAt(CallRuntime* expr) {
  ZoneList<Expression*>* args = expr->arguments();
  ASSERT(args->length() == 2);
  VisitForStackValue(args->at(0));
  VisitForAccumulatorValue(args->at(1));

  Register object = r1;
  Register index = r0;
  Register result = r3;

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
  // Load the undefined value into the result register, which will
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

  Register object = r1;
  Register index = r0;
  Register scratch = r3;
  Register result = r0;

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
  __ LoadRoot(result, Heap::kempty_stringRootIndex);
  __ jmp(&done);

  __ bind(&need_conversion);
  // Move smi zero into the result register, which will trigger
  // conversion.
  __ mov(result, Operand(Smi::FromInt(0)));
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
  VisitForAccumulatorValue(args->at(1));

  __ pop(r1);
  StringAddStub stub(STRING_ADD_CHECK_BOTH, NOT_TENURED);
  __ CallStub(&stub);
  context()->Plug(r0);
}


void FullCodeGenerator::EmitStringCompare(CallRuntime* expr) {
  ZoneList<Expression*>* args = expr->arguments();
  ASSERT_EQ(2, args->length());
  VisitForStackValue(args->at(0));
  VisitForStackValue(args->at(1));

  StringCompareStub stub;
  __ CallStub(&stub);
  context()->Plug(r0);
}


void FullCodeGenerator::EmitMathLog(CallRuntime* expr) {
  // Load the argument on the stack and call the runtime function.
  ZoneList<Expression*>* args = expr->arguments();
  ASSERT(args->length() == 1);
  VisitForStackValue(args->at(0));
  __ CallRuntime(Runtime::kMath_log, 1);
  context()->Plug(r0);
}


void FullCodeGenerator::EmitMathSqrt(CallRuntime* expr) {
  // Load the argument on the stack and call the runtime function.
  ZoneList<Expression*>* args = expr->arguments();
  ASSERT(args->length() == 1);
  VisitForStackValue(args->at(0));
  __ CallRuntime(Runtime::kMath_sqrt, 1);
  context()->Plug(r0);
}


void FullCodeGenerator::EmitCallFunction(CallRuntime* expr) {
  ZoneList<Expression*>* args = expr->arguments();
  ASSERT(args->length() >= 2);

  int arg_count = args->length() - 2;  // 2 ~ receiver and function.
  for (int i = 0; i < arg_count + 1; i++) {
    VisitForStackValue(args->at(i));
  }
  VisitForAccumulatorValue(args->last());  // Function.

  Label runtime, done;
  // Check for non-function argument (including proxy).
  __ JumpIfSmi(r0, &runtime);
  __ CompareObjectType(r0, r1, r1, JS_FUNCTION_TYPE);
  __ b(ne, &runtime);

  // InvokeFunction requires the function in r1. Move it in there.
  __ mov(r1, result_register());
  ParameterCount count(arg_count);
  __ InvokeFunction(r1, count, CALL_FUNCTION, NullCallWrapper());
  __ ldr(cp, MemOperand(fp, StandardFrameConstants::kContextOffset));
  __ jmp(&done);

  __ bind(&runtime);
  __ push(r0);
  __ CallRuntime(Runtime::kCall, args->length());
  __ bind(&done);

  context()->Plug(r0);
}


void FullCodeGenerator::EmitRegExpConstructResult(CallRuntime* expr) {
  RegExpConstructResultStub stub;
  ZoneList<Expression*>* args = expr->arguments();
  ASSERT(args->length() == 3);
  VisitForStackValue(args->at(0));
  VisitForStackValue(args->at(1));
  VisitForAccumulatorValue(args->at(2));
  __ pop(r1);
  __ pop(r2);
  __ CallStub(&stub);
  context()->Plug(r0);
}


void FullCodeGenerator::EmitGetFromCache(CallRuntime* expr) {
  ZoneList<Expression*>* args = expr->arguments();
  ASSERT_EQ(2, args->length());
  ASSERT_NE(NULL, args->at(0)->AsLiteral());
  int cache_id = Smi::cast(*(args->at(0)->AsLiteral()->value()))->value();

  Handle<FixedArray> jsfunction_result_caches(
      isolate()->native_context()->jsfunction_result_caches());
  if (jsfunction_result_caches->length() <= cache_id) {
    __ Abort(kAttemptToUseUndefinedCache);
    __ LoadRoot(r0, Heap::kUndefinedValueRootIndex);
    context()->Plug(r0);
    return;
  }

  VisitForAccumulatorValue(args->at(1));

  Register key = r0;
  Register cache = r1;
  __ ldr(cache, ContextOperand(cp, Context::GLOBAL_OBJECT_INDEX));
  __ ldr(cache, FieldMemOperand(cache, GlobalObject::kNativeContextOffset));
  __ ldr(cache, ContextOperand(cache, Context::JSFUNCTION_RESULT_CACHES_INDEX));
  __ ldr(cache,
         FieldMemOperand(cache, FixedArray::OffsetOfElementAt(cache_id)));


  Label done, not_found;
  __ ldr(r2, FieldMemOperand(cache, JSFunctionResultCache::kFingerOffset));
  // r2 now holds finger offset as a smi.
  __ add(r3, cache, Operand(FixedArray::kHeaderSize - kHeapObjectTag));
  // r3 now points to the start of fixed array elements.
  __ ldr(r2, MemOperand::PointerAddressFromSmiKey(r3, r2, PreIndex));
  // Note side effect of PreIndex: r3 now points to the key of the pair.
  __ cmp(key, r2);
  __ b(ne, &not_found);

  __ ldr(r0, MemOperand(r3, kPointerSize));
  __ b(&done);

  __ bind(&not_found);
  // Call runtime to perform the lookup.
  __ Push(cache, key);
  __ CallRuntime(Runtime::kHiddenGetFromCache, 2);

  __ bind(&done);
  context()->Plug(r0);
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

  __ ldr(r0, FieldMemOperand(r0, String::kHashFieldOffset));
  __ tst(r0, Operand(String::kContainsCachedArrayIndexMask));
  PrepareForBailoutBeforeSplit(expr, true, if_true, if_false);
  Split(eq, if_true, if_false, fall_through);

  context()->Plug(if_true, if_false);
}


void FullCodeGenerator::EmitGetCachedArrayIndex(CallRuntime* expr) {
  ZoneList<Expression*>* args = expr->arguments();
  ASSERT(args->length() == 1);
  VisitForAccumulatorValue(args->at(0));

  __ AssertString(r0);

  __ ldr(r0, FieldMemOperand(r0, String::kHashFieldOffset));
  __ IndexFromHash(r0, r0);

  context()->Plug(r0);
}


void FullCodeGenerator::EmitFastAsciiArrayJoin(CallRuntime* expr) {
  Label bailout, done, one_char_separator, long_separator, non_trivial_array,
      not_size_one_array, loop, empty_separator_loop, one_char_separator_loop,
      one_char_separator_loop_entry, long_separator_loop;
  ZoneList<Expression*>* args = expr->arguments();
  ASSERT(args->length() == 2);
  VisitForStackValue(args->at(1));
  VisitForAccumulatorValue(args->at(0));

  // All aliases of the same register have disjoint lifetimes.
  Register array = r0;
  Register elements = no_reg;  // Will be r0.
  Register result = no_reg;  // Will be r0.
  Register separator = r1;
  Register array_length = r2;
  Register result_pos = no_reg;  // Will be r2
  Register string_length = r3;
  Register string = r4;
  Register element = r5;
  Register elements_end = r6;
  Register scratch = r9;

  // Separator operand is on the stack.
  __ pop(separator);

  // Check that the array is a JSArray.
  __ JumpIfSmi(array, &bailout);
  __ CompareObjectType(array, scratch, array_length, JS_ARRAY_TYPE);
  __ b(ne, &bailout);

  // Check that the array has fast elements.
  __ CheckFastElements(scratch, array_length, &bailout);

  // If the array has length zero, return the empty string.
  __ ldr(array_length, FieldMemOperand(array, JSArray::kLengthOffset));
  __ SmiUntag(array_length, SetCC);
  __ b(ne, &non_trivial_array);
  __ LoadRoot(r0, Heap::kempty_stringRootIndex);
  __ b(&done);

  __ bind(&non_trivial_array);

  // Get the FixedArray containing array's elements.
  elements = array;
  __ ldr(elements, FieldMemOperand(array, JSArray::kElementsOffset));
  array = no_reg;  // End of array's live range.

  // Check that all array elements are sequential ASCII strings, and
  // accumulate the sum of their lengths, as a smi-encoded value.
  __ mov(string_length, Operand::Zero());
  __ add(element,
         elements, Operand(FixedArray::kHeaderSize - kHeapObjectTag));
  __ add(elements_end, element, Operand(array_length, LSL, kPointerSizeLog2));
  // Loop condition: while (element < elements_end).
  // Live values in registers:
  //   elements: Fixed array of strings.
  //   array_length: Length of the fixed array of strings (not smi)
  //   separator: Separator string
  //   string_length: Accumulated sum of string lengths (smi).
  //   element: Current array element.
  //   elements_end: Array end.
  if (generate_debug_code_) {
    __ cmp(array_length, Operand::Zero());
    __ Assert(gt, kNoEmptyArraysHereInEmitFastAsciiArrayJoin);
  }
  __ bind(&loop);
  __ ldr(string, MemOperand(element, kPointerSize, PostIndex));
  __ JumpIfSmi(string, &bailout);
  __ ldr(scratch, FieldMemOperand(string, HeapObject::kMapOffset));
  __ ldrb(scratch, FieldMemOperand(scratch, Map::kInstanceTypeOffset));
  __ JumpIfInstanceTypeIsNotSequentialAscii(scratch, scratch, &bailout);
  __ ldr(scratch, FieldMemOperand(string, SeqOneByteString::kLengthOffset));
  __ add(string_length, string_length, Operand(scratch), SetCC);
  __ b(vs, &bailout);
  __ cmp(element, elements_end);
  __ b(lt, &loop);

  // If array_length is 1, return elements[0], a string.
  __ cmp(array_length, Operand(1));
  __ b(ne, &not_size_one_array);
  __ ldr(r0, FieldMemOperand(elements, FixedArray::kHeaderSize));
  __ b(&done);

  __ bind(&not_size_one_array);

  // Live values in registers:
  //   separator: Separator string
  //   array_length: Length of the array.
  //   string_length: Sum of string lengths (smi).
  //   elements: FixedArray of strings.

  // Check that the separator is a flat ASCII string.
  __ JumpIfSmi(separator, &bailout);
  __ ldr(scratch, FieldMemOperand(separator, HeapObject::kMapOffset));
  __ ldrb(scratch, FieldMemOperand(scratch, Map::kInstanceTypeOffset));
  __ JumpIfInstanceTypeIsNotSequentialAscii(scratch, scratch, &bailout);

  // Add (separator length times array_length) - separator length to the
  // string_length to get the length of the result string. array_length is not
  // smi but the other values are, so the result is a smi
  __ ldr(scratch, FieldMemOperand(separator, SeqOneByteString::kLengthOffset));
  __ sub(string_length, string_length, Operand(scratch));
  __ smull(scratch, ip, array_length, scratch);
  // Check for smi overflow. No overflow if higher 33 bits of 64-bit result are
  // zero.
  __ cmp(ip, Operand::Zero());
  __ b(ne, &bailout);
  __ tst(scratch, Operand(0x80000000));
  __ b(ne, &bailout);
  __ add(string_length, string_length, Operand(scratch), SetCC);
  __ b(vs, &bailout);
  __ SmiUntag(string_length);

  // Get first element in the array to free up the elements register to be used
  // for the result.
  __ add(element,
         elements, Operand(FixedArray::kHeaderSize - kHeapObjectTag));
  result = elements;  // End of live range for elements.
  elements = no_reg;
  // Live values in registers:
  //   element: First array element
  //   separator: Separator string
  //   string_length: Length of result string (not smi)
  //   array_length: Length of the array.
  __ AllocateAsciiString(result,
                         string_length,
                         scratch,
                         string,  // used as scratch
                         elements_end,  // used as scratch
                         &bailout);
  // Prepare for looping. Set up elements_end to end of the array. Set
  // result_pos to the position of the result where to write the first
  // character.
  __ add(elements_end, element, Operand(array_length, LSL, kPointerSizeLog2));
  result_pos = array_length;  // End of live range for array_length.
  array_length = no_reg;
  __ add(result_pos,
         result,
         Operand(SeqOneByteString::kHeaderSize - kHeapObjectTag));

  // Check the length of the separator.
  __ ldr(scratch, FieldMemOperand(separator, SeqOneByteString::kLengthOffset));
  __ cmp(scratch, Operand(Smi::FromInt(1)));
  __ b(eq, &one_char_separator);
  __ b(gt, &long_separator);

  // Empty separator case
  __ bind(&empty_separator_loop);
  // Live values in registers:
  //   result_pos: the position to which we are currently copying characters.
  //   element: Current array element.
  //   elements_end: Array end.

  // Copy next array element to the result.
  __ ldr(string, MemOperand(element, kPointerSize, PostIndex));
  __ ldr(string_length, FieldMemOperand(string, String::kLengthOffset));
  __ SmiUntag(string_length);
  __ add(string,
         string,
         Operand(SeqOneByteString::kHeaderSize - kHeapObjectTag));
  __ CopyBytes(string, result_pos, string_length, scratch);
  __ cmp(element, elements_end);
  __ b(lt, &empty_separator_loop);  // End while (element < elements_end).
  ASSERT(result.is(r0));
  __ b(&done);

  // One-character separator case
  __ bind(&one_char_separator);
  // Replace separator with its ASCII character value.
  __ ldrb(separator, FieldMemOperand(separator, SeqOneByteString::kHeaderSize));
  // Jump into the loop after the code that copies the separator, so the first
  // element is not preceded by a separator
  __ jmp(&one_char_separator_loop_entry);

  __ bind(&one_char_separator_loop);
  // Live values in registers:
  //   result_pos: the position to which we are currently copying characters.
  //   element: Current array element.
  //   elements_end: Array end.
  //   separator: Single separator ASCII char (in lower byte).

  // Copy the separator character to the result.
  __ strb(separator, MemOperand(result_pos, 1, PostIndex));

  // Copy next array element to the result.
  __ bind(&one_char_separator_loop_entry);
  __ ldr(string, MemOperand(element, kPointerSize, PostIndex));
  __ ldr(string_length, FieldMemOperand(string, String::kLengthOffset));
  __ SmiUntag(string_length);
  __ add(string,
         string,
         Operand(SeqOneByteString::kHeaderSize - kHeapObjectTag));
  __ CopyBytes(string, result_pos, string_length, scratch);
  __ cmp(element, elements_end);
  __ b(lt, &one_char_separator_loop);  // End while (element < elements_end).
  ASSERT(result.is(r0));
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
  __ ldr(string_length, FieldMemOperand(separator, String::kLengthOffset));
  __ SmiUntag(string_length);
  __ add(string,
         separator,
         Operand(SeqOneByteString::kHeaderSize - kHeapObjectTag));
  __ CopyBytes(string, result_pos, string_length, scratch);

  __ bind(&long_separator);
  __ ldr(string, MemOperand(element, kPointerSize, PostIndex));
  __ ldr(string_length, FieldMemOperand(string, String::kLengthOffset));
  __ SmiUntag(string_length);
  __ add(string,
         string,
         Operand(SeqOneByteString::kHeaderSize - kHeapObjectTag));
  __ CopyBytes(string, result_pos, string_length, scratch);
  __ cmp(element, elements_end);
  __ b(lt, &long_separator_loop);  // End while (element < elements_end).
  ASSERT(result.is(r0));
  __ b(&done);

  __ bind(&bailout);
  __ LoadRoot(r0, Heap::kUndefinedValueRootIndex);
  __ bind(&done);
  context()->Plug(r0);
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
    // Push the builtins object as the receiver.
    __ ldr(r0, GlobalObjectOperand());
    __ ldr(r0, FieldMemOperand(r0, GlobalObject::kBuiltinsOffset));
    __ push(r0);

    // Load the function from the receiver.
    __ mov(r2, Operand(expr->name()));
    CallLoadIC(NOT_CONTEXTUAL, expr->CallRuntimeFeedbackId());

    // Push the target function under the receiver.
    __ ldr(ip, MemOperand(sp, 0));
    __ push(ip);
    __ str(r0, MemOperand(sp, kPointerSize));

    // Push the arguments ("left-to-right").
    int arg_count = args->length();
    for (int i = 0; i < arg_count; i++) {
      VisitForStackValue(args->at(i));
    }

    // Record source position of the IC call.
    SetSourcePosition(expr->position());
    CallFunctionStub stub(arg_count, NO_CALL_FUNCTION_FLAGS);
    __ ldr(r1, MemOperand(sp, (arg_count + 1) * kPointerSize));
    __ CallStub(&stub);

    // Restore context register.
    __ ldr(cp, MemOperand(fp, StandardFrameConstants::kContextOffset));

    context()->DropAndPlug(1, r0);
  } else {
    // Push the arguments ("left-to-right").
    for (int i = 0; i < arg_count; i++) {
      VisitForStackValue(args->at(i));
    }

    // Call the C runtime function.
    __ CallRuntime(expr->function(), arg_count);
    context()->Plug(r0);
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
        __ mov(r1, Operand(Smi::FromInt(strict_mode())));
        __ push(r1);
        __ InvokeBuiltin(Builtins::DELETE, CALL_FUNCTION);
        context()->Plug(r0);
      } else if (proxy != NULL) {
        Variable* var = proxy->var();
        // Delete of an unqualified identifier is disallowed in strict mode
        // but "delete this" is allowed.
        ASSERT(strict_mode() == SLOPPY || var->is_this());
        if (var->IsUnallocated()) {
          __ ldr(r2, GlobalObjectOperand());
          __ mov(r1, Operand(var->name()));
          __ mov(r0, Operand(Smi::FromInt(SLOPPY)));
          __ Push(r2, r1, r0);
          __ InvokeBuiltin(Builtins::DELETE, CALL_FUNCTION);
          context()->Plug(r0);
        } else if (var->IsStackAllocated() || var->IsContextSlot()) {
          // Result of deleting non-global, non-dynamic variables is false.
          // The subexpression does not have side effects.
          context()->Plug(var->is_this());
        } else {
          // Non-global variable.  Call the runtime to try to delete from the
          // context where the variable was introduced.
          ASSERT(!context_register().is(r2));
          __ mov(r2, Operand(var->name()));
          __ Push(context_register(), r2);
          __ CallRuntime(Runtime::kHiddenDeleteContextSlot, 2);
          context()->Plug(r0);
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
        __ LoadRoot(r0, Heap::kTrueValueRootIndex);
        if (context()->IsStackValue()) __ push(r0);
        __ jmp(&done);
        __ bind(&materialize_false);
        PrepareForBailoutForId(expr->MaterializeFalseId(), NO_REGISTERS);
        __ LoadRoot(r0, Heap::kFalseValueRootIndex);
        if (context()->IsStackValue()) __ push(r0);
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
      context()->Plug(r0);
      break;
    }

    default:
      UNREACHABLE();
  }
}


void FullCodeGenerator::VisitCountOperation(CountOperation* expr) {
  ASSERT(expr->expression()->IsValidLeftHandSide());

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
    ASSERT(expr->expression()->AsVariableProxy()->var() != NULL);
    AccumulatorValueContext context(this);
    EmitVariableLoad(expr->expression()->AsVariableProxy());
  } else {
    // Reserve space for result of postfix operation.
    if (expr->is_postfix() && !context()->IsEffect()) {
      __ mov(ip, Operand(Smi::FromInt(0)));
      __ push(ip);
    }
    if (assign_type == NAMED_PROPERTY) {
      // Put the object both on the stack and in the accumulator.
      VisitForAccumulatorValue(prop->obj());
      __ push(r0);
      EmitNamedPropertyLoad(prop);
    } else {
      VisitForStackValue(prop->obj());
      VisitForAccumulatorValue(prop->key());
      __ ldr(r1, MemOperand(sp, 0));
      __ push(r0);
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
  Label stub_call, done;
  JumpPatchSite patch_site(masm_);

  int count_value = expr->op() == Token::INC ? 1 : -1;
  if (ShouldInlineSmiCase(expr->op())) {
    Label slow;
    patch_site.EmitJumpIfNotSmi(r0, &slow);

    // Save result for postfix expressions.
    if (expr->is_postfix()) {
      if (!context()->IsEffect()) {
        // Save the result on the stack. If we have a named or keyed property
        // we store the result under the receiver that is currently on top
        // of the stack.
        switch (assign_type) {
          case VARIABLE:
            __ push(r0);
            break;
          case NAMED_PROPERTY:
            __ str(r0, MemOperand(sp, kPointerSize));
            break;
          case KEYED_PROPERTY:
            __ str(r0, MemOperand(sp, 2 * kPointerSize));
            break;
        }
      }
    }

    __ add(r0, r0, Operand(Smi::FromInt(count_value)), SetCC);
    __ b(vc, &done);
    // Call stub. Undo operation first.
    __ sub(r0, r0, Operand(Smi::FromInt(count_value)));
    __ jmp(&stub_call);
    __ bind(&slow);
  }
  ToNumberStub convert_stub;
  __ CallStub(&convert_stub);

  // Save result for postfix expressions.
  if (expr->is_postfix()) {
    if (!context()->IsEffect()) {
      // Save the result on the stack. If we have a named or keyed property
      // we store the result under the receiver that is currently on top
      // of the stack.
      switch (assign_type) {
        case VARIABLE:
          __ push(r0);
          break;
        case NAMED_PROPERTY:
          __ str(r0, MemOperand(sp, kPointerSize));
          break;
        case KEYED_PROPERTY:
          __ str(r0, MemOperand(sp, 2 * kPointerSize));
          break;
      }
    }
  }


  __ bind(&stub_call);
  __ mov(r1, r0);
  __ mov(r0, Operand(Smi::FromInt(count_value)));

  // Record position before stub call.
  SetSourcePosition(expr->position());

  BinaryOpICStub stub(Token::ADD, NO_OVERWRITE);
  CallIC(stub.GetCode(isolate()), expr->CountBinOpFeedbackId());
  patch_site.EmitPatchInfo();
  __ bind(&done);

  // Store the value returned in r0.
  switch (assign_type) {
    case VARIABLE:
      if (expr->is_postfix()) {
        { EffectContext context(this);
          EmitVariableAssignment(expr->expression()->AsVariableProxy()->var(),
                                 Token::ASSIGN);
          PrepareForBailoutForId(expr->AssignmentId(), TOS_REG);
          context.Plug(r0);
        }
        // For all contexts except EffectConstant We have the result on
        // top of the stack.
        if (!context()->IsEffect()) {
          context()->PlugTOS();
        }
      } else {
        EmitVariableAssignment(expr->expression()->AsVariableProxy()->var(),
                               Token::ASSIGN);
        PrepareForBailoutForId(expr->AssignmentId(), TOS_REG);
        context()->Plug(r0);
      }
      break;
    case NAMED_PROPERTY: {
      __ mov(r2, Operand(prop->key()->AsLiteral()->value()));
      __ pop(r1);
      CallStoreIC(expr->CountStoreFeedbackId());
      PrepareForBailoutForId(expr->AssignmentId(), TOS_REG);
      if (expr->is_postfix()) {
        if (!context()->IsEffect()) {
          context()->PlugTOS();
        }
      } else {
        context()->Plug(r0);
      }
      break;
    }
    case KEYED_PROPERTY: {
      __ Pop(r2, r1);  // r1 = key. r2 = receiver.
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
        context()->Plug(r0);
      }
      break;
    }
  }
}


void FullCodeGenerator::VisitForTypeofValue(Expression* expr) {
  ASSERT(!context()->IsEffect());
  ASSERT(!context()->IsTest());
  VariableProxy* proxy = expr->AsVariableProxy();
  if (proxy != NULL && proxy->var()->IsUnallocated()) {
    Comment cmnt(masm_, "[ Global variable");
    __ ldr(r0, GlobalObjectOperand());
    __ mov(r2, Operand(proxy->name()));
    // Use a regular load, not a contextual load, to avoid a reference
    // error.
    CallLoadIC(NOT_CONTEXTUAL);
    PrepareForBailout(expr, TOS_REG);
    context()->Plug(r0);
  } else if (proxy != NULL && proxy->var()->IsLookupSlot()) {
    Comment cmnt(masm_, "[ Lookup slot");
    Label done, slow;

    // Generate code for loading from variables potentially shadowed
    // by eval-introduced variables.
    EmitDynamicLookupFastCase(proxy->var(), INSIDE_TYPEOF, &slow, &done);

    __ bind(&slow);
    __ mov(r0, Operand(proxy->name()));
    __ Push(cp, r0);
    __ CallRuntime(Runtime::kHiddenLoadContextSlotNoReferenceError, 2);
    PrepareForBailout(expr, TOS_REG);
    __ bind(&done);

    context()->Plug(r0);
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

  if (check->Equals(isolate()->heap()->number_string())) {
    __ JumpIfSmi(r0, if_true);
    __ ldr(r0, FieldMemOperand(r0, HeapObject::kMapOffset));
    __ LoadRoot(ip, Heap::kHeapNumberMapRootIndex);
    __ cmp(r0, ip);
    Split(eq, if_true, if_false, fall_through);
  } else if (check->Equals(isolate()->heap()->string_string())) {
    __ JumpIfSmi(r0, if_false);
    // Check for undetectable objects => false.
    __ CompareObjectType(r0, r0, r1, FIRST_NONSTRING_TYPE);
    __ b(ge, if_false);
    __ ldrb(r1, FieldMemOperand(r0, Map::kBitFieldOffset));
    __ tst(r1, Operand(1 << Map::kIsUndetectable));
    Split(eq, if_true, if_false, fall_through);
  } else if (check->Equals(isolate()->heap()->symbol_string())) {
    __ JumpIfSmi(r0, if_false);
    __ CompareObjectType(r0, r0, r1, SYMBOL_TYPE);
    Split(eq, if_true, if_false, fall_through);
  } else if (check->Equals(isolate()->heap()->boolean_string())) {
    __ CompareRoot(r0, Heap::kTrueValueRootIndex);
    __ b(eq, if_true);
    __ CompareRoot(r0, Heap::kFalseValueRootIndex);
    Split(eq, if_true, if_false, fall_through);
  } else if (FLAG_harmony_typeof &&
             check->Equals(isolate()->heap()->null_string())) {
    __ CompareRoot(r0, Heap::kNullValueRootIndex);
    Split(eq, if_true, if_false, fall_through);
  } else if (check->Equals(isolate()->heap()->undefined_string())) {
    __ CompareRoot(r0, Heap::kUndefinedValueRootIndex);
    __ b(eq, if_true);
    __ JumpIfSmi(r0, if_false);
    // Check for undetectable objects => true.
    __ ldr(r0, FieldMemOperand(r0, HeapObject::kMapOffset));
    __ ldrb(r1, FieldMemOperand(r0, Map::kBitFieldOffset));
    __ tst(r1, Operand(1 << Map::kIsUndetectable));
    Split(ne, if_true, if_false, fall_through);

  } else if (check->Equals(isolate()->heap()->function_string())) {
    __ JumpIfSmi(r0, if_false);
    STATIC_ASSERT(NUM_OF_CALLABLE_SPEC_OBJECT_TYPES == 2);
    __ CompareObjectType(r0, r0, r1, JS_FUNCTION_TYPE);
    __ b(eq, if_true);
    __ cmp(r1, Operand(JS_FUNCTION_PROXY_TYPE));
    Split(eq, if_true, if_false, fall_through);
  } else if (check->Equals(isolate()->heap()->object_string())) {
    __ JumpIfSmi(r0, if_false);
    if (!FLAG_harmony_typeof) {
      __ CompareRoot(r0, Heap::kNullValueRootIndex);
      __ b(eq, if_true);
    }
    // Check for JS objects => true.
    __ CompareObjectType(r0, r0, r1, FIRST_NONCALLABLE_SPEC_OBJECT_TYPE);
    __ b(lt, if_false);
    __ CompareInstanceType(r0, r1, LAST_NONCALLABLE_SPEC_OBJECT_TYPE);
    __ b(gt, if_false);
    // Check for undetectable objects => false.
    __ ldrb(r1, FieldMemOperand(r0, Map::kBitFieldOffset));
    __ tst(r1, Operand(1 << Map::kIsUndetectable));
    Split(eq, if_true, if_false, fall_through);
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
      __ LoadRoot(ip, Heap::kTrueValueRootIndex);
      __ cmp(r0, ip);
      Split(eq, if_true, if_false, fall_through);
      break;

    case Token::INSTANCEOF: {
      VisitForStackValue(expr->right());
      InstanceofStub stub(InstanceofStub::kNoFlags);
      __ CallStub(&stub);
      PrepareForBailoutBeforeSplit(expr, true, if_true, if_false);
      // The stub returns 0 for true.
      __ tst(r0, r0);
      Split(eq, if_true, if_false, fall_through);
      break;
    }

    default: {
      VisitForAccumulatorValue(expr->right());
      Condition cond = CompareIC::ComputeCondition(op);
      __ pop(r1);

      bool inline_smi_code = ShouldInlineSmiCase(op);
      JumpPatchSite patch_site(masm_);
      if (inline_smi_code) {
        Label slow_case;
        __ orr(r2, r0, Operand(r1));
        patch_site.EmitJumpIfNotSmi(r2, &slow_case);
        __ cmp(r1, r0);
        Split(cond, if_true, if_false, NULL);
        __ bind(&slow_case);
      }

      // Record position and call the compare IC.
      SetSourcePosition(expr->position());
      Handle<Code> ic = CompareIC::GetUninitialized(isolate(), op);
      CallIC(ic, expr->CompareOperationFeedbackId());
      patch_site.EmitPatchInfo();
      PrepareForBailoutBeforeSplit(expr, true, if_true, if_false);
      __ cmp(r0, Operand::Zero());
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
  context()->PrepareTest(&materialize_true, &materialize_false,
                         &if_true, &if_false, &fall_through);

  VisitForAccumulatorValue(sub_expr);
  PrepareForBailoutBeforeSplit(expr, true, if_true, if_false);
  if (expr->op() == Token::EQ_STRICT) {
    Heap::RootListIndex nil_value = nil == kNullValue ?
        Heap::kNullValueRootIndex :
        Heap::kUndefinedValueRootIndex;
    __ LoadRoot(r1, nil_value);
    __ cmp(r0, r1);
    Split(eq, if_true, if_false, fall_through);
  } else {
    Handle<Code> ic = CompareNilICStub::GetUninitialized(isolate(), nil);
    CallIC(ic, expr->CompareOperationFeedbackId());
    __ cmp(r0, Operand(0));
    Split(ne, if_true, if_false, fall_through);
  }
  context()->Plug(if_true, if_false);
}


void FullCodeGenerator::VisitThisFunction(ThisFunction* expr) {
  __ ldr(r0, MemOperand(fp, JavaScriptFrameConstants::kFunctionOffset));
  context()->Plug(r0);
}


Register FullCodeGenerator::result_register() {
  return r0;
}


Register FullCodeGenerator::context_register() {
  return cp;
}


void FullCodeGenerator::StoreToFrameField(int frame_offset, Register value) {
  ASSERT_EQ(POINTER_SIZE_ALIGN(frame_offset), frame_offset);
  __ str(value, MemOperand(fp, frame_offset));
}


void FullCodeGenerator::LoadContextField(Register dst, int context_index) {
  __ ldr(dst, ContextOperand(cp, context_index));
}


void FullCodeGenerator::PushFunctionArgumentForContextAllocation() {
  Scope* declaration_scope = scope()->DeclarationScope();
  if (declaration_scope->is_global_scope() ||
      declaration_scope->is_module_scope()) {
    // Contexts nested in the native context have a canonical empty function
    // as their closure, not the anonymous closure containing the global
    // code.  Pass a smi sentinel and let the runtime look up the empty
    // function.
    __ mov(ip, Operand(Smi::FromInt(0)));
  } else if (declaration_scope->is_eval_scope()) {
    // Contexts created by a call to eval have the same closure as the
    // context calling eval, not the anonymous closure containing the eval
    // code.  Fetch it from the context.
    __ ldr(ip, ContextOperand(cp, Context::CLOSURE_INDEX));
  } else {
    ASSERT(declaration_scope->is_function_scope());
    __ ldr(ip, MemOperand(fp, JavaScriptFrameConstants::kFunctionOffset));
  }
  __ push(ip);
}


// ----------------------------------------------------------------------------
// Non-local control flow support.

void FullCodeGenerator::EnterFinallyBlock() {
  ASSERT(!result_register().is(r1));
  // Store result register while executing finally block.
  __ push(result_register());
  // Cook return address in link register to stack (smi encoded Code* delta)
  __ sub(r1, lr, Operand(masm_->CodeObject()));
  __ SmiTag(r1);

  // Store result register while executing finally block.
  __ push(r1);

  // Store pending message while executing finally block.
  ExternalReference pending_message_obj =
      ExternalReference::address_of_pending_message_obj(isolate());
  __ mov(ip, Operand(pending_message_obj));
  __ ldr(r1, MemOperand(ip));
  __ push(r1);

  ExternalReference has_pending_message =
      ExternalReference::address_of_has_pending_message(isolate());
  __ mov(ip, Operand(has_pending_message));
  __ ldr(r1, MemOperand(ip));
  __ SmiTag(r1);
  __ push(r1);

  ExternalReference pending_message_script =
      ExternalReference::address_of_pending_message_script(isolate());
  __ mov(ip, Operand(pending_message_script));
  __ ldr(r1, MemOperand(ip));
  __ push(r1);
}


void FullCodeGenerator::ExitFinallyBlock() {
  ASSERT(!result_register().is(r1));
  // Restore pending message from stack.
  __ pop(r1);
  ExternalReference pending_message_script =
      ExternalReference::address_of_pending_message_script(isolate());
  __ mov(ip, Operand(pending_message_script));
  __ str(r1, MemOperand(ip));

  __ pop(r1);
  __ SmiUntag(r1);
  ExternalReference has_pending_message =
      ExternalReference::address_of_has_pending_message(isolate());
  __ mov(ip, Operand(has_pending_message));
  __ str(r1, MemOperand(ip));

  __ pop(r1);
  ExternalReference pending_message_obj =
      ExternalReference::address_of_pending_message_obj(isolate());
  __ mov(ip, Operand(pending_message_obj));
  __ str(r1, MemOperand(ip));

  // Restore result register from stack.
  __ pop(r1);

  // Uncook return address and return.
  __ pop(result_register());
  __ SmiUntag(r1);
  __ add(pc, r1, Operand(masm_->CodeObject()));
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
    __ ldr(cp, MemOperand(sp, StackHandlerConstants::kContextOffset));
    __ str(cp, MemOperand(fp, StandardFrameConstants::kContextOffset));
  }
  __ PopTryHandler();
  __ bl(finally_entry_);

  *stack_depth = 0;
  *context_length = 0;
  return previous_;
}


#undef __


static Address GetInterruptImmediateLoadAddress(Address pc) {
  Address load_address = pc - 2 * Assembler::kInstrSize;
  if (!FLAG_enable_ool_constant_pool) {
    ASSERT(Assembler::IsLdrPcImmediateOffset(Memory::int32_at(load_address)));
  } else if (Assembler::IsMovT(Memory::int32_at(load_address))) {
    load_address -= Assembler::kInstrSize;
    ASSERT(Assembler::IsMovW(Memory::int32_at(load_address)));
  } else {
    ASSERT(Assembler::IsLdrPpImmediateOffset(Memory::int32_at(load_address)));
  }
  return load_address;
}


void BackEdgeTable::PatchAt(Code* unoptimized_code,
                            Address pc,
                            BackEdgeState target_state,
                            Code* replacement_code) {
  static const int kInstrSize = Assembler::kInstrSize;
  Address pc_immediate_load_address = GetInterruptImmediateLoadAddress(pc);
  Address branch_address = pc_immediate_load_address - kInstrSize;
  CodePatcher patcher(branch_address, 1);
  switch (target_state) {
    case INTERRUPT:
    {
      //  <decrement profiling counter>
      //   bpl ok
      //   ; load interrupt stub address into ip - either of:
      //   ldr ip, [pc/pp, <constant pool offset>]  |   movw ip, <immed low>
      //                                            |   movt ip, <immed high>
      //   blx ip
      //  ok-label

      // Calculate branch offet to the ok-label - this is the difference between
      // the branch address and |pc| (which points at <blx ip>) plus one instr.
      int branch_offset = pc + kInstrSize - branch_address;
      patcher.masm()->b(branch_offset, pl);
      break;
    }
    case ON_STACK_REPLACEMENT:
    case OSR_AFTER_STACK_CHECK:
      //  <decrement profiling counter>
      //   mov r0, r0 (NOP)
      //   ; load on-stack replacement address into ip - either of:
      //   ldr ip, [pc/pp, <constant pool offset>]  |   movw ip, <immed low>
      //                                            |   movt ip, <immed high>
      //   blx ip
      //  ok-label
      patcher.masm()->nop();
      break;
  }

  // Replace the call address.
  Assembler::set_target_address_at(pc_immediate_load_address, unoptimized_code,
      replacement_code->entry());

  unoptimized_code->GetHeap()->incremental_marking()->RecordCodeTargetPatch(
      unoptimized_code, pc_immediate_load_address, replacement_code);
}


BackEdgeTable::BackEdgeState BackEdgeTable::GetBackEdgeState(
    Isolate* isolate,
    Code* unoptimized_code,
    Address pc) {
  static const int kInstrSize = Assembler::kInstrSize;
  ASSERT(Memory::int32_at(pc - kInstrSize) == kBlxIp);

  Address pc_immediate_load_address = GetInterruptImmediateLoadAddress(pc);
  Address branch_address = pc_immediate_load_address - kInstrSize;
  Address interrupt_address = Assembler::target_address_at(
      pc_immediate_load_address, unoptimized_code);

  if (Assembler::IsBranch(Assembler::instr_at(branch_address))) {
    ASSERT(interrupt_address ==
           isolate->builtins()->InterruptCheck()->entry());
    return INTERRUPT;
  }

  ASSERT(Assembler::IsNop(Assembler::instr_at(branch_address)));

  if (interrupt_address ==
      isolate->builtins()->OnStackReplacement()->entry()) {
    return ON_STACK_REPLACEMENT;
  }

  ASSERT(interrupt_address ==
         isolate->builtins()->OsrAfterStackCheck()->entry());
  return OSR_AFTER_STACK_CHECK;
}


} }  // namespace v8::internal

#endif  // V8_TARGET_ARCH_ARM
