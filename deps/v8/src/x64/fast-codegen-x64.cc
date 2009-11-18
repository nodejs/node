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

#include "codegen-inl.h"
#include "compiler.h"
#include "debug.h"
#include "fast-codegen.h"
#include "parser.h"

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
void FastCodeGenerator::Generate(FunctionLiteral* fun) {
  function_ = fun;
  SetFunctionPosition(fun);

  __ push(rbp);  // Caller's frame pointer.
  __ movq(rbp, rsp);
  __ push(rsi);  // Callee's context.
  __ push(rdi);  // Callee's JS Function.

  { Comment cmnt(masm_, "[ Allocate locals");
    int locals_count = fun->scope()->num_stack_slots();
    if (locals_count <= 1) {
      if (locals_count > 0) {
        __ PushRoot(Heap::kUndefinedValueRootIndex);
      }
    } else {
      __ LoadRoot(rdx, Heap::kUndefinedValueRootIndex);
      for (int i = 0; i < locals_count; i++) {
        __ push(rdx);
      }
    }
  }

  bool function_in_register = true;

  Variable* arguments = fun->scope()->arguments()->AsVariable();
  if (arguments != NULL) {
    // Function uses arguments object.
    Comment cmnt(masm_, "[ Allocate arguments object");
    __ push(rdi);
    // The receiver is just before the parameters on the caller's stack.
    __ lea(rdx, Operand(rbp, StandardFrameConstants::kCallerSPOffset +
                                 fun->num_parameters() * kPointerSize));
    __ push(rdx);
    __ Push(Smi::FromInt(fun->num_parameters()));
    // Arguments to ArgumentsAccessStub:
    //   function, receiver address, parameter count.
    // The stub will rewrite receiver and parameter count if the previous
    // stack frame was an arguments adapter frame.
    ArgumentsAccessStub stub(ArgumentsAccessStub::NEW_OBJECT);
    __ CallStub(&stub);
    // Store new arguments object in both "arguments" and ".arguments" slots.
    __ movq(Operand(rbp, SlotOffset(arguments->slot())), rax);
    Slot* dot_arguments_slot =
        fun->scope()->arguments_shadow()->AsVariable()->slot();
    __ movq(Operand(rbp, SlotOffset(dot_arguments_slot)), rax);
    function_in_register = false;
  }

  // Possibly allocate a local context.
  if (fun->scope()->num_heap_slots() > 0) {
    Comment cmnt(masm_, "[ Allocate local context");
    if (function_in_register) {
      // Argument to NewContext is the function, still in rdi.
      __ push(rdi);
    } else {
      // Argument to NewContext is the function, no longer in rdi.
      __ push(Operand(rbp, JavaScriptFrameConstants::kFunctionOffset));
    }
    __ CallRuntime(Runtime::kNewContext, 1);
    // Context is returned in both rax and rsi.  It replaces the context
    // passed to us.  It's saved in the stack and kept live in rsi.
    __ movq(Operand(rbp, StandardFrameConstants::kContextOffset), rsi);
#ifdef DEBUG
    // Assert we do not have to copy any parameters into the context.
    for (int i = 0, len = fun->scope()->num_parameters(); i < len; i++) {
      Slot* slot = fun->scope()->parameter(i)->slot();
      ASSERT(slot != NULL && slot->type() != Slot::CONTEXT);
    }
#endif
  }

  { Comment cmnt(masm_, "[ Stack check");
    Label ok;
    __ CompareRoot(rsp, Heap::kStackLimitRootIndex);
    __ j(above_equal, &ok);
    StackCheckStub stub;
    __ CallStub(&stub);
    __ bind(&ok);
  }

  { Comment cmnt(masm_, "[ Declarations");
    VisitDeclarations(fun->scope()->declarations());
  }

  if (FLAG_trace) {
    __ CallRuntime(Runtime::kTraceEnter, 0);
  }

  { Comment cmnt(masm_, "[ Body");
    ASSERT(loop_depth() == 0);
    VisitStatements(fun->body());
    ASSERT(loop_depth() == 0);
  }

  { Comment cmnt(masm_, "[ return <undefined>;");
    // Emit a 'return undefined' in case control fell off the end of the body.
    __ LoadRoot(rax, Heap::kUndefinedValueRootIndex);
    EmitReturnSequence(function_->end_position());
  }
}


void FastCodeGenerator::EmitReturnSequence(int position) {
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
    CodeGenerator::RecordPositions(masm_, position);
    __ RecordJSReturn();
    // Do not use the leave instruction here because it is too short to
    // patch with the code required by the debugger.
    __ movq(rsp, rbp);
    __ pop(rbp);
    __ ret((function_->scope()->num_parameters() + 1) * kPointerSize);
#ifdef ENABLE_DEBUGGER_SUPPORT
    // Add padding that will be overwritten by a debugger breakpoint.  We
    // have just generated "movq rsp, rbp; pop rbp; ret k" with length 7
    // (3 + 1 + 3).
    const int kPadding = Debug::kX64JSReturnSequenceLength - 7;
    for (int i = 0; i < kPadding; ++i) {
      masm_->int3();
    }
    // Check that the size of the code used for returning matches what is
    // expected by the debugger.
    ASSERT_EQ(Debug::kX64JSReturnSequenceLength,
            masm_->SizeOfCodeGeneratedSince(&check_exit_codesize));
#endif
  }
}


void FastCodeGenerator::Move(Expression::Context context, Register source) {
  switch (context) {
    case Expression::kUninitialized:
      UNREACHABLE();
    case Expression::kEffect:
      break;
    case Expression::kValue:
      __ push(source);
      break;
    case Expression::kTest:
      TestAndBranch(source, true_label_, false_label_);
      break;
    case Expression::kValueTest: {
      Label discard;
      __ push(source);
      TestAndBranch(source, true_label_, &discard);
      __ bind(&discard);
      __ addq(rsp, Immediate(kPointerSize));
      __ jmp(false_label_);
      break;
    }
    case Expression::kTestValue: {
      Label discard;
      __ push(source);
      TestAndBranch(source, &discard, false_label_);
      __ bind(&discard);
      __ addq(rsp, Immediate(kPointerSize));
      __ jmp(true_label_);
      break;
    }
  }
}


