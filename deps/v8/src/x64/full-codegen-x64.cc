// Copyright 2010 the V8 project authors. All rights reserved.
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

#include "codegen-inl.h"
#include "compiler.h"
#include "debug.h"
#include "full-codegen.h"
#include "parser.h"
#include "scopes.h"

namespace v8 {
namespace internal {

#define __ ACCESS_MASM(masm_)

// Generate code for a JS function.  On entry to the function the receiver
// and arguments have been pushed on the stack left to right, with the
// return address on top of them.  The actual argument count matches the
// formal parameter count expected by the function.
//
// The live registers are:
//   o rdi: the JS function object being called (ie, ourselves)
//   o rsi: our context
//   o rbp: our caller's frame pointer
//   o rsp: stack pointer (pointing to return address)
//
// The function builds a JS frame.  Please see JavaScriptFrameConstants in
// frames-x64.h for its layout.
void FullCodeGenerator::Generate(CompilationInfo* info, Mode mode) {
  ASSERT(info_ == NULL);
  info_ = info;
  SetFunctionPosition(function());
  Comment cmnt(masm_, "[ function compiled by full code generator");

  if (mode == PRIMARY) {
    __ push(rbp);  // Caller's frame pointer.
    __ movq(rbp, rsp);
    __ push(rsi);  // Callee's context.
    __ push(rdi);  // Callee's JS Function.

    { Comment cmnt(masm_, "[ Allocate locals");
      int locals_count = scope()->num_stack_slots();
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
    int heap_slots = scope()->num_heap_slots() - Context::MIN_CONTEXT_SLOTS;
    if (heap_slots > 0) {
      Comment cmnt(masm_, "[ Allocate local context");
      // Argument to NewContext is the function, which is still in rdi.
      __ push(rdi);
      if (heap_slots <= FastNewContextStub::kMaximumSlots) {
        FastNewContextStub stub(heap_slots);
        __ CallStub(&stub);
      } else {
        __ CallRuntime(Runtime::kNewContext, 1);
      }
      function_in_register = false;
      // Context is returned in both rax and rsi.  It replaces the context
      // passed to us.  It's saved in the stack and kept live in rsi.
      __ movq(Operand(rbp, StandardFrameConstants::kContextOffset), rsi);

      // Copy any necessary parameters into the context.
      int num_parameters = scope()->num_parameters();
      for (int i = 0; i < num_parameters; i++) {
        Slot* slot = scope()->parameter(i)->slot();
        if (slot != NULL && slot->type() == Slot::CONTEXT) {
          int parameter_offset = StandardFrameConstants::kCallerSPOffset +
                                     (num_parameters - 1 - i) * kPointerSize;
          // Load parameter from stack.
          __ movq(rax, Operand(rbp, parameter_offset));
          // Store it in the context.
          int context_offset = Context::SlotOffset(slot->index());
          __ movq(Operand(rsi, context_offset), rax);
          // Update the write barrier. This clobbers all involved
          // registers, so we have use a third register to avoid
          // clobbering rsi.
          __ movq(rcx, rsi);
          __ RecordWrite(rcx, context_offset, rax, rbx);
        }
      }
    }

    // Possibly allocate an arguments object.
    Variable* arguments = scope()->arguments()->AsVariable();
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
      int offset = scope()->num_parameters() * kPointerSize;
      __ lea(rdx,
             Operand(rbp, StandardFrameConstants::kCallerSPOffset + offset));
      __ push(rdx);
      __ Push(Smi::FromInt(scope()->num_parameters()));
      // Arguments to ArgumentsAccessStub:
      //   function, receiver address, parameter count.
      // The stub will rewrite receiver and parameter count if the previous
      // stack frame was an arguments adapter frame.
      ArgumentsAccessStub stub(ArgumentsAccessStub::NEW_OBJECT);
      __ CallStub(&stub);
      // Store new arguments object in both "arguments" and ".arguments" slots.
      __ movq(rcx, rax);
      Move(arguments->slot(), rax, rbx, rdx);
      Slot* dot_arguments_slot =
          scope()->arguments_shadow()->AsVariable()->slot();
      Move(dot_arguments_slot, rcx, rbx, rdx);
    }
  }

  { Comment cmnt(masm_, "[ Declarations");
    // For named function expressions, declare the function name as a
    // constant.
    if (scope()->is_function_scope() && scope()->function() != NULL) {
      EmitDeclaration(scope()->function(), Variable::CONST, NULL);
    }
    // Visit all the explicit declarations unless there is an illegal
    // redeclaration.
    if (scope()->HasIllegalRedeclaration()) {
      scope()->VisitIllegalRedeclaration(this);
    } else {
      VisitDeclarations(scope()->declarations());
    }
  }

  { Comment cmnt(masm_, "[ Stack check");
    Label ok;
    __ CompareRoot(rsp, Heap::kStackLimitRootIndex);
    __ j(above_equal, &ok);
    StackCheckStub stub;
    __ CallStub(&stub);
    __ bind(&ok);
  }

  if (FLAG_trace) {
    __ CallRuntime(Runtime::kTraceEnter, 0);
  }

  { Comment cmnt(masm_, "[ Body");
    ASSERT(loop_depth() == 0);
    VisitStatements(function()->body());
    ASSERT(loop_depth() == 0);
  }

  { Comment cmnt(masm_, "[ return <undefined>;");
    // Emit a 'return undefined' in case control fell off the end of the body.
    __ LoadRoot(rax, Heap::kUndefinedValueRootIndex);
    EmitReturnSequence();
  }
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
#ifdef DEBUG
    // Add a label for checking the size of the code used for returning.
    Label check_exit_codesize;
    masm_->bind(&check_exit_codesize);
#endif
    CodeGenerator::RecordPositions(masm_, function()->end_position());
    __ RecordJSReturn();
    // Do not use the leave instruction here because it is too short to
    // patch with the code required by the debugger.
    __ movq(rsp, rbp);
    __ pop(rbp);
    __ ret((scope()->num_parameters() + 1) * kPointerSize);
#ifdef ENABLE_DEBUGGER_SUPPORT
    // Add padding that will be overwritten by a debugger breakpoint.  We
    // have just generated "movq rsp, rbp; pop rbp; ret k" with length 7
    // (3 + 1 + 3).
    const int kPadding = Assembler::kJSReturnSequenceLength - 7;
    for (int i = 0; i < kPadding; ++i) {
      masm_->int3();
    }
    // Check that the size of the code used for returning matches what is
    // expected by the debugger.
    ASSERT_EQ(Assembler::kJSReturnSequenceLength,
            masm_->SizeOfCodeGeneratedSince(&check_exit_codesize));
#endif
  }
}


void FullCodeGenerator::Apply(Expression::Context context, Register reg) {
  switch (context) {
    case Expression::kUninitialized:
      UNREACHABLE();

    case Expression::kEffect:
      // Nothing to do.
      break;

    case Expression::kValue:
      // Move value into place.
      switch (location_) {
        case kAccumulator:
          if (!reg.is(result_register())) __ movq(result_register(), reg);
          break;
        case kStack:
          __ push(reg);
          break;
      }
      break;

    case Expression::kTest:
      // For simplicity we always test the accumulator register.
      if (!reg.is(result_register())) __ movq(result_register(), reg);
      DoTest(context);
      break;

    case Expression::kValueTest:
    case Expression::kTestValue:
      if (!reg.is(result_register())) __ movq(result_register(), reg);
      switch (location_) {
        case kAccumulator:
          break;
        case kStack:
          __ push(result_register());
          break;
      }
      DoTest(context);
      break;
  }
}


void FullCodeGenerator::Apply(Expression::Context context, Slot* slot) {
  switch (context) {
    case Expression::kUninitialized:
      UNREACHABLE();
    case Expression::kEffect:
      // Nothing to do.
      break;
    case Expression::kValue: {
      MemOperand slot_operand = EmitSlotSearch(slot, result_register());
      switch (location_) {
        case kAccumulator:
          __ movq(result_register(), slot_operand);
          break;
        case kStack:
          // Memory operands can be pushed directly.
          __ push(slot_operand);
          break;
      }
      break;
    }

    case Expression::kTest:
      Move(result_register(), slot);
      DoTest(context);
      break;

    case Expression::kValueTest:
    case Expression::kTestValue:
      Move(result_register(), slot);
      switch (location_) {
        case kAccumulator:
          break;
        case kStack:
          __ push(result_register());
          break;
      }
      DoTest(context);
      break;
  }
}


void FullCodeGenerator::Apply(Expression::Context context, Literal* lit) {
  switch (context) {
    case Expression::kUninitialized:
      UNREACHABLE();
    case Expression::kEffect:
      // Nothing to do.
      break;
    case Expression::kValue:
      switch (location_) {
        case kAccumulator:
          __ Move(result_register(), lit->handle());
          break;
        case kStack:
          __ Push(lit->handle());
          break;
      }
      break;

    case Expression::kTest:
      __ Move(result_register(), lit->handle());
      DoTest(context);
      break;

    case Expression::kValueTest:
    case Expression::kTestValue:
      __ Move(result_register(), lit->handle());
      switch (location_) {
        case kAccumulator:
          break;
        case kStack:
          __ push(result_register());
          break;
      }
      DoTest(context);
      break;
  }
}


void FullCodeGenerator::ApplyTOS(Expression::Context context) {
  switch (context) {
    case Expression::kUninitialized:
      UNREACHABLE();

    case Expression::kEffect:
      __ Drop(1);
      break;

    case Expression::kValue:
      switch (location_) {
        case kAccumulator:
          __ pop(result_register());
          break;
        case kStack:
          break;
      }
      break;

    case Expression::kTest:
      __ pop(result_register());
      DoTest(context);
      break;

    case Expression::kValueTest:
    case Expression::kTestValue:
      switch (location_) {
        case kAccumulator:
          __ pop(result_register());
          break;
        case kStack:
          __ movq(result_register(), Operand(rsp, 0));
          break;
      }
      DoTest(context);
      break;
  }
}


void FullCodeGenerator::DropAndApply(int count,
                                     Expression::Context context,
                                     Register reg) {
  ASSERT(count > 0);
  ASSERT(!reg.is(rsp));
  switch (context) {
    case Expression::kUninitialized:
      UNREACHABLE();

    case Expression::kEffect:
      __ Drop(count);
      break;

    case Expression::kValue:
      switch (location_) {
        case kAccumulator:
          __ Drop(count);
          if (!reg.is(result_register())) __ movq(result_register(), reg);
          break;
        case kStack:
          if (count > 1) __ Drop(count - 1);
          __ movq(Operand(rsp, 0), reg);
          break;
      }
      break;

    case Expression::kTest:
      __ Drop(count);
      if (!reg.is(result_register())) __ movq(result_register(), reg);
      DoTest(context);
      break;

    case Expression::kValueTest:
    case Expression::kTestValue:
      switch (location_) {
        case kAccumulator:
          __ Drop(count);
          if (!reg.is(result_register())) __ movq(result_register(), reg);
          break;
        case kStack:
          if (count > 1) __ Drop(count - 1);
          __ movq(result_register(), reg);
          __ movq(Operand(rsp, 0), result_register());
          break;
      }
      DoTest(context);
      break;
  }
}


void FullCodeGenerator::PrepareTest(Label* materialize_true,
                                    Label* materialize_false,
                                    Label** if_true,
                                    Label** if_false) {
  switch (context_) {
    case Expression::kUninitialized:
      UNREACHABLE();
      break;
    case Expression::kEffect:
      // In an effect context, the true and the false case branch to the
      // same label.
      *if_true = *if_false = materialize_true;
      break;
    case Expression::kValue:
      *if_true = materialize_true;
      *if_false = materialize_false;
      break;
    case Expression::kTest:
      *if_true = true_label_;
      *if_false = false_label_;
      break;
    case Expression::kValueTest:
      *if_true = materialize_true;
      *if_false = false_label_;
      break;
    case Expression::kTestValue:
      *if_true = true_label_;
      *if_false = materialize_false;
      break;
  }
}


void FullCodeGenerator::Apply(Expression::Context context,
                              Label* materialize_true,
                              Label* materialize_false) {
  switch (context) {
    case Expression::kUninitialized:

    case Expression::kEffect:
      ASSERT_EQ(materialize_true, materialize_false);
      __ bind(materialize_true);
      break;

    case Expression::kValue: {
      Label done;
      switch (location_) {
        case kAccumulator:
          __ bind(materialize_true);
          __ Move(result_register(), Factory::true_value());
          __ jmp(&done);
          __ bind(materialize_false);
          __ Move(result_register(), Factory::false_value());
          break;
        case kStack:
          __ bind(materialize_true);
          __ Push(Factory::true_value());
          __ jmp(&done);
          __ bind(materialize_false);
          __ Push(Factory::false_value());
          break;
      }
      __ bind(&done);
      break;
    }

    case Expression::kTest:
      break;

    case Expression::kValueTest:
      __ bind(materialize_true);
      switch (location_) {
        case kAccumulator:
          __ Move(result_register(), Factory::true_value());
          break;
        case kStack:
          __ Push(Factory::true_value());
          break;
      }
      __ jmp(true_label_);
      break;

    case Expression::kTestValue:
      __ bind(materialize_false);
      switch (location_) {
        case kAccumulator:
          __ Move(result_register(), Factory::false_value());
          break;
        case kStack:
          __ Push(Factory::false_value());
          break;
      }
      __ jmp(false_label_);
      break;
  }
}


// Convert constant control flow (true or false) to the result expected for
// a given expression context.
void FullCodeGenerator::Apply(Expression::Context context, bool flag) {
  switch (context) {
    case Expression::kUninitialized:
      UNREACHABLE();
      break;
    case Expression::kEffect:
      break;
    case Expression::kValue: {
      Heap::RootListIndex value_root_index =
          flag ? Heap::kTrueValueRootIndex : Heap::kFalseValueRootIndex;
      switch (location_) {
        case kAccumulator:
          __ LoadRoot(result_register(), value_root_index);
          break;
        case kStack:
          __ PushRoot(value_root_index);
          break;
      }
      break;
    }
    case Expression::kTest:
      __ jmp(flag ? true_label_ : false_label_);
      break;
    case Expression::kTestValue:
      switch (location_) {
        case kAccumulator:
          // If value is false it's needed.
          if (!flag) __ LoadRoot(result_register(), Heap::kFalseValueRootIndex);
          break;
        case kStack:
          // If value is false it's needed.
          if (!flag) __ PushRoot(Heap::kFalseValueRootIndex);
          break;
      }
      __ jmp(flag ? true_label_ : false_label_);
      break;
    case Expression::kValueTest:
      switch (location_) {
        case kAccumulator:
          // If value is true it's needed.
          if (flag) __ LoadRoot(result_register(), Heap::kTrueValueRootIndex);
          break;
        case kStack:
          // If value is true it's needed.
          if (flag) __ PushRoot(Heap::kTrueValueRootIndex);
          break;
      }
      __ jmp(flag ? true_label_ : false_label_);
      break;
  }
}


void FullCodeGenerator::DoTest(Expression::Context context) {
  // The value to test is in the accumulator.  If the value might be needed
  // on the stack (value/test and test/value contexts with a stack location
  // desired), then the value is already duplicated on the stack.
  ASSERT_NE(NULL, true_label_);
  ASSERT_NE(NULL, false_label_);

  // In value/test and test/value expression contexts with stack as the
  // desired location, there is already an extra value on the stack.  Use a
  // label to discard it if unneeded.
  Label discard;
  Label* if_true = true_label_;
  Label* if_false = false_label_;
  switch (context) {
    case Expression::kUninitialized:
    case Expression::kEffect:
    case Expression::kValue:
      UNREACHABLE();
    case Expression::kTest:
      break;
    case Expression::kValueTest:
      switch (location_) {
        case kAccumulator:
          break;
        case kStack:
          if_false = &discard;
          break;
      }
      break;
    case Expression::kTestValue:
      switch (location_) {
        case kAccumulator:
          break;
        case kStack:
          if_true = &discard;
          break;
      }
      break;
  }

  // Emit the inlined tests assumed by the stub.
  __ CompareRoot(result_register(), Heap::kUndefinedValueRootIndex);
  __ j(equal, if_false);
  __ CompareRoot(result_register(), Heap::kTrueValueRootIndex);
  __ j(equal, if_true);
  __ CompareRoot(result_register(), Heap::kFalseValueRootIndex);
  __ j(equal, if_false);
  ASSERT_EQ(0, kSmiTag);
  __ SmiCompare(result_register(), Smi::FromInt(0));
  __ j(equal, if_false);
  Condition is_smi = masm_->CheckSmi(result_register());
  __ j(is_smi, if_true);

  // Save a copy of the value if it may be needed and isn't already saved.
  switch (context) {
    case Expression::kUninitialized:
    case Expression::kEffect:
    case Expression::kValue:
      UNREACHABLE();
    case Expression::kTest:
      break;
    case Expression::kValueTest:
      switch (location_) {
        case kAccumulator:
          __ push(result_register());
          break;
        case kStack:
          break;
      }
      break;
    case Expression::kTestValue:
      switch (location_) {
        case kAccumulator:
          __ push(result_register());
          break;
        case kStack:
          break;
      }
      break;
  }

  // Call the ToBoolean stub for all other cases.
  ToBooleanStub stub;
  __ push(result_register());
  __ CallStub(&stub);
  __ testq(rax, rax);

  // The stub returns nonzero for true.  Complete based on the context.
  switch (context) {
    case Expression::kUninitialized:
    case Expression::kEffect:
    case Expression::kValue:
      UNREACHABLE();

    case Expression::kTest:
      __ j(not_zero, true_label_);
      __ jmp(false_label_);
      break;

    case Expression::kValueTest:
      switch (location_) {
        case kAccumulator:
          __ j(zero, &discard);
          __ pop(result_register());
          __ jmp(true_label_);
          break;
        case kStack:
          __ j(not_zero, true_label_);
          break;
      }
      __ bind(&discard);
      __ Drop(1);
      __ jmp(false_label_);
      break;

    case Expression::kTestValue:
      switch (location_) {
        case kAccumulator:
          __ j(not_zero, &discard);
          __ pop(result_register());
          __ jmp(false_label_);
          break;
        case kStack:
          __ j(zero, false_label_);
          break;
      }
      __ bind(&discard);
      __ Drop(1);
      __ jmp(true_label_);
      break;
  }
}


MemOperand FullCodeGenerator::EmitSlotSearch(Slot* slot, Register scratch) {
  switch (slot->type()) {
    case Slot::PARAMETER:
    case Slot::LOCAL:
      return Operand(rbp, SlotOffset(slot));
    case Slot::CONTEXT: {
      int context_chain_length =
          scope()->ContextChainLength(slot->var()->scope());
      __ LoadContext(scratch, context_chain_length);
      return CodeGenerator::ContextOperand(scratch, slot->index());
    }
    case Slot::LOOKUP:
      UNREACHABLE();
  }
  UNREACHABLE();
  return Operand(rax, 0);
}


void FullCodeGenerator::Move(Register destination, Slot* source) {
  MemOperand location = EmitSlotSearch(source, destination);
  __ movq(destination, location);
}


void FullCodeGenerator::Move(Slot* dst,
                             Register src,
                             Register scratch1,
                             Register scratch2) {
  ASSERT(dst->type() != Slot::LOOKUP);  // Not yet implemented.
  ASSERT(!scratch1.is(src) && !scratch2.is(src));
  MemOperand location = EmitSlotSearch(dst, scratch1);
  __ movq(location, src);
  // Emit the write barrier code if the location is in the heap.
  if (dst->type() == Slot::CONTEXT) {
    int offset = FixedArray::kHeaderSize + dst->index() * kPointerSize;
    __ RecordWrite(scratch1, offset, src, scratch2);
  }
}


void FullCodeGenerator::EmitDeclaration(Variable* variable,
                                        Variable::Mode mode,
                                        FunctionLiteral* function) {
  Comment cmnt(masm_, "[ Declaration");
  ASSERT(variable != NULL);  // Must have been resolved.
  Slot* slot = variable->slot();
  Property* prop = variable->AsProperty();

  if (slot != NULL) {
    switch (slot->type()) {
      case Slot::PARAMETER:
      case Slot::LOCAL:
        if (mode == Variable::CONST) {
          __ LoadRoot(kScratchRegister, Heap::kTheHoleValueRootIndex);
          __ movq(Operand(rbp, SlotOffset(slot)), kScratchRegister);
        } else if (function != NULL) {
          VisitForValue(function, kAccumulator);
          __ movq(Operand(rbp, SlotOffset(slot)), result_register());
        }
        break;

      case Slot::CONTEXT:
        // We bypass the general EmitSlotSearch because we know more about
        // this specific context.

        // The variable in the decl always resides in the current context.
        ASSERT_EQ(0, scope()->ContextChainLength(variable->scope()));
        if (FLAG_debug_code) {
          // Check if we have the correct context pointer.
          __ movq(rbx,
                  CodeGenerator::ContextOperand(rsi, Context::FCONTEXT_INDEX));
          __ cmpq(rbx, rsi);
          __ Check(equal, "Unexpected declaration in current context.");
        }
        if (mode == Variable::CONST) {
          __ LoadRoot(kScratchRegister, Heap::kTheHoleValueRootIndex);
          __ movq(CodeGenerator::ContextOperand(rsi, slot->index()),
                  kScratchRegister);
          // No write barrier since the hole value is in old space.
        } else if (function != NULL) {
          VisitForValue(function, kAccumulator);
          __ movq(CodeGenerator::ContextOperand(rsi, slot->index()),
                  result_register());
          int offset = Context::SlotOffset(slot->index());
          __ movq(rbx, rsi);
          __ RecordWrite(rbx, offset, result_register(), rcx);
        }
        break;

      case Slot::LOOKUP: {
        __ push(rsi);
        __ Push(variable->name());
        // Declaration nodes are always introduced in one of two modes.
        ASSERT(mode == Variable::VAR || mode == Variable::CONST);
        PropertyAttributes attr = (mode == Variable::VAR) ? NONE : READ_ONLY;
        __ Push(Smi::FromInt(attr));
        // Push initial value, if any.
        // Note: For variables we must not push an initial value (such as
        // 'undefined') because we may have a (legal) redeclaration and we
        // must not destroy the current value.
        if (mode == Variable::CONST) {
          __ PushRoot(Heap::kTheHoleValueRootIndex);
        } else if (function != NULL) {
          VisitForValue(function, kStack);
        } else {
          __ Push(Smi::FromInt(0));  // no initial value!
        }
        __ CallRuntime(Runtime::kDeclareContextSlot, 4);
        break;
      }
    }

  } else if (prop != NULL) {
    if (function != NULL || mode == Variable::CONST) {
      // We are declaring a function or constant that rewrites to a
      // property.  Use (keyed) IC to set the initial value.
      VisitForValue(prop->obj(), kStack);
      if (function != NULL) {
        VisitForValue(prop->key(), kStack);
        VisitForValue(function, kAccumulator);
        __ pop(rcx);
      } else {
        VisitForValue(prop->key(), kAccumulator);
        __ movq(rcx, result_register());
        __ LoadRoot(result_register(), Heap::kTheHoleValueRootIndex);
      }
      __ pop(rdx);

      Handle<Code> ic(Builtins::builtin(Builtins::KeyedStoreIC_Initialize));
      __ call(ic, RelocInfo::CODE_TARGET);
      // Absence of a test rax instruction following the call
      // indicates that none of the load was inlined.
      __ nop();
    }
  }
}


void FullCodeGenerator::VisitDeclaration(Declaration* decl) {
  EmitDeclaration(decl->proxy()->var(), decl->mode(), decl->fun());
}


void FullCodeGenerator::DeclareGlobals(Handle<FixedArray> pairs) {
  // Call the runtime to declare the globals.
  __ push(rsi);  // The context is the first argument.
  __ Push(pairs);
  __ Push(Smi::FromInt(is_eval() ? 1 : 0));
  __ CallRuntime(Runtime::kDeclareGlobals, 3);
  // Return value is ignored.
}


void FullCodeGenerator::VisitSwitchStatement(SwitchStatement* stmt) {
  Comment cmnt(masm_, "[ SwitchStatement");
  Breakable nested_statement(this, stmt);
  SetStatementPosition(stmt);
  // Keep the switch value on the stack until a case matches.
  VisitForValue(stmt->tag(), kStack);

  ZoneList<CaseClause*>* clauses = stmt->cases();
  CaseClause* default_clause = NULL;  // Can occur anywhere in the list.

  Label next_test;  // Recycled for each test.
  // Compile all the tests with branches to their bodies.
  for (int i = 0; i < clauses->length(); i++) {
    CaseClause* clause = clauses->at(i);
    // The default is not a test, but remember it as final fall through.
    if (clause->is_default()) {
      default_clause = clause;
      continue;
    }

    Comment cmnt(masm_, "[ Case comparison");
    __ bind(&next_test);
    next_test.Unuse();

    // Compile the label expression.
    VisitForValue(clause->label(), kAccumulator);

    // Perform the comparison as if via '==='.  The comparison stub expects
    // the smi vs. smi case to be handled before it is called.
    Label slow_case;
    __ movq(rdx, Operand(rsp, 0));  // Switch value.
    __ JumpIfNotBothSmi(rdx, rax, &slow_case);
    __ SmiCompare(rdx, rax);
    __ j(not_equal, &next_test);
    __ Drop(1);  // Switch value is no longer needed.
    __ jmp(clause->body_target()->entry_label());

    __ bind(&slow_case);
    CompareStub stub(equal, true);
    __ CallStub(&stub);
    __ testq(rax, rax);
    __ j(not_equal, &next_test);
    __ Drop(1);  // Switch value is no longer needed.
    __ jmp(clause->body_target()->entry_label());
  }

  // Discard the test value and jump to the default if present, otherwise to
  // the end of the statement.
  __ bind(&next_test);
  __ Drop(1);  // Switch value is no longer needed.
  if (default_clause == NULL) {
    __ jmp(nested_statement.break_target());
  } else {
    __ jmp(default_clause->body_target()->entry_label());
  }

  // Compile all the case bodies.
  for (int i = 0; i < clauses->length(); i++) {
    Comment cmnt(masm_, "[ Case body");
    CaseClause* clause = clauses->at(i);
    __ bind(clause->body_target()->entry_label());
    VisitStatements(clause->statements());
  }

  __ bind(nested_statement.break_target());
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
  VisitForValue(stmt->enumerable(), kAccumulator);
  __ CompareRoot(rax, Heap::kUndefinedValueRootIndex);
  __ j(equal, &exit);
  __ CompareRoot(rax, Heap::kNullValueRootIndex);
  __ j(equal, &exit);

  // Convert the object to a JS object.
  Label convert, done_convert;
  __ JumpIfSmi(rax, &convert);
  __ CmpObjectType(rax, FIRST_JS_OBJECT_TYPE, rcx);
  __ j(above_equal, &done_convert);
  __ bind(&convert);
  __ push(rax);
  __ InvokeBuiltin(Builtins::TO_OBJECT, CALL_FUNCTION);
  __ bind(&done_convert);
  __ push(rax);

  // TODO(kasperl): Check cache validity in generated code. This is a
  // fast case for the JSObject::IsSimpleEnum cache validity
  // checks. If we cannot guarantee cache validity, call the runtime
  // system to check cache validity or get the property names in a
  // fixed array.

  // Get the set of properties to enumerate.
  __ push(rax);  // Duplicate the enumerable object on the stack.
  __ CallRuntime(Runtime::kGetPropertyNamesFast, 1);

  // If we got a map from the runtime call, we can do a fast
  // modification check. Otherwise, we got a fixed array, and we have
  // to do a slow check.
  Label fixed_array;
  __ CompareRoot(FieldOperand(rax, HeapObject::kMapOffset),
                 Heap::kMetaMapRootIndex);
  __ j(not_equal, &fixed_array);

  // We got a map in register rax. Get the enumeration cache from it.
  __ movq(rcx, FieldOperand(rax, Map::kInstanceDescriptorsOffset));
  __ movq(rcx, FieldOperand(rcx, DescriptorArray::kEnumerationIndexOffset));
  __ movq(rdx, FieldOperand(rcx, DescriptorArray::kEnumCacheBridgeCacheOffset));

  // Setup the four remaining stack slots.
  __ push(rax);  // Map.
  __ push(rdx);  // Enumeration cache.
  __ movq(rax, FieldOperand(rdx, FixedArray::kLengthOffset));
  __ push(rax);  // Enumeration cache length (as smi).
  __ Push(Smi::FromInt(0));  // Initial index.
  __ jmp(&loop);

  // We got a fixed array in register rax. Iterate through that.
  __ bind(&fixed_array);
  __ Push(Smi::FromInt(0));  // Map (0) - force slow check.
  __ push(rax);
  __ movq(rax, FieldOperand(rax, FixedArray::kLengthOffset));
  __ push(rax);  // Fixed array length (as smi).
  __ Push(Smi::FromInt(0));  // Initial index.

  // Generate code for doing the condition check.
  __ bind(&loop);
  __ movq(rax, Operand(rsp, 0 * kPointerSize));  // Get the current index.
  __ cmpq(rax, Operand(rsp, 1 * kPointerSize));  // Compare to the array length.
  __ j(above_equal, loop_statement.break_target());

  // Get the current entry of the array into register rbx.
  __ movq(rbx, Operand(rsp, 2 * kPointerSize));
  SmiIndex index = __ SmiToIndex(rax, rax, kPointerSizeLog2);
  __ movq(rbx, FieldOperand(rbx,
                            index.reg,
                            index.scale,
                            FixedArray::kHeaderSize));

  // Get the expected map from the stack or a zero map in the
  // permanent slow case into register rdx.
  __ movq(rdx, Operand(rsp, 3 * kPointerSize));

  // Check if the expected map still matches that of the enumerable.
  // If not, we have to filter the key.
  Label update_each;
  __ movq(rcx, Operand(rsp, 4 * kPointerSize));
  __ cmpq(rdx, FieldOperand(rcx, HeapObject::kMapOffset));
  __ j(equal, &update_each);

  // Convert the entry to a string or null if it isn't a property
  // anymore. If the property has been removed while iterating, we
  // just skip it.
  __ push(rcx);  // Enumerable.
  __ push(rbx);  // Current entry.
  __ InvokeBuiltin(Builtins::FILTER_KEY, CALL_FUNCTION);
  __ CompareRoot(rax, Heap::kNullValueRootIndex);
  __ j(equal, loop_statement.continue_target());
  __ movq(rbx, rax);

  // Update the 'each' property or variable from the possibly filtered
  // entry in register rbx.
  __ bind(&update_each);
  __ movq(result_register(), rbx);
  // Perform the assignment as if via '='.
  EmitAssignment(stmt->each());

  // Generate code for the body of the loop.
  Label stack_limit_hit, stack_check_done;
  Visit(stmt->body());

  __ StackLimitCheck(&stack_limit_hit);
  __ bind(&stack_check_done);

  // Generate code for going to the next element by incrementing the
  // index (smi) stored on top of the stack.
  __ bind(loop_statement.continue_target());
  __ SmiAddConstant(Operand(rsp, 0 * kPointerSize), Smi::FromInt(1));
  __ jmp(&loop);

  // Slow case for the stack limit check.
  StackCheckStub stack_check_stub;
  __ bind(&stack_limit_hit);
  __ CallStub(&stack_check_stub);
  __ jmp(&stack_check_done);

  // Remove the pointers stored on the stack.
  __ bind(loop_statement.break_target());
  __ addq(rsp, Immediate(5 * kPointerSize));

  // Exit and decrement the loop depth.
  __ bind(&exit);
  decrement_loop_depth();
}


void FullCodeGenerator::EmitNewClosure(Handle<SharedFunctionInfo> info) {
  // Use the fast case closure allocation code that allocates in new
  // space for nested functions that don't need literals cloning.
  if (scope()->is_function_scope() && info->num_literals() == 0) {
    FastNewClosureStub stub;
    __ Push(info);
    __ CallStub(&stub);
  } else {
    __ push(rsi);
    __ Push(info);
    __ CallRuntime(Runtime::kNewClosure, 2);
  }
  Apply(context_, rax);
}


void FullCodeGenerator::VisitVariableProxy(VariableProxy* expr) {
  Comment cmnt(masm_, "[ VariableProxy");
  EmitVariableLoad(expr->var(), context_);
}


void FullCodeGenerator::EmitVariableLoad(Variable* var,
                                         Expression::Context context) {
  // Four cases: non-this global variables, lookup slots, all other
  // types of slots, and parameters that rewrite to explicit property
  // accesses on the arguments object.
  Slot* slot = var->slot();
  Property* property = var->AsProperty();

  if (var->is_global() && !var->is_this()) {
    Comment cmnt(masm_, "Global variable");
    // Use inline caching. Variable name is passed in rcx and the global
    // object on the stack.
    __ Move(rcx, var->name());
    __ movq(rax, CodeGenerator::GlobalObject());
    Handle<Code> ic(Builtins::builtin(Builtins::LoadIC_Initialize));
    __ Call(ic, RelocInfo::CODE_TARGET_CONTEXT);
    // A test rax instruction following the call is used by the IC to
    // indicate that the inobject property case was inlined.  Ensure there
    // is no test rax instruction here.
    __ nop();
    Apply(context, rax);

  } else if (slot != NULL && slot->type() == Slot::LOOKUP) {
    Comment cmnt(masm_, "Lookup slot");
    __ push(rsi);  // Context.
    __ Push(var->name());
    __ CallRuntime(Runtime::kLoadContextSlot, 2);
    Apply(context, rax);

  } else if (slot != NULL) {
    Comment cmnt(masm_, (slot->type() == Slot::CONTEXT)
                            ? "Context slot"
                            : "Stack slot");
    if (var->mode() == Variable::CONST) {
      // Constants may be the hole value if they have not been initialized.
      // Unhole them.
      Label done;
      MemOperand slot_operand = EmitSlotSearch(slot, rax);
      __ movq(rax, slot_operand);
      __ CompareRoot(rax, Heap::kTheHoleValueRootIndex);
      __ j(not_equal, &done);
      __ LoadRoot(rax, Heap::kUndefinedValueRootIndex);
      __ bind(&done);
      Apply(context, rax);
    } else {
      Apply(context, slot);
    }

  } else {
    Comment cmnt(masm_, "Rewritten parameter");
    ASSERT_NOT_NULL(property);
    // Rewritten parameter accesses are of the form "slot[literal]".

    // Assert that the object is in a slot.
    Variable* object_var = property->obj()->AsVariableProxy()->AsVariable();
    ASSERT_NOT_NULL(object_var);
    Slot* object_slot = object_var->slot();
    ASSERT_NOT_NULL(object_slot);

    // Load the object.
    MemOperand object_loc = EmitSlotSearch(object_slot, rax);
    __ movq(rdx, object_loc);

    // Assert that the key is a smi.
    Literal* key_literal = property->key()->AsLiteral();
    ASSERT_NOT_NULL(key_literal);
    ASSERT(key_literal->handle()->IsSmi());

    // Load the key.
    __ Move(rax, key_literal->handle());

    // Do a keyed property load.
    Handle<Code> ic(Builtins::builtin(Builtins::KeyedLoadIC_Initialize));
    __ call(ic, RelocInfo::CODE_TARGET);
    // Notice: We must not have a "test rax, ..." instruction after the
    // call. It is treated specially by the LoadIC code.
    __ nop();
    Apply(context, rax);
  }
}


void FullCodeGenerator::VisitRegExpLiteral(RegExpLiteral* expr) {
  Comment cmnt(masm_, "[ RegExpLiteral");
  Label done;
  // Registers will be used as follows:
  // rdi = JS function.
  // rbx = literals array.
  // rax = regexp literal.
  __ movq(rdi, Operand(rbp, JavaScriptFrameConstants::kFunctionOffset));
  __ movq(rbx, FieldOperand(rdi, JSFunction::kLiteralsOffset));
  int literal_offset =
    FixedArray::kHeaderSize + expr->literal_index() * kPointerSize;
  __ movq(rax, FieldOperand(rbx, literal_offset));
  __ CompareRoot(rax, Heap::kUndefinedValueRootIndex);
  __ j(not_equal, &done);
  // Create regexp literal using runtime function
  // Result will be in rax.
  __ push(rbx);
  __ Push(Smi::FromInt(expr->literal_index()));
  __ Push(expr->pattern());
  __ Push(expr->flags());
  __ CallRuntime(Runtime::kMaterializeRegExpLiteral, 4);
  __ bind(&done);
  Apply(context_, rax);
}


void FullCodeGenerator::VisitObjectLiteral(ObjectLiteral* expr) {
  Comment cmnt(masm_, "[ ObjectLiteral");
  __ movq(rdi, Operand(rbp, JavaScriptFrameConstants::kFunctionOffset));
  __ push(FieldOperand(rdi, JSFunction::kLiteralsOffset));
  __ Push(Smi::FromInt(expr->literal_index()));
  __ Push(expr->constant_properties());
  __ Push(Smi::FromInt(expr->fast_elements() ? 1 : 0));
  if (expr->depth() > 1) {
    __ CallRuntime(Runtime::kCreateObjectLiteral, 4);
  } else {
    __ CallRuntime(Runtime::kCreateObjectLiteralShallow, 4);
  }

  // If result_saved is true the result is on top of the stack.  If
  // result_saved is false the result is in rax.
  bool result_saved = false;

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
          VisitForValue(value, kAccumulator);
          __ Move(rcx, key->handle());
          __ movq(rdx, Operand(rsp, 0));
          Handle<Code> ic(Builtins::builtin(Builtins::StoreIC_Initialize));
          __ call(ic, RelocInfo::CODE_TARGET);
          __ nop();
          break;
        }
        // Fall through.
      case ObjectLiteral::Property::PROTOTYPE:
        __ push(Operand(rsp, 0));  // Duplicate receiver.
        VisitForValue(key, kStack);
        VisitForValue(value, kStack);
        __ CallRuntime(Runtime::kSetProperty, 3);
        break;
      case ObjectLiteral::Property::SETTER:
      case ObjectLiteral::Property::GETTER:
        __ push(Operand(rsp, 0));  // Duplicate receiver.
        VisitForValue(key, kStack);
        __ Push(property->kind() == ObjectLiteral::Property::SETTER ?
                Smi::FromInt(1) :
                Smi::FromInt(0));
        VisitForValue(value, kStack);
        __ CallRuntime(Runtime::kDefineAccessor, 4);
        break;
    }
  }

  if (result_saved) {
    ApplyTOS(context_);
  } else {
    Apply(context_, rax);
  }
}


