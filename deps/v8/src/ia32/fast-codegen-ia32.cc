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
    for (int i = 0; i < locals_count; i++) {
      __ push(Immediate(Factory::undefined_value()));
    }
  }

  { Comment cmnt(masm_, "[ Stack check");
    Label ok;
    ExternalReference stack_guard_limit =
        ExternalReference::address_of_stack_guard_limit();
    __ cmp(esp, Operand::StaticVariable(stack_guard_limit));
    __ j(above_equal, &ok, taken);
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
    VisitStatements(fun->body());
  }

  { Comment cmnt(masm_, "[ return <undefined>;");
    // Emit a 'return undefined' in case control fell off the end of the
    // body.
    __ mov(eax, Factory::undefined_value());
    SetReturnPosition(fun);

    if (FLAG_trace) {
      __ push(eax);
      __ CallRuntime(Runtime::kTraceExit, 1);
    }
    __ RecordJSReturn();
    // Do not use the leave instruction here because it is too short to
    // patch with the code required by the debugger.
    __ mov(esp, ebp);
    __ pop(ebp);
    __ ret((fun->scope()->num_parameters() + 1) * kPointerSize);
  }
}


void FastCodeGenerator::Move(Location destination, Slot* source) {
  switch (destination.type()) {
    case Location::NOWHERE:
      break;
    case Location::TEMP:
      __ push(Operand(ebp, SlotOffset(source)));
      break;
  }
}


void FastCodeGenerator::Move(Location destination, Literal* expr) {
  switch (destination.type()) {
    case Location::NOWHERE:
      break;
    case Location::TEMP:
      __ push(Immediate(expr->handle()));
      break;
  }
}


void FastCodeGenerator::Move(Slot* destination, Location source) {
  switch (source.type()) {
    case Location::NOWHERE:
      UNREACHABLE();
    case Location::TEMP:
      __ pop(Operand(ebp, SlotOffset(destination)));
      break;
  }
}


void FastCodeGenerator::DropAndMove(Location destination, Register source) {
  switch (destination.type()) {
    case Location::NOWHERE:
      __ add(Operand(esp), Immediate(kPointerSize));
      break;
    case Location::TEMP:
      __ mov(Operand(esp, 0), source);
      break;
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
  SetStatementPosition(stmt);
  Expression* expr = stmt->expression();
  // Complete the statement based on the type of the subexpression.
  if (expr->AsLiteral() != NULL) {
    __ mov(eax, expr->AsLiteral()->handle());
  } else {
    Visit(expr);
    Move(eax, expr->location());
  }

  if (FLAG_trace) {
    __ push(eax);
    __ CallRuntime(Runtime::kTraceExit, 1);
  }
  __ RecordJSReturn();

  // Do not use the leave instruction here because it is too short to
  // patch with the code required by the debugger.
  __ mov(esp, ebp);
  __ pop(ebp);
  __ ret((function_->scope()->num_parameters() + 1) * kPointerSize);
}


void FastCodeGenerator::VisitFunctionLiteral(FunctionLiteral* expr) {
  Comment cmnt(masm_, "[ FunctionLiteral");

  // Build the function boilerplate and instantiate it.
  Handle<JSFunction> boilerplate = BuildBoilerplate(expr);
  if (HasStackOverflow()) return;

  ASSERT(boilerplate->IsBoilerplate());

  // Create a new closure.
  __ push(esi);
  __ push(Immediate(boilerplate));
  __ CallRuntime(Runtime::kNewClosure, 2);
  Move(expr->location(), eax);
}


void FastCodeGenerator::VisitVariableProxy(VariableProxy* expr) {
  Comment cmnt(masm_, "[ VariableProxy");
  Expression* rewrite = expr->var()->rewrite();
  if (rewrite == NULL) {
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

    DropAndMove(expr->location(), eax);
  } else {
    Comment cmnt(masm_, "Stack slot");
    Move(expr->location(), rewrite->AsSlot());
  }
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
          Move(eax, value->location());
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
        ASSERT(key->location().is_temporary());
        Visit(value);
        ASSERT(value->location().is_temporary());
        __ CallRuntime(Runtime::kSetProperty, 3);
        __ mov(eax, Operand(esp, 0));  // Restore result into eax.
        break;
      case ObjectLiteral::Property::SETTER:  // fall through
      case ObjectLiteral::Property::GETTER:
        __ push(eax);
        Visit(key);
        ASSERT(key->location().is_temporary());
        __ push(Immediate(property->kind() == ObjectLiteral::Property::SETTER ?
                          Smi::FromInt(1) :
                          Smi::FromInt(0)));
        Visit(value);
        ASSERT(value->location().is_temporary());
        __ CallRuntime(Runtime::kDefineAccessor, 4);
        __ mov(eax, Operand(esp, 0));  // Restore result into eax.
        break;
      default: UNREACHABLE();
    }
  }
  switch (expr->location().type()) {
    case Location::NOWHERE:
      if (result_saved) __ add(Operand(esp), Immediate(kPointerSize));
      break;
    case Location::TEMP:
      if (!result_saved) __ push(eax);
      break;
  }
}