void FastCodeGenerator::Move(Expression::Context context, Slot* source) {
  switch (context) {
    case Expression::kUninitialized:
      UNREACHABLE();
    case Expression::kEffect:
      break;
    case Expression::kValue:
      __ push(Operand(rbp, SlotOffset(source)));
      break;
    case Expression::kTest:  // Fall through.
    case Expression::kValueTest:  // Fall through.
    case Expression::kTestValue:
      __ movq(rax, Operand(rbp, SlotOffset(source)));
      Move(context, rax);
      break;
  }
}


void FastCodeGenerator::Move(Expression::Context context, Literal* expr) {
  switch (context) {
    case Expression::kUninitialized:
      UNREACHABLE();
    case Expression::kEffect:
      break;
    case Expression::kValue:
      __ Push(expr->handle());
      break;
    case Expression::kTest:  // Fall through.
    case Expression::kValueTest:  // Fall through.
    case Expression::kTestValue:
      __ Move(rax, expr->handle());
      Move(context, rax);
      break;
  }
}


void FastCodeGenerator::DropAndMove(Expression::Context context,
                                    Register source) {
  switch (context) {
    case Expression::kUninitialized:
      UNREACHABLE();
    case Expression::kEffect:
      __ addq(rsp, Immediate(kPointerSize));
      break;
    case Expression::kValue:
      __ movq(Operand(rsp, 0), source);
      break;
    case Expression::kTest:
      ASSERT(!source.is(rsp));
      __ addq(rsp, Immediate(kPointerSize));
      TestAndBranch(source, true_label_, false_label_);
      break;
    case Expression::kValueTest: {
      Label discard;
      __ movq(Operand(rsp, 0), source);
      TestAndBranch(source, true_label_, &discard);
      __ bind(&discard);
      __ addq(rsp, Immediate(kPointerSize));
      __ jmp(false_label_);
      break;
    }
    case Expression::kTestValue: {
      Label discard;
      __ movq(Operand(rsp, 0), source);
      TestAndBranch(source, &discard, false_label_);
      __ bind(&discard);
      __ addq(rsp, Immediate(kPointerSize));
      __ jmp(true_label_);
      break;
    }
  }
}


void FastCodeGenerator::TestAndBranch(Register source,
                                      Label* true_label,
                                      Label* false_label) {
  ASSERT_NE(NULL, true_label);
  ASSERT_NE(NULL, false_label);
  // Use the shared ToBoolean stub to compile the value in the register into
  // control flow to the code generator's true and false labels.  Perform
  // the fast checks assumed by the stub.

  // The undefined value is false.
  __ CompareRoot(source, Heap::kUndefinedValueRootIndex);
  __ j(equal, false_label);
  __ CompareRoot(source, Heap::kTrueValueRootIndex);  // True is true.
  __ j(equal, true_label);
  __ CompareRoot(source, Heap::kFalseValueRootIndex);  // False is false.
  __ j(equal, false_label);
  ASSERT_EQ(0, kSmiTag);
  __ SmiCompare(source, Smi::FromInt(0));  // The smi zero is false.
  __ j(equal, false_label);
  Condition is_smi = masm_->CheckSmi(source);  // All other smis are true.
  __ j(is_smi, true_label);

  // Call the stub for all other cases.
  __ push(source);
  ToBooleanStub stub;
  __ CallStub(&stub);
  __ testq(rax, rax);  // The stub returns nonzero for true.
  __ j(not_zero, true_label);
  __ jmp(false_label);
}


void FastCodeGenerator::VisitDeclaration(Declaration* decl) {
  Comment cmnt(masm_, "[ Declaration");
  Variable* var = decl->proxy()->var();
  ASSERT(var != NULL);  // Must have been resolved.
  Slot* slot = var->slot();
  ASSERT(slot != NULL);  // No global declarations here.

  // We have 3 cases for slots: LOOKUP, LOCAL, CONTEXT.
  switch (slot->type()) {
    case Slot::LOOKUP: {
      __ push(rsi);
      __ Push(var->name());
      // Declaration nodes are always introduced in one of two modes.
      ASSERT(decl->mode() == Variable::VAR || decl->mode() == Variable::CONST);
      PropertyAttributes attr = decl->mode() == Variable::VAR ?
          NONE : READ_ONLY;
      __ Push(Smi::FromInt(attr));
      // Push initial value, if any.
      // Note: For variables we must not push an initial value (such as
      // 'undefined') because we may have a (legal) redeclaration and we
      // must not destroy the current value.
      if (decl->mode() == Variable::CONST) {
        __ Push(Factory::the_hole_value());
      } else if (decl->fun() != NULL) {
        Visit(decl->fun());
      } else {
        __ Push(Smi::FromInt(0));  // no initial value!
      }
      __ CallRuntime(Runtime::kDeclareContextSlot, 4);
      break;
    }
    case Slot::LOCAL:
      if (decl->mode() == Variable::CONST) {
        __ Move(Operand(rbp, SlotOffset(var->slot())),
                Factory::the_hole_value());
      } else if (decl->fun() != NULL) {
        Visit(decl->fun());
        __ pop(Operand(rbp, SlotOffset(var->slot())));
      }
      break;
    case Slot::CONTEXT:
      // The variable in the decl always resides in the current context.
      ASSERT(function_->scope()->ContextChainLength(slot->var()->scope()) == 0);
      if (decl->mode() == Variable::CONST) {
        __ Move(rax, Factory::the_hole_value());
        if (FLAG_debug_code) {
          // Check if we have the correct context pointer.
          __ movq(rbx, CodeGenerator::ContextOperand(
              rsi, Context::FCONTEXT_INDEX));
          __ cmpq(rbx, rsi);
          __ Check(equal, "Unexpected declaration in current context.");
        }
        __ movq(CodeGenerator::ContextOperand(rsi, slot->index()), rax);
        // No write barrier since the_hole_value is in old space.
        ASSERT(Heap::InNewSpace(*Factory::the_hole_value()));
      } else if (decl->fun() != NULL) {
        Visit(decl->fun());
        __ pop(rax);
        if (FLAG_debug_code) {
          // Check if we have the correct context pointer.
          __ movq(rbx, CodeGenerator::ContextOperand(
              rsi, Context::FCONTEXT_INDEX));
          __ cmpq(rbx, rsi);
          __ Check(equal, "Unexpected declaration in current context.");
        }
        __ movq(CodeGenerator::ContextOperand(rsi, slot->index()), rax);
        int offset = FixedArray::kHeaderSize + slot->index() * kPointerSize;
        __ RecordWrite(rsi, offset, rax, rcx);
      }
      break;
    default:
      UNREACHABLE();
  }
}


