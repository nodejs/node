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

#define __ ACCESS_MASM(masm_)

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
  SetStatementPosition(stmt);
  VisitStatements(stmt->statements());
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
  UNREACHABLE();
}


void FastCodeGenerator::VisitBreakStatement(BreakStatement* stmt) {
  UNREACHABLE();
}


void FastCodeGenerator::VisitWithEnterStatement(WithEnterStatement* stmt) {
  UNREACHABLE();
}


void FastCodeGenerator::VisitWithExitStatement(WithExitStatement* stmt) {
  UNREACHABLE();
}


void FastCodeGenerator::VisitSwitchStatement(SwitchStatement* stmt) {
  UNREACHABLE();
}


void FastCodeGenerator::VisitDoWhileStatement(DoWhileStatement* stmt) {
  Comment cmnt(masm_, "[ DoWhileStatement");
  increment_loop_depth();
  Label body, exit, stack_limit_hit, stack_check_success;

  __ bind(&body);
  Visit(stmt->body());

  // Check stack before looping.
  __ StackLimitCheck(&stack_limit_hit);
  __ bind(&stack_check_success);

  // We are not in an expression context because we have been compiling
  // statements.  Set up a test expression context for the condition.
  ASSERT_EQ(NULL, true_label_);
  ASSERT_EQ(NULL, false_label_);
  true_label_ = &body;
  false_label_ = &exit;
  ASSERT(stmt->cond()->context() == Expression::kTest);
  Visit(stmt->cond());
  true_label_ = NULL;
  false_label_ = NULL;

  __ bind(&stack_limit_hit);
  StackCheckStub stack_stub;
  __ CallStub(&stack_stub);
  __ jmp(&stack_check_success);

  __ bind(&exit);

  decrement_loop_depth();
}


void FastCodeGenerator::VisitWhileStatement(WhileStatement* stmt) {
  Comment cmnt(masm_, "[ WhileStatement");
  increment_loop_depth();
  Label test, body, exit, stack_limit_hit, stack_check_success;

  // Emit the test at the bottom of the loop.
  __ jmp(&test);

  __ bind(&body);
  Visit(stmt->body());

  __ bind(&test);
  // Check stack before looping.
  __ StackLimitCheck(&stack_limit_hit);
  __ bind(&stack_check_success);

  // We are not in an expression context because we have been compiling
  // statements.  Set up a test expression context for the condition.
  ASSERT_EQ(NULL, true_label_);
  ASSERT_EQ(NULL, false_label_);
  true_label_ = &body;
  false_label_ = &exit;
  ASSERT(stmt->cond()->context() == Expression::kTest);
  Visit(stmt->cond());
  true_label_ = NULL;
  false_label_ = NULL;

  __ bind(&stack_limit_hit);
  StackCheckStub stack_stub;
  __ CallStub(&stack_stub);
  __ jmp(&stack_check_success);

  __ bind(&exit);

  decrement_loop_depth();
}


void FastCodeGenerator::VisitForStatement(ForStatement* stmt) {
  Comment cmnt(masm_, "[ ForStatement");
  Label test, body, exit, stack_limit_hit, stack_check_success;
  if (stmt->init() != NULL) Visit(stmt->init());

  increment_loop_depth();
  // Emit the test at the bottom of the loop (even if empty).
  __ jmp(&test);
  __ bind(&body);
  Visit(stmt->body());

  // Check stack before looping.
  __ StackLimitCheck(&stack_limit_hit);
  __ bind(&stack_check_success);

  if (stmt->next() != NULL) Visit(stmt->next());

  __ bind(&test);

  if (stmt->cond() == NULL) {
    // For an empty test jump to the top of the loop.
    __ jmp(&body);
  } else {
    // We are not in an expression context because we have been compiling
    // statements.  Set up a test expression context for the condition.
    ASSERT_EQ(NULL, true_label_);
    ASSERT_EQ(NULL, false_label_);

    true_label_ = &body;
    false_label_ = &exit;
    ASSERT(stmt->cond()->context() == Expression::kTest);
    Visit(stmt->cond());
    true_label_ = NULL;
    false_label_ = NULL;
  }

  __ bind(&stack_limit_hit);
  StackCheckStub stack_stub;
  __ CallStub(&stack_stub);
  __ jmp(&stack_check_success);

  __ bind(&exit);
  decrement_loop_depth();
}


void FastCodeGenerator::VisitForInStatement(ForInStatement* stmt) {
  UNREACHABLE();
}


void FastCodeGenerator::VisitTryCatchStatement(TryCatchStatement* stmt) {
  UNREACHABLE();
}


void FastCodeGenerator::VisitTryFinallyStatement(TryFinallyStatement* stmt) {
  UNREACHABLE();
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
  ASSERT(expr->op() == Token::ASSIGN || expr->op() == Token::INIT_VAR);

  // Record source code position of the (possible) IC call.
  SetSourcePosition(expr->position());

  Expression* rhs = expr->value();
  // Left-hand side can only be a property, a global or a (parameter or
  // local) slot.
  Variable* var = expr->target()->AsVariableProxy()->AsVariable();
  Property* prop = expr->target()->AsProperty();
  if (var != NULL) {
    Visit(rhs);
    ASSERT_EQ(Expression::kValue, rhs->context());
    EmitVariableAssignment(expr);
  } else if (prop != NULL) {
    // Assignment to a property.
    Visit(prop->obj());
    ASSERT_EQ(Expression::kValue, prop->obj()->context());
    // Use the expression context of the key subexpression to detect whether
    // we have decided to us a named or keyed IC.
    if (prop->key()->context() == Expression::kUninitialized) {
      ASSERT(prop->key()->AsLiteral() != NULL);
      Visit(rhs);
      ASSERT_EQ(Expression::kValue, rhs->context());
      EmitNamedPropertyAssignment(expr);
    } else {
      Visit(prop->key());
      ASSERT_EQ(Expression::kValue, prop->key()->context());
      Visit(rhs);
      ASSERT_EQ(Expression::kValue, rhs->context());
      EmitKeyedPropertyAssignment(expr);
    }
  } else {
    UNREACHABLE();
  }
}


void FastCodeGenerator::VisitCatchExtensionObject(CatchExtensionObject* expr) {
  UNREACHABLE();
}


void FastCodeGenerator::VisitThrow(Throw* expr) {
  UNREACHABLE();
}


void FastCodeGenerator::VisitThisFunction(ThisFunction* expr) {
  UNREACHABLE();
}


#undef __


} }  // namespace v8::internal