void FastCodeGenerator::VisitRegExpLiteral(RegExpLiteral* expr) {
  Comment cmnt(masm_, "[ RegExp Literal");
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
  Move(expr->location(), eax);
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
    ASSERT(subexpr->location().is_temporary());

    // Store the subexpression value in the array's elements.
    __ pop(eax);  // Subexpression value.
    __ mov(ebx, Operand(esp, 0));  // Copy of array literal.
    __ mov(ebx, FieldOperand(ebx, JSObject::kElementsOffset));
    int offset = FixedArray::kHeaderSize + (i * kPointerSize);
    __ mov(FieldOperand(ebx, offset), eax);

    // Update the write barrier for the array store.
    __ RecordWrite(ebx, offset, eax, ecx);
  }

  switch (expr->location().type()) {
    case Location::NOWHERE:
      if (result_saved) __ add(Operand(esp), Immediate(kPointerSize));
      break;
    case Location::TEMP:
      if (!result_saved) __ push(eax);
      break;
  }
}


void FastCodeGenerator::VisitAssignment(Assignment* expr) {
  Comment cmnt(masm_, "[ Assignment");
  ASSERT(expr->op() == Token::ASSIGN || expr->op() == Token::INIT_VAR);

  // Left-hand side can only be a global or a (parameter or local) slot.
  Variable* var = expr->target()->AsVariableProxy()->AsVariable();
  ASSERT(var != NULL);
  ASSERT(var->is_global() || var->slot() != NULL);

  Expression* rhs = expr->value();
  if (var->is_global()) {
    // Assignment to a global variable, use inline caching.  Right-hand-side
    // value is passed in eax, variable name in ecx, and the global object
    // on the stack.

    // Code for the right-hand-side expression depends on its type.
    if (rhs->AsLiteral() != NULL) {
      __ mov(eax, rhs->AsLiteral()->handle());
    } else {
      ASSERT(rhs->location().is_temporary());
      Visit(rhs);
      __ pop(eax);
    }
    __ mov(ecx, var->name());
    __ push(CodeGenerator::GlobalObject());
    Handle<Code> ic(Builtins::builtin(Builtins::StoreIC_Initialize));
    __ call(ic, RelocInfo::CODE_TARGET);
    // Overwrite the global object on the stack with the result if needed.
    DropAndMove(expr->location(), eax);
  } else {
    // Local or parameter assignment.

    // Code for the right-hand side expression depends on its type.
    if (rhs->AsLiteral() != NULL) {
      // Two cases: 'temp <- (var = constant)', or 'var = constant' with a
      // discarded result.  Always perform the assignment.
      __ mov(eax, rhs->AsLiteral()->handle());
      __ mov(Operand(ebp, SlotOffset(var->slot())), eax);
      Move(expr->location(), eax);
    } else {
      ASSERT(rhs->location().is_temporary());
      Visit(rhs);
      switch (expr->location().type()) {
        case Location::NOWHERE:
          // Case 'var = temp'.  Discard right-hand-side temporary.
          Move(var->slot(), rhs->location());
          break;
        case Location::TEMP:
          // Case 'temp1 <- (var = temp0)'.  Preserve right-hand-side
          // temporary on the stack.
          __ mov(eax, Operand(esp, 0));
          __ mov(Operand(ebp, SlotOffset(var->slot())), eax);
          break;
      }
    }
  }
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
  switch (expr->location().type()) {
    case Location::TEMP:
      __ mov(Operand(esp, 0), eax);
      break;
    case Location::NOWHERE:
      __ add(Operand(esp), Immediate(kPointerSize));
      break;
  }
}


void FastCodeGenerator::VisitCall(Call* expr) {
  Expression* fun = expr->expression();
  ZoneList<Expression*>* args = expr->arguments();
  Variable* var = fun->AsVariableProxy()->AsVariable();
  ASSERT(var != NULL && !var->is_this() && var->is_global());
  ASSERT(!var->is_possibly_eval());

  __ push(Immediate(var->name()));
  // Push global object (receiver).
  __ push(CodeGenerator::GlobalObject());
  int arg_count = args->length();
  for (int i = 0; i < arg_count; i++) {
    Visit(args->at(i));
    ASSERT(args->at(i)->location().is_temporary());
  }
  // Record source position for debugger
  SetSourcePosition(expr->position());
  // Call the IC initialization code.
  Handle<Code> ic = CodeGenerator::ComputeCallInitialize(arg_count,
                                                         NOT_IN_LOOP);
  __ call(ic, RelocInfo::CODE_TARGET_CONTEXT);
  // Restore context register.
  __ mov(esi, Operand(ebp, StandardFrameConstants::kContextOffset));
  // Discard the function left on TOS.
  DropAndMove(expr->location(), eax);
}


