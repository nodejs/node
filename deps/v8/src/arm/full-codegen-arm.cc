// Copyright 2009 the V8 project authors. All rights reserved.
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
void FullCodeGenerator::Generate(CompilationInfo* info, Mode mode) {
  ASSERT(info_ == NULL);
  info_ = info;
  SetFunctionPosition(function());
  Comment cmnt(masm_, "[ function compiled by full code generator");

  if (mode == PRIMARY) {
    int locals_count = scope()->num_stack_slots();

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
    int heap_slots = scope()->num_heap_slots() - Context::MIN_CONTEXT_SLOTS;
    if (heap_slots > 0) {
      Comment cmnt(masm_, "[ Allocate local context");
      // Argument to NewContext is the function, which is in r1.
      __ push(r1);
      if (heap_slots <= FastNewContextStub::kMaximumSlots) {
        FastNewContextStub stub(heap_slots);
        __ CallStub(&stub);
      } else {
        __ CallRuntime(Runtime::kNewContext, 1);
      }
      function_in_register = false;
      // Context is returned in both r0 and cp.  It replaces the context
      // passed to us.  It's saved in the stack and kept live in cp.
      __ str(cp, MemOperand(fp, StandardFrameConstants::kContextOffset));
      // Copy any necessary parameters into the context.
      int num_parameters = scope()->num_parameters();
      for (int i = 0; i < num_parameters; i++) {
        Slot* slot = scope()->parameter(i)->slot();
        if (slot != NULL && slot->type() == Slot::CONTEXT) {
          int parameter_offset = StandardFrameConstants::kCallerSPOffset +
                                   (num_parameters - 1 - i) * kPointerSize;
          // Load parameter from stack.
          __ ldr(r0, MemOperand(fp, parameter_offset));
          // Store it in the context.
          __ mov(r1, Operand(Context::SlotOffset(slot->index())));
          __ str(r0, MemOperand(cp, r1));
          // Update the write barrier. This clobbers all involved
          // registers, so we have use a third register to avoid
          // clobbering cp.
          __ mov(r2, Operand(cp));
          __ RecordWrite(r2, r1, r0);
        }
      }
    }

    Variable* arguments = scope()->arguments()->AsVariable();
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
      int offset = scope()->num_parameters() * kPointerSize;
      __ add(r2, fp,
             Operand(StandardFrameConstants::kCallerSPOffset + offset));
      __ mov(r1, Operand(Smi::FromInt(scope()->num_parameters())));
      __ Push(r3, r2, r1);

      // Arguments to ArgumentsAccessStub:
      //   function, receiver address, parameter count.
      // The stub will rewrite receiever and parameter count if the previous
      // stack frame was an arguments adapter frame.
      ArgumentsAccessStub stub(ArgumentsAccessStub::NEW_OBJECT);
      __ CallStub(&stub);
      // Duplicate the value; move-to-slot operation might clobber registers.
      __ mov(r3, r0);
      Move(arguments->slot(), r0, r1, r2);
      Slot* dot_arguments_slot =
          scope()->arguments_shadow()->AsVariable()->slot();
      Move(dot_arguments_slot, r3, r1, r2);
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

  // Check the stack for overflow or break request.
  // Put the lr setup instruction in the delay slot.  The kInstrSize is
  // added to the implicit 8 byte offset that always applies to operations
  // with pc and gives a return address 12 bytes down.
  { Comment cmnt(masm_, "[ Stack check");
    __ LoadRoot(r2, Heap::kStackLimitRootIndex);
    __ add(lr, pc, Operand(Assembler::kInstrSize));
    __ cmp(sp, Operand(r2));
    StackCheckStub stub;
    __ mov(pc,
           Operand(reinterpret_cast<intptr_t>(stub.GetCode().location()),
                   RelocInfo::CODE_TARGET),
           LeaveCC,
           lo);
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
    // Emit a 'return undefined' in case control fell off the end of the
    // body.
    __ LoadRoot(r0, Heap::kUndefinedValueRootIndex);
  }
  EmitReturnSequence();
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
      int32_t sp_delta = (scope()->num_parameters() + 1) * kPointerSize;
      CodeGenerator::RecordPositions(masm_, function()->end_position());
      __ RecordJSReturn();
      masm_->mov(sp, fp);
      masm_->ldm(ia_w, sp, fp.bit() | lr.bit());
      masm_->add(sp, sp, Operand(sp_delta));
      masm_->Jump(lr);
    }

#ifdef DEBUG
    // Check that the size of the code used for returning matches what is
    // expected by the debugger. If the sp_delts above cannot be encoded in the
    // add instruction the add will generate two instructions.
    int return_sequence_length =
        masm_->InstructionsGeneratedSince(&check_exit_codesize);
    CHECK(return_sequence_length ==
          Assembler::kJSReturnSequenceInstructions ||
          return_sequence_length ==
          Assembler::kJSReturnSequenceInstructions + 1);
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

    case Expression::kValueTest:
    case Expression::kTestValue:
      // Push an extra copy of the value in case it's needed.
      __ push(reg);
      // Fall through.

    case Expression::kTest:
      // We always call the runtime on ARM, so push the value as argument.
      __ push(reg);
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
    case Expression::kValue:
    case Expression::kTest:
    case Expression::kValueTest:
    case Expression::kTestValue:
      // On ARM we have to move the value into a register to do anything
      // with it.
      Move(result_register(), slot);
      Apply(context, result_register());
      break;
  }
}


void FullCodeGenerator::Apply(Expression::Context context, Literal* lit) {
  switch (context) {
    case Expression::kUninitialized:
      UNREACHABLE();
    case Expression::kEffect:
      break;
      // Nothing to do.
    case Expression::kValue:
    case Expression::kTest:
    case Expression::kValueTest:
    case Expression::kTestValue:
      // On ARM we have to move the value into a register to do anything
      // with it.
      __ mov(result_register(), Operand(lit->handle()));
      Apply(context, result_register());
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

    case Expression::kValueTest:
    case Expression::kTestValue:
      // Duplicate the value on the stack in case it's needed.
      __ ldr(ip, MemOperand(sp));
      __ push(ip);
      // Fall through.

    case Expression::kTest:
      DoTest(context);
      break;
  }
}


void FullCodeGenerator::DropAndApply(int count,
                                     Expression::Context context,
                                     Register reg) {
  ASSERT(count > 0);
  ASSERT(!reg.is(sp));
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
          __ str(reg, MemOperand(sp));
          break;
      }
      break;

    case Expression::kTest:
      if (count > 1) __ Drop(count - 1);
      __ str(reg, MemOperand(sp));
      DoTest(context);
      break;

    case Expression::kValueTest:
    case Expression::kTestValue:
      if (count == 1) {
        __ str(reg, MemOperand(sp));
        __ push(reg);
      } else {  // count > 1
        __ Drop(count - 2);
        __ str(reg, MemOperand(sp, kPointerSize));
        __ str(reg, MemOperand(sp));
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
          __ LoadRoot(result_register(), Heap::kTrueValueRootIndex);
          __ jmp(&done);
          __ bind(materialize_false);
          __ LoadRoot(result_register(), Heap::kFalseValueRootIndex);
          break;
        case kStack:
          __ bind(materialize_true);
          __ LoadRoot(ip, Heap::kTrueValueRootIndex);
          __ push(ip);
          __ jmp(&done);
          __ bind(materialize_false);
          __ LoadRoot(ip, Heap::kFalseValueRootIndex);
          __ push(ip);
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
          __ LoadRoot(result_register(), Heap::kTrueValueRootIndex);
          break;
        case kStack:
          __ LoadRoot(ip, Heap::kTrueValueRootIndex);
          __ push(ip);
          break;
      }
      __ jmp(true_label_);
      break;

    case Expression::kTestValue:
      __ bind(materialize_false);
      switch (location_) {
        case kAccumulator:
          __ LoadRoot(result_register(), Heap::kFalseValueRootIndex);
          break;
        case kStack:
          __ LoadRoot(ip, Heap::kFalseValueRootIndex);
          __ push(ip);
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
          __ LoadRoot(ip, value_root_index);
          __ push(ip);
          break;
      }
      break;
    }
    case Expression::kTest:
      __ b(flag ? true_label_ : false_label_);
      break;
    case Expression::kTestValue:
      switch (location_) {
        case kAccumulator:
          // If value is false it's needed.
          if (!flag) __ LoadRoot(result_register(), Heap::kFalseValueRootIndex);
          break;
        case kStack:
          // If value is false it's needed.
          if (!flag) {
            __ LoadRoot(ip, Heap::kFalseValueRootIndex);
            __ push(ip);
          }
          break;
      }
      __ b(flag ? true_label_ : false_label_);
      break;
    case Expression::kValueTest:
      switch (location_) {
        case kAccumulator:
          // If value is true it's needed.
          if (flag) __ LoadRoot(result_register(), Heap::kTrueValueRootIndex);
          break;
        case kStack:
          // If value is true it's needed.
          if (flag) {
            __ LoadRoot(ip, Heap::kTrueValueRootIndex);
            __ push(ip);
          }
          break;
      }
      __ b(flag ? true_label_ : false_label_);
      break;
  }
}


