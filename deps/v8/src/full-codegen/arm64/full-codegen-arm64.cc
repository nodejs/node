// Copyright 2013 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if V8_TARGET_ARCH_ARM64

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

#include "src/arm64/code-stubs-arm64.h"
#include "src/arm64/frames-arm64.h"
#include "src/arm64/macro-assembler-arm64.h"

namespace v8 {
namespace internal {

#define __ ACCESS_MASM(masm())

class JumpPatchSite BASE_EMBEDDED {
 public:
  explicit JumpPatchSite(MacroAssembler* masm) : masm_(masm), reg_(NoReg) {
#ifdef DEBUG
    info_emitted_ = false;
#endif
  }

  ~JumpPatchSite() {
    if (patch_site_.is_bound()) {
      DCHECK(info_emitted_);
    } else {
      DCHECK(reg_.IsNone());
    }
  }

  void EmitJumpIfNotSmi(Register reg, Label* target) {
    // This code will be patched by PatchInlinedSmiCode, in ic-arm64.cc.
    InstructionAccurateScope scope(masm_, 1);
    DCHECK(!info_emitted_);
    DCHECK(reg.Is64Bits());
    DCHECK(!reg.Is(csp));
    reg_ = reg;
    __ bind(&patch_site_);
    __ tbz(xzr, 0, target);   // Always taken before patched.
  }

  void EmitJumpIfSmi(Register reg, Label* target) {
    // This code will be patched by PatchInlinedSmiCode, in ic-arm64.cc.
    InstructionAccurateScope scope(masm_, 1);
    DCHECK(!info_emitted_);
    DCHECK(reg.Is64Bits());
    DCHECK(!reg.Is(csp));
    reg_ = reg;
    __ bind(&patch_site_);
    __ tbnz(xzr, 0, target);  // Never taken before patched.
  }

  void EmitJumpIfEitherNotSmi(Register reg1, Register reg2, Label* target) {
    UseScratchRegisterScope temps(masm_);
    Register temp = temps.AcquireX();
    __ Orr(temp, reg1, reg2);
    EmitJumpIfNotSmi(temp, target);
  }

  void EmitPatchInfo() {
    Assembler::BlockPoolsScope scope(masm_);
    InlineSmiCheckInfo::Emit(masm_, reg_, &patch_site_);
#ifdef DEBUG
    info_emitted_ = true;
#endif
  }