void FastCodeGenerator::DeclareGlobals(Handle<FixedArray> pairs) {
  // Call the runtime to declare the globals.
  __ push(rsi);  // The context is the first argument.
  __ Push(pairs);
  __ Push(Smi::FromInt(is_eval_ ? 1 : 0));
  __ CallRuntime(Runtime::kDeclareGlobals, 3);
  // Return value is ignored.
}


void FastCodeGenerator::VisitReturnStatement(ReturnStatement* stmt) {
  Comment cmnt(masm_, "[ ReturnStatement");
  Expression* expr = stmt->expression();
  if (expr->AsLiteral() != NULL) {
    __ Move(rax, expr->AsLiteral()->handle());
  } else {
    Visit(expr);
    ASSERT_EQ(Expression::kValue, expr->context());
    __ pop(rax);
  }
  EmitReturnSequence(stmt->statement_pos());
}


void FastCodeGenerator::VisitFunctionLiteral(FunctionLiteral* expr) {
  Comment cmnt(masm_, "[ FunctionLiteral");

  // Build the function boilerplate and instantiate it.
  Handle<JSFunction> boilerplate =
      Compiler::BuildBoilerplate(expr, script_, this);
  if (HasStackOverflow()) return;

  ASSERT(boilerplate->IsBoilerplate());

  // Create a new closure.
  __ push(rsi);
  __ Push(boilerplate);
  __ CallRuntime(Runtime::kNewClosure, 2);
  Move(expr->context(), rax);
}


void FastCodeGenerator::VisitVariableProxy(VariableProxy* expr) {
  Comment cmnt(masm_, "[ VariableProxy");
  Expression* rewrite = expr->var()->rewrite();
  if (rewrite == NULL) {
    ASSERT(expr->var()->is_global());
    Comment cmnt(masm_, "Global variable");
    // Use inline caching. Variable name is passed in rcx and the global
    // object on the stack.
    __ push(CodeGenerator::GlobalObject());
    __ Move(rcx, expr->name());
    Handle<Code> ic(Builtins::builtin(Builtins::LoadIC_Initialize));
    __ Call(ic, RelocInfo::CODE_TARGET_CONTEXT);
    // A test rax instruction following the call is used by the IC to
    // indicate that the inobject property case was inlined.  Ensure there
    // is no test rax instruction here.
    __ nop();

    DropAndMove(expr->context(), rax);
  } else if (rewrite->AsSlot() != NULL) {
    Slot* slot = rewrite->AsSlot();
    switch (slot->type()) {
      case Slot::LOCAL:
      case Slot::PARAMETER: {
        Comment cmnt(masm_, "Stack slot");
        Move(expr->context(), slot);
        break;
      }

      case Slot::CONTEXT: {
        Comment cmnt(masm_, "Context slot");
         int chain_length =
            function_->scope()->ContextChainLength(slot->var()->scope());
        if (chain_length > 0) {
          // Move up the chain of contexts to the context containing the slot.
          __ movq(rax,
                  Operand(rsi, Context::SlotOffset(Context::CLOSURE_INDEX)));
          // Load the function context (which is the incoming, outer context).
          __ movq(rax, FieldOperand(rax, JSFunction::kContextOffset));
          for (int i = 1; i < chain_length; i++) {
            __ movq(rax,
                    Operand(rax, Context::SlotOffset(Context::CLOSURE_INDEX)));
            __ movq(rax, FieldOperand(rax, JSFunction::kContextOffset));
          }
          // The context may be an intermediate context, not a function context.
          __ movq(rax,
                  Operand(rax, Context::SlotOffset(Context::FCONTEXT_INDEX)));
        } else {  // Slot is in the current function context.
          // The context may be an intermediate context, not a function context.
          __ movq(rax,
                  Operand(rsi, Context::SlotOffset(Context::FCONTEXT_INDEX)));
        }
        __ movq(rax, Operand(rax, Context::SlotOffset(slot->index())));
        Move(expr->context(), rax);
        break;
      }

      case Slot::LOOKUP:
        UNREACHABLE();
        break;
    }
  } else {
    // The parameter variable has been rewritten into an explict access to
    // the arguments object.
    Property* property = rewrite->AsProperty();
    ASSERT_NOT_NULL(property);
    ASSERT_EQ(expr->context(), property->context());
    Visit(property);
  }
}


void FastCodeGenerator::VisitRegExpLiteral(RegExpLiteral* expr) {
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
  // Label done:
  __ bind(&done);
  Move(expr->context(), rax);
}