void FullCodeGenerator::VisitArrayLiteral(ArrayLiteral* expr) {
  Comment cmnt(masm_, "[ ArrayLiteral");

  ZoneList<Expression*>* subexprs = expr->values();
  int length = subexprs->length();

  __ movq(rbx, Operand(rbp, JavaScriptFrameConstants::kFunctionOffset));
  __ push(FieldOperand(rbx, JSFunction::kLiteralsOffset));
  __ Push(Smi::FromInt(expr->literal_index()));
  __ Push(expr->constant_elements());
  if (expr->depth() > 1) {
    __ CallRuntime(Runtime::kCreateArrayLiteral, 3);
  } else if (length > FastCloneShallowArrayStub::kMaximumLength) {
    __ CallRuntime(Runtime::kCreateArrayLiteralShallow, 3);
  } else {
    FastCloneShallowArrayStub stub(length);
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
    VisitForValue(subexpr, kAccumulator);

    // Store the subexpression value in the array's elements.
    __ movq(rbx, Operand(rsp, 0));  // Copy of array literal.
    __ movq(rbx, FieldOperand(rbx, JSObject::kElementsOffset));
    int offset = FixedArray::kHeaderSize + (i * kPointerSize);
    __ movq(FieldOperand(rbx, offset), result_register());

    // Update the write barrier for the array store.
    __ RecordWrite(rbx, offset, result_register(), rcx);
  }

  if (result_saved) {
    ApplyTOS(context_);
  } else {
    Apply(context_, rax);
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
  // slot. Variables with rewrite to .arguments are treated as KEYED_PROPERTY.
  enum LhsKind { VARIABLE, NAMED_PROPERTY, KEYED_PROPERTY };
  LhsKind assign_type = VARIABLE;
  Property* prop = expr->target()->AsProperty();
  if (prop != NULL) {
    assign_type =
        (prop->key()->IsPropertyName()) ? NAMED_PROPERTY : KEYED_PROPERTY;
  }

  // Evaluate LHS expression.
  switch (assign_type) {
    case VARIABLE:
      // Nothing to do here.
      break;
    case NAMED_PROPERTY:
      if (expr->is_compound()) {
        // We need the receiver both on the stack and in the accumulator.
        VisitForValue(prop->obj(), kAccumulator);
        __ push(result_register());
      } else {
        VisitForValue(prop->obj(), kStack);
      }
      break;
    case KEYED_PROPERTY:
      if (expr->is_compound()) {
        VisitForValue(prop->obj(), kStack);
        VisitForValue(prop->key(), kAccumulator);
        __ movq(rdx, Operand(rsp, 0));
        __ push(rax);
      } else {
        VisitForValue(prop->obj(), kStack);
        VisitForValue(prop->key(), kStack);
      }
      break;
  }

  // If we have a compound assignment: Get value of LHS expression and
  // store in on top of the stack.
  if (expr->is_compound()) {
    Location saved_location = location_;
    location_ = kStack;
    switch (assign_type) {
      case VARIABLE:
        EmitVariableLoad(expr->target()->AsVariableProxy()->var(),
                         Expression::kValue);
        break;
      case NAMED_PROPERTY:
        EmitNamedPropertyLoad(prop);
        __ push(result_register());
        break;
      case KEYED_PROPERTY:
        EmitKeyedPropertyLoad(prop);
        __ push(result_register());
        break;
    }
    location_ = saved_location;
  }

  // Evaluate RHS expression.
  Expression* rhs = expr->value();
  VisitForValue(rhs, kAccumulator);

  // If we have a compound assignment: Apply operator.
  if (expr->is_compound()) {
    Location saved_location = location_;
    location_ = kAccumulator;
    EmitBinaryOp(expr->binary_op(), Expression::kValue);
    location_ = saved_location;
  }

  // Record source position before possible IC call.
  SetSourcePosition(expr->position());

  // Store the value.
  switch (assign_type) {
    case VARIABLE:
      EmitVariableAssignment(expr->target()->AsVariableProxy()->var(),
                             expr->op(),
                             context_);
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
  Handle<Code> ic(Builtins::builtin(Builtins::LoadIC_Initialize));
  __ Call(ic, RelocInfo::CODE_TARGET);
  __ nop();
}


void FullCodeGenerator::EmitKeyedPropertyLoad(Property* prop) {
  SetSourcePosition(prop->position());
  Handle<Code> ic(Builtins::builtin(Builtins::KeyedLoadIC_Initialize));
  __ Call(ic, RelocInfo::CODE_TARGET);
  __ nop();
}


void FullCodeGenerator::EmitBinaryOp(Token::Value op,
                                     Expression::Context context) {
  __ push(result_register());
  GenericBinaryOpStub stub(op,
                           NO_OVERWRITE,
                           NO_GENERIC_BINARY_FLAGS);
  __ CallStub(&stub);
  Apply(context, rax);
}


void FullCodeGenerator::EmitAssignment(Expression* expr) {
  // Invalid left-hand sides are rewritten to have a 'throw
  // ReferenceError' on the left-hand side.
  if (!expr->IsValidLeftHandSide()) {
    VisitForEffect(expr);
    return;
  }

  // Left-hand side can only be a property, a global or a (parameter or local)
  // slot. Variables with rewrite to .arguments are treated as KEYED_PROPERTY.
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
      EmitVariableAssignment(var, Token::ASSIGN, Expression::kEffect);
      break;
    }
    case NAMED_PROPERTY: {
      __ push(rax);  // Preserve value.
      VisitForValue(prop->obj(), kAccumulator);
      __ movq(rdx, rax);
      __ pop(rax);  // Restore value.
      __ Move(rcx, prop->key()->AsLiteral()->handle());
      Handle<Code> ic(Builtins::builtin(Builtins::StoreIC_Initialize));
      __ call(ic, RelocInfo::CODE_TARGET);
      __ nop();  // Signal no inlined code.
      break;
    }
    case KEYED_PROPERTY: {
      __ push(rax);  // Preserve value.
      VisitForValue(prop->obj(), kStack);
      VisitForValue(prop->key(), kAccumulator);
      __ movq(rcx, rax);
      __ pop(rdx);
      __ pop(rax);
      Handle<Code> ic(Builtins::builtin(Builtins::KeyedStoreIC_Initialize));
      __ call(ic, RelocInfo::CODE_TARGET);
      __ nop();  // Signal no inlined code.
      break;
    }
  }
}


