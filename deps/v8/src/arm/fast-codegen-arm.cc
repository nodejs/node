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
void FastCodeGenerator::Generate(FunctionLiteral* fun) {
  function_ = fun;
  // ARM does NOT call SetFunctionPosition.

  __ stm(db_w, sp, r1.bit() | cp.bit() | fp.bit() | lr.bit());
  // Adjust fp to point to caller's fp.
  __ add(fp, sp, Operand(2 * kPointerSize));

  { Comment cmnt(masm_, "[ Allocate locals");
    int locals_count = fun->scope()->num_stack_slots();
    if (locals_count > 0) {
      __ LoadRoot(ip, Heap::kUndefinedValueRootIndex);
    }
    __ LoadRoot(r2, Heap::kStackLimitRootIndex);
    for (int i = 0; i < locals_count; i++) {
      __ push(ip);
    }
  }

  // Check the stack for overflow or break request.
  // Put the lr setup instruction in the delay slot.  The kInstrSize is
  // added to the implicit 8 byte offset that always applies to operations
  // with pc and gives a return address 12 bytes down.
  Comment cmnt(masm_, "[ Stack check");
  __ add(lr, pc, Operand(Assembler::kInstrSize));
  __ cmp(sp, Operand(r2));
  StackCheckStub stub;
  __ mov(pc,
         Operand(reinterpret_cast<intptr_t>(stub.GetCode().location()),
                 RelocInfo::CODE_TARGET),
         LeaveCC,
         lo);

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
    __ LoadRoot(r0, Heap::kUndefinedValueRootIndex);
    SetReturnPosition(fun);
    if (FLAG_trace) {
      // Push the return value on the stack as the parameter.
      // Runtime::TraceExit returns its parameter in r0.
      __ push(r0);
      __ CallRuntime(Runtime::kTraceExit, 1);
    }

    __ RecordJSReturn();
    __ mov(sp, fp);
    __ ldm(ia_w, sp, fp.bit() | lr.bit());
    int num_parameters = function_->scope()->num_parameters();
    __ add(sp, sp, Operand((num_parameters + 1) * kPointerSize));
    __ Jump(lr);
  }
}


void FastCodeGenerator::Move(Location destination, Slot* source) {
  switch (destination.type()) {
    case Location::NOWHERE:
      break;
    case Location::TEMP:
      __ ldr(ip, MemOperand(fp, SlotOffset(source)));
      __ push(ip);
      break;
  }
}


void FastCodeGenerator::Move(Location destination, Literal* expr) {
  switch (destination.type()) {
    case Location::NOWHERE:
      break;
    case Location::TEMP:
      __ mov(ip, Operand(expr->handle()));
      __ push(ip);
      break;
  }
}


void FastCodeGenerator::Move(Slot* destination, Location source) {
  switch (source.type()) {
    case Location::NOWHERE:
      UNREACHABLE();
    case Location::TEMP:
      __ pop(ip);
      __ str(ip, MemOperand(fp, SlotOffset(destination)));
      break;
  }
}


void FastCodeGenerator::DropAndMove(Location destination, Register source) {
  switch (destination.type()) {
    case Location::NOWHERE:
      __ pop();
      break;
    case Location::TEMP:
      __ str(source, MemOperand(sp));
      break;
  }
}


void FastCodeGenerator::DeclareGlobals(Handle<FixedArray> pairs) {
  // Call the runtime to declare the globals.
  // The context is the first argument.
  __ mov(r1, Operand(pairs));
  __ mov(r0, Operand(Smi::FromInt(is_eval_ ? 1 : 0)));
  __ stm(db_w, sp, cp.bit() | r1.bit() | r0.bit());
  __ CallRuntime(Runtime::kDeclareGlobals, 3);
  // Return value is ignored.
}


void FastCodeGenerator::VisitReturnStatement(ReturnStatement* stmt) {
  Comment cmnt(masm_, "[ ReturnStatement");
  SetStatementPosition(stmt);
  Expression* expr = stmt->expression();
  // Complete the statement based on the type of the subexpression.
  if (expr->AsLiteral() != NULL) {
    __ mov(r0, Operand(expr->AsLiteral()->handle()));
  } else {
    Visit(expr);
    Move(r0, expr->location());
  }

  if (FLAG_trace) {
    __ push(r0);
    __ CallRuntime(Runtime::kTraceExit, 1);
  }

  __ RecordJSReturn();
  __ mov(sp, fp);
  __ ldm(ia_w, sp, fp.bit() | lr.bit());
  int num_parameters = function_->scope()->num_parameters();
  __ add(sp, sp, Operand((num_parameters + 1) * kPointerSize));
  __ Jump(lr);
}