void FullCodeGenerator::DoTest(Expression::Context context) {
  // The value to test is pushed on the stack, and duplicated on the stack
  // if necessary (for value/test and test/value contexts).
  ASSERT_NE(NULL, true_label_);
  ASSERT_NE(NULL, false_label_);

  // Call the runtime to find the boolean value of the source and then
  // translate it into control flow to the pair of labels.
  __ CallRuntime(Runtime::kToBool, 1);
  __ LoadRoot(ip, Heap::kTrueValueRootIndex);
  __ cmp(r0, ip);

  // Complete based on the context.
  switch (context) {
    case Expression::kUninitialized:
    case Expression::kEffect:
    case Expression::kValue:
      UNREACHABLE();

    case Expression::kTest:
      __ b(eq, true_label_);
      __ jmp(false_label_);
      break;

    case Expression::kValueTest: {
      Label discard;
      switch (location_) {
        case kAccumulator:
          __ b(ne, &discard);
          __ pop(result_register());
          __ jmp(true_label_);
          break;
        case kStack:
          __ b(eq, true_label_);
          break;
      }
      __ bind(&discard);
      __ Drop(1);
      __ jmp(false_label_);
      break;
    }

    case Expression::kTestValue: {
      Label discard;
      switch (location_) {
        case kAccumulator:
          __ b(eq, &discard);
          __ pop(result_register());
          __ jmp(false_label_);
          break;
        case kStack:
          __ b(ne, false_label_);
          break;
      }
      __ bind(&discard);
      __ Drop(1);
      __ jmp(true_label_);
      break;
    }
  }
}


MemOperand FullCodeGenerator::EmitSlotSearch(Slot* slot, Register scratch) {
  switch (slot->type()) {
    case Slot::PARAMETER:
    case Slot::LOCAL:
      return MemOperand(fp, SlotOffset(slot));
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
  return MemOperand(r0, 0);
}


void FullCodeGenerator::Move(Register destination, Slot* source) {
  // Use destination as scratch.
  MemOperand slot_operand = EmitSlotSearch(source, destination);
  __ ldr(destination, slot_operand);
}


void FullCodeGenerator::Move(Slot* dst,
                             Register src,
                             Register scratch1,
                             Register scratch2) {
  ASSERT(dst->type() != Slot::LOOKUP);  // Not yet implemented.
  ASSERT(!scratch1.is(src) && !scratch2.is(src));
  MemOperand location = EmitSlotSearch(dst, scratch1);
  __ str(src, location);
  // Emit the write barrier code if the location is in the heap.
  if (dst->type() == Slot::CONTEXT) {
    __ mov(scratch2, Operand(Context::SlotOffset(dst->index())));
    __ RecordWrite(scratch1, scratch2, src);
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
          __ LoadRoot(ip, Heap::kTheHoleValueRootIndex);
          __ str(ip, MemOperand(fp, SlotOffset(slot)));
        } else if (function != NULL) {
          VisitForValue(function, kAccumulator);
          __ str(result_register(), MemOperand(fp, SlotOffset(slot)));
        }
        break;

      case Slot::CONTEXT:
        // We bypass the general EmitSlotSearch because we know more about
        // this specific context.

        // The variable in the decl always resides in the current context.
        ASSERT_EQ(0, scope()->ContextChainLength(variable->scope()));
        if (FLAG_debug_code) {
          // Check if we have the correct context pointer.
          __ ldr(r1,
                 CodeGenerator::ContextOperand(cp, Context::FCONTEXT_INDEX));
          __ cmp(r1, cp);
          __ Check(eq, "Unexpected declaration in current context.");
        }
        if (mode == Variable::CONST) {
          __ LoadRoot(ip, Heap::kTheHoleValueRootIndex);
          __ str(ip, CodeGenerator::ContextOperand(cp, slot->index()));
          // No write barrier since the_hole_value is in old space.
        } else if (function != NULL) {
          VisitForValue(function, kAccumulator);
          __ str(result_register(),
                 CodeGenerator::ContextOperand(cp, slot->index()));
          int offset = Context::SlotOffset(slot->index());
          __ mov(r2, Operand(offset));
          // We know that we have written a function, which is not a smi.
          __ mov(r1, Operand(cp));
          __ RecordWrite(r1, r2, result_register());
        }
        break;

      case Slot::LOOKUP: {
        __ mov(r2, Operand(variable->name()));
        // Declaration nodes are always introduced in one of two modes.
        ASSERT(mode == Variable::VAR ||
               mode == Variable::CONST);
        PropertyAttributes attr =
            (mode == Variable::VAR) ? NONE : READ_ONLY;
        __ mov(r1, Operand(Smi::FromInt(attr)));
        // Push initial value, if any.
        // Note: For variables we must not push an initial value (such as
        // 'undefined') because we may have a (legal) redeclaration and we
        // must not destroy the current value.
        if (mode == Variable::CONST) {
          __ LoadRoot(r0, Heap::kTheHoleValueRootIndex);
          __ Push(cp, r2, r1, r0);
        } else if (function != NULL) {
          __ Push(cp, r2, r1);
          // Push initial value for function declaration.
          VisitForValue(function, kStack);
        } else {
          __ mov(r0, Operand(Smi::FromInt(0)));  // No initial value!
          __ Push(cp, r2, r1, r0);
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
        __ pop(r1);  // Key.
      } else {
        VisitForValue(prop->key(), kAccumulator);
        __ mov(r1, result_register());  // Key.
        __ LoadRoot(result_register(), Heap::kTheHoleValueRootIndex);
      }
      __ pop(r2);  // Receiver.

      Handle<Code> ic(Builtins::builtin(Builtins::KeyedStoreIC_Initialize));
      __ Call(ic, RelocInfo::CODE_TARGET);
      // Value in r0 is ignored (declarations are statements).
    }
  }
}


void FullCodeGenerator::VisitDeclaration(Declaration* decl) {
  EmitDeclaration(decl->proxy()->var(), decl->mode(), decl->fun());
}