 private:
  MacroAssembler* masm() { return masm_; }
  MacroAssembler* masm_;
  Label patch_site_;
  Register reg_;
#ifdef DEBUG
  bool info_emitted_;
#endif
};


// Generate code for a JS function. On entry to the function the receiver
// and arguments have been pushed on the stack left to right. The actual
// argument count matches the formal parameter count expected by the
// function.
//
// The live registers are:
//   - x1: the JS function object being called (i.e. ourselves).
//   - x3: the new target value
//   - cp: our context.
//   - fp: our caller's frame pointer.
//   - jssp: stack pointer.
//   - lr: return address.
//
// The function builds a JS frame. See JavaScriptFrameConstants in
// frames-arm.h for its layout.
void FullCodeGenerator::Generate() {
  CompilationInfo* info = info_;
  profiling_counter_ = isolate()->factory()->NewCell(
      Handle<Smi>(Smi::FromInt(FLAG_interrupt_budget), isolate()));
  SetFunctionPosition(literal());
  Comment cmnt(masm_, "[ Function compiled by full code generator");

  ProfileEntryHookStub::MaybeCallEntryHook(masm_);

  if (FLAG_debug_code && info->ExpectsJSReceiverAsReceiver()) {
    int receiver_offset = info->scope()->num_parameters() * kXRegSize;
    __ Peek(x10, receiver_offset);
    __ AssertNotSmi(x10);
    __ CompareObjectType(x10, x10, x11, FIRST_JS_RECEIVER_TYPE);
    __ Assert(ge, kSloppyFunctionExpectsJSReceiverReceiver);
  }

  // Open a frame scope to indicate that there is a frame on the stack.
  // The MANUAL indicates that the scope shouldn't actually generate code
  // to set up the frame because we do it manually below.
  FrameScope frame_scope(masm_, StackFrame::MANUAL);

  // This call emits the following sequence in a way that can be patched for
  // code ageing support:
  //  Push(lr, fp, cp, x1);
  //  Add(fp, jssp, 2 * kPointerSize);
  info->set_prologue_offset(masm_->pc_offset());
  __ Prologue(info->GeneratePreagedPrologue());

  // Increment invocation count for the function.
  {
    Comment cmnt(masm_, "[ Increment invocation count");
    __ Ldr(x11, FieldMemOperand(x1, JSFunction::kLiteralsOffset));
    __ Ldr(x11, FieldMemOperand(x11, LiteralsArray::kFeedbackVectorOffset));
    __ Ldr(x10, FieldMemOperand(x11, FeedbackVector::kInvocationCountIndex *
                                             kPointerSize +
                                         FeedbackVector::kHeaderSize));
    __ Add(x10, x10, Operand(Smi::FromInt(1)));
    __ Str(x10, FieldMemOperand(
                    x11, FeedbackVector::kInvocationCountIndex * kPointerSize +
                             FeedbackVector::kHeaderSize));
  }

  // Reserve space on the stack for locals.
  { Comment cmnt(masm_, "[ Allocate locals");
    int locals_count = info->scope()->num_stack_slots();
    OperandStackDepthIncrement(locals_count);
    if (locals_count > 0) {
      if (locals_count >= 128) {
        Label ok;
        DCHECK(jssp.Is(__ StackPointer()));
        __ Sub(x10, jssp, locals_count * kPointerSize);
        __ CompareRoot(x10, Heap::kRealStackLimitRootIndex);
        __ B(hs, &ok);
        __ CallRuntime(Runtime::kThrowStackOverflow);
        __ Bind(&ok);
      }
      __ LoadRoot(x10, Heap::kUndefinedValueRootIndex);
      if (FLAG_optimize_for_size) {
        __ PushMultipleTimes(x10 , locals_count);
      } else {
        const int kMaxPushes = 32;
        if (locals_count >= kMaxPushes) {
          int loop_iterations = locals_count / kMaxPushes;
          __ Mov(x2, loop_iterations);
          Label loop_header;
          __ Bind(&loop_header);
          // Do pushes.
          __ PushMultipleTimes(x10 , kMaxPushes);
          __ Subs(x2, x2, 1);
          __ B(ne, &loop_header);
        }
        int remaining = locals_count % kMaxPushes;
        // Emit the remaining pushes.
        __ PushMultipleTimes(x10 , remaining);
      }
    }
  }

  bool function_in_register_x1 = true;

  if (info->scope()->NeedsContext()) {
    // Argument to NewContext is the function, which is still in x1.
    Comment cmnt(masm_, "[ Allocate context");
    bool need_write_barrier = true;
    int slots = info->scope()->num_heap_slots() - Context::MIN_CONTEXT_SLOTS;
    if (info->scope()->is_script_scope()) {
      __ Mov(x10, Operand(info->scope()->scope_info()));
      __ Push(x1, x10);
      __ CallRuntime(Runtime::kNewScriptContext);
      PrepareForBailoutForId(BailoutId::ScriptContext(),
                             BailoutState::TOS_REGISTER);
      // The new target value is not used, clobbering is safe.
      DCHECK_NULL(info->scope()->new_target_var());
    } else {
      if (info->scope()->new_target_var() != nullptr) {
        __ Push(x3);  // Preserve new target.
      }
      if (slots <=
          ConstructorBuiltinsAssembler::MaximumFunctionContextSlots()) {
        Callable callable = CodeFactory::FastNewFunctionContext(
            isolate(), info->scope()->scope_type());
        __ Mov(FastNewFunctionContextDescriptor::SlotsRegister(), slots);
        __ Call(callable.code(), RelocInfo::CODE_TARGET);
        // Result of the FastNewFunctionContext builtin is always in new space.
        need_write_barrier = false;
      } else {
        __ Push(x1);
        __ Push(Smi::FromInt(info->scope()->scope_type()));
        __ CallRuntime(Runtime::kNewFunctionContext);
      }
      if (info->scope()->new_target_var() != nullptr) {
        __ Pop(x3);  // Restore new target.
      }
    }
    function_in_register_x1 = false;
    // Context is returned in x0.  It replaces the context passed to us.
    // It's saved in the stack and kept live in cp.
    __ Mov(cp, x0);
    __ Str(x0, MemOperand(fp, StandardFrameConstants::kContextOffset));
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
        __ Ldr(x10, MemOperand(fp, parameter_offset));
        // Store it in the context.
        MemOperand target = ContextMemOperand(cp, var->index());
        __ Str(x10, target);

        // Update the write barrier.
        if (need_write_barrier) {
          __ RecordWriteContextSlot(cp, static_cast<int>(target.offset()), x10,
                                    x11, kLRHasBeenSaved, kDontSaveFPRegs);
        } else if (FLAG_debug_code) {
          Label done;
          __ JumpIfInNewSpace(cp, &done);
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
    // Function uses arguments object.
    Comment cmnt(masm_, "[ Allocate arguments object");
    if (!function_in_register_x1) {
      // Load this again, if it's used by the local context below.
      __ Ldr(x1, MemOperand(fp, JavaScriptFrameConstants::kFunctionOffset));
    }
    if (is_strict(language_mode()) || !has_simple_parameters()) {
      FastNewStrictArgumentsStub stub(isolate());
      __ CallStub(&stub);
    } else if (literal()->has_duplicate_parameters()) {
      __ Push(x1);
      __ CallRuntime(Runtime::kNewSloppyArguments_Generic);
    } else {
      FastNewSloppyArgumentsStub stub(isolate());
      __ CallStub(&stub);
    }

    SetVar(arguments, x0, x1, x2);
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
    DCHECK(jssp.Is(__ StackPointer()));
    __ CompareRoot(jssp, Heap::kStackLimitRootIndex);
    __ B(hs, &ok);
    PredictableCodeSizeScope predictable(masm_,
                                         Assembler::kCallSizeWithRelocation);
    __ Call(isolate()->builtins()->StackCheck(), RelocInfo::CODE_TARGET);
    __ Bind(&ok);
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
    __ LoadRoot(x0, Heap::kUndefinedValueRootIndex);
  }
  EmitReturnSequence();

  // Force emission of the pools, so they don't get emitted in the middle
  // of the back edge table.
  masm()->CheckVeneerPool(true, false);
  masm()->CheckConstPool(true, false);
}

void FullCodeGenerator::ClearAccumulator() { __ Mov(x0, Smi::kZero); }

void FullCodeGenerator::EmitProfilingCounterDecrement(int delta) {
  __ Mov(x2, Operand(profiling_counter_));
  __ Ldr(x3, FieldMemOperand(x2, Cell::kValueOffset));
  __ Subs(x3, x3, Smi::FromInt(delta));
  __ Str(x3, FieldMemOperand(x2, Cell::kValueOffset));
}


void FullCodeGenerator::EmitProfilingCounterReset() {
  int reset_value = FLAG_interrupt_budget;
  __ Mov(x2, Operand(profiling_counter_));
  __ Mov(x3, Smi::FromInt(reset_value));
  __ Str(x3, FieldMemOperand(x2, Cell::kValueOffset));
}


void FullCodeGenerator::EmitBackEdgeBookkeeping(IterationStatement* stmt,
                                                Label* back_edge_target) {
  DCHECK(jssp.Is(__ StackPointer()));
  Comment cmnt(masm_, "[ Back edge bookkeeping");
  // Block literal pools whilst emitting back edge code.
  Assembler::BlockPoolsScope block_const_pool(masm_);
  Label ok;

  DCHECK(back_edge_target->is_bound());
  // We want to do a round rather than a floor of distance/kCodeSizeMultiplier
  // to reduce the absolute error due to the integer division. To do that,
  // we add kCodeSizeMultiplier/2 to the distance (equivalent to adding 0.5 to
  // the result).
  int distance =
      static_cast<int>(masm_->SizeOfCodeGeneratedSince(back_edge_target) +
                       kCodeSizeMultiplier / 2);
  int weight = Min(kMaxBackEdgeWeight,
                   Max(1, distance / kCodeSizeMultiplier));
  EmitProfilingCounterDecrement(weight);
  __ B(pl, &ok);
  __ Call(isolate()->builtins()->InterruptCheck(), RelocInfo::CODE_TARGET);

  // Record a mapping of this PC offset to the OSR id.  This is used to find
  // the AST id from the unoptimized code in order to use it as a key into
  // the deoptimization input data found in the optimized code.
  RecordBackEdge(stmt->OsrEntryId());

  EmitProfilingCounterReset();

  __ Bind(&ok);
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
  __ B(pl, &ok);
  // Don't need to save result register if we are going to do a tail call.
  if (!is_tail_call) {
    __ Push(x0);
  }
  __ Call(isolate()->builtins()->InterruptCheck(), RelocInfo::CODE_TARGET);
  if (!is_tail_call) {
    __ Pop(x0);
  }
  EmitProfilingCounterReset();
  __ Bind(&ok);
}

void FullCodeGenerator::EmitReturnSequence() {
  Comment cmnt(masm_, "[ Return sequence");

  if (return_label_.is_bound()) {
    __ B(&return_label_);

  } else {
    __ Bind(&return_label_);
    if (FLAG_trace) {
      // Push the return value on the stack as the parameter.
      // Runtime::TraceExit returns its parameter in x0.
      __ Push(result_register());
      __ CallRuntime(Runtime::kTraceExit);
      DCHECK(x0.Is(result_register()));
    }
    EmitProfilingCounterHandlingForReturnSequence(false);

    SetReturnPosition(literal());
    const Register& current_sp = __ StackPointer();
    // Nothing ensures 16 bytes alignment here.
    DCHECK(!current_sp.Is(csp));
    __ Mov(current_sp, fp);
    __ Ldp(fp, lr, MemOperand(current_sp, 2 * kXRegSize, PostIndex));
    // Drop the arguments and receiver and return.
    // TODO(all): This implementation is overkill as it supports 2**31+1
    // arguments, consider how to improve it without creating a security
    // hole.
    __ ldr_pcrel(ip0, (3 * kInstructionSize) >> kLoadLiteralScaleLog2);
    __ Add(current_sp, current_sp, ip0);
    __ Ret();
    int32_t arg_count = info_->scope()->num_parameters() + 1;
    __ dc64(kXRegSize * arg_count);
  }
}

void FullCodeGenerator::RestoreContext() {
  __ Ldr(cp, MemOperand(fp, StandardFrameConstants::kContextOffset));
}

void FullCodeGenerator::StackValueContext::Plug(Variable* var) const {
  DCHECK(var->IsStackAllocated() || var->IsContextSlot());
  codegen()->GetVar(result_register(), var);
  codegen()->PushOperand(result_register());
}


void FullCodeGenerator::EffectContext::Plug(Heap::RootListIndex index) const {
  // Root values have no side effects.
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
  codegen()->PrepareForBailoutBeforeSplit(condition(), true, true_label_,
                                          false_label_);
  if (index == Heap::kUndefinedValueRootIndex ||
      index == Heap::kNullValueRootIndex ||
      index == Heap::kFalseValueRootIndex) {
    if (false_label_ != fall_through_) __ B(false_label_);
  } else if (index == Heap::kTrueValueRootIndex) {
    if (true_label_ != fall_through_) __ B(true_label_);
  } else {
    __ LoadRoot(result_register(), index);
    codegen()->DoTest(this);
  }
}


void FullCodeGenerator::EffectContext::Plug(Handle<Object> lit) const {
}


void FullCodeGenerator::AccumulatorValueContext::Plug(
    Handle<Object> lit) const {
  __ Mov(result_register(), Operand(lit));
}


void FullCodeGenerator::StackValueContext::Plug(Handle<Object> lit) const {
  // Immediates cannot be pushed directly.
  __ Mov(result_register(), Operand(lit));
  codegen()->PushOperand(result_register());
}


void FullCodeGenerator::TestContext::Plug(Handle<Object> lit) const {
  codegen()->PrepareForBailoutBeforeSplit(condition(),
                                          true,
                                          true_label_,
                                          false_label_);
  DCHECK(lit->IsNullOrUndefined(isolate()) || !lit->IsUndetectable());
  if (lit->IsNullOrUndefined(isolate()) || lit->IsFalse(isolate())) {
    if (false_label_ != fall_through_) __ B(false_label_);
  } else if (lit->IsTrue(isolate()) || lit->IsJSObject()) {
    if (true_label_ != fall_through_) __ B(true_label_);
  } else if (lit->IsString()) {
    if (String::cast(*lit)->length() == 0) {
      if (false_label_ != fall_through_) __ B(false_label_);
    } else {
      if (true_label_ != fall_through_) __ B(true_label_);
    }
  } else if (lit->IsSmi()) {
    if (Smi::cast(*lit)->value() == 0) {
      if (false_label_ != fall_through_) __ B(false_label_);
    } else {
      if (true_label_ != fall_through_) __ B(true_label_);
    }
  } else {
    // For simplicity we always test the accumulator register.
    __ Mov(result_register(), Operand(lit));
    codegen()->DoTest(this);
  }
}


void FullCodeGenerator::StackValueContext::DropAndPlug(int count,
                                                       Register reg) const {
  DCHECK(count > 0);
  if (count > 1) codegen()->DropOperands(count - 1);
  __ Poke(reg, 0);
}


void FullCodeGenerator::EffectContext::Plug(Label* materialize_true,
                                            Label* materialize_false) const {
  DCHECK(materialize_true == materialize_false);
  __ Bind(materialize_true);
}


void FullCodeGenerator::AccumulatorValueContext::Plug(
    Label* materialize_true,
    Label* materialize_false) const {
  Label done;
  __ Bind(materialize_true);
  __ LoadRoot(result_register(), Heap::kTrueValueRootIndex);
  __ B(&done);
  __ Bind(materialize_false);
  __ LoadRoot(result_register(), Heap::kFalseValueRootIndex);
  __ Bind(&done);
}


void FullCodeGenerator::StackValueContext::Plug(
    Label* materialize_true,
    Label* materialize_false) const {
  Label done;
  __ Bind(materialize_true);
  __ LoadRoot(x10, Heap::kTrueValueRootIndex);
  __ B(&done);
  __ Bind(materialize_false);
  __ LoadRoot(x10, Heap::kFalseValueRootIndex);
  __ Bind(&done);
  codegen()->PushOperand(x10);
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
  __ LoadRoot(x10, value_root_index);
  codegen()->PushOperand(x10);
}


void FullCodeGenerator::TestContext::Plug(bool flag) const {
  codegen()->PrepareForBailoutBeforeSplit(condition(),
                                          true,
                                          true_label_,
                                          false_label_);
  if (flag) {
    if (true_label_ != fall_through_) {
      __ B(true_label_);
    }
  } else {
    if (false_label_ != fall_through_) {
      __ B(false_label_);
    }
  }
}


void FullCodeGenerator::DoTest(Expression* condition,
                               Label* if_true,
                               Label* if_false,
                               Label* fall_through) {
  Handle<Code> ic = ToBooleanICStub::GetUninitialized(isolate());
  CallIC(ic, condition->test_id());
  __ CompareRoot(result_register(), Heap::kTrueValueRootIndex);
  Split(eq, if_true, if_false, fall_through);
}


// If (cond), branch to if_true.
// If (!cond), branch to if_false.
// fall_through is used as an optimization in cases where only one branch
// instruction is necessary.
void FullCodeGenerator::Split(Condition cond,
                              Label* if_true,
                              Label* if_false,
                              Label* fall_through) {
  if (if_false == fall_through) {
    __ B(cond, if_true);
  } else if (if_true == fall_through) {
    DCHECK(if_false != fall_through);
    __ B(NegateCondition(cond), if_false);
  } else {
    __ B(cond, if_true);
    __ B(if_false);
  }
}


MemOperand FullCodeGenerator::StackOperand(Variable* var) {
  // Offset is negative because higher indexes are at lower addresses.
  int offset = -var->index() * kXRegSize;
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
  __ Ldr(dest, location);
}


void FullCodeGenerator::SetVar(Variable* var,
                               Register src,
                               Register scratch0,
                               Register scratch1) {
  DCHECK(var->IsContextSlot() || var->IsStackAllocated());
  DCHECK(!AreAliased(src, scratch0, scratch1));
  MemOperand location = VarOperand(var, scratch0);
  __ Str(src, location);

  // Emit the write barrier code if the location is in the heap.
  if (var->IsContextSlot()) {
    // scratch0 contains the correct context.
    __ RecordWriteContextSlot(scratch0, static_cast<int>(location.offset()),
                              src, scratch1, kLRHasBeenSaved, kDontSaveFPRegs);
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

  // TODO(all): Investigate to see if there is something to work on here.
  Label skip;
  if (should_normalize) {
    __ B(&skip);
  }
  PrepareForBailout(expr, BailoutState::TOS_REGISTER);
  if (should_normalize) {
    __ CompareRoot(x0, Heap::kTrueValueRootIndex);
    Split(eq, if_true, if_false, NULL);
    __ Bind(&skip);
  }
}


void FullCodeGenerator::EmitDebugCheckDeclarationContext(Variable* variable) {
  // The variable in the declaration always resides in the current function
  // context.
  DCHECK_EQ(0, scope()->ContextChainLength(variable->scope()));
  if (FLAG_debug_code) {
    // Check that we're not inside a with or catch context.
    __ Ldr(x1, FieldMemOperand(cp, HeapObject::kMapOffset));
    __ CompareRoot(x1, Heap::kWithContextMapRootIndex);
    __ Check(ne, kDeclarationInWithContext);
    __ CompareRoot(x1, Heap::kCatchContextMapRootIndex);
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
        __ LoadRoot(x10, Heap::kTheHoleValueRootIndex);
        __ Str(x10, StackOperand(variable));
      }
      break;

    case VariableLocation::CONTEXT:
      if (variable->binding_needs_init()) {
        Comment cmnt(masm_, "[ VariableDeclaration");
        EmitDebugCheckDeclarationContext(variable);
        __ LoadRoot(x10, Heap::kTheHoleValueRootIndex);
        __ Str(x10, ContextMemOperand(cp, variable->index()));
        // No write barrier since the_hole_value is in old space.
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
      // Check for stack overflow exception.
      if (function.is_null()) return SetStackOverflow();
      globals_->Add(function, zone());
      break;
    }

    case VariableLocation::PARAMETER:
    case VariableLocation::LOCAL: {
      Comment cmnt(masm_, "[ Function Declaration");
      VisitForAccumulatorValue(declaration->fun());
      __ Str(result_register(), StackOperand(variable));
      break;
    }

    case VariableLocation::CONTEXT: {
      Comment cmnt(masm_, "[ Function Declaration");
      EmitDebugCheckDeclarationContext(variable);
      VisitForAccumulatorValue(declaration->fun());
      __ Str(result_register(), ContextMemOperand(cp, variable->index()));
      int offset = Context::SlotOffset(variable->index());
      // We know that we have written a function, which is not a smi.
      __ RecordWriteContextSlot(cp,
                                offset,
                                result_register(),
                                x2,
                                kLRHasBeenSaved,
                                kDontSaveFPRegs,
                                EMIT_REMEMBERED_SET,
                                OMIT_SMI_CHECK);
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
  __ Mov(x11, Operand(pairs));
  Register flags = xzr;
  if (Smi::FromInt(DeclareGlobalsFlags())) {
    flags = x10;
  __ Mov(flags, Smi::FromInt(DeclareGlobalsFlags()));
  }
  __ EmitLoadFeedbackVector(x12);
  __ Push(x11, flags, x12);
  __ CallRuntime(Runtime::kDeclareGlobals);
  // Return value is ignored.
}


void FullCodeGenerator::VisitSwitchStatement(SwitchStatement* stmt) {
  ASM_LOCATION("FullCodeGenerator::VisitSwitchStatement");
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
    __ Bind(&next_test);
    next_test.Unuse();

    // Compile the label expression.
    VisitForAccumulatorValue(clause->label());

    // Perform the comparison as if via '==='.
    __ Peek(x1, 0);   // Switch value.

    JumpPatchSite patch_site(masm_);
    if (ShouldInlineSmiCase(Token::EQ_STRICT)) {
      Label slow_case;
      patch_site.EmitJumpIfEitherNotSmi(x0, x1, &slow_case);
      __ Cmp(x1, x0);
      __ B(ne, &next_test);
      __ Drop(1);  // Switch value is no longer needed.
      __ B(clause->body_target());
      __ Bind(&slow_case);
    }

    // Record position before stub call for type feedback.
    SetExpressionPosition(clause);
    Handle<Code> ic =
        CodeFactory::CompareIC(isolate(), Token::EQ_STRICT).code();
    CallIC(ic, clause->CompareId());
    patch_site.EmitPatchInfo();

    Label skip;
    __ B(&skip);
    PrepareForBailout(clause, BailoutState::TOS_REGISTER);
    __ JumpIfNotRoot(x0, Heap::kTrueValueRootIndex, &next_test);
    __ Drop(1);
    __ B(clause->body_target());
    __ Bind(&skip);

    __ Cbnz(x0, &next_test);
    __ Drop(1);  // Switch value is no longer needed.
    __ B(clause->body_target());
  }

  // Discard the test value and jump to the default if present, otherwise to
  // the end of the statement.
  __ Bind(&next_test);
  DropOperands(1);  // Switch value is no longer needed.
  if (default_clause == NULL) {
    __ B(nested_statement.break_label());
  } else {
    __ B(default_clause->body_target());
  }

  // Compile all the case bodies.
  for (int i = 0; i < clauses->length(); i++) {
    Comment cmnt(masm_, "[ Case body");
    CaseClause* clause = clauses->at(i);
    __ Bind(clause->body_target());
    PrepareForBailoutForId(clause->EntryId(), BailoutState::NO_REGISTERS);
    VisitStatements(clause->statements());
  }

  __ Bind(nested_statement.break_label());
  PrepareForBailoutForId(stmt->ExitId(), BailoutState::NO_REGISTERS);
}


void FullCodeGenerator::VisitForInStatement(ForInStatement* stmt) {
  ASM_LOCATION("FullCodeGenerator::VisitForInStatement");
  Comment cmnt(masm_, "[ ForInStatement");
  SetStatementPosition(stmt, SKIP_BREAK);

  FeedbackVectorSlot slot = stmt->ForInFeedbackSlot();

  // TODO(all): This visitor probably needs better comments and a revisit.

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
  __ JumpIfSmi(x0, &convert);
  __ JumpIfObjectType(x0, x10, x11, FIRST_JS_RECEIVER_TYPE, &done_convert, ge);
  __ JumpIfRoot(x0, Heap::kNullValueRootIndex, &exit);
  __ JumpIfRoot(x0, Heap::kUndefinedValueRootIndex, &exit);
  __ Bind(&convert);
  __ Call(isolate()->builtins()->ToObject(), RelocInfo::CODE_TARGET);
  RestoreContext();
  __ Bind(&done_convert);
  PrepareForBailoutForId(stmt->ToObjectId(), BailoutState::TOS_REGISTER);
  __ Push(x0);

  // Check cache validity in generated code. If we cannot guarantee cache
  // validity, call the runtime system to check cache validity or get the
  // property names in a fixed array. Note: Proxies never have an enum cache,
  // so will always take the slow path.
  Label call_runtime;
  __ CheckEnumCache(x0, x15, x10, x11, x12, x13, &call_runtime);

  // The enum cache is valid.  Load the map of the object being
  // iterated over and use the cache for the iteration.
  Label use_cache;
  __ Ldr(x0, FieldMemOperand(x0, HeapObject::kMapOffset));
  __ B(&use_cache);

  // Get the set of properties to enumerate.
  __ Bind(&call_runtime);
  __ Push(x0);  // Duplicate the enumerable object on the stack.
  __ CallRuntime(Runtime::kForInEnumerate);
  PrepareForBailoutForId(stmt->EnumId(), BailoutState::TOS_REGISTER);

  // If we got a map from the runtime call, we can do a fast
  // modification check. Otherwise, we got a fixed array, and we have
  // to do a slow check.
  Label fixed_array, no_descriptors;
  __ Ldr(x2, FieldMemOperand(x0, HeapObject::kMapOffset));
  __ JumpIfNotRoot(x2, Heap::kMetaMapRootIndex, &fixed_array);

  // We got a map in register x0. Get the enumeration cache from it.
  __ Bind(&use_cache);

  __ EnumLengthUntagged(x1, x0);
  __ Cbz(x1, &no_descriptors);

  __ LoadInstanceDescriptors(x0, x2);
  __ Ldr(x2, FieldMemOperand(x2, DescriptorArray::kEnumCacheOffset));
  __ Ldr(x2,
         FieldMemOperand(x2, DescriptorArray::kEnumCacheBridgeCacheOffset));

  // Set up the four remaining stack slots.
  __ SmiTag(x1);
  // Map, enumeration cache, enum cache length, zero (both last as smis).
  __ Push(x0, x2, x1, xzr);
  __ B(&loop);

  __ Bind(&no_descriptors);
  __ Drop(1);
  __ B(&exit);

  // We got a fixed array in register x0. Iterate through that.
  __ Bind(&fixed_array);

  __ Mov(x1, Smi::FromInt(1));  // Smi(1) indicates slow check.
  __ Ldr(x2, FieldMemOperand(x0, FixedArray::kLengthOffset));
  __ Push(x1, x0, x2);  // Smi and array, fixed array length (as smi).
  PrepareForBailoutForId(stmt->PrepareId(), BailoutState::NO_REGISTERS);
  __ Push(xzr);  // Initial index.

  // Generate code for doing the condition check.
  __ Bind(&loop);
  SetExpressionAsStatementPosition(stmt->each());

  // Load the current count to x0, load the length to x1.
  __ PeekPair(x0, x1, 0);
  __ Cmp(x0, x1);  // Compare to the array length.
  __ B(hs, loop_statement.break_label());

  // Get the current entry of the array into register x0.
  __ Peek(x10, 2 * kXRegSize);
  __ Add(x10, x10, Operand::UntagSmiAndScale(x0, kPointerSizeLog2));
  __ Ldr(x0, MemOperand(x10, FixedArray::kHeaderSize - kHeapObjectTag));

  // Get the expected map from the stack or a smi in the
  // permanent slow case into register x2.
  __ Peek(x2, 3 * kXRegSize);

  // Check if the expected map still matches that of the enumerable.
  // If not, we may have to filter the key.
  Label update_each;
  __ Peek(x1, 4 * kXRegSize);
  __ Ldr(x11, FieldMemOperand(x1, HeapObject::kMapOffset));
  __ Cmp(x11, x2);
  __ B(eq, &update_each);

  // We need to filter the key, record slow-path here.
  int const vector_index = SmiFromSlot(slot)->value();
  __ EmitLoadFeedbackVector(x3);
  __ Mov(x10, Operand(FeedbackVector::MegamorphicSentinel(isolate())));
  __ Str(x10, FieldMemOperand(x3, FixedArray::OffsetOfElementAt(vector_index)));

  // x0 contains the key. The receiver in x1 is the second argument to the
  // ForInFilter. ForInFilter returns undefined if the receiver doesn't
  // have the key or returns the name-converted key.
  __ Call(isolate()->builtins()->ForInFilter(), RelocInfo::CODE_TARGET);
  RestoreContext();
  PrepareForBailoutForId(stmt->FilterId(), BailoutState::TOS_REGISTER);
  __ CompareRoot(result_register(), Heap::kUndefinedValueRootIndex);
  __ B(eq, loop_statement.continue_label());

  // Update the 'each' property or variable from the possibly filtered
  // entry in register x0.
  __ Bind(&update_each);
  // Perform the assignment as if via '='.
  { EffectContext context(this);
    EmitAssignment(stmt->each(), stmt->EachFeedbackSlot());
    PrepareForBailoutForId(stmt->AssignmentId(), BailoutState::NO_REGISTERS);
  }

  // Both Crankshaft and Turbofan expect BodyId to be right before stmt->body().
  PrepareForBailoutForId(stmt->BodyId(), BailoutState::NO_REGISTERS);
  // Generate code for the body of the loop.
  Visit(stmt->body());

  // Generate code for going to the next element by incrementing
  // the index (smi) stored on top of the stack.
  __ Bind(loop_statement.continue_label());
  PrepareForBailoutForId(stmt->IncrementId(), BailoutState::NO_REGISTERS);
  // TODO(all): We could use a callee saved register to avoid popping.
  __ Pop(x0);
  __ Add(x0, x0, Smi::FromInt(1));
  __ Push(x0);

  EmitBackEdgeBookkeeping(stmt, &loop);
  __ B(&loop);

  // Remove the pointers stored on the stack.
  __ Bind(loop_statement.break_label());
  DropOperands(5);

  // Exit and decrement the loop depth.
  PrepareForBailoutForId(stmt->ExitId(), BailoutState::NO_REGISTERS);
  __ Bind(&exit);
  decrement_loop_depth();
}


void FullCodeGenerator::EmitSetHomeObject(Expression* initializer, int offset,
                                          FeedbackVectorSlot slot) {
  DCHECK(NeedsHomeObject(initializer));
  __ Peek(StoreDescriptor::ReceiverRegister(), 0);
  __ Peek(StoreDescriptor::ValueRegister(), offset * kPointerSize);
  CallStoreIC(slot, isolate()->factory()->home_object_symbol());
}


void FullCodeGenerator::EmitSetHomeObjectAccumulator(Expression* initializer,
                                                     int offset,
                                                     FeedbackVectorSlot slot) {
  DCHECK(NeedsHomeObject(initializer));
  __ Move(StoreDescriptor::ReceiverRegister(), x0);
  __ Peek(StoreDescriptor::ValueRegister(), offset * kPointerSize);
  CallStoreIC(slot, isolate()->factory()->home_object_symbol());
}

void FullCodeGenerator::EmitVariableLoad(VariableProxy* proxy,
                                         TypeofMode typeof_mode) {
  // Record position before possible IC call.
  SetExpressionPosition(proxy);
  PrepareForBailoutForId(proxy->BeforeId(), BailoutState::NO_REGISTERS);
  Variable* var = proxy->var();

  // Two cases: global variables and all other types of variables.
  switch (var->location()) {
    case VariableLocation::UNALLOCATED: {
      Comment cmnt(masm_, "Global variable");
      EmitGlobalVariableLoad(proxy, typeof_mode);
      context()->Plug(x0);
      break;
    }

    case VariableLocation::PARAMETER:
    case VariableLocation::LOCAL:
    case VariableLocation::CONTEXT: {
      DCHECK_EQ(NOT_INSIDE_TYPEOF, typeof_mode);
      Comment cmnt(masm_, var->IsContextSlot()
                              ? "Context variable"
                              : "Stack variable");
      if (proxy->hole_check_mode() == HoleCheckMode::kRequired) {
        // Throw a reference error when using an uninitialized let/const
        // binding in harmony mode.
        Label done;
        GetVar(x0, var);
        __ JumpIfNotRoot(x0, Heap::kTheHoleValueRootIndex, &done);
        __ Mov(x0, Operand(var->name()));
        __ Push(x0);
        __ CallRuntime(Runtime::kThrowReferenceError);
        __ Bind(&done);
        context()->Plug(x0);
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
    __ LoadRoot(x10, Heap::kNullValueRootIndex);
    PushOperand(x10);
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
  __ Ldr(x3, MemOperand(fp,  JavaScriptFrameConstants::kFunctionOffset));
  __ Mov(x2, Smi::FromInt(expr->literal_index()));
  __ Mov(x1, Operand(constant_properties));
  int flags = expr->ComputeFlags();
  __ Mov(x0, Smi::FromInt(flags));
  if (MustCreateObjectLiteralWithRuntime(expr)) {
    __ Push(x3, x2, x1, x0);
    __ CallRuntime(Runtime::kCreateObjectLiteral);
  } else {
    Callable callable = CodeFactory::FastCloneShallowObject(
        isolate(), expr->properties_count());
    __ Call(callable.code(), RelocInfo::CODE_TARGET);
    RestoreContext();
  }
  PrepareForBailoutForId(expr->CreateLiteralId(), BailoutState::TOS_REGISTER);

  // If result_saved is true the result is on top of the stack.  If
  // result_saved is false the result is in x0.
  bool result_saved = false;

  AccessorTable accessor_table(zone());
  for (int i = 0; i < expr->properties()->length(); i++) {
    ObjectLiteral::Property* property = expr->properties()->at(i);
    DCHECK(!property->is_computed_name());
    if (property->IsCompileTimeValue()) continue;

    Literal* key = property->key()->AsLiteral();
    Expression* value = property->value();
    if (!result_saved) {
      PushOperand(x0);  // Save result on stack
      result_saved = true;
    }
    switch (property->kind()) {
      case ObjectLiteral::Property::SPREAD:
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
            DCHECK(StoreDescriptor::ValueRegister().is(x0));
            __ Peek(StoreDescriptor::ReceiverRegister(), 0);
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
        __ Peek(x0, 0);
        PushOperand(x0);
        VisitForStackValue(key);
        VisitForStackValue(value);
        if (property->emit_store()) {
          if (NeedsHomeObject(value)) {
            EmitSetHomeObject(value, 2, property->GetSlot());
          }
          __ Mov(x0, Smi::FromInt(SLOPPY));  // Language mode
          PushOperand(x0);
          CallRuntimeWithOperands(Runtime::kSetProperty);
        } else {
          DropOperands(3);
        }
        break;
      case ObjectLiteral::Property::PROTOTYPE:
        DCHECK(property->emit_store());
        // Duplicate receiver on stack.
        __ Peek(x0, 0);
        PushOperand(x0);
        VisitForStackValue(value);
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
    __ Peek(x10, 0);  // Duplicate receiver.
    PushOperand(x10);
    VisitForStackValue(it->first);
    EmitAccessor(it->second->getter);
    EmitAccessor(it->second->setter);
    __ Mov(x10, Smi::FromInt(NONE));
    PushOperand(x10);
    CallRuntimeWithOperands(Runtime::kDefineAccessorPropertyUnchecked);
    PrepareForBailoutForId(it->second->bailout_id, BailoutState::NO_REGISTERS);
  }

  if (result_saved) {
    context()->PlugTOS();
  } else {
    context()->Plug(x0);
  }
}


void FullCodeGenerator::VisitArrayLiteral(ArrayLiteral* expr) {
  Comment cmnt(masm_, "[ ArrayLiteral");

  Handle<ConstantElementsPair> constant_elements =
      expr->GetOrBuildConstantElements(isolate());
  bool has_fast_elements =
      IsFastObjectElementsKind(expr->constant_elements_kind());

  AllocationSiteMode allocation_site_mode = TRACK_ALLOCATION_SITE;
  if (has_fast_elements && !FLAG_allocation_site_pretenuring) {
    // If the only customer of allocation sites is transitioning, then
    // we can turn it off if we don't have anywhere else to transition to.
    allocation_site_mode = DONT_TRACK_ALLOCATION_SITE;
  }

  __ Ldr(x3, MemOperand(fp, JavaScriptFrameConstants::kFunctionOffset));
  __ Mov(x2, Smi::FromInt(expr->literal_index()));
  __ Mov(x1, Operand(constant_elements));
  if (MustCreateArrayLiteralWithRuntime(expr)) {
    __ Mov(x0, Smi::FromInt(expr->ComputeFlags()));
    __ Push(x3, x2, x1, x0);
    __ CallRuntime(Runtime::kCreateArrayLiteral);
  } else {
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
      PushOperand(x0);
      result_saved = true;
    }
    VisitForAccumulatorValue(subexpr);

    __ Mov(StoreDescriptor::NameRegister(), Smi::FromInt(array_index));
    __ Peek(StoreDescriptor::ReceiverRegister(), 0);
    CallKeyedStoreIC(expr->LiteralFeedbackSlot());

    PrepareForBailoutForId(expr->GetIdForElement(array_index),
                           BailoutState::NO_REGISTERS);
  }

  if (result_saved) {
    context()->PlugTOS();
  } else {
    context()->Plug(x0);
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
        __ Peek(LoadDescriptor::ReceiverRegister(), 0);
      } else {
        VisitForStackValue(property->obj());
      }
      break;
    case KEYED_PROPERTY:
      if (expr->is_compound()) {
        VisitForStackValue(property->obj());
        VisitForStackValue(property->key());
        __ Peek(LoadDescriptor::ReceiverRegister(), 1 * kPointerSize);
        __ Peek(LoadDescriptor::NameRegister(), 0);
      } else {
        VisitForStackValue(property->obj());
        VisitForStackValue(property->key());
      }
      break;
    case NAMED_SUPER_PROPERTY:
    case KEYED_SUPER_PROPERTY:
      UNREACHABLE();
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
    PushOperand(x0);  // Left operand goes on the stack.
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
    case VARIABLE: {
      VariableProxy* proxy = expr->target()->AsVariableProxy();
      EmitVariableAssignment(proxy->var(), expr->op(), expr->AssignmentSlot(),
                             proxy->hole_check_mode());
      PrepareForBailoutForId(expr->AssignmentId(), BailoutState::TOS_REGISTER);
      context()->Plug(x0);
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


void FullCodeGenerator::EmitInlineSmiBinaryOp(BinaryOperation* expr,
                                              Token::Value op,
                                              Expression* left_expr,
                                              Expression* right_expr) {
  Label done, both_smis, stub_call;

  // Get the arguments.
  Register left = x1;
  Register right = x0;
  Register result = x0;
  PopOperand(left);

  // Perform combined smi check on both operands.
  __ Orr(x10, left, right);
  JumpPatchSite patch_site(masm_);
  patch_site.EmitJumpIfSmi(x10, &both_smis);

  __ Bind(&stub_call);

  Handle<Code> code = CodeFactory::BinaryOpIC(isolate(), op).code();
  {
    Assembler::BlockPoolsScope scope(masm_);
    CallIC(code, expr->BinaryOperationFeedbackId());
    patch_site.EmitPatchInfo();
  }
  __ B(&done);

  __ Bind(&both_smis);
  // Smi case. This code works in the same way as the smi-smi case in the type
  // recording binary operation stub, see
  // BinaryOpStub::GenerateSmiSmiOperation for comments.
  // TODO(all): That doesn't exist any more. Where are the comments?
  //
  // The set of operations that needs to be supported here is controlled by
  // FullCodeGenerator::ShouldInlineSmiCase().
  switch (op) {
    case Token::SAR:
      __ Ubfx(right, right, kSmiShift, 5);
      __ Asr(result, left, right);
      __ Bic(result, result, kSmiShiftMask);
      break;
    case Token::SHL:
      __ Ubfx(right, right, kSmiShift, 5);
      __ Lsl(result, left, right);
      break;
    case Token::SHR:
      // If `left >>> right` >= 0x80000000, the result is not representable in a
      // signed 32-bit smi.
      __ Ubfx(right, right, kSmiShift, 5);
      __ Lsr(x10, left, right);
      __ Tbnz(x10, kXSignBit, &stub_call);
      __ Bic(result, x10, kSmiShiftMask);
      break;
    case Token::ADD:
      __ Adds(x10, left, right);
      __ B(vs, &stub_call);
      __ Mov(result, x10);
      break;
    case Token::SUB:
      __ Subs(x10, left, right);
      __ B(vs, &stub_call);
      __ Mov(result, x10);
      break;
    case Token::MUL: {
      Label not_minus_zero, done;
      STATIC_ASSERT(static_cast<unsigned>(kSmiShift) == (kXRegSizeInBits / 2));
      STATIC_ASSERT(kSmiTag == 0);
      __ Smulh(x10, left, right);
      __ Cbnz(x10, &not_minus_zero);
      __ Eor(x11, left, right);
      __ Tbnz(x11, kXSignBit, &stub_call);
      __ Mov(result, x10);
      __ B(&done);
      __ Bind(&not_minus_zero);
      __ Cls(x11, x10);
      __ Cmp(x11, kXRegSizeInBits - kSmiShift);
      __ B(lt, &stub_call);
      __ SmiTag(result, x10);
      __ Bind(&done);
      break;
    }
    case Token::BIT_OR:
      __ Orr(result, left, right);
      break;
    case Token::BIT_AND:
      __ And(result, left, right);
      break;
    case Token::BIT_XOR:
      __ Eor(result, left, right);
      break;
    default:
      UNREACHABLE();
  }

  __ Bind(&done);
  context()->Plug(x0);
}


void FullCodeGenerator::EmitBinaryOp(BinaryOperation* expr, Token::Value op) {
  PopOperand(x1);
  Handle<Code> code = CodeFactory::BinaryOpIC(isolate(), op).code();
  JumpPatchSite patch_site(masm_);    // Unbound, signals no inlined smi code.
  {
    Assembler::BlockPoolsScope scope(masm_);
    CallIC(code, expr->BinaryOperationFeedbackId());
    patch_site.EmitPatchInfo();
  }
  context()->Plug(x0);
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
      PushOperand(x0);  // Preserve value.
      VisitForAccumulatorValue(prop->obj());
      // TODO(all): We could introduce a VisitForRegValue(reg, expr) to avoid
      // this copy.
      __ Mov(StoreDescriptor::ReceiverRegister(), x0);
      PopOperand(StoreDescriptor::ValueRegister());  // Restore value.
      CallStoreIC(slot, prop->key()->AsLiteral()->value());
      break;
    }
    case KEYED_PROPERTY: {
      PushOperand(x0);  // Preserve value.
      VisitForStackValue(prop->obj());
      VisitForAccumulatorValue(prop->key());
      __ Mov(StoreDescriptor::NameRegister(), x0);
      PopOperands(StoreDescriptor::ReceiverRegister(),
                  StoreDescriptor::ValueRegister());
      CallKeyedStoreIC(slot);
      break;
    }
    case NAMED_SUPER_PROPERTY:
    case KEYED_SUPER_PROPERTY:
      UNREACHABLE();
      break;
  }
  context()->Plug(x0);
}


void FullCodeGenerator::EmitStoreToStackLocalOrContextSlot(
    Variable* var, MemOperand location) {
  __ Str(result_register(), location);
  if (var->IsContextSlot()) {
    // RecordWrite may destroy all its register arguments.
    __ Mov(x10, result_register());
    int offset = Context::SlotOffset(var->index());
    __ RecordWriteContextSlot(
        x1, offset, x10, x11, kLRHasBeenSaved, kDontSaveFPRegs);
  }
}

void FullCodeGenerator::EmitVariableAssignment(Variable* var, Token::Value op,
                                               FeedbackVectorSlot slot,
                                               HoleCheckMode hole_check_mode) {
  ASM_LOCATION("FullCodeGenerator::EmitVariableAssignment");
  if (var->IsUnallocated()) {
    // Global var, const, or let.
    __ LoadGlobalObject(StoreDescriptor::ReceiverRegister());
    CallStoreIC(slot, var->name());

  } else if (IsLexicalVariableMode(var->mode()) && op != Token::INIT) {
    DCHECK(!var->IsLookupSlot());
    DCHECK(var->IsStackAllocated() || var->IsContextSlot());
    MemOperand location = VarOperand(var, x1);
    // Perform an initialization check for lexically declared variables.
    if (var->binding_needs_init()) {
      Label assign;
      __ Ldr(x10, location);
      __ JumpIfNotRoot(x10, Heap::kTheHoleValueRootIndex, &assign);
      __ Mov(x10, Operand(var->name()));
      __ Push(x10);
      __ CallRuntime(Runtime::kThrowReferenceError);
      __ Bind(&assign);
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
    MemOperand location = VarOperand(var, x1);
    __ Ldr(x10, location);
    __ JumpIfRoot(x10, Heap::kTheHoleValueRootIndex, &uninitialized_this);
    __ Mov(x0, Operand(var->name()));
    __ Push(x0);
    __ CallRuntime(Runtime::kThrowReferenceError);
    __ bind(&uninitialized_this);
    EmitStoreToStackLocalOrContextSlot(var, location);

  } else {
    DCHECK(var->mode() != CONST || op == Token::INIT);
    DCHECK(var->IsStackAllocated() || var->IsContextSlot());
    DCHECK(!var->IsLookupSlot());
    // Assignment to var or initializing assignment to let/const in harmony
    // mode.
    MemOperand location = VarOperand(var, x1);
    if (FLAG_debug_code && var->mode() == LET && op == Token::INIT) {
      __ Ldr(x10, location);
      __ CompareRoot(x10, Heap::kTheHoleValueRootIndex);
      __ Check(eq, kLetBindingReInitialization);
    }
    EmitStoreToStackLocalOrContextSlot(var, location);
  }
}


void FullCodeGenerator::EmitNamedPropertyAssignment(Assignment* expr) {
  ASM_LOCATION("FullCodeGenerator::EmitNamedPropertyAssignment");
  // Assignment to a property, using a named store IC.
  Property* prop = expr->target()->AsProperty();
  DCHECK(prop != NULL);
  DCHECK(prop->key()->IsLiteral());

  PopOperand(StoreDescriptor::ReceiverRegister());
  CallStoreIC(expr->AssignmentSlot(), prop->key()->AsLiteral()->value());

  PrepareForBailoutForId(expr->AssignmentId(), BailoutState::TOS_REGISTER);
  context()->Plug(x0);
}


void FullCodeGenerator::EmitKeyedPropertyAssignment(Assignment* expr) {
  ASM_LOCATION("FullCodeGenerator::EmitKeyedPropertyAssignment");
  // Assignment to a property, using a keyed store IC.

  // TODO(all): Could we pass this in registers rather than on the stack?
  PopOperands(StoreDescriptor::NameRegister(),
              StoreDescriptor::ReceiverRegister());
  DCHECK(StoreDescriptor::ValueRegister().is(x0));

  CallKeyedStoreIC(expr->AssignmentSlot());

  PrepareForBailoutForId(expr->AssignmentId(), BailoutState::TOS_REGISTER);
  context()->Plug(x0);
}

// Code common for calls using the IC.
void FullCodeGenerator::EmitCallWithLoadIC(Call* expr) {
  ASM_LOCATION("FullCodeGenerator::EmitCallWithLoadIC");
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
    {
      UseScratchRegisterScope temps(masm_);
      Register temp = temps.AcquireX();
      __ LoadRoot(temp, Heap::kUndefinedValueRootIndex);
      PushOperand(temp);
    }
    convert_mode = ConvertReceiverMode::kNullOrUndefined;
  } else {
    // Load the function from the receiver.
    DCHECK(callee->IsProperty());
    DCHECK(!callee->AsProperty()->IsSuperAccess());
    __ Peek(LoadDescriptor::ReceiverRegister(), 0);
    EmitNamedPropertyLoad(callee->AsProperty());
    PrepareForBailoutForId(callee->AsProperty()->LoadId(),
                           BailoutState::TOS_REGISTER);
    // Push the target function under the receiver.
    PopOperand(x10);
    PushOperands(x0, x10);
    convert_mode = ConvertReceiverMode::kNotNullOrUndefined;
  }

  EmitCall(expr, convert_mode);
}


// Code common for calls using the IC.
void FullCodeGenerator::EmitKeyedCallWithLoadIC(Call* expr,
                                                Expression* key) {
  ASM_LOCATION("FullCodeGenerator::EmitKeyedCallWithLoadIC");
  // Load the key.
  VisitForAccumulatorValue(key);

  Expression* callee = expr->expression();

  // Load the function from the receiver.
  DCHECK(callee->IsProperty());
  __ Peek(LoadDescriptor::ReceiverRegister(), 0);
  __ Move(LoadDescriptor::NameRegister(), x0);
  EmitKeyedPropertyLoad(callee->AsProperty());
  PrepareForBailoutForId(callee->AsProperty()->LoadId(),
                         BailoutState::TOS_REGISTER);

  // Push the target function under the receiver.
  PopOperand(x10);
  PushOperands(x0, x10);

  EmitCall(expr, ConvertReceiverMode::kNotNullOrUndefined);
}


void FullCodeGenerator::EmitCall(Call* expr, ConvertReceiverMode mode) {
  ASM_LOCATION("FullCodeGenerator::EmitCall");
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
  __ Mov(x3, SmiFromSlot(expr->CallFeedbackICSlot()));
  __ Peek(x1, (arg_count + 1) * kXRegSize);
  __ Mov(x0, arg_count);
  CallIC(code);
  OperandStackDepthDecrement(arg_count + 1);

  RecordJSReturnSite(expr);
  RestoreContext();
  context()->DropAndPlug(1, x0);
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

  // Load function and argument count into x1 and x0.
  __ Mov(x0, arg_count);
  __ Peek(x1, arg_count * kXRegSize);

  // Record call targets in unoptimized code.
  __ EmitLoadFeedbackVector(x2);
  __ Mov(x3, SmiFromSlot(expr->CallNewFeedbackSlot()));

  CallConstructStub stub(isolate());
  CallIC(stub.GetCode());
  OperandStackDepthDecrement(arg_count + 1);
  PrepareForBailoutForId(expr->ReturnId(), BailoutState::TOS_REGISTER);
  RestoreContext();
  context()->Plug(x0);
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
  __ TestAndSplit(x0, kSmiTagMask, if_true, if_false, fall_through);

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

  __ JumpIfSmi(x0, if_false);
  __ CompareObjectType(x0, x10, x11, FIRST_JS_RECEIVER_TYPE);
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
  context()->PrepareTest(&materialize_true, &materialize_false,
                         &if_true, &if_false, &fall_through);

  __ JumpIfSmi(x0, if_false);
  __ CompareObjectType(x0, x10, x11, JS_ARRAY_TYPE);
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

  __ JumpIfSmi(x0, if_false);
  __ CompareObjectType(x0, x10, x11, JS_TYPED_ARRAY_TYPE);
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

  __ JumpIfSmi(x0, if_false);
  __ CompareObjectType(x0, x10, x11, JS_PROXY_TYPE);
  PrepareForBailoutBeforeSplit(expr, true, if_true, if_false);
  Split(eq, if_true, if_false, fall_through);

  context()->Plug(if_true, if_false);
}


void FullCodeGenerator::EmitClassOf(CallRuntime* expr) {
  ASM_LOCATION("FullCodeGenerator::EmitClassOf");
  ZoneList<Expression*>* args = expr->arguments();
  DCHECK(args->length() == 1);
  Label done, null, function, non_function_constructor;

  VisitForAccumulatorValue(args->at(0));

  // If the object is not a JSReceiver, we return null.
  __ JumpIfSmi(x0, &null);
  STATIC_ASSERT(LAST_JS_RECEIVER_TYPE == LAST_TYPE);
  __ CompareObjectType(x0, x10, x11, FIRST_JS_RECEIVER_TYPE);
  // x10: object's map.
  // x11: object's type.
  __ B(lt, &null);

  // Return 'Function' for JSFunction objects.
  __ Cmp(x11, FIRST_FUNCTION_TYPE);
  STATIC_ASSERT(LAST_FUNCTION_TYPE == LAST_TYPE);
  __ B(hs, &function);

  // Check if the constructor in the map is a JS function.
  Register instance_type = x14;
  __ GetMapConstructor(x12, x10, x13, instance_type);
  __ Cmp(instance_type, JS_FUNCTION_TYPE);
  __ B(ne, &non_function_constructor);

  // x12 now contains the constructor function. Grab the
  // instance class name from there.
  __ Ldr(x13, FieldMemOperand(x12, JSFunction::kSharedFunctionInfoOffset));
  __ Ldr(x0,
         FieldMemOperand(x13, SharedFunctionInfo::kInstanceClassNameOffset));
  __ B(&done);

  // Functions have class 'Function'.
  __ Bind(&function);
  __ LoadRoot(x0, Heap::kFunction_stringRootIndex);
  __ B(&done);

  // Objects with a non-function constructor have class 'Object'.
  __ Bind(&non_function_constructor);
  __ LoadRoot(x0, Heap::kObject_stringRootIndex);
  __ B(&done);

  // Non-JS objects have class null.
  __ Bind(&null);
  __ LoadRoot(x0, Heap::kNullValueRootIndex);

  // All done.
  __ Bind(&done);

  context()->Plug(x0);
}


void FullCodeGenerator::EmitStringCharCodeAt(CallRuntime* expr) {
  ZoneList<Expression*>* args = expr->arguments();
  DCHECK(args->length() == 2);

  VisitForStackValue(args->at(0));
  VisitForAccumulatorValue(args->at(1));

  Register object = x1;
  Register index = x0;
  Register result = x3;

  PopOperand(object);

  Label need_conversion;
  Label index_out_of_range;
  Label done;
  StringCharCodeAtGenerator generator(object, index, result, &need_conversion,
                                      &need_conversion, &index_out_of_range);
  generator.GenerateFast(masm_);
  __ B(&done);

  __ Bind(&index_out_of_range);
  // When the index is out of range, the spec requires us to return NaN.
  __ LoadRoot(result, Heap::kNanValueRootIndex);
  __ B(&done);

  __ Bind(&need_conversion);
  // Load the undefined value into the result register, which will
  // trigger conversion.
  __ LoadRoot(result, Heap::kUndefinedValueRootIndex);
  __ B(&done);

  NopRuntimeCallHelper call_helper;
  generator.GenerateSlow(masm_, NOT_PART_OF_IC_HANDLER, call_helper);

  __ Bind(&done);
  context()->Plug(result);
}


void FullCodeGenerator::EmitCall(CallRuntime* expr) {
  ASM_LOCATION("FullCodeGenerator::EmitCall");
  ZoneList<Expression*>* args = expr->arguments();
  DCHECK_LE(2, args->length());
  // Push target, receiver and arguments onto the stack.
  for (Expression* const arg : *args) {
    VisitForStackValue(arg);
  }
  PrepareForBailoutForId(expr->CallId(), BailoutState::NO_REGISTERS);
  // Move target to x1.
  int const argc = args->length() - 2;
  __ Peek(x1, (argc + 1) * kXRegSize);
  // Call the target.
  __ Mov(x0, argc);
  __ Call(isolate()->builtins()->Call(), RelocInfo::CODE_TARGET);
  OperandStackDepthDecrement(argc + 1);
  RestoreContext();
  // Discard the function left on TOS.
  context()->DropAndPlug(1, x0);
}

void FullCodeGenerator::EmitGetSuperConstructor(CallRuntime* expr) {
  ZoneList<Expression*>* args = expr->arguments();
  DCHECK_EQ(1, args->length());
  VisitForAccumulatorValue(args->at(0));
  __ AssertFunction(x0);
  __ Ldr(x0, FieldMemOperand(x0, HeapObject::kMapOffset));
  __ Ldr(x0, FieldMemOperand(x0, Map::kPrototypeOffset));
  context()->Plug(x0);
}

void FullCodeGenerator::EmitDebugIsActive(CallRuntime* expr) {
  DCHECK(expr->arguments()->length() == 0);
  ExternalReference debug_is_active =
      ExternalReference::debug_is_active_address(isolate());
  __ Mov(x10, debug_is_active);
  __ Ldrb(x0, MemOperand(x10));
  __ SmiTag(x0);
  context()->Plug(x0);
}


void FullCodeGenerator::EmitCreateIterResultObject(CallRuntime* expr) {
  ZoneList<Expression*>* args = expr->arguments();
  DCHECK_EQ(2, args->length());
  VisitForStackValue(args->at(0));
  VisitForStackValue(args->at(1));

  Label runtime, done;

  Register result = x0;
  __ Allocate(JSIteratorResult::kSize, result, x10, x11, &runtime,
              NO_ALLOCATION_FLAGS);
  Register map_reg = x1;
  Register result_value = x2;
  Register boolean_done = x3;
  Register empty_fixed_array = x4;
  Register untagged_result = x5;
  __ LoadNativeContextSlot(Context::ITERATOR_RESULT_MAP_INDEX, map_reg);
  __ Pop(boolean_done);
  __ Pop(result_value);
  __ LoadRoot(empty_fixed_array, Heap::kEmptyFixedArrayRootIndex);
  STATIC_ASSERT(JSObject::kPropertiesOffset + kPointerSize ==
                JSObject::kElementsOffset);
  STATIC_ASSERT(JSIteratorResult::kValueOffset + kPointerSize ==
                JSIteratorResult::kDoneOffset);
  __ ObjectUntag(untagged_result, result);
  __ Str(map_reg, MemOperand(untagged_result, HeapObject::kMapOffset));
  __ Stp(empty_fixed_array, empty_fixed_array,
         MemOperand(untagged_result, JSObject::kPropertiesOffset));
  __ Stp(result_value, boolean_done,
         MemOperand(untagged_result, JSIteratorResult::kValueOffset));
  STATIC_ASSERT(JSIteratorResult::kSize == 5 * kPointerSize);
  __ B(&done);

  __ Bind(&runtime);
  CallRuntimeWithOperands(Runtime::kCreateIterResultObject);

  __ Bind(&done);
  context()->Plug(x0);
}


void FullCodeGenerator::EmitLoadJSRuntimeFunction(CallRuntime* expr) {
  // Push function.
  __ LoadNativeContextSlot(expr->context_index(), x0);
  PushOperand(x0);

  // Push undefined as the receiver.
  __ LoadRoot(x0, Heap::kUndefinedValueRootIndex);
  PushOperand(x0);
}


void FullCodeGenerator::EmitCallJSRuntimeFunction(CallRuntime* expr) {
  ZoneList<Expression*>* args = expr->arguments();
  int arg_count = args->length();

  SetCallPosition(expr);
  __ Peek(x1, (arg_count + 1) * kPointerSize);
  __ Mov(x0, arg_count);
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
        context()->Plug(x0);
      } else if (proxy != NULL) {
        Variable* var = proxy->var();
        // Delete of an unqualified identifier is disallowed in strict mode but
        // "delete this" is allowed.
        bool is_this = var->is_this();
        DCHECK(is_sloppy(language_mode()) || is_this);
        if (var->IsUnallocated()) {
          __ LoadGlobalObject(x12);
          __ Mov(x11, Operand(var->name()));
          __ Push(x12, x11);
          __ CallRuntime(Runtime::kDeleteProperty_Sloppy);
          context()->Plug(x0);
        } else {
          DCHECK(!var->IsLookupSlot());
          DCHECK(var->IsStackAllocated() || var->IsContextSlot());
          // Result of deleting non-global, non-dynamic variables is false.
          // The subexpression does not have side effects.
          context()->Plug(is_this);
        }
      } else {
        // Result of deleting non-property, non-variable reference is true.
        // The subexpression may have side effects.
        VisitForEffect(expr->expression());
        context()->Plug(true);
      }
      break;
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
        DCHECK(context()->IsAccumulatorValue() || context()->IsStackValue());
        // TODO(jbramley): This could be much more efficient using (for
        // example) the CSEL instruction.
        Label materialize_true, materialize_false, done;
        VisitForControl(expr->expression(),
                        &materialize_false,
                        &materialize_true,
                        &materialize_true);
        if (!context()->IsAccumulatorValue()) OperandStackDepthIncrement(1);

        __ Bind(&materialize_true);
        PrepareForBailoutForId(expr->MaterializeTrueId(),
                               BailoutState::NO_REGISTERS);
        __ LoadRoot(result_register(), Heap::kTrueValueRootIndex);
        __ B(&done);

        __ Bind(&materialize_false);
        PrepareForBailoutForId(expr->MaterializeFalseId(),
                               BailoutState::NO_REGISTERS);
        __ LoadRoot(result_register(), Heap::kFalseValueRootIndex);
        __ B(&done);

        __ Bind(&done);
        if (context()->IsStackValue()) {
          __ Push(result_register());
        }
      }
      break;
    }
    case Token::TYPEOF: {
      Comment cmnt(masm_, "[ UnaryOperation (TYPEOF)");
      {
        AccumulatorValueContext context(this);
        VisitForTypeofValue(expr->expression());
      }
      __ Mov(x3, x0);
      __ Call(isolate()->builtins()->Typeof(), RelocInfo::CODE_TARGET);
      context()->Plug(x0);
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
      PushOperand(xzr);
    }
    switch (assign_type) {
      case NAMED_PROPERTY: {
        // Put the object both on the stack and in the register.
        VisitForStackValue(prop->obj());
        __ Peek(LoadDescriptor::ReceiverRegister(), 0);
        EmitNamedPropertyLoad(prop);
        break;
      }

      case KEYED_PROPERTY: {
        VisitForStackValue(prop->obj());
        VisitForStackValue(prop->key());
        __ Peek(LoadDescriptor::ReceiverRegister(), 1 * kPointerSize);
        __ Peek(LoadDescriptor::NameRegister(), 0);
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
  Label stub_call, done;
  JumpPatchSite patch_site(masm_);

  int count_value = expr->op() == Token::INC ? 1 : -1;
  if (ShouldInlineSmiCase(expr->op())) {
    Label slow;
    patch_site.EmitJumpIfNotSmi(x0, &slow);

    // Save result for postfix expressions.
    if (expr->is_postfix()) {
      if (!context()->IsEffect()) {
        // Save the result on the stack. If we have a named or keyed property we
        // store the result under the receiver that is currently on top of the
        // stack.
        switch (assign_type) {
          case VARIABLE:
            __ Push(x0);
            break;
          case NAMED_PROPERTY:
            __ Poke(x0, kPointerSize);
            break;
          case KEYED_PROPERTY:
            __ Poke(x0, kPointerSize * 2);
            break;
          case NAMED_SUPER_PROPERTY:
          case KEYED_SUPER_PROPERTY:
            UNREACHABLE();
            break;
        }
      }
    }

    __ Adds(x0, x0, Smi::FromInt(count_value));
    __ B(vc, &done);
    // Call stub. Undo operation first.
    __ Sub(x0, x0, Smi::FromInt(count_value));
    __ B(&stub_call);
    __ Bind(&slow);
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
          PushOperand(x0);
          break;
        case NAMED_PROPERTY:
          __ Poke(x0, kXRegSize);
          break;
        case KEYED_PROPERTY:
          __ Poke(x0, 2 * kXRegSize);
          break;
        case NAMED_SUPER_PROPERTY:
        case KEYED_SUPER_PROPERTY:
          UNREACHABLE();
          break;
      }
    }
  }

  __ Bind(&stub_call);
  __ Mov(x1, x0);
  __ Mov(x0, Smi::FromInt(count_value));

  SetExpressionPosition(expr);

  {
    Assembler::BlockPoolsScope scope(masm_);
    Handle<Code> code = CodeFactory::BinaryOpIC(isolate(), Token::ADD).code();
    CallIC(code, expr->CountBinOpFeedbackId());
    patch_site.EmitPatchInfo();
  }
  __ Bind(&done);

  // Store the value returned in x0.
  switch (assign_type) {
    case VARIABLE: {
      VariableProxy* proxy = expr->expression()->AsVariableProxy();
      if (expr->is_postfix()) {
        { EffectContext context(this);
          EmitVariableAssignment(proxy->var(), Token::ASSIGN, expr->CountSlot(),
                                 proxy->hole_check_mode());
          PrepareForBailoutForId(expr->AssignmentId(),
                                 BailoutState::TOS_REGISTER);
          context.Plug(x0);
        }
        // For all contexts except EffectConstant We have the result on
        // top of the stack.
        if (!context()->IsEffect()) {
          context()->PlugTOS();
        }
      } else {
        EmitVariableAssignment(proxy->var(), Token::ASSIGN, expr->CountSlot(),
                               proxy->hole_check_mode());
        PrepareForBailoutForId(expr->AssignmentId(),
                               BailoutState::TOS_REGISTER);
        context()->Plug(x0);
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
        context()->Plug(x0);
      }
      break;
    }
    case KEYED_PROPERTY: {
      PopOperand(StoreDescriptor::NameRegister());
      PopOperand(StoreDescriptor::ReceiverRegister());
      CallKeyedStoreIC(expr->CountSlot());
      PrepareForBailoutForId(expr->AssignmentId(), BailoutState::TOS_REGISTER);
      if (expr->is_postfix()) {
        if (!context()->IsEffect()) {
          context()->PlugTOS();
        }
      } else {
        context()->Plug(x0);
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
  ASM_LOCATION("FullCodeGenerator::EmitLiteralCompareTypeof");
  Comment cmnt(masm_, "[ EmitLiteralCompareTypeof");
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
    ASM_LOCATION("FullCodeGenerator::EmitLiteralCompareTypeof number_string");
    __ JumpIfSmi(x0, if_true);
    __ Ldr(x0, FieldMemOperand(x0, HeapObject::kMapOffset));
    __ CompareRoot(x0, Heap::kHeapNumberMapRootIndex);
    Split(eq, if_true, if_false, fall_through);
  } else if (String::Equals(check, factory->string_string())) {
    ASM_LOCATION("FullCodeGenerator::EmitLiteralCompareTypeof string_string");
    __ JumpIfSmi(x0, if_false);
    __ CompareObjectType(x0, x0, x1, FIRST_NONSTRING_TYPE);
    Split(lt, if_true, if_false, fall_through);
  } else if (String::Equals(check, factory->symbol_string())) {
    ASM_LOCATION("FullCodeGenerator::EmitLiteralCompareTypeof symbol_string");
    __ JumpIfSmi(x0, if_false);
    __ CompareObjectType(x0, x0, x1, SYMBOL_TYPE);
    Split(eq, if_true, if_false, fall_through);
  } else if (String::Equals(check, factory->boolean_string())) {
    ASM_LOCATION("FullCodeGenerator::EmitLiteralCompareTypeof boolean_string");
    __ JumpIfRoot(x0, Heap::kTrueValueRootIndex, if_true);
    __ CompareRoot(x0, Heap::kFalseValueRootIndex);
    Split(eq, if_true, if_false, fall_through);
  } else if (String::Equals(check, factory->undefined_string())) {
    ASM_LOCATION(
        "FullCodeGenerator::EmitLiteralCompareTypeof undefined_string");
    __ JumpIfRoot(x0, Heap::kNullValueRootIndex, if_false);
    __ JumpIfSmi(x0, if_false);
    // Check for undetectable objects => true.
    __ Ldr(x0, FieldMemOperand(x0, HeapObject::kMapOffset));
    __ Ldrb(x1, FieldMemOperand(x0, Map::kBitFieldOffset));
    __ TestAndSplit(x1, 1 << Map::kIsUndetectable, if_false, if_true,
                    fall_through);
  } else if (String::Equals(check, factory->function_string())) {
    ASM_LOCATION("FullCodeGenerator::EmitLiteralCompareTypeof function_string");
    __ JumpIfSmi(x0, if_false);
    __ Ldr(x0, FieldMemOperand(x0, HeapObject::kMapOffset));
    __ Ldrb(x1, FieldMemOperand(x0, Map::kBitFieldOffset));
    __ And(x1, x1, (1 << Map::kIsCallable) | (1 << Map::kIsUndetectable));
    __ CompareAndSplit(x1, Operand(1 << Map::kIsCallable), eq, if_true,
                       if_false, fall_through);
  } else if (String::Equals(check, factory->object_string())) {
    ASM_LOCATION("FullCodeGenerator::EmitLiteralCompareTypeof object_string");
    __ JumpIfSmi(x0, if_false);
    __ JumpIfRoot(x0, Heap::kNullValueRootIndex, if_true);
    STATIC_ASSERT(LAST_JS_RECEIVER_TYPE == LAST_TYPE);
    __ JumpIfObjectType(x0, x10, x11, FIRST_JS_RECEIVER_TYPE, if_false, lt);
    // Check for callable or undetectable objects => false.
    __ Ldrb(x10, FieldMemOperand(x10, Map::kBitFieldOffset));
    __ TestAndSplit(x10, (1 << Map::kIsCallable) | (1 << Map::kIsUndetectable),
                    if_true, if_false, fall_through);
// clang-format off
#define SIMD128_TYPE(TYPE, Type, type, lane_count, lane_type)   \
  } else if (String::Equals(check, factory->type##_string())) { \
    ASM_LOCATION("FullCodeGenerator::EmitLiteralCompareTypeof " \
                 #type "_string");                              \
    __ JumpIfSmi(x0, if_true);                                  \
    __ Ldr(x0, FieldMemOperand(x0, HeapObject::kMapOffset));    \
    __ CompareRoot(x0, Heap::k##Type##MapRootIndex);            \
    Split(eq, if_true, if_false, fall_through);
  SIMD128_TYPES(SIMD128_TYPE)
#undef SIMD128_TYPE
    // clang-format on
  } else {
    ASM_LOCATION("FullCodeGenerator::EmitLiteralCompareTypeof other");
    if (if_false != fall_through) __ B(if_false);
  }
  context()->Plug(if_true, if_false);
}


void FullCodeGenerator::VisitCompareOperation(CompareOperation* expr) {
  Comment cmnt(masm_, "[ CompareOperation");

  // Try to generate an optimized comparison with a literal value.
  // TODO(jbramley): This only checks common values like NaN or undefined.
  // Should it also handle ARM64 immediate operands?
  if (TryLiteralCompare(expr)) {
    return;
  }

  // Assign labels according to context()->PrepareTest.
  Label materialize_true;
  Label materialize_false;
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
      __ CompareRoot(x0, Heap::kTrueValueRootIndex);
      Split(eq, if_true, if_false, fall_through);
      break;

    case Token::INSTANCEOF: {
      VisitForAccumulatorValue(expr->right());
      SetExpressionPosition(expr);
      PopOperand(x1);
      __ Call(isolate()->builtins()->InstanceOf(), RelocInfo::CODE_TARGET);
      PrepareForBailoutBeforeSplit(expr, false, NULL, NULL);
      __ CompareRoot(x0, Heap::kTrueValueRootIndex);
      Split(eq, if_true, if_false, fall_through);
      break;
    }

    default: {
      VisitForAccumulatorValue(expr->right());
      SetExpressionPosition(expr);
      Condition cond = CompareIC::ComputeCondition(op);

      // Pop the stack value.
      PopOperand(x1);

      JumpPatchSite patch_site(masm_);
      if (ShouldInlineSmiCase(op)) {
        Label slow_case;
        patch_site.EmitJumpIfEitherNotSmi(x0, x1, &slow_case);
        __ Cmp(x1, x0);
        Split(cond, if_true, if_false, NULL);
        __ Bind(&slow_case);
      }

      Handle<Code> ic = CodeFactory::CompareIC(isolate(), op).code();
      CallIC(ic, expr->CompareOperationFeedbackId());
      patch_site.EmitPatchInfo();
      PrepareForBailoutBeforeSplit(expr, true, if_true, if_false);
      __ CompareAndSplit(x0, 0, cond, if_true, if_false, fall_through);
    }
  }

  // Convert the result of the comparison into one expected for this
  // expression's context.
  context()->Plug(if_true, if_false);
}


void FullCodeGenerator::EmitLiteralCompareNil(CompareOperation* expr,
                                              Expression* sub_expr,
                                              NilValue nil) {
  ASM_LOCATION("FullCodeGenerator::EmitLiteralCompareNil");
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
    __ CompareRoot(x0, nil_value);
    Split(eq, if_true, if_false, fall_through);
  } else {
    __ JumpIfSmi(x0, if_false);
    __ Ldr(x0, FieldMemOperand(x0, HeapObject::kMapOffset));
    __ Ldrb(x1, FieldMemOperand(x0, Map::kBitFieldOffset));
    __ TestAndSplit(x1, 1 << Map::kIsUndetectable, if_false, if_true,
                    fall_through);
  }

  context()->Plug(if_true, if_false);
}


void FullCodeGenerator::VisitYield(Yield* expr) {
  // Resumable functions are not supported.
  UNREACHABLE();
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

void FullCodeGenerator::PopOperands(Register reg1, Register reg2) {
  OperandStackDepthDecrement(2);
  __ Pop(reg1, reg2);
}

void FullCodeGenerator::EmitOperandStackDepthCheck() {
  if (FLAG_debug_code) {
    int expected_diff = StandardFrameConstants::kFixedFrameSizeFromFp +
                        operand_stack_depth_ * kPointerSize;
    __ Sub(x0, fp, jssp);
    __ Cmp(x0, Operand(expected_diff));
    __ Assert(eq, kUnexpectedStackDepth);
  }
}

void FullCodeGenerator::EmitCreateIteratorResult(bool done) {
  Label allocate, done_allocate;

  // Allocate and populate an object with this form: { value: VAL, done: DONE }

  Register result = x0;
  __ Allocate(JSIteratorResult::kSize, result, x10, x11, &allocate,
              NO_ALLOCATION_FLAGS);
  __ B(&done_allocate);

  __ Bind(&allocate);
  __ Push(Smi::FromInt(JSIteratorResult::kSize));
  __ CallRuntime(Runtime::kAllocateInNewSpace);

  __ Bind(&done_allocate);
  Register map_reg = x1;
  Register result_value = x2;
  Register boolean_done = x3;
  Register empty_fixed_array = x4;
  Register untagged_result = x5;
  __ LoadNativeContextSlot(Context::ITERATOR_RESULT_MAP_INDEX, map_reg);
  PopOperand(result_value);
  __ LoadRoot(boolean_done,
              done ? Heap::kTrueValueRootIndex : Heap::kFalseValueRootIndex);
  __ LoadRoot(empty_fixed_array, Heap::kEmptyFixedArrayRootIndex);
  STATIC_ASSERT(JSObject::kPropertiesOffset + kPointerSize ==
                JSObject::kElementsOffset);
  STATIC_ASSERT(JSIteratorResult::kValueOffset + kPointerSize ==
                JSIteratorResult::kDoneOffset);
  __ ObjectUntag(untagged_result, result);
  __ Str(map_reg, MemOperand(untagged_result, HeapObject::kMapOffset));
  __ Stp(empty_fixed_array, empty_fixed_array,
         MemOperand(untagged_result, JSObject::kPropertiesOffset));
  __ Stp(result_value, boolean_done,
         MemOperand(untagged_result, JSIteratorResult::kValueOffset));
  STATIC_ASSERT(JSIteratorResult::kSize == 5 * kPointerSize);
}


// TODO(all): I don't like this method.
// It seems to me that in too many places x0 is used in place of this.
// Also, this function is not suitable for all places where x0 should be
// abstracted (eg. when used as an argument). But some places assume that the
// first argument register is x0, and use this function instead.
// Considering that most of the register allocation is hard-coded in the
// FullCodeGen, that it is unlikely we will need to change it extensively, and
// that abstracting the allocation through functions would not yield any
// performance benefit, I think the existence of this function is debatable.
Register FullCodeGenerator::result_register() {
  return x0;
}


Register FullCodeGenerator::context_register() {
  return cp;
}

void FullCodeGenerator::LoadFromFrameField(int frame_offset, Register value) {
  DCHECK(POINTER_SIZE_ALIGN(frame_offset) == frame_offset);
  __ Ldr(value, MemOperand(fp, frame_offset));
}

void FullCodeGenerator::StoreToFrameField(int frame_offset, Register value) {
  DCHECK(POINTER_SIZE_ALIGN(frame_offset) == frame_offset);
  __ Str(value, MemOperand(fp, frame_offset));
}


void FullCodeGenerator::LoadContextField(Register dst, int context_index) {
  __ Ldr(dst, ContextMemOperand(cp, context_index));
}


void FullCodeGenerator::PushFunctionArgumentForContextAllocation() {
  DeclarationScope* closure_scope = scope()->GetClosureScope();
  if (closure_scope->is_script_scope() ||
      closure_scope->is_module_scope()) {
    // Contexts nested in the native context have a canonical empty function
    // as their closure, not the anonymous closure containing the global
    // code.
    DCHECK(kSmiTag == 0);
    __ LoadNativeContextSlot(Context::CLOSURE_INDEX, x10);
  } else if (closure_scope->is_eval_scope()) {
    // Contexts created by a call to eval have the same closure as the
    // context calling eval, not the anonymous closure containing the eval
    // code.  Fetch it from the context.
    __ Ldr(x10, ContextMemOperand(cp, Context::CLOSURE_INDEX));
  } else {
    DCHECK(closure_scope->is_function_scope());
    __ Ldr(x10, MemOperand(fp, JavaScriptFrameConstants::kFunctionOffset));
  }
  PushOperand(x10);
}


#undef __


void BackEdgeTable::PatchAt(Code* unoptimized_code,
                            Address pc,
                            BackEdgeState target_state,
                            Code* replacement_code) {
  // Turn the jump into a nop.
  Address branch_address = pc - 3 * kInstructionSize;
  Isolate* isolate = unoptimized_code->GetIsolate();
  PatchingAssembler patcher(isolate, branch_address, 1);

  DCHECK(Instruction::Cast(branch_address)
             ->IsNop(Assembler::INTERRUPT_CODE_NOP) ||
         (Instruction::Cast(branch_address)->IsCondBranchImm() &&
          Instruction::Cast(branch_address)->ImmPCOffset() ==
              6 * kInstructionSize));

  switch (target_state) {
    case INTERRUPT:
      //  <decrement profiling counter>
      //  .. .. .. ..       b.pl ok
      //  .. .. .. ..       ldr x16, pc+<interrupt stub address>
      //  .. .. .. ..       blr x16
      //  ... more instructions.
      //  ok-label
      // Jump offset is 6 instructions.
      patcher.b(6, pl);
      break;
    case ON_STACK_REPLACEMENT:
      //  <decrement profiling counter>
      //  .. .. .. ..       mov x0, x0 (NOP)
      //  .. .. .. ..       ldr x16, pc+<on-stack replacement address>
      //  .. .. .. ..       blr x16
      patcher.nop(Assembler::INTERRUPT_CODE_NOP);
      break;
  }

  // Replace the call address.
  Instruction* load = Instruction::Cast(pc)->preceding(2);
  Address interrupt_address_pointer =
      reinterpret_cast<Address>(load) + load->ImmPCOffset();
  DCHECK((Memory::uint64_at(interrupt_address_pointer) ==
          reinterpret_cast<uint64_t>(
              isolate->builtins()->OnStackReplacement()->entry())) ||
         (Memory::uint64_at(interrupt_address_pointer) ==
          reinterpret_cast<uint64_t>(
              isolate->builtins()->InterruptCheck()->entry())) ||
         (Memory::uint64_at(interrupt_address_pointer) ==
          reinterpret_cast<uint64_t>(
              isolate->builtins()->OnStackReplacement()->entry())));
  Memory::uint64_at(interrupt_address_pointer) =
      reinterpret_cast<uint64_t>(replacement_code->entry());

  unoptimized_code->GetHeap()->incremental_marking()->RecordCodeTargetPatch(
      unoptimized_code, reinterpret_cast<Address>(load), replacement_code);
}


BackEdgeTable::BackEdgeState BackEdgeTable::GetBackEdgeState(
    Isolate* isolate,
    Code* unoptimized_code,
    Address pc) {
  // TODO(jbramley): There should be some extra assertions here (as in the ARM
  // back-end), but this function is gone in bleeding_edge so it might not
  // matter anyway.
  Instruction* jump_or_nop = Instruction::Cast(pc)->preceding(3);

  if (jump_or_nop->IsNop(Assembler::INTERRUPT_CODE_NOP)) {
    Instruction* load = Instruction::Cast(pc)->preceding(2);
    uint64_t entry = Memory::uint64_at(reinterpret_cast<Address>(load) +
                                       load->ImmPCOffset());
    if (entry == reinterpret_cast<uint64_t>(
        isolate->builtins()->OnStackReplacement()->entry())) {
      return ON_STACK_REPLACEMENT;
    } else {
      UNREACHABLE();
    }
  }

  return INTERRUPT;
}


}  // namespace internal
}  // namespace v8

#endif  // V8_TARGET_ARCH_ARM64
