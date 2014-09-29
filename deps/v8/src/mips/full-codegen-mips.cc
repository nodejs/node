// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/v8.h"

#if V8_TARGET_ARCH_MIPS

// Note on Mips implementation:
//
// The result_register() for mips is the 'v0' register, which is defined
// by the ABI to contain function return values. However, the first
// parameter to a function is defined to be 'a0'. So there are many
// places where we have to move a previous result in v0 to a0 for the
// next call: mov(a0, v0). This is not needed on the other architectures.

#include "src/code-stubs.h"
#include "src/codegen.h"
#include "src/compiler.h"
#include "src/debug.h"
#include "src/full-codegen.h"
#include "src/isolate-inl.h"
#include "src/parser.h"
#include "src/scopes.h"
#include "src/stub-cache.h"

#include "src/mips/code-stubs-mips.h"
#include "src/mips/macro-assembler-mips.h"

namespace v8 {
namespace internal {

#define __ ACCESS_MASM(masm_)


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
//   o cp: our context
//   o fp: our caller's frame pointer
//   o sp: stack pointer
//   o ra: return address
//
// The function builds a JS frame.  Please see JavaScriptFrameConstants in
// frames-mips.h for its layout.
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
    __ stop("stop-at");
  }
#endif

  // Sloppy mode functions and builtins need to replace the receiver with the
  // global proxy when called as functions (without an explicit receiver
  // object).
  if (info->strict_mode() == SLOPPY && !info->is_native()) {
    Label ok;
    int receiver_offset = info->scope()->num_parameters() * kPointerSize;
    __ lw(at, MemOperand(sp, receiver_offset));
    __ LoadRoot(a2, Heap::kUndefinedValueRootIndex);
    __ Branch(&ok, ne, a2, Operand(at));

    __ lw(a2, GlobalObjectOperand());
    __ lw(a2, FieldMemOperand(a2, GlobalObject::kGlobalProxyOffset));

    __ sw(a2, MemOperand(sp, receiver_offset));

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
    if (locals_count > 0) {
      if (locals_count >= 128) {
        Label ok;
        __ Subu(t5, sp, Operand(locals_count * kPointerSize));
        __ LoadRoot(a2, Heap::kRealStackLimitRootIndex);
        __ Branch(&ok, hs, t5, Operand(a2));
        __ InvokeBuiltin(Builtins::STACK_OVERFLOW, CALL_FUNCTION);
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

  bool function_in_register = true;

  // Possibly allocate a local context.
  int heap_slots = info->scope()->num_heap_slots() - Context::MIN_CONTEXT_SLOTS;
  if (heap_slots > 0) {
    Comment cmnt(masm_, "[ Allocate context");
    // Argument to NewContext is the function, which is still in a1.
    bool need_write_barrier = true;
    if (FLAG_harmony_scoping && info->scope()->is_global_scope()) {
      __ push(a1);
      __ Push(info->scope()->GetScopeInfo());
      __ CallRuntime(Runtime::kNewGlobalContext, 2);
    } else if (heap_slots <= FastNewContextStub::kMaximumSlots) {
      FastNewContextStub stub(isolate(), heap_slots);
      __ CallStub(&stub);
      // Result of FastNewContextStub is always in new space.
      need_write_barrier = false;
    } else {
      __ push(a1);
      __ CallRuntime(Runtime::kNewFunctionContext, 1);
    }
    function_in_register = false;
    // Context is returned in v0. It replaces the context passed to us.
    // It's saved in the stack and kept live in cp.
    __ mov(cp, v0);
    __ sw(v0, MemOperand(fp, StandardFrameConstants::kContextOffset));
    // Copy any necessary parameters into the context.
    int num_parameters = info->scope()->num_parameters();
    for (int i = 0; i < num_parameters; i++) {
      Variable* var = scope()->parameter(i);
      if (var->IsContextSlot()) {
        int parameter_offset = StandardFrameConstants::kCallerSPOffset +
                                 (num_parameters - 1 - i) * kPointerSize;
        // Load parameter from stack.
        __ lw(a0, MemOperand(fp, parameter_offset));
        // Store it in the context.
        MemOperand target = ContextOperand(cp, var->index());
        __ sw(a0, target);

        // Update the write barrier.
        if (need_write_barrier) {
          __ RecordWriteContextSlot(
              cp, target.offset(), a0, a3, kRAHasBeenSaved, kDontSaveFPRegs);
        } else if (FLAG_debug_code) {
          Label done;
          __ JumpIfInNewSpace(cp, a0, &done);
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
    if (!function_in_register) {
      // Load this again, if it's used by the local context below.
      __ lw(a3, MemOperand(fp, JavaScriptFrameConstants::kFunctionOffset));
    } else {
      __ mov(a3, a1);
    }
    // Receiver is just before the parameters on the caller's stack.
    int num_parameters = info->scope()->num_parameters();
    int offset = num_parameters * kPointerSize;
    __ Addu(a2, fp,
           Operand(StandardFrameConstants::kCallerSPOffset + offset));
    __ li(a1, Operand(Smi::FromInt(num_parameters)));
    __ Push(a3, a2, a1);

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
    ArgumentsAccessStub stub(isolate(), type);
    __ CallStub(&stub);

    SetVar(arguments, v0, a1, a2);
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
      __ LoadRoot(at, Heap::kStackLimitRootIndex);
      __ Branch(&ok, hs, sp, Operand(at));
      Handle<Code> stack_check = isolate()->builtins()->StackCheck();
      PredictableCodeSizeScope predictable(masm_,
          masm_->CallSize(stack_check, RelocInfo::CODE_TARGET));
      __ Call(stack_check, RelocInfo::CODE_TARGET);
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
  PrepareForBailoutForId(stmt->EntryId(), NO_REGISTERS);
  // Record a mapping of the OSR id to this PC.  This is used if the OSR
  // entry becomes the target of a bailout.  We don't expect it to be, but
  // we want it to work if it is.
  PrepareForBailoutForId(stmt->OsrEntryId(), NO_REGISTERS);
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
    __ Branch(&ok, ge, a3, Operand(zero_reg));
    __ push(v0);
    __ Call(isolate()->builtins()->InterruptCheck(),
            RelocInfo::CODE_TARGET);
    __ pop(v0);
    EmitProfilingCounterReset();
    __ bind(&ok);

#ifdef DEBUG
    // Add a label for checking the size of the code used for returning.
    Label check_exit_codesize;
    masm_->bind(&check_exit_codesize);
#endif
    // Make sure that the constant pool is not emitted inside of the return
    // sequence.
    { Assembler::BlockTrampolinePoolScope block_trampoline_pool(masm_);
      // Here we use masm_-> instead of the __ macro to avoid the code coverage
      // tool from instrumenting as we rely on the code size here.
      int32_t sp_delta = (info_->scope()->num_parameters() + 1) * kPointerSize;
      CodeGenerator::RecordPositions(masm_, function()->end_position() - 1);
      __ RecordJSReturn();
      masm_->mov(sp, fp);
      int no_frame_start = masm_->pc_offset();
      masm_->MultiPop(static_cast<RegList>(fp.bit() | ra.bit()));
      masm_->Addu(sp, sp, Operand(sp_delta));
      masm_->Jump(ra);
      info_->AddNoFrameRange(no_frame_start, masm_->pc_offset());
    }

#ifdef DEBUG
    // Check that the size of the code used for returning is large enough
    // for the debugger's requirements.
    DCHECK(Assembler::kJSReturnSequenceInstructions <=
           masm_->InstructionsGeneratedSince(&check_exit_codesize));
#endif
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
  codegen()->GetVar(result_register(), var);
  __ push(result_register());
}


void FullCodeGenerator::TestContext::Plug(Variable* var) const {
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
  __ push(result_register());
}


void FullCodeGenerator::TestContext::Plug(Handle<Object> lit) const {
  codegen()->PrepareForBailoutBeforeSplit(condition(),
                                          true,
                                          true_label_,
                                          false_label_);
  DCHECK(!lit->IsUndetectableObject());  // There are no undetectable literals.
  if (lit->IsUndefined() || lit->IsNull() || lit->IsFalse()) {
    if (false_label_ != fall_through_) __ Branch(false_label_);
  } else if (lit->IsTrue() || lit->IsJSObject()) {
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
  __ sw(reg, MemOperand(sp, 0));
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
  __ LoadRoot(result_register(), Heap::kTrueValueRootIndex);
  __ Branch(&done);
  __ bind(materialize_false);
  __ LoadRoot(result_register(), Heap::kFalseValueRootIndex);
  __ bind(&done);
}


void FullCodeGenerator::StackValueContext::Plug(
    Label* materialize_true,
    Label* materialize_false) const {
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
  __ LoadRoot(at, value_root_index);
  __ push(at);
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
  Handle<Code> ic = ToBooleanStub::GetUninitialized(isolate());
  CallIC(ic, condition->test_id());
  __ mov(at, zero_reg);
  Split(ne, v0, Operand(at), if_true, if_false, fall_through);
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
    return ContextOperand(scratch, var->index());
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
  if (!context()->IsTest() || !info_->IsOptimizable()) return;

  Label skip;
  if (should_normalize) __ Branch(&skip);
  PrepareForBailout(expr, TOS_REG);
  if (should_normalize) {
    __ LoadRoot(t0, Heap::kTrueValueRootIndex);
    Split(eq, a0, Operand(t0), if_true, if_false, NULL);
    __ bind(&skip);
  }
}


void FullCodeGenerator::EmitDebugCheckDeclarationContext(Variable* variable) {
  // The variable in the declaration always resides in the current function
  // context.
  DCHECK_EQ(0, scope()->ContextChainLength(variable->scope()));
  if (generate_debug_code_) {
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
        __ LoadRoot(t0, Heap::kTheHoleValueRootIndex);
        __ sw(t0, StackOperand(variable));
      }
      break;

      case Variable::CONTEXT:
      if (hole_init) {
        Comment cmnt(masm_, "[ VariableDeclaration");
        EmitDebugCheckDeclarationContext(variable);
          __ LoadRoot(at, Heap::kTheHoleValueRootIndex);
          __ sw(at, ContextOperand(cp, variable->index()));
          // No write barrier since the_hole_value is in old space.
          PrepareForBailoutForId(proxy->id(), NO_REGISTERS);
      }
      break;

    case Variable::LOOKUP: {
      Comment cmnt(masm_, "[ VariableDeclaration");
      __ li(a2, Operand(variable->name()));
      // Declaration nodes are always introduced in one of four modes.
      DCHECK(IsDeclaredVariableMode(mode));
      PropertyAttributes attr =
          IsImmutableVariableMode(mode) ? READ_ONLY : NONE;
      __ li(a1, Operand(Smi::FromInt(attr)));
      // Push initial value, if any.
      // Note: For variables we must not push an initial value (such as
      // 'undefined') because we may have a (legal) redeclaration and we
      // must not destroy the current value.
      if (hole_init) {
        __ LoadRoot(a0, Heap::kTheHoleValueRootIndex);
        __ Push(cp, a2, a1, a0);
      } else {
        DCHECK(Smi::FromInt(0) == 0);
        __ mov(a0, zero_reg);  // Smi::FromInt(0) indicates no initial value.
        __ Push(cp, a2, a1, a0);
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
      __ sw(result_register(), StackOperand(variable));
      break;
    }

    case Variable::CONTEXT: {
      Comment cmnt(masm_, "[ FunctionDeclaration");
      EmitDebugCheckDeclarationContext(variable);
      VisitForAccumulatorValue(declaration->fun());
      __ sw(result_register(), ContextOperand(cp, variable->index()));
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
      PrepareForBailoutForId(proxy->id(), NO_REGISTERS);
      break;
    }

    case Variable::LOOKUP: {
      Comment cmnt(masm_, "[ FunctionDeclaration");
      __ li(a2, Operand(variable->name()));
      __ li(a1, Operand(Smi::FromInt(NONE)));
      __ Push(cp, a2, a1);
      // Push initial value for function declaration.
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
  __ LoadContext(a1, scope_->ContextChainLength(scope_->GlobalScope()));
  __ lw(a1, ContextOperand(a1, variable->interface()->Index()));
  __ lw(a1, ContextOperand(a1, Context::EXTENSION_INDEX));

  // Assign it.
  __ sw(a1, ContextOperand(cp, variable->index()));
  // We know that we have written a module, which is not a smi.
  __ RecordWriteContextSlot(cp,
                            Context::SlotOffset(variable->index()),
                            a1,
                            a3,
                            kRAHasBeenSaved,
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
  __ li(a1, Operand(pairs));
  __ li(a0, Operand(Smi::FromInt(DeclareGlobalsFlags())));
  __ Push(cp, a1, a0);
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
    SetSourcePosition(clause->position());
    Handle<Code> ic = CompareIC::GetUninitialized(isolate(), Token::EQ_STRICT);
    CallIC(ic, clause->CompareId());
    patch_site.EmitPatchInfo();

    Label skip;
    __ Branch(&skip);
    PrepareForBailout(clause, TOS_REG);
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
  __ Drop(1);  // Switch value is no longer needed.
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
  __ mov(a0, result_register());  // Result as param to InvokeBuiltin below.
  __ LoadRoot(at, Heap::kUndefinedValueRootIndex);
  __ Branch(&exit, eq, a0, Operand(at));
  Register null_value = t1;
  __ LoadRoot(null_value, Heap::kNullValueRootIndex);
  __ Branch(&exit, eq, a0, Operand(null_value));
  PrepareForBailoutForId(stmt->PrepareId(), TOS_REG);
  __ mov(a0, v0);
  // Convert the object to a JS object.
  Label convert, done_convert;
  __ JumpIfSmi(a0, &convert);
  __ GetObjectType(a0, a1, a1);
  __ Branch(&done_convert, ge, a1, Operand(FIRST_SPEC_OBJECT_TYPE));
  __ bind(&convert);
  __ push(a0);
  __ InvokeBuiltin(Builtins::TO_OBJECT, CALL_FUNCTION);
  __ mov(a0, v0);
  __ bind(&done_convert);
  __ push(a0);

  // Check for proxies.
  Label call_runtime;
  STATIC_ASSERT(FIRST_JS_PROXY_TYPE == FIRST_SPEC_OBJECT_TYPE);
  __ GetObjectType(a0, a1, a1);
  __ Branch(&call_runtime, le, a1, Operand(LAST_JS_PROXY_TYPE));

  // Check cache validity in generated code. This is a fast case for
  // the JSObject::IsSimpleEnum cache validity checks. If we cannot
  // guarantee cache validity, call the runtime system to check cache
  // validity or get the property names in a fixed array.
  __ CheckEnumCache(null_value, &call_runtime);

  // The enum cache is valid.  Load the map of the object being
  // iterated over and use the cache for the iteration.
  Label use_cache;
  __ lw(v0, FieldMemOperand(a0, HeapObject::kMapOffset));
  __ Branch(&use_cache);

  // Get the set of properties to enumerate.
  __ bind(&call_runtime);
  __ push(a0);  // Duplicate the enumerable object on the stack.
  __ CallRuntime(Runtime::kGetPropertyNamesFast, 1);

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
  Label non_proxy;
  __ bind(&fixed_array);

  __ li(a1, FeedbackVector());
  __ li(a2, Operand(TypeFeedbackInfo::MegamorphicSentinel(isolate())));
  __ sw(a2, FieldMemOperand(a1, FixedArray::OffsetOfElementAt(slot)));

  __ li(a1, Operand(Smi::FromInt(1)));  // Smi indicates slow check
  __ lw(a2, MemOperand(sp, 0 * kPointerSize));  // Get enumerated object
  STATIC_ASSERT(FIRST_JS_PROXY_TYPE == FIRST_SPEC_OBJECT_TYPE);
  __ GetObjectType(a2, a3, a3);
  __ Branch(&non_proxy, gt, a3, Operand(LAST_JS_PROXY_TYPE));
  __ li(a1, Operand(Smi::FromInt(0)));  // Zero indicates proxy
  __ bind(&non_proxy);
  __ Push(a1, v0);  // Smi and array
  __ lw(a1, FieldMemOperand(v0, FixedArray::kLengthOffset));
  __ li(a0, Operand(Smi::FromInt(0)));
  __ Push(a1, a0);  // Fixed array length (as smi) and initial index.

  // Generate code for doing the condition check.
  PrepareForBailoutForId(stmt->BodyId(), NO_REGISTERS);
  __ bind(&loop);
  // Load the current count to a0, load the length to a1.
  __ lw(a0, MemOperand(sp, 0 * kPointerSize));
  __ lw(a1, MemOperand(sp, 1 * kPointerSize));
  __ Branch(loop_statement.break_label(), hs, a0, Operand(a1));

  // Get the current entry of the array into register a3.
  __ lw(a2, MemOperand(sp, 2 * kPointerSize));
  __ Addu(a2, a2, Operand(FixedArray::kHeaderSize - kHeapObjectTag));
  __ sll(t0, a0, kPointerSizeLog2 - kSmiTagSize);
  __ addu(t0, a2, t0);  // Array base + scaled (smi) index.
  __ lw(a3, MemOperand(t0));  // Current entry.

  // Get the expected map from the stack or a smi in the
  // permanent slow case into register a2.
  __ lw(a2, MemOperand(sp, 3 * kPointerSize));

  // Check if the expected map still matches that of the enumerable.
  // If not, we may have to filter the key.
  Label update_each;
  __ lw(a1, MemOperand(sp, 4 * kPointerSize));
  __ lw(t0, FieldMemOperand(a1, HeapObject::kMapOffset));
  __ Branch(&update_each, eq, t0, Operand(a2));

  // For proxies, no filtering is done.
  // TODO(rossberg): What if only a prototype is a proxy? Not specified yet.
  DCHECK_EQ(Smi::FromInt(0), 0);
  __ Branch(&update_each, eq, a2, Operand(zero_reg));

  // Convert the entry to a string or (smi) 0 if it isn't a property
  // any more. If the property has been removed while iterating, we
  // just skip it.
  __ Push(a1, a3);  // Enumerable and current entry.
  __ InvokeBuiltin(Builtins::FILTER_KEY, CALL_FUNCTION);
  __ mov(a3, result_register());
  __ Branch(loop_statement.continue_label(), eq, a3, Operand(zero_reg));

  // Update the 'each' property or variable from the possibly filtered
  // entry in register a3.
  __ bind(&update_each);
  __ mov(result_register(), a3);
  // Perform the assignment as if via '='.
  { EffectContext context(this);
    EmitAssignment(stmt->each());
  }

  // Generate code for the body of the loop.
  Visit(stmt->body());

  // Generate code for the going to the next element by incrementing
  // the index (smi) stored on top of the stack.
  __ bind(loop_statement.continue_label());
  __ pop(a0);
  __ Addu(a0, a0, Operand(Smi::FromInt(1)));
  __ push(a0);

  EmitBackEdgeBookkeeping(stmt, &loop);
  __ Branch(&loop);

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
    __ li(a2, Operand(info));
    __ CallStub(&stub);
  } else {
    __ li(a0, Operand(info));
    __ LoadRoot(a1, pretenure ? Heap::kTrueValueRootIndex
                              : Heap::kFalseValueRootIndex);
    __ Push(cp, a0, a1);
    __ CallRuntime(Runtime::kNewClosure, 3);
  }
  context()->Plug(v0);
}


void FullCodeGenerator::VisitVariableProxy(VariableProxy* expr) {
  Comment cmnt(masm_, "[ VariableProxy");
  EmitVariableLoad(expr);
}


void FullCodeGenerator::EmitLoadGlobalCheckExtensions(VariableProxy* proxy,
                                                      TypeofState typeof_state,
                                                      Label* slow) {
  Register current = cp;
  Register next = a1;
  Register temp = a2;

  Scope* s = scope();
  while (s != NULL) {
    if (s->num_heap_slots() > 0) {
      if (s->calls_sloppy_eval()) {
        // Check that extension is NULL.
        __ lw(temp, ContextOperand(current, Context::EXTENSION_INDEX));
        __ Branch(slow, ne, temp, Operand(zero_reg));
      }
      // Load next context in chain.
      __ lw(next, ContextOperand(current, Context::PREVIOUS_INDEX));
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
    __ lw(temp, FieldMemOperand(next, HeapObject::kMapOffset));
    __ LoadRoot(t0, Heap::kNativeContextMapRootIndex);
    __ Branch(&fast, eq, temp, Operand(t0));
    // Check that extension is NULL.
    __ lw(temp, ContextOperand(next, Context::EXTENSION_INDEX));
    __ Branch(slow, ne, temp, Operand(zero_reg));
    // Load next context in chain.
    __ lw(next, ContextOperand(next, Context::PREVIOUS_INDEX));
    __ Branch(&loop);
    __ bind(&fast);
  }

  __ lw(LoadIC::ReceiverRegister(), GlobalObjectOperand());
  __ li(LoadIC::NameRegister(), Operand(proxy->var()->name()));
  if (FLAG_vector_ics) {
    __ li(LoadIC::SlotRegister(),
          Operand(Smi::FromInt(proxy->VariableFeedbackSlot())));
  }

  ContextualMode mode = (typeof_state == INSIDE_TYPEOF)
      ? NOT_CONTEXTUAL
      : CONTEXTUAL;
  CallLoadIC(mode);
}


MemOperand FullCodeGenerator::ContextSlotOperandCheckExtensions(Variable* var,
                                                                Label* slow) {
  DCHECK(var->IsContextSlot());
  Register context = cp;
  Register next = a3;
  Register temp = t0;

  for (Scope* s = scope(); s != var->scope(); s = s->outer_scope()) {
    if (s->num_heap_slots() > 0) {
      if (s->calls_sloppy_eval()) {
        // Check that extension is NULL.
        __ lw(temp, ContextOperand(context, Context::EXTENSION_INDEX));
        __ Branch(slow, ne, temp, Operand(zero_reg));
      }
      __ lw(next, ContextOperand(context, Context::PREVIOUS_INDEX));
      // Walk the rest of the chain without clobbering cp.
      context = next;
    }
  }
  // Check that last extension is NULL.
  __ lw(temp, ContextOperand(context, Context::EXTENSION_INDEX));
  __ Branch(slow, ne, temp, Operand(zero_reg));

  // This function is used only for loads, not stores, so it's safe to
  // return an cp-based operand (the write barrier cannot be allowed to
  // destroy the cp register).
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
    __ Branch(done);
  } else if (var->mode() == DYNAMIC_LOCAL) {
    Variable* local = var->local_if_not_shadowed();
    __ lw(v0, ContextSlotOperandCheckExtensions(local, slow));
    if (local->mode() == LET || local->mode() == CONST ||
        local->mode() == CONST_LEGACY) {
      __ LoadRoot(at, Heap::kTheHoleValueRootIndex);
      __ subu(at, v0, at);  // Sub as compare: at == 0 on eq.
      if (local->mode() == CONST_LEGACY) {
        __ LoadRoot(a0, Heap::kUndefinedValueRootIndex);
        __ Movz(v0, a0, at);  // Conditional move: return Undefined if TheHole.
      } else {  // LET || CONST
        __ Branch(done, ne, at, Operand(zero_reg));
        __ li(a0, Operand(var->name()));
        __ push(a0);
        __ CallRuntime(Runtime::kThrowReferenceError, 1);
      }
    }
    __ Branch(done);
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
      __ lw(LoadIC::ReceiverRegister(), GlobalObjectOperand());
      __ li(LoadIC::NameRegister(), Operand(var->name()));
      if (FLAG_vector_ics) {
        __ li(LoadIC::SlotRegister(),
              Operand(Smi::FromInt(proxy->VariableFeedbackSlot())));
      }
      CallLoadIC(CONTEXTUAL);
      context()->Plug(v0);
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
          GetVar(v0, var);
          __ LoadRoot(at, Heap::kTheHoleValueRootIndex);
          __ subu(at, v0, at);  // Sub as compare: at == 0 on eq.
          if (var->mode() == LET || var->mode() == CONST) {
            // Throw a reference error when using an uninitialized let/const
            // binding in harmony mode.
            Label done;
            __ Branch(&done, ne, at, Operand(zero_reg));
            __ li(a0, Operand(var->name()));
            __ push(a0);
            __ CallRuntime(Runtime::kThrowReferenceError, 1);
            __ bind(&done);
          } else {
            // Uninitalized const bindings outside of harmony mode are unholed.
            DCHECK(var->mode() == CONST_LEGACY);
            __ LoadRoot(a0, Heap::kUndefinedValueRootIndex);
            __ Movz(v0, a0, at);  // Conditional move: Undefined if TheHole.
          }
          context()->Plug(v0);
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
      __ li(a1, Operand(var->name()));
      __ Push(cp, a1);  // Context and name.
      __ CallRuntime(Runtime::kLoadLookupSlot, 2);
      __ bind(&done);
      context()->Plug(v0);
    }
  }
}


void FullCodeGenerator::VisitRegExpLiteral(RegExpLiteral* expr) {
  Comment cmnt(masm_, "[ RegExpLiteral");
  Label materialized;
  // Registers will be used as follows:
  // t1 = materialized value (RegExp literal)
  // t0 = JS function, literals array
  // a3 = literal index
  // a2 = RegExp pattern
  // a1 = RegExp flags
  // a0 = RegExp literal clone
  __ lw(a0, MemOperand(fp, JavaScriptFrameConstants::kFunctionOffset));
  __ lw(t0, FieldMemOperand(a0, JSFunction::kLiteralsOffset));
  int literal_offset =
      FixedArray::kHeaderSize + expr->literal_index() * kPointerSize;
  __ lw(t1, FieldMemOperand(t0, literal_offset));
  __ LoadRoot(at, Heap::kUndefinedValueRootIndex);
  __ Branch(&materialized, ne, t1, Operand(at));

  // Create regexp literal using runtime function.
  // Result will be in v0.
  __ li(a3, Operand(Smi::FromInt(expr->literal_index())));
  __ li(a2, Operand(expr->pattern()));
  __ li(a1, Operand(expr->flags()));
  __ Push(t0, a3, a2, a1);
  __ CallRuntime(Runtime::kMaterializeRegExpLiteral, 4);
  __ mov(t1, v0);

  __ bind(&materialized);
  int size = JSRegExp::kSize + JSRegExp::kInObjectFieldCount * kPointerSize;
  Label allocated, runtime_allocate;
  __ Allocate(size, v0, a2, a3, &runtime_allocate, TAG_OBJECT);
  __ jmp(&allocated);

  __ bind(&runtime_allocate);
  __ li(a0, Operand(Smi::FromInt(size)));
  __ Push(t1, a0);
  __ CallRuntime(Runtime::kAllocateInNewSpace, 1);
  __ pop(t1);

  __ bind(&allocated);

  // After this, registers are used as follows:
  // v0: Newly allocated regexp.
  // t1: Materialized regexp.
  // a2: temp.
  __ CopyFields(v0, t1, a2.bit(), size / kPointerSize);
  context()->Plug(v0);
}


void FullCodeGenerator::EmitAccessor(Expression* expression) {
  if (expression == NULL) {
    __ LoadRoot(a1, Heap::kNullValueRootIndex);
    __ push(a1);
  } else {
    VisitForStackValue(expression);
  }
}


void FullCodeGenerator::VisitObjectLiteral(ObjectLiteral* expr) {
  Comment cmnt(masm_, "[ ObjectLiteral");

  expr->BuildConstantProperties(isolate());
  Handle<FixedArray> constant_properties = expr->constant_properties();
  __ lw(a3, MemOperand(fp, JavaScriptFrameConstants::kFunctionOffset));
  __ lw(a3, FieldMemOperand(a3, JSFunction::kLiteralsOffset));
  __ li(a2, Operand(Smi::FromInt(expr->literal_index())));
  __ li(a1, Operand(constant_properties));
  int flags = expr->fast_elements()
      ? ObjectLiteral::kFastElements
      : ObjectLiteral::kNoFlags;
  flags |= expr->has_function()
      ? ObjectLiteral::kHasFunction
      : ObjectLiteral::kNoFlags;
  __ li(a0, Operand(Smi::FromInt(flags)));
  int properties_count = constant_properties->length() / 2;
  if (expr->may_store_doubles() || expr->depth() > 1 ||
      masm()->serializer_enabled() || flags != ObjectLiteral::kFastElements ||
      properties_count > FastCloneShallowObjectStub::kMaximumClonedProperties) {
    __ Push(a3, a2, a1, a0);
    __ CallRuntime(Runtime::kCreateObjectLiteral, 4);
  } else {
    FastCloneShallowObjectStub stub(isolate(), properties_count);
    __ CallStub(&stub);
  }

  // If result_saved is true the result is on top of the stack.  If
  // result_saved is false the result is in v0.
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
      __ push(v0);  // Save result on stack.
      result_saved = true;
    }
    switch (property->kind()) {
      case ObjectLiteral::Property::CONSTANT:
        UNREACHABLE();
      case ObjectLiteral::Property::MATERIALIZED_LITERAL:
        DCHECK(!CompileTimeValue::IsCompileTimeValue(property->value()));
        // Fall through.
      case ObjectLiteral::Property::COMPUTED:
        if (key->value()->IsInternalizedString()) {
          if (property->emit_store()) {
            VisitForAccumulatorValue(value);
            __ mov(StoreIC::ValueRegister(), result_register());
            DCHECK(StoreIC::ValueRegister().is(a0));
            __ li(StoreIC::NameRegister(), Operand(key->value()));
            __ lw(StoreIC::ReceiverRegister(), MemOperand(sp));
            CallStoreIC(key->LiteralFeedbackId());
            PrepareForBailoutForId(key->id(), NO_REGISTERS);
          } else {
            VisitForEffect(value);
          }
          break;
        }
        // Duplicate receiver on stack.
        __ lw(a0, MemOperand(sp));
        __ push(a0);
        VisitForStackValue(key);
        VisitForStackValue(value);
        if (property->emit_store()) {
          __ li(a0, Operand(Smi::FromInt(SLOPPY)));  // PropertyAttributes.
          __ push(a0);
          __ CallRuntime(Runtime::kSetProperty, 4);
        } else {
          __ Drop(3);
        }
        break;
      case ObjectLiteral::Property::PROTOTYPE:
        // Duplicate receiver on stack.
        __ lw(a0, MemOperand(sp));
        __ push(a0);
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
    __ lw(a0, MemOperand(sp));  // Duplicate receiver.
    __ push(a0);
    VisitForStackValue(it->first);
    EmitAccessor(it->second->getter);
    EmitAccessor(it->second->setter);
    __ li(a0, Operand(Smi::FromInt(NONE)));
    __ push(a0);
    __ CallRuntime(Runtime::kDefineAccessorPropertyUnchecked, 5);
  }

  if (expr->has_function()) {
    DCHECK(result_saved);
    __ lw(a0, MemOperand(sp));
    __ push(a0);
    __ CallRuntime(Runtime::kToFastProperties, 1);
  }

  if (result_saved) {
    context()->PlugTOS();
  } else {
    context()->Plug(v0);
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
  bool has_fast_elements =
      IsFastObjectElementsKind(constant_elements_kind);
  Handle<FixedArrayBase> constant_elements_values(
      FixedArrayBase::cast(constant_elements->get(1)));

  AllocationSiteMode allocation_site_mode = TRACK_ALLOCATION_SITE;
  if (has_fast_elements && !FLAG_allocation_site_pretenuring) {
    // If the only customer of allocation sites is transitioning, then
    // we can turn it off if we don't have anywhere else to transition to.
    allocation_site_mode = DONT_TRACK_ALLOCATION_SITE;
  }

  __ mov(a0, result_register());
  __ lw(a3, MemOperand(fp, JavaScriptFrameConstants::kFunctionOffset));
  __ lw(a3, FieldMemOperand(a3, JSFunction::kLiteralsOffset));
  __ li(a2, Operand(Smi::FromInt(expr->literal_index())));
  __ li(a1, Operand(constant_elements));
  if (expr->depth() > 1 || length > JSObject::kInitialMaxFastElementArray) {
    __ li(a0, Operand(Smi::FromInt(flags)));
    __ Push(a3, a2, a1, a0);
    __ CallRuntime(Runtime::kCreateArrayLiteral, 4);
  } else {
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
      __ push(v0);  // array literal
      __ Push(Smi::FromInt(expr->literal_index()));
      result_saved = true;
    }

    VisitForAccumulatorValue(subexpr);

    if (IsFastObjectElementsKind(constant_elements_kind)) {
      int offset = FixedArray::kHeaderSize + (i * kPointerSize);
      __ lw(t2, MemOperand(sp, kPointerSize));  // Copy of array literal.
      __ lw(a1, FieldMemOperand(t2, JSObject::kElementsOffset));
      __ sw(result_register(), FieldMemOperand(a1, offset));
      // Update the write barrier for the array store.
      __ RecordWriteField(a1, offset, result_register(), a2,
                          kRAHasBeenSaved, kDontSaveFPRegs,
                          EMIT_REMEMBERED_SET, INLINE_SMI_CHECK);
    } else {
      __ li(a3, Operand(Smi::FromInt(i)));
      __ mov(a0, result_register());
      StoreArrayLiteralElementStub stub(isolate());
      __ CallStub(&stub);
    }

    PrepareForBailoutForId(expr->GetIdForElement(i), NO_REGISTERS);
  }
  if (result_saved) {
    __ Pop();  // literal index
    context()->PlugTOS();
  } else {
    context()->Plug(v0);
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
        __ lw(LoadIC::ReceiverRegister(), MemOperand(sp, 0));
      } else {
        VisitForStackValue(property->obj());
      }
      break;
    case KEYED_PROPERTY:
      // We need the key and receiver on both the stack and in v0 and a1.
      if (expr->is_compound()) {
        VisitForStackValue(property->obj());
        VisitForStackValue(property->key());
        __ lw(LoadIC::ReceiverRegister(), MemOperand(sp, 1 * kPointerSize));
        __ lw(LoadIC::NameRegister(), MemOperand(sp, 0));
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
    __ push(v0);  // Left operand goes on the stack.
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
      context()->Plug(v0);
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
      __ lw(cp, MemOperand(fp, StandardFrameConstants::kContextOffset));
      __ bind(&post_runtime);
      __ pop(result_register());
      EmitReturnSequence();

      __ bind(&resume);
      context()->Plug(result_register());
      break;
    }

    case Yield::FINAL: {
      VisitForAccumulatorValue(expr->generator_object());
      __ li(a1, Operand(Smi::FromInt(JSGeneratorObject::kGeneratorClosed)));
      __ sw(a1, FieldMemOperand(result_register(),
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
      Label l_next, l_call;
      Register load_receiver = LoadIC::ReceiverRegister();
      Register load_name = LoadIC::NameRegister();

      // Initial send value is undefined.
      __ LoadRoot(a0, Heap::kUndefinedValueRootIndex);
      __ Branch(&l_next);

      // catch (e) { receiver = iter; f = 'throw'; arg = e; goto l_call; }
      __ bind(&l_catch);
      __ mov(a0, v0);
      handler_table()->set(expr->index(), Smi::FromInt(l_catch.pos()));
      __ LoadRoot(load_name, Heap::kthrow_stringRootIndex);  // "throw"
      __ lw(a3, MemOperand(sp, 1 * kPointerSize));           // iter
      __ Push(load_name, a3, a0);                     // "throw", iter, except
      __ jmp(&l_call);

      // try { received = %yield result }
      // Shuffle the received result above a try handler and yield it without
      // re-boxing.
      __ bind(&l_try);
      __ pop(a0);                                        // result
      __ PushTryHandler(StackHandler::CATCH, expr->index());
      const int handler_size = StackHandlerConstants::kSize;
      __ push(a0);                                       // result
      __ jmp(&l_suspend);
      __ bind(&l_continuation);
      __ mov(a0, v0);
      __ jmp(&l_resume);
      __ bind(&l_suspend);
      const int generator_object_depth = kPointerSize + handler_size;
      __ lw(a0, MemOperand(sp, generator_object_depth));
      __ push(a0);                                       // g
      DCHECK(l_continuation.pos() > 0 && Smi::IsValid(l_continuation.pos()));
      __ li(a1, Operand(Smi::FromInt(l_continuation.pos())));
      __ sw(a1, FieldMemOperand(a0, JSGeneratorObject::kContinuationOffset));
      __ sw(cp, FieldMemOperand(a0, JSGeneratorObject::kContextOffset));
      __ mov(a1, cp);
      __ RecordWriteField(a0, JSGeneratorObject::kContextOffset, a1, a2,
                          kRAHasBeenSaved, kDontSaveFPRegs);
      __ CallRuntime(Runtime::kSuspendJSGeneratorObject, 1);
      __ lw(cp, MemOperand(fp, StandardFrameConstants::kContextOffset));
      __ pop(v0);                                      // result
      EmitReturnSequence();
      __ mov(a0, v0);
      __ bind(&l_resume);                              // received in a0
      __ PopTryHandler();

      // receiver = iter; f = 'next'; arg = received;
      __ bind(&l_next);

      __ LoadRoot(load_name, Heap::knext_stringRootIndex);  // "next"
      __ lw(a3, MemOperand(sp, 1 * kPointerSize));          // iter
      __ Push(load_name, a3, a0);                      // "next", iter, received

      // result = receiver[f](arg);
      __ bind(&l_call);
      __ lw(load_receiver, MemOperand(sp, kPointerSize));
      __ lw(load_name, MemOperand(sp, 2 * kPointerSize));
      if (FLAG_vector_ics) {
        __ li(LoadIC::SlotRegister(),
              Operand(Smi::FromInt(expr->KeyedLoadFeedbackSlot())));
      }
      Handle<Code> ic = isolate()->builtins()->KeyedLoadIC_Initialize();
      CallIC(ic, TypeFeedbackId::None());
      __ mov(a0, v0);
      __ mov(a1, a0);
      __ sw(a1, MemOperand(sp, 2 * kPointerSize));
      CallFunctionStub stub(isolate(), 1, CALL_AS_METHOD);
      __ CallStub(&stub);

      __ lw(cp, MemOperand(fp, StandardFrameConstants::kContextOffset));
      __ Drop(1);  // The function is still on the stack; drop it.

      // if (!result.done) goto l_try;
      __ Move(load_receiver, v0);

      __ push(load_receiver);                               // save result
      __ LoadRoot(load_name, Heap::kdone_stringRootIndex);  // "done"
      if (FLAG_vector_ics) {
        __ li(LoadIC::SlotRegister(),
              Operand(Smi::FromInt(expr->DoneFeedbackSlot())));
      }
      CallLoadIC(NOT_CONTEXTUAL);                           // v0=result.done
      __ mov(a0, v0);
      Handle<Code> bool_ic = ToBooleanStub::GetUninitialized(isolate());
      CallIC(bool_ic);
      __ Branch(&l_try, eq, v0, Operand(zero_reg));

      // result.value
      __ pop(load_receiver);                                 // result
      __ LoadRoot(load_name, Heap::kvalue_stringRootIndex);  // "value"
      if (FLAG_vector_ics) {
        __ li(LoadIC::SlotRegister(),
              Operand(Smi::FromInt(expr->ValueFeedbackSlot())));
      }
      CallLoadIC(NOT_CONTEXTUAL);                            // v0=result.value
      context()->DropAndPlug(2, v0);                         // drop iter and g
      break;
    }
  }
}


void FullCodeGenerator::EmitGeneratorResume(Expression *generator,
    Expression *value,
    JSGeneratorObject::ResumeMode resume_mode) {
  // The value stays in a0, and is ultimately read by the resumed generator, as
  // if CallRuntime(Runtime::kSuspendJSGeneratorObject) returned it. Or it
  // is read to throw the value when the resumed generator is already closed.
  // a1 will hold the generator object until the activation has been resumed.
  VisitForStackValue(generator);
  VisitForAccumulatorValue(value);
  __ pop(a1);

  // Check generator state.
  Label wrong_state, closed_state, done;
  __ lw(a3, FieldMemOperand(a1, JSGeneratorObject::kContinuationOffset));
  STATIC_ASSERT(JSGeneratorObject::kGeneratorExecuting < 0);
  STATIC_ASSERT(JSGeneratorObject::kGeneratorClosed == 0);
  __ Branch(&closed_state, eq, a3, Operand(zero_reg));
  __ Branch(&wrong_state, lt, a3, Operand(zero_reg));

  // Load suspended function and context.
  __ lw(cp, FieldMemOperand(a1, JSGeneratorObject::kContextOffset));
  __ lw(t0, FieldMemOperand(a1, JSGeneratorObject::kFunctionOffset));

  // Load receiver and store as the first argument.
  __ lw(a2, FieldMemOperand(a1, JSGeneratorObject::kReceiverOffset));
  __ push(a2);

  // Push holes for the rest of the arguments to the generator function.
  __ lw(a3, FieldMemOperand(t0, JSFunction::kSharedFunctionInfoOffset));
  __ lw(a3,
        FieldMemOperand(a3, SharedFunctionInfo::kFormalParameterCountOffset));
  __ LoadRoot(a2, Heap::kTheHoleValueRootIndex);
  Label push_argument_holes, push_frame;
  __ bind(&push_argument_holes);
  __ Subu(a3, a3, Operand(Smi::FromInt(1)));
  __ Branch(&push_frame, lt, a3, Operand(zero_reg));
  __ push(a2);
  __ jmp(&push_argument_holes);

  // Enter a new JavaScript frame, and initialize its slots as they were when
  // the generator was suspended.
  Label resume_frame;
  __ bind(&push_frame);
  __ Call(&resume_frame);
  __ jmp(&done);
  __ bind(&resume_frame);
  // ra = return address.
  // fp = caller's frame pointer.
  // cp = callee's context,
  // t0 = callee's JS function.
  __ Push(ra, fp, cp, t0);
  // Adjust FP to point to saved FP.
  __ Addu(fp, sp, 2 * kPointerSize);

  // Load the operand stack size.
  __ lw(a3, FieldMemOperand(a1, JSGeneratorObject::kOperandStackOffset));
  __ lw(a3, FieldMemOperand(a3, FixedArray::kLengthOffset));
  __ SmiUntag(a3);

  // If we are sending a value and there is no operand stack, we can jump back
  // in directly.
  if (resume_mode == JSGeneratorObject::NEXT) {
    Label slow_resume;
    __ Branch(&slow_resume, ne, a3, Operand(zero_reg));
    __ lw(a3, FieldMemOperand(t0, JSFunction::kCodeEntryOffset));
    __ lw(a2, FieldMemOperand(a1, JSGeneratorObject::kContinuationOffset));
    __ SmiUntag(a2);
    __ Addu(a3, a3, Operand(a2));
    __ li(a2, Operand(Smi::FromInt(JSGeneratorObject::kGeneratorExecuting)));
    __ sw(a2, FieldMemOperand(a1, JSGeneratorObject::kContinuationOffset));
    __ Jump(a3);
    __ bind(&slow_resume);
  }

  // Otherwise, we push holes for the operand stack and call the runtime to fix
  // up the stack and the handlers.
  Label push_operand_holes, call_resume;
  __ bind(&push_operand_holes);
  __ Subu(a3, a3, Operand(1));
  __ Branch(&call_resume, lt, a3, Operand(zero_reg));
  __ push(a2);
  __ Branch(&push_operand_holes);
  __ bind(&call_resume);
  DCHECK(!result_register().is(a1));
  __ Push(a1, result_register());
  __ Push(Smi::FromInt(resume_mode));
  __ CallRuntime(Runtime::kResumeJSGeneratorObject, 3);
  // Not reached: the runtime call returns elsewhere.
  __ stop("not-reached");

  // Reach here when generator is closed.
  __ bind(&closed_state);
  if (resume_mode == JSGeneratorObject::NEXT) {
    // Return completed iterator result when generator is closed.
    __ LoadRoot(a2, Heap::kUndefinedValueRootIndex);
    __ push(a2);
    // Pop value from top-of-stack slot; box result into result register.
    EmitCreateIteratorResult(true);
  } else {
    // Throw the provided value.
    __ push(a0);
    __ CallRuntime(Runtime::kThrow, 1);
  }
  __ jmp(&done);

  // Throw error if we attempt to operate on a running generator.
  __ bind(&wrong_state);
  __ push(a1);
  __ CallRuntime(Runtime::kThrowGeneratorStateError, 1);

  __ bind(&done);
  context()->Plug(result_register());
}


void FullCodeGenerator::EmitCreateIteratorResult(bool done) {
  Label gc_required;
  Label allocated;

  Handle<Map> map(isolate()->native_context()->iterator_result_map());

  __ Allocate(map->instance_size(), v0, a2, a3, &gc_required, TAG_OBJECT);
  __ jmp(&allocated);

  __ bind(&gc_required);
  __ Push(Smi::FromInt(map->instance_size()));
  __ CallRuntime(Runtime::kAllocateInNewSpace, 1);
  __ lw(context_register(),
        MemOperand(fp, StandardFrameConstants::kContextOffset));

  __ bind(&allocated);
  __ li(a1, Operand(map));
  __ pop(a2);
  __ li(a3, Operand(isolate()->factory()->ToBoolean(done)));
  __ li(t0, Operand(isolate()->factory()->empty_fixed_array()));
  DCHECK_EQ(map->instance_size(), 5 * kPointerSize);
  __ sw(a1, FieldMemOperand(v0, HeapObject::kMapOffset));
  __ sw(t0, FieldMemOperand(v0, JSObject::kPropertiesOffset));
  __ sw(t0, FieldMemOperand(v0, JSObject::kElementsOffset));
  __ sw(a2,
        FieldMemOperand(v0, JSGeneratorObject::kResultValuePropertyOffset));
  __ sw(a3,
        FieldMemOperand(v0, JSGeneratorObject::kResultDonePropertyOffset));

  // Only the value field needs a write barrier, as the other values are in the
  // root set.
  __ RecordWriteField(v0, JSGeneratorObject::kResultValuePropertyOffset,
                      a2, a3, kRAHasBeenSaved, kDontSaveFPRegs);
}


void FullCodeGenerator::EmitNamedPropertyLoad(Property* prop) {
  SetSourcePosition(prop->position());
  Literal* key = prop->key()->AsLiteral();
  __ li(LoadIC::NameRegister(), Operand(key->value()));
  if (FLAG_vector_ics) {
    __ li(LoadIC::SlotRegister(),
          Operand(Smi::FromInt(prop->PropertyFeedbackSlot())));
    CallLoadIC(NOT_CONTEXTUAL);
  } else {
    CallLoadIC(NOT_CONTEXTUAL, prop->PropertyFeedbackId());
  }
}


void FullCodeGenerator::EmitKeyedPropertyLoad(Property* prop) {
  SetSourcePosition(prop->position());
  Handle<Code> ic = isolate()->builtins()->KeyedLoadIC_Initialize();
  if (FLAG_vector_ics) {
    __ li(LoadIC::SlotRegister(),
          Operand(Smi::FromInt(prop->PropertyFeedbackSlot())));
    CallIC(ic);
  } else {
    CallIC(ic, prop->PropertyFeedbackId());
  }
}


void FullCodeGenerator::EmitInlineSmiBinaryOp(BinaryOperation* expr,
                                              Token::Value op,
                                              OverwriteMode mode,
                                              Expression* left_expr,
                                              Expression* right_expr) {
  Label done, smi_case, stub_call;

  Register scratch1 = a2;
  Register scratch2 = a3;

  // Get the arguments.
  Register left = a1;
  Register right = a0;
  __ pop(left);
  __ mov(a0, result_register());

  // Perform combined smi check on both operands.
  __ Or(scratch1, left, Operand(right));
  STATIC_ASSERT(kSmiTag == 0);
  JumpPatchSite patch_site(masm_);
  patch_site.EmitJumpIfSmi(scratch1, &smi_case);

  __ bind(&stub_call);
  BinaryOpICStub stub(isolate(), op, mode);
  CallIC(stub.GetCode(), expr->BinaryOperationFeedbackId());
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
      __ AdduAndCheckForOverflow(v0, left, right, scratch1);
      __ BranchOnOverflow(&stub_call, scratch1);
      break;
    case Token::SUB:
      __ SubuAndCheckForOverflow(v0, left, right, scratch1);
      __ BranchOnOverflow(&stub_call, scratch1);
      break;
    case Token::MUL: {
      __ SmiUntag(scratch1, right);
      __ Mult(left, scratch1);
      __ mflo(scratch1);
      __ mfhi(scratch2);
      __ sra(scratch1, scratch1, 31);
      __ Branch(&stub_call, ne, scratch1, Operand(scratch2));
      __ mflo(v0);
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


void FullCodeGenerator::EmitBinaryOp(BinaryOperation* expr,
                                     Token::Value op,
                                     OverwriteMode mode) {
  __ mov(a0, result_register());
  __ pop(a1);
  BinaryOpICStub stub(isolate(), op, mode);
  JumpPatchSite patch_site(masm_);    // unbound, signals no inlined smi code.
  CallIC(stub.GetCode(), expr->BinaryOperationFeedbackId());
  patch_site.EmitPatchInfo();
  context()->Plug(v0);
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
      __ push(result_register());  // Preserve value.
      VisitForAccumulatorValue(prop->obj());
      __ mov(StoreIC::ReceiverRegister(), result_register());
      __ pop(StoreIC::ValueRegister());  // Restore value.
      __ li(StoreIC::NameRegister(),
            Operand(prop->key()->AsLiteral()->value()));
      CallStoreIC();
      break;
    }
    case KEYED_PROPERTY: {
      __ push(result_register());  // Preserve value.
      VisitForStackValue(prop->obj());
      VisitForAccumulatorValue(prop->key());
      __ mov(KeyedStoreIC::NameRegister(), result_register());
      __ Pop(KeyedStoreIC::ValueRegister(), KeyedStoreIC::ReceiverRegister());
      Handle<Code> ic = strict_mode() == SLOPPY
        ? isolate()->builtins()->KeyedStoreIC_Initialize()
        : isolate()->builtins()->KeyedStoreIC_Initialize_Strict();
      CallIC(ic);
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


void FullCodeGenerator::EmitVariableAssignment(Variable* var, Token::Value op) {
  if (var->IsUnallocated()) {
    // Global var, const, or let.
    __ mov(StoreIC::ValueRegister(), result_register());
    __ li(StoreIC::NameRegister(), Operand(var->name()));
    __ lw(StoreIC::ReceiverRegister(), GlobalObjectOperand());
    CallStoreIC();

  } else if (op == Token::INIT_CONST_LEGACY) {
    // Const initializers need a write barrier.
    DCHECK(!var->IsParameter());  // No const parameters.
    if (var->IsLookupSlot()) {
      __ li(a0, Operand(var->name()));
      __ Push(v0, cp, a0);  // Context and name.
      __ CallRuntime(Runtime::kInitializeLegacyConstLookupSlot, 3);
    } else {
      DCHECK(var->IsStackAllocated() || var->IsContextSlot());
      Label skip;
      MemOperand location = VarOperand(var, a1);
      __ lw(a2, location);
      __ LoadRoot(at, Heap::kTheHoleValueRootIndex);
      __ Branch(&skip, ne, a2, Operand(at));
      EmitStoreToStackLocalOrContextSlot(var, location);
      __ bind(&skip);
    }

  } else if (var->mode() == LET && op != Token::INIT_LET) {
    // Non-initializing assignment to let variable needs a write barrier.
    DCHECK(!var->IsLookupSlot());
    DCHECK(var->IsStackAllocated() || var->IsContextSlot());
    Label assign;
    MemOperand location = VarOperand(var, a1);
    __ lw(a3, location);
    __ LoadRoot(t0, Heap::kTheHoleValueRootIndex);
    __ Branch(&assign, ne, a3, Operand(t0));
    __ li(a3, Operand(var->name()));
    __ push(a3);
    __ CallRuntime(Runtime::kThrowReferenceError, 1);
    // Perform the assignment.
    __ bind(&assign);
    EmitStoreToStackLocalOrContextSlot(var, location);

  } else if (!var->is_const_mode() || op == Token::INIT_CONST) {
    if (var->IsLookupSlot()) {
      // Assignment to var.
      __ li(a1, Operand(var->name()));
      __ li(a0, Operand(Smi::FromInt(strict_mode())));
      __ Push(v0, cp, a1, a0);  // Value, context, name, strict mode.
      __ CallRuntime(Runtime::kStoreLookupSlot, 4);
    } else {
      // Assignment to var or initializing assignment to let/const in harmony
      // mode.
      DCHECK((var->IsStackAllocated() || var->IsContextSlot()));
      MemOperand location = VarOperand(var, a1);
      if (generate_debug_code_ && op == Token::INIT_LET) {
        // Check for an uninitialized let binding.
        __ lw(a2, location);
        __ LoadRoot(t0, Heap::kTheHoleValueRootIndex);
        __ Check(eq, kLetBindingReInitialization, a2, Operand(t0));
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
  __ mov(StoreIC::ValueRegister(), result_register());
  __ li(StoreIC::NameRegister(), Operand(prop->key()->AsLiteral()->value()));
  __ pop(StoreIC::ReceiverRegister());
  CallStoreIC(expr->AssignmentFeedbackId());

  PrepareForBailoutForId(expr->AssignmentId(), TOS_REG);
  context()->Plug(v0);
}


void FullCodeGenerator::EmitKeyedPropertyAssignment(Assignment* expr) {
  // Assignment to a property, using a keyed store IC.

  // Record source code position before IC call.
  SetSourcePosition(expr->position());
  // Call keyed store IC.
  // The arguments are:
  // - a0 is the value,
  // - a1 is the key,
  // - a2 is the receiver.
  __ mov(KeyedStoreIC::ValueRegister(), result_register());
  __ Pop(KeyedStoreIC::ReceiverRegister(), KeyedStoreIC::NameRegister());
  DCHECK(KeyedStoreIC::ValueRegister().is(a0));

  Handle<Code> ic = strict_mode() == SLOPPY
      ? isolate()->builtins()->KeyedStoreIC_Initialize()
      : isolate()->builtins()->KeyedStoreIC_Initialize_Strict();
  CallIC(ic, expr->AssignmentFeedbackId());

  PrepareForBailoutForId(expr->AssignmentId(), TOS_REG);
  context()->Plug(v0);
}


void FullCodeGenerator::VisitProperty(Property* expr) {
  Comment cmnt(masm_, "[ Property");
  Expression* key = expr->key();

  if (key->IsPropertyName()) {
    VisitForAccumulatorValue(expr->obj());
    __ Move(LoadIC::ReceiverRegister(), v0);
    EmitNamedPropertyLoad(expr);
    PrepareForBailoutForId(expr->LoadId(), TOS_REG);
    context()->Plug(v0);
  } else {
    VisitForStackValue(expr->obj());
    VisitForAccumulatorValue(expr->key());
    __ Move(LoadIC::NameRegister(), v0);
    __ pop(LoadIC::ReceiverRegister());
    EmitKeyedPropertyLoad(expr);
    context()->Plug(v0);
  }
}


void FullCodeGenerator::CallIC(Handle<Code> code,
                               TypeFeedbackId id) {
  ic_total_count_++;
  __ Call(code, RelocInfo::CODE_TARGET, id);
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
    __ lw(LoadIC::ReceiverRegister(), MemOperand(sp, 0));
    EmitNamedPropertyLoad(callee->AsProperty());
    PrepareForBailoutForId(callee->AsProperty()->LoadId(), TOS_REG);
    // Push the target function under the receiver.
    __ lw(at, MemOperand(sp, 0));
    __ push(at);
    __ sw(v0, MemOperand(sp, kPointerSize));
  }

  EmitCall(expr, call_type);
}


// Code common for calls using the IC.
void FullCodeGenerator::EmitKeyedCallWithLoadIC(Call* expr,
                                                Expression* key) {
  // Load the key.
  VisitForAccumulatorValue(key);

  Expression* callee = expr->expression();

  // Load the function from the receiver.
  DCHECK(callee->IsProperty());
  __ lw(LoadIC::ReceiverRegister(), MemOperand(sp, 0));
  __ Move(LoadIC::NameRegister(), v0);
  EmitKeyedPropertyLoad(callee->AsProperty());
  PrepareForBailoutForId(callee->AsProperty()->LoadId(), TOS_REG);

  // Push the target function under the receiver.
  __ lw(at, MemOperand(sp, 0));
  __ push(at);
  __ sw(v0, MemOperand(sp, kPointerSize));

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
  __ li(a3, Operand(Smi::FromInt(expr->CallFeedbackSlot())));
  __ lw(a1, MemOperand(sp, (arg_count + 1) * kPointerSize));
  // Don't assign a type feedback id to the IC, since type feedback is provided
  // by the vector above.
  CallIC(ic);

  RecordJSReturnSite(expr);
  // Restore context register.
  __ lw(cp, MemOperand(fp, StandardFrameConstants::kContextOffset));
  context()->DropAndPlug(1, v0);
}


void FullCodeGenerator::EmitResolvePossiblyDirectEval(int arg_count) {
  // t2: copy of the first argument or undefined if it doesn't exist.
  if (arg_count > 0) {
    __ lw(t2, MemOperand(sp, arg_count * kPointerSize));
  } else {
    __ LoadRoot(t2, Heap::kUndefinedValueRootIndex);
  }

  // t1: the receiver of the enclosing function.
  int receiver_offset = 2 + info_->scope()->num_parameters();
  __ lw(t1, MemOperand(fp, receiver_offset * kPointerSize));

  // t0: the strict mode.
  __ li(t0, Operand(Smi::FromInt(strict_mode())));

  // a1: the start position of the scope the calls resides in.
  __ li(a1, Operand(Smi::FromInt(scope()->start_position())));

  // Do the runtime call.
  __ Push(t2, t1, t0, a1);
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
    // to resolve the function we need to call and the receiver of the
    // call.  Then we call the resolved function using the given
    // arguments.
    ZoneList<Expression*>* args = expr->arguments();
    int arg_count = args->length();

    { PreservePositionScope pos_scope(masm()->positions_recorder());
      VisitForStackValue(callee);
      __ LoadRoot(a2, Heap::kUndefinedValueRootIndex);
      __ push(a2);  // Reserved receiver slot.

      // Push the arguments.
      for (int i = 0; i < arg_count; i++) {
        VisitForStackValue(args->at(i));
      }

      // Push a copy of the function (found below the arguments) and
      // resolve eval.
      __ lw(a1, MemOperand(sp, (arg_count + 1) * kPointerSize));
      __ push(a1);
      EmitResolvePossiblyDirectEval(arg_count);

      // The runtime call returns a pair of values in v0 (function) and
      // v1 (receiver). Touch up the stack with the right values.
      __ sw(v0, MemOperand(sp, (arg_count + 1) * kPointerSize));
      __ sw(v1, MemOperand(sp, arg_count * kPointerSize));
    }
    // Record source position for debugger.
    SetSourcePosition(expr->position());
    CallFunctionStub stub(isolate(), arg_count, NO_CALL_FUNCTION_FLAGS);
    __ lw(a1, MemOperand(sp, (arg_count + 1) * kPointerSize));
    __ CallStub(&stub);
    RecordJSReturnSite(expr);
    // Restore context register.
    __ lw(cp, MemOperand(fp, StandardFrameConstants::kContextOffset));
    context()->DropAndPlug(1, v0);
  } else if (call_type == Call::GLOBAL_CALL) {
    EmitCallWithLoadIC(expr);
  } else if (call_type == Call::LOOKUP_SLOT_CALL) {
    // Call to a lookup slot (dynamically introduced variable).
    VariableProxy* proxy = callee->AsVariableProxy();
    Label slow, done;

    { PreservePositionScope scope(masm()->positions_recorder());
      // Generate code for loading from variables potentially shadowed
      // by eval-introduced variables.
      EmitDynamicLookupFastCase(proxy, NOT_INSIDE_TYPEOF, &slow, &done);
    }

    __ bind(&slow);
    // Call the runtime to find the function to call (returned in v0)
    // and the object holding it (returned in v1).
    DCHECK(!context_register().is(a2));
    __ li(a2, Operand(proxy->name()));
    __ Push(context_register(), a2);
    __ CallRuntime(Runtime::kLoadLookupSlot, 2);
    __ Push(v0, v1);  // Function, receiver.

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

    // The receiver is either the global receiver or an object found
    // by LoadContextSlot.
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
    __ LoadRoot(a1, Heap::kUndefinedValueRootIndex);
    __ push(a1);
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

  // Load function and argument count into a1 and a0.
  __ li(a0, Operand(arg_count));
  __ lw(a1, MemOperand(sp, arg_count * kPointerSize));

  // Record call targets in unoptimized code.
  if (FLAG_pretenuring_call_new) {
    EnsureSlotContainsAllocationSite(expr->AllocationSiteFeedbackSlot());
    DCHECK(expr->AllocationSiteFeedbackSlot() ==
           expr->CallNewFeedbackSlot() + 1);
  }

  __ li(a2, FeedbackVector());
  __ li(a3, Operand(Smi::FromInt(expr->CallNewFeedbackSlot())));

  CallConstructStub stub(isolate(), RECORD_CONSTRUCTOR_TARGET);
  __ Call(stub.GetCode(), RelocInfo::CONSTRUCT_CALL);
  PrepareForBailoutForId(expr->ReturnId(), TOS_REG);
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
  __ NonNegativeSmiTst(v0, at);
  Split(eq, at, Operand(zero_reg), if_true, if_false, fall_through);

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

  __ JumpIfSmi(v0, if_false);
  __ LoadRoot(at, Heap::kNullValueRootIndex);
  __ Branch(if_true, eq, v0, Operand(at));
  __ lw(a2, FieldMemOperand(v0, HeapObject::kMapOffset));
  // Undetectable objects behave like undefined when tested with typeof.
  __ lbu(a1, FieldMemOperand(a2, Map::kBitFieldOffset));
  __ And(at, a1, Operand(1 << Map::kIsUndetectable));
  __ Branch(if_false, ne, at, Operand(zero_reg));
  __ lbu(a1, FieldMemOperand(a2, Map::kInstanceTypeOffset));
  __ Branch(if_false, lt, a1, Operand(FIRST_NONCALLABLE_SPEC_OBJECT_TYPE));
  PrepareForBailoutBeforeSplit(expr, true, if_true, if_false);
  Split(le, a1, Operand(LAST_NONCALLABLE_SPEC_OBJECT_TYPE),
        if_true, if_false, fall_through);

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

  __ JumpIfSmi(v0, if_false);
  __ GetObjectType(v0, a1, a1);
  PrepareForBailoutBeforeSplit(expr, true, if_true, if_false);
  Split(ge, a1, Operand(FIRST_SPEC_OBJECT_TYPE),
        if_true, if_false, fall_through);

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

  __ JumpIfSmi(v0, if_false);
  __ lw(a1, FieldMemOperand(v0, HeapObject::kMapOffset));
  __ lbu(a1, FieldMemOperand(a1, Map::kBitFieldOffset));
  PrepareForBailoutBeforeSplit(expr, true, if_true, if_false);
  __ And(at, a1, Operand(1 << Map::kIsUndetectable));
  Split(ne, at, Operand(zero_reg), if_true, if_false, fall_through);

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

  __ AssertNotSmi(v0);

  __ lw(a1, FieldMemOperand(v0, HeapObject::kMapOffset));
  __ lbu(t0, FieldMemOperand(a1, Map::kBitField2Offset));
  __ And(t0, t0, 1 << Map::kStringWrapperSafeForDefaultValueOf);
  __ Branch(&skip_lookup, ne, t0, Operand(zero_reg));

  // Check for fast case object. Generate false result for slow case object.
  __ lw(a2, FieldMemOperand(v0, JSObject::kPropertiesOffset));
  __ lw(a2, FieldMemOperand(a2, HeapObject::kMapOffset));
  __ LoadRoot(t0, Heap::kHashTableMapRootIndex);
  __ Branch(if_false, eq, a2, Operand(t0));

  // Look for valueOf name in the descriptor array, and indicate false if
  // found. Since we omit an enumeration index check, if it is added via a
  // transition that shares its descriptor array, this is a false positive.
  Label entry, loop, done;

  // Skip loop if no descriptors are valid.
  __ NumberOfOwnDescriptors(a3, a1);
  __ Branch(&done, eq, a3, Operand(zero_reg));

  __ LoadInstanceDescriptors(a1, t0);
  // t0: descriptor array.
  // a3: valid entries in the descriptor array.
  STATIC_ASSERT(kSmiTag == 0);
  STATIC_ASSERT(kSmiTagSize == 1);
  STATIC_ASSERT(kPointerSize == 4);
  __ li(at, Operand(DescriptorArray::kDescriptorSize));
  __ Mul(a3, a3, at);
  // Calculate location of the first key name.
  __ Addu(t0, t0, Operand(DescriptorArray::kFirstOffset - kHeapObjectTag));
  // Calculate the end of the descriptor array.
  __ mov(a2, t0);
  __ sll(t1, a3, kPointerSizeLog2);
  __ Addu(a2, a2, t1);

  // Loop through all the keys in the descriptor array. If one of these is the
  // string "valueOf" the result is false.
  // The use of t2 to store the valueOf string assumes that it is not otherwise
  // used in the loop below.
  __ li(t2, Operand(isolate()->factory()->value_of_string()));
  __ jmp(&entry);
  __ bind(&loop);
  __ lw(a3, MemOperand(t0, 0));
  __ Branch(if_false, eq, a3, Operand(t2));
  __ Addu(t0, t0, Operand(DescriptorArray::kDescriptorSize * kPointerSize));
  __ bind(&entry);
  __ Branch(&loop, ne, t0, Operand(a2));

  __ bind(&done);

  // Set the bit in the map to indicate that there is no local valueOf field.
  __ lbu(a2, FieldMemOperand(a1, Map::kBitField2Offset));
  __ Or(a2, a2, Operand(1 << Map::kStringWrapperSafeForDefaultValueOf));
  __ sb(a2, FieldMemOperand(a1, Map::kBitField2Offset));

  __ bind(&skip_lookup);

  // If a valueOf property is not found on the object check that its
  // prototype is the un-modified String prototype. If not result is false.
  __ lw(a2, FieldMemOperand(a1, Map::kPrototypeOffset));
  __ JumpIfSmi(a2, if_false);
  __ lw(a2, FieldMemOperand(a2, HeapObject::kMapOffset));
  __ lw(a3, ContextOperand(cp, Context::GLOBAL_OBJECT_INDEX));
  __ lw(a3, FieldMemOperand(a3, GlobalObject::kNativeContextOffset));
  __ lw(a3, ContextOperand(a3, Context::STRING_FUNCTION_PROTOTYPE_MAP_INDEX));
  PrepareForBailoutBeforeSplit(expr, true, if_true, if_false);
  Split(eq, a2, Operand(a3), if_true, if_false, fall_through);

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

  __ JumpIfSmi(v0, if_false);
  __ GetObjectType(v0, a1, a2);
  PrepareForBailoutBeforeSplit(expr, true, if_true, if_false);
  __ Branch(if_true, eq, a2, Operand(JS_FUNCTION_TYPE));
  __ Branch(if_false);

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

  __ CheckMap(v0, a1, Heap::kHeapNumberMapRootIndex, if_false, DO_SMI_CHECK);
  __ lw(a2, FieldMemOperand(v0, HeapNumber::kExponentOffset));
  __ lw(a1, FieldMemOperand(v0, HeapNumber::kMantissaOffset));
  __ li(t0, 0x80000000);
  Label not_nan;
  __ Branch(&not_nan, ne, a2, Operand(t0));
  __ mov(t0, zero_reg);
  __ mov(a2, a1);
  __ bind(&not_nan);

  PrepareForBailoutBeforeSplit(expr, true, if_true, if_false);
  Split(eq, a2, Operand(t0), if_true, if_false, fall_through);

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


void FullCodeGenerator::EmitIsConstructCall(CallRuntime* expr) {
  DCHECK(expr->arguments()->length() == 0);

  Label materialize_true, materialize_false;
  Label* if_true = NULL;
  Label* if_false = NULL;
  Label* fall_through = NULL;
  context()->PrepareTest(&materialize_true, &materialize_false,
                         &if_true, &if_false, &fall_through);

  // Get the frame pointer for the calling frame.
  __ lw(a2, MemOperand(fp, StandardFrameConstants::kCallerFPOffset));

  // Skip the arguments adaptor frame if it exists.
  Label check_frame_marker;
  __ lw(a1, MemOperand(a2, StandardFrameConstants::kContextOffset));
  __ Branch(&check_frame_marker, ne,
            a1, Operand(Smi::FromInt(StackFrame::ARGUMENTS_ADAPTOR)));
  __ lw(a2, MemOperand(a2, StandardFrameConstants::kCallerFPOffset));

  // Check the marker in the calling frame.
  __ bind(&check_frame_marker);
  __ lw(a1, MemOperand(a2, StandardFrameConstants::kMarkerOffset));
  PrepareForBailoutBeforeSplit(expr, true, if_true, if_false);
  Split(eq, a1, Operand(Smi::FromInt(StackFrame::CONSTRUCT)),
        if_true, if_false, fall_through);

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

  __ pop(a1);
  PrepareForBailoutBeforeSplit(expr, true, if_true, if_false);
  Split(eq, v0, Operand(a1), if_true, if_false, fall_through);

  context()->Plug(if_true, if_false);
}


void FullCodeGenerator::EmitArguments(CallRuntime* expr) {
  ZoneList<Expression*>* args = expr->arguments();
  DCHECK(args->length() == 1);

  // ArgumentsAccessStub expects the key in a1 and the formal
  // parameter count in a0.
  VisitForAccumulatorValue(args->at(0));
  __ mov(a1, v0);
  __ li(a0, Operand(Smi::FromInt(info_->scope()->num_parameters())));
  ArgumentsAccessStub stub(isolate(), ArgumentsAccessStub::READ_ELEMENT);
  __ CallStub(&stub);
  context()->Plug(v0);
}


void FullCodeGenerator::EmitArgumentsLength(CallRuntime* expr) {
  DCHECK(expr->arguments()->length() == 0);
  Label exit;
  // Get the number of formal parameters.
  __ li(v0, Operand(Smi::FromInt(info_->scope()->num_parameters())));

  // Check if the calling frame is an arguments adaptor frame.
  __ lw(a2, MemOperand(fp, StandardFrameConstants::kCallerFPOffset));
  __ lw(a3, MemOperand(a2, StandardFrameConstants::kContextOffset));
  __ Branch(&exit, ne, a3,
            Operand(Smi::FromInt(StackFrame::ARGUMENTS_ADAPTOR)));

  // Arguments adaptor case: Read the arguments length from the
  // adaptor frame.
  __ lw(v0, MemOperand(a2, ArgumentsAdaptorFrameConstants::kLengthOffset));

  __ bind(&exit);
  context()->Plug(v0);
}


void FullCodeGenerator::EmitClassOf(CallRuntime* expr) {
  ZoneList<Expression*>* args = expr->arguments();
  DCHECK(args->length() == 1);
  Label done, null, function, non_function_constructor;

  VisitForAccumulatorValue(args->at(0));

  // If the object is a smi, we return null.
  __ JumpIfSmi(v0, &null);

  // Check that the object is a JS object but take special care of JS
  // functions to make sure they have 'Function' as their class.
  // Assume that there are only two callable types, and one of them is at
  // either end of the type range for JS object types. Saves extra comparisons.
  STATIC_ASSERT(NUM_OF_CALLABLE_SPEC_OBJECT_TYPES == 2);
  __ GetObjectType(v0, v0, a1);  // Map is now in v0.
  __ Branch(&null, lt, a1, Operand(FIRST_SPEC_OBJECT_TYPE));

  STATIC_ASSERT(FIRST_NONCALLABLE_SPEC_OBJECT_TYPE ==
                FIRST_SPEC_OBJECT_TYPE + 1);
  __ Branch(&function, eq, a1, Operand(FIRST_SPEC_OBJECT_TYPE));

  STATIC_ASSERT(LAST_NONCALLABLE_SPEC_OBJECT_TYPE ==
                LAST_SPEC_OBJECT_TYPE - 1);
  __ Branch(&function, eq, a1, Operand(LAST_SPEC_OBJECT_TYPE));
  // Assume that there is no larger type.
  STATIC_ASSERT(LAST_NONCALLABLE_SPEC_OBJECT_TYPE == LAST_TYPE - 1);

  // Check if the constructor in the map is a JS function.
  __ lw(v0, FieldMemOperand(v0, Map::kConstructorOffset));
  __ GetObjectType(v0, a1, a1);
  __ Branch(&non_function_constructor, ne, a1, Operand(JS_FUNCTION_TYPE));

  // v0 now contains the constructor function. Grab the
  // instance class name from there.
  __ lw(v0, FieldMemOperand(v0, JSFunction::kSharedFunctionInfoOffset));
  __ lw(v0, FieldMemOperand(v0, SharedFunctionInfo::kInstanceClassNameOffset));
  __ Branch(&done);

  // Functions have class 'Function'.
  __ bind(&function);
  __ LoadRoot(v0, Heap::kfunction_class_stringRootIndex);
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


void FullCodeGenerator::EmitSubString(CallRuntime* expr) {
  // Load the arguments on the stack and call the stub.
  SubStringStub stub(isolate());
  ZoneList<Expression*>* args = expr->arguments();
  DCHECK(args->length() == 3);
  VisitForStackValue(args->at(0));
  VisitForStackValue(args->at(1));
  VisitForStackValue(args->at(2));
  __ CallStub(&stub);
  context()->Plug(v0);
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
  context()->Plug(v0);
}


void FullCodeGenerator::EmitValueOf(CallRuntime* expr) {
  ZoneList<Expression*>* args = expr->arguments();
  DCHECK(args->length() == 1);

  VisitForAccumulatorValue(args->at(0));  // Load the object.

  Label done;
  // If the object is a smi return the object.
  __ JumpIfSmi(v0, &done);
  // If the object is not a value type, return the object.
  __ GetObjectType(v0, a1, a1);
  __ Branch(&done, ne, a1, Operand(JS_VALUE_TYPE));

  __ lw(v0, FieldMemOperand(v0, JSValue::kValueOffset));

  __ bind(&done);
  context()->Plug(v0);
}


void FullCodeGenerator::EmitDateField(CallRuntime* expr) {
  ZoneList<Expression*>* args = expr->arguments();
  DCHECK(args->length() == 2);
  DCHECK_NE(NULL, args->at(1)->AsLiteral());
  Smi* index = Smi::cast(*(args->at(1)->AsLiteral()->value()));

  VisitForAccumulatorValue(args->at(0));  // Load the object.

  Label runtime, done, not_date_object;
  Register object = v0;
  Register result = v0;
  Register scratch0 = t5;
  Register scratch1 = a1;

  __ JumpIfSmi(object, &not_date_object);
  __ GetObjectType(object, scratch1, scratch1);
  __ Branch(&not_date_object, ne, scratch1, Operand(JS_DATE_TYPE));

  if (index->value() == 0) {
    __ lw(result, FieldMemOperand(object, JSDate::kValueOffset));
    __ jmp(&done);
  } else {
    if (index->value() < JSDate::kFirstUncachedField) {
      ExternalReference stamp = ExternalReference::date_cache_stamp(isolate());
      __ li(scratch1, Operand(stamp));
      __ lw(scratch1, MemOperand(scratch1));
      __ lw(scratch0, FieldMemOperand(object, JSDate::kCacheStampOffset));
      __ Branch(&runtime, ne, scratch1, Operand(scratch0));
      __ lw(result, FieldMemOperand(object, JSDate::kValueOffset +
                                            kPointerSize * index->value()));
      __ jmp(&done);
    }
    __ bind(&runtime);
    __ PrepareCallCFunction(2, scratch1);
    __ li(a1, Operand(index));
    __ Move(a0, object);
    __ CallCFunction(ExternalReference::get_date_field_function(isolate()), 2);
    __ jmp(&done);
  }

  __ bind(&not_date_object);
  __ CallRuntime(Runtime::kThrowNotDateError, 0);
  __ bind(&done);
  context()->Plug(v0);
}


void FullCodeGenerator::EmitOneByteSeqStringSetChar(CallRuntime* expr) {
  ZoneList<Expression*>* args = expr->arguments();
  DCHECK_EQ(3, args->length());

  Register string = v0;
  Register index = a1;
  Register value = a2;

  VisitForStackValue(args->at(1));  // index
  VisitForStackValue(args->at(2));  // value
  VisitForAccumulatorValue(args->at(0));  // string
  __ Pop(index, value);

  if (FLAG_debug_code) {
    __ SmiTst(value, at);
    __ Check(eq, kNonSmiValue, at, Operand(zero_reg));
    __ SmiTst(index, at);
    __ Check(eq, kNonSmiIndex, at, Operand(zero_reg));
    __ SmiUntag(index, index);
    static const uint32_t one_byte_seq_type = kSeqStringTag | kOneByteStringTag;
    Register scratch = t5;
    __ EmitSeqStringSetCharCheck(
        string, index, value, scratch, one_byte_seq_type);
    __ SmiTag(index, index);
  }

  __ SmiUntag(value, value);
  __ Addu(at,
          string,
          Operand(SeqOneByteString::kHeaderSize - kHeapObjectTag));
  __ SmiUntag(index);
  __ Addu(at, at, index);
  __ sb(value, MemOperand(at));
  context()->Plug(string);
}


void FullCodeGenerator::EmitTwoByteSeqStringSetChar(CallRuntime* expr) {
  ZoneList<Expression*>* args = expr->arguments();
  DCHECK_EQ(3, args->length());

  Register string = v0;
  Register index = a1;
  Register value = a2;

  VisitForStackValue(args->at(1));  // index
  VisitForStackValue(args->at(2));  // value
  VisitForAccumulatorValue(args->at(0));  // string
  __ Pop(index, value);

  if (FLAG_debug_code) {
    __ SmiTst(value, at);
    __ Check(eq, kNonSmiValue, at, Operand(zero_reg));
    __ SmiTst(index, at);
    __ Check(eq, kNonSmiIndex, at, Operand(zero_reg));
    __ SmiUntag(index, index);
    static const uint32_t two_byte_seq_type = kSeqStringTag | kTwoByteStringTag;
    Register scratch = t5;
    __ EmitSeqStringSetCharCheck(
        string, index, value, scratch, two_byte_seq_type);
    __ SmiTag(index, index);
  }

  __ SmiUntag(value, value);
  __ Addu(at,
          string,
          Operand(SeqTwoByteString::kHeaderSize - kHeapObjectTag));
  __ Addu(at, at, index);
  STATIC_ASSERT(kSmiTagSize == 1 && kSmiTag == 0);
  __ sh(value, MemOperand(at));
    context()->Plug(string);
}


void FullCodeGenerator::EmitMathPow(CallRuntime* expr) {
  // Load the arguments on the stack and call the runtime function.
  ZoneList<Expression*>* args = expr->arguments();
  DCHECK(args->length() == 2);
  VisitForStackValue(args->at(0));
  VisitForStackValue(args->at(1));
  MathPowStub stub(isolate(), MathPowStub::ON_STACK);
  __ CallStub(&stub);
  context()->Plug(v0);
}


void FullCodeGenerator::EmitSetValueOf(CallRuntime* expr) {
  ZoneList<Expression*>* args = expr->arguments();
  DCHECK(args->length() == 2);

  VisitForStackValue(args->at(0));  // Load the object.
  VisitForAccumulatorValue(args->at(1));  // Load the value.
  __ pop(a1);  // v0 = value. a1 = object.

  Label done;
  // If the object is a smi, return the value.
  __ JumpIfSmi(a1, &done);

  // If the object is not a value type, return the value.
  __ GetObjectType(a1, a2, a2);
  __ Branch(&done, ne, a2, Operand(JS_VALUE_TYPE));

  // Store the value.
  __ sw(v0, FieldMemOperand(a1, JSValue::kValueOffset));
  // Update the write barrier.  Save the value as it will be
  // overwritten by the write barrier code and is needed afterward.
  __ mov(a2, v0);
  __ RecordWriteField(
      a1, JSValue::kValueOffset, a2, a3, kRAHasBeenSaved, kDontSaveFPRegs);

  __ bind(&done);
  context()->Plug(v0);
}


void FullCodeGenerator::EmitNumberToString(CallRuntime* expr) {
  ZoneList<Expression*>* args = expr->arguments();
  DCHECK_EQ(args->length(), 1);

  // Load the argument into a0 and call the stub.
  VisitForAccumulatorValue(args->at(0));
  __ mov(a0, result_register());

  NumberToStringStub stub(isolate());
  __ CallStub(&stub);
  context()->Plug(v0);
}


void FullCodeGenerator::EmitStringCharFromCode(CallRuntime* expr) {
  ZoneList<Expression*>* args = expr->arguments();
  DCHECK(args->length() == 1);

  VisitForAccumulatorValue(args->at(0));

  Label done;
  StringCharFromCodeGenerator generator(v0, a1);
  generator.GenerateFast(masm_);
  __ jmp(&done);

  NopRuntimeCallHelper call_helper;
  generator.GenerateSlow(masm_, call_helper);

  __ bind(&done);
  context()->Plug(a1);
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
  DCHECK(args->length() == 2);

  VisitForStackValue(args->at(0));
  VisitForAccumulatorValue(args->at(1));
  __ mov(a0, result_register());

  Register object = a1;
  Register index = a0;
  Register scratch = a3;
  Register result = v0;

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
  __ li(result, Operand(Smi::FromInt(0)));
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

  __ pop(a1);
  __ mov(a0, result_register());  // StringAddStub requires args in a0, a1.
  StringAddStub stub(isolate(), STRING_ADD_CHECK_BOTH, NOT_TENURED);
  __ CallStub(&stub);
  context()->Plug(v0);
}


void FullCodeGenerator::EmitStringCompare(CallRuntime* expr) {
  ZoneList<Expression*>* args = expr->arguments();
  DCHECK_EQ(2, args->length());

  VisitForStackValue(args->at(0));
  VisitForStackValue(args->at(1));

  StringCompareStub stub(isolate());
  __ CallStub(&stub);
  context()->Plug(v0);
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
  __ JumpIfSmi(v0, &runtime);
  __ GetObjectType(v0, a1, a1);
  __ Branch(&runtime, ne, a1, Operand(JS_FUNCTION_TYPE));

  // InvokeFunction requires the function in a1. Move it in there.
  __ mov(a1, result_register());
  ParameterCount count(arg_count);
  __ InvokeFunction(a1, count, CALL_FUNCTION, NullCallWrapper());
  __ lw(cp, MemOperand(fp, StandardFrameConstants::kContextOffset));
  __ jmp(&done);

  __ bind(&runtime);
  __ push(v0);
  __ CallRuntime(Runtime::kCall, args->length());
  __ bind(&done);

  context()->Plug(v0);
}


void FullCodeGenerator::EmitRegExpConstructResult(CallRuntime* expr) {
  RegExpConstructResultStub stub(isolate());
  ZoneList<Expression*>* args = expr->arguments();
  DCHECK(args->length() == 3);
  VisitForStackValue(args->at(0));
  VisitForStackValue(args->at(1));
  VisitForAccumulatorValue(args->at(2));
  __ mov(a0, result_register());
  __ pop(a1);
  __ pop(a2);
  __ CallStub(&stub);
  context()->Plug(v0);
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
    __ LoadRoot(v0, Heap::kUndefinedValueRootIndex);
    context()->Plug(v0);
    return;
  }

  VisitForAccumulatorValue(args->at(1));

  Register key = v0;
  Register cache = a1;
  __ lw(cache, ContextOperand(cp, Context::GLOBAL_OBJECT_INDEX));
  __ lw(cache, FieldMemOperand(cache, GlobalObject::kNativeContextOffset));
  __ lw(cache,
         ContextOperand(
             cache, Context::JSFUNCTION_RESULT_CACHES_INDEX));
  __ lw(cache,
         FieldMemOperand(cache, FixedArray::OffsetOfElementAt(cache_id)));


  Label done, not_found;
  STATIC_ASSERT(kSmiTag == 0 && kSmiTagSize == 1);
  __ lw(a2, FieldMemOperand(cache, JSFunctionResultCache::kFingerOffset));
  // a2 now holds finger offset as a smi.
  __ Addu(a3, cache, Operand(FixedArray::kHeaderSize - kHeapObjectTag));
  // a3 now points to the start of fixed array elements.
  __ sll(at, a2, kPointerSizeLog2 - kSmiTagSize);
  __ addu(a3, a3, at);
  // a3 now points to key of indexed element of cache.
  __ lw(a2, MemOperand(a3));
  __ Branch(&not_found, ne, key, Operand(a2));

  __ lw(v0, MemOperand(a3, kPointerSize));
  __ Branch(&done);

  __ bind(&not_found);
  // Call runtime to perform the lookup.
  __ Push(cache, key);
  __ CallRuntime(Runtime::kGetFromCache, 2);

  __ bind(&done);
  context()->Plug(v0);
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


void FullCodeGenerator::EmitFastAsciiArrayJoin(CallRuntime* expr) {
  Label bailout, done, one_char_separator, long_separator,
      non_trivial_array, not_size_one_array, loop,
      empty_separator_loop, one_char_separator_loop,
      one_char_separator_loop_entry, long_separator_loop;
  ZoneList<Expression*>* args = expr->arguments();
  DCHECK(args->length() == 2);
  VisitForStackValue(args->at(1));
  VisitForAccumulatorValue(args->at(0));

  // All aliases of the same register have disjoint lifetimes.
  Register array = v0;
  Register elements = no_reg;  // Will be v0.
  Register result = no_reg;  // Will be v0.
  Register separator = a1;
  Register array_length = a2;
  Register result_pos = no_reg;  // Will be a2.
  Register string_length = a3;
  Register string = t0;
  Register element = t1;
  Register elements_end = t2;
  Register scratch1 = t3;
  Register scratch2 = t5;
  Register scratch3 = t4;

  // Separator operand is on the stack.
  __ pop(separator);

  // Check that the array is a JSArray.
  __ JumpIfSmi(array, &bailout);
  __ GetObjectType(array, scratch1, scratch2);
  __ Branch(&bailout, ne, scratch2, Operand(JS_ARRAY_TYPE));

  // Check that the array has fast elements.
  __ CheckFastElements(scratch1, scratch2, &bailout);

  // If the array has length zero, return the empty string.
  __ lw(array_length, FieldMemOperand(array, JSArray::kLengthOffset));
  __ SmiUntag(array_length);
  __ Branch(&non_trivial_array, ne, array_length, Operand(zero_reg));
  __ LoadRoot(v0, Heap::kempty_stringRootIndex);
  __ Branch(&done);

  __ bind(&non_trivial_array);

  // Get the FixedArray containing array's elements.
  elements = array;
  __ lw(elements, FieldMemOperand(array, JSArray::kElementsOffset));
  array = no_reg;  // End of array's live range.

  // Check that all array elements are sequential ASCII strings, and
  // accumulate the sum of their lengths, as a smi-encoded value.
  __ mov(string_length, zero_reg);
  __ Addu(element,
          elements, Operand(FixedArray::kHeaderSize - kHeapObjectTag));
  __ sll(elements_end, array_length, kPointerSizeLog2);
  __ Addu(elements_end, element, elements_end);
  // Loop condition: while (element < elements_end).
  // Live values in registers:
  //   elements: Fixed array of strings.
  //   array_length: Length of the fixed array of strings (not smi)
  //   separator: Separator string
  //   string_length: Accumulated sum of string lengths (smi).
  //   element: Current array element.
  //   elements_end: Array end.
  if (generate_debug_code_) {
    __ Assert(gt, kNoEmptyArraysHereInEmitFastAsciiArrayJoin,
        array_length, Operand(zero_reg));
  }
  __ bind(&loop);
  __ lw(string, MemOperand(element));
  __ Addu(element, element, kPointerSize);
  __ JumpIfSmi(string, &bailout);
  __ lw(scratch1, FieldMemOperand(string, HeapObject::kMapOffset));
  __ lbu(scratch1, FieldMemOperand(scratch1, Map::kInstanceTypeOffset));
  __ JumpIfInstanceTypeIsNotSequentialAscii(scratch1, scratch2, &bailout);
  __ lw(scratch1, FieldMemOperand(string, SeqOneByteString::kLengthOffset));
  __ AdduAndCheckForOverflow(string_length, string_length, scratch1, scratch3);
  __ BranchOnOverflow(&bailout, scratch3);
  __ Branch(&loop, lt, element, Operand(elements_end));

  // If array_length is 1, return elements[0], a string.
  __ Branch(&not_size_one_array, ne, array_length, Operand(1));
  __ lw(v0, FieldMemOperand(elements, FixedArray::kHeaderSize));
  __ Branch(&done);

  __ bind(&not_size_one_array);

  // Live values in registers:
  //   separator: Separator string
  //   array_length: Length of the array.
  //   string_length: Sum of string lengths (smi).
  //   elements: FixedArray of strings.

  // Check that the separator is a flat ASCII string.
  __ JumpIfSmi(separator, &bailout);
  __ lw(scratch1, FieldMemOperand(separator, HeapObject::kMapOffset));
  __ lbu(scratch1, FieldMemOperand(scratch1, Map::kInstanceTypeOffset));
  __ JumpIfInstanceTypeIsNotSequentialAscii(scratch1, scratch2, &bailout);

  // Add (separator length times array_length) - separator length to the
  // string_length to get the length of the result string. array_length is not
  // smi but the other values are, so the result is a smi.
  __ lw(scratch1, FieldMemOperand(separator, SeqOneByteString::kLengthOffset));
  __ Subu(string_length, string_length, Operand(scratch1));
  __ Mult(array_length, scratch1);
  // Check for smi overflow. No overflow if higher 33 bits of 64-bit result are
  // zero.
  __ mfhi(scratch2);
  __ Branch(&bailout, ne, scratch2, Operand(zero_reg));
  __ mflo(scratch2);
  __ And(scratch3, scratch2, Operand(0x80000000));
  __ Branch(&bailout, ne, scratch3, Operand(zero_reg));
  __ AdduAndCheckForOverflow(string_length, string_length, scratch2, scratch3);
  __ BranchOnOverflow(&bailout, scratch3);
  __ SmiUntag(string_length);

  // Get first element in the array to free up the elements register to be used
  // for the result.
  __ Addu(element,
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
                         scratch1,
                         scratch2,
                         elements_end,
                         &bailout);
  // Prepare for looping. Set up elements_end to end of the array. Set
  // result_pos to the position of the result where to write the first
  // character.
  __ sll(elements_end, array_length, kPointerSizeLog2);
  __ Addu(elements_end, element, elements_end);
  result_pos = array_length;  // End of live range for array_length.
  array_length = no_reg;
  __ Addu(result_pos,
          result,
          Operand(SeqOneByteString::kHeaderSize - kHeapObjectTag));

  // Check the length of the separator.
  __ lw(scratch1, FieldMemOperand(separator, SeqOneByteString::kLengthOffset));
  __ li(at, Operand(Smi::FromInt(1)));
  __ Branch(&one_char_separator, eq, scratch1, Operand(at));
  __ Branch(&long_separator, gt, scratch1, Operand(at));

  // Empty separator case.
  __ bind(&empty_separator_loop);
  // Live values in registers:
  //   result_pos: the position to which we are currently copying characters.
  //   element: Current array element.
  //   elements_end: Array end.

  // Copy next array element to the result.
  __ lw(string, MemOperand(element));
  __ Addu(element, element, kPointerSize);
  __ lw(string_length, FieldMemOperand(string, String::kLengthOffset));
  __ SmiUntag(string_length);
  __ Addu(string, string, SeqOneByteString::kHeaderSize - kHeapObjectTag);
  __ CopyBytes(string, result_pos, string_length, scratch1);
  // End while (element < elements_end).
  __ Branch(&empty_separator_loop, lt, element, Operand(elements_end));
  DCHECK(result.is(v0));
  __ Branch(&done);

  // One-character separator case.
  __ bind(&one_char_separator);
  // Replace separator with its ASCII character value.
  __ lbu(separator, FieldMemOperand(separator, SeqOneByteString::kHeaderSize));
  // Jump into the loop after the code that copies the separator, so the first
  // element is not preceded by a separator.
  __ jmp(&one_char_separator_loop_entry);

  __ bind(&one_char_separator_loop);
  // Live values in registers:
  //   result_pos: the position to which we are currently copying characters.
  //   element: Current array element.
  //   elements_end: Array end.
  //   separator: Single separator ASCII char (in lower byte).

  // Copy the separator character to the result.
  __ sb(separator, MemOperand(result_pos));
  __ Addu(result_pos, result_pos, 1);

  // Copy next array element to the result.
  __ bind(&one_char_separator_loop_entry);
  __ lw(string, MemOperand(element));
  __ Addu(element, element, kPointerSize);
  __ lw(string_length, FieldMemOperand(string, String::kLengthOffset));
  __ SmiUntag(string_length);
  __ Addu(string, string, SeqOneByteString::kHeaderSize - kHeapObjectTag);
  __ CopyBytes(string, result_pos, string_length, scratch1);
  // End while (element < elements_end).
  __ Branch(&one_char_separator_loop, lt, element, Operand(elements_end));
  DCHECK(result.is(v0));
  __ Branch(&done);

  // Long separator case (separator is more than one character). Entry is at the
  // label long_separator below.
  __ bind(&long_separator_loop);
  // Live values in registers:
  //   result_pos: the position to which we are currently copying characters.
  //   element: Current array element.
  //   elements_end: Array end.
  //   separator: Separator string.

  // Copy the separator to the result.
  __ lw(string_length, FieldMemOperand(separator, String::kLengthOffset));
  __ SmiUntag(string_length);
  __ Addu(string,
          separator,
          Operand(SeqOneByteString::kHeaderSize - kHeapObjectTag));
  __ CopyBytes(string, result_pos, string_length, scratch1);

  __ bind(&long_separator);
  __ lw(string, MemOperand(element));
  __ Addu(element, element, kPointerSize);
  __ lw(string_length, FieldMemOperand(string, String::kLengthOffset));
  __ SmiUntag(string_length);
  __ Addu(string, string, SeqOneByteString::kHeaderSize - kHeapObjectTag);
  __ CopyBytes(string, result_pos, string_length, scratch1);
  // End while (element < elements_end).
  __ Branch(&long_separator_loop, lt, element, Operand(elements_end));
  DCHECK(result.is(v0));
  __ Branch(&done);

  __ bind(&bailout);
  __ LoadRoot(v0, Heap::kUndefinedValueRootIndex);
  __ bind(&done);
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
    Register receiver = LoadIC::ReceiverRegister();
    __ lw(receiver, GlobalObjectOperand());
    __ lw(receiver, FieldMemOperand(receiver, GlobalObject::kBuiltinsOffset));
    __ push(receiver);

    // Load the function from the receiver.
    __ li(LoadIC::NameRegister(), Operand(expr->name()));
    if (FLAG_vector_ics) {
      __ li(LoadIC::SlotRegister(),
            Operand(Smi::FromInt(expr->CallRuntimeFeedbackSlot())));
      CallLoadIC(NOT_CONTEXTUAL);
    } else {
      CallLoadIC(NOT_CONTEXTUAL, expr->CallRuntimeFeedbackId());
    }

    // Push the target function under the receiver.
    __ lw(at, MemOperand(sp, 0));
    __ push(at);
    __ sw(v0, MemOperand(sp, kPointerSize));

    // Push the arguments ("left-to-right").
    int arg_count = args->length();
    for (int i = 0; i < arg_count; i++) {
      VisitForStackValue(args->at(i));
    }

    // Record source position of the IC call.
    SetSourcePosition(expr->position());
    CallFunctionStub stub(isolate(), arg_count, NO_CALL_FUNCTION_FLAGS);
    __ lw(a1, MemOperand(sp, (arg_count + 1) * kPointerSize));
    __ CallStub(&stub);

    // Restore context register.
    __ lw(cp, MemOperand(fp, StandardFrameConstants::kContextOffset));

    context()->DropAndPlug(1, v0);
  } else {
    // Push the arguments ("left-to-right").
    for (int i = 0; i < arg_count; i++) {
      VisitForStackValue(args->at(i));
    }

    // Call the C runtime function.
    __ CallRuntime(expr->function(), arg_count);
    context()->Plug(v0);
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
        __ li(a1, Operand(Smi::FromInt(strict_mode())));
        __ push(a1);
        __ InvokeBuiltin(Builtins::DELETE, CALL_FUNCTION);
        context()->Plug(v0);
      } else if (proxy != NULL) {
        Variable* var = proxy->var();
        // Delete of an unqualified identifier is disallowed in strict mode
        // but "delete this" is allowed.
        DCHECK(strict_mode() == SLOPPY || var->is_this());
        if (var->IsUnallocated()) {
          __ lw(a2, GlobalObjectOperand());
          __ li(a1, Operand(var->name()));
          __ li(a0, Operand(Smi::FromInt(SLOPPY)));
          __ Push(a2, a1, a0);
          __ InvokeBuiltin(Builtins::DELETE, CALL_FUNCTION);
          context()->Plug(v0);
        } else if (var->IsStackAllocated() || var->IsContextSlot()) {
          // Result of deleting non-global, non-dynamic variables is false.
          // The subexpression does not have side effects.
          context()->Plug(var->is_this());
        } else {
          // Non-global variable.  Call the runtime to try to delete from the
          // context where the variable was introduced.
          DCHECK(!context_register().is(a2));
          __ li(a2, Operand(var->name()));
          __ Push(context_register(), a2);
          __ CallRuntime(Runtime::kDeleteLookupSlot, 2);
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
        __ bind(&materialize_true);
        PrepareForBailoutForId(expr->MaterializeTrueId(), NO_REGISTERS);
        __ LoadRoot(v0, Heap::kTrueValueRootIndex);
        if (context()->IsStackValue()) __ push(v0);
        __ jmp(&done);
        __ bind(&materialize_false);
        PrepareForBailoutForId(expr->MaterializeFalseId(), NO_REGISTERS);
        __ LoadRoot(v0, Heap::kFalseValueRootIndex);
        if (context()->IsStackValue()) __ push(v0);
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
      context()->Plug(v0);
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
      __ li(at, Operand(Smi::FromInt(0)));
      __ push(at);
    }
    if (assign_type == NAMED_PROPERTY) {
      // Put the object both on the stack and in the register.
      VisitForStackValue(prop->obj());
      __ lw(LoadIC::ReceiverRegister(), MemOperand(sp, 0));
      EmitNamedPropertyLoad(prop);
    } else {
      VisitForStackValue(prop->obj());
      VisitForStackValue(prop->key());
      __ lw(LoadIC::ReceiverRegister(), MemOperand(sp, 1 * kPointerSize));
      __ lw(LoadIC::NameRegister(), MemOperand(sp, 0));
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
          case KEYED_PROPERTY:
            __ sw(v0, MemOperand(sp, 2 * kPointerSize));
            break;
        }
      }
    }

    Register scratch1 = a1;
    Register scratch2 = t0;
    __ li(scratch1, Operand(Smi::FromInt(count_value)));
    __ AdduAndCheckForOverflow(v0, v0, scratch1, scratch2);
    __ BranchOnNoOverflow(&done, scratch2);
    // Call stub. Undo operation first.
    __ Move(v0, a0);
    __ jmp(&stub_call);
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
          __ push(v0);
          break;
        case NAMED_PROPERTY:
          __ sw(v0, MemOperand(sp, kPointerSize));
          break;
        case KEYED_PROPERTY:
          __ sw(v0, MemOperand(sp, 2 * kPointerSize));
          break;
      }
    }
  }

  __ bind(&stub_call);
  __ mov(a1, v0);
  __ li(a0, Operand(Smi::FromInt(count_value)));

  // Record position before stub call.
  SetSourcePosition(expr->position());

  BinaryOpICStub stub(isolate(), Token::ADD, NO_OVERWRITE);
  CallIC(stub.GetCode(), expr->CountBinOpFeedbackId());
  patch_site.EmitPatchInfo();
  __ bind(&done);

  // Store the value returned in v0.
  switch (assign_type) {
    case VARIABLE:
      if (expr->is_postfix()) {
        { EffectContext context(this);
          EmitVariableAssignment(expr->expression()->AsVariableProxy()->var(),
                                 Token::ASSIGN);
          PrepareForBailoutForId(expr->AssignmentId(), TOS_REG);
          context.Plug(v0);
        }
        // For all contexts except EffectConstant we have the result on
        // top of the stack.
        if (!context()->IsEffect()) {
          context()->PlugTOS();
        }
      } else {
        EmitVariableAssignment(expr->expression()->AsVariableProxy()->var(),
                               Token::ASSIGN);
        PrepareForBailoutForId(expr->AssignmentId(), TOS_REG);
        context()->Plug(v0);
      }
      break;
    case NAMED_PROPERTY: {
      __ mov(StoreIC::ValueRegister(), result_register());
      __ li(StoreIC::NameRegister(),
            Operand(prop->key()->AsLiteral()->value()));
      __ pop(StoreIC::ReceiverRegister());
      CallStoreIC(expr->CountStoreFeedbackId());
      PrepareForBailoutForId(expr->AssignmentId(), TOS_REG);
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
      __ mov(KeyedStoreIC::ValueRegister(), result_register());
      __ Pop(KeyedStoreIC::ReceiverRegister(), KeyedStoreIC::NameRegister());
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
        context()->Plug(v0);
      }
      break;
    }
  }
}


void FullCodeGenerator::VisitForTypeofValue(Expression* expr) {
  DCHECK(!context()->IsEffect());
  DCHECK(!context()->IsTest());
  VariableProxy* proxy = expr->AsVariableProxy();
  if (proxy != NULL && proxy->var()->IsUnallocated()) {
    Comment cmnt(masm_, "[ Global variable");
    __ lw(LoadIC::ReceiverRegister(), GlobalObjectOperand());
    __ li(LoadIC::NameRegister(), Operand(proxy->name()));
    if (FLAG_vector_ics) {
      __ li(LoadIC::SlotRegister(),
            Operand(Smi::FromInt(proxy->VariableFeedbackSlot())));
    }
    // Use a regular load, not a contextual load, to avoid a reference
    // error.
    CallLoadIC(NOT_CONTEXTUAL);
    PrepareForBailout(expr, TOS_REG);
    context()->Plug(v0);
  } else if (proxy != NULL && proxy->var()->IsLookupSlot()) {
    Comment cmnt(masm_, "[ Lookup slot");
    Label done, slow;

    // Generate code for loading from variables potentially shadowed
    // by eval-introduced variables.
    EmitDynamicLookupFastCase(proxy, INSIDE_TYPEOF, &slow, &done);

    __ bind(&slow);
    __ li(a0, Operand(proxy->name()));
    __ Push(cp, a0);
    __ CallRuntime(Runtime::kLoadLookupSlotNoReferenceError, 2);
    PrepareForBailout(expr, TOS_REG);
    __ bind(&done);

    context()->Plug(v0);
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
    __ JumpIfSmi(v0, if_true);
    __ lw(v0, FieldMemOperand(v0, HeapObject::kMapOffset));
    __ LoadRoot(at, Heap::kHeapNumberMapRootIndex);
    Split(eq, v0, Operand(at), if_true, if_false, fall_through);
  } else if (String::Equals(check, factory->string_string())) {
    __ JumpIfSmi(v0, if_false);
    // Check for undetectable objects => false.
    __ GetObjectType(v0, v0, a1);
    __ Branch(if_false, ge, a1, Operand(FIRST_NONSTRING_TYPE));
    __ lbu(a1, FieldMemOperand(v0, Map::kBitFieldOffset));
    __ And(a1, a1, Operand(1 << Map::kIsUndetectable));
    Split(eq, a1, Operand(zero_reg),
          if_true, if_false, fall_through);
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
    __ LoadRoot(at, Heap::kUndefinedValueRootIndex);
    __ Branch(if_true, eq, v0, Operand(at));
    __ JumpIfSmi(v0, if_false);
    // Check for undetectable objects => true.
    __ lw(v0, FieldMemOperand(v0, HeapObject::kMapOffset));
    __ lbu(a1, FieldMemOperand(v0, Map::kBitFieldOffset));
    __ And(a1, a1, Operand(1 << Map::kIsUndetectable));
    Split(ne, a1, Operand(zero_reg), if_true, if_false, fall_through);
  } else if (String::Equals(check, factory->function_string())) {
    __ JumpIfSmi(v0, if_false);
    STATIC_ASSERT(NUM_OF_CALLABLE_SPEC_OBJECT_TYPES == 2);
    __ GetObjectType(v0, v0, a1);
    __ Branch(if_true, eq, a1, Operand(JS_FUNCTION_TYPE));
    Split(eq, a1, Operand(JS_FUNCTION_PROXY_TYPE),
          if_true, if_false, fall_through);
  } else if (String::Equals(check, factory->object_string())) {
    __ JumpIfSmi(v0, if_false);
    __ LoadRoot(at, Heap::kNullValueRootIndex);
    __ Branch(if_true, eq, v0, Operand(at));
    // Check for JS objects => true.
    __ GetObjectType(v0, v0, a1);
    __ Branch(if_false, lt, a1, Operand(FIRST_NONCALLABLE_SPEC_OBJECT_TYPE));
    __ lbu(a1, FieldMemOperand(v0, Map::kInstanceTypeOffset));
    __ Branch(if_false, gt, a1, Operand(LAST_NONCALLABLE_SPEC_OBJECT_TYPE));
    // Check for undetectable objects => false.
    __ lbu(a1, FieldMemOperand(v0, Map::kBitFieldOffset));
    __ And(a1, a1, Operand(1 << Map::kIsUndetectable));
    Split(eq, a1, Operand(zero_reg), if_true, if_false, fall_through);
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
      __ LoadRoot(t0, Heap::kTrueValueRootIndex);
      Split(eq, v0, Operand(t0), if_true, if_false, fall_through);
      break;

    case Token::INSTANCEOF: {
      VisitForStackValue(expr->right());
      InstanceofStub stub(isolate(), InstanceofStub::kNoFlags);
      __ CallStub(&stub);
      PrepareForBailoutBeforeSplit(expr, true, if_true, if_false);
      // The stub returns 0 for true.
      Split(eq, v0, Operand(zero_reg), if_true, if_false, fall_through);
      break;
    }

    default: {
      VisitForAccumulatorValue(expr->right());
      Condition cc = CompareIC::ComputeCondition(op);
      __ mov(a0, result_register());
      __ pop(a1);

      bool inline_smi_code = ShouldInlineSmiCase(op);
      JumpPatchSite patch_site(masm_);
      if (inline_smi_code) {
        Label slow_case;
        __ Or(a2, a0, Operand(a1));
        patch_site.EmitJumpIfNotSmi(a2, &slow_case);
        Split(cc, a1, Operand(a0), if_true, if_false, NULL);
        __ bind(&slow_case);
      }
      // Record position and call the compare IC.
      SetSourcePosition(expr->position());
      Handle<Code> ic = CompareIC::GetUninitialized(isolate(), op);
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
  __ mov(a0, result_register());
  if (expr->op() == Token::EQ_STRICT) {
    Heap::RootListIndex nil_value = nil == kNullValue ?
        Heap::kNullValueRootIndex :
        Heap::kUndefinedValueRootIndex;
    __ LoadRoot(a1, nil_value);
    Split(eq, a0, Operand(a1), if_true, if_false, fall_through);
  } else {
    Handle<Code> ic = CompareNilICStub::GetUninitialized(isolate(), nil);
    CallIC(ic, expr->CompareOperationFeedbackId());
    Split(ne, v0, Operand(zero_reg), if_true, if_false, fall_through);
  }
  context()->Plug(if_true, if_false);
}


void FullCodeGenerator::VisitThisFunction(ThisFunction* expr) {
  __ lw(v0, MemOperand(fp, JavaScriptFrameConstants::kFunctionOffset));
  context()->Plug(v0);
}


Register FullCodeGenerator::result_register() {
  return v0;
}


Register FullCodeGenerator::context_register() {
  return cp;
}


void FullCodeGenerator::StoreToFrameField(int frame_offset, Register value) {
  DCHECK_EQ(POINTER_SIZE_ALIGN(frame_offset), frame_offset);
  __ sw(value, MemOperand(fp, frame_offset));
}


void FullCodeGenerator::LoadContextField(Register dst, int context_index) {
  __ lw(dst, ContextOperand(cp, context_index));
}


void FullCodeGenerator::PushFunctionArgumentForContextAllocation() {
  Scope* declaration_scope = scope()->DeclarationScope();
  if (declaration_scope->is_global_scope() ||
      declaration_scope->is_module_scope()) {
    // Contexts nested in the native context have a canonical empty function
    // as their closure, not the anonymous closure containing the global
    // code.  Pass a smi sentinel and let the runtime look up the empty
    // function.
    __ li(at, Operand(Smi::FromInt(0)));
  } else if (declaration_scope->is_eval_scope()) {
    // Contexts created by a call to eval have the same closure as the
    // context calling eval, not the anonymous closure containing the eval
    // code.  Fetch it from the context.
    __ lw(at, ContextOperand(cp, Context::CLOSURE_INDEX));
  } else {
    DCHECK(declaration_scope->is_function_scope());
    __ lw(at, MemOperand(fp, JavaScriptFrameConstants::kFunctionOffset));
  }
  __ push(at);
}


// ----------------------------------------------------------------------------
// Non-local control flow support.

void FullCodeGenerator::EnterFinallyBlock() {
  DCHECK(!result_register().is(a1));
  // Store result register while executing finally block.
  __ push(result_register());
  // Cook return address in link register to stack (smi encoded Code* delta).
  __ Subu(a1, ra, Operand(masm_->CodeObject()));
  DCHECK_EQ(1, kSmiTagSize + kSmiShiftSize);
  STATIC_ASSERT(0 == kSmiTag);
  __ Addu(a1, a1, Operand(a1));  // Convert to smi.

  // Store result register while executing finally block.
  __ push(a1);

  // Store pending message while executing finally block.
  ExternalReference pending_message_obj =
      ExternalReference::address_of_pending_message_obj(isolate());
  __ li(at, Operand(pending_message_obj));
  __ lw(a1, MemOperand(at));
  __ push(a1);

  ExternalReference has_pending_message =
      ExternalReference::address_of_has_pending_message(isolate());
  __ li(at, Operand(has_pending_message));
  __ lw(a1, MemOperand(at));
  __ SmiTag(a1);
  __ push(a1);

  ExternalReference pending_message_script =
      ExternalReference::address_of_pending_message_script(isolate());
  __ li(at, Operand(pending_message_script));
  __ lw(a1, MemOperand(at));
  __ push(a1);
}


void FullCodeGenerator::ExitFinallyBlock() {
  DCHECK(!result_register().is(a1));
  // Restore pending message from stack.
  __ pop(a1);
  ExternalReference pending_message_script =
      ExternalReference::address_of_pending_message_script(isolate());
  __ li(at, Operand(pending_message_script));
  __ sw(a1, MemOperand(at));

  __ pop(a1);
  __ SmiUntag(a1);
  ExternalReference has_pending_message =
      ExternalReference::address_of_has_pending_message(isolate());
  __ li(at, Operand(has_pending_message));
  __ sw(a1, MemOperand(at));

  __ pop(a1);
  ExternalReference pending_message_obj =
      ExternalReference::address_of_pending_message_obj(isolate());
  __ li(at, Operand(pending_message_obj));
  __ sw(a1, MemOperand(at));

  // Restore result register from stack.
  __ pop(a1);

  // Uncook return address and return.
  __ pop(result_register());
  DCHECK_EQ(1, kSmiTagSize + kSmiShiftSize);
  __ sra(a1, a1, 1);  // Un-smi-tag value.
  __ Addu(at, a1, Operand(masm_->CodeObject()));
  __ Jump(at);
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
    __ lw(cp, MemOperand(sp, StackHandlerConstants::kContextOffset));
    __ sw(cp, MemOperand(fp, StandardFrameConstants::kContextOffset));
  }
  __ PopTryHandler();
  __ Call(finally_entry_);

  *stack_depth = 0;
  *context_length = 0;
  return previous_;
}


#undef __


void BackEdgeTable::PatchAt(Code* unoptimized_code,
                            Address pc,
                            BackEdgeState target_state,
                            Code* replacement_code) {
  static const int kInstrSize = Assembler::kInstrSize;
  Address branch_address = pc - 6 * kInstrSize;
  CodePatcher patcher(branch_address, 1);

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
    case OSR_AFTER_STACK_CHECK:
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
  Address pc_immediate_load_address = pc - 4 * kInstrSize;
  // Replace the stack check address in the load-immediate (lui/ori pair)
  // with the entry address of the replacement code.
  Assembler::set_target_address_at(pc_immediate_load_address,
                                   replacement_code->entry());

  unoptimized_code->GetHeap()->incremental_marking()->RecordCodeTargetPatch(
      unoptimized_code, pc_immediate_load_address, replacement_code);
}


BackEdgeTable::BackEdgeState BackEdgeTable::GetBackEdgeState(
    Isolate* isolate,
    Code* unoptimized_code,
    Address pc) {
  static const int kInstrSize = Assembler::kInstrSize;
  Address branch_address = pc - 6 * kInstrSize;
  Address pc_immediate_load_address = pc - 4 * kInstrSize;

  DCHECK(Assembler::IsBeq(Assembler::instr_at(pc - 5 * kInstrSize)));
  if (!Assembler::IsAddImmediate(Assembler::instr_at(branch_address))) {
    DCHECK(reinterpret_cast<uint32_t>(
        Assembler::target_address_at(pc_immediate_load_address)) ==
           reinterpret_cast<uint32_t>(
               isolate->builtins()->InterruptCheck()->entry()));
    return INTERRUPT;
  }

  DCHECK(Assembler::IsAddImmediate(Assembler::instr_at(branch_address)));

  if (reinterpret_cast<uint32_t>(
      Assembler::target_address_at(pc_immediate_load_address)) ==
          reinterpret_cast<uint32_t>(
              isolate->builtins()->OnStackReplacement()->entry())) {
    return ON_STACK_REPLACEMENT;
  }

  DCHECK(reinterpret_cast<uint32_t>(
      Assembler::target_address_at(pc_immediate_load_address)) ==
         reinterpret_cast<uint32_t>(
             isolate->builtins()->OsrAfterStackCheck()->entry()));
  return OSR_AFTER_STACK_CHECK;
}


} }  // namespace v8::internal

#endif  // V8_TARGET_ARCH_MIPS