void FullCodeGenerator::EmitVariableAssignment(Variable* var,
                                               Token::Value op,
                                               Expression::Context context) {
  // Left-hand sides that rewrite to explicit property accesses do not reach
  // here.
  ASSERT(var != NULL);
  ASSERT(var->is_global() || var->slot() != NULL);

  if (var->is_global()) {
    ASSERT(!var->is_this());
    // Assignment to a global variable.  Use inline caching for the
    // assignment.  Right-hand-side value is passed in rax, variable name in
    // rcx, and the global object on the stack.
    __ Move(rcx, var->name());
    __ movq(rdx, CodeGenerator::GlobalObject());
    Handle<Code> ic(Builtins::builtin(Builtins::StoreIC_Initialize));
    __ Call(ic, RelocInfo::CODE_TARGET);
    __ nop();

  } else if (var->mode() != Variable::CONST || op == Token::INIT_CONST) {
    // Perform the assignment for non-const variables and for initialization
    // of const variables.  Const assignments are simply skipped.
    Label done;
    Slot* slot = var->slot();
    switch (slot->type()) {
      case Slot::PARAMETER:
      case Slot::LOCAL:
        if (op == Token::INIT_CONST) {
          // Detect const reinitialization by checking for the hole value.
          __ movq(rdx, Operand(rbp, SlotOffset(slot)));
          __ CompareRoot(rdx, Heap::kTheHoleValueRootIndex);
          __ j(not_equal, &done);
        }
        // Perform the assignment.
        __ movq(Operand(rbp, SlotOffset(slot)), rax);
        break;

      case Slot::CONTEXT: {
        MemOperand target = EmitSlotSearch(slot, rcx);
        if (op == Token::INIT_CONST) {
          // Detect const reinitialization by checking for the hole value.
          __ movq(rdx, target);
          __ CompareRoot(rdx, Heap::kTheHoleValueRootIndex);
          __ j(not_equal, &done);
        }
        // Perform the assignment and issue the write barrier.
        __ movq(target, rax);
        // The value of the assignment is in rax.  RecordWrite clobbers its
        // register arguments.
        __ movq(rdx, rax);
        int offset = FixedArray::kHeaderSize + slot->index() * kPointerSize;
        __ RecordWrite(rcx, offset, rdx, rbx);
        break;
      }

      case Slot::LOOKUP:
        // Call the runtime for the assignment.  The runtime will ignore
        // const reinitialization.
        __ push(rax);  // Value.
        __ push(rsi);  // Context.
        __ Push(var->name());
        if (op == Token::INIT_CONST) {
          // The runtime will ignore const redeclaration.
          __ CallRuntime(Runtime::kInitializeConstContextSlot, 3);
        } else {
          __ CallRuntime(Runtime::kStoreContextSlot, 3);
        }
        break;
    }
    __ bind(&done);
  }

  Apply(context, rax);
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
  Handle<Code> ic(Builtins::builtin(Builtins::StoreIC_Initialize));
  __ Call(ic, RelocInfo::CODE_TARGET);
  __ nop();

  // If the assignment ends an initialization block, revert to fast case.
  if (expr->ends_initialization_block()) {
    __ push(rax);  // Result of assignment, saved even if not needed.
    __ push(Operand(rsp, kPointerSize));  // Receiver is under value.
    __ CallRuntime(Runtime::kToFastProperties, 1);
    __ pop(rax);
    DropAndApply(1, context_, rax);
  } else {
    Apply(context_, rax);
  }
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
  Handle<Code> ic(Builtins::builtin(Builtins::KeyedStoreIC_Initialize));
  __ Call(ic, RelocInfo::CODE_TARGET);
  // This nop signals to the IC that there is no inlined code at the call
  // site for it to patch.
  __ nop();

  // If the assignment ends an initialization block, revert to fast case.
  if (expr->ends_initialization_block()) {
    __ pop(rdx);
    __ push(rax);  // Result of assignment, saved even if not needed.
    __ push(rdx);
    __ CallRuntime(Runtime::kToFastProperties, 1);
    __ pop(rax);
  }

  Apply(context_, rax);
}


