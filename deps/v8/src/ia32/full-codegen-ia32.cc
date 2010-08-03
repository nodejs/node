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

#if defined(V8_TARGET_ARCH_IA32)

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
//   o edi: the JS function object being called (ie, ourselves)
//   o esi: our context
//   o ebp: our caller's frame pointer
//   o esp: stack pointer (pointing to return address)
//
// The function builds a JS frame.  Please see JavaScriptFrameConstants in
// frames-ia32.h for its layout.
void FullCodeGenerator::Generate(CompilationInfo* info, Mode mode) {
  ASSERT(info_ == NULL);
  info_ = info;
  SetFunctionPosition(function());
  Comment cmnt(masm_, "[ function compiled by full code generator");

  if (mode == PRIMARY) {
    __ push(ebp);  // Caller's frame pointer.
    __ mov(ebp, esp);
    __ push(esi);  // Callee's context.
    __ push(edi);  // Callee's JS Function.

    { Comment cmnt(masm_, "[ Allocate locals");
      int locals_count = scope()->num_stack_slots();
      if (locals_count == 1) {
        __ push(Immediate(Factory::undefined_value()));
      } else if (locals_count > 1) {
        __ mov(eax, Immediate(Factory::undefined_value()));
        for (int i = 0; i < locals_count; i++) {
          __ push(eax);
        }
      }
    }

    bool function_in_register = true;

    // Possibly allocate a local context.
    int heap_slots = scope()->num_heap_slots() - Context::MIN_CONTEXT_SLOTS;
    if (heap_slots > 0) {
      Comment cmnt(masm_, "[ Allocate local context");
      // Argument to NewContext is the function, which is still in edi.
      __ push(edi);
      if (heap_slots <= FastNewContextStub::kMaximumSlots) {
        FastNewContextStub stub(heap_slots);
        __ CallStub(&stub);
      } else {
        __ CallRuntime(Runtime::kNewContext, 1);
      }
      function_in_register = false;
      // Context is returned in both eax and esi.  It replaces the context
      // passed to us.  It's saved in the stack and kept live in esi.
      __ mov(Operand(ebp, StandardFrameConstants::kContextOffset), esi);

      // Copy parameters into context if necessary.
      int num_parameters = scope()->num_parameters();
      for (int i = 0; i < num_parameters; i++) {
        Slot* slot = scope()->parameter(i)->slot();
        if (slot != NULL && slot->type() == Slot::CONTEXT) {
          int parameter_offset = StandardFrameConstants::kCallerSPOffset +
                                     (num_parameters - 1 - i) * kPointerSize;
          // Load parameter from stack.
          __ mov(eax, Operand(ebp, parameter_offset));
          // Store it in the context.
          int context_offset = Context::SlotOffset(slot->index());
          __ mov(Operand(esi, context_offset), eax);
          // Update the write barrier. This clobbers all involved
          // registers, so we have use a third register to avoid
          // clobbering esi.
          __ mov(ecx, esi);
          __ RecordWrite(ecx, context_offset, eax, ebx);
        }
      }
    }

    Variable* arguments = scope()->arguments()->AsVariable();
    if (arguments != NULL) {
      // Function uses arguments object.
      Comment cmnt(masm_, "[ Allocate arguments object");
      if (function_in_register) {
        __ push(edi);
      } else {
        __ push(Operand(ebp, JavaScriptFrameConstants::kFunctionOffset));
      }
      // Receiver is just before the parameters on the caller's stack.
      int offset = scope()->num_parameters() * kPointerSize;
      __ lea(edx,
             Operand(ebp, StandardFrameConstants::kCallerSPOffset + offset));
      __ push(edx);
      __ push(Immediate(Smi::FromInt(scope()->num_parameters())));
      // Arguments to ArgumentsAccessStub:
      //   function, receiver address, parameter count.
      // The stub will rewrite receiver and parameter count if the previous
      // stack frame was an arguments adapter frame.
      ArgumentsAccessStub stub(ArgumentsAccessStub::NEW_OBJECT);
      __ CallStub(&stub);
      __ mov(ecx, eax);  // Duplicate result.
      Move(arguments->slot(), eax, ebx, edx);
      Slot* dot_arguments_slot =
          scope()->arguments_shadow()->AsVariable()->slot();
      Move(dot_arguments_slot, ecx, ebx, edx);
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
    ExternalReference stack_limit =
        ExternalReference::address_of_stack_limit();
    __ cmp(esp, Operand::StaticVariable(stack_limit));
    __ j(above_equal, &ok, taken);
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
    __ mov(eax, Factory::undefined_value());
    EmitReturnSequence();
  }
}


void FullCodeGenerator::EmitReturnSequence() {
  Comment cmnt(masm_, "[ Return sequence");
  if (return_label_.is_bound()) {
    __ jmp(&return_label_);
  } else {
    // Common return label
    __ bind(&return_label_);
    if (FLAG_trace) {
      __ push(eax);
      __ CallRuntime(Runtime::kTraceExit, 1);
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
    __ mov(esp, ebp);
    __ pop(ebp);
    __ ret((scope()->num_parameters() + 1) * kPointerSize);
#ifdef ENABLE_DEBUGGER_SUPPORT
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
          if (!reg.is(result_register())) __ mov(result_register(), reg);
          break;
        case kStack:
          __ push(reg);
          break;
      }
      break;

    case Expression::kTest:
      // For simplicity we always test the accumulator register.
      if (!reg.is(result_register())) __ mov(result_register(), reg);
      DoTest(context);
      break;

    case Expression::kValueTest:
    case Expression::kTestValue:
      if (!reg.is(result_register())) __ mov(result_register(), reg);
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
          __ mov(result_register(), slot_operand);
          break;
        case kStack:
          // Memory operands can be pushed directly.
          __ push(slot_operand);
          break;
      }
      break;
    }

    case Expression::kTest:
      // For simplicity we always test the accumulator register.
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
          __ mov(result_register(), lit->handle());
          break;
        case kStack:
          // Immediates can be pushed directly.
          __ push(Immediate(lit->handle()));
          break;
      }
      break;

    case Expression::kTest:
      // For simplicity we always test the accumulator register.
      __ mov(result_register(), lit->handle());
      DoTest(context);
      break;

    case Expression::kValueTest:
    case Expression::kTestValue:
      __ mov(result_register(), lit->handle());
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
      // For simplicity we always test the accumulator register.
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
          __ mov(result_register(), Operand(esp, 0));
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
  ASSERT(!reg.is(esp));
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
          if (!reg.is(result_register())) __ mov(result_register(), reg);
          break;
        case kStack:
          if (count > 1) __ Drop(count - 1);
          __ mov(Operand(esp, 0), reg);
          break;
      }
      break;

    case Expression::kTest:
      // For simplicity we always test the accumulator register.
      __ Drop(count);
      if (!reg.is(result_register())) __ mov(result_register(), reg);
      DoTest(context);
      break;

    case Expression::kValueTest:
    case Expression::kTestValue:
      switch (location_) {
        case kAccumulator:
          __ Drop(count);
          if (!reg.is(result_register())) __ mov(result_register(), reg);
          break;
        case kStack:
          if (count > 1) __ Drop(count - 1);
          __ mov(result_register(), reg);
          __ mov(Operand(esp, 0), result_register());
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
          __ mov(result_register(), Factory::true_value());
          __ jmp(&done);
          __ bind(materialize_false);
          __ mov(result_register(), Factory::false_value());
          break;
        case kStack:
          __ bind(materialize_true);
          __ push(Immediate(Factory::true_value()));
          __ jmp(&done);
          __ bind(materialize_false);
          __ push(Immediate(Factory::false_value()));
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
          __ mov(result_register(), Factory::true_value());
          break;
        case kStack:
          __ push(Immediate(Factory::true_value()));
          break;
      }
      __ jmp(true_label_);
      break;

    case Expression::kTestValue:
      __ bind(materialize_false);
      switch (location_) {
        case kAccumulator:
          __ mov(result_register(), Factory::false_value());
          break;
        case kStack:
          __ push(Immediate(Factory::false_value()));
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
      Handle<Object> value =
          flag ? Factory::true_value() : Factory::false_value();
      switch (location_) {
        case kAccumulator:
          __ mov(result_register(), value);
          break;
        case kStack:
          __ push(Immediate(value));
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
          if (!flag) __ mov(result_register(), Factory::false_value());
          break;
        case kStack:
          // If value is false it's needed.
          if (!flag) __ push(Immediate(Factory::false_value()));
          break;
      }
      __ jmp(flag ? true_label_ : false_label_);
      break;
    case Expression::kValueTest:
      switch (location_) {
        case kAccumulator:
          // If value is true it's needed.
          if (flag) __ mov(result_register(), Factory::true_value());
          break;
        case kStack:
          // If value is true it's needed.
          if (flag) __ push(Immediate(Factory::true_value()));
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
  __ cmp(result_register(), Factory::undefined_value());
  __ j(equal, if_false);
  __ cmp(result_register(), Factory::true_value());
  __ j(equal, if_true);
  __ cmp(result_register(), Factory::false_value());
  __ j(equal, if_false);
  ASSERT_EQ(0, kSmiTag);
  __ test(result_register(), Operand(result_register()));
  __ j(zero, if_false);
  __ test(result_register(), Immediate(kSmiTagMask));
  __ j(zero, if_true);

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
  __ test(eax, Operand(eax));

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
      return Operand(ebp, SlotOffset(slot));
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
  return Operand(eax, 0);
}


void FullCodeGenerator::Move(Register destination, Slot* source) {
  MemOperand location = EmitSlotSearch(source, destination);
  __ mov(destination, location);
}


void FullCodeGenerator::Move(Slot* dst,
                             Register src,
                             Register scratch1,
                             Register scratch2) {
  ASSERT(dst->type() != Slot::LOOKUP);  // Not yet implemented.
  ASSERT(!scratch1.is(src) && !scratch2.is(src));
  MemOperand location = EmitSlotSearch(dst, scratch1);
  __ mov(location, src);
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
          __ mov(Operand(ebp, SlotOffset(slot)),
                 Immediate(Factory::the_hole_value()));
        } else if (function != NULL) {
          VisitForValue(function, kAccumulator);
          __ mov(Operand(ebp, SlotOffset(slot)), result_register());
        }
        break;

      case Slot::CONTEXT:
        // We bypass the general EmitSlotSearch because we know more about
        // this specific context.

        // The variable in the decl always resides in the current context.
        ASSERT_EQ(0, scope()->ContextChainLength(variable->scope()));
        if (FLAG_debug_code) {
          // Check if we have the correct context pointer.
          __ mov(ebx,
                 CodeGenerator::ContextOperand(esi, Context::FCONTEXT_INDEX));
          __ cmp(ebx, Operand(esi));
          __ Check(equal, "Unexpected declaration in current context.");
        }
        if (mode == Variable::CONST) {
          __ mov(CodeGenerator::ContextOperand(esi, slot->index()),
                 Immediate(Factory::the_hole_value()));
          // No write barrier since the hole value is in old space.
        } else if (function != NULL) {
          VisitForValue(function, kAccumulator);
          __ mov(CodeGenerator::ContextOperand(esi, slot->index()),
                 result_register());
          int offset = Context::SlotOffset(slot->index());
          __ mov(ebx, esi);
          __ RecordWrite(ebx, offset, result_register(), ecx);
        }
        break;

      case Slot::LOOKUP: {
        __ push(esi);
        __ push(Immediate(variable->name()));
        // Declaration nodes are always introduced in one of two modes.
        ASSERT(mode == Variable::VAR || mode == Variable::CONST);
        PropertyAttributes attr = (mode == Variable::VAR) ? NONE : READ_ONLY;
        __ push(Immediate(Smi::FromInt(attr)));
        // Push initial value, if any.
        // Note: For variables we must not push an initial value (such as
        // 'undefined') because we may have a (legal) redeclaration and we
        // must not destroy the current value.
        if (mode == Variable::CONST) {
          __ push(Immediate(Factory::the_hole_value()));
        } else if (function != NULL) {
          VisitForValue(function, kStack);
        } else {
          __ push(Immediate(Smi::FromInt(0)));  // No initial value!
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
        __ pop(ecx);
      } else {
        VisitForValue(prop->key(), kAccumulator);
        __ mov(ecx, result_register());
        __ mov(result_register(), Factory::the_hole_value());
      }
      __ pop(edx);

      Handle<Code> ic(Builtins::builtin(Builtins::KeyedStoreIC_Initialize));
      __ call(ic, RelocInfo::CODE_TARGET);
      // Absence of a test eax instruction following the call
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
  __ push(esi);  // The context is the first argument.
  __ push(Immediate(pairs));
  __ push(Immediate(Smi::FromInt(is_eval() ? 1 : 0)));
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
    __ mov(edx, Operand(esp, 0));  // Switch value.
    __ mov(ecx, edx);
    __ or_(ecx, Operand(eax));
    __ test(ecx, Immediate(kSmiTagMask));
    __ j(not_zero, &slow_case, not_taken);
    __ cmp(edx, Operand(eax));
    __ j(not_equal, &next_test);
    __ Drop(1);  // Switch value is no longer needed.
    __ jmp(clause->body_target()->entry_label());

    __ bind(&slow_case);
    CompareStub stub(equal, true);
    __ CallStub(&stub);
    __ test(eax, Operand(eax));
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
  __ cmp(eax, Factory::undefined_value());
  __ j(equal, &exit);
  __ cmp(eax, Factory::null_value());
  __ j(equal, &exit);

  // Convert the object to a JS object.
  Label convert, done_convert;
  __ test(eax, Immediate(kSmiTagMask));
  __ j(zero, &convert);
  __ CmpObjectType(eax, FIRST_JS_OBJECT_TYPE, ecx);
  __ j(above_equal, &done_convert);
  __ bind(&convert);
  __ push(eax);
  __ InvokeBuiltin(Builtins::TO_OBJECT, CALL_FUNCTION);
  __ bind(&done_convert);
  __ push(eax);

  // TODO(kasperl): Check cache validity in generated code. This is a
  // fast case for the JSObject::IsSimpleEnum cache validity
  // checks. If we cannot guarantee cache validity, call the runtime
  // system to check cache validity or get the property names in a
  // fixed array.

  // Get the set of properties to enumerate.
  __ push(eax);  // Duplicate the enumerable object on the stack.
  __ CallRuntime(Runtime::kGetPropertyNamesFast, 1);

  // If we got a map from the runtime call, we can do a fast
  // modification check. Otherwise, we got a fixed array, and we have
  // to do a slow check.
  Label fixed_array;
  __ cmp(FieldOperand(eax, HeapObject::kMapOffset), Factory::meta_map());
  __ j(not_equal, &fixed_array);

  // We got a map in register eax. Get the enumeration cache from it.
  __ mov(ecx, FieldOperand(eax, Map::kInstanceDescriptorsOffset));
  __ mov(ecx, FieldOperand(ecx, DescriptorArray::kEnumerationIndexOffset));
  __ mov(edx, FieldOperand(ecx, DescriptorArray::kEnumCacheBridgeCacheOffset));

  // Setup the four remaining stack slots.
  __ push(eax);  // Map.
  __ push(edx);  // Enumeration cache.
  __ mov(eax, FieldOperand(edx, FixedArray::kLengthOffset));
  __ push(eax);  // Enumeration cache length (as smi).
  __ push(Immediate(Smi::FromInt(0)));  // Initial index.
  __ jmp(&loop);

  // We got a fixed array in register eax. Iterate through that.
  __ bind(&fixed_array);
  __ push(Immediate(Smi::FromInt(0)));  // Map (0) - force slow check.
  __ push(eax);
  __ mov(eax, FieldOperand(eax, FixedArray::kLengthOffset));
  __ push(eax);  // Fixed array length (as smi).
  __ push(Immediate(Smi::FromInt(0)));  // Initial index.

  // Generate code for doing the condition check.
  __ bind(&loop);
  __ mov(eax, Operand(esp, 0 * kPointerSize));  // Get the current index.
  __ cmp(eax, Operand(esp, 1 * kPointerSize));  // Compare to the array length.
  __ j(above_equal, loop_statement.break_target());

  // Get the current entry of the array into register ebx.
  __ mov(ebx, Operand(esp, 2 * kPointerSize));
  __ mov(ebx, FieldOperand(ebx, eax, times_2, FixedArray::kHeaderSize));

  // Get the expected map from the stack or a zero map in the
  // permanent slow case into register edx.
  __ mov(edx, Operand(esp, 3 * kPointerSize));

  // Check if the expected map still matches that of the enumerable.
  // If not, we have to filter the key.
  Label update_each;
  __ mov(ecx, Operand(esp, 4 * kPointerSize));
  __ cmp(edx, FieldOperand(ecx, HeapObject::kMapOffset));
  __ j(equal, &update_each);

  // Convert the entry to a string or null if it isn't a property
  // anymore. If the property has been removed while iterating, we
  // just skip it.
  __ push(ecx);  // Enumerable.
  __ push(ebx);  // Current entry.
  __ InvokeBuiltin(Builtins::FILTER_KEY, CALL_FUNCTION);
  __ cmp(eax, Factory::null_value());
  __ j(equal, loop_statement.continue_target());
  __ mov(ebx, Operand(eax));

  // Update the 'each' property or variable from the possibly filtered
  // entry in register ebx.
  __ bind(&update_each);
  __ mov(result_register(), ebx);
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
  __ add(Operand(esp, 0 * kPointerSize), Immediate(Smi::FromInt(1)));
  __ jmp(&loop);

  // Slow case for the stack limit check.
  StackCheckStub stack_check_stub;
  __ bind(&stack_limit_hit);
  __ CallStub(&stack_check_stub);
  __ jmp(&stack_check_done);

  // Remove the pointers stored on the stack.
  __ bind(loop_statement.break_target());
  __ add(Operand(esp), Immediate(5 * kPointerSize));

  // Exit and decrement the loop depth.
  __ bind(&exit);
  decrement_loop_depth();
}


void FullCodeGenerator::EmitNewClosure(Handle<SharedFunctionInfo> info) {
  // Use the fast case closure allocation code that allocates in new
  // space for nested functions that don't need literals cloning.
  if (scope()->is_function_scope() && info->num_literals() == 0) {
    FastNewClosureStub stub;
    __ push(Immediate(info));
    __ CallStub(&stub);
  } else {
    __ push(esi);
    __ push(Immediate(info));
    __ CallRuntime(Runtime::kNewClosure, 2);
  }
  Apply(context_, eax);
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
    // Use inline caching. Variable name is passed in ecx and the global
    // object on the stack.
    __ mov(eax, CodeGenerator::GlobalObject());
    __ mov(ecx, var->name());
    Handle<Code> ic(Builtins::builtin(Builtins::LoadIC_Initialize));
    __ call(ic, RelocInfo::CODE_TARGET_CONTEXT);
    // By emitting a nop we make sure that we do not have a test eax
    // instruction after the call it is treated specially by the LoadIC code
    // Remember that the assembler may choose to do peephole optimization
    // (eg, push/pop elimination).
    __ nop();
    Apply(context, eax);

  } else if (slot != NULL && slot->type() == Slot::LOOKUP) {
    Comment cmnt(masm_, "Lookup slot");
    __ push(esi);  // Context.
    __ push(Immediate(var->name()));
    __ CallRuntime(Runtime::kLoadContextSlot, 2);
    Apply(context, eax);

  } else if (slot != NULL) {
    Comment cmnt(masm_, (slot->type() == Slot::CONTEXT)
                            ? "Context slot"
                            : "Stack slot");
    if (var->mode() == Variable::CONST) {
      // Constants may be the hole value if they have not been initialized.
      // Unhole them.
      Label done;
      MemOperand slot_operand = EmitSlotSearch(slot, eax);
      __ mov(eax, slot_operand);
      __ cmp(eax, Factory::the_hole_value());
      __ j(not_equal, &done);
      __ mov(eax, Factory::undefined_value());
      __ bind(&done);
      Apply(context, eax);
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
    MemOperand object_loc = EmitSlotSearch(object_slot, eax);
    __ mov(edx, object_loc);

    // Assert that the key is a smi.
    Literal* key_literal = property->key()->AsLiteral();
    ASSERT_NOT_NULL(key_literal);
    ASSERT(key_literal->handle()->IsSmi());

    // Load the key.
    __ mov(eax, Immediate(key_literal->handle()));

    // Do a keyed property load.
    Handle<Code> ic(Builtins::builtin(Builtins::KeyedLoadIC_Initialize));
    __ call(ic, RelocInfo::CODE_TARGET);
    // Notice: We must not have a "test eax, ..." instruction after the
    // call. It is treated specially by the LoadIC code.
    __ nop();
    // Drop key and object left on the stack by IC.
    Apply(context, eax);
  }
}


void FullCodeGenerator::VisitRegExpLiteral(RegExpLiteral* expr) {
  Comment cmnt(masm_, "[ RegExpLiteral");
  Label done;
  // Registers will be used as follows:
  // edi = JS function.
  // ebx = literals array.
  // eax = regexp literal.
  __ mov(edi, Operand(ebp, JavaScriptFrameConstants::kFunctionOffset));
  __ mov(ebx, FieldOperand(edi, JSFunction::kLiteralsOffset));
  int literal_offset =
    FixedArray::kHeaderSize + expr->literal_index() * kPointerSize;
  __ mov(eax, FieldOperand(ebx, literal_offset));
  __ cmp(eax, Factory::undefined_value());
  __ j(not_equal, &done);
  // Create regexp literal using runtime function
  // Result will be in eax.
  __ push(ebx);
  __ push(Immediate(Smi::FromInt(expr->literal_index())));
  __ push(Immediate(expr->pattern()));
  __ push(Immediate(expr->flags()));
  __ CallRuntime(Runtime::kMaterializeRegExpLiteral, 4);
  // Label done:
  __ bind(&done);
  Apply(context_, eax);
}


void FullCodeGenerator::VisitObjectLiteral(ObjectLiteral* expr) {
  Comment cmnt(masm_, "[ ObjectLiteral");
  __ mov(edi, Operand(ebp, JavaScriptFrameConstants::kFunctionOffset));
  __ push(FieldOperand(edi, JSFunction::kLiteralsOffset));
  __ push(Immediate(Smi::FromInt(expr->literal_index())));
  __ push(Immediate(expr->constant_properties()));
  __ push(Immediate(Smi::FromInt(expr->fast_elements() ? 1 : 0)));
  if (expr->depth() > 1) {
    __ CallRuntime(Runtime::kCreateObjectLiteral, 4);
  } else {
    __ CallRuntime(Runtime::kCreateObjectLiteralShallow, 4);
  }

  // If result_saved is true the result is on top of the stack.  If
  // result_saved is false the result is in eax.
  bool result_saved = false;

  for (int i = 0; i < expr->properties()->length(); i++) {
    ObjectLiteral::Property* property = expr->properties()->at(i);
    if (property->IsCompileTimeValue()) continue;

    Literal* key = property->key();
    Expression* value = property->value();
    if (!result_saved) {
      __ push(eax);  // Save result on the stack
      result_saved = true;
    }
    switch (property->kind()) {
      case ObjectLiteral::Property::MATERIALIZED_LITERAL:
        ASSERT(!CompileTimeValue::IsCompileTimeValue(value));
        // Fall through.
      case ObjectLiteral::Property::COMPUTED:
        if (key->handle()->IsSymbol()) {
          VisitForValue(value, kAccumulator);
          __ mov(ecx, Immediate(key->handle()));
          __ mov(edx, Operand(esp, 0));
          Handle<Code> ic(Builtins::builtin(Builtins::StoreIC_Initialize));
          __ call(ic, RelocInfo::CODE_TARGET);
          __ nop();
          break;
        }
        // Fall through.
      case ObjectLiteral::Property::PROTOTYPE:
        __ push(Operand(esp, 0));  // Duplicate receiver.
        VisitForValue(key, kStack);
        VisitForValue(value, kStack);
        __ CallRuntime(Runtime::kSetProperty, 3);
        break;
      case ObjectLiteral::Property::SETTER:
      case ObjectLiteral::Property::GETTER:
        __ push(Operand(esp, 0));  // Duplicate receiver.
        VisitForValue(key, kStack);
        __ push(Immediate(property->kind() == ObjectLiteral::Property::SETTER ?
                          Smi::FromInt(1) :
                          Smi::FromInt(0)));
        VisitForValue(value, kStack);
        __ CallRuntime(Runtime::kDefineAccessor, 4);
        break;
      default: UNREACHABLE();
    }
  }

  if (result_saved) {
    ApplyTOS(context_);
  } else {
    Apply(context_, eax);
  }
}


void FullCodeGenerator::VisitArrayLiteral(ArrayLiteral* expr) {
  Comment cmnt(masm_, "[ ArrayLiteral");

  ZoneList<Expression*>* subexprs = expr->values();
  int length = subexprs->length();

  __ mov(ebx, Operand(ebp, JavaScriptFrameConstants::kFunctionOffset));
  __ push(FieldOperand(ebx, JSFunction::kLiteralsOffset));
  __ push(Immediate(Smi::FromInt(expr->literal_index())));
  __ push(Immediate(expr->constant_elements()));
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
      __ push(eax);
      result_saved = true;
    }
    VisitForValue(subexpr, kAccumulator);

    // Store the subexpression value in the array's elements.
    __ mov(ebx, Operand(esp, 0));  // Copy of array literal.
    __ mov(ebx, FieldOperand(ebx, JSObject::kElementsOffset));
    int offset = FixedArray::kHeaderSize + (i * kPointerSize);
    __ mov(FieldOperand(ebx, offset), result_register());

    // Update the write barrier for the array store.
    __ RecordWrite(ebx, offset, result_register(), ecx);
  }

  if (result_saved) {
    ApplyTOS(context_);
  } else {
    Apply(context_, eax);
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
        __ mov(edx, Operand(esp, 0));
        __ push(eax);
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
  __ mov(ecx, Immediate(key->handle()));
  Handle<Code> ic(Builtins::builtin(Builtins::LoadIC_Initialize));
  __ call(ic, RelocInfo::CODE_TARGET);
  __ nop();
}


