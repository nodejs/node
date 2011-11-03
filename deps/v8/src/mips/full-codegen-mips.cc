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

#if defined(V8_TARGET_ARCH_MIPS)

// Note on Mips implementation:
//
// The result_register() for mips is the 'v0' register, which is defined
// by the ABI to contain function return values. However, the first
// parameter to a function is defined to be 'a0'. So there are many
// places where we have to move a previous result in v0 to a0 for the
// next call: mov(a0, v0). This is not needed on the other architectures.

#include "code-stubs.h"
#include "codegen.h"
#include "compiler.h"
#include "debug.h"
#include "full-codegen.h"
#include "parser.h"
#include "scopes.h"
#include "stub-cache.h"

#include "mips/code-stubs-mips.h"

namespace v8 {
namespace internal {

#define __ ACCESS_MASM(masm_)


static unsigned GetPropertyId(Property* property) {
  return property->id();
}


// A patch site is a location in the code which it is possible to patch. This
// class has a number of methods to emit the code which is patchable and the
// method EmitPatchInfo to record a marker back to the patchable code. This
// marker is a andi at, rx, #yyy instruction, and x * 0x0000ffff + yyy (raw 16
// bit immediate value is used) is the delta from the pc to the first
// instruction of the patchable code.
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
    Assembler::BlockTrampolinePoolScope block_trampoline_pool(masm_);
    __ bind(&patch_site_);
    __ andi(at, reg, 0);
    // Always taken before patched.
    __ Branch(target, eq, at, Operand(zero_reg));
  }

  // When initially emitting this ensure that a jump is never generated to skip
  // the inlined smi code.
  void EmitJumpIfSmi(Register reg, Label* target) {
    Assembler::BlockTrampolinePoolScope block_trampoline_pool(masm_);
    ASSERT(!patch_site_.is_bound() && !info_emitted_);
    __ bind(&patch_site_);
    __ andi(at, reg, 0);
    // Never taken before patched.
    __ Branch(target, ne, at, Operand(zero_reg));
  }