void FullCodeGenerator::VisitProperty(Property* expr) {
  Comment cmnt(masm_, "[ Property");
  Expression* key = expr->key();

  if (key->IsPropertyName()) {
    VisitForValue(expr->obj(), kAccumulator);
    EmitNamedPropertyLoad(expr);
    Apply(context_, rax);
  } else {
    VisitForValue(expr->obj(), kStack);
    VisitForValue(expr->key(), kAccumulator);
    __ pop(rdx);
    EmitKeyedPropertyLoad(expr);
    Apply(context_, rax);
  }
}


void FullCodeGenerator::EmitCallWithIC(Call* expr,
                                       Handle<Object> name,
                                       RelocInfo::Mode mode) {
  // Code common for calls using the IC.
  ZoneList<Expression*>* args = expr->arguments();
  int arg_count = args->length();
  for (int i = 0; i < arg_count; i++) {
    VisitForValue(args->at(i), kStack);
  }
  __ Move(rcx, name);
  // Record source position for debugger.
  SetSourcePosition(expr->position());
  // Call the IC initialization code.
  InLoopFlag in_loop = (loop_depth() > 0) ? IN_LOOP : NOT_IN_LOOP;
  Handle<Code> ic = CodeGenerator::ComputeCallInitialize(arg_count,
                                                         in_loop);
  __ Call(ic, mode);
  // Restore context register.
  __ movq(rsi, Operand(rbp, StandardFrameConstants::kContextOffset));
  Apply(context_, rax);
}