void FastCodeGenerator::VisitObjectLiteral(ObjectLiteral* expr) {
  Comment cmnt(masm_, "[ ObjectLiteral");
  Label boilerplate_exists;

  __ movq(rdi, Operand(rbp, JavaScriptFrameConstants::kFunctionOffset));
  __ movq(rbx, FieldOperand(rdi, JSFunction::kLiteralsOffset));
  int literal_offset =
    FixedArray::kHeaderSize + expr->literal_index() * kPointerSize;
  __ movq(rax, FieldOperand(rbx, literal_offset));
  __ CompareRoot(rax, Heap::kUndefinedValueRootIndex);
  __ j(not_equal, &boilerplate_exists);
  // Create boilerplate if it does not exist.
  // Literal array (0).
  __ push(rbx);
  // Literal index (1).
  __ Push(Smi::FromInt(expr->literal_index()));
  // Constant properties (2).
  __ Push(expr->constant_properties());
  __ CallRuntime(Runtime::kCreateObjectLiteralBoilerplate, 3);
  __ bind(&boilerplate_exists);
  // rax contains boilerplate.
  // Clone boilerplate.
  __ push(rax);
  if (expr->depth() == 1) {
    __ CallRuntime(Runtime::kCloneShallowLiteralBoilerplate, 1);
  } else {
    __ CallRuntime(Runtime::kCloneLiteralBoilerplate, 1);
  }

  // If result_saved == true: the result is saved on top of the stack.
  // If result_saved == false: the result is not on the stack, just in rax.
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
      case ObjectLiteral::Property::MATERIALIZED_LITERAL:  // fall through
        ASSERT(!CompileTimeValue::IsCompileTimeValue(value));
      case ObjectLiteral::Property::COMPUTED:
        if (key->handle()->IsSymbol()) {
          Visit(value);
          ASSERT_EQ(Expression::kValue, value->context());
          __ pop(rax);
          __ Move(rcx, key->handle());
          Handle<Code> ic(Builtins::builtin(Builtins::StoreIC_Initialize));
          __ call(ic, RelocInfo::CODE_TARGET);
          // StoreIC leaves the receiver on the stack.
          break;
        }
        // fall through
      case ObjectLiteral::Property::PROTOTYPE:
        __ push(rax);
        Visit(key);
        ASSERT_EQ(Expression::kValue, key->context());
        Visit(value);
        ASSERT_EQ(Expression::kValue, value->context());
        __ CallRuntime(Runtime::kSetProperty, 3);
        __ movq(rax, Operand(rsp, 0));  // Restore result into rax.
        break;
      case ObjectLiteral::Property::SETTER:  // fall through
      case ObjectLiteral::Property::GETTER:
        __ push(rax);
        Visit(key);
        ASSERT_EQ(Expression::kValue, key->context());
        __ Push(property->kind() == ObjectLiteral::Property::SETTER ?
                Smi::FromInt(1) :
                Smi::FromInt(0));
        Visit(value);
        ASSERT_EQ(Expression::kValue, value->context());
        __ CallRuntime(Runtime::kDefineAccessor, 4);
        __ movq(rax, Operand(rsp, 0));  // Restore result into rax.
        break;
      default: UNREACHABLE();
    }
  }
  switch (expr->context()) {
    case Expression::kUninitialized:
      UNREACHABLE();
    case Expression::kEffect:
      if (result_saved) __ addq(rsp, Immediate(kPointerSize));
      break;
    case Expression::kValue:
      if (!result_saved) __ push(rax);
      break;
    case Expression::kTest:
      if (result_saved) __ pop(rax);
      TestAndBranch(rax, true_label_, false_label_);
      break;
    case Expression::kValueTest: {
      Label discard;
      if (!result_saved) __ push(rax);
      TestAndBranch(rax, true_label_, &discard);
      __ bind(&discard);
      __ addq(rsp, Immediate(kPointerSize));
      __ jmp(false_label_);
      break;
    }
    case Expression::kTestValue: {
      Label discard;
      if (!result_saved) __ push(rax);
      TestAndBranch(rax, &discard, false_label_);
      __ bind(&discard);
      __ addq(rsp, Immediate(kPointerSize));
      __ jmp(true_label_);
      break;
    }
  }
}


void FastCodeGenerator::VisitArrayLiteral(ArrayLiteral* expr) {
  Comment cmnt(masm_, "[ ArrayLiteral");
  Label make_clone;

  // Fetch the function's literals array.
  __ movq(rbx, Operand(rbp, JavaScriptFrameConstants::kFunctionOffset));
  __ movq(rbx, FieldOperand(rbx, JSFunction::kLiteralsOffset));
  // Check if the literal's boilerplate has been instantiated.
  int offset =
      FixedArray::kHeaderSize + (expr->literal_index() * kPointerSize);
  __ movq(rax, FieldOperand(rbx, offset));
  __ CompareRoot(rax, Heap::kUndefinedValueRootIndex);
  __ j(not_equal, &make_clone);

  // Instantiate the boilerplate.
  __ push(rbx);
  __ Push(Smi::FromInt(expr->literal_index()));
  __ Push(expr->literals());
  __ CallRuntime(Runtime::kCreateArrayLiteralBoilerplate, 3);

  __ bind(&make_clone);
  // Clone the boilerplate.
  __ push(rax);
  if (expr->depth() > 1) {
    __ CallRuntime(Runtime::kCloneLiteralBoilerplate, 1);
  } else {
    __ CallRuntime(Runtime::kCloneShallowLiteralBoilerplate, 1);
  }

  bool result_saved = false;  // Is the result saved to the stack?

  // Emit code to evaluate all the non-constant subexpressions and to store
  // them into the newly cloned array.
  ZoneList<Expression*>* subexprs = expr->values();
  for (int i = 0, len = subexprs->length(); i < len; i++) {
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
    Visit(subexpr);
    ASSERT_EQ(Expression::kValue, subexpr->context());

    // Store the subexpression value in the array's elements.
    __ pop(rax);  // Subexpression value.
    __ movq(rbx, Operand(rsp, 0));  // Copy of array literal.
    __ movq(rbx, FieldOperand(rbx, JSObject::kElementsOffset));
    int offset = FixedArray::kHeaderSize + (i * kPointerSize);
    __ movq(FieldOperand(rbx, offset), rax);

    // Update the write barrier for the array store.
    __ RecordWrite(rbx, offset, rax, rcx);
  }

  switch (expr->context()) {
    case Expression::kUninitialized:
      UNREACHABLE();
    case Expression::kEffect:
      if (result_saved) __ addq(rsp, Immediate(kPointerSize));
      break;
    case Expression::kValue:
      if (!result_saved) __ push(rax);
      break;
    case Expression::kTest:
      if (result_saved) __ pop(rax);
      TestAndBranch(rax, true_label_, false_label_);
      break;
    case Expression::kValueTest: {
      Label discard;
      if (!result_saved) __ push(rax);
      TestAndBranch(rax, true_label_, &discard);
      __ bind(&discard);
      __ addq(rsp, Immediate(kPointerSize));
      __ jmp(false_label_);
      break;
    }
    case Expression::kTestValue: {
      Label discard;
      if (!result_saved) __ push(rax);
      TestAndBranch(rax, &discard, false_label_);
      __ bind(&discard);
      __ addq(rsp, Immediate(kPointerSize));
      __ jmp(true_label_);
      break;
    }
  }
}