void FastCodeGenerator::VisitFunctionLiteral(FunctionLiteral* expr) {
  Comment cmnt(masm_, "[ FunctionLiteral");

  // Build the function boilerplate and instantiate it.
  Handle<JSFunction> boilerplate = BuildBoilerplate(expr);
  if (HasStackOverflow()) return;

  ASSERT(boilerplate->IsBoilerplate());

  // Create a new closure.
  __ mov(r0, Operand(boilerplate));
  __ stm(db_w, sp, cp.bit() | r0.bit());
  __ CallRuntime(Runtime::kNewClosure, 2);
  Move(expr->location(), r0);
}


void FastCodeGenerator::VisitVariableProxy(VariableProxy* expr) {
  Comment cmnt(masm_, "[ VariableProxy");
  Expression* rewrite = expr->var()->rewrite();
  if (rewrite == NULL) {
    Comment cmnt(masm_, "Global variable");
    // Use inline caching. Variable name is passed in r2 and the global
    // object on the stack.
    __ ldr(ip, CodeGenerator::GlobalObject());
    __ push(ip);
    __ mov(r2, Operand(expr->name()));
    Handle<Code> ic(Builtins::builtin(Builtins::LoadIC_Initialize));
    __ Call(ic, RelocInfo::CODE_TARGET_CONTEXT);
    DropAndMove(expr->location(), r0);
  } else {
    Comment cmnt(masm_, "Stack slot");
    Move(expr->location(), rewrite->AsSlot());
  }
}


void FastCodeGenerator::VisitObjectLiteral(ObjectLiteral* expr) {
  Comment cmnt(masm_, "[ ObjectLiteral");
  Label boilerplate_exists;
  __ ldr(r2, MemOperand(fp,  JavaScriptFrameConstants::kFunctionOffset));
  // r2 = literal array (0).
  __ ldr(r2, FieldMemOperand(r2, JSFunction::kLiteralsOffset));
  int literal_offset =
      FixedArray::kHeaderSize + expr->literal_index() * kPointerSize;
  __ ldr(r0, FieldMemOperand(r2, literal_offset));
  // Check whether we need to materialize the object literal boilerplate.
  __ LoadRoot(ip, Heap::kUndefinedValueRootIndex);
  __ cmp(r0, Operand(ip));
  __ b(ne, &boilerplate_exists);
  // Create boilerplate if it does not exist.
  // r1 = literal index (1).
  __ mov(r1, Operand(Smi::FromInt(expr->literal_index())));
  // r0 = constant properties (2).
  __ mov(r0, Operand(expr->constant_properties()));
  __ stm(db_w, sp, r2.bit() | r1.bit() | r0.bit());
  __ CallRuntime(Runtime::kCreateObjectLiteralBoilerplate, 3);
  __ bind(&boilerplate_exists);
  // r0 contains boilerplate.
  // Clone boilerplate.
  __ push(r0);
  if (expr->depth() > 1) {
    __ CallRuntime(Runtime::kCloneLiteralBoilerplate, 1);
  } else {
    __ CallRuntime(Runtime::kCloneShallowLiteralBoilerplate, 1);
  }

  // If result_saved == true: the result is saved on top of the stack.
  // If result_saved == false: the result is in eax.
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
      case ObjectLiteral::Property::MATERIALIZED_LITERAL:   // fall through
        ASSERT(!CompileTimeValue::IsCompileTimeValue(property->value()));
      case ObjectLiteral::Property::COMPUTED:  // fall through
      case ObjectLiteral::Property::PROTOTYPE:
        __ push(r0);
        Visit(key);
        ASSERT(key->location().is_temporary());
        Visit(value);
        ASSERT(value->location().is_temporary());
        __ CallRuntime(Runtime::kSetProperty, 3);
        __ ldr(r0, MemOperand(sp));  // Restore result into r0
        break;
      case ObjectLiteral::Property::SETTER:  // fall through
      case ObjectLiteral::Property::GETTER:
        __ push(r0);
        Visit(key);
        ASSERT(key->location().is_temporary());
        __ mov(r1, Operand(property->kind() == ObjectLiteral::Property::SETTER ?
                           Smi::FromInt(1) :
                           Smi::FromInt(0)));
        __ push(r1);
        Visit(value);
        ASSERT(value->location().is_temporary());
        __ CallRuntime(Runtime::kDefineAccessor, 4);
        __ ldr(r0, MemOperand(sp));  // Restore result into r0
        break;
      default: UNREACHABLE();
    }
  }
  switch (expr->location().type()) {
    case Location::NOWHERE:
      if (result_saved) __ pop();
      break;
    case Location::TEMP:
      if (!result_saved) __ push(r0);
      break;
  }
}