void FullCodeGenerator::EmitKeyedPropertyLoad(Property* prop) {
  SetSourcePosition(prop->position());
  Handle<Code> ic(Builtins::builtin(Builtins::KeyedLoadIC_Initialize));
  __ call(ic, RelocInfo::CODE_TARGET);
  __ nop();
}


void FullCodeGenerator::EmitBinaryOp(Token::Value op,
                                     Expression::Context context) {
  __ push(result_register());
  GenericBinaryOpStub stub(op,
                           NO_OVERWRITE,
                           NO_GENERIC_BINARY_FLAGS,
                           TypeInfo::Unknown());
  __ CallStub(&stub);
  Apply(context, eax);
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
      __ push(eax);  // Preserve value.
      VisitForValue(prop->obj(), kAccumulator);
      __ mov(edx, eax);
      __ pop(eax);  // Restore value.
      __ mov(ecx, prop->key()->AsLiteral()->handle());
      Handle<Code> ic(Builtins::builtin(Builtins::StoreIC_Initialize));
      __ call(ic, RelocInfo::CODE_TARGET);
      __ nop();  // Signal no inlined code.
      break;
    }
    case KEYED_PROPERTY: {
      __ push(eax);  // Preserve value.
      VisitForValue(prop->obj(), kStack);
      VisitForValue(prop->key(), kAccumulator);
      __ mov(ecx, eax);
      __ pop(edx);
      __ pop(eax);  // Restore value.
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
    // assignment.  Right-hand-side value is passed in eax, variable name in
    // ecx, and the global object on the stack.
    __ mov(ecx, var->name());
    __ mov(edx, CodeGenerator::GlobalObject());
    Handle<Code> ic(Builtins::builtin(Builtins::StoreIC_Initialize));
    __ call(ic, RelocInfo::CODE_TARGET);
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
          __ mov(edx, Operand(ebp, SlotOffset(slot)));
          __ cmp(edx, Factory::the_hole_value());
          __ j(not_equal, &done);
        }
        // Perform the assignment.
        __ mov(Operand(ebp, SlotOffset(slot)), eax);
        break;

      case Slot::CONTEXT: {
        MemOperand target = EmitSlotSearch(slot, ecx);
        if (op == Token::INIT_CONST) {
          // Detect const reinitialization by checking for the hole value.
          __ mov(edx, target);
          __ cmp(edx, Factory::the_hole_value());
          __ j(not_equal, &done);
        }
        // Perform the assignment and issue the write barrier.
        __ mov(target, eax);
        // The value of the assignment is in eax.  RecordWrite clobbers its
        // register arguments.
        __ mov(edx, eax);
        int offset = FixedArray::kHeaderSize + slot->index() * kPointerSize;
        __ RecordWrite(ecx, offset, edx, ebx);
        break;
      }

      case Slot::LOOKUP:
        // Call the runtime for the assignment.  The runtime will ignore
        // const reinitialization.
        __ push(eax);  // Value.
        __ push(esi);  // Context.
        __ push(Immediate(var->name()));
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

  Apply(context, eax);
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
    __ push(Operand(esp, kPointerSize));  // Receiver is now under value.
    __ CallRuntime(Runtime::kToSlowProperties, 1);
    __ pop(result_register());
  }

  // Record source code position before IC call.
  SetSourcePosition(expr->position());
  __ mov(ecx, prop->key()->AsLiteral()->handle());
  if (expr->ends_initialization_block()) {
    __ mov(edx, Operand(esp, 0));
  } else {
    __ pop(edx);
  }
  Handle<Code> ic(Builtins::builtin(Builtins::StoreIC_Initialize));
  __ call(ic, RelocInfo::CODE_TARGET);
  __ nop();

  // If the assignment ends an initialization block, revert to fast case.
  if (expr->ends_initialization_block()) {
    __ push(eax);  // Result of assignment, saved even if not needed.
    __ push(Operand(esp, kPointerSize));  // Receiver is under value.
    __ CallRuntime(Runtime::kToFastProperties, 1);
    __ pop(eax);
    DropAndApply(1, context_, eax);
  } else {
    Apply(context_, eax);
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
    __ push(Operand(esp, 2 * kPointerSize));
    __ CallRuntime(Runtime::kToSlowProperties, 1);
    __ pop(result_register());
  }

  __ pop(ecx);
  if (expr->ends_initialization_block()) {
    __ mov(edx, Operand(esp, 0));  // Leave receiver on the stack for later.
  } else {
    __ pop(edx);
  }
  // Record source code position before IC call.
  SetSourcePosition(expr->position());
  Handle<Code> ic(Builtins::builtin(Builtins::KeyedStoreIC_Initialize));
  __ call(ic, RelocInfo::CODE_TARGET);
  // This nop signals to the IC that there is no inlined code at the call
  // site for it to patch.
  __ nop();

  // If the assignment ends an initialization block, revert to fast case.
  if (expr->ends_initialization_block()) {
    __ pop(edx);
    __ push(eax);  // Result of assignment, saved even if not needed.
    __ push(edx);
    __ CallRuntime(Runtime::kToFastProperties, 1);
    __ pop(eax);
  }

  Apply(context_, eax);
}