void FastCodeGenerator::EmitVariableAssignment(Assignment* expr) {
  Variable* var = expr->target()->AsVariableProxy()->AsVariable();
  ASSERT(var != NULL);

  if (var->is_global()) {
    // Assignment to a global variable.  Use inline caching for the
    // assignment.  Right-hand-side value is passed in rax, variable name in
    // rcx, and the global object on the stack.
    __ pop(rax);
    __ Move(rcx, var->name());
    __ push(CodeGenerator::GlobalObject());
    Handle<Code> ic(Builtins::builtin(Builtins::StoreIC_Initialize));
    __ Call(ic, RelocInfo::CODE_TARGET);
    // Overwrite the global object on the stack with the result if needed.
    DropAndMove(expr->context(), rax);

  } else {
    Slot* slot = var->slot();
    ASSERT_NOT_NULL(slot);  // Variables rewritten as properties not handled.
    switch (slot->type()) {
      case Slot::LOCAL:
      case Slot::PARAMETER: {
        switch (expr->context()) {
          case Expression::kUninitialized:
            UNREACHABLE();
          case Expression::kEffect:
            // Perform assignment and discard value.
            __ pop(Operand(rbp, SlotOffset(var->slot())));
            break;
          case Expression::kValue:
            // Perform assignment and preserve value.
            __ movq(rax, Operand(rsp, 0));
            __ movq(Operand(rbp, SlotOffset(var->slot())), rax);
            break;
          case Expression::kTest:
            // Perform assignment and test (and discard) value.
            __ pop(rax);
            __ movq(Operand(rbp, SlotOffset(var->slot())), rax);
            TestAndBranch(rax, true_label_, false_label_);
            break;
          case Expression::kValueTest: {
            Label discard;
            __ movq(rax, Operand(rsp, 0));
            __ movq(Operand(rbp, SlotOffset(var->slot())), rax);
            TestAndBranch(rax, true_label_, &discard);
            __ bind(&discard);
            __ addq(rsp, Immediate(kPointerSize));
            __ jmp(false_label_);
            break;
          }
          case Expression::kTestValue: {
            Label discard;
            __ movq(rax, Operand(rsp, 0));
            __ movq(Operand(rbp, SlotOffset(var->slot())), rax);
            TestAndBranch(rax, &discard, false_label_);
            __ bind(&discard);
            __ addq(rsp, Immediate(kPointerSize));
            __ jmp(true_label_);
            break;
          }
        }
        break;
      }

      case Slot::CONTEXT: {
        int chain_length =
            function_->scope()->ContextChainLength(slot->var()->scope());
        if (chain_length > 0) {
          // Move up the context chain to the context containing the slot.
          __ movq(rax,
                  Operand(rsi, Context::SlotOffset(Context::CLOSURE_INDEX)));
          // Load the function context (which is the incoming, outer context).
          __ movq(rax, FieldOperand(rax, JSFunction::kContextOffset));
          for (int i = 1; i < chain_length; i++) {
            __ movq(rax,
                    Operand(rax, Context::SlotOffset(Context::CLOSURE_INDEX)));
            __ movq(rax, FieldOperand(rax, JSFunction::kContextOffset));
          }
        } else {  // Slot is in the current context.  Generate optimized code.
          __ movq(rax, rsi);  // RecordWrite destroys the object register.
        }
        if (FLAG_debug_code) {
          __ cmpq(rax,
                  Operand(rax, Context::SlotOffset(Context::FCONTEXT_INDEX)));
          __ Check(equal, "Context Slot chain length wrong.");
        }
        __ pop(rcx);
        __ movq(Operand(rax, Context::SlotOffset(slot->index())), rcx);

        // RecordWrite may destroy all its register arguments.
        if (expr->context() == Expression::kValue) {
          __ push(rcx);
        } else if (expr->context() != Expression::kEffect) {
          __ movq(rdx, rcx);
        }
        int offset = FixedArray::kHeaderSize + slot->index() * kPointerSize;
        __ RecordWrite(rax, offset, rcx, rbx);
        if (expr->context() != Expression::kEffect &&
            expr->context() != Expression::kValue) {
          Move(expr->context(), rdx);
        }
        break;
      }

      case Slot::LOOKUP:
        UNREACHABLE();
        break;
    }
  }
}


void FastCodeGenerator::EmitNamedPropertyAssignment(Assignment* expr) {
  // Assignment to a property, using a named store IC.
  Property* prop = expr->target()->AsProperty();
  ASSERT(prop != NULL);
  ASSERT(prop->key()->AsLiteral() != NULL);

  // If the assignment starts a block of assignments to the same object,
  // change to slow case to avoid the quadratic behavior of repeatedly
  // adding fast properties.
  if (expr->starts_initialization_block()) {
    __ push(Operand(rsp, kPointerSize));  // Receiver is under value.
    __ CallRuntime(Runtime::kToSlowProperties, 1);
  }

  __ pop(rax);
  __ Move(rcx, prop->key()->AsLiteral()->handle());
  Handle<Code> ic(Builtins::builtin(Builtins::StoreIC_Initialize));
  __ Call(ic, RelocInfo::CODE_TARGET);

  // If the assignment ends an initialization block, revert to fast case.
  if (expr->ends_initialization_block()) {
    __ push(rax);  // Result of assignment, saved even if not needed.
    __ push(Operand(rsp, kPointerSize));  // Receiver is under value.
    __ CallRuntime(Runtime::kToFastProperties, 1);
    __ pop(rax);
  }

  DropAndMove(expr->context(), rax);
}


void FastCodeGenerator::EmitKeyedPropertyAssignment(Assignment* expr) {
  // Assignment to a property, using a keyed store IC.

  // If the assignment starts a block of assignments to the same object,
  // change to slow case to avoid the quadratic behavior of repeatedly
  // adding fast properties.
  if (expr->starts_initialization_block()) {
    // Reciever is under the key and value.
    __ push(Operand(rsp, 2 * kPointerSize));
    __ CallRuntime(Runtime::kToSlowProperties, 1);
  }

  __ pop(rax);
  Handle<Code> ic(Builtins::builtin(Builtins::KeyedStoreIC_Initialize));
  __ Call(ic, RelocInfo::CODE_TARGET);
  // This nop signals to the IC that there is no inlined code at the call
  // site for it to patch.
  __ nop();

  // If the assignment ends an initialization block, revert to fast case.
  if (expr->ends_initialization_block()) {
    __ push(rax);  // Result of assignment, saved even if not needed.
    // Reciever is under the key and value.
    __ push(Operand(rsp, 2 * kPointerSize));
    __ CallRuntime(Runtime::kToFastProperties, 1);
    __ pop(rax);
  }

  // Receiver and key are still on stack.
  __ addq(rsp, Immediate(2 * kPointerSize));
  Move(expr->context(), rax);
}