void FastCodeGenerator::VisitRegExpLiteral(RegExpLiteral* expr) {
  Comment cmnt(masm_, "[ RegExp Literal");
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
  __ stm(db_w, sp, r4.bit() | r3.bit() | r2.bit() | r1.bit());
  __ CallRuntime(Runtime::kMaterializeRegExpLiteral, 4);
  __ bind(&done);
  Move(expr->location(), r0);
}


void FastCodeGenerator::VisitArrayLiteral(ArrayLiteral* expr) {
  Comment cmnt(masm_, "[ ArrayLiteral");
  Label make_clone;

  // Fetch the function's literals array.
  __ ldr(r3, MemOperand(fp, JavaScriptFrameConstants::kFunctionOffset));
  __ ldr(r3, FieldMemOperand(r3, JSFunction::kLiteralsOffset));
  // Check if the literal's boilerplate has been instantiated.
  int offset =
      FixedArray::kHeaderSize + (expr->literal_index() * kPointerSize);
  __ ldr(r0, FieldMemOperand(r3, offset));
  __ LoadRoot(ip, Heap::kUndefinedValueRootIndex);
  __ cmp(r0, ip);
  __ b(&make_clone, ne);

  // Instantiate the boilerplate.
  __ mov(r2, Operand(Smi::FromInt(expr->literal_index())));
  __ mov(r1, Operand(expr->literals()));
  __ stm(db_w, sp, r3.bit() | r2.bit() | r1.bit());
  __ CallRuntime(Runtime::kCreateArrayLiteralBoilerplate, 3);

  __ bind(&make_clone);
  // Clone the boilerplate.
  __ push(r0);
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
      __ push(r0);
      result_saved = true;
    }
    Visit(subexpr);
    ASSERT(subexpr->location().is_temporary());

    // Store the subexpression value in the array's elements.
    __ pop(r0);  // Subexpression value.
    __ ldr(r1, MemOperand(sp));  // Copy of array literal.
    __ ldr(r1, FieldMemOperand(r1, JSObject::kElementsOffset));
    int offset = FixedArray::kHeaderSize + (i * kPointerSize);
    __ str(r0, FieldMemOperand(r1, offset));

    // Update the write barrier for the array store with r0 as the scratch
    // register.
    __ mov(r2, Operand(offset));
    __ RecordWrite(r1, r2, r0);
  }

  switch (expr->location().type()) {
    case Location::NOWHERE:
      if (result_saved) __ pop();
      break;
    case Location::TEMP:
      if (!result_saved) __ push(r0);
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
  Location destination = expr->location();
  if (var->is_global()) {
    // Assignment to a global variable, use inline caching.  Right-hand-side
    // value is passed in r0, variable name in r2, and the global object on
    // the stack.

    // Code for the right-hand-side expression depends on its type.
    if (rhs->AsLiteral() != NULL) {
      __ mov(r0, Operand(rhs->AsLiteral()->handle()));
    } else {
      ASSERT(rhs->location().is_temporary());
      Visit(rhs);
      __ pop(r0);
    }
    __ mov(r2, Operand(var->name()));
    __ ldr(ip, CodeGenerator::GlobalObject());
    __ push(ip);
    Handle<Code> ic(Builtins::builtin(Builtins::StoreIC_Initialize));
    __ Call(ic, RelocInfo::CODE_TARGET);
    // Overwrite the global object on the stack with the result if needed.
    DropAndMove(expr->location(), r0);
  } else {
    // Local or parameter assignment.

    // Code for the right-hand side expression depends on its type.
    if (rhs->AsLiteral() != NULL) {
      // Two cases: 'temp <- (var = constant)', or 'var = constant' with a
      // discarded result.  Always perform the assignment.
      __ mov(ip, Operand(rhs->AsLiteral()->handle()));
      __ str(ip, MemOperand(fp, SlotOffset(var->slot())));
      Move(expr->location(), ip);
    } else {
      ASSERT(rhs->location().is_temporary());
      Visit(rhs);
      // Load right-hand side into ip.
      switch (expr->location().type()) {
        case Location::NOWHERE:
          // Case 'var = temp'.  Discard right-hand-side temporary.
          __ pop(ip);
          break;
        case Location::TEMP:
          // Case 'temp1 <- (var = temp0)'.  Preserve right-hand-side
          // temporary on the stack.
          __ ldr(ip, MemOperand(sp));
          break;
      }
      // Do the slot assignment.
      __ str(ip, MemOperand(fp, SlotOffset(var->slot())));
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
    __ mov(r2, Operand(key->AsLiteral()->handle()));
    Handle<Code> ic(Builtins::builtin(Builtins::LoadIC_Initialize));
    __ Call(ic, RelocInfo::CODE_TARGET);
    // By emitting a nop we make sure that we do not have a "test eax,..."
    // instruction after the call it is treated specially by the LoadIC code.
    __ nop();
  } else {
    // Do a KEYED property load.
    Visit(expr->key());
    Handle<Code> ic(Builtins::builtin(Builtins::KeyedLoadIC_Initialize));
    __ Call(ic, RelocInfo::CODE_TARGET);
    // By emitting a nop we make sure that we do not have a "test eax,..."
    // instruction after the call it is treated specially by the LoadIC code.
    __ nop();
    // Drop key and receiver left on the stack by IC.
    __ pop();
  }
  switch (expr->location().type()) {
    case Location::TEMP:
      __ str(r0, MemOperand(sp));
      break;
    case Location::NOWHERE:
      __ pop();
  }
}


void FastCodeGenerator::VisitCall(Call* expr) {
  Comment cmnt(masm_, "[ Call");
  Expression* fun = expr->expression();
  ZoneList<Expression*>* args = expr->arguments();
  Variable* var = fun->AsVariableProxy()->AsVariable();
  ASSERT(var != NULL && !var->is_this() && var->is_global());
  ASSERT(!var->is_possibly_eval());

  __ mov(r1, Operand(var->name()));
  // Push global object as receiver.
  __ ldr(r0, CodeGenerator::GlobalObject());
  __ stm(db_w, sp, r1.bit() | r0.bit());
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
  __ Call(ic, RelocInfo::CODE_TARGET_CONTEXT);
  // Restore context register.
  __ ldr(cp, MemOperand(fp, StandardFrameConstants::kContextOffset));
  DropAndMove(expr->location(), r0);
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
  __ ldr(r0, CodeGenerator::GlobalObject());
  __ push(r0);
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

  // Load function, arg_count into r1 and r0.
  __ mov(r0, Operand(arg_count));
  // Function is in esp[arg_count + 1].
  __ ldr(r1, MemOperand(sp, (arg_count + 1) * kPointerSize));

  Handle<Code> construct_builtin(Builtins::builtin(Builtins::JSConstructCall));
  __ Call(construct_builtin, RelocInfo::CONSTRUCT_CALL);

  // Replace function on TOS with result in r0, or pop it.
  DropAndMove(node->location(), r0);
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
  Move(expr->location(), r0);
}


void FastCodeGenerator::VisitBinaryOperation(BinaryOperation* expr) {
  // Compile a short-circuited boolean or operation in a non-test
  // context.
  ASSERT(expr->op() == Token::OR);
  // Compile (e0 || e1) as if it were
  // (let (temp = e0) temp ? temp : e1).

  Label done;
  Location destination = expr->location();
  Expression* left = expr->left();
  Expression* right = expr->right();

  // Call the runtime to find the boolean value of the left-hand
  // subexpression.  Duplicate the value if it may be needed as the final
  // result.
  if (left->AsLiteral() != NULL) {
    __ mov(r0, Operand(left->AsLiteral()->handle()));
    __ push(r0);
    if (destination.is_temporary()) __ push(r0);
  } else {
    Visit(left);
    ASSERT(left->location().is_temporary());
    if (destination.is_temporary()) {
      __ ldr(r0, MemOperand(sp));
      __ push(r0);
    }
  }
  // The left-hand value is in on top of the stack.  It is duplicated on the
  // stack iff the destination location is temporary.
  __ CallRuntime(Runtime::kToBool, 1);
  __ LoadRoot(ip, Heap::kTrueValueRootIndex);
  __ cmp(r0, ip);
  __ b(eq, &done);

  // Discard the left-hand value if present on the stack.
  if (destination.is_temporary()) __ pop();
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