void FastCodeGenerator::VisitCallNew(CallNew* node) {
  Comment cmnt(masm_, "[ CallNew");
  // According to ECMA-262, section 11.2.2, page 44, the function
  // expression in new calls must be evaluated before the
  // arguments.
  // Push function on the stack.
  Visit(node->expression());
  ASSERT(node->expression()->location().is_temporary());

  // Push global object (receiver).
  __ push(CodeGenerator::GlobalObject());

  // Push the arguments ("left-to-right") on the stack.
  ZoneList<Expression*>* args = node->arguments();
  int arg_count = args->length();
  for (int i = 0; i < arg_count; i++) {
    Visit(args->at(i));
    ASSERT(args->at(i)->location().is_temporary());
    // If location is temporary, it is already on the stack,
    // so nothing to do here.
  }

  // Call the construct call builtin that handles allocation and
  // constructor invocation.
  SetSourcePosition(node->position());

  // Load function, arg_count into edi and eax.
  __ Set(eax, Immediate(arg_count));
  // Function is in esp[arg_count + 1].
  __ mov(edi, Operand(esp, eax, times_pointer_size, kPointerSize));

  Handle<Code> construct_builtin(Builtins::builtin(Builtins::JSConstructCall));
  __ call(construct_builtin, RelocInfo::CONSTRUCT_CALL);

  // Replace function on TOS with result in eax, or pop it.
  DropAndMove(node->location(), eax);
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
    ASSERT(args->at(i)->location().is_temporary());
  }

  __ CallRuntime(function, arg_count);
  Move(expr->location(), eax);
}


void FastCodeGenerator::VisitBinaryOperation(BinaryOperation* expr) {
  // Compile a short-circuited boolean or operation in a non-test
  // context.
  ASSERT(expr->op() == Token::OR);
  // Compile (e0 || e1) as if it were
  // (let (temp = e0) temp ? temp : e1).

  Label eval_right, done;
  Location destination = expr->location();
  Expression* left = expr->left();
  Expression* right = expr->right();

  // Use the shared ToBoolean stub to find the boolean value of the
  // left-hand subexpression.  Load the value into eax to perform some
  // inlined checks assumed by the stub.

  // Compile the left-hand value into eax.  Put it on the stack if we may
  // need it as the value of the whole expression.
  if (left->AsLiteral() != NULL) {
    __ mov(eax, left->AsLiteral()->handle());
    if (destination.is_temporary()) __ push(eax);
  } else {
    Visit(left);
    ASSERT(left->location().is_temporary());
    switch (destination.type()) {
      case Location::NOWHERE:
        // Pop the left-hand value into eax because we will not need it as the
        // final result.
        __ pop(eax);
        break;
      case Location::TEMP:
        // Copy the left-hand value into eax because we may need it as the
        // final result.
        __ mov(eax, Operand(esp, 0));
        break;
    }
  }
  // The left-hand value is in eax.  It is also on the stack iff the
  // destination location is temporary.

  // Perform fast checks assumed by the stub.
  __ cmp(eax, Factory::undefined_value());  // The undefined value is false.
  __ j(equal, &eval_right);
  __ cmp(eax, Factory::true_value());  // True is true.
  __ j(equal, &done);
  __ cmp(eax, Factory::false_value());  // False is false.
  __ j(equal, &eval_right);
  ASSERT(kSmiTag == 0);
  __ test(eax, Operand(eax));  // The smi zero is false.
  __ j(zero, &eval_right);
  __ test(eax, Immediate(kSmiTagMask));  // All other smis are true.
  __ j(zero, &done);

  // Call the stub for all other cases.
  __ push(eax);
  ToBooleanStub stub;
  __ CallStub(&stub);
  __ test(eax, Operand(eax));  // The stub returns nonzero for true.
  __ j(not_zero, &done);

  __ bind(&eval_right);
  // Discard the left-hand value if present on the stack.
  if (destination.is_temporary()) {
    __ add(Operand(esp), Immediate(kPointerSize));
  }
  // Save or discard the right-hand value as needed.
  if (right->AsLiteral() != NULL) {
    Move(destination, right->AsLiteral());
  } else {
    Visit(right);
    Move(destination, right->location());
  }

  __ bind(&done);
}


} }  // namespace v8::internal