void FullCodeGenerator::VisitProperty(Property* expr) {
  Comment cmnt(masm_, "[ Property");
  Expression* key = expr->key();

  if (key->IsPropertyName()) {
    VisitForValue(expr->obj(), kAccumulator);
    EmitNamedPropertyLoad(expr);
    Apply(context_, eax);
  } else {
    VisitForValue(expr->obj(), kStack);
    VisitForValue(expr->key(), kAccumulator);
    __ pop(edx);
    EmitKeyedPropertyLoad(expr);
    Apply(context_, eax);
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
  __ Set(ecx, Immediate(name));
  // Record source position of the IC call.
  SetSourcePosition(expr->position());
  InLoopFlag in_loop = (loop_depth() > 0) ? IN_LOOP : NOT_IN_LOOP;
  Handle<Code> ic = CodeGenerator::ComputeCallInitialize(arg_count, in_loop);
  __ call(ic, mode);
  // Restore context register.
  __ mov(esi, Operand(ebp, StandardFrameConstants::kContextOffset));
  Apply(context_, eax);
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
  __ mov(ecx, eax);
  // Record source position of the IC call.
  SetSourcePosition(expr->position());
  InLoopFlag in_loop = (loop_depth() > 0) ? IN_LOOP : NOT_IN_LOOP;
  Handle<Code> ic = CodeGenerator::ComputeKeyedCallInitialize(
      arg_count, in_loop);
  __ call(ic, mode);
  // Restore context register.
  __ mov(esi, Operand(ebp, StandardFrameConstants::kContextOffset));
  Apply(context_, eax);
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
  __ mov(esi, Operand(ebp, StandardFrameConstants::kContextOffset));
  DropAndApply(1, context_, eax);
}


void FullCodeGenerator::VisitCall(Call* expr) {
  Comment cmnt(masm_, "[ Call");
  Expression* fun = expr->expression();
  Variable* var = fun->AsVariableProxy()->AsVariable();

  if (var != NULL && var->is_possibly_eval()) {
    // In a call to eval, we first call %ResolvePossiblyDirectEval to
    // resolve the function we need to call and the receiver of the
    // call.  Then we call the resolved function using the given
    // arguments.
    VisitForValue(fun, kStack);
    __ push(Immediate(Factory::undefined_value()));  // Reserved receiver slot.

    // Push the arguments.
    ZoneList<Expression*>* args = expr->arguments();
    int arg_count = args->length();
    for (int i = 0; i < arg_count; i++) {
      VisitForValue(args->at(i), kStack);
    }

    // Push copy of the function - found below the arguments.
    __ push(Operand(esp, (arg_count + 1) * kPointerSize));

    // Push copy of the first argument or undefined if it doesn't exist.
    if (arg_count > 0) {
      __ push(Operand(esp, arg_count * kPointerSize));
    } else {
      __ push(Immediate(Factory::undefined_value()));
    }

    // Push the receiver of the enclosing function and do runtime call.
    __ push(Operand(ebp, (2 + scope()->num_parameters()) * kPointerSize));
    __ CallRuntime(Runtime::kResolvePossiblyDirectEval, 3);

    // The runtime call returns a pair of values in eax (function) and
    // edx (receiver). Touch up the stack with the right values.
    __ mov(Operand(esp, (arg_count + 0) * kPointerSize), edx);
    __ mov(Operand(esp, (arg_count + 1) * kPointerSize), eax);

    // Record source position for debugger.
    SetSourcePosition(expr->position());
    InLoopFlag in_loop = (loop_depth() > 0) ? IN_LOOP : NOT_IN_LOOP;
    CallFunctionStub stub(arg_count, in_loop, RECEIVER_MIGHT_BE_VALUE);
    __ CallStub(&stub);
    // Restore context register.
    __ mov(esi, Operand(ebp, StandardFrameConstants::kContextOffset));
    DropAndApply(1, context_, eax);
  } else if (var != NULL && !var->is_this() && var->is_global()) {
    // Push global object as receiver for the call IC.
    __ push(CodeGenerator::GlobalObject());
    EmitCallWithIC(expr, var->name(), RelocInfo::CODE_TARGET_CONTEXT);
  } else if (var != NULL && var->slot() != NULL &&
             var->slot()->type() == Slot::LOOKUP) {
    // Call to a lookup slot (dynamically introduced variable).  Call the
    // runtime to find the function to call (returned in eax) and the object
    // holding it (returned in edx).
    __ push(context_register());
    __ push(Immediate(var->name()));
    __ CallRuntime(Runtime::kLoadContextSlot, 2);
    __ push(eax);  // Function.
    __ push(edx);  // Receiver.
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
      // for a regular property use keyed CallIC.
      VisitForValue(prop->obj(), kStack);
      if (prop->is_synthetic()) {
        VisitForValue(prop->key(), kAccumulator);
        // Record source code position for IC call.
        SetSourcePosition(prop->position());
        __ pop(edx);  // We do not need to keep the receiver.

        Handle<Code> ic(Builtins::builtin(Builtins::KeyedLoadIC_Initialize));
        __ call(ic, RelocInfo::CODE_TARGET);
        // By emitting a nop we make sure that we do not have a "test eax,..."
        // instruction after the call as it is treated specially
        // by the LoadIC code.
        __ nop();
        // Push result (function).
        __ push(eax);
        // Push Global receiver.
        __ mov(ecx, CodeGenerator::GlobalObject());
        __ push(FieldOperand(ecx, GlobalObject::kGlobalReceiverOffset));
        EmitCallWithStub(expr);
      } else {
        EmitKeyedCallWithIC(expr, prop->key(), RelocInfo::CODE_TARGET);
      }
    }
  } else {
    // Call to some other expression.  If the expression is an anonymous
    // function literal not called in a loop, mark it as one that should
    // also use the full code generator.
    FunctionLiteral* lit = fun->AsFunctionLiteral();
    if (lit != NULL &&
        lit->name()->Equals(Heap::empty_string()) &&
        loop_depth() == 0) {
      lit->set_try_full_codegen(true);
    }
    VisitForValue(fun, kStack);
    // Load global receiver object.
    __ mov(ebx, CodeGenerator::GlobalObject());
    __ push(FieldOperand(ebx, GlobalObject::kGlobalReceiverOffset));
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

  // Load function, arg_count into edi and eax.
  __ Set(eax, Immediate(arg_count));
  // Function is in esp[arg_count + 1].
  __ mov(edi, Operand(esp, eax, times_pointer_size, kPointerSize));

  Handle<Code> construct_builtin(Builtins::builtin(Builtins::JSConstructCall));
  __ call(construct_builtin, RelocInfo::CONSTRUCT_CALL);

  // Replace function on TOS with result in eax, or pop it.
  DropAndApply(1, context_, eax);
}


void FullCodeGenerator::EmitIsSmi(ZoneList<Expression*>* args) {
  ASSERT(args->length() == 1);

  VisitForValue(args->at(0), kAccumulator);

  Label materialize_true, materialize_false;
  Label* if_true = NULL;
  Label* if_false = NULL;
  PrepareTest(&materialize_true, &materialize_false, &if_true, &if_false);

  __ test(eax, Immediate(kSmiTagMask));
  __ j(zero, if_true);
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

  __ test(eax, Immediate(kSmiTagMask | 0x80000000));
  __ j(zero, if_true);
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

  __ test(eax, Immediate(kSmiTagMask));
  __ j(zero, if_false);
  __ cmp(eax, Factory::null_value());
  __ j(equal, if_true);
  __ mov(ebx, FieldOperand(eax, HeapObject::kMapOffset));
  // Undetectable objects behave like undefined when tested with typeof.
  __ movzx_b(ecx, FieldOperand(ebx, Map::kBitFieldOffset));
  __ test(ecx, Immediate(1 << Map::kIsUndetectable));
  __ j(not_zero, if_false);
  __ movzx_b(ecx, FieldOperand(ebx, Map::kInstanceTypeOffset));
  __ cmp(ecx, FIRST_JS_OBJECT_TYPE);
  __ j(below, if_false);
  __ cmp(ecx, LAST_JS_OBJECT_TYPE);
  __ j(below_equal, if_true);
  __ jmp(if_false);

  Apply(context_, if_true, if_false);
}


void FullCodeGenerator::EmitIsSpecObject(ZoneList<Expression*>* args) {
  ASSERT(args->length() == 1);

  VisitForValue(args->at(0), kAccumulator);

  Label materialize_true, materialize_false;
  Label* if_true = NULL;
  Label* if_false = NULL;
  PrepareTest(&materialize_true, &materialize_false, &if_true, &if_false);

  __ test(eax, Immediate(kSmiTagMask));
  __ j(equal, if_false);
  __ CmpObjectType(eax, FIRST_JS_OBJECT_TYPE, ebx);
  __ j(above_equal, if_true);
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

  __ test(eax, Immediate(kSmiTagMask));
  __ j(zero, if_false);
  __ mov(ebx, FieldOperand(eax, HeapObject::kMapOffset));
  __ movzx_b(ebx, FieldOperand(ebx, Map::kBitFieldOffset));
  __ test(ebx, Immediate(1 << Map::kIsUndetectable));
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

  __ test(eax, Immediate(kSmiTagMask));
  __ j(zero, if_false);
  __ CmpObjectType(eax, JS_FUNCTION_TYPE, ebx);
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

  __ test(eax, Immediate(kSmiTagMask));
  __ j(equal, if_false);
  __ CmpObjectType(eax, JS_ARRAY_TYPE, ebx);
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

  __ test(eax, Immediate(kSmiTagMask));
  __ j(equal, if_false);
  __ CmpObjectType(eax, JS_REGEXP_TYPE, ebx);
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
  __ mov(eax, Operand(ebp, StandardFrameConstants::kCallerFPOffset));

  // Skip the arguments adaptor frame if it exists.
  Label check_frame_marker;
  __ cmp(Operand(eax, StandardFrameConstants::kContextOffset),
         Immediate(Smi::FromInt(StackFrame::ARGUMENTS_ADAPTOR)));
  __ j(not_equal, &check_frame_marker);
  __ mov(eax, Operand(eax, StandardFrameConstants::kCallerFPOffset));

  // Check the marker in the calling frame.
  __ bind(&check_frame_marker);
  __ cmp(Operand(eax, StandardFrameConstants::kMarkerOffset),
         Immediate(Smi::FromInt(StackFrame::CONSTRUCT)));
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

  __ pop(ebx);
  __ cmp(eax, Operand(ebx));
  __ j(equal, if_true);
  __ jmp(if_false);

  Apply(context_, if_true, if_false);
}


void FullCodeGenerator::EmitArguments(ZoneList<Expression*>* args) {
  ASSERT(args->length() == 1);

  // ArgumentsAccessStub expects the key in edx and the formal
  // parameter count in eax.
  VisitForValue(args->at(0), kAccumulator);
  __ mov(edx, eax);
  __ mov(eax, Immediate(Smi::FromInt(scope()->num_parameters())));
  ArgumentsAccessStub stub(ArgumentsAccessStub::READ_ELEMENT);
  __ CallStub(&stub);
  Apply(context_, eax);
}


void FullCodeGenerator::EmitArgumentsLength(ZoneList<Expression*>* args) {
  ASSERT(args->length() == 0);

  Label exit;
  // Get the number of formal parameters.
  __ Set(eax, Immediate(Smi::FromInt(scope()->num_parameters())));

  // Check if the calling frame is an arguments adaptor frame.
  __ mov(ebx, Operand(ebp, StandardFrameConstants::kCallerFPOffset));
  __ cmp(Operand(ebx, StandardFrameConstants::kContextOffset),
         Immediate(Smi::FromInt(StackFrame::ARGUMENTS_ADAPTOR)));
  __ j(not_equal, &exit);

  // Arguments adaptor case: Read the arguments length from the
  // adaptor frame.
  __ mov(eax, Operand(ebx, ArgumentsAdaptorFrameConstants::kLengthOffset));

  __ bind(&exit);
  if (FLAG_debug_code) __ AbortIfNotSmi(eax);
  Apply(context_, eax);
}


void FullCodeGenerator::EmitClassOf(ZoneList<Expression*>* args) {
  ASSERT(args->length() == 1);
  Label done, null, function, non_function_constructor;

  VisitForValue(args->at(0), kAccumulator);

  // If the object is a smi, we return null.
  __ test(eax, Immediate(kSmiTagMask));
  __ j(zero, &null);

  // Check that the object is a JS object but take special care of JS
  // functions to make sure they have 'Function' as their class.
  __ CmpObjectType(eax, FIRST_JS_OBJECT_TYPE, eax);  // Map is now in eax.
  __ j(below, &null);

  // As long as JS_FUNCTION_TYPE is the last instance type and it is
  // right after LAST_JS_OBJECT_TYPE, we can avoid checking for
  // LAST_JS_OBJECT_TYPE.
  ASSERT(LAST_TYPE == JS_FUNCTION_TYPE);
  ASSERT(JS_FUNCTION_TYPE == LAST_JS_OBJECT_TYPE + 1);
  __ CmpInstanceType(eax, JS_FUNCTION_TYPE);
  __ j(equal, &function);

  // Check if the constructor in the map is a function.
  __ mov(eax, FieldOperand(eax, Map::kConstructorOffset));
  __ CmpObjectType(eax, JS_FUNCTION_TYPE, ebx);
  __ j(not_equal, &non_function_constructor);

  // eax now contains the constructor function. Grab the
  // instance class name from there.
  __ mov(eax, FieldOperand(eax, JSFunction::kSharedFunctionInfoOffset));
  __ mov(eax, FieldOperand(eax, SharedFunctionInfo::kInstanceClassNameOffset));
  __ jmp(&done);

  // Functions have class 'Function'.
  __ bind(&function);
  __ mov(eax, Factory::function_class_symbol());
  __ jmp(&done);

  // Objects with a non-function constructor have class 'Object'.
  __ bind(&non_function_constructor);
  __ mov(eax, Factory::Object_symbol());
  __ jmp(&done);

  // Non-JS objects have class null.
  __ bind(&null);
  __ mov(eax, Factory::null_value());

  // All done.
  __ bind(&done);

  Apply(context_, eax);
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
  __ mov(eax, Factory::undefined_value());
  Apply(context_, eax);
}


void FullCodeGenerator::EmitRandomHeapNumber(ZoneList<Expression*>* args) {
  ASSERT(args->length() == 0);

  Label slow_allocate_heapnumber;
  Label heapnumber_allocated;

  __ AllocateHeapNumber(edi, ebx, ecx, &slow_allocate_heapnumber);
  __ jmp(&heapnumber_allocated);

  __ bind(&slow_allocate_heapnumber);
  // Allocate a heap number.
  __ CallRuntime(Runtime::kNumberAlloc, 0);
  __ mov(edi, eax);

  __ bind(&heapnumber_allocated);

  __ PrepareCallCFunction(0, ebx);
  __ CallCFunction(ExternalReference::random_uint32_function(), 0);

  // Convert 32 random bits in eax to 0.(32 random bits) in a double
  // by computing:
  // ( 1.(20 0s)(32 random bits) x 2^20 ) - (1.0 x 2^20)).
  // This is implemented on both SSE2 and FPU.
  if (CpuFeatures::IsSupported(SSE2)) {
    CpuFeatures::Scope fscope(SSE2);
    __ mov(ebx, Immediate(0x49800000));  // 1.0 x 2^20 as single.
    __ movd(xmm1, Operand(ebx));
    __ movd(xmm0, Operand(eax));
    __ cvtss2sd(xmm1, xmm1);
    __ pxor(xmm0, xmm1);
    __ subsd(xmm0, xmm1);
    __ movdbl(FieldOperand(edi, HeapNumber::kValueOffset), xmm0);
  } else {
    // 0x4130000000000000 is 1.0 x 2^20 as a double.
    __ mov(FieldOperand(edi, HeapNumber::kExponentOffset),
           Immediate(0x41300000));
    __ mov(FieldOperand(edi, HeapNumber::kMantissaOffset), eax);
    __ fld_d(FieldOperand(edi, HeapNumber::kValueOffset));
    __ mov(FieldOperand(edi, HeapNumber::kMantissaOffset), Immediate(0));
    __ fld_d(FieldOperand(edi, HeapNumber::kValueOffset));
    __ fsubp(1);
    __ fstp_d(FieldOperand(edi, HeapNumber::kValueOffset));
  }
  __ mov(eax, edi);
  Apply(context_, eax);
}


void FullCodeGenerator::EmitSubString(ZoneList<Expression*>* args) {
  // Load the arguments on the stack and call the stub.
  SubStringStub stub;
  ASSERT(args->length() == 3);
  VisitForValue(args->at(0), kStack);
  VisitForValue(args->at(1), kStack);
  VisitForValue(args->at(2), kStack);
  __ CallStub(&stub);
  Apply(context_, eax);
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
  Apply(context_, eax);
}


void FullCodeGenerator::EmitValueOf(ZoneList<Expression*>* args) {
  ASSERT(args->length() == 1);

  VisitForValue(args->at(0), kAccumulator);  // Load the object.

  Label done;
  // If the object is a smi return the object.
  __ test(eax, Immediate(kSmiTagMask));
  __ j(zero, &done);
  // If the object is not a value type, return the object.
  __ CmpObjectType(eax, JS_VALUE_TYPE, ebx);
  __ j(not_equal, &done);
  __ mov(eax, FieldOperand(eax, JSValue::kValueOffset));

  __ bind(&done);
  Apply(context_, eax);
}


void FullCodeGenerator::EmitMathPow(ZoneList<Expression*>* args) {
  // Load the arguments on the stack and call the runtime function.
  ASSERT(args->length() == 2);
  VisitForValue(args->at(0), kStack);
  VisitForValue(args->at(1), kStack);
  __ CallRuntime(Runtime::kMath_pow, 2);
  Apply(context_, eax);
}


void FullCodeGenerator::EmitSetValueOf(ZoneList<Expression*>* args) {
  ASSERT(args->length() == 2);

  VisitForValue(args->at(0), kStack);  // Load the object.
  VisitForValue(args->at(1), kAccumulator);  // Load the value.
  __ pop(ebx);  // eax = value. ebx = object.

  Label done;
  // If the object is a smi, return the value.
  __ test(ebx, Immediate(kSmiTagMask));
  __ j(zero, &done);

  // If the object is not a value type, return the value.
  __ CmpObjectType(ebx, JS_VALUE_TYPE, ecx);
  __ j(not_equal, &done);

  // Store the value.
  __ mov(FieldOperand(ebx, JSValue::kValueOffset), eax);
  // Update the write barrier.  Save the value as it will be
  // overwritten by the write barrier code and is needed afterward.
  __ mov(edx, eax);
  __ RecordWrite(ebx, JSValue::kValueOffset, edx, ecx);

  __ bind(&done);
  Apply(context_, eax);
}


void FullCodeGenerator::EmitNumberToString(ZoneList<Expression*>* args) {
  ASSERT_EQ(args->length(), 1);

  // Load the argument on the stack and call the stub.
  VisitForValue(args->at(0), kStack);

  NumberToStringStub stub;
  __ CallStub(&stub);
  Apply(context_, eax);
}


void FullCodeGenerator::EmitStringCharFromCode(ZoneList<Expression*>* args) {
  ASSERT(args->length() == 1);

  VisitForValue(args->at(0), kAccumulator);

  Label done;
  StringCharFromCodeGenerator generator(eax, ebx);
  generator.GenerateFast(masm_);
  __ jmp(&done);

  NopRuntimeCallHelper call_helper;
  generator.GenerateSlow(masm_, call_helper);

  __ bind(&done);
  Apply(context_, ebx);
}


void FullCodeGenerator::EmitStringCharCodeAt(ZoneList<Expression*>* args) {
  ASSERT(args->length() == 2);

  VisitForValue(args->at(0), kStack);
  VisitForValue(args->at(1), kAccumulator);

  Register object = ebx;
  Register index = eax;
  Register scratch = ecx;
  Register result = edx;

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
  __ Set(result, Immediate(Factory::nan_value()));
  __ jmp(&done);

  __ bind(&need_conversion);
  // Move the undefined value into the result register, which will
  // trigger conversion.
  __ Set(result, Immediate(Factory::undefined_value()));
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

  Register object = ebx;
  Register index = eax;
  Register scratch1 = ecx;
  Register scratch2 = edx;
  Register result = eax;

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
  __ Set(result, Immediate(Factory::empty_string()));
  __ jmp(&done);

  __ bind(&need_conversion);
  // Move smi zero into the result register, which will trigger
  // conversion.
  __ Set(result, Immediate(Smi::FromInt(0)));
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
  Apply(context_, eax);
}


void FullCodeGenerator::EmitStringCompare(ZoneList<Expression*>* args) {
  ASSERT_EQ(2, args->length());

  VisitForValue(args->at(0), kStack);
  VisitForValue(args->at(1), kStack);

  StringCompareStub stub;
  __ CallStub(&stub);
  Apply(context_, eax);
}


void FullCodeGenerator::EmitMathSin(ZoneList<Expression*>* args) {
  // Load the argument on the stack and call the stub.
  TranscendentalCacheStub stub(TranscendentalCache::SIN);
  ASSERT(args->length() == 1);
  VisitForValue(args->at(0), kStack);
  __ CallStub(&stub);
  Apply(context_, eax);
}


void FullCodeGenerator::EmitMathCos(ZoneList<Expression*>* args) {
  // Load the argument on the stack and call the stub.
  TranscendentalCacheStub stub(TranscendentalCache::COS);
  ASSERT(args->length() == 1);
  VisitForValue(args->at(0), kStack);
  __ CallStub(&stub);
  Apply(context_, eax);
}


void FullCodeGenerator::EmitMathSqrt(ZoneList<Expression*>* args) {
  // Load the argument on the stack and call the runtime function.
  ASSERT(args->length() == 1);
  VisitForValue(args->at(0), kStack);
  __ CallRuntime(Runtime::kMath_sqrt, 1);
  Apply(context_, eax);
}


void FullCodeGenerator::EmitCallFunction(ZoneList<Expression*>* args) {
  ASSERT(args->length() >= 2);

  int arg_count = args->length() - 2;  // For receiver and function.
  VisitForValue(args->at(0), kStack);  // Receiver.
  for (int i = 0; i < arg_count; i++) {
    VisitForValue(args->at(i + 1), kStack);
  }
  VisitForValue(args->at(arg_count + 1), kAccumulator);  // Function.

  // InvokeFunction requires function in edi. Move it in there.
  if (!result_register().is(edi)) __ mov(edi, result_register());
  ParameterCount count(arg_count);
  __ InvokeFunction(edi, count, CALL_FUNCTION);
  __ mov(esi, Operand(ebp, StandardFrameConstants::kContextOffset));
  Apply(context_, eax);
}


void FullCodeGenerator::EmitRegExpConstructResult(ZoneList<Expression*>* args) {
  ASSERT(args->length() == 3);
  VisitForValue(args->at(0), kStack);
  VisitForValue(args->at(1), kStack);
  VisitForValue(args->at(2), kStack);
  __ CallRuntime(Runtime::kRegExpConstructResult, 3);
  Apply(context_, eax);
}


void FullCodeGenerator::EmitSwapElements(ZoneList<Expression*>* args) {
  ASSERT(args->length() == 3);
  VisitForValue(args->at(0), kStack);
  VisitForValue(args->at(1), kStack);
  VisitForValue(args->at(2), kStack);
  __ CallRuntime(Runtime::kSwapElements, 3);
  Apply(context_, eax);
}


void FullCodeGenerator::EmitGetFromCache(ZoneList<Expression*>* args) {
  ASSERT_EQ(2, args->length());

  ASSERT_NE(NULL, args->at(0)->AsLiteral());
  int cache_id = Smi::cast(*(args->at(0)->AsLiteral()->handle()))->value();

  Handle<FixedArray> jsfunction_result_caches(
      Top::global_context()->jsfunction_result_caches());
  if (jsfunction_result_caches->length() <= cache_id) {
    __ Abort("Attempt to use undefined cache.");
    __ mov(eax, Factory::undefined_value());
    Apply(context_, eax);
    return;
  }

  VisitForValue(args->at(1), kAccumulator);

  Register key = eax;
  Register cache = ebx;
  Register tmp = ecx;
  __ mov(cache, CodeGenerator::ContextOperand(esi, Context::GLOBAL_INDEX));
  __ mov(cache,
         FieldOperand(cache, GlobalObject::kGlobalContextOffset));
  __ mov(cache,
         CodeGenerator::ContextOperand(
             cache, Context::JSFUNCTION_RESULT_CACHES_INDEX));
  __ mov(cache,
         FieldOperand(cache, FixedArray::OffsetOfElementAt(cache_id)));

  Label done, not_found;
  // tmp now holds finger offset as a smi.
  ASSERT(kSmiTag == 0 && kSmiTagSize == 1);
  __ mov(tmp, FieldOperand(cache, JSFunctionResultCache::kFingerOffset));
  __ cmp(key, CodeGenerator::FixedArrayElementOperand(cache, tmp));
  __ j(not_equal, &not_found);

  __ mov(eax, CodeGenerator::FixedArrayElementOperand(cache, tmp, 1));
  __ jmp(&done);

  __ bind(&not_found);
  // Call runtime to perform the lookup.
  __ push(cache);
  __ push(key);
  __ CallRuntime(Runtime::kGetFromCache, 2);

  __ bind(&done);
  Apply(context_, eax);
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
    __ mov(eax, CodeGenerator::GlobalObject());
    __ push(FieldOperand(eax, GlobalObject::kBuiltinsOffset));
  }

  // Push the arguments ("left-to-right").
  int arg_count = args->length();
  for (int i = 0; i < arg_count; i++) {
    VisitForValue(args->at(i), kStack);
  }

  if (expr->is_jsruntime()) {
    // Call the JS runtime function via a call IC.
    __ Set(ecx, Immediate(expr->name()));
    InLoopFlag in_loop = (loop_depth() > 0) ? IN_LOOP : NOT_IN_LOOP;
    Handle<Code> ic = CodeGenerator::ComputeCallInitialize(arg_count, in_loop);
    __ call(ic, RelocInfo::CODE_TARGET);
      // Restore context register.
    __ mov(esi, Operand(ebp, StandardFrameConstants::kContextOffset));
  } else {
    // Call the C runtime function.
    __ CallRuntime(expr->function(), arg_count);
  }
  Apply(context_, eax);
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
          __ push(Immediate(var->name()));
        } else {
          // Non-global variable.  Call the runtime to look up the context
          // where the variable was introduced.
          __ push(context_register());
          __ push(Immediate(var->name()));
          __ CallRuntime(Runtime::kLookupContext, 2);
          __ push(eax);
          __ push(Immediate(var->name()));
        }
        __ InvokeBuiltin(Builtins::DELETE, CALL_FUNCTION);
        Apply(context_, eax);
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
              __ mov(result_register(), Factory::undefined_value());
              break;
            case kStack:
              __ push(Immediate(Factory::undefined_value()));
              break;
          }
          break;
        case Expression::kTestValue:
          // Value is false so it's needed.
          switch (location_) {
            case kAccumulator:
              __ mov(result_register(), Factory::undefined_value());
              break;
            case kStack:
              __ push(Immediate(Factory::undefined_value()));
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
        __ mov(eax, CodeGenerator::GlobalObject());
        __ mov(ecx, Immediate(proxy->name()));
        Handle<Code> ic(Builtins::builtin(Builtins::LoadIC_Initialize));
        // Use a regular load, not a contextual load, to avoid a reference
        // error.
        __ call(ic, RelocInfo::CODE_TARGET);
        __ push(eax);
      } else if (proxy != NULL &&
                 proxy->var()->slot() != NULL &&
                 proxy->var()->slot()->type() == Slot::LOOKUP) {
        __ push(esi);
        __ push(Immediate(proxy->name()));
        __ CallRuntime(Runtime::kLoadContextSlotNoReferenceError, 2);
        __ push(eax);
      } else {
        // This expression cannot throw a reference error at the top level.
        VisitForValue(expr->expression(), kStack);
      }

      __ CallRuntime(Runtime::kTypeof, 1);
      Apply(context_, eax);
      break;
    }

    case Token::ADD: {
      Comment cmt(masm_, "[ UnaryOperation (ADD)");
      VisitForValue(expr->expression(), kAccumulator);
      Label no_conversion;
      __ test(result_register(), Immediate(kSmiTagMask));
      __ j(zero, &no_conversion);
      __ push(result_register());
      __ InvokeBuiltin(Builtins::TO_NUMBER, CALL_FUNCTION);
      __ bind(&no_conversion);
      Apply(context_, result_register());
      break;
    }

    case Token::SUB: {
      Comment cmt(masm_, "[ UnaryOperation (SUB)");
      bool can_overwrite =
          (expr->expression()->AsBinaryOperation() != NULL &&
           expr->expression()->AsBinaryOperation()->ResultOverwriteAllowed());
      UnaryOverwriteMode overwrite =
          can_overwrite ? UNARY_OVERWRITE : UNARY_NO_OVERWRITE;
      GenericUnaryOpStub stub(Token::SUB, overwrite);
      // GenericUnaryOpStub expects the argument to be in the
      // accumulator register eax.
      VisitForValue(expr->expression(), kAccumulator);
      __ CallStub(&stub);
      Apply(context_, eax);
      break;
    }

    case Token::BIT_NOT: {
      Comment cmt(masm_, "[ UnaryOperation (BIT_NOT)");
      bool can_overwrite =
          (expr->expression()->AsBinaryOperation() != NULL &&
           expr->expression()->AsBinaryOperation()->ResultOverwriteAllowed());
      UnaryOverwriteMode overwrite =
          can_overwrite ? UNARY_OVERWRITE : UNARY_NO_OVERWRITE;
      GenericUnaryOpStub stub(Token::BIT_NOT, overwrite);
      // GenericUnaryOpStub expects the argument to be in the
      // accumulator register eax.
      VisitForValue(expr->expression(), kAccumulator);
      // Avoid calling the stub for Smis.
      Label smi, done;
      __ test(result_register(), Immediate(kSmiTagMask));
      __ j(zero, &smi);
      // Non-smi: call stub leaving result in accumulator register.
      __ CallStub(&stub);
      __ jmp(&done);
      // Perform operation directly on Smis.
      __ bind(&smi);
      __ not_(result_register());
      __ and_(result_register(), ~kSmiTagMask);  // Remove inverted smi-tag.
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
  // Invalid left-hand sides are rewritten to have a 'throw ReferenceError'
  // as the left-hand side.
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
      __ push(Immediate(Smi::FromInt(0)));
    }
    if (assign_type == NAMED_PROPERTY) {
      // Put the object both on the stack and in the accumulator.
      VisitForValue(prop->obj(), kAccumulator);
      __ push(eax);
      EmitNamedPropertyLoad(prop);
    } else {
      VisitForValue(prop->obj(), kStack);
      VisitForValue(prop->key(), kAccumulator);
      __ mov(edx, Operand(esp, 0));
      __ push(eax);
      EmitKeyedPropertyLoad(prop);
    }
  }

  // Call ToNumber only if operand is not a smi.
  Label no_conversion;
  __ test(eax, Immediate(kSmiTagMask));
  __ j(zero, &no_conversion);
  __ push(eax);
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
            __ push(eax);
            break;
          case NAMED_PROPERTY:
            __ mov(Operand(esp, kPointerSize), eax);
            break;
          case KEYED_PROPERTY:
            __ mov(Operand(esp, 2 * kPointerSize), eax);
            break;
        }
        break;
    }
  }

  // Inline smi case if we are in a loop.
  Label stub_call, done;
  if (loop_depth() > 0) {
    if (expr->op() == Token::INC) {
      __ add(Operand(eax), Immediate(Smi::FromInt(1)));
    } else {
      __ sub(Operand(eax), Immediate(Smi::FromInt(1)));
    }
    __ j(overflow, &stub_call);
    // We could eliminate this smi check if we split the code at
    // the first smi check before calling ToNumber.
    __ test(eax, Immediate(kSmiTagMask));
    __ j(zero, &done);
    __ bind(&stub_call);
    // Call stub. Undo operation first.
    if (expr->op() == Token::INC) {
      __ sub(Operand(eax), Immediate(Smi::FromInt(1)));
    } else {
      __ add(Operand(eax), Immediate(Smi::FromInt(1)));
    }
  }
  // Call stub for +1/-1.
  GenericBinaryOpStub stub(expr->binary_op(),
                           NO_OVERWRITE,
                           NO_GENERIC_BINARY_FLAGS,
                           TypeInfo::Unknown());
  stub.GenerateCall(masm(), eax, Smi::FromInt(1));
  __ bind(&done);

  // Store the value returned in eax.
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
      __ mov(ecx, prop->key()->AsLiteral()->handle());
      __ pop(edx);
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
        Apply(context_, eax);
      }
      break;
    }
    case KEYED_PROPERTY: {
      __ pop(ecx);
      __ pop(edx);
      Handle<Code> ic(Builtins::builtin(Builtins::KeyedStoreIC_Initialize));
      __ call(ic, RelocInfo::CODE_TARGET);
      // This nop signals to the IC that there is no inlined code at the call
      // site for it to patch.
      __ nop();
      if (expr->is_postfix()) {
        // Result is on the stack
        if (context_ != Expression::kEffect) {
          ApplyTOS(context_);
        }
      } else {
        Apply(context_, eax);
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
  __ cmp(obj, Operand(null_const));
  if (strict) {
    __ j(equal, if_true);
  } else {
    __ j(equal, if_true);
    __ cmp(obj, Factory::undefined_value());
    __ j(equal, if_true);
    __ test(obj, Immediate(kSmiTagMask));
    __ j(zero, if_false);
    // It can be an undetectable object.
    __ mov(scratch, FieldOperand(obj, HeapObject::kMapOffset));
    __ movzx_b(scratch, FieldOperand(scratch, Map::kBitFieldOffset));
    __ test(scratch, Immediate(1 << Map::kIsUndetectable));
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
      __ cmp(eax, Factory::true_value());
      __ j(equal, if_true);
      __ jmp(if_false);
      break;

    case Token::INSTANCEOF: {
      VisitForValue(expr->right(), kStack);
      InstanceofStub stub;
      __ CallStub(&stub);
      __ test(eax, Operand(eax));
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
          // Fall through
        case Token::EQ: {
          cc = equal;
          __ pop(edx);
          // If either operand is constant null we do a fast compare
          // against null.
          Literal* right_literal = expr->right()->AsLiteral();
          Literal* left_literal = expr->left()->AsLiteral();
          if (right_literal != NULL && right_literal->handle()->IsNull()) {
            EmitNullCompare(strict, edx, eax, if_true, if_false, ecx);
            Apply(context_, if_true, if_false);
            return;
          } else if (left_literal != NULL && left_literal->handle()->IsNull()) {
            EmitNullCompare(strict, eax, edx, if_true, if_false, ecx);
            Apply(context_, if_true, if_false);
            return;
          }
          break;
        }
        case Token::LT:
          cc = less;
          __ pop(edx);
          break;
        case Token::GT:
          // Reverse left and right sizes to obtain ECMA-262 conversion order.
          cc = less;
          __ mov(edx, result_register());
          __ pop(eax);
         break;
        case Token::LTE:
          // Reverse left and right sizes to obtain ECMA-262 conversion order.
          cc = greater_equal;
          __ mov(edx, result_register());
          __ pop(eax);
          break;
        case Token::GTE:
          cc = greater_equal;
          __ pop(edx);
          break;
        case Token::IN:
        case Token::INSTANCEOF:
        default:
          UNREACHABLE();
      }

      // The comparison stub expects the smi vs. smi case to be handled
      // before it is called.
      Label slow_case;
      __ mov(ecx, Operand(edx));
      __ or_(ecx, Operand(eax));
      __ test(ecx, Immediate(kSmiTagMask));
      __ j(not_zero, &slow_case, not_taken);
      __ cmp(edx, Operand(eax));
      __ j(cc, if_true);
      __ jmp(if_false);

      __ bind(&slow_case);
      CompareStub stub(cc, strict);
      __ CallStub(&stub);
      __ test(eax, Operand(eax));
      __ j(cc, if_true);
      __ jmp(if_false);
    }
  }

  // Convert the result of the comparison into one expected for this
  // expression's context.
  Apply(context_, if_true, if_false);
}