void FullCodeGenerator::DeclareGlobals(Handle<FixedArray> pairs) {
  // Call the runtime to declare the globals.
  // The context is the first argument.
  __ mov(r1, Operand(pairs));
  __ mov(r0, Operand(Smi::FromInt(is_eval() ? 1 : 0)));
  __ Push(cp, r1, r0);
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
    __ ldr(r1, MemOperand(sp, 0));  // Switch value.
    __ mov(r2, r1);
    __ orr(r2, r2, r0);
    __ tst(r2, Operand(kSmiTagMask));
    __ b(ne, &slow_case);
    __ cmp(r1, r0);
    __ b(ne, &next_test);
    __ Drop(1);  // Switch value is no longer needed.
    __ b(clause->body_target()->entry_label());

    __ bind(&slow_case);
    CompareStub stub(eq, true);
    __ CallStub(&stub);
    __ tst(r0, r0);
    __ b(ne, &next_test);
    __ Drop(1);  // Switch value is no longer needed.
    __ b(clause->body_target()->entry_label());
  }

  // Discard the test value and jump to the default if present, otherwise to
  // the end of the statement.
  __ bind(&next_test);
  __ Drop(1);  // Switch value is no longer needed.
  if (default_clause == NULL) {
    __ b(nested_statement.break_target());
  } else {
    __ b(default_clause->body_target()->entry_label());
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
  __ LoadRoot(ip, Heap::kUndefinedValueRootIndex);
  __ cmp(r0, ip);
  __ b(eq, &exit);
  __ LoadRoot(ip, Heap::kNullValueRootIndex);
  __ cmp(r0, ip);
  __ b(eq, &exit);

  // Convert the object to a JS object.
  Label convert, done_convert;
  __ BranchOnSmi(r0, &convert);
  __ CompareObjectType(r0, r1, r1, FIRST_JS_OBJECT_TYPE);
  __ b(hs, &done_convert);
  __ bind(&convert);
  __ push(r0);
  __ InvokeBuiltin(Builtins::TO_OBJECT, CALL_JS);
  __ bind(&done_convert);
  __ push(r0);

  // TODO(kasperl): Check cache validity in generated code. This is a
  // fast case for the JSObject::IsSimpleEnum cache validity
  // checks. If we cannot guarantee cache validity, call the runtime
  // system to check cache validity or get the property names in a
  // fixed array.

  // Get the set of properties to enumerate.
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
  __ ldr(r1, FieldMemOperand(r0, Map::kInstanceDescriptorsOffset));
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
  __ b(hs, loop_statement.break_target());

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

  // Convert the entry to a string or null if it isn't a property
  // anymore. If the property has been removed while iterating, we
  // just skip it.
  __ push(r1);  // Enumerable.
  __ push(r3);  // Current entry.
  __ InvokeBuiltin(Builtins::FILTER_KEY, CALL_JS);
  __ mov(r3, Operand(r0));
  __ LoadRoot(ip, Heap::kNullValueRootIndex);
  __ cmp(r3, ip);
  __ b(eq, loop_statement.continue_target());

  // Update the 'each' property or variable from the possibly filtered
  // entry in register r3.
  __ bind(&update_each);
  __ mov(result_register(), r3);
  // Perform the assignment as if via '='.
  EmitAssignment(stmt->each());

  // Generate code for the body of the loop.
  Label stack_limit_hit, stack_check_done;
  Visit(stmt->body());

  __ StackLimitCheck(&stack_limit_hit);
  __ bind(&stack_check_done);

  // Generate code for the going to the next element by incrementing
  // the index (smi) stored on top of the stack.
  __ bind(loop_statement.continue_target());
  __ pop(r0);
  __ add(r0, r0, Operand(Smi::FromInt(1)));
  __ push(r0);
  __ b(&loop);

  // Slow case for the stack limit check.
  StackCheckStub stack_check_stub;
  __ bind(&stack_limit_hit);
  __ CallStub(&stack_check_stub);
  __ b(&stack_check_done);

  // Remove the pointers stored on the stack.
  __ bind(loop_statement.break_target());
  __ Drop(5);

  // Exit and decrement the loop depth.
  __ bind(&exit);
  decrement_loop_depth();
}


void FullCodeGenerator::EmitNewClosure(Handle<SharedFunctionInfo> info) {
  // Use the fast case closure allocation code that allocates in new
  // space for nested functions that don't need literals cloning.
  if (scope()->is_function_scope() && info->num_literals() == 0) {
    FastNewClosureStub stub;
    __ mov(r0, Operand(info));
    __ push(r0);
    __ CallStub(&stub);
  } else {
    __ mov(r0, Operand(info));
    __ Push(cp, r0);
    __ CallRuntime(Runtime::kNewClosure, 2);
  }
  Apply(context_, r0);
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
    // Use inline caching. Variable name is passed in r2 and the global
    // object (receiver) in r0.
    __ ldr(r0, CodeGenerator::GlobalObject());
    __ mov(r2, Operand(var->name()));
    Handle<Code> ic(Builtins::builtin(Builtins::LoadIC_Initialize));
    __ Call(ic, RelocInfo::CODE_TARGET_CONTEXT);
    Apply(context, r0);

  } else if (slot != NULL && slot->type() == Slot::LOOKUP) {
    Comment cmnt(masm_, "Lookup slot");
    __ mov(r1, Operand(var->name()));
    __ Push(cp, r1);  // Context and name.
    __ CallRuntime(Runtime::kLoadContextSlot, 2);
    Apply(context, r0);

  } else if (slot != NULL) {
    Comment cmnt(masm_, (slot->type() == Slot::CONTEXT)
                            ? "Context slot"
                            : "Stack slot");
    if (var->mode() == Variable::CONST) {
       // Constants may be the hole value if they have not been initialized.
       // Unhole them.
       Label done;
       MemOperand slot_operand = EmitSlotSearch(slot, r0);
       __ ldr(r0, slot_operand);
       __ LoadRoot(ip, Heap::kTheHoleValueRootIndex);
       __ cmp(r0, ip);
       __ b(ne, &done);
       __ LoadRoot(r0, Heap::kUndefinedValueRootIndex);
       __ bind(&done);
       Apply(context, r0);
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
    Move(r1, object_slot);

    // Assert that the key is a smi.
    Literal* key_literal = property->key()->AsLiteral();
    ASSERT_NOT_NULL(key_literal);
    ASSERT(key_literal->handle()->IsSmi());

    // Load the key.
    __ mov(r0, Operand(key_literal->handle()));

    // Call keyed load IC. It has arguments key and receiver in r0 and r1.
    Handle<Code> ic(Builtins::builtin(Builtins::KeyedLoadIC_Initialize));
    __ Call(ic, RelocInfo::CODE_TARGET);
    Apply(context, r0);
  }
}


void FullCodeGenerator::VisitRegExpLiteral(RegExpLiteral* expr) {
  Comment cmnt(masm_, "[ RegExpLiteral");
  Label done;
  // Registers will be used as follows:
  // r4 = JS function, literals array
  // r3 = literal index
  // r2 = RegExp pattern
  // r1 = RegExp flags
  // r0 = temp + return value (RegExp literal)
  __ ldr(r0, MemOperand(fp,  JavaScriptFrameConstants::kFunctionOffset));
  __ ldr(r4,  FieldMemOperand(r0, JSFunction::kLiteralsOffset));
  int literal_offset =
    FixedArray::kHeaderSize + expr->literal_index() * kPointerSize;
  __ ldr(r0, FieldMemOperand(r4, literal_offset));
  __ LoadRoot(ip, Heap::kUndefinedValueRootIndex);
  __ cmp(r0, ip);
  __ b(ne, &done);
  __ mov(r3, Operand(Smi::FromInt(expr->literal_index())));
  __ mov(r2, Operand(expr->pattern()));
  __ mov(r1, Operand(expr->flags()));
  __ Push(r4, r3, r2, r1);
  __ CallRuntime(Runtime::kMaterializeRegExpLiteral, 4);
  __ bind(&done);
  Apply(context_, r0);
}


