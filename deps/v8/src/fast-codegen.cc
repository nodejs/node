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
#include "stub-cache.h"
#include "debug.h"

namespace v8 {
namespace internal {

#define __ ACCESS_MASM(masm())

Handle<Code> FastCodeGenerator::MakeCode(FunctionLiteral* fun,
                                         Handle<Script> script,
                                         bool is_eval) {
  CodeGenerator::MakeCodePrologue(fun);
  const int kInitialBufferSize = 4 * KB;
  MacroAssembler masm(NULL, kInitialBufferSize);
  FastCodeGenerator cgen(&masm, script, is_eval);
  cgen.Generate(fun);
  if (cgen.HasStackOverflow()) {
    ASSERT(!Top::has_pending_exception());
    return Handle<Code>::null();
  }
  Code::Flags flags = Code::ComputeFlags(Code::FUNCTION, NOT_IN_LOOP);
  return CodeGenerator::MakeCodeEpilogue(fun, &masm, flags, script);
}


int FastCodeGenerator::SlotOffset(Slot* slot) {
  ASSERT(slot != NULL);
  // Offset is negative because higher indexes are at lower addresses.
  int offset = -slot->index() * kPointerSize;
  // Adjust by a (parameter or local) base offset.
  switch (slot->type()) {
    case Slot::PARAMETER:
      offset += (function_->scope()->num_parameters() + 1) * kPointerSize;
      break;
    case Slot::LOCAL:
      offset += JavaScriptFrameConstants::kLocal0Offset;
      break;
    default:
      UNREACHABLE();
  }
  return offset;
}


void FastCodeGenerator::VisitDeclarations(
    ZoneList<Declaration*>* declarations) {
  int length = declarations->length();
  int globals = 0;
  for (int i = 0; i < length; i++) {
    Declaration* decl = declarations->at(i);
    Variable* var = decl->proxy()->var();
    Slot* slot = var->slot();

    // If it was not possible to allocate the variable at compile
    // time, we need to "declare" it at runtime to make sure it
    // actually exists in the local context.
    if ((slot != NULL && slot->type() == Slot::LOOKUP) || !var->is_global()) {
      VisitDeclaration(decl);
    } else {
      // Count global variables and functions for later processing
      globals++;
    }
  }

  // Compute array of global variable and function declarations.
  // Do nothing in case of no declared global functions or variables.
  if (globals > 0) {
    Handle<FixedArray> array = Factory::NewFixedArray(2 * globals, TENURED);
    for (int j = 0, i = 0; i < length; i++) {
      Declaration* decl = declarations->at(i);
      Variable* var = decl->proxy()->var();
      Slot* slot = var->slot();

      if ((slot == NULL || slot->type() != Slot::LOOKUP) && var->is_global()) {
        array->set(j++, *(var->name()));
        if (decl->fun() == NULL) {
          if (var->mode() == Variable::CONST) {
            // In case this is const property use the hole.
            array->set_the_hole(j++);
          } else {
            array->set_undefined(j++);
          }
        } else {
          Handle<JSFunction> function =
              Compiler::BuildBoilerplate(decl->fun(), script_, this);
          // Check for stack-overflow exception.
          if (HasStackOverflow()) return;
          array->set(j++, *function);
        }
      }
    }
    // Invoke the platform-dependent code generator to do the actual
    // declaration the global variables and functions.
    DeclareGlobals(array);
  }
}


void FastCodeGenerator::SetFunctionPosition(FunctionLiteral* fun) {
  if (FLAG_debug_info) {
    CodeGenerator::RecordPositions(masm_, fun->start_position());
  }
}


void FastCodeGenerator::SetReturnPosition(FunctionLiteral* fun) {
  if (FLAG_debug_info) {
    CodeGenerator::RecordPositions(masm_, fun->end_position());
  }
}


void FastCodeGenerator::SetStatementPosition(Statement* stmt) {
  if (FLAG_debug_info) {
    CodeGenerator::RecordPositions(masm_, stmt->statement_pos());
  }
}


void FastCodeGenerator::SetSourcePosition(int pos) {
  if (FLAG_debug_info && pos != RelocInfo::kNoPosition) {
    masm_->RecordPosition(pos);
  }
}


void FastCodeGenerator::EmitLogicalOperation(BinaryOperation* expr) {
#ifdef DEBUG
  Expression::Context expected = Expression::kUninitialized;
  switch (expr->context()) {
    case Expression::kUninitialized:
      UNREACHABLE();
    case Expression::kEffect:  // Fall through.
    case Expression::kTest:
      // The value of the left subexpression is not needed.
      expected = Expression::kTest;
      break;
    case Expression::kValue:
      // The value of the left subexpression is needed and its specific
      // context depends on the operator.
      expected = (expr->op() == Token::OR)
          ? Expression::kValueTest
          : Expression::kTestValue;
      break;
    case Expression::kValueTest:
      // The value of the left subexpression is needed for OR.
      expected = (expr->op() == Token::OR)
          ? Expression::kValueTest
          : Expression::kTest;
      break;
    case Expression::kTestValue:
      // The value of the left subexpression is needed for AND.
      expected = (expr->op() == Token::OR)
          ? Expression::kTest
          : Expression::kTestValue;
      break;
  }
  ASSERT_EQ(expected, expr->left()->context());
  ASSERT_EQ(expr->context(), expr->right()->context());
#endif

  Label eval_right, done;
  Label* saved_true = true_label_;
  Label* saved_false = false_label_;

  // Set up the appropriate context for the left subexpression based on the
  // operation and our own context.
  if (expr->op() == Token::OR) {
    // If there is no usable true label in the OR expression's context, use
    // the end of this expression, otherwise inherit the same true label.
    if (expr->context() == Expression::kEffect ||
        expr->context() == Expression::kValue) {
      true_label_ = &done;
    }
    // The false label is the label of the second subexpression.
    false_label_ = &eval_right;
  } else {
    ASSERT_EQ(Token::AND, expr->op());
    // The true label is the label of the second subexpression.
    true_label_ = &eval_right;
    // If there is no usable false label in the AND expression's context,
    // use the end of the expression, otherwise inherit the same false
    // label.
    if (expr->context() == Expression::kEffect ||
        expr->context() == Expression::kValue) {
      false_label_ = &done;
    }
  }

  Visit(expr->left());
  true_label_ = saved_true;
  false_label_ = saved_false;

  __ bind(&eval_right);
  Visit(expr->right());

  __ bind(&done);
}


void FastCodeGenerator::VisitBlock(Block* stmt) {
  Comment cmnt(masm_, "[ Block");
  Breakable nested_statement(this, stmt);
  SetStatementPosition(stmt);
  VisitStatements(stmt->statements());
  __ bind(nested_statement.break_target());
}


void FastCodeGenerator::VisitExpressionStatement(ExpressionStatement* stmt) {
  Comment cmnt(masm_, "[ ExpressionStatement");
  SetStatementPosition(stmt);
  Visit(stmt->expression());
}


void FastCodeGenerator::VisitEmptyStatement(EmptyStatement* stmt) {
  Comment cmnt(masm_, "[ EmptyStatement");
  SetStatementPosition(stmt);
}


void FastCodeGenerator::VisitIfStatement(IfStatement* stmt) {
  Comment cmnt(masm_, "[ IfStatement");
  // Expressions cannot recursively enter statements, there are no labels in
  // the state.
  ASSERT_EQ(NULL, true_label_);
  ASSERT_EQ(NULL, false_label_);
  Label then_part, else_part, done;

  // Do not worry about optimizing for empty then or else bodies.
  true_label_ = &then_part;
  false_label_ = &else_part;
  ASSERT(stmt->condition()->context() == Expression::kTest);
  Visit(stmt->condition());
  true_label_ = NULL;
  false_label_ = NULL;

  __ bind(&then_part);
  Visit(stmt->then_statement());
  __ jmp(&done);

  __ bind(&else_part);
  Visit(stmt->else_statement());

  __ bind(&done);
}


void FastCodeGenerator::VisitContinueStatement(ContinueStatement* stmt) {
  Comment cmnt(masm_,  "[ ContinueStatement");
  NestedStatement* current = nesting_stack_;
  int stack_depth = 0;
  while (!current->IsContinueTarget(stmt->target())) {
    stack_depth = current->Exit(stack_depth);
    current = current->outer();
  }
  __ Drop(stack_depth);

  Iteration* loop = current->AsIteration();
  __ jmp(loop->continue_target());
}


void FastCodeGenerator::VisitBreakStatement(BreakStatement* stmt) {
  Comment cmnt(masm_,  "[ BreakStatement");
  NestedStatement* current = nesting_stack_;
  int stack_depth = 0;
  while (!current->IsBreakTarget(stmt->target())) {
    stack_depth = current->Exit(stack_depth);
    current = current->outer();
  }
  __ Drop(stack_depth);

  Breakable* target = current->AsBreakable();
  __ jmp(target->break_target());
}


void FastCodeGenerator::VisitReturnStatement(ReturnStatement* stmt) {
  Comment cmnt(masm_, "[ ReturnStatement");
  Expression* expr = stmt->expression();
  // Complete the statement based on the type of the subexpression.
  if (expr->AsLiteral() != NULL) {
    __ Move(result_register(), expr->AsLiteral()->handle());
  } else {
    ASSERT_EQ(Expression::kValue, expr->context());
    Visit(expr);
    __ pop(result_register());
  }

  // Exit all nested statements.
  NestedStatement* current = nesting_stack_;
  int stack_depth = 0;
  while (current != NULL) {
    stack_depth = current->Exit(stack_depth);
    current = current->outer();
  }
  __ Drop(stack_depth);

  EmitReturnSequence(stmt->statement_pos());
}




void FastCodeGenerator::VisitWithEnterStatement(WithEnterStatement* stmt) {
  Comment cmnt(masm_, "[ WithEnterStatement");
  SetStatementPosition(stmt);

  Visit(stmt->expression());
  if (stmt->is_catch_block()) {
    __ CallRuntime(Runtime::kPushCatchContext, 1);
  } else {
    __ CallRuntime(Runtime::kPushContext, 1);
  }
  // Both runtime calls return the new context in both the context and the
  // result registers.

  // Update local stack frame context field.
  StoreToFrameField(StandardFrameConstants::kContextOffset, context_register());
}


void FastCodeGenerator::VisitWithExitStatement(WithExitStatement* stmt) {
  Comment cmnt(masm_, "[ WithExitStatement");
  SetStatementPosition(stmt);

  // Pop context.
  LoadContextField(context_register(), Context::PREVIOUS_INDEX);
  // Update local stack frame context field.
  StoreToFrameField(StandardFrameConstants::kContextOffset, context_register());
}


void FastCodeGenerator::VisitSwitchStatement(SwitchStatement* stmt) {
  UNREACHABLE();
}


void FastCodeGenerator::VisitDoWhileStatement(DoWhileStatement* stmt) {
  Comment cmnt(masm_, "[ DoWhileStatement");
  Label body, stack_limit_hit, stack_check_success;

  Iteration loop_statement(this, stmt);
  increment_loop_depth();

  __ bind(&body);
  Visit(stmt->body());

  // Check stack before looping.
  __ StackLimitCheck(&stack_limit_hit);
  __ bind(&stack_check_success);

  // We are not in an expression context because we have been compiling
  // statements.  Set up a test expression context for the condition.
  __ bind(loop_statement.continue_target());
  ASSERT_EQ(NULL, true_label_);
  ASSERT_EQ(NULL, false_label_);
  true_label_ = &body;
  false_label_ = loop_statement.break_target();
  ASSERT(stmt->cond()->context() == Expression::kTest);
  Visit(stmt->cond());
  true_label_ = NULL;
  false_label_ = NULL;

  __ bind(&stack_limit_hit);
  StackCheckStub stack_stub;
  __ CallStub(&stack_stub);
  __ jmp(&stack_check_success);

  __ bind(loop_statement.break_target());

  decrement_loop_depth();
}


void FastCodeGenerator::VisitWhileStatement(WhileStatement* stmt) {
  Comment cmnt(masm_, "[ WhileStatement");
  Label body, stack_limit_hit, stack_check_success;

  Iteration loop_statement(this, stmt);
  increment_loop_depth();

  // Emit the test at the bottom of the loop.
  __ jmp(loop_statement.continue_target());

  __ bind(&body);
  Visit(stmt->body());

  __ bind(loop_statement.continue_target());
  // Check stack before looping.
  __ StackLimitCheck(&stack_limit_hit);
  __ bind(&stack_check_success);

  // We are not in an expression context because we have been compiling
  // statements.  Set up a test expression context for the condition.
  ASSERT_EQ(NULL, true_label_);
  ASSERT_EQ(NULL, false_label_);
  true_label_ = &body;
  false_label_ = loop_statement.break_target();
  ASSERT(stmt->cond()->context() == Expression::kTest);
  Visit(stmt->cond());
  true_label_ = NULL;
  false_label_ = NULL;

  __ bind(&stack_limit_hit);
  StackCheckStub stack_stub;
  __ CallStub(&stack_stub);
  __ jmp(&stack_check_success);

  __ bind(loop_statement.break_target());
  decrement_loop_depth();
}


void FastCodeGenerator::VisitForStatement(ForStatement* stmt) {
  UNREACHABLE();
}


void FastCodeGenerator::VisitForInStatement(ForInStatement* stmt) {
  UNREACHABLE();
}


void FastCodeGenerator::VisitTryCatchStatement(TryCatchStatement* stmt) {
  UNREACHABLE();
}


void FastCodeGenerator::VisitTryFinallyStatement(TryFinallyStatement* stmt) {
  // Try finally is compiled by setting up a try-handler on the stack while
  // executing the try body, and removing it again afterwards.
  //
  // The try-finally construct can enter the finally block in three ways:
  // 1. By exiting the try-block normally. This removes the try-handler and
  //      calls the finally block code before continuing.
  // 2. By exiting the try-block with a function-local control flow transfer
  //    (break/continue/return). The site of the, e.g., break removes the
  //    try handler and calls the finally block code before continuing
  //    its outward control transfer.
  // 3. by exiting the try-block with a thrown exception.
  //    This can happen in nested function calls. It traverses the try-handler
  //    chaing and consumes the try-handler entry before jumping to the
  //    handler code. The handler code then calls the finally-block before
  //    rethrowing the exception.
  //
  // The finally block must assume a return address on top of the stack
  // (or in the link register on ARM chips) and a value (return value or
  // exception) in the result register (rax/eax/r0), both of which must
  // be preserved. The return address isn't GC-safe, so it should be
  // cooked before GC.
  Label finally_entry;
  Label try_handler_setup;

  // Setup the try-handler chain. Use a call to
  // Jump to try-handler setup and try-block code. Use call to put try-handler
  // address on stack.
  __ Call(&try_handler_setup);
  // Try handler code. Return address of call is pushed on handler stack.
  {
    // This code is only executed during stack-handler traversal when an
    // exception is thrown. The execption is in the result register, which
    // is retained by the finally block.
    // Call the finally block and then rethrow the exception.
    __ Call(&finally_entry);
    ThrowException();
  }

  __ bind(&finally_entry);
  {
    // Finally block implementation.
    EnterFinallyBlock();
    Finally finally_block(this);
    Visit(stmt->finally_block());
    ExitFinallyBlock();  // Return to the calling code.
  }

  __ bind(&try_handler_setup);
  {
    // Setup try handler (stack pointer registers).
    __ PushTryHandler(IN_JAVASCRIPT, TRY_FINALLY_HANDLER);
    TryFinally try_block(this, &finally_entry);
    VisitStatements(stmt->try_block()->statements());
    __ PopTryHandler();
  }
  // Execute the finally block on the way out.
  __ Call(&finally_entry);
}


void FastCodeGenerator::VisitDebuggerStatement(DebuggerStatement* stmt) {
#ifdef ENABLE_DEBUGGER_SUPPORT
  Comment cmnt(masm_, "[ DebuggerStatement");
  SetStatementPosition(stmt);
  __ CallRuntime(Runtime::kDebugBreak, 0);
  // Ignore the return value.
#endif
}


void FastCodeGenerator::VisitFunctionBoilerplateLiteral(
    FunctionBoilerplateLiteral* expr) {
  UNREACHABLE();
}


void FastCodeGenerator::VisitConditional(Conditional* expr) {
  Comment cmnt(masm_, "[ Conditional");
  ASSERT_EQ(Expression::kTest, expr->condition()->context());
  ASSERT_EQ(expr->context(), expr->then_expression()->context());
  ASSERT_EQ(expr->context(), expr->else_expression()->context());


  Label true_case, false_case, done;
  Label* saved_true = true_label_;
  Label* saved_false = false_label_;

  true_label_ = &true_case;
  false_label_ = &false_case;
  Visit(expr->condition());
  true_label_ = saved_true;
  false_label_ = saved_false;

  __ bind(&true_case);
  Visit(expr->then_expression());
  // If control flow falls through Visit, jump to done.
  if (expr->context() == Expression::kEffect ||
      expr->context() == Expression::kValue) {
    __ jmp(&done);
  }

  __ bind(&false_case);
  Visit(expr->else_expression());
  // If control flow falls through Visit, merge it with true case here.
  if (expr->context() == Expression::kEffect ||
      expr->context() == Expression::kValue) {
    __ bind(&done);
  }
}


void FastCodeGenerator::VisitSlot(Slot* expr) {
  // Slots do not appear directly in the AST.
  UNREACHABLE();
}


void FastCodeGenerator::VisitLiteral(Literal* expr) {
  Comment cmnt(masm_, "[ Literal");
  Move(expr->context(), expr);
}


void FastCodeGenerator::VisitAssignment(Assignment* expr) {
  Comment cmnt(masm_, "[ Assignment");

  // Record source code position of the (possible) IC call.
  SetSourcePosition(expr->position());

  // Left-hand side can only be a property, a global or a (parameter or local)
  // slot. Variables with rewrite to .arguments are treated as KEYED_PROPERTY.
  enum LhsKind { VARIABLE, NAMED_PROPERTY, KEYED_PROPERTY };
  LhsKind assign_type = VARIABLE;
  Property* prop = expr->target()->AsProperty();
  // In case of a property we use the uninitialized expression context
  // of the key to detect a named property.
  if (prop != NULL) {
    assign_type = (prop->key()->context() == Expression::kUninitialized)
        ? NAMED_PROPERTY
        : KEYED_PROPERTY;
  }

  // Evaluate LHS expression.
  switch (assign_type) {
    case VARIABLE:
      // Nothing to do here.
      break;
    case NAMED_PROPERTY:
      Visit(prop->obj());
      ASSERT_EQ(Expression::kValue, prop->obj()->context());
      break;
    case KEYED_PROPERTY:
      Visit(prop->obj());
      ASSERT_EQ(Expression::kValue, prop->obj()->context());
      Visit(prop->key());
      ASSERT_EQ(Expression::kValue, prop->key()->context());
      break;
  }

  // If we have a compound assignment: Get value of LHS expression and
  // store in on top of the stack.
  // Note: Relies on kValue context being 'stack'.
  if (expr->is_compound()) {
    switch (assign_type) {
      case VARIABLE:
        EmitVariableLoad(expr->target()->AsVariableProxy()->var(),
                         Expression::kValue);
        break;
      case NAMED_PROPERTY:
        EmitNamedPropertyLoad(prop, Expression::kValue);
        break;
      case KEYED_PROPERTY:
        EmitKeyedPropertyLoad(Expression::kValue);
        break;
    }
  }

  // Evaluate RHS expression.
  Expression* rhs = expr->value();
  ASSERT_EQ(Expression::kValue, rhs->context());
  Visit(rhs);

  // If we have a compount assignment: Apply operator.
  if (expr->is_compound()) {
    EmitCompoundAssignmentOp(expr->binary_op(), Expression::kValue);
  }

  // Store the value.
  switch (assign_type) {
    case VARIABLE:
      EmitVariableAssignment(expr);
      break;
    case NAMED_PROPERTY:
      EmitNamedPropertyAssignment(expr);
      break;
    case KEYED_PROPERTY:
      EmitKeyedPropertyAssignment(expr);
      break;
  }
}


void FastCodeGenerator::VisitCatchExtensionObject(CatchExtensionObject* expr) {
  UNREACHABLE();
}


void FastCodeGenerator::VisitThrow(Throw* expr) {
  UNREACHABLE();
}


int FastCodeGenerator::TryFinally::Exit(int stack_depth) {
  // The macros used here must preserve the result register.
  __ Drop(stack_depth);
  __ PopTryHandler();
  __ Call(finally_entry_);
  return 0;
}


int FastCodeGenerator::TryCatch::Exit(int stack_depth) {
  // The macros used here must preserve the result register.
  __ Drop(stack_depth);
  __ PopTryHandler();
  return 0;
}


#undef __


} }  // namespace v8::internal
