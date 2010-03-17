// Copyright 2006-2008 the V8 project authors. All rights reserved.
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

#include "ast.h"
#include "func-name-inferrer.h"
#include "scopes.h"
#include "rewriter.h"

namespace v8 {
namespace internal {


class AstOptimizer: public AstVisitor {
 public:
  explicit AstOptimizer() : has_function_literal_(false) {}
  explicit AstOptimizer(Handle<String> enclosing_name)
      : has_function_literal_(false) {
    func_name_inferrer_.PushEnclosingName(enclosing_name);
  }

  void Optimize(ZoneList<Statement*>* statements);

 private:
  // Used for loop condition analysis.  Cleared before visiting a loop
  // condition, set when a function literal is visited.
  bool has_function_literal_;
  // Helper object for function name inferring.
  FuncNameInferrer func_name_inferrer_;

  // Helpers
  void OptimizeArguments(ZoneList<Expression*>* arguments);

  // Node visitors.
#define DEF_VISIT(type) \
  virtual void Visit##type(type* node);
  AST_NODE_LIST(DEF_VISIT)
#undef DEF_VISIT

  DISALLOW_COPY_AND_ASSIGN(AstOptimizer);
};


void AstOptimizer::Optimize(ZoneList<Statement*>* statements) {
  int len = statements->length();
  for (int i = 0; i < len; i++) {
    Visit(statements->at(i));
  }
}


void AstOptimizer::OptimizeArguments(ZoneList<Expression*>* arguments) {
  for (int i = 0; i < arguments->length(); i++) {
    Visit(arguments->at(i));
  }
}


void AstOptimizer::VisitBlock(Block* node) {
  Optimize(node->statements());
}


void AstOptimizer::VisitExpressionStatement(ExpressionStatement* node) {
  Visit(node->expression());
}


void AstOptimizer::VisitIfStatement(IfStatement* node) {
  Visit(node->condition());
  Visit(node->then_statement());
  if (node->HasElseStatement()) {
    Visit(node->else_statement());
  }
}


void AstOptimizer::VisitDoWhileStatement(DoWhileStatement* node) {
  Visit(node->cond());
  Visit(node->body());
}


void AstOptimizer::VisitWhileStatement(WhileStatement* node) {
  has_function_literal_ = false;
  Visit(node->cond());
  node->may_have_function_literal_ = has_function_literal_;
  Visit(node->body());
}


void AstOptimizer::VisitForStatement(ForStatement* node) {
  if (node->init() != NULL) {
    Visit(node->init());
  }
  if (node->cond() != NULL) {
    has_function_literal_ = false;
    Visit(node->cond());
    node->may_have_function_literal_ = has_function_literal_;
  }
  Visit(node->body());
  if (node->next() != NULL) {
    Visit(node->next());
  }
}


void AstOptimizer::VisitForInStatement(ForInStatement* node) {
  Visit(node->each());
  Visit(node->enumerable());
  Visit(node->body());
}


void AstOptimizer::VisitTryCatchStatement(TryCatchStatement* node) {
  Visit(node->try_block());
  Visit(node->catch_var());
  Visit(node->catch_block());
}


void AstOptimizer::VisitTryFinallyStatement(TryFinallyStatement* node) {
  Visit(node->try_block());
  Visit(node->finally_block());
}


void AstOptimizer::VisitSwitchStatement(SwitchStatement* node) {
  Visit(node->tag());
  for (int i = 0; i < node->cases()->length(); i++) {
    CaseClause* clause = node->cases()->at(i);
    if (!clause->is_default()) {
      Visit(clause->label());
    }
    Optimize(clause->statements());
  }
}


void AstOptimizer::VisitContinueStatement(ContinueStatement* node) {
  USE(node);
}


void AstOptimizer::VisitBreakStatement(BreakStatement* node) {
  USE(node);
}


void AstOptimizer::VisitDeclaration(Declaration* node) {
  // Will not be reached by the current optimizations.
  USE(node);
}


void AstOptimizer::VisitEmptyStatement(EmptyStatement* node) {
  USE(node);
}


void AstOptimizer::VisitReturnStatement(ReturnStatement* node) {
  Visit(node->expression());
}


void AstOptimizer::VisitWithEnterStatement(WithEnterStatement* node) {
  Visit(node->expression());
}


void AstOptimizer::VisitWithExitStatement(WithExitStatement* node) {
  USE(node);
}


void AstOptimizer::VisitDebuggerStatement(DebuggerStatement* node) {
  USE(node);
}


void AstOptimizer::VisitFunctionLiteral(FunctionLiteral* node) {
  has_function_literal_ = true;

  if (node->name()->length() == 0) {
    // Anonymous function.
    func_name_inferrer_.AddFunction(node);
  }
}


void AstOptimizer::VisitFunctionBoilerplateLiteral(
    FunctionBoilerplateLiteral* node) {
  USE(node);
}


void AstOptimizer::VisitConditional(Conditional* node) {
  node->condition()->set_no_negative_zero(true);
  Visit(node->condition());
  Visit(node->then_expression());
  Visit(node->else_expression());
}


void AstOptimizer::VisitSlot(Slot* node) {
  USE(node);
}


void AstOptimizer::VisitVariableProxy(VariableProxy* node) {
  Variable* var = node->AsVariable();
  if (var != NULL) {
    if (var->type()->IsKnown()) {
      node->type()->CopyFrom(var->type());
    } else if (node->type()->IsLikelySmi()) {
      var->type()->SetAsLikelySmi();
    }

    if (!var->is_this() &&
        !Heap::result_symbol()->Equals(*var->name())) {
      func_name_inferrer_.PushName(var->name());
    }

    if (FLAG_safe_int32_compiler) {
      if (var->IsStackAllocated() && !var->is_arguments()) {
        node->set_side_effect_free(true);
      }
    }
  }
}


void AstOptimizer::VisitLiteral(Literal* node) {
  Handle<Object> literal = node->handle();
  if (literal->IsSmi()) {
    node->type()->SetAsLikelySmi();
    node->set_side_effect_free(true);
  } else if (literal->IsString()) {
    Handle<String> lit_str(Handle<String>::cast(literal));
    if (!Heap::prototype_symbol()->Equals(*lit_str)) {
      func_name_inferrer_.PushName(lit_str);
    }
  } else if (literal->IsHeapNumber()) {
    if (node->to_int32()) {
      // Any HeapNumber has an int32 value if it is the input to a bit op.
      node->set_side_effect_free(true);
    } else {
      double double_value = HeapNumber::cast(*literal)->value();
      int32_t int32_value = DoubleToInt32(double_value);
      node->set_side_effect_free(double_value == int32_value);
    }
  }
}


void AstOptimizer::VisitRegExpLiteral(RegExpLiteral* node) {
  USE(node);
}


void AstOptimizer::VisitArrayLiteral(ArrayLiteral* node) {
  for (int i = 0; i < node->values()->length(); i++) {
    Visit(node->values()->at(i));
  }
}

void AstOptimizer::VisitObjectLiteral(ObjectLiteral* node) {
  for (int i = 0; i < node->properties()->length(); i++) {
    ScopedFuncNameInferrer scoped_fni(&func_name_inferrer_);
    scoped_fni.Enter();
    Visit(node->properties()->at(i)->key());
    Visit(node->properties()->at(i)->value());
  }
}


void AstOptimizer::VisitCatchExtensionObject(CatchExtensionObject* node) {
  Visit(node->key());
  Visit(node->value());
}


void AstOptimizer::VisitAssignment(Assignment* node) {
  ScopedFuncNameInferrer scoped_fni(&func_name_inferrer_);
  switch (node->op()) {
    case Token::INIT_VAR:
    case Token::INIT_CONST:
    case Token::ASSIGN:
      // No type can be infered from the general assignment.

      // Don't infer if it is "a = function(){...}();"-like expression.
      if (node->value()->AsCall() == NULL) {
        scoped_fni.Enter();
      }
      break;
    case Token::ASSIGN_BIT_OR:
    case Token::ASSIGN_BIT_XOR:
    case Token::ASSIGN_BIT_AND:
    case Token::ASSIGN_SHL:
    case Token::ASSIGN_SAR:
    case Token::ASSIGN_SHR:
      node->type()->SetAsLikelySmiIfUnknown();
      node->target()->type()->SetAsLikelySmiIfUnknown();
      node->value()->type()->SetAsLikelySmiIfUnknown();
      node->value()->set_to_int32(true);
      node->value()->set_no_negative_zero(true);
      break;
    case Token::ASSIGN_ADD:
    case Token::ASSIGN_SUB:
    case Token::ASSIGN_MUL:
    case Token::ASSIGN_DIV:
    case Token::ASSIGN_MOD:
      if (node->type()->IsLikelySmi()) {
        node->target()->type()->SetAsLikelySmiIfUnknown();
        node->value()->type()->SetAsLikelySmiIfUnknown();
      }
      break;
    default:
      UNREACHABLE();
      break;
  }

  Visit(node->target());
  Visit(node->value());

  switch (node->op()) {
    case Token::INIT_VAR:
    case Token::INIT_CONST:
    case Token::ASSIGN:
      // Pure assignment copies the type from the value.
      node->type()->CopyFrom(node->value()->type());
      break;
    case Token::ASSIGN_BIT_OR:
    case Token::ASSIGN_BIT_XOR:
    case Token::ASSIGN_BIT_AND:
    case Token::ASSIGN_SHL:
    case Token::ASSIGN_SAR:
    case Token::ASSIGN_SHR:
      // Should have been setup above already.
      break;
    case Token::ASSIGN_ADD:
    case Token::ASSIGN_SUB:
    case Token::ASSIGN_MUL:
    case Token::ASSIGN_DIV:
    case Token::ASSIGN_MOD:
      if (node->type()->IsUnknown()) {
        if (node->target()->type()->IsLikelySmi() ||
            node->value()->type()->IsLikelySmi()) {
          node->type()->SetAsLikelySmi();
        }
      }
      break;
    default:
      UNREACHABLE();
      break;
  }

  // Since this is an assignment. We have to propagate this node's type to the
  // variable.
  VariableProxy* proxy = node->target()->AsVariableProxy();
  if (proxy != NULL) {
    Variable* var = proxy->AsVariable();
    if (var != NULL) {
      StaticType* var_type = var->type();
      if (var_type->IsUnknown()) {
        var_type->CopyFrom(node->type());
      } else if (var_type->IsLikelySmi()) {
        // We do not reset likely types to Unknown.
      }
    }
  }
}


void AstOptimizer::VisitThrow(Throw* node) {
  Visit(node->exception());
}


void AstOptimizer::VisitProperty(Property* node) {
  node->key()->set_no_negative_zero(true);
  Visit(node->obj());
  Visit(node->key());
}


void AstOptimizer::VisitCall(Call* node) {
  Visit(node->expression());
  OptimizeArguments(node->arguments());
}


void AstOptimizer::VisitCallNew(CallNew* node) {
  Visit(node->expression());
  OptimizeArguments(node->arguments());
}


void AstOptimizer::VisitCallRuntime(CallRuntime* node) {
  ScopedFuncNameInferrer scoped_fni(&func_name_inferrer_);
  if (Factory::InitializeVarGlobal_symbol()->Equals(*node->name()) &&
      node->arguments()->length() >= 2 &&
      node->arguments()->at(1)->AsFunctionLiteral() != NULL) {
      scoped_fni.Enter();
  }
  OptimizeArguments(node->arguments());
}


void AstOptimizer::VisitUnaryOperation(UnaryOperation* node) {
  if (node->op() == Token::ADD || node->op() == Token::SUB) {
    node->expression()->set_no_negative_zero(node->no_negative_zero());
  } else {
    node->expression()->set_no_negative_zero(true);
  }
  Visit(node->expression());
  if (FLAG_safe_int32_compiler) {
    switch (node->op()) {
      case Token::BIT_NOT:
        node->expression()->set_to_int32(true);
        // Fall through.
      case Token::ADD:
      case Token::SUB:
        node->set_side_effect_free(node->expression()->side_effect_free());
        break;
      case Token::NOT:
      case Token::DELETE:
      case Token::TYPEOF:
      case Token::VOID:
        break;
      default:
        UNREACHABLE();
        break;
    }
  } else if (node->op() == Token::BIT_NOT) {
    node->expression()->set_to_int32(true);
  }
}


void AstOptimizer::VisitCountOperation(CountOperation* node) {
  // Count operations assume that they work on Smis.
  node->expression()->set_no_negative_zero(node->is_prefix() ?
                                           true :
                                           node->no_negative_zero());
  node->type()->SetAsLikelySmiIfUnknown();
  node->expression()->type()->SetAsLikelySmiIfUnknown();
  Visit(node->expression());
}


void AstOptimizer::VisitBinaryOperation(BinaryOperation* node) {
  // Depending on the operation we can propagate this node's type down the
  // AST nodes.
  switch (node->op()) {
    case Token::COMMA:
    case Token::OR:
      node->left()->set_no_negative_zero(true);
      node->right()->set_no_negative_zero(node->no_negative_zero());
      break;
    case Token::AND:
      node->left()->set_no_negative_zero(node->no_negative_zero());
      node->right()->set_no_negative_zero(node->no_negative_zero());
      break;
    case Token::BIT_OR:
    case Token::BIT_XOR:
    case Token::BIT_AND:
    case Token::SHL:
    case Token::SAR:
    case Token::SHR:
      node->type()->SetAsLikelySmiIfUnknown();
      node->left()->type()->SetAsLikelySmiIfUnknown();
      node->right()->type()->SetAsLikelySmiIfUnknown();
      node->left()->set_to_int32(true);
      node->right()->set_to_int32(true);
      node->left()->set_no_negative_zero(true);
      node->right()->set_no_negative_zero(true);
      break;
    case Token::ADD:
    case Token::SUB:
    case Token::MUL:
    case Token::DIV:
    case Token::MOD:
      if (node->type()->IsLikelySmi()) {
        node->left()->type()->SetAsLikelySmiIfUnknown();
        node->right()->type()->SetAsLikelySmiIfUnknown();
      }
      node->left()->set_no_negative_zero(node->no_negative_zero());
      node->right()->set_no_negative_zero(node->no_negative_zero());
      if (node->op() == Token::DIV) {
        node->right()->set_no_negative_zero(false);
      } else if (node->op() == Token::MOD) {
        node->right()->set_no_negative_zero(true);
      }
      break;
    default:
      UNREACHABLE();
      break;
  }

  Visit(node->left());
  Visit(node->right());

  // After visiting the operand nodes we have to check if this node's type
  // can be updated. If it does, then we can push that information down
  // towards the leafs again if the new information is an upgrade over the
  // previous type of the operand nodes.
  if (node->type()->IsUnknown()) {
    if (node->left()->type()->IsLikelySmi() ||
        node->right()->type()->IsLikelySmi()) {
      node->type()->SetAsLikelySmi();
    }
    if (node->type()->IsLikelySmi()) {
      // The type of this node changed to LIKELY_SMI. Propagate this knowledge
      // down through the nodes.
      if (node->left()->type()->IsUnknown()) {
        node->left()->type()->SetAsLikelySmi();
        Visit(node->left());
      }
      if (node->right()->type()->IsUnknown()) {
        node->right()->type()->SetAsLikelySmi();
        Visit(node->right());
      }
    }
  }

  if (FLAG_safe_int32_compiler) {
    switch (node->op()) {
      case Token::COMMA:
      case Token::OR:
      case Token::AND:
        break;
      case Token::BIT_OR:
      case Token::BIT_XOR:
      case Token::BIT_AND:
      case Token::SHL:
      case Token::SAR:
      case Token::SHR:
        // Add one to the number of bit operations in this expression.
        node->set_num_bit_ops(1);
        // Fall through.
      case Token::ADD:
      case Token::SUB:
      case Token::MUL:
      case Token::DIV:
      case Token::MOD:
        node->set_side_effect_free(node->left()->side_effect_free() &&
                                   node->right()->side_effect_free());
        node->set_num_bit_ops(node->num_bit_ops() +
                                  node->left()->num_bit_ops() +
                                  node->right()->num_bit_ops());
        if (!node->no_negative_zero() && node->op() == Token::MUL) {
          node->set_side_effect_free(false);
        }
        break;
      default:
        UNREACHABLE();
        break;
    }
  }
}


void AstOptimizer::VisitCompareOperation(CompareOperation* node) {
  if (node->type()->IsKnown()) {
    // Propagate useful information down towards the leafs.
    node->left()->type()->SetAsLikelySmiIfUnknown();
    node->right()->type()->SetAsLikelySmiIfUnknown();
  }

  node->left()->set_no_negative_zero(true);
  // Only [[HasInstance]] has the right argument passed unchanged to it.
  node->right()->set_no_negative_zero(true);

  Visit(node->left());
  Visit(node->right());

  // After visiting the operand nodes we have to check if this node's type
  // can be updated. If it does, then we can push that information down
  // towards the leafs again if the new information is an upgrade over the
  // previous type of the operand nodes.
  if (node->type()->IsUnknown()) {
    if (node->left()->type()->IsLikelySmi() ||
        node->right()->type()->IsLikelySmi()) {
      node->type()->SetAsLikelySmi();
    }
    if (node->type()->IsLikelySmi()) {
      // The type of this node changed to LIKELY_SMI. Propagate this knowledge
      // down through the nodes.
      if (node->left()->type()->IsUnknown()) {
        node->left()->type()->SetAsLikelySmi();
        Visit(node->left());
      }
      if (node->right()->type()->IsUnknown()) {
        node->right()->type()->SetAsLikelySmi();
        Visit(node->right());
      }
    }
  }
}


void AstOptimizer::VisitThisFunction(ThisFunction* node) {
  USE(node);
}


class Processor: public AstVisitor {
 public:
  explicit Processor(VariableProxy* result)
      : result_(result),
        result_assigned_(false),
        is_set_(false),
        in_try_(false) {
  }

