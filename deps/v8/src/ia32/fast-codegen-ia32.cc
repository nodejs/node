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
#include "fast-codegen.h"
#include "parser.h"
#include "debug.h"

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
void FastCodeGenerator::Generate(FunctionLiteral* fun) {
  function_ = fun;
  SetFunctionPosition(fun);

  __ push(ebp);  // Caller's frame pointer.
  __ mov(ebp, esp);
  __ push(esi);  // Callee's context.
  __ push(edi);  // Callee's JS Function.

  { Comment cmnt(masm_, "[ Allocate locals");
    int locals_count = fun->scope()->num_stack_slots();
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

  Variable* arguments = fun->scope()->arguments()->AsVariable();
  if (arguments != NULL) {
    // Function uses arguments object.
    Comment cmnt(masm_, "[ Allocate arguments object");
    __ push(edi);
    // Receiver is just before the parameters on the caller's stack.
    __ lea(edx, Operand(ebp, StandardFrameConstants::kCallerSPOffset +
                                 fun->num_parameters() * kPointerSize));
    __ push(edx);
    __ push(Immediate(Smi::FromInt(fun->num_parameters())));
    // Arguments to ArgumentsAccessStub:
    //   function, receiver address, parameter count.
    // The stub will rewrite receiever and parameter count if the previous
    // stack frame was an arguments adapter frame.
    ArgumentsAccessStub stub(ArgumentsAccessStub::NEW_OBJECT);
    __ CallStub(&stub);
    __ mov(Operand(ebp, SlotOffset(arguments->slot())), eax);
    Slot* dot_arguments_slot =
        fun->scope()->arguments_shadow()->AsVariable()->slot();
    __ mov(Operand(ebp, SlotOffset(dot_arguments_slot)), eax);

    function_in_register = false;
  }

  // Possibly allocate a local context.
  if (fun->scope()->num_heap_slots() > 0) {
    Comment cmnt(masm_, "[ Allocate local context");
    if (function_in_register) {
      // Argument to NewContext is the function, still in edi.
      __ push(edi);
    } else {
      // Argument to NewContext is the function, no longer in edi.
      __ push(Operand(ebp, JavaScriptFrameConstants::kFunctionOffset));
    }
    __ CallRuntime(Runtime::kNewContext, 1);
    // Context is returned in both eax and esi.  It replaces the context
    // passed to us.  It's saved in the stack and kept live in esi.
    __ mov(Operand(ebp, StandardFrameConstants::kContextOffset), esi);
#ifdef DEBUG
    // Assert we do not have to copy any parameters into the context.
    for (int i = 0, len = fun->scope()->num_parameters(); i < len; i++) {
      Slot* slot = fun->scope()->parameter(i)->slot();
      ASSERT(slot != NULL && slot->type() != Slot::CONTEXT);
    }
#endif
  }

  { Comment cmnt(masm_, "[ Declarations");
    VisitDeclarations(fun->scope()->declarations());
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
    VisitStatements(fun->body());
    ASSERT(loop_depth() == 0);
  }

  { Comment cmnt(masm_, "[ return <undefined>;");
    // Emit a 'return undefined' in case control fell off the end of the body.
    __ mov(eax, Factory::undefined_value());
    EmitReturnSequence(function_->end_position());
  }
}


void FastCodeGenerator::EmitReturnSequence(int position) {
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
    CodeGenerator::RecordPositions(masm_, position);
    __ RecordJSReturn();
    // Do not use the leave instruction here because it is too short to
    // patch with the code required by the debugger.
    __ mov(esp, ebp);
    __ pop(ebp);
    __ ret((function_->scope()->num_parameters() + 1) * kPointerSize);
#ifdef ENABLE_DEBUGGER_SUPPORT
    // Check that the size of the code used for returning matches what is
    // expected by the debugger.
    ASSERT_EQ(Debug::kIa32JSReturnSequenceLength,
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
      __ add(Operand(esp), Immediate(kPointerSize));
      __ jmp(false_label_);
      break;
    }
    case Expression::kTestValue: {
      Label discard;
      __ push(source);
      TestAndBranch(source, &discard, false_label_);
      __ bind(&discard);
      __ add(Operand(esp), Immediate(kPointerSize));
      __ jmp(true_label_);
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
      __ push(Operand(ebp, SlotOffset(source)));
      break;
    case Expression::kTest:  // Fall through.
    case Expression::kValueTest:  // Fall through.
    case Expression::kTestValue:
      __ mov(eax, Operand(ebp, SlotOffset(source)));
      Move(context, eax);
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
      __ push(Immediate(expr->handle()));
      break;
    case Expression::kTest:  // Fall through.
    case Expression::kValueTest:  // Fall through.
    case Expression::kTestValue:
      __ mov(eax, expr->handle());
      Move(context, eax);
      break;
  }
}


void FastCodeGenerator::DropAndMove(Expression::Context context,
                                    Register source) {
  switch (context) {
    case Expression::kUninitialized:
      UNREACHABLE();
    case Expression::kEffect:
      __ add(Operand(esp), Immediate(kPointerSize));
      break;
    case Expression::kValue:
      __ mov(Operand(esp, 0), source);
      break;
    case Expression::kTest:
      ASSERT(!source.is(esp));
      __ add(Operand(esp), Immediate(kPointerSize));
      TestAndBranch(source, true_label_, false_label_);
      break;
    case Expression::kValueTest: {
      Label discard;
      __ mov(Operand(esp, 0), source);
      TestAndBranch(source, true_label_, &discard);
      __ bind(&discard);
      __ add(Operand(esp), Immediate(kPointerSize));
      __ jmp(false_label_);
      break;
    }
    case Expression::kTestValue: {
      Label discard;
      __ mov(Operand(esp, 0), source);
      TestAndBranch(source, &discard, false_label_);
      __ bind(&discard);
      __ add(Operand(esp), Immediate(kPointerSize));
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
  __ cmp(source, Factory::undefined_value());  // The undefined value is false.
  __ j(equal, false_label);
  __ cmp(source, Factory::true_value());  // True is true.
  __ j(equal, true_label);
  __ cmp(source, Factory::false_value());  // False is false.
  __ j(equal, false_label);
  ASSERT_EQ(0, kSmiTag);
  __ test(source, Operand(source));  // The smi zero is false.
  __ j(zero, false_label);
  __ test(source, Immediate(kSmiTagMask));  // All other smis are true.
  __ j(zero, true_label);

  // Call the stub for all other cases.
  __ push(source);
  ToBooleanStub stub;
  __ CallStub(&stub);
  __ test(eax, Operand(eax));  // The stub returns nonzero for true.
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
      __ push(esi);
      __ push(Immediate(var->name()));
      // Declaration nodes are always introduced in one of two modes.
      ASSERT(decl->mode() == Variable::VAR || decl->mode() == Variable::CONST);
      PropertyAttributes attr =
          (decl->mode() == Variable::VAR) ? NONE : READ_ONLY;
      __ push(Immediate(Smi::FromInt(attr)));
      // Push initial value, if any.
      // Note: For variables we must not push an initial value (such as
      // 'undefined') because we may have a (legal) redeclaration and we
      // must not destroy the current value.
      if (decl->mode() == Variable::CONST) {
        __ push(Immediate(Factory::the_hole_value()));
      } else if (decl->fun() != NULL) {
        Visit(decl->fun());
      } else {
        __ push(Immediate(Smi::FromInt(0)));  // No initial value!
      }
      __ CallRuntime(Runtime::kDeclareContextSlot, 4);
      break;
    }
    case Slot::LOCAL:
      if (decl->mode() == Variable::CONST) {
        __ mov(Operand(ebp, SlotOffset(var->slot())),
               Immediate(Factory::the_hole_value()));
      } else if (decl->fun() != NULL) {
        Visit(decl->fun());
        __ pop(Operand(ebp, SlotOffset(var->slot())));
      }
      break;
    case Slot::CONTEXT:
      // The variable in the decl always resides in the current context.
      ASSERT(function_->scope()->ContextChainLength(slot->var()->scope()) == 0);
      if (decl->mode() == Variable::CONST) {
        __ mov(eax, Immediate(Factory::the_hole_value()));
        if (FLAG_debug_code) {
          // Check if we have the correct context pointer.
          __ mov(ebx,
                 CodeGenerator::ContextOperand(esi, Context::FCONTEXT_INDEX));
          __ cmp(ebx, Operand(esi));
          __ Check(equal, "Unexpected declaration in current context.");
        }
        __ mov(CodeGenerator::ContextOperand(esi, slot->index()), eax);
        // No write barrier since the_hole_value is in old space.
      } else if (decl->fun() != NULL) {
        Visit(decl->fun());
        __ pop(eax);
        if (FLAG_debug_code) {
          // Check if we have the correct context pointer.
          __ mov(ebx,
                 CodeGenerator::ContextOperand(esi, Context::FCONTEXT_INDEX));
          __ cmp(ebx, Operand(esi));
          __ Check(equal, "Unexpected declaration in current context.");
        }
        __ mov(CodeGenerator::ContextOperand(esi, slot->index()), eax);
        int offset = FixedArray::kHeaderSize + slot->index() * kPointerSize;
        __ RecordWrite(esi, offset, eax, ecx);
      }
      break;
    default:
      UNREACHABLE();
  }
}


void FastCodeGenerator::DeclareGlobals(Handle<FixedArray> pairs) {
  // Call the runtime to declare the globals.
  __ push(esi);  // The context is the first argument.
  __ push(Immediate(pairs));
  __ push(Immediate(Smi::FromInt(is_eval_ ? 1 : 0)));
  __ CallRuntime(Runtime::kDeclareGlobals, 3);
  // Return value is ignored.
}


void FastCodeGenerator::VisitReturnStatement(ReturnStatement* stmt) {
  Comment cmnt(masm_, "[ ReturnStatement");
  Expression* expr = stmt->expression();
  if (expr->AsLiteral() != NULL) {
    __ mov(eax, expr->AsLiteral()->handle());
  } else {
    ASSERT_EQ(Expression::kValue, expr->context());
    Visit(expr);
    __ pop(eax);
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
  __ push(esi);
  __ push(Immediate(boilerplate));
  __ CallRuntime(Runtime::kNewClosure, 2);
  Move(expr->context(), eax);
}


void FastCodeGenerator::VisitVariableProxy(VariableProxy* expr) {
  Comment cmnt(masm_, "[ VariableProxy");
  Expression* rewrite = expr->var()->rewrite();
  if (rewrite == NULL) {
    ASSERT(expr->var()->is_global());
    Comment cmnt(masm_, "Global variable");
    // Use inline caching. Variable name is passed in ecx and the global
    // object on the stack.
    __ push(CodeGenerator::GlobalObject());
    __ mov(ecx, expr->name());
    Handle<Code> ic(Builtins::builtin(Builtins::LoadIC_Initialize));
    __ call(ic, RelocInfo::CODE_TARGET_CONTEXT);
    // By emitting a nop we make sure that we do not have a test eax
    // instruction after the call it is treated specially by the LoadIC code
    // Remember that the assembler may choose to do peephole optimization
    // (eg, push/pop elimination).
    __ nop();

    DropAndMove(expr->context(), eax);
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
          __ mov(eax,
                 Operand(esi, Context::SlotOffset(Context::CLOSURE_INDEX)));
          // Load the function context (which is the incoming, outer context).
          __ mov(eax, FieldOperand(eax, JSFunction::kContextOffset));
          for (int i = 1; i < chain_length; i++) {
            __ mov(eax,
                   Operand(eax, Context::SlotOffset(Context::CLOSURE_INDEX)));
            __ mov(eax, FieldOperand(eax, JSFunction::kContextOffset));
          }
          // The context may be an intermediate context, not a function context.
          __ mov(eax,
                 Operand(eax, Context::SlotOffset(Context::FCONTEXT_INDEX)));
        } else {  // Slot is in the current function context.
          // The context may be an intermediate context, not a function context.
          __ mov(eax,
                 Operand(esi, Context::SlotOffset(Context::FCONTEXT_INDEX)));
        }
        __ mov(eax, Operand(eax, Context::SlotOffset(slot->index())));
        Move(expr->context(), eax);
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
  Move(expr->context(), eax);
}


void FastCodeGenerator::VisitObjectLiteral(ObjectLiteral* expr) {
  Comment cmnt(masm_, "[ ObjectLiteral");
  Label exists;
  // Registers will be used as follows:
  // edi = JS function.
  // ebx = literals array.
  // eax = boilerplate

  __ mov(edi, Operand(ebp, JavaScriptFrameConstants::kFunctionOffset));
  __ mov(ebx, FieldOperand(edi, JSFunction::kLiteralsOffset));
  int literal_offset =
      FixedArray::kHeaderSize + expr->literal_index() * kPointerSize;
  __ mov(eax, FieldOperand(ebx, literal_offset));
  __ cmp(eax, Factory::undefined_value());
  __ j(not_equal, &exists);
  // Create boilerplate if it does not exist.
  // Literal array (0).
  __ push(ebx);
  // Literal index (1).
  __ push(Immediate(Smi::FromInt(expr->literal_index())));
  // Constant properties (2).
  __ push(Immediate(expr->constant_properties()));
  __ CallRuntime(Runtime::kCreateObjectLiteralBoilerplate, 3);
  __ bind(&exists);
  // eax contains boilerplate.
  // Clone boilerplate.
  __ push(eax);
  if (expr->depth() == 1) {
    __ CallRuntime(Runtime::kCloneShallowLiteralBoilerplate, 1);
  } else {
    __ CallRuntime(Runtime::kCloneLiteralBoilerplate, 1);
  }

  // If result_saved == true: the result is saved on top of the stack.
  // If result_saved == false: the result not on the stack, just is in eax.
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
      case ObjectLiteral::Property::MATERIALIZED_LITERAL:  // fall through
        ASSERT(!CompileTimeValue::IsCompileTimeValue(value));
      case ObjectLiteral::Property::COMPUTED:
        if (key->handle()->IsSymbol()) {
          Visit(value);
          ASSERT_EQ(Expression::kValue, value->context());
          __ pop(eax);
          __ mov(ecx, Immediate(key->handle()));
          Handle<Code> ic(Builtins::builtin(Builtins::StoreIC_Initialize));
          __ call(ic, RelocInfo::CODE_TARGET);
          // StoreIC leaves the receiver on the stack.
          break;
        }
        // fall through
      case ObjectLiteral::Property::PROTOTYPE:
        __ push(eax);
        Visit(key);
        ASSERT_EQ(Expression::kValue, key->context());
        Visit(value);
        ASSERT_EQ(Expression::kValue, value->context());
        __ CallRuntime(Runtime::kSetProperty, 3);
        __ mov(eax, Operand(esp, 0));  // Restore result into eax.
        break;
      case ObjectLiteral::Property::SETTER:  // fall through
      case ObjectLiteral::Property::GETTER:
        __ push(eax);
        Visit(key);
        ASSERT_EQ(Expression::kValue, key->context());
        __ push(Immediate(property->kind() == ObjectLiteral::Property::SETTER ?
                          Smi::FromInt(1) :
                          Smi::FromInt(0)));
        Visit(value);
        ASSERT_EQ(Expression::kValue, value->context());
        __ CallRuntime(Runtime::kDefineAccessor, 4);
        __ mov(eax, Operand(esp, 0));  // Restore result into eax.
        break;
      default: UNREACHABLE();
    }
  }
  switch (expr->context()) {
    case Expression::kUninitialized:
      UNREACHABLE();
    case Expression::kEffect:
      if (result_saved) __ add(Operand(esp), Immediate(kPointerSize));
      break;
    case Expression::kValue:
      if (!result_saved) __ push(eax);
      break;
    case Expression::kTest:
      if (result_saved) __ pop(eax);
      TestAndBranch(eax, true_label_, false_label_);
      break;
    case Expression::kValueTest: {
      Label discard;
      if (!result_saved) __ push(eax);
      TestAndBranch(eax, true_label_, &discard);
      __ bind(&discard);
      __ add(Operand(esp), Immediate(kPointerSize));
      __ jmp(false_label_);
      break;
    }
    case Expression::kTestValue: {
      Label discard;
      if (!result_saved) __ push(eax);
      TestAndBranch(eax, &discard, false_label_);
      __ bind(&discard);
      __ add(Operand(esp), Immediate(kPointerSize));
      __ jmp(true_label_);
      break;
    }
  }
}