void FastCodeGenerator::VisitProperty(Property* expr) {
  Comment cmnt(masm_, "[ Property");
  Expression* key = expr->key();
  uint32_t dummy;

  // Record the source position for the property load.
  SetSourcePosition(expr->position());

  // Evaluate receiver.
  Visit(expr->obj());


  if (key->AsLiteral() != NULL && key->AsLiteral()->handle()->IsSymbol() &&
      !String::cast(*(key->AsLiteral()->handle()))->AsArrayIndex(&dummy)) {
    // Do a NAMED property load.
    // The IC expects the property name in rcx and the receiver on the stack.
    __ Move(rcx, key->AsLiteral()->handle());
    Handle<Code> ic(Builtins::builtin(Builtins::LoadIC_Initialize));
    __ call(ic, RelocInfo::CODE_TARGET);
    // By emitting a nop we make sure that we do not have a "test rax,..."
    // instruction after the call it is treated specially by the LoadIC code.
    __ nop();
  } else {
    // Do a KEYED property load.
    Visit(expr->key());
    Handle<Code> ic(Builtins::builtin(Builtins::KeyedLoadIC_Initialize));
    __ call(ic, RelocInfo::CODE_TARGET);
    // By emitting a nop we make sure that we do not have a "test rax,..."
    // instruction after the call it is treated specially by the LoadIC code.
    __ nop();
    // Drop key left on the stack by IC.
    __ addq(rsp, Immediate(kPointerSize));
  }
  DropAndMove(expr->context(), rax);
}


void FastCodeGenerator::EmitCallWithIC(Call* expr, RelocInfo::Mode reloc_info) {
  // Code common for calls using the IC.
  ZoneList<Expression*>* args = expr->arguments();
  int arg_count = args->length();
  for (int i = 0; i < arg_count; i++) {
    Visit(args->at(i));
    ASSERT_EQ(Expression::kValue, args->at(i)->context());
  }
  // Record source position for debugger.
  SetSourcePosition(expr->position());
  // Call the IC initialization code.
  Handle<Code> ic = CodeGenerator::ComputeCallInitialize(arg_count,
                                                         NOT_IN_LOOP);
  __ call(ic, reloc_info);
  // Restore context register.
  __ movq(rsi, Operand(rbp, StandardFrameConstants::kContextOffset));
  // Discard the function left on TOS.
  DropAndMove(expr->context(), rax);
}


void FastCodeGenerator::EmitCallWithStub(Call* expr) {
  // Code common for calls using the call stub.
  ZoneList<Expression*>* args = expr->arguments();
  int arg_count = args->length();
  for (int i = 0; i < arg_count; i++) {
    Visit(args->at(i));
  }
  // Record source position for debugger.
  SetSourcePosition(expr->position());
  CallFunctionStub stub(arg_count, NOT_IN_LOOP);
  __ CallStub(&stub);
  // Restore context register.
  __ movq(rsi, Operand(rbp, StandardFrameConstants::kContextOffset));
  // Discard the function left on TOS.
  DropAndMove(expr->context(), rax);
}


void FastCodeGenerator::VisitCall(Call* expr) {
  Comment cmnt(masm_, "[ Call");
  Expression* fun = expr->expression();
  Variable* var = fun->AsVariableProxy()->AsVariable();

  if (var != NULL && var->is_possibly_eval()) {
    // Call to the identifier 'eval'.
    UNREACHABLE();
  } else if (var != NULL && !var->is_this() && var->is_global()) {
    // Call to a global variable.
    __ Push(var->name());
    // Push global object as receiver for the call IC lookup.
    __ push(CodeGenerator::GlobalObject());
    EmitCallWithIC(expr, RelocInfo::CODE_TARGET_CONTEXT);
  } else if (var != NULL && var->slot() != NULL &&
             var->slot()->type() == Slot::LOOKUP) {
    // Call to a lookup slot.
    UNREACHABLE();
  } else if (fun->AsProperty() != NULL) {
    // Call to an object property.
    Property* prop = fun->AsProperty();
    Literal* key = prop->key()->AsLiteral();
    if (key != NULL && key->handle()->IsSymbol()) {
      // Call to a named property, use call IC.
      __ Push(key->handle());
      Visit(prop->obj());
      EmitCallWithIC(expr, RelocInfo::CODE_TARGET);
    } else {
      // Call to a keyed property, use keyed load IC followed by function
      // call.
      Visit(prop->obj());
      Visit(prop->key());
      // Record source code position for IC call.
      SetSourcePosition(prop->position());
      Handle<Code> ic(Builtins::builtin(Builtins::KeyedLoadIC_Initialize));
      __ call(ic, RelocInfo::CODE_TARGET);
      // By emitting a nop we make sure that we do not have a "test eax,..."
      // instruction after the call it is treated specially by the LoadIC code.
      __ nop();
      // Drop key left on the stack by IC.
      __ addq(rsp, Immediate(kPointerSize));
      // Pop receiver.
      __ pop(rbx);
      // Push result (function).
      __ push(rax);
      // Push receiver object on stack.
      if (prop->is_synthetic()) {
        __ push(CodeGenerator::GlobalObject());
      } else {
        __ push(rbx);
      }
      EmitCallWithStub(expr);
    }
  } else {
    // Call to some other expression.  If the expression is an anonymous
    // function literal not called in a loop, mark it as one that should
    // also use the fast code generator.
    FunctionLiteral* lit = fun->AsFunctionLiteral();
    if (lit != NULL &&
        lit->name()->Equals(Heap::empty_string()) &&
        loop_depth() == 0) {
      lit->set_try_fast_codegen(true);
    }
    Visit(fun);
    // Load global receiver object.
    __ movq(rbx, CodeGenerator::GlobalObject());
    __ push(FieldOperand(rbx, GlobalObject::kGlobalReceiverOffset));
    // Emit function call.
    EmitCallWithStub(expr);
  }
}


