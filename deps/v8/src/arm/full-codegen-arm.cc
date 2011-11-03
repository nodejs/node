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

#if defined(V8_TARGET_ARCH_ARM)

#include "code-stubs.h"
#include "codegen.h"
#include "compiler.h"
#include "debug.h"
#include "full-codegen.h"
#include "parser.h"
#include "scopes.h"
#include "stub-cache.h"

#include "arm/code-stubs-arm.h"

namespace v8 {
namespace internal {

#define __ ACCESS_MASM(masm_)


static unsigned GetPropertyId(Property* property) {
  return property->id();
}


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
    __ bind(&patch_site_);
    __ cmp(reg, Operand(reg));
    // Don't use b(al, ...) as that might emit the constant pool right after the
    // branch. After patching when the branch is no longer unconditional
    // execution can continue into the constant pool.
    __ b(eq, target);  // Always taken before patched.
  }

  // When initially emitting this ensure that a jump is never generated to skip
  // the inlined smi code.
  void EmitJumpIfSmi(Register reg, Label* target) {
    ASSERT(!patch_site_.is_bound() && !info_emitted_);
    __ bind(&patch_site_);
    __ cmp(reg, Operand(reg));
    __ b(ne, target);  // Never taken before patched.
  }

  void EmitPatchInfo() {
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
//   o r1: the JS function object being called (ie, ourselves)
//   o cp: our context
//   o fp: our caller's frame pointer
//   o sp: stack pointer
//   o lr: return address
//
// The function builds a JS frame.  Please see JavaScriptFrameConstants in
// frames-arm.h for its layout.
void FullCodeGenerator::Generate(CompilationInfo* info) {
  ASSERT(info_ == NULL);
  info_ = info;
  scope_ = info->scope();
  SetFunctionPosition(function());
  Comment cmnt(masm_, "[ function compiled by full code generator");

#ifdef DEBUG
  if (strlen(FLAG_stop_at) > 0 &&
      info->function()->name()->IsEqualTo(CStrVector(FLAG_stop_at))) {
    __ stop("stop-at");
  }
#endif

  // Strict mode functions and builtins need to replace the receiver
  // with undefined when called as functions (without an explicit
  // receiver object). r5 is zero for method calls and non-zero for
  // function calls.
  if (info->is_strict_mode() || info->is_native()) {
    Label ok;
    __ cmp(r5, Operand(0));
    __ b(eq, &ok);
    int receiver_offset = info->scope()->num_parameters() * kPointerSize;
    __ LoadRoot(r2, Heap::kUndefinedValueRootIndex);
    __ str(r2, MemOperand(sp, receiver_offset));
    __ bind(&ok);
  }

  int locals_count = info->scope()->num_stack_slots();

  __ Push(lr, fp, cp, r1);
  if (locals_count > 0) {
    // Load undefined value here, so the value is ready for the loop
    // below.
    __ LoadRoot(ip, Heap::kUndefinedValueRootIndex);
  }
  // Adjust fp to point to caller's fp.
  __ add(fp, sp, Operand(2 * kPointerSize));

  { Comment cmnt(masm_, "[ Allocate locals");
    for (int i = 0; i < locals_count; i++) {
      __ push(ip);
    }
  }

  bool function_in_register = true;

  // Possibly allocate a local context.
  int heap_slots = info->scope()->num_heap_slots() - Context::MIN_CONTEXT_SLOTS;
  if (heap_slots > 0) {
    Comment cmnt(masm_, "[ Allocate local context");
    // Argument to NewContext is the function, which is in r1.
    __ push(r1);
    if (heap_slots <= FastNewContextStub::kMaximumSlots) {
      FastNewContextStub stub(heap_slots);
      __ CallStub(&stub);
    } else {
      __ CallRuntime(Runtime::kNewFunctionContext, 1);
    }
    function_in_register = false;
    // Context is returned in both r0 and cp.  It replaces the context
    // passed to us.  It's saved in the stack and kept live in cp.
    __ str(cp, MemOperand(fp, StandardFrameConstants::kContextOffset));
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
        __ mov(r1, Operand(Context::SlotOffset(var->index())));
        __ str(r0, MemOperand(cp, r1));
        // Update the write barrier. This clobbers all involved
        // registers, so we have to use two more registers to avoid
        // clobbering cp.
        __ mov(r2, Operand(cp));
        __ RecordWrite(r2, Operand(r1), r3, r0);
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
    if (is_strict_mode()) {
      type = ArgumentsAccessStub::NEW_STRICT;
    } else if (function()->has_duplicate_parameters()) {
      type = ArgumentsAccessStub::NEW_NON_STRICT_SLOW;
    } else {
      type = ArgumentsAccessStub::NEW_NON_STRICT_FAST;
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
      __ LoadRoot(ip, Heap::kStackLimitRootIndex);
      __ cmp(sp, Operand(ip));
      __ b(hs, &ok);
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
    __ LoadRoot(r0, Heap::kUndefinedValueRootIndex);
  }
  EmitReturnSequence();

  // Force emit the constant pool, so it doesn't get emitted in the middle
  // of the stack check table.
  masm()->CheckConstPool(true, false);
}


void FullCodeGenerator::ClearAccumulator() {
  __ mov(r0, Operand(Smi::FromInt(0)));
}


void FullCodeGenerator::EmitStackCheck(IterationStatement* stmt) {
  Comment cmnt(masm_, "[ Stack check");
  Label ok;
  __ LoadRoot(ip, Heap::kStackLimitRootIndex);
  __ cmp(sp, Operand(ip));
  __ b(hs, &ok);
  StackCheckStub stub;
  __ CallStub(&stub);
  // Record a mapping of this PC offset to the OSR id.  This is used to find
  // the AST id from the unoptimized code in order to use it as a key into
  // the deoptimization input data found in the optimized code.
  RecordStackCheck(stmt->OsrEntryId());

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

#ifdef DEBUG
    // Add a label for checking the size of the code used for returning.
    Label check_exit_codesize;
    masm_->bind(&check_exit_codesize);
#endif
    // Make sure that the constant pool is not emitted inside of the return
    // sequence.
    { Assembler::BlockConstPoolScope block_const_pool(masm_);
      // Here we use masm_-> instead of the __ macro to avoid the code coverage
      // tool from instrumenting as we rely on the code size here.
      int32_t sp_delta = (info_->scope()->num_parameters() + 1) * kPointerSize;
      CodeGenerator::RecordPositions(masm_, function()->end_position() - 1);
      __ RecordJSReturn();
      masm_->mov(sp, fp);
      masm_->ldm(ia_w, sp, fp.bit() | lr.bit());
      masm_->add(sp, sp, Operand(sp_delta));
      masm_->Jump(lr);
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
  codegen()->PrepareForBailoutBeforeSplit(TOS_REG, false, NULL, NULL);
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
  codegen()->PrepareForBailoutBeforeSplit(TOS_REG,
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
  codegen()->PrepareForBailoutBeforeSplit(TOS_REG,
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
  codegen()->PrepareForBailoutBeforeSplit(TOS_REG, false, NULL, NULL);
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
  __ push(ip);
  __ jmp(&done);
  __ bind(materialize_false);
  __ LoadRoot(ip, Heap::kFalseValueRootIndex);
  __ push(ip);
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
  __ LoadRoot(ip, value_root_index);
  __ push(ip);
}


void FullCodeGenerator::TestContext::Plug(bool flag) const {
  codegen()->PrepareForBailoutBeforeSplit(TOS_REG,
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
  if (CpuFeatures::IsSupported(VFP3)) {
    ToBooleanStub stub(result_register());
    __ CallStub(&stub);
    __ tst(result_register(), result_register());
  } else {
    // Call the runtime to find the boolean value of the source and then
    // translate it into control flow to the pair of labels.
    __ push(result_register());
    __ CallRuntime(Runtime::kToBool, 1);
    __ LoadRoot(ip, Heap::kFalseValueRootIndex);
    __ cmp(r0, ip);
  }
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
    __ RecordWrite(scratch0,
                   Operand(Context::SlotOffset(var->index())),
                   scratch1,
                   src);
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
  if (should_normalize) __ b(&skip);

  ForwardBailoutStack* current = forward_bailout_stack_;
  while (current != NULL) {
    PrepareForBailout(current->expr(), state);
    current = current->parent();
  }

  if (should_normalize) {
    __ LoadRoot(ip, Heap::kTrueValueRootIndex);
    __ cmp(r0, ip);
    Split(eq, if_true, if_false, NULL);
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
        __ str(result_register(), StackOperand(variable));
      } else if (mode == Variable::CONST || mode == Variable::LET) {
        Comment cmnt(masm_, "[ Declaration");
        __ LoadRoot(ip, Heap::kTheHoleValueRootIndex);
        __ str(ip, StackOperand(variable));
      }
      break;

    case Variable::CONTEXT:
      // The variable in the decl always resides in the current function
      // context.
      ASSERT_EQ(0, scope()->ContextChainLength(variable->scope()));
      if (FLAG_debug_code) {
        // Check that we're not inside a with or catch context.
        __ ldr(r1, FieldMemOperand(cp, HeapObject::kMapOffset));
        __ CompareRoot(r1, Heap::kWithContextMapRootIndex);
        __ Check(ne, "Declaration in with context.");
        __ CompareRoot(r1, Heap::kCatchContextMapRootIndex);
        __ Check(ne, "Declaration in catch context.");
      }
      if (function != NULL) {
        Comment cmnt(masm_, "[ Declaration");
        VisitForAccumulatorValue(function);
        __ str(result_register(), ContextOperand(cp, variable->index()));
        int offset = Context::SlotOffset(variable->index());
        // We know that we have written a function, which is not a smi.
        __ mov(r1, Operand(cp));
        __ RecordWrite(r1, Operand(offset), r2, result_register());
        PrepareForBailoutForId(proxy->id(), NO_REGISTERS);
      } else if (mode == Variable::CONST || mode == Variable::LET) {
        Comment cmnt(masm_, "[ Declaration");
        __ LoadRoot(ip, Heap::kTheHoleValueRootIndex);
        __ str(ip, ContextOperand(cp, variable->index()));
        // No write barrier since the_hole_value is in old space.
        PrepareForBailoutForId(proxy->id(), NO_REGISTERS);
      }
      break;

    case Variable::LOOKUP: {
      Comment cmnt(masm_, "[ Declaration");
      __ mov(r2, Operand(variable->name()));
      // Declaration nodes are always introduced in one of three modes.
      ASSERT(mode == Variable::VAR ||
             mode == Variable::CONST ||
             mode == Variable::LET);
      PropertyAttributes attr = (mode == Variable::CONST) ? READ_ONLY : NONE;
      __ mov(r1, Operand(Smi::FromInt(attr)));
      // Push initial value, if any.
      // Note: For variables we must not push an initial value (such as
      // 'undefined') because we may have a (legal) redeclaration and we
      // must not destroy the current value.
      if (function != NULL) {
        __ Push(cp, r2, r1);
        // Push initial value for function declaration.
        VisitForStackValue(function);
      } else if (mode == Variable::CONST || mode == Variable::LET) {
        __ LoadRoot(r0, Heap::kTheHoleValueRootIndex);
        __ Push(cp, r2, r1, r0);
      } else {
        __ mov(r0, Operand(Smi::FromInt(0)));  // Indicates no initial value.
        __ Push(cp, r2, r1, r0);
      }
      __ CallRuntime(Runtime::kDeclareContextSlot, 4);
      break;
    }
  }
}


void FullCodeGenerator::VisitDeclaration(Declaration* decl) { }


void FullCodeGenerator::DeclareGlobals(Handle<FixedArray> pairs) {
  // Call the runtime to declare the globals.
  // The context is the first argument.
  __ mov(r1, Operand(pairs));
  __ mov(r0, Operand(Smi::FromInt(DeclareGlobalsFlags())));
  __ Push(cp, r1, r0);
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
    Handle<Code> ic = CompareIC::GetUninitialized(Token::EQ_STRICT);
    __ Call(ic, RelocInfo::CODE_TARGET, clause->CompareId());
    patch_site.EmitPatchInfo();

    __ cmp(r0, Operand(0));
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
  SetStatementPosition(stmt);

  Label loop, exit;
  ForIn loop_statement(this, stmt);
  increment_loop_depth();

  // Get the object to enumerate over. Both SpiderMonkey and JSC
  // ignore null and undefined in contrast to the specification; see
  // ECMA-262 section 12.6.4.
  VisitForAccumulatorValue(stmt->enumerable());
  __ LoadRoot(ip, Heap::kUndefinedValueRootIndex);
  __ cmp(r0, ip);
  __ b(eq, &exit);
  Register null_value = r5;
  __ LoadRoot(null_value, Heap::kNullValueRootIndex);
  __ cmp(r0, null_value);
  __ b(eq, &exit);

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

  // Check cache validity in generated code. This is a fast case for
  // the JSObject::IsSimpleEnum cache validity checks. If we cannot
  // guarantee cache validity, call the runtime system to check cache
  // validity or get the property names in a fixed array.
  Label next, call_runtime;
  // Preload a couple of values used in the loop.
  Register  empty_fixed_array_value = r6;
  __ LoadRoot(empty_fixed_array_value, Heap::kEmptyFixedArrayRootIndex);
  Register empty_descriptor_array_value = r7;
  __ LoadRoot(empty_descriptor_array_value,
              Heap::kEmptyDescriptorArrayRootIndex);
  __ mov(r1, r0);
  __ bind(&next);

  // Check that there are no elements.  Register r1 contains the
  // current JS object we've reached through the prototype chain.
  __ ldr(r2, FieldMemOperand(r1, JSObject::kElementsOffset));
  __ cmp(r2, empty_fixed_array_value);
  __ b(ne, &call_runtime);

  // Check that instance descriptors are not empty so that we can
  // check for an enum cache.  Leave the map in r2 for the subsequent
  // prototype load.
  __ ldr(r2, FieldMemOperand(r1, HeapObject::kMapOffset));
  __ ldr(r3, FieldMemOperand(r2, Map::kInstanceDescriptorsOrBitField3Offset));
  __ JumpIfSmi(r3, &call_runtime);

  // Check that there is an enum cache in the non-empty instance
  // descriptors (r3).  This is the case if the next enumeration
  // index field does not contain a smi.
  __ ldr(r3, FieldMemOperand(r3, DescriptorArray::kEnumerationIndexOffset));
  __ JumpIfSmi(r3, &call_runtime);

  // For all objects but the receiver, check that the cache is empty.
  Label check_prototype;
  __ cmp(r1, r0);
  __ b(eq, &check_prototype);
  __ ldr(r3, FieldMemOperand(r3, DescriptorArray::kEnumCacheBridgeCacheOffset));
  __ cmp(r3, empty_fixed_array_value);
  __ b(ne, &call_runtime);

  // Load the prototype from the map and loop if non-null.
  __ bind(&check_prototype);
  __ ldr(r1, FieldMemOperand(r2, Map::kPrototypeOffset));
  __ cmp(r1, null_value);
  __ b(ne, &next);

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
  __ mov(r2, r0);
  __ ldr(r1, FieldMemOperand(r2, HeapObject::kMapOffset));
  __ LoadRoot(ip, Heap::kMetaMapRootIndex);
  __ cmp(r1, ip);
  __ b(ne, &fixed_array);

  // We got a map in register r0. Get the enumeration cache from it.
  __ bind(&use_cache);
  __ LoadInstanceDescriptors(r0, r1);
  __ ldr(r1, FieldMemOperand(r1, DescriptorArray::kEnumerationIndexOffset));
  __ ldr(r2, FieldMemOperand(r1, DescriptorArray::kEnumCacheBridgeCacheOffset));

  // Setup the four remaining stack slots.
  __ push(r0);  // Map.
  __ ldr(r1, FieldMemOperand(r2, FixedArray::kLengthOffset));
  __ mov(r0, Operand(Smi::FromInt(0)));
  // Push enumeration cache, enumeration cache length (as smi) and zero.
  __ Push(r2, r1, r0);
  __ jmp(&loop);

  // We got a fixed array in register r0. Iterate through that.
  __ bind(&fixed_array);
  __ mov(r1, Operand(Smi::FromInt(0)));  // Map (0) - force slow check.
  __ Push(r1, r0);
  __ ldr(r1, FieldMemOperand(r0, FixedArray::kLengthOffset));
  __ mov(r0, Operand(Smi::FromInt(0)));
  __ Push(r1, r0);  // Fixed array length (as smi) and initial index.

  // Generate code for doing the condition check.
  __ bind(&loop);
  // Load the current count to r0, load the length to r1.
  __ Ldrd(r0, r1, MemOperand(sp, 0 * kPointerSize));
  __ cmp(r0, r1);  // Compare to the array length.
  __ b(hs, loop_statement.break_label());

  // Get the current entry of the array into register r3.
  __ ldr(r2, MemOperand(sp, 2 * kPointerSize));
  __ add(r2, r2, Operand(FixedArray::kHeaderSize - kHeapObjectTag));
  __ ldr(r3, MemOperand(r2, r0, LSL, kPointerSizeLog2 - kSmiTagSize));

  // Get the expected map from the stack or a zero map in the
  // permanent slow case into register r2.
  __ ldr(r2, MemOperand(sp, 3 * kPointerSize));

  // Check if the expected map still matches that of the enumerable.
  // If not, we have to filter the key.
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
  __ InvokeBuiltin(Builtins::FILTER_KEY, CALL_FUNCTION);
  __ mov(r3, Operand(r0), SetCC);
  __ b(eq, loop_statement.continue_label());

  // Update the 'each' property or variable from the possibly filtered
  // entry in register r3.
  __ bind(&update_each);
  __ mov(result_register(), r3);
  // Perform the assignment as if via '='.
  { EffectContext context(this);
    EmitAssignment(stmt->each(), stmt->AssignmentId());
  }

  // Generate code for the body of the loop.
  Visit(stmt->body());

  // Generate code for the going to the next element by incrementing
  // the index (smi) stored on top of the stack.
  __ bind(loop_statement.continue_label());
  __ pop(r0);
  __ add(r0, r0, Operand(Smi::FromInt(1)));
  __ push(r0);

  EmitStackCheck(stmt);
  __ b(&loop);

  // Remove the pointers stored on the stack.
  __ bind(loop_statement.break_label());
  __ Drop(5);

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
    __ mov(r0, Operand(info));
    __ push(r0);
    __ CallStub(&stub);
  } else {
    __ mov(r0, Operand(info));
    __ LoadRoot(r1, pretenure ? Heap::kTrueValueRootIndex
                              : Heap::kFalseValueRootIndex);
    __ Push(cp, r0, r1);
    __ CallRuntime(Runtime::kNewClosure, 3);
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
      if (s->calls_eval()) {
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
    if (!s->outer_scope_calls_eval() || s->is_eval_scope()) break;
    s = s->outer_scope();
  }

  if (s->is_eval_scope()) {
    Label loop, fast;
    if (!current.is(next)) {
      __ Move(next, current);
    }
    __ bind(&loop);
    // Terminate at global context.
    __ ldr(temp, FieldMemOperand(next, HeapObject::kMapOffset));
    __ LoadRoot(ip, Heap::kGlobalContextMapRootIndex);
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
  RelocInfo::Mode mode = (typeof_state == INSIDE_TYPEOF)
      ? RelocInfo::CODE_TARGET
      : RelocInfo::CODE_TARGET_CONTEXT;
  Handle<Code> ic = isolate()->builtins()->LoadIC_Initialize();
  __ Call(ic, mode);
}


MemOperand FullCodeGenerator::ContextSlotOperandCheckExtensions(Variable* var,
                                                                Label* slow) {
  ASSERT(var->IsContextSlot());
  Register context = cp;
  Register next = r3;
  Register temp = r4;

  for (Scope* s = scope(); s != var->scope(); s = s->outer_scope()) {
    if (s->num_heap_slots() > 0) {
      if (s->calls_eval()) {
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
  if (var->mode() == Variable::DYNAMIC_GLOBAL) {
    EmitLoadGlobalCheckExtensions(var, typeof_state, slow);
    __ jmp(done);
  } else if (var->mode() == Variable::DYNAMIC_LOCAL) {
    Variable* local = var->local_if_not_shadowed();
    __ ldr(r0, ContextSlotOperandCheckExtensions(local, slow));
    if (local->mode() == Variable::CONST) {
      __ CompareRoot(r0, Heap::kTheHoleValueRootIndex);
      __ LoadRoot(r0, Heap::kUndefinedValueRootIndex, eq);
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
      // Use inline caching. Variable name is passed in r2 and the global
      // object (receiver) in r0.
      __ ldr(r0, GlobalObjectOperand());
      __ mov(r2, Operand(var->name()));
      Handle<Code> ic = isolate()->builtins()->LoadIC_Initialize();
      __ Call(ic, RelocInfo::CODE_TARGET_CONTEXT);
      context()->Plug(r0);
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
        GetVar(r0, var);
        __ CompareRoot(r0, Heap::kTheHoleValueRootIndex);
        if (var->mode() == Variable::LET) {
          Label done;
          __ b(ne, &done);
          __ mov(r0, Operand(var->name()));
          __ push(r0);
          __ CallRuntime(Runtime::kThrowReferenceError, 1);
          __ bind(&done);
        } else {
          __ LoadRoot(r0, Heap::kUndefinedValueRootIndex, eq);
        }
        context()->Plug(r0);
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
      __ mov(r1, Operand(var->name()));
      __ Push(cp, r1);  // Context and name.
      __ CallRuntime(Runtime::kLoadContextSlot, 2);
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
  __ CallRuntime(Runtime::kMaterializeRegExpLiteral, 4);
  __ mov(r5, r0);

  __ bind(&materialized);
  int size = JSRegExp::kSize + JSRegExp::kInObjectFieldCount * kPointerSize;
  Label allocated, runtime_allocate;
  __ AllocateInNewSpace(size, r0, r2, r3, &runtime_allocate, TAG_OBJECT);
  __ jmp(&allocated);

  __ bind(&runtime_allocate);
  __ push(r5);
  __ mov(r0, Operand(Smi::FromInt(size)));
  __ push(r0);
  __ CallRuntime(Runtime::kAllocateInNewSpace, 1);
  __ pop(r5);

  __ bind(&allocated);
  // After this, registers are used as follows:
  // r0: Newly allocated regexp.
  // r5: Materialized regexp.
  // r2: temp.
  __ CopyFields(r0, r5, r2.bit(), size / kPointerSize);
  context()->Plug(r0);
}


void FullCodeGenerator::VisitObjectLiteral(ObjectLiteral* expr) {
  Comment cmnt(masm_, "[ ObjectLiteral");
  __ ldr(r3, MemOperand(fp,  JavaScriptFrameConstants::kFunctionOffset));
  __ ldr(r3, FieldMemOperand(r3, JSFunction::kLiteralsOffset));
  __ mov(r2, Operand(Smi::FromInt(expr->literal_index())));
  __ mov(r1, Operand(expr->constant_properties()));
  int flags = expr->fast_elements()
      ? ObjectLiteral::kFastElements
      : ObjectLiteral::kNoFlags;
  flags |= expr->has_function()
      ? ObjectLiteral::kHasFunction
      : ObjectLiteral::kNoFlags;
  __ mov(r0, Operand(Smi::FromInt(flags)));
  __ Push(r3, r2, r1, r0);
  if (expr->depth() > 1) {
    __ CallRuntime(Runtime::kCreateObjectLiteral, 4);
  } else {
    __ CallRuntime(Runtime::kCreateObjectLiteralShallow, 4);
  }

  // If result_saved is true the result is on top of the stack.  If
  // result_saved is false the result is in r0.
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
        if (key->handle()->IsSymbol()) {
          if (property->emit_store()) {
            VisitForAccumulatorValue(value);
            __ mov(r2, Operand(key->handle()));
            __ ldr(r1, MemOperand(sp));
            Handle<Code> ic = is_strict_mode()
                ? isolate()->builtins()->StoreIC_Initialize_Strict()
                : isolate()->builtins()->StoreIC_Initialize();
            __ Call(ic, RelocInfo::CODE_TARGET, key->id());
            PrepareForBailoutForId(key->id(), NO_REGISTERS);
          } else {
            VisitForEffect(value);
          }
          break;
        }
        // Fall through.
      case ObjectLiteral::Property::PROTOTYPE:
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
      case ObjectLiteral::Property::GETTER:
      case ObjectLiteral::Property::SETTER:
        // Duplicate receiver on stack.
        __ ldr(r0, MemOperand(sp));
        __ push(r0);
        VisitForStackValue(key);
        __ mov(r1, Operand(property->kind() == ObjectLiteral::Property::SETTER ?
                           Smi::FromInt(1) :
                           Smi::FromInt(0)));
        __ push(r1);
        VisitForStackValue(value);
        __ CallRuntime(Runtime::kDefineAccessor, 4);
        break;
    }
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

  ZoneList<Expression*>* subexprs = expr->values();
  int length = subexprs->length();

  __ ldr(r3, MemOperand(fp, JavaScriptFrameConstants::kFunctionOffset));
  __ ldr(r3, FieldMemOperand(r3, JSFunction::kLiteralsOffset));
  __ mov(r2, Operand(Smi::FromInt(expr->literal_index())));
  __ mov(r1, Operand(expr->constant_elements()));
  __ Push(r3, r2, r1);
  if (expr->constant_elements()->map() ==
      isolate()->heap()->fixed_cow_array_map()) {
    FastCloneShallowArrayStub stub(
        FastCloneShallowArrayStub::COPY_ON_WRITE_ELEMENTS, length);
    __ CallStub(&stub);
    __ IncrementCounter(
        isolate()->counters()->cow_arrays_created_stub(), 1, r1, r2);
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
      __ push(r0);
      result_saved = true;
    }
    VisitForAccumulatorValue(subexpr);

    // Store the subexpression value in the array's elements.
    __ ldr(r1, MemOperand(sp));  // Copy of array literal.
    __ ldr(r1, FieldMemOperand(r1, JSObject::kElementsOffset));
    int offset = FixedArray::kHeaderSize + (i * kPointerSize);
    __ str(result_register(), FieldMemOperand(r1, offset));

    // Update the write barrier for the array store with r0 as the scratch
    // register.
    __ RecordWrite(r1, Operand(offset), r2, result_register());

    PrepareForBailoutForId(expr->GetIdForElement(i), NO_REGISTERS);
  }

  if (result_saved) {
    context()->PlugTOS();
  } else {
    context()->Plug(r0);
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
          PrepareForBailoutForId(expr->CompoundLoadId(), TOS_REG);
          break;
        case KEYED_PROPERTY:
          EmitKeyedPropertyLoad(property);
          PrepareForBailoutForId(expr->CompoundLoadId(), TOS_REG);
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


void FullCodeGenerator::EmitNamedPropertyLoad(Property* prop) {
  SetSourcePosition(prop->position());
  Literal* key = prop->key()->AsLiteral();
  __ mov(r2, Operand(key->handle()));
  // Call load IC. It has arguments receiver and property name r0 and r2.
  Handle<Code> ic = isolate()->builtins()->LoadIC_Initialize();
  __ Call(ic, RelocInfo::CODE_TARGET, GetPropertyId(prop));
}


void FullCodeGenerator::EmitKeyedPropertyLoad(Property* prop) {
  SetSourcePosition(prop->position());
  // Call keyed load IC. It has arguments key and receiver in r0 and r1.
  Handle<Code> ic = isolate()->builtins()->KeyedLoadIC_Initialize();
  __ Call(ic, RelocInfo::CODE_TARGET, GetPropertyId(prop));
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
  BinaryOpStub stub(op, mode);
  __ Call(stub.GetCode(), RelocInfo::CODE_TARGET, expr->id());
  patch_site.EmitPatchInfo();
  __ jmp(&done);

  __ bind(&smi_case);
  // Smi case. This code works the same way as the smi-smi case in the type
  // recording binary operation stub, see
  // BinaryOpStub::GenerateSmiSmiOperation for comments.
  switch (op) {
    case Token::SAR:
      __ b(&stub_call);
      __ GetLeastBitsFromSmi(scratch1, right, 5);
      __ mov(right, Operand(left, ASR, scratch1));
      __ bic(right, right, Operand(kSmiTagMask));
      break;
    case Token::SHL: {
      __ b(&stub_call);
      __ SmiUntag(scratch1, left);
      __ GetLeastBitsFromSmi(scratch2, right, 5);
      __ mov(scratch1, Operand(scratch1, LSL, scratch2));
      __ add(scratch2, scratch1, Operand(0x40000000), SetCC);
      __ b(mi, &stub_call);
      __ SmiTag(right, scratch1);
      break;
    }
    case Token::SHR: {
      __ b(&stub_call);
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
      __ tst(scratch1, Operand(scratch1));
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
  BinaryOpStub stub(op, mode);
  JumpPatchSite patch_site(masm_);    // unbound, signals no inlined smi code.
  __ Call(stub.GetCode(), RelocInfo::CODE_TARGET, expr->id());
  patch_site.EmitPatchInfo();
  context()->Plug(r0);
}


void FullCodeGenerator::EmitAssignment(Expression* expr, int bailout_ast_id) {
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
      __ push(r0);  // Preserve value.
      VisitForAccumulatorValue(prop->obj());
      __ mov(r1, r0);
      __ pop(r0);  // Restore value.
      __ mov(r2, Operand(prop->key()->AsLiteral()->handle()));
      Handle<Code> ic = is_strict_mode()
          ? isolate()->builtins()->StoreIC_Initialize_Strict()
          : isolate()->builtins()->StoreIC_Initialize();
      __ Call(ic);
      break;
    }
    case KEYED_PROPERTY: {
      __ push(r0);  // Preserve value.
      VisitForStackValue(prop->obj());
      VisitForAccumulatorValue(prop->key());
      __ mov(r1, r0);
      __ pop(r2);
      __ pop(r0);  // Restore value.
      Handle<Code> ic = is_strict_mode()
          ? isolate()->builtins()->KeyedStoreIC_Initialize_Strict()
          : isolate()->builtins()->KeyedStoreIC_Initialize();
      __ Call(ic);
      break;
    }
  }
  PrepareForBailoutForId(bailout_ast_id, TOS_REG);
  context()->Plug(r0);
}


void FullCodeGenerator::EmitVariableAssignment(Variable* var,
                                               Token::Value op) {
  if (var->IsUnallocated()) {
    // Global var, const, or let.
    __ mov(r2, Operand(var->name()));
    __ ldr(r1, GlobalObjectOperand());
    Handle<Code> ic = is_strict_mode()
        ? isolate()->builtins()->StoreIC_Initialize_Strict()
        : isolate()->builtins()->StoreIC_Initialize();
    __ Call(ic, RelocInfo::CODE_TARGET_CONTEXT);

  } else if (op == Token::INIT_CONST) {
    // Const initializers need a write barrier.
    ASSERT(!var->IsParameter());  // No const parameters.
    if (var->IsStackLocal()) {
      Label skip;
      __ ldr(r1, StackOperand(var));
      __ CompareRoot(r1, Heap::kTheHoleValueRootIndex);
      __ b(ne, &skip);
      __ str(result_register(), StackOperand(var));
      __ bind(&skip);
    } else {
      ASSERT(var->IsContextSlot() || var->IsLookupSlot());
      // Like var declarations, const declarations are hoisted to function
      // scope.  However, unlike var initializers, const initializers are
      // able to drill a hole to that function context, even from inside a
      // 'with' context.  We thus bypass the normal static scope lookup for
      // var->IsContextSlot().
      __ push(r0);
      __ mov(r0, Operand(var->name()));
      __ Push(cp, r0);  // Context and name.
      __ CallRuntime(Runtime::kInitializeConstContextSlot, 3);
    }

  } else if (var->mode() == Variable::LET && op != Token::INIT_LET) {
    // Non-initializing assignment to let variable needs a write barrier.
    if (var->IsLookupSlot()) {
      __ push(r0);  // Value.
      __ mov(r1, Operand(var->name()));
      __ mov(r0, Operand(Smi::FromInt(strict_mode_flag())));
      __ Push(cp, r1, r0);  // Context, name, strict mode.
      __ CallRuntime(Runtime::kStoreContextSlot, 4);
    } else {
      ASSERT(var->IsStackAllocated() || var->IsContextSlot());
      Label assign;
      MemOperand location = VarOperand(var, r1);
      __ ldr(r3, location);
      __ CompareRoot(r3, Heap::kTheHoleValueRootIndex);
      __ b(ne, &assign);
      __ mov(r3, Operand(var->name()));
      __ push(r3);
      __ CallRuntime(Runtime::kThrowReferenceError, 1);
      // Perform the assignment.
      __ bind(&assign);
      __ str(result_register(), location);
      if (var->IsContextSlot()) {
        // RecordWrite may destroy all its register arguments.
        __ mov(r3, result_register());
        int offset = Context::SlotOffset(var->index());
        __ RecordWrite(r1, Operand(offset), r2, r3);
      }
    }

  } else if (var->mode() != Variable::CONST) {
    // Assignment to var or initializing assignment to let.
    if (var->IsStackAllocated() || var->IsContextSlot()) {
      MemOperand location = VarOperand(var, r1);
      if (FLAG_debug_code && op == Token::INIT_LET) {
        // Check for an uninitialized let binding.
        __ ldr(r2, location);
        __ CompareRoot(r2, Heap::kTheHoleValueRootIndex);
        __ Check(eq, "Let binding re-initialization.");
      }
      // Perform the assignment.
      __ str(r0, location);
      if (var->IsContextSlot()) {
        __ mov(r3, r0);
        __ RecordWrite(r1, Operand(Context::SlotOffset(var->index())), r2, r3);
      }
    } else {
      ASSERT(var->IsLookupSlot());
      __ push(r0);  // Value.
      __ mov(r1, Operand(var->name()));
      __ mov(r0, Operand(Smi::FromInt(strict_mode_flag())));
      __ Push(cp, r1, r0);  // Context, name, strict mode.
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
    __ ldr(ip, MemOperand(sp, kPointerSize));  // Receiver is now under value.
    __ push(ip);
    __ CallRuntime(Runtime::kToSlowProperties, 1);
    __ pop(result_register());
  }

  // Record source code position before IC call.
  SetSourcePosition(expr->position());
  __ mov(r2, Operand(prop->key()->AsLiteral()->handle()));
  // Load receiver to r1. Leave a copy in the stack if needed for turning the
  // receiver into fast case.
  if (expr->ends_initialization_block()) {
    __ ldr(r1, MemOperand(sp));
  } else {
    __ pop(r1);
  }

  Handle<Code> ic = is_strict_mode()
      ? isolate()->builtins()->StoreIC_Initialize_Strict()
      : isolate()->builtins()->StoreIC_Initialize();
  __ Call(ic, RelocInfo::CODE_TARGET, expr->id());

  // If the assignment ends an initialization block, revert to fast case.
  if (expr->ends_initialization_block()) {
    __ push(r0);  // Result of assignment, saved even if not needed.
    // Receiver is under the result value.
    __ ldr(ip, MemOperand(sp, kPointerSize));
    __ push(ip);
    __ CallRuntime(Runtime::kToFastProperties, 1);
    __ pop(r0);
    __ Drop(1);
  }
  PrepareForBailoutForId(expr->AssignmentId(), TOS_REG);
  context()->Plug(r0);
}


void FullCodeGenerator::EmitKeyedPropertyAssignment(Assignment* expr) {
  // Assignment to a property, using a keyed store IC.

  // If the assignment starts a block of assignments to the same object,
  // change to slow case to avoid the quadratic behavior of repeatedly
  // adding fast properties.
  if (expr->starts_initialization_block()) {
    __ push(result_register());
    // Receiver is now under the key and value.
    __ ldr(ip, MemOperand(sp, 2 * kPointerSize));
    __ push(ip);
    __ CallRuntime(Runtime::kToSlowProperties, 1);
    __ pop(result_register());
  }

  // Record source code position before IC call.
  SetSourcePosition(expr->position());
  __ pop(r1);  // Key.
  // Load receiver to r2. Leave a copy in the stack if needed for turning the
  // receiver into fast case.
  if (expr->ends_initialization_block()) {
    __ ldr(r2, MemOperand(sp));
  } else {
    __ pop(r2);
  }

  Handle<Code> ic = is_strict_mode()
      ? isolate()->builtins()->KeyedStoreIC_Initialize_Strict()
      : isolate()->builtins()->KeyedStoreIC_Initialize();
  __ Call(ic, RelocInfo::CODE_TARGET, expr->id());

  // If the assignment ends an initialization block, revert to fast case.
  if (expr->ends_initialization_block()) {
    __ push(r0);  // Result of assignment, saved even if not needed.
    // Receiver is under the result value.
    __ ldr(ip, MemOperand(sp, kPointerSize));
    __ push(ip);
    __ CallRuntime(Runtime::kToFastProperties, 1);
    __ pop(r0);
    __ Drop(1);
  }
  PrepareForBailoutForId(expr->AssignmentId(), TOS_REG);
  context()->Plug(r0);
}


void FullCodeGenerator::VisitProperty(Property* expr) {
  Comment cmnt(masm_, "[ Property");
  Expression* key = expr->key();

  if (key->IsPropertyName()) {
    VisitForAccumulatorValue(expr->obj());
    EmitNamedPropertyLoad(expr);
    context()->Plug(r0);
  } else {
    VisitForStackValue(expr->obj());
    VisitForAccumulatorValue(expr->key());
    __ pop(r1);
    EmitKeyedPropertyLoad(expr);
    context()->Plug(r0);
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
    __ mov(r2, Operand(name));
  }
  // Record source position for debugger.
  SetSourcePosition(expr->position());
  // Call the IC initialization code.
  Handle<Code> ic =
      isolate()->stub_cache()->ComputeCallInitialize(arg_count, mode);
  __ Call(ic, mode, expr->id());
  RecordJSReturnSite(expr);
  // Restore context register.
  __ ldr(cp, MemOperand(fp, StandardFrameConstants::kContextOffset));
  context()->Plug(r0);
}


void FullCodeGenerator::EmitKeyedCallWithIC(Call* expr,
                                            Expression* key) {
  // Load the key.
  VisitForAccumulatorValue(key);

  // Swap the name of the function and the receiver on the stack to follow
  // the calling convention for call ICs.
  __ pop(r1);
  __ push(r0);
  __ push(r1);

  // Code common for calls using the IC.
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
  __ ldr(r2, MemOperand(sp, (arg_count + 1) * kPointerSize));  // Key.
  __ Call(ic, RelocInfo::CODE_TARGET, expr->id());
  RecordJSReturnSite(expr);
  // Restore context register.
  __ ldr(cp, MemOperand(fp, StandardFrameConstants::kContextOffset));
  context()->DropAndPlug(1, r0);  // Drop the key still on the stack.
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
  __ ldr(cp, MemOperand(fp, StandardFrameConstants::kContextOffset));
  context()->DropAndPlug(1, r0);
}


void FullCodeGenerator::EmitResolvePossiblyDirectEval(ResolveEvalFlag flag,
                                                      int arg_count) {
  // Push copy of the first argument or undefined if it doesn't exist.
  if (arg_count > 0) {
    __ ldr(r1, MemOperand(sp, arg_count * kPointerSize));
  } else {
    __ LoadRoot(r1, Heap::kUndefinedValueRootIndex);
  }
  __ push(r1);

  // Push the receiver of the enclosing function and do runtime call.
  int receiver_offset = 2 + info_->scope()->num_parameters();
  __ ldr(r1, MemOperand(fp, receiver_offset * kPointerSize));
  __ push(r1);
  // Push the strict mode flag. In harmony mode every eval call
  // is a strict mode eval call.
  StrictModeFlag strict_mode = strict_mode_flag();
  if (FLAG_harmony_block_scoping) {
    strict_mode = kStrictMode;
  }
  __ mov(r1, Operand(Smi::FromInt(strict_mode)));
  __ push(r1);

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
    // resolve the function we need to call and the receiver of the
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

      // If we know that eval can only be shadowed by eval-introduced
      // variables we attempt to load the global eval function directly
      // in generated code. If we succeed, there is no need to perform a
      // context lookup in the runtime system.
      Label done;
      Variable* var = proxy->var();
      if (!var->IsUnallocated() && var->mode() == Variable::DYNAMIC_GLOBAL) {
        Label slow;
        EmitLoadGlobalCheckExtensions(var, NOT_INSIDE_TYPEOF, &slow);
        // Push the function and resolve eval.
        __ push(r0);
        EmitResolvePossiblyDirectEval(SKIP_CONTEXT_LOOKUP, arg_count);
        __ jmp(&done);
        __ bind(&slow);
      }

      // Push a copy of the function (found below the arguments) and
      // resolve eval.
      __ ldr(r1, MemOperand(sp, (arg_count + 1) * kPointerSize));
      __ push(r1);
      EmitResolvePossiblyDirectEval(PERFORM_CONTEXT_LOOKUP, arg_count);
      __ bind(&done);

      // The runtime call returns a pair of values in r0 (function) and
      // r1 (receiver). Touch up the stack with the right values.
      __ str(r0, MemOperand(sp, (arg_count + 1) * kPointerSize));
      __ str(r1, MemOperand(sp, arg_count * kPointerSize));
    }

    // Record source position for debugger.
    SetSourcePosition(expr->position());
    CallFunctionStub stub(arg_count, RECEIVER_MIGHT_BE_IMPLICIT);
    __ CallStub(&stub);
    RecordJSReturnSite(expr);
    // Restore context register.
    __ ldr(cp, MemOperand(fp, StandardFrameConstants::kContextOffset));
    context()->DropAndPlug(1, r0);
  } else if (proxy != NULL && proxy->var()->IsUnallocated()) {
    // Push global object as receiver for the call IC.
    __ ldr(r0, GlobalObjectOperand());
    __ push(r0);
    EmitCallWithIC(expr, proxy->name(), RelocInfo::CODE_TARGET_CONTEXT);
  } else if (proxy != NULL && proxy->var()->IsLookupSlot()) {
    // Call to a lookup slot (dynamically introduced variable).
    Label slow, done;

    { PreservePositionScope scope(masm()->positions_recorder());
      // Generate code for loading from variables potentially shadowed
      // by eval-introduced variables.
      EmitDynamicLookupFastCase(proxy->var(), NOT_INSIDE_TYPEOF, &slow, &done);
    }

    __ bind(&slow);
    // Call the runtime to find the function to call (returned in r0)
    // and the object holding it (returned in edx).
    __ push(context_register());
    __ mov(r2, Operand(proxy->name()));
    __ push(r2);
    __ CallRuntime(Runtime::kLoadContextSlot, 2);
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
      __ LoadRoot(r1, Heap::kTheHoleValueRootIndex);
      __ push(r1);
      __ bind(&call);
    }

    // The receiver is either the global receiver or an object found
    // by LoadContextSlot. That object could be the hole if the
    // receiver is implicitly the global object.
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
    __ ldr(r1, GlobalObjectOperand());
    __ ldr(r1, FieldMemOperand(r1, GlobalObject::kGlobalReceiverOffset));
    __ push(r1);
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

  // Load function and argument count into r1 and r0.
  __ mov(r0, Operand(arg_count));
  __ ldr(r1, MemOperand(sp, arg_count * kPointerSize));

  Handle<Code> construct_builtin =
      isolate()->builtins()->JSConstructCall();
  __ Call(construct_builtin, RelocInfo::CONSTRUCT_CALL);
  context()->Plug(r0);
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
  __ tst(r0, Operand(kSmiTagMask));
  Split(eq, if_true, if_false, fall_through);

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
  __ tst(r0, Operand(kSmiTagMask | 0x80000000));
  Split(eq, if_true, if_false, fall_through);

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
  PrepareForBailoutBeforeSplit(TOS_REG, true, if_true, if_false);
  Split(le, if_true, if_false, fall_through);

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

  __ JumpIfSmi(r0, if_false);
  __ CompareObjectType(r0, r1, r1, FIRST_SPEC_OBJECT_TYPE);
  PrepareForBailoutBeforeSplit(TOS_REG, true, if_true, if_false);
  Split(ge, if_true, if_false, fall_through);

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

  __ JumpIfSmi(r0, if_false);
  __ ldr(r1, FieldMemOperand(r0, HeapObject::kMapOffset));
  __ ldrb(r1, FieldMemOperand(r1, Map::kBitFieldOffset));
  __ tst(r1, Operand(1 << Map::kIsUndetectable));
  PrepareForBailoutBeforeSplit(TOS_REG, true, if_true, if_false);
  Split(ne, if_true, if_false, fall_through);

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

  if (FLAG_debug_code) __ AbortIfSmi(r0);

  __ ldr(r1, FieldMemOperand(r0, HeapObject::kMapOffset));
  __ ldrb(ip, FieldMemOperand(r1, Map::kBitField2Offset));
  __ tst(ip, Operand(1 << Map::kStringWrapperSafeForDefaultValueOf));
  __ b(ne, if_true);

  // Check for fast case object. Generate false result for slow case object.
  __ ldr(r2, FieldMemOperand(r0, JSObject::kPropertiesOffset));
  __ ldr(r2, FieldMemOperand(r2, HeapObject::kMapOffset));
  __ LoadRoot(ip, Heap::kHashTableMapRootIndex);
  __ cmp(r2, ip);
  __ b(eq, if_false);

  // Look for valueOf symbol in the descriptor array, and indicate false if
  // found. The type is not checked, so if it is a transition it is a false
  // negative.
  __ LoadInstanceDescriptors(r1, r4);
  __ ldr(r3, FieldMemOperand(r4, FixedArray::kLengthOffset));
  // r4: descriptor array
  // r3: length of descriptor array
  // Calculate the end of the descriptor array.
  STATIC_ASSERT(kSmiTag == 0);
  STATIC_ASSERT(kSmiTagSize == 1);
  STATIC_ASSERT(kPointerSize == 4);
  __ add(r2, r4, Operand(FixedArray::kHeaderSize - kHeapObjectTag));
  __ add(r2, r2, Operand(r3, LSL, kPointerSizeLog2 - kSmiTagSize));

  // Calculate location of the first key name.
  __ add(r4,
         r4,
         Operand(FixedArray::kHeaderSize - kHeapObjectTag +
                 DescriptorArray::kFirstIndex * kPointerSize));
  // Loop through all the keys in the descriptor array. If one of these is the
  // symbol valueOf the result is false.
  Label entry, loop;
  // The use of ip to store the valueOf symbol asumes that it is not otherwise
  // used in the loop below.
  __ mov(ip, Operand(FACTORY->value_of_symbol()));
  __ jmp(&entry);
  __ bind(&loop);
  __ ldr(r3, MemOperand(r4, 0));
  __ cmp(r3, ip);
  __ b(eq, if_false);
  __ add(r4, r4, Operand(kPointerSize));
  __ bind(&entry);
  __ cmp(r4, Operand(r2));
  __ b(ne, &loop);

  // If a valueOf property is not found on the object check that it's
  // prototype is the un-modified String prototype. If not result is false.
  __ ldr(r2, FieldMemOperand(r1, Map::kPrototypeOffset));
  __ JumpIfSmi(r2, if_false);
  __ ldr(r2, FieldMemOperand(r2, HeapObject::kMapOffset));
  __ ldr(r3, ContextOperand(cp, Context::GLOBAL_INDEX));
  __ ldr(r3, FieldMemOperand(r3, GlobalObject::kGlobalContextOffset));
  __ ldr(r3, ContextOperand(r3, Context::STRING_FUNCTION_PROTOTYPE_MAP_INDEX));
  __ cmp(r2, r3);
  __ b(ne, if_false);

  // Set the bit in the map to indicate that it has been checked safe for
  // default valueOf and set true result.
  __ ldrb(r2, FieldMemOperand(r1, Map::kBitField2Offset));
  __ orr(r2, r2, Operand(1 << Map::kStringWrapperSafeForDefaultValueOf));
  __ strb(r2, FieldMemOperand(r1, Map::kBitField2Offset));
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

  __ JumpIfSmi(r0, if_false);
  __ CompareObjectType(r0, r1, r2, JS_FUNCTION_TYPE);
  PrepareForBailoutBeforeSplit(TOS_REG, true, if_true, if_false);
  Split(eq, if_true, if_false, fall_through);

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

  __ JumpIfSmi(r0, if_false);
  __ CompareObjectType(r0, r1, r1, JS_ARRAY_TYPE);
  PrepareForBailoutBeforeSplit(TOS_REG, true, if_true, if_false);
  Split(eq, if_true, if_false, fall_through);

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

  __ JumpIfSmi(r0, if_false);
  __ CompareObjectType(r0, r1, r1, JS_REGEXP_TYPE);
  PrepareForBailoutBeforeSplit(TOS_REG, true, if_true, if_false);
  Split(eq, if_true, if_false, fall_through);

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
  __ ldr(r2, MemOperand(fp, StandardFrameConstants::kCallerFPOffset));

  // Skip the arguments adaptor frame if it exists.
  Label check_frame_marker;
  __ ldr(r1, MemOperand(r2, StandardFrameConstants::kContextOffset));
  __ cmp(r1, Operand(Smi::FromInt(StackFrame::ARGUMENTS_ADAPTOR)));
  __ b(ne, &check_frame_marker);
  __ ldr(r2, MemOperand(r2, StandardFrameConstants::kCallerFPOffset));

  // Check the marker in the calling frame.
  __ bind(&check_frame_marker);
  __ ldr(r1, MemOperand(r2, StandardFrameConstants::kMarkerOffset));
  __ cmp(r1, Operand(Smi::FromInt(StackFrame::CONSTRUCT)));
  PrepareForBailoutBeforeSplit(TOS_REG, true, if_true, if_false);
  Split(eq, if_true, if_false, fall_through);

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

  __ pop(r1);
  __ cmp(r0, r1);
  PrepareForBailoutBeforeSplit(TOS_REG, true, if_true, if_false);
  Split(eq, if_true, if_false, fall_through);

  context()->Plug(if_true, if_false);
}


void FullCodeGenerator::EmitArguments(ZoneList<Expression*>* args) {
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


void FullCodeGenerator::EmitArgumentsLength(ZoneList<Expression*>* args) {
  ASSERT(args->length() == 0);

  Label exit;
  // Get the number of formal parameters.
  __ mov(r0, Operand(Smi::FromInt(info_->scope()->num_parameters())));

  // Check if the calling frame is an arguments adaptor frame.
  __ ldr(r2, MemOperand(fp, StandardFrameConstants::kCallerFPOffset));
  __ ldr(r3, MemOperand(r2, StandardFrameConstants::kContextOffset));
  __ cmp(r3, Operand(Smi::FromInt(StackFrame::ARGUMENTS_ADAPTOR)));
  __ b(ne, &exit);

  // Arguments adaptor case: Read the arguments length from the
  // adaptor frame.
  __ ldr(r0, MemOperand(r2, ArgumentsAdaptorFrameConstants::kLengthOffset));

  __ bind(&exit);
  context()->Plug(r0);
}


void FullCodeGenerator::EmitClassOf(ZoneList<Expression*>* args) {
  ASSERT(args->length() == 1);
  Label done, null, function, non_function_constructor;

  VisitForAccumulatorValue(args->at(0));

  // If the object is a smi, we return null.
  __ JumpIfSmi(r0, &null);

  // Check that the object is a JS object but take special care of JS
  // functions to make sure they have 'Function' as their class.
  __ CompareObjectType(r0, r0, r1, FIRST_SPEC_OBJECT_TYPE);
  // Map is now in r0.
  __ b(lt, &null);

  // As long as LAST_CALLABLE_SPEC_OBJECT_TYPE is the last instance type, and
  // FIRST_CALLABLE_SPEC_OBJECT_TYPE comes right after
  // LAST_NONCALLABLE_SPEC_OBJECT_TYPE, we can avoid checking for the latter.
  STATIC_ASSERT(LAST_TYPE == LAST_CALLABLE_SPEC_OBJECT_TYPE);
  STATIC_ASSERT(FIRST_CALLABLE_SPEC_OBJECT_TYPE ==
                LAST_NONCALLABLE_SPEC_OBJECT_TYPE + 1);
  __ cmp(r1, Operand(FIRST_CALLABLE_SPEC_OBJECT_TYPE));
  __ b(ge, &function);

  // Check if the constructor in the map is a function.
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
  __ LoadRoot(r0, Heap::kfunction_class_symbolRootIndex);
  __ jmp(&done);

  // Objects with a non-function constructor have class 'Object'.
  __ bind(&non_function_constructor);
  __ LoadRoot(r0, Heap::kObject_symbolRootIndex);
  __ jmp(&done);

  // Non-JS objects have class null.
  __ bind(&null);
  __ LoadRoot(r0, Heap::kNullValueRootIndex);

  // All done.
  __ bind(&done);

  context()->Plug(r0);
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
  }

  // Finally, we're expected to leave a value on the top of the stack.
  __ LoadRoot(r0, Heap::kUndefinedValueRootIndex);
  context()->Plug(r0);
}


void FullCodeGenerator::EmitRandomHeapNumber(ZoneList<Expression*>* args) {
  ASSERT(args->length() == 0);

  Label slow_allocate_heapnumber;
  Label heapnumber_allocated;

  __ LoadRoot(r6, Heap::kHeapNumberMapRootIndex);
  __ AllocateHeapNumber(r4, r1, r2, r6, &slow_allocate_heapnumber);
  __ jmp(&heapnumber_allocated);

  __ bind(&slow_allocate_heapnumber);
  // Allocate a heap number.
  __ CallRuntime(Runtime::kNumberAlloc, 0);
  __ mov(r4, Operand(r0));

  __ bind(&heapnumber_allocated);

  // Convert 32 random bits in r0 to 0.(32 random bits) in a double
  // by computing:
  // ( 1.(20 0s)(32 random bits) x 2^20 ) - (1.0 x 2^20)).
  if (CpuFeatures::IsSupported(VFP3)) {
    __ PrepareCallCFunction(1, r0);
    __ mov(r0, Operand(ExternalReference::isolate_address()));
    __ CallCFunction(ExternalReference::random_uint32_function(isolate()), 1);

    CpuFeatures::Scope scope(VFP3);
    // 0x41300000 is the top half of 1.0 x 2^20 as a double.
    // Create this constant using mov/orr to avoid PC relative load.
    __ mov(r1, Operand(0x41000000));
    __ orr(r1, r1, Operand(0x300000));
    // Move 0x41300000xxxxxxxx (x = random bits) to VFP.
    __ vmov(d7, r0, r1);
    // Move 0x4130000000000000 to VFP.
    __ mov(r0, Operand(0, RelocInfo::NONE));
    __ vmov(d8, r0, r1);
    // Subtract and store the result in the heap number.
    __ vsub(d7, d7, d8);
    __ sub(r0, r4, Operand(kHeapObjectTag));
    __ vstr(d7, r0, HeapNumber::kValueOffset);
    __ mov(r0, r4);
  } else {
    __ PrepareCallCFunction(2, r0);
    __ mov(r0, Operand(r4));
    __ mov(r1, Operand(ExternalReference::isolate_address()));
    __ CallCFunction(
        ExternalReference::fill_heap_number_with_random_function(isolate()), 2);
  }

  context()->Plug(r0);
}


void FullCodeGenerator::EmitSubString(ZoneList<Expression*>* args) {
  // Load the arguments on the stack and call the stub.
  SubStringStub stub;
  ASSERT(args->length() == 3);
  VisitForStackValue(args->at(0));
  VisitForStackValue(args->at(1));
  VisitForStackValue(args->at(2));
  __ CallStub(&stub);
  context()->Plug(r0);
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
  context()->Plug(r0);
}


void FullCodeGenerator::EmitValueOf(ZoneList<Expression*>* args) {
  ASSERT(args->length() == 1);

  VisitForAccumulatorValue(args->at(0));  // Load the object.

  Label done;
  // If the object is a smi return the object.
  __ JumpIfSmi(r0, &done);
  // If the object is not a value type, return the object.
  __ CompareObjectType(r0, r1, r1, JS_VALUE_TYPE);
  __ b(ne, &done);
  __ ldr(r0, FieldMemOperand(r0, JSValue::kValueOffset));

  __ bind(&done);
  context()->Plug(r0);
}


void FullCodeGenerator::EmitMathPow(ZoneList<Expression*>* args) {
  // Load the arguments on the stack and call the runtime function.
  ASSERT(args->length() == 2);
  VisitForStackValue(args->at(0));
  VisitForStackValue(args->at(1));
  MathPowStub stub;
  __ CallStub(&stub);
  context()->Plug(r0);
}


void FullCodeGenerator::EmitSetValueOf(ZoneList<Expression*>* args) {
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
  __ RecordWrite(r1, Operand(JSValue::kValueOffset - kHeapObjectTag), r2, r3);

  __ bind(&done);
  context()->Plug(r0);
}


void FullCodeGenerator::EmitNumberToString(ZoneList<Expression*>* args) {
  ASSERT_EQ(args->length(), 1);

  // Load the argument on the stack and call the stub.
  VisitForStackValue(args->at(0));

  NumberToStringStub stub;
  __ CallStub(&stub);
  context()->Plug(r0);
}


void FullCodeGenerator::EmitStringCharFromCode(ZoneList<Expression*>* args) {
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


void FullCodeGenerator::EmitStringCharCodeAt(ZoneList<Expression*>* args) {
  ASSERT(args->length() == 2);

  VisitForStackValue(args->at(0));
  VisitForAccumulatorValue(args->at(1));

  Register object = r1;
  Register index = r0;
  Register scratch = r2;
  Register result = r3;

  __ pop(object);

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


void FullCodeGenerator::EmitStringCharAt(ZoneList<Expression*>* args) {
  ASSERT(args->length() == 2);

  VisitForStackValue(args->at(0));
  VisitForAccumulatorValue(args->at(1));

  Register object = r1;
  Register index = r0;
  Register scratch1 = r2;
  Register scratch2 = r3;
  Register result = r0;

  __ pop(object);

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
  __ LoadRoot(result, Heap::kEmptyStringRootIndex);
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


void FullCodeGenerator::EmitStringAdd(ZoneList<Expression*>* args) {
  ASSERT_EQ(2, args->length());

  VisitForStackValue(args->at(0));
  VisitForStackValue(args->at(1));

  StringAddStub stub(NO_STRING_ADD_FLAGS);
  __ CallStub(&stub);
  context()->Plug(r0);
}


void FullCodeGenerator::EmitStringCompare(ZoneList<Expression*>* args) {
  ASSERT_EQ(2, args->length());

  VisitForStackValue(args->at(0));
  VisitForStackValue(args->at(1));

  StringCompareStub stub;
  __ CallStub(&stub);
  context()->Plug(r0);
}


void FullCodeGenerator::EmitMathSin(ZoneList<Expression*>* args) {
  // Load the argument on the stack and call the stub.
  TranscendentalCacheStub stub(TranscendentalCache::SIN,
                               TranscendentalCacheStub::TAGGED);
  ASSERT(args->length() == 1);
  VisitForStackValue(args->at(0));
  __ CallStub(&stub);
  context()->Plug(r0);
}


void FullCodeGenerator::EmitMathCos(ZoneList<Expression*>* args) {
  // Load the argument on the stack and call the stub.
  TranscendentalCacheStub stub(TranscendentalCache::COS,
                               TranscendentalCacheStub::TAGGED);
  ASSERT(args->length() == 1);
  VisitForStackValue(args->at(0));
  __ CallStub(&stub);
  context()->Plug(r0);
}


void FullCodeGenerator::EmitMathLog(ZoneList<Expression*>* args) {
  // Load the argument on the stack and call the stub.
  TranscendentalCacheStub stub(TranscendentalCache::LOG,
                               TranscendentalCacheStub::TAGGED);
  ASSERT(args->length() == 1);
  VisitForStackValue(args->at(0));
  __ CallStub(&stub);
  context()->Plug(r0);
}


void FullCodeGenerator::EmitMathSqrt(ZoneList<Expression*>* args) {
  // Load the argument on the stack and call the runtime function.
  ASSERT(args->length() == 1);
  VisitForStackValue(args->at(0));
  __ CallRuntime(Runtime::kMath_sqrt, 1);
  context()->Plug(r0);
}


void FullCodeGenerator::EmitCallFunction(ZoneList<Expression*>* args) {
  ASSERT(args->length() >= 2);

  int arg_count = args->length() - 2;  // 2 ~ receiver and function.
  for (int i = 0; i < arg_count + 1; i++) {
    VisitForStackValue(args->at(i));
  }
  VisitForAccumulatorValue(args->last());  // Function.

  // InvokeFunction requires the function in r1. Move it in there.
  __ mov(r1, result_register());
  ParameterCount count(arg_count);
  __ InvokeFunction(r1, count, CALL_FUNCTION,
                    NullCallWrapper(), CALL_AS_METHOD);
  __ ldr(cp, MemOperand(fp, StandardFrameConstants::kContextOffset));
  context()->Plug(r0);
}


void FullCodeGenerator::EmitRegExpConstructResult(ZoneList<Expression*>* args) {
  RegExpConstructResultStub stub;
  ASSERT(args->length() == 3);
  VisitForStackValue(args->at(0));
  VisitForStackValue(args->at(1));
  VisitForStackValue(args->at(2));
  __ CallStub(&stub);
  context()->Plug(r0);
}


void FullCodeGenerator::EmitSwapElements(ZoneList<Expression*>* args) {
  ASSERT(args->length() == 3);
  VisitForStackValue(args->at(0));
  VisitForStackValue(args->at(1));
  VisitForStackValue(args->at(2));
  Label done;
  Label slow_case;
  Register object = r0;
  Register index1 = r1;
  Register index2 = r2;
  Register elements = r3;
  Register scratch1 = r4;
  Register scratch2 = r5;

  __ ldr(object, MemOperand(sp, 2 * kPointerSize));
  // Fetch the map and check if array is in fast case.
  // Check that object doesn't require security checks and
  // has no indexed interceptor.
  __ CompareObjectType(object, scratch1, scratch2, JS_ARRAY_TYPE);
  __ b(ne, &slow_case);
  // Map is now in scratch1.

  __ ldrb(scratch2, FieldMemOperand(scratch1, Map::kBitFieldOffset));
  __ tst(scratch2, Operand(KeyedLoadIC::kSlowCaseBitFieldMask));
  __ b(ne, &slow_case);

  // Check the object's elements are in fast case and writable.
  __ ldr(elements, FieldMemOperand(object, JSObject::kElementsOffset));
  __ ldr(scratch1, FieldMemOperand(elements, HeapObject::kMapOffset));
  __ LoadRoot(ip, Heap::kFixedArrayMapRootIndex);
  __ cmp(scratch1, ip);
  __ b(ne, &slow_case);

  // Check that both indices are smis.
  __ ldr(index1, MemOperand(sp, 1 * kPointerSize));
  __ ldr(index2, MemOperand(sp, 0));
  __ JumpIfNotBothSmi(index1, index2, &slow_case);

  // Check that both indices are valid.
  __ ldr(scratch1, FieldMemOperand(object, JSArray::kLengthOffset));
  __ cmp(scratch1, index1);
  __ cmp(scratch1, index2, hi);
  __ b(ls, &slow_case);

  // Bring the address of the elements into index1 and index2.
  __ add(scratch1, elements, Operand(FixedArray::kHeaderSize - kHeapObjectTag));
  __ add(index1,
         scratch1,
         Operand(index1, LSL, kPointerSizeLog2 - kSmiTagSize));
  __ add(index2,
         scratch1,
         Operand(index2, LSL, kPointerSizeLog2 - kSmiTagSize));

  // Swap elements.
  __ ldr(scratch1, MemOperand(index1, 0));
  __ ldr(scratch2, MemOperand(index2, 0));
  __ str(scratch1, MemOperand(index2, 0));
  __ str(scratch2, MemOperand(index1, 0));

  Label new_space;
  __ InNewSpace(elements, scratch1, eq, &new_space);
  // Possible optimization: do a check that both values are Smis
  // (or them and test against Smi mask.)

  __ mov(scratch1, elements);
  __ RecordWriteHelper(elements, index1, scratch2);
  __ RecordWriteHelper(scratch1, index2, scratch2);  // scratch1 holds elements.

  __ bind(&new_space);
  // We are done. Drop elements from the stack, and return undefined.
  __ Drop(3);
  __ LoadRoot(r0, Heap::kUndefinedValueRootIndex);
  __ jmp(&done);

  __ bind(&slow_case);
  __ CallRuntime(Runtime::kSwapElements, 3);

  __ bind(&done);
  context()->Plug(r0);
}


void FullCodeGenerator::EmitGetFromCache(ZoneList<Expression*>* args) {
  ASSERT_EQ(2, args->length());

  ASSERT_NE(NULL, args->at(0)->AsLiteral());
  int cache_id = Smi::cast(*(args->at(0)->AsLiteral()->handle()))->value();

  Handle<FixedArray> jsfunction_result_caches(
      isolate()->global_context()->jsfunction_result_caches());
  if (jsfunction_result_caches->length() <= cache_id) {
    __ Abort("Attempt to use undefined cache.");
    __ LoadRoot(r0, Heap::kUndefinedValueRootIndex);
    context()->Plug(r0);
    return;
  }

  VisitForAccumulatorValue(args->at(1));

  Register key = r0;
  Register cache = r1;
  __ ldr(cache, ContextOperand(cp, Context::GLOBAL_INDEX));
  __ ldr(cache, FieldMemOperand(cache, GlobalObject::kGlobalContextOffset));
  __ ldr(cache, ContextOperand(cache, Context::JSFUNCTION_RESULT_CACHES_INDEX));
  __ ldr(cache,
         FieldMemOperand(cache, FixedArray::OffsetOfElementAt(cache_id)));


  Label done, not_found;
  // tmp now holds finger offset as a smi.
  STATIC_ASSERT(kSmiTag == 0 && kSmiTagSize == 1);
  __ ldr(r2, FieldMemOperand(cache, JSFunctionResultCache::kFingerOffset));
  // r2 now holds finger offset as a smi.
  __ add(r3, cache, Operand(FixedArray::kHeaderSize - kHeapObjectTag));
  // r3 now points to the start of fixed array elements.
  __ ldr(r2, MemOperand(r3, r2, LSL, kPointerSizeLog2 - kSmiTagSize, PreIndex));
  // Note side effect of PreIndex: r3 now points to the key of the pair.
  __ cmp(key, r2);
  __ b(ne, &not_found);

  __ ldr(r0, MemOperand(r3, kPointerSize));
  __ b(&done);

  __ bind(&not_found);
  // Call runtime to perform the lookup.
  __ Push(cache, key);
  __ CallRuntime(Runtime::kGetFromCache, 2);

  __ bind(&done);
  context()->Plug(r0);
}


void FullCodeGenerator::EmitIsRegExpEquivalent(ZoneList<Expression*>* args) {
  ASSERT_EQ(2, args->length());

  Register right = r0;
  Register left = r1;
  Register tmp = r2;
  Register tmp2 = r3;

  VisitForStackValue(args->at(0));
  VisitForAccumulatorValue(args->at(1));
  __ pop(left);

  Label done, fail, ok;
  __ cmp(left, Operand(right));
  __ b(eq, &ok);
  // Fail if either is a non-HeapObject.
  __ and_(tmp, left, Operand(right));
  __ JumpIfSmi(tmp, &fail);
  __ ldr(tmp, FieldMemOperand(left, HeapObject::kMapOffset));
  __ ldrb(tmp2, FieldMemOperand(tmp, Map::kInstanceTypeOffset));
  __ cmp(tmp2, Operand(JS_REGEXP_TYPE));
  __ b(ne, &fail);
  __ ldr(tmp2, FieldMemOperand(right, HeapObject::kMapOffset));
  __ cmp(tmp, Operand(tmp2));
  __ b(ne, &fail);
  __ ldr(tmp, FieldMemOperand(left, JSRegExp::kDataOffset));
  __ ldr(tmp2, FieldMemOperand(right, JSRegExp::kDataOffset));
  __ cmp(tmp, tmp2);
  __ b(eq, &ok);
  __ bind(&fail);
  __ LoadRoot(r0, Heap::kFalseValueRootIndex);
  __ jmp(&done);
  __ bind(&ok);
  __ LoadRoot(r0, Heap::kTrueValueRootIndex);
  __ bind(&done);

  context()->Plug(r0);
}


void FullCodeGenerator::EmitHasCachedArrayIndex(ZoneList<Expression*>* args) {
  VisitForAccumulatorValue(args->at(0));

  Label materialize_true, materialize_false;
  Label* if_true = NULL;
  Label* if_false = NULL;
  Label* fall_through = NULL;
  context()->PrepareTest(&materialize_true, &materialize_false,
                         &if_true, &if_false, &fall_through);

  __ ldr(r0, FieldMemOperand(r0, String::kHashFieldOffset));
  __ tst(r0, Operand(String::kContainsCachedArrayIndexMask));
  PrepareForBailoutBeforeSplit(TOS_REG, true, if_true, if_false);
  Split(eq, if_true, if_false, fall_through);

  context()->Plug(if_true, if_false);
}


void FullCodeGenerator::EmitGetCachedArrayIndex(ZoneList<Expression*>* args) {
  ASSERT(args->length() == 1);
  VisitForAccumulatorValue(args->at(0));

  if (FLAG_debug_code) {
    __ AbortIfNotString(r0);
  }

  __ ldr(r0, FieldMemOperand(r0, String::kHashFieldOffset));
  __ IndexFromHash(r0, r0);

  context()->Plug(r0);
}


void FullCodeGenerator::EmitFastAsciiArrayJoin(ZoneList<Expression*>* args) {
  Label bailout, done, one_char_separator, long_separator,
      non_trivial_array, not_size_one_array, loop,
      empty_separator_loop, one_char_separator_loop,
      one_char_separator_loop_entry, long_separator_loop;

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
  Register scratch1 = r7;
  Register scratch2 = r9;

  // Separator operand is on the stack.
  __ pop(separator);

  // Check that the array is a JSArray.
  __ JumpIfSmi(array, &bailout);
  __ CompareObjectType(array, scratch1, scratch2, JS_ARRAY_TYPE);
  __ b(ne, &bailout);

  // Check that the array has fast elements.
  __ CheckFastElements(scratch1, scratch2, &bailout);

  // If the array has length zero, return the empty string.
  __ ldr(array_length, FieldMemOperand(array, JSArray::kLengthOffset));
  __ SmiUntag(array_length, SetCC);
  __ b(ne, &non_trivial_array);
  __ LoadRoot(r0, Heap::kEmptyStringRootIndex);
  __ b(&done);

  __ bind(&non_trivial_array);

  // Get the FixedArray containing array's elements.
  elements = array;
  __ ldr(elements, FieldMemOperand(array, JSArray::kElementsOffset));
  array = no_reg;  // End of array's live range.

  // Check that all array elements are sequential ASCII strings, and
  // accumulate the sum of their lengths, as a smi-encoded value.
  __ mov(string_length, Operand(0));
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
  if (FLAG_debug_code) {
    __ cmp(array_length, Operand(0));
    __ Assert(gt, "No empty arrays here in EmitFastAsciiArrayJoin");
  }
  __ bind(&loop);
  __ ldr(string, MemOperand(element, kPointerSize, PostIndex));
  __ JumpIfSmi(string, &bailout);
  __ ldr(scratch1, FieldMemOperand(string, HeapObject::kMapOffset));
  __ ldrb(scratch1, FieldMemOperand(scratch1, Map::kInstanceTypeOffset));
  __ JumpIfInstanceTypeIsNotSequentialAscii(scratch1, scratch2, &bailout);
  __ ldr(scratch1, FieldMemOperand(string, SeqAsciiString::kLengthOffset));
  __ add(string_length, string_length, Operand(scratch1));
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
  __ ldr(scratch1, FieldMemOperand(separator, HeapObject::kMapOffset));
  __ ldrb(scratch1, FieldMemOperand(scratch1, Map::kInstanceTypeOffset));
  __ JumpIfInstanceTypeIsNotSequentialAscii(scratch1, scratch2, &bailout);

  // Add (separator length times array_length) - separator length to the
  // string_length to get the length of the result string. array_length is not
  // smi but the other values are, so the result is a smi
  __ ldr(scratch1, FieldMemOperand(separator, SeqAsciiString::kLengthOffset));
  __ sub(string_length, string_length, Operand(scratch1));
  __ smull(scratch2, ip, array_length, scratch1);
  // Check for smi overflow. No overflow if higher 33 bits of 64-bit result are
  // zero.
  __ cmp(ip, Operand(0));
  __ b(ne, &bailout);
  __ tst(scratch2, Operand(0x80000000));
  __ b(ne, &bailout);
  __ add(string_length, string_length, Operand(scratch2));
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
                         scratch1,
                         scratch2,
                         elements_end,
                         &bailout);
  // Prepare for looping. Set up elements_end to end of the array. Set
  // result_pos to the position of the result where to write the first
  // character.
  __ add(elements_end, element, Operand(array_length, LSL, kPointerSizeLog2));
  result_pos = array_length;  // End of live range for array_length.
  array_length = no_reg;
  __ add(result_pos,
         result,
         Operand(SeqAsciiString::kHeaderSize - kHeapObjectTag));

  // Check the length of the separator.
  __ ldr(scratch1, FieldMemOperand(separator, SeqAsciiString::kLengthOffset));
  __ cmp(scratch1, Operand(Smi::FromInt(1)));
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
  __ add(string, string, Operand(SeqAsciiString::kHeaderSize - kHeapObjectTag));
  __ CopyBytes(string, result_pos, string_length, scratch1);
  __ cmp(element, elements_end);
  __ b(lt, &empty_separator_loop);  // End while (element < elements_end).
  ASSERT(result.is(r0));
  __ b(&done);

  // One-character separator case
  __ bind(&one_char_separator);
  // Replace separator with its ascii character value.
  __ ldrb(separator, FieldMemOperand(separator, SeqAsciiString::kHeaderSize));
  // Jump into the loop after the code that copies the separator, so the first
  // element is not preceded by a separator
  __ jmp(&one_char_separator_loop_entry);

  __ bind(&one_char_separator_loop);
  // Live values in registers:
  //   result_pos: the position to which we are currently copying characters.
  //   element: Current array element.
  //   elements_end: Array end.
  //   separator: Single separator ascii char (in lower byte).

  // Copy the separator character to the result.
  __ strb(separator, MemOperand(result_pos, 1, PostIndex));

  // Copy next array element to the result.
  __ bind(&one_char_separator_loop_entry);
  __ ldr(string, MemOperand(element, kPointerSize, PostIndex));
  __ ldr(string_length, FieldMemOperand(string, String::kLengthOffset));
  __ SmiUntag(string_length);
  __ add(string, string, Operand(SeqAsciiString::kHeaderSize - kHeapObjectTag));
  __ CopyBytes(string, result_pos, string_length, scratch1);
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
         Operand(SeqAsciiString::kHeaderSize - kHeapObjectTag));
  __ CopyBytes(string, result_pos, string_length, scratch1);

  __ bind(&long_separator);
  __ ldr(string, MemOperand(element, kPointerSize, PostIndex));
  __ ldr(string_length, FieldMemOperand(string, String::kLengthOffset));
  __ SmiUntag(string_length);
  __ add(string, string, Operand(SeqAsciiString::kHeaderSize - kHeapObjectTag));
  __ CopyBytes(string, result_pos, string_length, scratch1);
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
    __ ldr(r0, GlobalObjectOperand());
    __ ldr(r0, FieldMemOperand(r0, GlobalObject::kBuiltinsOffset));
    __ push(r0);
  }

  // Push the arguments ("left-to-right").
  int arg_count = args->length();
  for (int i = 0; i < arg_count; i++) {
    VisitForStackValue(args->at(i));
  }

  if (expr->is_jsruntime()) {
    // Call the JS runtime function.
    __ mov(r2, Operand(expr->name()));
    RelocInfo::Mode mode = RelocInfo::CODE_TARGET;
    Handle<Code> ic =
        isolate()->stub_cache()->ComputeCallInitialize(arg_count, mode);
    __ Call(ic, mode, expr->id());
    // Restore context register.
    __ ldr(cp, MemOperand(fp, StandardFrameConstants::kContextOffset));
  } else {
    // Call the C runtime function.
    __ CallRuntime(expr->function(), arg_count);
  }
  context()->Plug(r0);
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
        __ mov(r1, Operand(Smi::FromInt(strict_mode_flag())));
        __ push(r1);
        __ InvokeBuiltin(Builtins::DELETE, CALL_FUNCTION);
        context()->Plug(r0);
      } else if (proxy != NULL) {
        Variable* var = proxy->var();
        // Delete of an unqualified identifier is disallowed in strict mode
        // but "delete this" is allowed.
        ASSERT(strict_mode_flag() == kNonStrictMode || var->is_this());
        if (var->IsUnallocated()) {
          __ ldr(r2, GlobalObjectOperand());
          __ mov(r1, Operand(var->name()));
          __ mov(r0, Operand(Smi::FromInt(kNonStrictMode)));
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
          __ push(context_register());
          __ mov(r2, Operand(var->name()));
          __ push(r2);
          __ CallRuntime(Runtime::kDeleteContextSlot, 2);
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
      context()->Plug(r0);
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
  // accumulator register r0.
  VisitForAccumulatorValue(expr->expression());
  SetSourcePosition(expr->position());
  __ Call(stub.GetCode(), RelocInfo::CODE_TARGET, expr->id());
  context()->Plug(r0);
}


void FullCodeGenerator::VisitCountOperation(CountOperation* expr) {
  Comment cmnt(masm_, "[ CountOperation");
  SetSourcePosition(expr->position());

  // Invalid left-hand sides are rewritten to have a 'throw ReferenceError'
  // as the left-hand side.
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
    PrepareForBailoutForId(expr->CountId(), TOS_REG);
  }

  // Call ToNumber only if operand is not a smi.
  Label no_conversion;
  __ JumpIfSmi(r0, &no_conversion);
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


  // Inline smi case if we are in a loop.
  Label stub_call, done;
  JumpPatchSite patch_site(masm_);

  int count_value = expr->op() == Token::INC ? 1 : -1;
  if (ShouldInlineSmiCase(expr->op())) {
    __ add(r0, r0, Operand(Smi::FromInt(count_value)), SetCC);
    __ b(vs, &stub_call);
    // We could eliminate this smi check if we split the code at
    // the first smi check before calling ToNumber.
    patch_site.EmitJumpIfSmi(r0, &done);

    __ bind(&stub_call);
    // Call stub. Undo operation first.
    __ sub(r0, r0, Operand(Smi::FromInt(count_value)));
  }
  __ mov(r1, Operand(Smi::FromInt(count_value)));

  // Record position before stub call.
  SetSourcePosition(expr->position());

  BinaryOpStub stub(Token::ADD, NO_OVERWRITE);
  __ Call(stub.GetCode(), RelocInfo::CODE_TARGET, expr->CountId());
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
      __ mov(r2, Operand(prop->key()->AsLiteral()->handle()));
      __ pop(r1);
      Handle<Code> ic = is_strict_mode()
          ? isolate()->builtins()->StoreIC_Initialize_Strict()
          : isolate()->builtins()->StoreIC_Initialize();
      __ Call(ic, RelocInfo::CODE_TARGET, expr->id());
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
      __ pop(r1);  // Key.
      __ pop(r2);  // Receiver.
      Handle<Code> ic = is_strict_mode()
          ? isolate()->builtins()->KeyedStoreIC_Initialize_Strict()
          : isolate()->builtins()->KeyedStoreIC_Initialize();
      __ Call(ic, RelocInfo::CODE_TARGET, expr->id());
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
    Comment cmnt(masm_, "Global variable");
    __ ldr(r0, GlobalObjectOperand());
    __ mov(r2, Operand(proxy->name()));
    Handle<Code> ic = isolate()->builtins()->LoadIC_Initialize();
    // Use a regular load, not a contextual load, to avoid a reference
    // error.
    __ Call(ic);
    PrepareForBailout(expr, TOS_REG);
    context()->Plug(r0);
  } else if (proxy != NULL && proxy->var()->IsLookupSlot()) {
    Label done, slow;

    // Generate code for loading from variables potentially shadowed
    // by eval-introduced variables.
    EmitDynamicLookupFastCase(proxy->var(), INSIDE_TYPEOF, &slow, &done);

    __ bind(&slow);
    __ mov(r0, Operand(proxy->name()));
    __ Push(cp, r0);
    __ CallRuntime(Runtime::kLoadContextSlotNoReferenceError, 2);
    PrepareForBailout(expr, TOS_REG);
    __ bind(&done);

    context()->Plug(r0);
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
    __ JumpIfSmi(r0, if_true);
    __ ldr(r0, FieldMemOperand(r0, HeapObject::kMapOffset));
    __ LoadRoot(ip, Heap::kHeapNumberMapRootIndex);
    __ cmp(r0, ip);
    Split(eq, if_true, if_false, fall_through);
  } else if (check->Equals(isolate()->heap()->string_symbol())) {
    __ JumpIfSmi(r0, if_false);
    // Check for undetectable objects => false.
    __ CompareObjectType(r0, r0, r1, FIRST_NONSTRING_TYPE);
    __ b(ge, if_false);
    __ ldrb(r1, FieldMemOperand(r0, Map::kBitFieldOffset));
    __ tst(r1, Operand(1 << Map::kIsUndetectable));
    Split(eq, if_true, if_false, fall_through);
  } else if (check->Equals(isolate()->heap()->boolean_symbol())) {
    __ CompareRoot(r0, Heap::kTrueValueRootIndex);
    __ b(eq, if_true);
    __ CompareRoot(r0, Heap::kFalseValueRootIndex);
    Split(eq, if_true, if_false, fall_through);
  } else if (FLAG_harmony_typeof &&
             check->Equals(isolate()->heap()->null_symbol())) {
    __ CompareRoot(r0, Heap::kNullValueRootIndex);
    Split(eq, if_true, if_false, fall_through);
  } else if (check->Equals(isolate()->heap()->undefined_symbol())) {
    __ CompareRoot(r0, Heap::kUndefinedValueRootIndex);
    __ b(eq, if_true);
    __ JumpIfSmi(r0, if_false);
    // Check for undetectable objects => true.
    __ ldr(r0, FieldMemOperand(r0, HeapObject::kMapOffset));
    __ ldrb(r1, FieldMemOperand(r0, Map::kBitFieldOffset));
    __ tst(r1, Operand(1 << Map::kIsUndetectable));
    Split(ne, if_true, if_false, fall_through);

  } else if (check->Equals(isolate()->heap()->function_symbol())) {
    __ JumpIfSmi(r0, if_false);
    __ CompareObjectType(r0, r1, r0, FIRST_CALLABLE_SPEC_OBJECT_TYPE);
    Split(ge, if_true, if_false, fall_through);

  } else if (check->Equals(isolate()->heap()->object_symbol())) {
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
}


void FullCodeGenerator::EmitLiteralCompareUndefined(Expression* expr,
                                                    Label* if_true,
                                                    Label* if_false,
                                                    Label* fall_through) {
  VisitForAccumulatorValue(expr);
  PrepareForBailoutBeforeSplit(TOS_REG, true, if_true, if_false);

  __ CompareRoot(r0, Heap::kUndefinedValueRootIndex);
  Split(eq, if_true, if_false, fall_through);
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
  switch (op) {
    case Token::IN:
      VisitForStackValue(expr->right());
      __ InvokeBuiltin(Builtins::IN, CALL_FUNCTION);
      PrepareForBailoutBeforeSplit(TOS_REG, false, NULL, NULL);
      __ LoadRoot(ip, Heap::kTrueValueRootIndex);
      __ cmp(r0, ip);
      Split(eq, if_true, if_false, fall_through);
      break;

    case Token::INSTANCEOF: {
      VisitForStackValue(expr->right());
      InstanceofStub stub(InstanceofStub::kNoFlags);
      __ CallStub(&stub);
      PrepareForBailoutBeforeSplit(TOS_REG, true, if_true, if_false);
      // The stub returns 0 for true.
      __ tst(r0, r0);
      Split(eq, if_true, if_false, fall_through);
      break;
    }

    default: {
      VisitForAccumulatorValue(expr->right());
      Condition cond = eq;
      switch (op) {
        case Token::EQ_STRICT:
        case Token::EQ:
          cond = eq;
          __ pop(r1);
          break;
        case Token::LT:
          cond = lt;
          __ pop(r1);
          break;
        case Token::GT:
          // Reverse left and right sides to obtain ECMA-262 conversion order.
          cond = lt;
          __ mov(r1, result_register());
          __ pop(r0);
         break;
        case Token::LTE:
          // Reverse left and right sides to obtain ECMA-262 conversion order.
          cond = ge;
          __ mov(r1, result_register());
          __ pop(r0);
          break;
        case Token::GTE:
          cond = ge;
          __ pop(r1);
          break;
        case Token::IN:
        case Token::INSTANCEOF:
        default:
          UNREACHABLE();
      }

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
      Handle<Code> ic = CompareIC::GetUninitialized(op);
      __ Call(ic, RelocInfo::CODE_TARGET, expr->id());
      patch_site.EmitPatchInfo();
      PrepareForBailoutBeforeSplit(TOS_REG, true, if_true, if_false);
      __ cmp(r0, Operand(0));
      Split(cond, if_true, if_false, fall_through);
    }
  }

  // Convert the result of the comparison into one expected for this
  // expression's context.
  context()->Plug(if_true, if_false);
}


void FullCodeGenerator::VisitCompareToNull(CompareToNull* expr) {
  Comment cmnt(masm_, "[ CompareToNull");
  Label materialize_true, materialize_false;
  Label* if_true = NULL;
  Label* if_false = NULL;
  Label* fall_through = NULL;
  context()->PrepareTest(&materialize_true, &materialize_false,
                         &if_true, &if_false, &fall_through);

  VisitForAccumulatorValue(expr->expression());
  PrepareForBailoutBeforeSplit(TOS_REG, true, if_true, if_false);
  __ LoadRoot(r1, Heap::kNullValueRootIndex);
  __ cmp(r0, r1);
  if (expr->is_strict()) {
    Split(eq, if_true, if_false, fall_through);
  } else {
    __ b(eq, if_true);
    __ LoadRoot(r1, Heap::kUndefinedValueRootIndex);
    __ cmp(r0, r1);
    __ b(eq, if_true);
    __ JumpIfSmi(r0, if_false);
    // It can be an undetectable object.
    __ ldr(r1, FieldMemOperand(r0, HeapObject::kMapOffset));
    __ ldrb(r1, FieldMemOperand(r1, Map::kBitFieldOffset));
    __ and_(r1, r1, Operand(1 << Map::kIsUndetectable));
    __ cmp(r1, Operand(1 << Map::kIsUndetectable));
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
  ASSERT_EQ(POINTER_SIZE_ALIGN(frame_offset), frame_offset);
  __ str(value, MemOperand(fp, frame_offset));
}


void FullCodeGenerator::LoadContextField(Register dst, int context_index) {
  __ ldr(dst, ContextOperand(cp, context_index));
}


void FullCodeGenerator::PushFunctionArgumentForContextAllocation() {
  Scope* declaration_scope = scope()->DeclarationScope();
  if (declaration_scope->is_global_scope()) {
    // Contexts nested in the global context have a canonical empty function
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
  ASSERT_EQ(1, kSmiTagSize + kSmiShiftSize);
  STATIC_ASSERT(kSmiTag == 0);
  __ add(r1, r1, Operand(r1));  // Convert to smi.
  __ push(r1);
}


void FullCodeGenerator::ExitFinallyBlock() {
  ASSERT(!result_register().is(r1));
  // Restore result register from stack.
  __ pop(r1);
  // Uncook return address and return.
  __ pop(result_register());
  ASSERT_EQ(1, kSmiTagSize + kSmiShiftSize);
  __ mov(r1, Operand(r1, ASR, 1));  // Un-smi-tag value.
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

} }  // namespace v8::internal

#endif  // V8_TARGET_ARCH_ARM