void FullCodeGenerator::EmitKeyedCallWithIC(Call* expr,
                                            Expression* key,
                                            RelocInfo::Mode mode) {
  // Code common for calls using the IC.
  ZoneList<Expression*>* args = expr->arguments();
  int arg_count = args->length();
  for (int i = 0; i < arg_count; i++) {
    VisitForValue(args->at(i), kStack);
  }
  VisitForValue(key, kAccumulator);
  __ movq(rcx, rax);
  // Record source position for debugger.
  SetSourcePosition(expr->position());
  // Call the IC initialization code.
  InLoopFlag in_loop = (loop_depth() > 0) ? IN_LOOP : NOT_IN_LOOP;
  Handle<Code> ic = CodeGenerator::ComputeKeyedCallInitialize(arg_count,
                                                              in_loop);
  __ Call(ic, mode);
  // Restore context register.
  __ movq(rsi, Operand(rbp, StandardFrameConstants::kContextOffset));
  Apply(context_, rax);
}


void FullCodeGenerator::EmitCallWithStub(Call* expr) {
  // Code common for calls using the call stub.
  ZoneList<Expression*>* args = expr->arguments();
  int arg_count = args->length();
  for (int i = 0; i < arg_count; i++) {
    VisitForValue(args->at(i), kStack);
  }
  // Record source position for debugger.
  SetSourcePosition(expr->position());
  InLoopFlag in_loop = (loop_depth() > 0) ? IN_LOOP : NOT_IN_LOOP;
  CallFunctionStub stub(arg_count, in_loop, RECEIVER_MIGHT_BE_VALUE);
  __ CallStub(&stub);
  // Restore context register.
  __ movq(rsi, Operand(rbp, StandardFrameConstants::kContextOffset));
  // Discard the function left on TOS.
  DropAndApply(1, context_, rax);
}


void FullCodeGenerator::VisitCall(Call* expr) {
  Comment cmnt(masm_, "[ Call");
  Expression* fun = expr->expression();
  Variable* var = fun->AsVariableProxy()->AsVariable();

  if (var != NULL && var->is_possibly_eval()) {
    // In a call to eval, we first call %ResolvePossiblyDirectEval to
    // resolve the function we need to call and the receiver of the
    // call.  The we call the resolved function using the given
    // arguments.
    VisitForValue(fun, kStack);
    __ PushRoot(Heap::kUndefinedValueRootIndex);  // Reserved receiver slot.

    // Push the arguments.
    ZoneList<Expression*>* args = expr->arguments();
    int arg_count = args->length();
    for (int i = 0; i < arg_count; i++) {
      VisitForValue(args->at(i), kStack);
    }

    // Push copy of the function - found below the arguments.
    __ push(Operand(rsp, (arg_count + 1) * kPointerSize));

    // Push copy of the first argument or undefined if it doesn't exist.
    if (arg_count > 0) {
      __ push(Operand(rsp, arg_count * kPointerSize));
    } else {
      __ PushRoot(Heap::kUndefinedValueRootIndex);
    }

    // Push the receiver of the enclosing function and do runtime call.
    __ push(Operand(rbp, (2 + scope()->num_parameters()) * kPointerSize));
    __ CallRuntime(Runtime::kResolvePossiblyDirectEval, 3);

    // The runtime call returns a pair of values in rax (function) and
    // rdx (receiver). Touch up the stack with the right values.
    __ movq(Operand(rsp, (arg_count + 0) * kPointerSize), rdx);
    __ movq(Operand(rsp, (arg_count + 1) * kPointerSize), rax);

    // Record source position for debugger.
    SetSourcePosition(expr->position());
    InLoopFlag in_loop = (loop_depth() > 0) ? IN_LOOP : NOT_IN_LOOP;
    CallFunctionStub stub(arg_count, in_loop, RECEIVER_MIGHT_BE_VALUE);
    __ CallStub(&stub);
    // Restore context register.
    __ movq(rsi, Operand(rbp, StandardFrameConstants::kContextOffset));
    DropAndApply(1, context_, rax);
  } else if (var != NULL && !var->is_this() && var->is_global()) {
    // Call to a global variable.
    // Push global object as receiver for the call IC lookup.
    __ push(CodeGenerator::GlobalObject());
    EmitCallWithIC(expr, var->name(), RelocInfo::CODE_TARGET_CONTEXT);
  } else if (var != NULL && var->slot() != NULL &&
             var->slot()->type() == Slot::LOOKUP) {
    // Call to a lookup slot (dynamically introduced variable).  Call
    // the runtime to find the function to call (returned in rax) and
    // the object holding it (returned in rdx).
    __ push(context_register());
    __ Push(var->name());
    __ CallRuntime(Runtime::kLoadContextSlot, 2);
    __ push(rax);  // Function.
    __ push(rdx);  // Receiver.
    EmitCallWithStub(expr);
  } else if (fun->AsProperty() != NULL) {
    // Call to an object property.
    Property* prop = fun->AsProperty();
    Literal* key = prop->key()->AsLiteral();
    if (key != NULL && key->handle()->IsSymbol()) {
      // Call to a named property, use call IC.
      VisitForValue(prop->obj(), kStack);
      EmitCallWithIC(expr, key->handle(), RelocInfo::CODE_TARGET);
    } else {
      // Call to a keyed property.
      // For a synthetic property use keyed load IC followed by function call,
      // for a regular property use KeyedCallIC.
      VisitForValue(prop->obj(), kStack);
      if (prop->is_synthetic()) {
        VisitForValue(prop->key(), kAccumulator);
        __ movq(rdx, Operand(rsp, 0));
        // Record source code position for IC call.
        SetSourcePosition(prop->position());
        Handle<Code> ic(Builtins::builtin(Builtins::KeyedLoadIC_Initialize));
        __ call(ic, RelocInfo::CODE_TARGET);
        // By emitting a nop we make sure that we do not have a "test rax,..."
        // instruction after the call as it is treated specially
        // by the LoadIC code.
        __ nop();
        // Pop receiver.
        __ pop(rbx);
        // Push result (function).
        __ push(rax);
        // Push receiver object on stack.
        __ movq(rcx, CodeGenerator::GlobalObject());
        __ push(FieldOperand(rcx, GlobalObject::kGlobalReceiverOffset));
        EmitCallWithStub(expr);
      } else {
        EmitKeyedCallWithIC(expr, prop->key(), RelocInfo::CODE_TARGET);
      }
    }
  } else {
    // Call to some other expression.  If the expression is an anonymous
    // function literal not called in a loop, mark it as one that should
    // also use the fast code generator.
    FunctionLiteral* lit = fun->AsFunctionLiteral();
    if (lit != NULL &&
        lit->name()->Equals(Heap::empty_string()) &&
        loop_depth() == 0) {
      lit->set_try_full_codegen(true);
    }
    VisitForValue(fun, kStack);
    // Load global receiver object.
    __ movq(rbx, CodeGenerator::GlobalObject());
    __ push(FieldOperand(rbx, GlobalObject::kGlobalReceiverOffset));
    // Emit function call.
    EmitCallWithStub(expr);
  }
}


void FullCodeGenerator::VisitCallNew(CallNew* expr) {
  Comment cmnt(masm_, "[ CallNew");
  // According to ECMA-262, section 11.2.2, page 44, the function
  // expression in new calls must be evaluated before the
  // arguments.
  // Push function on the stack.
  VisitForValue(expr->expression(), kStack);

  // Push global object (receiver).
  __ push(CodeGenerator::GlobalObject());

  // Push the arguments ("left-to-right") on the stack.
  ZoneList<Expression*>* args = expr->arguments();
  int arg_count = args->length();
  for (int i = 0; i < arg_count; i++) {
    VisitForValue(args->at(i), kStack);
  }

  // Call the construct call builtin that handles allocation and
  // constructor invocation.
  SetSourcePosition(expr->position());

  // Load function, arg_count into rdi and rax.
  __ Set(rax, arg_count);
  // Function is in rsp[arg_count + 1].
  __ movq(rdi, Operand(rsp, rax, times_pointer_size, kPointerSize));

  Handle<Code> construct_builtin(Builtins::builtin(Builtins::JSConstructCall));
  __ Call(construct_builtin, RelocInfo::CONSTRUCT_CALL);

  // Replace function on TOS with result in rax, or pop it.
  DropAndApply(1, context_, rax);
}


void FullCodeGenerator::EmitIsSmi(ZoneList<Expression*>* args) {
  ASSERT(args->length() == 1);

  VisitForValue(args->at(0), kAccumulator);

  Label materialize_true, materialize_false;
  Label* if_true = NULL;
  Label* if_false = NULL;
  PrepareTest(&materialize_true, &materialize_false, &if_true, &if_false);

  __ JumpIfSmi(rax, if_true);
  __ jmp(if_false);

  Apply(context_, if_true, if_false);
}


void FullCodeGenerator::EmitIsNonNegativeSmi(ZoneList<Expression*>* args) {
  ASSERT(args->length() == 1);

  VisitForValue(args->at(0), kAccumulator);

  Label materialize_true, materialize_false;
  Label* if_true = NULL;
  Label* if_false = NULL;
  PrepareTest(&materialize_true, &materialize_false, &if_true, &if_false);

  Condition positive_smi = __ CheckPositiveSmi(rax);
  __ j(positive_smi, if_true);
  __ jmp(if_false);

  Apply(context_, if_true, if_false);
}


void FullCodeGenerator::EmitIsObject(ZoneList<Expression*>* args) {
  ASSERT(args->length() == 1);

  VisitForValue(args->at(0), kAccumulator);

  Label materialize_true, materialize_false;
  Label* if_true = NULL;
  Label* if_false = NULL;
  PrepareTest(&materialize_true, &materialize_false, &if_true, &if_false);

  __ JumpIfSmi(rax, if_false);
  __ CompareRoot(rax, Heap::kNullValueRootIndex);
  __ j(equal, if_true);
  __ movq(rbx, FieldOperand(rax, HeapObject::kMapOffset));
  // Undetectable objects behave like undefined when tested with typeof.
  __ testb(FieldOperand(rbx, Map::kBitFieldOffset),
           Immediate(1 << Map::kIsUndetectable));
  __ j(not_zero, if_false);
  __ movzxbq(rbx, FieldOperand(rbx, Map::kInstanceTypeOffset));
  __ cmpq(rbx, Immediate(FIRST_JS_OBJECT_TYPE));
  __ j(below, if_false);
  __ cmpq(rbx, Immediate(LAST_JS_OBJECT_TYPE));
  __ j(below_equal, if_true);
  __ jmp(if_false);

  Apply(context_, if_true, if_false);
}


void FullCodeGenerator::EmitIsUndetectableObject(ZoneList<Expression*>* args) {
  ASSERT(args->length() == 1);

  VisitForValue(args->at(0), kAccumulator);

  Label materialize_true, materialize_false;
  Label* if_true = NULL;
  Label* if_false = NULL;
  PrepareTest(&materialize_true, &materialize_false, &if_true, &if_false);

  __ JumpIfSmi(rax, if_false);
  __ movq(rbx, FieldOperand(rax, HeapObject::kMapOffset));
  __ testb(FieldOperand(rbx, Map::kBitFieldOffset),
           Immediate(1 << Map::kIsUndetectable));
  __ j(not_zero, if_true);
  __ jmp(if_false);

  Apply(context_, if_true, if_false);
}


void FullCodeGenerator::EmitIsFunction(ZoneList<Expression*>* args) {
  ASSERT(args->length() == 1);

  VisitForValue(args->at(0), kAccumulator);

  Label materialize_true, materialize_false;
  Label* if_true = NULL;
  Label* if_false = NULL;
  PrepareTest(&materialize_true, &materialize_false, &if_true, &if_false);

  __ JumpIfSmi(rax, if_false);
  __ CmpObjectType(rax, JS_FUNCTION_TYPE, rbx);
  __ j(equal, if_true);
  __ jmp(if_false);

  Apply(context_, if_true, if_false);
}