void FastCodeGenerator::VisitCallNew(CallNew* expr) {
  Comment cmnt(masm_, "[ CallNew");
  // According to ECMA-262, section 11.2.2, page 44, the function
  // expression in new calls must be evaluated before the
  // arguments.
  // Push function on the stack.
  Visit(expr->expression());
  ASSERT_EQ(Expression::kValue, expr->expression()->context());
  // If location is value, already on the stack,

  // Push global object (receiver).
  __ push(CodeGenerator::GlobalObject());

  // Push the arguments ("left-to-right") on the stack.
  ZoneList<Expression*>* args = expr->arguments();
  int arg_count = args->length();
  for (int i = 0; i < arg_count; i++) {
    Visit(args->at(i));
    ASSERT_EQ(Expression::kValue, args->at(i)->context());
    // If location is value, it is already on the stack,
    // so nothing to do here.
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
  DropAndMove(expr->context(), rax);
}


void FastCodeGenerator::VisitCallRuntime(CallRuntime* expr) {
  Comment cmnt(masm_, "[ CallRuntime");
  ZoneList<Expression*>* args = expr->arguments();
  Runtime::Function* function = expr->function();

  ASSERT(function != NULL);

  // Push the arguments ("left-to-right").
  int arg_count = args->length();
  for (int i = 0; i < arg_count; i++) {
    Visit(args->at(i));
    ASSERT_EQ(Expression::kValue, args->at(i)->context());
  }

  __ CallRuntime(function, arg_count);
  Move(expr->context(), rax);
}

void FastCodeGenerator::VisitCountOperation(CountOperation* expr) {
  Comment cmnt(masm_, "[ CountOperation");
  VariableProxy* proxy = expr->expression()->AsVariableProxy();
  ASSERT(proxy->AsVariable() != NULL);
  ASSERT(proxy->AsVariable()->is_global());

  Visit(proxy);
  __ InvokeBuiltin(Builtins::TO_NUMBER, CALL_FUNCTION);

  switch (expr->context()) {
    case Expression::kUninitialized:
      UNREACHABLE();
    case Expression::kValue:  // Fall through
    case Expression::kTest:  // Fall through
    case Expression::kTestValue:  // Fall through
    case Expression::kValueTest:
      // Duplicate the result on the stack.
      __ push(rax);
      break;
    case Expression::kEffect:
      // Do not save result.
      break;
  }
  // Call runtime for +1/-1.
  __ push(rax);
  __ Push(Smi::FromInt(1));
  if (expr->op() == Token::INC) {
    __ CallRuntime(Runtime::kNumberAdd, 2);
  } else {
    __ CallRuntime(Runtime::kNumberSub, 2);
  }
  // Call Store IC.
  __ Move(rcx, proxy->AsVariable()->name());
  __ push(CodeGenerator::GlobalObject());
  Handle<Code> ic(Builtins::builtin(Builtins::StoreIC_Initialize));
  __ call(ic, RelocInfo::CODE_TARGET);
  // Restore up stack after store IC
  __ addq(rsp, Immediate(kPointerSize));

  switch (expr->context()) {
    case Expression::kUninitialized:
      UNREACHABLE();
    case Expression::kEffect:  // Fall through
    case Expression::kValue:
      // Do nothing. Result in either on the stack for value context
      // or discarded for effect context.
      break;
    case Expression::kTest:
      __ pop(rax);
      TestAndBranch(rax, true_label_, false_label_);
      break;
    case Expression::kValueTest: {
      Label discard;
      __ movq(rax, Operand(rsp, 0));
      TestAndBranch(rax, true_label_, &discard);
      __ bind(&discard);
      __ addq(rsp, Immediate(kPointerSize));
      __ jmp(false_label_);
      break;
    }
    case Expression::kTestValue: {
      Label discard;
      __ movq(rax, Operand(rsp, 0));
      TestAndBranch(rax, &discard, false_label_);
      __ bind(&discard);
      __ addq(rsp, Immediate(kPointerSize));
      __ jmp(true_label_);
      break;
    }
  }
}


void FastCodeGenerator::VisitUnaryOperation(UnaryOperation* expr) {
  switch (expr->op()) {
    case Token::VOID: {
      Comment cmnt(masm_, "[ UnaryOperation (VOID)");
      Visit(expr->expression());
      ASSERT_EQ(Expression::kEffect, expr->expression()->context());
      switch (expr->context()) {
        case Expression::kUninitialized:
          UNREACHABLE();
          break;
        case Expression::kEffect:
          break;
        case Expression::kValue:
          __ PushRoot(Heap::kUndefinedValueRootIndex);
          break;
        case Expression::kTestValue:
          // Value is false so it's needed.
          __ PushRoot(Heap::kUndefinedValueRootIndex);
          // Fall through.
        case Expression::kTest:  // Fall through.
        case Expression::kValueTest:
          __ jmp(false_label_);
          break;
      }
      break;
    }

    case Token::NOT: {
      Comment cmnt(masm_, "[ UnaryOperation (NOT)");
      ASSERT_EQ(Expression::kTest, expr->expression()->context());

      Label push_true;
      Label push_false;
      Label done;
      Label* saved_true = true_label_;
      Label* saved_false = false_label_;
      switch (expr->context()) {
        case Expression::kUninitialized:
          UNREACHABLE();
          break;

        case Expression::kValue:
          true_label_ = &push_false;
          false_label_ = &push_true;
          Visit(expr->expression());
          __ bind(&push_true);
          __ PushRoot(Heap::kTrueValueRootIndex);
          __ jmp(&done);
          __ bind(&push_false);
          __ PushRoot(Heap::kFalseValueRootIndex);
          __ bind(&done);
          break;

        case Expression::kEffect:
          true_label_ = &done;
          false_label_ = &done;
          Visit(expr->expression());
          __ bind(&done);
          break;

        case Expression::kTest:
          true_label_ = saved_false;
          false_label_ = saved_true;
          Visit(expr->expression());
          break;

        case Expression::kValueTest:
          true_label_ = saved_false;
          false_label_ = &push_true;
          Visit(expr->expression());
          __ bind(&push_true);
          __ PushRoot(Heap::kTrueValueRootIndex);
          __ jmp(saved_true);
          break;

        case Expression::kTestValue:
          true_label_ = &push_false;
          false_label_ = saved_true;
          Visit(expr->expression());
          __ bind(&push_false);
          __ PushRoot(Heap::kFalseValueRootIndex);
          __ jmp(saved_false);
          break;
      }
      true_label_ = saved_true;
      false_label_ = saved_false;
      break;
    }

    case Token::TYPEOF: {
      Comment cmnt(masm_, "[ UnaryOperation (TYPEOF)");
      ASSERT_EQ(Expression::kValue, expr->expression()->context());

      VariableProxy* proxy = expr->expression()->AsVariableProxy();
      if (proxy != NULL &&
          !proxy->var()->is_this() &&
          proxy->var()->is_global()) {
        Comment cmnt(masm_, "Global variable");
        __ push(CodeGenerator::GlobalObject());
        __ Move(rcx, proxy->name());
        Handle<Code> ic(Builtins::builtin(Builtins::LoadIC_Initialize));
        // Use a regular load, not a contextual load, to avoid a reference
        // error.
        __ Call(ic, RelocInfo::CODE_TARGET);
        __ movq(Operand(rsp, 0), rax);
      } else if (proxy != NULL &&
                 proxy->var()->slot() != NULL &&
                 proxy->var()->slot()->type() == Slot::LOOKUP) {
        __ push(rsi);
        __ Push(proxy->name());
        __ CallRuntime(Runtime::kLoadContextSlotNoReferenceError, 2);
        __ push(rax);
      } else {
        // This expression cannot throw a reference error at the top level.
        Visit(expr->expression());
      }

      __ CallRuntime(Runtime::kTypeof, 1);
      Move(expr->context(), rax);
      break;
    }

    default:
      UNREACHABLE();
  }
}


void FastCodeGenerator::VisitBinaryOperation(BinaryOperation* expr) {
  Comment cmnt(masm_, "[ BinaryOperation");
  switch (expr->op()) {
    case Token::COMMA:
      ASSERT_EQ(Expression::kEffect, expr->left()->context());
      ASSERT_EQ(expr->context(), expr->right()->context());
      Visit(expr->left());
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
    case Token::SAR: {
      ASSERT_EQ(Expression::kValue, expr->left()->context());
      ASSERT_EQ(Expression::kValue, expr->right()->context());

      Visit(expr->left());
      Visit(expr->right());
      GenericBinaryOpStub stub(expr->op(),
                               NO_OVERWRITE,
                               NO_GENERIC_BINARY_FLAGS);
      __ CallStub(&stub);
      Move(expr->context(), rax);

      break;
    }
    default:
      UNREACHABLE();
  }
}


void FastCodeGenerator::VisitCompareOperation(CompareOperation* expr) {
  Comment cmnt(masm_, "[ CompareOperation");
  ASSERT_EQ(Expression::kValue, expr->left()->context());
  ASSERT_EQ(Expression::kValue, expr->right()->context());
  Visit(expr->left());
  Visit(expr->right());

  // Convert current context to test context: Pre-test code.
  Label push_true;
  Label push_false;
  Label done;
  Label* saved_true = true_label_;
  Label* saved_false = false_label_;
  switch (expr->context()) {
    case Expression::kUninitialized:
      UNREACHABLE();
      break;

    case Expression::kValue:
      true_label_ = &push_true;
      false_label_ = &push_false;
      break;

    case Expression::kEffect:
      true_label_ = &done;
      false_label_ = &done;
      break;

    case Expression::kTest:
      break;

    case Expression::kValueTest:
      true_label_ = &push_true;
      break;

    case Expression::kTestValue:
      false_label_ = &push_false;
      break;
  }
  // Convert current context to test context: End pre-test code.

  switch (expr->op()) {
    case Token::IN: {
      __ InvokeBuiltin(Builtins::IN, CALL_FUNCTION);
      __ CompareRoot(rax, Heap::kTrueValueRootIndex);
      __ j(equal, true_label_);
      __ jmp(false_label_);
      break;
    }

    case Token::INSTANCEOF: {
      InstanceofStub stub;
      __ CallStub(&stub);
      __ testq(rax, rax);
      __ j(zero, true_label_);  // The stub returns 0 for true.
      __ jmp(false_label_);
      break;
    }

    default: {
      Condition cc = no_condition;
      bool strict = false;
      switch (expr->op()) {
        case Token::EQ_STRICT:
          strict = true;
          // Fall through
        case Token::EQ:
          cc = equal;
          __ pop(rax);
          __ pop(rdx);
          break;
        case Token::LT:
          cc = less;
          __ pop(rax);
          __ pop(rdx);
          break;
        case Token::GT:
          // Reverse left and right sizes to obtain ECMA-262 conversion order.
          cc = less;
          __ pop(rdx);
          __ pop(rax);
         break;
        case Token::LTE:
          // Reverse left and right sizes to obtain ECMA-262 conversion order.
          cc = greater_equal;
          __ pop(rdx);
          __ pop(rax);
          break;
        case Token::GTE:
          cc = greater_equal;
          __ pop(rax);
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
      __ j(cc, true_label_);
      __ jmp(false_label_);

      __ bind(&slow_case);
      CompareStub stub(cc, strict);
      __ CallStub(&stub);
      __ testq(rax, rax);
      __ j(cc, true_label_);
      __ jmp(false_label_);
    }
  }

  // Convert current context to test context: Post-test code.
  switch (expr->context()) {
    case Expression::kUninitialized:
      UNREACHABLE();
      break;

    case Expression::kValue:
      __ bind(&push_true);
      __ PushRoot(Heap::kTrueValueRootIndex);
      __ jmp(&done);
      __ bind(&push_false);
      __ PushRoot(Heap::kFalseValueRootIndex);
      __ bind(&done);
      break;

    case Expression::kEffect:
      __ bind(&done);
      break;

    case Expression::kTest:
      break;

    case Expression::kValueTest:
      __ bind(&push_true);
      __ PushRoot(Heap::kTrueValueRootIndex);
      __ jmp(saved_true);
      break;

    case Expression::kTestValue:
      __ bind(&push_false);
      __ PushRoot(Heap::kFalseValueRootIndex);
      __ jmp(saved_false);
      break;
  }
  true_label_ = saved_true;
  false_label_ = saved_false;
  // Convert current context to test context: End post-test code.
}


#undef __


} }  // namespace v8::internal