void FastCodeGenerator::VisitArrayLiteral(ArrayLiteral* expr) {
  Comment cmnt(masm_, "[ ArrayLiteral");
  Label make_clone;

  // Fetch the function's literals array.
  __ mov(ebx, Operand(ebp, JavaScriptFrameConstants::kFunctionOffset));
  __ mov(ebx, FieldOperand(ebx, JSFunction::kLiteralsOffset));
  // Check if the literal's boilerplate has been instantiated.
  int offset =
      FixedArray::kHeaderSize + (expr->literal_index() * kPointerSize);
  __ mov(eax, FieldOperand(ebx, offset));
  __ cmp(eax, Factory::undefined_value());
  __ j(not_equal, &make_clone);

  // Instantiate the boilerplate.
  __ push(ebx);
  __ push(Immediate(Smi::FromInt(expr->literal_index())));
  __ push(Immediate(expr->literals()));
  __ CallRuntime(Runtime::kCreateArrayLiteralBoilerplate, 3);

  __ bind(&make_clone);
  // Clone the boilerplate.
  __ push(eax);
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
      __ push(eax);
      result_saved = true;
    }
    Visit(subexpr);
    ASSERT_EQ(Expression::kValue, subexpr->context());

    // Store the subexpression value in the array's elements.
    __ pop(eax);  // Subexpression value.
    __ mov(ebx, Operand(esp, 0));  // Copy of array literal.
    __ mov(ebx, FieldOperand(ebx, JSObject::kElementsOffset));
    int offset = FixedArray::kHeaderSize + (i * kPointerSize);
    __ mov(FieldOperand(ebx, offset), eax);

    // Update the write barrier for the array store.
    __ RecordWrite(ebx, offset, eax, ecx);
  }

  switch (expr->context()) {
    case Expression::kUninitialized:
      UNREACHABLE();
    case Expression::kEffect:
      if (result_saved) __ add(Operand(esp), Immediate(kPointerSize));
      break;
    case Expression::kValue:
      if (!result_saved) __ push(eax);
      break;
    case Expression::kTest:
      if (result_saved) __ pop(eax);
      TestAndBranch(eax, true_label_, false_label_);
      break;
    case Expression::kValueTest: {
      Label discard;
      if (!result_saved) __ push(eax);
      TestAndBranch(eax, true_label_, &discard);
      __ bind(&discard);
      __ add(Operand(esp), Immediate(kPointerSize));
      __ jmp(false_label_);
      break;
    }
    case Expression::kTestValue: {
      Label discard;
      if (!result_saved) __ push(eax);
      TestAndBranch(eax, &discard, false_label_);
      __ bind(&discard);
      __ add(Operand(esp), Immediate(kPointerSize));
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
    // assignment.  Right-hand-side value is passed in eax, variable name in
    // ecx, and the global object on the stack.
    __ pop(eax);
    __ mov(ecx, var->name());
    __ push(CodeGenerator::GlobalObject());
    Handle<Code> ic(Builtins::builtin(Builtins::StoreIC_Initialize));
    __ call(ic, RelocInfo::CODE_TARGET);
    // Overwrite the receiver on the stack with the result if needed.
    DropAndMove(expr->context(), eax);

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
            __ pop(Operand(ebp, SlotOffset(var->slot())));
            break;
          case Expression::kValue:
            // Perform assignment and preserve value.
            __ mov(eax, Operand(esp, 0));
            __ mov(Operand(ebp, SlotOffset(var->slot())), eax);
            break;
          case Expression::kTest:
            // Perform assignment and test (and discard) value.
            __ pop(eax);
            __ mov(Operand(ebp, SlotOffset(var->slot())), eax);
            TestAndBranch(eax, true_label_, false_label_);
            break;
          case Expression::kValueTest: {
            Label discard;
            __ mov(eax, Operand(esp, 0));
            __ mov(Operand(ebp, SlotOffset(var->slot())), eax);
            TestAndBranch(eax, true_label_, &discard);
            __ bind(&discard);
            __ add(Operand(esp), Immediate(kPointerSize));
            __ jmp(false_label_);
            break;
          }
          case Expression::kTestValue: {
            Label discard;
            __ mov(eax, Operand(esp, 0));
            __ mov(Operand(ebp, SlotOffset(var->slot())), eax);
            TestAndBranch(eax, &discard, false_label_);
            __ bind(&discard);
            __ add(Operand(esp), Immediate(kPointerSize));
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
          __ mov(eax,
                  Operand(esi, Context::SlotOffset(Context::CLOSURE_INDEX)));
          // Load the function context (which is the incoming, outer context).
          __ mov(eax, FieldOperand(eax, JSFunction::kContextOffset));
          for (int i = 1; i < chain_length; i++) {
            __ mov(eax,
                    Operand(eax, Context::SlotOffset(Context::CLOSURE_INDEX)));
            __ mov(eax, FieldOperand(eax, JSFunction::kContextOffset));
          }
        } else {  // Slot is in the current context.  Generate optimized code.
          __ mov(eax, esi);  // RecordWrite destroys the object register.
        }
        if (FLAG_debug_code) {
          __ cmp(eax,
                  Operand(eax, Context::SlotOffset(Context::FCONTEXT_INDEX)));
          __ Check(equal, "Context Slot chain length wrong.");
        }
        __ pop(ecx);
        __ mov(Operand(eax, Context::SlotOffset(slot->index())), ecx);

        // RecordWrite may destroy all its register arguments.
        if (expr->context() == Expression::kValue) {
          __ push(ecx);
        } else if (expr->context() != Expression::kEffect) {
          __ mov(edx, ecx);
        }
        int offset = FixedArray::kHeaderSize + slot->index() * kPointerSize;
        __ RecordWrite(eax, offset, ecx, ebx);
        if (expr->context() != Expression::kEffect &&
            expr->context() != Expression::kValue) {
          Move(expr->context(), edx);
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
    __ push(Operand(esp, kPointerSize));  // Receiver is under value.
    __ CallRuntime(Runtime::kToSlowProperties, 1);
  }

  __ pop(eax);
  __ mov(ecx, prop->key()->AsLiteral()->handle());
  Handle<Code> ic(Builtins::builtin(Builtins::StoreIC_Initialize));
  __ call(ic, RelocInfo::CODE_TARGET);

  // If the assignment ends an initialization block, revert to fast case.
  if (expr->ends_initialization_block()) {
    __ push(eax);  // Result of assignment, saved even if not needed.
    __ push(Operand(esp, kPointerSize));  // Receiver is under value.
    __ CallRuntime(Runtime::kToFastProperties, 1);
    __ pop(eax);
  }

  DropAndMove(expr->context(), eax);
}


void FastCodeGenerator::EmitKeyedPropertyAssignment(Assignment* expr) {
  // Assignment to a property, using a keyed store IC.

  // If the assignment starts a block of assignments to the same object,
  // change to slow case to avoid the quadratic behavior of repeatedly
  // adding fast properties.
  if (expr->starts_initialization_block()) {
    // Reciever is under the key and value.
    __ push(Operand(esp, 2 * kPointerSize));
    __ CallRuntime(Runtime::kToSlowProperties, 1);
  }

  __ pop(eax);
  Handle<Code> ic(Builtins::builtin(Builtins::KeyedStoreIC_Initialize));
  __ call(ic, RelocInfo::CODE_TARGET);
  // This nop signals to the IC that there is no inlined code at the call
  // site for it to patch.
  __ nop();

  // If the assignment ends an initialization block, revert to fast case.
  if (expr->ends_initialization_block()) {
    __ push(eax);  // Result of assignment, saved even if not needed.
    // Reciever is under the key and value.
    __ push(Operand(esp, 2 * kPointerSize));
    __ CallRuntime(Runtime::kToFastProperties, 1);
    __ pop(eax);
  }

  // Receiver and key are still on stack.
  __ add(Operand(esp), Immediate(2 * kPointerSize));
  Move(expr->context(), eax);
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
    // The IC expects the property name in ecx and the receiver on the stack.
    __ mov(ecx, Immediate(key->AsLiteral()->handle()));
    Handle<Code> ic(Builtins::builtin(Builtins::LoadIC_Initialize));
    __ call(ic, RelocInfo::CODE_TARGET);
    // By emitting a nop we make sure that we do not have a test eax
    // instruction after the call it is treated specially by the LoadIC code.
    __ nop();
  } else {
    // Do a KEYED property load.
    Visit(expr->key());
    Handle<Code> ic(Builtins::builtin(Builtins::KeyedLoadIC_Initialize));
    __ call(ic, RelocInfo::CODE_TARGET);
    // By emitting a nop we make sure that we do not have a "test eax,..."
    // instruction after the call it is treated specially by the LoadIC code.
    __ nop();
    // Drop key left on the stack by IC.
    __ add(Operand(esp), Immediate(kPointerSize));
  }
  DropAndMove(expr->context(), eax);
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
  __ mov(esi, Operand(ebp, StandardFrameConstants::kContextOffset));
  // Discard the function left on TOS.
  DropAndMove(expr->context(), eax);
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
  __ mov(esi, Operand(ebp, StandardFrameConstants::kContextOffset));
  // Discard the function left on TOS.
  DropAndMove(expr->context(), eax);
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
    __ push(Immediate(var->name()));
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
      __ push(Immediate(key->handle()));
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
      __ add(Operand(esp), Immediate(kPointerSize));
      // Pop receiver.
      __ pop(ebx);
      // Push result (function).
      __ push(eax);
      // Push receiver object on stack.
      if (prop->is_synthetic()) {
        __ push(CodeGenerator::GlobalObject());
      } else {
        __ push(ebx);
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
    __ mov(ebx, CodeGenerator::GlobalObject());
    __ push(FieldOperand(ebx, GlobalObject::kGlobalReceiverOffset));
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

  // Load function, arg_count into edi and eax.
  __ Set(eax, Immediate(arg_count));
  // Function is in esp[arg_count + 1].
  __ mov(edi, Operand(esp, eax, times_pointer_size, kPointerSize));

  Handle<Code> construct_builtin(Builtins::builtin(Builtins::JSConstructCall));
  __ call(construct_builtin, RelocInfo::CONSTRUCT_CALL);

  // Replace function on TOS with result in eax, or pop it.
  DropAndMove(expr->context(), eax);
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
  Move(expr->context(), eax);
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
          __ push(Immediate(Factory::undefined_value()));
          break;
        case Expression::kTestValue:
          // Value is false so it's needed.
          __ push(Immediate(Factory::undefined_value()));
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
          __ push(Immediate(Factory::true_value()));
          __ jmp(&done);
          __ bind(&push_false);
          __ push(Immediate(Factory::false_value()));
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
          __ push(Immediate(Factory::true_value()));
          __ jmp(saved_true);
          break;

        case Expression::kTestValue:
          true_label_ = &push_false;
          false_label_ = saved_true;
          Visit(expr->expression());
          __ bind(&push_false);
          __ push(Immediate(Factory::false_value()));
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
        __ mov(ecx, Immediate(proxy->name()));
        Handle<Code> ic(Builtins::builtin(Builtins::LoadIC_Initialize));
        // Use a regular load, not a contextual load, to avoid a reference
        // error.
        __ call(ic, RelocInfo::CODE_TARGET);
        __ mov(Operand(esp, 0), eax);
      } else if (proxy != NULL &&
                 proxy->var()->slot() != NULL &&
                 proxy->var()->slot()->type() == Slot::LOOKUP) {
        __ push(esi);
        __ push(Immediate(proxy->name()));
        __ CallRuntime(Runtime::kLoadContextSlotNoReferenceError, 2);
        __ push(eax);
      } else {
        // This expression cannot throw a reference error at the top level.
        Visit(expr->expression());
      }

      __ CallRuntime(Runtime::kTypeof, 1);
      Move(expr->context(), eax);
      break;
    }

    default:
      UNREACHABLE();
  }
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
      __ push(eax);
      break;
    case Expression::kEffect:
      // Do not save result.
      break;
  }
  // Call runtime for +1/-1.
  __ push(eax);
  __ push(Immediate(Smi::FromInt(1)));
  if (expr->op() == Token::INC) {
    __ CallRuntime(Runtime::kNumberAdd, 2);
  } else {
    __ CallRuntime(Runtime::kNumberSub, 2);
  }
  // Call Store IC.
  __ mov(ecx, proxy->AsVariable()->name());
  __ push(CodeGenerator::GlobalObject());
  Handle<Code> ic(Builtins::builtin(Builtins::StoreIC_Initialize));
  __ call(ic, RelocInfo::CODE_TARGET);
  // Restore up stack after store IC.
  __ add(Operand(esp), Immediate(kPointerSize));

  switch (expr->context()) {
    case Expression::kUninitialized:
      UNREACHABLE();
    case Expression::kEffect:  // Fall through
    case Expression::kValue:
      // Do nothing. Result in either on the stack for value context
      // or discarded for effect context.
      break;
    case Expression::kTest:
      __ pop(eax);
      TestAndBranch(eax, true_label_, false_label_);
      break;
    case Expression::kValueTest: {
      Label discard;
      __ mov(eax, Operand(esp, 0));
      TestAndBranch(eax, true_label_, &discard);
      __ bind(&discard);
      __ add(Operand(esp), Immediate(kPointerSize));
      __ jmp(false_label_);
      break;
    }
    case Expression::kTestValue: {
      Label discard;
      __ mov(eax, Operand(esp, 0));
      TestAndBranch(eax, &discard, false_label_);
      __ bind(&discard);
      __ add(Operand(esp), Immediate(kPointerSize));
      __ jmp(true_label_);
      break;
    }
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
      Move(expr->context(), eax);

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
      __ cmp(eax, Factory::true_value());
      __ j(equal, true_label_);
      __ jmp(false_label_);
      break;
    }

    case Token::INSTANCEOF: {
      InstanceofStub stub;
      __ CallStub(&stub);
      __ test(eax, Operand(eax));
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
          __ pop(eax);
          __ pop(edx);
          break;
        case Token::LT:
          cc = less;
          __ pop(eax);
          __ pop(edx);
          break;
        case Token::GT:
          // Reverse left and right sizes to obtain ECMA-262 conversion order.
          cc = less;
          __ pop(edx);
          __ pop(eax);
         break;
        case Token::LTE:
          // Reverse left and right sizes to obtain ECMA-262 conversion order.
          cc = greater_equal;
          __ pop(edx);
          __ pop(eax);
          break;
        case Token::GTE:
          cc = greater_equal;
          __ pop(eax);
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
      __ j(cc, true_label_);
      __ jmp(false_label_);

      __ bind(&slow_case);
      CompareStub stub(cc, strict);
      __ CallStub(&stub);
      __ test(eax, Operand(eax));
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
      __ push(Immediate(Factory::true_value()));
      __ jmp(&done);
      __ bind(&push_false);
      __ push(Immediate(Factory::false_value()));
      __ bind(&done);
      break;

    case Expression::kEffect:
      __ bind(&done);
      break;

    case Expression::kTest:
      break;

    case Expression::kValueTest:
      __ bind(&push_true);
      __ push(Immediate(Factory::true_value()));
      __ jmp(saved_true);
      break;

    case Expression::kTestValue:
      __ bind(&push_false);
      __ push(Immediate(Factory::false_value()));
      __ jmp(saved_false);
      break;
  }
  true_label_ = saved_true;
  false_label_ = saved_false;
  // Convert current context to test context: End post-test code.
}


#undef __


} }  // namespace v8::internal