void FullCodeGenerator::VisitThisFunction(ThisFunction* expr) {
  __ mov(eax, Operand(ebp, JavaScriptFrameConstants::kFunctionOffset));
  Apply(context_, eax);
}


Register FullCodeGenerator::result_register() { return eax; }


Register FullCodeGenerator::context_register() { return esi; }


void FullCodeGenerator::StoreToFrameField(int frame_offset, Register value) {
  ASSERT_EQ(POINTER_SIZE_ALIGN(frame_offset), frame_offset);
  __ mov(Operand(ebp, frame_offset), value);
}


void FullCodeGenerator::LoadContextField(Register dst, int context_index) {
  __ mov(dst, CodeGenerator::ContextOperand(esi, context_index));
}


// ----------------------------------------------------------------------------
// Non-local control flow support.

void FullCodeGenerator::EnterFinallyBlock() {
  // Cook return address on top of stack (smi encoded Code* delta)
  ASSERT(!result_register().is(edx));
  __ mov(edx, Operand(esp, 0));
  __ sub(Operand(edx), Immediate(masm_->CodeObject()));
  ASSERT_EQ(1, kSmiTagSize + kSmiShiftSize);
  ASSERT_EQ(0, kSmiTag);
  __ add(edx, Operand(edx));  // Convert to smi.
  __ mov(Operand(esp, 0), edx);
  // Store result register while executing finally block.
  __ push(result_register());
}


void FullCodeGenerator::ExitFinallyBlock() {
  ASSERT(!result_register().is(edx));
  // Restore result register from stack.
  __ pop(result_register());
  // Uncook return address.
  __ mov(edx, Operand(esp, 0));
  __ sar(edx, 1);  // Convert smi to int.
  __ add(Operand(edx), Immediate(masm_->CodeObject()));
  __ mov(Operand(esp, 0), edx);
  // And return.
  __ ret(0);
}


#undef __

} }  // namespace v8::internal

#endif  // V8_TARGET_ARCH_IA32