void FullCodeGenerator::VisitObjectLiteral(ObjectLiteral* expr) {
  Comment cmnt(masm_, "[ ObjectLiteral");
  __ ldr(r3, MemOperand(fp,  JavaScriptFrameConstants::kFunctionOffset));
  __ ldr(r3, FieldMemOperand(r3, JSFunction::kLiteralsOffset));
  __ mov(r2, Operand(Smi::FromInt(expr->literal_index())));
  __ mov(r1, Operand(expr->constant_properties()));
  __ mov(r0, Operand(Smi::FromInt(expr->fast_elements() ? 1 : 0)));
  __ Push(r3, r2, r1, r0);
  if (expr->depth() > 1) {
    __ CallRuntime(Runtime::kCreateObjectLiteral, 4);
  } else {
    __ CallRuntime(Runtime::kCreateObjectLiteralShallow, 4);
  }

  // If result_saved is true the result is on top of the stack.  If
  // result_saved is false the result is in r0.
  bool result_saved = false;

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
          VisitForValue(value, kAccumulator);
          __ mov(r2, Operand(key->handle()));
          __ ldr(r1, MemOperand(sp));
          Handle<Code> ic(Builtins::builtin(Builtins::StoreIC_Initialize));
          __ Call(ic, RelocInfo::CODE_TARGET);
          break;
        }
        // Fall through.
      case ObjectLiteral::Property::PROTOTYPE:
        // Duplicate receiver on stack.
        __ ldr(r0, MemOperand(sp));
        __ push(r0);
        VisitForValue(key, kStack);
        VisitForValue(value, kStack);
        __ CallRuntime(Runtime::kSetProperty, 3);
        break;
      case ObjectLiteral::Property::GETTER:
      case ObjectLiteral::Property::SETTER:
        // Duplicate receiver on stack.
        __ ldr(r0, MemOperand(sp));
        __ push(r0);
        VisitForValue(key, kStack);
        __ mov(r1, Operand(property->kind() == ObjectLiteral::Property::SETTER ?
                           Smi::FromInt(1) :
                           Smi::FromInt(0)));
        __ push(r1);
        VisitForValue(value, kStack);
        __ CallRuntime(Runtime::kDefineAccessor, 4);
        break;
    }
  }

  if (result_saved) {
    ApplyTOS(context_);
  } else {
    Apply(context_, r0);
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
      __ push(r0);
      result_saved = true;
    }
    VisitForValue(subexpr, kAccumulator);

    // Store the subexpression value in the array's elements.
    __ ldr(r1, MemOperand(sp));  // Copy of array literal.
    __ ldr(r1, FieldMemOperand(r1, JSObject::kElementsOffset));
    int offset = FixedArray::kHeaderSize + (i * kPointerSize);
    __ str(result_register(), FieldMemOperand(r1, offset));

    // Update the write barrier for the array store with r0 as the scratch
    // register.
    __ mov(r2, Operand(offset));
    __ RecordWrite(r1, r2, result_register());
  }

  if (result_saved) {
    ApplyTOS(context_);
  } else {
    Apply(context_, r0);
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
      // We need the key and receiver on both the stack and in r0 and r1.
      if (expr->is_compound()) {
        VisitForValue(prop->obj(), kStack);
        VisitForValue(prop->key(), kAccumulator);
        __ ldr(r1, MemOperand(sp, 0));
        __ push(r0);
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
  __ mov(r2, Operand(key->handle()));
  // Call load IC. It has arguments receiver and property name r0 and r2.
  Handle<Code> ic(Builtins::builtin(Builtins::LoadIC_Initialize));
  __ Call(ic, RelocInfo::CODE_TARGET);
}


void FullCodeGenerator::EmitKeyedPropertyLoad(Property* prop) {
  SetSourcePosition(prop->position());
  // Call keyed load IC. It has arguments key and receiver in r0 and r1.
  Handle<Code> ic(Builtins::builtin(Builtins::KeyedLoadIC_Initialize));
  __ Call(ic, RelocInfo::CODE_TARGET);
}


void FullCodeGenerator::EmitBinaryOp(Token::Value op,
                                     Expression::Context context) {
  __ pop(r1);
  GenericBinaryOpStub stub(op, NO_OVERWRITE, r1, r0);
  __ CallStub(&stub);
  Apply(context, r0);
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
      __ push(r0);  // Preserve value.
      VisitForValue(prop->obj(), kAccumulator);
      __ mov(r1, r0);
      __ pop(r0);  // Restore value.
      __ mov(r2, Operand(prop->key()->AsLiteral()->handle()));
      Handle<Code> ic(Builtins::builtin(Builtins::StoreIC_Initialize));
      __ Call(ic, RelocInfo::CODE_TARGET);
      break;
    }
    case KEYED_PROPERTY: {
      __ push(r0);  // Preserve value.
      VisitForValue(prop->obj(), kStack);
      VisitForValue(prop->key(), kAccumulator);
      __ mov(r1, r0);
      __ pop(r2);
      __ pop(r0);  // Restore value.
      Handle<Code> ic(Builtins::builtin(Builtins::KeyedStoreIC_Initialize));
      __ Call(ic, RelocInfo::CODE_TARGET);
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
    // assignment.  Right-hand-side value is passed in r0, variable name in
    // r2, and the global object in r1.
    __ mov(r2, Operand(var->name()));
    __ ldr(r1, CodeGenerator::GlobalObject());
    Handle<Code> ic(Builtins::builtin(Builtins::StoreIC_Initialize));
    __ Call(ic, RelocInfo::CODE_TARGET);

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
          __ ldr(r1, MemOperand(fp, SlotOffset(slot)));
          __ LoadRoot(ip, Heap::kTheHoleValueRootIndex);
          __ cmp(r1, ip);
          __ b(ne, &done);
        }
        // Perform the assignment.
        __ str(result_register(), MemOperand(fp, SlotOffset(slot)));
        break;

      case Slot::CONTEXT: {
        MemOperand target = EmitSlotSearch(slot, r1);
        if (op == Token::INIT_CONST) {
          // Detect const reinitialization by checking for the hole value.
          __ ldr(r2, target);
          __ LoadRoot(ip, Heap::kTheHoleValueRootIndex);
          __ cmp(r2, ip);
          __ b(ne, &done);
        }
        // Perform the assignment and issue the write barrier.
        __ str(result_register(), target);
        // RecordWrite may destroy all its register arguments.
        __ mov(r3, result_register());
        int offset = FixedArray::kHeaderSize + slot->index() * kPointerSize;
        __ mov(r2, Operand(offset));
        __ RecordWrite(r1, r2, r3);
        break;
      }

      case Slot::LOOKUP:
        // Call the runtime for the assignment.  The runtime will ignore
        // const reinitialization.
        __ push(r0);  // Value.
        __ mov(r0, Operand(slot->var()->name()));
        __ Push(cp, r0);  // Context and name.
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

  Apply(context, result_register());
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

  Handle<Code> ic(Builtins::builtin(Builtins::StoreIC_Initialize));
  __ Call(ic, RelocInfo::CODE_TARGET);

  // If the assignment ends an initialization block, revert to fast case.
  if (expr->ends_initialization_block()) {
    __ push(r0);  // Result of assignment, saved even if not needed.
    // Receiver is under the result value.
    __ ldr(ip, MemOperand(sp, kPointerSize));
    __ push(ip);
    __ CallRuntime(Runtime::kToFastProperties, 1);
    __ pop(r0);
    DropAndApply(1, context_, r0);
  } else {
    Apply(context_, r0);
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

  Handle<Code> ic(Builtins::builtin(Builtins::KeyedStoreIC_Initialize));
  __ Call(ic, RelocInfo::CODE_TARGET);

  // If the assignment ends an initialization block, revert to fast case.
  if (expr->ends_initialization_block()) {
    __ push(r0);  // Result of assignment, saved even if not needed.
    // Receiver is under the result value.
    __ ldr(ip, MemOperand(sp, kPointerSize));
    __ push(ip);
    __ CallRuntime(Runtime::kToFastProperties, 1);
    __ pop(r0);
    DropAndApply(1, context_, r0);
  } else {
    Apply(context_, r0);
  }
}


void FullCodeGenerator::VisitProperty(Property* expr) {
  Comment cmnt(masm_, "[ Property");
  Expression* key = expr->key();

  if (key->IsPropertyName()) {
    VisitForValue(expr->obj(), kAccumulator);
    EmitNamedPropertyLoad(expr);
    Apply(context_, r0);
  } else {
    VisitForValue(expr->obj(), kStack);
    VisitForValue(expr->key(), kAccumulator);
    __ pop(r1);
    EmitKeyedPropertyLoad(expr);
    Apply(context_, r0);
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
  __ mov(r2, Operand(name));
  // Record source position for debugger.
  SetSourcePosition(expr->position());
  // Call the IC initialization code.
  InLoopFlag in_loop = (loop_depth() > 0) ? IN_LOOP : NOT_IN_LOOP;
  Handle<Code> ic = CodeGenerator::ComputeCallInitialize(arg_count, in_loop);
  __ Call(ic, mode);
  // Restore context register.
  __ ldr(cp, MemOperand(fp, StandardFrameConstants::kContextOffset));
  Apply(context_, r0);
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
  __ mov(r2, r0);
  // Record source position for debugger.
  SetSourcePosition(expr->position());
  // Call the IC initialization code.
  InLoopFlag in_loop = (loop_depth() > 0) ? IN_LOOP : NOT_IN_LOOP;
  Handle<Code> ic = CodeGenerator::ComputeKeyedCallInitialize(arg_count,
                                                              in_loop);
  __ Call(ic, mode);
  // Restore context register.
  __ ldr(cp, MemOperand(fp, StandardFrameConstants::kContextOffset));
  Apply(context_, r0);
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
  __ ldr(cp, MemOperand(fp, StandardFrameConstants::kContextOffset));
  DropAndApply(1, context_, r0);
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
    __ LoadRoot(r2, Heap::kUndefinedValueRootIndex);
    __ push(r2);  // Reserved receiver slot.

    // Push the arguments.
    ZoneList<Expression*>* args = expr->arguments();
    int arg_count = args->length();
    for (int i = 0; i < arg_count; i++) {
      VisitForValue(args->at(i), kStack);
    }

    // Push copy of the function - found below the arguments.
    __ ldr(r1, MemOperand(sp, (arg_count + 1) * kPointerSize));
    __ push(r1);

    // Push copy of the first argument or undefined if it doesn't exist.
    if (arg_count > 0) {
      __ ldr(r1, MemOperand(sp, arg_count * kPointerSize));
      __ push(r1);
    } else {
      __ push(r2);
    }

    // Push the receiver of the enclosing function and do runtime call.
    __ ldr(r1, MemOperand(fp, (2 + scope()->num_parameters()) * kPointerSize));
    __ push(r1);
    __ CallRuntime(Runtime::kResolvePossiblyDirectEval, 3);

    // The runtime call returns a pair of values in r0 (function) and
    // r1 (receiver). Touch up the stack with the right values.
    __ str(r0, MemOperand(sp, (arg_count + 1) * kPointerSize));
    __ str(r1, MemOperand(sp, arg_count * kPointerSize));

    // Record source position for debugger.
    SetSourcePosition(expr->position());
    InLoopFlag in_loop = (loop_depth() > 0) ? IN_LOOP : NOT_IN_LOOP;
    CallFunctionStub stub(arg_count, in_loop, RECEIVER_MIGHT_BE_VALUE);
    __ CallStub(&stub);
    // Restore context register.
    __ ldr(cp, MemOperand(fp, StandardFrameConstants::kContextOffset));
    DropAndApply(1, context_, r0);
  } else if (var != NULL && !var->is_this() && var->is_global()) {
    // Push global object as receiver for the call IC.
    __ ldr(r0, CodeGenerator::GlobalObject());
    __ push(r0);
    EmitCallWithIC(expr, var->name(), RelocInfo::CODE_TARGET_CONTEXT);
  } else if (var != NULL && var->slot() != NULL &&
             var->slot()->type() == Slot::LOOKUP) {
    // Call to a lookup slot (dynamically introduced variable).  Call the
    // runtime to find the function to call (returned in eax) and the object
    // holding it (returned in edx).
    __ push(context_register());
    __ mov(r2, Operand(var->name()));
    __ push(r2);
    __ CallRuntime(Runtime::kLoadContextSlot, 2);
    __ push(r0);  // Function.
    __ push(r1);  // Receiver.
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
        __ pop(r1);  // We do not need to keep the receiver.

        Handle<Code> ic(Builtins::builtin(Builtins::KeyedLoadIC_Initialize));
        __ Call(ic, RelocInfo::CODE_TARGET);
        // Push result (function).
        __ push(r0);
        // Push Global receiver.
        __ ldr(r1, CodeGenerator::GlobalObject());
        __ ldr(r1, FieldMemOperand(r1, GlobalObject::kGlobalReceiverOffset));
        __ push(r1);
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
    __ ldr(r1, CodeGenerator::GlobalObject());
    __ ldr(r1, FieldMemOperand(r1, GlobalObject::kGlobalReceiverOffset));
    __ push(r1);
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
  __ ldr(r0, CodeGenerator::GlobalObject());
  __ push(r0);
  // Push the arguments ("left-to-right") on the stack.
  ZoneList<Expression*>* args = expr->arguments();
  int arg_count = args->length();
  for (int i = 0; i < arg_count; i++) {
    VisitForValue(args->at(i), kStack);
  }

  // Call the construct call builtin that handles allocation and
  // constructor invocation.
  SetSourcePosition(expr->position());

  // Load function, arg_count into r1 and r0.
  __ mov(r0, Operand(arg_count));
  // Function is in sp[arg_count + 1].
  __ ldr(r1, MemOperand(sp, (arg_count + 1) * kPointerSize));

  Handle<Code> construct_builtin(Builtins::builtin(Builtins::JSConstructCall));
  __ Call(construct_builtin, RelocInfo::CONSTRUCT_CALL);

  // Replace function on TOS with result in r0, or pop it.
  DropAndApply(1, context_, r0);
}


void FullCodeGenerator::EmitIsSmi(ZoneList<Expression*>* args) {
  ASSERT(args->length() == 1);

  VisitForValue(args->at(0), kAccumulator);

  Label materialize_true, materialize_false;
  Label* if_true = NULL;
  Label* if_false = NULL;
  PrepareTest(&materialize_true, &materialize_false, &if_true, &if_false);

  __ BranchOnSmi(r0, if_true);
  __ b(if_false);

  Apply(context_, if_true, if_false);
}


void FullCodeGenerator::EmitIsNonNegativeSmi(ZoneList<Expression*>* args) {
  ASSERT(args->length() == 1);

  VisitForValue(args->at(0), kAccumulator);

  Label materialize_true, materialize_false;
  Label* if_true = NULL;
  Label* if_false = NULL;
  PrepareTest(&materialize_true, &materialize_false, &if_true, &if_false);

  __ tst(r0, Operand(kSmiTagMask | 0x80000000));
  __ b(eq, if_true);
  __ b(if_false);

  Apply(context_, if_true, if_false);
}


void FullCodeGenerator::EmitIsObject(ZoneList<Expression*>* args) {
  ASSERT(args->length() == 1);

  VisitForValue(args->at(0), kAccumulator);

  Label materialize_true, materialize_false;
  Label* if_true = NULL;
  Label* if_false = NULL;
  PrepareTest(&materialize_true, &materialize_false, &if_true, &if_false);
  __ BranchOnSmi(r0, if_false);
  __ LoadRoot(ip, Heap::kNullValueRootIndex);
  __ cmp(r0, ip);
  __ b(eq, if_true);
  __ ldr(r2, FieldMemOperand(r0, HeapObject::kMapOffset));
  // Undetectable objects behave like undefined when tested with typeof.
  __ ldrb(r1, FieldMemOperand(r2, Map::kBitFieldOffset));
  __ tst(r1, Operand(1 << Map::kIsUndetectable));
  __ b(ne, if_false);
  __ ldrb(r1, FieldMemOperand(r2, Map::kInstanceTypeOffset));
  __ cmp(r1, Operand(FIRST_JS_OBJECT_TYPE));
  __ b(lt, if_false);
  __ cmp(r1, Operand(LAST_JS_OBJECT_TYPE));
  __ b(le, if_true);
  __ b(if_false);

  Apply(context_, if_true, if_false);
}


void FullCodeGenerator::EmitIsUndetectableObject(ZoneList<Expression*>* args) {
  ASSERT(args->length() == 1);

  VisitForValue(args->at(0), kAccumulator);

  Label materialize_true, materialize_false;
  Label* if_true = NULL;
  Label* if_false = NULL;
  PrepareTest(&materialize_true, &materialize_false, &if_true, &if_false);

  __ BranchOnSmi(r0, if_false);
  __ ldr(r1, FieldMemOperand(r0, HeapObject::kMapOffset));
  __ ldrb(r1, FieldMemOperand(r1, Map::kBitFieldOffset));
  __ tst(r1, Operand(1 << Map::kIsUndetectable));
  __ b(ne, if_true);
  __ b(if_false);

  Apply(context_, if_true, if_false);
}


void FullCodeGenerator::EmitIsFunction(ZoneList<Expression*>* args) {
  ASSERT(args->length() == 1);

  VisitForValue(args->at(0), kAccumulator);

  Label materialize_true, materialize_false;
  Label* if_true = NULL;
  Label* if_false = NULL;
  PrepareTest(&materialize_true, &materialize_false, &if_true, &if_false);

  __ BranchOnSmi(r0, if_false);
  __ CompareObjectType(r0, r1, r1, JS_FUNCTION_TYPE);
  __ b(eq, if_true);
  __ b(if_false);

  Apply(context_, if_true, if_false);
}


void FullCodeGenerator::EmitIsArray(ZoneList<Expression*>* args) {
  ASSERT(args->length() == 1);

  VisitForValue(args->at(0), kAccumulator);

  Label materialize_true, materialize_false;
  Label* if_true = NULL;
  Label* if_false = NULL;
  PrepareTest(&materialize_true, &materialize_false, &if_true, &if_false);

  __ BranchOnSmi(r0, if_false);
  __ CompareObjectType(r0, r1, r1, JS_ARRAY_TYPE);
  __ b(eq, if_true);
  __ b(if_false);

  Apply(context_, if_true, if_false);
}


void FullCodeGenerator::EmitIsRegExp(ZoneList<Expression*>* args) {
  ASSERT(args->length() == 1);

  VisitForValue(args->at(0), kAccumulator);

  Label materialize_true, materialize_false;
  Label* if_true = NULL;
  Label* if_false = NULL;
  PrepareTest(&materialize_true, &materialize_false, &if_true, &if_false);

  __ BranchOnSmi(r0, if_false);
  __ CompareObjectType(r0, r1, r1, JS_REGEXP_TYPE);
  __ b(eq, if_true);
  __ b(if_false);

  Apply(context_, if_true, if_false);
}



void FullCodeGenerator::EmitIsConstructCall(ZoneList<Expression*>* args) {
  ASSERT(args->length() == 0);

  Label materialize_true, materialize_false;
  Label* if_true = NULL;
  Label* if_false = NULL;
  PrepareTest(&materialize_true, &materialize_false, &if_true, &if_false);

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
  __ b(eq, if_true);
  __ b(if_false);

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

  __ pop(r1);
  __ cmp(r0, r1);
  __ b(eq, if_true);
  __ b(if_false);

  Apply(context_, if_true, if_false);
}


void FullCodeGenerator::EmitArguments(ZoneList<Expression*>* args) {
  ASSERT(args->length() == 1);

  // ArgumentsAccessStub expects the key in edx and the formal
  // parameter count in eax.
  VisitForValue(args->at(0), kAccumulator);
  __ mov(r1, r0);
  __ mov(r0, Operand(Smi::FromInt(scope()->num_parameters())));
  ArgumentsAccessStub stub(ArgumentsAccessStub::READ_ELEMENT);
  __ CallStub(&stub);
  Apply(context_, r0);
}


void FullCodeGenerator::EmitArgumentsLength(ZoneList<Expression*>* args) {
  ASSERT(args->length() == 0);

  Label exit;
  // Get the number of formal parameters.
  __ mov(r0, Operand(Smi::FromInt(scope()->num_parameters())));

  // Check if the calling frame is an arguments adaptor frame.
  __ ldr(r2, MemOperand(fp, StandardFrameConstants::kCallerFPOffset));
  __ ldr(r3, MemOperand(r2, StandardFrameConstants::kContextOffset));
  __ cmp(r3, Operand(Smi::FromInt(StackFrame::ARGUMENTS_ADAPTOR)));
  __ b(ne, &exit);

  // Arguments adaptor case: Read the arguments length from the
  // adaptor frame.
  __ ldr(r0, MemOperand(r2, ArgumentsAdaptorFrameConstants::kLengthOffset));

  __ bind(&exit);
  Apply(context_, r0);
}


void FullCodeGenerator::EmitClassOf(ZoneList<Expression*>* args) {
  ASSERT(args->length() == 1);
  Label done, null, function, non_function_constructor;

  VisitForValue(args->at(0), kAccumulator);

  // If the object is a smi, we return null.
  __ BranchOnSmi(r0, &null);

  // Check that the object is a JS object but take special care of JS
  // functions to make sure they have 'Function' as their class.
  __ CompareObjectType(r0, r0, r1, FIRST_JS_OBJECT_TYPE);  // Map is now in r0.
  __ b(lt, &null);

  // As long as JS_FUNCTION_TYPE is the last instance type and it is
  // right after LAST_JS_OBJECT_TYPE, we can avoid checking for
  // LAST_JS_OBJECT_TYPE.
  ASSERT(LAST_TYPE == JS_FUNCTION_TYPE);
  ASSERT(JS_FUNCTION_TYPE == LAST_JS_OBJECT_TYPE + 1);
  __ cmp(r1, Operand(JS_FUNCTION_TYPE));
  __ b(eq, &function);

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
  __ LoadRoot(r0, Heap::kfunction_class_symbolRootIndex);
  __ jmp(&done);

  // Non-JS objects have class null.
  __ bind(&null);
  __ LoadRoot(r0, Heap::kNullValueRootIndex);

  // All done.
  __ bind(&done);

  Apply(context_, r0);
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
  __ LoadRoot(r0, Heap::kUndefinedValueRootIndex);
  Apply(context_, r0);
}


void FullCodeGenerator::EmitRandomHeapNumber(ZoneList<Expression*>* args) {
  ASSERT(args->length() == 0);

  Label slow_allocate_heapnumber;
  Label heapnumber_allocated;

  __ AllocateHeapNumber(r4, r1, r2, &slow_allocate_heapnumber);
  __ jmp(&heapnumber_allocated);

  __ bind(&slow_allocate_heapnumber);
  // To allocate a heap number, and ensure that it is not a smi, we
  // call the runtime function FUnaryMinus on 0, returning the double
  // -0.0. A new, distinct heap number is returned each time.
  __ mov(r0, Operand(Smi::FromInt(0)));
  __ push(r0);
  __ CallRuntime(Runtime::kNumberUnaryMinus, 1);
  __ mov(r4, Operand(r0));

  __ bind(&heapnumber_allocated);

  // Convert 32 random bits in r0 to 0.(32 random bits) in a double
  // by computing:
  // ( 1.(20 0s)(32 random bits) x 2^20 ) - (1.0 x 2^20)).
  if (CpuFeatures::IsSupported(VFP3)) {
    __ PrepareCallCFunction(0, r1);
    __ CallCFunction(ExternalReference::random_uint32_function(), 0);

    CpuFeatures::Scope scope(VFP3);
    // 0x41300000 is the top half of 1.0 x 2^20 as a double.
    // Create this constant using mov/orr to avoid PC relative load.
    __ mov(r1, Operand(0x41000000));
    __ orr(r1, r1, Operand(0x300000));
    // Move 0x41300000xxxxxxxx (x = random bits) to VFP.
    __ vmov(d7, r0, r1);
    // Move 0x4130000000000000 to VFP.
    __ mov(r0, Operand(0));
    __ vmov(d8, r0, r1);
    // Subtract and store the result in the heap number.
    __ vsub(d7, d7, d8);
    __ sub(r0, r4, Operand(kHeapObjectTag));
    __ vstr(d7, r0, HeapNumber::kValueOffset);
    __ mov(r0, r4);
  } else {
    __ mov(r0, Operand(r4));
    __ PrepareCallCFunction(1, r1);
    __ CallCFunction(
        ExternalReference::fill_heap_number_with_random_function(), 1);
  }

  Apply(context_, r0);
}


void FullCodeGenerator::EmitSubString(ZoneList<Expression*>* args) {
  // Load the arguments on the stack and call the stub.
  SubStringStub stub;
  ASSERT(args->length() == 3);
  VisitForValue(args->at(0), kStack);
  VisitForValue(args->at(1), kStack);
  VisitForValue(args->at(2), kStack);
  __ CallStub(&stub);
  Apply(context_, r0);
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
  Apply(context_, r0);
}


void FullCodeGenerator::EmitValueOf(ZoneList<Expression*>* args) {
  ASSERT(args->length() == 1);

  VisitForValue(args->at(0), kAccumulator);  // Load the object.

  Label done;
  // If the object is a smi return the object.
  __ BranchOnSmi(r0, &done);
  // If the object is not a value type, return the object.
  __ CompareObjectType(r0, r1, r1, JS_VALUE_TYPE);
  __ b(ne, &done);
  __ ldr(r0, FieldMemOperand(r0, JSValue::kValueOffset));

  __ bind(&done);
  Apply(context_, r0);
}


void FullCodeGenerator::EmitMathPow(ZoneList<Expression*>* args) {
  // Load the arguments on the stack and call the runtime function.
  ASSERT(args->length() == 2);
  VisitForValue(args->at(0), kStack);
  VisitForValue(args->at(1), kStack);
  __ CallRuntime(Runtime::kMath_pow, 2);
  Apply(context_, r0);
}


void FullCodeGenerator::EmitSetValueOf(ZoneList<Expression*>* args) {
  ASSERT(args->length() == 2);

  VisitForValue(args->at(0), kStack);  // Load the object.
  VisitForValue(args->at(1), kAccumulator);  // Load the value.
  __ pop(r1);  // r0 = value. r1 = object.

  Label done;
  // If the object is a smi, return the value.
  __ BranchOnSmi(r1, &done);

  // If the object is not a value type, return the value.
  __ CompareObjectType(r1, r2, r2, JS_VALUE_TYPE);
  __ b(ne, &done);

  // Store the value.
  __ str(r0, FieldMemOperand(r1, JSValue::kValueOffset));
  // Update the write barrier.  Save the value as it will be
  // overwritten by the write barrier code and is needed afterward.
  __ mov(r2, Operand(JSValue::kValueOffset - kHeapObjectTag));
  __ RecordWrite(r1, r2, r3);

  __ bind(&done);
  Apply(context_, r0);
}


void FullCodeGenerator::EmitNumberToString(ZoneList<Expression*>* args) {
  ASSERT_EQ(args->length(), 1);

  // Load the argument on the stack and call the stub.
  VisitForValue(args->at(0), kStack);

  NumberToStringStub stub;
  __ CallStub(&stub);
  Apply(context_, r0);
}


void FullCodeGenerator::EmitStringCharFromCode(ZoneList<Expression*>* args) {
  ASSERT(args->length() == 1);

  VisitForValue(args->at(0), kAccumulator);

  Label done;
  StringCharFromCodeGenerator generator(r0, r1);
  generator.GenerateFast(masm_);
  __ jmp(&done);

  NopRuntimeCallHelper call_helper;
  generator.GenerateSlow(masm_, call_helper);

  __ bind(&done);
  Apply(context_, r1);
}


void FullCodeGenerator::EmitStringCharCodeAt(ZoneList<Expression*>* args) {
  ASSERT(args->length() == 2);

  VisitForValue(args->at(0), kStack);
  VisitForValue(args->at(1), kAccumulator);

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
  Apply(context_, result);
}


void FullCodeGenerator::EmitStringCharAt(ZoneList<Expression*>* args) {
  ASSERT(args->length() == 2);

  VisitForValue(args->at(0), kStack);
  VisitForValue(args->at(1), kAccumulator);

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
  Apply(context_, result);
}


void FullCodeGenerator::EmitStringAdd(ZoneList<Expression*>* args) {
  ASSERT_EQ(2, args->length());

  VisitForValue(args->at(0), kStack);
  VisitForValue(args->at(1), kStack);

  StringAddStub stub(NO_STRING_ADD_FLAGS);
  __ CallStub(&stub);
  Apply(context_, r0);
}


void FullCodeGenerator::EmitStringCompare(ZoneList<Expression*>* args) {
  ASSERT_EQ(2, args->length());

  VisitForValue(args->at(0), kStack);
  VisitForValue(args->at(1), kStack);

  StringCompareStub stub;
  __ CallStub(&stub);
  Apply(context_, r0);
}


void FullCodeGenerator::EmitMathSin(ZoneList<Expression*>* args) {
  // Load the argument on the stack and call the runtime.
  ASSERT(args->length() == 1);
  VisitForValue(args->at(0), kStack);
  __ CallRuntime(Runtime::kMath_sin, 1);
  Apply(context_, r0);
}


void FullCodeGenerator::EmitMathCos(ZoneList<Expression*>* args) {
  // Load the argument on the stack and call the runtime.
  ASSERT(args->length() == 1);
  VisitForValue(args->at(0), kStack);
  __ CallRuntime(Runtime::kMath_cos, 1);
  Apply(context_, r0);
}


void FullCodeGenerator::EmitMathSqrt(ZoneList<Expression*>* args) {
  // Load the argument on the stack and call the runtime function.
  ASSERT(args->length() == 1);
  VisitForValue(args->at(0), kStack);
  __ CallRuntime(Runtime::kMath_sqrt, 1);
  Apply(context_, r0);
}


void FullCodeGenerator::EmitCallFunction(ZoneList<Expression*>* args) {
  ASSERT(args->length() >= 2);

  int arg_count = args->length() - 2;  // For receiver and function.
  VisitForValue(args->at(0), kStack);  // Receiver.
  for (int i = 0; i < arg_count; i++) {
    VisitForValue(args->at(i + 1), kStack);
  }
  VisitForValue(args->at(arg_count + 1), kAccumulator);  // Function.

  // InvokeFunction requires function in r1. Move it in there.
  if (!result_register().is(r1)) __ mov(r1, result_register());
  ParameterCount count(arg_count);
  __ InvokeFunction(r1, count, CALL_FUNCTION);
  __ ldr(cp, MemOperand(fp, StandardFrameConstants::kContextOffset));
  Apply(context_, r0);
}


void FullCodeGenerator::EmitRegExpConstructResult(ZoneList<Expression*>* args) {
  ASSERT(args->length() == 3);
  VisitForValue(args->at(0), kStack);
  VisitForValue(args->at(1), kStack);
  VisitForValue(args->at(2), kStack);
  __ CallRuntime(Runtime::kRegExpConstructResult, 3);
  Apply(context_, r0);
}


void FullCodeGenerator::EmitSwapElements(ZoneList<Expression*>* args) {
  ASSERT(args->length() == 3);
  VisitForValue(args->at(0), kStack);
  VisitForValue(args->at(1), kStack);
  VisitForValue(args->at(2), kStack);
  __ CallRuntime(Runtime::kSwapElements, 3);
  Apply(context_, r0);
}


void FullCodeGenerator::EmitGetFromCache(ZoneList<Expression*>* args) {
  ASSERT_EQ(2, args->length());

  ASSERT_NE(NULL, args->at(0)->AsLiteral());
  int cache_id = Smi::cast(*(args->at(0)->AsLiteral()->handle()))->value();

  Handle<FixedArray> jsfunction_result_caches(
      Top::global_context()->jsfunction_result_caches());
  if (jsfunction_result_caches->length() <= cache_id) {
    __ Abort("Attempt to use undefined cache.");
    __ LoadRoot(r0, Heap::kUndefinedValueRootIndex);
    Apply(context_, r0);
    return;
  }

  VisitForValue(args->at(1), kAccumulator);

  Register key = r0;
  Register cache = r1;
  __ ldr(cache, CodeGenerator::ContextOperand(cp, Context::GLOBAL_INDEX));
  __ ldr(cache, FieldMemOperand(cache, GlobalObject::kGlobalContextOffset));
  __ ldr(cache,
         CodeGenerator::ContextOperand(
             cache, Context::JSFUNCTION_RESULT_CACHES_INDEX));
  __ ldr(cache,
         FieldMemOperand(cache, FixedArray::OffsetOfElementAt(cache_id)));


  Label done, not_found;
  // tmp now holds finger offset as a smi.
  ASSERT(kSmiTag == 0 && kSmiTagSize == 1);
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
  Apply(context_, r0);
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
    __ ldr(r0, CodeGenerator::GlobalObject());
    __ ldr(r0, FieldMemOperand(r0, GlobalObject::kBuiltinsOffset));
    __ push(r0);
  }

  // Push the arguments ("left-to-right").
  int arg_count = args->length();
  for (int i = 0; i < arg_count; i++) {
    VisitForValue(args->at(i), kStack);
  }

  if (expr->is_jsruntime()) {
    // Call the JS runtime function.
    __ mov(r2, Operand(expr->name()));
    Handle<Code> ic = CodeGenerator::ComputeCallInitialize(arg_count,
                                                           NOT_IN_LOOP);
    __ Call(ic, RelocInfo::CODE_TARGET);
    // Restore context register.
    __ ldr(cp, MemOperand(fp, StandardFrameConstants::kContextOffset));
  } else {
    // Call the C runtime function.
    __ CallRuntime(expr->function(), arg_count);
  }
  Apply(context_, r0);
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
          __ ldr(r1, CodeGenerator::GlobalObject());
          __ mov(r0, Operand(var->name()));
          __ Push(r1, r0);
        } else {
          // Non-global variable.  Call the runtime to look up the context
          // where the variable was introduced.
          __ push(context_register());
          __ mov(r2, Operand(var->name()));
          __ push(r2);
          __ CallRuntime(Runtime::kLookupContext, 2);
          __ push(r0);
          __ mov(r2, Operand(var->name()));
          __ push(r2);
        }
        __ InvokeBuiltin(Builtins::DELETE, CALL_JS);
        Apply(context_, r0);
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
          __ LoadRoot(result_register(), Heap::kUndefinedValueRootIndex);
          switch (location_) {
            case kAccumulator:
              break;
            case kStack:
              __ push(result_register());
              break;
          }
          break;
        case Expression::kTestValue:
          // Value is false so it's needed.
          __ LoadRoot(result_register(), Heap::kUndefinedValueRootIndex);
          switch (location_) {
            case kAccumulator:
              break;
            case kStack:
              __ push(result_register());
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
        __ ldr(r0, CodeGenerator::GlobalObject());
        __ mov(r2, Operand(proxy->name()));
        Handle<Code> ic(Builtins::builtin(Builtins::LoadIC_Initialize));
        // Use a regular load, not a contextual load, to avoid a reference
        // error.
        __ Call(ic, RelocInfo::CODE_TARGET);
        __ push(r0);
      } else if (proxy != NULL &&
                 proxy->var()->slot() != NULL &&
                 proxy->var()->slot()->type() == Slot::LOOKUP) {
        __ mov(r0, Operand(proxy->name()));
        __ Push(cp, r0);
        __ CallRuntime(Runtime::kLoadContextSlotNoReferenceError, 2);
        __ push(r0);
      } else {
        // This expression cannot throw a reference error at the top level.
        VisitForValue(expr->expression(), kStack);
      }

      __ CallRuntime(Runtime::kTypeof, 1);
      Apply(context_, r0);
      break;
    }

    case Token::ADD: {
      Comment cmt(masm_, "[ UnaryOperation (ADD)");
      VisitForValue(expr->expression(), kAccumulator);
      Label no_conversion;
      __ tst(result_register(), Operand(kSmiTagMask));
      __ b(eq, &no_conversion);
      __ push(r0);
      __ InvokeBuiltin(Builtins::TO_NUMBER, CALL_JS);
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
      // accumulator register r0.
      VisitForValue(expr->expression(), kAccumulator);
      __ CallStub(&stub);
      Apply(context_, r0);
      break;
    }

    case Token::BIT_NOT: {
      Comment cmt(masm_, "[ UnaryOperation (BIT_NOT)");
      bool overwrite =
          (expr->expression()->AsBinaryOperation() != NULL &&
           expr->expression()->AsBinaryOperation()->ResultOverwriteAllowed());
      GenericUnaryOpStub stub(Token::BIT_NOT, overwrite);
      // GenericUnaryOpStub expects the argument to be in the
      // accumulator register r0.
      VisitForValue(expr->expression(), kAccumulator);
      // Avoid calling the stub for Smis.
      Label smi, done;
      __ BranchOnSmi(result_register(), &smi);
      // Non-smi: call stub leaving result in accumulator register.
      __ CallStub(&stub);
      __ b(&done);
      // Perform operation directly on Smis.
      __ bind(&smi);
      __ mvn(result_register(), Operand(result_register()));
      // Bit-clear inverted smi-tag.
      __ bic(result_register(), result_register(), Operand(kSmiTagMask));
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
      __ mov(ip, Operand(Smi::FromInt(0)));
      __ push(ip);
    }
    if (assign_type == NAMED_PROPERTY) {
      // Put the object both on the stack and in the accumulator.
      VisitForValue(prop->obj(), kAccumulator);
      __ push(r0);
      EmitNamedPropertyLoad(prop);
    } else {
      VisitForValue(prop->obj(), kStack);
      VisitForValue(prop->key(), kAccumulator);
      __ ldr(r1, MemOperand(sp, 0));
      __ push(r0);
      EmitKeyedPropertyLoad(prop);
    }
  }

  // Call ToNumber only if operand is not a smi.
  Label no_conversion;
  __ BranchOnSmi(r0, &no_conversion);
  __ push(r0);
  __ InvokeBuiltin(Builtins::TO_NUMBER, CALL_JS);
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
            __ push(r0);
            break;
          case NAMED_PROPERTY:
            __ str(r0, MemOperand(sp, kPointerSize));
            break;
          case KEYED_PROPERTY:
            __ str(r0, MemOperand(sp, 2 * kPointerSize));
            break;
        }
        break;
    }
  }


  // Inline smi case if we are in a loop.
  Label stub_call, done;
  int count_value = expr->op() == Token::INC ? 1 : -1;
  if (loop_depth() > 0) {
    __ add(r0, r0, Operand(Smi::FromInt(count_value)), SetCC);
    __ b(vs, &stub_call);
    // We could eliminate this smi check if we split the code at
    // the first smi check before calling ToNumber.
    __ BranchOnSmi(r0, &done);
    __ bind(&stub_call);
    // Call stub. Undo operation first.
    __ sub(r0, r0, Operand(Smi::FromInt(count_value)));
  }
  __ mov(r1, Operand(Smi::FromInt(count_value)));
  GenericBinaryOpStub stub(Token::ADD, NO_OVERWRITE, r1, r0);
  __ CallStub(&stub);
  __ bind(&done);

  // Store the value returned in r0.
  switch (assign_type) {
    case VARIABLE:
      if (expr->is_postfix()) {
        EmitVariableAssignment(expr->expression()->AsVariableProxy()->var(),
                               Token::ASSIGN,
                               Expression::kEffect);
        // For all contexts except kEffect: We have the result on
        // top of the stack.
        if (context_ != Expression::kEffect) {
          ApplyTOS(context_);
        }
      } else {
        EmitVariableAssignment(expr->expression()->AsVariableProxy()->var(),
                               Token::ASSIGN,
                               context_);
      }
      break;
    case NAMED_PROPERTY: {
      __ mov(r2, Operand(prop->key()->AsLiteral()->handle()));
      __ pop(r1);
      Handle<Code> ic(Builtins::builtin(Builtins::StoreIC_Initialize));
      __ Call(ic, RelocInfo::CODE_TARGET);
      if (expr->is_postfix()) {
        if (context_ != Expression::kEffect) {
          ApplyTOS(context_);
        }
      } else {
        Apply(context_, r0);
      }
      break;
    }
    case KEYED_PROPERTY: {
      __ pop(r1);  // Key.
      __ pop(r2);  // Receiver.
      Handle<Code> ic(Builtins::builtin(Builtins::KeyedStoreIC_Initialize));
      __ Call(ic, RelocInfo::CODE_TARGET);
      if (expr->is_postfix()) {
        if (context_ != Expression::kEffect) {
          ApplyTOS(context_);
        }
      } else {
        Apply(context_, r0);
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
  __ cmp(obj, null_const);
  if (strict) {
    __ b(eq, if_true);
  } else {
    __ b(eq, if_true);
    __ LoadRoot(ip, Heap::kUndefinedValueRootIndex);
    __ cmp(obj, ip);
    __ b(eq, if_true);
    __ BranchOnSmi(obj, if_false);
    // It can be an undetectable object.
    __ ldr(scratch, FieldMemOperand(obj, HeapObject::kMapOffset));
    __ ldrb(scratch, FieldMemOperand(scratch, Map::kBitFieldOffset));
    __ tst(scratch, Operand(1 << Map::kIsUndetectable));
    __ b(ne, if_true);
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
      __ InvokeBuiltin(Builtins::IN, CALL_JS);
      __ LoadRoot(ip, Heap::kTrueValueRootIndex);
      __ cmp(r0, ip);
      __ b(eq, if_true);
      __ jmp(if_false);
      break;

    case Token::INSTANCEOF: {
      VisitForValue(expr->right(), kStack);
      InstanceofStub stub;
      __ CallStub(&stub);
      __ tst(r0, r0);
      __ b(eq, if_true);  // The stub returns 0 for true.
      __ jmp(if_false);
      break;
    }

    default: {
      VisitForValue(expr->right(), kAccumulator);
      Condition cc = eq;
      bool strict = false;
      switch (expr->op()) {
        case Token::EQ_STRICT:
          strict = true;
          // Fall through
        case Token::EQ: {
          cc = eq;
          __ pop(r1);
          // If either operand is constant null we do a fast compare
          // against null.
          Literal* right_literal = expr->right()->AsLiteral();
          Literal* left_literal = expr->left()->AsLiteral();
          if (right_literal != NULL && right_literal->handle()->IsNull()) {
            EmitNullCompare(strict, r1, r0, if_true, if_false, r2);
            Apply(context_, if_true, if_false);
            return;
          } else if (left_literal != NULL && left_literal->handle()->IsNull()) {
            EmitNullCompare(strict, r0, r1, if_true, if_false, r2);
            Apply(context_, if_true, if_false);
            return;
          }
          break;
        }
        case Token::LT:
          cc = lt;
          __ pop(r1);
          break;
        case Token::GT:
          // Reverse left and right sides to obtain ECMA-262 conversion order.
          cc = lt;
          __ mov(r1, result_register());
          __ pop(r0);
         break;
        case Token::LTE:
          // Reverse left and right sides to obtain ECMA-262 conversion order.
          cc = ge;
          __ mov(r1, result_register());
          __ pop(r0);
          break;
        case Token::GTE:
          cc = ge;
          __ pop(r1);
          break;
        case Token::IN:
        case Token::INSTANCEOF:
        default:
          UNREACHABLE();
      }

      // The comparison stub expects the smi vs. smi case to be handled
      // before it is called.
      Label slow_case;
      __ orr(r2, r0, Operand(r1));
      __ BranchOnNotSmi(r2, &slow_case);
      __ cmp(r1, r0);
      __ b(cc, if_true);
      __ jmp(if_false);

      __ bind(&slow_case);
      CompareStub stub(cc, strict);
      __ CallStub(&stub);
      __ cmp(r0, Operand(0));
      __ b(cc, if_true);
      __ jmp(if_false);
    }
  }

  // Convert the result of the comparison into one expected for this
  // expression's context.
  Apply(context_, if_true, if_false);
}


void FullCodeGenerator::VisitThisFunction(ThisFunction* expr) {
  __ ldr(r0, MemOperand(fp, JavaScriptFrameConstants::kFunctionOffset));
  Apply(context_, r0);
}


Register FullCodeGenerator::result_register() { return r0; }


Register FullCodeGenerator::context_register() { return cp; }


void FullCodeGenerator::StoreToFrameField(int frame_offset, Register value) {
  ASSERT_EQ(POINTER_SIZE_ALIGN(frame_offset), frame_offset);
  __ str(value, MemOperand(fp, frame_offset));
}


void FullCodeGenerator::LoadContextField(Register dst, int context_index) {
  __ ldr(dst, CodeGenerator::ContextOperand(cp, context_index));
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
  ASSERT_EQ(0, kSmiTag);
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

} }  // namespace v8::internal

#endif  // V8_TARGET_ARCH_ARM
