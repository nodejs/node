// Copyright 2013 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if V8_TARGET_ARCH_ARM64

#include "src/ast/scopes.h"
#include "src/code-factory.h"
#include "src/code-stubs.h"
#include "src/codegen.h"
#include "src/debug/debug.h"
#include "src/full-codegen/full-codegen.h"
#include "src/ic/ic.h"
#include "src/parsing/parser.h"

#include "src/arm64/code-stubs-arm64.h"
#include "src/arm64/frames-arm64.h"
#include "src/arm64/macro-assembler-arm64.h"

namespace v8 {
namespace internal {

#define __ ACCESS_MASM(masm_)

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

#ifdef DEBUG
  if (strlen(FLAG_stop_at) > 0 &&
      info->literal()->name()->IsUtf8EqualTo(CStrVector(FLAG_stop_at))) {
    __ Debug("stop-at", __LINE__, BREAK);
  }
#endif

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

  // Reserve space on the stack for locals.
  { Comment cmnt(masm_, "[ Allocate locals");
    int locals_count = info->scope()->num_stack_slots();
    // Generators allocate locals, if any, in context slots.
    DCHECK(!IsGeneratorFunction(info->literal()->kind()) || locals_count == 0);

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

  if (info->scope()->num_heap_slots() > 0) {
    // Argument to NewContext is the function, which is still in x1.
    Comment cmnt(masm_, "[ Allocate context");
    bool need_write_barrier = true;
    int slots = info->scope()->num_heap_slots() - Context::MIN_CONTEXT_SLOTS;
    if (info->scope()->is_script_scope()) {
      __ Mov(x10, Operand(info->scope()->GetScopeInfo(info->isolate())));
      __ Push(x1, x10);
      __ CallRuntime(Runtime::kNewScriptContext);
      PrepareForBailoutForId(BailoutId::ScriptContext(), TOS_REG);
      // The new target value is not used, clobbering is safe.
      DCHECK_NULL(info->scope()->new_target_var());
    } else {
      if (info->scope()->new_target_var() != nullptr) {
        __ Push(x3);  // Preserve new target.
      }
      if (slots <= FastNewContextStub::kMaximumSlots) {
        FastNewContextStub stub(isolate(), slots);
        __ CallStub(&stub);
        // Result of FastNewContextStub is always in new space.
        need_write_barrier = false;
      } else {
        __ Push(x1);
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
      Variable* var = (i == -1) ? scope()->receiver() : scope()->parameter(i);
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
  PrepareForBailoutForId(BailoutId::FunctionContext(), NO_REGISTERS);

  // Possibly set up a local binding to the this function which is used in
  // derived constructors with super calls.
  Variable* this_function_var = scope()->this_function_var();
  if (this_function_var != nullptr) {
    Comment cmnt(masm_, "[ This function");
    if (!function_in_register_x1) {
      __ Ldr(x1, MemOperand(fp, JavaScriptFrameConstants::kFunctionOffset));
      // The write barrier clobbers register again, keep it marked as such.
    }
    SetVar(this_function_var, x1, x0, x2);
  }

  // Possibly set up a local binding to the new target value.
  Variable* new_target_var = scope()->new_target_var();
  if (new_target_var != nullptr) {
    Comment cmnt(masm_, "[ new.target");
    SetVar(new_target_var, x3, x0, x2);
  }

  // Possibly allocate RestParameters
  int rest_index;
  Variable* rest_param = scope()->rest_parameter(&rest_index);
  if (rest_param) {
    Comment cmnt(masm_, "[ Allocate rest parameter array");

    int num_parameters = info->scope()->num_parameters();
    int offset = num_parameters * kPointerSize;
    __ Mov(RestParamAccessDescriptor::parameter_count(),
           Smi::FromInt(num_parameters));
    __ Add(RestParamAccessDescriptor::parameter_pointer(), fp,
           StandardFrameConstants::kCallerSPOffset + offset);
    __ Mov(RestParamAccessDescriptor::rest_parameter_index(),
           Smi::FromInt(rest_index));

    function_in_register_x1 = false;

    RestParamAccessStub stub(isolate());
    __ CallStub(&stub);

    SetVar(rest_param, x0, x1, x2);
  }

  Variable* arguments = scope()->arguments();
  if (arguments != NULL) {
    // Function uses arguments object.
    Comment cmnt(masm_, "[ Allocate arguments object");
    DCHECK(x1.is(ArgumentsAccessNewDescriptor::function()));
    if (!function_in_register_x1) {
      // Load this again, if it's used by the local context below.
      __ Ldr(x1, MemOperand(fp, JavaScriptFrameConstants::kFunctionOffset));
    }
    // Receiver is just before the parameters on the caller's stack.
    int num_parameters = info->scope()->num_parameters();
    int offset = num_parameters * kPointerSize;
    __ Mov(ArgumentsAccessNewDescriptor::parameter_count(),
           Smi::FromInt(num_parameters));
    __ Add(ArgumentsAccessNewDescriptor::parameter_pointer(), fp,
           StandardFrameConstants::kCallerSPOffset + offset);

    // Arguments to ArgumentsAccessStub:
    //   function, parameter pointer, parameter count.
    // The stub will rewrite parameter pointer and parameter count if the
    // previous stack frame was an arguments adapter frame.
    bool is_unmapped = is_strict(language_mode()) || !has_simple_parameters();
    ArgumentsAccessStub::Type type = ArgumentsAccessStub::ComputeType(
        is_unmapped, literal()->has_duplicate_parameters());
    ArgumentsAccessStub stub(isolate(), type);
    __ CallStub(&stub);

    SetVar(arguments, x0, x1, x2);
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

    {
      Comment cmnt(masm_, "[ Stack check");
      PrepareForBailoutForId(BailoutId::Declarations(), NO_REGISTERS);
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


void FullCodeGenerator::ClearAccumulator() {
  __ Mov(x0, Smi::FromInt(0));
}


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
  PrepareForBailoutForId(stmt->EntryId(), NO_REGISTERS);
  // Record a mapping of the OSR id to this PC.  This is used if the OSR
  // entry becomes the target of a bailout.  We don't expect it to be, but
  // we want it to work if it is.
  PrepareForBailoutForId(stmt->OsrEntryId(), NO_REGISTERS);
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
    // Pretend that the exit is a backwards jump to the entry.
    int weight = 1;
    if (info_->ShouldSelfOptimize()) {
      weight = FLAG_interrupt_budget / FLAG_self_opt_count;
    } else {
      int distance = masm_->pc_offset() + kCodeSizeMultiplier / 2;
      weight = Min(kMaxBackEdgeWeight,
                   Max(1, distance / kCodeSizeMultiplier));
    }
    EmitProfilingCounterDecrement(weight);
    Label ok;
    __ B(pl, &ok);
    __ Push(x0);
    __ Call(isolate()->builtins()->InterruptCheck(),
            RelocInfo::CODE_TARGET);
    __ Pop(x0);
    EmitProfilingCounterReset();
    __ Bind(&ok);

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


void FullCodeGenerator::StackValueContext::Plug(Variable* var) const {
  DCHECK(var->IsStackAllocated() || var->IsContextSlot());
  codegen()->GetVar(result_register(), var);
  __ Push(result_register());
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
  __ Push(result_register());
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
  __ Push(result_register());
}


void FullCodeGenerator::TestContext::Plug(Handle<Object> lit) const {
  codegen()->PrepareForBailoutBeforeSplit(condition(),
                                          true,
                                          true_label_,
                                          false_label_);
  DCHECK(!lit->IsUndetectableObject());  // There are no undetectable literals.
  if (lit->IsUndefined() || lit->IsNull() || lit->IsFalse()) {
    if (false_label_ != fall_through_) __ B(false_label_);
  } else if (lit->IsTrue() || lit->IsJSObject()) {
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
  __ Poke(reg, 0);
}


void FullCodeGenerator::TestContext::DropAndPlug(int count,
                                                 Register reg) const {
  DCHECK(count > 0);
  // For simplicity we always test the accumulator register.
  __ Drop(count);
  __ Mov(result_register(), reg);
  codegen()->PrepareForBailoutBeforeSplit(condition(), false, NULL, NULL);
  codegen()->DoTest(this);
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
  __ Push(x10);
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
  __ Push(x10);
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
  Handle<Code> ic = ToBooleanStub::GetUninitialized(isolate());
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
  PrepareForBailout(expr, TOS_REG);
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
  if (generate_debug_code_) {
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
                        : isolate()->factory()->undefined_value(),
                    zone());
      break;

    case VariableLocation::PARAMETER:
    case VariableLocation::LOCAL:
      if (hole_init) {
        Comment cmnt(masm_, "[ VariableDeclaration");
        __ LoadRoot(x10, Heap::kTheHoleValueRootIndex);
        __ Str(x10, StackOperand(variable));
      }
      break;

    case VariableLocation::CONTEXT:
      if (hole_init) {
        Comment cmnt(masm_, "[ VariableDeclaration");
        EmitDebugCheckDeclarationContext(variable);
        __ LoadRoot(x10, Heap::kTheHoleValueRootIndex);
        __ Str(x10, ContextMemOperand(cp, variable->index()));
        // No write barrier since the_hole_value is in old space.
        PrepareForBailoutForId(proxy->id(), NO_REGISTERS);
      }
      break;

    case VariableLocation::LOOKUP: {
      Comment cmnt(masm_, "[ VariableDeclaration");
      __ Mov(x2, Operand(variable->name()));
      // Declaration nodes are always introduced in one of four modes.
      DCHECK(IsDeclaredVariableMode(mode));
      // Push initial value, if any.
      // Note: For variables we must not push an initial value (such as
      // 'undefined') because we may have a (legal) redeclaration and we
      // must not destroy the current value.
      if (hole_init) {
        __ LoadRoot(x0, Heap::kTheHoleValueRootIndex);
        __ Push(x2, x0);
      } else {
        // Pushing 0 (xzr) indicates no initial value.
        __ Push(x2, xzr);
      }
      __ Push(Smi::FromInt(variable->DeclarationPropertyAttributes()));
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
      PrepareForBailoutForId(proxy->id(), NO_REGISTERS);
      break;
    }

    case VariableLocation::LOOKUP: {
      Comment cmnt(masm_, "[ Function Declaration");
      __ Mov(x2, Operand(variable->name()));
      __ Push(x2);
      // Push initial value for function declaration.
      VisitForStackValue(declaration->fun());
      __ Push(Smi::FromInt(variable->DeclarationPropertyAttributes()));
      __ CallRuntime(Runtime::kDeclareLookupSlot);
      break;
    }
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
  __ Push(x11, flags);
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
  ASM_LOCATION("FullCodeGenerator::VisitSwitchStatement");
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
    Handle<Code> ic = CodeFactory::CompareIC(isolate(), Token::EQ_STRICT,
                                             strength(language_mode())).code();
    CallIC(ic, clause->CompareId());
    patch_site.EmitPatchInfo();

    Label skip;
    __ B(&skip);
    PrepareForBailout(clause, TOS_REG);
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
  __ Drop(1);  // Switch value is no longer needed.
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
    PrepareForBailoutForId(clause->EntryId(), NO_REGISTERS);
    VisitStatements(clause->statements());
  }

  __ Bind(nested_statement.break_label());
  PrepareForBailoutForId(stmt->ExitId(), NO_REGISTERS);
}


void FullCodeGenerator::VisitForInStatement(ForInStatement* stmt) {
  ASM_LOCATION("FullCodeGenerator::VisitForInStatement");
  Comment cmnt(masm_, "[ ForInStatement");
  SetStatementPosition(stmt, SKIP_BREAK);

  FeedbackVectorSlot slot = stmt->ForInFeedbackSlot();

  // TODO(all): This visitor probably needs better comments and a revisit.

  Label loop, exit;
  ForIn loop_statement(this, stmt);
  increment_loop_depth();

  // Get the object to enumerate over. If the object is null or undefined, skip
  // over the loop.  See ECMA-262 version 5, section 12.6.4.
  SetExpressionAsStatementPosition(stmt->enumerable());
  VisitForAccumulatorValue(stmt->enumerable());
  __ JumpIfRoot(x0, Heap::kUndefinedValueRootIndex, &exit);
  Register null_value = x15;
  __ LoadRoot(null_value, Heap::kNullValueRootIndex);
  __ Cmp(x0, null_value);
  __ B(eq, &exit);

  PrepareForBailoutForId(stmt->PrepareId(), TOS_REG);

  // Convert the object to a JS object.
  Label convert, done_convert;
  __ JumpIfSmi(x0, &convert);
  __ JumpIfObjectType(x0, x10, x11, FIRST_JS_RECEIVER_TYPE, &done_convert, ge);
  __ Bind(&convert);
  ToObjectStub stub(isolate());
  __ CallStub(&stub);
  __ Bind(&done_convert);
  PrepareForBailoutForId(stmt->ToObjectId(), TOS_REG);
  __ Push(x0);

  // Check for proxies.
  Label call_runtime;
  __ JumpIfObjectType(x0, x10, x11, JS_PROXY_TYPE, &call_runtime, eq);

  // Check cache validity in generated code. This is a fast case for
  // the JSObject::IsSimpleEnum cache validity checks. If we cannot
  // guarantee cache validity, call the runtime system to check cache
  // validity or get the property names in a fixed array.
  __ CheckEnumCache(x0, null_value, x10, x11, x12, x13, &call_runtime);

  // The enum cache is valid.  Load the map of the object being
  // iterated over and use the cache for the iteration.
  Label use_cache;
  __ Ldr(x0, FieldMemOperand(x0, HeapObject::kMapOffset));
  __ B(&use_cache);

  // Get the set of properties to enumerate.
  __ Bind(&call_runtime);
  __ Push(x0);  // Duplicate the enumerable object on the stack.
  __ CallRuntime(Runtime::kGetPropertyNamesFast);
  PrepareForBailoutForId(stmt->EnumId(), TOS_REG);

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

  __ EmitLoadTypeFeedbackVector(x1);
  __ Mov(x10, Operand(TypeFeedbackVector::MegamorphicSentinel(isolate())));
  int vector_index = SmiFromSlot(slot)->value();
  __ Str(x10, FieldMemOperand(x1, FixedArray::OffsetOfElementAt(vector_index)));
  __ Mov(x1, Smi::FromInt(1));  // Smi(1) indicates slow check.
  __ Ldr(x2, FieldMemOperand(x0, FixedArray::kLengthOffset));
  // Smi and array, fixed array length (as smi) and initial index.
  __ Push(x1, x0, x2, xzr);

  // Generate code for doing the condition check.
  __ Bind(&loop);
  SetExpressionAsStatementPosition(stmt->each());

  // Load the current count to x0, load the length to x1.
  __ PeekPair(x0, x1, 0);
  __ Cmp(x0, x1);  // Compare to the array length.
  __ B(hs, loop_statement.break_label());

  // Get the current entry of the array into register r3.
  __ Peek(x10, 2 * kXRegSize);
  __ Add(x10, x10, Operand::UntagSmiAndScale(x0, kPointerSizeLog2));
  __ Ldr(x3, MemOperand(x10, FixedArray::kHeaderSize - kHeapObjectTag));

  // Get the expected map from the stack or a smi in the
  // permanent slow case into register x10.
  __ Peek(x2, 3 * kXRegSize);

  // Check if the expected map still matches that of the enumerable.
  // If not, we may have to filter the key.
  Label update_each;
  __ Peek(x1, 4 * kXRegSize);
  __ Ldr(x11, FieldMemOperand(x1, HeapObject::kMapOffset));
  __ Cmp(x11, x2);
  __ B(eq, &update_each);

  // Convert the entry to a string or (smi) 0 if it isn't a property
  // any more. If the property has been removed while iterating, we
  // just skip it.
  __ Push(x1, x3);
  __ CallRuntime(Runtime::kForInFilter);
  PrepareForBailoutForId(stmt->FilterId(), TOS_REG);
  __ Mov(x3, x0);
  __ JumpIfRoot(x0, Heap::kUndefinedValueRootIndex,
                loop_statement.continue_label());

  // Update the 'each' property or variable from the possibly filtered
  // entry in register x3.
  __ Bind(&update_each);
  __ Mov(result_register(), x3);
  // Perform the assignment as if via '='.
  { EffectContext context(this);
    EmitAssignment(stmt->each(), stmt->EachFeedbackSlot());
    PrepareForBailoutForId(stmt->AssignmentId(), NO_REGISTERS);
  }

  // Both Crankshaft and Turbofan expect BodyId to be right before stmt->body().
  PrepareForBailoutForId(stmt->BodyId(), NO_REGISTERS);
  // Generate code for the body of the loop.
  Visit(stmt->body());

  // Generate code for going to the next element by incrementing
  // the index (smi) stored on top of the stack.
  __ Bind(loop_statement.continue_label());
  // TODO(all): We could use a callee saved register to avoid popping.
  __ Pop(x0);
  __ Add(x0, x0, Smi::FromInt(1));
  __ Push(x0);

  EmitBackEdgeBookkeeping(stmt, &loop);
  __ B(&loop);

  // Remove the pointers stored on the stack.
  __ Bind(loop_statement.break_label());
  __ Drop(5);

  // Exit and decrement the loop depth.
  PrepareForBailoutForId(stmt->ExitId(), NO_REGISTERS);
  __ Bind(&exit);
  decrement_loop_depth();
}


void FullCodeGenerator::EmitNewClosure(Handle<SharedFunctionInfo> info,
                                       bool pretenure) {
  // Use the fast case closure allocation code that allocates in new space for
  // nested functions that don't need literals cloning. If we're running with
  // the --always-opt or the --prepare-always-opt flag, we need to use the
  // runtime function so that the new function we are creating here gets a
  // chance to have its code optimized and doesn't just get a copy of the
  // existing unoptimized code.
  if (!FLAG_always_opt &&
      !FLAG_prepare_always_opt &&
      !pretenure &&
      scope()->is_function_scope() &&
      info->num_literals() == 0) {
    FastNewClosureStub stub(isolate(), info->language_mode(), info->kind());
    __ Mov(x2, Operand(info));
    __ CallStub(&stub);
  } else {
    __ Push(info);
    __ CallRuntime(pretenure ? Runtime::kNewClosure_Tenured
                             : Runtime::kNewClosure);
  }
  context()->Plug(x0);
}


void FullCodeGenerator::EmitSetHomeObject(Expression* initializer, int offset,
                                          FeedbackVectorSlot slot) {
  DCHECK(NeedsHomeObject(initializer));
  __ Peek(StoreDescriptor::ReceiverRegister(), 0);
  __ Mov(StoreDescriptor::NameRegister(),
         Operand(isolate()->factory()->home_object_symbol()));
  __ Peek(StoreDescriptor::ValueRegister(), offset * kPointerSize);
  EmitLoadStoreICSlot(slot);
  CallStoreIC();
}


void FullCodeGenerator::EmitSetHomeObjectAccumulator(Expression* initializer,
                                                     int offset,
                                                     FeedbackVectorSlot slot) {
  DCHECK(NeedsHomeObject(initializer));
  __ Move(StoreDescriptor::ReceiverRegister(), x0);
  __ Mov(StoreDescriptor::NameRegister(),
         Operand(isolate()->factory()->home_object_symbol()));
  __ Peek(StoreDescriptor::ValueRegister(), offset * kPointerSize);
  EmitLoadStoreICSlot(slot);
  CallStoreIC();
}


void FullCodeGenerator::EmitLoadGlobalCheckExtensions(VariableProxy* proxy,
                                                      TypeofMode typeof_mode,
                                                      Label* slow) {
  Register current = cp;
  Register next = x10;
  Register temp = x11;

  Scope* s = scope();
  while (s != NULL) {
    if (s->num_heap_slots() > 0) {
      if (s->calls_sloppy_eval()) {
        // Check that extension is "the hole".
        __ Ldr(temp, ContextMemOperand(current, Context::EXTENSION_INDEX));
        __ JumpIfNotRoot(temp, Heap::kTheHoleValueRootIndex, slow);
      }
      // Load next context in chain.
      __ Ldr(next, ContextMemOperand(current, Context::PREVIOUS_INDEX));
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
    __ Mov(next, current);

    __ Bind(&loop);
    // Terminate at native context.
    __ Ldr(temp, FieldMemOperand(next, HeapObject::kMapOffset));
    __ JumpIfRoot(temp, Heap::kNativeContextMapRootIndex, &fast);
    // Check that extension is "the hole".
    __ Ldr(temp, ContextMemOperand(next, Context::EXTENSION_INDEX));
    __ JumpIfNotRoot(temp, Heap::kTheHoleValueRootIndex, slow);
    // Load next context in chain.
    __ Ldr(next, ContextMemOperand(next, Context::PREVIOUS_INDEX));
    __ B(&loop);
    __ Bind(&fast);
  }

  // All extension objects were empty and it is safe to use a normal global
  // load machinery.
  EmitGlobalVariableLoad(proxy, typeof_mode);
}


MemOperand FullCodeGenerator::ContextSlotOperandCheckExtensions(Variable* var,
                                                                Label* slow) {
  DCHECK(var->IsContextSlot());
  Register context = cp;
  Register next = x10;
  Register temp = x11;

  for (Scope* s = scope(); s != var->scope(); s = s->outer_scope()) {
    if (s->num_heap_slots() > 0) {
      if (s->calls_sloppy_eval()) {
        // Check that extension is "the hole".
        __ Ldr(temp, ContextMemOperand(context, Context::EXTENSION_INDEX));
        __ JumpIfNotRoot(temp, Heap::kTheHoleValueRootIndex, slow);
      }
      __ Ldr(next, ContextMemOperand(context, Context::PREVIOUS_INDEX));
      // Walk the rest of the chain without clobbering cp.
      context = next;
    }
  }
  // Check that last extension is "the hole".
  __ Ldr(temp, ContextMemOperand(context, Context::EXTENSION_INDEX));
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
    __ B(done);
  } else if (var->mode() == DYNAMIC_LOCAL) {
    Variable* local = var->local_if_not_shadowed();
    __ Ldr(x0, ContextSlotOperandCheckExtensions(local, slow));
    if (local->mode() == LET || local->mode() == CONST ||
        local->mode() == CONST_LEGACY) {
      __ JumpIfNotRoot(x0, Heap::kTheHoleValueRootIndex, done);
      if (local->mode() == CONST_LEGACY) {
        __ LoadRoot(x0, Heap::kUndefinedValueRootIndex);
      } else {  // LET || CONST
        __ Mov(x0, Operand(var->name()));
        __ Push(x0);
        __ CallRuntime(Runtime::kThrowReferenceError);
      }
    }
    __ B(done);
  }
}


void FullCodeGenerator::EmitGlobalVariableLoad(VariableProxy* proxy,
                                               TypeofMode typeof_mode) {
  Variable* var = proxy->var();
  DCHECK(var->IsUnallocatedOrGlobalSlot() ||
         (var->IsLookupSlot() && var->mode() == DYNAMIC_GLOBAL));
  __ LoadGlobalObject(LoadDescriptor::ReceiverRegister());
  __ Mov(LoadDescriptor::NameRegister(), Operand(var->name()));
  __ Mov(LoadDescriptor::SlotRegister(),
         SmiFromSlot(proxy->VariableFeedbackSlot()));
  CallLoadIC(typeof_mode);
}


void FullCodeGenerator::EmitVariableLoad(VariableProxy* proxy,
                                         TypeofMode typeof_mode) {
  // Record position before possible IC call.
  SetExpressionPosition(proxy);
  PrepareForBailoutForId(proxy->BeforeId(), NO_REGISTERS);
  Variable* var = proxy->var();

  // Three cases: global variables, lookup variables, and all other types of
  // variables.
  switch (var->location()) {
    case VariableLocation::GLOBAL:
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
      if (NeedsHoleCheckForLoad(proxy)) {
        // Let and const need a read barrier.
        GetVar(x0, var);
        Label done;
        __ JumpIfNotRoot(x0, Heap::kTheHoleValueRootIndex, &done);
        if (var->mode() == LET || var->mode() == CONST) {
          // Throw a reference error when using an uninitialized let/const
          // binding in harmony mode.
          __ Mov(x0, Operand(var->name()));
          __ Push(x0);
          __ CallRuntime(Runtime::kThrowReferenceError);
          __ Bind(&done);
        } else {
          // Uninitialized legacy const bindings are unholed.
          DCHECK(var->mode() == CONST_LEGACY);
          __ LoadRoot(x0, Heap::kUndefinedValueRootIndex);
          __ Bind(&done);
        }
        context()->Plug(x0);
        break;
      }
      context()->Plug(var);
      break;
    }

    case VariableLocation::LOOKUP: {
      Label done, slow;
      // Generate code for loading from variables potentially shadowed by
      // eval-introduced variables.
      EmitDynamicLookupFastCase(proxy, typeof_mode, &slow, &done);
      __ Bind(&slow);
      Comment cmnt(masm_, "Lookup variable");
      __ Mov(x1, Operand(var->name()));
      __ Push(cp, x1);  // Context and name.
      Runtime::FunctionId function_id =
          typeof_mode == NOT_INSIDE_TYPEOF
              ? Runtime::kLoadLookupSlot
              : Runtime::kLoadLookupSlotNoReferenceError;
      __ CallRuntime(function_id);
      __ Bind(&done);
      context()->Plug(x0);
      break;
    }
  }
}


void FullCodeGenerator::VisitRegExpLiteral(RegExpLiteral* expr) {
  Comment cmnt(masm_, "[ RegExpLiteral");
  __ Ldr(x3, MemOperand(fp, JavaScriptFrameConstants::kFunctionOffset));
  __ Mov(x2, Smi::FromInt(expr->literal_index()));
  __ Mov(x1, Operand(expr->pattern()));
  __ Mov(x0, Smi::FromInt(expr->flags()));
  FastCloneRegExpStub stub(isolate());
  __ CallStub(&stub);
  context()->Plug(x0);
}


void FullCodeGenerator::EmitAccessor(ObjectLiteralProperty* property) {
  Expression* expression = (property == NULL) ? NULL : property->value();
  if (expression == NULL) {
    __ LoadRoot(x10, Heap::kNullValueRootIndex);
    __ Push(x10);
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
  __ Ldr(x3, MemOperand(fp,  JavaScriptFrameConstants::kFunctionOffset));
  __ Mov(x2, Smi::FromInt(expr->literal_index()));
  __ Mov(x1, Operand(constant_properties));
  int flags = expr->ComputeFlags();
  __ Mov(x0, Smi::FromInt(flags));
  if (MustCreateObjectLiteralWithRuntime(expr)) {
    __ Push(x3, x2, x1, x0);
    __ CallRuntime(Runtime::kCreateObjectLiteral);
  } else {
    FastCloneShallowObjectStub stub(isolate(), expr->properties_count());
    __ CallStub(&stub);
  }
  PrepareForBailoutForId(expr->CreateLiteralId(), TOS_REG);

  // If result_saved is true the result is on top of the stack.  If
  // result_saved is false the result is in x0.
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
      __ Push(x0);  // Save result on stack
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
        if (key->value()->IsInternalizedString()) {
          if (property->emit_store()) {
            VisitForAccumulatorValue(value);
            DCHECK(StoreDescriptor::ValueRegister().is(x0));
            __ Mov(StoreDescriptor::NameRegister(), Operand(key->value()));
            __ Peek(StoreDescriptor::ReceiverRegister(), 0);
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
        __ Peek(x0, 0);
        __ Push(x0);
        VisitForStackValue(key);
        VisitForStackValue(value);
        if (property->emit_store()) {
          if (NeedsHomeObject(value)) {
            EmitSetHomeObject(value, 2, property->GetSlot());
          }
          __ Mov(x0, Smi::FromInt(SLOPPY));  // Language mode
          __ Push(x0);
          __ CallRuntime(Runtime::kSetProperty);
        } else {
          __ Drop(3);
        }
        break;
      case ObjectLiteral::Property::PROTOTYPE:
        DCHECK(property->emit_store());
        // Duplicate receiver on stack.
        __ Peek(x0, 0);
        __ Push(x0);
        VisitForStackValue(value);
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
      __ Peek(x10, 0);  // Duplicate receiver.
      __ Push(x10);
      VisitForStackValue(it->first);
      EmitAccessor(it->second->getter);
      EmitAccessor(it->second->setter);
      __ Mov(x10, Smi::FromInt(NONE));
      __ Push(x10);
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
      __ Push(x0);  // Save result on stack
      result_saved = true;
    }

    __ Peek(x10, 0);  // Duplicate receiver.
    __ Push(x10);

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
            __ Mov(x0, Smi::FromInt(NONE));
            __ Push(x0);
            __ CallRuntime(Runtime::kDefineDataPropertyUnchecked);
          } else {
            __ Drop(3);
          }
          break;

        case ObjectLiteral::Property::PROTOTYPE:
          UNREACHABLE();
          break;

        case ObjectLiteral::Property::GETTER:
          __ Mov(x0, Smi::FromInt(NONE));
          __ Push(x0);
          __ CallRuntime(Runtime::kDefineGetterPropertyUnchecked);
          break;

        case ObjectLiteral::Property::SETTER:
          __ Mov(x0, Smi::FromInt(NONE));
          __ Push(x0);
          __ CallRuntime(Runtime::kDefineSetterPropertyUnchecked);
          break;
      }
    }
  }

  if (expr->has_function()) {
    DCHECK(result_saved);
    __ Peek(x0, 0);
    __ Push(x0);
    __ CallRuntime(Runtime::kToFastProperties);
  }

  if (result_saved) {
    context()->PlugTOS();
  } else {
    context()->Plug(x0);
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

  __ Ldr(x3, MemOperand(fp, JavaScriptFrameConstants::kFunctionOffset));
  __ Mov(x2, Smi::FromInt(expr->literal_index()));
  __ Mov(x1, Operand(constant_elements));
  if (MustCreateArrayLiteralWithRuntime(expr)) {
    __ Mov(x0, Smi::FromInt(expr->ComputeFlags()));
    __ Push(x3, x2, x1, x0);
    __ CallRuntime(Runtime::kCreateArrayLiteral);
  } else {
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
      __ Push(x0);
      result_saved = true;
    }
    VisitForAccumulatorValue(subexpr);

    __ Mov(StoreDescriptor::NameRegister(), Smi::FromInt(array_index));
    __ Peek(StoreDescriptor::ReceiverRegister(), 0);
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
    __ Pop(x0);
    result_saved = false;
  }
  for (; array_index < length; array_index++) {
    Expression* subexpr = subexprs->at(array_index);

    __ Push(x0);
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
    context()->Plug(x0);
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
    case NAMED_PROPERTY:
      if (expr->is_compound()) {
        // We need the receiver both on the stack and in the register.
        VisitForStackValue(property->obj());
        __ Peek(LoadDescriptor::ReceiverRegister(), 0);
      } else {
        VisitForStackValue(property->obj());
      }
      break;
    case NAMED_SUPER_PROPERTY:
      VisitForStackValue(
          property->obj()->AsSuperPropertyReference()->this_var());
      VisitForAccumulatorValue(
          property->obj()->AsSuperPropertyReference()->home_object());
      __ Push(result_register());
      if (expr->is_compound()) {
        const Register scratch = x10;
        __ Peek(scratch, kPointerSize);
        __ Push(scratch, result_register());
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
        const Register scratch1 = x10;
        const Register scratch2 = x11;
        __ Peek(scratch1, 2 * kPointerSize);
        __ Peek(scratch2, kPointerSize);
        __ Push(scratch1, scratch2, result_register());
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
        case NAMED_SUPER_PROPERTY:
          EmitNamedSuperPropertyLoad(property);
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
    __ Push(x0);  // Left operand goes on the stack.
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
      context()->Plug(x0);
      break;
    case NAMED_PROPERTY:
      EmitNamedPropertyAssignment(expr);
      break;
    case NAMED_SUPER_PROPERTY:
      EmitNamedSuperPropertyStore(property);
      context()->Plug(x0);
      break;
    case KEYED_SUPER_PROPERTY:
      EmitKeyedSuperPropertyStore(property);
      context()->Plug(x0);
      break;
    case KEYED_PROPERTY:
      EmitKeyedPropertyAssignment(expr);
      break;
  }
}


void FullCodeGenerator::EmitNamedPropertyLoad(Property* prop) {
  SetExpressionPosition(prop);
  Literal* key = prop->key()->AsLiteral();
  DCHECK(!prop->IsSuperAccess());

  __ Mov(LoadDescriptor::NameRegister(), Operand(key->value()));
  __ Mov(LoadDescriptor::SlotRegister(),
         SmiFromSlot(prop->PropertyFeedbackSlot()));
  CallLoadIC(NOT_INSIDE_TYPEOF, language_mode());
}


void FullCodeGenerator::EmitNamedSuperPropertyLoad(Property* prop) {
  // Stack: receiver, home_object.
  SetExpressionPosition(prop);
  Literal* key = prop->key()->AsLiteral();
  DCHECK(!key->value()->IsSmi());
  DCHECK(prop->IsSuperAccess());

  __ Push(key->value());
  __ Push(Smi::FromInt(language_mode()));
  __ CallRuntime(Runtime::kLoadFromSuper);
}


void FullCodeGenerator::EmitKeyedPropertyLoad(Property* prop) {
  SetExpressionPosition(prop);
  // Call keyed load IC. It has arguments key and receiver in x0 and x1.
  Handle<Code> ic = CodeFactory::KeyedLoadIC(isolate(), language_mode()).code();
  __ Mov(LoadDescriptor::SlotRegister(),
         SmiFromSlot(prop->PropertyFeedbackSlot()));
  CallIC(ic);
}


void FullCodeGenerator::EmitKeyedSuperPropertyLoad(Property* prop) {
  // Stack: receiver, home_object, key.
  SetExpressionPosition(prop);
  __ Push(Smi::FromInt(language_mode()));
  __ CallRuntime(Runtime::kLoadKeyedFromSuper);
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
  __ Pop(left);

  // Perform combined smi check on both operands.
  __ Orr(x10, left, right);
  JumpPatchSite patch_site(masm_);
  patch_site.EmitJumpIfSmi(x10, &both_smis);

  __ Bind(&stub_call);

  Handle<Code> code =
      CodeFactory::BinaryOpIC(isolate(), op, strength(language_mode())).code();
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
  __ Pop(x1);
  Handle<Code> code =
      CodeFactory::BinaryOpIC(isolate(), op, strength(language_mode())).code();
  JumpPatchSite patch_site(masm_);    // Unbound, signals no inlined smi code.
  {
    Assembler::BlockPoolsScope scope(masm_);
    CallIC(code, expr->BinaryOperationFeedbackId());
    patch_site.EmitPatchInfo();
  }
  context()->Plug(x0);
}


void FullCodeGenerator::EmitClassDefineProperties(ClassLiteral* lit) {
  // Constructor is in x0.
  DCHECK(lit != NULL);
  __ push(x0);

  // No access check is needed here since the constructor is created by the
  // class literal.
  Register scratch = x1;
  __ Ldr(scratch,
         FieldMemOperand(x0, JSFunction::kPrototypeOrInitialMapOffset));
  __ Push(scratch);

  for (int i = 0; i < lit->properties()->length(); i++) {
    ObjectLiteral::Property* property = lit->properties()->at(i);
    Expression* value = property->value();

    if (property->is_static()) {
      __ Peek(scratch, kPointerSize);  // constructor
    } else {
      __ Peek(scratch, 0);  // prototype
    }
    __ Push(scratch);
    EmitPropertyKey(property, lit->GetIdForProperty(i));

    // The static prototype property is read only. We handle the non computed
    // property name case in the parser. Since this is the only case where we
    // need to check for an own read only property we special case this so we do
    // not need to do this for every property.
    if (property->is_static() && property->is_computed_name()) {
      __ CallRuntime(Runtime::kThrowIfStaticPrototype);
      __ Push(x0);
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
        __ Mov(x0, Smi::FromInt(DONT_ENUM));
        __ Push(x0);
        __ CallRuntime(Runtime::kDefineGetterPropertyUnchecked);
        break;

      case ObjectLiteral::Property::SETTER:
        __ Mov(x0, Smi::FromInt(DONT_ENUM));
        __ Push(x0);
        __ CallRuntime(Runtime::kDefineSetterPropertyUnchecked);
        break;

      default:
        UNREACHABLE();
    }
  }

  // Set both the prototype and constructor to have fast properties, and also
  // freeze them in strong mode.
  __ CallRuntime(Runtime::kFinalizeClassDefinition);
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
      __ Push(x0);  // Preserve value.
      VisitForAccumulatorValue(prop->obj());
      // TODO(all): We could introduce a VisitForRegValue(reg, expr) to avoid
      // this copy.
      __ Mov(StoreDescriptor::ReceiverRegister(), x0);
      __ Pop(StoreDescriptor::ValueRegister());  // Restore value.
      __ Mov(StoreDescriptor::NameRegister(),
             Operand(prop->key()->AsLiteral()->value()));
      EmitLoadStoreICSlot(slot);
      CallStoreIC();
      break;
    }
    case NAMED_SUPER_PROPERTY: {
      __ Push(x0);
      VisitForStackValue(prop->obj()->AsSuperPropertyReference()->this_var());
      VisitForAccumulatorValue(
          prop->obj()->AsSuperPropertyReference()->home_object());
      // stack: value, this; x0: home_object
      Register scratch = x10;
      Register scratch2 = x11;
      __ mov(scratch, result_register());  // home_object
      __ Peek(x0, kPointerSize);           // value
      __ Peek(scratch2, 0);                // this
      __ Poke(scratch2, kPointerSize);     // this
      __ Poke(scratch, 0);                 // home_object
      // stack: this, home_object; x0: value
      EmitNamedSuperPropertyStore(prop);
      break;
    }
    case KEYED_SUPER_PROPERTY: {
      __ Push(x0);
      VisitForStackValue(prop->obj()->AsSuperPropertyReference()->this_var());
      VisitForStackValue(
          prop->obj()->AsSuperPropertyReference()->home_object());
      VisitForAccumulatorValue(prop->key());
      Register scratch = x10;
      Register scratch2 = x11;
      __ Peek(scratch2, 2 * kPointerSize);  // value
      // stack: value, this, home_object; x0: key, x11: value
      __ Peek(scratch, kPointerSize);  // this
      __ Poke(scratch, 2 * kPointerSize);
      __ Peek(scratch, 0);  // home_object
      __ Poke(scratch, kPointerSize);
      __ Poke(x0, 0);
      __ Move(x0, scratch2);
      // stack: this, home_object, key; x0: value.
      EmitKeyedSuperPropertyStore(prop);
      break;
    }
    case KEYED_PROPERTY: {
      __ Push(x0);  // Preserve value.
      VisitForStackValue(prop->obj());
      VisitForAccumulatorValue(prop->key());
      __ Mov(StoreDescriptor::NameRegister(), x0);
      __ Pop(StoreDescriptor::ReceiverRegister(),
             StoreDescriptor::ValueRegister());
      EmitLoadStoreICSlot(slot);
      Handle<Code> ic =
          CodeFactory::KeyedStoreIC(isolate(), language_mode()).code();
      CallIC(ic);
      break;
    }
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
                                               FeedbackVectorSlot slot) {
  ASM_LOCATION("FullCodeGenerator::EmitVariableAssignment");
  if (var->IsUnallocated()) {
    // Global var, const, or let.
    __ Mov(StoreDescriptor::NameRegister(), Operand(var->name()));
    __ LoadGlobalObject(StoreDescriptor::ReceiverRegister());
    EmitLoadStoreICSlot(slot);
    CallStoreIC();

  } else if (var->mode() == LET && op != Token::INIT) {
    // Non-initializing assignment to let variable needs a write barrier.
    DCHECK(!var->IsLookupSlot());
    DCHECK(var->IsStackAllocated() || var->IsContextSlot());
    Label assign;
    MemOperand location = VarOperand(var, x1);
    __ Ldr(x10, location);
    __ JumpIfNotRoot(x10, Heap::kTheHoleValueRootIndex, &assign);
    __ Mov(x10, Operand(var->name()));
    __ Push(x10);
    __ CallRuntime(Runtime::kThrowReferenceError);
    // Perform the assignment.
    __ Bind(&assign);
    EmitStoreToStackLocalOrContextSlot(var, location);

  } else if (var->mode() == CONST && op != Token::INIT) {
    // Assignment to const variable needs a write barrier.
    DCHECK(!var->IsLookupSlot());
    DCHECK(var->IsStackAllocated() || var->IsContextSlot());
    Label const_error;
    MemOperand location = VarOperand(var, x1);
    __ Ldr(x10, location);
    __ JumpIfNotRoot(x10, Heap::kTheHoleValueRootIndex, &const_error);
    __ Mov(x10, Operand(var->name()));
    __ Push(x10);
    __ CallRuntime(Runtime::kThrowReferenceError);
    __ Bind(&const_error);
    __ CallRuntime(Runtime::kThrowConstAssignError);

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

  } else if (!var->is_const_mode() ||
             (var->mode() == CONST && op == Token::INIT)) {
    if (var->IsLookupSlot()) {
      // Assignment to var.
      __ Mov(x11, Operand(var->name()));
      __ Mov(x10, Smi::FromInt(language_mode()));
      // jssp[0]  : mode.
      // jssp[8]  : name.
      // jssp[16] : context.
      // jssp[24] : value.
      __ Push(x0, cp, x11, x10);
      __ CallRuntime(Runtime::kStoreLookupSlot);
    } else {
      // Assignment to var or initializing assignment to let/const in harmony
      // mode.
      DCHECK(var->IsStackAllocated() || var->IsContextSlot());
      MemOperand location = VarOperand(var, x1);
      if (FLAG_debug_code && var->mode() == LET && op == Token::INIT) {
        __ Ldr(x10, location);
        __ CompareRoot(x10, Heap::kTheHoleValueRootIndex);
        __ Check(eq, kLetBindingReInitialization);
      }
      EmitStoreToStackLocalOrContextSlot(var, location);
    }

  } else if (var->mode() == CONST_LEGACY && op == Token::INIT) {
    // Const initializers need a write barrier.
    DCHECK(!var->IsParameter());  // No const parameters.
    if (var->IsLookupSlot()) {
      __ Mov(x1, Operand(var->name()));
      __ Push(x0, cp, x1);
      __ CallRuntime(Runtime::kInitializeLegacyConstLookupSlot);
    } else {
      DCHECK(var->IsStackLocal() || var->IsContextSlot());
      Label skip;
      MemOperand location = VarOperand(var, x1);
      __ Ldr(x10, location);
      __ JumpIfNotRoot(x10, Heap::kTheHoleValueRootIndex, &skip);
      EmitStoreToStackLocalOrContextSlot(var, location);
      __ Bind(&skip);
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
  ASM_LOCATION("FullCodeGenerator::EmitNamedPropertyAssignment");
  // Assignment to a property, using a named store IC.
  Property* prop = expr->target()->AsProperty();
  DCHECK(prop != NULL);
  DCHECK(prop->key()->IsLiteral());

  __ Mov(StoreDescriptor::NameRegister(),
         Operand(prop->key()->AsLiteral()->value()));
  __ Pop(StoreDescriptor::ReceiverRegister());
  EmitLoadStoreICSlot(expr->AssignmentSlot());
  CallStoreIC();

  PrepareForBailoutForId(expr->AssignmentId(), TOS_REG);
  context()->Plug(x0);
}


void FullCodeGenerator::EmitNamedSuperPropertyStore(Property* prop) {
  // Assignment to named property of super.
  // x0 : value
  // stack : receiver ('this'), home_object
  DCHECK(prop != NULL);
  Literal* key = prop->key()->AsLiteral();
  DCHECK(key != NULL);

  __ Push(key->value());
  __ Push(x0);
  __ CallRuntime((is_strict(language_mode()) ? Runtime::kStoreToSuper_Strict
                                             : Runtime::kStoreToSuper_Sloppy));
}


void FullCodeGenerator::EmitKeyedSuperPropertyStore(Property* prop) {
  // Assignment to named property of super.
  // x0 : value
  // stack : receiver ('this'), home_object, key
  DCHECK(prop != NULL);

  __ Push(x0);
  __ CallRuntime((is_strict(language_mode())
                      ? Runtime::kStoreKeyedToSuper_Strict
                      : Runtime::kStoreKeyedToSuper_Sloppy));
}


void FullCodeGenerator::EmitKeyedPropertyAssignment(Assignment* expr) {
  ASM_LOCATION("FullCodeGenerator::EmitKeyedPropertyAssignment");
  // Assignment to a property, using a keyed store IC.

  // TODO(all): Could we pass this in registers rather than on the stack?
  __ Pop(StoreDescriptor::NameRegister(), StoreDescriptor::ReceiverRegister());
  DCHECK(StoreDescriptor::ValueRegister().is(x0));

  Handle<Code> ic =
      CodeFactory::KeyedStoreIC(isolate(), language_mode()).code();
  EmitLoadStoreICSlot(expr->AssignmentSlot());
  CallIC(ic);

  PrepareForBailoutForId(expr->AssignmentId(), TOS_REG);
  context()->Plug(x0);
}


void FullCodeGenerator::VisitProperty(Property* expr) {
  Comment cmnt(masm_, "[ Property");
  SetExpressionPosition(expr);
  Expression* key = expr->key();

  if (key->IsPropertyName()) {
    if (!expr->IsSuperAccess()) {
      VisitForAccumulatorValue(expr->obj());
      __ Move(LoadDescriptor::ReceiverRegister(), x0);
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
      __ Move(LoadDescriptor::NameRegister(), x0);
      __ Pop(LoadDescriptor::ReceiverRegister());
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
  context()->Plug(x0);
}


void FullCodeGenerator::CallIC(Handle<Code> code,
                               TypeFeedbackId ast_id) {
  ic_total_count_++;
  // All calls must have a predictable size in full-codegen code to ensure that
  // the debugger can patch them correctly.
  __ Call(code, RelocInfo::CODE_TARGET, ast_id);
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
      PrepareForBailout(callee, NO_REGISTERS);
    }
    // Push undefined as receiver. This is patched in the method prologue if it
    // is a sloppy mode method.
    {
      UseScratchRegisterScope temps(masm_);
      Register temp = temps.AcquireX();
      __ LoadRoot(temp, Heap::kUndefinedValueRootIndex);
      __ Push(temp);
    }
    convert_mode = ConvertReceiverMode::kNullOrUndefined;
  } else {
    // Load the function from the receiver.
    DCHECK(callee->IsProperty());
    DCHECK(!callee->AsProperty()->IsSuperAccess());
    __ Peek(LoadDescriptor::ReceiverRegister(), 0);
    EmitNamedPropertyLoad(callee->AsProperty());
    PrepareForBailoutForId(callee->AsProperty()->LoadId(), TOS_REG);
    // Push the target function under the receiver.
    __ Pop(x10);
    __ Push(x0, x10);
    convert_mode = ConvertReceiverMode::kNotNullOrUndefined;
  }

  EmitCall(expr, convert_mode);
}


void FullCodeGenerator::EmitSuperCallWithLoadIC(Call* expr) {
  ASM_LOCATION("FullCodeGenerator::EmitSuperCallWithLoadIC");
  Expression* callee = expr->expression();
  DCHECK(callee->IsProperty());
  Property* prop = callee->AsProperty();
  DCHECK(prop->IsSuperAccess());
  SetExpressionPosition(prop);

  Literal* key = prop->key()->AsLiteral();
  DCHECK(!key->value()->IsSmi());

  // Load the function from the receiver.
  const Register scratch = x10;
  SuperPropertyReference* super_ref =
      callee->AsProperty()->obj()->AsSuperPropertyReference();
  VisitForStackValue(super_ref->home_object());
  VisitForAccumulatorValue(super_ref->this_var());
  __ Push(x0);
  __ Peek(scratch, kPointerSize);
  __ Push(x0, scratch);
  __ Push(key->value());
  __ Push(Smi::FromInt(language_mode()));

  // Stack here:
  //  - home_object
  //  - this (receiver)
  //  - this (receiver) <-- LoadFromSuper will pop here and below.
  //  - home_object
  //  - language_mode
  __ CallRuntime(Runtime::kLoadFromSuper);

  // Replace home_object with target function.
  __ Poke(x0, kPointerSize);

  // Stack here:
  // - target function
  // - this (receiver)
  EmitCall(expr);
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
  PrepareForBailoutForId(callee->AsProperty()->LoadId(), TOS_REG);

  // Push the target function under the receiver.
  __ Pop(x10);
  __ Push(x0, x10);

  EmitCall(expr, ConvertReceiverMode::kNotNullOrUndefined);
}


void FullCodeGenerator::EmitKeyedSuperCallWithLoadIC(Call* expr) {
  ASM_LOCATION("FullCodeGenerator::EmitKeyedSuperCallWithLoadIC");
  Expression* callee = expr->expression();
  DCHECK(callee->IsProperty());
  Property* prop = callee->AsProperty();
  DCHECK(prop->IsSuperAccess());
  SetExpressionPosition(prop);

  // Load the function from the receiver.
  const Register scratch = x10;
  SuperPropertyReference* super_ref =
      callee->AsProperty()->obj()->AsSuperPropertyReference();
  VisitForStackValue(super_ref->home_object());
  VisitForAccumulatorValue(super_ref->this_var());
  __ Push(x0);
  __ Peek(scratch, kPointerSize);
  __ Push(x0, scratch);
  VisitForStackValue(prop->key());
  __ Push(Smi::FromInt(language_mode()));

  // Stack here:
  //  - home_object
  //  - this (receiver)
  //  - this (receiver) <-- LoadKeyedFromSuper will pop here and below.
  //  - home_object
  //  - key
  //  - language_mode
  __ CallRuntime(Runtime::kLoadKeyedFromSuper);

  // Replace home_object with target function.
  __ Poke(x0, kPointerSize);

  // Stack here:
  // - target function
  // - this (receiver)
  EmitCall(expr);
}


void FullCodeGenerator::EmitCall(Call* expr, ConvertReceiverMode mode) {
  ASM_LOCATION("FullCodeGenerator::EmitCall");
  // Load the arguments.
  ZoneList<Expression*>* args = expr->arguments();
  int arg_count = args->length();
  for (int i = 0; i < arg_count; i++) {
    VisitForStackValue(args->at(i));
  }

  PrepareForBailoutForId(expr->CallId(), NO_REGISTERS);
  SetCallPosition(expr);

  Handle<Code> ic = CodeFactory::CallIC(isolate(), arg_count, mode).code();
  __ Mov(x3, SmiFromSlot(expr->CallFeedbackICSlot()));
  __ Peek(x1, (arg_count + 1) * kXRegSize);
  // Don't assign a type feedback id to the IC, since type feedback is provided
  // by the vector above.
  CallIC(ic);

  RecordJSReturnSite(expr);
  // Restore context register.
  __ Ldr(cp, MemOperand(fp, StandardFrameConstants::kContextOffset));
  context()->DropAndPlug(1, x0);
}


void FullCodeGenerator::EmitResolvePossiblyDirectEval(int arg_count) {
  ASM_LOCATION("FullCodeGenerator::EmitResolvePossiblyDirectEval");
  // Prepare to push a copy of the first argument or undefined if it doesn't
  // exist.
  if (arg_count > 0) {
    __ Peek(x9, arg_count * kXRegSize);
  } else {
    __ LoadRoot(x9, Heap::kUndefinedValueRootIndex);
  }

  __ Ldr(x10, MemOperand(fp, JavaScriptFrameConstants::kFunctionOffset));

  // Prepare to push the language mode.
  __ Mov(x11, Smi::FromInt(language_mode()));
  // Prepare to push the start position of the scope the calls resides in.
  __ Mov(x12, Smi::FromInt(scope()->start_position()));

  // Push.
  __ Push(x9, x10, x11, x12);

  // Do the runtime call.
  __ CallRuntime(Runtime::kResolvePossiblyDirectEval);
}


// See http://www.ecma-international.org/ecma-262/6.0/#sec-function-calls.
void FullCodeGenerator::PushCalleeAndWithBaseObject(Call* expr) {
  VariableProxy* callee = expr->expression()->AsVariableProxy();
  if (callee->var()->IsLookupSlot()) {
    Label slow, done;
    SetExpressionPosition(callee);
      // Generate code for loading from variables potentially shadowed
      // by eval-introduced variables.
      EmitDynamicLookupFastCase(callee, NOT_INSIDE_TYPEOF, &slow, &done);

    __ Bind(&slow);
    // Call the runtime to find the function to call (returned in x0)
    // and the object holding it (returned in x1).
    __ Mov(x10, Operand(callee->name()));
    __ Push(context_register(), x10);
    __ CallRuntime(Runtime::kLoadLookupSlot);
    __ Push(x0, x1);  // Receiver, function.
    PrepareForBailoutForId(expr->LookupId(), NO_REGISTERS);

    // If fast case code has been generated, emit code to push the
    // function and receiver and have the slow path jump around this
    // code.
    if (done.is_linked()) {
      Label call;
      __ B(&call);
      __ Bind(&done);
      // Push function.
      // The receiver is implicitly the global receiver. Indicate this
      // by passing the undefined to the call function stub.
      __ LoadRoot(x1, Heap::kUndefinedValueRootIndex);
      __ Push(x0, x1);
      __ Bind(&call);
    }
  } else {
    VisitForStackValue(callee);
    // refEnv.WithBaseObject()
    __ LoadRoot(x10, Heap::kUndefinedValueRootIndex);
    __ Push(x10);  // Reserved receiver slot.
  }
}


void FullCodeGenerator::EmitPossiblyEvalCall(Call* expr) {
  ASM_LOCATION("FullCodeGenerator::EmitPossiblyEvalCall");
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
  __ Peek(x10, (arg_count + 1) * kPointerSize);
  __ Push(x10);
  EmitResolvePossiblyDirectEval(arg_count);

  // Touch up the stack with the resolved function.
  __ Poke(x0, (arg_count + 1) * kPointerSize);

  PrepareForBailoutForId(expr->EvalId(), NO_REGISTERS);

  // Record source position for debugger.
  SetCallPosition(expr);

  // Call the evaluated function.
  __ Peek(x1, (arg_count + 1) * kXRegSize);
  __ Mov(x0, arg_count);
  __ Call(isolate()->builtins()->Call(), RelocInfo::CODE_TARGET);
  RecordJSReturnSite(expr);
  // Restore context register.
  __ Ldr(cp, MemOperand(fp, StandardFrameConstants::kContextOffset));
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
  __ EmitLoadTypeFeedbackVector(x2);
  __ Mov(x3, SmiFromSlot(expr->CallNewFeedbackSlot()));

  CallConstructStub stub(isolate());
  __ Call(stub.GetCode(), RelocInfo::CODE_TARGET);
  PrepareForBailoutForId(expr->ReturnId(), TOS_REG);
  // Restore context register.
  __ Ldr(cp, MemOperand(fp, StandardFrameConstants::kContextOffset));
  context()->Plug(x0);
}


void FullCodeGenerator::EmitSuperConstructorCall(Call* expr) {
  ASM_LOCATION("FullCodeGenerator::EmitSuperConstructorCall");
  SuperCallReference* super_call_ref =
      expr->expression()->AsSuperCallReference();
  DCHECK_NOT_NULL(super_call_ref);

  // Push the super constructor target on the stack (may be null,
  // but the Construct builtin can deal with that properly).
  VisitForAccumulatorValue(super_call_ref->this_function_var());
  __ AssertFunction(result_register());
  __ Ldr(result_register(),
         FieldMemOperand(result_register(), HeapObject::kMapOffset));
  __ Ldr(result_register(),
         FieldMemOperand(result_register(), Map::kPrototypeOffset));
  __ Push(result_register());

  // Push the arguments ("left-to-right") on the stack.
  ZoneList<Expression*>* args = expr->arguments();
  int arg_count = args->length();
  for (int i = 0; i < arg_count; i++) {
    VisitForStackValue(args->at(i));
  }

  // Call the construct call builtin that handles allocation and
  // constructor invocation.
  SetConstructCallPosition(expr);

  // Load new target into x3.
  VisitForAccumulatorValue(super_call_ref->new_target_var());
  __ Mov(x3, result_register());

  // Load function and argument count into x1 and x0.
  __ Mov(x0, arg_count);
  __ Peek(x1, arg_count * kXRegSize);

  __ Call(isolate()->builtins()->Construct(), RelocInfo::CODE_TARGET);

  RecordJSReturnSite(expr);

  // Restore context register.
  __ Ldr(cp, MemOperand(fp, StandardFrameConstants::kContextOffset));
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

  __ JumpIfSmi(x0, if_false);
  __ CompareObjectType(x0, x10, x11, SIMD128_VALUE_TYPE);
  PrepareForBailoutBeforeSplit(expr, true, if_true, if_false);
  Split(eq, if_true, if_false, fall_through);

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

  __ JumpIfSmi(x0, if_false);
  __ CompareObjectType(x0, x10, x11, FIRST_FUNCTION_TYPE);
  PrepareForBailoutBeforeSplit(expr, true, if_true, if_false);
  Split(hs, if_true, if_false, fall_through);

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

  // Only a HeapNumber can be -0.0, so return false if we have something else.
  __ JumpIfNotHeapNumber(x0, if_false, DO_SMI_CHECK);

  // Test the bit pattern.
  __ Ldr(x10, FieldMemOperand(x0, HeapNumber::kValueOffset));
  __ Cmp(x10, 1);   // Set V on 0x8000000000000000.

  PrepareForBailoutBeforeSplit(expr, true, if_true, if_false);
  Split(vs, if_true, if_false, fall_through);

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

  __ JumpIfSmi(x0, if_false);
  __ CompareObjectType(x0, x10, x11, JS_REGEXP_TYPE);
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

  __ Pop(x1);
  __ Cmp(x0, x1);
  PrepareForBailoutBeforeSplit(expr, true, if_true, if_false);
  Split(eq, if_true, if_false, fall_through);

  context()->Plug(if_true, if_false);
}


void FullCodeGenerator::EmitArguments(CallRuntime* expr) {
  ZoneList<Expression*>* args = expr->arguments();
  DCHECK(args->length() == 1);

  // ArgumentsAccessStub expects the key in x1.
  VisitForAccumulatorValue(args->at(0));
  __ Mov(x1, x0);
  __ Mov(x0, Smi::FromInt(info_->scope()->num_parameters()));
  ArgumentsAccessStub stub(isolate(), ArgumentsAccessStub::READ_ELEMENT);
  __ CallStub(&stub);
  context()->Plug(x0);
}


void FullCodeGenerator::EmitArgumentsLength(CallRuntime* expr) {
  DCHECK(expr->arguments()->length() == 0);
  Label exit;
  // Get the number of formal parameters.
  __ Mov(x0, Smi::FromInt(info_->scope()->num_parameters()));

  // Check if the calling frame is an arguments adaptor frame.
  __ Ldr(x12, MemOperand(fp, StandardFrameConstants::kCallerFPOffset));
  __ Ldr(x13, MemOperand(x12, StandardFrameConstants::kContextOffset));
  __ Cmp(x13, Smi::FromInt(StackFrame::ARGUMENTS_ADAPTOR));
  __ B(ne, &exit);

  // Arguments adaptor case: Read the arguments length from the
  // adaptor frame.
  __ Ldr(x0, MemOperand(x12, ArgumentsAdaptorFrameConstants::kLengthOffset));

  __ Bind(&exit);
  context()->Plug(x0);
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
  __ Cmp(x11, JS_FUNCTION_TYPE);
  __ B(eq, &function);

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


void FullCodeGenerator::EmitValueOf(CallRuntime* expr) {
  ASM_LOCATION("FullCodeGenerator::EmitValueOf");
  ZoneList<Expression*>* args = expr->arguments();
  DCHECK(args->length() == 1);
  VisitForAccumulatorValue(args->at(0));  // Load the object.

  Label done;
  // If the object is a smi return the object.
  __ JumpIfSmi(x0, &done);
  // If the object is not a value type, return the object.
  __ JumpIfNotObjectType(x0, x10, x11, JS_VALUE_TYPE, &done);
  __ Ldr(x0, FieldMemOperand(x0, JSValue::kValueOffset));

  __ Bind(&done);
  context()->Plug(x0);
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

  __ JumpIfSmi(x0, if_false);
  __ CompareObjectType(x0, x10, x11, JS_DATE_TYPE);
  PrepareForBailoutBeforeSplit(expr, true, if_true, if_false);
  Split(eq, if_true, if_false, fall_through);

  context()->Plug(if_true, if_false);
}


void FullCodeGenerator::EmitOneByteSeqStringSetChar(CallRuntime* expr) {
  ZoneList<Expression*>* args = expr->arguments();
  DCHECK_EQ(3, args->length());

  Register string = x0;
  Register index = x1;
  Register value = x2;
  Register scratch = x10;

  VisitForStackValue(args->at(0));        // index
  VisitForStackValue(args->at(1));        // value
  VisitForAccumulatorValue(args->at(2));  // string
  __ Pop(value, index);

  if (FLAG_debug_code) {
    __ AssertSmi(value, kNonSmiValue);
    __ AssertSmi(index, kNonSmiIndex);
    static const uint32_t one_byte_seq_type = kSeqStringTag | kOneByteStringTag;
    __ EmitSeqStringSetCharCheck(string, index, kIndexIsSmi, scratch,
                                 one_byte_seq_type);
  }

  __ Add(scratch, string, SeqOneByteString::kHeaderSize - kHeapObjectTag);
  __ SmiUntag(value);
  __ SmiUntag(index);
  __ Strb(value, MemOperand(scratch, index));
  context()->Plug(string);
}


void FullCodeGenerator::EmitTwoByteSeqStringSetChar(CallRuntime* expr) {
  ZoneList<Expression*>* args = expr->arguments();
  DCHECK_EQ(3, args->length());

  Register string = x0;
  Register index = x1;
  Register value = x2;
  Register scratch = x10;

  VisitForStackValue(args->at(0));        // index
  VisitForStackValue(args->at(1));        // value
  VisitForAccumulatorValue(args->at(2));  // string
  __ Pop(value, index);

  if (FLAG_debug_code) {
    __ AssertSmi(value, kNonSmiValue);
    __ AssertSmi(index, kNonSmiIndex);
    static const uint32_t two_byte_seq_type = kSeqStringTag | kTwoByteStringTag;
    __ EmitSeqStringSetCharCheck(string, index, kIndexIsSmi, scratch,
                                 two_byte_seq_type);
  }

  __ Add(scratch, string, SeqTwoByteString::kHeaderSize - kHeapObjectTag);
  __ SmiUntag(value);
  __ SmiUntag(index);
  __ Strh(value, MemOperand(scratch, index, LSL, 1));
  context()->Plug(string);
}


void FullCodeGenerator::EmitSetValueOf(CallRuntime* expr) {
  ZoneList<Expression*>* args = expr->arguments();
  DCHECK(args->length() == 2);
  VisitForStackValue(args->at(0));  // Load the object.
  VisitForAccumulatorValue(args->at(1));  // Load the value.
  __ Pop(x1);
  // x0 = value.
  // x1 = object.

  Label done;
  // If the object is a smi, return the value.
  __ JumpIfSmi(x1, &done);

  // If the object is not a value type, return the value.
  __ JumpIfNotObjectType(x1, x10, x11, JS_VALUE_TYPE, &done);

  // Store the value.
  __ Str(x0, FieldMemOperand(x1, JSValue::kValueOffset));
  // Update the write barrier. Save the value as it will be
  // overwritten by the write barrier code and is needed afterward.
  __ Mov(x10, x0);
  __ RecordWriteField(
      x1, JSValue::kValueOffset, x10, x11, kLRHasBeenSaved, kDontSaveFPRegs);

  __ Bind(&done);
  context()->Plug(x0);
}


void FullCodeGenerator::EmitToInteger(CallRuntime* expr) {
  ZoneList<Expression*>* args = expr->arguments();
  DCHECK_EQ(1, args->length());

  // Load the argument into x0 and convert it.
  VisitForAccumulatorValue(args->at(0));

  // Convert the object to an integer.
  Label done_convert;
  __ JumpIfSmi(x0, &done_convert);
  __ Push(x0);
  __ CallRuntime(Runtime::kToInteger);
  __ bind(&done_convert);
  context()->Plug(x0);
}


void FullCodeGenerator::EmitToName(CallRuntime* expr) {
  ZoneList<Expression*>* args = expr->arguments();
  DCHECK_EQ(1, args->length());

  // Load the argument into x0 and convert it.
  VisitForAccumulatorValue(args->at(0));

  Label convert, done_convert;
  __ JumpIfSmi(x0, &convert);
  STATIC_ASSERT(FIRST_NAME_TYPE == FIRST_TYPE);
  __ JumpIfObjectType(x0, x1, x1, LAST_NAME_TYPE, &done_convert, ls);
  __ Bind(&convert);
  __ Push(x0);
  __ CallRuntime(Runtime::kToName);
  __ Bind(&done_convert);
  context()->Plug(x0);
}


void FullCodeGenerator::EmitStringCharFromCode(CallRuntime* expr) {
  ZoneList<Expression*>* args = expr->arguments();
  DCHECK(args->length() == 1);

  VisitForAccumulatorValue(args->at(0));

  Label done;
  Register code = x0;
  Register result = x1;

  StringCharFromCodeGenerator generator(code, result);
  generator.GenerateFast(masm_);
  __ B(&done);

  NopRuntimeCallHelper call_helper;
  generator.GenerateSlow(masm_, call_helper);

  __ Bind(&done);
  context()->Plug(result);
}


void FullCodeGenerator::EmitStringCharCodeAt(CallRuntime* expr) {
  ZoneList<Expression*>* args = expr->arguments();
  DCHECK(args->length() == 2);

  VisitForStackValue(args->at(0));
  VisitForAccumulatorValue(args->at(1));

  Register object = x1;
  Register index = x0;
  Register result = x3;

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


void FullCodeGenerator::EmitStringCharAt(CallRuntime* expr) {
  ZoneList<Expression*>* args = expr->arguments();
  DCHECK(args->length() == 2);

  VisitForStackValue(args->at(0));
  VisitForAccumulatorValue(args->at(1));

  Register object = x1;
  Register index = x0;
  Register result = x0;

  __ Pop(object);

  Label need_conversion;
  Label index_out_of_range;
  Label done;
  StringCharAtGenerator generator(object,
                                  index,
                                  x3,
                                  result,
                                  &need_conversion,
                                  &need_conversion,
                                  &index_out_of_range,
                                  STRING_INDEX_IS_NUMBER);
  generator.GenerateFast(masm_);
  __ B(&done);

  __ Bind(&index_out_of_range);
  // When the index is out of range, the spec requires us to return
  // the empty string.
  __ LoadRoot(result, Heap::kempty_stringRootIndex);
  __ B(&done);

  __ Bind(&need_conversion);
  // Move smi zero into the result register, which will trigger conversion.
  __ Mov(result, Smi::FromInt(0));
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
  PrepareForBailoutForId(expr->CallId(), NO_REGISTERS);
  // Move target to x1.
  int const argc = args->length() - 2;
  __ Peek(x1, (argc + 1) * kXRegSize);
  // Call the target.
  __ Mov(x0, argc);
  __ Call(isolate()->builtins()->Call(), RelocInfo::CODE_TARGET);
  // Restore context register.
  __ Ldr(cp, MemOperand(fp, StandardFrameConstants::kContextOffset));
  // Discard the function left on TOS.
  context()->DropAndPlug(1, x0);
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

  __ Ldr(x10, FieldMemOperand(x0, String::kHashFieldOffset));
  __ Tst(x10, String::kContainsCachedArrayIndexMask);
  PrepareForBailoutBeforeSplit(expr, true, if_true, if_false);
  Split(eq, if_true, if_false, fall_through);

  context()->Plug(if_true, if_false);
}


void FullCodeGenerator::EmitGetCachedArrayIndex(CallRuntime* expr) {
  ZoneList<Expression*>* args = expr->arguments();
  DCHECK(args->length() == 1);
  VisitForAccumulatorValue(args->at(0));

  __ AssertString(x0);

  __ Ldr(x10, FieldMemOperand(x0, String::kHashFieldOffset));
  __ IndexFromHash(x10, x0);

  context()->Plug(x0);
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


void FullCodeGenerator::EmitFastOneByteArrayJoin(CallRuntime* expr) {
  ASM_LOCATION("FullCodeGenerator::EmitFastOneByteArrayJoin");

  ZoneList<Expression*>* args = expr->arguments();
  DCHECK(args->length() == 2);
  VisitForStackValue(args->at(1));
  VisitForAccumulatorValue(args->at(0));

  Register array = x0;
  Register result = x0;
  Register elements = x1;
  Register element = x2;
  Register separator = x3;
  Register array_length = x4;
  Register result_pos = x5;
  Register map = x6;
  Register string_length = x10;
  Register elements_end = x11;
  Register string = x12;
  Register scratch1 = x13;
  Register scratch2 = x14;
  Register scratch3 = x7;
  Register separator_length = x15;

  Label bailout, done, one_char_separator, long_separator,
      non_trivial_array, not_size_one_array, loop,
      empty_separator_loop, one_char_separator_loop,
      one_char_separator_loop_entry, long_separator_loop;

  // The separator operand is on the stack.
  __ Pop(separator);

  // Check that the array is a JSArray.
  __ JumpIfSmi(array, &bailout);
  __ JumpIfNotObjectType(array, map, scratch1, JS_ARRAY_TYPE, &bailout);

  // Check that the array has fast elements.
  __ CheckFastElements(map, scratch1, &bailout);

  // If the array has length zero, return the empty string.
  // Load and untag the length of the array.
  // It is an unsigned value, so we can skip sign extension.
  // We assume little endianness.
  __ Ldrsw(array_length,
           UntagSmiFieldMemOperand(array, JSArray::kLengthOffset));
  __ Cbnz(array_length, &non_trivial_array);
  __ LoadRoot(result, Heap::kempty_stringRootIndex);
  __ B(&done);

  __ Bind(&non_trivial_array);
  // Get the FixedArray containing array's elements.
  __ Ldr(elements, FieldMemOperand(array, JSArray::kElementsOffset));

  // Check that all array elements are sequential one-byte strings, and
  // accumulate the sum of their lengths.
  __ Mov(string_length, 0);
  __ Add(element, elements, FixedArray::kHeaderSize - kHeapObjectTag);
  __ Add(elements_end, element, Operand(array_length, LSL, kPointerSizeLog2));
  // Loop condition: while (element < elements_end).
  // Live values in registers:
  //   elements: Fixed array of strings.
  //   array_length: Length of the fixed array of strings (not smi)
  //   separator: Separator string
  //   string_length: Accumulated sum of string lengths (not smi).
  //   element: Current array element.
  //   elements_end: Array end.
  if (FLAG_debug_code) {
    __ Cmp(array_length, 0);
    __ Assert(gt, kNoEmptyArraysHereInEmitFastOneByteArrayJoin);
  }
  __ Bind(&loop);
  __ Ldr(string, MemOperand(element, kPointerSize, PostIndex));
  __ JumpIfSmi(string, &bailout);
  __ Ldr(scratch1, FieldMemOperand(string, HeapObject::kMapOffset));
  __ Ldrb(scratch1, FieldMemOperand(scratch1, Map::kInstanceTypeOffset));
  __ JumpIfInstanceTypeIsNotSequentialOneByte(scratch1, scratch2, &bailout);
  __ Ldrsw(scratch1,
           UntagSmiFieldMemOperand(string, SeqOneByteString::kLengthOffset));
  __ Adds(string_length, string_length, scratch1);
  __ B(vs, &bailout);
  __ Cmp(element, elements_end);
  __ B(lt, &loop);

  // If array_length is 1, return elements[0], a string.
  __ Cmp(array_length, 1);
  __ B(ne, &not_size_one_array);
  __ Ldr(result, FieldMemOperand(elements, FixedArray::kHeaderSize));
  __ B(&done);

  __ Bind(&not_size_one_array);

  // Live values in registers:
  //   separator: Separator string
  //   array_length: Length of the array (not smi).
  //   string_length: Sum of string lengths (not smi).
  //   elements: FixedArray of strings.

  // Check that the separator is a flat one-byte string.
  __ JumpIfSmi(separator, &bailout);
  __ Ldr(scratch1, FieldMemOperand(separator, HeapObject::kMapOffset));
  __ Ldrb(scratch1, FieldMemOperand(scratch1, Map::kInstanceTypeOffset));
  __ JumpIfInstanceTypeIsNotSequentialOneByte(scratch1, scratch2, &bailout);

  // Add (separator length times array_length) - separator length to the
  // string_length to get the length of the result string.
  // Load the separator length as untagged.
  // We assume little endianness, and that the length is positive.
  __ Ldrsw(separator_length,
           UntagSmiFieldMemOperand(separator,
                                   SeqOneByteString::kLengthOffset));
  __ Sub(string_length, string_length, separator_length);
  __ Umaddl(string_length, array_length.W(), separator_length.W(),
            string_length);

  // Bailout for large object allocations.
  __ Cmp(string_length, Page::kMaxRegularHeapObjectSize);
  __ B(gt, &bailout);

  // Get first element in the array.
  __ Add(element, elements, FixedArray::kHeaderSize - kHeapObjectTag);
  // Live values in registers:
  //   element: First array element
  //   separator: Separator string
  //   string_length: Length of result string (not smi)
  //   array_length: Length of the array (not smi).
  __ AllocateOneByteString(result, string_length, scratch1, scratch2, scratch3,
                           &bailout);

  // Prepare for looping. Set up elements_end to end of the array. Set
  // result_pos to the position of the result where to write the first
  // character.
  // TODO(all): useless unless AllocateOneByteString trashes the register.
  __ Add(elements_end, element, Operand(array_length, LSL, kPointerSizeLog2));
  __ Add(result_pos, result, SeqOneByteString::kHeaderSize - kHeapObjectTag);

  // Check the length of the separator.
  __ Cmp(separator_length, 1);
  __ B(eq, &one_char_separator);
  __ B(gt, &long_separator);

  // Empty separator case
  __ Bind(&empty_separator_loop);
  // Live values in registers:
  //   result_pos: the position to which we are currently copying characters.
  //   element: Current array element.
  //   elements_end: Array end.

  // Copy next array element to the result.
  __ Ldr(string, MemOperand(element, kPointerSize, PostIndex));
  __ Ldrsw(string_length,
           UntagSmiFieldMemOperand(string, String::kLengthOffset));
  __ Add(string, string, SeqOneByteString::kHeaderSize - kHeapObjectTag);
  __ CopyBytes(result_pos, string, string_length, scratch1);
  __ Cmp(element, elements_end);
  __ B(lt, &empty_separator_loop);  // End while (element < elements_end).
  __ B(&done);

  // One-character separator case
  __ Bind(&one_char_separator);
  // Replace separator with its one-byte character value.
  __ Ldrb(separator, FieldMemOperand(separator, SeqOneByteString::kHeaderSize));
  // Jump into the loop after the code that copies the separator, so the first
  // element is not preceded by a separator
  __ B(&one_char_separator_loop_entry);

  __ Bind(&one_char_separator_loop);
  // Live values in registers:
  //   result_pos: the position to which we are currently copying characters.
  //   element: Current array element.
  //   elements_end: Array end.
  //   separator: Single separator one-byte char (in lower byte).

  // Copy the separator character to the result.
  __ Strb(separator, MemOperand(result_pos, 1, PostIndex));

  // Copy next array element to the result.
  __ Bind(&one_char_separator_loop_entry);
  __ Ldr(string, MemOperand(element, kPointerSize, PostIndex));
  __ Ldrsw(string_length,
           UntagSmiFieldMemOperand(string, String::kLengthOffset));
  __ Add(string, string, SeqOneByteString::kHeaderSize - kHeapObjectTag);
  __ CopyBytes(result_pos, string, string_length, scratch1);
  __ Cmp(element, elements_end);
  __ B(lt, &one_char_separator_loop);  // End while (element < elements_end).
  __ B(&done);

  // Long separator case (separator is more than one character). Entry is at the
  // label long_separator below.
  __ Bind(&long_separator_loop);
  // Live values in registers:
  //   result_pos: the position to which we are currently copying characters.
  //   element: Current array element.
  //   elements_end: Array end.
  //   separator: Separator string.

  // Copy the separator to the result.
  // TODO(all): hoist next two instructions.
  __ Ldrsw(string_length,
           UntagSmiFieldMemOperand(separator, String::kLengthOffset));
  __ Add(string, separator, SeqOneByteString::kHeaderSize - kHeapObjectTag);
  __ CopyBytes(result_pos, string, string_length, scratch1);

  __ Bind(&long_separator);
  __ Ldr(string, MemOperand(element, kPointerSize, PostIndex));
  __ Ldrsw(string_length,
           UntagSmiFieldMemOperand(string, String::kLengthOffset));
  __ Add(string, string, SeqOneByteString::kHeaderSize - kHeapObjectTag);
  __ CopyBytes(result_pos, string, string_length, scratch1);
  __ Cmp(element, elements_end);
  __ B(lt, &long_separator_loop);  // End while (element < elements_end).
  __ B(&done);

  __ Bind(&bailout);
  // Returning undefined will force slower code to handle it.
  __ LoadRoot(result, Heap::kUndefinedValueRootIndex);
  __ Bind(&done);
  context()->Plug(result);
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
  __ Allocate(JSIteratorResult::kSize, result, x10, x11, &runtime, TAG_OBJECT);
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
  __ CallRuntime(Runtime::kCreateIterResultObject);

  __ Bind(&done);
  context()->Plug(x0);
}


void FullCodeGenerator::EmitLoadJSRuntimeFunction(CallRuntime* expr) {
  // Push undefined as the receiver.
  __ LoadRoot(x0, Heap::kUndefinedValueRootIndex);
  __ Push(x0);

  __ LoadNativeContextSlot(expr->context_index(), x0);
}


void FullCodeGenerator::EmitCallJSRuntimeFunction(CallRuntime* expr) {
  ZoneList<Expression*>* args = expr->arguments();
  int arg_count = args->length();

  SetCallPosition(expr);
  __ Peek(x1, (arg_count + 1) * kPointerSize);
  __ Mov(x0, arg_count);
  __ Call(isolate()->builtins()->Call(ConvertReceiverMode::kNullOrUndefined),
          RelocInfo::CODE_TARGET);
}


void FullCodeGenerator::VisitCallRuntime(CallRuntime* expr) {
  ZoneList<Expression*>* args = expr->arguments();
  int arg_count = args->length();

  if (expr->is_jsruntime()) {
    Comment cmnt(masm_, "[ CallRunTime");
    EmitLoadJSRuntimeFunction(expr);

    // Push the target function under the receiver.
    __ Pop(x10);
    __ Push(x0, x10);

    for (int i = 0; i < arg_count; i++) {
      VisitForStackValue(args->at(i));
    }

    PrepareForBailoutForId(expr->CallId(), NO_REGISTERS);
    EmitCallJSRuntimeFunction(expr);

    // Restore context register.
    __ Ldr(cp, MemOperand(fp, StandardFrameConstants::kContextOffset));

    context()->DropAndPlug(1, x0);

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
        context()->Plug(x0);
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
        context()->Plug(x0);
      } else if (proxy != NULL) {
        Variable* var = proxy->var();
        // Delete of an unqualified identifier is disallowed in strict mode but
        // "delete this" is allowed.
        bool is_this = var->HasThisName(isolate());
        DCHECK(is_sloppy(language_mode()) || is_this);
        if (var->IsUnallocatedOrGlobalSlot()) {
          __ LoadGlobalObject(x12);
          __ Mov(x11, Operand(var->name()));
          __ Push(x12, x11);
          __ CallRuntime(Runtime::kDeleteProperty_Sloppy);
          context()->Plug(x0);
        } else if (var->IsStackAllocated() || var->IsContextSlot()) {
          // Result of deleting non-global, non-dynamic variables is false.
          // The subexpression does not have side effects.
          context()->Plug(is_this);
        } else {
          // Non-global variable.  Call the runtime to try to delete from the
          // context where the variable was introduced.
          __ Mov(x2, Operand(var->name()));
          __ Push(context_register(), x2);
          __ CallRuntime(Runtime::kDeleteLookupSlot);
          context()->Plug(x0);
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

        __ Bind(&materialize_true);
        PrepareForBailoutForId(expr->MaterializeTrueId(), NO_REGISTERS);
        __ LoadRoot(result_register(), Heap::kTrueValueRootIndex);
        __ B(&done);

        __ Bind(&materialize_false);
        PrepareForBailoutForId(expr->MaterializeFalseId(), NO_REGISTERS);
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
      TypeofStub typeof_stub(isolate());
      __ CallStub(&typeof_stub);
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
      __ Push(xzr);
    }
    switch (assign_type) {
      case NAMED_PROPERTY: {
        // Put the object both on the stack and in the register.
        VisitForStackValue(prop->obj());
        __ Peek(LoadDescriptor::ReceiverRegister(), 0);
        EmitNamedPropertyLoad(prop);
        break;
      }

      case NAMED_SUPER_PROPERTY: {
        VisitForStackValue(prop->obj()->AsSuperPropertyReference()->this_var());
        VisitForAccumulatorValue(
            prop->obj()->AsSuperPropertyReference()->home_object());
        __ Push(result_register());
        const Register scratch = x10;
        __ Peek(scratch, kPointerSize);
        __ Push(scratch, result_register());
        EmitNamedSuperPropertyLoad(prop);
        break;
      }

      case KEYED_SUPER_PROPERTY: {
        VisitForStackValue(prop->obj()->AsSuperPropertyReference()->this_var());
        VisitForStackValue(
            prop->obj()->AsSuperPropertyReference()->home_object());
        VisitForAccumulatorValue(prop->key());
        __ Push(result_register());
        const Register scratch1 = x10;
        const Register scratch2 = x11;
        __ Peek(scratch1, 2 * kPointerSize);
        __ Peek(scratch2, kPointerSize);
        __ Push(scratch1, scratch2, result_register());
        EmitKeyedSuperPropertyLoad(prop);
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
          case NAMED_SUPER_PROPERTY:
            __ Poke(x0, kPointerSize * 2);
            break;
          case KEYED_PROPERTY:
            __ Poke(x0, kPointerSize * 2);
            break;
          case KEYED_SUPER_PROPERTY:
            __ Poke(x0, kPointerSize * 3);
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
          __ Push(x0);
          break;
        case NAMED_PROPERTY:
          __ Poke(x0, kXRegSize);
          break;
        case NAMED_SUPER_PROPERTY:
          __ Poke(x0, 2 * kXRegSize);
          break;
        case KEYED_PROPERTY:
          __ Poke(x0, 2 * kXRegSize);
          break;
        case KEYED_SUPER_PROPERTY:
          __ Poke(x0, 3 * kXRegSize);
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
    Handle<Code> code =
        CodeFactory::BinaryOpIC(isolate(), Token::ADD,
                                strength(language_mode())).code();
    CallIC(code, expr->CountBinOpFeedbackId());
    patch_site.EmitPatchInfo();
  }
  __ Bind(&done);

  if (is_strong(language_mode())) {
    PrepareForBailoutForId(expr->ToNumberId(), TOS_REG);
  }
  // Store the value returned in x0.
  switch (assign_type) {
    case VARIABLE:
      if (expr->is_postfix()) {
        { EffectContext context(this);
          EmitVariableAssignment(expr->expression()->AsVariableProxy()->var(),
                                 Token::ASSIGN, expr->CountSlot());
          PrepareForBailoutForId(expr->AssignmentId(), TOS_REG);
          context.Plug(x0);
        }
        // For all contexts except EffectConstant We have the result on
        // top of the stack.
        if (!context()->IsEffect()) {
          context()->PlugTOS();
        }
      } else {
        EmitVariableAssignment(expr->expression()->AsVariableProxy()->var(),
                               Token::ASSIGN, expr->CountSlot());
        PrepareForBailoutForId(expr->AssignmentId(), TOS_REG);
        context()->Plug(x0);
      }
      break;
    case NAMED_PROPERTY: {
      __ Mov(StoreDescriptor::NameRegister(),
             Operand(prop->key()->AsLiteral()->value()));
      __ Pop(StoreDescriptor::ReceiverRegister());
      EmitLoadStoreICSlot(expr->CountSlot());
      CallStoreIC();
      PrepareForBailoutForId(expr->AssignmentId(), TOS_REG);
      if (expr->is_postfix()) {
        if (!context()->IsEffect()) {
          context()->PlugTOS();
        }
      } else {
        context()->Plug(x0);
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
        context()->Plug(x0);
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
        context()->Plug(x0);
      }
      break;
    }
    case KEYED_PROPERTY: {
      __ Pop(StoreDescriptor::NameRegister());
      __ Pop(StoreDescriptor::ReceiverRegister());
      Handle<Code> ic =
          CodeFactory::KeyedStoreIC(isolate(), language_mode()).code();
      EmitLoadStoreICSlot(expr->CountSlot());
      CallIC(ic);
      PrepareForBailoutForId(expr->AssignmentId(), TOS_REG);
      if (expr->is_postfix()) {
        if (!context()->IsEffect()) {
          context()->PlugTOS();
        }
      } else {
        context()->Plug(x0);
      }
      break;
    }
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
    __ JumpIfRoot(x0, Heap::kUndefinedValueRootIndex, if_true);
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
  SetExpressionPosition(expr);

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
      __ CallRuntime(Runtime::kHasProperty);
      PrepareForBailoutBeforeSplit(expr, false, NULL, NULL);
      __ CompareRoot(x0, Heap::kTrueValueRootIndex);
      Split(eq, if_true, if_false, fall_through);
      break;

    case Token::INSTANCEOF: {
      VisitForAccumulatorValue(expr->right());
      __ Pop(x1);
      InstanceOfStub stub(isolate());
      __ CallStub(&stub);
      PrepareForBailoutBeforeSplit(expr, false, NULL, NULL);
      __ CompareRoot(x0, Heap::kTrueValueRootIndex);
      Split(eq, if_true, if_false, fall_through);
      break;
    }

    default: {
      VisitForAccumulatorValue(expr->right());
      Condition cond = CompareIC::ComputeCondition(op);

      // Pop the stack value.
      __ Pop(x1);

      JumpPatchSite patch_site(masm_);
      if (ShouldInlineSmiCase(op)) {
        Label slow_case;
        patch_site.EmitJumpIfEitherNotSmi(x0, x1, &slow_case);
        __ Cmp(x1, x0);
        Split(cond, if_true, if_false, NULL);
        __ Bind(&slow_case);
      }

      Handle<Code> ic = CodeFactory::CompareIC(
                            isolate(), op, strength(language_mode())).code();
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
    Handle<Code> ic = CompareNilICStub::GetUninitialized(isolate(), nil);
    CallIC(ic, expr->CompareOperationFeedbackId());
    __ CompareRoot(x0, Heap::kTrueValueRootIndex);
    Split(eq, if_true, if_false, fall_through);
  }

  context()->Plug(if_true, if_false);
}


void FullCodeGenerator::VisitThisFunction(ThisFunction* expr) {
  __ Ldr(x0, MemOperand(fp, JavaScriptFrameConstants::kFunctionOffset));
  context()->Plug(x0);
}


void FullCodeGenerator::VisitYield(Yield* expr) {
  Comment cmnt(masm_, "[ Yield");
  SetExpressionPosition(expr);

  // Evaluate yielded value first; the initial iterator definition depends on
  // this. It stays on the stack while we update the iterator.
  VisitForStackValue(expr->expression());

  // TODO(jbramley): Tidy this up once the merge is done, using named registers
  // and suchlike. The implementation changes a little by bleeding_edge so I
  // don't want to spend too much time on it now.

  switch (expr->yield_kind()) {
    case Yield::kSuspend:
      // Pop value from top-of-stack slot; box result into result register.
      EmitCreateIteratorResult(false);
      __ Push(result_register());
      // Fall through.
    case Yield::kInitial: {
      Label suspend, continuation, post_runtime, resume;

      __ B(&suspend);
      // TODO(jbramley): This label is bound here because the following code
      // looks at its pos(). Is it possible to do something more efficient here,
      // perhaps using Adr?
      __ Bind(&continuation);
      __ RecordGeneratorContinuation();
      __ B(&resume);

      __ Bind(&suspend);
      VisitForAccumulatorValue(expr->generator_object());
      DCHECK((continuation.pos() > 0) && Smi::IsValid(continuation.pos()));
      __ Mov(x1, Smi::FromInt(continuation.pos()));
      __ Str(x1, FieldMemOperand(x0, JSGeneratorObject::kContinuationOffset));
      __ Str(cp, FieldMemOperand(x0, JSGeneratorObject::kContextOffset));
      __ Mov(x1, cp);
      __ RecordWriteField(x0, JSGeneratorObject::kContextOffset, x1, x2,
                          kLRHasBeenSaved, kDontSaveFPRegs);
      __ Add(x1, fp, StandardFrameConstants::kExpressionsOffset);
      __ Cmp(__ StackPointer(), x1);
      __ B(eq, &post_runtime);
      __ Push(x0);  // generator object
      __ CallRuntime(Runtime::kSuspendJSGeneratorObject, 1);
      __ Ldr(cp, MemOperand(fp, StandardFrameConstants::kContextOffset));
      __ Bind(&post_runtime);
      __ Pop(result_register());
      EmitReturnSequence();

      __ Bind(&resume);
      context()->Plug(result_register());
      break;
    }

    case Yield::kFinal: {
      VisitForAccumulatorValue(expr->generator_object());
      __ Mov(x1, Smi::FromInt(JSGeneratorObject::kGeneratorClosed));
      __ Str(x1, FieldMemOperand(result_register(),
                                 JSGeneratorObject::kContinuationOffset));
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
      __ LoadRoot(x0, Heap::kUndefinedValueRootIndex);
      __ B(&l_next);

      // catch (e) { receiver = iter; f = 'throw'; arg = e; goto l_call; }
      __ Bind(&l_catch);
      __ LoadRoot(load_name, Heap::kthrow_stringRootIndex);  // "throw"
      __ Peek(x3, 1 * kPointerSize);                         // iter
      __ Push(load_name, x3, x0);                       // "throw", iter, except
      __ B(&l_call);

      // try { received = %yield result }
      // Shuffle the received result above a try handler and yield it without
      // re-boxing.
      __ Bind(&l_try);
      __ Pop(x0);                                        // result
      int handler_index = NewHandlerTableEntry();
      EnterTryBlock(handler_index, &l_catch);
      const int try_block_size = TryCatch::kElementCount * kPointerSize;
      __ Push(x0);                                       // result

      __ B(&l_suspend);
      // TODO(jbramley): This label is bound here because the following code
      // looks at its pos(). Is it possible to do something more efficient here,
      // perhaps using Adr?
      __ Bind(&l_continuation);
      __ RecordGeneratorContinuation();
      __ B(&l_resume);

      __ Bind(&l_suspend);
      const int generator_object_depth = kPointerSize + try_block_size;
      __ Peek(x0, generator_object_depth);
      __ Push(x0);                                       // g
      __ Push(Smi::FromInt(handler_index));              // handler-index
      DCHECK((l_continuation.pos() > 0) && Smi::IsValid(l_continuation.pos()));
      __ Mov(x1, Smi::FromInt(l_continuation.pos()));
      __ Str(x1, FieldMemOperand(x0, JSGeneratorObject::kContinuationOffset));
      __ Str(cp, FieldMemOperand(x0, JSGeneratorObject::kContextOffset));
      __ Mov(x1, cp);
      __ RecordWriteField(x0, JSGeneratorObject::kContextOffset, x1, x2,
                          kLRHasBeenSaved, kDontSaveFPRegs);
      __ CallRuntime(Runtime::kSuspendJSGeneratorObject, 2);
      __ Ldr(cp, MemOperand(fp, StandardFrameConstants::kContextOffset));
      __ Pop(x0);                                        // result
      EmitReturnSequence();
      __ Bind(&l_resume);                                // received in x0
      ExitTryBlock(handler_index);

      // receiver = iter; f = 'next'; arg = received;
      __ Bind(&l_next);

      __ LoadRoot(load_name, Heap::knext_stringRootIndex);  // "next"
      __ Peek(x3, 1 * kPointerSize);                        // iter
      __ Push(load_name, x3, x0);                      // "next", iter, received

      // result = receiver[f](arg);
      __ Bind(&l_call);
      __ Peek(load_receiver, 1 * kPointerSize);
      __ Peek(load_name, 2 * kPointerSize);
      __ Mov(LoadDescriptor::SlotRegister(),
             SmiFromSlot(expr->KeyedLoadFeedbackSlot()));
      Handle<Code> ic = CodeFactory::KeyedLoadIC(isolate(), SLOPPY).code();
      CallIC(ic, TypeFeedbackId::None());
      __ Mov(x1, x0);
      __ Poke(x1, 2 * kPointerSize);
      SetCallPosition(expr);
      __ Mov(x0, 1);
      __ Call(
          isolate()->builtins()->Call(ConvertReceiverMode::kNotNullOrUndefined),
          RelocInfo::CODE_TARGET);

      __ Ldr(cp, MemOperand(fp, StandardFrameConstants::kContextOffset));
      __ Drop(1);  // The function is still on the stack; drop it.

      // if (!result.done) goto l_try;
      __ Bind(&l_loop);
      __ Move(load_receiver, x0);

      __ Push(load_receiver);                               // save result
      __ LoadRoot(load_name, Heap::kdone_stringRootIndex);  // "done"
      __ Mov(LoadDescriptor::SlotRegister(),
             SmiFromSlot(expr->DoneFeedbackSlot()));
      CallLoadIC(NOT_INSIDE_TYPEOF);  // x0=result.done
      // The ToBooleanStub argument (result.done) is in x0.
      Handle<Code> bool_ic = ToBooleanStub::GetUninitialized(isolate());
      CallIC(bool_ic);
      __ CompareRoot(result_register(), Heap::kTrueValueRootIndex);
      __ B(ne, &l_try);

      // result.value
      __ Pop(load_receiver);                                 // result
      __ LoadRoot(load_name, Heap::kvalue_stringRootIndex);  // "value"
      __ Mov(LoadDescriptor::SlotRegister(),
             SmiFromSlot(expr->ValueFeedbackSlot()));
      CallLoadIC(NOT_INSIDE_TYPEOF);                         // x0=result.value
      context()->DropAndPlug(2, x0);                         // drop iter and g
      break;
    }
  }
}


void FullCodeGenerator::EmitGeneratorResume(Expression *generator,
    Expression *value,
    JSGeneratorObject::ResumeMode resume_mode) {
  ASM_LOCATION("FullCodeGenerator::EmitGeneratorResume");
  Register generator_object = x1;
  Register the_hole = x2;
  Register operand_stack_size = w3;
  Register function = x4;

  // The value stays in x0, and is ultimately read by the resumed generator, as
  // if CallRuntime(Runtime::kSuspendJSGeneratorObject) returned it. Or it
  // is read to throw the value when the resumed generator is already closed. x1
  // will hold the generator object until the activation has been resumed.
  VisitForStackValue(generator);
  VisitForAccumulatorValue(value);
  __ Pop(generator_object);

  // Load suspended function and context.
  __ Ldr(cp, FieldMemOperand(generator_object,
                             JSGeneratorObject::kContextOffset));
  __ Ldr(function, FieldMemOperand(generator_object,
                                   JSGeneratorObject::kFunctionOffset));

  // Load receiver and store as the first argument.
  __ Ldr(x10, FieldMemOperand(generator_object,
                              JSGeneratorObject::kReceiverOffset));
  __ Push(x10);

  // Push holes for the rest of the arguments to the generator function.
  __ Ldr(x10, FieldMemOperand(function, JSFunction::kSharedFunctionInfoOffset));

  // The number of arguments is stored as an int32_t, and -1 is a marker
  // (SharedFunctionInfo::kDontAdaptArgumentsSentinel), so we need sign
  // extension to correctly handle it. However, in this case, we operate on
  // 32-bit W registers, so extension isn't required.
  __ Ldr(w10, FieldMemOperand(x10,
                              SharedFunctionInfo::kFormalParameterCountOffset));
  __ LoadRoot(the_hole, Heap::kTheHoleValueRootIndex);
  __ PushMultipleTimes(the_hole, w10);

  // Enter a new JavaScript frame, and initialize its slots as they were when
  // the generator was suspended.
  Label resume_frame, done;
  __ Bl(&resume_frame);
  __ B(&done);

  __ Bind(&resume_frame);
  __ Push(lr,           // Return address.
          fp,           // Caller's frame pointer.
          cp,           // Callee's context.
          function);    // Callee's JS Function.
  __ Add(fp, __ StackPointer(), kPointerSize * 2);

  // Load and untag the operand stack size.
  __ Ldr(x10, FieldMemOperand(generator_object,
                              JSGeneratorObject::kOperandStackOffset));
  __ Ldr(operand_stack_size,
         UntagSmiFieldMemOperand(x10, FixedArray::kLengthOffset));

  // If we are sending a value and there is no operand stack, we can jump back
  // in directly.
  if (resume_mode == JSGeneratorObject::NEXT) {
    Label slow_resume;
    __ Cbnz(operand_stack_size, &slow_resume);
    __ Ldr(x10, FieldMemOperand(function, JSFunction::kCodeEntryOffset));
    __ Ldrsw(x11,
             UntagSmiFieldMemOperand(generator_object,
                                     JSGeneratorObject::kContinuationOffset));
    __ Add(x10, x10, x11);
    __ Mov(x12, Smi::FromInt(JSGeneratorObject::kGeneratorExecuting));
    __ Str(x12, FieldMemOperand(generator_object,
                                JSGeneratorObject::kContinuationOffset));
    __ Br(x10);

    __ Bind(&slow_resume);
  }

  // Otherwise, we push holes for the operand stack and call the runtime to fix
  // up the stack and the handlers.
  __ PushMultipleTimes(the_hole, operand_stack_size);

  __ Mov(x10, Smi::FromInt(resume_mode));
  __ Push(generator_object, result_register(), x10);
  __ CallRuntime(Runtime::kResumeJSGeneratorObject);
  // Not reached: the runtime call returns elsewhere.
  __ Unreachable();

  __ Bind(&done);
  context()->Plug(result_register());
}


void FullCodeGenerator::EmitCreateIteratorResult(bool done) {
  Label allocate, done_allocate;

  // Allocate and populate an object with this form: { value: VAL, done: DONE }

  Register result = x0;
  __ Allocate(JSIteratorResult::kSize, result, x10, x11, &allocate, TAG_OBJECT);
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
  __ Pop(result_value);
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


void FullCodeGenerator::StoreToFrameField(int frame_offset, Register value) {
  DCHECK(POINTER_SIZE_ALIGN(frame_offset) == frame_offset);
  __ Str(value, MemOperand(fp, frame_offset));
}


void FullCodeGenerator::LoadContextField(Register dst, int context_index) {
  __ Ldr(dst, ContextMemOperand(cp, context_index));
}


void FullCodeGenerator::PushFunctionArgumentForContextAllocation() {
  Scope* closure_scope = scope()->ClosureScope();
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
  __ Push(x10);
}


void FullCodeGenerator::EnterFinallyBlock() {
  ASM_LOCATION("FullCodeGenerator::EnterFinallyBlock");
  DCHECK(!result_register().is(x10));
  // Preserve the result register while executing finally block.
  // Also cook the return address in lr to the stack (smi encoded Code* delta).
  __ Sub(x10, lr, Operand(masm_->CodeObject()));
  __ SmiTag(x10);
  __ Push(result_register(), x10);

  // Store pending message while executing finally block.
  ExternalReference pending_message_obj =
      ExternalReference::address_of_pending_message_obj(isolate());
  __ Mov(x10, pending_message_obj);
  __ Ldr(x10, MemOperand(x10));
  __ Push(x10);

  ClearPendingMessage();
}


void FullCodeGenerator::ExitFinallyBlock() {
  ASM_LOCATION("FullCodeGenerator::ExitFinallyBlock");
  DCHECK(!result_register().is(x10));

  // Restore pending message from stack.
  __ Pop(x10);
  ExternalReference pending_message_obj =
      ExternalReference::address_of_pending_message_obj(isolate());
  __ Mov(x13, pending_message_obj);
  __ Str(x10, MemOperand(x13));

  // Restore result register and cooked return address from the stack.
  __ Pop(x10, result_register());

  // Uncook the return address (see EnterFinallyBlock).
  __ SmiUntag(x10);
  __ Add(x11, x10, Operand(masm_->CodeObject()));
  __ Br(x11);
}


void FullCodeGenerator::ClearPendingMessage() {
  DCHECK(!result_register().is(x10));
  ExternalReference pending_message_obj =
      ExternalReference::address_of_pending_message_obj(isolate());
  __ LoadRoot(x10, Heap::kTheHoleValueRootIndex);
  __ Mov(x13, pending_message_obj);
  __ Str(x10, MemOperand(x13));
}


void FullCodeGenerator::EmitLoadStoreICSlot(FeedbackVectorSlot slot) {
  DCHECK(!slot.IsInvalid());
  __ Mov(VectorStoreICTrampolineDescriptor::SlotRegister(), SmiFromSlot(slot));
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
    case OSR_AFTER_STACK_CHECK:
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
              isolate->builtins()->OsrAfterStackCheck()->entry())) ||
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
    } else if (entry == reinterpret_cast<uint64_t>(
        isolate->builtins()->OsrAfterStackCheck()->entry())) {
      return OSR_AFTER_STACK_CHECK;
    } else {
      UNREACHABLE();
    }
  }

  return INTERRUPT;
}


}  // namespace internal
}  // namespace v8

#endif  // V8_TARGET_ARCH_ARM64