void FullCodeGenerator::EmitIsArray(ZoneList<Expression*>* args) {
  ASSERT(args->length() == 1);

  VisitForValue(args->at(0), kAccumulator);

  Label materialize_true, materialize_false;
  Label* if_true = NULL;
  Label* if_false = NULL;
  PrepareTest(&materialize_true, &materialize_false, &if_true, &if_false);

  __ JumpIfSmi(rax, if_false);
  __ CmpObjectType(rax, JS_ARRAY_TYPE, rbx);
  __ j(equal, if_true);
  __ jmp(if_false);

  Apply(context_, if_true, if_false);
}


void FullCodeGenerator::EmitIsRegExp(ZoneList<Expression*>* args) {
  ASSERT(args->length() == 1);

  VisitForValue(args->at(0), kAccumulator);

  Label materialize_true, materialize_false;
  Label* if_true = NULL;
  Label* if_false = NULL;
  PrepareTest(&materialize_true, &materialize_false, &if_true, &if_false);

  __ JumpIfSmi(rax, if_false);
  __ CmpObjectType(rax, JS_REGEXP_TYPE, rbx);
  __ j(equal, if_true);
  __ jmp(if_false);

  Apply(context_, if_true, if_false);
}



void FullCodeGenerator::EmitIsConstructCall(ZoneList<Expression*>* args) {
  ASSERT(args->length() == 0);

  Label materialize_true, materialize_false;
  Label* if_true = NULL;
  Label* if_false = NULL;
  PrepareTest(&materialize_true, &materialize_false, &if_true, &if_false);

  // Get the frame pointer for the calling frame.
  __ movq(rax, Operand(rbp, StandardFrameConstants::kCallerFPOffset));

  // Skip the arguments adaptor frame if it exists.
  Label check_frame_marker;
  __ SmiCompare(Operand(rax, StandardFrameConstants::kContextOffset),
                Smi::FromInt(StackFrame::ARGUMENTS_ADAPTOR));
  __ j(not_equal, &check_frame_marker);
  __ movq(rax, Operand(rax, StandardFrameConstants::kCallerFPOffset));

  // Check the marker in the calling frame.
  __ bind(&check_frame_marker);
  __ SmiCompare(Operand(rax, StandardFrameConstants::kMarkerOffset),
                Smi::FromInt(StackFrame::CONSTRUCT));
  __ j(equal, if_true);
  __ jmp(if_false);

  Apply(context_, if_true, if_false);
}


void FullCodeGenerator::EmitObjectEquals(ZoneList<Expression*>* args) {
  ASSERT(args->length() == 2);

  // Load the two objects into registers and perform the comparison.
  VisitForValue(args->at(0), kStack);
  VisitForValue(args->at(1), kAccumulator);

  Label materialize_true, materialize_false;
  Label* if_true = NULL;
  Label* if_false = NULL;
  PrepareTest(&materialize_true, &materialize_false, &if_true, &if_false);

  __ pop(rbx);
  __ cmpq(rax, rbx);
  __ j(equal, if_true);
  __ jmp(if_false);

  Apply(context_, if_true, if_false);
}


void FullCodeGenerator::EmitArguments(ZoneList<Expression*>* args) {
  ASSERT(args->length() == 1);

  // ArgumentsAccessStub expects the key in edx and the formal
  // parameter count in eax.
  VisitForValue(args->at(0), kAccumulator);
  __ movq(rdx, rax);
  __ Move(rax, Smi::FromInt(scope()->num_parameters()));
  ArgumentsAccessStub stub(ArgumentsAccessStub::READ_ELEMENT);
  __ CallStub(&stub);
  Apply(context_, rax);
}


void FullCodeGenerator::EmitArgumentsLength(ZoneList<Expression*>* args) {
  ASSERT(args->length() == 0);

  Label exit;
  // Get the number of formal parameters.
  __ Move(rax, Smi::FromInt(scope()->num_parameters()));

  // Check if the calling frame is an arguments adaptor frame.
  __ movq(rbx, Operand(rbp, StandardFrameConstants::kCallerFPOffset));
  __ SmiCompare(Operand(rbx, StandardFrameConstants::kContextOffset),
                Smi::FromInt(StackFrame::ARGUMENTS_ADAPTOR));
  __ j(not_equal, &exit);

  // Arguments adaptor case: Read the arguments length from the
  // adaptor frame.
  __ movq(rax, Operand(rbx, ArgumentsAdaptorFrameConstants::kLengthOffset));

  __ bind(&exit);
  if (FLAG_debug_code) __ AbortIfNotSmi(rax);
  Apply(context_, rax);
}


void FullCodeGenerator::EmitClassOf(ZoneList<Expression*>* args) {
  ASSERT(args->length() == 1);
  Label done, null, function, non_function_constructor;

  VisitForValue(args->at(0), kAccumulator);

  // If the object is a smi, we return null.
  __ JumpIfSmi(rax, &null);

  // Check that the object is a JS object but take special care of JS
  // functions to make sure they have 'Function' as their class.
  __ CmpObjectType(rax, FIRST_JS_OBJECT_TYPE, rax);  // Map is now in rax.
  __ j(below, &null);

  // As long as JS_FUNCTION_TYPE is the last instance type and it is
  // right after LAST_JS_OBJECT_TYPE, we can avoid checking for
  // LAST_JS_OBJECT_TYPE.
  ASSERT(LAST_TYPE == JS_FUNCTION_TYPE);
  ASSERT(JS_FUNCTION_TYPE == LAST_JS_OBJECT_TYPE + 1);
  __ CmpInstanceType(rax, JS_FUNCTION_TYPE);
  __ j(equal, &function);

  // Check if the constructor in the map is a function.
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
  __ Move(rax, Factory::function_class_symbol());
  __ jmp(&done);

  // Objects with a non-function constructor have class 'Object'.
  __ bind(&non_function_constructor);
  __ Move(rax, Factory::Object_symbol());
  __ jmp(&done);

  // Non-JS objects have class null.
  __ bind(&null);
  __ LoadRoot(rax, Heap::kNullValueRootIndex);

  // All done.
  __ bind(&done);

  Apply(context_, rax);
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
#ifdef ENABLE_LOGGING_AND_PROFILING
  if (CodeGenerator::ShouldGenerateLog(args->at(0))) {
    VisitForValue(args->at(1), kStack);
    VisitForValue(args->at(2), kStack);
    __ CallRuntime(Runtime::kLog, 2);
  }
#endif
  // Finally, we're expected to leave a value on the top of the stack.
  __ LoadRoot(rax, Heap::kUndefinedValueRootIndex);
  Apply(context_, rax);
}


void FullCodeGenerator::EmitRandomHeapNumber(ZoneList<Expression*>* args) {
  ASSERT(args->length() == 0);

  Label slow_allocate_heapnumber;
  Label heapnumber_allocated;

  __ AllocateHeapNumber(rbx, rcx, &slow_allocate_heapnumber);
  __ jmp(&heapnumber_allocated);

  __ bind(&slow_allocate_heapnumber);
  // To allocate a heap number, and ensure that it is not a smi, we
  // call the runtime function FUnaryMinus on 0, returning the double
  // -0.0.  A new, distinct heap number is returned each time.
  __ Push(Smi::FromInt(0));
  __ CallRuntime(Runtime::kNumberUnaryMinus, 1);
  __ movq(rbx, rax);

  __ bind(&heapnumber_allocated);

  // Return a random uint32 number in rax.
  // The fresh HeapNumber is in rbx, which is callee-save on both x64 ABIs.
  __ PrepareCallCFunction(0);
  __ CallCFunction(ExternalReference::random_uint32_function(), 0);

  // Convert 32 random bits in rax to 0.(32 random bits) in a double
  // by computing:
  // ( 1.(20 0s)(32 random bits) x 2^20 ) - (1.0 x 2^20)).
  __ movl(rcx, Immediate(0x49800000));  // 1.0 x 2^20 as single.
  __ movd(xmm1, rcx);
  __ movd(xmm0, rax);
  __ cvtss2sd(xmm1, xmm1);
  __ xorpd(xmm0, xmm1);
  __ subsd(xmm0, xmm1);
  __ movsd(FieldOperand(rbx, HeapNumber::kValueOffset), xmm0);

  __ movq(rax, rbx);
  Apply(context_, rax);
}


void FullCodeGenerator::EmitSubString(ZoneList<Expression*>* args) {
  // Load the arguments on the stack and call the stub.
  SubStringStub stub;
  ASSERT(args->length() == 3);
  VisitForValue(args->at(0), kStack);
  VisitForValue(args->at(1), kStack);
  VisitForValue(args->at(2), kStack);
  __ CallStub(&stub);
  Apply(context_, rax);
}


void FullCodeGenerator::EmitRegExpExec(ZoneList<Expression*>* args) {
  // Load the arguments on the stack and call the stub.
  RegExpExecStub stub;
  ASSERT(args->length() == 4);
  VisitForValue(args->at(0), kStack);
  VisitForValue(args->at(1), kStack);
  VisitForValue(args->at(2), kStack);
  VisitForValue(args->at(3), kStack);
  __ CallStub(&stub);
  Apply(context_, rax);
}


void FullCodeGenerator::EmitValueOf(ZoneList<Expression*>* args) {
  ASSERT(args->length() == 1);

  VisitForValue(args->at(0), kAccumulator);  // Load the object.

  Label done;
  // If the object is a smi return the object.
  __ JumpIfSmi(rax, &done);
  // If the object is not a value type, return the object.
  __ CmpObjectType(rax, JS_VALUE_TYPE, rbx);
  __ j(not_equal, &done);
  __ movq(rax, FieldOperand(rax, JSValue::kValueOffset));

  __ bind(&done);
  Apply(context_, rax);
}


void FullCodeGenerator::EmitMathPow(ZoneList<Expression*>* args) {
  // Load the arguments on the stack and call the runtime function.
  ASSERT(args->length() == 2);
  VisitForValue(args->at(0), kStack);
  VisitForValue(args->at(1), kStack);
  __ CallRuntime(Runtime::kMath_pow, 2);
  Apply(context_, rax);
}


void FullCodeGenerator::EmitSetValueOf(ZoneList<Expression*>* args) {
  ASSERT(args->length() == 2);

  VisitForValue(args->at(0), kStack);  // Load the object.
  VisitForValue(args->at(1), kAccumulator);  // Load the value.
  __ pop(rbx);  // rax = value. ebx = object.

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
  __ RecordWrite(rbx, JSValue::kValueOffset, rdx, rcx);

  __ bind(&done);
  Apply(context_, rax);
}


void FullCodeGenerator::EmitNumberToString(ZoneList<Expression*>* args) {
  ASSERT_EQ(args->length(), 1);

  // Load the argument on the stack and call the stub.
  VisitForValue(args->at(0), kStack);

  NumberToStringStub stub;
  __ CallStub(&stub);
  Apply(context_, rax);
}


void FullCodeGenerator::EmitStringCharFromCode(ZoneList<Expression*>* args) {
  ASSERT(args->length() == 1);

  VisitForValue(args->at(0), kAccumulator);

  Label done;
  StringCharFromCodeGenerator generator(rax, rbx);
  generator.GenerateFast(masm_);
  __ jmp(&done);

  NopRuntimeCallHelper call_helper;
  generator.GenerateSlow(masm_, call_helper);

  __ bind(&done);
  Apply(context_, rbx);
}


void FullCodeGenerator::EmitStringCharCodeAt(ZoneList<Expression*>* args) {
  ASSERT(args->length() == 2);

  VisitForValue(args->at(0), kStack);
  VisitForValue(args->at(1), kAccumulator);

  Register object = rbx;
  Register index = rax;
  Register scratch = rcx;
  Register result = rdx;

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
  // Move the undefined value into the result register, which will
  // trigger conversion.
  __ LoadRoot(result, Heap::kUndefinedValueRootIndex);
  __ jmp(&done);

  NopRuntimeCallHelper call_helper;
  generator.GenerateSlow(masm_, call_helper);

  __ bind(&done);
  Apply(context_, result);
}


