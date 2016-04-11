// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if V8_TARGET_ARCH_ARM

#include "src/ast/scopes.h"
#include "src/code-factory.h"
#include "src/code-stubs.h"
#include "src/codegen.h"
#include "src/debug/debug.h"
#include "src/full-codegen/full-codegen.h"
#include "src/ic/ic.h"
#include "src/parsing/parser.h"

#include "src/arm/code-stubs-arm.h"
#include "src/arm/macro-assembler-arm.h"

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
    DCHECK(patch_site_.is_bound() == info_emitted_);
  }

  // When initially emitting this ensure that a jump is always generated to skip
  // the inlined smi code.
  void EmitJumpIfNotSmi(Register reg, Label* target) {
    DCHECK(!patch_site_.is_bound() && !info_emitted_);
    Assembler::BlockConstPoolScope block_const_pool(masm_);
    __ bind(&patch_site_);
    __ cmp(reg, Operand(reg));
    __ b(eq, target);  // Always taken before patched.
  }

  // When initially emitting this ensure that a jump is never generated to skip
  // the inlined smi code.
  void EmitJumpIfSmi(Register reg, Label* target) {
    DCHECK(!patch_site_.is_bound() && !info_emitted_);
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


// Generate code for a JS function.  On entry to the function the receiver
// and arguments have been pushed on the stack left to right.  The actual
// argument count matches the formal parameter count expected by the
// function.
//
// The live registers are:
//   o r1: the JS function object being called (i.e., ourselves)
//   o r3: the new target value
//   o cp: our context
//   o pp: our caller's constant pool pointer (if enabled)
//   o fp: our caller's frame pointer
//   o sp: stack pointer
//   o lr: return address
//
// The function builds a JS frame.  Please see JavaScriptFrameConstants in
// frames-arm.h for its layout.
void FullCodeGenerator::Generate() {
  CompilationInfo* info = info_;
  profiling_counter_ = isolate()->factory()->NewCell(
      Handle<Smi>(Smi::FromInt(FLAG_interrupt_budget), isolate()));
  SetFunctionPosition(literal());
  Comment cmnt(masm_, "[ function compiled by full code generator");

  ProfileEntryHookStub::MaybeCallEntryHook(masm_);

#ifdef DEBUG
  if (strlen(FLAG_stop_at) > 0 &&
      info->literal()->name()->IsUtf8EqualTo(CStrVector(FLAG_stop_at))) {
    __ stop("stop-at");
  }
#endif

  if (FLAG_debug_code && info->ExpectsJSReceiverAsReceiver()) {
    int receiver_offset = info->scope()->num_parameters() * kPointerSize;
    __ ldr(r2, MemOperand(sp, receiver_offset));
    __ AssertNotSmi(r2);
    __ CompareObjectType(r2, r2, no_reg, FIRST_JS_RECEIVER_TYPE);
    __ Assert(ge, kSloppyFunctionExpectsJSReceiverReceiver);
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
    DCHECK(!IsGeneratorFunction(info->literal()->kind()) || locals_count == 0);
    if (locals_count > 0) {
      if (locals_count >= 128) {
        Label ok;
        __ sub(r9, sp, Operand(locals_count * kPointerSize));
        __ LoadRoot(r2, Heap::kRealStackLimitRootIndex);
        __ cmp(r9, Operand(r2));
        __ b(hs, &ok);
        __ CallRuntime(Runtime::kThrowStackOverflow);
        __ bind(&ok);
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

  bool function_in_register_r1 = true;

  // Possibly allocate a local context.
  if (info->scope()->num_heap_slots() > 0) {
    // Argument to NewContext is the function, which is still in r1.
    Comment cmnt(masm_, "[ Allocate context");
    bool need_write_barrier = true;
    int slots = info->scope()->num_heap_slots() - Context::MIN_CONTEXT_SLOTS;
    if (info->scope()->is_script_scope()) {
      __ push(r1);
      __ Push(info->scope()->GetScopeInfo(info->isolate()));
      __ CallRuntime(Runtime::kNewScriptContext);
      PrepareForBailoutForId(BailoutId::ScriptContext(), TOS_REG);
      // The new target value is not used, clobbering is safe.
      DCHECK_NULL(info->scope()->new_target_var());
    } else {
      if (info->scope()->new_target_var() != nullptr) {
        __ push(r3);  // Preserve new target.
      }
      if (slots <= FastNewContextStub::kMaximumSlots) {
        FastNewContextStub stub(isolate(), slots);
        __ CallStub(&stub);
        // Result of FastNewContextStub is always in new space.
        need_write_barrier = false;
      } else {
        __ push(r1);
        __ CallRuntime(Runtime::kNewFunctionContext);
      }
      if (info->scope()->new_target_var() != nullptr) {
        __ pop(r3);  // Preserve new target.
      }
    }
    function_in_register_r1 = false;
    // Context is returned in r0.  It replaces the context passed to us.
    // It's saved in the stack and kept live in cp.
    __ mov(cp, r0);
    __ str(r0, MemOperand(fp, StandardFrameConstants::kContextOffset));
    // Copy any necessary parameters into the context.
    int num_parameters = info->scope()->num_parameters();
    int first_parameter = info->scope()->has_this_declaration() ? -1 : 0;
    for (int i = first_parameter; i < num_parameters; i++) {
      Variable* var = (i == -1) ? scope()->receiver() : scope()->parameter(i);
      if (var->IsContextSlot()) {
        int parameter_offset = StandardFrameConstants::kCallerSPOffset +
            (num_parameters - 1 - i) * kPointerSize;
        // Load parameter from stack.
        __ ldr(r0, MemOperand(fp, parameter_offset));
        // Store it in the context.
        MemOperand target = ContextMemOperand(cp, var->index());
        __ str(r0, target);

        // Update the write barrier.
        if (need_write_barrier) {
          __ RecordWriteContextSlot(cp, target.offset(), r0, r2,
                                    kLRHasBeenSaved, kDontSaveFPRegs);
        } else if (FLAG_debug_code) {
          Label done;
          __ JumpIfInNewSpace(cp, r0, &done);
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
    if (!function_in_register_r1) {
      __ ldr(r1, MemOperand(fp, JavaScriptFrameConstants::kFunctionOffset));
      // The write barrier clobbers register again, keep it marked as such.
    }
    SetVar(this_function_var, r1, r0, r2);
  }

  // Possibly set up a local binding to the new target value.
  Variable* new_target_var = scope()->new_target_var();
  if (new_target_var != nullptr) {
    Comment cmnt(masm_, "[ new.target");
    SetVar(new_target_var, r3, r0, r2);
  }

  // Possibly allocate RestParameters
  int rest_index;
  Variable* rest_param = scope()->rest_parameter(&rest_index);
  if (rest_param) {
    Comment cmnt(masm_, "[ Allocate rest parameter array");

    int num_parameters = info->scope()->num_parameters();
    int offset = num_parameters * kPointerSize;

    __ mov(RestParamAccessDescriptor::parameter_count(),
           Operand(Smi::FromInt(num_parameters)));
    __ add(RestParamAccessDescriptor::parameter_pointer(), fp,
           Operand(StandardFrameConstants::kCallerSPOffset + offset));
    __ mov(RestParamAccessDescriptor::rest_parameter_index(),
           Operand(Smi::FromInt(rest_index)));
    function_in_register_r1 = false;

    RestParamAccessStub stub(isolate());
    __ CallStub(&stub);

    SetVar(rest_param, r0, r1, r2);
  }

  Variable* arguments = scope()->arguments();
  if (arguments != NULL) {
    // Function uses arguments object.
    Comment cmnt(masm_, "[ Allocate arguments object");
    DCHECK(r1.is(ArgumentsAccessNewDescriptor::function()));
    if (!function_in_register_r1) {
      // Load this again, if it's used by the local context below.
      __ ldr(r1, MemOperand(fp, JavaScriptFrameConstants::kFunctionOffset));
    }
    // Receiver is just before the parameters on the caller's stack.
    int num_parameters = info->scope()->num_parameters();
    int offset = num_parameters * kPointerSize;
    __ mov(ArgumentsAccessNewDescriptor::parameter_count(),
           Operand(Smi::FromInt(num_parameters)));
    __ add(ArgumentsAccessNewDescriptor::parameter_pointer(), fp,
           Operand(StandardFrameConstants::kCallerSPOffset + offset));

    // Arguments to ArgumentsAccessStub:
    //   function, parameter pointer, parameter count.
    // The stub will rewrite parameter pointer and parameter count if the
    // previous stack frame was an arguments adapter frame.
    bool is_unmapped = is_strict(language_mode()) || !has_simple_parameters();
    ArgumentsAccessStub::Type type = ArgumentsAccessStub::ComputeType(
        is_unmapped, literal()->has_duplicate_parameters());
    ArgumentsAccessStub stub(isolate(), type);
    __ CallStub(&stub);

    SetVar(arguments, r0, r1, r2);
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
      __ LoadRoot(ip, Heap::kStackLimitRootIndex);
      __ cmp(sp, Operand(ip));
      __ b(hs, &ok);
      Handle<Code> stack_check = isolate()->builtins()->StackCheck();
      PredictableCodeSizeScope predictable(masm_);
      predictable.ExpectSize(
          masm_->CallSize(stack_check, RelocInfo::CODE_TARGET));
      __ Call(stack_check, RelocInfo::CODE_TARGET);
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


#ifdef CAN_USE_ARMV7_INSTRUCTIONS
static const int kProfileCounterResetSequenceLength = 5 * Assembler::kInstrSize;
#else
static const int kProfileCounterResetSequenceLength = 7 * Assembler::kInstrSize;
#endif


void FullCodeGenerator::EmitProfilingCounterReset() {
  Assembler::BlockConstPoolScope block_const_pool(masm_);
  PredictableCodeSizeScope predictable_code_size_scope(
      masm_, kProfileCounterResetSequenceLength);
  Label start;
  __ bind(&start);
  int reset_value = FLAG_interrupt_budget;
  __ mov(r2, Operand(profiling_counter_));
  // The mov instruction above can be either 1 to 3 (for ARMv7) or 1 to 5
  // instructions (for ARMv6) depending upon whether it is an extended constant
  // pool - insert nop to compensate.
  int expected_instr_count =
      (kProfileCounterResetSequenceLength / Assembler::kInstrSize) - 2;
  DCHECK(masm_->InstructionsGeneratedSince(&start) <= expected_instr_count);
  while (masm_->InstructionsGeneratedSince(&start) != expected_instr_count) {
    __ nop();
  }
  __ mov(r3, Operand(Smi::FromInt(reset_value)));
  __ str(r3, FieldMemOperand(r2, Cell::kValueOffset));
}


void FullCodeGenerator::EmitBackEdgeBookkeeping(IterationStatement* stmt,
                                                Label* back_edge_target) {
  Comment cmnt(masm_, "[ Back edge bookkeeping");
  // Block literal pools whilst emitting back edge code.
  Assembler::BlockConstPoolScope block_const_pool(masm_);
  Label ok;

  DCHECK(back_edge_target->is_bound());
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
    __ b(pl, &ok);
    __ push(r0);
    __ Call(isolate()->builtins()->InterruptCheck(),
            RelocInfo::CODE_TARGET);
    __ pop(r0);
    EmitProfilingCounterReset();
    __ bind(&ok);

    // Make sure that the constant pool is not emitted inside of the return
    // sequence.
    { Assembler::BlockConstPoolScope block_const_pool(masm_);
      int32_t arg_count = info_->scope()->num_parameters() + 1;
      int32_t sp_delta = arg_count * kPointerSize;
      SetReturnPosition(literal());
      // TODO(svenpanne) The code below is sometimes 4 words, sometimes 5!
      PredictableCodeSizeScope predictable(masm_, -1);
      __ LeaveFrame(StackFrame::JAVA_SCRIPT);
      { ConstantPoolUnavailableScope constant_pool_unavailable(masm_);
        __ add(sp, sp, Operand(sp_delta));
        __ Jump(lr);
      }
    }
  }
}


void FullCodeGenerator::StackValueContext::Plug(Variable* var) const {
  DCHECK(var->IsStackAllocated() || var->IsContextSlot());
  codegen()->GetVar(result_register(), var);
  __ push(result_register());
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
  DCHECK(!lit->IsUndetectableObject());  // There are no undetectable literals.
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
  __ str(reg, MemOperand(sp, 0));
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
  __ CompareRoot(result_register(), Heap::kTrueValueRootIndex);
  Split(eq, if_true, if_false, fall_through);
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
    return ContextMemOperand(scratch, var->index());
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
  DCHECK(var->IsContextSlot() || var->IsStackAllocated());
  DCHECK(!scratch0.is(src));
  DCHECK(!scratch0.is(scratch1));
  DCHECK(!scratch1.is(src));
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
  if (!context()->IsTest()) return;

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
  DCHECK_EQ(0, scope()->ContextChainLength(variable->scope()));
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
        __ LoadRoot(r0, Heap::kTheHoleValueRootIndex);
        __ str(r0, StackOperand(variable));
      }
      break;

    case VariableLocation::CONTEXT:
      if (hole_init) {
        Comment cmnt(masm_, "[ VariableDeclaration");
        EmitDebugCheckDeclarationContext(variable);
        __ LoadRoot(r0, Heap::kTheHoleValueRootIndex);
        __ str(r0, ContextMemOperand(cp, variable->index()));
        // No write barrier since the_hole_value is in old space.
        PrepareForBailoutForId(proxy->id(), NO_REGISTERS);
      }
      break;

    case VariableLocation::LOOKUP: {
      Comment cmnt(masm_, "[ VariableDeclaration");
      __ mov(r2, Operand(variable->name()));
      // Declaration nodes are always introduced in one of four modes.
      DCHECK(IsDeclaredVariableMode(mode));
      // Push initial value, if any.
      // Note: For variables we must not push an initial value (such as
      // 'undefined') because we may have a (legal) redeclaration and we
      // must not destroy the current value.
      if (hole_init) {
        __ LoadRoot(r0, Heap::kTheHoleValueRootIndex);
      } else {
        __ mov(r0, Operand(Smi::FromInt(0)));  // Indicates no initial value.
      }
      __ Push(r2, r0);
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
      // Check for stack-overflow exception.
      if (function.is_null()) return SetStackOverflow();
      globals_->Add(function, zone());
      break;
    }

    case VariableLocation::PARAMETER:
    case VariableLocation::LOCAL: {
      Comment cmnt(masm_, "[ FunctionDeclaration");
      VisitForAccumulatorValue(declaration->fun());
      __ str(result_register(), StackOperand(variable));
      break;
    }

    case VariableLocation::CONTEXT: {
      Comment cmnt(masm_, "[ FunctionDeclaration");
      EmitDebugCheckDeclarationContext(variable);
      VisitForAccumulatorValue(declaration->fun());
      __ str(result_register(), ContextMemOperand(cp, variable->index()));
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

    case VariableLocation::LOOKUP: {
      Comment cmnt(masm_, "[ FunctionDeclaration");
      __ mov(r2, Operand(variable->name()));
      __ Push(r2);
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
  __ mov(r1, Operand(pairs));
  __ mov(r0, Operand(Smi::FromInt(DeclareGlobalsFlags())));
  __ Push(r1, r0);
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
    SetExpressionPosition(clause);
    Handle<Code> ic = CodeFactory::CompareIC(isolate(), Token::EQ_STRICT,
                                             strength(language_mode())).code();
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
  SetStatementPosition(stmt, SKIP_BREAK);

  FeedbackVectorSlot slot = stmt->ForInFeedbackSlot();

  Label loop, exit;
  ForIn loop_statement(this, stmt);
  increment_loop_depth();

  // Get the object to enumerate over. If the object is null or undefined, skip
  // over the loop.  See ECMA-262 version 5, section 12.6.4.
  SetExpressionAsStatementPosition(stmt->enumerable());
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
  __ CompareObjectType(r0, r1, r1, FIRST_JS_RECEIVER_TYPE);
  __ b(ge, &done_convert);
  __ bind(&convert);
  ToObjectStub stub(isolate());
  __ CallStub(&stub);
  __ bind(&done_convert);
  PrepareForBailoutForId(stmt->ToObjectId(), TOS_REG);
  __ push(r0);

  // Check for proxies.
  Label call_runtime;
  __ CompareObjectType(r0, r1, r1, JS_PROXY_TYPE);
  __ b(eq, &call_runtime);

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
  __ CallRuntime(Runtime::kGetPropertyNamesFast);
  PrepareForBailoutForId(stmt->EnumId(), TOS_REG);

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
  __ bind(&fixed_array);

  __ EmitLoadTypeFeedbackVector(r1);
  __ mov(r2, Operand(TypeFeedbackVector::MegamorphicSentinel(isolate())));
  int vector_index = SmiFromSlot(slot)->value();
  __ str(r2, FieldMemOperand(r1, FixedArray::OffsetOfElementAt(vector_index)));
  __ mov(r1, Operand(Smi::FromInt(1)));  // Smi(1) indicates slow check
  __ Push(r1, r0);  // Smi and array
  __ ldr(r1, FieldMemOperand(r0, FixedArray::kLengthOffset));
  __ mov(r0, Operand(Smi::FromInt(0)));
  __ Push(r1, r0);  // Fixed array length (as smi) and initial index.

  // Generate code for doing the condition check.
  __ bind(&loop);
  SetExpressionAsStatementPosition(stmt->each());

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

  // Convert the entry to a string or (smi) 0 if it isn't a property
  // any more. If the property has been removed while iterating, we
  // just skip it.
  __ push(r1);  // Enumerable.
  __ push(r3);  // Current entry.
  __ CallRuntime(Runtime::kForInFilter);
  PrepareForBailoutForId(stmt->FilterId(), TOS_REG);
  __ mov(r3, Operand(r0));
  __ LoadRoot(ip, Heap::kUndefinedValueRootIndex);
  __ cmp(r0, ip);
  __ b(eq, loop_statement.continue_label());

  // Update the 'each' property or variable from the possibly filtered
  // entry in register r3.
  __ bind(&update_each);
  __ mov(result_register(), r3);
  // Perform the assignment as if via '='.
  { EffectContext context(this);
    EmitAssignment(stmt->each(), stmt->EachFeedbackSlot());
    PrepareForBailoutForId(stmt->AssignmentId(), NO_REGISTERS);
  }

  // Both Crankshaft and Turbofan expect BodyId to be right before stmt->body().
  PrepareForBailoutForId(stmt->BodyId(), NO_REGISTERS);
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
    __ mov(r2, Operand(info));
    __ CallStub(&stub);
  } else {
    __ Push(info);
    __ CallRuntime(pretenure ? Runtime::kNewClosure_Tenured
                             : Runtime::kNewClosure);
  }
  context()->Plug(r0);
}


void FullCodeGenerator::EmitSetHomeObject(Expression* initializer, int offset,
                                          FeedbackVectorSlot slot) {
  DCHECK(NeedsHomeObject(initializer));
  __ ldr(StoreDescriptor::ReceiverRegister(), MemOperand(sp));
  __ mov(StoreDescriptor::NameRegister(),
         Operand(isolate()->factory()->home_object_symbol()));
  __ ldr(StoreDescriptor::ValueRegister(),
         MemOperand(sp, offset * kPointerSize));
  EmitLoadStoreICSlot(slot);
  CallStoreIC();
}


void FullCodeGenerator::EmitSetHomeObjectAccumulator(Expression* initializer,
                                                     int offset,
                                                     FeedbackVectorSlot slot) {
  DCHECK(NeedsHomeObject(initializer));
  __ Move(StoreDescriptor::ReceiverRegister(), r0);
  __ mov(StoreDescriptor::NameRegister(),
         Operand(isolate()->factory()->home_object_symbol()));
  __ ldr(StoreDescriptor::ValueRegister(),
         MemOperand(sp, offset * kPointerSize));
  EmitLoadStoreICSlot(slot);
  CallStoreIC();
}


void FullCodeGenerator::EmitLoadGlobalCheckExtensions(VariableProxy* proxy,
                                                      TypeofMode typeof_mode,
                                                      Label* slow) {
  Register current = cp;
  Register next = r1;
  Register temp = r2;

  Scope* s = scope();
  while (s != NULL) {
    if (s->num_heap_slots() > 0) {
      if (s->calls_sloppy_eval()) {
        // Check that extension is "the hole".
        __ ldr(temp, ContextMemOperand(current, Context::EXTENSION_INDEX));
        __ JumpIfNotRoot(temp, Heap::kTheHoleValueRootIndex, slow);
      }
      // Load next context in chain.
      __ ldr(next, ContextMemOperand(current, Context::PREVIOUS_INDEX));
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
    // Check that extension is "the hole".
    __ ldr(temp, ContextMemOperand(next, Context::EXTENSION_INDEX));
    __ JumpIfNotRoot(temp, Heap::kTheHoleValueRootIndex, slow);
    // Load next context in chain.
    __ ldr(next, ContextMemOperand(next, Context::PREVIOUS_INDEX));
    __ b(&loop);
    __ bind(&fast);
  }

  // All extension objects were empty and it is safe to use a normal global
  // load machinery.
  EmitGlobalVariableLoad(proxy, typeof_mode);
}


MemOperand FullCodeGenerator::ContextSlotOperandCheckExtensions(Variable* var,
                                                                Label* slow) {
  DCHECK(var->IsContextSlot());
  Register context = cp;
  Register next = r3;
  Register temp = r4;

  for (Scope* s = scope(); s != var->scope(); s = s->outer_scope()) {
    if (s->num_heap_slots() > 0) {
      if (s->calls_sloppy_eval()) {
        // Check that extension is "the hole".
        __ ldr(temp, ContextMemOperand(context, Context::EXTENSION_INDEX));
        __ JumpIfNotRoot(temp, Heap::kTheHoleValueRootIndex, slow);
      }
      __ ldr(next, ContextMemOperand(context, Context::PREVIOUS_INDEX));
      // Walk the rest of the chain without clobbering cp.
      context = next;
    }
  }
  // Check that last extension is "the hole".
  __ ldr(temp, ContextMemOperand(context, Context::EXTENSION_INDEX));
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
  __ LoadGlobalObject(LoadDescriptor::ReceiverRegister());
  __ mov(LoadDescriptor::NameRegister(), Operand(var->name()));
  __ mov(LoadDescriptor::SlotRegister(),
         Operand(SmiFromSlot(proxy->VariableFeedbackSlot())));
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
      Comment cmnt(masm_, "[ Global variable");
      EmitGlobalVariableLoad(proxy, typeof_mode);
      context()->Plug(r0);
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
        GetVar(r0, var);
        __ CompareRoot(r0, Heap::kTheHoleValueRootIndex);
        if (var->mode() == LET || var->mode() == CONST) {
          // Throw a reference error when using an uninitialized let/const
          // binding in harmony mode.
          Label done;
          __ b(ne, &done);
          __ mov(r0, Operand(var->name()));
          __ push(r0);
          __ CallRuntime(Runtime::kThrowReferenceError);
          __ bind(&done);
        } else {
          // Uninitialized legacy const bindings are unholed.
          DCHECK(var->mode() == CONST_LEGACY);
          __ LoadRoot(r0, Heap::kUndefinedValueRootIndex, eq);
        }
        context()->Plug(r0);
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
      __ mov(r1, Operand(var->name()));
      __ Push(cp, r1);  // Context and name.
      Runtime::FunctionId function_id =
          typeof_mode == NOT_INSIDE_TYPEOF
              ? Runtime::kLoadLookupSlot
              : Runtime::kLoadLookupSlotNoReferenceError;
      __ CallRuntime(function_id);
      __ bind(&done);
      context()->Plug(r0);
    }
  }
}


void FullCodeGenerator::VisitRegExpLiteral(RegExpLiteral* expr) {
  Comment cmnt(masm_, "[ RegExpLiteral");
  __ ldr(r3, MemOperand(fp, JavaScriptFrameConstants::kFunctionOffset));
  __ mov(r2, Operand(Smi::FromInt(expr->literal_index())));
  __ mov(r1, Operand(expr->pattern()));
  __ mov(r0, Operand(Smi::FromInt(expr->flags())));
  FastCloneRegExpStub stub(isolate());
  __ CallStub(&stub);
  context()->Plug(r0);
}


void FullCodeGenerator::EmitAccessor(ObjectLiteralProperty* property) {
  Expression* expression = (property == NULL) ? NULL : property->value();
  if (expression == NULL) {
    __ LoadRoot(r1, Heap::kNullValueRootIndex);
    __ push(r1);
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
  __ ldr(r3, MemOperand(fp, JavaScriptFrameConstants::kFunctionOffset));
  __ mov(r2, Operand(Smi::FromInt(expr->literal_index())));
  __ mov(r1, Operand(constant_properties));
  int flags = expr->ComputeFlags();
  __ mov(r0, Operand(Smi::FromInt(flags)));
  if (MustCreateObjectLiteralWithRuntime(expr)) {
    __ Push(r3, r2, r1, r0);
    __ CallRuntime(Runtime::kCreateObjectLiteral);
  } else {
    FastCloneShallowObjectStub stub(isolate(), expr->properties_count());
    __ CallStub(&stub);
  }
  PrepareForBailoutForId(expr->CreateLiteralId(), TOS_REG);

  // If result_saved is true the result is on top of the stack.  If
  // result_saved is false the result is in r0.
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
      __ push(r0);  // Save result on stack
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
            DCHECK(StoreDescriptor::ValueRegister().is(r0));
            __ mov(StoreDescriptor::NameRegister(), Operand(key->value()));
            __ ldr(StoreDescriptor::ReceiverRegister(), MemOperand(sp));
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
        // Duplicate receiver on stack.
        __ ldr(r0, MemOperand(sp));
        __ push(r0);
        VisitForStackValue(key);
        VisitForStackValue(value);
        if (property->emit_store()) {
          if (NeedsHomeObject(value)) {
            EmitSetHomeObject(value, 2, property->GetSlot());
          }
          __ mov(r0, Operand(Smi::FromInt(SLOPPY)));  // PropertyAttributes
          __ push(r0);
          __ CallRuntime(Runtime::kSetProperty);
        } else {
          __ Drop(3);
        }
        break;
      case ObjectLiteral::Property::PROTOTYPE:
        // Duplicate receiver on stack.
        __ ldr(r0, MemOperand(sp));
        __ push(r0);
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
    __ ldr(r0, MemOperand(sp));  // Duplicate receiver.
    __ push(r0);
    VisitForStackValue(it->first);
    EmitAccessor(it->second->getter);
    EmitAccessor(it->second->setter);
    __ mov(r0, Operand(Smi::FromInt(NONE)));
    __ push(r0);
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
      __ push(r0);  // Save result on the stack
      result_saved = true;
    }

    __ ldr(r0, MemOperand(sp));  // Duplicate receiver.
    __ push(r0);

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
            __ mov(r0, Operand(Smi::FromInt(NONE)));
            __ push(r0);
            __ CallRuntime(Runtime::kDefineDataPropertyUnchecked);
          } else {
            __ Drop(3);
          }
          break;

        case ObjectLiteral::Property::PROTOTYPE:
          UNREACHABLE();
          break;

        case ObjectLiteral::Property::GETTER:
          __ mov(r0, Operand(Smi::FromInt(NONE)));
          __ push(r0);
          __ CallRuntime(Runtime::kDefineGetterPropertyUnchecked);
          break;

        case ObjectLiteral::Property::SETTER:
          __ mov(r0, Operand(Smi::FromInt(NONE)));
          __ push(r0);
          __ CallRuntime(Runtime::kDefineSetterPropertyUnchecked);
          break;
      }
    }
  }

  if (expr->has_function()) {
    DCHECK(result_saved);
    __ ldr(r0, MemOperand(sp));
    __ push(r0);
    __ CallRuntime(Runtime::kToFastProperties);
  }

  if (result_saved) {
    context()->PlugTOS();
  } else {
    context()->Plug(r0);
  }
}


void FullCodeGenerator::VisitArrayLiteral(ArrayLiteral* expr) {
  Comment cmnt(masm_, "[ ArrayLiteral");

  Handle<FixedArray> constant_elements = expr->constant_elements();
  bool has_fast_elements =
      IsFastObjectElementsKind(expr->constant_elements_kind());
  Handle<FixedArrayBase> constant_elements_values(
      FixedArrayBase::cast(constant_elements->get(1)));

  AllocationSiteMode allocation_site_mode = TRACK_ALLOCATION_SITE;
  if (has_fast_elements && !FLAG_allocation_site_pretenuring) {
    // If the only customer of allocation sites is transitioning, then
    // we can turn it off if we don't have anywhere else to transition to.
    allocation_site_mode = DONT_TRACK_ALLOCATION_SITE;
  }

  __ ldr(r3, MemOperand(fp, JavaScriptFrameConstants::kFunctionOffset));
  __ mov(r2, Operand(Smi::FromInt(expr->literal_index())));
  __ mov(r1, Operand(constant_elements));
  if (MustCreateArrayLiteralWithRuntime(expr)) {
    __ mov(r0, Operand(Smi::FromInt(expr->ComputeFlags())));
    __ Push(r3, r2, r1, r0);
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
      __ push(r0);
      result_saved = true;
    }
    VisitForAccumulatorValue(subexpr);

    __ mov(StoreDescriptor::NameRegister(), Operand(Smi::FromInt(array_index)));
    __ ldr(StoreDescriptor::ReceiverRegister(), MemOperand(sp, 0));
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
    __ Pop(r0);
    result_saved = false;
  }
  for (; array_index < length; array_index++) {
    Expression* subexpr = subexprs->at(array_index);

    __ Push(r0);
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
    context()->Plug(r0);
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
        __ ldr(LoadDescriptor::ReceiverRegister(), MemOperand(sp, 0));
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
        const Register scratch = r1;
        __ ldr(scratch, MemOperand(sp, kPointerSize));
        __ Push(scratch);
        __ Push(result_register());
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
        const Register scratch = r1;
        __ ldr(scratch, MemOperand(sp, 2 * kPointerSize));
        __ Push(scratch);
        __ ldr(scratch, MemOperand(sp, 2 * kPointerSize));
        __ Push(scratch);
        __ Push(result_register());
      }
      break;
    case KEYED_PROPERTY:
      if (expr->is_compound()) {
        VisitForStackValue(property->obj());
        VisitForStackValue(property->key());
        __ ldr(LoadDescriptor::ReceiverRegister(),
               MemOperand(sp, 1 * kPointerSize));
        __ ldr(LoadDescriptor::NameRegister(), MemOperand(sp, 0));
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
    __ push(r0);  // Left operand goes on the stack.
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
      context()->Plug(r0);
      break;
    case NAMED_PROPERTY:
      EmitNamedPropertyAssignment(expr);
      break;
    case NAMED_SUPER_PROPERTY:
      EmitNamedSuperPropertyStore(property);
      context()->Plug(r0);
      break;
    case KEYED_SUPER_PROPERTY:
      EmitKeyedSuperPropertyStore(property);
      context()->Plug(r0);
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
      __ CallRuntime(Runtime::kSuspendJSGeneratorObject, 1);
      __ ldr(cp, MemOperand(fp, StandardFrameConstants::kContextOffset));
      __ bind(&post_runtime);
      __ pop(result_register());
      EmitReturnSequence();

      __ bind(&resume);
      context()->Plug(result_register());
      break;
    }

    case Yield::kFinal: {
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
      __ LoadRoot(r0, Heap::kUndefinedValueRootIndex);
      __ b(&l_next);

      // catch (e) { receiver = iter; f = 'throw'; arg = e; goto l_call; }
      __ bind(&l_catch);
      __ LoadRoot(load_name, Heap::kthrow_stringRootIndex);  // "throw"
      __ ldr(r3, MemOperand(sp, 1 * kPointerSize));          // iter
      __ Push(load_name, r3, r0);                       // "throw", iter, except
      __ jmp(&l_call);

      // try { received = %yield result }
      // Shuffle the received result above a try handler and yield it without
      // re-boxing.
      __ bind(&l_try);
      __ pop(r0);                                        // result
      int handler_index = NewHandlerTableEntry();
      EnterTryBlock(handler_index, &l_catch);
      const int try_block_size = TryCatch::kElementCount * kPointerSize;
      __ push(r0);                                       // result

      __ jmp(&l_suspend);
      __ bind(&l_continuation);
      __ RecordGeneratorContinuation();
      __ jmp(&l_resume);

      __ bind(&l_suspend);
      const int generator_object_depth = kPointerSize + try_block_size;
      __ ldr(r0, MemOperand(sp, generator_object_depth));
      __ push(r0);                                       // g
      __ Push(Smi::FromInt(handler_index));              // handler-index
      DCHECK(l_continuation.pos() > 0 && Smi::IsValid(l_continuation.pos()));
      __ mov(r1, Operand(Smi::FromInt(l_continuation.pos())));
      __ str(r1, FieldMemOperand(r0, JSGeneratorObject::kContinuationOffset));
      __ str(cp, FieldMemOperand(r0, JSGeneratorObject::kContextOffset));
      __ mov(r1, cp);
      __ RecordWriteField(r0, JSGeneratorObject::kContextOffset, r1, r2,
                          kLRHasBeenSaved, kDontSaveFPRegs);
      __ CallRuntime(Runtime::kSuspendJSGeneratorObject, 2);
      __ ldr(cp, MemOperand(fp, StandardFrameConstants::kContextOffset));
      __ pop(r0);                                      // result
      EmitReturnSequence();
      __ bind(&l_resume);                              // received in r0
      ExitTryBlock(handler_index);

      // receiver = iter; f = 'next'; arg = received;
      __ bind(&l_next);

      __ LoadRoot(load_name, Heap::knext_stringRootIndex);  // "next"
      __ ldr(r3, MemOperand(sp, 1 * kPointerSize));         // iter
      __ Push(load_name, r3, r0);                      // "next", iter, received

      // result = receiver[f](arg);
      __ bind(&l_call);
      __ ldr(load_receiver, MemOperand(sp, kPointerSize));
      __ ldr(load_name, MemOperand(sp, 2 * kPointerSize));
      __ mov(LoadDescriptor::SlotRegister(),
             Operand(SmiFromSlot(expr->KeyedLoadFeedbackSlot())));
      Handle<Code> ic = CodeFactory::KeyedLoadIC(isolate(), SLOPPY).code();
      CallIC(ic, TypeFeedbackId::None());
      __ mov(r1, r0);
      __ str(r1, MemOperand(sp, 2 * kPointerSize));
      SetCallPosition(expr);
      __ mov(r0, Operand(1));
      __ Call(
          isolate()->builtins()->Call(ConvertReceiverMode::kNotNullOrUndefined),
          RelocInfo::CODE_TARGET);

      __ ldr(cp, MemOperand(fp, StandardFrameConstants::kContextOffset));
      __ Drop(1);  // The function is still on the stack; drop it.

      // if (!result.done) goto l_try;
      __ bind(&l_loop);
      __ Move(load_receiver, r0);

      __ push(load_receiver);                               // save result
      __ LoadRoot(load_name, Heap::kdone_stringRootIndex);  // "done"
      __ mov(LoadDescriptor::SlotRegister(),
             Operand(SmiFromSlot(expr->DoneFeedbackSlot())));
      CallLoadIC(NOT_INSIDE_TYPEOF);  // r0=result.done
      Handle<Code> bool_ic = ToBooleanStub::GetUninitialized(isolate());
      CallIC(bool_ic);
      __ CompareRoot(result_register(), Heap::kTrueValueRootIndex);
      __ b(ne, &l_try);

      // result.value
      __ pop(load_receiver);                                 // result
      __ LoadRoot(load_name, Heap::kvalue_stringRootIndex);  // "value"
      __ mov(LoadDescriptor::SlotRegister(),
             Operand(SmiFromSlot(expr->ValueFeedbackSlot())));
      CallLoadIC(NOT_INSIDE_TYPEOF);                         // r0=result.value
      context()->DropAndPlug(2, r0);                         // drop iter and g
      break;
    }
  }
}


void FullCodeGenerator::EmitGeneratorResume(Expression *generator,
    Expression *value,
    JSGeneratorObject::ResumeMode resume_mode) {
  // The value stays in r0, and is ultimately read by the resumed generator, as
  // if CallRuntime(Runtime::kSuspendJSGeneratorObject) returned it. Or it
  // is read to throw the value when the resumed generator is already closed.
  // r1 will hold the generator object until the activation has been resumed.
  VisitForStackValue(generator);
  VisitForAccumulatorValue(value);
  __ pop(r1);

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
  Label resume_frame, done;
  __ bind(&push_frame);
  __ bl(&resume_frame);
  __ jmp(&done);
  __ bind(&resume_frame);
  // lr = return address.
  // fp = caller's frame pointer.
  // pp = caller's constant pool (if FLAG_enable_embedded_constant_pool),
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
      if (FLAG_enable_embedded_constant_pool) {
        // Load the new code object's constant pool pointer.
        __ LoadConstantPoolPointerRegisterFromCodeTargetAddress(r3);
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
  DCHECK(!result_register().is(r1));
  __ Push(r1, result_register());
  __ Push(Smi::FromInt(resume_mode));
  __ CallRuntime(Runtime::kResumeJSGeneratorObject);
  // Not reached: the runtime call returns elsewhere.
  __ stop("not-reached");

  __ bind(&done);
  context()->Plug(result_register());
}


void FullCodeGenerator::EmitCreateIteratorResult(bool done) {
  Label allocate, done_allocate;

  __ Allocate(JSIteratorResult::kSize, r0, r2, r3, &allocate, TAG_OBJECT);
  __ b(&done_allocate);

  __ bind(&allocate);
  __ Push(Smi::FromInt(JSIteratorResult::kSize));
  __ CallRuntime(Runtime::kAllocateInNewSpace);

  __ bind(&done_allocate);
  __ LoadNativeContextSlot(Context::ITERATOR_RESULT_MAP_INDEX, r1);
  __ pop(r2);
  __ LoadRoot(r3,
              done ? Heap::kTrueValueRootIndex : Heap::kFalseValueRootIndex);
  __ LoadRoot(r4, Heap::kEmptyFixedArrayRootIndex);
  __ str(r1, FieldMemOperand(r0, HeapObject::kMapOffset));
  __ str(r4, FieldMemOperand(r0, JSObject::kPropertiesOffset));
  __ str(r4, FieldMemOperand(r0, JSObject::kElementsOffset));
  __ str(r2, FieldMemOperand(r0, JSIteratorResult::kValueOffset));
  __ str(r3, FieldMemOperand(r0, JSIteratorResult::kDoneOffset));
}


void FullCodeGenerator::EmitNamedPropertyLoad(Property* prop) {
  SetExpressionPosition(prop);
  Literal* key = prop->key()->AsLiteral();
  DCHECK(!prop->IsSuperAccess());

  __ mov(LoadDescriptor::NameRegister(), Operand(key->value()));
  __ mov(LoadDescriptor::SlotRegister(),
         Operand(SmiFromSlot(prop->PropertyFeedbackSlot())));
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
  Handle<Code> ic = CodeFactory::KeyedLoadIC(isolate(), language_mode()).code();
  __ mov(LoadDescriptor::SlotRegister(),
         Operand(SmiFromSlot(prop->PropertyFeedbackSlot())));
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
  Handle<Code> code =
      CodeFactory::BinaryOpIC(isolate(), op, strength(language_mode())).code();
  CallIC(code, expr->BinaryOperationFeedbackId());
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


void FullCodeGenerator::EmitClassDefineProperties(ClassLiteral* lit) {
  // Constructor is in r0.
  DCHECK(lit != NULL);
  __ push(r0);

  // No access check is needed here since the constructor is created by the
  // class literal.
  Register scratch = r1;
  __ ldr(scratch,
         FieldMemOperand(r0, JSFunction::kPrototypeOrInitialMapOffset));
  __ push(scratch);

  for (int i = 0; i < lit->properties()->length(); i++) {
    ObjectLiteral::Property* property = lit->properties()->at(i);
    Expression* value = property->value();

    if (property->is_static()) {
      __ ldr(scratch, MemOperand(sp, kPointerSize));  // constructor
    } else {
      __ ldr(scratch, MemOperand(sp, 0));  // prototype
    }
    __ push(scratch);
    EmitPropertyKey(property, lit->GetIdForProperty(i));

    // The static prototype property is read only. We handle the non computed
    // property name case in the parser. Since this is the only case where we
    // need to check for an own read only property we special case this so we do
    // not need to do this for every property.
    if (property->is_static() && property->is_computed_name()) {
      __ CallRuntime(Runtime::kThrowIfStaticPrototype);
      __ push(r0);
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
        __ mov(r0, Operand(Smi::FromInt(DONT_ENUM)));
        __ push(r0);
        __ CallRuntime(Runtime::kDefineGetterPropertyUnchecked);
        break;

      case ObjectLiteral::Property::SETTER:
        __ mov(r0, Operand(Smi::FromInt(DONT_ENUM)));
        __ push(r0);
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


void FullCodeGenerator::EmitBinaryOp(BinaryOperation* expr, Token::Value op) {
  __ pop(r1);
  Handle<Code> code =
      CodeFactory::BinaryOpIC(isolate(), op, strength(language_mode())).code();
  JumpPatchSite patch_site(masm_);    // unbound, signals no inlined smi code.
  CallIC(code, expr->BinaryOperationFeedbackId());
  patch_site.EmitPatchInfo();
  context()->Plug(r0);
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
      __ push(r0);  // Preserve value.
      VisitForAccumulatorValue(prop->obj());
      __ Move(StoreDescriptor::ReceiverRegister(), r0);
      __ pop(StoreDescriptor::ValueRegister());  // Restore value.
      __ mov(StoreDescriptor::NameRegister(),
             Operand(prop->key()->AsLiteral()->value()));
      EmitLoadStoreICSlot(slot);
      CallStoreIC();
      break;
    }
    case NAMED_SUPER_PROPERTY: {
      __ Push(r0);
      VisitForStackValue(prop->obj()->AsSuperPropertyReference()->this_var());
      VisitForAccumulatorValue(
          prop->obj()->AsSuperPropertyReference()->home_object());
      // stack: value, this; r0: home_object
      Register scratch = r2;
      Register scratch2 = r3;
      __ mov(scratch, result_register());              // home_object
      __ ldr(r0, MemOperand(sp, kPointerSize));        // value
      __ ldr(scratch2, MemOperand(sp, 0));             // this
      __ str(scratch2, MemOperand(sp, kPointerSize));  // this
      __ str(scratch, MemOperand(sp, 0));              // home_object
      // stack: this, home_object; r0: value
      EmitNamedSuperPropertyStore(prop);
      break;
    }
    case KEYED_SUPER_PROPERTY: {
      __ Push(r0);
      VisitForStackValue(prop->obj()->AsSuperPropertyReference()->this_var());
      VisitForStackValue(
          prop->obj()->AsSuperPropertyReference()->home_object());
      VisitForAccumulatorValue(prop->key());
      Register scratch = r2;
      Register scratch2 = r3;
      __ ldr(scratch2, MemOperand(sp, 2 * kPointerSize));  // value
      // stack: value, this, home_object; r0: key, r3: value
      __ ldr(scratch, MemOperand(sp, kPointerSize));  // this
      __ str(scratch, MemOperand(sp, 2 * kPointerSize));
      __ ldr(scratch, MemOperand(sp, 0));  // home_object
      __ str(scratch, MemOperand(sp, kPointerSize));
      __ str(r0, MemOperand(sp, 0));
      __ Move(r0, scratch2);
      // stack: this, home_object, key; r0: value.
      EmitKeyedSuperPropertyStore(prop);
      break;
    }
    case KEYED_PROPERTY: {
      __ push(r0);  // Preserve value.
      VisitForStackValue(prop->obj());
      VisitForAccumulatorValue(prop->key());
      __ Move(StoreDescriptor::NameRegister(), r0);
      __ Pop(StoreDescriptor::ValueRegister(),
             StoreDescriptor::ReceiverRegister());
      EmitLoadStoreICSlot(slot);
      Handle<Code> ic =
          CodeFactory::KeyedStoreIC(isolate(), language_mode()).code();
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


void FullCodeGenerator::EmitVariableAssignment(Variable* var, Token::Value op,
                                               FeedbackVectorSlot slot) {
  if (var->IsUnallocated()) {
    // Global var, const, or let.
    __ mov(StoreDescriptor::NameRegister(), Operand(var->name()));
    __ LoadGlobalObject(StoreDescriptor::ReceiverRegister());
    EmitLoadStoreICSlot(slot);
    CallStoreIC();

  } else if (var->mode() == LET && op != Token::INIT) {
    // Non-initializing assignment to let variable needs a write barrier.
    DCHECK(!var->IsLookupSlot());
    DCHECK(var->IsStackAllocated() || var->IsContextSlot());
    Label assign;
    MemOperand location = VarOperand(var, r1);
    __ ldr(r3, location);
    __ CompareRoot(r3, Heap::kTheHoleValueRootIndex);
    __ b(ne, &assign);
    __ mov(r3, Operand(var->name()));
    __ push(r3);
    __ CallRuntime(Runtime::kThrowReferenceError);
    // Perform the assignment.
    __ bind(&assign);
    EmitStoreToStackLocalOrContextSlot(var, location);

  } else if (var->mode() == CONST && op != Token::INIT) {
    // Assignment to const variable needs a write barrier.
    DCHECK(!var->IsLookupSlot());
    DCHECK(var->IsStackAllocated() || var->IsContextSlot());
    Label const_error;
    MemOperand location = VarOperand(var, r1);
    __ ldr(r3, location);
    __ CompareRoot(r3, Heap::kTheHoleValueRootIndex);
    __ b(ne, &const_error);
    __ mov(r3, Operand(var->name()));
    __ push(r3);
    __ CallRuntime(Runtime::kThrowReferenceError);
    __ bind(&const_error);
    __ CallRuntime(Runtime::kThrowConstAssignError);

  } else if (var->is_this() && var->mode() == CONST && op == Token::INIT) {
    // Initializing assignment to const {this} needs a write barrier.
    DCHECK(var->IsStackAllocated() || var->IsContextSlot());
    Label uninitialized_this;
    MemOperand location = VarOperand(var, r1);
    __ ldr(r3, location);
    __ CompareRoot(r3, Heap::kTheHoleValueRootIndex);
    __ b(eq, &uninitialized_this);
    __ mov(r0, Operand(var->name()));
    __ Push(r0);
    __ CallRuntime(Runtime::kThrowReferenceError);
    __ bind(&uninitialized_this);
    EmitStoreToStackLocalOrContextSlot(var, location);

  } else if (!var->is_const_mode() ||
             (var->mode() == CONST && op == Token::INIT)) {
    if (var->IsLookupSlot()) {
      // Assignment to var.
      __ push(r0);  // Value.
      __ mov(r1, Operand(var->name()));
      __ mov(r0, Operand(Smi::FromInt(language_mode())));
      __ Push(cp, r1, r0);  // Context, name, language mode.
      __ CallRuntime(Runtime::kStoreLookupSlot);
    } else {
      // Assignment to var or initializing assignment to let/const in harmony
      // mode.
      DCHECK((var->IsStackAllocated() || var->IsContextSlot()));
      MemOperand location = VarOperand(var, r1);
      if (generate_debug_code_ && var->mode() == LET && op == Token::INIT) {
        // Check for an uninitialized let binding.
        __ ldr(r2, location);
        __ CompareRoot(r2, Heap::kTheHoleValueRootIndex);
        __ Check(eq, kLetBindingReInitialization);
      }
      EmitStoreToStackLocalOrContextSlot(var, location);
    }

  } else if (var->mode() == CONST_LEGACY && op == Token::INIT) {
    // Const initializers need a write barrier.
    DCHECK(!var->IsParameter());  // No const parameters.
    if (var->IsLookupSlot()) {
      __ push(r0);
      __ mov(r0, Operand(var->name()));
      __ Push(cp, r0);  // Context and name.
      __ CallRuntime(Runtime::kInitializeLegacyConstLookupSlot);
    } else {
      DCHECK(var->IsStackAllocated() || var->IsContextSlot());
      Label skip;
      MemOperand location = VarOperand(var, r1);
      __ ldr(r2, location);
      __ CompareRoot(r2, Heap::kTheHoleValueRootIndex);
      __ b(ne, &skip);
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
  Property* prop = expr->target()->AsProperty();
  DCHECK(prop != NULL);
  DCHECK(prop->key()->IsLiteral());

  __ mov(StoreDescriptor::NameRegister(),
         Operand(prop->key()->AsLiteral()->value()));
  __ pop(StoreDescriptor::ReceiverRegister());
  EmitLoadStoreICSlot(expr->AssignmentSlot());
  CallStoreIC();

  PrepareForBailoutForId(expr->AssignmentId(), TOS_REG);
  context()->Plug(r0);
}


void FullCodeGenerator::EmitNamedSuperPropertyStore(Property* prop) {
  // Assignment to named property of super.
  // r0 : value
  // stack : receiver ('this'), home_object
  DCHECK(prop != NULL);
  Literal* key = prop->key()->AsLiteral();
  DCHECK(key != NULL);

  __ Push(key->value());
  __ Push(r0);
  __ CallRuntime((is_strict(language_mode()) ? Runtime::kStoreToSuper_Strict
                                             : Runtime::kStoreToSuper_Sloppy));
}


void FullCodeGenerator::EmitKeyedSuperPropertyStore(Property* prop) {
  // Assignment to named property of super.
  // r0 : value
  // stack : receiver ('this'), home_object, key
  DCHECK(prop != NULL);

  __ Push(r0);
  __ CallRuntime((is_strict(language_mode())
                      ? Runtime::kStoreKeyedToSuper_Strict
                      : Runtime::kStoreKeyedToSuper_Sloppy));
}


void FullCodeGenerator::EmitKeyedPropertyAssignment(Assignment* expr) {
  // Assignment to a property, using a keyed store IC.
  __ Pop(StoreDescriptor::ReceiverRegister(), StoreDescriptor::NameRegister());
  DCHECK(StoreDescriptor::ValueRegister().is(r0));

  Handle<Code> ic =
      CodeFactory::KeyedStoreIC(isolate(), language_mode()).code();
  EmitLoadStoreICSlot(expr->AssignmentSlot());
  CallIC(ic);

  PrepareForBailoutForId(expr->AssignmentId(), TOS_REG);
  context()->Plug(r0);
}


void FullCodeGenerator::VisitProperty(Property* expr) {
  Comment cmnt(masm_, "[ Property");
  SetExpressionPosition(expr);

  Expression* key = expr->key();

  if (key->IsPropertyName()) {
    if (!expr->IsSuperAccess()) {
      VisitForAccumulatorValue(expr->obj());
      __ Move(LoadDescriptor::ReceiverRegister(), r0);
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
      __ Move(LoadDescriptor::NameRegister(), r0);
      __ pop(LoadDescriptor::ReceiverRegister());
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
  context()->Plug(r0);
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
    __ LoadRoot(ip, Heap::kUndefinedValueRootIndex);
    __ push(ip);
    convert_mode = ConvertReceiverMode::kNullOrUndefined;
  } else {
    // Load the function from the receiver.
    DCHECK(callee->IsProperty());
    DCHECK(!callee->AsProperty()->IsSuperAccess());
    __ ldr(LoadDescriptor::ReceiverRegister(), MemOperand(sp, 0));
    EmitNamedPropertyLoad(callee->AsProperty());
    PrepareForBailoutForId(callee->AsProperty()->LoadId(), TOS_REG);
    // Push the target function under the receiver.
    __ ldr(ip, MemOperand(sp, 0));
    __ push(ip);
    __ str(r0, MemOperand(sp, kPointerSize));
    convert_mode = ConvertReceiverMode::kNotNullOrUndefined;
  }

  EmitCall(expr, convert_mode);
}


void FullCodeGenerator::EmitSuperCallWithLoadIC(Call* expr) {
  Expression* callee = expr->expression();
  DCHECK(callee->IsProperty());
  Property* prop = callee->AsProperty();
  DCHECK(prop->IsSuperAccess());
  SetExpressionPosition(prop);

  Literal* key = prop->key()->AsLiteral();
  DCHECK(!key->value()->IsSmi());
  // Load the function from the receiver.
  const Register scratch = r1;
  SuperPropertyReference* super_ref = prop->obj()->AsSuperPropertyReference();
  VisitForStackValue(super_ref->home_object());
  VisitForAccumulatorValue(super_ref->this_var());
  __ Push(r0);
  __ Push(r0);
  __ ldr(scratch, MemOperand(sp, kPointerSize * 2));
  __ Push(scratch);
  __ Push(key->value());
  __ Push(Smi::FromInt(language_mode()));

  // Stack here:
  //  - home_object
  //  - this (receiver)
  //  - this (receiver) <-- LoadFromSuper will pop here and below.
  //  - home_object
  //  - key
  //  - language_mode
  __ CallRuntime(Runtime::kLoadFromSuper);

  // Replace home_object with target function.
  __ str(r0, MemOperand(sp, kPointerSize));

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
  __ ldr(LoadDescriptor::ReceiverRegister(), MemOperand(sp, 0));
  __ Move(LoadDescriptor::NameRegister(), r0);
  EmitKeyedPropertyLoad(callee->AsProperty());
  PrepareForBailoutForId(callee->AsProperty()->LoadId(), TOS_REG);

  // Push the target function under the receiver.
  __ ldr(ip, MemOperand(sp, 0));
  __ push(ip);
  __ str(r0, MemOperand(sp, kPointerSize));

  EmitCall(expr, ConvertReceiverMode::kNotNullOrUndefined);
}


void FullCodeGenerator::EmitKeyedSuperCallWithLoadIC(Call* expr) {
  Expression* callee = expr->expression();
  DCHECK(callee->IsProperty());
  Property* prop = callee->AsProperty();
  DCHECK(prop->IsSuperAccess());

  SetExpressionPosition(prop);
  // Load the function from the receiver.
  const Register scratch = r1;
  SuperPropertyReference* super_ref = prop->obj()->AsSuperPropertyReference();
  VisitForStackValue(super_ref->home_object());
  VisitForAccumulatorValue(super_ref->this_var());
  __ Push(r0);
  __ Push(r0);
  __ ldr(scratch, MemOperand(sp, kPointerSize * 2));
  __ Push(scratch);
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
  __ str(r0, MemOperand(sp, kPointerSize));

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
  __ mov(r3, Operand(SmiFromSlot(expr->CallFeedbackICSlot())));
  __ ldr(r1, MemOperand(sp, (arg_count + 1) * kPointerSize));
  // Don't assign a type feedback id to the IC, since type feedback is provided
  // by the vector above.
  CallIC(ic);

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
  __ ldr(r3, MemOperand(fp, JavaScriptFrameConstants::kFunctionOffset));

  // r2: language mode.
  __ mov(r2, Operand(Smi::FromInt(language_mode())));

  // r1: the start position of the scope the calls resides in.
  __ mov(r1, Operand(Smi::FromInt(scope()->start_position())));

  // Do the runtime call.
  __ Push(r4, r3, r2, r1);
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

    __ bind(&slow);
    // Call the runtime to find the function to call (returned in r0)
    // and the object holding it (returned in edx).
    DCHECK(!context_register().is(r2));
    __ mov(r2, Operand(callee->name()));
    __ Push(context_register(), r2);
    __ CallRuntime(Runtime::kLoadLookupSlot);
    __ Push(r0, r1);  // Function, receiver.
    PrepareForBailoutForId(expr->LookupId(), NO_REGISTERS);

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
  } else {
    VisitForStackValue(callee);
    // refEnv.WithBaseObject()
    __ LoadRoot(r2, Heap::kUndefinedValueRootIndex);
    __ push(r2);  // Reserved receiver slot.
  }
}


void FullCodeGenerator::EmitPossiblyEvalCall(Call* expr) {
  // In a call to eval, we first call
  // RuntimeHidden_asResolvePossiblyDirectEval to resolve the function we need
  // to call.  Then we call the resolved function using the given arguments.
  ZoneList<Expression*>* args = expr->arguments();
  int arg_count = args->length();

  PushCalleeAndWithBaseObject(expr);

  // Push the arguments.
  for (int i = 0; i < arg_count; i++) {
    VisitForStackValue(args->at(i));
  }

  // Push a copy of the function (found below the arguments) and
  // resolve eval.
  __ ldr(r1, MemOperand(sp, (arg_count + 1) * kPointerSize));
  __ push(r1);
  EmitResolvePossiblyDirectEval(arg_count);

  // Touch up the stack with the resolved function.
  __ str(r0, MemOperand(sp, (arg_count + 1) * kPointerSize));

  PrepareForBailoutForId(expr->EvalId(), NO_REGISTERS);

  // Record source position for debugger.
  SetCallPosition(expr);
  __ ldr(r1, MemOperand(sp, (arg_count + 1) * kPointerSize));
  __ mov(r0, Operand(arg_count));
  __ Call(isolate()->builtins()->Call(), RelocInfo::CODE_TARGET);
  RecordJSReturnSite(expr);
  // Restore context register.
  __ ldr(cp, MemOperand(fp, StandardFrameConstants::kContextOffset));
  context()->DropAndPlug(1, r0);
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

  // Load function and argument count into r1 and r0.
  __ mov(r0, Operand(arg_count));
  __ ldr(r1, MemOperand(sp, arg_count * kPointerSize));

  // Record call targets in unoptimized code.
  __ EmitLoadTypeFeedbackVector(r2);
  __ mov(r3, Operand(SmiFromSlot(expr->CallNewFeedbackSlot())));

  CallConstructStub stub(isolate());
  __ Call(stub.GetCode(), RelocInfo::CODE_TARGET);
  PrepareForBailoutForId(expr->ReturnId(), TOS_REG);
  // Restore context register.
  __ ldr(cp, MemOperand(fp, StandardFrameConstants::kContextOffset));
  context()->Plug(r0);
}


void FullCodeGenerator::EmitSuperConstructorCall(Call* expr) {
  SuperCallReference* super_call_ref =
      expr->expression()->AsSuperCallReference();
  DCHECK_NOT_NULL(super_call_ref);

  // Push the super constructor target on the stack (may be null,
  // but the Construct builtin can deal with that properly).
  VisitForAccumulatorValue(super_call_ref->this_function_var());
  __ AssertFunction(result_register());
  __ ldr(result_register(),
         FieldMemOperand(result_register(), HeapObject::kMapOffset));
  __ ldr(result_register(),
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

  // Load new target into r3.
  VisitForAccumulatorValue(super_call_ref->new_target_var());
  __ mov(r3, result_register());

  // Load function and argument count into r1 and r0.
  __ mov(r0, Operand(arg_count));
  __ ldr(r1, MemOperand(sp, arg_count * kPointerSize));

  __ Call(isolate()->builtins()->Construct(), RelocInfo::CODE_TARGET);

  RecordJSReturnSite(expr);

  // Restore context register.
  __ ldr(cp, MemOperand(fp, StandardFrameConstants::kContextOffset));
  context()->Plug(r0);
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
  __ SmiTst(r0);
  Split(eq, if_true, if_false, fall_through);

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

  __ JumpIfSmi(r0, if_false);
  __ CompareObjectType(r0, r1, r1, FIRST_JS_RECEIVER_TYPE);
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

  __ JumpIfSmi(r0, if_false);
  __ CompareObjectType(r0, r1, r1, SIMD128_VALUE_TYPE);
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

  __ JumpIfSmi(r0, if_false);
  __ CompareObjectType(r0, r1, r2, FIRST_FUNCTION_TYPE);
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
  DCHECK(args->length() == 1);

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

  __ JumpIfSmi(r0, if_false);
  __ CompareObjectType(r0, r1, r1, JS_TYPED_ARRAY_TYPE);
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

  __ JumpIfSmi(r0, if_false);
  __ CompareObjectType(r0, r1, r1, JS_REGEXP_TYPE);
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

  __ JumpIfSmi(r0, if_false);
  __ CompareObjectType(r0, r1, r1, JS_PROXY_TYPE);
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

  __ pop(r1);
  __ cmp(r0, r1);
  PrepareForBailoutBeforeSplit(expr, true, if_true, if_false);
  Split(eq, if_true, if_false, fall_through);

  context()->Plug(if_true, if_false);
}


void FullCodeGenerator::EmitArguments(CallRuntime* expr) {
  ZoneList<Expression*>* args = expr->arguments();
  DCHECK(args->length() == 1);

  // ArgumentsAccessStub expects the key in edx and the formal
  // parameter count in r0.
  VisitForAccumulatorValue(args->at(0));
  __ mov(r1, r0);
  __ mov(r0, Operand(Smi::FromInt(info_->scope()->num_parameters())));
  ArgumentsAccessStub stub(isolate(), ArgumentsAccessStub::READ_ELEMENT);
  __ CallStub(&stub);
  context()->Plug(r0);
}


void FullCodeGenerator::EmitArgumentsLength(CallRuntime* expr) {
  DCHECK(expr->arguments()->length() == 0);

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
  DCHECK(args->length() == 1);
  Label done, null, function, non_function_constructor;

  VisitForAccumulatorValue(args->at(0));

  // If the object is not a JSReceiver, we return null.
  __ JumpIfSmi(r0, &null);
  STATIC_ASSERT(LAST_JS_RECEIVER_TYPE == LAST_TYPE);
  __ CompareObjectType(r0, r0, r1, FIRST_JS_RECEIVER_TYPE);
  // Map is now in r0.
  __ b(lt, &null);

  // Return 'Function' for JSFunction objects.
  __ cmp(r1, Operand(JS_FUNCTION_TYPE));
  __ b(eq, &function);

  // Check if the constructor in the map is a JS function.
  Register instance_type = r2;
  __ GetMapConstructor(r0, r0, r1, instance_type);
  __ cmp(instance_type, Operand(JS_FUNCTION_TYPE));
  __ b(ne, &non_function_constructor);

  // r0 now contains the constructor function. Grab the
  // instance class name from there.
  __ ldr(r0, FieldMemOperand(r0, JSFunction::kSharedFunctionInfoOffset));
  __ ldr(r0, FieldMemOperand(r0, SharedFunctionInfo::kInstanceClassNameOffset));
  __ b(&done);

  // Functions have class 'Function'.
  __ bind(&function);
  __ LoadRoot(r0, Heap::kFunction_stringRootIndex);
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


void FullCodeGenerator::EmitValueOf(CallRuntime* expr) {
  ZoneList<Expression*>* args = expr->arguments();
  DCHECK(args->length() == 1);
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

  __ JumpIfSmi(r0, if_false);
  __ CompareObjectType(r0, r1, r1, JS_DATE_TYPE);
  PrepareForBailoutBeforeSplit(expr, true, if_true, if_false);
  Split(eq, if_true, if_false, fall_through);

  context()->Plug(if_true, if_false);
}


void FullCodeGenerator::EmitOneByteSeqStringSetChar(CallRuntime* expr) {
  ZoneList<Expression*>* args = expr->arguments();
  DCHECK_EQ(3, args->length());

  Register string = r0;
  Register index = r1;
  Register value = r2;

  VisitForStackValue(args->at(0));        // index
  VisitForStackValue(args->at(1));        // value
  VisitForAccumulatorValue(args->at(2));  // string
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
  DCHECK_EQ(3, args->length());

  Register string = r0;
  Register index = r1;
  Register value = r2;

  VisitForStackValue(args->at(0));        // index
  VisitForStackValue(args->at(1));        // value
  VisitForAccumulatorValue(args->at(2));  // string
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


void FullCodeGenerator::EmitSetValueOf(CallRuntime* expr) {
  ZoneList<Expression*>* args = expr->arguments();
  DCHECK(args->length() == 2);
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


void FullCodeGenerator::EmitToInteger(CallRuntime* expr) {
  ZoneList<Expression*>* args = expr->arguments();
  DCHECK_EQ(1, args->length());

  // Load the argument into r0 and convert it.
  VisitForAccumulatorValue(args->at(0));

  // Convert the object to an integer.
  Label done_convert;
  __ JumpIfSmi(r0, &done_convert);
  __ Push(r0);
  __ CallRuntime(Runtime::kToInteger);
  __ bind(&done_convert);
  context()->Plug(r0);
}


void FullCodeGenerator::EmitToName(CallRuntime* expr) {
  ZoneList<Expression*>* args = expr->arguments();
  DCHECK_EQ(1, args->length());

  // Load the argument into r0 and convert it.
  VisitForAccumulatorValue(args->at(0));

  Label convert, done_convert;
  __ JumpIfSmi(r0, &convert);
  STATIC_ASSERT(FIRST_NAME_TYPE == FIRST_TYPE);
  __ CompareObjectType(r0, r1, r1, LAST_NAME_TYPE);
  __ b(ls, &done_convert);
  __ bind(&convert);
  __ Push(r0);
  __ CallRuntime(Runtime::kToName);
  __ bind(&done_convert);
  context()->Plug(r0);
}


void FullCodeGenerator::EmitStringCharFromCode(CallRuntime* expr) {
  ZoneList<Expression*>* args = expr->arguments();
  DCHECK(args->length() == 1);
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
  DCHECK(args->length() == 2);
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
  generator.GenerateSlow(masm_, NOT_PART_OF_IC_HANDLER, call_helper);

  __ bind(&done);
  context()->Plug(result);
}


void FullCodeGenerator::EmitStringCharAt(CallRuntime* expr) {
  ZoneList<Expression*>* args = expr->arguments();
  DCHECK(args->length() == 2);
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
  // Move target to r1.
  int const argc = args->length() - 2;
  __ ldr(r1, MemOperand(sp, (argc + 1) * kPointerSize));
  // Call the target.
  __ mov(r0, Operand(argc));
  __ Call(isolate()->builtins()->Call(), RelocInfo::CODE_TARGET);
  // Restore context register.
  __ ldr(cp, MemOperand(fp, StandardFrameConstants::kContextOffset));
  // Discard the function left on TOS.
  context()->DropAndPlug(1, r0);
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
  DCHECK(args->length() == 1);
  VisitForAccumulatorValue(args->at(0));

  __ AssertString(r0);

  __ ldr(r0, FieldMemOperand(r0, String::kHashFieldOffset));
  __ IndexFromHash(r0, r0);

  context()->Plug(r0);
}


void FullCodeGenerator::EmitGetSuperConstructor(CallRuntime* expr) {
  ZoneList<Expression*>* args = expr->arguments();
  DCHECK_EQ(1, args->length());
  VisitForAccumulatorValue(args->at(0));
  __ AssertFunction(r0);
  __ ldr(r0, FieldMemOperand(r0, HeapObject::kMapOffset));
  __ ldr(r0, FieldMemOperand(r0, Map::kPrototypeOffset));
  context()->Plug(r0);
}


void FullCodeGenerator::EmitFastOneByteArrayJoin(CallRuntime* expr) {
  Label bailout, done, one_char_separator, long_separator, non_trivial_array,
      not_size_one_array, loop, empty_separator_loop, one_char_separator_loop,
      one_char_separator_loop_entry, long_separator_loop;
  ZoneList<Expression*>* args = expr->arguments();
  DCHECK(args->length() == 2);
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

  // Check that all array elements are sequential one-byte strings, and
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
    __ Assert(gt, kNoEmptyArraysHereInEmitFastOneByteArrayJoin);
  }
  __ bind(&loop);
  __ ldr(string, MemOperand(element, kPointerSize, PostIndex));
  __ JumpIfSmi(string, &bailout);
  __ ldr(scratch, FieldMemOperand(string, HeapObject::kMapOffset));
  __ ldrb(scratch, FieldMemOperand(scratch, Map::kInstanceTypeOffset));
  __ JumpIfInstanceTypeIsNotSequentialOneByte(scratch, scratch, &bailout);
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

  // Check that the separator is a flat one-byte string.
  __ JumpIfSmi(separator, &bailout);
  __ ldr(scratch, FieldMemOperand(separator, HeapObject::kMapOffset));
  __ ldrb(scratch, FieldMemOperand(scratch, Map::kInstanceTypeOffset));
  __ JumpIfInstanceTypeIsNotSequentialOneByte(scratch, scratch, &bailout);

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

  // Bailout for large object allocations.
  __ cmp(string_length, Operand(Page::kMaxRegularHeapObjectSize));
  __ b(gt, &bailout);

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
  __ AllocateOneByteString(result, string_length, scratch,
                           string,        // used as scratch
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
  DCHECK(result.is(r0));
  __ b(&done);

  // One-character separator case
  __ bind(&one_char_separator);
  // Replace separator with its one-byte character value.
  __ ldrb(separator, FieldMemOperand(separator, SeqOneByteString::kHeaderSize));
  // Jump into the loop after the code that copies the separator, so the first
  // element is not preceded by a separator
  __ jmp(&one_char_separator_loop_entry);

  __ bind(&one_char_separator_loop);
  // Live values in registers:
  //   result_pos: the position to which we are currently copying characters.
  //   element: Current array element.
  //   elements_end: Array end.
  //   separator: Single separator one-byte char (in lower byte).

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
  DCHECK(result.is(r0));
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
  DCHECK(result.is(r0));
  __ b(&done);

  __ bind(&bailout);
  __ LoadRoot(r0, Heap::kUndefinedValueRootIndex);
  __ bind(&done);
  context()->Plug(r0);
}


void FullCodeGenerator::EmitDebugIsActive(CallRuntime* expr) {
  DCHECK(expr->arguments()->length() == 0);
  ExternalReference debug_is_active =
      ExternalReference::debug_is_active_address(isolate());
  __ mov(ip, Operand(debug_is_active));
  __ ldrb(r0, MemOperand(ip));
  __ SmiTag(r0);
  context()->Plug(r0);
}


void FullCodeGenerator::EmitCreateIterResultObject(CallRuntime* expr) {
  ZoneList<Expression*>* args = expr->arguments();
  DCHECK_EQ(2, args->length());
  VisitForStackValue(args->at(0));
  VisitForStackValue(args->at(1));

  Label runtime, done;

  __ Allocate(JSIteratorResult::kSize, r0, r2, r3, &runtime, TAG_OBJECT);
  __ LoadNativeContextSlot(Context::ITERATOR_RESULT_MAP_INDEX, r1);
  __ pop(r3);
  __ pop(r2);
  __ LoadRoot(r4, Heap::kEmptyFixedArrayRootIndex);
  __ str(r1, FieldMemOperand(r0, HeapObject::kMapOffset));
  __ str(r4, FieldMemOperand(r0, JSObject::kPropertiesOffset));
  __ str(r4, FieldMemOperand(r0, JSObject::kElementsOffset));
  __ str(r2, FieldMemOperand(r0, JSIteratorResult::kValueOffset));
  __ str(r3, FieldMemOperand(r0, JSIteratorResult::kDoneOffset));
  STATIC_ASSERT(JSIteratorResult::kSize == 5 * kPointerSize);
  __ b(&done);

  __ bind(&runtime);
  __ CallRuntime(Runtime::kCreateIterResultObject);

  __ bind(&done);
  context()->Plug(r0);
}


void FullCodeGenerator::EmitLoadJSRuntimeFunction(CallRuntime* expr) {
  // Push undefined as the receiver.
  __ LoadRoot(r0, Heap::kUndefinedValueRootIndex);
  __ push(r0);

  __ LoadNativeContextSlot(expr->context_index(), r0);
}


void FullCodeGenerator::EmitCallJSRuntimeFunction(CallRuntime* expr) {
  ZoneList<Expression*>* args = expr->arguments();
  int arg_count = args->length();

  SetCallPosition(expr);
  __ ldr(r1, MemOperand(sp, (arg_count + 1) * kPointerSize));
  __ mov(r0, Operand(arg_count));
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
    __ ldr(ip, MemOperand(sp, 0));
    __ push(ip);
    __ str(r0, MemOperand(sp, kPointerSize));

    // Push the arguments ("left-to-right").
    for (int i = 0; i < arg_count; i++) {
      VisitForStackValue(args->at(i));
    }

    PrepareForBailoutForId(expr->CallId(), NO_REGISTERS);
    EmitCallJSRuntimeFunction(expr);

    // Restore context register.
    __ ldr(cp, MemOperand(fp, StandardFrameConstants::kContextOffset));

    context()->DropAndPlug(1, r0);

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
        context()->Plug(r0);
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
        context()->Plug(r0);
      } else if (proxy != NULL) {
        Variable* var = proxy->var();
        // Delete of an unqualified identifier is disallowed in strict mode but
        // "delete this" is allowed.
        bool is_this = var->HasThisName(isolate());
        DCHECK(is_sloppy(language_mode()) || is_this);
        if (var->IsUnallocatedOrGlobalSlot()) {
          __ LoadGlobalObject(r2);
          __ mov(r1, Operand(var->name()));
          __ Push(r2, r1);
          __ CallRuntime(Runtime::kDeleteProperty_Sloppy);
          context()->Plug(r0);
        } else if (var->IsStackAllocated() || var->IsContextSlot()) {
          // Result of deleting non-global, non-dynamic variables is false.
          // The subexpression does not have side effects.
          context()->Plug(is_this);
        } else {
          // Non-global variable.  Call the runtime to try to delete from the
          // context where the variable was introduced.
          DCHECK(!context_register().is(r2));
          __ mov(r2, Operand(var->name()));
          __ Push(context_register(), r2);
          __ CallRuntime(Runtime::kDeleteLookupSlot);
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
        DCHECK(context()->IsAccumulatorValue() || context()->IsStackValue());
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
      {
        AccumulatorValueContext context(this);
        VisitForTypeofValue(expr->expression());
      }
      __ mov(r3, r0);
      TypeofStub typeof_stub(isolate());
      __ CallStub(&typeof_stub);
      context()->Plug(r0);
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
      __ mov(ip, Operand(Smi::FromInt(0)));
      __ push(ip);
    }
    switch (assign_type) {
      case NAMED_PROPERTY: {
        // Put the object both on the stack and in the register.
        VisitForStackValue(prop->obj());
        __ ldr(LoadDescriptor::ReceiverRegister(), MemOperand(sp, 0));
        EmitNamedPropertyLoad(prop);
        break;
      }

      case NAMED_SUPER_PROPERTY: {
        VisitForStackValue(prop->obj()->AsSuperPropertyReference()->this_var());
        VisitForAccumulatorValue(
            prop->obj()->AsSuperPropertyReference()->home_object());
        __ Push(result_register());
        const Register scratch = r1;
        __ ldr(scratch, MemOperand(sp, kPointerSize));
        __ Push(scratch);
        __ Push(result_register());
        EmitNamedSuperPropertyLoad(prop);
        break;
      }

      case KEYED_SUPER_PROPERTY: {
        VisitForStackValue(prop->obj()->AsSuperPropertyReference()->this_var());
        VisitForStackValue(
            prop->obj()->AsSuperPropertyReference()->home_object());
        VisitForAccumulatorValue(prop->key());
        __ Push(result_register());
        const Register scratch = r1;
        __ ldr(scratch, MemOperand(sp, 2 * kPointerSize));
        __ Push(scratch);
        __ ldr(scratch, MemOperand(sp, 2 * kPointerSize));
        __ Push(scratch);
        __ Push(result_register());
        EmitKeyedSuperPropertyLoad(prop);
        break;
      }

      case KEYED_PROPERTY: {
        VisitForStackValue(prop->obj());
        VisitForStackValue(prop->key());
        __ ldr(LoadDescriptor::ReceiverRegister(),
               MemOperand(sp, 1 * kPointerSize));
        __ ldr(LoadDescriptor::NameRegister(), MemOperand(sp, 0));
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
          case NAMED_SUPER_PROPERTY:
            __ str(r0, MemOperand(sp, 2 * kPointerSize));
            break;
          case KEYED_PROPERTY:
            __ str(r0, MemOperand(sp, 2 * kPointerSize));
            break;
          case KEYED_SUPER_PROPERTY:
            __ str(r0, MemOperand(sp, 3 * kPointerSize));
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
          __ push(r0);
          break;
        case NAMED_PROPERTY:
          __ str(r0, MemOperand(sp, kPointerSize));
          break;
        case NAMED_SUPER_PROPERTY:
          __ str(r0, MemOperand(sp, 2 * kPointerSize));
          break;
        case KEYED_PROPERTY:
          __ str(r0, MemOperand(sp, 2 * kPointerSize));
          break;
        case KEYED_SUPER_PROPERTY:
          __ str(r0, MemOperand(sp, 3 * kPointerSize));
          break;
      }
    }
  }


  __ bind(&stub_call);
  __ mov(r1, r0);
  __ mov(r0, Operand(Smi::FromInt(count_value)));

  SetExpressionPosition(expr);

  Handle<Code> code = CodeFactory::BinaryOpIC(isolate(), Token::ADD,
                                              strength(language_mode())).code();
  CallIC(code, expr->CountBinOpFeedbackId());
  patch_site.EmitPatchInfo();
  __ bind(&done);

  if (is_strong(language_mode())) {
    PrepareForBailoutForId(expr->ToNumberId(), TOS_REG);
  }
  // Store the value returned in r0.
  switch (assign_type) {
    case VARIABLE:
      if (expr->is_postfix()) {
        { EffectContext context(this);
          EmitVariableAssignment(expr->expression()->AsVariableProxy()->var(),
                                 Token::ASSIGN, expr->CountSlot());
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
                               Token::ASSIGN, expr->CountSlot());
        PrepareForBailoutForId(expr->AssignmentId(), TOS_REG);
        context()->Plug(r0);
      }
      break;
    case NAMED_PROPERTY: {
      __ mov(StoreDescriptor::NameRegister(),
             Operand(prop->key()->AsLiteral()->value()));
      __ pop(StoreDescriptor::ReceiverRegister());
      EmitLoadStoreICSlot(expr->CountSlot());
      CallStoreIC();
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
    case NAMED_SUPER_PROPERTY: {
      EmitNamedSuperPropertyStore(prop);
      if (expr->is_postfix()) {
        if (!context()->IsEffect()) {
          context()->PlugTOS();
        }
      } else {
        context()->Plug(r0);
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
        context()->Plug(r0);
      }
      break;
    }
    case KEYED_PROPERTY: {
      __ Pop(StoreDescriptor::ReceiverRegister(),
             StoreDescriptor::NameRegister());
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
        context()->Plug(r0);
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
    __ JumpIfSmi(r0, if_true);
    __ ldr(r0, FieldMemOperand(r0, HeapObject::kMapOffset));
    __ LoadRoot(ip, Heap::kHeapNumberMapRootIndex);
    __ cmp(r0, ip);
    Split(eq, if_true, if_false, fall_through);
  } else if (String::Equals(check, factory->string_string())) {
    __ JumpIfSmi(r0, if_false);
    __ CompareObjectType(r0, r0, r1, FIRST_NONSTRING_TYPE);
    Split(lt, if_true, if_false, fall_through);
  } else if (String::Equals(check, factory->symbol_string())) {
    __ JumpIfSmi(r0, if_false);
    __ CompareObjectType(r0, r0, r1, SYMBOL_TYPE);
    Split(eq, if_true, if_false, fall_through);
  } else if (String::Equals(check, factory->boolean_string())) {
    __ CompareRoot(r0, Heap::kTrueValueRootIndex);
    __ b(eq, if_true);
    __ CompareRoot(r0, Heap::kFalseValueRootIndex);
    Split(eq, if_true, if_false, fall_through);
  } else if (String::Equals(check, factory->undefined_string())) {
    __ CompareRoot(r0, Heap::kUndefinedValueRootIndex);
    __ b(eq, if_true);
    __ JumpIfSmi(r0, if_false);
    // Check for undetectable objects => true.
    __ ldr(r0, FieldMemOperand(r0, HeapObject::kMapOffset));
    __ ldrb(r1, FieldMemOperand(r0, Map::kBitFieldOffset));
    __ tst(r1, Operand(1 << Map::kIsUndetectable));
    Split(ne, if_true, if_false, fall_through);

  } else if (String::Equals(check, factory->function_string())) {
    __ JumpIfSmi(r0, if_false);
    __ ldr(r0, FieldMemOperand(r0, HeapObject::kMapOffset));
    __ ldrb(r1, FieldMemOperand(r0, Map::kBitFieldOffset));
    __ and_(r1, r1,
            Operand((1 << Map::kIsCallable) | (1 << Map::kIsUndetectable)));
    __ cmp(r1, Operand(1 << Map::kIsCallable));
    Split(eq, if_true, if_false, fall_through);
  } else if (String::Equals(check, factory->object_string())) {
    __ JumpIfSmi(r0, if_false);
    __ CompareRoot(r0, Heap::kNullValueRootIndex);
    __ b(eq, if_true);
    STATIC_ASSERT(LAST_JS_RECEIVER_TYPE == LAST_TYPE);
    __ CompareObjectType(r0, r0, r1, FIRST_JS_RECEIVER_TYPE);
    __ b(lt, if_false);
    // Check for callable or undetectable objects => false.
    __ ldrb(r1, FieldMemOperand(r0, Map::kBitFieldOffset));
    __ tst(r1, Operand((1 << Map::kIsCallable) | (1 << Map::kIsUndetectable)));
    Split(eq, if_true, if_false, fall_through);
// clang-format off
#define SIMD128_TYPE(TYPE, Type, type, lane_count, lane_type)   \
  } else if (String::Equals(check, factory->type##_string())) { \
    __ JumpIfSmi(r0, if_false);                                 \
    __ ldr(r0, FieldMemOperand(r0, HeapObject::kMapOffset));    \
    __ CompareRoot(r0, Heap::k##Type##MapRootIndex);            \
    Split(eq, if_true, if_false, fall_through);
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
      __ CompareRoot(r0, Heap::kTrueValueRootIndex);
      Split(eq, if_true, if_false, fall_through);
      break;

    case Token::INSTANCEOF: {
      VisitForAccumulatorValue(expr->right());
      __ pop(r1);
      InstanceOfStub stub(isolate());
      __ CallStub(&stub);
      PrepareForBailoutBeforeSplit(expr, false, NULL, NULL);
      __ CompareRoot(r0, Heap::kTrueValueRootIndex);
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

      Handle<Code> ic = CodeFactory::CompareIC(
                            isolate(), op, strength(language_mode())).code();
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
    __ CompareRoot(r0, Heap::kTrueValueRootIndex);
    Split(eq, if_true, if_false, fall_through);
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
  DCHECK_EQ(POINTER_SIZE_ALIGN(frame_offset), frame_offset);
  __ str(value, MemOperand(fp, frame_offset));
}


void FullCodeGenerator::LoadContextField(Register dst, int context_index) {
  __ ldr(dst, ContextMemOperand(cp, context_index));
}


void FullCodeGenerator::PushFunctionArgumentForContextAllocation() {
  Scope* closure_scope = scope()->ClosureScope();
  if (closure_scope->is_script_scope() ||
      closure_scope->is_module_scope()) {
    // Contexts nested in the native context have a canonical empty function
    // as their closure, not the anonymous closure containing the global
    // code.
    __ LoadNativeContextSlot(Context::CLOSURE_INDEX, ip);
  } else if (closure_scope->is_eval_scope()) {
    // Contexts created by a call to eval have the same closure as the
    // context calling eval, not the anonymous closure containing the eval
    // code.  Fetch it from the context.
    __ ldr(ip, ContextMemOperand(cp, Context::CLOSURE_INDEX));
  } else {
    DCHECK(closure_scope->is_function_scope());
    __ ldr(ip, MemOperand(fp, JavaScriptFrameConstants::kFunctionOffset));
  }
  __ push(ip);
}


// ----------------------------------------------------------------------------
// Non-local control flow support.

void FullCodeGenerator::EnterFinallyBlock() {
  DCHECK(!result_register().is(r1));
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

  ClearPendingMessage();
}


void FullCodeGenerator::ExitFinallyBlock() {
  DCHECK(!result_register().is(r1));
  // Restore pending message from stack.
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


void FullCodeGenerator::ClearPendingMessage() {
  DCHECK(!result_register().is(r1));
  ExternalReference pending_message_obj =
      ExternalReference::address_of_pending_message_obj(isolate());
  __ LoadRoot(r1, Heap::kTheHoleValueRootIndex);
  __ mov(ip, Operand(pending_message_obj));
  __ str(r1, MemOperand(ip));
}


void FullCodeGenerator::EmitLoadStoreICSlot(FeedbackVectorSlot slot) {
  DCHECK(!slot.IsInvalid());
  __ mov(VectorStoreICTrampolineDescriptor::SlotRegister(),
         Operand(SmiFromSlot(slot)));
}


#undef __


static Address GetInterruptImmediateLoadAddress(Address pc) {
  Address load_address = pc - 2 * Assembler::kInstrSize;
  if (!FLAG_enable_embedded_constant_pool) {
    DCHECK(Assembler::IsLdrPcImmediateOffset(Memory::int32_at(load_address)));
  } else if (Assembler::IsLdrPpRegOffset(Memory::int32_at(load_address))) {
    // This is an extended constant pool lookup.
    if (CpuFeatures::IsSupported(ARMv7)) {
      load_address -= 2 * Assembler::kInstrSize;
      DCHECK(Assembler::IsMovW(Memory::int32_at(load_address)));
      DCHECK(Assembler::IsMovT(
          Memory::int32_at(load_address + Assembler::kInstrSize)));
    } else {
      load_address -= 4 * Assembler::kInstrSize;
      DCHECK(Assembler::IsMovImmed(Memory::int32_at(load_address)));
      DCHECK(Assembler::IsOrrImmed(
          Memory::int32_at(load_address + Assembler::kInstrSize)));
      DCHECK(Assembler::IsOrrImmed(
          Memory::int32_at(load_address + 2 * Assembler::kInstrSize)));
      DCHECK(Assembler::IsOrrImmed(
          Memory::int32_at(load_address + 3 * Assembler::kInstrSize)));
    }
  } else if (CpuFeatures::IsSupported(ARMv7) &&
             Assembler::IsMovT(Memory::int32_at(load_address))) {
    // This is a movw / movt immediate load.
    load_address -= Assembler::kInstrSize;
    DCHECK(Assembler::IsMovW(Memory::int32_at(load_address)));
  } else if (!CpuFeatures::IsSupported(ARMv7) &&
             Assembler::IsOrrImmed(Memory::int32_at(load_address))) {
    // This is a mov / orr immediate load.
    load_address -= 3 * Assembler::kInstrSize;
    DCHECK(Assembler::IsMovImmed(Memory::int32_at(load_address)));
    DCHECK(Assembler::IsOrrImmed(
        Memory::int32_at(load_address + Assembler::kInstrSize)));
    DCHECK(Assembler::IsOrrImmed(
        Memory::int32_at(load_address + 2 * Assembler::kInstrSize)));
  } else {
    // This is a small constant pool lookup.
    DCHECK(Assembler::IsLdrPpImmediateOffset(Memory::int32_at(load_address)));
  }
  return load_address;
}


void BackEdgeTable::PatchAt(Code* unoptimized_code,
                            Address pc,
                            BackEdgeState target_state,
                            Code* replacement_code) {
  Address pc_immediate_load_address = GetInterruptImmediateLoadAddress(pc);
  Address branch_address = pc_immediate_load_address - Assembler::kInstrSize;
  Isolate* isolate = unoptimized_code->GetIsolate();
  CodePatcher patcher(isolate, branch_address, 1);
  switch (target_state) {
    case INTERRUPT:
    {
      //  <decrement profiling counter>
      //   bpl ok
      //   ; load interrupt stub address into ip - either of (for ARMv7):
      //   ; <small cp load>      |  <extended cp load> |  <immediate load>
      //   ldr ip, [pc/pp, #imm]  |   movw ip, #imm     |   movw ip, #imm
      //                          |   movt ip, #imm     |   movw ip, #imm
      //                          |   ldr  ip, [pp, ip]
      //   ; or (for ARMv6):
      //   ; <small cp load>      |  <extended cp load> |  <immediate load>
      //   ldr ip, [pc/pp, #imm]  |   mov ip, #imm      |   mov ip, #imm
      //                          |   orr ip, ip, #imm> |   orr ip, ip, #imm
      //                          |   orr ip, ip, #imm> |   orr ip, ip, #imm
      //                          |   orr ip, ip, #imm> |   orr ip, ip, #imm
      //   blx ip
      //  <reset profiling counter>
      //  ok-label

      // Calculate branch offset to the ok-label - this is the difference
      // between the branch address and |pc| (which points at <blx ip>) plus
      // kProfileCounterResetSequence instructions
      int branch_offset = pc - Instruction::kPCReadOffset - branch_address +
                          kProfileCounterResetSequenceLength;
      patcher.masm()->b(branch_offset, pl);
      break;
    }
    case ON_STACK_REPLACEMENT:
    case OSR_AFTER_STACK_CHECK:
      //  <decrement profiling counter>
      //   mov r0, r0 (NOP)
      //   ; load on-stack replacement address into ip - either of (for ARMv7):
      //   ; <small cp load>      |  <extended cp load> |  <immediate load>
      //   ldr ip, [pc/pp, #imm]  |   movw ip, #imm     |   movw ip, #imm
      //                          |   movt ip, #imm>    |   movw ip, #imm
      //                          |   ldr  ip, [pp, ip]
      //   ; or (for ARMv6):
      //   ; <small cp load>      |  <extended cp load> |  <immediate load>
      //   ldr ip, [pc/pp, #imm]  |   mov ip, #imm      |   mov ip, #imm
      //                          |   orr ip, ip, #imm> |   orr ip, ip, #imm
      //                          |   orr ip, ip, #imm> |   orr ip, ip, #imm
      //                          |   orr ip, ip, #imm> |   orr ip, ip, #imm
      //   blx ip
      //  <reset profiling counter>
      //  ok-label
      patcher.masm()->nop();
      break;
  }

  // Replace the call address.
  Assembler::set_target_address_at(isolate, pc_immediate_load_address,
                                   unoptimized_code, replacement_code->entry());

  unoptimized_code->GetHeap()->incremental_marking()->RecordCodeTargetPatch(
      unoptimized_code, pc_immediate_load_address, replacement_code);
}


BackEdgeTable::BackEdgeState BackEdgeTable::GetBackEdgeState(
    Isolate* isolate,
    Code* unoptimized_code,
    Address pc) {
  DCHECK(Assembler::IsBlxIp(Memory::int32_at(pc - Assembler::kInstrSize)));

  Address pc_immediate_load_address = GetInterruptImmediateLoadAddress(pc);
  Address branch_address = pc_immediate_load_address - Assembler::kInstrSize;
  Address interrupt_address = Assembler::target_address_at(
      pc_immediate_load_address, unoptimized_code);

  if (Assembler::IsBranch(Assembler::instr_at(branch_address))) {
    DCHECK(interrupt_address ==
           isolate->builtins()->InterruptCheck()->entry());
    return INTERRUPT;
  }

  DCHECK(Assembler::IsNop(Assembler::instr_at(branch_address)));

  if (interrupt_address ==
      isolate->builtins()->OnStackReplacement()->entry()) {
    return ON_STACK_REPLACEMENT;
  }

  DCHECK(interrupt_address ==
         isolate->builtins()->OsrAfterStackCheck()->entry());
  return OSR_AFTER_STACK_CHECK;
}


}  // namespace internal
}  // namespace v8

#endif  // V8_TARGET_ARCH_ARM