  void Process(ZoneList<Statement*>* statements);
  bool result_assigned() const  { return result_assigned_; }

 private:
  VariableProxy* result_;

  // We are not tracking result usage via the result_'s use
  // counts (we leave the accurate computation to the
  // usage analyzer). Instead we simple remember if
  // there was ever an assignment to result_.
  bool result_assigned_;

  // To avoid storing to .result all the time, we eliminate some of
  // the stores by keeping track of whether or not we're sure .result
  // will be overwritten anyway. This is a bit more tricky than what I
  // was hoping for
  bool is_set_;
  bool in_try_;

  Expression* SetResult(Expression* value) {
    result_assigned_ = true;
    return new Assignment(Token::ASSIGN, result_, value,
                          RelocInfo::kNoPosition);
  }

  // Node visitors.
#define DEF_VISIT(type) \
  virtual void Visit##type(type* node);
  AST_NODE_LIST(DEF_VISIT)
#undef DEF_VISIT

  void VisitIterationStatement(IterationStatement* stmt);
};


void Processor::Process(ZoneList<Statement*>* statements) {
  for (int i = statements->length() - 1; i >= 0; --i) {
    Visit(statements->at(i));
  }
}


void Processor::VisitBlock(Block* node) {
  // An initializer block is the rewritten form of a variable declaration
  // with initialization expressions. The initializer block contains the
  // list of assignments corresponding to the initialization expressions.
  // While unclear from the spec (ECMA-262, 3rd., 12.2), the value of
  // a variable declaration with initialization expression is 'undefined'
  // with some JS VMs: For instance, using smjs, print(eval('var x = 7'))
  // returns 'undefined'. To obtain the same behavior with v8, we need
  // to prevent rewriting in that case.
  if (!node->is_initializer_block()) Process(node->statements());
}


void Processor::VisitExpressionStatement(ExpressionStatement* node) {
  // Rewrite : <x>; -> .result = <x>;
  if (!is_set_) {
    node->set_expression(SetResult(node->expression()));
    if (!in_try_) is_set_ = true;
  }
}


void Processor::VisitIfStatement(IfStatement* node) {
  // Rewrite both then and else parts (reversed).
  bool save = is_set_;
  Visit(node->else_statement());
  bool set_after_then = is_set_;
  is_set_ = save;
  Visit(node->then_statement());
  is_set_ = is_set_ && set_after_then;
}


void Processor::VisitIterationStatement(IterationStatement* node) {
  // Rewrite the body.
  bool set_after_loop = is_set_;
  Visit(node->body());
  is_set_ = is_set_ && set_after_loop;
}


void Processor::VisitDoWhileStatement(DoWhileStatement* node) {
  VisitIterationStatement(node);
}


void Processor::VisitWhileStatement(WhileStatement* node) {
  VisitIterationStatement(node);
}


void Processor::VisitForStatement(ForStatement* node) {
  VisitIterationStatement(node);
}


void Processor::VisitForInStatement(ForInStatement* node) {
  VisitIterationStatement(node);
}


void Processor::VisitTryCatchStatement(TryCatchStatement* node) {
  // Rewrite both try and catch blocks (reversed order).
  bool set_after_catch = is_set_;
  Visit(node->catch_block());
  is_set_ = is_set_ && set_after_catch;
  bool save = in_try_;
  in_try_ = true;
  Visit(node->try_block());
  in_try_ = save;
}


void Processor::VisitTryFinallyStatement(TryFinallyStatement* node) {
  // Rewrite both try and finally block (reversed order).
  Visit(node->finally_block());
  bool save = in_try_;
  in_try_ = true;
  Visit(node->try_block());
  in_try_ = save;
}


void Processor::VisitSwitchStatement(SwitchStatement* node) {
  // Rewrite statements in all case clauses in reversed order.
  ZoneList<CaseClause*>* clauses = node->cases();
  bool set_after_switch = is_set_;
  for (int i = clauses->length() - 1; i >= 0; --i) {
    CaseClause* clause = clauses->at(i);
    Process(clause->statements());
  }
  is_set_ = is_set_ && set_after_switch;
}


void Processor::VisitContinueStatement(ContinueStatement* node) {
  is_set_ = false;
}


void Processor::VisitBreakStatement(BreakStatement* node) {
  is_set_ = false;
}


// Do nothing:
void Processor::VisitDeclaration(Declaration* node) {}
void Processor::VisitEmptyStatement(EmptyStatement* node) {}
void Processor::VisitReturnStatement(ReturnStatement* node) {}
void Processor::VisitWithEnterStatement(WithEnterStatement* node) {}
void Processor::VisitWithExitStatement(WithExitStatement* node) {}
void Processor::VisitDebuggerStatement(DebuggerStatement* node) {}


// Expressions are never visited yet.
void Processor::VisitFunctionLiteral(FunctionLiteral* node) {
  USE(node);
  UNREACHABLE();
}


void Processor::VisitFunctionBoilerplateLiteral(
    FunctionBoilerplateLiteral* node) {
  USE(node);
  UNREACHABLE();
}


void Processor::VisitConditional(Conditional* node) {
  USE(node);
  UNREACHABLE();
}


void Processor::VisitSlot(Slot* node) {
  USE(node);
  UNREACHABLE();
}


void Processor::VisitVariableProxy(VariableProxy* node) {
  USE(node);
  UNREACHABLE();
}


void Processor::VisitLiteral(Literal* node) {
  USE(node);
  UNREACHABLE();
}


void Processor::VisitRegExpLiteral(RegExpLiteral* node) {
  USE(node);
  UNREACHABLE();
}


void Processor::VisitArrayLiteral(ArrayLiteral* node) {
  USE(node);
  UNREACHABLE();
}


void Processor::VisitObjectLiteral(ObjectLiteral* node) {
  USE(node);
  UNREACHABLE();
}


void Processor::VisitCatchExtensionObject(CatchExtensionObject* node) {
  USE(node);
  UNREACHABLE();
}


void Processor::VisitAssignment(Assignment* node) {
  USE(node);
  UNREACHABLE();
}


void Processor::VisitThrow(Throw* node) {
  USE(node);
  UNREACHABLE();
}


void Processor::VisitProperty(Property* node) {
  USE(node);
  UNREACHABLE();
}


void Processor::VisitCall(Call* node) {
  USE(node);
  UNREACHABLE();
}


void Processor::VisitCallNew(CallNew* node) {
  USE(node);
  UNREACHABLE();
}


void Processor::VisitCallRuntime(CallRuntime* node) {
  USE(node);
  UNREACHABLE();
}


void Processor::VisitUnaryOperation(UnaryOperation* node) {
  USE(node);
  UNREACHABLE();
}


void Processor::VisitCountOperation(CountOperation* node) {
  USE(node);
  UNREACHABLE();
}


void Processor::VisitBinaryOperation(BinaryOperation* node) {
  USE(node);
  UNREACHABLE();
}


void Processor::VisitCompareOperation(CompareOperation* node) {
  USE(node);
  UNREACHABLE();
}


void Processor::VisitThisFunction(ThisFunction* node) {
  USE(node);
  UNREACHABLE();
}


bool Rewriter::Process(FunctionLiteral* function) {
  HistogramTimerScope timer(&Counters::rewriting);
  Scope* scope = function->scope();
  if (scope->is_function_scope()) return true;

  ZoneList<Statement*>* body = function->body();
  if (body->is_empty()) return true;

  VariableProxy* result = scope->NewTemporary(Factory::result_symbol());
  Processor processor(result);
  processor.Process(body);
  if (processor.HasStackOverflow()) return false;

  if (processor.result_assigned()) body->Add(new ReturnStatement(result));
  return true;
}


bool Rewriter::Optimize(FunctionLiteral* function) {
  ZoneList<Statement*>* body = function->body();

  if (FLAG_optimize_ast && !body->is_empty()) {
    HistogramTimerScope timer(&Counters::ast_optimization);
    AstOptimizer optimizer(function->name());
    optimizer.Optimize(body);
    if (optimizer.HasStackOverflow()) {
      return false;
    }
  }
  return true;
}


} }  // namespace v8::internal