void FullCodeGenerator::EmitStringCharAt(ZoneList<Expression*>* args) {
  ASSERT(args->length() == 2);

  VisitForValue(args->at(0), kStack);
  VisitForValue(args->at(1), kAccumulator);

  Register object = rbx;
  Register index = rax;
  Register scratch1 = rcx;
  Register scratch2 = rdx;
  Register result = rax;

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
  __ Move(result, Smi::FromInt(0));
  __ jmp(&done);

  NopRuntimeCallHelper call_helper;
  generator.GenerateSlow(masm_, call_helper);

  __ bind(&done);
  Apply(context_, result);
}


void FullCodeGenerator::EmitStringAdd(ZoneList<Expression*>* args) {
  ASSERT_EQ(2, args->length());

  VisitForValue(args->at(0), kStack);
  VisitForValue(args->at(1), kStack);

  StringAddStub stub(NO_STRING_ADD_FLAGS);
  __ CallStub(&stub);
  Apply(context_, rax);
}


void FullCodeGenerator::EmitStringCompare(ZoneList<Expression*>* args) {
  ASSERT_EQ(2, args->length());

  VisitForValue(args->at(0), kStack);
  VisitForValue(args->at(1), kStack);

  StringCompareStub stub;
  __ CallStub(&stub);
  Apply(context_, rax);
}


void FullCodeGenerator::EmitMathSin(ZoneList<Expression*>* args) {
  // Load the argument on the stack and call the stub.
  TranscendentalCacheStub stub(TranscendentalCache::SIN);
  ASSERT(args->length() == 1);
  VisitForValue(args->at(0), kStack);
  __ CallStub(&stub);
  Apply(context_, rax);
}


void FullCodeGenerator::EmitMathCos(ZoneList<Expression*>* args) {
  // Load the argument on the stack and call the stub.
  TranscendentalCacheStub stub(TranscendentalCache::COS);
  ASSERT(args->length() == 1);
  VisitForValue(args->at(0), kStack);
  __ CallStub(&stub);
  Apply(context_, rax);
}


void FullCodeGenerator::EmitMathSqrt(ZoneList<Expression*>* args) {
  // Load the argument on the stack and call the runtime function.
  ASSERT(args->length() == 1);
  VisitForValue(args->at(0), kStack);
  __ CallRuntime(Runtime::kMath_sqrt, 1);
  Apply(context_, rax);
}


void FullCodeGenerator::EmitCallFunction(ZoneList<Expression*>* args) {
  ASSERT(args->length() >= 2);

  int arg_count = args->length() - 2;  // For receiver and function.
  VisitForValue(args->at(0), kStack);  // Receiver.
  for (int i = 0; i < arg_count; i++) {
    VisitForValue(args->at(i + 1), kStack);
  }
  VisitForValue(args->at(arg_count + 1), kAccumulator);  // Function.

  // InvokeFunction requires function in rdi. Move it in there.
  if (!result_register().is(rdi)) __ movq(rdi, result_register());
  ParameterCount count(arg_count);
  __ InvokeFunction(rdi, count, CALL_FUNCTION);
  __ movq(rsi, Operand(rbp, StandardFrameConstants::kContextOffset));
  Apply(context_, rax);
}


void FullCodeGenerator::EmitRegExpConstructResult(ZoneList<Expression*>* args) {
  ASSERT(args->length() == 3);
  VisitForValue(args->at(0), kStack);
  VisitForValue(args->at(1), kStack);
  VisitForValue(args->at(2), kStack);
  __ CallRuntime(Runtime::kRegExpConstructResult, 3);
  Apply(context_, rax);
}


void FullCodeGenerator::EmitSwapElements(ZoneList<Expression*>* args) {
  ASSERT(args->length() == 3);
  VisitForValue(args->at(0), kStack);
  VisitForValue(args->at(1), kStack);
  VisitForValue(args->at(2), kStack);
  __ CallRuntime(Runtime::kSwapElements, 3);
  Apply(context_, rax);
}


void FullCodeGenerator::EmitGetFromCache(ZoneList<Expression*>* args) {
  ASSERT_EQ(2, args->length());

  ASSERT_NE(NULL, args->at(0)->AsLiteral());
  int cache_id = Smi::cast(*(args->at(0)->AsLiteral()->handle()))->value();

  Handle<FixedArray> jsfunction_result_caches(
      Top::global_context()->jsfunction_result_caches());
  if (jsfunction_result_caches->length() <= cache_id) {
    __ Abort("Attempt to use undefined cache.");
    __ LoadRoot(rax, Heap::kUndefinedValueRootIndex);
    Apply(context_, rax);
    return;
  }

  VisitForValue(args->at(1), kAccumulator);

  Register key = rax;
  Register cache = rbx;
  Register tmp = rcx;
  __ movq(cache, CodeGenerator::ContextOperand(rsi, Context::GLOBAL_INDEX));
  __ movq(cache,
          FieldOperand(cache, GlobalObject::kGlobalContextOffset));
  __ movq(cache,
          CodeGenerator::ContextOperand(
              cache, Context::JSFUNCTION_RESULT_CACHES_INDEX));
  __ movq(cache,
          FieldOperand(cache, FixedArray::OffsetOfElementAt(cache_id)));

  Label done, not_found;
  // tmp now holds finger offset as a smi.
  ASSERT(kSmiTag == 0 && kSmiTagSize == 1);
  __ movq(tmp, FieldOperand(cache, JSFunctionResultCache::kFingerOffset));
  SmiIndex index =
      __ SmiToIndex(kScratchRegister, tmp, kPointerSizeLog2);
  __ cmpq(key, FieldOperand(cache,
                            index.reg,
                            index.scale,
                            FixedArray::kHeaderSize));
  __ j(not_equal, &not_found);
  __ movq(rax, FieldOperand(cache,
                            index.reg,
                            index.scale,
                            FixedArray::kHeaderSize + kPointerSize));
  __ jmp(&done);

  __ bind(&not_found);
  // Call runtime to perform the lookup.
  __ push(cache);
  __ push(key);
  __ CallRuntime(Runtime::kGetFromCache, 2);

  __ bind(&done);
  Apply(context_, rax);
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
    __ movq(rax, CodeGenerator::GlobalObject());
    __ push(FieldOperand(rax, GlobalObject::kBuiltinsOffset));
  }

  // Push the arguments ("left-to-right").
  int arg_count = args->length();
  for (int i = 0; i < arg_count; i++) {
    VisitForValue(args->at(i), kStack);
  }

  if (expr->is_jsruntime()) {
    // Call the JS runtime function using a call IC.
    __ Move(rcx, expr->name());
    InLoopFlag in_loop = (loop_depth() > 0) ? IN_LOOP : NOT_IN_LOOP;
    Handle<Code> ic = CodeGenerator::ComputeCallInitialize(arg_count, in_loop);
    __ call(ic, RelocInfo::CODE_TARGET);
    // Restore context register.
    __ movq(rsi, Operand(rbp, StandardFrameConstants::kContextOffset));
  } else {
    __ CallRuntime(expr->function(), arg_count);
  }
  Apply(context_, rax);
}


void FullCodeGenerator::VisitUnaryOperation(UnaryOperation* expr) {
  switch (expr->op()) {
    case Token::DELETE: {
      Comment cmnt(masm_, "[ UnaryOperation (DELETE)");
      Property* prop = expr->expression()->AsProperty();
      Variable* var = expr->expression()->AsVariableProxy()->AsVariable();
      if (prop == NULL && var == NULL) {
        // Result of deleting non-property, non-variable reference is true.
        // The subexpression may have side effects.
        VisitForEffect(expr->expression());
        Apply(context_, true);
      } else if (var != NULL &&
                 !var->is_global() &&
                 var->slot() != NULL &&
                 var->slot()->type() != Slot::LOOKUP) {
        // Result of deleting non-global, non-dynamic variables is false.
        // The subexpression does not have side effects.
        Apply(context_, false);
      } else {
        // Property or variable reference.  Call the delete builtin with
        // object and property name as arguments.
        if (prop != NULL) {
          VisitForValue(prop->obj(), kStack);
          VisitForValue(prop->key(), kStack);
        } else if (var->is_global()) {
          __ push(CodeGenerator::GlobalObject());
          __ Push(var->name());
        } else {
          // Non-global variable.  Call the runtime to look up the context
          // where the variable was introduced.
          __ push(context_register());
          __ Push(var->name());
          __ CallRuntime(Runtime::kLookupContext, 2);
          __ push(rax);
          __ Push(var->name());
        }
        __ InvokeBuiltin(Builtins::DELETE, CALL_FUNCTION);
        Apply(context_, rax);
      }
      break;
    }

    case Token::VOID: {
      Comment cmnt(masm_, "[ UnaryOperation (VOID)");
      VisitForEffect(expr->expression());
      switch (context_) {
        case Expression::kUninitialized:
          UNREACHABLE();
          break;
        case Expression::kEffect:
          break;
        case Expression::kValue:
          switch (location_) {
            case kAccumulator:
              __ LoadRoot(result_register(), Heap::kUndefinedValueRootIndex);
              break;
            case kStack:
              __ PushRoot(Heap::kUndefinedValueRootIndex);
              break;
          }
          break;
        case Expression::kTestValue:
          // Value is false so it's needed.
          switch (location_) {
            case kAccumulator:
              __ LoadRoot(result_register(), Heap::kUndefinedValueRootIndex);
              break;
            case kStack:
              __ PushRoot(Heap::kUndefinedValueRootIndex);
              break;
          }
          // Fall through.
        case Expression::kTest:
        case Expression::kValueTest:
          __ jmp(false_label_);
          break;
      }
      break;
    }

    case Token::NOT: {
      Comment cmnt(masm_, "[ UnaryOperation (NOT)");
      Label materialize_true, materialize_false;
      Label* if_true = NULL;
      Label* if_false = NULL;

      // Notice that the labels are swapped.
      PrepareTest(&materialize_true, &materialize_false, &if_false, &if_true);

      VisitForControl(expr->expression(), if_true, if_false);

      Apply(context_, if_false, if_true);  // Labels swapped.
      break;
    }

    case Token::TYPEOF: {
      Comment cmnt(masm_, "[ UnaryOperation (TYPEOF)");
      VariableProxy* proxy = expr->expression()->AsVariableProxy();
      if (proxy != NULL &&
          !proxy->var()->is_this() &&
          proxy->var()->is_global()) {
        Comment cmnt(masm_, "Global variable");
        __ Move(rcx, proxy->name());
        __ movq(rax, CodeGenerator::GlobalObject());
        Handle<Code> ic(Builtins::builtin(Builtins::LoadIC_Initialize));
        // Use a regular load, not a contextual load, to avoid a reference
        // error.
        __ Call(ic, RelocInfo::CODE_TARGET);
        __ push(rax);
      } else if (proxy != NULL &&
                 proxy->var()->slot() != NULL &&
                 proxy->var()->slot()->type() == Slot::LOOKUP) {
        __ push(rsi);
        __ Push(proxy->name());
        __ CallRuntime(Runtime::kLoadContextSlotNoReferenceError, 2);
        __ push(rax);
      } else {
        // This expression cannot throw a reference error at the top level.
        VisitForValue(expr->expression(), kStack);
      }

      __ CallRuntime(Runtime::kTypeof, 1);
      Apply(context_, rax);
      break;
    }

    case Token::ADD: {
      Comment cmt(masm_, "[ UnaryOperation (ADD)");
      VisitForValue(expr->expression(), kAccumulator);
      Label no_conversion;
      Condition is_smi = masm_->CheckSmi(result_register());
      __ j(is_smi, &no_conversion);
      __ push(result_register());
      __ InvokeBuiltin(Builtins::TO_NUMBER, CALL_FUNCTION);
      __ bind(&no_conversion);
      Apply(context_, result_register());
      break;
    }

    case Token::SUB: {
      Comment cmt(masm_, "[ UnaryOperation (SUB)");
      bool overwrite =
          (expr->expression()->AsBinaryOperation() != NULL &&
           expr->expression()->AsBinaryOperation()->ResultOverwriteAllowed());
      GenericUnaryOpStub stub(Token::SUB, overwrite);
      // GenericUnaryOpStub expects the argument to be in the
      // accumulator register rax.
      VisitForValue(expr->expression(), kAccumulator);
      __ CallStub(&stub);
      Apply(context_, rax);
      break;
    }

    case Token::BIT_NOT: {
      Comment cmt(masm_, "[ UnaryOperation (BIT_NOT)");
      bool overwrite =
          (expr->expression()->AsBinaryOperation() != NULL &&
           expr->expression()->AsBinaryOperation()->ResultOverwriteAllowed());
      GenericUnaryOpStub stub(Token::BIT_NOT, overwrite);
      // GenericUnaryOpStub expects the argument to be in the
      // accumulator register rax.
      VisitForValue(expr->expression(), kAccumulator);
      // Avoid calling the stub for Smis.
      Label smi, done;
      Condition is_smi = masm_->CheckSmi(result_register());
      __ j(is_smi, &smi);
      // Non-smi: call stub leaving result in accumulator register.
      __ CallStub(&stub);
      __ jmp(&done);
      // Perform operation directly on Smis.
      __ bind(&smi);
      __ SmiNot(result_register(), result_register());
      __ bind(&done);
      Apply(context_, result_register());
      break;
    }

    default:
      UNREACHABLE();
  }
}