  void EmitPatchInfo() {
    if (patch_site_.is_bound()) {
      int delta_to_patch_site = masm_->InstructionsGeneratedSince(&patch_site_);
      Register reg = Register::from_code(delta_to_patch_site / kImm16Mask);
      __ andi(at, reg, delta_to_patch_site % kImm16Mask);
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
//   o a1: the JS function object being called (ie, ourselves)
//   o cp: our context
//   o fp: our caller's frame pointer
//   o sp: stack pointer
//   o ra: return address
//
// The function builds a JS frame.  Please see JavaScriptFrameConstants in
// frames-mips.h for its layout.
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
  // receiver object). t1 is zero for method calls and non-zero for
  // function calls.
  if (info->is_strict_mode() || info->is_native()) {
    Label ok;
    __ Branch(&ok, eq, t1, Operand(zero_reg));
    int receiver_offset = info->scope()->num_parameters() * kPointerSize;
    __ LoadRoot(a2, Heap::kUndefinedValueRootIndex);
    __ sw(a2, MemOperand(sp, receiver_offset));
    __ bind(&ok);
  }

  int locals_count = info->scope()->num_stack_slots();

  __ Push(ra, fp, cp, a1);
  if (locals_count > 0) {
    // Load undefined value here, so the value is ready for the loop
    // below.
    __ LoadRoot(at, Heap::kUndefinedValueRootIndex);
  }
  // Adjust fp to point to caller's fp.
  __ Addu(fp, sp, Operand(2 * kPointerSize));

  { Comment cmnt(masm_, "[ Allocate locals");
    for (int i = 0; i < locals_count; i++) {
      __ push(at);
    }
  }

  bool function_in_register = true;

  // Possibly allocate a local context.
  int heap_slots = info->scope()->num_heap_slots() - Context::MIN_CONTEXT_SLOTS;
  if (heap_slots > 0) {
    Comment cmnt(masm_, "[ Allocate local context");
    // Argument to NewContext is the function, which is in a1.
    __ push(a1);
    if (heap_slots <= FastNewContextStub::kMaximumSlots) {
      FastNewContextStub stub(heap_slots);
      __ CallStub(&stub);
    } else {
      __ CallRuntime(Runtime::kNewFunctionContext, 1);
    }
    function_in_register = false;
    // Context is returned in both v0 and cp.  It replaces the context
    // passed to us.  It's saved in the stack and kept live in cp.
    __ sw(cp, MemOperand(fp, StandardFrameConstants::kContextOffset));
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
        __ li(a1, Operand(Context::SlotOffset(var->index())));
        __ addu(a2, cp, a1);
        __ sw(a0, MemOperand(a2, 0));
        // Update the write barrier. This clobbers all involved
        // registers, so we have to use two more registers to avoid
        // clobbering cp.
        __ mov(a2, cp);
        __ RecordWrite(a2, a1, a3);
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
    if (is_strict_mode()) {
      type = ArgumentsAccessStub::NEW_STRICT;
    } else if (function()->has_duplicate_parameters()) {
      type = ArgumentsAccessStub::NEW_NON_STRICT_SLOW;
    } else {
      type = ArgumentsAccessStub::NEW_NON_STRICT_FAST;
    }
    ArgumentsAccessStub stub(type);
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
      __ LoadRoot(t0, Heap::kStackLimitRootIndex);
      __ Branch(&ok, hs, sp, Operand(t0));
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
    __ LoadRoot(v0, Heap::kUndefinedValueRootIndex);
  }
  EmitReturnSequence();
}


void FullCodeGenerator::ClearAccumulator() {
  ASSERT(Smi::FromInt(0) == 0);
  __ mov(v0, zero_reg);
}


void FullCodeGenerator::EmitStackCheck(IterationStatement* stmt) {
  Comment cmnt(masm_, "[ Stack check");
  Label ok;
  __ LoadRoot(t0, Heap::kStackLimitRootIndex);
  __ Branch(&ok, hs, sp, Operand(t0));
  StackCheckStub stub;
  // Record a mapping of this PC offset to the OSR id.  This is used to find
  // the AST id from the unoptimized code in order to use it as a key into
  // the deoptimization input data found in the optimized code.
  RecordStackCheck(stmt->OsrEntryId());

  __ CallStub(&stub);
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
      masm_->MultiPop(static_cast<RegList>(fp.bit() | ra.bit()));
      masm_->Addu(sp, sp, Operand(sp_delta));
      masm_->Jump(ra);
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
  codegen()->PrepareForBailoutBeforeSplit(TOS_REG,
                                          true,
                                          true_label_,
                                          false_label_);
  ASSERT(!lit->IsUndetectableObject());  // There are no undetectable literals.
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
  __ sw(reg, MemOperand(sp, 0));
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
  __ push(at);
  __ Branch(&done);
  __ bind(materialize_false);
  __ LoadRoot(at, Heap::kFalseValueRootIndex);
  __ push(at);
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
  __ LoadRoot(at, value_root_index);
  __ push(at);
}


void FullCodeGenerator::TestContext::Plug(bool flag) const {
  codegen()->PrepareForBailoutBeforeSplit(TOS_REG,
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
  if (CpuFeatures::IsSupported(FPU)) {
    ToBooleanStub stub(result_register());
    __ CallStub(&stub);
    __ mov(at, zero_reg);
  } else {
    // Call the runtime to find the boolean value of the source and then
    // translate it into control flow to the pair of labels.
    __ push(result_register());
    __ CallRuntime(Runtime::kToBool, 1);
    __ LoadRoot(at, Heap::kFalseValueRootIndex);
  }
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
  __ lw(dest, location);
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
  __ sw(src, location);
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
  if (should_normalize) __ Branch(&skip);

  ForwardBailoutStack* current = forward_bailout_stack_;
  while (current != NULL) {
    PrepareForBailout(current->expr(), state);
    current = current->parent();
  }

  if (should_normalize) {
    __ LoadRoot(t0, Heap::kTrueValueRootIndex);
    Split(eq, a0, Operand(t0), if_true, if_false, NULL);
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
        __ sw(result_register(), StackOperand(variable));
      } else if (mode == Variable::CONST || mode == Variable::LET) {
          Comment cmnt(masm_, "[ Declaration");
          __ LoadRoot(t0, Heap::kTheHoleValueRootIndex);
          __ sw(t0, StackOperand(variable));
      }
      break;

      case Variable::CONTEXT:
      // The variable in the decl always resides in the current function
      // context.
      ASSERT_EQ(0, scope()->ContextChainLength(variable->scope()));
      if (FLAG_debug_code) {
        // Check that we're not inside a with or catch context.
        __ lw(a1, FieldMemOperand(cp, HeapObject::kMapOffset));
        __ LoadRoot(t0, Heap::kWithContextMapRootIndex);
        __ Check(ne, "Declaration in with context.",
                 a1, Operand(t0));
        __ LoadRoot(t0, Heap::kCatchContextMapRootIndex);
        __ Check(ne, "Declaration in catch context.",
                 a1, Operand(t0));
      }
      if (function != NULL) {
        Comment cmnt(masm_, "[ Declaration");
        VisitForAccumulatorValue(function);
        __ sw(result_register(), ContextOperand(cp, variable->index()));
        int offset = Context::SlotOffset(variable->index());
        // We know that we have written a function, which is not a smi.
        __ mov(a1, cp);
        __ RecordWrite(a1, Operand(offset), a2, result_register());
        PrepareForBailoutForId(proxy->id(), NO_REGISTERS);
      } else if (mode == Variable::CONST || mode == Variable::LET) {
          Comment cmnt(masm_, "[ Declaration");
          __ LoadRoot(at, Heap::kTheHoleValueRootIndex);
          __ sw(at, ContextOperand(cp, variable->index()));
          // No write barrier since the_hole_value is in old space.
          PrepareForBailoutForId(proxy->id(), NO_REGISTERS);
      }
      break;

    case Variable::LOOKUP: {
      Comment cmnt(masm_, "[ Declaration");
      __ li(a2, Operand(variable->name()));
      // Declaration nodes are always introduced in one of three modes.
      ASSERT(mode == Variable::VAR ||
             mode == Variable::CONST ||
             mode == Variable::LET);
      PropertyAttributes attr = (mode == Variable::CONST) ? READ_ONLY : NONE;
      __ li(a1, Operand(Smi::FromInt(attr)));
      // Push initial value, if any.
      // Note: For variables we must not push an initial value (such as
      // 'undefined') because we may have a (legal) redeclaration and we
      // must not destroy the current value.
      if (function != NULL) {
        __ Push(cp, a2, a1);
        // Push initial value for function declaration.
        VisitForStackValue(function);
      } else if (mode == Variable::CONST || mode == Variable::LET) {
          __ LoadRoot(a0, Heap::kTheHoleValueRootIndex);
          __ Push(cp, a2, a1, a0);
      } else {
        ASSERT(Smi::FromInt(0) == 0);
        __ mov(a0, zero_reg);  // Smi::FromInt(0) indicates no initial value.
        __ Push(cp, a2, a1, a0);
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
  __ li(a1, Operand(pairs));
  __ li(a0, Operand(Smi::FromInt(DeclareGlobalsFlags())));
  __ Push(cp, a1, a0);
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
    Handle<Code> ic = CompareIC::GetUninitialized(Token::EQ_STRICT);
    __ Call(ic, RelocInfo::CODE_TARGET, clause->CompareId());
    patch_site.EmitPatchInfo();

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
  SetStatementPosition(stmt);

  Label loop, exit;
  ForIn loop_statement(this, stmt);
  increment_loop_depth();

  // Get the object to enumerate over. Both SpiderMonkey and JSC
  // ignore null and undefined in contrast to the specification; see
  // ECMA-262 section 12.6.4.
  VisitForAccumulatorValue(stmt->enumerable());
  __ mov(a0, result_register());  // Result as param to InvokeBuiltin below.
  __ LoadRoot(at, Heap::kUndefinedValueRootIndex);
  __ Branch(&exit, eq, a0, Operand(at));
  Register null_value = t1;
  __ LoadRoot(null_value, Heap::kNullValueRootIndex);
  __ Branch(&exit, eq, a0, Operand(null_value));

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

  // Check cache validity in generated code. This is a fast case for
  // the JSObject::IsSimpleEnum cache validity checks. If we cannot
  // guarantee cache validity, call the runtime system to check cache
  // validity or get the property names in a fixed array.
  Label next, call_runtime;
  // Preload a couple of values used in the loop.
  Register  empty_fixed_array_value = t2;
  __ LoadRoot(empty_fixed_array_value, Heap::kEmptyFixedArrayRootIndex);
  Register empty_descriptor_array_value = t3;
  __ LoadRoot(empty_descriptor_array_value,
              Heap::kEmptyDescriptorArrayRootIndex);
  __ mov(a1, a0);
  __ bind(&next);

  // Check that there are no elements.  Register a1 contains the
  // current JS object we've reached through the prototype chain.
  __ lw(a2, FieldMemOperand(a1, JSObject::kElementsOffset));
  __ Branch(&call_runtime, ne, a2, Operand(empty_fixed_array_value));

  // Check that instance descriptors are not empty so that we can
  // check for an enum cache.  Leave the map in a2 for the subsequent
  // prototype load.
  __ lw(a2, FieldMemOperand(a1, HeapObject::kMapOffset));
  __ lw(a3, FieldMemOperand(a2, Map::kInstanceDescriptorsOrBitField3Offset));
  __ JumpIfSmi(a3, &call_runtime);

  // Check that there is an enum cache in the non-empty instance
  // descriptors (a3).  This is the case if the next enumeration
  // index field does not contain a smi.
  __ lw(a3, FieldMemOperand(a3, DescriptorArray::kEnumerationIndexOffset));
  __ JumpIfSmi(a3, &call_runtime);

  // For all objects but the receiver, check that the cache is empty.
  Label check_prototype;
  __ Branch(&check_prototype, eq, a1, Operand(a0));
  __ lw(a3, FieldMemOperand(a3, DescriptorArray::kEnumCacheBridgeCacheOffset));
  __ Branch(&call_runtime, ne, a3, Operand(empty_fixed_array_value));

  // Load the prototype from the map and loop if non-null.
  __ bind(&check_prototype);
  __ lw(a1, FieldMemOperand(a2, Map::kPrototypeOffset));
  __ Branch(&next, ne, a1, Operand(null_value));

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
  __ mov(a2, v0);
  __ lw(a1, FieldMemOperand(a2, HeapObject::kMapOffset));
  __ LoadRoot(at, Heap::kMetaMapRootIndex);
  __ Branch(&fixed_array, ne, a1, Operand(at));

  // We got a map in register v0. Get the enumeration cache from it.
  __ bind(&use_cache);
  __ LoadInstanceDescriptors(v0, a1);
  __ lw(a1, FieldMemOperand(a1, DescriptorArray::kEnumerationIndexOffset));
  __ lw(a2, FieldMemOperand(a1, DescriptorArray::kEnumCacheBridgeCacheOffset));

  // Setup the four remaining stack slots.
  __ push(v0);  // Map.
  __ lw(a1, FieldMemOperand(a2, FixedArray::kLengthOffset));
  __ li(a0, Operand(Smi::FromInt(0)));
  // Push enumeration cache, enumeration cache length (as smi) and zero.
  __ Push(a2, a1, a0);
  __ jmp(&loop);

  // We got a fixed array in register v0. Iterate through that.
  __ bind(&fixed_array);
  __ li(a1, Operand(Smi::FromInt(0)));  // Map (0) - force slow check.
  __ Push(a1, v0);
  __ lw(a1, FieldMemOperand(v0, FixedArray::kLengthOffset));
  __ li(a0, Operand(Smi::FromInt(0)));
  __ Push(a1, a0);  // Fixed array length (as smi) and initial index.

  // Generate code for doing the condition check.
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

  // Get the expected map from the stack or a zero map in the
  // permanent slow case into register a2.
  __ lw(a2, MemOperand(sp, 3 * kPointerSize));

  // Check if the expected map still matches that of the enumerable.
  // If not, we have to filter the key.
  Label update_each;
  __ lw(a1, MemOperand(sp, 4 * kPointerSize));
  __ lw(t0, FieldMemOperand(a1, HeapObject::kMapOffset));
  __ Branch(&update_each, eq, t0, Operand(a2));

  // Convert the entry to a string or (smi) 0 if it isn't a property
  // any more. If the property has been removed while iterating, we
  // just skip it.
  __ push(a1);  // Enumerable.
  __ push(a3);  // Current entry.
  __ InvokeBuiltin(Builtins::FILTER_KEY, CALL_FUNCTION);
  __ mov(a3, result_register());
  __ Branch(loop_statement.continue_label(), eq, a3, Operand(zero_reg));

  // Update the 'each' property or variable from the possibly filtered
  // entry in register a3.
  __ bind(&update_each);
  __ mov(result_register(), a3);
  // Perform the assignment as if via '='.
  { EffectContext context(this);
    EmitAssignment(stmt->each(), stmt->AssignmentId());
  }

  // Generate code for the body of the loop.
  Visit(stmt->body());

  // Generate code for the going to the next element by incrementing
  // the index (smi) stored on top of the stack.
  __ bind(loop_statement.continue_label());
  __ pop(a0);
  __ Addu(a0, a0, Operand(Smi::FromInt(1)));
  __ push(a0);

  EmitStackCheck(stmt);
  __ Branch(&loop);

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
    __ li(a0, Operand(info));
    __ push(a0);
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


void FullCodeGenerator::EmitLoadGlobalCheckExtensions(Variable* var,
                                                      TypeofState typeof_state,
                                                      Label* slow) {
  Register current = cp;
  Register next = a1;
  Register temp = a2;

  Scope* s = scope();
  while (s != NULL) {
    if (s->num_heap_slots() > 0) {
      if (s->calls_eval()) {
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
    __ lw(temp, FieldMemOperand(next, HeapObject::kMapOffset));
    __ LoadRoot(t0, Heap::kGlobalContextMapRootIndex);
    __ Branch(&fast, eq, temp, Operand(t0));
    // Check that extension is NULL.
    __ lw(temp, ContextOperand(next, Context::EXTENSION_INDEX));
    __ Branch(slow, ne, temp, Operand(zero_reg));
    // Load next context in chain.
    __ lw(next, ContextOperand(next, Context::PREVIOUS_INDEX));
    __ Branch(&loop);
    __ bind(&fast);
  }

  __ lw(a0, GlobalObjectOperand());
  __ li(a2, Operand(var->name()));
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
  Register next = a3;
  Register temp = t0;

  for (Scope* s = scope(); s != var->scope(); s = s->outer_scope()) {
    if (s->num_heap_slots() > 0) {
      if (s->calls_eval()) {
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
    __ Branch(done);
  } else if (var->mode() == Variable::DYNAMIC_LOCAL) {
    Variable* local = var->local_if_not_shadowed();
    __ lw(v0, ContextSlotOperandCheckExtensions(local, slow));
    if (local->mode() == Variable::CONST) {
      __ LoadRoot(at, Heap::kTheHoleValueRootIndex);
      __ subu(at, v0, at);  // Sub as compare: at == 0 on eq.
      __ LoadRoot(a0, Heap::kUndefinedValueRootIndex);
      __ movz(v0, a0, at);  // Conditional move: return Undefined if TheHole.
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
      Comment cmnt(masm_, "Global variable");
      // Use inline caching. Variable name is passed in a2 and the global
      // object (receiver) in a0.
      __ lw(a0, GlobalObjectOperand());
      __ li(a2, Operand(var->name()));
      Handle<Code> ic = isolate()->builtins()->LoadIC_Initialize();
      __ Call(ic, RelocInfo::CODE_TARGET_CONTEXT);
      context()->Plug(v0);
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
        GetVar(v0, var);
        __ LoadRoot(at, Heap::kTheHoleValueRootIndex);
        __ subu(at, v0, at);  // Sub as compare: at == 0 on eq.
        if (var->mode() == Variable::LET) {
          Label done;
          __ Branch(&done, ne, at, Operand(zero_reg));
          __ li(a0, Operand(var->name()));
          __ push(a0);
          __ CallRuntime(Runtime::kThrowReferenceError, 1);
          __ bind(&done);
        } else {
          __ LoadRoot(a0, Heap::kUndefinedValueRootIndex);
          __ movz(v0, a0, at);  // Conditional move: Undefined if TheHole.
        }
        context()->Plug(v0);
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
      __ li(a1, Operand(var->name()));
      __ Push(cp, a1);  // Context and name.
      __ CallRuntime(Runtime::kLoadContextSlot, 2);
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
  __ AllocateInNewSpace(size, v0, a2, a3, &runtime_allocate, TAG_OBJECT);
  __ jmp(&allocated);

  __ bind(&runtime_allocate);
  __ push(t1);
  __ li(a0, Operand(Smi::FromInt(size)));
  __ push(a0);
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


void FullCodeGenerator::VisitObjectLiteral(ObjectLiteral* expr) {
  Comment cmnt(masm_, "[ ObjectLiteral");
  __ lw(a3, MemOperand(fp,  JavaScriptFrameConstants::kFunctionOffset));
  __ lw(a3, FieldMemOperand(a3, JSFunction::kLiteralsOffset));
  __ li(a2, Operand(Smi::FromInt(expr->literal_index())));
  __ li(a1, Operand(expr->constant_properties()));
  int flags = expr->fast_elements()
      ? ObjectLiteral::kFastElements
      : ObjectLiteral::kNoFlags;
  flags |= expr->has_function()
      ? ObjectLiteral::kHasFunction
      : ObjectLiteral::kNoFlags;
  __ li(a0, Operand(Smi::FromInt(flags)));
  __ Push(a3, a2, a1, a0);
  if (expr->depth() > 1) {
    __ CallRuntime(Runtime::kCreateObjectLiteral, 4);
  } else {
    __ CallRuntime(Runtime::kCreateObjectLiteralShallow, 4);
  }

  // If result_saved is true the result is on top of the stack.  If
  // result_saved is false the result is in v0.
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
      __ push(v0);  // Save result on stack.
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
            __ mov(a0, result_register());
            __ li(a2, Operand(key->handle()));
            __ lw(a1, MemOperand(sp));
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
        __ lw(a0, MemOperand(sp));
        __ push(a0);
        VisitForStackValue(key);
        VisitForStackValue(value);
        if (property->emit_store()) {
          __ li(a0, Operand(Smi::FromInt(NONE)));  // PropertyAttributes.
          __ push(a0);
          __ CallRuntime(Runtime::kSetProperty, 4);
        } else {
          __ Drop(3);
        }
        break;
      case ObjectLiteral::Property::GETTER:
      case ObjectLiteral::Property::SETTER:
        // Duplicate receiver on stack.
        __ lw(a0, MemOperand(sp));
        __ push(a0);
        VisitForStackValue(key);
        __ li(a1, Operand(property->kind() == ObjectLiteral::Property::SETTER ?
                           Smi::FromInt(1) :
                           Smi::FromInt(0)));
        __ push(a1);
        VisitForStackValue(value);
        __ CallRuntime(Runtime::kDefineAccessor, 4);
        break;
    }
  }

  if (expr->has_function()) {
    ASSERT(result_saved);
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

  ZoneList<Expression*>* subexprs = expr->values();
  int length = subexprs->length();
  __ mov(a0, result_register());
  __ lw(a3, MemOperand(fp, JavaScriptFrameConstants::kFunctionOffset));
  __ lw(a3, FieldMemOperand(a3, JSFunction::kLiteralsOffset));
  __ li(a2, Operand(Smi::FromInt(expr->literal_index())));
  __ li(a1, Operand(expr->constant_elements()));
  __ Push(a3, a2, a1);
  if (expr->constant_elements()->map() ==
      isolate()->heap()->fixed_cow_array_map()) {
    FastCloneShallowArrayStub stub(
        FastCloneShallowArrayStub::COPY_ON_WRITE_ELEMENTS, length);
    __ CallStub(&stub);
    __ IncrementCounter(isolate()->counters()->cow_arrays_created_stub(),
        1, a1, a2);
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
      __ push(v0);
      result_saved = true;
    }
    VisitForAccumulatorValue(subexpr);

    // Store the subexpression value in the array's elements.
    __ lw(a1, MemOperand(sp));  // Copy of array literal.
    __ lw(a1, FieldMemOperand(a1, JSObject::kElementsOffset));
    int offset = FixedArray::kHeaderSize + (i * kPointerSize);
    __ sw(result_register(), FieldMemOperand(a1, offset));

    // Update the write barrier for the array store with v0 as the scratch
    // register.
    __ RecordWrite(a1, Operand(offset), a2, result_register());

    PrepareForBailoutForId(expr->GetIdForElement(i), NO_REGISTERS);
  }

  if (result_saved) {
    context()->PlugTOS();
  } else {
    context()->Plug(v0);
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
      // We need the key and receiver on both the stack and in v0 and a1.
      if (expr->is_compound()) {
        VisitForStackValue(property->obj());
        VisitForAccumulatorValue(property->key());
        __ lw(a1, MemOperand(sp, 0));
        __ push(v0);
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


void FullCodeGenerator::EmitNamedPropertyLoad(Property* prop) {
  SetSourcePosition(prop->position());
  Literal* key = prop->key()->AsLiteral();
  __ mov(a0, result_register());
  __ li(a2, Operand(key->handle()));
  // Call load IC. It has arguments receiver and property name a0 and a2.
  Handle<Code> ic = isolate()->builtins()->LoadIC_Initialize();
  __ Call(ic, RelocInfo::CODE_TARGET, GetPropertyId(prop));
}


void FullCodeGenerator::EmitKeyedPropertyLoad(Property* prop) {
  SetSourcePosition(prop->position());
  __ mov(a0, result_register());
  // Call keyed load IC. It has arguments key and receiver in a0 and a1.
  Handle<Code> ic = isolate()->builtins()->KeyedLoadIC_Initialize();
  __ Call(ic, RelocInfo::CODE_TARGET, GetPropertyId(prop));
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
      __ Branch(&stub_call);
      __ GetLeastBitsFromSmi(scratch1, right, 5);
      __ srav(right, left, scratch1);
      __ And(v0, right, Operand(~kSmiTagMask));
      break;
    case Token::SHL: {
      __ Branch(&stub_call);
      __ SmiUntag(scratch1, left);
      __ GetLeastBitsFromSmi(scratch2, right, 5);
      __ sllv(scratch1, scratch1, scratch2);
      __ Addu(scratch2, scratch1, Operand(0x40000000));
      __ Branch(&stub_call, lt, scratch2, Operand(zero_reg));
      __ SmiTag(v0, scratch1);
      break;
    }
    case Token::SHR: {
      __ Branch(&stub_call);
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
      ASSERT(Smi::FromInt(0) == 0);
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
  BinaryOpStub stub(op, mode);
  JumpPatchSite patch_site(masm_);    // unbound, signals no inlined smi code.
  __ Call(stub.GetCode(), RelocInfo::CODE_TARGET, expr->id());
  patch_site.EmitPatchInfo();
  context()->Plug(v0);
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
      __ push(result_register());  // Preserve value.
      VisitForAccumulatorValue(prop->obj());
      __ mov(a1, result_register());
      __ pop(a0);  // Restore value.
      __ li(a2, Operand(prop->key()->AsLiteral()->handle()));
      Handle<Code> ic = is_strict_mode()
          ? isolate()->builtins()->StoreIC_Initialize_Strict()
          : isolate()->builtins()->StoreIC_Initialize();
      __ Call(ic);
      break;
    }
    case KEYED_PROPERTY: {
      __ push(result_register());  // Preserve value.
      VisitForStackValue(prop->obj());
      VisitForAccumulatorValue(prop->key());
      __ mov(a1, result_register());
      __ pop(a2);
      __ pop(a0);  // Restore value.
      Handle<Code> ic = is_strict_mode()
        ? isolate()->builtins()->KeyedStoreIC_Initialize_Strict()
        : isolate()->builtins()->KeyedStoreIC_Initialize();
      __ Call(ic);
      break;
    }
  }
  PrepareForBailoutForId(bailout_ast_id, TOS_REG);
  context()->Plug(v0);
}


void FullCodeGenerator::EmitVariableAssignment(Variable* var,
                                               Token::Value op) {
  if (var->IsUnallocated()) {
    // Global var, const, or let.
    __ mov(a0, result_register());
    __ li(a2, Operand(var->name()));
    __ lw(a1, GlobalObjectOperand());
    Handle<Code> ic = is_strict_mode()
        ? isolate()->builtins()->StoreIC_Initialize_Strict()
        : isolate()->builtins()->StoreIC_Initialize();
    __ Call(ic, RelocInfo::CODE_TARGET_CONTEXT);

  } else if (op == Token::INIT_CONST) {
    // Const initializers need a write barrier.
    ASSERT(!var->IsParameter());  // No const parameters.
    if (var->IsStackLocal()) {
      Label skip;
      __ lw(a1, StackOperand(var));
      __ LoadRoot(t0, Heap::kTheHoleValueRootIndex);
      __ Branch(&skip, ne, a1, Operand(t0));
      __ sw(result_register(), StackOperand(var));
      __ bind(&skip);
    } else {
      ASSERT(var->IsContextSlot() || var->IsLookupSlot());
      // Like var declarations, const declarations are hoisted to function
      // scope.  However, unlike var initializers, const initializers are
      // able to drill a hole to that function context, even from inside a
      // 'with' context.  We thus bypass the normal static scope lookup for
      // var->IsContextSlot().
      __ push(v0);
      __ li(a0, Operand(var->name()));
      __ Push(cp, a0);  // Context and name.
      __ CallRuntime(Runtime::kInitializeConstContextSlot, 3);
    }

  } else if (var->mode() == Variable::LET && op != Token::INIT_LET) {
    // Non-initializing assignment to let variable needs a write barrier.
    if (var->IsLookupSlot()) {
      __ push(v0);  // Value.
      __ li(a1, Operand(var->name()));
      __ li(a0, Operand(Smi::FromInt(strict_mode_flag())));
      __ Push(cp, a1, a0);  // Context, name, strict mode.
      __ CallRuntime(Runtime::kStoreContextSlot, 4);
    } else {
      ASSERT(var->IsStackAllocated() || var->IsContextSlot());
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
      __ sw(result_register(), location);
      if (var->IsContextSlot()) {
        // RecordWrite may destroy all its register arguments.
        __ mov(a3, result_register());
        int offset = Context::SlotOffset(var->index());
        __ RecordWrite(a1, Operand(offset), a2, a3);
      }
    }

  } else if (var->mode() != Variable::CONST) {
    // Assignment to var or initializing assignment to let.
    if (var->IsStackAllocated() || var->IsContextSlot()) {
      MemOperand location = VarOperand(var, a1);
      if (FLAG_debug_code && op == Token::INIT_LET) {
        // Check for an uninitialized let binding.
        __ lw(a2, location);
        __ LoadRoot(t0, Heap::kTheHoleValueRootIndex);
        __ Check(eq, "Let binding re-initialization.", a2, Operand(t0));
      }
      // Perform the assignment.
      __ sw(v0, location);
      if (var->IsContextSlot()) {
        __ mov(a3, v0);
        __ RecordWrite(a1, Operand(Context::SlotOffset(var->index())), a2, a3);
      }
    } else {
      ASSERT(var->IsLookupSlot());
      __ push(v0);  // Value.
      __ li(a1, Operand(var->name()));
      __ li(a0, Operand(Smi::FromInt(strict_mode_flag())));
      __ Push(cp, a1, a0);  // Context, name, strict mode.
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
    __ lw(t0, MemOperand(sp, kPointerSize));  // Receiver is now under value.
    __ push(t0);
    __ CallRuntime(Runtime::kToSlowProperties, 1);
    __ pop(result_register());
  }

  // Record source code position before IC call.
  SetSourcePosition(expr->position());
  __ mov(a0, result_register());  // Load the value.
  __ li(a2, Operand(prop->key()->AsLiteral()->handle()));
  // Load receiver to a1. Leave a copy in the stack if needed for turning the
  // receiver into fast case.
  if (expr->ends_initialization_block()) {
    __ lw(a1, MemOperand(sp));
  } else {
    __ pop(a1);
  }

  Handle<Code> ic = is_strict_mode()
        ? isolate()->builtins()->StoreIC_Initialize_Strict()
        : isolate()->builtins()->StoreIC_Initialize();
  __ Call(ic, RelocInfo::CODE_TARGET, expr->id());

  // If the assignment ends an initialization block, revert to fast case.
  if (expr->ends_initialization_block()) {
    __ push(v0);  // Result of assignment, saved even if not needed.
    // Receiver is under the result value.
    __ lw(t0, MemOperand(sp, kPointerSize));
    __ push(t0);
    __ CallRuntime(Runtime::kToFastProperties, 1);
    __ pop(v0);
    __ Drop(1);
  }
  PrepareForBailoutForId(expr->AssignmentId(), TOS_REG);
  context()->Plug(v0);
}


void FullCodeGenerator::EmitKeyedPropertyAssignment(Assignment* expr) {
  // Assignment to a property, using a keyed store IC.

  // If the assignment starts a block of assignments to the same object,
  // change to slow case to avoid the quadratic behavior of repeatedly
  // adding fast properties.
  if (expr->starts_initialization_block()) {
    __ push(result_register());
    // Receiver is now under the key and value.
    __ lw(t0, MemOperand(sp, 2 * kPointerSize));
    __ push(t0);
    __ CallRuntime(Runtime::kToSlowProperties, 1);
    __ pop(result_register());
  }

  // Record source code position before IC call.
  SetSourcePosition(expr->position());
  // Call keyed store IC.
  // The arguments are:
  // - a0 is the value,
  // - a1 is the key,
  // - a2 is the receiver.
  __ mov(a0, result_register());
  __ pop(a1);  // Key.
  // Load receiver to a2. Leave a copy in the stack if needed for turning the
  // receiver into fast case.
  if (expr->ends_initialization_block()) {
    __ lw(a2, MemOperand(sp));
  } else {
    __ pop(a2);
  }

  Handle<Code> ic = is_strict_mode()
      ? isolate()->builtins()->KeyedStoreIC_Initialize_Strict()
      : isolate()->builtins()->KeyedStoreIC_Initialize();
  __ Call(ic, RelocInfo::CODE_TARGET, expr->id());

  // If the assignment ends an initialization block, revert to fast case.
  if (expr->ends_initialization_block()) {
    __ push(v0);  // Result of assignment, saved even if not needed.
    // Receiver is under the result value.
    __ lw(t0, MemOperand(sp, kPointerSize));
    __ push(t0);
    __ CallRuntime(Runtime::kToFastProperties, 1);
    __ pop(v0);
    __ Drop(1);
  }
  PrepareForBailoutForId(expr->AssignmentId(), TOS_REG);
  context()->Plug(v0);
}


void FullCodeGenerator::VisitProperty(Property* expr) {
  Comment cmnt(masm_, "[ Property");
  Expression* key = expr->key();

  if (key->IsPropertyName()) {
    VisitForAccumulatorValue(expr->obj());
    EmitNamedPropertyLoad(expr);
    context()->Plug(v0);
  } else {
    VisitForStackValue(expr->obj());
    VisitForAccumulatorValue(expr->key());
    __ pop(a1);
    EmitKeyedPropertyLoad(expr);
    context()->Plug(v0);
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
    __ li(a2, Operand(name));
  }
  // Record source position for debugger.
  SetSourcePosition(expr->position());
  // Call the IC initialization code.
  Handle<Code> ic =
      isolate()->stub_cache()->ComputeCallInitialize(arg_count, mode);
  __ Call(ic, mode, expr->id());
  RecordJSReturnSite(expr);
  // Restore context register.
  __ lw(cp, MemOperand(fp, StandardFrameConstants::kContextOffset));
  context()->Plug(v0);
}


void FullCodeGenerator::EmitKeyedCallWithIC(Call* expr,
                                            Expression* key) {
  // Load the key.
  VisitForAccumulatorValue(key);

  // Swap the name of the function and the receiver on the stack to follow
  // the calling convention for call ICs.
  __ pop(a1);
  __ push(v0);
  __ push(a1);

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
  __ lw(a2, MemOperand(sp, (arg_count + 1) * kPointerSize));  // Key.
  __ Call(ic, RelocInfo::CODE_TARGET, expr->id());
  RecordJSReturnSite(expr);
  // Restore context register.
  __ lw(cp, MemOperand(fp, StandardFrameConstants::kContextOffset));
  context()->DropAndPlug(1, v0);  // Drop the key still on the stack.
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
  __ lw(cp, MemOperand(fp, StandardFrameConstants::kContextOffset));
  context()->DropAndPlug(1, v0);
}


void FullCodeGenerator::EmitResolvePossiblyDirectEval(ResolveEvalFlag flag,
                                                      int arg_count) {
  // Push copy of the first argument or undefined if it doesn't exist.
  if (arg_count > 0) {
    __ lw(a1, MemOperand(sp, arg_count * kPointerSize));
  } else {
    __ LoadRoot(a1, Heap::kUndefinedValueRootIndex);
  }
  __ push(a1);

  // Push the receiver of the enclosing function and do runtime call.
  int receiver_offset = 2 + info_->scope()->num_parameters();
  __ lw(a1, MemOperand(fp, receiver_offset * kPointerSize));
  __ push(a1);
  // Push the strict mode flag. In harmony mode every eval call
  // is a strict mode eval call.
  StrictModeFlag strict_mode = strict_mode_flag();
  if (FLAG_harmony_block_scoping) {
    strict_mode = kStrictMode;
  }
  __ li(a1, Operand(Smi::FromInt(strict_mode)));
  __ push(a1);

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
      __ LoadRoot(a2, Heap::kUndefinedValueRootIndex);
      __ push(a2);  // Reserved receiver slot.

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
        __ push(v0);
        EmitResolvePossiblyDirectEval(SKIP_CONTEXT_LOOKUP, arg_count);
        __ jmp(&done);
        __ bind(&slow);
      }

      // Push a copy of the function (found below the arguments) and
      // resolve eval.
      __ lw(a1, MemOperand(sp, (arg_count + 1) * kPointerSize));
      __ push(a1);
      EmitResolvePossiblyDirectEval(PERFORM_CONTEXT_LOOKUP, arg_count);
      __ bind(&done);

      // The runtime call returns a pair of values in v0 (function) and
      // v1 (receiver). Touch up the stack with the right values.
      __ sw(v0, MemOperand(sp, (arg_count + 1) * kPointerSize));
      __ sw(v1, MemOperand(sp, arg_count * kPointerSize));
    }
    // Record source position for debugger.
    SetSourcePosition(expr->position());
    CallFunctionStub stub(arg_count, RECEIVER_MIGHT_BE_IMPLICIT);
    __ CallStub(&stub);
    RecordJSReturnSite(expr);
    // Restore context register.
    __ lw(cp, MemOperand(fp, StandardFrameConstants::kContextOffset));
    context()->DropAndPlug(1, v0);
  } else if (proxy != NULL && proxy->var()->IsUnallocated()) {
    // Push global object as receiver for the call IC.
    __ lw(a0, GlobalObjectOperand());
    __ push(a0);
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
    // Call the runtime to find the function to call (returned in v0)
    // and the object holding it (returned in v1).
    __ push(context_register());
    __ li(a2, Operand(proxy->name()));
    __ push(a2);
    __ CallRuntime(Runtime::kLoadContextSlot, 2);
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
      __ LoadRoot(a1, Heap::kTheHoleValueRootIndex);
      __ push(a1);
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
    __ lw(a1, GlobalObjectOperand());
    __ lw(a1, FieldMemOperand(a1, GlobalObject::kGlobalReceiverOffset));
    __ push(a1);
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

  // Load function and argument count into a1 and a0.
  __ li(a0, Operand(arg_count));
  __ lw(a1, MemOperand(sp, arg_count * kPointerSize));

  Handle<Code> construct_builtin =
      isolate()->builtins()->JSConstructCall();
  __ Call(construct_builtin, RelocInfo::CONSTRUCT_CALL);
  context()->Plug(v0);
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
  __ And(t0, v0, Operand(kSmiTagMask));
  Split(eq, t0, Operand(zero_reg), if_true, if_false, fall_through);

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
  __ And(at, v0, Operand(kSmiTagMask | 0x80000000));
  Split(eq, at, Operand(zero_reg), if_true, if_false, fall_through);

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
  PrepareForBailoutBeforeSplit(TOS_REG, true, if_true, if_false);
  Split(le, a1, Operand(LAST_NONCALLABLE_SPEC_OBJECT_TYPE),
        if_true, if_false, fall_through);

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

  __ JumpIfSmi(v0, if_false);
  __ GetObjectType(v0, a1, a1);
  PrepareForBailoutBeforeSplit(TOS_REG, true, if_true, if_false);
  Split(ge, a1, Operand(FIRST_SPEC_OBJECT_TYPE),
        if_true, if_false, fall_through);

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

  __ JumpIfSmi(v0, if_false);
  __ lw(a1, FieldMemOperand(v0, HeapObject::kMapOffset));
  __ lbu(a1, FieldMemOperand(a1, Map::kBitFieldOffset));
  __ And(at, a1, Operand(1 << Map::kIsUndetectable));
  PrepareForBailoutBeforeSplit(TOS_REG, true, if_true, if_false);
  Split(ne, at, Operand(zero_reg), if_true, if_false, fall_through);

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

  if (FLAG_debug_code) __ AbortIfSmi(v0);

  __ lw(a1, FieldMemOperand(v0, HeapObject::kMapOffset));
  __ lbu(t0, FieldMemOperand(a1, Map::kBitField2Offset));
  __ And(t0, t0, 1 << Map::kStringWrapperSafeForDefaultValueOf);
  __ Branch(if_true, ne, t0, Operand(zero_reg));

  // Check for fast case object. Generate false result for slow case object.
  __ lw(a2, FieldMemOperand(v0, JSObject::kPropertiesOffset));
  __ lw(a2, FieldMemOperand(a2, HeapObject::kMapOffset));
  __ LoadRoot(t0, Heap::kHashTableMapRootIndex);
  __ Branch(if_false, eq, a2, Operand(t0));

  // Look for valueOf symbol in the descriptor array, and indicate false if
  // found. The type is not checked, so if it is a transition it is a false
  // negative.
  __ LoadInstanceDescriptors(a1, t0);
  __ lw(a3, FieldMemOperand(t0, FixedArray::kLengthOffset));
  // t0: descriptor array
  // a3: length of descriptor array
  // Calculate the end of the descriptor array.
  STATIC_ASSERT(kSmiTag == 0);
  STATIC_ASSERT(kSmiTagSize == 1);
  STATIC_ASSERT(kPointerSize == 4);
  __ Addu(a2, t0, Operand(FixedArray::kHeaderSize - kHeapObjectTag));
  __ sll(t1, a3, kPointerSizeLog2 - kSmiTagSize);
  __ Addu(a2, a2, t1);

  // Calculate location of the first key name.
  __ Addu(t0,
          t0,
          Operand(FixedArray::kHeaderSize - kHeapObjectTag +
                  DescriptorArray::kFirstIndex * kPointerSize));
  // Loop through all the keys in the descriptor array. If one of these is the
  // symbol valueOf the result is false.
  Label entry, loop;
  // The use of t2 to store the valueOf symbol asumes that it is not otherwise
  // used in the loop below.
  __ li(t2, Operand(FACTORY->value_of_symbol()));
  __ jmp(&entry);
  __ bind(&loop);
  __ lw(a3, MemOperand(t0, 0));
  __ Branch(if_false, eq, a3, Operand(t2));
  __ Addu(t0, t0, Operand(kPointerSize));
  __ bind(&entry);
  __ Branch(&loop, ne, t0, Operand(a2));

  // If a valueOf property is not found on the object check that it's
  // prototype is the un-modified String prototype. If not result is false.
  __ lw(a2, FieldMemOperand(a1, Map::kPrototypeOffset));
  __ JumpIfSmi(a2, if_false);
  __ lw(a2, FieldMemOperand(a2, HeapObject::kMapOffset));
  __ lw(a3, ContextOperand(cp, Context::GLOBAL_INDEX));
  __ lw(a3, FieldMemOperand(a3, GlobalObject::kGlobalContextOffset));
  __ lw(a3, ContextOperand(a3, Context::STRING_FUNCTION_PROTOTYPE_MAP_INDEX));
  __ Branch(if_false, ne, a2, Operand(a3));

  // Set the bit in the map to indicate that it has been checked safe for
  // default valueOf and set true result.
  __ lbu(a2, FieldMemOperand(a1, Map::kBitField2Offset));
  __ Or(a2, a2, Operand(1 << Map::kStringWrapperSafeForDefaultValueOf));
  __ sb(a2, FieldMemOperand(a1, Map::kBitField2Offset));
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

  __ JumpIfSmi(v0, if_false);
  __ GetObjectType(v0, a1, a2);
  PrepareForBailoutBeforeSplit(TOS_REG, true, if_true, if_false);
  __ Branch(if_true, eq, a2, Operand(JS_FUNCTION_TYPE));
  __ Branch(if_false);

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

  __ JumpIfSmi(v0, if_false);
  __ GetObjectType(v0, a1, a1);
  PrepareForBailoutBeforeSplit(TOS_REG, true, if_true, if_false);
  Split(eq, a1, Operand(JS_ARRAY_TYPE),
        if_true, if_false, fall_through);

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

  __ JumpIfSmi(v0, if_false);
  __ GetObjectType(v0, a1, a1);
  PrepareForBailoutBeforeSplit(TOS_REG, true, if_true, if_false);
  Split(eq, a1, Operand(JS_REGEXP_TYPE), if_true, if_false, fall_through);

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
  PrepareForBailoutBeforeSplit(TOS_REG, true, if_true, if_false);
  Split(eq, a1, Operand(Smi::FromInt(StackFrame::CONSTRUCT)),
        if_true, if_false, fall_through);

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

  __ pop(a1);
  PrepareForBailoutBeforeSplit(TOS_REG, true, if_true, if_false);
  Split(eq, v0, Operand(a1), if_true, if_false, fall_through);

  context()->Plug(if_true, if_false);
}


void FullCodeGenerator::EmitArguments(ZoneList<Expression*>* args) {
  ASSERT(args->length() == 1);

  // ArgumentsAccessStub expects the key in a1 and the formal
  // parameter count in a0.
  VisitForAccumulatorValue(args->at(0));
  __ mov(a1, v0);
  __ li(a0, Operand(Smi::FromInt(info_->scope()->num_parameters())));
  ArgumentsAccessStub stub(ArgumentsAccessStub::READ_ELEMENT);
  __ CallStub(&stub);
  context()->Plug(v0);
}


void FullCodeGenerator::EmitArgumentsLength(ZoneList<Expression*>* args) {
  ASSERT(args->length() == 0);

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


void FullCodeGenerator::EmitClassOf(ZoneList<Expression*>* args) {
  ASSERT(args->length() == 1);
  Label done, null, function, non_function_constructor;

  VisitForAccumulatorValue(args->at(0));

  // If the object is a smi, we return null.
  __ JumpIfSmi(v0, &null);

  // Check that the object is a JS object but take special care of JS
  // functions to make sure they have 'Function' as their class.
  __ GetObjectType(v0, v0, a1);  // Map is now in v0.
  __ Branch(&null, lt, a1, Operand(FIRST_SPEC_OBJECT_TYPE));

  // As long as LAST_CALLABLE_SPEC_OBJECT_TYPE is the last instance type, and
  // FIRST_CALLABLE_SPEC_OBJECT_TYPE comes right after
  // LAST_NONCALLABLE_SPEC_OBJECT_TYPE, we can avoid checking for the latter.
  STATIC_ASSERT(LAST_TYPE == LAST_CALLABLE_SPEC_OBJECT_TYPE);
  STATIC_ASSERT(FIRST_CALLABLE_SPEC_OBJECT_TYPE ==
                LAST_NONCALLABLE_SPEC_OBJECT_TYPE + 1);
  __ Branch(&function, ge, a1, Operand(FIRST_CALLABLE_SPEC_OBJECT_TYPE));

  // Check if the constructor in the map is a function.
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
  __ LoadRoot(v0, Heap::kfunction_class_symbolRootIndex);
  __ jmp(&done);

  // Objects with a non-function constructor have class 'Object'.
  __ bind(&non_function_constructor);
  __ LoadRoot(v0, Heap::kObject_symbolRootIndex);
  __ jmp(&done);

  // Non-JS objects have class null.
  __ bind(&null);
  __ LoadRoot(v0, Heap::kNullValueRootIndex);

  // All done.
  __ bind(&done);

  context()->Plug(v0);
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
  __ LoadRoot(v0, Heap::kUndefinedValueRootIndex);
  context()->Plug(v0);
}


void FullCodeGenerator::EmitRandomHeapNumber(ZoneList<Expression*>* args) {
  ASSERT(args->length() == 0);

  Label slow_allocate_heapnumber;
  Label heapnumber_allocated;

  // Save the new heap number in callee-saved register s0, since
  // we call out to external C code below.
  __ LoadRoot(t6, Heap::kHeapNumberMapRootIndex);
  __ AllocateHeapNumber(s0, a1, a2, t6, &slow_allocate_heapnumber);
  __ jmp(&heapnumber_allocated);

  __ bind(&slow_allocate_heapnumber);

  // Allocate a heap number.
  __ CallRuntime(Runtime::kNumberAlloc, 0);
  __ mov(s0, v0);   // Save result in s0, so it is saved thru CFunc call.

  __ bind(&heapnumber_allocated);

  // Convert 32 random bits in v0 to 0.(32 random bits) in a double
  // by computing:
  // ( 1.(20 0s)(32 random bits) x 2^20 ) - (1.0 x 2^20)).
  if (CpuFeatures::IsSupported(FPU)) {
    __ PrepareCallCFunction(1, a0);
    __ li(a0, Operand(ExternalReference::isolate_address()));
    __ CallCFunction(ExternalReference::random_uint32_function(isolate()), 1);


    CpuFeatures::Scope scope(FPU);
    // 0x41300000 is the top half of 1.0 x 2^20 as a double.
    __ li(a1, Operand(0x41300000));
    // Move 0x41300000xxxxxxxx (x = random bits in v0) to FPU.
    __ Move(f12, v0, a1);
    // Move 0x4130000000000000 to FPU.
    __ Move(f14, zero_reg, a1);
    // Subtract and store the result in the heap number.
    __ sub_d(f0, f12, f14);
    __ sdc1(f0, MemOperand(s0, HeapNumber::kValueOffset - kHeapObjectTag));
    __ mov(v0, s0);
  } else {
    __ PrepareCallCFunction(2, a0);
    __ mov(a0, s0);
    __ li(a1, Operand(ExternalReference::isolate_address()));
    __ CallCFunction(
        ExternalReference::fill_heap_number_with_random_function(isolate()), 2);
  }

  context()->Plug(v0);
}


void FullCodeGenerator::EmitSubString(ZoneList<Expression*>* args) {
  // Load the arguments on the stack and call the stub.
  SubStringStub stub;
  ASSERT(args->length() == 3);
  VisitForStackValue(args->at(0));
  VisitForStackValue(args->at(1));
  VisitForStackValue(args->at(2));
  __ CallStub(&stub);
  context()->Plug(v0);
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
  context()->Plug(v0);
}


void FullCodeGenerator::EmitValueOf(ZoneList<Expression*>* args) {
  ASSERT(args->length() == 1);

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


void FullCodeGenerator::EmitMathPow(ZoneList<Expression*>* args) {
  // Load the arguments on the stack and call the runtime function.
  ASSERT(args->length() == 2);
  VisitForStackValue(args->at(0));
  VisitForStackValue(args->at(1));
  MathPowStub stub;
  __ CallStub(&stub);
  context()->Plug(v0);
}


void FullCodeGenerator::EmitSetValueOf(ZoneList<Expression*>* args) {
  ASSERT(args->length() == 2);

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
  __ RecordWrite(a1, Operand(JSValue::kValueOffset - kHeapObjectTag), a2, a3);

  __ bind(&done);
  context()->Plug(v0);
}


void FullCodeGenerator::EmitNumberToString(ZoneList<Expression*>* args) {
  ASSERT_EQ(args->length(), 1);

  // Load the argument on the stack and call the stub.
  VisitForStackValue(args->at(0));

  NumberToStringStub stub;
  __ CallStub(&stub);
  context()->Plug(v0);
}


void FullCodeGenerator::EmitStringCharFromCode(ZoneList<Expression*>* args) {
  ASSERT(args->length() == 1);

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


void FullCodeGenerator::EmitStringCharCodeAt(ZoneList<Expression*>* args) {
  ASSERT(args->length() == 2);

  VisitForStackValue(args->at(0));
  VisitForAccumulatorValue(args->at(1));
  __ mov(a0, result_register());

  Register object = a1;
  Register index = a0;
  Register scratch = a2;
  Register result = v0;

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
  __ mov(a0, result_register());

  Register object = a1;
  Register index = a0;
  Register scratch1 = a2;
  Register scratch2 = a3;
  Register result = v0;

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
  __ li(result, Operand(Smi::FromInt(0)));
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
  context()->Plug(v0);
}


void FullCodeGenerator::EmitStringCompare(ZoneList<Expression*>* args) {
  ASSERT_EQ(2, args->length());

  VisitForStackValue(args->at(0));
  VisitForStackValue(args->at(1));

  StringCompareStub stub;
  __ CallStub(&stub);
  context()->Plug(v0);
}


void FullCodeGenerator::EmitMathSin(ZoneList<Expression*>* args) {
  // Load the argument on the stack and call the stub.
  TranscendentalCacheStub stub(TranscendentalCache::SIN,
                               TranscendentalCacheStub::TAGGED);
  ASSERT(args->length() == 1);
  VisitForStackValue(args->at(0));
  __ mov(a0, result_register());  // Stub requires parameter in a0 and on tos.
  __ CallStub(&stub);
  context()->Plug(v0);
}


void FullCodeGenerator::EmitMathCos(ZoneList<Expression*>* args) {
  // Load the argument on the stack and call the stub.
  TranscendentalCacheStub stub(TranscendentalCache::COS,
                               TranscendentalCacheStub::TAGGED);
  ASSERT(args->length() == 1);
  VisitForStackValue(args->at(0));
  __ mov(a0, result_register());  // Stub requires parameter in a0 and on tos.
  __ CallStub(&stub);
  context()->Plug(v0);
}


void FullCodeGenerator::EmitMathLog(ZoneList<Expression*>* args) {
  // Load the argument on the stack and call the stub.
  TranscendentalCacheStub stub(TranscendentalCache::LOG,
                               TranscendentalCacheStub::TAGGED);
  ASSERT(args->length() == 1);
  VisitForStackValue(args->at(0));
  __ mov(a0, result_register());  // Stub requires parameter in a0 and on tos.
  __ CallStub(&stub);
  context()->Plug(v0);
}


void FullCodeGenerator::EmitMathSqrt(ZoneList<Expression*>* args) {
  // Load the argument on the stack and call the runtime function.
  ASSERT(args->length() == 1);
  VisitForStackValue(args->at(0));
  __ CallRuntime(Runtime::kMath_sqrt, 1);
  context()->Plug(v0);
}


void FullCodeGenerator::EmitCallFunction(ZoneList<Expression*>* args) {
  ASSERT(args->length() >= 2);

  int arg_count = args->length() - 2;  // 2 ~ receiver and function.
  for (int i = 0; i < arg_count + 1; i++) {
    VisitForStackValue(args->at(i));
  }
  VisitForAccumulatorValue(args->last());  // Function.

  // InvokeFunction requires the function in a1. Move it in there.
  __ mov(a1, result_register());
  ParameterCount count(arg_count);
  __ InvokeFunction(a1, count, CALL_FUNCTION,
                    NullCallWrapper(), CALL_AS_METHOD);
  __ lw(cp, MemOperand(fp, StandardFrameConstants::kContextOffset));
  context()->Plug(v0);
}


void FullCodeGenerator::EmitRegExpConstructResult(ZoneList<Expression*>* args) {
  RegExpConstructResultStub stub;
  ASSERT(args->length() == 3);
  VisitForStackValue(args->at(0));
  VisitForStackValue(args->at(1));
  VisitForStackValue(args->at(2));
  __ CallStub(&stub);
  context()->Plug(v0);
}


void FullCodeGenerator::EmitSwapElements(ZoneList<Expression*>* args) {
  ASSERT(args->length() == 3);
  VisitForStackValue(args->at(0));
  VisitForStackValue(args->at(1));
  VisitForStackValue(args->at(2));
  Label done;
  Label slow_case;
  Register object = a0;
  Register index1 = a1;
  Register index2 = a2;
  Register elements = a3;
  Register scratch1 = t0;
  Register scratch2 = t1;

  __ lw(object, MemOperand(sp, 2 * kPointerSize));
  // Fetch the map and check if array is in fast case.
  // Check that object doesn't require security checks and
  // has no indexed interceptor.
  __ GetObjectType(object, scratch1, scratch2);
  __ Branch(&slow_case, ne, scratch2, Operand(JS_ARRAY_TYPE));
  // Map is now in scratch1.

  __ lbu(scratch2, FieldMemOperand(scratch1, Map::kBitFieldOffset));
  __ And(scratch2, scratch2, Operand(KeyedLoadIC::kSlowCaseBitFieldMask));
  __ Branch(&slow_case, ne, scratch2, Operand(zero_reg));

  // Check the object's elements are in fast case and writable.
  __ lw(elements, FieldMemOperand(object, JSObject::kElementsOffset));
  __ lw(scratch1, FieldMemOperand(elements, HeapObject::kMapOffset));
  __ LoadRoot(scratch2, Heap::kFixedArrayMapRootIndex);
  __ Branch(&slow_case, ne, scratch1, Operand(scratch2));

  // Check that both indices are smis.
  __ lw(index1, MemOperand(sp, 1 * kPointerSize));
  __ lw(index2, MemOperand(sp, 0));
  __ JumpIfNotBothSmi(index1, index2, &slow_case);

  // Check that both indices are valid.
  Label not_hi;
  __ lw(scratch1, FieldMemOperand(object, JSArray::kLengthOffset));
  __ Branch(&slow_case, ls, scratch1, Operand(index1));
  __ Branch(&not_hi, NegateCondition(hi), scratch1, Operand(index1));
  __ Branch(&slow_case, ls, scratch1, Operand(index2));
  __ bind(&not_hi);

  // Bring the address of the elements into index1 and index2.
  __ Addu(scratch1, elements,
      Operand(FixedArray::kHeaderSize - kHeapObjectTag));
  __ sll(index1, index1, kPointerSizeLog2 - kSmiTagSize);
  __ Addu(index1, scratch1, index1);
  __ sll(index2, index2, kPointerSizeLog2 - kSmiTagSize);
  __ Addu(index2, scratch1, index2);

  // Swap elements.
  __ lw(scratch1, MemOperand(index1, 0));
  __ lw(scratch2, MemOperand(index2, 0));
  __ sw(scratch1, MemOperand(index2, 0));
  __ sw(scratch2, MemOperand(index1, 0));

  Label new_space;
  __ InNewSpace(elements, scratch1, eq, &new_space);
  // Possible optimization: do a check that both values are Smis
  // (or them and test against Smi mask).

  __ mov(scratch1, elements);
  __ RecordWriteHelper(elements, index1, scratch2);
  __ RecordWriteHelper(scratch1, index2, scratch2);  // scratch1 holds elements.

  __ bind(&new_space);
  // We are done. Drop elements from the stack, and return undefined.
  __ Drop(3);
  __ LoadRoot(v0, Heap::kUndefinedValueRootIndex);
  __ jmp(&done);

  __ bind(&slow_case);
  __ CallRuntime(Runtime::kSwapElements, 3);

  __ bind(&done);
  context()->Plug(v0);
}


void FullCodeGenerator::EmitGetFromCache(ZoneList<Expression*>* args) {
  ASSERT_EQ(2, args->length());

  ASSERT_NE(NULL, args->at(0)->AsLiteral());
  int cache_id = Smi::cast(*(args->at(0)->AsLiteral()->handle()))->value();

  Handle<FixedArray> jsfunction_result_caches(
      isolate()->global_context()->jsfunction_result_caches());
  if (jsfunction_result_caches->length() <= cache_id) {
    __ Abort("Attempt to use undefined cache.");
    __ LoadRoot(v0, Heap::kUndefinedValueRootIndex);
    context()->Plug(v0);
    return;
  }

  VisitForAccumulatorValue(args->at(1));

  Register key = v0;
  Register cache = a1;
  __ lw(cache, ContextOperand(cp, Context::GLOBAL_INDEX));
  __ lw(cache, FieldMemOperand(cache, GlobalObject::kGlobalContextOffset));
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


void FullCodeGenerator::EmitIsRegExpEquivalent(ZoneList<Expression*>* args) {
  ASSERT_EQ(2, args->length());

  Register right = v0;
  Register left = a1;
  Register tmp = a2;
  Register tmp2 = a3;

  VisitForStackValue(args->at(0));
  VisitForAccumulatorValue(args->at(1));  // Result (right) in v0.
  __ pop(left);

  Label done, fail, ok;
  __ Branch(&ok, eq, left, Operand(right));
  // Fail if either is a non-HeapObject.
  __ And(tmp, left, Operand(right));
  __ And(at, tmp, Operand(kSmiTagMask));
  __ Branch(&fail, eq, at, Operand(zero_reg));
  __ lw(tmp, FieldMemOperand(left, HeapObject::kMapOffset));
  __ lbu(tmp2, FieldMemOperand(tmp, Map::kInstanceTypeOffset));
  __ Branch(&fail, ne, tmp2, Operand(JS_REGEXP_TYPE));
  __ lw(tmp2, FieldMemOperand(right, HeapObject::kMapOffset));
  __ Branch(&fail, ne, tmp, Operand(tmp2));
  __ lw(tmp, FieldMemOperand(left, JSRegExp::kDataOffset));
  __ lw(tmp2, FieldMemOperand(right, JSRegExp::kDataOffset));
  __ Branch(&ok, eq, tmp, Operand(tmp2));
  __ bind(&fail);
  __ LoadRoot(v0, Heap::kFalseValueRootIndex);
  __ jmp(&done);
  __ bind(&ok);
  __ LoadRoot(v0, Heap::kTrueValueRootIndex);
  __ bind(&done);

  context()->Plug(v0);
}


void FullCodeGenerator::EmitHasCachedArrayIndex(ZoneList<Expression*>* args) {
  VisitForAccumulatorValue(args->at(0));

  Label materialize_true, materialize_false;
  Label* if_true = NULL;
  Label* if_false = NULL;
  Label* fall_through = NULL;
  context()->PrepareTest(&materialize_true, &materialize_false,
                         &if_true, &if_false, &fall_through);

  __ lw(a0, FieldMemOperand(v0, String::kHashFieldOffset));
  __ And(a0, a0, Operand(String::kContainsCachedArrayIndexMask));

  PrepareForBailoutBeforeSplit(TOS_REG, true, if_true, if_false);
  Split(eq, a0, Operand(zero_reg), if_true, if_false, fall_through);

  context()->Plug(if_true, if_false);
}


void FullCodeGenerator::EmitGetCachedArrayIndex(ZoneList<Expression*>* args) {
  ASSERT(args->length() == 1);
  VisitForAccumulatorValue(args->at(0));

  if (FLAG_debug_code) {
    __ AbortIfNotString(v0);
  }

  __ lw(v0, FieldMemOperand(v0, String::kHashFieldOffset));
  __ IndexFromHash(v0, v0);

  context()->Plug(v0);
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
  Register scratch4 = v1;

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
  __ LoadRoot(v0, Heap::kEmptyStringRootIndex);
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
  if (FLAG_debug_code) {
    __ Assert(gt, "No empty arrays here in EmitFastAsciiArrayJoin",
        array_length, Operand(zero_reg));
  }
  __ bind(&loop);
  __ lw(string, MemOperand(element));
  __ Addu(element, element, kPointerSize);
  __ JumpIfSmi(string, &bailout);
  __ lw(scratch1, FieldMemOperand(string, HeapObject::kMapOffset));
  __ lbu(scratch1, FieldMemOperand(scratch1, Map::kInstanceTypeOffset));
  __ JumpIfInstanceTypeIsNotSequentialAscii(scratch1, scratch2, &bailout);
  __ lw(scratch1, FieldMemOperand(string, SeqAsciiString::kLengthOffset));
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
  __ lw(scratch1, FieldMemOperand(separator, SeqAsciiString::kLengthOffset));
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
          Operand(SeqAsciiString::kHeaderSize - kHeapObjectTag));

  // Check the length of the separator.
  __ lw(scratch1, FieldMemOperand(separator, SeqAsciiString::kLengthOffset));
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
  __ Addu(string, string, SeqAsciiString::kHeaderSize - kHeapObjectTag);
  __ CopyBytes(string, result_pos, string_length, scratch1);
  // End while (element < elements_end).
  __ Branch(&empty_separator_loop, lt, element, Operand(elements_end));
  ASSERT(result.is(v0));
  __ Branch(&done);

  // One-character separator case.
  __ bind(&one_char_separator);
  // Replace separator with its ascii character value.
  __ lbu(separator, FieldMemOperand(separator, SeqAsciiString::kHeaderSize));
  // Jump into the loop after the code that copies the separator, so the first
  // element is not preceded by a separator.
  __ jmp(&one_char_separator_loop_entry);

  __ bind(&one_char_separator_loop);
  // Live values in registers:
  //   result_pos: the position to which we are currently copying characters.
  //   element: Current array element.
  //   elements_end: Array end.
  //   separator: Single separator ascii char (in lower byte).

  // Copy the separator character to the result.
  __ sb(separator, MemOperand(result_pos));
  __ Addu(result_pos, result_pos, 1);

  // Copy next array element to the result.
  __ bind(&one_char_separator_loop_entry);
  __ lw(string, MemOperand(element));
  __ Addu(element, element, kPointerSize);
  __ lw(string_length, FieldMemOperand(string, String::kLengthOffset));
  __ SmiUntag(string_length);
  __ Addu(string, string, SeqAsciiString::kHeaderSize - kHeapObjectTag);
  __ CopyBytes(string, result_pos, string_length, scratch1);
  // End while (element < elements_end).
  __ Branch(&one_char_separator_loop, lt, element, Operand(elements_end));
  ASSERT(result.is(v0));
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
          Operand(SeqAsciiString::kHeaderSize - kHeapObjectTag));
  __ CopyBytes(string, result_pos, string_length, scratch1);

  __ bind(&long_separator);
  __ lw(string, MemOperand(element));
  __ Addu(element, element, kPointerSize);
  __ lw(string_length, FieldMemOperand(string, String::kLengthOffset));
  __ SmiUntag(string_length);
  __ Addu(string, string, SeqAsciiString::kHeaderSize - kHeapObjectTag);
  __ CopyBytes(string, result_pos, string_length, scratch1);
  // End while (element < elements_end).
  __ Branch(&long_separator_loop, lt, element, Operand(elements_end));
  ASSERT(result.is(v0));
  __ Branch(&done);

  __ bind(&bailout);
  __ LoadRoot(v0, Heap::kUndefinedValueRootIndex);
  __ bind(&done);
  context()->Plug(v0);
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
    __ lw(a0, GlobalObjectOperand());
    __ lw(a0, FieldMemOperand(a0, GlobalObject::kBuiltinsOffset));
    __ push(a0);
  }

  // Push the arguments ("left-to-right").
  int arg_count = args->length();
  for (int i = 0; i < arg_count; i++) {
    VisitForStackValue(args->at(i));
  }

  if (expr->is_jsruntime()) {
    // Call the JS runtime function.
    __ li(a2, Operand(expr->name()));
    RelocInfo::Mode mode = RelocInfo::CODE_TARGET;
    Handle<Code> ic =
        isolate()->stub_cache()->ComputeCallInitialize(arg_count, mode);
    __ Call(ic, mode, expr->id());
    // Restore context register.
    __ lw(cp, MemOperand(fp, StandardFrameConstants::kContextOffset));
  } else {
    // Call the C runtime function.
    __ CallRuntime(expr->function(), arg_count);
  }
  context()->Plug(v0);
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
        __ li(a1, Operand(Smi::FromInt(strict_mode_flag())));
        __ push(a1);
        __ InvokeBuiltin(Builtins::DELETE, CALL_FUNCTION);
        context()->Plug(v0);
      } else if (proxy != NULL) {
        Variable* var = proxy->var();
        // Delete of an unqualified identifier is disallowed in strict mode
        // but "delete this" is allowed.
        ASSERT(strict_mode_flag() == kNonStrictMode || var->is_this());
        if (var->IsUnallocated()) {
          __ lw(a2, GlobalObjectOperand());
          __ li(a1, Operand(var->name()));
          __ li(a0, Operand(Smi::FromInt(kNonStrictMode)));
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
          __ push(context_register());
          __ li(a2, Operand(var->name()));
          __ push(a2);
          __ CallRuntime(Runtime::kDeleteContextSlot, 2);
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
      context()->Plug(v0);
      break;
    }

    case Token::ADD: {
      Comment cmt(masm_, "[ UnaryOperation (ADD)");
      VisitForAccumulatorValue(expr->expression());
      Label no_conversion;
      __ JumpIfSmi(result_register(), &no_conversion);
      __ mov(a0, result_register());
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
  // GenericUnaryOpStub expects the argument to be in a0.
  VisitForAccumulatorValue(expr->expression());
  SetSourcePosition(expr->position());
  __ mov(a0, result_register());
  __ Call(stub.GetCode(), RelocInfo::CODE_TARGET, expr->id());
  context()->Plug(v0);
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
      __ li(at, Operand(Smi::FromInt(0)));
      __ push(at);
    }
    if (assign_type == NAMED_PROPERTY) {
      // Put the object both on the stack and in the accumulator.
      VisitForAccumulatorValue(prop->obj());
      __ push(v0);
      EmitNamedPropertyLoad(prop);
    } else {
      VisitForStackValue(prop->obj());
      VisitForAccumulatorValue(prop->key());
      __ lw(a1, MemOperand(sp, 0));
      __ push(v0);
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
  __ JumpIfSmi(v0, &no_conversion);
  __ mov(a0, v0);
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
  __ mov(a0, result_register());

  // Inline smi case if we are in a loop.
  Label stub_call, done;
  JumpPatchSite patch_site(masm_);

  int count_value = expr->op() == Token::INC ? 1 : -1;
  __ li(a1, Operand(Smi::FromInt(count_value)));

  if (ShouldInlineSmiCase(expr->op())) {
    __ AdduAndCheckForOverflow(v0, a0, a1, t0);
    __ BranchOnOverflow(&stub_call, t0);  // Do stub on overflow.

    // We could eliminate this smi check if we split the code at
    // the first smi check before calling ToNumber.
    patch_site.EmitJumpIfSmi(v0, &done);
    __ bind(&stub_call);
  }

  // Record position before stub call.
  SetSourcePosition(expr->position());

  BinaryOpStub stub(Token::ADD, NO_OVERWRITE);
  __ Call(stub.GetCode(), RelocInfo::CODE_TARGET, expr->CountId());
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
      __ mov(a0, result_register());  // Value.
      __ li(a2, Operand(prop->key()->AsLiteral()->handle()));  // Name.
      __ pop(a1);  // Receiver.
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
        context()->Plug(v0);
      }
      break;
    }
    case KEYED_PROPERTY: {
      __ mov(a0, result_register());  // Value.
      __ pop(a1);  // Key.
      __ pop(a2);  // Receiver.
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
        context()->Plug(v0);
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
    __ lw(a0, GlobalObjectOperand());
    __ li(a2, Operand(proxy->name()));
    Handle<Code> ic = isolate()->builtins()->LoadIC_Initialize();
    // Use a regular load, not a contextual load, to avoid a reference
    // error.
    __ Call(ic);
    PrepareForBailout(expr, TOS_REG);
    context()->Plug(v0);
  } else if (proxy != NULL && proxy->var()->IsLookupSlot()) {
    Label done, slow;

    // Generate code for loading from variables potentially shadowed
    // by eval-introduced variables.
    EmitDynamicLookupFastCase(proxy->var(), INSIDE_TYPEOF, &slow, &done);

    __ bind(&slow);
    __ li(a0, Operand(proxy->name()));
    __ Push(cp, a0);
    __ CallRuntime(Runtime::kLoadContextSlotNoReferenceError, 2);
    PrepareForBailout(expr, TOS_REG);
    __ bind(&done);

    context()->Plug(v0);
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
    __ JumpIfSmi(v0, if_true);
    __ lw(v0, FieldMemOperand(v0, HeapObject::kMapOffset));
    __ LoadRoot(at, Heap::kHeapNumberMapRootIndex);
    Split(eq, v0, Operand(at), if_true, if_false, fall_through);
  } else if (check->Equals(isolate()->heap()->string_symbol())) {
    __ JumpIfSmi(v0, if_false);
    // Check for undetectable objects => false.
    __ GetObjectType(v0, v0, a1);
    __ Branch(if_false, ge, a1, Operand(FIRST_NONSTRING_TYPE));
    __ lbu(a1, FieldMemOperand(v0, Map::kBitFieldOffset));
    __ And(a1, a1, Operand(1 << Map::kIsUndetectable));
    Split(eq, a1, Operand(zero_reg),
          if_true, if_false, fall_through);
  } else if (check->Equals(isolate()->heap()->boolean_symbol())) {
    __ LoadRoot(at, Heap::kTrueValueRootIndex);
    __ Branch(if_true, eq, v0, Operand(at));
    __ LoadRoot(at, Heap::kFalseValueRootIndex);
    Split(eq, v0, Operand(at), if_true, if_false, fall_through);
  } else if (FLAG_harmony_typeof &&
             check->Equals(isolate()->heap()->null_symbol())) {
    __ LoadRoot(at, Heap::kNullValueRootIndex);
    Split(eq, v0, Operand(at), if_true, if_false, fall_through);
  } else if (check->Equals(isolate()->heap()->undefined_symbol())) {
    __ LoadRoot(at, Heap::kUndefinedValueRootIndex);
    __ Branch(if_true, eq, v0, Operand(at));
    __ JumpIfSmi(v0, if_false);
    // Check for undetectable objects => true.
    __ lw(v0, FieldMemOperand(v0, HeapObject::kMapOffset));
    __ lbu(a1, FieldMemOperand(v0, Map::kBitFieldOffset));
    __ And(a1, a1, Operand(1 << Map::kIsUndetectable));
    Split(ne, a1, Operand(zero_reg), if_true, if_false, fall_through);
  } else if (check->Equals(isolate()->heap()->function_symbol())) {
    __ JumpIfSmi(v0, if_false);
    __ GetObjectType(v0, a1, v0);  // Leave map in a1.
    Split(ge, v0, Operand(FIRST_CALLABLE_SPEC_OBJECT_TYPE),
        if_true, if_false, fall_through);

  } else if (check->Equals(isolate()->heap()->object_symbol())) {
    __ JumpIfSmi(v0, if_false);
    if (!FLAG_harmony_typeof) {
      __ LoadRoot(at, Heap::kNullValueRootIndex);
      __ Branch(if_true, eq, v0, Operand(at));
    }
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
}


void FullCodeGenerator::EmitLiteralCompareUndefined(Expression* expr,
                                                    Label* if_true,
                                                    Label* if_false,
                                                    Label* fall_through) {
  VisitForAccumulatorValue(expr);
  PrepareForBailoutBeforeSplit(TOS_REG, true, if_true, if_false);

  __ LoadRoot(at, Heap::kUndefinedValueRootIndex);
  Split(eq, v0, Operand(at), if_true, if_false, fall_through);
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
      __ LoadRoot(t0, Heap::kTrueValueRootIndex);
      Split(eq, v0, Operand(t0), if_true, if_false, fall_through);
      break;

    case Token::INSTANCEOF: {
      VisitForStackValue(expr->right());
      InstanceofStub stub(InstanceofStub::kNoFlags);
      __ CallStub(&stub);
      PrepareForBailoutBeforeSplit(TOS_REG, true, if_true, if_false);
      // The stub returns 0 for true.
      Split(eq, v0, Operand(zero_reg), if_true, if_false, fall_through);
      break;
    }

    default: {
      VisitForAccumulatorValue(expr->right());
      Condition cc = eq;
      switch (op) {
        case Token::EQ_STRICT:
        case Token::EQ:
          cc = eq;
          __ mov(a0, result_register());
          __ pop(a1);
          break;
        case Token::LT:
          cc = lt;
          __ mov(a0, result_register());
          __ pop(a1);
          break;
        case Token::GT:
          // Reverse left and right sides to obtain ECMA-262 conversion order.
          cc = lt;
          __ mov(a1, result_register());
          __ pop(a0);
         break;
        case Token::LTE:
          // Reverse left and right sides to obtain ECMA-262 conversion order.
          cc = ge;
          __ mov(a1, result_register());
          __ pop(a0);
          break;
        case Token::GTE:
          cc = ge;
          __ mov(a0, result_register());
          __ pop(a1);
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
        __ Or(a2, a0, Operand(a1));
        patch_site.EmitJumpIfNotSmi(a2, &slow_case);
        Split(cc, a1, Operand(a0), if_true, if_false, NULL);
        __ bind(&slow_case);
      }
      // Record position and call the compare IC.
      SetSourcePosition(expr->position());
      Handle<Code> ic = CompareIC::GetUninitialized(op);
      __ Call(ic, RelocInfo::CODE_TARGET, expr->id());
      patch_site.EmitPatchInfo();
      PrepareForBailoutBeforeSplit(TOS_REG, true, if_true, if_false);
      Split(cc, v0, Operand(zero_reg), if_true, if_false, fall_through);
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
  __ mov(a0, result_register());
  __ LoadRoot(a1, Heap::kNullValueRootIndex);
  if (expr->is_strict()) {
    Split(eq, a0, Operand(a1), if_true, if_false, fall_through);
  } else {
    __ Branch(if_true, eq, a0, Operand(a1));
    __ LoadRoot(a1, Heap::kUndefinedValueRootIndex);
    __ Branch(if_true, eq, a0, Operand(a1));
    __ And(at, a0, Operand(kSmiTagMask));
    __ Branch(if_false, eq, at, Operand(zero_reg));
    // It can be an undetectable object.
    __ lw(a1, FieldMemOperand(a0, HeapObject::kMapOffset));
    __ lbu(a1, FieldMemOperand(a1, Map::kBitFieldOffset));
    __ And(a1, a1, Operand(1 << Map::kIsUndetectable));
    Split(ne, a1, Operand(zero_reg), if_true, if_false, fall_through);
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
  ASSERT_EQ(POINTER_SIZE_ALIGN(frame_offset), frame_offset);
  __ sw(value, MemOperand(fp, frame_offset));
}


void FullCodeGenerator::LoadContextField(Register dst, int context_index) {
  __ lw(dst, ContextOperand(cp, context_index));
}


void FullCodeGenerator::PushFunctionArgumentForContextAllocation() {
  Scope* declaration_scope = scope()->DeclarationScope();
  if (declaration_scope->is_global_scope()) {
    // Contexts nested in the global context have a canonical empty function
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
    ASSERT(declaration_scope->is_function_scope());
    __ lw(at, MemOperand(fp, JavaScriptFrameConstants::kFunctionOffset));
  }
  __ push(at);
}


// ----------------------------------------------------------------------------
// Non-local control flow support.

void FullCodeGenerator::EnterFinallyBlock() {
  ASSERT(!result_register().is(a1));
  // Store result register while executing finally block.
  __ push(result_register());
  // Cook return address in link register to stack (smi encoded Code* delta).
  __ Subu(a1, ra, Operand(masm_->CodeObject()));
  ASSERT_EQ(1, kSmiTagSize + kSmiShiftSize);
  STATIC_ASSERT(0 == kSmiTag);
  __ Addu(a1, a1, Operand(a1));  // Convert to smi.
  __ push(a1);
}


void FullCodeGenerator::ExitFinallyBlock() {
  ASSERT(!result_register().is(a1));
  // Restore result register from stack.
  __ pop(a1);
  // Uncook return address and return.
  __ pop(result_register());
  ASSERT_EQ(1, kSmiTagSize + kSmiShiftSize);
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

} }  // namespace v8::internal

#endif  // V8_TARGET_ARCH_MIPS