void FullCodeGenerator::VisitCountOperation(CountOperation* expr) {
  Comment cmnt(masm_, "[ CountOperation");

  // Invalid left-hand-sides are rewritten to have a 'throw
  // ReferenceError' as the left-hand side.
  if (!expr->expression()->IsValidLeftHandSide()) {
    VisitForEffect(expr->expression());
    return;
  }

  // Expression can only be a property, a global or a (parameter or local)
  // slot. Variables with rewrite to .arguments are treated as KEYED_PROPERTY.
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
    Location saved_location = location_;
    location_ = kAccumulator;
    EmitVariableLoad(expr->expression()->AsVariableProxy()->var(),
                     Expression::kValue);
    location_ = saved_location;
  } else {
    // Reserve space for result of postfix operation.
    if (expr->is_postfix() && context_ != Expression::kEffect) {
      __ Push(Smi::FromInt(0));
    }
    if (assign_type == NAMED_PROPERTY) {
      VisitForValue(prop->obj(), kAccumulator);
      __ push(rax);  // Copy of receiver, needed for later store.
      EmitNamedPropertyLoad(prop);
    } else {
      VisitForValue(prop->obj(), kStack);
      VisitForValue(prop->key(), kAccumulator);
      __ movq(rdx, Operand(rsp, 0));  // Leave receiver on stack
      __ push(rax);  // Copy of key, needed for later store.
      EmitKeyedPropertyLoad(prop);
    }
  }

  // Call ToNumber only if operand is not a smi.
  Label no_conversion;
  Condition is_smi;
  is_smi = masm_->CheckSmi(rax);
  __ j(is_smi, &no_conversion);
  __ push(rax);
  __ InvokeBuiltin(Builtins::TO_NUMBER, CALL_FUNCTION);
  __ bind(&no_conversion);

  // Save result for postfix expressions.
  if (expr->is_postfix()) {
    switch (context_) {
      case Expression::kUninitialized:
        UNREACHABLE();
      case Expression::kEffect:
        // Do not save result.
        break;
      case Expression::kValue:
      case Expression::kTest:
      case Expression::kValueTest:
      case Expression::kTestValue:
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
        break;
    }
  }

  // Inline smi case if we are in a loop.
  Label stub_call, done;
  if (loop_depth() > 0) {
    if (expr->op() == Token::INC) {
      __ SmiAddConstant(rax, rax, Smi::FromInt(1));
    } else {
      __ SmiSubConstant(rax, rax, Smi::FromInt(1));
    }
    __ j(overflow, &stub_call);
    // We could eliminate this smi check if we split the code at
    // the first smi check before calling ToNumber.
    is_smi = masm_->CheckSmi(rax);
    __ j(is_smi, &done);
    __ bind(&stub_call);
    // Call stub. Undo operation first.
    if (expr->op() == Token::INC) {
      __ SmiSubConstant(rax, rax, Smi::FromInt(1));
    } else {
      __ SmiAddConstant(rax, rax, Smi::FromInt(1));
    }
  }
  // Call stub for +1/-1.
  GenericBinaryOpStub stub(expr->binary_op(),
                           NO_OVERWRITE,
                           NO_GENERIC_BINARY_FLAGS);
  stub.GenerateCall(masm_, rax, Smi::FromInt(1));
  __ bind(&done);

  // Store the value returned in rax.
  switch (assign_type) {
    case VARIABLE:
      if (expr->is_postfix()) {
        // Perform the assignment as if via '='.
        EmitVariableAssignment(expr->expression()->AsVariableProxy()->var(),
                               Token::ASSIGN,
                               Expression::kEffect);
        // For all contexts except kEffect: We have the result on
        // top of the stack.
        if (context_ != Expression::kEffect) {
          ApplyTOS(context_);
        }
      } else {
        // Perform the assignment as if via '='.
        EmitVariableAssignment(expr->expression()->AsVariableProxy()->var(),
                               Token::ASSIGN,
                               context_);
      }
      break;
    case NAMED_PROPERTY: {
      __ Move(rcx, prop->key()->AsLiteral()->handle());
      __ pop(rdx);
      Handle<Code> ic(Builtins::builtin(Builtins::StoreIC_Initialize));
      __ call(ic, RelocInfo::CODE_TARGET);
      // This nop signals to the IC that there is no inlined code at the call
      // site for it to patch.
      __ nop();
      if (expr->is_postfix()) {
        if (context_ != Expression::kEffect) {
          ApplyTOS(context_);
        }
      } else {
        Apply(context_, rax);
      }
      break;
    }
    case KEYED_PROPERTY: {
      __ pop(rcx);
      __ pop(rdx);
      Handle<Code> ic(Builtins::builtin(Builtins::KeyedStoreIC_Initialize));
      __ call(ic, RelocInfo::CODE_TARGET);
      // This nop signals to the IC that there is no inlined code at the call
      // site for it to patch.
      __ nop();
      if (expr->is_postfix()) {
        if (context_ != Expression::kEffect) {
          ApplyTOS(context_);
        }
      } else {
        Apply(context_, rax);
      }
      break;
    }
  }
}

void FullCodeGenerator::VisitBinaryOperation(BinaryOperation* expr) {
  Comment cmnt(masm_, "[ BinaryOperation");
  switch (expr->op()) {
    case Token::COMMA:
      VisitForEffect(expr->left());
      Visit(expr->right());
      break;

    case Token::OR:
    case Token::AND:
      EmitLogicalOperation(expr);
      break;

    case Token::ADD:
    case Token::SUB:
    case Token::DIV:
    case Token::MOD:
    case Token::MUL:
    case Token::BIT_OR:
    case Token::BIT_AND:
    case Token::BIT_XOR:
    case Token::SHL:
    case Token::SHR:
    case Token::SAR:
      VisitForValue(expr->left(), kStack);
      VisitForValue(expr->right(), kAccumulator);
      EmitBinaryOp(expr->op(), context_);
      break;

    default:
      UNREACHABLE();
  }
}


void FullCodeGenerator::EmitNullCompare(bool strict,
                                        Register obj,
                                        Register null_const,
                                        Label* if_true,
                                        Label* if_false,
                                        Register scratch) {
  __ cmpq(obj, null_const);
  if (strict) {
    __ j(equal, if_true);
  } else {
    __ j(equal, if_true);
    __ CompareRoot(obj, Heap::kUndefinedValueRootIndex);
    __ j(equal, if_true);
    __ JumpIfSmi(obj, if_false);
    // It can be an undetectable object.
    __ movq(scratch, FieldOperand(obj, HeapObject::kMapOffset));
    __ testb(FieldOperand(scratch, Map::kBitFieldOffset),
             Immediate(1 << Map::kIsUndetectable));
    __ j(not_zero, if_true);
  }
  __ jmp(if_false);
}


void FullCodeGenerator::VisitCompareOperation(CompareOperation* expr) {
  Comment cmnt(masm_, "[ CompareOperation");

  // Always perform the comparison for its control flow.  Pack the result
  // into the expression's context after the comparison is performed.
  Label materialize_true, materialize_false;
  Label* if_true = NULL;
  Label* if_false = NULL;
  PrepareTest(&materialize_true, &materialize_false, &if_true, &if_false);

  VisitForValue(expr->left(), kStack);
  switch (expr->op()) {
    case Token::IN:
      VisitForValue(expr->right(), kStack);
      __ InvokeBuiltin(Builtins::IN, CALL_FUNCTION);
      __ CompareRoot(rax, Heap::kTrueValueRootIndex);
      __ j(equal, if_true);
      __ jmp(if_false);
      break;

    case Token::INSTANCEOF: {
      VisitForValue(expr->right(), kStack);
      InstanceofStub stub;
      __ CallStub(&stub);
      __ testq(rax, rax);
      __ j(zero, if_true);  // The stub returns 0 for true.
      __ jmp(if_false);
      break;
    }

    default: {
      VisitForValue(expr->right(), kAccumulator);
      Condition cc = no_condition;
      bool strict = false;
      switch (expr->op()) {
        case Token::EQ_STRICT:
          strict = true;
          // Fall through.
        case Token::EQ: {
          cc = equal;
          __ pop(rdx);
          // If either operand is constant null we do a fast compare
          // against null.
          Literal* right_literal = expr->right()->AsLiteral();
          Literal* left_literal = expr->left()->AsLiteral();
          if (right_literal != NULL && right_literal->handle()->IsNull()) {
            EmitNullCompare(strict, rdx, rax, if_true, if_false, rcx);
            Apply(context_, if_true, if_false);
            return;
          } else if (left_literal != NULL && left_literal->handle()->IsNull()) {
            EmitNullCompare(strict, rax, rdx, if_true, if_false, rcx);
            Apply(context_, if_true, if_false);
            return;
          }
          break;
        }
        case Token::LT:
          cc = less;
          __ pop(rdx);
          break;
        case Token::GT:
          // Reverse left and right sizes to obtain ECMA-262 conversion order.
          cc = less;
          __ movq(rdx, result_register());
          __ pop(rax);
         break;
        case Token::LTE:
          // Reverse left and right sizes to obtain ECMA-262 conversion order.
          cc = greater_equal;
          __ movq(rdx, result_register());
          __ pop(rax);
          break;
        case Token::GTE:
          cc = greater_equal;
          __ pop(rdx);
          break;
        case Token::IN:
        case Token::INSTANCEOF:
        default:
          UNREACHABLE();
      }

      // The comparison stub expects the smi vs. smi case to be handled
      // before it is called.
      Label slow_case;
      __ JumpIfNotBothSmi(rax, rdx, &slow_case);
      __ SmiCompare(rdx, rax);
      __ j(cc, if_true);
      __ jmp(if_false);

      __ bind(&slow_case);
      CompareStub stub(cc, strict);
      __ CallStub(&stub);
      __ testq(rax, rax);
      __ j(cc, if_true);
      __ jmp(if_false);
    }
  }

  // Convert the result of the comparison into one expected for this
  // expression's context.
  Apply(context_, if_true, if_false);
}


void FullCodeGenerator::VisitThisFunction(ThisFunction* expr) {
  __ movq(rax, Operand(rbp, JavaScriptFrameConstants::kFunctionOffset));
  Apply(context_, rax);
}


Register FullCodeGenerator::result_register() { return rax; }


Register FullCodeGenerator::context_register() { return rsi; }


void FullCodeGenerator::StoreToFrameField(int frame_offset, Register value) {
  ASSERT(IsAligned(frame_offset, kPointerSize));
  __ movq(Operand(rbp, frame_offset), value);
}


void FullCodeGenerator::LoadContextField(Register dst, int context_index) {
  __ movq(dst, CodeGenerator::ContextOperand(rsi, context_index));
}


// ----------------------------------------------------------------------------
// Non-local control flow support.


void FullCodeGenerator::EnterFinallyBlock() {
  ASSERT(!result_register().is(rdx));
  ASSERT(!result_register().is(rcx));
  // Cook return address on top of stack (smi encoded Code* delta)
  __ movq(rdx, Operand(rsp, 0));
  __ Move(rcx, masm_->CodeObject());
  __ subq(rdx, rcx);
  __ Integer32ToSmi(rdx, rdx);
  __ movq(Operand(rsp, 0), rdx);
  // Store result register while executing finally block.
  __ push(result_register());
}


void FullCodeGenerator::ExitFinallyBlock() {
  ASSERT(!result_register().is(rdx));
  ASSERT(!result_register().is(rcx));
  // Restore result register from stack.
  __ pop(result_register());
  // Uncook return address.
  __ movq(rdx, Operand(rsp, 0));
  __ SmiToInteger32(rdx, rdx);
  __ Move(rcx, masm_->CodeObject());
  __ addq(rdx, rcx);
  __ movq(Operand(rsp, 0), rdx);
  // And return.
  __ ret(0);
}


#undef __


} }  // namespace v8::internal

#endif  // V8_TARGET_ARCH_X64
